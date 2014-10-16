
#ifndef __EXPORTY_Config_h
#define __EXPORTY_Config_h

#include "XSDK/XBaseObject.h"

namespace EXPORTY
{

class Config
{
public:
    X_API Config();
    X_API virtual ~Config() throw();

    X_API XSDK::XString GetRecorderIP() const;
    X_API int GetRecorderPort() const;

    X_API XSDK::XString GetLogFilePath() const;

private:
    Config( const Config& obj );
    Config& operator = ( const Config& obj );

    XSDK::XString _recorderIP;
    int _recorderPort;
    XSDK::XString _logFilePath;
};

}

#endif
