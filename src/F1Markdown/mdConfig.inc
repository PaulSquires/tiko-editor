'' ========================================================================================
'' mdConfig.inc -- the implementation. See mdConfig.bi for the two-files split.
''
'' KEY = VALUE, NOT KEY: VALUE. PsIniSyntaxColon trims both sides and would be the tidier
'' reader, and it cannot be used here for one concrete reason: a Windows path contains a
'' colon. "AfxNova: C:/dev/..." splits at the wrong one and silently yields a docset called
'' "AfxNova: C" pointing at "/dev/...".
'' ========================================================================================

#pragma once
#include once "mdConfig.bi"

private function CfgTrim( byref s as string ) as string
    dim as long a = 0, b = len(s) - 1
    while (a <= b) andalso ((s[a] = 32) orelse (s[a] = 9))
        a += 1
    wend
    while (b >= a) andalso ((s[b] = 32) orelse (s[b] = 9))
        b -= 1
    wend
    if b < a then return ""
    return mid(s, a + 1, b - a + 1)
end function

'' One pass over UTF-8 text: yields section, key and value per line. Written out rather than
'' driven through PsIniClassify because that one speaks DWSTRING and everything here is bytes
'' -- and because the colon problem above means the syntax is ours anyway.
private function CfgNextLine( byref s as string, byref nAt as long, byref sLine as string ) as boolean
    if nAt >= len(s) then return false
    dim as long nStart = nAt
    while (nAt < len(s)) andalso (s[nAt] <> 10) andalso (s[nAt] <> 13)
        nAt += 1
    wend
    sLine = mid(s, nStart + 1, nAt - nStart)
    if (nAt < len(s)) andalso (s[nAt] = 13) andalso (nAt + 1 < len(s)) andalso (s[nAt+1] = 10) then nAt += 1
    nAt += 1
    return true
end function


'' ---------------------------------------------------------------------------------------
'' f1markdown.ini
'' ---------------------------------------------------------------------------------------
private function StarterIni() as string
    dim as string NL = chr(13) & chr(10)
    return _
    "# F1Markdown -- what documentation this installation shows." & NL & _
    "#" & NL & "# One  name = path  per line under [docsets]. The name is the caption of the" & NL & _
    "# top-level node in the contents tree; the path is a folder that is walked" & NL & _
    "# recursively for .md files. A RELATIVE path resolves against this file's own" & NL & _
    "# folder, so the whole installation can be moved without editing this." & NL & _
    "#" & NL & "# A folder may carry a toc.txt naming the files that should come first, in" & NL & _
    "# order. Anything it does not mention follows in the default order -- numeric" & NL & _
    "# filename prefixes first (and hidden), then alphabetical by title." & NL & NL & _
    "[docsets]" & NL & _
    "tiko = docs" & NL & _
    "#AfxNova = ../HelpCenter/cache/AfxNova/docs" & NL & _
    "#PsControls = ../PsControls" & NL

end function

function MdConfigLoadDocsets( ds() as MdDocset, byref sExeDir as DWSTRING ) as long
    dim as DWSTRING sIni = PsPathJoin( sExeDir, PsText("f1markdown.ini") )

    if PsFileExists( sIni ) = false then
        '' A failure here is not fatal and not reported: an installation in a read-only
        '' location simply falls back to the built-in default root.
        PsFileWriteAll( sIni, StarterIni() )
    end if

    dim as boolean bOk = false
    dim as string s = MdReadUtf8( sIni, bOk )
    if bOk = false then return 0

    dim as long n = 0
    dim as long nAt = 0
    dim as string sLine
    dim as boolean bInDocsets = false

    while CfgNextLine( s, nAt, sLine )
        dim as string t = CfgTrim( sLine )
        if len(t) = 0 then continue while
        if (t[0] = 35) orelse (t[0] = 59) then continue while      '' # and ; comments
        if t[0] = 91 then                                          '' [section]
            bInDocsets = (lcase(t) = "[docsets]")
            continue while
        end if
        if bInDocsets = false then continue while

        dim as long nEq = instr(t, "=")
        if nEq = 0 then continue while
        dim as string sName = CfgTrim( left(t, nEq - 1) )
        dim as string sVal  = CfgTrim( mid(t, nEq + 1) )
        if (len(sName) = 0) orelse (len(sVal) = 0) then continue while

        dim as DWSTRING sRaw
        sRaw.Utf8 = sVal
        dim as DWSTRING sFull
        if PsPathIsAbsolute( sRaw ) then
            sFull = PsPathCanonicalise( sRaw )
        else
            sFull = PsPathCanonicalise( PsPathJoin(sExeDir, sRaw) )
        end if

        '' A configured root that does not exist is SKIPPED, not added as an empty node: an
        '' empty docset in the tree looks like documentation that failed to load.
        if PsFileIsDir( sFull ) = false then continue while

        if n > ubound(ds) then redim preserve ds(0 to (n + 1) * 2)
        ds(n).sName = sName
        ds(n).sPath = sFull
        n += 1
    wend
    return n
end function


'' ---------------------------------------------------------------------------------------
'' settings.ini
'' ---------------------------------------------------------------------------------------
function MdSettingsPath() as DWSTRING
    dim as DWSTRING sDir = PsPathJoin( PsKnownFolder(PSFOLDER_CONFIG), PsText("F1Markdown") )
    return PsPathJoin( sDir, PsText("settings.ini") )
end function

sub MdSettingsLoad( byref st as MdSettings )
    '' fbc zeroes neither New nor a stack Dim, so every field is set here whether or not the
    '' file exists. A settings file that is missing must read as "no preference", never as
    '' whatever was on the stack.
    st.nWinW = 0 : st.nWinH = 0 : st.nTocW = 0
    st.sTheme = PsText("")
    st.sLastPage = PsText("")

    dim as DWSTRING sPath = MdSettingsPath()
    if PsFileExists( sPath ) = false then exit sub
    dim as boolean bOk = false
    dim as string s = MdReadUtf8( sPath, bOk )
    if bOk = false then exit sub

    dim as long nAt = 0
    dim as string sLine
    while CfgNextLine( s, nAt, sLine )
        dim as string t = CfgTrim( sLine )
        if len(t) = 0 then continue while
        if (t[0] = 35) orelse (t[0] = 59) orelse (t[0] = 91) then continue while
        dim as long nEq = instr(t, "=")
        if nEq = 0 then continue while
        dim as string k = lcase( CfgTrim(left(t, nEq - 1)) )
        dim as string v = CfgTrim( mid(t, nEq + 1) )
        select case k
            case "window.width"  : st.nWinW = valint(v)
            case "window.height" : st.nWinH = valint(v)
            case "contents.width": st.nTocW = valint(v)
            case "theme"         : st.sTheme.Utf8 = v
            case "lastpage"      : st.sLastPage.Utf8 = v
        end select
    wend
end sub

sub MdSettingsSave( byref st as MdSettings )
    dim as string NL = chr(13) & chr(10)
    dim as string s = _
        "# F1Markdown per-user settings. Rewritten on exit; safe to delete." & NL & _
        "# Sizes are DESIGN units, not pixels, so they restore correctly on a display" & NL & _
        "# with a different scale factor than the one they were saved on." & NL & _
        "# The window POSITION is deliberately not here -- see mdConfig.bi." & NL & NL & _
        "window.width = "   & str(st.nWinW) & NL & _
        "window.height = "  & str(st.nWinH) & NL & _
        "contents.width = " & str(st.nTocW) & NL & _
        "theme = "          & st.sTheme.Utf8 & NL & _
        "lastpage = "       & st.sLastPage.Utf8 & NL

    dim as DWSTRING sPath = MdSettingsPath()
    PsDirCreateTree( PsPathDir(sPath) )

    '' Neighbour then replace, as the cache does. A settings file truncated by a power cut
    '' would be read back as "no preference", which is survivable -- but it costs one line to
    '' not have to think about it.
    dim as DWSTRING sTmp
    sTmp.Utf8 = sPath.Utf8 & ".tmp"
    if PsFileWriteAll( sTmp, s ) = false then exit sub
    if PsFileReplace( sTmp, sPath ) = false then PsFileDelete( sTmp )
end sub
