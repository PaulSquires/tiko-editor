'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' See frmSciHost.bi for what this class is and why 7d is four creation sites.

#pragma once

'' ---------------------------------------------------------------------------
'' Process-wide, initialised once. The render backend and the text engine are
'' global state in PsPlatform; the font is one face shared by every editor
'' window, because they all render the same document in the same theme.
'' ---------------------------------------------------------------------------
dim shared as boolean  g_bSciHostInit
dim shared as string   g_sSciHostFont
dim shared as PsC.PsTextEngine g_sciHostTE

sub SciHost_SetFontPath(byref sPath as string)
    g_sSciHostFont = sPath
end sub


'' SciHost_ApplyConfiguredFont LIVES IN modViewStyle.inc, not here. It reads
'' gConfig, and _check_scihost.bas compiles THIS file with a deliberately minimal
'' include set that has no config object -- so a gConfig reference here breaks the
'' one probe that proves tiko can host a PsSciView at all.



'' ---------------------------------------------------------------------------
'' A FONT FAMILY NAME IS NOT A FONT FILE, AND THIS LAYER NEEDS THE FILE.
''
'' PsPlatform's Scintilla renders through FreeType: PlatPs.cxx's FontPs takes
'' Scintilla's FontParameters, uses ONLY fp.size, and opens the process-global
'' path that PlatPs_SetFontPath holds. The face NAME is discarded -- so
'' SCI_STYLESETFONT, which is what every editor in the world uses to choose a
'' font, does nothing at all here.
''
'' That is why the Options font combo appeared to do nothing for so long: it
'' wrote gConfig.EditorFontname, ViewStyle_Apply dutifully sent it to Scintilla,
'' and the value was dropped one layer further down. The old build drew through
'' Scintilla64.dll -- Win32 GDI -- which honoured the name.
''
'' ---- THE REGISTRY IS WHERE WINDOWS KEEPS THE MAPPING -----------------------
''
''     "Consolas (TrueType)"      = "consola.ttf"
''     "Malgun Gothic (TrueType)" = "malgun.ttf"
''
'' under SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts. HKCU IS READ
'' FIRST and it is not optional: fonts a user installs without admin rights go
'' there and ONLY there, so an HKLM-only lookup silently cannot see them.
''
'' ---- EXACT MATCH, DELIBERATELY ---------------------------------------------
''
'' The value name is the family plus a style plus a parenthesised format. Strip
'' the parentheses and compare the whole remainder:
''
''   "Malgun Gothic (TrueType)"      -> "Malgun Gothic"      MATCHES
''   "Malgun Gothic Bold (TrueType)" -> "Malgun Gothic Bold" does not
''
'' which is what picks the REGULAR face rather than whichever weight happens to
'' enumerate first.
''
'' COLLECTIONS ARE NOT MATCHED, AND THAT IS THE SAFE ANSWER. Several CJK
'' families ship as one .ttc under a value name listing several families:
'' "Batang & BatangChe & Gungsuh & GungsuhChe (TrueType)". Such a name never
'' equals a single family, so it falls through to the Consolas fallback below.
'' The alternative -- splitting on "&" and using the file -- would hand
'' FT_New_Face a collection whose face INDEX 0 is very likely a different family
'' from the one asked for, and render the wrong typeface while reporting
'' success. A readable fallback beats a confident wrong answer.
'' ---------------------------------------------------------------------------
'' PLAIN FreeBASIC STRING BUILT-INS, NOT AfxNova's Ps* HELPERS. This file is
'' compiled by _check_scihost.bas as well as by tiko, and that probe deliberately
'' includes the minimum -- AfxNova's string layer is not in its translation unit.
'' ucase/instrrev/left/trim on a wstring do the same work with no dependency, and
'' they keep this function compilable anywhere frmSciHost is.
private function SciHost_FontFileFromHive( byval hRoot as HKEY, _
                                           byref wszFamily as const wstring ) as string
    dim as HKEY hk
    '' A wstring VARIABLE, not @"literal": RegOpenKeyExW takes LPCWSTR and fbc
    '' gives a literal's @ as a zstring ptr, which compiles with a warning and
    '' reads the wrong bytes.
    dim as wstring * 64 wszKey = "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"
    if RegOpenKeyExW( hRoot, @wszKey, 0, KEY_READ, @hk ) <> ERROR_SUCCESS then return ""

    dim as wstring * 256 wszWant = ucase( *@wszFamily )
    dim as string sFound

    dim as DWORD idx = 0
    do
        dim as wstring * 512 wszName
        dim as wstring * 512 wszData
        dim as DWORD cchName = 511
        dim as DWORD cbData  = 511 * 2
        dim as DWORD dwType  = 0

        dim as long rc = RegEnumValueW( hk, idx, @wszName, @cchName, 0, @dwType, _
                                        cast(BYTE PTR, @wszData), @cbData )
        if rc <> ERROR_SUCCESS then exit do
        idx += 1
        if dwType <> REG_SZ then continue do

        '' Strip the trailing " (TrueType)" / " (OpenType)" / anything in parens.
        dim as wstring * 512 wszEntry = wszName
        dim as long nParen = instrrev( wszEntry, "(" )
        if nParen > 1 then wszEntry = left( wszEntry, nParen - 1 )
        wszEntry = trim( wszEntry )

        if ucase( wszEntry ) = wszWant then
            sFound = str( wszData )
            exit do
        end if
    loop

    RegCloseKey( hk )
    return sFound
end function


'' The family the user chose, as a path FreeType can open. Never returns "" --
'' see the fallback note below.
function SciHost_ResolveFontFile( byref wszFamily as const wstring ) as string

    '' THE FALLBACK IS UNCONDITIONAL AND IT IS LOAD-BEARING. TE_Init returning 0
    '' makes SciHost_GlobalInit return false, which is an editor window that
    '' never appears -- so an unresolvable family must degrade to a font that is
    '' present on every Windows install, not to "".
    dim as string sFallback = environ("WINDIR") & "\Fonts\consola.ttf"

    if len( wszFamily ) = 0 then return sFallback

    '' HKCU FIRST: a font installed without admin rights exists only there.
    dim as string sFile = SciHost_FontFileFromHive( HKEY_CURRENT_USER, wszFamily )
    if len( sFile ) = 0 then
        sFile = SciHost_FontFileFromHive( HKEY_LOCAL_MACHINE, wszFamily )
    end if
    if len( sFile ) = 0 then return sFallback

    '' The value is USUALLY a bare filename relative to the Fonts directory, but
    '' it is allowed to be absolute -- and is, for fonts installed per-user.
    dim as boolean bAbsolute = false
    if len( sFile ) > 2 then
        if mid( sFile, 2, 1 ) = ":" then bAbsolute = true
        if left( sFile, 2 ) = "\\" then bAbsolute = true
    end if
    if bAbsolute = false then sFile = environ("WINDIR") & "\Fonts\" & sFile

    '' A registry entry whose file is gone is not rare -- an uninstall that left
    '' the key behind. FileExists rather than PsFileExists, for the dependency
    '' reason at the top of the hive reader.
    if len( dir( sFile ) ) = 0 then return sFallback

    return sFile
end function


'' ---------------------------------------------------------------------------
'' Env-gated (TIKO_FONTFILE_SELFTEST=1). PURE APART FROM THE REGISTRY, which is
'' the one thing it cannot fake -- so what it asserts is the LOGIC around the
'' lookup: the suffix strip, the case-insensitivity, the relative-path join and,
'' most of all, THE FALLBACK.
''
'' The fallback is the assertion that matters. TE_Init returning 0 makes
'' SciHost_GlobalInit return false, which is an editor window that never appears;
'' a resolver that answers "" for a font somebody uninstalled would take the
'' whole editor down rather than degrade to Consolas.
''
'' WHAT IT CANNOT ASSERT: that Consolas is installed. It is on every Windows
'' since Vista and the fallback names it unconditionally, so a machine without it
'' has a bigger problem than this suite -- but the dependency is real and stated
'' rather than assumed.
'' ---------------------------------------------------------------------------
sub SciHost_RunFontFileSelfTest()

    if environ("TIKO_FONTFILE_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0

    #macro FFASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            ? "  FAIL: " & _desc_
        end if
    #endmacro

    ? "--- TIKO_FONTFILE_SELFTEST ---"

    dim as string sFallback = environ("WINDIR") & "\Fonts\consola.ttf"

    '' A family that is on every Windows install, resolved through the real
    '' registry -- the only end-to-end case available without a fixture.
    dim as string sCon = SciHost_ResolveFontFile( "Consolas" )
    FFASSERT( len(sCon) > 0, "Consolas resolved to nothing" )
    FFASSERT( len(dir(sCon)) > 0, "Consolas resolved to a file that is not there: " & sCon )
    FFASSERT( lcase(right(sCon, 4)) = ".ttf", "Consolas did not resolve to a .ttf: " & sCon )

    '' THE SUFFIX STRIP, proved by the fact that the value name in the registry is
    '' "Consolas (TrueType)" and never the bare family. If the strip were missing,
    '' the case above would already have fallen back -- so this asserts the two are
    '' the SAME answer for the right reason rather than by luck.
    FFASSERT( lcase(sCon) = lcase(sFallback), _
              "Consolas should resolve to the same file the fallback names: " & sCon )

    '' CASE-INSENSITIVE. Config files, and users, carry either spelling.
    FFASSERT( lcase(SciHost_ResolveFontFile("CONSOLAS")) = lcase(sCon), _
              "an uppercased family did not resolve to the same file" )
    FFASSERT( lcase(SciHost_ResolveFontFile("consolas")) = lcase(sCon), _
              "a lowercased family did not resolve to the same file" )

    '' THE FALLBACK, three ways in. None of these may ever return "".
    FFASSERT( SciHost_ResolveFontFile("") = sFallback, _
              "an empty family did not fall back to Consolas" )
    FFASSERT( SciHost_ResolveFontFile("No Such Font Family At All") = sFallback, _
              "an unknown family did not fall back to Consolas" )
    '' EXACT MATCH DISCRIMINATES, and this is what that buys.
    ''
    '' THIS ASSERTION WAS WRONG WHEN IT WAS WRITTEN. It read
    ''     ResolveFontFile("Consolas Bold") = sFallback
    '' on the reasoning that a style is not a family and should not resolve. But
    '' "Consolas Bold (TrueType)" is a real VALUE NAME in that registry key, and
    '' this function maps NAME -> FILE: resolving it is correct, and the suite was
    '' asserting a rule the code had no reason to follow. It failed on the first
    '' run, which is the assertion doing its job in the wrong direction.
    ''
    '' The property that actually matters is that the two names give DIFFERENT
    '' files. A prefix or "starts with" match would answer consolab.ttf for both
    '' and silently make the bold face the regular one for the whole editor.
    dim as string sBold = SciHost_ResolveFontFile( "Consolas Bold" )
    FFASSERT( len(dir(sBold)) > 0, "Consolas Bold resolved to nothing on disk: " & sBold )
    FFASSERT( lcase(sBold) <> lcase(sCon), _
              "Consolas and Consolas Bold resolved to the SAME file: " & sBold )

    '' A RELATIVE VALUE IS JOINED TO THE FONTS DIRECTORY. The registry stores bare
    '' filenames, so a resolved path that is not absolute means the join is gone.
    FFASSERT( mid(sCon, 2, 1) = ":", "the resolved path is not absolute: " & sCon )

    ? "--- font file self-test: " & nPass & " passed, " & nFail & " failed ---"

end sub


'' GetDpiForWindow, resolved at run time. It is Windows 10 1607+, and fbc's
'' windows.bi does not declare it -- a static import would refuse to start on
'' anything older rather than fall back.
private function SciHost_DpiFor(byval hWnd as HWND) as long
    static as any ptr pfn
    static as boolean bTried
    if bTried = false then
        bTried = true
        dim as any ptr hUser = GetModuleHandle("user32.dll")
        if hUser then pfn = GetProcAddress(hUser, "GetDpiForWindow")
    end if
    if pfn <> 0 then
        dim as function stdcall (byval as HWND) as ulong f = pfn
        dim as long d = f(hWnd)
        if d >= 48 then return d
    end if
    '' The desktop DC, which reports the SYSTEM dpi -- right on a single-monitor
    '' box and stale on a mixed-DPI one. Only reached on pre-1607.
    dim as HDC hdc = GetDC(0)
    dim as long d = GetDeviceCaps(hdc, LOGPIXELSY)
    ReleaseDC(0, hdc)
    return iif(d >= 48, d, 96)
end function

private function SciHost_GlobalInit() as boolean
    if g_bSciHostInit then return true
    if len(g_sSciHostFont) = 0 then
        g_sSciHostFont = environ("WINDIR") & "\Fonts\consola.ttf"
    end if
    '' PsRenderInit BEFORE any paint, and PsTextEngineInstallApi before
    '' Scintilla measures anything -- Scintilla asks the engine for metrics
    '' during SCI_SETTEXT, long before the first WM_PAINT.
    PsC.PsRenderInit()
    PsC.PsTextEngineInstallApi()
    if PsC.TE_Init(g_sciHostTE, strptr(g_sSciHostFont), 15) = 0 then return false
    g_bSciHostInit = true
    return true
end function

'' ---------------------------------------------------------------------------
'' State lookup. GWLP_USERDATA rather than a lookup table: this class is created
'' four times per document plus sixteen for the FIP pool, and a table would be
'' one more thing to keep in step with DestroyWindow.
'' ---------------------------------------------------------------------------
function SciHost_StateFromHwnd(byval hWnd as HWND) as SciHostState ptr
    if hWnd = 0 then return 0
    return cast(SciHostState ptr, GetWindowLongPtr(hWnd, GWLP_USERDATA))
end function

function SciHost_DirectPointer(byval hWnd as HWND) as any ptr
    dim as SciHostState ptr pSt = SciHost_StateFromHwnd(hWnd)
    if pSt = 0 then return 0
    if pSt->pView = 0 then return 0
    return pSt->pView->pSci
end function

'' ---------------------------------------------------------------------------
'' Damage -> InvalidateRect. THE REPAINT DRIVER, and the reason it is a named
'' function called from several places rather than one line at the end of the
'' window procedure.
''
'' Scintilla repaints in response to things that are not input: a notification
'' handler that moves the caret, an SCI_* message sent from tiko's own code, an
'' autocompletion list closing. Every one of those marks damage on the surface
'' with no Win32 message in flight to hang a repaint off.
''
'' minieditor's "selecting from the completion list does not show until another
'' refresh" was exactly this, one layer down -- Window::InvalidateAll was a
'' no-op. Damage that nobody turns into a repaint looks identical to damage that
'' was never marked.
'' ---------------------------------------------------------------------------
private sub SciHost_FlushDamage(byval pSt as SciHostState ptr)
    if pSt = 0 then exit sub
    if pSt->surf.DamageCount() = 0 then exit sub
    '' FALSE: the whole client area is repainted from the DIB anyway, so erasing
    '' first would only add a flash of background.
    InvalidateRect(pSt->hWnd, null, false)
end sub

'' ---------------------------------------------------------------------------
'' PSEV_NOTIFY -> WM_NOTIFY. What keeps tiko's 13 notification files unchanged.
''
'' tiko resolves the document from `pNMHDR->hwndFrom` and gates on
'' `IsValidScintillaID(id)`, so both must be the host window's own -- getting
'' either wrong does not crash, it silently attributes an edit to no document.
'' ---------------------------------------------------------------------------
private sub SciHost_OnNotify(byval user as any ptr, byref ev as PsC.PsEvent)
    dim as SciHostState ptr pSt = cast(SciHostState ptr, user)
    if pSt = 0 then exit sub

    dim as SCNotification scn
    with scn
        .hdr.hwndFrom        = pSt->hWnd
        .hdr.idFrom          = pSt->nId
        .hdr.code            = ev.notify.code
        .position            = ev.notify.position
        .ch                  = ev.notify.ch
        .modifiers           = ev.notify.modifiers
        .modificationType    = ev.notify.modificationType
        .length              = ev.notify.length
        .linesAdded          = ev.notify.linesAdded
        .message             = ev.notify.message
        .wParam              = ev.notify.wParam
        .lParam              = ev.notify.lParam
        .line                = ev.notify.line
        .foldLevelNow        = ev.notify.foldLevelNow
        .foldLevelPrev       = ev.notify.foldLevelPrev
        .margin              = ev.notify.margin
        .listType            = ev.notify.listType
        .x                   = ev.notify.x
        .y                   = ev.notify.y
        .token               = ev.notify.token
        .annotationLinesAdded = ev.notify.annotationLinesAdded
        .updated             = ev.notify.updated
        .listCompletionMethod = ev.notify.listCompletionMethod
    end with

    '' Stored on the STATE, not in a local. The parent reads lptext during the
    '' SendMessage below; a local string would be destroyed at the end of this
    '' sub, which is after the pointer was handed over but -- with SendMessage
    '' being synchronous -- before anyone could notice the difference in testing.
    pSt->sNotifyUtf8 = ev.notify.text.Utf8
    scn.lptext = iif(len(pSt->sNotifyUtf8) > 0, strptr(pSt->sNotifyUtf8), cast(zstring ptr, 0))

    '' SYNCHRONOUS, matching Scintilla's own contract and PsSciNotify's. A
    '' handler that reacts to SCN_MODIFIED by reading the document must see the
    '' document as it was when the notification fired; posting would let the
    '' next edit land first.
    dim as HWND hParent = GetParent(pSt->hWnd)
    if hParent then SendMessage(hParent, WM_NOTIFY, pSt->nId, cast(LPARAM, @scn))

    '' A notification handler routinely moves the caret or changes styling, and
    '' that damage has no message of its own to ride home on.
    SciHost_FlushDamage(pSt)
end sub

'' ---------------------------------------------------------------------------
'' THE CLIPBOARD, and the caret's blink. Both are HOST obligations that the
'' editor shim deliberately does not default, and until this was written neither
'' was met -- so Ctrl+C filled a buffer nobody read, Ctrl+V was an empty
'' override, and the caret was asked to stand still because nothing could have
'' made it blink.
''
'' WHY WIN32 AND NOT PsCore'S. tiko never calls PsPlatformInit: the whole point
'' of PsWin32Host is that tiko keeps its own window and its own message loop, so
'' g_plat.clip is not filled in and SDL is not initialised. PsSciClipboard.inc is
'' the SDL wiring for demos; this is the same two hooks against the Win32
'' clipboard. Likewise the tickers: PsSciView drives them off PsTimer, which
'' needs a pump to service it, and a GetMessage loop is not that pump -- a
'' WM_TIMER is what wakes this loop, so that is what drives them here.
'' ---------------------------------------------------------------------------

'' Timer ids on the host window. Offset so they cannot collide with anything
'' else this window might one day set.
const SCIHOST_TICK_BASE = 700

private function SciHost_ClipGet cdecl (byval user as any ptr) as zstring ptr
    dim as SciHostState ptr pSt = cast(SciHostState ptr, user)
    if pSt = 0 then return 0
    pSt->sClipUtf8 = ""

    if IsClipboardFormatAvailable(CF_UNICODETEXT) = 0 then return strptr(pSt->sClipUtf8)
    '' A failed open is a normal outcome, not an error worth reporting: another
    '' process can hold the clipboard for a few milliseconds at a time.
    if OpenClipboard(pSt->hWnd) = 0 then return strptr(pSt->sClipUtf8)

    dim as HANDLE h = GetClipboardData(CF_UNICODETEXT)
    if h then
        dim as wstring ptr pw = cast(wstring ptr, GlobalLock(h))
        if pw then
            '' WideCharToMultiByte, NOT DWSTRING. This file is compiled in two
            '' include orders -- tiko's, where the unqualified name is PsCore's,
            '' and _check_scihost's probe, where it is AfxNova's -- and the two
            '' types have different members. The probe exists to prove this file
            '' stands on PsCore alone, so the answer is to depend on neither and
            '' use the Win32 call the conversion is, in a Win32 host file.
            dim as long nBytes = WideCharToMultiByte(CP_UTF8, 0, pw, -1, 0, 0, 0, 0)
            if nBytes > 1 then
                '' nBytes counts the terminator; the fbc string does not.
                pSt->sClipUtf8 = space(nBytes - 1)
                WideCharToMultiByte(CP_UTF8, 0, pw, -1, strptr(pSt->sClipUtf8), _
                                    nBytes, 0, 0)
            end if
            GlobalUnlock(h)
        end if
    end if
    CloseClipboard()
    return strptr(pSt->sClipUtf8)
end function

private sub SciHost_ClipSet cdecl (byval user as any ptr, byval utf8 as const zstring ptr)
    dim as SciHostState ptr pSt = cast(SciHostState ptr, user)
    if pSt = 0 then exit sub
    if utf8 = 0 then exit sub

    '' MultiByteToWideChar for the same reason as the getter -- see there.
    '' nChars INCLUDES the terminator, which is what -1 asks for and what the
    '' clipboard requires: a CF_UNICODETEXT block without one is read past its
    '' end by whatever pastes it.
    dim as long nChars = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0)
    if nChars < 1 then exit sub

    dim as HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nChars * 2)
    if hMem = 0 then exit sub

    dim as wstring ptr pw = cast(wstring ptr, GlobalLock(hMem))
    if pw = 0 then
        GlobalFree(hMem)
        exit sub
    end if
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, pw, nChars)
    GlobalUnlock(hMem)

    if OpenClipboard(pSt->hWnd) = 0 then
        '' OWNERSHIP DID NOT TRANSFER, so the block is still ours to free.
        '' Leaking it here would leak once per failed copy, which is exactly the
        '' shape that never shows up in a test and grows all afternoon.
        GlobalFree(hMem)
        exit sub
    end if
    EmptyClipboard()
    if SetClipboardData(CF_UNICODETEXT, hMem) = 0 then GlobalFree(hMem)
    CloseClipboard()
end sub

'' Scintilla starting or stopping a fine ticker. ARM AND RETURN: this is called
'' from inside an edit, and re-entering the editor here would run TickFor in the
'' middle of a document modification.
private sub SciHost_Ticker cdecl (byval user as any ptr, byval nReason as long, _
                                  byval nMillis as long, byval nRunning as long)
    dim as SciHostState ptr pSt = cast(SciHostState ptr, user)
    if pSt = 0 then exit sub
    if pSt->hWnd = 0 then exit sub
    if (nReason < 0) orelse (nReason > 4) then exit sub

    if nRunning <> 0 then
        if nMillis < 1 then nMillis = 1
        SetTimer(pSt->hWnd, SCIHOST_TICK_BASE + nReason, nMillis, 0)
    else
        KillTimer(pSt->hWnd, SCIHOST_TICK_BASE + nReason)
    end if
end sub

'' ---------------------------------------------------------------------------
'' Creation and teardown.
'' ---------------------------------------------------------------------------
private function SciHost_OnCreate(byval hWnd as HWND) as boolean
    if SciHost_GlobalInit() = false then return false

    dim as SciHostState ptr pSt = new SciHostState
    if pSt = 0 then return false
    pSt->hWnd = hWnd
    pSt->nId  = GetDlgCtrlID(hWnd)
    SetWindowLongPtr(hWnd, GWLP_USERDATA, cast(LONG_PTR, pSt))

    '' The engine widgets MEASURE against. It lives on the surface rather than in
    '' a global because measuring happens at layout time, when there is no paint
    '' in progress to ask -- leaving it null is not a crash, it makes every
    '' caption measure 0, which is what "the context menu does nothing" looked
    '' like in minieditor.
    pSt->surf.pText = cptr(PsC.PsTextEngine_ ptr, @g_sciHostTE)
    pSt->pnt.pText  = @g_sciHostTE


    dim as RECT rc
    GetClientRect(hWnd, @rc)
    '' A zero-sized window is the NORMAL case here: clsDocument creates both
    '' splits at 0,0,0,0 and sizes them later. Scintilla tolerates it, so this
    '' must too -- clamp rather than fail.
    dim as long cw = iif(rc.right  > 0, rc.right,  1)
    dim as long ch = iif(rc.bottom > 0, rc.bottom, 1)

    ' BEFORE the view is created, because Scintilla realises its default font
    ' during construction and would otherwise cache one at 96 dpi.
    '
    ' Without this the editor renders at 96 dpi while tiko -- which IS dpi aware --
    ' scales everything around it, so on a 150% display the text comes out two
    ' thirds size against correctly-sized tabs and menus. That is what "the text
    ' is VERY small" was.
    PlatPs_SetDpi( SciHost_DpiFor(hWnd) )

    pSt->pView = new PsC.PsSciView()
    if pSt->pView = 0 then return false
    if pSt->pView->Create(g_sSciHostFont, cw, ch) = false then return false

    ' THE VIEW IS THE ROOT. It was a child of a bare PsWidget, and that is why the
    ' pane came up blank BLACK with typing that worked but could not be seen.
    '
    ' PsSurface.Resize sets the bounds of THE ROOT and stops there -- a plain
    ' PsWidget is not a layout manager and does not pass anything to its
    ' children. So the view's bounds stayed 0x0, OnLayout published a size of
    ' zero to Scintilla, and the widget painted nothing: the black was the
    ' untouched DIB, not the editor's background.
    '
    ' minieditor got away with the same shape because it has a real layout
    ' function that sets the view's bounds explicitly -- it has a status bar to
    ' place. This window holds exactly one widget and needs no layout at all, so
    ' making it the root is both simpler and the thing that makes Resize work.
    pSt->surf.SetRoot(pSt->pView)

    if pSt->bridge.Attach(@pSt->surf, hWnd) = false then return false
    pSt->bridge.Resize(cw, ch)

    PsC.PsSciNotifyAttach(pSt->sink, pSt->pView->pSci, @SciHost_OnNotify, pSt)

    ' SURFACE FOCUS, SET ONCE AND NEVER MOVED. PsSurface routes key events to
    ' pFocus, and a surface with none drops every one of them -- so an editor
    ' that renders and takes the mouse would silently ignore the keyboard.
    '
    ' This is NOT the same thing as Win32 focus, which the HWND still tracks: the
    ' surface holds exactly one focusable widget, so the only question it can ask
    ' is answered here for good. The old Scintilla window had no such split --
    ' it handled WM_KEYDOWN whether it was focused or not, which is why
    ' frmFindInProject's SendMessage(hSlotView, WM_KEYDOWN, VK_DOWN, 0) worked
    ' against an unfocused, zero-sized, never-shown pool window.
    pSt->surf.SetFocus(pSt->pView)

    '' THE TWO HOST OBLIGATIONS. Both were unmet, and both were reported as
    '' defects rather than found: copy and paste did nothing, and the caret did
    '' not blink.
    SciPs_SetClipboardHooks(pSt->pView->pSci, @SciHost_ClipGet, @SciHost_ClipSet, pSt)
    SciPs_SetTickerCallback(pSt->pView->pSci, @SciHost_Ticker, pSt)

    '' THE SYSTEM'S BLINK RATE, not a number picked here. A user who has turned
    '' blinking off in Windows gets 0 back from GetCaretBlinkTime's own
    '' "infinite" value and must not be given a blinking caret anyway.
    '' This was 0 -- deliberately steady -- for as long as the fine tickers only
    '' recorded and nothing could have driven a blink.
    dim as ulong nBlink = GetCaretBlinkTime()
    if nBlink = INFINITE then nBlink = 0        '' the user asked for steady
    if nBlink > 10000 then nBlink = 0           '' and anything absurd is steady too
    pSt->pView->Msg(SCI_SETCARETPERIOD, nBlink, 0)

    return true
end function

private sub SciHost_OnDestroy(byval hWnd as HWND)
    dim as SciHostState ptr pSt = SciHost_StateFromHwnd(hWnd)
    if pSt = 0 then exit sub
    SetWindowLongPtr(hWnd, GWLP_USERDATA, 0)

    '' DETACH FIRST. Scintilla notifies during teardown as well as during
    '' editing, so a sink that outlives its registration is a use-after-free
    '' with a delay on it. The clipboard and ticker hooks carry `pSt` and are
    '' called from the same teardown, so they go the same way and for the same
    '' reason -- and the timers are killed, since a WM_TIMER arriving after the
    '' state is deleted would dispatch through freed memory.
    if pSt->pView <> 0 then
        if pSt->pView->pSci <> 0 then
            SciPs_SetClipboardHooks(pSt->pView->pSci, 0, 0, 0)
            SciPs_SetTickerCallback(pSt->pView->pSci, 0, 0)
        end if
    end if
    for i as long = 0 to 4
        KillTimer(hWnd, SCIHOST_TICK_BASE + i)
    next
    PsC.PsSciNotifyDetach(pSt->sink)
    pSt->bridge.Detach()
    '' pView is NOT deleted here -- the surface owns its tree and destroys it.
    pSt->pView = 0
    delete pSt
end sub

'' ---------------------------------------------------------------------------
'' The window procedure.
'' ---------------------------------------------------------------------------
'' STDCALL, EXPLICITLY. Every window procedure is stdcall, and fbc's default on
'' this target is not -- leaving it off produces only a "Suspicious pointer
'' assignment" warning at the RegisterClassEx, which is easy to read past.
'' On x64 the two conventions happen to coincide, so it would also have worked
'' by accident, and gone on working until the 32-bit build.
private function SciHost_WndProc stdcall ( byval hWnd as HWND, byval nMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM _
                                  ) as LRESULT
    '' ---- SCI_* FIRST, and before the state check has any chance to matter.
    ''
    '' THIS SINGLE BRANCH IS 7d. It carries all ~212 SciExec sites, all 350
    '' SciMsg sites and everything clsDocument sends, with no call site edited.
    ''
    '' UP TO 5000, NOT 4000. THE LEXER MESSAGES LIVE ABOVE 4000 -- SCI_SETILEXER
    '' is 4033, SCI_GETLEXER 4002, SCI_SETKEYWORDS 4005 -- and a 4000 ceiling sent
    '' every one of them to DefWindowProc instead. The editor worked and simply
    '' had no lexer: all text came out in style 0, which looks exactly like a
    '' theme that forgot its colours rather than like a message being dropped.
    ''
    '' Measured against a stock Scintilla window in the SAME PROCESS, given the
    '' same styles and the same tiko lexer: stock reported lexer id 8 and 5
    '' distinct style bytes, the fork reported id 0 and 1.
    ''
    '' The range is still safe by Scintilla's own design: ordinary window messages
    '' are below 0x0400, WM_APP is 0x8000 and registered messages are above
    '' 0xC000, so 2000-4999 belongs to nobody else.
    if (nMsg >= SCI_START) andalso (nMsg < 5000) then
        dim as SciHostState ptr pSt = SciHost_StateFromHwnd(hWnd)
        if pSt = 0 orelse pSt->pView = 0 then return 0
        '' NOT PsC.SciPs_Send. The extern "c" block lives at GLOBAL scope by
        '' necessity -- inside the namespace fbc mangles it to PSC::SciPs_Send and
        '' it links against nothing. So the C entry points are global while the
        '' FreeBASIC around them is not, and this line is where that shows.
        dim as LRESULT r = SciPs_Send(pSt->pView->pSci, nMsg, wParam, lParam)
        '' Sent from tiko's own code, with no input message in flight -- so this
        '' is the only chance to notice what it changed. See SciHost_FlushDamage.
        SciHost_FlushDamage(pSt)
        return r
    end if

    dim as SciHostState ptr pSt = SciHost_StateFromHwnd(hWnd)

    select case nMsg
        case WM_NCCREATE
            '' Must return TRUE or the window is not created at all.
            return DefWindowProc(hWnd, nMsg, wParam, lParam)

        case WM_CREATE
            '' -1 aborts creation, which is the right answer: a half-built
            '' editor that reports success would fail later, somewhere else.
            if SciHost_OnCreate(hWnd) = false then return -1
            return 0

        case WM_DESTROY
            SciHost_OnDestroy(hWnd)
            return 0

        case WM_TIMER
            '' A fine ticker coming due -- the caret's blink, and Scintilla's
            '' drag auto-scroll and dwell when they run. SciPs_Tick toggles the
            '' caret and invalidates it INSIDE Scintilla, which lands as surface
            '' damage; the flush is what turns that into a WM_PAINT.
            if pSt then
                dim as long nReason = clng(wParam) - SCIHOST_TICK_BASE
                if (nReason >= 0) andalso (nReason <= 4) then
                    if pSt->pView <> 0 then
                        SciPs_Tick(pSt->pView->pSci, nReason)
                        SciHost_FlushDamage(pSt)
                    end if
                    return 0
                end if
            end if
            '' NOT OURS: fall through to DefWindowProc rather than swallowing it.
            exit select

        case WM_SIZE
            if pSt then
                pSt->bridge.Resize(LOWORD(lParam), HIWORD(lParam))
                InvalidateRect(hWnd, null, false)
            end if
            return 0

        case WM_ERASEBKGND
            '' The DIB covers every pixel of the client area. Letting the
            '' default erase run first only produces a flash of background.
            return 1

        case WM_PAINT
            if pSt = 0 then return DefWindowProc(hWnd, nMsg, wParam, lParam)
            dim as PAINTSTRUCT ps
            dim as HDC hdc = BeginPaint(hWnd, @ps)
            pSt->bridge.PaintTo(hdc, pSt->pnt)
            EndPaint(hWnd, @ps)
            return 0

        case WM_GETDLGCODE
            '' EVERYTHING, INCLUDING TAB AND RETURN. Without this the dialog
            '' manager eats Tab for focus navigation and Return for the default
            '' button, and neither ever reaches the editor -- in a code editor
            '' that is indistinguishable from those two keys being broken.
            return DLGC_WANTALLKEYS or DLGC_WANTCHARS or DLGC_WANTARROWS or DLGC_WANTTAB

        case WM_SETFOCUS, WM_KILLFOCUS
            if pSt then
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

        case WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_LBUTTONDBLCLK
            '' TAKE THE FOCUS EXPLICITLY. A click on a child window does not move
            '' focus by itself, and the keyboard would go on talking to whatever
            '' had it before -- looking like an editor that accepts clicks but
            '' ignores typing.
            SetFocus(hWnd)
            SetCapture(hWnd)
            if pSt then
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

        case WM_LBUTTONUP, WM_MBUTTONUP
            ReleaseCapture()
            if pSt then
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

        case WM_RBUTTONUP
            '' THE ONE MOUSE-UP THAT MUST STILL REACH DefWindowProc, and the
            '' reason the editor's right-click menu disappeared in Phase 7d.
            ''
            '' Nothing SENDS WM_CONTEXTMENU. DefWindowProc SYNTHESISES it from
            '' the right-button release, and when the window does not handle it
            '' DefWindowProc walks it up to the parent -- which is HWND_FRMMAIN,
            '' where frmMain_OnContextMenu calls ShowScintillaContextMenu.
            ''
            '' The real Scintilla window went through that chain. This one
            '' returned 0 for the whole button-up group, so the chain stopped
            '' here and no WM_CONTEXTMENU was ever generated. The bridge still
            '' gets the message first; DefWindowProc only adds the synthesis.
            ReleaseCapture()
            if pSt then
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return DefWindowProc(hWnd, nMsg, wParam, lParam)

        case WM_MOUSEMOVE
            if pSt then
                '' TrackMouseEvent is ONE-SHOT. Re-armed after every leave, or
                '' the widget under the pointer never hears that it left -- and a
                '' hover highlight stays lit after the mouse has gone.
                if pSt->bTracking = false then
                    dim as TRACKMOUSEEVENT tme
                    tme.cbSize = sizeof(TRACKMOUSEEVENT)
                    tme.dwFlags = TME_LEAVE
                    tme.hwndTrack = hWnd
                    TrackMouseEvent(@tme)
                    pSt->bTracking = true
                end if
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

        case WM_MOUSELEAVE
            if pSt then
                pSt->bTracking = false
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

        case WM_MOUSEWHEEL
            '' WM_MOUSEWHEEL CARRIES SCREEN COORDINATES. Every other mouse
            '' message in this list is client-relative, and this one is not --
            '' Win32 sends it to the focused window wherever the pointer is. The
            '' bridge's translation is a pure function of (msg, wParam, lParam)
            '' and cannot know which it was handed, so the conversion has to
            '' happen HERE, before forwarding.
            ''
            '' Left alone it does not fail visibly: the wheel still scrolls,
            '' because Scintilla scrolls the view rather than whatever is under
            '' the pointer. It would matter the moment a widget wants the wheel
            '' position -- and by then nobody would connect the two.
            if pSt then
                dim as POINT pt
                pt.x = cshort(LOWORD(lParam))
                pt.y = cshort(HIWORD(lParam))
                ScreenToClient(hWnd, @pt)
                dim as LPARAM lpClient = (pt.x and &hFFFF) or (cast(LPARAM, pt.y and &hFFFF) shl 16)
                pSt->bridge.HandleMessage(nMsg, wParam, lpClient, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

        case WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_CHAR
            if pSt then
                if pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount()) then
                    SciHost_FlushDamage(pSt)
                    return 0
                end if
                SciHost_FlushDamage(pSt)
            end if
            '' NOT consumed -- so it goes to DefWindowProc, which is what turns
            '' an unhandled WM_KEYDOWN into a WM_CHAR. Swallowing it here would
            '' stop text input reaching the editor at all.
            return DefWindowProc(hWnd, nMsg, wParam, lParam)
    end select

    return DefWindowProc(hWnd, nMsg, wParam, lParam)
end function

'' ---------------------------------------------------------------------------
'' Registration and creation.
'' ---------------------------------------------------------------------------
function SciHost_Register() as boolean
    static as boolean bDone
    if bDone then return true

    dim as WNDCLASSEX wc
    wc.cbSize        = sizeof(WNDCLASSEX)
    '' NO CS_DBLCLKS. The toolkit synthesises the click count in shared code
    '' from timestamp and distance, so both platforms agree exactly -- asking
    '' Win32 for it would mean the two disagreed on Linux. The bridge reports 1
    '' click even for WM_LBUTTONDBLCLK for the same reason.
    wc.style         = CS_HREDRAW or CS_VREDRAW
    wc.lpfnWndProc   = @SciHost_WndProc
    wc.hInstance     = GetModuleHandle(null)
    wc.hCursor       = LoadCursor(null, IDC_IBEAM)
    '' NULL background: WM_ERASEBKGND returns 1 and the DIB covers everything.
    wc.hbrBackground = null
    wc.lpszClassName = @TIKO_SCIHOST_CLASS

    bDone = (RegisterClassEx(@wc) <> 0)
    return bDone
end function

function SciHost_Create(byval hParent as HWND, byval nId as long, _
                        byval nStyle as long) as HWND
    if SciHost_Register() = false then return 0
    return CreateWindowEx( 0, TIKO_SCIHOST_CLASS, "", _
                           nStyle, _
                           0, 0, 0, 0, hParent, _
                           cast(HMENU, cast(LONG_PTR, nId)), GetModuleHandle(null), null )
end function
