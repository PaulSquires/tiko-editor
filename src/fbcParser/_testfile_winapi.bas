'' Performance test input: pulls in the whole Win32 API surface.
'' Scan with FBCPARSER_INCDIR pointing at the FreeBASIC toolchain's inc dir,
'' e.g. C:\dev\tiko_editor\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\inc

#include once "windows.bi"

dim shared gTestWnd as HWND

declare function TestWndProc _
	( _
		byval hwnd as HWND, _
		byval uMsg as UINT, _
		byval wParam as WPARAM, _
		byval lParam as LPARAM _
	) as LRESULT
