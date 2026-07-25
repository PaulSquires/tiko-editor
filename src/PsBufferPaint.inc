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


#include once "PsBufferPaint.bi"

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


' ========================================================================================
' COLORREF is 0x00BBGGRR; GDI+ ARGB is 0xAARRGGBB. The byte order is REVERSED, not just
' shifted -- swapping R and B is the whole job, and getting it wrong yields a picture that
' still renders (so it looks like it works) with every colour subtly wrong.
' ========================================================================================
private function CBufToARGB( byval clr as COLORREF, byval nAlpha as ubyte ) as ARGB
    dim as ulong r = (clr        ) and &hFF
    dim as ulong g = (clr shr  8 ) and &hFF
    dim as ulong b = (clr shr 16 ) and &hFF
    return cast( ARGB, (culng(nAlpha) shl 24) or (r shl 16) or (g shl 8) or b )
end function


function PsBufferPaint.SetupBitmap() as long
    _memDC = CreateCompatibleDC( _hDC )
    SaveDC( _memDC )

    dim as long w = _rc.right - _rc.left
    dim as long h = _rc.bottom - _rc.top

    ' A 32bpp top-down DIB section rather than CreateCompatibleBitmap: GDI+ wants a
    ' KNOWN pixel format for antialiasing and alpha blending, where a compatible bitmap is
    ' whatever the device happens to be. On a 32bpp display this is still the device
    ' format, so the final BitBlt stays a straight copy.
    ' CreateDIBSection rejects a zero or negative extent, which BeginPaint can hand us for
    ' a collapsed window; clamp rather than leak a DC on the early return.
    if w < 1 then w = 1
    if h < 1 then h = 1
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = w
        .biHeight      = -h          ' negative = top-down, matching GDI's y-axis
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    dim as any ptr pBits
    _hbit = CreateDIBSection( _hDC, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )

    if _hbit then SelectObject( _memDC, _hbit )
    function = 0
end function

function PsBufferPaint.BeginDoubleBuffer( byval hwnd as HWND ) as long
    _UsePaint = true
    _hwnd = hwnd
    _hDC = BeginPaint( _hwnd, @_ps )
    GetClientRect( _hwnd, @_rc )
    this.SetupBitmap
    function = 0
end function

' Overload that provides an hDC rather than use BeginPaint/EndPaint
function PsBufferPaint.BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT ) as long
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
function PsBufferPaint.BeginDoubleBuffer( byval hwnd as HWND, byval hdc as HDC, byval rcItem as RECT, byval cachedMemDC as HDC ) as long
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

function PsBufferPaint.EndDoubleBuffer() as long
    ' Tear the GDI+ objects down BEFORE the blit. Deleting the Graphics flushes any
    ' batched drawing; blitting first would copy a surface that is still missing it.
    this.ReleaseGpObjects()
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
destructor PsBufferPaint()
    ' Same reasoning as the GDI handles below: End nulls these, so reaching them here
    ' means Begin/End was not balanced. Must still happen before the process calls
    ' GdiplusShutdown.
    this.ReleaseGpObjects()
    if _owns then
        if _hbit  then DeleteObject( _hbit ) : _hbit = 0
        if _memDC then DeleteDC( _memDC )    : _memDC = 0
    end if
    if _UsePaint andalso (_hwnd <> 0) then EndPaint( _hwnd, @_ps ) : _UsePaint = false
end destructor


' ========================================================================================
' GDI+ object lifetime. All three are lazy: a buffer that only ever paints text (several
' do) never constructs a Graphics at all.
' ========================================================================================
private function PsBufferPaint.EnsureGraphics() as CGpGraphics ptr
    if _pGraphics = 0 then
        if _memDC = 0 then return 0
        _pGraphics = new CGpGraphics( _memDC )
        if _pGraphics = 0 then return 0
        ' Integral coordinates land on whole pixels without this, but any half-pixel
        ' geometry (the inset on an even pen width) would straddle two.
        _pGraphics->SetPixelOffsetMode( PixelOffsetModeHalf )
    end if
    return _pGraphics
end function

private function PsBufferPaint.BrushFor( byval clr as ARGB ) as CGpBrush ptr
    if _pBrush = 0 then
        _pBrush = new CGpSolidBrush( clr )
        _brushARGB = clr
    elseif _brushARGB <> clr then
        _pBrush->SetColor( clr )
        _brushARGB = clr
    end if
    return cast( CGpBrush ptr, _pBrush )
end function

private function PsBufferPaint.PenFor( byval clr as ARGB, byval nWidth as single ) as CGpPen ptr
    if _pPen = 0 then
        _pPen = new CGpPen( clr, nWidth )
        _penARGB = clr
        _penWidthCached = nWidth
    else
        if _penARGB <> clr then
            _pPen->SetColor( clr )
            _penARGB = clr
        end if
        if _penWidthCached <> nWidth then
            _pPen->SetWidth( nWidth )
            _penWidthCached = nWidth
        end if
    end if
    return _pPen
end function

private sub PsBufferPaint.EnsureGdiReady()
    if _gpDirty andalso (_pGraphics <> 0) then
        _pGraphics->Flush( FlushIntentionSync )
        _gpDirty = false
    end if
end sub

private sub PsBufferPaint.ReleaseGpObjects()
    if _pPen      then delete _pPen      : _pPen = 0
    if _pBrush    then delete _pBrush    : _pBrush = 0
    if _pGraphics then delete _pGraphics : _pGraphics = 0
    _gpDirty = false
end sub

' ========================================================================================
' Build a rounded-rectangle path from four 90-degree arcs.
'
' radius is a TRUE RADIUS. Callers converting from GDI must halve first: RoundRect's
' curvature arguments are the ellipse WIDTH and HEIGHT, i.e. a diameter. Passing GDI's
' number straight through here makes every corner twice as round, which still looks
' deliberate and is why PaintRectFactory does the halving in one clearly-marked place.
' ========================================================================================
private sub PsBufferPaint.BuildRoundPath( _
            byval pPath as CGpGraphicsPath ptr, _
            byval x as single, _
            byval y as single, _
            byval w as single, _
            byval h as single, _
            byval radius as single _
            )
    pPath->Reset()

    ' GDI clamps the corner to half the box; match it, or a large curvature on a small
    ' icon button produces a degenerate path instead of a stadium.
    dim as single r = radius
    if r > (w / 2.0) then r = w / 2.0
    if r > (h / 2.0) then r = h / 2.0

    if r <= 0.0 then
        pPath->AddRectangle( x, y, w, h )
        exit sub
    end if

    dim as single d = r * 2.0
    pPath->AddArc( x,         y,         d, d, 180.0, 90.0 )   ' top-left
    pPath->AddArc( x + w - d, y,         d, d, 270.0, 90.0 )   ' top-right
    pPath->AddArc( x + w - d, y + h - d, d, d,   0.0, 90.0 )   ' bottom-right
    pPath->AddArc( x,         y + h - d, d, d,  90.0, 90.0 )   ' bottom-left
    pPath->CloseFigure()
end sub


function PsBufferPaint.rcClient() as RECT
    return _rc
end function

function PsBufferPaint.rcClientWidth() as long
    return _rc.right - _rc.left
end function

function PsBufferPaint.rcClientHeight() as long
    return _rc.bottom - _rc.top
end function

' The escape hatch to the raw surface. Asking for the HDC means the caller is about to do
' something with GDI -- read pixels back, blit, draw directly -- so the GDI+ batch is
' flushed first. Without this, anything drawn through the buffer's own methods may simply
' not be on the surface yet, and a caller that reads it sees a half-painted picture. That
' is not hypothetical: PsToggle's self-test scans the knob's rim through this exact path,
' BEFORE EndDoubleBuffer, and would otherwise be measuring blank background.
function PsBufferPaint.getMemDC() as HDC
    this.EnsureGdiReady()
    return _memDC
end function

' Store a caller-supplied font handle. The control/host owns the HFONT and is
' responsible for destroying it; the double buffer only selects it while painting.
function PsBufferPaint.SetFont( byval hFont as HFONT ) as long
    if hFont then _hFont = hFont
    function = 0
end function

function PsBufferPaint.SetForeColor( byval forecolor as COLORREF ) as long
    _forecolor = forecolor
    _forealpha = 255
    function = 0
end function

function PsBufferPaint.SetBackColor( byval backcolor as COLORREF ) as long
    _backcolor = backcolor
    _backalpha = 255
    function = 0
end function

function PsBufferPaint.SetColors( byval forecolor as COLORREF, byval backcolor as COLORREF ) as long
    _forecolor = forecolor
    _backcolor = backcolor
    _forealpha = 255
    _backalpha = 255
    function = 0
end function

function PsBufferPaint.SetPenColor( byval pencolor as COLORREF ) as long
    _pencolor = pencolor
    _penalpha = 255
    function = 0
end function

' --- Alpha-bearing variants. The plain setters above are these with nAlpha = 255, which
'     is why every existing call still means "opaque" without saying so. ---
function PsBufferPaint.SetForeColorA( byval forecolor as COLORREF, byval nAlpha as ubyte ) as long
    _forecolor = forecolor
    _forealpha = nAlpha
    function = 0
end function

function PsBufferPaint.SetBackColorA( byval backcolor as COLORREF, byval nAlpha as ubyte ) as long
    _backcolor = backcolor
    _backalpha = nAlpha
    function = 0
end function

function PsBufferPaint.SetPenColorA( byval pencolor as COLORREF, byval nAlpha as ubyte ) as long
    _pencolor = pencolor
    _penalpha = nAlpha
    function = 0
end function

' ========================================================================================
' The rounded/bordered rectangle workhorse.
'
' TWO PARAMETERS KEEP GDI'S VOCABULARY, on purpose -- every control in the family passes
' GDI-flavoured numbers and changing the contract would be a migration, not a rename:
'
'   iStyle      a pen style. PS_SOLID strokes a border, PS_NULL means fill only (that is
'               how PaintRoundRect asks for a borderless shape). nPenWidth = 0 has always
'               meant the same thing by a different route -- the old GDI implementation
'               forced the pen colour to the fill colour -- so both fold into one "no
'               stroke" decision here.
'   nCurvature  the ellipse DIAMETER, as RoundRect took it. BuildRoundPath wants a radius,
'               so the halving happens below, in one clearly-marked place.
' ========================================================================================
function PsBufferPaint.PaintRectFactory( _
            byval rc as RECT ptr, _
            byval iStyle as long, _
            byval nPenWidth as long = 1, _
            byval nCurvature as long = 0 _
            ) as long

    dim as CGpGraphics ptr g = this.EnsureGraphics()
    if g = 0 then return 0

    dim as boolean bNullPen  = (iStyle = PS_NULL)
    ' DPI-scale the pen, exactly as PaintLine already does. Without this the buffer was
    ' internally inconsistent: a rule drawn with PaintLine thickened with the display while
    ' a border or focus ring drawn here stayed at one physical pixel, so at 175% the two no
    ' longer matched. A no-op at 100%, where ScaleY(1) is 1.
    scope
        dim pWin as CWindow ptr = AfxCWindowPtr(_hwnd)
        if (pWin <> 0) andalso (nPenWidth > 0) then nPenWidth = pWin->ScaleY(nPenWidth)
        if nPenWidth < 1 then nPenWidth = 1
    end scope
    dim as boolean bZeroWide = (nPenWidth <= 0)
    dim as boolean bStroke   = (bNullPen = false) andalso (bZeroWide = false)

    ' GDI's RoundRect EXCLUDES right/bottom, so its outline sits on right-1 / bottom-1.
    ' GDI+ strokes THROUGH x+w. Hence the -1: without it every shape is a pixel wider
    ' and taller than the GDI build drew it.
    dim as single w = csng( rc->right - rc->left - 1 )
    dim as single h = csng( rc->bottom - rc->top - 1 )
    if (w < 0.0) orelse (h < 0.0) then return 0

    ' See BuildRoundPath: GDI's curvature is a diameter, a GDI+ arc takes a radius.
    dim as single radius = csng( nCurvature ) / 2.0

    ' Both APIs centre the stroke on the path, so half the pen hangs outside the shape.
    ' Inset by that half to keep it within rc -- in floats now, where the half-pixel the
    ' GDI path silently rounded away is actually visible.
    dim as single inset = 0.0
    dim as single fpw   = 0.0
    if bStroke then
        fpw   = csng( nPenWidth )
        inset = (fpw - 1.0) / 2.0
    end if

    ' Antialias curves; never antialias a plain rectangle. A square-cornered fill wants
    ' to land on exact pixel boundaries, and AA would fringe all four edges.
    dim as long oldSmooth = g->GetSmoothingMode()
    if nCurvature > 0 then
        g->SetSmoothingMode( SmoothingModeAntiAlias )
    else
        g->SetSmoothingMode( SmoothingModeNone )
    end if

    ' Fill and stroke need DIFFERENT paths, and the half pixel between them is not a
    ' rounding detail -- it was measured. A fill covers the pixels its path encloses,
    ' but a pen is CENTRED on its path, so a stroke path laid on the fill boundary
    ' straddles two pixel columns and resolves one to the left of where GDI put it
    ' (verified: GDI drew a 1px border at 10..39, GDI+ at 9..38). Offsetting the stroke
    ' path to the pixel CENTRE puts the pen back over the intended column.
    dim as CGpGraphicsPath fillPath
    this.BuildRoundPath( @fillPath, csng(rc->left), csng(rc->top), w, h, radius )
    g->FillPath( this.BrushFor( CBufToARGB( _backcolor, _backalpha ) ), @fillPath )

    if bStroke then
        dim as CGpGraphicsPath strokePath
        this.BuildRoundPath( @strokePath, _
                             csng(rc->left) + inset + 0.5, csng(rc->top) + inset + 0.5, _
                             w - (inset * 2.0), h - (inset * 2.0), _
                             radius )
        g->DrawPath( this.PenFor( CBufToARGB( _pencolor, _penalpha ), fpw ), @strokePath )
    end if

    g->SetSmoothingMode( oldSmooth )
    _gpDirty = true
    function = 0

end function

function PsBufferPaint.PaintRect( byval rc as RECT ptr ) as long
    dim as CGpGraphics ptr g = this.EnsureGraphics()
    if g = 0 then return 0

    ' ExtTextOut/ETO_OPAQUE fills [left,right) x [top,bottom). FillRectangle with
    ' smoothing off and integral coordinates covers exactly the same pixels, so the
    ' width is the plain difference here -- NOT the -1 that the stroked paths need.
    dim as single w = csng( rc->right - rc->left )
    dim as single h = csng( rc->bottom - rc->top )
    if (w <= 0.0) orelse (h <= 0.0) then return 0

    dim as long oldSmooth = g->GetSmoothingMode()
    g->SetSmoothingMode( SmoothingModeNone )
    g->FillRectangle( this.BrushFor( CBufToARGB( _backcolor, _backalpha ) ), _
                      csng(rc->left), csng(rc->top), w, h )
    g->SetSmoothingMode( oldSmooth )
    _gpDirty = true
    return 0
end function

function PsBufferPaint.PaintClientRect() as long
    return this.PaintRect( @_rc )
end function

function PsBufferPaint.PaintBorderRect( _
            byval rc as RECT ptr, _
            byval nPenWidth as long = 1 _
            ) as long
    return this.PaintRectFactory( rc, PS_SOLID, nPenWidth, 0 )
end function

function PsBufferPaint.PaintRoundRect( _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20 _
            ) as long
    return this.PaintRectFactory( rc, PS_NULL, 1, nCurvature )
end function

function PsBufferPaint.PaintRoundBorderRect( _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20, _
            byval nPenWidth as long = 1 _
            ) as long
    return this.PaintRectFactory( rc, PS_SOLID, nPenWidth, nCurvature )
end function

function PsBufferPaint.PaintText( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval wsStyle as DWORD _
            ) as long

    ' Text is GDI, geometry is GDI+, and GDI+ batches. Flush or the shapes this text sits
    ' on top of may not be on the surface yet.
    this.EnsureGdiReady()

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

' Same as PaintText, EXCEPT that the caller owns the layout flags. PaintText forces
' DT_VCENTER or DT_SINGLELINE, which makes it structurally unable to draw wrapped text --
' DT_WORDBREAK and DT_SINGLELINE cannot both hold. This variant adds ONLY DT_NOPREFIX, so
' the family's no-mnemonics rule stays enforced by the renderer (a caption is never allowed
' to eat an ampersand) while DT_WORDBREAK, DT_TOP and DT_CALCRECT become reachable.
'
' ADDITIVE: PaintText is untouched and no existing call site changes behaviour. Added for
' PsMessageBox, whose message body is multi-line by definition; anything drawing a single
' centred line should keep using PaintText, which cannot get those two flags wrong.
'
' MEASURE WITH THE SAME FLAGS YOU DRAW WITH. A caller that sizes a box with
' DT_CALCRECT or DT_WORDBREAK and then draws without DT_WORDBREAK gets one long clipped
' line inside a tall box, and nothing about that failure points back here.
function PsBufferPaint.PaintTextEx( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval wsStyle as DWORD _
            ) as long

    ' Text is GDI, geometry is GDI+, and GDI+ batches. Flush or the shapes this text sits
    ' on top of may not be on the surface yet.
    this.EnsureGdiReady()

    dim as COLORREF oldforecolor = SetTextColor( _memDC, _forecolor )
    SetBkMode(_memDC, TRANSPARENT)

    dim as HFONT hOldFont
    if _hFont then hOldFont = SelectObject( _memDC, _hFont )

    wsStyle = wsStyle or DT_NOPREFIX
    DrawText( _memDC, wszText, -1, cast(lpRect, rc), wsStyle )

    SetTextColor( _memDC, oldforecolor )
    if _hFont then SelectObject( _memDC, hOldFont )

    function = 0
end function

function PsBufferPaint.PaintChar( _
            byval wszChar as DWSTRING, _
            byval rc as RECT ptr, _
            byval forecolor as COLORREF _
            ) as long

    this.EnsureGdiReady()          ' see PaintText

    dim as HFONT hOldFont
    if _hFont then hOldFont = SelectObject( _memDC, _hFont )

    SetBkMode(_memDC, TRANSPARENT)
    SetTextColor(_memDC, forecolor)

    dim as DWORD wsStyle = DT_SINGLELINE or DT_VCENTER or DT_LEFT or DT_NOCLIP
    DrawText( _memDC, wszChar, -1, cast(lpRect, rc), wsStyle )

    if _hFont then SelectObject( _memDC, hOldFont )

    function = 0
end function

' ========================================================================================
' Stroke a rounded rect, leaving the interior untouched. Same geometry contract as
' PaintRectFactory -- see the fill/stroke path split there for why the stroke sits half a
' pixel inside the fill boundary.
' ========================================================================================
function PsBufferPaint.PaintRoundOutline( _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20, _
            byval nPenWidth as long = 1 _
            ) as long
    if nPenWidth <= 0 then return 0
    ' DPI-scale the pen, exactly as PaintLine already does. Without this the buffer was
    ' internally inconsistent: a rule drawn with PaintLine thickened with the display while
    ' a border or focus ring drawn here stayed at one physical pixel, so at 175% the two no
    ' longer matched. A no-op at 100%, where ScaleY(1) is 1.
    scope
        dim pWin as CWindow ptr = AfxCWindowPtr(_hwnd)
        if (pWin <> 0) andalso (nPenWidth > 0) then nPenWidth = pWin->ScaleY(nPenWidth)
        if nPenWidth < 1 then nPenWidth = 1
    end scope

    dim as CGpGraphics ptr g = this.EnsureGraphics()
    if g = 0 then return 0

    dim as single w = csng( rc->right - rc->left - 1 )
    dim as single h = csng( rc->bottom - rc->top - 1 )
    if (w < 0.0) orelse (h < 0.0) then return 0

    dim as single fpw   = csng( nPenWidth )
    dim as single inset = (fpw - 1.0) / 2.0

    dim as long oldSmooth = g->GetSmoothingMode()
    if nCurvature > 0 then
        g->SetSmoothingMode( SmoothingModeAntiAlias )
    else
        g->SetSmoothingMode( SmoothingModeNone )
    end if

    dim as CGpGraphicsPath strokePath
    this.BuildRoundPath( @strokePath, _
                         csng(rc->left) + inset + 0.5, csng(rc->top) + inset + 0.5, _
                         w - (inset * 2.0), h - (inset * 2.0), _
                         csng(nCurvature) / 2.0 )
    g->DrawPath( this.PenFor( CBufToARGB( _pencolor, _penalpha ), fpw ), @strokePath )

    g->SetSmoothingMode( oldSmooth )
    _gpDirty = true
    function = 0
end function

' ========================================================================================
' Fill a rect -- square or rounded -- with a two-stop linear gradient. FILL ONLY.
'
' Geometry follows PaintRectFactory EXACTLY (the -1 on both extents, radius = nCurvature/2,
' AA iff there is a curve), so a gradient bar and a PaintRoundRect track drawn on the same
' RECT cover the same pixels. Do NOT be tempted to use PaintRect's full-extent convention
' here: the two differ by one pixel and mixing them is the obvious thing to get wrong.
'
' Added for PsProgressBar. Purely additive -- no existing method was touched -- which is
' what makes the vendored copies safe to re-sync mechanically (PaintTextEx's precedent).
' ========================================================================================
function PsBufferPaint.PaintGradientRect( _
            byval rc as RECT ptr, _
            byval clr1 as COLORREF, _
            byval clr2 as COLORREF, _
            byval nMode as long = LinearGradientModeVertical, _
            byval nCurvature as long = 0, _
            byval rcBrush as RECT ptr = 0 _
            ) as long

    dim as CGpGraphics ptr g = this.EnsureGraphics()
    if g = 0 then return 0

    dim as single w = csng( rc->right - rc->left - 1 )
    dim as single h = csng( rc->bottom - rc->top - 1 )
    if (w < 0.0) orelse (h < 0.0) then return 0

    ' The ramp is measured across rcBrush when one is supplied, so a caller can fill a
    ' fragment (one block of a segmented bar) with the slice of a gradient that spans the
    ' whole run. Degenerate or absent -> measure across rc itself.
    dim as RECT rcRamp = *rc
    if rcBrush <> 0 then
        if (rcBrush->right > rcBrush->left) andalso (rcBrush->bottom > rcBrush->top) then
            rcRamp = *rcBrush
        end if
    end if

    ' GDI+ wraps a linear gradient at its brush rect, and a brush sized EXACTLY to the fill
    ' shows a hard seam on the last row/column where the wrap lands. One pixel of slack on
    ' every side moves the wrap off the visible area. WrapModeTileFlipXY rather than plain
    ' tile, so even that slack blends instead of snapping back to clr1.
    dim as GpRectF rcf
    rcf.x = csng(rcRamp.left) - 1.0
    rcf.y = csng(rcRamp.top) - 1.0
    rcf.Width  = csng(rcRamp.right  - rcRamp.left) + 2.0
    rcf.Height = csng(rcRamp.bottom - rcRamp.top ) + 2.0
    if (rcf.Width <= 0.0) orelse (rcf.Height <= 0.0) then return 0

    dim as long oldSmooth = g->GetSmoothingMode()
    if nCurvature > 0 then
        g->SetSmoothingMode( SmoothingModeAntiAlias )
    else
        g->SetSmoothingMode( SmoothingModeNone )
    end if

    scope
        ' Built per call and dropped at end scope. It cannot join the _pBrush cache: that is
        ' typed CGpSolidBrush ptr and keyed on one ARGB, while this is keyed on a rect, two
        ' colours and a direction.
        dim as CGpLinearGradientBrush br = CGpLinearGradientBrush( @rcf, _
                                                CBufToARGB( clr1, _backalpha ), _
                                                CBufToARGB( clr2, _backalpha ), _
                                                cast( LinearGradientMode, nMode ) )
        br.SetWrapMode( WrapModeTileFlipXY )

        dim as CGpGraphicsPath fillPath
        this.BuildRoundPath( @fillPath, csng(rc->left), csng(rc->top), w, h, _
                             csng(nCurvature) / 2.0 )
        ' FillPath takes the BASE CGpBrush ptr and CGpLinearGradientBrush extends it, so this
        ' is the same call site every other fill in this file uses.
        g->FillPath( cast( CGpBrush ptr, @br ), @fillPath )
    end scope

    g->SetSmoothingMode( oldSmooth )
    _gpDirty = true
    function = 0
end function

' ========================================================================================
' Filled ellipse; strokes the rim too when nPenWidth > 0. The rim is antialiased, which is
' the whole reason this exists as a primitive rather than being left to each control to
' hand-roll -- PsToggle spent a version doing exactly that, with a 4x supersampled tile.
' ========================================================================================
function PsBufferPaint.PaintEllipse( _
            byval rc as RECT ptr, _
            byval nPenWidth as long = 0 _
            ) as long
    dim as CGpGraphics ptr g = this.EnsureGraphics()
    if g = 0 then return 0

    ' MEASURED, and it contradicts the obvious reading: GDI's Ellipse(10,5,30,25) does not
    ' fill through x=29. It stops at 28, exactly as RoundRect stops a row short of its own
    ' rect -- so the ellipse takes the same -1 the rounded rect does, NOT the full extent
    ' that PaintRect uses. Asserting the "obvious" full extent here made GDI+ diverge from
    ' GDI by a pixel, and only the GDI backend running the same assertions caught it.
    dim as single w = csng( rc->right - rc->left - 1 )
    dim as single h = csng( rc->bottom - rc->top - 1 )
    if (w < 0.0) orelse (h < 0.0) then return 0

    dim as long oldSmooth = g->GetSmoothingMode()
    g->SetSmoothingMode( SmoothingModeAntiAlias )

    g->FillEllipse( this.BrushFor( CBufToARGB( _backcolor, _backalpha ) ), _
                    csng(rc->left), csng(rc->top), w, h )

    if nPenWidth > 0 then
        dim as single fpw   = csng( nPenWidth )
        dim as single inset = (fpw - 1.0) / 2.0
        g->DrawEllipse( this.PenFor( CBufToARGB( _pencolor, _penalpha ), fpw ), _
                        csng(rc->left) + inset + 0.5, csng(rc->top) + inset + 0.5, _
                        w - (inset * 2.0), h - (inset * 2.0) )
    end if

    g->SetSmoothingMode( oldSmooth )
    _gpDirty = true
    function = 0
end function

function PsBufferPaint.PaintIconButton( _
            byval wszText as DWSTRING, _
            byval rc as RECT ptr, _
            byval nCurvature as long = 20 _
            ) as long
    this.PaintRoundRect( rc, nCurvature )
    this.PaintText( wszText, rc, DT_CENTER )

    function = 0
end function

' ========================================================================================
' A straight line, nWidth units thick (DPI-scaled here, as it always has been).
'
' AN AXIS-ALIGNED RULE IS PAINTED AS A FILLED RECTANGLE, NOT AS A LINE. Every caller in
' the family draws one -- a divider or a separator -- and two reasons, both learned the
' expensive way, say a rectangle is the right primitive:
'   * A 1px horizontal rule drawn with DrawLine and antialiasing on comes out grey and
'     soft, because the pen straddles two pixel rows.
'   * GDI's LineTo EXCLUDES its endpoint while GDI+ DrawLine INCLUDES it, so translating
'     one to the other lengthens every divider by a pixel.
' A rectangle has neither problem: the extent is stated, not inferred.
'
' The diagonal branch is currently unreached (no caller draws one) but is the case that
' actually wants antialiasing, so it is implemented rather than left to fail silently.
'
' One inherited quirk is deliberately NOT reproduced: with a pen thicker than 1px, GDI's
' LineTo over-ran its endpoint (a 2px rule from x=10 to x=40 painted 10..40 inclusive,
' where the 1px rule painted 10..39). That was a thick-pen end-cap artifact, not intent.
' Here a 2px rule ends where a 1px rule does.
' ========================================================================================
function PsBufferPaint.PaintLine( _
            byval nWidth as long, _
            byval nLeft as long, _
            byval nTop as long, _
            byval nRight as long, _
            byval nBottom as long _
            ) as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(_hwnd)
    if pWindow = 0 then exit function
    dim as long penWidth = pWindow->ScaleY(nWidth)
    if penWidth < 1 then penWidth = 1

    dim as CGpGraphics ptr g = this.EnsureGraphics()
    if g = 0 then return 0

    dim as long oldSmooth = g->GetSmoothingMode()

    if nTop = nBottom then
        ' Horizontal. GDI centres the pen on the path, so replicate that offset exactly.
        dim as long y0 = nTop - (penWidth \ 2)
        g->SetSmoothingMode( SmoothingModeNone )
        g->FillRectangle( this.BrushFor( CBufToARGB( _pencolor, _penalpha ) ), _
                          csng(nLeft), csng(y0), csng(nRight - nLeft), csng(penWidth) )
    elseif nLeft = nRight then
        ' Vertical.
        dim as long x0 = nLeft - (penWidth \ 2)
        g->SetSmoothingMode( SmoothingModeNone )
        g->FillRectangle( this.BrushFor( CBufToARGB( _pencolor, _penalpha ) ), _
                          csng(x0), csng(nTop), csng(penWidth), csng(nBottom - nTop) )
    else
        ' Diagonal: this is the one that genuinely benefits from antialiasing.
        g->SetSmoothingMode( SmoothingModeAntiAlias )
        g->DrawLine( this.PenFor( CBufToARGB( _pencolor, _penalpha ), csng(penWidth) ), _
                     csng(nLeft), csng(nTop), csng(nRight), csng(nBottom) )
    end if

    g->SetSmoothingMode( oldSmooth )
    _gpDirty = true
    function = 0

end function


' ========================================================================================
' Geometry self-test, run with PSBUFFERPAINT_SELFTEST=1.
'
' Renders through the real public methods into an offscreen surface and reads the pixels
' back, so it asserts the rendering rather than the arithmetic behind it.
'
' WHERE THE EXPECTED NUMBERS CAME FROM, and why you should not "simplify" them: every one
' was MEASURED off the GDI implementation this class used to carry alongside the GDI+ one,
' not derived from what the documentation implies GDI does. The two disagreed twice while
' the GDI+ path was being written -- a stroke landing one column left, and RoundRect's
' fill stopping a row short of its own rect -- and each time the measurement was right.
'
' That second implementation is gone now, so these assertions no longer have an
' independent oracle behind them; they are a regression net, not a proof. A number here
' that looks off by one is far more likely to be the API's real behaviour than a typo.
'
' Reports assertions RUN as well as failed: a harness that only counts failures cannot
' tell "all passed" from "barely executed" (Learnings.md).
' ========================================================================================
namespace PsBufferPaintSelfTest
    dim shared as long gRun, gFail
    dim shared as HDC     gDC
    dim shared as HBITMAP gBmp

    const SW = 64
    const SH = 40
    const CBACK = BGR( 0, 0, 0 )
    const CFILL = BGR( 200, 100, 50 )
    const CPEN  = BGR( 10, 220, 240 )

    private sub Chk( byval sName as string, byval bOk as boolean, byval sDetail as string )
        gRun += 1
        dim as string s
        if bOk then s = "PASS  " else s = "FAIL  " : gFail += 1
        s &= sName & "   [" & sDetail & "]"
        print s
    end sub

    private sub ClearSurf()
        dim as RECT rc = type( 0, 0, SW, SH )
        dim as HBRUSH hbr = CreateSolidBrush( CBACK )
        FillRect( gDC, @rc, hbr )
        DeleteObject( hbr )
    end sub

    ' First and last non-background pixel across one row.
    private sub RowSpan( byval y as long, byref firstx as long, byref lastx as long )
        firstx = -1 : lastx = -1
        for x as long = 0 to SW - 1
            if GetPixel( gDC, x, y ) <> CBACK then
                if firstx = -1 then firstx = x
                lastx = x
            end if
        next
    end sub

    ' First and last non-background pixel down one column.
    private sub ColSpan( byval x as long, byref firsty as long, byref lasty as long )
        firsty = -1 : lasty = -1
        for y as long = 0 to SH - 1
            if GetPixel( gDC, x, y ) <> CBACK then
                if firsty = -1 then firsty = y
                lasty = y
            end if
        next
    end sub

    ' Distinct colours in a region. Exactly 2 means hard edges; more means antialiasing
    ' actually blended, rather than merely being switched on.
    private function ToneCount( byval x0 as long, byval y0 as long, byval x1 as long, byval y1 as long ) as long
        dim as ulong seen(0 to 255)
        dim as long n = 0
        for y as long = y0 to y1
            for x as long = x0 to x1
                dim as ulong p = GetPixel( gDC, x, y )
                dim as boolean found = false
                for i as long = 0 to n - 1
                    if seen(i) = p then found = true : exit for
                next
                if (found = false) andalso (n < 256) then seen(n) = p : n += 1
            next
        next
        return n
    end function
end namespace

sub PsBufferPaint_RunSelfTest( byval hwnd as HWND )
    using PsBufferPaintSelfTest

    print ""
    print "=== PsBufferPaint geometry self-test ==="

    gRun = 0 : gFail = 0

    ' A CWindow must sit behind the HWND: PaintLine asks it for the DPI scale.
    if AfxCWindowPtr( hwnd ) = 0 then
        print "FAIL  precondition: hwnd has no CWindow   [PaintLine cannot run]"
        print "=== 0 of 1 assertions passed ==="
        exit sub
    end if
    ' The same CWindow PaintLine scales through, so the pen-thickness assertions can derive
    ' their expected pixel spans exactly the way the code does -- ScaleY() stored in a LONG,
    ' then the pen centred at pos - (penWidth \ 2) -- rather than hardcoding 96-DPI pixels
    ' that fail on any DPI-aware process at a non-96 scale (see Learnings.md).
    dim as CWindow ptr pWin = AfxCWindowPtr( hwnd )

    dim as HDC hScreen = GetDC( null )
    gDC  = CreateCompatibleDC( hScreen )
    gBmp = CreateCompatibleBitmap( hScreen, SW, SH )
    SelectObject( gDC, gBmp )
    ReleaseDC( null, hScreen )

    dim as RECT rcAll = type( 0, 0, SW, SH )
    dim as long a, b2

    ' --- PaintRect: fills [left,right) x [top,bottom), same as ExtTextOut/ETO_OPAQUE ----
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 40, 20 )
        bb.SetBackColor( CFILL ) : bb.PaintRect( @r )
        bb.EndDoubleBuffer()
    end scope
    RowSpan( 10, a, b2 )
    Chk( "PaintRect: right edge exclusive", (a = 10) andalso (b2 = 39), "x " & a & ".." & b2 )
    ColSpan( 20, a, b2 )
    Chk( "PaintRect: bottom edge exclusive", (a = 5) andalso (b2 = 19), "y " & a & ".." & b2 )
    Chk( "PaintRect: colour survives BGR->ARGB", (GetPixel( gDC, 20, 10 ) = CFILL), _
         "want " & hex(CFILL,6) & " got " & hex(GetPixel(gDC,20,10),6) )

    ' --- PaintBorderRect: a 1px stroke sits ON the rect edge, not half a pixel off. -----
    '     This is the one that was wrong first time round.
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 40, 20 )
        bb.SetBackColor( CFILL ) : bb.SetPenColor( CPEN )
        bb.PaintBorderRect( @r, 1 )
        bb.EndDoubleBuffer()
    end scope
    RowSpan( 5, a, b2 )
    Chk( "Border: top edge on the rect, x span", (a = 10) andalso (b2 = 39), "x " & a & ".." & b2 )
    ColSpan( 20, a, b2 )
    Chk( "Border: y span on the rect", (a = 5) andalso (b2 = 19), "y " & a & ".." & b2 )
    Chk( "Border: stroke uses the pen colour", (GetPixel( gDC, 20, 5 ) = CPEN), _
         "want " & hex(CPEN,6) & " got " & hex(GetPixel(gDC,20,5),6) )

    ' --- PaintRoundRect, curvature 20. THE trap: GDI's curvature is an ellipse DIAMETER
    '     and a GDI+ arc takes a RADIUS. Halving wrongly doubles every corner, which
    '     still looks deliberate -- so the corner is measured, not eyeballed. -----------
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 50, 35 )      ' 40 x 30
        bb.SetBackColor( CFILL ) : bb.PaintRoundRect( @r, 20 )
        bb.EndDoubleBuffer()
    end scope
    ' Vertical mid-line: RoundRect (unlike PaintRect) stops a row short of its own
    ' bottom -- GDI does this too, and the GDI+ path matches it deliberately.
    ColSpan( 30, a, b2 )
    Chk( "RoundRect: vertical extent matches GDI", (a = 5) andalso (b2 = 33), "y " & a & ".." & b2 )
    ' Mid-height row must reach the full left/right edges (the straight part).
    RowSpan( 20, a, b2 )
    Chk( "RoundRect: mid-row spans full width", (a = 10) andalso (b2 = 48), "x " & a & ".." & b2 )
    ' With radius 10 the corner has bitten well into the top row; with radius 20
    ' (the un-halved bug) the shape degenerates and this row starts much further right.
    RowSpan( 6, a, b2 )
    Chk( "RoundRect: corner radius ~10 not ~20", (a >= 11) andalso (a <= 17), "row6 starts x=" & a )

    Chk( "RoundRect: corner is antialiased", (ToneCount( 8, 3, 24, 19 ) > 2), _
         "distinct tones = " & ToneCount( 8, 3, 24, 19 ) )

    ' --- PaintLine: axis-aligned rules must stay hard-edged and keep GDI's exclusive
    '     endpoint. An antialiased 1px divider goes grey, which reads as a theme bug. --
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        bb.SetPenColor( CPEN ) : bb.PaintLine( 1, 10, 8, 40, 8 )
        bb.EndDoubleBuffer()
    end scope
    RowSpan( 8, a, b2 )
    Chk( "HLine w1: endpoint exclusive", (a = 10) andalso (b2 = 39), "x " & a & ".." & b2 )
    ColSpan( 20, a, b2 )
    scope
        dim as long pw = pWin->ScaleY(1) : if pw < 1 then pw = 1
        dim as long e1 = 8 - (pw \ 2) : dim as long e2 = e1 + pw - 1
        Chk( "HLine w1: exactly the scaled pen thickness, no AA bleed", _
             (a = e1) andalso (b2 = e2), "y " & a & ".." & b2 & "  expect " & e1 & ".." & e2 )
    end scope
    Chk( "HLine w1: NOT antialiased", (ToneCount( 8, 6, 42, 10 ) = 2), _
         "distinct tones = " & ToneCount( 8, 6, 42, 10 ) )

    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        bb.SetPenColor( CPEN ) : bb.PaintLine( 2, 10, 8, 40, 8 )
        bb.EndDoubleBuffer()
    end scope
    ColSpan( 20, a, b2 )
    scope
        dim as long pw = pWin->ScaleY(2) : if pw < 1 then pw = 1
        dim as long e1 = 8 - (pw \ 2) : dim as long e2 = e1 + pw - 1
        Chk( "HLine w2: pen centred like GDI", _
             (a = e1) andalso (b2 = e2), "y " & a & ".." & b2 & "  expect " & e1 & ".." & e2 )
    end scope

    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        bb.SetPenColor( CPEN ) : bb.PaintLine( 1, 20, 5, 20, 30 )
        bb.EndDoubleBuffer()
    end scope
    ColSpan( 20, a, b2 )
    Chk( "VLine w1: endpoint exclusive", (a = 5) andalso (b2 = 29), "y " & a & ".." & b2 )
    RowSpan( 10, a, b2 )
    scope
        dim as long pw = pWin->ScaleY(1) : if pw < 1 then pw = 1
        dim as long e1 = 20 - (pw \ 2) : dim as long e2 = e1 + pw - 1
        Chk( "VLine w1: exactly the scaled pen thickness, no AA bleed", _
             (a = e1) andalso (b2 = e2), "x " & a & ".." & b2 & "  expect " & e1 & ".." & e2 )
    end scope

    ' --- Interleave: GDI+ batches, GDI does not. A shape drawn through GDI+ must still
    '     be on the surface after a GDI text call follows it. Without the flush in
    '     PaintText this is exactly what goes missing, intermittently. ----------------
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 40, 20 )
        bb.SetBackColor( CFILL ) : bb.PaintRect( @r )
        dim as RECT rt = type( 12, 6, 38, 18 )
        bb.SetForeColor( CPEN ) : bb.PaintText( "x", @rt, DT_LEFT )
        bb.EndDoubleBuffer()
    end scope
    RowSpan( 10, a, b2 )
    Chk( "GDI+/GDI interleave: fill survives a text call", (a = 10) andalso (b2 = 39), _
         "x " & a & ".." & b2 )

    ' --- PaintEllipse: the knob primitive. Must be round, centred, and antialiased. -----
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 30, 25 )        ' 20 x 20 circle
        bb.SetBackColor( CFILL ) : bb.PaintEllipse( @r, 0 )
        bb.EndDoubleBuffer()
    end scope
    ' 10..28 and 5..23, NOT 10..29 / 5..24: GDI's Ellipse stops one short of its own rect,
    ' the same way RoundRect does. These numbers came off the GDI backend.
    RowSpan( 15, a, b2 )         ' widest row, through the centre
    Chk( "Ellipse: widest row matches GDI", (a = 10) andalso (b2 = 28), "x " & a & ".." & b2 )
    ColSpan( 20, a, b2 )
    Chk( "Ellipse: tallest column matches GDI", (a = 5) andalso (b2 = 23), "y " & a & ".." & b2 )
    ' A circle must be narrower near its top than at its middle -- that it is an ellipse
    ' at all, rather than the bounding box getting filled.
    dim as long topFirst, topLast
    RowSpan( 7, topFirst, topLast )
    Chk( "Ellipse: is round, not a filled box", (topLast - topFirst) < 18, _
         "row7 width = " & (topLast - topFirst + 1) & " vs mid 20" )
    Chk( "Ellipse: rim is antialiased", (ToneCount( 8, 3, 32, 27 ) > 2), _
         "distinct tones = " & ToneCount( 8, 3, 32, 27 ) )

    ' --- PaintRoundOutline: strokes WITHOUT filling. The interior must survive. ---------
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 50, 30 )
        bb.SetBackColor( CFILL ) : bb.PaintRect( @r )      ' something underneath
        bb.SetPenColor( CPEN )   : bb.PaintRoundOutline( @r, 10, 1 )
        bb.EndDoubleBuffer()
    end scope
    Chk( "RoundOutline: interior NOT overpainted", (GetPixel( gDC, 30, 17 ) = CFILL), _
         "centre want " & hex(CFILL,6) & " got " & hex(GetPixel(gDC,30,17),6) )
    Chk( "RoundOutline: edge carries the pen colour", (GetPixel( gDC, 30, 5 ) = CPEN), _
         "top edge want " & hex(CPEN,6) & " got " & hex(GetPixel(gDC,30,5),6) )

    ' --- PaintGradientRect: fills, never strokes, and the ramp must actually ramp. The
    '     endpoint colours are deliberately NOT asserted exactly: the brush rect is inflated
    '     by a pixel on every side (to move GDI+'s wrap seam off the visible area), so the
    '     first painted column sits a fraction of the way along the ramp rather than at 0.
    '     What is asserted is what the primitive is FOR -- the two ends differ, and the
    '     transition between them is smooth rather than a hard step. ----------------------
    ClearSurf()
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
        bb.SetBackColor( CBACK ) : bb.PaintClientRect()
        dim as RECT r = type( 10, 5, 50, 30 )
        bb.PaintGradientRect( @r, CFILL, CPEN, LinearGradientModeHorizontal, 0 )
        bb.EndDoubleBuffer()
    end scope
    scope
        dim as ulong pL = GetPixel( gDC, 12, 17 )
        dim as ulong pR = GetPixel( gDC, 47, 17 )
        Chk( "Gradient: the two ends differ", (pL <> pR), _
             "left " & hex(pL,6) & " right " & hex(pR,6) )
        Chk( "Gradient: fill reached the surface", (pL <> CBACK) andalso (pR <> CBACK), _
             "left " & hex(pL,6) & " right " & hex(pR,6) & " back " & hex(CBACK,6) )
        ' A hard two-colour step would score 2. A real ramp scores many more.
        Chk( "Gradient: ramp is smooth, not a step", (ToneCount( 11, 17, 48, 17 ) > 8), _
             "distinct tones across the row = " & ToneCount( 11, 17, 48, 17 ) )
    end scope
    ' Square corners take PaintRectFactory's -1 extent, NOT PaintRect's full extent. Mixing
    ' the two conventions is the obvious mistake here, so it is measured rather than trusted.
    scope
        dim as long a, b2
        RowSpan( 17, a, b2 )
        ' 10..48, MEASURED -- and identical to what "RoundRect: mid-row spans full width"
        ' scores on the very same RECT above. That agreement is the assertion; the absolute
        ' number is GDI+'s, not ours (w = right-left-1 = 39, filled from x=10, so [10,49)).
        Chk( "Gradient: square extent matches PaintRectFactory (-1)", (a = 10) andalso (b2 = 48), _
             "row span " & a & ".." & b2 & " want 10..48" )
    end scope

    ' --- rcBrush: measure the ramp across a DIFFERENT rect than the one being filled. This
    '     is what lets a segmented bar's blocks read as slices of one gradient instead of N
    '     identical chips, so it is the parameter a caller is most likely to depend on. ----
    scope
        dim as RECT rBlock = type( 40, 5, 50, 30 )    ' a block near the right end
        dim as RECT rTrack = type( 10, 5, 50, 30 )    ' the run it belongs to
        dim as ulong pOwn, pSliced

        ClearSurf()
        scope
            dim bb as PsBufferPaint
            bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
            bb.SetBackColor( CBACK ) : bb.PaintClientRect()
            bb.PaintGradientRect( @rBlock, CFILL, CPEN, LinearGradientModeHorizontal, 0 )
            bb.EndDoubleBuffer()
        end scope
        pOwn = GetPixel( gDC, 42, 17 )

        ClearSurf()
        scope
            dim bb as PsBufferPaint
            bb.BeginDoubleBuffer( hwnd, gDC, rcAll )
            bb.SetBackColor( CBACK ) : bb.PaintClientRect()
            bb.PaintGradientRect( @rBlock, CFILL, CPEN, LinearGradientModeHorizontal, 0, @rTrack )
            bb.EndDoubleBuffer()
        end scope
        pSliced = GetPixel( gDC, 42, 17 )

        Chk( "Gradient: rcBrush moves the ramp off the fill rect", (pOwn <> pSliced), _
             "own-ramp " & hex(pOwn,6) & " sliced " & hex(pSliced,6) )
        Chk( "Gradient: rcBrush still fills the SAME rect", _
             (GetPixel( gDC, 39, 17 ) = CBACK) andalso (pSliced <> CBACK), _
             "x=39 (outside) " & hex(GetPixel(gDC,39,17),6) & " x=42 (inside) " & hex(pSliced,6) )
    end scope

    ' --- The CACHED overload: a caller-owned memDC, reused per row by PsListBox. It is
    '     the one Begin path that skips SetupBitmap, so it never gets the 32bpp DIB the
    '     others do -- it draws onto whatever surface the host already made. Every list
    '     in the app goes through here once per visible row, so it is asserted rather
    '     than assumed to behave like its siblings. --------------------------------------
    ClearSurf()
    scope
        dim as HDC hScr2 = GetDC( null )
        dim as HDC cacheDC = CreateCompatibleDC( hScr2 )
        dim as HBITMAP cacheBmp = CreateCompatibleBitmap( hScr2, SW, SH )
        dim as HBITMAP oldBmp = SelectObject( cacheDC, cacheBmp )
        ReleaseDC( null, hScr2 )

        dim as RECT rRow = type( 0, 0, SW, 12 )
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( hwnd, gDC, rRow, cacheDC )
        bb.SetBackColor( CFILL )
        bb.PaintRect( @rRow )
        dim as RECT rIn = type( 4, 2, 30, 10 )
        bb.SetBackColor( CPEN )
        bb.PaintRoundRect( @rIn, 6 )
        bb.EndDoubleBuffer()

        SelectObject( cacheDC, oldBmp )
        DeleteObject( cacheBmp )
        DeleteDC( cacheDC )
    end scope
    RowSpan( 6, a, b2 )
    Chk( "cached DC: row blits across full width", (a = 0) andalso (b2 = SW - 1), _
         "x " & a & ".." & b2 )
    Chk( "cached DC: inner shape reached the surface", (GetPixel( gDC, 15, 6 ) = CPEN), _
         "want " & hex(CPEN,6) & " got " & hex(GetPixel(gDC,15,6),6) )
    ' Nothing below the row rect may be touched: the blit is bounded by rcItem, not by
    ' the (larger) cached bitmap behind it.
    ColSpan( 15, a, b2 )
    Chk( "cached DC: blit bounded by rcItem", (b2 <= 11), "last row touched = " & b2 )

    DeleteObject( gBmp )
    DeleteDC( gDC )

    dim as string sSummary = "=== " & (gRun - gFail) & " of " & gRun & " assertions passed"
    if gFail > 0 then sSummary &= "  (" & gFail & " FAILED)"
    sSummary &= " ==="
    print sSummary
end sub
