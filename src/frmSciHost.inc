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

    dim as PsC.PsWidget ptr root = new PsC.PsWidget
    pSt->surf.SetRoot(root)

    dim as RECT rc
    GetClientRect(hWnd, @rc)
    '' A zero-sized window is the NORMAL case here: clsDocument creates both
    '' splits at 0,0,0,0 and sizes them later. Scintilla tolerates it, so this
    '' must too -- clamp rather than fail.
    dim as long cw = iif(rc.right  > 0, rc.right,  1)
    dim as long ch = iif(rc.bottom > 0, rc.bottom, 1)

    pSt->pView = new PsC.PsSciView()
    if pSt->pView = 0 then return false
    if pSt->pView->Create(g_sSciHostFont, cw, ch) = false then return false
    root->AddChild(pSt->pView)

    if pSt->bridge.Attach(@pSt->surf, hWnd) = false then return false
    pSt->bridge.Resize(cw, ch)

    PsC.PsSciNotifyAttach(pSt->sink, pSt->pView->pSci, @SciHost_OnNotify, pSt)

    '' STEADY, NOT BLINKING. There is no frame scheduler in this tree, so a
    '' blinking caret would sit at whatever phase it started in. Asking for
    '' steady makes that a decision rather than an artefact. A WM_TIMER could
    '' drive a blink later; it is not needed for correctness.
    pSt->pView->Msg(SCI_SETCARETPERIOD, 0, 0)

    return true
end function

private sub SciHost_OnDestroy(byval hWnd as HWND)
    dim as SciHostState ptr pSt = SciHost_StateFromHwnd(hWnd)
    if pSt = 0 then exit sub
    SetWindowLongPtr(hWnd, GWLP_USERDATA, 0)

    '' DETACH FIRST. Scintilla notifies during teardown as well as during
    '' editing, so a sink that outlives its registration is a use-after-free
    '' with a delay on it.
    PsC.PsSciNotifyDetach(pSt->sink)
    pSt->bridge.Detach()
    '' pView is NOT deleted here -- the surface owns its tree and destroys it.
    pSt->pView = 0
    delete pSt
end sub

'' ---------------------------------------------------------------------------
'' The window procedure.
'' ---------------------------------------------------------------------------
private function SciHost_WndProc( byval hWnd as HWND, byval nMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM _
                                  ) as LRESULT
    '' ---- SCI_* FIRST, and before the state check has any chance to matter.
    ''
    '' THIS SINGLE BRANCH IS 7d. It carries all ~212 SciExec sites, all 350
    '' SciMsg sites and everything clsDocument sends, with no call site edited.
    ''
    '' The range is safe by Scintilla's own design: ordinary window messages are
    '' below 0x0400, WM_APP is 0x8000 and registered messages are above 0xC000,
    '' so 2000-3999 belongs to nobody else.
    if (nMsg >= SCI_START) andalso (nMsg < 4000) then
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

        case WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP
            ReleaseCapture()
            if pSt then
                pSt->bridge.HandleMessage(nMsg, wParam, lParam, GetTickCount())
                SciHost_FlushDamage(pSt)
            end if
            return 0

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

function SciHost_Create(byval hParent as HWND, byval nId as long) as HWND
    if SciHost_Register() = false then return 0
    return CreateWindowEx( 0, TIKO_SCIHOST_CLASS, "", _
                           WS_CHILD or WS_TABSTOP or WS_CLIPCHILDREN, _
                           0, 0, 0, 0, hParent, _
                           cast(HMENU, cast(LONG_PTR, nId)), GetModuleHandle(null), null )
end function
