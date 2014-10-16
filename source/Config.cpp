
#include "Config.h"

#include "XSDK/XPath.h"
#include "XSDK/XDomParser.h"
#include "XSDK/XDomParserNode.h"

using namespace XSDK;
using namespace std;
using namespace EXPORTY;

Config::Config() :
    _recorderIP( "127.0.0.1" ),
    _recorderPort( 10013 ),
    _logFilePath( "" )
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
