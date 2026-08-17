'' ========================================================================================
'' clsConfig -- the CONSTRUCTOR, and only the constructor.
''
'' ---- WHY THE CONSTRUCTOR IS DOWN HERE AND THE OTHER ~90 METHODS ARE NOT ----------------
''
'' app/clsConfig.bi:281 is `dim shared gConfig as clsConfig`. The instance lives with its
'' type deliberately, so that app-layer code can name the OBJECT and not merely the TYPE --
'' modMenuDefinitions.inc reads gConfig.MRU, .Tools, .MRUPROJECT and .ExplorerPositionRight.
''
'' The consequence is the reason this file exists: INCLUDING THE HEADER INSTANTIATES IT, so
'' anything that includes app/clsConfig.bi must be able to LINK a constructor. While the
'' constructor sat in src/clsConfig.inc, that meant every consumer of the app layer needed
'' the shell -- AfxGuid, HWND_FRMMAIN, and 40-odd frm* headers -- to link.
''
'' The rest of clsConfig stays in src/clsConfig.inc. It reads and writes real files, rebuilds
'' accelerator tables and talks to frmMain; none of that is app-layer work and none of it is
'' reached by merely declaring the object.
''
'' ---- HOW THIS WAS FOUND, WHICH MATTERS MORE THAN THE FIX -------------------------------
''
'' The four UI defaults below had ALREADY been moved once, out of the header's field
'' initialisers and into the constructor, precisely so the header would compile without the
'' shell. That worked. It also moved the dependency from the header into the BODY, and
'' _check_app_standalone.bat compiles each file with `fbc -c` and never links -- so the gate
'' could not see the difference and reported 7 clean / 0 errors throughout.
''
'' Phase 7c's shell binary is the first thing that ever linked the layer, and it failed on
'' one symbol: `CLSCONFIG::CLSCONFIG()`. Nine of the sixteen app headers linked; the seventh
'' and everything after it failed on that one name.
''
'' The gate now links as well as compiles. A compile-only gate cannot answer a linkage
'' question, and this layer's whole purpose is a linkage claim.
'' ========================================================================================

#include once "app/clsConfig.bi"
#include once "app/modAppConstants.bi"
#include once "app/modPaths.bi"

'' ---- and everything ProjectSaveToFile needs, added when it moved down here.
'' modTextFile.inc rather than a .bi: TextFile_WriteUtf16 is `private`, which in fbc is
'' module scope -- fine in the unity build, and this is what makes the file compile ALONE
'' under _check_app_standalone, which is the whole point of the move.
#include once "app/clsApp.bi"
#include once "app/modAppHost.bi"
#include once "app/modProjectFolders.bi"
#include once "app/modTextFile.inc"

'' ---- FORWARD SLASHES, per PsPath.bi's house rule -- 7c step 18 -------------------------
''
'' These read `exe_path & "settings\..."` until step 18, which produced a MIXED path:
'' PsExePath has always returned forward slashes with a trailing '/' on BOTH platforms
'' (PsFile.inc:662), so the result was "C:/dev/tiko/settings\settings.ini". Windows opens
'' that happily, which is why nothing ever said so -- and on Linux the second half is not a
'' path at all, it is one filename with backslashes in its name.
''
'' So the profile-API risk of this change is smaller than it looks: ConfigFilename already
'' reached WritePrivateProfileString with forward slashes in its first half and worked.
constructor clsConfig
    dim as DWSTRING exe_path = PsExePath
    ConfigFilename            = exe_path & "settings/settings.ini"
    FBKeywordsFilename        = exe_path & "settings/keywords/freebasic_keywords.txt"
    WinApiKeywordsFilename    = exe_path & "settings/keywords/winapi_keywords.txt"
    ExtraKeywordsFilename     = exe_path & "settings/keywords/extra_keywords.txt"
    FBKeywordsDefaultFilename = exe_path & "settings/keywords/freebasic_keywords_default.txt"
    FBCodetipsFilename        = exe_path & "settings/keywords/codetips.ini"
    UntitledProjectFilename   = exe_path & "settings/default" & APPEXTENSION

    ' ---- UI LAYOUT DEFAULTS -----------------------------------------------------
    ' The split is between the FIELD and its DEFAULT. A stored panel height and a
    ' stored splitter percentage are portable user preferences; how tall the output
    ' tabs are, and where a debugger splitter should start, are statements about the
    ' UI. That distinction still holds and nothing about it has changed.
    '
    ' What changed is WHERE the four values are declared. They were shell constants
    ' in modDeclares.bi and frmDebug.bi, which made this constructor unlinkable
    ' without the shell -- so they moved DOWN to app/modAppConstants.bi and the two
    ' shell headers now include that. The shell still uses them everywhere it did;
    ' it just no longer owns them.
    ShowOutputPanelHeight     = OUTPUT_TABS_HEIGHT * 5
    DebugSplitMain            = FRMDEBUG_DEFPCTMAIN
    DebugSplitLeft            = FRMDEBUG_DEFPCTLEFT
    DebugSplitRight           = FRMDEBUG_DEFPCTRIGHT
end constructor


'' ========================================================================================
'' ProjectSaveToFile -- THE LAST LINK DEBT, and it moved here without touching a tab control.
''
'' _check_app_standalone carried this symbol as its final entry for three steps, with a
'' comment that read: "blocked on something real: it calls gTTabCtl -- clsTopTabCtl, Win32 to
'' its bones -- at four sites. It cannot close until the tab control does, which is the
'' largest open item in the port."
''
'' It was the fourth stated blocker in that file not to survive being re-read. A project file
'' records WHICH DOCUMENTS ARE OPEN AND IN WHAT ORDER; that is a question about a list. The
'' five calls became four seam fields and one app-layer function, and clsTopTabCtl was not
'' touched at all.
''
'' WHY THIS FUNCTION AND NOT ProjectLoadFromFile. The loader is genuinely shell code --
'' GetCursor/SetCursor, LoadExplorerFiles, frmMain_*, frmOutput_*, frmUserTools_*,
'' frmBuildConfig_* -- and it is not a link debt, because nothing in the app layer calls it.
'' Moving it would mean seaming a dozen pieces of chrome to no end.
'' ========================================================================================



' ========================================================================================
' Save all options for the Project to a diskfile
' ========================================================================================
function clsConfig.ProjectSaveToFile() as boolean

    dim as DWSTRING cwzRelative, wszText
    dim as long nCount
    dim as long i

    dim pDoc as clsDocument ptr

    ' Paths are stored RELATIVE to the project file, but only for a project the user named
    ' and placed. The untitled workspace lives in settings\ under the exe, while its files
    ' live wherever the user works -- relativizing against it would turn every entry into a
    ' "..\..\" chain, and any file on another volume cannot be relativized at all. So the
    ' untitled workspace stores absolute paths, which is exactly what the session file it
    ' replaces did.
    '
    ' The {CURDRIVE} encoding below is NOT conditional and does not need to be:
    ' ProcessToCurdriveProject keys off ProjectFilename's drive, and the untitled project
    ' sits in the exe's own directory, so it already resolves to the App variant's answer.
    dim as boolean bRelativize = gApp.IsProjectNamed()

    ' One buffer, one write -- see SaveConfigFileBody for why.
    dim as DWSTRING wszOut

    wszOut &= "' PROJECT FILE" & vbcrlf
    wszOut &= "ProjectBuild="       & gApp.ProjectBuild & vbcrlf
    wszOut &= "ProjectOther32="     & gApp.ProjectOther32 & vbcrlf
    wszOut &= "ProjectOther64="     & gApp.ProjectOther64 & vbcrlf
    wszOut &= "ProjectCommandLine=" & gApp.ProjectCommandLine & vbcrlf

    ' Save all of the loaded tabs first. NOT the tab bar's own index -- a project counts
    ' positions in the WRITTEN list, and an untitled tab ahead of this one shifts them.
    ' See Tabs_SaveActiveIndex.
    wszOut &= "ActiveTab=" & Tabs_SaveActiveIndex() & vbcrlf

    nCount = gAppHost.TabCount()

    ' TAB ORDER, WHICH gApp.pDocList CANNOT GIVE. The loop below walks the tabs in the
    ' order the user arranged them; the second loop walks the document list for the rest.
    ' Zero from TabDocAt covers both an invalid index and a tab holding no document, which
    ' is what the Find in Project tab is.
    for i = 0 To nCount - 1
        pDoc = gAppHost.TabDocAt(i)
        if pDoc = 0 then continue for

        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then
            pDoc->SetDocInfo()
            wszText = pDoc->DiskFilename
            if bRelativize then
                cwzRelative = PsPathRelativeTo( wszText, PsPathDirWithSep(gApp.ProjectFilename) )
                if PsLen(cwzRelative) then
                    if PsPathIsRelative(cwzRelative) then wszText = cwzRelative
                end if
            end if
            wszText = ProcessToCurdriveProject(wszText)
            wszOut &= "File=" & wszText & vbcrlf
            wszOut &= "FileType=" & pDoc->ProjectFileType & vbcrlf
            wszOut &= "FileFolder=" & pDoc->docData.wszFolder & vbcrlf
            wszOut &= "TabIndex=" & "-1"     ' use -1 rather than true & vbcrlf
            wszOut &= "Bookmarks=" & pDoc->docData.wszBookmarks & vbcrlf
            wszOut &= "BreakPoints=" & pDoc->docData.wszBreakPoints & vbcrlf
            wszOut &= "FoldPoints=" & pDoc->docData.wszFoldPoints & vbcrlf
            wszOut &= "FirstLine=" & pDoc->docData.nFirstLine & vbcrlf
            wszOut &= "Position=" & pDoc->docData.nPosition & vbcrlf
            wszOut &= "FirstLine1=" & pDoc->docData.nFirstLine1 & vbcrlf
            wszOut &= "Position1=" & pDoc->docData.nPosition1 & vbcrlf
            wszOut &= "SplitMode=" & pDoc->docData.nSplitMode & vbcrlf
            wszOut &= "SplitPositionX=" & pDoc->docData.nSplitPositionX & vbcrlf
            wszOut &= "SplitPositionY=" & pDoc->docData.nSplitPositionY & vbcrlf
            wszOut &= "FocusEdit=" & pDoc->docData.nFocusEdit & vbcrlf
            wszOut &= "FileEnd=[-]" & vbcrlf
        end if
    next

    ' Save all other non-displayed documents for the project

    pDoc = gApp.pDocList
    do until pDoc = 0
        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then
            if gAppHost.TabIndexOfDoc(pDoc) = -1 then
                wszText = pDoc->DiskFilename
                if bRelativize then
                    cwzRelative = PsPathRelativeTo( wszText, PsPathDirWithSep(gApp.ProjectFilename) )
                    ' The len() test is NOT optional. PsPathRelativeTo returns "" when
                    ' PathRelativePathToW fails -- a file on another volume, most obviously --
                    ' and PsPathIsRelative("") is TRUE. Without the guard wszText becomes ""
                    ' and the document is written as "File=", i.e. SILENTLY DROPPED from the
                    ' project. The tabbed loop above has always had this guard; this one did not.
                    if PsLen(cwzRelative) then
                        if PsPathIsRelative(cwzRelative) then wszText = cwzRelative
                    end if
                end if
                wszText = ProcessToCurdriveProject(wszText)
                wszOut &= "File=" & wszText & vbcrlf
                wszOut &= "FileType=" & pDoc->ProjectFileType & vbcrlf
            wszOut &= "FileFolder=" & pDoc->docData.wszFolder & vbcrlf
                wszOut &= "TabIndex=" & "0" & vbcrlf
                wszOut &= "Bookmarks=" & pDoc->docData.wszBookmarks & vbcrlf
                wszOut &= "BreakPoints=" & pDoc->docData.wszBreakPoints & vbcrlf
                wszOut &= "FoldPoints=" & pDoc->docData.wszFoldPoints & vbcrlf
                wszOut &= "FirstLine=" & pDoc->docData.nFirstLine & vbcrlf
                wszOut &= "Position=" & pDoc->docData.nPosition & vbcrlf
                wszOut &= "FirstLine1=" & pDoc->docData.nFirstLine1 & vbcrlf
                wszOut &= "Position1=" & pDoc->docData.nPosition1 & vbcrlf
                wszOut &= "SplitMode=" & pDoc->docData.nSplitMode & vbcrlf
                wszOut &= "SplitPositionX=" & pDoc->docData.nSplitPositionX & vbcrlf
                wszOut &= "SplitPositionY=" & pDoc->docData.nSplitPositionY & vbcrlf
                wszOut &= "FocusEdit=" & pDoc->docData.nFocusEdit & vbcrlf
                wszOut &= "FileEnd=[-]" & vbcrlf
            end If
        end if
        pDoc = pDoc->pDocnext
    loop

    ' The user's subfolders, one record each.
    '
    ' Written INDEPENDENTLY of the files rather than being derived from their FileFolder
    ' lines, because an EMPTY folder has no file to be derived from -- and creating a folder
    ' before putting anything in it is the obvious order to work in. Without these records
    ' such a folder would silently disappear on the next open.
    '
    ' A distinct key from the per-file "FileFolder": record assembly is driven by FileEnd,
    ' so a bare "Folder=" line out here would be absorbed into whichever file record happened
    ' to be pending. Emitted after the file block for readability only -- the reader is
    ' order-independent.
    for i = lbound(gProjectFolders) to ubound(gProjectFolders)
        wszOut &= "ProjectFolder=" & gProjectFolders(i).catIndex & _ & vbcrlf
                          "|-|" & gProjectFolders(i).wszPath
    next

    ' Notes belong to the workspace, so they are always written.
    dim wszNotes as DWSTRING = wstr("NOTES-START") + vbcrlf + _
                            gApp.ProjectNotes + vbcrlf + _
                            wstr("NOTES-END") + vbcrlf
    wszOut &= "" & vbcrlf
    wszOut &= "[Notes]" & vbcrlf
    wszOut &= wszNotes & vbcrlf

    if TextFile_WriteUtf16( gApp.ProjectFilename, wszOut ) = false then return false

    ' The restore slot names the NAMED project to reopen, and only that. Writing the
    ' untitled workspace's own path here makes the slot permanently name settings\
    ' default.tiko, after which no project is ever restored again -- the next launch sees
    ' the untitled path, treats it as "nothing named to reopen", and comes up untitled
    ' forever. An untitled workspace clears the slot instead: there is no named project to
    ' go back to, which is exactly what an empty slot means.
    if gApp.IsProjectNamed() then
        this.wszLastActiveProject = gApp.ProjectFilename
    else
        this.wszLastActiveProject = ""
    end if

    function = true   ' successful save
end function
