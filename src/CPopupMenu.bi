
#pragma once

#include once "clsDoubleBuffer.bi"

' Safety-net poll timer, run on the ROOT of an open popup chain only. It does two jobs:
'   1. Hover net: WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so
'      the innermost level's highlight is cleared by polling the cursor.
'   2. Foreground watch: the popups never activate (WS_EX_NOACTIVATE / MA_NOACTIVATE),
'      so they cannot see focus loss themselves, and WM_ACTIVATEAPP is SENT to the host,
'      never pumped -- the message filter cannot catch it. Polling GetForegroundWindow
'      closes the chain when the user alt-tabs away. A host wanting INSTANT dismissal
'      calls CPopupMenu_CloseChain / CMenuBar_CloseMenu from its own WM_ACTIVATEAPP.
' Timer ids are per-window, so every instance can share this id.
#define IDT_CPOPUPMENU_POLL    &hCB41
#define CPOPUPMENU_POLL_MS     100

' How long the cursor rests on an item before its submenu opens (or a stale sibling
' submenu closes). A click opens the submenu immediately; hover waits this long.
#define CPOPUPMENU_HOVER_MS    250

' Default geometry, DPI-scaled once at Create; the setters take raw pixels thereafter.
#define CPOPUPMENU_DEFAULT_ITEMHEIGHT   24   ' command row height (tiko compact mode = 20)
' Separator rows are SHORT -- a rule with a little air, not a full-height row. Rows are
' therefore not all the same height, which is why LayoutItems walks a running y instead
' of multiplying by an index. Everything downstream (hit-testing, painting, submenu
' anchoring) reads the rects, so nothing else had to learn about the difference.
#define CPOPUPMENU_DEFAULT_SEPHEIGHT     9   ' separator row height
' Thickness of the rule drawn inside that row. NOT DPI-scaled at Create, unlike every
' other geometry input here: a hairline divider should stay a hairline at 150% rather
' than growing into a bar. A host that wants it scaled scales it itself.
#define CPOPUPMENU_DEFAULT_SEPTHICKNESS  1
#define CPOPUPMENU_DEFAULT_CHECKCOL     30   ' left gutter column for the check glyph
#define CPOPUPMENU_DEFAULT_CHEVRONCOL   20   ' right column for the submenu chevron
#define CPOPUPMENU_DEFAULT_ACCELGAP     24   ' minimum gap between caption and accel text
#define CPOPUPMENU_DEFAULT_HPADDING     2    ' window padding left/right of the rows
#define CPOPUPMENU_DEFAULT_VPADDING     8    ' window padding above/below the rows

' Anchor alignment for CPopupMenu_ShowForRect (rcAnchor is in SCREEN pixels):
'   PM_ALIGN_BELOW: a menubar dropdown -- left edges align, top = rcAnchor.bottom - 1
'                   (minus one so the popup border merges with the bar, tiko-style).
'   PM_ALIGN_RIGHT: a submenu -- left = rcAnchor.right, top = rcAnchor.top; flips to
'                   the anchor's left side when it would leave the work area.
#define PM_ALIGN_BELOW   1
#define PM_ALIGN_RIGHT   2

' Notifications SENT BY a popup chain's ROOT to its notify window (CMenuBar registers
' itself on its dropdowns; a bar-less host may SetNotifyWindow too, or just use the
' callbacks -- one implementation, two doors).
'   CPM_NOTIFY_COMMAND: an enabled command item was executed (mouse or Enter). The chain
'                       is ALREADY closed when this arrives. wParam = command id,
'                       lParam = the chain's root popup HWND.
'   CPM_NOTIFY_CLOSED:  the chain was dismissed by USER interaction without executing
'                       anything. wParam = 1 when closed by Escape at the root level,
'                       0 for every other dismissal (outside click, foreground loss).
'                       CMenuBar uses that distinction: Escape keeps the bar item
'                       highlighted (keyboard mode), an outside click clears it.
'                       lParam = the chain's root popup HWND. Programmatic Hide /
'                       CloseChain / CloseMenu send NOTHING (setters are silent; only
'                       user interaction notifies).
#define CPM_NOTIFY_COMMAND   (WM_USER + 110)
#define CPM_NOTIFY_CLOSED    (WM_USER + 111)


' One row of a popup menu. A separator is stored as a row (id 0); a submenu parent is a
' row whose hSubMenu holds another CPopupMenu window -- attachment is what makes the
' menu tree recursive, so nesting depth falls out of the data model rather than being a
' hard-coded level count.
type CPOPUPMENU_ITEM
    id          as long          ' host command id (0 for separators). The by-id API
                                 ' searches this menu then recurses into attached
                                 ' submenus, so ids should be unique across the tree.
    wszCaption  as DWSTRING
    wszAccel    as DWSTRING      ' right-aligned accelerator text ("Ctrl+O"); "" = none.
                                 ' DISPLAY ONLY -- the host owns actual accelerators.
    isSeparator as boolean = false
    isDisabled  as boolean = false
    isChecked   as boolean = false
    hSubMenu    as HWND          ' attached CPopupMenu window. OWNED once attached: the
                                 ' parent destroys it when it is itself destroyed.
    ' --- DERIVED by LayoutItems, never set from outside. ---
    rc          as RECT          ' full row rect, client coordinates
end type

' Colors for the built-in painter. Copied on Set (the struct stays yours). Disabled
' rows use ForeColorDisabled over the NORMAL BackColor -- a disabled row never lights
' up, so no separate disabled background exists; use the paint callback if you need one.
type CPOPUPMENU_COLORS
    BackColor         as COLORREF    ' window panel + normal row background
    ForeColor         as COLORREF
    BackColorHot      as COLORREF    ' the selected/hovered row
    ForeColorHot      as COLORREF
    ForeColorDisabled as COLORREF    ' disabled text and the accel text of disabled rows
    BorderColor       as COLORREF    ' 1px window border (the control's chrome)
    ' The separator rule. Split out of ForeColorDisabled (which drew it until
    ' 2026-07-21): a divider and greyed-out text are different things and a host
    ' generally already owns a divider color it wants menus to match.
    ' It carries a DEFAULT, unlike every field above, precisely so that a host written
    ' against the older struct -- filling the fields it knows and leaving this one alone
    ' -- gets a visible grey rule instead of the black one a zero-initialised field
    ' would produce. Set it explicitly to get black.
    SeparatorColor    as COLORREF = BGR(128,128,128)
end type

' Handed to the per-item paint callback. Paint through p->b (the control's double
' buffer for this repaint) -- do not touch the screen DC. Fill p->rc; the caption goes
' in p->rcText DT_LEFT, the accel in the SAME rect DT_RIGHT, the check glyph centered
' in p->rcCheck, the chevron centered in p->rcChevron. Use the rects as handed to you
' instead of re-deriving insets: they are exactly the space the measured width accounts
' for. Draw with the same fonts you handed to SetFonts -- the widths were measured with
' them. The control paints the window background and its border chrome itself (border
' AFTER the callbacks, so chrome is authoritative no matter what a callback paints).
type CPOPUPMENU_PAINTINFO
    hPopup      as HWND                   ' the control, so the callback can query it
    itemID      as long                   ' row index within this popup
    id          as long                   ' the row's command id (0 for separators)
    b           as clsDoubleBuffer ptr    ' the control's buffer for this repaint
    ' --- Geometry, precomputed. rcCheck/rcText/rcChevron partition rc left-to-right. ---
    rc          as RECT                   ' the full row: fill THIS
    rcCheck     as RECT                   ' left gutter (check glyph)
    rcText      as RECT                   ' caption DT_LEFT + accel DT_RIGHT
    rcChevron   as RECT                   ' right column (submenu chevron)
    ' --- State: the control decides these, you only render. ---
    isSeparator as boolean
    isDisabled  as boolean
    isChecked   as boolean
    hasSubMenu  as boolean
    isHot       as boolean                ' this row is the current selection/hover
    wszCaption  as DWSTRING
    wszAccel    as DWSTRING
end type

' An enabled command item was executed (mouse click or Enter). Fired on the chain's
' ROOT popup, AFTER the whole chain has closed -- so the handler can open dialogs, or
' re-open menus, without fighting a live popup. Fired for user interaction only.
type PM_SelectCallbackSub as sub( byval hPopup as HWND, byval id as long )

' THIS popup is about to become visible (each level fires its own as it opens). The
' hook for just-in-time work: flip enabled/checked states, rebuild a dynamic section
' (MRU lists) with Clear + AddItem, retitle items. Mutations here are coalesced into
' the single layout pass that follows.
type PM_InitPopupCallbackSub as sub( byval hPopup as HWND )

' Draw one row, INSTEAD OF the built-in painter (all rows or none -- setting the
' callback takes over every row of that popup). See CPOPUPMENU_PAINTINFO.
type PM_PaintItemCallbackSub as sub( byval p as CPOPUPMENU_PAINTINFO ptr )


type CPOPUPMENU
    hWin              as HWND
    items(any)        as CPOPUPMENU_ITEM
    itemCount         as long = 0
    idc_Popup         as long = 0
    ' The current selection. Menu semantics: hover IS selection (one highlight moved by
    ' mouse or keyboard alike), so there is no separate hot index. While this level has
    ' an open submenu the selection is PINNED to the submenu's parent item: the clear
    ' paths (WM_MOUSELEAVE, the poll net) stand down so crossing into the submenu can
    ' never strip the parent row's highlight.
    nCurSel           as long = -1
    ' Who last set nCurSel. TRUE only when the mouse did (WM_MOUSEMOVE); keyboard and
    ' programmatic selection set it FALSE. The hover-clear paths (WM_MOUSELEAVE and
    ' the poll net) only clear a HOVER-owned selection -- in keyboard mode the cursor
    ' legitimately sits anywhere, and clearing would wipe the highlight ~100ms after
    ' every keypress (the bug that motivated this flag).
    bHoverSel         as boolean = false
    colors            as CPOPUPMENU_COLORS
    ' Caller-supplied fonts (caller owns them). NOT named hFont: FreeBASIC is
    ' case-insensitive, so a member called hFont would shadow the TYPE name HFONT inside
    ' every member procedure of this type (see C:\dev\Learnings.md).
    hTextFont         as HFONT            ' captions + accel text; also the measuring font
    hGlyphFont        as HFONT            ' check/chevron glyphs; 0 = use hTextFont
    wszCheckGlyph     as DWSTRING         ' default chr(&h2713) "checkmark"
    wszChevronGlyph   as DWSTRING         ' default chr(&h203A) "single right angle quote"
    ' --- Geometry inputs, all pixels, DPI-scaled once at Create. ---
    nItemHeight       as long = CPOPUPMENU_DEFAULT_ITEMHEIGHT
    nSeparatorHeight  as long = CPOPUPMENU_DEFAULT_SEPHEIGHT   ' separator rows only
    nSeparatorThickness as long = CPOPUPMENU_DEFAULT_SEPTHICKNESS  ' the rule's own weight
    nCheckColWidth    as long = CPOPUPMENU_DEFAULT_CHECKCOL
    nChevronColWidth  as long = CPOPUPMENU_DEFAULT_CHEVRONCOL
    nAccelGap         as long = CPOPUPMENU_DEFAULT_ACCELGAP
    nHPadding         as long = CPOPUPMENU_DEFAULT_HPADDING
    nVPadding         as long = CPOPUPMENU_DEFAULT_VPADDING
    nBorderWidth      as long = 1
    nMinWidth         as long = 0         ' floor on CONTENT width; 0 = auto (MRU menus)
    ' --- Layout. Row rects and the ideal window size are derived, never set;
    '     LayoutItems() owns them. Layout is lazy: mutators mark it dirty, the next
    '     paint / show / rect query runs it, coalescing a burst of AddItem calls into
    '     one measuring pass. ---
    bLayoutDirty      as boolean = true
    nIdealW           as long = 0         ' measured window width (content + padding + border)
    nIdealH           as long = 0
    ' --- Chain links. The open chain IS these fields -- root.hOpenChild -> sub ->
    '     sub.hOpenChild -> ... with hParentLevel/nParentItem as the back-links and the
    '     module-shared gPM_OpenRoot marking the root. One chain at a time is a
    '     documented invariant (real menus share it). ---
    hOwner            as HWND             ' activation owner passed to Create (host main
                                          ' window): owns the popup's z-order and is what
                                          ' the foreground watch treats as "still ours"
    hParentLevel      as HWND             ' popup that opened us as a submenu; 0 while
                                          ' shown as a root (bar dropdown / context menu)
    nParentItem       as long = -1        ' index in hParentLevel we hang off
    hOpenChild        as HWND             ' currently-open submenu window, 0 = none
    hNotifyWnd        as HWND             ' CPM_NOTIFY_* target; 0 = no messages sent
    pollTimerOn       as boolean = false  ' safety-net timer running (chain root only)
    SelectCallback    as PM_SelectCallbackSub
    InitPopupCallback as PM_InitPopupCallbackSub
    PaintItemCallback as PM_PaintItemCallbackSub

    declare sub      LayoutItems()
    declare function HitTest( byval x as long, byval y as long ) as long
    declare function NextSelectable( byval nFrom as long, byval nDir as long ) as long
    declare function FindItem( byval id as long ) as long
    declare function IsValidItem( byval idx as long ) as boolean
    declare sub      Refresh()
end type


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A FLOATING MENU WINDOW, USABLE ALONE OR UNDER CMenuBar
'   One CPopupMenu is one long-lived hidden WS_POPUP window: create it once, fill it
'   with items, then Show/Hide it as often as needed (no create-per-open). It never
'   takes focus or activation -- the host window's title bar stays active while a menu
'   is up, and keyboard input keeps flowing to the host, where CPopupMenu_FilterMessage
'   (called from the host's message pump, IsDialogMessage-style) intercepts it.
'
' THE MESSAGE-PUMP CONTRACT (this is not optional)
'   Keyboard navigation and click-outside dismissal both live in the filter. A host
'   that shows a popup without calling the filter in its pump gets a menu that opens
'   and paints but cannot be keyboard-driven and never dismisses on an outside click:
'       do while GetMessage(@uMsg, null, 0, 0)
'           if CPopupMenu_FilterMessage( hMyMenu, @uMsg ) then continue do
'           ...TranslateMessage/DispatchMessage...
'   (Under CMenuBar, call CMenuBar_FilterMessage instead -- it wraps this one.)
'
' THE CONTROL HANDLE
'   A real HWND on purpose (not an opaque type): the family convention, and Hide/Show
'   state is legitimately queryable with IsWindowVisible.
'
' LIFETIME AND OWNERSHIP
'   The control frees itself when its window is destroyed. Attaching a popup to a
'   parent (CPopupMenu_SetItemSubMenu / CMenuBar_SetDropDown) TRANSFERS OWNERSHIP: the
'   parent destroys attached children when it is destroyed, so one DestroyWindow on the
'   bar unwinds the whole menu tree. Never attach the same popup to two parents.
'   Fonts you pass in stay yours.
' ----------------------------------------------------------------------------------------
' Creation. hOwner is the host's top-level window: it owns the popup (z-order, minimize
' behavior) and anchors the foreground watch. The window is created hidden and sized by
' Show. CtrlID is stored for the host's own bookkeeping (there is no parent to route
' notifications by id).
' ----------------------------------------------------------------------------------------
declare function CPopupMenu_Create( byval hOwner as HWND, byval CtrlID as long = 0 ) as HWND

' ----------------------------------------------------------------------------------------
' Items. AddItem/AddSeparator append and return the new row index (or -1). Everything
' addressed "byval id" searches THIS menu first, then recurses into attached submenus
' (first match wins) -- so a host can flip any state in the tree from the root.
' Programmatic mutators are SILENT (no callbacks, no notifications).
'   SetItemSubMenu attaches a CPopupMenu window to an item (ownership transfers);
'   attaching over an existing submenu DESTROYS the old one; hSubMenu = 0 detaches
'   WITHOUT destroying (ownership returns to the caller).
'   DeleteItem / Clear destroy any attached submenu windows of the removed rows.
' ----------------------------------------------------------------------------------------
declare function CPopupMenu_AddItem( byval hPopup as HWND, byval id as long, byval Caption as DWSTRING, byval Accel as DWSTRING = "", byval bDisabled as boolean = false, byval bChecked as boolean = false ) as long
declare function CPopupMenu_AddSeparator( byval hPopup as HWND ) as long
declare function CPopupMenu_GetItemCount( byval hPopup as HWND ) as long
declare function CPopupMenu_SetItemSubMenu( byval hPopup as HWND, byval id as long, byval hSubMenu as HWND ) as boolean
declare function CPopupMenu_GetItemSubMenu( byval hPopup as HWND, byval id as long ) as HWND
declare function CPopupMenu_DeleteItem( byval hPopup as HWND, byval id as long ) as boolean
declare sub      CPopupMenu_Clear( byval hPopup as HWND )
declare function CPopupMenu_SetItemEnabled( byval hPopup as HWND, byval id as long, byval bEnabled as boolean ) as boolean
declare function CPopupMenu_GetItemEnabled( byval hPopup as HWND, byval id as long ) as boolean
declare function CPopupMenu_SetItemChecked( byval hPopup as HWND, byval id as long, byval bChecked as boolean ) as boolean
declare function CPopupMenu_GetItemChecked( byval hPopup as HWND, byval id as long ) as boolean
declare function CPopupMenu_SetItemText( byval hPopup as HWND, byval id as long, byval Caption as DWSTRING, byval Accel as DWSTRING = "" ) as boolean

' ----------------------------------------------------------------------------------------
' Geometry (derived; the queries force a pending layout so results are always current).
'   width  = max( nMinWidth, checkCol + widest caption + accel block + chevronCol )
'            where the accel block = accelGap + widest accel, only if any row has one
'   height = sum of the row heights + 2*vPadding, plus the border on both axes, where a
'            row is itemHeight unless it is a SEPARATOR, which gets separatorHeight
' GetItemRect is by INDEX (client coords) -- geometry is per-row, ids are for state.
' SetMinWidth floors the CONTENT width in pixels (an MRU menu that must not shrink);
' SetItemHeight / SetSeparatorHeight take raw pixels (the caller DPI-scales, e.g. for a
' compact mode). Set the separator height equal to the item height to get the old
' uniform-row look back.
' ----------------------------------------------------------------------------------------
declare function CPopupMenu_GetItemRect( byval hPopup as HWND, byval idx as long, byref rc as RECT ) as boolean
' Which row is this CLIENT point over? -1 for none (padding, border, or outside). The
' inverse of GetItemRect, and the same routine the mouse handlers use -- exposed so the
' rect/hit-test agreement can be asserted rather than eyeballed now that rows differ in
' height.
declare function CPopupMenu_HitTest( byval hPopup as HWND, byval x as long, byval y as long ) as long
declare function CPopupMenu_GetIdealSize( byval hPopup as HWND, byref nWidth as long, byref nHeight as long ) as boolean
declare function CPopupMenu_SetMinWidth( byval hPopup as HWND, byval nMinWidth as long ) as boolean
declare sub      CPopupMenu_SetItemHeight( byval hPopup as HWND, byval nItemHeight as long )
' SetSeparatorHeight is the ROW's height (the air around the rule); SetSeparatorThickness
' is the rule's own weight in pixels. They are independent: a thick rule in a short row
' and a hairline in a tall row are both reasonable. Thickness is not DPI-scaled for you.
declare sub      CPopupMenu_SetSeparatorHeight( byval hPopup as HWND, byval nSeparatorHeight as long )
declare sub      CPopupMenu_SetSeparatorThickness( byval hPopup as HWND, byval nThickness as long )

' ----------------------------------------------------------------------------------------
' Appearance. Fonts are borrowed, never owned: keep them alive and destroy them
' yourself. hTextFont is also the MEASURING font. The glyph strings are drawn with
' hGlyphFont (tiko passes Segoe Fluent Icons glyphs + that font); the defaults are
' plain Unicode characters that render in any text font.
' ----------------------------------------------------------------------------------------
declare sub      CPopupMenu_SetColors( byval hPopup as HWND, byval pColors as CPOPUPMENU_COLORS ptr )
declare sub      CPopupMenu_SetFonts( byval hPopup as HWND, byval hTextFont as HFONT, byval hGlyphFont as HFONT = 0 )
declare sub      CPopupMenu_SetGlyphs( byval hPopup as HWND, byval wszCheck as DWSTRING, byval wszChevron as DWSTRING )

' ----------------------------------------------------------------------------------------
' Show / dismiss. Coordinates are SCREEN pixels; both Show forms are non-blocking
' (TrackPopupMenu-style usage, without the modal loop). Showing fires the popup's
' InitPopup callback, runs any pending layout, sizes the window, clamps it to the work
' area (bottom clamp; PM_ALIGN_RIGHT flips to the anchor's left edge) and shows it
' WITHOUT activation. Showing a popup as a root closes any other open chain first.
'   Show:        at a point (context-menu door; also selects nothing).
'   ShowForRect: against an anchor rect (PM_ALIGN_BELOW = dropdown, PM_ALIGN_RIGHT =
'                submenu). CMenuBar uses this; hosts rarely need it directly.
'   Hide:        this level and any open descendants. SILENT.
'   CloseChain:  the whole open chain containing hPopup (any level). SILENT.
'   IsOpen:      is this popup currently visible?
' ----------------------------------------------------------------------------------------
declare function CPopupMenu_Show( byval hPopup as HWND, byval x as long, byval y as long ) as boolean
declare function CPopupMenu_ShowForRect( byval hPopup as HWND, byval rcAnchor as RECT, byval nAlign as long ) as boolean
declare sub      CPopupMenu_Hide( byval hPopup as HWND )
declare sub      CPopupMenu_CloseChain( byval hPopup as HWND )
declare function CPopupMenu_IsOpen( byval hPopup as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Selection. SILENT (only user interaction fires callbacks). SetCurSel accepts -1 to
' clear; SelectFirst puts the selection on the first selectable row (skipping
' separators and disabled rows) -- what keyboard-opening a menu wants.
' ----------------------------------------------------------------------------------------
declare function CPopupMenu_GetCurSel( byval hPopup as HWND ) as long
declare function CPopupMenu_SetCurSel( byval hPopup as HWND, byval idx as long ) as boolean
declare function CPopupMenu_SelectFirst( byval hPopup as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Plumbing.
'   FilterMessage    - THE message-pump hook (see the contract at the top). Accepts any
'                      handle in the popup's chain. Returns TRUE when the message was
'                      consumed (the pump must then skip Translate/Dispatch). While a
'                      chain is open it consumes every WM_KEYDOWN/WM_KEYUP/WM_CHAR
'                      (stray typing must not reach the host's editor) EXCEPT the
'                      WM_SYSKEY* family, which stays the host's (Alt handling).
'                      Outside-click dismissal returns FALSE on purpose: the click
'                      that dismissed the menu still reaches its target, tiko-style.
'   SetNotifyWindow  - where CPM_NOTIFY_* messages go (CMenuBar sets itself here).
'   Callbacks        - see the type declarations above for each contract.
' ----------------------------------------------------------------------------------------
declare function CPopupMenu_FilterMessage( byval hPopup as HWND, byval pMsg as MSG ptr ) as boolean
declare sub      CPopupMenu_SetNotifyWindow( byval hPopup as HWND, byval hNotifyWnd as HWND )
declare sub      CPopupMenu_SetSelectCallback( byval hPopup as HWND, byval usersub as PM_SelectCallbackSub )
declare sub      CPopupMenu_SetInitPopupCallback( byval hPopup as HWND, byval usersub as PM_InitPopupCallbackSub )
declare sub      CPopupMenu_SetPaintItemCallback( byval hPopup as HWND, byval usersub as PM_PaintItemCallbackSub )
