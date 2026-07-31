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

#define IDC_FRMOUTPUT_TABS                          1000
#define IDC_FRMOUTPUT_LVRESULTS                     1001
#define IDC_FRMOUTPUT_TXTLOGFILE                    1002
' 1003 was IDC_FRMOUTPUT_LVSEARCH (the Search Results pane). Deliberately left as a hole
' rather than closed up: these are control ids, so reusing 1003 gains nothing and renaming
' the survivors would churn every GetDlgItem call site for no benefit.
#define IDC_FRMOUTPUT_LVTODO                        1004
#define IDC_FRMOUTPUT_TXTNOTES                      1005
#define IDC_FRMOUTPUT_BTNCLOSE                      1006
#define IDC_FRMOUTPUT_VSCROLL                       1007
#define IDC_FRMOUTPUT_BTNUNDOCK                     1008

' The tab strip is a PsSelectBar plus a one-item PsIconPanel for the "X", sized side by side
' to cover the strip. There is no container behind them: each paints its own background, so
' the old subclassed LABEL (and its ~150 lines of rect arithmetic, hit-testing, hover
' tracking and painting) is gone rather than merely bypassed.
'
' The CURRENT TAB lives in the PsSelectBar -- there is deliberately no gOutputTabsCurSel
' global any more. Read it with frmOutput_GetCurTab and set it with frmOutput_SetCurTab
' (silent: only user clicks fire the change callback, so calling it from a handler cannot
' recurse). gConfig.ShowOutputPanelIndex is the persisted copy, applied to the control once
' it exists.
'
' A tab's IDENTITY IS ITS PANEL ID, NOT ITS PANEL INDEX, and the two no longer agree.
' These ARE the values persisted in the config file, so their numbering must not change --
' which is why removing the Search Results tab left a HOLE at 2 instead of renumbering.
' settings.ini carries no version key, so a renumber could not tell an old stored 3 (TODO)
' from a new stored 3 (Notes): every user whose last tab was TODO would silently land on
' Notes, with nothing to detect it. PsSelectBar_AddPanel already carries a host id per
' panel, so the id is what is stored and the index is derived. Always convert through
' frmOutput_GetCurTab / frmOutput_SetCurTab (or PsSelectBar_FindPanelByID / GetPanelID)
' rather than comparing a raw GetCurSel index against one of these.
#define OUTPUT_TAB_RESULTS    0
#define OUTPUT_TAB_LOGFILE    1
' 2 was OUTPUT_TAB_SEARCH. Retired -- search results now live in the Find in Project tab.
#define OUTPUT_TAB_TODO       3
#define OUTPUT_TAB_NOTES      4

declare sub      frmOutput_TextBoxScrollChanged( byval hTextBox as HWND )
declare sub      frmOutput_SetTabCaptions()
declare function frmOutput_ShowNotes() as long
declare function frmOutput_UpdateToDoListview() as long
' Current tab as an OUTPUT_TAB_* ID, and the setter that takes one. The index<->id
' conversion lives in exactly these two places -- see the note on the constants above.
' GetCurTab answers OUTPUT_TAB_RESULTS when the bar does not exist yet.
declare function frmOutput_GetCurTab() as long
declare function frmOutput_SetCurTab( byval nTabID as long ) as boolean
declare function frmOutput_ShowHideOutputControls( byval hwnd as HWND ) As LRESULT
declare function frmOutput_PositionWindows() as LRESULT
declare function frmOutput_Show( byval hWndParent as HWND ) as LRESULT
declare function frmOutput_ResetAllControls() as long 
declare function frmOutput_RestorePanel() as long
' Refresh every persisted Output-panel field from the live windows. Called by
' SaveConfigFile, so it runs on ALL save paths rather than only on the way out -- see the
' comment block on those fields in clsConfig.bi. Safe before the panel exists.
declare sub      frmOutput_CaptureState()
' Clamp a stored (unscaled) panel height into the legal range. Pure, so the self-test can
' drive it; also applied on load so an existing settings.ini carrying a ratcheted or
' divided value self-heals on first run.
declare function frmOutput_ClampHeight( byval nHeight as long ) as long
' Point the undock icon at whichever direction is currently available, and show or hide the
' "X" beside it. Called on every dock-state change. Safe before the controls exist.
' NOTE: frmOutput_IconPanelMessage is deliberately NOT declared here. This header is pulled
' in by clsConfig.inc, far ahead of the Ps* block, so it may name no Ps* type -- and that
' callback takes a PSICONPANEL_MESSAGEINFO. It is defined in frmOutput.inc ahead of its only
' caller, exactly as frmOutput_SelectBarMessage and frmOutput_ClosePaint are.
declare sub      frmOutput_UpdateDockIcon()
declare sub      frmOutput_RunSelfTest()
' Second half of the suite, run AFTER frmOutput_Show. It exists because the pure half is
' structurally blind to the defect this change fixes: reinstating the scale-up inside
' frmOutput_Show left the pure half completely green, so the only assertion that can catch
' it has to observe the field ACROSS the call. gOutPanelHeightAtShow is the witness.
declare sub      frmOutput_RunPostShowSelfTest()
dim shared gOutPanelHeightAtShow as long
' Forward declared: the splitter's message callback is defined above them.
declare function frmOutput_MinimizePanel() as long

