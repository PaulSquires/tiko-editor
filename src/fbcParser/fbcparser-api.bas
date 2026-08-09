'' fbcParser DLL entry points.
''
'' Thin, defensive wrappers around fbc_compile() (fbc.bas). See fbcParser.bi
'' for the API documentation and the threading contract: one scan at a time,
'' always from the same thread. The busy guard below is a safety net against
'' accidental nesting/concurrency, not a synchronization mechanism.

#include once "fbcParser.bi"
#include once "fb.bi"
#include once "fbint.bi"
#include once "collector.bi"

declare function fbc_compile _
	( _
		byref sfilename as string, _
		byval result as FBCP_RESULT ptr, _
		byval scanoptions as FBCP_OPTIONS ptr _
	) as integer

declare function fbc_compile_text _
	( _
		byval pText as zstring ptr, _
		byval result as FBCP_RESULT ptr, _
		byval scanoptions as FBCP_OPTIONS ptr, _
		byval pVirtualName as const zstring ptr = 0 _
	) as integer

dim shared gInScan as integer = FALSE

'' shared validation for both entry points; returns FBCP_OK if the scan may
'' proceed (and has then set the console flag and claimed the busy guard)
private function hBeginScan _
	( _
		byval scanoptions as FBCP_OPTIONS ptr, _
		byval ppResult as FBCP_RESULT ptr ptr _
	) as long

	if( ppResult = NULL ) then
		return FBCP_E_BADARGS
	end if
	*ppResult = NULL

	if( scanoptions <> NULL ) then
		if( scanoptions->version <> FBCP_VERSION ) then
			return FBCP_E_VERSION
		end if
		collSetConsoleOutput( (scanoptions->flags and FBCP_OPTFLAG_CONSOLE) <> 0 )
	else
		collSetConsoleOutput( FALSE )
	end if

	if( gInScan ) then
		return FBCP_E_BUSY
	end if
	gInScan = TRUE
	function = FBCP_OK
end function

public function fbcparser_scan alias "FBCPARSER_SCAN" _
	( _
		byval wszFile as wstring ptr, _
		byval scanoptions as FBCP_OPTIONS ptr, _
		byval ppResult as FBCP_RESULT ptr ptr _
	) as long export

	if( (wszFile = NULL) orelse (len( *wszFile ) = 0) ) then
		if( ppResult <> NULL ) then
			*ppResult = NULL
		end if
		return FBCP_E_BADARGS
	end if

	dim as long rc = hBeginScan( scanoptions, ppResult )
	if( rc <> FBCP_OK ) then
		return rc
	end if

	dim as string fname = *wszFile
	dim as FBCP_RESULT ptr result = callocate( sizeof( FBCP_RESULT ) )

	fbc_compile( fname, result, scanoptions )

	*ppResult = result
	gInScan = FALSE
	function = FBCP_OK
end function

public function fbcparser_scan_text alias "FBCPARSER_SCAN_TEXT" _
	( _
		byval pszText as zstring ptr, _
		byval wszVirtualName as wstring ptr, _
		byval scanoptions as FBCP_OPTIONS ptr, _
		byval ppResult as FBCP_RESULT ptr ptr _
	) as long export

	if( pszText = NULL ) then
		if( ppResult <> NULL ) then
			*ppResult = NULL
		end if
		return FBCP_E_BADARGS
	end if

	dim as long rc = hBeginScan( scanoptions, ppResult )
	if( rc <> FBCP_OK ) then
		return rc
	end if

	'' the virtual name attributes the buffer's symbols and anchors
	'' same-directory #include resolution; optional
	dim as string vname
	if( wszVirtualName <> NULL ) then
		vname = *wszVirtualName
	end if

	dim as FBCP_RESULT ptr result = callocate( sizeof( FBCP_RESULT ) )

	fbc_compile_text( pszText, result, scanoptions, strptr( vname ) )

	*ppResult = result
	gInScan = FALSE
	function = FBCP_OK
end function

public sub fbcparser_free alias "FBCPARSER_FREE" _
	( byval pResult as FBCP_RESULT ptr ) export

	if( pResult = NULL ) then
		exit sub
	end if
	collFreeResult( pResult )
	deallocate( pResult )
end sub
