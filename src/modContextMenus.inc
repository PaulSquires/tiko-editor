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
' EVERY STANDALONE POPUP MENU IN TIKO, ON THE CPopupMenu CONTROL
' (migrated off TrackPopupMenu 2026-07-21; the CreatePopupMenu/AppendMenu builders that
'  used to live in modMenus.inc are gone, as is every DestroyMenu that paired with them.)
'
' THE ONE THING TO UNDERSTAND HERE: TrackPopupMenu WAS MODAL, CPopupMenu IS NOT.
'   TrackPopupMenu ran its own message loop and did not return until the user picked or
'   cancelled, so a caller could write "show the menu, then act on the answer" in one
'   stack frame. CPopupMenu_Show returns the instant the window is up. Everything that
'   used to follow the TrackPopupMenu call now lives in that popup's select callback,
'   which the control fires AFTER the chain has closed.
'
' LIFETIME
'   The old builders created an HMENU per open and destroyed it after. A CPopupMenu is a
'   real window that must outlive the show (its callback fires later), so instead each
'   popup is created ONCE in ContextMenus_Init and its items are rebuilt per open with
'   Clear + AddItem -- the same pattern the menubar's MRU submenus use. Never destroy a
'   popup from inside its own callback.
'
' COLORS
'   All of them go through frmMenuBar_CreatePopup, so they are enrolled in
'   gMenuPopupList and pick up ghPopup.* colors and the GUI/symbol fonts from
'   frmMenuBar_ApplyPopupTheme -- identical to the topmenu dropdowns, and re-pushed
'   automatically by frmMenuBar_SyncTheme on a theme change.
'
' ROW HEIGHT: ALWAYS COMPACT
'   Every popup here is created with frmMenuBar_CreatePopup( TRUE ), which pins it to
'   MENUITEM_HEIGHT_COMPACT and ignores gConfig.CompactMenus. Deliberate: these are
'   short transient lists you point at and dismiss, not a menu tree you browse, so they
'   stay tight even for a user who wants roomy topmenu dropdowns. The flag is stored in
'   the registry, not applied once, so a later theme sync cannot undo it.
'
' CHECKMARKS, NOT RADIO DOTS
'   CheckMenuRadioItem/MFT_RADIOCHECK has no CPopupMenu equivalent: the control draws one
'   check glyph (ghPopup's, via wszIconCheckmark). The five menus that used radio items
'   (build config, encoding, tab size, file type, tabs list) now show that checkmark, and
'   the mutual exclusion is done here by only ever passing bChecked=true for one row.
' ========================================================================================

' The popups, created once. Names match the menus they replace.
dim shared as HWND ghPopupBuildConfig
dim shared as HWND ghPopupTabsInfoFileType
dim shared as HWND ghPopupFileEncoding
dim shared as HWND ghPopupLineEndings
dim shared as HWND ghPopupSpaces
dim shared as HWND ghPopupScintilla
dim shared as HWND ghPopupExplorer
dim shared as HWND ghPopupExplorerAction
dim shared as HWND ghPopupFunctionsAction
dim shared as HWND ghPopupBookmarksAction
dim shared as HWND ghPopupTopTabCtl
dim shared as HWND ghPopupTopTabsList

' The context chain currently (or last) shown. The pump filter needs a handle in the open
' chain, and CPopupMenu exposes no "which chain is open" query -- so the host tracks it.
' Never stale-dangerous: CPopupMenu_FilterMessage no-ops unless this really is the open
' chain, so it is only cleared for tidiness.
dim shared as HWND ghContextMenuOpen


' ========================================================================================
' Show a context popup at a screen point.
'
' bRightAlign is the TPM_RIGHTALIGN replacement: CPopupMenu_Show puts the popup's LEFT
' edge at x, and the control has no right-align mode, so the width is resolved here.
' GetIdealSize forces any pending layout, which is why it is valid to ask before showing.
' ========================================================================================
private function ContextMenu_ShowAt( _
            byval hPopup      as HWND, _
            byval x           as long, _
            byval y           as long, _
            byval bRightAlign as boolean = false _
            ) as boolean

    if hPopup = 0 then return false
    ' An empty menu is a no-op rather than a 0-row window: several of these are built
    ' from state (no open document, no build configs) that can legitimately yield none.
    if CPopupMenu_GetItemCount( hPopup ) = 0 then return false

    if bRightAlign then
        dim as long nWidth, nHeight
        CPopupMenu_GetIdealSize( hPopup, nWidth, nHeight )
        x -= nWidth
        if x < 0 then x = 0
    end if

    ghContextMenuOpen = hPopup
    return CPopupMenu_Show( hPopup, x, y )
end function


' Show at the cursor -- what every right-click menu wants.
private function ContextMenu_ShowAtCursor( byval hPopup as HWND ) as boolean
    dim as POINT pt: GetCursorPos( @pt )
    return ContextMenu_ShowAt( hPopup, pt.x, pt.y )
end function


' ========================================================================================
' The shared select callback: menus whose item ids ARE real WM_COMMAND ids just route
' through frmMain's normal command dispatcher, exactly as the menubar does
' (frmMenuBar_OnMenuCommand). Posted, not sent, so the handler never runs inside the
' control's own message handling.
'
' NOT usable by the build-config, tab-size or tabs-list menus -- see their own callbacks.
' ========================================================================================
private sub ContextMenu_PostCommand( byval hPopup as HWND, byval id as long )
    if id = 0 then exit sub
    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(id, 0), 0 )
end sub


' ========================================================================================
' Add the "Main file / Header file / Module file / Resource file / Normal file" block.
' Shared by the tab-info menu, the top-tab menu and the explorer menu, as the old
' AddProjectFileTypesToMenu was. The explorer variant uses a parallel set of ids
' (*_EXPLORERTREEVIEW) because its commands act on the whole selection, not the document.
' ========================================================================================
private sub AddProjectFileTypesToPopup( _
            byval hPopup            as HWND, _
            byval pDoc              as clsDocument ptr, _
            byval fSeparator        as boolean, _
            byval fExplorerTreeview as boolean _
            )

    if pDoc = 0 then exit sub
    if fSeparator then CPopupMenu_AddSeparator( hPopup )

    ' Which row carries the checkmark. Multiple explorer rows selected means there is no
    ' single current file type, so nothing is checked (the old code suppressed
    ' CheckMenuRadioItem for exactly this case).
    dim as long nFileType = -1
    dim as long nSelCount = CListBox_GetSelCount( HWND_FRMEXPLORER )
    dim as boolean bAllowCheckMark = true
    if fExplorerTreeview andalso (nSelCount > 1) then bAllowCheckMark = false

    if bAllowCheckMark then
        select case pDoc->ProjectFileType
            case FILETYPE_MAIN:     nFileType = iif( fExplorerTreeview, IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILEMAIN )
            case FILETYPE_RESOURCE: nFileType = iif( fExplorerTreeview, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, IDM_SETFILERESOURCE )
            case FILETYPE_HEADER:   nFileType = iif( fExplorerTreeview, IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEHEADER )
            case FILETYPE_MODULE:   nFileType = iif( fExplorerTreeview, IDM_SETFILEMODULE_EXPLORERTREEVIEW, IDM_SETFILEMODULE )
            case FILETYPE_NORMAL:   nFileType = iif( fExplorerTreeview, IDM_SETFILENORMAL_EXPLORERTREEVIEW, IDM_SETFILENORMAL )
        end select
    end if

    dim as long msg

    msg = iif( fExplorerTreeview, IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILEMAIN )
    CPopupMenu_AddItem( hPopup, msg, L(206,"Main file"), "", false, (msg = nFileType) )

    msg = iif( fExplorerTreeview, IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEHEADER )
    CPopupMenu_AddItem( hPopup, msg, L(181,"Header file"), "", false, (msg = nFileType) )

    if gApp.IsProjectActive then
        msg = iif( fExplorerTreeview, IDM_SETFILEMODULE_EXPLORERTREEVIEW, IDM_SETFILEMODULE )
        CPopupMenu_AddItem( hPopup, msg, L(205,"Module file"), "", false, (msg = nFileType) )
        msg = iif( fExplorerTreeview, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, IDM_SETFILERESOURCE )
        CPopupMenu_AddItem( hPopup, msg, L(209,"Resource file"), "", false, (msg = nFileType) )
    end if

    msg = iif( fExplorerTreeview, IDM_SETFILENORMAL_EXPLORERTREEVIEW, IDM_SETFILENORMAL )
    CPopupMenu_AddItem( hPopup, msg, L(204,"Normal file"), "", false, (msg = nFileType) )
end sub


' ========================================================================================
' Build configuration (statusbar panel)
'
' Its own callback, not ContextMenu_PostCommand: the item ids are IDM_BUILDCONFIG_POPUP+i
' and IDM_BUILDCONFIG_POPUP is ALSO the command that opens this menu -- so posting the
' first item's id as a WM_COMMAND would re-open the menu forever. TPM_RETURNCMD used to
' keep those ids out of the command stream entirely, which is why the collision never
' showed up before.
' ========================================================================================
private sub BuildConfigContextMenu_Rebuild()
    CPopupMenu_Clear( ghPopupBuildConfig )

    dim as DWSTRING wszCurrentBuild = frmBuildConfig_GetSelectedBuildDescription()

    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).ExcludeInPopup then continue for
        dim as long idMenu = IDM_BUILDCONFIG_POPUP + i
        dim as DWSTRING wszCaption = gConfig.Builds(i).wszDescription
        gConfig.Builds(i).idMenu = idMenu
        CPopupMenu_AddItem( ghPopupBuildConfig, idMenu, wszCaption, "", false, _
                            (wszCaption = wszCurrentBuild) )
    next
end sub

private sub BuildConfigContextMenu_OnSelect( byval hPopup as HWND, byval idMenu as long )
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).idMenu <> idMenu then continue for
        ' Set the active project or the active document to the selected build
        if gApp.IsProjectActive then
            gApp.ProjectBuild = gConfig.Builds(i).Id
        else
            dim pDocMain as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDocMain then pDocMain->DocumentBuild = gConfig.Builds(i).Id
            gConfig.CompilerBuild = gConfig.Builds(i).Id
        end if
        frmMain_SetStatusbar
        gConfig.SaveConfigFile
        exit for
    next
end sub

sub ShowBuildConfigContextMenu()
    BuildConfigContextMenu_Rebuild()
    ContextMenu_ShowAtCursor( ghPopupBuildConfig )
end sub


' ========================================================================================
' File type of the active document (top tabs info bar)
' ========================================================================================
sub ShowTabsInfoFileTypeContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub
    CPopupMenu_Clear( ghPopupTabsInfoFileType )
    AddProjectFileTypesToPopup( ghPopupTabsInfoFileType, pDoc, false, false )
    ContextMenu_ShowAtCursor( ghPopupTabsInfoFileType )
end sub


' ========================================================================================
' File encoding (statusbar panel). Right-aligned on the cursor, as TPM_RIGHTALIGN was.
' ========================================================================================
sub ShowFileEncodingContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    dim as long nEncoding
    select case pDoc->FileEncoding
        case FILE_ENCODING_ANSI:      nEncoding = IDM_ANSI
        case FILE_ENCODING_UTF8:      nEncoding = IDM_UTF8
        case FILE_ENCODING_UTF8_BOM:  nEncoding = IDM_UTF8BOM
        case FILE_ENCODING_UTF16_BOM: nEncoding = IDM_UTF16BOM
    end select

    CPopupMenu_Clear( ghPopupFileEncoding )
    CPopupMenu_AddItem( ghPopupFileEncoding, IDM_ANSI,     "ANSI",         "", false, (nEncoding = IDM_ANSI) )
    CPopupMenu_AddItem( ghPopupFileEncoding, IDM_UTF8,     "UTF-8",        "", false, (nEncoding = IDM_UTF8) )
    CPopupMenu_AddItem( ghPopupFileEncoding, IDM_UTF8BOM,  "UTF-8 (BOM)",  "", false, (nEncoding = IDM_UTF8BOM) )
    CPopupMenu_AddItem( ghPopupFileEncoding, IDM_UTF16BOM, "UTF-16 (BOM)", "", false, (nEncoding = IDM_UTF16BOM) )

    dim as POINT pt: GetCursorPos( @pt )
    ContextMenu_ShowAt( ghPopupFileEncoding, pt.x, pt.y, true )
end sub


' ========================================================================================
' Line endings (statusbar panel)
' ========================================================================================
sub ShowLineEndingsContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    CPopupMenu_Clear( ghPopupLineEndings )
    CPopupMenu_AddItem( ghPopupLineEndings, IDM_EOLTOCRLF, L(36,"Convert to CRLF") )
    CPopupMenu_AddItem( ghPopupLineEndings, IDM_EOLTOLF,   L(38,"Convert to LF") )

    dim as POINT pt: GetCursorPos( @pt )
    ContextMenu_ShowAt( ghPopupLineEndings, pt.x, pt.y, true )
end sub


' ========================================================================================
' Tab size (statusbar panel)
'
' Own callback for the same id-collision reason as the build config menu: the items are
' IDM_SPACES+0..8 and IDM_SPACES itself is the command that opens the menu, so "0" would
' re-open it if it went out as a WM_COMMAND.
' ========================================================================================
private sub SpacesContextMenu_OnSelect( byval hPopup as HWND, byval idMenu as long )
    if (idMenu < IDM_SPACES) orelse (idMenu > IDM_SPACES + 8) then exit sub

    gConfig.TabSize = idMenu - IDM_SPACES
    dim as DWSTRING wszText = "Spaces: " & gConfig.TabSize
    CStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, SPACES_PANEL, wszText )

    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        pDoc->ApplyProperties
        pDoc = pDoc->pDocNext
    loop
end sub

sub ShowSpacesContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    CPopupMenu_Clear( ghPopupSpaces )
    for i as long = 0 to 8
        dim as DWSTRING wszCaption = str(i)
        if i = val(gConfig.TabSize) then wszCaption = wszCaption & "  (Current Tab Size)"
        CPopupMenu_AddItem( ghPopupSpaces, IDM_SPACES + i, wszCaption, "", false, _
                            (i = gConfig.TabSize) )
    next

    ContextMenu_ShowAtCursor( ghPopupSpaces )
end sub


' ========================================================================================
' The code editor's own right-click menu
' ========================================================================================
sub ShowScintillaContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    ' Ensure that the window under the cursor is actually a Scintilla window
    ' and not another window like a tab in the top tabcontrol.
    dim as POINT pt: GetCursorPos( @pt )
    dim as POINT ptClient = pt
    ScreenToClient( HWND_FRMMAIN, @ptClient )
    if RealChildWindowFromPoint( HWND_FRMMAIN, ptClient ) <> hEdit then exit sub

    CPopupMenu_Clear( ghPopupScintilla )

    ' Determine if the text under the cursor is an #Include file. if yes, then add a
    ' menu option to allow to open it.
    if IsCurrentLineIncludeFilename() then
        CPopupMenu_AddItem( ghPopupScintilla, IDM_OPENINCLUDE, _
                L(75,"Open") + chr(32, 34) + gApp.IncludeFilename + chr(34) )
        CPopupMenu_AddSeparator( ghPopupScintilla )
    end if

    CPopupMenu_AddItem( ghPopupScintilla, IDM_UNDO, L(15,"Undo") )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_REDO, L(16,"Redo") )
    CPopupMenu_AddSeparator( ghPopupScintilla )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_CUT,   L(17,"Cut") )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_COPY,  L(18,"Copy") )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_PASTE, L(19,"Paste") )
    CPopupMenu_AddSeparator( ghPopupScintilla )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_DEBUG_TOGGLEBREAKPOINT, L(313,"Toggle Breakpoint") )
    if gApp.IsDebuggerActive then
        CPopupMenu_AddItem( ghPopupScintilla, IDM_DEBUG_RUNTOCURSOR, L(320,"Run to Cursor") )
    end if
    CPopupMenu_AddSeparator( ghPopupScintilla )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_INDENTBLOCK,   L(23,"Block Indent") )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_UNINDENTBLOCK, L(24,"Block Unindent") )
    CPopupMenu_AddSeparator( ghPopupScintilla )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_COMMENTBLOCK,   L(25,"Block Comment") )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_UNCOMMENTBLOCK, L(26,"Block Uncomment") )
    CPopupMenu_AddSeparator( ghPopupScintilla )
    CPopupMenu_AddItem( ghPopupScintilla, IDM_SELECTALL, L(40,"Select All") )

    ContextMenu_ShowAt( ghPopupScintilla, pt.x, pt.y )
end sub


' ========================================================================================
' Explorer listbox right-click
'
' The one site where losing TrackPopupMenu's modal loop really changes the code. The old
' version picked the command, then re-read the listbox selection, built an array of
' clsDocument ptrs, ran the action over it and reloaded the tree -- all after the menu
' closed but inside the same WM_RBUTTONDOWN handler. That whole tail is this callback
' now; the control fires it once the chain is down, so the ordering it depends on
' (selection read AFTER dismissal) is unchanged.
'
' Nothing is captured from the right-click: pDoc is needed only to BUILD the menu, and
' the actions run over whatever is selected at the time they run -- deliberately, because
' some of them reload the listbox and invalidate indexes.
' ========================================================================================
private sub ExplorerContextMenu_OnSelect( byval hPopup as HWND, byval id as long )
    dim as HWND hCtl = HWND_FRMEXPLORER

    ' Save the current state of selected rows prior to processing because as the items
    ' are processed some of the actions may reset the state of the listbox. We save the
    ' pDoc's in an array rather than index values because the listbox may get reloaded
    ' during some actions invalidating some indexes.
    dim as long nSelCount = CListBox_GetSelCount( hCtl )
    if nSelCount < 1 then exit sub

    dim pDocArray(nSelCount-1) as clsDocument ptr
    dim as integer nextIdx = 0
    for i as integer = 0 to CListBox_GetCount(hCtl) - 1
        if CListBox_IsHeader(hCtl, i) then continue for
        if CListBox_GetSel( hCtl, i ) = true then
            if nextIdx > ubound(pDocArray) then exit for
            dim as integer nItemData = CListBox_GetItemData( hCtl, i )
            pDocArray(nextIdx) = cast(clsDocument ptr, nItemData)
            nextIdx = nextIdx + 1
        end if
    next

    ' Process our array of selected pDoc's.
    for i as long = lbound(pDocArray) to ubound(pDocArray)
        if pDocArray(i) = null then continue for
        select case id
            case IDM_FILEOPEN_EXPLORERLISTBOX, IDM_FILECLOSE_EXPLORERLISTBOX, _
                IDM_FILESAVE_EXPLORERLISTBOX, IDM_FILESAVEAS_EXPLORERLISTBOX, _
                IDM_FILERENAME_EXPLORERLISTBOX, IDM_FILEDUPLICATE_EXPLORERLISTBOX
                OnCommand_FileExplorerMessage( id, pDocArray(i) )
            case IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX
                OnCommand_ProjectRemove( id, pDocArray(i) )
            case IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, _
                IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEMODULE_EXPLORERTREEVIEW, _
                IDM_SETFILENORMAL_EXPLORERTREEVIEW
                OnCommand_ProjectSetFileType( id, pDocArray(i) )
            case is > IDM_SETCATEGORY
                OnCommand_ProjectSetFileType( id, pDocArray(i) )
        end select
    next

    LoadExplorerFiles()
    frmExplorer_UnSelectListBox()
    frmMain_PositionWindows()
end sub

sub ShowExplorerContextMenu( byval pDoc as clsDocument ptr )
    ' if pDoc is zero then most likely we have right clicked on a Project or FILES root node
    if pDoc = 0 then exit sub

    ' Some of our available choices are affected by the fact that multiple selections
    ' in the explorer listbox may exist. In that case, we allow options like "Close"
    ' and also don't set a selected FileType.
    dim as long nSelCount = CListBox_GetSelCount( HWND_FRMEXPLORER )

    CPopupMenu_Clear( ghPopupExplorer )

    if gApp.IsProjectActive then
        CPopupMenu_AddItem( ghPopupExplorer, IDM_FILEOPEN_EXPLORERLISTBOX, L(75,"Open") )
        CPopupMenu_AddSeparator( ghPopupExplorer )
        if (gTTabCtl.GetTabIndexByDocumentPtr( pDoc ) <> -1) orelse (nSelCount > 1) then
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILECLOSE_EXPLORERLISTBOX, L(5,"Close") )
            CPopupMenu_AddSeparator( ghPopupExplorer )
        end if
        if nSelCount = 1 then
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILESAVE_EXPLORERLISTBOX,      L(7,"Save") )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILESAVEAS_EXPLORERLISTBOX,    L(8,"Save As") )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILERENAME_EXPLORERLISTBOX,    L(21,"Rename") )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILEDUPLICATE_EXPLORERLISTBOX, L(22,"Duplicate") )
            CPopupMenu_AddSeparator( ghPopupExplorer )
        end if
        CPopupMenu_AddItem( ghPopupExplorer, IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX, _
                            L(190,"Remove from project") )
    else
        CPopupMenu_AddItem( ghPopupExplorer, IDM_FILECLOSE_EXPLORERLISTBOX, L(5,"Close") )
        if nSelCount = 1 then
            CPopupMenu_AddSeparator( ghPopupExplorer )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILESAVE_EXPLORERLISTBOX,      L(7,"Save") )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILESAVEAS_EXPLORERLISTBOX,    L(8,"Save As") )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILERENAME_EXPLORERLISTBOX,    L(21,"Rename") )
            CPopupMenu_AddItem( ghPopupExplorer, IDM_FILEDUPLICATE_EXPLORERLISTBOX, L(22,"Duplicate") )
        end if
    end if

    AddProjectFileTypesToPopup( ghPopupExplorer, pDoc, true, true )

    ContextMenu_ShowAtCursor( ghPopupExplorer )
end sub


' ========================================================================================
' The side panel's "..." More Actions button. Three menus, one per pane; which one shows
' follows gPanel.hActiveChild. Right-aligned under the button.
' ========================================================================================
sub ShowPanelActionContextMenu( byval rcButton as RECT )
    dim as HWND hPopup

    select case gPanel.hActiveChild
        case HWND_FRMEXPLORER
            hPopup = ghPopupExplorerAction
            CPopupMenu_Clear( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_EXPLORER_EXPANDALL,   L(441,"Expand All") )
            CPopupMenu_AddItem( hPopup, IDM_EXPLORER_COLLAPSEALL, L(442,"Collapse All") )
            CPopupMenu_AddSeparator( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_CLOSEPANEL, L(161,"Close") )

        case HWND_FRMFUNCTIONS
            hPopup = ghPopupFunctionsAction
            CPopupMenu_Clear( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_EXPANDALL,   L(441,"Expand All") )
            CPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_COLLAPSEALL, L(442,"Collapse All") )
            CPopupMenu_AddSeparator( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_VIEWASTREE, L(445,"View as Tree") )
            CPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_VIEWASLIST, L(446,"View as List") )
            CPopupMenu_AddSeparator( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_CLOSEPANEL, L(161,"Close") )

        case HWND_FRMBOOKMARKS
            hPopup = ghPopupBookmarksAction
            CPopupMenu_Clear( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_BOOKMARKS_EXPANDALL,   L(441,"Expand All") )
            CPopupMenu_AddItem( hPopup, IDM_BOOKMARKS_COLLAPSEALL, L(442,"Collapse All") )
            CPopupMenu_AddSeparator( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_BOOKMARKCLEARALLDOCS, L(53,"Clear Bookmarks") )
            CPopupMenu_AddSeparator( hPopup )
            CPopupMenu_AddItem( hPopup, IDM_CLOSEPANEL, L(161,"Close") )

        case else
            exit sub
    end select

    ContextMenu_ShowAt( hPopup, rcButton.right, rcButton.bottom, true )
end sub


' ========================================================================================
' Top tab control right-click
' ========================================================================================
sub ShowTopTabCtlContextMenu( byval idx as long )
    dim as long CuriTab = gTTabCtl.GetCurSel()
    dim as long MaxiTab = gTTabCtl.GetItemCount - 1

    CPopupMenu_Clear( ghPopupTopTabCtl )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILECLOSE,     L(5,"Close") )
    CPopupMenu_AddSeparator( ghPopupTopTabCtl )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILESAVE,      L(7,"Save") )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILESAVEAS,    L(8,"Save As") )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILERENAME,    L(21,"Rename") )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILEDUPLICATE, L(22,"Duplicate") )
    CPopupMenu_AddSeparator( ghPopupTopTabCtl )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILECLOSEALL,       L(6,"Close All"),               "", (MaxiTab <= 0) )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILECLOSEALLOTHERS, L(354,"Close Others"),          "", (MaxiTab <= 0) )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_CLOSEALLBACKWARD,   L(355,"Close Tabs on the Left"), "", (CuriTab <= 0) )
    CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_CLOSEALLFORWARD,    L(356,"Close Tabs on the Right"), "", (MaxiTab <= CuriTab) )

    dim pDoc as clsDocument ptr = gTTabCtl.GetDocumentPtr(idx)
    if pDoc then
        if gApp.IsProjectActive then
            CPopupMenu_AddSeparator( ghPopupTopTabCtl )
            CPopupMenu_AddItem( ghPopupTopTabCtl, IDM_REMOVEFILEFROMPROJECT, L(190,"Remove from project") )
        end if
        AddProjectFileTypesToPopup( ghPopupTopTabCtl, pDoc, true, false )
    end if

    ContextMenu_ShowAtCursor( ghPopupTopTabCtl )
end sub


' ========================================================================================
' The open-tabs list (top tabs "list" button)
'
' Own callback: the item ids are tab INDEXES, not commands, so they must never reach
' frmMain's WM_COMMAND dispatcher. They stay 1-based -- id 0 is CPopupMenu's separator
' marker, so a zeroth tab could not be addressed by id.
' ========================================================================================
private sub TopTabsListContextMenu_OnSelect( byval hPopup as HWND, byval id as long )
    if (id > 0) andalso (id <= gTTabCtl.GetItemCount) then gTTabCtl.SetFocusTab( id - 1 )
end sub

sub ShowTopTabsListContextMenu( byval rcButton as RECT )
    CPopupMenu_Clear( ghPopupTopTabsList )
    dim as long nCurSel = gTTabCtl.GetCurSel()
    for idMenu as long = 0 to gTTabCtl.GetItemCount - 1
        CPopupMenu_AddItem( ghPopupTopTabsList, idMenu + 1, gTTabCtl.GetTabText(idMenu), _
                            "", false, (idMenu = nCurSel) )
    next
    ContextMenu_ShowAt( ghPopupTopTabsList, rcButton.right, rcButton.bottom, true )
end sub


' ========================================================================================
' Plumbing
' ========================================================================================

' Each CTextBox creates its own Cut/Copy/Paste/Select All popup. The control does not
' know tiko's theme, so hand its popup to the same registry the topmenu dropdowns use --
' after which a theme switch repaints it along with everything else.
sub ThemeTextBoxContextMenu( byval hTextBox as HWND )
    frmMenuBar_RegisterPopup( CTextBox_GetContextMenu( hTextBox ), true )
end sub


function ContextMenu_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if ghContextMenuOpen = 0 then return false
    if IsWindow( ghContextMenuOpen ) = 0 then
        ghContextMenuOpen = 0
        return false
    end if
    ' The control's filter is self-guarding: it stands down unless this handle really is
    ' in the currently open chain, so a stale handle here costs one comparison.
    return CPopupMenu_FilterMessage( ghContextMenuOpen, pMsg )
end function


sub ContextMenu_CloseAll()
    if ghContextMenuOpen = 0 then exit sub
    if IsWindow( ghContextMenuOpen ) then CPopupMenu_CloseChain( ghContextMenuOpen )
    ghContextMenuOpen = 0
end sub


' ========================================================================================
' Create every popup once, wired to its select callback. frmMenuBar_CreatePopup enrols
' each in gMenuPopupList and applies the current topmenu theme, so these track a theme
' or compact-menus change for free.
' ========================================================================================
sub ContextMenus_Init()

    ghPopupBuildConfig      = frmMenuBar_CreatePopup( true )
    ghPopupTabsInfoFileType = frmMenuBar_CreatePopup( true )
    ghPopupFileEncoding     = frmMenuBar_CreatePopup( true )
    ghPopupLineEndings      = frmMenuBar_CreatePopup( true )
    ghPopupSpaces           = frmMenuBar_CreatePopup( true )
    ghPopupScintilla        = frmMenuBar_CreatePopup( true )
    ghPopupExplorer         = frmMenuBar_CreatePopup( true )
    ghPopupExplorerAction   = frmMenuBar_CreatePopup( true )
    ghPopupFunctionsAction  = frmMenuBar_CreatePopup( true )
    ghPopupBookmarksAction  = frmMenuBar_CreatePopup( true )
    ghPopupTopTabCtl        = frmMenuBar_CreatePopup( true )
    ghPopupTopTabsList      = frmMenuBar_CreatePopup( true )

    ' Menus whose ids are real command ids: straight through the WM_COMMAND dispatcher.
    CPopupMenu_SetSelectCallback( ghPopupTabsInfoFileType, @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupFileEncoding,     @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupLineEndings,      @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupScintilla,        @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupExplorerAction,   @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupFunctionsAction,  @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupBookmarksAction,  @ContextMenu_PostCommand )
    CPopupMenu_SetSelectCallback( ghPopupTopTabCtl,        @ContextMenu_PostCommand )

    ' Menus whose ids are NOT safe as commands (see each callback's comment).
    CPopupMenu_SetSelectCallback( ghPopupBuildConfig,  @BuildConfigContextMenu_OnSelect )
    CPopupMenu_SetSelectCallback( ghPopupSpaces,       @SpacesContextMenu_OnSelect )
    CPopupMenu_SetSelectCallback( ghPopupTopTabsList,  @TopTabsListContextMenu_OnSelect )
    CPopupMenu_SetSelectCallback( ghPopupExplorer,     @ExplorerContextMenu_OnSelect )

end sub
