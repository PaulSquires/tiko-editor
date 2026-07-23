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

' ========================================================================================
' RENDERING
' ----------------------------------------------------------------------------------------
' GEOMETRY (fills, rounded rects, borders, ellipses, lines) -> GDI+, antialiased where it
'                                                              helps.
' TEXT     (PaintText / PaintChar)                          -> GDI DrawText. Deliberate.
'
' Text stays on GDI because GDI+ measures and lays out text differently: every
' GetTextExtentPoint32W / GetTextMetricsW site in the control family's LayoutItems would
' have to convert to MeasureString in lockstep or text clips, and the icon glyphs
' (Segoe Fluent Icons) would shift. That is a separate project of its own.
'
' Because the two APIs share one HDC and only one of them batches, every GDI call the class
' makes is preceded by a flush -- see EnsureGdiReady. That is the whole cost of the split,
' and it is paid in here so no control or host ever has to think about it.
'
' HISTORY, so the absence is not mistaken for an oversight: this class carried BOTH a GDI
' and a GDI+ renderer behind a `#define DBUF_GDIPLUS`, as clsDoubleBuffer. The dual backend
' existed to make the migration's A/B screenshot diff a one-variable experiment and to make
' the self-test two-sided -- assertions passing on both backends are ground truth rather
' than a snapshot. That migration shipped, so the reason expired and the GDI arms are gone.
' The cost is real and worth stating: the self-test below can no longer prove its expected
' numbers against an independent implementation. Those numbers were MEASURED off the GDI
' backend while both existed; do not "correct" them by reasoning about what the docs imply.
' ========================================================================================

' Included HERE rather than left to the call site on purpose. CListBox.bi names typedefs it
' does not include and so only compiles where the host happens to have pre-loaded them (see
' CLAUDE.md); this file does not repeat that trap.
'
' ONE THING THIS COSTS THE HOST, and it is worth knowing before you adopt it:
' GDI+'s Status enum defines Ok = 0 in namespace AfxNova. Every host in this family already
' says "using AfxNova", so including this header puts Ok into the host's namespace and ANY
' identifier named "ok" -- a variable, a parameter -- becomes a duplicated definition. Five
' of the sibling demos had a SelfTest_Check parameter called exactly that and stopped
' compiling the moment they took this file. The fix is to rename yours (bOK is what the
' family uses); it cannot be fixed from in here, because the host's own "using AfxNova" is
' what exposes the name.
#include once "AfxNova\CGdiPlus.inc"
using AfxNova

declare function isMouseOverRECT( byval hWin as HWND, byval rc as RECT ) as boolean
declare function isMouseOverWindow( byval hChild as HWND ) as boolean
declare function PaintRect( byval hDC as HDC, byval rc as RECT ptr, byval clr as COLORREF ) as long

type CBufferPaint
    private:
        _hwnd            as HWND
        _hDC             as HDC
        _memDC           as HDC
        _hbit            as HBITMAP
        _ps              as PAINTSTRUCT
        _rc              as RECT
        _pencolor        as COLORREF
        _forecolor       as COLORREF
        _backcolor       as COLORREF
        ' Alpha is carried alongside the COLORREF rather than folded into it: every
        ' existing SetXxxColor call keeps working unchanged and simply means "opaque".
        ' Only the SetXxxColorA overloads ever set these to anything but 255.
        _penalpha        as ubyte = 255
        _forealpha       as ubyte = 255
        _backalpha       as ubyte = 255
        _hFont           as HFONT         ' caller-supplied font; the control/host owns it
        _UsePaint        as boolean       ' use Begin/EndPaint. Used when WM_PAINT or WM_DRAWITEM
        _owns            as boolean = true ' does this object own _memDC/_hbit (delete on End)?

        ' One Graphics per buffer, built on first use and torn down before the blit.
        _pGraphics       as CGpGraphics ptr
        ' Brush and pen are cached and reused across calls, keyed on what they were
        ' built from. A CListBox repaint runs this path once per visible row, so
        ' allocating a fresh GDI+ object per fill would be pure churn.
        _pBrush          as CGpSolidBrush ptr
        _brushARGB       as ARGB
        _pPen            as CGpPen ptr
        _penARGB         as ARGB
        _penWidthCached  as single
        ' Set by any GDI+ draw, cleared by a flush. See EnsureGdiReady.
        _gpDirty         as boolean

        declare function EnsureGraphics() as CGpGraphics ptr
        declare function BrushFor( byval clr as ARGB ) as CGpBrush ptr
        declare function PenFor( byval clr as ARGB, byval nWidth as single ) as CGpPen ptr
        ' GDI+ batches its drawing; GDI does not. Mixing the two on one HDC without a
        ' flush loses shapes intermittently -- so every GDI text call goes through this
        ' first. Centralised here so no control or host ever has to think about it.
        declare sub      EnsureGdiReady()
        declare sub      ReleaseGpObjects()
        declare sub      BuildRoundPath( _
                    byval pPath as CGpGraphicsPath ptr, _
                    byval x as single, _
                    byval y as single, _
                    byval w as single, _
                    byval h as single, _
                    byval radius as single _
                    )

    public:

    declare destructor()
    declare function BeginDoubleBuffer( byval hwnd as HWND ) as long
    declare function BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT ) as long
    ' Cached variant: reuse a caller-owned memDC (with its bitmap already selected);
    ' EndDoubleBuffer will blit but NOT delete it. Used to avoid per-row GDI churn.
    declare function BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT, byval cachedMemDC as HDC ) as long
    declare function EndDoubleBuffer() as long
    declare function PaintClientRect() as long
    declare function SetupBitmap() as long
    ' Painting always uses the CURRENT fore/back colors - hot/hover styling is
    ' the caller's responsibility: decide (e.g. via isMouseOverRECT or tracked
    ' hover state) and set the colors BEFORE painting.
    declare function PaintRectFactory( _
                byval rc as RECT ptr, _
                byval iStyle as long, _
                byval nPenWidth as long = 1, _
                byval nCurvature as long = 0 _
                ) as long
    declare function PaintRect( byval rc as RECT ptr ) as long
    declare function PaintBorderRect( _
                byval rc as RECT ptr, _
                byval nPenWidth as long = 1 _
                ) as long
    declare function PaintRoundRect( _
                byval rc as RECT ptr, _
                byval nCurvature as long = 20 _
                ) as long
    declare function PaintRoundBorderRect( _
                byval rc as RECT ptr, _
                byval nCurvature as long = 20, _
                byval nPenWidth as long = 1 _
                ) as long
    ' Stroke a rounded rect WITHOUT filling it. PaintRoundBorderRect always paints the
    ' interior, which is wrong for anything drawn over existing pixels -- a focus ring
    ' around an already-painted control being the case that needed it.
    declare function PaintRoundOutline( _
                byval rc as RECT ptr, _
                byval nCurvature as long = 20, _
                byval nPenWidth as long = 1 _
                ) as long
    ' Filled ellipse, optionally stroked. nPenWidth 0 = fill only.
    declare function PaintEllipse( _
                byval rc as RECT ptr, _
                byval nPenWidth as long = 0 _
                ) as long
    declare function PaintIconButton( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
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
                byval wsStyle as DWORD _
                ) as long
    ' PaintText with the LAYOUT FLAGS LEFT TO THE CALLER -- only DT_NOPREFIX is forced.
    ' PaintText above forces DT_VCENTER or DT_SINGLELINE, so it structurally cannot draw
    ' wrapped text; this is the way in for DT_WORDBREAK / DT_TOP / DT_CALCRECT. Additive,
    ' added for CMessageBox: nothing that used PaintText changed.
    declare function PaintTextEx( _
                byval wszText as DWSTRING, _
                byval rc as RECT ptr, _
                byval wsStyle as DWORD _
                ) as long
    declare function PaintChar( _
                byval wszChar as DWSTRING, _
                byval rc as RECT ptr, _
                byval forecolor as COLORREF _
                ) as long
    declare function SetFont( byval hFont as HFONT ) as long
    declare function SetForeColor( byval forecolor as COLORREF ) as long
    declare function SetBackColor( byval backcolor as COLORREF ) as long
    declare function SetColors( byval forecolor as COLORREF, byval backcolor as COLORREF ) as long
    declare function SetPenColor( byval pencolor as COLORREF ) as long
    ' --- Alpha-bearing variants. Additive: no control in the family calls these, so
    '     nothing changes appearance because they exist. ---
    declare function SetForeColorA( byval forecolor as COLORREF, byval nAlpha as ubyte ) as long
    declare function SetBackColorA( byval backcolor as COLORREF, byval nAlpha as ubyte ) as long
    declare function SetPenColorA( byval pencolor as COLORREF, byval nAlpha as ubyte ) as long
    declare function rcClient() as RECT
    declare function rcClientWidth() as long
    declare function rcClientHeight() as long
    declare function getMemDC() as HDC

end type
