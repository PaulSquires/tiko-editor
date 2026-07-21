'    Owner-drawn vertical scrollbar control
'
'    A self-contained, per-instance control in the same vein as CListBox: it owns its
'    own window class and keeps all state per instance in the CWindow UserData area, so
'    any number of instances can coexist. It knows nothing about listboxes -- the owner
'    drives it purely through CVScrollBar_SetRange( total, page, pos ) and is told about
'    user scrolling through a scroll callback.

#pragma once

#include once "clsDoubleBuffer.bi"

' Timer ids are per-window, so every instance can share these values.
#define IDT_CVSCROLL_HOTTRACK   &hCB10
#define IDT_CVSCROLL_REPEAT     &hCB11
#define CVSCROLL_HOTTRACK_MS    100    ' hover safety-net poll (TME_LEAVE is unreliable)
#define CVSCROLL_REPEAT_DELAY   400    ' initial delay before track auto-repeat kicks in
#define CVSCROLL_REPEAT_MS      80     ' auto-repeat interval while the button is held
#define CVSCROLL_MIN_THUMB      20     ' minimum thumb height (DPI scaled at use)
#define CVSCROLL_DEFAULT_WIDTH  12     ' default track width (DPI scaled at use)

type CVSCROLLBAR_PAINTINFO
    b            as clsDoubleBuffer ptr  ' points to the caller's buffer (no copy)
    rcClient     as RECT                 ' the whole track
    rcThumb      as RECT                 ' empty when no thumb is needed
    isHot        as boolean              ' mouse is over the thumb
    isDragging   as boolean
end type

' Paint the scrollbar. If unset, a default paint using the colors below is used.
type VScrollPaintCallbackSub as sub( byval p as CVSCROLLBAR_PAINTINFO ptr )
' Called when the user scrolls (drag or track paging). newPos is the new top position.
type VScrollCallbackSub as sub( byval hScrollBar as HWND, byval newPos as integer )

type CVSCROLLBAR
    hWin          as HWND
    ' --- Range model. Deliberately generic: "total" units, "page" units visible,
    '     "pos" = index of the first visible unit. ---
    nTotal        as integer = 0
    nPage         as integer = 0
    nPos          as integer = 0
    ' --- Geometry (derived from the range; never stored independently) ---
    rcThumb       as RECT
    thumbHeight   as integer = 0
    ' --- Interaction ---
    isDragging    as boolean = false
    dragOffset    as integer = 0      ' cursor offset within the thumb when grabbed
    isHot         as boolean = false
    hotTimerOn    as boolean = false
    repeatTimerOn as boolean = false
    repeatDir     as integer = 0      ' -1 = page up, +1 = page down
    ' --- Mouse wheel / trackpad ---
    '     nWheelAccum is what makes a precision touchpad usable: its deltas are routinely
    '     well under one 120 notch, so the unconsumed remainder is carried between messages
    '     and a slow two-finger swipe scrolls continuously instead of in jumps.
    nWheelAccum   as integer = 0
    nWheelStep    as integer = 0      ' units per notch; 0 = follow the system setting
    ' --- Appearance (used by the default paint; a PaintCallback overrides entirely) ---
    BackColor     as COLORREF = BGR(33,37,43)
    ForeColor     as COLORREF = BGR(53,59,69)
    ForeColorHot  as COLORREF = BGR(90,98,112)
    PaintCallback   as VScrollPaintCallbackSub
    ScrollCallback  as VScrollCallbackSub

    declare function TrackHeight() as integer
    declare function MaxPos() as integer
    declare function NeedsThumb() as boolean
    declare sub      Recalc()
    declare function PosFromThumbTop( byval thumbTop as integer ) as integer
end type

function CVSCROLLBAR.TrackHeight() as integer
    dim as RECT rc : GetClientRect( this.hWin, @rc )
    return rc.bottom - rc.top
end function

function CVSCROLLBAR.MaxPos() as integer
    dim as integer m = this.nTotal - this.nPage
    if m < 0 then m = 0
    return m
end function

' Is there anything to scroll? This is a RANGE question only -- deliberately independent
' of our own geometry, because the owner asks it to decide whether to show us at all, and
' at that point we may still be 0x0 (we only get a size once we are shown). Folding a
' track-height test in here would deadlock: never sized -> never needed -> never shown.
' The geometry guard lives in Recalc instead.
function CVSCROLLBAR.NeedsThumb() as boolean
    if (this.nTotal <= 0) orelse (this.nPage <= 0) then return false
    return (this.nTotal > this.nPage)                       ' false = everything fits
end function

' Derive the thumb rect from the range. Size and position both scale off the TRACK
' height (not any external control's height), and the thumb is clamped to a minimum
' so it stays grabbable on very large lists.
sub CVSCROLLBAR.Recalc()
    SetRectEmpty( @this.rcThumb )
    this.thumbHeight = 0
    if this.NeedsThumb() = false then exit sub

    dim as RECT rc : GetClientRect( this.hWin, @rc )
    dim as integer track = rc.bottom - rc.top
    if track <= 0 then exit sub          ' no geometry yet: nothing to draw, no divide

    dim as integer minThumb = CVSCROLL_MIN_THUMB
    dim pWindow as CWindow ptr = AfxCWindowPtr( this.hWin )
    if pWindow then minThumb = pWindow->ScaleY( CVSCROLL_MIN_THUMB )

    dim as integer th = (this.nPage / this.nTotal) * track
    if th < minThumb then th = minThumb
    if th > track then th = track
    this.thumbHeight = th

    dim as integer span = track - th
    dim as integer y = 0
    if this.MaxPos() > 0 then y = (this.nPos / this.MaxPos()) * span
    if y < 0 then y = 0
    if y > span then y = span

    SetRect( @this.rcThumb, rc.left, y, rc.right, y + th )
end sub

' Inverse of Recalc's position mapping: which pos does a given thumb top represent?
function CVSCROLLBAR.PosFromThumbTop( byval thumbTop as integer ) as integer
    dim as integer span = this.TrackHeight() - this.thumbHeight
    if span <= 0 then return 0
    if thumbTop < 0 then thumbTop = 0
    if thumbTop > span then thumbTop = span
    dim as integer p = (thumbTop / span) * this.MaxPos()
    if p < 0 then p = 0
    if p > this.MaxPos() then p = this.MaxPos()
    return p
end function


' ----------------------------------------------------------------------------------------
' PUBLIC API
' All CVScrollBar_* functions take the handle returned by CVScrollBar_Create().
' ----------------------------------------------------------------------------------------
declare function CVScrollBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND
declare sub      CVScrollBar_SetRange( byval hSB as HWND, byval nTotal as integer, byval nPage as integer, byval nPosition as integer )
declare function CVScrollBar_GetPos( byval hSB as HWND ) as integer
declare sub      CVScrollBar_SetPos( byval hSB as HWND, byval nPosition as integer )
declare function CVScrollBar_NeedsThumb( byval hSB as HWND ) as boolean
declare sub      CVScrollBar_SetColors( byval hSB as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
declare sub      CVScrollBar_SetPaintCallback( byval hSB as HWND, byval usersub as VScrollPaintCallbackSub )
declare sub      CVScrollBar_SetScrollCallback( byval hSB as HWND, byval usersub as VScrollCallbackSub )

' ----------------------------------------------------------------------------------------
' MOUSE WHEEL / TRACKPAD
'
' The control handles WM_MOUSEWHEEL itself, so a wheel or a two-finger trackpad swipe with
' the cursor over the bar scrolls it. Precision-touchpad deltas smaller than one 120 notch
' are accumulated, so a slow swipe scrolls smoothly rather than in jumps.
'
' HandleWheelDelta is that same implementation, exposed. Call it to route a wheel gesture
' the HOST received -- typically one made over the content the bar scrolls, which is a far
' bigger target than the bar itself -- passing the ALREADY-EXTRACTED signed delta:
'
'     case WM_MOUSEWHEEL
'         CVScrollBar_HandleWheelDelta( hSB, cast(short, hiword(wParam)) )
'
' The cast matters: loword/hiword yield UNSIGNED words, so a delta read without it turns
' ~-120 into ~65416 and the direction silently inverts (see C:\dev\Learnings.md).
' Returns TRUE if the position actually moved.
'
' ROUTING, worth knowing: Windows sends wheel messages to the FOCUSED window, and this
' control never takes focus. What makes hovering work is Windows 10+'s "scroll inactive
' windows when I hover over them" (Settings -> Mouse), which is on by default and routes
' them to the window under the cursor instead. With it OFF the message goes to whatever has
' focus and DefWindowProc bubbles it to that window's PARENT -- never sideways to this bar.
' Hosting HandleWheelDelta yourself is the reliable path; the built-in handler is the
' convenience.
'
' SetWheelStep sets how many range units one 120-unit notch scrolls. 0 (the default) means
' follow the system setting -- SPI_GETWHEELSCROLLLINES, typically 3, re-read per message so
' a Control Panel change takes effect immediately, and honouring WHEEL_PAGESCROLL ("a page
' per notch"). Set it when your range units are neither lines nor characters and the system
' number would be meaningless.
' ----------------------------------------------------------------------------------------
declare function CVScrollBar_HandleWheelDelta( byval hSB as HWND, byval nDelta as integer ) as boolean
declare sub      CVScrollBar_SetWheelStep( byval hSB as HWND, byval nUnitsPerNotch as integer )
declare function CVScrollBar_GetWheelStep( byval hSB as HWND ) as integer
