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

' ==========================================================================================
' frmDebug - the debugger window.
'
' A modeless WS_POPUP owned by frmMain. Every element is a real control in the Ps* idiom;
' nothing here is hand-painted except the status line and the window background.
'
'   [PsButton x7]  Continue  Break  Stop | Step Into  Step Over  Step Out | Run to Cursor
'   ------------------------------------------------------------------------------------
'   Stopped - breakpoint  frmMain.inc:412  in frmMain_PositionWindows        (painted)
'   ------------------------------------------------------------------------------------
'   [PsSelectBar] Locals Globals         |  Call stack
'   [PsListTree, a real tree]           |  [PsListTree]
'                                    PsSplitter (vertical bar, panes side by side)
'
' All debugger state lives in debugParser.dll -- this file owns no line table, no
' breakpoint list and no process handle. It reacts to MSG_DBGP_STOPPED / _EXITED / _FAILED
' and calls back in.
'
' ------------------------------------------------------------------------------------------
' PUMP OBLIGATION -- and it is frmMain's, not this file's.
'
' Watch expressions are typed IN the variables list, so that control has label editing
' enabled, and PsListTree.bi is explicit that label editing is the one thing which makes
' PsListTree_FilterMessage mandatory: without it IsDialogMessage eats Enter and Escape
' before the in-place editor ever sees them.
'
' There is still no frmDebug_FilterMessage. The call frmMain's pump gained is
' PsListTree_FilterMessage, which is control-wide rather than window-wide -- one call covers
' every PsListTree in the application, so a second editing list would add nothing.
'
' ------------------------------------------------------------------------------------------
' THE VARIABLES TREE IS BUILT TO A BOUNDED DEPTH, NOT LAZILY.
'
' PsListTree reports a twisty click through its message callback but does not offer a
' "node expanded" hook, so there is no clean point at which to fetch children on demand.
' The tree is therefore built eagerly to FRMDEBUG_MAXDEPTH levels with at most
' FRMDEBUG_MAXCHILDREN per node, and everything below the top level starts collapsed. The
' cost is bounded and paid once per stop; the alternative would be a twisty-click handler
' that has to distinguish "expanding" from "collapsing" by inspecting state the control
' already owns.
' ==========================================================================================

#pragma once

#define IDC_FRMDEBUG_CMDFIRST      1001
#define IDC_FRMDEBUG_CMDCONTINUE   1001
#define IDC_FRMDEBUG_CMDBREAK      1002
#define IDC_FRMDEBUG_CMDSTOP       1003
#define IDC_FRMDEBUG_CMDSTEPINTO   1004
#define IDC_FRMDEBUG_CMDSTEPOVER   1005
#define IDC_FRMDEBUG_CMDSTEPOUT    1006
#define IDC_FRMDEBUG_CMDRUNCURSOR  1007
#define IDC_FRMDEBUG_CMDLAST       1007

#define IDC_FRMDEBUG_SELECTBAR     1010
#define IDC_FRMDEBUG_LVVARS        1011
#define IDC_FRMDEBUG_LVSTACK       1012
#define IDC_FRMDEBUG_SPLITTER      1013

' Tab identity is the panel ID, not the panel index -- frmOutput's rule, for the same
' reason: an index shifts when a tab is added.
#define DEBUG_TAB_LOCALS   0
#define DEBUG_TAB_GLOBALS  1
#define DEBUG_TAB_WATCH    2

const as long FRMDEBUG_MAXDEPTH    = 3
const as long FRMDEBUG_MAXCHILDREN = 200

' Unscaled layout constants. Scaled at point of use, never stored scaled -- the Output
' panel's height bug came from storing a scaled value in a field documented as unscaled.
const as long FRMDEBUG_DEFWIDTH    = 900
const as long FRMDEBUG_DEFHEIGHT   = 620
const as long FRMDEBUG_TOOLHEIGHT  = 44
const as long FRMDEBUG_STATUSHEIGHT = 26
const as long FRMDEBUG_TABHEIGHT   = 34
const as long FRMDEBUG_SPLITWIDTH  = 6
const as long FRMDEBUG_MINPANE     = 180

type DebugWindowPosition
    bInitialized as boolean = false
    bMaximized   as boolean = false
    nLeft        as long
    nTop         as long
    nRight       as long
    nBottom      as long
end type
dim shared gDebugPos as DebugWindowPosition

declare function frmDebug_Show( byval hWndParent as HWND, byval executable as DWSTRING, byval cmdparams as DWSTRING ) as LRESULT
declare sub      frmDebug_Command( byval id as long )
declare sub      frmDebug_ApplyTheme()
declare function frmDebug_IsOpen() as boolean
declare sub      frmDebug_ClearExecutionMarkers()
declare sub      frmDebug_Trace()
declare sub      frmDebug_RunSelfTest()

' The pane geometry, computed as a pure function of the client size and the bar position so
' it can be asserted without a window. LayoutPanes uses this and nothing else, which is what
' stops the assertions testing a parallel implementation of the layout.
type FRMDEBUG_LAYOUT
    rcStatus as RECT
    rcTabs   as RECT
    rcVars   as RECT
    rcSplit  as RECT
    rcStack  as RECT
end type
declare sub frmDebug_ComputeLayout( byval cx as long, byval cy as long, byval barPos as long, _
                                    byval nTool as long, byval nStatus as long, byval nTab as long, _
                                    byval nSplit as long, byref lo as FRMDEBUG_LAYOUT )

' The identifier-with-dotted-path under a column of a line. Pure, and separated from the
' Scintilla plumbing precisely so it can be asserted -- it is the one piece of the datatip
' path with real edge cases.
declare function frmDebug_ExprFromLine( byval wszLine as DWSTRING, byval col as long ) as DWSTRING
' Takes the Scintilla handle rather than a clsDocument ptr: this header is pulled in by
' modCompile.inc, which is included before clsDocument's type is complete.
declare sub      frmDebug_ShowDataTip( byval hEdit as HWND, byval nPos as long )
declare sub      frmDebug_HideDataTip()
declare sub      frmDebug_InitDataTip()
declare sub      frmDebug_DestroyDataTip()

' Watch expressions. Held here rather than in the engine: they are a UI concept -- text the
' user typed -- and the engine only ever sees them one at a time, already resolved.
const as long FRMDEBUG_MAXWATCH = 64
