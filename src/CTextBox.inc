
''
''  CTextBox.inc
''
''  Textbox custom control built on a RichEdit50W child -- single-line by default,
''  optionally multiline (word-wrapped, creation-time choice). RichEdit rather
''  than EDIT because it supports vertical centering of the text (EM_SETRECT) and
''  per-control colors (EM_SETCHARFORMAT / EM_SETBKGNDCOLOR) without owner-draw.
''  The container owns the chrome (border, rounded corners, outer fill) and the
''  geometry; the RichEdit child does all the editing. A multiline control shows no
''  native scrollbar -- an external scrollbar (CVScrollBar) is driven through
''  ScrollChangedCallback / GetVScrollInfo / ScrollToLine.
''

#include once "CTextBox.bi"


function GetTextBoxPointer( byval hTextBoxControl as HWND ) as CTEXTBOX ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hTextBoxControl)
    dim as CTEXTBOX ptr pTextBox = null
    if pWindow then pTextBox = cast( CTEXTBOX ptr, pWindow->UserData(0) )
    return pTextBox
end function


' ========================================================================================
' THE BUILT-IN CONTEXT MENU
'
' Replaces the CreatePopupMenu/TrackPopupMenu block that used to sit inline in the
' RichEdit subclass. The visible win is a themeable menu that matches the rest of the
' family; the structural cost is that CPopupMenu is NOT modal. TrackPopupMenu returned
' the chosen id, so cut/copy/paste ran in the same stack frame as WM_CONTEXTMENU; now
' Show returns immediately and the command runs later, from the select callback.
'
' The callback signature carries no user data, so the target RichEdit is parked here at
' show time. Safe because CPopupMenu enforces ONE open chain per application: a second
' textbox's menu closes the first, and a closed menu never fires a select.
' ========================================================================================
dim shared as HWND gTB_ContextMenuTarget   ' RichEdit whose menu is currently open

' Item ids. Local to this menu (they never leave the control), 1-based because 0 is
' CPopupMenu's separator marker.
#define CTEXTBOX_IDM_CUT         1
#define CTEXTBOX_IDM_COPY        2
#define CTEXTBOX_IDM_PASTE       3
#define CTEXTBOX_IDM_SELECTALL   4

private sub CTextBox_ContextMenuSelect( byval hPopup as HWND, byval id as long )
    dim as HWND hWin = gTB_ContextMenuTarget
    if hWin = 0 then exit sub
    if IsWindow( hWin ) = 0 then exit sub
    select case id
        case CTEXTBOX_IDM_CUT:       SendMessage( hWin, WM_CUT, 0, 0 )
        case CTEXTBOX_IDM_COPY:      SendMessage( hWin, WM_COPY, 0, 0 )
        case CTEXTBOX_IDM_PASTE:     SendMessage( hWin, WM_PASTE, 0, 0 )
        case CTEXTBOX_IDM_SELECTALL: SendMessage( hWin, EM_SETSEL, 0, -1 )
    end select
end sub

' Rebuild from the current selection / clipboard / read-only state and show at the
' cursor (which also covers the keyboard menu key well enough). An empty menu -- nothing
' selected, nothing to paste, no text -- shows nothing at all, as before.
private sub CTextBox_ShowContextMenu( byval pTextBox as CTEXTBOX ptr )
    if pTextBox = null then exit sub
    if pTextBox->hContextMenu = 0 then exit sub

    dim as HWND hWin = pTextBox->hRichEdit
    dim as HWND hPopup = pTextBox->hContextMenu
    dim as boolean bReadOnly = ( (GetWindowLongPtr(hWin, GWL_STYLE) and ES_READONLY) <> 0 )
    dim as DWSTRING wszSelText = RichEdit_GetSelText( hWin )
    dim as boolean bHasSel = (len(wszSelText) > 0)

    CPopupMenu_Clear( hPopup )

    if bHasSel andalso (bReadOnly = false) then
        CPopupMenu_AddItem( hPopup, CTEXTBOX_IDM_CUT, pTextBox->wszMenuCut )
    end if
    if bHasSel then
        CPopupMenu_AddItem( hPopup, CTEXTBOX_IDM_COPY, pTextBox->wszMenuCopy )
    end if
    if (RichEdit_CanPaste(hWin, 0) <> 0) andalso (bReadOnly = false) then
        if CPopupMenu_GetItemCount(hPopup) > 0 then CPopupMenu_AddSeparator( hPopup )
        CPopupMenu_AddItem( hPopup, CTEXTBOX_IDM_PASTE, pTextBox->wszMenuPaste )
    end if
    ' Select All: offered whenever the control holds any text (read-only included --
    ' selecting is always allowed).
    if GetWindowTextLength(hWin) > 0 then
        if CPopupMenu_GetItemCount(hPopup) > 0 then CPopupMenu_AddSeparator( hPopup )
        CPopupMenu_AddItem( hPopup, CTEXTBOX_IDM_SELECTALL, pTextBox->wszMenuSelectAll )
    end if

    if CPopupMenu_GetItemCount(hPopup) = 0 then exit sub

    dim as POINT pt
    GetCursorPos( @pt )
    gTB_ContextMenuTarget = hWin
    CPopupMenu_Show( hPopup, pt.x, pt.y )
end sub


' ========================================================================================
' The context menu's public surface. See the contract block in CTextBox.bi -- the filter
' is an OBLIGATION on the host's message pump, not an optional extra.
' ========================================================================================
function CTextBox_GetContextMenu( byval hTextBoxControl as HWND ) as HWND
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->hContextMenu
end function

' Resolve the popup of whichever textbox last opened its menu (0 if none). Both the
' filter and the close path need it.
private function CTextBox_OpenContextMenu() as HWND
    if gTB_ContextMenuTarget = 0 then return 0
    if IsWindow( gTB_ContextMenuTarget ) = 0 then
        gTB_ContextMenuTarget = 0
        return 0
    end if
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer( GetParent(gTB_ContextMenuTarget) )
    if pTextBox = null then return 0
    return pTextBox->hContextMenu
end function

function CTextBox_FilterMessage( byval pMsg as MSG ptr ) as boolean
    ' One call serves every CTextBox: only one chain can be open, and the last-shown
    ' target identifies it. CPopupMenu_FilterMessage is self-guarding -- it stands down
    ' unless the handle really is in the currently open chain -- so a stale handle here
    ' costs one comparison and nothing else.
    dim as HWND hPopup = CTextBox_OpenContextMenu()
    if hPopup = 0 then return false
    return CPopupMenu_FilterMessage( hPopup, pMsg )
end function

sub CTextBox_CloseContextMenu()
    dim as HWND hPopup = CTextBox_OpenContextMenu()
    if hPopup then CPopupMenu_CloseChain( hPopup )
    gTB_ContextMenuTarget = 0
end sub


' ========================================================================================
' Convert the caller's HFONT (or the stock DEFAULT_GUI_FONT) plus ForeColor into a
' CHARFORMATW and apply it to the WHOLE buffer. This is the only place character
' formatting is produced; SCF_ALL plus TM_PLAINTEXT mode keeps the entire buffer in
' this one format -- the control never colors individual characters.
' ========================================================================================
private sub CTextBox_ApplyCharFormat( byval pTextBox as CTEXTBOX ptr )
    if pTextBox = null then exit sub
    if pTextBox->hRichEdit = 0 then exit sub

    dim as HFONT hFontUse = pTextBox->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject(DEFAULT_GUI_FONT) )
    dim as LOGFONTW lf
    if GetObjectW( hFontUse, sizeof(LOGFONTW), @lf ) = 0 then exit sub

    ' lfHeight is negative for character height (the usual CreateFont case); a positive
    ' value is cell height, for which the point size below is a close approximation.
    dim as HDC hdc = GetDC( pTextBox->hRichEdit )
    dim as integer dpiY = GetDeviceCaps( hdc, LOGPIXELSY )
    ReleaseDC( pTextBox->hRichEdit, hdc )
    dim as integer pointSize = MulDiv( abs(lf.lfHeight), 72, dpiY )

    dim cf as CHARFORMATW
    cf.cbSize = sizeof(cf)
    cf.dwMask = CFM_FACE or CFM_SIZE or CFM_CHARSET or CFM_COLOR or _
                CFM_BOLD or CFM_ITALIC or CFM_UNDERLINE or CFM_STRIKEOUT
    cf.crTextColor = pTextBox->ForeColor
    cf.yHeight = pointSize * 20               ' twips (1/20 pt)
    cf.bCharSet = lf.lfCharSet
    cf.bPitchAndFamily = lf.lfPitchAndFamily
    cf.szFaceName = lf.lfFaceName
    if lf.lfWeight >= FW_BOLD then cf.dwEffects or= CFE_BOLD
    if lf.lfItalic then cf.dwEffects or= CFE_ITALIC
    if lf.lfUnderline then cf.dwEffects or= CFE_UNDERLINE
    if lf.lfStrikeOut then cf.dwEffects or= CFE_STRIKEOUT
    SendMessage( pTextBox->hRichEdit, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) )
end sub


' ========================================================================================
' Center the (single) text line vertically by raising the formatting rect's top
' (EM_SETRECT). Works on a single-line RichEdit even though the classic EDIT would
' ignore it -- proven in tiko's search box, which this code is ported from.
' Single-line only: a multiline control never calls EM_SETRECT, so its formatting rect
' keeps tracking the client area automatically and text starts at the top.
' ========================================================================================
private sub CTextBox_CenterSingleLineText( byval pTextBox as CTEXTBOX ptr )
    if pTextBox = null then exit sub
    if pTextBox->bMultiline then exit sub
    dim as HWND hRichEdit = pTextBox->hRichEdit
    if hRichEdit = 0 then exit sub

    dim as RECT rc
    GetClientRect( hRichEdit, @rc )
    if (rc.bottom - rc.top) <= 0 then exit sub

    dim as HFONT hFontUse = pTextBox->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject(DEFAULT_GUI_FONT) )
    dim as HDC hdc = GetDC( hRichEdit )
    if hdc = 0 then exit sub
    dim as HFONT hOldFont = SelectObject( hdc, hFontUse )
    dim as TEXTMETRIC tm
    GetTextMetrics( hdc, @tm )
    SelectObject( hdc, hOldFont )
    ReleaseDC( hRichEdit, hdc )

    dim as long topMargin = ((rc.bottom - rc.top) - tm.tmHeight) / 2
    if topMargin < 0 then topMargin = 0
    rc.top = topMargin
    SendMessage( hRichEdit, EM_SETRECT, 0, cast(LPARAM, @rc) )
end sub


' ========================================================================================
' Push the stored left/right margins to the RichEdit. Kept in the TYPE (not only in the
' control) so they can be re-applied after every EM_SETRECT: the formatting rect and
' the margins are set through different messages and geometry changes redo both.
' ========================================================================================
private sub CTextBox_ApplyMargins( byval pTextBox as CTEXTBOX ptr )
    if pTextBox = null then exit sub
    if pTextBox->hRichEdit = 0 then exit sub
    SendMessage( pTextBox->hRichEdit, EM_SETMARGINS, _
                 EC_LEFTMARGIN or EC_RIGHTMARGIN, _
                 MAKELPARAM( pTextBox->nMarginLeft, pTextBox->nMarginRight ) )
end sub


' ========================================================================================
' Derive the RichEdit child's rect from the container's client rect (inset by the
' border width on all sides), then redo vertical centering and margins. This is the
' only place the child geometry is produced -- rects are derived, never set.
' ========================================================================================
private sub CTextBox_PositionRichEdit( byval pTextBox as CTEXTBOX ptr )
    if pTextBox = null then exit sub
    if pTextBox->hRichEdit = 0 then exit sub

    dim as RECT rc
    GetClientRect( pTextBox->hWin, @rc )
    dim as integer bw = pTextBox->nBorderWidth
    dim as integer w = (rc.right - rc.left) - (bw * 2)
    dim as integer h = (rc.bottom - rc.top) - (bw * 2)
    if w < 0 then w = 0
    if h < 0 then h = 0
    SetWindowPos( pTextBox->hRichEdit, 0, rc.left + bw, rc.top + bw, w, h, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )
    CTextBox_CenterSingleLineText( pTextBox )
    CTextBox_ApplyMargins( pTextBox )
end sub


' ========================================================================================
' Format a numeric value with the specified decimal places. Ported from tracks'
' AfxMoney and renamed (it never was an AfxNova function). bUseComma inserts thousands
' separators; the control itself always formats WITHOUT them, since "," is not an
' accepted input character.
' ========================================================================================
private function CTextBox_FormatNumeric( byval nValue as double, byval nDecimalPlaces as integer, byval bUseComma as boolean ) as DWSTRING
    dim as string fmt = "0"
    if bUseComma then fmt = "#,##" & fmt
    if nDecimalPlaces > 0 then fmt = fmt & "." & string(nDecimalPlaces, "0")
    return format( nValue, fmt )
end function


' ========================================================================================
' Is wszText acceptable as (possibly partial) numeric input? Grammar: optional leading
' minus, digits, at most one period followed by at most nDecimalPlaces digits (no
' period at all when nDecimalPlaces = 0). Empty and "-" are valid intermediate states.
' Used to vet a paste before it lands.
' ========================================================================================
private function CTextBox_IsValidNumericText( byval pTextBox as CTEXTBOX ptr, byref wszText as DWSTRING ) as boolean
    dim as integer periodAt = 0
    for i as integer = 1 to len(wszText)
        select case asc(mid(wszText, i, 1))
            case asc("0") to asc("9")
            case asc("-")
                if i <> 1 then return false
            case asc(".")
                if pTextBox->nDecimalPlaces = 0 then return false
                if periodAt > 0 then return false
                periodAt = i
            case else
                return false
        end select
    next
    if periodAt > 0 then
        if (len(wszText) - periodAt) > pTextBox->nDecimalPlaces then return false
    end if
    return true
end function


' ========================================================================================
' Retrieve the RichEdit child's handle. Escape hatch for RichEdit-specific messages the
' flat API and the forwarded EM_ range do not cover.
' ========================================================================================
function CTextBox_GetRichEditHandle( byval hTextBoxControl as HWND ) as HWND
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->hRichEdit
end function


' ========================================================================================
' Is this a multiline control? (Fixed at creation.)
' ========================================================================================
function CTextBox_GetMultiline( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return pTextBox->bMultiline
end function


' ========================================================================================
' Does this textbox own the keyboard focus? (It does when its RichEdit child does.)
' ========================================================================================
function CTextBox_HasFocus( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( GetFocus() = pTextBox->hRichEdit )
end function


' ========================================================================================
' Retrieve the control's text.
' ========================================================================================
function CTextBox_GetText( byval hTextBoxControl as HWND ) as DWSTRING
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return ""
    return RichEdit_GetText( pTextBox->hRichEdit )
end function


' ========================================================================================
' Set the control's text. Programmatic: does NOT fire the ChangeCallback.
' ========================================================================================
function CTextBox_SetText( byval hTextBoxControl as HWND, byval Text as DWSTRING ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    pTextBox->bInternalChange = true
    SendMessage( pTextBox->hRichEdit, WM_SETTEXT, 0, cast(LPARAM, Text.vptr) )
    pTextBox->bInternalChange = false
    return true
end function


' ========================================================================================
' Retrieve the length of the control's text, in characters.
' ========================================================================================
function CTextBox_GetTextLength( byval hTextBoxControl as HWND ) as integer
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return GetWindowTextLength( pTextBox->hRichEdit )
end function


' ========================================================================================
' Retrieve the currently selected text ("" if no selection).
' ========================================================================================
function CTextBox_GetSelText( byval hTextBoxControl as HWND ) as DWSTRING
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return ""
    return RichEdit_GetSelText( pTextBox->hRichEdit )
end function


' ========================================================================================
' Replace the current selection (or insert at the caret when there is none).
' Programmatic: does NOT fire the ChangeCallback. The replace is undoable.
' ========================================================================================
function CTextBox_ReplaceSel( byval hTextBoxControl as HWND, byval Text as DWSTRING ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    pTextBox->bInternalChange = true
    SendMessage( pTextBox->hRichEdit, EM_REPLACESEL, true, cast(LPARAM, Text.vptr) )
    pTextBox->bInternalChange = false
    return true
end function


' ========================================================================================
' Clear all text. Programmatic: does NOT fire the ChangeCallback.
' ========================================================================================
function CTextBox_Clear( byval hTextBoxControl as HWND ) as boolean
    return CTextBox_SetText( hTextBoxControl, "" )
end function


' ========================================================================================
' Retrieve the selection as 0-based character indices (nStart = nEnd means a caret).
' ========================================================================================
sub CTextBox_GetSel( byval hTextBoxControl as HWND, byref nStart as integer, byref nEnd as integer )
    nStart = 0: nEnd = 0
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    dim as DWORD dwStart, dwEnd
    SendMessage( pTextBox->hRichEdit, EM_GETSEL, cast(WPARAM, @dwStart), cast(LPARAM, @dwEnd) )
    nStart = dwStart: nEnd = dwEnd
end sub


' ========================================================================================
' Set the selection. nEnd = -1 selects through the end of the text.
' ========================================================================================
sub CTextBox_SetSel( byval hTextBoxControl as HWND, byval nStart as integer, byval nEnd as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, EM_SETSEL, nStart, nEnd )
end sub


' ========================================================================================
' Select all text.
' ========================================================================================
sub CTextBox_SelectAll( byval hTextBoxControl as HWND )
    CTextBox_SetSel( hTextBoxControl, 0, -1 )
end sub


' ========================================================================================
' Text length limit (characters). 0 restores the RichEdit default limit.
' ========================================================================================
function CTextBox_GetLimitText( byval hTextBoxControl as HWND ) as integer
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return SendMessage( pTextBox->hRichEdit, EM_GETLIMITTEXT, 0, 0 )
end function

sub CTextBox_SetLimitText( byval hTextBoxControl as HWND, byval nLimit as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, EM_EXLIMITTEXT, 0, nLimit )
end sub


' ========================================================================================
' Read-only state.
' ========================================================================================
function CTextBox_GetReadOnly( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( (GetWindowLongPtr( pTextBox->hRichEdit, GWL_STYLE ) and ES_READONLY) <> 0 )
end function

function CTextBox_SetReadOnly( byval hTextBoxControl as HWND, byval bReadOnly as boolean ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( SendMessage( pTextBox->hRichEdit, EM_SETREADONLY, bReadOnly, 0 ) <> 0 )
end function


' ========================================================================================
' Modified flag (set by user edits, cleared by WM_SETTEXT or SetModify(false)).
' ========================================================================================
function CTextBox_GetModify( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( SendMessage( pTextBox->hRichEdit, EM_GETMODIFY, 0, 0 ) <> 0 )
end function

sub CTextBox_SetModify( byval hTextBoxControl as HWND, byval bModified as boolean )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, EM_SETMODIFY, bModified, 0 )
end sub


' ========================================================================================
' Password character (0 turns password mode off).
' ========================================================================================
sub CTextBox_SetPasswordChar( byval hTextBoxControl as HWND, byval wchChar as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, EM_SETPASSWORDCHAR, wchChar, 0 )
    InvalidateRect( pTextBox->hRichEdit, null, true )
end sub


' ========================================================================================
' Select-all-on-focus behavior.
' ========================================================================================
function CTextBox_GetSelectOnFocus( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return pTextBox->bSelectOnFocus
end function

sub CTextBox_SetSelectOnFocus( byval hTextBoxControl as HWND, byval bSelect as boolean )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->bSelectOnFocus = bSelect
end sub


' ========================================================================================
' Numeric input mode.
' ========================================================================================
function CTextBox_GetNumericMode( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return pTextBox->bNumericOnly
end function

sub CTextBox_SetNumericMode( byval hTextBoxControl as HWND, byval bEnable as boolean )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    if pTextBox->bMultiline then exit sub    ' numeric mode is single-line only
    pTextBox->bNumericOnly = bEnable
end sub

function CTextBox_GetDecimalPlaces( byval hTextBoxControl as HWND ) as integer
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->nDecimalPlaces
end function

sub CTextBox_SetDecimalPlaces( byval hTextBoxControl as HWND, byval nPlaces as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    if nPlaces < 0 then nPlaces = 0
    pTextBox->nDecimalPlaces = nPlaces
end sub

' The current text as a double (val of the text; empty/"-"/"." read as 0).
function CTextBox_GetValue( byval hTextBoxControl as HWND ) as double
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return val( RichEdit_GetText( pTextBox->hRichEdit ) )
end function

' Set the text from a double, formatted to DecimalPlaces. Programmatic: silent.
function CTextBox_SetValue( byval hTextBoxControl as HWND, byval nValue as double ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return CTextBox_SetText( hTextBoxControl, _
                CTextBox_FormatNumeric( nValue, pTextBox->nDecimalPlaces, false ) )
end function

' Display the formatted zero instead of an empty box (numeric mode only).
function CTextBox_GetZeroWhenEmpty( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return pTextBox->bZeroWhenEmpty
end function

sub CTextBox_SetZeroWhenEmpty( byval hTextBoxControl as HWND, byval bEnable as boolean )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->bZeroWhenEmpty = bEnable
    ' Apply immediately when the box is already empty and not being edited; a focused
    ' box is left alone (the user may be mid-entry) -- its focus loss will stamp it.
    if bEnable andalso pTextBox->bNumericOnly then
        if (GetWindowTextLength(pTextBox->hRichEdit) = 0) andalso (GetFocus() <> pTextBox->hRichEdit) then
            CTextBox_SetValue( hTextBoxControl, 0 )
        end if
    end if
end sub


' ========================================================================================
' Clipboard and undo.
' ========================================================================================
sub CTextBox_Cut( byval hTextBoxControl as HWND )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, WM_CUT, 0, 0 )
end sub

sub CTextBox_Copy( byval hTextBoxControl as HWND )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, WM_COPY, 0, 0 )
end sub

sub CTextBox_Paste( byval hTextBoxControl as HWND )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    SendMessage( pTextBox->hRichEdit, WM_PASTE, 0, 0 )
end sub

function CTextBox_CanPaste( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( RichEdit_CanPaste( pTextBox->hRichEdit, 0 ) <> 0 )
end function

function CTextBox_Undo( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( SendMessage( pTextBox->hRichEdit, EM_UNDO, 0, 0 ) <> 0 )
end function

function CTextBox_CanUndo( byval hTextBoxControl as HWND ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    return ( SendMessage( pTextBox->hRichEdit, EM_CANUNDO, 0, 0 ) <> 0 )
end function


' ========================================================================================
' Left/right text margins, in pixels.
' ========================================================================================
sub CTextBox_GetMargins( byval hTextBoxControl as HWND, byref nLeft as integer, byref nRight as integer )
    nLeft = 0: nRight = 0
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    nLeft = pTextBox->nMarginLeft
    nRight = pTextBox->nMarginRight
end sub

sub CTextBox_SetMargins( byval hTextBoxControl as HWND, byval nLeft as integer, byval nRight as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->nMarginLeft = nLeft
    pTextBox->nMarginRight = nRight
    CTextBox_ApplyMargins( pTextBox )
    InvalidateRect( pTextBox->hRichEdit, null, true )
end sub


' ========================================================================================
' Vertical scroll state, in LINES -- shaped 1:1 for CVScrollBar_SetRange(total, page, pos).
' LinesPerPage derives from the formatting-rect height and the text font's line height
' (TEXTMETRIC height + external leading, which is how the RichEdit spaces single-format
' plain-text lines); a partial line at the bottom is not counted. No fudge factors --
' the harness asserts these numbers against hand counts.
' ========================================================================================
sub CTextBox_GetVScrollInfo( byval hTextBoxControl as HWND, byref nTotalLines as integer, byref nLinesPerPage as integer, byref nFirstVisibleLine as integer )
    nTotalLines = 0: nLinesPerPage = 0: nFirstVisibleLine = 0
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    dim as HWND hRichEdit = pTextBox->hRichEdit
    if hRichEdit = 0 then exit sub

    nTotalLines = SendMessage( hRichEdit, EM_GETLINECOUNT, 0, 0 )
    nFirstVisibleLine = SendMessage( hRichEdit, EM_GETFIRSTVISIBLELINE, 0, 0 )

    dim as RECT rcFormat
    SendMessage( hRichEdit, EM_GETRECT, 0, cast(LPARAM, @rcFormat) )
    dim as integer rectHeight = rcFormat.bottom - rcFormat.top
    if rectHeight <= 0 then exit sub

    dim as HFONT hFontUse = pTextBox->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject(DEFAULT_GUI_FONT) )
    dim as HDC hdc = GetDC( hRichEdit )
    if hdc = 0 then exit sub
    dim as HFONT hOldFont = SelectObject( hdc, hFontUse )
    dim as TEXTMETRIC tm
    GetTextMetrics( hdc, @tm )
    SelectObject( hdc, hOldFont )
    ReleaseDC( hRichEdit, hdc )

    dim as integer lineHeight = tm.tmHeight + tm.tmExternalLeading
    if lineHeight <= 0 then exit sub         ' guard BEFORE dividing (see Learnings.md)
    nLinesPerPage = rectHeight \ lineHeight
end sub


' ========================================================================================
' Scroll so nLine (0-based) becomes the first visible line -- the receiving end of a
' scrollbar's position callback (CVScrollBar's ScrollCallback newPos goes straight here).
' EM_LINESCROLL takes a DELTA, so the current first line is subtracted; the RichEdit
' clamps overscroll itself.
' ========================================================================================
sub CTextBox_ScrollToLine( byval hTextBoxControl as HWND, byval nLine as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    if pTextBox->hRichEdit = 0 then exit sub
    if nLine < 0 then nLine = 0
    dim as integer nFirst = SendMessage( pTextBox->hRichEdit, EM_GETFIRSTVISIBLELINE, 0, 0 )
    if nLine <> nFirst then
        SendMessage( pTextBox->hRichEdit, EM_LINESCROLL, 0, cast(LPARAM, nLine - nFirst) )
    end if
end sub


' ========================================================================================
' Text font (caller-owned HFONT; 0 = stock DEFAULT_GUI_FONT).
' ========================================================================================
function CTextBox_GetFont( byval hTextBoxControl as HWND ) as HFONT
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->hTextFont
end function

function CTextBox_SetFont( byval hTextBoxControl as HWND, byval hFont as HFONT ) as boolean
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return false
    pTextBox->hTextFont = hFont
    CTextBox_ApplyCharFormat( pTextBox )
    CTextBox_CenterSingleLineText( pTextBox )
    CTextBox_ApplyMargins( pTextBox )
    InvalidateRect( pTextBox->hRichEdit, null, true )
    return true
end function


' ========================================================================================
' Text forecolor (one color for ALL text).
' ========================================================================================
function CTextBox_GetForeColor( byval hTextBoxControl as HWND ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->ForeColor
end function

function CTextBox_SetForeColor( byval hTextBoxControl as HWND, byval clr as COLORREF ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    dim as COLORREF oldclr = pTextBox->ForeColor
    pTextBox->ForeColor = clr
    CTextBox_ApplyCharFormat( pTextBox )
    return oldclr
end function


' ========================================================================================
' Background color (the text area; the border/outer chrome has its own colors).
' ========================================================================================
function CTextBox_GetBackColor( byval hTextBoxControl as HWND ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->BackColor
end function

function CTextBox_SetBackColor( byval hTextBoxControl as HWND, byval clr as COLORREF ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    dim as COLORREF oldclr = pTextBox->BackColor
    pTextBox->BackColor = clr
    SendMessage( pTextBox->hRichEdit, EM_SETBKGNDCOLOR, 0, cast(LPARAM, clr) )
    InvalidateRect( pTextBox->hWin, null, true )
    return oldclr
end function


' ========================================================================================
' Cue banner: text, color, font.
' ========================================================================================
function CTextBox_GetCueBannerText( byval hTextBoxControl as HWND ) as DWSTRING
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return ""
    return pTextBox->CueText
end function

sub CTextBox_SetCueBannerText( byval hTextBoxControl as HWND, byval Text as DWSTRING )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->CueText = Text
    InvalidateRect( pTextBox->hRichEdit, null, true )
end sub

function CTextBox_GetCueBannerColor( byval hTextBoxControl as HWND ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->CueColor
end function

sub CTextBox_SetCueBannerColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->CueColor = clr
    InvalidateRect( pTextBox->hRichEdit, null, true )
end sub

function CTextBox_GetCueBannerFont( byval hTextBoxControl as HWND ) as HFONT
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->hCueFont
end function

sub CTextBox_SetCueBannerFont( byval hTextBoxControl as HWND, byval hFont as HFONT )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->hCueFont = hFont
    InvalidateRect( pTextBox->hRichEdit, null, true )
end sub


' ========================================================================================
' Border chrome: colors, width, corner radius, outer fill.
' ========================================================================================
function CTextBox_GetBorderColor( byval hTextBoxControl as HWND ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->BorderColor
end function

sub CTextBox_SetBorderColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->BorderColor = clr
    InvalidateRect( pTextBox->hWin, null, true )
end sub

function CTextBox_GetFocusBorderColor( byval hTextBoxControl as HWND ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->FocusBorderColor
end function

sub CTextBox_SetFocusBorderColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->FocusBorderColor = clr
    InvalidateRect( pTextBox->hWin, null, true )
end sub

function CTextBox_GetBorderWidth( byval hTextBoxControl as HWND ) as integer
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->nBorderWidth
end function

sub CTextBox_SetBorderWidth( byval hTextBoxControl as HWND, byval nWidth as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    if nWidth < 0 then nWidth = 0
    pTextBox->nBorderWidth = nWidth
    CTextBox_PositionRichEdit( pTextBox )    ' the child inset derives from the width
    InvalidateRect( pTextBox->hWin, null, true )
end sub

function CTextBox_GetCornerRadius( byval hTextBoxControl as HWND ) as integer
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->nCornerRadius
end function

sub CTextBox_SetCornerRadius( byval hTextBoxControl as HWND, byval nRadius as integer )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    if nRadius < 0 then nRadius = 0
    pTextBox->nCornerRadius = nRadius
    InvalidateRect( pTextBox->hWin, null, true )
end sub

function CTextBox_GetOuterBackColor( byval hTextBoxControl as HWND ) as COLORREF
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then return 0
    return pTextBox->OuterBackColor
end function

sub CTextBox_SetOuterBackColor( byval hTextBoxControl as HWND, byval clr as COLORREF )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->OuterBackColor = clr
    InvalidateRect( pTextBox->hWin, null, true )
end sub


' ========================================================================================
' Localize the built-in context menu. An empty SelectAllText keeps the current Select All
' label, so pre-existing 3-argument callers keep their default.
' ========================================================================================
sub CTextBox_SetMenuText( byval hTextBoxControl as HWND, byval CutText as DWSTRING, byval CopyText as DWSTRING, byval PasteText as DWSTRING, byval SelectAllText as DWSTRING = "" )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->wszMenuCut = CutText
    pTextBox->wszMenuCopy = CopyText
    pTextBox->wszMenuPaste = PasteText
    if len(SelectAllText) > 0 then pTextBox->wszMenuSelectAll = SelectAllText
end sub


' ========================================================================================
' Callback setters.
' ========================================================================================
sub CTextBox_SetChangeCallback( byval hTextBoxControl as HWND, byval usersub as TXT_ChangeCallbackSub )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->ChangeCallback = usersub
end sub

sub CTextBox_SetFocusCallback( byval hTextBoxControl as HWND, byval usersub as TXT_FocusCallbackSub )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->FocusCallback = usersub
end sub

sub CTextBox_SetEnterPressedCallback( byval hTextBoxControl as HWND, byval usersub as TXT_EnterPressedCallbackSub )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->EnterPressedCallback = usersub
end sub

sub CTextBox_SetMessageCallback( byval hTextBoxControl as HWND, byval userfunc as TXT_MessageCallbackFunc )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->MessageCallback = userfunc
end sub

sub CTextBox_SetScrollChangedCallback( byval hTextBoxControl as HWND, byval usersub as TXT_ScrollChangedCallbackSub )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hTextBoxControl)
    if pTextBox = null then exit sub
    pTextBox->ScrollChangedCallback = usersub
end sub


' ========================================================================================
' RichEdit child subclass procedure. Cue banner painting, single-line enforcement,
' Enter/focus events, built-in context menu, and the host MessageCallback with veto.
'
' The control takes NO mouse capture: the RichEdit manages its own selection-drag
' capture internally, so there is no press state of ours a suppressed message could
' strand -- which is why the MessageCallback result is honored uniformly.
' ========================================================================================
function CTextBoxRichEdit_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _              ' // First message parameter
            byval _lParam as LPARAM, _              ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim as HWND hCtrl = GetParent( hWin )
    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer( hCtrl )

    ' Host observation with veto, before the control acts on the message.
    if (pTextBox <> null) andalso (pTextBox->MessageCallback <> null) then
        select case uMsg
            case WM_KEYDOWN, WM_KEYUP, WM_CHAR, _
                 WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, _
                 WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, _
                 WM_MOUSEMOVE, WM_MOUSEWHEEL, WM_CONTEXTMENU, WM_SETFOCUS, WM_KILLFOCUS
                dim m as CTEXTBOX_MESSAGEINFO
                m.hTextBox = hCtrl
                m.uMsg = uMsg
                m.wParam = _wParam
                m.lParam = _lParam
                if pTextBox->MessageCallback( @m ) then return 0
        end select
    end if

    select case uMsg

        case WM_PRINTCLIENT, WM_PAINT
            ' Cue banner: drawn whenever the buffer is empty (focused or not).
            dim as long textLength = GetWindowTextLength( hWin )
            dim as boolean bHasCue = false
            if pTextBox then bHasCue = ( len(pTextBox->CueText) > 0 )
            if (textLength = 0) andalso bHasCue then
                ' Get the needed DC with DCX_INTERSECTUPDATE before the RichEdit's
                ' WM_PAINT handler calls BeginPaint/EndPaint, which validates the update
                ' rect and would otherwise lead to drawing nothing later because the
                ' region is empty. Also, grab it from the cache so we don't mess with
                ' the RichEdit's DC.
                dim as HDC hdc = _
                    iif( uMsg = WM_PRINTCLIENT, GetDC(hWin), _
                         GetDCEx(hWin, 0, DCX_INTERSECTUPDATE or DCX_CACHE or DCX_CLIPCHILDREN or DCX_CLIPSIBLINGS) )

                ' Call the RichEdit so that the caret is properly handled -- no caret
                ' litter left on the screen after tabbing away.
                dim as LRESULT paintResult = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

                SaveDC( hdc )

                ' Draw at the formatting rect so the cue sits exactly where typed text
                ' will appear (the rect already carries the vertical centering).
                dim as RECT editRect
                SendMessage( hWin, EM_GETRECT, 0, cast(LPARAM, @editRect) )

                dim as HFONT hFontUse = pTextBox->hCueFont
                if hFontUse = 0 then hFontUse = pTextBox->hTextFont
                if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject(DEFAULT_GUI_FONT) )
                SelectObject( hdc, hFontUse )
                SetTextColor( hdc, pTextBox->CueColor )
                SetBkMode( hdc, TRANSPARENT )
                ' Single-line: one centered-by-rect line (the rect carries the centering,
                ' NOCLIP because it may start above 0). Multiline: word-wrap at the top
                ' left, clipped to the formatting rect.
                dim as long wsStyle = DT_TOP or DT_LEFT or DT_NOPREFIX
                if pTextBox->bMultiline then
                    wsStyle or= DT_WORDBREAK
                else
                    wsStyle or= DT_NOCLIP or DT_SINGLELINE
                end if
                DrawText( hdc, pTextBox->CueText.vptr, -1, @editRect, wsStyle )
                RestoreDC( hdc, -1 )
                ReleaseDC( hWin, hdc )

                return paintResult
            end if

        case WM_CHAR
            ' Multiline lets ENTER (newline) and TAB (tab character) through to the
            ' RichEdit; single line swallows both (no newline/beep; Tab is navigation,
            ' handled on WM_KEYDOWN below).
            dim as boolean bSingleLine = true
            if pTextBox then bSingleLine = (pTextBox->bMultiline = false)
            if bSingleLine then
                if _wParam = 13 then return 0
                if _wParam = 9 then return 0
            end if
            ' ESCAPE would beep (both modes); a host acts on it via the MessageCallback
            ' if at all.
            if _wParam = 27 then return 0

            ' Numeric mode: filter printable characters (ported from tracks'
            ' NumericTextBox_SubclassProc, tightened). Control characters below 32
            ' (backspace, Ctrl+C/V/X/Z/A...) always pass -- the tracks version blocked
            ' them, which broke clipboard shortcuts. Paste itself is vetted in WM_PASTE.
            if (pTextBox <> null) andalso (_wParam >= 32) then
                if pTextBox->bNumericOnly then
                    ' Work against the text as it will be AFTER the pending selection is
                    ' replaced: caret position + remaining text decide validity.
                    dim as DWORD dwStart, dwEnd
                    SendMessage( hWin, EM_GETSEL, cast(WPARAM, @dwStart), cast(LPARAM, @dwEnd) )
                    dim as DWSTRING wszText = RichEdit_GetText( hWin )
                    if dwEnd > dwStart then
                        wszText = left(wszText, dwStart) & mid(wszText, dwEnd + 1)
                    end if

                    select case _wParam
                        case asc("0") to asc("9")
                            ' Nothing may land in front of an existing leading minus.
                            if (dwStart = 0) andalso (left(wszText, 1) = "-") then return 0
                            ' In the fractional part, respect the decimal-places limit.
                            dim as integer foundat = instr( wszText, "." )
                            if (foundat > 0) andalso (dwStart >= foundat) then
                                if len(mid(wszText, foundat + 1)) >= pTextBox->nDecimalPlaces then return 0
                            end if
                        case asc(".")
                            if pTextBox->nDecimalPlaces = 0 then return 0
                            if instr( wszText, "." ) > 0 then return 0
                            if (dwStart = 0) andalso (left(wszText, 1) = "-") then return 0
                            ' Everything right of the caret becomes the fractional part.
                            if (len(wszText) - dwStart) > pTextBox->nDecimalPlaces then return 0
                        case asc("-")
                            if dwStart <> 0 then return 0
                            if instr( wszText, "-" ) > 0 then return 0
                        case else
                            return 0
                    end select
                end if
            end if

        case WM_PASTE
            ' Numeric mode: vet the paste by building the prospective text. Rejected
            ' wholesale if the result would be invalid -- nothing is half-pasted.
            if (pTextBox <> null) andalso (pTextBox->bNumericOnly = true) then
                dim as DWSTRING wszClip = AfxGetClipboardText()
                ' A single-line control keeps only the first pasted line; mirror that
                ' before validating.
                dim as integer nEol = instr( wszClip, wchr(13) )
                if nEol = 0 then nEol = instr( wszClip, wchr(10) )
                if nEol > 0 then wszClip = left( wszClip, nEol - 1 )
                dim as DWORD dwStart, dwEnd
                SendMessage( hWin, EM_GETSEL, cast(WPARAM, @dwStart), cast(LPARAM, @dwEnd) )
                dim as DWSTRING wszText = RichEdit_GetText( hWin )
                dim as DWSTRING wszProspective = left(wszText, dwStart) & wszClip & mid(wszText, dwEnd + 1)
                if CTextBox_IsValidNumericText( pTextBox, wszProspective ) = false then return 0
            end if

        case WM_KEYDOWN
            ' Multiline: ENTER and TAB are ordinary editing keys -- fall through to the
            ' RichEdit (the newline/tab char itself arrives via WM_CHAR above). The
            ' EnterPressedCallback is a single-line contract and never fires here.
            dim as boolean bSingleLine = true
            if pTextBox then bSingleLine = (pTextBox->bMultiline = false)
            if (_wParam = VK_RETURN) andalso (bSingleLine = true) then
                if (pTextBox <> null) andalso (pTextBox->EnterPressedCallback <> null) then
                    pTextBox->EnterPressedCallback( hCtrl )
                end if
                return 0
            end if
            if (_wParam = VK_TAB) andalso (bSingleLine = true) then
                ' Tab leaves the control: focus the next (Shift+Tab: previous) tab stop
                ' of the top-level window. Handled here so tabbing works even in hosts
                ' whose message loop has no IsDialogMessage (see CTextBox.bi). Reached
                ' only when the MessageCallback declined the key -- a host can veto
                ' VK_TAB to repurpose it (e.g. list navigation in a picker).
                dim as HWND hRoot = GetAncestor( hWin, GA_ROOT )
                if hRoot then
                    dim as boolean bPrev = ( GetKeyState(VK_SHIFT) < 0 )
                    dim as HWND hNext = GetNextDlgTabItem( hRoot, hWin, bPrev )
                    if (hNext <> null) andalso (hNext <> hWin) then SetFocus( hNext )
                end if
                return 0
            end if

        case WM_MOUSEWHEEL
            ' Multiline: the control scrolls itself. The RichEdit's own WM_MOUSEWHEEL
            ' handling proved unreliable (ignored even when shown, focused and styled
            ' WS_VSCROLL), so the subclass converts the wheel delta to EM_LINESCROLL.
            ' A user scroll notifies the ScrollChangedCallback -- the same
            ' user-interaction-notifies split the other callbacks follow (the
            ' programmatic ScrollToLine stays silent).
            if (pTextBox <> null) andalso (pTextBox->bMultiline = true) then
                ' The delta is a SIGNED word in the high word (see C:\dev\Learnings.md);
                ' positive = wheel away = scroll up. Accumulate so high-precision
                ' wheels (deltas below one 120 notch) still add up to full notches.
                pTextBox->nWheelAccum += cast(short, hiword(_wParam))
                dim as long nNotches = pTextBox->nWheelAccum \ 120
                if nNotches <> 0 then
                    pTextBox->nWheelAccum -= nNotches * 120
                    dim as ulong scrollLines = 3
                    SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, @scrollLines, 0 )
                    if scrollLines = cast(ulong, &hFFFFFFFF) then
                        ' WHEEL_PAGESCROLL: a page per notch.
                        dim as long sbCmd = iif( nNotches > 0, SB_PAGEUP, SB_PAGEDOWN )
                        for i as long = 1 to abs(nNotches)
                            SendMessage( hWin, EM_SCROLL, sbCmd, 0 )
                        next
                    else
                        SendMessage( hWin, EM_LINESCROLL, 0, _
                                     cast(LPARAM, -(nNotches * cast(long, scrollLines)) ) )
                    end if
                    if pTextBox->ScrollChangedCallback then
                        pTextBox->ScrollChangedCallback( hCtrl )
                    end if
                end if
                return 0
            end if

        case WM_SETFOCUS, WM_KILLFOCUS
            ' Numeric mode: on focus loss reformat the value to exactly DecimalPlaces
            ' ("5." -> "5.00"), silently. Runs before the FocusCallback so the host
            ' reads the formatted text. Empty stays empty -- the cue banner shows --
            ' unless ZeroWhenEmpty, which stamps the formatted zero instead.
            if (uMsg = WM_KILLFOCUS) andalso (pTextBox <> null) then
                if pTextBox->bNumericOnly then
                    dim as DWSTRING wszText = RichEdit_GetText( hWin )
                    if (len(wszText) > 0) orelse (pTextBox->bZeroWhenEmpty = true) then
                        dim as DWSTRING wszFormatted = _
                            CTextBox_FormatNumeric( val(wszText), pTextBox->nDecimalPlaces, false )
                        if wszFormatted <> wszText then
                            pTextBox->bInternalChange = true
                            SendMessage( hWin, WM_SETTEXT, 0, cast(LPARAM, wszFormatted.vptr) )
                            pTextBox->bInternalChange = false
                        end if
                    end if
                end if
            end if
            if (pTextBox <> null) andalso (pTextBox->FocusCallback <> null) then
                pTextBox->FocusCallback( hCtrl, (uMsg = WM_SETFOCUS) )
            end if
            ' The border color depends on focus; repaint the container's chrome.
            InvalidateRect( hCtrl, null, true )
            if (uMsg = WM_SETFOCUS) andalso (pTextBox <> null) then
                if pTextBox->bSelectOnFocus then
                    ' Let the RichEdit process the focus change first, then select:
                    ' selection painting depends on the focus state being current. On a
                    ' mouse click the click's own caret placement runs after this and
                    ' wins -- select-on-focus is a keyboard/programmatic-focus behavior.
                    dim as LRESULT focusResult = DefSubclassProc( hWin, uMsg, _wParam, _lParam )
                    SendMessage( hWin, EM_SETSEL, 0, -1 )
                    return focusResult
                end if
            end if
            ' fall through to DefSubclassProc: the RichEdit manages the caret here

        case WM_CONTEXTMENU
            if pTextBox = null then exit select
            CTextBox_ShowContextMenu( pTextBox )
            return 0

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @CTextBoxRichEdit_SubclassProc, uIdSubclass )

    end select

    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' CTextBox container window procedure. Owns the chrome and the child geometry, relays
' EN_CHANGE, and forwards the classic edit-control message range to the RichEdit child
' (the message door -- see CTextBox.bi).
' ========================================================================================
function CTextBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CTEXTBOX ptr pTextBox = GetTextBoxPointer(hwnd)

    select case uMsg

        case WM_SIZE
            if pTextBox then CTextBox_PositionRichEdit( pTextBox )
            return 0

        case WM_PAINT
            if pTextBox = null then exit select    ' DefWindowProc validates
            dim b as CBufferPaint
            b.BeginDoubleBuffer(hwnd)
            ' Outer fill first: with rounded corners the pixels outside the arcs show
            ' this color (pass the host's background color to blend in).
            b.SetBackColor( pTextBox->OuterBackColor )
            b.PaintClientRect()
            dim as RECT rc = b.rcClient()
            dim as boolean bFocus = ( GetFocus() = pTextBox->hRichEdit )
            b.SetBackColor( pTextBox->BackColor )
            b.SetPenColor( iif( bFocus, pTextBox->FocusBorderColor, pTextBox->BorderColor ) )
            ' Curvature is the corner ellipse diameter = radius * 2.
            dim as long nCurve = pTextBox->nCornerRadius * 2
            if pTextBox->nBorderWidth > 0 then
                b.PaintRoundBorderRect( @rc, nCurve, pTextBox->nBorderWidth )
            else
                b.PaintRoundRect( @rc, nCurve )
            end if
            b.EndDoubleBuffer()
            return 0

        case WM_ERASEBKGND
            return true

        case WM_SETFOCUS
            ' Focus on the container means focus on the editor.
            if pTextBox then SetFocus( pTextBox->hRichEdit )
            return 0

        case WM_COMMAND
            if pTextBox then
                if cast(HWND, lParam) = pTextBox->hRichEdit then
                    select case hiword(wParam)
                        case EN_CHANGE
                            ' Programmatic setters are silent: bInternalChange gates every
                            ' change the control itself wrote to the buffer.
                            if (pTextBox->bInternalChange = false) andalso (pTextBox->ChangeCallback <> null) then
                                pTextBox->ChangeCallback( hwnd )
                            end if
                            return 0
                        case EN_UPDATE
                            ' Multiline only (ENM_UPDATE is set only then). Deliberately
                            ' NOT gated by bInternalChange: a programmatic SetText changes
                            ' the line count and a scrollbar must hear about it.
                            if pTextBox->ScrollChangedCallback then
                                pTextBox->ScrollChangedCallback( hwnd )
                            end if
                            return 0
                    end select
                end if
            end if

        ' ------------------------------------------------------------------------------
        ' Message door: sending classic edit-control messages to the CONTROL works as if
        ' it were the edit control itself. Specials first (they add control-side state),
        ' then the blanket EM_ range, then the WM_ text/clipboard messages.
        ' ------------------------------------------------------------------------------
        case WM_SETTEXT
            if pTextBox then
                pTextBox->bInternalChange = true
                dim as LRESULT res = SendMessage( pTextBox->hRichEdit, uMsg, wParam, lParam )
                pTextBox->bInternalChange = false
                return res
            end if

        case EM_REPLACESEL
            if pTextBox then
                pTextBox->bInternalChange = true
                dim as LRESULT res = SendMessage( pTextBox->hRichEdit, uMsg, wParam, lParam )
                pTextBox->bInternalChange = false
                return res
            end if

        case EM_SETMARGINS
            ' Intercepted so the stored values survive geometry/font changes, which
            ' re-apply the margins after every EM_SETRECT.
            if pTextBox then
                if (wParam and EC_LEFTMARGIN) then pTextBox->nMarginLeft = loword(lParam)
                if (wParam and EC_RIGHTMARGIN) then pTextBox->nMarginRight = hiword(lParam)
                CTextBox_ApplyMargins( pTextBox )
                InvalidateRect( pTextBox->hRichEdit, null, true )
                return 0
            end if

        case WM_SETFONT
            ' The door version of CTextBox_SetFont (lParam redraw flag is implicit:
            ' the child is always invalidated).
            if pTextBox then
                CTextBox_SetFont( hwnd, cast(HFONT, wParam) )
                return 0
            end if

        case WM_GETFONT
            if pTextBox then return cast( LRESULT, pTextBox->hTextFont )

        case EM_GETSEL to EM_GETIMESTATUS
            if pTextBox then return SendMessage( pTextBox->hRichEdit, uMsg, wParam, lParam )

        case WM_GETTEXT, WM_GETTEXTLENGTH, WM_CUT, WM_COPY, WM_PASTE, WM_CLEAR, WM_UNDO
            if pTextBox then return SendMessage( pTextBox->hRichEdit, uMsg, wParam, lParam )

        case WM_NCDESTROY
            if pTextBox then
                ' The context menu is a WS_POPUP window, not a child, so it is not
                ' destroyed with this control's window tree -- do it explicitly. If its
                ' chain is open, clear the shared target first so a late filter call
                ' cannot reach a dead window.
                if pTextBox->hContextMenu then
                    if gTB_ContextMenuTarget = pTextBox->hRichEdit then gTB_ContextMenuTarget = 0
                    DestroyWindow( pTextBox->hContextMenu )
                    pTextBox->hContextMenu = 0
                end if
            end if
            ' Delete the stored textbox data pointer
            if pTextBox then delete(pTextBox)
            ' delete the allocated memory for window
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Creates the CTextBox custom control. The returned HWND is the control: position it
' with SetWindowPos, find it with GetDlgItem(CtrlID).
' ========================================================================================
function CTextBox_Create( byval hWndParent as HWND, byval CtrlID as integer, byval bMultiline as boolean = false ) as HWND

    dim pWindow as CWindow ptr = new CWindow

    ' WS_EX_CONTROLPARENT: tab navigation (IsDialogMessage / GetNextDlgTabItem) must
    ' see through this container to the WS_TABSTOP RichEdit child inside it.
    dim as HWND hForm = _
            pWindow->Create( hWndParent, "", @CTextBox_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_CONTROLPARENT _
            )
    ' CtrlID becomes the control window's id (GWLP_ID) so hosts can locate the
    ' control with GetDlgItem, same contract as CListBox/CVScrollBar/CStatusBar/CTabBar.
    SetWindowLongPtr( hForm, GWLP_ID, CtrlID )

    ' Define the TYPE that holds all the information about this custom textbox.
    dim as CTEXTBOX ptr pTextBox = new CTEXTBOX
    pTextBox->hWin = hForm
    pTextBox->idc_TextBox = CtrlID
    pTextBox->bMultiline = bMultiline
    pTextBox->wszMenuCut = "Cut"
    pTextBox->wszMenuCopy = "Copy"
    pTextBox->wszMenuPaste = "Paste"
    pTextBox->wszMenuSelectAll = "Select All"

    ' The right-click menu, created up front rather than on first use so a host can style
    ' it (CTextBox_GetContextMenu) at any time after Create. Its owner is the top-level
    ' window: CPopupMenu uses that for z-order and for its foreground watch, so passing
    ' this child container instead would break dismissal on alt-tab.
    pTextBox->hContextMenu = CPopupMenu_Create( GetAncestor( hForm, GA_ROOT ) )
    CPopupMenu_SetSelectCallback( pTextBox->hContextMenu, @CTextBox_ContextMenuSelect )

    ' Store the new pointer into the user data area of the container window
    pWindow->UserData(0) = cast(LONG_PTR, pTextBox)

    ' Single line: no ES_MULTILINE; ES_AUTOHSCROLL scrolls the text automatically when
    ' it grows past the client width. Multiline: word wrap (so NO ES_AUTOHSCROLL) with
    ' vertical auto-scrolling; ES_WANTRETURN only matters under IsDialogMessage but is
    ' harmless -- the newline itself is guaranteed by the subclass letting VK_RETURN
    ' through. No scrollbar styles: an external scrollbar is driven via the scroll API,
    ' and the subclass implements wheel scrolling itself (the RichEdit's own
    ' WM_MOUSEWHEEL handling proved unreliable -- it ignored the message even when
    ' shown, focused and carrying WS_VSCROLL; proven by the harness assert).
    ' The child is shown by the first WM_SIZE.
    dim as long nEditStyle = WS_CHILD or WS_TABSTOP or ES_LEFT
    if bMultiline then
        nEditStyle or= (ES_MULTILINE or ES_AUTOVSCROLL or ES_WANTRETURN)
    else
        nEditStyle or= ES_AUTOHSCROLL
    end if
    pTextBox->hRichEdit = _
        pWindow->AddControl( "RICHEDIT", , CtrlID, "", 0, 0, 0, 0, _
        nEditStyle, _
        WS_EX_LEFT or WS_EX_LTRREADING )
    pWindow->SetControlSubclass( pTextBox->hRichEdit, @CTextBoxRichEdit_SubclassProc, CtrlID, NULL )

    ' Plain-text mode: one character format for the whole buffer (this control never
    ' colors individual characters) and pasted text sheds any rich formatting. Must be
    ' set while the control is still empty.
    SendMessage( pTextBox->hRichEdit, EM_SETTEXTMODE, TM_PLAINTEXT or TM_MULTILEVELUNDO or TM_MULTICODEPAGE, 0 )
    ' EN_CHANGE delivery (arrives as WM_COMMAND at the container); multiline adds
    ' EN_UPDATE, which feeds the ScrollChangedCallback.
    dim as long nEventMask = ENM_CHANGE
    if bMultiline then nEventMask or= ENM_UPDATE
    SendMessage( pTextBox->hRichEdit, EM_SETEVENTMASK, 0, nEventMask )
    SendMessage( pTextBox->hRichEdit, EM_SETBKGNDCOLOR, 0, cast(LPARAM, pTextBox->BackColor) )
    CTextBox_ApplyCharFormat( pTextBox )

    return hForm
end function
