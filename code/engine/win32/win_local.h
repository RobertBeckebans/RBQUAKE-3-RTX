/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// win_local.h: Win32-specific Quake3 header file

#if defined( _MSC_VER ) && ( _MSC_VER >= 1200 )
	#pragma warning( disable : 4201 )
	#pragma warning( push )
#endif
#include <windows.h>
#if defined( _MSC_VER ) && ( _MSC_VER >= 1200 )
	#pragma warning( pop )
#endif

#define DIRECTSOUND_VERSION 0x0300
#define DIRECTINPUT_VERSION 0x0300

#include <dinput.h>
#include <dsound.h>
#include <winsock.h>
#include <wsipx.h>

#define GWL_WNDPROC	   ( -4 )
#define GWL_HINSTANCE  ( -6 )
#define GWL_HWNDPARENT ( -8 )
#define GWL_STYLE	   ( -16 )
#define GWL_EXSTYLE	   ( -20 )
#define GWL_USERDATA   ( -21 )
#define GWL_ID		   ( -12 )

#ifdef _WIN64

	#undef GWL_WNDPROC
	#undef GWL_HINSTANCE
	#undef GWL_HWNDPARENT
	#undef GWL_USERDATA

#endif /* _WIN64 */

#define GWLP_WNDPROC	( -4 )
#define GWLP_HINSTANCE	( -6 )
#define GWLP_HWNDPARENT ( -8 )
#define GWLP_USERDATA	( -21 )
#define GWLP_ID			( -12 )

#undef SetWindowLong
#undef GetWindowLong

#define SetWindowLong SetWindowLongPtr
#define GetWindowLong GetWindowLongPtr

void		IN_MouseEvent( int mstate );

void		Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void* ptr );

void		Sys_CreateConsole();
void		Sys_DestroyConsole();

char*		Sys_ConsoleInput();

qboolean	Sys_GetPacket( netadr_t* net_from, msg_t* net_message );

// Input subsystem

void		IN_Init();
void		IN_Shutdown();
void		IN_JoystickCommands();

void		IN_Move( usercmd_t* cmd );
// add additional non keyboard / non mouse movement on top of the keyboard move cmd

void		IN_DeactivateWin32Mouse();

void		IN_Activate( qboolean active );
void		IN_Frame();

// window procedure
LONG WINAPI MainWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

void		Conbuf_AppendText( const char* msg );

void		SNDDMA_Activate();
int			SNDDMA_InitDS();

typedef struct
{
	HINSTANCE	  reflib_library; // Handle to refresh DLL
	qboolean	  reflib_active;

	HWND		  hWnd;
	HINSTANCE	  hInstance;
	qboolean	  activeApp;
	qboolean	  isMinimized;
	OSVERSIONINFO osversion;

	// when we get a windows message, we store the time off so keyboard processing
	// can know the exact time of an event
	unsigned	  sysMsgTime;
} WinVars_t;

extern WinVars_t g_wv;
