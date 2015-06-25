
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#ifndef __EXPORTY_TranscodyCacheItem_h
#define __EXPORTY_TranscodyCacheItem_h

#include "AVKit/H264Decoder.h"
#include "AVKit/H264Encoder.h"
#include "AVKit/AVMuxer.h"
#include "AVKit/Encoder.h"
#include "AVKit/Decoder.h"
#include "XSDK/XRef.h"
#include "XSDK/XMutex.h"

namespace EXPORTY
{

struct TranscodyCacheItem
{
    XRef<AVKit::Decoder> decoder;
    XRef<AVKit::Encoder> encoder;
    XRef<AVKit::AVMuxer> muxer;
    double framerateStep;
    XSDK::XMutex sessionLok;
};

}

#endif
