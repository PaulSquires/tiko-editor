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
    return PsTabBar_GetCount( this.hWindow )
end function

''
''
function clsTopTabCtl.GetCurSel() as integer
    return PsTabBar_GetCurSel( this.hWindow )
end function

''
''
function clsTopTabCtl.SetCurSel( byval idx as integer ) as boolean
    return PsTabBar_SetCurSel( this.hWindow, idx )
end function

''
''
function clsTopTabCtl.IsValidTab( byval idx as integer ) as boolean
    return PsTabBar_IsValidTab( this.hWindow, idx )
end function

''
''
function clsTopTabCtl.GetpDoc( byval idx as integer ) as clsDocument ptr
    if this.IsValidTab(idx) = false then return null
    ' Cast the pDoc stored in the itemData integer.
    dim as integer itemData = PsTabBar_GetItemData(this.hWindow, idx)
    ' The Find in Project tab has no document. Filtering the sentinel HERE is what makes the
    ' ~70 "if pDoc then <dereference>" call sites correct without touching any of them -- see
    ' the note on TAB_FINDINPROJECT in clsTopTabCtl.bi.
    if itemData = TAB_FINDINPROJECT then return null
    return cast(clsDocument ptr, itemData)
end function


''
''  Raw itemData test, so this can see what GetpDoc deliberately hides.
''
function clsTopTabCtl.IsFindTab( byval idx as integer ) as boolean
    if this.IsValidTab(idx) = false then return false
    return (PsTabBar_GetItemData(this.hWindow, idx) = TAB_FINDINPROJECT)
end function


''
''  What "ActiveTab=" in a project file should hold.
''
''  It is NOT the raw GetCurSel index. The writer skips tabs on its way to disk -- a tab
''  with no document (Find in Project) and a document that has never been saved (IsNewFlag) --
''  so the file's index space is denser than the live tab bar's, and every tab after a skipped
''  one shifts down by one. Writing the live index means reopening on the wrong document.
''
''  ProjectLoadFromFile sanity-checks the value it reads with IsValidTab, but that cannot
''  detect an index which is in range and simply wrong, which is exactly what this produces.
''
''  The IsNewFlag half of this was already broken before the Find tab existed: any untitled
''  document sitting to the left of the active tab shifted it. Fixed here because the two skip
''  conditions have to agree with the writers, and there is no way to honour one without the
''  other.
''
function clsTopTabCtl.SaveActiveTabIndex() as long
    dim as long nCur = this.GetCurSel()
    if this.IsValidTab(nCur) = false then return 0

    ' The active tab is itself not written (the Find tab, or an unsaved document): there is no
    ' honest answer, so land on the first restored document.
    dim as clsDocument ptr pCur = this.GetpDoc(nCur)
    if pCur = 0 then return 0
    if pCur->IsNewFlag then return 0

    ' Count only the tabs ahead of it that WILL be written.
    dim as long nWritten = 0
    for i as long = 0 to nCur - 1
        dim as clsDocument ptr pDoc = this.GetpDoc(i)
        if pDoc = 0 then continue for
        if pDoc->IsNewFlag then continue for
        nWritten += 1
    next
    function = nWritten
end function


''
''  There is at most one Find in Project tab. Returns -1 when it is not open.
''
function clsTopTabCtl.FindTabIndex() as long
    dim as long nCount = this.GetItemCount()
    for i as long = 0 to nCount - 1
        if this.IsFindTab(i) then return i
    next
    function = -1
end function


''
''  Tabs that actually hold a document. This is what a menu enabler wants: with the Find tab
''  open, GetItemCount is non-zero while GetActiveDocumentPtr can still be null, which would
''  light up File and Compile commands over nothing.
''
function clsTopTabCtl.GetDocumentTabCount() as long
    dim as long nCount = this.GetItemCount()
    dim as long nDocs = 0
    for i as long = 0 to nCount - 1
        if this.GetpDoc(i) then nDocs += 1
    next
    function = nDocs
end function

''
''
function clsTopTabCtl.AddTab( byval pDoc as clsDocument ptr ) as long
    dim as integer idx = PsTabBar_AddTab( this.hWindow, "", cast(integer, pDoc) )
    this.SetTabText(idx)
    function = idx
end function

''
''
function clsTopTabCtl.InsertTab( byval pDoc as clsDocument ptr, byval insertIdx as long ) as long
    dim as integer idx = PsTabBar_InsertTab( this.hWindow, insertIdx, "", cast(integer, pDoc) )
    this.SetTabText(idx)
    function = idx
end function

''
''
function clsTopTabCtl.RemoveElement( byval idx as long ) as long
    PsTabBar_DeleteTab( this.hWindow, idx )
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
    if PsLen(wszName) = 0 then return -1
    
    dim as clsDocument ptr pDoc
    
    for i as integer = 0 to nCount - 1
        pDoc = this.GetpDoc(i)
        if pDoc then
            if ucase(pDoc->DiskFilename) = PsUCase(wszName) then return i
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
    PsTabBar_SetCurSel( this.hWindow, idx )

    ' The Find in Project tab has no document, so it takes none of the per-document work
    ' below. It replaces the file-info strip with the Find bar -- the find phrase IS this
    ' tab's subject, so the bar is not optional chrome here -- and puts the caret in it.
    if this.IsFindTab(idx) then
        FindControls_Show()
        if gFind.bShowReplacePanel then ReplaceControls_Show()
        ' TopTabsInfoPanel_Hide MUST come AFTER those two. FindReplace_SetVisible sets
        ' bShowInfoPanel = true unconditionally, so hiding the strip first is silently undone
        ' -- and it also early-returns when the bar state already matches, which makes the
        ' ordering matter in both directions. Hiding last is correct either way.
        TopTabsInfoPanel_Hide()
        ' Hide the outgoing document's editors. DisplayScintilla's hide-all loop does this
        ' even though there is nothing to show.
        gTTabCtl.DisplayScintilla( idx, true )
        frmMain_PositionWindows()
        ' POSTED, not set here. Back to where the caret was if it was in an excerpt, and the
        ' search box otherwise -- but a tab switch hides and shows several windows, and a
        ' SetFocus made part-way through that gets undone by the rest of it. frmMain's
        ' MSG_USER_SETFOCUS handler does it once the churn is over, which is what that
        ' message has always been for.
        frmMain_SetFocusToCurrentCodeWindow()
        return PsTabBar_GetCurSel( this.hWindow )
    end if

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
                    if IsWindowVisible(HWND_FRMMAIN_FIND) = 0 then pDoc->RemoveSelectionAttributes()
                end if

    function = PsTabBar_GetCurSel( this.hWindow )
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
    ' Leaving the Find in Project tab: put the file-info strip back and close the Find and
    ' Replace bars, so they do not follow us onto a document tab that never asked for them.
    ' SetFocusTab forced them open on the way in.
    if this.IsFindTab(nTabIdx) then
        gFind.bShowInfoPanel = true
        gFind.bShowFindPanel = false
        gFind.bShowReplacePanel = false
        gFind.bProjectReplaceActive = false
    end if

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

    ' HIDE-ALL RUNS FIRST, AND UNCONDITIONALLY. It used to sit below an early return on a
    ' null pDocShow, which meant switching TO a document-less tab (Find in Project) left the
    ' outgoing document's editors painted on top of it.
    '
    ' Fixing it here rather than at the call sites is deliberate: TabFocusLost -- which hides
    ' the outgoing tab -- is paired with SetFocusTab on only two of the nine paths that change
    ' tabs. The other seven call SetFocusTab bare (clsConfig's project load, three sites in
    ' frmMainFile, the tab right-click, the tabs-list menu, OpenSelectedDocument) and rely on
    ' this loop. One fix covers all seven.
    dim pDoc as clsDocument ptr = gApp.pDocList

    do until pDoc = 0
        ' Scintilla windows and scrollbars
        ShowWindow(pDoc->hWindow(0), SW_HIDE)
        ShowWindow(pDoc->hWindow(1), SW_HIDE)
        pDoc = pDoc->pDocNext
    loop

    ' Show/Hide our current active Scintilla window and scrollbar. Nothing to show for a tab
    ' with no document -- frmMain_PositionWindows puts the results surface in that space.
    if pDocShow then
        ShowWindow(pDocShow->hWindow(0), iif(bShow, SW_SHOWNORMAL, SW_HIDE))
        ShowWindow(pDocShow->hWindow(1), iif(bShow, SW_SHOWNORMAL, SW_HIDE))
    end if

    function = 0
end function
    
''
''
function clsTopTabCtl.GetTabText( byval idx as long ) as DWSTRING
    if this.IsValidTab(idx) = false then return ""
    return PsTabBar_GetText( this.hWindow, idx)
end function

''
''
function clsTopTabCtl.SetTabText( byval idx as long ) as long
    ' Set the text for the incoming tab index. if the index
    ' is -1 then set the text of the current tab.
    if idx = -1 then idx = PsTabBar_GetCurSel( this.hWindow )
    if this.IsValidTab(idx) = false then return idx

    dim as clsDocument ptr pDoc = this.GetpDoc(idx)

    ' The pDoc test is what protects the Find in Project tab's caption. That matters more than
    ' it looks: frmMain_SetStatusbar calls SetTabText(-1) on every caret move, so a caption
    ' set by hand with PsTabBar_SetText survives only because a doc-less tab falls straight
    ' through here. Do not "fix" this into an unconditional derive-from-filename.
    if pDoc then
        ' Set the text that displays on the tab
        dim as DWSTRING wszOldText = this.GetTabText( idx )
        dim as DWSTRING wszNewText = AfxStrPathname( "NAMEX", pDoc->DiskFilename )
            
        ' We only update the text if it has changed in order to prevent flicker.
        if wszNewText <> wszOldText then
            PsTabBar_SetText( this.hWindow, idx, wszNewText )
        end if
    end if
    
    function = idx
end function

