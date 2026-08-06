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

'  modLocalization.bi  --  the localized phrase tables and the L() macro.
'
'  MOVED OUT OF modDeclares.bi, the shell's grab-bag header, because localization is an
'  APP-LAYER concern and every app-layer file with user-facing text needs it. It is the
'  first thing modMenuDefinitions.inc reaches for, and modDeclares.bi is full of HWNDs.
'
'  The four declarations are one concept and share one size, so they moved together: the
'  live table, the English fallback, the staging copy the translation editor edits, and the
'  flag that says the editor is open.
'
'  WHAT DID NOT MOVE: LoadLocalizationFile, which fills these. It reads files and lives in
'  modRoutines.inc, shell-side, and there is no reason to drag it across yet -- the app layer
'  needs to READ the table, not to load it.

#pragma once

'  DWSTRING, NOT `wstring * MAX_PATH`.
'
'  The tables were fixed-width buffers of 260 characters each -- MAX_PATH, a WIN32 constant,
'  which is how the array holding every translated string in the program came to be
'  undeclarable without windows.bi. That a phrase is bounded by the length of a file path was
'  an accident of whatever was in scope when it was written.
'
'  As DWSTRING there is no bound at all, so nothing truncates at 260 in any of the six
'  languages, and the storage is one pointer per entry rather than 520 bytes whether the
'  phrase is "OK" or a sentence. It is also the type the rest of the port is converging on.

'  The live table: the language currently loaded. Resized and filled by
'  LoadLocalizationFile, which reads a MAXIMUM: line from the .lang file and redims to it.
'
'  IT IS INDEXED WITHOUT BOUNDS CHECKING -- see L() below.
redim shared LL(any) as DWSTRING

'  The English phrases, always loaded. When a localization is loaded, any missing translation
'  is replaced with the English one, so a partial .lang file renders as English rather than
'  as blanks.
redim shared gLangEnglish(any) as DWSTRING

'  The staging copy frmOptionsLocal edits, and the flag that says it is open. Edits go here
'  and are committed on OK, so Cancel costs nothing and the running UI never renders a
'  half-edited language.
redim shared gLocalPhrases(any) as DWSTRING
dim shared gLocalPhrasesEdit as boolean

'  L(id, "english default") -- returns the localized phrase for id.
'
'  THE SECOND ARGUMENT IS DISCARDED. It is documentation at the call site and nothing else,
'  so an id with no entry in english.lang renders as an EMPTY STRING and the default written
'  right beside it is never used. Every new id must be added to all six .lang files.
'
'  AND IT DOES NOT BOUNDS-CHECK, because it is a raw array index. An id past ubound(LL) reads
'  whatever follows the array -- it does not fault and does not render blank, it renders
'  garbage that differs between runs and between builds. That is not hypothetical: two format
'  self-tests asserted ids 593-669 against a table sized to 521 and were nondeterministic for
'  it, which cost two wrong conclusions during the 7d port before it was found.
'
'  All 748 real call sites are within bounds -- checked. The macro is left as a plain index
'  rather than made safe because making it a function would put a DWSTRING copy on every one
'  of those sites; the bounds test belongs in code that computes an id rather than writes one.
#Define L(e,s) LL(e)
