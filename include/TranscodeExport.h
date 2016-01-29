
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
#include <cairo.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>

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

class ExportOverlay
{
public:
    ExportOverlay( const XSDK::XString& msg,
                   bool withTime,
                   XSDK::XTime clockTime,
                   OverlayHAlign hAlign,
                   OverlayVAlign vAlign,
                   uint16_t width,
                   uint16_t height,
                   int timeBaseNum,
                   int timeBaseDen );

    virtual ~ExportOverlay() throw();

    XIRef<AVKit::Packet> Process( XIRef<AVKit::Packet> input );

private:
    void _GetXPositions( uint16_t& timeX, uint16_t& msgX, uint16_t messageWidth );
    void _DrawMessage( cairo_t* cr, PangoLayout* layout, uint16_t msgX, uint16_t y );
    void _DrawTime( cairo_t* cr, uint16_t timeX, uint16_t y );

    XSDK::XString _msg;
    XSDK::XString _decodedMsg;
    bool _withTime;
    XSDK::XTime _clockTime;
    OverlayHAlign _hAlign;
    OverlayVAlign _vAlign;
    uint16_t _width;
    uint16_t _height;
    int _timeBaseNum;
    int _timeBaseDen;
    double _timePerFrame;
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

    void _FinishInit( XRef<AVKit::H264Encoder>& encoder,
                      XRef<AVKit::AVMuxer>& muxer,
                      XRef<ExportOverlay>& ov,
                      AVKit::H264Decoder& decoder,
                      const XSDK::XString& tempFileName,
                      bool outputToFile,
                      int traversalNum,
                      int traversalDen );

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
