
#pragma once

#include once "vbcompat.bi"                 ' format() -- numeric mode formatting
#include once "clsDoubleBuffer.bi"
' AfxNova's RichEdit wrapper: pulls in win/richedit.bi (CHARFORMATW, EM_CANPASTE, ...)
' and supplies the RichEdit_* helpers used by the implementation (GetText, GetSelText,
' CanPaste, ...). Prefer these over re-rolled SendMessage wrappers.
#include once "AfxNova\AfxRichEdit.inc"
#include once "AfxNova\AfxWin.inc"          ' AfxGetClipboardText -- numeric paste validation

type CTEXTBOX_MESSAGEINFO
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
' swallowed (the control is single-line; no newline, no beep).
type TXT_EnterPressedCallbackSub as sub( byval hTextBox as HWND )

' Observe key, mouse, focus and context-menu messages before the control acts on them.
' Return TRUE if you handled the message and want the control's default handling (and
' the RichEdit's own) suppressed, FALSE to let it proceed. The result is honored
' uniformly: the control takes no mouse capture, so there is no invariant a callback
' can strand by suppressing a message. CAUTION: suppressing WM_SETFOCUS/WM_KILLFOCUS
' also suppresses the RichEdit's own caret handling -- only do that on purpose.
' Vetoing WM_KEYDOWN for VK_TAB repurposes Tab (the control otherwise consumes it to
' move focus to the next/previous tab stop -- e.g. veto it to drive a picker list).
type TXT_MessageCallbackFunc as function( byval m as CTEXTBOX_MESSAGEINFO ptr ) as boolean

type CTEXTBOX
    hWin            as HWND
    hRichEdit       as HWND               ' the RichEdit50W child that does the editing
    idc_TextBox     as integer = 1000
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
    ' Caller-supplied fonts (caller owns them; the control never deletes an HFONT).
    ' NOT named hFont: a member called hFont would shadow the HFONT type inside member
    ' procedures (FreeBASIC is case-insensitive -- see C:\dev\Learnings.md).
    hTextFont       as HFONT              ' 0 = stock DEFAULT_GUI_FONT
    hCueFont        as HFONT              ' 0 = use hTextFont
    CueText         as DWSTRING           ' drawn whenever the buffer is empty ("" = none)
    ' Built-in context menu labels; defaulted to Cut/Copy/Paste at Create, replace via
    ' CTextBox_SetMenuText to localize.
    wszMenuCut      as DWSTRING
    wszMenuCopy     as DWSTRING
    wszMenuPaste    as DWSTRING
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
    ChangeCallback       as TXT_ChangeCallbackSub
    FocusCallback        as TXT_FocusCallbackSub
    EnterPressedCallback as TXT_EnterPressedCallbackSub
    MessageCallback      as TXT_MessageCallbackFunc
end type


' ----------------------------------------------------------------------------------------
' Creation.
' The returned handle is the control's real HWND: position it with SetWindowPos (include
' SWP_SHOWWINDOW -- the control is created hidden), find it with GetDlgItem(CtrlID). The
' RichEdit child is an implementation detail; reach it with CTextBox_GetRichEditHandle
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
declare function CTextBox_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND
declare function CTextBox_GetRichEditHandle( byval hTextBoxControl as HWND ) as HWND
' Does this textbox own the keyboard focus? Focus sits on the RichEdit child, so a
' plain GetFocus() = hTextBox comparison is ALWAYS false -- use this instead.
declare function CTextBox_HasFocus( byval hTextBoxControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Text.
' ----------------------------------------------------------------------------------------
declare function CTextBox_GetText( byval hTextBoxControl as HWND ) as DWSTRING
declare function CTextBox_SetText( byval hTextBoxControl as HWND, byval Text as DWSTRING ) as boolean
declare function CTextBox_GetTextLength( byval hTextBoxControl as HWND ) as integer
declare function CTextBox_GetSelText( byval hTextBoxControl as HWND ) as DWSTRING
declare function CTextBox_ReplaceSel( byval hTextBoxControl as HWND, byval Text as DWSTRING ) as boolean
declare function CTextBox_Clear( byval hTextBoxControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Selection. Positions are 0-based character indices; nEnd = -1 selects to the end.
' ----------------------------------------------------------------------------------------
declare sub      CTextBox_GetSel( byval hTextBoxControl as HWND, byref nStart as integer, byref nEnd as integer )
declare sub      CTextBox_SetSel( byval hTextBoxControl as HWND, byval nStart as integer, byval nEnd as integer )
declare sub      CTextBox_SelectAll( byval hTextBoxControl as HWND )

' ----------------------------------------------------------------------------------------
' Edit behavior.
' ----------------------------------------------------------------------------------------
declare function CTextBox_GetLimitText( byval hTextBoxControl as HWND ) as integer
declare sub      CTextBox_SetLimitText( byval hTextBoxControl as HWND, byval nLimit as integer )
declare function CTextBox_GetReadOnly( byval hTextBoxControl as HWND ) as boolean
declare function CTextBox_SetReadOnly( byval hTextBoxControl as HWND, byval bReadOnly as boolean ) as boolean
declare function CTextBox_GetModify( byval hTextBoxControl as HWND ) as boolean
declare sub      CTextBox_SetModify( byval hTextBoxControl as HWND, byval bModified as boolean )
declare sub      CTextBox_SetPasswordChar( byval hTextBoxControl as HWND, byval wchChar as integer )   ' 0 = off
' Select all text when the control gains keyboard focus (see the TYPE field comment:
' a mouse click still places the caret). Default false.
declare function CTextBox_GetSelectOnFocus( byval hTextBoxControl as HWND ) as boolean
declare sub      CTextBox_SetSelectOnFocus( byval hTextBoxControl as HWND, byval bSelect as boolean )
' Numeric input mode (see the TYPE field comment for the accepted grammar). DecimalPlaces
' 0 means integers only. GetValue/SetValue trade the text as a double; SetValue formats
' to DecimalPlaces and, like every programmatic setter, is silent.
declare function CTextBox_GetNumericMode( byval hTextBoxControl as HWND ) as boolean
declare sub      CTextBox_SetNumericMode( byval hTextBoxControl as HWND, byval bEnable as boolean )
declare function CTextBox_GetDecimalPlaces( byval hTextBoxControl as HWND ) as integer
declare sub      CTextBox_SetDecimalPlaces( byval hTextBoxControl as HWND, byval nPlaces as integer )
declare function CTextBox_GetValue( byval hTextBoxControl as HWND ) as double
declare function CTextBox_SetValue( byval hTextBoxControl as HWND, byval nValue as double ) as boolean
' Numeric mode only: display the formatted zero ("0.00") instead of an empty box. The
' setter stamps it immediately when the box is currently empty and unfocused; after
' that, every focus loss with an empty buffer restores it. Trumps the cue banner.
declare function CTextBox_GetZeroWhenEmpty( byval hTextBoxControl as HWND ) as boolean
declare sub      CTextBox_SetZeroWhenEmpty( byval hTextBoxControl as HWND, byval bEnable as boolean )
declare sub      CTextBox_Cut( byval hTextBoxControl as HWND )
declare sub      CTextBox_Copy( byval hTextBoxControl as HWND )
declare sub      CTextBox_Paste( byval hTextBoxControl as HWND )
declare function CTextBox_CanPaste( byval hTextBoxControl as HWND ) as boolean
declare function CTextBox_Undo( byval hTextBoxControl as HWND ) as boolean
declare function CTextBox_CanUndo( byval hTextBoxControl as HWND ) as boolean
' Left/right margins between the border and the text, in pixels (EM_SETMARGINS).
declare sub      CTextBox_GetMargins( byval hTextBoxControl as HWND, byref nLeft as integer, byref nRight as integer )
declare sub      CTextBox_SetMargins( byval hTextBoxControl as HWND, byval nLeft as integer, byval nRight as integer )

' ----------------------------------------------------------------------------------------
' Appearance. All fonts are caller-owned HFONTs; the control converts the text font to a
' CHARFORMATW internally (face/size/charset/bold/italic + ForeColor) and re-applies it
' whenever the font or forecolor changes.
' ----------------------------------------------------------------------------------------
declare function CTextBox_GetFont( byval hTextBoxControl as HWND ) as HFONT
declare function CTextBox_SetFont( byval hTextBoxControl as HWND, byval hFont as HFONT ) as boolean
declare function CTextBox_GetForeColor( byval hTextBoxControl as HWND ) as COLORREF
declare function CTextBox_SetForeColor( byval hTextBoxControl as HWND, byval clr as COLORREF ) as COLORREF
declare function CTextBox_GetBackColor( byval hTextBoxControl as HWND ) as COLORREF
declare function CTextBox_SetBackColor( byval hTextBoxControl as HWND, byval clr as COLORREF ) as COLORREF
' Cue banner: drawn whenever the buffer is empty, focused or not (the caret blinks over
' it). Its color is independent of the text ForeColor; its font defaults to the text font.
declare function CTextBox_GetCueBannerText( byval hTextBoxControl as HWND ) as DWSTRING
declare sub      CTextBox_SetCueBannerText( byval hTextBoxControl as HWND, byval Text as DWSTRING )
declare function CTextBox_GetCueBannerColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      CTextBox_SetCueBannerColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
declare function CTextBox_GetCueBannerFont( byval hTextBoxControl as HWND ) as HFONT
declare sub      CTextBox_SetCueBannerFont( byval hTextBoxControl as HWND, byval hFont as HFONT )
' Border chrome. Width 0 = borderless; the border color switches to FocusBorderColor
' while the RichEdit has focus. CornerRadius rounds the frame (plain GDI RoundRect;
' keep the radius small). OuterBackColor fills the pixels outside the corner arcs --
' pass the host's background color so rounded corners blend into it.
declare function CTextBox_GetBorderColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      CTextBox_SetBorderColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
declare function CTextBox_GetFocusBorderColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      CTextBox_SetFocusBorderColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
declare function CTextBox_GetBorderWidth( byval hTextBoxControl as HWND ) as integer
declare sub      CTextBox_SetBorderWidth( byval hTextBoxControl as HWND, byval nWidth as integer )
declare function CTextBox_GetCornerRadius( byval hTextBoxControl as HWND ) as integer
declare sub      CTextBox_SetCornerRadius( byval hTextBoxControl as HWND, byval nRadius as integer )
declare function CTextBox_GetOuterBackColor( byval hTextBoxControl as HWND ) as COLORREF
declare sub      CTextBox_SetOuterBackColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
' Localize the built-in right-click menu (Cut/Copy/Paste).
declare sub      CTextBox_SetMenuText( byval hTextBoxControl as HWND, byval CutText as DWSTRING, byval CopyText as DWSTRING, byval PasteText as DWSTRING )

' ----------------------------------------------------------------------------------------
' Callbacks. See the type declarations above for each signature and contract.
'   ChangeCallback       - text changed by USER interaction (programmatic changes silent).
'   FocusCallback        - RichEdit gained/lost keyboard focus.
'   EnterPressedCallback - ENTER pressed (keypress always swallowed).
'   MessageCallback      - observe key/mouse/focus/context-menu messages; TRUE suppresses.
' ----------------------------------------------------------------------------------------
declare sub      CTextBox_SetChangeCallback( byval hTextBoxControl as HWND, byval usersub as TXT_ChangeCallbackSub )
declare sub      CTextBox_SetFocusCallback( byval hTextBoxControl as HWND, byval usersub as TXT_FocusCallbackSub )
declare sub      CTextBox_SetEnterPressedCallback( byval hTextBoxControl as HWND, byval usersub as TXT_EnterPressedCallbackSub )
declare sub      CTextBox_SetMessageCallback( byval hTextBoxControl as HWND, byval userfunc as TXT_MessageCallbackFunc )
