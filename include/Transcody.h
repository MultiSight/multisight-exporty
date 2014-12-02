
#ifndef __EXPORTY_Transcody_h
#define __EXPORTY_Transcody_h

#include "MediaParser/MediaParser.h"
#include "FrameStoreClient/ResultParser.h"
#include "XSDK/XHash.h"
#include "XSDK/XCache.h"
#include "AVKit/H264Decoder.h"
#include "AVKit/Encoder.h"
#include "Config.h"
#include "TranscodyCacheItem.h"

namespace EXPORTY
{

class Transcody : public XSDK::XBaseObject
{
public:
    Transcody( XRef<Config> config,
               XRef<XSDK::XTSCache<XRef<TranscodyCacheItem> > > cache,
               const XSDK::XString& sessionID,
               const XSDK::XString& dataSourceID,
               const XSDK::XString& startTime,
               const XSDK::XString& endTime,
               const XSDK::XString& width,
               const XSDK::XString& height,
               const XSDK::XString& bitRate,
               const XSDK::XString& framerate,
               const XSDK::XString& profile,
               const XSDK::XString& qmin,
               const XSDK::XString& qmax,
               const XSDK::XString& maxQDiff );

    virtual ~Transcody() throw();

    XIRef<XSDK::XMemory> Get( int64_t& lastFrameTS );

private:
    Transcody( const Transcody& obj );
    Transcody& operator = ( const Transcody& obj );

    void _SleepTillThePast() const;

    int _GetNumFramesToExport( XIRef<XSDK::XMemory> result, int64_t requestedDuration ) const;

    double _GetFramerate( const XIRef<FRAME_STORE_CLIENT::ResultParser> resultParser ) const;

    void _PopulateSessionCache();

    void _FinishConfig( XRef<AVKit::H264Decoder> decoder,
                        XRef<AVKit::Encoder>& encoder,
                        XRef<AVKit::AVMuxer>& muxer );

    void _UpdateConfig( XRef<AVKit::H264Decoder> decoder,
                        XRef<AVKit::Encoder>& encoder,
                        XRef<AVKit::AVMuxer> muxer );

    XRef<AVKit::Encoder> _CreateEncoder( XRef<AVKit::H264Decoder> decoder );

    bool _EncoderNeedsInit( XRef<AVKit::Encoder> encoder );

    void _GetResolution( const XIRef<XSDK::XMemory> sps, uint16_t& width, uint16_t& height ) const;

    XRef<Config> _config;
    XRef<XSDK::XTSCache<XRef<TranscodyCacheItem> > > _cache;
    XSDK::XString _sessionID;
    XSDK::XString _dataSourceID;
    XSDK::XString _startTime;
    XSDK::XString _endTime;
    XSDK::XString _type;

    uint16_t _width;
    uint16_t _height;

    uint32_t _bitRate;
    double _framerate;
    XSDK::XString _profile;
    XSDK::XString _qmin;
    XSDK::XString _qmax;
    XSDK::XString _max_qdiff;

    uint8_t* _decodeBuffer;
};

}

#endif
