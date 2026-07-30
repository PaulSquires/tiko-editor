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

' See modDebugProcess.bi for the threading model, the stepping model and the pause caveat.

#include once "modDebugProcess.bi"

dim shared gDbgSession as DBG_SESSION
dim shared gDbgStop    as DBG_STOPINFO
dim shared gDbgBp()    as DBG_BREAKPOINT
dim shared gDbgBpCount as long

' Per-line trap bookkeeping, parallel to gDbgLine. Kept out of DBG_LINE so that modDebugInfo
' stays pure -- an original opcode byte is process state, not debug information.
dim shared gDbgLineOrig()  as ubyte
dim shared gDbgLineArmed() as boolean

' Live debuggee threads. Needed because a stop can be reported on any of them and the
' context has to be fetched from the right handle.
type DBG_THREADREC
    dwThreadId as DWORD
    hThread    as HANDLE
end type
dim shared gDbgThread()   as DBG_THREADREC
dim shared gDbgThreadCount as long

const as ubyte DBG_INT3 = &hCC

' The line whose trap was removed for a single-step, so the single-step handler knows what
' to put back. Only ever touched on the worker thread.
dim shared gDbgPendingRearm as ulongint

' ==========================================================================================
' Memory
' ==========================================================================================
function DebugProc_ReadMem( byval addr as ulongint, byval dest as any ptr, byval nBytes as long ) as boolean
    if gDbgSession.hProcess = 0 then return false
    if (addr = 0) orelse (nBytes <= 0) then return false
    dim as SIZE_T got
    if ReadProcessMemory( gDbgSession.hProcess, cast(LPCVOID, addr), dest, nBytes, @got ) = 0 then return false
    return (got = nBytes)
end function

function DebugProc_WriteMem( byval addr as ulongint, byval src as any ptr, byval nBytes as long ) as boolean
    if gDbgSession.hProcess = 0 then return false
    dim as SIZE_T nWrote
    dim as DWORD oldProt
    ' Code pages are read/execute, so arming a trap needs the page made writable first.
    VirtualProtectEx( gDbgSession.hProcess, cast(LPVOID, addr), nBytes, PAGE_EXECUTE_READWRITE, @oldProt )
    dim as long res = WriteProcessMemory( gDbgSession.hProcess, cast(LPVOID, addr), src, nBytes, @nWrote )
    VirtualProtectEx( gDbgSession.hProcess, cast(LPVOID, addr), nBytes, oldProt, @oldProt )
    if res = 0 then return false
    FlushInstructionCache( gDbgSession.hProcess, cast(LPCVOID, addr), nBytes )
    return (nWrote = nBytes)
end function

' Pointer width of the TARGET, which is not necessarily the debugger's.
function DebugProc_PointerSize() as long
    return iif( gDbgSession.isWow64, 4, 8 )
end function

function DebugProc_ReadPtr( byval addr as ulongint, byref outVal as ulongint ) as boolean
    outVal = 0
    if gDbgSession.isWow64 then
        dim as ulong v32
        if DebugProc_ReadMem( addr, @v32, 4 ) = false then return false
        outVal = v32
    else
        dim as ulongint v64
        if DebugProc_ReadMem( addr, @v64, 8 ) = false then return false
        outVal = v64
    end if
    return true
end function

' ==========================================================================================
' Threads and registers
' ==========================================================================================
private function ThreadHandleFor( byval dwThreadId as DWORD ) as HANDLE
    for i as long = 0 to gDbgThreadCount - 1
        if gDbgThread(i).dwThreadId = dwThreadId then return gDbgThread(i).hThread
    next
    return 0
end function

private sub ThreadAdd( byval dwThreadId as DWORD, byval hThread as HANDLE )
    if gDbgThreadCount > ubound(gDbgThread) then redim preserve gDbgThread( gDbgThreadCount + 15 )
    gDbgThread(gDbgThreadCount).dwThreadId = dwThreadId
    gDbgThread(gDbgThreadCount).hThread    = hThread
    gDbgThreadCount += 1
end sub

private sub ThreadRemove( byval dwThreadId as DWORD )
    for i as long = 0 to gDbgThreadCount - 1
        if gDbgThread(i).dwThreadId = dwThreadId then
            for k as long = i to gDbgThreadCount - 2
                gDbgThread(k) = gDbgThread(k + 1)
            next
            gDbgThreadCount -= 1
            exit sub
        end if
    next
end sub

' x64 CONTEXT holds XMM registers and must sit on a 16-byte boundary. fbc does not guarantee
' that for a plain local, and the failure mode is silent corruption of the neighbouring stack
' slot rather than an error, so the buffer is allocated and aligned by hand.
private function GetRegs( byval dwThreadId as DWORD, byref regs as DBG_REGS ) as boolean
    dim as HANDLE hThread = ThreadHandleFor( dwThreadId )
    if hThread = 0 then return false

    if gDbgSession.isWow64 then
        dim as WOW64_CONTEXT ctx32
        ctx32.ContextFlags = WOW64_CONTEXT_CONTROL or WOW64_CONTEXT_INTEGER
        if Wow64GetThreadContext( hThread, @ctx32 ) = 0 then return false
        regs.pc    = ctx32.Eip
        regs.fp    = ctx32.Ebp
        regs.sp    = ctx32.Esp
        regs.flags = ctx32.EFlags
        return true
    end if

    dim as ubyte ptr raw = allocate( sizeof(CONTEXT) + 16 )
    if raw = 0 then return false
    dim as CONTEXT ptr pCtx = cast(CONTEXT ptr, (cast(ulongint, raw) + 15) and (not cast(ulongint, 15)))
    ' CONTEXT_INTEGER is required for Rbp -- CONTEXT_CONTROL alone does not return it.
    pCtx->ContextFlags = CONTEXT_CONTROL or CONTEXT_INTEGER
    dim as boolean bGot = (GetThreadContext( hThread, pCtx ) <> 0)
    if bGot then
        regs.pc    = pCtx->Rip
        regs.fp    = pCtx->Rbp
        regs.sp    = pCtx->Rsp
        regs.flags = pCtx->EFlags
    end if
    deallocate raw
    return bGot
end function

private function SetRegs( byval dwThreadId as DWORD, byref regs as DBG_REGS ) as boolean
    dim as HANDLE hThread = ThreadHandleFor( dwThreadId )
    if hThread = 0 then return false

    if gDbgSession.isWow64 then
        dim as WOW64_CONTEXT ctx32
        ctx32.ContextFlags = WOW64_CONTEXT_CONTROL or WOW64_CONTEXT_INTEGER
        if Wow64GetThreadContext( hThread, @ctx32 ) = 0 then return false
        ctx32.Eip    = cast(DWORD, regs.pc)
        ctx32.EFlags = cast(DWORD, regs.flags)
        return (Wow64SetThreadContext( hThread, @ctx32 ) <> 0)
    end if

    dim as ubyte ptr raw = allocate( sizeof(CONTEXT) + 16 )
    if raw = 0 then return false
    dim as CONTEXT ptr pCtx = cast(CONTEXT ptr, (cast(ulongint, raw) + 15) and (not cast(ulongint, 15)))
    pCtx->ContextFlags = CONTEXT_CONTROL or CONTEXT_INTEGER
    dim as boolean bDone = false
    if GetThreadContext( hThread, pCtx ) <> 0 then
        pCtx->Rip    = regs.pc
        pCtx->EFlags = cast(DWORD, regs.flags)
        bDone = (SetThreadContext( hThread, pCtx ) <> 0)
    end if
    deallocate raw
    return bDone
end function

' ==========================================================================================
' Line traps
' ==========================================================================================
private sub ArmLineTraps()
    if gDbgSession.ccArmed then exit sub
    for i as long = 0 to gDbgLineCount - 1
        if gDbgLineArmed(i) then continue for
        ' Skip the prologue. fbc emits a line record at each procedure's entry address, and
        ' trapping it makes Step Into stop on the function header instead of its first
        ' statement -- and reports a line number that is not where execution actually is.
        dim as long pi = gDbgLine(i).procIndex
        if pi >= 0 then
            if gDbgLine(i).addr = gDbgProc(pi).addrStart then continue for
        end if
        dim as ubyte orig
        if DebugProc_ReadMem( gDbgLine(i).addr, @orig, 1 ) = false then continue for
        gDbgLineOrig(i) = orig
        dim as ubyte cc = DBG_INT3
        if DebugProc_WriteMem( gDbgLine(i).addr, @cc, 1 ) then gDbgLineArmed(i) = true
    next
    gDbgSession.ccArmed = true
end sub

private sub DisarmLineTraps()
    if gDbgSession.ccArmed = false then exit sub
    for i as long = 0 to gDbgLineCount - 1
        if gDbgLineArmed(i) = false then continue for
        DebugProc_WriteMem( gDbgLine(i).addr, @gDbgLineOrig(i), 1 )
        gDbgLineArmed(i) = false
    next
    gDbgSession.ccArmed = false
end sub

' Arms only the addresses the user asked for, plus any temporary target. Used for every RUN
' mode; the line traps stay off so the debuggee executes at full speed.
private sub ArmUserBreakpoints()
    for b as long = 0 to gDbgBpCount - 1
        if gDbgBp(b).enabled = false then continue for
        if gDbgBp(b).addr = 0 then continue for
        dim as long li = DebugInfo_LineFromAddr( cast(longint, gDbgBp(b).addr) )
        if li < 0 then continue for
        if gDbgLineArmed(li) then continue for
        dim as ubyte orig
        if DebugProc_ReadMem( gDbgBp(b).addr, @orig, 1 ) = false then continue for
        gDbgLineOrig(li) = orig
        dim as ubyte cc = DBG_INT3
        if DebugProc_WriteMem( gDbgBp(b).addr, @cc, 1 ) then gDbgLineArmed(li) = true
    next

    if gDbgSession.tempBpAddr <> 0 then
        dim as long li = DebugInfo_LineFromAddr( cast(longint, gDbgSession.tempBpAddr) )
        if li >= 0 then
            if gDbgLineArmed(li) = false then
                dim as ubyte orig
                if DebugProc_ReadMem( gDbgSession.tempBpAddr, @orig, 1 ) then
                    gDbgLineOrig(li) = orig
                    dim as ubyte cc = DBG_INT3
                    if DebugProc_WriteMem( gDbgSession.tempBpAddr, @cc, 1 ) then gDbgLineArmed(li) = true
                end if
            end if
        end if
    end if
end sub

' Resuming from an address that carries a trap: restore the byte, rewind the instruction
' pointer over the INT3 we already consumed, set the trap flag, and let one instruction run.
' The single-step handler puts the trap back. Nothing else may do this dance.
private function StepOverTrap( byval dwThreadId as DWORD, byval addr as ulongint ) as boolean
    dim as long li = DebugInfo_LineFromAddr( cast(longint, addr) )
    if li < 0 then return false
    if gDbgLineArmed(li) = false then return false

    DebugProc_WriteMem( addr, @gDbgLineOrig(li), 1 )
    gDbgLineArmed(li) = false

    dim as DBG_REGS regs
    if GetRegs( dwThreadId, regs ) = false then return false
    regs.pc = addr
    regs.flags = regs.flags or &h100          ' EFLAGS.TF
    SetRegs( dwThreadId, regs )
    gDbgPendingRearm = addr
    return true
end function

' ==========================================================================================
' Breakpoints (the user's, as opposed to the line traps stepping uses)
' ==========================================================================================
function DebugProc_FindBreakpoint( byval srcIndex as long, byval lineNum as long ) as long
    for i as long = 0 to gDbgBpCount - 1
        if (gDbgBp(i).srcIndex = srcIndex) andalso (gDbgBp(i).lineNum = lineNum) then return i
    next
    return -1
end function

function DebugProc_AddBreakpoint( byval srcIndex as long, byval lineNum as long ) as long
    dim as long existing = DebugProc_FindBreakpoint( srcIndex, lineNum )
    if existing >= 0 then return existing
    if gDbgBpCount > ubound(gDbgBp) then redim preserve gDbgBp( gDbgBpCount + 31 )
    with gDbgBp(gDbgBpCount)
        .srcIndex = srcIndex
        .lineNum  = lineNum
        .addr     = cast(ulongint, DebugInfo_AddrFromLine( srcIndex, lineNum ))
        .enabled  = true
    end with
    gDbgBpCount += 1
    return gDbgBpCount - 1
end function

function DebugProc_RemoveBreakpoint( byval srcIndex as long, byval lineNum as long ) as boolean
    dim as long i = DebugProc_FindBreakpoint( srcIndex, lineNum )
    if i < 0 then return false
    for k as long = i to gDbgBpCount - 2
        gDbgBp(k) = gDbgBp(k + 1)
    next
    gDbgBpCount -= 1
    return true
end function

sub DebugProc_ClearBreakpoints()
    gDbgBpCount = 0
end sub

sub DebugProc_SetRunToCursor( byval srcIndex as long, byval lineNum as long )
    gDbgSession.tempBpAddr = cast(ulongint, DebugInfo_AddrFromLine( srcIndex, lineNum ))
end sub

' ==========================================================================================
' Call stack
'
' Frame-pointer walk: [rbp] is the caller's frame, [rbp+ptr] the return address. It ends as
' soon as the recovered instruction pointer leaves known code, which is how it stops at the
' boundary with the C runtime instead of wandering. There are no unwind tables in play.
' ==========================================================================================
private sub BuildCallStack( byval dwThreadId as DWORD )
    gDbgStop.frameCount = 0

    dim as DBG_REGS regs
    if GetRegs( dwThreadId, regs ) = false then exit sub

    dim as long ptrSize = DebugProc_PointerSize()
    dim as ulongint pc = regs.pc
    dim as ulongint fp = regs.fp

    do while gDbgStop.frameCount < DBG_MAXFRAMES
        dim as long pi = DebugInfo_ProcFromAddr( cast(longint, pc) )
        if pi < 0 then exit do

        with gDbgStop.frames(gDbgStop.frameCount)
            .addrPC    = pc
            .frameBase = fp
            .procIndex = pi
            ' For a caller frame, pc is the RETURN address -- the instruction after the
            ' call, which usually belongs to the NEXT source line. Resolving pc-1 reports
            ' the line containing the call, which is what clicking that frame should show.
            if gDbgStop.frameCount = 0 then
                .lineIndex = DebugInfo_LineFromAddr( cast(longint, pc) )
            else
                .lineIndex = DebugInfo_LineFromAddr( cast(longint, pc) - 1 )
            end if
            .retAddr   = 0
        end with

        dim as ulongint retAddr, nextFp
        if DebugProc_ReadPtr( fp + ptrSize, retAddr ) = false then
            gDbgStop.frameCount += 1
            exit do
        end if
        if DebugProc_ReadPtr( fp, nextFp ) = false then
            gDbgStop.frameCount += 1
            exit do
        end if
        gDbgStop.frames(gDbgStop.frameCount).retAddr = retAddr
        gDbgStop.frameCount += 1

        ' A frame pointer must move upward; anything else means the chain is not a chain and
        ' following it would loop or read arbitrary memory.
        if (nextFp <= fp) orelse (retAddr = 0) then exit do
        pc = retAddr
        fp = nextFp
    loop
end sub

' ==========================================================================================
' Exception naming
' ==========================================================================================
function DebugProc_ExceptionText( byval code as DWORD ) as string
    select case code
        case EXCEPTION_ACCESS_VIOLATION      : return "Access violation"
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED : return "Array bounds exceeded"
        case EXCEPTION_DATATYPE_MISALIGNMENT : return "Datatype misalignment"
        case EXCEPTION_FLT_DIVIDE_BY_ZERO    : return "Floating point division by zero"
        case EXCEPTION_FLT_OVERFLOW          : return "Floating point overflow"
        case EXCEPTION_FLT_UNDERFLOW         : return "Floating point underflow"
        case EXCEPTION_FLT_INVALID_OPERATION : return "Invalid floating point operation"
        case EXCEPTION_ILLEGAL_INSTRUCTION   : return "Illegal instruction"
        case EXCEPTION_INT_DIVIDE_BY_ZERO    : return "Integer division by zero"
        case EXCEPTION_INT_OVERFLOW          : return "Integer overflow"
        case EXCEPTION_PRIV_INSTRUCTION      : return "Privileged instruction"
        case EXCEPTION_STACK_OVERFLOW        : return "Stack overflow"
        case EXCEPTION_IN_PAGE_ERROR         : return "Page error"
        case EXCEPTION_NONCONTINUABLE_EXCEPTION : return "Noncontinuable exception"
        case &hE06D7363                      : return "C++ exception"
    end select
    return "Exception &h" & hex(code)
end function

' ==========================================================================================
' Stop handling. Runs on the worker thread; fills the snapshot, notifies the UI, then blocks
' until the UI answers with a run mode.
' ==========================================================================================
private sub ReportStopAndWait( byval reason as DBG_STOPREASON, byval dwThreadId as DWORD, byval addr as ulongint )
    gDbgStop.reason    = reason
    gDbgStop.addr      = addr
    gDbgStop.threadId  = dwThreadId
    gDbgStop.lineIndex = DebugInfo_LineFromAddr( cast(longint, addr) )
    gDbgStop.procIndex = DebugInfo_ProcFromAddr( cast(longint, addr) )
    gDbgStop.srcIndex  = -1
    gDbgStop.lineNum   = 0
    if gDbgStop.lineIndex >= 0 then
        gDbgStop.srcIndex = gDbgLine(gDbgStop.lineIndex).srcIndex
        gDbgStop.lineNum  = gDbgLine(gDbgStop.lineIndex).lineNum
    end if

    dim as DBG_REGS regsNow
    if GetRegs( dwThreadId, regsNow ) then gDbgStop.stopSP = regsNow.sp

    BuildCallStack( dwThreadId )

    gDbgSession.isRunning = false
    PostMessage( gDbgSession.hNotifyWnd, MSG_USER_DEBUG_STOPPED, cast(WPARAM, reason), 0 )

    ' Block until the UI thread calls DebugProc_Resume. The debuggee is frozen for the whole
    ' of this wait, which is what lets the UI read its memory with no synchronisation.
    WaitForSingleObject( gDbgSession.hResumeEvent, INFINITE )
    gDbgSession.isRunning = true
end sub

' Decides which traps are armed for the mode the user chose, then continues.
private sub ApplyRunMode( byval dwThreadId as DWORD, byval atAddr as ulongint )
    gDbgSession.runMode = gDbgSession.pendingMode

    select case gDbgSession.runMode
        case DBG_RUN_STEPINTO, DBG_RUN_STEPOVER, DBG_RUN_STEPOUT
            ArmLineTraps()
        case else
            DisarmLineTraps()
            ArmUserBreakpoints()
    end select

    ' If the address we are leaving still carries a trap, it has to be stepped over first or
    ' the debuggee traps again immediately on the same instruction.
    StepOverTrap( dwThreadId, atAddr )
end sub

' ==========================================================================================
' The worker thread: CreateProcess plus the debug event loop. These cannot be separated --
' Windows delivers debug events only to the thread that started the session.
' ==========================================================================================
private sub DebugWorker( byval userdata as any ptr )
    dim as PROCESS_INFORMATION pi
    dim as STARTUPINFOW si
    si.cb = sizeof(STARTUPINFOW)

    dim as wstring * 32768 wCmd
    wCmd = """" & gDbgSession.exePath & """"
    if len(gDbgSession.cmdLine) then wCmd = wCmd & " " & gDbgSession.cmdLine

    dim as wstring * 4096 wDir
    dim as long bs = instrrev( gDbgSession.exePath, "\" )
    if bs > 0 then wDir = left( gDbgSession.exePath, bs - 1 )

    ' CREATE_NEW_CONSOLE: the debuggee gets its own console, so INPUT, colour and cursor
    ' positioning behave exactly as they do when it is run normally. Capturing its output
    ' into a pane instead would break every console-mode program.
    dim as DWORD flags = DEBUG_PROCESS or DEBUG_ONLY_THIS_PROCESS or CREATE_NEW_CONSOLE
    if CreateProcessW( null, @wCmd, null, null, false, flags, null, _
                       iif(len(wDir), @wDir, null), @si, @pi ) = 0 then
        gDbgSession.lastError = "CreateProcess failed, error " & str(GetLastError())
        gDbgSession.isActive = false
        PostMessage( gDbgSession.hNotifyWnd, MSG_USER_DEBUG_FAILED, 0, 0 )
        exit sub
    end if

    gDbgSession.hProcess    = pi.hProcess
    gDbgSession.hMainThread = pi.hThread
    gDbgSession.dwProcessId = pi.dwProcessId

    dim as BOOL bWow = false
    IsWow64Process( pi.hProcess, @bWow )
    gDbgSession.isWow64 = (bWow <> 0)

    dim as DEBUG_EVENT ev
    dim as DWORD contStatus = DBG_CONTINUE
    dim as boolean bRunning = true

    do while bRunning
        if WaitForDebugEvent( @ev, INFINITE ) = 0 then exit do
        contStatus = DBG_CONTINUE

        select case ev.dwDebugEventCode

            case CREATE_PROCESS_DEBUG_EVENT
                ThreadAdd( ev.dwThreadId, ev.u.CreateProcessInfo.hThread )
                gDbgSession.loadBase = cast(ulongint, ev.u.CreateProcessInfo.lpBaseOfImage)
                ' Load the debug tables against the address the image ACTUALLY landed at, so
                ' every recorded address is already correct if the target is relocated.
                DebugInfo_Load( gDbgSession.exePath, gDbgSession.loadBase )
                if gDbgLineCount > 0 then
                    redim gDbgLineOrig( gDbgLineCount - 1 )
                    redim gDbgLineArmed( gDbgLineCount - 1 )
                end if
                ' Resolve every breakpoint the user set before the session started.
                for b as long = 0 to gDbgBpCount - 1
                    gDbgBp(b).addr = cast(ulongint, DebugInfo_AddrFromLine( gDbgBp(b).srcIndex, gDbgBp(b).lineNum ))
                next
                if ev.u.CreateProcessInfo.hFile then CloseHandle( ev.u.CreateProcessInfo.hFile )

            case CREATE_THREAD_DEBUG_EVENT
                ThreadAdd( ev.dwThreadId, ev.u.CreateThread.hThread )

            case EXIT_THREAD_DEBUG_EVENT
                ThreadRemove( ev.dwThreadId )

            case LOAD_DLL_DEBUG_EVENT
                if ev.u.LoadDll.hFile then CloseHandle( ev.u.LoadDll.hFile )

            case EXIT_PROCESS_DEBUG_EVENT
                gDbgStop.exitCode = ev.u.ExitProcess.dwExitCode
                bRunning = false

            case EXCEPTION_DEBUG_EVENT
                dim as DWORD code = ev.u.Exception.ExceptionRecord.ExceptionCode
                dim as ulongint exAddr = cast(ulongint, ev.u.Exception.ExceptionRecord.ExceptionAddress)

                select case code

                    case EXCEPTION_BREAKPOINT, STATUS_WX86_BREAKPOINT
                        ' A breakpoint exception is only OURS if we armed a trap at exactly
                        ' this address. Anything else -- the loader breakpoint, and for a
                        ' WOW64 target the SECOND one the 32-bit ntdll raises, or an INT3
                        ' the program itself contains -- is passed straight through. Keying
                        ' this off a "first breakpoint" flag instead leaves a 32-bit target
                        ' spinning on ntdll's own trap forever, because there is nothing
                        ' there for us to restore and step over.
                        dim as long liOwn = DebugInfo_LineFromAddr( cast(longint, exAddr) )
                        dim as boolean bOurTrap = false
                        if liOwn >= 0 then
                            if (gDbgLine(liOwn).addr = cast(longint, exAddr)) andalso gDbgLineArmed(liOwn) then
                                bOurTrap = true
                            end if
                        end if

                        if bOurTrap = false then
                            if gDbgSession.sawEntryBp = false then
                                ' The first foreign breakpoint is the loader's, and it is
                                ' the earliest point at which the image is mapped and the
                                ' line traps can be written.
                                gDbgSession.sawEntryBp = true
                                gDbgSession.pendingMode = DBG_RUN_STEPINTO
                                ArmLineTraps()
                            end if
                        else
                            dim as long li = liOwn
                            dim as DBG_STOPREASON why = DBG_STOP_STEP

                            if gDbgSession.pauseRequested then
                                gDbgSession.pauseRequested = false
                                why = DBG_STOP_USERBREAK
                            elseif (gDbgSession.tempBpAddr <> 0) andalso (exAddr = gDbgSession.tempBpAddr) then
                                why = DBG_STOP_TEMPBP
                                gDbgSession.tempBpAddr = 0
                            elseif li >= 0 then
                                if DebugProc_FindBreakpoint( gDbgLine(li).srcIndex, gDbgLine(li).lineNum ) >= 0 then
                                    why = DBG_STOP_BREAKPOINT
                                end if
                            end if

                            ' Step Over and Step Out must not stop in a deeper frame. If this
                            ' trap is below the frame the step started from, resume silently.
                            dim as boolean bSkip = false
                            if (why = DBG_STOP_STEP) andalso (li >= 0) then
                                select case gDbgSession.runMode
                                    case DBG_RUN_STEPOVER, DBG_RUN_STEPOUT
                                        dim as DBG_REGS r
                                        if GetRegs( ev.dwThreadId, r ) then
                                            ' Depth is measured with the STACK pointer, not
                                            ' the frame pointer. At a callee's entry the
                                            ' prologue has not run yet, so RBP still holds
                                            ' the CALLER's frame and cannot distinguish
                                            ' "one level deeper" from "same function". RSP
                                            ' is already lower by then -- the CALL pushed a
                                            ' return address -- so it is reliable at every
                                            ' instruction, prologue included.
                                            if r.sp < gDbgSession.stepSP then bSkip = true
                                            if (gDbgSession.runMode = DBG_RUN_STEPOUT) andalso _
                                               (r.sp <= gDbgSession.stepSP) then bSkip = true
                                        end if
                                end select
                            end if

                            if bSkip then
                                StepOverTrap( ev.dwThreadId, exAddr )
                            else
                                dim as DBG_REGS r
                                if GetRegs( ev.dwThreadId, r ) then
                                    r.pc = exAddr
                                    SetRegs( ev.dwThreadId, r )
                                end if
                                ReportStopAndWait( why, ev.dwThreadId, exAddr )
                                if gDbgSession.stopRequested then
                                    bRunning = false
                                else
                                    ApplyRunMode( ev.dwThreadId, exAddr )
                                end if
                            end if
                        end if

                    case EXCEPTION_SINGLE_STEP, STATUS_WX86_SINGLE_STEP
                        ' The one instruction after a StepOverTrap has retired; put the trap
                        ' back. The trap flag clears itself.
                        if gDbgPendingRearm <> 0 then
                            dim as long li = DebugInfo_LineFromAddr( cast(longint, gDbgPendingRearm) )
                            dim as boolean bWant = gDbgSession.ccArmed
                            if (bWant = false) andalso (li >= 0) then
                                ' In a RUN mode only the user's own breakpoints stay armed.
                                if DebugProc_FindBreakpoint( gDbgLine(li).srcIndex, gDbgLine(li).lineNum ) >= 0 then bWant = true
                                if gDbgSession.tempBpAddr = gDbgPendingRearm then bWant = true
                            end if
                            if bWant andalso (li >= 0) then
                                dim as ubyte cc = DBG_INT3
                                if DebugProc_WriteMem( gDbgPendingRearm, @cc, 1 ) then gDbgLineArmed(li) = true
                            end if
                            gDbgPendingRearm = 0
                        end if

                    case else
                        ' A real fault. Report it; the UI decides whether to continue.
                        if ev.u.Exception.dwFirstChance <> 0 then
                            gDbgStop.excepCode    = code
                            gDbgStop.excepAddr    = exAddr
                            gDbgStop.excepText    = DebugProc_ExceptionText( code )
                            gDbgStop.excepIsWrite = false
                            gDbgStop.excepTarget  = 0
                            if code = EXCEPTION_ACCESS_VIOLATION then
                                if ev.u.Exception.ExceptionRecord.NumberParameters >= 2 then
                                    gDbgStop.excepIsWrite = (ev.u.Exception.ExceptionRecord.ExceptionInformation(0) <> 0)
                                    gDbgStop.excepTarget  = cast(ulongint, ev.u.Exception.ExceptionRecord.ExceptionInformation(1))
                                end if
                            end if
                            ReportStopAndWait( DBG_STOP_EXCEPTION, ev.dwThreadId, exAddr )
                            if gDbgSession.stopRequested then
                                bRunning = false
                            else
                                ApplyRunMode( ev.dwThreadId, exAddr )
                            end if
                        end if
                        contStatus = DBG_EXCEPTION_NOT_HANDLED
                end select
        end select

        if bRunning = false then exit do
        ContinueDebugEvent( ev.dwProcessId, ev.dwThreadId, contStatus )
    loop

    ' The debuggee is gone (or we killed it). Everything below runs once.
    if gDbgSession.hProcess then
        ContinueDebugEvent( ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE )
    end if

    dim as DWORD code = gDbgStop.exitCode
    gDbgSession.isActive  = false
    gDbgSession.isRunning = false
    gDbgSession.ccArmed   = false
    gDbgThreadCount = 0

    if gDbgSession.hProcess    then CloseHandle( gDbgSession.hProcess )    : gDbgSession.hProcess = 0
    if gDbgSession.hMainThread then CloseHandle( gDbgSession.hMainThread ) : gDbgSession.hMainThread = 0

    PostMessage( gDbgSession.hNotifyWnd, MSG_USER_DEBUG_EXITED, cast(WPARAM, code), 0 )
end sub

' ==========================================================================================
' Public control surface (UI thread)
' ==========================================================================================
function DebugProc_Start( byref imgPath as string, byref cmdLine as string, byval hNotify as HWND ) as boolean
    if gDbgSession.isActive then return false

    gDbgSession = type<DBG_SESSION>()
    gDbgSession.exePath    = imgPath
    gDbgSession.cmdLine    = cmdLine
    gDbgSession.hNotifyWnd = hNotify
    gDbgSession.isActive   = true
    gDbgSession.isRunning  = true
    gDbgSession.runMode    = DBG_RUN_STEPINTO
    gDbgSession.pendingMode = DBG_RUN_STEPINTO
    gDbgSession.stepProcIndex = -1

    ' Auto-reset: every Resume releases exactly one wait.
    gDbgSession.hResumeEvent = CreateEvent( null, false, false, null )
    if gDbgSession.hResumeEvent = 0 then
        gDbgSession.isActive = false
        gDbgSession.lastError = "CreateEvent failed"
        return false
    end if

    gDbgSession.hWorker = ThreadCreate( @DebugWorker, 0 )
    if gDbgSession.hWorker = 0 then
        CloseHandle( gDbgSession.hResumeEvent )
        gDbgSession.hResumeEvent = 0
        gDbgSession.isActive = false
        gDbgSession.lastError = "could not start the debug thread"
        return false
    end if
    return true
end function

sub DebugProc_Resume( byval mode as DBG_RUNMODE )
    if gDbgSession.isActive = false then exit sub
    if gDbgSession.isRunning then exit sub

    gDbgSession.pendingMode = mode

    ' Step Over and Step Out are relative to the frame we are leaving, so it is captured
    ' here rather than when the trap fires -- by then we are already somewhere else.
    select case mode
        case DBG_RUN_STEPOVER, DBG_RUN_STEPOUT
            gDbgSession.stepSP = gDbgStop.stopSP
        case else
            gDbgSession.stepSP = 0
    end select

    SetEvent( gDbgSession.hResumeEvent )
end sub

' Acts on a RUNNING debuggee, from the UI thread. Re-arming the line traps is what makes the
' next source line stop -- see the pause caveat in the .bi header.
sub DebugProc_Pause()
    if gDbgSession.isActive = false then exit sub
    if gDbgSession.isRunning = false then exit sub
    gDbgSession.pauseRequested = true
    gDbgSession.ccArmed = false
    ArmLineTraps()
end sub

sub DebugProc_Stop()
    if gDbgSession.isActive = false then exit sub
    gDbgSession.stopRequested = true
    if gDbgSession.hProcess then TerminateProcess( gDbgSession.hProcess, 0 )
    ' If the worker is parked waiting for a run mode, release it so it can unwind.
    if gDbgSession.isRunning = false then SetEvent( gDbgSession.hResumeEvent )
end sub

function DebugProc_IsActive() as boolean
    return gDbgSession.isActive
end function

function DebugProc_IsRunning() as boolean
    return gDbgSession.isRunning
end function
