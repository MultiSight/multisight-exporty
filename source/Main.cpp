
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

#include "AVKit/Locky.h"

#ifdef IS_LINUX
#include <signal.h>
#endif

#ifdef IS_WINDOWS
#include <windows.h>
#include <tchar.h>
#endif

using namespace EXPORTY;
using namespace XSDK;
using namespace AVKit;

static XIRef<ExportyServer> exportyServer;

#ifdef IS_WINDOWS

TCHAR* serviceName = TEXT("Exporty Service");
SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle = 0;

void WINAPI ServiceControlHandler( DWORD controlCode )
{
    switch ( controlCode )
    {
    case SERVICE_CONTROL_INTERROGATE:
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );
        break;

    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );

        exportyServer->Shutdown();

        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );
        return;

    case SERVICE_CONTROL_PAUSE:
        break;

    case SERVICE_CONTROL_CONTINUE:
        break;

    default:
        if ( controlCode >= 128 && controlCode <= 255 )
            // user defined control code
            break;
        else
            // unrecognised control code
            break;
    }

    SetServiceStatus( serviceStatusHandle, &serviceStatus );
}

VOID WINAPI ServiceMain( DWORD argc, LPTSTR* argv )
{
    serviceStatus.dwServiceType = SERVICE_WIN32;
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwWin32ExitCode = NO_ERROR;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;

    exportyServer = new ExportyServer;
    exportyServer->Start();

    serviceStatusHandle = RegisterServiceCtrlHandler( serviceName, ServiceControlHandler );

    serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
    serviceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus( serviceStatusHandle, &serviceStatus );

    exportyServer->Join();
    exportyServer.Clear();
}

void RunService()
{
    SERVICE_TABLE_ENTRY sti[] =
    {
        {
            serviceName,
            ServiceMain
        },
        {
            0,
            0
        }
    };

    if( !StartServiceCtrlDispatcher( sti ) )
        X_THROW(("Unable to start service control dispatcher: %s", GetLastErrorMsg().c_str() ));
}

void InstallService()
{
    SC_HANDLE serviceControlManager = OpenSCManager( 0, 0, SC_MANAGER_CREATE_SERVICE );
    if( !serviceControlManager )
        X_THROW(("Unable to open service control manager: %s",GetLastErrorMsg().c_str()));

    TCHAR path[ _MAX_PATH + 1 ];
    if ( !GetModuleFileName( 0, path, sizeof(path)/sizeof(path[0]) ) )
        X_THROW(("Unable to get module file name."));

    SC_HANDLE service = CreateService( serviceControlManager,
                                       serviceName,
                                       serviceName,
                                       SERVICE_ALL_ACCESS,
                                       SERVICE_WIN32_OWN_PROCESS,
                                       SERVICE_AUTO_START,
                                       SERVICE_ERROR_IGNORE,
                                       path, 0, 0, 0, 0, 0 );

    if( !service )
        X_THROW(("Unable to create service."));

    CloseServiceHandle( service );

    CloseServiceHandle( serviceControlManager );
}

void UninstallService()
{
    SC_HANDLE serviceControlManager = OpenSCManager( 0, 0, SC_MANAGER_CONNECT );

    if ( serviceControlManager )
    {
        SC_HANDLE service = OpenService( serviceControlManager,
                                         serviceName, SERVICE_QUERY_STATUS | DELETE );
        if ( service )
        {
            SERVICE_STATUS serviceStatus;
            if ( QueryServiceStatus( service, &serviceStatus ) )
            {
                if ( serviceStatus.dwCurrentState == SERVICE_STOPPED )
                    DeleteService( service );
            }

            CloseServiceHandle( service );
        }

        CloseServiceHandle( serviceControlManager );
    }
}

LONG GetStringRegKey( HKEY key,
                      const std::wstring& valueName,
                      std::wstring& value,
                      const std::wstring& defaultValue )
{
    value = defaultValue;
    WCHAR szBuffer[512];
    DWORD dwBufferSize = sizeof( szBuffer );
    ULONG nError;

    nError = RegQueryValueExW( key, valueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize );

    if (ERROR_SUCCESS == nError)
        value = szBuffer;

    return nError;
}

int main( int argc, char* argv[] )
{
    XLog::InstallLogLevelSigHandler();
    XLog::SetLogLevel( LOGLEVEL_INFO );
    XLog::InstallTerminate();

    Locky::RegisterFFMPEG();

    XString firstArg;
    if( argc > 1 )
        firstArg = argv[1];

    try
    {
        if( firstArg == "--interactive" )
        {
            exportyServer = new ExportyServer();

            exportyServer->Start();

            exportyServer->Join();

            exportyServer.Clear();
        }
        else
        {
            HKEY key;
            LONG res = RegOpenKeyExW( HKEY_LOCAL_MACHINE, XString("Software\\Schneider Electric\\Exporty\\Settings").get_wide_string().c_str(), 0, KEY_READ, &key );
            if( res != ERROR_SUCCESS )
                X_THROW(("Unable to find installation directory in registry."));

            std::wstring strValueOfInstallPath;
            GetStringRegKey( key, XString("InstallPath").get_wide_string().c_str(), strValueOfInstallPath, XString("bad").get_wide_string().c_str() );

            if( strValueOfInstallPath != XString("bad").get_wide_string().c_str() )
                SetCurrentDirectory( strValueOfInstallPath.c_str() );

            if( firstArg == "--install" )
                InstallService();
            else if( firstArg == "--uninstall" )
                UninstallService();
            else RunService();
        }
    }
    catch(XException& ex)
    {
        printf("%s",ex.what());
        X_LOG_XSDK_EXCEPTION(ex);
    }
    catch(exception& ex)
    {
        printf("%s",ex.what());
        X_LOG_STD_EXCEPTION(ex);
    }
    catch(...)
    {
        X_LOG_NOTICE("An exception has occured.");
    }

    Locky::UnregisterFFMPEG();

    return 0;
}

#else

void HandleTerminate( int sigNum )
{
    if( sigNum == SIGTERM )
    {
        if( exportyServer.IsValid() )
        {
            exportyServer->Shutdown();
        }
    }
}

int main( int argc, char* argv[] )
{
    XLog::InstallLogLevelSigHandler();
    XLog::SetLogLevel( LOGLEVEL_INFO );
    XLog::InstallTerminate();

    Locky::RegisterFFMPEG();

    XString firstArg;

    if( argc > 1 )
        firstArg = argv[1];

    if( firstArg != "--interactive" )
        daemon( 1, 0 );

    exportyServer = new ExportyServer();

    signal( SIGTERM, HandleTerminate );

    exportyServer->Start();

    exportyServer->Join();

    exportyServer.Clear();

    Locky::UnregisterFFMPEG();

    return 0;
}

#endif
