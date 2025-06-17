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
/*
** WIN_GLIMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_LogComment
** GLimp_Shutdown
**
** Note that the GLW_xxx functions are Windows specific GL-subsystem
** related functions that are relevant ONLY to win_glimp.c
*/
#include <assert.h>
#include "../renderer/tr_local.h"
#include "../qcommon/qcommon.h"
#include "resource.h"
#include "glw_win.h"
#include "win_local.h"

extern void WG_CheckHardwareGamma();
extern void WG_RestoreGamma();

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

#define TRY_PFD_SUCCESS	  0
#define TRY_PFD_FAIL_SOFT 1
#define TRY_PFD_FAIL_HARD 2

#define WINDOW_CLASS_NAME "Quake 3: Arena"

static void		GLW_InitExtensions();
static rserr_t	GLW_SetMode( const char* drivername, int mode, int colorbits, qboolean cdsFullscreen );

static qboolean s_classRegistered = qfalse;

//
// function declaration
//
void			QGL_EnableLogging( qboolean enable );
qboolean		QGL_Init( const char* dllname );
void			QGL_Shutdown();

//
// variable declarations
//
glwstate_t		glw_state;

cvar_t*			r_allowSoftwareGL; // don't abort out if the pixelformat claims software
cvar_t*			r_maskMinidriver;  // allow a different dll name to be treated as if it were opengl32.dll

/*
** GLW_StartDriverAndSetMode
*/
static qboolean GLW_StartDriverAndSetMode( const char* drivername, int mode, int colorbits, qboolean cdsFullscreen )
{
	rserr_t err;

	err = GLW_SetMode( drivername, r_mode->integer, colorbits, cdsFullscreen );

	switch( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
			return qfalse;
		default:
			break;
	}
	return qtrue;
}

/*
** ChoosePFD
**
** Helper function that replaces ChoosePixelFormat.
*/
#define MAX_PFDS 256

static int GLW_ChoosePFD( HDC hDC, PIXELFORMATDESCRIPTOR* pPFD )
{
	return 0;
}

/*
** void GLW_CreatePFD
**
** Helper function zeros out then fills in a PFD
*/
static void GLW_CreatePFD( PIXELFORMATDESCRIPTOR* pPFD, int colorbits, int depthbits, int stencilbits, qboolean stereo )
{
	PIXELFORMATDESCRIPTOR src = {
		sizeof( PIXELFORMATDESCRIPTOR ), // size of this pfd
		1,								 // version number
		PFD_DRAW_TO_WINDOW |			 // support window
			PFD_SUPPORT_OPENGL |		 // support OpenGL
			PFD_DOUBLEBUFFER,			 // double buffered
		PFD_TYPE_RGBA,					 // RGBA type
		24,								 // 24-bit color depth
		0,
		0,
		0,
		0,
		0,
		0, // color bits ignored
		0, // no alpha buffer
		0, // shift bit ignored
		0, // no accumulation buffer
		0,
		0,
		0,
		0,				// accum bits ignored
		24,				// 24-bit z-buffer
		8,				// 8-bit stencil buffer
		0,				// no auxiliary buffer
		PFD_MAIN_PLANE, // main layer
		0,				// reserved
		0,
		0,
		0 // layer masks ignored
	};

	src.cColorBits	 = colorbits;
	src.cDepthBits	 = depthbits;
	src.cStencilBits = stencilbits;

	if( stereo )
	{
		ri.Printf( PRINT_ALL, "...attempting to use stereo\n" );
		src.dwFlags |= PFD_STEREO;
		glConfig.stereoEnabled = qtrue;
	}
	else
	{
		glConfig.stereoEnabled = qfalse;
	}

	*pPFD = src;
}

/*
** GLW_MakeContext
*/
static int GLW_MakeContext( PIXELFORMATDESCRIPTOR* pPFD )
{
	return TRY_PFD_SUCCESS;
}

/*
** GLW_InitDriver
**
** - get a DC if one doesn't exist
** - create an HGLRC if one doesn't exist
*/
static qboolean GLW_InitDriver( const char* drivername, int colorbits )
{
	int							 tpfd;
	int							 depthbits, stencilbits;
	static PIXELFORMATDESCRIPTOR pfd; // save between frames since 'tr' gets cleared

	ri.Printf( PRINT_ALL, "Initializing OpenGL driver\n" );

	//
	// get a DC for our window if we don't already have one allocated
	//
	if( glw_state.hDC == NULL )
	{
		ri.Printf( PRINT_ALL, "...getting DC: " );

		if( ( glw_state.hDC = GetDC( g_wv.hWnd ) ) == NULL )
		{
			ri.Printf( PRINT_ALL, "failed\n" );
			return qfalse;
		}
		ri.Printf( PRINT_ALL, "succeeded\n" );
	}

	if( colorbits == 0 )
	{
		colorbits = glw_state.desktopBitsPixel;
	}

	//
	// implicitly assume Z-buffer depth == desktop color depth
	//
	if( r_depthbits->integer == 0 )
	{
		if( colorbits > 16 )
		{
			depthbits = 24;
		}
		else
		{
			depthbits = 16;
		}
	}
	else
	{
		depthbits = r_depthbits->integer;
	}

	//
	// do not allow stencil if Z-buffer depth likely won't contain it
	//
	stencilbits = r_stencilbits->integer;
	if( depthbits < 24 )
	{
		stencilbits = 0;
	}

	//
	// make two attempts to set the PIXELFORMAT
	//

	//
	// first attempt: r_colorbits, depthbits, and r_stencilbits
	//
	if( !glw_state.pixelFormatSet )
	{
		GLW_CreatePFD( &pfd, colorbits, depthbits, stencilbits, r_stereo->integer );
		if( ( tpfd = GLW_MakeContext( &pfd ) ) != TRY_PFD_SUCCESS )
		{
			if( tpfd == TRY_PFD_FAIL_HARD )
			{
				ri.Printf( PRINT_WARNING, "...failed hard\n" );
				return qfalse;
			}

			//
			// punt if we've already tried the desktop bit depth and no stencil bits
			//
			if( ( r_colorbits->integer == glw_state.desktopBitsPixel ) && ( stencilbits == 0 ) )
			{
				ReleaseDC( g_wv.hWnd, glw_state.hDC );
				glw_state.hDC = NULL;

				ri.Printf( PRINT_ALL, "...failed to find an appropriate PIXELFORMAT\n" );

				return qfalse;
			}

			//
			// second attempt: desktop's color bits and no stencil
			//
			if( colorbits > glw_state.desktopBitsPixel )
			{
				colorbits = glw_state.desktopBitsPixel;
			}
			GLW_CreatePFD( &pfd, colorbits, depthbits, 0, r_stereo->integer );
			if( GLW_MakeContext( &pfd ) != TRY_PFD_SUCCESS )
			{
				if( glw_state.hDC )
				{
					ReleaseDC( g_wv.hWnd, glw_state.hDC );
					glw_state.hDC = NULL;
				}

				ri.Printf( PRINT_ALL, "...failed to find an appropriate PIXELFORMAT\n" );

				return qfalse;
			}
		}

		/*
		** report if stereo is desired but unavailable
		*/
		if( !( pfd.dwFlags & PFD_STEREO ) && ( r_stereo->integer != 0 ) )
		{
			ri.Printf( PRINT_ALL, "...failed to select stereo pixel format\n" );
			glConfig.stereoEnabled = qfalse;
		}
	}

	/*
	** store PFD specifics
	*/
	glConfig.colorBits	 = ( int )pfd.cColorBits;
	glConfig.depthBits	 = ( int )pfd.cDepthBits;
	glConfig.stencilBits = ( int )pfd.cStencilBits;

	return qtrue;
}

LONG WINAPI MainWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

/*
** GLW_CreateWindow
**
** Responsible for creating the Win32 window and initializing the OpenGL driver.
*/
#define WINDOW_STYLE ( WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_VISIBLE )
static qboolean GLW_CreateWindow( const char* drivername, int width, int height, int colorbits, qboolean cdsFullscreen )
{
	RECT	r;
	cvar_t *vid_xpos, *vid_ypos;
	int		stylebits;
	int		x, y, w, h;
	int		exstyle;

	//
	// register the window class if necessary
	//
	if( !s_classRegistered )
	{
		WNDCLASS wc;

		memset( &wc, 0, sizeof( wc ) );

		wc.style		 = 0;
		wc.lpfnWndProc	 = ( WNDPROC )MainWndProc;
		wc.cbClsExtra	 = 0;
		wc.cbWndExtra	 = 0;
		wc.hInstance	 = g_wv.hInstance;
		wc.hIcon		 = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE( IDI_ICON1 ) );
		wc.hCursor		 = LoadCursor( NULL, IDC_ARROW );
		wc.hbrBackground = ( void* )COLOR_GRAYTEXT;
		wc.lpszMenuName	 = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME;

		if( !RegisterClass( &wc ) )
		{
			ri.Error( ERR_FATAL, "GLW_CreateWindow: could not register window class" );
		}
		s_classRegistered = qtrue;
		ri.Printf( PRINT_ALL, "...registered window class\n" );
	}

	//
	// create the HWND if one does not already exist
	//
	if( !g_wv.hWnd )
	{
		//
		// compute width and height
		//
		r.left	 = 0;
		r.top	 = 0;
		r.right	 = width;
		r.bottom = height;

		if( cdsFullscreen || !Q_stricmp( _3DFX_DRIVER_NAME, drivername ) )
		{
			exstyle	  = WS_EX_TOPMOST;
			stylebits = WS_POPUP | WS_VISIBLE | WS_SYSMENU;
		}
		else
		{
			exstyle	  = 0;
			stylebits = WINDOW_STYLE | WS_SYSMENU;
			AdjustWindowRect( &r, stylebits, FALSE );
		}

		w = r.right - r.left;
		h = r.bottom - r.top;

		if( cdsFullscreen || !Q_stricmp( _3DFX_DRIVER_NAME, drivername ) )
		{
			x = 0;
			y = 0;
		}
		else
		{
			vid_xpos = ri.Cvar_Get( "vid_xpos", "", 0 );
			vid_ypos = ri.Cvar_Get( "vid_ypos", "", 0 );
			x		 = vid_xpos->integer;
			y		 = vid_ypos->integer;

			// adjust window coordinates if necessary
			// so that the window is completely on screen
			if( x < 0 )
				x = 0;
			if( y < 0 )
				y = 0;

			if( w < glw_state.desktopWidth && h < glw_state.desktopHeight )
			{
				if( x + w > glw_state.desktopWidth )
					x = ( glw_state.desktopWidth - w );
				if( y + h > glw_state.desktopHeight )
					y = ( glw_state.desktopHeight - h );
			}
		}

		// g_wv.hInstance = GetModuleHandle(NULL);

		g_wv.hWnd = CreateWindowEx( exstyle, WINDOW_CLASS_NAME, "Quake 3: Arena", stylebits, x, y, w, h, NULL, NULL, g_wv.hInstance, NULL );

		if( !g_wv.hWnd )
		{
			ri.Error( ERR_FATAL, "GLW_CreateWindow() - Couldn't create window" );
		}

		ShowWindow( g_wv.hWnd, SW_SHOW );
		UpdateWindow( g_wv.hWnd );
		ri.Printf( PRINT_ALL, "...created window@%d,%d (%dx%d)\n", x, y, w, h );
	}
	else
	{
		ri.Printf( PRINT_ALL, "...window already present, CreateWindowEx skipped\n" );
	}

	if( !GLW_InitDriver( drivername, colorbits ) )
	{
		ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;

		return qfalse;
	}

	SetForegroundWindow( g_wv.hWnd );
	SetFocus( g_wv.hWnd );

	return qtrue;
}

static void PrintCDSError( int value )
{
	switch( value )
	{
		case DISP_CHANGE_RESTART:
			ri.Printf( PRINT_ALL, "restart required\n" );
			break;
		case DISP_CHANGE_BADPARAM:
			ri.Printf( PRINT_ALL, "bad param\n" );
			break;
		case DISP_CHANGE_BADFLAGS:
			ri.Printf( PRINT_ALL, "bad flags\n" );
			break;
		case DISP_CHANGE_FAILED:
			ri.Printf( PRINT_ALL, "DISP_CHANGE_FAILED\n" );
			break;
		case DISP_CHANGE_BADMODE:
			ri.Printf( PRINT_ALL, "bad mode\n" );
			break;
		case DISP_CHANGE_NOTUPDATED:
			ri.Printf( PRINT_ALL, "not updated\n" );
			break;
		default:
			ri.Printf( PRINT_ALL, "unknown error %d\n", value );
			break;
	}
}

/*
** GLW_SetMode
*/
static rserr_t GLW_SetMode( const char* drivername, int mode, int colorbits, qboolean cdsFullscreen )
{
	HDC			hDC;
	const char* win_fs[] = { "W", "FS" };
	int			cdsRet;
	DEVMODE		dm;

	//
	// print out informational messages
	//
	ri.Printf( PRINT_ALL, "...setting mode %d:", mode );
	if( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d %s\n", glConfig.vidWidth, glConfig.vidHeight, win_fs[cdsFullscreen] );

	//
	// check our desktop attributes
	//
	hDC						   = GetDC( GetDesktopWindow() );
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth	   = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight	   = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	//
	// verify desktop bit depth
	//
	if( glConfig.driverType != GLDRV_VOODOO )
	{
		if( glw_state.desktopBitsPixel < 15 || glw_state.desktopBitsPixel == 24 )
		{
			if( colorbits == 0 || ( !cdsFullscreen && colorbits >= 15 ) )
			{
				if( MessageBox( NULL,
						"It is highly unlikely that a correct\n"
						"windowed display can be initialized with\n"
						"the current desktop display depth.  Select\n"
						"'OK' to try anyway.  Press 'Cancel' if you\n"
						"have a 3Dfx Voodoo, Voodoo-2, or Voodoo Rush\n"
						"3D accelerator installed, or if you otherwise\n"
						"wish to quit.",
						"Low Desktop Color Depth",
						MB_OKCANCEL | MB_ICONEXCLAMATION ) != IDOK )
				{
					return RSERR_INVALID_MODE;
				}
			}
		}
	}

	// do a CDS if needed
	if( cdsFullscreen )
	{
		memset( &dm, 0, sizeof( dm ) );

		dm.dmSize = sizeof( dm );

		dm.dmPelsWidth	= glConfig.vidWidth;
		dm.dmPelsHeight = glConfig.vidHeight;
		dm.dmFields		= DM_PELSWIDTH | DM_PELSHEIGHT;

		if( r_displayRefresh->integer != 0 )
		{
			dm.dmDisplayFrequency = r_displayRefresh->integer;
			dm.dmFields |= DM_DISPLAYFREQUENCY;
		}

		// try to change color depth if possible
		if( colorbits != 0 )
		{
			if( glw_state.allowdisplaydepthchange )
			{
				dm.dmBitsPerPel = colorbits;
				dm.dmFields |= DM_BITSPERPEL;
				ri.Printf( PRINT_ALL, "...using colorsbits of %d\n", colorbits );
			}
			else
			{
				ri.Printf( PRINT_ALL, "WARNING:...changing depth not supported on Win95 < pre-OSR 2.x\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...using desktop display depth of %d\n", glw_state.desktopBitsPixel );
		}

		//
		// if we're already in fullscreen then just create the window
		//
		if( glw_state.cdsFullscreen )
		{
			ri.Printf( PRINT_ALL, "...already fullscreen, avoiding redundant CDS\n" );

			if( !GLW_CreateWindow( drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue ) )
			{
				ri.Printf( PRINT_ALL, "...restoring display settings\n" );
				ChangeDisplaySettings( 0, 0 );
				return RSERR_INVALID_MODE;
			}
		}
		//
		// need to call CDS
		//
		else
		{
			ri.Printf( PRINT_ALL, "...calling CDS: " );

			// try setting the exact mode requested, because some drivers don't report
			// the low res modes in EnumDisplaySettings, but still work
			if( ( cdsRet = ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) ) == DISP_CHANGE_SUCCESSFUL )
			{
				ri.Printf( PRINT_ALL, "ok\n" );

				if( !GLW_CreateWindow( drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue ) )
				{
					ri.Printf( PRINT_ALL, "...restoring display settings\n" );
					ChangeDisplaySettings( 0, 0 );
					return RSERR_INVALID_MODE;
				}

				glw_state.cdsFullscreen = qtrue;
			}
			else
			{
				//
				// the exact mode failed, so scan EnumDisplaySettings for the next largest mode
				//
				DEVMODE devmode;
				int		modeNum;

				ri.Printf( PRINT_ALL, "failed, " );

				PrintCDSError( cdsRet );

				ri.Printf( PRINT_ALL, "...trying next higher resolution:" );

				// we could do a better matching job here...
				for( modeNum = 0;; modeNum++ )
				{
					if( !EnumDisplaySettings( NULL, modeNum, &devmode ) )
					{
						modeNum = -1;
						break;
					}
					if( devmode.dmPelsWidth >= glConfig.vidWidth && devmode.dmPelsHeight >= glConfig.vidHeight && devmode.dmBitsPerPel >= 15 )
					{
						break;
					}
				}

				if( modeNum != -1 && ( cdsRet = ChangeDisplaySettings( &devmode, CDS_FULLSCREEN ) ) == DISP_CHANGE_SUCCESSFUL )
				{
					ri.Printf( PRINT_ALL, " ok\n" );
					if( !GLW_CreateWindow( drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue ) )
					{
						ri.Printf( PRINT_ALL, "...restoring display settings\n" );
						ChangeDisplaySettings( 0, 0 );
						return RSERR_INVALID_MODE;
					}

					glw_state.cdsFullscreen = qtrue;
				}
				else
				{
					ri.Printf( PRINT_ALL, " failed, " );

					PrintCDSError( cdsRet );

					ri.Printf( PRINT_ALL, "...restoring display settings\n" );
					ChangeDisplaySettings( 0, 0 );

					glw_state.cdsFullscreen = qfalse;
					glConfig.isFullscreen	= qfalse;
					if( !GLW_CreateWindow( drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qfalse ) )
					{
						return RSERR_INVALID_MODE;
					}
					return RSERR_INVALID_FULLSCREEN;
				}
			}
		}
	}
	else
	{
		if( glw_state.cdsFullscreen )
		{
			ChangeDisplaySettings( 0, 0 );
		}

		glw_state.cdsFullscreen = qfalse;
		if( !GLW_CreateWindow( drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qfalse ) )
		{
			return RSERR_INVALID_MODE;
		}
	}

	//
	// success, now check display frequency, although this won't be valid on Voodoo(2)
	//
	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );
	if( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm ) )
	{
		glConfig.displayFrequency = dm.dmDisplayFrequency;
	}

	// NOTE: this is overridden later on standalone 3Dfx drivers
	glConfig.isFullscreen = cdsFullscreen;

	return RSERR_OK;
}

/*
** GLW_CheckOSVersion
*/
static qboolean GLW_CheckOSVersion()
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO vinfo;

	vinfo.dwOSVersionInfoSize = sizeof( vinfo );

	glw_state.allowdisplaydepthchange = qfalse;

	if( GetVersionEx( &vinfo ) )
	{
		if( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = qtrue;
		}
		else if( vinfo.dwMajorVersion == 4 )
		{
			if( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = qtrue;
			}
			else if( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				if( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = qtrue;
				}
			}
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "GLW_CheckOSVersion() - GetVersionEx failed\n" );
		return qfalse;
	}

	return qtrue;
}

/*
** GLW_LoadOpenGL
**
** GLimp_win.c internal function that attempts to load and use
** a specific OpenGL DLL.
*/
static qboolean GLW_LoadOpenGL( const char* drivername )
{
	char	 buffer[1024];
	qboolean cdsFullscreen;

	Q_strncpyz( buffer, drivername, sizeof( buffer ) );
	Q_strlwr( buffer );

	//
	// determine if we're on a standalone driver
	//
	if( strstr( buffer, "opengl32" ) != 0 || r_maskMinidriver->integer )
	{
		glConfig.driverType = GLDRV_ICD;
	}
	else
	{
		glConfig.driverType = GLDRV_STANDALONE;

		ri.Printf( PRINT_ALL, "...assuming '%s' is a standalone driver\n", drivername );

		if( strstr( buffer, _3DFX_DRIVER_NAME ) )
		{
			glConfig.driverType = GLDRV_VOODOO;
		}
	}

	// disable the 3Dfx splash screen
	_putenv( "FX_GLIDE_NO_SPLASH=0" );

	//
	// load the driver and bind our function pointers to it
	//
	if( QGL_Init( buffer ) )
	{
		cdsFullscreen = r_fullscreen->integer;

		// create the window and set up the context
		if( !GLW_StartDriverAndSetMode( drivername, r_mode->integer, r_colorbits->integer, cdsFullscreen ) )
		{
			// if we're on a 24/32-bit desktop and we're going fullscreen on an ICD,
			// try it again but with a 16-bit desktop
			if( glConfig.driverType == GLDRV_ICD )
			{
				if( r_colorbits->integer != 16 || cdsFullscreen != qtrue || r_mode->integer != 3 )
				{
					if( !GLW_StartDriverAndSetMode( drivername, 3, 16, qtrue ) )
					{
						goto fail;
					}
				}
			}
			else
			{
				goto fail;
			}
		}

		if( glConfig.driverType == GLDRV_VOODOO )
		{
			glConfig.isFullscreen = qtrue;
		}

		GL_Init( g_wv.hWnd, g_wv.hInstance, glConfig.vidWidth, glConfig.vidHeight );

		return qtrue;
	}
fail:

	QGL_Shutdown();

	return qfalse;
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame()
{
	GL_EndRendering();
}

static void GLW_StartOpenGL()
{
	qboolean attemptedOpenGL32 = qfalse;
	qboolean attempted3Dfx	   = qfalse;

	//
	// load and initialize the specific OpenGL driver
	//
	if( !GLW_LoadOpenGL( r_glDriver->string ) )
	{
		if( !Q_stricmp( r_glDriver->string, OPENGL_DRIVER_NAME ) )
		{
			attemptedOpenGL32 = qtrue;
		}
		else if( !Q_stricmp( r_glDriver->string, _3DFX_DRIVER_NAME ) )
		{
			attempted3Dfx = qtrue;
		}

		if( !attempted3Dfx )
		{
			attempted3Dfx = qtrue;
			if( GLW_LoadOpenGL( _3DFX_DRIVER_NAME ) )
			{
				ri.Cvar_Set( "r_glDriver", _3DFX_DRIVER_NAME );
				r_glDriver->modified = qfalse;
			}
			else
			{
				if( !attemptedOpenGL32 )
				{
					if( !GLW_LoadOpenGL( OPENGL_DRIVER_NAME ) )
					{
						ri.Error( ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n" );
					}
					ri.Cvar_Set( "r_glDriver", OPENGL_DRIVER_NAME );
					r_glDriver->modified = qfalse;
				}
				else
				{
					ri.Error( ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n" );
				}
			}
		}
		else if( !attemptedOpenGL32 )
		{
			attemptedOpenGL32 = qtrue;
			if( GLW_LoadOpenGL( OPENGL_DRIVER_NAME ) )
			{
				ri.Cvar_Set( "r_glDriver", OPENGL_DRIVER_NAME );
				r_glDriver->modified = qfalse;
			}
			else
			{
				ri.Error( ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n" );
			}
		}
	}
}

/*
** GLimp_Init
**
** This is the platform specific OpenGL initialization function.  It
** is responsible for loading OpenGL, initializing it, setting
** extensions, creating a window of the appropriate size, doing
** fullscreen manipulations, etc.  Its overall responsibility is
** to make sure that a functional OpenGL subsystem is operating
** when it returns to the ref.
*/
void GLimp_Init()
{
	char	buf[1024];
	cvar_t* lastValidRenderer = ri.Cvar_Get( "r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE );
	cvar_t* cv;

	ri.Printf( PRINT_ALL, "Initializing OpenGL subsystem\n" );

	//
	// check OS version to see if we can do fullscreen display changes
	//
	if( !GLW_CheckOSVersion() )
	{
		ri.Error( ERR_FATAL, "GLimp_Init() - incorrect operating system\n" );
	}

	// save off hInstance and wndproc
	// cv = ri.Cvar_Get( "win_hinstance", "", 0 );
	// sscanf( cv->string, "%i", (int *)&g_wv.hInstance );

	cv = ri.Cvar_Get( "win_wndproc", "", 0 );
	sscanf( cv->string, "%i", ( int* )&glw_state.wndproc );

	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	r_maskMinidriver  = ri.Cvar_Get( "r_maskMinidriver", "0", CVAR_LATCH );

	// load appropriate DLL and initialize subsystem
	GLW_StartOpenGL();

	// get our config strings
	// Q_strncpyz( glConfig.vendor_string, qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	// Q_strncpyz( glConfig.renderer_string, qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	// Q_strncpyz( glConfig.version_string, qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	// Q_strncpyz( glConfig.extensions_string, qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	//
	// chipset specific configuration
	//
	Q_strncpyz( buf, glConfig.renderer_string, sizeof( buf ) );
	Q_strlwr( buf );

	//
	// NOTE: if changing cvars, do it within this block.  This allows them
	// to be overridden when testing driver fixes, etc. but only sets
	// them to their default state when the hardware is first installed/run.
	//
	if( Q_stricmp( lastValidRenderer->string, glConfig.renderer_string ) )
	{
		glConfig.hardwareType = GLHW_GENERIC;

		ri.Cvar_Set( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST" );

		// VOODOO GRAPHICS w/ 2MB
		if( strstr( buf, "voodoo graphics/1 tmu/2 mb" ) )
		{
			ri.Cvar_Set( "r_picmip", "2" );
			ri.Cvar_Get( "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
		}
		else
		{
			ri.Cvar_Set( "r_picmip", "1" );

			if( strstr( buf, "rage 128" ) || strstr( buf, "rage128" ) )
			{
				ri.Cvar_Set( "r_finish", "0" );
			}
			// Savage3D and Savage4 should always have trilinear enabled
			else if( strstr( buf, "savage3d" ) || strstr( buf, "s3 savage4" ) )
			{
				ri.Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
			}
		}
	}

	//
	// this is where hardware specific workarounds that should be
	// detected/initialized every startup should go.
	//
	if( strstr( buf, "banshee" ) || strstr( buf, "voodoo3" ) )
	{
		glConfig.hardwareType = GLHW_3DFX_2D3D;
	}
	// VOODOO GRAPHICS w/ 2MB
	else if( strstr( buf, "voodoo graphics/1 tmu/2 mb" ) )
	{
	}
	else if( strstr( buf, "glzicd" ) )
	{
	}
	else if( strstr( buf, "rage pro" ) || strstr( buf, "Rage Pro" ) || strstr( buf, "ragepro" ) )
	{
		glConfig.hardwareType = GLHW_RAGEPRO;
	}
	else if( strstr( buf, "rage 128" ) )
	{
	}
	else if( strstr( buf, "permedia2" ) )
	{
		glConfig.hardwareType = GLHW_PERMEDIA2;
	}
	else if( strstr( buf, "riva 128" ) )
	{
		glConfig.hardwareType = GLHW_RIVA128;
	}
	else if( strstr( buf, "riva tnt " ) )
	{
	}

	ri.Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );

	// GLW_InitExtensions();
	WG_CheckHardwareGamma();
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.
*/
void GLimp_Shutdown()
{
	//	const char *strings[] = { "soft", "hard" };
	const char* success[] = { "failed", "success" };
	int			retVal;

	ri.Printf( PRINT_ALL, "Shutting down OpenGL subsystem\n" );

	// restore gamma.  We do this first because 3Dfx's extension needs a valid OGL subsystem
	WG_RestoreGamma();

	// release DC
	if( glw_state.hDC )
	{
		retVal = ReleaseDC( g_wv.hWnd, glw_state.hDC ) != 0;
		ri.Printf( PRINT_ALL, "...releasing DC: %s\n", success[retVal] );
		glw_state.hDC = NULL;
	}

	// destroy window
	if( g_wv.hWnd )
	{
		ri.Printf( PRINT_ALL, "...destroying window\n" );
		ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd				 = NULL;
		glw_state.pixelFormatSet = qfalse;
	}

	// close the r_logFile
	if( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	// reset display settings
	if( glw_state.cdsFullscreen )
	{
		ri.Printf( PRINT_ALL, "...resetting display\n" );
		ChangeDisplaySettings( 0, 0 );
		glw_state.cdsFullscreen = qfalse;
	}

	// shutdown QGL subsystem
	QGL_Shutdown();

	memset( &glConfig, 0, sizeof( glConfig ) );
	memset( &glState, 0, sizeof( glState ) );
}

/*
** GLimp_LogComment
*/
void GLimp_LogComment( char* comment )
{
	if( glw_state.log_fp )
	{
		fprintf( glw_state.log_fp, "%s", comment );
	}
}

/*
===========================================================

SMP acceleration

===========================================================
*/

HANDLE renderCommandsEvent;
HANDLE renderCompletedEvent;
HANDLE renderActiveEvent;

void ( *glimpRenderThread )();

void GLimp_RenderThreadWrapper()
{
	glimpRenderThread();

	// unbind the context before we die
	// qwglMakeCurrent( glw_state.hDC, NULL );
}

/*
=======================
GLimp_SpawnRenderThread
=======================
*/
HANDLE	 renderThreadHandle;
int		 renderThreadId;
qboolean GLimp_SpawnRenderThread( void ( *function )() )
{
	renderCommandsEvent	 = CreateEvent( NULL, TRUE, FALSE, NULL );
	renderCompletedEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	renderActiveEvent	 = CreateEvent( NULL, TRUE, FALSE, NULL );

	glimpRenderThread = function;

	renderThreadHandle = CreateThread( NULL,				 // LPSECURITY_ATTRIBUTES lpsa,
		0,													 // DWORD cbStack,
		( LPTHREAD_START_ROUTINE )GLimp_RenderThreadWrapper, // LPTHREAD_START_ROUTINE lpStartAddr,
		0,													 // LPVOID lpvThreadParm,
		0,													 //   DWORD fdwCreate,
		&renderThreadId );

	if( !renderThreadHandle )
	{
		return qfalse;
	}

	return qtrue;
}

static void* smpData;
static int	 wglErrors;

void*		 GLimp_RendererSleep()
{
	return NULL;
}

void GLimp_FrontEndSleep()
{
	// WaitForSingleObject( renderCompletedEvent, INFINITE );
	//
	// if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) ) {
	//	wglErrors++;
	// }
}

void GLimp_WakeRenderer( void* data )
{
	smpData = data;

	// if ( !qwglMakeCurrent( glw_state.hDC, NULL ) ) {
	//	wglErrors++;
	// }

	// after this, the renderer can continue through GLimp_RendererSleep
	SetEvent( renderCommandsEvent );

	WaitForSingleObject( renderActiveEvent, INFINITE );
}
