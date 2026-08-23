'    debugParser - FreeBASIC debug-information reader and debug engine
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
' dbgeval - turns a variable in the debuggee into something a person can read.
'
' Sits on dbginfo (what exists and what shape it is) and dbgprocess (what is in
' memory right now). Everything here runs on the UI thread while the debuggee is stopped and
' therefore frozen, which is what makes reading its memory directly safe -- see the
' threading note in dbgprocess.bi.
'
' ------------------------------------------------------------------------------------------
' ADDRESSING, WHICH IS THE PART THAT GOES WRONG SILENTLY
'
' Where a variable lives depends on its scope, and the debug format records the two cases in
' the same field:
'
'   shared / static / common   .addr is an ABSOLUTE address
'   local / byval parameter    .addr is a SIGNED offset from the frame pointer
'   byref parameter            as above, and then ONE MORE indirection -- the slot holds a
'                              pointer to the caller's variable, not the value
'
' Getting the byref case wrong shows the pointer as though it were the value, which for a
' UDT means reading a completely unrelated part of the stack and rendering it confidently.
'
' ------------------------------------------------------------------------------------------
' STRINGS
'
' FreeBASIC has four and they are not interchangeable:
'
'   ZSTRING     the address IS the characters; read to the first NUL
'   fixed-len   same, but the length is known from the field declaration
'   WSTRING     as ZSTRING but 2 bytes per character on Windows
'   STRING      a DESCRIPTOR -- the address holds { data pointer, len, size }, so the
'               characters need one extra read through the data pointer
'
' The descriptor's field offsets come from the debug info rather than being hardcoded,
' because fbc declares that struct's SIZE wrongly on 64-bit (it says 12 when it is 24) while
' the field offsets in the same declaration are right.
'
' ------------------------------------------------------------------------------------------
' DYNAMIC ARRAYS
'
' A REDIM array is a descriptor UDT. Its bounds are not in the debug info at all -- only in
' the descriptor, at run time. Every field is looked up BY NAME (DATA, DIMENSIONS, DIMTB),
' so the fbc version that built the target does not matter; FBdebugger hardcodes the layout
' behind its own compile-time version test and gets it wrong across versions.
' ==========================================================================================

#pragma once

#include once "dbginfo.bi"
#include once "dbgprocess.bi"

' How many characters of a string to render inline before truncating.
const as long DBG_STRPREVIEW = 160

' The largest element count a dynamic array is believed to really have. This is a GARBAGE
' DETECTOR, not a display cap: a REDIM descriptor that has not been executed yet is
' uninitialized stack, and the debugger is asked to display exactly that whenever it stops
' inside a procedure before its declarations have run. A billion elements is past anything a
' debuggee plausibly holds and far below the ~1e16 extents junk produces.
const as longint DBG_MAXELEMENTS = 1000000000ll

' One row in the variables tree. Children are produced on demand, never up front -- a UDT
' with a thousand elements must not cost a thousand memory reads to display collapsed.
type DBG_NODE
    nm          as string
    typeName    as string
    valueText   as string
    addr        as ulongint     ' resolved absolute address of this node's data
    typeId      as long
    descTypeId  as long        ' FBARRAY descriptor type for a dynamic array; see dbginfo.bi
    ptrDepth    as long
    arrayIndex  as long
    declBytes   as longint      ' fixed-length string size, 0 if not applicable
    bitOffset   as long
    bitLen      as long
    scope       as DBG_SCOPE
    hasChildren as boolean
    isValid     as boolean      ' false when the address could not be read
end type

' The shape of an array node, whichever kind it is. For a FIXED array this is a copy of the
' compile-time table entry; for a DYNAMIC one it is read from the live descriptor, which is the
' only place those bounds exist at all. One struct for both so a caller never has to ask which
' kind it is holding.
type DBG_ARRAYBOUNDS
    dims      as long
    lo(0 to DBG_MAXDIM - 1) as longint
    hi(0 to DBG_MAXDIM - 1) as longint
    elemSize  as longint        ' bytes per element, descriptor ELEMENT_LEN when available
    total     as longint        ' product of every extent
    isDynamic as boolean
end type

' Bounds of an array node. False when the node is not an array, or when the descriptor of a
' dynamic one cannot be read (unallocated, or the debuggee has gone away).
declare function DebugEval_ArrayBounds( byref node as DBG_NODE, byref dst as DBG_ARRAYBOUNDS ) as boolean

' Bytes per element for a type. Public because the array API needs it for fixed arrays, where
' there is no descriptor to ask.
declare function DebugEval_ElementSize( byval typeId as long, byval ptrDepth as long ) as longint

' Resolve a variable in a given stack frame into a displayable node.
declare function DebugEval_VarNode( byval varIndex as long, byval frameIndex as long, byref node as DBG_NODE ) as boolean

' Produce the children of a node (UDT fields, array elements, pointer target).
declare function DebugEval_ChildCount( byref node as DBG_NODE ) as long
declare function DebugEval_ChildNode ( byref parent as DBG_NODE, byval childIndex as long, byref node as DBG_NODE ) as boolean

' Format the value at an already-resolved address.
declare function DebugEval_FormatValue( byref node as DBG_NODE ) as string

' Name lookup, for watches and for hover datatips. Accepts a dotted path such as
' "rec.inner.ix"; array subscripts are not parsed (see the README note on scope).
declare function DebugEval_Lookup( byref expr as string, byval frameIndex as long, byref node as DBG_NODE ) as boolean

' Which variables are in scope for a frame, and the module-level ones.
declare function DebugEval_FrameVarFirst( byval frameIndex as long ) as long
declare function DebugEval_FrameVarLast ( byval frameIndex as long ) as long
declare function DebugEval_IsGlobal     ( byval varIndex as long ) as boolean
