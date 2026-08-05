'    tiko
'    Copyright (C) 2024-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify it under the
'    terms of the GNU General Public License as published by the Free Software Foundation,
'    either version 3 of the License, or (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful, but WITHOUT ANY
'    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
'    PARTICULAR PURPOSE.  See the GNU General Public License for more details.

' ========================================================================================
' frmOutputFloat -- implementation. See frmOutputFloat.bi for what this is and why the
' panel is reparented rather than recreated.
' ========================================================================================

#include once "frmOutputFloat.bi"


' Re-entrancy guard. _Undock and _Dock both call frmMain_PositionWindows and
' frmOutput_PositionWindows, either of which can end up back in a click callback; and
' _Dock destroys a window from inside that window's own WM_CLOSE. One flag held across
' both bodies is cheaper than making every path re-entrant.
dim shared as boolean gOutFloatBusy = false

' The frame's class background brush, owned here and deleted in WM_NCDESTROY.
'
' It exists to kill the WHITE FLASH on open. CWindow registers its per-instance class with
' hbrBackground = COLOR_3DFACE+1 (CWindow.inc:378), so the very first erase -- which Windows
' performs at ShowWindow time, before our WndProc sees anything at all -- paints the whole
' client in the system 3D face colour. WM_ERASEBKGND returning TRUE cannot prevent that: it
' is not our message yet. The only way to make that first erase the right colour is to make
' the CLASS brush the right colour.
dim shared as HBRUSH ghOutFloatBrush = 0


' ========================================================================================
' The single source of truth for the dock state. There is deliberately no boolean beside
' this: gConfig.OutputFloating is a PERSISTED COPY refreshed on capture, not the live
' state, exactly as gConfig.ShowOutputPanelIndex is a persisted copy of the tab bar's
' current panel rather than the panel itself.
' ========================================================================================
function frmOutputFloat_IsFloating() as boolean
    function = iif( IsWindow( HWND_FRMOUTPUTFLOAT ), true, false )
end function


' ========================================================================================
' The window that carries the Output panel's VISIBILITY in the current dock state.
'
' While floating the panel itself is always shown -- it fills the frame's client area and
' the FRAME is what gets hidden. So every visibility question (the View>Output toggle, the
' menu checkmark, frmOutput_CaptureState) has to be asked of this window rather than of
' HWND_FRMOUTPUT, or a hidden floating panel reads as visible.
' ========================================================================================
function frmOutputFloat_VisibilityTarget() as HWND
    if frmOutputFloat_IsFloating() then return HWND_FRMOUTPUTFLOAT
    function = HWND_FRMOUTPUT
end function


sub frmOutputFloat_SetVisible( byval bVisible as boolean )
    dim as HWND hTarget = frmOutputFloat_VisibilityTarget()
    if IsWindow( hTarget ) = 0 then exit sub
    ShowWindow( hTarget, iif( bVisible, SW_SHOW, SW_HIDE ) )
    ' Synchronously, for the same reason as in _Undock -- a show that only invalidates
    ' leaves the window on screen unpainted until the pump next idles.
    if bVisible then
        RedrawWindow( hTarget, 0, 0, RDW_INVALIDATE or RDW_UPDATENOW or RDW_ALLCHILDREN )
    end if
    if frmOutputFloat_IsFloating() then
        ' The docked layout does not change when a FLOATING window is shown or hidden --
        ' the panel already reserves nothing down there. Bringing it forward is the useful
        ' half, or a "show" leaves it behind the editor with nothing visibly happening.
        if bVisible then SetForegroundWindow( HWND_FRMOUTPUTFLOAT )
    else
        frmMain_PositionWindows()
    end if
end sub


' ========================================================================================
' Refresh the persisted geometry from the live frame.
'
' Mirrors frmHelpCenter_CaptureState: skipped while minimized, where the live rect is not
' the rect worth remembering, and reading rcNormalPosition rather than GetWindowRect so a
' maximized window still records the size it will restore to.
' ========================================================================================
sub frmOutputFloat_CaptureState()
    if IsWindow( HWND_FRMOUTPUTFLOAT ) = 0 then exit sub
    if IsIconic( HWND_FRMOUTPUTFLOAT ) then exit sub

    dim WinPla as WINDOWPLACEMENT
    WinPla.Length = sizeof(WinPla)
    if GetWindowPlacement( HWND_FRMOUTPUTFLOAT, @WinPla ) = 0 then exit sub

    with gConfig
        .OutputFloatLeft      = WinPla.rcNormalPosition.Left
        .OutputFloatTop       = WinPla.rcNormalPosition.Top
        .OutputFloatRight     = WinPla.rcNormalPosition.Right
        .OutputFloatBottom    = WinPla.rcNormalPosition.Bottom
        .OutputFloatMaximized = iif( WinPla.showCmd = SW_MAXIMIZE, true, false )
    end with
end sub


' ========================================================================================
' Point the frame's class background brush at the current theme.
'
' Called at create time (before the window is ever shown) and again on a theme change, so
' the system's own erase is always the colour WM_PAINT is about to draw. Same colour, so
' there is nothing to flash between the two.
' ========================================================================================
sub frmOutputFloat_SyncBrush()
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMOUTPUTFLOAT )
    if pWindow = 0 then exit sub
    dim as HBRUSH hOld = ghOutFloatBrush
    ghOutFloatBrush = CreateSolidBrush( theme.output.texthot.backcolor )
    pWindow->Brush = ghOutFloatBrush
    ' Freed only AFTER the class has stopped pointing at it.
    if hOld then DeleteObject( hOld )
end sub


' ========================================================================================
' Size the panel to fill the frame's client area.
' ========================================================================================
private sub frmOutputFloat_FitPanel()
    if IsWindow( HWND_FRMOUTPUTFLOAT ) = 0 then exit sub
    if IsWindow( HWND_FRMOUTPUT ) = 0 then exit sub
    dim as RECT rc: GetClientRect( HWND_FRMOUTPUTFLOAT, @rc )
    SetWindowPos( HWND_FRMOUTPUT, HWND_TOP, 0, 0, rc.right, rc.bottom, SWP_FRAMECHANGED )
    frmOutput_PositionWindows()
end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmOutputFloat_WndProc( byval hwnd   as HWND, _
                                         byval uMsg   as UINT, _
                                         byval wParam as WPARAM, _
                                         byval lParam as LPARAM _
                                         ) as LRESULT
    select case uMsg
        case WM_SIZE
            if wParam <> SIZE_MINIMIZED then frmOutputFloat_FitPanel()
            return 0

        case WM_SETFOCUS
            ' The frame itself is never a useful focus target -- clicking its caption would
            ' otherwise leave the keyboard nowhere. Hand focus to the panel, whose own
            ' WS_EX_CONTROLPARENT lets the dialog manager find the visible pane from there.
            if IsWindow( HWND_FRMOUTPUT ) then SetFocus( HWND_FRMOUTPUT )
            return 0

        case WM_ERASEBKGND
            ' The panel covers the whole client; erasing under it only adds a flash.
            return true

        case WM_PAINT
            ' Only ever visible for the instant between a resize and the panel catching up.
            dim as PsBufferPaint b
            b.BeginDoubleBuffer(hwnd)
            b.SetBackColor( theme.output.texthot.backcolor )
            b.PaintClientRect()
            b.EndDoubleBuffer()
            return 0

        case WM_CLOSE
            ' The X RE-DOCKS. It does not destroy this window directly and it does not hide
            ' the panel -- _Dock does the destroying, and only after the panel has been
            ' reparented back out of here.
            frmOutputFloat_Dock()
            return 0

        case WM_DESTROY
            ' Shutdown only: frmMain destroys its owned windows on the way out, and by then
            ' the config has already been written. Re-docking from here would reparent a
            ' panel into a window that is itself being torn down.
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMOUTPUTFLOAT = 0
            ' The class brush is ours; the class dies with the instance.
            ' REQUIRED, and measured rather than assumed: the system does NOT delete a class
            ' background brush when the class is unregistered (CWindow's destructor does
            ' unregister, at CWindow.inc:600). Dropping this line leaks exactly one brush per
            ' undock -- 60 cycles grew the process's GDI objects by 59.
            if ghOutFloatBrush then
                DeleteObject( ghOutFloatBrush )
                ghOutFloatBrush = 0
            end if
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Build the floating frame, placed but HIDDEN. The caller shows it.
' ========================================================================================
private function frmOutputFloat_CreateWindow() as boolean
    if IsWindow( HWND_FRMOUTPUTFLOAT ) then return true
    if IsWindow( HWND_FRMMAIN ) = 0 then return false

    dim pWindow as CWindow ptr = new CWindow
    dim as DWSTRING wszText = L(395, "Output")

    ' hParent = HWND_FRMMAIN together with WS_POPUP makes frmMain the OWNER (not the
    ' parent): that is what buys "always above tiko", "no taskbar button" and "goes away
    ' with tiko". WS_EX_CONTROLPARENT is required here -- unlike frmHelpCenter, which
    ' documents why it omits it -- because this frame really does host tab-navigable
    ' children, and it is what lets IsDialogMessage recurse down into the panel.
    HWND_FRMOUTPUTFLOAT = pWindow->Create( _
        HWND_FRMMAIN, wszText, @frmOutputFloat_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_OVERLAPPEDWINDOW or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_TOOLWINDOW or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    if HWND_FRMOUTPUTFLOAT = 0 then
        delete pWindow
        return false
    end if

    ' Reads the theme currently IN MEMORY, not gConfig -- see Theme_IsDark.
    Theme_ApplyDarkCaption( HWND_FRMOUTPUTFLOAT )

    ' Set BEFORE the window is ever shown, which is the whole point -- the first erase
    ' happens inside ShowWindow, ahead of any message we could answer.
    frmOutputFloat_SyncBrush()

    ' A zero Right/Bottom is the "never floated before" sentinel. Heal it here rather than
    ' at load, so the default is computed at the real DPI.
    if (gConfig.OutputFloatRight = 0) orelse (gConfig.OutputFloatBottom = 0) then
        dim as RECT rcMain
        GetWindowRect( HWND_FRMMAIN, @rcMain )
        gConfig.OutputFloatLeft   = rcMain.left + pWindow->ScaleX(80)
        gConfig.OutputFloatTop    = rcMain.top  + pWindow->ScaleY(80)
        gConfig.OutputFloatRight  = gConfig.OutputFloatLeft + pWindow->ScaleX(900)
        gConfig.OutputFloatBottom = gConfig.OutputFloatTop  + pWindow->ScaleY(400)
    end if

    ' NOTE: the stored coordinates are already SCALED, and CWindow.Create multiplies its
    ' x/y/cx/cy arguments by the DPI ratio -- so passing them there would scale them a
    ' second time. Zeros above, real placement here, exactly as frmHelpCenter does it.
    dim WinPla as WINDOWPLACEMENT
    with WinPla
        .Length = sizeof(WinPla)
        .rcNormalPosition.Left   = gConfig.OutputFloatLeft
        .rcNormalPosition.Top    = gConfig.OutputFloatTop
        .rcNormalPosition.Right  = gConfig.OutputFloatRight
        .rcNormalPosition.Bottom = gConfig.OutputFloatBottom
        .showCmd = SW_HIDE
    end with
    SetWindowPlacement( HWND_FRMOUTPUTFLOAT, @WinPla )

    function = true
end function


' ========================================================================================
' Detach the panel into the floating frame.
' ========================================================================================
sub frmOutputFloat_Undock()
    if gOutFloatBusy then exit sub
    if frmOutputFloat_IsFloating() then exit sub
    if IsWindow( HWND_FRMOUTPUT ) = 0 then exit sub
    gOutFloatBusy = true

    ' Bank the DOCKED state first. This is the only moment the user's docked height can be
    ' read off a live window; a moment from now the panel's height is the frame's client
    ' height and the docked value would be unrecoverable.
    frmOutput_CaptureState()

    ' The panel's OWN WS_VISIBLE bit, never IsWindowVisible -- that walks the ancestor chain,
    ' and the startup restore runs from frmMain_Show BEFORE frmMain itself is placed and
    ' shown. So IsWindowVisible answers FALSE for a panel that is perfectly visible as far as
    ' its own style is concerned, the float would be created and never shown, and the next
    ' save would then capture ShowOutputPanel = false from that hidden frame -- losing the
    ' setting permanently and unticking View>Output. (Third site in this change to hit the
    ' same trap; the other two are frmOutput_PositionWindows and the self-test.)
    dim as boolean bWasVisible = _
        (GetWindowLongPtr( HWND_FRMOUTPUT, GWL_STYLE ) and WS_VISIBLE) <> 0
    dim as HWND    hFocus      = GetFocus()

    if frmOutputFloat_CreateWindow() = false then
        gOutFloatBusy = false
        exit sub
    end if

    SetParent( HWND_FRMOUTPUT, HWND_FRMOUTPUTFLOAT )

    ' While floating the panel is ALWAYS shown -- the frame carries visibility.
    dim as RECT rc: GetClientRect( HWND_FRMOUTPUTFLOAT, @rc )
    SetWindowPos( HWND_FRMOUTPUT, HWND_TOP, 0, 0, rc.right, rc.bottom, _
                  SWP_FRAMECHANGED or SWP_SHOWWINDOW )

    ' ShowOutputPanelMinimized IS DELIBERATELY LEFT ALONE. It is half of the banked DOCKED
    ' state, exactly like ShowOutputPanelHeight beside it, and _Dock reads it back to decide
    ' which height to restore to -- so undocking a minimized panel and closing the float
    ' returns it minimized rather than sprung open. Clearing it here (which this did at
    ' first) silently discarded that, and no floating-state assertion could see it: the
    ' flag is unobservable for as long as the panel has a frame of its own.
    '
    ' Nothing reads it while floating either. frmOutput_MinimizePanel and _RestorePanel both
    ' exit early in that state, so a stale-looking flag cannot drive the panel anywhere.
    gConfig.OutputFloating = 1

    ' SetParent can orphan the focus when the focused window is inside the moved subtree.
    if IsWindow( hFocus ) then SetFocus( hFocus )

    ' UpdateDockIcon re-lays out the panel itself (the "X" going away changes how much of the
    ' strip the tab bar gets), so there is no separate frmOutput_PositionWindows call here.
    frmOutput_UpdateDockIcon()
    frmMain_PositionWindows()

    if bWasVisible then
        ShowWindow( HWND_FRMOUTPUTFLOAT, _
                    iif( gConfig.OutputFloatMaximized, SW_MAXIMIZE, SW_SHOWNORMAL ) )
        ' AFTER the show, not before: this reads GetWindowRect, so it is a no-op until the
        ' window has its final rect. It moves the window only when the saved rect lands on
        ' no monitor at all -- a position saved on a display that is no longer attached --
        ' and leaves a valid multi-monitor position alone.
        AfxForceVisibleDisplay( HWND_FRMOUTPUTFLOAT )
        SetForegroundWindow( HWND_FRMOUTPUTFLOAT )
        ' Paint NOW rather than whenever the pump next goes idle. ShowWindow only posts the
        ' work: it invalidates and leaves a WM_PAINT to be picked up later, and undocking
        ' runs from a click callback with frmMain_PositionWindows and a full panel re-layout
        ' still to go -- so "later" is long enough to see. RDW_UPDATENOW dispatches it
        ' synchronously, and RDW_ALLCHILDREN carries it into the panel and its controls,
        ' which are the windows actually covering the client.
        RedrawWindow( HWND_FRMOUTPUTFLOAT, 0, 0, _
                      RDW_INVALIDATE or RDW_UPDATENOW or RDW_ALLCHILDREN )
    end if

    gOutFloatBusy = false
end sub


' ========================================================================================
' Return the panel to the bottom of frmMain and destroy the frame.
' ========================================================================================
sub frmOutputFloat_Dock()
    if gOutFloatBusy then exit sub
    if frmOutputFloat_IsFloating() = false then exit sub
    ' On the way out frmMain destroys its owned windows itself, and the config has already
    ' been written by then. Reparenting into a window that is being torn down is not.
    if gApp.IsShutDown then exit sub
    gOutFloatBusy = true

    ' While the frame still exists.
    frmOutputFloat_CaptureState()

    dim as HWND hFrame     = HWND_FRMOUTPUTFLOAT
    dim as boolean bWasVisible = iif( IsWindowVisible( hFrame ), true, false )
    dim as HWND hFocus     = GetFocus()
    gConfig.OutputFloating = 0

    ' ORDER IS LOAD-BEARING: reparent the panel OUT of the frame before destroying it.
    ' Reversed, DestroyWindow takes HWND_FRMOUTPUT and every pane with it -- the Notes
    ' buffer, the results, the lot -- frmOutput's WM_NCDESTROY zeroes the handles, and the
    ' next layout pass positions a dead window. The whole "reparent, never recreate"
    ' design rests on these two lines being this way round.
    SetParent( HWND_FRMOUTPUT, HWND_FRMMAIN )

    ' Restore to whichever DOCKED height the banked pair calls for. A panel that was
    ' minimized when it undocked comes back minimized -- the flag survived the float
    ' untouched, and frmOutput_Show reads the same pair the same way at startup, so the
    ' undock/dock round trip and a restart agree about what "minimized" restores to.
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMOUTPUT )
    if pWindow then
        gConfig.ShowOutputPanelHeight = frmOutput_ClampHeight( gConfig.ShowOutputPanelHeight )
        dim as long nHeight = gConfig.ShowOutputPanelHeight
        if gConfig.ShowOutputPanelMinimized <> 0 then nHeight = OUTPUT_TABS_HEIGHT
        AfxSetWindowSize( HWND_FRMOUTPUT, _
                          AfxGetWindowWidth( HWND_FRMOUTPUT ), _
                          pWindow->ScaleY( nHeight ) )
    end if
    SetWindowPos( HWND_FRMOUTPUT, 0, 0, 0, 0, 0, _
                  SWP_NOMOVE or SWP_NOSIZE or SWP_NOZORDER or SWP_FRAMECHANGED )
    ShowWindow( HWND_FRMOUTPUT, iif( bWasVisible, SW_SHOW, SW_HIDE ) )

    DestroyWindow( hFrame )

    frmOutput_UpdateDockIcon()
    frmMain_PositionWindows()

    if IsWindow( hFocus ) then SetFocus( hFocus )

    gOutFloatBusy = false
end sub


sub frmOutputFloat_Toggle()
    if frmOutputFloat_IsFloating() then
        frmOutputFloat_Dock()
    else
        frmOutputFloat_Undock()
    end if
end sub


' ========================================================================================
' Env-gated self-test: TIKO_OUTPUTFLOAT_SELFTEST=1
'
' A POST-SHOW SUITE BY CONSTRUCTION: every assertion here needs live windows, because what
' is being asserted is what SetParent did to them. There is no pure half to write -- the
' arithmetic in this feature is one clamp that frmOutput's own suite already covers.
'
' WHAT IT CAN SEE, and it is worth being explicit because the headline group is unusual:
' the content-survival assertions are the ones this whole design exists for. A
' destroy-and-recreate implementation passes every parent/style/geometry check in this file
' and fails those, because the child handles come back as DIFFERENT NUMBERS.
'
' WHAT IT IS BLIND TO, structurally: whether any of it RENDERS. The frame's caption, the
' dark-caption theming, the "X" hiding cleanly rather than half-drawn, the panel actually
' filling the frame after a live resize, and whether AfxForceVisibleDisplay ever rescues an
' off-screen window -- none of that is reachable from here, and all of it is the author's
' interactive pass. Every assertion below would stay green with the frame painted black.
'
' It restores the dock state, visibility and current tab on the way out.
' ========================================================================================
sub frmOutputFloat_RunSelfTest()
    if environ("TIKO_OUTPUTFLOAT_SELFTEST") <> "1" then exit sub
    if IsWindow( HWND_FRMOUTPUT ) = 0 then exit sub

    gOutPanelTestPass = 0
    gOutPanelTestFail = 0
    print "--- TIKO_OUTPUTFLOAT_SELFTEST ---"

    dim as boolean bWasFloating = frmOutputFloat_IsFloating()
    dim as long    nTabAtStart  = frmOutput_GetCurTab()
    dim as boolean bVisAtStart  = iif( IsWindowVisible( frmOutputFloat_VisibilityTarget() ), true, false )

    ' Start from a known state: docked.
    if bWasFloating then frmOutputFloat_Dock()

    ' --- the localized strings ----------------------------------------------------------
    ' L() DISCARDS its default (it is #Define L(e,s) LL(e)), so an id missing from
    ' english.lang renders as an EMPTY caption -- a blank menu row and a blank tooltip, with
    ' nothing in the build or the warnings to say so.
    frmOutput_Check( "lang: 404 (Undock Output Window) is not blank", len(L(393,"")) > 0 )
    frmOutput_Check( "lang: 405 (Dock Output Window) is not blank",   len(L(394,"")) > 0 )
    frmOutput_Check( "lang: 406 (the float window caption) is not blank", len(L(395,"")) > 0 )

    ' --- initial state -------------------------------------------------------------------
    frmOutput_Check( "state: starts docked", frmOutputFloat_IsFloating() = false )
    frmOutput_Check( "state: no frame exists while docked", HWND_FRMOUTPUTFLOAT = 0 )
    frmOutput_Check( "state: the visibility target is the panel while docked", _
        frmOutputFloat_VisibilityTarget() = HWND_FRMOUTPUT )

    ' --- snapshot everything the round trip must preserve --------------------------------
    dim as HWND hSelBar0 = HWND_FRMOUTPUT_SELECTBAR
    dim as HWND hUndock0 = HWND_FRMOUTPUT_UNDOCK
    dim as HWND hClose0  = HWND_FRMOUTPUT_CLOSE
    dim as HWND hResults0 = HWND_FRMOUTPUT_LVRESULTS
    dim as HWND hTodo0   = HWND_FRMOUTPUT_LVTODO
    dim as HWND hScroll0 = HWND_FRMOUTPUT_VSCROLL
    dim as HWND hNotes0  = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES )
    dim as HWND hLog0    = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE )

    ' A sentinel in the LOG buffer, deliberately NOT in the Notes one.
    '
    ' The Notes box looks like the obvious probe and is useless as one: every
    ' frmMain_PositionWindows pass ends in frmOutput_ShowNotes, which re-seeds it from
    ' gApp.NonProjectNotes -- so its text is REGENERATED across the round trip rather than
    ' preserved, and it would read as intact even under a destroy-and-recreate. (Measured:
    ' a sentinel written here came back as the stored notes, and that is the code behaving
    ' correctly.) Nothing re-seeds the compiler log, so it is the pane whose content is real
    ' evidence. SetText is programmatic and therefore silent -- no change callback fires.
    dim as DWSTRING wszLog0 = PsTextBox_GetText( hLog0 )
    PsTextBox_SetText( hLog0, "tiko-outputfloat-selftest-sentinel" )

    dim as long nResults0 = PsListTree_GetCount( hResults0 )
    dim as long nHeight0  = gConfig.ShowOutputPanelHeight
    dim as long nMin0     = gConfig.ShowOutputPanelMinimized
    dim as DWSTRING wszGlyphDocked = PsIconPanel_GetGlyph( HWND_FRMOUTPUT_UNDOCK, 0 )

    ' Own WS_VISIBLE bit, not IsWindowVisible: that walks the ancestor chain, so with the
    ' Output panel merely CLOSED every one of its children reads as invisible and these
    ' three assertions would be testing the panel's visibility instead of the button's.
    ' (The layout code hit the identical trap; see frmOutput_PositionWindows.)
    frmOutput_Check( "icon: the ""X"" is shown while docked", _
        (GetWindowLongPtr( HWND_FRMOUTPUT_CLOSE, GWL_STYLE ) and WS_VISIBLE) <> 0 )
    frmOutput_Check( "icon: the docked tooltip is not blank", _
        len( PsIconPanel_GetTooltipText( HWND_FRMOUTPUT_UNDOCK, 0 ) ) > 0 )

    ' --- undock --------------------------------------------------------------------------
    frmOutputFloat_Undock()

    frmOutput_Check( "undock: a frame now exists", frmOutputFloat_IsFloating() )
    frmOutput_Check( "undock: the panel's parent IS the frame", _
        GetParent( HWND_FRMOUTPUT ) = HWND_FRMOUTPUTFLOAT )
    frmOutput_Check( "undock: the panel's root ancestor is the frame", _
        GetAncestor( HWND_FRMOUTPUT, GA_ROOT ) = HWND_FRMOUTPUTFLOAT )
    ' THE INVARIANT: this is a child-to-child move, so WS_CHILD stays ON. Clearing it is
    ' only correct when a window becomes top-level itself, which this design avoids -- and a
    ' later "tidy-up" that adds the style surgery should turn this red rather than break
    ' things quietly.
    frmOutput_Check( "undock: the panel is STILL WS_CHILD", _
        (GetWindowLongPtr( HWND_FRMOUTPUT, GWL_STYLE ) and WS_CHILD) <> 0 )
    frmOutput_Check( "undock: the frame is NOT WS_CHILD", _
        (GetWindowLongPtr( HWND_FRMOUTPUTFLOAT, GWL_STYLE ) and WS_CHILD) = 0 )
    frmOutput_Check( "undock: the frame carries WS_EX_TOOLWINDOW (no taskbar button)", _
        (GetWindowLongPtr( HWND_FRMOUTPUTFLOAT, GWL_EXSTYLE ) and WS_EX_TOOLWINDOW) <> 0 )
    ' Load-bearing: without it IsDialogMessage cannot recurse into the panel, so Tab inside
    ' the floating window would reach nothing.
    frmOutput_Check( "undock: the frame carries WS_EX_CONTROLPARENT", _
        (GetWindowLongPtr( HWND_FRMOUTPUTFLOAT, GWL_EXSTYLE ) and WS_EX_CONTROLPARENT) <> 0 )
    frmOutput_Check( "undock: frmMain owns the frame", _
        GetWindow( HWND_FRMOUTPUTFLOAT, GW_OWNER ) = HWND_FRMMAIN )
    frmOutput_Check( "undock: the visibility target is now the frame", _
        frmOutputFloat_VisibilityTarget() = HWND_FRMOUTPUTFLOAT )
    ' The docked space really did collapse. Read off the splitter, which is the window
    ' frmMain_PositionWindows hides on the else branch.
    frmOutput_Check( "undock: the docked splitter is hidden", _
        IsWindowVisible( HWND_FRMMAIN_SPLITOUTPUT ) = 0 )
    frmOutput_Check( "undock: the panel itself is shown (the FRAME carries visibility)", _
        (GetWindowLongPtr( HWND_FRMOUTPUT, GWL_STYLE ) and WS_VISIBLE) <> 0 )
    ' NOT cleared -- banked, like the height beside it. This assertion used to say the
    ' opposite and was asserting a defect: undocking a minimized panel and closing the float
    ' brought it back sprung open. The flag is unobservable while floating, so nothing else
    ' in this suite can see it.
    frmOutput_Check( "undock: the docked MINIMIZED flag is banked, not cleared", _
        gConfig.ShowOutputPanelMinimized = nMin0, str(gConfig.ShowOutputPanelMinimized) )

    ' The icon flipped and the "X" went away.
    frmOutput_Check( "icon: the glyph CHANGED on undock", _
        PsIconPanel_GetGlyph( HWND_FRMOUTPUT_UNDOCK, 0 ) <> wszGlyphDocked )
    frmOutput_Check( "icon: the floating tooltip is not blank", _
        len( PsIconPanel_GetTooltipText( HWND_FRMOUTPUT_UNDOCK, 0 ) ) > 0 )
    frmOutput_Check( "icon: the ""X"" is hidden while floating", _
        (GetWindowLongPtr( HWND_FRMOUTPUT_CLOSE, GWL_STYLE ) and WS_VISIBLE) = 0 )

    ' A HIDDEN control still has a position, and this one used to be parked exactly ONE CELL
    ' PAST the strip's right edge -- so any pass that showed it before the hide caught up
    ' drew a red "X" hanging off the corner of the floating window. Hidden is not the same as
    ' harmless: assert where it is parked, not just that it is invisible.
    scope
        dim as RECT rcPanel, rcClose, rcUndock
        GetClientRect( HWND_FRMOUTPUT, @rcPanel )
        GetWindowRect( HWND_FRMOUTPUT_CLOSE, @rcClose )
        GetWindowRect( HWND_FRMOUTPUT_UNDOCK, @rcUndock )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMOUTPUT, cast(LPPOINT, @rcClose), 2 )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMOUTPUT, cast(LPPOINT, @rcUndock), 2 )
        frmOutput_Check( "icon: the hidden ""X"" is parked INSIDE the panel, not past its edge", _
            (rcClose.right <= rcPanel.right) andalso (rcClose.left >= rcPanel.left), _
            str(rcClose.left) & ".." & str(rcClose.right) & " in 0.." & str(rcPanel.right) )
        frmOutput_Check( "icon: the dock icon takes the rightmost cell while floating", _
            rcUndock.right >= rcPanel.right - AfxScaleX(2), _
            str(rcUndock.right) & " vs " & str(rcPanel.right) )
    end scope

    ' CaptureState must not overwrite the banked docked height with the frame's client
    ' height. Drive it with a witness rather than trusting the guard by inspection.
    dim as long nWitness = 777
    gConfig.ShowOutputPanelHeight = nWitness
    frmOutput_CaptureState()
    frmOutput_Check( "capture: the DOCKED height is left alone while floating", _
        gConfig.ShowOutputPanelHeight = nWitness, str(gConfig.ShowOutputPanelHeight) )
    frmOutput_Check( "capture: OutputFloating is recorded", gConfig.OutputFloating = 1 )
    frmOutput_Check( "capture: the geometry is a real rectangle", _
        (gConfig.OutputFloatRight > gConfig.OutputFloatLeft) andalso _
        (gConfig.OutputFloatBottom > gConfig.OutputFloatTop), _
        str(gConfig.OutputFloatLeft) & "," & str(gConfig.OutputFloatTop) & "," & _
        str(gConfig.OutputFloatRight) & "," & str(gConfig.OutputFloatBottom) )
    gConfig.ShowOutputPanelHeight = nHeight0

    ' Visibility is ORTHOGONAL to dock state: hiding a floating panel must hide the FRAME
    ' and leave it floating.
    frmOutputFloat_SetVisible( false )
    frmOutput_Check( "visible: hiding a floating panel hides the frame", _
        IsWindowVisible( HWND_FRMOUTPUTFLOAT ) = 0 )
    frmOutput_Check( "visible: hiding it does NOT re-dock", frmOutputFloat_IsFloating() )
    frmOutputFloat_SetVisible( true )
    frmOutput_Check( "visible: showing it again shows the frame", _
        IsWindowVisible( HWND_FRMOUTPUTFLOAT ) <> 0 )

    ' A VISIBLE PANEL MUST UNDOCK INTO A VISIBLE FRAME, and this suite is the right place to
    ' assert it because of WHEN it runs: from frmMain_Show, before frmMain has been placed
    ' and shown. IsWindowVisible walks the ancestor chain, so at this moment it answers FALSE
    ' for a panel whose own WS_VISIBLE bit is set -- which is exactly the startup restore's
    ' situation. Reading it the wrong way created the float and never showed it, and the next
    ' save then captured ShowOutputPanel = false off that hidden frame, losing the setting
    ' for good. Driven from the style bit on both sides so the assertion cannot inherit the
    ' very mistake it is testing for.
    frmOutputFloat_Dock()
    ShowWindow( HWND_FRMOUTPUT, SW_SHOW )
    frmOutput_Check( "startup: the panel's own WS_VISIBLE bit is set", _
        (GetWindowLongPtr( HWND_FRMOUTPUT, GWL_STYLE ) and WS_VISIBLE) <> 0 )
    frmOutputFloat_Undock()
    frmOutput_Check( "startup: undocking a VISIBLE panel yields a VISIBLE frame", _
        (GetWindowLongPtr( HWND_FRMOUTPUTFLOAT, GWL_STYLE ) and WS_VISIBLE) <> 0 )
    ' And the round trip that lost the setting: capture from that frame must report it
    ' visible, not false.
    frmOutput_CaptureState()
    frmOutput_Check( "startup: CaptureState reports the visible float as visible", _
        gConfig.ShowOutputPanel <> 0, str(gConfig.ShowOutputPanel) )

    ' The mirror, so this pair cannot pass by simply always showing the frame.
    frmOutputFloat_Dock()
    ShowWindow( HWND_FRMOUTPUT, SW_HIDE )
    frmOutputFloat_Undock()
    frmOutput_Check( "startup: undocking a HIDDEN panel yields a HIDDEN frame", _
        (GetWindowLongPtr( HWND_FRMOUTPUTFLOAT, GWL_STYLE ) and WS_VISIBLE) = 0 )

    ' --- dock ----------------------------------------------------------------------------
    frmOutputFloat_Dock()

    frmOutput_Check( "dock: no frame remains", frmOutputFloat_IsFloating() = false )
    frmOutput_Check( "dock: the frame handle was zeroed", HWND_FRMOUTPUTFLOAT = 0 )
    frmOutput_Check( "dock: the panel's parent is frmMain again", _
        GetParent( HWND_FRMOUTPUT ) = HWND_FRMMAIN )
    frmOutput_Check( "dock: the panel's root ancestor is frmMain", _
        GetAncestor( HWND_FRMOUTPUT, GA_ROOT ) = HWND_FRMMAIN )
    frmOutput_Check( "dock: the panel is STILL WS_CHILD", _
        (GetWindowLongPtr( HWND_FRMOUTPUT, GWL_STYLE ) and WS_CHILD) <> 0 )
    frmOutput_Check( "dock: the ""X"" is shown again", _
        (GetWindowLongPtr( HWND_FRMOUTPUT_CLOSE, GWL_STYLE ) and WS_VISIBLE) <> 0 )
    frmOutput_Check( "dock: the glyph flipped back", _
        PsIconPanel_GetGlyph( HWND_FRMOUTPUT_UNDOCK, 0 ) = wszGlyphDocked )
    frmOutput_Check( "dock: the stored docked height round-tripped unchanged", _
        gConfig.ShowOutputPanelHeight = nHeight0, _
        str(gConfig.ShowOutputPanelHeight) & " vs " & str(nHeight0) )

    ' --- CONTENT SURVIVAL: the group this design exists for -------------------------------
    ' Every handle must be the SAME NUMBER, not merely a live window. That is what separates
    ' "reparented" from "recreated": a recreate produces perfectly valid controls with
    ' different handles and empty contents, and passes every assertion above.
    frmOutput_Check( "survive: the tab bar is the same window", _
        (HWND_FRMOUTPUT_SELECTBAR = hSelBar0) andalso IsWindow(hSelBar0) )
    frmOutput_Check( "survive: the undock icon panel is the same window", _
        (HWND_FRMOUTPUT_UNDOCK = hUndock0) andalso IsWindow(hUndock0) )
    frmOutput_Check( "survive: the close icon panel is the same window", _
        (HWND_FRMOUTPUT_CLOSE = hClose0) andalso IsWindow(hClose0) )
    frmOutput_Check( "survive: the results list is the same window", _
        (HWND_FRMOUTPUT_LVRESULTS = hResults0) andalso IsWindow(hResults0) )
    frmOutput_Check( "survive: the TODO list is the same window", _
        (HWND_FRMOUTPUT_LVTODO = hTodo0) andalso IsWindow(hTodo0) )
    frmOutput_Check( "survive: the shared scrollbar is the same window", _
        (HWND_FRMOUTPUT_VSCROLL = hScroll0) andalso IsWindow(hScroll0) )
    frmOutput_Check( "survive: the Notes box is the same window", _
        (GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES ) = hNotes0) andalso IsWindow(hNotes0) )
    frmOutput_Check( "survive: the Log box is the same window", _
        (GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE ) = hLog0) andalso IsWindow(hLog0) )
    frmOutput_Check( "survive: every child is parented to the panel again", _
        (GetParent( hSelBar0 ) = HWND_FRMOUTPUT) andalso _
        (GetParent( hResults0 ) = HWND_FRMOUTPUT) andalso _
        (GetParent( hNotes0 ) = HWND_FRMOUTPUT) )
    ' The CONTENT, not just the windows.
    ' instr, not equality: a multiline RichEdit legitimately hands back a trailing newline,
    ' so an exact compare would report a content loss that has not happened. What is being
    ' asserted is that the buffer still HOLDS the sentinel -- a recreate would give back an
    ' empty box. Reported as a LENGTH, because a DWSTRING pushed through this harness's
    ' `string` parameter prints with its nulls interleaved.
    dim as DWSTRING wszLogBack = PsTextBox_GetText( hLog0 )
    frmOutput_Check( "survive: the compiler-log TEXT came back", _
        PsInStr( wszLogBack, "tiko-outputfloat-selftest-sentinel" ) > 0, _
        "len=" & str(PsLen(wszLogBack)) )
    ' The Notes box, by contrast, is re-seeded from the global on every layout pass -- so
    ' this asserts the RE-SEED, which is what correct behaviour looks like here, and records
    ' why the survival probe above is not pointed at it.
    ' This used to read "... = gApp.NonProjectNotes orelse gApp.IsProjectActive", which was
    ' VACUOUSLY TRUE whenever a project happened to be open -- and every workspace is a
    ' project now, so it could never have failed. There is one notes store to compare
    ' against, so compare against it and nothing else.
    frmOutput_Check( "survive: the Notes box agrees with gApp's stored notes (it is re-seeded)", _
        PsTextBox_GetText( hNotes0 ) = gApp.ProjectNotes )
    frmOutput_Check( "survive: the results row count is unchanged", _
        PsListTree_GetCount( hResults0 ) = nResults0, _
        str(PsListTree_GetCount(hResults0)) & " vs " & str(nResults0) )
    frmOutput_Check( "survive: the current tab is unchanged", _
        frmOutput_GetCurTab() = nTabAtStart, str(frmOutput_GetCurTab()) )

    ' --- the class background brushes -----------------------------------------------------
    ' The white flash on open was the SYSTEM's own erase, which happens inside ShowWindow
    ' before the window procedure is reached -- so WM_ERASEBKGND returning TRUE could not
    ' prevent it, and only the CLASS brush can. CWindow registers COLOR_3DFACE+1
    ' (CWindow.inc:378); both windows now carry a brush of the colour their WM_PAINT draws.
    ' Asserted as "not the default" as well as "not null", because the default is a system
    ' colour INDEX rather than a handle and would otherwise read as a plausible brush.
    frmOutput_Check( "flash: the panel's class brush is ours", _
        (ghOutputPanelBrush <> 0) andalso _
        (GetClassLongPtr( HWND_FRMOUTPUT, GCLP_HBRBACKGROUND ) = cast(ULONG_PTR, ghOutputPanelBrush)) )
    frmOutput_Check( "flash: the panel's class brush is NOT the CWindow default", _
        GetClassLongPtr( HWND_FRMOUTPUT, GCLP_HBRBACKGROUND ) <> cast(ULONG_PTR, COLOR_3DFACE + 1) )

    frmOutputFloat_Undock()
    frmOutput_Check( "flash: the frame's class brush is ours", _
        (ghOutFloatBrush <> 0) andalso _
        (GetClassLongPtr( HWND_FRMOUTPUTFLOAT, GCLP_HBRBACKGROUND ) = cast(ULONG_PTR, ghOutFloatBrush)) )
    frmOutput_Check( "flash: the frame's class brush is NOT the CWindow default", _
        GetClassLongPtr( HWND_FRMOUTPUTFLOAT, GCLP_HBRBACKGROUND ) <> cast(ULONG_PTR, COLOR_3DFACE + 1) )

    ' A theme change must REPLACE it, not leave the old colour to flash on the next erase.
    dim as HBRUSH hBrushBefore = ghOutFloatBrush
    frmOutputFloat_SyncBrush()
    frmOutput_Check( "flash: a theme sync rebuilds the frame brush", _
        (ghOutFloatBrush <> 0) andalso (ghOutFloatBrush <> hBrushBefore) )
    frmOutput_Check( "flash: and the class points at the NEW brush", _
        GetClassLongPtr( HWND_FRMOUTPUTFLOAT, GCLP_HBRBACKGROUND ) = cast(ULONG_PTR, ghOutFloatBrush) )
    frmOutputFloat_Dock()

    ' --- the brushes do not leak ----------------------------------------------------------
    ' Each undock now CREATES a brush and each dock destroys the window that owned it. A
    ' missed DeleteObject would be invisible in every assertion above and would bleed GDI
    ' handles for as long as the session lasts. Counted rather than reasoned about.
    ' SIXTY cycles, not six, and the count is why. At six the leak is six objects, which sat
    ' inside the noise of what the rest of the process churns while this runs -- the
    ' assertion passed with the DeleteObject removed, i.e. it was worth nothing. At sixty the
    ' signal is unmistakable: measured 179 -> 238 leaking, flat when correct.
    dim as long nGdi0 = GetGuiResources( GetCurrentProcess(), 0 )
    for i as long = 1 to 60
        frmOutputFloat_Undock()
        frmOutputFloat_Dock()
    next
    dim as long nGdi1 = GetGuiResources( GetCurrentProcess(), 0 )
    frmOutput_Check( "flash: 60 undock/dock cycles do not leak GDI objects", _
        (nGdi1 - nGdi0) < 30, str(nGdi0) & " -> " & str(nGdi1) )

    ' --- the MINIMIZED docked state survives the float ------------------------------------
    ' Driven explicitly over BOTH values rather than riding on whatever the app happened to
    ' start in: the reported defect only shows from one of the two, so a suite that tests
    ' only the state it inherits is green about half the time for no reason it can name.
    ' The panel must come back at the TABS height when it left minimized, and at the user's
    ' height when it did not -- the flag alone is not enough, since the first version of this
    ' fix could have restored the flag and still sized the window wrong.
    dim pWin as CWindow ptr = AfxCWindowPtr( HWND_FRMOUTPUT )
    if pWin then
        for nWantMin as long = 1 to 0 step -1
            gConfig.ShowOutputPanelMinimized = nWantMin
            gConfig.ShowOutputPanelHeight    = nHeight0

            frmOutputFloat_Undock()
            frmOutput_Check( "minimized(" & str(nWantMin) & "): the flag survives the undock", _
                gConfig.ShowOutputPanelMinimized = nWantMin, _
                str(gConfig.ShowOutputPanelMinimized) )

            frmOutputFloat_Dock()
            frmOutput_Check( "minimized(" & str(nWantMin) & "): the flag survives the dock", _
                gConfig.ShowOutputPanelMinimized = nWantMin, _
                str(gConfig.ShowOutputPanelMinimized) )

            dim as long nWantH = iif( nWantMin <> 0, OUTPUT_TABS_HEIGHT, nHeight0 )
            dim as long nGotH  = pWin->UnScaleY( AfxGetWindowHeight( HWND_FRMOUTPUT ) )
            frmOutput_Check( "minimized(" & str(nWantMin) & "): the panel docks at the right height", _
                nGotH = nWantH, str(nGotH) & " vs " & str(nWantH) )
        next
    end if

    ' --- restore -------------------------------------------------------------------------
    PsTextBox_SetText( hLog0, wszLog0 )
    gConfig.ShowOutputPanelMinimized = nMin0
    gConfig.ShowOutputPanelHeight    = nHeight0
    frmOutput_SetCurTab( nTabAtStart )
    if bWasFloating then frmOutputFloat_Undock()
    frmOutputFloat_SetVisible( bVisAtStart )

    print "--- TIKO_OUTPUTFLOAT_SELFTEST: " & str(gOutPanelTestPass) & " passed, " & _
          str(gOutPanelTestFail) & " failed ---"
end sub
