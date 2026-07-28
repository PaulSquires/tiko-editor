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
dim shared ghThemeResetF    as HWND      ' PsButton -- undo this key's foreground
dim shared ghThemeResetB    as HWND      ' PsButton -- undo this key's background
' R/G/B numeric entry for the CURRENT channel's colour. The large colour chooser now lives in a
' popup (frmThemes_OpenColorPopup); these stay on the dialog. ghThemePicker is kept but HIDDEN --
' it is the current-colour model the popup seeds from and these fields read.
dim shared ghThemeR         as HWND      ' PsTextBox
dim shared ghThemeG         as HWND      ' PsTextBox
dim shared ghThemeB         as HWND      ' PsTextBox
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
    return (lcase(left(wszShortFilename, 8)) = "default_")
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

    dim as DWSTRING wszDir = AfxGetExePathName & "settings\themes\"

    for i as long = 1 to 99
        dim as string sSuffix = "_custom"
        if i > 1 then sSuffix &= str(i)
        dim as DWSTRING wszCandidate = sBase & sSuffix & ".theme"
        if AfxFileExists( wszDir & wszCandidate ) = 0 then return wszCandidate
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
    dim as string sNamespace = AfxStrParse( sKey, 1, "." )
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

    dim as string ns = AfxStrParse( sKey, 1, "." )

    if ns <> "editor" then
        select case ns
            case "main":      return THEMECAT_MAIN
            case "panel":     return THEMECAT_PANEL
            case "toptabs":   return THEMECAT_TOPTABS
            case "menubar":   return THEMECAT_MENUBAR
            case "topmenu":   return THEMECAT_POPUP
            case "statusbar": return THEMECAT_STATUSBAR
            case "output":    return THEMECAT_OUTPUT
            case "compile":   return THEMECAT_COMPILE
            case "ui":        return THEMECAT_SHARED
        end select
        return THEMECAT_OTHER
    end if

    select case AfxStrParse( sKey, 2, "." )
        case "text", "comments", "keyword", "keyword2", "keyword3", _
             "numbers", "operators", "preprocessor", "strings"
            return THEMECAT_SYNTAX
        case "caret", "currentline", "selection", "indentguides"
            return THEMECAT_CURSOR
        case "linenumbers", "foldmargin", "foldsymbol"
            return THEMECAT_MARGINS
        case "bookmark", "breakpoint", "debugline", "bracegood", "bracebad", "occurrence"
            return THEMECAT_MARKERS
        case "scrollbar", "divider"
            return THEMECAT_EDCHROME
    end select

    return THEMECAT_OTHER

end function


' The localized heading for a category id. Display only -- nothing branches on this.
function frmThemes_CategoryTitle( byval nCat as long ) as DWSTRING
    select case nCat
        case THEMECAT_MAIN:      return L(465, "Main Window")
        case THEMECAT_PANEL:     return L(466, "Side Panels")
        case THEMECAT_TOPTABS:   return L(467, "Document Tabs")
        case THEMECAT_MENUBAR:   return L(468, "Menu Bar")
        case THEMECAT_POPUP:     return L(469, "Menus and Popups")
        case THEMECAT_STATUSBAR: return L(470, "Status Bar")
        case THEMECAT_OUTPUT:    return L(471, "Output Pane")
        case THEMECAT_COMPILE:   return L(472, "Build Indicators")
        case THEMECAT_SHARED:    return L(473, "Shared Chrome")
        case THEMECAT_SYNTAX:    return L(475, "Text and Syntax")
        case THEMECAT_CURSOR:    return L(476, "Cursor and Selection")
        case THEMECAT_MARGINS:   return L(477, "Margins and Folding")
        case THEMECAT_MARKERS:   return L(478, "Markers and Highlights")
        case THEMECAT_EDCHROME:  return L(479, "Editor Chrome")
    end select
    return L(474, "Other")
end function


' ========================================================================================
' Tooltip: the actual RGB values behind the swatches. The callback is per-ROW, not per-cell,
' so both channels are reported at once rather than asking the user to hover a 30px square
' precisely -- which is easier to read anyway when comparing a pair.
' ========================================================================================
private function frmThemes_ListTooltip( byval hList as HWND, byval row as integer ) as DWSTRING

    if (row < lbound(gThemeRowBind)) orelse (row > ubound(gThemeRowBind)) then return ""
    dim as long nb = gThemeRowBind(row)
    if nb < 0 then return ""                      ' a group header

    dim as long n = Theme_FindEntry( gThemeBind(nb).keyValue )
    if n < 0 then return ""

    dim as DWSTRING wszText = gThemeBind(nb).keyValue

    if gThemeBind(nb).pFore <> 0 then
        dim as long nA = -1
        if gTheme(n).hasForeAlpha then nA = clng(gTheme(n).foregroundAlpha)
        wszText &= chr(13) & L(455,"Foreground") & ": " & Theme_FormatColor( gTheme(n).foreground, nA )
    end if
    if gThemeBind(nb).pBack <> 0 then
        dim as long nA = -1
        if gTheme(n).hasBackAlpha then nA = clng(gTheme(n).backgroundAlpha)
        wszText &= chr(13) & L(456,"Background") & ": " & Theme_FormatColor( gTheme(n).background, nA )
    end if

    return wszText

end function


private sub frmThemes_PopulateList()

    if ghThemeKeys = 0 then exit sub

    Theme_BuildBindTable()

    PsListTree_Clear( ghThemeKeys )
    erase gThemeRowBind

    ' A FLAT list, no group headers -- rows are appended in bind-table order, which the engine
    ' already groups by namespace, so related keys still sit together without a divider row
    ' between them.
    dim as long nRow = -1
    dim as long nFirstSelectable = -1

    for i as long = lbound(gThemeBind) to ubound(gThemeBind)

        dim as string sKey = gThemeBind(i).keyValue
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
' The bind row behind the currently selected list row, or -1.
' ========================================================================================
private function frmThemes_SelectedBind() as long
    if ghThemeKeys = 0 then return -1
    dim as long row = PsListTree_GetCurSel( ghThemeKeys )
    if (row < lbound(gThemeRowBind)) orelse (row > ubound(gThemeRowBind)) then return -1
    return gThemeRowBind(row)
end function


' ========================================================================================
' Enable/disable and reload the edit controls for whatever row is selected. A key with no
' background channel gets a disabled Background button rather than a hidden one, so the
' column does not reflow every time the selection moves.
' ========================================================================================
private sub frmThemes_SyncEditControls()

    dim as long nb = frmThemes_SelectedBind()

    dim as boolean bHasFore, bHasBack, bHasFlags, bHasAlpha
    dim as long nAlpha = 255

    if nb >= 0 then
        bHasFore  = (gThemeBind(nb).pFore <> 0)
        bHasBack  = (gThemeBind(nb).pBack <> 0)
        bHasFlags = (gThemeBind(nb).pBold <> 0)
        bHasAlpha = gThemeBind(nb).bAlphaCapable

        dim as long n = Theme_FindEntry( gThemeBind(nb).keyValue )
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
        bResetF = bHasFore andalso Theme_IsChannelModified( gThemeBind(nb).keyValue, THEMECH_FORE )
        bResetB = bHasBack andalso Theme_IsChannelModified( gThemeBind(nb).keyValue, THEMECH_BACK )
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

        ' The DEFAULT border marks the active channel. It is appearance-only and survives all
        ' four moods, so it says "this is the one you are editing" without a second colour or a
        ' toggle control -- and without claiming Enter from anywhere else in the dialog.
        if ghThemeFore then PsButton_SetDefault( ghThemeFore, (gThemeChannel = THEMECH_FORE) )
        if ghThemeBack then PsButton_SetDefault( ghThemeBack, (gThemeChannel = THEMECH_BACK) )

        dim as COLORREF clrEdit = 0
        dim as boolean bCanEdit = false
        if nb >= 0 then
            dim as long n2 = Theme_FindEntry( gThemeBind(nb).keyValue )
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

        ' The palette tab appears only when this theme actually defines named colours. A tab that
        ' is always there and usually empty teaches the user to ignore it.
        PsColorPicker_ShowPaletteTab( ghThemePicker, (Theme_MacroCount() > 0) )
        EnableWindow( ghThemePicker, bCanEdit )

        ' Mirror the current colour into the R/G/B fields. SetText is silent, so no recursion.
        if ghThemeR then PsTextBox_SetText( ghThemeR, str(GetRValue(clrEdit)) )
        if ghThemeG then PsTextBox_SetText( ghThemeG, str(GetGValue(clrEdit)) )
        if ghThemeB then PsTextBox_SetText( ghThemeB, str(GetBValue(clrEdit)) )
        if ghThemeR then EnableWindow( ghThemeR, bCanEdit )
        if ghThemeG then EnableWindow( ghThemeG, bCanEdit )
        if ghThemeB then EnableWindow( ghThemeB, bCanEdit )
    end if

    ' Swatches live in the list rows AND beside the Fore/Back buttons (painted on the page), so a
    ' selection change has to repaint both.
    AfxRedrawWindow( ghThemeKeys )
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

    dim as DWSTRING wszMsg = L(452, "This is a built-in theme and cannot be modified.") & _
                             chr(13) & chr(13) & _
                             L(453, "Your changes will be saved as a new theme:") & _
                             chr(13) & wszFork

    if frmThemes_ThemedMsgBox( wszMsg, L(486, "Themes"), _
                                MBX_ICON_QUESTION, MBX_BTN_OKCANCEL ) <> IDOK then return false

    dim as DWSTRING wszPath = AfxGetExePathName & "settings\themes\" & wszFork
    dim as DWSTRING wszDesc = ghGeneral.description & " (custom)"

    if Theme_WriteFile( wszPath, wszDesc ) then
        frmThemes_ThemedMsgBox( L(454, "The new theme file could not be written."), _
                                 L(486, "Themes"), MBX_ICON_ERROR, MBX_BTN_OK )
        return false
    end if

    ' Everything now points at the fork: the engine (so Theme_IsDark and any later save use
    ' it) and the staging copy (so OK persists it to gConfig).
    gThemeCurrentShortFilename = wszFork
    gThemesWorkFile = wszFork
    ghGeneral.description = wszDesc

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
    dim as DWSTRING wszUserDesc = ghGeneral.description
    dim as boolean  bUserDesc   = gThemesDescEdited

    if frmThemes_EnsureForked() = false then return false   ' declined the fork -> do not save
    if bUserDesc then ghGeneral.description = wszUserDesc

    dim as DWSTRING wszPath = AfxGetExePathName & "settings\themes\" & gThemesWorkFile
    if Theme_WriteFile( wszPath, ghGeneral.description ) then
        frmThemes_ThemedMsgBox( L(454,"The new theme file could not be written."), _
                                L(486,"Themes"), MBX_ICON_ERROR, MBX_BTN_OK )
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
dim shared ghThemeColorPopup  as HWND      ' the WS_POPUP window
dim shared ghThemePopupPicker as HWND      ' the selection-only picker inside it
dim shared gThemePopupOpener  as HWND      ' the button that opened it (reopen-guard target)
dim shared gThemePopupGuard   as boolean   ' one-shot: swallow the reopen click on the opener

' Forward -- these are all defined LATER in this file, but the popup functions above them here
' call them (FB is single-pass).
declare sub      frmThemes_PickerChanged( byval hCtrl as HWND, byval clr as COLORREF, _
                                          byval nAlpha as ubyte, byval nPhase as long )
declare function frmThemes_PaletteCount( byval hCtrl as HWND ) as long
declare function frmThemes_PaletteEntry( byval hCtrl as HWND, byval idx as long, _
                                         byref wszName as DWSTRING, byref clr as COLORREF ) as boolean
declare sub      frmThemes_PaletteAssign( byval hCtrl as HWND, byval idx as long )
declare sub      frmThemes_CloseColorPopup()
declare sub      frmThemes_OpenColorPopup( byval nChannel as long, byval hOpener as HWND )


' Build a PSCOLORPICKER_COLORS from the current dialog theme. Shared by the embedded picker and
' the popup picker so their chrome cannot drift.
private sub frmThemes_FillPickerColors( byval pc as PSCOLORPICKER_COLORS ptr )
    if pc = 0 then exit sub
    pc->BackColor           = ghPanel.BackColor
    pc->ForeColor           = ghPanel.ForeColor
    pc->BorderColor         = ghEditor.Divider
    pc->TabBackColor        = ghPanel.BackColor
    pc->TabBackColorSel     = ghPanel.BackColorHot
    pc->TabForeColor        = ghPanel.ForeColor
    pc->TabForeColorSel     = ghPanel.ForeColorHot
    pc->FieldBackColor      = ghEditor.BackColorText
    pc->FieldForeColor      = ghEditor.ForeColorText
    pc->FieldBorderColor    = ghEditor.Divider
    pc->FieldBorderColorSel = ghUI.AccentColor
    pc->SelRingColor        = ghUI.AccentColor
    pc->FocusRingColor      = ghUI.AccentColor
    ' HotRingColor stays at its white default: it is drawn ON a swatch of an arbitrary colour.
end sub


' The popup picker's colour changed. Route through the SAME PickerChanged that the embedded
' picker uses (fork, apply, sync -- one path), then close on the committing END.
private sub frmThemes_PopupPickerChanged( byval hCtrl as HWND, byval clr as COLORREF, _
                                          byval nAlpha as ubyte, byval nPhase as long )
    frmThemes_PickerChanged( hCtrl, clr, nAlpha, nPhase )
    if nPhase = CLR_PHASE_END then frmThemes_CloseColorPopup()
end sub


private function frmThemes_ColorPopupWndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                              byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
    case WM_MOUSEACTIVATE
        ' Never take activation -- the dialog keeps focus, the family's popup contract.
        return MA_NOACTIVATE
    case WM_ERASEBKGND
        return true
    case WM_PAINT
        ' The client is one pixel larger than the picker on every side; filling it in the divider
        ' colour is what draws the 1px frame around the inset picker.
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )
        dim as RECT rc : GetClientRect( hwnd, @rc )
        b.SetBackColor( ghEditor.Divider )
        b.PaintRect( @rc )
        b.EndDoubleBuffer()
        return 0
    case WM_NCDESTROY
        dim as CWindow ptr pW = AfxCWindowPtr( hwnd )
        if pW then delete(pW)
        return 0
    end select
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


private sub frmThemes_EnsureColorPopup()
    if ghThemeColorPopup then exit sub

    dim pW as CWindow ptr = new CWindow
    pW->DPI = AfxCWindowOwnerPtr( HWND_FRMTHEMES )->DPI
    ghThemeColorPopup = pW->Create( HWND_FRMTHEMES, "", @frmThemes_ColorPopupWndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_TOOLWINDOW or WS_EX_NOACTIVATE )
    pW->ClassStyle = CS_DROPSHADOW

    ghThemePopupPicker = PsColorPicker_Create( ghThemeColorPopup, IDC_FRMTHEMES_PICKER2 )
    PsColorPicker_SetSelectionOnly( ghThemePopupPicker, true )
    PsColorPicker_SetColorChangedCallback( ghThemePopupPicker, @frmThemes_PopupPickerChanged )
    PsColorPicker_SetPaletteCallbacks( ghThemePopupPicker, @frmThemes_PaletteCount, _
                                       @frmThemes_PaletteEntry, @frmThemes_PaletteAssign )
    PsColorPicker_SetAssignText( ghThemePopupPicker, L(481, "Use named color") )

    dim as PSCOLORPICKER_COLORS pc
    frmThemes_FillPickerColors( @pc )
    PsColorPicker_SetColors( ghThemePopupPicker, @pc )
    PsColorPicker_SetFont( ghThemePopupPicker, OptionsFont_Base() )

    ' PsColorPicker_Create does NOT set WS_VISIBLE (the embedded one is shown by ShowPage). Show
    ' it once here; as a child it then follows the popup window's own show/hide.
    ShowWindow( ghThemePopupPicker, SW_SHOW )
end sub


' Destroys the popup and its picker. Called from frmThemes_Reset (dialog close), because a
' WS_POPUP is a top-level window and is NOT destroyed with the page.
sub frmThemes_DestroyColorPopup()
    if ghThemeColorPopup then DestroyWindow( ghThemeColorPopup )
    ghThemeColorPopup  = 0
    ghThemePopupPicker = 0
    gThemePopupOpener  = 0
    gThemePopupGuard   = false
end sub


sub frmThemes_CloseColorPopup()
    if ghThemeColorPopup = 0 then exit sub
    if IsWindowVisible( ghThemeColorPopup ) = 0 then exit sub
    ShowWindow( ghThemeColorPopup, SW_HIDE )
end sub


private sub frmThemes_OpenColorPopup( byval nChannel as long, byval hOpener as HWND )
    frmThemes_EnsureColorPopup()
    if ghThemePopupPicker = 0 then exit sub

    ' Point the dialog at this channel. That seeds the embedded picker, which we then read to
    ' seed the popup -- one source of "the current colour" rather than two.
    frmThemes_SetChannel( nChannel )

    dim as PSCOLORPICKER_COLORS pc
    frmThemes_FillPickerColors( @pc )
    PsColorPicker_SetColors( ghThemePopupPicker, @pc )
    PsColorPicker_ShowPaletteTab( ghThemePopupPicker, (Theme_MacroCount() > 0) )
    PsColorPicker_SetTab( ghThemePopupPicker, PsColorPicker_GetTab( ghThemePicker ) )
    PsColorPicker_SetColor( ghThemePopupPicker, PsColorPicker_GetColor( ghThemePicker ), 255, false )

    ' Size the popup to the picker's ideal size plus a 1px frame on every side.
    dim as long pw, ph
    PsColorPicker_GetIdealSize( ghThemePopupPicker, pw, ph )
    dim as long bd = 1
    SetWindowPos( ghThemePopupPicker, 0, bd, bd, pw, ph, SWP_NOZORDER or SWP_NOACTIVATE )
    dim as long popW = pw + (2 * bd)
    dim as long popH = ph + (2 * bd)

    ' Below the opener, clamped to the monitor work area; flipped above if it will not fit below.
    dim as RECT rcBtn : GetWindowRect( hOpener, @rcBtn )
    dim as long px = rcBtn.left
    dim as long py = rcBtn.bottom + AfxScaleY(2)
    dim as MONITORINFO mi : mi.cbSize = sizeof(MONITORINFO)
    if GetMonitorInfo( MonitorFromWindow( HWND_FRMTHEMES, MONITOR_DEFAULTTONEAREST ), @mi ) then
        if px + popW > mi.rcWork.right then px = mi.rcWork.right - popW
        if px < mi.rcWork.left then px = mi.rcWork.left
        if py + popH > mi.rcWork.bottom then py = rcBtn.top - popH - AfxScaleY(2)
        if py < mi.rcWork.top then py = mi.rcWork.top
    end if

    SetWindowPos( ghThemeColorPopup, HWND_TOP, px, py, popW, popH, SWP_NOACTIVATE )
    ShowWindow( ghThemeColorPopup, SW_SHOWNOACTIVATE )
    PsColorPicker_Refresh( ghThemePopupPicker )
    gThemePopupOpener = hOpener
end sub


' Escape and outside-click dismissal. Runs from frmThemes' pump, ahead of its own Escape
' handling, so Escape closes the popup rather than backing out of the editor.
function frmThemes_ColorPopupFilterMessage( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if ghThemeColorPopup = 0 then return false
    if IsWindowVisible( ghThemeColorPopup ) = 0 then return false

    select case pMsg->message
    case WM_KEYDOWN
        if pMsg->wParam = VK_ESCAPE then
            frmThemes_CloseColorPopup()
            return true      ' consume, so the dialog does not ALSO act on Escape
        end if
    case WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_NCLBUTTONDOWN, WM_MBUTTONDOWN
        dim as boolean bInPopup = (pMsg->hwnd = ghThemeColorPopup) orelse _
                                  (IsChild( ghThemeColorPopup, pMsg->hwnd ) <> 0)
        if bInPopup = false then
            ' A click on the OPENER button -- or on the PAGE, where the clickable swatches are
            ' painted -- would close-then-reopen (PsComboBox's trap); the guard lets that click's
            ' handler swallow this one.
            dim as boolean bOnOpener = (gThemePopupOpener <> 0) andalso _
                ((pMsg->hwnd = gThemePopupOpener) orelse (IsChild( gThemePopupOpener, pMsg->hwnd ) <> 0))
            dim as boolean bOnPage = (pMsg->hwnd = HWND_FRMTHEMES_PAGE)
            frmThemes_CloseColorPopup()
            if bOnOpener orelse bOnPage then gThemePopupGuard = true
        end if
        return false         ' let the click still reach whatever it landed on
    end select

    return false
end function


' A click on the page (page client coords). If it hit a Foreground/Background colour swatch,
' open the popup for that channel -- anchored to the matching button so it lands in the same
' place a button click would. Returns TRUE if it was a swatch.
function frmThemes_SwatchClick( byval x as long, byval y as long ) as boolean
    dim as boolean bGuarded = gThemePopupGuard
    gThemePopupGuard = false

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then return false

    dim as POINT pt : pt.x = x : pt.y = y
    dim as long  nChan   = -1
    dim as HWND  hAnchor = 0
    if (gThemeBind(nb).pFore <> 0) andalso (PtInRect( @gThemeSwatchRc(0), pt ) <> 0) then
        nChan = THEMECH_FORE : hAnchor = ghThemeFore
    elseif (gThemeBind(nb).pBack <> 0) andalso (PtInRect( @gThemeSwatchRc(1), pt ) <> 0) then
        nChan = THEMECH_BACK : hAnchor = ghThemeBack
    end if
    if nChan < 0 then return false

    ' The popup was just closed by this same click (the pump filter ran first) -- don't reopen.
    if bGuarded then return true
    frmThemes_OpenColorPopup( nChan, hAnchor )
    return true
end function


private sub frmThemes_ForeClick( byval hButton as HWND, byval id as long )
    if gThemePopupGuard then gThemePopupGuard = false : exit sub   ' this click just closed the popup
    frmThemes_OpenColorPopup( THEMECH_FORE, hButton )
end sub

private sub frmThemes_BackClick( byval hButton as HWND, byval id as long )
    if gThemePopupGuard then gThemePopupGuard = false : exit sub
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

    dim as string sKey = gThemeBind(nb).keyValue

    ' The channel must exist on this key. The picker is disabled when it does not, but a stale
    ' notification arriving as the selection moves would otherwise write to a null binding.
    if gThemeChannel = THEMECH_FORE then
        if gThemeBind(nb).pFore = 0 then exit sub
    else
        if gThemeBind(nb).pBack = 0 then exit sub
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
    if gThemeBind(nb).bAlphaCapable then nA = clng( nAlpha )

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

    dim as ulong nMask = Theme_AssignMacro( gThemeBind(nb).keyValue, gThemeChannel, wszName )
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

    dim as ulong nMask = Theme_ResetChannel( gThemeBind(nb).keyValue, nChannel )
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

    dim as ulong nMask = Theme_SetFlag( gThemeBind(nb).keyValue, nChannel, iif(bChecked, 1, 0) )
    gThemeEdited = true
    Theme_RequestApply( nMask )

end sub


' ========================================================================================
' Alpha. Only ever enabled for a key whose alpha reaches a pixel -- see Theme_IsAlphaCapable.
' ========================================================================================
private sub frmThemes_AlphaChanged( byval hCtrl as HWND, byval dblValue as double )

    dim as long nb = frmThemes_SelectedBind()
    if nb < 0 then exit sub
    if gThemeBind(nb).bAlphaCapable = false then exit sub

    dim as string sKey = gThemeBind(nb).keyValue
    dim as long n = Theme_FindEntry( sKey )
    if n < 0 then exit sub

    if frmThemes_EnsureForked() = false then exit sub

    ' Apply the alpha to whichever channel this key actually carries it on.
    dim as long nChannel = iif( gThemeBind(nb).pBack <> 0, THEMECH_BACK, THEMECH_FORE )
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

    dim as boolean bHasChan = iif( gThemeChannel = THEMECH_FORE, _
                                   (gThemeBind(nb).pFore <> 0), (gThemeBind(nb).pBack <> 0) )
    if bHasChan = false then exit sub

    dim as long r = frmThemes_ClampByte( val( PsTextBox_GetText( ghThemeR ) ) )
    dim as long g = frmThemes_ClampByte( val( PsTextBox_GetText( ghThemeG ) ) )
    dim as long b = frmThemes_ClampByte( val( PsTextBox_GetText( ghThemeB ) ) )
    dim as COLORREF clr = RGB( r, g, b )

    if frmThemes_EnsureForked() = false then exit sub     ' defaults are blocked from Edit -> a no-op

    dim as ulong nMask = Theme_SetColor( gThemeBind(nb).keyValue, gThemeChannel, clr, -1 )
    gThemeEdited = true
    Theme_RequestApply( nMask )

    ' Keep the hidden colour model (the popup's seed) and the row swatches in step -- NOT the fields.
    if ghThemePicker then PsColorPicker_SetColor( ghThemePicker, clr, 255, true )
    AfxRedrawWindow( ghThemeKeys )
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
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
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
        p->b->SetForeColor( ghPanel.ForeColorHot )
        p->b->PaintText( p->wszCaption, @rcHdr, DT_LEFT or DT_END_ELLIPSIS )
        exit sub
    end if

    dim as long n = Theme_FindEntry( gThemeBind(nb).keyValue )
    if n < 0 then exit sub

    dim as long sw = AfxScaleX(THEME_SWATCHW)
    dim as long sh = AfxScaleY(THEME_SWATCHH)
    dim as long gap = AfxScaleX(6)
    dim as long biuW = AfxScaleX(50)

    ' Lay the right-hand furniture out from the right edge inwards.
    dim as long xRight = p->rc.right - AfxScaleX(8)
    dim as RECT rcBiu = ( xRight - biuW, p->rc.top, xRight, p->rc.bottom )
    dim as long xBack = rcBiu.left - gap - sw
    dim as long xFore = xBack - gap - sw
    dim as long yMid  = p->rc.top + ((p->rc.bottom - p->rc.top) - sh) \ 2

    ' Key name.
    dim as RECT rcText = ( p->rc.left + AfxScaleX(6), p->rc.top, xFore - gap, p->rc.bottom )
    p->b->PaintText( gThemeBind(nb).keyValue, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    ' Swatches. Outlined in the divider colour so a swatch the same shade as the row behind
    ' it is still visibly a swatch rather than a hole.
    if gThemeBind(nb).pFore <> 0 then
        dim as RECT rcF = ( xFore, yMid, xFore + sw, yMid + sh )
        p->b->SetBackColor( gTheme(n).foreground )
        p->b->PaintRect( @rcF )
        p->b->SetPenColor( ghUI.Divider )
        p->b->PaintRoundOutline( @rcF, 0, 1 )
    end if
    if gThemeBind(nb).pBack <> 0 then
        dim as RECT rcB = ( xBack, yMid, xBack + sw, yMid + sh )
        p->b->SetBackColor( gTheme(n).background )
        p->b->PaintRect( @rcB )
        p->b->SetPenColor( ghUI.Divider )
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
    PsButton_SetText( ghThemeFore, L(455, "Foreground") & "..." )
    PsButton_SetClickCallback( ghThemeFore, @frmThemes_ForeClick )

    ghThemeBack = PsButton_Create( hPage, IDC_FRMTHEMES_CMDBACK )
    PsButton_SetText( ghThemeBack, L(456, "Background") & "..." )
    PsButton_SetClickCallback( ghThemeBack, @frmThemes_BackClick )

    ' Undo glyph (Segoe Fluent Icons E7A7) plus the caption. The glyph font is a SEPARATE
    ' paint-time input from the text font -- see PsButton_SetGlyphFont.
    ghThemeResetF = PsButton_Create( hPage, IDC_FRMTHEMES_CMDRESETF )
    PsButton_SetGlyphLeft( ghThemeResetF, wchr(&hE7A7) )
    PsButton_SetTooltipText( ghThemeResetF, L(480, "Reset") )
    PsButton_SetClickCallback( ghThemeResetF, @frmThemes_ResetForeClick )

    ghThemeResetB = PsButton_Create( hPage, IDC_FRMTHEMES_CMDRESETB )
    PsButton_SetTooltipText( ghThemeResetB, L(480, "Reset") )
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
    PsColorPicker_SetPaletteCallbacks( ghThemePicker, @frmThemes_PaletteCount, _
                                       @frmThemes_PaletteEntry, @frmThemes_PaletteAssign )
    ' Every word the user reads is supplied by the host: the control does no localization.
    PsColorPicker_SetAssignText( ghThemePicker, L(481, "Use named color") )
    PsColorPicker_SetPreviewText( ghThemePicker, L(482, "Initial"), L(483, "Current") )

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
    dim tabOrder(0 to 11) as HWND = { ghThemeKeys, ghThemeFore, ghThemeResetF, _
                                      ghThemeBack, ghThemeResetB, ghThemeR, ghThemeG, ghThemeB, _
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

    PsListTree_SetBackColor( ghThemeKeys, ghPanel.BackColor )
    PsListTree_SetScrollBarColors( ghThemeKeys, ghPanel.BackColorScrollBar, _
                                  ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
    PsListTree_SetFont( ghThemeKeys, OptionsFont_Base() )
    PsListTree_SetBorderWidth( ghThemeKeys, 1 )
    PsListTree_SetBorderColor( ghThemeKeys, ghEditor.Divider )

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

    dim as PSTOGGLE_COLORS tc : OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghThemeBold, @tc )   : PsToggle_Refresh( ghThemeBold )
    PsToggle_SetColors( ghThemeItalic, @tc ) : PsToggle_Refresh( ghThemeItalic )
    PsToggle_SetColors( ghThemeUnder, @tc )  : PsToggle_Refresh( ghThemeUnder )

    dim as PSNUMERICUPDOWN_COLORS nc : OptionsTheme_FillNumeric( @nc )
    PsNumericUpDown_SetColors( ghThemeAlpha, @nc )
    PsNumericUpDown_SetFont( ghThemeAlpha, OptionsFont_Base() )
    PsNumericUpDown_Refresh( ghThemeAlpha )

    ' The R/G/B fields as editor-coloured text boxes.
    dim hRGB(0 to 2) as HWND = { ghThemeR, ghThemeG, ghThemeB }
    for i as long = 0 to 2
        if hRGB(i) then
            PsTextBox_SetFont( hRGB(i), OptionsFont_Base() )
            PsTextBox_SetForeColor( hRGB(i), ghEditor.ForeColorText )
            PsTextBox_SetBackColor( hRGB(i), ghEditor.BackColorText )
            PsTextBox_SetOuterBackColor( hRGB(i), ghPanel.BackColor )
            PsTextBox_SetBorderColor( hRGB(i), ghEditor.Divider )
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
    if ghThemePopupPicker then
        PsColorPicker_SetColors( ghThemePopupPicker, @pc )
        PsColorPicker_SetFont( ghThemePopupPicker, OptionsFont_Base() )
        PsColorPicker_Refresh( ghThemePopupPicker )
    end if

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
    ShowWindow( ghThemeKeys, nCmd )
    ShowWindow( ghThemeFore, nCmd )
    ShowWindow( ghThemeBack, nCmd )
    ShowWindow( ghThemeResetF, nCmd )
    ShowWindow( ghThemeResetB, nCmd )
    ShowWindow( ghThemeBold, nStyle )
    ShowWindow( ghThemeItalic, nStyle )
    ShowWindow( ghThemeUnder, nStyle )
    ShowWindow( ghThemeAlpha, nStyle )
    ShowWindow( ghThemeR, nCmd )
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
    dim as long ctlH     = AfxScaleY(28)
    dim as long listH    = AfxScaleY(THEME_LISTHEIGHT)

    ' A key list on the left (the names are short) and the controls beside it.
    dim as long editW = AfxScaleX(300)
    dim as long listW = AfxScaleX(THEME_LISTW)
    if listW < AfxScaleX(THEME_LISTMINW) then listW = AfxScaleX(THEME_LISTMINW)

    dim as long yTop = y
    SetWindowPos( ghThemeKeys, 0, padLeft, yTop, listW, listH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long xEdit = padLeft + listW + gapX
    dim as long yEdit = yTop

    ' Foreground / Background, each with a Reset glyph button beside it. Clicking Fore/Back opens
    ' the colour popup for that channel; the swatch of the current colour is drawn in Paint.
    ' The colour swatch is the SAME HEIGHT as the buttons (it reads as part of the row).
    dim as long resetW  = AfxScaleX(34)
    dim as long swatchW = AfxScaleX(THEME_SWATCHW)
    dim as long chanW   = editW - resetW - swatchW - (2 * AfxScaleX(6))
    dim as long swatchX = xEdit + chanW + AfxScaleX(6)
    SetWindowPos( ghThemeFore, 0, xEdit, yEdit, chanW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetRect( @gThemeSwatchRc(0), swatchX, yEdit, swatchX + swatchW, yEdit + ctlH )
    SetWindowPos( ghThemeResetF, 0, xEdit + editW - resetW, yEdit, resetW, ctlH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += ctlH + rowGap
    SetWindowPos( ghThemeBack, 0, xEdit, yEdit, chanW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetRect( @gThemeSwatchRc(1), swatchX, yEdit, swatchX + swatchW, yEdit + ctlH )
    SetWindowPos( ghThemeResetB, 0, xEdit + editW - resetW, yEdit, resetW, ctlH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += ctlH + rowGap + rowGap

    ' R / G / B fields in one row, each a labelled box.
    dim as long fldH  = ctlH
    dim as long labW  = AfxScaleX(16)
    dim as long cellW = (editW - (2 * AfxScaleX(10))) \ 3      ' three equal cells with two gaps
    dim as long boxW  = cellW - labW
    dim as long xc    = xEdit
    dim rgbBoxes(0 to 2) as HWND = { ghThemeR, ghThemeG, ghThemeB }
    dim rgbLabels(0 to 2) as string = { "R", "G", "B" }
    for i as long = 0 to 2
        frmThemes_AddLabel( rgbLabels(i), xc, yEdit + AfxScaleY(5), xc + labW, yEdit + fldH )
        SetWindowPos( rgbBoxes(i), 0, xc + labW, yEdit, boxW, fldH, SWP_NOZORDER or SWP_NOACTIVATE )
        xc += cellW + AfxScaleX(10)
    next
    yEdit += fldH + rowGap + rowGap

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
        frmThemes_AddLabel( L(459, "Bold"), lblX, yEdit + lblDY, xEdit + editW, yEdit + togH )
        SetWindowPos( ghThemeBold, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += togH + rowGap
        frmThemes_AddLabel( L(460, "Italic"), lblX, yEdit + lblDY, xEdit + editW, yEdit + togH )
        SetWindowPos( ghThemeItalic, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += togH + rowGap
        frmThemes_AddLabel( L(461, "Underline"), lblX, yEdit + lblDY, xEdit + editW, yEdit + togH )
        SetWindowPos( ghThemeUnder, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += togH + rowGap + rowGap

        frmThemes_AddLabel( L(462, "Opacity"), xEdit, yEdit, xEdit + editW, yEdit + textH )
        yEdit += textH + AfxScaleY(4)
        SetWindowPos( ghThemeAlpha, 0, xEdit, yEdit, AfxScaleX(110), ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
        yEdit += ctlH
    end if

    ' The page is as tall as its tallest column.
    dim as long yBottom = yTop + listH
    if yEdit > yBottom then yBottom = yEdit

    if environ("TIKO_OPTIONS_SELFTEST") = "1" then
        dim as string sP
        if (xEdit + editW) <= contentR then sP = "PASS  " else sP = "FAIL  "
        ? sP & "theme page: the two columns fit the panel width   [right edge=" & _
            str(xEdit + editW) & " content right=" & str(contentR) & _
            "  list=" & str(listW) & " ctl=" & str(editW) & "]"
        if listW >= AfxScaleX(THEME_LISTMINW) then sP = "PASS  " else sP = "FAIL  "
        ? sP & "theme page: the key list keeps its minimum width   [listW=" & str(listW) & _
            " floor=" & str(AfxScaleX(THEME_LISTMINW)) & "]"
    end if

    return yBottom + AfxScaleY(OPTROW_PADBOTTOM)

end function


sub frmThemes_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( ghPanel.BackColor )
    p->b->SetForeColor( ghPanel.ForeColor )
    for i as long = lbound(gThemeLabels) to ubound(gThemeLabels)
        dim as RECT rc = gThemeLabels(i).rc
        p->b->PaintText( gThemeLabels(i).caption, @rc, DT_LEFT or DT_END_ELLIPSIS )
    next

    ' The Foreground / Background current-colour swatches beside their buttons.
    dim as long nb = frmThemes_SelectedBind()
    if nb >= 0 then
        dim as long n = Theme_FindEntry( gThemeBind(nb).keyValue )
        if n >= 0 then
            if (gThemeBind(nb).pFore <> 0) andalso (IsRectEmpty( @gThemeSwatchRc(0) ) = 0) then
                p->b->SetBackColor( gTheme(n).foreground )
                p->b->PaintRect( @gThemeSwatchRc(0) )
                p->b->SetPenColor( ghEditor.Divider )
                p->b->PaintRoundOutline( @gThemeSwatchRc(0), 0, 1 )
            end if
            if (gThemeBind(nb).pBack <> 0) andalso (IsRectEmpty( @gThemeSwatchRc(1) ) = 0) then
                p->b->SetBackColor( gTheme(n).background )
                p->b->PaintRect( @gThemeSwatchRc(1) )
                p->b->SetPenColor( ghEditor.Divider )
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
    frmThemes_DestroyColorPopup()   ' a WS_POPUP top-level; not destroyed with the page window
    ghThemeKeys   = 0
    ghThemeFore   = 0
    ghThemeBack   = 0
    ghThemeBold   = 0
    ghThemeItalic = 0
    ghThemeUnder  = 0
    ghThemeAlpha  = 0
    ghThemeResetF = 0
    ghThemeResetB = 0
    ghThemeR      = 0
    ghThemeG      = 0
    ghThemeB      = 0
    gThemeBuilt   = false
    gThemeEdited  = false
    gThemeCurPage = THEMEPAGE_NONE
    erase gThemeRowBind
    erase gThemeLabels
end sub
