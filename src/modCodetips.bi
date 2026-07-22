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

' The type of autocomplete popup that is active. This is necessary
' because the autocomplete popup list is rebuilt every time a new
' character is entered.
enum
    AUTOCOMPLETE_NONE   = 0
    AUTOCOMPLETE_DIM_AS
    AUTOCOMPLETE_TYPE
    AUTOCOMPLETE_WORD
end enum

' Autocomplete popup lists are capped - a list longer than this is useless to
' scroll, and the dedupe cost grows quadratically with list length.
const AUTOCOMPLETE_MAX_ITEMS = 1000

' The reason ShowAutocompleteList was called. A rebuild triggered by a DELETION (the
' user backspaced while the popup was up) must not reset the sticky AutoCompleteType or
' clear the match word, and must not cancel the popup when the rebuild comes up empty.
' This was Scintilla's SCN_AUTOCCHARDELETED before the popup became ours.
const AUTOCOMPLETE_NOTIFY_NONE        = 0
const AUTOCOMPLETE_NOTIFY_CHARDELETED = 1

declare function DereferenceLine( byval pDoc as clsDocument ptr, _
            byval sTrigger as string, byval nPosition as long ) as SYMBOLREF
