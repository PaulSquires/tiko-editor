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
    _hwnd = hwnd
    _hDC = hdc
    _rc = rcItem
    this.SetupBitmap
    function = 0
end function

function clsDoubleBuffer.EndDoubleBuffer() as long
    BitBlt( _hDC, _rc.left, _rc.top, _rc.right - _rc.left, _rc.bottom - _rc.top, _memDC, 0, 0, SRCCOPY )
    RestoreDC( _memDC, -1 )
    if _hbit  then DeleteObject( _hbit )
    if _memDC then DeleteDC( _memDC )
    if _UsePaint then EndPaint( _hwnd, @_ps )
    function = 0
end function

function clsDoubleBuffer.rcClient() as RECT
    return _rc
end function

function clsDoubleBuffer.rcClientWidth() as long
    return _rc.right - _rc.left
end function

function clsDoubleBuffer.rcClientHeight() as long
    return _rc.bottom - _rc.top
end function

function clsDoubleBuffer.SetFont( byval FontIndex as long ) as long
    if (FontIndex < lbound(ghFont)) orelse (FontIndex > ubound(ghFont)) then exit function
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if (pWindow = 0) orelse (ghFont(FontIndex) = 0) then exit function
    _FontIndex = FontIndex
    return 0
end function

function clsDoubleBuffer.SetForeColors( byval forecolor as COLORREF, byval forecolorhot as COLORREF ) as long
    _forecolor = forecolor
    _forecolorhot = forecolorhot
    function = 0
end function

function clsDoubleBuffer.SetBackColors( byval backcolor as COLORREF, byval backcolorhot as COLORREF ) as long
    _backcolor = backcolor
    _backcolorhot = backcolorhot
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
            byval nCurvature as long, _
            byval bHitTest as boolean = false _
            ) as long 
    dim as COLORREF backcolor = _backcolor  
    if bHitTest then backcolor = iif( isMouseOverRECT(_hwnd, *rc), _backcolorhot, _backcolor )
    
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

function clsDoubleBuffer.PaintRect( _
            byval rc as RECT ptr, _
            byval bHitTest as boolean = false _
            ) as long
    ' Non-Brush painting
    dim as COLORREF backcolor = _backcolor  
    if bHitTest then backcolor = iif( isMouseOverRECT(_hwnd, *rc), _backcolorhot, _backcolor )
    dim as COLORREF oldclr = SetBkColor(_memDC, backcolor) 
    ExtTextOut( _memDC, 0, 0, ETO_OPAQUE, rc, "", 0, 0)
    SetBkColor( _memDC, oldclr )
    return 0
end function

function clsDoubleBuffer.PaintClientRect() as long 
    return this.PaintRect( @_rc, false )
end function

function clsDoubleBuffer.PaintBorderRect( _
            byval rc as RECT ptr, _
            byval bHitTest as boolean = false, _
            byval nPenWidth as long = 1 _
            ) as long
    return this.PaintRectFactory( rc, PS_SOLID, nPenWidth, 0, bHitTest )
end function

function clsDoubleBuffer.PaintRoundRect( _
            byval rc as RECT ptr, _
            byval bHitTest as boolean = false, _
            byval nCurvature as long = 20 _
            ) as long 
    return this.PaintRectFactory( rc, PS_NULL, 1, nCurvature, bHitTest )
end function

function clsDoubleBuffer.PaintRoundBorderRect( _
            byval rc as RECT ptr, _
            byval bHitTest as boolean = false, _
            byval nCurvature as long = 20, _
            byval nPenWidth as long = 1 _
            ) as long 
    return this.PaintRectFactory( rc, PS_SOLID, nPenWidth, nCurvature, bHitTest )
end function

function clsDoubleBuffer.PaintText( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval wsStyle as DWORD, _
            byval bHitTest as boolean = false _
            ) as long
    dim as COLORREF forecolor = _forecolor
    if bHitTest then forecolor = iif( isMouseOverRECT(_hwnd, *rc), _forecolorhot, _forecolor )

    dim as COLORREF oldforecolor = SetTextColor( _memDC, forecolor )
    SetBkMode(_memDC, TRANSPARENT)
    
    dim as HFONT hOldFont = SelectObject( _memDC, ghFont(_FontIndex) ) 

    wsStyle = wsStyle or DT_NOPREFIX or DT_VCENTER or DT_SINGLELINE
    DrawText( _memDC, wszText, -1, cast(lpRect, rc), wsStyle )

    SetTextColor( _memDC, oldforecolor )
    SelectObject(_memDC, hOldFont) 

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

