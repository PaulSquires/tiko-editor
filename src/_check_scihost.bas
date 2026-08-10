'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' _check_scihost -- can tiko host a PsSciView AT ALL?
''
'' Phase 7d replaces CreateWindowEx(0, "Scintilla", ...) with a PsSciView driven
'' through PsPlatform's Win32 host bridge. Before writing the window class that
'' does it, three things had to be true, and none of them was obvious:
''
''   1. tiko must not have to link SDL3. The bridge exists precisely so the host
''      keeps its own window and message loop; if SDL3 came along anyway the
''      bridge would have bought nothing.
''
''   2. PsSciView must compile with windows.bi AND AfxNova already in scope.
''      Those two DWSTRINGs are different types -- AfxNova's EXTENDS WSTRING and
''      is NUL-terminated, PsCore's is an explicit UShort buffer whose LENGTH is
''      authoritative -- and both must survive in one translation unit.
''
''   3. AfxNova's DWSTRING must still work afterwards. tiko has ~1400 sites
''      using it; a namespace trick that quietly rebound them would be far worse
''      than a compile error.
''
'' ---- WHAT THIS PROBE ALREADY CAUGHT ---------------------------------------
''
'' THE C BINDINGS CANNOT GO INSIDE THE NAMESPACE. fbc mangles an `extern "C"`
'' block declared inside `namespace PsC` as PSC::bl_context_save, which matches
'' nothing in libblend2d -- 5 undefined references, at link time only. This is
'' why `namespace PsC` solved the encoder and does NOT simply solve the
'' renderer: PsEncoding is pure FreeBASIC and PsSciView is not. The bindings are
'' hoisted to global scope below, and `#include once` makes the inner includes
'' no-ops.
''
'' THE BRIDGE COULD NOT HAND-DECLARE ITS OWN GDI IMPORTS. PsWin32Host declared
'' CreateCompatibleDC and friends itself, to avoid dragging windows.bi around.
'' Against a host that already has windows.bi that emits the same C symbol with
'' a different type and gcc rejects the pair. Fixed on the PsPlatform side by
'' including windows.bi there, which costs nothing: the bridge lives under
'' src/platform/, which is where the isolation ratchet allows Win32.
''
'' Neither would have been found by reading. Both were found in about a minute
'' by trying to compile it.
''
'' Build: _check_scihost.bat. Exit code is the verdict.
'' ===========================================================================

'' #define UNICODE BEFORE windows.bi, exactly as tiko.bas:21 does. Without it
'' this probe gets the ANSI WNDCLASSEX/CreateWindowEx while tiko gets the wide
'' ones -- and the gate would then be testing a configuration that does not
'' exist. It caught the class-name literal only because the two were made to
'' match; a gate that quietly differs from its subject is worth nothing.
#define UNICODE
#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"

'' THE C BINDINGS GO OUTSIDE THE NAMESPACE. fbc mangles an `extern "C"` block
'' inside a namespace as PSC::bl_context_save, which matches nothing in
'' libblend2d -- so the whole Blend2D and Scintilla import surface must be at
'' global scope. `#include once` then makes the inner includes no-ops.
''
'' This is why namespace PsC solved the encoder but does not solve the renderer:
'' PsEncoding is pure FreeBASIC, and PsSciView is not.
'' modScintilla.bi FIRST, and the order is load-bearing. tiko #Defines all 117
'' SCI_* constants (and 19 SCK_*); PsPlatform declares them as `const` behind
'' #ifndef guards. Guards only work in this direction -- with PsScintilla.bi
'' first the const already exists and tiko's #Define becomes the duplicate,
'' which no guard on the library side can prevent.
#include once "app/modScintilla.bi"
#include once "bind/Blend2D.bi"
#include once "bind/FreeType.bi"
#include once "bind/HarfBuzz.bi"
#include once "scintilla/PsScintilla.bi"

namespace PsC
    #include once "crt/stddef.bi"
    #include once "core/DWString.inc"
    #include once "ui/core/PsDispatch.inc"
    #include once "ui/core/PsPaintWalk.inc"
    #include once "scintilla/PsTextEngineC.inc"
    #include once "scintilla/PsSciView.inc"
    #include once "scintilla/PsSciNotify.inc"
    #include once "platform/win32host/PsWin32Host.inc"
end namespace

#include once "frmSciHost.bi"
#include once "frmSciHost.inc"

dim shared as long g_nBad

sub Ck(byref sWhat as string, byval bOk as boolean, byref sDetail as string = "")
    if bOk then
        print "  ok      " & sWhat & iif(len(sDetail), "  = " & sDetail, "")
    else
        print "  FAIL    " & sWhat & iif(len(sDetail), "  got " & sDetail, "")
        g_nBad += 1
    end if
end sub

print ""
print "=== can tiko host a PsSciView? ==============================="
print ""

'' ---- 3: AfxNova is UNDISTURBED -------------------------------------------
'' Asserted FIRST and by LENGTH rather than by printing it. tiko has ~1400
'' DWSTRING sites, and a namespace trick that silently rebound them to PsCore's
'' type -- whose length is authoritative and whose buffer is not NUL-terminated
'' -- would corrupt far more than it fixed. `print` would not have shown it.
dim as DWSTRING sAfx = "abcde"
Ck("AfxNova's DWSTRING still resolves to AfxNova's", len(sAfx) = 5, str(len(sAfx)))
sAfx = sAfx & "fg"
Ck("...and still concatenates", len(sAfx) = 7, str(len(sAfx)))

'' ---- 2: BOTH DWSTRINGs in one translation unit ---------------------------
dim as PsC.DWSTRING sPs
sPs.Utf8 = "abcde"
Ck("PsCore's DWSTRING coexists with it", sPs.Length = 5, str(sPs.Length))
Ck("...and they are genuinely different types", sPs.Utf8 = "abcde", sPs.Utf8)

'' ---- 1: the whole render stack, with no SDL3 -----------------------------
'' A REAL EDITOR, not just a linked one. Creating the view initialises Blend2D,
'' FreeType, HarfBuzz and the vendored Scintilla fork; if any of them were
'' missing or mis-linked this is where it shows, rather than at run time inside
'' tiko.
dim as string sFont = environ("WINDIR") & "\Fonts\consola.ttf"
Ck("a font to measure with exists", len(dir(sFont)) > 0, sFont)

PsC.PsRenderInit()
PsC.PsTextEngineInstallApi()

dim as PsC.PsTextEngine te
Ck("the text engine opens the font", PsC.TE_Init(te, strptr(sFont), 15) <> 0, "")

dim as PsC.PsSurface surf
surf.pText = cptr(PsC.PsTextEngine_ ptr, @te)
dim as PsC.PsWidget ptr root = new PsC.PsWidget
surf.SetRoot(root)

dim as PsC.PsSciView ptr pv = new PsC.PsSciView()
Ck("PsSciView creates inside tiko's translation unit", _
   pv->Create(sFont, 400, 300), "")
root->AddChild(pv)

'' The 801 SciMsg sites survive the port only if a message sent to the fork
'' behaves exactly as one sent to the DLL. Round-tripping text through
'' SCI_SETTEXT and SCI_GETLENGTH is the smallest thing that proves the message
'' path is live at all.
dim as string sText = "line one" & chr(10) & "line two"
pv->Msg(SCI_SETTEXT, 0, cast(integer, strptr(sText)))
Ck("SCI_SETTEXT / SCI_GETLENGTH round-trips", _
   pv->Msg(SCI_GETLENGTH) = len(sText), str(pv->Msg(SCI_GETLENGTH)))
Ck("...and the line count is right", pv->Msg(SCI_GETLINECOUNT) = 2, _
   str(pv->Msg(SCI_GETLINECOUNT)))

'' The bridge itself, constructed in anger.
dim as PsC.PsWin32Host bridge
Ck("the Win32 bridge constructs", bridge.Attach(@surf, cast(any ptr, 1)) <> 0, "")
bridge.Detach()


'' ---- the window class, driven exactly as tiko drives it ------------------
''
'' Everything above proves the pieces link. This proves the CLASS works, and it
'' is driven the way the 212 call sites do it -- SendMessage of an SCI_* message
'' to an HWND -- rather than by calling PsSciView directly, which would test a
'' path no caller uses.

dim shared as long g_nNotify
dim shared as HWND g_hNotifyFrom
dim shared as long g_nNotifyId

function ParentProc stdcall (byval hWnd as HWND, byval nMsg as UINT, _
                           byval wParam as WPARAM, byval lParam as LPARAM) as LRESULT
    if nMsg = WM_NOTIFY then
        dim as NMHDR ptr p = cast(NMHDR ptr, lParam)
        g_nNotify += 1
        g_hNotifyFrom = p->hwndFrom
        g_nNotifyId = clng(p->idFrom)
    end if
    return DefWindowProc(hWnd, nMsg, wParam, lParam)
end function

dim as WNDCLASSEX wcp
wcp.cbSize = sizeof(WNDCLASSEX)
wcp.lpfnWndProc = @ParentProc
wcp.hInstance = GetModuleHandle(null)
wcp.lpszClassName = @wstr("tikoSciHostProbeParent")
RegisterClassEx(@wcp)

dim as HWND hParent = CreateWindowEx(0, wstr("tikoSciHostProbeParent"), "", WS_OVERLAPPED, _
                                     0, 0, 400, 300, null, null, GetModuleHandle(null), null)
Ck("a parent window to receive WM_NOTIFY", hParent <> 0, "")

'' 4242 rather than IDC_SCINTILLA: an id echoed back correctly by accident is
'' not echoed back correctly. tiko gates on IsValidScintillaID(id), so a host
'' that reported the wrong id would drop every notification silently.
const PROBE_ID = 4242
dim as HWND hSci = SciHost_Create(hParent, PROBE_ID)
Ck("SciHost_Create returns a window", hSci <> 0, "")
scope
    '' ASKED OF THE WINDOW, not assumed. The first version of this line was
    '' `len(dir("")) >= 0`, which is true for every input and therefore asserted
    '' nothing at all -- a green tick that could never go red.
    '' WSTRING: tiko is a Unicode build, so this is GetClassNameW and a zstring
    '' buffer would come back as the first byte of each wide character.
    dim as wstring * 64 zCls
    GetClassName(hSci, @zCls, 64)
    Ck("...of class tikoSciHost", zCls = TIKO_SCIHOST_CLASS, zCls)
end scope

'' THE BRANCH THAT IS 7d: SCI_* by SendMessage, exactly as SciExec does it.
dim as string sProbe = "alpha" & chr(10) & "beta" & chr(10) & "gamma"
SendMessage(hSci, SCI_SETTEXT, 0, cast(LPARAM, strptr(sProbe)))
Ck("SciExec-shaped SCI_SETTEXT reaches the fork", _
   SendMessage(hSci, SCI_GETLENGTH, 0, 0) = len(sProbe), _
   str(SendMessage(hSci, SCI_GETLENGTH, 0, 0)))
Ck("...and the line count is right", _
   SendMessage(hSci, SCI_GETLINECOUNT, 0, 0) = 3, _
   str(SendMessage(hSci, SCI_GETLINECOUNT, 0, 0)))

'' The direct pointer the 350 SciMsg sites use. It must be the SAME editor the
'' HWND route just talked to -- two editors behind one window would diverge
'' silently, and only under a split.
dim as any ptr pProbe = SciHost_DirectPointer(hSci)
Ck("SciHost_DirectPointer is non-null", pProbe <> 0, "")
Ck("...and addresses the same editor as the HWND route", _
   SciPs_Send(pProbe, SCI_GETLENGTH, 0, 0) = len(sProbe), _
   str(SciPs_Send(pProbe, SCI_GETLENGTH, 0, 0)))

'' PSEV_NOTIFY -> WM_NOTIFY. tiko resolves the document from hwndFrom and gates
'' on the id; both wrong is not a crash, it is an edit attributed to nothing.
Ck("the edit produced WM_NOTIFY at the parent", g_nNotify > 0, str(g_nNotify))
Ck("...with hwndFrom = the host window", g_hNotifyFrom = hSci, "")
Ck("...and idFrom = the control id", g_nNotifyId = PROBE_ID, str(g_nNotifyId))

'' Sizing, which clsDocument does after creating both splits at 0,0,0,0.
SetWindowPos(hSci, null, 0, 0, 320, 240, SWP_NOZORDER or SWP_NOACTIVATE)
Ck("resizing does not destroy the editor", _
   SendMessage(hSci, SCI_GETLENGTH, 0, 0) = len(sProbe), "")

'' PER-WINDOW STATE, not global. clsDocument creates TWO of these per document
'' and Find in Project creates sixteen more, all in one process -- state that
'' leaked between them would show up as the split view editing the wrong buffer.
scope
    dim as HWND hSci2 = SciHost_Create(hParent, PROBE_ID + 1)
    Ck("a second host window creates", hSci2 <> 0, "")
    Ck("...with its own state", _
       SciHost_StateFromHwnd(hSci2) <> SciHost_StateFromHwnd(hSci), "")
    Ck("...and its own editor", _
       SciHost_DirectPointer(hSci2) <> SciHost_DirectPointer(hSci), "")

    '' Independent documents: text set in one must not appear in the other.
    dim as string sOther = "x"
    SendMessage(hSci2, SCI_SETTEXT, 0, cast(LPARAM, strptr(sOther)))
    Ck("...whose content is independent", _
       SendMessage(hSci2, SCI_GETLENGTH, 0, 0) = 1 andalso _
       SendMessage(hSci,  SCI_GETLENGTH, 0, 0) = len(sProbe), _
       str(SendMessage(hSci, SCI_GETLENGTH, 0, 0)))
    DestroyWindow(hSci2)
end scope

'' ---- THE PIXELS ----------------------------------------------------------
''
'' "the editor pane is blank BLACK and typing seems to work but I can't see it".
'' Reasoning about that produced three plausible causes and no answer; LOOKING
'' at the buffer settles it in one run. minieditor's white page went the same
'' way and was only ever solved by dumping the picture.
''
'' The styles set here are the ones clsDocument.ApplyProperties sets, in the
'' same order: a font FACE NAME (not a path), a point size, then fore/back from
'' the theme, then SCI_STYLECLEARALL.
scope
    const CLR_BACK = &h1E1E1E     '' Scintilla colours are 0xBBGGRR
    const CLR_FORE = &hD4D4D4

    SendMessage(hSci, SCI_STYLESETFONT, STYLE_DEFAULT, cast(LPARAM, strptr("Consolas")))
    SendMessage(hSci, SCI_STYLESETSIZE, STYLE_DEFAULT, 10)
    SendMessage(hSci, SCI_STYLESETFORE, STYLE_DEFAULT, CLR_FORE)
    SendMessage(hSci, SCI_STYLESETBACK, STYLE_DEFAULT, CLR_BACK)
    SendMessage(hSci, SCI_STYLECLEARALL, 0, 0)

    dim as string sPix = "MMMMMMMMMMMMMMMM"
    SendMessage(hSci, SCI_SETTEXT, 0, cast(LPARAM, strptr(sPix)))
    SendMessage(hSci, SCI_COLOURISE, 0, -1)

    '' PAINTED THROUGH THE WIDGET, not through SciPs_PaintTo.
    ''
    '' The first version of this called SciPs_PaintTo directly and reported 3926
    '' ink pixels while the real pane was BLANK BLACK -- because calling it
    '' directly also meant calling SciPs_SetSize directly, which is the one thing
    '' the broken path never did. It measured the renderer, which was never at
    '' fault, and skipped the widget, which was.
    ''
    '' So this drives the surface exactly as WM_SIZE and WM_PAINT do: resize the
    '' host, then walk the tree into the buffer.
    const PW = 400, PH = 200
    dim as SciHostState ptr pStP = SciHost_StateFromHwnd(hSci)
    Ck("the host state is reachable", pStP <> 0, "")
    pStP->bridge.Resize(PW, PH)

    '' Bounds first, because a zero-sized widget paints nothing and reports no
    '' error while doing it -- which is precisely how this failed.
    Ck("the view has a non-zero width after resize", pStP->pView->bounds.w = PW, _
       str(pStP->pView->bounds.w))
    Ck("...and height", pStP->pView->bounds.h = PH, str(pStP->pView->bounds.h))

    dim as ulong ptr pBuf = callocate(PW * PH * 4)
    pStP->pnt.BeginFrame(pBuf, PW, PH, PW * 4)
    PsC.PsSurfacePaint(pStP->surf, pStP->pnt)
    pStP->pnt.EndFrame()
    dim as long nOk = 1

    '' THE MODE, not pixel 0. Pixel 0 is in the margin, which is a different
    '' colour from the text background -- measuring against it was wrong in
    '' minieditor and would be wrong here.
    dim as long nDistinct, nInk, nBackPix
    dim as ulong uMode
    scope
        '' Cheap mode: the most common value wins. 400x200 is small enough to
        '' do this with a linear scan over a tiny candidate list.
        dim as ulong cand(0 to 15)
        dim as long cnt(0 to 15), nc
        for i as long = 0 to PW * PH - 1
            dim as ulong c = pBuf[i] and &hFFFFFF
            dim as boolean bFound
            for j as long = 0 to nc - 1
                if cand(j) = c then cnt(j) += 1 : bFound = true : exit for
            next
            if (not bFound) andalso (nc < 16) then
                cand(nc) = c : cnt(nc) = 1 : nc += 1
            end if
        next
        nDistinct = nc
        dim as long nBest = -1
        for j as long = 0 to nc - 1
            if cnt(j) > nBest then nBest = cnt(j) : uMode = cand(j)
        next
        nBackPix = nBest
        nInk = (PW * PH) - nBest
    end scope

    Ck("the buffer is not one flat colour", nDistinct > 1, str(nDistinct) & " distinct")
    Ck("the background is the style back colour", uMode = &h1E1E1E, hex(uMode, 6))
    '' 16 M glyphs cannot cover less than a few hundred pixels. A handful would
    '' mean the caret and nothing else -- which is exactly what "typing works but
    '' I can't see it" looks like.
    Ck("there is INK on the page, not just a caret", nInk > 500, str(nInk) & " px")

    deallocate(pBuf)
end scope

'' ---- A/B AGAINST STOCK SCINTILLA -----------------------------------------
''
'' "The text is VERY small and there is no syntax coloring."
''
'' Both are answerable by MEASUREMENT rather than by eye, because tiko still
'' loads Scintilla64.dll and Lexilla64.dll -- so the thing 7d replaced can be
'' stood up in the same process, given the SAME styles, and asked the same
'' questions. That is a real A/B, not a recollection of what tiko used to look
'' like.
scope
    dim as any ptr pLibSci = dylibload( exepath() & "\..\Scintilla64.dll" )
    dim as any ptr pLibLex = dylibload( exepath() & "\..\Lexilla64.dll" )
    Ck("Scintilla64.dll loads (the A side)", pLibSci <> 0, "")
    Ck("Lexilla64.dll loads", pLibLex <> 0, "")

    if (pLibSci <> 0) andalso (pLibLex <> 0) then
        dim as CreateLexerFn pfnCreate = _
            cast(CreateLexerFn, dylibsymbol(pLibLex, "CreateLexer"))
        Ck("Lexilla exports CreateLexer", pfnCreate <> 0, "")

        '' The stock control, same parent, a DIFFERENT id.
        dim as HWND hRef = CreateWindowEx(0, wstr("Scintilla"), "", _
                                          WS_CHILD, 0, 0, 400, 200, hParent, _
                                          cast(HMENU, 5000), GetModuleHandle(null), null)
        Ck("a stock Scintilla window", hRef <> 0, "")

        '' IDENTICAL style setup on both, in clsDocument.ApplyProperties's order.
        const FSIZE = 10
        for each_ as long = 0 to 1
            dim as HWND h = iif(each_ = 0, hRef, hSci)
            SendMessage(h, SCI_STYLESETFONT, STYLE_DEFAULT, cast(LPARAM, strptr("Consolas")))
            SendMessage(h, SCI_STYLESETSIZE, STYLE_DEFAULT, FSIZE)
            SendMessage(h, SCI_STYLESETFORE, STYLE_DEFAULT, &hD4D4D4)
            SendMessage(h, SCI_STYLESETBACK, STYLE_DEFAULT, &h1E1E1E)
            SendMessage(h, SCI_STYLECLEARALL, 0, 0)
        next

        '' ---- THE SIZE, ASKED OF SCINTILLA ITSELF -------------------------
        '' SCI_TEXTHEIGHT is the line height Scintilla lays out with, which is
        '' the number that decides how big the text LOOKS. Comparing it to the
        '' stock control at the same point size turns "very small" into a ratio.
        dim as long hRefLine = SendMessage(hRef, SCI_TEXTHEIGHT, 0, 0)
        dim as long hPsLine  = SendMessage(hSci, SCI_TEXTHEIGHT, 0, 0)
        Ck("stock line height is sane", hRefLine > 0, str(hRefLine))
        '' WITHIN A PIXEL, not equal. FreeType and GDI round ascent and descent
        '' differently and always will; insisting on equality would be a test that
        '' fails for a reason nobody can fix. A pixel is the tolerance -- anything
        '' larger is a real difference in how big the text comes out.
        Ck("the fork's line height matches stock within 1px", _
           abs(hPsLine - hRefLine) <= 1, str(hPsLine) & " vs " & str(hRefLine))

        '' ---- DPI, WHICH IS WHAT "VERY SMALL" ACTUALLY WAS -----------------
        ''
        '' PlatPs hardcoded LogPixelsY to 96, and Scintilla turns points into
        '' pixels with points * LogPixelsY / 72 -- so on a DPI-aware host at 150%
        '' the editor rendered at two thirds size while every control around it
        '' scaled correctly.
        ''
        '' This gate is NOT DPI-aware, so it cannot reproduce that by running at
        '' high DPI. It asserts the MECHANISM instead: tell PlatPs 144, force the
        '' fonts to be realised again, and the line height must grow by half.
        '' Without that, "the host now calls PlatPs_SetDpi" is an unverified claim.
        scope
            PlatPs_SetDpi(144)
            SendMessage(hSci, SCI_STYLESETSIZE, STYLE_DEFAULT, FSIZE)
            SendMessage(hSci, SCI_STYLECLEARALL, 0, 0)
            dim as long hHi = SendMessage(hSci, SCI_TEXTHEIGHT, 0, 0)

            PlatPs_SetDpi(96)
            SendMessage(hSci, SCI_STYLESETSIZE, STYLE_DEFAULT, FSIZE)
            SendMessage(hSci, SCI_STYLECLEARALL, 0, 0)
            dim as long hLo = SendMessage(hSci, SCI_TEXTHEIGHT, 0, 0)

            Ck("at 144 dpi the line height grows by about half", _
               (hHi > hLo * 13 \ 10) andalso (hHi < hLo * 17 \ 10), _
               str(hHi) & " at 144 vs " & str(hLo) & " at 96")
            Ck("...and setting it back restores the old height", hLo = hPsLine, _
               str(hLo) & " vs " & str(hPsLine))
        end scope

        '' ---- THE LEXER ---------------------------------------------------
        '' Colouring means SCI_GETSTYLEAT returns something other than 0 across
        '' the text. Asserted as a COUNT OF DISTINCT STYLE BYTES, because a
        '' lexer that ran but classified everything the same way is also broken,
        '' and "not all zero" would not catch it.
        dim as string sCode = "' a comment" & chr(10) & "dim as long x = 42" & chr(10)
        for each_ as long = 0 to 1
            dim as HWND h = iif(each_ = 0, hRef, hSci)
            if pfnCreate <> 0 then
                dim as any ptr pLx = pfnCreate("tiko")
                SendMessage(h, SCI_SETILEXER, 0, cast(LPARAM, pLx))
            end if
            SendMessage(h, SCI_SETTEXT, 0, cast(LPARAM, strptr(sCode)))
            SendMessage(h, SCI_COLOURISE, 0, -1)
        next

        dim as long nRefStyles, nPsStyles
        for each_ as long = 0 to 1
            dim as HWND h = iif(each_ = 0, hRef, hSci)
            dim as long nLen = SendMessage(h, SCI_GETLENGTH, 0, 0)
            dim as long seen(0 to 255), n
            for i as long = 0 to nLen - 1
                dim as long st = SendMessage(h, SCI_GETSTYLEAT, i, 0) and &hFF
                if seen(st) = 0 then seen(st) = 1 : n += 1
            next
            if each_ = 0 then nRefStyles = n else nPsStyles = n
        next

        '' Diagnostics, printed rather than asserted: which of the three steps --
        '' accepting the lexer, running it, writing styles -- actually differs.
        print "        [dx] endStyled  stock=" & _
              str(SendMessage(hRef, SCI_GETENDSTYLED, 0, 0)) & _
              "  fork=" & str(SendMessage(hSci, SCI_GETENDSTYLED, 0, 0))
        print "        [dx] lexer id   stock=" & _
              str(SendMessage(hRef, SCI_GETLEXER, 0, 0)) & _
              "  fork=" & str(SendMessage(hSci, SCI_GETLEXER, 0, 0))
        print "        [dx] len        stock=" & _
              str(SendMessage(hRef, SCI_GETLENGTH, 0, 0)) & _
              "  fork=" & str(SendMessage(hSci, SCI_GETLENGTH, 0, 0))

        Ck("stock Scintilla styles the text with the tiko lexer", nRefStyles > 1, _
           str(nRefStyles) & " distinct styles")
        Ck("the fork styles it the SAME way", nPsStyles = nRefStyles, _
           str(nPsStyles) & " vs " & str(nRefStyles))

        DestroyWindow(hRef)
    end if
end scope

'' NOT ASSERTED: that teardown releases the state.
''
'' The obvious line -- SciHost_StateFromHwnd(hSci) = 0 after DestroyWindow -- was
'' here and was VACUOUS: GetWindowLongPtr on a destroyed HWND returns 0 whatever
'' WM_DESTROY did. It survived a mutation that deleted the SetWindowLongPtr(0)
'' entirely, which is how it was found. A green tick that cannot go red is worse
'' than no tick, so it is gone rather than reworded.
DestroyWindow(hSci)
DestroyWindow(hParent)

print ""
if g_nBad > 0 then
    print "  " & g_nBad & " failure(s) -- 7d cannot proceed on this footing."
    end 1
end if
print "  ok      tiko can host a PsSciView: no SDL3, one DWSTRING"
print ""
end 0
