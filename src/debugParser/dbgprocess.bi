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
' dbgprocess - launches the debuggee and owns every interaction with it.
'
' Pairs with dbginfo, which supplies the line and symbol tables this drives off.
'
' ------------------------------------------------------------------------------------------
' THE THREADING MODEL, WHICH IS FORCED RATHER THAN CHOSEN
'
' Windows delivers debug events only to the thread that started the session. CreateProcess
' and the WaitForDebugEvent loop therefore have to live on the SAME worker thread -- they
' cannot be split, and the loop cannot run on the UI thread because it blocks.
'
'   UI thread                          worker thread
'   ---------                          -------------
'   DebugProc_Start                    CreateProcess( DEBUG_PROCESS )
'     ThreadCreate  -------------->    loop: WaitForDebugEvent
'                                        on a stop:
'                                          fill gDbgStop, build the call stack
'                                          PostMessage( MSG_USER_DEBUG_STOPPED )
'                                          WaitForSingleObject( hResumeEvent )  <-- blocks
'   ...renders from gDbgStop
'   ...reads debuggee memory directly
'   DebugProc_Resume( mode )  ------>    wakes, applies the mode, ContinueDebugEvent
'
' WHILE STOPPED THE DEBUGGEE IS FROZEN AND THE WORKER IS BLOCKED, so the UI thread may call
' DebugProc_ReadMem directly with no lock and no request/response plumbing. That is what
' makes lazy UDT expansion and hover datatips cheap, and it is the reason this module hands
' out a snapshot plus a memory reader rather than a message protocol.
'
' The two exceptions, both deliberate, are DebugProc_Pause and DebugProc_Stop: they act on a
' RUNNING debuggee from the UI thread. Neither touches gDbgStop, and both rely only on
' Write/TerminateProcess being safe against a running target.
'
' ------------------------------------------------------------------------------------------
' THE STEPPING MODEL
'
' An INT3 (0xCC) is written over the first byte of EVERY executable line, so a step is just
' "continue until the next trap". Continue restores all of them and re-arms only the user's
' breakpoints. This is FBdebugger's design and it is chosen because it is proven against this
' exact debug format -- correct across reordered code with no reliance on unwind data.
'
' The cost is stated rather than hidden: PAUSE LANDS AT THE NEXT SOURCE LINE. It is
' implemented by re-arming the line traps, so it cannot interrupt a Sleep, a blocking read,
' or a tight loop inside the runtime. Stop works at any time; pause does not.
'
' Resuming from a line that carries a trap needs the standard dance -- restore the original
' byte, rewind the instruction pointer over the INT3, set the trap flag, single-step one
' instruction, then re-arm. DebugProc_StepOverTrap owns that and nothing else may.
'
' ------------------------------------------------------------------------------------------
' CROSS-BITNESS
'
' tiko.exe is 64-bit and may debug either a 64-bit or a 32-bit target. Register access goes
' through DBG_REGS and Get/SetRegs, which branch on IsWow64Process; nothing above that layer
' knows which it is. ReadProcessMemory is unaffected. FBdebugger refuses this case outright.
'
' x64 CONTEXT contains XMM registers and must be 16-byte aligned. A plain local is NOT
' reliably aligned by fbc -- FBdebugger pads with a dummy variable and hopes; this module
' allocates and aligns explicitly, because the failure is silent corruption of whatever
' shares the stack slot.
' ==========================================================================================

#pragma once

' REQUIRES _WIN32_WINNT >= &h0600 to have been defined before windows.bi was included --
' Wow64GetThreadContext / Wow64SetThreadContext sit behind that guard in winbase.bi, and
' without it the 32-bit target path fails to compile rather than failing at run time.
' tiko.bas sets &h0602 at its very top; a standalone host must do the same.
#include once "debugParser.bi"
#include once "dbginfo.bi"

' The notification messages are declared ONCE, in the public header, so the value a host
' waits on and the value this module posts cannot drift apart.

' A 32-bit process running under WOW64 reports its breakpoint and single-step exceptions
' with DIFFERENT codes from a native one -- measured, not assumed: a 32-bit target reported
' &h4000001F where the 64-bit path reports EXCEPTION_BREAKPOINT. FreeBASIC's headers do not
' declare these, and without them every trap in a 32-bit target is treated as a crash.
const as DWORD STATUS_WX86_SINGLE_STEP = &h4000001E
const as DWORD STATUS_WX86_BREAKPOINT  = &h4000001F

enum DBG_RUNMODE
    DBG_RUN_STEPINTO = 0       ' stop at the next executable line, wherever it is
    DBG_RUN_STEPOVER           ' as above, but skip anything deeper than the current frame
    DBG_RUN_STEPOUT            ' run until the current frame returns
    DBG_RUN_CONTINUE           ' only user breakpoints are armed
    DBG_RUN_TOCURSOR           ' continue, plus one temporary breakpoint
end enum

enum DBG_STOPREASON
    DBG_STOP_NONE = 0
    DBG_STOP_ENTRY             ' the initial loader breakpoint, before any user code
    DBG_STOP_STEP              ' a line trap during a step
    DBG_STOP_BREAKPOINT        ' a line trap carrying a user breakpoint
    DBG_STOP_TEMPBP            ' the run-to-cursor / step-over target
    DBG_STOP_USERBREAK         ' the user asked to pause
    DBG_STOP_EXCEPTION         ' a fault in the debuggee
end enum

const as long DBG_MAXFRAMES = 128

type DBG_FRAME
    addrPC     as ulongint     ' instruction pointer for this frame
    frameBase  as ulongint     ' rbp/ebp
    retAddr    as ulongint
    procIndex  as long         ' into gDbgProc, -1 if outside known code
    lineIndex  as long         ' into gDbgLine, -1 if unknown
end type

type DBG_STOPINFO
    reason      as DBG_STOPREASON
    addr        as ulongint
    threadId    as DWORD
    procIndex   as long
    lineIndex   as long
    srcIndex    as long
    lineNum     as long
    stopSP      as ulongint    ' stack pointer at the stop; the depth reference for stepping
    exitCode    as DWORD
    excepCode   as DWORD
    excepAddr   as ulongint
    excepIsWrite as boolean
    excepTarget as ulongint
    excepText   as string
    frameCount  as long
    frames(0 to DBG_MAXFRAMES - 1) as DBG_FRAME
end type

' A neutral register view, so nothing above Get/SetRegs cares about bitness.
type DBG_REGS
    pc    as ulongint          ' rip / eip
    fp    as ulongint          ' rbp / ebp
    sp    as ulongint          ' rsp / esp
    flags as ulongint          ' eflags
end type

type DBG_BREAKPOINT
    srcIndex as long
    lineNum  as long
    addr     as ulongint
    enabled  as boolean
end type

type DBG_SESSION
    isActive      as boolean
    isRunning     as boolean       ' debuggee executing rather than stopped
    hProcess      as HANDLE
    hMainThread   as HANDLE
    dwProcessId   as DWORD
    hWorker       as any ptr
    hResumeEvent  as HANDLE
    hNotifyWnd    as HWND
    loadBase      as ulongint
    isWow64       as boolean       ' a 32-bit target under a 64-bit debugger
    exePath       as string
    cmdLine       as string
    lastError     as string

    runMode       as DBG_RUNMODE
    pendingMode   as DBG_RUNMODE
    stopRequested as boolean
    pauseRequested as boolean
    sawEntryBp    as boolean       ' the loader breakpoint has been consumed
    ccArmed       as boolean       ' the line traps are currently installed
    tempBpAddr    as ulongint      ' run-to-cursor / step-over target, 0 if none
    stepSP        as ulongint      ' stack pointer to compare against for over/out
    stepProcIndex as long
end type

extern gDbgSession as DBG_SESSION
extern gDbgStop    as DBG_STOPINFO
extern gDbgBp()    as DBG_BREAKPOINT
extern gDbgBpCount as long

declare function DebugProc_Start        ( byref imgPath as string, byref cmdLine as string, byval hNotify as HWND ) as boolean
declare sub      DebugProc_Stop         ()
declare sub      DebugProc_Resume       ( byval mode as DBG_RUNMODE )
declare sub      DebugProc_Pause        ()
declare function DebugProc_IsActive     () as boolean
declare function DebugProc_IsRunning    () as boolean

declare function DebugProc_ReadMem      ( byval addr as ulongint, byval dest as any ptr, byval nBytes as long ) as boolean
declare function DebugProc_WriteMem     ( byval addr as ulongint, byval src as any ptr, byval nBytes as long ) as boolean
declare function DebugProc_ReadPtr      ( byval addr as ulongint, byref outVal as ulongint ) as boolean
declare function DebugProc_PointerSize  () as long

declare function DebugProc_AddBreakpoint   ( byval srcIndex as long, byval lineNum as long ) as long
declare function DebugProc_RemoveBreakpoint( byval srcIndex as long, byval lineNum as long ) as boolean
declare sub      DebugProc_ClearBreakpoints()
declare function DebugProc_FindBreakpoint  ( byval srcIndex as long, byval lineNum as long ) as long
declare sub      DebugProc_SetRunToCursor  ( byval srcIndex as long, byval lineNum as long )

declare function DebugProc_ExceptionText( byval code as DWORD ) as string
