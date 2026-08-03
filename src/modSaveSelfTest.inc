' ########################################################################################
' modSaveSelfTest.inc  -  see modSaveSelfTest.bi for the contract.
' ########################################################################################

' Read a whole file back as raw bytes, through code that is NOT the writer under test.
private function SaveSelfTest_ReadAll( byref wszPath as wstring ) as string
    dim as HANDLE h = CreateFileW( @wszPath, GENERIC_READ, _
                                   FILE_SHARE_READ or FILE_SHARE_WRITE, 0, _
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 )
    if h = INVALID_HANDLE_VALUE then return ""
    dim as DWORD cb = GetFileSize( h, 0 ), cbRead
    dim as string s
    if cb then
        s = string( cb, 0 )
        ReadFile( h, strptr(s), cb, @cbRead, 0 )
        if cbRead <> cb then s = left( s, cbRead )
    end if
    CloseHandle( h )
    return s
end function


' ========================================================================================
sub Save_RunSelfTest()

    if environ("TIKO_SAVE_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro SAVEASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            sMsg = "  FAIL: " & msg
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_SAVE_SELFTEST ---"

    dim as DWSTRING wszDir  = environ("TEMP") & "\tiko_savetest"
    AfxCreateDirectory( wszDir )
    dim as DWSTRING wszFile = wszDir & "\doc.bas"
    dim as DWSTRING wszTmp  = wszFile & ".tiko-tmp"
    dim as DWSTRING wszErr
    dim as string sBack

    ' ---- 1. happy path -----------------------------------------------------------------
    AfxDeleteFile( wszFile )
    dim as string sV1 = "print ""version one""" & chr(13,10)
    SAVEASSERT( Doc_WriteToDisk( wszFile, sV1, wszErr ) = DOCWRITE_OK, _
                "create of a new file failed: " & wszErr )
    sBack = SaveSelfTest_ReadAll( wszFile )
    SAVEASSERT( sBack = sV1, "new file bytes differ (got " & len(sBack) & " of " & len(sV1) & ")" )
    SAVEASSERT( AfxFileExists( wszTmp ) = false, "temp file left behind after a successful create" )

    ' ---- 2. overwrite an existing file --------------------------------------------------
    dim as string sV2 = "print ""version two, which is a good deal longer""" & chr(13,10)
    SAVEASSERT( Doc_WriteToDisk( wszFile, sV2, wszErr ) = DOCWRITE_OK, _
                "overwrite failed: " & wszErr )
    sBack = SaveSelfTest_ReadAll( wszFile )
    SAVEASSERT( sBack = sV2, "overwritten bytes differ" )
    SAVEASSERT( len(sBack) = len(sV2), "overwrite left a stale tail (old file was shorter/longer)" )
    SAVEASSERT( AfxFileExists( wszTmp ) = false, "temp file left behind after a successful overwrite" )

    ' ---- 3. an empty document is a legal save -------------------------------------------
    ' CFileStream.Write rejects a null pv with E_INVALIDARG and STRPTR of a zero-length FB
    ' string may BE null, so the zero-byte case has to skip the call rather than fail on it.
    SAVEASSERT( Doc_WriteToDisk( wszFile, "", wszErr ) = DOCWRITE_OK, _
                "empty save reported failure: " & wszErr )
    SAVEASSERT( len( SaveSelfTest_ReadAll( wszFile ) ) = 0, "empty save did not truncate the file" )

    ' ---- 4. THE ONE THAT MATTERS: a failed write must not destroy the original ----------
    ' Put known content down, then hold the target open with NO sharing at all -- the exact
    ' condition produced by the file being open in another program, or held by a scanner.
    Doc_WriteToDisk( wszFile, sV2, wszErr )
    dim as HANDLE hLock = CreateFileW( wszFile, GENERIC_READ or GENERIC_WRITE, _
                                       0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 )
    if hLock = INVALID_HANDLE_VALUE then
        ? "  NOTE: could not take an exclusive lock; the failure-path assertions did NOT run."
    else
        dim as string sV3 = "print ""this must never reach the disk"""
        dim as long rc = Doc_WriteToDisk( wszFile, sV3, wszErr )
        CloseHandle( hLock )

        SAVEASSERT( rc = DOCWRITE_FAILED, "a locked target was reported as a SUCCESSFUL save" )
        SAVEASSERT( len(wszErr) > 0, "failure returned no reason for the user" )
        sBack = SaveSelfTest_ReadAll( wszFile )
        SAVEASSERT( sBack = sV2, "THE ORIGINAL FILE WAS DAMAGED by a failed save" )
        SAVEASSERT( len(sBack) > 0, "THE ORIGINAL FILE WAS DESTROYED by a failed save" )
        SAVEASSERT( AfxFileExists( wszTmp ) = false, "a failed save left a stray .tiko-tmp behind" )
    end if

    ' ---- 5. an unwritable directory is a failure, not a crash --------------------------
    dim as DWSTRING wszBadPath = wszDir & "\no_such_subfolder\doc.bas"
    SAVEASSERT( Doc_WriteToDisk( wszBadPath, sV2, wszErr ) = DOCWRITE_FAILED, _
                "a path in a non-existent folder was reported as saved" )
    SAVEASSERT( len(wszErr) > 0, "missing-folder failure returned no reason" )

    ' ---- 6. an empty path is refused ----------------------------------------------------
    SAVEASSERT( Doc_WriteToDisk( "", sV2, wszErr ) = DOCWRITE_FAILED, _
                "an empty filename was reported as saved" )

    AfxDeleteFile( wszFile )

    sMsg = "TIKO_SAVE_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
