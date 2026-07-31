'    tiko
'    Copyright (C) 2024-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify it under the
'    terms of the GNU General Public License as published by the Free Software Foundation,
'    either version 3 of the License, or (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful, but WITHOUT ANY
'    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
'    PARTICULAR PURPOSE.  See the GNU General Public License for more details.

#pragma once

' ========================================================================================
' frmOutputFloat -- the Output panel's FLOATING (undocked) frame.
'
' The panel can be detached from the bottom of frmMain into a resizable, non-modal window
' of its own, and returns to its docked position when that window's caption X is clicked.
'
' HOW IT WORKS, AND THE ONE THING TO KNOW: the panel window is REPARENTED, never destroyed
' and recreated. frmOutputFloat_Undock calls SetParent( HWND_FRMOUTPUT, the frame ) and
' _Dock calls SetParent( HWND_FRMOUTPUT, HWND_FRMMAIN ). Because no control is destroyed,
' every pane's content survives the move -- the compiler results, the log text, the TODO
' list, the Notes buffer, scroll positions and selection. Recreating the panel instead
' would empty all of it and mean re-seeding the Notes from the project on every undock.
'
' WS_CHILD STAYS ON IN BOTH STATES. The panel goes from being a child of frmMain to being a
' child of the frame -- child to child -- and SetParent alone is correct for that. Clearing
' WS_CHILD is only needed when a window becomes top-level ITSELF, which this design
' deliberately avoids. A self-test assertion pins the invariant so a later "tidy-up" that
' adds the style surgery turns the suite red rather than breaking things quietly.
'
' The frame is a WS_POPUP OWNED BY frmMain with WS_EX_TOOLWINDOW: it therefore has no
' taskbar button, always floats above the editor, and is destroyed with it.
'
' There is no pump obligation and no frmOutputFloat_FilterMessage, deliberately -- see the
' block comment on frmMain's IsDialogMessage call. Keystrokes typed into the floating panel
' MUST still reach frmMain's accelerator tables, because that is how Ctrl+C and friends are
' routed (by GetFocus(), which does not care which top-level window owns the focused
' control). A filter of frmHelpCenter's shape would break exactly that.
'
' This header names no Ps* type, so it can be included by clsConfig.inc for the
' frmOutput_CaptureState guard, ahead of the whole Ps* block.
' ========================================================================================

' TRUE while the panel is undocked. IsWindow on the frame is the SINGLE source of truth --
' there is deliberately no parallel boolean that could disagree with it.
declare function frmOutputFloat_IsFloating() as boolean

' Detach the panel into a floating frame / return it to the bottom of frmMain.
' Both are no-ops when already in the requested state.
declare sub frmOutputFloat_Undock()
declare sub frmOutputFloat_Dock()
declare sub frmOutputFloat_Toggle()

' Refresh gConfig.OutputFloat* from the live frame. Called from frmOutput_CaptureState,
' which SaveConfigFile calls -- so every save path persists a coherent set, the same rule
' the docked fields follow.
declare sub frmOutputFloat_CaptureState()

' Show / hide the floating frame. Dock state and VISIBLE state are orthogonal: View>Output
' toggles visibility wherever the panel happens to live, and never re-docks it.
declare sub frmOutputFloat_SetVisible( byval bVisible as boolean )

' The window the View menu's checkmark and the visibility toggles should be asking about --
' the frame while floating, the panel itself while docked.
declare function frmOutputFloat_VisibilityTarget() as HWND

' Re-point the frame's class background brush at the current theme. Called from
' frmOutput_SetControlColors, which is the theme hook -- the class brush is the one colour
' in this window that is not read live at paint time, so it is the one that must be pushed.
declare sub frmOutputFloat_SyncBrush()

declare sub frmOutputFloat_RunSelfTest()
