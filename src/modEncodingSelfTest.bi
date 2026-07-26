' ########################################################################################
' modEncodingSelfTest.bi
'
' Env-gated self-test for the file-encoding conversion path (ANSI / UTF-8 / UTF-8 BOM /
' UTF-16 BOM). Runs only when TIKO_ENCODING_SELFTEST=1. See modEncodingSelfTest.inc.
'
' Two tiers:
'   Tier A - pure, byte-exact assertions on Doc_EncodeForDisk (no window, no document).
'            This is where the UTF-16 half-length truncation bug is pinned.
'   Disk   - encode -> write via CFileStream -> read back via GetFileToString, closing the
'            loop end to end (the only test that would have caught the truncated save).
'
' The live-document tier (driving the real ConvertTextBuffer and asserting INVARIANT E1 in
' the editor) is intentionally NOT here: it needs real Scintilla windows created at startup,
' which perturbs tab/session state. That path's correctness rests on the shared conversion
' primitives (covered here) plus the author's interactive pass.
' ########################################################################################

declare sub Encoding_RunSelfTest()
