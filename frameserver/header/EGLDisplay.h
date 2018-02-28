/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#ifndef EGLDISPLAY_H_
#define EGLDISPLAY_H_

#include "../header/EGL/egl.h"
#include "../header/EGL/eglext.h"

// Globals
static const EGLint configAttribs[] = {
EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
EGL_BLUE_SIZE, 8,
EGL_GREEN_SIZE, 8,
EGL_RED_SIZE, 8,
EGL_DEPTH_SIZE, 8,
EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
EGL_NONE };

// General data structure holding device information
// The user creates an instance of gpuEGL
struct gpuEGL {
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGLint majorVersion;
	EGLint minorVersion;
	int width;
	int height;
};

PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT =
		(PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");
PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
		(PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress(
				"eglGetPlatformDisplayEXT");

// Modify this according to the max num. of GPUs per system.
static const int MAX_DEVICES = 8;
EGLDeviceEXT devices[MAX_DEVICES];

// Lists the number of GPUs available in the system.
int getEGLNumDisplays() {
	EGLint numDevices;

	if (eglQueryDevicesEXT(MAX_DEVICES, devices, &numDevices)) {
		return numDevices;
	}

	std::cerr << "Error while querying devices \n";
	return 0;
}

// Creates an OpenGL context with no visible window.
gpuEGL createEGLWindowless(int numGPU, int width, int height, int winX,
		int winY) {

	EGLint major, minor;
	EGLint numConfigs;
	EGLConfig eglCfg;
	gpuEGL gpu;

	EGLDisplay eglDpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
			devices[numGPU], 0);
	eglInitialize(eglDpy, &major, &minor);

	std::cout << "Using EGL " << major << "." << minor << std::endl;

	// Select an appropriate configuration
	eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

	// Create a surface

	static const EGLint pbufferAttribs[] = {
	EGL_WIDTH, width,
	EGL_HEIGHT, height,
	EGL_NONE, };
	EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg,
			pbufferAttribs);

	// Bind the API
	eglBindAPI(EGL_OPENGL_API);

	// Create a context and make it current
	EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);

	std::cout << "Using GPU " << numGPU << std::endl;

	gpu.display = eglDpy;
	gpu.surface = eglSurf;
	gpu.context = eglCtx;
	gpu.width = width;
	gpu.height = height;
	gpu.majorVersion = major;
	gpu.minorVersion = minor;

	return gpu;
}

// Set the gpu's OpenGL context given by *gpu
void makeEGLContextCurrent(gpuEGL *gpu) {
	eglMakeCurrent(gpu->display, gpu->surface, gpu->surface, gpu->context);

}

// Closes the gpu's OpenGL Context given by *gpu
void closeEGL(gpuEGL *gpu) {
	eglTerminate(gpu->display);

}

#endif /* EGLDISPLAY_H_ */
