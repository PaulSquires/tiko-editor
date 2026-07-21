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

#pragma once

#define IDC_BTN_RUN             1001
#define IDC_BTN_CONTINUE        1002
#define IDC_BTN_BREAK           1003
#define IDC_BTN_STOP            1004
#define IDC_BTN_STEP_INTO       1005
#define IDC_BTN_STEP_OVER       1006
#define IDC_BTN_STEP_OUT        1007
#define IDC_FRMDEBUG_TXTOUTPUT  1008
#define IDC_FRMDEBUG_LVVARS     1009

#define IDM_LOAD_BREAKPOINT_ARRAY           2000
#define IDM_DELETE_CURRENT_LINE_BREAKPOINT  2001

type DebugWindowPosition
    bInitialized as boolean = false
    bMaximized   as boolean = false
    nLeft        as long
    nTop         as long
    nRight       as long
    nBottom      as long
end type
dim shared gDebugPos as DebugWindowPosition
        
enum DEBUG_BUTTONS explicit
    BUTTON_FIRST         = 0
    STOP_DEBUGGING       = 0
    CONTINUE_DEBUGGING   = 1
    STEP_OVER            = 2
    STEP_INTO            = 3
    STEP_OUT             = 4
    RUN_TO_CURSOR        = 5
    BUTTON_LAST          = RUN_TO_CURSOR
end enum

type DEBUG_BUTTONS_TYPE
    wszToolTip as DWSTRING
    wszIcon    as DWSTRING
    wszText    as DWSTRING
    rc         as RECT            ' client coordinates 
    nID        as long            ' id to invoke if clicked on
    isHot      as boolean
end type


type _GDBMessage as GDBMessage

type GDBMessage
    message   as string
    pNextNode as _GDBMessage ptr
end type

type VariableType
    VarName         as string
    VarNameDisplay  as string
    VarValue        as string
end type

type BreakpointType
    numberId        as long
    func            as string
    linenum         as long
end type


type GDBSession
    hProcess                  as HANDLE
    hThread                   as HANDLE
    hStdInWrite               as HANDLE
    hStdOutRead               as HANDLE
    hStdErrRead               as HANDLE
    dwProcessId               as DWORD
    initialized               as boolean
    hThreadMessages           as any ptr
    KillMessageThread         as boolean
    hThreadMutex              as any ptr

    current_file_name         as string
    current_function_name     as string
    current_function_linenum  as long
    variable_array(any)       as VariableType
    breakpoint_array(any)     as BreakpointType
    is_new_variables          as boolean 
    current_debug_action      as long               ' IDM_DEBUG_STEPOVER, IDM_DEBUG_STEPOUT, etc
    breakpoint_line_to_delete as long               ' line where breakpoint during toggle

    function_breakpoints      as string             ' function where breakpoints are temporarily disabled/enabled  
    disabled_breakpoints      as string             ' simply list of breakpoint numbers cached for subsequent enable breakpoints.
    
    ' Message fifo queue
    head                      as GDBMessage ptr
    tail                      as GDBMessage ptr
    count                     as long
end type

dim shared as GDBSession gdb_session

dim shared gDbgBtns(DEBUG_BUTTONS.BUTTON_LAST) as DEBUG_BUTTONS_TYPE

const as long DEBUG_BUTTON_IMAGE_WIDTH = 18

declare function gdb_dequeue_message() as string
declare function gdb_isMessageQueueEmpty() as boolean
declare function gdb_init( byval executable as string = "", byval cmdparams as string = "" ) as boolean
declare sub      gdb_threadListener( byval userdata as any ptr )
declare sub      gdb_close()
declare function gdb_send( byref cmd as string ) as boolean
declare function frmDebug_Show( byval hWndParent as HWND, byval executable as string, byval cmdparams as string ) as LRESULT
