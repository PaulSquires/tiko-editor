'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' _check_app_layer -- src\app must not know it is on Windows.
''
'' src\app is the part of tiko that Phase 8 carries to Linux unchanged: the
'' symbol database, encoding, the ini parser, project folders, the fuzzy
'' matcher, the menu and theme definitions, the debugParser/fbcParser
'' integration.
''
'' A BOUNDARY THAT IS NOT CHECKED IS NOT A BOUNDARY. Every file in there was
'' measured free of Win32 before it moved, and one HWND parameter added in
'' passing puts it back on the wrong side without anyone noticing, because it
'' still compiles.
''
'' ---- WHY THIS IS A PROGRAM AND NOT A findstr LOOP -------------------------
''
'' It was a findstr loop first, and it reported three violations that were not:
''
''     ghMenuBar            matched HMENU
''     FlushPendingEdit     matched HPEN
''     "wParam carries..."  matched WPARAM, in a comment
''
'' Substring matching cannot tell a token from a fragment, and a checker that
'' cries wolf gets switched off. So this matches WHOLE WORDS and ignores
'' comments -- which is also what makes its output worth acting on: every line
'' it prints is a real dependency on Windows.
''
'' Build:  fbc64 _check_app_layer.bas    (or _check_app_layer.bat)
'' Exit code is the verdict.
'' ===========================================================================

#include once "file.bi"
#include once "dir.bi"

'' The Win32 and AfxNova vocabulary. Whole words only.
''
'' zstring, not string: fbc cannot initialise a var-len string array in its
'' declaration ("Var-len strings cannot be initialized"). Nor can it take a
'' comment inside the initialiser's line continuations, which is why all of
'' this is up here.
''
'' THIS LIST IS A SAMPLE OF THE WIN32 SURFACE, AND BEHAVES LIKE ONE. The first
'' version of it passed 28 files, two of which call MultiByteToWideChar,
'' WideCharToMultiByte or SciExec -- none of which it knew about. A checker is
'' only as good as its vocabulary. Widen it whenever something slips through,
'' and read a green run as evidence rather than as proof.
''
'' SciExec is tiko's own Scintilla wrapper rather than Win32, but it takes an
'' HWND and so ties a file to the editor window -- the same dependency by
'' another name.
''
'' THE AfxNova CLASSES ARE NOT CAUGHT BY THE Afx-PREFIX RULE BELOW, because they
'' are named CTextStream, CFileStream, CWebView2 and so on. That gap let
'' modIniParse.inc into app/ with a `dim pS as CTextStream` in it -- found when
'' src/app was compiled WITHOUT AfxNova in scope for the first time, which is
'' the thing the boundary was supposed to guarantee and had never been tested.
'' IsWindow, SetWindowRedraw, DestroyWindow, MapWindowPoints and CoTaskMemFree
'' were ADDED 2026-08-10, and the reason is worth more than the names. 7c step 3
'' commit 2 converted clsDocument's Win32 vocabulary by fixing what THIS LIST
'' reported, and stopping there. Five IsWindow calls and two SetWindowRedraw calls
'' survived -- because IsWindowVisible was here and IsWindow was not.
''
'' That is the same mistake as trusting a grep, one layer up: a checker is evidence
'' about the names it knows and says nothing whatever about the rest. The paragraph
'' above already said so, and it happened anyway.
dim shared as zstring * 24 g_banned(...) = { _
    "HWND", "HDC", "HMENU", "HBITMAP", "HICON", "HFONT", "HBRUSH", "HPEN", _
    "HINSTANCE", "WPARAM", "LPARAM", "LRESULT", "CWindow", "SendMessage", _
    "PostMessage", "InvalidateRect", "CreateWindowEx", "DefWindowProc", _
    "GetStockObject", "SelectObject", "BeginPaint", "DrawText", "SetTimer", _
    "KillTimer", "CoCreateInstance", "MessageBoxW", "IsWindowVisible", _
    "MultiByteToWideChar", "WideCharToMultiByte", "FormatMessageW", _
    "CreateFileW", "WriteFile", "ReadFile", "CloseHandle", "GetLastError", _
    "DWORD", "LPWSTR", "LPCWSTR", "LPSTR", "HGLOBAL", "GlobalAlloc", _
    "GlobalLock", "CP_UTF8", "CP_ACP", "IDOK", "IDCANCEL", "IDYES", "IDNO", _
    "SciExec", _
    "IsWindow", "SetWindowRedraw", "DestroyWindow", "MapWindowPoints", _
    "CoTaskMemFree", _
    _
    "CTextStream", "CFileStream", "CWinHttpRequest", "CImageCtx", "CGdiPlus", _
    "CWebView2", "CBSTR", "CWSTR" }

dim shared as long g_nBad

function IsWordChar(byval c as integer) as boolean
    if (c >= asc("A")) andalso (c <= asc("Z")) then return true
    if (c >= asc("a")) andalso (c <= asc("z")) then return true
    if (c >= asc("0")) andalso (c <= asc("9")) then return true
    return (c = asc("_"))
end function

'' TRUE when the word at pStart is immediately followed by the keyword `as`, i.e.
'' it is the NAME being declared rather than the TYPE it is declared as.
function IsDeclaredName(byref sLine as string, byval pStart as long, byval nLen as long) as boolean
    dim as long p = pStart + nLen
    do while (p <= len(sLine)) andalso ((sLine[p - 1] = asc(" ")) orelse (sLine[p - 1] = 9))
        p += 1
    loop
    if p + 1 > len(sLine) then return false
    if ucase(mid(sLine, p, 2)) <> "AS" then return false
    '' `as` must itself be a whole word, or `assert` would look like one.
    if p + 2 <= len(sLine) then
        if IsWordChar(sLine[p + 1]) then return false
    end if
    return true
end function


'' Whole-word search, case-insensitive. Position or 0.
function FindWord(byref sLine as string, byref sWord as string, _
                  byval pFrom as long = 1) as long
    dim as string a = ucase(sLine), b = ucase(sWord)
    dim as long p = pFrom
    if p < 1 then p = 1
    do
        p = instr(p, a, b)
        if p = 0 then return 0
        dim as boolean bLeft  = (p = 1) orelse (not IsWordChar(a[p - 2]))
        dim as boolean bRight = (p + len(b) - 1 >= len(a)) orelse _
                                (not IsWordChar(a[p + len(b) - 1]))
        if bLeft andalso bRight then return p
        p += 1
    loop
end function

'' Everything before a comment marker. Quotes are tracked so an apostrophe
'' INSIDE a string literal does not truncate the line -- otherwise a legitimate
'' path like "C:\it's" would hide whatever follows it.
function CodePart(byref sLine as string) as string
    '' bInQuote, not bIn: fbc has a Bin() function and identifiers are
    '' case-insensitive, so `bIn` is a duplicated definition -- reported two
    '' lines later as "no matching overloaded function, BIN". Same family as
    '' bSearch/bsearch and tip/Tip() elsewhere in this project.
    dim as boolean bInQuote
    for i as long = 0 to len(sLine) - 1
        dim as integer c = sLine[i]
        if c = asc("""") then bInQuote = (not bInQuote)
        if (not bInQuote) then
            if c = asc("'") then return left(sLine, i)
            '' A REM-style comment, which tiko uses in places.
            if (c = asc("r")) orelse (c = asc("R")) then
                if ucase(mid(sLine, i + 1, 4)) = "REM " then return left(sLine, i)
            end if
        end if
    next
    return sLine
end function

sub CheckFile(byref sPath as string, byref sName as string)
    dim as long h = freefile
    if open(sPath for input as #h) <> 0 then exit sub
    dim as long nLine
    do until eof(h)
        dim as string sLine
        line input #h, sLine
        nLine += 1
        dim as string sCode = CodePart(sLine)
        if len(trim(sCode)) = 0 then continue do

        for i as long = lbound(g_banned) to ubound(g_banned)
            '' EVERY occurrence on the line, not just the first. `hdc as HDC`
            '' declares a field NAMED hdc of TYPE HDC -- and the name comes first.
            '' Stopping at the first match let that line pass as a declaration
            '' while the real Win32 type sat three characters later.
            dim as long pHit = 0, pScan = 1
            do
                dim as long pTry = FindWord(sCode, g_banned(i), pScan)
                if pTry = 0 then exit do
                if IsDeclaredName(sCode, pTry, len(g_banned(i))) = false then
                    pHit = pTry
                    exit do
                end if
                pScan = pTry + 1
            loop
            if pHit > 0 then
                '' A NAME, NOT A TYPE, when the very next token is `as`. FreeBASIC
                '' declares as `<name> as <type>`, so a banned word FOLLOWED by `as`
                '' is being declared, and a banned word PRECEDED by `as` is being
                '' used. Only the second is a dependency on Win32.
                ''
                '' This is not hypothetical: modScintilla.bi mirrors Scintilla's C
                '' API and has struct fields NAMED wParam and lParam, which tripped
                '' the rules for the TYPES WPARAM and LPARAM. Renaming them to please
                '' a checker would have been the wrong repair -- the names match the
                '' upstream header on purpose.
                print "  FAIL  " & sName & "(" & nLine & "): " & g_banned(i)
                print "        " & trim(sLine)
                g_nBad += 1
                exit for
            end if
        next

        '' AfxNova is Windows-only by construction, so ANY Afx call is a
        '' violation. Matched as a prefix followed by a capital, which is the
        '' library's own naming, rather than as a list that would go stale.
        dim as long p = instr(ucase(sCode), "AFX")
        do while p > 0
            dim as integer nxt = iif(p + 3 <= len(sCode), sCode[p + 2], 0)
            dim as boolean bLeft = (p = 1) orelse (not IsWordChar(sCode[p - 2]))
            if bLeft andalso (nxt >= asc("A")) andalso (nxt <= asc("Z")) then
                print "  FAIL  " & sName & "(" & nLine & "): an AfxNova call"
                print "        " & trim(sLine)
                g_nBad += 1
                exit do
            end if
            p = instr(p + 1, ucase(sCode), "AFX")
        loop
    loop
    close #h
end sub

'' ---------------------------------------------------------------------------
'' NO TRAILING SEPARATOR for the fbDirectory test -- fbc's Dir() will not find
'' a directory named with one, and the same trap is written up in PsPlatform's
'' build.bas. Getting it wrong here made the checker scan ZERO files and then
'' report success, which is the worst answer available.
'' ---- CANDIDATE MODE: check files that are NOT in the layer yet -------------
''
'' `_check_app_layer.exe <file> [file...]` checks the named files instead of
'' scanning src/app, so "would this file survive the move?" can be asked BEFORE
'' moving it.
''
'' THIS EXISTS BECAUSE THE CHECKER WAS BYPASSED, NOT BECAUSE IT WAS WRONG. 7c
'' step 3 planned to move modSaveSelfTest.inc and modEncodingSelfTest.inc into
'' src/app on the strength of an ad-hoc grep that reported them free of Win32.
'' They are not: between them they name CreateFileW, ReadFile, CloseHandle,
'' LoadLibraryW, WideCharToMultiByte and CFileStream -- and EVERY ONE of those is
'' already in g_banned above. The vocabulary was never the gap. The gap was that
'' this checker could only be pointed at files ALREADY in the layer, so the cheap
'' way to ask the question was a grep, and the grep was worse.
if command(1) <> "" then
    dim as long nArg = 1, nChecked
    do while command(nArg) <> ""
        dim as string sPath = command(nArg)
        if dir(sPath, fbNormal) = "" then
            print "  FAIL  no such file: " & sPath
            end 2
        end if
        CheckFile(sPath, sPath)
        nChecked += 1
        nArg += 1
    loop
    print ""
    if g_nBad > 0 then
        print "  " & g_nBad & " violation(s) across " & nChecked & " candidate file(s)."
        print ""
        print "  These files cannot move into src/app as they stand."
        end 1
    end if
    print "  ok      " & nChecked & " candidate file(s) are free of Win32 and AfxNova"
    end 0
end if

dim as string sRoot = exepath() & "\src\app"
if dir(sRoot, fbDirectory) = "" then sRoot = exepath() & "\app"
dim as string sDir = sRoot & "\"

dim as string sFile = dir(sDir & "*.*", fbNormal)
dim as long nFiles
do while len(sFile) > 0
    dim as string sExt = lcase(right(sFile, 4))
    if (sExt = ".bas") orelse (sExt = ".inc") orelse (right(lcase(sFile), 3) = ".bi") then
        CheckFile(sDir & sFile, sFile)
        nFiles += 1
    end if
    sFile = dir()
loop

print ""
'' A CHECKER THAT SCANNED NOTHING MUST NOT SAY OK. Silence is not success: the
'' first version of this resolved its directory wrongly, found no files, and
'' reported a clean run -- which would have passed CI forever.
if nFiles = 0 then
    print "  FAIL  no files found in " & sDir
    print "        The app layer is missing or the path is wrong. Either way"
    print "        this checker is not checking anything."
    end 2
end if

if g_nBad > 0 then
    print "  " & g_nBad & " violation(s) across " & nFiles & " files in src\app."
    print ""
    print "  Either the code belongs in the shell, or it needs a PsPlatform"
    print "  abstraction. Not an #ifdef."
    end 1
end if
print "  ok      src\app is free of Win32 and AfxNova (" & nFiles & " files)"
end 0
