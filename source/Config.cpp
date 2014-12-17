
#include "Config.h"

#include "XSDK/XPath.h"
#include "XSDK/XDomParser.h"
#include "XSDK/XDomParserNode.h"
#include "VAKit/VAH264Encoder.h"
#include "VAKit/VAH264Decoder.h"
#include "AVKit/Options.h"

using namespace XSDK;
using namespace std;
using namespace EXPORTY;
using namespace AVKit;
using namespace VAKit;

Config::Config() :
    _recorderIP( "127.0.0.1" ),
    _recorderPort( 10013 ),
    _logFilePath( "" ),
    _hasDRIEncoding( false ),
    _hasDRIDecoding( false )
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
    }

    try
    {
#ifndef WIN32
        CodecOptions options;
        options.width = 640;
        options.height = 360;
        options.bit_rate = 125000;
        options.gop_size = 15;
        options.time_base_num = 1;
        options.time_base_den = 15;
        options.device_path = "/dev/dri/card0";

        XRef<VAH264Encoder> encoder = new VAH264Encoder( options, "/dev/dri/card0" );

        _hasDRIEncoding = true;
#endif
    }
    catch(...)
    {
        X_LOG_NOTICE("/dev/dri/card0 device not supported for encoding.");
    }

    try
    {
#ifndef WIN32
        XRef<VAH264Decoder> encoder = new VAH264Decoder( GetFastH264DecoderOptions( "/dev/dri/card0" ) );

        _hasDRIDecoding = true;
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
