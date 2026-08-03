' ########################################################################################
' modSaveSelfTest.bi
'
' Env-gated: TIKO_SAVE_SELFTEST=1
'
' Asserts the ATOMIC-SAVE contract of Doc_WriteToDisk (modRoutines.inc). It exists because
' the defect it guards against was invisible to every other suite in this tree:
'
'   the save path deleted the target BEFORE writing the replacement, discarded the write's
'   byte count, and cleared the dirty flag unconditionally -- so any failure to open the
'   file destroyed the document AND reported success.
'
' THE LOAD-BEARING ASSERTIONS ARE THE FAILURE ONES, not the happy path. A writer that works
' when nothing is wrong is what the old code already did. What has to be proved is that when
' the write CANNOT succeed, the bytes already on disk are still there afterwards and the
' caller is told. Both are asserted by taking a real exclusive lock on the target with
' CreateFileW( ..., dwShareMode = 0 ) and then trying to save over it -- the same condition a
' user hits when the file is open in another program or held by a scanner.
'
' It touches disk (%TEMP%), which is the whole point: this contract is not reachable from a
' pure function, and that is exactly why it went unasserted for so long.
' ########################################################################################

#pragma once

declare sub Save_RunSelfTest()
