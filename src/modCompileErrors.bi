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

#pragma once

declare function SetDocumentErrorPosition( byval hLV as HWnd ) as long
declare function SetLogFileTextbox( byval pCompile as COMPILE_TYPE ptr ) as long
declare function ParseLogForError( _
            byval pCompile as COMPILE_TYPE ptr, _
            byref wsLogSt as DWSTRING, _
            byval bAllowSuccessMessage as boolean, _
            byval fCompilingObjFiles as boolean _
            ) as boolean
declare function ResetScintillaCursors() as long
declare function RunEXE( byval pDocMain as clsDocument ptr, byref wszFileExe as DWSTRING, byref wszParam as DWSTRING ) as long
declare function SetCompileStatusBarMessage(byref wszText as wstring, byval hIconCompile as long) as LRESULT
declare function RedirConsoleToFile(byval wszExe as DWSTRING, byval wszCmdLine as DWSTRING, byref sConsoleText as string ) as long
' Compose lpCommandLine: quoted exe as argv[0], then the switches. See its own header --
' the first token is ALWAYS eaten by the child, so switches cannot be passed alone.
declare function BuildChildCommandLine( byval wszExe as DWSTRING, byval wszParams as DWSTRING ) as DWSTRING
declare sub CompileCmd_RunSelfTest()
declare function CreateTempResourceFile() as boolean

