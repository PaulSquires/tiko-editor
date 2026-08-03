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

#include once "frmMain.bi"
#include once "clsDocument.bi"

' ========================================================================================
' Create a new editing window
' ========================================================================================
function OnCommand_FileNew( byval hwnd as HWND ) as clsDocument ptr
    dim pDoc as clsDocument ptr
    
    ' Create the new pDoc object and assign blank text buffer to it which will
    ' automatically create the scintilla windows.
    pDoc = gApp.CreateEmptyDocument( true )
    pDoc->AssignTextBuffer()

    ' Load Explorer names first SetFocusTab will highlight it.
    LoadExplorerFiles()
    LoadFunctionsFiles()
    LoadBookmarksFiles()

    ' Add the new document to the top tabcontrol
    dim as long iTab = gTTabCtl.AddTab( pDoc )  
    gTTabCtl.SetFocusTab(iTab)
    frmMain_PositionWindows
    frmMain_SetFocusToCurrentCodeWindow
    
    function = pDoc
end function


' ========================================================================================
' Open one or more files for editing
' ========================================================================================
function OnCommand_FileOpen( _
            byval hwnd as HWND, _
            byval bShowInTab as boolean = true, _
            byval bIsTemplate as boolean = false _
            ) as LRESULT

    ' This function is called from these top menu items:
    ' File / Open File
    ' Project / Add Files to Project
    
    ' Display the Open File Dialog
    dim pItems as IShellItemArray ptr = AfxIFileOpenDialogMultiple(hwnd, IDM_FILEOPEN)
    if pItems = null then exit function
    dim dwItemCount as long, i as long, pItem as IShellItem ptr, pwszName as wstring ptr
    pItems->lpVtbl->GetCount(pItems, @dwItemCount)
    
    ' Variable length array to hold all filenames to open. 
    dim AllFilenames(dwItemCount - 1) as DWSTRING
    
    ' Variable length array to hold sequence of TabCtrl tabs to open. We save the sequence
    ' here and open them only after all over documents have been loaded. This will look
    ' visually more appealing for those files that will display in the top Tab Control.
    dim pDocTabs(dwItemCount - 1) as clsDocument ptr

    SetCursor( LoadCursor(0, IDC_WAIT) )
    dim pDoc as clsDocument ptr
    pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then SciExec( pDoc->hWindow(0), SCI_SETCURSOR, SC_CURSORWAIT, 0 )

    ' Prevent the constant updating of the app caption bar with filename
    gApp.IsFileLoading = true  
    
    for i = 0 To dwItemCount - 1
        pItems->lpVtbl->GetItemAt(pItems, i, @pItem)
        if pItem then
            pItem->lpVtbl->GetDisplayName(pItem, SIGDN_FILESYSPATH, @pwszName)
            if pwszName then 
                AllFilenames(i) = *pwszName
                CoTaskMemFree(pwszName)
                pwszName = null
            end if
            pItem->lpVtbl->Release(pItem)
            pItem = null
        end if
    next
    pItems->lpVtbl->Release(pItems)
    
    ' Loop through all file names and create pDoc objects. Load the text into the
    ' buffers and parse it. We do not create scintilla window until the document is
    ' asked to be displayed.
    for i = lbound(AllFilenames) to ubound(AllFilenames)
        if len(AllFilenames(i)) = 0 then continue for

        ' Save the folder where this file was opened from into the gApp.wszLastOpenFolder
        ' variable so that subsequent uses of the File Open dialog will default to this folder.
        if i = lbound(AllFilenames) then 
            gApp.wszLastOpenFolder = AfxStrPathName( "PATH", AllFilenames(i) )
        end if

        ' Test to see if the file is already loaded in the editor. if it is, then
        ' bypass loading it again thereby creating multiple ghost instances.
        dim pDoc as clsDocument ptr
        dim pDocSearch as clsDocument ptr
        
        if bIsTemplate then
            pDoc = gApp.CreateEmptyDocument( true )
            pDoc->LoadDiskFile( AllFilenames(i), true )
        else
            pDocSearch = gApp.GetDocumentPtrByFilename(AllFilenames(i)) 
            if pDocSearch = 0 then
                pDoc = gApp.CreateEmptyDocument()
                pDoc->LoadDiskFile( AllFilenames(i) )
                ' Set type of file based on file extension (used to categorize project files)
                pDoc->SetProjectFileType()
            end if    
        end if
        
        if (bShowInTab = true) andalso (pDoc <> 0) then pDocTabs(i) = pDoc
        if (bShowInTab = true) andalso (pDocSearch <> 0) then pDocTabs(i) = pDocSearch
    next


    ' Loop through all files that are tagged to be displayed in the top tabcontrol.
    ' Files being added to a Project do not get added to the tabs control.
    dim as long iTab = -1
    for i = lbound(pDocTabs) to ubound(pDocTabs)
        if pDocTabs(i) = 0 then continue for
        iTab = gTTabCtl.GetTabIndexByDocumentPtr(pDocTabs(i))
        if iTab = -1 then 
            pDocTabs(i)->AssignTextBuffer()
            ' If this is a Template then set the buffer dirty (Templates would have IsNewFlag)
            if pDocTabs(i)->IsNewFlag then
                pDocTabs(i)->UserModified = true
            end if
            ' Add the new document to the top tabcontrol
            iTab = gTTabCtl.AddTab(pDocTabs(i))  
            UpdateMRUList(pDocTabs(i)->DiskFilename) 
        end if
    next   

    ' Load Explorer names first SetFocusTab will highlight it.
    LoadExplorerFiles()
    LoadFunctionsFiles()
    LoadBookmarksFiles()
    if iTab > -1 then gTTabCtl.SetFocusTab(iTab)

    gApp.IsFileLoading = false

    SetCursor( LoadCursor(0, IDC_ARROW) )
    ResetScintillaCursors()
    frmMain_PositionWindows()
    frmMain_SetFocusToCurrentCodeWindow()
    
    function = 0

end function


' ========================================================================================
' Save the incoming pDoc file to disk
' ========================================================================================
function OnCommand_FileSave( _
            byval hwnd as HWND, _
            byval pDoc as clsDocument ptr, _
            byval bSaveAs as boolean, _
            byval bSaveAll as boolean _
            ) as boolean

    if gApp.GetDocumentCount = 0 then exit function
    
    dim as boolean bReloadExplorer = false
    dim as boolean bReloadFunctions = false
    
    dim as long i, iStart, iEnd

    if bSaveAll then
        ' Save All
        iStart = 0
        iEnd = gTTabCtl.GetItemCount() - 1
    else
        ' Save
        iStart = gTTabCtl.GetTabIndexByDocumentPtr(pDoc)
        iEnd = iStart

        ' A DOCUMENT WITH NO TAB still has to be saveable. The loop below walks tabs, so a
        ' document that has none -- which is most of a project's files, and exactly what the
        ' Find in Project excerpts edit -- produced iStart = -1 and saved nothing at all.
        if (iStart = -1) andalso (pDoc <> 0) then
            if IsWindow( pDoc->hWindow(0) ) then
                dim as boolean bDirty = cbool(SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0)
                if bDirty orelse pDoc->UserModified orelse bSaveAs then
                    if pDoc->SaveFile(bSaveAs) then return true   ' cancelled
                    pDoc->ApplyProperties
                    LoadFunctionsFiles()
                end if
            end if
            return false
        end if
    end if

    for i = iStart to iEnd
        ' Get the document pointer and then save file to disk
        if gTTabCtl.IsValidTab(i) = false then continue for
        pDoc = gTTabCtl.GetpDoc(i)
        if pDoc = null then continue for

        If (cbool(SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0)) orelse _
           (pDoc->UserModified = true) orelse (bSaveAs = true) then   
            ' Do the actual saving to disk
            dim wszFilename as DWSTRING = pDoc->DiskFilename
            if pDoc->SaveFile(bSaveAs) = false then
                pDoc->ApplyProperties
                bReloadFunctions = true
                ' Apply document properties to this file because the file extension may have
                ' changed. for example from Untitled to *.bas
                if pDoc->DiskFilename <> wszFilename then
                    bReloadExplorer = true
                    gApp.ProjectSetFileType( pDoc, pDoc->ProjectFileType )
                end if
            else
                'return true   
                exit for
            end if
        end if    

        ' Ensure that the Tab displays the correct filename in case it was changed
        gTTabCtl.SetTabText(-1)
    next
    
    if bReloadExplorer then LoadExplorerFiles()
    if bReloadFunctions then LoadFunctionsFiles()
    ' We neeed to reload Bookmarks when any of the filenames have changed.
    if bReloadExplorer then LoadBookmarksFiles()

    ' Files changed on disk - refresh the background project-tier scan
    if bReloadFunctions then gScanMgr.RequestProjectScan()

    frmMain_SetFocusToCurrentCodeWindow()

    function = false
end function
 

' ========================================================================================
' Rename the incoming pDoc file to disk
' ========================================================================================
function OnCommand_FileRename( _
            byval hwnd as HWND, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    dim wszFilename_original  as wstring * MAX_PATH
    dim wszFilename_AutoSave  as wstring * MAX_PATH
    dim wszFilename  as wstring * MAX_PATH
    dim wszExtension as wstring * MAX_PATH

    ' Display the Rename Dialog (Save File Dialog)
    wszFilename_original = pDoc->DiskFilename
    dim pwszName as wstring ptr = AfxIFileSaveDialog(HWND_FRMMAIN, @wszFilename_original, @wszExtension, IDM_FILERENAME)
    if pwszName then
        wszFilename = *pwszName
        ' Apply document properties to this file because the file extension may have
        ' changed. for example from Untitled to *.bas
        if pDoc->DiskFilename <> wszFilename then
            gApp.ProjectSetFileType( pDoc, pDoc->ProjectFileType )
        end if
        ' If we are trying to rename and overwrite an existing file (we would have already gotten
        ' the popup message warning to replace the file), then we need to delete that file first 
        ' otherwise the AfxRenameFile will fail.
        if AfxFileExists(wszFilename) then AfxDeleteFile(wszFilename)
        if AfxRenameFile( wszFilename_original, wszFilename ) then
            pDoc->DiskFilename = wszFilename
        end if    

        CoTaskMemFree(pwszName)

    else
        return false
    end if

    LoadExplorerFiles()
    LoadFunctionsFiles()
    LoadBookmarksFiles()

    frmMain_SetFocusToCurrentCodeWindow()

    function = 0
end function


' ========================================================================================
' Duplicate the incoming pDoc file to disk
' ========================================================================================
function OnCommand_FileDuplicate( _
            byval hwnd as HWND, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    dim as DWSTRING wszFilename
    dim as long nextFileNum 

    ' The new duplicated filename will have "-1" appended to the end of the filename.
    ' The appended number will increment until a valid filename is available.
    do
        nextFileNum += 1
        wszFilename = _
            AfxStrPathName( "PATH", pDoc->DiskFilename ) & _
            AfxStrPathName( "NAME", pDoc->DiskFilename ) & _
             "-" & str(nextFileNum) & _
            AfxStrPathName( "EXTN", pDoc->DiskFilename )
        if AfxFileExists( wszFilename ) = false then exit do
    loop    
    
    ' Encode the full Scintilla buffer through the single shared writer (same path as SaveFile).
    dim as long sciCodePage = SciExec(pDoc->hWindow(0), SCI_GETCODEPAGE, 0, 0)   ' 0 or SC_CP_UTF8

    dim as boolean bLossy
    dim as string sOut = Doc_EncodeForDisk( Scintilla_GetTextBytes(pDoc->hWindow(0)), _
                                            (sciCodePage = SC_CP_UTF8), pDoc->FileEncoding, bLossy )

    if bLossy then
        ' pDoc is the SOURCE document, so accepting here also suppresses the prompt on its
        ' own next save -- correct: it is the same text in the same encoding.
        if Doc_ConfirmLossySave( pDoc, wszFilename, pDoc->FileEncoding ) = false then return 0
    end if

    ' The loop above guarantees wszFilename does not exist, so nothing here can overwrite a
    ' file -- but the write can still fail, and opening a duplicate that was never created
    ' put an empty tab on screen named after a file that is not there.
    dim as DWSTRING wszErr
    if Doc_WriteToDisk( wszFilename, sOut, wszErr ) <> DOCWRITE_OK then
        Doc_ReportWriteFailure( wszFilename, wszErr )
        return 0
    end if

    ' Load the duplicate file from disk. This will display it in the top tab control
    ' and the Explorer.
    pDoc = OpenSelectedDocument( wszFilename )
    if pDoc then
        ' Set type of file based on file extension (used to categorize project files)
        pDoc->SetProjectFileType()
        LoadExplorerFiles()
        LoadFunctionsFiles()
        LoadBookmarksFiles()
    end if    

    frmMain_PositionWindows()
    frmMain_SetFocusToCurrentCodeWindow()

    function = 0
end function


' ========================================================================================
' Save all open files to disk
' ========================================================================================
function OnCommand_FileSaveAll( byval hwnd as HWND ) as boolean

    if gTTabCtl.GetItemCount() = 0 then exit function

    dim as boolean bCancelled = OnCommand_FileSave( hwnd, 0, false, true )
    if bCancelled then return true

    ' ...and the Find in Project results, whose files usually have NO TAB for the loop above
    ' to have found. Save All that quietly skipped everything the user had just edited in the
    ' results list would be the worst kind of wrong.
    if FindProject_SaveAllDirty() = false then return true

    return false
end function


' ========================================================================================
' Close current (or all) open file(s)
' ========================================================================================
function OnCommand_FileClose( _
            byval hwnd as HWND, _
            byval veFileClose as eFileClose, _
            byval nTabNum as long = -1 _
            ) as LRESULT

    dim wText as wstring * MAX_PATH  
    dim pDoc as clsDocument ptr
    dim as long r 
    
    ' if a Project is not active then we need to save the current non-project notes
    ' when this file is closed. It is possible that this file is being closed and
    ' a project is being opened.
    ' No notes capture here. frmOutput_NotesChanged writes gApp.ProjectNotes on every
    ' keystroke and ProjectSaveToFile persists it, so there is nothing to flush.

    ' Build a list of files to check
    dim as long nCurSel 
    ' if the incoming nTabNum is -1 then we want to close the current
    ' active document, otherwise close the specified tab.
    nCurSel = iif(nTabNum = -1, gTTabCtl.GetCurSel(), nTabNum)

    dim as long nCount = gTTabCtl.GetItemCount()

    if nCount = 0 then return true
    
    redim bCloseIndex(nCount - 1) as boolean

    for i as long = 0 to nCount - 1 
        SELECT CASE veFileClose
            case EFC_CLOSEALL:         bCloseIndex(i) = true
            case EFC_CLOSECURRENT:     bCloseIndex(i) = iif(i = nCurSel, true, false)
            case EFC_CLOSEALLOTHERS:   bCloseIndex(i) = iif(i <> nCurSel, true, false)
            case EFC_CLOSEALLFORWARD:  bCloseIndex(i) = iif(i > nCurSel, true, false) 
            case EFC_CLOSEALLBACKWARD: bCloseIndex(i) = iif(i < nCurSel, true, false)
        end select
    next
    
    ' Save the pDoc of the current active document in case we need to us it after
    ' the deletes are done to restore the correct tab index.
    dim as clsDocument ptr pDocActive = gTTabCtl.GetpDoc(gTTabCtl.GetCurSel())
    
    ' Must do everything in reverse order because when Tabs are removed
    ' then the indexes would be out of sync if done in ascending order.
    for i as long = nCount - 1 to 0 step -1
        if bCloseIndex(i) = false then continue for

        ' The Find in Project tab has no document, so none of the save-prompt or
        ' Scintilla-hiding work below applies to it. Every close route reaches here -- the
        ' tab's own X goes through gTTabCtl.CloseTab, and shutdown through EFC_CLOSEALL -- so
        ' this is the single place its window is destroyed.
        if gTTabCtl.IsFindTab(i) then
            ' THE RESULTS ARE EDITABLE, so closing this tab can throw away real work. The
            ' files it edited may have no tab of their own -- a project loads every file but
            ' tabs only some -- so nothing else in this loop would ever prompt for them.
            if FindProject_AnyDirty() then
                gTTabCtl.SetFocusTab(i)
                r = TikoMsgBox( hwnd, L(74,"Save current changes?") & wstr(" ") & wText, APPNAME, _
                                TMB_ICON_QUESTION, TMB_YESNOCANCEL )
                if r = IDCANCEL then exit function
                if r = IDYES then
                    if FindProject_SaveAllDirty() = false then exit function  ' a save was cancelled
                end if
            end if

            gTTabCtl.TabFocusLost(i)     ' puts the info strip back, closes the Find bars

            ' DROP THE RESULTS. Closing the tab used to leave the whole model standing, so a
            ' later Ctrl+Shift+F reopened it showing the OLD result set, stale against files
            ' that had since been edited and saved. It also leaked: the model holds one
            ' document reference per result file,
            ' and nothing was giving them back.
            '
            ' BEFORE DestroyWindow, deliberately: Clear asks the surface to unbind the excerpt
            ' views first, and after the window is destroyed that callback is gone.
            FindProject_Clear()

            if IsWindow( HWND_FRMFINDINPROJECT ) then
                DestroyWindow( HWND_FRMFINDINPROJECT )
                HWND_FRMFINDINPROJECT = 0
            end if
            gTTabCtl.RemoveElement(i)
            continue for
        end if

        pDoc = gTTabCtl.GetDocumentPtr(i)
        if pDoc then
            ' A document with no Scintilla windows still has to lose its tab. This used to
            ' be "continue for", which skipped the RemoveElement below and so made such a
            ' tab impossible to close at all -- reachable now that a project's untabbed
            ' documents can be realized on demand.
            if IsWindow(pDoc->hWindow(0)) then
                if cbool(SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0)) orelse _
                    (pDoc->UserModified = true) then

                    gTTabCtl.SetFocusTab(i)
                    r = TikoMsgBox( hwnd, L(74,"Save current changes?") & wstr(" ") & wText, APPNAME, _
                                    TMB_ICON_QUESTION, TMB_YESNOCANCEL )
                    if r = IDCANCEL then exit function
                    if r = IDYES then
                        r = pDoc->SaveFile()
                        if r = true then exit function   ' save was cancelled
                        elseif r = IDNO then
                    end if
                end if

                ' Hide the Scintilla editing windows, and the split bar with them -- this used
                ' to be SetRectEmpty on the document's gripper rect, but the bar is a real
                ' window (owned by frmMain) now.
                for ii as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
                    ShowWindow( pDoc->hWindow(ii), SW_HIDE)
                next
                ShowWindow( HWND_FRMMAIN_SPLITV, SW_HIDE )
                ShowWindow( HWND_FRMMAIN_SPLITH, SW_HIDE )
            end if
        end if

        ' remove tab from PsTabBar
        gTTabCtl.RemoveElement(i)
            
        ' CLOSING A TAB DOES NOT REMOVE THE DOCUMENT FROM THE WORKSPACE. It stays listed in
        ' the Explorer under its category until the user explicitly removes it -- that is
        ' the whole point of there being one workspace model instead of two.
        '
        ' TWO exceptions. A never-saved buffer has no disk file to go back to, so closing
        ' its tab is the only way it can end and keeping it would leave a row pointing at
        ' nothing. And an UNTITLED workspace has no project for a file to be removed from --
        ' "Remove from project" is withheld from both context menus there, so closing has to
        ' be what takes the row out of the Explorer or the file could never leave at all.
        if pDoc then
            ' ALWAYS capture caret/scroll/bookmarks first. The document is about to lose its
            ' Scintilla, and the project writer's untabbed branch persists docData without
            ' refreshing it -- so skipping this here would silently write a stale caret for
            ' every file the user closed during the session.
            pDoc->SetDocInfo()

            dim as boolean bRemoveOnClose = _
                ( pDoc->IsNewFlag = true ) orelse ( gApp.IsProjectNamed() = false )

            if bRemoveOnClose then
                gApp.RemoveDocument( pDoc )
                LoadExplorerFiles()
                ' The Functions panel lists the Explorer's files, so a document leaving the
                ' workspace has to drop out of both.
                LoadFunctionsFiles()
            else
                ' Still in the workspace, but no longer in a tab: refresh the Explorer so
                ' the row repaints without its open-document styling.
                LoadExplorerFiles()
            end if
        end if
    next
     
    ' Set the active tab to the closest tab to the one just removed if it was the
    ' current active tab that was removed. PsTabBar deliberately deselects (-1) when
    ' the current tab is deleted, so that is the signal that the active tab closed
    ' (and that pDocActive may now point to a freed document).
    if gTTabCtl.GetCurSel() = -1 then
        dim as long idx = nCurSel - 1
        if idx < 0 then idx = 0
        if gTTabCtl.IsValidTab( idx ) then
            gTTabCtl.SetFocusTab( idx )
        end if
    else
        ' The active tab survived the close; its index may have shifted, so refocus
        ' it by document pointer (guaranteed alive on this branch).
        gTTabCtl.SetTabIndexByDocumentPtr(pDocActive)
    end if
        
    frmMain_PositionWindows()
    
    ' important to repaint the workspace to remove any separator bar and splitter
    ' rectangle should all documents be closed. Removes screen artifacts.
    AfxRedrawWindow( HWND_FRMMAIN )
    
    function = true
end function


' ========================================================================================
' Message received from Explorer popup menu
' ========================================================================================
function OnCommand_FileExplorerMessage( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
    
    if id = IDM_FILEOPEN_EXPLORERLISTBOX then
        OpenSelectedDocument( pDoc->DiskFilename )

    elseif id = IDM_FILESAVE_EXPLORERLISTBOX then
        OnCommand_FileSave( HWND_FRMMAIN, pDoc, false )

    elseif id = IDM_FILESAVEAS_EXPLORERLISTBOX then
        OnCommand_FileSave( HWND_FRMMAIN, pDoc, true )

    elseif id = IDM_FILERENAME_EXPLORERLISTBOX then
        OnCommand_FileRename( HWND_FRMMAIN, pDoc )

    elseif id = IDM_FILEDUPLICATE_EXPLORERLISTBOX then
        OnCommand_FileDuplicate( HWND_FRMMAIN, pDoc )

    elseif IDM_FILECLOSE_EXPLORERLISTBOX then
        dim as long nCurSel = gTTabCtl.GetTabIndexByDocumentPtr( pDoc )
        if nCurSel <> -1 then
            OnCommand_FileClose( HWND_FRMMAIN, EFC_CLOSECURRENT, nCurSel )
        end if   
    end if   

    function = 0
end function


        
