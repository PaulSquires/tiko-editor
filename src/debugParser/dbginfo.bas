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

' See dbginfo.bi for the format description, the seven traps, and the three things
' FBdebugger does that this module deliberately does not.

#include once "dbginfo.bi"

dim shared gDbg        as DBG_INFO
dim shared gDbgSrc()   as DBG_SOURCE
dim shared gDbgLine()  as DBG_LINE
dim shared gDbgProc()  as DBG_PROC
dim shared gDbgVar()   as DBG_VAR
dim shared gDbgType()  as DBG_TYPE
dim shared gDbgField() as DBG_FIELD
dim shared gDbgArray() as DBG_ARRAYDEF

dim shared gDbgSrcCount   as long
dim shared gDbgLineCount  as long
dim shared gDbgProcCount  as long
dim shared gDbgVarCount   as long
dim shared gDbgTypeCount  as long
dim shared gDbgFieldCount as long
dim shared gDbgArrayCount as long

' Parse state, valid only for the duration of DebugInfo_Load.
namespace DbgParse
    dim shared as ubyte ptr img          ' whole file image
    dim shared as ulongint imgLen
    dim shared as long     typeBias      ' see trap 2
    dim shared as long     typeMax
    dim shared as long     curSrc        ' index into gDbgSrc
    dim shared as long     curProc       ' index into gDbgProc, -1 outside a procedure
    dim shared as long     unitSrc       ' the compilation unit's own primary source
    dim shared as boolean  inBuiltIns    ' skipping the built-in typedef preamble
    dim shared as string   pendingDir    ' directory from a code-100 awaiting its file name
end namespace

' ==========================================================================================
' Growth. Doubling rather than a hard ceiling -- FBdebugger's fixed LINEMAX/PROCMAX/VARMAX
' turn a large target into a "report to dev please" box, and tiko.exe is a large target.
' ==========================================================================================
#macro DBG_GROW( arr, count )
    if (count) > ubound(arr) then
        dim as long newUB = iif( ubound(arr) < 63, 127, (ubound(arr) + 1) * 2 - 1 )
        redim preserve arr( newUB )
    end if
#endmacro

private function ReadU16( byval ofs as ulongint ) as ulongint
    if ofs + 2 > DbgParse.imgLen then return 0
    return cast(ushort ptr, DbgParse.img + ofs)[0]
end function

private function ReadU32( byval ofs as ulongint ) as ulongint
    if ofs + 4 > DbgParse.imgLen then return 0
    return cast(ulong ptr, DbgParse.img + ofs)[0]
end function

private function ReadU64( byval ofs as ulongint ) as ulongint
    if ofs + 8 > DbgParse.imgLen then return 0
    return cast(ulongint ptr, DbgParse.img + ofs)[0]
end function

' Read the NUL terminated string at tblBase+strOfs out of the mapped image.
private function ReadStabString( byval tblBase as ulongint, byval strOfs as ulongint ) as string
    dim as ulongint p = tblBase + strOfs
    if p >= DbgParse.imgLen then return ""
    dim as ulongint q = p
    do while (q < DbgParse.imgLen) andalso (DbgParse.img[q] <> 0)
        q += 1
    loop
    if q = p then return ""
    dim as string s = space( cast(long, q - p) )
    memcpy( strptr(s), DbgParse.img + p, cast(long, q - p) )
    return s
end function

' ==========================================================================================
' Small string helpers. All operate on 1-based positions to match FB's own mid/instr.
' ==========================================================================================
private function IsDigitAt( byref s as string, byval p as long ) as boolean
    if (p < 1) orelse (p > len(s)) then return false
    dim as ubyte c = s[p - 1]
    return (c >= asc("0")) andalso (c <= asc("9"))
end function

private function CharAt( byref s as string, byval p as long ) as long
    if (p < 1) orelse (p > len(s)) then return 0
    return s[p - 1]
end function

' Reads an optionally signed integer starting at p, advancing p past it.
private function ScanInt( byref s as string, byref p as long ) as longint
    dim as long start = p
    if CharAt(s, p) = asc("-") then p += 1
    do while IsDigitAt(s, p)
        p += 1
    loop
    if p = start then return 0
    return valint( mid(s, start, p - start) )
end function

' ==========================================================================================
' Type table
' ==========================================================================================
private function EnsureType( byval id as long ) as long
    if id < 0 then return 0
    if id > gDbgTypeCount - 1 then
        DBG_GROW( gDbgType, id )
        for i as long = gDbgTypeCount to id
            gDbgType(i).nm         = ""
            gDbgType(i).sizeBytes  = 0
            gDbgType(i).isEnum     = false
            gDbgType(i).isBuiltIn  = false
            gDbgType(i).baseCode   = 0
            gDbgType(i).fieldFirst = -1
            gDbgType(i).fieldLast  = -1
            gDbgType(i).isAlias    = false
            gDbgType(i).aliasType  = 0
            gDbgType(i).aliasPtr   = 0
            gDbgType(i).aliasArray = DBG_ARR_NONE
        next
        gDbgTypeCount = id + 1
    end if
    if id > DbgParse.typeMax then DbgParse.typeMax = id
    return id
end function

' Apply the per compilation unit bias. Built-in ids are shared across units and are not
' biased; everything above them belongs to the unit that declared it. See trap 2.
private function BiasType( byval rawId as long ) as long
    if rawId > DBG_TYPESTD then return rawId + DbgParse.typeBias
    return rawId
end function

' ==========================================================================================
' "ar1;lo;hi;" repeated, then the element type spec. Returns the gDbgArray index.
' ==========================================================================================
private function ParseArraySpec( byref s as string, byref p as long ) as long
    dim as DBG_ARRAYDEF ad
    ad.dims = 0
    do while (p + 2 <= len(s)) andalso (mid(s, p, 3) = "ar1")
        p += 3
        if CharAt(s, p) = asc(";") then p += 1
        dim as longint lo = ScanInt(s, p)
        if CharAt(s, p) = asc(";") then p += 1
        dim as longint hi = ScanInt(s, p)
        if CharAt(s, p) = asc(";") then p += 1
        if ad.dims < DBG_MAXDIM then
            ad.lo(ad.dims) = lo
            ad.hi(ad.dims) = hi
            ad.dims += 1
        end if
    loop
    if ad.dims = 0 then return DBG_ARR_NONE
    DBG_GROW( gDbgArray, gDbgArrayCount )
    gDbgArray(gDbgArrayCount) = ad
    gDbgArrayCount += 1
    return gDbgArrayCount - 1
end function

' ==========================================================================================
' The type-reference grammar, as it appears after a variable's scope letter or inside a UDT
' field. Handles:  N  |  N=*inner  |  N=ar1;lo;hi;inner  |  N=-code  |  N=fRET  |  *inner
' Pointer depth and array shape are flattened onto the caller (the variable or field); the
' defining id is also recorded as an alias so a later bare reference to it still resolves.
' ==========================================================================================
private sub ParseTypeRef( byref s as string, byref p as long, _
                          byref typeId as long, byref ptrDepth as long, byref arrayIndex as long )
    typeId     = 0
    ptrDepth   = 0
    arrayIndex = DBG_ARR_NONE

    dim as long definedId = -1

    ' An "N=" prefix introduces a new id for whatever follows.
    if IsDigitAt(s, p) then
        dim as long save = p
        dim as long n = cast(long, ScanInt(s, p))
        if CharAt(s, p) = asc("=") then
            definedId = BiasType(n)
            p += 1
        else
            ' A bare reference. Expand it if it names an alias we recorded earlier.
            p = save
            dim as long refId = BiasType( cast(long, ScanInt(s, p)) )
            dim as boolean isAliasRef = false
            if (refId >= 0) andalso (refId < gDbgTypeCount) then isAliasRef = gDbgType(refId).isAlias
            if isAliasRef then
                typeId     = gDbgType(refId).aliasType
                ptrDepth   = gDbgType(refId).aliasPtr
                arrayIndex = gDbgType(refId).aliasArray
            else
                typeId = refId
            end if
            exit sub
        end if
    end if

    ' Arrays bind outside pointers in this grammar: "29=ar1;1;3;ar1;0;2;1".
    if (p + 2 <= len(s)) andalso (mid(s, p, 3) = "ar1") then
        arrayIndex = ParseArraySpec( s, p )
    end if

    ' Pointers, possibly several.
    do while CharAt(s, p) = asc("*")
        ptrDepth += 1
        p += 1
    loop

    ' A procedure type. "f7" is a SUB (returns void); anything else is a FUNCTION. Only
    ' meaningful when we already counted at least one pointer.
    if (CharAt(s, p) = asc("f")) orelse (CharAt(s, p) = asc("F")) then
        dim as boolean isSub = (mid(s, p, 2) = "f7") orelse (mid(s, p, 2) = "F7")
        p += 1
        dim as long inner = cast(long, ScanInt(s, p))
        typeId = BiasType(inner)
        if ptrDepth > 0 then
            ptrDepth = iif( isSub, DBG_PTR_SUB_BASE, DBG_PTR_FUNC_BASE ) + ptrDepth
        end if
    elseif CharAt(s, p) = asc("-") then
        ' A built-in, given as a negative stabs code. Register it under the defining id.
        dim as long code = cast(long, ScanInt(s, p))
        if definedId >= 0 then
            EnsureType( definedId )
            gDbgType(definedId).isBuiltIn = true
            gDbgType(definedId).baseCode  = code
        end if
        typeId = iif( definedId >= 0, definedId, 0 )
        exit sub
    elseif IsDigitAt(s, p) then
        dim as long inner = cast(long, ScanInt(s, p))
        ' A nested "N=..." (e.g. "23=*1" inside a field list) recurses.
        if CharAt(s, p) = asc("=") then
            dim as long p2 = p - len(str(inner))
            dim as long t2, d2, a2
            ParseTypeRef( s, p2, t2, d2, a2 )
            p = p2
            typeId = t2
            ptrDepth += d2
            if a2 <> DBG_ARR_NONE then arrayIndex = a2
        else
            dim as long refId = BiasType(inner)
            dim as boolean isAliasRef2 = false
            if (refId >= 0) andalso (refId < gDbgTypeCount) then isAliasRef2 = gDbgType(refId).isAlias
            if isAliasRef2 then
                typeId    = gDbgType(refId).aliasType
                ptrDepth += gDbgType(refId).aliasPtr
                if gDbgType(refId).aliasArray <> DBG_ARR_NONE then arrayIndex = gDbgType(refId).aliasArray
            else
                typeId = refId
            end if
        end if
    end if

    ' Record the alias so a later bare ":N" resolves to the same shape.
    if (definedId >= 0) andalso ((ptrDepth <> 0) orelse (arrayIndex <> DBG_ARR_NONE)) then
        EnsureType( definedId )
        gDbgType(definedId).isAlias    = true
        gDbgType(definedId).aliasType  = typeId
        gDbgType(definedId).aliasPtr   = ptrDepth
        gDbgType(definedId).aliasArray = arrayIndex
    end if
end sub

' ==========================================================================================
' UDT:  NAME:Tt19=s8IX:1,0,32;IY:1,32,32;;
' Enum: NAME:T27=eCK_RED:5,CK_GREEN:8,CK_BLUE:99,;
' The ":Tt" / ":T" distinction is real -- fbc emits "Tt" for a structure and "T" for an enum.
' ==========================================================================================
private sub ParseAggregate( byref s as string, byval colonPos as long )
    dim as string nm = left(s, colonPos - 1)
    dim as long p = colonPos + 1

    if CharAt(s, p) <> asc("T") then exit sub
    p += 1
    if CharAt(s, p) = asc("t") then p += 1

    dim as long id = BiasType( cast(long, ScanInt(s, p)) )
    if CharAt(s, p) <> asc("=") then exit sub
    p += 1

    EnsureType( id )
    gDbgType(id).nm         = nm
    gDbgType(id).fieldFirst = -1
    gDbgType(id).fieldLast  = -1

    if CharAt(s, p) = asc("e") then
        ' ---- enum ----
        p += 1
        gDbgType(id).isEnum    = true
        gDbgType(id).sizeBytes = 4
        dim as long first = gDbgFieldCount
        do while p <= len(s)
            if CharAt(s, p) = asc(";") then exit do
            dim as long c = instr(p, s, ":")
            if c = 0 then exit do
            dim as string mnm = mid(s, p, c - p)
            p = c + 1
            dim as longint v = ScanInt(s, p)
            DBG_GROW( gDbgField, gDbgFieldCount )
            with gDbgField(gDbgFieldCount)
                .nm         = mnm
                .typeId     = 0
                .descTypeId = 0
                .ptrDepth   = 0
                .arrayIndex = DBG_ARR_NONE
                .byteOffset = 0
                .bitOffset  = 0
                .bitLen     = 0
                .declBytes  = 0
                .enumValue  = v
            end with
            gDbgFieldCount += 1
            if CharAt(s, p) = asc(",") then p += 1
        loop
        if gDbgFieldCount > first then
            gDbgType(id).fieldFirst = first
            gDbgType(id).fieldLast  = gDbgFieldCount - 1
        end if
        exit sub
    end if

    if CharAt(s, p) <> asc("s") then exit sub
    ' ---- structure ----
    p += 1
    gDbgType(id).sizeBytes = ScanInt(s, p)

    dim as long first = gDbgFieldCount
    do while p <= len(s)
        if CharAt(s, p) = asc(";") then exit do
        dim as long c = instr(p, s, ":")
        if c = 0 then exit do
        dim as string fnm = mid(s, p, c - p)
        p = c + 1

        dim as long ft, fp, fa
        ParseTypeRef( s, p, ft, fp, fa )

        if CharAt(s, p) <> asc(",") then exit do
        p += 1
        dim as longint bitOfs = ScanInt(s, p)
        if CharAt(s, p) = asc(",") then p += 1
        dim as longint bitLen = ScanInt(s, p)
        if CharAt(s, p) = asc(";") then p += 1

        DBG_GROW( gDbgField, gDbgFieldCount )
        with gDbgField(gDbgFieldCount)
            .nm         = fnm
            .typeId     = ft
            .descTypeId = 0
            .ptrDepth   = fp
            .arrayIndex = fa
            .byteOffset = bitOfs \ 8
            .bitOffset  = cast(long, bitOfs - (bitOfs \ 8) * 8)
            .bitLen     = 0
            .declBytes  = bitLen \ 8
            .enumValue  = 0
            ' A bitfield is NARROWER than its type. Testing merely that the declared width
            ' DIFFERS from the natural one -- which is what the reference implementation
            ' does -- produces false positives in both directions:
            '   len:1,64,64   the string descriptor declares a 64-bit field of the 32-bit
            '                 type t1. fbc gets that declaration wrong, the same way it
            '                 declares the descriptor itself as s12 (trap 6).
            '   FIXED:4,96,128  a char field 128 bits wide is a zstring * 16.
            ' Both are wider, so "narrower" excludes them without special-casing either.
            if (fp = 0) andalso (fa = DBG_ARR_NONE) andalso (ft > 0) andalso (ft < gDbgTypeCount) then
                dim as longint natural = gDbgType(ft).sizeBytes * 8
                if (natural > 0) andalso (bitLen > 0) andalso (bitLen < natural) then
                    .bitLen = cast(long, bitLen)
                end if
            end if
        end with
        gDbgFieldCount += 1
    loop

    if gDbgFieldCount > first then
        gDbgType(id).fieldFirst = first
        gDbgType(id).fieldLast  = gDbgFieldCount - 1
    end if
end sub

' ==========================================================================================
' Is this a symbol fbc generated for its own use rather than something the user wrote?
' Every name here was observed in the probe output.
' ==========================================================================================
private function IsInternalSymbol( byref nm as string ) as boolean
    if len(nm) = 0 then return true
    if left(nm, 10) = "fb$result$" then return true
    if left(nm, 9)  = "fb$result" then return true
    if left(nm, 3)  = "LT_" then return true
    if left(nm, 3)  = "vr$" then return true
    if left(nm, 4)  = "TMP$" then return true
    if left(nm, 4)  = "tmp$" then return true
    if left(nm, 4)  = "_ZTS" then return true
    if left(nm, 4)  = "_ZTV" then return true
    if left(nm, 3)  = ".Lt" then return true
    if left(nm, 3)  = "Lt_" then return true
    if nm = "$fb_RTTI" then return true
    if nm = "FDBG_COMPIL_INFO" then return true
    if left(nm, 16) = "__FB_ARRAYDIMTB$" then return true
    return false
end function

' ==========================================================================================
' Variable records: codes 32 / 38 / 40 (static, shared, common) and 128 / 160 (local, param).
'   NAME:S1      shared        NAME:V1   static      NAME:p1  byval param
'   NAME:v18     byref param   NAME:1    local       NAME:G1  common
' ==========================================================================================
private sub ParseVariable( byref s as string, byval addr as longint )
    dim as long colonPos = instr(s, ":")
    if colonPos < 1 then exit sub

    dim as string nm = left(s, colonPos - 1)

    ' The built-in typedef preamble that opens every compilation unit. It ends with
    ' va_list, after which real user symbols start.
    dim as long afterColon = colonPos + 1
    if CharAt(s, afterColon) = asc("t") then
        ' "integer:t1=-1" and friends -- a built-in type declaration, not a variable.
        dim as long p = afterColon + 1
        dim as long id = BiasType( cast(long, ScanInt(s, p)) )
        if CharAt(s, p) = asc("=") then
            p += 1
            EnsureType( id )
            if len(gDbgType(id).nm) = 0 then gDbgType(id).nm = nm
            dim as long t2, d2, a2
            dim as long p2 = p
            ' Handle "string:t13=s12data:..." -- a named struct in the preamble.
            if CharAt(s, p) = asc("s") andalso IsDigitAt(s, p + 1) then
                ParseAggregate( "?:Tt" & str(id) & "=" & mid(s, p), 2 )
                gDbgType(id).nm = nm
            elseif CharAt(s, p) = asc("@") then
                ' "boolean:t16=@s8;-16" -- a size attribute then the base code.
                dim as long semi = instr(p, s, ";")
                if semi > 0 then p2 = semi + 1
                dim as long p3 = p2
                gDbgType(id).isBuiltIn = true
                gDbgType(id).baseCode  = cast(long, ScanInt(s, p3))
                gDbgType(id).sizeBytes = 1
            elseif CharAt(s, p) = asc("-") then
                ' "integer:t1=-1" -- a built-in given directly as its negative stabs code.
                dim as long p4 = p
                gDbgType(id).isBuiltIn = true
                gDbgType(id).baseCode  = cast(long, ScanInt(s, p4))
                gDbgType(id).sizeBytes = DebugInfo_BaseSize( gDbgType(id).baseCode )
            else
                ParseTypeRef( s, p2, t2, d2, a2 )
                gDbgType(id).isBuiltIn = true
                if (t2 > 0) andalso (t2 < gDbgTypeCount) andalso (gDbgType(t2).baseCode <> 0) then
                    gDbgType(id).baseCode  = gDbgType(t2).baseCode
                    gDbgType(id).sizeBytes = gDbgType(t2).sizeBytes
                end if
            end if
        end if
        if nm = "va_list" then DbgParse.inBuiltIns = false
        exit sub
    end if

    if IsInternalSymbol( nm ) then exit sub

    ' Aggregate declarations arrive through the same codes as variables.
    if CharAt(s, afterColon) = asc("T") then
        ParseAggregate( s, colonPos )
        exit sub
    end if

    dim as long p = afterColon
    dim as DBG_SCOPE sc
    select case CharAt(s, p)
        case asc("S") : sc = DBG_SCOPE_SHARED   : p += 1
        case asc("G") : sc = DBG_SCOPE_COMMON   : p += 1
        case asc("V") : sc = DBG_SCOPE_STATIC   : p += 1
        case asc("v") : sc = DBG_SCOPE_PARAMREF : p += 1
        case asc("p") : sc = DBG_SCOPE_PARAMVAL : p += 1
        case asc("F"), asc("f") : exit sub          ' a procedure, handled by ParseProcedure
        case else     : sc = DBG_SCOPE_LOCAL
    end select

    dim as long ty, pd, ai
    ParseTypeRef( s, p, ty, pd, ai )

    ' A dynamic array is a variable whose type is one of fbc's generated FBARRAY descriptor
    ' structures. Collapse it to the element type and mark it dynamic; the real bounds live
    ' in the descriptor at run time. Resolved BY FIELD NAME -- see the .bi header.
    dim as long descTy = 0
    if (ty > 0) andalso (ty < gDbgTypeCount) andalso (ai = DBG_ARR_NONE) then
        if left( gDbgType(ty).nm, 7 ) = "FBARRAY" then
            dim as long fi = gDbgType(ty).fieldFirst
            if fi >= 0 then
                descTy = ty
                for k as long = fi to gDbgType(ty).fieldLast
                    if gDbgField(k).nm = "DATA" then
                        ' DATA is "pointer to element", so one level of indirection is the
                        ' descriptor's own and does not belong to the variable.
                        ty = gDbgField(k).typeId
                        pd = gDbgField(k).ptrDepth - 1
                        if pd < 0 then pd = 0
                        exit for
                    end if
                next
                ai = DBG_ARR_DYNAMIC
            end if
        end if
    end if

    DBG_GROW( gDbgVar, gDbgVarCount )
    with gDbgVar(gDbgVarCount)
        .nm         = nm
        .typeId     = ty
        .descTypeId = descTy
        .ptrDepth   = pd
        .arrayIndex = ai
        .scope      = sc
        .addr       = addr
    end with
    gDbgVarCount += 1

    if DbgParse.curProc >= 0 then
        if gDbgProc(DbgParse.curProc).varFirst < 0 then
            gDbgProc(DbgParse.curProc).varFirst = gDbgVarCount - 1
        end if
        gDbgProc(DbgParse.curProc).varLast = gDbgVarCount - 1
    end if
end sub

' ==========================================================================================
' Procedure records (code 36).  "ADDTHEM:F1"  "TOUCHRECORD:F7"  "{MODLEVEL}:f7"
' A code-36 record whose line number is 0 is the procedure END marker, not a new procedure.
' ==========================================================================================
private sub ParseProcedure( byref s as string, byval lineNum as long, byval addr as longint )
    if len(s) = 0 then
        ' End marker: close the open procedure. This address is procedure-relative like
        ' the line and epilog addresses -- storing it raw yields an "end" of &h45.
        if DbgParse.curProc >= 0 then
            gDbgProc(DbgParse.curProc).addrEnd = gDbgProc(DbgParse.curProc).addrStart + addr
        end if
        DbgParse.curProc = -1
        exit sub
    end if

    dim as long colonPos = instr(s, ":")
    dim as string rawName = iif( colonPos > 0, left(s, colonPos - 1), s )

    dim as long retType = 0, retPtr = 0
    dim as boolean isSub = false
    if colonPos > 0 then
        dim as long p = colonPos + 1
        dim as long k = CharAt(s, p)
        if (k = asc("F")) orelse (k = asc("f")) then p += 1
        dim as long t2, d2, a2
        ParseTypeRef( s, p, t2, d2, a2 )
        retType = t2
        retPtr  = d2
        ' Return type void == a SUB.
        if (t2 > 0) andalso (t2 < gDbgTypeCount) andalso (gDbgType(t2).nm = "void") then isSub = true
    end if

    DBG_GROW( gDbgProc, gDbgProcCount )
    with gDbgProc(gDbgProcCount)
        .rawName    = rawName
        .nm         = DebugInfo_Demangle( rawName )
        .addrStart  = addr
        .addrFirst  = 0
        .addrEpilog = 0
        .addrEnd    = 0
        .srcIndex   = DbgParse.curSrc
        .lineNum    = lineNum
        .varFirst   = -1
        .varLast    = -1
        .retType    = retType
        .retPtr     = retPtr
        .isSub      = isSub
        .isModLevel = (rawName = "{MODLEVEL}")
    end with
    gDbgProcCount += 1
    DbgParse.curProc = gDbgProcCount - 1
end sub

' ==========================================================================================
' Sources
' ==========================================================================================
private function AddSource( byref fullName as string ) as long
    dim as string shortNm = ucase( fullName )
    dim as long bs = instrrev( shortNm, "\" )
    if bs = 0 then bs = instrrev( shortNm, "/" )
    dim as boolean hasPath = (bs > 0)
    if hasPath then shortNm = mid( shortNm, bs + 1 )

    ' CASE-INSENSITIVE, and that is load-bearing rather than tidy. fbc's code-132
    ' re-announcement of the primary source is UPPER CASE, so when the source reached fbc as
    ' an ABSOLUTE path -- which is what tiko passes -- the re-announcement is a full upper-case
    ' path that a case-sensitive compare does not match. The file then lands in the table
    ' twice: the code-100 entry owning NO line records, and the upper-case entry owning them
    ' all. DebugInfo_SourceFromName returns the first, so every breakpoint resolved through it
    ' reports "no executable line" and is silently dropped. Measured 2026-08-02; it is why
    ' tiko honoured no breakpoints at all. A relative source name hides it, because then the
    ' re-announcement has no path and the shortName fallback below catches it instead.
    for i as long = 0 to gDbgSrcCount - 1
        if ucase( gDbgSrc(i).fullName ) = ucase( fullName ) then return i
    next

    ' fbc re-announces the primary source as a bare upper-case include (code 132) after the
    ' code-100 pair that gave its full path. Without this the same file lands in the table
    ' twice and half the line records point at the copy with no path on it.
    if hasPath = false then
        for i as long = 0 to gDbgSrcCount - 1
            if gDbgSrc(i).shortName = shortNm then return i
        next
    end if

    DBG_GROW( gDbgSrc, gDbgSrcCount )
    gDbgSrc(gDbgSrcCount).fullName  = fullName
    gDbgSrc(gDbgSrcCount).shortName = shortNm
    gDbgSrcCount += 1
    return gDbgSrcCount - 1
end function

' ==========================================================================================
' DebugInfo_Load
' ==========================================================================================
function DebugInfo_Load( byref imgPath as string, byval imgLoadBase as ulongint ) as boolean
    DebugInfo_Free()
    gDbg.exePath   = imgPath
    gDbg.lastError = ""

    ' ---- read the image ----
    dim as long f = freefile
    if open( imgPath for binary access read as #f ) <> 0 then
        gDbg.lastError = "cannot open " & imgPath
        return false
    end if
    DbgParse.imgLen = lof(f)
    if DbgParse.imgLen < 64 then
        close #f
        gDbg.lastError = "file too small to be a PE image"
        return false
    end if
    DbgParse.img = allocate( DbgParse.imgLen )
    if DbgParse.img = 0 then
        close #f
        gDbg.lastError = "out of memory reading image"
        return false
    end if
    dim as longint got
    get #f, , *DbgParse.img, DbgParse.imgLen, got
    close #f

    ' ---- PE headers ----
    if ReadU16(0) <> &h5A4D then
        gDbg.lastError = "not an MZ image" : DebugInfo_Free() : return false
    end if
    dim as ulongint eLfanew = ReadU32( &h3C )
    if ReadU32( eLfanew ) <> &h00004550 then
        gDbg.lastError = "not a PE image" : DebugInfo_Free() : return false
    end if

    dim as ulongint coff       = eLfanew + 4
    dim as ulongint numSec     = ReadU16( coff + 2 )
    dim as ulongint sizeOptHdr = ReadU16( coff + 16 )
    dim as ulongint opt        = coff + 20
    dim as boolean  is64       = (ReadU16( opt ) = &h20B)

    gDbg.target64  = is64
    gDbg.imageBase = iif( is64, ReadU64( opt + 24 ), ReadU32( opt + 28 ) )
    gDbg.loadBase  = iif( imgLoadBase = 0, gDbg.imageBase, imgLoadBase )

    ' ---- locate the debug sections ----
    dim as string wantDat = iif( is64, ".dbgdat", ".stab" )
    dim as string wantStr = iif( is64, ".dbgstr", ".stabstr" )
    dim as ulongint secTab = opt + sizeOptHdr
    dim as ulongint datPtr, datVSize, strTblPtr

    for i as ulongint = 0 to numSec - 1
        dim as ulongint e = secTab + i * 40
        dim as string nm
        for j as long = 0 to 7
            dim as ubyte c = DbgParse.img[e + j]
            if c = 0 then exit for
            nm &= chr(c)
        next
        if nm = wantDat then
            datVSize = ReadU32( e + 8 )                    ' VirtualSize -- see trap 4
            if datVSize = 0 orelse datVSize > ReadU32( e + 16 ) then datVSize = ReadU32( e + 16 )
            datPtr = ReadU32( e + 20 )
        elseif nm = wantStr then
            strTblPtr = ReadU32( e + 20 )
        end if
    next

    if (datPtr = 0) orelse (strTblPtr = 0) then
        gDbg.lastError = "no debug information (" & wantDat & " / " & wantStr & " absent). " & _
                         "Build with -g and -gen " & iif(is64, "gas64", "gas") & "."
        DebugInfo_Free()
        return false
    end if

    ' ---- walk the records ----
    dim as long recSize = iif( is64, 16, 12 )
    dim as ulongint nRec = datVSize \ recSize
    dim as ulongint strCur = strTblPtr        ' current module's string sub-table
    dim as ulongint strHigh = 0            ' high-water mark within it

    DbgParse.typeBias  = 0
    DbgParse.typeMax   = DBG_TYPESTD
    DbgParse.curSrc    = -1
    DbgParse.curProc   = -1
    DbgParse.unitSrc   = -1
    DbgParse.inBuiltIns = true
    DbgParse.pendingDir = ""
    gDbg.lineCapHit    = false

    ' Type slot 0 is a placeholder so that "no type" and "type 0" never collide.
    EnsureType( DBG_TYPESTD )

    for i as ulongint = 0 to nRec - 1
        dim as ulongint e = datPtr + i * recSize
        dim as ulongint so  = ReadU32( e + 0 )
        dim as ulongint cod = ReadU16( e + 4 )
        dim as ulongint dsc = ReadU16( e + 6 )
        dim as longint  ad
        if is64 then
            ad = cast(longint, ReadU64( e + 8 ))
        else
            ' Sign extend: locals and parameters carry negative frame offsets. See trap 3.
            ad = cast(longint, cast(long, ReadU32( e + 8 )))
        end if

        ' Record 0 is this module's header. It is also code 0, which is the module boundary
        ' marker, so it must be skipped explicitly rather than treated as a terminator.
        if i = 0 then continue for

        if cod = 0 then
            ' Module boundary. gas32 puts that module's string-table size in the record's
            ' value; gas64 leaves it 0, so fall back to how far we actually read.
            dim as ulongint adv = iif( ad > 0, cast(ulongint, ad), strHigh )
            strCur += adv
            strHigh = 0
            DbgParse.curProc = -1
            continue for
        end if

        dim as string s = ReadStabString( strCur, so )
        if so + len(s) + 1 > strHigh then strHigh = so + len(s) + 1

        select case cod
            case 100                                   ' compilation unit
                if len(s) = 0 then
                    ' End of the unit. Everything above the built-ins belonged to it.
                    DbgParse.typeBias = DbgParse.typeMax - DBG_TYPESTD
                    DbgParse.curProc  = -1
                else
                    if right(s, 1) = "\" orelse right(s, 1) = "/" then
                        ' A bare directory. It is the PREFIX for the file named in the next
                        ' record, not a source in its own right -- registering it puts a
                        ' nameless entry at index 0 that nothing can ever match.
                        DbgParse.pendingDir = s
                    else
                        dim as string full = DbgParse.pendingDir & s
                        DbgParse.pendingDir = ""
                        DbgParse.curSrc  = AddSource( full )
                        DbgParse.unitSrc = DbgParse.curSrc
                    end if
                end if

            case 132                                   ' include file
                if len(s) then DbgParse.curSrc = AddSource( s )

            case 255                                   ' fbc version
                if len(gDbg.compilerVer) = 0 then gDbg.compilerVer = s

            case 36                                    ' procedure
                if dsc = 0 then
                    ParseProcedure( "", 0, ad )
                else
                    ParseProcedure( s, cast(long, dsc), ad )
                end if

            case 224                                   ' epilog address
                if DbgParse.curProc >= 0 then
                    gDbgProc(DbgParse.curProc).addrEpilog = _
                        gDbgProc(DbgParse.curProc).addrStart + ad
                end if

            case 68                                    ' line
                if dsc > 0 then
                    if dsc = 65535 then gDbg.lineCapHit = true
                    dim as longint absAddr = ad
                    if DbgParse.curProc >= 0 then
                        ' Line addresses are procedure relative. See trap 1.
                        absAddr += gDbgProc(DbgParse.curProc).addrStart
                        if (gDbgProc(DbgParse.curProc).addrFirst = 0) andalso (ad <> 0) then
                            gDbgProc(DbgParse.curProc).addrFirst = absAddr
                        end if
                    end if
                    DBG_GROW( gDbgLine, gDbgLineCount )
                    with gDbgLine(gDbgLineCount)
                        .addr      = absAddr
                        .lineNum   = cast(long, dsc)
                        .procIndex = DbgParse.curProc
                        .srcIndex  = DbgParse.curSrc
                    end with
                    gDbgLineCount += 1
                end if

            case 32, 38, 40, 128, 160                  ' variables and type declarations
                ParseVariable( s, ad )

            case 42                                    ' main entry point -- not used
        end select
    next

    ' fbc emits a placeholder line record at every procedure's entry address carrying the
    ' line number 1, and folds the first statement's code into the prologue region. Left
    ' alone, any address that resolves to a procedure entry -- a fault in a prologue, or a
    ' call-stack frame that returns exactly to one -- reports "line 1" of the file. Relabel
    ' it with the procedure's own declared line, which is what that code actually is.
    for i as long = 0 to gDbgLineCount - 1
        dim as long pi = gDbgLine(i).procIndex
        if pi < 0 then continue for
        if gDbgLine(i).lineNum <> 1 then continue for
        if gDbgLine(i).addr <> gDbgProc(pi).addrStart then continue for
        if gDbgProc(pi).lineNum > 1 then gDbgLine(i).lineNum = gDbgProc(pi).lineNum
    next

    ' Rebase everything if the image is loaded somewhere other than its preferred base.
    if gDbg.loadBase <> gDbg.imageBase then
        dim as longint delta = cast(longint, gDbg.loadBase) - cast(longint, gDbg.imageBase)
        for i as long = 0 to gDbgLineCount - 1
            gDbgLine(i).addr += delta
        next
        for i as long = 0 to gDbgProcCount - 1
            with gDbgProc(i)
                if .addrStart  then .addrStart  += delta
                if .addrFirst  then .addrFirst  += delta
                if .addrEpilog then .addrEpilog += delta
                if .addrEnd    then .addrEnd    += delta
            end with
        next
        for i as long = 0 to gDbgVarCount - 1
            select case gDbgVar(i).scope
                case DBG_SCOPE_SHARED, DBG_SCOPE_STATIC, DBG_SCOPE_COMMON
                    gDbgVar(i).addr += delta
            end select
        next
    end if

    deallocate DbgParse.img
    DbgParse.img = 0

    gDbg.isLoaded = true
    return true
end function

sub DebugInfo_Free()
    if DbgParse.img then deallocate DbgParse.img
    DbgParse.img = 0
    erase gDbgSrc, gDbgLine, gDbgProc, gDbgVar, gDbgType, gDbgField, gDbgArray
    gDbgSrcCount   = 0
    gDbgLineCount  = 0
    gDbgProcCount  = 0
    gDbgVarCount   = 0
    gDbgTypeCount  = 0
    gDbgFieldCount = 0
    gDbgArrayCount = 0
    gDbg.isLoaded    = false
    gDbg.compilerVer = ""
    gDbg.lineCapHit  = false
end sub

' ==========================================================================================
' Name demangling.
'
' fbc mangles namespaced and member procedures with the Itanium ABI scheme. A plain
' module-level SUB or FUNCTION is emitted unmangled, so the common case costs one test.
' Only the nested-name grammar is decoded -- the argument list that follows is deliberately
' dropped, because the parameters are already in the variable table with real types and
' repeating a mangled approximation of them in the call stack helps nobody.
' ==========================================================================================
function DebugInfo_Demangle( byref rawName as string ) as string
    if left( rawName, 2 ) <> "_Z" then return rawName

    dim as long p = 3                       ' past "_Z"
    if left( rawName, 3 ) = "__Z" then p = 4

    dim as boolean nested = false
    if CharAt( rawName, p ) = asc("N") then
        nested = true
        p += 1
    else
        p -= 1                              ' no "N": the length count starts here
    end if

    dim as string parts()
    dim as long   nParts = 0
    redim parts( 15 )

    do while p <= len( rawName )
        if CharAt( rawName, p ) = asc("E") then exit do
        if IsDigitAt( rawName, p ) = false then exit do
        dim as long n = cast(long, ScanInt( rawName, p ))
        if (n <= 0) orelse (p + n - 1 > len(rawName)) then exit do
        if nParts > ubound(parts) then redim preserve parts( (nParts + 1) * 2 )
        parts( nParts ) = mid( rawName, p, n )
        nParts += 1
        p += n
        if nested = false then exit do
    loop

    if nParts = 0 then return rawName

    dim as string outNm = parts(0)
    for i as long = 1 to nParts - 1
        outNm &= "." & parts(i)
    next

    ' fbc spells property accessors this way. Done with plain mid/instr rather than an
    ' AfxNova helper on purpose -- this pair has to compile with no tiko global in scope.
    dim as long g = instr( outNm, "__get__" )
    if g > 0 then return left(outNm, g - 1) & mid(outNm, g + 7) & " (Get property)"
    dim as long st = instr( outNm, "__set__" )
    if st > 0 then return left(outNm, st - 1) & mid(outNm, st + 7) & " (Set property)"

    return outNm
end function

function DebugInfo_ScopeName( byval sc as DBG_SCOPE ) as string
    select case sc
        case DBG_SCOPE_LOCAL    : return "local"
        case DBG_SCOPE_SHARED   : return "shared"
        case DBG_SCOPE_STATIC   : return "static"
        case DBG_SCOPE_PARAMREF : return "byref param"
        case DBG_SCOPE_PARAMVAL : return "byval param"
        case DBG_SCOPE_COMMON   : return "common"
    end select
    return "?"
end function

function DebugInfo_TypeName( byval typeId as long, byval ptrDepth as long ) as string
    dim as string baseNm
    if (typeId >= 0) andalso (typeId < gDbgTypeCount) andalso len(gDbgType(typeId).nm) then
        baseNm = gDbgType(typeId).nm
    else
        baseNm = "type" & str(typeId)
    end if

    if ptrDepth >= DBG_PTR_FUNC_BASE then
        return string( ptrDepth - DBG_PTR_FUNC_BASE, "*" ) & " function"
    elseif ptrDepth >= DBG_PTR_SUB_BASE then
        return string( ptrDepth - DBG_PTR_SUB_BASE, "*" ) & " sub"
    elseif ptrDepth > 0 then
        return string( ptrDepth, "*" ) & " " & baseNm
    end if
    return baseNm
end function

' ==========================================================================================
' Lookups. The line table is built in address order because the records arrive that way,
' so address -> line is a binary search. Both directions are needed on every stop.
' ==========================================================================================
function DebugInfo_LineFromAddr( byval addr as longint ) as long
    if gDbgLineCount = 0 then return -1
    dim as long lo = 0, hi = gDbgLineCount - 1, best = -1
    do while lo <= hi
        dim as long mid_ = (lo + hi) \ 2
        if gDbgLine(mid_).addr = addr then
            return mid_
        elseif gDbgLine(mid_).addr < addr then
            best = mid_
            lo = mid_ + 1
        else
            hi = mid_ - 1
        end if
    loop
    ' Not an exact hit. A fault can land mid-statement, so the preceding line is the right
    ' answer -- but ONLY if the address is still inside that line's procedure. Without this
    ' bound, any address outside user code (a trap in ntdll, a frame that returns into a
    ' system DLL) resolves to the last line of the program and reports it confidently.
    if best < 0 then return -1
    dim as long pi = gDbgLine(best).procIndex
    if pi >= 0 then
        dim as longint last = gDbgProc(pi).addrEnd
        if last = 0 then last = gDbgProc(pi).addrEpilog
        if (last <> 0) andalso (addr >= last) then return -1
        if addr < gDbgProc(pi).addrStart then return -1
    end if
    return best
end function

function DebugInfo_AddrFromLine( byval srcIndex as long, byval lineNum as long ) as longint
    for i as long = 0 to gDbgLineCount - 1
        if (gDbgLine(i).srcIndex = srcIndex) andalso (gDbgLine(i).lineNum = lineNum) then
            return gDbgLine(i).addr
        end if
    next
    return 0
end function

function DebugInfo_ProcFromAddr( byval addr as longint ) as long
    for i as long = 0 to gDbgProcCount - 1
        with gDbgProc(i)
            if addr < .addrStart then continue for
            ' addrEnd is EXCLUSIVE -- it is the address just past the procedure, which is
            ' also the next procedure's entry. Testing addr <= addrEnd attributes every
            ' procedure's first instruction to the one before it.
            if .addrEnd <> 0 then
                if addr < .addrEnd then return i
            elseif .addrEpilog <> 0 then
                if addr <= .addrEpilog then return i
            else
                return i
            end if
        end with
    next
    return -1
end function

function DebugInfo_SourceFromName( byref fileName as string ) as long
    dim as string want = ucase( fileName )
    dim as long bs = instrrev( want, "\" )
    if bs = 0 then bs = instrrev( want, "/" )
    if bs > 0 then want = mid( want, bs + 1 )
    for i as long = 0 to gDbgSrcCount - 1
        if gDbgSrc(i).shortName = want then return i
    next
    return -1
end function

' A breakpoint is only useful on a line that generated code. The line table says exactly
' which those are -- something gdb never gave tiko, and the reason a breakpoint on a blank
' line or a declaration can now be moved rather than silently never firing.
function DebugInfo_IsExecutableLine( byval srcIndex as long, byval lineNum as long ) as boolean
    for i as long = 0 to gDbgLineCount - 1
        if (gDbgLine(i).srcIndex = srcIndex) andalso (gDbgLine(i).lineNum = lineNum) then
            ' The procedure's own entry address is the prologue, not an FB statement.
            dim as long pi = gDbgLine(i).procIndex
            if pi >= 0 then
                if gDbgLine(i).addr = gDbgProc(pi).addrStart then continue for
            end if
            return true
        end if
    next
    return false
end function

function DebugInfo_NextExecutable( byval srcIndex as long, byval lineNum as long ) as long
    dim as long best = -1
    for i as long = 0 to gDbgLineCount - 1
        if gDbgLine(i).srcIndex <> srcIndex then continue for
        if gDbgLine(i).lineNum < lineNum then continue for
        dim as long pi = gDbgLine(i).procIndex
        if pi >= 0 then
            if gDbgLine(i).addr = gDbgProc(pi).addrStart then continue for
        end if
        if (best = -1) orelse (gDbgLine(i).lineNum < best) then best = gDbgLine(i).lineNum
    next
    return best
end function

' Width in bytes of a stabs built-in, by its negative type code. These are the codes fbc
' actually emits -- read out of the declarations in the probe target, not assumed from the
' stabs specification, several of whose codes fbc never uses.
function DebugInfo_BaseSize( byval baseCode as long ) as long
    select case baseCode
        case -1  : return 4      ' int          -> FB Long   (64-bit) / Integer (32-bit)
        case -2  : return 1      ' char         -> FB Byte / ZString element
        case -3  : return 2      ' short
        case -5  : return 1      ' unsigned char
        case -6  : return 1      ' signed char
        case -7  : return 2      ' unsigned short
        case -8  : return 4      ' unsigned int
        case -11 : return 0      ' void
        case -12 : return 4      ' float        -> FB Single
        case -13 : return 8      ' double
        case -16 : return 1      ' boolean
        case -31 : return 8      ' long long    -> FB LongInt / Integer (64-bit)
        case -32 : return 8      ' unsigned long long
    end select
    return 0
end function

' Renders a fixed array's declared bounds, e.g. "(1 to 3, 0 to 2)".
function DebugInfo_ArrayShape( byval arrayIndex as long ) as string
    if (arrayIndex < 0) orelse (arrayIndex >= gDbgArrayCount) then return ""
    dim as string s = "("
    with gDbgArray(arrayIndex)
        for d as long = 0 to .dims - 1
            if d then s &= ", "
            s &= str(.lo(d)) & " to " & str(.hi(d))
        next
    end with
    return s & ")"
end function

' Formats one variable row for the dump. Shared by the per-procedure and file-scope passes
' so the two cannot drift into describing the same variable differently.
private function DumpVarLine( byval i as long, byref indent as string ) as string
    dim as string s
    with gDbgVar(i)
        s = indent & .nm & " : " & DebugInfo_TypeName( .typeId, .ptrDepth )
        if .arrayIndex = DBG_ARR_DYNAMIC then
            s &= " (dyn array)"
        elseif .arrayIndex >= 0 then
            s &= " " & DebugInfo_ArrayShape( .arrayIndex )
        end if
        s &= "   [" & DebugInfo_ScopeName( .scope ) & " "
        select case .scope
            case DBG_SCOPE_SHARED, DBG_SCOPE_STATIC, DBG_SCOPE_COMMON
                s &= "@&h" & hex(.addr)
            case else
                s &= "frame" & iif( .addr >= 0, "+", "" ) & str(.addr)
        end select
        s &= "]"
    end with
    return s
end function

' ==========================================================================================
' DebugInfo_Dump / DebugInfo_RunDump
'
' The headless verification path, and the reason this module is pure. A layout suite cannot
' reach a parser, and an assertion suite cannot tell you the tables are MEANINGFUL -- reading
' them back against the compiler's own .asm output is what does that.
'
'   TIKO_DEBUGINFO_DUMP=C:\proj\myapp.exe  tiko.exe
'   TIKO_DEBUGINFO_DUMP_DETAIL=1           also lists every line record
' ==========================================================================================
sub DebugInfo_Dump( byval detail as long )
    dim as string s

    print "=== TIKO_DEBUGINFO_DUMP ==="
    print "exe        : " & gDbg.exePath
    if gDbg.isLoaded = false then
        print "FAILED     : " & gDbg.lastError
        print "=== end ==="
        exit sub
    end if

    print "target     : " & iif( gDbg.target64, "64-bit (.dbgdat/.dbgstr)", "32-bit (.stab/.stabstr)" )
    print "compiler   : " & gDbg.compilerVer
    s = "imagebase  : &h" & hex(gDbg.imageBase) & "   loadbase: &h" & hex(gDbg.loadBase)
    print s
    if gDbg.lineCapHit then
        print "WARNING    : a line number reached 65535. The format stores it in a ushort, so"
        print "             lines past that in a single file cannot be represented at all."
    end if
    s = "sources=" & gDbgSrcCount & "  procs=" & gDbgProcCount & "  lines=" & gDbgLineCount
    s &= "  vars=" & gDbgVarCount & "  types=" & gDbgTypeCount & "  fields=" & gDbgFieldCount
    s &= "  arrays=" & gDbgArrayCount
    print s
    print

    print "--- sources ---"
    for i as long = 0 to gDbgSrcCount - 1
        print "  [" & i & "] " & gDbgSrc(i).shortName & "   " & gDbgSrc(i).fullName
    next
    print

    print "--- types ---"
    for i as long = 0 to gDbgTypeCount - 1
        with gDbgType(i)
            if (len(.nm) = 0) andalso (.isAlias = false) then continue for
            s = "  t" & i & "  " & .nm
            if .isBuiltIn then s &= "  <builtin base=" & .baseCode & ">"
            if .isEnum    then s &= "  <enum>"
            if .isAlias   then s &= "  <alias -> t" & .aliasType & " ptr=" & .aliasPtr & ">"
            if .sizeBytes then s &= "  size=" & .sizeBytes
            print s
            if .fieldFirst >= 0 then
                for k as long = .fieldFirst to .fieldLast
                    if .isEnum then
                        print "        " & gDbgField(k).nm & " = " & gDbgField(k).enumValue
                    else
                        dim as string fs = "        +" & gDbgField(k).byteOffset & "  " & _
                            gDbgField(k).nm & " : " & _
                            DebugInfo_TypeName( gDbgField(k).typeId, gDbgField(k).ptrDepth )
                        if gDbgField(k).arrayIndex = DBG_ARR_DYNAMIC then
                            fs &= " (dyn array)"
                        elseif gDbgField(k).arrayIndex >= 0 then
                            fs &= " " & DebugInfo_ArrayShape( gDbgField(k).arrayIndex )
                        end if
                        if gDbgField(k).bitLen then
                            fs &= "  <bitfield bit " & gDbgField(k).bitOffset & _
                                  " len " & gDbgField(k).bitLen & ">"
                        end if
                        print fs
                    end if
                next
            end if
        end with
    next
    print

    print "--- procedures ---"
    for i as long = 0 to gDbgProcCount - 1
        with gDbgProc(i)
            s = "  [" & i & "] " & .nm
            if .rawName <> .nm then s &= "   (raw " & .rawName & ")"
            print s
            s = "        start=&h" & hex(.addrStart) & "  first=&h" & hex(.addrFirst)
            s &= "  epilog=&h" & hex(.addrEpilog) & "  end=&h" & hex(.addrEnd)
            print s
            dim as string srcnm = "?"
            if (.srcIndex >= 0) andalso (.srcIndex < gDbgSrcCount) then srcnm = gDbgSrc(.srcIndex).shortName
            s = "        " & srcnm & ":" & .lineNum & "   returns "
            s &= iif( .isSub, "(sub)", DebugInfo_TypeName( .retType, .retPtr ) )
            print s
            if .varFirst >= 0 then
                for k as long = .varFirst to .varLast
                    print DumpVarLine( k, "          " )
                next
            end if
        end with
    next
    print

    print "--- file-scope variables (not owned by any procedure) ---"
    for i as long = 0 to gDbgVarCount - 1
        dim as boolean owned = false
        for k as long = 0 to gDbgProcCount - 1
            if gDbgProc(k).varFirst < 0 then continue for
            if (i >= gDbgProc(k).varFirst) andalso (i <= gDbgProc(k).varLast) then
                owned = true
                exit for
            end if
        next
        if owned = false then print DumpVarLine( i, "  " )
    next
    print

    if detail then
        print "--- line table ---"
        for i as long = 0 to gDbgLineCount - 1
            with gDbgLine(i)
                dim as string srcnm = "?"
                if (.srcIndex >= 0) andalso (.srcIndex < gDbgSrcCount) then srcnm = gDbgSrc(.srcIndex).shortName
                dim as string pnm = "-"
                if (.procIndex >= 0) andalso (.procIndex < gDbgProcCount) then pnm = gDbgProc(.procIndex).nm
                print "  &h" & hex(.addr) & "  " & srcnm & ":" & .lineNum & "   " & pnm
            end with
        next
        print
    end if

    ' A round trip through the address lookup. If the tables are self-consistent every line
    ' address resolves back to a record with that same address; anything else means the
    ' procedure-relative arithmetic is wrong, which is trap 1 and is otherwise invisible.
    dim as long checked = 0, bad = 0
    for i as long = 0 to gDbgLineCount - 1
        dim as long back = DebugInfo_LineFromAddr( gDbgLine(i).addr )
        if back < 0 then
            bad += 1
        elseif gDbgLine(back).addr <> gDbgLine(i).addr then
            bad += 1
        end if
        checked += 1
    next
    print "address -> line round trip: " & checked & " checked, " & bad & " mismatched"
    print "=== end ==="
end sub

sub DebugInfo_RunDump()
    dim as string target = environ( "TIKO_DEBUGINFO_DUMP" )
    if len(target) = 0 then exit sub
    dim as long detail = iif( environ("TIKO_DEBUGINFO_DUMP_DETAIL") = "1", 1, 0 )
    DebugInfo_Load( target )
    DebugInfo_Dump( detail )
    DebugInfo_Free()
end sub
