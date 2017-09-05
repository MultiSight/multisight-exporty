
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
#include "XSDK/XMutex.h"
#include "XSDK/XCache.h"

namespace EXPORTY
{

struct ProgressReport
{
    bool working;
    float progress;
};

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

    X_API bool EnableDecodeSkipping() const;

    X_API void UpdateProgress( const XSDK::XString& dataSourceID, float progress );
    X_API ProgressReport GetProgress( const XSDK::XString& dataSourceID );

private:
    Config( const Config& obj );
    Config& operator = ( const Config& obj );

    XSDK::XString _recorderIP;
    int _recorderPort;
    XSDK::XString _logFilePath;
    bool _hasDRIEncoding;
    bool _hasDRIDecoding;
    int _transcodeSleep;
    bool _enableDecodeSkipping;
    XSDK::XMutex _cacheLok;
    XSDK::XCache<ProgressReport> _progressCache;
};

}

#endif
