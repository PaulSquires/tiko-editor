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


''  HANDLE RAW INCOMING MESSAGES IN THE MESSAGE PUMP

#include once "modFindReplace.inc"


function handleMouseShowScrollBar( byval uMsg as MSG ) as boolean
    if (gApp.IsProjectLoading) orelse (gApp.IsFileLoading) then return false

    ' (The menubar's hover/highlight housekeeping moved into the CMenuBar control.)

    ' Should we show an Editor HScrollbar
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if (pDoc <> 0) andalso (pDoc->bSizing = false) then   ' no testing during split panel resizing
        ' If the xOffset has changed then push the fresh range into the horizontal
        ' scrollbars (SetRange inside re-derives the thumb and repaints).
        dim as long GetXOffset = SciExec( pDoc->hWndActiveScintilla, SCI_GETXOFFSET, 0, 0 )
        if pDoc->lastXOffsetPos <> GetXOffset then
            pDoc->lastXOffsetPos = GetXOffset
            frmEditorHScroll_UpdateScrollBars( pDoc )
        end if
        frmEditorHScroll_DoNeedScrollBar( pDoc )
    end if
    
    ' we want our mouse message to continue so return FALSE
    return false
end function


' (handleMouseTopMenu is gone: outside-click dismissal of the topmenu now lives in
'  CMenuBar_FilterMessage, called from frmMain's message pump.)


function handleAltKeyMenuBar( byval uMsg as MSG ) as boolean
    ' 2022-01-08: skip processing the Alt key here because it seems to
    ' interfere with the Alt+Mouse column editing in Scintilla. Need
    ' to investigate a better alternative. The following testing for 
    ' the mouse button does work, but it still results in the menubar
    ' being selected/deselected.

    ' Handle the Split editor window icon in the toptab control
    if IsWindowVisible(HWND_FRMMAIN_TOPTABS) then
        dim as DWSTRING wszNewSplitButton
        ' Alt key is currently down
        if (GetAsyncKeyState(VK_MENU) and &H8000) then
            wszNewSplitButton = wszIconSplitTopBottom
        else    
            wszNewSplitButton = wszIconSplitLeftRight
        end if    
        if wszIconSplitEditor <> wszNewSplitButton then
            wszIconSplitEditor = wszNewSplitButton
            AfxRedrawWindow(HWND_FRMMAIN_TOPTABS)
        end if    
    end if
    
'   ' single ALT key pressed, activate the first top menubar button
'    if (uMsg.message = WM_SYSKEYDOWN) and (uMsg.wParam = VK_MENU) then
'      killPopupSubMenus()
'      killPopupMenus()
'      ' if active menubar exists then simply toggle it off and exit
'      dim as HWND hCtrl 
'      if gActiveMenuBarIndex then
'         hCtrl = gActiveMenuBarIndex
'         gActiveMenuBarIndex = 0
'         AfxRedrawWindow(hCtrl)
'         return true
'      else   
'         hCtrl = GetDlgItem(HWND_FRMMAIN_MENUBAR, IDC_MENUBAR_FILE)
'         gActiveMenuBarIndex = hCtrl
'         AfxRedrawWindow(hCtrl)
'         return true
'      end if   
'   end if
    ' we want our message to continue so return FALSE
    return false
end function




function handleEscKeyModeless( byval uMsg as MSG ) as boolean
    ' Close any open Find/Replace window with ESC key unless it
    ' is the Project Search tab which always displays the Find pane
    ' but can toggle on/off the Replace with hot key or icon press.
    if (uMsg.message = WM_KEYDOWN) andAlso (uMsg.wParam = VK_ESCAPE) then
        dim as long idx = gTTabCtl.GetCurSel()
        if gTTabCtl.IsValidTab(idx) then
'            if gTTabCtl.tabs(idx).docType <> DocumentType.ProjectSearch then
                if (gFind.bShowFindPanel = true) orelse (gFind.bShowReplacePanel = true) then
                    FindReplaceControls_Hide()
                    return true
                end if
'            end if    
            ' When ESC is pressed it should also ensure to unhighlight any search occurrences.
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then pDoc->RemoveSelectionAttributes()
        end if
    end if
    
    ' we want our message to continue so return FALSE
    return false
end function


function handleKeysFindReplace( byval uMsg as MSG ) as boolean
    ' For Find textbox, an ENTER will move to the next selection. A SHIFT+ENTER
    ' will move to the previous. For Replace textbox, use the same logic but 
    ' also perform the replace action.
    ' The pDoc can be null for Project Search tabs so allow the function to 
    ' continue and do explicit pDoc null checks.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()

    ' The find/replace textboxes are CTextBox controls: keyboard focus sits on their
    ' RichEdit CHILD, so compare (and SetFocus) against that handle, not the control's.
    dim as HWND hFocus   = GetFocus()
    dim as HWND hFind    = CTextBox_GetRichEditHandle( GetDlgItem( HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND ) )
    dim as HWND hReplace = CTextBox_GetRichEditHandle( GetDlgItem( HWND_FRMMAIN_TOPTABSREPLACE, IDC_FRMTOPTABSREPLACE_TXTREPLACE ) )
    dim as HWND hEdit    
    if pDoc then hEdit = pDoc->hWndActiveScintilla

    if (hFocus <> hFind) andalso (hFocus <> hReplace) then return false
    
    if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_RETURN) then
        if (GetAsyncKeyState(VK_CONTROL) and &H8000) andalso _
                (GetAsyncKeyState(VK_MENU) and &H8000) then
            ' Ctrl+Alt+Enter  Replace All
            SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEALL, 0), 0 )
            return true
        end if
        if (GetAsyncKeyState(VK_SHIFT) and &H8000) then
            if hFocus = hFind then
                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDPREV, 0), 0 )
            elseif GetFocus = hReplace then
                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEPREV, 0), 0 )
            end if   
            return true
        else   
            if hFocus = hFind then
                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDNEXT, 0), 0 )
            elseif GetFocus = hReplace then
                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACENEXT, 0), 0 )
            end if
            return true
        end if   

    elseif (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_TAB) then
        ' Move between the Find Replace textboxes and the Edit window.
        dim as HWND hNewFocus
        if (GetAsyncKeyState(VK_SHIFT) and &H8000) then 
            if (hFocus = hFind)    then hNewFocus = hEdit
            if (hFocus = hReplace) then hNewFocus = hFind
        else    
            if (hFocus = hFind)    then hNewFocus = hReplace
            if IsWindowVisible(hReplace) = 0 then hNewFocus = hEdit
            if (hFocus = hReplace) then hNewFocus = hEdit
        end if
        SetFocus( hNewFocus )
        return true
    end if
    
    ' we want our message to continue so return FALSE
    return false
end function


'function handleScintillaENTER( byval uMsg as MSG ) as boolean
'    ' Catch any ENTER key destined for the Scintilla editor and beautify the
'    ' current text line. We replace the line with our new code and then allow
'    ' the ENTER key to continue to pass on to Scintilla where it will do the
'    ' insertion of the ENTER character. Scintilla will then fire SCN_CHARADDED
'    ' where we do additional things like AutoIndent.
'    dim pDoc as clsDocument ptr = gApp.GetDocumentPtrByWindow( uMsg.HWnd )
'    if pDoc = 0 then return false

'    dim as HWND hEdit = pDoc->hWndActiveScintilla   

'    if (uMsg.HWnd = hEdit) andalso (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_RETURN) then
'        dim as long nCurLine = pDoc->GetCurrentLineNumber()
'        dim as string strLine = pDoc->GetLine(nCurLine)  
        
        
'        ' CALL YOUR FORMATTING ROUTINE HERE AND RETURN THE NEW LINE
'        ' Beautify the current line 
        
'        'dim as string strNewLine ='AddSpacesAroundOperators( strLine )
        
        
'        ' Replace the old line with our new line (don't use pDoc->SetLine)
'        dim as long lineStart = SendMessage( hEdit, SCI_POSITIONFROMLINE, nCurLine, 0)
'        dim as long lineEnd = SendMessage( hEdit, SCI_GETLINEENDPOSITION, nCurLine, 0)
'        SendMessage( hEdit, SCI_SETSELECTIONSTART, lineStart, 0) 
'        SendMessage( hEdit, SCI_SETSELECTIONEND, lineEnd, 0)
'        SendMessage( hEdit, SCI_REPLACESEL, 0, cast(LPARAM, strptr(strNewLine)) )

'    end if 
    
'     ' we want our message to continue so return FALSE
'    return false
'end function
   