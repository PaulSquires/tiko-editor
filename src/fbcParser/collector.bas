'' fbcParser symbol collector - stages user-declared symbols during a compile
'' and marshals them into the flat FBCP_RESULT structure handed to the host.
''
'' Staging happens while compiler state is alive (interned filename ptrs,
'' symbol names, symbTypeToStr); everything a record needs is copied into the
'' string pool at append time, so finalization never touches compiler memory.
'' FBSYMBOL ptrs are kept only as identity keys for parent resolution and are
'' never dereferenced after the append that recorded them.

#include once "fb.bi"
#include once "fbint.bi"
#include once "parser.bi"                '' parser.currproc, for the paramvar join
#include once "lex.bi"                   '' lexLineNum(), for reference locations
#include once "fbcParser.bi"
#include once "collector.bi"

const COLL_INITSYMS  = 1024
const COLL_INITPOOL  = 65536                '' wchars
const COLL_INITFILES = 16
const COLL_INITDIAGS = 64
const COLL_INITREFS  = 4096
const COLL_WRITEBACKSCAN = 64           '' how far collMarkLastRefWrite looks back

type COLLSTAGE
	sym         as any ptr                  '' identity only - never deref
	parent      as any ptr                  '' identity only - never deref
	stagedat    as integer                  '' coll.refcount when staged - see below
	nextsame    as integer                  '' next stage index sharing .sym; -1 = end
	rec         as FBCP_SYMBOL              '' final except parentIndex
end type

'' One recorded source-text reference. .sym is an identity key ONLY and is never
'' dereferenced - by resolve time the symbol may be long freed.
''
'' WHY .stagedat ABOVE EXISTS. A public proc's locals are freed as soon as its
'' body is parsed (ast-node-proc.bas: astProcEnd -> hProcFlush ->
'' collHarvestProcLocals -> symbDelSymbolTb), so the pool recycles the address
'' and a LATER symbol can be staged under a pointer an EARLIER ref already used.
'' Matching on the pointer alone would credit one symbol's references to another.
''
'' coll.refcount is a monotonic clock both sides share. A symbol occupies its
'' address over one continuous interval, and two symbols can never occupy one
'' address at the same time, so the symbol alive when ref i was recorded is
'' exactly the one staged at the FIRST stage-time after i. Resolution therefore
'' walks the same-pointer chain and takes the smallest .stagedat > i.
'' Persistent symbols (globals, procs, params, fields) are staged last, at the
'' maximum clock value, and their addresses are never recycled.
type COLLREF
	sym         as any ptr                  '' identity only - NEVER deref
	iswrite     as integer
	fileidx     as long                     '' resolved AT RECORD TIME - both
	linenum     as long                     ''   are parse-time state
end type

type COLLFILE
	key         as any ptr                  '' interned filename ptr (identity only)
	nameOffset  as long
end type

type COLLCTX
	active      as integer

	stagecount  as integer
	refcount    as integer
	pool        as wstring ptr
	poolused    as integer
	poolcap     as integer
	filecount   as integer
	diagcount   as integer

	'' tiny cache: last file looked up
	lastfilekey as any ptr
	lastfileidx as long
end type

dim shared coll as COLLCTX
redim shared collstage( 0 to 0 ) as COLLSTAGE
redim shared collfiles( 0 to 0 ) as COLLFILE
redim shared colldiags( 0 to 0 ) as FBCP_DIAG
redim shared collrefs( 0 to 0 ) as COLLREF

''::::: init/end

sub collInit( )
	redim collstage( 0 to COLL_INITSYMS-1 )
	redim collfiles( 0 to COLL_INITFILES-1 )
	redim colldiags( 0 to COLL_INITDIAGS-1 )
	redim collrefs( 0 to COLL_INITREFS-1 )
	coll.stagecount = 0
	coll.refcount = 0
	coll.filecount = 0
	coll.diagcount = 0
	coll.poolcap = COLL_INITPOOL
	coll.poolused = 0
	coll.pool = allocate( coll.poolcap * sizeof( wstring ) )
	coll.lastfilekey = NULL
	coll.lastfileidx = -1
	coll.active = TRUE
end sub

sub collEnd( )
	coll.active = FALSE
	deallocate( coll.pool )
	coll.pool = NULL
	redim collstage( 0 to 0 )
	redim collfiles( 0 to 0 )
	redim colldiags( 0 to 0 )
	redim collrefs( 0 to 0 )
end sub

function collIsActive( ) as integer
	function = coll.active
end function

'' console echo of parse errors: on for standalone/debug runs, off for silent
'' DLL scans (see FBCP_OPTFLAG_CONSOLE). Deliberately NOT reset by collInit -
'' it's a host setting, not per-scan state.
dim shared coll_console as integer = TRUE

sub collSetConsoleOutput( byval enable as integer )
	coll_console = enable
end sub

function collConsoleOutput( ) as integer
	function = coll_console
end function

''::::: string pool

private sub hPoolEnsure( byval chars as integer )
	if( coll.poolused + chars > coll.poolcap ) then
		do
			coll.poolcap *= 2
		loop while( coll.poolused + chars > coll.poolcap )
		coll.pool = reallocate( coll.pool, coll.poolcap * sizeof( wstring ) )
	end if
end sub

'' append a zstring (ANSI source text) as widechars; returns the pool offset
private function hPoolAddZ( byval z as const zstring ptr ) as long
	dim as integer n = iif( z <> NULL, len( *z ), 0 )
	hPoolEnsure( n + 1 )
	dim as long ofs = coll.poolused
	for i as integer = 0 to n-1
		coll.pool[ofs+i] = z[i]
	next
	coll.pool[ofs+n] = 0
	coll.poolused += n + 1
	function = ofs
end function

private function hPoolAddStr( byref s as string ) as long
	function = hPoolAddZ( strptr( s ) )
end function

''::::: file table

private function hFileIndexOf( byval f as zstring ptr ) as long
	if( f = NULL ) then
		return -1
	end if

	'' consecutive symbols almost always share the file
	if( f = coll.lastfilekey ) then
		return coll.lastfileidx
	end if

	dim as long idx = -1
	for i as integer = 0 to coll.filecount-1
		if( collfiles(i).key = f ) then
			idx = i
			exit for
		end if
	next

	if( idx = -1 ) then
		if( coll.filecount > ubound( collfiles ) ) then
			redim preserve collfiles( 0 to ubound( collfiles ) * 2 + 1 )
		end if
		idx = coll.filecount
		collfiles(idx).key = f
		collfiles(idx).nameOffset = hPoolAddZ( f )
		coll.filecount += 1
	end if

	coll.lastfilekey = f
	coll.lastfileidx = idx
	function = idx
end function

''::::: symbol staging

'' should this symbol be reported to the host at all?
private function hShouldCollect( byval s as FBSYMBOL ptr ) as integer
	function = FALSE

	'' no location = created by the compiler, not by user code
	if( s->defloc.file = NULL ) then
		exit function
	end if

	dim as zstring ptr nm = symbGetName( s )
	if( nm = NULL ) then
		exit function
	end if

	'' compiler-generated names: descriptor structs ($FBARRAY..), temps
	'' (tmp$..), literal vars ({fbsc}..), internal labels (LABEL$..)
	dim as integer c = (*nm)[0]
	select case( c )
	case asc( "A" ) to asc( "Z" ), asc( "a" ) to asc( "z" ), asc( "_" )
	case else
		exit function
	end select
	if( instr( *nm, "$" ) > 0 ) then
		exit function
	end if

	select case( s->class )
	case FB_SYMBCLASS_PROC
		'' implicit main / module-level code proc
		if( (s->stats and (FB_SYMBSTATS_MAINPROC or FB_SYMBSTATS_MODLEVELPROC)) <> 0 ) then
			exit function
		end if

	case FB_SYMBCLASS_VAR
		'' param-backing vars and the function-result var duplicate what
		'' the param/proc records already report
		if( (s->attrib and (FB_SYMBATTRIB_PARAMVARBYDESC or _
		                    FB_SYMBATTRIB_PARAMVARBYVAL or _
		                    FB_SYMBATTRIB_PARAMVARBYREF or _
		                    FB_SYMBATTRIB_FUNCRESULT)) <> 0 ) then
			exit function
		end if

	case FB_SYMBCLASS_CONST, FB_SYMBCLASS_STRUCT, FB_SYMBCLASS_ENUM, _
	     FB_SYMBCLASS_FIELD, FB_SYMBCLASS_PARAM, FB_SYMBCLASS_TYPEDEF, _
	     FB_SYMBCLASS_DEFINE, FB_SYMBCLASS_NAMESPACE

	case else
		exit function
	end select

	function = TRUE
end function

private function hKindOf _
	( _
		byval s as FBSYMBOL ptr, _
		byval parent as FBSYMBOL ptr _
	) as long

	select case( s->class )
	case FB_SYMBCLASS_PROC
		function = iif( s->typ = FB_DATATYPE_VOID, FBCP_KIND_SUB, FBCP_KIND_FUNCTION )
	case FB_SYMBCLASS_STRUCT
		function = iif( symbGetUDTIsUnion( s ), FBCP_KIND_UNION, FBCP_KIND_TYPE )
	case FB_SYMBCLASS_ENUM
		function = FBCP_KIND_ENUM
	case FB_SYMBCLASS_CONST
		if( (parent <> NULL) andalso (parent->class = FB_SYMBCLASS_ENUM) ) then
			function = FBCP_KIND_ENUMVAL
		else
			function = FBCP_KIND_CONST
		end if
	case FB_SYMBCLASS_VAR
		function = FBCP_KIND_VAR
	case FB_SYMBCLASS_FIELD
		function = FBCP_KIND_FIELD
	case FB_SYMBCLASS_PARAM
		function = FBCP_KIND_PARAM
	case FB_SYMBCLASS_TYPEDEF
		function = FBCP_KIND_TYPEDEF
	case FB_SYMBCLASS_DEFINE
		function = FBCP_KIND_DEFINE
	case FB_SYMBCLASS_NAMESPACE
		function = FBCP_KIND_NAMESPACE
	case else
		function = 0
	end select
end function

'' symbTypeToStr() builds the text from id.name, so UDT/enum names inside it
'' are UPPERCASE - substitute the subtype's original casing back in
'' ("WIDGET PTR" -> "Widget PTR")
private function hFixTypeTextCase _
	( _
		byref typetext as string, _
		byval subtype as FBSYMBOL ptr _
	) as string

	if( subtype <> NULL ) then
		if( (subtype->origname <> NULL) andalso (symbGetName( subtype ) <> NULL) ) then
			dim as integer p = instr( typetext, *symbGetName( subtype ) )
			if( p > 0 ) then
				return left( typetext, p - 1 ) + *subtype->origname + _
					mid( typetext, p + len( *symbGetName( subtype ) ) )
			end if
		end if
	end if
	function = typetext
end function

private function hTypeText( byval s as FBSYMBOL ptr ) as string
	dim as string typetext

	select case( s->class )
	case FB_SYMBCLASS_PROC
		if( s->typ <> FB_DATATYPE_VOID ) then
			typetext = symbTypeToStr( s->typ, s->subtype )
		end if
	case FB_SYMBCLASS_STRUCT
		'' TYPE/UNION: report the EXTENDS base type's name. fbc models the
		'' base as a compiler-generated field ("base$") that the $-filter in
		'' hShouldCollect drops, so this is the only place the base survives.
		if( s->udt.base <> NULL ) then
			dim as FBSYMBOL ptr basetype = s->udt.base->subtype
			if( basetype <> NULL ) then
				if( basetype->origname <> NULL ) then
					typetext = *basetype->origname
				elseif( symbGetName( basetype ) <> NULL ) then
					typetext = *symbGetName( basetype )
				end if
			end if
		end if
	case FB_SYMBCLASS_VAR, FB_SYMBCLASS_FIELD
		typetext = symbTypeToStr( s->typ, s->subtype, s->lgt, symbGetIsFixLenStr( s ) )
	case FB_SYMBCLASS_PARAM, FB_SYMBCLASS_CONST, FB_SYMBCLASS_TYPEDEF
		typetext = symbTypeToStr( s->typ, s->subtype )
	end select

	if( len( typetext ) > 0 ) then
		typetext = hFixTypeTextCase( typetext, s->subtype )
	end if
	function = typetext
end function

private function hFlagsOf( byval s as FBSYMBOL ptr ) as long
	dim as long flags = 0

	if( symbIsShared( s ) ) then
		flags or= FBCP_SYMBFLAG_SHARED
	end if
	if( symbIsStatic( s ) ) then
		flags or= FBCP_SYMBFLAG_STATIC
	end if
	if( (s->attrib and FB_SYMBATTRIB_LOCAL) <> 0 ) then
		flags or= FBCP_SYMBFLAG_LOCAL
	end if
	'' reachable from outside anything a scan can see, so a zero reference
	'' count says nothing about it - the host cannot detect this itself
	if( symbIsExport( s ) ) then
		flags or= FBCP_SYMBFLAG_EXPORT
	end if

	select case( s->class )
	case FB_SYMBCLASS_PARAM
		select case( s->param.mode )
		case FB_PARAMMODE_BYREF
			flags or= FBCP_SYMBFLAG_BYREF
		case FB_PARAMMODE_BYDESC
			flags or= FBCP_SYMBFLAG_ARRAY
		end select
		if( symbParamIsOptional( s ) ) then
			flags or= FBCP_SYMBFLAG_OPTIONAL
		end if
		if( ucase( *symbGetName( s ) ) = "THIS" ) then
			flags or= FBCP_SYMBFLAG_INSTANCE
		end if

	case FB_SYMBCLASS_VAR, FB_SYMBCLASS_FIELD
		if( symbIsRef( s ) ) then
			flags or= FBCP_SYMBFLAG_BYREF
		end if
		if( symbGetArrayDimensions( s ) <> 0 ) then
			flags or= FBCP_SYMBFLAG_ARRAY
		end if
	end select

	'' declared AS a TYPE/UNION or enum? (directly or through ptr/typedef)
	select case( s->class )
	case FB_SYMBCLASS_VAR, FB_SYMBCLASS_PARAM, FB_SYMBCLASS_FIELD
		if( s->subtype <> NULL ) then
			select case( s->subtype->class )
			case FB_SYMBCLASS_STRUCT
				flags or= FBCP_SYMBFLAG_UDTTYPE
			case FB_SYMBCLASS_ENUM
				flags or= FBCP_SYMBFLAG_ENUMTYPE
			end select
		end if
	end select

	function = flags
end function

'' Are this symbol's reference counts meaningful? See FBCP_SYMBFLAG_REFTRACKED.
'' Kinds absent here report 0/0 meaning UNKNOWN, not unused: DEFINEs expand in
'' the preprocessor and never resolve to a symbol at all, namespaces are only
'' ever qualifier segments, and type names are not hooked (yet).
private function hRefTrackedFlag _
	( _
		byval s as FBSYMBOL ptr, _
		byval kind as long _
	) as long

	select case( kind )
	case FBCP_KIND_VAR, FBCP_KIND_PARAM, FBCP_KIND_SUB, FBCP_KIND_FUNCTION, _
	     FBCP_KIND_FIELD, FBCP_KIND_CONST, FBCP_KIND_ENUMVAL
	case else
		return 0
	end select

	'' A call resolves to the overload HEAD, so every member of an overloaded
	'' set would read as uncalled but one. Report the whole set as unknown
	'' rather than confidently wrong.
	if( s->class = FB_SYMBCLASS_PROC ) then
		if( symbIsOverloaded( s ) ) then
			return 0
		end if
	end if

	function = FBCP_SYMBFLAG_REFTRACKED
end function

'' append one symbol; returns TRUE if it was collected (walker descends into
'' containers only when the container itself was accepted)
private function hCollect _
	( _
		byval s as FBSYMBOL ptr, _
		byval parent as FBSYMBOL ptr _
	) as integer

	function = FALSE

	if( coll.active = FALSE ) then
		exit function
	end if
	if( hShouldCollect( s ) = FALSE ) then
		exit function
	end if

	dim as long kind = hKindOf( s, parent )
	if( kind = 0 ) then
		exit function
	end if

	if( coll.stagecount > ubound( collstage ) ) then
		redim preserve collstage( 0 to ubound( collstage ) * 2 + 1 )
	end if

	with collstage(coll.stagecount)
		.sym = s
		.parent = parent
		.stagedat = coll.refcount
		.nextsame = -1
		.rec.kind = kind
		.rec.flags = hFlagsOf( s ) or hRefTrackedFlag( s, kind )
		.rec.readCount = 0
		.rec.writeCount = 0
		.rec.refFileIndex = -1
		.rec.refLine = 0

		'' report the identifier as written in the source; id.name (the
		'' ucased lookup name) is only the fallback
		if( s->origname <> NULL ) then
			.rec.nameOffset = hPoolAddZ( s->origname )
		else
			.rec.nameOffset = hPoolAddZ( symbGetName( s ) )
		end if

		dim as string typetext = hTypeText( s )
		if( len( typetext ) > 0 ) then
			.rec.typeOffset = hPoolAddStr( typetext )
		else
			.rec.typeOffset = -1
		end if

		.rec.parentIndex = -1                   '' resolved in collFinalize
		.rec.fileIndex = hFileIndexOf( s->defloc.file )
		.rec.lineNum = s->defloc.linenum
		.rec.colNum = s->defloc.colnum

		.rec.bodyLine = 0
		.rec.bodyEndLine = 0
		if( s->class = FB_SYMBCLASS_PROC ) then
			if( s->proc.ext <> NULL ) then
				.rec.bodyLine = s->proc.ext->dbg.iniline
				.rec.bodyEndLine = s->proc.ext->dbg.endline
				'' a proc WITH a body reports the implementation's location:
				'' hosts want the body file/line (goto-definition, enclosing-
				'' proc scope tests, per-file proc lists), and the params
				'' already carry body locations (hCheckPrototype). A declare-
				'' only proc keeps its DECLARE location.
				if( (.rec.bodyLine > 0) and (s->proc.ext->dbg.incfile <> NULL) ) then
					.rec.fileIndex = hFileIndexOf( s->proc.ext->dbg.incfile )
					.rec.lineNum = .rec.bodyLine
					.rec.colNum = 0
				end if
			end if
		end if
	end with

	coll.stagecount += 1
	function = TRUE
end function

''::::: reference recording (called from the parser hooks)

function collWantsRefs( ) as integer
	function = coll.active
end function

'' A parameter reference never resolves to the PARAM record: symbAddVarForParam()
'' creates a SEPARATE VAR carrying FB_SYMBATTRIB_PARAMVARBY* (which hShouldCollect
'' excludes), and the only link is param->param.var, one-way. Map it back here,
'' DURING the parse, while both symbols are alive - the same lookup done later
'' from the harvest feed reads freed memory, since the paramvar dies with the
'' proc's symtb one line after collHarvestProcLocals runs.
private function hParamOfParamVar( byval s as FBSYMBOL ptr ) as FBSYMBOL ptr
	function = NULL

	if( (s->attrib and (FB_SYMBATTRIB_PARAMVARBYVAL or _
	                    FB_SYMBATTRIB_PARAMVARBYREF or _
	                    FB_SYMBATTRIB_PARAMVARBYDESC)) = 0 ) then
		exit function
	end if
	if( parser.currproc = NULL ) then
		exit function
	end if

	dim as FBSYMBOL ptr p = symbGetProcHeadParam( parser.currproc )
	while( p )
		if( symbGetParamVar( p ) = s ) then
			return p
		end if
		p = symbGetParamNext( p )
	wend
end function

sub collRecordRef( byval sym as any ptr, byval iswrite as integer )
	if( coll.active = FALSE ) then
		exit sub
	end if
	if( sym = NULL ) then
		exit sub
	end if

	dim as FBSYMBOL ptr s = cast( FBSYMBOL ptr, sym )
	if( s->class = FB_SYMBCLASS_VAR ) then
		dim as FBSYMBOL ptr p = hParamOfParamVar( s )
		if( p <> NULL ) then
			s = p
		end if
	end if

	if( coll.refcount > ubound( collrefs ) ) then
		redim preserve collrefs( 0 to ubound( collrefs ) * 2 + 1 )
	end if

	with collrefs(coll.refcount)
		.sym = s
		.iswrite = FALSE
		'' env.inf.incfile and lexLineNum() are parse-time state - resolve now
		.fileidx = hFileIndexOf( env.inf.incfile )
		.linenum = lexLineNum( )
	end with
	coll.refcount += 1

	if( iswrite ) then
		collrefs(coll.refcount-1).iswrite = TRUE
	end if
end sub

'' Turn the newest reference to sym into a WRITE. The parser cannot know an
'' identifier is an assignment target when it resolves it - cAssignmentOrPtrCall
'' parses the whole LHS before it sees the '=' - so the record is retro-fixed
'' one token later, from cAssignment. alsoread is for the self-BOPs (x += 1),
'' which genuinely read AND write.
sub collMarkLastRefWrite( byval sym as any ptr, byval alsoread as integer )
	if( coll.active = FALSE ) then
		exit sub
	end if
	if( sym = NULL ) then
		exit sub
	end if

	dim as FBSYMBOL ptr s = cast( FBSYMBOL ptr, sym )
	if( s->class = FB_SYMBCLASS_VAR ) then
		dim as FBSYMBOL ptr p = hParamOfParamVar( s )
		if( p <> NULL ) then
			s = p
		end if
	end if

	'' the target was resolved within the last few tokens; a bounded scan
	'' keeps this O(1) on a module with hundreds of thousands of references
	dim as integer lo = coll.refcount - COLL_WRITEBACKSCAN
	if( lo < 0 ) then
		lo = 0
	end if

	for i as integer = coll.refcount-1 to lo step -1
		if( collrefs(i).sym = s ) then
			if( alsoread ) then
				'' keep the existing record as the read, add the write
				collRecordRef( s, TRUE )
			else
				collrefs(i).iswrite = TRUE
			end if
			exit sub
		end if
	next

	'' no matching record (error recovery, or an LHS shape that never passed
	'' through a hooked resolver): a write at roughly the right line beats a
	'' lost write, which would read as dead code
	collRecordRef( s, TRUE )
end sub

''::::: feeds

'' locals (incl. STATICs and nested SCOPE blocks) - called right before a
'' proc's local symtb is deleted
private sub hHarvestLocalsTb _
	( _
		byval head as FBSYMBOL ptr, _
		byval proc as FBSYMBOL ptr _
	)

	dim as FBSYMBOL ptr s = head
	while( s )
		select case( s->class )
		case FB_SYMBCLASS_VAR, FB_SYMBCLASS_CONST
			hCollect( s, proc )
		case FB_SYMBCLASS_SCOPE
			hHarvestLocalsTb( symbGetScopeSymbTbHead( s ), proc )
		end select
		s = s->next
	wend
end sub

sub collHarvestProcLocals( byval proc as FBSYMBOL ptr )
	if( coll.active = FALSE ) then
		exit sub
	end if
	hHarvestLocalsTb( symbGetProcSymbTbHead( proc ), proc )
end sub

'' everything reachable from the global namespace
private sub hWalkSymTb _
	( _
		byval head as FBSYMBOL ptr, _
		byval parent as FBSYMBOL ptr _
	)

	dim as FBSYMBOL ptr s = head
	while( s )
		select case( s->class )
		case FB_SYMBCLASS_STRUCT, FB_SYMBCLASS_ENUM
			if( hCollect( s, parent ) ) then
				'' fields / enum constants / member proc declares
				hWalkSymTb( symbGetUDTSymbTbHead( s ), s )
			end if

		case FB_SYMBCLASS_PROC
			if( hCollect( s, parent ) ) then
				dim as FBSYMBOL ptr p = symbGetProcHeadParam( s )
				while( p )
					hCollect( p, s )
					p = symbGetParamNext( p )
				wend
			end if

		case FB_SYMBCLASS_NAMESPACE
			if( hCollect( s, parent ) ) then
				hWalkSymTb( symbGetNamespaceSymbTb( s ).head, s )
			end if

		case else
			hCollect( s, parent )
		end select

		s = s->next
	wend
end sub

sub collWalkGlobals( )
	if( coll.active = FALSE ) then
		exit sub
	end if
	hWalkSymTb( symbGetGlobalTbHead( ), NULL )
end sub

''::::: diagnostics

sub collAddDiag _
	( _
		byval errnum as integer, _
		byval linenum as integer, _
		byref msgtext as string _
	)

	if( coll.active = FALSE ) then
		exit sub
	end if

	if( coll.diagcount > ubound( colldiags ) ) then
		redim preserve colldiags( 0 to ubound( colldiags ) * 2 + 1 )
	end if

	with colldiags(coll.diagcount)
		.errNum = errnum
		.fileIndex = hFileIndexOf( env.inf.incfile )
		.lineNum = linenum
		.textOffset = hPoolAddStr( msgtext )
	end with
	coll.diagcount += 1
end sub

''::::: finalize - parent resolution + marshalling

'' open-addressing hash: FBSYMBOL ptr -> staged index, for the containers
'' (procs/TYPEs/enums/namespaces). Keys are used purely as identity - the
'' pointed-to symbols may be long freed. Container symbols themselves live in
'' persistent tables, so a container ptr can't have been recycled from a
'' freed local (only locals are freed mid-compile).
private function hHashSlot _
	( _
		keys() as any ptr, _
		byval mask as integer, _
		byval key as any ptr _
	) as integer

	dim as integer idx = cint( (culngint( key ) shr 4) and mask )
	while( (keys(idx) <> NULL) andalso (keys(idx) <> key) )
		idx = (idx + 1) and mask
	wend
	function = idx
end function

sub collFinalize( byval result as FBCP_RESULT ptr )
	clear *result, 0, sizeof( FBCP_RESULT )
	result->version = FBCP_VERSION

	'' parent map
	dim as integer cap = 16
	while( cap < coll.stagecount * 2 )
		cap shl= 1
	wend
	dim as integer mask = cap - 1
	redim as any ptr mapkeys( 0 to cap-1 )
	redim as long mapvals( 0 to cap-1 )

	for i as integer = 0 to coll.stagecount-1
		select case( collstage(i).rec.kind )
		case FBCP_KIND_SUB, FBCP_KIND_FUNCTION, FBCP_KIND_TYPE, _
		     FBCP_KIND_UNION, FBCP_KIND_ENUM, FBCP_KIND_NAMESPACE
			dim as integer slot = hHashSlot( mapkeys(), mask, collstage(i).sym )
			mapkeys(slot) = collstage(i).sym
			mapvals(slot) = i
		end select
	next

	'' Reference map. DELIBERATELY SEPARATE from the parent map above, which
	'' must keep holding containers only: a proc can be allocated into a freed
	'' local's recycled slot, and the local is staged first, so a shared map
	'' would resolve that proc's children to the local instead.
	'' Here every staged kind participates, and entries sharing a pointer are
	'' chained through .nextsame in STAGE ORDER.
	redim as any ptr refkeys( 0 to cap-1 )
	redim as long refvals( 0 to cap-1 )

	for i as integer = 0 to coll.stagecount-1
		dim as integer slot = hHashSlot( refkeys(), mask, collstage(i).sym )
		if( refkeys(slot) = NULL ) then
			refkeys(slot) = collstage(i).sym
			refvals(slot) = i
		else
			dim as integer j = refvals(slot)
			while( collstage(j).nextsame <> -1 )
				j = collstage(j).nextsame
			wend
			collstage(j).nextsame = i
		end if
	next

	'' references -> counts. The symbol alive when ref i was recorded is the
	'' one staged at the first stage-time after i (see COLLREF's header).
	for i as integer = 0 to coll.refcount-1
		dim as integer slot = hHashSlot( refkeys(), mask, collrefs(i).sym )
		if( refkeys(slot) = NULL ) then
			continue for                    '' never staged - dropped
		end if

		dim as integer j = refvals(slot), hit = -1
		while( j <> -1 )
			if( collstage(j).stagedat > i ) then
				hit = j
				exit while
			end if
			j = collstage(j).nextsame
		wend
		if( hit = -1 ) then
			continue for
		end if

		with collstage(hit)
			if( collrefs(i).iswrite ) then
				.rec.writeCount += 1
			else
				.rec.readCount += 1
			end if
			if( .rec.refLine = 0 ) then
				.rec.refFileIndex = collrefs(i).fileidx
				.rec.refLine = collrefs(i).linenum
			end if
		end with
	next

	'' symbols
	result->symbolCount = coll.stagecount
	if( coll.stagecount > 0 ) then
		result->symbols = callocate( coll.stagecount * sizeof( FBCP_SYMBOL ) )
		for i as integer = 0 to coll.stagecount-1
			dim as FBCP_SYMBOL rec = collstage(i).rec
			if( collstage(i).parent <> NULL ) then
				dim as integer slot = hHashSlot( mapkeys(), mask, collstage(i).parent )
				if( mapkeys(slot) <> NULL ) then
					rec.parentIndex = mapvals(slot)
				end if
			end if
			'' a "local" whose proc wasn't collected (e.g. module-level
			'' code in the implicit main) is effectively top-level
			if( rec.parentIndex = -1 ) then
				rec.flags and= not FBCP_SYMBFLAG_LOCAL
			end if
			result->symbols[i] = rec
		next
	end if

	'' files
	result->fileCount = coll.filecount
	if( coll.filecount > 0 ) then
		result->fileOffsets = callocate( coll.filecount * sizeof( long ) )
		for i as integer = 0 to coll.filecount-1
			result->fileOffsets[i] = collfiles(i).nameOffset
		next
	end if

	'' diags
	result->diagCount = coll.diagcount
	if( coll.diagcount > 0 ) then
		result->diags = callocate( coll.diagcount * sizeof( FBCP_DIAG ) )
		for i as integer = 0 to coll.diagcount-1
			result->diags[i] = colldiags(i)
		next
	end if

	'' pool
	result->poolChars = coll.poolused
	if( coll.poolused > 0 ) then
		result->stringPool = allocate( coll.poolused * sizeof( wstring ) )
		for i as integer = 0 to coll.poolused-1
			result->stringPool[i] = coll.pool[i]
		next
	end if
end sub

sub collFreeResult( byval result as FBCP_RESULT ptr )
	if( result = NULL ) then
		exit sub
	end if
	deallocate( result->symbols )
	deallocate( result->fileOffsets )
	deallocate( result->diags )
	deallocate( result->stringPool )
	clear *result, 0, sizeof( FBCP_RESULT )
end sub
