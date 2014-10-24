
#ifndef __EXPORTY_TranscodyCacheItem_h
#define __EXPORTY_TranscodyCacheItem_h

#include "AVKit/H264Decoder.h"
#include "AVKit/H264Encoder.h"
#include "AVKit/AVMuxer.h"
#include "XSDK/XRef.h"
#include "XSDK/XMutex.h"

namespace EXPORTY
{

struct TranscodyCacheItem
{
    XRef<AVKit::H264Decoder> decoder;
    XRef<AVKit::H264Encoder> encoder;
    XRef<AVKit::AVMuxer> muxer;
    double framerateStep;
    XSDK::XMutex sessionLok;
};

}

#endif