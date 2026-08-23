
#pragma once

#include once "vbcompat.bi"                 ' format() -- numeric mode formatting
#include once "PsBufferPaint.bi"
' The built-in context menu is a PsPopupMenu (canonical home C:\dev\PsMenuBar; vendored
' here). This is the control family's owner-drawn floating menu -- using it instead of
' TrackPopupMenu is what makes the textbox's right-click menu themeable and consistent
' with the rest of an app's menus. It brings a MESSAGE-PUMP OBLIGATION with it: see
' PsTextBox_FilterMessage below.
#include once "PsPopupMenu.bi"
' AfxNova's RichEdit wrapper: pulls in win/richedit.bi (CHARFORMATW, EM_CANPASTE, ...)
' and supplies the RichEdit_* helpers used by the implementation (GetText, GetSelText,
' CanPaste, ...). Prefer these over re-rolled SendMessage wrappers.
#include once "AfxNova\AfxRichEdit.inc"
#include once "AfxNova\AfxWin.inc"          ' AfxGetClipboardText -- numeric paste validation

type PSTEXTBOX_MESSAGEINFO
    hTextBox        as HWND       ' the CONTROL (container) handle, not the RichEdit child
    uMsg            as UINT
    wParam          as WPARAM
    lParam          as LPARAM
end type

' Fired after the text changed from USER interaction (typing, cut/paste, undo). Never
' fired for programmatic changes (SetText/ReplaceSel/Clear or a forwarded WM_SETTEXT):
' programmatic setters are silent, only user interaction notifies -- the same split the
' sibling controls apply, and it lets a host call a setter from inside its own handler
' without re-entrancy.
type TXT_ChangeCallbackSub as sub( byval hTextBox as HWND )

' Fired when the RichEdit child gains (bGotFocus = true) or loses (false) the keyboard
' focus. The control has already invalidated itself for the border color switch.
type TXT_FocusCallbackSub as sub( byval hTextBox as HWND, byval bGotFocus as boolean )

' Fired when the user presses ENTER in the textbox. The keypress itself is always
' swallowed (no newline, no beep). SINGLE-LINE ONLY: in a multiline control ENTER
' inserts a newline and this callback never fires.
type TXT_EnterPressedCallbackSub as sub( byval hTextBox as HWND )

' Fired whenever the vertical scroll state MAY have changed (relayed from the RichEdit's
' EN_UPDATE, which fires on any display update: typing, programmatic SetText, scrolling
' by wheel/keys/caret). Multiline only; deliberately NOT gated like the ChangeCallback --
' a programmatic SetText changes the line count and a scrollbar must hear about it.
' Shaped for PsVScrollBar: on each fire call PsTextBox_GetVScrollInfo and push the three
' numbers into PsVScrollBar_SetRange( total, page, pos ); wire PsVScrollBar's
' ScrollCallback( newPos ) to PsTextBox_ScrollToLine.
type TXT_ScrollChangedCallbackSub as sub( byval hTextBox as HWND )

' Observe key, mouse, focus and context-menu messages before the control acts on them.
' Return TRUE if you handled the message and want the control's default handling (and
' the RichEdit's own) suppressed, FALSE to let it proceed. The result is honored
' uniformly: the control takes no mouse capture, so there is no invariant a callback
' can strand by suppressing a message. CAUTION: suppressing WM_SETFOCUS/WM_KILLFOCUS
' also suppresses the RichEdit's own caret handling -- only do that on purpose.
' Vetoing WM_KEYDOWN for VK_TAB repurposes Tab (the control otherwise consumes it to
' move focus to the next/previous tab stop -- e.g. veto it to drive a picker list).
type TXT_MessageCallbackFunc as function( byval m as PSTEXTBOX_MESSAGEINFO ptr ) as boolean


' Horizontal text alignment. LEFT is the default and is what every host had before this
' existed, so adding it changed nothing for any of them.
'
' These are the control's own constants rather than richedit.bi's PFA_* values, because the
' mapping is an implementation detail: a host should not have to know that alignment is
' delivered by EM_SETPARAFORMAT, any more than it has to know that the forecolor is
' delivered by EM_SETCHARFORMAT.
enum
    TXT_ALIGN_LEFT = 0
    TXT_ALIGN_CENTER
    TXT_ALIGN_RIGHT
end enum


type PSTEXTBOX
    hWin            as HWND
    hRichEdit       as HWND               ' the RichEdit50W child that does the editing
    idc_TextBox     as integer = 1000
    ' Multiline mode. Fixed at creation (ES_MULTILINE cannot be toggled on a live
    ' window): word-wrapped, ENTER inserts a newline, TAB inserts a tab character,
    ' no vertical text centering, numeric mode unavailable. The control adds no
    ' scrollbar styles -- pair it with an external scrollbar via the scroll API below.
    bMultiline      as boolean = false
    ' --- Colors. One forecolor/backcolor for ALL text (the control never colors
    '     individual characters; the child runs in TM_PLAINTEXT mode to guarantee it). ---
    ForeColor       as COLORREF = BGR(0,0,0)
    BackColor       as COLORREF = BGR(255,255,255)
    CueColor        as COLORREF = BGR(128,128,128)
    BorderColor     as COLORREF = BGR(122,122,122)
    FocusBorderColor as COLORREF = BGR(0,120,215)
    OuterBackColor  as COLORREF = BGR(255,255,255)  ' fills the pixels outside rounded corners
    nBorderWidth    as integer = 1        ' 0 = no border drawn
    nCornerRadius   as integer = 0        ' 0 = square corners
    nMarginLeft     as integer = 0        ' EM_SETMARGINS values, stored so they can be
    nMarginRight    as integer = 0        '   re-applied after font/geometry changes
    ' Horizontal alignment (TXT_ALIGN_*). LEFT is the RichEdit's own starting state, so a
    ' control whose host never calls PsTextBox_SetTextAlign never touches any of the
    ' machinery behind this field. Applying a non-default value is surprisingly expensive --
    ' see PsTextBox_ApplyParaFormat, which had to be built around TM_PLAINTEXT refusing
    ' EM_SETPARAFORMAT outright.
    nTextAlign      as long = TXT_ALIGN_LEFT
    ' Caller-supplied fonts (caller owns them; the control never deletes an HFONT).
    ' NOT named hFont: a member called hFont would shadow the HFONT type inside member
    ' procedures (FreeBASIC is case-insensitive -- see C:\dev\Learnings.md).
    hTextFont       as HFONT              ' 0 = stock DEFAULT_GUI_FONT
    hCueFont        as HFONT              ' 0 = use hTextFont
    CueText         as DWSTRING           ' drawn whenever the buffer is empty ("" = none)
    ' Built-in context menu labels; defaulted to Cut/Copy/Paste/Select All at Create,
    ' replace via PsTextBox_SetMenuText to localize.
    wszMenuCut       as DWSTRING
    wszMenuCopy      as DWSTRING
    wszMenuPaste     as DWSTRING
    wszMenuSelectAll as DWSTRING
    ' The context menu itself: one PsPopupMenu window per textbox, created at Create so a
    ' host can theme it immediately (PsTextBox_GetContextMenu), rebuilt from the current
    ' selection/clipboard/read-only state each time it opens, and destroyed with the
    ' control. Unlike the HMENU it replaces it is NOT modal -- the commands run from its
    ' select callback after it closes, not inline in WM_CONTEXTMENU.
    hContextMenu     as HWND
    ' Select the whole text when the control gains keyboard focus (Tab or programmatic
    ' SetFocus). A mouse click still places the caret at the click point: the click's
    ' own caret placement runs after the focus change and wins, by design.
    bSelectOnFocus  as boolean = false
    ' --- Numeric input mode: only digits, one leading minus and one decimal separator
    '     are accepted (typed OR pasted), with at most nDecimalPlaces fractional digits
    '     (0 = integers only). On focus loss a non-empty value is reformatted to exactly
    '     nDecimalPlaces (silently -- no ChangeCallback); empty stays empty so the cue
    '     banner can show -- unless bZeroWhenEmpty, which stamps the formatted zero
    '     ("0.00") instead, so the box always displays a value (and the cue banner
    '     effectively never shows once the box has been visited). ---
    bNumericOnly    as boolean = false
    nDecimalPlaces  as integer = 2
    bZeroWhenEmpty  as boolean = false
    ' TRUE while the control itself writes to the RichEdit; EN_CHANGE is dropped so
    ' programmatic changes never reach the ChangeCallback.
    bInternalChange as boolean = false
    ' Multiline wheel scrolling is implemented by the control (the RichEdit's own
    ' handling proved unreliable). High-precision wheels send deltas below one notch
    ' (120); the remainder accumulates here between events.
    nWheelAccum           as long = 0
    ChangeCallback        as TXT_ChangeCallbackSub
    FocusCallback         as TXT_FocusCallbackSub
    EnterPressedCallback  as TXT_EnterPressedCallbackSub
    MessageCallback       as TXT_MessageCallbackFunc
    ScrollChangedCallback as TXT_ScrollChangedCallbackSub
end type


' ----------------------------------------------------------------------------------------
' Creation.
' The returned handle is the control's real HWND: position it with SetWindowPos (include
' SWP_SHOWWINDOW -- the control is created hidden), find it with GetDlgItem(CtrlID). The
' RichEdit child is an implementation detail; reach it with PsTextBox_GetRichEditHandle
' only for RichEdit-specific messages the flat API (or the forwarded EM_ range, see
' below) does not cover.
'
' Every pixel value passed to this API (margins, border width, corner radius) is a RAW
' device pixel: DPI-scale at the call site (pWindow->ScaleX). The control never scales
' a caller's value -- margins go verbatim into EM_SETMARGINS, so a control-side scale
' would both lie to EM_GETMARGINS readers and double-scale hosts that already follow
' the sibling convention of scaling before the call.
'
' Message door: the container forwards the classic edit-control message range
' (EM_GETSEL .. EM_GETIMESTATUS) plus WM_SETTEXT / WM_GETTEXT / WM_GETTEXTLENGTH /
' WM_CUT / WM_COPY / WM_PASTE / WM_CLEAR / WM_UNDO to the RichEdit child, so
' SendMessage(hTextBox, EM_SETSEL, ...) works from a separate control or window. Same
' operation, two doors: message for a separate control, function for an in-process host.
' ----------------------------------------------------------------------------------------
' bMultiline is fixed for the control's lifetime (see the TYPE field comment). Multiline
' controls word-wrap (no ES_AUTOHSCROLL) and scroll vertically without showing a native
' scrollbar; drive an external one through the scroll API below.
declare function PsTextBox_Create( byval hWndParent as HWND, byval CtrlID as integer, byval bMultiline as boolean = false ) as HWND
declare function PsTextBox_GetRichEditHandle( byval hTextBoxControl as HWND ) as HWND
declare function PsTextBox_GetMultiline( byval hTextBoxControl as HWND ) as boolean
' Does this textbox own the keyboard focus? Focus sits on the RichEdit child, so a
' plain GetFocus() = hTextBox comparison is ALWAYS false -- use this instead.
declare function PsTextBox_HasFocus( byval hTextBoxControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Text.
' ----------------------------------------------------------------------------------------
declare function PsTextBox_GetText( byval hTextBoxControl as HWND ) as DWSTRING
declare function PsTextBox_SetText( byval hTextBoxControl as HWND, byval Text as DWSTRING ) as boolean
declare function PsTextBox_GetTextLength( byval hTextBoxControl as HWND ) as integer
declare function PsTextBox_GetSelText( byval hTextBoxControl as HWND ) as DWSTRING
declare function PsTextBox_ReplaceSel( byval hTextBoxControl as HWND, byval Text as DWSTRING ) as boolean
declare function PsTextBox_Clear( byval hTextBoxControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Selection. Positions are 0-based character indices; nEnd = -1 selects to the end.
' ----------------------------------------------------------------------------------------
declare sub      PsTextBox_GetSel( byval hTextBoxControl as HWND, byref nStart as integer, byref nEnd as integer )
declare sub      PsTextBox_SetSel( byval hTextBoxControl as HWND, byval nStart as integer, byval nEnd as integer )
declare sub      PsTextBox_SelectAll( byval hTextBoxControl as HWND )

' ----------------------------------------------------------------------------------------
' Edit behavior.
' ----------------------------------------------------------------------------------------
declare function PsTextBox_GetLimitText( byval hTextBoxControl as HWND ) as integer
declare sub      PsTextBox_SetLimitText( byval hTextBoxControl as HWND, byval nLimit as integer )
declare function PsTextBox_GetReadOnly( byval hTextBoxControl as HWND ) as boolean
declare function PsTextBox_SetReadOnly( byval hTextBoxControl as HWND, byval bReadOnly as boolean ) as boolean
declare function PsTextBox_GetModify( byval hTextBoxControl as HWND ) as boolean
declare sub      PsTextBox_SetModify( byval hTextBoxControl as HWND, byval bModified as boolean )
declare sub      PsTextBox_SetPasswordChar( byval hTextBoxControl as HWND, byval wchChar as integer )   ' 0 = off
' Select all text when the control gains keyboard focus (see the TYPE field comment:
' a mouse click still places the caret). Default false.
declare function PsTextBox_GetSelectOnFocus( byval hTextBoxControl as HWND ) as boolean
declare sub      PsTextBox_SetSelectOnFocus( byval hTextBoxControl as HWND, byval bSelect as boolean )
' Numeric input mode (see the TYPE field comment for the accepted grammar). DecimalPlaces
' 0 means integers only. GetValue/SetValue trade the text as a double; SetValue formats
' to DecimalPlaces and, like every programmatic setter, is silent. SINGLE-LINE ONLY:
' SetNumericMode is a no-op on a multiline control.
declare function PsTextBox_GetNumericMode( byval hTextBoxControl as HWND ) as boolean
declare sub      PsTextBox_SetNumericMode( byval hTextBoxControl as HWND, byval bEnable as boolean )
declare function PsTextBox_GetDecimalPlaces( byval hTextBoxControl as HWND ) as integer
declare sub      PsTextBox_SetDecimalPlaces( byval hTextBoxControl as HWND, byval nPlaces as integer )
declare function PsTextBox_GetValue( byval hTextBoxControl as HWND ) as double
declare function PsTextBox_SetValue( byval hTextBoxControl as HWND, byval nValue as double ) as boolean
' Numeric mode only: display the formatted zero ("0.00") instead of an empty box. The
' setter stamps it immediately when the box is currently empty and unfocused; after
' that, every focus loss with an empty buffer restores it. Trumps the cue banner.
declare function PsTextBox_GetZeroWhenEmpty( byval hTextBoxControl as HWND ) as boolean
declare sub      PsTextBox_SetZeroWhenEmpty( byval hTextBoxControl as HWND, byval bEnable as boolean )
declare sub      PsTextBox_Cut( byval hTextBoxControl as HWND )
declare sub      PsTextBox_Copy( byval hTextBoxControl as HWND )
declare sub      PsTextBox_Paste( byval hTextBoxControl as HWND )
declare function PsTextBox_CanPaste( byval hTextBoxControl as HWND ) as boolean
declare function PsTextBox_Undo( byval hTextBoxControl as HWND ) as boolean
declare function PsTextBox_CanUndo( byval hTextBoxControl as HWND ) as boolean
' Left/right margins between the border and the text, in pixels (EM_SETMARGINS).
declare sub      PsTextBox_GetMargins( byval hTextBoxControl as HWND, byref nLeft as integer, byref nRight as integer )
declare sub      PsTextBox_SetMargins( byval hTextBoxControl as HWND, byval nLeft as integer, byval nRight as integer )

' ----------------------------------------------------------------------------------------
' Vertical scroll state (multiline). Units are LINES, shaped 1:1 for PsVScrollBar:
' ScrollChangedCallback fires -> GetVScrollInfo -> PsVScrollBar_SetRange( total, page, pos );
' PsVScrollBar's ScrollCallback( newPos ) -> ScrollToLine( newPos ). LinesPerPage derives
' from the formatting-rect height and the text font's line height -- a partial line at the
' bottom is not counted. Single-line controls report total=1, page=0/1, first=0.
' ----------------------------------------------------------------------------------------
declare sub      PsTextBox_GetVScrollInfo( byval hTextBoxControl as HWND, byref nTotalLines as integer, byref nLinesPerPage as integer, byref nFirstVisibleLine as integer )
' Scroll so nLine (0-based) becomes the first visible line. The RichEdit clamps overscroll.
declare sub      PsTextBox_ScrollToLine( byval hTextBoxControl as HWND, byval nLine as integer )

' ----------------------------------------------------------------------------------------
' Appearance. All fonts are caller-owned HFONTs; the control converts the text font to a
' CHARFORMATW internally (face/size/charset/bold/italic + ForeColor) and re-applies it
' whenever the font or forecolor changes.
' ----------------------------------------------------------------------------------------
declare function PsTextBox_GetFont( byval hTextBoxControl as HWND ) as HFONT
declare function PsTextBox_SetFont( byval hTextBoxControl as HWND, byval hFont as HFONT ) as boolean
declare function PsTextBox_GetForeColor( byval hTextBoxControl as HWND ) as COLORREF
declare function PsTextBox_SetForeColor( byval hTextBoxControl as HWND, byval clr as COLORREF ) as COLORREF
declare function PsTextBox_GetBackColor( byval hTextBoxControl as HWND ) as COLORREF
declare function PsTextBox_SetBackColor( byval hTextBoxControl as HWND, byval clr as COLORREF ) as COLORREF
' Cue banner: drawn whenever the buffer is empty, focused or not (the caret blinks over
' it). Its color is independent of the text ForeColor; its font defaults to the text font.
declare function PsTextBox_GetCueBannerText( byval hTextBoxControl as HWND ) as DWSTRING
declare sub      PsTextBox_SetCueBannerText( byval hTextBoxControl as HWND, byval Text as DWSTRING )
declare function PsTextBox_GetCueBannerColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      PsTextBox_SetCueBannerColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
declare function PsTextBox_GetCueBannerFont( byval hTextBoxControl as HWND ) as HFONT
declare sub      PsTextBox_SetCueBannerFont( byval hTextBoxControl as HWND, byval hFont as HFONT )
' Horizontal alignment of the text (TXT_ALIGN_LEFT / CENTER / RIGHT; LEFT is the default,
' so this changed nothing for hosts written before it existed). It applies to the whole
' buffer, in both line modes, and it survives every subsequent SetText.
'
' SET IT AT SETUP TIME, BEFORE THE CONTROL HAS CONTENT. This is not a style preference:
' TM_PLAINTEXT -- which this control relies on for uniform formatting and for making pasted
' text shed its rich formatting -- REFUSES EM_SETPARAFORMAT outright, silently, returning
' failure and leaving the alignment at LEFT. The only sequence the RichEdit accepts is to
' empty the buffer, flip to rich-text mode, apply, and flip back, so the setter does exactly
' that. On an empty control that costs nothing. On a control the user has been typing into
' IT DISCARDS THE UNDO HISTORY, which is the whole reason to set it early.
'
' The margins set by PsTextBox_SetMargins still apply -- centring happens between them, not
' between the border edges, so a centred value in a control with lopsided margins sits
' off-centre by design. Give it equal margins if you want it optically centred.
declare function PsTextBox_GetTextAlign( byval hTextBoxControl as HWND ) as long
declare sub      PsTextBox_SetTextAlign( byval hTextBoxControl as HWND, byval nAlign as long )
' Border chrome. Width 0 = borderless; the border color switches to FocusBorderColor
' while the RichEdit has focus. CornerRadius rounds the frame -- the arcs are
' antialiased now, so a larger radius no longer looks stepped the way it did under
' GDI's RoundRect. OuterBackColor fills the pixels outside the corner arcs -- pass the
' host's background color so rounded corners blend into it.
declare function PsTextBox_GetBorderColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      PsTextBox_SetBorderColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
declare function PsTextBox_GetFocusBorderColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      PsTextBox_SetFocusBorderColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
declare function PsTextBox_GetBorderWidth( byval hTextBoxControl as HWND ) as integer
declare sub      PsTextBox_SetBorderWidth( byval hTextBoxControl as HWND, byval nWidth as integer )
declare function PsTextBox_GetCornerRadius( byval hTextBoxControl as HWND ) as integer
declare sub      PsTextBox_SetCornerRadius( byval hTextBoxControl as HWND, byval nRadius as integer )
declare function PsTextBox_GetOuterBackColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      PsTextBox_SetOuterBackColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
' Localize the built-in right-click menu (Cut/Copy/Paste/Select All). An empty
' SelectAllText keeps the current Select All label, so 3-argument callers are unaffected.
declare sub      PsTextBox_SetMenuText( byval hTextBoxControl as HWND, byval CutText as DWSTRING, byval CopyText as DWSTRING, byval PasteText as DWSTRING, byval SelectAllText as DWSTRING = "" )

' ----------------------------------------------------------------------------------------
' THE BUILT-IN CONTEXT MENU (Cut / Copy / Paste / Select All)
'
' It is a PsPopupMenu window owned by the control, created at Create and destroyed with it.
' Two consequences a host has to know about:
'
' 1. THE MESSAGE-PUMP OBLIGATION (not optional). A PsPopupMenu is not modal: keyboard
'    navigation and click-outside dismissal live in its message filter. A host that never
'    calls PsTextBox_FilterMessage gets a right-click menu that opens and paints but cannot
'    be driven from the keyboard and never closes on an outside click:
'        do while GetMessage(@uMsg, null, 0, 0)
'            if PsTextBox_FilterMessage( @uMsg ) then continue do
'            ...TranslateMessage/DispatchMessage...
'    One call serves every PsTextBox in the application -- only one menu chain can be open
'    at a time, and the filter finds it. (An app that also hosts PsMenuBar calls that
'    filter too; the two are independent and each stands down when the other's menu is up.)
'
' 2. STYLING IS YOURS. GetContextMenu hands back the popup so the usual PsPopupMenu setters
'    (SetColors / SetFonts / SetGlyphs / SetItemHeight) apply -- point them at the same
'    values the rest of your menus use and the textbox menu matches them. Left alone it
'    renders with PsPopupMenu's own defaults. The handle is stable for the control's
'    lifetime, so theming it once at startup is enough; the labels are rebuilt per open
'    but colors and fonts are not.
' ----------------------------------------------------------------------------------------
' CloseContextMenu dismisses an open menu from any PsTextBox. SILENT (no select callback),
' matching the family's programmatic-setter rule. For hosts with a global "close every
' menu" moment -- app deactivation, or a modal dialog about to open.
declare function PsTextBox_GetContextMenu( byval hTextBoxControl as HWND ) as HWND
declare function PsTextBox_FilterMessage( byval pMsg as MSG ptr ) as boolean
declare sub      PsTextBox_CloseContextMenu()

' ----------------------------------------------------------------------------------------
' Callbacks. See the type declarations above for each signature and contract.
'   ChangeCallback        - text changed by USER interaction (programmatic changes silent).
'   FocusCallback         - RichEdit gained/lost keyboard focus.
'   EnterPressedCallback  - ENTER pressed (single-line only; keypress always swallowed).
'   MessageCallback       - observe key/mouse/focus/context-menu messages; TRUE suppresses.
'   ScrollChangedCallback - vertical scroll state may have changed (multiline only).
' ----------------------------------------------------------------------------------------
declare sub      PsTextBox_SetChangeCallback( byval hTextBoxControl as HWND, byval usersub as TXT_ChangeCallbackSub )
declare sub      PsTextBox_SetFocusCallback( byval hTextBoxControl as HWND, byval usersub as TXT_FocusCallbackSub )
declare sub      PsTextBox_SetEnterPressedCallback( byval hTextBoxControl as HWND, byval usersub as TXT_EnterPressedCallbackSub )
declare sub      PsTextBox_SetMessageCallback( byval hTextBoxControl as HWND, byval userfunc as TXT_MessageCallbackFunc )
declare sub      PsTextBox_SetScrollChangedCallback( byval hTextBoxControl as HWND, byval usersub as TXT_ScrollChangedCallbackSub )
