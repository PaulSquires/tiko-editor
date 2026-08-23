'' fbcParser test harness - a standalone exe that consumes fbcParser.dll
'' exactly the way a host application (tiko) would: through fbcparser_scan/
'' fbcparser_free and the flat FBCP_RESULT only. Nothing here touches
'' compiler internals.
''
'' usage: tiko_fbctest [file.bas]     (default: _testfile.bas)
'' env:   FBCPARSER_NOPAUSE=1  skip the console pause
''        FBCPARSER_QUIET=1    summary only, no per-symbol dump
''        FBCPARSER_TWICE=1    scan a second time and compare (re-entrancy)
''        FBCPARSER_INCDIR=dir pass dir as an include search path option
''        FBCPARSER_LOOP=N     scan N times total; report working-set growth
''                             per scan (leak detector)
''        FBCPARSER_TEXTMODE=1 load the file into memory, scan it again via
''                             fbcparser_scan_text and byte-compare results

#include once "fbcParser.bi"
#include once "windows.bi"
#include once "win\psapi.bi"

private function hLoadFile( byref fname as string ) as string
	dim as integer f = freefile
	if( open( fname, for binary, access read, as #f ) <> 0 ) then
		return ""
	end if
	dim as string s = space( lof( f ) )
	if( lof( f ) > 0 ) then
		get #f, 1, s
	end if
	close #f
	function = s
end function

private function hMemEq _
	( _
		byval a as any ptr, _
		byval b as any ptr, _
		byval n as integer _
	) as integer

	dim as ubyte ptr pa = a, pb = b
	for i as integer = 0 to n-1
		if( pa[i] <> pb[i] ) then
			return 0
		end if
	next
	function = -1
end function

private function hWorkingSetKb( ) as longint
	dim as PROCESS_MEMORY_COUNTERS pmc
	pmc.cb = sizeof( pmc )
	if( GetProcessMemoryInfo( GetCurrentProcess( ), @pmc, pmc.cb ) ) then
		return pmc.WorkingSetSize \ 1024
	end if
	function = 0
end function

private function hKindName( byval kind as long ) as string
	select case( kind )
	case FBCP_KIND_SUB       : function = "sub"
	case FBCP_KIND_FUNCTION  : function = "function"
	case FBCP_KIND_PARAM     : function = "param"
	case FBCP_KIND_TYPE      : function = "type"
	case FBCP_KIND_UNION     : function = "union"
	case FBCP_KIND_FIELD     : function = "field"
	case FBCP_KIND_ENUM      : function = "enum"
	case FBCP_KIND_ENUMVAL   : function = "enumval"
	case FBCP_KIND_VAR       : function = "var"
	case FBCP_KIND_CONST     : function = "const"
	case FBCP_KIND_TYPEDEF   : function = "typedef"
	case FBCP_KIND_DEFINE    : function = "define"
	case FBCP_KIND_NAMESPACE : function = "namespace"
	case else                : function = "kind#" & kind
	end select
end function

'' assign through a string var before printing - print'ing wstring data (even
'' &-concatenated, which promotes to wstring) interleaves the wide chars on
'' the console (Learnings.md)
private function hPoolStr( byval res as FBCP_RESULT ptr, byval ofs as long ) as string
	if( ofs < 0 ) then
		return ""
	end if
	function = *cptr( wstring ptr, @res->stringPool[ofs] )
end function

private sub hPrintResult( byval res as FBCP_RESULT ptr )
	print "=== result: "; str( res->symbolCount ); " symbols, "; _
		str( res->fileCount ); " files, "; str( res->diagCount ); " diags, "; _
		str( res->poolChars ); " pool chars ==="

	for i as integer = 0 to res->fileCount-1
		print "file[" & str( i ) & "] " & hPoolStr( res, res->fileOffsets[i] )
	next

	for i as integer = 0 to res->symbolCount-1
		with res->symbols[i]
			dim as string ln = hKindName( .kind ) & " " & hPoolStr( res, .nameOffset )
			if( .typeOffset >= 0 ) then
				ln += " as " & hPoolStr( res, .typeOffset )
			end if
			ln += "  [file " & .fileIndex & ", " & .lineNum & ":" & .colNum & "]"
			if( .parentIndex >= 0 ) then
				ln += " parent=" & .parentIndex & " (" & _
					hPoolStr( res, res->symbols[.parentIndex].nameOffset ) & ")"
			end if
			if( .flags <> 0 ) then
				ln += " flags=&h" & hex( .flags )
			end if
			if( .bodyLine <> 0 ) then
				ln += " body=" & .bodyLine & ".." & .bodyEndLine
			end if
			if( (.flags and FBCP_SYMBFLAG_REFTRACKED) <> 0 ) then
				ln += " refs=" & .readCount & "r" & .writeCount & "w"
				if( .refLine <> 0 ) then
					ln += "@" & .refFileIndex & ":" & .refLine
				end if
			else
				ln += " refs=?"
			end if
			print ln
		end with
	next

	for i as integer = 0 to res->diagCount-1
		with res->diags[i]
			print "diag[" & str( i ) & "] err " & str( .errNum ) & " [file " & _
				str( .fileIndex ) & ", line " & str( .lineNum ) & "] " & _
				hPoolStr( res, .textOffset )
		end with
	next
end sub

''
'' Reference-count self-check.
''
'' Expectations live in the SYMBOL NAMES of the fixture (a trailing _R<n>W<n>,
'' or _RQ for "counts must be untracked"), so the fixture cannot drift away from
'' the assertions: adding a case is adding a declaration, and a symbol whose
'' name says nothing is not checked.
''
private function hCheckRefs( byval res as FBCP_RESULT ptr, byref testfile as const string ) as integer
	dim as integer npass = 0, nfail = 0, nchecked = 0

	print "--- reference counts: expectations read from symbol names ---"

	for i as integer = 0 to res->symbolCount-1
		with res->symbols[i]
			dim as string nm = hPoolStr( res, .nameOffset )

			dim as integer p = instrrev( nm, "_R" )
			if( p = 0 ) then
				continue for
			end if
			dim as string tail = mid( nm, p + 2 )
			dim as integer tracked = ((.flags and FBCP_SYMBFLAG_REFTRACKED) <> 0)

			'' _RQ: the counts must be reported as UNKNOWN
			if( tail = "Q" ) then
				nchecked += 1
				if( tracked ) then
					print "  FAIL " & nm & ": expected untracked, got refs=" & _
						.readCount & "r" & .writeCount & "w"
					nfail += 1
				else
					npass += 1
				end if
				continue for
			end if

			dim as integer w = instr( tail, "W" )
			if( w = 0 ) then
				continue for
			end if
			dim as integer wantr = valint( left( tail, w - 1 ) )
			dim as integer wantw = valint( mid( tail, w + 1 ) )

			nchecked += 1
			if( tracked = FALSE ) then
				print "  FAIL " & nm & ": expected " & wantr & "r" & wantw & _
					"w but the symbol is not reference-tracked"
				nfail += 1
			elseif( (.readCount <> wantr) or (.writeCount <> wantw) ) then
				print "  FAIL " & nm & ": expected " & wantr & "r" & wantw & _
					"w, got " & .readCount & "r" & .writeCount & "w"
				nfail += 1
			else
				npass += 1
			end if
		end with
	next

	'' A name-driven checker passes vacuously if the names stop being parsed -
	'' a rename, a pool bug or an empty result would all read as "0 failures".
	'' Assert the fixture's own size so silence cannot look like success.
	dim as integer expected = 0
	select case lcase( testfile )
	case "_testfile_unused.bas" : expected = 26
	case "_testfile_this.bas"   : expected = 9
	end select
	if( expected > 0 ) then
		if( nchecked <> expected ) then
			print "  FAIL fixture: checked " & nchecked & " symbols, expected " & _
				expected & " (did the fixture change?)"
			nfail += 1
		end if
	else
		print "  NOTE unknown fixture - size not asserted, " & nchecked & " symbols checked"
	end if

	print "--- refs: " & npass & " passed, " & nfail & " failed (" & _
		nchecked & " symbols checked) ---"
	function = nfail
end function

'' Summary of what an unused-symbols report would show. Mirrors the host-side
'' policy roughly enough to judge the noise level on a real project.
private sub hSummarizeUnused( byval res as FBCP_RESULT ptr )
	dim as integer tracked = 0, dead = 0, writeonly = 0, untracked = 0
	dim as integer deadproc = 0, deadvar = 0, deadparam = 0, deadfield = 0

	'' optional: restrict to files under a prefix, the way a host restricts the
	'' report to the project's OWN sources and excludes the toolchain includes
	dim as string only = ucase( environ( "FBCPARSER_UNUSED_UNDER" ) )

	for i as integer = 0 to res->symbolCount-1
		with res->symbols[i]
			if( len( only ) > 0 ) then
				if( .fileIndex < 0 ) then continue for
				dim as string fn = hPoolStr( res, res->fileOffsets[.fileIndex] )
				if( instr( fn, only ) <> 1 ) then continue for
			end if
			if( (.flags and FBCP_SYMBFLAG_REFTRACKED) = 0 ) then
				untracked += 1
				continue for
			end if
			tracked += 1
			if( (.readCount = 0) and (.writeCount = 0) ) then
				dead += 1
				select case( .kind )
				case FBCP_KIND_SUB, FBCP_KIND_FUNCTION : deadproc += 1
				case FBCP_KIND_VAR                     : deadvar += 1
				case FBCP_KIND_PARAM                   : deadparam += 1
				case FBCP_KIND_FIELD                   : deadfield += 1
				end select
			elseif( .readCount = 0 ) then
				writeonly += 1
			end if
		end with
	next

	print "--- unused summary ---"
	print "  tracked   " & tracked & " of " & res->symbolCount & _
		" (" & untracked & " untracked)"
	print "  0 refs    " & dead & "  (procs " & deadproc & ", vars " & deadvar & _
		", params " & deadparam & ", fields " & deadfield & ")"
	print "  write-only " & writeonly
end sub

'' List the unused symbols themselves - the engine-side equivalent of the host's
'' report, and the only way to eyeball whether the counts are believable.
private sub hListUnused( byval res as FBCP_RESULT ptr, byval maxrows as integer )
	dim as string only = ucase( environ( "FBCPARSER_UNUSED_UNDER" ) )
    dim as string kindfilter = lcase( environ( "FBCPARSER_UNUSED_KIND" ) )
	dim as integer shown = 0

	for i as integer = 0 to res->symbolCount-1
		with res->symbols[i]
			if( (.flags and FBCP_SYMBFLAG_REFTRACKED) = 0 ) then continue for
			if( (.readCount <> 0) or (.writeCount <> 0) ) then continue for
			if( .fileIndex < 0 ) then continue for
			dim as string fn = hPoolStr( res, res->fileOffsets[.fileIndex] )
			if( len( only ) > 0 ) then
				if( instr( fn, only ) <> 1 ) then continue for
			end if
			if( len( kindfilter ) > 0 ) then
				if( hKindName( .kind ) <> kindfilter ) then continue for
			end if
			dim as integer sl = instrrev( fn, "\\" )
			print "  " & mid( fn, sl + 1 ) & ":" & .lineNum & "  " & _
				hKindName( .kind ) & " " & hPoolStr( res, .nameOffset )
			shown += 1
			if( shown >= maxrows ) then
				print "  ... (stopped at " & maxrows & ")"
				exit for
			end if
		end with
	next
end sub

'' MAIN

dim as string testfile = command( 1 )
if( len( testfile ) = 0 ) then
	testfile = "_testfile.bas"
end if
dim as wstring * 1024 wfile = testfile

dim as FBCP_OPTIONS opts
opts.version = FBCP_VERSION
opts.flags = FBCP_OPTFLAG_CONSOLE          '' echo parse errors while testing

'' optional include search path, the way tiko would pass its project dirs
dim as wstring * 1024 wincdir
dim as wstring ptr incdirs( 0 to 0 )
dim as string sincdir = environ( "FBCPARSER_INCDIR" )
if( len( sincdir ) > 0 ) then
	wincdir = sincdir
	incdirs(0) = @wincdir
	opts.includeCount = 1
	opts.includePaths = @incdirs(0)
end if

dim as FBCP_RESULT ptr res = 0
dim as long rc = fbcparser_scan( @wfile, @opts, @res )

if( rc <> FBCP_OK ) then
	print "fbcparser_scan( """; testfile; """ ) FAILED, rc = "; rc
else
	if( environ( "FBCPARSER_QUIET" ) = "" ) then
		hPrintResult( res )
	end if
	print "fbcparser_scan( """; testfile; """ ) rc = "; rc; ", symbols: "; _
		str( res->symbolCount ); ", diags: "; str( res->diagCount )

	if( environ( "FBCPARSER_CHECKREFS" ) <> "" ) then
		hCheckRefs( res, testfile )
	end if

	if( environ( "FBCPARSER_UNUSED" ) <> "" ) then
		hSummarizeUnused( res )
		if( environ( "FBCPARSER_UNUSED_LIST" ) <> "" ) then
			hListUnused( res, valint( environ( "FBCPARSER_UNUSED_LIST" ) ) )
		end if
	end if

	'' re-entrancy check: a second scan must succeed and produce the same
	'' counts and pool size as the first
	if( environ( "FBCPARSER_TWICE" ) <> "" ) then
		dim as FBCP_RESULT ptr res2 = 0
		opts.flags = 0                      '' second scan silent
		dim as long rc2 = fbcparser_scan( @wfile, @opts, @res2 )
		if( rc2 <> FBCP_OK ) then
			print "REENTRANCY FAILED: second scan rc = "; rc2
		elseif( (res2->symbolCount <> res->symbolCount) orelse _
		        (res2->diagCount <> res->diagCount) orelse _
		        (res2->fileCount <> res->fileCount) orelse _
		        (res2->poolChars <> res->poolChars) ) then
			print "REENTRANCY MISMATCH: scan2 symbols="; str( res2->symbolCount ); _
				" diags="; str( res2->diagCount ); " files="; str( res2->fileCount ); _
				" pool="; str( res2->poolChars )
		else
			print "REENTRANCY OK: second scan matches ("; str( res2->symbolCount ); _
				" symbols, "; str( res2->diagCount ); " diags)"
		end if
		fbcparser_free( res2 )
	end if

	'' buffer-scan check: the same content scanned from memory (with the
	'' file's own path as virtual name) must produce a byte-identical result
	if( environ( "FBCPARSER_TEXTMODE" ) <> "" ) then
		dim as string text = hLoadFile( testfile )
		if( len( text ) = 0 ) then
			print "TEXTMODE: failed to load "; testfile
		else
			dim as FBCP_RESULT ptr rest = 0
			opts.flags = 0                  '' silent
			dim as long rct = fbcparser_scan_text( strptr( text ), @wfile, @opts, @rest )
			if( rct <> FBCP_OK ) then
				print "TEXTMODE FAILED: rc = "; rct
			elseif( (rest->symbolCount <> res->symbolCount) orelse _
			        (rest->diagCount <> res->diagCount) orelse _
			        (rest->fileCount <> res->fileCount) orelse _
			        (rest->poolChars <> res->poolChars) orelse _
			        (hMemEq( rest->symbols, res->symbols, _
			                 res->symbolCount * sizeof( FBCP_SYMBOL ) ) = 0) orelse _
			        (hMemEq( rest->stringPool, res->stringPool, _
			                 res->poolChars * 2 ) = 0) ) then
				print "TEXTMODE MISMATCH: buffer scan symbols="; _
					str( rest->symbolCount ); " diags="; str( rest->diagCount ); _
					" pool="; str( rest->poolChars )
			else
				print "TEXTMODE OK: buffer scan is byte-identical to the file scan ("; _
					str( rest->symbolCount ); " symbols, "; str( rest->diagCount ); " diags)"
			end if
			if( rct = FBCP_OK ) then
				fbcparser_free( rest )
			end if
		end if
	end if

	'' leak detector: scan repeatedly, watch the working set
	dim as string sloop = environ( "FBCPARSER_LOOP" )
	if( len( sloop ) > 0 ) then
		dim as integer nloops = valint( sloop )
		dim as longint ws0 = hWorkingSetKb( )
		dim as integer mismatches = 0
		opts.flags = 0                      '' silent
		for i as integer = 2 to nloops
			dim as FBCP_RESULT ptr resn = 0
			if( fbcparser_scan( @wfile, @opts, @resn ) <> FBCP_OK ) then
				print "LOOP FAILED at scan "; i
				exit for
			end if
			if( (resn->symbolCount <> res->symbolCount) orelse _
			    (resn->diagCount <> res->diagCount) ) then
				mismatches += 1
			end if
			fbcparser_free( resn )
		next
		dim as longint ws1 = hWorkingSetKb( )
		print "LOOP: "; str( nloops ); " scans, mismatches: "; str( mismatches ); _
			", working set "; str( ws0 ); " -> "; str( ws1 ); " KB ("; _
			str( (ws1 - ws0) \ iif( nloops > 1, nloops - 1, 1 ) ); " KB/scan)"
	end if

	fbcparser_free( res )
end if

'' Keep the console open when run interactively (e.g. from tiko), but let
'' automated runs skip the pause: set FBCPARSER_NOPAUSE=1
if( environ( "FBCPARSER_NOPAUSE" ) = "" ) then
	sleep
end if
