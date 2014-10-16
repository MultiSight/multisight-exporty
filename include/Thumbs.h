
#ifndef __EXPORTY_Thumbs_h
#define __EXPORTY_Thumbs_h

#include "Config.h"

#include "XSDK/XRef.h"

namespace EXPORTY
{

XIRef<XSDK::XMemory> CreateJPEGThumbnail( XRef<Config> config,
                                          const XSDK::XString& dataSourceID,
                                          const XSDK::XString& time,
                                          uint16_t destWidth,
                                          uint16_t destHeight,
                                          uint32_t qmax,
                                          uint32_t qmin,
                                          uint32_t bitRate );

}

#endif
