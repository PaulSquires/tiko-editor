'    PsTooltip - reusable owner-drawn tooltip control
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT ANY WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#pragma once

#include once "PsTooltip.bi"


' ========================================================================================
' Optional live trace: set PSTOOLTIP_TRACE=1. Resolved lazily through a -1 sentinel, because a
' module-level `dim shared` initialiser has to be a CONSTANT -- environ() is not one, and the
' obvious spelling fails with `error 11: Expected constant`. PsDatePicker's pattern.
'
' Everything goes through a `dim as string` first: `print` on a DWSTRING, or on a
' `string & wstring` concat, interleaves nulls (Learnings.md).
' ========================================================================================
dim shared gTIP_Trace as long = -1

private sub TIP_Log( byval sMsg as string )
    if gTIP_Trace = -1 then
        if environ("PSTOOLTIP_TRACE") = "1" then gTIP_Trace = 1 else gTIP_Trace = 0
    end if
    if gTIP_Trace <> 1 then exit sub
    dim as string s = "[PsTooltip] " & sMsg
    print s
end sub


' ========================================================================================
' THE SHARED CLOCK AND THE INSTANCE REGISTRY
'
' Every sibling in this family arms its hot-track timer from a WM_MOUSEMOVE and kills it on
' the leave. This control cannot: it never sees the attached window's messages, because it
' deliberately does not subclass it. Detecting that the cursor arrived IS the timer's job, so
' the timer has to be running before there is anything to detect.
'
' That is the whole reason there is ONE of it. A per-instance timer would mean N permanently
' running timers for N attached controls (tiko would carry about fifteen); one shared timer on
' a hidden message-only window costs the same whether one tip is attached or fifty.
'
' WHY REMOVAL COMPACTS RATHER THAN SWAPPING WITH THE LAST ENTRY -- and note this is NOT
' PsOptionButton's reason. There, order IS the arrow-key navigation order and compaction is
' required to preserve it. Here order carries no meaning at all: the tick does the same thing
' to every entry. Compaction is still correct, for a different reason -- the tick walks the
' registry BACKWARDS and evicts dead entries as it goes, and compaction only ever shifts
' entries ABOVE the removal point, all of which have already been visited. Swap-with-last
' would move an unvisited entry down into a slot the walk has passed, silently skipping it for
' that tick. The rationale was re-tested rather than inherited (CLAUDE.md); the conclusion
' happens to agree, the reasoning does not.
'
' SINGLE-THREADED. UI thread only.
' ========================================================================================
#define IDT_PSTOOLTIP_CLOCK   &hCB61

dim shared gTIP_Registry() as HWND
dim shared gTIP_Count    as long = 0
dim shared gTIP_Cap      as long = 0
' The one tip currently on screen (0 = none). Only one may be visible at a time -- the
' gPM_OpenRoot pattern from PsPopupMenu.
dim shared gTIP_Visible  as HWND
dim shared gTIP_hClock   as HWND
dim shared gTIP_ClockMs  as long = 0
dim shared gTIP_ClassReg as boolean = false

' Process-wide defaults applied by PsTooltip_Create. Each is INDEPENDENTLY ARMED, so a field
' the host never set keeps whatever Create derives for it -- see the .bi header for why that
' is not one struct. Same thread, same lifetime rules as the registry above.
dim shared gTIP_DefColors     as PSTOOLTIP_COLORS
dim shared gTIP_HasDefColors  as boolean = false
dim shared gTIP_DefFontText   as HFONT
dim shared gTIP_DefFontTitle  as HFONT
dim shared gTIP_DefFontGlyph  as HFONT
dim shared gTIP_HasDefFonts   as boolean = false
dim shared gTIP_DefStyle      as long = TIP_STYLE_RECT
dim shared gTIP_HasDefStyle   as boolean = false
dim shared gTIP_DefMaxWidth   as long = 0
dim shared gTIP_HasDefMaxW    as boolean = false
dim shared gTIP_DefInitial    as long = 0
dim shared gTIP_HasDefInitial as boolean = false
dim shared gTIP_DefAutoPop    as long = 0
dim shared gTIP_HasDefAutoPop as boolean = false
dim shared gTIP_DefReshow     as long = 0
dim shared gTIP_HasDefReshow  as boolean = false


private function GetTooltipPointer( byval hTooltip as HWND ) as PSTOOLTIP ptr
    if hTooltip = 0 then return 0
    if IsWindow(hTooltip) = 0 then return 0
    dim pWindow as CWindow ptr = AfxCWindowPtr(hTooltip)
    if pWindow = 0 then return 0
    dim as PSTOOLTIP ptr pTip = cast( PSTOOLTIP ptr, pWindow->UserData(0) )
    if pTip = 0 then return 0
    ' The signature is what makes the asynchronous clock tick safe: a handle whose window has
    ' been destroyed and whose slot 0 now belongs to something else fails here rather than
    ' being dereferenced.
    if pTip->dwSignature <> PSTOOLTIP_SIGNATURE then return 0
    return pTip
end function


private sub TIP_RegistryAdd( byval hTooltip as HWND )
    if hTooltip = 0 then exit sub
    if gTIP_Count >= gTIP_Cap then
        dim as long nNewCap = gTIP_Cap + 16
        redim preserve gTIP_Registry( 0 to nNewCap - 1 )
        gTIP_Cap = nNewCap
    end if
    gTIP_Registry( gTIP_Count ) = hTooltip
    gTIP_Count += 1
end sub


' Remove and CLOSE THE GAP. See the note above -- this is not a swap-with-last, and the reason
' is iteration safety during the tick, not ordering.
private sub TIP_RegistryRemove( byval hTooltip as HWND )
    if hTooltip = 0 then exit sub
    for i as long = 0 to gTIP_Count - 1
        if gTIP_Registry(i) = hTooltip then
            for j as long = i to gTIP_Count - 2
                gTIP_Registry(j) = gTIP_Registry(j + 1)
            next
            gTIP_Count -= 1
            gTIP_Registry( gTIP_Count ) = 0
            exit sub
        end if
    next
end sub


private function TIP_RegistryIndex( byval hTooltip as HWND ) as long
    for i as long = 0 to gTIP_Count - 1
        if gTIP_Registry(i) = hTooltip then return i
    next
    return -1
end function


' ========================================================================================
' Forward declarations -- the tick, the show path and the WndProc are mutually recursive.
' ========================================================================================
declare sub      TIP_ClockTick()
declare sub      TIP_ApplyClockRate()
declare function TIP_ShowCore( byval pTip as PSTOOLTIP ptr, byval rcAnchor as RECT, byval nAlign as long ) as boolean
declare sub      TIP_HideCore( byval pTip as PSTOOLTIP ptr, byval bAllowFade as boolean )
declare sub      PsTooltip_Render( byval pTip as PSTOOLTIP ptr, byval pb as PsBufferPaint ptr )
' The painter and the window region both build this, and they must agree.
declare sub      TIP_StemHouse( byval pTip as PSTOOLTIP ptr, byval nSkirt as long, byval pts as POINT ptr )


' ========================================================================================
' The clock's own window: message-only, invisible, owns nothing but the timer.
' ========================================================================================
private function TIP_ClockWndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                   byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    if uMsg = WM_TIMER then
        if wParam = IDT_PSTOOLTIP_CLOCK then
            TIP_ClockTick()
            return 0
        end if
    end if
    return DefWindowProcW( hwnd, uMsg, wParam, lParam )
end function


private sub TIP_EnsureClock()
    if gTIP_hClock <> 0 then
        if IsWindow(gTIP_hClock) then exit sub
        gTIP_hClock = 0
    end if

    if gTIP_ClassReg = false then
        dim as WNDCLASSEXW wc
        wc.cbSize        = sizeof(WNDCLASSEXW)
        wc.style         = 0
        wc.lpfnWndProc   = @TIP_ClockWndProc
        wc.hInstance     = GetModuleHandleW(NULL)
        wc.lpszClassName = @wstr("PsTooltipClock")
        if RegisterClassExW( @wc ) = 0 then
            ' A duplicate registration is fine (a second copy of this file in one process);
            ' anything else and the clock simply never runs, which degrades to "no tips"
            ' rather than to a crash.
            if GetLastError() <> ERROR_CLASS_ALREADY_EXISTS then exit sub
        end if
        gTIP_ClassReg = true
    end if

    ' HWND_MESSAGE: never painted, never enumerated, never activated -- it exists only to own
    ' a timer, which needs a window to post WM_TIMER to.
    gTIP_hClock = CreateWindowExW( 0, @wstr("PsTooltipClock"), @wstr(""), 0, _
                                   0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL )
    if gTIP_hClock = 0 then exit sub
    gTIP_ClockMs = PSTOOLTIP_CLOCK_IDLE_MS
    SetTimer( gTIP_hClock, IDT_PSTOOLTIP_CLOCK, gTIP_ClockMs, NULL )
    TIP_Log( "clock started" )
end sub


private sub TIP_ReleaseClock()
    if gTIP_Count > 0 then exit sub          ' still someone to serve
    if gTIP_hClock = 0 then exit sub
    KillTimer( gTIP_hClock, IDT_PSTOOLTIP_CLOCK )
    DestroyWindow( gTIP_hClock )
    gTIP_hClock  = 0
    gTIP_ClockMs = 0
    TIP_Log( "clock stopped" )
end sub


' The dwell poll wants 100 ms; a 120 ms fade at 100 ms would be two visible steps. Raise the
' rate only while something is actually fading, and drop it back the moment nothing is.
sub TIP_ApplyClockRate()
    if gTIP_hClock = 0 then exit sub
    dim as long nWant = PSTOOLTIP_CLOCK_IDLE_MS
    for i as long = 0 to gTIP_Count - 1
        dim as PSTOOLTIP ptr p = GetTooltipPointer( gTIP_Registry(i) )
        if p = 0 then continue for
        if p->nFadePhase <> 0 then
            nWant = PSTOOLTIP_CLOCK_FADE_MS
            exit for
        end if
    next
    if nWant = gTIP_ClockMs then exit sub
    gTIP_ClockMs = nWant
    SetTimer( gTIP_hClock, IDT_PSTOOLTIP_CLOCK, nWant, NULL )
end sub


' ========================================================================================
' PURE FUNCTIONS -- see the contract in PsTooltip.bi. Nothing here touches a window, a timer
' or the clock; that is what makes the placement law, the stem geometry, the fade ramp and
' the show decision assertable over their whole ranges with no message pump.
' ========================================================================================

function PsTooltip_ComputeOrigin( _
            byval rcAnchor as RECT, _
            byval tipW as long, _
            byval tipH as long, _
            byval rcWork as RECT, _
            byval nAlign as long, _
            byval nCursorOffset as long _
            ) as POINT

    dim as long x, y

    select case nAlign
        case TIP_ALIGN_ABOVE
            x = rcAnchor.left
            y = rcAnchor.top - tipH
        case TIP_ALIGN_BELOW
            x = rcAnchor.left
            y = rcAnchor.bottom
        case else                       ' TIP_ALIGN_CURSOR: the anchor is the hotspot
            x = rcAnchor.left
            y = rcAnchor.top + nCursorOffset
    end select

    ' Horizontal: a plain clamp. There is no flip-left case, because no placement mode puts
    ' the tip beside its anchor -- see the TIP_ALIGN_* comment in the header.
    if x + tipW > rcWork.right then x = rcWork.right - tipW
    if x < rcWork.left then x = rcWork.left

    ' Vertical: FLIP past the anchor rather than clamping back over it. PsPopupMenu's rule,
    ' and it matters here for the same reason it mattered there -- a tip clamped up over its
    ' own anchor sits between the user and the thing they are pointing at.
    if nAlign = TIP_ALIGN_ABOVE then
        if y < rcWork.top then
            dim as long yBelow = rcAnchor.bottom
            if yBelow + tipH <= rcWork.bottom then
                y = yBelow
            else
                y = rcWork.top
            end if
        end if
    else
        if y + tipH > rcWork.bottom then
            dim as long yAbove = rcAnchor.top - tipH
            if yAbove >= rcWork.top then
                y = yAbove
            else
                y = rcWork.bottom - tipH
            end if
        end if
    end if

    ' A FULL clamp on BOTH edges, after the flip -- not just the `y < top` half.
    '
    ' The flip places the tip relative to the ANCHOR, and an anchor is not guaranteed to be
    ' inside the work area: here it is usually the CURSOR, which sits happily over a taskbar or
    ' on a second monitor whose work rect this is not. Anchor y = 855 against a work area
    ' ending at 850 flips to 805, which is >= work.top and so passed the old half-clamp -- and
    ' put the tip's bottom 5px past the edge. PsPopupMenu carries the same shape and never
    ' meets it, because a menu's anchor is always an on-screen control.
    '
    ' Bottom first, then top, so a tip TALLER than the work area pins to the top rather than
    ' hanging off it.
    if y + tipH > rcWork.bottom then y = rcWork.bottom - tipH
    if y < rcWork.top then y = rcWork.top

    dim as POINT pt
    pt.x = x
    pt.y = y
    return pt
end function


' ONE RULE COVERS ALL SIX CASES (three alignments x flipped or not): the stem leaves the TOP
' edge exactly when the tip ended up at or below its anchor's top. Deriving it from the
' RESULT rather than from the requested alignment is what makes the flip cases fall out for
' free instead of needing their own branches.
function PsTooltip_StemOnTop( _
            byval rcAnchor as RECT, _
            byval ptOrigin as POINT, _
            byval nAlign as long _
            ) as boolean
    if ptOrigin.y >= rcAnchor.top then return true
    return false
end function


function PsTooltip_ComputeStem( _
            byval cxClient as long, _
            byval cyClient as long, _
            byval bStemOnTop as boolean, _
            byval nStemW as long, _
            byval nStemH as long, _
            byval nTargetX as long, _
            byval nCurvature as long, _
            byval pts as POINT ptr _
            ) as boolean

    if pts = 0 then return false
    if nStemW < 2 then return false
    if nStemH < 1 then return false

    dim as long nHalf = nStemW \ 2
    ' nCurvature is a DIAMETER (GDI's vocabulary, kept throughout this family), so the corner
    ' radius is half of it. Keeping the stem's base clear of both corners is what stops it
    ' growing out of the curve where there is no straight edge to leave from.
    dim as long nRadius = nCurvature \ 2
    dim as long nMinBx  = nRadius + nHalf
    dim as long nMaxBx  = cxClient - nRadius - nHalf
    if nMinBx > nMaxBx then return false        ' too narrow to seat a stem at all

    dim as long bx = nTargetX
    if bx < nMinBx then bx = nMinBx
    if bx > nMaxBx then bx = nMaxBx

    if bStemOnTop then
        pts[0] = type<POINT>( bx - nHalf, nStemH )
        pts[1] = type<POINT>( bx,         0 )
        pts[2] = type<POINT>( bx + nHalf, nStemH )
    else
        pts[0] = type<POINT>( bx - nHalf, cyClient - nStemH )
        pts[1] = type<POINT>( bx,         cyClient )
        pts[2] = type<POINT>( bx + nHalf, cyClient - nStemH )
    end if
    return true
end function


function PsTooltip_ComputeAlpha( _
            byval nElapsed as long, _
            byval nFadeMs as long, _
            byval bFadingIn as boolean _
            ) as long

    if nFadeMs <= 0 then
        if bFadingIn then return 255
        return 0
    end if

    dim as long t = nElapsed
    if t < 0 then t = 0
    if t > nFadeMs then t = nFadeMs

    dim as long a = (255 * t) \ nFadeMs
    if bFadingIn then return a
    return 255 - a
end function


' Written as a ladder rather than one andalso chain on purpose: FreeBASIC's warning 38 fires
' on any andalso chain mixing bare booleans with anything else, and the always-safe shape is
' to test each one alone (Learnings.md).
function PsTooltip_ShouldShow( _
            byval bOver as boolean, _
            byval bResting as boolean, _
            byval bHasText as boolean, _
            byval bButtonDown as boolean, _
            byval bSuppressed as boolean _
            ) as boolean
    if bOver = false then return false
    if bResting = false then return false
    if bHasText = false then return false
    if bButtonDown then return false
    if bSuppressed then return false
    return true
end function


' ========================================================================================
' LAYOUT
'
' Sizes the tip to its content. Nothing here depends on the window's current size or on where
' the tip is going to end up -- layout is INTRINSIC, placement is EXTRINSIC, and keeping them
' apart is what lets the whole placement law live in a pure function.
'
' CONSEQUENCE WORTH KNOWING: rcGlyph / rcTitle / rcText are stored BODY-LOCAL, with the body's
' top-left at (0,0). They become client-absolute only when the stem side is known, which is a
' placement fact. PsTooltip_Render and PsTooltip_GetPartRect are the two places that apply the
' offset; everything else should read them through those.
' ========================================================================================
sub PSTOOLTIP.LayoutTooltip()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' could not measure; try again at the next consume
        exit sub
    end if

    ' Measure with a deterministic font -- the caller's if set, else the stock GUI font, never
    ' "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hTextUse = this.hTextFont
    if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hTitleUse = this.hTitleFont
    if hTitleUse = 0 then hTitleUse = hTextUse

    dim as long nBorder = this.nBorderWidth

    ' The message. DT_WORDBREAK only when a max width was set: with no cap the natural width
    ' is what comctl32 gives, and only embedded newlines break a line.
    dim as DWORD dwFlags = DT_LEFT or DT_TOP or DT_NOPREFIX
    dim as long nWrapW = 0
    if this.nMaxWidth > 0 then
        nWrapW = this.nMaxWidth - (this.nPadX * 2) - (nBorder * 2)
        if nWrapW < 16 then nWrapW = 16
        dwFlags or= DT_WORDBREAK
    end if
    this.nTextFlags = dwFlags

    dim as HFONT hOldFont = SelectObject( hDC, hTextUse )

    dim as RECT rcCalc
    dim as long nTextW = 0, nTextH = 0
    if len( this.wszTipBuf ) then
        SetRect( @rcCalc, 0, 0, nWrapW, 0 )
        DrawText( hDC, this.wszTipBuf.vptr, -1, cast(lpRect, @rcCalc), dwFlags or DT_CALCRECT )
        nTextW = rcCalc.right - rcCalc.left
        nTextH = rcCalc.bottom - rcCalc.top
    end if

    dim as long nTitleW = 0, nTitleH = 0
    if len( this.wszTitle ) then
        SelectObject( hDC, hTitleUse )
        SetRect( @rcCalc, 0, 0, 0, 0 )
        DrawText( hDC, this.wszTitle.vptr, -1, cast(lpRect, @rcCalc), _
                  DT_LEFT or DT_TOP or DT_NOPREFIX or DT_SINGLELINE or DT_CALCRECT )
        nTitleW = rcCalc.right - rcCalc.left
        nTitleH = rcCalc.bottom - rcCalc.top
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' THE GLYPH CELL IS DECLARED, NOT MEASURED -- PsIconPanel's rule. The author owns the
    ' space the icon gets, so a glyph that is missing from the font (or is a different size
    ' from the last one) cannot move the layout underneath the text.
    dim as long nGlyphW = 0, nGlyphH = 0
    if len( this.wszGlyph ) then
        nGlyphW = this.nGlyphCell
        nGlyphH = this.nGlyphCell
    end if

    dim as boolean bTitleRow = false
    if nGlyphW > 0 then bTitleRow = true
    if nTitleW > 0 then bTitleRow = true

    dim as long nRowW = 0, nRowH = 0
    if bTitleRow then
        nRowW = nTitleW
        if nGlyphW > 0 then
            nRowW += nGlyphW
            ' The gap is charged only when there is something on BOTH sides of it -- PsButton's
            ' rule. A glyph with no title does not carry an inset around for nothing.
            if nTitleW > 0 then nRowW += this.nGlyphGap
        end if
        nRowH = nTitleH
        if nGlyphH > nRowH then nRowH = nGlyphH
    end if

    dim as long nContentW = nTextW
    if nRowW > nContentW then nContentW = nRowW

    dim as long nContentH = nRowH
    if nTextH > 0 then
        if nRowH > 0 then nContentH += this.nTitleGap
        nContentH += nTextH
    end if

    dim as long nBodyW = nContentW + (this.nPadX * 2) + (nBorder * 2)
    dim as long nBodyH = nContentH + (this.nPadY * 2) + (nBorder * 2)

    ' nMaxWidth caps the TEXT WRAP, not the window: the width is a max() over the bands. A
    ' title longer than the cap legitimately widens the tip. PsMessageBox's SetMaxWidth has
    ' exactly this contract and for the same reason -- clamping here would clip the title
    ' instead of the thing the cap was aimed at.
    dim as long nStemBand = 0
    if this.nStyle = TIP_STYLE_BALLOON then nStemBand = this.nStemH

    this.nIdealW = nBodyW
    this.nIdealH = nBodyH + nStemBand

    ' --- band rects, BODY-LOCAL (body top-left = 0,0) ---
    SetRect( @this.rcGlyph, 0, 0, 0, 0 )
    SetRect( @this.rcTitle, 0, 0, 0, 0 )
    SetRect( @this.rcText,  0, 0, 0, 0 )

    dim as long x0 = nBorder + this.nPadX
    dim as long y0 = nBorder + this.nPadY

    if bTitleRow then
        if nGlyphW > 0 then
            dim as long gy = y0 + ((nRowH - nGlyphH) \ 2)
            SetRect( @this.rcGlyph, x0, gy, x0 + nGlyphW, gy + nGlyphH )
        end if
        if nTitleW > 0 then
            dim as long tx = x0
            if nGlyphW > 0 then tx += nGlyphW + this.nGlyphGap
            dim as long ty = y0 + ((nRowH - nTitleH) \ 2)
            ' The title gets the SPAN out to the content's right edge, not a tight box round
            ' the glyphs, so DT_END_ELLIPSIS has somewhere to ellipsize into. PsButton's rcText
            ' contract -- and the natural thing to get wrong when painting a background behind
            ' "the title".
            SetRect( @this.rcTitle, tx, ty, x0 + nContentW, ty + nTitleH )
        end if
    end if

    if nTextH > 0 then
        dim as long ty = y0
        if nRowH > 0 then ty += nRowH + this.nTitleGap
        SetRect( @this.rcText, x0, ty, x0 + nContentW, ty + nTextH )
    end if
end sub


sub PSTOOLTIP.Refresh()
    this.bLayoutDirty = true
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' Refresh() alone is NOT enough when what changed is the text SOURCE. The resolved message
' lives in wszTipBuf, and the show path and GetIdealSize both reuse a non-empty one rather
' than re-running the host callback on every tick -- so a stale buffer would have the tip
' measure and display the PREVIOUS item's text while every layout number looked correct.
' Only the three setters that can change what the text would be call this.
private sub TIP_InvalidateText( byval pTip as PSTOOLTIP ptr )
    if pTip = 0 then exit sub
    pTip->wszTipBuf = ""
    pTip->Refresh()
end sub


' Body-local -> client-absolute. The one place the offset is applied, plus GetPartRect.
private sub TIP_BandRects( byval pTip as PSTOOLTIP ptr, _
                           byref rcG as RECT, byref rcTi as RECT, byref rcTx as RECT )
    rcG  = pTip->rcGlyph
    rcTi = pTip->rcTitle
    rcTx = pTip->rcText
    dim as long dx = pTip->rcBody.left
    dim as long dy = pTip->rcBody.top
    if IsRectEmpty( @rcG  ) = 0 then OffsetRect( @rcG,  dx, dy )
    if IsRectEmpty( @rcTi ) = 0 then OffsetRect( @rcTi, dx, dy )
    if IsRectEmpty( @rcTx ) = 0 then OffsetRect( @rcTx, dx, dy )
end sub


' ========================================================================================
' TEXT RESOLUTION -- authored text wins, then the callback, then nothing.
'
' NO CAPTION FALLBACK, deliberately: see the TIP_TooltipCallbackFunc header for why that is a
' real behaviour change from four of the controls this replaces, and a wanted one.
' ========================================================================================
private function TIP_ResolveText( byval pTip as PSTOOLTIP ptr ) as DWSTRING
    if pTip = 0 then return ""
    if len( pTip->wszText ) then
        dim as DWSTRING wszOut = pTip->wszText
        return wszOut
    end if
    if pTip->TooltipCallback then
        return pTip->TooltipCallback( pTip->hWin, pTip->hAttached, pTip->nToolIndex )
    end if
    return ""
end function


' Could this tip have text, WITHOUT running the callback?
'
' The callback is a pull that must fire only when a tip is genuinely about to appear -- not on
' every 100 ms tick that decides not to show one. So a tip with a callback installed answers
' "maybe" here, and the show path resolves for real and quietly abandons the show if the
' answer comes back empty.
private function TIP_HasText( byval pTip as PSTOOLTIP ptr ) as boolean
    if pTip = 0 then return false
    if len( pTip->wszText ) then return true
    if pTip->TooltipCallback then return true
    return false
end function


' ========================================================================================
' RENDERING
'
' SPLIT FROM WM_PAINT ON PURPOSE. PsTooltip_Render takes a buffer, and a thin WM_PAINT wrapper
' owns it. That is what lets the probes drive the exact path WM_PAINT uses: PsDatePicker
' shipped a tone probe that passed at 59 tones while its popup rendered as one flat blue
' rectangle, because the probe drove a per-cell host callback and never ran the real painter.
' ========================================================================================
private sub TIP_DefaultRender( byval pTip as PSTOOLTIP ptr, byval p as PSTOOLTIP_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b

    ' The stroke thickness AS DRAWN. PaintRoundOutline, PaintLine and PaintPolygon all scale
    ' the pen they are handed, so the raw number is not what ends up on screen -- and the
    ' eraser below has to cover what was actually drawn, not what was asked for.
    dim as long nPenDrawn = p->nBorderWidth
    scope
        dim pWin as CWindow ptr = AfxCWindowPtr( pTip->hWin )
        if pWin then nPenDrawn = pWin->ScaleY( p->nBorderWidth )
        if nPenDrawn < 1 then nPenDrawn = 1
    end scope

    ' 1. The body fill. PaintRoundRect fills without stroking, which is what we want -- the
    '    frame goes on last so it is authoritative over anything a callback drew.
    b->SetBackColor( pTip->colors.BackColor )
    b->PaintRoundRect( @p->rcBody, p->nCurvature )

    ' 2. The frame.
    if p->nBorderWidth > 0 then
        b->SetPenColor( pTip->colors.BorderColor )
        b->PaintRoundOutline( @p->rcBody, p->nCurvature, p->nBorderWidth )
    end if

    ' 3. The stem, as a five-point "house": the triangle plus a skirt reaching nPenDrawn pixels
    '    INTO the body. Filling it after the frame is what erases the frame's edge where the
    '    stem meets it -- without that skirt the balloon reads as a triangle stuck onto a
    '    closed box rather than as one shape. A plain 3-point fill will not do it: its base
    '    edge lies exactly on the frame line, so it covers the line only ambiguously.
    if p->bHasStem then
        dim as long nSkirt = nPenDrawn + 1
        dim as POINT house(0 to 4)
        TIP_StemHouse( pTip, nSkirt, @house(0) )
        b->SetBackColor( pTip->colors.BackColor )
        b->PaintPolygon( @house(0), 5, 0 )          ' fill only -- the skirt must not be outlined

        ' 4. Only the two SLOPED sides are stroked. Stroking the base as well would put the
        '    very line back that step 3 just removed.
        if p->nBorderWidth > 0 then
            b->SetPenColor( pTip->colors.BorderColor )
            b->PaintLine( p->nBorderWidth, p->ptStem(0).x, p->ptStem(0).y, p->ptStem(1).x, p->ptStem(1).y )
            b->PaintLine( p->nBorderWidth, p->ptStem(1).x, p->ptStem(1).y, p->ptStem(2).x, p->ptStem(2).y )
        end if
    end if

    ' 5. Content.
    dim as HFONT hTextUse = pTip->hTextFont
    dim as HFONT hTitleUse = pTip->hTitleFont
    if hTitleUse = 0 then hTitleUse = hTextUse
    dim as HFONT hGlyphUse = pTip->hGlyphFont
    if hGlyphUse = 0 then hGlyphUse = hTextUse

    if len( p->wszGlyph ) then
        if IsRectEmpty( @p->rcGlyph ) = 0 then
            b->SetFont( hGlyphUse )
            b->SetForeColor( pTip->colors.GlyphColor )
            b->PaintText( p->wszGlyph, @p->rcGlyph, DT_CENTER )
        end if
    end if

    if len( p->wszTitle ) then
        if IsRectEmpty( @p->rcTitle ) = 0 then
            b->SetFont( hTitleUse )
            b->SetForeColor( pTip->colors.TitleColor )
            b->PaintText( p->wszTitle, @p->rcTitle, DT_LEFT or DT_END_ELLIPSIS )
        end if
    end if

    if len( p->wszText ) then
        if IsRectEmpty( @p->rcText ) = 0 then
            b->SetFont( hTextUse )
            b->SetForeColor( pTip->colors.ForeColor )
            ' PaintTextEx, not PaintText: PaintText forces DT_VCENTER or DT_SINGLELINE and so
            ' structurally cannot draw wrapped text. These are the SAME flags the width was
            ' measured with -- measuring and painting with different flags is how text that
            ' fits in the arithmetic clips on screen.
            b->PaintTextEx( p->wszText, @p->rcText, p->nTextFlags )
        end if
    end if
end sub


sub PsTooltip_Render( byval pTip as PSTOOLTIP ptr, byval pb as PsBufferPaint ptr )
    if pTip = 0 then exit sub
    if pb = 0 then exit sub

    dim p as PSTOOLTIP_PAINTINFO
    p.hTooltip     = pTip->hWin
    p.hAttached    = pTip->hAttached
    p.b            = pb
    GetClientRect( pTip->hWin, @p.rcClient )
    p.rcBody       = pTip->rcBody
    TIP_BandRects( pTip, p.rcGlyph, p.rcTitle, p.rcText )
    p.ptStem(0)    = pTip->ptStem(0)
    p.ptStem(1)    = pTip->ptStem(1)
    p.ptStem(2)    = pTip->ptStem(2)
    p.bHasStem     = pTip->bHasStem
    p.bStemOnTop   = pTip->bStemOnTop
    p.wszText      = pTip->wszTipBuf
    p.wszTitle     = pTip->wszTitle
    p.wszGlyph     = pTip->wszGlyph
    p.nStyle       = pTip->nStyle
    p.nCurvature   = pTip->nCurvature
    p.nBorderWidth = pTip->nBorderWidth
    p.nToolIndex   = pTip->nToolIndex
    p.nTextFlags   = pTip->nTextFlags

    if pTip->PaintCallback then
        pTip->PaintCallback( @p )
    else
        TIP_DefaultRender( pTip, @p )
    end if
end sub


private function PsTooltip_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hwnd)
    if pTip = 0 then return 0

    if pTip->bLayoutDirty then pTip->LayoutTooltip()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )
    PsTooltip_Render( pTip, @b )
    b.EndDoubleBuffer()
    return 0
end function


' ========================================================================================
' SHOW / HIDE
' ========================================================================================
' Build the five-point "house" -- the stem triangle plus a skirt reaching nSkirt pixels INTO the
' body. Used twice and it must agree with itself both times: once by the painter to fill the
' stem and erase the frame line under its base, once by the window region so the shape and the
' paint cover the same pixels.
sub TIP_StemHouse( byval pTip as PSTOOLTIP ptr, byval nSkirt as long, byval pts as POINT ptr )
    if pts = 0 then exit sub
    if pTip->bStemOnTop then
        pts[0] = pTip->ptStem(1)
        pts[1] = type<POINT>( pTip->ptStem(2).x, pTip->ptStem(2).y )
        pts[2] = type<POINT>( pTip->ptStem(2).x, pTip->ptStem(2).y + nSkirt )
        pts[3] = type<POINT>( pTip->ptStem(0).x, pTip->ptStem(0).y + nSkirt )
        pts[4] = type<POINT>( pTip->ptStem(0).x, pTip->ptStem(0).y )
    else
        pts[0] = pTip->ptStem(1)
        pts[1] = type<POINT>( pTip->ptStem(0).x, pTip->ptStem(0).y )
        pts[2] = type<POINT>( pTip->ptStem(0).x, pTip->ptStem(0).y - nSkirt )
        pts[3] = type<POINT>( pTip->ptStem(2).x, pTip->ptStem(2).y - nSkirt )
        pts[4] = type<POINT>( pTip->ptStem(2).x, pTip->ptStem(2).y )
    end if
end sub


' SHAPE THE WINDOW TO THE BALLOON.
'
' Without this the window is a plain rectangle and everything the painter does not cover shows
' through as whatever the buffer started as -- BLACK. Two regions are affected and one of them
' is glaring: the four rounded corners (small, and nearly invisible against a dark host, which
' is exactly why it survived review) and, in TIP_STYLE_BALLOON, THE WHOLE STEM BAND except the
' stem itself -- a black bar the full width of the tip. It was invisible to every assertion in
' the suite, because the part rects the probes read are all inside the body.
'
' A region rather than a colour key: LWA_COLORKEY would leave an antialiased fringe of the key
' colour around every rounded corner, and it cannot coexist with the LWA_ALPHA the fade needs.
' The cost is that the region's edge is hard, so the outermost antialiased pixel of each corner
' is clipped -- at the default 6px curvature that is not visible, and it buys a drop shadow that
' follows the real silhouette instead of a rectangle.
private sub TIP_ApplyRegion( byval pTip as PSTOOLTIP ptr )
    if pTip = 0 then exit sub
    if pTip->hWin = 0 then exit sub

    ' CreateRoundRectRgn's right/bottom are EXCLUSIVE, and rcBody is already stored that way,
    ' so it is passed through unadjusted. Its ellipse arguments are a width and a height, i.e.
    ' the same DIAMETER vocabulary nCurvature carries everywhere in this family.
    dim as HRGN hRgn = CreateRoundRectRgn( pTip->rcBody.left, pTip->rcBody.top, _
                                           pTip->rcBody.right, pTip->rcBody.bottom, _
                                           pTip->nCurvature, pTip->nCurvature )
    if hRgn = 0 then exit sub

    if pTip->bHasStem then
        dim as POINT house(0 to 4)
        ' One pixel of skirt is enough here: the region only has to REACH the body, and any
        ' more would bulge the silhouette out past the frame.
        TIP_StemHouse( pTip, 1, @house(0) )
        dim as HRGN hStem = CreatePolygonRgn( @house(0), 5, WINDING )
        if hStem then
            CombineRgn( hRgn, hRgn, hStem, RGN_OR )
            DeleteObject( hStem )
        end if
    end if

    ' SetWindowRgn takes OWNERSHIP of the region -- deleting it here would be a double free.
    SetWindowRgn( pTip->hWin, hRgn, FALSE )
end sub


private sub TIP_FinishHide( byval pTip as PSTOOLTIP ptr, byval dwNow as DWORD )
    ShowWindow( pTip->hWin, SW_HIDE )
    pTip->bVisible   = false
    pTip->nFadePhase = 0
    pTip->dwHiddenAt = dwNow
    if gTIP_Visible = pTip->hWin then gTIP_Visible = 0
    if pTip->ShowCallback then
        pTip->ShowCallback( pTip->hWin, pTip->hAttached, pTip->nToolIndex, false )
    end if
end sub


sub TIP_HideCore( byval pTip as PSTOOLTIP ptr, byval bAllowFade as boolean )
    if pTip = 0 then exit sub
    if pTip->bVisible = false then
        pTip->nFadePhase = 0
        exit sub
    end if

    if bAllowFade then
        if pTip->nFadeTime > 0 then
            if pTip->nFadePhase <> 2 then
                pTip->nFadePhase  = 2
                ' Back-date the ramp so it starts from the CURRENT alpha rather than snapping
                ' to opaque first. This is what makes a fade interruptible -- hiding a tip
                ' that is still fading in must continue down from where it got to, not jump.
                dim as long nBack = 0
                if pTip->nFadeTime > 0 then nBack = ((255 - pTip->nAlpha) * pTip->nFadeTime) \ 255
                pTip->dwFadeStart = GetTickCount() - cast(DWORD, nBack)
                TIP_ApplyClockRate()
            end if
            exit sub                    ' the tick finishes the job
        end if
    end if

    TIP_FinishHide( pTip, GetTickCount() )
    TIP_ApplyClockRate()
end sub


function TIP_ShowCore( byval pTip as PSTOOLTIP ptr, byval rcAnchor as RECT, byval nAlign as long ) as boolean
    if pTip = 0 then return false
    if pTip->hWin = 0 then return false

    ' Pull the text NOW -- this is the one place the host callback runs, and it runs only when
    ' a tip is genuinely about to appear.
    pTip->wszTipBuf = TIP_ResolveText( pTip )
    if len( pTip->wszTipBuf ) = 0 then
        if pTip->bVisible then TIP_HideCore( pTip, false )
        return false
    end if

    ' The resolved text is an input to the size, so the layout is always stale by this point.
    pTip->bLayoutDirty = true
    pTip->LayoutTooltip()
    if pTip->bLayoutDirty then return false        ' could not measure (no DC)

    dim as RECT rcTarget = type( rcAnchor.left, rcAnchor.top, _
                                 rcAnchor.left + pTip->nIdealW, rcAnchor.top + pTip->nIdealH )
    dim as MONITORINFO mi
    mi.cbSize = sizeof(MONITORINFO)
    dim as HMONITOR hMon = MonitorFromRect( @rcTarget, MONITOR_DEFAULTTONEAREST )
    dim as RECT rcWork
    if GetMonitorInfo( hMon, @mi ) then
        rcWork = mi.rcWork
    else
        SetRect( @rcWork, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) )
    end if

    ' A BALLOON IN CURSOR MODE NEEDS THE TIP SHIFTED LEFT, or its stem cannot reach the cursor.
    '
    ' Placement puts the tip's LEFT EDGE at the cursor, so the stem's target in client
    ' coordinates is x = 0 -- and ComputeStem clamps it to (radius + half) to keep it clear of
    ' the rounded corner. The stem therefore parks at the corner and points at nothing, every
    ' single time. Moving the anchor left by exactly that clamp distance makes the clamped
    ' position land ON the cursor. Done to the ANCHOR rather than to the result so the work-area
    ' clamp downstream still gets the final say.
    dim as RECT rcPlace = rcAnchor
    if nAlign = TIP_ALIGN_CURSOR then
        if pTip->nStyle = TIP_STYLE_BALLOON then
            dim as long nInset = (pTip->nCurvature \ 2) + (pTip->nStemW \ 2)
            rcPlace.left -= nInset
            rcPlace.right -= nInset
        end if
    end if

    dim as POINT ptOrigin = PsTooltip_ComputeOrigin( rcPlace, pTip->nIdealW, pTip->nIdealH, _
                                                     rcWork, nAlign, GetSystemMetrics(SM_CYCURSOR) )
    pTip->bStemOnTop = PsTooltip_StemOnTop( rcAnchor, ptOrigin, nAlign )

    ' What the stem aims at, in screen coordinates.
    dim as POINT ptT
    if nAlign = TIP_ALIGN_CURSOR then
        ptT.x = rcAnchor.left
        ptT.y = rcAnchor.top
    else
        ptT.x = (rcAnchor.left + rcAnchor.right) \ 2
        if pTip->bStemOnTop then ptT.y = rcAnchor.bottom else ptT.y = rcAnchor.top
    end if
    pTip->ptTarget = ptT

    dim as long nStemBand = 0
    if pTip->nStyle = TIP_STYLE_BALLOON then nStemBand = pTip->nStemH
    if pTip->bStemOnTop then
        SetRect( @pTip->rcBody, 0, nStemBand, pTip->nIdealW, pTip->nIdealH )
    else
        SetRect( @pTip->rcBody, 0, 0, pTip->nIdealW, pTip->nIdealH - nStemBand )
    end if

    pTip->bHasStem = false
    if pTip->nStyle = TIP_STYLE_BALLOON then
        pTip->bHasStem = PsTooltip_ComputeStem( pTip->nIdealW, pTip->nIdealH, pTip->bStemOnTop, _
                                pTip->nStemW, pTip->nStemH, ptT.x - ptOrigin.x, _
                                pTip->nCurvature, @pTip->ptStem(0) )
    end if

    ' The veto runs AFTER the text is resolved and the window sized -- so a host deciding here
    ' can see what would have been shown -- but BEFORE anything reaches the screen.
    if pTip->ShowCallback then
        if pTip->ShowCallback( pTip->hWin, pTip->hAttached, pTip->nToolIndex, true ) = false then
            TIP_Log( "show vetoed by callback" )
            return false
        end if
    end if

    ' Only one tip on screen at a time. Close the other one first, so its hide notification
    ' lands before ours claims the slot.
    if gTIP_Visible <> 0 then
        if gTIP_Visible <> pTip->hWin then
            dim as PSTOOLTIP ptr pOther = GetTooltipPointer( gTIP_Visible )
            if pOther then TIP_HideCore( pOther, false )
        end if
    end if

    dim as DWORD dwNow = GetTickCount()
    if pTip->nFadeTime > 0 then
        if pTip->bVisible then
            ' Already on screen (a re-show, or a fade-out we are reversing): continue from the
            ' current alpha rather than restarting from transparent.
            pTip->dwFadeStart = dwNow - cast(DWORD, (pTip->nAlpha * pTip->nFadeTime) \ 255)
        else
            pTip->nAlpha      = 0
            pTip->dwFadeStart = dwNow
        end if
        pTip->nFadePhase = 1
    else
        pTip->nAlpha     = 255
        pTip->nFadePhase = 0
    end if
    SetLayeredWindowAttributes( pTip->hWin, 0, cast(ubyte, pTip->nAlpha), LWA_ALPHA )

    ' HWND_TOPMOST, not PsPopupMenu's HWND_TOP. A menu rides above its host because it is
    ' OWNED by it; a tooltip has to sit above everything, including other applications'
    ' topmost windows, or it is occluded exactly when it matters.
    SetWindowPos( pTip->hWin, HWND_TOPMOST, ptOrigin.x, ptOrigin.y, pTip->nIdealW, pTip->nIdealH, _
                  SWP_NOACTIVATE or SWP_SHOWWINDOW )
    ' Shape the window to the balloon. AFTER the SetWindowPos, not before: see TIP_ApplyRegion.
    TIP_ApplyRegion( pTip )
    InvalidateRect( pTip->hWin, NULL, TRUE )    ' a same-size re-show generates no WM_PAINT

    pTip->bVisible  = true
    pTip->dwShownAt = dwNow
    gTIP_Visible    = pTip->hWin
    TIP_ApplyClockRate()

    dim as string s = "show at " & ptOrigin.x & "," & ptOrigin.y & _
                      " size " & pTip->nIdealW & "x" & pTip->nIdealH & _
                      " stemOnTop=" & pTip->bStemOnTop & " hasStem=" & pTip->bHasStem
    TIP_Log( s )
    return true
end function


' ========================================================================================
' THE TICK
' ========================================================================================
private function TIP_AnyButtonDown() as boolean
    if (GetAsyncKeyState(VK_LBUTTON) and &h8000) <> 0 then return true
    if (GetAsyncKeyState(VK_RBUTTON) and &h8000) <> 0 then return true
    if (GetAsyncKeyState(VK_MBUTTON) and &h8000) <> 0 then return true
    return false
end function


' Is the cursor over the attached control, and is that control actually the thing on top?
'
' The WindowFromPoint test is what stops a tip appearing for a control that is covered by
' something else. Our own tip window answers WM_NCHITTEST with HTTRANSPARENT, so it is
' invisible to this search -- which is exactly why a tip overlapping its own control does not
' immediately hide itself.
private function TIP_CursorOverAttached( byval pTip as PSTOOLTIP ptr, byval ptCur as POINT ) as boolean
    dim as HWND hA = pTip->hAttached
    if hA = 0 then return false
    if IsWindow( hA ) = 0 then return false
    ' IsWindowVisible walks the whole ancestor chain, which is what we want here: a control on
    ' a hidden page must not tip. (It is also the trap PsScrollPanel recorded, where the same
    ' behaviour was NOT what was wanted.)
    if IsWindowVisible( hA ) = 0 then return false

    dim as RECT rc
    GetWindowRect( hA, @rc )
    if PtInRect( @rc, ptCur ) = 0 then return false

    dim as HWND hUnder = WindowFromPoint( ptCur )
    if hUnder = hA then return true
    ' The hit landed on a child of the attached control -- still our tool.
    if IsChild( hA, hUnder ) then return true
    ' THE HIT LANDED ON AN ANCESTOR OF THE ATTACHED CONTROL, which means the control is
    ' hit-test TRANSPARENT and WindowFromPoint looked straight through it. A plain STATIC does
    ' exactly this unless it carries SS_NOTIFY -- and a label is one of the most obvious things
    ' anyone will ever attach a tip to, so rejecting it here would make the control look broken
    ' for the simplest possible case. The PtInRect test above has already established the
    ' cursor is inside hA, and anything genuinely covering hA would have come back as that
    ' window rather than as hA's parent.
    if IsChild( hUnder, hA ) then return true
    return false
end function


' ONE TICK OF THE STATE MACHINE.
'
' Every input it needs is a PARAMETER, including bOver -- the real clock computes that from
' WindowFromPoint and the attached window's rect, but nothing in here does. That is what makes
' the whole machine drivable from a test with no message pump and no real mouse
' (PsTooltip_TickForTest), which matters because this is where the dwell, the reshow-versus-
' initial delay choice, click suppression, auto-pop, the fade and the veto rule all live -- and
' none of it was reachable by an assertion until the seam existed.
private sub TIP_TickOne( byval pTip as PSTOOLTIP ptr, byval ptCur as POINT, _
                         byval bOver as boolean, _
                         byval bBtn as boolean, byval dwNow as DWORD )

    ' --- advance any live fade first, so a hide decided below acts on current alpha ---
    if pTip->nFadePhase <> 0 then
        dim as boolean bFadeIn = false
        if pTip->nFadePhase = 1 then bFadeIn = true
        dim as long nElapsed = cast(long, dwNow - pTip->dwFadeStart)
        dim as long nA = PsTooltip_ComputeAlpha( nElapsed, pTip->nFadeTime, bFadeIn )
        if nA <> pTip->nAlpha then
            pTip->nAlpha = nA
            SetLayeredWindowAttributes( pTip->hWin, 0, cast(ubyte, nA), LWA_ALPHA )
        end if
        if nElapsed >= pTip->nFadeTime then
            pTip->nFadePhase = 0
            if bFadeIn = false then TIP_FinishHide( pTip, dwNow )
        end if
    end if

    if pTip->hAttached = 0 then exit sub
    if IsWindow( pTip->hAttached ) = 0 then
        ' The attached control went away without a Detach. Stand down rather than keep
        ' probing a dead handle.
        pTip->hAttached = 0
        if pTip->bVisible then TIP_HideCore( pTip, false )
        exit sub
    end if

    if bOver = false then
        ' Leaving the tool is what clears the click suppression -- comctl32's rule. A tip must
        ' not reappear over the thing you just clicked while the cursor is still sitting on it.
        pTip->bSuppressed = false
        pTip->dwRestSince = 0
        if pTip->bVisible then TIP_HideCore( pTip, true )
        exit sub
    end if

    if bBtn then
        pTip->bSuppressed = true
        pTip->dwRestSince = 0
        if pTip->bVisible then TIP_HideCore( pTip, true )
        exit sub
    end if

    ' --- movement, against the jitter tolerance ---
    dim as long dx = ptCur.x - pTip->ptLastCursor.x
    dim as long dy = ptCur.y - pTip->ptLastCursor.y
    if dx < 0 then dx = -dx
    if dy < 0 then dy = -dy
    dim as boolean bMoved = false
    if dx > pTip->nMoveTolerance then bMoved = true
    if dy > pTip->nMoveTolerance then bMoved = true
    if bMoved then
        pTip->ptLastCursor = ptCur
        ' Restart the dwell only while HIDDEN. Once a tip is up, moving inside the same tool
        ' neither moves it nor dismisses it -- comctl32's behaviour, and the alternative (a
        ' tip that chases the cursor) is visually noisy for no gain.
        if pTip->bVisible = false then pTip->dwRestSince = dwNow
    end if

    if pTip->bVisible then
        if pTip->nAutoPopDelay > 0 then
            if cast(long, dwNow - pTip->dwShownAt) >= pTip->nAutoPopDelay then
                TIP_HideCore( pTip, true )
                ' Suppress until the cursor leaves, or the dwell would simply put it straight
                ' back up and the auto-pop would mean nothing.
                pTip->bSuppressed = true
            end if
        end if
        exit sub
    end if

    if pTip->dwRestSince = 0 then
        pTip->dwRestSince  = dwNow
        pTip->ptLastCursor = ptCur
        exit sub
    end if

    ' A tip shown and dismissed a moment ago comes back on the SHORT delay -- moving along a
    ' row of buttons should not re-serve the full initial dwell at every one.
    dim as long nDelay = pTip->nInitialDelay
    if pTip->dwHiddenAt <> 0 then
        if cast(long, dwNow - pTip->dwHiddenAt) < (pTip->nInitialDelay * 2) then
            nDelay = pTip->nReshowDelay
        end if
    end if

    dim as boolean bResting = false
    if cast(long, dwNow - pTip->dwRestSince) >= nDelay then bResting = true

    if PsTooltip_ShouldShow( bOver, bResting, TIP_HasText(pTip), bBtn, pTip->bSuppressed ) = false then exit sub

    ' A REFUSED SHOW SUPPRESSES THE REST OF THIS HOVER -- it does not get retried 100 ms later.
    '
    ' Without this the dwell conditions are all still true on the very next tick, so the attempt
    ' repeats immediately: a ShowCallback veto held for exactly one tick and the tip appeared
    ' anyway a tenth of a second late, and a TooltipCallback returning "" was re-consulted ten
    ' times a second for as long as the cursor rested. Both read as "the callback is ignored".
    '
    ' Suppression is cleared where all the other suppressions are -- when the cursor leaves the
    ' control -- so "no tip for this hover" is the unit, which is what a host refusing one
    ' actually means. A host that wants to re-offer a tip without the cursor leaving changes the
    ' tool index, which resets the dwell.
    dim as RECT rcAnchor = type( ptCur.x, ptCur.y, ptCur.x, ptCur.y )
    if TIP_ShowCore( pTip, rcAnchor, TIP_ALIGN_CURSOR ) = false then
        pTip->bSuppressed = true
    end if
end sub


sub TIP_ClockTick()
    if gTIP_Count = 0 then exit sub

    dim as POINT ptCur
    GetCursorPos( @ptCur )
    dim as boolean bBtn  = TIP_AnyButtonDown()
    dim as DWORD   dwNow = GetTickCount()

    ' BACKWARDS, because a dead entry is evicted in place and TIP_RegistryRemove compacts:
    ' compaction only shifts entries above the removal point, all already visited.
    for i as long = gTIP_Count - 1 to 0 step -1
        if i >= gTIP_Count then continue for
        dim as HWND h = gTIP_Registry(i)
        dim as PSTOOLTIP ptr pTip = GetTooltipPointer( h )
        if pTip = 0 then
            TIP_RegistryRemove( h )
            continue for
        end if
        TIP_TickOne( pTip, ptCur, TIP_CursorOverAttached( pTip, ptCur ), bBtn, dwNow )
    next

    TIP_ApplyClockRate()
end sub


' ========================================================================================
' WINDOW PROCEDURE
'
' Deliberately tiny. The tip never activates, never takes focus, and is click-through, so
' almost nothing arrives here -- all the behaviour lives on the shared clock.
' ========================================================================================
private function TIP_CallMessage( byval pTip as PSTOOLTIP ptr, byval hwnd as HWND, byval uMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM ) as boolean
    if pTip = 0 then return false
    if pTip->MessageCallback = 0 then return false
    dim m as PSTOOLTIP_MESSAGEINFO
    m.hTooltip  = hwnd
    m.hAttached = pTip->hAttached
    m.uMsg      = uMsg
    m.wParam    = wParam
    m.lParam    = lParam
    return pTip->MessageCallback( @m )
end function


private function PsTooltip_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                    byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT

    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hwnd)

    select case uMsg

    case WM_PAINT
        if TIP_CallMessage( pTip, hwnd, uMsg, wParam, lParam ) then return 0
        return PsTooltip_OnPaint( hwnd )

    ' Never take activation -- with it would go the host's focus and its active title bar.
    case WM_MOUSEACTIVATE
        return MA_NOACTIVATE

    ' CLICK-THROUGH. A tooltip must never eat a click: HTTRANSPARENT sends the hit test on to
    ' whatever is underneath. It also keeps the tip invisible to the WindowFromPoint test in
    ' TIP_CursorOverAttached, so a tip overlapping its own control does not hide itself.
    case WM_NCHITTEST
        return HTTRANSPARENT

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer.
        return true

    case WM_DESTROY
        ' Result deliberately ignored -- see the MessageCallback header.
        TIP_CallMessage( pTip, hwnd, uMsg, wParam, lParam )
        if pTip then
            if gTIP_Visible = hwnd then gTIP_Visible = 0
            TIP_RegistryRemove( hwnd )
            TIP_ReleaseClock()
        end if

    case WM_NCDESTROY
        TIP_CallMessage( pTip, hwnd, uMsg, wParam, lParam )
        if pTip then
            ' The three fonts are the host's and are left alone. Nothing else here is owned.
            TIP_RegistryRemove( hwnd )      ' belt and braces: WM_DESTROY may not have run
            pTip->dwSignature = 0           ' so a stale handle cannot pass GetTooltipPointer
            delete(pTip)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    case else
        if TIP_CallMessage( pTip, hwnd, uMsg, wParam, lParam ) then return 0

    end select

    return DefWindowProcW( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' PUBLIC API -- lifecycle
' ========================================================================================
function PsTooltip_Create( byval hAttach as HWND, byval CtrlID as long = 0 ) as HWND
    if hAttach = 0 then return 0
    if IsWindow(hAttach) = 0 then return 0

    ' The popup is OWNED by the attached control's top-level window, so it z-orders above it,
    ' hides with it on minimize, and is destroyed with it if the host forgets.
    dim as HWND hOwner = GetAncestor( hAttach, GA_ROOT )
    if hOwner = 0 then hOwner = hAttach

    dim pWindow as CWindow ptr = new CWindow
    if pWindow = 0 then return 0

    ' WS_EX_LAYERED is the family first here, and it is what the fade is built on.
    dim as HWND hCtrl = pWindow->Create( hOwner, "", @PsTooltip_WndProc, _
            0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_TOOLWINDOW or WS_EX_NOACTIVATE or WS_EX_LAYERED )
    if hCtrl = 0 then
        delete pWindow
        return 0
    end if

    ' Replace the class style as every sibling does, dropping CS_HREDRAW/CS_VREDRAW (which only
    ' add flicker) and adding the system drop shadow. CWindow registers a UNIQUE class per
    ' instance, so this cannot leak to another window. NO CS_DBLCLKS: the tip is click-through
    ' and can never receive one.
    pWindow->ClassStyle = CS_DROPSHADOW

    dim as PSTOOLTIP ptr pTip = new PSTOOLTIP
    if pTip = 0 then
        DestroyWindow( hCtrl )
        delete pWindow
        return 0
    end if

    pTip->hWin        = hCtrl
    pTip->idc_Tooltip = CtrlID

    ' DPI-scale every geometry default ONCE, here. Public setters take RAW pixels afterwards
    ' and the caller scales -- the family's convention.
    pTip->nPadX       = pWindow->ScaleX( PSTOOLTIP_DEFAULT_PADX )
    pTip->nPadY       = pWindow->ScaleY( PSTOOLTIP_DEFAULT_PADY )
    pTip->nCurvature  = pWindow->ScaleX( PSTOOLTIP_DEFAULT_CURVATURE )
    pTip->nStemW      = pWindow->ScaleX( PSTOOLTIP_DEFAULT_STEMW )
    pTip->nStemH      = pWindow->ScaleY( PSTOOLTIP_DEFAULT_STEMH )
    pTip->nGlyphCell  = pWindow->ScaleX( PSTOOLTIP_DEFAULT_GLYPHCELL )
    pTip->nGlyphGap   = pWindow->ScaleX( PSTOOLTIP_DEFAULT_GLYPHGAP )
    pTip->nTitleGap   = pWindow->ScaleY( PSTOOLTIP_DEFAULT_TITLEGAP )
    pTip->nMoveTolerance = pWindow->ScaleX( PSTOOLTIP_DEFAULT_MOVETOL )

    ' THE BORDER IS NOT PRE-SCALED, and neither is anything else handed to a pen. PaintLine,
    ' PaintRoundOutline and PaintPolygon all scale the pen width they are given, so scaling it
    ' here as well would double it at any DPI above 100% -- PsOptionButton's decision, reached
    ' the hard way and confirmed by its 175% pass.
    pTip->nBorderWidth = 1

    ' comctl32's own derivation of the three delays. A tip that appears on a different beat
    ' from every other tip on the machine reads as broken.
    dim as long nDbl = GetDoubleClickTime()
    if nDbl <= 0 then nDbl = 500
    pTip->nInitialDelay = nDbl
    pTip->nAutoPopDelay = nDbl * 10
    pTip->nReshowDelay  = nDbl \ 5
    pTip->nFadeTime     = PSTOOLTIP_DEFAULT_FADEMS

    pTip->nStyle    = TIP_STYLE_RECT
    pTip->nAlpha    = 255
    pTip->bVisible  = false

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )
    pWindow->UserData(0) = cast(LONG_PTR, pTip)

    ' A layered window with no attributes ever set can render as nothing at all, so establish
    ' opaque before the first show.
    SetLayeredWindowAttributes( hCtrl, 0, 255, LWA_ALPHA )

    ' AFTER every derived value above, so an armed default overrides the derivation and an
    ' unarmed one leaves it standing. This is the only reason a control's lazily-created tip
    ' can carry the host's theme without the host ever seeing its HWND.
    PsTooltip_ApplyDefaults( hCtrl )

    TIP_RegistryAdd( hCtrl )
    PsTooltip_Attach( hCtrl, hAttach )
    return hCtrl
end function


sub PsTooltip_Attach( byval hTooltip as HWND, byval hAttach as HWND )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pTip->bVisible then TIP_HideCore( pTip, false )
    pTip->hAttached   = hAttach
    pTip->dwRestSince = 0
    pTip->bSuppressed = false
    if hAttach <> 0 then TIP_EnsureClock()
end sub


sub PsTooltip_Detach( byval hTooltip as HWND )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pTip->bVisible then TIP_HideCore( pTip, false )
    pTip->hAttached   = 0
    pTip->dwRestSince = 0
end sub


function PsTooltip_GetAttached( byval hTooltip as HWND ) as HWND
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return 0
    return pTip->hAttached
end function


' ========================================================================================
' PUBLIC API -- text
' ========================================================================================
sub PsTooltip_SetText( byval hTooltip as HWND, byval wszText as DWSTRING )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->wszText = wszText
    TIP_InvalidateText( pTip )
end sub

function PsTooltip_GetText( byval hTooltip as HWND ) as DWSTRING
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return ""
    dim as DWSTRING wszOut = pTip->wszText
    return wszOut
end function

sub PsTooltip_SetTitle( byval hTooltip as HWND, byval wszTitle as DWSTRING )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->wszTitle = wszTitle
    pTip->Refresh()
end sub

function PsTooltip_GetTitle( byval hTooltip as HWND ) as DWSTRING
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return ""
    dim as DWSTRING wszOut = pTip->wszTitle
    return wszOut
end function

sub PsTooltip_SetGlyph( byval hTooltip as HWND, byval wszGlyph as DWSTRING )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->wszGlyph = wszGlyph
    pTip->Refresh()
end sub

function PsTooltip_GetGlyph( byval hTooltip as HWND ) as DWSTRING
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return ""
    dim as DWSTRING wszOut = pTip->wszGlyph
    return wszOut
end function


' The per-item selector, and the one call an attached control has to make. Changing it while a
' tip is up hides that tip so the next dwell re-resolves for the new item -- the same job
' TTM_POP does at the same call site in the six per-item controls this replaces.
'
' A NO-OP SET IS FREE. Hosts call this from their WM_MOUSEMOVE, which re-runs constantly, so a
' setter that reset state unconditionally would keep tearing down a tip that should stay up --
' PsHScrollBar's SetWheelStep bug (Learnings.md), applied before it could happen again.
sub PsTooltip_SetToolIndex( byval hTooltip as HWND, byval idx as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pTip->nToolIndex = idx then exit sub
    pTip->nToolIndex = idx
    TIP_InvalidateText( pTip )
    if pTip->bVisible then
        ' Hide WITHOUT a fade: the tip is about to come back on the short reshow delay, and
        ' fading out only to fade straight back in reads as a flicker.
        TIP_HideCore( pTip, false )
    end if
    pTip->dwRestSince = 0
    ' A new item is a new offer: clear any suppression left by a refused show, a click or an
    ' auto-pop on the PREVIOUS item. Without this, one vetoed cell would silence every other
    ' cell in the same control until the cursor left it entirely.
    pTip->bSuppressed = false
end sub

function PsTooltip_GetToolIndex( byval hTooltip as HWND ) as long
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return -1
    return pTip->nToolIndex
end function


' ========================================================================================
' PUBLIC API -- appearance
' ========================================================================================
sub PsTooltip_SetColors( byval hTooltip as HWND, byval pColors as PSTOOLTIP_COLORS ptr )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pColors = 0 then exit sub
    pTip->colors = *pColors
    if pTip->hWin then InvalidateRect( pTip->hWin, NULL, TRUE )
end sub

sub PsTooltip_GetColors( byval hTooltip as HWND, byval pColors as PSTOOLTIP_COLORS ptr )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pColors = 0 then exit sub
    *pColors = pTip->colors
end sub

sub PsTooltip_SetFonts( byval hTooltip as HWND, byval hText as HFONT, _
                        byval hTitle as HFONT = 0, byval hGlyph as HFONT = 0 )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->hTextFont  = hText
    pTip->hTitleFont = hTitle
    pTip->hGlyphFont = hGlyph
    pTip->Refresh()
end sub

sub PsTooltip_SetStyle( byval hTooltip as HWND, byval nStyle as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pTip->nStyle = nStyle then exit sub
    pTip->nStyle = nStyle
    pTip->Refresh()
end sub

function PsTooltip_GetStyle( byval hTooltip as HWND ) as long
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return TIP_STYLE_RECT
    return pTip->nStyle
end function

sub PsTooltip_SetMaxWidth( byval hTooltip as HWND, byval nMaxWidth as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if pTip->nMaxWidth = nMaxWidth then exit sub
    pTip->nMaxWidth = nMaxWidth
    pTip->Refresh()
end sub

function PsTooltip_GetMaxWidth( byval hTooltip as HWND ) as long
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return 0
    return pTip->nMaxWidth
end function

sub PsTooltip_SetPadding( byval hTooltip as HWND, byval nX as long, byval nY as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nX >= 0 then pTip->nPadX = nX
    if nY >= 0 then pTip->nPadY = nY
    pTip->Refresh()
end sub

sub PsTooltip_SetCurvature( byval hTooltip as HWND, byval nCurvature as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nCurvature < 0 then nCurvature = 0
    pTip->nCurvature = nCurvature
    pTip->Refresh()
end sub

sub PsTooltip_SetBorderWidth( byval hTooltip as HWND, byval nBorderWidth as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nBorderWidth < 0 then nBorderWidth = 0
    pTip->nBorderWidth = nBorderWidth
    pTip->Refresh()
end sub

sub PsTooltip_SetStemSize( byval hTooltip as HWND, byval nW as long, byval nH as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nW >= 0 then pTip->nStemW = nW
    if nH >= 0 then pTip->nStemH = nH
    pTip->Refresh()
end sub

sub PsTooltip_SetGlyphCell( byval hTooltip as HWND, byval nCell as long, byval nGap as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nCell >= 0 then pTip->nGlyphCell = nCell
    if nGap  >= 0 then pTip->nGlyphGap  = nGap
    pTip->Refresh()
end sub


' ========================================================================================
' PUBLIC API -- timing
' ========================================================================================
sub PsTooltip_SetInitialDelay( byval hTooltip as HWND, byval nMs as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nMs < 0 then nMs = 0
    pTip->nInitialDelay = nMs
end sub

sub PsTooltip_SetAutoPopDelay( byval hTooltip as HWND, byval nMs as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nMs < 0 then nMs = 0
    pTip->nAutoPopDelay = nMs
end sub

sub PsTooltip_SetReshowDelay( byval hTooltip as HWND, byval nMs as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nMs < 0 then nMs = 0
    pTip->nReshowDelay = nMs
end sub

sub PsTooltip_SetFadeTime( byval hTooltip as HWND, byval nMs as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nMs < 0 then nMs = 0
    pTip->nFadeTime = nMs
end sub

' ========================================================================================
' PUBLIC API -- process-wide defaults
'
' These exist because a control creates its own tip lazily and the host never sees the HWND,
' so "theme every tooltip on this form" is otherwise unreachable. Setting each independently
' is what keeps a host that only wants new COLOURS from also flattening the delays Create
' derived from GetDoubleClickTime().
' ========================================================================================
sub PsTooltip_SetDefaultColors( byval pColors as PSTOOLTIP_COLORS ptr )
    if pColors = 0 then exit sub
    gTIP_DefColors    = *pColors
    gTIP_HasDefColors = true
end sub

sub PsTooltip_SetDefaultFonts( byval hText as HFONT, _
                               byval hTitle as HFONT = 0, byval hGlyph as HFONT = 0 )
    gTIP_DefFontText  = hText
    gTIP_DefFontTitle = hTitle
    gTIP_DefFontGlyph = hGlyph
    gTIP_HasDefFonts  = true
end sub

sub PsTooltip_SetDefaultStyle( byval nStyle as long )
    gTIP_DefStyle    = nStyle
    gTIP_HasDefStyle = true
end sub

sub PsTooltip_SetDefaultMaxWidth( byval nMaxWidth as long )
    gTIP_DefMaxWidth = nMaxWidth
    gTIP_HasDefMaxW  = true
end sub

' A negative argument leaves that delay alone, which is what makes the common case -- "just
' change the hover time" -- a single call that cannot disturb the other two.
sub PsTooltip_SetDefaultDelays( byval nInitialMs as long, _
                                byval nAutoPopMs as long = -1, _
                                byval nReshowMs as long = -1 )
    if nInitialMs >= 0 then
        gTIP_DefInitial    = nInitialMs
        gTIP_HasDefInitial = true
    end if
    if nAutoPopMs >= 0 then
        gTIP_DefAutoPop    = nAutoPopMs
        gTIP_HasDefAutoPop = true
    end if
    if nReshowMs >= 0 then
        gTIP_DefReshow    = nReshowMs
        gTIP_HasDefReshow = true
    end if
end sub

sub PsTooltip_ClearDefaults()
    gTIP_HasDefColors  = false
    gTIP_HasDefFonts   = false
    gTIP_HasDefStyle   = false
    gTIP_HasDefMaxW    = false
    gTIP_HasDefInitial = false
    gTIP_HasDefAutoPop = false
    gTIP_HasDefReshow  = false
    gTIP_DefFontText   = 0
    gTIP_DefFontTitle  = 0
    gTIP_DefFontGlyph  = 0
end sub

sub PsTooltip_ApplyDefaults( byval hTooltip as HWND )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub

    ' Deliberately routed through the public setters rather than written to the struct, so the
    ' dirty-marking and repainting each of them owns cannot be forgotten here.
    if gTIP_HasDefColors then PsTooltip_SetColors( hTooltip, @gTIP_DefColors )
    if gTIP_HasDefFonts then
        PsTooltip_SetFonts( hTooltip, gTIP_DefFontText, gTIP_DefFontTitle, gTIP_DefFontGlyph )
    end if
    if gTIP_HasDefStyle then PsTooltip_SetStyle( hTooltip, gTIP_DefStyle )
    if gTIP_HasDefMaxW then PsTooltip_SetMaxWidth( hTooltip, gTIP_DefMaxWidth )
    if gTIP_HasDefInitial then PsTooltip_SetInitialDelay( hTooltip, gTIP_DefInitial )
    if gTIP_HasDefAutoPop then PsTooltip_SetAutoPopDelay( hTooltip, gTIP_DefAutoPop )
    if gTIP_HasDefReshow then PsTooltip_SetReshowDelay( hTooltip, gTIP_DefReshow )
end sub

' ========================================================================================
' PsTooltip_GetSystemFont
'
' SPI_GETNONCLIENTMETRICS's lfStatusFont -- the face Windows itself draws tooltips and the
' status bar in. Read-only: this control creates no fonts and owns none, so the caller builds
' the HFONT and keeps it alive, exactly as it does for PsTooltip_SetFonts.
'
' The pixel->point conversion is the whole reason this is worth a shared function rather than
' six lines in each host; see the contract in the .bi before changing it. Briefly: lfHeight is
' pixels at the current DPI, a point size is not, and every CreateFont-style call that takes
' points scales them again.
' ========================================================================================
function PsTooltip_GetSystemFont( byref info as PSTOOLTIP_SYSTEMFONT ) as boolean
    dim as NONCLIENTMETRICSW ncm
    ncm.cbSize = sizeof(NONCLIENTMETRICSW)

    if SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), @ncm, 0 ) = 0 then
        return false
    end if

    dim as PSTOOLTIP_SYSTEMFONT got
    got.wszFaceName = ncm.lfStatusFont.lfFaceName
    got.pixelHeight = abs( ncm.lfStatusFont.lfHeight )
    got.lfWeight    = ncm.lfStatusFont.lfWeight
    got.isBold      = (ncm.lfStatusFont.lfWeight >= FW_BOLD)
    got.isItalic    = (ncm.lfStatusFont.lfItalic <> 0)

    dim as HDC hDC = GetDC( HWND_DESKTOP )
    if hDC then
        dim as long dpiY = GetDeviceCaps( hDC, LOGPIXELSY )
        if dpiY > 0 then got.pointSize = MulDiv( got.pixelHeight, 72, dpiY )
        ReleaseDC( HWND_DESKTOP, hDC )
    end if

    ' Nothing is written to the caller's struct until we know the whole answer is usable. A
    ' partial fill on the failure path would hand back a face with no size (or a size with no
    ' face) to a caller that only checked the return value -- and the fallback it was about to
    ' choose is exactly what it should get instead.
    if len( got.wszFaceName ) = 0 then return false
    if got.pointSize <= 0 then return false

    info = got
    return true
end function



sub PsTooltip_SetMoveTolerance( byval hTooltip as HWND, byval nPixels as long )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    if nPixels < 0 then nPixels = 0
    pTip->nMoveTolerance = nPixels
end sub


' ========================================================================================
' PUBLIC API -- manual drive
' ========================================================================================
function PsTooltip_Show( byval hTooltip as HWND ) as boolean
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return false
    dim as POINT pt
    GetCursorPos( @pt )
    dim as RECT rcAnchor = type( pt.x, pt.y, pt.x, pt.y )
    return TIP_ShowCore( pTip, rcAnchor, TIP_ALIGN_CURSOR )
end function

function PsTooltip_ShowForRect( byval hTooltip as HWND, byval rcAnchor as RECT, _
                                byval nAlign as long = TIP_ALIGN_BELOW ) as boolean
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return false
    return TIP_ShowCore( pTip, rcAnchor, nAlign )
end function

sub PsTooltip_Hide( byval hTooltip as HWND )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    TIP_HideCore( pTip, true )
end sub

function PsTooltip_IsVisible( byval hTooltip as HWND ) as boolean
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return false
    return pTip->bVisible
end function


' ========================================================================================
' PUBLIC API -- callbacks
' ========================================================================================
sub PsTooltip_SetPaintCallback( byval hTooltip as HWND, byval userfunc as TIP_PaintCallbackSub )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->PaintCallback = userfunc
    if pTip->hWin then InvalidateRect( pTip->hWin, NULL, TRUE )
end sub

sub PsTooltip_SetMessageCallback( byval hTooltip as HWND, byval userfunc as TIP_MessageCallbackFunc )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->MessageCallback = userfunc
end sub

sub PsTooltip_SetTooltipCallback( byval hTooltip as HWND, byval userfunc as TIP_TooltipCallbackFunc )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->TooltipCallback = userfunc
    TIP_InvalidateText( pTip )
end sub

sub PsTooltip_SetShowCallback( byval hTooltip as HWND, byval userfunc as TIP_ShowCallbackFunc )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    pTip->ShowCallback = userfunc
end sub


' ========================================================================================
' PUBLIC API -- introspection
' ========================================================================================
sub PsTooltip_GetIdealSize( byval hTooltip as HWND, byref nW as long, byref nH as long )
    nW = 0 : nH = 0
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    ' Force the pending layout. An assertion that reads DERIVED state must force the
    ' derivation first, or it reads the previous text's numbers (Learnings.md, PsDatePicker).
    if len( pTip->wszTipBuf ) = 0 then pTip->wszTipBuf = TIP_ResolveText( pTip )
    if pTip->bLayoutDirty then pTip->LayoutTooltip()
    nW = pTip->nIdealW
    nH = pTip->nIdealH
end sub


function PsTooltip_GetPartRect( byval hTooltip as HWND, byval nPart as long ) as RECT
    dim as RECT rcEmpty
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return rcEmpty
    if pTip->bLayoutDirty then pTip->LayoutTooltip()

    dim as RECT rcG, rcTi, rcTx
    TIP_BandRects( pTip, rcG, rcTi, rcTx )

    select case nPart
        case TIP_PART_CLIENT
            dim as RECT rc = type( 0, 0, pTip->nIdealW, pTip->nIdealH )
            return rc
        case TIP_PART_BODY  : return pTip->rcBody
        case TIP_PART_GLYPH : return rcG
        case TIP_PART_TITLE : return rcTi
        case TIP_PART_TEXT  : return rcTx
        case TIP_PART_STEM
            if pTip->bHasStem = false then return rcEmpty
            dim as RECT rc
            if pTip->bStemOnTop then
                SetRect( @rc, pTip->ptStem(0).x, 0, pTip->ptStem(2).x, pTip->nStemH )
            else
                SetRect( @rc, pTip->ptStem(0).x, pTip->nIdealH - pTip->nStemH, _
                              pTip->ptStem(2).x, pTip->nIdealH )
            end if
            return rc
    end select
    return rcEmpty
end function


' ========================================================================================
' RENDER PROBES
'
' Both drive PsTooltip_Render -- the SAME function WM_PAINT calls -- into an offscreen buffer.
' See the header for why that split is the whole point, and for the warning about inheriting a
' tone threshold from a sibling.
' ========================================================================================
private function TIP_RenderOffscreen( byval pTip as PSTOOLTIP ptr, byval nPart as long, _
                                      byref rcOut as RECT, byref hdcOut as HDC, byref hbmOut as HBITMAP ) as boolean
    hdcOut = 0 : hbmOut = 0
    if pTip = 0 then return false
    if pTip->bLayoutDirty then pTip->LayoutTooltip()

    dim as long cx = pTip->nIdealW
    dim as long cy = pTip->nIdealH
    if cx < 1 then cx = 1
    if cy < 1 then cy = 1

    dim as HDC hScreen = GetDC( null )
    dim as HDC hMem    = CreateCompatibleDC( hScreen )
    dim as HBITMAP hbm = CreateCompatibleBitmap( hScreen, cx, cy )
    ReleaseDC( null, hScreen )
    if hMem = 0 then return false
    if hbm = 0 then
        DeleteDC( hMem )
        return false
    end if
    SelectObject( hMem, hbm )

    dim as RECT rcAll = type( 0, 0, cx, cy )
    scope
        dim bb as PsBufferPaint
        bb.BeginDoubleBuffer( pTip->hWin, hMem, rcAll )
        PsTooltip_Render( pTip, @bb )
        bb.EndDoubleBuffer()
    end scope

    rcOut = PsTooltip_GetPartRect( pTip->hWin, nPart )
    if IsRectEmpty( @rcOut ) then
        DeleteObject( hbm )
        DeleteDC( hMem )
        return false
    end if
    ' Clip to the surface -- a part rect is derived geometry and a bad one must not read
    ' outside the bitmap.
    if rcOut.left   < 0  then rcOut.left   = 0
    if rcOut.top    < 0  then rcOut.top    = 0
    if rcOut.right  > cx then rcOut.right  = cx
    if rcOut.bottom > cy then rcOut.bottom = cy

    hdcOut = hMem
    hbmOut = hbm
    return true
end function


' ========================================================================================
' Drive exactly ONE tick with injected inputs. See the contract in PsTooltip.bi.
' ========================================================================================
sub PsTooltip_TickForTest( byval hTooltip as HWND, byval ptCursor as POINT, _
                           byval bOver as boolean, byval bButtonDown as boolean, _
                           byval dwNow as DWORD )
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then exit sub
    TIP_TickOne( pTip, ptCursor, bOver, bButtonDown, dwNow )
end sub


function PsTooltip_CountRenderedTones( byval hTooltip as HWND, byval nPart as long ) as long
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return 0

    dim as RECT rc
    dim as HDC hMem
    dim as HBITMAP hbm
    if TIP_RenderOffscreen( pTip, nPart, rc, hMem, hbm ) = false then return 0

    dim as ulong seen(0 to 255)
    dim as long n = 0
    for y as long = rc.top to rc.bottom - 1
        for x as long = rc.left to rc.right - 1
            dim as ulong px = GetPixel( hMem, x, y )
            dim as boolean bFound = false
            for i as long = 0 to n - 1
                if seen(i) = px then
                    bFound = true
                    exit for
                end if
            next
            if bFound = false then
                if n < 256 then
                    seen(n) = px
                    n += 1
                end if
            end if
        next
    next

    DeleteObject( hbm )
    DeleteDC( hMem )
    return n
end function


' FNV-1a over one part rect. For asserting that a state change actually REACHED THE PIXELS.
'
' READ THIS BEFORE TRUSTING IT: PsCheckBox shipped the first version of this probe, and its
' "the checked state reaches the pixels" assertion PASSED with the glyph resolution frozen --
' because isChecked also moves the box COLOUR, so the pixels differed for a reason unrelated
' to the thing the assertion was named for. An assertion built on this must ISOLATE its
' variable: hold everything else constant so nothing but the change under test can account for
' a different hash.
function PsTooltip_HashRenderedPart( byval hTooltip as HWND, byval nPart as long ) as ulong
    dim as PSTOOLTIP ptr pTip = GetTooltipPointer(hTooltip)
    if pTip = 0 then return 0

    dim as RECT rc
    dim as HDC hMem
    dim as HBITMAP hbm
    if TIP_RenderOffscreen( pTip, nPart, rc, hMem, hbm ) = false then return 0

    dim as ulong h = &h811C9DC5
    for y as long = rc.top to rc.bottom - 1
        for x as long = rc.left to rc.right - 1
            dim as ulong px = GetPixel( hMem, x, y )
            for nByte as long = 0 to 3
                h = h xor ((px shr (nByte * 8)) and &hFF)
                h = h * &h01000193
            next
        next
    next

    DeleteObject( hbm )
    DeleteDC( hMem )
    return h
end function