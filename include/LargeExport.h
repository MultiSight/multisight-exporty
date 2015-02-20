
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// XSDK
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#ifndef __EXPORTY_LargeExport_h
#define __EXPORTY_LargeExport_h

#include "MediaParser/MediaParser.h"
#include "FrameStoreClient/ResultParser.h"
#include "FrameStoreClient/RecorderURLS.h"
#include "AVKit/AVMuxer.h"
#include "AVKit/Options.h"

#include "Config.h"

namespace EXPORTY
{

class LargeExport : public XSDK::XBaseObject
{
public:
    LargeExport( XRef<Config> config,
                 const XSDK::XString& dataSourceID,
                 const XSDK::XString& startTime,
                 const XSDK::XString& endTime,
                 const XSDK::XString& fileName );

    virtual ~LargeExport() throw();

    void Create( XIRef<XSDK::XMemory> output = XIRef<XSDK::XMemory>() );

    XSDK::XString GetExtension() const;

private:
    LargeExport( const LargeExport& obj );
    LargeExport& operator = ( const LargeExport& obj );

    XIRef<XSDK::XMemory> _GetExtraData( XIRef<XSDK::XMemory> sps, XIRef<XSDK::XMemory> pps ) const;

    XSDK::XString _GetTMPName( const XSDK::XString& fileName ) const;

    int _MeasureGOP( XIRef<XSDK::XMemory> responseBuffer ) const;

    struct AVKit::CodecOptions _GetCodecOptions( const XIRef<XSDK::XMemory> sps,
                                                 const XSDK::XString& sdpFrameRate,
                                                 int gopSize ) const;

    XRef<Config> _config;
    XSDK::XString _dataSourceID;
    XSDK::XString _startTime;
    XSDK::XString _endTime;
    XSDK::XString _fileName;
    XRef<FRAME_STORE_CLIENT::RecorderURLS> _recorderURLS;
    XRef<AVKit::AVMuxer> _muxer;
    XSDK::XString _extension;
};

}

#endif
