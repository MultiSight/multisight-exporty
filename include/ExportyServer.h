
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#ifndef __EXPORTY_ExportyServer_h
#define __EXPORTY_ExportyServer_h

#include "Request.h"
#include "TranscodyCacheItem.h"
#include "Config.h"

#include "XSDK/XCache.h"

namespace EXPORTY
{

class ExportyServer: public XSDK::XBaseObject, public XSDK::XTaskBase
{
public:
    X_API ExportyServer();
    X_API virtual ~ExportyServer() throw();

    X_API virtual void* EntryPoint();

    X_API static int Run( int argc, char* argv[] );

    X_API void Shutdown();

    X_API XRef<Config> GetConfig() const;

    X_API XRef<XSDK::XTSCache<XRef<TranscodyCacheItem> > > GetTranscodyCache() const;

private:
    ExportyServer( const ExportyServer& obj );
    ExportyServer& operator = ( const ExportyServer& obj );

    void _ConfigureServerSocket();
    void _PruneDone();

    bool _running;
    XSDK::XSocket _serverSocket;

    std::list<XIRef<Request> > _activeRequests;
    XRef<Config> _config;

    XRef<XSDK::XTSCache<XRef<TranscodyCacheItem> > > _transcodyCache;
};

}

#endif
