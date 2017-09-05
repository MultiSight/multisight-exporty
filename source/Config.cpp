
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "Config.h"

#include "XSDK/XPath.h"
#include "XSDK/XDomParser.h"
#include "XSDK/XDomParserNode.h"
#include "XSDK/XGuard.h"
#ifndef WIN32
#include "VAKit/VAH264Encoder.h"
#include "VAKit/VAH264Decoder.h"
#endif
#include "AVKit/Options.h"

using namespace XSDK;
using namespace std;
using namespace EXPORTY;
using namespace AVKit;
#ifndef WIN32
using namespace VAKit;
#endif

Config::Config() :
    _recorderIP( "127.0.0.1" ),
    _recorderPort( 10013 ),
    _logFilePath( "" ),
    _hasDRIEncoding( false ),
    _hasDRIDecoding( false ),
    _transcodeSleep( 0 ),
    _enableDecodeSkipping( false ),
    _cacheLok(),
    _progressCache(10)
{
    if( XPath::Exists( "config.xml" ) )
    {
        XIRef<XDomParser> domParser = new XDomParser;

        domParser->OpenAndSetDocument( "config.xml" );

        XIRef<XDomParserNode> rootNode = domParser->Parse();

        {
            list<XIRef<XDomParserNode> > searchResults = domParser->SearchForAll( "recorder_ip", rootNode );

            if( !searchResults.empty() )
                _recorderIP = searchResults.front()->GetData();
        }

        {
            list<XIRef<XDomParserNode> > searchResults = domParser->SearchForAll( "recorder_port", rootNode );

            if( !searchResults.empty() )
                _recorderPort = searchResults.front()->GetData().ToInt();
        }

        {
            list<XIRef<XDomParserNode> > searchResults = domParser->SearchForAll( "log_file_path", rootNode );

            if( !searchResults.empty() )
                _logFilePath = searchResults.front()->GetData();
        }

        {
            list<XIRef<XDomParserNode> > searchResults = domParser->SearchForAll( "transcode_sleep", rootNode );

            if( !searchResults.empty() )
                _transcodeSleep = searchResults.front()->GetData().ToInt();
        }

        {
            list<XIRef<XDomParserNode> > searchResults = domParser->SearchForAll( "decode_skipping", rootNode );

            if( !searchResults.empty() )
                _enableDecodeSkipping = searchResults.front()->GetData().ToInt() != 0;
        }

    }

    try
    {
#ifndef WIN32
        _hasDRIEncoding = VAH264Encoder::HasHW( "/dev/dri/card0" );
#endif
    }
    catch(...)
    {
        X_LOG_NOTICE("/dev/dri/card0 device not supported for encoding.");
    }

    try
    {
#ifndef WIN32
        _hasDRIDecoding = VAH264Decoder::HasHW( "/dev/dri/card0" );
#endif
    }
    catch(...)
    {
        X_LOG_NOTICE("/dev/dri/card0 device not supported for encoding.");
    }
}

Config::~Config() throw()
{
}

XString Config::GetRecorderIP() const
{
    return _recorderIP;
}

int Config::GetRecorderPort() const
{
    return _recorderPort;
}

XString Config::GetLogFilePath() const
{
    return _logFilePath;
}

bool Config::HasDRIEncoder() const
{
    return _hasDRIEncoding;
}

bool Config::HasDRIDecoder() const
{
    return _hasDRIDecoding;
}

int Config::GetTranscodeSleep() const
{
    return _transcodeSleep;
}

bool Config::EnableDecodeSkipping() const
{
    return _enableDecodeSkipping;
}

void Config::UpdateProgress( const XString& dataSourceID, float progress )
{
    XGuard g(_cacheLok);

    ProgressReport pr;
    pr.working = (progress<1.0) ? true : false;
    pr.progress = progress;

    _progressCache.Put( dataSourceID, pr );
}

ProgressReport Config::GetProgress( const XString& dataSourceID )
{
    XGuard g(_cacheLok);

    ProgressReport pr;
    if( !_progressCache.Get( dataSourceID, pr ) )
    {
        pr.working = false;
        pr.progress = 0.0;
    }

    return pr;
}
