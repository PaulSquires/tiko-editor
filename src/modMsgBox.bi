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

'  modMsgBox.bi  --  ONE themed message box for the whole application. Declarations only.
'
'  WHAT THIS REPLACES
'    Fifteen raw Win32 MessageBox calls, spread over eleven files, which were the last
'    unthemed windows tiko put on screen. They came up in system grey beside a fully themed
'    editor, and they were the only dialogs that ignored the theme entirely.
'
'  WHY A SHARED FUNCTION AND NOT ANOTHER HAND-ROLLED BUILDER
'    Nine dialogs had grown their own private ThemedMsgBox -- frmBuildConfig, frmHelpCenter,
'    frmOptions, frmThemes (x2), frmUserTools, frmMainSearch, modFileWatch (x2) -- each ~30 lines
'    of the same nine MBX_COLORS assignments, the same OptionsTheme_FillButton, and the same
'    create-a-22pt-glyph-font-and-free-it dance. All nine are folded onto this function, so
'    there is now exactly ONE place that knows what a tiko message box looks like.
'
'    THAT FOLD FIXED THREE SILENT BUGS, all the same mistake: PsMessageBox_SetDefaultButton
'    takes a button INDEX and three of those builders passed it an ID (IDCANCEL, IDOK, IDYES).
'    The setter REJECTS an out-of-range index by exiting quietly (PsMessageBox.inc), so
'    frmThemes' "default to Cancel" and its Save/Discard/Cancel prompt's "default to Save" never
'    took effect and nothing reported it. An index is what this function takes, and it is
'    validated against the button count it just built.
'
'  THE .bi / .inc SPLIT IS FORCED, NOT TIDY
'    modCompile.inc is included at tiko.bas:115 and PsMessageBox.inc at :154, so modCompile
'    may name no Ps* type. Same trap as frmAutoComplete, frmHelpCenter and modFileWatch. Hence
'    the TMB_* enums below: they are tiko's own vocabulary, mapped onto MBX_ICON_* / IDOK et al
'    inside the .inc, so this header stays free of the control's identifiers and can be included
'    early enough for every caller to see it.
'
'  IT MUST WORK BEFORE THE APPLICATION EXISTS
'    Four of the fifteen calls are in tiko.bas and fire during startup -- two of them BEFORE the
'    localization file loads and all four before frmMain_Show creates ghFont() or reads a theme.
'    So the implementation falls back to the system message font and GetSysColor when tiko's own
'    font cache and palette are not there yet, and those four call sites pass TMB_ICON_NONE
'    because SegoeFluentIcons.ttf is not loaded either (one of them is the box reporting that it
'    could not be). A box that cannot be read is worse than an ugly one.
'
'  WE OWN THE BUTTON WORDS NOW
'    The Win32 MessageBox got OS-localized buttons for free. PsMessageBox_AddPreset's text is
'    hardcoded English (PsMessageBox.inc), so the buttons are localized here through L() -- with
'    an English literal fallback for the pre-localization case above, which is the one place
'    L() legitimately returns nothing.

#pragma once


' Which glyph the box shows. Mapped onto MBX_ICON_* in the .inc.
'
' TMB_ICON_NONE is a real layout state, not "no artwork available" -- the icon cell and its gap
' collapse and the text takes the full body width.
enum TMB_ICON
    TMB_ICON_NONE = 0
    TMB_ICON_INFO
    TMB_ICON_WARNING
    TMB_ICON_ERROR
    TMB_ICON_QUESTION
end enum


' Which buttons, and therefore what the return value can be. The cancel id (Escape / the X /
' Alt+F4) is the rightmost button, or IDOK for a lone [OK].
'
' The button WORDS are localized here rather than through PsMessageBox_AddPreset, whose text is
' hardcoded English -- so these are deliberately not a 1:1 map onto MBX_BTN_*.
enum TMB_BUTTONS
    TMB_OK = 0                  ' [OK]                      -> IDOK
    TMB_OKCANCEL                ' [OK] [Cancel]             -> IDOK / IDCANCEL
    TMB_YESNO                   ' [Yes] [No]                -> IDYES / IDNO
    TMB_YESNOCANCEL             ' [Yes] [No] [Cancel]       -> IDYES / IDNO / IDCANCEL
    TMB_SAVE_DISCARD_CANCEL     ' [Save] [Discard] [Cancel] -> IDYES / IDNO / IDCANCEL
end enum


' Show a themed modal message box and return the dismissing id (IDOK / IDCANCEL / IDYES / IDNO).
'
' hParent may be 0 -- the startup boxes have no window to own them, and PsMessageBox clamps a
' parentless box to the work area of the monitor holding the cursor.
'
' BLOCKS: PsMessageBox runs its own nested GetMessage loop, so this returns only once the user
' has answered. Calling it from a dialog that is itself modal is fine and is what frmUserTools
' and frmBuildConfig already do -- but pass THAT dialog as hParent, never HWND_FRMMAIN: a box
' owned by a disabled ancestor comes up behind the dialog that asked for it.
'
' nDefaultIndex is a zero-based button INDEX, not an id. It is clamped to the buttons actually
' built, so an out-of-range value lands on the first button rather than being dropped -- which is
' the failure the three folded-in builders shipped with. Pass 1 for the
' guarding-a-destructive-action case where Cancel should be the default.
'
' hTextFont of 0 resolves to tiko's GUIFONT_11, or to the system message font when the cache does
' not exist yet. Pass one explicitly only when the caller has a reason -- the two Options-family
' dialogs pass OptionsFont_Base(). The caption, icon and close glyphs are all resolved from it.
declare function TikoMsgBox( byval hParent      as HWND, _
                             byval wszText      as DWSTRING, _
                             byval wszCaption   as DWSTRING, _
                             byval nIcon        as long = TMB_ICON_INFO, _
                             byval nButtons     as long = TMB_OK, _
                             byval nDefaultIndex as long = 0, _
                             byval hTextFont    as HFONT = 0 ) as long
