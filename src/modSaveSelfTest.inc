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

    ' ---- 7. lossy encoding is DETECTED and the flag is CONSUMED ------------------------
    ' Doc_EncodeForDisk has always reported this correctly and the encoding suite has always
    ' asserted so. The defect was never in the function -- it was that all three save sites
    ' declared bLossy and never read it, which is exactly the kind of gap a suite that tests
    ' functions rather than call sites cannot see.
    scope
        dim as boolean bLossy
        ' CJK cannot be represented in any Western ANSI codepage.
        dim as wstring * 32 wCJK = wchr(&h4F60) & wchr(&h597D)
        dim as long cb = WideCharToMultiByte( CP_UTF8, 0, @wCJK, len(wCJK), NULL, 0, NULL, NULL )
        dim as string sUtf8 = string( cb, 0 )
        WideCharToMultiByte( CP_UTF8, 0, @wCJK, len(wCJK), strptr(sUtf8), cb, NULL, NULL )

        Doc_EncodeForDisk( sUtf8, true, FILE_ENCODING_ANSI, bLossy )
        SAVEASSERT( bLossy = true, "CJK -> ANSI was not reported lossy" )
        Doc_EncodeForDisk( sUtf8, true, FILE_ENCODING_UTF8, bLossy )
        SAVEASSERT( bLossy = false, "CJK -> UTF-8 was wrongly reported lossy" )

        ' The suppression flag is what makes the prompt a warning rather than a nag, and
        ' clearing it on a deliberate encoding change is what stops a stale "yes" applying
        ' to a target the user never agreed to.
        dim as clsDocument ptr pD = new clsDocument
        if pD then
            pD->bLossySaveAccepted = true
            SAVEASSERT( Doc_ConfirmLossySave( pD, "x.bas", FILE_ENCODING_ANSI ) = true, _
                        "an already-accepted document re-prompted (this would nag on every Ctrl+S)" )
            delete pD
        end if
    end scope

    ' ---- 8. THE CALL SITE consumes bLossy, not just the function -----------------------
    ' This is the assertion that would have caught the original defect. It drives the real
    ' clsDocument.SaveFile over a document whose text cannot survive its own encoding, and
    ' checks that a "Cancel" answer actually stops the write and leaves the document dirty.
    ' Everything above this point passed for years while all three save sites ignored bLossy.
    scope
        dim as DWSTRING wszLossy = wszDir & "\lossy.bas"
        AfxDeleteFile( wszLossy )
        Doc_WriteToDisk( wszLossy, "original bytes", wszErr )

        dim pD as clsDocument ptr = gApp.CreateEmptyDocument()
        if pD = 0 then
            ? "  NOTE: could not create a document; the call-site assertions did NOT run."
        else
            pD->DiskFilename  = wszLossy
            pD->IsNewFlag     = false
            pD->FileEncoding  = FILE_ENCODING_ANSI
            pD->AssignTextBuffer()
            if pD->hWindow(0) then
                ' Two CJK characters: representable in UTF-8, not in any Western ANSI page.
                dim as string sCJK = chr(&hE4,&hBD,&hA0) & chr(&hE5,&hA5,&hBD)
                SciExec( pD->hWindow(0), SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
                SciExec( pD->hWindow(0), SCI_SETTEXT, 0, cast(LPARAM, strptr(sCJK)) )
                pD->UserModified = true

                gLossySaveTestAnswer = -1                  ' force Cancel
                dim as boolean bRc = pD->SaveFile()
                gLossySaveTestAnswer = 0

                SAVEASSERT( bRc = true, _
                    "SaveFile ignored a REFUSED lossy save and reported success" )
                SAVEASSERT( SaveSelfTest_ReadAll( wszLossy ) = "original bytes", _
                    "SaveFile WROTE the file after the lossy save was refused" )
                SAVEASSERT( pD->UserModified = true, _
                    "SaveFile cleared the dirty flag after a refused save -- the close prompt would not fire" )
            else
                ? "  NOTE: no Scintilla window; the call-site assertions did NOT run."
            end if
            gApp.RemoveDocument( pD )
        end if
        AfxDeleteFile( wszLossy )
    end scope

    ' ---- 9. a failed config load/save must not strand PreventConfigLoad ----------------
    ' gApp.PreventConfigLoad is set around both, and its only reader is
    ' ReloadConfigFileTest, which short-circuits on it. Both bodies had an early "return
    ' true" between the set and the clear -- an unreadable or uncreatable settings.ini --
    ' so ONE transient failure permanently killed external config-change detection for the
    ' session, silently and irrecoverably.
    '
    ' Driven by pointing ConfigFilename at a path that cannot be opened OR created, which
    ' takes both bodies down their early-return path for real rather than by inspection.
    scope
        dim as DWSTRING wszSavedCfg = gConfig.ConfigFilename
        dim as boolean bSavedFlag   = gApp.PreventConfigLoad

        ' A directory that does not exist: OpenUnicode and Create both fail.
        gConfig.ConfigFilename = wszDir & "\no_such_dir\settings.ini"

        gApp.PreventConfigLoad = false
        gConfig.SaveConfigFile()
        SAVEASSERT( gApp.PreventConfigLoad = false, _
            "a FAILED SaveConfigFile left PreventConfigLoad latched -- config-change detection is dead for the session" )

        ' LoadConfigFile only reaches OpenUnicode when the file appears to exist, so the
        ' missing-directory case above exercises Save; for Load, create the file and then
        ' hold it open with no sharing at all.
        gConfig.ConfigFilename = wszDir & "\lockedcfg.ini"
        Doc_WriteToDisk( gConfig.ConfigFilename, "MAXIMUM:1", wszErr )
        dim as HANDLE hCfgLock = CreateFileW( gConfig.ConfigFilename, GENERIC_READ or GENERIC_WRITE, _
                                              0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 )
        if hCfgLock = INVALID_HANDLE_VALUE then
            ? "  NOTE: could not lock a config file; the Load half did NOT run."
        else
            gApp.PreventConfigLoad = false
            gConfig.LoadConfigFile( true )
            CloseHandle( hCfgLock )
            SAVEASSERT( gApp.PreventConfigLoad = false, _
                "a FAILED LoadConfigFile left PreventConfigLoad latched -- config-change detection is dead for the session" )
        end if
        AfxDeleteFile( gConfig.ConfigFilename )

        gConfig.ConfigFilename = wszSavedCfg
        gApp.PreventConfigLoad = bSavedFlag
    end scope

    AfxDeleteFile( wszFile )

    sMsg = "TIKO_SAVE_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
