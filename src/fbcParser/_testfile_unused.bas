'' Test input for reference counting (readCount/writeCount).
''
'' EVERY symbol here encodes its own expected counts in its NAME, as a trailing
'' _R<n>W<n>. `FBCPARSER_NOPAUSE=1 tiko_fbctest _testfile_unused.bas refs`
'' parses the name and compares, so the fixture and the assertions cannot drift
'' apart - adding a case is adding a declaration.
''
'' A symbol whose reference counts are not tracked (an overloaded proc, a TYPE
'' name) is named _RQ ("query" - expect FBCP_SYMBFLAG_REFTRACKED to be CLEAR).
'' Anything with no suffix at all is ignored by the checker.
''
'' Must parse with ZERO errors.

'' ---- globals ------------------------------------------------------------
dim shared gRead_R2W0 as long
dim shared gWritten_R1W1 as long
dim shared gDead_R0W0 as long

'' ---- consts -------------------------------------------------------------
const KUSED_R1W0 = 10
const KDEAD_R0W0 = 20
'' referenced ONLY by #ifdef, which resolves in the preprocessor and must NOT
'' count as a reference
#define MACRO_ONLY_GUARD
#ifdef MACRO_ONLY_GUARD
	const KGUARD_R0W0 = 30
#endif

'' ---- a TYPE with fields -------------------------------------------------
type Rec_RQ
	fRead_R1W0 as long
	fWritten_R0W1 as long
	fDead_R0W0 as long
end type

'' ---- procs --------------------------------------------------------------
declare sub DeclOnly_R0W0( byval pProto_R0W0 as long )

'' overloaded: a call resolves to the overload HEAD, so the whole set must
'' report as untracked rather than "one used, one dead"
declare sub Ovl_RQ overload ( byval a as long )
declare sub Ovl_RQ ( byval a as double )
sub Ovl_RQ( byval a as long )
end sub
sub Ovl_RQ( byval a as double )
end sub

'' address-taken only - never called. Must NOT read as dead: @Proc does not go
'' through cProcCall, it resolves via cAddrOfExpression -> hProcPtrBody.
sub Callback_R1W0( byval n as long )
	n = n
end sub

function Worker_R2W0( byval pUsed_R1W0 as long, byval pDead_R0W0 as long ) as long
	'' plain local read twice
	dim as long lRead_R2W0 = 1
	'' declared, never mentioned again - the case fbc's ACCESSED bit could
	'' never report, because the declaration itself marks it
	dim as long lDead_R0W0
	'' a STRING local, whose ctor/dtor also marked it under the old approach
	dim as string lDeadStr_R0W0
	'' assigned once, never read
	dim as long lWriteOnly_R0W1
	'' self-BOP reads AND writes
	dim as long lSelfBop_R1W1 = 0
	'' array + index: `a(i).f = 0` must charge the FIELD the write, and the
	'' array and the index a read each
	dim as Rec_RQ aRec_R1W0( 0 to 3 )
	dim as long lIdx_R1W0 = 0

	lWriteOnly_R0W1 = 5
	lSelfBop_R1W1 += 1
	aRec_R1W0( lIdx_R1W0 ).fWritten_R0W1 = 7

	gWritten_R1W1 = pUsed_R1W0 + lRead_R2W0 + lRead_R2W0 + KUSED_R1W0

	dim as Rec_RQ r
	return gWritten_R1W1 + r.fRead_R1W0 + gRead_R2W0
end function

'' ---- module level -------------------------------------------------------
dim shared pfn_R0W1 as sub( byval n as long )
pfn_R0W1 = @Callback_R1W0

'' Worker called from both statement and expression position
Worker_R2W0( 1, 2 )
dim as long lTmp = Worker_R2W0( 3, 4 ) + gRead_R2W0
