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
' Note UTF-16 does NOT go through DWSTRING: the wide buffer BytesToWide produced already IS
' the UTF-16LE payload, and its byte length is code-units * 2. The old code handed
' DWSTRING.m_BufferLen (a COUNT OF UTF-16 CHARACTERS) to CFileStream.Write's BYTE-count
' parameter, writing exactly half the file and truncating every UTF-16 save mid-character.
' ========================================================================================
function Doc_EncodeForDisk( _
            byref sBuffer as string, _
            byval bSourceIsUtf8 as boolean, _
            byval nTargetEnc as long, _
            byref bLossy as boolean _
            ) as string

    bLossy = false
    dim as long nWide
    dim as string sWide = BytesToWide( sBuffer, iif(bSourceIsUtf8, CP_UTF8, CP_ACP), nWide )

    select case nTargetEnc
        case FILE_ENCODING_ANSI
            return WideToAnsi( sWide, nWide, bLossy )

        case FILE_ENCODING_UTF8
            return WideToUtf8( sWide, nWide )

        case FILE_ENCODING_UTF8_BOM
            return chr(&HEF, &HBB, &HBF) & WideToUtf8( sWide, nWide )

        case FILE_ENCODING_UTF16_BOM
            ' FF FE (little-endian BOM) + the wide payload, byte length = code-units * 2.
            return chr(&HFF, &HFE) & left( sWide, nWide * 2 )

        case else
            return WideToUtf8( sWide, nWide )
    end select

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
    return rtrim( wszOut, any !" \r\n" )
end function


' ========================================================================================
' Write a byte buffer to disk ATOMICALLY, and report whether it worked.
'
' Returns one of the DOCWRITE_* codes; wszErr receives a human-readable reason on failure
' (already localized by the caller's standards -- it is a Win32 error string, not a phrase).
'
' WHY THIS EXISTS. Every save path used to do this:
'
'     if AfxFileExists(f) then AfxDeleteFile(f)      <-- the user's file, gone
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
' ReplaceFileW rather than delete-then-rename: it preserves the target's ACLs, its
' alternate data streams, its creation time and its object id, which a delete throws away.
' It requires the replacement to be on the SAME VOLUME, which is why the temp file is put
' in the target's own directory and not in %TEMP%. MoveFileExW is the fallback for the
' cases ReplaceFileW refuses (notably: the target does not exist yet, which is every
' Save As and every new file).
' ========================================================================================
function Doc_WriteToDisk( _
            byval wszPath as DWSTRING, _
            byref sBytes as string, _
            byref wszErr as DWSTRING _
            ) as long

    wszErr = ""
    if len( wszPath ) = 0 then
        wszErr = "No filename."
        return DOCWRITE_FAILED
    end if

    ' The temp file MUST be a sibling of the target: ReplaceFileW and an atomic
    ' MoveFileExW both require one volume, and %TEMP% is frequently on another.
    dim as DWSTRING wszTemp = wszPath & ".tiko-tmp"
    if AfxFileExists( wszTemp ) then AfxDeleteFile( wszTemp )

    dim as boolean bWritten = false
    scope
        dim pStream as CFileStream
        if pStream.Open( wszTemp, STGM_CREATE or STGM_WRITE ) <> S_OK then
            wszErr = Win32ErrorText( GetLastError() )
            if len( wszErr ) = 0 then wszErr = "Could not create the temporary file."
            return DOCWRITE_FAILED
        end if

        ' An empty document is a legal save. CFileStream.Write rejects a null pv with
        ' E_INVALIDARG, and STRPTR of a zero-length FB string may well be null, so the
        ' zero-byte case must skip the call rather than "fail" on it.
        if len( sBytes ) = 0 then
            bWritten = true
        else
            dim as ulong cbWrote = pStream.Write( strptr(sBytes), len(sBytes) )
            bWritten = ( cbWrote = culng(len(sBytes)) )
            if bWritten = false then
                wszErr = "Wrote " & cbWrote & " of " & len(sBytes) & " bytes (disk full?)."
            end if
        end if
        ' Close releases the IStream, which is what flushes it. It has to happen before
        ' the swap below, so the scope ends here rather than at the end of the function.
    end scope

    if bWritten = false then
        AfxDeleteFile( wszTemp )
        return DOCWRITE_FAILED
    end if

    ' ---- the swap ----------------------------------------------------------------------
    '
    ' RETRIED, because the commonest failure here is TRANSIENT AND NOT THE USER'S DOING.
    ' Both calls need to delete or rename the target, which Windows refuses while ANY
    ' handle is open on it without FILE_SHARE_DELETE -- and tiko itself opens exactly
    ' such handles: fbcParser reads every include with "open ... for binary access read"
    ' (fbcParser\fb.bas), on the scan worker thread, for the ~3 s a project scan lasts.
    ' Save-all before a compile (modCompile) lands inside that window at random. Antivirus
    ' and the search indexer do the same thing to the freshly closed .tiko-tmp.
    '
    ' MEASURED with a probe against this exact sequence: with such a handle open,
    ' ReplaceFileW fails 32 (SHARING_VIOLATION) and MoveFileExW fails 5 (ACCESS_DENIED);
    ' with FILE_SHARE_DELETE granted, both succeed. That is why the box said "Access
    ' denied" on a folder the user owned -- it is a HELD HANDLE, not a permission.
    '
    ' Ten tries, 25 ms apart: a quarter-second worst case, invisible against a save the
    ' user asked for, and long enough to outlast a scan moving off this file. A failure
    ' that is NOT a sharing failure exits at once -- a real access denial does not heal by
    ' being asked again, and a user owed an error message should not wait for it.
    const SWAP_TRIES  = 10
    const SWAP_WAITMS = 25

    dim as boolean bSwapped = false
    dim as DWORD dwErr = 0

    for nTry as long = 1 to SWAP_TRIES
        dim as boolean bTargetExists = AfxFileExists( wszPath )
        if bTargetExists then
            if ReplaceFileW( wszPath, wszTemp, 0, _
                             REPLACEFILE_IGNORE_MERGE_ERRORS, 0, 0 ) <> 0 then
                bSwapped = true
                exit for
            end if
            ' KEEP THIS ERROR, not MoveFileExW's. ReplaceFileW reports the sharing
            ' violation by name (32); MoveFileExW reports the same condition as a bare 5,
            ' and reporting the 5 is what sent the reader off to check permissions.
            dwErr = GetLastError()
        end if

        ' Either the target did not exist (the common Save As / new-file case, which
        ' ReplaceFileW refuses outright) or ReplaceFileW declined it. MoveFileExW with
        ' REPLACE_EXISTING is still atomic on NTFS; it simply does not carry the
        ' target's metadata across.
        if MoveFileExW( wszTemp, wszPath, _
                        MOVEFILE_REPLACE_EXISTING or MOVEFILE_WRITE_THROUGH ) <> 0 then
            bSwapped = true
            exit for
        end if
        if bTargetExists = false then dwErr = GetLastError()

        if (dwErr <> ERROR_SHARING_VIOLATION) andalso (dwErr <> ERROR_ACCESS_DENIED) then exit for
        if nTry < SWAP_TRIES then sleep SWAP_WAITMS, 1     ' ,1 = not interruptible by a keypress
    next

    if bSwapped = false then
        wszErr = Win32ErrorText( dwErr )
        if len( wszErr ) = 0 then wszErr = "Could not replace the existing file."
        AfxDeleteFile( wszTemp )     ' never leave a stray .tiko-tmp behind
        return DOCWRITE_FAILED
    end if

    return DOCWRITE_OK
end function


' ========================================================================================
' Confirm a save that cannot represent every character. Returns TRUE to go ahead.
'
' Doc_EncodeForDisk has ALWAYS reported this, through its byref bLossy, and the encoding
' self-test has always asserted that it reports it correctly. Every real save site declared
' the variable and then never looked at it:
'
'     dim as boolean bLossy
'     dim as string sOut = Doc_EncodeForDisk( ..., this.FileEncoding, bLossy )
'     pStream.Write strptr(sOut), len(sOut)      ' bLossy never examined
'
' The only consumer was the explicit "change encoding" command, which does warn. So saving
' an ANSI-labelled document holding CJK or emoji replaced those characters with "?"
' silently, permanently, on every Ctrl+S -- while the suite stayed green, because it tests
' the function that works rather than the call sites that ignore it.
'
' Answered once per document (pDoc->bLossySaveAccepted): a warning, not a nag. pDoc may be
' NULL, in which case it is asked every time.
' ========================================================================================
function Doc_ConfirmLossySave( _
            byval pDoc as clsDocument ptr, _
            byval wszPath as DWSTRING, _
            byval nEncoding as long _
            ) as boolean

    ' TEST HOOK. Zero in every normal run; the save self-test sets it to force an answer
    ' without a modal box. It exists because the defect being guarded was in the CALL SITES,
    ' not in this function -- and a suite that can only reach the function is blind to
    ' exactly that, which is how the original bug survived a green encoding suite. Same idea
    ' as PsMessageBox_LayoutForTest in the control family.
    if gLossySaveTestAnswer <> 0 then return (gLossySaveTestAnswer > 0)

    ' "pDoc <> 0", not a bare "pDoc": fbc warns 38 on mixing a pointer with a boolean in
    ' andalso, and a warning-free build is the only signal this tree has.
    if (pDoc <> 0) andalso pDoc->bLossySaveAccepted then return true

    dim as long nAnswer = TikoMsgBox( HWND_FRMMAIN, _
            L(516, "Some characters in this document cannot be represented in") & _
            " " & Doc_EncodingName( nEncoding ) & "." & vbcrlf & vbcrlf & _
            wszPath & vbcrlf & vbcrlf & _
            L(517, "Saving will replace them and cannot be undone. Change the file's " & _
                   "encoding first to keep them."), _
            L(518, "Characters will be lost"), _
            TMB_ICON_WARNING, TMB_OKCANCEL, 1 )   ' default = Cancel: this is destructive

    if nAnswer <> IDOK then return false
    if pDoc then pDoc->bLossySaveAccepted = true
    return true
end function


' ========================================================================================
' Report a failed save to the user.
'
' Split out because all three save paths must say the same thing, and because a save that
' fails silently is the defect this whole group of functions exists to remove.
' ========================================================================================
sub Doc_ReportWriteFailure( byval wszPath as DWSTRING, byval wszErr as DWSTRING )
    TikoMsgBox( HWND_FRMMAIN, _
                L(507, "The file could not be saved.") & vbcrlf & vbcrlf & _
                wszPath & vbcrlf & vbcrlf & _
                wszErr & vbcrlf & vbcrlf & _
                L(508, "The file on disk has not been changed."), _
                L(509, "Save failed"), _
                TMB_ICON_NONE, TMB_OK )
end sub


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
    dim as string sBuffer = Scintilla_GetTextBytes( hEdit )
    dim as long nWide
    dim as string sWide = BytesToWide( sBuffer, iif(bHaveUtf8, CP_UTF8, CP_ACP), nWide )

    dim as string sNew
    dim as long nNewCodePage
    if bWantUtf8 then
        sNew = WideToUtf8( sWide, nWide )
        nNewCodePage = SC_CP_UTF8
    else
        dim as boolean bLossy
        sNew = WideToAnsi( sWide, nWide, bLossy )
        if bLossy then
            ' Converting to ANSI will destroy characters outside the system ANSI codepage,
            ' and SetText empties the undo buffer, so there is no going back. Confirm first.
            dim as long nAnswer = MessageBoxW( HWND_FRMMAIN, _
                    L(316,"This document contains characters that cannot be represented in ANSI. " & _
                          "Converting will replace them and cannot be undone. Convert anyway?"), _
                    L(317,"Convert to ANSI"), _
                    MB_OKCANCEL or MB_ICONEXCLAMATION or MB_DEFBUTTON2 )
            if nAnswer <> IDOK then return false
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

    dim as long wlen = MultiByteToWideChar( _
        CP_UTF8, _
        MB_ERR_INVALID_CHARS, _   ' strict validation
        strptr(s), _
        len(s), _
        NULL, _
        0)

    return (wlen <> 0)
end function
