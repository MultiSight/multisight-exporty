cmake_minimum_required(VERSION 2.8)
project(Exporty)

include(common.cmake NO_POLICY_SCOPE)
include(packaging.cmake NO_POLICY_SCOPE)

set(SOURCES source/Main.cpp
            source/Config.cpp
            source/Request.cpp
            source/ExportyServer.cpp
            source/LargeExport.cpp
            source/Thumbs.cpp
            source/Transcody.cpp
            source/TranscodeExport.cpp)

set(WINDOWS_LIBS XSDK ws2_32 Webby MediaParser FrameStoreClient AVKit)
set(LINUX_LIBS XSDK Webby MediaParser FrameStoreClient AVKit VAKit)

add_definitions(-D__STDC_CONSTANT_MACROS)

if(CMAKE_SYSTEM MATCHES "Windows")
    set(RC_FILE "Exporty.rc")
    set(APPLICATION_TYPE "WINDOWS_CONSOLE")
elseif(CMAKE_SYSTEM MATCHES "Linux")
    set(APPLICATION_TYPE "NORMAL")
#    include_directories(/usr/include/ffmpeg)
endif(CMAKE_SYSTEM MATCHES "Windows")

include("${devel_artifacts_path}/build/base_app.cmake" NO_POLICY_SCOPE)

install(TARGETS Exporty DESTINATION Exporty)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/exporty_exec_start_pre PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE DESTINATION Exporty)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/exportyd.service DESTINATION /lib/systemd/system)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/runExporty DESTINATION Exporty)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/60_exporty_rsyslog.conf DESTINATION /etc/rsyslog.d)
if(CMAKE_SYSTEM MATCHES "Windows")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/windows/config.xml DESTINATION Exporty)
elseif(CMAKE_SYSTEM MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/linux/config.xml DESTINATION Exporty)
endif(CMAKE_SYSTEM MATCHES "Windows")
