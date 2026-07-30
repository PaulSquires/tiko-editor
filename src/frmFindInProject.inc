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

#include once "frmFindInProject.bi"


' ========================================================================================
' Surface state. The scroll position is a PIXEL OFFSET into the virtual canvas; the row list
' is rebuilt whenever the model or the metrics change, never per paint.
' ========================================================================================
dim shared gFipRows(any)   as FIP_ROW
dim shared gFipRowCount    as long
dim shared gFipCanvasH     as long
dim shared gFipScrollTop   as long
dim shared gFipMetrics     as FIP_METRICS
dim shared as HWND HWND_FRMFINDINPROJECT_SURFACE, HWND_FRMFINDINPROJECT_VSCROLL


' ========================================================================================
' PURE geometry. No window, no document.
' ========================================================================================

function FipRows_BlockHeight( byval nLineCount as long, byref m as FIP_METRICS ) as long
    dim as long n = nLineCount
    if n < 1 then n = 1
    function = (n * m.nLineHeight) + (m.nBlockPad * 2)
end function


function FipRows_Build( rows() as FIP_ROW, byref nRowCount as long, _
                        byref m as FIP_METRICS ) as long
    nRowCount = 0
    dim as long y = 0

    ' Worst case is one header per group plus every block, so size once rather than growing.
    dim as long nMax = gFip.nGroupCount + gFip.nBlockCount
    if nMax < 1 then
        erase rows
        return 0
    end if
    redim rows(0 to nMax - 1)

    for g as long = 0 to gFip.nGroupCount - 1
        with rows(nRowCount)
            .nKind   = FIP_ROW_HEADER
            .nGroup  = g
            .nBlock  = -1
            .nTop    = y
            .nHeight = m.nHeaderHeight
        end with
        y += m.nHeaderHeight
        nRowCount += 1

        ' A collapsed group contributes its header and nothing else -- which is what makes
        ' collapsing cheap: the canvas simply gets shorter and every row below moves up.
        if gFip.groups(g).bCollapsed = false then
            dim as long b0 = gFip.groups(g).nBlockFirst
            for b as long = b0 to b0 + gFip.groups(g).nBlockCount - 1
                with rows(nRowCount)
                    .nKind   = FIP_ROW_BLOCK
                    .nGroup  = g
                    .nBlock  = b
                    .nTop    = y
                    .nHeight = FipRows_BlockHeight( gFip.blocks(b).nLineCount, m )
                end with
                y += rows(nRowCount).nHeight + m.nBlockGap
                nRowCount += 1
            next
        end if
    next

    function = y
end function


function FipRows_FirstVisible( rows() as FIP_ROW, byval nRowCount as long, _
                               byval nTop as long ) as long
    if nRowCount <= 0 then return -1
    ' Rows ascend by nTop and tile the canvas without gaps other than the inter-block gap, so
    ' the first row whose BOTTOM edge is past nTop is the first one on screen.
    ' (nMid, not mid -- MID is FreeBASIC's string function and cannot be a variable name.)
    dim as long lo = 0, hi = nRowCount - 1, nAns = -1
    do while lo <= hi
        dim as long nMid = lo + (hi - lo) \ 2
        if (rows(nMid).nTop + rows(nMid).nHeight) > nTop then
            nAns = nMid
            hi = nMid - 1
        else
            lo = nMid + 1
        end if
    loop
    function = nAns
end function


' ========================================================================================
' One document line's height, asked of Scintilla rather than guessed, so the excerpt views
' that replace this painted placeholder line up with the geometry already built for them.
' Falls back to a scaled default when nothing is realized yet.
' ========================================================================================
private function FipMetrics_LineHeight() as long
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if IsWindow( pDoc->hWindow(0) ) then
            dim as long n = SciExec( pDoc->hWindow(0), SCI_TEXTHEIGHT, 0, 0 )
            if n > 0 then return n
        end if
        pDoc = pDoc->pDocNext
    loop
    ' Nothing realized: a plausible scaled default. Any later Refresh picks up the real value.
    function = AfxScaleY(16)
end function


private sub FipMetrics_Measure( byref m as FIP_METRICS )
    m.nLineHeight   = FipMetrics_LineHeight()
    m.nHeaderHeight = AfxScaleY(FIP_HEADER_HEIGHT)
    m.nBlockPad     = AfxScaleY(FIP_BLOCK_PAD)
    m.nBlockGap     = AfxScaleY(FIP_BLOCK_GAP)
    m.nGutter       = AfxScaleX(FIP_GUTTER_WIDTH)
    m.nIndent       = AfxScaleX(FIP_INDENT)
end sub


' ========================================================================================
' Push the scrollbar's range from the canvas, and clamp the scroll position to it.
' ========================================================================================
private sub FipSurface_SyncScrollBar()
    if IsWindow( HWND_FRMFINDINPROJECT_VSCROLL ) = 0 then exit sub

    dim as RECT rc
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rc )
    dim as long nPage = rc.bottom - rc.top
    if nPage < 1 then nPage = 1

    dim as long nMax = gFipCanvasH - nPage
    if nMax < 0 then nMax = 0
    if gFipScrollTop > nMax then gFipScrollTop = nMax
    if gFipScrollTop < 0 then gFipScrollTop = 0

    PsVScrollBar_SetRange( HWND_FRMFINDINPROJECT_VSCROLL, gFipCanvasH, nPage, gFipScrollTop )
    ' One notch moves three document lines, which is what the editor's own wheel does.
    PsVScrollBar_SetWheelStep( HWND_FRMFINDINPROJECT_VSCROLL, gFipMetrics.nLineHeight * 3 )
end sub


sub frmFindInProject_Refresh( byval bResetScroll as boolean = false )
    FipMetrics_Measure( gFipMetrics )
    gFipCanvasH = FipRows_Build( gFipRows(), gFipRowCount, gFipMetrics )
    if bResetScroll then gFipScrollTop = 0
    FipSurface_SyncScrollBar()
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then
        AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )
    end if
end sub


' ========================================================================================
' True when the Find in Project tab is the tab currently showing.
' ========================================================================================
function frmFindInProject_IsActive() as boolean
    return gTTabCtl.IsFindTab( gTTabCtl.GetCurSel() )
end function


' Width of a run of text in the font currently set on the buffer. DT_CALCRECT through
' PaintTextEx, because PaintText forces DT_VCENTER or DT_SINGLELINE and cannot measure --
' the same route frmAbout takes.
private function FipSurface_TextW( byval b as PsBufferPaint ptr, byval wszText as DWSTRING ) as long
    dim as RECT rc
    rc.left   = 0
    rc.top    = 0
    rc.right  = 32000
    rc.bottom = 32000
    b->PaintTextEx( wszText, @rc, DT_LEFT or DT_SINGLELINE or DT_CALCRECT )
    function = rc.right - rc.left
end function


' ========================================================================================
' One file's separator bar: chevron, name, path, match count.
' ========================================================================================
private sub FipSurface_PaintHeader( byval b as PsBufferPaint ptr, byref r as FIP_ROW, _
                                    byval yTop as long, byref rcClient as RECT )
    dim as RECT rc
    rc.left   = 0
    rc.right  = rcClient.right
    rc.top    = yTop
    rc.bottom = yTop + r.nHeight

    b->SetBackColor( ghEditor.BackColorLinenumbers )
    b->PaintRect( @rc )

    dim as long nPad = AfxScaleX(6)

    ' The twisty. Down = expanded, right = collapsed, which is the direction every tree in
    ' tiko already uses.
    dim as RECT rcGlyph = rc
    rcGlyph.left  += nPad
    rcGlyph.right = rcGlyph.left + AfxScaleX(16)
    b->SetFont( ghFont(SYMBOLFONT_9) )
    b->SetForeColor( ghEditor.ForeColorText )
    b->PaintText( iif( gFip.groups(r.nGroup).bCollapsed, wszIconChevronRight, wszIconChevronDown ), _
                  @rcGlyph, DT_CENTER )

    ' Name, then the match count, then the directory in the dimmer line-number colour. Drawn
    ' in that order because the count must not be pushed off the right by a long path.
    dim as DWSTRING wszName  = AfxStrPathname( "NAMEX", gFip.groups(r.nGroup).wszFile )
    dim as DWSTRING wszDir   = AfxStrPathname( "PATH",  gFip.groups(r.nGroup).wszFile )
    dim as DWSTRING wszCount = "(" & str( gFip.groups(r.nGroup).nMatchCount ) & ")"

    dim as RECT rcText = rc
    rcText.left = rcGlyph.right + nPad

    b->SetFont( ghFont(GUIFONTBOLD_10) )
    b->SetForeColor( ghEditor.ForeColorText )
    dim as long nNameW = FipSurface_TextW( b, wszName ) + nPad
    b->PaintText( wszName, @rcText, DT_LEFT )

    dim as RECT rcCount = rc
    rcCount.left = rcText.left + nNameW
    b->SetFont( ghFont(GUIFONT_9) )
    b->SetForeColor( ghEditor.ForeColorLinenumbers )
    dim as long nCountW = FipSurface_TextW( b, wszCount ) + nPad
    b->PaintText( wszCount, @rcCount, DT_LEFT )

    if len(wszDir) then
        dim as RECT rcDir = rc
        rcDir.left = rcCount.left + nCountW
        if rcDir.left < rcDir.right then
            b->PaintText( wszDir, @rcDir, DT_LEFT )
        end if
    end if

    ' Hairline along the bottom so a run of headers reads as separated.
    dim as RECT rcDiv = rc
    rcDiv.top = rcDiv.bottom - AfxScaleY(1)
    b->SetBackColor( ghEditor.Divider )
    b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' One excerpt block.
'
' PLACEHOLDER RENDERING, deliberately: the lines are painted here with a proportional GUI
' font, which is not what code should look like. It exists so the GEOMETRY can be measured
' and asserted before a real Scintilla view is involved -- the pooled views replace this
' whole function, and they bring the editor's own font, styling and match highlighting with
' them. The block HEIGHT is already the real one (nLineHeight comes from SCI_TEXTHEIGHT), so
' swapping the painter in will not move anything.
' ========================================================================================
private sub FipSurface_PaintBlock( byval b as PsBufferPaint ptr, byref r as FIP_ROW, _
                                   byval yTop as long, byref rcClient as RECT )
    dim as long nBlock = r.nBlock
    dim as clsDocument ptr pDoc = gFip.groups(r.nGroup).pDoc
    if pDoc = 0 then exit sub

    dim as RECT rc
    rc.left   = gFipMetrics.nIndent
    rc.right  = rcClient.right
    rc.top    = yTop
    rc.bottom = yTop + r.nHeight

    b->SetBackColor( ghEditor.BackColorText )
    b->PaintRect( @rc )

    ' The line-number gutter, running the full height of the block.
    dim as RECT rcGutter = rc
    rcGutter.right = rcGutter.left + gFipMetrics.nGutter
    b->SetBackColor( ghEditor.BackColorLinenumbers )
    b->PaintRect( @rcGutter )

    dim as boolean bUtf8 = (pDoc->FileEncoding <> FILE_ENCODING_ANSI)
    dim as long nFirst = gFip.blocks(nBlock).nFirstLine
    dim as long y = yTop + gFipMetrics.nBlockPad

    for i as long = 0 to gFip.blocks(nBlock).nLineCount - 1
        ' Bail as soon as the rest of the block is below the viewport.
        if y > rcClient.bottom then exit for

        dim as long nLine = nFirst + i

        dim as RECT rcNum
        rcNum.left   = rc.left
        rcNum.right  = rcGutter.right - AfxScaleX(6)
        rcNum.top    = y
        rcNum.bottom = y + gFipMetrics.nLineHeight
        b->SetFont( ghFont(GUIFONT_9) )
        b->SetForeColor( ghEditor.ForeColorLinenumbers )
        b->SetBackColor( ghEditor.BackColorLinenumbers )
        ' 1-based, as the editor's own margin shows them.
        b->PaintText( str(nLine + 1), @rcNum, DT_RIGHT )

        dim as string sLine = pDoc->GetLine( nLine )
        ' GetLine keeps the line ending; painting it would draw a stray glyph.
        do while len(sLine) > 0
            dim as long c = asc( right(sLine, 1) )
            if (c <> 10) andalso (c <> 13) then exit do
            sLine = left( sLine, len(sLine) - 1 )
        loop

        dim as DWSTRING wszLine
        ' A UTF-8 document has to go through the .utf8 property or non-latin text garbles.
        if bUtf8 then
            wszLine.utf8 = sLine
        else
            wszLine = sLine
        end if

        dim as RECT rcLine
        rcLine.left   = rcGutter.right + AfxScaleX(6)
        rcLine.right  = rc.right
        rcLine.top    = y
        rcLine.bottom = y + gFipMetrics.nLineHeight
        b->SetForeColor( ghEditor.ForeColorText )
        b->SetBackColor( ghEditor.BackColorText )
        b->PaintText( wszLine, @rcLine, DT_LEFT )

        y += gFipMetrics.nLineHeight
    next
end sub


' ========================================================================================
' Paint the surface: the visible slice of the canvas and nothing else.
'
' Colours come from the EDITOR palette, not the panel one: this window sits in the space a
' document's Scintilla windows occupy, and it is about to hold real Scintilla excerpt views
' of those same documents. Chrome drawn against a different background from the code inside
' it would read as broken.
' ========================================================================================
' The whole frame, taking the buffer rather than owning it. Split out from WM_PAINT SO THAT A
' PROBE CAN DRIVE THE EXACT PATH WM_PAINT USES -- PsDatePicker's lesson: a probe that renders
' through some other route can report a healthy number while the real window draws nothing.
private sub FipSurface_Render( byval b as PsBufferPaint ptr, byref rc as RECT )
    b->SetBackColor( ghEditor.BackColorText )
    b->PaintRect( @rc )

    if (gFip.bValid = false) orelse (gFipRowCount = 0) then
        ' Empty state. Reuses the tab's own caption rather than minting a localized id that
        ' would be retired the moment this gets a real message.
        dim as RECT rcText = rc
        rcText.top  += AfxScaleY(20)
        rcText.left += AfxScaleX(20)
        b->SetFont( ghFont(GUIFONT_10) )
        b->SetForeColor( ghEditor.ForeColorLinenumbers )
        b->SetBackColor( ghEditor.BackColorText )
        b->PaintText( L(206, "Find in Project"), @rcText, DT_LEFT )
        exit sub
    end if

    dim as long nFirst = FipRows_FirstVisible( gFipRows(), gFipRowCount, gFipScrollTop )
    if nFirst >= 0 then
        for i as long = nFirst to gFipRowCount - 1
            dim as long yTop = gFipRows(i).nTop - gFipScrollTop
            if yTop > rc.bottom then exit for
            if gFipRows(i).nKind = FIP_ROW_HEADER then
                FipSurface_PaintHeader( b, gFipRows(i), yTop, rc )
            else
                FipSurface_PaintBlock( b, gFipRows(i), yTop, rc )
            end if
        next
    end if
end sub


' Thin WM_PAINT wrapper: owns the buffer, delegates the drawing.
private sub FipSurface_OnPaint( byval hwnd as HWND )
    dim as PsBufferPaint b
    ' NOTE: BeginDoubleBuffer calls BeginPaint itself. Do not wrap this in another
    ' BeginPaint/EndPaint pair -- the outer one validates the update region and the buffer
    ' then paints into an empty rect, giving a blank window with a clean build.
    b.BeginDoubleBuffer( hwnd )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    FipSurface_Render( @b, rc )
    b.EndDoubleBuffer()
end sub


' ========================================================================================
' How many distinct colours does the surface actually put on screen?
'
' Renders offscreen THROUGH THE REAL FipSurface_Render and counts distinct pixels over a
' sampled grid. This exists because every geometry assertion in this file is blind to whether
' anything is drawn at all: a painter that renders nothing, or floods the whole surface with
' one colour, satisfies all of them. Both have happened in this codebase.
'
' Counting stops at the cap -- the question is "is there ink", not "how much".
' ========================================================================================
private function FipSurface_CountRenderedTones() as long
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) = 0 then return -1

    dim as RECT rcClient
    GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return -1

    dim as HDC hRef = GetDC( HWND_FRMFINDINPROJECT_SURFACE )
    if hRef = 0 then return -1
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( HWND_FRMFINDINPROJECT_SURFACE, hRef )
        return -1
    end if

    ' 32bpp top-down, matching what PsBufferPaint.SetupBitmap builds, so the pixels read back
    ' are laid out the way the renderer wrote them.
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = cw
        .biHeight      = -ch
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    dim as any ptr pBits
    dim as HBITMAP hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( HWND_FRMFINDINPROJECT_SURFACE, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return -1
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    scope
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( HWND_FRMFINDINPROJECT_SURFACE, hMem, rcClient, hMem )
        FipSurface_Render( @b, rcClient )
        b.EndDoubleBuffer()
    end scope

    const FIP_TONE_CAP = 64
    dim as ulong seen(0 to FIP_TONE_CAP - 1)
    dim as long nSeen = 0
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = 0 to ch - 1 step 2
        for x as long = 0 to cw - 1 step 2
            dim as ulong c = px[(y * cw) + x]
            dim as boolean bFound = false
            for i as long = 0 to nSeen - 1
                if seen(i) = c then bFound = true: exit for
            next
            if bFound = false then
                if nSeen >= FIP_TONE_CAP then exit for, for
                seen(nSeen) = c
                nSeen += 1
            end if
        next
    next

    SelectObject( hMem, hOldBmp )
    DeleteObject( hbm )
    DeleteDC( hMem )
    function = nSeen
end function


' ========================================================================================
' Which row is at this client y? -1 for none.
' ========================================================================================
private function FipSurface_RowAtPoint( byval y as long ) as long
    dim as long nCanvasY = y + gFipScrollTop
    dim as long i = FipRows_FirstVisible( gFipRows(), gFipRowCount, nCanvasY )
    if i < 0 then return -1
    ' FirstVisible answers the row whose bottom is past nCanvasY, which may be the row AFTER a
    ' point that fell in an inter-block gap. Reject that rather than reporting a near miss.
    if nCanvasY < gFipRows(i).nTop then return -1
    function = i
end function


' ========================================================================================
' Surface window procedure.
' ========================================================================================
private function FipSurface_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            return true

        case WM_PAINT
            FipSurface_OnPaint( hwnd )
            return 0

        case WM_SIZE
            ' The page size changed, so the scrollbar range and the clamp both move.
            FipSurface_SyncScrollBar()
            return 0

        case WM_MOUSEWHEEL
            ' Route the gesture into the bar: the surface owns no scroll state of its own
            ' beyond gFipScrollTop, and the bar owns the arithmetic (and the sub-notch
            ' accumulation that makes a precision touchpad scroll smoothly).
            PsVScrollBar_HandleWheelDelta( HWND_FRMFINDINPROJECT_VSCROLL, _
                                           cast(short, hiword(wParam)) )
            return 0

        case WM_LBUTTONDOWN
            ' Signed: a click can be dragged above the client area.
            dim as long i = FipSurface_RowAtPoint( cast(short, hiword(lParam)) )
            if i >= 0 then
                if gFipRows(i).nKind = FIP_ROW_HEADER then
                    dim as long g = gFipRows(i).nGroup
                    gFip.groups(g).bCollapsed = not gFip.groups(g).bCollapsed
                    ' Keep the scroll where it is: the header the user clicked should not
                    ' jump out from under the cursor.
                    frmFindInProject_Refresh( false )
                end if
            end if
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMFINDINPROJECT_SURFACE = 0
            return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' The scrollbar moved.
' ========================================================================================
private sub FipSurface_ScrollCallback( byval hSB as HWND, byval nPos as integer )
    gFipScrollTop = nPos
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then
        AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )
    end if
end sub


' ========================================================================================
' frmFindInProject window procedure
' ========================================================================================
private function frmFindInProject_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            ' Fully painted in WM_PAINT.
            return true

        case WM_PAINT
            ' The surface and the scrollbar cover almost all of this window; only the strip
            ' the scrollbar vacates when it hides is left, so a plain fill is enough.
            dim as PsBufferPaint b
            b.BeginDoubleBuffer( hwnd )
            b.SetBackColor( ghEditor.BackColorText )
            b.PaintClientRect()
            b.EndDoubleBuffer()
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMFINDINPROJECT = 0
            ' The children have been destroyed with this window. The surface zeroes its own
            ' handle in its WM_NCDESTROY; the scrollbar's is a plain global, so it is cleared
            ' here rather than left dangling.
            HWND_FRMFINDINPROJECT_VSCROLL = 0
            return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Create the results window. Idempotent -- returns the existing handle if it already exists.
' ========================================================================================
function frmFindInProject_CreateWindow() as HWND
    if IsWindow( HWND_FRMFINDINPROJECT ) then return HWND_FRMFINDINPROJECT

    dim pWindow as CWindow ptr = new CWindow
    ' A child of frmMain, created hidden and sized by frmMain_PositionWindows -- exactly how
    ' a document's Scintilla windows are handled.
    '
    ' WS_EX_CONTROLPARENT is correct here and is NOT the mistake PsButton had to fix: this
    ' window really will hold child controls the dialog manager should recurse into (the
    ' pooled Scintilla excerpt views), and it never takes a tab stop of its own.
    HWND_FRMFINDINPROJECT = pWindow->Create( HWND_FRMMAIN, "", @frmFindInProject_WndProc, _
                    0, 0, 0, 0, _
                    WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                    WS_EX_CONTROLPARENT )
    if HWND_FRMFINDINPROJECT = 0 then return 0

    ' ------------------------------------------------------------------------------------
    ' The surface: the viewport onto the virtual canvas. It is a separate window from the
    ' container so that its CLIENT AREA IS the viewport -- the scrollbar sits outside it, so
    ' nothing has to subtract a scrollbar width when hit-testing or when placing the excerpt
    ' views that will live on it.
    ' ------------------------------------------------------------------------------------
    dim pSurface as CWindow ptr = new CWindow
    HWND_FRMFINDINPROJECT_SURFACE = pSurface->Create( HWND_FRMFINDINPROJECT, "", _
                    @FipSurface_WndProc, 0, 0, 0, 0, _
                    WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                    WS_EX_CONTROLPARENT )
    ' CWindow's default class style carries CS_HREDRAW or CS_VREDRAW, which flicker badly on
    ' a window that is resized and repainted as often as this one. Read-modify-write: the
    ' setter REPLACES, and it needs the window to exist. (PsScrollPanel's precedent.)
    if HWND_FRMFINDINPROJECT_SURFACE then
        pSurface->ClassStyle = pSurface->ClassStyle and (not (CS_HREDRAW or CS_VREDRAW))
    end if

    HWND_FRMFINDINPROJECT_VSCROLL = PsVScrollBar_Create( HWND_FRMFINDINPROJECT, _
                                                         IDC_FRMFINDINPROJECT_VSCROLL )
    PsVScrollBar_SetScrollCallback( HWND_FRMFINDINPROJECT_VSCROLL, @FipSurface_ScrollCallback )
    PsVScrollBar_SetColors( HWND_FRMFINDINPROJECT_VSCROLL, ghEditor.BackColorScrollBar, _
                            ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBar )

    return HWND_FRMFINDINPROJECT
end function


' ========================================================================================
' Size the surface into the document client area. rcDoc arrives in frmMain client
' coordinates -- the same rect frmMain_PositionWindows hands a document's editors.
' ========================================================================================
sub frmFindInProject_PositionWindows( byval rcDoc as RECT )
    if IsWindow( HWND_FRMFINDINPROJECT ) = 0 then exit sub

    dim as long nW = rcDoc.right - rcDoc.left
    dim as long nH = rcDoc.bottom - rcDoc.top

    SetWindowPos( HWND_FRMFINDINPROJECT, 0, _
                  rcDoc.left, rcDoc.top, nW, nH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' The scrollbar strip stays RESERVED whether or not the thumb is showing -- the same call
    ' PsScrollPanel makes, and for the same reason: reclaiming the width would reflow every
    ' excerpt line the moment the results got short enough not to need scrolling.
    dim as long nBar = AfxScaleX(SCROLLBAR_WIDTH_EDITOR)
    dim as long nSurfaceW = nW - nBar
    if nSurfaceW < 0 then nSurfaceW = 0

    SetWindowPos( HWND_FRMFINDINPROJECT_SURFACE, 0, 0, 0, nSurfaceW, nH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )
    SetWindowPos( HWND_FRMFINDINPROJECT_VSCROLL, 0, nSurfaceW, 0, nBar, nH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' A width change does not alter the canvas height (excerpts do not wrap), but the page
    ' size did change, so the range and the clamp have to be re-pushed.
    FipSurface_SyncScrollBar()
end sub


' ========================================================================================
' Env-gated self-test: TIKO_FINDPROJ_SELFTEST=1
'
' Drives the real Show and the real close path, because the thing under test is WIRING --
' the sentinel surviving GetpDoc, the outgoing editors actually hiding, the surface being
' sized, the menus' document count, and the session index. None of that is reachable from a
' pure function, and the build was clean before any of it worked.
' ========================================================================================
dim shared as long gFipTestPass, gFipTestFail

private sub frmFindInProject_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gFipTestPass += 1
    else
        gFipTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

' How many documents currently have a VISIBLE Scintilla window.
private function frmFindInProject_CountVisibleEditors() as long
    dim as long n = 0
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
            if IsWindowVisible( pDoc->hWindow(i) ) then n += 1
        next
        pDoc = pDoc->pDocNext
    loop
    function = n
end function

sub frmFindInProject_RunSelfTest()
    if environ("TIKO_FINDPROJ_SELFTEST") <> "1" then exit sub

    gFipTestPass = 0
    gFipTestFail = 0
    print "--- TIKO_FINDPROJ_SELFTEST ---"

    ' ---- the visible-row search, on a SYNTHETIC canvas ------------------------------------
    ' Driven by a hand-built row array rather than by a real search, so it is tested
    ' independently of the model and of whatever files happen to be open. Four rows, 100px
    ' each, tiling 0..399.
    dim synth(0 to 3) as FIP_ROW
    for i as long = 0 to 3
        synth(i).nTop    = i * 100
        synth(i).nHeight = 100
    next
    frmFindInProject_Check( "rowsearch: the top of the canvas finds row 0", _
        FipRows_FirstVisible( synth(), 4, 0 ) = 0, str(FipRows_FirstVisible( synth(), 4, 0 )) )
    frmFindInProject_Check( "rowsearch: one pixel inside row 0 still finds row 0", _
        FipRows_FirstVisible( synth(), 4, 99 ) = 0, str(FipRows_FirstVisible( synth(), 4, 99 )) )
    ' The boundary: at y=100 row 0 is entirely above the fold, so row 1 is first.
    frmFindInProject_Check( "rowsearch: exactly on a boundary finds the LOWER row", _
        FipRows_FirstVisible( synth(), 4, 100 ) = 1, str(FipRows_FirstVisible( synth(), 4, 100 )) )
    frmFindInProject_Check( "rowsearch: mid-canvas finds the containing row", _
        FipRows_FirstVisible( synth(), 4, 250 ) = 2, str(FipRows_FirstVisible( synth(), 4, 250 )) )
    frmFindInProject_Check( "rowsearch: the last pixel finds the last row", _
        FipRows_FirstVisible( synth(), 4, 399 ) = 3, str(FipRows_FirstVisible( synth(), 4, 399 )) )
    ' Past the end there is nothing to draw -- and -1 rather than a clamped 3, so the painter
    ' does not redraw the last row forever when scrolled past it.
    frmFindInProject_Check( "rowsearch: past the end answers -1, not a clamp", _
        FipRows_FirstVisible( synth(), 4, 400 ) = -1, str(FipRows_FirstVisible( synth(), 4, 400 )) )
    frmFindInProject_Check( "rowsearch: an empty canvas answers -1", _
        FipRows_FirstVisible( synth(), 0, 0 ) = -1, str(FipRows_FirstVisible( synth(), 0, 0 )) )

    dim as long nTabsBefore    = gTTabCtl.GetItemCount()
    dim as long nDocTabsBefore = gTTabCtl.GetDocumentTabCount()
    dim as long nSelBefore     = gTTabCtl.GetCurSel()
    dim as long nVisBefore     = frmFindInProject_CountVisibleEditors()

    ' With no documents open, the "editors get hidden" and "document count is unchanged"
    ' assertions cannot distinguish a working fix from a broken one. Said out loud rather
    ' than left as a flattering green.
    if nDocTabsBefore = 0 then
        print "  NOTE: no document tabs open -- the hide-the-editors assertions are VACUOUS."
    end if

    frmFindInProject_Check( "baseline: no Find tab is open", _
        gTTabCtl.FindTabIndex() = -1, str(gTTabCtl.FindTabIndex()) )

    ' ---- open it ------------------------------------------------------------------------
    frmFindInProject_Show()

    dim as long idx = gTTabCtl.FindTabIndex()
    frmFindInProject_Check( "Show: the Find tab exists", idx <> -1, str(idx) )

    if idx <> -1 then
        ' THE SENTINEL INVARIANT. Every one of the ~70 "if pDoc then <dereference>" call
        ' sites in the app depends on these three answers being exactly this combination:
        ' a VALID tab, that IS the Find tab, whose pDoc reads back NULL.
        frmFindInProject_Check( "sentinel: the tab is a valid tab", _
            gTTabCtl.IsValidTab(idx) = true )
        frmFindInProject_Check( "sentinel: IsFindTab sees it", _
            gTTabCtl.IsFindTab(idx) = true )
        frmFindInProject_Check( "sentinel: GetpDoc answers NULL, not 1", _
            gTTabCtl.GetpDoc(idx) = 0 )
        frmFindInProject_Check( "sentinel: GetActiveDocumentPtr answers NULL", _
            gTTabCtl.GetActiveDocumentPtr() = 0 )

        frmFindInProject_Check( "Show: the Find tab became the current tab", _
            gTTabCtl.GetCurSel() = idx, str(gTTabCtl.GetCurSel()) & " vs " & str(idx) )

        ' The count split that keeps the File / Compile / Debug menus honest.
        frmFindInProject_Check( "counts: the tab count went UP by one", _
            gTTabCtl.GetItemCount() = nTabsBefore + 1, str(gTTabCtl.GetItemCount()) )
        frmFindInProject_Check( "counts: the DOCUMENT tab count did not move", _
            gTTabCtl.GetDocumentTabCount() = nDocTabsBefore, _
            str(gTTabCtl.GetDocumentTabCount()) & " vs " & str(nDocTabsBefore) )

        ' The session writers skip this tab, so the persisted index must name a tab that will
        ' actually be recreated -- i.e. it must land inside the DOCUMENT tab count.
        '
        ' The obvious form of this assertion, "nSave <> idx", is WRONG and was written that
        ' way first: with the Find tab as the only tab it sits at index 0 and the fallback is
        ' also 0, so it failed while nothing was broken. It could not tell "points at the
        ' Find tab" from "there is nothing else to point at". This form can.
        dim as long nSave = gTTabCtl.SaveActiveTabIndex()
        if nDocTabsBefore = 0 then
            frmFindInProject_Check( "session: with no documents, ActiveTab is 0", _
                nSave = 0, str(nSave) )
        else
            frmFindInProject_Check( "session: ActiveTab names a tab that will be restored", _
                (nSave >= 0) andalso (nSave < nDocTabsBefore), _
                str(nSave) & " of " & str(nDocTabsBefore) )
        end if

        frmFindInProject_Check( "layout: every document editor is hidden", _
            frmFindInProject_CountVisibleEditors() = 0, _
            str(frmFindInProject_CountVisibleEditors()) & " still visible" )

        ' THE DisplayScintilla FIX, and it needs a BARE SetFocusTab to be visible at all.
        '
        ' The assertion above does NOT test it: frmFindInProject_Show pairs TabFocusLost with
        ' SetFocusTab, and TabFocusLost hides the outgoing document through a VALID document
        ' index -- so pDocShow is non-null there and the hide-all loop runs even with the old
        ' early return in place. Measured: restoring that early return left this suite fully
        ' green until the sequence below was added.
        '
        ' Seven paths call SetFocusTab bare, and two of them can land on the Find tab: the
        ' tabs-list dropdown (modContextMenus) and the tab right-click. This reproduces that
        ' exact sequence -- select a document tab, then select the Find tab with no
        ' TabFocusLost -- which is where a null pDocShow reaches DisplayScintilla and the
        ' outgoing editors used to stay painted over the results.
        if nDocTabsBefore > 0 then
            dim as long nDocTab = -1
            for i as long = 0 to gTTabCtl.GetItemCount() - 1
                if gTTabCtl.GetpDoc(i) then nDocTab = i: exit for
            next
            if nDocTab <> -1 then
                gTTabCtl.SetFocusTab( nDocTab )
                frmFindInProject_Check( "bare switch: the document's editor is showing first", _
                    frmFindInProject_CountVisibleEditors() > 0, _
                    str(frmFindInProject_CountVisibleEditors()) )
                ' Deliberately NO TabFocusLost here -- that is the whole point.
                gTTabCtl.SetFocusTab( idx )
                frmFindInProject_Check( "bare switch: editors hide without TabFocusLost", _
                    frmFindInProject_CountVisibleEditors() = 0, _
                    str(frmFindInProject_CountVisibleEditors()) & " still visible" )
            end if
        else
            print "  NOTE: no document tabs -- the bare-SetFocusTab assertions are SKIPPED."
        end if

        ' The surface itself, sized by frmMain_PositionWindows off the hoisted rcDoc. Before
        ' that hoist the rect was computed only on the pDoc <> 0 path, so this was 0x0.
        frmFindInProject_Check( "layout: the results window exists", _
            IsWindow( HWND_FRMFINDINPROJECT ) <> 0 )
        frmFindInProject_Check( "layout: the results window is visible", _
            IsWindowVisible( HWND_FRMFINDINPROJECT ) <> 0 )

        dim as RECT rcSurf = AfxGetWindowRect( HWND_FRMFINDINPROJECT )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rcSurf), 2 )
        dim as RECT rcMain: GetClientRect( HWND_FRMMAIN, @rcMain )
        frmFindInProject_Check( "layout: the results window has a real size", _
            (rcSurf.right - rcSurf.left > 0) andalso (rcSurf.bottom - rcSurf.top > 0), _
            str(rcSurf.right - rcSurf.left) & " x " & str(rcSurf.bottom - rcSurf.top) )
        frmFindInProject_Check( "layout: it fits inside frmMain's client area", _
            (rcSurf.right <= rcMain.right) andalso (rcSurf.bottom <= rcMain.bottom), _
            str(rcSurf.right) & "," & str(rcSurf.bottom) & " in " & _
            str(rcMain.right) & "," & str(rcMain.bottom) )

        ' The bar is forced open, and the file-info strip stood down for it.
        frmFindInProject_Check( "bars: the Find bar was forced open", _
            gFind.bShowFindPanel = true )
        frmFindInProject_Check( "bars: the file-info strip stood down", _
            gFind.bShowInfoPanel = false )
    end if

    ' ---- the search engine, against an independent oracle ---------------------------------
    ' NOTE THE ORDER: everything from here on runs with the tab still OPEN, because the
    ' surface has to be alive for the canvas and render assertions to mean anything. Closing
    ' first left the tone probe answering -1 and the scrollbar sync never running, while every
    ' canvas assertion still passed -- they only need the row list, which is pure.
    ' THE PURE HALF IS BLIND TO WHETHER THE ENGINE FINDS ANYTHING. It only ever reaches the
    ' remap and window arithmetic, so stubbing the search out entirely leaves it fully green.
    ' What follows checks the real thing: it takes a phrase out of a real document's own text,
    ' counts the occurrences itself with plain string operations, and requires the engine to
    ' agree -- both on how many there are and on where each one is.
    dim as clsDocument ptr pTest = 0
    dim pWalk as clsDocument ptr = gApp.pDocList
    do until pWalk = 0
        if IsWindow( pWalk->hWindow(0) ) then pTest = pWalk: exit do
        pWalk = pWalk->pDocNext
    loop

    if pTest = 0 then
        print "  NOTE: no realized document -- the search-engine assertions are SKIPPED."
    else
        dim as string sText = pTest->GetText()

        ' Take the first run of >= 5 letters as the phrase, so this works against whatever
        ' file happens to be open rather than against a hardcoded needle.
        dim as string sPhrase
        dim as long nRun = 0, nRunStart = 0
        for i as long = 1 to len(sText)
            dim as long c = asc( mid(sText, i, 1) )
            dim as boolean bAlpha = ((c >= 65) andalso (c <= 90)) orelse ((c >= 97) andalso (c <= 122))
            if bAlpha then
                if nRun = 0 then nRunStart = i
                nRun += 1
                if nRun >= 5 then sPhrase = mid(sText, nRunStart, 5): exit for
            else
                nRun = 0
            end if
        next

        if len(sPhrase) = 0 then
            print "  NOTE: no usable phrase in the test document -- search assertions SKIPPED."
        else
            ' The oracle: count case-sensitively with instr, which shares no code with the
            ' engine (that goes through Scintilla's SCI_SEARCHINTARGET).
            dim as long nOracle = 0
            dim as long p = instr( sText, sPhrase )
            do while p > 0
                nOracle += 1
                p = instr( p + 1, sText, sPhrase )
            loop

            FindProject_Search( sPhrase, true, false )

            ' Find this document's group.
            dim as long nGrp = -1
            for g as long = 0 to gFip.nGroupCount - 1
                if gFip.groups(g).pDoc = pTest then nGrp = g: exit for
            next

            frmFindInProject_Check( "engine: the test document produced a group", _
                nGrp <> -1, "phrase [" & sPhrase & "] oracle " & str(nOracle) )

            if nGrp <> -1 then
                frmFindInProject_Check( "engine: match count agrees with the oracle", _
                    gFip.groups(nGrp).nMatchCount = nOracle, _
                    str(gFip.groups(nGrp).nMatchCount) & " vs " & str(nOracle) )

                ' EVERY recorded offset must actually point at the phrase in the raw bytes.
                ' This is the assertion that would catch a position off by a BOM, by a line
                ' ending, or by a UTF-8 unit -- none of which a count can see.
                dim as long nBadPos = 0
                dim as long m0 = gFip.groups(nGrp).nMatchFirst
                for i as long = m0 to m0 + gFip.groups(nGrp).nMatchCount - 1
                    if mid( sText, gFip.matches(i).nStart + 1, len(sPhrase) ) <> sPhrase then
                        nBadPos += 1
                    end if
                next
                frmFindInProject_Check( "engine: every offset points at the phrase", _
                    nBadPos = 0, str(nBadPos) & " wrong of " & str(gFip.groups(nGrp).nMatchCount) )

                ' Structural contract of the coalescing: every match belongs to exactly one
                ' block, and blocks run in ascending line order without overlapping.
                dim as long nSumMatches = 0
                dim as long nOutOfOrder  = 0
                dim as long nPrevLast    = -1
                dim as long b0 = gFip.groups(nGrp).nBlockFirst
                for b as long = b0 to b0 + gFip.groups(nGrp).nBlockCount - 1
                    nSumMatches += gFip.blocks(b).nMatchCount
                    if gFip.blocks(b).nFirstLine <= nPrevLast then nOutOfOrder += 1
                    nPrevLast = gFip.blocks(b).nFirstLine + gFip.blocks(b).nLineCount - 1
                next
                frmFindInProject_Check( "engine: every match belongs to exactly one block", _
                    nSumMatches = gFip.groups(nGrp).nMatchCount, _
                    str(nSumMatches) & " vs " & str(gFip.groups(nGrp).nMatchCount) )
                frmFindInProject_Check( "engine: blocks ascend and never overlap", _
                    nOutOfOrder = 0, str(nOutOfOrder) & " out of order" )
                frmFindInProject_Check( "engine: every block shows at least the context window", _
                    gFip.blocks(b0).nLineCount >= 1, str(gFip.blocks(b0).nLineCount) )

                ' The remap, driven through the REAL hook rather than the pure function: an
                ' insertion at the very start of the file must shift every match in it.
                dim as long nFirstStart = gFip.matches(m0).nStart
                FindProject_OnDocumentModified( pTest, SC_MOD_INSERTTEXT, 0, 7, 0 )
                frmFindInProject_Check( "remap: the hook shifted a match by the insert length", _
                    gFip.matches(m0).nStart = nFirstStart + 7, _
                    str(gFip.matches(m0).nStart) & " vs " & str(nFirstStart + 7) )

                ' ...and the suppression flag the replace loop will rely on really suppresses.
                dim as long nNow = gFip.matches(m0).nStart
                gFipSuppressRemap = true
                FindProject_OnDocumentModified( pTest, SC_MOD_INSERTTEXT, 0, 7, 0 )
                gFipSuppressRemap = false
                frmFindInProject_Check( "remap: gFipSuppressRemap blocks the hook", _
                    gFip.matches(m0).nStart = nNow, _
                    str(gFip.matches(m0).nStart) & " vs " & str(nNow) )
            end if

            ' ---- the virtual canvas ------------------------------------------------------
            ' Geometry is asserted by READING THE NUMBERS, never by eye.
            frmFindInProject_Refresh( true )

            frmFindInProject_Check( "canvas: one row per header plus one per block", _
                gFipRowCount = gFip.nGroupCount + gFip.nBlockCount, _
                str(gFipRowCount) & " vs " & str(gFip.nGroupCount + gFip.nBlockCount) )

            ' Rows must TILE the canvas in ascending order with no row starting before the
            ' previous one ended. This is what the binary search depends on.
            dim as long nBadOrder = 0
            dim as long nPrevEnd = 0
            for i as long = 0 to gFipRowCount - 1
                if gFipRows(i).nTop < nPrevEnd then nBadOrder += 1
                if gFipRows(i).nHeight <= 0 then nBadOrder += 1
                nPrevEnd = gFipRows(i).nTop + gFipRows(i).nHeight
            next
            frmFindInProject_Check( "canvas: rows ascend and every height is positive", _
                nBadOrder = 0, str(nBadOrder) & " bad of " & str(gFipRowCount) )

            frmFindInProject_Check( "canvas: the canvas is at least as tall as the last row", _
                gFipCanvasH >= nPrevEnd, str(gFipCanvasH) & " vs " & str(nPrevEnd) )

            ' A block's height must be exactly its line count times the line height plus the
            ' padding -- the identity the excerpt views will be sized against, so an error
            ' here would misplace every one of them.
            dim as long nBadHeight = 0
            for i as long = 0 to gFipRowCount - 1
                if gFipRows(i).nKind <> FIP_ROW_BLOCK then continue for
                dim as long nWant = FipRows_BlockHeight( gFip.blocks( gFipRows(i).nBlock ).nLineCount, _
                                                         gFipMetrics )
                if gFipRows(i).nHeight <> nWant then nBadHeight += 1
            next
            frmFindInProject_Check( "canvas: every block height matches the line-count formula", _
                nBadHeight = 0, str(nBadHeight) & " wrong" )

            ' The line height came from Scintilla, not from a guess -- so the painted
            ' placeholder and the real excerpt views agree about how tall a line is.
            frmFindInProject_Check( "canvas: the line height came from Scintilla", _
                gFipMetrics.nLineHeight > 0, str(gFipMetrics.nLineHeight) )

            ' COLLAPSING drops a group's blocks and shortens the canvas; expanding restores
            ' it exactly. An off-by-one in the rebuild would show up as a canvas that grows a
            ' little on every toggle.
            dim as long nRowsOpen = gFipRowCount
            dim as long nCanvasOpen = gFipCanvasH
            gFip.groups(nGrp).bCollapsed = true
            frmFindInProject_Refresh( false )
            frmFindInProject_Check( "collapse: the row count drops by that group's blocks", _
                gFipRowCount = nRowsOpen - gFip.groups(nGrp).nBlockCount, _
                str(gFipRowCount) & " vs " & str(nRowsOpen - gFip.groups(nGrp).nBlockCount) )
            frmFindInProject_Check( "collapse: the canvas got shorter", _
                gFipCanvasH < nCanvasOpen, str(gFipCanvasH) & " vs " & str(nCanvasOpen) )
            gFip.groups(nGrp).bCollapsed = false
            frmFindInProject_Refresh( false )
            frmFindInProject_Check( "collapse: expanding restores the canvas EXACTLY", _
                (gFipRowCount = nRowsOpen) andalso (gFipCanvasH = nCanvasOpen), _
                str(gFipRowCount) & "/" & str(gFipCanvasH) & " vs " & _
                str(nRowsOpen) & "/" & str(nCanvasOpen) )

            ' ---- does it actually DRAW? ----------------------------------------------------
            dim as long nTonesResults = FipSurface_CountRenderedTones()

            ' Leave no results behind: this suite runs at startup, not on the user's command.
            FindProject_Clear()
            frmFindInProject_Refresh( true )

            dim as long nTonesEmpty = FipSurface_CountRenderedTones()
            print "  TONES: with results = " & str(nTonesResults) & _
                  ", empty state = " & str(nTonesEmpty) & " (cap 64)"

            ' Ink reached the surface at all. A render that draws nothing, or floods the whole
            ' client with one colour, reports 1 here -- and satisfies every geometry
            ' assertion above.
            frmFindInProject_Check( "render: the empty state draws something", _
                nTonesEmpty > 1, str(nTonesEmpty) )

            ' THE DISCRIMINATING ONE, and it is RELATIVE rather than an absolute floor
            ' inherited from a sibling: how many tones a healthy render produces depends on
            ' the theme, the DPI and how much of the canvas is on screen, but a surface
            ' showing headers and excerpts must always be busier than one showing a single
            ' line of grey text. Measured here at 64 (capped) against 36.
            frmFindInProject_Check( "render: results draw strictly more than the empty state", _
                nTonesResults > nTonesEmpty, _
                str(nTonesResults) & " vs " & str(nTonesEmpty) )
        end if
    end if

    ' ---- close it -------------------------------------------------------------------------
    ' The real user path: the tab's X routes through CloseTab -> OnCommand_FileClose.
    if idx <> -1 then gTTabCtl.CloseTab( idx )

    frmFindInProject_Check( "close: the Find tab is gone", _
        gTTabCtl.FindTabIndex() = -1, str(gTTabCtl.FindTabIndex()) )
    frmFindInProject_Check( "close: the results window was destroyed", _
        IsWindow( HWND_FRMFINDINPROJECT ) = 0 )
    frmFindInProject_Check( "close: the tab count is back to baseline", _
        gTTabCtl.GetItemCount() = nTabsBefore, _
        str(gTTabCtl.GetItemCount()) & " vs " & str(nTabsBefore) )
    frmFindInProject_Check( "close: the file-info strip came back", _
        gFind.bShowInfoPanel = true )
    frmFindInProject_Check( "close: the Find bar was closed again", _
        gFind.bShowFindPanel = false )

    ' The editors have to come BACK. Closing the tab that was covering them is the half of
    ' the DisplayScintilla fix that a "did they hide" assertion cannot see.
    frmFindInProject_Check( "close: the document editors are visible again", _
        frmFindInProject_CountVisibleEditors() = nVisBefore, _
        str(frmFindInProject_CountVisibleEditors()) & " vs " & str(nVisBefore) )
    frmFindInProject_Check( "close: the previously active tab is current again", _
        gTTabCtl.GetCurSel() = nSelBefore, _
        str(gTTabCtl.GetCurSel()) & " vs " & str(nSelBefore) )

    print "--- TIKO_FINDPROJ_SELFTEST: " & str(gFipTestPass) & " passed, " & _
          str(gFipTestFail) & " failed ---"
end sub


' ========================================================================================
' frmFindInProject_Show
'
' Reached from the Search menu, the Ctrl+Shift+F accelerator and the Explorer panel's
' magnifier icon. There is at most ONE Find in Project tab: a second invocation re-selects
' the existing one rather than stacking another.
' ========================================================================================
function frmFindInProject_Show() as LRESULT
    frmFindInProject_CreateWindow()

    dim as long idx = gTTabCtl.FindTabIndex()

    if idx = -1 then
        ' TAB_FINDINPROJECT goes into the itemData slot that normally holds a clsDocument
        ' ptr. AddTab calls SetTabText, which leaves the caption alone for a tab with no
        ' document -- which is what lets the caption below stick.
        idx = PsTabBar_AddTab( gTTabCtl.hWindow, L(206, "Find in Project"), TAB_FINDINPROJECT )
        if idx = -1 then return 0
    end if

    gTTabCtl.TabFocusLost( gTTabCtl.GetCurSel() )
    gTTabCtl.SetFocusTab( idx )

    ' Adopt whatever the model holds. After SetFocusTab, so the surface has its real size and
    ' the scrollbar's page height is the one it will actually have.
    frmFindInProject_Refresh( true )

    function = 0
end function
