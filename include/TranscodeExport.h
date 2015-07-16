
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#ifndef __EXPORTY_TranscodeExport_h
#define __EXPORTY_TranscodeExport_h

#include "Config.h"
#include "XSDK/XTime.h"
#include "XSDK/XMemory.h"
#include "XSDK/XRef.h"
#include "FrameStoreClient/RecorderURLS.h"
#include "AVKit/H264Decoder.h"
#include "AVKit/H264Encoder.h"
#include "AVKit/AVMuxer.h"

namespace EXPORTY
{

enum OverlayHAlign
{
    H_ALIGN_LEFT,
    H_ALIGN_RIGHT
};

enum OverlayVAlign
{
    V_ALIGN_TOP,
    V_ALIGN_BOTTOM
};

class TranscodeExport
{
public:
    TranscodeExport( XRef<Config> config,
                     const XSDK::XString& dataSourceID,
                     const XSDK::XString& startTime,
                     const XSDK::XString& endTime,
                     uint16_t width,
                     uint16_t height,
                     uint32_t bitRate,
                     double frameRate,
                     const XSDK::XString& fileName,
                     OverlayHAlign hAlign,
                     OverlayVAlign vAlign,
                     const XSDK::XString& msg,
                     bool withTime,
                     double speed = 1.0 );

    virtual ~TranscodeExport() throw();

    void Create( XIRef<XSDK::XMemory> output );

private:
    TranscodeExport( const TranscodeExport& );
    TranscodeExport& operator = ( const TranscodeExport& );

    XSDK::XString _GetTMPName( const XSDK::XString& fileName ) const;

    void _CreateEncoder( XRef<AVKit::H264Encoder>& encoder,
                         XRef<AVKit::AVMuxer>& muxer,
                         AVKit::H264Decoder& decoder,
                         const XSDK::XString& tempFileName,
                         bool outputToFile );

    XRef<Config> _config;
    XSDK::XString _dataSourceID;
    XSDK::XString _startTime;
    XSDK::XString _endTime;
    uint16_t _requestedWidth;
    uint16_t _requestedHeight;
    uint32_t _bitRate;
    double _frameRate;
    XSDK::XString _fileName;
    OverlayHAlign _hAlign;
    OverlayVAlign _vAlign;
    XSDK::XString _msg;
    bool _withTime;
    double _speed;
    FRAME_STORE_CLIENT::RecorderURLS _recorderURLS;
};

}

#endif
