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

'  frmThemesPage.inc  --  see frmThemes.bi for the design.

#include once "frmThemes.bi"

' frmThemes_ThemedMsgBox is defined in frmOptions.inc, which is included AFTER this file
' (it dispatches to us, so it must come later). Its declaration already arrives early via
' frmOptions.bi, so nothing more is needed here.

' ----------------------------------------------------------------------------------------
' Controls. One set, shared by both pages: every panel-hosted page is built onto the same
' PsScrollPanel page window and switched with Show/Hide, so a second set would be dead
' weight. gThemeCurPage decides which keys the list holds.
' ----------------------------------------------------------------------------------------
dim shared ghThemeKeys      as HWND      ' PsListTree
dim shared ghThemeFore      as HWND      ' PsButton
dim shared ghThemeBack      as HWND      ' PsButton
dim shared ghThemeBold      as HWND      ' PsToggle
dim shared ghThemeItalic    as HWND      ' PsToggle
dim shared ghThemeUnder     as HWND      ' PsToggle
dim shared ghThemeAlpha     as HWND      ' PsNumericUpDown -- the alpha/opacity (channel-correct)
dim shared ghThemeInhF      as HWND      ' use the palette colour (foreground)
dim shared ghThemeInhB      as HWND      ' use the palette colour (background)
dim shared ghThemeResetF    as HWND      ' PsButton -- undo this key's foreground
dim shared ghThemeResetB    as HWND      ' PsButton -- undo this key's background
' R/G/B numeric entry for the CURRENT channel's colour. The large colour chooser now lives in a
' popup (frmThemes_OpenColorPopup); these stay on the dialog. ghThemePicker is kept but HIDDEN --
' it is the current-colour model the popup seeds from and these fields read.
dim shared ghThemeR         as HWND      ' PsTextBox -- FOREGROUND row
dim shared ghThemeR2        as HWND      ' PsTextBox -- BACKGROUND row
dim shared ghThemeG         as HWND
dim shared ghThemeG2        as HWND      ' PsTextBox
dim shared ghThemeB         as HWND
dim shared ghThemeB2        as HWND      ' PsTextBox
dim shared ghThemePicker    as HWND      ' PsColorPicker -- hidden colour model (popup does the UI)

dim shared gThemeBuilt      as boolean
dim shared gThemeCurPage    as long = THEMEPAGE_NONE

' The current-colour swatches drawn beside the Foreground/Background buttons: 0 = fore, 1 = back.
' Rects are set by Layout; Paint fills them from the selected key's colours.
dim shared gThemeSwatchRc(0 to 1) as RECT

' Bind-table indices for the rows currently in the list, in list order. Rebuilt whenever the
' page changes; row N of the listbox is gThemeRowBind(N).
dim shared gThemeRowBind(any) as long

' Set once any colour or flag has actually been changed, so OK knows whether it needs to
' write the theme file at all.
dim shared gThemeEdited     as boolean

' WHICH CHANNEL THE EMBEDDED PICKER IS EDITING -- THEMECH_FORE or THEMECH_BACK.
'
' The Foreground / Background buttons used to OPEN a modal colour dialog. They now SELECT, and
' the picker below them retargets. That is the whole shape of the Phase-3 swap: a modal that
' returns a colour becomes a panel that is always on screen, so "pick a colour" stops being a
' round trip and becomes direct manipulation with the editor recolouring underneath it.
dim shared gThemeChannel    as long = THEMECH_FORE

' A drag is BEGIN, many MOVEs, then END. The fork prompt is a MESSAGE BOX, so it must fire at
' most once per gesture and never during one -- this records that BEGIN has already asked.
' It is also what lets a declined fork abandon the whole drag rather than re-asking on every
' pixel of it.
dim shared gThemeForkAsked  as boolean
dim shared gThemeForkOK     as boolean

' What the picker is currently pointed at, so a re-sync can tell "the user selected a different
' key" from "the value under the same key just changed". Only the first re-establishes the
' picker's Initial baseline -- see the bKeepInitial block in frmThemes_SyncEditControls.
dim shared gThemePickerBind    as long = -1
dim shared gThemePickerChannel as long = -1

' Drawn labels, same trick frmOptionsColors uses: captions are painted, not controls.
type THEMELABEL_TYPE
    rc      as RECT
    caption as DWSTRING
end type
dim shared gThemeLabels(any) as THEMELABEL_TYPE

' Unscaled geometry.
' THREE COLUMNS: key list | colour picker | the controls column.
'
' The page does not scroll any more (only OPTPAGE_EDITOR does), so it has to fit a viewport of
' 680 x 510 unscaled -- and it is the WIDTH that is tight, not the height. Stacking the picker
' under the buttons and toggles, as this page did when the picker first went in, needed ~738px of
' height. Beside them it needs about 450.
'
' THE LIST GETS WHATEVER IS LEFT, and its floor is what the fit assertion watches: the three
' columns plus their gaps have to come out of one fixed width, so the floor is the number that
' decides whether they overlap rather than merely look cramped.
const THEME_LISTHEIGHT   = 372
' MEASURED, NOT CHOSEN. At 175% the panel offers 1106px and the first attempt (floor 170,
' controls 156) needed 1114 -- the remainder fell BELOW the floor, the floor raised the list,
' and the total ran 8px past the edge. That is exactly what the floor is for: it converts an
' overflow into a reported overlap instead of a silently clipped list.
'
' The controls column gives up those 8px and the floor drops 5, so the remainder now clears it
' with room to spare -- and while the floor does not bind, the three columns tile the width
' EXACTLY by construction, because the list is computed as what is left.
const THEME_LISTMINW     = 165      ' below this the key names stop being readable
const THEME_LISTW        = 440      ' the key list width (2-column layout)

' ROW FURNITURE, shared by the layout and the row painter so they cannot disagree about how
' much of the list a name has left. All UNSCALED; run through AfxScaleX at use.
'
'     [ key name ....... ][ fore role ][FG][ back role ][BG][ B I U ]
'
' The two role columns were added after the fact and pushed the name column down to about
' 78px inside a 440px list -- enough for "main.panel" and not remotely enough for
' "output.scrollbardivider", which is the trap CLAUDE.md states outright: DrawText clips with
' DT_END_ELLIPSIS and reports nothing, so a positive span is not the same claim as the label
' fitting. The list width is DERIVED from these now instead of being a constant that happened
' to work, and the floor is asserted.
const THEME_ROLECOLW     = 100      ' one role-name column
' NO bold/italic/underline cell. 50px was reserved for one and NOTHING EVER PAINTED THERE --
' the row reserved the space and drew nothing in it. The flags are not a palette concept
' anyway, and the edit column shows them for the selected key, which is where they belong.
const THEME_ROWGAP       =   6
const THEME_ROWPADR      =   8
const THEME_KEYNAMEMINW  = 170      ' the longest key is output.scrollbardivider
const THEME_CTLCOLW      = 148      ' buttons, toggles and Opacity -- the narrow third column
const THEME_SWATCHW      = 30
const THEME_SWATCHH      = 14
const THEME_ROWHEIGHT    = 22




function frmThemes_WasEdited() as boolean
    return gThemeEdited
end function


' ========================================================================================
' A theme that ships with tiko must never be modified in place -- it is the thing a user
' falls back to when their own experiment goes wrong, and "reset" is only meaningful while
' the originals are pristine.
' ========================================================================================
function frmThemes_IsProtectedTheme( byval wszShortFilename as DWSTRING ) as boolean
    return (lcase(PsLeft(wszShortFilename, 8)) = "default_")
end function


' ========================================================================================
' Pick a filename for the fork. "default_dark.theme" -> "default_dark_custom.theme", then
' _custom2, _custom3... so repeatedly forking the same built-in never silently overwrites an
' earlier attempt.
' ========================================================================================
function frmThemes_MakeForkName( byval wszShortFilename as DWSTRING ) as DWSTRING

    dim as string sBase = wszShortFilename
    if lcase(right(sBase, 6)) = ".theme" then sBase = left(sBase, len(sBase) - 6)

    ' STRIP THE "default_" PREFIX. Without this the fork of default_dark.theme is
    ' default_dark_custom.theme, which still matches IsProtectedTheme -- so the next edit
    ' would fork it AGAIN, and worse, OK would refuse to write it and discard the entire
    ' session's colour work without a word. Found by assertion 13, not by inspection.
    if lcase(left(sBase, 8)) = "default_" then sBase = mid(sBase, 9)
    if len(sBase) = 0 then sBase = "theme"

    dim as DWSTRING wszDir = PsExePath & "settings\themes\"

    for i as long = 1 to 99
        dim as string sSuffix = "_custom"
        if i > 1 then sSuffix &= str(i)
        dim as DWSTRING wszCandidate = sBase & sSuffix & ".theme"
        if PsFileExists( wszDir & wszCandidate ) = 0 then return wszCandidate
    next

    ' 99 forks of one theme. Give up gracefully rather than looping forever.
    return sBase & "_custom99.theme"

end function


' ========================================================================================
' Rebuild gThemeRowBind + the listbox for the current page, straight from the bind table.
' Syntax page = the editor.* namespace; interface page = everything else that carries a
' colour. general.* is excluded from both: it holds the description and version strings.
' ========================================================================================
' Which page owns a key. Public so the self-test can assert the two pages PARTITION the bind
' table -- every colour key on exactly one page, nothing listed twice, nothing orphaned. The
' list builder below calls this too, so the assertion checks the real rule rather than a copy
' of it that could drift.
function frmThemes_PageForKey( byval sKey as string ) as long
    dim as string sNamespace = PsStrParse( sKey, 1, "." )
    ' general.* is description and version text, not colour: on neither page.
    if sNamespace = "general" then return THEMEPAGE_NONE
    if sNamespace = "editor" then return THEMEPAGE_SYNTAX
    return THEMEPAGE_INTERFACE
end function


' ========================================================================================
' The group header a key sits under. Sixty-odd keys in one flat list is a wall; grouping
' them makes the list skimmable and collapsible.
'
' The interface side is DERIVED from the namespace, so it cannot drift. The syntax side
' cannot be -- every key there is "editor.*" -- so it uses a written classification. That is
' a drift risk, and it is handled rather than ignored: anything unrecognised falls into
' "Other" instead of vanishing, and the self-test asserts that bucket stays EMPTY. Add a key
' to the engine without classifying it here and the assertion says so by name.
' ========================================================================================
' Returns a STABLE ID, never display text. This was originally one function returning a
' localized string, and grouping compared those strings -- which collapsed every category into
' one band because the string ids did not yet exist in english.lang and L() therefore answered
' "" for all of them. Adding the ids fixed the symptom; splitting id from title fixes the
' cause. Two categories that happen to translate alike must still be two categories, so
' structure cannot be keyed on display text in any language.
function frmThemes_CategoryIdForKey( byval sKey as string ) as long

    ' THE KEY TABLE ANSWERS THIS NOW. It was a hand-written select case on the namespace for
    ' the interface side and on the second segment for the syntax side -- a third place the
    ' key list was written out, and the one the self-test's "no key lands in Other" assertion
    ' was really policing.
    '
    ' It went red the moment eight keys were added to the engine, which is that assertion
    ' doing exactly its job -- and is also the argument for deleting the duplicate rather
    ' than extending it. A key now carries its category from the same row that carries its
    ' member, its subsystem and its role, so the two cannot disagree.
    dim as long n = Theme_KeyIndex( sKey )
    if n >= 0 then return gThemeKeys(n).nCat

    return THEMECAT_OTHER

end function


' The localized heading for a category id. Display only -- nothing branches on this.
function frmThemes_CategoryTitle( byval nCat as long ) as DWSTRING
    select case nCat
        case THEMECAT_MAIN:      return L(298, "Main Window")
        case THEMECAT_PANEL:     return L(299, "Side Panels")
        case THEMECAT_TOPTABS:   return L(300, "Document Tabs")
        case THEMECAT_MENUBAR:   return L(301, "Menu Bar")
        case THEMECAT_POPUP:     return L(302, "Menus and Popups")
        case THEMECAT_STATUSBAR: return L(303, "Status Bar")
        case THEMECAT_OUTPUT:    return L(304, "Output Pane")
        case THEMECAT_COMPILE:   return L(305, "Build Indicators")
        case THEMECAT_SHARED:    return L(306, "Shared Chrome")
        case THEMECAT_SYNTAX:    return L(308, "Text and Syntax")
        case THEMECAT_CURSOR:    return L(309, "Cursor and Selection")
        case THEMECAT_MARGINS:   return L(310, "Margins and Folding")
        case THEMECAT_MARKERS:   return L(311, "Markers and Highlights")
        case THEMECAT_EDCHROME:  return L(312, "Editor Chrome")
    end select
    return L(307, "Other")
end function


' ========================================================================================
' Tooltip: the actual RGB values behind the swatches. The callback is per-ROW, not per-cell,
' so both channels are reported at once rather than asking the user to hover a 30px square
' precisely -- which is easier to read anyway when comparing a pair.
' ========================================================================================
private function frmThemes_ListTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    ' Disable tooltips on the this listtree
    return ""
end function


private sub frmThemes_PopulateList()

    if ghThemeKeys = 0 then exit sub

    Theme_BuildKeyTable()

    PsListTree_Clear( ghThemeKeys )
    erase gThemeRowBind

    ' A FLAT list, no group headers -- rows are appended in bind-table order, which the engine
    ' already groups by namespace, so related keys still sit together without a divider row
    ' between them.
    dim as long nRow = -1
    dim as long nFirstSelectable = -1

    ' THE PALETTE PAGE FILLS THE SAME LIST WITH ROLES. Reusing the list, its painter, its
    ' selection handling and the swatch click path rather than building a second control:
    ' a role row IS a key row with one channel instead of two, and gThemeRowBind carries the
    ' role index in place of the key index. frmThemes_OnPalettePage() is the only thing that
    ' has to know which, and everything that branches on it says so out loud.
    if gThemeCurPage = THEMEPAGE_PALETTE then
        for i as long = 0 to Theme_RoleCount() - 1
            nRow += 1
            redim preserve gThemeRowBind(nRow) as long
            gThemeRowBind(nRow) = i
            PsListTree_AddString( ghThemeKeys, Theme_RoleName(i), i )
            if nFirstSelectable < 0 then nFirstSelectable = nRow
        next
        if nFirstSelectable >= 0 then PsListTree_SetCurSel( ghThemeKeys, nFirstSelectable )
        exit sub
    end if

    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)

        dim as string sKey = gThemeKeys(i).keyValue
        if frmThemes_PageForKey( sKey ) <> gThemeCurPage then continue for

        nRow += 1
        redim preserve gThemeRowBind(nRow) as long
        gThemeRowBind(nRow) = i
        PsListTree_AddString( ghThemeKeys, sKey, i )
        if nFirstSelectable < 0 then nFirstSelectable = nRow

    next

    if nFirstSelectable >= 0 then PsListTree_SetCurSel( ghThemeKeys, nFirstSelectable )

end sub


' ========================================================================================
' THE SELECTED ROW MEANS TWO DIFFERENT THINGS AND THE COMPILER CANNOT TELL THEM APART.
'
' One list serves three pages. On Syntax and Interface a row maps to a KEY-TABLE index; on
' Palette it maps to a ROLE index. gThemeRowBind carries whichever is current, and both are
' plain longs, so handing one to code expecting the other is silent and wrong -- it reads an
' unrelated key and shows or edits its colour.
'
' That shipped three times: the R/G/B fields showed 0,0,0 for some roles, typing into them
' would have edited an unrelated key, and the page painter drew the wrong swatch. Each was
' found by the author looking at the screen, and none was visible to 900+ assertions.
'
' So the raw accessor is PRIVATE and the two public ones REFUSE the page they do not belong
' to. A consumer that forgets to branch now gets -1 -- which renders nothing and edits
' nothing -- instead of quietly operating on the wrong object.
' ========================================================================================
private function frmThemes_SelectedRow() as long
    if ghThemeKeys = 0 then return -1
    dim as long row = PsListTree_GetCurSel( ghThemeKeys )
    if (row < lbound(gThemeRowBind)) orelse (row > ubound(gThemeRowBind)) then return -1
    return gThemeRowBind(row)
end function

' The KEY-TABLE index behind the selected row, or -1 -- including on the Palette page, where
' there is no key.
private function frmThemes_SelectedBind() as long
    if gThemeCurPage = THEMEPAGE_PALETTE then return -1
    return frmThemes_SelectedRow()
end function

' The ROLE index behind the selected row, or -1 on any page but Palette.
private function frmThemes_SelectedRole() as long
    if gThemeCurPage <> THEMEPAGE_PALETTE then return -1
    return frmThemes_SelectedRow()
end function


' ========================================================================================
' Enable/disable and reload the edit controls for whatever row is selected. A key with no
' background channel gets a disabled Background button rather than a hidden one, so the
' column does not reflow every time the selection moves.
' ========================================================================================
private sub frmThemes_SyncEditControls()

    dim as long nb = frmThemes_SelectedBind()

    ' PALETTE PAGE: the selection is a ROLE index, not a key-table index, so everything below
    ' would read gThemeKeys(nb) -- a different key entirely -- and mirror ITS colour into the
    ' R/G/B fields. That is what shipped: selecting "accent" showed 0,0,0 beside a blue
    ' swatch, because the key that happens to sit at the accent role's index has no value on
    ' the channel it was asked for.
    if gThemeCurPage = THEMEPAGE_PALETTE then
        dim as long nr = frmThemes_SelectedRole()
        dim as boolean bOK = (nr >= 0)
        dim as COLORREF clrRole = iif( bOK, Theme_RoleColor( nr ), 0 )
        if ghThemeR then PsTextBox_SetText( ghThemeR, str(GetRValue(clrRole)) )
        if ghThemeG then PsTextBox_SetText( ghThemeG, str(GetGValue(clrRole)) )
        if ghThemeB then PsTextBox_SetText( ghThemeB, str(GetBValue(clrRole)) )
        if ghThemeR then EnableWindow( ghThemeR, bOK )
        if ghThemeG then EnableWindow( ghThemeG, bOK )
        if ghThemeB then EnableWindow( ghThemeB, bOK )
        if ghThemeFore then
            EnableWindow( ghThemeFore, bOK )
            ' A ROLE HAS ONE COLOUR, so the button says so. Captioning it "Foreground..."
            ' was left over from the key pages, where the pair of buttons picks a CHANNEL;
            ' here there is no channel to pick and the row may well be named "background",
            ' which made the button read as its own opposite.
            PsButton_SetText( ghThemeFore, L(505, "Choose Color") & "..." )
            PsButton_SetDefault( ghThemeFore, true )
            PsButton_Refresh( ghThemeFore )
        end if
        PsListTree_Refresh( ghThemeKeys )
        if HWND_FRMTHEMES_PAGE then AfxRedrawWindow( HWND_FRMTHEMES_PAGE )
        exit sub
    end if

    dim as boolean bHasFore, bHasBack, bHasFlags, bHasAlpha
    dim as long nAlpha = 255

    if nb >= 0 then
        bHasFore  = (gThemeKeys(nb).ofsFore >= 0)
        bHasBack  = (gThemeKeys(nb).ofsBack >= 0)
        bHasFlags = (gThemeKeys(nb).ofsBold >= 0)
        bHasAlpha = gThemeKeys(nb).bAlphaCapable

        dim as long n = Theme_FindEntry( gThemeKeys(nb).keyValue )
        if n >= 0 then
            if ghThemeBold then PsToggle_SetChecked( ghThemeBold, (gTheme(n).bold <> 0) )
            if ghThemeItalic then PsToggle_SetChecked( ghThemeItalic, (gTheme(n).italic <> 0) )
            if ghThemeUnder then PsToggle_SetChecked( ghThemeUnder, (gTheme(n).underline <> 0) )
            ' Show the channel the alpha actually belongs to: background for a selection
            ' highlight, foreground for an indicator.
            if bHasBack then
                if gTheme(n).hasBackAlpha then nAlpha = gTheme(n).backgroundAlpha
            else
                if gTheme(n).hasForeAlpha then nAlpha = gTheme(n).foregroundAlpha
            end if
        end if
    end if

    if ghThemeFore   then EnableWindow( ghThemeFore, bHasFore )
    if ghThemeBack   then EnableWindow( ghThemeBack, bHasBack )

    ' A Reset that would do nothing is disabled rather than silently inert, so the button
    ' doubles as an indicator of which channels the user has actually touched.
    dim as boolean bResetF, bResetB
    if nb >= 0 then
        bResetF = bHasFore andalso Theme_IsChannelModified( gThemeKeys(nb).keyValue, THEMECH_FORE )
        bResetB = bHasBack andalso Theme_IsChannelModified( gThemeKeys(nb).keyValue, THEMECH_BACK )
    end if
    if ghThemeResetF then EnableWindow( ghThemeResetF, bResetF )
    if ghThemeResetB then EnableWindow( ghThemeResetB, bResetB )
    if ghThemeBold   then PsToggle_SetEnabled( ghThemeBold, bHasFlags )
    if ghThemeItalic then PsToggle_SetEnabled( ghThemeItalic, bHasFlags )
    if ghThemeUnder  then PsToggle_SetEnabled( ghThemeUnder, bHasFlags )

    if ghThemeAlpha then
        PsNumericUpDown_SetValue( ghThemeAlpha, nAlpha )
        EnableWindow( ghThemeAlpha, bHasAlpha )
    end if

    ' ---- retarget the embedded picker ----------------------------------------------------
    if ghThemePicker then

        ' A channel the key does not have cannot be edited, so fall back to the one it does.
        ' Silently: the user picked a ROW, not a channel, and being told "this key has no
        ' background" would be noise on a list where half the keys are foreground-only.
        ' Hoisted to boolean locals rather than chained inline: mixing a comparison result with a
        ' boolean field in one andalso chain trips fbc's warning 38, which is a real rule and not
        ' noise (C:\dev\Learnings.md).
        dim as boolean bOnFore = (gThemeChannel = THEMECH_FORE)
        dim as boolean bOnBack = (gThemeChannel = THEMECH_BACK)
        if bOnFore andalso (bHasFore = false) andalso bHasBack then
            gThemeChannel = THEMECH_BACK
        elseif bOnBack andalso (bHasBack = false) andalso bHasFore then
            gThemeChannel = THEMECH_FORE
        end if

        ' Back to the channel caption. The Palette page renames this button, and without
        ' restoring it here the rename would survive the page switch -- the control is shared,
        ' so whichever page touched it last wins.
        if ghThemeFore then PsButton_SetText( ghThemeFore, L(290, "Foreground") & "..." )

        ' The DEFAULT border marks the active channel. It is appearance-only and survives all
        ' four moods, so it says "this is the one you are editing" without a second colour or a
        ' toggle control -- and without claiming Enter from anywhere else in the dialog.
        if ghThemeFore then PsButton_SetDefault( ghThemeFore, (gThemeChannel = THEMECH_FORE) )
        if ghThemeBack then PsButton_SetDefault( ghThemeBack, (gThemeChannel = THEMECH_BACK) )

        dim as COLORREF clrEdit = 0
        dim as boolean bCanEdit = false
        if nb >= 0 then
            dim as long n2 = Theme_FindEntry( gThemeKeys(nb).keyValue )
            if n2 >= 0 then
                if gThemeChannel = THEMECH_FORE then
                    clrEdit  = gTheme(n2).foreground
                    bCanEdit = bHasFore
                else
                    clrEdit  = gTheme(n2).background
                    bCanEdit = bHasBack
                end if
            end if
        end if

        ' bKeepInitial IS THE POINT OF THIS BLOCK. "Initial" must mean "before you started
        ' editing THIS key on THIS channel", so the baseline is re-established only when the key
        ' or the channel actually moves -- not on every END, which would make the Initial swatch
        ' chase the Current one and show nothing.
        dim as boolean bSameTarget = ((nb = gThemePickerBind) andalso (gThemeChannel = gThemePickerChannel))
        PsColorPicker_SetColor( ghThemePicker, clrEdit, 255, bSameTarget )
        gThemePickerBind    = nb
        gThemePickerChannel = gThemeChannel

        EnableWindow( ghThemePicker, bCanEdit )

        ' Each row mirrors ITS OWN channel -- neither follows a selection. SetText is
        ' silent, so no recursion. A row whose channel the key does not have is disabled
        ' rather than blanked: an empty enabled box invites typing into nothing.
        dim as COLORREF clrF, clrB
        dim as long nrc = Theme_FindEntry( gThemeKeys(nb).keyValue )
        if nrc >= 0 then
            clrF = gTheme(nrc).foreground
            clrB = gTheme(nrc).background
        end if
        if ghThemeR then PsTextBox_SetText( ghThemeR, str(GetRValue(clrF)) )
        if ghThemeG then PsTextBox_SetText( ghThemeG, str(GetGValue(clrF)) )
        if ghThemeB then PsTextBox_SetText( ghThemeB, str(GetBValue(clrF)) )
        if ghThemeR2 then PsTextBox_SetText( ghThemeR2, str(GetRValue(clrB)) )
        if ghThemeG2 then PsTextBox_SetText( ghThemeG2, str(GetGValue(clrB)) )
        if ghThemeB2 then PsTextBox_SetText( ghThemeB2, str(GetBValue(clrB)) )
        ' Enabled only when it would DO something: the channel must have a role and must not
        ' already be following it. A permanently live button that is usually a no-op teaches
        ' the user to ignore it.
        if ghThemeInhF then
            EnableWindow( ghThemeInhF, Theme_ChannelIsOverridden( gThemeKeys(nb).keyValue, THEMECH_FORE ) )
        end if
        if ghThemeInhB then
            EnableWindow( ghThemeInhB, Theme_ChannelIsOverridden( gThemeKeys(nb).keyValue, THEMECH_BACK ) )
        end if

        if ghThemeR then EnableWindow( ghThemeR, bHasFore )
        if ghThemeG then EnableWindow( ghThemeG, bHasFore )
        if ghThemeB then EnableWindow( ghThemeB, bHasFore )
        if ghThemeR2 then EnableWindow( ghThemeR2, bHasBack )
        if ghThemeG2 then EnableWindow( ghThemeG2, bHasBack )
        if ghThemeB2 then EnableWindow( ghThemeB2, bHasBack )
    end if

    ' Swatches live in the list rows AND beside the Fore/Back buttons (painted on the page), so a
    ' selection change has to repaint both.
    '
    ' PsListTree_Refresh, NOT AfxRedrawWindow. The control is a CONTAINER over a child surface
    ' and the rows are painted onto that surface, so invalidating the container never reaches
    ' them -- the documented trap in CLAUDE.md, and all four call sites here had it. It showed
    ' up on "use palette color", because that is the action where a row VISIBLY changes (the
    ' grey role name appears); the others mostly changed a swatch the page redraw also owned.
    ' Refresh also preserves the scroll position, which a plain invalidate would not.
    PsListTree_Refresh( ghThemeKeys )
    if HWND_FRMTHEMES_PAGE then AfxRedrawWindow( HWND_FRMTHEMES_PAGE )

end sub


' ========================================================================================
' Fork the current theme if it is one of the shipped ones, so an edit never lands on a
' built-in. Returns FALSE if the user declined, in which case the edit must not proceed.
'
' The fork file is written IMMEDIATELY rather than on OK, because the Themes and Fonts page
' lists the folder and the new theme has to be selectable there. A cancelled dialog therefore
' leaves the copy on disk -- deliberate: the user asked for it to exist, and the alternative
' is a theme that vanishes when they change their mind about one colour.
' ========================================================================================
private function frmThemes_EnsureForked() as boolean

    if frmThemes_IsProtectedTheme( gThemeCurrentShortFilename ) = false then return true

    dim as DWSTRING wszFork = frmThemes_MakeForkName( gThemeCurrentShortFilename )

    dim as DWSTRING wszMsg = L(287, "This is a built-in theme and cannot be modified.") & _
                             chr(13) & chr(13) & _
                             L(288, "Your changes will be saved as a new theme:") & _
                             chr(13) & wszFork

    if frmThemes_ThemedMsgBox( wszMsg, L(318, "Themes"), _
                                TMB_ICON_QUESTION, TMB_OKCANCEL ) <> IDOK then return false

    dim as DWSTRING wszPath = PsExePath & "settings\themes\" & wszFork
    dim as DWSTRING wszDesc = theme.general.description & " (custom)"

    if Theme_WriteFile( wszPath, wszDesc ) then
        frmThemes_ThemedMsgBox( L(289, "The new theme file could not be written."), _
                                 L(318, "Themes"), TMB_ICON_ERROR, TMB_OK )
        return false
    end if

    ' Everything now points at the fork: the engine (so Theme_IsDark and any later save use
    ' it) and the staging copy (so OK persists it to gConfig).
    gThemeCurrentShortFilename = wszFork
    gThemesWorkFile = wszFork
    theme.general.description = wszDesc

    ' The Themes and Fonts page has a stale folder listing now.
    frmThemes_RefreshThemeList()

    return true

end function


' ========================================================================================
' Editor-lifecycle hooks called by the shell (frmThemes.inc). Defined here because gThemeEdited,
' ghGeneral and EnsureForked are all in this file.
' ========================================================================================
sub frmThemes_MarkEdited()
    gThemeEdited = true
end sub

sub frmThemes_ClearEdited()
    gThemeEdited = false
end sub

' Write the theme being edited to its .theme file. Forks first if it is a built-in. The user's
' typed description wins over the fork's auto "(custom)" one, but ONLY if they actually typed --
' otherwise a fork of an untouched built-in keeps the distinguishing suffix. Returns TRUE on
' success; clears the dirty flag and refreshes the shell's theme list.
function frmThemes_SaveThemeFile() as boolean
    dim as DWSTRING wszUserDesc = theme.general.description
    dim as boolean  bUserDesc   = gThemesDescEdited

    if frmThemes_EnsureForked() = false then return false   ' declined the fork -> do not save
    if bUserDesc then theme.general.description = wszUserDesc

    dim as DWSTRING wszPath = PsExePath & "settings\themes\" & gThemesWorkFile
    if Theme_WriteFile( wszPath, theme.general.description ) then
        frmThemes_ThemedMsgBox( L(289,"The new theme file could not be written."), _
                                L(318,"Themes"), TMB_ICON_ERROR, TMB_OK )
        return false
    end if

    gThemeEdited     = false
    gThemesDescEdited = false
    frmThemes_RefreshThemeList()
    return true
end function


' ========================================================================================
' CHANNEL SELECTION.  The Foreground / Background buttons no longer open anything -- they say
' which channel the embedded picker is pointed at, and the picker retargets.
'
' The active one is marked with PsButton's DEFAULT border, which is appearance-only and
' survives all four moods, so it reads as "this is the one you are editing" without needing a
' second colour or a toggle control.
' ========================================================================================
private sub frmThemes_SetChannel( byval nChannel as long )
    if gThemeChannel = nChannel then exit sub
    gThemeChannel = nChannel
    frmThemes_SyncEditControls()
end sub

' ========================================================================================
' THE COLOUR POPUP.  A WS_POPUP hosting a SELECTION-ONLY PsColorPicker (tabs + body, no fields
' or previews), opened by the Foreground / Background buttons. The numeric fields stay on the
' dialog, so the popup is the colour-CHOOSING surface only.
'
' IT COMMITS AND CLOSES ON THE PICKER'S END PHASE, and that one rule is both behaviours the
' brief asked for: a list-tab swatch click emits a bare END (close on the click), and a matrix
' drag emits END on release (close on release). No special-casing of the tab.
'
' NOTE FOR PHASE C: the embedded picker (ghThemePicker) is still created and shown on the page;
' this popup is additive for now. It reads the CURRENT colour and tab from the embedded picker
' to seed itself. Phase C removes the embedded picker and adds the R/G/B/A fields.
' ========================================================================================
' ========================================================================================
'  THE COLOUR POPUP IS PsColorPicker's OWN NOW.
'
'  This was ~200 lines: a hand-rolled WS_POPUP with its own WndProc, a 1px frame painted by
'  filling the client, monitor clamping, flip-above-if-it-will-not-fit, an Escape and
'  outside-click pump filter, and a one-shot guard so the click that dismissed the popup did
'  not immediately reopen it (PsComboBox's trap). Canonical PsColorPicker_DoModalForRect does
'  every one of those things itself, so all of it is deleted -- along with the SECOND picker
'  instance it needed and the hidden "colour model" picker that seeded it.
'
'  WHAT IS LOST, AND IT WAS PRICED IN: the live drag preview. The old popup reported every
'  CLR_PHASE_MOVE and the theme repainted under the cursor; DoModal blocks and answers once,
'  so the colour lands on OK. That is the trade the sync was accepted on.
'
'  THE FORK PROMPT MOVES WITH IT. It used to fire on CLR_PHASE_BEGIN -- deliberately, because
'  asking per MOVE would stack a hundred message boxes during one drag and asking on END would
'  let the user watch the app recolour before being told it could not be saved. With a modal
'  picker there is no BEGIN to hook, so it asks BEFORE the picker opens, which is the same
'  moment in the user's terms: the question is answered before anything changes.
' ========================================================================================

' Build a PSCOLORPICKER_COLORS from the current dialog theme.
private sub frmThemes_FillPickerColors( byval pc as PSCOLORPICKER_COLORS ptr )
    if pc = 0 then exit sub
    pc->BackColor           = theme.panel.text.backcolor
    pc->ForeColor           = theme.panel.text.forecolor
    pc->BorderColor         = theme.ui.divider
    pc->TabBackColor        = theme.panel.text.backcolor
    pc->TabBackColorSel     = theme.panel.texthot.backcolor
    pc->TabForeColor        = theme.panel.text.forecolor
    pc->TabForeColorSel     = theme.panel.texthot.forecolor
    pc->FieldBackColor      = theme.ui.field.backcolor
    pc->FieldForeColor      = theme.ui.field.forecolor
    pc->FieldBorderColor    = theme.ui.divider
    pc->FieldBorderColorSel = theme.ui.accent
    pc->SelRingColor        = theme.ui.accent
    pc->FocusRingColor      = theme.ui.accent
    ' HotRingColor stays at its white default: it is drawn ON a swatch of an arbitrary colour.
end sub


' Forward -- defined later in this file (FB is single-pass).
declare sub frmThemes_ApplyPickedColor( byval nChannel as long, byval clr as COLORREF )
declare sub frmThemes_OpenRolePopup( byval idxRole as long, byval hOpener as HWND )
declare sub frmThemes_InheritForeClick( byval hButton as HWND, byval id as long )
declare sub frmThemes_InheritBackClick( byval hButton as HWND, byval id as long )


' Ask for a colour for one channel of the selected key, anchored under hOpener.
private sub frmThemes_OpenColorPopup( byval nChannel as long, byval hOpener as HWND )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub
    if (nChannel = THEMECH_FORE) andalso (gThemeKeys(nb).ofsFore < 0) then exit sub
    if (nChannel = THEMECH_BACK) andalso (gThemeKeys(nb).ofsBack < 0) then exit sub

    ' BEFORE the picker opens -- see the note above. A declined fork means nothing is asked.
    if frmThemes_EnsureForked() = false then exit sub

    frmThemes_SetChannel( nChannel )

    dim as long n = Theme_FindEntry( gThemeKeys(nb).keyValue )
    if n < 0 then exit sub
    dim as COLORREF clr = iif( nChannel = THEMECH_FORE, gTheme(n).foreground, gTheme(n).background )
    dim as ubyte nAlpha = 255

    dim as RECT rcAnchor
    if hOpener then
        GetWindowRect( hOpener, @rcAnchor )
    else
        GetWindowRect( HWND_FRMTHEMES, @rcAnchor )
    end if

    if PsColorPicker_DoModalForRect( HWND_FRMTHEMES, rcAnchor, clr, nAlpha ) then
        frmThemes_ApplyPickedColor( nChannel, clr )
    end if

end sub


' Ask for a colour for one ROLE, anchored under hOpener.
'
' Separate from frmThemes_OpenColorPopup because the commit is different in a way that
' matters: Theme_SetRole re-resolves every key inheriting the role, and returns THSUB_ALL
' because a role feeds keys across every namespace by design.
private sub frmThemes_OpenRolePopup( byval idxRole as long, byval hOpener as HWND )

    if (idxRole < 0) orelse (idxRole >= Theme_RoleCount()) then exit sub
    if frmThemes_EnsureForked() = false then exit sub

    dim as COLORREF clr = Theme_RoleColor( idxRole )
    dim as ubyte nAlpha = 255

    dim as RECT rcAnchor
    if hOpener then
        GetWindowRect( hOpener, @rcAnchor )
    else
        GetWindowRect( HWND_FRMTHEMES, @rcAnchor )
    end if

    if PsColorPicker_DoModalForRect( HWND_FRMTHEMES, rcAnchor, clr, nAlpha ) then
        dim as ulong nMask = Theme_SetRole( idxRole, clr )
        gThemeEdited = true
        Theme_FillGlobals()
        Theme_RequestApply( nMask )
        frmThemes_PopulateList()      ' the swatch, and any "default" marker, just moved
        frmThemes_SyncEditControls()
    end if

end sub


' Commit a colour the modal picker returned.
'
' The tail of what frmThemes_PickerChanged used to do on CLR_PHASE_END, minus the phase
' machinery: with a modal picker there is one answer at one moment, so there is no BEGIN to
' arm a fork latch on, no MOVE to preview, and no declined-gesture state to reset. The fork
' question is already answered by the time this runs -- frmThemes_OpenColorPopup asks before
' the picker opens.
private sub frmThemes_ApplyPickedColor( byval nChannel as long, byval clr as COLORREF )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub
    dim as string sKey = gThemeKeys(nb).keyValue

    ' Alpha rides ALONGSIDE the COLORREF, never folded into its high byte -- the house rule in
    ' PsBufferPaint.bi, unsafe to break because GDI reads 0x01/0x02 there as palette-relative.
    ' -1 is Theme_SetColor's "leave the alpha alone" sentinel; the picker's own alpha field is
    ' hidden on this page, so alpha is owned by the PsNumericUpDown beside the swatches.
    dim as ulong nMask = Theme_SetColor( sKey, nChannel, clr, -1 )
    gThemeEdited = true

    Theme_RequestApply( nMask )
    frmThemes_SyncEditControls()

end sub


' A click on the page (page client coords). If it hit a Foreground/Background colour swatch,
' open the popup for that channel -- anchored to the matching button so it lands in the same
' place a button click would. Returns TRUE if it was a swatch.
function frmThemes_SwatchClick( byval x as long, byval y as long ) as boolean
    dim as POINT pt : pt.x = x : pt.y = y

    ' PALETTE PAGE: one swatch, and it edits a ROLE rather than a key. Routed here rather
    ' than through the key path because Theme_SetRole has to re-resolve every key that
    ' inherits the role -- Theme_SetColor would set the role's own entry and reach nothing.
    if gThemeCurPage = THEMEPAGE_PALETTE then
        dim as long nr = frmThemes_SelectedRole()
        if nr < 0 then return false
        if PtInRect( @gThemeSwatchRc(0), pt ) = 0 then return false
        frmThemes_OpenRolePopup( nr, ghThemeFore )
        return true
    end if

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then return false

    dim as long  nChan   = -1
    dim as HWND  hAnchor = 0
    if (gThemeKeys(nb).ofsFore >= 0) andalso (PtInRect( @gThemeSwatchRc(0), pt ) <> 0) then
        nChan = THEMECH_FORE : hAnchor = ghThemeFore
    elseif (gThemeKeys(nb).ofsBack >= 0) andalso (PtInRect( @gThemeSwatchRc(1), pt ) <> 0) then
        nChan = THEMECH_BACK : hAnchor = ghThemeBack
    end if
    if nChan < 0 then return false

    frmThemes_OpenColorPopup( nChan, hAnchor )
    return true
end function


' Hand a channel back to its role. The palette's other direction: without this every edit
' detaches a key for good and the theme decays into literals one key at a time.
private sub frmThemes_InheritChannel( byval nChannel as long )
    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub
    if frmThemes_EnsureForked() = false then exit sub

    dim as ulong nMask = Theme_InheritChannel( gThemeKeys(nb).keyValue, nChannel )
    if nMask = THSUB_NONE then exit sub

    gThemeEdited = true
    Theme_FillGlobals()
    Theme_RequestApply( nMask )
    frmThemes_SyncEditControls()
end sub

private sub frmThemes_InheritForeClick( byval hButton as HWND, byval id as long )
    frmThemes_InheritChannel( THEMECH_FORE )
end sub

private sub frmThemes_InheritBackClick( byval hButton as HWND, byval id as long )
    frmThemes_InheritChannel( THEMECH_BACK )
end sub


private sub frmThemes_ForeClick( byval hButton as HWND, byval id as long )
    ' On the palette page this button edits the selected ROLE -- there is only one channel,
    ' so Background is hidden and this is simply "the colour".
    if gThemeCurPage = THEMEPAGE_PALETTE then
        frmThemes_OpenRolePopup( frmThemes_SelectedRole(), hButton )
        exit sub
    end if
    frmThemes_OpenColorPopup( THEMECH_FORE, hButton )
end sub

private sub frmThemes_BackClick( byval hButton as HWND, byval id as long )
    frmThemes_OpenColorPopup( THEMECH_BACK, hButton )
end sub


' ========================================================================================
' THE PICKER MOVED.  This is what the modal AfxChooseColorDialog call became.
'
' THE PHASE IS THE WHOLE REASON THIS WORKS. A drag emits BEGIN, many MOVEs and one END, and the
' three are treated differently for one concrete reason: THE FORK PROMPT IS A MESSAGE BOX. Ask
' on every MOVE and dragging across a built-in theme's matrix would stack a hundred modal
' dialogs; ask only on END and the user would have watched the whole application recolour before
' being told the change cannot be saved there.
'
' So the prompt goes on BEGIN -- before a single pixel has moved -- and its answer is remembered
' for the rest of the gesture. A declined fork abandons the drag silently: nothing is written and
' nothing repaints, which is the honest reading of "no, do not modify the built-in theme".
'
' A swatch click, a typed value and an arrow key all emit a bare END with no BEGIN before it, so
' END has to be able to ask as well. gThemeForkAsked is what stops it asking twice.
'
' MOVE deliberately does NOT call frmThemes_SyncEditControls: that redraws the key list to
' refresh its swatches, and doing it sixty times a second while dragging is exactly the kind of
' work that makes a live preview feel worse than a modal.
' ========================================================================================
private sub frmThemes_PickerChanged( byval hCtrl as HWND, _
                                           byval clr as COLORREF, _
                                           byval nAlpha as ubyte, _
                                           byval nPhase as long )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub

    dim as string sKey = gThemeKeys(nb).keyValue

    ' The channel must exist on this key. The picker is disabled when it does not, but a stale
    ' notification arriving as the selection moves would otherwise write to a null binding.
    if gThemeChannel = THEMECH_FORE then
        if gThemeKeys(nb).ofsFore < 0 then exit sub
    else
        if gThemeKeys(nb).ofsBack < 0 then exit sub
    end if

    if nPhase = CLR_PHASE_BEGIN then
        gThemeForkAsked = true
        gThemeForkOK    = frmThemes_EnsureForked()
        if gThemeForkOK = false then exit sub
    end if

    if gThemeForkAsked = false then
        gThemeForkAsked = true
        gThemeForkOK    = frmThemes_EnsureForked()
    end if
    if gThemeForkOK = false then
        ' Still inside a declined gesture. Reset the latch on the END so the NEXT gesture gets a
        ' fresh chance to ask -- the user may well fork on the second attempt.
        if nPhase = CLR_PHASE_END then gThemeForkAsked = false
        exit sub
    end if

    ' Alpha rides ALONGSIDE the COLORREF, never folded into its high byte -- the house rule
    ' stated in PsBufferPaint.bi, and unsafe to break because GDI reads 0x01/0x02 in that byte as
    ' palette-relative. It is only passed on for a key that has somewhere to put it; everything
    ' else gets -1, which is Theme_SetColor's "leave the alpha alone" sentinel.
    dim as long nA = -1
    if gThemeKeys(nb).bAlphaCapable then nA = clng( nAlpha )

    dim as ulong nMask = Theme_SetColor( sKey, gThemeChannel, clr, nA )
    gThemeEdited = true

    ' Live: the whole application recolours on the next coalescing tick. During a drag that
    ' means it follows the cursor at ~30ms, which is the intended feel.
    Theme_RequestApply( nMask )

    if nPhase = CLR_PHASE_END then
        gThemeForkAsked = false
        frmThemes_SyncEditControls()
    end if

end sub


' ========================================================================================
' THE PALETTE TAB.  The theme's %MACRO definitions, offered as named colours.
'
' This is the format's own variables finally becoming a feature: a .theme file has been able to
' declare "%accent: 97,175,239" and refer to it from a dozen keys since long before there was
' any UI for it. The picker lists them and the host owns them entirely.
' ========================================================================================
private function frmThemes_PaletteCount( byval hCtrl as HWND ) as long
    return Theme_MacroCount()
end function

private function frmThemes_PaletteEntry( byval hCtrl as HWND, _
                                               byval idx as long, _
                                               byref wszName as DWSTRING, _
                                               byref clr as COLORREF _
                                               ) as boolean
    return Theme_MacroAt( idx, wszName, clr )
end function


' ASSIGN -- the counterpart to detach-on-edit, and the reason the palette can be rebuilt rather
' than only spent. Editing a key that shares a macro detaches THAT key to a literal and leaves
' the macro and its other users alone; without an explicit re-attach every edit would erode the
' palette and a theme would drift toward all-literals.
'
' It forks like any other modification, because it changes what gets written to the file.
private sub frmThemes_PaletteAssign( byval hCtrl as HWND, byval idx as long )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub

    dim as DWSTRING wszName
    dim as COLORREF clr
    if Theme_MacroAt( idx, wszName, clr ) = false then exit sub

    if frmThemes_EnsureForked() = false then exit sub

    dim as ulong nMask = Theme_AssignMacro( gThemeKeys(nb).keyValue, gThemeChannel, wszName )
    if nMask = THSUB_NONE then exit sub

    gThemeEdited = true
    Theme_RequestApply( nMask )
    frmThemes_SyncEditControls()

end sub


' ========================================================================================
' Reset one channel back to what the CURRENT THEME loaded with -- see Theme_CaptureBaseline
' for why that is not the same thing as what the dialog opened with.
'
' No fork prompt: putting a value back the way it was is not a modification of the built-in,
' and prompting here would be absurd if the user has not forked yet (there would be nothing
' to reset). Nor does it set gThemeEdited -- if every experiment is reset, OK has nothing to
' write, which is the honest outcome.
' ========================================================================================
private sub frmThemes_ResetChannel( byval nChannel as long )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub

    dim as ulong nMask = Theme_ResetChannel( gThemeKeys(nb).keyValue, nChannel )
    if nMask = THSUB_NONE then exit sub

    Theme_RequestApply( nMask )
    frmThemes_SyncEditControls()

end sub

private sub frmThemes_ResetForeClick( byval hButton as HWND, byval id as long )
    frmThemes_ResetChannel( THEMECH_FORE )
end sub

private sub frmThemes_ResetBackClick( byval hButton as HWND, byval id as long )
    frmThemes_ResetChannel( THEMECH_BACK )
end sub


' ========================================================================================
' Bold / italic / underline. PsToggle fires this only for user action, never for
' SetChecked, so reloading the controls in _SyncEditControls cannot recurse back in here.
' ========================================================================================
private sub frmThemes_FlagChanged( byval hToggle as HWND, byval bChecked as boolean )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub

    dim as long nChannel = THEMECH_BOLD
    if hToggle = ghThemeItalic then nChannel = THEMECH_ITALIC
    if hToggle = ghThemeUnder  then nChannel = THEMECH_UNDER

    if frmThemes_EnsureForked() = false then
        ' Put the switch back: the model did not change.
        PsToggle_SetChecked( hToggle, (bChecked = false) )
        exit sub
    end if

    dim as ulong nMask = Theme_SetFlag( gThemeKeys(nb).keyValue, nChannel, iif(bChecked, 1, 0) )
    gThemeEdited = true
    Theme_RequestApply( nMask )

end sub


' ========================================================================================
' Alpha. Only ever enabled for a key whose alpha reaches a pixel -- see Theme_IsAlphaCapable.
' ========================================================================================
private sub frmThemes_AlphaChanged( byval hCtrl as HWND, byval dblValue as double )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub
    if gThemeKeys(nb).bAlphaCapable = false then exit sub

    dim as string sKey = gThemeKeys(nb).keyValue
    dim as long n = Theme_FindEntry( sKey )
    if n < 0 then exit sub

    if frmThemes_EnsureForked() = false then exit sub

    ' Apply the alpha to whichever channel this key actually carries it on.
    dim as long nChannel = iif( gThemeKeys(nb).ofsBack >= 0, THEMECH_BACK, THEMECH_FORE )
    dim as COLORREF clr = iif( nChannel = THEMECH_FORE, gTheme(n).foreground, gTheme(n).background )

    dim as ulong nMask = Theme_SetColor( sKey, nChannel, clr, clng(dblValue) )
    gThemeEdited = true
    Theme_RequestApply( nMask )

end sub


private function frmThemes_ClampByte( byval n as long ) as long
    if n < 0 then return 0
    if n > 255 then return 255
    return n
end function


' Any of the R/G/B fields changed. Compose the colour for the CURRENT channel and apply it live.
' PsTextBox fires per keystroke, so -- like a picker drag -- this applies but must NOT re-set the
' fields (SetText would move the caret out from under the typist).
private sub frmThemes_RGBChanged( byval hTextBox as HWND )
    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub

    ' PALETTE PAGE: edit the ROLE. Theme_SetRole re-resolves every key inheriting it, which
    ' Theme_SetColor on the role's own entry would not do.
    if gThemeCurPage = THEMEPAGE_PALETTE then
        dim as long nr = frmThemes_SelectedRole()
        if nr < 0 then exit sub
        dim as long rr = frmThemes_ClampByte( val( PsTextBox_GetText( ghThemeR ) ) )
        dim as long rg = frmThemes_ClampByte( val( PsTextBox_GetText( ghThemeG ) ) )
        dim as long rb = frmThemes_ClampByte( val( PsTextBox_GetText( ghThemeB ) ) )
        if frmThemes_EnsureForked() = false then exit sub
        dim as ulong nMaskR = Theme_SetRole( nr, RGB( rr, rg, rb ) )
        gThemeEdited = true
        Theme_FillGlobals()
        Theme_RequestApply( nMaskR )
        ' The row swatch and any "default" marker just moved -- but NOT the fields, which the
        ' user is typing into.
        PsListTree_Refresh( ghThemeKeys )
        if HWND_FRMTHEMES_PAGE then AfxRedrawWindow( HWND_FRMTHEMES_PAGE )
        exit sub
    end if

    ' WHICH ROW WAS TYPED IN DECIDES THE CHANNEL. It used to be gThemeChannel -- hidden
    ' state, and the reason there was only ever one row.
    dim as boolean bIsBack = (hTextBox = ghThemeR2)
    bIsBack = (bIsBack orelse (hTextBox = ghThemeG2))
    bIsBack = (bIsBack orelse (hTextBox = ghThemeB2))
    dim as long nChan = iif( bIsBack, THEMECH_BACK, THEMECH_FORE )

    dim as boolean bHasChan = iif( bIsBack, _
                                   (gThemeKeys(nb).ofsBack >= 0), (gThemeKeys(nb).ofsFore >= 0) )
    if bHasChan = false then exit sub

    dim as HWND hR = iif( bIsBack, ghThemeR2, ghThemeR )
    dim as HWND hG = iif( bIsBack, ghThemeG2, ghThemeG )
    dim as HWND hB = iif( bIsBack, ghThemeB2, ghThemeB )
    dim as long r = frmThemes_ClampByte( val( PsTextBox_GetText( hR ) ) )
    dim as long g = frmThemes_ClampByte( val( PsTextBox_GetText( hG ) ) )
    dim as long b = frmThemes_ClampByte( val( PsTextBox_GetText( hB ) ) )
    dim as COLORREF clr = RGB( r, g, b )

    if frmThemes_EnsureForked() = false then exit sub     ' defaults are blocked from Edit -> a no-op

    dim as ulong nMask = Theme_SetColor( gThemeKeys(nb).keyValue, nChan, clr, -1 )
    gThemeEdited = true
    Theme_RequestApply( nMask )

    ' Keep the hidden colour model (the popup's seed) and the row swatches in step -- NOT the fields.
    if ghThemePicker then PsColorPicker_SetColor( ghThemePicker, clr, 255, true )
    PsListTree_Refresh( ghThemeKeys )
    if HWND_FRMTHEMES_PAGE then AfxRedrawWindow( HWND_FRMTHEMES_PAGE )
end sub


private sub frmThemes_ListSelChange( byval hList as HWND, byval row as integer )
    frmThemes_SyncEditControls()
end sub


' ========================================================================================
' Row painting: the key name, then its foreground and background swatches, then B/I/U.
' Right-aligned so the swatches line up in a column no matter how long the key is.
' ========================================================================================
private sub frmThemes_ListPaint( byval p as PSLISTTREE_PAINTINFO ptr )

    if p = 0 then exit sub

    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    else
        backclr = theme.panel.text.backcolor
        foreclr = theme.panel.text.forecolor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )

    dim as long idx = p->itemID
    if (idx < lbound(gThemeRowBind)) orelse (idx > ubound(gThemeRowBind)) then exit sub
    dim as long nb = gThemeRowBind(idx)

    ' Group header: caption only, no swatches. Drawn in the panel's hot foreground so it
    ' reads as a divider rather than as another editable row.
    if (nb < 0) orelse p->isHeader then
        dim as RECT rcHdr = p->rc
        rcHdr.left += AfxScaleX(6)
        p->b->SetForeColor( theme.panel.texthot.forecolor )
        p->b->PaintText( p->wszCaption, @rcHdr, DT_LEFT or DT_END_ELLIPSIS )
        exit sub
    end if

    ' A ROLE ROW: the name, one swatch, and whether this theme actually chose it. A role
    ' left at its built-in default is marked so the user can see which of the twenty-one
    ' this file speaks for -- otherwise every theme looks equally opinionated.
    if gThemeCurPage = THEMEPAGE_PALETTE then
        dim as long swr = AfxScaleX(THEME_SWATCHW)
        dim as long shH = AfxScaleY(THEME_SWATCHH)   ' NOT shr -- that is the shift-right operator
        dim as long xR  = p->rc.right - AfxScaleX(8)
        dim as long yR  = p->rc.top + ((p->rc.bottom - p->rc.top) - shH) \ 2
        dim as RECT rcSw = ( xR - swr, yR, xR, yR + shH )

        dim as RECT rcNm = ( p->rc.left + AfxScaleX(6), p->rc.top, rcSw.left - AfxScaleX(60), p->rc.bottom )
        ' The "role." prefix is on every row and carries no information; drop it.
        dim as string sRole = Theme_RoleName(nb)
        if left(sRole,5) = "role." then sRole = mid(sRole, 6)
        p->b->PaintText( sRole, @rcNm, DT_LEFT or DT_END_ELLIPSIS )

        if Theme_RoleIsExplicit(nb) = false then
            dim as RECT rcDf = ( rcSw.left - AfxScaleX(58), p->rc.top, rcSw.left - AfxScaleX(6), p->rc.bottom )
            p->b->SetForeColor( theme.role.clr(THROLE_FOREGROUNDDIM) )
            p->b->PaintText( L(504, "default"), @rcDf, DT_RIGHT or DT_END_ELLIPSIS )
        end if

        p->b->SetBackColor( theme.ui.divider )
        p->b->PaintRect( @rcSw )
        dim as RECT rcIn = ( rcSw.left + 1, rcSw.top + 1, rcSw.right - 1, rcSw.bottom - 1 )
        p->b->SetBackColor( Theme_RoleColor(nb) )
        p->b->PaintRect( @rcIn )
        ' DELIBERATELY DOES NOT TOUCH gThemeSwatchRc. That pair is PAGE-LAYOUT state: set by
        ' frmThemes_Layout in page coordinates and consumed by the page painter and the page
        ' hit test. Writing a LIST-ROW rect into it made the page draw its swatch wherever
        ' the last row happened to be painted -- which is why the artifact appeared below the
        ' list and moved while scrolling, since scrolling repaints rows.
        exit sub
    end if

    dim as long n = Theme_FindEntry( gThemeKeys(nb).keyValue )
    if n < 0 then exit sub

    dim as long sw = AfxScaleX(THEME_SWATCHW)
    dim as long sh = AfxScaleY(THEME_SWATCHH)
    dim as long gap = AfxScaleX(THEME_ROWGAP)

    ' Lay the right-hand furniture out from the right edge inwards. Each colour channel gets
    ' its OWN role label, in its own column, immediately left of its own swatch:
    '
    '     [ key name ....... ][ fore role ][FG][BG][ back role ]
    '
    ' MIRRORED: the two swatches sit together so fore and back can be compared at a glance,
    ' and each role name sits on the OUTSIDE of its own swatch. It read
    ' [fore role][FG][back role][BG] first, which put a word between the two colours and made
    ' them awkward to compare.
    '
    ' It showed ONE label before, right-aligned before both swatches, chosen as the fore
    ' role if the foreground was inherited and otherwise the back role. So a key inheriting
    ' both -- ui.field takes foreground AND background from the palette -- named only one of
    ' them, and a key inheriting only its background showed a label with nothing to say which
    ' channel it belonged to. A label that can mean either channel means neither.
    dim as long roleW = AfxScaleX(THEME_ROLECOLW)
    dim as long xRight = p->rc.right - AfxScaleX(THEME_ROWPADR)
    dim as long xBackRole = xRight - roleW
    dim as long xBack     = xBackRole - gap - sw
    dim as long xFore     = xBack - gap - sw
    dim as long xForeRole = xFore - gap - roleW
    dim as long yMid  = p->rc.top + ((p->rc.bottom - p->rc.top) - sh) \ 2

    dim as RECT rcText = ( p->rc.left + AfxScaleX(6), p->rc.top, xForeRole - gap, p->rc.bottom )
    p->b->PaintText( gThemeKeys(nb).keyValue, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    ' The role a channel INHERITS, or nothing when it has been given a colour of its own.
    ' That is the whole two-layer model on one row.
    #macro PAINTROLE( _has_, _from_, _rid_, _x_, _align_ )
        scope
            dim as boolean bShow = (_has_)
            bShow = (bShow andalso (_from_))
            if bShow andalso (_rid_ > THROLE_NONE) then
                dim as string sRole
                for i as long = 0 to Theme_RoleCount() - 1
                    if gThemeRoleDef(i).nRole = (_rid_) then
                        sRole = Theme_RoleName(i)
                        if left(sRole,5) = "role." then sRole = mid(sRole, 6)
                        exit for
                    end if
                next
                if len(sRole) > 0 then
                    dim as RECT rcR = ( (_x_), p->rc.top, (_x_) + roleW, p->rc.bottom )
                    p->b->SetForeColor( theme.role.clr(THROLE_FOREGROUNDDIM) )
                    ' Each name hugs its own swatch: the fore name is right-aligned (its
                    ' swatch is to its right), the back name left-aligned (its swatch is to
                    ' its left). Aligning both the same way would leave one of them floating
                    ' a column away from the colour it describes.
                    p->b->PaintText( sRole, @rcR, (_align_) or DT_END_ELLIPSIS )
                    p->b->SetForeColor( foreclr )
                end if
            end if
        end scope
    #endmacro

    PAINTROLE( gThemeKeys(nb).ofsFore >= 0, gTheme(n).foreFromRole, _
               gThemeKeys(nb).nRoleFore, xForeRole, DT_RIGHT )
    PAINTROLE( gThemeKeys(nb).ofsBack >= 0, gTheme(n).backFromRole, _
               gThemeKeys(nb).nRoleBack, xBackRole, DT_LEFT )
    #undef PAINTROLE

    ' Swatches. Outlined in the divider colour so a swatch the same shade as the row behind
    ' it is still visibly a swatch rather than a hole.
    if gThemeKeys(nb).ofsFore >= 0 then
        dim as RECT rcF = ( xFore, yMid, xFore + sw, yMid + sh )
        p->b->SetBackColor( gTheme(n).foreground )
        p->b->PaintRect( @rcF )
        p->b->SetPenColor( theme.ui.divider )
        p->b->PaintRoundOutline( @rcF, 0, 1 )
    end if
    if gThemeKeys(nb).ofsBack >= 0 then
        dim as RECT rcB = ( xBack, yMid, xBack + sw, yMid + sh )
        p->b->SetBackColor( gTheme(n).background )
        p->b->PaintRect( @rcB )
        p->b->SetPenColor( theme.ui.divider )
        p->b->PaintRoundOutline( @rcB, 0, 1 )
    end if
end sub


private sub frmThemes_AddLabel( byval wszCaption as DWSTRING, _
                                      byval x1 as long, byval y1 as long, _
                                      byval x2 as long, byval y2 as long )
    dim as long n = ubound(gThemeLabels) + 1
    redim preserve gThemeLabels(n) as THEMELABEL_TYPE
    gThemeLabels(n).rc = type<RECT>( x1, y1, x2, y2 )
    gThemeLabels(n).caption = wszCaption
end sub


' ========================================================================================
' Build. Guarded: both pages share one set of controls, and whichever is visited first
' creates them.
' ========================================================================================
sub frmThemes_Build( byval nPage as long, byval hPage as HWND )

    gThemeCurPage = nPage

    if gThemeBuilt then
        frmThemes_PopulateList()
        frmThemes_SyncEditControls()
        exit sub
    end if
    gThemeBuilt = true

    ghThemeKeys = PsListTree_Create( hPage, IDC_FRMTHEMES_LSTKEYS )
    PsListTree_SetPaintCallback( ghThemeKeys, @frmThemes_ListPaint )
    PsListTree_SetSelChangeCallback( ghThemeKeys, @frmThemes_ListSelChange )
    PsListTree_SetTooltipCallback( ghThemeKeys, @frmThemes_ListTooltip )
    PsListTree_SetRowHeight( ghThemeKeys, THEME_ROWHEIGHT )
    PsListTree_SetScrollBarWidth( ghThemeKeys, SCROLLBAR_WIDTH_PANEL )

    ghThemeFore = PsButton_Create( hPage, IDC_FRMTHEMES_CMDFORE )
    PsButton_SetText( ghThemeFore, L(290, "Foreground") & "..." )
    PsButton_SetClickCallback( ghThemeFore, @frmThemes_ForeClick )

    ghThemeBack = PsButton_Create( hPage, IDC_FRMTHEMES_CMDBACK )
    PsButton_SetText( ghThemeBack, L(291, "Background") & "..." )
    PsButton_SetClickCallback( ghThemeBack, @frmThemes_BackClick )

    ' Undo glyph (Segoe Fluent Icons E7A7) plus the caption. The glyph font is a SEPARATE
    ' paint-time input from the text font -- see PsButton_SetGlyphFont.
    ghThemeResetF = PsButton_Create( hPage, IDC_FRMTHEMES_CMDRESETF )
    PsButton_SetGlyphLeft( ghThemeResetF, wchr(&hE7A7) )
    PsButton_SetTooltipText( ghThemeResetF, L(313, "Reset") )
    PsButton_SetClickCallback( ghThemeResetF, @frmThemes_ResetForeClick )

    ' USE THE PALETTE COLOUR. Segoe Fluent E790 is the colour/palette glyph; the caption is a
    ' tooltip because the row already carries a button, a swatch and a reset in 300px.
    ghThemeInhF = PsButton_Create( hPage, IDC_FRMTHEMES_CMDINHF )
    PsButton_SetGlyphLeft( ghThemeInhF, !"\uE790" )
    PsButton_SetTooltipText( ghThemeInhF, L(506, "Use palette color") )
    PsButton_SetClickCallback( ghThemeInhF, @frmThemes_InheritForeClick )

    ghThemeInhB = PsButton_Create( hPage, IDC_FRMTHEMES_CMDINHB )
    PsButton_SetGlyphLeft( ghThemeInhB, !"\uE790" )
    PsButton_SetTooltipText( ghThemeInhB, L(506, "Use palette color") )
    PsButton_SetClickCallback( ghThemeInhB, @frmThemes_InheritBackClick )

    ghThemeResetB = PsButton_Create( hPage, IDC_FRMTHEMES_CMDRESETB )
    PsButton_SetTooltipText( ghThemeResetB, L(313, "Reset") )
    PsButton_SetGlyphLeft( ghThemeResetB, wchr(&hE7A7) )
    PsButton_SetClickCallback( ghThemeResetB, @frmThemes_ResetBackClick )

    ghThemeBold = PsToggle_Create( hPage, IDC_FRMTHEMES_TOGBOLD )
    PsToggle_SetCheckChangedCallback( ghThemeBold, @frmThemes_FlagChanged )
    ghThemeItalic = PsToggle_Create( hPage, IDC_FRMTHEMES_TOGITALIC )
    PsToggle_SetCheckChangedCallback( ghThemeItalic, @frmThemes_FlagChanged )
    ghThemeUnder = PsToggle_Create( hPage, IDC_FRMTHEMES_TOGUNDER )
    PsToggle_SetCheckChangedCallback( ghThemeUnder, @frmThemes_FlagChanged )

    ghThemeAlpha = PsNumericUpDown_Create( hPage, IDC_FRMTHEMES_NUMALPHA )
    PsNumericUpDown_SetRange( ghThemeAlpha, 0, 255 )
    PsNumericUpDown_SetDecimalPlaces( ghThemeAlpha, 0 )
    PsNumericUpDown_SetValueChangedCallback( ghThemeAlpha, @frmThemes_AlphaChanged )

    ' R/G/B entry fields for the current channel's colour (the big chooser is in the popup now).
    ghThemeR = PsTextBox_Create( hPage, IDC_FRMTHEMES_TXTR )
    PsTextBox_SetChangeCallback( ghThemeR, @frmThemes_RGBChanged )
    ghThemeG = PsTextBox_Create( hPage, IDC_FRMTHEMES_TXTG )
    PsTextBox_SetChangeCallback( ghThemeG, @frmThemes_RGBChanged )
    ghThemeB = PsTextBox_Create( hPage, IDC_FRMTHEMES_TXTB )
    PsTextBox_SetChangeCallback( ghThemeB, @frmThemes_RGBChanged )

    ' The BACKGROUND row. Same callback -- it tells the rows apart by hwnd.
    ghThemeR2 = PsTextBox_Create( hPage, IDC_FRMTHEMES_TXTR2 )
    PsTextBox_SetChangeCallback( ghThemeR2, @frmThemes_RGBChanged )
    ghThemeG2 = PsTextBox_Create( hPage, IDC_FRMTHEMES_TXTG2 )
    PsTextBox_SetChangeCallback( ghThemeG2, @frmThemes_RGBChanged )
    ghThemeB2 = PsTextBox_Create( hPage, IDC_FRMTHEMES_TXTB2 )
    PsTextBox_SetChangeCallback( ghThemeB2, @frmThemes_RGBChanged )

    ' DRESS ALL SIX HERE, not only in ApplyTheme. Two reasons, and the second is a documented
    ' trap rather than caution: ApplyTheme is not guaranteed to have run by the time the page
    ' is first shown, so the new row came up in PsTextBox's own defaults -- white box, small
    ' top-left text, no border colour -- next to three correctly dressed ones. And
    ' PsTextBox_SetTextAlign MUST be set before the control has content: TM_PLAINTEXT refuses
    ' EM_SETPARAFORMAT silently, so the setter can only apply it by emptying the buffer, and
    ' on a box that already holds a value that discards the undo history (CLAUDE.md).
    scope
        dim hAll(0 to 5) as HWND = { ghThemeR, ghThemeG, ghThemeB, _
                                     ghThemeR2, ghThemeG2, ghThemeB2 }
        for i as long = 0 to 5
            if hAll(i) = 0 then continue for
            PsTextBox_SetTextAlign( hAll(i), TXT_ALIGN_CENTER )
            PsTextBox_SetFont( hAll(i), OptionsFont_Base() )
            PsTextBox_SetForeColor( hAll(i), theme.ui.field.forecolor )
            PsTextBox_SetBackColor( hAll(i), theme.ui.field.backcolor )
            PsTextBox_SetOuterBackColor( hAll(i), theme.panel.text.backcolor )
            PsTextBox_SetBorderColor( hAll(i), theme.ui.divider )
        next
    end scope

    ' THE EMBEDDED PICKER. It adds NOTHING to frmOptions' message pump -- there is no
    ' PsColorPicker_FilterMessage, unlike the PsComboBox and PsTextBox calls already in there.
    ' That absence is the reason it is a panel rather than a dropdown.
    ghThemePicker = PsColorPicker_Create( hPage, IDC_FRMTHEMES_PICKER )

    ' NARROWER MATRIX CELLS THAN THE PICKER'S DEFAULT, and this is a width budget rather than a
    ' preference. Three columns have to come out of one fixed 680px viewport: the list needs its
    ' 170px floor, the controls column 156, and two 16px gaps -- which leaves the picker 290. At
    ' the default 18px cell the matrix alone is 288 and the control asks for 304, so the columns
    ' would overlap by 14px. At 16 the matrix stops being what sizes it (the entry-field row does,
    ' at 262) and the picker settles at 278, leaving 12px of slack for DPI rounding.
    '
    ' The cell HEIGHT is left alone: it is what makes the matrix exactly as tall as the picker's
    ' own lists, which is what keeps the Custom tab free of a dead band.
    PsColorPicker_SetCellSize( ghThemePicker, AfxScaleX(16), AfxScaleY(22) )

    PsColorPicker_SetColorChangedCallback( ghThemePicker, @frmThemes_PickerChanged )
    ' No palette callbacks and no Assign button: canonical PsColorPicker has neither, and with
    ' %MACRO retired there is no named-colour table left for them to talk to. A key's "named
    ' colour" is its ROLE now, and a role is chosen on the page rather than inside the picker.
    PsColorPicker_SetPreviewText( ghThemePicker, L(314, "Initial"), L(315, "Current") )

    ' THE PICKER'S OWN ALPHA FIELD STAYS HIDDEN, deliberately, even on an alpha-capable key.
    ' This page already has a PsNumericUpDown bound to alpha, which knows which CHANNEL the
    ' alpha belongs to (background for a selection highlight, foreground for an indicator) --
    ' a distinction the picker has no way to make. Two editors for one value is how sync bugs
    ' start, so there is one, and it is the one that is already right.

    ' TAB ORDER = Z-ORDER. The controls were created grouped by kind, which does not match how
    ' they read down the page, so re-thread the z-order to the visual sequence: key list, then
    ' Foreground/Reset, Background/Reset, R/G/B, Bold/Italic/Underline, Opacity. Each control is
    ' placed immediately AFTER the previous one, so GetNextDlgTabItem walks them in this order.
    ' (The hidden picker is skipped by the tab walker, so it does not need a slot here.)
    ' IN VISUAL ORDER, which is now block-per-channel: button, use-palette, reset, then that
    ' channel's R/G/B. The second R/G/B row was missing from this list entirely when it was
    ' added -- Tab skipped straight past three live fields.
    dim tabOrder(0 to 16) as HWND = { ghThemeKeys, _
                                      ghThemeFore, ghThemeInhF, ghThemeResetF, _
                                      ghThemeR, ghThemeG, ghThemeB, _
                                      ghThemeBack, ghThemeInhB, ghThemeResetB, _
                                      ghThemeR2, ghThemeG2, ghThemeB2, _
                                      ghThemeBold, ghThemeItalic, ghThemeUnder, ghThemeAlpha }
    for i as long = 1 to ubound(tabOrder)
        if tabOrder(i) andalso tabOrder(i-1) then
            SetWindowPos( tabOrder(i), tabOrder(i-1), 0, 0, 0, 0, _
                          SWP_NOMOVE or SWP_NOSIZE or SWP_NOACTIVATE )
        end if
    next

    frmThemes_ApplyTheme()
    frmThemes_PopulateList()
    frmThemes_SyncEditControls()

end sub


sub frmThemes_ApplyTheme()

    if gThemeBuilt = false then exit sub

    PsListTree_SetBackColor( ghThemeKeys, theme.panel.text.backcolor )
    PsListTree_SetScrollBarColors( ghThemeKeys, theme.panel.scrollbar.backcolor, _
                                  theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    PsListTree_SetFont( ghThemeKeys, OptionsFont_Base() )
    PsListTree_SetBorderWidth( ghThemeKeys, 1 )
    PsListTree_SetBorderColor( ghThemeKeys, theme.ui.divider )

    dim as PSBUTTON_COLORS bc : OptionsTheme_FillButton( @bc )
    PsButton_SetColors( ghThemeFore, @bc )   : PsButton_Refresh( ghThemeFore )
    PsButton_SetColors( ghThemeBack, @bc )   : PsButton_Refresh( ghThemeBack )
    PsButton_SetColors( ghThemeResetF, @bc ) : PsButton_Refresh( ghThemeResetF )
    PsButton_SetColors( ghThemeResetB, @bc ) : PsButton_Refresh( ghThemeResetB )
    PsButton_SetFont( ghThemeFore, OptionsFont_Base() )
    PsButton_SetFont( ghThemeBack, OptionsFont_Base() )
    PsButton_SetFont( ghThemeResetF, OptionsFont_Base() )
    PsButton_SetFont( ghThemeResetB, OptionsFont_Base() )
    PsButton_SetGlyphFont( ghThemeResetF, ghFont(SYMBOLFONT_10) )
    PsButton_SetGlyphFont( ghThemeResetB, ghFont(SYMBOLFONT_10) )
    ' The use-palette buttons, dressed the same way. The GLYPH font is a separate paint-time
    ' input from the caption font -- SetFont does not reach it.
    PsButton_SetColors( ghThemeInhF, @bc )   : PsButton_Refresh( ghThemeInhF )
    PsButton_SetColors( ghThemeInhB, @bc )   : PsButton_Refresh( ghThemeInhB )
    PsButton_SetFont( ghThemeInhF, OptionsFont_Base() )
    PsButton_SetFont( ghThemeInhB, OptionsFont_Base() )
    PsButton_SetGlyphFont( ghThemeInhF, ghFont(SYMBOLFONT_10) )
    PsButton_SetGlyphFont( ghThemeInhB, ghFont(SYMBOLFONT_10) )

    dim as PSTOGGLE_COLORS tc : OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghThemeBold, @tc )   : PsToggle_Refresh( ghThemeBold )
    PsToggle_SetColors( ghThemeItalic, @tc ) : PsToggle_Refresh( ghThemeItalic )
    PsToggle_SetColors( ghThemeUnder, @tc )  : PsToggle_Refresh( ghThemeUnder )

    dim as PSNUMERICUPDOWN_COLORS nc : OptionsTheme_FillNumeric( @nc )
    PsNumericUpDown_SetColors( ghThemeAlpha, @nc )
    PsNumericUpDown_SetFont( ghThemeAlpha, OptionsFont_Base() )
    PsNumericUpDown_Refresh( ghThemeAlpha )

    ' The R/G/B fields as editor-coloured text boxes.
    dim hRGB(0 to 5) as HWND = { ghThemeR, ghThemeG, ghThemeB, _
                                 ghThemeR2, ghThemeG2, ghThemeB2 }
    for i as long = 0 to 2
        if hRGB(i) then
            PsTextBox_SetFont( hRGB(i), OptionsFont_Base() )
            PsTextBox_SetForeColor( hRGB(i), theme.ui.field.forecolor )
            PsTextBox_SetBackColor( hRGB(i), theme.ui.field.backcolor )
            PsTextBox_SetOuterBackColor( hRGB(i), theme.panel.text.backcolor )
            PsTextBox_SetBorderColor( hRGB(i), theme.ui.divider )
            PsTextBox_SetTextAlign( hRGB(i), TXT_ALIGN_CENTER )
        end if
    next

    ' The picker's CHROME follows the dialog's theme. The colours it is EDITING are content and
    ' are never themed -- which is worth saying out loud on this page of all places, because
    ' here the content IS the theme and the two would otherwise be easy to conflate.
    ' The picker's CHROME follows the dialog's theme (its CONTENT -- the colours it edits -- is
    ' never themed). Built by the shared helper so the embedded picker and the popup picker agree.
    dim as PSCOLORPICKER_COLORS pc
    frmThemes_FillPickerColors( @pc )
    PsColorPicker_SetColors( ghThemePicker, @pc )
    PsColorPicker_SetFont( ghThemePicker, OptionsFont_Base() )
    PsColorPicker_Refresh( ghThemePicker )

    ' Keep the popup picker's chrome in step with a live theme change too.
    ' No live picker to re-push to: DoModalForRect builds one per invocation and takes its
    ' colours from frmThemes_FillPickerColors at that moment, so a theme change is picked up
    ' the next time it opens.

end sub


sub frmThemes_ShowPage( byval nPage as long, byval bShow as boolean )

    if gThemeBuilt = false then exit sub

    if bShow andalso (nPage <> gThemeCurPage) then
        gThemeCurPage = nPage
        frmThemes_PopulateList()
        frmThemes_SyncEditControls()
    end if

    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ' The style controls exist only for syntax colours -- hidden on the interface page.
    dim as long nStyle = iif( bShow andalso (nPage = THEMEPAGE_SYNTAX), SW_SHOW, SW_HIDE )
    ' A role carries ONE colour: no background channel, no bold/italic/underline, no alpha.
    ' Hiding rather than disabling -- a permanently dead control on a page it can never apply
    ' to reads as broken, and this page is the first thing the dialog shows.
    dim as boolean bKeyPage = (nPage <> THEMEPAGE_PALETTE)
    dim as long nKeyOnly = iif( bShow andalso bKeyPage, SW_SHOW, SW_HIDE )
    if bShow andalso (bKeyPage = false) then nStyle = SW_HIDE
    ShowWindow( ghThemeKeys, nCmd )
    ShowWindow( ghThemeFore, nCmd )
    ShowWindow( ghThemeBack, nKeyOnly )
    ShowWindow( ghThemeResetF, nKeyOnly )
    ShowWindow( ghThemeResetB, nKeyOnly )
    ' Inheriting is a KEY concept: a role has nothing above it to inherit from.
    ShowWindow( ghThemeInhF, nKeyOnly )
    ShowWindow( ghThemeInhB, nKeyOnly )
    ShowWindow( ghThemeBold, nStyle )
    ShowWindow( ghThemeItalic, nStyle )
    ShowWindow( ghThemeUnder, nStyle )
    ShowWindow( ghThemeAlpha, nStyle )
    ShowWindow( ghThemeR, nCmd )
    ' The background row exists only where there IS a background channel to type into.
    ShowWindow( ghThemeR2, nKeyOnly )
    ShowWindow( ghThemeG2, nKeyOnly )
    ShowWindow( ghThemeB2, nKeyOnly )
    ShowWindow( ghThemeG, nCmd )
    ShowWindow( ghThemeB, nCmd )
    ' The embedded picker is the HIDDEN colour model now -- the popup does the choosing.
    ShowWindow( ghThemePicker, SW_HIDE )

end sub


' ========================================================================================
' Layout. List down the left, edit column down the right -- the right column is where
' Phase 3's embedded PsColorPicker goes, replacing the two buttons.
' Returns the content height, as the PsScrollPanel contract requires.
' ========================================================================================
function frmThemes_Layout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long

    if gThemeBuilt = false then return 0

    erase gThemeLabels

    dim as long padLeft  = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight = AfxScaleX(OPTROW_PADRIGHT)
    dim as long y        = AfxScaleY(OPTROW_PADTOP)
    dim as long contentR = cxPanel - padRight
    dim as long gapX     = AfxScaleX(32)

    ' TWO COLUMNS NOW: the key list, and the controls. The big colour chooser moved to a popup,
    ' so its column is gone and the controls (Foreground/Background, R/G/B, style, opacity, reset)
    ' fill the freed width.
    dim as long textH    = AfxScaleY(18)
    dim as long rowGap   = AfxScaleY(10)
    dim as long ctlH     = AfxScaleY(32)
    dim as long listH    = AfxScaleY(THEME_LISTHEIGHT)

    ' A key list on the left (the names are short) and the controls beside it.
    dim as long editW = AfxScaleX(300)
    ' DERIVED from the row furniture, not a constant that happened to fit. Everything to the
    ' right of the key name is fixed width, so what the name gets is whatever is left -- and
    ' that is the number worth controlling.
    dim as long rowFurniture = AfxScaleX( THEME_ROWPADR + THEME_SWATCHW + THEME_ROWGAP + _
                                          THEME_ROLECOLW + THEME_ROWGAP + THEME_SWATCHW + _
                                          THEME_ROWGAP + THEME_ROLECOLW )
    ' ONE truncated value, used by the derivation AND by the assertion below. Deriving with
    ' the float and checking against clng() of it fails forever by one: the derivation
    ' TRUNCATES (297.5 -> 804 total) and clng ROUNDS (297.5 -> 298). Truncating one side was
    ' not enough -- both have to come from the same long.
    dim as long nameFloor = int( AfxScaleX(THEME_KEYNAMEMINW) )
    dim as long listW = rowFurniture + nameFloor + SCROLLBAR_WIDTH_PANEL
    if listW < AfxScaleX(THEME_LISTMINW) then listW = AfxScaleX(THEME_LISTMINW)

    ' ...but the edit column still has to fit beside it. Shrinking the list is the right
    ' give: the assertion below reports when that has eaten the name column.
    dim as long listMax = contentR - padLeft - gapX - editW
    if listW > listMax then listW = listMax

    dim as long yTop = y
    SetWindowPos( ghThemeKeys, 0, padLeft, yTop, listW, listH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long xEdit = padLeft + listW + gapX
    dim as long yEdit = yTop

    ' EACH CHANNEL IS A BLOCK: its button + swatch + reset, with its own R/G/B row directly
    ' underneath. Clicking Fore/Back opens the colour popup for that channel; the swatch of
    ' the current colour is drawn in Paint.
    '
    ' The R/G/B rows sat together at the bottom under two caption lines saying "Foreground"
    ' and "Background". That cost two text rows the buttons above already accounted for, and
    ' overflowed the Syntax page by 67px at 175% -- caught by the height assertion added with
    ' them, on its first run. Putting each row under the button that names it removes the
    ' captions AND the ambiguity, which is what the captions were there for.
    dim as long resetW  = AfxScaleX(34)
    dim as long inhW    = AfxScaleX(34)
    dim as long swatchW = AfxScaleX(THEME_SWATCHW)
    dim as long chanW   = editW - resetW - inhW - swatchW - (3 * AfxScaleX(6))
    dim as long swatchX = xEdit + chanW + AfxScaleX(6)
    dim as long inhX    = xEdit + editW - resetW - AfxScaleX(6) - inhW

    dim as long fldH  = ctlH
    dim as long labW  = AfxScaleX(16)
    dim as long cellW = (editW - (2 * AfxScaleX(10))) \ 3      ' three equal cells, two gaps
    dim as long boxW  = cellW - labW
    dim as long xc    = xEdit
    dim rgbLabels(0 to 2) as string = { "R", "G", "B" }
    dim rgbBoxes(0 to 2)  as HWND = { ghThemeR, ghThemeG, ghThemeB }
    dim rgbBoxes2(0 to 2) as HWND = { ghThemeR2, ghThemeG2, ghThemeB2 }

    ' ---- foreground block ----
    SetWindowPos( ghThemeFore, 0, xEdit, yEdit, chanW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetRect( @gThemeSwatchRc(0), swatchX, yEdit, swatchX + swatchW, yEdit + ctlH )
    SetWindowPos( ghThemeInhF, 0, inhX, yEdit, inhW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghThemeResetF, 0, xEdit + editW - resetW, yEdit, resetW, ctlH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += ctlH + AfxScaleY(4)

    xc = xEdit
    for i as long = 0 to 2
        frmThemes_AddLabel( rgbLabels(i), xc, yEdit + AfxScaleY(5), xc + labW, yEdit + fldH )
        SetWindowPos( rgbBoxes(i), 0, xc + labW, yEdit, boxW, fldH, SWP_NOZORDER or SWP_NOACTIVATE )
        xc += cellW + AfxScaleX(10)
    next
    yEdit += fldH + rowGap

    ' ---- background block. A role has ONE colour, so the Palette page has no second one. ----
    if gThemeCurPage <> THEMEPAGE_PALETTE then
        SetWindowPos( ghThemeBack, 0, xEdit, yEdit, chanW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
        SetRect( @gThemeSwatchRc(1), swatchX, yEdit, swatchX + swatchW, yEdit + ctlH )
        SetWindowPos( ghThemeInhB, 0, inhX, yEdit, inhW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
        SetWindowPos( ghThemeResetB, 0, xEdit + editW - resetW, yEdit, resetW, ctlH, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += ctlH + AfxScaleY(4)

        xc = xEdit
        for i as long = 0 to 2
            frmThemes_AddLabel( rgbLabels(i), xc, yEdit + AfxScaleY(5), xc + labW, yEdit + fldH )
            SetWindowPos( rgbBoxes2(i), 0, xc + labW, yEdit, boxW, fldH, _
                          SWP_NOZORDER or SWP_NOACTIVATE )
            xc += cellW + AfxScaleX(10)
        next
        yEdit += fldH + rowGap
    else
        SetRect( @gThemeSwatchRc(1), 0, 0, 0, 0 )
    end if
    yEdit += rowGap

    ' STYLE SECTION -- syntax page only. Bold/Italic/Underline/Opacity are meaningless for
    ' interface colours (fore/back only), so ShowPage keeps the controls hidden and the layout
    ' skips them here.
    if gThemeCurPage = THEMEPAGE_SYNTAX then
        ' SIZE THE TOGGLE TO ITS IDEAL SIZE, which INCLUDES the focus-ring band -- a fixed size
        ' clips the ring when the control gets keyboard focus.
        dim as long togW, togH
        PsToggle_GetIdealSize( ghThemeBold, togW, togH )
        dim as long lblX = xEdit + togW + AfxScaleX(10)
        dim as long lblDY = (togH - textH) \ 2                ' vertically centre the label with the pill
        frmThemes_AddLabel( L(292, "Bold"), lblX, yEdit + lblDY, xEdit + editW, yEdit + togH )
        SetWindowPos( ghThemeBold, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += togH + rowGap
        frmThemes_AddLabel( L(293, "Italic"), lblX, yEdit + lblDY, xEdit + editW, yEdit + togH )
        SetWindowPos( ghThemeItalic, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += togH + rowGap
        frmThemes_AddLabel( L(294, "Underline"), lblX, yEdit + lblDY, xEdit + editW, yEdit + togH )
        SetWindowPos( ghThemeUnder, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += togH + rowGap + rowGap

        frmThemes_AddLabel( L(295, "Opacity"), xEdit, yEdit, xEdit + editW, yEdit + textH )
        yEdit += textH + AfxScaleY(8)
        ' Same height as the plain fields now that PsNumericUpDown insets its value cell by
        ' border + nVertPadding -- the inner PsTextBox clears the frame instead of clipping.
        SetWindowPos( ghThemeAlpha, 0, xEdit, yEdit, AfxScaleX(110), ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += ctlH
    end if

    ' The page is as tall as its tallest column.
    dim as long yBottom = yTop + listH
    if yEdit > yBottom then yBottom = yEdit

    ' HEIGHT, which nothing here was checking. Every assertion below is about WIDTH, so the
    ' second R/G/B row could have pushed the edit column off the bottom of a fixed-height
    ' dialog and reported three PASSes on the way down. A layout suite is blind to whatever
    ' it does not measure, and this one had a whole axis missing.
    if environ("TIKO_OPTIONS_SELFTEST") = "1" then
        dim as string sH
        dim as long avail = AfxScaleY(THEME_LISTHEIGHT)
        dim as long usedEdit = yEdit - yTop
        if usedEdit <= avail then sH = "PASS  " else sH = "FAIL  "
        ? sH & "theme page: the edit column fits beside the list   [edit=" & str(usedEdit) & _
            " list=" & str(avail) & " page=" & str(yBottom - yTop) & "]"
    end if

    if environ("TIKO_OPTIONS_SELFTEST") = "1" then
        dim as string sP
        if (xEdit + editW) <= contentR then sP = "PASS  " else sP = "FAIL  "
        ? sP & "theme page: the two columns fit the panel width   [right edge=" & _
            str(xEdit + editW) & " content right=" & str(contentR) & _
            "  list=" & str(listW) & " ctl=" & str(editW) & "]"
        if listW >= AfxScaleX(THEME_LISTMINW) then sP = "PASS  " else sP = "FAIL  "
        ? sP & "theme page: the key list keeps its minimum width   [listW=" & str(listW) & _
            " floor=" & str(AfxScaleX(THEME_LISTMINW)) & "]"

        ' THE NAME COLUMN, MEASURED AGAINST THE LONGEST KEY THERE ACTUALLY IS.
        '
        ' Comparing it to THEME_KEYNAMEMINW would be a TAUTOLOGY: listW is derived as
        ' furniture + that floor + scrollbar, so nameW comes back as exactly the floor every
        ' time and the check would compare a derived value with its own input. It would fire
        ' only when the listMax clamp bit, and would say nothing whatever about the actual
        ' question -- whether 170 unscaled is enough for "output.scrollbardivider".
        '
        ' So the longest key is MEASURED, in the font the row paints with. Measuring belongs
        ' in a layout; it is the paint CALLBACK that has no business taking a DC.
        dim as long nameW = listW - rowFurniture - SCROLLBAR_WIDTH_PANEL
        dim as long widest
        dim as string sWidest
        scope
            dim as HDC hdc = GetDC( hPage )
            if hdc then
                dim as HFONT hOld = cast( HFONT, SelectObject( hdc, OptionsFont_Base() ) )
                dim as SIZE sz
                for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
                    dim as wstring * 64 wsz = gThemeKeys(i).keyValue
                    if GetTextExtentPoint32W( hdc, @wsz, PsLen(wsz), @sz ) then
                        if sz.cx > widest then
                            widest  = sz.cx
                            sWidest = gThemeKeys(i).keyValue
                        end if
                    end if
                next
                SelectObject( hdc, hOld )
                ReleaseDC( hPage, hdc )
            end if
        end scope
        dim as boolean bFits = (widest > 0)
        bFits = (bFits andalso (nameW >= widest))
        if bFits then sP = "PASS  " else sP = "FAIL  "
        ? sP & "theme page: key names keep room beside the role columns   [nameW=" & _
            str(nameW) & " widest=" & str(widest) & " (" & sWidest & ")" & _
            "  furniture=" & str(rowFurniture) & "]"
    end if

    return yBottom + AfxScaleY(OPTROW_PADBOTTOM)

end function


sub frmThemes_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( theme.panel.text.backcolor )
    p->b->SetForeColor( theme.panel.text.forecolor )
    for i as long = lbound(gThemeLabels) to ubound(gThemeLabels)
        dim as RECT rc = gThemeLabels(i).rc
        p->b->PaintText( gThemeLabels(i).caption, @rc, DT_LEFT or DT_END_ELLIPSIS )
    next

    ' The current-colour swatch(es) beside the buttons.
    dim as long nb = frmThemes_SelectedBind()

    ' PALETTE PAGE: one swatch, and nb is a ROLE index -- gThemeKeys(nb) would be an
    ' unrelated key. Third place this index-space split has had to be spelled out; the
    ' selection is shared by both pages and every consumer has to say which it means.
    if gThemeCurPage = THEMEPAGE_PALETTE then
        dim as long nr = frmThemes_SelectedRole()
        if (nr >= 0) andalso (IsRectEmpty( @gThemeSwatchRc(0) ) = 0) then
            p->b->SetBackColor( Theme_RoleColor( nr ) )
            p->b->PaintRect( @gThemeSwatchRc(0) )
            p->b->SetPenColor( theme.ui.divider )
            p->b->PaintRoundOutline( @gThemeSwatchRc(0), 0, 1 )
        end if
        exit sub
    end if

    if nb >= 0 then
        dim as long n = Theme_FindEntry( gThemeKeys(nb).keyValue )
        if n >= 0 then
            if (gThemeKeys(nb).ofsFore >= 0) andalso (IsRectEmpty( @gThemeSwatchRc(0) ) = 0) then
                p->b->SetBackColor( gTheme(n).foreground )
                p->b->PaintRect( @gThemeSwatchRc(0) )
                p->b->SetPenColor( theme.ui.divider )
                p->b->PaintRoundOutline( @gThemeSwatchRc(0), 0, 1 )
            end if
            if (gThemeKeys(nb).ofsBack >= 0) andalso (IsRectEmpty( @gThemeSwatchRc(1) ) = 0) then
                p->b->SetBackColor( gTheme(n).background )
                p->b->PaintRect( @gThemeSwatchRc(1) )
                p->b->SetPenColor( theme.ui.divider )
                p->b->PaintRoundOutline( @gThemeSwatchRc(1), 0, 1 )
            end if
        end if
    end if
end sub


' ========================================================================================
' Called every time the dialog opens. The controls themselves are destroyed with the scroll
' panel, so everything here is state that must not survive one visit to the next.
' ========================================================================================
sub frmThemes_Reset()
    ' Nothing to destroy: the picker popup lives only for the duration of DoModalForRect.
    ghThemeKeys   = 0
    ghThemeFore   = 0
    ghThemeBack   = 0
    ghThemeBold   = 0
    ghThemeItalic = 0
    ghThemeUnder  = 0
    ghThemeAlpha  = 0
    ghThemeResetF = 0
    ghThemeResetB = 0
    ghThemeInhF   = 0 : ghThemeInhB = 0
    ghThemeR      = 0
    ghThemeR2     = 0 : ghThemeG2 = 0 : ghThemeB2 = 0
    ghThemeG      = 0
    ghThemeB      = 0
    gThemeBuilt   = false
    gThemeEdited  = false
    gThemeCurPage = THEMEPAGE_NONE
    erase gThemeRowBind
    erase gThemeLabels
end sub
