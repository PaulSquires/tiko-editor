' ########################################################################################
' modEncodingSelfTest.bi
'
' Env-gated self-test for the file-encoding conversion path (ANSI / UTF-8 / UTF-8 BOM /
' UTF-16 BOM). Runs only when TIKO_ENCODING_SELFTEST=1. See modEncodingSelfTest.inc.
'
' Two tiers:
'   Tier A - pure, byte-exact assertions on Doc_EncodeForDisk (no window, no document).
'            This is where the UTF-16 half-length truncation bug is pinned.
'   Disk   - encode -> write via PsFileWriteAll -> read back via Doc_ReadFromDisk, closing
'            the loop end to end (the only test that would have caught the truncated save).
'
' ---- IT LIVES IN app/ SINCE 7c STEP 9, AND THAT IS WHAT MAKES IT RUNNABLE ---------------
'
' It was in src/ because it named WideCharToMultiByte, CFileStream and GetFileToString. All
' three went that step -- the oracle is hand-written now (and is a STRONGER oracle for it:
' see the note on its body), the writer is PsFileWriteAll, and the reader is the shared
' Doc_ReadFromDisk.
'
' WHY THAT MATTERS MORE THAN TIDINESS: tiko can only run these from inside a started GUI,
' behind TIKO_ENCODING_SELFTEST. The portable shell runs them HEADLESSLY from --selftest.
' During the step that moved them, they were not run ONCE until the shell could -- which is
' the argument for the move, stated as evidence rather than as a preference.
'
' The live-document tier (driving the real ConvertTextBuffer) is still NOT here: it needs
' real Scintilla windows created at startup, which perturbs tab/session state. INVARIANT E1
' no longer exists to assert -- see modEncoding.inc for E2, which replaced it -- and the
' shell's own suite asserts E2 against a real tab. What remains uncovered is
' ConvertTextBuffer itself, which is tiko-side, and the author's interactive pass.
' ########################################################################################

declare sub Encoding_RunSelfTest()
