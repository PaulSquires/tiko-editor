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

'  frmOptionsTheme.inc  --  see frmOptionsTheme.bi for the design.

#include once "frmOptionsTheme.bi"

' frmOptions_ThemedMsgBox is defined in frmOptions.inc, which is included AFTER this file
' (it dispatches to us, so it must come later). Its declaration already arrives early via
' frmOptions.bi, so nothing more is needed here.

' ----------------------------------------------------------------------------------------
' Controls. One set, shared by both pages: every panel-hosted page is built onto the same
' PsScrollPanel page window and switched with Show/Hide, so a second set would be dead
' weight. gThemeCurPage decides which keys the list holds.
' ----------------------------------------------------------------------------------------
dim shared ghThemeKeys      as HWND      ' PsListBox
dim shared ghThemeFore      as HWND      ' PsButton
dim shared ghThemeBack      as HWND      ' PsButton
dim shared ghThemeBold      as HWND      ' PsToggle
dim shared ghThemeItalic    as HWND      ' PsToggle
dim shared ghThemeUnder     as HWND      ' PsToggle
dim shared ghThemeAlpha     as HWND      ' PsNumericUpDown
dim shared ghThemeResetF    as HWND      ' PsButton -- undo this key's foreground
dim shared ghThemeResetB    as HWND      ' PsButton -- undo this key's background

dim shared gThemeBuilt      as boolean
dim shared gThemeCurPage    as long = OPTPAGE_NONE

' Bind-table indices for the rows currently in the list, in list order. Rebuilt whenever the
' page changes; row N of the listbox is gThemeRowBind(N).
dim shared gThemeRowBind(any) as long

' Set once any colour or flag has actually been changed, so OK knows whether it needs to
' write the theme file at all.
dim shared gThemeEdited     as boolean

' ChooseColor's 16 custom slots. Kept across invocations so a colour mixed for one key is
' still one click away for the next -- which matters a lot when building a coherent theme.
dim shared gThemeCustom(0 to 15) as COLORREF

' Drawn labels, same trick frmOptionsColors uses: captions are painted, not controls.
type THEMELABEL_TYPE
    rc      as RECT
    caption as DWSTRING
end type
dim shared gThemeLabels(any) as THEMELABEL_TYPE

' Unscaled geometry.
const THEME_LISTHEIGHT   = 380
const THEME_EDITCOLW     = 200      ' the edit column down the right-hand side
const THEME_SWATCHW      = 30
const THEME_SWATCHH      = 14
const THEME_ROWHEIGHT    = 22


function frmOptionsTheme_OwnsPage( byval nPage as long ) as boolean
    return (nPage = OPTPAGE_SYNTAX) orelse (nPage = OPTPAGE_INTERFACE)
end function


function frmOptionsTheme_WasEdited() as boolean
    return gThemeEdited
end function


' ========================================================================================
' A theme that ships with tiko must never be modified in place -- it is the thing a user
' falls back to when their own experiment goes wrong, and "reset" is only meaningful while
' the originals are pristine.
' ========================================================================================
function frmOptionsTheme_IsProtectedTheme( byval wszShortFilename as DWSTRING ) as boolean
    return (lcase(left(wszShortFilename, 8)) = "default_")
end function


' ========================================================================================
' Pick a filename for the fork. "default_dark.theme" -> "default_dark_custom.theme", then
' _custom2, _custom3... so repeatedly forking the same built-in never silently overwrites an
' earlier attempt.
' ========================================================================================
function frmOptionsTheme_MakeForkName( byval wszShortFilename as DWSTRING ) as DWSTRING

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
function frmOptionsTheme_PageForKey( byval sKey as string ) as long
    dim as string sNamespace = AfxStrParse( sKey, 1, "." )
    ' general.* is description and version text, not colour: on neither page.
    if sNamespace = "general" then return OPTPAGE_NONE
    if sNamespace = "editor" then return OPTPAGE_SYNTAX
    return OPTPAGE_INTERFACE
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
' Returns a STABLE ID, never display text. The two were originally one function returning a
' localized string, and the self-test caught why that is wrong: it runs before the
' localization table is loaded, so every L() answered alike, every category compared equal,
' and the grouping collapsed to a single band. Structure must not depend on what language the
' UI happens to be in.
function frmOptionsTheme_CategoryIdForKey( byval sKey as string ) as long

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
function frmOptionsTheme_CategoryTitle( byval nCat as long ) as DWSTRING
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
private function frmOptionsTheme_ListTooltip( byval hList as HWND, byval row as integer ) as DWSTRING

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


private sub frmOptionsTheme_PopulateList()

    if ghThemeKeys = 0 then exit sub

    Theme_BuildBindTable()

    PsListBox_Clear( ghThemeKeys )
    erase gThemeRowBind

    ' Rows are appended in bind-table order, and a group header is emitted whenever the
    ' category changes. That relies on the bind table already grouping its keys by namespace,
    ' which it does -- so no sort is needed and the list order stays the order the engine
    ' declares, which is also the order the theme file is written in.
    dim as long nLastCat = -1
    dim as long nRow = -1
    dim as long nFirstSelectable = -1

    for i as long = lbound(gThemeBind) to ubound(gThemeBind)

        dim as string sKey = gThemeBind(i).keyValue
        if frmOptionsTheme_PageForKey( sKey ) <> gThemeCurPage then continue for

        dim as long nCat = frmOptionsTheme_CategoryIdForKey( sKey )
        if nCat <> nLastCat then
            nLastCat = nCat
            nRow += 1
            redim preserve gThemeRowBind(nRow) as long
            gThemeRowBind(nRow) = -1                 ' -1 marks a header row
            PsListBox_AddHeader( ghThemeKeys, frmOptionsTheme_CategoryTitle( nCat ) )
        end if

        nRow += 1
        redim preserve gThemeRowBind(nRow) as long
        gThemeRowBind(nRow) = i
        PsListBox_AddString( ghThemeKeys, sKey, i )
        if nFirstSelectable < 0 then nFirstSelectable = nRow

    next

    if nFirstSelectable >= 0 then PsListBox_SetCurSel( ghThemeKeys, nFirstSelectable )

end sub


' ========================================================================================
' The bind row behind the currently selected list row, or -1.
' ========================================================================================
private function frmOptionsTheme_SelectedBind() as long
    if ghThemeKeys = 0 then return -1
    dim as long row = PsListBox_GetCurSel( ghThemeKeys )
    if (row < lbound(gThemeRowBind)) orelse (row > ubound(gThemeRowBind)) then return -1
    return gThemeRowBind(row)
end function


' ========================================================================================
' Enable/disable and reload the edit controls for whatever row is selected. A key with no
' background channel gets a disabled Background button rather than a hidden one, so the
' column does not reflow every time the selection moves.
' ========================================================================================
private sub frmOptionsTheme_SyncEditControls()

    dim as long nb = frmOptionsTheme_SelectedBind()

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

    ' The swatches live in the list rows, so a change has to redraw the list too.
    AfxRedrawWindow( ghThemeKeys )

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
private function frmOptionsTheme_EnsureForked() as boolean

    if frmOptionsTheme_IsProtectedTheme( gThemeCurrentShortFilename ) = false then return true

    dim as DWSTRING wszFork = frmOptionsTheme_MakeForkName( gThemeCurrentShortFilename )

    dim as DWSTRING wszMsg = L(452, "This is a built-in theme and cannot be modified.") & _
                             chr(13) & chr(13) & _
                             L(453, "Your changes will be saved as a new theme:") & _
                             chr(13) & wszFork

    if frmOptions_ThemedMsgBox( wszMsg, L(111, "Themes and Fonts"), _
                                MBX_ICON_QUESTION, MBX_BTN_OKCANCEL ) <> IDOK then return false

    dim as DWSTRING wszPath = AfxGetExePathName & "settings\themes\" & wszFork
    dim as DWSTRING wszDesc = ghGeneral.description & " (custom)"

    if Theme_WriteFile( wszPath, wszDesc ) then
        frmOptions_ThemedMsgBox( L(454, "The new theme file could not be written."), _
                                 L(111, "Themes and Fonts"), MBX_ICON_ERROR, MBX_BTN_OK )
        return false
    end if

    ' Everything now points at the fork: the engine (so Theme_IsDark and any later save use
    ' it) and the staging copy (so OK persists it to gConfig).
    gThemeCurrentShortFilename = wszFork
    gOptWork.ThemeShortFilename = wszFork
    ghGeneral.description = wszDesc

    ' The Themes and Fonts page has a stale folder listing now.
    frmOptionsColors_RefreshThemeList()

    return true

end function


' ========================================================================================
' Colour edit. THE ONE PLACE a colour is chosen -- Phase 3 swaps PsColorPicker in here and
' nothing else moves.
' ========================================================================================
private sub frmOptionsTheme_EditColor( byval nChannel as long )

    dim as long nb = frmOptionsTheme_SelectedBind()
    if nb < 0 then exit sub

    dim as string sKey = gThemeBind(nb).keyValue
    dim as long n = Theme_FindEntry( sKey )
    if n < 0 then exit sub

    dim as COLORREF clrCurrent = iif( nChannel = THEMECH_FORE, _
                                      gTheme(n).foreground, gTheme(n).background )

    dim as long lResult = AfxChooseColorDialog( HWND_FRMOPTIONS, clrCurrent, @gThemeCustom(0) )
    if lResult = -1 then exit sub                       ' cancelled

    dim as COLORREF clrNew = cast(COLORREF, lResult)
    if clrNew = clrCurrent then exit sub                ' no change, so no fork prompt

    if frmOptionsTheme_EnsureForked() = false then exit sub

    dim as ulong nMask = Theme_SetColor( sKey, nChannel, clrNew )
    gThemeEdited = true

    ' Live: the editor behind the dialog recolours on the next coalescing tick.
    Theme_RequestApply( nMask )
    frmOptionsTheme_SyncEditControls()

end sub


private sub frmOptionsTheme_ForeClick( byval hButton as HWND, byval id as long )
    frmOptionsTheme_EditColor( THEMECH_FORE )
end sub

private sub frmOptionsTheme_BackClick( byval hButton as HWND, byval id as long )
    frmOptionsTheme_EditColor( THEMECH_BACK )
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
private sub frmOptionsTheme_ResetChannel( byval nChannel as long )

    dim as long nb = frmOptionsTheme_SelectedBind()
    if nb < 0 then exit sub

    dim as ulong nMask = Theme_ResetChannel( gThemeBind(nb).keyValue, nChannel )
    if nMask = THSUB_NONE then exit sub

    Theme_RequestApply( nMask )
    frmOptionsTheme_SyncEditControls()

end sub

private sub frmOptionsTheme_ResetForeClick( byval hButton as HWND, byval id as long )
    frmOptionsTheme_ResetChannel( THEMECH_FORE )
end sub

private sub frmOptionsTheme_ResetBackClick( byval hButton as HWND, byval id as long )
    frmOptionsTheme_ResetChannel( THEMECH_BACK )
end sub


' ========================================================================================
' Bold / italic / underline. PsToggle fires this only for user action, never for
' SetChecked, so reloading the controls in _SyncEditControls cannot recurse back in here.
' ========================================================================================
private sub frmOptionsTheme_FlagChanged( byval hToggle as HWND, byval bChecked as boolean )

    dim as long nb = frmOptionsTheme_SelectedBind()
    if nb < 0 then exit sub

    dim as long nChannel = THEMECH_BOLD
    if hToggle = ghThemeItalic then nChannel = THEMECH_ITALIC
    if hToggle = ghThemeUnder  then nChannel = THEMECH_UNDER

    if frmOptionsTheme_EnsureForked() = false then
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
private sub frmOptionsTheme_AlphaChanged( byval hCtrl as HWND, byval dblValue as double )

    dim as long nb = frmOptionsTheme_SelectedBind()
    if nb < 0 then exit sub
    if gThemeBind(nb).bAlphaCapable = false then exit sub

    dim as string sKey = gThemeBind(nb).keyValue
    dim as long n = Theme_FindEntry( sKey )
    if n < 0 then exit sub

    if frmOptionsTheme_EnsureForked() = false then exit sub

    ' Apply the alpha to whichever channel this key actually carries it on.
    dim as long nChannel = iif( gThemeBind(nb).pBack <> 0, THEMECH_BACK, THEMECH_FORE )
    dim as COLORREF clr = iif( nChannel = THEMECH_FORE, gTheme(n).foreground, gTheme(n).background )

    dim as ulong nMask = Theme_SetColor( sKey, nChannel, clr, clng(dblValue) )
    gThemeEdited = true
    Theme_RequestApply( nMask )

end sub


private sub frmOptionsTheme_ListSelChange( byval hList as HWND, byval row as integer )
    frmOptionsTheme_SyncEditControls()
end sub


' ========================================================================================
' Row painting: the key name, then its foreground and background swatches, then B/I/U.
' Right-aligned so the swatches line up in a column no matter how long the key is.
' ========================================================================================
private sub frmOptionsTheme_ListPaint( byval p as PSLISTBOX_PAINTINFO ptr )

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

    ' B / I / U, shown only for the keys that carry them, and dimmed when off.
    if gThemeBind(nb).pBold <> 0 then
        dim as string sBiu = ""
        sBiu &= iif( gTheme(n).bold      <> 0, "B", "-" )
        sBiu &= iif( gTheme(n).italic    <> 0, "I", "-" )
        sBiu &= iif( gTheme(n).underline <> 0, "U", "-" )
        p->b->SetBackColor( backclr )
        p->b->SetForeColor( foreclr )
        p->b->PaintText( sBiu, @rcBiu, DT_RIGHT )
    end if

end sub


private sub frmOptionsTheme_AddLabel( byval wszCaption as DWSTRING, _
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
sub frmOptionsTheme_Build( byval nPage as long, byval hPage as HWND )

    gThemeCurPage = nPage

    if gThemeBuilt then
        frmOptionsTheme_PopulateList()
        frmOptionsTheme_SyncEditControls()
        exit sub
    end if
    gThemeBuilt = true

    ghThemeKeys = PsListBox_Create( hPage, IDC_FRMOPTIONSTHEME_LSTKEYS )
    PsListBox_SetPaintCallback( ghThemeKeys, @frmOptionsTheme_ListPaint )
    PsListBox_SetSelChangeCallback( ghThemeKeys, @frmOptionsTheme_ListSelChange )
    PsListBox_SetTooltipCallback( ghThemeKeys, @frmOptionsTheme_ListTooltip )
    PsListBox_SetRowHeight( ghThemeKeys, THEME_ROWHEIGHT )
    PsListBox_SetScrollBarWidth( ghThemeKeys, SCROLLBAR_WIDTH_PANEL )

    ghThemeFore = PsButton_Create( hPage, IDC_FRMOPTIONSTHEME_CMDFORE )
    PsButton_SetText( ghThemeFore, L(455, "Foreground") & "..." )
    PsButton_SetClickCallback( ghThemeFore, @frmOptionsTheme_ForeClick )

    ghThemeBack = PsButton_Create( hPage, IDC_FRMOPTIONSTHEME_CMDBACK )
    PsButton_SetText( ghThemeBack, L(456, "Background") & "..." )
    PsButton_SetClickCallback( ghThemeBack, @frmOptionsTheme_BackClick )

    ghThemeResetF = PsButton_Create( hPage, IDC_FRMOPTIONSTHEME_CMDRESETF )
    PsButton_SetText( ghThemeResetF, L(480, "Reset") )
    PsButton_SetClickCallback( ghThemeResetF, @frmOptionsTheme_ResetForeClick )

    ghThemeResetB = PsButton_Create( hPage, IDC_FRMOPTIONSTHEME_CMDRESETB )
    PsButton_SetText( ghThemeResetB, L(480, "Reset") )
    PsButton_SetClickCallback( ghThemeResetB, @frmOptionsTheme_ResetBackClick )

    ghThemeBold = PsToggle_Create( hPage, IDC_FRMOPTIONSTHEME_TOGBOLD )
    PsToggle_SetCheckChangedCallback( ghThemeBold, @frmOptionsTheme_FlagChanged )
    ghThemeItalic = PsToggle_Create( hPage, IDC_FRMOPTIONSTHEME_TOGITALIC )
    PsToggle_SetCheckChangedCallback( ghThemeItalic, @frmOptionsTheme_FlagChanged )
    ghThemeUnder = PsToggle_Create( hPage, IDC_FRMOPTIONSTHEME_TOGUNDER )
    PsToggle_SetCheckChangedCallback( ghThemeUnder, @frmOptionsTheme_FlagChanged )

    ghThemeAlpha = PsNumericUpDown_Create( hPage, IDC_FRMOPTIONSTHEME_NUMALPHA )
    PsNumericUpDown_SetRange( ghThemeAlpha, 0, 255 )
    PsNumericUpDown_SetDecimalPlaces( ghThemeAlpha, 0 )
    PsNumericUpDown_SetValueChangedCallback( ghThemeAlpha, @frmOptionsTheme_AlphaChanged )

    frmOptionsTheme_ApplyTheme()
    frmOptionsTheme_PopulateList()
    frmOptionsTheme_SyncEditControls()

end sub


sub frmOptionsTheme_ApplyTheme()

    if gThemeBuilt = false then exit sub

    PsListBox_SetBackColor( ghThemeKeys, ghPanel.BackColor )
    PsListBox_SetScrollBarColors( ghThemeKeys, ghPanel.BackColorScrollBar, _
                                  ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
    PsListBox_SetFont( ghThemeKeys, OptionsFont_Base() )

    dim as PSBUTTON_COLORS bc : OptionsTheme_FillButton( @bc )
    PsButton_SetColors( ghThemeFore, @bc )   : PsButton_Refresh( ghThemeFore )
    PsButton_SetColors( ghThemeBack, @bc )   : PsButton_Refresh( ghThemeBack )
    PsButton_SetColors( ghThemeResetF, @bc ) : PsButton_Refresh( ghThemeResetF )
    PsButton_SetColors( ghThemeResetB, @bc ) : PsButton_Refresh( ghThemeResetB )
    PsButton_SetFont( ghThemeFore, OptionsFont_Base() )
    PsButton_SetFont( ghThemeBack, OptionsFont_Base() )
    PsButton_SetFont( ghThemeResetF, OptionsFont_Base() )
    PsButton_SetFont( ghThemeResetB, OptionsFont_Base() )

    dim as PSTOGGLE_COLORS tc : OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghThemeBold, @tc )   : PsToggle_Refresh( ghThemeBold )
    PsToggle_SetColors( ghThemeItalic, @tc ) : PsToggle_Refresh( ghThemeItalic )
    PsToggle_SetColors( ghThemeUnder, @tc )  : PsToggle_Refresh( ghThemeUnder )

    dim as PSNUMERICUPDOWN_COLORS nc : OptionsTheme_FillNumeric( @nc )
    PsNumericUpDown_SetColors( ghThemeAlpha, @nc )
    PsNumericUpDown_SetFont( ghThemeAlpha, OptionsFont_Base() )
    PsNumericUpDown_Refresh( ghThemeAlpha )

end sub


sub frmOptionsTheme_Show( byval nPage as long, byval bShow as boolean )

    if gThemeBuilt = false then exit sub

    if bShow andalso (nPage <> gThemeCurPage) then
        gThemeCurPage = nPage
        frmOptionsTheme_PopulateList()
        frmOptionsTheme_SyncEditControls()
    end if

    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ShowWindow( ghThemeKeys, nCmd )
    ShowWindow( ghThemeFore, nCmd )
    ShowWindow( ghThemeBack, nCmd )
    ShowWindow( ghThemeResetF, nCmd )
    ShowWindow( ghThemeResetB, nCmd )
    ShowWindow( ghThemeBold, nCmd )
    ShowWindow( ghThemeItalic, nCmd )
    ShowWindow( ghThemeUnder, nCmd )
    ShowWindow( ghThemeAlpha, nCmd )

end sub


' ========================================================================================
' Layout. List down the left, edit column down the right -- the right column is where
' Phase 3's embedded PsColorPicker goes, replacing the two buttons.
' Returns the content height, as the PsScrollPanel contract requires.
' ========================================================================================
function frmOptionsTheme_Layout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long

    if gThemeBuilt = false then return 0

    erase gThemeLabels

    dim as long padLeft  = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight = AfxScaleX(OPTROW_PADRIGHT)
    dim as long y        = AfxScaleY(OPTROW_PADTOP)
    dim as long contentR = cxPanel - padRight
    dim as long editW    = AfxScaleX(THEME_EDITCOLW)
    dim as long gapX     = AfxScaleX(16)
    dim as long listW    = contentR - padLeft - editW - gapX
    if listW < AfxScaleX(180) then listW = AfxScaleX(180)     ' narrow panel: keep it usable
    dim as long listH    = AfxScaleY(THEME_LISTHEIGHT)
    dim as long textH    = AfxScaleY(18)
    dim as long rowGap   = AfxScaleY(10)
    dim as long ctlH     = AfxScaleY(28)

    frmOptionsTheme_AddLabel( iif( gThemeCurPage = OPTPAGE_SYNTAX, _
                                   L(457, "Editor syntax and margin colors"), _
                                   L(458, "Window, panel and menu colors") ), _
                              padLeft, y, contentR, y + textH )
    dim as long yTop = y + textH + AfxScaleY(6)

    SetWindowPos( ghThemeKeys, 0, padLeft, yTop, listW, listH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' Edit column.
    dim as long xEdit = padLeft + listW + gapX
    dim as long yEdit = yTop

    ' Each colour button gets a Reset beside it, sharing the column width.
    dim as long resetW = AfxScaleX(62)
    dim as long pickW  = editW - resetW - AfxScaleX(6)
    SetWindowPos( ghThemeFore, 0, xEdit, yEdit, pickW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghThemeResetF, 0, xEdit + pickW + AfxScaleX(6), yEdit, resetW, ctlH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += ctlH + rowGap
    SetWindowPos( ghThemeBack, 0, xEdit, yEdit, pickW, ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghThemeResetB, 0, xEdit + pickW + AfxScaleX(6), yEdit, resetW, ctlH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += ctlH + rowGap * 2

    dim as long togW = AfxScaleX(46)
    dim as long togH = AfxScaleY(22)
    frmOptionsTheme_AddLabel( L(459, "Bold"), xEdit + togW + AfxScaleX(10), yEdit + AfxScaleY(2), _
                              xEdit + editW, yEdit + togH )
    SetWindowPos( ghThemeBold, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += togH + rowGap
    frmOptionsTheme_AddLabel( L(460, "Italic"), xEdit + togW + AfxScaleX(10), yEdit + AfxScaleY(2), _
                              xEdit + editW, yEdit + togH )
    SetWindowPos( ghThemeItalic, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += togH + rowGap
    frmOptionsTheme_AddLabel( L(461, "Underline"), xEdit + togW + AfxScaleX(10), yEdit + AfxScaleY(2), _
                              xEdit + editW, yEdit + togH )
    SetWindowPos( ghThemeUnder, 0, xEdit, yEdit, togW, togH, SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += togH + rowGap * 2

    frmOptionsTheme_AddLabel( L(462, "Opacity"), xEdit, yEdit, xEdit + editW, yEdit + textH )
    yEdit += textH + AfxScaleY(4)
    SetWindowPos( ghThemeAlpha, 0, xEdit, yEdit, AfxScaleX(110), ctlH, SWP_NOZORDER or SWP_NOACTIVATE )
    yEdit += ctlH

    dim as long yBottom = yTop + listH
    if yEdit > yBottom then yBottom = yEdit

    return yBottom + AfxScaleY(OPTROW_PADBOTTOM)

end function


sub frmOptionsTheme_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( ghPanel.BackColor )
    p->b->SetForeColor( ghPanel.ForeColor )
    for i as long = lbound(gThemeLabels) to ubound(gThemeLabels)
        dim as RECT rc = gThemeLabels(i).rc
        p->b->PaintText( gThemeLabels(i).caption, @rc, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub


' ========================================================================================
' Called every time the dialog opens. The controls themselves are destroyed with the scroll
' panel, so everything here is state that must not survive one visit to the next.
' ========================================================================================
sub frmOptionsTheme_Reset()
    ghThemeKeys   = 0
    ghThemeFore   = 0
    ghThemeBack   = 0
    ghThemeBold   = 0
    ghThemeItalic = 0
    ghThemeUnder  = 0
    ghThemeAlpha  = 0
    ghThemeResetF = 0
    ghThemeResetB = 0
    gThemeBuilt   = false
    gThemeEdited  = false
    gThemeCurPage = OPTPAGE_NONE
    erase gThemeRowBind
    erase gThemeLabels
end sub
