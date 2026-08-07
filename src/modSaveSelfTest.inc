' ########################################################################################
' modSaveSelfTest.inc  -  see modSaveSelfTest.bi for the contract.
' ########################################################################################

' For code_Initialize/code_Cleanup: section 12 drives the real pair. Safe this early --
' modCompile.bi is types and declarations only and names no Ps* type.
#include once "modCompile.bi"
#include once "app/modBuildService.bi"

' Read a whole file back as raw bytes, through code that is NOT the writer under test.
private function SaveSelfTest_ReadAll( byval wszPath as DWSTRING ) as string
    dim as HANDLE h = CreateFileW( wszPath.Wz(), GENERIC_READ, _
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
            '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
            dim as DWSTRING wszMsg = msg
            sMsg = "  FAIL: " & wszMsg.Utf8
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_SAVE_SELFTEST ---"

    dim as DWSTRING wszDir  = environ("TEMP") & "\tiko_savetest"
    PsDirCreate( wszDir )
    dim as DWSTRING wszFile = wszDir & "\doc.bas"
    dim as DWSTRING wszTmp  = wszFile & ".tiko-tmp"
    dim as DWSTRING wszErr
    dim as string sBack

    ' ---- 1. happy path -----------------------------------------------------------------
    PsFileDelete( wszFile )
    dim as string sV1 = "print ""version one""" & chr(13,10)
    SAVEASSERT( Doc_WriteToDisk( wszFile, sV1, wszErr ) = DOCWRITE_OK, _
                "create of a new file failed: " & wszErr )
    sBack = SaveSelfTest_ReadAll( wszFile )
    SAVEASSERT( sBack = sV1, "new file bytes differ (got " & len(sBack) & " of " & len(sV1) & ")" )
    SAVEASSERT( PsFileExists( wszTmp ) = false, "temp file left behind after a successful create" )

    ' ---- 2. overwrite an existing file --------------------------------------------------
    dim as string sV2 = "print ""version two, which is a good deal longer""" & chr(13,10)
    SAVEASSERT( Doc_WriteToDisk( wszFile, sV2, wszErr ) = DOCWRITE_OK, _
                "overwrite failed: " & wszErr )
    sBack = SaveSelfTest_ReadAll( wszFile )
    SAVEASSERT( sBack = sV2, "overwritten bytes differ" )
    SAVEASSERT( len(sBack) = len(sV2), "overwrite left a stale tail (old file was shorter/longer)" )
    SAVEASSERT( PsFileExists( wszTmp ) = false, "temp file left behind after a successful overwrite" )

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
    dim as HANDLE hLock = CreateFileW( wszFile.Wz(), GENERIC_READ or GENERIC_WRITE,                                         0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 )
    if hLock = INVALID_HANDLE_VALUE then
        ? "  NOTE: could not take an exclusive lock; the failure-path assertions did NOT run."
    else
        dim as string sV3 = "print ""this must never reach the disk"""
        dim as long rc = Doc_WriteToDisk( wszFile, sV3, wszErr )
        CloseHandle( hLock )

        SAVEASSERT( rc = DOCWRITE_FAILED, "a locked target was reported as a SUCCESSFUL save" )
        SAVEASSERT( PsLen(wszErr) > 0, "failure returned no reason for the user" )
        sBack = SaveSelfTest_ReadAll( wszFile )
        SAVEASSERT( sBack = sV2, "THE ORIGINAL FILE WAS DAMAGED by a failed save" )
        SAVEASSERT( len(sBack) > 0, "THE ORIGINAL FILE WAS DESTROYED by a failed save" )
        SAVEASSERT( PsFileExists( wszTmp ) = false, "a failed save left a stray .tiko-tmp behind" )
    end if

    ' ---- 5. an unwritable directory is a failure, not a crash --------------------------
    dim as DWSTRING wszBadPath = wszDir & "\no_such_subfolder\doc.bas"
    SAVEASSERT( Doc_WriteToDisk( wszBadPath, sV2, wszErr ) = DOCWRITE_FAILED, _
                "a path in a non-existent folder was reported as saved" )
    SAVEASSERT( PsLen(wszErr) > 0, "missing-folder failure returned no reason" )

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
        PsFileDelete( wszLossy )
        Doc_WriteToDisk( wszLossy, "original bytes", wszErr )

        dim pD as clsDocument ptr = gApp.CreateEmptyDocument()
        if pD = 0 then
            ? "  NOTE: could not create a document; the call-site assertions did NOT run."
        else
            pD->DiskFilename  = *(wszLossy).Wz()
            pD->IsNewFlag     = false
            pD->FileEncoding  = FILE_ENCODING_ANSI
            pD->AssignTextBuffer()
            if DocView(pD, 0) then
                ' Two CJK characters: representable in UTF-8, not in any Western ANSI page.
                dim as string sCJK = chr(&hE4,&hBD,&hA0) & chr(&hE5,&hA5,&hBD)
                SciExec( DocView(pD, 0), SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
                SciExec( DocView(pD, 0), SCI_SETTEXT, 0, cast(LPARAM, strptr(sCJK)) )
                pD->UserModified = true

                ' GetTextRange with an empty or inverted range: space() of a non-positive
                ' length yields a zero-length FB string whose STRPTR is NULL (measured),
                ' so without its guard SCI_GETTEXTRANGE writes through a null pointer.
                ' No caller does this today -- the guard is defensive, but it is not
                ' cosmetic, and removing it crashes rather than misbehaving.
                SAVEASSERT( pD->GetTextRange( 5, 5 ) = "", "an empty range returned text" )
                SAVEASSERT( pD->GetTextRange( 10, 3 ) = "", "an inverted range returned text" )

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
        PsFileDelete( wszLossy )
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
        dim as HANDLE hCfgLock = CreateFileW( gConfig.ConfigFilename.Wz(), GENERIC_READ or GENERIC_WRITE,                                                0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 )
        if hCfgLock = INVALID_HANDLE_VALUE then
            ? "  NOTE: could not lock a config file; the Load half did NOT run."
        else
            gApp.PreventConfigLoad = false
            gConfig.LoadConfigFile( true )
            CloseHandle( hCfgLock )
            SAVEASSERT( gApp.PreventConfigLoad = false, _
                "a FAILED LoadConfigFile left PreventConfigLoad latched -- config-change detection is dead for the session" )
        end if
        PsFileDelete( gConfig.ConfigFilename )

        gConfig.ConfigFilename = wszSavedCfg
        gApp.PreventConfigLoad = bSavedFlag
    end scope

    ' ---- 10. a SECOND config load must re-read the indexed arrays ----------------------
    ' The assignments used to be nested inside "if i > ubound(...)", so an entry was stored
    ' only when its index GREW the array. Tools was never erased, so on every load after the
    ' first, ubound was already large and EVERY USERTOOL_ line was skipped -- user tools
    ' silently kept their pre-edit values while the file said otherwise. It also dropped a
    ' duplicate key and any non-ascending index in a hand-edited file.
    '
    ' Loading the SAME file twice and requiring the same result is the shape that catches
    ' it; a single load passes either way, which is why this went unnoticed.
    scope
        dim as DWSTRING wszSavedCfg = gConfig.ConfigFilename
        dim as DWSTRING wszCfg = wszDir & "\arrays.ini"

        dim as string sIni = _
            "MAXIMUM:1" & chr(13,10) & _
            "USERTOOL_00=ToolAlpha|-|a.exe|-||-||-||-|0|-|0|-|0|-|0|-|0|-|0|-|1|-|0" & chr(13,10) & _
            "USERTOOL_01=ToolBeta|-|b.exe|-||-||-||-|0|-|0|-|0|-|0|-|0|-|0|-|1|-|0" & chr(13,10)
        ' UTF-16LE with BOM: OpenUnicode is what reads this. Built byte by byte on purpose --
        ' casting the buffer to a zstring would stop at the first NUL, and in UTF-16 every
        ' second byte of ASCII text IS a NUL, so that yields one character.
        dim as string sOut16 = chr(&HFF,&HFE)
        for iC as long = 1 to len(sIni)
            sOut16 &= chr( asc(sIni, iC) ) & chr(0)
        next
        Doc_WriteToDisk( wszCfg, sOut16, wszErr )

        gConfig.ConfigFilename = wszCfg
        gConfig.LoadConfigFile( true )
        dim as long nFirst = ubound( gConfig.Tools )
        dim as DWSTRING wszFirst0
        if nFirst >= 0 then wszFirst0 = gConfig.Tools(0).wszDescription

        gConfig.LoadConfigFile( true )
        dim as long nSecond = ubound( gConfig.Tools )
        dim as DWSTRING wszSecond0
        if nSecond >= 0 then wszSecond0 = gConfig.Tools(0).wszDescription

        SAVEASSERT( nFirst = 1, "first load read " & (nFirst + 1) & " tools, expected 2" )
        SAVEASSERT( wszFirst0 = "ToolAlpha", "first load got [" & wszFirst0 & "]" )
        SAVEASSERT( nSecond = nFirst, _
            "SECOND load read " & (nSecond + 1) & " tools where the first read " & (nFirst + 1) & _
            " -- a hot reload does not re-read USERTOOL_ lines" )
        SAVEASSERT( wszSecond0 = "ToolAlpha", _
            "second load left a stale value: [" & wszSecond0 & "]" )

        gConfig.ConfigFilename = wszSavedCfg
        PsFileDelete( wszCfg )
        erase gConfig.Tools
        gConfig.LoadConfigFile( true )     ' restore the real configuration
    end scope

    ' ---- 11. a FAILED project open must not dismantle the current workspace ------------
    ' ProjectLoadFromFile used to overwrite ProjectFilename and ProjectName, blank
    ' ProjectNotes and run ProjectFolders_Clear() BEFORE opening the stream -- and its
    ' "return false" left all of that in place. A .tiko that exists but cannot be opened
    ' (held by another process, a share that dropped, permissions) destroyed the folder tree
    ' of the workspace the user still had open, pointed the app at a project it had failed
    ' to read, and left the WAIT cursor up because the restore at the end was never reached.
    scope
        dim as DWSTRING wszProj = wszDir & "\locked.tiko"
        Doc_WriteToDisk( wszProj, "ProjectBuild=x", wszErr )

        ' State the failed open must leave alone.
        dim as DWSTRING wszKeepFile = gApp.ProjectFilename
        dim as DWSTRING wszKeepName = gApp.ProjectName
        ' CATINDEX_NORMAL, not 0: ProjectFolders_CatAllowsFolders refuses CATINDEX_FILES, so
        ' adding there silently does nothing -- which made the count assertion below VACUOUS
        ' (0 = 0) and left it green under the A/B. The precondition is asserted so it cannot
        ' go quiet again.
        ProjectFolders_Add( CATINDEX_NORMAL, "SentinelFolder" )
        dim as long nFoldersBefore = ProjectFolders_Count()
        SAVEASSERT( nFoldersBefore > 0, _
            "precondition: the sentinel folder was not added, so the clear-check below tests nothing" )

        dim as HANDLE hPLock = CreateFileW( wszProj.Wz(), GENERIC_READ or GENERIC_WRITE,                                              0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 )
        if hPLock = INVALID_HANDLE_VALUE then
            ? "  NOTE: could not lock a .tiko; the project-open assertions did NOT run."
        else
            dim as boolean bOpened = gConfig.ProjectLoadFromFile( wszProj )
            CloseHandle( hPLock )

            SAVEASSERT( bOpened = false, "an unopenable .tiko reported a successful open" )
            SAVEASSERT( ProjectFolders_Count() = nFoldersBefore, _
                "a FAILED project open CLEARED the folder tree of the workspace still open (" & _
                ProjectFolders_Count() & " vs " & nFoldersBefore & ")" )
            SAVEASSERT( gApp.ProjectFilename = wszKeepFile, _
                "a FAILED project open overwrote ProjectFilename with [" & gApp.ProjectFilename & "]" )
            SAVEASSERT( gApp.ProjectName = wszKeepName, _
                "a FAILED project open overwrote ProjectName with [" & gApp.ProjectName & "]" )
        end if

        ProjectFolders_Clear()
        PsFileDelete( wszProj )
    end scope

    ' ---- 12. a compile must leave the process where it found it ------------------------
    ' code_Compile chdir's to the main file's folder, because fbc is handed RELATIVE paths
    ' (ObjFolderShort is ".\<ext>\") that the child resolves against the directory it
    ' inherits. Nothing undid that, so the editor's own working directory silently became
    ' whichever project was last built -- deciding where every later relative path resolved,
    ' and what a bare LoadLibrary would search.
    '
    ' Driven through the REAL code_Initialize/code_Cleanup pair rather than a copy of them.
    scope
        dim as boolean bSavedCompiling = BuildService_IsRunning()
        dim as string sBefore = curdir

        if bSavedCompiling then BuildService_End()
        dim pC as COMPILE_TYPE ptr = code_Initialize()
        if pC = 0 then
            ? "  NOTE: code_Initialize failed; the cwd assertions did NOT run."
        else
            chdir *wszDir.Wz()                ' stand in for the compile's own chdir
            SAVEASSERT( curdir <> sBefore, _
                "precondition: the directory did not actually change, so the restore below tests nothing" )
            code_Cleanup( pC )
            SAVEASSERT( curdir = sBefore, _
                "a compile left the process in [" & curdir & "] instead of [" & sBefore & "]" )
        end if

        ' code_Cleanup used to dereference pCompile->hCurSave BEFORE its null check, and
        ' code_Compile calls it with NULL on exactly the path it was written to clean up
        ' after -- "new COMPILE_TYPE returned null". Reaching the next line proves it does
        ' not crash on that input.
        code_Cleanup( 0 )
        SAVEASSERT( true, "code_Cleanup(NULL) crashed" )

        chdir sBefore
        if bSavedCompiling then BuildService_Begin()
    end scope

    ' ---- 13. the current directory is NOT on the DLL search path -----------------------
    ' By default LoadLibrary with a bare name searches the CURRENT DIRECTORY. tiko is
    ' exposed to that -- it loads Lexilla/Scintilla by name, AfxWebView2 resolves
    ' WebView2Loader.dll by name at runtime, and code_Compile chdir's the whole process into
    ' whichever project is being built. WinMain now calls SetDefaultDllDirectories with
    ' LOAD_LIBRARY_SEARCH_DEFAULT_DIRS.
    '
    ' Asserted by actually trying it: plant a real, loadable DLL in a directory, make that
    ' the current directory, and ask for it BY BARE NAME. It must not be found. Copying an
    ' existing DLL rather than fabricating one matters -- a malformed file would fail to
    ' load for the wrong reason and the assertion would pass while proving nothing.
    scope
        dim as DWSTRING wszSrcDll   = PsExePath() & "Lexilla64.dll"
        dim as DWSTRING wszPlanted  = wszDir & "\zz_hijack_probe.dll"

        if PsFileExists( wszSrcDll ) = false then
            ? "  NOTE: no Lexilla64.dll to copy; the DLL-search assertion did NOT run."
        else
            PsFileDelete( wszPlanted )
            CopyFileW( wszSrcDll.Wz(), wszPlanted.Wz(), FALSE )
            SAVEASSERT( PsFileExists( wszPlanted ), _
                "precondition: the probe DLL was not planted, so the search test proves nothing" )

            dim as string sCwdSave = curdir
            chdir *wszDir.Wz()
            dim as HMODULE hProbe = LoadLibraryW( "zz_hijack_probe.dll" )
            chdir sCwdSave

            SAVEASSERT( hProbe = 0, _
                "a DLL was loaded BY BARE NAME out of the current directory -- the search path is not hardened" )
            if hProbe then FreeLibrary( hProbe )
            PsFileDelete( wszPlanted )
        end if

        ' ...AND THE APPLICATION DIRECTORY STILL IS ON IT. This is the half the hardening
        ' could plausibly break: AfxWebView2 resolves WebView2Loader.dll by PLAIN NAME at
        ' runtime (there is no import lib), so if LOAD_LIBRARY_SEARCH_DEFAULT_DIRS had
        ' excluded the app directory the Help Center would have stopped working with no
        ' build error and no failing assertion anywhere. Load it the same way it does.
        if PsFileExists( PsExePath() & "WebView2Loader.dll" ) = false then
            ? "  NOTE: no WebView2Loader.dll beside the exe; that half did NOT run."
        else
            dim as HMODULE hWv = LoadLibraryW( "WebView2Loader.dll" )
            SAVEASSERT( hWv <> 0, _
                "WebView2Loader.dll no longer resolves by bare name -- DLL hardening broke the Help Center" )
            if hWv then FreeLibrary( hWv )
        end if
    end scope

    ' ---- 14. the legacy [Notes] migration is strictly ONE-WAY -------------------------
    ' gApp.NonProjectNotes is a migration-only field whose comment said it was removable
    ' "once no settings.ini in the wild still carries the section" -- a condition nobody can
    ' evaluate, which is how such fields become permanent. The condition that IS checkable
    ' is this one: the writer must never emit the block again. If that holds, any file saved
    ' once by a current build has already lost it, and the field is dead.
    scope
        dim as DWSTRING wszSavedCfg = gConfig.ConfigFilename
        dim as DWSTRING wszOutCfg   = wszDir & "\writeprobe.ini"
        PsFileDelete( wszOutCfg )

        gConfig.ConfigFilename = wszOutCfg
        gConfig.SaveConfigFile()
        gConfig.ConfigFilename = wszSavedCfg

        dim as string sWritten = SaveSelfTest_ReadAll( wszOutCfg )
        SAVEASSERT( len(sWritten) > 0, _
            "precondition: SaveConfigFile wrote nothing, so the scan below tests nothing" )
        ' UTF-16LE on disk; strip the NUL bytes rather than decode, which is enough to
        ' search for an ASCII marker.
        dim as string sFlat
        for iB as long = 1 to len(sWritten)
            if asc(sWritten, iB) <> 0 then sFlat &= chr( asc(sWritten, iB) )
        next
        SAVEASSERT( instr( sFlat, "[Notes]" ) = 0, _
            "SaveConfigFile still writes a [Notes] section -- the NonProjectNotes migration " & _
            "is NOT one-way and that field can never be removed" )
        SAVEASSERT( instr( sFlat, "NOTES-START" ) = 0, _
            "SaveConfigFile still writes a NOTES-START block into settings.ini" )

        PsFileDelete( wszOutCfg )
    end scope

    PsFileDelete( wszFile )

    sMsg = "TIKO_SAVE_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
