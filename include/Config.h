
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

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

    X_API bool HasDRIEncoder() const;
    X_API bool HasDRIDecoder() const;

    X_API int GetTranscodeSleep() const;

private:
    Config( const Config& obj );
    Config& operator = ( const Config& obj );

    XSDK::XString _recorderIP;
    int _recorderPort;
    XSDK::XString _logFilePath;
    bool _hasDRIEncoding;
    bool _hasDRIDecoding;
    int _transcodeSleep;
};

}

#endif
