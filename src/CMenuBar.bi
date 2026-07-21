
#pragma once

' The bar's dropdowns ARE CPopupMenu windows; the dependency is one-way (CPopupMenu
' never knows about the bar -- it reports through CPM_NOTIFY_* messages to whatever
' notify window registered, which the bar sets to itself).
#include once "CPopupMenu.bi"

' Polling timer that guarantees hover highlight is cleared when the mouse leaves the
' bar. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer ids are per-window, so every
' instance can share this id.
#define IDT_CMENUBAR_HOTTRACK   &hCB40
#define CMENUBAR_HOTTRACK_MS    100

' Default horizontal inset applied to each side of an item's caption when measuring
' (tiko uses 6+6). DPI-scaled at Create; CMenuBar_SetPadding takes raw pixels.
#define CMENUBAR_DEFAULT_PADDING  6


type CMENUBAR_ITEM
    id        as long            ' host id for this bar entry (not a command id --
                                 ' commands live on the popup items)
    wszText   as DWSTRING
    hDropDown as HWND            ' attached CPopupMenu window. OWNED once attached: the
                                 ' bar destroys it when it is itself destroyed. 0 = a
                                 ' bar item with no menu (clicks on it do nothing).
    ' --- DERIVED by LayoutItems, never set from outside. ---
    nWidth    as long = 0        ' measured width: text + 2*padding
    rc        as RECT            ' full item rect, client coordinates (full bar height)
end type

' Colors for the built-in painter. Copied on Set. A hovered item and the ACTIVE item
' (its dropdown open, or holding the keyboard highlight) render identically -- hot
' fill + hot text -- because a click opens the dropdown at once, so the two are
' frames of one gesture. Set a paint callback if you want them distinguished.
type CMENUBAR_COLORS
    BackColor    as COLORREF     ' bar background + idle item fill
    ForeColor    as COLORREF
    BackColorHot as COLORREF     ' hovered / active item fill
    ForeColorHot as COLORREF     ' hovered / active item text
end type

' Handed to the per-item paint callback (which replaces the built-in painter for every
' item when set). Paint through p->b; fill p->rc and center the caption in it. The
' control paints the bar background itself, before the items.
type CMENUBAR_PAINTINFO
    hMenuBar   as HWND
    itemID     as long                    ' bar item index
    b          as clsDoubleBuffer ptr
    rc         as RECT                    ' the full item rect: fill THIS
    isHot      as boolean                 ' mouse is over this item
    isActive   as boolean                 ' this item's dropdown is open, or it holds
                                          ' the keyboard highlight
    wszCaption as DWSTRING
end type

' A command item on one of this bar's dropdowns was executed (mouse or Enter). The
' popup chain is ALREADY closed and the bar's highlight already cleared when this
' fires. id is the POPUP item's command id -- tiko's shim posts it straight on:
' PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(id, 0), 0 ).
type MB_CommandCallbackSub as sub( byval hMenuBar as HWND, byval id as long )

' The dropdown for bar item idx is about to open (click, hover-switch, keyboard, or
' CMenuBar_OpenMenu). The hook for just-in-time state: flip enabled/checked on the
' popup tree (the by-id setters recurse), rebuild dynamic sections. Fired BEFORE the
' popup's own PM_InitPopupCallback. hPopup is the dropdown being opened.
type MB_InitPopupCallbackSub as sub( byval hMenuBar as HWND, byval idx as long, byval hPopup as HWND )

' Draw one bar item, INSTEAD OF the built-in painter. See CMENUBAR_PAINTINFO.
type MB_PaintItemCallbackSub as sub( byval p as CMENUBAR_PAINTINFO ptr )


type CMENUBAR
    hWin              as HWND
    items(any)        as CMENUBAR_ITEM
    itemCount         as long = 0
    idc_MenuBar       as long = 0
    ' Two single-valued facts, deliberately separate:
    '   nHotIdx    - transient mouse hover (text-only highlight). Cleared on leave.
    '   nActiveIdx - the item whose dropdown is open, OR the keyboard highlight when
    '                no dropdown is open (bMenuOpen tells the two modes apart).
    nHotIdx           as long = -1
    nActiveIdx        as long = -1
    bMenuOpen         as boolean = false
    colors            as CMENUBAR_COLORS
    ' Caller-supplied font (caller owns it). NOT named hFont -- the HFONT shadowing
    ' trap (see C:\dev\Learnings.md).
    hBarFont          as HFONT
    nPadding          as long = CMENUBAR_DEFAULT_PADDING   ' DPI-scaled at Create
    bLayoutDirty      as boolean = true
    hotTimerOn        as boolean = false
    CommandCallback   as MB_CommandCallbackSub
    InitPopupCallback as MB_InitPopupCallbackSub
    PaintItemCallback as MB_PaintItemCallbackSub

    declare sub      LayoutItems()
    declare function HitTest( byval x as long, byval y as long ) as long
    declare function IsValidItem( byval idx as long ) as boolean
    declare sub      Refresh()
end type


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A MENU BAR: a horizontal strip of text items, each optionally owning a CPopupMenu
' dropdown. Click opens the dropdown; while one is open, hovering another bar item
' auto-switches to its dropdown; clicking the bar again closes. The bar never takes
' focus -- keyboard navigation arrives through CMenuBar_FilterMessage, which the host
' MUST call in its message pump (see the contract in CPopupMenu.bi; the bar's filter
' wraps the popup one, so one call serves the whole menu system).
'
' ALT-KEY ACTIVATION IS THE HOST'S. The filter never consumes WM_SYSKEY* -- the host
' decides what Alt (or an accelerator like Alt+F) does and calls CMenuBar_OpenMenu /
' CMenuBar_SetActiveIndex to drive the bar (tiko routes Alt+F etc. through its
' accelerator table precisely because a global Alt hook conflicts with Scintilla).
'
' THE CONTROL HANDLE is a real HWND (family convention): the host positions and sizes
' the bar with SetWindowPos like any child window -- bar height, compact mode, and the
' strip to the right of the items (tiko's "Update Available" label) are all the
' host's. CMenuBar_GetIdealWidth reports the items' total width for exactly that.
'
' LIFETIME: the control frees itself on destroy, and destroys its attached dropdowns
' (which destroy their submenus) -- one DestroyWindow unwinds the whole menu system.
' Fonts you pass in stay yours.
' ----------------------------------------------------------------------------------------
declare function CMenuBar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Items. AddItem appends and returns the new index (or -1). SetDropDown attaches a
' CPopupMenu as the item's dropdown -- OWNERSHIP TRANSFERS; attaching over an existing
' dropdown destroys the old one; 0 detaches without destroying. Programmatic mutators
' are SILENT.
' ----------------------------------------------------------------------------------------
declare function CMenuBar_AddItem( byval hMenuBar as HWND, byval Text as DWSTRING, byval id as long ) as long
declare function CMenuBar_GetItemCount( byval hMenuBar as HWND ) as long
declare function CMenuBar_GetItemText( byval hMenuBar as HWND, byval idx as long ) as DWSTRING
declare function CMenuBar_SetItemText( byval hMenuBar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
declare function CMenuBar_SetDropDown( byval hMenuBar as HWND, byval idx as long, byval hPopup as HWND ) as boolean
declare function CMenuBar_GetDropDown( byval hMenuBar as HWND, byval idx as long ) as HWND

' ----------------------------------------------------------------------------------------
' Geometry (derived; queries force a pending layout). Items are laid out contiguously
' from x = 0, each item = text width + 2*padding, spanning the full bar height.
' GetIdealWidth = the right edge of the last item: size the bar with it, or draw your
' own content (an update label) in the leftover strip of a full-width bar.
' ----------------------------------------------------------------------------------------
declare function CMenuBar_GetItemRect( byval hMenuBar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function CMenuBar_GetIdealWidth( byval hMenuBar as HWND ) as long
declare function CMenuBar_GetPadding( byval hMenuBar as HWND ) as long
declare sub      CMenuBar_SetPadding( byval hMenuBar as HWND, byval nPadding as long )

' ----------------------------------------------------------------------------------------
' Menu control. OpenMenu is the host's keyboard entry point (tiko's Alt+F accelerator):
' opens item idx's dropdown, optionally putting the selection on its first selectable
' row. CloseMenu closes any open dropdown chain AND clears the highlight -- call it
' from your WM_ACTIVATEAPP(FALSE) handler for instant dismissal on app switch (the
' control's own foreground watch is a 100ms poll). Both are SILENT (programmatic).
' SetActiveIndex sets the keyboard highlight WITHOUT opening a menu (-1 clears).
' ----------------------------------------------------------------------------------------
declare function CMenuBar_OpenMenu( byval hMenuBar as HWND, byval idx as long, byval bSelectFirst as boolean = false ) as boolean
declare sub      CMenuBar_CloseMenu( byval hMenuBar as HWND )
declare function CMenuBar_IsMenuOpen( byval hMenuBar as HWND ) as boolean
declare function CMenuBar_GetActiveIndex( byval hMenuBar as HWND ) as long
declare function CMenuBar_SetActiveIndex( byval hMenuBar as HWND, byval idx as long ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance. The font is borrowed (caller owns it) and is also the measuring font.
' ----------------------------------------------------------------------------------------
declare sub      CMenuBar_SetColors( byval hMenuBar as HWND, byval pColors as CMENUBAR_COLORS ptr )
declare sub      CMenuBar_SetFont( byval hMenuBar as HWND, byval hBarFont as HFONT )

' ----------------------------------------------------------------------------------------
' Plumbing.
'   FilterMessage - THE message-pump hook for the whole menu system (bar + dropdowns
'                   + submenus): call it once per pumped message, skip Translate/
'                   Dispatch when it returns TRUE. Handles arrows/Enter/Escape/Home/
'                   End while a dropdown is open (wrapping the CPopupMenu filter and
'                   adding the bar-boundary moves: Left at the root / Right on a row
'                   without a submenu step between bar items), the highlight-only
'                   keyboard mode, and outside-click dismissal.
'   Callbacks     - see the type declarations above for each contract.
' ----------------------------------------------------------------------------------------
declare function CMenuBar_FilterMessage( byval hMenuBar as HWND, byval pMsg as MSG ptr ) as boolean
declare sub      CMenuBar_SetCommandCallback( byval hMenuBar as HWND, byval usersub as MB_CommandCallbackSub )
declare sub      CMenuBar_SetInitPopupCallback( byval hMenuBar as HWND, byval usersub as MB_InitPopupCallbackSub )
declare sub      CMenuBar_SetPaintItemCallback( byval hMenuBar as HWND, byval usersub as MB_PaintItemCallbackSub )
