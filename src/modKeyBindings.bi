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

'  modKeyBindings.bi  --  the keyboard shortcut MODEL, with no UI in it.
'
'  This is everything frmKeyboard.inc used to own besides the dialog itself: the binding
'  array, the defaults table, the keybindings.ini reader/writer, the accelerator-table
'  build, and the string<->virtual-key vocabulary. It lives apart from the dialog because
'  frmMain calls frmKeyboard_BuildAcceleratorTable at STARTUP and modMenuDefinitions reads
'  gKeys on every menu open -- neither has anything to do with the Keyboard Shortcuts window.
'
'  FUNCTION NAMES ARE UNCHANGED by the move (frmKeyboard_* throughout). The file they live
'  in changed; nothing that calls them did.
'
'  THE VOCABULARY IS ONE TABLE NOW. It used to exist twice and agree only by convention --
'  as the case arms of frmKeyboard_AccelKeyToValue (name -> VK) and as the hardcoded
'  AddShortcutsToComboBox list (the names a user may pick). They did NOT agree: "PageUp" and
'  "PageDn" were offered by the combobox and understood by nothing, so picking either
'  produced a binding that saved, displayed, and silently never fired. gKeyNames() is now
'  the single list, and KeyBindings_VKToName is DERIVED from AccelKeyToValue rather than
'  written out again -- so a third disagreement is not reachable.

#pragma once


' KEYBINDINGS_TYPE and gKeys MOVED to app/modKeyBindings.bi, which this now includes.
' They are strings and integers; the vocabulary below is not, because its punctuation arms
' resolve through VkKeyScanEx against the live keyboard layout. Phase 7c's shell binary
' needed the data half and cannot have the other -- see that header.
#include once "app/modKeyBindings.bi"

' The single entry being edited by frmKeyboardEdit, copied in and out by frmKeyboard.
dim shared gKeysEdit as KEYBINDINGS_TYPE


' ========================================================================================
' THE ACCELERATOR KEY VOCABULARY -- every name a shortcut string may use for its final key.
'
' Order is the order the pick-lists show, and it is also what makes a name CANONICAL:
' several names map to the same virtual key ("+" and "Plus" both give VK_OEM_PLUS), and
' KeyBindings_VKToName returns whichever appears first here.
'
' Built lazily by KeyBindings_EnsureKeyNames -- no startup ordering to get wrong.
' ========================================================================================
' KEYNAME_TYPE and gKeyNames() MOVED to app/modKeyBindings.bi in 7c step 10, with
' KeyBindings_EnsureKeyNames that fills them. The vocabulary is a list of STRINGS and
' names no Win32; what stays here is everything that turns one of those strings into a
' VK_*, which is the part that does.


' SaveKeyBindings, AddKeyBinding, AddKeyBinding_User and CreateDefaultKeyBindings are
' declared in app/modKeyBindings.bi, included above, beside the array they fill.
declare function frmKeyboard_BuildAcceleratorTable() as long

declare function frmKeyboard_VirtKeyToValue( byval wszString as DWSTRING ) as long
declare function frmKeyboard_OEMtoVirtKey( byval ch as ubyte ) as long
declare function frmKeyboard_AccelKeyToValue( byval wszString as DWSTRING ) as long

' The vocabulary itself is declared in app/modKeyBindings.bi since 7c step 10 -- along
' with EnsureKeyNames, which used to be declared here. Everything below turns one of
' those names into a VIRTUAL KEY, and that is the half this file keeps.
' Virtual key -> its canonical name, or "" when this key has no name in the vocabulary.
' An unnamed key CANNOT be persisted or turned back into an accelerator, so "" is the
' signal to refuse it rather than to store something that will never fire.
declare function KeyBindings_VKToName( byval vk as long ) as DWSTRING
' Fill a pick-list from the vocabulary. Used by frmKeyboardEdit, frmBuildConfig and
' frmUserTools (the last two on raw Win32 comboboxes -- do not change this signature).
' ========================================================================================
' WHICH LIVE EDITOR COMMAND OWNS THIS KEYSTROKE?
'
' Returns the description of the first command in gKeys() whose EFFECTIVE binding equals
' wszKeys, or "" when the keystroke is free. wszKeys must be in the canonical form
' frmKeyboardEdit_BuildKeyString produces -- Ctrl, then Alt, then Shift -- because the
' comparison is verbatim against what the bindings file stores.
'
' DELIBERATELY OVER gKeys(), NOT gKeysWork(). frmKeyboard_CheckForKeyConflict is the staging
' equivalent and exists only while the Keyboard Shortcuts dialog is open; this one answers for
' the bindings the editor is ACTUALLY running on, which is what a caller outside that dialog
' (frmAssignKey) needs. The two are not interchangeable and neither should be "unified" into
' the other.
'
' A disabled default is not a binding: it fires nothing, so it clashes with nothing.
' ========================================================================================
declare function KeyBindings_FindCommandForKeys( byval wszKeys as DWSTRING ) as DWSTRING

' A BARE key name straight out of that pick-list -> its virtual key, or 0 for "no key".
' This is what frmBuildConfig and frmUserTools resolve their stored key with; they keep the
' modifiers in booleans and never build a "Ctrl+X" string, so AccelKeyToValue is the WRONG
' entry point for them -- see the comment on the implementation for the two ways it silently
' produces a wrong accelerator rather than none.
declare function KeyBindings_PickListKeyToValue( byval wszString as DWSTRING ) as long

' TIKO_KEYBOARD_SELFTEST=1. Asserts the vocabulary; needs no window and no message pump,
' so it runs at startup rather than when the dialog opens.
declare sub      KeyBindings_RunSelfTest()
