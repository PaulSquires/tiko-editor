'' main module, front-end
''
'' chng: sep/2004 written [v1ctor]
''       dec/2004 linux support added [lillo]
''       jan/2005 dos support added [DrV]

#include once "fb.bi"
#include once "fbint.bi"
#include once "symb.bi"
#include once "hlp.bi"
#include once "hash.bi"
#include once "list.bi"
#include once "fbcParser.bi"
#include once "collector.bi"

#include once "file.bi"

#if defined( ENABLE_STANDALONE ) and defined( __FB_WIN32__ )
	#define ENABLE_GORC
#endif

enum
	PRINT_HOST
	PRINT_TARGET
	PRINT_X
	PRINT_FBLIBDIR
	PRINT_SHA1
	PRINT_FORK_ID
end enum

type FBC_EXTOPT
	gas         as string
	ld          as string
	gcc         as string
end type

type FBCIOFILE
	'' Input file name (usually *.bas, but also *.rc, *.res, *.xpm)
	srcfile         as string     '' input file

	'' Output .o file
	'' - for modules from the command line this points to a node from
	''   fbc.objlist, see also fbcAddObj()
	'' - for example in hCompileFbctinf(), add temporary FBCIOFILE is used,
	''   with objfile pointing to a string var on stack
	objfile         as string ptr

	'' Whether -o was used to override the default .o file name
	is_custom_objfile   as integer
end type

type FBC_OBJINF
	lang        as FB_LANG
	mt          as integer
end type

type FBCCTX
	'' For command line parsing
	optid               as integer     '' Current option
	lastmodule          as FBCIOFILE ptr '' module for last input file, so the default .o name can be overwritten with a following -o filename
	objfile             as string      '' -o filename waiting for next input file
	backend             as integer     '' FB_BACKEND_* given via -gen, or -1 if -gen wasn't given
	cputype             as integer     '' FB_CPUTYPE_* (-arch's argument), or -1
	cputype_is_native   as integer     '' Whether -arch native was used
	asmsyntax           as integer     '' FB_ASMSYNTAX_* from -asm, or -1 if not given

	emitasmonly         as integer     '' write out FB backend output file only (.asm/.c)
	keepasm             as integer     '' preserve FB backend output file (.asm/.c)
	emitfinalasmonly    as integer     '' write out final .asm file only
	keepfinalasm        as integer     '' preserve final .asm
	keepobj             as integer
	verbose             as integer
	showversion         as integer
	showhelp            as integer
	print               as integer     '' PRINT_* (-print option)

	'' Command line input
	modules             as TLIST    '' FBCIOFILE's for input .bas files
	rcs                 as TLIST    '' FBCIOFILE's for input .rc/.res files
	xpm                 as FBCIOFILE '' .xpm input file
	temps               as TSTRSET  '' Temporary files to delete at shutdown
	objlist             as TLIST    '' Objects from command line and from compilation
	libfiles            as TLIST
	libs                as TSTRSET
	libpaths            as TSTRSET
	excludedlibs        as TSTRSET  '' lib names explicitly excluded via -nodeflib option(s)

	'' Final list of libs and paths for linking
	'' (each module can have #inclibs and #libpaths and add more, and for
	'' objinfo emitting only the module-specific libs are wanted, so there
	'' are multiple lists necessary to allow each module to start fresh
	'' with the same input libs)
	finallibs           as TSTRSET
	finallibpaths       as TSTRSET

	outname             as zstring * FB_MAXPATHLEN+1
	mainname            as zstring * FB_MAXPATHLEN+1
	entry               as zstring * FB_MAXNAMELEN+1
	mainset             as integer
	mapfile             as zstring * FB_MAXPATHLEN+1
	subsystem           as zstring * FB_MAXNAMELEN+1
	extopt              as FBC_EXTOPT
#ifndef ENABLE_STANDALONE
	target              as zstring * FB_MAXNAMELEN+1  '' Target system identifier (e.g. a name like "win32", or a GNU triplet) to prefix in front of cross-compiling tool names
	targetprefix        as zstring * FB_MAXNAMELEN+1  '' same, but with "-" appended, if there was a target id given; otherwise empty.
#endif
	sysroot             as zstring * FB_MAXPATHLEN+1
	xbe_title           as zstring * FB_MAXNAMELEN+1  '' For the '-title <title>' xbox option
	nodeflibs           as integer
	nofbrt0             as integer  '' If we should exclude fbrt0.o or fbrt0pic.o (implied by nodeflibs, and optional by -nolib fbrt0.o,fbrt0pic.o)
	staticlink          as integer
	stripsymbols        as integer

	'' Compiler paths
	prefix              as zstring * FB_MAXPATHLEN+1  '' Path from -prefix or empty
	binpath             as zstring * FB_MAXPATHLEN+1  '' standalone=prefix/bin/target/  normal=prefix/<fbname>/(target|build)prefix
	incpath             as zstring * FB_MAXPATHLEN+1  '' standalone=prefix/inc          normal=prefix/include/<fbname>
	libpath             as zstring * FB_MAXPATHLEN+1  '' standalone=prefix/lib/target   normal=prefix/lib[64]/<fbname>/target

	'' Tool prefix
	buildprefix         as zstring * FB_MAXPATHLEN+1  '' command line option to override target prefix (affects tool names executed)

	objinf              as FBC_OBJINF
end type

enum FBCTOOL
	FBCTOOL_NONE = 0
	FBCTOOL_AS
	FBCTOOL_AR
	FBCTOOL_LD
	FBCTOOL_GCC
	FBCTOOL_LLC
	FBCTOOL_CLANG
	FBCTOOL_DLLTOOL
	FBCTOOL_GORC
	FBCTOOL_WINDRES
	FBCTOOL_CXBE
	FBCTOOL_DXEGEN
	FBCTOOL_EMAS
	FBCTOOL_EMAR
	FBCTOOL_EMLD
	FBCTOOL_EMCC
	FBCTOOL__COUNT
end enum

enum FBCTOOLFLAG
	FBCTOOLFLAG_INVALID            = 0  '' tool is disabled
	FBCTOOLFLAG_ASSUME_EXISTS      = 1  '' assume the tool exists
	FBCTOOLFLAG_CAN_USE_ENVIRON    = 2  '' allow path to tool to specified by environment variable
	FBCTOOLFLAG_FOUND              = 4  '' tool was checked for
	FBCTOOLFLAG_RELYING_ON_SYSTEM  = 8  '' tool is expected to be on system PATH

	FBCTOOLFLAG_DEFAULT = FBCTOOLFLAG_ASSUME_EXISTS or FBCTOOLFLAG_CAN_USE_ENVIRON
end enum

type FBCTOOLINFO
	name as zstring * 16                  '' default name of tool to invoke
	env_variable as zstring * 16          '' environment variable to override
	flags as FBCTOOLFLAG
	path as zstring * (FB_MAXPATHLEN + 1) '' cached tool path and name
end type

#define fbctoolGetFlags( tool, f )   ((fbctoolTB( tool ).flags and (f)) <> 0)
#define fbctoolSetFlags( tool, f )   fbctoolTB( tool ).flags or= f
#define fbctoolUnsetFlags( tool, f ) fbctoolTB( tool ).flags and= not f

'' must be same order as enum FBCTOOL
static shared as FBCTOOLINFO fbctoolTB(0 to FBCTOOL__COUNT-1) = _
{ _
	/' FBCTOOL_NONE    '/ ( ""       , ""       , FBCTOOLFLAG_INVALID  ), _
	/' FBCTOOL_AS      '/ ( "as"     , "AS"     , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_AR      '/ ( "ar"     , "AR"     , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_LD      '/ ( "ld"     , "LD"     , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_GCC     '/ ( "gcc"    , "GCC"    , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_LLC     '/ ( "llc"    , "LLC"    , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_CLANG   '/ ( "clang"  , "CLANG"  , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_DLLTOOL '/ ( "dlltool", "DLLTOOL", FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_GORC    '/ ( "GoRC"   , "GORC"   , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_WINDRES '/ ( "windres", "WINDRES", FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_CXBE    '/ ( "cxbe"   , "CXBE"   , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_DXEGEN  '/ ( "dxe3gen", "DXEGEN" , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_EMAS    '/ ( "emcc"   , "EMAS"   , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_EMAR    '/ ( "emar"   , "EMAR"   , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_EMLD    '/ ( "emcc"   , "EMLD"   , FBCTOOLFLAG_DEFAULT  ), _
	/' FBCTOOL_EMCC    '/ ( "emcc"   , "EMCC"   , FBCTOOLFLAG_DEFAULT  )  _
}

declare sub fbcFindBin _
	( _
		byval tool as integer, _
		byref path as string _
	)

declare sub hPrintVersion( byval verbose as integer )

#macro safeKill(f)
	if( kill( f ) <> 0 ) then
	end if
#endmacro

dim shared as FBCCTX fbc

'' fbcParser: FBCIOFILE nodes own their srcfile string (objfile points into
'' fbc.objlist, which is freed separately)
private sub hFreeIoFileList( byval list as TLIST ptr )
	dim as FBCIOFILE ptr m = listGetHead( list )
	while( m )
		m->srcfile = ""
		m = listGetNext( m )
	wend
	listEnd( list )
end sub

private sub fbcInit( )
	const FBC_INITFILES = 64

	'' fbcParser: repeated scans re-enter here - free the previous scan's
	'' lists first (upstream ran once per process and leaked them on exit)
	static as integer inited
	if( inited ) then
		hFreeIoFileList( @fbc.modules )
		hFreeIoFileList( @fbc.rcs )
		strsetEnd( @fbc.temps )
		strlistEnd( @fbc.objlist )
		strlistEnd( @fbc.libfiles )
		strsetEnd( @fbc.libs )
		strsetEnd( @fbc.libpaths )
		strsetEnd( @fbc.excludedlibs )
		strsetEnd( @fbc.finallibs )
		strsetEnd( @fbc.finallibpaths )
	end if
	inited = TRUE

	fbc.backend = -1
	fbc.cputype = -1
	fbc.asmsyntax = -1

	listInit( @fbc.modules, FBC_INITFILES, sizeof(FBCIOFILE) )
	listInit( @fbc.rcs, FBC_INITFILES\4, sizeof(FBCIOFILE) )
	strsetInit( @fbc.temps, FBC_INITFILES\4 )
	strlistInit( @fbc.objlist, FBC_INITFILES )
	strlistInit( @fbc.libfiles, FBC_INITFILES\4 )
	strsetInit( @fbc.libs, FBC_INITFILES\4 )
	strsetInit( @fbc.libpaths, FBC_INITFILES\4 )
	strsetInit( @fbc.excludedlibs, FBC_INITFILES\4 )

	strsetInit(@fbc.finallibs, FBC_INITFILES\2)
	strsetInit(@fbc.finallibpaths, FBC_INITFILES\2)

	fbGlobalInit()

#ifdef ENABLE_STRIPALL
	fbc.stripsymbols = TRUE
#endif

	fbc.objinf.lang = fbGetOption( FB_COMPOPT_LANG )

	fbc.print = -1
end sub

private sub fbcEnd( byval errnum as integer )
	'' Clean up temporary files
	dim as TSTRSETITEM ptr file = listGetHead(@fbc.temps.list)
	while( file )
		safeKill( file->s )
		file = listGetNext( file )
	wend

'	end errnum
end sub

private sub fbcAddTemp(byref file as string)
	strsetAdd(@fbc.temps, file, 0)
end sub

private sub fbcRemoveTemp(byref file as string)
	strsetDel(@fbc.temps, file)
end sub

private function fbcAddObj( byref file as string ) as string ptr
	'' .o's should be linked/archived in the order they were found on
	'' command line, so callers of this function must take care to preserve
	'' the order...
	dim as string ptr s = listNewNode( @fbc.objlist )
	*s = file
	function = s
end function

private sub hFatalInvalidOption _
	( _
		byref arg as string, _
		byval is_source as integer _
	)
	'' if 'is_source' then show a line number, otherwise it's the actual command line and line number is undefined
	errReportEx( FB_ERRMSG_INVALIDCMDOPTION, QUOTE + arg + QUOTE, iif( is_source, 0, -1 ) )
	fbcEnd( 1 )
end sub

private sub hCheckWaitingObjfile( )
	if( len( fbc.objfile ) > 0 ) then
		errReportEx( FB_ERRMSG_OBJFILEWITHOUTINPUTFILE, "-o " & fbc.objfile, -1 )
		fbc.objfile = ""
	end if
end sub

private sub hSetIofile _
	( _
		byval module as FBCIOFILE ptr, _
		byref srcfile as string, _
		byval is_rc as integer _
	)

	module->srcfile = srcfile

	'' No objfile name set yet (from the -o <file> option)?
	if( len( fbc.objfile ) = 0 ) then
		module->is_custom_objfile = FALSE

		'' Choose default *.o name based on input file name
		if( is_rc ) then
#ifdef ENABLE_GORC
			'' GoRC only accepts *.obj
			'' foo.rc -> foo.obj, so there is no collision with foo.bas' foo.o
			fbc.objfile = hStripExt( srcfile ) + ".obj"
#else
			'' windres doesn't care, so we use the default *.o
			'' foo.rc -> foo.rc.o to avoid collision with foo.bas' foo.o
			fbc.objfile = srcfile + ".o"
#endif
		else
			'' foo.bas -> foo.o
			fbc.objfile = hStripExt( srcfile ) + ".o"
		end if

		'' Since there was no preceding -o for this module, allow
		'' -o <file> to follow later
		fbc.lastmodule = module
	else
		module->is_custom_objfile = TRUE
	end if

	module->objfile = fbcAddObj( fbc.objfile )
	fbc.objfile = ""

end sub

private sub hAddBas( byref basfile as string )
	hSetIofile( listNewNode( @fbc.modules ), basfile, FALSE )
end sub

type FBGNUOSINFO
	gnuid       as zstring ptr  '' Part of GNU triplet identifying a certain OS
	os          as integer      '' Corresponding FB_COMPTARGET_*
end type

type FBGNUARCHINFO
	gnuid       as zstring ptr  '' Part of GNU triplet identifying a certain architecture
	cputype     as integer      '' Corresponding FB_CPUTYPE_*
end type

'' OS name strings recognized when parsing GNU triplets (-target option)
dim shared as FBGNUOSINFO gnuosmap(0 to ...) => _
{ _
	(@"android"    , FB_COMPTARGET_ANDROID  ), _ '' Must appear before linux
	(@"linux"      , FB_COMPTARGET_LINUX    ), _
	(@"mingw"      , FB_COMPTARGET_WIN32    ), _
	(@"djgpp"      , FB_COMPTARGET_DOS      ), _
	(@"msdosdjgpp" , FB_COMPTARGET_DOS      ), _
	(@"cygwin"     , FB_COMPTARGET_CYGWIN   ), _
	(@"darwin"     , FB_COMPTARGET_DARWIN   ), _
	(@"freebsd"    , FB_COMPTARGET_FREEBSD  ), _
	(@"dragonfly"  , FB_COMPTARGET_DRAGONFLY), _
	(@"solaris"    , FB_COMPTARGET_SOLARIS  ), _
	(@"netbsd"     , FB_COMPTARGET_NETBSD   ), _
	(@"openbsd"    , FB_COMPTARGET_OPENBSD  ), _
	(@"xbox"       , FB_COMPTARGET_XBOX     )  _
}

'' Architectures recognized when parsing GNU triplets (-target option)
dim shared as FBGNUARCHINFO gnuarchmap(0 to ...) => _
{ _
	(@"i386"       , FB_CPUTYPE_386            ), _
	(@"i486"       , FB_CPUTYPE_486            ), _
	(@"i586"       , FB_CPUTYPE_586            ), _
	(@"i686"       , FB_CPUTYPE_686            ), _
	(@"x86"        , FB_DEFAULT_CPUTYPE_X86    ), _
	(@"x86_64"     , FB_DEFAULT_CPUTYPE_X86_64 ), _
	(@"amd64"      , FB_DEFAULT_CPUTYPE_X86_64 ), _
	(@"armv5te"    , FB_CPUTYPE_ARMV5TE        ), _
	(@"armv6"      , FB_CPUTYPE_ARMV6          ), _
	(@"armv6+fp"   , FB_CPUTYPE_ARMV6_FP       ), _
	(@"armv7a"     , FB_CPUTYPE_ARMV7A         ), _
	(@"armv7a+fp"  , FB_CPUTYPE_ARMV7A_FP      ), _
	(@"arm"        , FB_DEFAULT_CPUTYPE_ARM    ), _
	(@"aarch64"    , FB_DEFAULT_CPUTYPE_AARCH64), _
	(@"ppc"        , FB_DEFAULT_CPUTYPE_PPC    ), _
	(@"powerpc"    , FB_DEFAULT_CPUTYPE_PPC    ), _
	(@"ppc64  "    , FB_DEFAULT_CPUTYPE_PPC64  ), _
	(@"powerpc64"  , FB_DEFAULT_CPUTYPE_PPC64  ),  _
	(@"ppc64le  "  , FB_DEFAULT_CPUTYPE_PPC64LE), _
	(@"powerpc64le", FB_DEFAULT_CPUTYPE_PPC64LE)  _
}

'' Identify OS (FB_COMPTARGET_*) and architecture (FB_CPUTYPE_*) in a GNU
'' triplet string (gcc toolchain target name).
private sub hParseGnuTriplet _
	( _
		byref arg as string, _
		byval separator as integer, _
		byref os as integer, _
		byref cputype as integer _
	)
	dim arch as string

	'' Search for OS, it be anywere in the triplet:
	''    mingw32              -> mingw
	''    arm-linux-gnueabihf  -> linux
	''    arm-linux-androideabi-> android
	''    i686-w64-mingw32     -> mingw
	''    i686-pc-linux-gnu    -> linux
	''    i386-pc-msdosdjgpp   -> dos386
	''    i486-pc-msdosdjgpp   -> dos486
	''    i586-pc-msdosdjgpp   -> dos586
	for i as integer = 0 to ubound( gnuosmap )
		if( instr( arg, *gnuosmap(i).gnuid ) > 0 ) then
			os = gnuosmap(i).os
			exit for
		end if
	next

	'' If the triplet has at least two components (<arch>-<...>),
	'' extract the first (the architecture) and try to identify it.
	if( separator > 0 ) then
		arch = left( arg, separator - 1 )
		for i as integer = 0 to ubound( gnuarchmap )
			if( arch = *gnuarchmap(i).gnuid ) then
				cputype = gnuarchmap(i).cputype
				exit for
			end if
		next
	end if

end sub

function fbCpuTypeFromGNUArchInfo( byref arch as string ) as integer
	for i as integer = 0 to ubound( gnuarchmap )
		if( arch = *gnuarchmap(i).gnuid ) then
			return gnuarchmap(i).cputype
		end if
	next
	return -1
end function

type FBOSARCHINFO
	targetid    as zstring ptr  '' -target option argument
	os          as integer      '' FB_COMPTARGET_*
	cputype     as integer      '' FB_CPUTYPE_*
end type

'' Simple free-form arguments accepted by -target option
dim shared as FBOSARCHINFO fbosarchmap(0 to ...) => _
{ _
	_ '' win32/win64 refer to specific OS/arch combinations
	(@"win32"  , FB_COMPTARGET_WIN32  , FB_DEFAULT_CPUTYPE_X86   ), _
	(@"win64"  , FB_COMPTARGET_WIN32  , FB_DEFAULT_CPUTYPE_X86_64), _
	_ '' dragonfly is 64 bit only
	(@"dragonfly", FB_COMPTARGET_DRAGONFLY, FB_DEFAULT_CPUTYPE_X86_64), _
	_ '' solaris is 64 bit only
	(@"solaris", FB_COMPTARGET_SOLARIS, FB_DEFAULT_CPUTYPE_X86_64), _
	_
	_ '' OS given without arch, using the default arch, except for dos/xbox
	_ ''  which only work with x86, so we can always default to x86 for them.
	_ '' (these are supported for backwards compatibility with x86-only FB)
	_ '' When targetting android assume cross-compiling.
	_ '' armv7a is the default arch for android ndk r11 and later
	(@"dos"    , FB_COMPTARGET_DOS    , FB_DEFAULT_CPUTYPE_X86   ), _
	(@"xbox"   , FB_COMPTARGET_XBOX   , FB_DEFAULT_CPUTYPE_X86   ), _
	(@"cygwin" , FB_COMPTARGET_CYGWIN , FB_DEFAULT_CPUTYPE       ), _
	(@"darwin" , FB_COMPTARGET_DARWIN , FB_DEFAULT_CPUTYPE       ), _
	(@"freebsd", FB_COMPTARGET_FREEBSD, FB_DEFAULT_CPUTYPE       ), _
	(@"linux"  , FB_COMPTARGET_LINUX  , FB_DEFAULT_CPUTYPE       ), _
	(@"android", FB_COMPTARGET_ANDROID, FB_CPUTYPE_ARMV7A        ), _
	(@"netbsd" , FB_COMPTARGET_NETBSD , FB_DEFAULT_CPUTYPE       ), _
	(@"openbsd", FB_COMPTARGET_OPENBSD, FB_DEFAULT_CPUTYPE       )  _
}

''
'' Parse the -target option's argument.
''
'' Examples:
''    -target win32           ->    Windows + default x86 arch
''    -target win64           ->    Windows + x86_64
''    -target dos             ->    DOS + x86
''    -target linux           ->    Linux + default arch
''    -target linux-x86       ->    Linux + default x86 arch
''    -target linux-x86_64    ->    Linux + x86_64
''    -target android         ->    Android + ARMv7a (the default ARM)
''    ...
''
'' The normal (non-standalone) build also accepts GNU triplets:
'' (the rough format is <arch>-<vendor>-<os> but it can vary a lot)
''    -target i686-pc-linux-gnu        ->    Linux + i686
''    -target arm-linux-gnueabihf      ->    Linux + default ARM arch
''    -target arm-android              ->    android-arm, armv7-a, 32bit
''    -target android -arch armv5      ->    android-arm, armv5te, 32bit
''    -target armv5te-linux-android    ->    android-arm, armv5te, 32bit
''    -target android -arch armv7      ->    android-arm, armv7-a, 32bit
''    -target arm-linux-android        ->    android-arm, armv7-a, 32bit
''    -target armv7a-linux-android     ->    android-arm, armv7-a, 32bit
''    -target armv7a-linux-androideabi ->    android-arm, armv7-a, 32bit
''    -target i686-linux-android       ->    android-x86, 686, 32bit
''    -target x86_64-w64-mingw32       ->    Windows + x86_64
''    ...
''
'' The normal build uses the -target argument as prefix for binutils/gcc tools.
'' This allows fbc to work well with gcc/binutils cross-compiling toolchains,
'' for example: -target i686-pc-mingw32 causes it to use i686-pc-mingw32-ld
'' instead of the native ld.
''
'' Something like -target win32 is mostly useful for the standalone build, where
'' binutils/gcc tools are arranged into the bin/win32/ld.exe etc. directory
'' layout. -target win32 is less useful for the normal build, because typically
'' binutils/gcc toolchains for cross-compiling to Windows are named something
'' like "i686-pc-mingw32", not just "win32". Nevertheless, even the normal build
'' should accept these options -- it can be useful for debugging purposes or
'' with the -print option. Furthermore, people (unnecessarily) specify -target
'' for native compilation (e.g. using -target linux on Linux), and this supports
'' that.
''
'' This function should just do parsing, without any validation. It would be
'' nice if
''    -target linux-x86_64
'' would just be the same as
''    -target linux -arch x86_64
'' Thus, any validation should be done later when the command line has been
'' parsed completely.
''
'' It's up to the caller to report an error if only one of OS/arch (but not
'' both) could be identified.
''
private sub hParseTargetArg _
	( _
		byref arg as string, _
		byref os as integer, _
		byref cputype as integer, _
		byref is_gnu_triplet as integer _
	)

	os = -1
	cputype = -1
	is_gnu_triplet = FALSE

	'' Case-insensitive so "-target WIN32" etc. works too
	var lcasearg = lcase( arg )

	'' Check for simple arguments (dos, linux, win32, etc.)
	for i as integer = 0 to ubound( fbosarchmap )
		if( lcasearg = *fbosarchmap(i).targetid ) then
			os = fbosarchmap(i).os
			cputype = fbosarchmap(i).cputype
			exit sub
		end if
	next

	'' <os>-<cpufamily>
	var separator = instr( arg, "-" )
	if( separator > 0 ) then
		os = fbIdentifyOs( left( lcasearg, separator - 1 ) )
		cputype = fbDefaultCpuTypeFromCpuFamilyId( os, right( lcasearg, len( lcasearg ) - separator ) )

		'' allow normalizing on gnu arch types to determine the standalone targetid
		#ifdef ENABLE_STANDALONE
			if( (os < 0) and (cputype < 0) ) then
				cputype = fbCpuTypeFromGNUArchInfo( right( lcasearg, len( lcasearg ) - separator ) )
			end if
		#endif
	end if

	'' Normal build: Check for GNU triplets, if the above checks failed.
	#ifndef ENABLE_STANDALONE
		if( (os < 0) and (cputype < 0) ) then
			hParseGnuTriplet( arg, separator, os, cputype )
			is_gnu_triplet = TRUE
		end if
	#else
		if( (os < 0) and (cputype < 0) ) then
			hParseGnuTriplet( arg, separator, os, cputype )
		end if
	#endif
end sub

enum
	OPT_A = 0
	OPT_ARCH
	OPT_ASM
	OPT_B
	OPT_BUILDPREFIX
	OPT_C
	OPT_CKEEPOBJ
	OPT_D
	OPT_DLL
	OPT_DYLIB
	OPT_E
	OPT_EARRAY
	OPT_EARRAYDIMS
	OPT_EASSERT
	OPT_EDEBUG
	OPT_EDEBUGINFO
	OPT_ELOCATION
	OPT_ENULLPTR
	OPT_EUNWIND
	OPT_ENTRY
	OPT_EX
	OPT_EXX
	OPT_EXPORT
	OPT_FBGFX
	OPT_FORCELANG
	OPT_FPMODE
	OPT_FPU
	OPT_G
	OPT_GEN
	OPT_HELP
	OPT_I
	OPT_INCLUDE
	OPT_L
	OPT_LANG
	OPT_LIB
	OPT_M
	OPT_MAP
	OPT_MAXERR
	OPT_MT
	OPT_NODEFLIBS
	OPT_NOERRLINE
	OPT_NOLIB
	OPT_NOOBJINFO
	OPT_NOSTRIP
	OPT_O
	OPT_OPTIMIZE
	OPT_P
	OPT_PIC
	OPT_PP
	OPT_PREFIX
	OPT_PRINT
	OPT_PROFGEN
	OPT_PROFILE
	OPT_R
	OPT_RKEEPASM
	OPT_RR
	OPT_RRKEEPASM
	OPT_S
	OPT_SHOWINCLUDES
	OPT_STATIC
	OPT_STRIP
	OPT_SYSROOT
	OPT_T
	OPT_TARGET
	OPT_TITLE
	OPT_V
	OPT_VEC
	OPT_VERSION
	OPT_W
	OPT_WA
	OPT_WC
	OPT_WL
	OPT_X
	OPT_Z
	OPT__COUNT
end enum

type FBC_CMDLINE_OPTION
	takes_argument as boolean          '' true = option requires argument
	allowed_in_source as boolean       '' true = can be used with #cmdline directive
	parser_restart as boolean          '' true = restart of parser is required when used with #cmdline directive
	fbc_restart as integer             '' true = major restart of fbc required
end type

dim shared as FBC_CMDLINE_OPTION cmdlineOptionTB(0 to (OPT__COUNT - 1)) = _
{ _
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_A            add files to link, affects link
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_ARCH         affects major initialization
	( TRUE , TRUE , FALSE, TRUE  ), _ '' OPT_ASM          affects major initialization,affects second stage compile
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_B            adds files to compile
	( TRUE , TRUE , FALSE, TRUE  ), _ '' OPT_BUILDPREFIX  affects tools executed (fbcSetupCompilerPaths(), so restart is required)
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_C            affects code generation / compile / assemble /link process
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_CKEEPOBJ     affects removal of temporary files
	( TRUE , TRUE , FALSE, TRUE  ), _ '' OPT_D            add symbols to current source also, not just the preDefines, affects global defines
	( FALSE, TRUE , TRUE , TRUE  ), _ '' OPT_DLL          affects major initialization, affects output format
	( FALSE, TRUE , TRUE , TRUE  ), _ '' OPT_DYLIB        affects major initialization, affects output format
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_E            affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EARRAY       affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EARRAYDIMS   affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EASSERT      affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EDEBUG       affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EDEBUGINFO   affects code generation, affects link
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_ELOCATION    affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_ENULLPTR     affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EUNWIND      affects code generation
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_ENTRY        affects major initialization, affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EX           affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EXX          affects code generation
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_EXPORT       affects code generation
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_FBGFX        affects link
	( TRUE , TRUE , TRUE , FALSE ), _ '' OPT_FORCELANG    never allow, command line only
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_FPMODE       affects major initialization, affects code generation
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_FPU          affects major initialization,affects code generation, affects second stage compile, affects link
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_G            affects code generation, affects link
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_GEN          affects major initialization
	( FALSE, FALSE, FALSE, FALSE ), _ '' OPT_HELP         never allow, real command line only, makes no sense to have in source
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_I            add include path before the default one
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_INCLUDE      restart required to inject preInclude
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_L            affects link, same as #inclib
	( TRUE , TRUE , TRUE , FALSE ), _ '' OPT_LANG         affects code generation, affects initialization
	( FALSE, TRUE , TRUE , TRUE  ), _ '' OPT_LIB          affects major initialization, affects output format
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_M            affects major initialization for all modules
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_MAP          affects output files
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_MAXERR       affects compile process
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_MT           affects link, __FB_MT__
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_NODEFLIBS    affects link
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_NOERRLINE    affects compiler output display
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_NOLIB        affects link
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_NOOBJINFO    affects post compile process
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_NOSTRIP      affects link
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_O            affects output file naming with initialization before compile
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_OPTIMIZE     affects link
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_P            affects link, same as #libpath
	( FALSE, TRUE , FALSE, TRUE  ), _ '' OPT_PIC          affects major initialization, affects link
	( FALSE, TRUE , FALSE, TRUE  ), _ '' OPT_PP           affects major initialization
	( TRUE , TRUE , FALSE, TRUE  ), _ '' OPT_PREFIX       affects major initialization
	( TRUE , FALSE, FALSE, FALSE ), _ '' OPT_PRINT        never allow, makes no sense to have in source
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_PROFGEN      affects major initialization, affects initialization, affects code generation, affects link
	( FALSE, TRUE , TRUE , TRUE  ), _ '' OPT_PROFILE      affects major initialization, affects initialization, affects code generation, affects link
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_R            affects compile / assemble / link process, removal of temporary files
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_RKEEPASM     affects removal of temporary files
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_RR           affects compile / assemble / link process, removal of temporary files
	( FALSE, TRUE , TRUE , FALSE ), _ '' OPT_RRKEEPASM    affects removal of temporary files
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_S            affects link
	( FALSE, TRUE , FALSE, TRUE  ), _ '' OPT_SHOWINCLUDES affects compiler output display
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_STATIC       affects link
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_STRIP        affects link
	( TRUE,  TRUE , FALSE, FALSE ), _ '' OPT_SYSROOT      affects link
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_T            affects link
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_TARGET       affects major initialization
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_TITLE        affects link
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_V            affects nothing
	( TRUE , TRUE , TRUE , TRUE  ), _ '' OPT_VEC          affects major initialization, affects code generation
	( FALSE, TRUE , FALSE, FALSE ), _ '' OPT_VERSION      print version information
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_W            affects compiler display output
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_WA           affects assembly
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_WC           affects second stage compile
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_WL           affects link
	( TRUE , TRUE , FALSE, FALSE ), _ '' OPT_X            affects output file
	( TRUE , TRUE , TRUE , TRUE  )  _ '' OPT_Z            affects various - code generation
}

private sub handleOpt _
	( _
		byval optid as integer, _
		byref arg as string, _
		byval is_source as integer _
	)

	select case as const (optid)
	case OPT_A
		fbcAddObj( arg )

	case OPT_ARCH
		'' Set cputype later, so it overrides -target
		fbc.cputype_is_native = (arg = "native")
		fbc.cputype = fbIdentifyFbcArch( arg )
		if( fbc.cputype < 0 ) then
			hFatalInvalidOption( "-arch " + arg, is_source )
		end if

	case OPT_ASM
		select case( arg )
		case "att"
			fbc.asmsyntax = FB_ASMSYNTAX_ATT
		case "intel"
			fbc.asmsyntax = FB_ASMSYNTAX_INTEL
		case else
			hFatalInvalidOption( arg, is_source )
		end select

	case OPT_B
		hAddBas( arg )

	case OPT_BUILDPREFIX
		fbc.buildprefix = arg

	case OPT_C
		'' -c changes the output type from exe/lib/dll to object,
		'' overwriting previous -dll, -lib or the default exe.
		fbSetOption( FB_COMPOPT_OUTTYPE, FB_OUTTYPE_OBJECT )
		fbc.keepobj = TRUE

	case OPT_CKEEPOBJ
		fbc.keepobj = TRUE

	case OPT_D
		fbAddPreDefine(arg)

	case OPT_DLL, OPT_DYLIB
		fbSetOption( FB_COMPOPT_OUTTYPE, FB_OUTTYPE_DYNAMICLIB )

	case OPT_E
		fbSetOption( FB_COMPOPT_ERRORCHECK, TRUE )
		fbSetOption( FB_COMPOPT_UNWINDINFO, TRUE )

	case OPT_EARRAY
		fbSetOption( FB_COMPOPT_ARRAYBOUNDCHECK, TRUE )

	case OPT_EARRAYDIMS
		fbSetOption( FB_COMPOPT_ARRAYDIMSCHECK, TRUE )

	case OPT_EASSERT
		fbSetOption( FB_COMPOPT_ASSERTIONS, TRUE )

	case OPT_EDEBUG
		fbSetOption( FB_COMPOPT_DEBUG, TRUE )

	case OPT_EDEBUGINFO
		fbSetOption( FB_COMPOPT_DEBUGINFO, TRUE )

	case OPT_ELOCATION
		fbSetOption( FB_COMPOPT_ERRLOCATION, TRUE )

	case OPT_ENULLPTR
		fbSetOption( FB_COMPOPT_NULLPTRCHECK, TRUE )

	case OPT_EUNWIND
		fbSetOption( FB_COMPOPT_UNWINDINFO, TRUE )

	case OPT_ENTRY
		fbc.entry = arg

	case OPT_EX
		fbSetOption( FB_COMPOPT_ERRORCHECK, TRUE )
		fbSetOption( FB_COMPOPT_RESUMEERROR, TRUE )
		fbSetOption( FB_COMPOPT_UNWINDINFO, TRUE )

	case OPT_EXX
		fbSetOption( FB_COMPOPT_ERRORCHECK, TRUE )
		fbSetOption( FB_COMPOPT_RESUMEERROR, TRUE )
		fbSetOption( FB_COMPOPT_EXTRAERRCHECK, TRUE )
		fbSetOption( FB_COMPOPT_ERRLOCATION, TRUE )
		fbSetOption( FB_COMPOPT_ARRAYBOUNDCHECK, TRUE )
		fbSetOption( FB_COMPOPT_ARRAYDIMSCHECK, TRUE )
		fbSetOption( FB_COMPOPT_NULLPTRCHECK, TRUE )
		fbSetOption( FB_COMPOPT_UNWINDINFO, TRUE )

	case OPT_EXPORT
		fbSetOption( FB_COMPOPT_EXPORT, TRUE )

	case OPT_FBGFX
		fbSetOption( FB_COMPOPT_FBGFX, TRUE )

	case OPT_FORCELANG
		dim as integer value = fbGetLangId(strptr(arg))
		if( value = FB_LANG_INVALID ) then
			hFatalInvalidOption( arg, is_source )
		end if

		'' show a warning only if forcelang was already set and
		'' we are handling this from a #cmdline.  We'd like that
		'' -forcelang from the real command line takes priority
		'' over source code #cmdline "-forcelang"
		if( is_source and fbGetOption( FB_COMPOPT_FORCELANG ) ) then
			errReportWarn( FB_WARNINGMSG_CMDLINEOVERRIDES )
		else
			fbSetOption( FB_COMPOPT_LANG, value )
			fbSetOption( FB_COMPOPT_FORCELANG, TRUE )
			fbc.objinf.lang = value

			if( is_source ) then
				fbSetOption( FB_COMPOPT_RESTART_LANG, value )
			end if
		end if

	case OPT_FPMODE
		dim as integer value = any

		select case ucase(arg)
		case "PRECISE"
			value = FB_FPMODE_PRECISE
		case "FAST"
			value = FB_FPMODE_FAST
		case else
			hFatalInvalidOption( arg, is_source )
		end select

		fbSetOption( FB_COMPOPT_FPMODE, value )

	case OPT_FPU
		dim as integer value = any

		select case ucase(arg)
		case "X87", "FPU"
			value = FB_FPUTYPE_FPU
		case "SSE"
			value = FB_FPUTYPE_SSE
		case "NEON"
			value = FB_FPUTYPE_NEON
		case else
			hFatalInvalidOption( arg, is_source )
		end select

		fbSetOption( FB_COMPOPT_FPUTYPE, value )

	case OPT_G
		fbSetOption( FB_COMPOPT_DEBUG, TRUE )
		fbSetOption( FB_COMPOPT_DEBUGINFO, TRUE )
		fbSetOption( FB_COMPOPT_ASSERTIONS, TRUE )

	case OPT_GEN
		select case( lcase( arg ) )
		case "gas"
			fbc.backend = FB_BACKEND_GAS
		case "gcc"
			fbc.backend = FB_BACKEND_GCC
		case "clang"
			fbc.backend = FB_BACKEND_CLANG
		case "llvm"
			fbc.backend = FB_BACKEND_LLVM
		Case "gas64"
			fbc.backend = FB_BACKEND_GAS64
		case else
			hFatalInvalidOption( arg, is_source )
		end select

	case OPT_HELP
		fbc.showhelp = TRUE

	case OPT_I
		fbAddIncludePath(pathStripDiv(arg))

	case OPT_INCLUDE
		fbAddPreInclude(arg)

	case OPT_L
		strsetAdd(@fbc.libs, arg, FALSE)

	case OPT_LANG
		dim as integer value = fbGetLangId( strptr(arg) )
		if( value = FB_LANG_INVALID ) then
			hFatalInvalidOption( arg, is_source )
		end if

		'' don't let '-lang' option or #cmdline "-lang" overide -forcelang
		if( fbGetOption( FB_COMPOPT_FORCELANG ) = FALSE ) then
			fbSetOption( FB_COMPOPT_LANG, value )
			fbc.objinf.lang = value

			if( is_source ) then
				fbSetOption( FB_COMPOPT_RESTART_LANG, value )
			endif
		end if

	case OPT_LIB
		fbSetOption( FB_COMPOPT_OUTTYPE, FB_OUTTYPE_STATICLIB )

	case OPT_M
		fbc.mainname = arg
		fbc.mainset = TRUE

	case OPT_MAP
		fbc.mapfile = arg

	case OPT_MAXERR
		dim as integer value = any

		if( arg = "inf" ) then
			value = FB_ERR_INFINITE
		else
			value = clng( arg )
			if( value <= 0 ) then
				hFatalInvalidOption( arg, is_source )
			end if
		end if

		fbSetOption( FB_COMPOPT_MAXERRORS, value )

	case OPT_MT
		fbSetOption( FB_COMPOPT_MULTITHREADED, TRUE )
		fbc.objinf.mt = TRUE

	case OPT_NODEFLIBS
		fbc.nodeflibs = TRUE
		fbc.nofbrt0 = TRUE

	case OPT_NOERRLINE
		fbSetOption( FB_COMPOPT_SHOWERROR, FALSE )

	case OPT_NOLIB
		dim libs() as string
		hSplitStr(arg, ",", libs())
		for i as integer = lbound(libs) to ubound(libs)
			if len(libs(i)) > 0 then
				strsetAdd(@fbc.excludedlibs, libs(i), 0 /'unused userdata'/)
			end if
		next

	case OPT_NOOBJINFO
		fbSetOption( FB_COMPOPT_OBJINFO, FALSE )

	case OPT_NOSTRIP
		fbc.stripsymbols = FALSE

	case OPT_O
		'' Error if there already is an -o waiting to be assigned
		hCheckWaitingObjfile( )

		'' Assign it to the last module, if it doesn't have an
		'' -o filename yet, or store it for later otherwise.
		if( fbc.lastmodule ) then
			*fbc.lastmodule->objfile = arg
			fbc.lastmodule->is_custom_objfile = TRUE
		else
			fbc.objfile = arg
		end if

	case OPT_OPTIMIZE
		dim as integer value = any

		if (arg = "max") then
			value = 3
		else
			value = clng( arg )
			if (value < 0) then
				value = 0
			elseif (value > 3) then
				value = 3
			end if
		end if

		fbSetOption( FB_COMPOPT_OPTIMIZELEVEL, value )

	case OPT_P
		strsetAdd(@fbc.libpaths, pathStripDiv(arg), FALSE)

	case OPT_PIC
		fbSetOption( FB_COMPOPT_PIC, TRUE )

	case OPT_PP
		'' -pp doesn't change the output type, but like -r we want to
		'' stop fbc very early.
		fbSetOption( FB_COMPOPT_PPONLY, TRUE )
		fbc.emitasmonly = TRUE

	case OPT_PREFIX
		fbc.prefix = pathStripDiv(arg)
		hReplaceSlash( fbc.prefix, asc( FB_HOST_PATHDIV ) )

	case OPT_PRINT
		select case( arg )
		case "host"   : fbc.print = PRINT_HOST
		case "target" : fbc.print = PRINT_TARGET
		case "x"      : fbc.print = PRINT_X
		case "fblibdir" : fbc.print = PRINT_FBLIBDIR
		case "sha-1"  : fbc.print = PRINT_SHA1
		case "fork-id": fbc.print = PRINT_FORK_ID
		case else
			hFatalInvalidOption( arg, is_source )
		end select

	case OPT_PROFILE
		fbSetOption( FB_COMPOPT_PROFILE, FB_PROFILE_OPT_GMON )

	case OPT_PROFGEN
		select case( arg )
		case "default", "gmon"
			fbSetOption( FB_COMPOPT_PROFILE, FB_PROFILE_OPT_GMON )
		case "fb"
			fbSetOption( FB_COMPOPT_PROFILE, FB_PROFILE_OPT_CALLS )
		case "cycles"
			fbSetOption( FB_COMPOPT_PROFILE, FB_PROFILE_OPT_CYCLES )
		case else
			hFatalInvalidOption( arg, is_source )
		end select

	case OPT_R
		'' -r changes the output type to .o, like -c, i.e. -m may have
		'' to be used to mark the main module, just like -c.
		fbSetOption( FB_COMPOPT_OUTTYPE, FB_OUTTYPE_OBJECT )
		'' -r will stop fbc earlier than -c though.
		fbc.emitasmonly = TRUE
		fbc.keepasm = TRUE

	case OPT_RKEEPASM
		fbc.keepasm = TRUE

	case OPT_RR
		fbSetOption( FB_COMPOPT_OUTTYPE, FB_OUTTYPE_OBJECT )
		fbc.emitfinalasmonly = TRUE
		fbc.keepfinalasm = TRUE

	case OPT_RRKEEPASM
		fbc.keepfinalasm = TRUE

	case OPT_S
		fbc.subsystem = arg
		select case( arg )
		case "gui"
			fbSetOption( FB_COMPOPT_MODEVIEW, FB_MODEVIEW_GUI )

		end select

	case OPT_SHOWINCLUDES
		fbSetOption( FB_COMPOPT_SHOWINCLUDES, TRUE )

	case OPT_STATIC
		fbc.staticlink = TRUE

	case OPT_STRIP
		fbc.stripsymbols = TRUE

	case OPT_SYSROOT
		fbc.sysroot = arg

	case OPT_T
		fbSetOption( FB_COMPOPT_STACKSIZE, clng( arg ) * 1024 )

	case OPT_TARGET
		dim as integer os, cputype, is_gnu_triplet
		hParseTargetArg( arg, os, cputype, is_gnu_triplet )

		if( (os < 0) or (cputype < 0) ) then
			hFatalInvalidOption( arg, is_source )
		end if

		'' Store the OS/cputype, overwriting the values from any
		'' previous -target options.
		fbSetOption( FB_COMPOPT_TARGET, os )
		fbSetOption( FB_COMPOPT_CPUTYPE, cputype )

		#ifndef ENABLE_STANDALONE
			'' Normal build: Store the original -target argument
			'' for use as prefix for binutils/gcc tools, but only
			'' when cross-compiling or if it's really a GNU triplet.
			if( (os <> FB_DEFAULT_TARGET) or _
				(cputype <> FB_DEFAULT_CPUTYPE) or _
				is_gnu_triplet ) then
				fbc.target = arg
				fbc.targetprefix = fbc.target + "-"
			end if
		#endif

	case OPT_TITLE
		fbc.xbe_title = arg

	case OPT_V
		fbc.verbose = TRUE

	case OPT_VEC
		dim as integer value = any

		select case (ucase(arg))
		case "NONE", "0"
			value = FB_VECTORIZE_NONE
		case "1"
			value = FB_VECTORIZE_NORMAL
		case "2"
			value = FB_VECTORIZE_INTRATREE
		case else
			hFatalInvalidOption( arg, is_source )
		end select

		fbSetOption( FB_COMPOPT_VECTORIZE, value )

	case OPT_VERSION
		if( is_source ) then
			if( fbc.showversion = FALSE ) then
				hPrintVersion( fbc.verbose )
			end if
		end if
		fbc.showversion = TRUE

	case OPT_W
		dim as integer value = FB_WARNINGMSGS_LOWEST_LEVEL - 1

		select case (arg)
		case "all"
			value = FB_WARNINGMSGS_LOWEST_LEVEL

		case "none"
			value = FB_WARNINGMSGS_HIGHEST_LEVEL + 1

		case "param"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_PARAMMODE )

		case "escape"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_ESCSEQ )

		case "next"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_NEXTVAR )

		case "signedness"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_SIGNEDNESS )

		case "constness"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
						fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_CONSTNESS )
			value = FB_WARNINGMSGS_LOWEST_LEVEL

		case "funcptr"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_CASTFUNCPTR )
			value = FB_WARNINGMSGS_LOWEST_LEVEL

		case "suffix"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_SUFFIX )

		case "pedantic"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, FB_PDCHECK_DEFAULT )
			if( value > FB_WARNINGMSGS_DEFAULT_LEVEL ) then
				value = FB_WARNINGMSGS_DEFAULT_LEVEL
			end if

		case "error"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_ERROR )

		case "upcast"
			fbSetOption( FB_COMPOPT_PEDANTICCHK, _
				fbGetOption( FB_COMPOPT_PEDANTICCHK ) or FB_PDCHECK_UPCAST )

		case else
			value = clng( arg )
		end select

		if( value >= FB_WARNINGMSGS_LOWEST_LEVEL ) then
			fbSetOption( FB_COMPOPT_WARNINGLEVEL, value )
		end if

	case OPT_WA
		fbc.extopt.gas += " " + hReplace( arg, ",", " " ) + " "

	case OPT_WC
		fbc.extopt.gcc += " " + hReplace( arg, ",", " " ) + " "

	case OPT_WL
		fbc.extopt.ld += " " + hReplace( arg, ",", " " ) + " "

	case OPT_X
		fbc.outname = arg

	case OPT_Z
		select case( lcase( arg ) )
		case "gosub-setjmp"
			fbSetOption( FB_COMPOPT_GOSUBSETJMP, TRUE )
		case "valist-as-ptr"
			fbSetOption( FB_COMPOPT_VALISTASPTR, TRUE )
		case "no-thiscall"
			fbSetOption( FB_COMPOPT_NOTHISCALL, TRUE )
		case "no-fastcall"
			fbSetOption( FB_COMPOPT_NOFASTCALL, TRUE )
		case "fbrt"
			fbSetOption( FB_COMPOPT_FBRT, TRUE )
		case "nocmdline"
			fbSetOption( FB_COMPOPT_NOCMDLINE, TRUE )
		case "retinflts"
			fbSetOption( FB_COMPOPT_RETURNINFLTS, TRUE )
		case "nobuiltins"
			fbSetOption( FB_COMPOPT_NOBUILTINS, TRUE )
		case "optabstract"
			fbSetOption( FB_COMPOPT_OPTABSTRACT, TRUE )
		case else
			hFatalInvalidOption( arg, is_source )
		end select

	end select
end sub

private function parseOption(byval opt as zstring ptr) as integer
	#macro CHECK(opttext, optid)
		if (*opt = opttext) then
			return optid
		end if
	#endmacro

	#macro ONECHAR(optid)
		if (cptr(ubyte ptr, opt)[1] = 0) then
			return optid
		end if
	#endmacro

	select case as const (cptr(ubyte ptr, opt)[0])
	case asc("a")
		ONECHAR(OPT_A)
		CHECK("arch", OPT_ARCH)
		CHECK("asm", OPT_ASM)

	case asc("b")
		ONECHAR(OPT_B)
		CHECK("buildprefix", OPT_BUILDPREFIX)

	case asc("c")
		ONECHAR(OPT_C)

	case asc("C")
		ONECHAR(OPT_CKEEPOBJ)

	case asc("d")
		ONECHAR(OPT_D)
		CHECK("dll", OPT_DLL)
		CHECK("dylib", OPT_DYLIB)

	case asc("e")
		ONECHAR(OPT_E)
		CHECK("ex", OPT_EX)
		CHECK("earray", OPT_EARRAY)
		CHECK("earraydims", OPT_EARRAYDIMS)
		CHECK("eassert", OPT_EASSERT)
		CHECK("edebug", OPT_EDEBUG)
		CHECK("edebuginfo", OPT_EDEBUGINFO)
		CHECK("elocation", OPT_ELOCATION)
		CHECK("enullptr", OPT_ENULLPTR)
		CHECK("eunwind", OPT_EUNWIND)
		CHECK("entry", OPT_ENTRY)
		CHECK("exx", OPT_EXX)
		CHECK("export", OPT_EXPORT)

	case asc("f")
		CHECK("fbgfx", OPT_FBGFX)
		CHECK("forcelang", OPT_FORCELANG)
		CHECK("fpmode", OPT_FPMODE)
		CHECK("fpu", OPT_FPU)

	case asc("g")
		ONECHAR(OPT_G)
		CHECK("gen", OPT_GEN)

	case asc( "h" )
		CHECK( "help", OPT_HELP )

	case asc("i")
		ONECHAR(OPT_I)
		CHECK("include", OPT_INCLUDE)

	case asc("l")
		ONECHAR(OPT_L)
		CHECK("lang", OPT_LANG)
		CHECK("lib", OPT_LIB)

	case asc("m")
		ONECHAR(OPT_M)
		CHECK("map", OPT_MAP)
		CHECK("maxerr", OPT_MAXERR)
		CHECK("mt", OPT_MT)

	case asc("n")
		CHECK("noerrline", OPT_NOERRLINE)
		CHECK("nodeflibs", OPT_NODEFLIBS)
		CHECK("nolib", OPT_NOLIB)
		CHECK("noobjinfo", OPT_NOOBJINFO)
		CHECK("nostrip", OPT_NOSTRIP)

	case asc("o")
		ONECHAR(OPT_O)

	case asc("O")
		ONECHAR(OPT_OPTIMIZE)

	case asc("p")
		ONECHAR(OPT_P)
		CHECK("pic", OPT_PIC)
		CHECK("pp", OPT_PP)
		CHECK("prefix", OPT_PREFIX)
		CHECK("print", OPT_PRINT)
		CHECK("profile", OPT_PROFILE)
		CHECK("profgen", OPT_PROFGEN)

	case asc("r")
		ONECHAR(OPT_R)
		CHECK("rr", OPT_RR)

	case asc("R")
		ONECHAR(OPT_RKEEPASM)
		CHECK("RR", OPT_RRKEEPASM)

	case asc("s")
		ONECHAR(OPT_S)
		CHECK("showincludes", OPT_SHOWINCLUDES)
		CHECK("static", OPT_STATIC)
		CHECK("strip", OPT_STRIP)
		CHECK("sysroot", OPT_SYSROOT)

	case asc("t")
		ONECHAR(OPT_T)
		CHECK("target", OPT_TARGET)
		CHECK("title", OPT_TITLE)

	case asc("v")
		ONECHAR(OPT_V)
		CHECK("vec", OPT_VEC)
		CHECK("version", OPT_VERSION)

	case asc("w")
		ONECHAR(OPT_W)

	case asc("W")
		CHECK("Wa", OPT_WA)
		CHECK("Wl", OPT_WL)
		CHECK("Wc", OPT_WC)

	case asc("x")
		ONECHAR(OPT_X)

	case asc("z")
		ONECHAR(OPT_Z)

	case asc( "-" )
		CHECK( "-version", OPT_VERSION )
		CHECK( "-help", OPT_HELP )

	end select

	return -1
end function

declare sub parseArgsFromFile _
	( _
		byref filename as string, _
		byval is_source as integer _
	)

private sub handleArg _
	( _
		byref arg as string, _
		byval is_source as integer, _
		byval is_file as integer _
	)
	'' If the previous option wants this argument as parameter,
	'' call the handler with it, now that it's known.
	'' Note: Anything is accepted, even if it starts with '-' or '@'.
	if( fbc.optid >= 0 ) then
		'' Complain about empty next argument
		if (len(arg) = 0) then
			hFatalInvalidOption( arg, is_source )
		end if

		handleOpt( fbc.optid, arg, is_source )
		fbc.optid = -1
		return
	end if

	if (len(arg) = 0) then
		'' Ignore empty argument
		return
	end if

	select case (arg[0])
	case asc("-")
		dim as zstring ptr opt = strptr(arg) + 1

		'' Complain about '-' only
		if (cptr(ubyte ptr, opt)[0] = 0) then
			'' Incomplete command line option
			hFatalInvalidOption( arg, is_source )
		end if

		'' Parse the option after the '-'
		dim as integer optid = parseOption(opt)
		if (optid < 0) then
			'' Unrecognized command line option
			hFatalInvalidOption( arg, is_source )
		end if

		'' Are we in source and option not allowed in source?
		if( is_source ) then
			if( not cmdlineOptionTB( optid ).allowed_in_source ) then
				hFatalInvalidOption( arg, is_source )
			endif
		end if

		'' Does this option take a parameter?
		if( cmdlineOptionTB( optid ).takes_argument ) then
			'' Delay handling it, until the next argument is known.
			fbc.optid = optid
		else
			'' Handle this option now
			handleOpt( optid, arg, is_source )
		end if

		'' even if the handling of the option is delayed, check the restart options here
		if( is_source ) then
			if( cmdlineOptionTB( optid ).parser_restart ) then
				fbRestartBeginRequest( FB_RESTART_PARSER_CMDLINE )
			end if

			if( cmdlineOptionTB( optid ).fbc_restart ) then
				fbRestartBeginRequest( FB_RESTART_FBC_CMDLINE )
			end if
		end if

	case asc("@")
		'' Maximum nesting/recursion level
		const MAX_LEVELS = 128
		static as integer reclevel = 0

		if (reclevel > MAX_LEVELS) then
			'' Options file nesting level too deep (recursion?)
			errReportEx( FB_ERRMSG_RECLEVELTOODEEP, arg, -1 )
			fbcEnd(1)
		end if

		'' Cut off the '@' at the front to get just the file name
		arg = right(arg, len(arg) - 1)

		'' Complain about '@' only
		if (len(arg) = 0) then
			'' Missing file name after '@'
			hFatalInvalidOption( arg, is_source )
		end if

		'' Recursively read in the additional options from the file
		reclevel += 1
		parseArgsFromFile( arg, is_source )
		reclevel -= 1

	case else
		'' Input file, get its extension to determine what it is
		dim as string ext = hGetFileExt(arg)

		#if defined(__FB_WIN32__) or _
			defined(__FB_DOS__) or _
			defined(__FB_CYGWIN__)
			'' For case in-sensitive file systems
			ext = lcase(ext)
		#endif

		select case (ext)
		case "bas"
			hAddBas( arg )

		case "o"
			fbcAddObj( arg )

		case "a"
			strlistAppend( @fbc.libfiles, arg )

		case "rc", "res"
			hSetIofile( listNewNode( @fbc.rcs ), arg, TRUE )

		case "xpm"
			'' Can have only one .xpm, or the fb_program_icon
			'' symbol will be duplicated
			if( len( fbc.xpm.srcfile ) > 0 ) then
				hFatalInvalidOption( arg, is_source )
			end if

			hSetIofile( @fbc.xpm, arg, TRUE )

		case else
			'' Input file without or with unknown extension
			hFatalInvalidOption( arg, is_source )

		end select
	end select
end sub

sub fbcParseArgsFromString _
	( _
		byval args_in as zstring ptr, _
		byval is_source as integer, _
		byval is_file as integer _
	)

	dim as string args = *args_in
	dim as string arg

	'' Parse the line containing command line arguments,
	'' separated by spaces. Double- and single-quoted strings
	'' are handled too, but nothing else.
	do
		dim as integer length = len(args)
		if (length = 0) then
			exit do
		end if

		dim as integer i = 0
		dim as integer quotech = 0

		while (i < length)
			dim as integer ch = args[i]

			select case as const (ch)
			case asc(" ")
				if (quotech = 0) then
					exit while
				end if

			case asc(""""), asc("'")
				if (quotech = ch) then
					'' String closed
					quotech = 0
				elseif (quotech = 0) then
					'' String opened
					quotech = ch
				end if

			end select

			i += 1
		wend

		if (i = 0) then
			'' Just space, skip it
			i = 1
		else
			arg = left(args, i)
			arg = trim(arg)
			arg = strUnquote(arg)
			handleArg( arg, is_source, is_file )
		end if

		args = right(args, length - i)
	loop

end sub

private sub parseArgsFromFile _
	( _
		byref filename as string, _
		byval is_source as integer _
	)
	dim as integer f = freefile()
	if (open(filename, for input, as #f)) then
		errReportEx( FB_ERRMSG_FILEACCESSERROR, filename, -1 )
		fbcEnd(1)
	end if

	dim as string args

	while (eof(f) = FALSE)
		line input #f, args
		args = trim(args)
		fbcParseArgsFromString( strptr( args ), is_source, TRUE )
	wend

	close #f
end sub

'' Whether a target needs shared libraries to be built with PIC.
'' (Note: Android 5.0+ also need executables to be built with PIC (gcc -pie argument),
'' but Android <4.1 didn't support PIE executables. We assume 4.1+.)
private function hTargetNeedsPIC( ) as integer
	function = FALSE
	if( fbGetCpuFamily( ) <> FB_CPUFAMILY_X86 ) then
		select case as const( fbGetOption( FB_COMPOPT_TARGET ) )
		case FB_COMPTARGET_LINUX, FB_COMPTARGET_FREEBSD, _
		     FB_COMPTARGET_OPENBSD, FB_COMPTARGET_NETBSD, _
		     FB_COMPTARGET_DRAGONFLY, FB_COMPTARGET_SOLARIS, _
		     FB_COMPTARGET_ANDROID
			function = TRUE
		end select
	else
		'' On android-x86, PIC is necessary even to access globals in dynamic
		'' libraries, because the runtime linker doesn't support usual relocation types.
		'' GCC defaults to -fPIC anyway, but we need to be aware of whether PIC is used.
		if( fbGetOption( FB_COMPOPT_TARGET ) = FB_COMPTARGET_ANDROID ) then
			function = TRUE
		end if
	end if
end function

private sub hParseArgs( byval argc as integer, byval argv as zstring ptr ptr )
	fbc.optid = -1

	'' Note: ignoring argv[0], assuming it's the path used to run fbc
	dim as string arg
	for i as integer = 1 to (argc - 1)
		arg = *argv[i]
		handleArg( arg, FALSE, FALSE )
	next

	'' Waiting for argument to an option? If the user did something like
	'' 'fbc foo.bas -o' this shows the error.
	if (fbc.optid >= 0) then
		'' Missing argument for command line option
		hFatalInvalidOption( *argv[argc - 1], FALSE )
	end if
end sub

private sub hCheckArgs()
	'' In case there was an '-o <file>', but no corresponding input file,
	'' this will report the error.
	hCheckWaitingObjfile( )

	''
	'' Check for incompatible options etc.
	''
	select case( fbGetOption( FB_COMPOPT_FPUTYPE ) )
	case FB_FPUTYPE_FPU
		if( fbGetOption( FB_COMPOPT_VECTORIZE ) >= FB_VECTORIZE_NORMAL ) then
			errReportEx( FB_ERRMSG_OPTIONREQUIRESSSE, "", -1 )
			fbcEnd( 1 )
		end if
	case FB_FPUTYPE_SSE
		if( (fbGetCpuFamily( ) <> FB_CPUFAMILY_X86) and _
		    (fbGetCpuFamily( ) <> FB_CPUFAMILY_X86_64) ) then
			errReportEx( FB_ERRMSG_SSEREQUIRESX86, "", -1 )
			fbcEnd( 1 )
		end if
	case FB_FPUTYPE_NEON
		if( (fbGetCpuFamily( ) <> FB_CPUFAMILY_ARM) and _
		    (fbGetCpuFamily( ) <> FB_CPUFAMILY_AARCH64) ) then
			errReportEx( FB_ERRMSG_NEONREQUIRESARM, "", -1 )
			fbcEnd( 1 )
		end if
	end select

	'' 1. The compiler (fb.bas) starts with default target settings for
	''    native compilation.

	'' 2. -target option handling has already switched the target if given.

	'' 3. -arch overrides any other arch settings.
	if( fbc.cputype >= 0 ) then
		fbSetOption( FB_COMPOPT_CPUTYPE, fbc.cputype )
	end if

	'' NEON implies at least armv7-a
	if( (fbGetOption( FB_COMPOPT_FPUTYPE ) = FB_FPUTYPE_NEON) and _
	    (fbGetOption( FB_COMPOPT_CPUTYPE ) < FB_CPUTYPE_ARMV7A) ) then
		fbSetOption( FB_COMPOPT_CPUTYPE, FB_CPUTYPE_ARMV7A )
	end if

	'' 4. Check for target/arch conflicts, e.g. dos and non-x86
	if( (fbGetOption( FB_COMPOPT_TARGET ) = FB_COMPTARGET_DOS) and _
		(fbGetCpuFamily( ) <> FB_CPUFAMILY_X86) ) then
		errReportEx( FB_ERRMSG_DOSWITHNONX86, fbGetFbcArch( ), -1 )
		fbcEnd( 1 )
	end if

	'' 4.5. Enable -pic automatically when building a Unix shared library
	''      or Android executable (required on Android 5+)
	if( (fbGetOption( FB_COMPOPT_OUTTYPE ) = FB_OUTTYPE_DYNAMICLIB) or _
	    (fbGetOption( FB_COMPOPT_TARGET ) = FB_COMPTARGET_ANDROID) ) then
		if( hTargetNeedsPIC( ) ) then
			fbSetOption( FB_COMPOPT_PIC, TRUE )
		end if
	end if

	'' Complain if -pic was given in cases where it's not needed/supported
	if( fbGetOption( FB_COMPOPT_PIC ) ) then
		if( hTargetNeedsPIC( ) = FALSE ) then
			errReportEx( FB_ERRMSG_PICNOTSUPPORTEDFORTARGET, "", -1 )
		end if
	end if

	'' 5. Select default backend based on selected arch, e.g. when compiling
	''    for x86-64 or ARM, we shouldn't default to -gen gas anymore (as
	''    long as it doesn't support it).
	''
	'' This should be done no matter whether compiling for the native system
	'' or cross-compiling. Even on a 64bit x86_64 host where
	'' FB_DEFAULT_BACKEND is -gen gcc, we still prefer using -gen gas when
	'' cross-compiling to 32bit x86.
	'' (Apple gas assembler has such broken support for intel syntax
	'' (see https://discussions.apple.com/message/10163960#10163960)
	'' that it can't work for non-trivial programs, so default to -gen gcc.)
	if( (fbGetCpuFamily( ) = FB_CPUFAMILY_X86) and _
		(fbGetOption(FB_COMPOPT_TARGET) <> FB_COMPTARGET_DARWIN) ) then
		fbSetOption( FB_COMPOPT_BACKEND, FB_BACKEND_GAS )
	else
		fbSetOption( FB_COMPOPT_BACKEND, FB_BACKEND_GCC )
	end if
	'' gas/gas64 doesn't currently support PIC
	if( ((fbGetOption( FB_COMPOPT_BACKEND ) = FB_BACKEND_GAS) or _
	     (fbGetOption( FB_COMPOPT_BACKEND ) = FB_BACKEND_GAS64)) and _
	    fbGetOption( FB_COMPOPT_PIC ) ) then
		fbSetOption( FB_COMPOPT_BACKEND, FB_BACKEND_GCC )
	end if

	'' 6. -gen overrides any other backend setting.
	if( fbc.backend >= 0 ) then
		fbSetOption( FB_COMPOPT_BACKEND, fbc.backend )
	end if

	'' 7. Check whether backend supports the target/arch.
	'' -gen gas with non-x86 arch or with PIC isn't possible.
	'' -gen gas64 with non-x86_64 or with PIC isn't possible.
	if( ((fbGetOption( FB_COMPOPT_BACKEND ) = FB_BACKEND_GAS) and _
	    (fbGetCpuFamily( ) <> FB_CPUFAMILY_X86)) or _
	    ((fbGetOption( FB_COMPOPT_BACKEND ) = FB_BACKEND_GAS64) and _
	    (fbGetCpuFamily( ) <> FB_CPUFAMILY_X86_64)) ) then
		errReportEx( FB_ERRMSG_GENGASWITHNONX86, fbGetFbcArch( ), -1 )
		fbcEnd( 1 )
	end if

	if( ((fbGetOption( FB_COMPOPT_BACKEND ) = FB_BACKEND_GAS) or _
	     (fbGetOption( FB_COMPOPT_BACKEND ) = FB_BACKEND_GAS64)) and _
	    fbGetOption( FB_COMPOPT_PIC ) ) then
		errReportEx( FB_ERRMSG_GENGASWITHPIC, "", -1 )
		fbcEnd( 1 )
	end if

	'' Resource scripts are only allowed for win32 & co,
	select case as const (fbGetOption(FB_COMPOPT_TARGET))
	case FB_COMPTARGET_WIN32, FB_COMPTARGET_CYGWIN, FB_COMPTARGET_XBOX

	case else
		dim as FBCIOFILE ptr rc = listGetHead(@fbc.rcs)
		if (rc) then
			errReportEx(FB_ERRMSG_RCFILEWRONGTARGET, rc->srcfile, -1)
			fbcEnd(1)
		end if
	end select

	'' The embedded .xpm is only useful for the X11 gfxlib
	select case as const (fbGetOption(FB_COMPOPT_TARGET))
	case FB_COMPTARGET_LINUX, FB_COMPTARGET_DARWIN, _
		FB_COMPTARGET_FREEBSD, FB_COMPTARGET_OPENBSD, _
		FB_COMPTARGET_NETBSD, FB_COMPTARGET_DRAGONFLY, FB_COMPTARGET_SOLARIS

	case else
		if (len(fbc.xpm.srcfile) > 0) then
			errReportEx(FB_ERRMSG_RCFILEWRONGTARGET, fbc.xpm.srcfile, -1)
			fbcEnd(1)
		end if
	end select

	'' On darwin need to change the default asm syntax when using gen gcc because
	'' most C compilers on OSX seem to be configured without intel syntax support;
	'' probably because Apple as and llvm-mc have horribly broken intel support.
	if( (fbGetOption( FB_COMPOPT_TARGET ) = FB_COMPTARGET_DARWIN) and _
		(fbGetOption( FB_COMPOPT_BACKEND ) <> FB_BACKEND_GAS) ) then
		fbSetOption( FB_COMPOPT_ASMSYNTAX, FB_ASMSYNTAX_ATT )
	end if

	if( fbc.asmsyntax >= 0 ) then
		'' -asm only applies to x86 and x86_64
		select case( fbGetCpuFamily( ) )
		case FB_CPUFAMILY_X86, FB_CPUFAMILY_X86_64
		case else
			errReportEx( FB_ERRMSG_ASMOPTIONGIVENFORNONX86, fbGetTargetId( ), -1 )
		end select

		'' -gen gas only supports -asm intel
		select case fbGetOption( FB_COMPOPT_BACKEND )
		case FB_BACKEND_GAS, FB_BACKEND_GAS64
			if( fbc.asmsyntax <> FB_ASMSYNTAX_INTEL ) then
				errReportEx( FB_ERRMSG_GENGASWITHOUTINTEL, "", -1 )
			end if
		end select

		'' -asm overrides the target's default
		fbSetOption( FB_COMPOPT_ASMSYNTAX, fbc.asmsyntax )
	end if

	'' Update the stacksize for the current target options if
	'' stacksize was never set yet by passing a negative stacksize
	fbSetOption( FB_COMPOPT_STACKSIZE, -1 )

	'' TODO: Check whether subsystem/stacksize/xboxtitle were set and
	'' complain about it when the target doesn't allow it, or just
	'' ignore silently (that might not even be too bad for portability)?
end sub


private sub fbcPrintTargetInfo( )
	var s = fbGetTargetId( )
	s += ", " + *fbGetFbcArch( )
	s += ", " & fbGetBits( ) & "bit"
	#ifndef ENABLE_STANDALONE
		if( len( fbc.target ) > 0 ) then
			s += " (" + fbc.target + ")"
		end if
	#endif
	print "target:", s
	print "backend:", fbGetBackendName( fbGetOption( FB_COMPOPT_BACKEND ) )
end sub

private sub fbcDetermineMainName( )
	'' Determine the main module path/name if not given via -m
	if (len(fbc.mainname) = 0) then
		'' 1) First input .bas module
		dim as FBCIOFILE ptr m = listGetHead( @fbc.modules )
		if( m ) then
			fbc.mainname = m->srcfile
		else
			'' 2) First input .o
			dim as string ptr objf = listGetHead( @fbc.objlist )
			if( objf <> NULL ) then
				fbc.mainname = *objf
			else
				'' 3) Neither input .bas nor .o, that is rare,
				'' but happens in this case:
				''      $ fbc a.bas -lib -m a
				''      $ fbc b.bas -lib
				''      $ fbc -l a -l b
				'' Usually -x is used too though, so this
				'' fallback name won't be seen often.
				'' This name should be 8.3 compatible (for DOS)
				fbc.mainname = "unnamed"
			end if
		end if
		fbc.mainname = hStripExt(fbc.mainname)
	end if
end sub

'' Build the intermediate file name for the given module and step
private function hGetAsmName _
	( _
		byval module as FBCIOFILE ptr, _
		byval stage as integer _
	) as string

	dim as zstring ptr ext = any
	dim as string asmfile

	'' Based on the objfile name so it's also affected by -o
	asmfile = hStripExt( *module->objfile )

	if( fbGetOption( FB_COMPOPT_TARGET ) <> FB_COMPTARGET_JS ) then
		ext = @".asm"
	else
		ext = @".o"
	end if
	if( stage = 1 ) then
		select case( fbGetOption( FB_COMPOPT_BACKEND ) )
		case FB_BACKEND_GCC, FB_BACKEND_CLANG
			ext = @".c"
		case FB_BACKEND_LLVM
			ext = @".ll"
		end select
	end if

	asmfile += *ext

	function = asmfile
end function

private sub hCompileBas _
	( _
		byval module as FBCIOFILE ptr, _
		byval is_main as integer, _
		byval is_fbctinf as integer, _
		byval module_count as integer _
	)

	dim as integer prevlang = any, prevouttype = any
	dim as string asmfile, pponlyfile

	asmfile = hGetAsmName( module, 1 )

	'' -pp?
	if( fbGetOption( FB_COMPOPT_PPONLY ) ) then
		'' Re-use the full -o path/filename for the -pp output file,
		'' since no .o will be generated anyways (if -o was given)
		pponlyfile = *module->objfile
		if( module->is_custom_objfile = FALSE ) then
			'' Otherwise, use a default file name
			pponlyfile = hStripExt( pponlyfile ) + ".pp.bas"
		end if
	end if

	if( fbc.verbose ) then
		print "compiling: ", module->srcfile; " -o "; asmfile;
		if( fbGetOption( FB_COMPOPT_PPONLY ) ) then
			print " -pp " + pponlyfile;
		end if
		if( is_main ) then
			print " (main module)";
		elseif( is_fbctinf ) then
			print " (FB compile-time info)";
		end if
		print
	end if

	'' Restarting with a new lang option?
	'' We need to initialize with the restart lang
	if( fbGetOption( FB_COMPOPT_RESTART_LANG ) <> FB_LANG_INVALID ) then
		fbSetOption( FB_COMPOPT_LANG, fbGetOption( FB_COMPOPT_RESTART_LANG ) )
	end if

	'' preserve orginal values that might have to restored
	'' (e.g. -lang mode could be overwritten while parsing due to #lang,
	'' but that shouldn't affect other modules)
	prevlang = fbGetOption( FB_COMPOPT_LANG )
	prevouttype = fbGetOption( FB_COMPOPT_OUTTYPE )

	if( is_fbctinf ) then
		'' Switch to -c mode temporarily to get the compiler to write objinfo
		fbSetOption( FB_COMPOPT_OUTTYPE, FB_OUTTYPE_OBJECT )
	end if

	do
		'' Clean up stage 1 output (FB backend's output, *.asm/*.c/*.ll),
		'' unless -R was given, and additionally in case of -gen gas, unless -RR
		'' was given (because for -gen gas, the FB backend's .asm output is also
		'' the final .asm which -RR is supposed to preserve).
		if( (not fbc.keepasm) and _
			(((fbGetOption( FB_COMPOPT_BACKEND ) <> FB_BACKEND_GAS) and (fbGetOption( FB_COMPOPT_BACKEND ) <> FB_BACKEND_GAS64) ) or _
			(not fbc.keepfinalasm)) ) then
			fbcAddTemp( asmfile )

		'' first module? handle side effects of #cmdline
		elseif( module_count = 1 ) then
			'' Keep the asm file.  If the keep option was in a #cmdline then
			'' the temporary file probably was already added to the fbc.temps
			'' list on the first pass in to the parser (unless real command
			'' line also had an option to keep the asm file).

			if( fbRestartGetCount() > 0 ) then
				fbcRemoveTemp( asmfile )
			end if
		end if

		'' init the parser (note: initializes env)
		fbInit( is_main, fbc.entry, module_count )

		if( is_fbctinf ) then
			'' Let the compiler know about all libs collected so far,
			'' so the fbctinf module represents all the other modules
			'' compiled/included in this fbc invocation.
			fbSetLibs( @fbc.finallibs, @fbc.finallibpaths )
		else
			'' Add only the libs and paths passed on the command line,
			'' so this module will only include objinfo for those libs
			'' and the ones found while parsing it, but not unrelated
			'' libs from other modules.
			fbSetLibs( @fbc.libs, @fbc.libpaths )
		end if

		fbCompile( module->srcfile, asmfile, pponlyfile, is_main )

		'' fbcParser: collect everything reachable from the global namespace
		'' - must run before fbEnd() below frees the symbol table (locals
		'' were already harvested per-proc during the parse)
		collWalkGlobals( )

		'' If there were any errors during parsing, just exit without
		'' doing anything else.
		if( errGetCount( ) > 0 ) then
			fbcEnd( 1 )
		end if

		'' Don't restart unless asked for
		if( fbShouldRestart( ) = FALSE ) then
			exit do
		end if

		'' Close the request to restart the parser
		fbRestartEndRequest( FB_RESTART_PARSER )

		'' Shutdown the parser before restarting
		fbEnd( )

		'' Still have restart set?  It must be a request to restart fbc
		if( fbShouldRestart( ) ) then
			'' Restore original #lang? only if we didn't set a new lang to restart with
			if( fbGetOption( FB_COMPOPT_RESTART_LANG ) = FB_LANG_INVALID ) then
				fbSetOption( FB_COMPOPT_LANG, prevlang )
			end if
			exit sub
		end if
	loop

	'' (unnecessary for the empty fbctinf module, it won't add anything new)
	if( is_fbctinf = FALSE ) then
		'' Update the list of libs and paths with the ones found when parsing
		fbGetLibs( @fbc.finallibs, @fbc.finallibpaths )
	end if

	'' Shutdown the parser
	fbEnd( )

	'' Restore original options
	if( is_fbctinf ) then
		fbSetOption( FB_COMPOPT_OUTTYPE, prevouttype )
	end if
	fbSetOption( FB_COMPOPT_LANG, prevlang )
end sub

private sub hCompileModules( )
	dim as integer ismain = any, checkmain = any
	dim as string mainfile
	dim as FBCIOFILE ptr module = any

	ismain = FALSE

	select case fbGetOption( FB_COMPOPT_OUTTYPE )
	case FB_OUTTYPE_EXECUTABLE, FB_OUTTYPE_DYNAMICLIB
		checkmain = TRUE
	case else
		'' When building an object or a library (-c/-r, -lib), nothing
		'' is compiled with ismain = TRUE until -m was given for it.
		'' This makes sense because -c is usually used to compile
		'' single modules of which only a very specific one is the
		'' main one (nobody would want -c to include main() everywhere),
		'' and because -lib is for making libraries which generally
		'' don't include a main module for programs to use.
		checkmain = fbc.mainset
	end select

	if( checkmain ) then
		'' Note: This causes the path given with -m to be ignored in
		'' the ismain check below. This is good because -m is easier
		'' to use that way (e.g. fbc ../../main.bas -m main), and bad
		'' because then modules with the same name but in different
		'' directories will both be seen as the main one.
		mainfile = hStripPath( fbc.mainname )
	end if

	module = listGetHead( @fbc.modules )

	if( module = NULL ) then
		'' No input .bas files to compile - make sure to add the libs
		'' from the command line to the final lists anyways.
		strsetCopy( @fbc.finallibs, @fbc.libs )
		strsetCopy( @fbc.finallibpaths, @fbc.libpaths )
		exit sub
	end if

	'' We have input .bas files to compile - hCompileBas() will take care of
	'' copying the command line libs into the final lists:
	'' 1. into the compiler
	''    (fbc.libs -> fbSetLibs() -> compiler)
	'' 2. compiler collects additional #inclibs etc...
	'' 3. and copy back into final lists
	''    (compiler -> fbGetLibs() -> fbc.finallibs)

	dim as integer module_count = 0
	do
		if( checkmain ) then
			ismain = (mainfile = hStripPath( hStripExt( module->srcfile ) ))
			'' Note: checking continues for all modules, because
			'' "the" main module could be passed multiple times,
			'' and it makes sense to always treat it the same,
			'' so that <fbc 1.bas 1.bas -c> generates the same 1.o
			'' twice and <fbc 1.bas 1.bas> causes a duplicated
			'' definition of main().
			/'checkmain = not ismain'/
		end if

		module_count += 1
		hCompileBas( module, ismain, FALSE, module_count )

		if( fbShouldRestart( ) ) then
			exit sub
		end if

		module = listGetNext( module )
	loop while( module )
end sub

private sub hAppendConfigInfo( byref config as string, byval info as zstring ptr )
	if( len( config ) > 0 ) then
		config += ", "
	end if
	config += *info
end sub

private sub hPrintVersion( byval verbose as integer )
	dim as string config

	print "FreeBASIC Compiler - Version " + FB_VERSION + _
		" (" + FB_BUILD_DATE_ISO + "), built for " + fbGetHostId( ) + " (" & fbGetHostBits( ) & "bit)"
	print "Copyright (C) 2004-2025 The FreeBASIC development team."

	#ifdef ENABLE_STANDALONE
		hAppendConfigInfo( config, "standalone" )
	#endif

	#ifdef ENABLE_PREFIX
		hAppendConfigInfo( config, "prefix: '" + ENABLE_PREFIX + "'" )
	#endif

	if( len( config ) > 0 ) then
		print config
	end if

	if( verbose ) then
		fbcPrintTargetInfo( )
		if( FB_BUILD_SHA1 > "" ) then
			print "source sha-1: " & FB_BUILD_SHA1
		end if
		if( FB_BUILD_FORK_ID > "" ) then
			print "fbc fork id:  " & FB_BUILD_FORK_ID
		end if
	end if
end sub

'//  fbc_compile replaces the original FBC startup code and is called
'//  by Tiko as an array of files to be parsed. The files are 
'//
'//  The fbcEnd() function has been modified to comment out the End
'//  statement which would simply exit if errors occurred.



function fbc_compile _
	( _
		byref sfilename as string, _
		byval result as FBCP_RESULT ptr, _
		byval scanoptions as FBCP_OPTIONS ptr _
	) as integer

	collInit( )

	fbcInit( )

	'' fbcParser: a symbol extractor has no use for fbc's interactive
	'' max-error abort (FB_DEFAULT_MAXERRORS = 10) - a lone .inc scanned
	'' outside its project context errors on every unresolved type, and the
	'' scan would abort before ever reaching the file's own body, losing
	'' every symbol below the cutoff. Diags are data here, not a reason to
	'' stop. Errors are deduped per statement (errctx.lastline/laststmt),
	'' so the count is bounded by the statement count; the huge finite cap
	'' remains purely as a runaway backstop. Set after fbcInit() because
	'' fbGlobalInit() resets the default on every scan.
	fbSetOption( FB_COMPOPT_MAXERRORS, 100000 )

    hAddBas(sfilename)

	hCheckArgs( )

	do
		'' fbcParser: no fbcDeterminePrefix()/fbcSetupCompilerPaths() - they
		'' derive paths from the host exe's location, which is meaningless
		'' for a DLL. The caller supplies the include search paths instead.
		'' (env.includepaths/env.predefines are re-inited by fbGlobalInit
		'' inside fbcInit, so this is per-scan clean.)
		if( scanoptions <> NULL ) then
			for i as integer = 0 to scanoptions->includeCount-1
				dim as string path = *scanoptions->includePaths[i]
				fbAddIncludePath( path )
			next
			for i as integer = 0 to scanoptions->defineCount-1
				dim as string def = *scanoptions->defines[i]
				fbAddPreDefine( def )
			next
		end if

		fbcDetermineMainName( )

		''
		'' Compile .bas modules
		''
		hCompileModules( )

		if( fbShouldRestart( ) = FALSE ) then
			exit do
		end if

		fbRestartEndRequest( FB_RESTART_FBC_CMDLINE )

		'' we are restarting, so show errors again
		errPreInit( )

		'' command line arguments have changed, check them again
		hCheckArgs( )

	loop

    '' The fbcEnd() function has been modified to comment out the End
    '' statement which would simply exit our language server.
	fbcEnd( 0 )

	'' marshal everything staged during the compile into the caller's
	'' result structure (independent of compiler state, which is gone now)
	collFinalize( result )
	collEnd( )

    function = 0
end function

''
'' fbc_compile_text: like fbc_compile(), but the root source comes from an
'' already-loaded, null-terminated buffer (the host's editor buffer) instead
'' of a file on disk - nothing is opened or read for the root module.
'' The text is treated as ASCII/ANSI; a leading UTF-8 BOM is tolerated.
''
'' pVirtualName (optional) is the filename the buffer's symbols are
'' attributed to; it also anchors same-directory #include resolution, so
'' pass the real path of the file being edited. #includes themselves are
'' still read from disk.
''
function fbc_compile_text _
	( _
		byval pText as zstring ptr, _
		byval result as FBCP_RESULT ptr, _
		byval scanoptions as FBCP_OPTIONS ptr, _
		byval pVirtualName as const zstring ptr = 0 _
	) as integer

	if( pText = NULL ) then
		return -1
	end if

	dim as string vname
	if( (pVirtualName <> NULL) andalso (len( *pVirtualName ) > 0) ) then
		vname = *pVirtualName
	else
		vname = "unnamed.bas"
	end if

	'' stays staged across #cmdline restarts inside the scan; every
	'' fbCompile() of the root module picks it up
	fbSetMemSource( pText )
	function = fbc_compile( vname, result, scanoptions )
	fbSetMemSource( NULL )
end function
