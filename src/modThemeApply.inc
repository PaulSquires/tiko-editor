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
    PsListTree_SetBackColor( hList, theme.panel.text.backcolor )
    PsListTree_SetScrollBarColors( hList, theme.panel.scrollbar.backcolor, _
                                  theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
end sub


''
''  RE-PUSH THE OUTPUT COLOURS INTO ONE PsListTree
''
private sub Theme_PushOutputList( byval hList as HWND )
    if IsWindow(hList) = 0 then exit sub
    PsListTree_SetBackColor( hList, theme.output.text.backcolor )
    PsListTree_SetScrollBarColors( hList, theme.output.scrollbar.backcolor, _
                                  theme.output.scrollbar.forecolor, theme.output.scrollbar.forecolor )
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
    PsTextBox_SetForeColor( hTextBox, theme.toptabs.texthot.forecolor )
    PsTextBox_SetBackColor( hTextBox, theme.toptabs.texthot.backcolor )
    PsTextBox_SetCueBannerColor( hTextBox, theme.toptabs.text.forecolor )
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
    c.BackColor         = theme.toptabs.texthot.backcolor
    c.ForeColor         = theme.toptabs.texthot.forecolor
    c.BackColorHot      = theme.toptabs.text.backcolor
    c.ForeColorHot      = theme.toptabs.texthot.forecolor
    c.BackColorSelect   = theme.toptabs.text.backcolor
    c.ForeColorSelect   = theme.toptabs.texthot.forecolor
    c.SeparatorColor    = theme.editor.divider
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
' Theme_FillTooltipColors lives in modThemeTips.inc now, beside its code-tip sibling
' and ahead of the four tip owners that could not see it from here.


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
    Theme_ApplyTooltip( PsButton_GetPsTooltipHandle( frmDebug_GetButton(IDC_FRMDEBUG_CMDRESTORE) ) )
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
            PsStatusBar_SetBackColor( HWND_FRMMAIN_STATUSBAR, theme.statusbar.text.backcolor )
    end if

    if (nMask and THSUB_TOPTABS) <> 0 then
        if IsWindow(HWND_FRMMAIN_TOPTABS) then
            PsTabBar_SetBackColor( HWND_FRMMAIN_TOPTABS, theme.toptabs.text.backcolor )
            PsTabBar_SetBorderColor( HWND_FRMMAIN_TOPTABS, theme.editor.divider )
        end if
        if IsWindow(HWND_FRMMAIN_TOPTABSMENU) then _
            PsStatusBar_SetBackColor( HWND_FRMMAIN_TOPTABSMENU, theme.toptabs.text.backcolor )
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
    if PsLen(wszPath) = 0 then return true
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
    dim as string sPath = (PsExePath & "theme_trace.log").Utf8
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
            '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
            dim as DWSTRING wszMsg = msg
            sLine = "  FAIL: " & wszMsg.Utf8
            ? sLine
        end if
    #endmacro

    ? "--- TIKO_THEME_SELFTEST ---"

    Theme_BuildKeyTable()

    ' Groups 1-3 READ THE GENERATED KEY TABLE, not the hand-written bind table, and that
    ' changed when the first gh* group was migrated. These three assert properties of THE
    ' THEME -- every key resolves, every key repaints something, every subsystem has a
    ' feeder. The bind table is no longer the theme: it is a bridge that shrinks by one
    ' group per commit until Phase 3 deletes it. Left pointed at it, group 3 would have gone
    ' red the moment main.panel migrated, because "main" is the only namespace feeding
    ' THSUB_FRAME -- an assertion failing to report that the app got WORSE, when nothing had.
    Theme_BuildKeyTable()

    ' -- 1. Every key resolves to a real entry, and to the channels it claims. ------------
    '    Catches a typo'd key or a key missing from a shipped theme. Would have caught the
    '    original panel.panel, whose lookup was overwritten before it was ever read.
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        dim as string sKey = gThemeKeys(i).keyValue
        dim as long n = Theme_FindEntry( sKey )
        THASSERT( n >= 0, "key not present in theme: " & sKey )
        if n < 0 then continue for
        if gThemeKeys(i).ofsFore >= 0 then
            THASSERT( gTheme(n).hasFore, "bound foreground missing: " & sKey )
        end if
        if gThemeKeys(i).ofsBack >= 0 then
            THASSERT( gTheme(n).hasBack, "bound background missing: " & sKey )
        end if
    next

    ' -- 2. No colour key has an empty subsystem mask. -----------------------------------
    '    This IS the "change it and nothing updates" failure mode, in its only testable form.
    '    general.* is exempt and always was: description and version are text, repaint
    '    nothing, and carry THSUB_NONE deliberately.
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        if PsLeft( gThemeKeys(i).keyValue, 8 ) = "general." then continue for
        THASSERT( gThemeKeys(i).nSubsys <> THSUB_NONE, _
                  "zero subsystem mask: " & gThemeKeys(i).keyValue )
    next

    ' -- 3. Every subsystem bit is fed by at least one key. ------------------------------
    dim as ulong nCovered
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        nCovered or= gThemeKeys(i).nSubsys
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
    dim as DWSTRING wszTmp = PsExePath & "_theme_selftest.tmp"
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
            THASSERT( (nd >= 0) andalso (PsInStr(gTheme(nd).keyData, "Self Test") > 0), _
                      "description did not survive sanitising and a round trip" )
        else
            THASSERT( false, "could not re-parse the file the writer just produced" )
        end if
        Theme_Restore()
        Theme_DiscardSnapshot()
        kill( *wszTmp.Wz() )
    end if

    ' -- 11. The two colour pages PARTITION the COLOUR keys. -----------------------------
    '    Every colour key on exactly one page, nothing listed twice, nothing orphaned. The
    '    pages are generated from this same predicate, so a key added to the engine cannot
    '    quietly fail to appear in the UI.
    '
    '    This walked the bind table until the migration emptied it, and its premise went with
    '    it: "general.* have no bind row, so nothing lands in neither" was true of that table
    '    and is false of the key table, which carries the two text payloads like any other
    '    key. Rather than exempt them by count, the check is now STRONGER -- a key is allowed
    '    off both pages only if it is a text key, which is asserted rather than assumed.
    dim as long nSyntax, nInterface, nNeither, nNeitherText
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        select case frmThemes_PageForKey( gThemeKeys(i).keyValue )
            case THEMEPAGE_SYNTAX:    nSyntax += 1
            case THEMEPAGE_INTERFACE: nInterface += 1
            case else
                nNeither += 1
                ' ofsText >= 0 IS the definition of a text payload rather than a colour.
                if gThemeKeys(i).ofsText >= 0 then
                    nNeitherText += 1
                else
                    THASSERT( false, "a COLOUR key belongs to no page: " & gThemeKeys(i).keyValue )
                end if
        end select
    next
    THASSERT( nSyntax + nInterface + nNeither = (ubound(gThemeKeys) - lbound(gThemeKeys) + 1), _
              "page partition lost a key" )
    THASSERT( nSyntax > 0, "syntax page is empty" )
    THASSERT( nInterface > 0, "interface page is empty" )
    '    ...and conversely, every text key IS off both pages. Without this the arm above
    '    would be satisfied by a theme with no text keys at all.
    THASSERT( nNeither = nNeitherText, "a colour key escaped both pages" )
    THASSERT( nNeitherText = 2, "expected exactly 2 text keys off-page, got " & nNeitherText )
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
    THASSERT( PsInStr( wszFork, "_custom" ) > 0, "fork name should be recognisable" )
    THASSERT( PsFileExists( PsExePath & "settings\themes\" & wszFork ) = 0, _
              "MakeForkName returned a filename that already exists" )
    THASSERT( frmThemes_IsProtectedTheme( wszFork ) = false, _
              "a fork of a built-in must NOT itself be protected, or it could never be saved" )

    ' -- 14. general.appearance survives a write and drives Theme_IsDark. ----------------
    '    The shipped dark theme must still read as dark once round-tripped through the
    '    writer, which derives the key from the editor background's luminance rather than
    '    from the filename.
    dim as DWSTRING wszTmp2 = PsExePath & "_theme_selftest2.tmp"
    Theme_Snapshot()
    if Theme_WriteFile( wszTmp2, "Appearance Probe" ) = false then
        if Theme_ParseFile( wszTmp2 ) = false then
            dim as long na = Theme_FindEntry( "general.appearance" )
            THASSERT( na >= 0, "writer did not emit general.appearance" )
            if na >= 0 then
                dim as string sApp = PsLCase(PsTrim(gTheme(na).keyData)).Utf8
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
    kill( *wszTmp2.Wz() )

    ' -- 15. Every key is classified into a real category. -------------------------------
    '    The interface side derives its category from the namespace and cannot drift, but the
    '    syntax side is a written list, so this is the guard on it: an unclassified key falls
    '    into "Other" rather than vanishing, and this names it. Adding an editor.* key to the
    '    engine without listing it in CategoryForKey fails here.
    '    Compares category IDS, never their localized titles -- two categories that happen to
    '    translate alike are still two categories, and a missing string id would make every
    '    title compare equal (which is exactly how this went wrong the first time).
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        if frmThemes_PageForKey( gThemeKeys(i).keyValue ) = OPTPAGE_NONE then continue for
        THASSERT( frmThemes_CategoryIdForKey( gThemeKeys(i).keyValue ) <> THEMECAT_OTHER, _
                  "key has no category: " & gThemeKeys(i).keyValue )
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
    for nId as long = 287 to 313
        dim as boolean bHasText = false
        if (nId >= lbound(LL)) andalso (nId <= ubound(LL)) then bHasText = (len(LL(nId)) > 0)
        THASSERT( bHasText, "localized string " & str(nId) & " is missing from english.lang" )
    next

    ' -- 18. The two toggle states must be TELLABLE APART. -------------------------------
    '    The failure this guards against is not a wrong value -- it is a control that shows
    '    the right value in a way people cannot read at a glance.
    '
    '    THIS MEASURED DIRECTION AND NOW MEASURES DISTANCE, and the reason is that the old
    '    form was dark-theme-only. It required ON to out-luminate OFF, which is true of a
    '    dark theme (accent track brighter than the page) and FALSE of a light one, where
    '    the accent is a tint darker than the page and ON is legitimately the darker state.
    '    Running the suite under default_light for the first time reported exactly that:
    '    "an OFF switch reads BRIGHTER than an ON one (on=226 off=243)" -- a correct control
    '    failing an assumption baked into the assertion.
    '
    '    ELEMENT-WISE, which is the other half of the fix. The old form reduced each state to
    '    max(track, knob) and compared those, which throws away the distinction that actually
    '    carries the state on a light theme: there ON is a pale track with a DARK knob and
    '    OFF is a near-white track with a light knob. Those are obvious side by side and
    '    indistinguishable under max(). Comparing track-to-track and knob-to-knob, and
    '    accepting either, is what makes the assertion true of both theme families.
    scope
        dim as PSTOGGLE_COLORS tc
        OptionsTheme_FillToggle( @tc )

        #macro LUMA( c )
            (( 299 * ((c) and &hFF) + 587 * (((c) shr 8) and &hFF) + 114 * (((c) shr 16) and &hFF) ) \ 1000)
        #endmacro

        dim as long lumOnTrack  = LUMA( tc.TrackColorOn )
        dim as long lumOnKnob   = LUMA( tc.KnobColorOn )
        dim as long lumOffTrack = LUMA( tc.TrackColorOff )
        dim as long lumOffKnob  = LUMA( tc.KnobColorOff )

        dim as long dTrack = abs( lumOnTrack - lumOffTrack )
        dim as long dKnob  = abs( lumOnKnob  - lumOffKnob )
        dim as long dBest  = iif( dTrack > dKnob, dTrack, dKnob )

        ' 40 of 255. Measured, not picked: it clears both shipped themes with room and
        ' still fails the original reported bug, where the two states differed by single
        ' digits on every element.
        const TOGGLE_MIN_DELTA = 40
        THASSERT( dBest >= TOGGLE_MIN_DELTA, _
                  "the two toggle states are not tellable apart (track delta=" & str(dTrack) & _
                  ", knob delta=" & str(dKnob) & ", need " & str(TOGGLE_MIN_DELTA) & _
                  ") -- ON and OFF read the same at a glance" )
        THASSERT( tc.TrackColorOn = theme.ui.accent, _
                  "the ON track is not the theme accent" )
    end scope

    ' -- 19. The accent key exists and actually carries a colour. -------------------------
    THASSERT( Theme_FindEntry( "ui.accent" ) >= 0, "ui.accent was not materialised by the fallback" )
    THASSERT( theme.ui.accent <> theme.panel.text.backcolor, "the accent is the same as the page background" )

    ' -- 20/21. RETIRED WITH THE FEATURE. -----------------------------------------------
    '    These asserted that a %MACRO survived a write/read round trip, and that editing one
    '    key detached only that key from its macro. Both were real and both earned their
    '    keep -- the writer used to flatten every macro into literals on the first save.
    '
    '    %MACRO is gone now. The ROLE layer replaced it and is strictly better at the job:
    '    a macro was pure text substitution with no meaning, so nothing could inherit from
    '    it and a new key still had to be written out in every file, while a role carries
    '    WHICH colour this is and every key falls back to one automatically. The shipped
    '    themes are macro-free and the twelve new ones never used them.
    '
    '    The PARSER still accepts %MACRO so an existing hand-authored theme keeps loading;
    '    the writer no longer emits it, so the first save migrates such a file to literals.
    '    That compatibility path is asserted in group 20b below rather than left to trust.

    ' -- 20b. AN OLD MACRO-BEARING THEME STILL LOADS. ------------------------------------
    scope
        dim as string sTmp = (PsExePath() & "_theme_macrocompat.tmp").Utf8
        dim as long f = freefile
        if open( sTmp for output encoding "utf-8" as #f ) = 0 then
            print #f, "general.description: legacy macros"
            print #f, "%OLDBLUE: 10,20,30"
            print #f, "editor.keyword.foreground: %OLDBLUE"
            close #f

            Theme_Snapshot()
            erase gTheme
            Theme_ParseFile( sTmp )
            dim as long n = Theme_FindEntry( "editor.keyword" )
            THASSERT( n >= 0, "a macro-bearing theme did not load" )
            if n >= 0 then
                THASSERT( gTheme(n).foreground = BGR(10,20,30), _
                          "the macro did not expand on load" )
            end if
            THASSERT( ubound(gThemeParseWarnings) < lbound(gThemeParseWarnings), _
                      "a macro-bearing theme produced parse warnings" )
            Theme_Restore()
            Theme_DiscardSnapshot()
            Theme_ResolveFallbacks()
            Theme_FillGlobals()
            kill( sTmp )
        end if
    end scope

    ' -- 22. THE GENERATED KEY TABLE AGREES WITH THE HAND-WRITTEN BIND TABLE. -------------
    '    The whole of Phase 1's claim, in its only testable form. The new "theme" root is
    '    filled from generated offsets; the gh* globals are filled from hand-written
    '    pointers; every read site in the app still reads gh*. So the root is only correct
    '    if it holds, key for key and channel for channel, exactly what the legacy table
    '    put in the globals.
    '
    '    These assertions are DELETED in Phase 3, when the gh* side they compare against
    '    stops existing. That is the stated reason, not a quiet drop.
    scope
        ' 22a. The table generated any rows at all.
        '      This is the "#include once" trap's only symptom: it is otherwise completely
        '      silent -- clean build, no warning, and an empty table means every colour in
        '      the app reads as black the moment the gh* globals are retired.
        dim as long nKeys = Theme_KeyCount()
        THASSERT( nKeys > 0, "the generated key table is EMPTY (#include once on modThemeKeys.bi?)" )

        ' 22b. The key table holds the COMPLETE key set -- 61 colour keys plus the two
        '      DWSTRING payloads the legacy bind table could not carry.
        '
        '      A CONSTANT, deliberately, and not "nBind + 2": the bind table shrinks by one
        '      group per migration commit while this number must not move at all. Written as
        '      a tripwire -- adding a key in Phase 4 is supposed to fail here until the
        '      number is updated on purpose.
        const THEME_TOTAL_KEYS = 71
        THASSERT( nKeys = THEME_TOTAL_KEYS, _
                  "key table has " & nKeys & " rows, expected " & THEME_TOTAL_KEYS )
        THASSERT( Theme_KeyIndex( "general.description" ) >= 0, _
                  "general.description is not in the key table" )
        THASSERT( Theme_KeyIndex( "general.version" ) >= 0, _
                  "general.version is not in the key table" )

        ' 22c. RETIRED. It compared every channel of every bind row against the member the
        '      generated offset addressed -- which was the whole of Phase 1's claim, and is
        '      now unassertable because the legacy side no longer exists. The bind table, the
        '      ten gh* globals and ThemeBind_Add are all deleted.
        '
        '      Deleted rather than left to pass: with an empty bind table the loop compared
        '      NOTHING and reported success for it, which is the exact failure mode the rest
        '      of this suite exists to catch.
        '
        '      What still covers the same ground: group 1 (every key resolves to a real entry
        '      and to the channels it claims), 22e (no two keys share a member), and the fact
        '      that every consumer in the app now reads the root directly, so a wrong offset
        '      is a wrong colour on screen rather than a silent disagreement between two
        '      tables.

        ' 22d. The two DWSTRING payloads reached the root -- the rows the legacy bind table
        '      could not hold at all, and which used to be hand-copied in Theme_FillGlobals.
        '
        '      Compared against the PARSED ENTRY, not against ghGeneral. That struct is gone,
        '      and the mechanical rename rewrote BOTH sides of the old comparison into
        '      "theme.general.description = theme.general.description" -- a tautology that
        '      would have passed forever, in a group whose entire job is catching exactly
        '      this. A rename can silently convert an assertion into one that cannot fail.
        scope
            dim as long nd = Theme_FindEntry( "general.description" )
            dim as long nv = Theme_FindEntry( "general.version" )
            THASSERT( nd >= 0, "general.description is not in the parsed theme" )
            THASSERT( nv >= 0, "general.version is not in the parsed theme" )
            if nd >= 0 then
                THASSERT( theme.general.description = gTheme(nd).keyData, _
                          "general.description did not reach the root" )
                ' Non-empty too: getThemePtr answers with a shared empty dummy on a miss, so
                ' "" = "" would satisfy the line above while nothing had actually loaded.
                THASSERT( PsLen( theme.general.description ) > 0, _
                          "general.description reached the root EMPTY" )
            end if
            if nv >= 0 then
                THASSERT( theme.general.version = gTheme(nv).keyData, _
                          "general.version did not reach the root" )
            end if
        end scope

        ' 22e. Every offset is inside the struct, and no two rows claim the SAME offset on
        '      the same channel -- which is what a duplicated member path in the table would
        '      look like, and it would silently make one key overwrite another.
        for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
            THASSERT( gThemeKeys(i).ofsFore < sizeof(THEMEROOT_TYPE), _
                      "foreground offset outside the struct: " & gThemeKeys(i).keyValue )
            THASSERT( gThemeKeys(i).ofsBack < sizeof(THEMEROOT_TYPE), _
                      "background offset outside the struct: " & gThemeKeys(i).keyValue )
            for j as long = i + 1 to ubound(gThemeKeys)
                if (gThemeKeys(i).ofsFore >= 0) andalso _
                   (gThemeKeys(i).ofsFore = gThemeKeys(j).ofsFore) then
                    THASSERT( false, "two keys share one foreground member: " & _
                              gThemeKeys(i).keyValue & " and " & gThemeKeys(j).keyValue )
                end if
                if (gThemeKeys(i).ofsBack >= 0) andalso _
                   (gThemeKeys(i).ofsBack = gThemeKeys(j).ofsBack) then
                    THASSERT( false, "two keys share one background member: " & _
                              gThemeKeys(i).keyValue & " and " & gThemeKeys(j).keyValue )
                end if
            next
        next

        ' 22f. Every key carries a real category. The theme editor derives its group headers
        '      from the table now, so an unclassified key would silently land in "Other".
        for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
            if PsLeft( gThemeKeys(i).keyValue, 8 ) = "general." then continue for
            THASSERT( gThemeKeys(i).nCat <> THEMECAT_OTHER, _
                      "key has no category: " & gThemeKeys(i).keyValue )
        next

        ' 22g. Filling a SECOND root gives the same answer -- which is the property that
        '      makes offsets worth having over pointers, since the snapshot, the baseline
        '      and the defaults are all just another destination.
        scope
            dim as THEMEROOT_TYPE other
            Theme_FillRoot( @other )
            THASSERT( other.editor.keyword1.forecolor = theme.editor.keyword1.forecolor, _
                      "filling a second root gave a different colour" )
            THASSERT( other.general.description = theme.general.description, _
                      "filling a second root gave a different description" )
            ' And a whole-struct assignment deep-copies it, including the DWSTRINGs.
            dim as THEMEROOT_TYPE copy
            copy = other
            other.general.description = "mutated"
            THASSERT( copy.general.description = theme.general.description, _
                      "assignment did not deep-copy the description" )
        end scope
    end scope

    ' -- 22h. A role.* LINE IN THE FILE IS PARSED AS A COLOUR. ---------------------------
    '    role.accent has only TWO dot segments, and two-segment keys are the TEXT path --
    '    it is the path general.description takes. So a role parsed perfectly happily,
    '    stored its digits as a string, set no colour, and silently fell through to the
    '    built-in default. A full 20-line palette was added to both shipped themes and
    '    changed absolutely nothing, with a clean build and every assertion green.
    '
    '    Asserted on the PARSE, not on the resolved value: a resolved role equal to its
    '    built-in default is legitimate, so comparing those cannot tell the two apart.
    scope
        dim as long n = Theme_FindEntry( "role.background" )
        THASSERT( n >= 0, "role.background is not in the parsed theme" )
        if n >= 0 then
            THASSERT( gTheme(n).hasFore, _
                      "role.background parsed as TEXT, not as a colour -- the palette is inert" )
            THASSERT( theme.role.clr(THROLE_BACKGROUND) = gTheme(n).foreground, _
                      "the resolved palette does not match the file" )
        end if
    end scope

    ' -- 22i. THE STROKE ACCENT OUT-CONTRASTS THE FILL ACCENT. ---------------------------
    '    role.accent is a FILL -- it resolves to toptabs.iconselected, a colour chosen to
    '    sit BEHIND an icon: dark navy in default_dark, a pale tint in default_light. Used
    '    as ink it is too dark to read on one theme and invisible on the other. That is the
    '    rule CLAUDE.md states, and it was broken once already (the Unused Symbols check
    '    glyph, reported as "a little too dark" on sight).
    '
    '    So role.accentfore exists, and the property that makes it worth having is that it
    '    reads against the page. Asserted as CONTRAST rather than raw brightness: on a light
    '    theme the readable accent is DARKER than the fill, so "brighter" would be exactly
    '    backwards and would pass on dark while failing on light.
    scope
        #macro LUMA( c )
            ( (  30 * clng( (c) and &hFF ) _
               + 59 * clng( ((c) shr 8) and &hFF ) _
               + 11 * clng( ((c) shr 16) and &hFF ) ) \ 100 )
        #endmacro
        dim as long lumBg   = LUMA( theme.role.clr(THROLE_BACKGROUND) )
        dim as long lumFill = LUMA( theme.role.clr(THROLE_ACCENT) )
        dim as long lumInk  = LUMA( theme.role.clr(THROLE_ACCENTFORE) )
        dim as long conFill = abs( lumFill - lumBg )
        dim as long conInk  = abs( lumInk  - lumBg )
        THASSERT( conInk > conFill, _
                  "role.accentfore reads no better than the fill accent (ink=" & conInk & _
                  " fill=" & conFill & ") -- it is ink, it has to be legible" )
        ' And it is genuinely a separate colour, not the fill copied into a second key.
        THASSERT( theme.role.clr(THROLE_ACCENTFORE) <> theme.role.clr(THROLE_ACCENT), _
                  "role.accentfore is the same colour as role.accent" )
        #undef LUMA
    end scope

    ' -- 22m. THE ROLE ACCESSORS ARE CORRECT AT EVERY INDEX. -----------------------------
    '    Theme_RoleColor(i) is what the Palette page paints in a row and mirrors into the
    '    R/G/B fields, so an off-by-one or a wrong lookup there is a page that shows one
    '    colour and edits another. Checked across the WHOLE table, not at index 0: the bug
    '    this guards against showed the right thing for some rows and 0,0,0 for others.
    '
    '    Also asserts the two index spaces are distinct sizes, which is the mistake itself --
    '    the page briefly fed a ROLE index to gThemeKeys() and read a different key entirely.
    scope
        dim as long nBad
        for i as long = 0 to Theme_RoleCount() - 1
            if Theme_RoleColor(i) <> theme.role.clr( gThemeRoleDef(i).nRole ) then nBad += 1
            if len( Theme_RoleName(i) ) = 0 then nBad += 1
        next
        THASSERT( nBad = 0, "role accessors disagree with the palette at " & nBad & " index(es)" )
        THASSERT( Theme_RoleCount() = THROLE_COUNT - 1, _
                  "the role table does not cover the enum (" & Theme_RoleCount() & _
                  " rows for " & (THROLE_COUNT - 1) & " roles)" )
        THASSERT( Theme_RoleCount() <> Theme_KeyCount(), _
                  "role and key index spaces are the same size -- a mix-up would not show" )
    end scope

    ' -- 22l. PAGE ID == SELECT-BAR PANEL INDEX. -----------------------------------------
    '    frmThemes_BarSelChange receives the panel INDEX from PsSelectBar and hands it to
    '    frmThemes_SelectPage as a page ID; PsSelectBar_SetCurSel takes an index too. So the
    '    dialog uses the two interchangeably and only ever worked because they matched.
    '
    '    Adding the Palette tab as id 2 while showing it FIRST broke that silently: the bar
    '    highlighted Interface (index 2) while the list filled with roles, so the tab and its
    '    contents disagreed and nothing failed. Reported by the author, invisible to every
    '    assertion here at the time.
    '
    '    The ids are the panel order now, and this says so. It cannot see the bar -- that
    '    only exists while the dialog is open -- but it pins the half a reorder would move.
    THASSERT( THEMEPAGE_PALETTE = 0, "THEMEPAGE_PALETTE is not the first panel" )
    THASSERT( THEMEPAGE_SYNTAX = 1, "THEMEPAGE_SYNTAX is not the second panel" )
    THASSERT( THEMEPAGE_INTERFACE = 2, "THEMEPAGE_INTERFACE is not the third panel" )
    THASSERT( THEMEPAGE_COUNT = 3, "THEMEPAGE_COUNT does not match the panel count" )

    ' -- 22k. THE PALETTE PAGE'S OWN STRINGS RESOLVE, AND TO THE RIGHT THING. ------------
    '    L(id, "default") DISCARDS the default -- an id with no entry renders EMPTY, and an
    '    id belonging to something else renders THAT. Both were live here: 327 and 328 were
    '    the first free-looking pair and are in fact the encoding-warning messages, so the
    '    Palette tab would have been captioned "This document contains characters...".
    '
    '    Checked by VALUE, not by len() > 0. A wrong-but-present id passes a non-empty test
    '    perfectly, which is exactly how the bug would have shipped.
    THASSERT( PsLen( LL(503) ) > 0, "671 (Palette) is empty in this language file" )
    THASSERT( PsLen( LL(504) ) > 0, "672 (default) is empty in this language file" )
    THASSERT( PsInStr( LL(503), "document" ) = 0, _
              "671 is not the Palette caption -- it reads: " & LL(503) )
    THASSERT( PsInStr( LL(504), "ANSI" ) = 0, _
              "672 is not the default marker -- it reads: " & LL(504) )
    THASSERT( PsLen( LL(505) ) > 0, "673 (Choose Color) is empty in this language file" )
    THASSERT( PsLen( LL(506) ) > 0, "674 (Use palette color) is empty in this language file" )

    ' -- 22n. A KEY CAN BE HANDED BACK TO ITS ROLE. --------------------------------------
    '    The half of the palette that was missing. Every edit DETACHES a key, so without a
    '    way back a theme decays into literals one key at a time and the palette stops
    '    reaching anything -- which is exactly what CLAUDE.md says the picker's old Assign
    '    button was for. Reported by the author as "no way to assign a palette colour".
    '
    '    Round trip: inherited -> overridden -> inherited, checking the FLAG each time, not
    '    just the colour. A key overridden with its role's own value would look identical by
    '    colour while being written to the file as a literal and no longer following the role.
    scope
        Theme_Snapshot()

        ' editor.edge, NOT a syntax key: the shipped themes name every syntax colour
        ' explicitly, so editor.keyword2 is authored and was never inheriting. editor.edge is
        ' one of the keys added with the role layer and appears in no theme file, so it is
        ' inheriting in every shipped theme -- which is what this needs to start from.
        dim as long n = Theme_FindEntry( "editor.edge" )
        if n >= 0 then
            THASSERT( gTheme(n).foreFromRole, "editor.edge is not inheriting to begin with" )

            ' Override it with the role's OWN colour -- the case a colour comparison cannot see.
            dim as COLORREF clrRole = theme.role.clr(THROLE_BORDER)
            Theme_SetColor( "editor.edge", THEMECH_FORE, clrRole )
            dim as long n2 = Theme_FindEntry( "editor.edge" )
            THASSERT( gTheme(n2).foreFromRole = false, "an edit did not detach the key" )
            THASSERT( Theme_ChannelIsOverridden( "editor.edge", THEMECH_FORE ), _
                      "an overridden channel does not report itself overridden" )

            ' ...and back.
            dim as ulong nMask = Theme_InheritChannel( "editor.edge", THEMECH_FORE )
            THASSERT( nMask <> THSUB_NONE, "inherit returned no subsystems to repaint" )
            dim as long n3 = Theme_FindEntry( "editor.edge" )
            THASSERT( gTheme(n3).foreFromRole, "the key did not go back to inheriting" )
            THASSERT( gTheme(n3).foreground = theme.role.clr(THROLE_BORDER), _
                      "the key did not take the role's colour" )
            THASSERT( Theme_ChannelIsOverridden( "editor.edge", THEMECH_FORE ) = false, _
                      "an inherited channel still reports itself overridden" )

            ' A channel with NO role has nothing to go back to and must be refused rather
            ' than cleared -- clearing would leave the key black.
            THASSERT( Theme_InheritChannel( "general.description", THEMECH_FORE ) = THSUB_NONE, _
                      "inherit accepted a key with no role" )
        end if

        Theme_Restore()
        Theme_DiscardSnapshot()
        Theme_ResolveFallbacks()
        Theme_FillGlobals()
    end scope

    ' -- 22j. EDITING A ROLE MOVES THE KEYS THAT INHERIT IT. -----------------------------
    '    The palette page's entire promise, and a silent no-op without Theme_ReapplyRoles:
    '    role resolution only fills a channel whose has* flag is FALSE, so once a key has
    '    taken its role's colour it looks exactly like an authored one and a later change to
    '    that role reaches nothing. The page would appear to work and change nothing.
    '
    '    Also asserts the other half -- that an AUTHORED colour is NOT dragged along. The
    '    provenance flags exist to tell those two apart and this is what checks they do.
    scope
        Theme_Snapshot()

        ' editor.keyword2 inherits role.apikeyword in every shipped theme; editor.keyword is
        ' authored in the two defaults, so pin it explicitly to be independent of the file.
        Theme_SetColor( "editor.keyword", THEMECH_FORE, BGR(7,7,7) )

        dim as long nInherit = Theme_FindEntry( "editor.keyword2" )
        dim as long nAuthored = Theme_FindEntry( "editor.keyword" )
        if (nInherit >= 0) andalso (nAuthored >= 0) then
            dim as boolean bWasInherited = gTheme(nInherit).foreFromRole

            ' find role.apikeyword by name rather than by index
            dim as long idxRole = -1
            for i as long = 0 to Theme_RoleCount() - 1
                if Theme_RoleName(i) = "role.apikeyword" then idxRole = i : exit for
            next
            THASSERT( idxRole >= 0, "role.apikeyword is not in the role table" )

            if (idxRole >= 0) andalso bWasInherited then
                Theme_SetRole( idxRole, BGR(3,5,7) )
                dim as long n2 = Theme_FindEntry( "editor.keyword2" )
                THASSERT( n2 >= 0, "editor.keyword2 vanished when its role was edited" )
                if n2 >= 0 then
                    THASSERT( gTheme(n2).foreground = BGR(3,5,7), _
                              "editing a role did not move the key that inherits it" )
                end if
                dim as long n3 = Theme_FindEntry( "editor.keyword" )
                if n3 >= 0 then
                    THASSERT( gTheme(n3).foreground = BGR(7,7,7), _
                              "editing a role overwrote an AUTHORED colour" )
                end if
                THASSERT( theme.role.clr(THROLE_APIKEYWORD) = BGR(3,5,7), _
                          "the palette did not take the new value" )
            end if
        end if

        Theme_Restore()
        Theme_DiscardSnapshot()
        Theme_ResolveFallbacks()
        Theme_FillGlobals()
    end scope

    ' -- 23. A THEME THAT DEFINES NOTHING IS STILL COMPLETE. -----------------------------
    '    This is the role layer's entire promise, in its only testable form: every colour
    '    key resolves, from the built-in defaults alone, with no file involved. It is what
    '    makes a roles-only theme legal and what removes the app's dependence on
    '    default_dark.theme existing on disk.
    '
    '    Destructive, so it runs inside a snapshot/restore pair and LAST -- everything above
    '    reads the real loaded theme.
    scope
        Theme_Snapshot()

        erase gTheme                        ' the emptiest possible theme
        Theme_ResolveFallbacks()            ' roles -> ui.* table -> key roles

        dim as long nMissing, nFilled
        dim as string sFirstMissing
        for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
            if gThemeKeys(i).ofsText >= 0 then continue for      ' text payloads, not colours
            dim as long n = Theme_FindEntry( gThemeKeys(i).keyValue )
            dim as boolean bWantF = (gThemeKeys(i).ofsFore >= 0)
            dim as boolean bWantB = (gThemeKeys(i).ofsBack >= 0)
            dim as boolean bGotF  = false
            dim as boolean bGotB  = false
            if n >= 0 then
                bGotF = gTheme(n).hasFore
                bGotB = gTheme(n).hasBack
            end if
            if (bWantF andalso (bGotF = false)) orelse (bWantB andalso (bGotB = false)) then
                nMissing += 1
                if len(sFirstMissing) = 0 then sFirstMissing = gThemeKeys(i).keyValue
            else
                nFilled += 1
            end if
        next
        THASSERT( nMissing = 0, _
                  "an empty theme left " & nMissing & " key(s) unresolved, first: " & sFirstMissing )
        THASSERT( nFilled > 50, "only " & nFilled & " keys resolved from defaults" )

        ' The palette itself came from the built-in defaults, not from zero. A role resolving
        ' to black would satisfy the completeness count above while rendering nothing.
        THASSERT( theme.role.clr(THROLE_BACKGROUND) <> 0, "role.background defaulted to black" )
        THASSERT( theme.role.clr(THROLE_FOREGROUND) <> theme.role.clr(THROLE_BACKGROUND), _
                  "foreground and background defaulted to the same colour" )
        THASSERT( theme.role.clr(THROLE_KEYWORD) <> theme.role.clr(THROLE_COMMENT), _
                  "keyword and comment defaulted to the same colour" )

        ' A role NAMED IN THE FILE must beat the built-in default -- the other half of the
        ' rule, and the half a defaults-only test cannot see.
        scope
            dim as long n = Theme_EnsureEntry( "role.accent" )
            gTheme(n).foreground = BGR( 1, 2, 3 )
            gTheme(n).hasFore    = true
            Theme_ResolveRoles()
            THASSERT( theme.role.clr(THROLE_ACCENT) = BGR(1,2,3), _
                      "a role named in the file did not beat the built-in default" )
        end scope

        ' 23b. A ROLES-ONLY THEME SURVIVES A SAVE. This is what the foreFromRole /
        '      backFromRole provenance flags buy, and it is the same failure the %MACRO
        '      round trip exists to prevent, one layer up: role resolution sets hasFore
        '      exactly as a parse does, so without provenance the writer cannot tell an
        '      authored colour from an inherited one and would flatten the whole palette
        '      into hard-coded literals on the first save.
        scope
            dim as string sTmp = (PsExePath() & "_theme_roletest.tmp").Utf8

            ' A theme with ONE authored key and everything else inherited.
            erase gTheme
            dim as long na = Theme_EnsureEntry( "editor.keyword" )
            gTheme(na).foreground = BGR( 9, 9, 9 )
            gTheme(na).hasFore    = true
            ' One authored ROLE too, so the round trip has a palette to lose.
            dim as long nr = Theme_EnsureEntry( "role.accent" )
            gTheme(nr).foreground = BGR( 4, 4, 4 )
            gTheme(nr).hasFore    = true
            Theme_ResolveFallbacks()

            THASSERT( Theme_WriteFile( sTmp, "roles only" ) = false, "roles-only write failed" )

            dim as long nLines
            dim as long f = freefile
            if open( sTmp for input encoding "utf-8" as #f ) = 0 then
                dim as string sLineIn
                while eof(f) = 0
                    line input #f, sLineIn
                    dim as boolean bReal = (len(trim(sLineIn)) > 0)
                    bReal = (bReal andalso (left(trim(sLineIn),1) <> "#"))
                    if bReal then nLines += 1
                wend
                close #f
            end if

            ' The authored key comes back...
            erase gTheme
            Theme_ParseFile( sTmp )
            dim as long nb = Theme_FindEntry( "editor.keyword" )
            THASSERT( nb >= 0, "the authored key did not survive the save" )
            if nb >= 0 then
                THASSERT( gTheme(nb).foreground = BGR(9,9,9), "the authored colour changed" )
            end if
            ' ...and the inherited ones did NOT get baked into the file. Without provenance
            ' this file carries a line for every colour key.
            THASSERT( nLines < 40, _
                      "a roles-only save emitted " & nLines & " lines -- the palette was flattened" )

            ' AND THE PALETTE ITSELF SURVIVED. This assertion was previously the line count
            ' alone, and it was happy with short-because-EMPTY: the writer emitted no role.*
            ' block at all, so a roles-only theme saved as a handful of lines and lost every
            ' colour it had. Found by forking a theme and reading the file, not by this test.
            ' A count is not a round trip.
            dim as long nRoles
            for i as long = 0 to Theme_RoleCount() - 1
                if Theme_RoleIsExplicit(i) then nRoles += 1
            next
            THASSERT( nRoles > 0, "the role palette did not survive the save" )
            THASSERT( theme.role.clr(THROLE_ACCENT) = BGR(4,4,4), _
                      "an authored role came back with a different colour" )
            kill( sTmp )
        end scope

        Theme_Restore()
        Theme_DiscardSnapshot()
        Theme_ResolveFallbacks()
        Theme_FillGlobals()
    end scope

    ' -- 24. EVERY SHIPPED THEME IS LEGIBLE. ---------------------------------------------
    '    Twelve themes were about to be authored by someone who cannot see them. This is
    '    what makes that responsible: it loads every .theme file on disk and puts each one
    '    through the same battery, so "the comment colour vanished into the page" fails a
    '    build instead of waiting to be noticed.
    '
    '    It asserts LEGIBILITY, not taste. Whether a palette is pleasant is the author's
    '    call and no assertion's business; whether its text can be read on its own
    '    background is arithmetic.
    '
    '    Destructive -- it parses other files over the live theme -- so it runs last, inside
    '    its own snapshot, and puts the real theme back afterwards.
    scope
        #macro LUM( c )
            (( 299 * ((c) and &hFF) + 587 * (((c) shr 8) and &hFF) + 114 * (((c) shr 16) and &hFF) ) \ 1000)
        #endmacro

        Theme_Snapshot()

        dim as DWSTRING wszFolder = frmThemes_ThemesFolder()
        dim as WIN32_FIND_DATAW WFD
        dim as HANDLE hSearch = FindFirstFile( (wszFolder & "*.theme").Wz(), @WFD )
        dim as long nThemes

        if hSearch <> INVALID_HANDLE_VALUE then
            do
                dim as DWSTRING wszName = WFD.cFileName
                if (wszName <> ".") andalso (wszName <> "..") then
                    nThemes += 1
                    dim as string sT = " [" & wszName.Utf8 & "]"

                    erase gTheme
                    dim as boolean bErr = Theme_ParseFile( wszFolder & wszName )
                    THASSERT( bErr = false, "theme failed to parse" & sT )
                    THASSERT( ubound(gThemeParseWarnings) < lbound(gThemeParseWarnings), _
                              "theme produced parse warnings" & sT )
                    Theme_ResolveFallbacks()
                    Theme_FillRoot( @theme )

                    ' Every colour key resolved -- the roles-only promise, per theme.
                    dim as long nUnres
                    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
                        if gThemeKeys(i).ofsText >= 0 then continue for
                        dim as long n = Theme_FindEntry( gThemeKeys(i).keyValue )
                        dim as boolean bF = (gThemeKeys(i).ofsFore >= 0)
                        dim as boolean bB = (gThemeKeys(i).ofsBack >= 0)
                        dim as boolean bOkF = true
                        dim as boolean bOkB = true
                        if n < 0 then
                            bOkF = (bF = false) : bOkB = (bB = false)
                        else
                            if bF then bOkF = gTheme(n).hasFore
                            if bB then bOkB = gTheme(n).hasBack
                        end if
                        if (bOkF = false) orelse (bOkB = false) then nUnres += 1
                    next
                    THASSERT( nUnres = 0, "unresolved colour keys: " & nUnres & sT )

                    THASSERT( PsLen(theme.general.description) > 0, "no description" & sT )

                    ' CODE MUST BE READABLE ON ITS OWN PAGE. The single most important
                    ' number in a theme, and the one a blind author gets wrong.
                    dim as long lumBg = LUM( theme.editor.text.backcolor )
                    dim as long dText = abs( LUM( theme.editor.text.forecolor ) - lumBg )
                    THASSERT( dText >= 90, "body text contrast is only " & dText & sT )

                    ' NO TOKEN CLASS DISAPPEARS INTO THE PAGE. A comment or a string the
                    ' same luminance as the background is invisible, and it is exactly the
                    ' mistake a hand-ported palette makes on one class out of eight.
                    #macro TOKCHK( style, nm )
                        scope
                            dim as long d = abs( LUM( theme.editor.style.forecolor ) - lumBg )
                            THASSERT( d >= 25, nm & " is invisible on the page (" & d & ")" & sT )
                        end scope
                    #endmacro
                    TOKCHK( keyword1,     "keyword" )
                    TOKCHK( keyword2,     "api keyword" )
                    TOKCHK( keyword3,     "user keyword" )
                    TOKCHK( comments,     "comment" )
                    TOKCHK( strings,      "string" )
                    TOKCHK( numbers,      "number" )
                    TOKCHK( operators,    "operator" )
                    TOKCHK( preprocessor, "preprocessor" )
                    #undef TOKCHK

                    ' A HOVERED ROW MUST LOOK HOVERED. role.hot fills the row under the
                    ' cursor and role.backgroundalt fills the ones that are not; if they
                    ' are the same colour the panels, menus, tabs and status bar all lose
                    ' their hover feedback at once, silently, in that theme only.
                    '
                    ' Both sepia themes shipped exactly that: Solarized names one colour
                    ' (base02 / base2) for both jobs and I took it for both roles. Caught by
                    ' a human hovering a row, which is precisely the kind of defect this
                    ' battery exists to find first -- and it did not, because every earlier
                    ' check compares INK against a background and this one compares two
                    ' backgrounds against each other.
                    '
                    ' Floor of 5, calibrated against the shipped themes rather than chosen:
                    ' the tightest legitimate one is studio_dark at 7, which is VS Code's
                    ' own list-hover step (#2A2D2E on #252526). This catches invisible, not
                    ' merely subtle.
                    scope
                        dim as long dHot = abs( LUM( theme.role.clr(THROLE_HOT) ) - _
                                                LUM( theme.role.clr(THROLE_BACKGROUNDALT) ) )
                        THASSERT( dHot >= 5, _
                                  "row hover is invisible -- role.hot is " & dHot & _
                                  " from role.backgroundalt" & sT )
                    end scope

                    ' A POPUP MUST BE DISTINGUISHABLE FROM THE PAGE -- BY FILL *OR* BY
                    ' BORDER. Menus, dropdowns and tooltips have to read as floating over
                    ' the window rather than as holes in it.
                    '
                    ' The first version of this check required a different FILL, and it was
                    ' wrong in the same way assertion 18 was: it encoded a dark-theme
                    ' assumption. It failed studio_light, contrast_light AND default_light
                    ' -- the last of which predates all of this work. A white popup on a
                    ' white page is not a defect, it is what VS Code Light+ ships; the
                    ' separation is carried by the border and the drop shadow. So the
                    ' requirement is that SOMETHING separates them, not that the fill does.
                    scope
                        dim as long lumRz  = LUM( theme.role.clr(THROLE_BACKGROUNDRAISED) )
                        dim as long dFill  = abs( lumRz - LUM( theme.role.clr(THROLE_BACKGROUND) ) )
                        dim as long dEdge  = abs( lumRz - LUM( theme.role.clr(THROLE_BORDER) ) )
                        dim as long dBest  = iif( dFill > dEdge, dFill, dEdge )
                        THASSERT( dBest >= 5, _
                                  "popups are indistinguishable from the page (fill " & dFill & _
                                  ", border " & dEdge & ")" & sT )
                    end scope

                    ' THE EDITOR/PANEL BOUNDARY MUST BE READABLE. Either the divider shows
                    ' against a surface, or the two surfaces differ from each other enough
                    ' that the boundary reads as a tonal step with no line needed.
                    '
                    ' Also softened from a first version that demanded the border be visible
                    ' against BOTH surfaces. That failed both sepia themes, where Solarized
                    ' uses base02 for the panel and for subtle rules alike -- the line
                    ' vanishes into the panel, but the panel and the editor are themselves
                    ' 10 apart, so the edge is perfectly legible. Requiring the line
                    ' specifically was requiring one implementation of an outcome.
                    scope
                        dim as long lumBd = LUM( theme.role.clr(THROLE_BORDER) )
                        dim as long lumBg = LUM( theme.role.clr(THROLE_BACKGROUND) )
                        dim as long lumAl = LUM( theme.role.clr(THROLE_BACKGROUNDALT) )
                        dim as long d1 = abs( lumBd - lumBg )
                        dim as long d2 = abs( lumBd - lumAl )
                        dim as long d3 = abs( lumBg - lumAl )
                        dim as long dMax = d1
                        if d2 > dMax then dMax = d2
                        if d3 > dMax then dMax = d3
                        THASSERT( dMax >= 5, _
                                  "the editor/panel boundary is invisible (border-vs-bg " & d1 & _
                                  ", border-vs-alt " & d2 & ", bg-vs-alt " & d3 & ")" & sT )
                    end scope

                    ' Selected text stays readable against its own highlight.
                    dim as long dSel = abs( LUM( theme.editor.selection.forecolor ) - _
                                            LUM( theme.editor.selection.backcolor ) )
                    THASSERT( dSel >= 40, "selected text contrast is only " & dSel & sT )

                    ' The stroke accent is legible (22i, per theme).
                    dim as long lumPg = LUM( theme.role.clr(THROLE_BACKGROUND) )
                    THASSERT( abs( LUM(theme.role.clr(THROLE_ACCENTFORE)) - lumPg ) > _
                              abs( LUM(theme.role.clr(THROLE_ACCENT)) - lumPg ), _
                              "role.accentfore reads no better than role.accent" & sT )

                    ' And the toggle states are tellable apart under THIS palette.
                    scope
                        dim as PSTOGGLE_COLORS tc
                        OptionsTheme_FillToggle( @tc )
                        dim as long dTr = abs( LUM(tc.TrackColorOn) - LUM(tc.TrackColorOff) )
                        dim as long dKn = abs( LUM(tc.KnobColorOn)  - LUM(tc.KnobColorOff) )
                        THASSERT( iif(dTr > dKn, dTr, dKn) >= 40, _
                                  "toggle states not tellable apart (track " & dTr & _
                                  ", knob " & dKn & ")" & sT )
                    end scope
                end if
            loop while FindNextFile( hSearch, @WFD )
            FindClose( hSearch )
        end if

        THASSERT( nThemes >= 2, "found only " & nThemes & " theme files" )
        sLine = "    (battery ran over " & nThemes & " theme files)"
        ? sLine

        Theme_Restore()
        Theme_DiscardSnapshot()
        Theme_ResolveFallbacks()
        Theme_FillGlobals()
        #undef LUM
    end scope

    sLine = "--- theme self-test: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine

end sub
