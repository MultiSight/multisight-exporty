
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "Request.h"
#include "ExportyServer.h"
#include "LargeExport.h"
#include "Transcody.h"
#include "TranscodeExport.h"
#include "Thumbs.h"
#include "XSDK/XBytePtr.h"
#include "XSDK/XTime.h"
#include "XSDK/TimeUtils.h"
#include "XSDK/XPath.h"
#include "Webby/WebbyException.h"

using namespace EXPORTY;
using namespace XSDK;
using namespace WEBBY;
using namespace std;

static const uint32_t MEDIA_CHUNK_SIZE = 16384;

static const XString DEFAULT_ENCODE_BIT_RATE = "0";
static const XString DEFUALT_ENCODE_MAX_RATE = "0";

Request::Request( ExportyServer& server ) :
    _server( server ),
    _clientSocket( new XSocket ),
    _httpRequest(),
    _done( false ),
    _doneTime()
{
}

Request::~Request() throw()
{
}

#ifndef WIN32
#define UNLINK unlink
#else
#define UNLINK _unlink
#endif

void* Request::EntryPoint()
{
    _done = false;

    try
    {
        _httpRequest.ReadRequest( _clientSocket );

        URI uri = _httpRequest.GetURI();
        XString fullURI = uri.GetFullRawURI();

        ServerSideResponse response;

        bool responseWritten = false;

        try
        {
            XString requestMethod = _httpRequest.GetMethod().ToLower();

            if( requestMethod == "get" )
            {
                response.AddAdditionalHeader( "Connection", "Close" );

                if( fullURI == "/server" )
                {
                    response.SetBody("running");
                }
                else if( fullURI.StartsWith( "/thumbnail" ) )
                {
                    XHash<XString> getArgs = uri.GetGetArgs();

                    XString dataSourceID;
                    if( getArgs.Find( "data_source_id" ) )
                        dataSourceID = *getArgs.Find( "data_source_id" );
                    else X_STHROW( HTTP400Exception, ("data_source_id is required for /thumbnail queries." ));

                    XString timeString;
                    if( getArgs.Find( "time" ) )
                    {
                        if( XTime::FromISOExtString( *getArgs.Find( "time" ) ) >
                            XTime( Clock::currTime() - XDuration( MSECS, 1000 ) ) )
                        {
                            timeString = XTime( Clock::currTime() - XDuration( MSECS, 1000 ) ).ToISOExtString();
                        }
                        else
                        {
                            timeString = *getArgs.Find( "time" );
                        }
                    }
                    else timeString = XTime( Clock::currTime() - XDuration( MSECS, 1000 ) ).ToISOExtString();

                    XString width;
                    if( getArgs.Find( "width" ) )
                        width = *getArgs.Find( "width" );
                    else X_STHROW( HTTP400Exception, ("width is required for /thumbnail queries." ));
                    if( width.ToUInt16() < 64 )
                        width = "64";

                    XString height;
                    if( getArgs.Find( "height" ) )
                        height = *getArgs.Find( "height" );
                    else X_STHROW( HTTP400Exception, ("height is required for /thumbnail queries." ));
                    if( height.ToUInt16() < 36 )
                        height = "36";

                    XString qmax;
                    if( getArgs.Find( "qmax" ) )
                        qmax = *getArgs.Find( "qmax" );
                    else qmax = "8";

                    XString qmin;
                    if( getArgs.Find( "qmin" ) )
                        qmin = *getArgs.Find( "qmin" );
                    else qmin = "8";

                    XString bitRate;
                    if( getArgs.Find( "bit_rate" ) )
                        bitRate = *getArgs.Find( "bit_rate" );
                    else bitRate = "10000000";

                    response.SetContentType("image/jpeg");

                    response.SetBody( CreateJPEGThumbnail( _server.GetConfig(), dataSourceID, timeString, width.ToUInt16(), height.ToUInt16(), qmax.ToUInt32(), qmin.ToUInt32(), bitRate.ToUInt32() ) );
                }
                else if( fullURI.StartsWith( "/transcoded_media" ) )
                {
                    XHash<XString> getArgs = uri.GetGetArgs();

                    XString dataSourceID;
                    if( getArgs.Find( "data_source_id" ) )
                        dataSourceID = *getArgs.Find( "data_source_id" );
                    else X_STHROW( HTTP400Exception, ("data_source_id is required for /transcoded_media queries." ));

                    XString startTime;
                    if( getArgs.Find( "start_time" ) )
                        startTime = *getArgs.Find( "start_time" );
                    else X_STHROW( HTTP400Exception, ("start_time is required for /transcoded_media queries." ));

                    XString endTime;
                    if( getArgs.Find( "end_time" ) )
                        endTime = *getArgs.Find( "end_time" );
                    else X_STHROW( HTTP400Exception, ("end_time is required for /transcoded_media queries." ));

                    XString width;
                    if( getArgs.Find( "width" ) )
                        width = *getArgs.Find( "width" );
                    else width = "1280";

                    XString height;
                    if( getArgs.Find( "height" ) )
                        height = *getArgs.Find( "height" );
                    else height = "720";

                    XString speed;
                    if( getArgs.Find( "speed" ) )
                        speed = *getArgs.Find( "speed" );
                    else speed = "1.0";

                    XString bitRate;
                    if( getArgs.Find( "bit_rate" ) )
                        bitRate = *getArgs.Find( "bit_rate" );
                    else bitRate = DEFAULT_ENCODE_BIT_RATE;

                    XString maxRate;
                    if( getArgs.Find( "max_rate" ) )
                        maxRate = *getArgs.Find( "max_rate" );
                    else maxRate = DEFUALT_ENCODE_MAX_RATE;

                    XString bufSize = "0";
                    if( getArgs.Find( "buf_size" ) )
                        bufSize = *getArgs.Find( "buf_size" );

                    XString initialQP;
                    if( getArgs.Find( "initial_qp" ) )
                        initialQP = *getArgs.Find( "initial_qp" );
                    else initialQP = "-1";

                    XString framerate;
                    if( getArgs.Find( "framerate" ) )
                        framerate = *getArgs.Find( "framerate" );
                    else framerate = "15.0";

                    // a GET on /transcoded_media with a file_name arguments means that we should do a transcoded
                    // export and return it with chunked transfer encoding.
                    if( getArgs.Find( "file_name" ) )
                    {
                        XString fileName = getArgs["file_name"];

                        XString msg;
                        if( getArgs.Find( "msg" ) )
                            msg = *getArgs.Find( "msg" );

                        bool withTime = true;
                        if( getArgs.Find( "with_time" ) )
                        {
                            XString withTimeString = *getArgs.Find( "with_time" );
                            if( withTimeString.ToLower() == "false" )
                                withTime = false;
                        }

                        OverlayHAlign hAlign = H_ALIGN_LEFT;
                        if( getArgs.Find( "h_align" ) )
                        {
                            XString hAlignString = *getArgs.Find( "h_align" );
                            if( hAlignString.ToLower() == "right" )
                                hAlign = H_ALIGN_RIGHT;
                        }

                        OverlayVAlign vAlign = V_ALIGN_TOP;
                        if( getArgs.Find( "v_align" ) )
                        {
                            XString vAlignString = *getArgs.Find( "v_align" );
                            if( vAlignString.ToLower() == "bottom" )
                                vAlign = V_ALIGN_BOTTOM;
                        }

                        XRef<TranscodeExport> te = new TranscodeExport( _server.GetConfig(),
                                                                        [](float progress){},
                                                                        dataSourceID,
                                                                        startTime,
                                                                        endTime,
                                                                        width.ToUInt16(),
                                                                        height.ToUInt16(),
                                                                        bitRate.ToUInt32(),
                                                                        maxRate.ToUInt32(),
                                                                        bufSize.ToUInt32(),
                                                                        framerate.ToDouble(),
                                                                        fileName,
                                                                        hAlign,
                                                                        vAlign,
                                                                        msg,
                                                                        withTime,
                                                                        speed.ToDouble() );

                        XIRef<XMemory> result = new XMemory;

                        te->Create( result );

                        responseWritten = true;

                        XString ext = fileName.substr( fileName.rfind( "." ) );

                        _ChunkTransfer( _clientSocket, response, result, XString::Format( "video/%s", ext.c_str() ) );
                    }
                    else  // If no file_name or file_path was specified, then this is HLS.
                    {
                        XString sessionID;
                        if( getArgs.Find( "session_id" ) )
                            sessionID = *getArgs.Find( "session_id" );
                        else X_STHROW( HTTP400Exception, ("session_id is required for /transcoded_media queries." ));

                        XString previousPlayable;
                        if( getArgs.Find( "previous_playable" ) )
                            previousPlayable = *getArgs.Find( "previous_playable" );
                        else previousPlayable = "true";

                        XString profile;
                        if( getArgs.Find( "profile" ) )
                            profile = *getArgs.Find( "profile" );
                        else profile = "DEFAULT";

                        XString qmin;
                        if( getArgs.Find( "qmin" ) )
                            qmin = *getArgs.Find( "qmin" );
                        else qmin = "DEFAULT";

                        XString qmax;
                        if( getArgs.Find( "qmax" ) )
                            qmax = *getArgs.Find( "qmax" );
                        else qmax = "DEFAULT";

                        XString maxQDiff;
                        if( getArgs.Find( "max_qdiff" ) )
                            maxQDiff = *getArgs.Find( "max_qdiff" );
                        else maxQDiff = "DEFAULT";

                        XRef<XTSCache<XRef<TranscodyCacheItem> > > cache = _server.GetTranscodyCache();

                        XIRef<Transcody> transcody = new Transcody( _server.GetConfig(),
                                                                    cache,
                                                                    sessionID,
                                                                    dataSourceID,
                                                                    startTime,
                                                                    endTime,
                                                                    width,
                                                                    height,
                                                                    speed,
                                                                    bitRate,
                                                                    initialQP,
                                                                    framerate,
                                                                    profile,
                                                                    qmin,
                                                                    qmax,
                                                                    maxQDiff );

                        int64_t lastFrameTS = 0;
                        XIRef<XMemory> resultBuffer = transcody->Get( lastFrameTS );

                        response.AddAdditionalHeader( "last-frame-ts",
                                                      XTime::CreateFromUnixTimeAsMSecs( lastFrameTS ).ToISOExtString() );

                        responseWritten = true;

                        _ChunkTransfer( _clientSocket, response, resultBuffer, "video/ts" );
                    }
                }
                else if( fullURI.StartsWith("/progress") )
                {
                    XHash<XString> getArgs = uri.GetGetArgs();
    
                    XString filePath;
                    if( getArgs.Find( "file_path" ) )
                        filePath = *getArgs.Find( "file_path" );
                    else X_STHROW( HTTP400Exception, ("file_path is required for POST on /transcoded_media." ));

                    response.SetContentType("application/json");

                    auto pr = this->_server.GetConfig()->GetProgress(filePath);

                    XString report = XString::Format("{ \"data\": { \"state\": \"%s\", \"progress\": \"%f\" } }", pr.state.c_str(), pr.progress );

                    size_t reportLength = report.length();
                    XIRef<XMemory> body = new XMemory( reportLength );
                    memcpy( &body->Extend( reportLength ), report.c_str(), reportLength );
                    response.SetBody(body);
                }
                else X_STHROW( HTTP404Exception, ("An http get occured against and unknown URI." ));
            }
            else if( requestMethod == "post" )
            {
                if( fullURI == "/server/shutdown" )
                {
                    response.AddAdditionalHeader( "Connection", "Close" );
                    _server.Shutdown();
                }
                else if( fullURI.StartsWith( "/media" ) )
                {
                    response.AddAdditionalHeader( "Connection", "Close" );
                    XHash<XString> getArgs = uri.GetGetArgs();

                    XString dataSourceID;
                    if( getArgs.Find( "data_source_id" ) )
                        dataSourceID = *getArgs.Find( "data_source_id" );
                    else X_STHROW( HTTP400Exception, ("data_source_id is required for /media queries." ));

                    XString startTime;
                    if( getArgs.Find( "start_time" ) )
                        startTime = *getArgs.Find( "start_time" );
                    else X_STHROW( HTTP400Exception, ("start_time is required for /media queries." ));

                    XString endTime;
                    if( getArgs.Find( "end_time" ) )
                        endTime = *getArgs.Find( "end_time" );
                    else X_STHROW( HTTP400Exception, ("end_time is required for /media queries." ));

                    XString fileNameOrPath;
                    bool makesFile = false;
                    if( getArgs.Find( "file_path" ) )
                    {
                        makesFile = true;
                        fileNameOrPath = *getArgs.Find( "file_path" );
                    }
                    else if( getArgs.Find( "file_name" ) )
                    {
                        fileNameOrPath = *getArgs.Find( "file_name" );
                    }
                    else X_STHROW( HTTP400Exception, ("file_path or file_name are required for /media queries.") );

                    auto cfg = this->_server.GetConfig();
                    cfg->UpdateProgress(fileNameOrPath, "preparing", 0.f);

                    XRef<LargeExport> largeExport = new LargeExport( _server.GetConfig(),
                                                                     [cfg, fileNameOrPath](float progress) {
                                                                        cfg->UpdateProgress(fileNameOrPath, "preparing", progress);
                                                                     },
                                                                     dataSourceID,
                                                                     startTime,
                                                                     endTime,
                                                                     fileNameOrPath );

                    if( makesFile )
                    {
                        responseWritten = true;
                        response.WriteResponse( _clientSocket );

                        bool exceptionThrown = false;

                        try
                        {
                            largeExport->Create();
                        }
                        catch(HTTP404Exception& ex)
                        {
                            X_LOG_NOTICE("Export failed: %s",ex.what());
                            cfg->UpdateProgress(fileNameOrPath, "failed", 1.f);
                            exceptionThrown = true;
                        }
                        catch(...)
                        {
                            cfg->UpdateProgress(fileNameOrPath, "failed", 1.f);
                            exceptionThrown = true;
                        }

                        if( !exceptionThrown )
                            cfg->UpdateProgress(fileNameOrPath, "complete", 1.f);
                    }
                    else
                    {
                        XIRef<XMemory> output = new XMemory;

                        largeExport->Create( output );

                        XString ext = largeExport->GetExtension();

                        responseWritten = true;

                        _ChunkTransfer( _clientSocket, response, output, XString::Format( "video/%s", ext.c_str() ) );
                    }
                }
                else if( fullURI.StartsWith( "/transcoded_media" ) )
                {
                    XHash<XString> getArgs = uri.GetGetArgs();

                    XString dataSourceID;
                    if( getArgs.Find( "data_source_id" ) )
                        dataSourceID = *getArgs.Find( "data_source_id" );
                    else X_STHROW( HTTP400Exception, ("data_source_id is required for /transcoded_media queries." ));

                    XString startTime;
                    if( getArgs.Find( "start_time" ) )
                        startTime = *getArgs.Find( "start_time" );
                    else X_STHROW( HTTP400Exception, ("start_time is required for /transcoded_media queries." ));

                    XString endTime;
                    if( getArgs.Find( "end_time" ) )
                        endTime = *getArgs.Find( "end_time" );
                    else X_STHROW( HTTP400Exception, ("end_time is required for /transcoded_media queries." ));

                    XString width;
                    if( getArgs.Find( "width" ) )
                        width = *getArgs.Find( "width" );
                    else width = "1280";

                    XString height;
                    if( getArgs.Find( "height" ) )
                        height = *getArgs.Find( "height" );
                    else height = "720";

                    XString bitRate;
                    if( getArgs.Find( "bit_rate" ) )
                        bitRate = *getArgs.Find( "bit_rate" );
                    else bitRate = DEFAULT_ENCODE_BIT_RATE;

                    XString maxRate;
                    if( getArgs.Find( "max_rate" ) )
                        maxRate = *getArgs.Find( "max_rate" );
                    else maxRate = DEFUALT_ENCODE_MAX_RATE;

                    XString bufSize = "0";
                    if( getArgs.Find( "buf_size" ) )
                        bufSize = *getArgs.Find( "buf_size" );

                    XString framerate;
                    if( getArgs.Find( "framerate" ) )
                        framerate = *getArgs.Find( "framerate" );
                    else framerate = "0.0";

                    XString filePath;
                    if( getArgs.Find( "file_path" ) )
                        filePath = *getArgs.Find( "file_path" );
                    else X_STHROW( HTTP400Exception, ("file_path is required for POST on /transcoded_media." ));

                    XString speed;
                    if( getArgs.Find( "speed" ) )
                        speed = *getArgs.Find( "speed" );
                    else speed = "1.0";

                    XString msg;
                    if( getArgs.Find( "msg" ) )
                        msg = *getArgs.Find( "msg" );

                    bool withTime = true;
                    if( getArgs.Find( "with_time" ) )
                    {
                        XString withTimeString = *getArgs.Find( "with_time" );
                        if( withTimeString.ToLower() == "false" )
                            withTime = false;
                    }

                    OverlayHAlign hAlign = H_ALIGN_LEFT;
                    if( getArgs.Find( "h_align" ) )
                    {
                        XString hAlignString = *getArgs.Find( "h_align" );
                        if( hAlignString.ToLower() == "right" )
                            hAlign = H_ALIGN_RIGHT;
                    }

                    OverlayVAlign vAlign = V_ALIGN_TOP;
                    if( getArgs.Find( "v_align" ) )
                    {
                        XString vAlignString = *getArgs.Find( "v_align" );
                        if( vAlignString.ToLower() == "bottom" )
                            vAlign = V_ALIGN_BOTTOM;
                    }

                    auto cfg = this->_server.GetConfig();
                    cfg->UpdateProgress(filePath, "preparing", 0.f);

                    XRef<TranscodeExport> te = new TranscodeExport( _server.GetConfig(),
                                                                    [cfg, filePath](float progress) {
                                                                        cfg->UpdateProgress(filePath, "preparing", progress);
                                                                    },
                                                                    dataSourceID,
                                                                    startTime,
                                                                    endTime,
                                                                    width.ToUInt16(),
                                                                    height.ToUInt16(),
                                                                    bitRate.ToUInt32(),
                                                                    maxRate.ToUInt32(),
                                                                    bufSize.ToUInt32(),
                                                                    framerate.ToDouble(),
                                                                    filePath,
                                                                    hAlign,
                                                                    vAlign,
                                                                    msg,
                                                                    withTime,
                                                                    speed.ToDouble() );

                    responseWritten = true;
                    response.WriteResponse( _clientSocket );

                    bool exceptionThrown = false;

                    try
                    {
                        te->Create( XIRef<XMemory>() );
                    }
                    catch(HTTP404Exception& ex)
                    {
                        X_LOG_NOTICE("Export failed: %s",ex.what());
                        cfg->UpdateProgress(filePath, "failed", 1.f);
                        exceptionThrown = true;
                    }
                    catch(...)
                    {
                        cfg->UpdateProgress(filePath, "failed", 1.f);
                        exceptionThrown = true;
                    }

                    if( !exceptionThrown )
                        cfg->UpdateProgress(filePath, "complete", 1.f);
                }
                else X_STHROW( HTTP400Exception, ("An http post occured against and unknown URI." ));
            }
            else if( requestMethod == "put" )
            {
                X_STHROW( HTTP400Exception, ("An http put occured against and unknown URI." ));
            }
            else if( requestMethod == "delete" )
            {
                response.AddAdditionalHeader( "Connection", "Close" );

                if( fullURI.StartsWith( "/media" ) )
                {
                    XHash<XString> getArgs = uri.GetGetArgs();

                    XString filePath;
                    if( getArgs.Find( "file_path" ) )
                        filePath = *getArgs.Find( "file_path" );
                    else X_STHROW( HTTP400Exception, ("file_path is required for /media deletes." ));

                    int err = 0;
                    if( !XPath::Exists("/data/exports/save_export") )
                        err = UNLINK( filePath.c_str() );

                    if( err < 0 )
                        X_STHROW( HTTP500Exception,
                                  (XString::Format("Unable to delete %s", filePath.c_str()) .c_str()) );
                }
                else if( fullURI.StartsWith( "/transcoded_media" ) )
                {
                    XHash<XString> getArgs = uri.GetGetArgs();

                    XString sessionID;
                    if( getArgs.Find( "session" ) )
                        sessionID = *getArgs.Find( "session" );
                    else X_STHROW( HTTP400Exception,
                                   ("DELETE on /transcoded_media requires a session.") );

                    X_LOG_NOTICE("GOT DELETE on SESSION %s",sessionID.c_str());
                }
                else X_STHROW( HTTP404Exception, ("An http delete occured against and unknown URI." ));
            }
            else X_STHROW( HTTP400Exception, ("A request with an unsupported HTTP method was received." ));

        }
        CATCH_TRANSLATE_HTTP_EXCEPTIONS(response)

        // Certain cases write their own response, and so we only write here if we still need
        // to.
        if( !responseWritten )
            response.WriteResponse( _clientSocket );

        _clientSocket->Close();
    }
    catch( XException& ex )
    {
        X_LOG_XSDK_EXCEPTION(ex);
    }
    catch( exception& ex )
    {
        X_LOG_STD_EXCEPTION(ex);
    }

    _doneTime = Clock::currTime();

    _done = true;

    return NULL;
}

bool Request::IsDone() const
{
    if( _done )
    {
        if( (Clock::currTime() - _doneTime) > XDuration( SECONDS, 60 ) )
            return true;
    }

    return false;
}

void Request::_ChunkTransfer( XRef<XSocket> sok,
                              ServerSideResponse& response,
                              XIRef<XMemory> responseBody,
                              const XString& type ) const
{
    uint32_t numChunks = (responseBody->GetDataSize()>MEDIA_CHUNK_SIZE) ? responseBody->GetDataSize() / MEDIA_CHUNK_SIZE : 0;

    uint32_t remainderBytes = (numChunks>0) ? responseBody->GetDataSize() % MEDIA_CHUNK_SIZE : responseBody->GetDataSize();

    XBytePtr p( responseBody->Map(), responseBody->GetDataSize() );

    vector<XString> typeParts;
    type.Split( "/", typeParts );

    if( typeParts.size() < 2 )
        X_THROW(("Invalid mime type."));

    response.SetContentType( "video/" + typeParts[1] );

    bool chunksWritten = false;

    for( uint32_t i = 0; i < numChunks; i++ )
    {
        XIRef<XMemory> chunk = new XMemory;

        memcpy( &chunk->Extend( MEDIA_CHUNK_SIZE ), p.GetPtr(), MEDIA_CHUNK_SIZE );

        p += MEDIA_CHUNK_SIZE;

        response.WriteChunk( sok, chunk );

        chunksWritten = true;
    }

    // remainder chunk...

    if( remainderBytes > 0 )
    {
        XIRef<XMemory> chunk = new XMemory;

        memcpy( &chunk->Extend( remainderBytes ), p.GetPtr(), remainderBytes );

        response.WriteChunk( sok, chunk );

        chunksWritten = true;
    }

    if( chunksWritten )
        response.WriteChunkFinalizer( sok );
}
