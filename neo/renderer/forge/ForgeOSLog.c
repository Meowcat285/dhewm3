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
 * Replacement for The Forge's platform log backend (Common_3/OS/Windows/WindowsLog.c
 * and Common_3/OS/Linux/LinuxLog.c), compiled into the TheForge library instead of them.
 *
 * The Forge's logger prints every line to stdout itself (_PrintUnicode) and then
 * calls the registered log callbacks. dhewm3 registers a callback that forwards to
 * its console (ForgeBackend.cpp), and dhewm3's console already goes to stdout /
 * dhewm3log.txt, so every Forge line ended up in the log twice. Here _PrintUnicode
 * doesn't write to stdout, so the dhewm3 console is the only place Forge lines go.
 * Everything else behaves like The Forge's own versions.
 */

#include "Application/Config.h"

#include <stdarg.h>
#include <stdio.h>

#include "Utilities/Interfaces/ILog.h"

#ifdef _WINDOWS
#include <windows.h>

// set by The Forge's app framework (WindowsBase.cpp), which dhewm3 doesn't use
HWND* gLogWindowHandle = NULL;
#else
#include <signal.h>
#endif

static bool gIsInteractiveMode = true;

void _EnableInteractiveMode( bool isInteractiveMode ) { gIsInteractiveMode = isInteractiveMode; }

bool _IsInteractiveMode( void ) { return gIsInteractiveMode; }

// debugger output only (Visual Studio's output window on Windows; nothing on Linux
// unless The Forge is built in debug mode, like The Forge's LinuxLog.c)
void _OutputDebugStringV( const char* str, va_list args ) {
#ifdef _WINDOWS
	char buf[4096];
	vsnprintf( buf, sizeof( buf ), str, args );
	OutputDebugStringA( buf );
#elif defined( FORGE_DEBUG )
	vfprintf( stderr, str, args );
#else
	(void)str;
	(void)args;
#endif
}

void _OutputDebugString( const char* str, ... ) {
	va_list args;
	va_start( args, str );
	_OutputDebugStringV( str, args );
	va_end( args );
}

// The assert message itself has already been logged (and so is in the dhewm3 console)
// when this is called. Only break into an attached debugger, never show message boxes
// or stop the game.
void _FailedAssertImpl( const char* file, int line, const char* statement, const char* message ) {
	(void)file;
	(void)line;
	(void)statement;
	(void)message;
	if ( !gIsInteractiveMode ) {
		return;
	}
#ifdef _WINDOWS
	if ( IsDebuggerPresent() ) {
		__debugbreak();
	}
#elif defined( FORGE_DEBUG )
	raise( SIGTRAP );
#endif
}

// Called by The Forge's logger for every line. The line also goes to the log callbacks,
// which forward it to the dhewm3 console, so only pass it on to the debugger here.
void _PrintUnicode( const char* str, bool error ) {
	(void)error;
#ifdef _WINDOWS
	OutputDebugStringA( str );
#else
	(void)str;
#endif
}
