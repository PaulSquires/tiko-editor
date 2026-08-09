' ========================================================================================
' frmHelpCenter -- see frmHelpCenter.bi for what this is and why there is no window.
' ========================================================================================

#pragma once


' ========================================================================================
' The folder a help site lives in, with a trailing backslash.
'
' BOTH SITES SIT UNDER settings\help, which is what makes that one folder the whole of
' tiko's documentation -- the FreeBASIC .chm the generator ingests is already there. The
' generated tree is gitignored and staged by _copy_helpcenter.bat; the tikohelp tree is
' hand written and tracked, so the two are not interchangeable however alike they look.
' ========================================================================================
function HelpCenter_RootFolder( byval nSite as long ) as DWSTRING
    if nSite = HELPCENTER_SITE_TIKO then
        function = PsExePath & "settings\help\tikohelp\"
    else
        function = PsExePath & "settings\help\helpcenter\"
    end if
end function


' The name of a site. Once the window's caption; now only the caption of that site's
' "files not found" box, so the two cannot name different things.
function HelpCenter_SiteTitle( byval nSite as long ) as DWSTRING
    if nSite = HELPCENTER_SITE_TIKO then
        function = L(519,"Tiko Editor Help")
    else
        function = L(86,"Help Center")
    end if
end function


' ========================================================================================
' A Windows path as a file:// URL.
'
' Both conversions are needed rather than tidy: backslashes are not path separators in a
' URL, and the four encoded characters change how the URL PARSES rather than merely
' looking untidy -- a "#" truncates the path into a fragment and a " " ends the URL
' outright, both silently. "%" must be encoded FIRST or it re-encodes the escapes the
' later passes introduce.
'
' ENCODING "?" IS WHAT MAKES THE QUERY SAFE TO APPEND. HelpCenter_SearchUrl builds
' "<path>?q=<term>", so a "?" left raw in an install path would be read as the start of
' the query and the real one would land inside a parameter value.
'
' Split out from HelpCenter_IndexUrl so it can be asserted against paths this machine does
' not have. IndexUrl reads the live exe path, which here has nothing to encode -- an
' assertion driving only that would pass under an encoder that does nothing at all, and
' the failure would then only ever appear on someone else's machine.
' ========================================================================================
function HelpCenter_PathToFileUrl( byval wszPath as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWork = wszPath
    wszWork = PsStrReplace( wszWork, "\", "/" )
    wszWork = PsStrReplace( wszWork, "%", "%25" )    ' first, or it re-encodes the rest
    wszWork = PsStrReplace( wszWork, " ", "%20" )
    wszWork = PsStrReplace( wszWork, "#", "%23" )
    wszWork = PsStrReplace( wszWork, "?", "%3F" )
    function = "file:///" & wszWork
end function


' ========================================================================================
' Escape arbitrary text so it can be a QUERY STRING VALUE.
'
' This replaces HelpCenter_JsQuote, which escaped the same text for a JavaScript string
' literal because the term used to be injected with ExecuteScript. The input is the same
' -- a Scintilla selection, text the user chose and we did not write -- so every case here
' is reachable; only the destination changed.
'
' THE PERCENT MUST GO FIRST, for the same reason it does in PathToFileUrl: encode it last
' and it re-encodes the escapes the other passes introduced.
'
' "+" IS ENCODED, and that is the one that is easy to miss. A query reader decodes "+" as
' a SPACE, so a search for "a+b" left raw arrives at the site as "a b" -- a wrong answer
' rather than a broken one, which is the kind that survives review.
'
' "&" and "#" END THE PARAMETER: an unencoded "&" starts the next one and a "#" starts the
' fragment, so either would silently truncate the term.
' ========================================================================================
function HelpCenter_QueryEncode( byval wszText as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWork = wszText
    wszWork = PsStrReplace( wszWork, "%", "%25" )    ' first, always
    wszWork = PsStrReplace( wszWork, "+", "%2B" )
    wszWork = PsStrReplace( wszWork, "&", "%26" )
    wszWork = PsStrReplace( wszWork, "#", "%23" )
    wszWork = PsStrReplace( wszWork, "?", "%3F" )
    wszWork = PsStrReplace( wszWork, " ", "%20" )
    wszWork = PsStrReplace( wszWork, """", "%22" )
    function = wszWork
end function


function HelpCenter_IndexUrl( byval nSite as long ) as DWSTRING
    function = HelpCenter_PathToFileUrl( HelpCenter_RootFolder( nSite ) & "index.html" )
end function


' ========================================================================================
' The landing page, with a search term if there is one.
'
' THE QUERY FORM, not the fragment. The site's app.js accepts both -- "?q=" and "#q=" --
' because a fragment survives hosts that drop a query string from a file:// URL, and the
' embedded pane this module used to carry was one of them. A real browser keeps both, and
' the query form is the conventional one, so that is what is sent.
' ========================================================================================
function HelpCenter_SearchUrl( byval wszQuery as DWSTRING, byval nSite as long ) as DWSTRING
    dim as DWSTRING wszUrl = HelpCenter_IndexUrl( nSite )
    if PsLen( wszQuery ) = 0 then
        function = wszUrl
    else
        function = wszUrl & "?q=" & HelpCenter_QueryEncode( wszQuery )
    end if
end function


' ========================================================================================
' Reduce a Scintilla selection to something a search box can use: leading/trailing
' whitespace off, FIRST LINE ONLY, and capped at HELPCENTER_MAXQUERY characters.
'
' The first-line rule is the one that matters. A multi-line selection is a block of code,
' not an identifier, and pushing it into a one-line input gives the site's ranker nothing
' to work with while looking, on screen, as though the search simply failed.
' ========================================================================================
function HelpCenter_NormalizeQuery( byval wszText as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWork = wszText
    if PsLen( wszWork ) = 0 then
        function = ""
        exit function
    end if

    ' First line only. Handle CR, LF and CRLF by cutting at whichever arrives first.
    dim as long nCR = PsInStr( wszWork, wchr(13) )
    dim as long nLF = PsInStr( wszWork, wchr(10) )
    dim as long nCut = 0
    if (nCR > 0) andalso (nLF > 0) then
        nCut = iif( nCR < nLF, nCR, nLF )
    elseif nCR > 0 then
        nCut = nCR
    elseif nLF > 0 then
        nCut = nLF
    end if
    if nCut > 0 then wszWork = PsLeft( wszWork, nCut - 1 )

    ' A tab inside the remaining line is whitespace to a search box, not a separator.
    wszWork = PsStrReplace( wszWork, wchr(9), " " )
    wszWork = PsTrim( wszWork )

    if PsLen( wszWork ) > HELPCENTER_MAXQUERY then
        wszWork = PsLeft( wszWork, HELPCENTER_MAXQUERY )
    end if

    function = wszWork
end function


' ========================================================================================
' Is a word-under-the-caret worth searching for?
'
' Only the FALLBACK is filtered, never a selection: selecting "=>" is an explicit request
' and we honour it, but the caret merely sitting somewhere is not. GetWord walks a run of
' non-word characters when the caret is on punctuation, so F1 on the "'" that opens a
' comment line -- or on an operator, or a lone digit -- would otherwise fire a search for
' that character and show "No matches", which reads as the feature being broken.
'
' The test is "contains at least one letter", not "is a valid identifier": FreeBASIC names
' can carry digits and underscores, and "#define" or "__FB_WIN32__" are legitimate things
' to look up. A pure digit run is not.
' ========================================================================================
function HelpCenter_IsSearchableWord( byval wszWord as DWSTRING ) as boolean
    dim as long nLen = PsLen( wszWord )
    if nLen = 0 then return false
    for i as long = 1 to nLen
        dim as ulong c = wszWord[i - 1]
        dim as boolean bUpper = (c >= 65) andalso (c <= 90)
        dim as boolean bLower = (c >= 97) andalso (c <= 122)
        ' Anything above ASCII is treated as a letter: accented and CJK identifiers are
        ' letters to their author, and the alternative is refusing to look them up.
        dim as boolean bWide  = (c > 127)
        if bUpper orelse bLower orelse bWide then return true
    next
    function = false
end function


' ========================================================================================
' A theme-matched message box for the one failure path left.
' ========================================================================================
private function frmHelpCenter_ThemedMsgBox( byval wszText as DWSTRING, _
                                             byval wszCaption as DWSTRING ) as long
    ' Owned by frmMain. GUIFONT_10 is kept: this box carries long sentences.
    return TikoMsgBox( HWND_FRMMAIN, wszText, wszCaption, _
                       TMB_ICON_WARNING, TMB_OK, 0, ghFont(GUIFONT_10) )
end function


' ========================================================================================
' Open the Help Center in the user's browser, optionally running a search.
'
' wszSearch is already normalized by the caller (frmMainOnCommand), so it is a single line
' and within the length cap by the time it arrives here.
'
' ONE FAILURE PATH IS LEFT, and it is the one that was always worth reporting: the content
' is not there. The two WebView2 failures -- no runtime, no loader DLL -- are gone with the
' dependency, which is most of what this change buys.
'
' SHELLEXECUTE, NOT A NAMED BROWSER. "Open this URL" is the shell's question to answer, and
' it answers it with whatever the user chose. On the PsPlatform side the same call is
' g_plat's OpenUrl over SDL_OpenURL, which is the identical idea on both platforms -- see
' docs/port/webview2-decision.md.
' ========================================================================================
function frmHelpCenter_Show( byval wszSearch as DWSTRING, byval nSite as long ) as LRESULT

    ' --- the content has to be there ---------------------------------------------------
    ' Checked before launching rather than after: a browser handed a file:// URL that does
    ' not exist shows ITS OWN error page, which tells the user their browser is broken
    ' rather than that tiko's help files are missing.
    dim as DWSTRING wszIndex = HelpCenter_RootFolder( nSite ) & "index.html"
    if PsFileExists( wszIndex ) = 0 then
        ' The two sites report separately -- one can be staged and the other missing, and
        ' "Help Center files not found" while looking for tikohelp names the wrong tree.
        dim as DWSTRING wszMissing = L(90,"Help Center files not found.")
        if nSite = HELPCENTER_SITE_TIKO then
            wszMissing = L(520,"Tiko Editor Help files not found.")
        end if
        frmHelpCenter_ThemedMsgBox( wszMissing & !"\r\n\r\n" & wszIndex, _
                                    HelpCenter_SiteTitle( nSite ) )
        exit function
    end if

    dim as DWSTRING wszUrl = HelpCenter_SearchUrl( wszSearch, nSite )

    ' SW_SHOWNORMAL, and the return is the documented > 32 test. ShellExecute's failure
    ' codes are all <= 32 and are not HRESULTs; comparing against S_OK would read every
    ' failure as success.
    dim as integer nRc = cast(integer, ShellExecuteW( HWND_FRMMAIN, "open", wszUrl.Wz(), _
                                                      null, null, SW_SHOWNORMAL ))
    if nRc <= 32 then
        ' No handler for file:// at all, which on a normal desktop means no browser is
        ' registered. Reported rather than swallowed: the alternative is F1 doing nothing.
        '
        ' Ids 88 and 89 were the WebView2 runtime warning. They are REPURPOSED rather than
        ' retired -- same module, same failure path, and the count of live ids does not
        ' move. english.lang's three blank slots stay free for something that needs them.
        frmHelpCenter_ThemedMsgBox( L(89,"The Help Center could not be opened in your web browser.") & _
                                    !"\r\n\r\n" & wszUrl, _
                                    L(88,"Cannot Open Browser") )
        exit function
    end if

    if environ("TIKO_HELPCENTER_TRACE") = "1" then
        print "  helpcenter: " & wszUrl.Utf8
    end if

    function = 0
end function


' ========================================================================================
' Env-gated self-test: TIKO_HELPCENTER_SELFTEST=1
'
' READ THIS BEFORE TRUSTING THE COUNT. This suite is PURE, and it now covers a much larger
' share of the module than it used to -- because the module is now almost entirely pure.
' What it still cannot reach is the ShellExecute itself: whether a browser actually opened,
' and whether the site honoured ?q=. That is the interactive pass, and TIKO_HELPCENTER_TRACE=1
' prints the exact URL handed to the shell so it can be checked by eye or pasted.
'
' What it does cover is the URL arithmetic and the two encoders, which is where a silent
' wrong answer is possible -- a mis-encoded query does not fail, it searches for the wrong
' thing.
' ========================================================================================
dim shared as long gHelpCenterTestPass, gHelpCenterTestFail

private sub HelpCenter_Check( byval sName as string, byval bCondition as boolean )
    if bCondition then
        gHelpCenterTestPass += 1
    else
        gHelpCenterTestFail += 1
        dim as string sMsg = "  FAIL: " & sName
        print sMsg
    end if
end sub

sub frmHelpCenter_RunSelfTest()
    if environ("TIKO_HELPCENTER_SELFTEST") <> "1" then exit sub

    gHelpCenterTestPass = 0
    gHelpCenterTestFail = 0
    print "--- TIKO_HELPCENTER_SELFTEST ---"

    ' --- HelpCenter_QueryEncode --------------------------------------------------------
    ' The four that change what the URL MEANS rather than how it looks.
    HelpCenter_Check( "query: a space becomes %20", _
        HelpCenter_QueryEncode( "Afx Window" ) = "Afx%20Window" )
    ' "+" decodes as a SPACE at the far end, so leaving it raw is a WRONG search, not a
    ' broken one.
    HelpCenter_Check( "query: plus is encoded, not left to mean space", _
        HelpCenter_QueryEncode( "a+b" ) = "a%2Bb" )
    ' "&" would start the next parameter and "#" the fragment -- either truncates.
    HelpCenter_Check( "query: ampersand is encoded", _
        HelpCenter_QueryEncode( "this&that" ) = "this%26that" )
    HelpCenter_Check( "query: hash is encoded", _
        HelpCenter_QueryEncode( "#define" ) = "%23define" )
    HelpCenter_Check( "query: question mark is encoded", _
        HelpCenter_QueryEncode( "what?" ) = "what%3F" )
    ' "%" FIRST, or it re-encodes the escapes the later passes add. The giveaway of a
    ' wrong order is a doubled "25".
    HelpCenter_Check( "query: percent is encoded first, not last", _
        HelpCenter_QueryEncode( "100% sure" ) = "100%25%20sure" )
    HelpCenter_Check( "query: nothing to encode is left alone", _
        HelpCenter_QueryEncode( "CreateWindowEx" ) = "CreateWindowEx" )
    HelpCenter_Check( "query: empty stays empty", _
        HelpCenter_QueryEncode( "" ) = "" )

    ' --- HelpCenter_SearchUrl ----------------------------------------------------------
    HelpCenter_Check( "search: no term means no query string at all", _
        HelpCenter_SearchUrl( "", HELPCENTER_SITE_DOCS ) = HelpCenter_IndexUrl( HELPCENTER_SITE_DOCS ) )
    HelpCenter_Check( "search: a term is appended as ?q=", _
        PsInStr( HelpCenter_SearchUrl( "CreateWindowEx", HELPCENTER_SITE_DOCS ), _
                 "index.html?q=CreateWindowEx" ) > 0 )
    ' EXACTLY ONE "?" in the finished URL. The path encoder turns any "?" in the install
    ' path into %3F precisely so this holds; if it ever stopped, the term would land
    ' inside a parameter value and the search would silently do nothing.
    Scope
        dim as DWSTRING wszU = HelpCenter_SearchUrl( "a?b", HELPCENTER_SITE_DOCS )
        dim as long nQ = 0
        for i as long = 1 to PsLen( wszU )
            if wszU[i - 1] = 63 then nQ += 1
        next
        HelpCenter_Check( "search: exactly one unencoded question mark", nQ = 1 )
    end Scope

    ' --- HelpCenter_NormalizeQuery -----------------------------------------------------
    HelpCenter_Check( "query: first line only", _
        HelpCenter_NormalizeQuery( "one" & wchr(13) & wchr(10) & "two" ) = "one" )
    HelpCenter_Check( "query: trimmed", _
        HelpCenter_NormalizeQuery( "  spaced  " ) = "spaced" )
    HelpCenter_Check( "query: a tab is whitespace", _
        HelpCenter_NormalizeQuery( "a" & wchr(9) & "b" ) = "a b" )
    HelpCenter_Check( "query: capped at the maximum", _
        PsLen( HelpCenter_NormalizeQuery( string( HELPCENTER_MAXQUERY + 50, "x" ) ) ) = HELPCENTER_MAXQUERY )

    ' --- HelpCenter_IsSearchableWord ---------------------------------------------------
    HelpCenter_Check( "word: an operator is not searchable", _
        HelpCenter_IsSearchableWord( "=>" ) = false )
    HelpCenter_Check( "word: a digit run is not searchable", _
        HelpCenter_IsSearchableWord( "1234" ) = false )
    HelpCenter_Check( "word: letters plus digits are searchable", _
        HelpCenter_IsSearchableWord( "keyword2" ) = true )
    ' Non-ASCII counts as a letter: refusing it would mean refusing to look up any
    ' identifier its author can actually type.
    HelpCenter_Check( "word: non-ASCII counts as a letter", _
        HelpCenter_IsSearchableWord( wchr(&h00E9) ) = true )

    ' --- HelpCenter_IndexUrl -----------------------------------------------------------
    dim as DWSTRING wszUrl = HelpCenter_IndexUrl()
    HelpCenter_Check( "url: has the file scheme", _
        PsLeft( wszUrl, 8 ) = "file:///" )
    HelpCenter_Check( "url: no backslash survives", _
        PsInStr( wszUrl, "\" ) = 0 )
    HelpCenter_Check( "url: points at the landing page", _
        PsInStr( wszUrl, "settings/help/helpcenter/index.html" ) > 0 )

    ' --- the two sites are two folders -------------------------------------------------
    dim as DWSTRING wszTikoUrl = HelpCenter_IndexUrl( HELPCENTER_SITE_TIKO )
    HelpCenter_Check( "site: the default is the generated Help Center", _
        HelpCenter_IndexUrl() = HelpCenter_IndexUrl( HELPCENTER_SITE_DOCS ) )
    HelpCenter_Check( "site: Tiko Editor Help is a different folder", _
        wszTikoUrl <> wszUrl )
    HelpCenter_Check( "site: and it points at its own landing page", _
        PsInStr( wszTikoUrl, "settings/help/tikohelp/index.html" ) > 0 )
    HelpCenter_Check( "site: both sites live under settings\help", _
        (PsInStr( HelpCenter_RootFolder( HELPCENTER_SITE_DOCS ), "settings\help\" ) > 0) andalso _
        (PsInStr( HelpCenter_RootFolder( HELPCENTER_SITE_TIKO ), "settings\help\" ) > 0) )
    HelpCenter_Check( "site: the two captions are not the same string", _
        HelpCenter_SiteTitle( HELPCENTER_SITE_DOCS ) <> HelpCenter_SiteTitle( HELPCENTER_SITE_TIKO ) )
    HelpCenter_Check( "site: both captions are localized", _
        (len( HelpCenter_SiteTitle( HELPCENTER_SITE_DOCS ) ) > 0) andalso _
        (len( HelpCenter_SiteTitle( HELPCENTER_SITE_TIKO ) ) > 0) )
    HelpCenter_Check( "site: the not-found message is localized too", _
        (len( L(90,"") ) > 0) andalso (len( L(520,"") ) > 0) )

    ' These drive HelpCenter_PathToFileUrl on paths this machine does not have. tiko
    ' normally installs somewhere with no spaces, so an assertion against the live path
    ' alone would pass under an encoder that does nothing whatsoever.
    HelpCenter_Check( "url: a space becomes %20", _
        HelpCenter_PathToFileUrl( "C:\Program Files\tiko\index.html" ) = _
        "file:///C:/Program%20Files/tiko/index.html" )
    HelpCenter_Check( "url: percent is encoded first, not last", _
        HelpCenter_PathToFileUrl( "C:\a%b c\i.html" ) = _
        "file:///C:/a%25b%20c/i.html" )
    HelpCenter_Check( "url: a hash is encoded", _
        HelpCenter_PathToFileUrl( "C:\c#\i.html" ) = "file:///C:/c%23/i.html" )
    HelpCenter_Check( "url: a question mark is encoded", _
        HelpCenter_PathToFileUrl( "C:\a?b\i.html" ) = "file:///C:/a%3Fb/i.html" )

    ' --- localized ids -----------------------------------------------------------------
    ' len() > 0 CANNOT distinguish a real translation from the English fallback -- a key
    ' left blank in a translation is filled with English at load. The resolved values are
    ' printed for that reason; read them, do not just count the passes.
    HelpCenter_Check( "lang: 86 (Help Center) is present",  len( LL(86) ) > 0 )
    HelpCenter_Check( "lang: 88 (open failed) is present",  len( LL(88) ) > 0 )
    HelpCenter_Check( "lang: 90 (files missing) is present", len( LL(90) ) > 0 )
    dim as string sLang = "  resolved: 86=" & LL(86).Utf8 & " | 88=" & LL(88).Utf8 & _
                          " | 90=" & LL(90).Utf8
    print sLang

    print "--- TIKO_HELPCENTER_SELFTEST: " & str(gHelpCenterTestPass) & " passed, " & _
          str(gHelpCenterTestFail) & " failed ---"
end sub
