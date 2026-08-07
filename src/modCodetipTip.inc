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

#include once "modCodetipTip.bi"


' The singleton tip, and what it is currently anchored to. gCodetipAnchorPos is the
' document position of the call's "(" -- the caret may move forward through the parameter
' list without dismissing, but never back past this.
dim shared ghCodetip          as HWND
dim shared ghCodetipEdit      as HWND     ' the Scintilla the live tip belongs to
dim shared gCodetipAnchorPos  as long = -1
dim shared gCodetipAnchorLine as long = -1
' Where the anchor last landed on screen. Held so SCN_UPDATEUI -- which fires for a great
' deal more than scrolling -- only re-places the window when the point actually moved.
dim shared gCodetipLastPt     as POINT


' ========================================================================================
' THE DISMISSAL LAW. Pure: no window, no caret, no pump -- which is the only reason it can
' be asserted at all. Codetip_SyncToCaret does nothing this function does not decide.
'
' A caret moving FORWARD along the same line is typing the parameters, and is exactly when
' the tip should stay up. A different line, or a caret back before the "(", has left the
' call.
' ========================================================================================
function Codetip_ShouldDismiss( byval nCaretPos   as long, byval nCaretLine  as long, _
                                byval nAnchorPos  as long, byval nAnchorLine as long _
                                ) as boolean
    if nAnchorPos < 0 then return true              ' nothing anchored
    if nCaretLine <> nAnchorLine then return true   ' left the line
    if nCaretPos < nAnchorPos then return true      ' backed out past the "("
    return false
end function


' ========================================================================================
' The screen rect the tip hangs off: the anchor character's cell in the editor, one text
' line tall. Returns FALSE when the anchor has scrolled out of the client area, which is
' the caller's cue to hide rather than to place the tip off the edge of the view.
' ========================================================================================
private function Codetip_AnchorRect( byval hEdit as HWND, byval nAnchorPos as long, _
                                     byref rcOut as RECT ) as boolean
    if hEdit = 0 then return false
    if IsWindow( hEdit ) = 0 then return false

    dim as long x = SciExec( hEdit, SCI_POINTXFROMPOSITION, 0, nAnchorPos )
    dim as long y = SciExec( hEdit, SCI_POINTYFROMPOSITION, 0, nAnchorPos )
    dim as long nLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nAnchorPos, 0 )
    dim as long h = SciExec( hEdit, SCI_TEXTHEIGHT, nLine, 0 )
    if h <= 0 then h = 16

    dim as RECT rcClient
    GetClientRect( hEdit, @rcClient )
    ' Scrolled out of view. Note the bottom test uses the line's TOP: a line half off the
    ' bottom edge still has a usable anchor, one entirely below it does not.
    if (y + h) <= 0 then return false
    if y >= rcClient.bottom then return false

    dim as POINT pt = type( x, y )
    ClientToScreen( hEdit, @pt )
    SetRect( @rcOut, pt.x, pt.y, pt.x, pt.y + h )
    return true
end function


' ========================================================================================
' Create the singleton. Once, from frmMain.
' ========================================================================================
sub Codetip_Init()
    if ghCodetip then exit sub

    ' Create ALSO attaches, and attaching is what enrols this tip in the shared dwell
    ' clock. DETACH IMMEDIATELY -- see the header of modCodetipTip.bi. Without this the
    ' clock hides the tip the moment the cursor is not over the editor, on any mouse
    ' button, and after the auto-pop delay: three behaviours that are right for a hover
    ' tip and wrong for a code tip.
    ghCodetip = PsTooltip_Create( HWND_FRMMAIN )
    if ghCodetip = 0 then exit sub
    PsTooltip_Detach( ghCodetip )

    ' Belt and braces on top of the detach: Create's default is GetDoubleClickTime * 10,
    ' and a tip that vanishes part-way through a parameter list is the one failure mode
    ' this control must not have.
    PsTooltip_SetAutoPopDelay( ghCodetip, 0 )

    PsTooltip_SetStyle( ghCodetip, TIP_STYLE_RECT )
    ' No title and no glyph: the tip IS the signature. The bold and symbol fonts are
    ' passed anyway because PsTooltip_SetFonts takes all three.
    PsTooltip_SetFonts( ghCodetip, ghFont(GUIFONT_10), ghFont(GUIFONTBOLD_11), ghFont(SYMBOLFONT_10) )

    Codetip_SyncTheme()
end sub


sub Codetip_Destroy()
    if ghCodetip = 0 then exit sub
    DestroyWindow( ghCodetip )
    ghCodetip          = 0
    ghCodetipEdit      = 0
    gCodetipAnchorPos  = -1
    gCodetipAnchorLine = -1
end sub


' ========================================================================================
' Re-apply the editor theme.
'
' The EDITOR palette, not the popup one -- this tip and the autocomplete list appear in
' the same place, alternating, so they must match each other rather than the menus.
' frmAutoComplete_SyncTheme uses theme.editor.text.backcolor for exactly this reason.
' ========================================================================================
sub Codetip_SyncTheme()
    if ghCodetip = 0 then exit sub
    dim as PSTOOLTIP_COLORS tipc
    ' Editor palette on purpose -- see modThemeTips.inc. This used to be five lines here
    ' and five more (with a DIFFERENT title colour) in frmDebug.
    Theme_FillCodeTipColors( tipc )
    PsTooltip_SetColors( ghCodetip, @tipc )
end sub


function Codetip_IsVisible() as boolean
    if ghCodetip = 0 then return false
    ' Asked of the control, never held as a second boolean here: PsTooltip allows only one
    ' visible tip machine-wide (gTIP_Visible), so any other tip appearing hides ours
    ' without telling us, and a local flag would go stale.
    return PsTooltip_IsVisible( ghCodetip )
end function


' ========================================================================================
' Show the tip for a call whose "(" sits at nAnchorPos.
' ========================================================================================
function Codetip_Show( byval pDoc as clsDocument ptr, byval wszText as DWSTRING, _
                       byval nAnchorPos as long ) as boolean
    if ghCodetip = 0 then return false
    if pDoc = 0 then return false
    if PsLen( wszText ) = 0 then return false

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as RECT rcAnchor
    if Codetip_AnchorRect( hEdit, nAnchorPos, rcAnchor ) = false then
        Codetip_Hide()
        return false
    end if

    PsTooltip_SetText( ghCodetip, wszText )

    ' TIP_ALIGN_BELOW flips ABOVE by itself when the caret is near the bottom of the
    ' monitor, which is the whole reason for using it rather than placing by hand.
    if PsTooltip_ShowForRect( ghCodetip, rcAnchor, TIP_ALIGN_BELOW ) = false then
        Codetip_Hide()
        return false
    end if

    ghCodetipEdit      = hEdit
    gCodetipAnchorPos  = nAnchorPos
    gCodetipAnchorLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nAnchorPos, 0 )
    gCodetipLastPt     = type( rcAnchor.left, rcAnchor.top )
    return true
end function


sub Codetip_Hide()
    if ghCodetip = 0 then exit sub
    PsTooltip_Hide( ghCodetip )
    ghCodetipEdit      = 0
    gCodetipAnchorPos  = -1
    gCodetipAnchorLine = -1
end sub


' ========================================================================================
' Called from SCN_UPDATEUI, which fires for caret moves, selection changes AND scrolling.
' One function covers all three: the caret decides whether the tip lives, the anchor's
' screen position decides where it sits.
'
' Note this must run OUTSIDE the caller's "has the caret moved" guard -- a wheel scroll
' moves the tip's correct position without moving the caret at all.
' ========================================================================================
sub Codetip_SyncToCaret( byval pDoc as clsDocument ptr )
    if Codetip_IsVisible() = false then
        ' Something else took the one visible-tip slot, or the tip faded out. Drop the
        ' anchor so a later scroll cannot resurrect it.
        if gCodetipAnchorPos >= 0 then
            ghCodetipEdit      = 0
            gCodetipAnchorPos  = -1
            gCodetipAnchorLine = -1
        end if
        exit sub
    end if

    if pDoc = 0 then
        Codetip_Hide()
        exit sub
    end if

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    ' A different Scintilla is active: the document was switched, or the split moved. The
    ' tip belongs to a view that is no longer in front of the user.
    if hEdit <> ghCodetipEdit then
        Codetip_Hide()
        exit sub
    end if

    dim as long nCaretPos  = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nCaretPos, 0 )
    if Codetip_ShouldDismiss( nCaretPos, nCaretLine, gCodetipAnchorPos, gCodetipAnchorLine ) then
        Codetip_Hide()
        exit sub
    end if

    ' Still inside the call. Follow the anchor if the view moved under it.
    dim as RECT rcAnchor
    if Codetip_AnchorRect( hEdit, gCodetipAnchorPos, rcAnchor ) = false then
        Codetip_Hide()
        exit sub
    end if
    if (rcAnchor.left = gCodetipLastPt.x) andalso (rcAnchor.top = gCodetipLastPt.y) then exit sub

    ' Re-place. ShowForRect on an already-visible tip continues from the current alpha
    ' rather than restarting the fade, so this does not flicker.
    if PsTooltip_ShowForRect( ghCodetip, rcAnchor, TIP_ALIGN_BELOW ) then
        gCodetipLastPt = type( rcAnchor.left, rcAnchor.top )
    else
        Codetip_Hide()
    end if
end sub


' ========================================================================================
' Escape, and NOTHING else. This is tiko's own key handling, not a PsTooltip obligation --
' that control has no FilterMessage and needs no pump call.
'
' Position in the pump matters for the same reason frmAutoComplete_FilterMessage's does:
' ahead of handleEscKeyModeless, or the Escape aimed at the tip closes the Find panel.
' ========================================================================================
function Codetip_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if pMsg->message <> WM_KEYDOWN then return false
    if pMsg->wParam <> VK_ESCAPE then return false
    if Codetip_IsVisible() = false then return false
    Codetip_Hide()
    return true
end function


' ========================================================================================
' Env-gated self-test: TIKO_CODETIP_SELFTEST=1
'
' HEADLESS, and therefore covers only what a pure function can reach: the dismissal law
' and FormatCodetip's output shape. IT PROVES NOTHING ABOUT THE TIP APPEARING -- a suite
' like this stays green with Codetip_Show stubbed out to "return false", which is why the
' A/B pass includes exactly that break and why the interactive pass is the load-bearing
' evidence for this feature.
' ========================================================================================
dim shared gCodetipTestPass as long
dim shared gCodetipTestFail as long

private sub Codetip_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gCodetipTestPass += 1
    else
        gCodetipTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

sub Codetip_RunSelfTest()
    if environ("TIKO_CODETIP_SELFTEST") <> "1" then exit sub

    gCodetipTestPass = 0
    gCodetipTestFail = 0
    print "--- TIKO_CODETIP_SELFTEST ---"

    ' --- the dismissal law -------------------------------------------------------------
    ' Anchor: "(" at position 100 on line 7.
    Codetip_Check( "dismiss: caret sits on the anchor", _
        Codetip_ShouldDismiss( 100, 7, 100, 7 ) = false )
    Codetip_Check( "dismiss: caret typing forward on the same line stays", _
        Codetip_ShouldDismiss( 137, 7, 100, 7 ) = false )
    Codetip_Check( "dismiss: caret backed out past the open paren", _
        Codetip_ShouldDismiss( 99, 7, 100, 7 ) = true )
    Codetip_Check( "dismiss: caret on the next line", _
        Codetip_ShouldDismiss( 190, 8, 100, 7 ) = true )
    Codetip_Check( "dismiss: caret on an earlier line", _
        Codetip_ShouldDismiss( 40, 3, 100, 7 ) = true )
    ' A line change dismisses even when the position still reads as "forward", which is
    ' what a wrapped or continued call looks like. The line test must be checked BEFORE
    ' the position test or this passes for the wrong reason.
    Codetip_Check( "dismiss: line beats position", _
        Codetip_ShouldDismiss( 500, 9, 100, 7 ) = true )
    Codetip_Check( "dismiss: nothing anchored", _
        Codetip_ShouldDismiss( 100, 7, -1, -1 ) = true )

    ' --- the tip's own configuration ---------------------------------------------------
    ' THE DETACH IS THE ONE LOAD-BEARING STRUCTURAL DECISION IN THIS MODULE, and dropping
    ' it fails SILENTLY: the tip becomes hover-driven again and vanishes the moment the
    ' cursor leaves the editor or a mouse button goes down -- mid-parameter-list, with a
    ' clean build and (before these three lines) a green suite. Asserted for that reason
    ' and no other. Runs after Codetip_Init, which is why the call site is where it is.
    Codetip_Check( "init: the tip window exists", ghCodetip <> 0 )
    Codetip_Check( "init: the tip is DETACHED from the dwell clock", _
        PsTooltip_GetAttached( ghCodetip ) = 0 )
    ' Named for what it actually tests. PsTooltip exposes no GetAutoPopDelay, so the
    ' "auto-pop is off" half of Codetip_Init is NOT asserted anywhere -- the detach above
    ' is what makes it moot, and this line only covers the style.
    Codetip_Check( "init: the style is a plain rect, not a balloon", _
        PsTooltip_GetStyle( ghCodetip ) = TIP_STYLE_RECT )

    ' --- FormatCodetip's output shape --------------------------------------------------
    ' Short signatures are left alone; long ones are broken one parameter per line. The
    ' assertion is on the LINE COUNT, not on the exact text: the substitutions inside
    ' FormatCodetip are cosmetic, the multi-line shape is the contract PsTooltip renders.
    scope
        dim as string sShort = FormatCodetip( "Foo(x as long) as long" )
        Codetip_Check( "format: a short signature is not broken up", _
            instr( sShort, chr(10) ) = 0, sShort )

        dim as string sNoArgs = FormatCodetip( "Foo() as long" )
        Codetip_Check( "format: a no-argument call is not broken up", _
            instr( sNoArgs, chr(10) ) = 0, sNoArgs )

        dim as string sLong = FormatCodetip( _
            "MyVeryLongFunctionName(byval wszFirstParameter as DWSTRING, " & _
            "byval nSecondParameter as long, byval nThirdParameter as long) as boolean" )
        dim as long nLines = 1
        for i as long = 1 to len(sLong)
            if mid(sLong, i, 1) = chr(10) then nLines += 1
        next
        ' Four lines: the name and "(", then one per parameter, with the ")" trailing the
        ' last. The exact figure is measured off this input, not predicted.
        Codetip_Check( "format: a long signature breaks one parameter per line", _
            nLines >= 4, "lines=" & str(nLines) )
        Codetip_Check( "format: the broken form keeps every parameter name", _
            (instr(sLong, "wszFirstParameter") > 0) andalso _
            (instr(sLong, "nSecondParameter") > 0) andalso _
            (instr(sLong, "nThirdParameter") > 0) )
    end scope

    print "--- TIKO_CODETIP_SELFTEST: " & str(gCodetipTestPass) & " passed, " & _
          str(gCodetipTestFail) & " failed ---"
end sub
