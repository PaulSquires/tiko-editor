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

#pragma once


type clsDoubleBuffer
    private: 
        _hwnd            as HWND
        _hDC             as HDC
        _memDC           as HDC
        _hbit            as HBITMAP
        _ps              as PAINTSTRUCT
        _rc              as RECT
        _pencolor        as COLORREF
        _forecolor       as COLORREF
        _forecolorhot    as COLORREF
        _backcolor       as COLORREF
        _backcolorhot    as COLORREF
        _FontIndex       as long = GUIFONT_10
        _UsePaint        as boolean       ' use Begin/EndPaint. Used when WM_PAINT or WM_DRAWITEM

    public:

    declare function BeginDoubleBuffer( byval hwnd as HWND ) as long
    declare function BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT ) as long
    declare function EndDoubleBuffer() as long
    declare function PaintClientRect() as long 
    declare function SetupBitmap() as long
    declare function PaintRectFactory( _
                byval rc as RECT ptr, _
                byval iStyle as long, _
                byval nPenWidth as long = 1, _
                byval nCurvature as long, _
                byval bHitTest as boolean = false _
                ) as long 
    declare function PaintRect( _
                byval rc as RECT ptr, _
                byval bHitTest as boolean = false _
                ) as long
    declare function PaintBorderRect( _
                byval rc as RECT ptr, _
                byval bHitTest as boolean = false, _
                byval nPenWidth as long = 1 _
                ) as long
    declare function PaintRoundRect( _
                byval rc as RECT ptr, _
                byval bHitTest as boolean = false, _
                byval nCurvature as long = 20 _
                ) as long 
    declare function PaintRoundBorderRect( _
                byval rc as RECT ptr, _
                byval bHitTest as boolean = false, _
                byval nCurvature as long = 20, _
                byval nPenWidth as long = 1 _
                ) as long 
    declare function PaintIconButton( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval bHitTest as boolean = true, _
            byval nCurvature as long = 20 _
            ) as long
    declare function PaintLine( _
                byval nWidth as long, _
                byval nLeft as long, _
                byval nTop as long, _
                byval nRight as long, _
                byval nBottom as long _
                ) as long
    declare function PaintText( _
                byval wszText as DWSTRING, _
                byval rc as RECT ptr, _ 
                byval wsStyle as DWORD, _
                byval bHitTest as boolean = false _
                ) as long 
    declare function PaintChar( _
                byval wszChar as DWSTRING, _
                byval rc as RECT ptr, _
                byval forecolor as COLORREF _
                ) as long
    declare function SetFont( byval FontIndex as long ) as long
    declare function SetForeColors( byval forecolor as COLORREF, byval forecolorhot as COLORREF ) as long
    declare function SetBackColors( byval backcolor as COLORREF, byval backcolorhot as COLORREF ) as long
    declare function SetPenColor( byval pencolor as COLORREF ) as long
    declare function rcClient() as RECT
    declare function rcClientWidth() as long
    declare function rcClientHeight() as long
    declare function getMemDC() as HDC

end type
