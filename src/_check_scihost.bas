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

print ""
if g_nBad > 0 then
    print "  " & g_nBad & " failure(s) -- 7d cannot proceed on this footing."
    end 1
end if
print "  ok      tiko can host a PsSciView: no SDL3, both DWSTRINGs intact"
print ""
end 0
