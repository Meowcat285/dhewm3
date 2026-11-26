/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

/*
 * "Null" OpenGL implementation for the The Forge renderer port.
 *
 * With FORGE_RENDERER enabled there is no OpenGL context anymore: The Forge
 * owns the window's swapchain (Vulkan). The existing renderer still issues
 * qgl* calls though, until the draw code is ported to The Forge. To keep the
 * game running in the meantime, GLimp_ExtensionPointer() hands out the
 * functions from this file: they do nothing, and the getters report a
 * plausible GL 2.0 driver with the extensions the renderer requires.
 */

#include "sys/platform.h"
#include "renderer/tr_local.h"

#include "renderer/forge/NullGL.h"


namespace {

template<typename T> inline T NullRet() { return T(); }
template<> inline void NullRet<void>() {}

// default no-op implementation for every function in qgl_proc.h
#define QGLPROC(name, rettype, args) \
	rettype APIENTRY null_##name args { return NullRet<rettype>(); }
#include "renderer/qgl_proc.h"
#undef QGLPROC

// no-op implementation of the extension functions the renderer loads
// (all of them return void)
void APIENTRY null_ext_noop() {}

// Extensions reported to the renderer. Only the ones it requires (see
// R_CheckPortableExtensions()) plus ARB programs, so the ARB2 path is
// "available". VBOs and texture compression are deliberately left out,
// so the vertex cache keeps vertices in system memory and images are
// "uploaded" uncompressed.
const char* const nullExtensions =
	"GL_ARB_multitexture "
	"GL_ARB_texture_env_combine "
	"GL_ARB_texture_cube_map "
	"GL_ARB_texture_env_dot3 "
	"GL_ARB_texture_env_add "
	"GL_ARB_texture_non_power_of_two "
	"GL_EXT_texture_filter_anisotropic "
	"GL_EXT_stencil_wrap "
	"GL_ARB_vertex_program "
	"GL_ARB_fragment_program";

const GLubyte* APIENTRY nullGetString( GLenum name ) {
	switch ( name ) {
		case GL_VENDOR:     return (const GLubyte*)"dhewm3";
		case GL_RENDERER:   return (const GLubyte*)"The Forge (Vulkan), null GL";
		case GL_VERSION:    return (const GLubyte*)"2.0 dhewm3 null GL";
		case GL_EXTENSIONS: return (const GLubyte*)nullExtensions;
	}
	return (const GLubyte*)"";
}

void APIENTRY nullGetIntegerv( GLenum pname, GLint* params ) {
	if ( params == NULL ) {
		return;
	}
	switch ( pname ) {
		case GL_MAX_TEXTURE_SIZE:           *params = 16384; break;
		case GL_MAX_TEXTURE_UNITS_ARB:      *params = 8; break;
		case GL_MAX_TEXTURE_COORDS_ARB:     *params = 8; break;
		case GL_MAX_TEXTURE_IMAGE_UNITS_ARB:*params = 16; break;
		// -1 means "no error" when loading ARB programs (see RB_ARB2_LoadProgram)
		case GL_PROGRAM_ERROR_POSITION_ARB: *params = -1; break;
		default:                            *params = 0; break;
	}
}

void APIENTRY nullGetFloatv( GLenum pname, GLfloat* params ) {
	if ( params == NULL ) {
		return;
	}
	switch ( pname ) {
		case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: *params = 16.0f; break;
		default:                                *params = 0.0f; break;
	}
}

void APIENTRY nullGetBooleanv( GLenum pname, GLboolean* params ) {
	if ( params != NULL ) {
		*params = GL_FALSE;
	}
}

// texture/display list names must be unique and non-zero
GLuint nullNextName = 1;

void APIENTRY nullGenTextures( GLsizei n, GLuint* textures ) {
	for ( GLsizei i = 0; i < n; i++ ) {
		textures[i] = nullNextName++;
	}
}

GLuint APIENTRY nullGenLists( GLsizei range ) {
	GLuint first = nullNextName;
	nullNextName += range;
	return first;
}

GLboolean APIENTRY nullIsTexture( GLuint texture ) {
	return texture != 0 ? GL_TRUE : GL_FALSE;
}

struct nullProc_t {
	const char*   name;
	GLExtension_t proc;
};

const nullProc_t nullOverrides[] = {
	{ "glGetString",   (GLExtension_t)nullGetString },
	{ "glGetIntegerv", (GLExtension_t)nullGetIntegerv },
	{ "glGetFloatv",   (GLExtension_t)nullGetFloatv },
	{ "glGetBooleanv", (GLExtension_t)nullGetBooleanv },
	{ "glGenTextures", (GLExtension_t)nullGenTextures },
	{ "glGenLists",    (GLExtension_t)nullGenLists },
	{ "glIsTexture",   (GLExtension_t)nullIsTexture },
};

const nullProc_t nullCoreProcs[] = {
#define QGLPROC(name, rettype, args) { #name, (GLExtension_t)null_##name },
#include "renderer/qgl_proc.h"
#undef QGLPROC
};

// extension functions loaded by R_CheckPortableExtensions() for the
// extensions advertised above (all return void)
const char* const nullExtensionProcs[] = {
	"glMultiTexCoord2fARB",
	"glMultiTexCoord2fvARB",
	"glActiveTextureARB",
	"glClientActiveTextureARB",
	"glStencilOpSeparate",
	"glVertexAttribPointerARB",
	"glEnableVertexAttribArrayARB",
	"glDisableVertexAttribArrayARB",
	"glProgramStringARB",
	"glBindProgramARB",
	"glGenProgramsARB",
	"glProgramEnvParameter4fvARB",
	"glProgramLocalParameter4fvARB",
};

template<typename T, size_t N> size_t ArrayCount( const T (&)[N] ) { return N; }

} // anonymous namespace

GLExtension_t NullGL_GetProcAddress( const char* name ) {
	if ( name == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < ArrayCount( nullOverrides ); i++ ) {
		if ( idStr::Cmp( nullOverrides[i].name, name ) == 0 ) {
			return nullOverrides[i].proc;
		}
	}
	for ( size_t i = 0; i < ArrayCount( nullCoreProcs ); i++ ) {
		if ( idStr::Cmp( nullCoreProcs[i].name, name ) == 0 ) {
			return nullCoreProcs[i].proc;
		}
	}
	for ( size_t i = 0; i < ArrayCount( nullExtensionProcs ); i++ ) {
		if ( idStr::Cmp( nullExtensionProcs[i], name ) == 0 ) {
			return (GLExtension_t)null_ext_noop;
		}
	}
	// not supported, like a real driver would do for an unknown extension
	return NULL;
}
