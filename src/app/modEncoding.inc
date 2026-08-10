'' ========================================================================================
'' modEncoding -- the WRITE path, in the app layer.
''
'' Doc_EncodeForDisk and Doc_WriteToDisk. Their DECLARATIONS were already in
'' app/modEncoding.bi; only the bodies were still in src/modEncoding.inc, which nothing
'' noticed until clsDocument moved down and started calling them from inside the layer.
''
'' THE ONLY WIN32 IN EITHER WAS AN ERROR MESSAGE. Doc_WriteToDisk is PsFileExists,
'' PsFileWriteAll and PsFileReplace throughout; the two Win32ErrorText(GetLastError()) calls
'' produced TEXT, and both already fell back to a fixed string when the platform offered
'' nothing. They go through gAppHost.LastErrorText now, so a host without an equivalent loses
'' detail and never correctness.
''
'' The READ path stays in the shell: it decodes UTF-16 through WideCharToMultiByte.
'' ========================================================================================

#pragma once

#include once "modEncoding.bi"



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
        wszErr = gAppHost.LastErrorText()
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
        wszErr = gAppHost.LastErrorText()
        if PsLen( wszErr ) = 0 then wszErr = "Could not replace the existing file."
        PsFileDelete( wszTemp )     ' never leave a stray .tiko-tmp behind
        return DOCWRITE_FAILED
    end if

    return DOCWRITE_OK
end function


' ========================================================================================
' The display name of a FILE_ENCODING_* value -- the SINGLE source for all four strings.
'
' MOVED HERE in 7c step 3 commit 7, from src/modEncoding.inc. Its declaration has been in
' app/modEncoding.bi all along; only the body lagged, exactly as Doc_EncodeForDisk's and
' Doc_WriteToDisk's did in commit 5. Nothing about it was ever shell-side -- it is a Select
' Case over constants -- and the shell's lossy-save prompt, which names the encoding it is
' about to lose characters to, is what finally asked for it.
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
