
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#ifndef __EXPORTY_Request_h
#define __EXPORTY_Request_h

#include "XSDK/XTaskBase.h"
#include "XSDK/XSocket.h"
#include "XSDK/XTime.h"
#include "Webby/ServerSideRequest.h"
#include "Webby/ServerSideResponse.h"

namespace EXPORTY
{

class ExportyServer;

class Request : public XSDK::XBaseObject, public XSDK::XTaskBase
{
public:
    X_API Request( ExportyServer& server );
    X_API virtual ~Request() throw();

    X_API virtual void* EntryPoint();

    X_API bool IsDone() const;

    X_API inline XRef<XSDK::XSocket>& GetClientSocket() { return _clientSocket; }

private:
    void _ChunkTransfer( XRef<XSDK::XSocket> sok,
                         WEBBY::ServerSideResponse& response,
                         XIRef<XSDK::XMemory> responseBody,
                         const XSDK::XString& type ) const;

    ExportyServer& _server;
    XRef<XSDK::XSocket> _clientSocket;
    WEBBY::ServerSideRequest _httpRequest;
    bool _done;
    XSDK::XTime _doneTime;
};

}

#endif
