'' fbcParser symbol collector - internal interface.
''
'' The collector stages symbol records during a compile and marshals them
'' into an FBCP_RESULT afterwards. Two feeds:
''  - collHarvestProcLocals(): called from astProcEnd/hProcFlush right before
''    a proc's local symtb is deleted (locals do NOT survive to a post-parse
''    walk - see REFACTOR_PLAN.md phase 2 notes).
''  - collWalkGlobals(): called after cProgram() while the global symbol
''    table is still alive; collects everything else (procs+params, TYPEs,
''    enums, typedefs, module-level vars, defines, namespaces).
'' Diagnostics are fed from error.bas::hPrintErrMsg().
''
'' A third feed carries REFERENCE counts: collRecordRef() is called from hooks
'' in the parser, at the point a source identifier resolves to a symbol.
''
'' WHY THE PARSER AND NOT fbc's OWN FB_SYMBSTATS_ACCESSED. That bit looks like
'' exactly this signal and is not: it is a CODEGEN LIVENESS bit, set from
'' astLoadVAR while flushing the AST, so it is set by trees the compiler
'' synthesized rather than by anything the user wrote. Measured: every
'' non-STATIC local is marked by its OWN DECLARATION, because astNewDECL hangs
'' hDefaultInit()'s tree off the DECL node (ast-node-decl.bas:104) and
'' astLoadDECL loads it (:122); explicit initialisers and STRING/UDT ctors and
'' dtors do the same by other routes. Only STATIC locals came out clean.
'' Reading it for PARAMs is worse than useless - the paramvar is freed with the
'' proc's symtb before any post-parse walk can see it. Do not revisit this.
''
'' Include after fb.bi/symb.bi (needs FBSYMBOL) and fbcParser.bi.

#ifndef __COLLECTOR_BI__
#define __COLLECTOR_BI__

'' self-sufficient for FBCP_RESULT: the parser modules carrying reference hooks
'' include this header without including the public one
#include once "fbcParser.bi"

declare sub collInit( )
declare sub collEnd( )
declare function collIsActive( ) as integer
declare sub collSetConsoleOutput( byval enable as integer )
declare function collConsoleOutput( ) as integer
declare sub collHarvestProcLocals( byval proc as FBSYMBOL ptr )
declare function collWantsRefs( ) as integer
declare sub collRecordRef( byval sym as any ptr, byval iswrite as integer )
declare sub collMarkLastRefWrite( byval sym as any ptr, byval alsoread as integer )
declare sub collWalkGlobals( )
declare sub collAddDiag _
	( _
		byval errnum as integer, _
		byval linenum as integer, _
		byref msgtext as string _
	)
declare sub collFinalize( byval result as FBCP_RESULT ptr )
declare sub collFreeResult( byval result as FBCP_RESULT ptr )

#endif '' __COLLECTOR_BI__
