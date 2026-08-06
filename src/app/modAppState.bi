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

'  modAppState.bi  --  two pieces of application state that are not the shell's.
'
'  "Is the debugger running?" and "is there an update?" were fields in clsApp -- a 51-field
'  flag bag that also holds window handles, which is what made it shell-side. Neither fact
'  has anything to do with a window: one is owned by the debug engine and the other by the
'  update-check worker, and both are read by MENUS, which is app-layer code.
'
'  SAME MOVE, SAME REASON, AS gApp.IsCompiling -> modBuildService. That one is written up at
'  length in modBuildService.bi; this is the second and third field to leave for the same
'  reason, and the pattern is deliberately identical so there is one shape to recognise.
'
'  NOT a test-and-set, unlike BuildService_Begin. Neither of these guards anything -- they are
'  reported state, set by whoever owns the activity and read by whoever draws it. Making them
'  claim-style would imply a mutual exclusion that does not exist.

#pragma once

declare function AppState_IsDebuggerActive() as boolean
declare sub      AppState_SetDebuggerActive( byval bActive as boolean )

declare function AppState_IsUpdateAvailable() as boolean
declare sub      AppState_SetUpdateAvailable( byval bAvailable as boolean )
