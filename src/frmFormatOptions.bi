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

' ========================================================================================
' FORMAT OPTIONS dialog -- declarations.
'
' Deliberately names NO Ps* type, the frmBuildConfig / frmProjectOptions rule: every handle
' is a plain HWND and the callback typedefs are declared privately in the .inc. That is what
' lets this header sit early if it ever needs to, and it costs nothing to keep.
'
' This dialog is SELF-CONTAINED and deliberately does NOT reuse modOptionsRows' table. That
' machinery is welded to gOptWork (its rows hold `pField as any ptr` pointing into that one
' struct, and OptionsWork_Load/Commit marshal it), and the Environment Options dialog it
' serves is declared feature-complete. Widening it to carry a second dialog's staging copy
' would be a restructure of working code for the sake of a few saved lines. What IS reused
' is the part with no coupling: the OptionsTheme_Fill* colour helpers, so a toggle here and
' a toggle there cannot drift apart.
' ========================================================================================

#pragma once

' ---- control ids -----------------------------------------------------------------------
#define IDC_FRMFORMATOPTIONS_NAV          100
#define IDC_FRMFORMATOPTIONS_PREVIEW      101
#define IDC_FRMFORMATOPTIONS_CMDOK        102
#define IDC_FRMFORMATOPTIONS_CMDCANCEL    103
#define IDC_FRMFORMATOPTIONS_CMDRESET     104
' The rule rows are allocated a contiguous block; a row's control id is FIRSTROW + its index.
#define IDC_FRMFORMATOPTIONS_FIRSTROW     200

' ---- pages -------------------------------------------------------------------------------
' 1-based on purpose. These are stored as a PsListTree row's itemData, and PsListTree_AddNode
' defaults itemData to ZERO -- so a page id of 0 is indistinguishable from "nobody set one".
' A sentinel must not be a value the domain can produce. (Learnings.md, the debugger's
' "<click to add a watch>" row.)
#define FMTPAGE_NONE          0
#define FMTPAGE_INDENT        1
#define FMTPAGE_CASING        2
#define FMTPAGE_SPACING       3
#define FMTPAGE_BLANKLINES    4
#define FMTPAGE_TRIGGERS      5
#define FMTPAGE_COUNT         5

' ---- layout, in UNSCALED units (AfxScaleX/Y applied at point of use only) ----------------
#define FRMFORMATOPTIONS_CLIENT_W    900
#define FRMFORMATOPTIONS_CLIENT_H    620
#define FRMFORMATOPTIONS_NAV_W       176
#define FRMFORMATOPTIONS_PREVIEW_W   288
#define FRMFORMATOPTIONS_TITLE_H      52
#define FRMFORMATOPTIONS_FOOTER_H     58
#define FRMFORMATOPTIONS_MARGIN       16
' Tall enough for the tallest control a row can hold, which is decided by the CONTROL's
' ideal height (font-dependent), not by this number. Kept comfortably above it rather than
' trimmed to fit -- a row exactly as tall as its control has no visual separation at all.
#define FRMFORMATOPTIONS_ROW_H        38
#define FRMFORMATOPTIONS_NAVROW_H     30
#define FRMFORMATOPTIONS_LABELGAP     12
' Control widths are PER KIND, not one column width for all three.
'
' A single width meant every label stopped short of the widest control on the page even when
' its own row held a 44px toggle -- so "Reindent code from block structure" ellipsized against
' whitespace a combo two rows down was not using. The label span is now measured from the
' control that is actually on THAT row.
#define FRMFORMATOPTIONS_COMBO_W     120
#define FRMFORMATOPTIONS_NUMERIC_W    90
#define FRMFORMATOPTIONS_TOGGLE_W     44
#define FRMFORMATOPTIONS_BUTTON_W     92
#define FRMFORMATOPTIONS_BUTTON_H     30

' ---- window handles ----------------------------------------------------------------------
dim shared HWND_FRMFORMATOPTIONS      as HWND
dim shared HWND_FRMFORMATOPTIONS_NAV  as HWND

declare function frmFormatOptions_Show( byval hWndParent as HWND ) as LRESULT
' Env-gated (TIKO_FORMAT_SELFTEST=1). Two halves, the second measuring real windows -- so it
' only runs when the dialog is actually opened. See the suite headers for what each cannot see.
declare sub frmFormatOptions_RunSelfTest( byval bLayoutHalf as boolean )
