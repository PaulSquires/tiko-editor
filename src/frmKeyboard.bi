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

' The MODEL (gKeys, the defaults table, the ini reader/writer, the accelerator build and
' the key vocabulary) moved to modKeyBindings.* -- frmMain builds the accelerator table at
' startup and modMenuDefinitions reads gKeys on every menu open, and neither has anything
' to do with this dialog. Included here so the existing includers of frmKeyboard.bi
' (modMenuDefinitions.inc, frmKeyboardEdit.inc) still see gKeys without changing.
#include once "modKeyBindings.bi"


#DEFINE IDC_FRMKEYBOARD_LIST1                1000
#DEFINE IDC_FRMKEYBOARD_CMDMODIFY            1001
#DEFINE IDC_FRMKEYBOARD_CMDCLEAR             1002
#DEFINE IDC_FRMKEYBOARD_CMDOK                1004
#DEFINE IDC_FRMKEYBOARD_CMDCANCEL            1005
#DEFINE IDC_FRMKEYBOARD_FILTER               1006

dim shared HWND_FRMKEYBOARD_LIST   as HWND   ' PsListTree, 4 columns
dim shared HWND_FRMKEYBOARD_FILTER as HWND   ' PsTextBox -- filters across all four columns


' ========================================================================================
' THE STAGING COPY -- what makes Cancel mean anything.
'
' The dialog used to edit the live gKeys() in place and its only button ("Close") SAVED, so
' there was no way to back out of an experiment. Every edit now writes gKeysWork() and
' nothing else; OK commits, and Cancel / Escape / the X simply drop it. gKeys() is untouched
' until commit, which also means the top menus (modMenuDefinitions reads gKeys on every
' popup open) keep showing the old bindings for as long as the dialog is open.
' ========================================================================================
dim shared gKeysWork(any) as KEYBINDINGS_TYPE

' Parallel to gKeysWork(): TRUE where this command's effective binding is also some other
' command's. Recomputed wholesale by frmKeyboard_RescanConflicts after every edit; the row
' painter only reads it. Kept beside the array rather than inside KEYBINDINGS_TYPE because it
' is a derived view of the whole set, not a property of one binding, and storing it in the
' record would invite it being saved to disk.
dim shared gKeysConflict(any) as boolean

' The filter box's live text. Empty shows every row.
dim shared gKeysFilter as DWSTRING


' Shell metrics, UNSCALED -- run through AfxScaleX/AfxScaleY at use, exactly as FRMOPTIONS_*.
' THE CLIENT SIZE IS FIXED (no WS_THICKFRAME), which is what makes the list viewport one
' number rather than "whatever the user dragged the window to":
'
'     list viewport = (CLIENT_W - 2*MARGIN) x (CLIENT_H - TITLE_H - FOOTER_H)
'                   = (900 - 32)            x (620 - 52 - 58)
'                   =  868                  x  510          , unscaled
#Define FRMKEYBOARD_CLIENT_W                         900
#Define FRMKEYBOARD_CLIENT_H                         620
#Define FRMKEYBOARD_TITLE_H                           52
#Define FRMKEYBOARD_FOOTER_H                          58
#Define FRMKEYBOARD_MARGIN                            16
#Define FRMKEYBOARD_BTN_W                             92
#Define FRMKEYBOARD_BTN_H                             32
#Define FRMKEYBOARD_FILTER_W                         320
#Define FRMKEYBOARD_FILTER_H                          40
#Define FRMKEYBOARD_FILTER_GLYPHW                     34
#Define FRMKEYBOARD_ROW_H                             24

' Column widths, unscaled. Description (index 1) is the fill column and absorbs the slack.
#Define FRMKEYBOARD_COL_CATEGORY                     110
#Define FRMKEYBOARD_COL_DESCRIPTION                  330
#Define FRMKEYBOARD_COL_DEFAULTKEYS                  170
#Define FRMKEYBOARD_COL_USERKEYS                     170
                                                

declare function frmKeyboard_Show( byval hWndParent as HWND ) as LRESULT

' Describe the first command whose effective binding equals wszKeys, skipping nSkipIndex.
' Returns "" when there is no clash. RETURNS THE TEXT rather than writing it into a label --
' the edit dialog draws its own conflict line, and a shared setter would have to know about
' two windows.
declare function frmKeyboard_CheckForKeyConflict ( byval wszKeys as DWSTRING, byval nSkipIndex as long ) as DWSTRING
' Recompute gKeysConflict() for the whole staging copy.
declare sub      frmKeyboard_RescanConflicts()
' What the Default Keys cell SHOWS for a row: the keys, plus a "(disabled)" suffix when the
' default is suppressed. Composed on demand from the model rather than stored, so the cell
' text stays the raw keys and the filter and the painter cannot disagree about it.
declare function frmKeyboard_DefaultKeysDisplay( byval idx as long ) as DWSTRING

' Seed the staging copy from the live bindings. Called once, before any control exists.
declare sub      frmKeyboard_LoadWork()
' Commit the staging copy: gKeys, then DISK, then the accelerator table -- in that order.
declare sub      frmKeyboard_CommitWork()
' TIKO_KEYBOARD_SELFTEST=1. Staging semantics and the on-disk round trip. Commits nothing
' and writes only to a temp file, so it is safe to run at startup.
declare sub      frmKeyboard_RunSelfTest()

