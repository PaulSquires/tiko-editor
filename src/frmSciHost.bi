'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' frmSciHost -- the "tikoSciHost" window class: a PsSciView wearing an HWND.
''
'' Phase 7d. This replaces CreateWindowEx(0, "Scintilla", ...) at four sites.
'' From the outside it is still a child window that answers SCI_* messages and
'' sends WM_NOTIFY to its parent. Inside, there is no Scintilla window at all --
'' there is a PsSurface holding a PsSciView, painted with Blend2D and driven
'' through PsPlatform's Win32 host bridge.
''
'' ---- WHY A WINDOW CLASS AND NOT 212 EDITS --------------------------------
''
''     #Define SciExec(h, m, w, l) SendMessage(h, m, w, CAST(LPARAM, l))
''
'' Every one of tiko's ~212 external editor calls is an SCI_* message sent to an
'' HWND. Routing `nMsg >= SCI_START` to SciPs_Send therefore carries ALL of them
'' with no call site edited -- and the same is true of the ~90 hWndActiveScintilla
'' sites and the 142 hWindow uses, which keep working because there is still a
'' real HWND. The measured work of 7d is four CreateWindowEx calls.
''
'' ---- WHY ALL FOUR SITES AT ONCE ------------------------------------------
''
'' modFindProject.inc takes a Scintilla Document* out of the main editor with
'' SCI_GETDOCPOINTER and hands it to a Find in Project excerpt view with
'' SCI_SETDOCPOINTER. A document from the VENDORED FORK cannot be given to the
'' STOCK Scintilla DLL -- different code, different vtables, different heap.
'' It would not fail loudly; it would corrupt. So clsDocument's two windows,
'' the FIP pool and the FIP scratch window convert together or not at all.
''
'' ---- PsC. EVERYWHERE, AND NEVER `using PsC` ------------------------------
''
'' PsCore lives in `namespace PsC` because its DWSTRING is NOT AfxNova's: one
'' EXTENDS WSTRING and is NUL-terminated, the other is an explicit UShort buffer
'' whose LENGTH is authoritative. A `using PsC` here would pull PsCore's type
'' into global scope and silently rebind tiko's ~1400 DWSTRING sites to it.
'' The prefix is verbose on purpose.
'' ===========================================================================

#pragma once

'' The window class name. Deliberately NOT "Scintilla": both must be able to
'' exist in one process during the conversion, and a class name collision would
'' be reported as nothing more useful than a failed CreateWindowEx.
'' wstr(), not a narrow literal. tiko is a UNICODE build, so WNDCLASSEX and
'' CreateWindowEx want LPCWSTR -- a plain "tikoSciHost" here is a zstring ptr
'' and produces only a "Suspicious pointer assignment" warning at the
'' RegisterClassEx, after which the class registers under a garbled name and
'' every CreateWindowEx for it fails.
#define TIKO_SCIHOST_CLASS  wstr("tikoSciHost")

type SciHostState
    hWnd     as HWND
    nId      as long                 '' the control id, echoed in every WM_NOTIFY

    surf     as PsC.PsSurface
    pView    as PsC.PsSciView ptr    '' owned by surf's tree, NOT deleted here
    bridge   as PsC.PsWin32Host
    pnt      as PsC.PsBufferPaint
    sink     as PsC.PsSciNotifySink

    '' lptext must stay alive for the duration of the SendMessage that carries
    '' it. PsNotifyEvent hands over a DWSTRING (UTF-16, a copy); tiko's
    '' SCNotification wants a UTF-8 zstring ptr. This member is where the
    '' converted bytes live while the parent reads them -- a local would be
    '' destroyed before the parent ever looked.
    sNotifyUtf8 as string

    '' TrackMouseEvent is one-shot: it must be re-armed after every WM_MOUSELEAVE
    '' or the widget under the pointer never hears that the pointer left.
    bTracking   as boolean

    '' The clipboard text handed back to Scintilla on a paste. IT MUST OUTLIVE
    '' THE CALL and a local would not: the shim copies the pointer's contents
    '' during the callback, so a buffer built on the stack is freed underneath
    '' it. One per window rather than one global, because two editors can be
    '' asked to paste from two different message loops.
    sClipUtf8   as string
end type

declare function SciHost_Register() as boolean
'' The default style is clsDocument's. Find in Project wants WS_CLIPSIBLINGS and
'' no WS_TABSTOP, and the scratch window wants nothing but WS_CHILD -- so the
'' style is a parameter rather than baked in, exactly as it was when these were
'' four separate CreateWindowEx calls.
declare function SciHost_Create(byval hParent as HWND, byval nId as long, _
                                byval nStyle as long = _
                                    WS_CHILD or WS_TABSTOP or WS_CLIPCHILDREN) as HWND
declare function SciHost_StateFromHwnd(byval hWnd as HWND) as SciHostState ptr

'' The direct pointer, replacing SendMessage(h, SCI_GETDIRECTPOINTER, 0, 0).
'' Same opaque `any ptr` the 350 SciMsg sites already pass around -- they do not
'' care that it now comes from SciPs_Create.
declare function SciHost_DirectPointer(byval hWnd as HWND) as any ptr

'' Set once, before any editor window is created.
declare sub SciHost_SetFontPath(byref sPath as string)
'' A font FAMILY name -> the font FILE FreeType must open. See the body.
declare function SciHost_ResolveFontFile(byref wszFamily as const wstring) as string
'' Env-gated (TIKO_FONTFILE_SELFTEST=1).
declare sub SciHost_RunFontFileSelfTest()
