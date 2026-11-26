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

#ifndef __NULLGL_H__
#define __NULLGL_H__

#include "renderer/qgl.h"

// Returns a do-nothing implementation of the given GL function, or NULL if
// it's an extension function that isn't supported. See NullGL.cpp.
GLExtension_t NullGL_GetProcAddress( const char* name );

#endif
