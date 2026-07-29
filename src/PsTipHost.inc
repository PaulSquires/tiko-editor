' ########################################################################################
'  PsTipHost implementation. See PsTipHost.bi for what this is and what it deliberately is
'  not.
' ########################################################################################

#pragma once

#include once "PsTipHost.bi"
#include once "PsTooltip.inc"

using AfxNova


' ========================================================================================
' Build the comctl32 tool. Lifted unchanged from the twelve identical copies in the family
' (PsButton_AddTooltip and its siblings) so that PSTIP_MODE_SYSTEM is byte-for-byte the
' behaviour those controls have today -- including LPSTR_TEXTCALLBACK, which is why every
' control keeps its own TTN_GETDISPINFOW arm.
'
' TTF_SUBCLASS is retained here on purpose even though it is one of the reasons PsTooltip
' exists (it never sees the mouse when the real input window is a descendant, and it
' aggravates the unreliable WM_MOUSELEAVE this family carries poll timers for). This arm is
' the COMPATIBILITY path; changing it would make the default mode something other than what
' the controls ship today, which is the one property that makes this whole change safe to
' land on thirteen controls at once.
' ========================================================================================
private function PsTipHost_AddSystemTool( byval hCtrl as HWND, byval bTracked as boolean = false ) as HWND
    if IsWindow(hCtrl) = 0 then return 0

    ' The TRACKED flavour is what PsCalendar and PsDatePicker built by hand: owned by the
    ' control, TTS_NOPREFIX (a date can contain an ampersand), and the tool carries TTF_TRACK
    ' or TTF_ABSOLUTE with text PUSHED through TTM_UPDATETIPTEXTW rather than pulled through
    ' LPSTR_TEXTCALLBACK. Reproduced exactly, because PSTIP_MODE_SYSTEM has to be what those
    ' controls already ship.
    dim as HWND hTooltip
    if bTracked then
        hTooltip = CreateWindowExW( WS_EX_TOPMOST, "tooltips_class32", "", _
                                    WS_POPUP or TTS_NOPREFIX or TTS_ALWAYSTIP, _
                                    0, 0, 0, 0, hCtrl, NULL, GetModuleHandleW(NULL), NULL )
    else
        hTooltip = CreateWindowExW( 0, "tooltips_class32", "", _
                                    WS_POPUP or TTS_ALWAYSTIP, _
                                    0, 0, 0, 0, NULL, NULL, NULL, NULL )
    end if
    if hTooltip = 0 then return 0
    SetWindowPos( hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE or SWP_NOACTIVATE )

    if bTracked then
        dim tiT as TOOLINFOW
        tiT.cbSize   = sizeof(TOOLINFOW)
        tiT.uFlags   = TTF_TRACK or TTF_ABSOLUTE or TTF_IDISHWND
        tiT.hwnd     = hCtrl
        tiT.uId      = cast(UINT_PTR, hCtrl)
        tiT.lpszText = @wstr("")
        SendMessageW( hTooltip, TTM_ADDTOOLW, 0, cast(LPARAM, @tiT) )
        return hTooltip
    end if

    dim tti as TOOLINFOW
    ' cbSize must match the running comctl32 version (see AfxNova AfxAddTooltip).
    #ifdef __FB_64BIT__
        if AfxComCtlVersion < 600 then tti.cbSize = 64 else tti.cbSize = 72
    #else
        if AfxComCtlVersion < 600 then tti.cbSize = 44 else tti.cbSize = 48
    #endif
    tti.uFlags   = TTF_IDISHWND or TTF_SUBCLASS
    tti.hwnd     = hCtrl
    tti.uId      = cast(UINT_PTR, hCtrl)
    tti.hinst    = GetModuleHandleW(NULL)
    tti.lpszText = LPSTR_TEXTCALLBACK
    SendMessageW( hTooltip, TTM_ADDTOOLW, 0, cast(LPARAM, @tti) )
    return hTooltip
end function


' Push whatever delays the host has asked for at the live backend. Called after every create
' and after every mode switch, so the two backends cannot disagree about the hover time.
'
' A stored -1 is left alone rather than pushed: the backend's own derivation (comctl32's
' internal default, or PsTooltip_Create's GetDoubleClickTime arithmetic) is the right answer
' when the host has expressed no opinion, and both derive the SAME numbers from the same
' system setting.
private sub PsTipHost_ApplyDelays( byval pHost as PSTIPHOST ptr )
    if pHost = 0 then exit sub

    if pHost->nMode = PSTIP_MODE_PS then
        if pHost->hPsTip = 0 then exit sub
        if pHost->nInitialMs >= 0 then PsTooltip_SetInitialDelay( pHost->hPsTip, pHost->nInitialMs )
        if pHost->nAutoPopMs >= 0 then PsTooltip_SetAutoPopDelay( pHost->hPsTip, pHost->nAutoPopMs )
        if pHost->nReshowMs  >= 0 then PsTooltip_SetReshowDelay( pHost->hPsTip, pHost->nReshowMs )
    else
        if pHost->hSysTip = 0 then exit sub
        if pHost->nInitialMs >= 0 then
            SendMessageW( pHost->hSysTip, TTM_SETDELAYTIME, TTDT_INITIAL, cast(LPARAM, pHost->nInitialMs) )
        end if
        if pHost->nAutoPopMs >= 0 then
            SendMessageW( pHost->hSysTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, cast(LPARAM, pHost->nAutoPopMs) )
        end if
        if pHost->nReshowMs >= 0 then
            SendMessageW( pHost->hSysTip, TTM_SETDELAYTIME, TTDT_RESHOW, cast(LPARAM, pHost->nReshowMs) )
        end if
    end if
end sub


' ========================================================================================
' PUBLIC API -- lifecycle
' ========================================================================================
function PsTipHost_Ensure( byval pHost as PSTIPHOST ptr, byval hCtrl as HWND, _
                           byval TipCallback as TIP_TooltipCallbackFunc = 0 ) as boolean
    if pHost = 0 then return false
    if IsWindow(hCtrl) = 0 then return false

    pHost->hCtrl = hCtrl
    ' A caller passing 0 is saying "keep what you were given", not "clear it" -- Ensure is
    ' called from setters as well as from Create, and only Create knows the callback.
    if TipCallback <> 0 then pHost->TipCallback = TipCallback

    if pHost->nMode = PSTIP_MODE_PS then
        if IsWindow(pHost->hPsTip) = 0 then
            ' Create ALSO attaches, and applies whatever the host armed process-wide. That
            ' is what lets a control's lazily-built tip carry the host's theme without the
            ' host ever seeing this HWND.
            pHost->hPsTip = PsTooltip_Create( hCtrl )
            if pHost->hPsTip = 0 then return false
            if pHost->TipCallback <> 0 then
                PsTooltip_SetTooltipCallback( pHost->hPsTip, pHost->TipCallback )
            end if
            PsTipHost_ApplyDelays( pHost )
        end if
        return true
    end if

    if IsWindow(pHost->hSysTip) = 0 then
        pHost->hSysTip = PsTipHost_AddSystemTool( hCtrl, pHost->bTracked )
        if pHost->hSysTip = 0 then return false
        PsTipHost_ApplyDelays( pHost )
    end if
    return true
end function


sub PsTipHost_Destroy( byval pHost as PSTIPHOST ptr )
    if pHost = 0 then exit sub
    ' Both, unconditionally of nMode. A switch that failed half way -- PsTooltip_Create
    ' returning 0 on a control whose top-level window is already going down -- can leave the
    ' outgoing handle behind, and the control's WM_NCDESTROY is the last chance either has.
    if IsWindow(pHost->hSysTip) then DestroyWindow( pHost->hSysTip )
    if IsWindow(pHost->hPsTip)  then DestroyWindow( pHost->hPsTip )
    pHost->hSysTip = 0
    pHost->hPsTip  = 0
end sub


' ========================================================================================
' PUBLIC API -- mode
' ========================================================================================
function PsTipHost_SetMode( byval pHost as PSTIPHOST ptr, byval nMode as long ) as boolean
    if pHost = 0 then return false
    if (nMode <> PSTIP_MODE_SYSTEM) andalso (nMode <> PSTIP_MODE_PS) then return false
    if pHost->nMode = nMode then return true

    ' Take down the outgoing backend before the mode changes, or Destroy below would be
    ' asked to tear down the one we are about to build.
    if IsWindow(pHost->hSysTip) then DestroyWindow( pHost->hSysTip )
    if IsWindow(pHost->hPsTip)  then DestroyWindow( pHost->hPsTip )
    pHost->hSysTip = 0
    pHost->hPsTip  = 0

    pHost->nMode = nMode

    ' Rebuild ONLY if the control already had a tip -- i.e. if hCtrl was ever set. A control
    ' whose host switches mode before the first Ensure must still get its tip lazily, on the
    ' same schedule it would have had.
    if IsWindow(pHost->hCtrl) then return PsTipHost_Ensure( pHost, pHost->hCtrl )
    return true
end function


function PsTipHost_GetMode( byval pHost as PSTIPHOST ptr ) as long
    if pHost = 0 then return PSTIP_MODE_SYSTEM
    return pHost->nMode
end function


function PsTipHost_SysHandle( byval pHost as PSTIPHOST ptr ) as HWND
    if pHost = 0 then return 0
    return pHost->hSysTip
end function


function PsTipHost_PsHandle( byval pHost as PSTIPHOST ptr ) as HWND
    if pHost = 0 then return 0
    return pHost->hPsTip
end function


' ========================================================================================
' PUBLIC API -- per-item and manual take-down
' ========================================================================================
sub PsTipHost_SetToolIndex( byval pHost as PSTIPHOST ptr, byval idx as long )
    if pHost = 0 then exit sub
    if pHost->nMode = PSTIP_MODE_PS then
        if pHost->hPsTip then PsTooltip_SetToolIndex( pHost->hPsTip, idx )
    else
        ' comctl32 has no notion of the item; TTM_POP is how the family tells it the text it
        ' cached is stale, and the next dwell re-asks through TTN_GETDISPINFOW.
        if pHost->hSysTip then SendMessageW( pHost->hSysTip, TTM_POP, 0, 0 )
    end if
end sub


sub PsTipHost_Pop( byval pHost as PSTIPHOST ptr )
    if pHost = 0 then exit sub
    if pHost->nMode = PSTIP_MODE_PS then
        if pHost->hPsTip then PsTooltip_Hide( pHost->hPsTip )
    else
        if pHost->hSysTip then SendMessageW( pHost->hSysTip, TTM_POP, 0, 0 )
    end if
end sub


' ========================================================================================
' PUBLIC API -- tracked tips
'
' The control decides when, what and where; the dwell plays no part. See the .bi header for
' why the two backends are handed different things.
' ========================================================================================
sub PsTipHost_SetTracked( byval pHost as PSTIPHOST ptr, byval bTracked as boolean )
    if pHost = 0 then exit sub
    pHost->bTracked = bTracked
end sub


function PsTipHost_IsTracked( byval pHost as PSTIPHOST ptr ) as boolean
    if pHost = 0 then return false
    return pHost->bTracked
end function


sub PsTipHost_TrackShow( byval pHost as PSTIPHOST ptr, byval ptScreen as POINT, _
                         byval rcAnchorScreen as RECT, byval wszText as DWSTRING )
    if pHost = 0 then exit sub
    if len(wszText) = 0 then
        PsTipHost_TrackHide( pHost )
        exit sub
    end if

    if pHost->nMode = PSTIP_MODE_PS then
        if pHost->hPsTip = 0 then exit sub
        ' Authored text, not the callback: a tracked control has ALREADY resolved what this
        ' tip says (it had to, in order to decide whether to show one at all), so asking again
        ' through the callback would resolve it twice per cell.
        PsTooltip_SetText( pHost->hPsTip, wszText )
        PsTooltip_ShowForRect( pHost->hPsTip, rcAnchorScreen, TIP_ALIGN_BELOW )
        exit sub
    end if

    if pHost->hSysTip = 0 then exit sub
    dim ti as TOOLINFOW
    ti.cbSize   = sizeof(TOOLINFOW)
    ti.hwnd     = pHost->hCtrl
    ti.uId      = cast(UINT_PTR, pHost->hCtrl)
    ti.lpszText = cast(WSTRING ptr, wszText.vptr)
    SendMessageW( pHost->hSysTip, TTM_UPDATETIPTEXTW, 0, cast(LPARAM, @ti) )
    SendMessageW( pHost->hSysTip, TTM_TRACKPOSITION, 0, _
                  cast(LPARAM, MAKELONG(ptScreen.x, ptScreen.y)) )
    SendMessageW( pHost->hSysTip, TTM_TRACKACTIVATE, TRUE, cast(LPARAM, @ti) )
end sub


sub PsTipHost_TrackHide( byval pHost as PSTIPHOST ptr )
    if pHost = 0 then exit sub
    if pHost->nMode = PSTIP_MODE_PS then
        if pHost->hPsTip then PsTooltip_Hide( pHost->hPsTip )
        exit sub
    end if
    if pHost->hSysTip = 0 then exit sub
    dim ti as TOOLINFOW
    ti.cbSize = sizeof(TOOLINFOW)
    ti.hwnd   = pHost->hCtrl
    ti.uId    = cast(UINT_PTR, pHost->hCtrl)
    SendMessageW( pHost->hSysTip, TTM_TRACKACTIVATE, FALSE, cast(LPARAM, @ti) )
end sub


' ========================================================================================
' PUBLIC API -- timing
' ========================================================================================
sub PsTipHost_SetDelay( byval pHost as PSTIPHOST ptr, byval nWhich as long, byval nMs as long )
    if pHost = 0 then exit sub
    if nMs < 0 then nMs = 0

    ' Stored FIRST, then pushed, so the value survives a mode switch that happens later. A
    ' setter that only pushed at the live backend would silently lose the host's hover time
    ' the moment the mode changed -- and the tip would still appear, just on the wrong beat.
    select case nWhich
        case TTDT_INITIAL  : pHost->nInitialMs = nMs
        case TTDT_AUTOPOP  : pHost->nAutoPopMs = nMs
        case TTDT_RESHOW   : pHost->nReshowMs  = nMs
        case else          : exit sub
    end select

    PsTipHost_ApplyDelays( pHost )
end sub


function PsTipHost_GetDelay( byval pHost as PSTIPHOST ptr, byval nWhich as long ) as long
    if pHost = 0 then return -1
    select case nWhich
        case TTDT_INITIAL  : return pHost->nInitialMs
        case TTDT_AUTOPOP  : return pHost->nAutoPopMs
        case TTDT_RESHOW   : return pHost->nReshowMs
    end select
    return -1
end function
