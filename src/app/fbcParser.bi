'' fbcParser public interface - the structures Tiko receives from a scan.
''
'' This is the ONLY header a host application includes. All data is returned
'' in flat arrays plus one wide-string pool: fixed-size records, one
'' allocation each, no per-symbol string ownership. Offsets are wchar indices
'' into stringPool; every pooled string is null-terminated, so
'' @stringPool[offset] is directly usable as a wstring ptr.
''
'' Location caveats (documented once here):
''  - lineNum/colNum are 1-based; 0 means unknown.
''  - colNum is best-effort: it is 0 for UTF-16/32 source files, for symbols
''    declared via macro expansion (lineNum then points at the invocation
''    site), and for identifiers on an earlier line of a multi-line
''    (_-continued) declaration.
''  - A proc WITH a body reports the implementation: fileIndex/lineNum point
''    at the body (lineNum = first body line, colNum 0) and bodyLine/
''    bodyEndLine give its line range. A declare-only proc points at its
''    DECLARE and has bodyLine/bodyEndLine = 0.
''  - The root file's name is the path exactly as passed to the scan (pass
''    absolute paths); include files are always absolute.
''  - Symbol names (and UDT/enum names inside the type text) are returned in
''    their ORIGINAL source casing. Filenames are uppercased (fbc interns
''    them ucased for include-once bookkeeping; Windows is case-insensitive).
''
'' Reference counts (readCount/writeCount/refFileIndex/refLine) answer "is this
'' symbol used?". They count identifiers that literally appear in the SOURCE
'' TEXT: the counters are driven from the parser, at the point an identifier
'' resolves to a symbol, so nothing the compiler synthesizes is counted.
'' (fbc's own FB_SYMBSTATS_ACCESSED is deliberately NOT used - it is a codegen
'' liveness bit, and a local's own declaration sets it. See collector.bi.)
''
'' A count is only meaningful when FBCP_SYMBFLAG_REFTRACKED is set. Without it
'' the counts are UNKNOWN, not zero-meaning-unused.
''
'' Counting caveats (each is a MISS, i.e. a real use that is not counted, so
'' the risk is calling something unused when it is not):
''  - Counts are per SCAN. A proc called only from another module that this
''    scan never reached reads as unreferenced. A whole-project verdict needs
''    the host to combine every scan that saw the symbol.
''  - A reference inside a macro body is counted, but refLine reports the
''    macro's INVOCATION site (same limitation as colNum above).
''  - `#ifdef X` / `#ifndef X` do NOT count - the preprocessor resolves those
''    without going through the parser's identifier paths. `#if X > 0` DOES
''    count, since it parses a real constant expression.
''  - Inline-ASM operands are counted, but always as READS: an asm write is
''    not distinguishable, and calling it a read is the safe direction.
''  - LET() multi-assignment targets count as reads, not writes.
''  - Labels, GOTO/RESTORE/DATA targets and namespace qualifier segments are
''    not counted at all (those kinds are never reported either).

#ifndef __FBCPARSER_BI__
#define __FBCPARSER_BI__

'' 2: FBCP_SYMBOL grew the four reference fields. The record size changed, so a
''    host built against version 1 strides symbols[] wrong - always rebuild the
''    host against this header, do not mix.
const FBCP_VERSION = 2

enum FBCP_KIND
	FBCP_KIND_SUB = 1
	FBCP_KIND_FUNCTION
	FBCP_KIND_PARAM
	FBCP_KIND_TYPE
	FBCP_KIND_UNION
	FBCP_KIND_FIELD
	FBCP_KIND_ENUM
	FBCP_KIND_ENUMVAL
	FBCP_KIND_VAR
	FBCP_KIND_CONST
	FBCP_KIND_TYPEDEF
	FBCP_KIND_DEFINE
	FBCP_KIND_NAMESPACE
end enum

'' FBCP_SYMBOL.flags
const FBCP_SYMBFLAG_BYREF    = &h0001   '' BYREF param / REF var
const FBCP_SYMBFLAG_SHARED   = &h0002
const FBCP_SYMBFLAG_STATIC   = &h0004
const FBCP_SYMBFLAG_UDTTYPE  = &h0008   '' declared AS a TYPE/UNION (directly or ptr)
const FBCP_SYMBFLAG_ENUMTYPE = &h0010   '' declared AS an ENUM
const FBCP_SYMBFLAG_LOCAL    = &h0020   '' declared inside a proc body
const FBCP_SYMBFLAG_INSTANCE = &h0040   '' implicit THIS param
const FBCP_SYMBFLAG_OPTIONAL = &h0080   '' param with a default value
const FBCP_SYMBFLAG_ARRAY    = &h0100   '' array variable/field, or BYDESC param
const FBCP_SYMBFLAG_REFTRACKED = &h0200 '' readCount/writeCount are MEANINGFUL for
                                        ''   this symbol. When clear they are
                                        ''   UNKNOWN - do not read 0 as "unused".
                                        ''   Set for VAR/PARAM/SUB/FUNCTION/FIELD/
                                        ''   CONST/ENUMVAL; never for DEFINE or
                                        ''   NAMESPACE; cleared on every member of
                                        ''   an overloaded proc set (a call
                                        ''   resolves to the overload HEAD, so the
                                        ''   siblings cannot be told apart).
const FBCP_SYMBFLAG_EXPORT   = &h0400   '' EXPORTed proc - callable from outside
                                        ''   anything this scan can see, so a zero
                                        ''   count says nothing about it

type FBCP_SYMBOL
	kind        as long                 '' FBCP_KIND
	flags       as long                 '' FBCP_SYMBFLAG_*
	nameOffset  as long                 '' into stringPool
	typeOffset  as long                 '' type as text, e.g. "WIDGET PTR"; for a
	                                    ''   TYPE/UNION record: the EXTENDS base
	                                    ''   type's name ("OBJECT" for extends
	                                    ''   object); -1 if n/a
	parentIndex as long                 '' owning proc/TYPE/UNION/ENUM/namespace
	                                    ''   in symbols[]; -1 if top-level
	fileIndex   as long                 '' into fileOffsets[]; -1 if unknown
	lineNum     as long
	colNum      as long
	bodyLine    as long                 '' procs with a body: implementation range
	bodyEndLine as long                 ''   (0 if declare-only)
	readCount   as long                 '' source-text references that READ it
	writeCount  as long                 '' source-text references that WRITE it
	                                    ''   (assignment targets). A symbol with
	                                    ''   writeCount > 0 and readCount = 0 is
	                                    ''   assigned but never read.
	refFileIndex as long                '' FIRST reference, in PARSE order (not
	refLine      as long                ''   lexical order): -1 / 0 if none
end type

type FBCP_DIAG
	errNum      as long                 '' fbc error number
	fileIndex   as long                 '' -1 if unknown
	lineNum     as long
	textOffset  as long                 '' formatted message, into stringPool
end type

type FBCP_RESULT
	version     as long                 '' FBCP_VERSION
	symbolCount as long
	symbols     as FBCP_SYMBOL ptr
	fileCount   as long
	fileOffsets as long ptr             '' filename of file i: @stringPool[fileOffsets[i]]
	diagCount   as long
	diags       as FBCP_DIAG ptr
	poolChars   as long                 '' total wchars in stringPool
	stringPool  as wstring ptr
end type

'' FBCP_OPTIONS.flags
const FBCP_OPTFLAG_CONSOLE = &h0001     '' echo parse errors to the console
                                        '' (off by default - a DLL scan is silent)

type FBCP_OPTIONS
	version      as long                '' must be FBCP_VERSION
	flags        as long                '' FBCP_OPTFLAG_*
	includeCount as long
	includePaths as wstring ptr ptr     '' search paths for #include, or NULL
	defineCount  as long
	defines      as wstring ptr ptr     '' "NAME" or "NAME=VALUE", or NULL
end type

'' fbcparser_scan() return codes
const FBCP_OK        = 0
const FBCP_E_BADARGS = -1               '' NULL file/result ptr
const FBCP_E_VERSION = -2               '' options.version <> FBCP_VERSION
const FBCP_E_BUSY    = -3               '' a scan is already running
const FBCP_E_UNAUTHORIZED = -4          '' the host process is not tiko.exe; the DLL refuses to scan

''
'' fbcparser_scan: parse wszFile (and everything it #includes) and return the
'' declared symbols + diagnostics in *ppResult. Returns FBCP_OK even when the
'' source has errors - check result->diagCount; symbols are then partial.
'' options may be NULL. Free the result with fbcparser_free().
''
'' THREADING CONTRACT: the engine is a single global compiler instance.
'' Exactly one scan may run at a time, and all calls must come from the same
'' thread (serialize scans on one worker thread; a second concurrent call
'' returns FBCP_E_BUSY as a safety net, not as a synchronization mechanism).
''
'' The DLL cannot derive any paths from its host process - pass the include
'' search paths (e.g. C:\dev and the FreeBASIC inc directory) in options.
''
''
'' fbcparser_scan_text: like fbcparser_scan, but the root source comes from
'' pszText - an already-loaded, null-terminated buffer (e.g. the editor's
'' unsaved buffer) - so nothing is opened or read for the root module.
'' The text is treated as ASCII/ANSI; a leading UTF-8 BOM is tolerated.
'' wszVirtualName (may be NULL) is the filename the buffer's symbols are
'' attributed to AND the anchor for same-directory #include resolution -
'' pass the real path of the file being edited. #includes are still read
'' from disk. The buffer only needs to stay valid for the duration of the
'' call. Same threading contract and return codes as fbcparser_scan.
''
#ifdef FBCPARSER_BUILDING_DLL
	'' building the DLL itself (implemented in fbcparser-api.bas)
	declare function fbcparser_scan _
		( _
			byval wszFile as wstring ptr, _
			byval options as FBCP_OPTIONS ptr, _
			byval ppResult as FBCP_RESULT ptr ptr _
		) as long
	declare function fbcparser_scan_text _
		( _
			byval pszText as zstring ptr, _
			byval wszVirtualName as wstring ptr, _
			byval options as FBCP_OPTIONS ptr, _
			byval ppResult as FBCP_RESULT ptr ptr _
		) as long
	declare sub fbcparser_free( byval pResult as FBCP_RESULT ptr )
#else
	'' consuming the DLL
	declare function fbcparser_scan _
		lib "fbcParser" alias "FBCPARSER_SCAN" _
		( _
			byval wszFile as wstring ptr, _
			byval options as FBCP_OPTIONS ptr, _
			byval ppResult as FBCP_RESULT ptr ptr _
		) as long
	declare function fbcparser_scan_text _
		lib "fbcParser" alias "FBCPARSER_SCAN_TEXT" _
		( _
			byval pszText as zstring ptr, _
			byval wszVirtualName as wstring ptr, _
			byval options as FBCP_OPTIONS ptr, _
			byval ppResult as FBCP_RESULT ptr ptr _
		) as long
	declare sub fbcparser_free _
		lib "fbcParser" alias "FBCPARSER_FREE" _
		( byval pResult as FBCP_RESULT ptr )
#endif

#endif '' __FBCPARSER_BI__
