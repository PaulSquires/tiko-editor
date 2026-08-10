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
' Convert the current in-editor text buffer to the byte layout the specified encoding
' needs (ANSI codepage 0, or UTF-8 codepage 65001), and relabel the document.
'
' Returns TRUE whenever the document's encoding actually changed -- INCLUDING a pure relabel
' (e.g. UTF-8 -> UTF-8 BOM), because a relabel still changes what is written to disk (a BOM
' added/removed, UTF-8 vs UTF-16), so the caller must mark the document dirty and let it be
' saved. Returns FALSE only when nothing changed: the document is already that encoding, or a
' lossy ANSI conversion was cancelled by the user.
'
' Scintilla holds exactly two layouts, ANSI or UTF-8; UTF-16 is a DISK format only and its
' in-memory representation is UTF-8. So UTF8 / UTF8_BOM / UTF16_BOM all share one in-editor
' layout, and switching between them is a relabel, not a conversion.
'
' INVARIANT E1: on return, SCI_GETCODEPAGE = iif(FileEncoding = ANSI, 0, SC_CP_UTF8), and
' this function is the ONLY code permitted to move either side of that on a populated
' document (ApplyProperties establishes it on load; nothing else may touch the codepage).
' ========================================================================================
function ConvertTextBuffer( _
            byval pDoc as clsDocument ptr, _
            byval nNewEncoding as long _
            ) as boolean

    if pDoc = 0 then return false
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then return false

    dim as boolean bHaveUtf8 = ( SciExec( hEdit, SCI_GETCODEPAGE, 0, 0 ) = SC_CP_UTF8 )
    dim as boolean bWantUtf8 = ( nNewEncoding <> FILE_ENCODING_ANSI )

    ' Pure relabel: the in-memory layout already matches the target (e.g. UTF-8 -> UTF-8 BOM,
    ' or UTF-8 BOM -> UTF-16 BOM). No byte conversion, only the disk format changes -- but
    ' that IS a change (the BOM must reach disk), so report it as one when the label moved.
    ' A deliberate encoding change invalidates any earlier "yes, lose those characters"
    ' answer: the question was about a different target encoding.
    pDoc->bLossySaveAccepted = false

    if bHaveUtf8 = bWantUtf8 then
        dim as boolean bChanged = ( pDoc->FileEncoding <> nNewEncoding )
        pDoc->FileEncoding = nNewEncoding
        return bChanged
    end if

    ' Save caret and first-visible line as LINE + COLUMN. Byte offsets shift when the
    ' encoding changes (ANSI grows to UTF-8), so a saved byte position would land in the
    ' wrong place, possibly mid-character.
    dim as long nPos       = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nPos, 0 )
    dim as long nCaretCol  = SciExec( hEdit, SCI_GETCOLUMN, nPos, 0 )
    dim as long nFirstLine = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0 )

    ' Decode the current buffer to UTF-16, then re-encode to the wanted layout.
    dim as string sBuffer = Scintilla_GetTextBytes( SciHost_DirectPointer(hEdit) )
    dim as DWSTRING dwsText = PsEncDecode( sBuffer, _
            iif(bHaveUtf8, PSENC_UTF8, PSENC_ANSI) )

    dim as string sNew
    dim as long nNewCodePage
    dim as boolean bLossy
    if bWantUtf8 then
        sNew = PsEncEncode( dwsText, PSENC_UTF8, bLossy )
        nNewCodePage = SC_CP_UTF8
    else
        sNew = PsEncEncode( dwsText, PSENC_ANSI, bLossy )
        if bLossy then
            ' Converting to ANSI will destroy characters outside the system ANSI codepage,
            ' and SetText empties the undo buffer, so there is no going back. Confirm first.
            ' The dialog lives in modEncodingUi.inc. This function decides WHETHER to
            ' ask; it does not decide what the asking looks like.
            if Doc_ConfirmAnsiConversion() = false then return false
        end if
        nNewCodePage = 0
    end if

    ' Apply in the safe order: clear, THEN set the codepage on the empty buffer, THEN insert
    ' the already-converted bytes. Setting the codepage first over the old bytes would make
    ' Scintilla re-validate UTF-8 and fix up positions over the wrong layout.
    SciExec( hEdit, SCI_CLEARALL, 0, 0 )
    SciExec( hEdit, SCI_SETCODEPAGE, nNewCodePage, 0 )
    pDoc->SetText( sNew )

    ' Assign the label LAST, only after the bytes are in place, so a cancel above leaves the
    ' document wholly untouched (INVARIANT E1 preserved on every exit path).
    pDoc->FileEncoding = nNewEncoding

    ' Restore caret and scroll by line + column.
    dim as long nRestore = SciExec( hEdit, SCI_FINDCOLUMN, nCaretLine, nCaretCol )
    SciExec( hEdit, SCI_SETSEL, nRestore, nRestore )
    SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, nFirstLine, 0 )

    return true
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
