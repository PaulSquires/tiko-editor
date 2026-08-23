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

'  frmOptionsCompiler.bi
'
'  The Compiler Setup page. Unlike the old version this is NOT a child CWindow: its
'  controls are built directly onto the Options scroll panel's page window and
'  driven by the same hooks the table-driven pages use (build / layout / paint / show /
'  theme). It reads and writes gOptWork, never gConfig, so Cancel is a no-op and the page
'  can be built lazily on first visit.
'
'  The controls are the reusable family: PsListTree (toolchains), PsTextBox x3 (switches,
'  include paths, help file), PsToggle x2 (run-via-cmd, disable-beep) and PsButton (browse).

#pragma once

' SCP_PAINTINFO is named in the declares below, and this header is pulled in early (by
' modRoutines.inc) before PsScrollPanel.bi would otherwise be seen.
#include once "PsControls\PsScrollPanel.bi"

' 9213 and 9214 are RETIRED, not reused: they were the FreeBASIC help file textbox and its
' browse button. The setting they edited was never read by anything -- the Help Center
' replaced the .chm -- so the controls, the gConfig field and the file dialog's *.chm case
' are all gone. Leaving the numbers vacant keeps a stale settings.ini or a stale id in some
' other switch from landing on a live control.
#Define IDC_FRMOPTIONSCOMPILER_LSTTOOLCHAINS          9210
#Define IDC_FRMOPTIONSCOMPILER_TXTFBSWITCHES          9211
#Define IDC_FRMOPTIONSCOMPILER_TXTINCLUDES            9212
#Define IDC_FRMOPTIONSCOMPILER_CHKRUNVIACOMMANDWINDOW 9215
#Define IDC_FRMOPTIONSCOMPILER_CHKDISABLECOMPILEBEEP  9216

declare function frmOptionsCompiler_OwnsPage( byval nPage as long ) as boolean
declare sub      frmOptionsCompiler_Reset()
declare sub      frmOptionsCompiler_Build( byval hPage as HWND )
declare function frmOptionsCompiler_Layout( byval hPage as HWND, byval cxPanel as long ) as long
declare sub      frmOptionsCompiler_Paint( byval p as SCP_PAINTINFO ptr )
declare sub      frmOptionsCompiler_Show( byval bShow as boolean )
declare sub      frmOptionsCompiler_ApplyTheme()
