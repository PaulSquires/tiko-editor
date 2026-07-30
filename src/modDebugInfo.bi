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
' modDebugInfo - reader for the debug information fbc embeds with -g -gen gas / -gen gas64.
'
' The format is a stabs derivative private to FreeBASIC. fbc's own changelog describes it as
' emitted "for FBdebugger only" -- gdb cannot read it, which is why tiko used to force the
' gcc backend for every debug build. The only other implementation is L. Gras (SARG)'s
' FBdebugger, which is also the author of the gas64 emitter that produces this data. That
' program was read as the specification for this module; no code was copied from it.
'
' THIS MODULE IS PURE. Given a file on disk it fills tables and nothing else: no window, no
' thread, no process. That is deliberate -- it is the risky half of the debugger and it has
' to be verifiable headlessly, which is what DebugInfo_Dump exists for. It also means this
' pair can be compiled on its own, with no tiko global in scope.
'
' ------------------------------------------------------------------------------------------
' WHAT THE FORMAT ACTUALLY IS  (measured against fbc 1.10.1, both backends, not assumed)
'
'   -gen gas   (32-bit)   records in .stab     strings in .stabstr    12 bytes per record
'   -gen gas64 (64-bit)   records in .dbgdat   strings in .dbgstr     16 bytes per record
'
' gas32 emits .stabs assembler directives and lets the assembler build the section; gas64
' emits the records directly as .quad pairs. The grammar is identical either way.
'
'   record := strOffset as long | code as ushort | nline as ushort | addr (4 or 8 bytes)
'
' Codes used:  0 module header / module boundary     36  procedure
'             40 static or shared variable           42  main
'            100 compilation unit (path, then name; empty name = END of the unit)
'            128 local variable, and the built-in type declarations
'            132 include file                       160  procedure parameter
'            224 procedure epilog address           255  fbc version string
'
' ------------------------------------------------------------------------------------------
' SEVEN THINGS THAT SILENTLY PRODUCE WRONG ANSWERS. Each was measured, several the hard way.
'
' 1. Line addresses (code 68) are PROCEDURE-RELATIVE. Add the enclosing code-36 address.
'    A table built without that looks entirely sane and points nowhere near the right code.
'
' 2. Type ids RESTART IN EVERY COMPILATION UNIT. Each module re-declares t1..t17 and then
'    reuses t18+ for its own types, so module B's "t18" is not module A's. Everything above
'    DBG_TYPESTD is biased by DebugInfo_EndOfUnit. Without it a two-module program resolves
'    its second module's UDTs to the first module's types -- and still renders plausibly.
'
' 3. Locals and parameters carry SIGNED frame offsets (a local measured at -60); statics and
'    shareds carry absolute addresses. Read the field as signed or every local is at ~16 EB.
'
' 4. Bound the record walk by the section's VirtualSize, NEVER SizeOfRawData. The raw data is
'    padded up to the file alignment and that padding decodes as valid-looking code-0
'    records -- 26 of them in the probe that found this, which is how it was found.
'
' 5. Record 0 is the module header and is skipped -- but it is ALSO code 0, which is the
'    module-boundary marker. Treating code 0 as a terminator stops parsing at record zero.
'
' 6. "string:t13=s12" declares the wrong struct size on 64-bit (the descriptor is 24 bytes,
'    not 12). The FIELD OFFSETS in the same declaration are correct. Trust the offsets.
'
' 7. nline is a ushort, so line numbers saturate at 65535. Detected and reported rather than
'    silently mapping the whole tail of a long file to one line.
'
' ------------------------------------------------------------------------------------------
' THREE THINGS FBDEBUGGER DOES THAT WE DELIBERATELY DO NOT. All three were measured stale
' against fbc 1.10.1; copying them would introduce bugs that are not otherwise present.
'
' 1. It hardcodes the built-in type table behind #ifdef __FB_64BIT__. The stream DECLARES
'    those types inline ("integer:t1=-1" ... "boolean:t16=@s8;-16") and the ids are identical
'    on both backends, so we read them. This is also what lets one 64-bit tiko.exe debug a
'    32-bit target at all -- the reference implementation refuses that outright.
'
' 2. It rewrites type id 15 to 16 to work around gas emitting BOOLEAN wrongly. On 1.10.1
'    BOOLEAN is t16 on both backends and t15 is a real type (pchar), so that rewrite would
'    corrupt every pchar variable.
'
' 3. It picks the dynamic-array descriptor layout from its OWN compile-time __FB_VERSION__.
'    The descriptor is declared in the stream as a UDT with named fields, so we resolve
'    DATA / DIMENSIONS / DIMTB by NAME and no version test is needed anywhere.
'
' ------------------------------------------------------------------------------------------
' Note on strings: the tables below hold FB `string`, not DWSTRING, against the house default.
' Stabs strings are ASCII bytes out of a file and this parser is the one hot loop in the
' debugger -- a large target yields tens of thousands of symbols. Conversion to DWSTRING
' happens at the UI boundary, where the count is what fits on screen.
' ==========================================================================================

#pragma once

' Type ids at or below this are the built-in FreeBASIC types and are NOT biased per unit.
const as long DBG_TYPESTD = 17

' Sentinel in DBG_VAR.arrayIndex / DBG_FIELD.arrayIndex meaning "dynamic array", i.e. the
' variable is really an FBARRAY descriptor whose bounds only exist at run time.
const as long DBG_ARR_DYNAMIC = -2
const as long DBG_ARR_NONE    = -1

' Highest number of dimensions a fixed array table will hold.
const as long DBG_MAXDIM = 8

enum DBG_SCOPE
    DBG_SCOPE_LOCAL    = 1     ' frame relative
    DBG_SCOPE_SHARED   = 2     ' absolute
    DBG_SCOPE_STATIC   = 3     ' absolute
    DBG_SCOPE_PARAMREF = 4     ' frame relative, one extra indirection
    DBG_SCOPE_PARAMVAL = 5     ' frame relative
    DBG_SCOPE_COMMON   = 6     ' absolute
end enum

' ptrDepth encoding. A plain count is a data pointer; the two bands below mark a pointer to
' code, which is displayed as the target procedure's name rather than as a number.
const as long DBG_PTR_SUB_BASE  = 200      ' 200 + depth == pointer to SUB
const as long DBG_PTR_FUNC_BASE = 220      ' 220 + depth == pointer to FUNCTION

type DBG_SOURCE
    fullName   as string       ' path as fbc recorded it
    shortName  as string       ' file name only, upper case, for matching editor documents
end type

type DBG_ARRAYDEF
    dims       as long
    lo(0 to DBG_MAXDIM - 1) as longint
    hi(0 to DBG_MAXDIM - 1) as longint
end type

type DBG_FIELD
    nm         as string
    typeId     as long
    ptrDepth   as long
    arrayIndex as long         ' into gDbgArray, or DBG_ARR_NONE / DBG_ARR_DYNAMIC
    byteOffset as longint      ' from the start of the owning UDT
    bitOffset  as long         ' within the byte, bitfields only
    bitLen     as long         ' 0 unless a bitfield
    declBytes  as longint      ' declared length. The ONLY place a fixed-length string's
                               ' size survives -- "FIXED:4,96,128" is a zstring*16, and the
                               ' type reference alone just says "char".
    enumValue  as longint      ' enum members only
end type

type DBG_TYPE
    nm         as string
    sizeBytes  as longint      ' as declared; see trap 6 -- not always trustworthy
    isEnum     as boolean
    isBuiltIn  as boolean
    baseCode   as long         ' negative stabs built-in code, 0 for aggregates
    fieldFirst as long         ' into gDbgField, -1 if none
    fieldLast  as long
    ' An id can be introduced as "20=*1" -- a name for "pointer to type 1" -- and then
    ' referenced bare as ":20" by a later variable. fbdebugger flattens the pointer onto
    ' whichever variable carried the definition and loses the id, so a later bare reference
    ' resolves to the wrong thing. Recording the alias here keeps both forms working.
    isAlias    as boolean
    aliasType  as long
    aliasPtr   as long
    aliasArray as long
end type

type DBG_VAR
    nm         as string
    typeId     as long
    ptrDepth   as long
    arrayIndex as long
    scope      as DBG_SCOPE
    addr       as longint      ' absolute VA, or a SIGNED frame offset -- see trap 3
end type

type DBG_PROC
    nm         as string       ' demangled
    rawName    as string       ' as emitted, for diagnostics
    addrStart  as longint      ' entry point
    addrFirst  as longint      ' first FB statement, i.e. past the prologue
    addrEpilog as longint      ' from code 224
    addrEnd    as longint
    srcIndex   as long
    lineNum    as long
    varFirst   as long         ' into gDbgVar
    varLast    as long
    retType    as long
    retPtr     as long
    isSub      as boolean
    isModLevel as boolean      ' the {MODLEVEL} pseudo-procedure, i.e. module main
end type

type DBG_LINE
    addr       as longint      ' ABSOLUTE, already rebased off the owning procedure
    lineNum    as long
    procIndex  as long
    srcIndex   as long
end type

type DBG_INFO
    isLoaded      as boolean
    target64      as boolean       ' target bitness, NOT the debugger's
    imageBase     as ulongint      ' preferred base from the PE header
    loadBase      as ulongint      ' base actually used when the tables were built
    compilerVer   as string        ' from code 255
    exePath       as string
    lineCapHit    as boolean       ' a line number saturated the ushort -- see trap 7
    lastError     as string
end type

extern gDbg      as DBG_INFO
extern gDbgSrc()   as DBG_SOURCE
extern gDbgLine()  as DBG_LINE
extern gDbgProc()  as DBG_PROC
extern gDbgVar()   as DBG_VAR
extern gDbgType()  as DBG_TYPE
extern gDbgField() as DBG_FIELD
extern gDbgArray() as DBG_ARRAYDEF

extern gDbgSrcCount   as long
extern gDbgLineCount  as long
extern gDbgProcCount  as long
extern gDbgVarCount   as long
extern gDbgTypeCount  as long
extern gDbgFieldCount as long
extern gDbgArrayCount as long

declare function DebugInfo_Load    ( byref imgPath as string, byval imgLoadBase as ulongint = 0 ) as boolean
declare sub      DebugInfo_Free    ()
declare sub      DebugInfo_Dump    ( byval detail as long = 0 )
declare sub      DebugInfo_RunDump ()

' Lookups. Both directions, because the debugger needs both on every stop.
declare function DebugInfo_LineFromAddr    ( byval addr as longint ) as long
declare function DebugInfo_AddrFromLine    ( byval srcIndex as long, byval lineNum as long ) as longint
declare function DebugInfo_ProcFromAddr    ( byval addr as longint ) as long
declare function DebugInfo_SourceFromName  ( byref fileName as string ) as long
declare function DebugInfo_IsExecutableLine( byval srcIndex as long, byval lineNum as long ) as boolean
declare function DebugInfo_NextExecutable  ( byval srcIndex as long, byval lineNum as long ) as long
declare function DebugInfo_TypeName        ( byval typeId as long, byval ptrDepth as long = 0 ) as string
declare function DebugInfo_Demangle        ( byref rawName as string ) as string
declare function DebugInfo_ScopeName       ( byval sc as DBG_SCOPE ) as string
declare function DebugInfo_ArrayShape      ( byval arrayIndex as long ) as string
declare function DebugInfo_BaseSize        ( byval baseCode as long ) as long
