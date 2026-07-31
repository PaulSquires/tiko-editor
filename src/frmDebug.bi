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
' nothing here is hand-painted except the status line, the pane captions and the background.
'
'   [PsButton x7]  Continue  Break  Stop | Step  Step Over  Step Out | Run to Cursor
'   ------------------------------------------------------------------------------------
'   Stopped - breakpoint  frmMain.inc:412  in frmMain_PositionWindows        (painted)
'   ------------------------------------------------------------------------------------
'    GLOBALS               |  CALL STACK
'    [PsListTree]          |  [PsListTree]
'   ----------------------- ------------------  <- SPLITLEFT / SPLITRIGHT (horizontal)
'    LOCALS                |  WATCH
'    [PsListTree]          |  [PsListTree]
'                          ^ SPLITMAIN (vertical)
'
' FOUR PANES, ALWAYS VISIBLE. This replaced a PsSelectBar strip over ONE shared tree, and
' the strip is gone rather than kept per-pane: the whole point of the change is that locals,
' globals, the call stack and the watch list are visible AT ONCE while stepping. A tab strip
' over each pane would cost 34px of four panes to offer a choice there is no longer anything
' to choose between.
'
' All debugger state lives in debugParser.dll -- this file owns no line table, no
' breakpoint list and no process handle. It reacts to MSG_DBGP_STOPPED / _EXITED / _FAILED
' and calls back in.
'
' ------------------------------------------------------------------------------------------
' PUMP OBLIGATION -- and it is frmMain's, not this file's.
'
' Watch expressions are typed IN the watch list, so that control has label editing enabled,
' and PsListTree.bi is explicit that label editing is the one thing which makes
' PsListTree_FilterMessage mandatory: without it IsDialogMessage eats Enter and Escape
' before the in-place editor ever sees them.
'
' There is still no frmDebug_FilterMessage. The call frmMain's pump gained is
' PsListTree_FilterMessage, which is control-wide rather than window-wide -- one call covers
' every PsListTree in the application, so a second editing list adds nothing.
'
' ------------------------------------------------------------------------------------------
' THE VARIABLES TREES ARE BUILT LAZILY, AND THAT IS FORCED RATHER THAN PREFERRED.
'
' This file used to say PsListTree offered no "node expanded" hook and therefore built the
' tree eagerly to a bounded depth on every stop. That was wrong even then --
' PsListTree_SetExpandCollapseCallback exists and fires for user toggles only -- and it
' became untenable when the engine's 1000-element array cap was lifted: an eager walk of a
' ten-thousand-element array is ten thousand memory reads into the debuggee for a node that
' displays as one collapsed row.
'
' A node that has children therefore gets ONE placeholder child, which is what makes the
' twisty appear. The first expand deletes the placeholder and materializes the real
' children. Every row's descriptor lives in gDbgRow(), indexed by the row's itemData, which
' survives insertion because the control owns the association and the descriptor array is
' only ever appended to.
'
' ------------------------------------------------------------------------------------------
' ARRAYS ARE SHOWN IN INDEX GROUPS, NOT FLAT.
'
'   my_array (single(0 to 9999))
'     [0 .. 99]
'     [100 .. 199]
'       (100) 3.1415
'
' The group tree is ADAPTIVE: a level that would produce more than FRMDEBUG_MAXGROUPS groups
' gets another level above it, so a million elements is three levels of a hundred rather than
' ten thousand sibling groups. Groups address their elements by the LINEAR child index the
' engine uses (row-major), so a multidimensional array groups over the same index space and
' the element keeps the readable "M(1, 0)" name the engine gives it.
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

#define IDC_FRMDEBUG_LVLOCALS      1011
#define IDC_FRMDEBUG_LVSTACK       1012
#define IDC_FRMDEBUG_SPLITMAIN     1013
#define IDC_FRMDEBUG_LVGLOBALS     1014
#define IDC_FRMDEBUG_LVWATCH       1015
#define IDC_FRMDEBUG_SPLITLEFT     1016
#define IDC_FRMDEBUG_SPLITRIGHT    1017
#define IDC_FRMDEBUG_CMDADDWATCH   1018

' A UDT still expands eagerly one level at a time; this bounds the FIELD count, not an
' array's element count, which is grouped instead.
const as long FRMDEBUG_MAXCHILDREN = 200

' Array grouping. FRMDEBUG_CHUNK is the leaf group size (the example in the header);
' FRMDEBUG_MAXGROUPS is what forces a further level rather than a very long list of groups.
const as long FRMDEBUG_CHUNK     = 100
const as long FRMDEBUG_MAXGROUPS = 100

' Unscaled layout constants. Scaled at point of use, never stored scaled -- the Output
' panel's height bug came from storing a scaled value in a field documented as unscaled.
const as long FRMDEBUG_DEFWIDTH    = 900
const as long FRMDEBUG_DEFHEIGHT   = 620
const as long FRMDEBUG_TOOLHEIGHT  = 44
const as long FRMDEBUG_STATUSHEIGHT = 26
const as long FRMDEBUG_CAPHEIGHT   = 26
const as long FRMDEBUG_SPLITWIDTH  = 6
const as long FRMDEBUG_MINPANE     = 180
const as long FRMDEBUG_MINPANEV    = 70

' Splitter positions are persisted as PERCENT x 100 of the span they divide, never as
' pixels: the window is resizable and can reopen on a different monitor at a different DPI,
' where a stored pixel offset either strands the bar against one edge or lands outside the
' pane entirely.
' The defaults are deliberately ASYMMETRIC and were chosen from a real session rather than by
' halving: globals is a short list and locals is the one you read while stepping, so the left
' column gives locals the larger share; the call stack is usually a handful of frames while
' the watch list grows, so the right column splits low.
const as long FRMDEBUG_PCTSCALE      = 10000
const as long FRMDEBUG_DEFPCTMAIN    = 6000    ' vertical bar, 60% across
const as long FRMDEBUG_DEFPCTLEFT    = 3200    ' globals over locals
const as long FRMDEBUG_DEFPCTRIGHT   = 8300    ' call stack over watch

declare function frmDebug_Show( byval hWndParent as HWND, byval executable as DWSTRING, byval cmdparams as DWSTRING ) as LRESULT
declare sub      frmDebug_Command( byval id as long )
declare sub      frmDebug_ApplyTheme()
declare function frmDebug_IsOpen() as boolean
declare sub      frmDebug_ClearExecutionMarkers()
declare sub      frmDebug_Trace()
declare sub      frmDebug_RunSelfTest()
declare sub      frmDebug_CaptureState()
declare sub      frmDebug_ArmButtonTooltips()

' The pane geometry, computed as a pure function of the client size and the three bar
' positions so it can be asserted without a window. LayoutPanes uses this and nothing else,
' which is what stops the assertions testing a parallel implementation of the layout.
'
' The caption rects are part of the layout rather than being derived by the painter, so the
' band above a pane and the pane itself cannot drift apart.
type FRMDEBUG_LAYOUT
    rcStatus     as RECT
    rcCapGlobals as RECT
    rcGlobals    as RECT
    rcCapLocals  as RECT
    rcLocals     as RECT
    rcCapStack   as RECT
    rcStack      as RECT
    rcCapWatch   as RECT
    rcWatch      as RECT
    rcSplitMain  as RECT
    rcSplitLeft  as RECT
    rcSplitRight as RECT
end type
declare sub frmDebug_ComputeLayout( byval cx as long, byval cy as long, _
                                    byval barMain as long, byval barLeft as long, byval barRight as long, _
                                    byval nTool as long, byval nStatus as long, byval nCap as long, _
                                    byval nSplit as long, byref lo as FRMDEBUG_LAYOUT )

' Splitter position <-> stored percent. Pure, and a pair rather than one function with a
' flag, because the round trip is what the assertions check.
declare function frmDebug_PctToPos( byval nPct as long, byval nFrom as long, byval nTo as long ) as long
declare function frmDebug_PosToPct( byval nPos as long, byval nFrom as long, byval nTo as long ) as long

' The group tree for an array of nTotal elements: how many elements one group at the top
' level spans. FRMDEBUG_CHUNK when that gives few enough groups, otherwise the next power of
' FRMDEBUG_CHUNK up. Pure, so the shape can be asserted for sizes no test could build.
declare function frmDebug_GroupSpan( byval nTotal as longint ) as longint

' The index range of group nGroup, counting from idxFrom, covering nCount elements in steps
' of nSpan. Returns FALSE past the last group. Pure, and it is what the real expansion loop
' calls -- so an off-by-one here fails an assertion rather than silently dropping the last
' element of every array or listing one twice.
declare function frmDebug_GroupBounds( byval idxFrom as longint, byval nCount as longint, _
                                       byval nSpan as longint, byval nGroup as longint, _
                                       byref idxLo as longint, byref idxHi as longint ) as boolean

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

' The watch list's delete affordance: an X at the LEADING EDGE of each row, shown only while
' the cursor is on that row. It is drawn inside column 0 rather than being a column of its
' own, and that is forced rather than chosen -- PsListTree draws the row's own Text in
' column 0 and hangs the tree indent and twisty off it, so a real column 0 would put the X
' where the expander belongs and push the name out of the tree column entirely. The painter
' reserves the first FRMDEBUG_DELCOLW pixels of column 0 and insets the name past them,
' which reads exactly as a leading column and costs the tree model nothing.
const as long FRMDEBUG_DELCOLW = 26

' ------------------------------------------------------------------------------------------
' Row descriptors
'
' A tree row is one of three things and the expand handler has to know which. The descriptor
' index travels in the row's itemData, which is safe because gDbgRow() is only ever appended
' to -- an index handed out stays valid even as rows are inserted above it.
' ------------------------------------------------------------------------------------------
#define DBGROW_NODE         0   ' a variable, UDT field, pointer target or array element
#define DBGROW_GROUP        1   ' an array index range: [lo .. hi]
#define DBGROW_PLACEHOLDER  2   ' the one child that makes a twisty appear before expanding

' "This row has no descriptor." It must not be 0: PsListTree_AddNode DEFAULTS itemData to
' zero, so a row added without one would claim descriptor 0 and inherit whatever that row's
' changed flag happens to be.
#define FRMDEBUG_NOROW     (-1)

type FRMDEBUG_ROW
    kind     as long
    nd       as DBGP_NODE   ' NODE: this row's node.  GROUP: the ARRAY the group belongs to.
    idxLo    as longint     ' GROUP: first linear child index covered
    idxHi    as longint     ' GROUP: last  linear child index covered
    bFilled  as boolean     ' children have been materialized
    bChanged as boolean     ' value differs from the previous stop; painted in the alert colour
    wszPath  as DWSTRING    ' identity for change detection; see the value cache
end type

' Which column a variables list is sorted by, and which way. Per tree, session-lifetime, not
' persisted -- a sort is a way of looking at THIS stop, and stepping must not reshuffle it.
type FRMDEBUG_SORT
    nCol    as long = -1    ' -1 = unsorted, i.e. the order the engine reported
    bDesc   as boolean
end type

' Compare two cells for the sort. A plain case-insensitive TEXT compare, deliberately: every
' column here is text, and a numeric-aware compare would order the Value column by what the
' value happens to look like -- so "10" and "9" would sort numerically while "true" and
' "{ 2 fields }" beside them did not. Pure, so it can be asserted without a control.
declare function frmDebug_SortLess( byval wszA as DWSTRING, byval wszB as DWSTRING, _
                                    byval bDesc as boolean ) as boolean
