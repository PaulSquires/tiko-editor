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
#include once "frmDebug.bi"
#include once "clsDocument.bi"


' ========================================================================================
' Search action Common actions
' ========================================================================================
function OnCommand_DebugCommon( byval id as long ) as LRESULT
            
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id
        case IDM_DEBUG_STARTDEBUGGING
            ' Start and Continue share one menu item, whose caption changes with the state.
            ' They used to share one CASE as well -- listed twice, and FreeBASIC takes the
            ' first match -- so Continue recompiled and opened a second debugger instead of
            ' resuming. Only the toolbar button ever worked.
            if AppState_IsDebuggerActive() then
                frmDebug_Command( IDM_DEBUG_STARTDEBUGGING )
            else
                code_Compile(id)
            end if

        case IDM_DEBUG_STOPDEBUGGING, IDM_DEBUG_PAUSE, IDM_DEBUG_STEPINTO, _
             IDM_DEBUG_STEPOVER, IDM_DEBUG_STEPOUT, IDM_DEBUG_RUNTOCURSOR
            frmDebug_Command( id )

        case IDM_DEBUG_UNUSEDSYMBOLS
            frmUnusedSymbols_Show( HWND_FRMMAIN )

        case IDM_DEBUG_TOGGLEBREAKPOINT
            pDoc->ToggleBreakPoint( pDoc->GetCurrentLineNumber )
            
        case IDM_DEBUG_DELETEALLBREAKPOINTS
            if TikoMsgBox( HWND_FRMMAIN, L(247,"Do you want to delete all breakpoints?"), L(214,"Confirm"), _
                        TMB_ICON_QUESTION, TMB_YESNOCANCEL ) <> IDYES then
                return true
            end if            
            dim pDoc as clsDocument ptr = gApp.pDocList
            do until pDoc = 0
                hEdit = DocView(pDoc, 0)
                SciExec( hEdit, SCI_MARKERDELETEALL, MARKER_BREAKPOINT, 0 )
                pDoc = pDoc->pDocNext
            loop

    end select
    
    function = 0
end function

