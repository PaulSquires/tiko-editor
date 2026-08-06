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

'  See modAppState.bi for why these two left clsApp.

#pragma once

' Set by frmDebug as the debug session starts and stops. Nothing outside the debugger writes
' it; the menus and the context menu only read it.
dim shared as boolean gDebuggerActive

' Set once by the update-check worker when the server reports a newer version. It resolves
' the Update item's caption in the menu bar and nothing else.
dim shared as boolean gUpdateAvailable

function AppState_IsDebuggerActive() as boolean
    return gDebuggerActive
end function

sub AppState_SetDebuggerActive( byval bActive as boolean )
    gDebuggerActive = bActive
end sub

function AppState_IsUpdateAvailable() as boolean
    return gUpdateAvailable
end function

sub AppState_SetUpdateAvailable( byval bAvailable as boolean )
    gUpdateAvailable = bAvailable
end sub
