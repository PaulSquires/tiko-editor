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

'  modThemeApply.inc  --  the theme APPLY layer. See modThemeApply.bi for why this is a
'  separate file and why it is included at tiko.bas:173, immediately before frmMain.inc.
'
'  THE ONE RULE: PUSH, THEN INVALIDATE.
'    tiko's surface is drawn two different ways and they need opposite treatment.
'
'      * ~26 host paint callbacks (explorer/functions/bookmarks rows, top tabs, status bars,
'        splitters, editor scrollbars, output panes) read the gh* globals LIVE at paint time.
'        Those need nothing but an invalidate.
'      * Every Ps* control COPIES its colours at Set time and never looks at a global again.
'        Those need an explicit re-push first, or an invalidate just redraws them in exactly
'        the colours they already had -- which looks precisely like "the theme didn't change".
'
'    So a subsystem apply is always: refill globals -> push to that subsystem's controls ->
'    invalidate that subsystem's windows.


''
''  REPAINT A WINDOW AND ITS CHILDREN
''
sub Theme_RedrawDeep( byval hwnd as HWND )
    if IsWindow(hwnd) = 0 then exit sub
    ' RDW_UPDATENOW rather than a deferred paint: the live preview has to land inside the
    ' 30ms coalescing tick or a drag looks like it is lagging a frame behind the cursor.
    RedrawWindow( hwnd, null, null, _
                  RDW_INVALIDATE or RDW_ERASE or RDW_ALLCHILDREN or RDW_UPDATENOW )
end sub


''
''  THE WINDOW TABLE
''
''  Pointers TO the HWND_FRM* globals, not copies of them: several are assigned long after
''  startup and a few are reassigned when their window is recreated, so a snapshot of the
''  handle would go stale.
''
''  Deliberately NOT EnumWindows. That would descend into the Ps* controls' own internals
''  (every PsListTree owns an inner listbox plus a PsVScrollBar, every PsNumericUpDown owns a
''  PsTextBox), visit subsystems the caller deliberately did not route to, and -- the real
''  objection -- it cannot tell "needs a colour push" from "needs an invalidate", which is the
''  only distinction that matters here.
''
type THEMEWND_TYPE
    phWnd   as HWND ptr
    nSubsys as ulong
    bDeep   as boolean      ' true = container form, redraw its children too
end type
dim shared gThemeWnd(any) as THEMEWND_TYPE

private sub ThemeWnd_Add( byval phWnd as HWND ptr, _
                          byval nSubsys as ulong, _
                          byval bDeep as boolean = false )
    if phWnd = 0 then exit sub
    dim as long n = ubound(gThemeWnd) + 1
    redim preserve gThemeWnd(n) as THEMEWND_TYPE
    gThemeWnd(n).phWnd   = phWnd
    gThemeWnd(n).nSubsys = nSubsys
    gThemeWnd(n).bDeep   = bDeep
end sub


private sub Theme_BuildWndTable()

    static as boolean bBuilt
    if bBuilt then exit sub
    bBuilt = true

    erase gThemeWnd

    ' ---- container forms: redraw children too -----------------------------------------
    ThemeWnd_Add( @HWND_FRMMAIN, THSUB_FRAME or THSUB_EDITORCHROME or THSUB_SCINTILLA, true )
    ThemeWnd_Add( @HWND_FRMOUTPUT,             THSUB_OUTPUT,       true )
    ' The Output panel's floating frame. This table is built once, but it stores the ADDRESS
    ' of each handle, so a window created later in the session (as this one always is) is
    ' picked up with no rebuild -- which is exactly why that indirection is there.
    ThemeWnd_Add( @HWND_FRMOUTPUTFLOAT,        THSUB_OUTPUT,       true )
    ThemeWnd_Add( @HWND_FRMPANEL,              THSUB_PANEL,        true )
    ThemeWnd_Add( @HWND_FRMOPTIONS,            THSUB_OPTIONSDLG,   true )
    ThemeWnd_Add( @HWND_FRMHELPCENTER,         THSUB_HELPCENTER,   false )
    ThemeWnd_Add( @HWND_FRMDEBUG,              THSUB_DEBUG,        true )
    ThemeWnd_Add( @HWND_FRMSEARCHSYMBOL,       THSUB_SEARCHSYMBOL, true )
    ThemeWnd_Add( @HWND_FRMMAIN_FIND,   THSUB_TOPTABS,      true )
    ThemeWnd_Add( @HWND_FRMMAIN_REPLACE, THSUB_TOPTABS,     true )

    ' ---- leaf controls -----------------------------------------------------------------
    ' Most of these are children of frmMain and would be caught by its deep redraw, but only
    ' when the mask happens to include FRAME/EDITORCHROME/SCINTILLA. Listing them under their
    ' OWN subsystem is what lets a menubar-only change repaint just the menubar.
    ThemeWnd_Add( @HWND_FRMMAIN_MENUBAR,       THSUB_MENUBAR )
    ThemeWnd_Add( @HWND_FRMMAIN_STATUSBAR,     THSUB_STATUSBAR )
    ThemeWnd_Add( @HWND_FRMMAIN_TOPTABS,       THSUB_TOPTABS )
    ThemeWnd_Add( @HWND_FRMMAIN_TOPTABSMENU,   THSUB_TOPTABS )
    ThemeWnd_Add( @HWND_FRMMAIN_TOPTABSINFO,   THSUB_TOPTABS )
    ThemeWnd_Add( @HWND_FRMEXPLORER,           THSUB_PANEL )
    ThemeWnd_Add( @HWND_FRMFUNCTIONS,          THSUB_PANEL )
    ThemeWnd_Add( @HWND_FRMBOOKMARKS,          THSUB_PANEL )
    ThemeWnd_Add( @HWND_FRMPANEL_MENU,         THSUB_PANEL )
    ThemeWnd_Add( @HWND_FRMPANEL_MENU_RIGHT,   THSUB_PANEL )
    ThemeWnd_Add( @HWND_FRMPANEL_VSCROLLBAR,   THSUB_PANEL )
    ThemeWnd_Add( @HWND_FRMAUTOCOMPLETE,       THSUB_AUTOCOMPLETE )
    ThemeWnd_Add( @HWND_FRMMAIN_SPLITV,        THSUB_EDITORCHROME )
    ThemeWnd_Add( @HWND_FRMMAIN_SPLITH,        THSUB_EDITORCHROME )
    ThemeWnd_Add( @HWND_FRMMAIN_SPLITOUTPUT,   THSUB_EDITORCHROME )
    ThemeWnd_Add( @HWND_FRMMAIN_SPLITPANEL,    THSUB_EDITORCHROME )
    for i as long = 0 to 1
        ThemeWnd_Add( @HWND_FRMEDITOR_VSCROLLBAR(i), THSUB_EDITORCHROME )
        ThemeWnd_Add( @HWND_FRMEDITOR_HSCROLLBAR(i), THSUB_EDITORCHROME )
    next

end sub


''
''  INVALIDATE THE WINDOWS OF THE GIVEN SUBSYSTEMS
''
sub Theme_InvalidateSubsystems( byval nMask as ulong )

    Theme_BuildWndTable()

    for i as long = lbound(gThemeWnd) to ubound(gThemeWnd)
        if (gThemeWnd(i).nSubsys and nMask) = 0 then continue for
        dim as HWND h = *gThemeWnd(i).phWnd
        if IsWindow(h) = 0 then continue for
        if gThemeWnd(i).bDeep then
            Theme_RedrawDeep( h )
        else
            InvalidateRect( h, null, true )
        end if
    next

end sub


''
''  RE-PUSH THE PANEL COLOURS INTO ONE PsListTree
''
''  frmExplorer / frmFunctions / frmBookmarks / the help viewer's topic list all carried the
''  identical three lines inside their _Show, which is exactly why a theme change never
''  reached them. Written once rather than four times so the four cannot drift.
''
''  No invalidate here on purpose: Theme_ApplySubsystems repaints afterwards, and the design
''  rule is push-then-invalidate in that order. (PsListTree_SetBackColor is one of the family's
''  silent setters -- it stores without repainting -- so the invalidate is not optional, it is
''  just somebody else's job.)
''
private sub Theme_PushPanelList( byval hList as HWND )
    if IsWindow(hList) = 0 then exit sub
    PsListTree_SetBackColor( hList, ghPanel.BackColor )
    PsListTree_SetScrollBarColors( hList, ghPanel.BackColorScrollBar, _
                                  ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
end sub


''
''  RE-PUSH THE OUTPUT COLOURS INTO ONE PsListTree
''
private sub Theme_PushOutputList( byval hList as HWND )
    if IsWindow(hList) = 0 then exit sub
    PsListTree_SetBackColor( hList, ghOutput.BackColor )
    PsListTree_SetScrollBarColors( hList, ghOutput.BackColorScrollBar, _
                                  ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )
end sub


''
''  RE-PUSH THE TOP-TAB STRIP COLOURS INTO ONE PsTextBox
''
''  Colours only. The cue banner TEXT and any current content are deliberately not touched --
''  they are localized strings and user data, not theme, and re-setting them here would wipe
''  whatever the user had typed in the Find box the moment they changed a colour.
''
private sub Theme_PushTopTabsTextBox( byval hTextBox as HWND )
    if IsWindow(hTextBox) = 0 then exit sub
    PsTextBox_SetForeColor( hTextBox, ghTopTabs.ForeColorHot )
    PsTextBox_SetBackColor( hTextBox, ghTopTabs.BackColorHot )
    PsTextBox_SetCueBannerColor( hTextBox, ghTopTabs.ForeColor )
end sub


''
''  PUSH THE CURRENT THEME AND FONTS INTO ONE PsTooltip
''
''  Deliberately NOT driven from Theme_ApplySubsystems, and that is the whole design: the tips
''  belonging to frmPanel, frmDebug and the three top-tab bars are created once and live for the
''  life of the window, so a re-push would need a registry of them kept in step with every
''  create and destroy. Calling this from each tip's TEXT callback instead makes the question go
''  away -- the callback fires once per hover, just before the tip is sized, so whatever the
''  theme is at that moment is what the tip wears. See the note in modThemeApply.bi for why the
''  FONT in particular has to be set at that point rather than later.
''
''  The colour mapping is frmUserTools' and frmThemes' recipe, unchanged, so all the editor's
''  tips agree.
''
' ========================================================================================
' Push the current theme into the top-tabs info bar's icon strip.
' ========================================================================================
' Called from the strip's own create (through the declaration in modThemeApply.bi) and again
' on every THSUB_TOPTABS apply. Only the colours the CONTROL owns are set here: the cells
' themselves are drawn by frmTopTabsInfo_IconPaint, which reads ghTopTabs live.
private sub Theme_PushStripIcons( byval hPanel as HWND )
    if IsWindow(hPanel) = 0 then exit sub
    dim as PSICONPANEL_COLORS c
    PsIconPanel_GetColors( hPanel, @c )
    ' The strip's field colour, not the tab area's -- a hovered button inverts TO BackColor,
    ' so idle is the hot one. Same inversion the bar has always drawn.
    c.BackColor         = ghTopTabs.BackColorHot
    c.ForeColor         = ghTopTabs.ForeColorHot
    c.BackColorHot      = ghTopTabs.BackColor
    c.ForeColorHot      = ghTopTabs.ForeColorHot
    c.BackColorSelect   = ghTopTabs.BackColor
    c.ForeColorSelect   = ghTopTabs.ForeColorHot
    c.SeparatorColor    = ghEditor.Divider
    PsIconPanel_SetColors( hPanel, @c )
end sub

sub Theme_ApplyTopTabsIcons()
    Theme_PushStripIcons( ghTopTabsInfoIcons )
end sub

sub Theme_ApplyFindIcons()
    Theme_PushStripIcons( ghFindIconsFold )
    Theme_PushStripIcons( ghFindIconsField )
    Theme_PushStripIcons( ghFindIconsNav )
    Theme_PushStripIcons( ghFindIconsClose )
end sub

sub Theme_ApplyReplaceIcons()
    Theme_PushStripIcons( ghReplaceIconsField )
    Theme_PushStripIcons( ghReplaceIconsActions )
end sub


' The one place the tooltip palette is built. Both doors below read it, so a hand-built tip
' (frmFind, frmPanel, ...) and a control's PsTipHost-owned tip cannot come to disagree.
' PRIVATE, and it has to be: modThemeApply.bi is included at tiko.bas:86, well before
' PsTooltip.bi at 148, so a declaration naming PSTOOLTIP_COLORS cannot live in the header.
private sub Theme_FillTooltipColors( byref tipc as PSTOOLTIP_COLORS )
    tipc.BackColor   = ghPopup.BackColor
    tipc.BorderColor = ghEditor.Divider
    tipc.ForeColor   = ghPopup.ForeColor
    tipc.TitleColor  = ghPanel.ForeColorHot
    tipc.GlyphColor  = ghUI.AccentColor
end sub


' Theme ONE tip -- the door the hand-built tips (frmFind, frmPanel, frmReplace, frmDebug,
' frmTopTabsInfo) call right after creating theirs, and the one Theme_ApplyHoverTips uses
' per control.
'
' It re-arms and then replays the defaults rather than setting colours and fonts itself.
' That is deliberate: a tip created by a CONTROL is themed by PsTooltip_Create applying the
' armed defaults, and a tip themed here used to set its own font triple a few lines away
' from the one being armed. Two places, one look, and they had already been written with
' different fonts once. Now there is a single source and no way for the two kinds of tip to
' diverge.
sub Theme_ApplyTooltip( byval hTip as HWND )
    if IsWindow(hTip) = 0 then exit sub
    Theme_ArmTooltipDefaults()
    PsTooltip_ApplyDefaults( hTip )
end sub


' ========================================================================================
' Theme_ArmTooltipDefaults
'
' The tips owned by a control through PsTipHost are created by the control, not by us, so
' there is no call site at which to theme them. PsTooltip's process-wide defaults are the
' documented door: Create applies whatever is armed here to every new tip.
'
' MUST run before any control switches to PSTIP_MODE_PS -- an already-created tip is NOT
' retro-fitted (that is what Theme_ApplyTooltip is for, and what Theme_ApplyHoverTips does
' on a theme change).
'
' The max width is armed here because PsTooltip wraps natively. It replaces the hand-sent
' TTM_SETMAXTIPWIDTH that frmFunctions and frmTopTabsMenu used to push at the comctl32 tip
' -- frmFunctions did it from inside its tooltip callback, i.e. on every tip.
' ========================================================================================
sub Theme_ArmTooltipDefaults()
    dim as PSTOOLTIP_COLORS tipc
    Theme_FillTooltipColors( tipc )
    PsTooltip_SetDefaultColors( @tipc )
    ' THE SYSTEM'S OWN TOOLTIP FACE (SPI_GETNONCLIENTMETRICS lfStatusFont), built once in
    ' frmMain and guaranteed non-zero there, so there is no fallback branch to write here.
    ' A tip drawn in a different face and size from every other tip on the machine reads as
    ' broken -- the same argument PsTooltip already makes for deriving its delays from
    ' GetDoubleClickTime rather than picking a number.
    '
    ' This is the single line to change if the interactive pass says the system size is wrong:
    ' swapping in ghFont(GUIFONT_10)/ghFont(GUIFONTBOLD_10) restores the hand-picked 10pt.
    '
    ' The glyph stays at SYMBOLFONT_10 -- it is an icon, not text, and it is sized to the
    ' tip's glyph cell rather than to the message.
    PsTooltip_SetDefaultFonts( ghFont(TOOLTIPFONT), ghFont(TOOLTIPFONTBOLD), ghFont(SYMBOLFONT_10) )
    PsTooltip_SetDefaultMaxWidth( AfxScaleX(300) )

    ' Half the hover delay. PsTooltip's own default is comctl32's derivation,
    ' GetDoubleClickTime() -- so this stays tied to the machine's setting rather than
    ' becoming a hard 250, and a user on a slow double-click still gets a proportional wait.
    ' Only the INITIAL delay is armed: -1 leaves auto-pop and reshow on that same derivation.
    dim as long nDbl = GetDoubleClickTime()
    if nDbl <= 0 then nDbl = 500
    PsTooltip_SetDefaultDelays( nDbl \ 2, -1, -1 )
end sub


' ========================================================================================
' Theme_ApplyHoverTips  -- re-theme the PsTipHost-owned tips of the seven adopting controls
'
' Each accessor answers 0 while that control is on the system backend, and
' Theme_ApplyTooltip ignores a non-window, so this is safe for controls that never opted
' in and for those not yet created.
' ========================================================================================
sub Theme_ApplyHoverTips()
    ' No arm call here: Theme_ApplyTooltip re-arms from the current theme itself.
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMEXPLORER ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMFUNCTIONS ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMBOOKMARKS ) )
    Theme_ApplyTooltip( PsIconPanel_GetPsTooltipHandle( HWND_FRMPANEL_MENU ) )
    Theme_ApplyTooltip( PsIconPanel_GetPsTooltipHandle( HWND_FRMPANEL_MENU_RIGHT ) )
    Theme_ApplyTooltip( PsTabBar_GetPsTooltipHandle( HWND_FRMMAIN_TOPTABS ) )
    Theme_ApplyTooltip( PsStatusBar_GetPsTooltipHandle( HWND_FRMMAIN_STATUSBAR ) )
    Theme_ApplyTooltip( PsStatusBar_GetPsTooltipHandle( HWND_FRMMAIN_TOPTABSMENU ) )

    ' The Output panel: two icons and two lists.
    Theme_ApplyTooltip( PsIconPanel_GetPsTooltipHandle( HWND_FRMOUTPUT_UNDOCK ) )
    Theme_ApplyTooltip( PsIconPanel_GetPsTooltipHandle( HWND_FRMOUTPUT_CLOSE ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMOUTPUT_LVRESULTS ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMOUTPUT_LVTODO ) )

    ' The debugger's four panes.
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMDEBUG_LVGLOBALS ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMDEBUG_LVLOCALS ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMDEBUG_LVSTACK ) )
    Theme_ApplyTooltip( PsListTree_GetPsTooltipHandle( HWND_FRMDEBUG_LVWATCH ) )

    ' The debugger's nine buttons. Listed eagerly and unconditionally: the accessor answers 0
    ' for a control on the system backend or a window that does not exist, and
    ' Theme_ApplyTooltip ignores a non-window -- so this costs nothing while the debugger is
    ' closed, which is the usual case.
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        Theme_ApplyTooltip( PsButton_GetPsTooltipHandle( frmDebug_GetButton(id) ) )
    next
    Theme_ApplyTooltip( PsButton_GetPsTooltipHandle( frmDebug_GetButton(IDC_FRMDEBUG_CMDADDWATCH) ) )
    Theme_ApplyTooltip( PsButton_GetPsTooltipHandle( frmDebug_GetButton(IDC_FRMDEBUG_CMDDELWATCH) ) )
end sub


''
''  RE-PUSH COLOURS INTO THE CONTROLS OF THE GIVEN SUBSYSTEMS, THEN REPAINT
''
sub Theme_ApplySubsystems( byval nMask as ulong )

    if nMask = THSUB_NONE then exit sub

    ' Unconditional, and cheap -- roughly 130 pointer stores. Making it selective would need
    ' a second mapping from subsystem back to fields, which is exactly the duplicate table
    ' this design exists to avoid.
    Theme_FillGlobals()

    ' Unconditional for the same reason Theme_FillGlobals is: the seven hover tips belong to
    ' five different subsystems, so gating them would need a second subsystem->control map.
    ' Both halves are cheap and both are no-ops for a control that does not exist yet.
    Theme_ApplyHoverTips()

    ' ---- push: the Ps* controls that copied their colours ------------------------------
    if (nMask and (THSUB_MENUBAR or THSUB_POPUP)) <> 0 then
        ' Also walks the popup registry, so every live PsPopupMenu follows.
        frmMenuBar_SyncTheme()
    end if

    if (nMask and THSUB_AUTOCOMPLETE) <> 0 then
        frmAutoComplete_SyncTheme()
        ' The code tip rides the same submask: it is the other half of the completion UI,
        ' appears in the same place, and reads the same editor palette.
        Codetip_SyncTheme()
    end if

    if (nMask and THSUB_OUTPUT) <> 0 then
        frmOutput_SetControlColors()
        ' The floating frame's own WM_PAINT reads ghOutput live, so the only thing a theme
        ' change has to push at it is the DWM caption -- the Help Center's case exactly.
        if IsWindow(HWND_FRMOUTPUTFLOAT) then Theme_ApplyDarkCaption( HWND_FRMOUTPUTFLOAT )
    end if

    if (nMask and THSUB_OPTIONSDLG) <> 0 then
        if IsWindow(HWND_FRMOPTIONS) then frmOptions_ApplyTheme( HWND_FRMOPTIONS )
    end if

    ' ---- push: controls whose colours were previously set ONLY at create time -----------
    ' These are the sites that made a runtime theme change impossible before. Pushed here,
    ' centrally, rather than as a frmXxx_ApplyTheme() in each of the eight files: every one of
    ' these handles is already a global that IS the control (HWND_FRMEXPLORER etc. are assigned
    ' the listbox handle itself, not a container), so a per-form function would be a wrapper
    ' round a single line. Keeping them together also means the whole theme -> control mapping
    ' is readable on one screen.
    if (nMask and THSUB_PANEL) <> 0 then
        Theme_PushPanelList( HWND_FRMEXPLORER )
        Theme_PushPanelList( HWND_FRMFUNCTIONS )
        Theme_PushPanelList( HWND_FRMBOOKMARKS )
        frmPanelMenu_ApplyTheme()
    end if

    if (nMask and THSUB_STATUSBAR) <> 0 then
        if IsWindow(HWND_FRMMAIN_STATUSBAR) then _
            PsStatusBar_SetBackColor( HWND_FRMMAIN_STATUSBAR, ghStatusBar.BackColor )
    end if

    if (nMask and THSUB_TOPTABS) <> 0 then
        if IsWindow(HWND_FRMMAIN_TOPTABS) then
            PsTabBar_SetBackColor( HWND_FRMMAIN_TOPTABS, ghTopTabs.BackColor )
            PsTabBar_SetBorderColor( HWND_FRMMAIN_TOPTABS, ghEditor.Divider )
        end if
        if IsWindow(HWND_FRMMAIN_TOPTABSMENU) then _
            PsStatusBar_SetBackColor( HWND_FRMMAIN_TOPTABSMENU, ghTopTabs.BackColor )
        Theme_ApplyTopTabsIcons()
        Theme_ApplyFindIcons()
        Theme_ApplyReplaceIcons()
        ' The Find / Replace strips. Both are persistent once created, and their PsTextBoxes
        ' copy colours, so an invalidate alone would leave them on the old theme.
        Theme_PushTopTabsTextBox( GetDlgItem( HWND_FRMMAIN_FIND, _
                                              IDC_FRMFIND_TXTFIND ) )
        Theme_PushTopTabsTextBox( GetDlgItem( HWND_FRMMAIN_REPLACE, _
                                              IDC_FRMREPLACE_TXTREPLACE ) )
        ' The push above just rewrote the find field's colours, so a phrase that is red for
        ' having matched nothing has to be re-reddened. Forced past the change cache, which
        ' cannot see a write it did not make.
        frmFind_ApplyErrorState( true )
    end if

    ' The Help Center is a WebView2 pane: the SITE owns its own colours, so the only thing
    ' a theme change can touch here is the DWM caption.
    if (nMask and THSUB_HELPCENTER) <> 0 then
        Theme_ApplyDarkCaption( HWND_FRMHELPCENTER )
    end if

    ' The debugger's Variables pane. Only ever populated during a live gdb session, so this
    ' is the least-exercised path here -- but it is one line and the handle is a global.
    if (nMask and THSUB_DEBUG) <> 0 then
        ' The debugger owns several controls now, so it themes itself.
        frmDebug_ApplyTheme()
    end if

    ' The main window's non-client area. Cheap, and it has to happen on the preview path too,
    ' not just at load, or the title bar lags a theme change all the way to OK.
    if (nMask and THSUB_FRAME) <> 0 then
        Theme_ApplyDarkCaption( HWND_FRMMAIN )
    end if

    ' ---- push: Scintilla ----------------------------------------------------------------
    ' N documents x 2 split views; ApplyProperties already loops the views itself.
    if (nMask and THSUB_SCINTILLA) <> 0 then
        dim pDoc as clsDocument ptr = gApp.pDocList
        do until pDoc = 0
            pDoc->ApplyProperties
            pDoc = pDoc->pDocNext
        loop
        ' Find in Project's excerpt views are Scintilla views too, but they are NOT in
        ' pDocList -- the loop above cannot reach them, so they would keep the old palette
        ' until something happened to rebind them.
        frmFindInProject_ApplyTheme()
    end if

    ' ---- then, and only then, repaint ---------------------------------------------------
    Theme_InvalidateSubsystems( nMask )

end sub


sub Theme_ApplyAll()
    Theme_ApplySubsystems( THSUB_ALL )
end sub


''
''  LOAD A DIFFERENT THEME INTO MEMORY AND SCHEDULE A FULL APPLY
''
''  The preview path. gConfig is deliberately untouched: the Options dialog stages the user's
''  pick in gOptWork and only commits on OK, and Cancel has to be able to put everything back.
''
function Theme_PreviewFile( byval wszShortFilename as DWSTRING ) as boolean

    dim as DWSTRING wszPath = Theme_ResolvePath( wszShortFilename )
    if len(wszPath) = 0 then return true
    if Theme_ParseFile( wszPath ) then return true

    gThemeCurrentShortFilename = wszShortFilename
    Theme_ResolveFallbacks()

    ' Reset is relative to the theme now being edited, not the one the dialog opened with.
    Theme_CaptureBaseline()

    Theme_RequestApply( THSUB_ALL )

    return false

end function


''
''  COALESCED APPLY
''
''  SetTimer with an id that is already armed RESTARTS it, which is the whole coalescing
''  mechanism -- the same trick PsNumericUpDown uses for auto-repeat. Consequence worth
''  stating out loud so it is not later mistaken for a hang: a CONTINUOUS drag never applies
''  until the user pauses for 30ms. For a colour picker that is the intended feel.
''
dim shared gThemePending as ulong

''
''  TRACE ONE LINE                                             (TIKO_THEME_TRACE=1)
''
''  Exists to answer one specific open question: frmOptions runs its OWN message pump, which
''  offers every message to PsComboBox_FilterMessage and PsTextBox_FilterMessage before
''  dispatching. Reading those says neither eats a WM_TIMER aimed at a foreign hwnd -- but
''  that is inference, not measurement, and if it is wrong the live preview simply never
''  fires while the dialog is open. A request/fire pair in this log settles it.
''
private sub Theme_TraceLine( byval sText as string )
    static as boolean bChecked, bOn
    if bChecked = false then
        bChecked = true
        bOn = (environ("TIKO_THEME_TRACE") = "1")
    end if
    if bOn = false then exit sub
    dim as string sPath = AfxGetExePathName & "theme_trace.log"
    dim as long f = freefile
    if open( sPath for append as #f ) <> 0 then exit sub
    print #f, sText
    close #f
end sub


sub Theme_RequestApply( byval nMask as ulong )
    if nMask = THSUB_NONE then exit sub
    Theme_TraceLine( "REQUEST mask=&h" & hex(nMask) )
    gThemePending or= nMask
    if IsWindow(HWND_FRMMAIN) then
        SetTimer( HWND_FRMMAIN, IDT_THEME_COALESCE, THEME_COALESCE_MS, NULL )
    else
        ' No frame yet (startup): nothing is on screen to update, so just drop it.
        gThemePending = THSUB_NONE
    end if
end sub


sub Theme_OnCoalesceTimer()
    ' Read and CLEAR before doing any work. Anything the apply itself schedules then becomes
    ' a fresh request on the next tick rather than being swallowed here or looping forever.
    dim as ulong nMask = gThemePending
    gThemePending = THSUB_NONE
    if nMask = THSUB_NONE then exit sub
    Theme_TraceLine( "  FIRE mask=&h" & hex(nMask) )
    Theme_ApplySubsystems( nMask )
end sub


''
''  SELF-TEST                                            (TIKO_THEME_SELFTEST=1)
''
''  WHAT THIS CAN AND CANNOT PROVE
''    It cannot prove a pixel changed. The surface is 26 host paint callbacks across a live
''    window tree, and Learnings.md records PrintWindow producing FALSE CONFIDENCE precisely
''    because it forces a WM_PAINT and hides a missing invalidation -- the exact bug class
''    this refactor introduces. A pixel assertion here would be worse than none.
''
''    It also cannot prove a subsystem mask is the RIGHT set. A key routed to STATUSBAR when
''    it should be STATUSBAR or OUTPUT passes everything below and still fails silently in the
''    app. That residual risk is handled structurally rather than assertionally: masks are
''    derived from the namespace so the common case is never hand-written, and the explicit
''    overrides are few enough to read at a glance.
''
sub Theme_RunSelfTest()

    if environ("TIKO_THEME_SELFTEST") <> "1" then exit sub

    dim as long nPass, nFail
    dim as string sLine

    #macro THASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & msg
            ? sLine
        end if
    #endmacro

    ? "--- TIKO_THEME_SELFTEST ---"

    Theme_BuildBindTable()

    ' -- 1. Every bind row resolves to a real key, and to the channels it claims. ---------
    '    Catches a typo'd bind key or a key missing from a shipped theme. Would have caught
    '    the original panel.panel, whose lookup was overwritten before it was ever read.
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        dim as string sKey = gThemeBind(i).keyValue
        dim as long n = Theme_FindEntry( sKey )
        THASSERT( n >= 0, "bind key not present in theme: " & sKey )
        if n < 0 then continue for
        if gThemeBind(i).pFore <> 0 then
            THASSERT( gTheme(n).hasFore, "bound foreground missing: " & sKey )
        end if
        if gThemeBind(i).pBack <> 0 then
            THASSERT( gTheme(n).hasBack, "bound background missing: " & sKey )
        end if
    next

    ' -- 2. No bind row has an empty subsystem mask. -------------------------------------
    '    This IS the "change it and nothing updates" failure mode, in its only testable form.
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        THASSERT( gThemeBind(i).nSubsys <> THSUB_NONE, _
                  "zero subsystem mask: " & gThemeBind(i).keyValue )
    next

    ' -- 3. Every subsystem bit is fed by at least one key. ------------------------------
    dim as ulong nCovered
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        nCovered or= gThemeBind(i).nSubsys
    next
    THASSERT( nCovered = THSUB_ALL, "subsystem coverage &h" & hex(nCovered) & " <> THSUB_ALL" )

    ' -- 4. The parser reported nothing it could not act on. -----------------------------
    THASSERT( ubound(gThemeParseWarnings) < lbound(gThemeParseWarnings), _
              "parser produced warnings (mistyped key, or macro table overflow)" )

    ' -- 5. Colour formatting is the exact inverse of parsing. ---------------------------
    '    Asymmetric values only: a swapped red/blue round-trips perfectly on every grey, and
    '    a theme file is mostly greys.
    THASSERT( Theme_FormatColor( BGR(1, 2, 3) ) = "1,2,3", "colour round-trip 1,2,3" )
    THASSERT( Theme_FormatColor( BGR(255, 0, 0) ) = "255,0,0", "colour round-trip red" )
    THASSERT( Theme_FormatColor( BGR(0, 0, 255) ) = "0,0,255", "colour round-trip blue" )
    THASSERT( Theme_FormatColor( BGR(10, 20, 30), 128 ) = "10,20,30,128", "colour+alpha round-trip" )

    ' -- 6. Alpha is claimed exactly where a consumer exists. ----------------------------
    THASSERT( Theme_IsAlphaCapable( "editor.selection" ), "editor.selection should be alpha-capable" )
    THASSERT( Theme_IsAlphaCapable( "editor.occurrence" ), "editor.occurrence should be alpha-capable" )
    THASSERT( Theme_IsAlphaCapable( "editor.keyword" ) = false, "a Scintilla style must NOT be alpha-capable" )
    THASSERT( Theme_IsAlphaCapable( "panel.text" ) = false, "chrome is not alpha-capable yet" )

    ' -- 7. ui.* inherited from editor.* when the theme did not define it. ---------------
    dim as long nUiDiv = Theme_FindEntry( "ui.divider" )
    dim as long nEdDiv = Theme_FindEntry( "editor.divider" )
    THASSERT( (nUiDiv >= 0) andalso (nEdDiv >= 0), "ui.divider / editor.divider must both exist" )
    if (nUiDiv >= 0) andalso (nEdDiv >= 0) then
        THASSERT( gTheme(nUiDiv).foreground = gTheme(nEdDiv).foreground, _
                  "ui.divider did not inherit editor.divider" )
    end if

    ' -- 8. ...and an EXPLICIT ui.* value is never overwritten by the fallback. ----------
    '    The branch most likely to be skipped, and the one that makes this a fallback rather
    '    than an override.
    if nUiDiv >= 0 then
        dim as COLORREF clrSaved = gTheme(nUiDiv).foreground
        Theme_SetColor( "ui.divider", THEMECH_FORE, BGR(1, 2, 3) )
        Theme_ResolveFallbacks()
        THASSERT( gTheme(nUiDiv).foreground = BGR(1, 2, 3), _
                  "fallback clobbered an explicitly-set ui.divider" )
        gTheme(nUiDiv).foreground = clrSaved
    end if

    ' -- 9. Snapshot deep-copies rather than aliasing. -----------------------------------
    '    Tested BEHAVIOURALLY: mutate the live entry, then check the snapshot did not move.
    '    Comparing the two for equality would pass either way -- aliasing surfaces as a double
    '    free much later, never as a value mismatch. That is exactly the un-isolated-assertion
    '    shape CLAUDE.md records four times, so the extra step earns its keep.
    Theme_Snapshot()
    if (ubound(gTheme) >= lbound(gTheme)) andalso gThemeSnapValid then
        dim as long i0 = lbound(gTheme)
        dim as DWSTRING wszOrig = gTheme(i0).keyData
        dim as COLORREF clrOrig = gTheme(i0).foreground
        gTheme(i0).keyData = "MUTATED-BY-SELFTEST"
        gTheme(i0).foreground = BGR(9, 9, 9)
        THASSERT( gThemeSnapEntries(i0).keyData <> "MUTATED-BY-SELFTEST", _
                  "snapshot ALIASED the DWSTRING instead of deep-copying it" )
        THASSERT( gThemeSnapEntries(i0).foreground = clrOrig, "snapshot lost the colour" )
        Theme_Restore()
        THASSERT( gTheme(i0).keyData = wszOrig, "restore did not undo the string mutation" )
        THASSERT( gTheme(i0).foreground = clrOrig, "restore did not undo the colour mutation" )
    end if
    Theme_DiscardSnapshot()

    ' -- 10. A written theme parses back to the same values. -----------------------------
    dim as DWSTRING wszTmp = AfxGetExePathName & "_theme_selftest.tmp"
    dim as long nk = Theme_FindEntry( "editor.keyword" )
    dim as long np = Theme_FindEntry( "panel.text" )
    if (nk >= 0) andalso (np >= 0) then
        dim as COLORREF clrKeyword = gTheme(nk).foreground
        dim as COLORREF clrPanel   = gTheme(np).background
        Theme_Snapshot()
        THASSERT( Theme_WriteFile( wszTmp, "Self Test: colon and # hash" ) = false, "writer failed" )
        if Theme_ParseFile( wszTmp ) = false then
            dim as long nk2 = Theme_FindEntry( "editor.keyword" )
            dim as long np2 = Theme_FindEntry( "panel.text" )
            THASSERT( (nk2 >= 0) andalso (gTheme(nk2).foreground = clrKeyword), _
                      "editor.keyword foreground did not survive write/read" )
            THASSERT( (np2 >= 0) andalso (gTheme(np2).background = clrPanel), _
                      "panel.text background did not survive write/read" )
            dim as long nd = Theme_FindEntry( "general.description" )
            THASSERT( (nd >= 0) andalso (instr(gTheme(nd).keyData, "Self Test") > 0), _
                      "description did not survive sanitising and a round trip" )
        else
            THASSERT( false, "could not re-parse the file the writer just produced" )
        end if
        Theme_Restore()
        Theme_DiscardSnapshot()
        kill( wszTmp )
    end if

    ' -- 11. The two colour pages PARTITION the bind table. ------------------------------
    '    Every colour key on exactly one page, nothing listed twice, nothing orphaned. The
    '    pages are generated from this same predicate, so a key added to the engine cannot
    '    quietly fail to appear in the UI.
    dim as long nSyntax, nInterface, nNeither
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        select case frmThemes_PageForKey( gThemeBind(i).keyValue )
            case THEMEPAGE_SYNTAX:    nSyntax += 1
            case THEMEPAGE_INTERFACE: nInterface += 1
            case else:              nNeither += 1
        end select
    next
    THASSERT( nSyntax + nInterface + nNeither = (ubound(gThemeBind) - lbound(gThemeBind) + 1), _
              "page partition lost a key" )
    THASSERT( nSyntax > 0, "syntax page is empty" )
    THASSERT( nInterface > 0, "interface page is empty" )
    '    general.description / general.version have no bind row at all, so nothing should land
    '    in "neither" -- if it does, a colour key is invisible in both pages.
    THASSERT( nNeither = 0, "a bound colour key belongs to no page" )
    THASSERT( frmThemes_PageForKey( "editor.keyword" ) = THEMEPAGE_SYNTAX, "editor.* -> syntax" )
    THASSERT( frmThemes_PageForKey( "panel.text" ) = THEMEPAGE_INTERFACE, "panel.* -> interface" )
    THASSERT( frmThemes_PageForKey( "ui.divider" ) = THEMEPAGE_INTERFACE, "ui.* -> interface" )

    ' -- 12. Built-in themes are recognised as protected. --------------------------------
    THASSERT( frmThemes_IsProtectedTheme( "default_dark.theme" ), "default_dark is protected" )
    THASSERT( frmThemes_IsProtectedTheme( "default_light.theme" ), "default_light is protected" )
    THASSERT( frmThemes_IsProtectedTheme( "DEFAULT_Dark.theme" ), "protection is case-insensitive" )
    THASSERT( frmThemes_IsProtectedTheme( "mytheme.theme" ) = false, "a user theme is not protected" )
    THASSERT( frmThemes_IsProtectedTheme( "my_default_x.theme" ) = false, _
              "protection must match a PREFIX, not a substring" )

    ' -- 13. A fork name is new, and never collides with what is already on disk. ---------
    dim as DWSTRING wszFork = frmThemes_MakeForkName( "default_dark.theme" )
    THASSERT( wszFork <> "default_dark.theme", "fork name must differ from its source" )
    THASSERT( instr( wszFork, "_custom" ) > 0, "fork name should be recognisable" )
    THASSERT( AfxFileExists( AfxGetExePathName & "settings\themes\" & wszFork ) = 0, _
              "MakeForkName returned a filename that already exists" )
    THASSERT( frmThemes_IsProtectedTheme( wszFork ) = false, _
              "a fork of a built-in must NOT itself be protected, or it could never be saved" )

    ' -- 14. general.appearance survives a write and drives Theme_IsDark. ----------------
    '    The shipped dark theme must still read as dark once round-tripped through the
    '    writer, which derives the key from the editor background's luminance rather than
    '    from the filename.
    dim as DWSTRING wszTmp2 = AfxGetExePathName & "_theme_selftest2.tmp"
    Theme_Snapshot()
    if Theme_WriteFile( wszTmp2, "Appearance Probe" ) = false then
        if Theme_ParseFile( wszTmp2 ) = false then
            dim as long na = Theme_FindEntry( "general.appearance" )
            THASSERT( na >= 0, "writer did not emit general.appearance" )
            if na >= 0 then
                dim as string sApp = lcase(trim(gTheme(na).keyData))
                THASSERT( (sApp = "dark") orelse (sApp = "light"), _
                          "general.appearance must be dark or light, got: " & sApp )
                ' Force the filename sniff to disagree, so this proves the KEY is what is
                ' being read rather than the name it happens to sit beside.
                dim as DWSTRING wszSaveName = gThemeCurrentShortFilename
                gThemeCurrentShortFilename = "no_hint_here.theme"
                THASSERT( Theme_IsDark() = (sApp = "dark"), _
                          "Theme_IsDark ignored general.appearance" )
                gThemeCurrentShortFilename = wszSaveName
            end if
        else
            THASSERT( false, "could not re-parse the appearance probe" )
        end if
    else
        THASSERT( false, "could not write the appearance probe" )
    end if
    Theme_Restore()
    Theme_DiscardSnapshot()
    kill( wszTmp2 )

    ' -- 15. Every key is classified into a real category. -------------------------------
    '    The interface side derives its category from the namespace and cannot drift, but the
    '    syntax side is a written list, so this is the guard on it: an unclassified key falls
    '    into "Other" rather than vanishing, and this names it. Adding an editor.* key to the
    '    engine without listing it in CategoryForKey fails here.
    '    Compares category IDS, never their localized titles -- two categories that happen to
    '    translate alike are still two categories, and a missing string id would make every
    '    title compare equal (which is exactly how this went wrong the first time).
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        if frmThemes_PageForKey( gThemeBind(i).keyValue ) = OPTPAGE_NONE then continue for
        THASSERT( frmThemes_CategoryIdForKey( gThemeBind(i).keyValue ) <> THEMECAT_OTHER, _
                  "key has no category: " & gThemeBind(i).keyValue )
    next

    ' -- 16. Reset puts a channel back, and reports honestly whether it needs to. ---------
    dim as long nrk = Theme_FindEntry( "editor.keyword" )
    if nrk >= 0 then
        Theme_CaptureBaseline()
        dim as COLORREF clrWas = gTheme(nrk).foreground

        THASSERT( Theme_IsChannelModified( "editor.keyword", THEMECH_FORE ) = false, _
                  "nothing changed yet, so nothing should report as modified" )

        Theme_SetColor( "editor.keyword", THEMECH_FORE, BGR(7, 8, 9) )
        THASSERT( Theme_IsChannelModified( "editor.keyword", THEMECH_FORE ), _
                  "a changed channel must report as modified" )
        '    The OTHER channel must not be dragged along -- Reset is per-channel.
        THASSERT( Theme_IsChannelModified( "editor.keyword", THEMECH_BACK ) = false, _
                  "changing the foreground must not mark the background modified" )

        dim as ulong nrm = Theme_ResetChannel( "editor.keyword", THEMECH_FORE )
        THASSERT( nrm <> THSUB_NONE, "reset should report the subsystems it touched" )
        THASSERT( gTheme(nrk).foreground = clrWas, "reset did not restore the colour" )
        THASSERT( Theme_IsChannelModified( "editor.keyword", THEMECH_FORE ) = false, _
                  "after a reset the channel must report unmodified again" )
    end if

    ' -- 17. Every string id these pages use actually EXISTS in the language file. --------
    '    L(e,s) is `#Define L(e,s) LL(e)` -- the second argument is DOCUMENTATION and is
    '    DISCARDED. An id with no entry in english.lang renders as an empty string, so a
    '    caption, a heading or a whole message box silently comes up blank with nothing in the
    '    build or the assertions to say so. That is exactly how the group headers shipped
    '    captionless. english.lang is the fallback for every other language, so checking it
    '    covers them all.
    for nId as long = 297 to 323
        dim as boolean bHasText = false
        if (nId >= lbound(LL)) andalso (nId <= ubound(LL)) then bHasText = (len(LL(nId)) > 0)
        THASSERT( bHasText, "localized string " & str(nId) & " is missing from english.lang" )
    next

    ' -- 18. An ON toggle must read BRIGHTER than an OFF one. ----------------------------
    '    The failure this guards against is not a wrong value -- it is a control that shows
    '    the right value in a way people read backwards. tiko's toggle put a page-dark knob
    '    on a near-page-grey track for ON, and a bright knob on ON's darker sibling for OFF,
    '    so the OFF state was the visually louder one and a setting got read as its opposite.
    '    Compared by LUMINANCE rather than by naming a colour, so a light theme passes too.
    scope
        dim as PSTOGGLE_COLORS tc
        OptionsTheme_FillToggle( @tc )

        #macro LUMA( c )
            (( 299 * ((c) and &hFF) + 587 * (((c) shr 8) and &hFF) + 114 * (((c) shr 16) and &hFF) ) \ 1000)
        #endmacro

        ' Compare the BRIGHTEST element of each state, track or knob. Comparing tracks alone
        ' is what this assertion did first, and it PASSED against the very colours that were
        ' being misread -- the track was never the loud part, the knob was. An assertion that
        ' cannot fail on the reported bug is not an assertion.
        dim as long lumOnTrack  = LUMA( tc.TrackColorOn )
        dim as long lumOnKnob   = LUMA( tc.KnobColorOn )
        dim as long lumOffTrack = LUMA( tc.TrackColorOff )
        dim as long lumOffKnob  = LUMA( tc.KnobColorOff )

        dim as long lumOn  = iif( lumOnTrack  > lumOnKnob,  lumOnTrack,  lumOnKnob )
        dim as long lumOff = iif( lumOffTrack > lumOffKnob, lumOffTrack, lumOffKnob )

        THASSERT( lumOn > lumOff, _
                  "an OFF switch reads BRIGHTER than an ON one (on=" & str(lumOn) & _
                  " off=" & str(lumOff) & ") -- the control inverts at a glance" )
        THASSERT( tc.TrackColorOn = ghUI.AccentColor, _
                  "the ON track is not the theme accent" )
    end scope

    ' -- 19. The accent key exists and actually carries a colour. -------------------------
    THASSERT( Theme_FindEntry( "ui.accent" ) >= 0, "ui.accent was not materialised by the fallback" )
    THASSERT( ghUI.AccentColor <> ghPanel.BackColor, "the accent is the same as the page background" )

    ' -- 20. %MACRO survives a write/read round trip. ------------------------------------
    '    The writer used to expand every macro to literal r,g,b, so the first save of a
    '    hand-authored theme silently flattened its palette. The shipped themes define six,
    '    which is what makes this testable against real data rather than a fixture.
    THASSERT( ubound(gThemeMacros) >= lbound(gThemeMacros), _
              "the shipped theme defines macros but none were retained" )

    scope
        dim as long nMacrosBefore = ubound(gThemeMacros) - lbound(gThemeMacros) + 1
        dim as long nk = Theme_FindEntry( "editor.text" )
        dim as DWSTRING wszMacroBefore
        if nk >= 0 then wszMacroBefore = gTheme(nk).backMacro

        dim as DWSTRING wszTmp3 = AfxGetExePathName & "_theme_selftest3.tmp"
        Theme_Snapshot()
        if Theme_WriteFile( wszTmp3, "Macro Probe" ) = false then
            if Theme_ParseFile( wszTmp3 ) = false then
                THASSERT( (ubound(gThemeMacros) - lbound(gThemeMacros) + 1) = nMacrosBefore, _
                          "macro count changed across a write/read round trip" )
                dim as long nk2 = Theme_FindEntry( "editor.text" )
                THASSERT( (nk2 >= 0) andalso (gTheme(nk2).backMacro = wszMacroBefore), _
                          "editor.text lost its macro binding across a round trip" )
            else
                THASSERT( false, "could not re-parse the macro probe" )
            end if
        else
            THASSERT( false, "could not write the macro probe" )
        end if
        Theme_Restore()
        Theme_DiscardSnapshot()
        kill( wszTmp3 )
    end scope

    ' -- 21. Editing a key DETACHES only that key. ---------------------------------------
    '    The decided rule: no action at a distance. Editing one key that shares a macro with
    '    six others must not move the other six, and must not alter the macro itself.
    scope
        dim as long na = Theme_FindEntry( "editor.text" )
        dim as long nb2 = Theme_FindEntry( "editor.comments" )
        if (na >= 0) andalso (nb2 >= 0) then
            Theme_Snapshot()
            dim as DWSTRING wszOtherMacro = gTheme(nb2).backMacro
            dim as long nMacros = ubound(gThemeMacros) - lbound(gThemeMacros) + 1

            Theme_SetColor( "editor.text", THEMECH_BACK, BGR(3, 3, 3) )

            THASSERT( len(gTheme(na).backMacro) = 0, _
                      "the edited key is still attached to its macro" )
            THASSERT( gTheme(nb2).backMacro = wszOtherMacro, _
                      "editing one key detached a DIFFERENT key from its macro" )
            THASSERT( (ubound(gThemeMacros) - lbound(gThemeMacros) + 1) = nMacros, _
                      "editing a key changed the macro table" )
            Theme_Restore()
            Theme_DiscardSnapshot()
        end if
    end scope

    sLine = "--- theme self-test: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine

end sub
