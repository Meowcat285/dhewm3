/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

===========================================================================
*/

// NOTE: this file must not include dhewm3 headers (sys/platform.h, idlib, ...),
//       their macros collide with The Forge headers. Talk to the rest of the
//       engine through ForgeBackend.h only.

#include "renderer/forge/ForgeBackend.h"

// The Forge headers first: on Windows they include windows.h with NOMINMAX
// and WIN32_LEAN_AND_MEAN, SDL_syswm.h would include it without those.
#include "Graphics/Interfaces/IGraphics.h"
#include "Graphics/GraphicsConfig.h"
#include "Utilities/Interfaces/IFileSystem.h"
#include "Utilities/Interfaces/ILog.h"
#include "Utilities/RingBuffer.h"
#include "Utilities/Interfaces/IMemory.h" // must be the last The Forge include

#ifdef D3_SDL3
  #include <SDL3/SDL.h>
#else
  #include <SDL.h>
  #include <SDL_syswm.h>
#endif

#include <stdio.h>
#include <string.h>

namespace {

// number of frames the CPU may be ahead of the GPU
const uint32_t FRAMES_IN_FLIGHT = 2;

struct ForgeState {
	bool          initialized;
	bool          memInitialized;
	bool          fsInitialized;
	bool          logInitialized;
	bool          gpuConfigInitialized;

	WindowHandle  windowHandle;
	Renderer*     renderer;
	Queue*        graphicsQueue;
	GpuCmdRing    cmdRing;
	Semaphore*    imageAcquiredSemaphore;
	SwapChain*    swapChain;
	bool          vsync;
	uint32_t      frameIndex;
};

ForgeState fs;

// Fills a The Forge WindowHandle from the native handles behind an SDL window.
bool GetNativeWindowHandle( SDL_Window* window, WindowHandle* out ) {
	memset( out, 0, sizeof( *out ) );

#if defined( D3_SDL3 )
	SDL_PropertiesID props = SDL_GetWindowProperties( window );
  #if defined( _WIN32 )
	out->type = WINDOW_HANDLE_TYPE_WIN32;
	out->window = (HWND)SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL );
	return out->window != NULL;
  #elif defined( __linux__ )
	const char* driver = SDL_GetCurrentVideoDriver();
	if ( driver != NULL && strcmp( driver, "x11" ) == 0 ) {
		out->type = WINDOW_HANDLE_TYPE_XLIB;
		out->display = (Display*)SDL_GetPointerProperty( props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL );
		out->window = (Window)SDL_GetNumberProperty( props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0 );
		return out->display != NULL && out->window != 0;
	}
	if ( driver != NULL && strcmp( driver, "wayland" ) == 0 ) {
		out->type = WINDOW_HANDLE_TYPE_WAYLAND;
		out->wl_display = (struct wl_display*)SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL );
		out->wl_surface = (struct wl_surface*)SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL );
		return out->wl_display != NULL && out->wl_surface != NULL;
	}
	ForgeBackend_Warning( "The Forge: unsupported SDL video driver '%s'\n", driver ? driver : "(null)" );
	return false;
  #else
	return false;
  #endif

#else // SDL2
	SDL_SysWMinfo info;
	SDL_VERSION( &info.version );
	if ( !SDL_GetWindowWMInfo( window, &info ) ) {
		ForgeBackend_Warning( "The Forge: SDL_GetWindowWMInfo() failed: %s\n", SDL_GetError() );
		return false;
	}
	switch ( info.subsystem ) {
  #if defined( _WIN32 )
		case SDL_SYSWM_WINDOWS:
			out->type = WINDOW_HANDLE_TYPE_WIN32;
			out->window = info.info.win.window;
			return true;
  #endif
  #if defined( __linux__ ) && defined( SDL_VIDEO_DRIVER_X11 )
		case SDL_SYSWM_X11:
			out->type = WINDOW_HANDLE_TYPE_XLIB;
			out->display = info.info.x11.display;
			out->window = info.info.x11.window;
			return true;
  #endif
  #if defined( __linux__ ) && defined( SDL_VIDEO_DRIVER_WAYLAND )
		case SDL_SYSWM_WAYLAND:
			out->type = WINDOW_HANDLE_TYPE_WAYLAND;
			out->wl_display = info.info.wl.display;
			out->wl_surface = info.info.wl.surface;
			return true;
  #endif
		default:
			break;
	}
	ForgeBackend_Warning( "The Forge: unsupported SDL window subsystem %d\n", (int)info.subsystem );
	return false;
#endif
}

// The Forge's GPU selection and per-GPU workaround rules, normally shipped as
// a gpu.cfg file next to the executable. Based on The Forge's unit test
// config; written to the Forge data dir at startup so nothing extra has to
// be packaged.
const char* const gpuCfg =
	"#version: 0.3\n"
	"\n"
	"BEGIN_GPU_SELECTION;\n"
	"GraphicQueueSupported == 1;\n"
	"isHeadLess != 1;\n"
	"deviceid == PreferredGPU;\n"
	"GpuPresetLevel;\n"
	"# prefer other GPUs over Intel (usually integrated) ones\n"
	"VendorID != 0x8086, VendorID != 0x8087, VendorID != 0x163C;\n"
	"DirectXFeatureLevel;\n"
	"VRAM;\n"
	"END_GPU_SELECTION;\n"
	"\n"
	"BEGIN_DRIVER_REJECTION;\n"
	"END_DRIVER_REJECTION;\n"
	"\n"
	"BEGIN_GPU_SETTINGS;\n"
	"# driver bug on intel\n"
	"dynamicRenderingEnabled; vendorID == 0x163C; 0;\n"
	"dynamicRenderingEnabled; vendorID == 0x8086; 0;\n"
	"dynamicRenderingEnabled; vendorID == 0x8087; 0;\n"
	"END_GPU_SETTINGS;\n"
	"\n"
	"BEGIN_USER_SETTINGS;\n"
	"END_USER_SETTINGS;\n";

void WriteGpuCfg( const char* dataDir ) {
	char path[1024];
	snprintf( path, sizeof( path ), "%s/gpu.cfg", dataDir );
	FILE* f = fopen( path, "wb" );
	if ( f == NULL ) {
		ForgeBackend_Warning( "The Forge: couldn't write %s, GPU selection rules won't be used\n", path );
		return;
	}
	fwrite( gpuCfg, 1, strlen( gpuCfg ), f );
	fclose( f );
}

// forward The Forge warnings and errors to the dhewm3 console
void ForgeLogCallback( void* /*userData*/, const char* message ) {
	ForgeBackend_Printf( "%s", message );
}

bool AddSwapChain( uint32_t width, uint32_t height ) {
	SwapChainDesc desc = {};
	desc.mWindowHandle = fs.windowHandle;
	desc.mPresentQueueCount = 1;
	desc.ppPresentQueues = &fs.graphicsQueue;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mImageCount = getRecommendedSwapchainImageCount( fs.renderer, &fs.windowHandle );
	desc.mColorFormat = getSupportedSwapchainFormat( fs.renderer, &desc, COLOR_SPACE_SDR_SRGB );
	desc.mColorSpace = COLOR_SPACE_SDR_SRGB;
	desc.mEnableVsync = fs.vsync;
	// Placeholder until the real renderer draws something: a dark blue-grey,
	// so a working swapchain is easy to tell apart from a black/broken window.
	desc.mColorClearValue.r = 0.10f;
	desc.mColorClearValue.g = 0.12f;
	desc.mColorClearValue.b = 0.18f;
	desc.mColorClearValue.a = 1.0f;

	addSwapChain( fs.renderer, &desc, &fs.swapChain );
	if ( fs.swapChain == NULL ) {
		ForgeBackend_Warning( "The Forge: couldn't create a %ux%u swapchain\n", width, height );
		return false;
	}
	ForgeBackend_Printf( "The Forge: created %ux%u swapchain with %u images, vsync %s\n",
	                     width, height, desc.mImageCount, fs.vsync ? "on" : "off" );
	return true;
}

void RemoveSwapChain() {
	if ( fs.swapChain != NULL ) {
		waitQueueIdle( fs.graphicsQueue );
		removeSwapChain( fs.renderer, fs.swapChain );
		fs.swapChain = NULL;
	}
}

} // anonymous namespace

#if defined( _WINDOWS )
// The Forge's Vulkan backend calls this on Windows when the device was lost.
// It's normally implemented by The Forge's app framework, which we don't use.
void requestReset( const ResetDesc* /*pResetDesc*/ ) {
	ForgeBackend_Warning( "The Forge: GPU device lost!\n" );
}
#endif

bool ForgeBackend_IsInitialized() {
	return fs.initialized;
}

bool ForgeBackend_Init( SDL_Window* window, int width, int height, bool vsync, const char* dataDir ) {
	if ( fs.initialized ) {
		ForgeBackend_Warning( "ForgeBackend_Init() called twice!\n" );
		return true;
	}

	memset( &fs, 0, sizeof( fs ) );
	fs.vsync = vsync;

	if ( !GetNativeWindowHandle( window, &fs.windowHandle ) ) {
		ForgeBackend_Warning( "The Forge: couldn't get the native window handle\n" );
		return false;
	}

	if ( !initMemAlloc( "dhewm3" ) ) {
		ForgeBackend_Warning( "The Forge: initMemAlloc() failed\n" );
		return false;
	}
	fs.memInitialized = true;

	// "tool" mode: don't look for The Forge's PathStatement.txt, set the
	// resource directories The Forge uses so far manually instead.
	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = "dhewm3";
	fsDesc.mIsTool = true;
	if ( !initFileSystem( &fsDesc ) ) {
		ForgeBackend_Warning( "The Forge: initFileSystem() failed\n" );
		ForgeBackend_Shutdown();
		return false;
	}
	fs.fsInitialized = true;

	// all point to a writable dir in dhewm3's save path for now
	// (gpu.cfg, optional gpu.data, logs, pipeline caches).
	// The Forge appends file names to these paths as they are, so they need a trailing slash.
	char dataDirSlash[1024];
	snprintf( dataDirSlash, sizeof( dataDirSlash ), "%s/", dataDir );
	// (The Forge keeps them over exitFileSystem(), so only set them once,
	//  otherwise it complains on every vid_restart)
	static char dataDirSet[1024] = "";
	if ( strcmp( dataDirSet, dataDirSlash ) != 0 ) {
		const ResourceDirectory dataDirs[] = { RD_GPU_CONFIG, RD_OTHER_FILES, RD_LOG, RD_PIPELINE_CACHE, RD_DEBUG };
		for ( size_t i = 0; i < sizeof( dataDirs ) / sizeof( dataDirs[0] ); i++ ) {
			fsSetPathForResourceDir( pSystemFileIO, dataDirs[i], dataDirSlash );
		}
		strcpy( dataDirSet, dataDirSlash );
	}
	WriteGpuCfg( dataDir );

	// NULL app name: don't create a separate log file, everything goes to
	// the dhewm3 console through the callback below
	initLog( NULL, (LogLevel)( eINFO | eWARNING | eERROR ) );
	addLogCallback( "dhewm3", eINFO | eWARNING | eERROR, NULL, ForgeLogCallback, NULL, NULL );
	fs.logInitialized = true;

	RendererDesc rendererDesc;
	memset( &rendererDesc, 0, sizeof( rendererDesc ) );
	initGPUConfiguration( rendererDesc.pExtendedSettings );
	fs.gpuConfigInitialized = true;

	// The Forge crashes in initRenderer() if there is no usable Vulkan GPU
	// (it ignores software implementations like lavapipe), so check first.
	{
		RendererContextDesc contextDesc;
		memset( &contextDesc, 0, sizeof( contextDesc ) );
		RendererContext* context = NULL;
		initRendererContext( "dhewm3", &contextDesc, &context );
		uint32_t gpuCount = context ? context->mGpuCount : 0;
		if ( context ) {
			exitRendererContext( context );
		}
		if ( gpuCount == 0 ) {
			ForgeBackend_Warning( "The Forge: no usable Vulkan GPU found. Make sure your graphics driver supports Vulkan.\n" );
			ForgeBackend_Shutdown();
			return false;
		}
	}

	initRenderer( "dhewm3", &rendererDesc, &fs.renderer );
	if ( fs.renderer == NULL ) {
		ForgeBackend_Warning( "The Forge: couldn't create a Vulkan renderer. Is a Vulkan driver installed?\n" );
		ForgeBackend_Shutdown();
		return false;
	}
	setupGPUConfigurationPlatformParameters( fs.renderer, rendererDesc.pExtendedSettings );

	if ( fs.renderer->pGpu != NULL ) {
		ForgeBackend_Printf( "The Forge: using GPU \"%s\" (vendor 0x%x)\n",
		                     fs.renderer->pGpu->mGpuVendorPreset.mGpuName,
		                     fs.renderer->pGpu->mGpuVendorPreset.mVendorId );
	}

	QueueDesc queueDesc = {};
	queueDesc.mType = QUEUE_TYPE_GRAPHICS;
	initQueue( fs.renderer, &queueDesc, &fs.graphicsQueue );

	GpuCmdRingDesc cmdRingDesc = {};
	cmdRingDesc.pQueue = fs.graphicsQueue;
	cmdRingDesc.mPoolCount = FRAMES_IN_FLIGHT;
	cmdRingDesc.mCmdPerPoolCount = 1;
	cmdRingDesc.mAddSyncPrimitives = true;
	initGpuCmdRing( fs.renderer, &cmdRingDesc, &fs.cmdRing );

	initSemaphore( fs.renderer, &fs.imageAcquiredSemaphore );

	fs.initialized = true;

	if ( !AddSwapChain( (uint32_t)width, (uint32_t)height ) ) {
		ForgeBackend_Shutdown();
		return false;
	}

	return true;
}

void ForgeBackend_Shutdown() {
	if ( fs.initialized ) {
		waitQueueIdle( fs.graphicsQueue );
		RemoveSwapChain();
		exitSemaphore( fs.renderer, fs.imageAcquiredSemaphore );
		exitGpuCmdRing( fs.renderer, &fs.cmdRing );
		exitQueue( fs.renderer, fs.graphicsQueue );
	}
	if ( fs.renderer != NULL ) {
		exitRenderer( fs.renderer );
	}
	if ( fs.gpuConfigInitialized ) {
		exitGPUConfiguration();
	}
	if ( fs.logInitialized ) {
		exitLog();
	}
	if ( fs.fsInitialized ) {
		exitFileSystem();
	}
	if ( fs.memInitialized ) {
		exitMemAlloc();
	}
	memset( &fs, 0, sizeof( fs ) );
}

void ForgeBackend_SetVSync( bool vsync ) {
	if ( !fs.initialized ) {
		return;
	}
	if ( fs.swapChain != NULL && (bool)fs.swapChain->mEnableVsync != vsync ) {
		waitQueueIdle( fs.graphicsQueue );
		toggleVSync( fs.renderer, &fs.swapChain );
	}
	fs.vsync = vsync;
}

void ForgeBackend_EndFrame( int width, int height ) {
	if ( !fs.initialized || width <= 0 || height <= 0 ) {
		return; // e.g. minimized window
	}

	// recreate the swapchain if the window size changed
	if ( fs.swapChain == NULL
		|| fs.swapChain->ppRenderTargets[0]->mWidth != (uint32_t)width
		|| fs.swapChain->ppRenderTargets[0]->mHeight != (uint32_t)height )
	{
		RemoveSwapChain();
		if ( !AddSwapChain( (uint32_t)width, (uint32_t)height ) ) {
			return;
		}
	}

	uint32_t imageIndex = 0;
	acquireNextImage( fs.renderer, fs.swapChain, fs.imageAcquiredSemaphore, NULL, &imageIndex );
	if ( imageIndex == (uint32_t)-1 ) {
		// swapchain out of date (resized or so) => recreate it next frame
		RemoveSwapChain();
		return;
	}

	RenderTarget* backBuffer = fs.swapChain->ppRenderTargets[imageIndex];
	GpuCmdRingElement elem = getNextGpuCmdRingElement( &fs.cmdRing, true, 1 );

	// stall if the CPU is running FRAMES_IN_FLIGHT frames ahead of the GPU
	FenceStatus fenceStatus;
	getFenceStatus( fs.renderer, elem.pFence, &fenceStatus );
	if ( fenceStatus == FENCE_STATUS_INCOMPLETE ) {
		waitForFences( fs.renderer, 1, &elem.pFence );
	}

	resetCmdPool( fs.renderer, elem.pCmdPool );

	Cmd* cmd = elem.pCmds[0];
	beginCmd( cmd );

	RenderTargetBarrier barrier = {};
	barrier.pRenderTarget = backBuffer;
	barrier.mCurrentState = RESOURCE_STATE_PRESENT;
	barrier.mNewState = RESOURCE_STATE_RENDER_TARGET;
	cmdResourceBarrier( cmd, 0, NULL, 0, NULL, 1, &barrier );

	// TODO: this is where the ported draw passes will go. For now just
	//       clear the back buffer to the swapchain's clear color.
	BindRenderTargetsDesc bindDesc = {};
	bindDesc.mRenderTargetCount = 1;
	bindDesc.mRenderTargets[0].pRenderTarget = backBuffer;
	bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
	cmdBindRenderTargets( cmd, &bindDesc );
	cmdBindRenderTargets( cmd, NULL );

	barrier.mCurrentState = RESOURCE_STATE_RENDER_TARGET;
	barrier.mNewState = RESOURCE_STATE_PRESENT;
	cmdResourceBarrier( cmd, 0, NULL, 0, NULL, 1, &barrier );

	endCmd( cmd );

	QueueSubmitDesc submitDesc = {};
	submitDesc.mCmdCount = 1;
	submitDesc.ppCmds = &cmd;
	submitDesc.mWaitSemaphoreCount = 1;
	submitDesc.ppWaitSemaphores = &fs.imageAcquiredSemaphore;
	submitDesc.mSignalSemaphoreCount = 1;
	submitDesc.ppSignalSemaphores = &elem.pSemaphore;
	submitDesc.pSignalFence = elem.pFence;
	queueSubmit( fs.graphicsQueue, &submitDesc );

	QueuePresentDesc presentDesc = {};
	presentDesc.pSwapChain = fs.swapChain;
	presentDesc.mIndex = (uint8_t)imageIndex;
	presentDesc.mWaitSemaphoreCount = 1;
	presentDesc.ppWaitSemaphores = &elem.pSemaphore;
	presentDesc.mSubmitDone = true;
	queuePresent( fs.graphicsQueue, &presentDesc );

	fs.frameIndex = ( fs.frameIndex + 1 ) % FRAMES_IN_FLIGHT;
}
