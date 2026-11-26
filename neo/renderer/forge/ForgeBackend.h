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

#ifndef __FORGEBACKEND_H__
#define __FORGEBACKEND_H__

/*
 * Glue between dhewm3 and The Forge renderer.
 *
 * This header deliberately doesn't include any The Forge or dhewm3 headers,
 * so it can be used from dhewm3 code (which has its own macros for assert,
 * strcmp etc.) without the two colliding. ForgeBackend.cpp is the only
 * dhewm3 file that includes The Forge headers.
 */

struct SDL_Window;

// Initializes The Forge (memory, logging, file system), creates a Vulkan
// renderer, graphics queue and a swapchain for the given SDL window.
// The window must not have been created with SDL_WINDOW_OPENGL.
// width/height are the window's drawable size in pixels.
// dataDir is an existing, writable directory for The Forge's config files,
// logs and caches.
bool ForgeBackend_Init( SDL_Window* window, int width, int height, bool vsync, const char* dataDir );

// Waits for the GPU and destroys everything created by ForgeBackend_Init().
void ForgeBackend_Shutdown();

// Renders and presents one frame. For now that just means clearing the
// back buffer. width/height are the window's current drawable size in
// pixels; the swapchain is recreated if they changed.
void ForgeBackend_EndFrame( int width, int height );

// Switches vsync on or off (recreates the swapchain if needed).
void ForgeBackend_SetVSync( bool vsync );

bool ForgeBackend_IsInitialized();

// Implemented in sys/glimp.cpp: forwards messages to the dhewm3 console.
void ForgeBackend_Printf( const char* fmt, ... );
void ForgeBackend_Warning( const char* fmt, ... );

#endif
