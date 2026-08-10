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


' ========================================================================================
' The display name of a FILE_ENCODING_* value -- the SINGLE source for all four strings.
'
' There were two hardcoded copies of this list before (the statusbar panel in
' frmMain_SetStatusbar and the statusbar popup in ShowFileEncodingContextMenu), and the
' Options page's "encoding for new files" combo would have made a third. They are now all
' driven from here, which is what makes the combo's ITEM ORDER and the FILE_ENCODING_*
' VALUES provably the same thing: the combo is filled by walking ANSI..UTF16_BOM through
' this function, so the selected index IS the encoding constant and there is no mapping
' table anywhere to fall out of step.
'
' NOT localized, deliberately: these are the encodings' own names, and both call sites this
' replaces spelled them in English under every language file. Changing that is a separate
' decision from adding a setting, and would need all six .lang files either way.
'
' An unknown value answers "ANSI", matching the `case else` the statusbar has always had --
' encoding 0 is the one a zeroed/garbage field lands on.
' ========================================================================================
function Doc_EncodingName( byval nEnc as long ) as DWSTRING
    select case nEnc
        case FILE_ENCODING_UTF8      : return "UTF-8"
        case FILE_ENCODING_UTF8_BOM  : return "UTF-8 (BOM)"
        case FILE_ENCODING_UTF16_BOM : return "UTF-16 (BOM)"
        case else                    : return "ANSI"
    end select
end function


' ---------------------------------------------------------------------------
' PsFileWriteAll AND PsFileReplace WERE HERE AND ARE GONE.
'
' They were tiko-local implementations on CFileStream and ReplaceFileW, written
' while PsCore's DWSTRING was still behind `namespace PsC` and its PsFile could
' not be reached unqualified. The swap makes PsCore's the ones in scope, and two
' definitions of the same name in one unity build is a hard error rather than a
' silent shadow -- which is the good outcome, because the alternative was two
' writers with the same name and different atomicity guarantees.
'
' PsFile.inc's versions do the same two jobs: verify every byte landed (a short
' write is a full disk, and reporting success for one is how a truncated file
' reaches the user looking saved), and prefer ReplaceFileW so the destination's
' ACLs, alternate data streams, creation time and object id survive.
' ---------------------------------------------------------------------------


' ========================================================================================
' tiko's FILE_ENCODING_* to PsCore's PsEncodingId. The two lists were written years and a
' project apart and agree one for one, which is why this is a Select Case and not a design.
' ========================================================================================
private function Doc_EncToPs( byval nEnc as long ) as PsEncodingId
    select case nEnc
        case FILE_ENCODING_ANSI      : return PSENC_ANSI
        case FILE_ENCODING_UTF8      : return PSENC_UTF8
        case FILE_ENCODING_UTF8_BOM  : return PSENC_UTF8_BOM
        case FILE_ENCODING_UTF16_BOM : return PSENC_UTF16LE_BOM
    end select
    ' Same fallback the old code had: an unknown target writes UTF-8.
    return PSENC_UTF8
end function


' ========================================================================================
' Produce the exact bytes to write to disk for a document, BOM included.
'
'   sBuffer       - the raw Scintilla bytes (from Scintilla_GetTextBytes)
'   bSourceIsUtf8 - the CURRENT in-editor layout, read from SCI_GETCODEPAGE (NOT from
'                   FileEncoding, which may already have been changed to the target)
'   nTargetEnc    - one of FILE_ENCODING_*
'   bLossy        - out: true if the target could not represent every character (ANSI only)
'
' Pure: string in, string out, no window and no globals. This is the single writer that
' clsDocument.SaveFile / SaveTempFile and frmMainFile's duplicate-file path all share; it
' is also what the Tier-A self-test drives directly.
'
' ---- NOW ON PsEncoding, THROUGH DWSTRING --------------------------------------------
'
' The first attempt at this used AfxNova's DWSTRING as the intermediate and failed 14
' assertions in the encoding self-test -- "embedded NUL truncated: length 0 <> 5" among
' them. A document may legitimately contain a NUL, and AfxNova's conversions stop at one.
'
' PsCore's DWSTRING does not: "length is authoritative, not the terminator" is rule 1 of
' its header, written after exactly this class of bug. It is reached here as DWSTRING,
' because the unqualified name still means AfxNova's for tiko's other 1631 sites -- see
' the namespace note in tiko.bas.
'
' There is no second length to get wrong any more. The comment that used to live here
' warned that the wide buffer's LENGTH IN CHARACTERS had once been handed to a BYTE-count
' parameter, truncating every UTF-16 save in half; PsEncEncode returns the finished byte
' string, BOM included, so the three cases that used to concatenate one by hand are gone
' with it.
'
' ---- AND ONE BEHAVIOUR CHANGES ----------------------------------------------------------
'
' ANSI was CP_ACP -- whatever codepage the machine is set to. PsEncoding is CP-1252,
' always, on both platforms. On a Western Windows install the two are the same mapping and
' nothing moves. On a Shift-JIS or GBK machine an ANSI save now writes CP-1252 and reports
' more characters as lossy than it used to.
'
' That is deliberate and it is the price of determinism: Linux has no CP_ACP to ask, and a
' locale-dependent conversion means one document saves differently on two computers, which
' is the property this whole port exists to remove. UTF-8 and UTF-16 -- the default and the
' overwhelming majority of real files -- are untouched.
' ========================================================================================
function Doc_EncodeForDisk( _
            byref sBuffer as string, _
            byval bSourceIsUtf8 as boolean, _
            byval nTargetEnc as long, _
            byref bLossy as boolean _
            ) as string

    bLossy = false

    ' The in-editor buffer carries no BOM -- it is Scintilla's text, not a file -- so it
    ' decodes as the plain form of whichever layout SCI_GETCODEPAGE reports.
    dim as DWSTRING dwsText = PsEncDecode( sBuffer, _
            iif(bSourceIsUtf8, PSENC_UTF8, PSENC_ANSI) )

    return PsEncEncode( dwsText, Doc_EncToPs( nTargetEnc ), bLossy )
end function


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
' Write a byte buffer to disk ATOMICALLY, and report whether it worked.
'
' Returns one of the DOCWRITE_* codes; wszErr receives a human-readable reason on failure
' (already localized by the caller's standards -- it is a Win32 error string, not a phrase).
'
' WHY THIS EXISTS. Every save path used to do this:
'
'     if PsFileExists(f) then PsFileDelete(f)      <-- the user's file, gone
'     if pStream.Open(f, STGM_CREATE or STGM_WRITE) = S_OK then
'         pStream.Write strptr(sOut), len(sOut)      <-- result discarded
'     end if                                          <-- no else: failure invisible
'
' The delete happened BEFORE the write, so any failure to open -- disk full, an antivirus
' lock, a read-only volume, a dropped network share, the file open in another process --
' destroyed the document with nothing left to restore it from. The write's byte count was
' thrown away, so a short write (disk full) truncated the file silently. And because the
' caller could not tell, it went on to clear the dirty flag and send SCI_SETSAVEPOINT, so
' the tab stopped showing modified and the close prompt never fired: the user exited
' believing the work was saved.
'
' The order here is the only order that is safe: write the WHOLE new file somewhere else
' first, verify every byte landed, and only then swap it into place in one step. Until the
' swap succeeds the original is untouched, so every failure above leaves the user exactly
' where they started.
'
' NOW WRITTEN IN PsCore's VOCABULARY. The order and the guarantees are unchanged -- this
' function was already correct, and porting it was never about fixing it. What changed is
' that it no longer NAMES CreateFileW, ReplaceFileW or MoveFileExW: it says PsFileWriteAll
' and PsFileReplace, which is what it will still say once those are PsCore's own
' implementations rather than the shims in PsCompat.bi.
'
' PsFileReplace keeps the same two-step preference for the same reason the Win32 code had
' it: a replace carries the target's ACLs, alternate data streams, creation time and
' object id across, and refuses outright when the target does not exist -- which is every
' Save As and every new file, hence the atomic-move fallback.
' ========================================================================================
function Doc_WriteToDisk( _
            byval wszPath as DWSTRING, _
            byref sBytes as string, _
            byref wszErr as DWSTRING _
            ) as long

    wszErr = ""
    if PsLen( wszPath ) = 0 then
        wszErr = "No filename."
        return DOCWRITE_FAILED
    end if

    ' The temp file MUST be a sibling of the target: an atomic replace requires one
    ' volume, and %TEMP% is frequently on another.
    dim as DWSTRING wszTemp = wszPath & ".tiko-tmp"
    if PsFileExists( wszTemp ) then PsFileDelete( wszTemp )

    ' PsFileWriteAll verifies that EVERY byte landed. A short write is a full disk, and
    ' reporting success for one is how a truncated file reaches the user looking saved.
    if PsFileWriteAll( wszTemp, sBytes ) = false then
        wszErr = Win32ErrorText( GetLastError() )
        if PsLen( wszErr ) = 0 then wszErr = "Could not write the temporary file."
        PsFileDelete( wszTemp )
        return DOCWRITE_FAILED
    end if

    ' ---- the swap ----------------------------------------------------------------------
    ' Until this succeeds the original is untouched, so every failure above leaves the user
    ' exactly where they started. PsFileReplace prefers a replace that carries the target's
    ' ACLs, streams, creation time and object id across, and falls back to an atomic move
    ' for the case a replace refuses -- the target not existing, which is every Save As.
    if PsFileReplace( wszTemp, wszPath ) = false then
        wszErr = Win32ErrorText( GetLastError() )
        if PsLen( wszErr ) = 0 then wszErr = "Could not replace the existing file."
        PsFileDelete( wszTemp )     ' never leave a stray .tiko-tmp behind
        return DOCWRITE_FAILED
    end if

    return DOCWRITE_OK
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
