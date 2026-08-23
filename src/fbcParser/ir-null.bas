'' intermediate representation - null backend
''
'' Discards all code generation. fbcParser only needs the front end (lexer,
'' preprocessor, parser, symbol table) to extract symbol information; this
'' backend satisfies the ir.vtbl calls the parser/AST make during cProgram()
'' without emitting anything or writing any output file.
''
'' Modeled on the C backend's vtable (the deleted ir-hlc.bas): the same slots
'' are NULL here that were NULL there, because with the backend option pinned
'' to FB_BACKEND_GCC the AST takes the high-level paths that never call them.
'' The vreg allocators must return real IRVREG objects - the AST dereferences
'' them constantly - so they reuse the irhl* pool helpers from ir.bas.

#include once "fb.bi"
#include once "fbint.bi"
#include once "list.bi"
#include once "flist.bi"
#include once "ir.bi"
#include once "ir-private.bi"
#include once "lex.bi"

private sub _init( )
	irhlInit( )

	'' No IR_OPT_MISSINGOPS: _supportsOp() reports everything as available,
	'' so the AST never bothers emulating "missing" operators.
	irSetOption( IR_OPT_FPUIMMEDIATES )
end sub

private sub _end( )
	irhlEnd( )
end sub

private function _emitBegin( ) as integer
	'' No output file is opened; parsing proceeds normally.
	function = TRUE
end function

private sub _emitEnd( )
end sub

private function _getOptionValue( byval opt as IR_OPTIONVALUE ) as integer
	select case opt
	case IR_OPTIONVALUE_MAXMEMBLOCKLEN
		'' 0 = unlimited, same as the C backend
		return 0
	case else
		errReportEx( FB_ERRMSG_INTERNAL, __FUNCTION__ )
	end select
end function

private function _supportsOp _
	( _
		byval op as integer, _
		byval dtype as integer _
	) as integer
	function = TRUE
end function

'' Keep recording the proc body's line range - it costs nothing and the
'' symbol extraction (phase 3) can report it to tiko.
private sub _procBegin( byval proc as FBSYMBOL ptr )
	proc->proc.ext->dbg.iniline = lexLineNum( )
end sub

private sub _procEnd( byval proc as FBSYMBOL ptr )
	proc->proc.ext->dbg.endline = lexLineNum( )
end sub

private sub _scopeBegin( byval s as FBSYMBOL ptr )
end sub

private sub _scopeEnd( byval s as FBSYMBOL ptr )
end sub

private sub _procAllocStaticVars( byval head_sym as FBSYMBOL ptr )
end sub

private sub _emitConvert( byval v1 as IRVREG ptr, byval v2 as IRVREG ptr )
end sub

private sub _emitLabel( byval label as FBSYMBOL ptr )
end sub

private sub _emitProcBegin _
	( _
		byval proc as FBSYMBOL ptr, _
		byval initlabel as FBSYMBOL ptr _
	)
	irhlEmitProcBegin( )
end sub

private sub _emitProcEnd _
	( _
		byval proc as FBSYMBOL ptr, _
		byval initlabel as FBSYMBOL ptr, _
		byval exitlabel as FBSYMBOL ptr _
	)
	'' Resets the per-proc vreg pool, keeping memory bounded.
	irhlEmitProcEnd( )
end sub

'' Deliberately NOT irhlEmitPushArg(): that records args in irhl.callargs for
'' the backend's _emitCall to drain. Nothing drains them here, so recording
'' them would only leak list nodes until fbEnd.
private sub _emitPushArg _
	( _
		byval param as FBSYMBOL ptr, _
		byval vr as IRVREG ptr, _
		byval udtlen as longint, _
		byval level as integer, _
		byval lreg as IRVREG ptr _
	)
end sub

private sub _emitAsmLine( byval asmtokenhead as ASTASMTOK ptr )
end sub

private sub _emitComment( byval text as zstring ptr )
end sub

private sub _emitBop _
	( _
		byval op as integer, _
		byval v1 as IRVREG ptr, _
		byval v2 as IRVREG ptr, _
		byval vr as IRVREG ptr, _
		byval label as FBSYMBOL ptr, _
		byval options as IR_EMITOPT _
	)
end sub

private sub _emitUop _
	( _
		byval op as integer, _
		byval v1 as IRVREG ptr, _
		byval vr as IRVREG ptr _
	)
end sub

private sub _emitStore( byval v1 as IRVREG ptr, byval v2 as IRVREG ptr )
end sub

private sub _emitSpillRegs( )
end sub

private sub _emitLoad( byval v1 as IRVREG ptr )
end sub

private sub _emitLoadRes( byval v1 as IRVREG ptr, byval vr as IRVREG ptr )
end sub

private sub _emitAddr _
	( _
		byval op as integer, _
		byval v1 as IRVREG ptr, _
		byval vr as IRVREG ptr _
	)
end sub

private sub _emitCall _
	( _
		byval proc as FBSYMBOL ptr, _
		byval bytestopop as integer, _
		byval vr as IRVREG ptr, _
		byval level as integer _
	)
end sub

private sub _emitCallPtr _
	( _
		byval proc as FBSYMBOL ptr, _
		byval v1 as IRVREG ptr, _
		byval vr as IRVREG ptr, _
		byval bytestopop as integer, _
		byval level as integer _
	)
end sub

private sub _emitJumpPtr( byval v1 as IRVREG ptr )
end sub

private sub _emitBranch( byval op as integer, byval label as FBSYMBOL ptr )
end sub

private sub _emitJmpTb _
	( _
		byval v1 as IRVREG ptr, _
		byval tbsym as FBSYMBOL ptr, _
		byval values as ulongint ptr, _
		byval labels as FBSYMBOL ptr ptr, _
		byval labelcount as integer, _
		byval deflabel as FBSYMBOL ptr, _
		byval bias as ulongint, _
		byval span as ulongint _
	)
end sub

private sub _emitMem _
	( _
		byval op as integer, _
		byval v1 as IRVREG ptr, _
		byval v2 as IRVREG ptr, _
		byval bytes as longint, _
		byval fillchar as integer _
	)
end sub

private sub _emitMacro _
	( _
		byval op as integer, _
		byval v1 as IRVREG ptr, _
		byval v2 as IRVREG ptr, _
		byval vr as IRVREG ptr _
	)
end sub

private sub _emitScopeBegin( byval s as FBSYMBOL ptr )
end sub

private sub _emitScopeEnd( byval s as FBSYMBOL ptr )
end sub

private sub _emitDECL( byval sym as FBSYMBOL ptr )
end sub

private sub _emitDBG _
	( _
		byval op as integer, _
		byval proc as FBSYMBOL ptr, _
		byval ex as integer, _
		byval filename as zstring ptr = 0 _
	)
end sub

private sub _emitVarIniBegin( byval sym as FBSYMBOL ptr )
end sub

private sub _emitVarIniEnd( byval sym as FBSYMBOL ptr )
end sub

private sub _emitVarIniI( byval sym as FBSYMBOL ptr, byval value as longint )
end sub

private sub _emitVarIniF( byval sym as FBSYMBOL ptr, byval value as double )
end sub

private sub _emitVarIniOfs _
	( _
		byval sym as FBSYMBOL ptr, _
		byval rhs as FBSYMBOL ptr, _
		byval ofs as longint _
	)
end sub

private sub _emitVarIniStr _
	( _
		byval totlgt as longint, _
		byval litstr as zstring ptr, _
		byval litlgt as longint, _
		byval noterm as integer _
	)
end sub

private sub _emitVarIniWstr _
	( _
		byval totlgt as longint, _
		byval litstr as wstring ptr, _
		byval litlgt as longint _
	)
end sub

private sub _emitVarIniPad( byval bytes as longint, byval fillchar as integer )
end sub

private sub _emitVarIniScopeBegin( byval sym as FBSYMBOL ptr, byval is_array as integer )
end sub

private sub _emitVarIniScopeEnd( )
end sub

private sub _emitFbctinfBegin( )
end sub

private sub _emitFbctinfString( byval s as const zstring ptr )
end sub

private sub _emitFbctinfEnd( )
end sub

private sub _setVregDataType _
	( _
		byval vreg as IRVREG ptr, _
		byval dtype as integer, _
		byval subtype as FBSYMBOL ptr _
	)

	if( vreg <> NULL ) then
		vreg->dtype = dtype
		vreg->subtype = subtype
	end if

end sub

'' Same NULL slots as the deleted irhlc_vtbl: procAllocArg, procAllocLocal,
'' procGetFrameRegName, emitReturn, emitStack, emitStackAlign, getDistance,
'' loadVr, storeVr, xchgTOS are never called on the high-level backend paths.
dim shared as IR_VTBL irnull_vtbl = _
( _
	@_init, _
	@_end, _
	@_emitBegin, _
	@_emitEnd, _
	@_getOptionValue, _
	@_supportsOp, _
	@_procBegin, _
	@_procEnd, _
	NULL, _
	NULL, _
	NULL, _
	@_scopeBegin, _
	@_scopeEnd, _
	@_procAllocStaticVars, _
	@_emitConvert, _
	@_emitLabel, _
	@_emitLabel, _
	NULL, _
	@_emitProcBegin, _
	@_emitProcEnd, _
	@_emitPushArg, _
	@_emitAsmLine, _
	@_emitComment, _
	@_emitBop, _
	@_emitUop, _
	@_emitStore, _
	@_emitSpillRegs, _
	@_emitLoad, _
	@_emitLoadRes, _
	NULL, _
	@_emitAddr, _
	@_emitCall, _
	@_emitCallPtr, _
	NULL, _
	@_emitJumpPtr, _
	@_emitBranch, _
	@_emitJmpTb, _
	@_emitMem, _
	@_emitMacro, _
	@_emitScopeBegin, _
	@_emitScopeEnd, _
	@_emitDECL, _
	@_emitDBG, _
	@_emitVarIniBegin, _
	@_emitVarIniEnd, _
	@_emitVarIniI, _
	@_emitVarIniF, _
	@_emitVarIniOfs, _
	@_emitVarIniStr, _
	@_emitVarIniWstr, _
	@_emitVarIniPad, _
	@_emitVarIniScopeBegin, _
	@_emitVarIniScopeEnd, _
	@_emitFbctinfBegin, _
	@_emitFbctinfString, _
	@_emitFbctinfEnd, _
	@irhlAllocVreg, _
	@irhlAllocVrImm, _
	@irhlAllocVrImmF, _
	@irhlAllocVrVar, _
	@irhlAllocVrIdx, _
	@irhlAllocVrPtr, _
	@irhlAllocVrOfs, _
	@_setVregDataType, _
	NULL, _
	NULL, _
	NULL, _
	NULL _
)
