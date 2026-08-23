'' Test input for tiko_fbctest.exe - must parse with ZERO errors.
'' Self-contained (no includes) and exercises everything the symbol
'' extraction must report: an enum, TYPEs with fields and a member proc,
'' a typedef, declares + implementations with params, module-level vars
'' (including of TYPE/enum type) and locals inside procs.

#include once "_testfile_inc.bi"

#define MAX_WIDGETS 16
const DEFAULT_TITLE = "widget"

'' DECLARE-only proc (no body anywhere) - param names must still be reported,
'' located at this line
declare function ClampLong( byval nValue as long, byval nMin as long, byval nMax as long ) as long

'' macro-declared proc - expect the invocation line, col 0 (documented caveat)
#define DECLARE_GETTER(nm) declare function Get##nm( ) as long
DECLARE_GETTER(Total)

enum WidgetKind
	WK_BUTTON = 1
	WK_LABEL
	WK_LISTBOX
end enum

type Vector2D
	x as double
	y as double
end type

type Widget
	id as long
	kind as WidgetKind
	origin as Vector2D
	title as string * 32
	declare function Describe( ) as string
end type

function Widget.Describe( ) as string
	dim as string s = "widget #" & this.id
	return s
end function

type WidgetRef as Widget ptr

declare sub AddWidget( byval w as Widget ptr, byval kind as WidgetKind )
declare function CountWidgets( ) as long

dim shared gWidgets( 1 to MAX_WIDGETS ) as Widget
dim shared gWidgetCount as long
dim shared gDefaultKind as WidgetKind

sub AddWidget( byval w as Widget ptr, byval kind as WidgetKind )
	dim as long idx = gWidgetCount + 1
	dim as Vector2D pt
	pt.x = 0.0
	pt.y = 0.0
	if idx <= MAX_WIDGETS then
		w->kind = kind
		w->origin = pt
		gWidgetCount = idx
	end if
end sub

function CountWidgets( ) as long
	dim as long total = gWidgetCount
	return total
end function
