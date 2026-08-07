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
' EVERY STANDALONE POPUP MENU IN TIKO, ON THE PsPopupMenu CONTROL
' (migrated off TrackPopupMenu 2026-07-21; the CreatePopupMenu/AppendMenu builders that
'  used to live in modMenus.inc are gone, as is every DestroyMenu that paired with them.)
'
' THE ONE THING TO UNDERSTAND HERE: TrackPopupMenu WAS MODAL, PsPopupMenu IS NOT.
'   TrackPopupMenu ran its own message loop and did not return until the user picked or
'   cancelled, so a caller could write "show the menu, then act on the answer" in one
'   stack frame. PsPopupMenu_Show returns the instant the window is up. Everything that
'   used to follow the TrackPopupMenu call now lives in that popup's select callback,
'   which the control fires AFTER the chain has closed.
'
' LIFETIME
'   The old builders created an HMENU per open and destroyed it after. A PsPopupMenu is a
'   real window that must outlive the show (its callback fires later), so instead each
'   popup is created ONCE in ContextMenus_Init and its items are rebuilt per open with
'   Clear + AddItem -- the same pattern the menubar's MRU submenus use. Never destroy a
'   popup from inside its own callback.
'
' COLORS
'   All of them go through frmMenuBar_CreatePopup, so they are enrolled in
'   gMenuPopupList and pick up theme.topmenu.* colors and the GUI/symbol fonts from
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
'   CheckMenuRadioItem/MFT_RADIOCHECK has no PsPopupMenu equivalent: the control draws one
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
dim shared as HWND ghPopupExplorerFolder
dim shared as HWND ghPopupExplorerAction
dim shared as HWND ghPopupFunctionsAction
dim shared as HWND ghPopupBookmarksAction
dim shared as HWND ghPopupTopTabCtl
dim shared as HWND ghPopupTopTabsList
dim shared as HWND ghPopupEditorSplit
dim shared as HWND ghPopupOutputResults
dim shared as HWND ghPopupOutputDock

' The context chain currently (or last) shown. The pump filter needs a handle in the open
' chain, and PsPopupMenu exposes no "which chain is open" query -- so the host tracks it.
' Never stale-dangerous: PsPopupMenu_FilterMessage no-ops unless this really is the open
' chain, so it is only cleared for tidiness.
dim shared as HWND ghContextMenuOpen


' ========================================================================================
' Show a context popup at a screen point.
'
' bRightAlign is the TPM_RIGHTALIGN replacement: PsPopupMenu_Show puts the popup's LEFT
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
    if PsPopupMenu_GetItemCount( hPopup ) = 0 then return false

    if bRightAlign then
        dim as long nWidth, nHeight
        PsPopupMenu_GetIdealSize( hPopup, nWidth, nHeight )
        x -= nWidth
        if x < 0 then x = 0
    end if

    ghContextMenuOpen = hPopup
    return PsPopupMenu_Show( hPopup, x, y )
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
    if fSeparator then PsPopupMenu_AddSeparator( hPopup )

    ' Which row carries the checkmark. Multiple explorer rows selected means there is no
    ' single current file type, so nothing is checked (the old code suppressed
    ' CheckMenuRadioItem for exactly this case).
    dim as long nFileType = -1
    dim as long nSelCount = PsListTree_GetSelCount( HWND_FRMEXPLORER )
    dim as boolean bAllowCheckMark = true
    if fExplorerTreeview andalso (nSelCount > 1) then bAllowCheckMark = false

    if bAllowCheckMark then
        ' .Utf8 on the selector -- `select case <DWSTRING>` binds today only because
        ' AfxNova's DWSTRING extends WSTRING. Every `case` label below is ASCII, so
        ' the ANSI-vs-UTF-8 difference between the two conversions cannot change a
        ' match. See clsConfig.inc for the long form.
        select case pDoc->ProjectFileType.Utf8
            case FILETYPE_MAIN:     nFileType = iif( fExplorerTreeview, IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILEMAIN )
            case FILETYPE_RESOURCE: nFileType = iif( fExplorerTreeview, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, IDM_SETFILERESOURCE )
            case FILETYPE_HEADER:   nFileType = iif( fExplorerTreeview, IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEHEADER )
            case FILETYPE_MODULE:   nFileType = iif( fExplorerTreeview, IDM_SETFILEMODULE_EXPLORERTREEVIEW, IDM_SETFILEMODULE )
            case FILETYPE_NORMAL:   nFileType = iif( fExplorerTreeview, IDM_SETFILENORMAL_EXPLORERTREEVIEW, IDM_SETFILENORMAL )
        end select
    end if

    dim as long msg

    msg = iif( fExplorerTreeview, IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILEMAIN )
    PsPopupMenu_AddItem( hPopup, msg, L(167,"Main file"), "", false, (msg = nFileType) )

    msg = iif( fExplorerTreeview, IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEHEADER )
    PsPopupMenu_AddItem( hPopup, msg, L(145,"Header file"), "", false, (msg = nFileType) )

    ' Every category is offered in every workspace. Module and Resource used to be
    ' project-only; with one workspace model there is no second kind of workspace to
    ' withhold them from, and the Explorer shows their headers unconditionally.
    msg = iif( fExplorerTreeview, IDM_SETFILEMODULE_EXPLORERTREEVIEW, IDM_SETFILEMODULE )
    PsPopupMenu_AddItem( hPopup, msg, L(166,"Module file"), "", false, (msg = nFileType) )
    msg = iif( fExplorerTreeview, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, IDM_SETFILERESOURCE )
    PsPopupMenu_AddItem( hPopup, msg, L(170,"Resource file"), "", false, (msg = nFileType) )

    msg = iif( fExplorerTreeview, IDM_SETFILENORMAL_EXPLORERTREEVIEW, IDM_SETFILENORMAL )
    PsPopupMenu_AddItem( hPopup, msg, L(165,"Normal file"), "", false, (msg = nFileType) )
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
    PsPopupMenu_Clear( ghPopupBuildConfig )

    dim as DWSTRING wszCurrentBuild = frmBuildConfig_GetSelectedBuildDescription()

    ' Opens the Build Configurations dialog. Handled explicitly in the select callback --
    ' this id is NOT in the IDM_BUILDCONFIG_POPUP+i range the loop below uses.
    PsPopupMenu_AddItem( ghPopupBuildConfig, IDM_BUILDCONFIG, _
                        L(215,"Build Configurations") & "...", "", false, false )

    dim as boolean fSeparatorAdded = false

    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).ExcludeInPopup then continue for
        if fSeparatorAdded = false then
            PsPopupMenu_AddSeparator( ghPopupBuildConfig )
            fSeparatorAdded = true
        end if
        dim as long idMenu = IDM_BUILDCONFIG_POPUP + i
        dim as DWSTRING wszCaption = gConfig.Builds(i).wszDescription
        gConfig.Builds(i).idMenu = idMenu
        ' The shortcut slot was hardcoded "" here for as long as this menu has existed, so a
        ' build accelerator fired but was never displayed ANYWHERE in the editor. Composed by
        ' frmBuildConfig_MenuShortcutText, which is the same composer the dialog uses -- so
        ' the two cannot come to disagree, and in particular cannot disagree about modifier
        ' ORDER (Ctrl+Shift+Alt here, matching the Tools menu; keybindings.ini stores
        ' Ctrl+Alt+Shift, and that difference is deliberate).
        PsPopupMenu_AddItem( ghPopupBuildConfig, idMenu, wszCaption, _
                            frmBuildConfig_MenuShortcutText( i ), false, _
                            (wszCaption = wszCurrentBuild) )
    next
end sub

private sub BuildConfigContextMenu_OnSelect( byval hPopup as HWND, byval idMenu as long )
    ' Posted, not called, so the dialog never opens inside the popup's own message handling.
    if idMenu = IDM_BUILDCONFIG then
        PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_BUILDCONFIG, 0), 0 )
        exit sub
    end if
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).idMenu <> idMenu then continue for
        ' Set the active project or the active document to the selected build
        ' The build belongs to the workspace. gConfig.CompilerBuild is the seed for a NEW
        ' workspace only. (clsDocument.DocumentBuild was write-only even before this and
        ' has been deleted.)
        gApp.ProjectBuild = gConfig.Builds(i).Id
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
    PsPopupMenu_Clear( ghPopupTabsInfoFileType )
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

    ' Captions come from Doc_EncodingName -- see the note there. The command ids are NOT
    ' the FILE_ENCODING_* values (they are IDM_* entries in the app's command space), so
    ' the pairing stays explicit here rather than being derived from a loop counter.
    PsPopupMenu_Clear( ghPopupFileEncoding )
    PsPopupMenu_AddItem( ghPopupFileEncoding, IDM_ANSI, _
                         Doc_EncodingName(FILE_ENCODING_ANSI),      "", false, (nEncoding = IDM_ANSI) )
    PsPopupMenu_AddItem( ghPopupFileEncoding, IDM_UTF8, _
                         Doc_EncodingName(FILE_ENCODING_UTF8),      "", false, (nEncoding = IDM_UTF8) )
    PsPopupMenu_AddItem( ghPopupFileEncoding, IDM_UTF8BOM, _
                         Doc_EncodingName(FILE_ENCODING_UTF8_BOM),  "", false, (nEncoding = IDM_UTF8BOM) )
    PsPopupMenu_AddItem( ghPopupFileEncoding, IDM_UTF16BOM, _
                         Doc_EncodingName(FILE_ENCODING_UTF16_BOM), "", false, (nEncoding = IDM_UTF16BOM) )

    dim as POINT pt: GetCursorPos( @pt )
    ContextMenu_ShowAt( ghPopupFileEncoding, pt.x, pt.y, true )
end sub


' ========================================================================================
' Line endings (statusbar panel)
' ========================================================================================
sub ShowLineEndingsContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    PsPopupMenu_Clear( ghPopupLineEndings )
    PsPopupMenu_AddItem( ghPopupLineEndings, IDM_EOLTOCRLF, L(36,"Convert to CRLF") )
    PsPopupMenu_AddItem( ghPopupLineEndings, IDM_EOLTOLF,   L(38,"Convert to LF") )

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

    gConfig.TabSize = str(idMenu - IDM_SPACES)
    dim as DWSTRING wszText = "Spaces: " & gConfig.TabSize
    PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, SPACES_PANEL, wszText )

    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        pDoc->ApplyProperties
        pDoc = pDoc->pDocNext
    loop
end sub

sub ShowSpacesContextMenu()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    PsPopupMenu_Clear( ghPopupSpaces )
    for i as long = 0 to 8
        dim as DWSTRING wszCaption = str(i)
        if str(i) = gConfig.TabSize then wszCaption = wszCaption & "  (Current Tab Size)"
        PsPopupMenu_AddItem( ghPopupSpaces, IDM_SPACES + i, wszCaption, "", false, _
                            (str(i) = gConfig.TabSize) )
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

    PsPopupMenu_Clear( ghPopupScintilla )

    ' Determine if the text under the cursor is an #Include file. if yes, then add a
    ' menu option to allow to open it.
    if IsCurrentLineIncludeFilename() then
        PsPopupMenu_AddItem( ghPopupScintilla, IDM_OPENINCLUDE, _
                L(72,"Open") + chr(32, 34) + gApp.IncludeFilename + chr(34) )
        PsPopupMenu_AddSeparator( ghPopupScintilla )
    end if

    PsPopupMenu_AddItem( ghPopupScintilla, IDM_UNDO, L(15,"Undo") )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_REDO, L(16,"Redo") )
    PsPopupMenu_AddSeparator( ghPopupScintilla )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_CUT,   L(17,"Cut") )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_COPY,  L(18,"Copy") )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_PASTE, L(19,"Paste") )
    PsPopupMenu_AddSeparator( ghPopupScintilla )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_DEBUG_TOGGLEBREAKPOINT, L(245,"Toggle Breakpoint") )
    if AppState_IsDebuggerActive() then
        PsPopupMenu_AddItem( ghPopupScintilla, IDM_DEBUG_RUNTOCURSOR, L(252,"Run to Cursor") )
    end if
    PsPopupMenu_AddSeparator( ghPopupScintilla )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_INDENTBLOCK,   L(23,"Block Indent") )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_UNINDENTBLOCK, L(24,"Block Unindent") )
    PsPopupMenu_AddSeparator( ghPopupScintilla )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_COMMENTBLOCK,   L(25,"Block Comment") )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_UNCOMMENTBLOCK, L(26,"Block Uncomment") )
    PsPopupMenu_AddSeparator( ghPopupScintilla )
    PsPopupMenu_AddItem( ghPopupScintilla, IDM_SELECTALL, L(40,"Select All") )

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
    dim as long nSelCount = PsListTree_GetSelCount( hCtl )
    if nSelCount < 1 then exit sub

    dim pDocArray(nSelCount-1) as clsDocument ptr
    dim as integer nextIdx = 0
    for i as integer = 0 to PsListTree_GetCount(hCtl) - 1
        if PsListTree_GetSel( hCtl, i ) = true then
            if nextIdx > ubound(pDocArray) then exit for
            ' No IsHeader skip: group rows are plain tree parents now, and non-selectable
            ' besides, so GetSel is false for them. DocFromRow returns NULL for every
            ' non-file row regardless, and the loop below already skips nulls.
            pDocArray(nextIdx) = frmExplorer_DocFromRow( hCtl, i )
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

' ========================================================================================
' Right-click menu for a group or folder row.
'
' The action icons only appear on the hot row, so they are a mouse-only affordance and one
' the user has to discover by hovering. This is the same three actions spelled out. Rename
' and Delete are offered on folders alone -- a root group has neither, its caption coming
' from gConfig.Cat() and its existence being what every file's ProjectFileType names --
' which is the same rule frmExplorer_IconRects applies, expressed the same way round.
'
' Lives here rather than in frmExplorer.inc because that file is included 58 lines earlier
' than this one and so can see neither the popup handles nor ContextMenu_ShowAtCursor. Same
' reason ShowExplorerContextMenu below is here.
' ========================================================================================
sub ShowExplorerFolderContextMenu( byval hCtl as HWND, byval row as integer )
    dim as EXPLORER_ROWKIND kind = frmExplorer_KindFromRow( hCtl, row )
    dim as long catIndex
    frmExplorer_FolderPathFromRow( hCtl, row, catIndex )
    if catIndex < 0 then exit sub
    if ProjectFolders_CatAllowsFolders( catIndex ) = false then exit sub

    ' The row the commands will act on, remembered rather than re-derived when the menu
    ' closes. PsPopupMenu is NOT modal, so the callback runs later -- and every one of these
    ' commands rebuilds the tree, so an index resolved at that point would be meaningless.
    gExpMenuRow = row

    PsPopupMenu_Clear( ghPopupExplorerFolder )
    PsPopupMenu_AddItem( ghPopupExplorerFolder, IDM_EXPLORER_NEWFOLDER, L(462,"New Folder") )
    if kind = EXPKIND_FOLDER then
        PsPopupMenu_AddItem( ghPopupExplorerFolder, IDM_EXPLORER_RENAMEFOLDER, L(464,"Rename Folder") )
        PsPopupMenu_AddSeparator( ghPopupExplorerFolder )
        PsPopupMenu_AddItem( ghPopupExplorerFolder, IDM_EXPLORER_DELETEFOLDER, L(463,"Delete Folder") )
    end if
    ContextMenu_ShowAtCursor( ghPopupExplorerFolder )
end sub


' ========================================================================================
' The folder menu's command handler, fired by PsPopupMenu once the menu has closed.
'
' Reads gExpMenuRow rather than the live selection, and clears it on the way in so a second
' delivery cannot act twice. NOT routed through ContextMenu_PostCommand: these ids act on a
' remembered row index, not on a document, so the generic WM_COMMAND dispatcher has nothing
' to hand them.
' ========================================================================================
private sub ExplorerFolderContextMenu_OnSelect( byval hPopup as HWND, byval id as long )
    dim as long row = gExpMenuRow
    gExpMenuRow = -1
    if row < 0 then exit sub

    select case id
        case IDM_EXPLORER_NEWFOLDER:    frmExplorer_NewFolder( HWND_FRMEXPLORER, row )
        case IDM_EXPLORER_RENAMEFOLDER: frmExplorer_RenameFolder( HWND_FRMEXPLORER, row )
        case IDM_EXPLORER_DELETEFOLDER: frmExplorer_DeleteFolder( HWND_FRMEXPLORER, row )
    end select
end sub


sub ShowExplorerContextMenu( byval pDoc as clsDocument ptr )
    ' if pDoc is zero then most likely we have right clicked on a Project or FILES root node
    if pDoc = 0 then exit sub

    ' Some of our available choices are affected by the fact that multiple selections
    ' in the explorer listbox may exist. In that case, we allow options like "Close"
    ' and also don't set a selected FileType.
    dim as long nSelCount = PsListTree_GetSelCount( HWND_FRMEXPLORER )

    PsPopupMenu_Clear( ghPopupExplorer )

    ' One menu for every workspace. The old non-project variant offered no Open (a listed
    ' file was always already open) and no Remove (closing removed it). Both assumptions
    ' are gone: a file stays listed after its tab closes, so it can be reopened, and Remove
    ' is now the only way it leaves the workspace.
    PsPopupMenu_AddItem( ghPopupExplorer, IDM_FILEOPEN_EXPLORERLISTBOX, L(72,"Open") )
    PsPopupMenu_AddSeparator( ghPopupExplorer )
    if (gTTabCtl.GetTabIndexByDocumentPtr( pDoc ) <> -1) orelse (nSelCount > 1) then
        PsPopupMenu_AddItem( ghPopupExplorer, IDM_FILECLOSE_EXPLORERLISTBOX, L(5,"Close") )
        PsPopupMenu_AddSeparator( ghPopupExplorer )
    end if
    if nSelCount = 1 then
        PsPopupMenu_AddItem( ghPopupExplorer, IDM_FILESAVE_EXPLORERLISTBOX,      L(7,"Save") )
        PsPopupMenu_AddItem( ghPopupExplorer, IDM_FILESAVEAS_EXPLORERLISTBOX,    L(8,"Save As") )
        PsPopupMenu_AddItem( ghPopupExplorer, IDM_FILERENAME_EXPLORERLISTBOX,    L(21,"Rename") )
        PsPopupMenu_AddItem( ghPopupExplorer, IDM_FILEDUPLICATE_EXPLORERLISTBOX, L(22,"Duplicate") )
        PsPopupMenu_AddSeparator( ghPopupExplorer )
    end if
    ' "Remove from project" is offered only while the workspace is NAMED. An untitled
    ' workspace has no project for a file to be removed FROM, and the wording reads as a
    ' contradiction there -- so closing is what takes the file out of the Explorer instead
    ' (see the removal rule in OnCommand_FileClose).
    dim as boolean bIsNamed = gApp.IsProjectNamed()
    if bIsNamed then
        PsPopupMenu_AddItem( ghPopupExplorer, IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX, _
                            L(152,"Remove from project") )
    end if

    ' Only charge a separator when Remove was actually added -- the blocks above always
    ' end with one, so charging it unconditionally gives two in a row on the untitled path.
    AddProjectFileTypesToPopup( ghPopupExplorer, pDoc, bIsNamed, true )

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
            PsPopupMenu_Clear( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_EXPLORER_EXPANDALL,   L(279,"Expand All") )
            PsPopupMenu_AddItem( hPopup, IDM_EXPLORER_COLLAPSEALL, L(280,"Collapse All") )
            PsPopupMenu_AddSeparator( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_CLOSEPANEL, L(135,"Close") )

        case HWND_FRMFUNCTIONS
            hPopup = ghPopupFunctionsAction
            PsPopupMenu_Clear( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_EXPANDALL,   L(279,"Expand All") )
            PsPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_COLLAPSEALL, L(280,"Collapse All") )
            PsPopupMenu_AddSeparator( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_VIEWASTREE, L(282,"View as Tree") )
            PsPopupMenu_AddItem( hPopup, IDM_FUNCTIONS_VIEWASLIST, L(283,"View as List") )
            PsPopupMenu_AddSeparator( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_CLOSEPANEL, L(135,"Close") )

        case HWND_FRMBOOKMARKS
            hPopup = ghPopupBookmarksAction
            PsPopupMenu_Clear( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_BOOKMARKS_EXPANDALL,   L(279,"Expand All") )
            PsPopupMenu_AddItem( hPopup, IDM_BOOKMARKS_COLLAPSEALL, L(280,"Collapse All") )
            PsPopupMenu_AddSeparator( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_BOOKMARKCLEARALLDOCS, L(52,"Clear Bookmarks") )
            PsPopupMenu_AddSeparator( hPopup )
            PsPopupMenu_AddItem( hPopup, IDM_CLOSEPANEL, L(135,"Close") )

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

    PsPopupMenu_Clear( ghPopupTopTabCtl )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILECLOSE,     L(5,"Close") )
    PsPopupMenu_AddSeparator( ghPopupTopTabCtl )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILESAVE,      L(7,"Save") )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILESAVEAS,    L(8,"Save As") )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILERENAME,    L(21,"Rename") )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILEDUPLICATE, L(22,"Duplicate") )
    PsPopupMenu_AddSeparator( ghPopupTopTabCtl )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILECLOSEALL,       L(6,"Close All"),               "", (MaxiTab <= 0) )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_FILECLOSEALLOTHERS, L(255,"Close Others"),          "", (MaxiTab <= 0) )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_CLOSEALLBACKWARD,   L(256,"Close Tabs on the Left"), "", (CuriTab <= 0) )
    PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_CLOSEALLFORWARD,    L(257,"Close Tabs on the Right"), "", (MaxiTab <= CuriTab) )

    dim pDoc as clsDocument ptr = gTTabCtl.GetDocumentPtr(idx)
    if pDoc then
        ' Removing a file from the workspace is only meaningful while the workspace is
        ' NAMED. Untitled, there is no project to remove it from and closing the tab is
        ' what drops it from the Explorer (see OnCommand_FileClose).
        PsPopupMenu_AddSeparator( ghPopupTopTabCtl )
        if gApp.IsProjectNamed() then
            PsPopupMenu_AddItem( ghPopupTopTabCtl, IDM_REMOVEFILEFROMPROJECT, L(152,"Remove from project") )
        end if
        AddProjectFileTypesToPopup( ghPopupTopTabCtl, pDoc, true, false )
    end if

    ContextMenu_ShowAtCursor( ghPopupTopTabCtl )
end sub


' ========================================================================================
' The open-tabs list (top tabs "list" button)
'
' Own callback: the item ids are tab INDEXES, not commands, so they must never reach
' frmMain's WM_COMMAND dispatcher. They stay 1-based -- id 0 is PsPopupMenu's separator
' marker, so a zeroth tab could not be addressed by id.
' ========================================================================================
private sub TopTabsListContextMenu_OnSelect( byval hPopup as HWND, byval id as long )
    if (id > 0) andalso (id <= gTTabCtl.GetItemCount) then gTTabCtl.SetFocusTab( id - 1 )
end sub

sub ShowTopTabsListContextMenu( byval rcButton as RECT )
    PsPopupMenu_Clear( ghPopupTopTabsList )
    dim as long nCurSel = gTTabCtl.GetCurSel()
    for idMenu as long = 0 to gTTabCtl.GetItemCount - 1
        PsPopupMenu_AddItem( ghPopupTopTabsList, idMenu + 1, gTTabCtl.GetTabText(idMenu), _
                            "", false, (idMenu = nCurSel) )
    next
    ContextMenu_ShowAt( ghPopupTopTabsList, rcButton.right, rcButton.bottom, true )
end sub


' ========================================================================================
' The split-editor button on the top tabs icon bar.
'
' The button used to toggle a split directly, choosing the axis from whichever icon the
' Alt-key watcher in modMsgPump happened to have left on it -- so the command depended on
' a modifier held at click time and was undiscoverable. It now offers both axes as a menu.
'
' The item ids ARE the real commands (IDM_SPLITLEFTRIGHT / IDM_SPLITTOPBOTTOM), so this
' popup rides the shared ContextMenu_PostCommand callback. They are distinct from the
' opener's own id (IDM_EDITORSPLIT), which is what the id-collision trap in Learnings.md
' is about.
' ========================================================================================
sub ShowEditorSplitContextMenu( byval rcButton as RECT )
    ' getMenuAccelText returns the keystroke with a LEADING TAB (it is built for the
    ' caption & chr(9) & accel shape the menubar uses); PsPopupMenu takes the accel as its
    ' own parameter, so parse the text back out. Empty when the command has no shortcut.
    dim as DWSTRING wszAccelLR = PsStrParse( getMenuAccelText(IDM_SPLITLEFTRIGHT), 2, wchr(9) )
    dim as DWSTRING wszAccelTB = PsStrParse( getMenuAccelText(IDM_SPLITTOPBOTTOM), 2, wchr(9) )

    PsPopupMenu_Clear( ghPopupEditorSplit )
    PsPopupMenu_AddItem( ghPopupEditorSplit, IDM_SPLITLEFTRIGHT, L(67,"Split Editor Left/Right"), wszAccelLR )
    PsPopupMenu_AddItem( ghPopupEditorSplit, IDM_SPLITTOPBOTTOM, L(68,"Split Editor Top/Bottom"), wszAccelTB )
    ContextMenu_ShowAt( ghPopupEditorSplit, rcButton.right, rcButton.bottom, true )
end sub


' ========================================================================================
' The Output panel's Compiler Results list.
'
' The list is in EXTENDED-select mode (Shift ranges, Ctrl toggles), so "Copy" can span
' many rows; they are joined with CRLF, and within a row the cells are joined with a TAB
' so a paste into a spreadsheet or another editor keeps the columns.
'
' COLUMN 0 IS SKIPPED, and that is not a tidy-up: it is the 20px SPACER column
' frmOutput_CreateListView adds before Line/File/Description, and it is always empty. A
' generic "join every cell" would put a leading tab on every line.
'
' ITS ITEM IDS ARE IDM_COPY / IDM_SELECTALL BUT IT MUST NOT USE ContextMenu_PostCommand:
' those are the EDITOR's commands and frmMain would route them to Scintilla, copying the
' source document instead of the results. Hence a callback of its own that acts on the
' list directly. (The captions are reused for the same reason they are the right ones --
' L(18) and L(40) already exist, so no new .lang ids were minted.)
' ========================================================================================
private sub OutputResultsContextMenu_OnSelect( byval hPopup as HWND, byval id as long )
    dim as HWND hLV = HWND_FRMOUTPUT_LVRESULTS
    if hLV = 0 then exit sub

    select case id
        case IDM_SELECTALL
            PsListTree_SelectAll( hLV, true )
            PsListTree_Refresh( hLV )

        case IDM_COPY
            dim as long nCols = PsListTree_GetColumnCount( hLV )
            dim selItems() as integer
            dim as long nSel = PsListTree_GetSelItems( hLV, selItems() )
            if nSel <= 0 then exit sub

            dim as DWSTRING wszOut, wszRow
            for i as long = 0 to nSel - 1
                wszRow = ""
                if nCols <= 1 then
                    ' Single-column (no header band) -- the row's own text is all there is.
                    wszRow = PsListTree_GetText( hLV, selItems(i) )
                else
                    for c as long = 1 to nCols - 1
                        if c > 1 then wszRow = wszRow & chr(9)
                        wszRow = wszRow & PsListTree_GetCellText( hLV, selItems(i), c )
                    next
                end if
                if i > 0 then wszOut = wszOut & chr(13) & chr(10)
                wszOut = wszOut & wszRow
            next

            AfxSetClipboardText( *wszOut.Wz() )
    end select
end sub


sub ShowOutputResultsContextMenu()
    dim as HWND hLV = HWND_FRMOUTPUT_LVRESULTS
    if hLV = 0 then exit sub
    ' Nothing in the list means nothing to copy and nothing to select: show no menu at all
    ' rather than two dead rows (ContextMenu_ShowAt's own empty-menu rule, one level up).
    if PsListTree_GetCount( hLV ) = 0 then exit sub

    PsPopupMenu_Clear( ghPopupOutputResults )
    PsPopupMenu_AddItem( ghPopupOutputResults, IDM_COPY, L(18, "Copy"), "", _
                        (PsListTree_GetSelCount( hLV ) = 0) )
    PsPopupMenu_AddItem( ghPopupOutputResults, IDM_SELECTALL, L(40, "Select All") )

    ContextMenu_ShowAtCursor( ghPopupOutputResults )
end sub


' ========================================================================================
' The Output panel's tab strip and icon bands: undock the panel into its own floating
' window, or dock it back.
'
' One item, and its CAPTION carries the direction rather than the id -- the same single
' toggle the undock icon beside the "X" performs, so there is one command and one code
' path whichever way the panel is currently sitting.
'
' Its id is IDM_OUTPUTDOCKTOGGLE and it deliberately does NOT go through
' ContextMenu_PostCommand: undocking reparents HWND_FRMOUTPUT, and doing that from a POSTED
' message would run it at an arbitrary later point in the pump rather than while the menu's
' own gesture is finished and the mouse capture released.
' ========================================================================================
private sub OutputDockContextMenu_OnSelect( byval hPopup as HWND, byval id as long )
    if id = IDM_OUTPUTDOCKTOGGLE then frmOutputFloat_Toggle()
end sub


sub ShowOutputDockContextMenu()
    if IsWindow( HWND_FRMOUTPUT ) = 0 then exit sub

    PsPopupMenu_Clear( ghPopupOutputDock )
    if frmOutputFloat_IsFloating() then
        PsPopupMenu_AddItem( ghPopupOutputDock, IDM_OUTPUTDOCKTOGGLE, L(394, "Dock Output Window") )
    else
        PsPopupMenu_AddItem( ghPopupOutputDock, IDM_OUTPUTDOCKTOGGLE, L(393, "Undock Output Window") )
    end if

    ContextMenu_ShowAtCursor( ghPopupOutputDock )
end sub


' ========================================================================================
' Plumbing
' ========================================================================================

' Each PsTextBox creates its own Cut/Copy/Paste/Select All popup. The control does not
' know tiko's theme, so hand its popup to the same registry the topmenu dropdowns use --
' after which a theme switch repaints it along with everything else.
sub ThemeTextBoxContextMenu( byval hTextBox as HWND )
    frmMenuBar_RegisterPopup( PsTextBox_GetContextMenu( hTextBox ), true )
end sub


function ContextMenu_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if ghContextMenuOpen = 0 then return false
    if IsWindow( ghContextMenuOpen ) = 0 then
        ghContextMenuOpen = 0
        return false
    end if
    ' The control's filter is self-guarding: it stands down unless this handle really is
    ' in the currently open chain, so a stale handle here costs one comparison.
    return PsPopupMenu_FilterMessage( ghContextMenuOpen, pMsg )
end function


sub ContextMenu_CloseAll()
    if ghContextMenuOpen = 0 then exit sub
    if IsWindow( ghContextMenuOpen ) then PsPopupMenu_CloseChain( ghContextMenuOpen )
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
    ghPopupExplorerFolder   = frmMenuBar_CreatePopup( true )
    ghPopupExplorerAction   = frmMenuBar_CreatePopup( true )
    ghPopupFunctionsAction  = frmMenuBar_CreatePopup( true )
    ghPopupBookmarksAction  = frmMenuBar_CreatePopup( true )
    ghPopupTopTabCtl        = frmMenuBar_CreatePopup( true )
    ghPopupTopTabsList      = frmMenuBar_CreatePopup( true )
    ghPopupEditorSplit      = frmMenuBar_CreatePopup( true )
    ghPopupOutputResults    = frmMenuBar_CreatePopup( true )
    ghPopupOutputDock       = frmMenuBar_CreatePopup( true )

    ' Menus whose ids are real command ids: straight through the WM_COMMAND dispatcher.
    PsPopupMenu_SetSelectCallback( ghPopupTabsInfoFileType, @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupFileEncoding,     @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupLineEndings,      @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupScintilla,        @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupExplorerAction,   @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupFunctionsAction,  @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupBookmarksAction,  @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupTopTabCtl,        @ContextMenu_PostCommand )
    PsPopupMenu_SetSelectCallback( ghPopupEditorSplit,      @ContextMenu_PostCommand )

    ' Menus whose ids are NOT safe as commands (see each callback's comment).
    PsPopupMenu_SetSelectCallback( ghPopupBuildConfig,  @BuildConfigContextMenu_OnSelect )
    PsPopupMenu_SetSelectCallback( ghPopupSpaces,       @SpacesContextMenu_OnSelect )
    PsPopupMenu_SetSelectCallback( ghPopupTopTabsList,  @TopTabsListContextMenu_OnSelect )
    PsPopupMenu_SetSelectCallback( ghPopupExplorer,     @ExplorerContextMenu_OnSelect )
    PsPopupMenu_SetSelectCallback( ghPopupExplorerFolder,   @ExplorerFolderContextMenu_OnSelect )
    PsPopupMenu_SetSelectCallback( ghPopupOutputResults, @OutputResultsContextMenu_OnSelect )
    PsPopupMenu_SetSelectCallback( ghPopupOutputDock,    @OutputDockContextMenu_OnSelect )

end sub
