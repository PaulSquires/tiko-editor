'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
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

'  frmKeyboardEdit.inc  --  assign one keyboard shortcut.
'
'  TWO VIEWS, ONE MODEL. The shortcut being built lives in gEditKeys as the same
'  "Ctrl+Shift+D" string the rest of tiko uses. A LIVE CAPTURE FIELD records a real
'  keystroke; three PsToggles and a scrollable key list describe the same thing by hand. Both views
'  WRITE the model and then RE-READ it -- neither writes the other -- so there is one place
'  the truth lives and no direction in which the two can drift.
'
'  The picker is not a fallback in the apologetic sense: it reaches combinations that are
'  awkward or impossible to press (Escape is reserved as the capture field's exit), and it is
'  the ONLY route to a bare letter or digit, which the capture field refuses on purpose. That
'  is precisely why it has to show ALL 90 entries -- see FRMKBEDIT_KEYLIST_H in the .bi for
'  why it is a scrolling list rather than the dropdown it started as.
'
'  NO PUMP OBLIGATION. This dialog owns no popup and no wrapped child that needs one: the key
'  picker is a PsListTree with in-place editing off, which adds no *_FilterMessage call.

#include once "frmKeyboardEdit.bi"


' ----------------------------------------------------------------------------------------
' THE MODEL, and the re-entrancy guard that keeps the two views from ping-ponging.
'
' PsToggle and PsListTree both document their programmatic setters as silent, so in
' principle refreshing a view cannot re-enter. The guard is kept anyway: it costs a byte,
' and the failure it prevents is an infinite loop rather than a cosmetic glitch.
' ----------------------------------------------------------------------------------------
dim shared gEditKeys     as DWSTRING     ' the user-defined shortcut being built, "" = none
dim shared gEditDisabled as boolean      ' suppress the shipped default?
dim shared gEditIndex    as long = -1    ' gKeysWork() row being edited, for conflict tests
dim shared gEditSyncing  as boolean      ' TRUE while a view is being refreshed from the model
dim shared gEditWhy      as DWSTRING     ' why the last capture was refused ("" = it was not)
dim shared gEditCaptureFocus as boolean  ' does the capture field hold the focus?

dim shared as HWND ghEditTogCtrl, ghEditTogAlt, ghEditTogShift, ghEditTogDisable
dim shared as HWND ghEditKeyList, ghEditOK, ghEditCancel, ghEditReset

' ----------------------------------------------------------------------------------------
' THE CAPTURE FIELD'S TWO ANIMATIONS.
'
' A BLINKING CARET while the field is waiting, and a BRIEF RED FLASH of its frame when a
' keystroke is refused. Both exist because the field is the one control here with no
' affordance of its own: a rounded rectangle containing a sentence looks exactly like a
' label, and a user can sit staring at it without realising it is listening.
'
' A SPINNER WAS DELIBERATELY NOT USED, even though tiko already ships one (the Braille
' spinner() array the status bar runs during a compile). A spinner means "the application is
' busy, wait for it" -- the precise opposite of this field, which is idle and waiting on the
' user. Borrowing it would have been cheap and would have said the wrong thing.
' ----------------------------------------------------------------------------------------
#Define IDT_KBEDIT_CARET   1
#Define IDT_KBEDIT_FLASH   2
#Define KBEDIT_FLASH_MS  200

dim shared gEditCaretOn as boolean   ' current blink phase; meaningless unless waiting
dim shared gEditFlashOn as boolean   ' the frame is showing its refusal colour right now


' ========================================================================================
' Measure a string in DEVICE pixels, with the font it will actually be painted in.
'
' DELIBERATELY NOT tiko's getTextWidth, which looks like exactly the right helper and is not.
' That one ends with pWindow->UnScaleX(size.cx) -- documented, and correct for its callers,
' which hand the result to control-creation helpers that scale it up again. A PAINT path
' wants the number as measured. Using it here returned 1/DPI of the truth and clipped the
' prompt to "Press the nev" at 175%; at 100% it would have looked perfect and shipped broken
' on every scaled display.
' ========================================================================================
private function frmKeyboardEdit_TextWidthPx( byval hwnd as HWND, _
                                              byval wszText as DWSTRING, _
                                              byval hFont as HFONT ) as long
    dim as SIZEL sz
    dim as HDC hDC = GetDC( hwnd )
    if hDC = 0 then return 0

    dim as HFONT oldFont
    if hFont then oldFont = SelectObject( hDC, hFont )
    GetTextExtentPoint32( hDC, wszText.sptr, len(wszText), @sz )
    if hFont then SelectObject( hDC, oldFont )

    ReleaseDC( hwnd, hDC )
    return sz.cx
end function


' ========================================================================================
' Start or stop the caret blink, from one place, based on whether there is anything to
' blink FOR: the field must hold the focus AND still be empty.
'
' THE TIMER MUST NOT RUN OTHERWISE. A blink timer left armed on an unfocused field, or on
' one that already holds a shortcut, wakes the process several times a second to repaint
' something nobody is looking at. PsProgressBar makes the same point about its marquee --
' the timer exists only for the mode that actually animates.
'
' Called from WM_SETFOCUS, WM_KILLFOCUS and the end of RefreshViews, so every route that can
' change either input passes through here.
' ========================================================================================
private sub frmKeyboardEdit_SyncCaretTimer()

    if HWND_FRMKEYBOARDEDIT_CAPTURE = 0 then exit sub

    dim as boolean bEmpty  = (len(gEditKeys) = 0)
    dim as boolean bWaiting = (gEditCaptureFocus andalso bEmpty)

    if bWaiting then
        ' The SYSTEM caret rate, so this matches every other text field on the machine.
        ' GetCaretBlinkTime returns -1 when the user has turned blinking off for
        ' accessibility; honour that by simply not animating rather than by picking a rate.
        dim as long nRate = cast(long, GetCaretBlinkTime())
        if nRate <= 0 then
            KillTimer( HWND_FRMKEYBOARDEDIT_CAPTURE, IDT_KBEDIT_CARET )
            gEditCaretOn = true       ' steady, not absent -- still says "input goes here"
        else
            SetTimer( HWND_FRMKEYBOARDEDIT_CAPTURE, IDT_KBEDIT_CARET, nRate, 0 )
        end if
    else
        KillTimer( HWND_FRMKEYBOARDEDIT_CAPTURE, IDT_KBEDIT_CARET )
        gEditCaretOn = false
    end if

    AfxRedrawWindow( HWND_FRMKEYBOARDEDIT_CAPTURE )

end sub


' ========================================================================================
' Compose the canonical "Ctrl+Shift+D" form.
'
' MODIFIER ORDER IS FIXED at Ctrl, Alt, Shift and must stay that way: the string is compared
' verbatim when looking for conflicts, so "Ctrl+Shift+D" and "Shift+Ctrl+D" would read as two
' different shortcuts while binding the identical keystroke.
'
' The key's spelling comes from KeyBindings_VKToName, never from the raw virtual key, so what
' is built here is by construction something frmKeyboard_AccelKeyToValue can turn back into
' the same key when the accelerator table is rebuilt.
' ========================================================================================
function frmKeyboardEdit_BuildKeyString( byval vk as long, _
                                         byval bCtrl as boolean, _
                                         byval bAlt as boolean, _
                                         byval bShift as boolean ) as DWSTRING

    dim as DWSTRING wszName = KeyBindings_VKToName( vk )
    if len(wszName) = 0 then return ""

    dim as DWSTRING wszKeys
    if bCtrl  then wszKeys = wszKeys & "Ctrl+"
    if bAlt   then wszKeys = wszKeys & "Alt+"
    if bShift then wszKeys = wszKeys & "Shift+"

    return wszKeys & wszName
end function


' ========================================================================================
' MAY THIS KEYSTROKE BECOME A SHORTCUT?
'
' Pure. See the declaration in frmKeyboardEdit.bi for why.
' ========================================================================================
function frmKeyboardEdit_AcceptCapture( byval vk as long, _
                                        byval bCtrl as boolean, _
                                        byval bAlt as boolean, _
                                        byval bShift as boolean, _
                                        byref wszWhy as DWSTRING ) as boolean

    wszWhy = ""

    ' RULE 1: the key must have a name in the persistence vocabulary.
    '
    ' PrintScreen, F13, the Windows key, media and IME keys are all real keys a user can
    ' press and none can be written into keybindings.ini. Storing one would produce a
    ' binding that saves and displays perfectly and never fires.
    dim as DWSTRING wszName = KeyBindings_VKToName( vk )
    if len(wszName) = 0 then
        wszWhy = L(346,"That key cannot be used as a shortcut.")
        return false
    end if

    ' RULE 2: a bare letter or digit needs a modifier.
    '
    ' Binding plain "D" makes typing the letter D in the editor fire a command. The DROPDOWN
    ' still allows it -- this rule belongs to the capture field, where the user is pressing
    ' keys rather than deliberately choosing one, and not to the model.
    dim as boolean bAnyMod = (bCtrl orelse bAlt orelse bShift)
    if bAnyMod = false then
        dim as boolean bLetter = (vk >= asc("A")) andalso (vk <= asc("Z"))
        dim as boolean bDigit  = (vk >= asc("0")) andalso (vk <= asc("9"))
        if bLetter orelse bDigit then
            wszWhy = L(347,"A letter or digit needs Ctrl, Alt or Shift.")
            return false
        end if
    end if

    ' RULE 3: everything else is accepted, INCLUDING a bare function or navigation key.
    ' F5, F7, Tab, Del and PgUp are all shipped bare defaults in this very editor; refusing
    ' them would make the field feel broken the first time someone tried to rebind Tab.
    return true
end function


' ========================================================================================
' Model -> views. Every setter here is programmatic and therefore silent, but the guard is
' raised anyway so a future control that DOES notify cannot start a loop.
' ========================================================================================
private sub frmKeyboardEdit_RefreshViews()

    if gEditSyncing then exit sub
    gEditSyncing = true

    ' Split the model back into its parts.
    dim as boolean bCtrl, bAlt, bShift
    dim as DWSTRING wszKey

    dim as long nCount = AfxStrParseCount( gEditKeys, "+" )
    if len(gEditKeys) then
        for i as long = 1 to nCount
            dim as DWSTRING wszPart = AfxStrParse( gEditKeys, i, "+" )
            if i = nCount then
                wszKey = wszPart
            else
                select case ucase(wszPart)
                    case "CTRL":  bCtrl  = true
                    case "ALT":   bAlt   = true
                    case "SHIFT": bShift = true
                end select
            end if
        next
    end if

    PsToggle_SetChecked( ghEditTogCtrl,  bCtrl )
    PsToggle_SetChecked( ghEditTogAlt,   bAlt )
    PsToggle_SetChecked( ghEditTogShift, bShift )
    PsToggle_SetChecked( ghEditTogDisable, gEditDisabled )

    ' Row 0 of the picker is "None"; the vocabulary follows in gKeyNames() order.
    dim as long nSel = 0
    if len(wszKey) then
        KeyBindings_EnsureKeyNames()
        for i as long = lbound(gKeyNames) to ubound(gKeyNames)
            if ucase(gKeyNames(i).wszName) = ucase(wszKey) then
                nSel = i + 1
                exit for
            end if
        next
    end if
    PsListTree_SetCurSel( ghEditKeyList, nSel )

    gEditSyncing = false

    ' The model just changed, so whether there is anything to blink for may have changed
    ' with it -- capturing a shortcut stops the caret, Reset starts it again.
    frmKeyboardEdit_SyncCaretTimer()

    if HWND_FRMKEYBOARDEDIT_CAPTURE then AfxRedrawWindow( HWND_FRMKEYBOARDEDIT_CAPTURE )
    if HWND_FRMKEYBOARDEDIT then AfxRedrawWindow( HWND_FRMKEYBOARDEDIT )

end sub


' ========================================================================================
' Views -> model. Reads the toggles and the dropdown, writes gEditKeys, refreshes.
' ========================================================================================
private sub frmKeyboardEdit_ModelFromViews()

    if gEditSyncing then exit sub

    dim as long nSel = PsListTree_GetCurSel( ghEditKeyList )
    if nSel <= 0 then
        ' "None" -- clears the whole binding, modifiers included. A shortcut with modifiers
        ' and no key is not a shortcut.
        gEditKeys = ""
    else
        KeyBindings_EnsureKeyNames()
        dim as long idx = nSel - 1
        if (idx >= lbound(gKeyNames)) andalso (idx <= ubound(gKeyNames)) then
            dim as DWSTRING wszKeys
            if PsToggle_GetChecked( ghEditTogCtrl )  then wszKeys = wszKeys & "Ctrl+"
            if PsToggle_GetChecked( ghEditTogAlt )   then wszKeys = wszKeys & "Alt+"
            if PsToggle_GetChecked( ghEditTogShift ) then wszKeys = wszKeys & "Shift+"
            gEditKeys = wszKeys & gKeyNames(idx).wszName
        end if
    end if

    gEditDisabled = PsToggle_GetChecked( ghEditTogDisable )
    gEditWhy = ""
    frmKeyboardEdit_RefreshViews()

end sub


' ========================================================================================
' THE CAPTURE FIELD
'
' A bespoke owner-drawn child, local to this dialog -- not a reusable sibling and not a
' PsTextBox (that wraps a RichEdit, which would fight for every keystroke). It exists to do
' one thing no other control in the family does: swallow a raw key combination.
'
' THE KEY HANDLING IS THE RISKY PART, and three things about it are load-bearing:
'
'   1. WM_GETDLGCODE claims DLGC_WANTALLKEYS PER MESSAGE and only for keys this field
'      actually wants. Claiming unconditionally would swallow Tab and strand the user.
'
'   2. Alt+key arrives as WM_SYSKEYDOWN, NOT WM_KEYDOWN. Handling only the latter would make
'      every Alt combination -- including Alt+F4, a shipped default -- uncapturable.
'
'   3. A CLAIMED KEY'S WM_CHAR MUST BE SWALLOWED or the system beeps on every press. The
'      keystroke still works, so the beep is the only symptom and no assertion can hear it;
'      this shipped in five controls of this family at once and was found by a user pressing
'      Enter. The swallow is driven by a flag armed in WM_KEYDOWN, never by re-reading state,
'      because a key that changes state has already changed it before its character arrives.
'
' ESCAPE IS NOT CLAIMED. It is the field's exit -- a capture field with no keyboard way out
' is a trap, and this dialog has to stay usable without a mouse.
' ========================================================================================
private function frmKeyboardEdit_CaptureProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    ' Armed in WM_KEYDOWN/WM_SYSKEYDOWN, consumed by the WM_CHAR immediately following.
    static as boolean bSwallowChar

    select case uMsg

        case WM_GETDLGCODE
            ' Per-message, and never for Escape or Tab-with-no-message.
            dim as MSG ptr pMsg = cast(MSG ptr, lParam)
            if pMsg then
                if (pMsg->message = WM_KEYDOWN) orelse (pMsg->message = WM_SYSKEYDOWN) orelse _
                   (pMsg->message = WM_CHAR)    orelse (pMsg->message = WM_SYSCHAR) then
                    if pMsg->wParam <> VK_ESCAPE then return DLGC_WANTALLKEYS
                end if
            end if
            return DLGC_WANTARROWS

        case WM_SETFOCUS
            gEditCaptureFocus = true
            gEditCaretOn      = true      ' show immediately; do not wait out a blink period
            frmKeyboardEdit_SyncCaretTimer()
            return 0

        case WM_KILLFOCUS
            gEditCaptureFocus = false
            frmKeyboardEdit_SyncCaretTimer()
            return 0

        case WM_TIMER
            select case wParam
                case IDT_KBEDIT_CARET
                    gEditCaretOn = not gEditCaretOn
                    AfxRedrawWindow( hwnd )
                case IDT_KBEDIT_FLASH
                    ' One-shot: kill it here rather than arming a repeating timer and
                    ' ignoring most of its ticks.
                    KillTimer( hwnd, IDT_KBEDIT_FLASH )
                    gEditFlashOn = false
                    AfxRedrawWindow( hwnd )
            end select
            return 0

        case WM_DESTROY
            KillTimer( hwnd, IDT_KBEDIT_CARET )
            KillTimer( hwnd, IDT_KBEDIT_FLASH )

        case WM_LBUTTONDOWN
            SetFocus( hwnd )
            return 0

        case WM_KEYDOWN, WM_SYSKEYDOWN
            dim as long vk = cast(long, wParam)

            ' Escape leaves the field; it is deliberately not capturable here.
            if vk = VK_ESCAPE then
                bSwallowChar = false
                return DefWindowProc( hwnd, uMsg, wParam, lParam )
            end if

            ' A modifier on its own is not a complete shortcut. Repaint so the live preview
            ' follows the user's fingers, but do not commit anything.
            select case vk
                case VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN, _
                     VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT
                    bSwallowChar = true
                    AfxRedrawWindow( hwnd )
                    return 0
            end select

            dim as boolean bCtrl  = ((GetKeyState(VK_CONTROL) and &h8000) <> 0)
            dim as boolean bAlt   = ((GetKeyState(VK_MENU)    and &h8000) <> 0)
            dim as boolean bShift = ((GetKeyState(VK_SHIFT)   and &h8000) <> 0)

            dim as DWSTRING wszWhy
            if frmKeyboardEdit_AcceptCapture( vk, bCtrl, bAlt, bShift, wszWhy ) then
                gEditKeys = frmKeyboardEdit_BuildKeyString( vk, bCtrl, bAlt, bShift )
                gEditWhy  = ""
                frmKeyboardEdit_RefreshViews()
            else
                ' REFUSED. The message line below explains it in words, but words alone make
                ' "I heard you and said no" look identical to "I never saw that key" -- the
                ' field simply sits there either way. A brief flash of the frame is motion,
                ' and motion is what pulls the eye back to a control already being stared at.
                gEditWhy     = wszWhy
                gEditFlashOn = true
                SetTimer( hwnd, IDT_KBEDIT_FLASH, KBEDIT_FLASH_MS, 0 )
                AfxRedrawWindow( hwnd )
                AfxRedrawWindow( HWND_FRMKEYBOARDEDIT )
            end if

            bSwallowChar = true
            return 0

        case WM_CHAR
            ' See point 3 in the header. Silence is the entire purpose of this arm.
            if bSwallowChar then
                bSwallowChar = false
                return 0
            end if

        case WM_SYSCHAR
            ' LEFT ALONE on purpose: swallowing it breaks Alt+F4 and the host's mnemonics.

        case WM_PAINT
            dim as PAINTSTRUCT ps
            dim as HDC hdc = BeginPaint( hwnd, @ps )
            dim as RECT rc
            GetClientRect( hwnd, @rc )

            dim buffer as PsBufferPaint
            buffer.BeginDoubleBuffer( hwnd, hdc, rc )

            buffer.SetBackColor( ghEditor.BackColorText )
            buffer.PaintRect( @rc )

            ' The frame. PaintRoundOutline STROKES; a fill-then-stroke primitive here would
            ' flood the field and erase the text -- the family's repeat offender.
            '
            ' Three states, and the refusal flash outranks focus: for its 200ms the frame is
            ' carrying news, and the focus it would otherwise report has not changed anyway.
            dim as long curve = AfxScaleY(6)
            dim as COLORREF penclr = ghEditor.Divider
            dim as long     penw   = 1
            if gEditFlashOn then
                penclr = ghGeneral.iconfail : penw = 2
            elseif gEditCaptureFocus then
                penclr = ghUI.AccentColor   : penw = 2
            end if
            buffer.SetPenColor( penclr )
            buffer.PaintRoundOutline( @rc, curve, penw )

            dim as DWSTRING wszShow = gEditKeys
            dim as COLORREF clr = ghEditor.ForeColorText
            dim as boolean bWaiting = (len(wszShow) = 0)
            if bWaiting then
                wszShow = L(343,"Press the new shortcut")
                clr = ghPanel.ForeColor
            end if

            buffer.SetForeColor( clr )
            buffer.SetBackColor( ghEditor.BackColorText )
            buffer.SetFont( ghFont(GUIFONTBOLD_11) )

            if bWaiting = false then
                ' A settled shortcut: plain centred text, nothing animating.
                buffer.PaintText( wszShow, @rc, DT_CENTER or DT_VCENTER or DT_SINGLELINE )
            else
                ' WAITING. The caret sits after the prompt, and the space it occupies is
                ' RESERVED WHETHER OR NOT IT IS CURRENTLY DRAWN -- centre the text plus the
                ' caret slot as one unit, then draw the text into the left part. Centring the
                ' text alone and appending the caret would shuffle the whole sentence a few
                ' pixels left and right twice a second, which reads as a rendering fault
                ' rather than as a caret.
                dim as long tw     = frmKeyboardEdit_TextWidthPx( hwnd, wszShow, ghFont(GUIFONTBOLD_11) )
                dim as long caretW = AfxScaleX(2)
                dim as long gapW   = AfxScaleX(6)
                dim as long x0     = (rc.right - (tw + gapW + caretW)) \ 2
                if x0 < 0 then x0 = 0

                dim as RECT rcText = rc
                rcText.left  = x0
                rcText.right = x0 + tw
                buffer.PaintText( wszShow, @rcText, DT_LEFT or DT_VCENTER or DT_SINGLELINE )

                if gEditCaretOn then
                    dim as long barH = (rc.bottom - rc.top) \ 2
                    dim as RECT rcCaret
                    rcCaret.left   = x0 + tw + gapW
                    rcCaret.right  = rcCaret.left + caretW
                    rcCaret.top    = (rc.bottom - barH) \ 2
                    rcCaret.bottom = rcCaret.top + barH
                    buffer.SetBackColor( ghUI.AccentColor )
                    buffer.PaintRect( @rcCaret )
                    buffer.SetBackColor( ghEditor.BackColorText )
                end if
            end if

            buffer.EndDoubleBuffer()
            EndPaint( hwnd, @ps )
            return 0

        case WM_ERASEBKGND
            return 1

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Control callbacks
' ========================================================================================

private sub frmKeyboardEdit_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    frmKeyboardEdit_ModelFromViews()
end sub

private sub frmKeyboardEdit_KeyListChanged( byval hList as HWND, byval row as integer )
    frmKeyboardEdit_ModelFromViews()
end sub


' No tooltips on the key picker either: with no callback PsListTree falls back to the row's
' own text, which here would pop "F5" over a row already reading "F5".
private function frmKeyboardEdit_KeyListTooltip( byval hList as HWND, byval row as integer ) as DWSTRING
    return ""
end function


' The key picker's row painter. Plain single-column rows in the dialog's own palette.
private sub frmKeyboardEdit_KeyListPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as boolean bActive = (p->isSelected orelse p->isHot)
    p->b->SetBackColor( iif( bActive, ghPanel.BackColorHot, ghEditor.BackColorText ) )
    p->b->PaintRect( @p->rc )
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(8)
    p->b->SetForeColor( iif( bActive, ghPanel.ForeColorHot, ghEditor.ForeColorText ) )
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
end sub

private sub frmKeyboardEdit_ButtonClick( byval hButton as HWND, byval id as long )
    select case id
        ' RESET restores this command to its shipped default: no user keys, and the default
        ' re-enabled. Exactly what the list's Reset does, so the two cannot mean different
        ' things by the same word.
        '
        ' IT DOES NOT CLOSE THE DIALOG, deliberately. Everything here is still only the edit
        ' dialog's own model until OK is pressed, so a reset followed by Cancel leaves the
        ' command untouched -- the user gets to see what Reset did before committing to it.
        case IDC_FRMKEYBOARDEDIT_CMDRESET
            gEditKeys     = ""
            gEditDisabled = false
            gEditWhy      = ""
            frmKeyboardEdit_RefreshViews()
            SetFocus( HWND_FRMKEYBOARDEDIT_CAPTURE )

        case IDC_FRMKEYBOARDEDIT_CMDOK
            ' Commit into the row the caller staged for us. The dialog owns nothing else.
            gKeysEdit.wszUserKeys      = gEditKeys
            gKeysEdit.bDefaultDisabled = gEditDisabled
            SendMessage( HWND_FRMKEYBOARDEDIT, WM_CLOSE, 0, 0 )
        case IDC_FRMKEYBOARDEDIT_CMDCANCEL
            SendMessage( HWND_FRMKEYBOARDEDIT, WM_CLOSE, 0, 0 )
    end select
end sub


' ========================================================================================
' Paint the dialog's own chrome: labels, the default line, and the conflict / refusal line.
' ========================================================================================
private function frmKeyboardEdit_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    dim as long mx = AfxScaleX(FRMKBEDIT_MARGIN)
    dim as long y  = AfxScaleY(FRMKBEDIT_MARGIN)
    dim as long rowH = AfxScaleY(FRMKBEDIT_ROW_H)

    dim as RECT rcT
    rcT.left = mx : rcT.right = rc.right - mx

    ' Description, bold.
    rcT.top = y : rcT.bottom = y + rowH
    buffer.SetForeColor( ghPanel.ForeColorHot )
    buffer.SetFont( ghFont(GUIFONTBOLD_11) )
    buffer.PaintText( gKeysEdit.wszDescription, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    y += rowH

    ' Category, dim.
    rcT.top = y : rcT.bottom = y + rowH
    buffer.SetForeColor( ghPanel.ForeColor )
    buffer.SetFont( ghFont(GUIFONT_11) )
    buffer.PaintText( gKeysEdit.wszCategory, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    y += rowH + AfxScaleY(10)

    ' "Default Keys: Ctrl+N", with the Disable toggle parked on the right of the same line.
    ' THE LABEL IS NOT DECORATION. Without it the line reads as a bare "Ctrl+N" directly
    ' above an empty shortcut field, and there is nothing to say which of the two is the
    ' default and which is what the user is about to set.
    dim as DWSTRING wszDef = gKeysEdit.wszDefaultKeys
    if len(wszDef) = 0 then wszDef = L(348,"(no default)")
    rcT.top = y : rcT.bottom = y + rowH
    rcT.right = rc.right - mx - AfxScaleX(210)
    buffer.SetForeColor( ghPanel.ForeColor )
    buffer.SetFont( ghFont(GUIFONT_11) )
    buffer.PaintText( L(337,"Default Keys") & ": " & wszDef, @rcT, DT_LEFT or DT_END_ELLIPSIS )

    ' The toggle's caption. PsToggle carries no caption of its own and therefore no font,
    ' so every host draws its label -- the same arrangement the Options dialog's rows use.
    ' The rect is sized to fit "Disable default" whole; at 150px it ellipsized to
    ' "Disable defa...", which is worse than useless on a control whose entire meaning is
    ' its caption.
    rcT.left  = rc.right - mx - AfxScaleX(200)
    rcT.right = rc.right - mx - AfxScaleX(58)
    buffer.PaintText( L(344,"Disable default"), @rcT, DT_RIGHT or DT_END_ELLIPSIS )
    rcT.left = mx : rcT.right = rc.right - mx
    y += rowH + AfxScaleY(14)

    ' NO LABEL ABOVE THE CAPTURE FIELD. It used to carry "Press the new shortcut" -- the same
    ' words the empty field itself shows, one line apart. The field's own placeholder says it
    ' where the user is looking, so the label was removed rather than reworded.
    y += AfxScaleY(FRMKBEDIT_CAPTURE_H) + AfxScaleY(18)

    ' The dropdown row's labels: three toggle captions and "Key:".
    rcT.top = y : rcT.bottom = y + rowH
    dim as long togW = AfxScaleX(46)
    dim as long capW = AfxScaleX(42)
    dim as long x    = mx
    buffer.SetForeColor( ghPanel.ForeColor )
    buffer.SetFont( ghFont(GUIFONT_11) )

    #macro TOGCAPTION( _text_ )
        rcT.left = x + togW : rcT.right = x + togW + capW
        buffer.PaintText( _text_, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE )
        x += togW + capW + AfxScaleX(8)
    #endmacro

    TOGCAPTION( "Ctrl" )
    TOGCAPTION( "Alt" )
    TOGCAPTION( "Shift" )

    ' NO "Key:" LABEL. The picker is the only list on the dialog and it sits directly under
    ' this row; naming it added a word without adding information. (Its localized string,
    ' "Key", was removed from english.lang in the id compaction -- the id space is dense now,
    ' so an unused phrase is not free: it holds a number nothing references.)

    ' THE MESSAGE LINE. One line serving two purposes, and the refusal wins: a rejected
    ' keystroke is news about what the user just did, a conflict is a standing property of
    ' what is already there.
    ' Keep the y arithmetic here in step with frmKeyboardEdit_PositionWindows.
    y += rowH + AfxScaleY(6) + AfxScaleY(FRMKBEDIT_KEYLIST_H) + AfxScaleY(12)
    rcT.left = mx : rcT.right = rc.right - mx
    rcT.top = y : rcT.bottom = y + rowH

    dim as DWSTRING wszMsg = gEditWhy
    if len(wszMsg) = 0 then wszMsg = frmKeyboard_CheckForKeyConflict( gEditKeys, gEditIndex )
    if len(wszMsg) then
        buffer.SetForeColor( ghGeneral.iconfail )
        buffer.SetFont( ghFont(GUIFONT_11) )
        buffer.PaintText( wszMsg, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    end if

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Lay the children out. Fixed-size dialog, so this runs once.
' ========================================================================================
private sub frmKeyboardEdit_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long mx   = AfxScaleX(FRMKBEDIT_MARGIN)
    dim as long rowH = AfxScaleY(FRMKBEDIT_ROW_H)
    dim as long y    = AfxScaleY(FRMKBEDIT_MARGIN) + (rowH * 2) + AfxScaleY(10)

    ' The Disable toggle, on the "Default:" line.
    dim as long togW = 0, togH = 0
    PsToggle_GetIdealSize( ghEditTogDisable, togW, togH )
    SetWindowPos( ghEditTogDisable, 0, rc.right - mx - AfxScaleX(48), y + (rowH - togH) \ 2, _
                  togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
    ' One row, not two: the label that used to sit above the capture field is gone (it
    ' repeated the field's own placeholder). Keep this in step with frmKeyboardEdit_OnPaint.
    y += rowH + AfxScaleY(14)

    ' The capture field.
    dim as long capH = AfxScaleY(FRMKBEDIT_CAPTURE_H)
    SetWindowPos( HWND_FRMKEYBOARDEDIT_CAPTURE, 0, mx, y, rc.right - (mx * 2), capH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    y += capH + AfxScaleY(18)

    ' The three modifier toggles and the key dropdown, on one line.
    dim as long capW = AfxScaleX(42)
    dim as long x    = mx
    dim as long tw = 0, th = 0

    #macro PLACETOGGLE( _h_ )
        PsToggle_GetIdealSize( _h_, tw, th )
        SetWindowPos( _h_, 0, x, y + (rowH - th) \ 2, tw, th, SWP_NOZORDER or SWP_NOACTIVATE )
        x += AfxScaleX(46) + capW + AfxScaleX(8)
    #endmacro

    PLACETOGGLE( ghEditTogCtrl )
    PLACETOGGLE( ghEditTogAlt )
    PLACETOGGLE( ghEditTogShift )

    ' The key picker gets its own full-width band under the toggles -- it is a scrolling
    ' list, not a dropdown, so it occupies space rather than borrowing the screen.
    y += rowH + AfxScaleY(6)
    SetWindowPos( ghEditKeyList, 0, mx, y, rc.right - (mx * 2), AfxScaleY(FRMKBEDIT_KEYLIST_H), _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    ' Footer.
    dim as long btnW = AfxScaleX(FRMKBEDIT_BTN_W)
    dim as long btnH = AfxScaleY(FRMKBEDIT_BTN_H)
    dim as long btnY = rc.bottom - AfxScaleY(FRMKBEDIT_MARGIN) - btnH
    SetWindowPos( ghEditReset, 0, mx, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghEditCancel, 0, rc.right - mx - btnW, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghEditOK, 0, rc.right - mx - (btnW * 2) - AfxScaleX(8), btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Theme every child that carries its own colours.
' ========================================================================================
private sub frmKeyboardEdit_ApplyTheme( byval hwnd as HWND )

    ' Same recipes as the Options dialog's OptionsTheme_Fill* helpers, spelled out because
    ' those live in modOptionsRows.inc, which the unity build includes after this file.
    dim as PSTOGGLE_COLORS tc
    tc.BackColor                   = ghPanel.BackColor
    tc.FocusRingColor              = ghPanel.ForeColorHot
    tc.TrackColorOn                = ghUI.AccentColor
    tc.TrackColorOnHot             = ghUI.AccentColor
    tc.TrackColorOnDisabled        = ghPanel.BackColorButton
    tc.TrackBorderColorOn          = tc.TrackColorOn
    tc.TrackBorderColorOnHot       = tc.TrackColorOnHot
    tc.TrackBorderColorOnDisabled  = tc.TrackColorOnDisabled
    tc.KnobColorOn                 = ghPanel.ForeColor
    tc.KnobColorOnHot              = ghPanel.ForeColorHot
    tc.KnobColorOnDisabled         = ghPanel.ForeColorScrollBar
    tc.TrackColorOff               = ghPanel.BackColor
    tc.TrackColorOffHot            = ghPanel.BackColorHot
    tc.TrackColorOffDisabled       = ghPanel.BackColor
    tc.TrackBorderColorOff         = ghEditor.Divider
    tc.TrackBorderColorOffHot      = ghPanel.ForeColorHot
    tc.TrackBorderColorOffDisabled = ghEditor.Divider
    tc.KnobColorOff                = ghPanel.ForeColorScrollBar
    tc.KnobColorOffHot             = ghPanel.ForeColor
    tc.KnobColorOffDisabled        = ghEditor.Divider

    PsToggle_SetColors( ghEditTogCtrl,    @tc )
    PsToggle_SetColors( ghEditTogAlt,     @tc )
    PsToggle_SetColors( ghEditTogShift,   @tc )
    PsToggle_SetColors( ghEditTogDisable, @tc )

    PsListTree_SetBackColor( ghEditKeyList, ghEditor.BackColorText )
    PsListTree_SetScrollBarColors( ghEditKeyList, ghPanel.BackColorScrollBar, _
                                   ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
    PsListTree_SetBorderColor( ghEditKeyList, ghEditor.Divider )
    PsListTree_SetBorderWidth( ghEditKeyList, 1 )

    dim as PSBUTTON_COLORS bc
    bc.BackColor           = ghPanel.BackColorButton
    bc.ForeColor           = ghPanel.ForeColorButton
    bc.BorderColor         = ghEditor.Divider
    bc.IconColor           = ghPanel.ForeColorButton
    bc.BackColorHot        = ghPanel.BackColorButtonHot
    bc.ForeColorHot        = ghPanel.ForeColorButtonHot
    bc.BorderColorHot      = ghPanel.BackColorButtonHot
    bc.IconColorHot        = ghPanel.ForeColorButtonHot
    bc.BackColorPressed    = ghPanel.BackColorButtonHot
    bc.ForeColorPressed    = ghPanel.ForeColorButtonHot
    bc.BorderColorPressed  = ghPanel.BackColorButtonHot
    bc.IconColorPressed    = ghPanel.ForeColorButtonHot
    bc.BackColorDisabled   = ghPanel.BackColor
    bc.ForeColorDisabled   = ghEditor.Divider
    bc.BorderColorDisabled = ghEditor.Divider
    bc.IconColorDisabled   = ghEditor.Divider
    PsButton_SetColors( ghEditReset, @bc )
    PsButton_SetColors( ghEditOK, @bc )
    PsButton_SetColors( ghEditCancel, @bc )
    PsButton_Refresh( ghEditReset )
    PsButton_Refresh( ghEditOK )
    PsButton_Refresh( ghEditCancel )

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' THE CAPTURE RULES SELF-TEST                               (TIKO_KEYBOARD_SELFTEST=1)
'
' Drives frmKeyboardEdit_AcceptCapture and frmKeyboardEdit_BuildKeyString directly. Both are
' pure, which is exactly why they were written that way: a self-test has no message pump, so
' anything that could only be reached by pressing a key would be untestable.
'
' WHAT THIS CANNOT REACH, and it is most of the risk: the WM_CHAR beep, whether Escape really
' leaves the field, whether Alt+F4 is captured rather than closing the dialog, and whether
' Tab still navigates. Those are pump behaviour and need a human. See the hand-off notes.
' ========================================================================================
sub frmKeyboardEdit_RunSelfTest()

    if environ("TIKO_KEYBOARD_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine
    dim as DWSTRING wszWhy

    #macro KEASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? ""
    ? "--- TIKO_KEYBOARD_SELFTEST (capture rules) ---"

    ' ---- RULE 1: a key with no name in the vocabulary is refused. ----
    KEASSERT( frmKeyboardEdit_AcceptCapture( &h2C, true, false, false, wszWhy ) = false, _
              "Ctrl+PrintScreen should be refused (no name in the vocabulary)" )
    KEASSERT( len(wszWhy) > 0, "a refused capture must say why" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( &h7C, true, false, false, wszWhy ) = false, _
              "Ctrl+F13 should be refused" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( &h5B, false, false, false, wszWhy ) = false, _
              "the Windows key should be refused" )

    ' ---- RULE 2: a BARE letter or digit is refused; with any modifier it is accepted. ----
    KEASSERT( frmKeyboardEdit_AcceptCapture( asc("D"), false, false, false, wszWhy ) = false, _
              "a bare D should be refused" )
    KEASSERT( len(wszWhy) > 0, "a refused bare letter must say why" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( asc("7"), false, false, false, wszWhy ) = false, _
              "a bare 7 should be refused" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( asc("D"), true,  false, false, wszWhy ), "Ctrl+D should be accepted" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( asc("D"), false, true,  false, wszWhy ), "Alt+D should be accepted" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( asc("D"), false, false, true,  wszWhy ), "Shift+D should be accepted" )
    KEASSERT( len(wszWhy) = 0, "an accepted capture must not leave a reason set" )

    ' ---- RULE 3: bare function and navigation keys ARE accepted. ----
    ' Every one of these is a shipped bare default in this editor, so refusing them would
    ' contradict the very table the dialog is editing.
    KEASSERT( frmKeyboardEdit_AcceptCapture( VK_F5,     false, false, false, wszWhy ), "bare F5 should be accepted" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( VK_F7,     false, false, false, wszWhy ), "bare F7 should be accepted" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( VK_TAB,    false, false, false, wszWhy ), "bare Tab should be accepted" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( VK_DELETE, false, false, false, wszWhy ), "bare Del should be accepted" )
    KEASSERT( frmKeyboardEdit_AcceptCapture( VK_PRIOR,  false, false, false, wszWhy ), "bare PgUp should be accepted" )

    ' ---- The composed string, and its ROUND TRIP back through the accelerator parser. ----
    ' This is the assertion that ties the capture field to something that actually fires: a
    ' string that cannot be re-parsed is a shortcut that saves, displays and never works.
    KEASSERT( frmKeyboardEdit_BuildKeyString( asc("D"), true, false, true ) = "Ctrl+Shift+D", _
              "Ctrl+Shift+D composed as """ & str(frmKeyboardEdit_BuildKeyString( asc("D"), true, false, true )) & """" )
    KEASSERT( frmKeyboardEdit_BuildKeyString( VK_F4, false, true, false ) = "Alt+F4", "Alt+F4 mis-composed" )
    KEASSERT( frmKeyboardEdit_BuildKeyString( VK_PRIOR, true, false, false ) = "Ctrl+PgUp", "Ctrl+PgUp mis-composed" )
    KEASSERT( len(frmKeyboardEdit_BuildKeyString( &h2C, true, false, false )) = 0, _
              "an unnamed key must compose to the empty string, not to a modifier-only stub" )

    ' MODIFIER ORDER IS PART OF THE CONTRACT: conflict detection compares these strings
    ' verbatim, so "Shift+Ctrl+D" would read as a different shortcut to "Ctrl+Shift+D" while
    ' binding the identical keystroke.
    KEASSERT( frmKeyboardEdit_BuildKeyString( asc("A"), true, true, true ) = "Ctrl+Alt+Shift+A", _
              "modifier order must be Ctrl, Alt, Shift" )

    ' Round trip: every composed string must parse back to the same virtual key.
    scope
        dim as long vks(4) = { asc("D"), VK_F4, VK_PRIOR, VK_TAB, VK_DELETE }
        for i as long = 0 to 4
            dim as DWSTRING wsz = frmKeyboardEdit_BuildKeyString( vks(i), true, false, false )
            dim as long n = AfxStrParseCount( wsz, "+" )
            dim as DWSTRING wszFinal = AfxStrParse( wsz, n, "+" )
            dim as string sW = wsz
            KEASSERT( frmKeyboard_AccelKeyToValue( wszFinal ) = vks(i), _
                      "composed """ & sW & """ does not parse back to the same key" )
        next
    end scope

    sLine = "--- TIKO_KEYBOARD_SELFTEST (capture): " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine
    ? ""

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmKeyboardEdit_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

        case WM_CREATE
            EnableWindow( GetParent(hwnd), false )
            return 0

        case WM_ERASEBKGND
            return 1

        case WM_PAINT
            return frmKeyboardEdit_OnPaint( hwnd )

        case WM_SIZE
            frmKeyboardEdit_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            select case loword(wParam)
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case WM_CLOSE
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMKEYBOARDEDIT         = 0
            HWND_FRMKEYBOARDEDIT_CAPTURE = 0
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmKeyboardEdit_Show
'
' Modal. Reads gKeysEdit on the way in and writes it back on OK; the caller stages that.
' ========================================================================================
function frmKeyboardEdit_Show( byval hWndParent as HWND ) as LRESULT

    ' Seed the model from the row the caller staged.
    gEditKeys     = gKeysEdit.wszUserKeys
    gEditDisabled = gKeysEdit.bDefaultDisabled
    gEditWhy      = ""
    gEditSyncing  = false
    gEditCaptureFocus = false

    ' Which staging row is this, so the conflict test can skip it? Found by message string
    ' rather than by index -- gKeysEdit is a COPY, so it carries no index of its own, and
    ' matching on the IDM_ name cannot go stale the way a remembered row could.
    gEditIndex = -1
    for i as long = lbound(gKeysWork) to ubound(gKeysWork)
        if gKeysWork(i).wszMsgString = gKeysEdit.wszMsgString then
            gEditIndex = i
            exit for
        end if
    next

    dim pWindow as CWindow ptr = new CWindow
    pWindow->DPI = AfxCWindowOwnerPtr(hWndParent)->DPI

    HWND_FRMKEYBOARDEDIT = pWindow->Create( hWndParent, L(342,"Assign Shortcut"), _
        @frmKeyboardEdit_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMKBEDIT_CLIENT_W, FRMKBEDIT_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the capture field ------------------------------------------------------------
    scope
        dim pCap as CWindow ptr = new CWindow
        HWND_FRMKEYBOARDEDIT_CAPTURE = pCap->Create( HWND_FRMKEYBOARDEDIT, "", _
            @frmKeyboardEdit_CaptureProc, 0, 0, 0, 0, _
            WS_CHILD or WS_VISIBLE or WS_TABSTOP or WS_CLIPSIBLINGS, 0 )
    end scope

    ' ---- toggles ----------------------------------------------------------------------
    ghEditTogDisable = PsToggle_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_TOGDISABLE )
    ghEditTogCtrl    = PsToggle_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_TOGCTRL )
    ghEditTogAlt     = PsToggle_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_TOGALT )
    ghEditTogShift   = PsToggle_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_TOGSHIFT )

    PsToggle_SetCheckChangedCallback( ghEditTogDisable, @frmKeyboardEdit_ToggleChanged )
    PsToggle_SetCheckChangedCallback( ghEditTogCtrl,    @frmKeyboardEdit_ToggleChanged )
    PsToggle_SetCheckChangedCallback( ghEditTogAlt,     @frmKeyboardEdit_ToggleChanged )
    PsToggle_SetCheckChangedCallback( ghEditTogShift,   @frmKeyboardEdit_ToggleChanged )

    ' ---- the key dropdown -------------------------------------------------------------
    ghEditKeyList = PsListTree_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_KEYLIST )
    PsListTree_SetFont( ghEditKeyList, ghFont(GUIFONT_11) )
    PsListTree_SetRowHeight( ghEditKeyList, FRMKBEDIT_KEYROW_H )
    PsListTree_SetPaintCallback( ghEditKeyList, @frmKeyboardEdit_KeyListPaint )
    PsListTree_SetTooltipCallback( ghEditKeyList, @frmKeyboardEdit_KeyListTooltip )
    ' Row 0 is "None" and every later row is gKeyNames(row-1) -- the two views' shared
    ' contract. Filled from the same vocabulary the capture field spells its keys with.
    PsListTree_AddString( ghEditKeyList, "None" )
    KeyBindings_EnsureKeyNames()
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        PsListTree_AddString( ghEditKeyList, gKeyNames(i).wszName )
    next
    PsListTree_SetSelChangeCallback( ghEditKeyList, @frmKeyboardEdit_KeyListChanged )

    ' ---- footer -----------------------------------------------------------------------
    ' Reset sits bottom-LEFT, opposite OK/Cancel -- the same split the shortcuts list uses,
    ' where Modify/Reset are left and OK/Cancel right. It is created first so it takes its
    ' place in the tab order before the two commit buttons.
    ghEditReset = PsButton_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_CMDRESET )
    PsButton_SetText( ghEditReset, L(341,"Reset") )
    PsButton_SetFont( ghEditReset, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghEditReset, @frmKeyboardEdit_ButtonClick )

    ghEditOK = PsButton_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_CMDOK )
    PsButton_SetText( ghEditOK, L(0,"OK") )
    PsButton_SetFont( ghEditOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( ghEditOK, true )
    PsButton_SetClickCallback( ghEditOK, @frmKeyboardEdit_ButtonClick )

    ghEditCancel = PsButton_Create( HWND_FRMKEYBOARDEDIT, IDC_FRMKEYBOARDEDIT_CMDCANCEL )
    PsButton_SetText( ghEditCancel, L(1,"Cancel") )
    PsButton_SetFont( ghEditCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghEditCancel, @frmKeyboardEdit_ButtonClick )

    ShowWindow( ghEditTogDisable, SW_SHOW )
    ShowWindow( ghEditTogCtrl, SW_SHOW )
    ShowWindow( ghEditTogAlt, SW_SHOW )
    ShowWindow( ghEditTogShift, SW_SHOW )
    ShowWindow( ghEditKeyList, SW_SHOW )
    ShowWindow( ghEditReset, SW_SHOW )
    ShowWindow( ghEditOK, SW_SHOW )
    ShowWindow( ghEditCancel, SW_SHOW )

    frmKeyboardEdit_ApplyTheme( HWND_FRMKEYBOARDEDIT )
    frmKeyboardEdit_PositionWindows( HWND_FRMKEYBOARDEDIT )
    frmKeyboardEdit_RefreshViews()

    ShowWindow( HWND_FRMKEYBOARDEDIT, SW_SHOW )
    UpdateWindow( HWND_FRMKEYBOARDEDIT )

    ' Focus the capture field: it is what the dialog is for, and IsDialogMessage will not
    ' Tab anywhere until something already holds the focus.
    SetFocus( HWND_FRMKEYBOARDEDIT_CAPTURE )

    ' ---- message loop -----------------------------------------------------------------
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' NO *_FilterMessage CALL HERE, and that is a property worth keeping: the key picker
        ' is a PsListTree with in-place editing off, and nothing else in this dialog owns a
        ' popup or a wrapped child. It was a PsComboBox once and did need one.

        ' ESCAPE. Two meanings, decided by where the focus is -- and the capture field's
        ' meaning has to win, or a field that swallows every other key would have no
        ' keyboard exit at all.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            if gEditCaptureFocus then
                SetFocus( ghEditOK )
            else
                SendMessage( HWND_FRMKEYBOARDEDIT, WM_CLOSE, 0, 0 )
            end if
            continue while
        end if

        if IsDialogMessage( HWND_FRMKEYBOARDEDIT, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    ghEditTogCtrl = 0 : ghEditTogAlt = 0 : ghEditTogShift = 0 : ghEditTogDisable = 0
    ghEditKeyList = 0 : ghEditOK = 0 : ghEditCancel = 0 : ghEditReset = 0
    gEditIndex = -1

    function = 0

end function
