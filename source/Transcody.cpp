
#include "Transcody.h"

#include "XSDK/XSocket.h"
#include "XSDK/XTime.h"
#include "XSDK/XGuard.h"
#include "Webby/ClientSideRequest.h"
#include "Webby/WebbyException.h"
#include "AVKit/AVMuxer.h"
#include "AVKit/Options.h"
#include "AVKit/Utils.h"
#include "AVKit/FrameTypes.h"
#include "VAKit/VAH264Encoder.h"
#include "VAKit/VAH264Decoder.h"
#include "FrameStoreClient/Video.h"
#include "MediaParser/MediaParser.h"

using namespace XSDK;
using namespace std;
using namespace EXPORTY;
using namespace WEBBY;
using namespace FRAME_STORE_CLIENT;
using namespace AVKit;
using namespace VAKit;

const unsigned int DECODE_BUFFER_SIZE = (1024*1024) * 2;
const unsigned int BUFFER_PADDING = 16;
const unsigned int ENCODED_FRAME_BUFFER = (1024*1024);

Transcody::Transcody( XRef<Config> config,
                      XRef<XTSCache<XRef<TranscodyCacheItem> > > cache,
                      const XString& sessionID,
                      const XString& dataSourceID,
                      const XString& startTime,
                      const XString& endTime,
                      const XString& width,
                      const XString& height,
                      const XString& bitRate,
                      const XString& framerate,
                      const XString& profile,
                      const XString& qmin,
                      const XString& qmax,
                      const XString& maxQDiff ) :
    XBaseObject(),
    _config( config ),
    _cache( cache ),
    _sessionID( sessionID ),
    _dataSourceID( dataSourceID ),
    _startTime( startTime ),
    _endTime( endTime ),
    _width( width.ToUInt16() ),
    _height( height.ToUInt16() ),
    _bitRate( bitRate.ToUInt32() ),
    _framerate( framerate.ToDouble() ),
    _profile( profile ),
    _qmin( qmin ),
    _qmax( qmax ),
    _max_qdiff( maxQDiff ),
    _decodeBuffer( NULL )
{
    X_NEW_ALIGNED_BUFFER( _decodeBuffer, DECODE_BUFFER_SIZE, 16 );
}

Transcody::~Transcody() throw()
{
    X_DELETE_ALIGNED_BUFFER( _decodeBuffer );
}

XIRef<XMemory> Transcody::Get( int64_t& lastFrameTS )
{
    if( !_cache->IsCached( _sessionID ) )
        _PopulateSessionCache();

    XRef<TranscodyCacheItem> cacheItem;
    _cache->Get( _sessionID, cacheItem );

    XGuard g( cacheItem->sessionLok );

    double framerateStep = cacheItem->framerateStep;

    XTime transcodeStart = Clock::currTime();

    X_LOG_NOTICE( "==>HLS Transcody::Get @ %s, bitrate = %u", transcodeStart.ToISOExtString().c_str(), _bitRate);

    _SleepTillThePast();

    XString adjustedEnd = (XTime::FromISOExtString( _endTime ) + XDuration( SECONDS, 2 )).ToISOExtString();

    XIRef<XMemory> responseBuffer = FRAME_STORE_CLIENT::FetchVideo( _config->GetRecorderIP(),
                                                                    _config->GetRecorderPort(),
                                                                    _dataSourceID,
                                                                    _startTime,
                                                                    adjustedEnd );

    int numFramesToExport =
        _GetNumFramesToExport( responseBuffer,
                               (XTime::FromISOExtString( _endTime ) - XTime::FromISOExtString( _startTime )).Total(MSECS) );
    if( numFramesToExport <= 0 )
        X_STHROW( HTTP400Exception, ("No video was found for the requested time.") );

    XIRef<ResultParser> resultParser = new ResultParser;

    resultParser->Parse( responseBuffer );

    struct ResultStatistics resultStatistics = resultParser->GetStatistics();

    uint32_t inputAvgBitRate = resultStatistics.averageBitRate;

    // Our threshold for non transcoded HLS is a requested bit rate within 10% of our calculated average.
    uint32_t transcodeThresholdBitRate = inputAvgBitRate - (uint32_t)((double)inputAvgBitRate * 0.10);

    uint16_t inputWidth = 0;
    uint16_t inputHeight = 0;
    _GetResolution( resultParser->GetSPS(), inputWidth, inputHeight );

    double sourceFramerate = resultStatistics.frameRate;

    XRef<H264Decoder> decoder = cacheItem->decoder;
    XRef<Encoder> encoder = cacheItem->encoder;
    XRef<AVMuxer> muxer = cacheItem->muxer;

    if( _bitRate >= transcodeThresholdBitRate )
    {
        // If the requested bitrate is greater than the bitrate of the source, then transcoding it makes no sense...
        // So, just containerize it as a ts and return it.

        int timeBaseNum = 0;
        int timeBaseDen = 0;
        AVKit::DToQ( (1 / (double)resultStatistics.frameRate), timeBaseNum, timeBaseDen );

        struct CodecOptions options;
        options.bit_rate = resultStatistics.averageBitRate;
        options.width = inputWidth;
        options.height = inputHeight;
        options.time_base_num = timeBaseNum;
        options.time_base_den = timeBaseDen;
        options.gop_size = resultStatistics.gopSize;

        if( cacheItem->muxer.IsEmpty() )
            cacheItem->muxer = muxer = new AVMuxer( options, "0.ts", AVMuxer::OUTPUT_LOCATION_BUFFER );

        int videoStreamIndex = resultParser->GetVideoStreamIndex();

        while( numFramesToExport > 0 )
        {
            int streamIndex = 0;
            resultParser->ReadFrame( streamIndex );

            if( streamIndex != videoStreamIndex )
                continue;

            numFramesToExport--;

            lastFrameTS = resultParser->GetFrameTS();

            size_t frameSize = resultParser->GetFrameSize();

            if( (frameSize + BUFFER_PADDING) > DECODE_BUFFER_SIZE )
                X_THROW(( "Frame is too large to be decoded." ));

            resultParser->GetFrame( _decodeBuffer );

            muxer->WriteVideoFrame( _decodeBuffer, frameSize, resultParser->IsKey() );
        }
    }
    else
    {
        double outputFramesPerInputFrame = (_framerate / sourceFramerate);

        bool doneDecoding = false;
        bool doneEncoding = false;

        int numFramesWritten = 0;

        int videoStreamIndex = resultParser->GetVideoStreamIndex();

        while( !doneDecoding || !doneEncoding )
        {
            if( numFramesToExport >= 0 )
            {
                int streamIndex = 0;
                resultParser->ReadFrame( streamIndex );

                if( streamIndex != videoStreamIndex )
                    continue;

                numFramesToExport--;
                if( numFramesToExport <= 0 )
                    doneDecoding = true;

                lastFrameTS = resultParser->GetFrameTS();

                size_t frameSize = resultParser->GetFrameSize();

                if( (frameSize + BUFFER_PADDING) > DECODE_BUFFER_SIZE )
                    X_THROW(( "Frame is too large to be decoded." ));

                resultParser->GetFrame( _decodeBuffer );

                decoder->Decode( _decodeBuffer, frameSize );

                framerateStep += outputFramesPerInputFrame;

                // on the first frame of the first file, our encoder needs to be setup...
                if( encoder.IsEmpty() )
                    _FinishConfig( decoder, encoder, muxer );
                else
                {
                    if( _EncoderNeedsInit( encoder ) )
                    {
                        X_LOG_NOTICE("************         Updating encoder configuration.");
                        _UpdateConfig( decoder, encoder, muxer );
                    }
                }
            }

            XIRef<XMemory> picture = decoder->MakeYUV420P();

            XIRef<XMemory> encoded = new XMemory( ENCODED_FRAME_BUFFER + BUFFER_PADDING );
            uint8_t* encodedBuffer = &encoded->Extend( ENCODED_FRAME_BUFFER );

            while( framerateStep >= 1.0 && !doneEncoding )
            {
                bool key = (numFramesWritten == 0) ? true : false;

                AVKit::FrameType frameType = AVKit::FRAME_TYPE_AUTO_GOP;

                size_t encodedFrameSize = encoder->EncodeYUV420P( picture->Map(),
                                                                  encodedBuffer,
                                                                  ENCODED_FRAME_BUFFER,
                                                                  frameType );

                muxer->WriteVideoFrame( encodedBuffer, encodedFrameSize, key );

                numFramesWritten++;
                framerateStep -= 1.0;
            }

            if( doneDecoding && (framerateStep < 1.0) )
                doneEncoding = true;
        }
    }

    muxer->Flush();

    XIRef<XMemory> resultBuffer = new XMemory;

    muxer->FinalizeBuffer( resultBuffer );

    XTime transcodeStop = Clock::currTime();
    XDuration delta = transcodeStop - transcodeStart;

    X_LOG_NOTICE( "<==HLS Transcody::Get @ %s (delta millis == %s)",
                  transcodeStop.ToISOExtString().c_str(),
                  XString::FromInt64( delta.Total( MSECS ) ).c_str() );

    cacheItem->framerateStep = framerateStep;

    _cache->Put( _sessionID, cacheItem );

    return resultBuffer;
}

void Transcody::_SleepTillThePast() const
{
    XTime end = XTime::FromISOExtString( _endTime );
    XTime livest = Clock::currTime() - XDuration( SECONDS, 1 );

    if( end > livest )
    {
        int64_t sleepMillis = end.ToUnixTimeAsMSecs() - livest.ToUnixTimeAsMSecs();

        X_LOG_NOTICE("sleep at live %s",XString::FromInt64(sleepMillis).c_str());

        x_usleep( (unsigned int)sleepMillis * 1000 );
    }
}

int Transcody::_GetNumFramesToExport( XIRef<XMemory> result, int64_t requestedDuration ) const
{
    XIRef<ResultParser> resultParser = new ResultParser;

    resultParser->Parse( result );

    int frameIndex = 0;

    int64_t lastTS = 0;
    int64_t accumulatedDuration = 0;
    int64_t oneFrameDuration = 0;

    int videoStreamIndex = resultParser->GetVideoStreamIndex();

    int streamIndex = 0;
    while( resultParser->ReadFrame( streamIndex ) )
    {
        if( streamIndex != videoStreamIndex )
            continue;

        int64_t frameTS = resultParser->GetFrameTS();

        if( lastTS != 0 )
        {
            accumulatedDuration += frameTS - lastTS;
            oneFrameDuration = frameTS - lastTS;
        }
        lastTS = frameTS;

        if( accumulatedDuration > (requestedDuration-oneFrameDuration) )
        {
            if( resultParser->IsKey() )
                return frameIndex;
        }

        frameIndex++;
    }

    X_THROW(("Stepped over entire result without accumulating enough duration and finding a key!"));
}

double Transcody::_GetFramerate( const XIRef<FRAME_STORE_CLIENT::ResultParser> resultParser ) const
{
    XIRef<XMemory> sps = resultParser->GetSPS();
    XIRef<XMemory> pps = resultParser->GetPPS();

    MEDIA_PARSER::H264Info h264Info;

    if( !MEDIA_PARSER::MediaParser::GetMediaInfo( sps->Map(), sps->GetDataSize(), h264Info ) )
        X_STHROW( HTTP500Exception, ( "Unable to parse provided SPS. Exporty cannot continue." ));

    MEDIA_PARSER::MediaInfo* mediaInfo = &h264Info;

    XString SDPFrameRate = resultParser->GetSDPFrameRate();

    if( ((double)mediaInfo->GetFrameRate() == 0.0) &&
        SDPFrameRate.ToDouble() == 0.0 )
        X_THROW(("FrameRate Unknown: mediaparser failed to parse, and not included in source SDP."));

    return (SDPFrameRate.length() > 0) ? SDPFrameRate.ToDouble() : (double)mediaInfo->GetFrameRate();
}

void Transcody::_PopulateSessionCache()
{
    XRef<TranscodyCacheItem> cacheItem = new TranscodyCacheItem;

    if( _config->HasDRIDecoder() )
        cacheItem->decoder = new VAH264Decoder( GetFastH264DecoderOptions( "/dev/dri/card0" ) );
    else cacheItem->decoder = new H264Decoder( GetFastH264DecoderOptions() );

    cacheItem->framerateStep = 0.0;

    _cache->Put( _sessionID, cacheItem );
}

void Transcody::_FinishConfig( XRef<H264Decoder> decoder, XRef<Encoder>& encoder, XRef<AVMuxer>& muxer )
{
    XRef<TranscodyCacheItem> cacheItem;
    _cache->Get( _sessionID, cacheItem );

    if( cacheItem.IsEmpty() )
        X_THROW(("Unable to fetch session cache that should exist in _FinishConfig()."));

    cacheItem->encoder = encoder = _CreateEncoder( decoder );

    if( cacheItem->muxer.IsValid() )
    {
        cacheItem->muxer->ApplyCodecOptions( encoder->GetOptions() );
    }
    else cacheItem->muxer = muxer = new AVMuxer( encoder->GetOptions(), "0.ts", AVMuxer::OUTPUT_LOCATION_BUFFER );

    _cache->Put( _sessionID, cacheItem );
}

void Transcody::_UpdateConfig( XRef<H264Decoder> decoder, XRef<Encoder>& encoder, XRef<AVMuxer> muxer )
{
    XRef<TranscodyCacheItem> cacheItem;
    _cache->Get( _sessionID, cacheItem );

    if( cacheItem.IsEmpty() )
        X_THROW(("Unable to fetch session cache that should exist in _UpdateConfig()."));

    cacheItem->encoder = encoder = _CreateEncoder( decoder );

    muxer->ApplyCodecOptions( encoder->GetOptions() );

    _cache->Put( _sessionID, cacheItem );
}

XRef<Encoder> Transcody::_CreateEncoder( XRef<H264Decoder> decoder )
{
    AspectCorrectDimensions( decoder->GetInputWidth(), decoder->GetInputHeight(),
                             _width, _height,
                             _width, _height );

    decoder->SetOutputWidth( _width );
    decoder->SetOutputHeight( _height );

    int timeBaseNum = 0;
    int timeBaseDen = 0;
    AVKit::DToQ( (1 / _framerate), timeBaseNum, timeBaseDen );

    struct CodecOptions options = GetHLSH264EncoderOptions( _bitRate, _width, _height, 1000, timeBaseNum, timeBaseDen );

    if( _profile != "DEFAULT" )
        options.profile = _profile;

    if( _qmin != "DEFAULT" )
        options.qmin = _qmin.ToInt();

    if( _qmax != "DEFAULT" )
        options.qmax = _qmax.ToInt();

    if( _max_qdiff != "DEFAULT" )
        options.max_qdiff = _max_qdiff.ToInt();

    options.gop_size = 150;

    if( _config->HasDRIEncoder() )
    {
        options.device_path = "/dev/dri/card0";
        return new VAH264Encoder( options );
    }
    else return new H264Encoder( options );
}

bool Transcody::_EncoderNeedsInit( XRef<AVKit::Encoder> encoder )
{
    bool different = false;

    if( encoder.IsValid() )
    {
        struct CodecOptions options = encoder->GetOptions();

        if( (options.bit_rate.Value() != (int)_bitRate) ||
            (options.width.Value() != (int)_width) ||
            (options.height.Value() != (int)_height) )
            different = true;

        double encFramerate = AVKit::QToD( options.time_base_num.Value(), options.time_base_den.Value() );

        // Comparing floating point values is tricky. Really, its about margin of error.... Here I care
        // about margins of error greater than a thousandth of a second. We'll see how this works... :)
        if( fabs( (1 / (double)_framerate) - encFramerate ) > 0.001 )
            different = true;
    }

    return different;
}

void Transcody::_GetResolution( const XIRef<XMemory> sps, uint16_t& width, uint16_t& height ) const
{
    MEDIA_PARSER::H264Info h264Info;

    if( !MEDIA_PARSER::MediaParser::GetMediaInfo( sps->Map(), sps->GetDataSize(), h264Info ) )
        X_STHROW( HTTP500Exception, ( "Unable to parse provided SPS. LargeExport cannot continue." ));

    MEDIA_PARSER::MediaInfo* mediaInfo = &h264Info;

    width = mediaInfo->GetFrameWidth();
    height = mediaInfo->GetFrameHeight();
}
