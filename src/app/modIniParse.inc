' ########################################################################################
' modIniParse.inc  -  see modIniParse.bi for the contract and why this is a classifier.
' ########################################################################################

function Ini_ClassifyLine( byval wszLine as DWSTRING, _
                           byref wszKey as DWSTRING, _
                           byref wszValue as DWSTRING ) as long
    wszKey = ""
    wszValue = ""

    if PsLen( wszLine ) = 0 then return INI_LINE_SKIP
    if PsLeft( wszLine, 1 ) = "'" then return INI_LINE_SKIP
    if PsLeft( wszLine, 1 ) = "[" then return INI_LINE_SKIP

    ' Prefix matches, not equality, exactly as all three loops did -- the markers have
    ' never carried a value and a trailing anything has always been tolerated.
    if PsLeft( wszLine, 11 ) = "NOTES-START" then return INI_LINE_NOTES_START
    if PsLeft( wszLine, 9 )  = "NOTES-END"   then return INI_LINE_NOTES_END

    dim as long i = PsInStr( wszLine, "=" )
    if i = 0 then return INI_LINE_OTHER

    ' The FIRST "=" splits. A value may contain more of them -- a compiler switch or a run
    ' argument routinely does -- and splitting on the last would truncate it.
    wszKey   = PsLeft( wszLine, i - 1 )
    wszValue = PsMid( wszLine, i + 1 )
    return INI_LINE_KEYVALUE
end function


' ########################################################################################
' Env-gated: TIKO_INIPARSE_SELFTEST=1
'
' Pure, and small. It exists because this decode had three copies that disagreed and no
' assertion anywhere -- the drift was found by writing this file, not by anything failing.
' ########################################################################################
sub Ini_RunSelfTest()

    if environ("TIKO_INIPARSE_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg
    dim as DWSTRING k, v

    #macro INIASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            sMsg = "  FAIL: " & msg
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_INIPARSE_SELFTEST ---"

    INIASSERT( Ini_ClassifyLine( "", k, v ) = INI_LINE_SKIP, "a blank line was not skipped" )
    INIASSERT( Ini_ClassifyLine( "' a comment", k, v ) = INI_LINE_SKIP, "a comment was not skipped" )
    INIASSERT( Ini_ClassifyLine( "[Section]", k, v ) = INI_LINE_SKIP, "a section header was not skipped" )

    ' The drift that prompted this file: a COMMENTED-OUT key must not classify as a key.
    INIASSERT( Ini_ClassifyLine( "'ProjectOther64=-l payload", k, v ) = INI_LINE_SKIP, _
               "a commented-out setting was read as a key -- this is the bug the copies had" )

    INIASSERT( Ini_ClassifyLine( "NOTES-START", k, v ) = INI_LINE_NOTES_START, "NOTES-START missed" )
    INIASSERT( Ini_ClassifyLine( "NOTES-END", k, v ) = INI_LINE_NOTES_END, "NOTES-END missed" )

    INIASSERT( Ini_ClassifyLine( "Key=Value", k, v ) = INI_LINE_KEYVALUE, "a key=value was not recognised" )
    INIASSERT( k = "Key", "key wrong: [" & k & "]" )
    INIASSERT( v = "Value", "value wrong: [" & v & "]" )

    ' Splitting on the FIRST "=" is load-bearing: compiler switches and run arguments
    ' contain them, and splitting on the last would truncate the value.
    Ini_ClassifyLine( "ProjectOther64=-Wl --defsym=foo=1", k, v )
    INIASSERT( k = "ProjectOther64", "multi-= line: key wrong: [" & k & "]" )
    INIASSERT( v = "-Wl --defsym=foo=1", "multi-= line: value truncated: [" & v & "]" )

    ' An empty value is legal and common -- it is what "no compiler switches" looks like.
    INIASSERT( Ini_ClassifyLine( "Key=", k, v ) = INI_LINE_KEYVALUE, "an empty value was not a key=value" )
    INIASSERT( PsLen(v) = 0, "an empty value came back as [" & v & "]" )

    INIASSERT( Ini_ClassifyLine( "no equals here", k, v ) = INI_LINE_OTHER, _
               "a line with no = was not reported as free text" )

    ' Every non-key kind must leave the out-parameters cleared, or a caller that reads them
    ' after a SKIP picks up the previous line's key.
    k = "stale" : v = "stale"
    Ini_ClassifyLine( "[Section]", k, v )
    INIASSERT( (PsLen(k) = 0) andalso (PsLen(v) = 0), _
               "a skipped line left stale key/value behind: [" & k & "]=[" & v & "]" )

    ' ---- EQUIVALENCE WITH THE CODE THIS REPLACED, over the REAL files ------------------
    ' The three loops were deduplicated, not rewritten, so the question that matters is not
    ' "does the classifier look right" but "does it decide the same thing the old preamble
    ' decided, on the actual data". Re-implemented inline below, straight from the original
    ' source, and run over this install's own settings.ini and .tiko -- thousands of real
    ' lines rather than the dozen hand-written cases above.
    scope
        dim as long nLines = 0, nDisagree = 0
        dim as DWSTRING wszFirstBad

        #macro CHECK_FILE( wszPath )
            scope
                dim pS as CTextStream
                if PsFileExists( wszPath ) andalso (pS.OpenUnicode( wszPath ) = S_OK) then
                    do until pS.EOS
                        dim as DWSTRING wl = pS.ReadLine
                        nLines += 1

                        ' --- the ORIGINAL preamble, verbatim ---
                        dim as long oKind
                        dim as DWSTRING oKey, oVal
                        if PsLen(wl) = 0 then
                            oKind = INI_LINE_SKIP
                        elseif PsLeft(wl, 1) = "'" then
                            oKind = INI_LINE_SKIP
                        elseif PsLeft(wl, 1) = "[" then
                            oKind = INI_LINE_SKIP
                        elseif PsLeft(wl, 11) = "NOTES-START" then
                            oKind = INI_LINE_NOTES_START
                        elseif PsLeft(wl, 9) = "NOTES-END" then
                            oKind = INI_LINE_NOTES_END
                        else
                            dim as long ii = PsInStr(wl, "=")
                            if ii = 0 then
                                oKind = INI_LINE_OTHER
                            else
                                oKind = INI_LINE_KEYVALUE
                                oKey  = PsLeft(wl, ii-1)
                                oVal  = PsMid(wl, ii+1)
                            end if
                        end if

                        ' --- the shared classifier ---
                        dim as DWSTRING nKey, nVal
                        dim as long nk = Ini_ClassifyLine( wl, nKey, nVal )

                        dim as boolean bSame = (nk = oKind)
                        if bSame andalso (nk = INI_LINE_KEYVALUE) then
                            bSame = (nKey = oKey) andalso (nVal = oVal)
                        end if
                        if bSame = false then
                            nDisagree += 1
                            if PsLen(wszFirstBad) = 0 then wszFirstBad = wl
                        end if
                    loop
                    pS.Close
                end if
            end scope
        #endmacro

        CHECK_FILE( gConfig.ConfigFilename )
        CHECK_FILE( gApp.ProjectFilename )

        INIASSERT( nLines > 0, _
            "precondition: no real file was read, so the equivalence check tested nothing" )
        INIASSERT( nDisagree = 0, _
            nDisagree & " of " & nLines & " real lines classify DIFFERENTLY from the code this " & _
            "replaced -- first: [" & wszFirstBad & "]" )
    end scope

    sMsg = "TIKO_INIPARSE_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
