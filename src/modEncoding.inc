' ########################################################################################
' modEncoding.inc
'
' Byte-level encoding conversion, and the single writer every save path goes through.
'
' Split out of modRoutines, which the audit called a junk drawer and which it was: text
' metrics, listview helpers, file dialogs, the localization loader, an HTTP update check and
' THIS all shared one 2,250-line file. Doc_EncodeForDisk and Doc_WriteToDisk decide whether
' a user's document survives being saved, and burying them among utilities is how the
' discarded bLossy flag went unnoticed for as long as it did.
'
' PURE MOVE. Nothing here was edited -- the functions and their comments are byte-identical
' to what they were in modRoutines.inc, in the same order relative to each other. Included
' BEFORE modRoutines.inc, which is load-bearing: BytesToWide, WideToAnsi, WideToUtf8 and
' Win32ErrorText are PRIVATE (undeclared in any .bi), and modRoutines' GetFileToString calls
' WideToUtf8 while modCompileErrors calls Win32ErrorText. In a unity build a private
' function has to be DEFINED before it is used, not merely declared.
' ########################################################################################

#include once "modRoutines.bi"



' ========================================================================================
' Length-explicit codepage conversions. These are the ONLY conversion primitives the
' encoding code should use: every call passes an explicit byte/char count (never -1), so
' an embedded NUL is preserved rather than acting as a terminator, and the destination
' byte count is computed by the API rather than guessed. AfxNova's DWSTRING conversion
' properties all pass -1 and stop at the first NUL, which is why they are avoided here.
' ========================================================================================

' Decode an ANSI or UTF-8 byte buffer into a UTF-16LE buffer held in a FreeBASIC string
' (2 bytes per code unit). nWideChars receives the number of UTF-16 code units.
private function BytesToWide( byref sBuffer as string, byval nCodePage as long, byref nWideChars as long ) as string
    nWideChars = 0
    if len(sBuffer) = 0 then return ""
    dim as long wlen = MultiByteToWideChar( nCodePage, 0, strptr(sBuffer), len(sBuffer), NULL, 0 )
    if wlen <= 0 then return ""
    dim as string sWide = string( wlen * 2, 0 )
    MultiByteToWideChar( nCodePage, 0, strptr(sBuffer), len(sBuffer), cast(LPWSTR, strptr(sWide)), wlen )
    nWideChars = wlen
    return sWide
end function

' Encode a UTF-16LE buffer (nWideChars code units) to ANSI bytes. bLossy is set true when
' any character had no representation in the active ANSI codepage (WC_NO_BEST_FIT_CHARS
' disables approximate substitution so the flag is meaningful).
private function WideToAnsi( byref sWide as string, byval nWideChars as long, byref bLossy as boolean ) as string
    bLossy = false
    if nWideChars = 0 then return ""
    dim as long alen = WideCharToMultiByte( CP_ACP, WC_NO_BEST_FIT_CHARS, cast(LPCWSTR, strptr(sWide)), nWideChars, NULL, 0, NULL, NULL )
    if alen <= 0 then return ""
    dim as string sAnsi = string( alen, 0 )
    dim as long usedDefault = 0
    WideCharToMultiByte( CP_ACP, WC_NO_BEST_FIT_CHARS, cast(LPCWSTR, strptr(sWide)), nWideChars, strptr(sAnsi), alen, NULL, @usedDefault )
    bLossy = (usedDefault <> 0)
    return sAnsi
end function

' Encode a UTF-16LE buffer (nWideChars code units) to UTF-8 bytes.
private function WideToUtf8( byref sWide as string, byval nWideChars as long ) as string
    if nWideChars = 0 then return ""
    dim as long u8len = WideCharToMultiByte( CP_UTF8, 0, cast(LPCWSTR, strptr(sWide)), nWideChars, NULL, 0, NULL, NULL )
    if u8len <= 0 then return ""
    dim as string sUtf8 = string( u8len, 0 )
    WideCharToMultiByte( CP_UTF8, 0, cast(LPCWSTR, strptr(sWide)), nWideChars, strptr(sUtf8), u8len, NULL, NULL )
    return sUtf8
end function


' Doc_EncodingName MOVED to app/modEncoding.inc (7c step 3, commit 7). Its declaration was
' already in app/modEncoding.bi and its body is a Select Case over FILE_ENCODING_* -- it was
' shell-side for proximity, like Doc_EncToPs before it. The shell's lossy-save prompt names
' the encoding, and this was the only unresolved symbol in that whole commit.
' Doc_EncToPs moved to app/modEncoding.inc with the two functions that use it -- it is a
' Select Case over constants and was never shell-side for any reason but proximity.
' Doc_EncodeForDisk and Doc_WriteToDisk MOVED to app/modEncoding.inc (7c step 3,
' commit 5). Both were already pure PsCore apart from two Win32ErrorText calls in the
' second, and those go through gAppHost.LastErrorText now. The declarations had been
' in app/modEncoding.bi all along; only the bodies lagged.


' ========================================================================================
' The system's own text for a GetLastError code. Neither AfxNova nor tiko had one of these
' (AfxGetOleErrorInfo is for IErrorInfo, which a Win32 file API does not set), and a save
' failure the user cannot act on is barely better than a silent one -- "the process cannot
' access the file because it is being used by another process" tells them to close Excel.
' ========================================================================================
private function Win32ErrorText( byval dwErr as DWORD ) as DWSTRING
    if dwErr = 0 then return ""
    dim as wstring ptr pMsg = 0
    dim as DWORD cch = FormatMessageW( _
            FORMAT_MESSAGE_ALLOCATE_BUFFER or FORMAT_MESSAGE_FROM_SYSTEM or _
            FORMAT_MESSAGE_IGNORE_INSERTS, _
            0, dwErr, 0, cast(wstring ptr, @pMsg), 0, 0 )
    if (cch = 0) orelse (pMsg = 0) then return "Windows error " & dwErr & "."
    dim as DWSTRING wszOut = *pMsg
    LocalFree( pMsg )
    ' FormatMessage terminates its text with CRLF; a message box adds its own layout.
    return PsStrTrimAny( wszOut, !" \r\n" )
end function




' ========================================================================================
' Relabel the document with the encoding it should be SAVED as.
'
' ---- IT USED TO CONVERT. NOW IT ONLY RELABELS (7c step 9) ------------------------------
'
' The old version of this function is worth describing, because what it did explains what
' this one does not have to. Scintilla held TWO layouts -- ANSI bytes under codepage 0, or
' UTF-8 under 65001 -- so switching a document to or from ANSI meant decoding the whole
' buffer, re-encoding it, SCI_CLEARALL, SCI_SETCODEPAGE, SetText, and restoring the caret
' by LINE AND COLUMN because every byte offset had moved.
'
' The reader decodes ANSI now (app/modEncoding.inc), so the editor holds UTF-8 for every
' document whatever its file was. ANSI is a DISK FORMAT ONLY, exactly as UTF-16 already
' was -- and this function's whole job is choosing which disk format the next save writes.
'
' ---- INVARIANT E1 IS RETIRED, AND ITS REPLACEMENT IS STRONGER --------------------------
'
' E1 said: "on return, SCI_GETCODEPAGE = iif(FileEncoding = ANSI, 0, SC_CP_UTF8), and this
' function is the ONLY code permitted to move either side of that on a populated document."
' It was enforced in five places that each had to remember the rule.
'
'   INVARIANT E2: SCI_GETCODEPAGE IS SC_CP_UTF8 ON EVERY POPULATED DOCUMENT, ALWAYS.
'   FileEncoding says what to WRITE and never what is in memory. Nothing needs permission
'   to maintain E2 because nothing needs to do anything.
'
' ---- AND THE CONVERSION PROMPT IS GONE WITH IT -----------------------------------------
'
' Switching to ANSI used to ask Doc_ConfirmAnsiConversion first, because the conversion
' destroyed characters immediately and SetText emptied the undo buffer, so there was no way
' back. NOTHING IS DESTROYED NOW -- the buffer is untouched and the loss, if any, happens
' when the file is written. The lossy-SAVE prompt (Doc_ConfirmLossySave) already guards
' that, at the moment it is true, and it is the only place the question is now asked.
'
' THIS IS A USER-VISIBLE CHANGE: choosing Edit > Encoding > ANSI on a document full of CJK
' no longer warns at the click. It warns at the save, which is when the characters are
' actually at risk, and the document survives a cancel there completely.
'
' Returns TRUE whenever the document's encoding actually changed, so the caller marks the
' document dirty -- a relabel still changes what reaches disk (a BOM added or removed,
' UTF-8 vs UTF-16 vs CP-1252). Returns FALSE only when it was already that encoding.
' ========================================================================================
function ConvertTextBuffer( _
            byval pDoc as clsDocument ptr, _
            byval nNewEncoding as long _
            ) as boolean

    if pDoc = 0 then return false
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then return false

    ' A deliberate encoding change invalidates any earlier "yes, lose those characters"
    ' answer: the question was about a different target encoding.
    pDoc->bLossySaveAccepted = false

    ' THE WHOLE FUNCTION. Every target is a relabel now -- the bytes in the editor are UTF-8
    ' and stay UTF-8 (E2), and nothing is converted or lost until the file is written.
    dim as boolean bChanged = ( pDoc->FileEncoding <> nNewEncoding )
    pDoc->FileEncoding = nNewEncoding
    return bChanged
end function



' ========================================================================================
' NOTE: Utf8ToAnsi / AnsiToUtf8 / UnicodeToUtf8 were REMOVED here. They converted through
' AfxNova DWSTRING, whose conversion properties pass -1 to Win32 and therefore stop at the
' first embedded NUL, silently truncating. All encoding conversion now goes through the
' length-explicit Doc_EncodeForDisk / BytesToWide / WideToAnsi / WideToUtf8 above.
' ========================================================================================


' ========================================================================================
' Determine if a string is likely UTF-8 encoded.
'
' NOTE: pure ASCII passes UTF-8 validation, so an ASCII file is labelled FILE_ENCODING_UTF8
' here, never ANSI. This is harmless (ASCII is byte-identical in both, and Doc_EncodeForDisk
' writes the same bytes either way) and is deliberately left as-is: making ASCII report ANSI
' would change the on-disk save format of every existing plain-text file in every project.
' ========================================================================================
function isUTF8encoded( byref s as string ) as boolean
    if len(s) = 0 then return false

    ' Optional: Detect UTF-8 BOM
    if len(s) >= 3 then
        if asc(s,1)=&hEF and asc(s,2)=&hBB and asc(s,3)=&hBF then return true
    end if

    return PsEncIsValidUtf8( s )
end function
