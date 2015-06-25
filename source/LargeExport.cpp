
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "LargeExport.h"

#include "XSDK/XSocket.h"
#include "XSDK/XPath.h"

#include "Webby/ClientSideRequest.h"
#include "Webby/WebbyException.h"

#include "FrameStoreClient/Video.h"

using namespace XSDK;
using namespace EXPORTY;
using namespace WEBBY;
using namespace FRAME_STORE_CLIENT;
using namespace AVKit;
using namespace std;

LargeExport::LargeExport( XRef<Config> config,
                          const XString& dataSourceID,
                          const XString& startTime,
                          const XString& endTime,
                          const XString& fileName ) :
    XBaseObject(),
    _config( config ),
    _dataSourceID( dataSourceID ),
    _startTime( startTime ),
    _endTime( endTime ),
    _fileName( fileName ),
    _recorderURLS( new RecorderURLS( dataSourceID, startTime, endTime, 6000, false ) ),
    _muxer(),
    _extension()
{
}

LargeExport::~LargeExport() throw()
{
}

void LargeExport::Create( XIRef<XMemory> output )
{
    XString tempFileName = _GetTMPName( _fileName );

    XString recorderURI;

    while( _recorderURLS->GetNextURL( recorderURI ) )
    {
        XIRef<XMemory> responseBuffer = FRAME_STORE_CLIENT::FetchVideo( _config->GetRecorderIP(),
                                                                        _config->GetRecorderPort(),
                                                                        recorderURI );

        XIRef<ResultParser> resultParser = new ResultParser;

        resultParser->Parse( responseBuffer );

        XIRef<XMemory> sps = resultParser->GetSPS();
        XIRef<XMemory> pps = resultParser->GetPPS();

        if( _muxer.IsEmpty() )
        {
            _muxer = new AVMuxer( _GetCodecOptions( sps,
                                                    resultParser->GetSDPFrameRate(),
                                                    _MeasureGOP( responseBuffer ) ),
                                  tempFileName,
                                  (output.IsEmpty()) ? AVMuxer::OUTPUT_LOCATION_FILE : AVMuxer::OUTPUT_LOCATION_BUFFER );

            _muxer->SetExtraData( _GetExtraData( sps, pps ) );
        }

        int videoStreamIndex = resultParser->GetVideoStreamIndex();

        int streamIndex = 0;
        while( resultParser->ReadFrame( streamIndex ) )
        {
            if( streamIndex != videoStreamIndex )  // For now, we only support video stream exports.
                continue;

            XIRef<Packet> pkt = resultParser->Get();

            _muxer->WriteVideoPacket( pkt, resultParser->IsKey() );
        }
    }

    if( output.IsEmpty() )
    {
        _muxer->FinalizeFile();

        rename( tempFileName.c_str(), _fileName.c_str() );
    }
    else
    {
        _muxer->FinalizeBuffer( output );
    }
}

XString LargeExport::GetExtension() const
{
    return _fileName.substr( _fileName.rfind( "." ) );
}

XIRef<XMemory> LargeExport::_GetExtraData( XIRef<XMemory> sps, XIRef<XMemory> pps ) const
{
    uint8_t code[] = { 0, 0, 0, 1 };

    XIRef<XMemory> extra = new XMemory;

    memcpy( &extra->Extend( 4 ), code, 4 );

    memcpy( &extra->Extend( sps->GetDataSize() ), sps->Map(), sps->GetDataSize() );

    memcpy( &extra->Extend( 4 ), code, 4 );

    memcpy( &extra->Extend( pps->GetDataSize() ), pps->Map(), pps->GetDataSize() );

    return extra;
}

XString LargeExport::_GetTMPName( const XString& fileName ) const
{
#ifdef WIN32
    X_THROW(("This function has a bug on Windows. I am throwing here so that if anyone uses it on Windows, I wont forget about it.")); // The bug is at least that the Split() below should operate on PATH_SLASH not "/".
#endif

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

int LargeExport::_MeasureGOP( XIRef<XMemory> responseBuffer ) const
{
    XIRef<ResultParser> resultParser = new ResultParser;

    resultParser->Parse( responseBuffer );

    bool foundFirstKey = false;
    int gopSize = 0;

    int videoStreamIndex = resultParser->GetVideoStreamIndex();

    int streamIndex = 0;
    while( resultParser->ReadFrame( streamIndex ) )
    {
        if( streamIndex != videoStreamIndex )
            continue;

        if( foundFirstKey == false )
        {
            if( resultParser->IsKey() )
            {
                foundFirstKey = true;

                continue;
            }
        }

        if( foundFirstKey )
        {
            gopSize++;

            if( resultParser->IsKey() )
                break;
        }
    }

    return gopSize;
}

struct CodecOptions LargeExport::_GetCodecOptions( const XIRef<XSDK::XMemory> sps,
                                                   const XSDK::XString& sdpFrameRate,
                                                   int gopSize ) const
{
    MEDIA_PARSER::H264Info h264Info;

    if( !MEDIA_PARSER::MediaParser::GetMediaInfo( sps->Map(), sps->GetDataSize(), h264Info ) )
        X_STHROW( HTTP500Exception, ( "Unable to parse provided SPS. LargeExport cannot continue." ));

    MEDIA_PARSER::MediaInfo* mediaInfo = &h264Info;

    struct CodecOptions options;
    options.gop_size = gopSize;
    options.bit_rate = mediaInfo->GetBitRate();
    options.width = mediaInfo->GetFrameWidth();
    options.height = mediaInfo->GetFrameHeight();
    options.time_base_num = 1;

    int frameRate = (sdpFrameRate.length() > 0) ? sdpFrameRate.ToInt() : (int)mediaInfo->GetFrameRate();

    // XXX - FrameRate hack. Right now, KingFisher cameras are reporting their framerate
    // (via mediaparser) as 30. This forces them to a max of 15.
    if( frameRate > 15 )
    {
        X_LOG_WARNING( "Detected frame rate out of our supported range (<=15)" );
        frameRate = 15;
    }

    options.time_base_den = frameRate;

    return options;
}
