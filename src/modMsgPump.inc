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

    ' (The menubar's hover/highlight housekeeping moved into the PsMenuBar control.)

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
'  PsMenuBar_FilterMessage, called from frmMain's message pump.)


function handleAltKeyMenuBar( byval uMsg as MSG ) as boolean
    ' 2022-01-08: skip processing the Alt key here because it seems to
    ' interfere with the Alt+Mouse column editing in Scintilla. Need
    ' to investigate a better alternative. The following testing for 
    ' the mouse button does work, but it still results in the menubar
    ' being selected/deselected.

    ' (The Alt-key swap of the split-editor icon is gone: that button no longer picks an
    '  axis from the icon it happens to be showing -- it opens a menu offering both.
    '  See ShowEditorSplitContextMenu in modContextMenus.inc.)

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
        ' ON THE FIND IN PROJECT TAB THE FIND BAR IS NOT DISMISSABLE. The find phrase is that
        ' tab's whole subject, and closing the bar would leave a results tab with no way to
        ' type into it. Escape there closes the REPLACE bar if it is open -- so it still
        ' toggles the tab back from replace mode to find-only -- and is otherwise inert.
        if frmFindInProject_IsActive() then
            if gFind.bShowReplacePanel then
                gFind.bProjectReplaceActive = false
                FindReplace_SetVisible( true, false )
                return true
            end if
            return false
        end if

        ' THE PANEL TEST STANDS ALONE, ahead of any tab test. FindControls_Show runs with a
        ' null pDoc for Project Search, so "a bar is open" and "the current tab is valid"
        ' are independent facts -- gating the close on the second left Escape dead in
        ' exactly the state the first was reachable in.
        if (gFind.bShowFindPanel = true) orelse (gFind.bShowReplacePanel = true) then
            FindReplaceControls_Hide()
            return true
        end if

        ' When ESC is pressed it should also ensure to unhighlight any search occurrences.
        ' This half still wants a valid tab: it acts on the active document.
        dim as long idx = gTTabCtl.GetCurSel()
        if gTTabCtl.IsValidTab(idx) then
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then pDoc->RemoveSelectionAttributes()
        end if
    end if
    
    ' we want our message to continue so return FALSE
    return false
end function


' ========================================================================================
' Ctrl+Space: raise the word autocomplete list on demand.
'
' The ONLY keyboard trigger for AUTOCOMPLETE_WORD. Ordinary typing deliberately never opens
' a popup (see the "case else" in the SCN_CHARADDED handler in frmMainOnNotify.inc), and the
' space key lands in AUTOCOMPLETE_DIM_AS, so before this the word list was reachable only by
' pasting.
'
' The mode is named HERE rather than left to ShowAutocompleteList to infer: the elseif chain
' that guesses a mode runs only while AutoCompleteType is NONE, and it demands two already-
' typed characters before it will guess AUTOCOMPLETE_WORD. An explicit request should not
' have to meet that bar, and setting the type up front makes the chain skip itself.
'
' Claimed rather than passed on, so TranslateMessage never runs for it and no WM_CHAR is
' generated -- an unclaimed Ctrl+Space would otherwise reach the editor as a NUL.
' ========================================================================================
function handleCtrlSpaceAutoComplete( byval uMsg as MSG ) as boolean
    if uMsg.message <> WM_KEYDOWN then return false
    if uMsg.wParam <> VK_SPACE then return false
    if (GetKeyState( VK_CONTROL ) and &h8000) = 0 then return false
    ' Ctrl+Alt+Space is AltGr on several layouts and belongs to the editor.
    if (GetKeyState( VK_MENU ) and &h8000) <> 0 then return false
    if gConfig.AutoComplete = false then return false

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return false
    ' Only when the keystroke is on its way to the editor itself -- the Find box and the
    ' Notes pane are ordinary text fields and keep their Ctrl+Space.
    if uMsg.hwnd <> pDoc->hWndActiveScintilla then return false

    ' Already showing: the list is filtering itself as the user types, so there is nothing
    ' to rebuild. Still claimed, so the key does not fall through to the editor.
    if frmAutoComplete_IsOpen() then return true

    pDoc->AutoCompleteType = AUTOCOMPLETE_WORD
    ShowAutocompleteList()
    return true
end function


function handleKeysFindReplace( byval uMsg as MSG ) as boolean
    ' For Find textbox, an ENTER will move to the next selection. A SHIFT+ENTER
    ' will move to the previous. For Replace textbox, use the same logic but 
    ' also perform the replace action.
    ' The pDoc can be null for Project Search tabs so allow the function to 
    ' continue and do explicit pDoc null checks.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()

    ' The find/replace textboxes are PsTextBox controls: keyboard focus sits on their
    ' RichEdit CHILD, so compare (and SetFocus) against that handle, not the control's.
    dim as HWND hFocus   = GetFocus()
    dim as HWND hFind    = PsTextBox_GetRichEditHandle( GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND ) )
    dim as HWND hReplace = PsTextBox_GetRichEditHandle( GetDlgItem( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE ) )
    dim as HWND hEdit
    if pDoc then hEdit = pDoc->hWndActiveScintilla

    ' The Find in Project tab holds no document, so hEdit would be 0 and Tab past the bars
    ' had nowhere to go. Its stand-in is the first excerpt view in the results list.
    if hEdit = 0 then hEdit = frmFindInProject_FirstExcerptWindow()

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
        ' Move between the Find/Replace textboxes and the code window:
        '   Find -> Replace (only when the Replace bar is open) -> Scintilla, and back.
        '
        ' The Replace test is on the CONTROL's visibility, walking up to the bar window
        ' that frmMain_PositionWindows shows and hides -- when Replace is closed, Tab from
        ' Find goes straight to the editor.
        dim as boolean bShift     = cbool( (GetAsyncKeyState(VK_SHIFT) and &H8000) <> 0 )
        dim as boolean bReplaceOn = cbool( IsWindow(hReplace) <> 0 )
        if bReplaceOn then bReplaceOn = cbool( IsWindowVisible(hReplace) <> 0 )
        dim as HWND hNewFocus

        if bShift then
            if hFocus = hFind then
                hNewFocus = hEdit
            elseif hFocus = hReplace then
                hNewFocus = hFind
            end if
        else
            if hFocus = hFind then
                if bReplaceOn then hNewFocus = hReplace else hNewFocus = hEdit
            elseif hFocus = hReplace then
                hNewFocus = hEdit
            end if
        end if

        ' hEdit is 0 on the Find in Project tab -- that tab holds no document. SetFocus(0)
        ' would take the focus off EVERY window, so the keystroke reads as "Tab did
        ' nothing" and the caret is gone as well. Consume the key and stay put instead.
        if IsWindow(hNewFocus) then SetFocus( hNewFocus )
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
   