'' Broken/mid-edit test input: every construct here is deliberately
'' incomplete, the way a file looks while the user is still typing.
'' The scan must return partial symbols + diags - never crash or hang.

'' false #if block (skipped content must not be reported)
#if defined( NEVER_DEFINED )
dim shared gInsideIf as long
#endif

'' half-typed declaration
dim shared gCount as

'' TYPE with no END TYPE, interrupted by another declaration
type HalfWidget
	id as long
	title as string * 32

declare sub UseWidget( byval nId as long )

'' proc with locals but no END SUB before EOF
sub DanglingProc( byval nMode as long )
	dim as long nLocal = nMode
	dim as HalfWidget hw
