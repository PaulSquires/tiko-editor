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

''
''
function clsTopTabCtl.IsSafeIndex( byval idx as long ) as boolean
    ' test to see if the incoming idx is valid based on the boundaries
    ' of the global tab array
    if (idx >= lbound(this.tabs)) andalso (idx <= ubound(this.tabs)) then
        return true
    else   
        return false   
    end if
end function

''
''
function clsTopTabCtl.GetItemCount() as long
    ' get the number tabs in the global tab array
    return ubound(this.tabs) - lbound(this.tabs) + 1
end function

''
''
function clsTopTabCtl.AddTab( byval pDoc as clsDocument ptr ) as long
    ' Return the index of the added tab. We attempt to add it immediately
    ' after the current active tab.
    dim idx as long
    redim preserve this.tabs(ubound(this.tabs) + 1) as TOPTABS_TYPE
    
    if (this.CurSel = -1) orelse (this.CurSel = this.GetItemCount - 1) then
        ' add the tab at the end of the array
        idx = ubound(this.tabs)
    else
        ' insert the tab after the current tab
        for i as long = ubound(this.tabs) to this.CurSel + 1 step -1
            this.tabs(i) = this.tabs(i - 1)
        next
        idx = this.CurSel + 1
    end if
    
    this.tabs(idx).pDoc = pDoc
    this.SetTabText(idx)

    function = idx
end function

''
''
function clsTopTabCtl.InsertTab( byval pDoc as clsDocument ptr, byval insertIdx as long ) as long
    ' Return the index of the inserted tab. We attempt to insert it at the
    ' insertIdx tab.
    dim idx as long
    redim preserve this.tabs(ubound(this.tabs) + 1) as TOPTABS_TYPE
    
    if insertIdx = this.GetItemCount - 1 then
        ' add the tab at the end of the array
        idx = ubound(this.tabs)
    else
        ' move down elements in order to make room for the insert
        for i as long = ubound(this.tabs) to insertIdx + 1 step -1
            this.tabs(i) = this.tabs(i - 1)
        next
        idx = insertIdx + 1
    end if

    this.tabs(idx).pDoc = pDoc
    this.SetTabText(idx)

    function = idx
end function

''
''
function clsTopTabCtl.RemoveElement( byval idx as long ) as long
    ' remove an element from the this.tabs array
    if this.IsSafeIndex(idx) = false then exit function
    ' if this is the last element being removed then we need
    ' to erase the global array
    if this.GetItemCount() = 1 then
        erase this.tabs
    else
        for i as long = idx to ubound(this.tabs) - 1
            this.tabs(i) = this.tabs(i+1)
        next   
        redim preserve this.tabs(ubound(this.tabs)-1)
    end if
    function = 0
end function


''
''
function clsTopTabCtl.GetTabIndexFromFilename( byref wszName as wstring ) as long
    ' Get the index of tab that holds the incoming filename. This is used
    ' mostly for positioning the editor to the correct document when an
    ' error occurs during compiling. 
    ' Returns -1 if not found, otherwise zero based index of tab.
    
    dim as long nCount = this.GetItemCount()
    if nCount = 0 then return -1 
    if len(wszName) = 0 then return -1
    
    for i as long = 0 to nCount - 1
        if this.tabs(i).pDoc then
            if ucase(this.tabs(i).pDoc->DiskFilename) = ucase(wszName) then return i
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

    for i as long = 0 To nCount - 1
        if this.tabs(i).pDoc then
            if this.tabs(i).pDoc = pDocIn then return i
        end if   
    next

    function = -1  ' if not found
end function


''
''
function clsTopTabCtl.SetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
    dim as long nCount = this.GetItemCount()
    if nCount = 0 then return -1 
    if pDocIn = 0 then return -1
    for i as long = 0 to nCount - 1
        if this.tabs(i).pDoc = pDocIn then 
            function = this.SetFocusTab(i)
            exit function
        end if   
    next
    function = -1  ' if not found
end function


''
''
function clsTopTabCtl.SetFocusTab( byval idx as long ) as long
    dim as long nCount = this.GetItemCount()
    if nCount = 0 then return -1 
    if idx < 0 then idx = nCount - 1
    if this.IsSafeIndex(idx) = false then exit function
    if this.CurSel <> idx then
        ' Send a user message to accomplish the same thing as TCN_SELCHANGING and TCN_SELCHANGE 
        SendMessage( HWND_FRMMAIN, MSG_USER_TOPTABS_CHANGING, 0, 0 )
        this.CurSel = idx
        SendMessage( HWND_FRMMAIN, MSG_USER_TOPTABS_CHANGED, 0, 0 )
        ' Highlight the selected tab file in the Explorer listbox
        frmExplorer_SelectItemData( this.tabs(idx).pDoc )
        ' Highlight the selected tab file in the Function List listbox
        frmFunctions_SelectItemData( this.tabs(idx).pDoc )
        ' If moving to a new Tab and the Find panel is closed then ensure that we 
        ' remove any find occurrence highlights.
        dim as clsDocument ptr pDoc = this.GetActiveDocumentPtr()
        if pDoc then 
            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) = 0 then pDoc->RemoveSelectionAttributes()
        end if
    end if

    function = this.CurSel
end function


''
''
function clsTopTabCtl.NextTab() as long
    ' Invoked by Ctl+TAB
    ' Set the tab with focus (this sends the TCN_SELCHANGING and TCN_SELCHANGE 
    ' notification codes to its parent window). 
    dim as long nCount = this.GetItemCount
    dim as long idx = this.CurSel
    if nCount = 0 then exit function
    idx += 1
    if idx > nCount - 1 then idx = 0
    function = this.SetFocusTab(idx)
end function

''
''
function clsTopTabCtl.PrevTab() as long
    ' Invoked by Ctl+Shift+TAB
    ' Set the tab with focus (this sends the TCN_SELCHANGING and TCN_SELCHANGE 
    ' notification codes to its parent window). 
    dim as long nCount = this.GetItemCount 
    dim as long idx = this.CurSel
    if nCount = 0 then exit function
    idx -= 1
    if idx < 0 then idx = nCount - 1 
    function = this.SetFocusTab(idx)
end function


''
''
function clsTopTabCtl.TabFocusLost( byval nTabIdx as long = -1) as long
    ' If changing away from the Project Search tab then close any open
    ' Find and Replace panes so that they do not display when the new tab
    ' is switched to and displayed.
    if gTTabCtl.IsSafeIndex(nTabIdx) then
        if gTTabCtl.tabs(nTabIdx).docType = DocumentType.ProjectSearch then
            gFind.bShowInfoPanel = true
            gFind.bShowFindPanel = false
            gFind.bShowReplacePanel = false
            gFind.bExpanded = false
        end if

        ' If moving to a new Tab and the Find panel is closed then ensure that we 
        ' remove any find occurrence highlights.
        dim as clsDocument ptr pDoc = this.GetActiveDocumentPtr()
        if pDoc then 
            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) = 0 then pDoc->RemoveSelectionAttributes()
        end if
    end if
        
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
    if this.GetItemCount() = 0 then return 0 
    if this.IsSafeIndex(this.CurSel) = false then exit function
    function = this.tabs(this.CurSel).pDoc   
end function

''
''
function clsTopTabCtl.GetDocumentPtr( byval idx as long ) as clsDocument ptr
    ' Return pointer to clsDocument class for the current active tab
    if this.IsSafeIndex(idx) = false then exit function
    function = this.tabs(idx).pDoc
end function

''
''
function clsTopTabCtl.DisplayScintilla( byval idx as long, byval bShow as boolean ) as long
    ' Show/Hide the Scintilla editing window for the incoming tab index
    dim pDocShow as clsDocument ptr

    if this.IsSafeIndex(idx) = false then exit function
    pDocShow = this.tabs(idx).pDoc
    if pDocShow = 0 then exit function

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
function clsTopTabCtl.SetTabText( byval idx as long ) as long
    ' Set the text for the incoming tab index. if the index
    ' is -1 then set the text of the current tab.
    dim wszText as wstring * MAX_PATH  
    dim wszTemp as wstring * MAX_PATH  
    dim pDoc as clsDocument Ptr

    if idx = -1 then idx = gTTabCtl.CurSel
    if this.IsSafeIndex(idx) = false then exit function

    pDoc = this.tabs(idx).pDoc
    if pDoc then 
        ' Set the text that displays on the tab
        wszText = AfxStrPathname( "NAMEX", pDoc->DiskFilename )
            
        ' We only update the text if it has changed in order to prevent flicker.
        if wszText <> this.tabs(idx).wszText then
            this.tabs(idx).wszText = wszText
            frmTopTabs_PositionWindows()
        end if
    end if
    
    function = idx
end function

