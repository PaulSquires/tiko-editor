'' debugParser public interface - the debug engine for FreeBASIC executables.
''
'' This is the ONLY header a host application includes.
''
'' ==========================================================================================
'' WHAT THIS IS
''
'' fbc embeds debug information when built with -g and the gas or gas64 backend. That format
'' is a stabs derivative private to FreeBASIC -- fbc's own changelog describes it as emitted
'' "for FBdebugger only", and gdb cannot read it. This DLL reads it, and drives a debuggee
'' against it: breakpoints, stepping, a call stack, and variable values.
''
'' A debug build must therefore use -gen gas64 (64-bit) or -gen gas (32-bit). A -gen gcc
'' build emits DWARF instead and is rejected with DBGP_ERR_NODEBUGINFO.
''
'' ==========================================================================================
'' HOW THIS DIFFERS FROM fbcParser, WHICH IT OTHERWISE FOLLOWS
''
'' fbcParser is a one-shot scan: it returns flat arrays plus a string pool and the host walks
'' them. A debugger is not one-shot -- it owns a live process whose values change at every
'' stop -- so the shape here is an ACCESSOR API over state the DLL keeps, rather than a
'' snapshot the host walks.
''
'' The consequences are deliberate:
''   - Nothing returned needs freeing. Every struct is fixed-size with inline wstring fields,
''     so there is no per-item ownership to get wrong.
''   - DBGP_NODE carries its own identity (address, type, shape). The host passes a node back
''     to ask for its children, so there is no handle table and no lifetime to track.
''   - One session at a time, and the session belongs to the process that started it.
''
'' ==========================================================================================
'' THREADING CONTRACT -- read this before calling anything
''
'' Windows delivers debug events only to the thread that started the session, so the DLL runs
'' its event loop on a worker thread it creates. On every stop it posts MSG_DBGP_STOPPED to
'' the window the host supplied and then BLOCKS until the host calls debugparser_resume.
''
'' While stopped, the debuggee is frozen. That is what makes every query below safe and cheap
'' from the host's UI thread with no locking: reading a variable, expanding a UDT, walking the
'' call stack. It is also why the host MUST eventually call debugparser_resume or
'' debugparser_stop -- until it does, the debuggee stays suspended.
''
'' Call every function except the notifications from ONE thread (the host's UI thread).
''
'' ==========================================================================================
'' PAUSE IS NOT INSTANT, AND THIS IS INHERENT
''
'' Stepping is implemented by writing an INT3 over every executable line, so pause works by
'' re-arming those traps: it takes effect when the debuggee next reaches a source line. It
'' cannot interrupt a Sleep, a blocking read, or a tight loop inside the runtime.
'' debugparser_stop works at any time; debugparser_pause does not.

#ifndef __DEBUGPARSER_BI__
#define __DEBUGPARSER_BI__

const DBGP_VERSION = 1

'' Inline buffer sizes. Chosen so every struct below is fixed-size and copyable.
const DBGP_MAXNAME  = 256
const DBGP_MAXVALUE = 512
const DBGP_MAXPATH  = 520

'' Return codes. Zero is success; everything else is a failure the host can report.
const DBGP_OK               =  0
const DBGP_ERR_BADARG       = -1
const DBGP_ERR_NOTLOADED    = -2   '' no debug information loaded
const DBGP_ERR_NODEBUGINFO  = -3   '' the exe carries none: built without -g, or with -gen gcc
const DBGP_ERR_BADIMAGE     = -4   '' not a PE image
const DBGP_ERR_IOERROR      = -5
const DBGP_ERR_BUSY         = -6   '' a session is already running
const DBGP_ERR_NOSESSION    = -7
const DBGP_ERR_LAUNCH       = -8   '' CreateProcess failed
const DBGP_ERR_NOTFOUND     = -9   '' name/line/frame does not resolve
const DBGP_ERR_UNAUTHORIZED = -10  '' the host process is not tiko.exe; the engine refuses to run

'' Posted to the host's notify window. wParam carries the detail noted.
'' Chosen well clear of tiko's own MSG_USER_* block.
const MSG_DBGP_STOPPED = &h0400 + 810   '' wParam = DBGP_STOPREASON
const MSG_DBGP_EXITED  = &h0400 + 811   '' wParam = the debuggee's exit code
const MSG_DBGP_FAILED  = &h0400 + 812   '' launch failed; see debugparser_get_error

enum DBGP_RUNMODE
	DBGP_RUN_STEPINTO = 0   '' stop at the next executable line, wherever it is
	DBGP_RUN_STEPOVER       '' as above, but do not descend into calls
	DBGP_RUN_STEPOUT        '' run until the current frame returns
	DBGP_RUN_CONTINUE       '' only user breakpoints are armed
	DBGP_RUN_TOCURSOR       '' continue, plus one temporary breakpoint
end enum

enum DBGP_STOPREASON
	DBGP_STOP_NONE = 0
	DBGP_STOP_ENTRY         '' the loader breakpoint, before any user code
	DBGP_STOP_STEP
	DBGP_STOP_BREAKPOINT
	DBGP_STOP_TEMPBP        '' the run-to-cursor / step-over target
	DBGP_STOP_USERBREAK     '' the host asked to pause
	DBGP_STOP_EXCEPTION     '' the debuggee faulted
end enum

'' Variable storage class. Only relevant if the host wants to group or annotate.
enum DBGP_SCOPE
	DBGP_SCOPE_LOCAL = 1
	DBGP_SCOPE_SHARED
	DBGP_SCOPE_STATIC
	DBGP_SCOPE_PARAMREF
	DBGP_SCOPE_PARAMVAL
	DBGP_SCOPE_COMMON
end enum

'' Where execution is, filled by debugparser_get_stop after MSG_DBGP_STOPPED.
type DBGP_STOPINFO
	reason       as long          '' DBGP_STOPREASON
	threadId     as ulong
	addr         as ulongint
	srcIndex     as long          '' -1 when the address is outside known code
	lineNum      as long
	frameCount   as long
	exitCode     as ulong
	excepCode    as ulong         '' 0 unless reason is DBGP_STOP_EXCEPTION
	excepAddr    as ulongint
	excepIsWrite as long
	excepTarget  as ulongint
	wszProcName  as wstring * DBGP_MAXNAME
	wszSrcPath   as wstring * DBGP_MAXPATH
	wszExcepText as wstring * DBGP_MAXNAME
end type

'' One call-stack frame. Frame 0 is where execution is; higher indices are callers.
'' A caller's line is the line CONTAINING the call, not the one it returns to.
type DBGP_FRAME
	addrPC      as ulongint
	frameBase   as ulongint
	srcIndex    as long
	lineNum     as long
	wszProcName as wstring * DBGP_MAXNAME
	wszSrcPath  as wstring * DBGP_MAXPATH
end type

'' One row in a variables view.
''
'' The first block is this node's IDENTITY: the host passes the struct back to
'' debugparser_child_count / debugparser_child_node to expand it. Treat those fields as
'' opaque -- they are not a stable ABI for anything except being handed straight back.
type DBGP_NODE
	addr        as ulongint
	typeId      as long
	descTypeId  as long
	ptrDepth    as long
	arrayIndex  as long
	declBytes   as longint
	bitOffset   as long
	bitLen      as long
	scope       as long           '' DBGP_SCOPE
	isValid     as long           '' 0 when the address could not be read
	hasChildren as long
	wszName     as wstring * DBGP_MAXNAME
	wszType     as wstring * DBGP_MAXNAME
	wszValue    as wstring * DBGP_MAXVALUE
end type

'' The shape of an array variable.
''
'' Ask for this on any node whose hasChildren is set and you want to know whether it is a UDT
'' or an array -- debugparser_array_info answers non-zero only for an array. It covers both
'' kinds: a fixed array's bounds come from the debug information, a REDIM array's from its
'' run-time descriptor, so for the dynamic case the answer is only valid while the debuggee is
'' stopped and can be different at the next stop.
''
'' `total` is the product of every extent and is exactly what debugparser_child_count reports,
'' so children are addressed by a single linear index in ROW-MAJOR order (the last subscript
'' varies fastest, matching memory). There is no cap: a ten-million-element array reports ten
'' million children, and deciding how to present that is the host's business.
type DBGP_ARRAYINFO
	dims      as long                       '' 1..DBGP_MAXDIM
	lo(0 to 7) as longint
	hi(0 to 7) as longint
	elemSize  as longint                    '' bytes per element
	total     as longint                    '' product of every extent
	isDynamic as long                       '' non-zero for a REDIM array
end type

const DBGP_MAXDIM = 8

#ifdef DEBUGPARSER_BUILDING_DLL
	#define DBGP_DECL(fn) declare
#endif

#ifdef DEBUGPARSER_BUILDING_DLL

	'' ---- static debug information ----
	declare function debugparser_load( byval wszExePath as wstring ptr, byval loadBase as ulongint ) as long
	declare sub      debugparser_unload()
	declare function debugparser_is_loaded() as long
	declare sub      debugparser_get_error( byval wszBuf as wstring ptr, byval cchBuf as long )
	declare function debugparser_target_is_64bit() as long
	declare sub      debugparser_get_compiler( byval wszBuf as wstring ptr, byval cchBuf as long )
	declare function debugparser_line_cap_hit() as long

	declare function debugparser_source_count() as long
	declare function debugparser_source_path( byval srcIndex as long, byval wszBuf as wstring ptr, byval cchBuf as long ) as long
	declare function debugparser_source_index( byval wszFileName as wstring ptr ) as long
	declare function debugparser_is_executable_line( byval srcIndex as long, byval lineNum as long ) as long
	declare function debugparser_next_executable_line( byval srcIndex as long, byval lineNum as long ) as long

	'' ---- session ----
	declare function debugparser_start( byval wszExePath as wstring ptr, byval wszCmdLine as wstring ptr, byval hNotifyWnd as any ptr ) as long
	declare sub      debugparser_stop()
	declare function debugparser_resume( byval mode as long ) as long
	declare sub      debugparser_pause()
	declare function debugparser_is_active() as long
	declare function debugparser_is_running() as long

	declare function debugparser_get_stop( byval pStop as DBGP_STOPINFO ptr ) as long
	declare function debugparser_get_frame( byval frameIndex as long, byval pFrame as DBGP_FRAME ptr ) as long

	'' ---- breakpoints ----
	declare function debugparser_add_breakpoint( byval srcIndex as long, byval lineNum as long ) as long
	declare function debugparser_remove_breakpoint( byval srcIndex as long, byval lineNum as long ) as long
	declare sub      debugparser_clear_breakpoints()
	declare function debugparser_set_run_to_cursor( byval srcIndex as long, byval lineNum as long ) as long

	'' ---- variables ----
	declare function debugparser_local_count( byval frameIndex as long ) as long
	declare function debugparser_local_node( byval frameIndex as long, byval index as long, byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_global_count() as long
	declare function debugparser_global_node( byval index as long, byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_child_count( byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_child_node( byval pNode as DBGP_NODE ptr, byval index as long, byval pOut as DBGP_NODE ptr ) as long
	declare function debugparser_evaluate( byval wszExpr as wstring ptr, byval frameIndex as long, byval pNode as DBGP_NODE ptr ) as long
		declare function debugparser_array_info( byval pNode as DBGP_NODE ptr, byval pOut as DBGP_ARRAYINFO ptr ) as long

#else

	declare function debugparser_load lib "debugParser" alias "DEBUGPARSER_LOAD" ( byval wszExePath as wstring ptr, byval loadBase as ulongint ) as long
	declare sub      debugparser_unload lib "debugParser" alias "DEBUGPARSER_UNLOAD" ()
	declare function debugparser_is_loaded lib "debugParser" alias "DEBUGPARSER_IS_LOADED" () as long
	declare sub      debugparser_get_error lib "debugParser" alias "DEBUGPARSER_GET_ERROR" ( byval wszBuf as wstring ptr, byval cchBuf as long )
	declare function debugparser_target_is_64bit lib "debugParser" alias "DEBUGPARSER_TARGET_IS_64BIT" () as long
	declare sub      debugparser_get_compiler lib "debugParser" alias "DEBUGPARSER_GET_COMPILER" ( byval wszBuf as wstring ptr, byval cchBuf as long )
	declare function debugparser_line_cap_hit lib "debugParser" alias "DEBUGPARSER_LINE_CAP_HIT" () as long

	declare function debugparser_source_count lib "debugParser" alias "DEBUGPARSER_SOURCE_COUNT" () as long
	declare function debugparser_source_path lib "debugParser" alias "DEBUGPARSER_SOURCE_PATH" ( byval srcIndex as long, byval wszBuf as wstring ptr, byval cchBuf as long ) as long
	declare function debugparser_source_index lib "debugParser" alias "DEBUGPARSER_SOURCE_INDEX" ( byval wszFileName as wstring ptr ) as long
	declare function debugparser_is_executable_line lib "debugParser" alias "DEBUGPARSER_IS_EXECUTABLE_LINE" ( byval srcIndex as long, byval lineNum as long ) as long
	declare function debugparser_next_executable_line lib "debugParser" alias "DEBUGPARSER_NEXT_EXECUTABLE_LINE" ( byval srcIndex as long, byval lineNum as long ) as long

	declare function debugparser_start lib "debugParser" alias "DEBUGPARSER_START" ( byval wszExePath as wstring ptr, byval wszCmdLine as wstring ptr, byval hNotifyWnd as any ptr ) as long
	declare sub      debugparser_stop lib "debugParser" alias "DEBUGPARSER_STOP" ()
	declare function debugparser_resume lib "debugParser" alias "DEBUGPARSER_RESUME" ( byval mode as long ) as long
	declare sub      debugparser_pause lib "debugParser" alias "DEBUGPARSER_PAUSE" ()
	declare function debugparser_is_active lib "debugParser" alias "DEBUGPARSER_IS_ACTIVE" () as long
	declare function debugparser_is_running lib "debugParser" alias "DEBUGPARSER_IS_RUNNING" () as long

	declare function debugparser_get_stop lib "debugParser" alias "DEBUGPARSER_GET_STOP" ( byval pStop as DBGP_STOPINFO ptr ) as long
	declare function debugparser_get_frame lib "debugParser" alias "DEBUGPARSER_GET_FRAME" ( byval frameIndex as long, byval pFrame as DBGP_FRAME ptr ) as long

	declare function debugparser_add_breakpoint lib "debugParser" alias "DEBUGPARSER_ADD_BREAKPOINT" ( byval srcIndex as long, byval lineNum as long ) as long
	declare function debugparser_remove_breakpoint lib "debugParser" alias "DEBUGPARSER_REMOVE_BREAKPOINT" ( byval srcIndex as long, byval lineNum as long ) as long
	declare sub      debugparser_clear_breakpoints lib "debugParser" alias "DEBUGPARSER_CLEAR_BREAKPOINTS" ()
	declare function debugparser_set_run_to_cursor lib "debugParser" alias "DEBUGPARSER_SET_RUN_TO_CURSOR" ( byval srcIndex as long, byval lineNum as long ) as long

	declare function debugparser_local_count lib "debugParser" alias "DEBUGPARSER_LOCAL_COUNT" ( byval frameIndex as long ) as long
	declare function debugparser_local_node lib "debugParser" alias "DEBUGPARSER_LOCAL_NODE" ( byval frameIndex as long, byval index as long, byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_global_count lib "debugParser" alias "DEBUGPARSER_GLOBAL_COUNT" () as long
	declare function debugparser_global_node lib "debugParser" alias "DEBUGPARSER_GLOBAL_NODE" ( byval index as long, byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_child_count lib "debugParser" alias "DEBUGPARSER_CHILD_COUNT" ( byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_child_node lib "debugParser" alias "DEBUGPARSER_CHILD_NODE" ( byval pNode as DBGP_NODE ptr, byval index as long, byval pOut as DBGP_NODE ptr ) as long
	declare function debugparser_evaluate lib "debugParser" alias "DEBUGPARSER_EVALUATE" ( byval wszExpr as wstring ptr, byval frameIndex as long, byval pNode as DBGP_NODE ptr ) as long
	declare function debugparser_array_info lib "debugParser" alias "DEBUGPARSER_ARRAY_INFO" ( byval pNode as DBGP_NODE ptr, byval pOut as DBGP_ARRAYINFO ptr ) as long

#endif

#endif '' __DEBUGPARSER_BI__
