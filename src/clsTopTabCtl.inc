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

#include once "clsTopTabCtl.bi"
#include once "frmTopTabs.bi"


''
''
function clsTopTabCtl.GetItemCount() as long
    return CTabBar_GetCount( this.hWindow )
end function

''
''
function clsTopTabCtl.GetCurSel() as integer
    return CTabBar_GetCurSel( this.hWindow )
end function

''
''
function clsTopTabCtl.SetCurSel( byval idx as integer ) as boolean
    return CTabBar_SetCurSel( this.hWindow, idx )
end function

''
''
function clsTopTabCtl.IsValidTab( byval idx as integer ) as boolean
    return CTabBar_IsValidTab( this.hWindow, idx )
end function

''
''
function clsTopTabCtl.GetpDoc( byval idx as integer ) as clsDocument ptr
    if this.IsValidTab(idx) = false then return null
    ' Cast the pDoc stored in the itemData integer.
    dim as integer itemData = CTabBar_GetItemData(this.hWindow, idx)
    return cast(clsDocument ptr, itemData)
end function

''
''
function clsTopTabCtl.AddTab( byval pDoc as clsDocument ptr ) as long
    dim as integer idx = CTabBar_AddTab( this.hWindow, "", cast(integer, pDoc) )
    this.SetTabText(idx)
    function = idx
end function

''
''
function clsTopTabCtl.InsertTab( byval pDoc as clsDocument ptr, byval insertIdx as long ) as long
    dim as integer idx = CTabBar_InsertTab( this.hWindow, insertIdx, "", cast(integer, pDoc) )
    this.SetTabText(idx)
    function = idx
end function

''
''
function clsTopTabCtl.RemoveElement( byval idx as long ) as long
    CTabBar_DeleteTab( this.hWindow, idx )
    function = 0
end function


''
''
function clsTopTabCtl.GetTabIndexFromFilename( byval wszName as DWSTRING ) as long
    ' Get the index of tab that holds the incoming filename. This is used
    ' mostly for positioning the editor to the correct document when an
    ' error occurs during compiling. 
    ' Returns -1 if not found, otherwise zero based index of tab.
    dim as long nCount = this.GetItemCount()
    if nCount = 0 then return -1 
    if len(wszName) = 0 then return -1
    
    dim as clsDocument ptr pDoc
    
    for i as integer = 0 to nCount - 1
        pDoc = this.GetpDoc(i)
        if pDoc then
            if ucase(pDoc->DiskFilename) = ucase(wszName) then return i
        end if   
    next

    function = -1  ' if not found
end function


''
''
function clsTopTabCtl.GetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
    dim as long nCount = this.GetItemCount()
    if nCount = 0 then return -1 
    if pDocIn = 0 then return -1

    dim as clsDocument ptr pDoc

    for i as integer = 0 To nCount - 1
        pDoc = this.GetpDoc(i)
        if pDoc = pDocIn then return i
    next

    function = -1  ' if not found
end function


''
''
function clsTopTabCtl.SetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
    dim as long nCount = this.GetItemCount()
    if nCount = 0 then return -1 
    if pDocIn = 0 then return -1

    dim as clsDocument ptr pDoc

    for i as integer = 0 to nCount - 1
        pDoc = this.GetpDoc(i)
        if pDoc = pDocIn then 
            gTTabCtl.TabFocusLost( gTTabCtl.GetCurSel() )
            return this.SetFocusTab(i)
        end if   
    next
    function = -1  ' if not found
end function


''
''
function clsTopTabCtl.SetFocusTab( byval idx as long ) as long
    if this.IsValidTab(idx) = false then exit function

    ' TOPTABS_CHANGED:  Set the new current tab and display edit window, etc.
    CTabBar_SetCurSel( this.hWindow, idx )

'            ' Hide the companion frmTopTabsInfo pane if this is not a NORMAL document.
'            ' For example, if displaying "Find in Project" search.
'            if gTTabCtl.tabs(gTTabCtl.CurSel).docType = DocumentType.ProjectSearch then
'                TopTabsInfoPanel_Hide()
'                FindControls_Show()
'                if gFind.bShowReplacePanel then
'                    ReplaceControls_Show()
'                end if
'                frmMain_PositionWindows()
'                dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND)
'                SetFocus( hCtl )
'            else
                TopTabsInfoPanel_Show()
                gTTabCtl.DisplayScintilla( idx, true )
                frmMain_PositionWindows()

                dim pDoc as clsDocument ptr = this.GetpDoc(idx)
                if pDoc then
                    AfxRedrawWindow( pDoc->hWindow(0) )  ' makes the code window display quicker especially when loading Project.
                    AfxRedrawWindow( pDoc->hWindow(1) )  ' makes the code window display quicker especially when loading Project.

                    ' We have moved to a new Tab. If Find is open then highlight any search phrase.   
                    FindReplace_HighlightSearches()

                    ' Set the focus to the Scintilla window. This will call frmMain_SetStatusbar.
                    frmMain_SetFocusToCurrentCodeWindow()

                    ' Highlight the selected tab file in the Explorer listbox
                    frmExplorer_SelectItemData( this.GetpDoc(idx) )

                    ' Highlight the selected tab file in the Function List listbox
                    frmFunctions_SelectItemData( this.GetpDoc(idx) )

                    ' Refresh the background symbol scan for the newly active document
                    if (gApp.IsProjectLoading = false) andalso (gApp.IsShutdown = false) then
                        gScanMgr.RequestBufferScan( pDoc )
                    end if

                    ' If moving to a new Tab and the Find panel is closed then ensure that we 
                    ' remove any find occurrence highlights.
                    if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) = 0 then pDoc->RemoveSelectionAttributes()
                end if
'            end if    

    function = CTabBar_GetCurSel( this.hWindow )
end function


''
''
function clsTopTabCtl.NextTab() as long
    ' Invoked by Ctl+TAB
    dim as integer nCurSel = gTTabCtl.GetCurSel()
    dim as integer nNewSel = nCurSel + 1
    if nNewSel <= gTTabCtl.GetItemCount - 1 then
        this.TabFocusLost( nCurSel )
        return this.SetFocusTab( nNewSel )
    end if
    return -1    
end function

''
''
function clsTopTabCtl.PrevTab() as long
    ' Invoked by Ctl+Shift+TAB
    dim as integer nCurSel = gTTabCtl.GetCurSel()
    dim as integer nNewSel = nCurSel - 1
    if nNewSel >= 0 then
        this.TabFocusLost( nCurSel )
        return this.SetFocusTab( nNewSel )
    end if
    return -1    
end function


''
''
function clsTopTabCtl.TabFocusLost( byval nTabIdx as long = -1) as long
    ' If changing away from the Project Search tab then close any open
    ' Find and Replace panes so that they do not display when the new tab
    ' is switched to and displayed.
'    if gTTabCtl.IsValidTab(nTabIdx) then
'        if gTTabCtl.tabs(nTabIdx).docType = DocumentType.ProjectSearch then
'            gFind.bShowInfoPanel = true
'            gFind.bShowFindPanel = false
'            gFind.bShowReplacePanel = false
'            gFind.bExpanded = false
'        end if

'        ' If moving to a new Tab and the Find panel is closed then ensure that we 
'        ' remove any find occurrence highlights.
'        dim as clsDocument ptr pDoc = this.GetActiveDocumentPtr()
'        if pDoc then 
'            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) = 0 then pDoc->RemoveSelectionAttributes()
'        end if
'    end if

    ' Hide the editing window being moved away from
    gTTabCtl.DisplayScintilla( nTabIdx, false )
    
    function = 0
end function


''
''
function clsTopTabCtl.CloseTab( byval nTabIdx as long = -1) as long
    ' Invoked by clicking "X" on tab
    OnCommand_FileClose( HWND_FRMMAIN, EFC_CLOSECURRENT, nTabIdx )
    function = 0
end function


''
''
function clsTopTabCtl.GetActiveDocumentPtr() as clsDocument ptr
    ' Return pointer to clsDocument class for the current active tab
    if this.GetItemCount() = 0 then return null
    return this.GetpDoc( this.GetCurSel() )
end function

''
''
function clsTopTabCtl.GetDocumentPtr( byval idx as long ) as clsDocument ptr
    ' Return pointer to clsDocument class for the current active tab
    if this.IsValidTab(idx) = false then return null
    return this.GetpDoc(idx)
end function

''
''
function clsTopTabCtl.DisplayScintilla( byval idx as long, byval bShow as boolean ) as long
    ' Show/Hide the Scintilla editing window for the incoming tab index
    if this.IsValidTab(idx) = false then exit function
    dim as clsDocument ptr pDocShow = this.GetpDoc(idx)
    if pDocShow = null then return 0

    ' Hide all documents and their associated scrollbars
    dim pDoc as clsDocument ptr = gApp.pDocList
    
    do until pDoc = 0
        ' Scintilla windows and scrollbars
        ShowWindow(pDoc->hWindow(0), SW_HIDE)
        ShowWindow(pDoc->hWindow(1), SW_HIDE)
        pDoc = pDoc->pDocNext
    loop
    
    ' Show/Hide our current active Scintilla window and scrollbar
    ShowWindow(pDocShow->hWindow(0), iif(bShow, SW_SHOWNORMAL, SW_HIDE))
    ShowWindow(pDocShow->hWindow(1), iif(bShow, SW_SHOWNORMAL, SW_HIDE))
    
    function = 0
end function
    
''
''
function clsTopTabCtl.GetTabText( byval idx as long ) as DWSTRING
    if this.IsValidTab(idx) = false then return ""
    return CTabBar_GetText( this.hWindow, idx)
end function

''
''
function clsTopTabCtl.SetTabText( byval idx as long ) as long
    ' Set the text for the incoming tab index. if the index
    ' is -1 then set the text of the current tab.
    if idx = -1 then idx = CTabBar_GetCurSel( this.hWindow )
    if this.IsValidTab(idx) = false then return idx

    dim as clsDocument ptr pDoc = this.GetpDoc(idx)

    if pDoc then 
        ' Set the text that displays on the tab
        dim as DWSTRING wszOldText = this.GetTabText( idx )
        dim as DWSTRING wszNewText = AfxStrPathname( "NAMEX", pDoc->DiskFilename )
            
        ' We only update the text if it has changed in order to prevent flicker.
        if wszNewText <> wszOldText then
            CTabBar_SetText( this.hWindow, idx, wszNewText )
        end if
    end if
    
    function = idx
end function

