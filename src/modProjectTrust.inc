' ########################################################################################
' modProjectTrust.inc  -  see modProjectTrust.bi for the contract and the reasoning.
' ########################################################################################

' ========================================================================================
' Does this project carry anything that can execute?
'
' Pure, and the whole of what decides to interrupt the user -- so it is asserted directly.
' Whitespace-only values count as empty: a .tiko written by tiko itself with no compiler
' switches set stores an empty value, and a stray space must not turn that into a prompt.
' ========================================================================================
function ProjectTrust_NeedsPrompt( byval wszOther32 as DWSTRING, _
                                   byval wszOther64 as DWSTRING, _
                                   byval wszCmdLine as DWSTRING ) as boolean
    if len( trim( wszOther32 ) ) > 0 then return true
    if len( trim( wszOther64 ) ) > 0 then return true
    if len( trim( wszCmdLine ) ) > 0 then return true
    return false
end function


' ========================================================================================
' The trusted list. Compared case-insensitively on the full path, because Windows paths are
' and the same project reached through a different case is the same project.
' ========================================================================================
function ProjectTrust_IsTrusted( byval wszFile as DWSTRING ) as boolean
    if len( wszFile ) = 0 then return false
    dim as DWSTRING wszWant = ucase( wszFile )
    for i as long = lbound( gConfig.TrustedProjects ) to ubound( gConfig.TrustedProjects )
        if len( gConfig.TrustedProjects(i) ) = 0 then continue for
        if ucase( gConfig.TrustedProjects(i) ) = wszWant then return true
    next
    return false
end function


' Newest first, capped. A trust list is not an audit log -- it exists so the prompt does not
' repeat, and an unbounded one would grow forever in settings.ini.
sub ProjectTrust_Add( byval wszFile as DWSTRING )
    if len( wszFile ) = 0 then exit sub
    if ProjectTrust_IsTrusted( wszFile ) then exit sub

    const MAX_TRUSTED = 50
    dim as long nOld = ubound( gConfig.TrustedProjects )
    dim as long nNew = nOld + 1
    if nNew > MAX_TRUSTED - 1 then nNew = MAX_TRUSTED - 1

    redim preserve gConfig.TrustedProjects( nNew )
    for i as long = nNew to 1 step -1
        gConfig.TrustedProjects(i) = gConfig.TrustedProjects(i - 1)
    next
    gConfig.TrustedProjects(0) = wszFile
end sub


' ========================================================================================
' Pull only ProjectOther32 / ProjectOther64 / ProjectCommandLine out of a .tiko.
'
' A separate, read-only pass over the file. It exists because the decision has to be made
' BEFORE ProjectLoadFromFile runs: by the time that has parsed the file it has already
' cleared the folder tree and opened documents, and "cancel" would mean unwinding all of it.
' ========================================================================================
function ProjectTrust_ScanFile( byval wszFile as DWSTRING, _
                                byref wszOther32 as DWSTRING, _
                                byref wszOther64 as DWSTRING, _
                                byref wszCmdLine as DWSTRING ) as boolean
    wszOther32 = "" : wszOther64 = "" : wszCmdLine = ""
    if AfxFileExists( wszFile ) = 0 then return false

    dim pStream as CTextStream   ' (utf16)
    if pStream.OpenUnicode( wszFile ) <> S_OK then return false

    dim as boolean bReadingNote
    do until pStream.EOS
        dim as DWSTRING wst = pStream.ReadLine
        if len(wst) = 0 then continue do

        ' The NOTES block is free text and can contain anything, including lines that look
        ' like keys. Skip it exactly as the real parser does.
        if left(wst, 11) = "NOTES-START" then bReadingNote = true  : continue do
        if left(wst, 9)  = "NOTES-END"   then bReadingNote = false : continue do
        if bReadingNote then continue do

        dim as long i = instr( wst, "=" )
        if i = 0 then continue do
        dim as DWSTRING wKey  = left( wst, i - 1 )
        dim as DWSTRING wData = mid( wst, i + 1 )

        select case wKey
            case "ProjectOther32":     wszOther32 = wData
            case "ProjectOther64":     wszOther64 = wData
            case "ProjectCommandLine": wszCmdLine = wData
        end select
    loop
    pStream.Close

    return true
end function


' ========================================================================================
' The decision. Silent for anything the user made or anything with nothing to execute.
' ========================================================================================
function ProjectTrust_Check( byval wszFile as DWSTRING ) as long

    if ProjectTrust_IsTrusted( wszFile ) then return PROJTRUST_OPEN

    dim as DWSTRING wszOther32, wszOther64, wszCmdLine
    if ProjectTrust_ScanFile( wszFile, wszOther32, wszOther64, wszCmdLine ) = false then
        ' Unreadable. ProjectLoadFromFile will fail on it too and say so; nothing to warn
        ' about here, and a second message would only confuse.
        return PROJTRUST_OPEN
    end if

    if ProjectTrust_NeedsPrompt( wszOther32, wszOther64, wszCmdLine ) = false then
        ' Nothing in it can run. Remember that, so adding switches later asks once.
        return PROJTRUST_OPEN
    end if

    ' Show what is actually being agreed to, not a generic warning. A user who cannot see
    ' the switches cannot judge them, and a warning that cannot be judged gets dismissed.
    dim as DWSTRING wszDetail
    if len( trim(wszOther32) ) then wszDetail &= vbcrlf & "  -  32-bit: " & trim(wszOther32)
    if len( trim(wszOther64) ) then wszDetail &= vbcrlf & "  -  64-bit: " & trim(wszOther64)
    if len( trim(wszCmdLine) ) then wszDetail &= vbcrlf & "  -  " & L(688, "Run arguments") & ": " & trim(wszCmdLine)

    dim as long nAnswer = TikoMsgBox( HWND_FRMMAIN, _
            L(687, "This project supplies its own compiler switches and run arguments. " & _
                   "They are used when you build or run it.") & vbcrlf & _
            wszDetail & vbcrlf & vbcrlf & _
            wszFile & vbcrlf & vbcrlf & _
            L(689, "Use these settings?  Yes = use them.  No = open the files but discard " & _
                   "them.  Cancel = do not open."), _
            L(690, "Open project"), _
            TMB_ICON_WARNING, TMB_YESNOCANCEL, 1 )   ' default = No, the safe middle

    select case nAnswer
        case IDYES
            ProjectTrust_Add( wszFile )
            return PROJTRUST_OPEN
        case IDNO
            ' Remembered too: the answer was "open this project without its build settings",
            ' and re-asking every time would make the safe choice the annoying one.
            ProjectTrust_Add( wszFile )
            return PROJTRUST_OPEN_SAFE
        case else
            return PROJTRUST_CANCEL
    end select
end function


' ########################################################################################
' Env-gated: TIKO_PROJTRUST_SELFTEST=1
'
' The prompt itself cannot be asserted -- it is modal. What IS asserted is everything that
' decides whether it appears, which is where a mistake would actually matter: a rule that
' prompts too often gets trained away, and one that prompts too rarely is not a rule.
' ########################################################################################
sub ProjectTrust_RunSelfTest()

    if environ("TIKO_PROJTRUST_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro PTASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            sMsg = "  FAIL: " & msg
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_PROJTRUST_SELFTEST ---"

    ' ---- what counts as executable content ---------------------------------------------
    PTASSERT( ProjectTrust_NeedsPrompt( "", "", "" ) = false, _
              "an empty project asked for confirmation" )
    PTASSERT( ProjectTrust_NeedsPrompt( "   ", "  ", " " ) = false, _
              "whitespace-only settings asked for confirmation -- this would fire on ordinary projects" )
    PTASSERT( ProjectTrust_NeedsPrompt( "-l evil", "", "" ) = true, _
              "32-bit compiler switches did not ask" )
    PTASSERT( ProjectTrust_NeedsPrompt( "", "-Wl --script=x", "" ) = true, _
              "64-bit compiler switches did not ask" )
    PTASSERT( ProjectTrust_NeedsPrompt( "", "", "--run-this" ) = true, _
              "run arguments did not ask" )

    ' ---- the trusted list ---------------------------------------------------------------
    scope
        dim as DWSTRING wszSaved( any )
        redim wszSaved( ubound( gConfig.TrustedProjects ) )
        for i as long = 0 to ubound( gConfig.TrustedProjects )
            wszSaved(i) = gConfig.TrustedProjects(i)
        next

        erase gConfig.TrustedProjects
        PTASSERT( ProjectTrust_IsTrusted( "C:\a\b.tiko" ) = false, _
                  "an unknown project was already trusted" )
        ProjectTrust_Add( "C:\a\b.tiko" )
        PTASSERT( ProjectTrust_IsTrusted( "C:\a\b.tiko" ) = true, "adding did not take" )
        PTASSERT( ProjectTrust_IsTrusted( "c:\A\B.TIKO" ) = true, _
                  "trust is case sensitive -- the same project via a different case would re-ask" )
        PTASSERT( ProjectTrust_IsTrusted( "C:\a\other.tiko" ) = false, _
                  "trusting one project trusted another" )
        PTASSERT( ProjectTrust_IsTrusted( "" ) = false, "an empty path was trusted" )

        ' Adding twice must not grow the list, or a project opened daily fills settings.ini.
        dim as long nBefore = ubound( gConfig.TrustedProjects )
        ProjectTrust_Add( "C:\a\b.tiko" )
        PTASSERT( ubound( gConfig.TrustedProjects ) = nBefore, _
                  "re-adding a trusted project grew the list" )

        ' The cap holds, and the newest entry survives.
        for i as long = 1 to 80
            ProjectTrust_Add( "C:\p\proj" & i & ".tiko" )
        next
        PTASSERT( ubound( gConfig.TrustedProjects ) < 50, _
                  "the trusted list is unbounded (" & (ubound(gConfig.TrustedProjects)+1) & " entries)" )
        PTASSERT( ProjectTrust_IsTrusted( "C:\p\proj80.tiko" ) = true, _
                  "the most recent entry was evicted -- the list is ordered the wrong way" )

        erase gConfig.TrustedProjects
        if ubound( wszSaved ) >= 0 then
            redim gConfig.TrustedProjects( ubound( wszSaved ) )
            for i as long = 0 to ubound( wszSaved )
                gConfig.TrustedProjects(i) = wszSaved(i)
            next
        end if
    end scope

    ' ---- the file scan ------------------------------------------------------------------
    scope
        dim as DWSTRING wszDir = environ("TEMP") & "\tiko_savetest"
        AfxCreateDirectory( wszDir )
        dim as DWSTRING wszProj = wszDir & "\trust.tiko"
        dim as DWSTRING wszErr

        dim as string sIni = _
            "ProjectOther64=-l payload" & chr(13,10) & _
            "NOTES-START" & chr(13,10) & _
            "ProjectCommandLine=this is inside the notes and must be ignored" & chr(13,10) & _
            "NOTES-END" & chr(13,10) & _
            "File=main.bas" & chr(13,10)
        dim as string sOut16 = chr(&HFF,&HFE)
        for iC as long = 1 to len(sIni)
            sOut16 &= chr( asc(sIni, iC) ) & chr(0)
        next
        Doc_WriteToDisk( wszProj, sOut16, wszErr )

        dim as DWSTRING o32, o64, cmd
        PTASSERT( ProjectTrust_ScanFile( wszProj, o32, o64, cmd ) = true, "the scan failed to read the file" )
        PTASSERT( trim(o64) = "-l payload", "64-bit switches not read: [" & o64 & "]" )
        PTASSERT( len(trim(o32)) = 0, "32-bit switches invented: [" & o32 & "]" )
        PTASSERT( len(trim(cmd)) = 0, _
                  "a key INSIDE the notes block was read as a run argument: [" & cmd & "]" )
        PTASSERT( ProjectTrust_NeedsPrompt( o32, o64, cmd ) = true, _
                  "a project with linker switches would open without asking" )

        dim as DWSTRING n32, n64, ncmd
        PTASSERT( ProjectTrust_ScanFile( wszDir & "\no_such.tiko", n32, n64, ncmd ) = false, _
                  "scanning a missing file reported success" )

        AfxDeleteFile( wszProj )
    end scope

    sMsg = "TIKO_PROJTRUST_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
