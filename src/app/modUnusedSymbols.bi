' ========================================================================================
' modUnusedSymbols.bi
'
' The MODEL behind the Unused Symbols report: build a row list from one scan, sort it,
' and decide whether a row passes the current filter.
'
' This is deliberately NOT part of clsSymbolDb. That class is a symbol INDEX, queried by
' name and file; this is a REPORT, and keeping it separate is what lets the two risky
' routines - the comparator and the filter test - be pure functions over an array,
' assertable with no window open and no scan running.
'
' The counts come from fbcParser (see FBCP_SYMBFLAG_REFTRACKED in fbcParser.bi). They are
' per SCAN, so a symbol referenced only from a file this scan never reached reads as
' unused; that is why UnusedSymbols_Build reads ONE tier and never the merged view, and
' why EXPORTed procs are downgraded rather than reported.
' ========================================================================================

#ifndef __MODUNUSEDSYMBOLS_BI__
#define __MODUNUSEDSYMBOLS_BI__

' Row status. A row is only ever built for a symbol with zero reads.
enum UNUSED_STATUS
    UnusedStatusDead = 0        ' no references at all
    UnusedStatusWriteOnly       ' assigned, never read
    UnusedStatusUnknown         ' counts not trustworthy for this symbol (EXPORT,
                                '   overloaded set, ctor/dtor, ...) - shown, never
                                '   asserted to be dead
end enum

' Kind groups, one per filter toggle. Deliberately coarser than FBCP_KIND: the user
' thinks in "parameters", not in SUB-params vs FUNCTION-params.
enum UNUSED_KIND
    UnusedKindVariable = 0
    UnusedKindProcedure
    UnusedKindParameter
    UnusedKindType
    UnusedKindField
    UnusedKindConstant
    UNUSED_KIND_COUNT
end enum

' Sort columns, in display order. The list control's column indices are these values.
enum UNUSED_COLUMN
    UnusedColFile = 0
    UnusedColLine
    UnusedColClass
    UnusedColName
    UnusedColStatus
    UNUSED_COLUMN_COUNT
end enum

type UNUSEDROW
    wszFile   as DWSTRING       ' full path, as the scan reported it
    nLine     as long           ' 1-BASED, as the engine reports. OpenSelectedDocument
                                '   takes 0-based - convert at the call site.
    nKind     as long           ' UNUSED_KIND
    wszName   as DWSTRING       ' qualified where the parent is a TYPE/namespace
    nStatus   as long           ' UNUSED_STATUS
    nReads    as long
    nWrites   as long
end type

' Build the report from one tier (pass ScanTierProject; see the header). Returns the row
' count. Also reports, in wszDirtyFiles, the open documents with unsaved edits that
' contributed to this scan - their line numbers may be stale, which the window warns about
' rather than blocking on.
declare function UnusedSymbols_Build( byval nTier as long, rows() as UNUSEDROW, _
                                      byref wszDirtyFiles as DWSTRING ) as long

' Pure. Stable, with a deterministic tiebreak, so re-clicking a header cannot shuffle
' equal rows. Line and reference counts compare NUMERICALLY.
declare sub UnusedSymbols_Sort( rows() as UNUSEDROW, byval nCount as long, _
                               byval nCol as long, byval bDesc as boolean )

' Pure. nKindMask is a bitmask of (1 shl UNUSED_KIND); wszFilter matches
' case-insensitively against any displayed column.
declare function UnusedSymbols_Passes( byref r as UNUSEDROW, byval nKindMask as long ) as boolean

' Pure. Display text for a row's class and status columns (localized).
declare function UnusedSymbols_KindText( byval nKind as long ) as DWSTRING
declare function UnusedSymbols_StatusText( byref r as UNUSEDROW ) as DWSTRING

' Pure. -1 / 0 / +1, the comparator the sort is built from. Public so it can be asserted
' directly rather than through the sort.
declare function UnusedSymbols_Compare( byref a as UNUSEDROW, byref b as UNUSEDROW, _
                                        byval nCol as long ) as long

declare sub UnusedSymbols_RunSelfTest()

#endif
