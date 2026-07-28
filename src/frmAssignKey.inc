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

'  frmAssignKey.inc  --  assign one User Tool shortcut. See the header in the .bi for why
'  this dialog exists and why it is not frmKeyboardEdit.
'
'  TWO VIEWS, ONE MODEL, exactly as frmKeyboardEdit: a live capture field records a real
'  keystroke; three PsToggles and a scrolling key list describe the same thing by hand. Both
'  views WRITE the model and then RE-READ it -- neither writes the other -- so there is one
'  place the truth lives and no direction in which the two can drift.
'
'  The model here is the PARTS (a bare key name plus three booleans), not a combined
'  "Ctrl+Alt+R" string, because that is the shape a TYPE_TOOLS record stores. The combined
'  form is composed for display only, by frmUserTools_ShortcutText.

#include once "frmAssignKey.bi"


' ----------------------------------------------------------------------------------------
' THE MODEL, private to the dialog. gAssignKey* (the .bi) is the hand-off; these are the
' working copies, so Cancel costs nothing.
' ----------------------------------------------------------------------------------------
dim shared gTKKey     as DWSTRING     ' bare key name, "" = no shortcut
dim shared gTKCtrl    as boolean
dim shared gTKAlt     as boolean
dim shared gTKShift   as boolean
dim shared gTKWhy     as DWSTRING     ' why the last capture was refused ("" = it was not)
dim shared gTKSyncing as boolean      ' TRUE while a view is being refreshed from the model
dim shared gTKCaptureFocus as boolean ' does the capture field hold the focus?

dim shared as HWND ghTKTogCtrl, ghTKTogAlt, ghTKTogShift
dim shared as HWND ghTKKeyList, ghTKOK, ghTKCancel, ghTKClear

' The capture field's two animations, and the reason they exist, are frmKeyboardEdit's --
' see the long comment there. A blinking caret while the field is waiting, and a brief red
' flash of the frame when a keystroke is refused.
#Define IDT_TKEY_CARET   1
#Define IDT_TKEY_FLASH   2
#Define TKEY_FLASH_MS  200

dim shared gTKCaretOn as boolean
dim shared gTKFlashOn as boolean


' ========================================================================================
' Measure a string in DEVICE pixels, with the font it will actually be painted in.
'
' NOT tiko's getTextWidth: that one ends with UnScaleX and is correct for its callers, which
' hand the result to control-creation helpers that scale it up again. A PAINT path wants the
' number as measured -- frmKeyboardEdit clipped its prompt to "Press the nev" at 175% before
' this was separated out, and at 100% it looked perfect.
' ========================================================================================
private function frmAssignKey_TextWidthPx( byval hwnd as HWND, _
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
' What the capture field SHOWS: "Ctrl+Alt+R", or "" when there is no key yet.
'
' MODIFIER ORDER IS Ctrl, Shift, Alt and must stay that way -- it is what
' createToolsMenuShortcut has always emitted into the Tools menu, and the two strings sit a
' few pixels apart on screen when the menu is open over the dialog.
' ========================================================================================
private function frmAssignKey_Compose() as DWSTRING
    if len(gTKKey) = 0 then return ""
    dim as DWSTRING wsz
    if gTKCtrl  then wsz = wsz & "Ctrl+"
    if gTKShift then wsz = wsz & "Shift+"
    if gTKAlt   then wsz = wsz & "Alt+"
    return wsz & gTKKey
end function


' ========================================================================================
' DOES THIS KEYSTROKE ALREADY BELONG TO SOMETHING?
'
' Returns a sentence naming the owner, or "" when the keystroke is free. Two sources, and
' they are checked differently on purpose:
'
'   1. THE EDITOR'S OWN COMMANDS, through KeyBindings_FindCommandForKeys. That compares
'      STRINGS against what keybindings.ini stores, so the string handed to it must be in the
'      canonical form -- Ctrl, then ALT, then Shift. THAT IS NOT THE ORDER THIS DIALOG
'      DISPLAYS: createToolsMenuShortcut has always written the Tools menu as Ctrl+Shift+Alt,
'      and frmUserTools_ShortcutText matches it. The two orders coexist deliberately, and the
'      comparison here MUST use the bindings one -- built by calling
'      frmKeyboardEdit_BuildKeyString rather than composing it again, so the two cannot drift.
'      Get that wrong and every three-modifier shortcut silently reports no clash.
'
'   2. THE USER TOOLS, and 3. THE BUILD CONFIGURATIONS, both compared as PARTS -- key name
'      plus three booleans -- which sides the ordering question entirely.
'
' WHICH ARRAY EACH OF THOSE TWO READS, AND WHY IT DIFFERS.
' For the domain the caller is editing we walk its STAGING array (gToolsWork / gBuildsWork)
' and skip nSkipIndex, so a clash against a record the user has changed but not yet committed
' is caught. For the OTHER domain we walk the COMMITTED array (gConfig.Tools /
' gConfig.Builds) and skip nothing.
'
' That is correct rather than a fudge because BOTH host dialogs are modal from frmMain and
' cannot be open at the same time: the other domain's staging array is guaranteed empty (each
' dialog seeds it on entry and erases it in WM_DESTROY), so the committed array is the only
' live truth there is. THE DAY EITHER DIALOG GOES MODELESS, THIS IS THE ASSUMPTION TO
' RE-CHECK -- it would then have to read whichever array that dialog currently holds.
'
' A record with no key clashes with nothing, and neither does one whose key does not resolve
' (it can never reach an accelerator table, so it cannot collide inside one).
' ========================================================================================
' One stored record against the candidate, compared as PARTS. wszCandidateUpper is
' pre-uppercased by the caller because this runs once per record per keystroke.
' The stored flags arrive as longs -- TYPE_TOOLS and TYPE_BUILDS both keep them that way so
' the config writer emits 0/1 rather than true/false.
private function AssignKey_PartsMatch( byval wszStoredKey as DWSTRING, _
                                       byval nStoredCtrl as long, _
                                       byval nStoredAlt as long, _
                                       byval nStoredShift as long, _
                                       byval wszCandidateUpper as DWSTRING, _
                                       byval bCtrl as boolean, _
                                       byval bAlt as boolean, _
                                       byval bShift as boolean ) as boolean
    if ucase(trim(wszStoredKey)) <> wszCandidateUpper then return false
    if (nStoredCtrl  <> 0) <> bCtrl  then return false
    if (nStoredAlt   <> 0) <> bAlt   then return false
    if (nStoredShift <> 0) <> bShift then return false
    return true
end function


private function AssignKey_OwnerText( byval wszDescription as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWho = wszDescription
    if len(wszWho) = 0 then wszWho = L(513,"(untitled)")
    return L(526,"Already used by") & ": " & wszWho
end function


function frmAssignKey_ClashText( byval wszKey as DWSTRING, _
                                   byval bCtrl as boolean, _
                                   byval bAlt as boolean, _
                                   byval bShift as boolean, _
                                   byval nDomain as long, _
                                   byval nSkipIndex as long ) as DWSTRING

    if len(wszKey) = 0 then return ""

    dim as long vk = KeyBindings_PickListKeyToValue( wszKey )
    if vk = 0 then return ""

    ' --- 1. the editor's commands ---
    dim as DWSTRING wszCanonical = frmKeyboardEdit_BuildKeyString( vk, bCtrl, bAlt, bShift )
    dim as DWSTRING wszCmd = KeyBindings_FindCommandForKeys( wszCanonical )
    if len(wszCmd) then return L(526,"Already used by") & ": " & wszCmd

    dim as DWSTRING wszUpper = ucase(wszKey)

    ' --- 2. the user tools ---
    if nDomain = ASSIGNKEY_DOMAIN_TOOLS then
        for i as long = lbound(gToolsWork) to ubound(gToolsWork)
            if i = nSkipIndex then continue for
            if AssignKey_PartsMatch( gToolsWork(i).wszKey, gToolsWork(i).IsCtrl, _
                                     gToolsWork(i).IsAlt, gToolsWork(i).IsShift, _
                                     wszUpper, bCtrl, bAlt, bShift ) then
                return AssignKey_OwnerText( gToolsWork(i).wszDescription )
            end if
        next
    else
        for i as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
            if AssignKey_PartsMatch( gConfig.Tools(i).wszKey, gConfig.Tools(i).IsCtrl, _
                                     gConfig.Tools(i).IsAlt, gConfig.Tools(i).IsShift, _
                                     wszUpper, bCtrl, bAlt, bShift ) then
                return AssignKey_OwnerText( gConfig.Tools(i).wszDescription )
            end if
        next
    end if

    ' --- 3. the build configurations ---
    if nDomain = ASSIGNKEY_DOMAIN_BUILDS then
        for i as long = lbound(gBuildsWork) to ubound(gBuildsWork)
            if i = nSkipIndex then continue for
            if AssignKey_PartsMatch( gBuildsWork(i).wszKey, gBuildsWork(i).IsCtrl, _
                                     gBuildsWork(i).IsAlt, gBuildsWork(i).IsShift, _
                                     wszUpper, bCtrl, bAlt, bShift ) then
                return AssignKey_OwnerText( gBuildsWork(i).wszDescription )
            end if
        next
    else
        for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
            if AssignKey_PartsMatch( gConfig.Builds(i).wszKey, gConfig.Builds(i).IsCtrl, _
                                     gConfig.Builds(i).IsAlt, gConfig.Builds(i).IsShift, _
                                     wszUpper, bCtrl, bAlt, bShift ) then
                return AssignKey_OwnerText( gConfig.Builds(i).wszDescription )
            end if
        next
    end if

    return ""
end function


' The clash for the combination currently in the model. Recomputed rather than cached: every
' view change goes through RefreshViews, and a cached verdict is one more thing that can be
' stale at the moment OK is pressed.
private function frmAssignKey_CurrentClash() as DWSTRING
    return frmAssignKey_ClashText( gTKKey, gTKCtrl, gTKAlt, gTKShift, _
                                   gAssignKeyDomain, gAssignKeyIndex )
end function


' ========================================================================================
' Start or stop the caret blink, from one place, based on whether there is anything to blink
' FOR: the field must hold the focus AND still be empty. A blink timer left armed wakes the
' process several times a second to repaint something nobody is looking at.
' ========================================================================================
private sub frmAssignKey_SyncCaretTimer()

    if HWND_FRMASSIGNKEY_CAPTURE = 0 then exit sub

    dim as boolean bEmpty   = (len(gTKKey) = 0)
    dim as boolean bWaiting = (gTKCaptureFocus andalso bEmpty)

    if bWaiting then
        ' The SYSTEM caret rate, so this matches every other text field on the machine.
        ' GetCaretBlinkTime returns -1 when the user has turned blinking off for
        ' accessibility; honour that by not animating rather than by picking a rate.
        dim as long nRate = cast(long, GetCaretBlinkTime())
        if nRate <= 0 then
            KillTimer( HWND_FRMASSIGNKEY_CAPTURE, IDT_TKEY_CARET )
            gTKCaretOn = true      ' steady, not absent -- still says "input goes here"
        else
            SetTimer( HWND_FRMASSIGNKEY_CAPTURE, IDT_TKEY_CARET, nRate, 0 )
        end if
    else
        KillTimer( HWND_FRMASSIGNKEY_CAPTURE, IDT_TKEY_CARET )
        gTKCaretOn = false
    end if

    AfxRedrawWindow( HWND_FRMASSIGNKEY_CAPTURE )

end sub


' ========================================================================================
' Model -> views.
' ========================================================================================
private sub frmAssignKey_RefreshViews()

    if gTKSyncing then exit sub
    gTKSyncing = true

    PsToggle_SetChecked( ghTKTogCtrl,  gTKCtrl )
    PsToggle_SetChecked( ghTKTogAlt,   gTKAlt )
    PsToggle_SetChecked( ghTKTogShift, gTKShift )

    ' Row 0 of the picker is "None"; the vocabulary follows in gKeyNames() order.
    dim as long nSel = 0
    if len(gTKKey) then
        KeyBindings_EnsureKeyNames()
        for i as long = lbound(gKeyNames) to ubound(gKeyNames)
            if ucase(gKeyNames(i).wszName) = ucase(gTKKey) then
                nSel = i + 1
                exit for
            end if
        next
    end if
    PsListTree_SetCurSel( ghTKKeyList, nSel )

    ' A CLASHING COMBINATION CANNOT BE COMMITTED. The capture field refuses a clashing
    ' KEYSTROKE outright (see CaptureProc), but the picker and the toggles have to allow a
    ' clashing state to exist transiently -- you must be able to switch Ctrl on before you have
    ' chosen the key, and the intermediate combinations along the way are nobody's business.
    ' So this path warns instead, and disables OK for as long as the warning stands. Cancel and
    ' Clear stay live, which is what keeps that from being a trap.
    PsButton_SetEnabled( ghTKOK, iif( len(frmAssignKey_CurrentClash()) = 0, true, false ) )

    gTKSyncing = false

    ' The model just changed, so whether there is anything to blink for may have changed with
    ' it -- capturing a shortcut stops the caret, Clear starts it again.
    frmAssignKey_SyncCaretTimer()

    if HWND_FRMASSIGNKEY_CAPTURE then AfxRedrawWindow( HWND_FRMASSIGNKEY_CAPTURE )
    if HWND_FRMASSIGNKEY then AfxRedrawWindow( HWND_FRMASSIGNKEY )

end sub


' ========================================================================================
' Views -> model.
' ========================================================================================
private sub frmAssignKey_ModelFromViews()

    if gTKSyncing then exit sub

    dim as long nSel = PsListTree_GetCurSel( ghTKKeyList )
    if nSel <= 0 then
        ' "None" clears the whole binding, modifiers included. A shortcut with modifiers and
        ' no key is not a shortcut, and leaving IsCtrl set on a keyless tool is exactly what
        ' put a stray "Ctrl+" into the Tools menu in the version this replaces.
        gTKKey   = ""
        gTKCtrl  = false
        gTKAlt   = false
        gTKShift = false
    else
        KeyBindings_EnsureKeyNames()
        dim as long idx = nSel - 1
        if (idx >= lbound(gKeyNames)) andalso (idx <= ubound(gKeyNames)) then
            gTKKey = gKeyNames(idx).wszName
        end if
        gTKCtrl  = PsToggle_GetChecked( ghTKTogCtrl )
        gTKAlt   = PsToggle_GetChecked( ghTKTogAlt )
        gTKShift = PsToggle_GetChecked( ghTKTogShift )
    end if

    gTKWhy = ""
    frmAssignKey_RefreshViews()

end sub


' ========================================================================================
' THE CAPTURE FIELD
'
' A bespoke owner-drawn child, local to this dialog -- not a reusable sibling and not a
' PsTextBox (that wraps a RichEdit, which would fight for every keystroke). Lifted from
' frmKeyboardEdit, whose header explains the three load-bearing parts of the key handling:
'
'   1. WM_GETDLGCODE claims DLGC_WANTALLKEYS PER MESSAGE and only for keys this field wants.
'      Claiming unconditionally swallows Tab and strands the user.
'   2. Alt+key arrives as WM_SYSKEYDOWN, not WM_KEYDOWN.
'   3. A CLAIMED KEY'S WM_CHAR MUST BE SWALLOWED or the system beeps on every press. The
'      keystroke still works, so the beep is the only symptom and no assertion can hear it.
'
' ESCAPE IS NOT CLAIMED -- it is the field's exit.
' ========================================================================================
private function frmAssignKey_CaptureProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    ' Armed in WM_KEYDOWN/WM_SYSKEYDOWN, consumed by the WM_CHAR immediately following.
    static as boolean bSwallowChar

    select case uMsg

        case WM_GETDLGCODE
            dim as MSG ptr pMsg = cast(MSG ptr, lParam)
            if pMsg then
                if (pMsg->message = WM_KEYDOWN) orelse (pMsg->message = WM_SYSKEYDOWN) orelse _
                   (pMsg->message = WM_CHAR)    orelse (pMsg->message = WM_SYSCHAR) then
                    if pMsg->wParam <> VK_ESCAPE then return DLGC_WANTALLKEYS
                end if
            end if
            return DLGC_WANTARROWS

        case WM_SETFOCUS
            gTKCaptureFocus = true
            gTKCaretOn      = true      ' show immediately; do not wait out a blink period
            frmAssignKey_SyncCaretTimer()
            return 0

        case WM_KILLFOCUS
            gTKCaptureFocus = false
            frmAssignKey_SyncCaretTimer()
            return 0

        case WM_TIMER
            select case wParam
                case IDT_TKEY_CARET
                    gTKCaretOn = not gTKCaretOn
                    AfxRedrawWindow( hwnd )
                case IDT_TKEY_FLASH
                    ' One-shot: killed here rather than arming a repeating timer and ignoring
                    ' most of its ticks.
                    KillTimer( hwnd, IDT_TKEY_FLASH )
                    gTKFlashOn = false
                    AfxRedrawWindow( hwnd )
            end select
            return 0

        case WM_DESTROY
            KillTimer( hwnd, IDT_TKEY_CARET )
            KillTimer( hwnd, IDT_TKEY_FLASH )

        case WM_LBUTTONDOWN
            SetFocus( hwnd )
            return 0

        case WM_KEYDOWN, WM_SYSKEYDOWN
            dim as long vk = cast(long, wParam)

            ' Escape leaves the field; deliberately not capturable here.
            if vk = VK_ESCAPE then
                bSwallowChar = false
                return DefWindowProc( hwnd, uMsg, wParam, lParam )
            end if

            ' A modifier on its own is not a complete shortcut. Repaint so the live preview
            ' follows the user's fingers, but commit nothing.
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

            ' THE RULES ARE frmKeyboardEdit'S, called rather than copied. Sharing the function
            ' is the whole reason the two dialogs cannot come to disagree about which
            ' keystrokes are legal -- a duplicated rule set would drift the first time either
            ' was touched.
            dim as DWSTRING wszWhy
            dim as boolean bAccept = frmKeyboardEdit_AcceptCapture( vk, bCtrl, bAlt, bShift, wszWhy )

            ' AND IT MUST NOT ALREADY BELONG TO SOMETHING. Checked here rather than inside
            ' AcceptCapture because that function is shared with frmKeyboardEdit, where the
            ' answer is different: a clash there is reported and still allowed (the user may be
            ' deliberately taking a shortcut off another command, and the dialog is editing
            ' every command at once). A user tool or a build configuration cannot take a
            ' binding off the editor -- both accelerator tables are live at the same time --
            ' so here it is a refusal.
            if bAccept then
                dim as DWSTRING wszOwner = frmAssignKey_ClashText( KeyBindings_VKToName( vk ), _
                                                                     bCtrl, bAlt, bShift, _
                                                                     gAssignKeyDomain, gAssignKeyIndex )
                if len(wszOwner) then
                    bAccept = false
                    wszWhy  = wszOwner
                end if
            end if

            if bAccept then
                ' The BARE NAME, not a combined string: that is what a TYPE_TOOLS record
                ' stores and what KeyBindings_PickListKeyToValue resolves.
                gTKKey   = KeyBindings_VKToName( vk )
                gTKCtrl  = bCtrl
                gTKAlt   = bAlt
                gTKShift = bShift
                gTKWhy   = ""
                frmAssignKey_RefreshViews()
            else
                ' REFUSED. The message line explains it in words, but words alone make
                ' "I heard you and said no" look identical to "I never saw that key" -- the
                ' field just sits there either way. A brief flash of the frame is motion, and
                ' motion is what pulls the eye back to a control already being stared at.
                gTKWhy     = wszWhy
                gTKFlashOn = true
                SetTimer( hwnd, IDT_TKEY_FLASH, TKEY_FLASH_MS, 0 )
                AfxRedrawWindow( hwnd )
                AfxRedrawWindow( HWND_FRMASSIGNKEY )
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
            ' Three states, and the refusal flash outranks focus: for its 200 ms the frame is
            ' carrying news, and the focus it would otherwise report has not changed anyway.
            dim as long curve = AfxScaleY(6)
            dim as COLORREF penclr = ghEditor.Divider
            dim as long     penw   = 1
            if gTKFlashOn then
                penclr = ghGeneral.iconfail : penw = 2
            elseif gTKCaptureFocus then
                penclr = ghUI.AccentColor   : penw = 2
            end if
            buffer.SetPenColor( penclr )
            buffer.PaintRoundOutline( @rc, curve, penw )

            dim as DWSTRING wszShow = frmAssignKey_Compose()
            dim as COLORREF clr = ghEditor.ForeColorText
            dim as boolean bWaiting = (len(wszShow) = 0)
            if bWaiting then
                wszShow = L(501,"Press the new shortcut")
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
                ' text alone and appending the caret shuffles the whole sentence a few pixels
                ' left and right twice a second, which reads as a rendering fault.
                dim as long tw     = frmAssignKey_TextWidthPx( hwnd, wszShow, ghFont(GUIFONTBOLD_11) )
                dim as long caretW = AfxScaleX(2)
                dim as long gapW   = AfxScaleX(6)
                dim as long x0     = (rc.right - (tw + gapW + caretW)) \ 2
                if x0 < 0 then x0 = 0

                dim as RECT rcText = rc
                rcText.left  = x0
                rcText.right = x0 + tw
                buffer.PaintText( wszShow, @rcText, DT_LEFT or DT_VCENTER or DT_SINGLELINE )

                if gTKCaretOn then
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

private sub frmAssignKey_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    frmAssignKey_ModelFromViews()
end sub

private sub frmAssignKey_KeyListChanged( byval hList as HWND, byval row as integer )
    frmAssignKey_ModelFromViews()
end sub

' No tooltips on the picker: with no callback PsListTree falls back to the row's own text,
' which here would pop "F5" over a row already reading "F5".
private function frmAssignKey_KeyListTooltip( byval hList as HWND, byval row as integer ) as DWSTRING
    return ""
end function

private sub frmAssignKey_KeyListPaint( byval p as PSLISTTREE_PAINTINFO ptr )
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

private sub frmAssignKey_ButtonClick( byval hButton as HWND, byval id as long )
    select case id
        ' CLEAR, not "Reset": a user tool has no shipped default to go back to, so the only
        ' meaning available is "no shortcut at all". It does NOT close the dialog -- everything
        ' here is still private until OK, so the user gets to see what Clear did first.
        case IDC_FRMASSIGNKEY_CMDCLEAR
            gTKKey   = ""
            gTKCtrl  = false
            gTKAlt   = false
            gTKShift = false
            gTKWhy   = ""
            frmAssignKey_RefreshViews()
            SetFocus( HWND_FRMASSIGNKEY_CAPTURE )

        case IDC_FRMASSIGNKEY_CMDOK
            ' The ONLY write to the hand-off. Everything above this line edits private copies.
            gAssignKeyName  = gTKKey
            gAssignKeyCtrl  = gTKCtrl
            gAssignKeyAlt   = gTKAlt
            gAssignKeyShift = gTKShift
            SendMessage( HWND_FRMASSIGNKEY, WM_CLOSE, 0, 0 )

        case IDC_FRMASSIGNKEY_CMDCANCEL
            SendMessage( HWND_FRMASSIGNKEY, WM_CLOSE, 0, 0 )
    end select
end sub


' ========================================================================================
' Paint the dialog's own chrome: the tool name, the three toggle captions, and the message
' line. Everything else on screen belongs to a child control.
'
' KEEP THE RUNNING y IN STEP WITH frmAssignKey_PositionWindows -- the two walk the same
' sum, and the .bi writes it down.
' ========================================================================================
private function frmAssignKey_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    dim as long mx   = AfxScaleX(FRMASSIGNKEY_MARGIN)
    dim as long y    = AfxScaleY(FRMASSIGNKEY_MARGIN)
    dim as long rowH = AfxScaleY(FRMASSIGNKEY_ROW_H)

    dim as RECT rcT
    rcT.left = mx : rcT.right = rc.right - mx

    ' The tool being edited, bold. WITHOUT THIS the dialog is a shortcut editor with no
    ' indication of what it is a shortcut FOR -- and it can be reached from any of six rows.
    rcT.top = y : rcT.bottom = y + rowH
    buffer.SetForeColor( ghPanel.ForeColorHot )
    buffer.SetFont( ghFont(GUIFONTBOLD_11) )
    dim as DWSTRING wszWho = gAssignKeyCaption
    if len(wszWho) = 0 then wszWho = L(513,"(untitled)")
    buffer.PaintText( wszWho, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    y += rowH

    ' NO LABEL ABOVE THE CAPTURE FIELD: the empty field shows "Press the new shortcut" itself,
    ' where the user is already looking.
    y += AfxScaleY(FRMASSIGNKEY_CAPTURE_H) + AfxScaleY(18)

    ' The three toggle captions. PsToggle carries no caption and therefore no font, so every
    ' host draws its own -- the same arrangement the Options dialog's rows use.
    rcT.top = y : rcT.bottom = y + rowH
    dim as long togW = AfxScaleX(FRMASSIGNKEY_TOGW)
    dim as long capW = AfxScaleX(FRMASSIGNKEY_CAPW)
    dim as long x    = mx
    buffer.SetForeColor( ghPanel.ForeColor )
    buffer.SetFont( ghFont(GUIFONT_11) )

    #macro TKCAPTION( _text_ )
        rcT.left = x + togW : rcT.right = x + togW + capW
        buffer.PaintText( _text_, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE )
        x += togW + capW + AfxScaleX(8)
    #endmacro

    TKCAPTION( "Ctrl" )
    TKCAPTION( "Alt" )
    TKCAPTION( "Shift" )

    ' THE MESSAGE LINE. One line serving two purposes, and the refusal wins -- frmKeyboardEdit's
    ' rule and its reasoning: a rejected keystroke is news about what the user just did, a
    ' standing clash is a property of what is already there.
    y += rowH + AfxScaleY(6) + AfxScaleY(FRMASSIGNKEY_KEYLIST_H) + AfxScaleY(12)
    rcT.left = mx : rcT.right = rc.right - mx
    rcT.top = y : rcT.bottom = y + rowH

    dim as DWSTRING wszMsg = gTKWhy
    if len(wszMsg) = 0 then wszMsg = frmAssignKey_CurrentClash()
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
private sub frmAssignKey_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long mx   = AfxScaleX(FRMASSIGNKEY_MARGIN)
    dim as long rowH = AfxScaleY(FRMASSIGNKEY_ROW_H)
    dim as long y    = AfxScaleY(FRMASSIGNKEY_MARGIN) + rowH

    ' The capture field.
    dim as long capH = AfxScaleY(FRMASSIGNKEY_CAPTURE_H)
    SetWindowPos( HWND_FRMASSIGNKEY_CAPTURE, 0, mx, y, rc.right - (mx * 2), capH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    y += capH + AfxScaleY(18)

    ' The three modifier toggles, on one line.
    dim as long capW = AfxScaleX(FRMASSIGNKEY_CAPW)
    dim as long x    = mx
    dim as long tw = 0, th = 0

    #macro TKPLACE( _h_ )
        PsToggle_GetIdealSize( _h_, tw, th )
        SetWindowPos( _h_, 0, x, y + (rowH - th) \ 2, tw, th, SWP_NOZORDER or SWP_NOACTIVATE )
        x += AfxScaleX(FRMASSIGNKEY_TOGW) + capW + AfxScaleX(8)
    #endmacro

    TKPLACE( ghTKTogCtrl )
    TKPLACE( ghTKTogAlt )
    TKPLACE( ghTKTogShift )

    ' The key picker gets its own full-width band -- it is a scrolling list, not a dropdown,
    ' so it occupies space rather than borrowing the screen.
    y += rowH + AfxScaleY(6)
    SetWindowPos( ghTKKeyList, 0, mx, y, rc.right - (mx * 2), AfxScaleY(FRMASSIGNKEY_KEYLIST_H), _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    ' Footer: Clear bottom-left, OK / Cancel bottom-right -- the same split frmKeyboard and
    ' frmKeyboardEdit use.
    dim as long btnW = AfxScaleX(FRMASSIGNKEY_BTN_W)
    dim as long btnH = AfxScaleY(FRMASSIGNKEY_BTN_H)
    dim as long btnY = rc.bottom - AfxScaleY(FRMASSIGNKEY_MARGIN) - btnH
    SetWindowPos( ghTKClear,  0, mx, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghTKCancel, 0, rc.right - mx - btnW, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghTKOK,     0, rc.right - mx - (btnW * 2) - AfxScaleX(8), _
                  btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Theme every child that carries its own colours.
'
' Through the Options dialog's shared builders rather than a hand-copied recipe: this file is
' included after modOptionsRows.inc in the unity build, so unlike frmKeyboard/frmKeyboardEdit
' it can simply call them, and the palettes cannot drift.
' ========================================================================================
private sub frmAssignKey_ApplyTheme( byval hwnd as HWND )

    dim as PSTOGGLE_COLORS tc
    OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghTKTogCtrl,  @tc )
    PsToggle_SetColors( ghTKTogAlt,   @tc )
    PsToggle_SetColors( ghTKTogShift, @tc )

    PsListTree_SetBackColor( ghTKKeyList, ghEditor.BackColorText )
    PsListTree_SetScrollBarColors( ghTKKeyList, ghPanel.BackColorScrollBar, _
                                   ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
    OptionsTheme_ApplyListBorder( ghTKKeyList )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    PsButton_SetColors( ghTKClear,  @bc ) : PsButton_Refresh( ghTKClear )
    PsButton_SetColors( ghTKOK,     @bc ) : PsButton_Refresh( ghTKOK )
    PsButton_SetColors( ghTKCancel, @bc ) : PsButton_Refresh( ghTKCancel )

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmAssignKey_WndProc( _
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
            return frmAssignKey_OnPaint( hwnd )

        case WM_SIZE
            frmAssignKey_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes through its click callback. What still arrives here is IDCANCEL,
            ' which IsDialogMessage synthesises for Escape.
            select case loword(wParam)
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case WM_CLOSE
            ' NOTHING IS WRITTEN HERE. The commit is on the OK path only, so reaching this by
            ' any route leaves gAssignKey* exactly as the caller set them.
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMASSIGNKEY         = 0
            HWND_FRMASSIGNKEY_CAPTURE = 0
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmAssignKey_Show
'
' Modal. Reads gAssignKey* on the way in and writes them back on OK; the caller stages that.
' ========================================================================================
function frmAssignKey_Show( byval hWndParent as HWND ) as LRESULT

    ' Seed the private model from the hand-off.
    gTKKey   = gAssignKeyName
    gTKCtrl  = gAssignKeyCtrl
    gTKAlt   = gAssignKeyAlt
    gTKShift = gAssignKeyShift
    gTKWhy   = ""
    gTKSyncing      = false
    gTKCaptureFocus = false
    gTKFlashOn      = false

    ' A tool whose key survived from an older settings.ini may hold a name this vocabulary
    ' does not know -- or the literal "None" the previous dialog wrote as an affordance. Drop
    ' it here rather than showing a picker with nothing selected and a capture field
    ' displaying a shortcut that cannot fire.
    if len(gTKKey) then
        if KeyBindings_PickListKeyToValue( gTKKey ) = 0 then
            gTKKey  = ""
            gTKCtrl = false : gTKAlt = false : gTKShift = false
        end if
    end if

    dim pWindow as CWindow ptr = new CWindow
    pWindow->DPI = AfxCWindowOwnerPtr(hWndParent)->DPI

    HWND_FRMASSIGNKEY = pWindow->Create( hWndParent, L(499,"Assign Shortcut"), _
        @frmAssignKey_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMASSIGNKEY_CLIENT_W, FRMASSIGNKEY_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the capture field ------------------------------------------------------------
    ' CREATION ORDER IS THE TAB ORDER: these controls live on an ordinary CWindow, not a
    ' dialog template, so GetNextDlgTabItem walks the child Z-order and Z-order is creation
    ' order. capture -> Ctrl -> Alt -> Shift -> picker -> Clear -> OK -> Cancel.
    dim pCap as CWindow ptr = new CWindow
    HWND_FRMASSIGNKEY_CAPTURE = pCap->Create( HWND_FRMASSIGNKEY, "", _
        @frmAssignKey_CaptureProc, 0, 0, 0, 0, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or WS_CLIPSIBLINGS, 0 )

    ' ---- toggles ----------------------------------------------------------------------
    ghTKTogCtrl  = PsToggle_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_TOGCTRL )
    ghTKTogAlt   = PsToggle_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_TOGALT )
    ghTKTogShift = PsToggle_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_TOGSHIFT )

    PsToggle_SetCheckChangedCallback( ghTKTogCtrl,  @frmAssignKey_ToggleChanged )
    PsToggle_SetCheckChangedCallback( ghTKTogAlt,   @frmAssignKey_ToggleChanged )
    PsToggle_SetCheckChangedCallback( ghTKTogShift, @frmAssignKey_ToggleChanged )

    ' ---- the key picker ---------------------------------------------------------------
    ghTKKeyList = PsListTree_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_KEYLIST )
    PsListTree_SetFont( ghTKKeyList, ghFont(GUIFONT_11) )
    ' UNSCALED: PsListTree scales what it is handed, so a pre-scaled value is scaled twice.
    PsListTree_SetRowHeight( ghTKKeyList, FRMASSIGNKEY_KEYROW_H )
    PsListTree_SetPaintCallback( ghTKKeyList, @frmAssignKey_KeyListPaint )
    PsListTree_SetTooltipCallback( ghTKKeyList, @frmAssignKey_KeyListTooltip )
    ' Row 0 is "None" and every later row is gKeyNames(row-1) -- the contract the two views
    ' share. Filled from the same vocabulary the capture field spells its keys with.
    PsListTree_AddString( ghTKKeyList, "None" )
    KeyBindings_EnsureKeyNames()
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        PsListTree_AddString( ghTKKeyList, gKeyNames(i).wszName )
    next
    PsListTree_SetSelChangeCallback( ghTKKeyList, @frmAssignKey_KeyListChanged )

    ' ---- footer -----------------------------------------------------------------------
    ghTKClear = PsButton_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_CMDCLEAR )
    PsButton_SetText( ghTKClear, L(517,"Clear") )
    PsButton_SetFont( ghTKClear, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghTKClear, @frmAssignKey_ButtonClick )

    ghTKOK = PsButton_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_CMDOK )
    PsButton_SetText( ghTKOK, L(0,"OK") )
    PsButton_SetFont( ghTKOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( ghTKOK, true )
    PsButton_SetClickCallback( ghTKOK, @frmAssignKey_ButtonClick )

    ghTKCancel = PsButton_Create( HWND_FRMASSIGNKEY, IDC_FRMASSIGNKEY_CMDCANCEL )
    PsButton_SetText( ghTKCancel, L(1,"Cancel") )
    PsButton_SetFont( ghTKCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghTKCancel, @frmAssignKey_ButtonClick )

    ShowWindow( ghTKTogCtrl, SW_SHOW )
    ShowWindow( ghTKTogAlt, SW_SHOW )
    ShowWindow( ghTKTogShift, SW_SHOW )
    ShowWindow( ghTKKeyList, SW_SHOW )
    ShowWindow( ghTKClear, SW_SHOW )
    ShowWindow( ghTKOK, SW_SHOW )
    ShowWindow( ghTKCancel, SW_SHOW )

    frmAssignKey_ApplyTheme( HWND_FRMASSIGNKEY )
    frmAssignKey_PositionWindows( HWND_FRMASSIGNKEY )
    frmAssignKey_RefreshViews()

    ShowWindow( HWND_FRMASSIGNKEY, SW_SHOW )
    UpdateWindow( HWND_FRMASSIGNKEY )

    ' Focus the capture field: it is what the dialog is for, and IsDialogMessage will not Tab
    ' anywhere until something already holds the focus.
    SetFocus( HWND_FRMASSIGNKEY_CAPTURE )

    ' ---- message loop -----------------------------------------------------------------
    ' NO *_FilterMessage CALL, and that is a property worth keeping -- see the .bi header.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' ESCAPE. Two meanings, decided by where the focus is -- and the capture field's has
        ' to win, or a field that swallows every other key would have no keyboard exit at all.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            if gTKCaptureFocus then
                SetFocus( ghTKOK )
            else
                SendMessage( HWND_FRMASSIGNKEY, WM_CLOSE, 0, 0 )
            end if
            continue while
        end if

        if IsDialogMessage( HWND_FRMASSIGNKEY, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    ' Both CWindow wrappers, not just the outer one. The destructor unregisters the
    ' per-instance window class CWindow.Create registered, so a leaked child wrapper leaks a
    ' class registration per open -- frmKeyboardEdit leaks its capture field's wrapper this
    ' way and is worth fixing there too.
    if pCap then delete(pCap)
    if pWindow then delete(pWindow)

    ghTKTogCtrl = 0 : ghTKTogAlt = 0 : ghTKTogShift = 0
    ghTKKeyList = 0 : ghTKOK = 0 : ghTKCancel = 0 : ghTKClear = 0

    function = 0

end function
