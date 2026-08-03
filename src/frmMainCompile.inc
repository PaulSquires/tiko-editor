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
' Compile common actions
' ========================================================================================
function OnCommand_CompileCommon( byval id as long ) as LRESULT

    select case id
        case IDM_BUILDEXECUTE, IDM_COMPILE, IDM_REBUILDALL, IDM_QUICKRUN, IDM_COMPILEMODULE
            code_Compile( id )
        
        case IDM_RUNEXE
            RunExe( 0, _
                GetRunExecutableFilename, _
                gApp.ProjectCommandLine _
                )
        
        case IDM_COMMANDLINE
            ' The command-line specifics live here now; frmInputBox is generic. Seed it with
            ' the active command line, and write the result back only on OK -- Cancel returns
            ' the (possibly edited) text too, but the IDCANCEL says to disregard it.
            dim as DWSTRING wszText
            wszText = gApp.ProjectCommandLine
            if frmInputBox_Show( HWND_FRMMAIN, _
                        L(142,"Command Line"), L(143,"Enter command line arguments:"), _
                        wszText ) = IDOK then
                gApp.ProjectCommandLine = wszText
            end if
    end select
    
    function = 0
end function

