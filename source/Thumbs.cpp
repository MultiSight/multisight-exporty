
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "Thumbs.h"

#include "XSDK/XSocket.h"
#include "XSDK/XTime.h"
#include "Webby/ClientSideRequest.h"
#include "Webby/ClientSideResponse.h"
#include "Webby/WebbyException.h"
#include "AVKit/Utils.h"
#include "AVKit/H264Decoder.h"
#include "AVKit/JPEGEncoder.h"
#include "AVKit/Options.h"
#include "FrameStoreClient/Frame.h"

using namespace EXPORTY;
using namespace XSDK;
using namespace WEBBY;
using namespace std;
using namespace AVKit;

namespace EXPORTY
{

// Our globally defined master Lock...
XSDK::XMutex locky;

static struct CodecOptions _GetDecoderCodecOptions()
{
    struct CodecOptions options = GetFastH264DecoderOptions();

    options.jpeg_source = true; // We add jpeg_source here so YUVJ420P is used by scaler context.

    return options;
}

XIRef<XMemory> CreateJPEGThumbnail( XRef<Config> config,
                                    const XString& dataSourceID,
                                    const XString& time,
                                    uint16_t destWidth,
                                    uint16_t destHeight,
                                    uint32_t qmax,
                                    uint32_t qmin,
                                    uint32_t bitRate )
{
    XGuard g( locky );

    XRef<H264Decoder> decoder = new H264Decoder( _GetDecoderCodecOptions() );

    XIRef<XMemory> frame = FRAME_STORE_CLIENT::FetchFrame( config->GetRecorderIP(),
                                                           config->GetRecorderPort(),
                                                           dataSourceID,
                                                           time );

    XIRef<Packet> pkt = new Packet;
    pkt->Config( frame->Map(), frame->GetDataSize(), false );

    decoder->Decode( pkt );

    uint16_t correctedWidth = 0, correctedHeight = 0;

    AspectCorrectDimensions( decoder->GetInputWidth(), decoder->GetInputHeight(),
                             destWidth, destHeight,
                             correctedWidth, correctedHeight );

    decoder->SetOutputWidth( correctedWidth );
    decoder->SetOutputHeight( correctedHeight );

    XRef<JPEGEncoder> encoder = new JPEGEncoder( GetJPEGOptions( correctedWidth, correctedHeight, bitRate, qmin, qmax ) );

    encoder->EncodeYUV420P( decoder->Get() );

    // This code can be optimized... If the return value of this function was modified to be AVPacket, we could avoid
    // a buffer copy here...

    XIRef<Packet> encodedPkt = encoder->Get();

    XIRef<XMemory> result = new XMemory;

    memcpy( &result->Extend( encodedPkt->GetDataSize() ), encodedPkt->Map(), encodedPkt->GetDataSize() );

    return result;
}

}
