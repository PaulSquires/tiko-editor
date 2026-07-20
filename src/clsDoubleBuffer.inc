'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.


#include once "clsDoubleBuffer.bi"

' ========================================================================================
' Determine if the mouse cursor is currently over client RECT area
' ========================================================================================
function isMouseOverRECT( byval hWin as HWND, byval rc as RECT ) as boolean
    dim as point pt
    GetCursorPos( @pt )
    MapWindowpoints( hWin, HWND_DESKTOP, cast( point ptr, @rc), 2 )
    if PtInRect( @rc, pt ) then return true
end function

function isMouseOverWindow( byval hChild as HWND ) as boolean
    dim as point pt
    GetCursorPos( @pt )
    dim as HWND hWindow = WindowFromPoint(pt)
    if hWindow = hChild then return true
end function

function PaintRect( byval hDC as HDC, byval rc as RECT ptr, byval clr as COLORREF ) as long 
    dim as COLORREF oldclr = SetBkColor(hDC, clr) 
    ExtTextOut( hDC, 0, 0, ETO_OPAQUE, rc, "", 0, 0)
    SetBkColor( hDC, oldclr )
    function = 0
end function

    
function clsDoubleBuffer.SetupBitmap() as long
    _memDC = CreateCompatibleDC( _hDC ) 
    SaveDC( _memDC )
    _hbit = CreateCompatibleBitmap( _hDC, _rc.right - _rc.left, _rc.bottom - _rc.top )
    if _hbit then SelectObject( _memDC, _hbit )
    function = 0
end function

function clsDoubleBuffer.BeginDoubleBuffer( byval hwnd as HWND ) as long
    _UsePaint = true
    _hwnd = hwnd
    _hDC = BeginPaint( _hwnd, @_ps )
    GetClientRect( _hwnd, @_rc )
    this.SetupBitmap
    function = 0
end function

' Overload that provides an hDC rather than use BeginPaint/EndPaint
function clsDoubleBuffer.BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT ) as long
    _UsePaint = false
    _owns = true
    _hwnd = hwnd
    _hDC = hdc
    _rc = rcItem
    this.SetupBitmap
    function = 0
end function

' Cached overload: draw into a caller-owned memDC (bitmap already selected and
' large enough). Saves DC state so per-row object selections don't accumulate;
' EndDoubleBuffer blits but does not delete the DC/bitmap (the caller owns them).
function clsDoubleBuffer.BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT, byval cachedMemDC as HDC ) as long
    _UsePaint = false
    _owns = false
    _hwnd = hwnd
    _hDC = hdc
    _rc = rcItem
    _memDC = cachedMemDC
    _hbit = 0
    SaveDC( _memDC )
    function = 0
end function

function clsDoubleBuffer.EndDoubleBuffer() as long
    BitBlt( _hDC, _rc.left, _rc.top, _rc.right - _rc.left, _rc.bottom - _rc.top, _memDC, 0, 0, SRCCOPY )
    RestoreDC( _memDC, -1 )
    if _owns then
        if _hbit  then DeleteObject( _hbit ) : _hbit = 0
        if _memDC then DeleteDC( _memDC )    : _memDC = 0
    else
        _memDC = 0    ' cached DC is owned by the caller; do not delete
    end if
    if _UsePaint then EndPaint( _hwnd, @_ps ) : _UsePaint = false
    function = 0
end function

' RAII safety net: release any GDI objects still held if EndDoubleBuffer was
' not reached (e.g. an early return between Begin and End). Only frees objects
' this instance owns; handles are nulled by End, so this never double-frees.
destructor clsDoubleBuffer()
    if _owns then
        if _hbit  then DeleteObject( _hbit ) : _hbit = 0
        if _memDC then DeleteDC( _memDC )    : _memDC = 0
    end if
    if _UsePaint andalso (_hwnd <> 0) then EndPaint( _hwnd, @_ps ) : _UsePaint = false
end destructor

function clsDoubleBuffer.rcClient() as RECT
    return _rc
end function

function clsDoubleBuffer.rcClientWidth() as long
    return _rc.right - _rc.left
end function

function clsDoubleBuffer.rcClientHeight() as long
    return _rc.bottom - _rc.top
end function

function clsDoubleBuffer.getMemDC() as HDC
    return _memDC
end function

' Store a caller-supplied font handle. The control/host owns the HFONT and is
' responsible for destroying it; the double buffer only selects it while painting.
function clsDoubleBuffer.SetFont( byval hFont as HFONT ) as long
    if hFont then _hFont = hFont
    function = 0
end function

function clsDoubleBuffer.SetForeColor( byval forecolor as COLORREF ) as long
    _forecolor = forecolor
    function = 0
end function

function clsDoubleBuffer.SetBackColor( byval backcolor as COLORREF ) as long
    _backcolor = backcolor
    function = 0
end function

function clsDoubleBuffer.SetColors( byval forecolor as COLORREF, byval backcolor as COLORREF ) as long
    _forecolor = forecolor
    _backcolor = backcolor
    function = 0
end function

function clsDoubleBuffer.SetPenColor( byval pencolor as COLORREF ) as long
    _pencolor = pencolor
    function = 0
end function
    
function clsDoubleBuffer.PaintRectFactory( _
            byval rc as RECT ptr, _
            byval iStyle as long, _
            byval nPenWidth as long = 1, _
            byval nCurvature as long = 0 _
            ) as long
    dim as COLORREF backcolor = _backcolor

    if nPenWidth = 0 then _pencolor = backcolor
    dim as HPEN hPenSolid = CreatePen( iStyle, nPenWidth, _pencolor )
    dim as HPEN hOldPen = SelectObject( _memDC, hPenSolid )
    
    dim as HBRUSH hBrushSolid = CreateSolidBrush( backcolor )
    dim as HBRUSH hOldBrush = SelectObject( _memDC, hBrushSolid )
    
    RoundRect( _memDC, rc->left, rc->top, rc->right, rc->bottom, nCurvature, nCurvature )
    
    DeleteObject( SelectObject( _memDC, hOldPen ) )
    DeleteObject( SelectObject( _memDC, hOldBrush ) )
    
    function = 0
end function

function clsDoubleBuffer.PaintRect( byval rc as RECT ptr ) as long
    ' Non-Brush painting
    dim as COLORREF oldclr = SetBkColor(_memDC, _backcolor)
    ExtTextOut( _memDC, 0, 0, ETO_OPAQUE, rc, "", 0, 0)
    SetBkColor( _memDC, oldclr )
    return 0
end function

function clsDoubleBuffer.PaintClientRect() as long
    return this.PaintRect( @_rc )
end function

function clsDoubleBuffer.PaintBorderRect( _
            byval rc as RECT ptr, _
            byval nPenWidth as long = 1 _
            ) as long
    return this.PaintRectFactory( rc, PS_SOLID, nPenWidth, 0 )
end function

function clsDoubleBuffer.PaintRoundRect( _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20 _
            ) as long
    return this.PaintRectFactory( rc, PS_NULL, 1, nCurvature )
end function

function clsDoubleBuffer.PaintRoundBorderRect( _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20, _
            byval nPenWidth as long = 1 _
            ) as long
    return this.PaintRectFactory( rc, PS_SOLID, nPenWidth, nCurvature )
end function

function clsDoubleBuffer.PaintText( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval wsStyle as DWORD _
            ) as long
    dim as COLORREF oldforecolor = SetTextColor( _memDC, _forecolor )
    SetBkMode(_memDC, TRANSPARENT)

    dim as HFONT hOldFont
    if _hFont then hOldFont = SelectObject( _memDC, _hFont )

    wsStyle = wsStyle or DT_NOPREFIX or DT_VCENTER or DT_SINGLELINE
    DrawText( _memDC, wszText, -1, cast(lpRect, rc), wsStyle )

    SetTextColor( _memDC, oldforecolor )
    if _hFont then SelectObject( _memDC, hOldFont )

    function = 0
end function

function clsDoubleBuffer.PaintChar( _
            byval wszChar as DWSTRING, _
            byval rc as RECT ptr, _
            byval forecolor as COLORREF _
            ) as long
    
    dim as HFONT hOldFont
    if _hFont then hOldFont = SelectObject( _memDC, _hFont )

    SetBkMode(_memDC, TRANSPARENT)
    SetTextColor(_memDC, forecolor)

    dim as DWORD wsStyle = DT_SINGLELINE or DT_VCENTER or DT_LEFT or DT_NOCLIP
    DrawText( _memDC, wszChar, -1, cast(lpRect, rc), wsStyle )

    if _hFont then SelectObject( _memDC, hOldFont )

    function = 0
end function

function clsDoubleBuffer.PaintIconButton( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20 _
            ) as long
    this.PaintRoundRect( rc, nCurvature )
    this.PaintText( wszText, rc, DT_CENTER )

    function = 0
end function

function clsDoubleBuffer.PaintLine( _
            byval nWidth as long, _
            byval nLeft as long, _
            byval nTop as long, _
            byval nRight as long, _
            byval nBottom as long _
            ) as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(_hwnd)
    if pWindow = 0 then exit function
    dim as long penWidth = pWindow->ScaleY(nWidth)
    dim as HPEN hPenSolid = CreatePen( PS_SOLID, penWidth, _pencolor )
    dim as HPEN hOldPen = SelectObject( _memDC, hPenSolid)
    MoveToEx( _memDC, nLeft, nTop, null )
    LineTo( _memDC, nRight, nBottom )   
    DeleteObject( SelectObject(_memDC, hOldPen) )
    function = 0
end function

