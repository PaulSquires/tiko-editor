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

#include once "clsConfig.bi"
#include once "frmKeyboard.bi"
#include once "modRoutines.bi"

' ========================================================================================
' Seed the staging copy from the live bindings.
'
' Called once from frmKeyboard_Show BEFORE any control is created: every control is built
' from gKeysWork(), never from gKeys().
' ========================================================================================
sub frmKeyboard_LoadWork()
    erase gKeysWork
    if ubound(gKeys) < lbound(gKeys) then exit sub

    redim gKeysWork( ubound(gKeys) ) as KEYBINDINGS_TYPE
    for i as long = lbound(gKeys) to ubound(gKeys)
        gKeysWork(i) = gKeys(i)
    next
end sub


' ========================================================================================
' Commit the staging copy. ONLY the OK path calls this.
'
' THE ORDER OF THE LAST TWO STEPS IS LOAD-BEARING, and it is not obvious from their names.
' frmKeyboard_BuildAcceleratorTable calls frmKeyboard_CreateDefaultKeyBindings, which
' ERASES gKeys() and rebuilds it from the defaults table plus a fresh read of
' keybindings.ini. Rebuild before writing and it reinstates the file as it was before the
' edit -- the user's changes vanish with no error anywhere.
'
' The copy into gKeys() is therefore not redundant even though the rebuild overwrites it:
' frmKeyboard_SaveKeyBindings serialises gKeys(), so that is how the staging copy reaches
' the disk. And because the accelerator table is then rebuilt from what was actually
' written, the commit doubles as a real round-trip check of the file just saved -- anything
' the format cannot represent is visibly lost now rather than at the next startup.
' ========================================================================================
sub frmKeyboard_CommitWork()

    if ubound(gKeysWork) >= lbound(gKeysWork) then
        redim gKeys( ubound(gKeysWork) ) as KEYBINDINGS_TYPE
        for i as long = lbound(gKeysWork) to ubound(gKeysWork)
            gKeys(i) = gKeysWork(i)
        next
    end if

    ' 1. DISK FIRST.
    frmKeyboard_SaveKeyBindings( AfxGetExePathName & "settings\keybindings.ini" )

    ' 2. Then rebuild, which re-reads what step 1 just wrote.
    frmKeyboard_BuildAcceleratorTable()

end sub


' ========================================================================================
' THE STAGING / PERSISTENCE SELF-TEST                        (TIKO_KEYBOARD_SELFTEST=1)
'
' Runs at startup, beside KeyBindings_RunSelfTest, and deliberately touches NOTHING the
' user owns: it never calls frmKeyboard_CommitWork (which would rewrite the real
' keybindings.ini and rebuild the live accelerator table) and it serialises to a temp file.
'
' WHAT IT IS FOR. "Cancel reverts" is a claim about something NOT happening, which is the
' hardest kind to notice going wrong -- a staging copy that quietly aliases the live array
' looks completely normal until the day a user cancels and finds their bindings changed
' anyway. Asserting it costs four lines; discovering it by hand costs a support report.
' ========================================================================================
sub frmKeyboard_RunSelfTest()

    if environ("TIKO_KEYBOARD_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro FKASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? ""
    ? "--- TIKO_KEYBOARD_SELFTEST (staging + persistence) ---"

    ' ---- TEST A: the staging copy is a COPY. ----
    frmKeyboard_LoadWork()

    FKASSERT( ubound(gKeysWork) = ubound(gKeys), "staging copy has a different length to gKeys" )

    dim as boolean bSame = true
    for i as long = lbound(gKeys) to ubound(gKeys)
        ' Each comparison is hoisted into its own boolean first. A DWSTRING <> DWSTRING
        ' yields a non-boolean while bDefaultDisabled <> bDefaultDisabled yields a boolean,
        ' and orelse-ing the two kinds together is fbc warning 38.
        dim as boolean bMsg  = (gKeysWork(i).wszMsgString    <> gKeys(i).wszMsgString)
        dim as boolean bUser = (gKeysWork(i).wszUserKeys     <> gKeys(i).wszUserKeys)
        dim as boolean bDef  = (gKeysWork(i).wszDefaultKeys  <> gKeys(i).wszDefaultKeys)
        dim as boolean bDis  = (gKeysWork(i).bDefaultDisabled <> gKeys(i).bDefaultDisabled)
        if bMsg orelse bUser orelse bDef orelse bDis then
            bSame = false
            exit for
        end if
    next
    FKASSERT( bSame, "staging copy does not match gKeys after LoadWork" )

    ' ---- TEST B: THE CANCEL GUARANTEE. Editing the staging copy must not reach gKeys. ----
    ' If gKeysWork ever became an alias rather than a copy, this is the assertion that says
    ' so -- and it is the only one that can, because every other symptom of that mistake
    ' looks exactly like the dialog working.
    if ubound(gKeys) >= lbound(gKeys) then
        dim as DWSTRING wszKeepUser = gKeys(0).wszUserKeys
        dim as boolean   bKeepFlag   = gKeys(0).bDefaultDisabled

        gKeysWork(0).wszUserKeys     = "Ctrl+Shift+Alt+F12"
        gKeysWork(0).bDefaultDisabled = not bKeepFlag

        FKASSERT( gKeys(0).wszUserKeys = wszKeepUser, _
                  "editing gKeysWork changed gKeys().wszUserKeys -- Cancel would not revert" )
        FKASSERT( gKeys(0).bDefaultDisabled = bKeepFlag, _
                  "editing gKeysWork changed gKeys().bDefaultDisabled -- Cancel would not revert" )

        ' Put the staging copy back so nothing downstream sees the probe's edit.
        frmKeyboard_LoadWork()
        FKASSERT( gKeysWork(0).wszUserKeys = wszKeepUser, "LoadWork did not restore the probed row" )
    end if

    ' ---- TEST D: every localized string this dialog uses actually EXISTS. ----
    '
    ' L(id, "default") DISCARDS the default -- it is #Define L(e,s) LL(e), so the second
    ' argument is documentation and nothing more. An id with no entry in english.lang renders
    ' as an EMPTY STRING: a blank column header, a blank button, a blank cue banner, with
    ' nothing in the build, the warnings or any other assertion to say so.
    '
    ' This is not hypothetical here. The first render of this dialog came up with three of
    ' its four column headers blank, the Reset button blank and the filter's cue banner
    ' blank, because ids 336-348 had not been added yet. Everything compiled and every other
    ' assertion passed.
    scope
        dim as long nBlank = 0
        for id as long = 336 to 348
            if len(LL(id)) = 0 then
                nBlank += 1
                sLine = "  FAIL: localized string " & str(id) & " is empty -- it will render blank"
                ? sLine
            end if
        next
        nFail += nBlank
        nPass += (13 - nBlank)

        ' The ids borrowed from elsewhere in the file. If one of these ever moves, this
        ' dialog goes partly blank and nothing else would notice.
        FKASSERT( len(LL(0))   > 0, "localized string 0 (OK) is empty" )
        FKASSERT( len(LL(1))   > 0, "localized string 1 (Cancel) is empty" )
        FKASSERT( len(LL(183)) > 0, "localized string 220 (Keyboard Shortcuts) is empty" )
        FKASSERT( len(LL(205)) > 0, "localized string 255 (Description) is empty" )
        FKASSERT( len(LL(282)) > 0, "localized string 433 (Modify) is empty" )
    end scope

    ' ---- TEST C: the on-disk format round-trips, INCLUDING the disabled flag. ----
    ' Serialises the live bindings to a temp file and reads them back with the same parse
    ' the loader uses. (DISABLED) is the field to watch: it is smuggled into the message
    ' string rather than given a column of its own, so it is the one a naive writer drops.
    scope
        dim as DWSTRING wszTmp = environ("TEMP") & "\tiko_keybind_selftest.tmp"
        frmKeyboard_SaveKeyBindings( wszTmp )

        dim as long nRead = 0, nMatched = 0
        dim pStream as CTextStream
        if pStream.Open( wszTmp ) = S_OK then
            do until pStream.EOS
                dim as DWSTRING wst = trim(pStream.ReadLine)
                if len(wst) = 0 then continue do
                if instr( wst, ":" ) = 0 then continue do
                nRead += 1

                dim as DWSTRING wszMsg = AfxStrParse(wst, 1, ":")
                dim as boolean bDisabled = false
                dim as long p = instr(wszMsg, "(DISABLED)")
                if p then
                    bDisabled = true
                    wszMsg = left(wszMsg, p - 1)
                end if
                dim as DWSTRING wszUser = trim(AfxStrParse(wst, 2, ":"))

                for i as long = lbound(gKeys) to ubound(gKeys)
                    if gKeys(i).wszMsgString = wszMsg then
                        if (gKeys(i).wszUserKeys = wszUser) andalso _
                           (gKeys(i).bDefaultDisabled = bDisabled) then nMatched += 1
                        exit for
                    end if
                next
            loop
            pStream.Close
        end if
        kill wszTmp

        dim as long nExpected = ubound(gKeys) - lbound(gKeys) + 1
        FKASSERT( nRead = nExpected, _
                  "saved file has " & str(nRead) & " entries, expected " & str(nExpected) )
        FKASSERT( nMatched = nExpected, _
                  "only " & str(nMatched) & " of " & str(nExpected) & " entries survived the file round trip" )
    end scope

    erase gKeysWork

    sLine = "--- TIKO_KEYBOARD_SELFTEST (staging): " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine
    ? ""

end sub


' ========================================================================================
' THE EFFECTIVE BINDING of a staged command: what would actually reach the accelerator
' table. User keys win; otherwise the default, unless the default has been suppressed.
' Empty means this command has no shortcut at all.
' ========================================================================================
private function frmKeyboard_EffectiveKeys( byval idx as long ) as DWSTRING
    if (idx < lbound(gKeysWork)) orelse (idx > ubound(gKeysWork)) then return ""
    if len(gKeysWork(idx).wszUserKeys) then return gKeysWork(idx).wszUserKeys
    if gKeysWork(idx).bDefaultDisabled then return ""
    return gKeysWork(idx).wszDefaultKeys
end function


' ========================================================================================
' Describe the first command that already owns wszKeys, skipping nSkipIndex. "" = no clash.
' ========================================================================================
function frmKeyboard_CheckForKeyConflict ( _
            byval wszKeys as DWSTRING, _
            byval nSkipIndex as long _
            ) as DWSTRING

    if len(wszKeys) = 0 then return ""

    for i as long = lbound(gKeysWork) to ubound(gKeysWork)
        if i = nSkipIndex then continue for
        if wszKeys = frmKeyboard_EffectiveKeys(i) then
            return L(345,"Conflict") & ": " & gKeysWork(i).wszDescription & " (" & wszKeys & ")"
        end if
    next

    return ""
end function


' ========================================================================================
' Recompute the whole conflict map. Called after every edit, and once at build.
'
' O(n^2) over ~104 commands is about 5000 string compares, on a user action. Not worth an
' index; a map keyed by binding string would have to be invalidated on every edit anyway.
' ========================================================================================
sub frmKeyboard_RescanConflicts()

    erase gKeysConflict
    if ubound(gKeysWork) < lbound(gKeysWork) then exit sub
    redim gKeysConflict( ubound(gKeysWork) ) as boolean

    for i as long = lbound(gKeysWork) to ubound(gKeysWork)
        dim as DWSTRING wszI = frmKeyboard_EffectiveKeys(i)
        if len(wszI) = 0 then continue for
        for j as long = i + 1 to ubound(gKeysWork)
            if wszI = frmKeyboard_EffectiveKeys(j) then
                gKeysConflict(i) = true
                gKeysConflict(j) = true
            end if
        next
    next

end sub


' ========================================================================================
' What the Default Keys cell SHOWS. Composed from the model at every use rather than stored
' in the cell, so the painter and the filter cannot end up describing the row differently.
' ========================================================================================
function frmKeyboard_DefaultKeysDisplay( byval idx as long ) as DWSTRING
    if (idx < lbound(gKeysWork)) orelse (idx > ubound(gKeysWork)) then return ""
    dim as DWSTRING wsz = gKeysWork(idx).wszDefaultKeys
    if gKeysWork(idx).bDefaultDisabled then
        if len(wsz) = 0 then
            wsz = L(339,"(disabled)")
        else
            wsz = wsz & " " & L(339,"(disabled)")
        end if
    end if
    return wsz
end function


' ========================================================================================
' Does this row survive the filter? Matches across ALL FOUR columns, case-insensitively, so
' "ctrl+f" answers "what is that bound to?" and "bookmark" finds the whole family.
' ========================================================================================
private function frmKeyboard_RowMatchesFilter( byval idx as long ) as boolean
    if len(gKeysFilter) = 0 then return true

    dim as DWSTRING wszNeedle = ucase(gKeysFilter)
    if instr( ucase(gKeysWork(idx).wszCategory), wszNeedle ) then return true
    if instr( ucase(gKeysWork(idx).wszDescription), wszNeedle ) then return true
    if instr( ucase(frmKeyboard_DefaultKeysDisplay(idx)), wszNeedle ) then return true
    if instr( ucase(gKeysWork(idx).wszUserKeys), wszNeedle ) then return true

    return false
end function


' ========================================================================================
' The gKeysWork() index behind the highlighted row, or -1.
'
' ALWAYS VIA itemData, NEVER via the row index. The filter rebuilds the list, so visible row
' 3 is not gKeysWork(3) -- that equivalence held only in the old dialog, which never filtered
' and stashed the index in a hidden text column to recover it.
' ========================================================================================
private function frmKeyboard_SelectedWorkIndex() as long
    if HWND_FRMKEYBOARD_LIST = 0 then return -1
    dim as long nRow = PsListTree_GetCurSel( HWND_FRMKEYBOARD_LIST )
    if nRow < 0 then return -1
    dim as long idx = PsListTree_GetItemData( HWND_FRMKEYBOARD_LIST, nRow )
    if (idx < lbound(gKeysWork)) orelse (idx > ubound(gKeysWork)) then return -1
    return idx
end function


' ========================================================================================
' Row painter.
'
' NOT the shared ListBox_LVPaintRow in modRoutines.inc: that one draws in the ghOutput
' palette for the Output panel's three lists, and this dialog is dressed like frmOptions.
' It also needs two states the shared painter has no concept of -- a dimmed disabled default
' and a conflict-marked user binding.
' ========================================================================================
private sub frmKeyboard_ListPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as boolean bActive = (p->isSelected orelse p->isHot)
    dim as COLORREF backclr = iif( bActive, theme.panel.texthot.backcolor, theme.panel.text.backcolor )
    dim as COLORREF foreclr = iif( bActive, theme.panel.texthot.forecolor, theme.panel.text.forecolor )

    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_11) )

    dim as long idx = PsListTree_GetItemData( HWND_FRMKEYBOARD_LIST, p->itemID )
    if (idx < lbound(gKeysWork)) orelse (idx > ubound(gKeysWork)) then exit sub
    if p->columnCount <= 0 then exit sub

    dim as boolean bConflict = false
    if (idx >= lbound(gKeysConflict)) andalso (idx <= ubound(gKeysConflict)) then
        bConflict = gKeysConflict(idx)
    end if
    ' Hoisted into a boolean of its own: len() yields a long, and andalso-ing that with a
    ' boolean is fbc warning 38.
    dim as boolean bHasUserKeys = (len(gKeysWork(idx).wszUserKeys) > 0)

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(6)

        dim as DWSTRING wszCell = p->cells[i].wszText
        dim as COLORREF clr = foreclr

        select case i
            case 2
                ' Default Keys. The "(disabled)" suffix is added HERE, and the whole cell
                ' goes dim -- state readable both as text and as weight, so it survives a
                ' greyscale screenshot and a colour-blind reader alike.
                wszCell = frmKeyboard_DefaultKeysDisplay( idx )
                if gKeysWork(idx).bDefaultDisabled then clr = theme.topmenu.textdisabled.forecolor
                ' A row whose clash comes from its DEFAULT has no user cell to mark, so the
                ' marker goes here instead. Checked before the disabled dimming above would
                ' otherwise win, because a disabled default cannot conflict with anything.
                if bConflict andalso (bHasUserKeys = false) then clr = theme.compile.iconfail

            case 3
                ' User Keys. A conflict is marked on the cell the user actually edits.
                if bConflict andalso bHasUserKeys then clr = theme.compile.iconfail
        end select

        p->b->SetForeColor( clr )
        p->b->PaintText( wszCell, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next

end sub


' ========================================================================================
' Header band painter. REQUIRED -- without one the header does not render at all.
' ========================================================================================
private sub frmKeyboard_ListPaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr = theme.panel.text.backcolor
    if p->isHot orelse p->isResizeHot then backclr = theme.panel.texthot.backcolor

    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(6)
    p->b->SetForeColor( theme.panel.texthot.forecolor )
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    ' Hairline on the trailing edge so columns read as separated.
    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.ui.divider )
    p->b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' (Re)fill the list from the staging copy, honouring the filter.
'
' Selection is preserved BY gKeysWork() INDEX, not by row: a filter keystroke can move a row
' from position 40 to position 0, and restoring "row 40" would land on something unrelated.
' ========================================================================================
private sub frmKeyboard_BuildList()

    if HWND_FRMKEYBOARD_LIST = 0 then exit sub

    dim as long nKeepIdx = frmKeyboard_SelectedWorkIndex()

    PsListTree_Clear( HWND_FRMKEYBOARD_LIST )

    dim as long nSelectRow = -1
    for i as long = lbound(gKeysWork) to ubound(gKeysWork)
        if frmKeyboard_RowMatchesFilter(i) = false then continue for

        ' Column 0 is the row's own text; the itemData carries the model index.
        dim as long nRow = PsListTree_AddString( HWND_FRMKEYBOARD_LIST, gKeysWork(i).wszCategory, i )
        if nRow < 0 then continue for

        PsListTree_SetCellText( HWND_FRMKEYBOARD_LIST, nRow, 1, gKeysWork(i).wszDescription )
        ' Cell 2 holds the RAW keys; the painter adds the "(disabled)" suffix. Storing the
        ' decorated string here would leak a display concern into the model's mirror.
        PsListTree_SetCellText( HWND_FRMKEYBOARD_LIST, nRow, 2, gKeysWork(i).wszDefaultKeys )
        PsListTree_SetCellText( HWND_FRMKEYBOARD_LIST, nRow, 3, gKeysWork(i).wszUserKeys )

        if i = nKeepIdx then nSelectRow = nRow
    next

    if (nSelectRow < 0) andalso (PsListTree_GetCount( HWND_FRMKEYBOARD_LIST ) > 0) then nSelectRow = 0
    if nSelectRow >= 0 then PsListTree_SetCurSel( HWND_FRMKEYBOARD_LIST, nSelectRow )

    ' ONLY the list. The dialog's own chrome used to be redrawn here too, because the title
    ' band showed a match count that every rebuild changed. With the count gone the band holds
    ' nothing a filter keystroke can affect, so repainting the whole dialog on each one would
    ' be work with no result to show for it.
    AfxRedrawWindow( HWND_FRMKEYBOARD_LIST )

end sub


' ========================================================================================
' Refresh one row's cells after an edit, without rebuilding the whole list.
' ========================================================================================
private sub frmKeyboard_RefreshRow( byval nRow as long )
    if (HWND_FRMKEYBOARD_LIST = 0) orelse (nRow < 0) then exit sub
    dim as long idx = PsListTree_GetItemData( HWND_FRMKEYBOARD_LIST, nRow )
    if (idx < lbound(gKeysWork)) orelse (idx > ubound(gKeysWork)) then exit sub

    PsListTree_SetCellText( HWND_FRMKEYBOARD_LIST, nRow, 2, gKeysWork(idx).wszDefaultKeys )
    PsListTree_SetCellText( HWND_FRMKEYBOARD_LIST, nRow, 3, gKeysWork(idx).wszUserKeys )

    frmKeyboard_RescanConflicts()
    AfxRedrawWindow( HWND_FRMKEYBOARD_LIST )
end sub


' ========================================================================================
' Open the Modify dialog on the highlighted row.
' ========================================================================================
private sub frmKeyboard_DoModify()

    dim as long nRow = PsListTree_GetCurSel( HWND_FRMKEYBOARD_LIST )
    dim as long idx  = frmKeyboard_SelectedWorkIndex()
    if idx < 0 then exit sub

    gKeysEdit = gKeysWork(idx)
    frmKeyboardEdit_Show( HWND_FRMKEYBOARD )
    gKeysWork(idx) = gKeysEdit

    frmKeyboard_RefreshRow( nRow )
    SetFocus( HWND_FRMKEYBOARD_LIST )

end sub


' ========================================================================================
' Reset the highlighted row to its shipped default.
'
' Clears the user keys AND re-enables the default. Clearing only the keys would leave a row
' whose default is still suppressed -- no shortcut at all, and a Default Keys cell still
' reading "(disabled)", which looks exactly like the button having failed.
' ========================================================================================
private sub frmKeyboard_DoReset()

    dim as long nRow = PsListTree_GetCurSel( HWND_FRMKEYBOARD_LIST )
    dim as long idx  = frmKeyboard_SelectedWorkIndex()
    if idx < 0 then exit sub

    gKeysWork(idx).wszUserKeys      = ""
    gKeysWork(idx).bDefaultDisabled = false

    frmKeyboard_RefreshRow( nRow )
    SetFocus( HWND_FRMKEYBOARD_LIST )

end sub


' ========================================================================================
' Control callbacks
' ========================================================================================

' PsButton does not post WM_COMMAND to its parent, so all four buttons arrive here.
private sub frmKeyboard_ButtonClickCallback( byval hButton as HWND, byval id as long )
    select case id
        case IDC_FRMKEYBOARD_CMDMODIFY:  frmKeyboard_DoModify()
        case IDC_FRMKEYBOARD_CMDCLEAR:   frmKeyboard_DoReset()
        case IDC_FRMKEYBOARD_CMDOK
            frmKeyboard_CommitWork()
            SendMessage( HWND_FRMKEYBOARD, WM_CLOSE, 0, 0 )
        case IDC_FRMKEYBOARD_CMDCANCEL
            SendMessage( HWND_FRMKEYBOARD, WM_CLOSE, 0, 0 )
    end select
end sub

' Double-click, or ENTER on the focused row -- PsListTree treats them as one gesture, and
' setting this callback is ALSO what makes the control claim ENTER (per-message
' DLGC_WANTALLKEYS plus the WM_CHAR swallow that stops the system beeping on every press).
' Without it, Enter would reach IsDialogMessage and fire the default OK button instead.
private sub frmKeyboard_ListActivateCallback( byval hList as HWND, byval row as integer )
    frmKeyboard_DoModify()
end sub

' NO TOOLTIPS ON THE LIST.
'
' This has to be said explicitly: with NO tooltip callback set, PsListTree falls back to
' showing the row's OWN TEXT. Every row here is fully visible in its column already, so the
' fallback would pop a tip repeating what the cursor is sitting on. Returning "" is the
' documented way to suppress them (the same thing tiko's autocomplete popup does).
private function frmKeyboard_ListTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    return ""
end function

' ...and the same for the COLUMN HEADER, which has its own callback and its own identical
' fallback: unset, PsColumnHeader shows the column's own Text, so hovering "User Keys" pops a
' tip reading "User Keys". Suppressing the rows does nothing for the header band -- these are
' two separate opt-outs and it is easy to fix one and leave the other.
private function frmKeyboard_HeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function

private sub frmKeyboard_FilterChanged( byval hTextBox as HWND )
    gKeysFilter = PsTextBox_GetText( hTextBox )
    frmKeyboard_BuildList()
end sub


' ========================================================================================
' Paint the dialog's own chrome: background, title text, and the filter box's frame.
' Everything else on screen belongs to a child control.
' ========================================================================================
private function frmKeyboard_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( theme.panel.text.backcolor )
    buffer.PaintRect( @rc )

    dim as long titleH = AfxScaleY(FRMKEYBOARD_TITLE_H)
    dim as long mx     = AfxScaleX(FRMKEYBOARD_MARGIN)

    ' THE FILTER FRAME. One rounded rect around the glyph AND the box, filled in the field
    ' colour so the borderless PsTextBox (same colour) sits inside it invisibly, then the
    ' magnifying glass on the fill, then the border last so it stays crisp. Copied from the
    ' Options dialog deliberately -- the two dialogs should not differ here by accident.
    dim as long filterH = AfxScaleY(FRMKEYBOARD_FILTER_H)
    dim as long filterY = (titleH - filterH) \ 2
    dim as long curve   = AfxScaleY(8)

    dim as RECT rcFrame
    rcFrame.left   = mx
    rcFrame.top    = filterY + AfxScaleY(2)
    rcFrame.right  = mx + AfxScaleX(FRMKEYBOARD_FILTER_W)
    rcFrame.bottom = filterY + filterH

    buffer.SetBackColor( theme.ui.field.backcolor )
    buffer.PaintRoundRect( @rcFrame, curve )

    dim as RECT rcGlyph
    rcGlyph.left   = mx
    rcGlyph.top    = filterY
    rcGlyph.right  = mx + AfxScaleX(FRMKEYBOARD_FILTER_GLYPHW)
    rcGlyph.bottom = filterY + filterH
    buffer.SetForeColor( theme.panel.text.forecolor )
    buffer.SetBackColor( theme.ui.field.backcolor )
    buffer.SetFont( ghFont(SYMBOLFONT_10) )
    buffer.PaintText( wchr(&hE721), @rcGlyph, DT_CENTER or DT_VCENTER or DT_SINGLELINE )

    buffer.SetPenColor( theme.ui.divider )
    buffer.PaintRoundOutline( @rcFrame, curve, 1 )

    ' NOTHING ELSE IS DRAWN IN THIS BAND. It briefly carried the page title, which was just
    ' the window caption repeated an inch below itself, and then a match count -- both removed
    ' on request. The Options dialog puts a page title here because there the caption and the
    ' band genuinely say different things; this dialog has one page and needs neither.
    '
    ' (The count also had a "15 / 104" form while filtering. That went with it. Restoring only
    ' that variant would be a few lines here, gated on len(gKeysFilter).)

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Position the shell's children. The dialog is fixed-size, so this runs once after creation.
' ========================================================================================
private sub frmKeyboard_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long titleH  = AfxScaleY(FRMKEYBOARD_TITLE_H)
    dim as long footerH = AfxScaleY(FRMKEYBOARD_FOOTER_H)
    dim as long mx      = AfxScaleX(FRMKEYBOARD_MARGIN)
    dim as long btnW    = AfxScaleX(FRMKEYBOARD_BTN_W)
    dim as long btnH    = AfxScaleY(FRMKEYBOARD_BTN_H)
    dim as long gap     = AfxScaleX(8)

    ' Filter box: inside the frame the dialog paints, past the glyph gutter on the left and
    ' inset by the border on the other three sides so it never paints over the frame.
    dim as long filterH = AfxScaleY(FRMKEYBOARD_FILTER_H)
    dim as long filterY = (titleH - filterH) \ 2
    dim as long inset   = AfxScaleX(2)
    dim as long boxX    = mx + AfxScaleX(FRMKEYBOARD_FILTER_GLYPHW)
    dim as long boxW    = (mx + AfxScaleX(FRMKEYBOARD_FILTER_W) - inset) - boxX
    if boxW < 0 then boxW = 0
    SetWindowPos( HWND_FRMKEYBOARD_FILTER, 0, boxX, filterY + inset + AfxScaleY(2), _
                  boxW, filterH - (2 * inset) - AfxScaleY(2), SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long listY = titleH
    dim as long listH = rc.bottom - footerH - titleH
    if listH < 0 then listH = 0
    SetWindowPos( HWND_FRMKEYBOARD_LIST, 0, mx, listY, rc.right - (mx * 2), listH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long btnY = rc.bottom - footerH + (footerH - btnH) \ 2

    ' Modify / Reset on the left, OK / Cancel on the right.
    SetWindowPos( GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDMODIFY), 0, _
                  mx, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDCLEAR), 0, _
                  mx + btnW + gap, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDCANCEL), 0, _
                  rc.right - mx - btnW, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDOK), 0, _
                  rc.right - mx - (btnW * 2) - gap, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Push the current theme into every child that carries its own colours.
' ========================================================================================
private sub frmKeyboard_ApplyTheme( byval hwnd as HWND )

    PsListTree_SetBackColor( HWND_FRMKEYBOARD_LIST, theme.panel.text.backcolor )
    PsListTree_SetScrollBarColors( HWND_FRMKEYBOARD_LIST, theme.panel.scrollbar.backcolor, _
                                   theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    ' The frame's colour lives here, not beside SetBorderWidth at creation, so it follows a
    ' theme change. Same key the search box's drawn frame uses.
    PsListTree_SetBorderColor( HWND_FRMKEYBOARD_LIST, theme.ui.divider )

    if HWND_FRMKEYBOARD_FILTER then
        PsTextBox_SetBackColor( HWND_FRMKEYBOARD_FILTER, theme.ui.field.backcolor )
        PsTextBox_SetOuterBackColor( HWND_FRMKEYBOARD_FILTER, theme.ui.field.backcolor )
        PsTextBox_SetForeColor( HWND_FRMKEYBOARD_FILTER, theme.ui.field.forecolor )
        PsTextBox_SetCueBannerColor( HWND_FRMKEYBOARD_FILTER, theme.panel.text.forecolor )
    end if

    ' Same recipe as the Options dialog's OptionsTheme_FillButton, spelled out here rather
    ' than called: that helper lives in modOptionsRows.inc, which the unity build includes
    ' AFTER this file. Keep the two in step by eye -- a shared helper would have to move to
    ' a module both can see, and that is a bigger change than this dialog should make.
    dim as PSBUTTON_COLORS bc
    bc.BackColor           = theme.panel.button.backcolor
    bc.ForeColor           = theme.panel.button.forecolor
    bc.BorderColor         = theme.ui.divider
    bc.IconColor           = theme.panel.button.forecolor
    bc.BackColorHot        = theme.panel.buttonhot.backcolor
    bc.ForeColorHot        = theme.panel.buttonhot.forecolor
    bc.BorderColorHot      = theme.panel.buttonhot.backcolor
    bc.IconColorHot        = theme.panel.buttonhot.forecolor
    bc.BackColorPressed    = theme.panel.buttonhot.backcolor
    bc.ForeColorPressed    = theme.panel.buttonhot.forecolor
    bc.BorderColorPressed  = theme.panel.buttonhot.backcolor
    bc.IconColorPressed    = theme.panel.buttonhot.forecolor
    bc.BackColorDisabled   = theme.panel.text.backcolor
    bc.ForeColorDisabled   = theme.ui.divider
    bc.BorderColorDisabled = theme.ui.divider
    bc.IconColorDisabled   = theme.ui.divider

    dim as HWND hBtn(3) = { GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDMODIFY), _
                            GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDCLEAR), _
                            GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDOK), _
                            GetDlgItem(hwnd, IDC_FRMKEYBOARD_CMDCANCEL) }
    for i as long = 0 to 3
        if hBtn(i) = 0 then continue for
        PsButton_SetColors( hBtn(i), @bc )
        PsButton_Refresh( hBtn(i) )
    next

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' Is hCandidate hContainer, or anything nested inside it?
'
' Needed because every focusable thing on this dialog is a CONTAINER whose real tab stop is
' an inner child -- see the tab-order assertions below.
' ========================================================================================
private function frmKeyboard_IsWithin( byval hCandidate as HWND, byval hContainer as HWND ) as boolean
    if (hCandidate = 0) orelse (hContainer = 0) then return false
    dim as HWND h = hCandidate
    do while h <> 0
        if h = hContainer then return true
        h = GetParent( h )
    loop
    return false
end function


' ========================================================================================
' DOES THE LIST FIT, AND DOES THE FILTER WORK?              (TIKO_KEYBOARD_SELFTEST=1)
'
' Runs from frmKeyboard_Show once the controls exist and the list has been filled, so it
' measures the real windows at the real DPI rather than the unscaled constants.
'
' The fit half exists for the same reason the Options dialog's does: this dialog is FIXED
' SIZE, so a column set wider than the viewport does not grow a horizontal scrollbar -- the
' User Keys column simply falls off the right edge, silently, and only above 100% DPI where
' nobody is looking.
' ========================================================================================
private sub frmKeyboard_RunLayoutSelfTest()

    if environ("TIKO_KEYBOARD_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro LAYASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? ""
    ? "--- TIKO_KEYBOARD_SELFTEST (layout + filter) ---"

    dim as RECT rcList
    GetClientRect( HWND_FRMKEYBOARD_LIST, @rcList )

    dim as long nCols = AfxScaleX(FRMKEYBOARD_COL_CATEGORY) + _
                        AfxScaleX(FRMKEYBOARD_COL_DESCRIPTION) + _
                        AfxScaleX(FRMKEYBOARD_COL_DEFAULTKEYS) + _
                        AfxScaleX(FRMKEYBOARD_COL_USERKEYS)

    sLine = "  list viewport " & str(rcList.right) & " x " & str(rcList.bottom) & _
            " px, columns want " & str(nCols) & " px at this DPI"
    ? sLine

    LAYASSERT( rcList.right > 0 andalso rcList.bottom > 0, "the list has no client area" )
    ' Slack for the vertical scrollbar, which this list always needs at ~104 rows.
    LAYASSERT( nCols <= rcList.right - AfxScaleX(20), _
               "the four columns are wider than the list viewport -- User Keys will clip" )

    ' The footer must not eat the list: four buttons plus their gaps and margins.
    dim as RECT rcClient
    GetClientRect( HWND_FRMKEYBOARD, @rcClient )
    dim as long nBtnRun = (AfxScaleX(FRMKEYBOARD_BTN_W) * 4) + (AfxScaleX(8) * 2) + _
                          (AfxScaleX(FRMKEYBOARD_MARGIN) * 2)
    LAYASSERT( nBtnRun <= rcClient.right, _
               "the four footer buttons are wider than the dialog -- they would overlap" )

    ' The filter frame must not run into the title.
    LAYASSERT( AfxScaleX(FRMKEYBOARD_MARGIN) + AfxScaleX(FRMKEYBOARD_FILTER_W) < rcClient.right \ 2, _
               "the filter box takes more than half the title band" )

    ' ---- TAB ORDER: search box first, then the list. ----
    '
    ' MEASURED THROUGH THE DIALOG MANAGER, not inferred from the creation order that produces
    ' it. These controls sit on an ordinary CWindow, so the tab order IS the child Z-order and
    ' the only thing setting it is which control was created first -- a property with no
    ' syntax of its own, invisible at the point where someone would break it by moving a
    ' block. GetNextDlgTabItem is what the Tab key itself consults, so asking it is the only
    ' check that cannot disagree with the real behaviour.
    '
    ' COMPARE BY CONTAINMENT, NEVER BY HANDLE EQUALITY. Both the search box and the list are
    ' CONTAINERS -- a PsTextBox is a shell around a RichEdit, a PsListTree a shell around the
    ' surface it paints -- and both carry WS_EX_CONTROLPARENT, so the dialog manager descends
    ' into them and hands back the INNER child. Neither GetNextDlgTabItem nor GetFocus will
    ' ever return the handle this file holds. The first version of these three assertions
    ' compared handles directly and failed against a dialog that was behaving perfectly.
    ' (Same trap as PsNumericUpDown, which ships PsNumericUpDown_HasFocus for exactly this.)
    scope
        dim as HWND hFirst  = GetNextDlgTabItem( HWND_FRMKEYBOARD, 0, false )
        dim as HWND hSecond = GetNextDlgTabItem( HWND_FRMKEYBOARD, hFirst, false )

        LAYASSERT( frmKeyboard_IsWithin( hFirst,  HWND_FRMKEYBOARD_FILTER ), _
                   "the first tab stop is not the search box" )
        LAYASSERT( frmKeyboard_IsWithin( hSecond, HWND_FRMKEYBOARD_LIST ), _
                   "the second tab stop is not the list" )
        LAYASSERT( hFirst <> hSecond, "the first two tab stops are the same window" )

        ' And the dialog OPENS on the search box. Focus is set explicitly because
        ' IsDialogMessage will not Tab anywhere until some child already holds it.
        LAYASSERT( frmKeyboard_IsWithin( GetFocus(), HWND_FRMKEYBOARD_FILTER ), _
                   "the dialog did not open with focus in the search box" )
    end scope

    ' ---- THE FILTER. Counts, against a needle each column can be asked about. ----
    ' Not vacuous: each case names a column the match could ONLY have come from, so a filter
    ' that quietly searched just the description would fail three of the four.
    dim as long nAll = PsListTree_GetCount( HWND_FRMKEYBOARD_LIST )
    LAYASSERT( nAll = (ubound(gKeysWork) - lbound(gKeysWork) + 1), _
               "an empty filter does not show every row" )

    #macro FILTERCOUNT( _needle_, _out_ )
        gKeysFilter = _needle_
        frmKeyboard_BuildList()
        _out_ = PsListTree_GetCount( HWND_FRMKEYBOARD_LIST )
    #endmacro

    ' Ground truth for ONE column, computed straight from the model. Comparing the filter's
    ' answer against this is what makes the per-column cases mean anything.
    '
    ' THE FIRST VERSION OF THESE ASSERTED ONLY "0 < hits < all" AND WAS WRONG. An A/B that
    ' cut frmKeyboard_RowMatchesFilter down to the description column alone left the CATEGORY
    ' case GREEN -- because every one of tiko's eight category names also appears in some
    ' command's description ("Debug" is the Debug category, and it is also in "Start
    ' Debugging"). The assertion was named for the category column and was in fact satisfied
    ' by the description column. There is no needle that avoids this for any category, so the
    ' fix is to compare COUNTS: all-columns "Debug" matches the 8 Debug commands, while
    ' description-only matches 2.
    #macro COUNTIN( _field_, _needle_, _out_ )
        _out_ = 0
        for i as long = lbound(gKeysWork) to ubound(gKeysWork)
            if instr( ucase(_field_), ucase(_needle_) ) then _out_ += 1
        next
    #endmacro

    dim as long nHits = 0, nWant = 0

    ' CATEGORY.
    COUNTIN( gKeysWork(i).wszCategory, "Debug", nWant )
    FILTERCOUNT( "Debug", nHits )
    LAYASSERT( nWant > 0, "the CATEGORY probe needle matches nothing -- the test is vacuous" )
    LAYASSERT( nHits = nWant, _
               "filtering on a CATEGORY matched " & str(nHits) & " rows, expected " & str(nWant) )

    ' DESCRIPTION.
    COUNTIN( gKeysWork(i).wszDescription, "bookmark", nWant )
    FILTERCOUNT( "bookmark", nHits )
    LAYASSERT( nWant > 0, "the DESCRIPTION probe needle matches nothing -- the test is vacuous" )
    LAYASSERT( nHits = nWant, _
               "filtering on a DESCRIPTION matched " & str(nHits) & " rows, expected " & str(nWant) )

    ' DEFAULT KEYS.
    COUNTIN( gKeysWork(i).wszDefaultKeys, "Ctrl+Shift+F", nWant )
    FILTERCOUNT( "Ctrl+Shift+F", nHits )
    LAYASSERT( nWant > 0, "the DEFAULT KEYS probe needle matches nothing -- the test is vacuous" )
    LAYASSERT( nHits = nWant, _
               "filtering on DEFAULT KEYS matched " & str(nHits) & " rows, expected " & str(nWant) )

    ' USER KEYS. Nothing has a user binding by default, so one is seeded, filtered on, and
    ' removed again -- otherwise the fourth column is the one the suite never exercises.
    scope
        dim as DWSTRING wszKeep = gKeysWork(0).wszUserKeys
        gKeysWork(0).wszUserKeys = "Ctrl+Alt+Shift+F11"
        FILTERCOUNT( "Alt+Shift+F11", nHits )
        LAYASSERT( nHits = 1, "filtering on USER KEYS matched " & str(nHits) & " rows, expected 1" )
        gKeysWork(0).wszUserKeys = wszKeep
    end scope

    ' Case-insensitive, and the same needle in the other case must give the same count.
    dim as long nUpper = 0, nLower = 0
    FILTERCOUNT( "BOOKMARK", nUpper )
    FILTERCOUNT( "bookmark", nLower )
    LAYASSERT( nUpper = nLower, "the filter is case sensitive" )

    ' A needle nothing can match empties the list rather than falling back to everything.
    FILTERCOUNT( "zzzznotakeybinding", nHits )
    LAYASSERT( nHits = 0, "an unmatchable filter left " & str(nHits) & " rows showing" )

    ' Restore.
    gKeysFilter = ""
    frmKeyboard_BuildList()
    LAYASSERT( PsListTree_GetCount( HWND_FRMKEYBOARD_LIST ) = nAll, _
               "clearing the filter did not restore every row" )

    sLine = "--- TIKO_KEYBOARD_SELFTEST (layout): " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine
    ? ""

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmKeyboard_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

        case WM_CREATE
            ' Modal: keep the rest of the app out while this is up.
            EnableWindow(GetParent(hwnd), false)
            return 0

        case WM_ERASEBKGND
            ' The whole client is painted in WM_PAINT through one buffer, so letting the
            ' system erase first would only flicker.
            return 1

        case WM_PAINT
            return frmKeyboard_OnPaint( hwnd )

        case WM_SIZE
            frmKeyboard_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes through its click callback, not WM_COMMAND. What still arrives
            ' here is IDCANCEL, which IsDialogMessage synthesises for the Escape key.
            select case loword(wParam)
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case MSG_USER_SHOW_KEYBOARDEDIT
            frmKeyboard_DoModify()
            return 0

        case WM_CLOSE
            ' NOTHING IS SAVED HERE. Closing used to mean saving, which is why the dialog had
            ' no way to abandon an edit; the commit is on the OK path only. Reaching this by
            ' any route discards gKeysWork() and leaves gKeys(), keybindings.ini and the
            ' accelerator table exactly as they were found.
            ' Enables parent window keeping parent's zorder
            EnableWindow(GetParent(hwnd), true)
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMKEYBOARD        = 0
            HWND_FRMKEYBOARD_LIST   = 0
            HWND_FRMKEYBOARD_FILTER = 0
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmKeyboard_Show
' ========================================================================================
function frmKeyboard_Show( byval hWndParent as HWND ) as LRESULT

    ' Seed the staging copy BEFORE any control exists -- everything below is built from
    ' gKeysWork(), and gKeys() is not touched again unless OK commits.
    frmKeyboard_LoadWork()
    frmKeyboard_RescanConflicts()
    gKeysFilter = ""


    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMKEYBOARD = pWindow->Create( hWndParent, L(183,"Keyboard Shortcuts"), _
        @frmKeyboard_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMKEYBOARD_CLIENT_W, FRMKEYBOARD_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the filter box, CREATED FIRST ------------------------------------------------
    '
    ' CREATION ORDER IS THE TAB ORDER. These controls live on an ordinary CWindow, not a
    ' dialog template, so there is no ordinal to set: GetNextDlgTabItem walks the child
    ' Z-ORDER, and Z-order is creation order. The filter box is therefore built before the
    ' list so that Tab goes search -> list -> Modify -> Reset -> OK -> Cancel. Moving either
    ' block past the other silently reorders the dialog.
    '
    ' NO BORDER and NO CORNER RADIUS of its own: the dialog paints one rounded frame around
    ' the magnifying glyph AND this box together, so the glyph reads as being inside the
    ' field. A child window paints over anything drawn under it, so the box is inset within
    ' that frame rather than straddling it.
    HWND_FRMKEYBOARD_FILTER = PsTextBox_Create( HWND_FRMKEYBOARD, IDC_FRMKEYBOARD_FILTER )
    PsTextBox_SetFont( HWND_FRMKEYBOARD_FILTER, ghFont(GUIFONT_11) )
    PsTextBox_SetBorderWidth( HWND_FRMKEYBOARD_FILTER, 0 )
    PsTextBox_SetCornerRadius( HWND_FRMKEYBOARD_FILTER, 0 )
    PsTextBox_SetCueBannerText( HWND_FRMKEYBOARD_FILTER, L(340,"Filter shortcuts...") )
    PsTextBox_SetChangeCallback( HWND_FRMKEYBOARD_FILTER, @frmKeyboard_FilterChanged )

    ' ---- the list ---------------------------------------------------------------------
    HWND_FRMKEYBOARD_LIST = PsListTree_Create( HWND_FRMKEYBOARD, IDC_FRMKEYBOARD_LIST1 )

    PsListTree_AddColumn( HWND_FRMKEYBOARD_LIST, L(336,"Category"),     pWindow->ScaleX(FRMKEYBOARD_COL_CATEGORY) )
    PsListTree_AddColumn( HWND_FRMKEYBOARD_LIST, L(205,"Description"),  pWindow->ScaleX(FRMKEYBOARD_COL_DESCRIPTION) )
    PsListTree_AddColumn( HWND_FRMKEYBOARD_LIST, L(337,"Default Keys"), pWindow->ScaleX(FRMKEYBOARD_COL_DEFAULTKEYS) )
    PsListTree_AddColumn( HWND_FRMKEYBOARD_LIST, L(338,"User Keys"),    pWindow->ScaleX(FRMKEYBOARD_COL_USERKEYS) )
    PsListTree_SetFillColumn( HWND_FRMKEYBOARD_LIST, 1 )      ' Description absorbs the slack
    PsListTree_ShowHeader( HWND_FRMKEYBOARD_LIST, true )

    PsListTree_SetPaintCallback( HWND_FRMKEYBOARD_LIST, @frmKeyboard_ListPaintRow )
    PsListTree_SetHeaderPaintCallback( HWND_FRMKEYBOARD_LIST, @frmKeyboard_ListPaintHeader )
    ' Double-click AND Enter, as one gesture. Setting this is also what makes the control
    ' claim Enter -- see the callback.
    PsListTree_SetActivateCallback( HWND_FRMKEYBOARD_LIST, @frmKeyboard_ListActivateCallback )
    PsListTree_SetTooltipCallback( HWND_FRMKEYBOARD_LIST, @frmKeyboard_ListTooltip )
    PsListTree_SetHeaderTooltipCallback( HWND_FRMKEYBOARD_LIST, @frmKeyboard_HeaderTooltip )
    PsListTree_SetFont( HWND_FRMKEYBOARD_LIST, ghFont(GUIFONT_11) )
    ' UNSCALED: PsListTree scales what it is handed, so a pre-scaled value is scaled twice.
    PsListTree_SetRowHeight( HWND_FRMKEYBOARD_LIST, FRMKEYBOARD_ROW_H )
    ' The border is RAW PIXELS and deliberately NOT DPI-scaled -- a chrome hairline should
    ' stay a hairline. PsListTree is the odd one out in its own API here: RowHeight above IS
    ' scaled internally, this is not. Same width and colour as the search box's frame, so the
    ' two read as one family. The colour is pushed in frmKeyboard_ApplyTheme.
    PsListTree_SetBorderWidth( HWND_FRMKEYBOARD_LIST, 1 )

    ' ---- the four buttons -------------------------------------------------------------
    dim as HWND hModify = PsButton_Create( HWND_FRMKEYBOARD, IDC_FRMKEYBOARD_CMDMODIFY )
    PsButton_SetText( hModify, L(282,"Modify") )
    PsButton_SetFont( hModify, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( hModify, @frmKeyboard_ButtonClickCallback )

    ' "Reset", not "Clear": it restores the row to its shipped default rather than emptying
    ' it -- it re-enables a suppressed default as well as dropping the user keys.
    dim as HWND hReset = PsButton_Create( HWND_FRMKEYBOARD, IDC_FRMKEYBOARD_CMDCLEAR )
    PsButton_SetText( hReset, L(341,"Reset") )
    PsButton_SetFont( hReset, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( hReset, @frmKeyboard_ButtonClickCallback )

    dim as HWND hOK = PsButton_Create( HWND_FRMKEYBOARD, IDC_FRMKEYBOARD_CMDOK )
    PsButton_SetText( hOK, L(0,"OK") )
    PsButton_SetFont( hOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( hOK, true )
    PsButton_SetClickCallback( hOK, @frmKeyboard_ButtonClickCallback )

    dim as HWND hCancel = PsButton_Create( HWND_FRMKEYBOARD, IDC_FRMKEYBOARD_CMDCANCEL )
    PsButton_SetText( hCancel, L(1,"Cancel") )
    PsButton_SetFont( hCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( hCancel, @frmKeyboard_ButtonClickCallback )

    ShowWindow( HWND_FRMKEYBOARD_LIST, SW_SHOW )
    ShowWindow( HWND_FRMKEYBOARD_FILTER, SW_SHOW )
    ShowWindow( hModify, SW_SHOW )
    ShowWindow( hReset, SW_SHOW )
    ShowWindow( hOK, SW_SHOW )
    ShowWindow( hCancel, SW_SHOW )

    frmKeyboard_ApplyTheme( HWND_FRMKEYBOARD )
    frmKeyboard_PositionWindows( HWND_FRMKEYBOARD )
    frmKeyboard_BuildList()

    ShowWindow( HWND_FRMKEYBOARD, SW_SHOW )
    UpdateWindow( HWND_FRMKEYBOARD )

    ' FOCUS STARTS IN THE SEARCH BOX, so the dialog opens ready to be typed at -- the usual
    ' reason for opening it is to find one command among a hundred.
    '
    ' It also has to be set explicitly for a second, unrelated reason: IsDialogMessage
    ' ignores Tab until a child already holds the focus, and an ordinary CWindow has no
    ' dialog manager to arrange that. (See the WS_EX_CONTROLPARENT section of
    ' C:\dev\Learnings.md.)
    SetFocus( HWND_FRMKEYBOARD_FILTER )

    ' AFTER the SetFocus above, deliberately: one of its assertions is that the dialog opens
    ' with the focus in the search box, and running it any earlier would sample GetFocus
    ' before anything had been focused at all -- a test that reads the wrong instant and
    ' reports a failure the user would never see.
    frmKeyboard_RunLayoutSelfTest()

    ' Open the Assign Shortcut dialog straight away on the first row, so it can be driven
    ' without a human clicking Modify. Companion to TIKO_KEYBOARD_AUTOOPEN.
    if environ("TIKO_KEYBOARD_AUTOEDIT") = "1" then
        PostMessage( HWND_FRMKEYBOARD, MSG_USER_SHOW_KEYBOARDEDIT, 0, 0 )
    end if

    ' ---- message loop -----------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents: that has no *_FilterMessage hooks, and the filter
    ' box needs one.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. The filter box's right-click menu is a PsPopupMenu; without this it has
        ' no keyboard navigation and never closes on an outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' ESCAPE cancels -- handled explicitly, and AFTER the popup filter above so an open
        ' context menu closes on Escape first rather than the whole dialog going with it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMKEYBOARD, WM_CLOSE, 0, 0 )
            continue while
        end if

        ' DOWN ARROW IN THE SEARCH BOX MOVES TO THE LIST, the way Tab does. Type a few
        ' letters, press Down, and you are on the results -- without lifting a hand to the
        ' mouse or hunting for Tab.
        '
        ' Gated on the focus actually being IN the search box: once the list has it, Down is
        ' the list's own row navigation and must not be stolen. Tested by CONTAINMENT because
        ' the search box is a PsTextBox, a shell around a RichEdit -- GetFocus returns the
        ' inner child and never the handle this file holds. (Same trap as the tab-order
        ' assertions; see frmKeyboard_IsWithin.)
        '
        ' Safe to claim: the box is single-line, so Down does nothing there anyway. It sits
        ' after PsTextBox_FilterMessage so that an open right-click menu gets its own Down for
        ' menu navigation first.
        ' Each test hoisted into its own boolean: the two message comparisons yield
        ' non-booleans while IsWithin yields a boolean, and andalso-ing the kinds together is
        ' fbc warning 38.
        scope
            dim as boolean bDownKey = ((uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_DOWN))
            dim as boolean bInFilter = frmKeyboard_IsWithin( GetFocus(), HWND_FRMKEYBOARD_FILTER )
            if bDownKey andalso bInFilter then
                SetFocus( HWND_FRMKEYBOARD_LIST )
                continue while
            end if
        end scope

        ' ENTER IS NOT INTERCEPTED HERE, deliberately. The list claims it itself (that is
        ' what PsListTree_SetActivateCallback turns on, along with the WM_CHAR swallow that
        ' stops the system beeping), so it opens the editor when the list has focus and
        ' falls through to the default OK button when anything else does. Catching it in the
        ' pump would break the second half of that.
        if IsDialogMessage( HWND_FRMKEYBOARD, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    erase gKeysWork
    erase gKeysConflict
    gKeysFilter = ""

    function = 0

end function
