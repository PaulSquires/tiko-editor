'' UTF-8 (BOM) test input
type Utf8Point
	ux as long
	uy as long
end type
declare function Utf8Sum( byval a as long, byval b as long ) as long
dim shared gUtf8Title as string   '' comment with unicode: Grüße
