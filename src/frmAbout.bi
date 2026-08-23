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
' frmAbout -- the About box.
'
' Four bands stacked in a FIXED 720x480 client: a painted hero, a PsSelectBar tab strip, a
' body that swaps between three pages, and a footer of three PsButtons. Nothing resizes and
' nothing scrolls, which is what makes the fit assertable (see frmAbout_RunLayoutSelfTest).
'
' The dialog paints its OWN hero, tab-strip background, About page and footer divider in one
' PsBufferPaint pass; the only child windows are the select bar, the Credits list, the
' License field and the three buttons.
' ========================================================================================

#define IDC_FRMABOUT_TABS           1000
#define IDC_FRMABOUT_CREDITS        1001
#define IDC_FRMABOUT_LICENSE        1002
#define IDC_FRMABOUT_CMDWEBSITE     1003
#define IDC_FRMABOUT_CMDGITHUB      1004
#define IDC_FRMABOUT_CMDCLOSE       1005
' 1006 was IDC_FRMABOUT_LICSEL, the GPL-3 / Proprietary strip, removed in 7c step 15.
' NOT REUSED and the ids after it are not renumbered -- they are matched by value.
#define IDC_FRMABOUT_LICVSCROLL     1007

' The three pages, in tab order. Also the PsSelectBar panel indices, which is why they start
' at 0 and are contiguous -- frmAbout_ShowPage indexes with them directly.
enum ABOUTPAGE_ENUM
    ABOUTPAGE_ABOUT = 0
    ABOUTPAGE_CREDITS
    ABOUTPAGE_LICENSE
end enum

' ========================================================================================
' ONE LICENCE FILE, SINCE 7c STEP 15.
'
' This page used to offer two: a GPL-3 panel and a Proprietary one, chosen with a
' PsSelectBar, reading LICENSE and LICENSE-PROPRIETARY.txt. THE SECOND FILE DOES NOT EXIST
' and the dialog had been quietly showing its fallback notice instead -- caught the first
' time _check_selftests.bat opened this dialog, because the layout suite had asserted the
' file ships and nothing had ever run it.
'
' The proprietary licence is deprecated. LICENSE is the only option, so the file, the
' LICPAGE_ENUM that indexed between them and the two-panel selector all went: a selector
' over one item is dead UI, the same judgement step 13 applied to the Character Set combo.
' ========================================================================================
#define FRMABOUT_FILE_GPL          wstr("LICENSE")

' ---- geometry, ALL UNSCALED ------------------------------------------------------------
' Every one of these is passed through AfxScaleX/Y at the point of use. None is ever stored
' pre-scaled, so nothing here can be scaled twice.
#define FRMABOUT_CLIENT_W      720
#define FRMABOUT_CLIENT_H      480
#define FRMABOUT_HERO_H        168
#define FRMABOUT_TABS_H         40
#define FRMABOUT_FOOTER_H       60
#define FRMABOUT_MARGIN         32
#define FRMABOUT_PLATE          96     ' the rounded plate the "tk" wordmark sits on
#define FRMABOUT_PLATE_CURVE    28     ' ELLIPSE DIAMETER, not a radius (GDI vocabulary)
#define FRMABOUT_HERO_GAP       28     ' plate -> text column
#define FRMABOUT_PILL_H         24
#define FRMABOUT_PILL_PADX      12
#define FRMABOUT_PILL_GAP        8
#define FRMABOUT_BTN_W          96
#define FRMABOUT_BTN_H          30
#define FRMABOUT_BTN_GAP         8
#define FRMABOUT_BODY_PAD       16     ' body band -> the control or text inside it
' The GPL-3 / Proprietary strip carries NO geometry constants of its own. It shares the tab
' strip's band, so its height and vertical position come from FRMABOUT_TABS_H like the page
' tabs do, and its width is whatever its two captions measure -- which is the whole reason a
' right-aligned strip needs no number: it is pinned to the same right margin as the body.
#define FRMABOUT_ROW_H          24     ' Credits list row
#define FRMABOUT_KV_KEYW       148     ' About page: width of the key column
#define FRMABOUT_KV_ROWH        21
#define FRMABOUT_COL_WHO       210     ' Credits: "Contributor" column

' The two links the footer buttons open. Not localized: they are URLs, not prose.
#define FRMABOUT_URL_WEBSITE   wstr("http://www.planetsquires.com")
#define FRMABOUT_URL_GITHUB    wstr("https://github.com/PaulSquires/tiko")

declare function frmAbout_Show( byval hWndParent as hwnd ) as LRESULT
