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

' See frmDebug.bi for the layout, the absence of a pump obligation, and why the variables
' tree is built to a bounded depth rather than lazily.
'
' ------------------------------------------------------------------------------------------
' WS_EX_COMPOSITED IS ON THIS WINDOW, AND IT IS THE ONLY ONE IN TIKO THAT HAS IT.
'
' Dragging the splitter tore. Batching the pane moves into one DeferWindowPos pass helped and
' was worth doing on its own, but did not fix it: a PsListTree is a CONTAINER, so resizing one
' re-places its own surface, header and scrollbar as separate child moves that no batching at
' this level can reach. WS_EX_COMPOSITED makes Windows paint the whole hierarchy bottom-up
' into an off-screen buffer and blit it once, which is the only thing that covers those.
'
' The cost is real and is accepted here rather than hidden: every repaint of this window
' allocates and blits a full-window buffer, so it is slower per paint than the default. That
' is the right trade for a window whose panes are resized by dragging, and the wrong one for
' frmMain, which repaints constantly while typing. Do not copy it there without measuring.

#include once "frmDebug.bi"

declare sub frmDebug_LayoutPanes()
declare sub frmDebug_SyncWatchButtons()
declare sub frmDebug_FillGlobals()
declare sub frmDebug_FillLocals()
declare sub frmDebug_FillWatch()

dim shared as HWND     gDbgBtn(IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST)
dim shared as DWSTRING gDbgStatusText
dim shared as long     gDbgCurFrame          ' call-stack frame the variables view is scoped to
dim shared as boolean  gDbgSessionEnding
dim shared as boolean  gDbgSplitInit         ' the splitters' positions are applied once
dim shared as DWSTRING gDbgWatch(FRMDEBUG_MAXWATCH - 1)
dim shared as long     gDbgWatchCount
dim shared as HWND     gDbgDataTip           ' manually driven; see frmDebug_InitDataTip
dim shared as DWSTRING gDbgDataTipExpr       ' what the visible tip is showing, "" if none

' The first stop of a session is the program's entry, not a breakpoint. When breakpoints are
' armed we resume straight through it -- see frmDebug_OnStopped.
dim shared as boolean  gDbgFirstStop
dim shared as boolean  gDbgHaveBreakpoints

' Row descriptors. Appended to and never compacted, so an index already handed to a row's
' itemData stays valid. Cleared per fill of a given tree only by starting a new session --
' abandoned entries cost a struct each and are reclaimed when the window closes.
dim shared as FRMDEBUG_ROW gDbgRow()
dim shared as long         gDbgRowCount

' Sort state, one per variables tree.
dim shared as FRMDEBUG_SORT gDbgSortGlobals
dim shared as FRMDEBUG_SORT gDbgSortLocals

' The + button in the WATCH caption band. A real control rather than a painted glyph, so it
' carries its own tooltip, hover and press states for free.
dim shared as HWND gDbgBtnAddWatch
dim shared as HWND gDbgBtnDelWatch

' Collapsed ("minimized") state -- see frmDebug.bi. gDbgRestoreRect is the window rect to come
' BACK to, in screen coords, captured at the moment of collapse: the strip's own rect is not it.
dim shared as boolean gDbgCollapsed
dim shared as RECT    gDbgRestoreRect
dim shared as HWND    gDbgBtnRestore

' The strip, in the order it is drawn. Break is absent by design (frmDebug.bi says why), and
' the restore icon is appended by the layout rather than listed here -- it is not a debug
' command and must never pick up the enable state of one.
dim shared as long gDbgStripIds(0 to 5) => { _
    IDC_FRMDEBUG_CMDCONTINUE, IDC_FRMDEBUG_CMDSTOP, IDC_FRMDEBUG_CMDSTEPINTO, _
    IDC_FRMDEBUG_CMDSTEPOVER, IDC_FRMDEBUG_CMDSTEPOUT, IDC_FRMDEBUG_CMDRUNCURSOR }

declare sub frmDebug_PositionWindows()
declare sub frmDebug_SyncButtons()

' ==========================================================================================
' Status line
' ==========================================================================================
private sub frmDebug_SetStatus( byval wszText as DWSTRING )
    gDbgStatusText = wszText
    if IsWindow(HWND_FRMDEBUG) then AfxRedrawWindow( HWND_FRMDEBUG )
end sub

private function frmDebug_ReasonText( byval reason as long ) as DWSTRING
    select case reason
        case DBGP_STOP_ENTRY      : return L(435, "Started")
        case DBGP_STOP_STEP       : return L(436, "Stepped")
        case DBGP_STOP_BREAKPOINT : return L(437, "Breakpoint")
        case DBGP_STOP_TEMPBP     : return L(252, "Run to Cursor")
        case DBGP_STOP_USERBREAK  : return L(433, "Break")
        case DBGP_STOP_EXCEPTION  : return L(438, "Exception")
    end select
    return ""
end function

' ==========================================================================================
' Editor markers
' ==========================================================================================
sub frmDebug_ClearExecutionMarkers()
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        for i as long = 0 to 1
            if pDoc->hWindow(i) then
                SciExec( pDoc->hWindow(i), SCI_MARKERDELETEALL, MARKER_DEBUGGER_CURLINE, 0 )
            end if
        next
        pDoc = pDoc->pDocNext
    loop
end sub

' Navigate the editor to a location and mark it as the execution point. The marker is ADDED
' explicitly rather than toggled: the old implementation called ToggleMarker here, which
' REMOVES the arrow whenever the debugger stops twice on the same line -- which is exactly
' what a loop body does on every iteration.
private sub frmDebug_ShowExecutionPoint( byval wszFile as DWSTRING, byval nLine1Based as long )
    if PsLen(wszFile) = 0 then exit sub
    if nLine1Based <= 0 then exit sub

    frmDebug_ClearExecutionMarkers()

    dim as long nLine0 = nLine1Based - 1
    dim pDoc as clsDocument ptr = OpenSelectedDocument( wszFile, "", nLine0 )
    if pDoc = 0 then exit sub

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then exit sub
    SciExec( hEdit, SCI_MARKERDELETEALL, MARKER_DEBUGGER_CURLINE, 0 )
    SciExec( hEdit, SCI_MARKERADD, nLine0, MARKER_DEBUGGER_CURLINE )
end sub

' ==========================================================================================
' Call stack
' ==========================================================================================
private sub frmDebug_FillCallStack( byval frameCount as long )
    dim as HWND hLV = HWND_FRMDEBUG_LVSTACK
    if IsWindow(hLV) = 0 then exit sub

    PsListTree_Clear( hLV )
    for i as long = 0 to frameCount - 1
        dim as DBGP_FRAME fr
        if debugparser_get_frame( i, @fr ) <> DBGP_OK then continue for

        dim as DWSTRING wszFile
        if len(fr.wszSrcPath) then wszFile = PsPathName(fr.wszSrcPath )
        dim as DWSTRING wszLine
        if fr.lineNum > 0 then wszLine = wstr(str(fr.lineNum))

        dim as DWSTRING wszRow = "" & chr(9) & fr.wszProcName & chr(9) & wszLine & chr(9) & wszFile
        dim as long nRow = ListBox_AddTabbedRow( hLV, wszRow )
        ' itemData carries the frame index, so selecting a row can re-scope the variables.
        if nRow >= 0 then PsListTree_SetItemData( hLV, nRow, i )
    next
    if frameCount > 0 then PsListTree_SetCurSel( hLV, 0 )
end sub

' ==========================================================================================
' The value cache -- what "changed" means
'
' The engine caches nothing and should not start: a REDIM array's address moves when it is
' reallocated, so an address is not a stable identity for a variable across two stops. The
' key is the row's PATH ("G:gRecs(1).x", "L0:count"), which is stable precisely because it is
' what the user would type to reach the same thing.
'
' Two tables, not one. gDbgValPrev is the picture as of the previous stop and is what a fill
' compares against; gDbgValNext accumulates what the current stop has shown. Prev is replaced
' from Next at the START of a stop rather than the end, so a fill triggered WITHIN a stop --
' clicking a call-stack frame, expanding a node -- still compares against the previous stop
' and does not quietly mark everything unchanged.
' ==========================================================================================
const as long FRMDEBUG_VALSLOTS = 4096          ' power of two; index = hash and (slots-1)

type FRMDEBUG_VALSLOT
    wszKey as DWSTRING
    wszVal as DWSTRING
    bUsed  as boolean
end type
dim shared as FRMDEBUG_VALSLOT gDbgValPrev(FRMDEBUG_VALSLOTS - 1)
dim shared as FRMDEBUG_VALSLOT gDbgValNext(FRMDEBUG_VALSLOTS - 1)

private function frmDebug_ValSlot( arr() as FRMDEBUG_VALSLOT, byval wszKey as DWSTRING ) as long
    ' FNV-1a over the key, then linear probing. A full table simply stops recording, which
    ' degrades to "nothing looks changed" rather than to a wrong highlight.
    dim as ulongint h = 14695981039346656037ull
    for i as long = 1 to PsLen(wszKey)
        h = (h xor cast(ulongint, asc(PsMid(wszKey, i, 1)))) * 1099511628211ull
    next
    dim as long idx = cast(long, h and cast(ulongint, FRMDEBUG_VALSLOTS - 1))
    for probe as long = 0 to FRMDEBUG_VALSLOTS - 1
        dim as long k = (idx + probe) and (FRMDEBUG_VALSLOTS - 1)
        if arr(k).bUsed = false then return k
        if arr(k).wszKey = wszKey then return k
    next
    return -1
end function

' Has this row's value changed since the previous stop? A key never seen before is NOT a
' change -- a variable coming into scope for the first time has not changed, it has appeared.
private function frmDebug_ValueChanged( byval wszKey as DWSTRING, byval wszVal as DWSTRING ) as boolean
    if PsLen(wszKey) = 0 then return false

    dim as boolean bChanged = false
    dim as long kPrev = frmDebug_ValSlot( gDbgValPrev(), wszKey )
    if (kPrev >= 0) andalso gDbgValPrev(kPrev).bUsed then
        if gDbgValPrev(kPrev).wszVal <> wszVal then bChanged = true
    end if

    dim as long kNext = frmDebug_ValSlot( gDbgValNext(), wszKey )
    if kNext >= 0 then
        gDbgValNext(kNext).bUsed  = true
        gDbgValNext(kNext).wszKey = wszKey
        gDbgValNext(kNext).wszVal = wszVal
    end if
    return bChanged
end function

private sub frmDebug_ValueRoll()
    for i as long = 0 to FRMDEBUG_VALSLOTS - 1
        gDbgValPrev(i) = gDbgValNext(i)
    next
end sub

private sub frmDebug_ValueReset()
    for i as long = 0 to FRMDEBUG_VALSLOTS - 1
        gDbgValPrev(i).bUsed = false : gDbgValPrev(i).wszKey = "" : gDbgValPrev(i).wszVal = ""
        gDbgValNext(i).bUsed = false : gDbgValNext(i).wszKey = "" : gDbgValNext(i).wszVal = ""
    next
end sub

' ==========================================================================================
' Row descriptors
' ==========================================================================================
private sub frmDebug_RowsReset()
    gDbgRowCount = 0
    redim gDbgRow(0 to 255)
end sub

private function frmDebug_RowNew() as long
    if gDbgRowCount > ubound(gDbgRow) then
        redim preserve gDbgRow( 0 to (gDbgRowCount * 2) + 15 )
    end if
    dim as long k = gDbgRowCount
    gDbgRowCount += 1
    gDbgRow(k).kind    = DBGROW_NODE
    gDbgRow(k).idxLo   = 0
    gDbgRow(k).idxHi   = -1
    gDbgRow(k).bFilled = false
    gDbgRow(k).wszPath = ""
    return k
end function

' ==========================================================================================
' Array grouping
'
' The span of ONE top-level group. FRMDEBUG_CHUNK while that gives at most FRMDEBUG_MAXGROUPS
' groups, and multiplied by FRMDEBUG_CHUNK for each further level needed -- so 10,000
' elements is 100 groups of 100, and 1,000,000 is 100 groups of 10,000, each of which splits
' again on expand. Pure: it takes a count and returns a span, so the shape of a tree that
' could never be built in a test is still assertable.
' ==========================================================================================
function frmDebug_GroupSpan( byval nTotal as longint ) as longint
    if nTotal <= FRMDEBUG_CHUNK then return 0        ' small enough to list directly
    dim as longint span = FRMDEBUG_CHUNK
    ' Grow the span until the number of groups it produces fits.
    do while ((nTotal + span - 1) \ span) > FRMDEBUG_MAXGROUPS
        span *= FRMDEBUG_CHUNK
        if span <= 0 then exit do                    ' overflow guard; cannot happen for a long
    loop
    return span
end function

function frmDebug_GroupBounds( byval idxFrom as longint, byval nCount as longint, _
                               byval nSpan as longint, byval nGroup as longint, _
                               byref idxLo as longint, byref idxHi as longint ) as boolean
    idxLo = 0 : idxHi = -1
    if (nSpan <= 0) orelse (nCount <= 0) orelse (nGroup < 0) then return false
    dim as longint start = nGroup * nSpan
    if start >= nCount then return false
    idxLo = idxFrom + start
    dim as longint stop_ = start + nSpan - 1
    if stop_ > nCount - 1 then stop_ = nCount - 1        ' the last group is short
    idxHi = idxFrom + stop_
    return true
end function

' ==========================================================================================
' Building the variables trees
'
' Every "add a row" path goes through frmDebug_AddNodeRow so that the descriptor, the cell
' text, the change flag and the placeholder are decided in ONE place. Children are NOT walked
' here -- that is the expand handler's job.
' ==========================================================================================
private sub frmDebug_AddPlaceholder( byval hLV as HWND, byval parentRow as long )
    dim as long k = frmDebug_RowNew()
    gDbgRow(k).kind = DBGROW_PLACEHOLDER
    dim as long r = PsListTree_AddNode( hLV, parentRow, "", k )
    if r >= 0 then PsListTree_SetRowSelectable( hLV, r, false )
end sub

' Returns the row index, or -1. wszPath is the identity used for change detection; pass ""
' for a row whose value is not worth tracking.
private function frmDebug_AddNodeRow( byval hLV as HWND, byval parentRow as long, _
                                      byval pNode as DBGP_NODE ptr, byval wszPath as DWSTRING, _
                                      byval bTrackChange as boolean ) as long
    dim as long k = frmDebug_RowNew()
    gDbgRow(k).kind    = DBGROW_NODE
    gDbgRow(k).nd      = *pNode
    gDbgRow(k).wszPath = wszPath

    dim as long r = PsListTree_AddNode( hLV, parentRow, pNode->wszName, k )
    if r < 0 then return -1

    PsListTree_SetCellText( hLV, r, 1, pNode->wszValue )
    PsListTree_SetCellText( hLV, r, 2, pNode->wszType )

    if bTrackChange then
        gDbgRow(k).bChanged = frmDebug_ValueChanged( wszPath, pNode->wszValue )
    end if

    if pNode->hasChildren <> 0 then
        frmDebug_AddPlaceholder( hLV, r )
        PsListTree_SetNodeCollapsed( hLV, r, true )
    end if
    return r
end function

' One index group row: "[100 .. 199]". Non-selectable -- it is a way of reaching elements,
' not a thing with a value.
private function frmDebug_AddGroupRow( byval hLV as HWND, byval parentRow as long, _
                                       byval pArray as DBGP_NODE ptr, byval wszPath as DWSTRING, _
                                       byval idxLo as longint, byval idxHi as longint ) as long
    dim as long k = frmDebug_RowNew()
    gDbgRow(k).kind    = DBGROW_GROUP
    gDbgRow(k).nd      = *pArray
    gDbgRow(k).idxLo   = idxLo
    gDbgRow(k).idxHi   = idxHi
    ' A group's path has to differ from its ARRAY's -- they used to be the same string, so
    ' every group of one array was indistinguishable from the array and from its siblings.
    ' Nothing depended on it until expansion state had to survive a step, which is keyed on
    ' exactly this.
    gDbgRow(k).wszPath = wszPath & "#" & str(idxLo) & "-" & str(idxHi)

    dim as DWSTRING wszCaption = "[" & str(idxLo) & " .. " & str(idxHi) & "]"
    dim as long r = PsListTree_AddNode( hLV, parentRow, wszCaption, k )
    if r < 0 then return -1
    PsListTree_SetCellText( hLV, r, 1, str(idxHi - idxLo + 1) & " " & L(458, "elements") )
    PsListTree_SetRowSelectable( hLV, r, false )
    frmDebug_AddPlaceholder( hLV, r )
    PsListTree_SetNodeCollapsed( hLV, r, true )
    return r
end function

' Materialize the children of one already-expanded row. Deletes the placeholder first: the
' twisty exists because of it, and by this point the real children are about to replace it.
private sub frmDebug_ExpandRowChildren( byval hLV as HWND, byval row as long )
    dim as long k = cast(long, PsListTree_GetItemData( hLV, row ))
    if (k < 0) orelse (k >= gDbgRowCount) then exit sub
    if gDbgRow(k).bFilled then exit sub
    gDbgRow(k).bFilled = true

    ' Drop the placeholder. It is the row's only child at this point, so it sits directly
    ' after it in the flat model.
    if PsListTree_GetChildCount( hLV, row ) > 0 then
        dim as long kc = cast(long, PsListTree_GetItemData( hLV, row + 1 ))
        if (kc >= 0) andalso (kc < gDbgRowCount) then
            if gDbgRow(kc).kind = DBGROW_PLACEHOLDER then PsListTree_DeleteString( hLV, row + 1 )
        end if
    end if

    dim as DBGP_NODE parent = gDbgRow(k).nd
    dim as DWSTRING  wszBase = gDbgRow(k).wszPath

    ' ---- an index group: either the elements themselves, or a finer set of groups ----
    if gDbgRow(k).kind = DBGROW_GROUP then
        dim as longint nSpan = gDbgRow(k).idxHi - gDbgRow(k).idxLo + 1
        if nSpan > FRMDEBUG_CHUNK then
            dim as longint sub_ = frmDebug_GroupSpan( nSpan )
            dim as longint g = 0, gLo, gHi
            do while frmDebug_GroupBounds( gDbgRow(k).idxLo, nSpan, sub_, g, gLo, gHi )
                frmDebug_AddGroupRow( hLV, row, @parent, wszBase, gLo, gHi )
                g += 1
            loop
        else
            for i as longint = gDbgRow(k).idxLo to gDbgRow(k).idxHi
                dim as DBGP_NODE ch
                if debugparser_child_node( @parent, cast(long, i), @ch ) <> DBGP_OK then continue for
                frmDebug_AddNodeRow( hLV, row, @ch, wszBase & "." & ch.wszName, true )
            next
        end if
        PsListTree_Refresh( hLV )
        exit sub
    end if

    ' ---- an array: group it rather than listing thousands of elements ----
    dim as DBGP_ARRAYINFO ai
    if debugparser_array_info( @parent, @ai ) = DBGP_OK then
        dim as longint span = frmDebug_GroupSpan( ai.total )
        if span > 0 then
            dim as longint g = 0, gLo, gHi
            do while frmDebug_GroupBounds( 0, ai.total, span, g, gLo, gHi )
                frmDebug_AddGroupRow( hLV, row, @parent, wszBase, gLo, gHi )
                g += 1
            loop
            PsListTree_Refresh( hLV )
            exit sub
        end if
        ' Small enough to list directly; fall through to the plain child walk below.
    end if

    ' ---- UDT fields, a pointer target, or a short array ----
    dim as long n = debugparser_child_count( @parent )
    if n <= 0 then
        PsListTree_Refresh( hLV )
        exit sub
    end if
    dim as long shown = n
    if shown > FRMDEBUG_MAXCHILDREN then shown = FRMDEBUG_MAXCHILDREN

    for i as long = 0 to shown - 1
        dim as DBGP_NODE ch
        if debugparser_child_node( @parent, i, @ch ) <> DBGP_OK then continue for
        frmDebug_AddNodeRow( hLV, row, @ch, wszBase & "." & ch.wszName, true )
    next

    ' Say so rather than silently showing a prefix. A truncated list that looks complete is
    ' worse than a short one that admits what it left out.
    if n > shown then
        dim as long kk = frmDebug_RowNew()
        gDbgRow(kk).kind = DBGROW_PLACEHOLDER
        dim as long r = PsListTree_AddNode( hLV, row, "", kk )
        if r >= 0 then
            PsListTree_SetCellText( hLV, r, 1, L(439, "more elements not shown") & _
                                    "  (" & str(n - shown) & ")" )
            PsListTree_SetRowSelectable( hLV, r, false )
        end if
    end if
    PsListTree_Refresh( hLV )
end sub

' The USER toggled a twisty. Silent for every programmatic setter, which is what makes this
' safe to call PsListTree back from.
private sub frmDebug_ExpandCollapse( byval hListControl as HWND, byval row as integer, byval bExpanded as boolean )
    if bExpanded = false then exit sub
    frmDebug_ExpandRowChildren( hListControl, row )
end sub

' ==========================================================================================
' Expansion state has to SURVIVE A STEP
'
' Every stop rebuilds all three variables trees from scratch, so without this an expanded
' node closes on every step -- which makes inspecting one element of an array impossible,
' since reaching it is several twisty clicks and stepping is the whole point of being there.
'
' What is remembered is the set of expanded PATHS, not row indices: rows move as soon as
' anything above them expands or the sort changes, and a variable that goes out of scope
' takes its index with it. A path that no longer resolves simply never matches, which is
' exactly the "unless the group itself no longer exists" rule -- it needs no special case.
'
' The scroll position is restored too. Re-opening the node you were reading and then
' scrolling it off the top would answer the letter of the problem and not the point of it.
' ==========================================================================================
const as long FRMDEBUG_MAXEXPAND = 256
const as long FRMDEBUG_TREESLOTS = 3            ' globals, locals, watch

dim shared as DWSTRING gDbgExp(FRMDEBUG_TREESLOTS - 1, FRMDEBUG_MAXEXPAND - 1)
dim shared as long     gDbgExpCount(FRMDEBUG_TREESLOTS - 1)
dim shared as long     gDbgExpTop(FRMDEBUG_TREESLOTS - 1)

' Record which nodes are open, before the rows that describe them are thrown away.
'
' A row count of zero means the descriptors have ALREADY been reset for this rebuild, so
' there is nothing left to read -- and in that case the stash must be left exactly as it is
' rather than cleared, because it is what the restore is about to use. That is what lets
' frmDebug_FillVariables capture all three trees up front and each fill still call this.
private sub frmDebug_CaptureExpanded( byval nSlot as long, byval hLV as HWND )
    if (nSlot < 0) orelse (nSlot >= FRMDEBUG_TREESLOTS) then exit sub
    if IsWindow(hLV) = 0 then exit sub
    if gDbgRowCount = 0 then exit sub

    gDbgExpCount(nSlot) = 0
    gDbgExpTop(nSlot)   = PsListTree_GetTopIndex( hLV )

    for r as long = 0 to PsListTree_GetCount(hLV) - 1
        if gDbgExpCount(nSlot) >= FRMDEBUG_MAXEXPAND then exit for
        if PsListTree_IsCollapsed( hLV, r ) then continue for
        if PsListTree_HasChildren( hLV, r ) = false then continue for
        dim as long k = cast(long, PsListTree_GetItemData( hLV, r ))
        if (k < 0) orelse (k >= gDbgRowCount) then continue for
        if PsLen(gDbgRow(k).wszPath) = 0 then continue for
        gDbgExp(nSlot, gDbgExpCount(nSlot)) = gDbgRow(k).wszPath
        gDbgExpCount(nSlot) += 1
    next
end sub

private function frmDebug_WasExpanded( byval nSlot as long, byval wszPath as DWSTRING ) as boolean
    if (nSlot < 0) orelse (nSlot >= FRMDEBUG_TREESLOTS) then return false
    for i as long = 0 to gDbgExpCount(nSlot) - 1
        if gDbgExp(nSlot, i) = wszPath then return true
    next
    return false
end function

' Re-open everything that was open. The walk runs FORWARD and re-reads the row count each
' time round, because expanding a node inserts its children immediately after it -- so a
' nested node is reached by the same pass that opened its parent, at any depth, with no
' recursion here.
private sub frmDebug_RestoreExpanded( byval nSlot as long, byval hLV as HWND )
    if (nSlot < 0) orelse (nSlot >= FRMDEBUG_TREESLOTS) then exit sub
    if IsWindow(hLV) = 0 then exit sub
    if gDbgExpCount(nSlot) <= 0 then exit sub

    PsListTree_BeginUpdate( hLV )
    dim as long r = 0
    do while r < PsListTree_GetCount(hLV)
        dim as long k = cast(long, PsListTree_GetItemData( hLV, r ))
        if (k >= 0) andalso (k < gDbgRowCount) then
            if (gDbgRow(k).bFilled = false) andalso (PsLen(gDbgRow(k).wszPath) > 0) then
                if frmDebug_WasExpanded( nSlot, gDbgRow(k).wszPath ) then
                    frmDebug_ExpandRowChildren( hLV, r )
                    ' The SILENT setter, deliberately: the user did not just toggle this, so
                    ' the expand/collapse callback must not fire and re-enter the fill.
                    PsListTree_SetNodeCollapsed( hLV, r, false )
                end if
            end if
        end if
        r += 1
    loop
    PsListTree_EndUpdate( hLV )

    if gDbgExpTop(nSlot) > 0 then PsListTree_SetTopIndex( hLV, gDbgExpTop(nSlot) )
    PsListTree_Refresh( hLV )
end sub

' ------------------------------------------------------------------------------------------
' Sorting. The MODEL is sorted before rows are built, never the control -- PsListTree does
' not sort and says so; it only hands the host a header click. Top-level rows only: array
' groups must stay in index order, and a UDT's fields in declaration order.
' ------------------------------------------------------------------------------------------
function frmDebug_SortLess( byval wszA as DWSTRING, byval wszB as DWSTRING, byval bDesc as boolean ) as boolean
    dim as DWSTRING a = PsUCase(wszA)
    dim as DWSTRING b = PsUCase(wszB)
    if a = b then return false
    if bDesc then return (a > b)
    return (a < b)
end function

' The sort key for one node under a given column.
private function frmDebug_SortKey( byval pNode as DBGP_NODE ptr, byval nCol as long ) as DWSTRING
    select case nCol
        case 1  : return pNode->wszValue
        case 2  : return pNode->wszType
        case else : return pNode->wszName
    end select
end function

private sub frmDebug_SortNodes( byval pn as DBGP_NODE ptr, byval n as long, byref srt as FRMDEBUG_SORT )
    if srt.nCol < 0 then exit sub
    if n < 2 then exit sub
    ' Insertion sort: n is the number of variables in one scope -- tens, occasionally
    ' hundreds -- and it is stable, so equal keys keep the engine's order.
    for i as long = 1 to n - 1
        dim as DBGP_NODE tmp = pn[i]
        dim as DWSTRING wszKey = frmDebug_SortKey( @tmp, srt.nCol )
        dim as long j = i - 1
        do while j >= 0
            if frmDebug_SortLess( wszKey, frmDebug_SortKey( @pn[j], srt.nCol ), srt.bDesc ) = false then exit do
            pn[j + 1] = pn[j]
            j -= 1
        loop
        pn[j + 1] = tmp
    next
end sub

' ------------------------------------------------------------------------------------------
' The three fills. Each owns ONE tree; there is no shared "which tab is showing" state left.
' ------------------------------------------------------------------------------------------
private sub frmDebug_FillVarList( byval hLV as HWND, byval bGlobals as boolean, byref srt as FRMDEBUG_SORT )
    if IsWindow(hLV) = 0 then exit sub
    dim as long nSlot = iif( bGlobals, 0, 1 )
    frmDebug_CaptureExpanded( nSlot, hLV )
    PsListTree_Clear( hLV )

    dim as long n
    if bGlobals then
        n = debugparser_global_count()
    else
        n = debugparser_local_count( gDbgCurFrame )
    end if
    if n <= 0 then
        PsListTree_Refresh( hLV )
        exit sub
    end if

    ' Fetch the whole scope first so the sort can run on the model.
    dim as DBGP_NODE nodes(0 to n - 1)
    dim as long have = 0
    for i as long = 0 to n - 1
        dim as long rc
        if bGlobals then
            rc = debugparser_global_node( i, @nodes(have) )
        else
            rc = debugparser_local_node( gDbgCurFrame, i, @nodes(have) )
        end if
        if rc = DBGP_OK then have += 1
    next
    if have > 1 then frmDebug_SortNodes( @nodes(0), have, srt )

    ' The scope is part of the identity: a local named "i" in one frame is not the local
    ' named "i" in another, and marking one changed because the other differs is a lie.
    dim as DWSTRING wszScope = "G:"
    if bGlobals = false then wszScope = "L" & str(gDbgCurFrame) & ":"

    for i as long = 0 to have - 1
        if frmDebug_AddNodeRow( hLV, -1, @nodes(i), wszScope & nodes(i).wszName, true ) < 0 then exit for
    next
    PsListTree_Refresh( hLV )
    frmDebug_RestoreExpanded( nSlot, hLV )
end sub

sub frmDebug_FillGlobals()
    frmDebug_FillVarList( HWND_FRMDEBUG_LVGLOBALS, true, gDbgSortGlobals )
end sub

sub frmDebug_FillLocals()
    frmDebug_FillVarList( HWND_FRMDEBUG_LVLOCALS, false, gDbgSortLocals )
end sub

sub frmDebug_FillWatch()
    dim as HWND hLV = HWND_FRMDEBUG_LVWATCH
    if IsWindow(hLV) = 0 then exit sub
    frmDebug_CaptureExpanded( 2, hLV )
    PsListTree_Clear( hLV )

    for i as long = 0 to gDbgWatchCount - 1
        dim as DBGP_NODE nd
        dim as wstring * 512 wexpr = gDbgWatch(i)
        dim as long r
        if debugparser_evaluate( @wexpr, gDbgCurFrame, @nd ) = DBGP_OK then
            ' The row's caption is what the USER typed, not the name the engine resolved it
            ' to -- editing the row has to hand the editor back the same text.
            r = frmDebug_AddNodeRow( hLV, -1, @nd, "W:" & gDbgWatch(i), true )
            if r < 0 then exit for
            PsListTree_SetText( hLV, r, gDbgWatch(i) )
        else
            r = PsListTree_AddNode( hLV, -1, gDbgWatch(i), FRMDEBUG_NOROW )
            if r < 0 then exit for
            ' Out of scope is the NORMAL state for a watch on a local while execution is
            ' somewhere else, so it is reported as such rather than as an error.
            PsListTree_SetCellText( hLV, r, 1, L(455, "<not in scope>") )
        end if
    next

    ' A watch is added with the + in the WATCH caption band, NOT with a trailing prompt row.
    ' The prompt row was tried and removed: it depended on click-to-edit, which PsListTree
    ' applies before a host can veto a click, so it fought the delete X and opened an editor
    ' on rows it should not have. A button is a button.
    PsListTree_Refresh( hLV )
    frmDebug_RestoreExpanded( 2, hLV )
    if IsWindow(gDbgBtnAddWatch) then
        PsButton_SetEnabled( gDbgBtnAddWatch, (gDbgWatchCount < FRMDEBUG_MAXWATCH) )
    end if
    frmDebug_SyncWatchButtons()
end sub

' The + beside the WATCH caption. Appends an empty expression and opens its editor straight
' away, so the gesture is one click and then type.
private sub frmDebug_AddWatchClick( byval hButton as HWND, byval id as long )
    if gDbgWatchCount >= FRMDEBUG_MAXWATCH then exit sub
    gDbgWatch(gDbgWatchCount) = ""
    gDbgWatchCount += 1
    frmDebug_FillWatch()

    dim as long r = gDbgWatchCount - 1
    if IsWindow(HWND_FRMDEBUG_LVWATCH) andalso (r >= 0) then
        PsListTree_SetCurSel( HWND_FRMDEBUG_LVWATCH, r )
        PsListTree_BeginEdit( HWND_FRMDEBUG_LVWATCH, r, 0 )
    end if
end sub

' Every variables pane, in one call. Used by the stop handler and by anything that re-scopes.
'
' The descriptor array is reset HERE and only here. It is append-only by design -- an index
' already handed to a row's itemData has to stay valid -- so something has to drop the lot,
' and the only safe moment is when every tree that could be holding an index is about to be
' rebuilt. That is exactly this function. A single-tree refill (a header click re-sorting
' globals) appends instead, which is bounded by how often a person clicks a header; without
' this reset a long stepping session would accumulate a DBGP_NODE per row per stop.
private sub frmDebug_FillVariables()
    ' Capture BEFORE the reset: the stash is read out of the descriptors, and the reset is
    ' what destroys them. Each fill then calls capture again and finds a row count of zero,
    ' which leaves its stash intact -- see frmDebug_CaptureExpanded.
    frmDebug_CaptureExpanded( 0, HWND_FRMDEBUG_LVGLOBALS )
    frmDebug_CaptureExpanded( 1, HWND_FRMDEBUG_LVLOCALS )
    frmDebug_CaptureExpanded( 2, HWND_FRMDEBUG_LVWATCH )
    frmDebug_RowsReset()
    frmDebug_FillGlobals()
    frmDebug_FillLocals()
    frmDebug_FillWatch()
end sub

' A header click sorts that tree. First click on a column is ascending, a second on the same
' column reverses it -- the indicator is painted by frmDebug_PaintVarHeader.
private sub frmDebug_HeaderClick( byval hHeader as HWND, byval idx as integer )
    dim as HWND hLV = GetParent( hHeader )
    if IsWindow(hLV) = 0 then exit sub

    dim pSort as FRMDEBUG_SORT ptr
    if hLV = HWND_FRMDEBUG_LVGLOBALS then
        pSort = @gDbgSortGlobals
    elseif hLV = HWND_FRMDEBUG_LVLOCALS then
        pSort = @gDbgSortLocals
    else
        exit sub
    end if

    if pSort->nCol = idx then
        pSort->bDesc = not pSort->bDesc
    else
        pSort->nCol  = idx
        pSort->bDesc = false
    end if

    if hLV = HWND_FRMDEBUG_LVGLOBALS then
        frmDebug_FillGlobals()
    else
        frmDebug_FillLocals()
    end if
    AfxRedrawWindow( hLV )
end sub

' Removes the watch a row stands for and rebuilds. Only a top-level row IS a watch; an
' expanded child of one is a field of the value, not an expression.
private sub frmDebug_DeleteWatch( byval row as long )
    if (row < 0) orelse (row >= gDbgWatchCount) then exit sub
    for k as long = row to gDbgWatchCount - 2
        gDbgWatch(k) = gDbgWatch(k + 1)
    next
    gDbgWatchCount -= 1
    frmDebug_FillWatch()
end sub

' The trashcan is enabled only while a deletable row is selected, so the button itself says
' whether the gesture is available rather than the click silently doing nothing.
private sub frmDebug_SyncWatchButtons()
    if IsWindow(gDbgBtnDelWatch) = 0 then exit sub
    dim as long r = -1
    if IsWindow(HWND_FRMDEBUG_LVWATCH) then r = PsListTree_GetCurSel( HWND_FRMDEBUG_LVWATCH )
    PsButton_SetEnabled( gDbgBtnDelWatch, (r >= 0) andalso (r < gDbgWatchCount) )
end sub

private sub frmDebug_WatchSelChange( byval hListControl as HWND, byval row as integer )
    frmDebug_SyncWatchButtons()
end sub

' The trashcan beside the + in the WATCH caption. Acts on the SELECTED row.
private sub frmDebug_DelWatchClick( byval hButton as HWND, byval id as long )
    if IsWindow(HWND_FRMDEBUG_LVWATCH) = 0 then exit sub
    frmDebug_DeleteWatch( PsListTree_GetCurSel( HWND_FRMDEBUG_LVWATCH ) )
end sub

' Double-click, or ENTER on the focused row -- one callback for both, which is what
' ActivateCallback is. Only a row that IS a watch can be edited; an expanded child of one is
' a field of the value, not an expression.
private sub frmDebug_WatchActivate( byval hListControl as HWND, byval row as integer )
    if (row < 0) orelse (row >= gDbgWatchCount) then exit sub
    PsListTree_BeginEdit( hListControl, row, 0 )
end sub

' Committing an in-place edit. An empty result DELETES the watch, which is how a row is
' removed without a menu; anything else replaces or appends.
private function frmDebug_EndLabelEdit( byval hListControl as HWND, byval row as integer, _
                                        byval newText as DWSTRING ) as boolean
    dim as DWSTRING wszNew = PsTrim(newText)

    if (row >= 0) andalso (row < gDbgWatchCount) then
        if PsLen(wszNew) = 0 then
            for k as long = row to gDbgWatchCount - 2
                gDbgWatch(k) = gDbgWatch(k + 1)
            next
            gDbgWatchCount -= 1
        else
            gDbgWatch(row) = wszNew
        end if
    elseif PsLen(wszNew) then
        if gDbgWatchCount < FRMDEBUG_MAXWATCH then
            gDbgWatch(gDbgWatchCount) = wszNew
            gDbgWatchCount += 1
        end if
    end if

    ' Re-evaluate and rebuild, so the new expression shows a value immediately rather than
    ' waiting for the next stop.
    frmDebug_FillVariables()
    return true
end function

' ==========================================================================================
' Command enable state. Everything except Stop and Break is meaningless while the debuggee
' is executing, and Break is meaningless while it is stopped.
' ==========================================================================================
private sub frmDebug_SyncButtons()
    dim as boolean bActive  = (debugparser_is_active() <> 0)
    dim as boolean bRunning = (debugparser_is_running() <> 0)
    dim as boolean bStopped = false
    if bActive andalso (bRunning = false) then bStopped = true

    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) = 0 then continue for
        dim as boolean bEnable
        select case id
            case IDC_FRMDEBUG_CMDSTOP
                bEnable = bActive
            case IDC_FRMDEBUG_CMDBREAK
                bEnable = false
                if bActive andalso bRunning then bEnable = true
            case else
                bEnable = bStopped
        end select
        PsButton_SetEnabled( gDbgBtn(id), bEnable )
    next
end sub

' ==========================================================================================
' Stop / exit handling
' ==========================================================================================
private sub frmDebug_OnStopped()
    dim as DBGP_STOPINFO si
    if debugparser_get_stop( @si ) <> DBGP_OK then exit sub

    ' ---- RUN TO THE FIRST BREAKPOINT ----
    '
    ' The engine always posts its first stop at the program's first executable line, not at a
    ' breakpoint (dbgprocess arms the line traps after the loader's own breakpoint). If the
    ' user set breakpoints, stopping at the entry is not what they asked for, so resume
    ' straight through it -- WITHOUT painting: no execution arrow, no variable fill, no
    ' status line. A stop the user never asked to see should leave no trace of having
    ' happened. With no breakpoints armed, stopping at the start IS the useful default and
    ' nothing here changes.
    '
    ' Host-side rather than an engine change: the engine's stop state machine is what its
    ' headless harness tests, and "which stop is uninteresting" is a UI policy.
    if gDbgFirstStop then
        gDbgFirstStop = false
        if gDbgHaveBreakpoints then
            debugparser_resume( DBGP_RUN_CONTINUE )
            frmDebug_SetStatus( L(432, "Running...") )
            frmDebug_SyncButtons()
            exit sub
        end if
    end if

    ' What the previous stop showed becomes what "changed" is measured against. Done HERE,
    ' at the start of the stop, rather than at the end: a fill triggered later within this
    ' same stop -- clicking a stack frame, expanding a node -- must still compare against the
    ' previous stop, not against itself.
    frmDebug_ValueRoll()

    gDbgCurFrame = 0

    dim as DWSTRING wszWhere
    if len(si.wszSrcPath) then
        wszWhere = PsPathName(si.wszSrcPath ) & ":" & str(si.lineNum)
        frmDebug_ShowExecutionPoint( si.wszSrcPath, si.lineNum )
    else
        wszWhere = "&h" & hex(si.addr)
    end if

    dim as DWSTRING wszMsg = frmDebug_ReasonText( si.reason ) & "   " & wszWhere
    if len(si.wszProcName) then wszMsg = wszMsg & "   " & L(440, "in") & " " & si.wszProcName
    if si.reason = DBGP_STOP_EXCEPTION then
        wszMsg = wszMsg & "   " & si.wszExcepText
        if si.excepCode = &hC0000005 then
            dim as DWSTRING wszDir = L(442, "reading")
            if si.excepIsWrite <> 0 then wszDir = L(441, "writing")
            wszMsg = wszMsg & " (" & wszDir & " &h" & hex(si.excepTarget) & ")"
        end if
    end if
    frmDebug_SetStatus( wszMsg )

    frmDebug_HideDataTip()
    frmDebug_FillCallStack( si.frameCount )
    frmDebug_FillVariables()
    frmDebug_SyncButtons()
    frmDebug_Trace()

    ' TIKO_DEBUG_AUTOSTEP=n -- step n times and stop, unattended.
    '
    ' It exists for ONE reason: the changed-value highlight is only observable across TWO
    ' stops, and nothing else here can reach a second one without a person pressing a key. It
    ' is what showed the highlight correctly tracking a loop counter over 14 steps -- and, on
    ' the same run, showed a dynamic array reading as eight junk dimensions before its REDIM
    ' had executed, which was a real engine bug and is now rejected as "<not allocated>".
    ' Pair it with TIKO_DEBUG_AUTORUN and TIKO_DEBUG_TRACEEXPAND.
    if len(environ("TIKO_DEBUG_AUTOSTEP")) then
        static as long nAutoSteps
        nAutoSteps += 1
        if nAutoSteps < valint(environ("TIKO_DEBUG_AUTOSTEP")) then
            debugparser_resume( DBGP_RUN_STEPINTO )
        end if
    end if
end sub

private sub frmDebug_OnExited( byval exitCode as long )
    gDbgSessionEnding = true
    gApp.IsDebuggerActive = false
    frmDebug_ClearExecutionMarkers()

    if exitCode = 0 then
        SetCompileStatusBarMessage( L(253, "Debug Target Exited Normally"), ghIconGood )
    else
        SetCompileStatusBarMessage( L(254, "Debug Target Terminated Unexpectantly"), ghIconBad )
    end if

    frmMain_DebugTopMenuStates()
    if IsWindow(HWND_FRMDEBUG) then DestroyWindow( HWND_FRMDEBUG )
end sub

' ==========================================================================================
' Commands. Reached from the toolbar, the Debug menu and the accelerators alike.
' ==========================================================================================
sub frmDebug_Command( byval id as long )
    if debugparser_is_active() = 0 then exit sub

    select case id
        case IDC_FRMDEBUG_CMDCONTINUE, IDM_DEBUG_STARTDEBUGGING
            debugparser_resume( DBGP_RUN_CONTINUE )
        case IDC_FRMDEBUG_CMDSTEPINTO, IDM_DEBUG_STEPINTO
            debugparser_resume( DBGP_RUN_STEPINTO )
        case IDC_FRMDEBUG_CMDSTEPOVER, IDM_DEBUG_STEPOVER
            debugparser_resume( DBGP_RUN_STEPOVER )
        case IDC_FRMDEBUG_CMDSTEPOUT, IDM_DEBUG_STEPOUT
            debugparser_resume( DBGP_RUN_STEPOUT )
        case IDC_FRMDEBUG_CMDBREAK, IDM_DEBUG_PAUSE
            debugparser_pause()
            frmDebug_SetStatus( L(434, "Pausing at the next source line...") )
            exit sub
        case IDC_FRMDEBUG_CMDSTOP, IDM_DEBUG_STOPDEBUGGING
            debugparser_stop()
            exit sub
        case IDC_FRMDEBUG_CMDRUNCURSOR, IDM_DEBUG_RUNTOCURSOR
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc = 0 then exit sub
            dim as wstring * 520 wszFile = pDoc->DiskFilename
            dim as long si = debugparser_source_index( @wszFile )
            if si < 0 then exit sub
            dim as long ln = debugparser_next_executable_line( si, pDoc->GetCurrentLineNumber + 1 )
            if ln < 0 then exit sub
            debugparser_set_run_to_cursor( si, ln )
            debugparser_resume( DBGP_RUN_TOCURSOR )
    end select

    frmDebug_SetStatus( L(432, "Running...") )
    frmDebug_SyncButtons()
end sub

private sub frmDebug_ButtonClick( byval hButton as HWND, byval id as long )
    frmDebug_Command( id )
end sub

' Selecting a call-stack frame re-scopes the variables view to it and navigates the editor.
' Selecting a call-stack frame RE-SCOPES the whole window to it: the Locals pane shows that
' frame's variables, the Watch pane re-evaluates its expressions against it, and the editor
' navigates to the call site. Globals are unaffected -- they have no frame.
'
' The callback is handed the CONTROL (PsListTree passes pList->hWin), so the old GetParent
' here resolved to the debugger WINDOW, which is a live HWND and therefore passed the
' IsWindow guard -- every GetItemData then ran against the wrong window and answered 0. The
' frame index was always 0, so clicking a stack row did nothing at all.
private sub frmDebug_StackSelChange( byval hListControl as HWND, byval row as integer )
    if row < 0 then exit sub
    dim as long frameIndex = cast(long, PsListTree_GetItemData( hListControl, row ))
    if frameIndex < 0 then exit sub

    gDbgCurFrame = frameIndex
    dim as DBGP_FRAME fr
    if debugparser_get_frame( frameIndex, @fr ) = DBGP_OK then
        if (len(fr.wszSrcPath) > 0) andalso (fr.lineNum > 0) then
            frmDebug_ShowExecutionPoint( fr.wszSrcPath, fr.lineNum )
        end if
    end if
    frmDebug_FillVariables()
end sub

' ==========================================================================================
' Layout
' ==========================================================================================
private sub frmDebug_PositionWindows()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long nMargin  = pWindow->ScaleX(12)
    dim as long nToolH   = pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT)
    dim as long nStatusH = pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT)
    dim as long nCapH    = pWindow->ScaleY(FRMDEBUG_CAPHEIGHT)
    dim as long nSplitW  = pWindow->ScaleX(FRMDEBUG_SPLITWIDTH)
    dim as long nMinPane = pWindow->ScaleX(FRMDEBUG_MINPANE)
    dim as long nMinPaneV = pWindow->ScaleY(FRMDEBUG_MINPANEV) + nCapH

    ' --- toolbar: buttons left to right at their ideal widths ---
    dim as long x = nMargin
    dim as long y = pWindow->ScaleY(6)
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) = 0 then continue for
        dim as long bw, bh
        PsButton_GetIdealSize( gDbgBtn(id), bw, bh )
        SetWindowPos( gDbgBtn(id), 0, x, y, bw, bh, SWP_NOZORDER )
        x += bw + pWindow->ScaleX(6)
        ' A wider gap after Stop and after Step Out groups the seven into three sets.
        if (id = IDC_FRMDEBUG_CMDSTOP) orelse (id = IDC_FRMDEBUG_CMDSTEPOUT) then
            x += pWindow->ScaleX(14)
        end if
    next

    dim as long nTop = nToolH + nStatusH

    ' --- splitter ranges. Recomputed here rather than derived by the controls, which cannot
    ' know what else lives in this client area -- PsSplitter.bi states that. The two
    ' horizontal bars must be re-ranged on EVERY pass and not only on a resize: their span is
    ' the content area, which the vertical bar does not change, but the window height does.
    dim as long nMinBar = nMinPane
    dim as long nMaxBar = rc.right - nMinPane - nSplitW
    if nMaxBar < nMinBar then nMaxBar = nMinBar

    dim as long nMinBarV = nTop + nMinPaneV
    dim as long nMaxBarV = rc.bottom - nMinPaneV - nSplitW
    if nMaxBarV < nMinBarV then nMaxBarV = nMinBarV

    ' The stored positions have to be applied BEFORE SetRange, not after: SetRange clamps the
    ' current position into the new range silently, so by the time it returns the bar is
    ' never out of range and an "is it out of range" test can never fire.
    ' Guarded on the splitters EXISTING, not just on the flag. SetClientSize and Center both
    ' fire WM_SIZE, which lays out -- so an unguarded one-shot is spent on a pass that runs
    ' before these controls are created, and the defaults silently never apply.
    dim as boolean bHaveSplitters = (IsWindow(HWND_FRMDEBUG_SPLITMAIN) <> 0) andalso _
                                    (IsWindow(HWND_FRMDEBUG_SPLITLEFT) <> 0) andalso _
                                    (IsWindow(HWND_FRMDEBUG_SPLITRIGHT) <> 0)
    if bHaveSplitters andalso (gDbgSplitInit = false) then
        gDbgSplitInit = true
        PsSplitter_SetPos( HWND_FRMDEBUG_SPLITMAIN, _
                           frmDebug_PctToPos( gConfig.DebugSplitMain, 0, rc.right ) )
        PsSplitter_SetPos( HWND_FRMDEBUG_SPLITLEFT, _
                           frmDebug_PctToPos( gConfig.DebugSplitLeft, nTop, rc.bottom ) )
        PsSplitter_SetPos( HWND_FRMDEBUG_SPLITRIGHT, _
                           frmDebug_PctToPos( gConfig.DebugSplitRight, nTop, rc.bottom ) )
    end if
    PsSplitter_SetRange( HWND_FRMDEBUG_SPLITMAIN,  nMinBar,  nMaxBar )
    PsSplitter_SetRange( HWND_FRMDEBUG_SPLITLEFT,  nMinBarV, nMaxBarV )
    PsSplitter_SetRange( HWND_FRMDEBUG_SPLITRIGHT, nMinBarV, nMaxBarV )

    frmDebug_LayoutPanes()
end sub

' Everything that moves when a splitter does, placed in ONE atomic pass.
'
' Separate SetWindowPos calls each repaint as they land, and during a drag that runs on every
' mouse move -- which is what tore. DeferWindowPos moves them together, so the panes change
' size once per message instead of seven times. The caption bands are painted rather than
' being windows, so only the four trees and three bars are in the pass.
'
' The bar positions are read back from the CONTROLS rather than passed in: a splitter has
' already moved itself by the time its callback fires, and the other two have not moved at
' all, so asking each one is the only way to get three positions that are all current.
private sub frmDebug_LayoutPanes()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as FRMDEBUG_LAYOUT lo
    frmDebug_ComputeLayout( rc.right, rc.bottom, _
                            PsSplitter_GetPos( HWND_FRMDEBUG_SPLITMAIN ), _
                            PsSplitter_GetPos( HWND_FRMDEBUG_SPLITLEFT ), _
                            PsSplitter_GetPos( HWND_FRMDEBUG_SPLITRIGHT ), _
                            pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT), _
                            pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT), _
                            pWindow->ScaleY(FRMDEBUG_CAPHEIGHT), _
                            pWindow->ScaleX(FRMDEBUG_SPLITWIDTH), lo )

    dim as any ptr hDWP = BeginDeferWindowPos( 7 )
    if hDWP = 0 then exit sub
    dim as long fl = SWP_NOZORDER or SWP_NOACTIVATE

    #macro DWPRECT( h, r )
        hDWP = DeferWindowPos( hDWP, h, 0, (r).left, (r).top, _
                               (r).right - (r).left, (r).bottom - (r).top, fl )
    #endmacro
    DWPRECT( HWND_FRMDEBUG_SPLITMAIN,  lo.rcSplitMain )
    DWPRECT( HWND_FRMDEBUG_SPLITLEFT,  lo.rcSplitLeft )
    DWPRECT( HWND_FRMDEBUG_SPLITRIGHT, lo.rcSplitRight )
    DWPRECT( HWND_FRMDEBUG_LVGLOBALS,  lo.rcGlobals )
    DWPRECT( HWND_FRMDEBUG_LVLOCALS,   lo.rcLocals )
    DWPRECT( HWND_FRMDEBUG_LVSTACK,    lo.rcStack )
    DWPRECT( HWND_FRMDEBUG_LVWATCH,    lo.rcWatch )
    if hDWP then EndDeferWindowPos( hDWP )

    ' The watch commands sit at the RIGHT end of the WATCH caption band, square and inset,
    ' so they never collide with the caption text however long a translation of "WATCH" turns
    ' out to be. Laid out right to left, so + is leftmost and the trashcan outermost -- the
    ' destructive one furthest from where the eye lands.
    scope
        dim as long nPad  = pWindow->ScaleY(4)
        dim as long nSide = (lo.rcCapWatch.bottom - lo.rcCapWatch.top) - nPad * 2
        if nSide < 1 then nSide = 1
        dim as long x = lo.rcCapWatch.right - pWindow->ScaleX(8) - nSide
        dim as long y = lo.rcCapWatch.top + nPad
        if IsWindow(gDbgBtnDelWatch) then
            SetWindowPos( gDbgBtnDelWatch, 0, x, y, nSide, nSide, SWP_NOZORDER or SWP_NOACTIVATE )
            x -= nSide + pWindow->ScaleX(4)
        end if
        if IsWindow(gDbgBtnAddWatch) then
            SetWindowPos( gDbgBtnAddWatch, 0, x, y, nSide, nSide, SWP_NOZORDER or SWP_NOACTIVATE )
        end if
    end scope

    ' The caption bands are painted, so moving a bar has to invalidate them explicitly --
    ' the trees repaint themselves, the bands do not.
    AfxRedrawWindow( HWND_FRMDEBUG )
end sub

' ==========================================================================================
' Collapse to the icon strip, and come back. The window is never made ICONIC -- see the .bi.
' ==========================================================================================
function frmDebug_IsCollapsed() as boolean
    return gDbgCollapsed
end function

' ------------------------------------------------------------------------------------------
' The strip's client size. PURE.
' ------------------------------------------------------------------------------------------
sub frmDebug_ComputeStripSize( byval nSumW as long, byval nMaxH as long, byval nCount as long, _
                               byval nPad as long, byval nGap as long, _
                               byref cx as long, byref cy as long )
    dim as long nGaps = 0
    if nCount > 1 then nGaps = (nCount - 1) * nGap
    cx = nPad * 2 + nSumW + nGaps
    cy = nPad * 2 + nMaxH
end sub

' ------------------------------------------------------------------------------------------
' Where the strip sits. PURE. Top-RIGHT of the editing area, inset by the margin.
' ------------------------------------------------------------------------------------------
sub frmDebug_ComputeStubRect( byref rcAnchor as RECT, _
                              byval cx as long, byval cy as long, _
                              byval nMargin as long, byref rcOut as RECT )
    dim as long x = rcAnchor.right - nMargin - cx
    dim as long y = rcAnchor.top + nMargin

    ' An editing area narrower than the strip would push it off the LEFT edge; clamp so it
    ' stays reachable rather than sliding out of the anchor entirely.
    if x < rcAnchor.left then x = rcAnchor.left
    if y < rcAnchor.top then y = rcAnchor.top

    SetRect( @rcOut, x, y, x + cx, y + cy )
end sub

' The editing area, in SCREEN coords: the active document's Scintilla, falling back to
' frmMain's client area when there is no document, so the strip is always placed against
' something real.
private sub frmDebug_AnchorRect( byref rcOut as RECT )
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        if IsWindow( pDoc->hWndActiveScintilla ) then
            GetWindowRect( pDoc->hWndActiveScintilla, @rcOut )
            exit sub
        end if
    end if

    GetClientRect( HWND_FRMMAIN, @rcOut )
    dim as POINT pt = ( rcOut.left, rcOut.top )
    ClientToScreen( HWND_FRMMAIN, @pt )
    SetRect( @rcOut, pt.x, pt.y, pt.x + (rcOut.right - rcOut.left), _
                     pt.y + (rcOut.bottom - rcOut.top) )
end sub

' The captions come off the toolbar buttons while collapsed and go back on when restored.
' PsButton measures an empty caption as ICON ONLY -- no text cell and no gap either side --
' so this is what turns the seven-word toolbar into a row of square icons, and the ideal
' sizes read back afterwards are the icon-only ones.
private sub frmDebug_SetButtonCaption( byval id as long, byval wszText as DWSTRING, byval bShow as boolean )
    if IsWindow(gDbgBtn(id)) = 0 then exit sub
    if bShow then
        PsButton_SetText( gDbgBtn(id), wszText )
    else
        PsButton_SetText( gDbgBtn(id), "" )
    end if
end sub

private sub frmDebug_SetToolbarCaptions( byval bShow as boolean )
    ' The same strings the buttons were created with. Re-resolved through L() rather than
    ' cached at creation, so a language switch while the debugger is open cannot restore the
    ' previous language's captions.
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDCONTINUE,  L(453, "Continue"),      bShow )
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDBREAK,     L(433, "Break"),         bShow )
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDSTOP,      L(454, "Stop"),          bShow )
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDSTEPINTO,  L(243, "Step"),          bShow )
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDSTEPOVER,  L(244, "Step Over"),     bShow )
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDSTEPOUT,   L(251, "Step Out"),      bShow )
    frmDebug_SetButtonCaption( IDC_FRMDEBUG_CMDRUNCURSOR, L(252, "Run to Cursor"), bShow )
end sub

' Everything that is NOT in the strip. Hidden rather than left behind the strip's paint: a
' tree that is still there is still hit-testable, and a splitter still draggable, in a client
' area far too small to hold either.
private sub frmDebug_SetPanesVisible( byval bShow as boolean )
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    #macro SHOWIF( h )
        if IsWindow(h) then ShowWindow( h, nCmd )
    #endmacro
    SHOWIF( HWND_FRMDEBUG_LVGLOBALS )
    SHOWIF( HWND_FRMDEBUG_LVLOCALS )
    SHOWIF( HWND_FRMDEBUG_LVSTACK )
    SHOWIF( HWND_FRMDEBUG_LVWATCH )
    SHOWIF( HWND_FRMDEBUG_SPLITMAIN )
    SHOWIF( HWND_FRMDEBUG_SPLITLEFT )
    SHOWIF( HWND_FRMDEBUG_SPLITRIGHT )
    SHOWIF( gDbgBtnAddWatch )
    SHOWIF( gDbgBtnDelWatch )
    SHOWIF( gDbgBtn(IDC_FRMDEBUG_CMDBREAK) )
end sub

' The strip's client size, measured from the controls themselves rather than from constants:
' the icon cell, the padding and the focus-ring band are PsButton's, not this window's, and a
' strip sized from a guess at them clips its last icon at some DPI.
private sub frmDebug_StripMetrics( byref cx as long, byref cy as long )
    cx = 0 : cy = 0
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as long nSumW = 0, nMaxH = 0, nCount = 0
    #macro MEASURE( h )
        if IsWindow(h) then
            dim as long bw, bh
            PsButton_GetIdealSize( h, bw, bh )
            nSumW += bw
            if bh > nMaxH then nMaxH = bh
            nCount += 1
        end if
    #endmacro
    for i as long = lbound(gDbgStripIds) to ubound(gDbgStripIds)
        MEASURE( gDbgBtn(gDbgStripIds(i)) )
    next
    MEASURE( gDbgBtnRestore )

    frmDebug_ComputeStripSize( nSumW, nMaxH, nCount, _
                               pWindow->ScaleX(FRMDEBUG_STUBPAD), _
                               pWindow->ScaleX(FRMDEBUG_STUBGAP), cx, cy )
end sub

' Place the strip's icons. Left to right at their ideal sizes, vertically centred in whatever
' client the window happens to have -- the frame is resizable, so this must not assume the
' client is the size Collapse asked for.
private sub frmDebug_LayoutStrip()
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long nPad = pWindow->ScaleX(FRMDEBUG_STUBPAD)
    dim as long nGap = pWindow->ScaleX(FRMDEBUG_STUBGAP)
    dim as long x    = nPad

    #macro PLACE( h )
        if IsWindow(h) then
            dim as long bw, bh
            PsButton_GetIdealSize( h, bw, bh )
            dim as long y = (rc.bottom - bh) \ 2
            if y < 0 then y = 0
            SetWindowPos( h, 0, x, y, bw, bh, SWP_NOZORDER or SWP_NOACTIVATE )
            x += bw + nGap
        end if
    #endmacro
    for i as long = lbound(gDbgStripIds) to ubound(gDbgStripIds)
        PLACE( gDbgBtn(gDbgStripIds(i)) )
    next
    PLACE( gDbgBtnRestore )
end sub

sub frmDebug_Collapse()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    if gDbgCollapsed then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    ' remember where to come back to BEFORE moving anything
    GetWindowRect( HWND_FRMDEBUG, @gDbgRestoreRect )
    gDbgCollapsed = true

    frmDebug_SetPanesVisible( false )
    frmDebug_SetToolbarCaptions( false )
    if IsWindow(gDbgBtnRestore) then ShowWindow( gDbgBtnRestore, SW_SHOW )

    dim as long cxClient, cyClient
    frmDebug_StripMetrics( cxClient, cyClient )
    if (cxClient <= 0) orelse (cyClient <= 0) then exit sub

    ' The strip is sized by its CLIENT: this window has a caption and a sizing frame, so the
    ' window rect is that client plus a non-client border which is the frame's business to
    ' report, not ours to assume.
    dim as RECT rcW
    SetRect( @rcW, 0, 0, cxClient, cyClient )
    AdjustWindowRectEx( @rcW, GetWindowLongPtr( HWND_FRMDEBUG, GWL_STYLE ), false, _
                        GetWindowLongPtr( HWND_FRMDEBUG, GWL_EXSTYLE ) )

    dim as RECT rcAnchor, rcStub
    frmDebug_AnchorRect( rcAnchor )
    frmDebug_ComputeStubRect( rcAnchor, rcW.right - rcW.left, rcW.bottom - rcW.top, _
                              pWindow->ScaleX(FRMDEBUG_STUBMARGIN), rcStub )

    SetWindowPos( HWND_FRMDEBUG, 0, rcStub.left, rcStub.top, _
                  rcStub.right - rcStub.left, rcStub.bottom - rcStub.top, SWP_NOZORDER )
    frmDebug_LayoutStrip()
    AfxRedrawWindow( HWND_FRMDEBUG )
end sub

private sub frmDebug_RestoreFromStub()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    if gDbgCollapsed = false then exit sub

    gDbgCollapsed = false
    if IsWindow(gDbgBtnRestore) then ShowWindow( gDbgBtnRestore, SW_HIDE )
    frmDebug_SetToolbarCaptions( true )
    frmDebug_SetPanesVisible( true )

    SetWindowPos( HWND_FRMDEBUG, 0, gDbgRestoreRect.left, gDbgRestoreRect.top, _
                  gDbgRestoreRect.right - gDbgRestoreRect.left, _
                  gDbgRestoreRect.bottom - gDbgRestoreRect.top, SWP_NOZORDER )

    ' the bands are sized from the client, which has just changed
    frmDebug_PositionWindows()
    ' Break comes back visible, and its enable state is a property of the session rather than
    ' of the collapse -- so it is re-derived rather than assumed.
    frmDebug_SyncButtons()
    AfxRedrawWindow( HWND_FRMDEBUG )
end sub

private sub frmDebug_RestoreClick( byval hButton as HWND, byval id as long )
    frmDebug_RestoreFromStub()
end sub

' Env-gated geometry dump (TIKO_DEBUGWIN_TRACE=1). Every rect this window computes, read
' back from the CONTROLS rather than from the variables that placed them -- a layout bug
' that leaves a control at 0x0 is invisible to a trace of the intended numbers.
sub frmDebug_Trace()
    if environ("TIKO_DEBUGWIN_TRACE") <> "1" then exit sub
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as string sLine
    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    sLine = "--- TIKO_DEBUGWIN_TRACE ---"
    print sLine
    sLine = "  DPI rx=" & str(pWindow->rxRatio) & " ry=" & str(pWindow->ryRatio) & _
            "   client=" & str(rc.right) & "x" & str(rc.bottom)
    print sLine

    #macro TRACEWND( nm, h )
        if IsWindow(h) then
            dim as RECT r2
            GetWindowRect( h, @r2 )
            MapWindowPoints( HWND_DESKTOP, HWND_FRMDEBUG, cast(LPPOINT, @r2), 2 )
            sLine = "  " & nm & " = (" & str(r2.left) & "," & str(r2.top) & ")-(" & _
                    str(r2.right) & "," & str(r2.bottom) & ")  " & _
                    str(r2.right - r2.left) & "x" & str(r2.bottom - r2.top) & _
                    iif( IsWindowVisible(h), "  visible", "  HIDDEN" )
            print sLine
        else
            sLine = "  " & nm & " = NOT A WINDOW"
            print sLine
        end if
    #endmacro

    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        TRACEWND( "button" & str(id), gDbgBtn(id) )
    next
    TRACEWND( "splitmain ", HWND_FRMDEBUG_SPLITMAIN )
    TRACEWND( "splitleft ", HWND_FRMDEBUG_SPLITLEFT )
    TRACEWND( "splitright", HWND_FRMDEBUG_SPLITRIGHT )
    TRACEWND( "globals   ", HWND_FRMDEBUG_LVGLOBALS )
    TRACEWND( "locals    ", HWND_FRMDEBUG_LVLOCALS )
    TRACEWND( "stack     ", HWND_FRMDEBUG_LVSTACK )
    TRACEWND( "watch     ", HWND_FRMDEBUG_LVWATCH )
    TRACEWND( "addwatch  ", gDbgBtnAddWatch )
    TRACEWND( "delwatch  ", gDbgBtnDelWatch )

    ' Which tooltip BACKEND each surface ended up on. A SetTooltipMode call that never landed
    ' is invisible otherwise -- the tips still work, they are simply drawn by comctl32 in the
    ' system colours, which on a dark theme is the whole thing this migration was for.
    #macro TIPMODE( nm, h, getter )
        if IsWindow(h) then
            sLine = "  tip " & nm & " = " & iif( getter(h) <> 0, "PsTooltip", "SYSTEM" )
            print sLine
        end if
    #endmacro
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        TIPMODE( "button" & str(id), gDbgBtn(id), PsButton_GetPsTooltipHandle )
    next
    TIPMODE( "addwatch  ", gDbgBtnAddWatch,         PsButton_GetPsTooltipHandle )
    TIPMODE( "delwatch  ", gDbgBtnDelWatch,         PsButton_GetPsTooltipHandle )
    TIPMODE( "globals   ", HWND_FRMDEBUG_LVGLOBALS, PsListTree_GetPsTooltipHandle )
    TIPMODE( "locals    ", HWND_FRMDEBUG_LVLOCALS,  PsListTree_GetPsTooltipHandle )
    TIPMODE( "stack     ", HWND_FRMDEBUG_LVSTACK,   PsListTree_GetPsTooltipHandle )
    TIPMODE( "watch     ", HWND_FRMDEBUG_LVWATCH,   PsListTree_GetPsTooltipHandle )
    TIPMODE( "output-lv ", HWND_FRMOUTPUT_LVRESULTS, PsListTree_GetPsTooltipHandle )
    TIPMODE( "output-todo", HWND_FRMOUTPUT_LVTODO,  PsListTree_GetPsTooltipHandle )
    TIPMODE( "output-undock", HWND_FRMOUTPUT_UNDOCK, PsIconPanel_GetPsTooltipHandle )
    TIPMODE( "output-close ", HWND_FRMOUTPUT_CLOSE,  PsIconPanel_GetPsTooltipHandle )

    sLine = "  bars: main=" & str(PsSplitter_GetPos(HWND_FRMDEBUG_SPLITMAIN)) & _
            "  left=" & str(PsSplitter_GetPos(HWND_FRMDEBUG_SPLITLEFT)) & _
            "  right=" & str(PsSplitter_GetPos(HWND_FRMDEBUG_SPLITRIGHT))
    print sLine
    sLine = "  pct:  main=" & str(frmDebug_PosToPct(PsSplitter_GetPos(HWND_FRMDEBUG_SPLITMAIN), 0, rc.right))
    print sLine
    sLine = "  rows: globals=" & str(PsListTree_GetCount(HWND_FRMDEBUG_LVGLOBALS)) & _
            "  locals=" & str(PsListTree_GetCount(HWND_FRMDEBUG_LVLOCALS)) & _
            "  stack=" & str(PsListTree_GetCount(HWND_FRMDEBUG_LVSTACK)) & _
            "  watch=" & str(PsListTree_GetCount(HWND_FRMDEBUG_LVWATCH)) & _
            "  descriptors=" & str(gDbgRowCount)
    print sLine
    ' Expand every top-level variables row and dump the resulting tree. Separate from the
    ' rect dump above because it drives real work rather than reading state, and because the
    ' path it drives -- frmDebug_ExpandRowChildren -- is otherwise reachable only by clicking
    ' a twisty, which no unattended run can do. This is what showed a 10,000-element array
    ' coming out as 100 groups ending exactly at [9900 .. 9999] and costing 224 rows.
    if environ("TIKO_DEBUG_TRACEEXPAND") = "1" then
        ' Expanded on the FIRST stop only, then merely dumped. That is what makes this a test
        ' of whether expansion SURVIVES a step: re-expanding on every stop would print an
        ' open tree either way and prove nothing.
        static as boolean bExpandedOnce

        #macro DUMPTREE( hG, nm )
            scope
                if bExpandedOnce = false then
                    for r as long = PsListTree_GetCount(hG) - 1 to 0 step -1
                        dim as long kk = cast(long, PsListTree_GetItemData( hG, r ))
                        if (kk < 0) orelse (kk >= gDbgRowCount) then continue for
                        if gDbgRow(kk).kind <> DBGROW_NODE then continue for
                        if gDbgRow(kk).nd.hasChildren = 0 then continue for
                        frmDebug_ExpandRowChildren( hG, r )
                        PsListTree_SetNodeCollapsed( hG, r, false )
                    next
                    ' ...and the FIRST index group of the first array, so the dump covers a
                    ' NESTED open node. Survival of a top-level node says nothing about one
                    ' that only exists because its parent is open.
                    for r as long = 0 to PsListTree_GetCount(hG) - 1
                        dim as long kg = cast(long, PsListTree_GetItemData( hG, r ))
                        if (kg < 0) orelse (kg >= gDbgRowCount) then continue for
                        if gDbgRow(kg).kind <> DBGROW_GROUP then continue for
                        frmDebug_ExpandRowChildren( hG, r )
                        PsListTree_SetNodeCollapsed( hG, r, false )
                        exit for
                    next
                end if
                sLine = "  -- " & nm & ", every top-level row expanded --"
                print sLine
                for r as long = 0 to PsListTree_GetCount(hG) - 1
                    ' The leading marker is the CHANGED flag -- the one thing about this
                    ' window that is otherwise only visible as a colour, and therefore the
                    ' one thing a trace can say something useful about.
                    dim as long kk2 = cast(long, PsListTree_GetItemData( hG, r ))
                    dim as string sMark = "  "
                    if (kk2 >= 0) andalso (kk2 < gDbgRowCount) then
                        if gDbgRow(kk2).bChanged then sMark = " *"
                    end if
                    sLine = sMark & " " & space(PsListTree_GetLevel(hG, r) * 2) & _
                            str(PsListTree_GetText(hG, r)) & " | " & _
                            str(PsListTree_GetCellText(hG, r, 1)) & " | " & _
                            str(PsListTree_GetCellText(hG, r, 2))
                    print sLine
                next
                sLine = "   rows=" & str(PsListTree_GetCount(hG)) & _
                        "  descriptors=" & str(gDbgRowCount)
                print sLine
            end scope
        #endmacro
        DUMPTREE( HWND_FRMDEBUG_LVGLOBALS, "globals" )
        DUMPTREE( HWND_FRMDEBUG_LVLOCALS,  "locals" )
        bExpandedOnce = true
    end if

    print "--- end ---"
end sub

private sub frmDebug_SplitterMoved( byval hSplitter as HWND, byval newPos as integer, byval nPhase as integer )
    ' Only the panes move with a bar. The toolbar and the status line do not, so a drag must
    ' not re-place seven buttons on every mouse move -- that is repaint churn with no visible
    ' effect. The control has already moved ITSELF by the time this fires; putting it back at
    ' the same coordinates is the documented harmless no-op.
    '
    ' Which bar moved is not passed on: LayoutPanes asks all three where they are, so one
    ' handler serves all three and cannot get the other two stale.
    frmDebug_LayoutPanes()
end sub

' The GRAB AREA and the RULE are different sizes, deliberately.
'
' Filling the whole bar with the divider colour -- which is what this did -- draws a 6px slab
' between every pair of panes, and three of those dominate the window. The bar keeps its full
' width so it stays easy to grab, and paints the window background with a ONE-PIXEL rule down
' its centre. Hovering thickens the rule rather than flooding the bar, so there is still
' feedback that it can be dragged.
private sub frmDebug_SplitterPaint( byval p as PSSPLITTER_PAINTINFO ptr )
    if p = 0 then exit sub

    p->b->SetBackColor( theme.output.texthot.backcolor )
    p->b->PaintRect( @p->rcClient )

    dim as boolean bLive = (p->isHot orelse p->isDragging)
    dim as long nThick = AfxScaleY(1)
    if nThick < 1 then nThick = 1
    if bLive then nThick *= 2

    dim as RECT rc = p->rcClient
    if (rc.right - rc.left) > (rc.bottom - rc.top) then
        ' A horizontal bar: the rule runs across it, centred vertically.
        dim as long mid_ = rc.top + ((rc.bottom - rc.top) - nThick) \ 2
        rc.top = mid_ : rc.bottom = mid_ + nThick
    else
        dim as long mid_ = rc.left + ((rc.right - rc.left) - nThick) \ 2
        rc.left = mid_ : rc.right = mid_ + nThick
    end if
    p->b->SetBackColor( iif( bLive, theme.output.scrollbar.forecolor, theme.output.divider ) )
    p->b->PaintRect( @rc )
end sub

' ==========================================================================================
' Theming
'
' The list colours are pushed from here rather than through modThemeApply's shared helper:
' that file is included AFTER this one (it dispatches to every frmXxx_ApplyTheme, so it has
' to come last), and its helpers are therefore not in scope here.
' ==========================================================================================
' ==========================================================================================
' The variables row painter.
'
' A debugger-specific painter rather than a tweak to ListBox_LVPaintRow: that one sets ONE
' fore colour for the whole row and is shared by four other panes (the compiler results, the
' search results, TODO, and this window's own call stack). Colouring a single CELL is not
' something it can express, and widening it would change four panes to serve one.
'
' Only the VALUE cell is coloured, and only when the value differs from what it was at the
' previous stop.
'
' PSLISTTREE_PAINTINFO carries no control handle and no itemData -- only the model row index
' -- so there are three thin wrappers, one per variables tree, each naming its own HWND. That
' is what lets the shared body reach the row descriptor at all. (A "which control am I?"
' field on the paint struct would be the alternative, and that is a change to a control five
' other hosts share, for this window's convenience.)
'
' The colour is output.valuechanged -- a soft ORANGE, and its own theme key rather than a
' reuse of an icon colour. It is deliberately DIFFERENT per theme (240,178,102 dark /
' 198,106,12 light): a tint light enough to read against the dark Output background is
' washed out on white, so one shared value would be legible in one theme and not the other.
' That is the ui.accent trap, and this key exists to avoid it.
' ==========================================================================================
private sub frmDebug_PaintVarRowFor( byval hLV as HWND, byval p as PSLISTTREE_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    ' HOT TRACKING IS DELIBERATELY IGNORED on every tree in this window. isHot is still
    ' reported -- the control cannot be told to stop tracking -- it is simply not painted.
    ' A debugger pane is read while the mouse happens to be resting somewhere, and a row
    ' lighting up under the cursor competes with the one thing here that IS meant to catch
    ' the eye: a value that just changed.
    dim as COLORREF backclr = iif( p->isSelected, theme.output.scrollbar.forecolor, theme.output.text.backcolor )
    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_9) )

    ' A group row ("[100 .. 199]") is chrome, not data -- dimmed so the eye runs past it to
    ' the elements.
    dim as COLORREF foreNormal = theme.output.texthot.forecolor
    if p->isSelectable = false then foreNormal = theme.output.text.forecolor

    ' There is no per-row delete affordance any more. An X drawn in the row could not be
    ' made to behave: PsListTree hit-tests rows and not cells, it re-resolves a tooltip only
    ' when the hot ROW changes so a column-dependent tip went stale, and it starts a
    ' click-to-edit before a host can veto the click. Deleting a watch is the trashcan in the
    ' WATCH caption band, which acts on the SELECTED row -- a button, with a real hit area, a
    ' real tooltip and a real disabled state.

    ' -1, not 0, is "this row has no descriptor". PsListTree_AddNode defaults itemData to
    ' ZERO, so a row added without one silently claimed descriptor 0 and inherited its
    ' changed flag -- which is why the "<click to add a watch>" row lit up in the changed
    ' colour whenever the first global happened to change. Every row that has no descriptor
    ' now passes FRMDEBUG_NOROW explicitly.
    dim as boolean bChanged = false
    dim as long k = cast(long, PsListTree_GetItemData( hLV, p->itemID ))
    if (k >= 0) andalso (k < gDbgRowCount) then bChanged = gDbgRow(k).bChanged

    if p->columnCount <= 0 then
        dim as RECT rcText = p->rc
        rcText.left += pWindow->ScaleX(4)
        p->b->SetForeColor( foreNormal )
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )
        exit sub
    end if

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += pWindow->ScaleX(4)
        dim as COLORREF clr = foreNormal
        if bChanged andalso (i = 1) then clr = theme.output.valuechanged
        p->b->SetForeColor( clr )
        p->b->PaintText( p->cells[i].wszText, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub

' The call stack gets its own painter for one reason: ListBox_LVPaintRow lights a row under
' the cursor, and this window does not. It is shared by four other panes, so it is left alone.
sub frmDebug_PaintStackRow( byval p as PSLISTTREE_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    dim as COLORREF backclr = iif( p->isSelected, theme.output.scrollbar.forecolor, theme.output.text.backcolor )
    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_9) )

    if p->columnCount <= 0 then
        dim as RECT rcText = p->rc
        rcText.left += pWindow->ScaleX(4)
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )
        exit sub
    end if

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += pWindow->ScaleX(4)
        dim as long flags = DT_LEFT or DT_END_ELLIPSIS
        if i = 3 then flags = DT_LEFT or DT_PATH_ELLIPSIS      ' the source file
        p->b->PaintText( p->cells[i].wszText, @rcText, flags )
    next
end sub

' Every header in this window suppresses its tooltip. Unset, PsColumnHeader shows the
' column's own caption back at the user -- a tip that says "Value" over a column headed
' "Value".
private function frmDebug_NoHeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function

sub frmDebug_PaintGlobalsRow( byval p as PSLISTTREE_PAINTINFO ptr )
    frmDebug_PaintVarRowFor( HWND_FRMDEBUG_LVGLOBALS, p )
end sub
sub frmDebug_PaintLocalsRow( byval p as PSLISTTREE_PAINTINFO ptr )
    frmDebug_PaintVarRowFor( HWND_FRMDEBUG_LVLOCALS, p )
end sub
sub frmDebug_PaintWatchRow( byval p as PSLISTTREE_PAINTINFO ptr )
    frmDebug_PaintVarRowFor( HWND_FRMDEBUG_LVWATCH, p )
end sub

' The sort indicator has to be painted by the host: PsColumnHeader never sorts and never
' draws one -- it only reports the click. Same band as ListBox_LVPaintHeader otherwise.
sub frmDebug_PaintVarHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    ' isHot / isResizeHot deliberately unused -- the header does not light up under the
    ' cursor, for the same reason the rows do not.
    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( theme.output.text.backcolor )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += pWindow->ScaleX(4)
    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' Which tree this header belongs to decides which sort state to read.
    dim as HWND hLV = GetParent( p->hHeader )
    dim pSort as FRMDEBUG_SORT ptr
    if hLV = HWND_FRMDEBUG_LVGLOBALS then
        pSort = @gDbgSortGlobals
    elseif hLV = HWND_FRMDEBUG_LVLOCALS then
        pSort = @gDbgSortLocals
    end if

    if pSort then
        if pSort->nCol = p->itemID then
            dim as RECT rcArrow = p->rc
            rcArrow.right -= pWindow->ScaleX(6)
            p->b->SetFont( ghFont(SYMBOLFONT_9) )
            p->b->PaintText( iif( pSort->bDesc, wszIconSortDesc, wszIconSortAsc ), _
                             @rcArrow, DT_RIGHT )
            p->b->SetFont( ghFont(GUIFONT_9) )
        end if
    end if

    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.output.scrollbardivider )
    p->b->PaintRect( @rcDiv )
end sub

private sub frmDebug_PushListColors( byval hCtl as HWND )
    if IsWindow(hCtl) = 0 then exit sub
    PsListTree_SetBackColor( hCtl, theme.output.text.backcolor )
    PsListTree_SetScrollBarColors( hCtl, theme.output.scrollbar.backcolor, _
                                   theme.output.scrollbar.forecolor, theme.output.scrollbar.forecolor )
    AfxRedrawWindow( hCtl )
end sub

sub frmDebug_ApplyTheme()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub

    #macro TOOLBTNTHEME( h )
        if IsWindow(h) then
            dim as PSBUTTON_COLORS bc
            PsButton_GetColors( h, @bc )
            bc.BackColor        = theme.output.texthot.backcolor
            bc.BackColorHot     = theme.output.scrollbar.forecolor
            bc.BackColorPressed = theme.output.scrollbar.forecolor
            bc.ForeColor        = theme.output.texthot.forecolor
            bc.ForeColorHot     = theme.output.texthot.forecolor
            bc.BorderColor      = theme.output.divider
            bc.BorderColorHot   = theme.output.divider
            PsButton_SetColors( h, @bc )
        end if
    #endmacro
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        TOOLBTNTHEME( gDbgBtn(id) )
    next
    ' The restore icon is a toolbar button that happens to live in the strip, so it takes the
    ' toolbar palette rather than the caption-band one.
    TOOLBTNTHEME( gDbgBtnRestore )

    ' The caption band is BackColorHot, so these melt into it until hovered.
    #macro CAPBTNTHEME( h )
        if IsWindow(h) then
            dim as PSBUTTON_COLORS ac
            PsButton_GetColors( h, @ac )
            ac.BackColor        = theme.output.texthot.backcolor
            ac.BackColorHot     = theme.output.scrollbar.forecolor
            ac.BackColorPressed = theme.output.scrollbar.forecolor
            ac.ForeColor        = theme.output.text.forecolor
            ac.ForeColorHot     = theme.output.texthot.forecolor
            ac.BorderColor      = theme.output.texthot.backcolor
            ac.BorderColorHot   = theme.output.divider
            PsButton_SetColors( h, @ac )
        end if
    #endmacro
    CAPBTNTHEME( gDbgBtnAddWatch )
    CAPBTNTHEME( gDbgBtnDelWatch )

    frmDebug_PushListColors( HWND_FRMDEBUG_LVGLOBALS )
    frmDebug_PushListColors( HWND_FRMDEBUG_LVLOCALS )
    frmDebug_PushListColors( HWND_FRMDEBUG_LVSTACK )
    frmDebug_PushListColors( HWND_FRMDEBUG_LVWATCH )

    #macro SPLITTHEME( h )
        if IsWindow(h) then PsSplitter_SetColors( h, theme.output.texthot.backcolor, theme.output.divider )
    #endmacro
    SPLITTHEME( HWND_FRMDEBUG_SPLITMAIN )
    SPLITTHEME( HWND_FRMDEBUG_SPLITLEFT )
    SPLITTHEME( HWND_FRMDEBUG_SPLITRIGHT )

    Theme_ApplyDarkCaption( HWND_FRMDEBUG )
    AfxRedrawWindow( HWND_FRMDEBUG )
end sub

function frmDebug_IsOpen() as boolean
    return (IsWindow(HWND_FRMDEBUG) <> 0)
end function

' One button by command id, for the theme pass -- which has to reach every tooltip in the
' application and would otherwise need gDbgBtn() and the two caption buttons made global.
' Answers 0 for an unknown id or a closed window, which is what lets the caller ask
' unconditionally.
function frmDebug_GetButton( byval id as long ) as HWND
    select case id
        case IDC_FRMDEBUG_CMDADDWATCH : return gDbgBtnAddWatch
        case IDC_FRMDEBUG_CMDDELWATCH : return gDbgBtnDelWatch
        case IDC_FRMDEBUG_CMDRESTORE  : return gDbgBtnRestore
        case IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST : return gDbgBtn(id)
    end select
    return 0
end function

' ==========================================================================================
' Persisted geometry
'
' Called from clsConfig.SaveConfigFile alongside frmHelpCenter_CaptureState and
' frmOutput_CaptureState, and again from WM_DESTROY. Same shape as those two: skip while
' minimized, and read rcNormalPosition from GetWindowPlacement rather than GetWindowRect --
' a maximized window's GetWindowRect is the monitor, which is not what should be restored.
'
' The splitters are stored as a PERCENT of the span they divide, not as pixels. This window
' is resizable and can reopen at a different size, on a different monitor, at a different
' DPI; a stored pixel offset would then either sit against one edge or outside the pane
' altogether. The percent survives all three.
' ==========================================================================================
sub frmDebug_CaptureState()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    if IsIconic(HWND_FRMDEBUG) then exit sub
    ' COLLAPSED: persist nothing. The live geometry is the strip's -- a corner of the editing
    ' area, a few icons wide -- and both the window rect and the splitter percents computed
    ' from that client are meaningless. Saving them would reopen the debugger as a sliver.
    if gDbgCollapsed then exit sub

    dim as WINDOWPLACEMENT wp
    wp.length = sizeof(WINDOWPLACEMENT)
    if GetWindowPlacement( HWND_FRMDEBUG, @wp ) then
        gConfig.DebugMaximized = (wp.showCmd = SW_SHOWMAXIMIZED)
        gConfig.DebugLeft      = wp.rcNormalPosition.left
        gConfig.DebugTop       = wp.rcNormalPosition.top
        gConfig.DebugRight     = wp.rcNormalPosition.right
        gConfig.DebugBottom    = wp.rcNormalPosition.bottom
    end if

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub
    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub
    dim as long nTop = pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT) + pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT)

    if IsWindow(HWND_FRMDEBUG_SPLITMAIN) then
        gConfig.DebugSplitMain = frmDebug_PosToPct( PsSplitter_GetPos(HWND_FRMDEBUG_SPLITMAIN), 0, rc.right )
    end if
    if IsWindow(HWND_FRMDEBUG_SPLITLEFT) then
        gConfig.DebugSplitLeft = frmDebug_PosToPct( PsSplitter_GetPos(HWND_FRMDEBUG_SPLITLEFT), nTop, rc.bottom )
    end if
    if IsWindow(HWND_FRMDEBUG_SPLITRIGHT) then
        gConfig.DebugSplitRight = frmDebug_PosToPct( PsSplitter_GetPos(HWND_FRMDEBUG_SPLITRIGHT), nTop, rc.bottom )
    end if
end sub

' ==========================================================================================
' Button tooltips
'
' Composed LIVE from the key bindings rather than baked into the .lang file, so rebinding a
' debug command in the Keyboard Shortcuts dialog is reflected here without the string going
' stale. Re-armed whenever the accelerator table is rebuilt.
' ==========================================================================================
' getMenuAccelText is the ONE place that resolves a command to its keystroke (user binding
' overrides the default unless the default is disabled). Calling it rather than re-reading
' gKeys here is what stops the tooltip and the menu disagreeing about a shortcut. It returns
' the text prefixed with a tab, because its caller is a menu string.
private function frmDebug_TipFor( byval wszCaption as DWSTRING, byval idmCommand as long ) as DWSTRING
    dim as DWSTRING wszKeys = getMenuAccelText( idmCommand )
    if PsLen(wszKeys) = 0 then return wszCaption
    if PsLeft(wszKeys, 1) = chr(9) then wszKeys = PsMid(wszKeys, 2)
    if PsLen(wszKeys) = 0 then return wszCaption
    return wszCaption & "  (" & wszKeys & ")"
end function

sub frmDebug_ArmButtonTooltips()
    ' PSTIP_MODE_PS is set here rather than at Create, and re-set on every re-arm, because it
    ' is idempotent and this is the one place every button passes through. The mode has to be
    ' armed BEFORE the text: PsTipHost builds the tool on first use, and the defaults a tip
    ' is created with are the ones Theme_ArmTooltipDefaults last published.
    #macro ARMTIP( id, wszCap, idm )
        if IsWindow(gDbgBtn(id)) then
            PsButton_SetTooltipMode( gDbgBtn(id), PSTIP_MODE_PS )
            PsButton_SetTooltipText( gDbgBtn(id), frmDebug_TipFor( wszCap, idm ) )
        end if
    #endmacro
    ARMTIP( IDC_FRMDEBUG_CMDCONTINUE,  L(453, "Continue"),      IDM_DEBUG_STARTDEBUGGING )
    ARMTIP( IDC_FRMDEBUG_CMDBREAK,     L(433, "Break"),         IDM_DEBUG_PAUSE )
    ARMTIP( IDC_FRMDEBUG_CMDSTOP,      L(454, "Stop"),          IDM_DEBUG_STOPDEBUGGING )
    ARMTIP( IDC_FRMDEBUG_CMDSTEPINTO,  L(243, "Step"),          IDM_DEBUG_STEPINTO )
    ARMTIP( IDC_FRMDEBUG_CMDSTEPOVER,  L(244, "Step Over"),     IDM_DEBUG_STEPOVER )
    ARMTIP( IDC_FRMDEBUG_CMDSTEPOUT,   L(251, "Step Out"),      IDM_DEBUG_STEPOUT )
    ARMTIP( IDC_FRMDEBUG_CMDRUNCURSOR, L(252, "Run to Cursor"), IDM_DEBUG_RUNTOCURSOR )
end sub

' ==========================================================================================
' Pure geometry and pure text handling. Both are separated from their callers so the
' self-test can assert the code that actually runs rather than a copy of it.
' ==========================================================================================
' Two columns split by barMain, each split again by its own horizontal bar. barLeft and
' barRight are ABSOLUTE client y coordinates, not offsets into their column: the splitter
' control reports its own position in the parent's coordinates and PsSplitter.bi says so, so
' converting here and back would be two chances to disagree with the control.
sub frmDebug_ComputeLayout( byval cx as long, byval cy as long, _
                            byval barMain as long, byval barLeft as long, byval barRight as long, _
                            byval nTool as long, byval nStatus as long, byval nCap as long, _
                            byval nSplit as long, byref lo as FRMDEBUG_LAYOUT )
    dim as long nTop = nTool + nStatus

    lo.rcStatus.left = 0        : lo.rcStatus.top = nTool
    lo.rcStatus.right = cx      : lo.rcStatus.bottom = nTool + nStatus

    dim as long xL2 = barMain + nSplit          ' left edge of the right column
    if xL2 > cx then xL2 = cx

    ' --- left column: globals over locals ---
    lo.rcCapGlobals.left = 0       : lo.rcCapGlobals.top    = nTop
    lo.rcCapGlobals.right = barMain: lo.rcCapGlobals.bottom = nTop + nCap

    lo.rcGlobals.left = 0          : lo.rcGlobals.top    = nTop + nCap
    lo.rcGlobals.right = barMain   : lo.rcGlobals.bottom = barLeft

    lo.rcSplitLeft.left = 0        : lo.rcSplitLeft.top    = barLeft
    lo.rcSplitLeft.right = barMain : lo.rcSplitLeft.bottom = barLeft + nSplit

    lo.rcCapLocals.left = 0        : lo.rcCapLocals.top    = barLeft + nSplit
    lo.rcCapLocals.right = barMain : lo.rcCapLocals.bottom = barLeft + nSplit + nCap

    lo.rcLocals.left = 0           : lo.rcLocals.top    = barLeft + nSplit + nCap
    lo.rcLocals.right = barMain    : lo.rcLocals.bottom = cy

    ' --- the vertical bar, full height of the content area ---
    lo.rcSplitMain.left = barMain          : lo.rcSplitMain.top    = nTop
    lo.rcSplitMain.right = barMain + nSplit: lo.rcSplitMain.bottom = cy

    ' --- right column: call stack over watch ---
    lo.rcCapStack.left = xL2       : lo.rcCapStack.top    = nTop
    lo.rcCapStack.right = cx       : lo.rcCapStack.bottom = nTop + nCap

    lo.rcStack.left = xL2          : lo.rcStack.top    = nTop + nCap
    lo.rcStack.right = cx          : lo.rcStack.bottom = barRight

    lo.rcSplitRight.left = xL2     : lo.rcSplitRight.top    = barRight
    lo.rcSplitRight.right = cx     : lo.rcSplitRight.bottom = barRight + nSplit

    lo.rcCapWatch.left = xL2       : lo.rcCapWatch.top    = barRight + nSplit
    lo.rcCapWatch.right = cx       : lo.rcCapWatch.bottom = barRight + nSplit + nCap

    lo.rcWatch.left = xL2          : lo.rcWatch.top    = barRight + nSplit + nCap
    lo.rcWatch.right = cx          : lo.rcWatch.bottom = cy
end sub

' ------------------------------------------------------------------------------------------
' Splitter position <-> stored percent.
'
' A pair rather than one function with a direction flag, so the round trip is two separate
' pieces of arithmetic and an assertion that they agree is worth something.
' ------------------------------------------------------------------------------------------
function frmDebug_PctToPos( byval nPct as long, byval nFrom as long, byval nTo as long ) as long
    dim as long span = nTo - nFrom
    if span <= 0 then return nFrom
    if nPct < 0 then nPct = 0
    if nPct > FRMDEBUG_PCTSCALE then nPct = FRMDEBUG_PCTSCALE
    return nFrom + cast(long, (cast(longint, span) * nPct) \ FRMDEBUG_PCTSCALE)
end function

function frmDebug_PosToPct( byval nPos as long, byval nFrom as long, byval nTo as long ) as long
    dim as long span = nTo - nFrom
    if span <= 0 then return 0
    dim as long nPct = cast(long, (cast(longint, nPos - nFrom) * FRMDEBUG_PCTSCALE) \ span)
    if nPct < 0 then nPct = 0
    if nPct > FRMDEBUG_PCTSCALE then nPct = FRMDEBUG_PCTSCALE
    return nPct
end function

function frmDebug_ExprFromLine( byval wszLine as DWSTRING, byval col as long ) as DWSTRING
    if (col < 1) orelse (col > PsLen(wszLine)) then return ""

    #macro ISIDCH( c )
        ( ((c) >= 48 andalso (c) <= 57) orelse ((c) >= 65 andalso (c) <= 90) orelse _
          ((c) >= 97 andalso (c) <= 122) orelse (c) = 95 )
    #endmacro

    dim as long ch = asc(PsMid(wszLine, col, 1))
    if ISIDCH(ch) = 0 then return ""

    ' Walk left through identifier characters AND dots, so the dotted path comes with it --
    ' hovering "ix" inside "rec.inner.ix" has to evaluate the whole chain, because the bare
    ' field name resolves to nothing.
    dim as long loPos = col
    do while loPos > 1
        dim as long c2 = asc(PsMid(wszLine, loPos - 1, 1))
        if ISIDCH(c2) orelse (c2 = 46) then
            loPos -= 1
        else
            exit do
        end if
    loop
    ' Walk right through identifier characters ONLY: a trailing dot is an incomplete path,
    ' and the member after it is not what the cursor is on.
    dim as long hiPos = col
    do while hiPos < PsLen(wszLine)
        dim as long c2 = asc(PsMid(wszLine, hiPos + 1, 1))
        if ISIDCH(c2) then
            hiPos += 1
        else
            exit do
        end if
    loop

    dim as DWSTRING wszExpr = PsMid(wszLine, loPos, hiPos - loPos + 1)
    ' A leading dot means the walk started inside a path whose head is off this line.
    do while (PsLen(wszExpr) > 0) andalso (PsLeft(wszExpr, 1) = ".")
        wszExpr = PsMid(wszExpr, 2)
    loop
    return wszExpr
end function

' ==========================================================================================
' Hover datatips
'
' A PsTooltip driven ENTIRELY BY HAND, the same way tiko's code tips are: Create then
' Detach immediately, which takes the tip off PsTooltip's shared dwell clock. A datatip is
' anchored to a WORD under the cursor, not to a control, so hover timing, auto-pop and
' mouse-button dismissal all belong to us -- the fade still works.
' ==========================================================================================
sub frmDebug_InitDataTip()
    if IsWindow(gDbgDataTip) then exit sub
    gDbgDataTip = PsTooltip_Create( HWND_FRMMAIN )
    if gDbgDataTip = 0 then exit sub
    PsTooltip_Detach( gDbgDataTip )
    PsTooltip_SetFonts( gDbgDataTip, ghFont(GUIFONT_10), ghFont(GUIFONTBOLD_11), ghFont(SYMBOLFONT_10) )
    PsTooltip_SetMaxWidth( gDbgDataTip, AfxScaleX(520) )

    ' The EDITOR palette, not the popup one -- this tip appears over source text and has to
    ' read as part of it. Same call the code tips make, for the same reason.
    dim as PSTOOLTIP_COLORS tipc
    Theme_FillCodeTipColors( tipc )
    PsTooltip_SetColors( gDbgDataTip, @tipc )
end sub

sub frmDebug_DestroyDataTip()
    if IsWindow(gDbgDataTip) then DestroyWindow( gDbgDataTip )
    gDbgDataTip = 0
    gDbgDataTipExpr = ""
end sub

sub frmDebug_HideDataTip()
    if IsWindow(gDbgDataTip) = 0 then exit sub
    if PsLen(gDbgDataTipExpr) = 0 then exit sub
    PsTooltip_Hide( gDbgDataTip )
    gDbgDataTipExpr = ""
end sub

' The identifier under a document position, including any dotted path it belongs to, so
' hovering "ix" inside "rec.inner.ix" evaluates the whole chain rather than a bare field
' name that resolves to nothing.
private function frmDebug_ExprAt( byval hEdit as HWND, byval nPos as long ) as DWSTRING
    if hEdit = 0 then return ""
    dim as long nLine  = SciExec( hEdit, SCI_LINEFROMPOSITION, nPos, 0 )
    dim as long nStart = SciExec( hEdit, SCI_POSITIONFROMLINE, nLine, 0 )
    dim as long nEnd   = SciExec( hEdit, SCI_GETLINEENDPOSITION, nLine, 0 )
    if (nEnd <= nStart) orelse (nPos < nStart) orelse (nPos > nEnd) then return ""

    dim as long nLen = nEnd - nStart
    if (nLen <= 0) orelse (nLen > 4096) then return ""
    dim as zstring ptr pBuf = allocate( nLen + 1 )
    if pBuf = 0 then return ""
    dim as Sci_TextRange tr
    tr.chrg.cpMin = nStart
    tr.chrg.cpMax = nEnd
    tr.lpstrText  = pBuf
    SciExec( hEdit, SCI_GETTEXTRANGE, 0, cast(LPARAM, @tr) )
    pBuf[nLen] = 0
    dim as DWSTRING wszLine = *pBuf
    deallocate pBuf
    if PsLen(wszLine) = 0 then return ""

    dim as long col = nPos - nStart + 1        ' 1-based index into the line
    if col > PsLen(wszLine) then col = PsLen(wszLine)
    if col < 1 then return ""

    return frmDebug_ExprFromLine( wszLine, col )
end function

sub frmDebug_ShowDataTip( byval hEdit as HWND, byval nPos as long )
    if debugparser_is_active() = 0 then exit sub
    ' Only while STOPPED. A running debuggee has no frame to evaluate against, and its
    ' memory is changing underneath any answer we could give.
    if debugparser_is_running() <> 0 then exit sub
    if hEdit = 0 then exit sub

    dim as DWSTRING wszExpr = frmDebug_ExprAt( hEdit, nPos )
    if PsLen(wszExpr) = 0 then
        frmDebug_HideDataTip()
        exit sub
    end if
    ' Already showing this one: leave it alone rather than restarting its fade.
    if wszExpr = gDbgDataTipExpr then exit sub

    dim as DBGP_NODE nd
    dim as wstring * 512 wexpr = wszExpr
    if debugparser_evaluate( @wexpr, gDbgCurFrame, @nd ) <> DBGP_OK then
        frmDebug_HideDataTip()
        exit sub
    end if

    frmDebug_InitDataTip()
    if IsWindow(gDbgDataTip) = 0 then exit sub

    PsTooltip_SetTitle( gDbgDataTip, wszExpr & "   " & nd.wszType )
    PsTooltip_SetText( gDbgDataTip, nd.wszValue )

    ' Anchor on the WORD, so the tip points at what it describes rather than at the cursor.
    dim as long nLine   = SciExec( hEdit, SCI_LINEFROMPOSITION, nPos, 0 )
    dim as long xPos    = SciExec( hEdit, SCI_POINTXFROMPOSITION, 0, nPos )
    dim as long yPos    = SciExec( hEdit, SCI_POINTYFROMPOSITION, 0, nPos )
    dim as long nHeight = SciExec( hEdit, SCI_TEXTHEIGHT, nLine, 0 )

    dim as RECT rcAnchor
    rcAnchor.left   = xPos
    rcAnchor.top    = yPos
    rcAnchor.right  = xPos
    rcAnchor.bottom = yPos + nHeight
    dim as POINT pt
    pt.x = rcAnchor.left : pt.y = rcAnchor.top
    ClientToScreen( hEdit, @pt )
    rcAnchor.left = pt.x : rcAnchor.top = pt.y
    pt.x = rcAnchor.right : pt.y = rcAnchor.bottom
    ClientToScreen( hEdit, @pt )
    rcAnchor.right = pt.x : rcAnchor.bottom = pt.y

    if PsTooltip_ShowForRect( gDbgDataTip, rcAnchor, TIP_ALIGN_BELOW ) then
        gDbgDataTipExpr = wszExpr
    end if
end sub

' ==========================================================================================
' Control construction
' ==========================================================================================
private function frmDebug_CreateListView( byval CtrlID as long ) as HWND
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then return 0

    dim as HWND hCtl = PsListTree_Create( HWND_FRMDEBUG, CtrlID )

    if CtrlID = IDC_FRMDEBUG_LVSTACK then
        PsListTree_AddColumn( hCtl, "",                 pWindow->ScaleX(20) )
        PsListTree_AddColumn( hCtl, L(445, "Function"), pWindow->ScaleX(200) )
        PsListTree_AddColumn( hCtl, L(200, "Line"),     pWindow->ScaleX(60) )
        PsListTree_AddColumn( hCtl, L(201, "File"),     pWindow->ScaleX(160) )
        PsListTree_SetFillColumn( hCtl, 3 )
        PsListTree_SetSelChangeCallback( hCtl, @frmDebug_StackSelChange )
        PsListTree_SetPaintCallback( hCtl, @frmDebug_PaintStackRow )
        PsListTree_SetHeaderPaintCallback( hCtl, @frmDebug_PaintVarHeader )
    else
        ' Value is the column being READ -- it carries array shapes, string previews and
        ' "{ 3 fields }" -- so it is given the room and is the fill column. Name and Type are
        ' both short and predictable.
        PsListTree_AddColumn( hCtl, L(443, "Name"),  pWindow->ScaleX(140) )
        PsListTree_AddColumn( hCtl, L(276, "Value"), pWindow->ScaleX(540) )
        PsListTree_AddColumn( hCtl, L(444, "Type"),  pWindow->ScaleX(80) )
        ' Value is the FILL column, so it absorbs whatever the other two do not use -- the
        ' way to widen it further is to widen the pane, which the splitters now remember.
        PsListTree_SetFillColumn( hCtl, 1 )
        ' A real tree: UDT fields and array index groups are children, not flattened rows.
        PsListTree_ShowTwisty( hCtl, true )
        PsListTree_SetIndentWidth( hCtl, 14 )
        PsListTree_SetTwistyFont( hCtl, ghFont(SYMBOLFONT_9) )
        ' Children are fetched on the first expand; see the header. Without this the
        ' placeholder row is all a node ever shows.
        PsListTree_SetExpandCollapseCallback( hCtl, @frmDebug_ExpandCollapse )

        select case CtrlID
            case IDC_FRMDEBUG_LVGLOBALS
                PsListTree_SetPaintCallback( hCtl, @frmDebug_PaintGlobalsRow )
                PsListTree_SetHeaderPaintCallback( hCtl, @frmDebug_PaintVarHeader )
                PsListTree_SetColumnClickCallback( hCtl, @frmDebug_HeaderClick )
            case IDC_FRMDEBUG_LVLOCALS
                PsListTree_SetPaintCallback( hCtl, @frmDebug_PaintLocalsRow )
                PsListTree_SetHeaderPaintCallback( hCtl, @frmDebug_PaintVarHeader )
                PsListTree_SetColumnClickCallback( hCtl, @frmDebug_HeaderClick )
            case else
                ' Watch. Expressions are typed into column 0, and THAT is what makes
                ' PsListTree_FilterMessage mandatory in frmMain's pump -- see the header.
                ' Not sortable: these rows are in the order the user entered them, which is
                ' the only order that means anything here.
                PsListTree_SetPaintCallback( hCtl, @frmDebug_PaintWatchRow )
                PsListTree_SetHeaderPaintCallback( hCtl, @frmDebug_PaintVarHeader )
                PsListTree_EnableLabelEdit( hCtl, true )
                PsListTree_SetEditColumn( hCtl, 0 )
                PsListTree_SetEnterEdits( hCtl, true )
                ' CLICK-TO-EDIT IS DELIBERATELY OFF. PsListTree opens the editor inside its
                ' own WM_LBUTTONDOWN, before the message callback runs, so a host cannot stop
                ' a click from starting an edit -- which made clicking the delete X open an
                ' editor on the row it was about to remove. Double-click and ENTER edit
                ' instead, and both are routed explicitly.
                PsListTree_SetActivateCallback( hCtl, @frmDebug_WatchActivate )
                PsListTree_SetEndLabelEditCallback( hCtl, @frmDebug_EndLabelEdit )
                PsListTree_SetSelChangeCallback( hCtl, @frmDebug_WatchSelChange )
                ' The editor is created lazily and owned by the control, so a host cannot
                ' reach the PsTextBox to colour it -- without this it opens black-on-white
                ' in the middle of a dark list.
                PsListTree_SetEditColors( hCtl, theme.output.text.backcolor, theme.output.texthot.forecolor, _
                                          theme.output.divider, theme.output.scrollbar.forecolor )
                PsListTree_SetEditTextInset( hCtl, pWindow->ScaleX(4) )
        end select
    end if

    PsListTree_ShowHeader( hCtl, true )
    PsListTree_SetColumnAutoSizeCallback( hCtl, @ListBox_LVAutoSizeColumn )
    PsListTree_SetHeaderTooltipCallback( hCtl, @frmDebug_NoHeaderTooltip )
    ' All four panes on PsTooltip. WHICH tips appear is unchanged -- with no row callback the
    ' control still falls back to the row's own column-0 text; this changes only what draws
    ' it. (The column HEADERS stay on comctl32 because PsColumnHeader has no PsTipHost, and
    ' that is invisible here: frmDebug_NoHeaderTooltip suppresses them outright.)
    PsListTree_SetTooltipMode( hCtl, PSTIP_MODE_PS )
    PsListTree_SetFont( hCtl, ghFont(GUIFONT_9) )
    PsListTree_SetBackColor( hCtl, theme.output.text.backcolor )
    PsListTree_SetScrollBarColors( hCtl, theme.output.scrollbar.backcolor, _
                                   theme.output.scrollbar.forecolor, theme.output.scrollbar.forecolor )
    return hCtl
end function

private sub frmDebug_CreateButton( byval id as long, byval wszText as DWSTRING, byval wszGlyph as DWSTRING )
    dim as HWND h = PsButton_Create( HWND_FRMDEBUG, id )
    PsButton_SetText( h, wszText )
    if PsLen(wszGlyph) then PsButton_SetGlyphLeft( h, wszGlyph )
    PsButton_SetFont( h, ghFont(GUIFONT_9) )
    PsButton_SetGlyphFont( h, ghFont(SYMBOLFONT_9) )
    PsButton_SetClickCallback( h, @frmDebug_ButtonClick )
    gDbgBtn(id) = h
end sub

' ==========================================================================================
' Window procedure
' ==========================================================================================
private function frmDebug_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                   byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case MSG_DBGP_STOPPED
            frmDebug_OnStopped()
            return 0

        case MSG_DBGP_EXITED
            frmDebug_OnExited( cast(long, wParam) )
            return 0

        case MSG_DBGP_FAILED
            dim as wstring * 512 werr
            debugparser_get_error( @werr, 512 )
            gApp.IsDebuggerActive = false
            gDbgSessionEnding = true
            TikoMsgBox( HWND_FRMMAIN, L(446, "The debugger could not start the program.") & _
                        chr(13) & chr(13) & werr, L(214, "Confirm"), TMB_ICON_ERROR, TMB_OK )
            DestroyWindow( hwnd )
            return 0

        case WM_SYSCOMMAND
            ' The minimize button never reaches Windows: an owned window has no taskbar
            ' button, so a real minimize would park the debugger somewhere unreachable with
            ' the debuggee still stopped. Collapse to the icon strip instead. Covers the
            ' caption button, Alt+Space > Minimize and anything else that raises SC_MINIMIZE.
            ' The low four bits carry internal state and must be masked off before comparing.
            if (wParam and &hFFF0) = SC_MINIMIZE then
                frmDebug_Collapse()
                return 0
            end if

        case WM_SIZE
            ' A minimize sends WM_SIZE with a 0x0 client; nothing here is worth laying out
            ' against it.
            if wParam = SIZE_MINIMIZED then return 0
            if gDbgCollapsed then
                frmDebug_LayoutStrip()
                AfxRedrawWindow( hwnd )
                return 0
            end if
            frmDebug_PositionWindows()
            return 0

        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            dim as PsBufferPaint b
            b.BeginDoubleBuffer(hwnd)
            b.SetBackColor( theme.output.texthot.backcolor )
            b.PaintClientRect()

            ' COLLAPSED: the client is the icon strip and nothing else. The buttons paint
            ' themselves, so the filled background above is the whole job -- and returning
            ' here is what stops the status line and the four captions being drawn into a
            ' client far too small to hold them.
            if gDbgCollapsed then
                b.EndDoubleBuffer()
                return 0
            end if

            if pWindow then
                dim as RECT rc
                GetClientRect( hwnd, @rc )
                dim as RECT rcStatus
                rcStatus.left   = pWindow->ScaleX(12)
                rcStatus.top    = pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT)
                rcStatus.right  = rc.right - pWindow->ScaleX(12)
                rcStatus.bottom = rcStatus.top + pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT)
                b.SetFont( ghFont(GUIFONT_9) )
                b.SetForeColor( theme.output.texthot.forecolor )
                b.SetBackColor( theme.output.texthot.backcolor )
                b.PaintText( gDbgStatusText, @rcStatus, DT_LEFT or DT_END_ELLIPSIS )

                ' A hairline under the status line, separating chrome from content.
                dim as RECT rcLine
                rcLine.left   = 0
                rcLine.top    = rcStatus.bottom - 1
                rcLine.right  = rc.right
                rcLine.bottom = rcStatus.bottom
                b.SetBackColor( theme.output.divider )
                b.PaintRect( @rcLine )

                ' --- the four pane captions ---
                '
                ' Painted rather than being four more controls: a caption is a word and a
                ' rule, and a PsSelectBar or a label per pane would be four windows, four
                ' theme pushes and four more entries in the DeferWindowPos pass to say the
                ' same thing. The rects come from ComputeLayout, so a band cannot drift away
                ' from the pane it names.
                dim as FRMDEBUG_LAYOUT lo
                frmDebug_ComputeLayout( rc.right, rc.bottom, _
                                        PsSplitter_GetPos( HWND_FRMDEBUG_SPLITMAIN ), _
                                        PsSplitter_GetPos( HWND_FRMDEBUG_SPLITLEFT ), _
                                        PsSplitter_GetPos( HWND_FRMDEBUG_SPLITRIGHT ), _
                                        pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT), _
                                        pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT), _
                                        pWindow->ScaleY(FRMDEBUG_CAPHEIGHT), _
                                        pWindow->ScaleX(FRMDEBUG_SPLITWIDTH), lo )

                #macro PAINTCAPTION( r, wszCap )
                    scope
                        dim as RECT rcCap = (r)
                        b.SetBackColor( theme.output.texthot.backcolor )
                        b.PaintRect( @rcCap )
                        rcCap.left += pWindow->ScaleX(8)
                        b.SetFont( ghFont(GUIFONT_9) )
                        b.SetForeColor( theme.output.text.forecolor )
                        b.PaintText( wszCap, @rcCap, DT_LEFT or DT_END_ELLIPSIS )
                    end scope
                #endmacro
                PAINTCAPTION( lo.rcCapGlobals, ucase(L(452, "Globals")) )
                PAINTCAPTION( lo.rcCapLocals,  ucase(L(451, "Locals")) )
                PAINTCAPTION( lo.rcCapStack,   ucase(L(459, "Call Stack")) )
                PAINTCAPTION( lo.rcCapWatch,   ucase(L(457, "Watch")) )
            end if

            b.EndDoubleBuffer()
            return 0

        case WM_CLOSE
            ' Closing the window ends the session. The engine's teardown then posts
            ' MSG_DBGP_EXITED, and THAT is what destroys this window -- so the debuggee is
            ' never left running with no way to reach it.
            if (debugparser_is_active() <> 0) andalso (gDbgSessionEnding = false) then
                debugparser_stop()
                return 0
            end if
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            ' Capture on the way out AS WELL as on every save. Closing the debugger is the
            ' moment its geometry is most likely to have just changed, and the next
            ' SaveConfigFile may be much later -- or never, if tiko is killed.
            frmDebug_CaptureState()
            return 0

        case WM_NCDESTROY
            frmDebug_DestroyDataTip()
            frmDebug_ClearExecutionMarkers()
            gApp.IsDebuggerActive = false
            debugparser_unload()
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if pWindow then delete pWindow
            HWND_FRMDEBUG            = 0
            HWND_FRMDEBUG_LVGLOBALS  = 0
            HWND_FRMDEBUG_LVLOCALS   = 0
            HWND_FRMDEBUG_LVSTACK    = 0
            HWND_FRMDEBUG_LVWATCH    = 0
            HWND_FRMDEBUG_SPLITMAIN  = 0
            HWND_FRMDEBUG_SPLITLEFT  = 0
            HWND_FRMDEBUG_SPLITRIGHT = 0
            gDbgBtnAddWatch          = 0
            gDbgBtnDelWatch          = 0
            gDbgBtnRestore           = 0
            gDbgCollapsed            = false
            ' The descriptors hold a DBGP_NODE each; a long session that expanded a lot of
            ' arrays can be holding thousands, and the session they describe is over.
            frmDebug_RowsReset()
            for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
                gDbgBtn(id) = 0
            next
            frmMain_DebugTopMenuStates()
            return 0
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function

' ==========================================================================================
' frmDebug_Show - builds the window, loads the debug information, seeds the breakpoints the
' user set in the editor, and starts the session.
' ==========================================================================================
function frmDebug_Show( byval hWndParent as HWND, byval executable as DWSTRING, byval cmdparams as DWSTRING ) as LRESULT
    if IsWindow(HWND_FRMDEBUG) then
        SetForegroundWindow( HWND_FRMDEBUG )
        return 0
    end if

    ' Read the debug information BEFORE creating any window. A release build carries none,
    ' and failing here means no debugger window ever appears rather than an empty one that
    ' then has to explain itself.
    dim as wstring * 1024 wszExe = executable
    dim as long rc = debugparser_load( @wszExe, 0 )
    if rc <> DBGP_OK then
        dim as DWSTRING wszMsg
        if rc = DBGP_ERR_NODEBUGINFO then
            wszMsg = L(447, "This program carries no debug information.") & chr(13) & chr(13) & _
                     L(448, "Build it with -g and the gas64 backend, or gas for 32-bit. The gcc backend emits a format that cannot be read.")
        else
            dim as wstring * 512 werr
            debugparser_get_error( @werr, 512 )
            wszMsg = L(449, "The debug information could not be read.") & chr(13) & chr(13) & werr
        end if
        TikoMsgBox( hWndParent, wszMsg, L(214, "Confirm"), TMB_ICON_ERROR, TMB_OK )
        return 0
    end if

    gDbgSessionEnding   = false
    gDbgSplitInit       = false
    gDbgCollapsed       = false
    gDbgCurFrame        = 0
    gDbgFirstStop       = true
    gDbgHaveBreakpoints = false
    gDbgStatusText      = L(432, "Running...")
    gDbgSortGlobals.nCol = -1 : gDbgSortGlobals.bDesc = false
    gDbgSortLocals.nCol  = -1 : gDbgSortLocals.bDesc  = false
    frmDebug_RowsReset()
    frmDebug_ValueReset()
    for i as long = 0 to FRMDEBUG_TREESLOTS - 1
        gDbgExpCount(i) = 0
        gDbgExpTop(i)   = 0
    next

    dim pWindow as CWindow ptr = new CWindow
    dim pParent as CWindow ptr = AfxCWindowPtr(hWndParent)
    if pParent then pWindow->DPI = pParent->DPI

    ' WS_MINIMIZEBOX is what puts the caption button there; SC_MINIMIZE is intercepted in the
    ' window procedure and collapses to the icon strip instead of ever becoming iconic.
    HWND_FRMDEBUG = pWindow->Create( hWndParent, L(450, "Debugger"), @frmDebug_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_THICKFRAME or WS_CAPTION or WS_SYSMENU or WS_MINIMIZEBOX or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_COMPOSITED )
    ' UNSCALED. SetClientSize applies the DPI factor itself; passing a pre-scaled value
    ' squares it, which at 175% turned a 900px window into a 2756px one.
    pWindow->SetClientSize( FRMDEBUG_DEFWIDTH, FRMDEBUG_DEFHEIGHT )
    pWindow->Center( HWND_FRMDEBUG, hWndParent )

    ' Stored geometry is in SCALED screen coordinates -- it came from GetWindowPlacement, so
    ' it must not go through ScaleX/Y again. Zero right/bottom is the "never saved" sentinel
    ' and leaves the centred default above in place.
    if (gConfig.DebugRight > gConfig.DebugLeft) andalso (gConfig.DebugBottom > gConfig.DebugTop) then
        dim as WINDOWPLACEMENT wp
        wp.length = sizeof(WINDOWPLACEMENT)
        wp.rcNormalPosition.left   = gConfig.DebugLeft
        wp.rcNormalPosition.top    = gConfig.DebugTop
        wp.rcNormalPosition.right  = gConfig.DebugRight
        wp.rcNormalPosition.bottom = gConfig.DebugBottom
        wp.showCmd = iif( gConfig.DebugMaximized, SW_SHOWMAXIMIZED, SW_SHOWNORMAL )
        SetWindowPlacement( HWND_FRMDEBUG, @wp )
    end if

    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' --- toolbar ---
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDCONTINUE,  L(453, "Continue"), wszIconContinue )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDBREAK,     L(433, "Break"),              wszIconPause )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTOP,      L(454, "Stop"),     wszIconStop )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTEPINTO,  L(243, "Step"),               wszIconStepNext )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTEPOVER,  L(244, "Step Over"),          wszIconStepOver )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTEPOUT,   L(251, "Step Out"),           wszIconStepOut )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDRUNCURSOR, L(252, "Run to Cursor"),      wszIconRunToCursor )

    ' The restore icon. Created here with the toolbar it belongs to, but never shown until the
    ' window collapses -- and it is the ONLY way back, so it is never disabled with the debug
    ' commands: a strip whose last icon is greyed while the debuggee runs is a window with no
    ' way out.
    gDbgBtnRestore = PsButton_Create( HWND_FRMDEBUG, IDC_FRMDEBUG_CMDRESTORE )
    PsButton_SetGlyphLeft( gDbgBtnRestore, wszIconDock )
    PsButton_SetGlyphFont( gDbgBtnRestore, ghFont(SYMBOLFONT_9) )
    PsButton_SetFont( gDbgBtnRestore, ghFont(GUIFONT_9) )
    PsButton_SetTooltipMode( gDbgBtnRestore, PSTIP_MODE_PS )
    PsButton_SetTooltipText( gDbgBtnRestore, L(521, "Restore Window") )
    PsButton_SetClickCallback( gDbgBtnRestore, @frmDebug_RestoreClick )

    frmDebug_ArmButtonTooltips()

    ' --- the four panes ---
    HWND_FRMDEBUG_LVGLOBALS = frmDebug_CreateListView( IDC_FRMDEBUG_LVGLOBALS )
    HWND_FRMDEBUG_LVLOCALS  = frmDebug_CreateListView( IDC_FRMDEBUG_LVLOCALS )
    HWND_FRMDEBUG_LVSTACK   = frmDebug_CreateListView( IDC_FRMDEBUG_LVSTACK )
    HWND_FRMDEBUG_LVWATCH   = frmDebug_CreateListView( IDC_FRMDEBUG_LVWATCH )

    ' A splitter is named for its OWN axis, not for the direction it moves in: the main bar
    ' is a vertical bar separating two columns, the other two are horizontal bars each
    ' splitting one column.
    #macro MAKESPLITTER( h, id, axis )
        h = PsSplitter_Create( HWND_FRMDEBUG, id, axis )
        PsSplitter_SetPaintCallback( h, @frmDebug_SplitterPaint )
        PsSplitter_SetPosChangedCallback( h, @frmDebug_SplitterMoved )
    #endmacro
    ' The two watch commands, living in the WATCH caption band. PsButtons rather than painted
    ' glyphs so they bring their own tooltips, hover and press states -- and, for the
    ' trashcan, a real disabled state when there is nothing selected to delete.
    #macro MAKECAPBUTTON( h, id, glyph, tip, cb )
        h = PsButton_Create( HWND_FRMDEBUG, id )
        PsButton_SetGlyphLeft( h, glyph )
        PsButton_SetGlyphFont( h, ghFont(SYMBOLFONT_9) )
        PsButton_SetFont( h, ghFont(GUIFONT_9) )
        PsButton_SetPadding( h, 0, 0, 0, 0 )
        PsButton_SetTooltipMode( h, PSTIP_MODE_PS )
        PsButton_SetTooltipText( h, tip )
        PsButton_SetClickCallback( h, cb )
    #endmacro
    MAKECAPBUTTON( gDbgBtnAddWatch, IDC_FRMDEBUG_CMDADDWATCH, wszIconAddFileButton, _
                   L(461, "Add Watch"), @frmDebug_AddWatchClick )
    MAKECAPBUTTON( gDbgBtnDelWatch, IDC_FRMDEBUG_CMDDELWATCH, wszIconTrash, _
                   L(460, "Delete Watch"), @frmDebug_DelWatchClick )

    MAKESPLITTER( HWND_FRMDEBUG_SPLITMAIN,  IDC_FRMDEBUG_SPLITMAIN,  PSSPLITTER_VERTICAL )
    MAKESPLITTER( HWND_FRMDEBUG_SPLITLEFT,  IDC_FRMDEBUG_SPLITLEFT,  PSSPLITTER_HORIZONTAL )
    MAKESPLITTER( HWND_FRMDEBUG_SPLITRIGHT, IDC_FRMDEBUG_SPLITRIGHT, PSSPLITTER_HORIZONTAL )

    frmDebug_ApplyTheme()
    frmDebug_PositionWindows()

    ' Every Ps* control is created WS_CHILD but NOT WS_VISIBLE: the family leaves showing
    ' them to the host, which has to position them first. Miss this and the window paints
    ' its background and nothing else.
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) then ShowWindow( gDbgBtn(id), SW_SHOW )
    next
    ShowWindow( HWND_FRMDEBUG_LVGLOBALS,   SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_LVLOCALS,    SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_LVSTACK,     SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_LVWATCH,     SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_SPLITMAIN,   SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_SPLITLEFT,   SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_SPLITRIGHT,  SW_SHOW )
    ShowWindow( gDbgBtnAddWatch,           SW_SHOW )
    ShowWindow( gDbgBtnDelWatch,           SW_SHOW )

    ' --- seed the breakpoints the user set in the editor ---
    '
    ' Traced under TIKO_DEBUGWIN_TRACE because a failure here is SILENT and looks exactly
    ' like "the debugger ignores my breakpoints": every step of it can fail for a different
    ' reason (the document has no Scintilla yet, its filename does not match a compiled
    ' source, the line generated no code) and none of them says anything.
    dim as boolean bTrace = (environ("TIKO_DEBUGWIN_TRACE") = "1")
    dim as string sSeed
    if bTrace then print "--- TIKO_DEBUGWIN_TRACE: breakpoint seeding ---"

    debugparser_clear_breakpoints()
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if pDoc->hWindow(0) then
            dim as wstring * 520 wszFile = pDoc->DiskFilename
            dim as long si = debugparser_source_index( @wszFile )
            if bTrace then
                sSeed = "  doc " & str(pDoc->DiskFilename) & "  srcIndex=" & str(si) & _
                      "  markers=[" & str(pDoc->GetBreakPoints()) & "]"
                print sSeed
            end if
            if si >= 0 then
                dim as DWSTRING wszBps = pDoc->GetBreakPoints()
                dim as long n = PsStrParseCount( wszBps, "," )
                for i as long = 1 to n
                    dim as DWSTRING wszOne = PsStrParse( wszBps, i, "," )
                    if PsLen(PsTrim(wszOne)) = 0 then continue for
                    ' Editor lines are 0-based; the engine's are 1-based. That boundary is
                    ' stated once, here, because the old gdb path converted it in four
                    ' places and got one of them wrong.
                    dim as long ln = valint(wszOne) + 1
                    ' The line table says which lines actually generated code, so a
                    ' breakpoint on a comment or a declaration moves forward to the next
                    ' real one instead of silently never firing.
                    dim as long lnExec = debugparser_next_executable_line( si, ln )
                    if lnExec > 0 then
                        if debugparser_add_breakpoint( si, lnExec ) = DBGP_OK then
                            ' Whether ANY breakpoint is armed decides what the first stop
                            ' means -- see frmDebug_OnStopped.
                            gDbgHaveBreakpoints = true
                        end if
                    end if
                    if bTrace then
                        sSeed = "    editor line " & str(ln - 1) & " -> engine line " & str(ln) & _
                              " -> executable " & str(lnExec)
                        if lnExec <= 0 then sSeed = sSeed & "   NO CODE ON OR AFTER THIS LINE"
                        print sSeed
                    end if
                next
            end if
        elseif bTrace then
            sSeed = "  doc " & str(pDoc->DiskFilename) & "  HAS NO SCINTILLA -- skipped"
            print sSeed
        end if
        pDoc = pDoc->pDocNext
    loop
    if bTrace then
        sSeed = "  seeded any breakpoint = " & str(gDbgHaveBreakpoints) & _
              "   (false => the session stops at the program entry, by design)"
        print sSeed
        print "--- end ---"
    end if

    ShowWindow( HWND_FRMDEBUG, SW_SHOW )

    ' AFTER ShowWindow, deliberately. AfxForceVisibleDisplay reads GetWindowRect, so before
    ' the window has its final rect it is a no-op -- frmOutputFloat records the same ordering
    ' for the same reason. This is what rescues a window whose stored geometry belongs to a
    ' monitor that is no longer attached.
    AfxForceVisibleDisplay( HWND_FRMDEBUG )
    UpdateWindow( HWND_FRMDEBUG )

    gApp.IsDebuggerActive = true
    frmMain_DebugTopMenuStates()

    dim as wstring * 1024 wszParams = cmdparams
    if debugparser_start( @wszExe, @wszParams, HWND_FRMDEBUG ) <> DBGP_OK then
        dim as wstring * 512 werr
        debugparser_get_error( @werr, 512 )
        gApp.IsDebuggerActive = false
        gDbgSessionEnding = true
        TikoMsgBox( hWndParent, L(446, "The debugger could not start the program.") & _
                    chr(13) & chr(13) & werr, L(214, "Confirm"), TMB_ICON_ERROR, TMB_OK )
        DestroyWindow( HWND_FRMDEBUG )
        return 0
    end if

    frmDebug_SyncButtons()
    return 0
end function


' ==========================================================================================
' TIKO_DEBUG_SELFTEST=1
'
' WHAT THIS SUITE CANNOT REACH, SAID UP FRONT RATHER THAN LEFT TO BE DISCOVERED.
'
' It asserts pure functions: the pane geometry, the splitter percent round trip, the array
' group shape, the sort comparator, the change-detection rule, the datatip expression walk,
' and the localized strings. It does NOT launch a debuggee, so it cannot tell you whether the
' debugger stops, steps, unwinds, or renders anything at all. Every defect found in this
' window so far -- the double-scaled client size, the controls created without WS_VISIBLE,
' the splitter painted in the background colour, the one-shot default spent before the
' control existed -- would pass every assertion below, because each produces code that runs
' and answers correctly when asked.
'
' TWO THINGS IN THIS CHANGE ARE SPECIFICALLY BEYOND IT, and are named rather than left to be
' assumed covered:
'
'   - LAZY EXPANSION. Whether a twisty click actually reaches frmDebug_ExpandRowChildren
'     needs a control, a message pump and a stopped debuggee. The GROUPING arithmetic is
'     asserted; the wiring that invokes it is not, so a node that silently shows nothing but
'     its placeholder for ever would pass this suite.
'   - EXPANSION SURVIVING A STEP. It is a capture out of live rows and a restore into rebuilt
'     ones, so it needs both a control and two stops. Verified instead by
'     TIKO_DEBUG_TRACEEXPAND, which opens the tree on the FIRST stop only and then merely
'     dumps it -- re-expanding every stop would print an open tree either way and prove
'     nothing. Measured: 323 rows held across six steps, nested group elements included.
'   - RUN TO THE FIRST BREAKPOINT. It is a branch on the first MSG_DBGP_STOPPED of a live
'     session. Nothing here posts that message.
'
' The engine's own behaviour is verified separately and headlessly, by debugparser_test.exe
' in C:\dev\debugParser. The interactive pass is what covers this window.
' ==========================================================================================
dim shared gDbgTestPass as long
dim shared gDbgTestFail as long

private sub frmDebug_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gDbgTestPass += 1
    else
        gDbgTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

sub frmDebug_RunSelfTest()
    if environ("TIKO_DEBUG_SELFTEST") <> "1" then exit sub

    gDbgTestPass = 0
    gDbgTestFail = 0
    dim as string sLine
    print "--- TIKO_DEBUG_SELFTEST ---"

    ' ---- layout ------------------------------------------------------------------------
    ' 1575 x 1085 is the real client at 175%, read off a live window by TIKO_DEBUGWIN_TRACE.
    dim as FRMDEBUG_LAYOUT lo
    dim as long cx = 1575, cy = 1085
    dim as long nTool = 77, nStatus = 45, nCap = 46, nSplit = 10
    dim as long nTop  = nTool + nStatus
    dim as long barMain = 945, barLeft = 600, barRight = 700
    frmDebug_ComputeLayout( cx, cy, barMain, barLeft, barRight, nTool, nStatus, nCap, nSplit, lo )

    frmDebug_Check( "status band sits directly under the toolbar", _
        lo.rcStatus.top = nTool, str(lo.rcStatus.top) )
    frmDebug_Check( "the globals caption starts where the status band ends", _
        lo.rcCapGlobals.top = nTop, str(lo.rcCapGlobals.top) )
    frmDebug_Check( "the globals tree starts where its caption ends", _
        lo.rcGlobals.top = lo.rcCapGlobals.bottom, str(lo.rcGlobals.top) )
    frmDebug_Check( "the call stack caption starts where the status band ends", _
        lo.rcCapStack.top = nTop, str(lo.rcCapStack.top) )

    ' Every pane must TILE with its neighbours: no overlap and no gap, or the window shows a
    ' seam of its own background between them.
    frmDebug_Check( "globals ends exactly at the left bar", _
        lo.rcGlobals.bottom = lo.rcSplitLeft.top, _
        str(lo.rcGlobals.bottom) & " vs " & str(lo.rcSplitLeft.top) )
    frmDebug_Check( "the locals caption starts exactly where the left bar ends", _
        lo.rcCapLocals.top = lo.rcSplitLeft.bottom, _
        str(lo.rcCapLocals.top) & " vs " & str(lo.rcSplitLeft.bottom) )
    frmDebug_Check( "locals starts exactly where its caption ends", _
        lo.rcLocals.top = lo.rcCapLocals.bottom, str(lo.rcLocals.top) )
    frmDebug_Check( "the call stack ends exactly at the right bar", _
        lo.rcStack.bottom = lo.rcSplitRight.top, _
        str(lo.rcStack.bottom) & " vs " & str(lo.rcSplitRight.top) )
    frmDebug_Check( "the watch caption starts exactly where the right bar ends", _
        lo.rcCapWatch.top = lo.rcSplitRight.bottom, str(lo.rcCapWatch.top) )
    frmDebug_Check( "watch starts exactly where its caption ends", _
        lo.rcWatch.top = lo.rcCapWatch.bottom, str(lo.rcWatch.top) )

    frmDebug_Check( "the left column ends exactly at the main bar", _
        (lo.rcGlobals.right = lo.rcSplitMain.left) andalso (lo.rcLocals.right = lo.rcSplitMain.left), _
        str(lo.rcGlobals.right) & "/" & str(lo.rcLocals.right) & " vs " & str(lo.rcSplitMain.left) )
    frmDebug_Check( "the right column starts exactly where the main bar ends", _
        (lo.rcStack.left = lo.rcSplitMain.right) andalso (lo.rcWatch.left = lo.rcSplitMain.right), _
        str(lo.rcStack.left) & "/" & str(lo.rcWatch.left) & " vs " & str(lo.rcSplitMain.right) )
    frmDebug_Check( "a caption is exactly as wide as the pane it names", _
        (lo.rcCapGlobals.right = lo.rcGlobals.right) andalso (lo.rcCapLocals.right = lo.rcLocals.right) andalso _
        (lo.rcCapStack.right = lo.rcStack.right) andalso (lo.rcCapWatch.right = lo.rcWatch.right) )
    frmDebug_Check( "a caption starts at the same left edge as its pane", _
        (lo.rcCapGlobals.left = lo.rcGlobals.left) andalso (lo.rcCapStack.left = lo.rcStack.left) )
    frmDebug_Check( "the bottom panes reach the bottom of the client", _
        (lo.rcLocals.bottom = cy) andalso (lo.rcWatch.bottom = cy) andalso (lo.rcSplitMain.bottom = cy), _
        str(lo.rcLocals.bottom) & "/" & str(lo.rcWatch.bottom) )
    frmDebug_Check( "the right column reaches the right edge", _
        (lo.rcStack.right = cx) andalso (lo.rcWatch.right = cx), str(lo.rcStack.right) )
    frmDebug_Check( "the main bar is the requested thickness", _
        (lo.rcSplitMain.right - lo.rcSplitMain.left) = nSplit )
    frmDebug_Check( "both horizontal bars are the requested thickness", _
        ((lo.rcSplitLeft.bottom - lo.rcSplitLeft.top) = nSplit) andalso _
        ((lo.rcSplitRight.bottom - lo.rcSplitRight.top) = nSplit) )
    frmDebug_Check( "a horizontal bar spans exactly its own column", _
        (lo.rcSplitLeft.right = barMain) andalso (lo.rcSplitRight.left = lo.rcSplitMain.right) )
    frmDebug_Check( "no pane has negative width", _
        (lo.rcGlobals.right >= lo.rcGlobals.left) andalso (lo.rcStack.right >= lo.rcStack.left) )

    ' A bar dragged hard against an edge must not give a pane a negative extent --
    ' DeferWindowPos with a negative extent is not an error, it is a silently wrong window.
    frmDebug_ComputeLayout( cx, cy, cx - 2, barLeft, barRight, nTool, nStatus, nCap, nSplit, lo )
    frmDebug_Check( "a main bar past the right edge clamps the right column to zero, not negative", _
        lo.rcStack.right >= lo.rcStack.left, _
        str(lo.rcStack.left) & ".." & str(lo.rcStack.right) )

    ' ---- the collapsed icon strip -------------------------------------------------------
    ' 7 icons of 36x36 at 100%, the size PsButton measures an icon-only button at.
    dim as long stripCX, stripCY
    frmDebug_ComputeStripSize( 7 * 36, 36, 7, 8, 4, stripCX, stripCY )
    frmDebug_Check( "the strip is padding + icons + the gaps between them", _
        stripCX = (8 + 7 * 36 + 6 * 4 + 8), str(stripCX) )
    frmDebug_Check( "the strip is one icon tall plus its padding", _
        stripCY = (8 + 36 + 8), str(stripCY) )
    scope
        dim as long cx1, cy1
        frmDebug_ComputeStripSize( 36, 36, 1, 8, 4, cx1, cy1 )
        frmDebug_Check( "one icon costs no gap", cx1 = (8 + 36 + 8), str(cx1) )
    end scope

    ' The strip is parked top-RIGHT of the editing area, and INSIDE it: getting the sign
    ' wrong puts the only way back to the debugger off-screen.
    scope
        dim as RECT rcAnchor, rcStub
        SetRect( @rcAnchor, 200, 100, 1400, 900 )
        frmDebug_ComputeStubRect( rcAnchor, 300, 60, 12, rcStub )
        frmDebug_Check( "the strip sits at the top-right of the editing area, inset", _
            (rcStub.right = rcAnchor.right - 12) andalso (rcStub.top = rcAnchor.top + 12), _
            str(rcStub.left) & "," & str(rcStub.top) )
        frmDebug_Check( "the strip is the size it was asked for", _
            ((rcStub.right - rcStub.left) = 300) andalso ((rcStub.bottom - rcStub.top) = 60) )
        frmDebug_Check( "the strip stays inside the editing area", _
            (rcStub.left >= rcAnchor.left) andalso (rcStub.right <= rcAnchor.right) )

        ' An editing area narrower than the strip clamps rather than sliding off the left.
        SetRect( @rcAnchor, 200, 100, 400, 900 )
        frmDebug_ComputeStubRect( rcAnchor, 300, 60, 12, rcStub )
        frmDebug_Check( "a narrow editing area clamps the strip instead of losing it", _
            rcStub.left >= rcAnchor.left, str(rcStub.left) )
    end scope

    ' ---- splitter percent round trip ----------------------------------------------------
    ' The stored form is a percent of the span, so restoring into a DIFFERENT span has to put
    ' the bar in the same relative place rather than at the same pixel.
    frmDebug_Check( "a bar at the middle of a span is 50%", _
        frmDebug_PosToPct( 500, 0, 1000 ) = 5000, str(frmDebug_PosToPct(500, 0, 1000)) )
    frmDebug_Check( "50% of a span lands at its middle", _
        frmDebug_PctToPos( 5000, 0, 1000 ) = 500, str(frmDebug_PctToPos(5000, 0, 1000)) )
    frmDebug_Check( "the round trip is stable over a non-zero origin", _
        frmDebug_PctToPos( frmDebug_PosToPct( 700, 200, 1200 ), 200, 1200 ) = 700, _
        str(frmDebug_PctToPos( frmDebug_PosToPct(700, 200, 1200), 200, 1200 )) )
    frmDebug_Check( "the same percent scales to a bigger span", _
        frmDebug_PctToPos( frmDebug_PosToPct( 500, 0, 1000 ), 0, 2000 ) = 1000, _
        str(frmDebug_PctToPos( frmDebug_PosToPct(500, 0, 1000), 0, 2000 )) )
    frmDebug_Check( "a degenerate span answers its origin rather than dividing by zero", _
        (frmDebug_PctToPos( 5000, 400, 400 ) = 400) andalso (frmDebug_PosToPct( 400, 400, 400 ) = 0) )
    frmDebug_Check( "a percent outside the scale is clamped, not wrapped", _
        (frmDebug_PctToPos( -1, 0, 1000 ) = 0) andalso (frmDebug_PctToPos( 99999, 0, 1000 ) = 1000) )

    ' ---- array index groups -------------------------------------------------------------
    ' The whole point of the group tree is that a very large array stays navigable, so the
    ' sizes asserted here are ones no test could actually allocate.
    frmDebug_Check( "an array that fits in one chunk is not grouped at all", _
        frmDebug_GroupSpan( FRMDEBUG_CHUNK ) = 0, str(frmDebug_GroupSpan(FRMDEBUG_CHUNK)) )
    frmDebug_Check( "one element over a chunk starts grouping", _
        frmDebug_GroupSpan( FRMDEBUG_CHUNK + 1 ) = FRMDEBUG_CHUNK )
    frmDebug_Check( "10,000 elements is 100 groups of 100", _
        frmDebug_GroupSpan( 10000 ) = 100, str(frmDebug_GroupSpan(10000)) )
    frmDebug_Check( "10,001 elements adds a level rather than a 101st group", _
        frmDebug_GroupSpan( 10001 ) = 10000, str(frmDebug_GroupSpan(10001)) )
    frmDebug_Check( "1,000,000 elements is 100 groups of 10,000", _
        frmDebug_GroupSpan( 1000000 ) = 10000, str(frmDebug_GroupSpan(1000000)) )

    ' No level may ever produce more groups than the cap -- that is the property the
    ' adaptive span exists to guarantee, and it is worth asserting directly rather than
    ' inferring it from the five sizes above.
    dim as boolean bCapOK = true
    dim as longint probe(0 to 6) = { 101, 999, 10000, 10001, 250000, 1000000, 99999999 }
    for i as long = 0 to 6
        dim as longint sp = frmDebug_GroupSpan( probe(i) )
        if sp > 0 then
            if ((probe(i) + sp - 1) \ sp) > FRMDEBUG_MAXGROUPS then
                bCapOK = false
                sLine = "    (" & str(probe(i)) & " -> " & str((probe(i) + sp - 1) \ sp) & " groups)"
                print sLine
            end if
        end if
    next
    frmDebug_Check( "no array size produces more than FRMDEBUG_MAXGROUPS groups at a level", bCapOK )

    ' The groups must TILE the index space exactly: every element in exactly one group, no
    ' gap and no overlap. A slip here silently drops the last element of every array, or
    ' lists one twice, and the group captions still look perfectly plausible.
    dim as boolean bTileOK = true
    dim as longint sizes(0 to 4) = { 101, 250, 10000, 10001, 1000000 }
    for i as long = 0 to 4
        dim as longint total = sizes(i)
        dim as longint sp = frmDebug_GroupSpan( total )
        if sp <= 0 then continue for
        dim as longint g = 0, gLo, gHi, expectNext = 0
        do while frmDebug_GroupBounds( 0, total, sp, g, gLo, gHi )
            if gLo <> expectNext then bTileOK = false
            if gHi < gLo then bTileOK = false
            expectNext = gHi + 1
            g += 1
        loop
        if expectNext <> total then
            bTileOK = false
            sLine = "    (" & str(total) & " covered up to " & str(expectNext) & ")"
            print sLine
        end if
    next
    frmDebug_Check( "index groups tile the array exactly, with no gap and no overlap", bTileOK )

    ' The offset form is what a nested group uses -- it re-groups a RANGE, not an array from
    ' zero, so the base index has to carry through.
    dim as longint nLo, nHi
    frmDebug_GroupBounds( 500, 250, 100, 1, nLo, nHi )
    frmDebug_Check( "a nested group's bounds are offset by its parent's start", _
        (nLo = 600) andalso (nHi = 699), str(nLo) & ".." & str(nHi) )
    frmDebug_GroupBounds( 500, 250, 100, 2, nLo, nHi )
    frmDebug_Check( "the last group is short rather than running past the end", _
        (nLo = 700) andalso (nHi = 749), str(nLo) & ".." & str(nHi) )
    frmDebug_Check( "there is no group past the last one", _
        frmDebug_GroupBounds( 500, 250, 100, 3, nLo, nHi ) = false )

    ' A group's own span re-enters the same function when it is expanded, so the recursion
    ' has to terminate: each level must be strictly smaller than the one above it.
    dim as longint spanTop = frmDebug_GroupSpan( 1000000 )
    frmDebug_Check( "expanding a group yields a strictly smaller span, so the nesting ends", _
        (frmDebug_GroupSpan( spanTop ) > 0) andalso (frmDebug_GroupSpan( spanTop ) < spanTop), _
        str(spanTop) & " -> " & str(frmDebug_GroupSpan(spanTop)) )

    ' ---- the sort comparator ------------------------------------------------------------
    frmDebug_Check( "ascending puts a before b", frmDebug_SortLess( "alpha", "beta", false ) )
    frmDebug_Check( "ascending does not put b before a", frmDebug_SortLess( "beta", "alpha", false ) = false )
    frmDebug_Check( "descending reverses it", frmDebug_SortLess( "beta", "alpha", true ) )
    frmDebug_Check( "the compare is case-insensitive", frmDebug_SortLess( "Alpha", "beta", false ) )
    frmDebug_Check( "equal keys are not less than each other, so the sort stays stable", _
        (frmDebug_SortLess( "same", "SAME", false ) = false) andalso _
        (frmDebug_SortLess( "same", "SAME", true ) = false) )

    ' ---- change detection ---------------------------------------------------------------
    ' The rule that matters is what happens the FIRST time a key is seen: a variable coming
    ' into scope has not changed, it has appeared, and painting every local red on entry to a
    ' procedure would make the feature useless exactly when stepping starts.
    frmDebug_ValueReset()
    frmDebug_Check( "a value never seen before does not count as changed", _
        frmDebug_ValueChanged( "L0:count", "1" ) = false )
    frmDebug_Check( "the same value within one stop still does not count as changed", _
        frmDebug_ValueChanged( "L0:count", "1" ) = false )
    frmDebug_ValueRoll()
    frmDebug_Check( "an unchanged value after a stop is still not changed", _
        frmDebug_ValueChanged( "L0:count", "1" ) = false )
    frmDebug_ValueRoll()
    frmDebug_Check( "a different value after a stop IS changed", _
        frmDebug_ValueChanged( "L0:count", "2" ) )
    frmDebug_Check( "a second read within the same stop still reports it changed", _
        frmDebug_ValueChanged( "L0:count", "2" ) )
    frmDebug_ValueRoll()
    frmDebug_Check( "the change does not persist past the next stop", _
        frmDebug_ValueChanged( "L0:count", "2" ) = false )
    ' Scope is part of the key, so two frames' identically-named locals are separate.
    frmDebug_ValueReset()
    frmDebug_ValueChanged( "L0:i", "5" )
    frmDebug_ValueRoll()
    frmDebug_Check( "the same name in another frame is a different variable", _
        frmDebug_ValueChanged( "L1:i", "9" ) = false )
    frmDebug_Check( "an empty key is never reported as changed", _
        frmDebug_ValueChanged( "", "anything" ) = false )
    frmDebug_ValueReset()

    ' ---- the datatip expression walk ----------------------------------------------------
    ' Columns are 1-based, matching mid().
    dim as DWSTRING wszSrc = "    rec.inner.ix = n * 2"
    frmDebug_Check( "a bare name resolves to itself", _
        frmDebug_ExprFromLine( "    total = a + b", 7 ) = "total", _
        str(frmDebug_ExprFromLine( "    total = a + b", 7 )) )
    frmDebug_Check( "hovering the LAST member returns the whole dotted path", _
        frmDebug_ExprFromLine( wszSrc, 15 ) = "rec.inner.ix", _
        str(frmDebug_ExprFromLine( wszSrc, 15 )) )
    ' This is the asymmetry that matters: left walks through dots, right does not. Hovering
    ' the HEAD must not drag in members the cursor is not on.
    frmDebug_Check( "hovering the HEAD does not drag in the members after it", _
        frmDebug_ExprFromLine( wszSrc, 5 ) = "rec", _
        str(frmDebug_ExprFromLine( wszSrc, 5 )) )
    frmDebug_Check( "hovering a MIDDLE member stops at that member", _
        frmDebug_ExprFromLine( wszSrc, 9 ) = "rec.inner", _
        str(frmDebug_ExprFromLine( wszSrc, 9 )) )
    frmDebug_Check( "whitespace yields nothing", _
        len(frmDebug_ExprFromLine( wszSrc, 1 )) = 0 )
    frmDebug_Check( "an operator yields nothing", _
        len(frmDebug_ExprFromLine( wszSrc, 18 )) = 0, _
        str(frmDebug_ExprFromLine( wszSrc, 18 )) )
    frmDebug_Check( "a column past the end of the line yields nothing", _
        len(frmDebug_ExprFromLine( wszSrc, 9999 )) = 0 )
    frmDebug_Check( "column zero yields nothing", _
        len(frmDebug_ExprFromLine( wszSrc, 0 )) = 0 )
    frmDebug_Check( "an empty line yields nothing", _
        len(frmDebug_ExprFromLine( "", 1 )) = 0 )
    frmDebug_Check( "underscores and digits are part of a name", _
        frmDebug_ExprFromLine( "  my_var2 = 1", 3 ) = "my_var2", _
        str(frmDebug_ExprFromLine( "  my_var2 = 1", 3 )) )
    ' A member access written against a call result has no head we can evaluate; the leading
    ' dot must be stripped rather than passed to the engine as ".field".
    frmDebug_Check( "a leading dot is stripped", _
        frmDebug_ExprFromLine( "  ).field = 1", 5 ) = "field", _
        str(frmDebug_ExprFromLine( "  ).field = 1", 5 )) )

    ' ---- localization --------------------------------------------------------------------
    ' Every id this window uses must resolve. An id with no entry renders as an EMPTY string,
    ' so a caption or a whole message box comes up blank with nothing in the build or the
    ' assertions to say so.
    ' 624 is the retired "<click to add a watch>" prompt. It is deliberately still in the
    ' .lang files and deliberately NOT asserted: the slot stays claimed so a future feature
    ' does not reuse the id and inherit a stale translation in five languages.
    dim as long nBlank = 0
    for id as long = 432 to 461
        if id = 624 then continue for
        if len( LL(id) ) = 0 then
            nBlank += 1
            print "  FAIL: localized id " & id & " is empty"
        end if
    next
    frmDebug_Check( "every debugger string 600-629 resolves (624 retired)", nBlank = 0, str(nBlank) & " blank" )

    for id as long = 243 to 244
        frmDebug_Check( "reused menu string " & id & " still resolves", len(LL(id)) > 0 )
    next
    frmDebug_Check( "the restore-icon tooltip resolves", len(LL(521)) > 0 )
    frmDebug_Check( "reused menu string 255 still resolves", len(LL(251)) > 0 )
    frmDebug_Check( "reused menu string 256 still resolves", len(LL(252)) > 0 )

    ' len() > 0 CANNOT distinguish a real translation from the English fallback -- the
    ' loader fills a blank key with English, so this suite reads the same under any language
    ' file. Printing some of them is what actually distinguishes them. 247 is here because it
    ' was RE-WORDED rather than added ("Step Into" -> "Step"): a stale translation left behind
    ' in one of the six files still resolves, and only reading it back catches that.
    sLine = "  NOTE resolved: 247=" & str(LL(243)) & " | 618=" & str(LL(450)) & _
            " | 619=" & str(LL(451)) & " | 620=" & str(LL(452)) & _
            " | 627=" & str(LL(459)) & " | 629=" & str(LL(461))
    print sLine

    sLine = "--- TIKO_DEBUG_SELFTEST: " & str(gDbgTestPass) & " passed, " & _
            str(gDbgTestFail) & " failed ---"
    print sLine
end sub
