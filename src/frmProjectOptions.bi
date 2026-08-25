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

' THIS HEADER MAY NAME NO Ps* TYPE, and that is a build-order constraint rather than a style
' choice: frmMain.inc and modRoutines.inc both pull it in, and both are compiled long before
' the control family's headers are reachable. The same rule frmBuildConfig.bi carries. Every
' Ps* handle below is therefore a plain HWND, and every callback typedef the implementation
' needs is declared privately in the .inc.

' Control ids.
'
' IDC_FRMPROJECTOPTIONS_CMDSELECT MUST STAY 1006: AfxIFileSaveDialog is handed it as the
' dialog's persistence GUID, so changing it makes the file dialog forget the folder the user
' last saved a project into. Same reason frmUserTools pins its browse button to 1012.
#Define IDC_FRMPROJECTOPTIONS_TXTPROJECTPATH        1005
#Define IDC_FRMPROJECTOPTIONS_CMDSELECT             1006
#Define IDC_FRMPROJECTOPTIONS_TXTOPTIONS32          1007
#Define IDC_FRMPROJECTOPTIONS_TXTOPTIONS64          1008
#Define IDC_FRMPROJECTOPTIONS_TOGMANIFEST           1009
#Define IDC_FRMPROJECTOPTIONS_TXTCOMMANDLINE        1010
#Define IDC_FRMPROJECTOPTIONS_CMDOK                 1011
#Define IDC_FRMPROJECTOPTIONS_CMDCANCEL             1012


' ========================================================================================
' Layout constants. ALL UNSCALED -- AfxScaleX/Y is applied at every point of use, never
' stored pre-scaled, so a constant read here is the same number the design was drawn at.
'
' The client height is DERIVED, not chosen: the body walk below sums to 471 and the footer
' is 58 of it, so 480 leaves a 9px tail above the footer rule and no dead band. Change any
' row or gap constant and this number has to be re-summed -- the layout self-test asserts
' the walk ends inside the footer, which is what turns a forgotten re-sum into a failure
' rather than into a control hanging off the bottom edge at 175% DPI.
' ========================================================================================
' THE VERTICAL SUM, unscaled. ComputeLayout walks it and the painter reads the stored
' results, so this is the one place the total is written down:
'
'     16 TOP + 20 hdr + 0            = 36    Project
'     + 2 rows x 38                  = 112
'     + 16 gap + 20 hdr + 6          = 154   Compiler options
'     + 22 hint + 2 rows x 38        = 246
'     + 16 gap + 20 hdr + 0          = 282   Build output
'     + 1 row x 38                   = 320   body ends
'     + 16 slack + 48 FOOTER_H       = 384
'
' IT CAME DOWN FROM 480 with the section rules. Those hairlines were what justified 46px rows
' and 20px gaps -- once the headings alone divide the sections, the same structure reads at 38
' and 16, and the page stops looking like a form with three empty bands in it. Every header
' also stopped advancing by the 1px the rule used to occupy.
'
' HDR_AFTER IS 0 ON PURPOSE, not an oversight. The heading band (HDR_TEXT_H) and the row
' stride below it (ROWH, with its content vertically centred) already put ~9 unscaled px
' between the heading text and the first label under it. Any explicit gap on top of that read
' as a blank ROW rather than as spacing -- most visibly under Build output, where a single
' toggle row follows the heading and there is nothing else to absorb the eye.
#Define FRMPROJECTOPTIONS_CLIENT_W                   720
#Define FRMPROJECTOPTIONS_CLIENT_H                   384
#Define FRMPROJECTOPTIONS_MARGIN                      22
#Define FRMPROJECTOPTIONS_TOP                         16     ' first section header's top
#Define FRMPROJECTOPTIONS_FOOTER_H                    48

' A section header: the caption, then a hairline, then air. HDR_AFTER is the air, and the
' Compiler options header uses HDR_AFTER_HINT instead because a hint line follows it.
#Define FRMPROJECTOPTIONS_HDR_TEXT_H                  20
#Define FRMPROJECTOPTIONS_HDR_AFTER                   0
#Define FRMPROJECTOPTIONS_HDR_AFTER_HINT               6
#Define FRMPROJECTOPTIONS_HINT_H                      22
#Define FRMPROJECTOPTIONS_SECTION_GAP                 16

' A row. ROWH is the STRIDE and FIELD_H is the control inside it -- the 16px difference is
' the vertical breathing room, and it is the only thing that separates two fields.
#Define FRMPROJECTOPTIONS_ROWH                        38
#Define FRMPROJECTOPTIONS_FIELD_H                     30
#Define FRMPROJECTOPTIONS_LABEL_W                    150

' THE BROWSE GUTTER IS RESERVED ON EVERY ROW, not just the one that has a button in it.
' That is what makes the right edge of the four fields, the "..." button and the manifest
' toggle one number -- xFieldRight, derived once in PositionWindows -- instead of three
' that can drift. Asserted as an equality.
#Define FRMPROJECTOPTIONS_BROWSE_W                    34
#Define FRMPROJECTOPTIONS_BROWSE_GAP                   8

#Define FRMPROJECTOPTIONS_BTN_W                       92
#Define FRMPROJECTOPTIONS_BTN_H                       32
#Define FRMPROJECTOPTIONS_GAP                          8

' Row and section header indices, used by both the layout and the painter.
#Define PO_ROW_PATH         0
#Define PO_ROW_COMMANDLINE  1
#Define PO_ROW_OTHER32      2
#Define PO_ROW_OTHER64      3
#Define PO_ROW_MANIFEST     4
#Define PO_ROW_COUNT        5

#Define PO_HDR_PROJECT      0
#Define PO_HDR_COMPILER     1
#Define PO_HDR_OUTPUT       2
#Define PO_HDR_COUNT        3


' ========================================================================================
' The staging copy.
'
' NOTHING BELOW REACHES gApp UNTIL OK. The dialog this replaces wrote straight through, and
' its Cancel path was only safe by accident. Deliberately dialog-local rather than a field
' on clsApp: an undo buffer is not application state, and clsConfig's own header already
' says why variable-length staging arrays do not belong there.
' ========================================================================================
type PROJECTOPTIONS_WORK
    ProjectPath as DWSTRING
    CommandLine as DWSTRING
    Other32     as DWSTRING
    Other64     as DWSTRING
    Manifest    as boolean
end type

declare function frmProjectOptions_Show( byval hWndParent as HWND, byval IsNewProject as boolean ) as LRESULT

' TIKO_PROJECTOPTIONS_SELFTEST=1. FALSE runs the headless half (frmMain, at startup); TRUE the
' layout half, which measures real windows and so is called from Show.
declare sub frmProjectOptions_RunSelfTest( byval bLayoutHalf as boolean )
