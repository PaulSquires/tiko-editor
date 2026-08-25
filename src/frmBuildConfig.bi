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

' ========================================================================================
' NOTHING IN THIS FILE MAY NAME A Ps* TYPE -- same rule, same reason, as frmUserTools.bi.
'
' This header is pulled in by clsConfig.inc (its line 23), which the unity build reaches at
' tiko.bas:87 -- ahead of every custom-control include. HWNDs, DWSTRINGs, longs and
' TYPE_BUILDS only. Break it and the errors surface in clsConfig.inc, an unrelated file,
' which reads as an unrelated breakage.
'
' The .inc, by contrast, is included LATE (after modOptionsRows.inc and frmAssignKey.inc)
' because it dresses its controls through OptionsTheme_Fill* and opens the shortcut dialog.
' That split is what lets the five exports below stay reachable from clsConfig, clsDocument,
' frmMain and frmProjectOptions while the implementation sits at the bottom of the build.
' ========================================================================================


' ---- control ids -----------------------------------------------------------------------
#Define IDC_FRMBUILDCONFIG_LIST              1000    ' PsListTree, 3 columns
#Define IDC_FRMBUILDCONFIG_CMDADD            1001
#Define IDC_FRMBUILDCONFIG_CMDDELETE         1002
#Define IDC_FRMBUILDCONFIG_CMDUP             1003
#Define IDC_FRMBUILDCONFIG_CMDDOWN           1004
#Define IDC_FRMBUILDCONFIG_SELECTBAR         1005
#Define IDC_FRMBUILDCONFIG_TXTDESCRIPTION    1006
#Define IDC_FRMBUILDCONFIG_TOGDEFAULT        1007
#Define IDC_FRMBUILDCONFIG_COMBOTARGET       1008
#Define IDC_FRMBUILDCONFIG_CMDSHORTCUT       1009
#Define IDC_FRMBUILDCONFIG_TOGEXCLUDEPOPUP   1010
#Define IDC_FRMBUILDCONFIG_CMDEDITSWITCHES   1011
#Define IDC_FRMBUILDCONFIG_TXTOPTIONS        1012
#Define IDC_FRMBUILDCONFIG_LSTSWITCHES       1013
#Define IDC_FRMBUILDCONFIG_CMDOK             1029
#Define IDC_FRMBUILDCONFIG_CMDCANCEL         1030

' ---- the two pages of the right pane ---------------------------------------------------
' PANEL IDS, not indices. PsSelectBar_FindPanelByID / _GetPanelID address these, so the two
' panels can be reordered without touching a single comparison.
#Define IDP_FRMBUILDCONFIG_GENERAL             1
#Define IDP_FRMBUILDCONFIG_SWITCHES            2

' ---- compiler target ---------------------------------------------------------------------
' The COMBO INDEX. What is persisted is the Is32bit/Is64bit pair -- two separate longs in
' TYPE_BUILDS, written complementarily on every change. Do not collapse them into one field.
#Define BUILDTARGET_32                         0
#Define BUILDTARGET_64                         1

' ---- shell metrics, UNSCALED (AfxScaleX/Y at the point of use) --------------------------
' The client is FIXED -- no WS_THICKFRAME -- which is what makes each viewport ONE NUMBER
' rather than "whatever the user dragged the window to":
'
'     list       = LIST_W x (CLIENT_H - TITLE_H - FOOTER_H)      = 320 x 510
'     right pane = CLIENT_W - MARGIN*2 - LIST_W - DETAIL_GAP     = 528 wide
'     page       = 528 x (510 - SELECTBAR_H)                     = 528 x 470
'
' NEITHER PAGE SCROLLS. A row pushed past 470 is silently CLIPPED -- no scrollbar appears --
' and only above 100% DPI where nobody is looking. That is the same trade frmOptions,
' frmKeyboard and frmUserTools make, and the reason each of them carries a fit check.
#Define FRMBUILDCONFIG_CLIENT_W              720
#Define FRMBUILDCONFIG_CLIENT_H              432
#Define FRMBUILDCONFIG_TITLE_H                44
#Define FRMBUILDCONFIG_FOOTER_H               48
#Define FRMBUILDCONFIG_MARGIN                 16
#Define FRMBUILDCONFIG_BTN_W                  92
#Define FRMBUILDCONFIG_BTN_H                  32
#Define FRMBUILDCONFIG_GAP                     8

' ---- the list column, and its command strip in the title band ---------------------------
#Define FRMBUILDCONFIG_LIST_W                320
#Define FRMBUILDCONFIG_ROW_H                  24
#Define FRMBUILDCONFIG_CMD_W                  88     ' Add / Delete
#Define FRMBUILDCONFIG_GLYPH_W                32     ' the two chevrons
' Description is the FILL column and takes whatever the other two leave.
#Define FRMBUILDCONFIG_COL_SHORTCUT           92
#Define FRMBUILDCONFIG_COL_DEFAULT            64     ' UNCAPTIONED, like the Localization
                                                     ' page's "Active" column

' ---- the right pane ---------------------------------------------------------------------
#Define FRMBUILDCONFIG_DETAIL_GAP             20
#Define FRMBUILDCONFIG_SELECTBAR_H            32
' The gap BETWEEN select-bar panels. The bar's left padding is zeroed so its first panel's
' text lines up with the labels beneath it, which means this is the only spacing left.
#Define FRMBUILDCONFIG_SELECTBAR_PANELGAP     28
' ---- the General page's label/control rows -----------------------------------------------
' THERE IS NO LABEL-WIDTH CONSTANT, deliberately. A fixed gutter ellipsized the longest
' caption ("Exclude build from status bar po...") while leaving whitespace beside the short
' ones. Each label instead runs from the left edge of the pane to the left edge of ITS OWN
' row's control, less one gap -- so a long caption borrows the room a small toggle is not
' using. The rects are derived in PositionWindows, which is the only place that knows where
' each control actually landed, and the painter reads them back; see gBCRowLabelRect.
#Define BCROW_DEFAULT                          0
#Define BCROW_TARGET                           1
#Define BCROW_SHORTCUT                         2
#Define BCROW_EXCLUDE                          3
#Define BCROW_COUNT                            4

#Define FRMBUILDCONFIG_ROWH                   40
#Define FRMBUILDCONFIG_FIELD_H                32
' A STACKED row -- label above a full-width field -- used by Description and by the raw
' options field on the switches page.
#Define FRMBUILDCONFIG_STACKLABEL_H           20
#Define FRMBUILDCONFIG_STACKROW_H             54
' The Shortcut button. 150, not BTN_W: its caption is a live accelerator that can read
' "Ctrl+Shift+Alt+F12", and PsButton ellipsizes without complaining.
#Define FRMBUILDCONFIG_SHORTCUT_W            150
' The switches preview, pinned to the BOTTOM of the General page. Its label row carries a
' small Edit button on the right, and the whole block is clickable.
'
' THE LABEL BAND MUST BE TALLER THAN THE EDIT BUTTON IT CARRIES (24 units, in
' PositionWindows). At 22 the button was centred in a band 2 units shorter than itself, so it
' hung a unit past the bottom and touched the preview box. 30 leaves 3 units clear either
' side; the layout self-test asserts the clearance.
'
' THE BLOCK'S TOTAL HEIGHT IS UNCHANGED at 78 -- gap 8 + label 30 + box 40, where it was
' 12 + 22 + 44. There was no slack to grow into: the General page ended EXACTLY at the
' footer, so the first attempt (band 32, nothing else touched) pushed the preview past it and
' the fits-above-the-footer assertion caught it. The eight units the band gained come from
' the gap above it and from the box, which holds one line of text in either size.
#Define FRMBUILDCONFIG_PREVIEW_GAP             8      ' above the label band
#Define FRMBUILDCONFIG_PREVIEWLABEL_H         30
#Define FRMBUILDCONFIG_PREVIEW_H              40
#Define FRMBUILDCONFIG_EDIT_W                 72

' ---- the compiler switches page ----------------------------------------------------------
#Define FRMBUILDCONFIG_SWROW_H                26     ' one switch row
#Define FRMBUILDCONFIG_SWGLYPH_W              28     ' its checkbox glyph cell

common shared as HACCEL ghAccelBuildConfigurations

' ----------------------------------------------------------------------------------------
' THE STAGING COPY -- what makes Cancel mean anything.
'
' Every edit writes here; nothing reaches gConfig.Builds until OK. It replaces the old
' gConfig.BuildsTemp for the reason frmUserTools.bi gives for gToolsWork: an undo buffer is
' not configuration, and clsConfig's own header explains why its variable-length arrays do
' not belong in a staging copy. BuildsTemp also LEAKED -- it was erased only on the IDOK
' path, so any other exit left it populated with live DWSTRINGs for the process lifetime.
' This one is erased in WM_DESTROY.
'
' It lives in the .bi rather than the .inc because frmAssignKey_ClashText reads it, and
' frmAssignKey is included BEFORE frmBuildConfig.inc.
' ----------------------------------------------------------------------------------------
' Aliased, same reason as TOOLSLIST -- see RFC-0004 s1.
type BUILDSLIST as FB.Array( of TYPE_BUILDS )
dim shared gBuildsWork as BUILDSLIST

dim shared HWND_FRMBUILDCONFIG_LIST as HWND

' ---- exports whose signatures are PINNED by callers outside this dialog ------------------
' All five read gConfig.Builds, never the staging array: getActiveBuildIndex is called from
' modCompile, modRoutines and frmUserTools while this dialog is not even open.
declare function frmBuildConfig_CreateAcceleratorTable() as long
declare function frmBuildConfig_getActiveBuildIndex() as long
declare function frmBuildConfig_GetSelectedBuildDescription() as DWSTRING
declare function frmBuildConfig_GetSelectedBuildGUID() as string
declare function frmBuildConfig_GetDefaultBuildGUID() as string
declare function frmBuildConfig_Show( byval hWndParent as HWND ) as LRESULT

' The shortcut a COMMITTED build displays: "Ctrl+Shift+Alt+F12", or "" when it has none.
' Composed, never stored. For the statusbar popup menu, which is built while this dialog is
' shut -- see the note on modifier order in the .inc before touching it.
declare function frmBuildConfig_MenuShortcutText( byval idx as long ) as DWSTRING
