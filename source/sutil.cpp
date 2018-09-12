/* 
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "../header/sutil.h"
//#include <sutil/HDRLoader.h>
//#include <sutil/PPMLoader.h>
//#include <sampleConfig.h>

#include <optixu/optixu_math_namespace.h>

#include <cstring>
#include <iostream>
#include <fstream>
#include <stdint.h>

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN 1
#    endif
#    include<windows.h>
#    include<mmsystem.h>
#else // Apple and Linux both use this 
#    include<sys/time.h>
#    include <unistd.h>
#    include <dirent.h>
#endif

#include <stdlib.h>

using namespace optix;



// Global variables for GLUT display functions
RTcontext g_context           = 0;
RTbuffer  g_image_buffer      = 0;
bool      g_glut_initialized  = false;


void keyPressed(unsigned char key, int x, int y)
{
    switch (key)
    {
        case 27: // esc
        case 'q':
            rtContextDestroy( g_context );
            exit(EXIT_SUCCESS);
    }
}



void checkBuffer( RTbuffer buffer )
{
    // Check to see if the buffer is two dimensional
    unsigned int dimensionality;
    RT_CHECK_ERROR( rtBufferGetDimensionality(buffer, &dimensionality) );
    if (2 != dimensionality)
        throw Exception( "Attempting to display non-2D buffer" );

    // Check to see if the buffer is of type float{1,3,4} or uchar4
    RTformat format;
    RT_CHECK_ERROR( rtBufferGetFormat(buffer, &format) );
    if( RT_FORMAT_FLOAT  != format && 
            RT_FORMAT_FLOAT4 != format &&
            RT_FORMAT_FLOAT3 != format &&
            RT_FORMAT_UNSIGNED_BYTE4 != format )
        throw Exception( "Attempting to diaplay buffer with format not float, float3, float4, or uchar4");
}


void SavePPM(const unsigned char *Pix, const char *fname, unsigned int wid, unsigned int hgt, int chan)
{
    if( Pix==NULL || wid < 1 || hgt < 1 )
        throw Exception( "Image is ill-formed. Not saving" );

    if( chan != 1 && chan != 3 && chan != 4 )
        throw Exception( "Attempting to save image with channel count != 1, 3, or 4.");

    std::ofstream OutFile(fname, std::ios::out | std::ios::binary);
    if(!OutFile.is_open())
        throw Exception( "Could not open file for SavePPM" );

    bool is_float = false;
    OutFile << 'P';
    OutFile << ((chan==1 ? (is_float?'Z':'5') : (chan==3 ? (is_float?'7':'6') : '8'))) << std::endl;
    OutFile << wid << " " << hgt << std::endl << 255 << std::endl;

    OutFile.write(reinterpret_cast<char*>(const_cast<unsigned char*>( Pix )), wid * hgt * chan * (is_float ? 4 : 1));

    OutFile.close();
}


bool dirExists( const char* path )
{
#if defined(_WIN32)
    DWORD attrib = GetFileAttributes( path );
    return (attrib != INVALID_FILE_ATTRIBUTES) && (attrib & FILE_ATTRIBUTE_DIRECTORY);
#else
    DIR* dir = opendir( path );
    if( dir == NULL )
        return false;
    
    closedir(dir);
    return true;
#endif
}




void sutil::reportErrorMessage( const char* message )
{
    std::cerr << "OptiX Error: '" << message << "'\n";
#if defined(_WIN32) && defined(RELEASE_PUBLIC)
    {
        char s[2048];
        sprintf( s, "OptiX Error: %s", message );
        MessageBox( 0, s, "OptiX Error", MB_OK|MB_ICONWARNING|MB_SYSTEMMODAL );
    }
#endif
}


void sutil::handleError( RTcontext context, RTresult code, const char* file,
        int line)
{
    const char* message;
    char s[2048];
    rtContextGetErrorString(context, code, &message);
    sprintf(s, "%s\n(%s:%d)", message, file, line);
    reportErrorMessage( s );
}




void sutil::displayBufferPPM( const char* filename, Buffer buffer)
{
    displayBufferPPM( filename, buffer->get() );
}


void sutil::displayBufferPPM( const char* filename, RTbuffer buffer)
{
    unsigned int width, height;
    RTsize buffer_width, buffer_height;

    void* imageData;
    RT_CHECK_ERROR( rtBufferMap( buffer, &imageData) );

    RT_CHECK_ERROR( rtBufferGetSize2D(buffer, &buffer_width, &buffer_height) );
    width  = static_cast<unsigned int>(buffer_width);
    height = static_cast<unsigned int>(buffer_height);

    std::vector<unsigned char> pix(width * height * 3);

    RTformat buffer_format;
    RT_CHECK_ERROR( rtBufferGetFormat(buffer, &buffer_format) );

    switch(buffer_format) {
        case RT_FORMAT_UNSIGNED_BYTE4:
            // Data is BGRA and upside down, so we need to swizzle to RGB
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                unsigned char *src = ((unsigned char*)imageData) + (4*width*j);
                for(int i = 0; i < width; i++) {
                    *dst++ = *(src + 2);
                    *dst++ = *(src + 1);
                    *dst++ = *(src + 0);
                    src += 4;
                }
            }
            break;

        case RT_FORMAT_FLOAT:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + width*(height-1-j);
                float* src = ((float*)imageData) + (3*width*j);
                for(int i = 0; i < width; i++) {
                    int P = static_cast<int>((*src++) * 255.0f);
                    unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;

                    // write the pixel to all 3 channels
                    *dst++ = static_cast<unsigned char>(Clamped);
                    *dst++ = static_cast<unsigned char>(Clamped);
                    *dst++ = static_cast<unsigned char>(Clamped);
                }
            }
            break;

        case RT_FORMAT_FLOAT3:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                float* src = ((float*)imageData) + (3*width*j);
                for(int i = 0; i < width; i++) {
                    for(int elem = 0; elem < 3; ++elem) {
                        int P = static_cast<int>((*src++) * 255.0f);
                        unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;
                        *dst++ = static_cast<unsigned char>(Clamped);
                    }
                }
            }
            break;

        case RT_FORMAT_FLOAT4:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                float* src = ((float*)imageData) + (4*width*j);
                for(int i = 0; i < width; i++) {
                    for(int elem = 0; elem < 3; ++elem) {
                        int P = static_cast<int>((*src++) * 255.0f);
                        unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;
                        *dst++ = static_cast<unsigned char>(Clamped);
                    }

                    // skip alpha
                    src++;
                }
            }
            break;

        default:
            fprintf(stderr, "Unrecognized buffer data type or format.\n");
            exit(2);
            break;
    }

    SavePPM(&pix[0], filename, width, height, 3);

    // Now unmap the buffer
    RT_CHECK_ERROR( rtBufferUnmap(buffer) );

}

void sutil::displayStreamBuffer( unsigned char *pix, RTbuffer buffer, unsigned int width, unsigned int height)
{
	    void* imageData;
	    RT_CHECK_ERROR( rtBufferMap( buffer, &imageData) );

	    // In Optix 4.0.x a stream buffer format must be RT_FORMAT_UNSIGNED_BYTE4:
	    // Data is BGRA and upside down, so we need to swizzle to RGB
	    for(int j = height-1; j >= 0; --j) {
	    	unsigned char *dst = pix + (3*width*(height-1-j));
	        unsigned char *src = ((unsigned char*)imageData) + (4*width*j);
	        for(int i = 0; i < width; i++)
	        {
	        	*dst++ = *(src + 2);
	            *dst++ = *(src + 1);
	            *dst++ = *(src + 0);
	            src += 4;
	        }
	    }
	    // Now unmap the buffer
	    RT_CHECK_ERROR( rtBufferUnmap(buffer) );
}

void sutil::displayBuffer( unsigned char *pix, RTbuffer buffer)
{
    unsigned int width, height;
    RTsize buffer_width, buffer_height;

    void* imageData;
    RT_CHECK_ERROR( rtBufferMap( buffer, &imageData) );

    RT_CHECK_ERROR( rtBufferGetSize2D(buffer, &buffer_width, &buffer_height) );
    width  = static_cast<unsigned int>(buffer_width);
    height = static_cast<unsigned int>(buffer_height);

    //std::vector<unsigned char> pix(width * height * 3);

    RTformat buffer_format;
    RT_CHECK_ERROR( rtBufferGetFormat(buffer, &buffer_format) );

    switch(buffer_format) {
        case RT_FORMAT_UNSIGNED_BYTE4:
            // Data is BGRA and upside down, so we need to swizzle to RGB
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = pix + (3*width*(height-1-j));
                unsigned char *src = ((unsigned char*)imageData) + (4*width*j);
                for(int i = 0; i < width; i++) {
                    *dst++ = *(src + 2);
                    *dst++ = *(src + 1);
                    *dst++ = *(src + 0);
                    src += 4;
                }
            }
            break;

        case RT_FORMAT_FLOAT:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + width*(height-1-j);
                float* src = ((float*)imageData) + (3*width*j);
                for(int i = 0; i < width; i++) {
                    int P = static_cast<int>((*src++) * 255.0f);
                    unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;

                    // write the pixel to all 3 channels
                    *dst++ = static_cast<unsigned char>(Clamped);
                    *dst++ = static_cast<unsigned char>(Clamped);
                    *dst++ = static_cast<unsigned char>(Clamped);
                }
            }
            break;

        case RT_FORMAT_FLOAT3:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                float* src = ((float*)imageData) + (3*width*j);
                for(int i = 0; i < width; i++) {
                    for(int elem = 0; elem < 3; ++elem) {
                        int P = static_cast<int>((*src++) * 255.0f);
                        unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;
                        *dst++ = static_cast<unsigned char>(Clamped);
                    }
                }
            }
            break;

        case RT_FORMAT_FLOAT4:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                float* src = ((float*)imageData) + (4*width*j);
                for(int i = 0; i < width; i++) {
                    for(int elem = 0; elem < 3; ++elem) {
                        int P = static_cast<int>((*src++) * 255.0f);
                        unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;
                        *dst++ = static_cast<unsigned char>(Clamped);
                    }

                    // skip alpha
                    src++;
                }
            }
            break;

        default:
            fprintf(stderr, "Unrecognized buffer data type or format.\n");
            exit(2);
            break;
    }

     // Now unmap the buffer
    RT_CHECK_ERROR( rtBufferUnmap(buffer) );

}

namespace
{


static const float FPS_UPDATE_INTERVAL = 4.0;  //seconds

} // namespace

void sutil::displayFps( unsigned int frame_count  )
{
    static double fps = -1.0;
    static unsigned last_frame_count = 0;
    static double last_update_time = sutil::currentTime();
    static double current_time = 0.0;
    current_time = sutil::currentTime();
    if ( (current_time - last_update_time) > FPS_UPDATE_INTERVAL ) {
        fps = ( frame_count - last_frame_count ) / ( current_time - last_update_time );
        last_frame_count = frame_count;
        last_update_time = current_time;
        if (fps > 0.0 )
        {
        	printf ("Time: %4.2f ms fps: %7.2f\n", (1/fps)*1000.0f, fps);
        }
    }
}

void sutil::displayFps( unsigned int frame_count, double *fpsexpave )
{
    static double fps = -1.0;
    static unsigned last_frame_count = 0;
    static double last_update_time = sutil::currentTime();
    static double current_time = 0.0;
    current_time = sutil::currentTime();
    if ( current_time - last_update_time > FPS_UPDATE_INTERVAL )
    {
        fps = ( frame_count - last_frame_count ) / ( current_time - last_update_time );
        last_frame_count = frame_count;
        last_update_time = current_time;
        *fpsexpave = (*fpsexpave * 0.90) + (fps * 0.10);

        if ( frame_count > 0 && fps >= 0.0 ) {
        	//static char fps_text[32];
        	//sprintf( fps_text, "fps: %7.2f", fps );
        	printf ("Time: %7.2f ms fps: %7.2f\n", 1/fps, fps);
        }
    }
}

/*
optix::TextureSampler sutil::loadTexture( optix::Context context,
        const std::string& filename, optix::float3 default_color )
{
    bool isHDR = false;
    size_t len = filename.length();
    if(len >= 3) {
      isHDR = (filename[len-3] == 'H' || filename[len-3] == 'h') &&
              (filename[len-2] == 'D' || filename[len-2] == 'd') &&
              (filename[len-1] == 'R' || filename[len-1] == 'r');
    }
    if ( isHDR ) {
        return loadHDRTexture(context, filename, default_color);
    } else {
        return loadPPMTexture(context, filename, default_color);
    }
}


optix::Buffer sutil::loadCubeBuffer( optix::Context context,
        const std::vector<std::string>& filenames )
{
    return loadPPMCubeBuffer( context, filenames );
}
*/

void sutil::calculateCameraVariables( float3 eye, float3 lookat, float3 up,
        float  fov, float  aspect_ratio,
        float3& U, float3& V, float3& W, bool fov_is_vertical )
{
    float ulen, vlen, wlen;
    W = lookat - eye; // Do not normalize W -- it implies focal length

    wlen = length( W ); 
    U = normalize( cross( W, up ) );
    V = normalize( cross( U, W  ) );

	if ( fov_is_vertical ) {
		vlen = wlen * tanf( 0.5f * fov * M_PIf / 180.0f );
		V *= vlen;
		ulen = vlen * aspect_ratio;
		U *= ulen;
	}
	else {
		ulen = wlen * tanf( 0.5f * fov * M_PIf / 180.0f );
		U *= ulen;
		vlen = ulen / aspect_ratio;
		V *= vlen;
	}
}


void sutil::parseDimensions( const char* arg, int& width, int& height )
{

    // look for an 'x': <width>x<height>
    size_t width_end = strchr( arg, 'x' ) - arg;
    size_t height_begin = width_end + 1;

    if ( height_begin < strlen( arg ) )
    {
        // find the beginning of the height string/
        const char *height_arg = &arg[height_begin];

        // copy width to null-terminated string
        char width_arg[32];
        strncpy( width_arg, arg, width_end );
        width_arg[width_end] = '\0';

        // terminate the width string
        width_arg[width_end] = '\0';

        width  = atoi( width_arg );
        height = atoi( height_arg );
        return;
    }

    throw Exception(
            "Failed to parse width, heigh from string '" +
            std::string( arg ) +
            "'" );
}


double sutil::currentTime()
{
#if defined(_WIN32)

    // inv_freq is 1 over the number of ticks per second.
    static double inv_freq;
    static bool freq_initialized = 0;
    static bool use_high_res_timer = 0;

    if(!freq_initialized)
    {
        LARGE_INTEGER freq;
        use_high_res_timer = QueryPerformanceFrequency( &freq );
        inv_freq = 1.0/freq.QuadPart;
        freq_initialized = 1;
    }

    if (use_high_res_timer)
    {
        LARGE_INTEGER c_time;
        if( QueryPerformanceCounter( &c_time ) )
            return c_time.QuadPart*inv_freq;
        else
            throw Exception( "sutil::currentTime: QueryPerformanceCounter failed" );
    }

    return static_cast<double>( timeGetTime() ) * 1.0e-3;

#else

    struct timeval tv;
    if( gettimeofday( &tv, 0 ) )
        throw Exception( "sutil::urrentTime(): gettimeofday failed!\n" );

    return  tv.tv_sec+ tv.tv_usec * 1.0e-6;

#endif 
}


void sutil::sleep( int seconds )
{
#if defined(_WIN32)
    Sleep( seconds * 1000 );
#else
    ::sleep( seconds );
#endif
}

