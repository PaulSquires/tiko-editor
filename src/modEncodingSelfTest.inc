' ########################################################################################
' modEncodingSelfTest.inc  -  see modEncodingSelfTest.bi for the contract.
' ########################################################################################

' Independent oracle: raw Win32 UTF-16 -> UTF-8 (distinct code from modRoutines' private
' helpers, so a shared bug cannot hide behind both). Length-explicit.
private function EncSelfTest_WideToUtf8( byval w as DWSTRING ) as string
    dim as long n = PsLen(w)
    if n = 0 then return ""
    dim as long u8 = WideCharToMultiByte( CP_UTF8, 0, w.Wz(), n, NULL, 0, NULL, NULL )
    if u8 <= 0 then return ""
    dim as string s = string( u8, 0 )
    WideCharToMultiByte( CP_UTF8, 0, w.Wz(), n, strptr(s), u8, NULL, NULL )
    return s
end function

' Write raw bytes to a file with the same stream the save path uses.
private function EncSelfTest_WriteBytes( byval wszPath as DWSTRING, byref sBytes as string ) as boolean
    dim pStream as CFileStream
    if pStream.Open( *wszPath.Wz(), STGM_CREATE or STGM_WRITE ) <> S_OK then return false
    if len(sBytes) then pStream.Write strptr(sBytes), len(sBytes)
    pStream.Close
    return true
end function

' Hex of the first n bytes, for readable failure output.
private function EncSelfTest_HeadHex( byref s as string, byval n as long ) as string
    dim as string r
    for i as long = 1 to iif(n < len(s), n, len(s))
        r &= hex( asc(s, i), 2 ) & " "
    next
    return r
end function


' ========================================================================================
sub Encoding_RunSelfTest()

    if environ("TIKO_ENCODING_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro ENCASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
            dim as DWSTRING wszMsg = msg
            sMsg = "  FAIL: " & wszMsg.Utf8
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_ENCODING_SELFTEST ---"

    ' ---- Corpora (built from explicit code points; source-file encoding independent) ----
    ' wBase : Latin-1 accents only -- representable in every Western SBCS, so ANSI-safe.
    ' wRich : adds Euro / curly quotes / em-dash -- CP-1252 members, multi-width in UTF-8.
    ' wCJK  : adds CJK -- NOT representable in any Western ANSI codepage (the lossy case).
    dim as wstring * 128 wAccents = wchr(&hE1,&hE9,&hED,&hF3,&hFA,&hF6,&hFC)
    dim as wstring * 128 wBase = "Jose " & wAccents & " tail_marker_EOF"
    dim as wstring * 128 wRich = "Jose " & wAccents & " " & _
                                 wchr(&h20AC,&h201C,&h201D,&h2014) & " tail_marker_EOF"
    dim as wstring * 128 wCJK  = "Jose " & wAccents & " " & wchr(&h65E5,&h672C,&h8A9E) & _
                                 " tail_marker_EOF"

    dim as string srcBase = EncSelfTest_WideToUtf8( wBase )   ' editor-form (UTF-8) source
    dim as string srcRich = EncSelfTest_WideToUtf8( wRich )
    dim as string srcCJK  = EncSelfTest_WideToUtf8( wCJK )

    dim as boolean bLossy
    dim as string outA, outU, outUB, outWB

    ' ================= Tier A : pure Doc_EncodeForDisk assertions =================

    outWB = Doc_EncodeForDisk( srcRich, true, FILE_ENCODING_UTF16_BOM, bLossy )
    ' THE truncation assertion: UTF-16 disk length = 2 (BOM) + code-units * 2. The old save
    ' path handed a UTF-16 CHARACTER count to a BYTE-count parameter and wrote half of this.
    ENCASSERT( len(outWB) = 2 + len(wRich) * 2, _
        "UTF-16 length " & str(len(outWB)) & " <> " & str(2 + len(wRich)*2) & " (BOM + 2*chars)" )

    outU  = Doc_EncodeForDisk( srcRich, true, FILE_ENCODING_UTF8, bLossy )
    outUB = Doc_EncodeForDisk( srcRich, true, FILE_ENCODING_UTF8_BOM, bLossy )
    outA  = Doc_EncodeForDisk( srcRich, true, FILE_ENCODING_ANSI, bLossy )

    ' BOM prefixes exact, and absent where they must be absent.
    ENCASSERT( left(outUB, 3) = chr(&HEF,&HBB,&HBF), "UTF-8 BOM prefix wrong: " & EncSelfTest_HeadHex(outUB,3) )
    ENCASSERT( left(outWB, 2) = chr(&HFF,&HFE),      "UTF-16 BOM prefix wrong: " & EncSelfTest_HeadHex(outWB,2) )
    ENCASSERT( left(outU, 3) <> chr(&HEF,&HBB,&HBF), "UTF-8 (no BOM) must not start with a BOM" )
    ENCASSERT( (asc(outA,1) <> &HEF) andalso (asc(outA,1) <> &HFF), "ANSI must not start with a BOM" )

    ' UTF-8 (no BOM) equals the editor source byte-for-byte (source is already UTF-8).
    ENCASSERT( outU = srcRich, "UTF-8 output should equal the UTF-8 source unchanged" )
    ' UTF-8 BOM = BOM + the same UTF-8 payload.
    ENCASSERT( outUB = chr(&HEF,&HBB,&HBF) & srcRich, "UTF-8 BOM output = BOM + UTF-8 payload" )

    ' Tail marker survives to the end of every encoding (truncation guard).
    ENCASSERT( instr(outA,  "tail_marker_EOF") > 0, "ANSI output lost the tail marker" )
    ENCASSERT( instr(outU,  "tail_marker_EOF") > 0, "UTF-8 output lost the tail marker" )
    ENCASSERT( instr(outUB, "tail_marker_EOF") > 0, "UTF-8 BOM output lost the tail marker" )
    ' UTF-16: last code unit is 'F' (0x46 0x00).
    ENCASSERT( (asc(outWB, len(outWB)-1) = &H46) andalso (asc(outWB, len(outWB)) = &H00), _
        "UTF-16 output does not end in 'F',00 -- tail truncated" )

    ' Round-trip through ANSI reproduces the source exactly (Latin-1 subset only).
    dim as string aBase = Doc_EncodeForDisk( srcBase, true, FILE_ENCODING_ANSI, bLossy )
    ENCASSERT( bLossy = false, "Latin-1 accents wrongly reported lossy for ANSI (system codepage?)" )
    dim as string backU = Doc_EncodeForDisk( aBase, false, FILE_ENCODING_UTF8, bLossy )
    ENCASSERT( backU = srcBase, "ANSI round-trip did not reproduce the UTF-8 source" )

    ' Embedded NUL survives (old strlen path stopped at the first NUL).
    dim as string srcNul = "AB" & chr(0) & "CD"
    dim as string outNul = Doc_EncodeForDisk( srcNul, true, FILE_ENCODING_UTF8, bLossy )
    ENCASSERT( len(outNul) = 5, "embedded NUL truncated: length " & str(len(outNul)) & " <> 5" )

    ' Lossy detection: CJK -> ANSI is lossy; Latin-1 -> ANSI is not.
    Doc_EncodeForDisk( srcCJK, true, FILE_ENCODING_ANSI, bLossy )
    ENCASSERT( bLossy = true,  "CJK -> ANSI should be flagged lossy" )
    Doc_EncodeForDisk( srcBase, true, FILE_ENCODING_ANSI, bLossy )
    ENCASSERT( bLossy = false, "Latin-1 -> ANSI should not be flagged lossy" )

    ' ============ Disk round-trip : encode -> write -> Doc_ReadFromDisk ============
    dim as wstring * MAX_PATH wFolder = environ("TEMP")
    if len(wFolder) = 0 then wFolder = "."

    #macro DISKTRIP( enc, encName, expectEnc, expectRecovered )
        scope
            dim as boolean bL
            dim as string sDisk = Doc_EncodeForDisk( srcRich, true, enc, bL )
            dim as string sPath = GetTemporaryFilename( wFolder, "txt" )
            if EncSelfTest_WriteBytes( sPath, sDisk ) = false then
                nFail += 1 : ? "  FAIL: could not write temp file for " & encName
            else
                dim d as clsDocument
                dim as string sRecovered
                dim as long nRdEnc = FILE_ENCODING_UTF8
                dim as DWSTRING wszRdErr
                Doc_ReadFromDisk( sPath, sRecovered, nRdEnc, wszRdErr )
                d.FileEncoding = nRdEnc
                ENCASSERT( d.FileEncoding = expectEnc, _
                    encName & ": detected encoding " & str(d.FileEncoding) & " <> " & str(expectEnc) )
                ENCASSERT( sRecovered = expectRecovered, _
                    encName & ": recovered buffer differs (len " & str(len(sRecovered)) & _
                    " vs " & str(len(expectRecovered)) & ") -- truncation/corruption on round-trip" )
                PsFileDelete( sPath )
            end if
        end scope
    #endmacro

    ' UTF-8 / UTF-8 BOM / UTF-16 BOM all recover to the editor-form UTF-8 source.
    DISKTRIP( FILE_ENCODING_UTF8,      "UTF-8",      FILE_ENCODING_UTF8,      srcRich )
    DISKTRIP( FILE_ENCODING_UTF8_BOM,  "UTF-8 BOM",  FILE_ENCODING_UTF8_BOM,  srcRich )
    DISKTRIP( FILE_ENCODING_UTF16_BOM, "UTF-16 BOM", FILE_ENCODING_UTF16_BOM, srcRich )
    ' ANSI file: the reader leaves the bytes ANSI (no conversion), so the recovered buffer is
    ' the ANSI encoding of the source -- compared ANSI-to-ANSI, which is robust to whether the
    ' system codepage can represent every character.
    '
    ' THIS ASSERTION CHANGES IN THE NEXT COMMIT. Once ANSI is a disk format only, the reader
    ' decodes it to UTF-8 like everything else and the recovered buffer becomes srcRich. It is
    ' left alone here so that this commit is a swap of readers and nothing else.
    DISKTRIP( FILE_ENCODING_ANSI,      "ANSI",       FILE_ENCODING_ANSI,      outA )

    ' Localization ids owned by the lossy prompt must exist (blank = silent failure).
    ENCASSERT( len(LL(316)) > 0, "L(316) lossy-prompt body is blank -- missing from english.lang" )
    ENCASSERT( len(LL(317)) > 0, "L(317) lossy-prompt title is blank -- missing from english.lang" )

    ' ================= Default encoding for new files =================
    ' Three separate claims, kept apart on purpose -- the name table, the index/value
    ' identity the Options combo relies on, and the actual honouring of the setting.

    ' (a) Every encoding has a distinct, non-empty name, and an out-of-range value answers
    '     ANSI rather than "". A blank name would put an empty row in the combo and an
    '     empty statusbar panel, neither of which reports anything.
    for e as long = FILE_ENCODING_ANSI to FILE_ENCODING_UTF16_BOM
        ENCASSERT( len(Doc_EncodingName(e)) > 0, _
            "Doc_EncodingName(" & str(e) & ") is blank" )
        for f as long = FILE_ENCODING_ANSI to FILE_ENCODING_UTF16_BOM
            if f <= e then continue for
            ENCASSERT( Doc_EncodingName(e) <> Doc_EncodingName(f), _
                "Doc_EncodingName: " & str(e) & " and " & str(f) & " share the name """ & _
                Doc_EncodingName(e) & """" )
        next
    next
    ENCASSERT( Doc_EncodingName(-1) = Doc_EncodingName(FILE_ENCODING_ANSI), _
        "Doc_EncodingName(-1) should fall back to the ANSI name" )
    ENCASSERT( Doc_EncodingName(99) = Doc_EncodingName(FILE_ENCODING_ANSI), _
        "Doc_EncodingName(99) should fall back to the ANSI name" )

    ' (b) The index/value identity the Options combo is built on is asserted in
    '     OptionsRows_RunBindTest (TIKO_OPTIONS_SELFTEST) instead of here: gOptRows and
    '     gOptWork are declared by modOptionsRows, which tiko.bas includes well AFTER this
    '     file, so naming either of them here would not compile.

    ' (c) A NEW DOCUMENT ACTUALLY TAKES THE SETTING. This is the only assertion that would
    '     fail if the honouring line in clsApp.CreateEmptyDocument were deleted; (a) and (b)
    '     are both blind to it. Driven for every encoding rather than one, so a hardcoded
    '     constant left behind cannot pass by coincidentally matching the default.
    '     gConfig is written here, so it is snapshotted and restored (Theme_RunSelfTest's
    '     precedent), and every document created is removed again.
    scope
        dim as long nSaved = gConfig.NewFileEncoding
        for e as long = FILE_ENCODING_ANSI to FILE_ENCODING_UTF16_BOM
            gConfig.NewFileEncoding = e
            dim pNew as clsDocument ptr = gApp.CreateEmptyDocument( true )
            if pNew = 0 then
                nFail += 1 : ? "  FAIL: CreateEmptyDocument returned null"
            else
                ENCASSERT( pNew->FileEncoding = e, _
                    "a new document took encoding " & str(pNew->FileEncoding) & _
                    " with gConfig.NewFileEncoding = " & str(e) )
                gApp.RemoveDocument( pNew )
            end if
        next
        gConfig.NewFileEncoding = nSaved
        ENCASSERT( gConfig.NewFileEncoding = nSaved, "failed to restore gConfig.NewFileEncoding" )
    end scope

    sMsg = "--- encoding self-test: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sMsg

end sub
