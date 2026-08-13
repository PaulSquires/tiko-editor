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

'  app/modKeyBindings.bi  --  the keyboard shortcut DATA, with no vocabulary in it.
'
'  ---- WHY THIS IS SPLIT FROM src/modKeyBindings.bi -------------------------------------
'
'  That file already carried the model apart from the dialog, and its header explains why:
'  frmMain builds the accelerator table at startup and modMenuDefinitions reads gKeys on
'  every menu open, neither of which has anything to do with the Keyboard Shortcuts window.
'
'  This is the SECOND cut through the same material, along a different line: what is DATA
'  from what has to ask WINDOWS. The 112 default bindings, the array they live in and the
'  record type are strings and integers; the name->virtual-key vocabulary is not, because
'  frmKeyboard_OEMtoVirtKey resolves punctuation through VkKeyScanEx against the LIVE
'  keyboard layout. Only the first half can exist below the shell.
'
'  ---- WHAT NEEDED IT ------------------------------------------------------------------
'
'  Phase 7c's shell binary. getMenuAccelText walks gKeys to put "Ctrl+S" beside a menu row,
'  and modMenuDefinitions is app-layer -- so with the array declared in the shell, every
'  menu in the new shell rendered with a blank accelerator column and nothing said why.
'
'  The same array now feeds a PsAccelTable, which is what actually makes the shortcut fire.
'  PsAccel's vocabulary is PHYSICAL (PsKey is a position, not a character) where
'  frmKeyboard_AccelKeyToValue's is layout-dependent; the two disagree on any non-US
'  keyboard, and that divergence is recorded in docs/port/d2-decision.md rather than
'  papered over here.
'
'  FUNCTION NAMES ARE UNCHANGED by the move, exactly as they were unchanged by the first
'  one. The file they live in changed; nothing that calls them did.

#pragma once


' ========================================================================================
' One command that can carry a keyboard shortcut.
'
' idAction is the IDM_* value; wszMsgString is its NAME ("IDM_FILESAVE"), and that is what
' keybindings.ini stores -- renumbering an IDM_ constant must not invalidate a user's file.
' ========================================================================================
type KEYBINDINGS_TYPE
    idAction         as long         ' IDM_* message
    wszMsgString     as DWSTRING     ' "IDM_SAVE", "IDM_SAVEAS", etc
    wszCategory      as DWSTRING
    wszDescription   as DWSTRING
    wszDefaultKeys   as DWSTRING
    wszUserKeys      as DWSTRING
    bDefaultDisabled as boolean = false
end type

' The live bindings. Read by modMenuDefinitions (menu accelerator text), by
' frmKeyboard_BuildAcceleratorTable in the shell, and by tikoshell's PsAccel table.
' The dialog edits a STAGING COPY and commits here.
dim shared gKeys(any) as KEYBINDINGS_TYPE

declare function frmKeyBoard_AddKeyBinding( byval wszCategory as DWSTRING, _
                                            byval idAction as long, _
                                            byval wszMsgString as DWSTRING, _
                                            byval wszDescription as DWSTRING, _
                                            byval wszDefaultKeys as DWSTRING ) as long
declare function frmKeyBoard_AddKeyBinding_User( byval wszMsgString as DWSTRING, _
                                                 byval wszUserKeys as DWSTRING, _
                                                 byval bDisabled as boolean ) as long
declare function frmKeyboard_CreateDefaultKeyBindings() as long
declare function frmKeyboard_SaveKeyBindings( byval wszFilename as DWSTRING ) as long

' ========================================================================================
' THE ACCELERATOR KEY VOCABULARY -- the names a user may bind to.
'
' MOVED DOWN IN 7c STEP 10, and the reason is one include line. app/modMenuDefinitions.inc
' carried
'
'     #include once "../modKeyBindings.bi"
'
' -- an app/ file reaching UP into the shell BY RELATIVE PATH, which no token scan can see
' and which is why _check_shell reads #include lines instead of grepping for Afx and Win32
' names. It wanted one thing: to ask whether a stored key name is one the pick list offers.
'
' IT ASKED THE WRONG FUNCTION. It called KeyBindings_PickListKeyToValue and tested for 0 --
' and that function's own header says, in as many words: "MEMBERSHIP IN gKeyNames() is the
' validity test here, never 'the return is non-zero'". The non-zero return is a VK_*, and a
' VK_* is precisely what app/ may not name.
'
' So the membership test moved instead, and the vocabulary with it. It is a list of
' STRINGS; nothing about it was ever platform-specific. What stayed in the shell is
' everything that turns a name into a virtual key -- AccelKeyToValue, OEMtoVirtKey,
' PickListKeyToValue and VKToName -- because that is the half that needs a keyboard layout.
' ========================================================================================
type KEYNAME_TYPE
    wszName as DWSTRING
end type

dim shared gKeyNames(any) as KEYNAME_TYPE

' Idempotent and safe to call from anywhere, which matters more now than it did: its
' definition and some of its callers are in different files and different layers.
declare sub KeyBindings_EnsureKeyNames()

' Is this one of the names the pick list offers? The membership test, without resolving to
' a virtual key -- which is what lets it live here at all.
declare function KeyBindings_IsPickListName( byval wszName as DWSTRING ) as boolean
