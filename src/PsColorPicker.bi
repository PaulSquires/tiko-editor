'    PsColorPicker - reusable owner-drawn colour picker panel
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

'  PsColorPicker.bi  --  a colour picker that is an EMBEDDED PANEL, not a dialog and not a
'  dropdown. The host places it like any other child control and it stays on screen while the
'  user works, which is what makes live preview feel like direct manipulation rather than a
'  round trip through a modal.
'
'  WHY A PANEL, AND WHAT THAT BUYS
'    The family's floating controls -- PsComboBox, PsDatePicker -- own a second top-level
'    window, and every one of them therefore imposes a *_FilterMessage call on the host's
'    message pump. Getting that wrong is the single most common integration bug in this
'    family. A panel owns no popup, takes no capture beyond its own drags, and so
'    **ADDS NO PUMP OBLIGATION AT ALL** -- the PsButton/PsCheckBox property. Preserving that
'    is the reason for the next decision too.
'
'  THE ENTRY FIELDS ARE HAND-ROLLED
'    R/G/B/A and hex are drawn and key-handled by this control, NOT by embedded PsTextBox
'    children. Wrapping PsTextBox would inherit its caret, selection, undo and context menu
'    for free -- and would also inherit its pump obligation, making this a wrap-a-real-child
'    control and undoing the paragraph above. The fields here accept digits, Backspace,
'    Delete, arrows, Home/End and Tab, and nothing else. That is deliberately less than a
'    real edit control: no selection, no clipboard, no undo. If a host needs those, it wants
'    a PsTextBox next to the picker, not inside it.
'
'  TABS
'    Web / System / Custom, after the reference design, plus a fourth PALETTE tab listing the
'    host's own named colours -- see the callbacks below; it is hidden unless the host supplies
'    them.
'
'    THREE OF THE FOUR ARE LISTS. Web, System and Palette are scrolling lists of NAMED rows --
'    a swatch, then the name -- and they share one renderer and one hit test. Only Custom is a
'    grid, and its cells are unnamed because it is a LAW rather than a table.
'
'    An earlier version drew Web and System as 6x8 grids of anonymous swatches. That fitted the
'    panel neatly and told the user nothing: a named colour without its name is just a swatch,
'    and the names are most of what those two tabs are for. The lists need a scrollbar (137 web
'    entries), which is why this control has exactly one child window.
'
'    ONE DEPARTURE FROM THE REFERENCE, AND IT IS DELIBERATE. The entry fields and the
'    Initial/Current pair are drawn on EVERY tab, not only on Custom. Two reasons, and the
'    second is structural: a value readout that disappears when you go looking for a colour is
'    the wrong way round, and PsColorPicker_GetIdealSize is contracted to vary with ShowAlpha
'    and ShowPaletteTab ONLY. A per-tab body would make the panel resize as the user browsed,
'    which for an EMBEDDED control means the host's layout moving underneath it. The BODY is
'    what the tab switches; everything below it is fixed. (PsDatePicker's fixed 6x7 day grid is
'    the same decision for the same reason.)
'
'  INITIAL vs CURRENT
'    The picker shows both, stacked, exactly as the reference does. "Initial" is whatever
'    PsColorPicker_SetColor last established as the baseline; "Current" is the live value.
'    That pair IS the visual form of a Reset button, and it needs no extra host state.
'
'  FOCUS IS ON THIS WINDOW, NOT ON A CHILD
'    Because the fields are hand-rolled there is no inner child to hold the caret, so
'    GetFocus() = hCtrl is TRUE when this control is focused. That is worth stating because the
'    other wrap-a-child siblings are the opposite: PsNumericUpDown and PsDatePicker both need a
'    *_HasFocus helper precisely because their focus sits two levels down. This control needs
'    none, and does not have one.

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0 in namespace
' AfxNova, and every host in this family says "using AfxNova" -- so ANY identifier named "ok"
' becomes a duplicate definition. The family convention is bOK.
#include once "PsBufferPaint.bi"
' THE PICKER'S ONE CHILD WINDOW, and the only one it will ever have. The Web, System and Palette
' tabs are scrolling LISTS, and hand-rolling a scrollbar to avoid a child would have produced a
' worse one that did not match the scrollbars beside it in a host.
'
' IT DOES NOT COST THE NO-PUMP-OBLIGATION PROPERTY, which is the thing actually worth protecting:
' PsVScrollBar has no *_FilterMessage of its own. Nor does it make this control a tab CONTAINER --
' a scrollbar is never a tabstop and never takes focus, so the picker stays a tab ITEM and keeps
' its explicit dwExStyle of 0. Both facts are asserted in the self-test rather than remembered.
#include once "PsVScrollBar.bi"


' ----------------------------------------------------------------------------------------
' Timers. Ids are per-window, so every instance can share these.
'
' HOTTRACK is the family's standard safety net: WM_MOUSELEAVE (TME_LEAVE) is not reliably
' delivered on a fast exit, so a periodic cursor check stops the control being stranded painted
' hot. CARET is new to this control and runs ONLY while a field has focus -- an idle picker
' burns no timer at all.
' ----------------------------------------------------------------------------------------
#define IDT_PSCOLORPICKER_HOTTRACK    &hCBC0
#define PSCOLORPICKER_HOTTRACK_MS      100
#define IDT_PSCOLORPICKER_CARET       &hCBC1

' Stamped into the first field of every PSCOLORPICKER and checked before any other field is
' read. PsOptionButton's precedent.
#define PSCOLORPICKER_SIGNATURE   &h434C5250    ' 'CLRP'


' ----------------------------------------------------------------------------------------
' Default geometry, in unscaled pixels. DPI scaling is applied once at Create; every setter
' afterwards takes raw pixels and the caller scales (the family rule).
'
' THE TWO PEN WIDTHS ARE DELIBERATELY EXCLUDED FROM THAT SCALING, for the reason PsOptionButton
' records: they are handed to PaintRoundOutline / PaintLine, WHICH SCALE THE PEN THEMSELVES.
' Scaling here as well would give a two-pixel hairline at 175% where one was asked for.
' ----------------------------------------------------------------------------------------
#define PSCOLORPICKER_DEFAULT_PAD           8
#define PSCOLORPICKER_DEFAULT_GAP           6
#define PSCOLORPICKER_DEFAULT_TABHEIGHT    24

' The tint/shade matrix. The grid is an EXACT multiple of the cell size, which is what makes
' the point->cell arithmetic a plain division with no rounding case to get wrong.
' 14 ROWS OF 22, WHICH IS EXACTLY nListRows * nRowH -- and that is not a coincidence, it is the
' point. The body is sized by whichever tab needs more (see LayoutColorPicker), so a matrix that
' was shorter than the lists would leave a band of dead panel under it on the Custom tab. Making
' the two agree costs nothing here: the matrix is a LAW rather than a table, so 14 rows is simply
' a finer lightness ramp than 12 was.
#define PSCOLORPICKER_DEFAULT_MATRIXCOLS   16
#define PSCOLORPICKER_DEFAULT_MATRIXROWS   14
#define PSCOLORPICKER_DEFAULT_CELLW        18
#define PSCOLORPICKER_DEFAULT_CELLH        22

' The Web / System / Palette LISTS. One row per entry: a swatch of nSwatchW, then the name.
'
' nListRows is how many rows are VISIBLE, and it is what sizes the body -- 137 named web colours
' will never fit, so this is the number that decides whether the list is comfortable to scan or a
' letterbox. It is also the page size handed to the scrollbar.
#define PSCOLORPICKER_DEFAULT_SWATCHW      34
#define PSCOLORPICKER_DEFAULT_LISTROWS     14
#define PSCOLORPICKER_DEFAULT_SCROLLW      14

#define PSCOLORPICKER_DEFAULT_PREVIEWW     58
#define PSCOLORPICKER_DEFAULT_PREVIEWH     26
#define PSCOLORPICKER_DEFAULT_FIELDW       44
#define PSCOLORPICKER_DEFAULT_FIELDH       22
#define PSCOLORPICKER_DEFAULT_HEXW         92
#define PSCOLORPICKER_DEFAULT_LABELW       18
#define PSCOLORPICKER_DEFAULT_ROWH         22

' Curvature is an ellipse DIAMETER, as RoundRect took it -- the vocabulary every method in
' PsBufferPaint uses.
#define PSCOLORPICKER_DEFAULT_CURVATURE     4
#define PSCOLORPICKER_DEFAULT_BORDERTHICK   1   ' NOT scaled here; see above
#define PSCOLORPICKER_DEFAULT_FOCUSTHICK    1   ' NOT scaled here; see above

' How far the hot/selected ring is inset into a swatch, so the ring reads as being ON the
' swatch rather than as a gap around it.
#define PSCOLORPICKER_RINGINSET             1


' ----------------------------------------------------------------------------------------
' Which tab is showing. PALETTE is only reachable when the host has supplied entries.
' ----------------------------------------------------------------------------------------
enum
    CLR_TAB_WEB = 0
    CLR_TAB_SYSTEM
    CLR_TAB_CUSTOM
    CLR_TAB_PALETTE
end enum

' ----------------------------------------------------------------------------------------
' Phase of a change, mirroring PsSplitter's SPL_PosChangedCallback. The distinction matters
' here more than anywhere else in the family: a host driving a LIVE PREVIEW wants to coalesce
' MOVE notifications and only commit on END, and it cannot do that if every notification looks
' identical. Dragging inside the matrix emits BEGIN, many MOVEs, then END; typing in a field
' or clicking a swatch emits a single END.
' ----------------------------------------------------------------------------------------
enum
    CLR_PHASE_BEGIN = 0
    CLR_PHASE_MOVE
    CLR_PHASE_END
end enum

' Parts, for the paint callback and for hit-test assertions.
enum
    CLR_PART_TABS = 0
    CLR_PART_MATRIX
    CLR_PART_SWATCHES
    CLR_PART_INITIAL
    CLR_PART_CURRENT
    CLR_PART_FIELDS
    CLR_PART_ALPHA
end enum

' ----------------------------------------------------------------------------------------
' The entry fields, in Tab order. THE ORDER IS THE CONTRACT: Tab walks this enum upward and
' Shift+Tab downward, and running off either end is NOT claimed, so the host's own tab order
' takes over. A picker that swallowed Tab forever would be a trap in exactly the dialog it is
' meant to live in.
'
' A is present in the enum whether or not the alpha field is SHOWN -- visibility is a runtime
' question (PsColorPicker_ShowAlpha) and the field walker skips it. Keeping the id stable means
' a stored "which field had focus" never means something different after ShowAlpha flips.
'
' THE ENUM ORDER IS ALSO THE VISUAL ORDER -- R G B on the first row, A then Hex on the second --
' and the layout is written to keep it that way. Laying Hex out before A would have been the
' more natural reading of the reference screenshot and would have made Tab jump backwards
' across the row, which is the kind of defect that is invisible in a screenshot and obvious the
' first time anyone types in it.
' ----------------------------------------------------------------------------------------
enum
    CLR_FIELD_R = 0
    CLR_FIELD_G
    CLR_FIELD_B
    CLR_FIELD_A
    CLR_FIELD_HEX
    CLR_FIELD_COUNT
end enum


' ----------------------------------------------------------------------------------------
' Colours. Flat struct, family convention: the host fills it and calls SetColors, the control
' copies and never reads a host global.
'
' NOTE these dress the picker's own CHROME. The colours it is EDITING are the content, and
' are never themed.
' ----------------------------------------------------------------------------------------
type PSCOLORPICKER_COLORS
    BackColor           as COLORREF = BGR( 44, 49, 58)
    ForeColor           as COLORREF = BGR(200,205,214)
    BorderColor         as COLORREF = BGR( 68, 74, 86)
    TabBackColor        as COLORREF = BGR( 44, 49, 58)
    TabBackColorSel     as COLORREF = BGR( 55, 61, 72)
    TabForeColor        as COLORREF = BGR(150,156,167)
    TabForeColorSel     as COLORREF = BGR(226,230,238)
    FieldBackColor      as COLORREF = BGR( 33, 37, 43)
    FieldForeColor      as COLORREF = BGR(200,205,214)
    FieldBorderColor    as COLORREF = BGR( 68, 74, 86)
    FieldBorderColorSel as COLORREF = BGR( 97,175,239)
    ' The ring drawn around the swatch under the cursor, and around the selected one. Kept
    ' distinct from the swatch fill so a selection is visible on a swatch of any colour --
    ' including one identical to the panel behind it.
    HotRingColor        as COLORREF = BGR(255,255,255)
    SelRingColor        as COLORREF = BGR( 97,175,239)
    FocusRingColor      as COLORREF = BGR( 97,175,239)
end type


' ----------------------------------------------------------------------------------------
' Everything the painter needs. Every rect is precomputed by LayoutColorPicker -- never
' re-derive one from another.
'
' rcMatrix and rcSwatches ARE MUTUALLY EXCLUSIVE: whichever the current tab does not use is
' EMPTY. That is what lets a paint callback branch on IsRectEmpty rather than having to
' re-implement the tab-to-body mapping. rcBody is the union and is never empty.
' ----------------------------------------------------------------------------------------
type PSCOLORPICKER_PAINTINFO
    hColorPicker  as HWND              ' the control, so the callback can query it
    b             as PsBufferPaint ptr ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed ---
    rcClient      as RECT
    rcContent     as RECT              ' rcClient deflated by the padding
    rcTabs        as RECT              ' the whole tab strip
    rcBody        as RECT              ' the tab's content area. FIXED HEIGHT across all tabs
    rcMatrix      as RECT              ' the tint/shade grid, or EMPTY when not on CUSTOM
    rcSwatches    as RECT              ' the Web/System/Palette grid, or EMPTY when on CUSTOM
    rcAssign      as RECT              ' the palette Assign button, or EMPTY off the PALETTE tab
    rcInitial     as RECT              ' the baseline preview
    rcCurrent     as RECT              ' the live preview
    rcFields      as RECT              ' the whole entry block, previews excluded
    rcAlpha       as RECT              ' the alpha field's box, or EMPTY when alpha is hidden
    ' --- State: the control decides these, you only render. ---
    clrCurrent    as COLORREF
    nAlphaCurrent as ubyte
    clrInitial    as COLORREF
    nAlphaInitial as ubyte
    nTab          as long              ' CLR_TAB_*
    isEnabled     as boolean
    isFocused     as boolean
    isDragging    as boolean           ' a live matrix drag
    nFocusField   as long              ' CLR_FIELD_*, or -1 when no field has the caret
end type

type PSCOLORPICKER_MESSAGEINFO
    hColorPicker  as HWND
    uMsg          as UINT
    wParam        as WPARAM
    lParam        as LPARAM
end type


' ----------------------------------------------------------------------------------------
' Callbacks. CLR_ prefix -- unclaimed across the family (checked against the list in
' CLAUDE.md's control-idiom section).
' ----------------------------------------------------------------------------------------

' The value changed. nPhase says whether this is one of many during a drag or a settled
' value; see the CLR_PHASE_* note above. Fires for USER action only -- PsColorPicker_SetColor
' is silent, which is the family's programmatic-setters-are-silent rule and is what lets a
' host call the setter from inside this very callback without re-entering.
type CLR_ColorChangedCallbackSub as sub( byval hCtrl as HWND, _
                                         byval clr as COLORREF, _
                                         byval nAlpha as ubyte, _
                                         byval nPhase as long )

' The user switched tabs. Reported because a host may want to persist the last tab.
type CLR_TabChangedCallbackSub as sub( byval hCtrl as HWND, byval nTab as long )

' PALETTE TAB. The host owns its named colours entirely -- this control never invents,
' renames or stores one. GetCount/GetEntry are pulled at paint time so a palette that changes
' underneath does not need an invalidation protocol.
type CLR_PaletteCountCallbackFunc as function( byval hCtrl as HWND ) as long
type CLR_PaletteEntryCallbackFunc as function( byval hCtrl as HWND, _
                                               byval idx as long, _
                                               byref wszName as DWSTRING, _
                                               byref clr as COLORREF ) as boolean

' The user asked to bind the current key to palette entry idx, or picked one to apply.
' ASSIGN is the deliberate counterpart to detach-on-edit: without it a palette can only ever
' decay, because every edit detaches and nothing re-attaches. The control only reports the
' intent; the host decides what binding means.
type CLR_PaletteAssignCallbackSub as sub( byval hCtrl as HWND, byval idx as long )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' CAUTION: the result is IGNORED for three messages.
'   WM_LBUTTONUP        - the control holds mouse capture across a matrix drag and the
'                         up-message is what releases it: a callback that suppressed it would
'                         strand capture and route every subsequent click here (the PsListBox
'                         bug recorded in Learnings.md).
'   WM_SETFOCUS         - focus is a FACT the system reports, not an action to veto. It is
'   WM_KILLFOCUS          also what COMMITS a half-typed field, so suppressing it would
'                         silently discard the user's typing.
type CLR_MessageCallbackFunc as function( byval m as PSCOLORPICKER_MESSAGEINFO ptr ) as boolean

' Replace the built-in painter wholesale, as every sibling allows.
'
' The control has already filled the client with BackColor before calling you.
'
' DO NOT reach for PaintBorderRect to draw an outline. It FILLS unconditionally, so used as a
' frame it erases everything beneath -- a mistake this family has made FOUR separate times,
' most spectacularly in PsDatePicker where it rendered an entire popup calendar as one flat
' blue rectangle. PaintRoundOutline strokes without filling.
' PsColorPicker_CountRenderedTones exists so you can ASSERT you have not done it.
type CLR_PaintCallbackSub as sub( byval p as any ptr )


' ----------------------------------------------------------------------------------------
' The control's state block.
' ----------------------------------------------------------------------------------------
type PSCOLORPICKER
    ' MUST BE FIRST and must be checked before any other field is read.
    dwSignature      as DWORD = PSCOLORPICKER_SIGNATURE
    hWin             as HWND
    hParent          as HWND
    idc_ColorPicker  as long = 0
    id               as long = 0
    itemData         as integer = 0

    ' --- The value ---
    clrCurrent       as COLORREF = 0
    nAlphaCurrent    as ubyte = 255
    clrInitial       as COLORREF = 0
    nAlphaInitial    as ubyte = 255

    ' --- Tabs ---
    nTab             as long = CLR_TAB_CUSTOM
    bShowPalette     as boolean = false
    bShowAlpha       as boolean = false
    ' WHICH CELL OF THE CURRENT TAB'S BODY IS SELECTED, or -1. ONE field, not one per tab, and
    ' that is deliberate: a per-tab selection would be four places for "what is picked" to
    ' disagree with clrCurrent. It is RE-DERIVED from the colour by PsColorPicker_SyncBodySel
    ' whenever the colour or the tab changes -- so a colour set programmatically still lights
    ' up its cell, and a colour that is not on the current tab lights up nothing (-1), which is
    ' the honest answer rather than a stale ring left over from another tab.
    nBodySel         as long = -1

    ' --- The list scrollbar. Created once, moved and re-ranged per tab, and HIDDEN on Custom
    '     (the matrix always fits by construction). nScrollTop is the index of the first VISIBLE
    '     row -- rows, not pixels, because the list scrolls a whole row at a time and a pixel
    '     offset would let a half-row sit at the top of the viewport. ---
    hScroll          as HWND
    nScrollTop       as long = 0
    ' "Bring the selected row into view at the next paint." A FLAG rather than a direct call,
    ' because the scroll-into-view arithmetic needs the viewport height, and at the moment the
    ' colour or the tab changes the layout is dirty and rcSwatches is still describing the
    ' PREVIOUS tab. Deferring it to the paint is what makes it read the right rect.
    bScrollToSel     as boolean = true

    ' --- State ---
    isEnabled        as boolean = true
    isFocused        as boolean = false
    isDragging       as boolean = false
    hotTimerOn       as boolean = false
    nHotTab          as long = -1
    nHotSwatch       as long = -1     ' index into the current grid, -1 = none
    nHotField        as long = -1
    bHotAssign       as boolean = false

    ' --- The one edit buffer. THERE IS EXACTLY ONE, and that is a design statement rather
    '     than an economy: only the focused field can be edited, so a per-field buffer would
    '     be five places for the displayed text and the committed value to disagree. Entering
    '     a field seeds it from the value; leaving commits it. ---
    nFocusField      as long = -1     ' CLR_FIELD_*, or -1
    wszEdit          as DWSTRING
    ' SELECT-ALL-ON-ENTRY, WITHOUT A SELECTION MODEL. The buffer is seeded from the value when a
    ' field is entered, so the field never reads as blank -- but the FIRST accepted character
    ' then replaces it wholesale, which is what a real spinner's select-all-on-focus does from
    ' the user's point of view. Without this, clicking R (showing "0") and typing 128 leaves
    ' "012" and commits 12, because the three-character limit eats the last digit: measured, and
    ' the only reason it was not shipped is that an assertion typed the digits rather than
    ' describing them. Any editing key (Backspace, Delete, an arrow, Home/End) clears the flag,
    ' because those say "amend" rather than "replace".
    bEditFresh       as boolean = false
    nCaret           as long = 0
    bCaretOn         as boolean = false
    caretTimerOn     as boolean = false

    ' --- Appearance ---
    ' Host-supplied words. This control does no localization, so anything a user reads is
    ' settable. Defaults are English and are assigned at Create, not here, because a DWSTRING
    ' field default and a Create-time assignment would be two places to change them.
    wszAssign        as DWSTRING
    wszInitialLabel  as DWSTRING
    wszCurrentLabel  as DWSTRING
    colors           as PSCOLORPICKER_COLORS
    ' NOT named hFont: FreeBASIC is case-insensitive, so a member called hFont shadows the
    ' TYPE name HFONT inside every member procedure of this type (C:\dev\Learnings.md).
    hTextFont        as HFONT

    ' --- Layout inputs. All pixels; DPI-scaled once at Create EXCEPT the two pens. ---
    nPad             as long = PSCOLORPICKER_DEFAULT_PAD
    nGap             as long = PSCOLORPICKER_DEFAULT_GAP
    nTabHeight       as long = PSCOLORPICKER_DEFAULT_TABHEIGHT
    nMatrixCols      as long = PSCOLORPICKER_DEFAULT_MATRIXCOLS
    nMatrixRows      as long = PSCOLORPICKER_DEFAULT_MATRIXROWS
    nCellW           as long = PSCOLORPICKER_DEFAULT_CELLW
    nCellH           as long = PSCOLORPICKER_DEFAULT_CELLH
    nSwatchW         as long = PSCOLORPICKER_DEFAULT_SWATCHW
    nListRows        as long = PSCOLORPICKER_DEFAULT_LISTROWS
    nScrollBarW      as long = PSCOLORPICKER_DEFAULT_SCROLLW
    nPreviewW        as long = PSCOLORPICKER_DEFAULT_PREVIEWW
    nPreviewH        as long = PSCOLORPICKER_DEFAULT_PREVIEWH
    nFieldW          as long = PSCOLORPICKER_DEFAULT_FIELDW
    nFieldH          as long = PSCOLORPICKER_DEFAULT_FIELDH
    nHexW            as long = PSCOLORPICKER_DEFAULT_HEXW
    nLabelW          as long = PSCOLORPICKER_DEFAULT_LABELW
    nRowH            as long = PSCOLORPICKER_DEFAULT_ROWH
    nCurvature       as long = PSCOLORPICKER_DEFAULT_CURVATURE
    nBorderThick     as long = PSCOLORPICKER_DEFAULT_BORDERTHICK   ' NOT scaled; see above
    nFocusThick      as long = PSCOLORPICKER_DEFAULT_FOCUSTHICK    ' NOT scaled; see above

    ' --- Layout outputs. DERIVED, never set from outside. Layout is lazy: mutators mark it
    '     dirty, the next paint (or any rect/size query) runs it, which coalesces a burst of
    '     mutations into ONE pass. ---
    nIdealW          as long = 0
    nIdealH          as long = 0
    rcContent        as RECT
    rcTabs           as RECT
    rcBody           as RECT
    rcMatrix         as RECT
    rcSwatches       as RECT        ' the LIST area on Web / System / Palette, scrollbar excluded
    rcScroll         as RECT        ' where the PsVScrollBar child sits; empty on Custom
    rcAssign         as RECT
    rcInitial        as RECT
    rcCurrent        as RECT
    rcFields         as RECT
    rcAlpha          as RECT
    rcTab(0 to 3)      as RECT
    rcFieldBox(0 to CLR_FIELD_COUNT - 1)   as RECT
    rcFieldLabel(0 to CLR_FIELD_COUNT - 1) as RECT
    bLayoutDirty     as boolean = true

    ColorChangedCallback as CLR_ColorChangedCallbackSub
    TabChangedCallback   as CLR_TabChangedCallbackSub
    PaletteCountCallback as CLR_PaletteCountCallbackFunc
    PaletteEntryCallback as CLR_PaletteEntryCallbackFunc
    PaletteAssignCallback as CLR_PaletteAssignCallbackSub
    MessageCallback      as CLR_MessageCallbackFunc
    PaintCallback        as CLR_PaintCallbackSub

    declare sub      LayoutColorPicker()
    declare sub      Refresh()
    declare sub      CancelDrag()
    declare function TabCount() as long
    declare function IsFieldVisible( byval nField as long ) as boolean
end type


' ========================================================================================
' PURE COLOUR FUNCTIONS
'
' These take no control pointer and touch no global, which is the whole point: the matrix's
' colour law and the hex round trip are the two things in this control most likely to be
' subtly wrong, and a pure function's truth table can be asserted directly instead of only
' being observable from inside a WM_PAINT. (PsProgressBar's ComputeBarLength precedent.)
' ========================================================================================

' One channel of the HSL->RGB conversion. Standard formulation; t is a hue offset that is
' allowed to fall outside 0..1 and is wrapped here rather than at each of the three call sites.
'
' DOUBLE, NOT SINGLE, AND THAT IS A BUG FIX RATHER THAN A PREFERENCE. In single precision
' `0.0 - 1/3 + 1.0` lands a hair BELOW 2/3, so a pure red took the third branch instead of
' returning p, and its blue channel came out as 1 rather than 0. Measured: HSL(0,1,0.5) gave
' 255,0,1 and HSL(1/3,1,0.5) gave 0,255,1. The pure primaries are the one place this has to be
' exact, because they are what a user typing #FF0000 and then clicking the matrix compares.
'
' IT IS ONE OF TWO REDUNDANT FIXES, AND THE REDUNDANCY WAS MEASURED RATHER THAN ASSUMED. The
' rounding below changed from clng() to int(x+0.5) at the same time, and an A/B of all four
' combinations showed that EITHER change alone is sufficient -- the bug needed single precision
' AND half-to-even rounding together. Both are kept because each is independently the right
' thing, but that means neither can be falsified by the self-test on its own: the assertions on
' the primaries are what actually guards this, not the redundancy. (A third fix, an epsilon snap
' of each channel to 0 or 1, was written and then REMOVED -- it was arbitrary, it could not be
' made to fail, and unfalsifiable defensive code is the thing this family keeps learning to
' delete.)
private function PsColorPicker_HueChannel( _
            byval p as double, _
            byval q as double, _
            byval t as double _
            ) as double

    if t < 0.0 then t += 1.0
    if t > 1.0 then t -= 1.0
    if t < (1.0 / 6.0) then return p + (q - p) * 6.0 * t
    if t < (1.0 / 2.0) then return q
    if t < (2.0 / 3.0) then return p + (q - p) * ((2.0 / 3.0) - t) * 6.0
    return p
end function


' h, s and lum are all 0..1 (h is a FRACTION of the circle, not degrees -- the matrix indexes by
' column, so degrees would only be a unit to convert back out of).
'
' LUM, NOT L, AND THAT IS AN ADOPTION BUG RATHER THAN A STYLE CHOICE. tiko defines a
' preprocessor macro `L(e,s)` for localized strings, FreeBASIC is case-insensitive, and a macro
' applies everywhere -- so a parameter named `l` became a duplicated definition the moment this
' file was vendored into that host, with errors ("Duplicated definition, at parameter 3") that
' name neither the macro nor the collision. It compiled perfectly in its own repo.
'
' The standing lesson: a SINGLE-LETTER identifier in a header meant to be vendored is a name
' collision waiting for a host that happens to have used it. This is the same shape as `fb`,
' `bIn`, `width`, `step` and `hFont` -- with the extra sting that no amount of testing inside
' this repo could have found it.
function PsColorPicker_HSLtoRGB( _
            byval h as double, _
            byval s as double, _
            byval lum as double _
            ) as COLORREF

    if h < 0.0 then h = 0.0
    if h > 1.0 then h = 1.0
    if s < 0.0 then s = 0.0
    if s > 1.0 then s = 1.0
    if lum < 0.0 then lum = 0.0
    if lum > 1.0 then lum = 1.0

    ' fRed / fGrn / fBlu, NOT fr / fg / fb. "fb" is FreeBASIC's own namespace name and using it
    ' as a variable fails with "error 120: Expected period ('.')", which names the syntax it was
    ' expecting rather than the collision -- the same class of unhelpful message this family has
    ' already hit with `width`, `step` and `hFont` (C:\dev\Learnings.md).
    dim as double fRed, fGrn, fBlu

    if s = 0.0 then
        fRed = lum : fGrn = lum : fBlu = lum
    else
        dim as double q
        if lum < 0.5 then
            q = lum * (1.0 + s)
        else
            q = lum + s - (lum * s)
        end if
        dim as double p = (2.0 * lum) - q
        fRed = PsColorPicker_HueChannel( p, q, h + (1.0 / 3.0) )
        fGrn = PsColorPicker_HueChannel( p, q, h )
        fBlu = PsColorPicker_HueChannel( p, q, h - (1.0 / 3.0) )
    end if

    ' int(x + 0.5): round half UP, and deterministically. A bare cast TRUNCATES, which biases
    ' every channel low and makes the greyscale column miss pure white by one. clng() rounds half
    ' to EVEN, which is the other half of the primaries bug -- a channel that should be 0 but
    ' carries a few ULPs of float residue arrives as 0.5000000001 and rounds AWAY from zero.
    ' int() floors, so 0.5 is always 0 and 255.5 is always 255.
    dim as long nR = clng( int( (fRed * 255.0) + 0.5 ) )
    dim as long nG = clng( int( (fGrn * 255.0) + 0.5 ) )
    dim as long nB = clng( int( (fBlu * 255.0) + 0.5 ) )
    if nR < 0 then nR = 0
    if nR > 255 then nR = 255
    if nG < 0 then nG = 0
    if nG > 255 then nG = 255
    if nB < 0 then nB = 0
    if nB > 255 then nB = 255

    return BGR( nR, nG, nB )
end function


' ========================================================================================
' THE MATRIX'S COLOUR LAW.  Column = hue, row = lightness, LAST COLUMN = greyscale.
'
'   l = 1 - (row + 0.5) / rows       so the top row is a near-white TINT and the bottom row a
'                                    near-black SHADE, and neither end is pure white or pure
'                                    black -- those are reachable from the greyscale column,
'                                    and spending a whole ROW on each would waste two of
'                                    twelve.
'   h = col / (cols - 1)             over the CHROMATIC columns only, so hue 0 appears once
'                                    rather than at both ends.
'
' A LAW, NOT A TABLE, which is what makes the grid resizable: a host that sets a 24x16 matrix
' gets a finer sweep of the same colours rather than a stretched picture of a 16x12 one.
' ========================================================================================
function PsColorPicker_MatrixColorAt( _
            byval nCol  as long, _
            byval nRow  as long, _
            byval nCols as long, _
            byval nRows as long _
            ) as COLORREF

    if nCols < 2 then nCols = 2
    if nRows < 1 then nRows = 1
    if nCol < 0 then nCol = 0
    if nCol > (nCols - 1) then nCol = nCols - 1
    if nRow < 0 then nRow = 0
    if nRow > (nRows - 1) then nRow = nRows - 1

    dim as double lum = 1.0 - ((cdbl(nRow) + 0.5) / cdbl(nRows))

    ' The greyscale column. Saturation 0 makes the hue irrelevant, so it is passed as 0 rather
    ' than as a value that would look meaningful and is not.
    if nCol = (nCols - 1) then return PsColorPicker_HSLtoRGB( 0.0, 0.0, lum )

    dim as double h = cdbl(nCol) / cdbl(nCols - 1)
    return PsColorPicker_HSLtoRGB( h, 1.0, lum )
end function


' Where does this colour sit in the matrix? FALSE when it is not one of the matrix's colours,
' which is the normal case for anything the user typed or picked off another tab -- so a caller
' uses this to decide whether to draw a selection ring at all, not to validate a colour.
function PsColorPicker_MatrixIndexOf( _
            byval clr   as COLORREF, _
            byval nCols as long, _
            byval nRows as long, _
            byref nCol  as long, _
            byref nRow  as long _
            ) as boolean

    nCol = -1 : nRow = -1
    for r as long = 0 to nRows - 1
        for c as long = 0 to nCols - 1
            if PsColorPicker_MatrixColorAt( c, r, nCols, nRows ) = clr then
                nCol = c : nRow = r
                return true
            end if
        next
    next
    return false
end function


' ========================================================================================
' POINT -> CELL, as a pure function of the grid rather than of the control.
'
' Both grids and the palette list go through this, which is what stops the hit test and the
' painter disagreeing about where a cell is -- the defect class that produces "clicking one
' swatch selects its neighbour" and that no amount of looking at the render will show, because
' both halves look right on their own.
'
' The point is in the same coordinate space as rc (client coords, here).
' ========================================================================================
function PsColorPicker_CellFromPoint( _
            byval rc     as RECT, _
            byval nCellW as long, _
            byval nCellH as long, _
            byval nCols  as long, _
            byval nRows  as long, _
            byval x      as long, _
            byval y      as long, _
            byref nCol   as long, _
            byref nRow   as long _
            ) as boolean

    nCol = -1 : nRow = -1
    if (nCellW <= 0) orelse (nCellH <= 0) then return false
    if (nCols  <= 0) orelse (nRows  <= 0) then return false
    if (x < rc.left) orelse (y < rc.top) then return false

    dim as long c = (x - rc.left) \ nCellW
    dim as long r = (y - rc.top)  \ nCellH
    if (c < 0) orelse (c > (nCols - 1)) then return false
    if (r < 0) orelse (r > (nRows - 1)) then return false

    nCol = c : nRow = r
    return true
end function


' ========================================================================================
' HEX PARSE / FORMAT.  #RRGGBB, with or without the '#', case-insensitive.
'
' A COLORREF is 0x00BBGGRR -- the BYTE ORDER IS REVERSED from the way hex colours are written,
' which is exactly the wrong-and-plausible mistake the theme engine's own colour round trip was
' asserted against. Both directions are here so the round trip is one assertion.
' ========================================================================================
function PsColorPicker_FormatHex( byval clr as COLORREF ) as DWSTRING
    dim as string sHex = "#" & _
                  right( "0" & hex( clng(clr) and &hFF ), 2 ) & _
                  right( "0" & hex( (clng(clr) shr 8) and &hFF ), 2 ) & _
                  right( "0" & hex( (clng(clr) shr 16) and &hFF ), 2 )
    dim as DWSTRING wszOut = ucase( sHex )
    return wszOut
end function


function PsColorPicker_ParseHex( byval wszText as DWSTRING, byref clr as COLORREF ) as boolean
    dim as string s = ucase( trim( str( wszText ) ) )
    if left( s, 1 ) = "#" then s = mid( s, 2 )
    if len( s ) <> 6 then return false

    dim as long nVal(0 to 2)
    for i as long = 0 to 2
        dim as long v = 0
        for j as long = 0 to 1
            dim as long ch = asc( mid( s, (i * 2) + j + 1, 1 ) )
            dim as long d
            select case ch
            case asc("0") to asc("9") : d = ch - asc("0")
            case asc("A") to asc("F") : d = (ch - asc("A")) + 10
            case else : return false
            end select
            v = (v * 16) + d
        next
        nVal(i) = v
    next

    clr = BGR( nVal(0), nVal(1), nVal(2) )
    return true
end function


' ========================================================================================
' Black or white, whichever will be legible ON this colour.
'
' The two preview boxes carry their own "Initial" / "Current" labels INSIDE them -- there is no
' room beside them, and a preview that is not labelled is a pair of anonymous rectangles. That
' only works if the label colour follows the fill, so this is load-bearing rather than a nicety.
'
' ITU-R BT.601 luma, which is the weighting that matches perceived brightness closely enough for
' a legibility decision; the sRGB-linear form would be more correct and would change the verdict
' for essentially no colour a user picks. Threshold 140 rather than 128 because white-on-mid is
' harder to read than black-on-mid.
' ========================================================================================
function PsColorPicker_ContrastColor( byval clr as COLORREF ) as COLORREF
    dim as long nR = clng(clr) and &hFF
    dim as long nG = (clng(clr) shr 8) and &hFF
    dim as long nB = (clng(clr) shr 16) and &hFF
    dim as long nLuma = ((299 * nR) + (587 * nG) + (114 * nB)) \ 1000
    if nLuma >= 140 then return BGR(0, 0, 0)
    return BGR(255, 255, 255)
end function


' Which characters this field will accept, and how long it may get. Pure, and separate from the
' key handler, because "the R field takes three digits and the hex field takes a leading # then
' six hex digits" is a rule worth asserting directly rather than by typing into a window.
function PsColorPicker_FieldAccepts( _
            byval nField as long, _
            byval ch     as long, _
            byval nPos   as long _
            ) as boolean

    if nField = CLR_FIELD_HEX then
        dim as boolean bIsHash = (ch = asc("#"))
        if bIsHash then return (nPos = 0)
        dim as boolean bDigit = ((ch >= asc("0")) andalso (ch <= asc("9")))
        if bDigit then return true
        dim as boolean bLower = ((ch >= asc("a")) andalso (ch <= asc("f")))
        if bLower then return true
        return ((ch >= asc("A")) andalso (ch <= asc("F")))
    end if
    return ((ch >= asc("0")) andalso (ch <= asc("9")))
end function

' "#" plus six digits for hex; three digits for a 0..255 channel.
function PsColorPicker_FieldMaxLen( byval nField as long ) as long
    if nField = CLR_FIELD_HEX then return 7
    return 3
end function


' ========================================================================================
' Inline members.
' ========================================================================================

' How many tabs are on the strip. The PALETTE tab is the only optional one, and it is LAST,
' so a visible index maps straight onto CLR_TAB_* with no translation table.
function PSCOLORPICKER.TabCount() as long
    if this.bShowPalette then return 4
    return 3
end function

' Is this field on screen? Only the alpha field is ever hidden. Kept as a member rather than an
' inline `nField <> CLR_FIELD_A orelse bShowAlpha` at each site because the layout, the painter,
' the Tab walker and the hit test must all agree, and four copies is four chances to diverge.
function PSCOLORPICKER.IsFieldVisible( byval nField as long ) as boolean
    if nField = CLR_FIELD_A then return this.bShowAlpha
    if (nField < 0) orelse (nField >= CLR_FIELD_COUNT) then return false
    return true
end function

' Forget a live drag WITHOUT touching the capture. Releasing capture is the WndProc's job, and
' only on the up-message or WM_DESTROY -- doing it here would let a callback strand or
' double-release it.
sub PSCOLORPICKER.CancelDrag()
    this.isDragging = false
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here.
sub PSCOLORPICKER.Refresh()
    this.bLayoutDirty = true
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' LAYOUT.  Three bands that tile the content area top to bottom, plus a fixed-size body.
'
'   rcContent = rcClient deflated by nPad on all four sides
'   rcTabs    = the top nTabHeight of it, full width; nTabCount equal cells across
'   rcBody    = below it after nGap, full width, and ALWAYS nMatrixRows * nCellH TALL
'   the bottom block, below that after nGap:
'       rcInitial / rcCurrent   stacked at the left, nPreviewW x nPreviewH each
'       row 1:  [R][G][B]       label + box, nGap between groups
'       row 2:  [Hex]  [A]      the alpha group present only when bShowAlpha
'
' THE BODY HEIGHT IS FIXED ACROSS TABS, and that is the load-bearing decision. The Web grid
' needs 5 rows and the System grid 4; letting the body shrink to fit would resize an EMBEDDED
' control as the user browsed tabs, which means the host's layout moving underneath it. So the
' body is sized by the matrix -- the tallest content -- and the shorter grids simply leave the
' bottom of it empty. PsDatePicker fixes its day grid at 6 rows for the identical reason.
'
' WHICH OF rcMatrix / rcSwatches IS EMPTY IS THE TAB'S ONLY LAYOUT EFFECT. Everything else is
' identical on every tab, so a tab switch is an invalidate and not a re-measure. (The layout is
' still marked dirty on a tab change, because the two body rects have to swap.)
'
' THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area, and PsColorPicker_GetIdealSize is exactly what a host calls to decide how big to
' make the control in the FIRST place (PsToggle's rule).
'
' OVERFLOW is honest rather than squeezed: a control narrower than ideal keeps its declared
' cell sizes and lets the right-hand columns fall outside rcBody, where the paint loop clips
' them. Nothing is scaled down to fit, because a half-width swatch is not a smaller swatch, it
' is a wrong one.
' ========================================================================================
sub PSCOLORPICKER.LayoutColorPicker()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    ' ---- the ideal size, which does not depend on the client area ------------------------
    ' THE BODY IS SIZED BY WHICHEVER TAB NEEDS MORE, and then that size is used by ALL of them.
    ' Formerly it was simply the matrix's height; the lists made that too short to scan, so the
    ' rule is now an explicit max(). The property that matters is unchanged and is the reason
    ' there is a rule at all: the body must not RESIZE between tabs, because this is an EMBEDDED
    ' control and a body that grew would move the host's layout underneath it.
    dim as long bodyW = this.nMatrixCols * this.nCellW
    dim as long bodyH = this.nMatrixRows * this.nCellH

    dim as long listH = this.nListRows * this.nRowH
    if listH > bodyH then bodyH = listH

    ' Row 1 is three label+box groups; row 2 is, optionally, the alpha group and then the hex
    ' group. That order matches CLR_FIELD_* -- see the note on that enum.
    dim as long grpRGB = this.nLabelW + this.nFieldW
    dim as long row1W  = (3 * grpRGB) + (2 * this.nGap)
    dim as long row2W  = this.nLabelW + this.nHexW
    if this.bShowAlpha then row2W += grpRGB + this.nGap

    dim as long fieldsW = row1W
    if row2W > fieldsW then fieldsW = row2W

    dim as long bottomW = this.nPreviewW + this.nGap + fieldsW
    dim as long bottomH = (2 * this.nFieldH) + this.nGap
    if (2 * this.nPreviewH) > bottomH then bottomH = 2 * this.nPreviewH

    dim as long innerW = bodyW
    if bottomW > innerW then innerW = bottomW

    this.nIdealW = (2 * this.nPad) + innerW
    this.nIdealH = (2 * this.nPad) + this.nTabHeight + this.nGap + bodyH + this.nGap + bottomH

    ' ---- placement, which does -----------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcContent = rcClient
    InflateRect( @this.rcContent, -this.nPad, -this.nPad )

    ' ---- the tab strip --------------------------------------------------------------------
    SetRect( @this.rcTabs, this.rcContent.left, this.rcContent.top, _
                           this.rcContent.right, this.rcContent.top + this.nTabHeight )

    dim as long nTabs = this.TabCount()
    dim as long stripW = this.rcTabs.right - this.rcTabs.left
    for i as long = 0 to 3
        SetRectEmpty( @this.rcTab(i) )
    next
    if (nTabs > 0) andalso (stripW > 0) then
        for i as long = 0 to nTabs - 1
            ' Edges are computed from the STRIP, not by accumulating a width -- accumulating
            ' leaves a rounding remainder on the last tab and the strip stops meeting its own
            ' right edge.
            dim as long x0 = this.rcTabs.left + ((stripW * i) \ nTabs)
            dim as long x1 = this.rcTabs.left + ((stripW * (i + 1)) \ nTabs)
            SetRect( @this.rcTab(i), x0, this.rcTabs.top, x1, this.rcTabs.bottom )
        next
    end if

    ' ---- the body -------------------------------------------------------------------------
    dim as long bodyTop = this.rcTabs.bottom + this.nGap
    SetRect( @this.rcBody, this.rcContent.left, bodyTop, this.rcContent.right, bodyTop + bodyH )

    SetRectEmpty( @this.rcMatrix )
    SetRectEmpty( @this.rcSwatches )
    SetRectEmpty( @this.rcScroll )
    SetRectEmpty( @this.rcAssign )

    if this.nTab = CLR_TAB_CUSTOM then
        ' EXACTLY the grid, not the whole body: the point->cell division must not be able to
        ' report a cell that is not there.
        SetRect( @this.rcMatrix, this.rcBody.left, this.rcBody.top, _
                                 this.rcBody.left + (this.nMatrixCols * this.nCellW), _
                                 this.rcBody.top + (this.nMatrixRows * this.nCellH) )
    else
        this.rcSwatches = this.rcBody

        if this.nTab = CLR_TAB_PALETTE then
            ' The palette list gives up its bottom strip to the Assign button. Reserved
            ' unconditionally so the list's row count does not change when the host's palette
            ' is empty.
            SetRect( @this.rcAssign, this.rcBody.left, this.rcBody.bottom - this.nFieldH, _
                                     this.rcBody.left + this.nHexW, this.rcBody.bottom )
            this.rcSwatches.bottom = this.rcAssign.top - this.nGap
            if this.rcSwatches.bottom < this.rcSwatches.top then
                this.rcSwatches.bottom = this.rcSwatches.top
            end if
        end if

        ' THE SCROLLBAR STRIP IS RESERVED WHETHER OR NOT A THUMB IS NEEDED, and that is
        ' PsScrollPanel's rule rather than PsListBox's: reclaiming the width when a short list
        ' fits would re-flow every row's name the moment the user switched from Web (137 rows) to
        ' a five-entry palette. A list whose text jumps sideways as you browse reads as a glitch.
        SetRect( @this.rcScroll, this.rcSwatches.right - this.nScrollBarW, this.rcSwatches.top, _
                                 this.rcSwatches.right, this.rcSwatches.bottom )
        this.rcSwatches.right = this.rcScroll.left
        if this.rcSwatches.right < this.rcSwatches.left then
            this.rcSwatches.right = this.rcSwatches.left
        end if
    end if

    ' ---- the previews ---------------------------------------------------------------------
    dim as long botTop = this.rcBody.bottom + this.nGap
    SetRect( @this.rcInitial, this.rcContent.left, botTop, _
                              this.rcContent.left + this.nPreviewW, botTop + this.nPreviewH )
    SetRect( @this.rcCurrent, this.rcContent.left, this.rcInitial.bottom, _
                              this.rcContent.left + this.nPreviewW, this.rcInitial.bottom + this.nPreviewH )

    ' ---- the entry fields -----------------------------------------------------------------
    dim as long fx = this.rcCurrent.right + this.nGap
    dim as long r1 = botTop
    dim as long r2 = botTop + this.nFieldH + this.nGap

    for i as long = 0 to CLR_FIELD_COUNT - 1
        SetRectEmpty( @this.rcFieldBox(i) )
        SetRectEmpty( @this.rcFieldLabel(i) )
    next

    dim as long x = fx
    for i as long = CLR_FIELD_R to CLR_FIELD_B
        SetRect( @this.rcFieldLabel(i), x, r1, x + this.nLabelW, r1 + this.nFieldH )
        SetRect( @this.rcFieldBox(i), x + this.nLabelW, r1, x + this.nLabelW + this.nFieldW, r1 + this.nFieldH )
        x += grpRGB + this.nGap
    next

    x = fx
    if this.bShowAlpha then
        SetRect( @this.rcFieldLabel(CLR_FIELD_A), x, r2, x + this.nLabelW, r2 + this.nFieldH )
        SetRect( @this.rcFieldBox(CLR_FIELD_A), x + this.nLabelW, r2, x + this.nLabelW + this.nFieldW, r2 + this.nFieldH )
        x += grpRGB + this.nGap
    end if
    SetRect( @this.rcFieldLabel(CLR_FIELD_HEX), x, r2, x + this.nLabelW, r2 + this.nFieldH )
    SetRect( @this.rcFieldBox(CLR_FIELD_HEX), x + this.nLabelW, r2, x + this.nLabelW + this.nHexW, r2 + this.nFieldH )

    ' rcAlpha ALIASES the alpha field's box and is empty when alpha is hidden, so a paint
    ' callback and the CLR_PART_ALPHA probe agree with the field walker by construction.
    this.rcAlpha = this.rcFieldBox(CLR_FIELD_A)

    ' The union of everything in the entry block, previews excluded. Built from the rects
    ' rather than re-derived from the arithmetic above, so it cannot drift from them.
    SetRect( @this.rcFields, fx, r1, fx, r2 + this.nFieldH )
    for i as long = 0 to CLR_FIELD_COUNT - 1
        if IsRectEmpty( @this.rcFieldBox(i) ) = 0 then
            if this.rcFieldBox(i).right > this.rcFields.right then
                this.rcFields.right = this.rcFieldBox(i).right
            end if
        end if
    next
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A COLOUR PICKER PANEL: a tab strip, a body that is either a tint/shade matrix or a swatch
' grid, an Initial/Current preview pair, and hand-rolled R/G/B/(A)/hex entry fields.
'
' NO PUMP OBLIGATION
'   There is no PsColorPicker_FilterMessage and THERE MUST NEVER BE ONE. That property is the
'   reason this control is a panel rather than a dropdown, and the reason its entry fields are
'   hand-rolled rather than PsTextBox children. Like PsButton and PsCheckBox, it adds nothing
'   to the host's message loop. Tab NAVIGATION still needs IsDialogMessage in the host pump, as
'   it does for any tabstop, and a host must SetFocus its first control itself.
'
' MOUSE CAPTURE IS TAKEN, FOR ONE GESTURE ONLY
'   The matrix drag. The family's test is "take capture only if something consumes the
'   guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing", and a drag is the clearest positive
'   case there is (PsSplitter's). Tabs, swatches, fields and the Assign button all act on the
'   DOWN or the UP without needing the pairing, and none of them takes capture.
'
' DELIBERATELY ABSENT
'   Tooltips (by request, as in PsComboBox and PsMessageBox). Selection, clipboard and undo in
'   the entry fields -- see the header. An eyedropper. A named-colour list with text (the Web
'   and System tabs are swatch grids). Multi-select. CS_DBLCLKS: a rapid second click on a
'   swatch is a legitimate re-pick, and a double-click on the matrix would swallow one of two
'   deliberate drags.

' ---- Creation --------------------------------------------------------------------------

' Create the picker as a child of hParent. It is created 0x0 and hidden; size it with
' PsColorPicker_GetIdealSize (valid immediately) and show it.
declare function PsColorPicker_Create( byval hParent as HWND, byval id as long = 0 ) as HWND

' The live value. SetColor is SILENT and also re-establishes the "Initial" baseline unless
' bKeepInitial is true -- which is what a host passes while stepping through a list of keys,
' so that Initial keeps meaning "before this editing session" rather than "the previous key".
declare sub      PsColorPicker_SetColor( byval hCtrl as HWND, _
                                         byval clr as COLORREF, _
                                         byval nAlpha as ubyte = 255, _
                                         byval bKeepInitial as boolean = false )
declare function PsColorPicker_GetColor( byval hCtrl as HWND ) as COLORREF
declare function PsColorPicker_GetAlpha( byval hCtrl as HWND ) as ubyte

' The baseline shown as "Initial", and the one-call revert to it. Revert FIRES the change
' callback (it is an action, not a setter -- PsButton_Click's precedent).
declare sub      PsColorPicker_SetInitial( byval hCtrl as HWND, byval clr as COLORREF, byval nAlpha as ubyte = 255 )
declare function PsColorPicker_GetInitial( byval hCtrl as HWND ) as COLORREF
declare sub      PsColorPicker_RevertToInitial( byval hCtrl as HWND )

' ALPHA IS OPT-IN. Hidden by default, because most callers are picking an opaque colour and
' an inert slider reads as a broken one. tiko drives this from its per-key bAlphaCapable flag.
declare sub      PsColorPicker_ShowAlpha( byval hCtrl as HWND, byval bShow as boolean )
declare function PsColorPicker_IsAlphaShown( byval hCtrl as HWND ) as boolean

declare sub      PsColorPicker_SetTab( byval hCtrl as HWND, byval nTab as long )
declare function PsColorPicker_GetTab( byval hCtrl as HWND ) as long
' Hiding the palette tab while it is CURRENT falls back to CLR_TAB_CUSTOM rather than leaving
' the control showing a tab that is no longer on the strip.
declare sub      PsColorPicker_ShowPaletteTab( byval hCtrl as HWND, byval bShow as boolean )
declare function PsColorPicker_IsPaletteTabShown( byval hCtrl as HWND ) as boolean
' Which palette row is highlighted, and therefore what the Assign button would bind to.
'
' RETURNS -1 WHENEVER THE PALETTE TAB IS NOT THE CURRENT ONE, and that is the honest answer
' rather than an omission: the selection is ONE field shared by every tab's body (see nBodySel),
' so "which palette row is selected" genuinely has no meaning while the matrix is showing. The
' setter is likewise only meaningful on that tab. SILENT, both of them.
declare function PsColorPicker_GetPaletteSel( byval hCtrl as HWND ) as long
declare sub      PsColorPicker_SetPaletteSel( byval hCtrl as HWND, byval idx as long )

declare sub      PsColorPicker_SetColors( byval hCtrl as HWND, byval pColors as PSCOLORPICKER_COLORS ptr )
declare sub      PsColorPicker_GetColors( byval hCtrl as HWND, byval pColors as PSCOLORPICKER_COLORS ptr )
declare sub      PsColorPicker_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function PsColorPicker_GetFont( byval hCtrl as HWND ) as HFONT
' The Assign button's caption, and the two preview labels. Settable because this control does
' no localization of its own and a host that has any needs to supply the words -- the family
' rule that a control owns no strings the host might want to translate. Defaults are English.
declare sub      PsColorPicker_SetAssignText( byval hCtrl as HWND, byval Text as DWSTRING )
declare sub      PsColorPicker_SetPreviewText( byval hCtrl as HWND, byval TextInitial as DWSTRING, byval TextCurrent as DWSTRING )
declare sub      PsColorPicker_Refresh( byval hCtrl as HWND )

declare function PsColorPicker_GetEnabled( byval hCtrl as HWND ) as boolean
' Goes through EnableWindow, so the disable is enforced by the system rather than being a
' cosmetic flag. WM_ENABLE syncs the control's own flag back.
declare sub      PsColorPicker_SetEnabled( byval hCtrl as HWND, byval bEnabled as boolean )
declare function PsColorPicker_GetFocused( byval hCtrl as HWND ) as boolean

' Ideal size is VALID BEFORE THE CONTROL IS EVER SIZED (PsToggle's rule), so a host can lay
' out around it on the first pass. It varies with ShowAlpha and ShowPaletteTab.
declare sub      PsColorPicker_GetIdealSize( byval hCtrl as HWND, byref cx as long, byref cy as long )

' The grid geometry. Both are SILENT and both re-measure: changing them changes the ideal size.
declare sub      PsColorPicker_GetMatrixSize( byval hCtrl as HWND, byref nCols as long, byref nRows as long )
declare sub      PsColorPicker_SetMatrixSize( byval hCtrl as HWND, byval nCols as long, byval nRows as long )
declare sub      PsColorPicker_GetCellSize( byval hCtrl as HWND, byref cx as long, byref cy as long )
declare sub      PsColorPicker_SetCellSize( byval hCtrl as HWND, byval cx as long, byval cy as long )

' Rect getters, for a host laying out around the control and for assertions. FALSE means the
' rect is legitimately empty (rcMatrix off the Custom tab, rcAlpha with alpha hidden) or the
' control has no geometry yet.
declare function PsColorPicker_GetPartRect( byval hCtrl as HWND, byval nPart as long, byref rc as RECT ) as boolean
declare function PsColorPicker_GetFieldRect( byval hCtrl as HWND, byval nField as long, byref rc as RECT ) as boolean
declare function PsColorPicker_GetTabRect( byval hCtrl as HWND, byval idx as long, byref rc as RECT ) as boolean

' Hit tests, public so a host can reason about its own paint callback and so the self-test can
' assert the click map round-trips against the layout. All take CLIENT coordinates.
declare function PsColorPicker_HitTestTab( byval hCtrl as HWND, byval x as long, byval y as long ) as long
declare function PsColorPicker_HitTestField( byval hCtrl as HWND, byval x as long, byval y as long ) as long
' -1 when the point is not on a cell of the CURRENT tab's grid. On Custom this is the matrix
' cell index (row * cols + col); on Web/System/Palette it is the entry index.
declare function PsColorPicker_HitTestBody( byval hCtrl as HWND, byval x as long, byval y as long ) as long

' The two fixed tables, exposed so a host can offer the same colours elsewhere and so the
' self-test can walk them without reaching into the control.
declare function PsColorPicker_WebColorCount() as long
declare function PsColorPicker_WebColorAt( byval idx as long ) as COLORREF
declare function PsColorPicker_SystemColorCount() as long
declare function PsColorPicker_SystemColorAt( byval idx as long ) as COLORREF

declare sub      PsColorPicker_SetColorChangedCallback( byval hCtrl as HWND, byval usersub as CLR_ColorChangedCallbackSub )
declare sub      PsColorPicker_SetTabChangedCallback( byval hCtrl as HWND, byval usersub as CLR_TabChangedCallbackSub )
declare sub      PsColorPicker_SetPaletteCallbacks( byval hCtrl as HWND, _
                                                    byval fnCount as CLR_PaletteCountCallbackFunc, _
                                                    byval fnEntry as CLR_PaletteEntryCallbackFunc, _
                                                    byval subAssign as CLR_PaletteAssignCallbackSub )
declare sub      PsColorPicker_SetMessageCallback( byval hCtrl as HWND, byval userfunc as CLR_MessageCallbackFunc )
declare sub      PsColorPicker_SetPaintCallback( byval hCtrl as HWND, byval usersub as CLR_PaintCallbackSub )

' RUN THE BUILT-IN PAINTER FROM INSIDE A PAINT CALLBACK, so a host can DECORATE rather than
' REPLACE. Pass it the same pointer the callback was handed.
'
' NEW TO THIS FAMILY, and it exists because of how much this particular control draws. Every
' sibling's paint callback is all-or-nothing, which is a fair trade when the built-in painter is
' a circle and a caption -- a host replacing it re-implements two shapes. Here it would have to
' re-implement four tabs, a 192-cell grid, a palette list, two previews and five entry fields
' before it could change the one thing it cared about, and a callback nobody can afford to write
' is a callback nobody writes. So: call this first, then draw on top.
declare sub      PsColorPicker_RenderInfo( byval p as any ptr )

' Offscreen probes, public for the same reason PsButton's and PsCheckBox's are: a host that
' replaces the painter needs to be able to assert it has not flooded the control, and that a
' state change actually reached the pixels.
declare function PsColorPicker_CountRenderedTones( byval hCtrl as HWND, byval nPart as long ) as long
declare function PsColorPicker_HashRenderedPart( byval hCtrl as HWND, byval nPart as long ) as ulong

declare sub      PsColorPicker_RunSelfTest( byval hWndParent as HWND )

' NO PsColorPicker_FilterMessage, and there must never be one. See the header.
