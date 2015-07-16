
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Exporty
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "TranscodeExport.h"
#include "FrameStoreClient/ResultParser.h"
#include "FrameStoreClient/Video.h"
#include "XSDK/XPath.h"
#include "AVKit/AVDeMuxer.h"
#include "AVKit/H264Transcoder.h"
#include "AVKit/ARGB24ToYUV420P.h"
#include "AVKit/YUV420PToARGB24.h"
#include "AVKit/Options.h"
#include "AVKit/Utils.h"
#include <vector>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>

using namespace EXPORTY;
using namespace XSDK;
using namespace AVKit;
using namespace FRAME_STORE_CLIENT;
using namespace std;

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
               int timeBaseDen ) :
        _msg( msg ),
        _decodedMsg(),
        _withTime( withTime ),
        _clockTime( clockTime ),
        _hAlign( hAlign ),
        _vAlign( vAlign ),
        _width( width ),
        _height( height ),
        _timeBaseNum( timeBaseNum),
        _timeBaseDen( timeBaseDen ),
        _timePerFrame( ((double)timeBaseNum / timeBaseDen) )
    {
        if( !_msg.empty() )
        {
            XIRef<XSDK::XMemory> decodedBuf = _msg.FromBase64();
            _decodedMsg = XString( (const char*)decodedBuf->Map(), decodedBuf->GetDataSize() );
        }
    }

    virtual ~ExportOverlay() throw()
    {
    }

    XIRef<Packet> Process( XIRef<Packet> input )
    {
        _clockTime += XDuration( MSECS, (int)(_timePerFrame * 1000) );

        cairo_surface_t* surface = NULL;
        cairo_t* cr = NULL;

        try
        {
            surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, _width, _height );
            cr = cairo_create( surface );

            uint8_t* cairoSrc = cairo_image_surface_get_data( surface );
            int cairoSrcWidth = cairo_image_surface_get_width( surface );
            int cairoSrcHeight = cairo_image_surface_get_height( surface );
            if( cairo_image_surface_get_stride( surface ) != (cairoSrcWidth * 4) )
                X_THROW(("Unexpected cairo stride!"));

            cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 1.0 );
            cairo_rectangle( cr, 0.0, 0.0, cairoSrcWidth, cairoSrcHeight );
            cairo_fill( cr );

            memcpy( cairoSrc, input->Map(), input->GetDataSize() );

            PangoLayout* layout = pango_cairo_create_layout( cr );

            pango_layout_set_text( layout, _decodedMsg.c_str(), -1 );
            PangoFontDescription* desc = pango_font_description_from_string( "Helvetica 11.0" );
            pango_layout_set_font_description( layout, desc );
            pango_font_description_free( desc );

            PangoRectangle logicalRect;
            pango_layout_get_pixel_extents( layout, NULL, &logicalRect );

            uint16_t y = (_vAlign==V_ALIGN_TOP) ? 10 : _height - 22;

            uint16_t timeX = 0;
            uint16_t msgX = 0;
            _GetXPositions( timeX, msgX, logicalRect.width );

            cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 1.0 );

            if( !_decodedMsg.empty() )
                _DrawMessage( cr, layout, msgX, y );

            if( _withTime )
                _DrawTime( cr, timeX, y );

            g_object_unref( layout );

            size_t outputSize = (cairoSrcWidth * 4) * cairoSrcHeight;
            XIRef<Packet> dest = new Packet( outputSize );
            memcpy( dest->Map(), cairoSrc, outputSize );
            dest->SetDataSize( outputSize );

            cairo_destroy( cr );
            cairo_surface_destroy( surface );

            return dest;
        }
        catch(...)
        {
            if( cr )
                cairo_destroy( cr );
            if( surface )
                cairo_surface_destroy( surface );

            throw;
        }
    }

private:
    void _GetXPositions( uint16_t& timeX, uint16_t& msgX, uint16_t messageWidth )
    {
        if( _hAlign == H_ALIGN_LEFT )
        {
            if( _withTime )
                timeX = 10;
            if( !_msg.empty() )
            {
                if( _withTime )
                    msgX = 180;
                else msgX = 10;
            }
        }
        else // _hAlign == H_ALIGN_RIGHT
        {
            if( _withTime )
                timeX = _width - 170;
            if( !_msg.empty() )
            {
                if( _withTime )
                    msgX = _width - 170 - 10 - messageWidth;
                else msgX = _width - 10 - messageWidth;
            }
        }
    }

    void _DrawMessage( cairo_t* cr, PangoLayout* layout, uint16_t msgX, uint16_t y )
    {
        cairo_move_to( cr, (double)msgX, (double)y );
        pango_cairo_show_layout( cr, layout );
    }

    void _DrawTime( cairo_t* cr, uint16_t timeX, uint16_t y )
    {
        int64_t milliTime = _clockTime.ToUnixTimeAsMSecs();

        time_t ts = (time_t)(milliTime / 1000);
        struct tm* bdt = localtime( &ts );

        char timeMessage[1024];
        memset( timeMessage, 0, 1024 );
        strftime( timeMessage, 1024, "%m/%d/%G %I:%M:%S %p", bdt );

        PangoLayout* layout = pango_cairo_create_layout( cr );
        pango_layout_set_text( layout, timeMessage, -1 );
        PangoFontDescription* desc = pango_font_description_from_string( "Helvetica 11.0" );
        pango_layout_set_font_description( layout, desc );
        pango_font_description_free( desc );

        cairo_move_to( cr, timeX, y );
        pango_cairo_show_layout( cr, layout );

        g_object_unref( layout );
    }

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

TranscodeExport::TranscodeExport( XRef<Config> config,
                                  const XString& dataSourceID,
                                  const XString& startTime,
                                  const XString& endTime,
                                  uint16_t width,
                                  uint16_t height,
                                  uint32_t bitRate,
                                  double frameRate,
                                  const XString& fileName,
                                  OverlayHAlign hAlign,
                                  OverlayVAlign vAlign,
                                  const XString& msg,
                                  bool withTime,
                                  double speed ) :
    _config( config ),
    _dataSourceID( dataSourceID ),
    _startTime( startTime ),
    _endTime( endTime ),
    _requestedWidth( width ),
    _requestedHeight( height ),
    _bitRate( bitRate ),
    _frameRate( frameRate ),
    _fileName( fileName ),
    _hAlign( hAlign ),
    _vAlign( vAlign ),
    _msg( msg ),
    _withTime( withTime ),
    _speed( speed ),
    _recorderURLS( dataSourceID, startTime, endTime, speed )
{
}

TranscodeExport::~TranscodeExport() throw()
{
}

void TranscodeExport::Create( XIRef<XMemory> output )
{
    XString tempFileName = _GetTMPName( _fileName );
    bool outputToFile = (output.IsEmpty()) ? true : false;
    H264Decoder decoder( GetFastH264DecoderOptions() );
    XRef<YUV420PToARGB24> yuvToARGB;
    XRef<ARGB24ToYUV420P> argbToYUV;
    XRef<H264Transcoder> transcoder;
    XRef<H264Encoder> encoder;
    XRef<AVMuxer> muxer;
    XRef<ExportOverlay> ov;

    XString recorderURI;
    while( _recorderURLS.GetNextURL( recorderURI ) )
    {
        XIRef<XMemory> responseBuffer = FRAME_STORE_CLIENT::FetchVideo( _config->GetRecorderIP(),
                                                                        _config->GetRecorderPort(),
                                                                        recorderURI );

        ResultParser resultParser;

        resultParser.Parse( responseBuffer );

        int outputTimeBaseNum = 0;
        int outputTimeBaseDen = 0;
        int inputTimeBaseNum = 0;
        int inputTimeBaseDen = 0;

        if( transcoder.IsEmpty() )
        {
            struct ResultStatistics stats = resultParser.GetStatistics();

            AVKit::DToQ( (1/(double)stats.frameRate), inputTimeBaseNum, inputTimeBaseDen );

            AVKit::DToQ( (1/_frameRate), outputTimeBaseNum, outputTimeBaseDen );

            transcoder = new H264Transcoder( inputTimeBaseNum, inputTimeBaseDen,
                                             outputTimeBaseNum, outputTimeBaseDen,
                                             _speed,
                                             _recorderURLS.KeyFrameOnly() ); // if our input is key only, enable decode skipping...
        }

        double secondsPerFrameOfInput = (AVKit::QToD( inputTimeBaseNum, inputTimeBaseDen ) * _speed);
        int traversalNum = 0;
        int traversalDen = 0;

        AVKit::DToQ( secondsPerFrameOfInput, traversalNum, traversalDen );

        while( !resultParser.EndOfFile() )
        {
            if( transcoder->Decode( resultParser, decoder ) )
            {
                if( encoder.IsEmpty() )
                    _CreateEncoder( encoder, muxer, decoder, tempFileName, outputToFile );

                if( yuvToARGB.IsEmpty() )
                    yuvToARGB = new YUV420PToARGB24;

                if( argbToYUV.IsEmpty() )
                    argbToYUV = new ARGB24ToYUV420P;

                if( ov.IsEmpty() )
                    ov = new ExportOverlay( _msg,
                                            _withTime,
                                            XTime::FromISOExtString( _startTime ),
                                            _hAlign,
                                            _vAlign,
                                            decoder.GetOutputWidth(),
                                            decoder.GetOutputHeight(),
                                            traversalNum,
                                            traversalDen );

                yuvToARGB->Transform( decoder.Get(), decoder.GetOutputWidth(), decoder.GetOutputHeight() );

                XIRef<Packet> rgb = yuvToARGB->Get();

                XIRef<Packet> withOverlay = ov->Process( rgb );

                argbToYUV->Transform( withOverlay, decoder.GetOutputWidth(), decoder.GetOutputHeight() );

                transcoder->EncodeYUV420PAndMux( *encoder, *muxer, argbToYUV->Get() );
            }
        }
    }

    if( outputToFile )
    {
        muxer->FinalizeFile();
        rename( tempFileName.c_str(), _fileName.c_str() );
    }
    else muxer->FinalizeBuffer( output );
}

XString TranscodeExport::_GetTMPName( const XString& fileName ) const
{
    vector<XString> parts;
    fileName.Split( "/", parts );

    XString path;

    for( int i = 0; i < ((int)parts.size() - 1); i++ )
        path += XString::Format( "%s%s", PATH_SLASH, parts[i].c_str() );

#ifdef WIN32
    uint32_t randomVal = rand();
#else
    uint32_t randomVal = random();
#endif

    XString ext = fileName.substr( fileName.rfind( "." ) );

    return XString::Format( "%s%sexporty-tmp-%s%s",
                            path.c_str(),
                            PATH_SLASH,
                            XString::FromUInt32(randomVal).c_str(),
                            ext.c_str() );
}

void TranscodeExport::_CreateEncoder( XRef<H264Encoder>& encoder,
                                      XRef<AVMuxer>& muxer,
                                      H264Decoder& decoder,
                                      const XString& tempFileName,
                                      bool outputToFile )
{
    uint16_t width = 0;
    uint16_t height = 0;

    AspectCorrectDimensions( decoder.GetInputWidth(), decoder.GetInputHeight(),
                             _requestedWidth, _requestedHeight,
                             width, height );

    decoder.SetOutputWidth( width );
    decoder.SetOutputHeight( height );

    int timeBaseNum = 0;
    int timeBaseDen = 0;
    AVKit::DToQ( (1 / _frameRate), timeBaseNum, timeBaseDen );

    CodecOptions options = GetHLSH264EncoderOptions( _bitRate, width, height, 15, timeBaseNum, timeBaseDen );

    encoder = new H264Encoder( options, false );

    muxer = new AVMuxer( encoder->GetOptions(),
                         tempFileName,
                         (outputToFile) ? AVMuxer::OUTPUT_LOCATION_FILE : AVMuxer::OUTPUT_LOCATION_BUFFER );

    muxer->SetExtraData( encoder->GetExtraData() );
}
