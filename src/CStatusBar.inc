
''
''  CStatusBar.inc
''   

#include once "CStatusBar.bi"


function GetStatusBarPointer( byval hStatusBarControl as HWND ) as CSTATUSBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hStatusBarControl)
    dim as CSTATUSBAR ptr pStatus = null
    if pWindow then pStatus = cast( CSTATUSBAR ptr, pWindow->UserData(0) )
    return pStatus
end function

' Which panel is the cursor over? Returns a model index, or -1 for none (the cursor is
' over empty bar area, or outside the control entirely).
private function GetHotStatusBarPanel( byval hStatusBarControl as HWND ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return -1
    dim as integer idx = -1
    for i as integer = 0 to pStatus->panelCount - 1
        if isMouseOverRECT( hStatusBarControl, pStatus->panels(i).rc ) then
            idx = i: exit for
        end if
    next
    return idx
end function


' Repaint one panel. Silently does nothing for an invalid index, which is what makes the
' hover path safe when the cursor is over empty bar area (idx = -1). FALSE = no background
' erase; the panel's rect is fully painted by the paint callback.
private sub CStatusBar_InvalidatePanel( byval pStatus as CSTATUSBAR ptr, byval idx as integer )
    if pStatus = null then exit sub
    if pStatus->IsValidPanel(idx) = false then exit sub
    InvalidateRect( pStatus->hWin, @pStatus->panels(idx).rc, FALSE )
end sub



' ========================================================================================
' Add a string (and optional itemData) to the end of the custom statusbar control.
' ========================================================================================
function CStatusBar_AddPanel( byval hStatusBarControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return -1
    dim as CSTATUSBAR_PANELINFO ptr pPanel = pStatus->AddPanel()
    if pPanel = null then return -1
    pPanel->Text = Text
    pPanel->itemData = itemData
    return pStatus->GetCount() - 1  ' model index of the appended row
end function


' ========================================================================================
' Insert a panel at the given index (existing panels shift down). Returns the
' actual index used (clamped to [0, count]), or -1 on failure.
' ========================================================================================
function CStatusBar_InsertPanel( byval hStatusBarControl as HWND, byval idx as integer, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return -1
    dim as integer at = idx
    if at < 0 then at = 0
    if at > pStatus->GetCount() then at = pStatus->GetCount()
    dim as CSTATUSBAR_PANELINFO ptr pPanel = pStatus->InsertPanelAt( at )
    if pPanel = null then return -1
    pPanel->Text = Text
    pPanel->itemData = itemData
    return at
end function


' ========================================================================================
' Delete the panel at the given index. Returns TRUE on success.
' ========================================================================================
function CStatusBar_DeletePanel( byval hStatusBarControl as HWND, byval idx as integer ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return false
    return pStatus->DeletePanelAt( idx )
end function


' ========================================================================================
' Remove all panels from the control.
' ========================================================================================
sub CStatusBar_Clear( byval hStatusBarControl as HWND )
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return
    pStatus->Clear()
end sub


' ========================================================================================
' Set specified panel text in the custom statusbar control. Returns TRUE on success,
' FALSE if the idx index is invalid.
' ========================================================================================
function CStatusBar_SetText( byval hStatusBarControl as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return false
    if pStatus->IsValidPanel(idx) then
        pStatus->GetPanel(idx)->Text = Text
        ' Text drives the panel's width, so this needs a re-layout, not just a repaint.
        pStatus->Refresh()
        return true     ' success
    end if
    return false  ' invalid panel
end function


' ========================================================================================
' Retrieve specified panel text from the custom statusbar control.
' ========================================================================================
function CStatusBar_GetText( byval hStatusBarControl as HWND, byval idx as integer ) as DWSTRING
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return ""
    if pStatus->IsValidPanel(idx) = false then return ""
    dim as CSTATUSBAR_PANELINFO ptr pPanel = pStatus->GetPanel(idx)
    dim as DWSTRING wszText = pPanel->Text
    return wszText
end function


' ========================================================================================
' Set the user defined idx painting callback for the custom statusbar control.
' ========================================================================================
sub CStatusBar_SetPaintCallback( byval hStatusBarControl as HWND, byval usersub as SB_PaintCallbackSub )
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 
    pStatus->PaintCallback = usersub
end sub


' ========================================================================================
' Set the user defined message callback for the custom statusbar control.
' ========================================================================================
sub CStatusBar_SetMessageCallback( byval hStatusBarControl as HWND, byval userfunc as SB_MessageCallbackFunc )
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return
    pStatus->MessageCallback = userfunc
end sub


' ========================================================================================
' Set the on-demand tooltip text callback. If none is set, the tooltip shows the
' hovered panel's Text. Return "" from the callback to suppress the tooltip.
' ========================================================================================
sub CStatusBar_SetTooltipCallback( byval hStatusBarControl as HWND, byval userfunc as SB_TooltipCallbackFunc )
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return
    pStatus->TooltipCallback = userfunc
end sub


' ========================================================================================
' Resolve the tooltip text for a panel: host callback if set, else panel Text.
' ========================================================================================
function CStatusBar_ResolveTooltipText( byval pStatus as CSTATUSBAR ptr, byval idx as integer ) as DWSTRING
    if (pStatus = null) orelse (pStatus->IsValidPanel(idx) = false) then return ""
    if pStatus->TooltipCallback then return pStatus->TooltipCallback( pStatus->hWin, idx )
    return pStatus->panels(idx).Text
end function


' ========================================================================================
' Set the tooltip hovertime for the custom statusbar control.
' ========================================================================================
sub CStatusBar_SetHoverTime( byval hStatusBarControl as HWND, byval milliseconds as integer )
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 
    pStatus->HoverTime = milliseconds
end sub


' ========================================================================================
' Set specified idx itemData for the custom statusbar control.
' Returns TRUE on success, FALSE if the idx index is invalid.
' ========================================================================================
function CStatusBar_SetItemData( byval hStatusBarControl as HWND, byval idx as integer, byval itemData as integer ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return false
    if pStatus->IsValidPanel(idx) then
        pStatus->GetPanel(idx)->itemData = itemData
        return true     ' success
    end if
    return false  ' invalid row
end function


' ========================================================================================
' Retrieve specified idx itemData from the custom statusbar control.
' ========================================================================================
function CStatusBar_GetItemData( byval hStatusBarControl as HWND, byval idx as integer ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    if pStatus->IsValidPanel(idx) = false then return 0
    return pStatus->GetPanel(idx)->itemData
end function


' ========================================================================================
' Retrieve the client-coordinate RECT of the specified idx panel. Returns an empty RECT
' for an invalid index.
' ========================================================================================
function CStatusBar_GetItemRect( byval hStatusBarControl as HWND, byval idx as integer ) as RECT
    dim as RECT rc
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return rc
    if pStatus->IsValidPanel(idx) = false then return rc
    return pStatus->GetPanel(idx)->rc
end function


' ========================================================================================
' Retrieve the handle of the statusbar's tooltip control.
' ========================================================================================
function CStatusBar_GetTooltipHandle( byval hStatusBarControl as HWND ) as HWND
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    return pStatus->hTooltip
end function


' ========================================================================================
' Retrieve the font used to measure panel text.
' ========================================================================================
function CStatusBar_GetFont( byval hStatusBarControl as HWND ) as HFONT
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    return pStatus->hPanelFont
end function


' ========================================================================================
' Set the font used to paint idx text. The caller owns the HFONT and is responsible for
' destroying it; the control only stores the handle.
' ========================================================================================
function CStatusBar_SetFont( byval hStatusBarControl as HWND, byval hFont as HFONT ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return false
    pStatus->hPanelFont = hFont
    ' The font is what panel widths are measured with, so changing it re-lays out.
    pStatus->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the minimum width (pixels) of the specified panel. 0 = auto-size to text.
' ========================================================================================
function CStatusBar_GetPanelMinWidth( byval hStatusBarControl as HWND, byval idx as integer ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    if pStatus->IsValidPanel(idx) = false then return 0
    return pStatus->GetPanel(idx)->nMinWidth
end function


' ========================================================================================
' Set the minimum width (pixels) of the specified panel. The panel never renders narrower
' than its text, so this is a floor, not a fixed width. Returns FALSE for a bad index.
' ========================================================================================
function CStatusBar_SetPanelMinWidth( byval hStatusBarControl as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return false
    if pStatus->IsValidPanel(idx) = false then return false
    if nMinWidth < 0 then nMinWidth = 0
    pStatus->GetPanel(idx)->nMinWidth = nMinWidth
    pStatus->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the index of the expanding (spring) panel, or -1 if none.
' ========================================================================================
function CStatusBar_GetExpandPanel( byval hStatusBarControl as HWND ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return -1
    return pStatus->expandPanel
end function


' ========================================================================================
' Nominate the single panel that expands to fill the bar's unused width. Setting one
' clears any previous spring; pass -1 to clear it entirely. Returns FALSE (leaving the
' current spring untouched) for an index that is neither valid nor -1.
' ========================================================================================
function CStatusBar_SetExpandPanel( byval hStatusBarControl as HWND, byval idx as integer ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return false
    if (idx <> -1) andalso (pStatus->IsValidPanel(idx) = false) then return false
    pStatus->expandPanel = idx
    pStatus->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the horizontal padding (pixels) added to each side of a panel's text.
' ========================================================================================
function CStatusBar_GetPadding( byval hStatusBarControl as HWND ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    return pStatus->nPadding
end function


' ========================================================================================
' Set the horizontal padding (raw pixels -- the caller DPI-scales) added to each side of a
' panel's text when measuring. Inset your paint callback's text by the same amount.
' ========================================================================================
sub CStatusBar_SetPadding( byval hStatusBarControl as HWND, byval nPadding as integer )
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then exit sub
    if nPadding < 0 then nPadding = 0
    pStatus->nPadding = nPadding
    pStatus->Refresh()
end sub


' ========================================================================================
' Retrieve the computed client-coordinate rect of the specified panel. Forces a pending
' layout so the result is always current. Returns FALSE for a bad index.
' ========================================================================================
function CStatusBar_GetPanelRect( byval hStatusBarControl as HWND, byval idx as integer, byref rc as RECT ) as boolean
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    SetRectEmpty( @rc )
    if pStatus = null then return false
    if pStatus->IsValidPanel(idx) = false then return false
    if pStatus->bLayoutDirty then pStatus->LayoutPanels()
    rc = pStatus->GetPanel(idx)->rc
    return true
end function


' ========================================================================================
' Retrieve the background color from the custom statusbar control.
' ========================================================================================
function CStatusBar_GetBackColor( byval hStatusBarControl as HWND ) as COLORREF
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    return pStatus->BackColor
end function


' ========================================================================================
' Set the background color for the custom statusbar control.
' ========================================================================================
function CStatusBar_SetBackColor( byval hStatusBarControl as HWND, byval clr as COLORREF ) as COLORREF 
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    dim as COLORREF oldClr = pStatus->BackColor
    pStatus->BackColor = clr
    return oldClr
end function


' ========================================================================================
' Retrieve the current panel count in the custom statusbar control.
' ========================================================================================
function CStatusBar_GetCount( byval hStatusBarControl as HWND ) as integer
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus = null then return 0
    return pStatus->GetCount()
end function


' ========================================================================================
' Refresh the custom statusbar control.
' ========================================================================================
sub CStatusBar_Refresh( byval hStatusBarControl as HWND ) 
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hStatusBarControl)
    if pStatus then pStatus->Refresh()
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: CStatusBar
' ========================================================================================
function CStatusBar_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' There are no child windows to position. The client width decides how much
        ' unused space the spring panel absorbs, so a resize invalidates the layout.
        dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hwnd)
        if pStatus then pStatus->Refresh()
    end if
    function = 0
end function
 
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: CStatusBar
' ========================================================================================
function CStatusBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer(hwnd)
    if pStatus = null then return 0

    ' Consume any pending layout before painting. This is what makes a burst of SetText
    ' calls cost one measuring pass instead of N.
    if pStatus->bLayoutDirty then pStatus->LayoutPanels()

    ' Capture the update rect BEFORE BeginDoubleBuffer -- BeginPaint clears it.
    '
    ' This is a CONSERVATIVE approximation and correctness never depends on it: it only
    ' skips handing the host panels that could not show a pixel anyway. GetUpdateRect
    ' returns the BOUNDING BOX of the update region, not the region, so when hover moves
    ' between two far-apart panels their union bounds the whole bar and nothing is skipped.
    ' Measured: hovering one panel repaints one; moving from panel 0 to panel 4 repaints all
    ' five. That is fine -- the buffer allocates and fills the whole client every paint
    ' regardless, so the skip is noise next to that. Reach for GetUpdateRgn/RectInRegion
    ' only if a real panel count ever makes this matter.
    dim as RECT rcUpdate
    if GetUpdateRect( hwnd, @rcUpdate, FALSE ) = 0 then GetClientRect( hwnd, @rcUpdate )

    dim as clsDoubleBuffer b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pStatus->BackColor )

    ' Fill the whole bar, which also covers any gap no panel owns (the control's own
    ' background is the only thing it paints; panels belong to the paint callback).
    b.PaintClientRect()

    ' Hand each affected panel to the host. Skipping the rest is safe even though
    ' EndDoubleBuffer blits the WHOLE client: BeginPaint's DC is clipped to the update
    ' region, so the skipped (background-filled) areas never reach the screen.
    if pStatus->PaintCallback then
        dim p as CSTATUSBAR_PAINTINFO
        p.hStatusBar = hwnd
        p.b          = @b
        for i as integer = 0 to pStatus->panelCount - 1
            dim as RECT rcHit
            if IntersectRect( @rcHit, @pStatus->panels(i).rc, @rcUpdate ) = 0 then continue for
            p.itemID     = i
            p.rc         = pStatus->panels(i).rc
            p.isHot      = (i = pStatus->nLastHotIdx)
            p.wszCaption = pStatus->panels(i).Text
            pStatus->PaintCallback( @p )
        next
    end if

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Clear any hot-tracked panel and stop the safety-net timer. Repaints only if a panel was
' actually hot, so this is cheap to call repeatedly.
' ========================================================================================
private sub CStatusBar_ClearHotTracking( byval pStatus as CSTATUSBAR ptr, byval hStatusBar as HWND )
    if pStatus = null then exit sub
    if pStatus->hotTimerOn then
        KillTimer( hStatusBar, IDT_CSTATUSBAR_HOTTRACK )
        pStatus->hotTimerOn = false
    end if
    if pStatus->nLastHotIdx <> -1 then
        pStatus->nLastHotIdx = -1
        AfxRedrawWindow( hStatusBar )
    end if
end sub


' ========================================================================================
' CStatusBar Window procedure
' This control has no child windows, so there is nothing to subclass: every message is
' handled here and anything unhandled goes to DefWindowProc.
' ========================================================================================
function CStatusBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CSTATUSBAR ptr pStatus = GetStatusBarPointer( hwnd )

    ' Per-message scratch info passed to the user MessageCallback. A plain local (NOT
    ' static) so that multiple control instances never share it. The hover tracking index
    ' likewise lives per-instance in pStatus->nLastHotIdx.
    dim m as CSTATUSBAR_MESSAGEINFO

    select case uMsg
        ' Prepare the callback MessageInfo for only the messages that we hand to the host.
        case WM_MOUSEMOVE, WM_MOUSEHOVER, WM_MOUSELEAVE, _
             WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK
            with m
                .hStatusBar = hwnd
                .uMsg       = uMsg
                .wParam     = wParam
                .lParam     = lParam
                .idx        = -1
            end with
    end select

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        CStatusBar_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       CStatusBar_OnPaint)

    case WM_TIMER
        if (pStatus <> 0) andalso (wParam = IDT_CSTATUSBAR_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no
            ' longer over our client area, clear the hot panel ourselves.
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                CStatusBar_ClearHotTracking( pStatus, hwnd )
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        ' Track that we are over the control in order to catch the eventual
        ' WM_MOUSEHOVER and WM_MOUSELEAVE events.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 250    ' HOVER_DEFAULT is 500
        if pStatus then tme.dwHoverTime = pStatus->HoverTime
        TrackMouseEvent(@tme)

        ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
        ' quickly (or onto another control), which would strand the hot panel painted.
        ' Poll the cursor while we are over the control.
        if (pStatus <> 0) andalso (pStatus->hotTimerOn = false) then
            SetTimer( hwnd, IDT_CSTATUSBAR_HOTTRACK, CSTATUSBAR_HOTTRACK_MS, NULL )
            pStatus->hotTimerOn = true
        end if

        ' Repaint only the panels whose state changed, not the whole bar.
        if pStatus then
            dim as integer idx = GetHotStatusBarPanel(hwnd)
            if idx <> pStatus->nLastHotIdx then
                CStatusBar_InvalidatePanel( pStatus, idx )                   ' newly hot
                CStatusBar_InvalidatePanel( pStatus, pStatus->nLastHotIdx )  ' previously hot
                pStatus->nLastHotIdx = idx
                ' Panel changed: pop the current tip so it re-queries TTN_GETDISPINFOW
                ' for the new panel on the next hover.
                if pStatus->hTooltip then SendMessage( pStatus->hTooltip, TTM_POP, 0, 0 )
            end if
            m.idx = idx
            if pStatus->MessageCallback then
                if pStatus->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSEHOVER
        if pStatus then
            m.idx = GetHotStatusBarPanel(hwnd)
            if pStatus->MessageCallback then
                if pStatus->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSELEAVE
        if pStatus then
            ' Fast path for clearing the hot panel (the WM_TIMER poll is the fallback for
            ' when this message never arrives). The tooltip hides itself when the mouse
            ' leaves the tool, so nothing to push. m.idx stays -1: the mouse is gone.
            CStatusBar_ClearHotTracking( pStatus, hwnd )
            if pStatus->MessageCallback then
                if pStatus->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_LBUTTONDBLCLK
        ' No SetCapture here, and so no ReleaseCapture anywhere -- see the header comment.
        ' Nor any SetFocus: a statusbar must not steal focus from the host's editor.
        if pStatus then
            m.idx = GetHotStatusBarPanel(hwnd)
            if pStatus->MessageCallback then
                if pStatus->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot panel's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pStatus then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' store in an instance-lifetime buffer and point lpszText at it
                pStatus->wszTooltip = CStatusBar_ResolveTooltipText( pStatus, pStatus->nLastHotIdx )
                pdi->lpszText = cast( LPWSTR, pStatus->wszTooltip.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        ' Don't let the hot-tracking timer outlive the window.
        if (pStatus <> 0) andalso pStatus->hotTimerOn then
            KillTimer( hwnd, IDT_CSTATUSBAR_HOTTRACK )
            pStatus->hotTimerOn = false
        end if

    case WM_NCDESTROY
        ' Delete the stored statusbar data pointer
        if pStatus then
            if IsWindow(pStatus->hToolTip) then DestroyWindow(pStatus->hToolTip)
            delete(pStatus)
        end if

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Create a tooltip that pulls its text on demand (LPSTR_TEXTCALLBACK) for the whole
' control -- one tool covering the entire bar, not one per panel, because the panels are
' painted rects rather than windows. TTN_GETDISPINFOW therefore goes to TOOLINFO.hwnd,
' which is the control itself (NOT the parent form), and CStatusBar_WndProc answers it
' using the current hot panel.
' ========================================================================================
private function CStatusBar_AddTooltip( byval hCtrl as HWND ) as HWND
    if IsWindow(hCtrl) = 0 then return 0
    dim as HWND hTooltip = CreateWindowExW( 0, "tooltips_class32", "", _
                                            WS_POPUP or TTS_ALWAYSTIP, _
                                            0, 0, 0, 0, NULL, NULL, NULL, NULL )
    if hTooltip = 0 then return 0
    SetWindowPos( hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE or SWP_NOACTIVATE )

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


' ========================================================================================
' CStatusBar_Create
' ========================================================================================
function CStatusBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @CStatusBar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN _
            )
    ' Enable double-click messages (WM_LBUTTONDBLCLK) on the control's window class,
    ' preserving the class's existing styles rather than replacing them. (CWindow's
    ' ClassStyle setter REPLACES, so this must be read-modify-write, and it needs
    ' m_hwnd -- i.e. it only works after Create.)
    pWindow->ClassStyle = pWindow->ClassStyle or CS_DBLCLKS

    ' Define the TYPE that holds all the information about this custom statusbar.
    dim as CSTATUSBAR ptr pStatus = new CSTATUSBAR
    pStatus->hWin = hCtrl
    pStatus->idc_StatusBar = CtrlID
    pStatus->nPadding = pWindow->ScaleX( CSTATUSBAR_DEFAULT_PADDING )
    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the statusbar window
    pWindow->UserData(0) = cast(LONG_PTR, pStatus)
    
    pStatus->hTooltip = CStatusBar_AddTooltip( hCtrl )

    function = hCtrl
    
end function

