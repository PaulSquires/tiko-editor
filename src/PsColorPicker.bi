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

'  PsColorPicker.bi  --  a colour picker that is a CHROMELESS MODAL POPUP: a WS_POPUP window
'  with no caption band, dropped under an anchor the way a combobox drops its list, run by a
'  nested message loop that BLOCKS until the user answers. PsColorPicker_DoModal returns TRUE
'  with the chosen colour written back, or FALSE if the user abandoned it.
'
'  IT WAS AN EMBEDDED PANEL UNTIL 2026-07-29 and the header used to argue at length for that
'  shape. The argument is gone with the shape; what survives from it is the one property worth
'  protecting, and it survives for a DIFFERENT reason -- see the next paragraph.
'
'  IT ADDS NO PUMP OBLIGATION, AND THAT IS NOW STRUCTURAL RATHER THAN CHOSEN
'    PsComboBox and PsDatePicker own a second top-level window and therefore impose a
'    *_FilterMessage call on the host's pump; getting that wrong is the single most common
'    integration bug in this family. This control owns a top-level window too, so by that rule
'    it should cost one -- except that DoModal runs its OWN GetMessage loop and that loop owns
'    IsDialogMessage, the outside-click test and the Esc key. There is nothing left for a host
'    pump to do. **THERE IS NO PsColorPicker_FilterMessage AND THERE MUST NEVER BE ONE.**
'    PsMessageBox is the only other control in the family that REMOVES a host contract instead
'    of adding one, and it is so for the same reason.
'
'  THE ENTRY FIELDS ARE HAND-ROLLED
'    R/G/B/A are drawn and key-handled by this control, NOT by embedded PsTextBox or
'    PsNumericUpDown children. That decision predates the popup conversion, where its original
'    justification was the pump obligation those children carry -- a justification the nested
'    loop has since made moot, since the loop could simply make the call itself. It is kept
'    anyway because the fields are worth less than the three vendored files they would cost:
'    they accept digits, Backspace, Delete, arrows, Home/End and Tab, and nothing else. No
'    selection, no clipboard, no undo. A host that needs those wants a PsTextBox on its own
'    dialog, not inside this popup.
'
'  TABS -- Web / System / Custom, and there is no fourth
'    A PALETTE tab listing the host's own named colours existed until 2026-07-29 and was
'    removed wholesale, along with its three callbacks and the Assign button that bound a
'    colour to an entry. Do not re-add it piecemeal: the Assign button is the half that makes a
'    palette more than decoration, and without it a palette can only ever decay.
'
'    TWO OF THE THREE ARE LISTS. Web and System are scrolling lists of NAMED rows -- a swatch,
'    then the name -- and they share one renderer and one hit test. Only Custom is a grid, and
'    its cells are unnamed because it is a LAW rather than a table.
'
'    An earlier version drew Web and System as 6x8 grids of anonymous swatches. That fitted the
'    panel neatly and told the user nothing: a named colour without its name is just a swatch,
'    and the names are most of what those two tabs are for. The lists need a scrollbar (137 web
'    entries), which is why this control has exactly one child window.
'
'    THE TAB STRIP IS SIZED TO ITS TEXT AND LEFT-ALIGNED -- notebook tabs, after the reference
'    design -- rather than dividing the strip into equal thirds as it did while embedded. Three
'    equal thirds of a popup this wide gave tabs far wider than their captions.
'
'    THE FIELDS AND THE INITIAL/CURRENT PAIR ARE DRAWN ON EVERY TAB, not only on Custom. Two
'    reasons, and the second is structural: a value readout that disappears when you go looking
'    for a colour is the wrong way round, and the popup would RESIZE as the user browsed --
'    which, for a window already placed against an anchor, means it grows off the bottom of the
'    monitor or has to be re-placed mid-interaction. The BODY is what the tab switches;
'    everything below it is fixed. (PsDatePicker's fixed 6x7 day grid is the same decision for
'    the same reason.)
'
'  INITIAL vs CURRENT
'    The picker shows both, as one box split top and bottom with its labels above and below,
'    exactly as the reference does. "Initial" is the colour DoModal was handed; "Current" is
'    the live value. That pair IS the visual form of a Reset button -- clicking the Initial
'    half reverts -- and it needs no extra host state.
'
'  HOW IT ENDS -- ONE BUTTON, TWO WAYS OUT
'    OK is the ONLY control that commits: it is the only path on which DoModal returns TRUE and
'    the only path that writes the out-parameters. Esc and a click anywhere OUTSIDE the popup
'    both CANCEL, returning FALSE and leaving the host's variables untouched. There is
'    deliberately no Cancel button -- an outside click is how a dropdown is abandoned, and a
'    second button for the same job would only make the footer wider.
'
'    THE OK BUTTON IS DRAWN BY THIS CONTROL, not a vendored PsButton. It is one rect, one paint
'    branch and one hit test, and vendoring PsButton would pull PsImage in behind it for a
'    button that will never show an image.
'
'    THE PARENT IS NOT DISABLED for the duration, which is the one place this departs from
'    PsMessageBox. It cannot be: a click on a disabled window generates nothing, so disabling
'    the parent would make the outside-click cancel above unreachable. "Modal" here means the
'    call blocks, not that the rest of the application is inert.
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
' THE PICKER'S ONE CHILD WINDOW, and the only one it will ever have. The Web and System tabs
' are scrolling LISTS, and hand-rolling a scrollbar to avoid a child would have produced a
' worse one that did not match the scrollbars beside it in a host.
'
' IT DOES NOT COST THE NO-PUMP-OBLIGATION PROPERTY, which is the thing actually worth protecting:
' PsVScrollBar has no *_FilterMessage of its own. Nor does it make the popup a tab CONTAINER --
' a scrollbar is never a tabstop and never takes focus. Asserted in the self-test rather than
' remembered.
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

' The Web / System LISTS. One row per entry: a swatch of nSwatchW, then the name.
'
' nListRows is how many rows are VISIBLE, and it is what sizes the body -- 137 named web colours
' will never fit, so this is the number that decides whether the list is comfortable to scan or a
' letterbox. It is also the page size handed to the scrollbar.
#define PSCOLORPICKER_DEFAULT_SWATCHW      34
#define PSCOLORPICKER_DEFAULT_LISTROWS     14
#define PSCOLORPICKER_DEFAULT_SCROLLW      14

' The Initial/Current preview is ONE box of PREVIEWW x (2 * PREVIEWH), split across the middle,
' with its two labels drawn above and below it. PREVIEWH is therefore the height of ONE half.
#define PSCOLORPICKER_DEFAULT_PREVIEWW     92
#define PSCOLORPICKER_DEFAULT_PREVIEWH     46
#define PSCOLORPICKER_DEFAULT_FIELDW       58
#define PSCOLORPICKER_DEFAULT_FIELDH       24
#define PSCOLORPICKER_DEFAULT_OKW          92
#define PSCOLORPICKER_DEFAULT_OKH          30
' Wide enough for "Green:" -- the field labels are whole words now, not single letters.
#define PSCOLORPICKER_DEFAULT_LABELW       52
#define PSCOLORPICKER_DEFAULT_ROWH         22

' The tab strip is sized to its text: each tab is its caption plus this much padding on either
' side. Left-aligned, so the strip's leftover width to the right of the last tab is empty.
#define PSCOLORPICKER_DEFAULT_TABPAD       14

' Curvature is an ellipse DIAMETER, as RoundRect took it -- the vocabulary every method in
' PsBufferPaint uses.
#define PSCOLORPICKER_DEFAULT_CURVATURE     4
#define PSCOLORPICKER_DEFAULT_BORDERTHICK   1   ' NOT scaled here; see above
#define PSCOLORPICKER_DEFAULT_FOCUSTHICK    1   ' NOT scaled here; see above

' How far the hot/selected ring is inset into a swatch, so the ring reads as being ON the
' swatch rather than as a gap around it.
#define PSCOLORPICKER_RINGINSET             1


' ----------------------------------------------------------------------------------------
' Which tab is showing. There are exactly three and they are always all present -- a fourth
' PALETTE tab was removed on 2026-07-29 along with its callbacks and its Assign button.
' ----------------------------------------------------------------------------------------
enum
    CLR_TAB_WEB = 0
    CLR_TAB_SYSTEM
    CLR_TAB_CUSTOM
    CLR_TAB_COUNT
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
    CLR_PART_OK
end enum

' ----------------------------------------------------------------------------------------
' The entry fields, in Tab order. THE ORDER IS THE CONTRACT: Tab walks this enum upward and
' Shift+Tab downward, and running off either end is NOT claimed, so the host's own tab order
' takes over. A picker that swallowed Tab forever would be a trap in exactly the dialog it is
' meant to live in.
'
' A is present in the enum whether or not the alpha field is SHOWN -- visibility is a runtime
' question (PsColorPicker_ShowAlpha, which now defaults ON) and the field walker skips it.
' Keeping the id stable means a stored "which field had focus" never means something different
' after ShowAlpha flips.
'
' THE ENUM ORDER IS ALSO THE VISUAL ORDER -- one field per row, top to bottom -- and the layout
' is written to keep it that way.
'
' A HEX FIELD used to sit after A and was removed on 2026-07-29 with the popup conversion: the
' reference design has no hex row, and the four rows it leaves fill the block beside the
' preview exactly.
' ----------------------------------------------------------------------------------------
enum
    CLR_FIELD_R = 0
    CLR_FIELD_G
    CLR_FIELD_B
    CLR_FIELD_A
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
    ' The OK button. Four fills and one border, which is the minimum that still reads as a
    ' button: idle, hot, pressed, and the border. There is no disabled pair because OK is
    ' never disabled -- every reachable state of this popup has a committable colour in it.
    OKBackColor         as COLORREF = BGR( 55, 61, 72)
    OKBackColorHot      as COLORREF = BGR( 68, 74, 86)
    OKBackColorPressed  as COLORREF = BGR( 40, 45, 53)
    OKForeColor         as COLORREF = BGR(226,230,238)
    OKBorderColor       as COLORREF = BGR( 68, 74, 86)
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
    rcSwatches    as RECT              ' the Web/System list, or EMPTY when on CUSTOM
    rcOK          as RECT              ' the OK button. NEVER empty -- it is the only commit path
    rcPreview     as RECT              ' the whole split box; rcInitial and rcCurrent tile it exactly
    rcInitial     as RECT              ' the baseline preview, top half of the split box
    rcCurrent     as RECT              ' the live preview, bottom half
    rcInitialLabel as RECT             ' "Initial", ABOVE the box -- not inside it
    rcCurrentLabel as RECT             ' "Current", BELOW the box
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
    isOKHot       as boolean           ' the cursor is over OK
    isOKPressed   as boolean           ' OK is held down (and the cursor has not slid off)
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
    ' DEFAULTS ON, unlike the embedded control it grew out of. A popup that exists to answer
    ' "what colour" and then closes has no reason to hide a channel; a host that genuinely has
    ' no alpha calls PsColorPicker_ShowAlpha( false ) and the row goes.
    bShowAlpha       as boolean = true

    ' --- The modal run. bModalDone ends the nested loop in PsColorPicker_DoModal; bAccepted
    '     says WHICH way it ended, and is the single source for DoModal's return value. They
    '     are two fields rather than one tri-state because "still running" and "finished,
    '     cancelled" must not be the same value -- the loop tests the first and only the first.
    bModalDone       as boolean = false
    bAccepted        as boolean = false

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
    ' OK's press/cancel gesture. The button takes capture on the DOWN and commits on the UP
    ' only if the cursor is still over it -- PsButton's rule, and the reason isOKPressed is
    ' distinct from isOKHot rather than derived from it.
    bOKHot           as boolean = false
    bOKPressed       as boolean = false

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
    wszOK            as DWSTRING
    wszInitialLabel  as DWSTRING
    wszCurrentLabel  as DWSTRING
    wszFieldLabel(0 to CLR_FIELD_COUNT - 1) as DWSTRING
    colors           as PSCOLORPICKER_COLORS
    ' NOT named hFont: FreeBASIC is case-insensitive, so a member called hFont shadows the
    ' TYPE name HFONT inside every member procedure of this type (C:\dev\Learnings.md).
    hTextFont        as HFONT

    ' --- Layout inputs. All pixels; DPI-scaled once at Create EXCEPT the two pens. ---
    nPad             as long = PSCOLORPICKER_DEFAULT_PAD
    nGap             as long = PSCOLORPICKER_DEFAULT_GAP
    nTabHeight       as long = PSCOLORPICKER_DEFAULT_TABHEIGHT
    nTabPad          as long = PSCOLORPICKER_DEFAULT_TABPAD
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
    nOKW             as long = PSCOLORPICKER_DEFAULT_OKW
    nOKH             as long = PSCOLORPICKER_DEFAULT_OKH
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
    rcSwatches       as RECT        ' the LIST area on Web / System, scrollbar excluded
    rcScroll         as RECT        ' where the PsVScrollBar child sits; empty on Custom
    rcOK             as RECT
    rcPreview        as RECT        ' the whole split box; rcInitial and rcCurrent are its halves
    rcInitial        as RECT
    rcCurrent        as RECT
    rcInitialLabel   as RECT
    rcCurrentLabel   as RECT
    rcFields         as RECT
    rcAlpha          as RECT
    rcTab(0 to CLR_TAB_COUNT - 1) as RECT
    rcFieldBox(0 to CLR_FIELD_COUNT - 1)   as RECT
    rcFieldLabel(0 to CLR_FIELD_COUNT - 1) as RECT
    bLayoutDirty     as boolean = true

    ColorChangedCallback as CLR_ColorChangedCallbackSub
    TabChangedCallback   as CLR_TabChangedCallbackSub
    MessageCallback      as CLR_MessageCallbackFunc
    PaintCallback        as CLR_PaintCallbackSub

    declare sub      LayoutColorPicker()
    declare sub      Refresh()
    declare sub      CancelDrag()
    declare function TabCount() as long
    declare function IsFieldVisible( byval nField as long ) as boolean
    declare function MeasureTabWidth( byval idx as long ) as long
    declare function MeasureTabStrip() as long
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
' THE HEX FIELD IS GONE BUT THESE TWO ARE NOT, deliberately. They are pure, they are the
' best-tested pair in the file, and #RRGGBB is how a host stores and shows a colour even when it
' does not let the user type one -- this control's own README uses them in its examples. They
' have no caller inside the control now, which is the honest reason to mention them here.
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
    dim as string s = ucase( trim( wszText.Utf8 ) )
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
' key handler, because "a channel field takes three digits and nothing else" is a rule worth
' asserting directly rather than by typing into a window.
'
' nPos is unused now that the hex field is gone -- it existed for "# is legal only at position
' 0". Kept in the signature because the self-test drives this function over a position sweep and
' the parameter is what makes "position never matters" assertable rather than merely true.
function PsColorPicker_FieldAccepts( _
            byval nField as long, _
            byval ch     as long, _
            byval nPos   as long _
            ) as boolean

    if (nField < 0) orelse (nField >= CLR_FIELD_COUNT) then return false
    return ((ch >= asc("0")) andalso (ch <= asc("9")))
end function

' Three digits for a 0..255 channel.
function PsColorPicker_FieldMaxLen( byval nField as long ) as long
    return 3
end function


' The caption on tab idx. ONE definition, consulted by both the layout (which MEASURES it, to
' size the tab) and the painter (which draws it). While the strip divided itself into equal
' thirds these could afford to be a literal in the painter; now that a tab is as wide as its own
' word, a second copy of the word would be a second, silently different width.
declare function PsColorPicker_TabName( byval idx as long ) as DWSTRING


' ========================================================================================
' Inline members.
' ========================================================================================

' How many tabs are on the strip. A constant since the PALETTE tab was removed -- kept as a
' function, rather than folded into its callers as the literal 3, because it is the thing every
' tab loop and the tab hit test count against and a single definition is what stops them
' disagreeing if a tab is ever added back.
function PSCOLORPICKER.TabCount() as long
    return CLR_TAB_COUNT
end function

' Is this field on screen? Only the alpha field is ever hidden. Kept as a member rather than an
' inline `nField <> CLR_FIELD_A orelse bShowAlpha` at each site because the layout, the painter,
' the Tab walker and the hit test must all agree, and four copies is four chances to diverge.
function PSCOLORPICKER.IsFieldVisible( byval nField as long ) as boolean
    if nField = CLR_FIELD_A then return this.bShowAlpha
    if (nField < 0) orelse (nField >= CLR_FIELD_COUNT) then return false
    return true
end function

' How wide tab idx wants to be: its caption plus nTabPad on either side.
'
' THE FALLBACK MATTERS. With no window yet -- which is the state PsColorPicker_GetIdealSize is
' called in, since a host asks how big the popup should be BEFORE there is one -- there is no DC
' to measure with, so the width is estimated from the caption's length. It is deliberately
' generous: a strip measured slightly too wide leaves a few empty pixels at its right end, while
' one measured too narrow ellipsizes a caption. Once the window exists, every layout pass
' measures for real and the estimate is never seen again.
function PSCOLORPICKER.MeasureTabWidth( byval idx as long ) as long
    dim as DWSTRING wszCaption = PsColorPicker_TabName( idx )
    dim as long nTextW = 0

    if this.hWin then
        dim as HDC hDC = GetDC( this.hWin )
        if hDC then
            dim as HFONT hFontUse = this.hTextFont
            if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
            dim as HFONT hOld = cast( HFONT, SelectObject( hDC, hFontUse ) )
            dim as SIZE sz
            GetTextExtentPoint32W( hDC, wszCaption.vptr, PsLen(wszCaption), @sz )
            nTextW = sz.cx
            SelectObject( hDC, hOld )
            ReleaseDC( this.hWin, hDC )
        end if
    end if

    if nTextW <= 0 then nTextW = PsLen(wszCaption) * this.nTabPad

    return nTextW + (2 * this.nTabPad)
end function

' What the whole strip needs. Sums MeasureTabWidth rather than measuring again, so the strip's
' width and the tabs placed into it cannot disagree.
function PSCOLORPICKER.MeasureTabStrip() as long
    dim as long nTotal = 0
    for i as long = 0 to this.TabCount() - 1
        nTotal += this.MeasureTabWidth(i)
    next
    return nTotal
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
'   rcTabs    = the top nTabHeight of it, full width; each tab as wide as ITS OWN CAPTION plus
'               2 * nTabPad, laid left to right, the remainder of the strip left empty
'   rcBody    = below it after nGap, full width, and ALWAYS nMatrixRows * nCellH TALL
'   the bottom block, below that after nGap -- three columns, left to right:
'       the preview     "Initial" label, the split box, "Current" label, stacked
'       the fields      one per row: [label][box], nGap between rows, A present only when
'                       bShowAlpha
'       the OK button   bottom-aligned with the last field row, against the right margin
'
' THE BODY HEIGHT IS FIXED ACROSS TABS, and that is the load-bearing decision. The Web grid
' needs 5 rows and the System grid 4; letting the body shrink to fit would RESIZE THE POPUP as
' the user browsed tabs -- and this window has already been placed against an anchor, so
' growing means growing off the bottom of the monitor or being re-placed mid-interaction. So the
' body is sized by the tallest content and the shorter grids simply leave the bottom of it
' empty. PsDatePicker fixes its day grid at 6 rows for the identical reason.
'
' WHICH OF rcMatrix / rcSwatches IS EMPTY IS THE TAB'S ONLY LAYOUT EFFECT. Everything else is
' identical on every tab, so a tab switch is an invalidate and not a re-measure. (The layout is
' still marked dirty on a tab change, because the two body rects have to swap.)
'
' THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area, and PsColorPicker_GetIdealSize is exactly what a host calls to decide how big to
' make the control in the FIRST place (PsToggle's rule).
'
' OVERFLOW is honest rather than squeezed: a window narrower than ideal keeps its declared
' cell sizes and lets the right-hand columns fall outside rcBody, where the paint loop clips
' them. Nothing is scaled down to fit, because a half-width swatch is not a smaller swatch, it
' is a wrong one. In practice DoModal sizes the window to nIdealW/nIdealH, so overflow is
' reachable only by a host that calls SetMatrixSize or SetCellSize after the popup is up.
'
' THE TAB MEASURE IS THE ONE PLACE THIS LAYOUT TAKES A DC. It has to: a tab is as wide as its
' caption, and a caption's width is a font question. The DC is released before anything else
' happens, and the measure is skipped entirely when there is no window yet.
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

    ' One row per VISIBLE field, stacked. Counted rather than assumed to be four, because
    ' ShowAlpha( false ) drops one and the block's height has to follow it -- an assumed four
    ' would leave a row of dead panel beside the preview.
    dim as long nFieldRows = 0
    for i as long = 0 to CLR_FIELD_COUNT - 1
        if this.IsFieldVisible(i) then nFieldRows += 1
    next

    dim as long fieldsW = this.nLabelW + this.nFieldW
    dim as long fieldsH = 0
    if nFieldRows > 0 then
        fieldsH = (nFieldRows * this.nFieldH) + ((nFieldRows - 1) * this.nGap)
    end if

    ' The preview column is taller than it looks: a label row, the split box (TWO halves of
    ' nPreviewH), then a second label row.
    dim as long previewH = (2 * this.nRowH) + (2 * this.nPreviewH)

    ' Three columns. The OK button gets its own rather than sitting under the fields, because
    ' the reference puts it on the last field's baseline and against the right margin.
    dim as long bottomW = this.nPreviewW + this.nGap + fieldsW + this.nGap + this.nOKW
    dim as long bottomH = fieldsH
    if previewH > bottomH then bottomH = previewH
    if this.nOKH > bottomH then bottomH = this.nOKH

    dim as long innerW = bodyW
    if bottomW > innerW then innerW = bottomW

    ' The strip must be at least as wide as the tabs actually measure, or the last caption is
    ' ellipsized in a popup with empty space to its right.
    dim as long stripNeed = this.MeasureTabStrip()
    if stripNeed > innerW then innerW = stripNeed

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

    ' EACH TAB IS AS WIDE AS ITS OWN CAPTION, laid left to right from the strip's left edge.
    ' They used to divide the strip into equal parts, which is right for a control whose tabs
    ' should fill their container and wrong here: three equal thirds of a popup this wide gave
    ' "Web" a cell four times the width of the word. Accumulating an x is safe now precisely
    ' BECAUSE the tabs are not required to meet the strip's right edge -- there is no rounding
    ' remainder to strand, and the leftover to the right of the last tab is simply strip.
    dim as long nTabs = this.TabCount()
    for i as long = 0 to CLR_TAB_COUNT - 1
        SetRectEmpty( @this.rcTab(i) )
    next
    dim as long tabX = this.rcTabs.left
    for i as long = 0 to nTabs - 1
        dim as long tabW = this.MeasureTabWidth(i)
        SetRect( @this.rcTab(i), tabX, this.rcTabs.top, tabX + tabW, this.rcTabs.bottom )
        tabX += tabW
    next

    ' ---- the body -------------------------------------------------------------------------
    dim as long bodyTop = this.rcTabs.bottom + this.nGap
    SetRect( @this.rcBody, this.rcContent.left, bodyTop, this.rcContent.right, bodyTop + bodyH )

    SetRectEmpty( @this.rcMatrix )
    SetRectEmpty( @this.rcSwatches )
    SetRectEmpty( @this.rcScroll )

    if this.nTab = CLR_TAB_CUSTOM then
        ' EXACTLY the grid, not the whole body: the point->cell division must not be able to
        ' report a cell that is not there.
        SetRect( @this.rcMatrix, this.rcBody.left, this.rcBody.top, _
                                 this.rcBody.left + (this.nMatrixCols * this.nCellW), _
                                 this.rcBody.top + (this.nMatrixRows * this.nCellH) )
    else
        this.rcSwatches = this.rcBody

        ' THE SCROLLBAR STRIP IS RESERVED WHETHER OR NOT A THUMB IS NEEDED, and that is
        ' PsScrollPanel's rule rather than PsListBox's: reclaiming the width when a short list
        ' fits would re-flow every row's name the moment the user switched between two lists of
        ' different lengths. A list whose text jumps sideways as you browse reads as a glitch.
        SetRect( @this.rcScroll, this.rcSwatches.right - this.nScrollBarW, this.rcSwatches.top, _
                                 this.rcSwatches.right, this.rcSwatches.bottom )
        this.rcSwatches.right = this.rcScroll.left
        if this.rcSwatches.right < this.rcSwatches.left then
            this.rcSwatches.right = this.rcSwatches.left
        end if
    end if

    ' ---- the preview column ----------------------------------------------------------------
    ' ONE box with a label above and a label below, and the box split across its middle: the
    ' top half is Initial, the bottom half Current. They are two rects of one box rather than
    ' two boxes because the reference draws no gap between them -- the point of the pair is to
    ' compare two colours edge to edge, and a gap of panel between them defeats it.
    dim as long botTop = this.rcBody.bottom + this.nGap

    SetRect( @this.rcInitialLabel, this.rcContent.left, botTop, _
                                   this.rcContent.left + this.nPreviewW, botTop + this.nRowH )
    SetRect( @this.rcPreview, this.rcContent.left, this.rcInitialLabel.bottom, _
                              this.rcContent.left + this.nPreviewW, _
                              this.rcInitialLabel.bottom + (2 * this.nPreviewH) )
    SetRect( @this.rcInitial, this.rcPreview.left, this.rcPreview.top, _
                              this.rcPreview.right, this.rcPreview.top + this.nPreviewH )
    SetRect( @this.rcCurrent, this.rcPreview.left, this.rcInitial.bottom, _
                              this.rcPreview.right, this.rcPreview.bottom )
    SetRect( @this.rcCurrentLabel, this.rcContent.left, this.rcPreview.bottom, _
                                   this.rcContent.left + this.nPreviewW, _
                                   this.rcPreview.bottom + this.nRowH )

    ' ---- the entry fields, one per row ------------------------------------------------------
    dim as long fx = this.rcPreview.right + this.nGap

    for i as long = 0 to CLR_FIELD_COUNT - 1
        SetRectEmpty( @this.rcFieldBox(i) )
        SetRectEmpty( @this.rcFieldLabel(i) )
    next

    ' A HIDDEN FIELD CONSUMES NO ROW -- y only advances for a field that was placed. Advancing
    ' unconditionally would leave a gap where alpha would have been, and would put the OK button
    ' (which bottom-aligns on the last row) a row lower than the last field it sits beside.
    dim as long fy = botTop
    dim as long fieldsBottom = botTop
    for i as long = 0 to CLR_FIELD_COUNT - 1
        if this.IsFieldVisible(i) = false then continue for
        SetRect( @this.rcFieldLabel(i), fx, fy, fx + this.nLabelW, fy + this.nFieldH )
        SetRect( @this.rcFieldBox(i), fx + this.nLabelW, fy, _
                                      fx + this.nLabelW + this.nFieldW, fy + this.nFieldH )
        fieldsBottom = fy + this.nFieldH
        fy += this.nFieldH + this.nGap
    next

    ' ---- the OK button ----------------------------------------------------------------------
    ' Against the right margin, and its BOTTOM on the last field's bottom -- the reference's
    ' alignment. Derived from fieldsBottom rather than from the arithmetic that produced it, so
    ' hiding a field moves the button and the fields together by construction.
    SetRect( @this.rcOK, this.rcContent.right - this.nOKW, fieldsBottom - this.nOKH, _
                         this.rcContent.right, fieldsBottom )

    ' rcAlpha ALIASES the alpha field's box and is empty when alpha is hidden, so a paint
    ' callback and the CLR_PART_ALPHA probe agree with the field walker by construction.
    this.rcAlpha = this.rcFieldBox(CLR_FIELD_A)

    ' The union of everything in the entry block, previews excluded. Built from the rects
    ' rather than re-derived from the arithmetic above, so it cannot drift from them.
    SetRect( @this.rcFields, fx, botTop, fx, fieldsBottom )
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
' A MODAL COLOUR PICKER POPUP: a tab strip, a body that is either a tint/shade matrix or a
' named list, an Initial/Current preview, hand-rolled R/G/B/A entry fields, and an OK button.
'
' NO PUMP OBLIGATION
'   There is no PsColorPicker_FilterMessage and THERE MUST NEVER BE ONE -- see the file header.
'   DoModal's own loop owns IsDialogMessage, Esc and the outside-click test, so there is nothing
'   for a host pump to do. Nor does the host need to SetFocus anything: DoModal focuses the
'   popup itself, which is the only window in the interaction.
'
' MOUSE CAPTURE IS TAKEN, FOR TWO GESTURES
'   The matrix drag, and the OK button's press/cancel. The family's test is "take capture only
'   if something consumes the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing"; a drag is the
'   clearest positive case there is (PsSplitter's) and a button that must NOT fire when you
'   press it and slide off is the other (PsButton's). Tabs, swatches and fields all act on the
'   DOWN or the UP without needing the pairing, and none of them takes capture.
'
' DELIBERATELY ABSENT
'   A Cancel button (Esc and an outside click do that job). A caption band, and therefore
'   dragging the popup by it. Tooltips (by request, as in PsComboBox and PsMessageBox).
'   Selection, clipboard and undo in the entry fields -- see the header. An eyedropper. A hex
'   field. Typing a colour by NAME: the Web and System tabs ARE named lists you pick from, but
'   no field accepts "cornflowerblue" as text. Multi-select.
'
' DOUBLE-CLICKING A BODY CELL ACCEPTS -- the mouse's equivalent of ENTER. It adopts the cell that
' was hit and then commits, so double-clicking a cell other than the selected one answers with the
' one under the cursor. A double-click anywhere else -- a tab, a field, the preview, the OK button
' -- behaves exactly as an ordinary click there and cannot close the popup.

' ---- Running it ------------------------------------------------------------------------

' SHOW THE PICKER AND BLOCK UNTIL THE USER ANSWERS.
'
' Returns TRUE if the user pressed OK, in which case clr and nAlpha have been written with the
' chosen value. Returns FALSE if the user cancelled -- Esc, or a click outside the popup -- and
' in that case NEITHER out-parameter is touched, so a host can pass its live variables straight
' in and let a cancel leave them alone. That is the whole reason for the byref-out shape rather
' than a COLORREF return: alpha needs a second value out, and a sentinel return could not carry
' "and don't disturb what you had".
'
' Both take the colour to START from through the same two parameters, so a host reads:
'
'     if PsColorPicker_DoModal( hWnd, myColor, myAlpha ) then ... it changed ...
'
' DoModal centres the popup on hParent. DoModalForRect drops it UNDER rcAnchor -- screen
' coordinates, typically the field or button that opened it -- flipping it ABOVE when it will
' not fit below, and clamping it onto the monitor's work area either way. The flip is
' PsMenuBar_ShowForRect's rule and it is not cosmetic: a popup CLAMPED up over its own anchor
' has the opening click's own release land inside it, and the popup self-dismisses.
'
' hParent may be NULL, in which case the popup is unowned and DoModal centres it on the monitor
' holding the cursor.
declare function PsColorPicker_DoModal( byval hParent as HWND, _
                                        byref clr as COLORREF, _
                                        byref nAlpha as ubyte ) as boolean
declare function PsColorPicker_DoModalForRect( byval hParent as HWND, _
                                               byref rcAnchor as RECT, _
                                               byref clr as COLORREF, _
                                               byref nAlpha as ubyte ) as boolean

' ---- Creation --------------------------------------------------------------------------

' Create the popup WITHOUT showing it. THIS IS THE LOW-LEVEL DOOR, and a host that only wants
' to ask for a colour should call DoModal instead -- it creates, configures, runs and destroys.
'
' Create exists so a host can reach the setters (colours, font, tab, matrix size, callbacks)
' between construction and display, which the DoModal signature has no room for. A window made
' this way is a WS_POPUP: it has no parent to be laid out in, and showing it is the caller's
' job, as is pumping it. PsColorPicker_RunModal is that pump.
declare function PsColorPicker_Create( byval hParent as HWND, byval id as long = 0 ) as HWND

' Show the popup and run the nested loop, returning TRUE on OK. The half of DoModal that does
' not create anything -- for the host that used Create to configure first. It does NOT destroy
' the window; a caller that used Create owns it and calls DestroyWindow.
declare function PsColorPicker_RunModal( byval hCtrl as HWND ) as boolean

' Place the popup against an anchor, in SCREEN coordinates, without showing it. Called by
' DoModalForRect; public so a Create-then-RunModal host can place it the same way rather than
' re-deriving the flip-above and work-area rules.
declare sub      PsColorPicker_PositionForRect( byval hCtrl as HWND, byref rcAnchor as RECT )
declare sub      PsColorPicker_PositionCentered( byval hCtrl as HWND, byval hParent as HWND )

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

' ALPHA IS SHOWN BY DEFAULT, reversing the embedded control's opt-in: a popup that exists to
' answer "what colour" has no reason to hide a channel. A host picking an opaque colour calls
' ShowAlpha( false ) and the row goes, taking its height out of the ideal size with it.
declare sub      PsColorPicker_ShowAlpha( byval hCtrl as HWND, byval bShow as boolean )
declare function PsColorPicker_IsAlphaShown( byval hCtrl as HWND ) as boolean

declare sub      PsColorPicker_SetTab( byval hCtrl as HWND, byval nTab as long )
declare function PsColorPicker_GetTab( byval hCtrl as HWND ) as long

declare sub      PsColorPicker_SetColors( byval hCtrl as HWND, byval pColors as PSCOLORPICKER_COLORS ptr )
declare sub      PsColorPicker_GetColors( byval hCtrl as HWND, byval pColors as PSCOLORPICKER_COLORS ptr )
declare sub      PsColorPicker_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function PsColorPicker_GetFont( byval hCtrl as HWND ) as HFONT
' The OK button's caption, the two preview labels, and the four field labels. Settable because
' this control does no localization of its own and a host that has any needs to supply the words
' -- the family rule that a control owns no strings the host might want to translate. Defaults
' are English.
'
' SetFieldText takes the labels in CLR_FIELD_* order and is the one place the trailing colon
' lives: the control appends nothing, so a host that wants "Red" without one simply omits it.
declare sub      PsColorPicker_SetOKText( byval hCtrl as HWND, byval Text as DWSTRING )
declare sub      PsColorPicker_SetPreviewText( byval hCtrl as HWND, byval TextInitial as DWSTRING, byval TextCurrent as DWSTRING )
declare sub      PsColorPicker_SetFieldText( byval hCtrl as HWND, byval nField as long, byval Text as DWSTRING )
declare sub      PsColorPicker_Refresh( byval hCtrl as HWND )

declare function PsColorPicker_GetEnabled( byval hCtrl as HWND ) as boolean
' Goes through EnableWindow, so the disable is enforced by the system rather than being a
' cosmetic flag. WM_ENABLE syncs the control's own flag back.
declare sub      PsColorPicker_SetEnabled( byval hCtrl as HWND, byval bEnabled as boolean )
declare function PsColorPicker_GetFocused( byval hCtrl as HWND ) as boolean

' Ideal size is VALID BEFORE THE POPUP IS EVER SIZED (PsToggle's rule) -- which here is not a
' convenience but the mechanism: DoModal has nothing else to size the window from.
'
' It varies with the MATRIX and CELL size, with ShowAlpha (hiding alpha takes a whole field ROW
' out of the height, since the fields stack), and with the FONT, because the tab strip is as
' wide as its captions measure.
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
' cell index (row * cols + col); on Web/System it is the entry index.
declare function PsColorPicker_HitTestBody( byval hCtrl as HWND, byval x as long, byval y as long ) as long

' The two fixed tables, exposed so a host can offer the same colours elsewhere and so the
' self-test can walk them without reaching into the control.
' The NAME accessors matter as much as the values: these two tabs are named lists, not
' anonymous swatch grids, so a host offering the same colours elsewhere wants the label too.
' An out-of-range index gives "" rather than failing.
declare function PsColorPicker_WebColorCount() as long
declare function PsColorPicker_WebColorAt( byval idx as long ) as COLORREF
declare function PsColorPicker_WebColorName( byval idx as long ) as DWSTRING
declare function PsColorPicker_SystemColorCount() as long
declare function PsColorPicker_SystemColorAt( byval idx as long ) as COLORREF
declare function PsColorPicker_SystemColorName( byval idx as long ) as DWSTRING

' Half-open containment (left/top inclusive, right/bottom exclusive) -- the same convention the
' cell arithmetic uses, which is what keeps the two agreeing at a grid's last column. A pure
' function of four longs, so it can be asserted without a window.
declare function PsColorPicker_PtIn( byval rc as RECT, byval x as long, byval y as long ) as boolean

declare sub      PsColorPicker_SetColorChangedCallback( byval hCtrl as HWND, byval usersub as CLR_ColorChangedCallbackSub )
declare sub      PsColorPicker_SetTabChangedCallback( byval hCtrl as HWND, byval usersub as CLR_TabChangedCallbackSub )
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
