
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "TranscodeExport.h"
#include "FrameStoreClient/ResultParser.h"
#include "FrameStoreClient/Media.h"
#include "Webby/WebbyException.h"
#include "XSDK/XPath.h"
#include "AVKit/AVDeMuxer.h"
#include "AVKit/H264Transcoder.h"
#include "AVKit/ARGB24ToYUV420P.h"
#include "AVKit/YUV420PToARGB24.h"
#include "AVKit/Options.h"
#include "AVKit/Utils.h"
#include "librsvg/rsvg.h" 
#include <vector>
#include <cmath>

using namespace EXPORTY;
using namespace XSDK;
using namespace AVKit;
using namespace WEBBY;
using namespace FRAME_STORE_CLIENT;
using namespace std;

ExportOverlay::ExportOverlay( const XSDK::XString& msg,
                              bool withTime,
                              XSDK::XTime clockTime,
                              OverlayHAlign hAlign,
                              OverlayVAlign vAlign,
                              uint16_t width,
                              uint16_t height,
                              int timeBaseNum,
                              int timeBaseDen ) :
    _msg( msg ),
    _decodedMsg(),
    _withTime( withTime ),
    _clockTime( clockTime ),
    _hAlign( hAlign ),
    _vAlign( vAlign ),
    _width( width ),
    _height( height ),
    _timeBaseNum( timeBaseNum),
    _timeBaseDen( timeBaseDen ),
    _timePerFrame( ((double)timeBaseNum / timeBaseDen) ),
    _logoX( (uint16_t)((double)_width * 0.79) ),
    _logoY( (uint16_t)((double)_height * 0.91) ),
    _logoWidth( (uint16_t)((double)_width * 0.2) ),
    _logoHeight( (uint16_t)((double)_height * 0.07) ),
    _wmSurface( NULL )
{
    if( !_msg.empty() )
    {
        XIRef<XSDK::XMemory> decodedBuf = _msg.FromBase64();
        _decodedMsg = XString( (const char*)decodedBuf->Map(), decodedBuf->GetDataSize() );
    }

    X_LOG_NOTICE("watermark: x=%u, y=%u, w=%u, h=%u", _logoX, _logoY, _logoWidth, _logoHeight);

    _wmSurface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, _logoWidth, _logoHeight );

    if( !_wmSurface )
        X_THROW(("Unable to allocate cairo surface for watermark: _logoWidth = %u, _logoHeight = %u", _logoWidth, _logoHeight));

    cairo_t* wmCr = cairo_create( _wmSurface );

    if( !wmCr )
        X_THROW(("Unable to allocate cairo handle for watermark."));

    cairo_scale( wmCr, (double)_width / 1408, (double)_height / 792 );

    GError* err = NULL;
    RsvgHandle* rsvgHandle = rsvg_handle_new_from_file("multisight-logo-white-outline.svg", &err);

    if( !rsvgHandle )
        X_THROW(("Unable to open ms logo from svg for watermark."));

    if( rsvg_handle_render_cairo( rsvgHandle, wmCr ) != TRUE )
        X_THROW(("svg render failed for watermark."));

    g_object_unref(rsvgHandle);

    cairo_destroy( wmCr );

}

ExportOverlay::~ExportOverlay() throw()
{
    cairo_surface_destroy( _wmSurface );
}

XIRef<Packet> ExportOverlay::Process( XIRef<Packet> input )
{
    _clockTime += XDuration( MSECS, (int)(_timePerFrame * 1000) );

    cairo_surface_t* surface = NULL;
    cairo_t* cr = NULL;

    try
    {
        surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, _width, _height );
        cr = cairo_create( surface );

        uint8_t* cairoSrc = cairo_image_surface_get_data( surface );
        int cairoSrcWidth = cairo_image_surface_get_width( surface );
        int cairoSrcHeight = cairo_image_surface_get_height( surface );
        if( cairo_image_surface_get_stride( surface ) != (cairoSrcWidth * 4) )
            X_THROW(("Unexpected cairo stride!"));

        cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 1.0 );
        cairo_rectangle( cr, 0.0, 0.0, cairoSrcWidth, cairoSrcHeight );
        cairo_fill( cr );

        memcpy( cairoSrc, input->Map(), input->GetDataSize() );

        PangoLayout* layout = pango_cairo_create_layout( cr );

        pango_layout_set_text( layout, _decodedMsg.c_str(), -1 );
        PangoFontDescription* desc = pango_font_description_from_string( "Helvetica 24" );
        pango_layout_set_font_description( layout, desc );
        pango_font_description_free( desc );

        PangoRectangle logicalRect;
        pango_layout_get_pixel_extents( layout, NULL, &logicalRect );

        uint16_t y = (_vAlign==V_ALIGN_TOP) ? 10 : _height - 52;

        uint16_t timeX = 0;
        uint16_t msgX = 0;
        uint16_t bgX = 0;
        uint16_t bgWidth = 0;
        _GetXPositions( timeX, msgX, logicalRect.width, bgX, bgWidth );

        cairo_set_source_rgba( cr, 0.5, 0.5, 0.5, 0.50 );
        cairo_rectangle( cr, bgX, y, bgWidth, 38 );
        cairo_fill( cr );

        cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 1.0 );

        if( !_decodedMsg.empty() )
            _DrawMessage( cr, layout, msgX, y );

        if( _withTime )
            _DrawTime( cr, timeX, y );

        g_object_unref( layout );

        // copy from our watermark surface to our output surface...
        cairo_set_source_surface( cr, _wmSurface, _logoX, _logoY );
        cairo_rectangle( cr, _logoX, _logoY, _logoWidth, _logoHeight );
        cairo_clip( cr );
        cairo_paint_with_alpha( cr, 0.70 );
        //cairo_fill( cr );

        // Copy data out of our cairo surface into our output packet...
        size_t outputSize = (cairoSrcWidth * 4) * cairoSrcHeight;
        XIRef<Packet> dest = new Packet( outputSize );
        memcpy( dest->Map(), cairoSrc, outputSize );
        dest->SetDataSize( outputSize );

        cairo_destroy( cr );
        cairo_surface_destroy( surface );

        return dest;
    }
    catch(...)
    {
        if( cr )
            cairo_destroy( cr );
        if( surface )
            cairo_surface_destroy( surface );

        throw;
    }
}

void ExportOverlay::_GetXPositions( uint16_t& timeX, uint16_t& msgX, uint16_t messageWidth, uint16_t& bgX, uint16_t& bgWidth )
{
    if( _hAlign == H_ALIGN_LEFT )
    {
        bgWidth = 0;
        if( _withTime )
        {
            timeX = 10;
            bgWidth += 5 + 370;
        }
        if( !_msg.empty() )
        {
            if( _withTime )
                msgX = 380;
            else msgX = 10;
            bgWidth += messageWidth;
        }
        bgX = 0;
    }
    else // _hAlign == H_ALIGN_RIGHT
    {
        bgX = _width;
        if( _withTime )
        {
            timeX = _width - 170;
            bgX -= 170;
        }
        if( !_msg.empty() )
        {
            if( _withTime )
                msgX = _width - 170 - 10 - messageWidth;
            else msgX = _width - 10 - messageWidth;
            bgX -= messageWidth + 10 + 10;
        }
        bgWidth = _width - bgX;
    }
}

void ExportOverlay::_DrawMessage( cairo_t* cr, PangoLayout* layout, uint16_t msgX, uint16_t y )
{
    cairo_move_to( cr, (double)msgX, (double)y );
    pango_cairo_show_layout( cr, layout );
}

void ExportOverlay::_DrawTime( cairo_t* cr, uint16_t timeX, uint16_t y )
{
    int64_t milliTime = _clockTime.ToUnixTimeAsMSecs();

    time_t ts = (time_t)(milliTime / 1000);
    struct tm* bdt = localtime( &ts );

    char timeMessage[1024];
    memset( timeMessage, 0, 1024 );
    strftime( timeMessage, 1024, "%m/%d/%G %I:%M:%S %p", bdt );

    PangoLayout* layout = pango_cairo_create_layout( cr );
    pango_layout_set_text( layout, timeMessage, -1 );
    PangoFontDescription* desc = pango_font_description_from_string( "Helvetica 24" );
    pango_layout_set_font_description( layout, desc );
    pango_font_description_free( desc );

    cairo_move_to( cr, timeX, y );
    pango_cairo_show_layout( cr, layout );

    g_object_unref( layout );
}

TranscodeExport::TranscodeExport( XRef<Config> config,
                                  function<void(int)> progress,
                                  const XString& dataSourceID,
                                  const XString& startTime,
                                  const XString& endTime,
                                  uint16_t width,
                                  uint16_t height,
                                  uint32_t bitRate,
                                  double frameRate,
                                  const XString& fileName,
                                  OverlayHAlign hAlign,
                                  OverlayVAlign vAlign,
                                  const XString& msg,
                                  bool withTime,
                                  double speed ) :
    _config( config ),
    _progress( progress ),
    _dataSourceID( dataSourceID ),
    _startTime( startTime ),
    _endTime( endTime ),
    _requestedWidth( width ),
    _requestedHeight( height ),
    _bitRate( bitRate ),
    _frameRate( frameRate ),
    _fileName( fileName ),
    _hAlign( hAlign ),
    _vAlign( vAlign ),
    _msg( msg ),
    _withTime( withTime ),
    _speed( speed ),
    _recorderURLS( dataSourceID, startTime, endTime, speed )
{
}

TranscodeExport::~TranscodeExport() throw()
{
}

void TranscodeExport::Create( XIRef<XMemory> output )
{
    XString tempFileName = _GetTMPName( _fileName );
    bool outputToFile = (output.IsEmpty()) ? true : false;
    H264Decoder decoder( GetFastH264DecoderOptions() );
    XRef<YUV420PToARGB24> yuvToARGB = new YUV420PToARGB24;
    XRef<ARGB24ToYUV420P> argbToYUV = new ARGB24ToYUV420P;
    XRef<H264Transcoder> transcoder;
    XRef<H264Encoder> encoder;
    XRef<AVMuxer> muxer;
    XRef<ExportOverlay> ov;
    bool wroteToContainer = false;

    float lastPercentComplete = 0.0;

    XString recorderURI;
    while( _recorderURLS.GetNextURL( recorderURI ) )
    {
        float percentComplete = _recorderURLS.PercentComplete();

        X_LOG_NOTICE("percentComplete = %f\n",percentComplete);

        if( (percentComplete - lastPercentComplete) > 0.01 )
        {
            lastPercentComplete = percentComplete;
            _progress( (int)(percentComplete * 100) );
        }

        try
        {
            XIRef<XMemory> responseBuffer = FRAME_STORE_CLIENT::FetchMedia( _config->GetRecorderIP(),
                                                                            _config->GetRecorderPort(),
                                                                            recorderURI );

            ResultParser resultParser;

            resultParser.Parse( responseBuffer );

            FRAME_STORE_CLIENT::ResultStatistics stats = resultParser.GetStatistics();

            // If we are not provided with a bit rate or a frame rate, we use the sources values.
            if( _bitRate == 0 )
                _bitRate = stats.averageBitRate;

            if( _frameRate == 0.0 )
                _frameRate = stats.frameRate;

            // Fix for ffmpeg's inability to make files with fps < 6.0. Don't believe me? Try these 2 commands and play
            // output in vlc:
            //
            //   # generate a test movie of the game of life in life.mp4
            //   ffmpeg -f lavfi -i life -frames:v 1000 life.mp4
            //   # transcode and drop framerate of life.mp4 to 1 fps. output.mp4 won't play in vlc and will have a weird
            //   # pause at the beginning for other players.
            //   ffmpeg -i life.mp4 -vf fps=fps=1/1 -vcodec h264 output.mp4
            //
            if( _frameRate < 6.0 )
                _frameRate = 6.0;

            int outputTimeBaseNum = 0;
            int outputTimeBaseDen = 0;
            int inputTimeBaseNum = 0;
            int inputTimeBaseDen = 0;

            AVKit::DToQ( (1/stats.frameRate), inputTimeBaseNum, inputTimeBaseDen );
            AVKit::DToQ( (1/_frameRate), outputTimeBaseNum, outputTimeBaseDen );

            if( transcoder.IsEmpty() )
            {
                transcoder = new H264Transcoder( inputTimeBaseNum, inputTimeBaseDen,
                                                 outputTimeBaseNum, outputTimeBaseDen,
                                                 _speed,
                                                 // if our input is key only, enable decode skipping...
                                                 _recorderURLS.KeyFrameOnly() );
            }

            double secondsPer = AVKit::QToD(inputTimeBaseNum, inputTimeBaseDen) / (AVKit::QToD(inputTimeBaseNum, inputTimeBaseDen) / (AVKit::QToD(outputTimeBaseNum, outputTimeBaseDen) * _speed));
            int traversalNum = 0;
            int traversalDen = 0;

            AVKit::DToQ( secondsPer, traversalNum, traversalDen );

            while( !resultParser.EndOfFile() )
            {
                if( transcoder->Decode( resultParser, decoder ) )
                {
                    if( encoder.IsEmpty() )
                        _FinishInit( encoder, muxer, decoder, tempFileName, outputToFile, traversalNum, traversalDen );

                    if( ov.IsEmpty() )
                        ov = new ExportOverlay( _msg,
                                                _withTime,
                                                XTime::CreateFromUnixTimeAsMSecs( resultParser.GetFrameTS() ),
                                                _hAlign,
                                                _vAlign,
                                                decoder.GetOutputWidth(),
                                                decoder.GetOutputHeight(),
                                                traversalNum,
                                                traversalDen );


                    yuvToARGB->Transform( decoder.Get(), decoder.GetOutputWidth(), decoder.GetOutputHeight() );

                    XIRef<Packet> rgb = yuvToARGB->Get();

                    XIRef<Packet> withOverlay = ov->Process( rgb );

                    argbToYUV->Transform( withOverlay, decoder.GetOutputWidth(), decoder.GetOutputHeight() );

                    transcoder->EncodeYUV420PAndMux( *encoder, *muxer, argbToYUV->Get() );
                    wroteToContainer = true;
                }
            }

            ov.Clear();
        }
        catch( HTTP404Exception& ex )
        {
            X_LOG_NOTICE("Encountered 404 and gap in video during export. Continuing.");
        }
    }

    if( !wroteToContainer )
        X_STHROW( HTTP404Exception, ("No video was found during entire export."));

    if( outputToFile )
    {
        muxer->FinalizeFile();
        rename( tempFileName.c_str(), _fileName.c_str() );
    }
    else muxer->FinalizeBuffer( output );
}

XString TranscodeExport::_GetTMPName( const XString& fileName ) const
{
    vector<XString> parts;
    fileName.Split( "/", parts );

    XString path;

    for( int i = 0; i < ((int)parts.size() - 1); i++ )
        path += XString::Format( "%s%s", PATH_SLASH, parts[i].c_str() );

#ifdef WIN32
    uint32_t randomVal = rand();
#else
    uint32_t randomVal = random();
#endif

    XString ext = fileName.substr( fileName.rfind( "." ) );

    return XString::Format( "%s%sexporty-tmp-%s%s",
                            path.c_str(),
                            PATH_SLASH,
                            XString::FromUInt32(randomVal).c_str(),
                            ext.c_str() );
}

void TranscodeExport::_FinishInit( XRef<H264Encoder>& encoder,
                                   XRef<AVMuxer>& muxer,
                                   H264Decoder& decoder,
                                   const XString& tempFileName,
                                   bool outputToFile,
                                   int traversalNum,
                                   int traversalDen )
{
    // Now that we have decoded the first frame, we can finish initializing everything...

    // First, we should finish initializing our Decoder by setting an output resolution.
    uint16_t width = 0;
    uint16_t height = 0;

    AspectCorrectDimensions( decoder.GetInputWidth(), decoder.GetInputHeight(),
                             _requestedWidth, _requestedHeight,
                             width, height );

    decoder.SetOutputWidth( width );
    decoder.SetOutputHeight( height );

    // Configure and create our encoder...
    int timeBaseNum = 0;
    int timeBaseDen = 0;
    AVKit::DToQ( (1 / _frameRate), timeBaseNum, timeBaseDen );

    CodecOptions options = GetHLSH264EncoderOptions( _bitRate, width, height, 15, timeBaseNum, timeBaseDen );

    encoder = new H264Encoder( options, false );

    // Create our muxer...
    muxer = new AVMuxer( encoder->GetOptions(),
                         tempFileName,
                         (outputToFile) ? AVMuxer::OUTPUT_LOCATION_FILE : AVMuxer::OUTPUT_LOCATION_BUFFER );

    // Finally, provide the muxer with our encoders extra data so we create valid conainers.
    muxer->SetExtraData( encoder->GetExtraData() );
}
