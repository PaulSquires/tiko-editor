
#pragma once

#include once "CBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id.
#define IDT_CSTATUSBAR_HOTTRACK   &hCB01
#define CSTATUSBAR_HOTTRACK_MS    100


' Default horizontal inset applied to each side of a panel's text when measuring. The
' control DPI-scales this at Create; CStatusBar_SetPadding takes raw pixels thereafter.
#define CSTATUSBAR_DEFAULT_PADDING  8

' type and array to hold values related to the statusbar panels
' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
' that never name the real problem -- hence nMinWidth / nWidth throughout.
' NOTE: no isHot here. Hover is transient and single-valued, so the control's nLastHotIdx
' is the one source of truth; a per-panel copy would be a second one to keep in sync.
type CSTATUSBAR_PANELINFO
    Text      as DWSTRING
    itemData  as integer         ' id to invoke if clicked on
    nMinWidth as integer = 0     ' floor for this panel's width; 0 = auto-size to text
    rc        as RECT            ' computed client coordinates (see CSTATUSBAR.LayoutPanels)
end type

type CSTATUSBAR_PAINTINFO
    hStatusBar      as HWND                   ' the control, so the callback can query it
    itemID          as integer                ' panel index (model index)
    b               as CBufferPaint ptr    ' the control's buffer for this repaint (no copy)
    rc              as RECT                   ' this panel's rect, in client coordinates
    isHot           as boolean                ' the mouse is over this panel
    wszCaption      as DWSTRING               ' the panel's Text
end type

type CSTATUSBAR_MESSAGEINFO
    hStatusBar      as HWND
    uMsg            as UINT
    wParam          as WPARAM
    lParam          as LPARAM
    idx             as integer    ' panel index under the mouse (-1 if none)
end type


' Draw one panel. Called for each panel touched by the repaint; keep it cheap. Paint through
' p->b (the control's double buffer for this repaint), using p->rc as the panel rect -- do not
' touch the screen DC. Style from the state flags; the control decides them, you only render.
' Nothing is drawn if no paint callback is set.
'
' Two contracts worth honoring, or the layout will not match what you draw:
'   - Draw with the SAME font you handed to CStatusBar_SetFont. The control measured the
'     panel widths with it; a different font means the widths lie.
'   - Inset your text by CStatusBar_GetPadding( p->hStatusBar ). That padding was added to
'     the measured text width, so it is the space the width already accounts for.
type SB_PaintCallbackSub as sub( byval p as CSTATUSBAR_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the control's default
' handling suppressed, FALSE to let it proceed. The result is honored uniformly for every
' message: the control takes no mouse capture, so there is no invariant a callback can
' strand by suppressing a message.
type SB_MessageCallbackFunc as function( byval m as CSTATUSBAR_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for a panel, on demand (only when a tip is about to show).
' Return "" for no tooltip. If unset, the panel's own Text is used.
type SB_TooltipCallbackFunc as function( byval hStatusBar as HWND, byval idx as integer ) as DWSTRING

type CSTATUSBAR
    hWin            as HWND
    hToolTip        as HWND
    wszTooltip      as DWSTRING
    panels(any)     as CSTATUSBAR_PANELINFO
    panelCount      as integer = 0
    idc_StatusBar   as integer = 1000
    HoverTime       as integer = 250
    nLastHotIdx     as integer = -1       ' panel the mouse was last over (hover tracking)
    hotTimerOn      as boolean = false    ' is the hot-tracking safety-net timer running?
    BackColor       as COLORREF
    ' Caller-supplied font (caller owns it). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member
    ' procedure of this type, and "dim as HFONT x" there fails with a misleading
    ' "error 14: Expected identifier, found 'HFONT'". See C:\dev\Learnings.md.
    hPanelFont      as HFONT
    PaintCallback   as SB_PaintCallbackSub
    MessageCallback as SB_MessageCallbackFunc
    TooltipCallback as SB_TooltipCallbackFunc    ' optional; defaults to the panel's Text
    ' --- Layout. Panel rects are derived, never set from outside; LayoutPanels() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs it,
    '     which coalesces a burst of SetText calls into one measuring pass. ---
    expandPanel     as integer = -1       ' the single spring panel, or -1 for none
    nPadding        as integer = CSTATUSBAR_DEFAULT_PADDING   ' DPI-scaled at Create
    bLayoutDirty    as boolean = true

    declare sub      LayoutPanels()
    declare function GetCount() as integer                                  ' panel count
    declare function InsertPanelAt( byval idx as integer ) as CSTATUSBAR_PANELINFO ptr
    declare function AddPanel() as CSTATUSBAR_PANELINFO ptr                       ' append
    declare function GetPanel( byval idx as integer ) as CSTATUSBAR_PANELINFO ptr
    declare function DeletePanelAt( byval idx as integer ) as boolean
    declare function IsValidPanel( byval idx as integer ) as boolean
    declare sub      Clear()
    declare sub      Refresh()
end type

' Measure every panel and assign its rect. This is the only place panel geometry is
' produced; everything else (hit-testing, painting, invalidation) consumes it.
'
'   width = max( text width + 2*padding, nMinWidth )
'
' The single spring panel then absorbs whatever width is left over. On overflow nothing
' grows -- the spring is already at its natural width -- and panels simply run past the
' right edge, which is the "shrink then clip" rule. Rects are computed honestly in that
' case rather than being squeezed: clipping is the paint pass's job, and the cursor can't
' reach past the edge anyway, so hit-testing stays correct for free.
sub CSTATUSBAR.LayoutPanels()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub
    if this.panelCount = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as integer clientW = rcClient.right - rcClient.left
    dim as integer clientH = rcClient.bottom - rcClient.top
    ' No geometry yet (created 0x0, sized only once the host shows us). Nothing to lay out,
    ' and bLayoutDirty stays consumed only because WM_SIZE will mark it again.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font --
    ' never "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hPanelFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim panelW( 0 to this.panelCount - 1 ) as integer
    dim as integer totalW = 0

    for i as integer = 0 to this.panelCount - 1
        dim as integer textW = 0
        dim as integer nChars = len( this.panels(i).Text )
        if nChars > 0 then
            dim as SIZE sz
            if GetTextExtentPoint32W( hDC, this.panels(i).Text.vptr, nChars, @sz ) then
                textW = sz.cx
            end if
        end if
        dim as integer nWidth = textW + (this.nPadding * 2)
        if nWidth < this.panels(i).nMinWidth then nWidth = this.panels(i).nMinWidth
        panelW(i) = nWidth
        totalW += nWidth
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' The spring panel absorbs any unused width. leftover <= 0 means we overflow, and no
    ' panel grows.
    dim as integer leftover = clientW - totalW
    if (leftover > 0) andalso this.IsValidPanel( this.expandPanel ) then
        panelW(this.expandPanel) += leftover
    end if

    ' Position left-to-right; every panel spans the full client height.
    dim as integer x = rcClient.left
    for i as integer = 0 to this.panelCount - 1
        SetRect( @this.panels(i).rc, x, rcClient.top, x + panelW(i), rcClient.bottom )
        x += panelW(i)
    next
end sub

function CSTATUSBAR.GetCount() as integer
    return this.panelCount
end function

function CSTATUSBAR.IsValidPanel( byval idx as integer ) as boolean
    return (idx >= 0) andalso (idx < this.panelCount)
end function

function CSTATUSBAR.GetPanel( byval idx as integer ) as CSTATUSBAR_PANELINFO ptr
    if this.IsValidPanel(idx) = false then return null
    return @this.panels(idx)
end function

' Insert a fresh (reset) panel at idx, shifting later panels up. Grows the
' backing store by doubling so bulk inserts are amortized O(1), not O(n^2).
function CSTATUSBAR.InsertPanelAt( byval idx as integer ) as CSTATUSBAR_PANELINFO ptr
    if idx < 0 then idx = 0
    if idx > this.panelCount then idx = this.panelCount

    dim as integer cap = ubound(this.panels) + 1
    if this.panelCount >= cap then
        dim as integer newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.panels( 0 to newcap - 1 )
    end if

    ' shift [idx .. rowCount-1] up by one (no-op when appending)
    for i as integer = this.panelCount to idx + 1 step -1
        this.panels(i) = this.panels(i - 1)
    next

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot)
    with this.panels(idx)
        .Text       = ""
        .itemData   = 0
        .nMinWidth  = 0
        SetRectEmpty( @.rc )
    end with

    this.panelCount += 1
    ' expandPanel is a stored index: inserting at or before it shifts it up.
    if (this.expandPanel <> -1) andalso (idx <= this.expandPanel) then
        this.expandPanel += 1
    end if
    this.Refresh()
    return @this.panels(idx)
end function

function CSTATUSBAR.AddPanel() as CSTATUSBAR_PANELINFO ptr
    return this.InsertPanelAt( this.panelCount )
end function

function CSTATUSBAR.DeletePanelAt( byval idx as integer ) as boolean
    if this.IsValidPanel(idx) = false then return false
    ' shift [idx+1 .. panelCount-1] down by one
    for i as integer = idx to this.panelCount - 2
        this.panels(i) = this.panels(i + 1)
    next
    this.panels(this.panelCount - 1).Text = ""   ' free the vacated last slot's string
    this.panelCount -= 1
    ' Keep the stored spring index meaningful: deleting the spring itself clears it,
    ' deleting anything before it shifts it down.
    if this.expandPanel <> -1 then
        if idx = this.expandPanel then
            this.expandPanel = -1
        elseif idx < this.expandPanel then
            this.expandPanel -= 1
        end if
    end if
    ' A hot index can likewise be left dangling past the end.
    if this.nLastHotIdx >= this.panelCount then this.nLastHotIdx = -1
    this.Refresh()
    return true
end function

sub CSTATUSBAR.Clear()
    for i as integer = 0 to this.panelCount - 1
        this.panels(i).Text = ""
    next
    this.panelCount  = 0
    this.expandPanel = -1
    this.nLastHotIdx = -1
    this.Refresh()
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy: a burst of SetText calls costs one measuring pass, not N.
sub CSTATUSBAR.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking panel is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' THE CONTROL HANDLE
'   Every CStatusBar_* function takes the handle returned by CStatusBar_Create(). 
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need
'   to treat the control as a window, e.g. SetWindowPos() to place and size it. An opaque
'   wrapper would buy a little type-safety and break that, so it was rejected.
''
' LIFETIME
'   The control frees itself when its window is destroyed. Fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). There are no child controls.
'   The control is created zero-sized: position it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function CStatusBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

' ----------------------------------------------------------------------------------------
' Adding / removing panels.  Add* and Insert* return the new panels's index, or -1.
' ----------------------------------------------------------------------------------------
declare function CStatusBar_AddPanel( byval hStatusBarControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
declare function CStatusBar_InsertPanel( byval hStatusBarControl as HWND, byval idx as integer, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
declare function CStatusBar_DeletePanel( byval hStatusBarControl as HWND, byval idx as integer ) as boolean
declare sub      CStatusBar_Clear( byval hStatusBarControl as HWND )
declare sub      CStatusBar_Refresh( byval hStatusBarControl as HWND )

' ----------------------------------------------------------------------------------------
' Counts.  GetCount = every panel in the control.
' ----------------------------------------------------------------------------------------
declare function CStatusBar_GetCount( byval hStatusBarControl as HWND ) as integer

' ----------------------------------------------------------------------------------------
' Panel contents.  Set* return FALSE for an invalid panel index.
' ----------------------------------------------------------------------------------------
declare function CStatusBar_GetText( byval hStatusBarControl as HWND, byval idx as integer ) as DWSTRING
declare function CStatusBar_SetText( byval hStatusBarControl as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
declare function CStatusBar_GetItemData( byval hStatusBarControl as HWND, byval idx as integer ) as integer
declare function CStatusBar_SetItemData( byval hStatusBarControl as HWND, byval idx as integer, byval itemData as integer ) as boolean
declare function CStatusBar_GetItemRect( byval hStatusBarControl as HWND, byval idx as integer ) as RECT

' ----------------------------------------------------------------------------------------
' Layout.
'
'   A panel is always at least as wide as its text:
'       width = max( text width + 2*padding, nMinWidth )
'
'   nMinWidth defaults to 0, so panels auto-size. Set a floor on a panel whose text changes
'   length (a line/col readout) to stop it resizing and shoving its neighbours on every
'   update.
'
'   SetExpandPanel nominates the ONE spring panel that absorbs all unused bar width;
'   setting it clears any previous one, and -1 clears it entirely. Panels after the spring
'   are pushed right, which is how you build a trailing right-aligned group -- put the
'   spring in the middle. If the panels overflow the bar, nothing grows and the panels on
'   the right are clipped by the edge.
'
'   Padding is a LAYOUT input, not just cosmetics: it is added to the measured text width.
'   Inset your paint callback's text by GetPadding or the text will not sit where the width
'   assumed it would.
'
'   Rects are computed lazily. GetPanelRect forces a layout if one is pending, so it is
'   always current.
' ----------------------------------------------------------------------------------------
declare function CStatusBar_GetPanelMinWidth( byval hStatusBarControl as HWND, byval idx as integer ) as integer
declare function CStatusBar_SetPanelMinWidth( byval hStatusBarControl as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
declare function CStatusBar_GetExpandPanel( byval hStatusBarControl as HWND ) as integer
declare function CStatusBar_SetExpandPanel( byval hStatusBarControl as HWND, byval idx as integer ) as boolean
declare function CStatusBar_GetPadding( byval hStatusBarControl as HWND ) as integer
declare sub      CStatusBar_SetPadding( byval hStatusBarControl as HWND, byval nPadding as integer )
declare function CStatusBar_GetPanelRect( byval hStatusBarControl as HWND, byval idx as integer, byref rc as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  The font is borrowed, never owned: keep it alive and destroy it yourself.
' ----------------------------------------------------------------------------------------
declare function CStatusBar_GetBackColor( byval hStatusBarControl as HWND ) as COLORREF
declare function CStatusBar_SetBackColor( byval hStatusBarControl as HWND, byval clr as COLORREF ) as COLORREF
declare function CStatusBar_GetFont( byval hStatusBarControl as HWND ) as HFONT
declare function CStatusBar_SetFont( byval hStatusBarControl as HWND, byval hFont as HFONT ) as boolean
declare function CStatusBar_GetTooltipHandle( byval hStatusBarControl as HWND ) as HWND
declare sub      CStatusBar_SetHoverTime( byval hStatusBarControl as HWND, byval milliseconds as integer )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback   - draw one panel. Required if you want to see anything.
'   MessageCallback - observe mouse messages; return TRUE to suppress default handling.
'   TooltipCallback - supply per-panel tooltip text on demand; "" for none.
' ----------------------------------------------------------------------------------------
declare sub      CStatusBar_SetPaintCallback( byval hStatusBarControl as HWND, byval usersub as SB_PaintCallbackSub )
declare sub      CStatusBar_SetMessageCallback( byval hStatusBarControl as HWND, byval userfunc as SB_MessageCallbackFunc )
declare sub      CStatusBar_SetTooltipCallback( byval hStatusBarControl as HWND, byval userfunc as SB_TooltipCallbackFunc )


