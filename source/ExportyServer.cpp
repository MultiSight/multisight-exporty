
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "ExportyServer.h"

using namespace XSDK;
using namespace EXPORTY;
using namespace WEBBY;
using namespace std;

static const int SERVER_PORT = 10013;
static const size_t MAX_TRANSCODY_CACHE = 4;

ExportyServer::ExportyServer() :
    XBaseObject(),
    XTaskBase(),
    _running( false ),
    _serverSocket(),
    _activeRequests(),
    _config( new Config ),
    _transcodyCache( new XTSCache<XRef<TranscodyCacheItem> >( MAX_TRANSCODY_CACHE ) )
{
    XString logFilePath = _config->GetLogFilePath();
    if( !logFilePath.empty() )
        XLog::EnablePrintToFile( logFilePath, true );
}

ExportyServer::~ExportyServer() throw()
{
}

void* ExportyServer::EntryPoint()
{
    X_LOG_INFO("ExportyServer: Starting up.");

    _ConfigureServerSocket();

    _running = true;

    while( _running )
    {
        int timeout = 200;
        if( !_serverSocket.WaitRecv( timeout ) )
        {
            XIRef<Request> request = new Request( *this );

            XRef<XSocket>& clientSok = request->GetClientSocket();
            clientSok = _serverSocket.Accept();

            clientSok->Linger( 0 );

            clientSok->EnableAutoServerClose( 5000 );

            if( !_running )
                continue;

            if( clientSok->BufferedRecv() )
            {
                X_LOG_INFO("ExportyServer: accepted new connection.");

                request->Start();

                _activeRequests.push_back( request );
            }
        }

        _PruneDone();

    }

    X_LOG_INFO( "ExportyServer: Exiting.\n");

    return NULL;
}

void ExportyServer::Shutdown()
{
    _running = false;
}

XRef<Config> ExportyServer::GetConfig() const
{
    return _config;
}

XRef<XTSCache<XRef<TranscodyCacheItem> > > ExportyServer::GetTranscodyCache() const
{
    return _transcodyCache;
}

void ExportyServer::_ConfigureServerSocket()
{
    _serverSocket.Bind( SERVER_PORT );
    _serverSocket.Listen();
}

void ExportyServer::_PruneDone()
{

retry:

    list<XIRef<Request> >::iterator i = _activeRequests.begin();

    while( i != _activeRequests.end() )
    {
        if( (*i)->IsDone() )
        {
            (*i)->Join();
            _activeRequests.erase( i );
            goto retry;
        }

        i++;
    }
}

