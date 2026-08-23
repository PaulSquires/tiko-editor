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

' See dbgeval.bi for the addressing rules, the four string kinds, and why the dynamic
' array descriptor is resolved by field name rather than by a hardcoded layout.

#include once "dbgeval.bi"

' ==========================================================================================
' Descriptor field offsets, resolved by NAME out of the debug info rather than hardcoded.
' ==========================================================================================
private function FieldOffsetByName( byval typeId as long, byref wantName as string, byref outOfs as longint ) as boolean
    outOfs = 0
    if (typeId < 0) orelse (typeId >= gDbgTypeCount) then return false
    dim as long fi = gDbgType(typeId).fieldFirst
    if fi < 0 then return false
    for k as long = fi to gDbgType(typeId).fieldLast
        if gDbgField(k).nm = wantName then
            outOfs = gDbgField(k).byteOffset
            return true
        end if
    next
    return false
end function

private function IsStringDescriptor( byval typeId as long ) as boolean
    if (typeId < 0) orelse (typeId >= gDbgTypeCount) then return false
    return (gDbgType(typeId).nm = "string")
end function

private function IsFBArrayDescriptor( byval typeId as long ) as boolean
    if (typeId < 0) orelse (typeId >= gDbgTypeCount) then return false
    return (left( gDbgType(typeId).nm, 7 ) = "FBARRAY")
end function

' ==========================================================================================
' Array shape -- the ONE implementation
'
' This used to be written out three times (format the value, count the children, address a
' child), each copy re-reading the descriptor with its own hardcoded offsets. They drifted:
' the value formatter reported every dimension while the child walk refused anything but one,
' so a 2-D REDIM array displayed its full shape and then expanded to nothing.
'
' The descriptor is FBARRAY { ..., ELEMENT_LEN, DIMENSIONS, DIMTB[] } with DIMTB entries of
' { ELEMENTS, LBOUND, UBOUND }. Every one of those members is ssize_t, i.e. POINTER-SIZED --
' not 8 bytes. The old copies read 8 unconditionally, which on a 32-bit target walks straight
' off the end of each entry and into the next one, so the bounds came back as garbage. That
' target width is exactly the case this engine exists to support, since FBdebugger refuses it.
' ==========================================================================================
function DebugEval_ElementSize( byval typeId as long, byval ptrDepth as long ) as longint
    if ptrDepth > 0 then return DebugProc_PointerSize()
    if (typeId < 0) orelse (typeId >= gDbgTypeCount) then return 0
    if gDbgType(typeId).sizeBytes > 0 then return gDbgType(typeId).sizeBytes
    return DebugInfo_BaseSize( gDbgType(typeId).baseCode )
end function

function DebugEval_ArrayBounds( byref node as DBG_NODE, byref dst as DBG_ARRAYBOUNDS ) as boolean
    ' clear, not type<>() -- a UDT holding fixed-length arrays has no default constructor.
    clear dst, 0, sizeof(DBG_ARRAYBOUNDS)

    ' ---- fixed: the compile-time table already holds every dimension ----
    if node.arrayIndex >= 0 then
        if node.arrayIndex >= gDbgArrayCount then return false
        dst.isDynamic = false
        dst.dims      = gDbgArray(node.arrayIndex).dims
        if dst.dims > DBG_MAXDIM then dst.dims = DBG_MAXDIM
        dst.total = 1
        for d as long = 0 to dst.dims - 1
            dst.lo(d) = gDbgArray(node.arrayIndex).lo(d)
            dst.hi(d) = gDbgArray(node.arrayIndex).hi(d)
            dim as longint extent = dst.hi(d) - dst.lo(d) + 1
            if extent < 0 then extent = 0
            dst.total *= extent
        next
        dst.elemSize = DebugEval_ElementSize( node.typeId, 0 )
        return (dst.dims > 0)
    end if

    if node.arrayIndex <> DBG_ARR_DYNAMIC then return false

    ' ---- dynamic: bounds exist ONLY in the live descriptor ----
    dim as longint ps = DebugProc_PointerSize()
    dim as longint ofsDims, ofsTb, ofsElem, ofsData
    dim as ulongint dataPtr

    if FieldOffsetByName( node.descTypeId, "DATA", ofsData ) = false then return false
    if DebugProc_ReadPtr( node.addr + ofsData, dataPtr ) = false then return false
    if dataPtr = 0 then return false                      ' declared but never REDIMmed
    if FieldOffsetByName( node.descTypeId, "DIMENSIONS", ofsDims ) = false then return false
    if FieldOffsetByName( node.descTypeId, "DIMTB", ofsTb ) = false then return false

    ' A descriptor whose declaration has not EXECUTED yet is uninitialized stack, and the
    ' debugger will happily be asked to display it -- stepping into a procedure stops before
    ' its REDIM does. So every field is sanity-checked and a failure REJECTS the node rather
    ' than being clamped into range: clamping is what made this look like a real answer.
    ' Reading garbage as "8 dimensions of 9.4e16 elements" is worse than reading nothing,
    ' because the caller cannot tell it apart from a real array.
    dim as longint nd = 0
    if DebugProc_ReadMem( node.addr + ofsDims, @nd, ps ) = false then return false
    if (nd <= 0) orelse (nd > DBG_MAXDIM) then return false      ' NOT clamped -- rejected

    dim as longint stride = ps * 3                        ' { ELEMENTS, LBOUND, UBOUND }
    dst.isDynamic = true
    dst.dims      = cast(long, nd)
    dst.total     = 1
    for d as long = 0 to dst.dims - 1
        dim as longint lo_ = 0, hi_ = 0
        if DebugProc_ReadMem( node.addr + ofsTb + d * stride + ps,     @lo_, ps ) = false then return false
        if DebugProc_ReadMem( node.addr + ofsTb + d * stride + ps * 2, @hi_, ps ) = false then return false
        dim as longint extent = hi_ - lo_ + 1
        ' An extent past the ceiling is not a big array, it is not an array. The product is
        ' checked as it accumulates rather than at the end, because garbage extents multiply
        ' straight through a longint and can land back on a small positive number -- the
        ' first version of this reported 8 junk dimensions and a total of ZERO.
        if (extent < 0) orelse (extent > DBG_MAXELEMENTS) then return false
        dst.lo(d) = lo_
        dst.hi(d) = hi_
        dst.total *= extent
        if dst.total > DBG_MAXELEMENTS then return false
    next

    ' The descriptor knows the real stride; the type table is only a fallback.
    if FieldOffsetByName( node.descTypeId, "ELEMENT_LEN", ofsElem ) then
        DebugProc_ReadMem( node.addr + ofsElem, @dst.elemSize, ps )
    end if
    if dst.elemSize <= 0 then dst.elemSize = DebugEval_ElementSize( node.typeId, 0 )

    return true
end function

' ==========================================================================================
' Reading raw values
' ==========================================================================================
private function ReadZString( byval addr as ulongint, byval maxLen as long ) as string
    if addr = 0 then return ""
    dim as long cap = maxLen
    if (cap <= 0) orelse (cap > DBG_STRPREVIEW) then cap = DBG_STRPREVIEW
    dim as string buf = space(cap)
    ' A short read is normal at the end of a page, so shrink until something comes back.
    dim as long n = cap
    do while n > 0
        if DebugProc_ReadMem( addr, strptr(buf), n ) then exit do
        n \= 2
    loop
    if n <= 0 then return ""
    dim as long z = instr( left(buf, n), chr(0) )
    if z > 0 then return left( buf, z - 1 )
    return left( buf, n )
end function

private function ReadWString( byval addr as ulongint, byval maxChars as long ) as string
    if addr = 0 then return ""
    dim as long cap = maxChars
    if (cap <= 0) orelse (cap > DBG_STRPREVIEW) then cap = DBG_STRPREVIEW
    dim as string outS
    for i as long = 0 to cap - 1
        dim as ushort ch
        if DebugProc_ReadMem( addr + i * 2, @ch, 2 ) = false then exit for
        if ch = 0 then exit for
        ' Anything outside the BMP low range is rendered as a dot rather than mangled.
        if (ch >= 32) andalso (ch < 127) then
            outS &= chr(ch)
        else
            outS &= "."
        end if
    next
    return outS
end function

' ==========================================================================================
' DebugEval_FormatValue
' ==========================================================================================
function DebugEval_FormatValue( byref node as DBG_NODE ) as string
    if node.isValid = false then return "<unavailable>"
    if node.addr = 0 then return "<null>"

    ' ---- pointers -------------------------------------------------------------------
    if node.ptrDepth >= DBG_PTR_SUB_BASE then
        dim as ulongint target
        if DebugProc_ReadPtr( node.addr, target ) = false then return "<unreadable>"
        if target = 0 then return "0 (null)"
        dim as long pi = DebugInfo_ProcFromAddr( cast(longint, target) )
        if pi >= 0 then return "&h" & hex(target) & "  " & gDbgProc(pi).nm
        return "&h" & hex(target)
    end if

    if node.ptrDepth > 0 then
        dim as ulongint target
        if DebugProc_ReadPtr( node.addr, target ) = false then return "<unreadable>"
        if target = 0 then return "0 (null)"
        dim as string s = "&h" & hex(target)
        ' A pointer to characters is far more useful shown as the text it points at.
        if node.ptrDepth = 1 then
            if (node.typeId >= 0) andalso (node.typeId < gDbgTypeCount) then
                if gDbgType(node.typeId).baseCode = -2 then
                    dim as string txt = ReadZString( target, DBG_STRPREVIEW )
                    if len(txt) then s &= "  """ & txt & """"
                end if
            end if
        end if
        return s
    end if

    ' ---- arrays ---------------------------------------------------------------------
    if (node.arrayIndex = DBG_ARR_DYNAMIC) orelse (node.arrayIndex >= 0) then
        dim as DBG_ARRAYBOUNDS ab
        if DebugEval_ArrayBounds( node, ab ) = false then
            ' A dynamic array reaches here when it has been declared but never REDIMmed.
            if node.arrayIndex = DBG_ARR_DYNAMIC then return "<not allocated>"
            return "<array>"
        end if
        dim as string sh = "("
        for d as long = 0 to ab.dims - 1
            if d then sh &= ", "
            sh &= str(ab.lo(d)) & " to " & str(ab.hi(d))
        next
        return sh & ")  " & str(ab.total) & " element" & iif( ab.total = 1, "", "s" )
    end if

    ' ---- strings --------------------------------------------------------------------
    if IsStringDescriptor( node.typeId ) then
        ' A descriptor: one indirection to reach the characters. Offsets come from the
        ' debug info because fbc declares this struct's SIZE wrongly on 64-bit.
        dim as longint ofsData, ofsLen
        dim as ulongint dataPtr
        dim as longint slen = 0
        if FieldOffsetByName( node.typeId, "data", ofsData ) = false then return "<string>"
        if DebugProc_ReadPtr( node.addr + ofsData, dataPtr ) = false then return "<unreadable>"
        if dataPtr = 0 then return """"" (empty)"
        if FieldOffsetByName( node.typeId, "len", ofsLen ) then
            DebugProc_ReadMem( node.addr + ofsLen, @slen, DebugProc_PointerSize() )
        end if
        dim as string txt = ReadZString( dataPtr, iif( slen > 0, cast(long, slen), DBG_STRPREVIEW ) )
        return """" & txt & """  (len = " & str(slen) & ")"
    end if

    dim as long baseCode = 0
    if (node.typeId >= 0) andalso (node.typeId < gDbgTypeCount) then baseCode = gDbgType(node.typeId).baseCode

    ' A char-typed slot is a ZSTRING. Fields carry a declared length; a LOCAL does not --
    ' fbc emits "zstring * 8" as bare char with no size anywhere -- so it is read to the
    ' first NUL. Nothing else uses this type: a plain FB `byte` is t2, never t4.
    if baseCode = -2 then
        dim as long cap = iif( node.declBytes > 1, cast(long, node.declBytes), DBG_STRPREVIEW )
        return """" & ReadZString( node.addr, cap ) & """"
    end if
    ' Likewise a ushort-typed slot with a declared length is a fixed-length WSTRING.
    if (baseCode = -7) andalso (node.declBytes > 2) then
        return """" & ReadWString( node.addr, cast(long, node.declBytes \ 2) ) & """"
    end if

    ' ---- enums ----------------------------------------------------------------------
    if (node.typeId >= 0) andalso (node.typeId < gDbgTypeCount) then
        if gDbgType(node.typeId).isEnum then
            dim as long v
            if DebugProc_ReadMem( node.addr, @v, 4 ) = false then return "<unreadable>"
            dim as long fi = gDbgType(node.typeId).fieldFirst
            if fi >= 0 then
                for k as long = fi to gDbgType(node.typeId).fieldLast
                    if gDbgField(k).enumValue = v then return str(v) & "  (" & gDbgField(k).nm & ")"
                next
            end if
            return str(v)
        end if
    end if

    ' ---- bitfields ------------------------------------------------------------------
    if node.bitLen > 0 then
        dim as ulongint raw = 0
        dim as long nb = (node.bitOffset + node.bitLen + 7) \ 8
        if nb > 8 then nb = 8
        if DebugProc_ReadMem( node.addr, @raw, nb ) = false then return "<unreadable>"
        raw = raw shr node.bitOffset
        dim as ulongint mask = (cast(ulongint, 1) shl node.bitLen) - 1
        return str( raw and mask )
    end if

    ' ---- built-in scalars -----------------------------------------------------------
    select case baseCode
        case -1, -8                                  ' 4-byte int / uint
            dim as long v
            if DebugProc_ReadMem( node.addr, @v, 4 ) = false then return "<unreadable>"
            if baseCode = -8 then return str( cast(ulong, v) )
            return str(v)
        case -2, -6                                  ' char / signed char
            dim as byte v
            if DebugProc_ReadMem( node.addr, @v, 1 ) = false then return "<unreadable>"
            return str(v)
        case -5                                      ' unsigned char
            dim as ubyte v
            if DebugProc_ReadMem( node.addr, @v, 1 ) = false then return "<unreadable>"
            return str(v)
        case -3                                      ' short
            dim as short v
            if DebugProc_ReadMem( node.addr, @v, 2 ) = false then return "<unreadable>"
            return str(v)
        case -7                                      ' ushort
            dim as ushort v
            if DebugProc_ReadMem( node.addr, @v, 2 ) = false then return "<unreadable>"
            return str(v)
        case -12                                     ' single
            dim as single v
            if DebugProc_ReadMem( node.addr, @v, 4 ) = false then return "<unreadable>"
            return str(v)
        case -13                                     ' double
            dim as double v
            if DebugProc_ReadMem( node.addr, @v, 8 ) = false then return "<unreadable>"
            return str(v)
        case -16                                     ' boolean
            dim as ubyte v
            if DebugProc_ReadMem( node.addr, @v, 1 ) = false then return "<unreadable>"
            return iif( v <> 0, "true", "false" )
        case -31                                     ' longint
            dim as longint v
            if DebugProc_ReadMem( node.addr, @v, 8 ) = false then return "<unreadable>"
            return str(v)
        case -32                                     ' ulongint
            dim as ulongint v
            if DebugProc_ReadMem( node.addr, @v, 8 ) = false then return "<unreadable>"
            return str(v)
        case -11                                     ' void
            return ""
    end select

    ' ---- aggregates -----------------------------------------------------------------
    if (node.typeId >= 0) andalso (node.typeId < gDbgTypeCount) then
        if gDbgType(node.typeId).fieldFirst >= 0 then
            dim as long n = gDbgType(node.typeId).fieldLast - gDbgType(node.typeId).fieldFirst + 1
            return "{ " & str(n) & " field" & iif( n = 1, "", "s" ) & " }"
        end if
    end if

    return "<&h" & hex(node.addr) & ">"
end function

' ==========================================================================================
' Scope
' ==========================================================================================
function DebugEval_FrameVarFirst( byval frameIndex as long ) as long
    if (frameIndex < 0) orelse (frameIndex >= gDbgStop.frameCount) then return -1
    dim as long pi = gDbgStop.frames(frameIndex).procIndex
    if (pi < 0) orelse (pi >= gDbgProcCount) then return -1
    return gDbgProc(pi).varFirst
end function

function DebugEval_FrameVarLast( byval frameIndex as long ) as long
    if (frameIndex < 0) orelse (frameIndex >= gDbgStop.frameCount) then return -1
    dim as long pi = gDbgStop.frames(frameIndex).procIndex
    if (pi < 0) orelse (pi >= gDbgProcCount) then return -1
    return gDbgProc(pi).varLast
end function

function DebugEval_IsGlobal( byval varIndex as long ) as boolean
    if (varIndex < 0) orelse (varIndex >= gDbgVarCount) then return false
    select case gDbgVar(varIndex).scope
        case DBG_SCOPE_SHARED, DBG_SCOPE_STATIC, DBG_SCOPE_COMMON
            return true
    end select
    return false
end function

' ==========================================================================================
' DebugEval_VarNode -- resolve a variable to an address in a specific frame.
' ==========================================================================================
function DebugEval_VarNode( byval varIndex as long, byval frameIndex as long, byref node as DBG_NODE ) as boolean
    node = type<DBG_NODE>()
    if (varIndex < 0) orelse (varIndex >= gDbgVarCount) then return false

    with gDbgVar(varIndex)
        node.nm         = .nm
        node.typeId     = .typeId
        node.descTypeId = .descTypeId
        node.ptrDepth   = .ptrDepth
        node.arrayIndex = .arrayIndex
        node.scope      = .scope
        node.typeName   = DebugInfo_TypeName( .typeId, .ptrDepth )

        select case .scope
            case DBG_SCOPE_SHARED, DBG_SCOPE_STATIC, DBG_SCOPE_COMMON
                node.addr = cast(ulongint, .addr)

            case else
                ' Frame relative. Without a frame there is nothing to be relative to, which
                ' is the honest answer for a local when no stack has been built.
                if (frameIndex < 0) orelse (frameIndex >= gDbgStop.frameCount) then return false
                node.addr = cast(ulongint, cast(longint, gDbgStop.frames(frameIndex).frameBase) + .addr)

                ' A byref parameter's slot holds a POINTER to the caller's variable. Missing
                ' this renders the pointer as though it were the value.
                if .scope = DBG_SCOPE_PARAMREF then
                    dim as ulongint realAddr
                    if DebugProc_ReadPtr( node.addr, realAddr ) = false then return false
                    node.addr = realAddr
                end if
        end select
    end with

    node.isValid = (node.addr <> 0)
    node.hasChildren = (DebugEval_ChildCount( node ) > 0)
    node.valueText = DebugEval_FormatValue( node )
    return true
end function

' ==========================================================================================
' Children
' ==========================================================================================
function DebugEval_ChildCount( byref node as DBG_NODE ) as long
    if node.isValid = false then return 0

    ' A pointer expands to what it points at, one level.
    if (node.ptrDepth > 0) andalso (node.ptrDepth < DBG_PTR_SUB_BASE) then return 1

    ' Arrays -- fixed and dynamic alike, every dimension, flattened row-major. There is NO
    ' display cap here any more: the count is the truth about the variable, and a host that
    ' cannot show a million rows is the one that should be deciding how to group them. The old
    ' 1000 cap made element 1001 unreachable through the whole API, not merely unshown.
    if (node.arrayIndex = DBG_ARR_DYNAMIC) orelse (node.arrayIndex >= 0) then
        dim as DBG_ARRAYBOUNDS ab
        if DebugEval_ArrayBounds( node, ab ) = false then return 0
        if ab.total <= 0 then return 0
        ' The count crosses the API as a long; a larger array is reported saturated rather
        ' than wrapped negative, which would read as "no children".
        if ab.total > 2147483647ll then return 2147483647
        return cast(long, ab.total)
    end if

    if IsStringDescriptor( node.typeId ) then return 0
    if (node.typeId < 0) orelse (node.typeId >= gDbgTypeCount) then return 0
    if gDbgType(node.typeId).isEnum then return 0
    if gDbgType(node.typeId).fieldFirst < 0 then return 0
    return gDbgType(node.typeId).fieldLast - gDbgType(node.typeId).fieldFirst + 1
end function

function DebugEval_ChildNode( byref parent as DBG_NODE, byval childIndex as long, byref node as DBG_NODE ) as boolean
    node = type<DBG_NODE>()
    if parent.isValid = false then return false

    ' ---- pointer target ----
    if (parent.ptrDepth > 0) andalso (parent.ptrDepth < DBG_PTR_SUB_BASE) then
        dim as ulongint target
        if DebugProc_ReadPtr( parent.addr, target ) = false then return false
        if target = 0 then return false
        node.nm         = "*" & parent.nm
        node.typeId     = parent.typeId
        node.ptrDepth   = parent.ptrDepth - 1
        node.arrayIndex = DBG_ARR_NONE
        node.addr       = target
        node.typeName   = DebugInfo_TypeName( node.typeId, node.ptrDepth )
        node.isValid    = true
        node.hasChildren = (DebugEval_ChildCount( node ) > 0)
        node.valueText  = DebugEval_FormatValue( node )
        return true
    end if

    ' ---- array element, fixed or dynamic ----
    '
    ' One path for both, which is what lets a multidimensional REDIM array expand at all --
    ' the dynamic arm used to refuse anything but 1-D. The two kinds differ in exactly one
    ' respect, where element zero lives: a fixed array's data IS the variable, a dynamic one's
    ' is behind the descriptor's DATA pointer. Everything after that is identical.
    if (parent.arrayIndex = DBG_ARR_DYNAMIC) orelse (parent.arrayIndex >= 0) then
        dim as DBG_ARRAYBOUNDS ab
        if DebugEval_ArrayBounds( parent, ab ) = false then return false
        if (childIndex < 0) orelse (childIndex >= ab.total) then return false
        dim as longint esz = ab.elemSize
        if esz <= 0 then return false

        dim as ulongint elem0 = parent.addr
        if ab.isDynamic then
            dim as longint ofsData
            if FieldOffsetByName( parent.descTypeId, "DATA", ofsData ) = false then return false
            if DebugProc_ReadPtr( parent.addr + ofsData, elem0 ) = false then return false
            if elem0 = 0 then return false
        end if

        ' Row-major: the last dimension varies fastest, which is also the memory order, so the
        ' linear child index doubles as the element offset.
        dim as string subs
        dim as longint rem_ = childIndex
        dim as longint idx(0 to DBG_MAXDIM - 1)
        for d as long = ab.dims - 1 to 0 step -1
            dim as longint extent = ab.hi(d) - ab.lo(d) + 1
            if extent <= 0 then extent = 1
            idx(d) = ab.lo(d) + (rem_ mod extent)
            rem_ \= extent
        next
        for d as long = 0 to ab.dims - 1
            if d then subs &= ", "
            subs &= str(idx(d))
        next

        node.nm         = parent.nm & "(" & subs & ")"
        node.typeId     = parent.typeId
        node.ptrDepth   = 0
        node.arrayIndex = DBG_ARR_NONE
        node.addr       = elem0 + childIndex * esz
        node.typeName   = DebugInfo_TypeName( node.typeId, 0 )
        node.isValid    = true
        node.hasChildren = (DebugEval_ChildCount( node ) > 0)
        node.valueText  = DebugEval_FormatValue( node )
        return true
    end if

    ' ---- UDT field ----
    if (parent.typeId < 0) orelse (parent.typeId >= gDbgTypeCount) then return false
    dim as long fi = gDbgType(parent.typeId).fieldFirst
    if fi < 0 then return false
    dim as long k = fi + childIndex
    if k > gDbgType(parent.typeId).fieldLast then return false

    with gDbgField(k)
        node.nm         = .nm
        node.typeId     = .typeId
        node.descTypeId = .descTypeId
        node.ptrDepth   = .ptrDepth
        node.arrayIndex = .arrayIndex
        node.declBytes  = .declBytes
        node.bitOffset  = .bitOffset
        node.bitLen     = .bitLen
        node.addr       = parent.addr + .byteOffset
        node.typeName   = DebugInfo_TypeName( .typeId, .ptrDepth )
    end with
    node.isValid     = true
    node.hasChildren = (DebugEval_ChildCount( node ) > 0)
    node.valueText   = DebugEval_FormatValue( node )
    return true
end function

' ==========================================================================================
' DebugEval_Lookup -- name resolution for watches and hover datatips.
'
' Accepts a dotted path. The leading name is searched in the frame's own variables first and
' then among the module-level ones, which is the same order the language resolves them in.
' ==========================================================================================
function DebugEval_Lookup( byref expr as string, byval frameIndex as long, byref node as DBG_NODE ) as boolean
    node = type<DBG_NODE>()
    dim as string e = ucase( trim(expr) )
    if len(e) = 0 then return false

    dim as long dotPos = instr( e, "." )
    dim as string headNm = iif( dotPos > 0, left(e, dotPos - 1), e )
    dim as string restNm = iif( dotPos > 0, mid(e, dotPos + 1), "" )

    ' Locals and parameters of the selected frame.
    dim as long found = -1
    dim as long vf = DebugEval_FrameVarFirst( frameIndex )
    dim as long vl = DebugEval_FrameVarLast( frameIndex )
    if (vf >= 0) andalso (vl >= vf) then
        for i as long = vf to vl
            if gDbgVar(i).nm = headNm then found = i : exit for
        next
    end if

    ' Then module-level variables.
    if found < 0 then
        for i as long = 0 to gDbgVarCount - 1
            if gDbgVar(i).nm <> headNm then continue for
            if DebugEval_IsGlobal( i ) then found = i : exit for
        next
    end if

    if found < 0 then return false
    if DebugEval_VarNode( found, frameIndex, node ) = false then return false

    ' Walk the remaining dotted components.
    do while len(restNm)
        dotPos = instr( restNm, "." )
        dim as string part = iif( dotPos > 0, left(restNm, dotPos - 1), restNm )
        restNm = iif( dotPos > 0, mid(restNm, dotPos + 1), "" )

        ' A pointer is stepped through transparently, so "p.field" works as well as
        ' "(*p).field" -- which is what a FreeBASIC programmer writes anyway.
        if (node.ptrDepth > 0) andalso (node.ptrDepth < DBG_PTR_SUB_BASE) then
            dim as DBG_NODE deref
            if DebugEval_ChildNode( node, 0, deref ) = false then return false
            node = deref
        end if

        if (node.typeId < 0) orelse (node.typeId >= gDbgTypeCount) then return false
        dim as long fi = gDbgType(node.typeId).fieldFirst
        if fi < 0 then return false
        dim as long hit = -1
        for k as long = fi to gDbgType(node.typeId).fieldLast
            if gDbgField(k).nm = part then hit = k - fi : exit for
        next
        if hit < 0 then return false

        dim as DBG_NODE child
        if DebugEval_ChildNode( node, hit, child ) = false then return false
        node = child
    loop

    return true
end function
