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

'  frmOptionsLocal.bi
'
'  The Localization page. Like Compiler Setup, this is a panel-hosted page (controls on the
'  dialog's page container, driven by build/layout/paint/show hooks) rather than a child form.
'  The file-editing business logic is unchanged from the old page -- only the controls were
'  swapped for the reusable family and the "current file" store moved from a groupbox caption
'  to a module variable.
'
'  THE PAGE IS A LIST OF INSTALLED LANGUAGES plus a command stack beside it, and -- while a
'  language is being edited -- a phrase list whose THIRD column is edited in place. There is no
'  file-open dialog on the Select path any more (the languages list is the picker) and there are
'  no English / translation textboxes (the third column replaced them).

#pragma once

' SCP_PAINTINFO is named in the declares below; this header is pulled in early.
#include once "PsScrollPanel.bi"

' CMDNEW / CMDLOCALIZATION keep their values: AfxIFileSaveDialog / AfxIFileOpenDialogW
' switch on them (by name -- see modRoutines.inc) to pick the *.lang filter and title.
#Define IDC_FRMOPTIONSLOCAL_CMDNEW                  1002
#Define IDC_FRMOPTIONSLOCAL_CMDEDIT                 1003
#Define IDC_FRMOPTIONSLOCAL_CMDDELETE              1004
#Define IDC_FRMOPTIONSLOCAL_CMDLOCALIZATION         1006
#Define IDC_FRMOPTIONSLOCAL_LSTPHRASES              1008
#Define IDC_FRMOPTIONSLOCAL_LSTLANGUAGES            1014
#Define IDC_FRMOPTIONSLOCAL_CMDCLOSE                1016

' Called from frmOptions_SaveEditorOptions on OK, and internally before switching files.
declare function frmOptionsLocal_LocalEditCheck() as long

' The current localization filename the page is pointed at (e.g. "english.lang"). Replaces
' the old FRAMELOCALIZATION groupbox caption; read by SaveEditorOptions to update
' gConfig.LocalizationFile. Seeded when the dialog opens.
extern gLocalCurrentFile as DWSTRING

' TRUE once the SELECTED language's own file has been rewritten during this dialog session --
' the only editing that the running application would notice, and so the only editing that
' warrants the "changes apply next run" notice. Read by frmOptions_SaveEditorOptions.
extern gLocalSelectedEdited as boolean

declare sub      frmOptionsLocal_SeedFromConfig()
declare function frmOptionsLocal_OwnsPage( byval nPage as long ) as boolean
declare sub      frmOptionsLocal_Reset()
declare sub      frmOptionsLocal_Build( byval hPage as HWND )
declare function frmOptionsLocal_Layout( byval hPage as HWND, byval cxPanel as long ) as long
declare sub      frmOptionsLocal_Paint( byval p as SCP_PAINTINFO ptr )
declare sub      frmOptionsLocal_Show( byval bShow as boolean )
declare sub      frmOptionsLocal_ApplyTheme()
