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

' ========================================================================================
' The application menubar, hosted on the reusable PsMenuBar + PsPopupMenu controls
' (vendored from the PsMenuBar repo; canonical home C:\dev\PsMenuBar). This module
' replaces the hand-rolled frmMenuBar/frmPopupMenu owner-draw listbox system
' (migrated 2026-07-21):
'   - gTopMenu() (modMenuDefinitions) stays the STATIC menu definition table; this
'     module builds the PsPopupMenu trees from it at startup.
'   - Dynamic sections (MRU files/sessions/projects, User Tools) are rebuilt in their
'     popup's InitPopup callback each time that submenu opens.
'   - Captions/accelerators are re-resolved through getMenuText() in the bar's
'     InitPopup callback each time a dropdown opens (localization, Start/Continue
'     Debugging, Explorer Left/Right stay current).
'   - Enable/checked state stays with EnableTopMenuItem/CheckMarkTopMenuItem
'     (modMenus.inc), which now drive the controls directly.
'   - Keyboard navigation and outside-click dismissal live in PsMenuBar_FilterMessage,
'     called once from frmMain's message pump (replacing handleKeysTopMenu and
'     handleMouseTopMenu).
'   - The "Update Available" label is a small host-owned sibling window sitting over
'     the right end of the menubar strip.
' ========================================================================================

declare sub frmMenuBar_DynamicInitPopup( byval hPopup as HWND )
declare sub frmMenuBar_ApplyPopupTheme( byval hPopup as HWND, byval bForceCompact as boolean = false )

' The three dynamic submenus (rebuilt per open in frmMenuBar_DynamicInitPopup).
dim shared as HWND ghMenuPopupMRU
dim shared as HWND ghMenuPopupMRUProject
dim shared as HWND ghMenuPopupUserTools

' Registry of every popup this module creates, so a theme change or a compact-menus
' toggle can re-push colors/fonts/heights into all of them (the controls COPY colors
' at Set time; they never read tiko's theme globals).
'
' The parallel flag array is the "always compact" opt-out of gConfig.CompactMenus. The
' CONTEXT menus (modContextMenus.inc, and the PsTextBox controls' own right-click menu)
' set it: they are short, transient lists rather than a browsable dropdown tree, so they
' use MENUITEM_HEIGHT_COMPACT whatever the user chose for the topmenu. It has to live in
' the registry rather than being applied once at create time, because
' frmMenuBar_SyncTheme re-pushes the height into every registered popup and would
' otherwise hand them the config value back on the next theme change.
dim shared gMenuPopupList(any) as HWND
dim shared gMenuPopupCompact(any) as boolean
dim shared as long gMenuPopupCount = 0

' The host-owned "Update Available" label window (sibling above the bar strip).
dim shared as HWND HWND_FRMMAIN_MENUBARUPDATE


' ========================================================================================
' Compatibility wrapper: every legacy dismissal site funnels through this name.
' Closes the menubar's chain AND any standalone context-menu chain -- the callers
' (WM_ACTIVATEAPP, a click on the main window, and every modal dialog about to open) mean
' "no menu should survive this", and since the TrackPopupMenu migration a context menu is
' a window that can outlive its caller just like a dropdown can.
' ========================================================================================
function killAllPopupMenus() as boolean
    PsMenuBar_CloseMenu( HWND_FRMMAIN_MENUBAR )
    ContextMenu_CloseAll()
    PsTextBox_CloseContextMenu()
    ' The code-completion popup is a fourth family of floating window with the same
    ' "must not survive this" contract.
    frmAutoComplete_Cancel()
    ' And the code tip -- a fifth. This one line covers the menu-opened, app-deactivated
    ' and modal-dialog-opening paths that all funnel through here.
    Codetip_Hide()
    return true
end function


' ========================================================================================
' Split a getMenuText() result into caption and accelerator (packed caption chr(9) accel).
' ========================================================================================
private sub frmMenuBar_SplitCaption( byval nID as long, byref wszCaption as DWSTRING, byref wszAccel as DWSTRING )
    dim as DWSTRING wszText = getMenuText( nID )
    wszCaption = AfxStrParse( wszText, 1, wchr(9) )
    wszAccel   = AfxStrParse( wszText, 2, wchr(9) )
end sub


' ========================================================================================
' Push the current theme colors/fonts/glyphs/item height into one popup.
' bForceCompact ignores gConfig.CompactMenus and always uses the compact row height --
' see the registry comment above for which popups pass it and why.
' ========================================================================================
private sub frmMenuBar_ApplyPopupTheme( byval hPopup as HWND, byval bForceCompact as boolean = false )
    dim c as PSPOPUPMENU_COLORS
    c.BackColor         = theme.topmenu.text.backcolor
    c.ForeColor         = theme.topmenu.text.forecolor
    c.BackColorHot      = theme.topmenu.texthot.backcolor
    c.ForeColorHot      = theme.topmenu.texthot.forecolor
    c.ForeColorDisabled = theme.topmenu.textdisabled.forecolor
    c.BorderColor       = theme.topmenu.panel
    ' Separators use the editor's divider color, not the muted text color the control
    ' drew them in before it grew this field -- it is the same rule tiko draws between
    ' every other pane, so menus now match the splitters and the panel dividers.
    c.SeparatorColor    = theme.editor.divider
    PsPopupMenu_SetColors( hPopup, @c )
    PsPopupMenu_SetFonts( hPopup, ghFont(GUIFONT_10), ghFont(SYMBOLFONT_10) )
    PsPopupMenu_SetGlyphs( hPopup, wszIconCheckmark, wszIconChevronRight )
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow then
        ' Two statements, not an orelse: gConfig.CompactMenus is a LONG, and mixing it
        ' with a boolean in a short-circuit operator is fbc warning 38.
        dim as boolean bCompact = bForceCompact
        if gConfig.CompactMenus then bCompact = true
        dim as long nHeight = iif( bCompact, MENUITEM_HEIGHT_COMPACT, MENUITEM_HEIGHT )
        PsPopupMenu_SetItemHeight( hPopup, pWindow->ScaleY(nHeight) )
    end if
end sub


' ========================================================================================
' Enrol an already-created popup for theme syncs and apply the current theme now. Used
' for popups this module does not own -- the PsTextBox controls each create their own
' right-click menu, and it has to track a theme change like every other menu.
'
' Dead slots are reused rather than appended to: the textbox popups die with their
' dialogs (frmSearchSymbol) and a host that opens one repeatedly would otherwise grow
' this list without bound.
' ========================================================================================
sub frmMenuBar_RegisterPopup( byval hPopup as HWND, byval bForceCompact as boolean = false )
    if hPopup = 0 then exit sub
    for i as long = 0 to gMenuPopupCount - 1
        if gMenuPopupList(i) = hPopup then
            gMenuPopupCompact(i) = bForceCompact
            frmMenuBar_ApplyPopupTheme( hPopup, bForceCompact )
            exit sub
        end if
    next
    for i as long = 0 to gMenuPopupCount - 1
        if IsWindow( gMenuPopupList(i) ) = 0 then
            gMenuPopupList(i) = hPopup
            gMenuPopupCompact(i) = bForceCompact
            frmMenuBar_ApplyPopupTheme( hPopup, bForceCompact )
            exit sub
        end if
    next
    redim preserve gMenuPopupList( gMenuPopupCount )
    redim preserve gMenuPopupCompact( gMenuPopupCount )
    gMenuPopupList(gMenuPopupCount) = hPopup
    gMenuPopupCompact(gMenuPopupCount) = bForceCompact
    gMenuPopupCount += 1
    frmMenuBar_ApplyPopupTheme( hPopup, bForceCompact )
end sub


' ========================================================================================
' Create one popup, register it for theme syncs, apply the current theme.
' bForceCompact is passed straight through -- the context menus create themselves this
' way (frmMenuBar's own dropdowns never do; they follow gConfig.CompactMenus).
' ========================================================================================
private function frmMenuBar_CreatePopup( byval bForceCompact as boolean = false ) as HWND
    dim as HWND hPopup = PsPopupMenu_Create( HWND_FRMMAIN )
    frmMenuBar_RegisterPopup( hPopup, bForceCompact )
    return hPopup
end function


' ========================================================================================
' Re-push theme colors into the bar and every popup. Called at startup and whenever
' the theme or the compact-menus setting may have changed (frmMenuBar_PositionWindows).
' ========================================================================================
sub frmMenuBar_SyncTheme()
    if HWND_FRMMAIN_MENUBAR = 0 then exit sub
    dim c as PSMENUBAR_COLORS
    c.BackColor    = theme.menubar.text.backcolor
    c.ForeColor    = theme.menubar.text.forecolor
    c.BackColorHot = theme.menubar.texthot.backcolor
    c.ForeColorHot = theme.menubar.texthot.forecolor
    PsMenuBar_SetColors( HWND_FRMMAIN_MENUBAR, @c )
    PsMenuBar_SetFont( HWND_FRMMAIN_MENUBAR, ghFont(GUIFONT_10) )
    for i as long = 0 to gMenuPopupCount - 1
        if IsWindow( gMenuPopupList(i) ) then _
            frmMenuBar_ApplyPopupTheme( gMenuPopupList(i), gMenuPopupCompact(i) )
    next
end sub


' ========================================================================================
' Rebuild the dynamic submenus from current config state. Each runs in its popup's
' InitPopup callback, so the list is always current when it opens (the old
' updateMRU*FilesItems/updateUserToolsMenuItems gTopMenu machinery is gone).
' ========================================================================================
private sub frmMenuBar_RebuildMRUFiles( byval hPopup as HWND )
    PsPopupMenu_Clear( hPopup )
    dim as boolean hasFiles = false
    for i as long = 0 to 9
        ' if the file no longer exists then remove it from the MRU list
        gConfig.MRU(i) = ProcessFromCurdriveApp( gConfig.MRU(i) )
        if AfxFileExists( gConfig.MRU(i) ) = 0 then
            gConfig.MRU(i) = ""
        else
            hasFiles = true
            PsPopupMenu_AddItem( hPopup, IDM_MRUBASE + i, gConfig.MRU(i) )
        end if
    next
    if hasFiles = false then
        PsPopupMenu_AddItem( hPopup, IDM_MRUFILES, L(11,"(Empty)") )
    else
        PsPopupMenu_AddSeparator( hPopup )
        PsPopupMenu_AddItem( hPopup, IDM_MRUCLEAR, L(185,"Clear this list") )
    end if
end sub

private sub frmMenuBar_RebuildMRUProjects( byval hPopup as HWND )
    PsPopupMenu_Clear( hPopup )
    dim as boolean hasFiles = false
    for i as long = 0 to 9
        gConfig.MRUPROJECT(i) = ProcessFromCurdriveApp( gConfig.MRUPROJECT(i) )
        if AfxFileExists( gConfig.MRUPROJECT(i) ) = 0 then
            gConfig.MRUPROJECT(i) = ""
        else
            hasFiles = true
            PsPopupMenu_AddItem( hPopup, IDM_MRUPROJECTBASE + i, gConfig.MRUPROJECT(i) )
        end if
    next
    if hasFiles = false then
        PsPopupMenu_AddItem( hPopup, IDM_MRUPROJECTFILES, L(11,"(Empty)") )
    else
        PsPopupMenu_AddSeparator( hPopup )
        PsPopupMenu_AddItem( hPopup, IDM_MRUPROJECTCLEAR, L(185,"Clear this list") )
    end if
end sub

private sub frmMenuBar_RebuildUserTools( byval hPopup as HWND )
    PsPopupMenu_Clear( hPopup )
    dim as DWSTRING wszCaption, wszAccel
    frmMenuBar_SplitCaption( IDM_USERTOOLSDIALOG, wszCaption, wszAccel )
    PsPopupMenu_AddItem( hPopup, IDM_USERTOOLSDIALOG, wszCaption, wszAccel )
    PsPopupMenu_AddSeparator( hPopup )
    dim as boolean hasTools = false
    for i as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
        if gConfig.Tools(i).IsDisplayMenu then
            hasTools = true
            PsPopupMenu_AddItem( hPopup, IDM_USERTOOLSBASE + i, _
                gConfig.Tools(i).wszDescription, createToolsMenuShortcut(i) )
        end if
    next
    if hasTools = false then
        PsPopupMenu_AddItem( hPopup, IDM_USERTOOLSLIST, L(11,"(Empty)") )
    end if
end sub

' One InitPopup callback serves all three dynamic submenus.
private sub frmMenuBar_DynamicInitPopup( byval hPopup as HWND )
    if hPopup = ghMenuPopupMRU then
        frmMenuBar_RebuildMRUFiles( hPopup )
    elseif hPopup = ghMenuPopupMRUProject then
        frmMenuBar_RebuildMRUProjects( hPopup )
    elseif hPopup = ghMenuPopupUserTools then
        frmMenuBar_RebuildUserTools( hPopup )
    end if
end sub


' ========================================================================================
' Build one dropdown tree from the static gTopMenu definition rows (rows whose
' nParentID = nParentID). A row with nChildID <> 0 owns a submenu: the dynamic ones
' (MRU / User Tools) are created empty with the rebuild callback; static ones
' (Preferences) recurse into gTopMenu keyed by the row's own nID.
' ========================================================================================
private function frmMenuBar_BuildDropDown( byval nParentID as long ) as HWND
    dim as HWND hPopup = frmMenuBar_CreatePopup()
    for i as long = lbound(gTopMenu) to ubound(gTopMenu)
        if gTopMenu(i).nParentID <> nParentID then continue for
        if gTopMenu(i).isSeparator then
            PsPopupMenu_AddSeparator( hPopup )
            continue for
        end if
        dim as DWSTRING wszCaption, wszAccel
        frmMenuBar_SplitCaption( gTopMenu(i).nID, wszCaption, wszAccel )
        PsPopupMenu_AddItem( hPopup, gTopMenu(i).nID, wszCaption, wszAccel, gTopMenu(i).isDisabled )
        if gTopMenu(i).nChildID <> 0 then
            dim as HWND hSub
            select case gTopMenu(i).nID
                case IDM_MRU
                    hSub = frmMenuBar_CreatePopup()
                    PsPopupMenu_SetMinWidth( hSub, AfxCWindowPtr(HWND_FRMMAIN)->ScaleX(262) )
                    PsPopupMenu_SetInitPopupCallback( hSub, @frmMenuBar_DynamicInitPopup )
                    ghMenuPopupMRU = hSub
                case IDM_MRUPROJECT
                    hSub = frmMenuBar_CreatePopup()
                    PsPopupMenu_SetMinWidth( hSub, AfxCWindowPtr(HWND_FRMMAIN)->ScaleX(262) )
                    PsPopupMenu_SetInitPopupCallback( hSub, @frmMenuBar_DynamicInitPopup )
                    ghMenuPopupMRUProject = hSub
                case IDM_USERTOOLS
                    hSub = frmMenuBar_CreatePopup()
                    PsPopupMenu_SetInitPopupCallback( hSub, @frmMenuBar_DynamicInitPopup )
                    ghMenuPopupUserTools = hSub
                case else
                    hSub = frmMenuBar_BuildDropDown( gTopMenu(i).nID )
            end select
            PsPopupMenu_SetItemSubMenu( hPopup, gTopMenu(i).nID, hSub )
        end if
    next
    return hPopup
end function


' ========================================================================================
' Refresh captions/accelerators of a dropdown's STATIC rows from getMenuText, so
' dynamic captions (Start/Continue Debugging, Move Explorer Left/Right), localization
' changes and re-mapped accelerator keys are current every time the menu opens. The
' dynamic submenus are skipped -- they fully rebuild in their own InitPopup.
' ========================================================================================
private sub frmMenuBar_RefreshCaptions( byval hDropDown as HWND, byval nParentID as long )
    for i as long = lbound(gTopMenu) to ubound(gTopMenu)
        if gTopMenu(i).nParentID <> nParentID then continue for
        if gTopMenu(i).isSeparator then continue for
        if gTopMenu(i).nID = 0 then continue for
        dim as DWSTRING wszCaption, wszAccel
        frmMenuBar_SplitCaption( gTopMenu(i).nID, wszCaption, wszAccel )
        PsPopupMenu_SetItemText( hDropDown, gTopMenu(i).nID, wszCaption, wszAccel )
        if gTopMenu(i).nChildID <> 0 then
            select case gTopMenu(i).nID
                case IDM_MRU, IDM_MRUPROJECT, IDM_USERTOOLS
                    ' dynamic: rebuilt when it opens
                case else
                    frmMenuBar_RefreshCaptions( hDropDown, gTopMenu(i).nID )
            end select
        end if
    next
end sub


' ========================================================================================
' PsMenuBar callbacks
' ========================================================================================

' A dropdown is about to open: refresh its captions and recompute its enable/checked
' states (frmMain_ChangeTopMenuStates dispatches to the per-menu state functions,
' whose EnableTopMenuItem/CheckMarkTopMenuItem calls now drive the controls).
private sub frmMenuBar_OnInitPopup( byval hMenuBar as HWND, byval idx as long, byval hPopup as HWND )
    dim as long nBarID = IDC_MENUBAR_FILE + idx
    frmMenuBar_RefreshCaptions( hPopup, nBarID )
    frmMain_ChangeTopMenuStates( nBarID )
end sub

' A menu command was executed (mouse or Enter). The popup chain is already closed and
' the bar highlight cleared; dispatch through the normal WM_COMMAND path.
private sub frmMenuBar_OnMenuCommand( byval hMenuBar as HWND, byval id as long )
    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(id, 0), 0 )
end sub


' ========================================================================================
' The "Update Available" label: a small host-owned window over the right end of the
' menubar strip (the bar control knows nothing about it). Shown only while
' gApp.IsUpdateAvailable resolves getMenuText(IDC_MENUBAR_UPDATE) to a caption.
' ========================================================================================
function frmMenuBarUpdate_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim as PsBufferPaint b
            b.BeginDoubleBuffer(hwnd)
            dim as RECT rc
            GetClientRect( hwnd, @rc )
            dim as boolean isHot = isMouseOverWindow( hwnd )
            if isHot then
                b.SetColors( theme.menubar.texthot.forecolor, theme.menubar.texthot.backcolor )
            else
                b.SetColors( theme.menubar.text.forecolor, theme.menubar.text.backcolor )
            end if
            b.PaintClientRect()
            b.SetFont( ghFont(GUIFONT_10) )
            b.PaintText( getMenuText(IDC_MENUBAR_UPDATE), @rc, DT_CENTER )
            b.EndDoubleBuffer()
            return 0

        case WM_MOUSEMOVE
            ' Arm leave tracking so the hot effect clears; repaint for the hot state.
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            AfxRedrawWindow( hwnd )

        case WM_MOUSELEAVE
            AfxRedrawWindow( hwnd )

        case WM_LBUTTONDOWN
            ShellExecute( NULL, "open", "https://github.com/PaulSquires/tiko/releases", Null, Null, SW_SHOWNORMAL )
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMMAIN_MENUBARUPDATE = 0
            return 0
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Size/position the menubar strip pieces. Called after startup, when the update check
' completes, and from frmOptions after settings save (theme / compact menus /
' localization may all have changed, so this also re-syncs theme colors and captions).
' ========================================================================================
function frmMenuBar_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function
    if HWND_FRMMAIN_MENUBAR = 0 then exit function

    frmMenuBar_SyncTheme()

    ' The completion popup draws in the EDITOR theme, not the menu theme, but this is
    ' the one place every settings-save path already funnels through.
    frmAutoComplete_SyncTheme()
    Codetip_SyncTheme()

    ' Bar captions may have changed (localization).
    for i as long = 0 to PsMenuBar_GetItemCount( HWND_FRMMAIN_MENUBAR ) - 1
        PsMenuBar_SetItemText( HWND_FRMMAIN_MENUBAR, i, getMenuText( IDC_MENUBAR_FILE + i ) )
    next

    ' The bar spans the full client width; its height follows the compact setting
    ' (frmMain_PositionWindows reads the resulting height to lay out everything below).
    dim as RECT rc
    GetClientRect( HWND_FRMMAIN, @rc )
    dim as long nHeight = pWindow->ScaleY( iif( gConfig.CompactMenus, MENUBAR_HEIGHT_COMPACT, MENUBAR_HEIGHT ) )
    SetWindowPos( HWND_FRMMAIN_MENUBAR, 0, 0, 0, rc.right - rc.left, nHeight, SWP_NOZORDER )

    ' The update label sits right-aligned over the bar strip, only while there is
    ' something to say.
    if IsWindow( HWND_FRMMAIN_MENUBARUPDATE ) then
        dim as DWSTRING wszText = getMenuText( IDC_MENUBAR_UPDATE )
        if len( wszText ) then
            dim as long nPadding = 12
            dim as long nWidth = pWindow->ScaleX( getTextWidth( HWND_FRMMAIN, wszText, GUIFONT_10 ) + nPadding )
            dim as long nLeft = (rc.right - rc.left) - nWidth - pWindow->ScaleX( nPadding )
            SetWindowPos( HWND_FRMMAIN_MENUBARUPDATE, HWND_TOP, nLeft, 0, nWidth, nHeight, SWP_SHOWWINDOW )
        else
            ShowWindow( HWND_FRMMAIN_MENUBARUPDATE, SW_HIDE )
        end if
    end if

    AfxRedrawWindow( HWND_FRMMAIN_MENUBAR )
    function = 0
end function


' ========================================================================================
' frmMenuBar_Show: create the bar, build every dropdown tree from gTopMenu, and
' create the update label.
' ========================================================================================
function frmMenuBar_Show( byval hWndParent as HWND ) as LRESULT

    buildTopMenuDefinitions()

    HWND_FRMMAIN_MENUBAR = PsMenuBar_Create( hWndParent, 0 )
    PsMenuBar_SetCommandCallback( HWND_FRMMAIN_MENUBAR, @frmMenuBar_OnMenuCommand )
    PsMenuBar_SetInitPopupCallback( HWND_FRMMAIN_MENUBAR, @frmMenuBar_OnInitPopup )

    ' One bar item + dropdown tree per menubar id (the ids are contiguous, and the
    ' bar item index = id - IDC_MENUBAR_FILE everywhere in the host).
    for id as long = IDC_MENUBAR_FILE to IDC_MENUBAR_HELP
        dim as long idx = PsMenuBar_AddItem( HWND_FRMMAIN_MENUBAR, getMenuText(id), id )
        PsMenuBar_SetDropDown( HWND_FRMMAIN_MENUBAR, idx, frmMenuBar_BuildDropDown(id) )
    next

    ' The update label (hidden until the update check resolves a caption).
    dim pWindow as CPanelWindow ptr = new CPanelWindow
    HWND_FRMMAIN_MENUBARUPDATE = pWindow->Create( hWndParent, @frmMenuBarUpdate_WndProc, 0, 0, 0, 0 )
    ShowWindow( HWND_FRMMAIN_MENUBARUPDATE, SW_HIDE )

    frmMenuBar_PositionWindows()

    function = 0
end function
