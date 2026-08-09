'' Include-file test input: symbols declared here must be reported with THIS
'' file's name, not the includer's.

type IncPoint
	px as long
	py as long
end type

declare function IncSum( byval a as long, byval b as long ) as long

dim shared gIncCounter as long
