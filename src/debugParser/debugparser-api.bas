'' debugParser DLL entry points.
''
'' Thin, defensive wrappers over dbginfo / dbgprocess / dbgeval. See debugParser.bi for the
'' API documentation and the threading contract.
''
'' Everything here converts between the DLL's internal FB `string` world and the fixed-size
'' wstring buffers the host sees. Internal names stay `string` because they come out of a
'' byte-oriented parser and a large target yields tens of thousands of them; the conversion
'' happens here, at the boundary, where the count is what a host actually asks for.

#define _WIN32_WINNT &h0602
#include once "windows.bi"
#include once "crt.bi"

#define DEBUGPARSER_BUILDING_DLL
#include once "debugParser.bi"

#include once "dbginfo.bas"
#include once "dbgprocess.bas"
#include once "dbgeval.bas"

'' Copy an internal string into a caller-supplied fixed wstring buffer, always terminated.
private sub CopyOut( byval wszBuf as wstring ptr, byval cchBuf as long, byref src as string )
    if (wszBuf = 0) orelse (cchBuf <= 0) then exit sub
    dim as wstring * 4096 tmp = wstr( src )
    dim as long n = len( tmp )
    if n > cchBuf - 1 then n = cchBuf - 1
    *wszBuf = left( tmp, n )
end sub

'' ==========================================================================================
'' Static debug information
'' ==========================================================================================
function debugparser_load( byval wszExePath as wstring ptr, byval loadBase as ulongint ) as long export
    if wszExePath = 0 then return DBGP_ERR_BADARG
    dim as string p = str( *wszExePath )
    if DebugInfo_Load( p, loadBase ) then return DBGP_OK

    '' Map the internal message onto a code the host can branch on, without the host having
    '' to match on text.
    if instr( gDbg.lastError, "no debug information" ) then return DBGP_ERR_NODEBUGINFO
    if instr( gDbg.lastError, "not a PE" ) orelse instr( gDbg.lastError, "not an MZ" ) then return DBGP_ERR_BADIMAGE
    return DBGP_ERR_IOERROR
end function

sub debugparser_unload() export
    DebugInfo_Free()
end sub

function debugparser_is_loaded() as long export
    return iif( gDbg.isLoaded, 1, 0 )
end function

sub debugparser_get_error( byval wszBuf as wstring ptr, byval cchBuf as long ) export
    dim as string s = gDbg.lastError
    if len( gDbgSession.lastError ) then s = gDbgSession.lastError
    CopyOut( wszBuf, cchBuf, s )
end sub

function debugparser_target_is_64bit() as long export
    return iif( gDbg.target64, 1, 0 )
end function

sub debugparser_get_compiler( byval wszBuf as wstring ptr, byval cchBuf as long ) export
    CopyOut( wszBuf, cchBuf, gDbg.compilerVer )
end sub

function debugparser_line_cap_hit() as long export
    return iif( gDbg.lineCapHit, 1, 0 )
end function

function debugparser_source_count() as long export
    return gDbgSrcCount
end function

function debugparser_source_path( byval srcIndex as long, byval wszBuf as wstring ptr, byval cchBuf as long ) as long export
    if (srcIndex < 0) orelse (srcIndex >= gDbgSrcCount) then return DBGP_ERR_BADARG
    CopyOut( wszBuf, cchBuf, gDbgSrc(srcIndex).fullName )
    return DBGP_OK
end function

function debugparser_source_index( byval wszFileName as wstring ptr ) as long export
    if wszFileName = 0 then return -1
    dim as string f = str( *wszFileName )
    return DebugInfo_SourceFromName( f )
end function

function debugparser_is_executable_line( byval srcIndex as long, byval lineNum as long ) as long export
    return iif( DebugInfo_IsExecutableLine( srcIndex, lineNum ), 1, 0 )
end function

function debugparser_next_executable_line( byval srcIndex as long, byval lineNum as long ) as long export
    return DebugInfo_NextExecutable( srcIndex, lineNum )
end function

'' ==========================================================================================
'' Session
'' ==========================================================================================
function debugparser_start( byval wszExePath as wstring ptr, byval wszCmdLine as wstring ptr, byval hNotifyWnd as any ptr ) as long export
    if wszExePath = 0 then return DBGP_ERR_BADARG
    if gDbgSession.isActive then return DBGP_ERR_BUSY
    dim as string p = str( *wszExePath )
    dim as string c
    if wszCmdLine then c = str( *wszCmdLine )
    if DebugProc_Start( p, c, cast(HWND, hNotifyWnd) ) then return DBGP_OK
    return DBGP_ERR_LAUNCH
end function

sub debugparser_stop() export
    DebugProc_Stop()
end sub

function debugparser_resume( byval mode as long ) as long export
    if gDbgSession.isActive = false then return DBGP_ERR_NOSESSION
    if (mode < 0) orelse (mode > DBGP_RUN_TOCURSOR) then return DBGP_ERR_BADARG
    DebugProc_Resume( cast(DBG_RUNMODE, mode) )
    return DBGP_OK
end function

sub debugparser_pause() export
    DebugProc_Pause()
end sub

function debugparser_is_active() as long export
    return iif( DebugProc_IsActive(), 1, 0 )
end function

function debugparser_is_running() as long export
    return iif( DebugProc_IsRunning(), 1, 0 )
end function

function debugparser_get_stop( byval pStop as DBGP_STOPINFO ptr ) as long export
    if pStop = 0 then return DBGP_ERR_BADARG
    if gDbgSession.isActive = false then return DBGP_ERR_NOSESSION

    clear *pStop, 0, sizeof(DBGP_STOPINFO)
    with *pStop
        .reason       = cast(long, gDbgStop.reason)
        .threadId     = gDbgStop.threadId
        .addr         = gDbgStop.addr
        .srcIndex     = gDbgStop.srcIndex
        .lineNum      = gDbgStop.lineNum
        .frameCount   = gDbgStop.frameCount
        .exitCode     = gDbgStop.exitCode
        .excepCode    = gDbgStop.excepCode
        .excepAddr    = gDbgStop.excepAddr
        .excepIsWrite = iif( gDbgStop.excepIsWrite, 1, 0 )
        .excepTarget  = gDbgStop.excepTarget
        if gDbgStop.procIndex >= 0 then .wszProcName = wstr( gDbgProc(gDbgStop.procIndex).nm )
        if gDbgStop.srcIndex  >= 0 then .wszSrcPath  = wstr( gDbgSrc(gDbgStop.srcIndex).fullName )
        .wszExcepText = wstr( gDbgStop.excepText )
    end with
    return DBGP_OK
end function

function debugparser_get_frame( byval frameIndex as long, byval pFrame as DBGP_FRAME ptr ) as long export
    if pFrame = 0 then return DBGP_ERR_BADARG
    if (frameIndex < 0) orelse (frameIndex >= gDbgStop.frameCount) then return DBGP_ERR_NOTFOUND

    clear *pFrame, 0, sizeof(DBGP_FRAME)
    with gDbgStop.frames(frameIndex)
        pFrame->addrPC    = .addrPC
        pFrame->frameBase = .frameBase
        pFrame->srcIndex  = -1
        pFrame->lineNum   = 0
        if .lineIndex >= 0 then
            pFrame->srcIndex = gDbgLine(.lineIndex).srcIndex
            pFrame->lineNum  = gDbgLine(.lineIndex).lineNum
            if pFrame->srcIndex >= 0 then pFrame->wszSrcPath = wstr( gDbgSrc(pFrame->srcIndex).fullName )
        end if
        if .procIndex >= 0 then pFrame->wszProcName = wstr( gDbgProc(.procIndex).nm )
    end with
    return DBGP_OK
end function

'' ==========================================================================================
'' Breakpoints
'' ==========================================================================================
function debugparser_add_breakpoint( byval srcIndex as long, byval lineNum as long ) as long export
    if (srcIndex < 0) orelse (srcIndex >= gDbgSrcCount) then return DBGP_ERR_BADARG
    DebugProc_AddBreakpoint( srcIndex, lineNum )
    return DBGP_OK
end function

function debugparser_remove_breakpoint( byval srcIndex as long, byval lineNum as long ) as long export
    if DebugProc_RemoveBreakpoint( srcIndex, lineNum ) then return DBGP_OK
    return DBGP_ERR_NOTFOUND
end function

sub debugparser_clear_breakpoints() export
    DebugProc_ClearBreakpoints()
end sub

function debugparser_set_run_to_cursor( byval srcIndex as long, byval lineNum as long ) as long export
    if (srcIndex < 0) orelse (srcIndex >= gDbgSrcCount) then return DBGP_ERR_BADARG
    DebugProc_SetRunToCursor( srcIndex, lineNum )
    return DBGP_OK
end function

'' ==========================================================================================
'' Variables
'' ==========================================================================================
private sub NodeOut( byref src as DBG_NODE, byval pOut as DBGP_NODE ptr )
    clear *pOut, 0, sizeof(DBGP_NODE)
    with *pOut
        .addr        = src.addr
        .typeId      = src.typeId
        .descTypeId  = src.descTypeId
        .ptrDepth    = src.ptrDepth
        .arrayIndex  = src.arrayIndex
        .declBytes   = src.declBytes
        .bitOffset   = src.bitOffset
        .bitLen      = src.bitLen
        .scope       = cast(long, src.scope)
        .isValid     = iif( src.isValid, 1, 0 )
        .hasChildren = iif( src.hasChildren, 1, 0 )
        .wszName     = wstr( src.nm )
        .wszType     = wstr( src.typeName )
        .wszValue    = wstr( src.valueText )
    end with
end sub

private sub NodeIn( byval pIn as DBGP_NODE ptr, byref dst as DBG_NODE )
    dst = type<DBG_NODE>()
    with *pIn
        dst.addr        = .addr
        dst.typeId      = .typeId
        dst.descTypeId  = .descTypeId
        dst.ptrDepth    = .ptrDepth
        dst.arrayIndex  = .arrayIndex
        dst.declBytes   = .declBytes
        dst.bitOffset   = .bitOffset
        dst.bitLen      = .bitLen
        dst.scope       = cast(DBG_SCOPE, .scope)
        dst.isValid     = (.isValid <> 0)
        dst.hasChildren = (.hasChildren <> 0)
        dst.nm          = str( .wszName )
    end with
end sub

function debugparser_local_count( byval frameIndex as long ) as long export
    dim as long vf = DebugEval_FrameVarFirst( frameIndex )
    dim as long vl = DebugEval_FrameVarLast( frameIndex )
    if (vf < 0) orelse (vl < vf) then return 0
    return vl - vf + 1
end function

function debugparser_local_node( byval frameIndex as long, byval index as long, byval pNode as DBGP_NODE ptr ) as long export
    if pNode = 0 then return DBGP_ERR_BADARG
    dim as long vf = DebugEval_FrameVarFirst( frameIndex )
    dim as long vl = DebugEval_FrameVarLast( frameIndex )
    if (vf < 0) orelse (vl < vf) then return DBGP_ERR_NOTFOUND
    if (index < 0) orelse (vf + index > vl) then return DBGP_ERR_NOTFOUND

    dim as DBG_NODE n
    if DebugEval_VarNode( vf + index, frameIndex, n ) = false then return DBGP_ERR_NOTFOUND
    NodeOut( n, pNode )
    return DBGP_OK
end function

'' Module-level variables are not contiguous in the table, so they are counted by walking.
'' Both calls agree because they use the same predicate.
private function GlobalIndexToVar( byval index as long ) as long
    dim as long seen = 0
    for i as long = 0 to gDbgVarCount - 1
        if DebugEval_IsGlobal( i ) = false then continue for
        if seen = index then return i
        seen += 1
    next
    return -1
end function

function debugparser_global_count() as long export
    dim as long n = 0
    for i as long = 0 to gDbgVarCount - 1
        if DebugEval_IsGlobal( i ) then n += 1
    next
    return n
end function

function debugparser_global_node( byval index as long, byval pNode as DBGP_NODE ptr ) as long export
    if pNode = 0 then return DBGP_ERR_BADARG
    dim as long vi = GlobalIndexToVar( index )
    if vi < 0 then return DBGP_ERR_NOTFOUND
    dim as DBG_NODE n
    if DebugEval_VarNode( vi, -1, n ) = false then return DBGP_ERR_NOTFOUND
    NodeOut( n, pNode )
    return DBGP_OK
end function

function debugparser_child_count( byval pNode as DBGP_NODE ptr ) as long export
    if pNode = 0 then return 0
    dim as DBG_NODE n
    NodeIn( pNode, n )
    return DebugEval_ChildCount( n )
end function

function debugparser_child_node( byval pNode as DBGP_NODE ptr, byval index as long, byval pOut as DBGP_NODE ptr ) as long export
    if (pNode = 0) orelse (pOut = 0) then return DBGP_ERR_BADARG
    dim as DBG_NODE parent, child
    NodeIn( pNode, parent )
    if DebugEval_ChildNode( parent, index, child ) = false then return DBGP_ERR_NOTFOUND
    NodeOut( child, pOut )
    return DBGP_OK
end function

'' The node is the argument rather than its arrayIndex because for a REDIM array the bounds are
'' not in the debug information at all -- they have to be read from the descriptor at this
'' node's own address, in the stopped debuggee.
function debugparser_array_info( byval pNode as DBGP_NODE ptr, byval pOut as DBGP_ARRAYINFO ptr ) as long export
    if (pNode = 0) orelse (pOut = 0) then return DBGP_ERR_BADARG
    clear *pOut, 0, sizeof(DBGP_ARRAYINFO)

    dim as DBG_NODE n
    NodeIn( pNode, n )
    dim as DBG_ARRAYBOUNDS ab
    if DebugEval_ArrayBounds( n, ab ) = false then return DBGP_ERR_NOTFOUND

    with *pOut
        .dims      = ab.dims
        .elemSize  = ab.elemSize
        .total     = ab.total
        .isDynamic = iif( ab.isDynamic, 1, 0 )
        for d as long = 0 to ab.dims - 1
            .lo(d) = ab.lo(d)
            .hi(d) = ab.hi(d)
        next
    end with
    return DBGP_OK
end function

function debugparser_evaluate( byval wszExpr as wstring ptr, byval frameIndex as long, byval pNode as DBGP_NODE ptr ) as long export
    if (wszExpr = 0) orelse (pNode = 0) then return DBGP_ERR_BADARG
    dim as string e = str( *wszExpr )
    dim as DBG_NODE n
    if DebugEval_Lookup( e, frameIndex, n ) = false then return DBGP_ERR_NOTFOUND
    NodeOut( n, pNode )
    return DBGP_OK
end function
