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

'  frmOptionsCompiler.inc  --  the Compiler Setup page (see frmOptionsCompiler.bi).
'
'  A hand-written page hosted directly on the Options scroll panel. Controls
'  come from the reusable family; every value goes through gOptWork, never gConfig.

#include once "frmOptionsCompiler.bi"
#include once "modOptionsRows.bi"     ' gOptWork, shared padding, theme helpers
#include once "modRoutines.bi"        ' AfxIFileOpenDialogW, ProcessFromCurdriveApp
#include once "clsConfig.bi"


' Fixed heights, UNSCALED (run through AfxScaleY at use).
' MEASURED AGAINST THE FIXED VIEWPORT. The page needed 948px of the 892 available at 175%, so
' 32 unscaled px had to come out of it; 30 of them come from here and the rest from the section
' gap below. The toolchain list loses about one visible row, which is the cheapest thing on this
' page to give up -- the two long single-line fields under it cannot shrink and the toggles are
' already at row height.
#define COMP_LISTHEIGHT     120
#define COMP_FIELDPAD        11      ' vertical padding inside a single-line PsTextBox
#define COMP_LABELGAP         4      ' between a field label and the field under it
#define COMP_SECTIONGAP      11      ' between one field block and the next


' One drawn label on the page. The caption is composed at layout time (some carry a
' trailing colon, some do not -- matching the old dialog exactly) and drawn in Paint.


dim shared ghCompToolchains as HWND      ' PsListTree
dim shared ghCompSwitches   as HWND      ' PsTextBox
dim shared ghCompIncludes   as HWND      ' PsTextBox
dim shared ghCompRunCmd     as HWND      ' PsToggle
dim shared ghCompBeep       as HWND      ' PsToggle

' Clean toolchain folder names, parallel to the PsListTree rows. The row's display TEXT may
' carry a "path contains spaces" warning suffix, so the folder name is kept separately
' rather than parsed back out of the list.
dim shared gCompToolchainNames(any) as DWSTRING

dim shared gCompLabels as PAGELABELLIST
dim shared gCompBuilt as boolean = false


' ========================================================================================
' This module owns exactly the Compiler Setup page.
' ========================================================================================
function frmOptionsCompiler_OwnsPage( byval nPage as long ) as boolean
    function = iif( nPage = OPTPAGE_COMPILER, true, false )
end function


' ========================================================================================
' Forget all state. The controls live on the scroll panel's page window and are destroyed
' with it when the dialog closes, so their handles are stale on the next open -- this must
' run before a fresh dialog rebuilds the page, or Build would short-circuit on the old
' gCompBuilt and leave every handle pointing at a dead window.
' ========================================================================================
sub frmOptionsCompiler_Reset()
    gCompBuilt       = false
    ghCompToolchains = 0
    ghCompSwitches   = 0
    ghCompIncludes   = 0
    ghCompRunCmd     = 0
    ghCompBeep       = 0
    erase gCompToolchainNames
    gCompLabels.Clear( )
end sub


' ========================================================================================
' Toolchain selection changed (mouse or keyboard). Write the clean folder name to staging.
' ========================================================================================
private sub frmOptionsCompiler_ToolchainSelChange( byval hList as HWND, byval row as integer )
    if (row < lbound(gCompToolchainNames)) orelse (row > ubound(gCompToolchainNames)) then exit sub
    gOptWork.CompilerToolchain = gCompToolchainNames(row)
end sub


' ========================================================================================
' Paint one toolchain row.
' ========================================================================================
private sub frmOptionsCompiler_ToolchainPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    else
        ' Idle rows match the nav panel's lighter shade (see frmOptions nav list).
        backclr = theme.toptabs.texthot.backcolor
        foreclr = theme.panel.text.forecolor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(8)
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
end sub


' ========================================================================================
' A PsTextBox changed through user typing (SetText is silent). Route to the right field.
' ========================================================================================
private sub frmOptionsCompiler_TextChanged( byval hTextBox as HWND )
    if hTextBox = ghCompSwitches then
        gOptWork.CompilerSwitches = PsTextBox_GetText( hTextBox )
    elseif hTextBox = ghCompIncludes then
        gOptWork.CompilerIncludes = PsTextBox_GetText( hTextBox )
    end if
end sub


' ========================================================================================
' A PsToggle changed through user action. THE TYPE TRAP: store a long 0/1, not a boolean.
' ========================================================================================
private sub frmOptionsCompiler_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    if hToggle = ghCompRunCmd then
        gOptWork.RunViaCommandWindow = iif( isChecked, 1, 0 )
    elseif hToggle = ghCompBeep then
        gOptWork.DisableCompileBeep = iif( isChecked, 1, 0 )
    end if
end sub


' ========================================================================================
' Enumerate the toolchains folder and fill the list. Returns the row index that matches
' the currently stored toolchain, or -1.
' ========================================================================================
private function frmOptionsCompiler_LoadToolchains() as long

    PsListTree_BeginUpdate( ghCompToolchains )
    PsListTree_Clear( ghCompToolchains )
    erase gCompToolchainNames

    dim as long idxMatch = -1
    dim as long idx = 0
    dim as DWSTRING wszWanted = ucase( gOptWork.CompilerToolchain )

    dim hSearch as HANDLE
    dim WFD as WIN32_FIND_DATAW
    dim as DWSTRING wszPath = AfxGetExePathName + "toolchains\"

    hSearch = FindFirstFile( wszPath + "*", @WFD )
    if hSearch <> INVALID_HANDLE_VALUE then
        do
            if (WFD.dwFileAttributes and FILE_ATTRIBUTE_DIRECTORY) = FILE_ATTRIBUTE_DIRECTORY then
                if (WFD.cFileName <> ".") andalso (WFD.cFileName <> "..") then
                    dim as DWSTRING wszFolder  = WFD.cFileName
                    dim as DWSTRING wszDisplay = wszFolder
                    ' The old page warned when the compiler path contains a space (the fbc
                    ' invocation would break) -- keep the warning on the display text only.
                    ' Hard-coded English, exactly as the old page had it (not localized).
                    if instr( ucase(wszPath & wszFolder & "\fbc32.exe"), " " ) then
                        wszDisplay = wszDisplay & "   (Warning: File path contains spaces)"
                    end if

                    idx = PsListTree_AddString( ghCompToolchains, wszDisplay )
                    redim preserve gCompToolchainNames(idx) as DWSTRING
                    gCompToolchainNames(idx) = wszFolder
                    if ucase(wszFolder) = wszWanted then idxMatch = idx
                end if
            end if
        loop while FindNextFile( hSearch, @WFD )
        FindClose( hSearch )
    end if

    PsListTree_EndUpdate( ghCompToolchains )
    function = idxMatch
end function


' ========================================================================================
' Build the page's controls onto hPage. Idempotent.
' ========================================================================================
sub frmOptionsCompiler_Build( byval hPage as HWND )

    if gCompBuilt then exit sub

    ' ---- toolchain list --------------------------------------------------------------
    ghCompToolchains = PsListTree_Create( hPage, IDC_FRMOPTIONSCOMPILER_LSTTOOLCHAINS )
    PsListTree_SetPaintCallback( ghCompToolchains, @frmOptionsCompiler_ToolchainPaint )
    PsListTree_SetSelChangeCallback( ghCompToolchains, @frmOptionsCompiler_ToolchainSelChange )
    PsListTree_SetFont( ghCompToolchains, OptionsFont_Base() )
    PsListTree_SetScrollBarWidth( ghCompToolchains, SCROLLBAR_WIDTH_PANEL )

    dim as long idxMatch = frmOptionsCompiler_LoadToolchains()
    if idxMatch >= 0 then
        PsListTree_SetCurSel( ghCompToolchains, idxMatch )       ' silent; does not fire the callback
    else
        ' No on-disk folder matched the stored path. Leave staging as Load seeded it, so
        ' Commit writes the same path back rather than blanking it.
    end if

    ' ---- single-line text fields -----------------------------------------------------
    ghCompSwitches = PsTextBox_Create( hPage, IDC_FRMOPTIONSCOMPILER_TXTFBSWITCHES )
    PsTextBox_SetText( ghCompSwitches, gOptWork.CompilerSwitches )   ' silent
    PsTextBox_SetChangeCallback( ghCompSwitches, @frmOptionsCompiler_TextChanged )

    ghCompIncludes = PsTextBox_Create( hPage, IDC_FRMOPTIONSCOMPILER_TXTINCLUDES )
    PsTextBox_SetText( ghCompIncludes, gOptWork.CompilerIncludes )
    PsTextBox_SetChangeCallback( ghCompIncludes, @frmOptionsCompiler_TextChanged )

    ' ---- toggles ---------------------------------------------------------------------
    ghCompRunCmd = PsToggle_Create( hPage, IDC_FRMOPTIONSCOMPILER_CHKRUNVIACOMMANDWINDOW )
    PsToggle_SetChecked( ghCompRunCmd, iif( gOptWork.RunViaCommandWindow <> 0, true, false ) )   ' silent
    PsToggle_SetCheckChangedCallback( ghCompRunCmd, @frmOptionsCompiler_ToggleChanged )

    ghCompBeep = PsToggle_Create( hPage, IDC_FRMOPTIONSCOMPILER_CHKDISABLECOMPILEBEEP )
    PsToggle_SetChecked( ghCompBeep, iif( gOptWork.DisableCompileBeep <> 0, true, false ) )
    PsToggle_SetCheckChangedCallback( ghCompBeep, @frmOptionsCompiler_ToggleChanged )

    gCompBuilt = true
    frmOptionsCompiler_ApplyTheme()
end sub


' ========================================================================================
' Theme the page's controls. Re-runnable on a theme change.
' ========================================================================================
sub frmOptionsCompiler_ApplyTheme()
    if gCompBuilt = false then exit sub

    PsListTree_SetBackColor( ghCompToolchains, theme.toptabs.texthot.backcolor )
    OptionsTheme_ApplyListBorder( ghCompToolchains )
    PsListTree_SetScrollBarColors( ghCompToolchains, theme.panel.scrollbar.backcolor, _
                                 theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )

    OptionsTheme_ApplyTextBox( ghCompSwitches )
    OptionsTheme_ApplyTextBox( ghCompIncludes )

    dim as PSTOGGLE_COLORS tc
    OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghCompRunCmd, @tc ) : PsToggle_Refresh( ghCompRunCmd )
    PsToggle_SetColors( ghCompBeep, @tc )   : PsToggle_Refresh( ghCompBeep )
end sub


' ========================================================================================
' Show or hide every control on the page.
' ========================================================================================
sub frmOptionsCompiler_Show( byval bShow as boolean )
    if gCompBuilt = false then exit sub
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ShowWindow( ghCompToolchains, nCmd )
    ShowWindow( ghCompSwitches, nCmd )
    ShowWindow( ghCompIncludes, nCmd )
    ShowWindow( ghCompRunCmd, nCmd )
    ShowWindow( ghCompBeep, nCmd )
end sub


' ========================================================================================
' Record one drawn label at the given rect.
' ========================================================================================
private sub frmOptionsCompiler_AddLabel( byval wszCaption as DWSTRING, _
                                         byval nLeft as long, byval nTop as long, _
                                         byval nRight as long, byval nBottom as long )
    AddPageLabel( gCompLabels, wszCaption, nLeft, nTop, nRight, nBottom )
end sub


' ========================================================================================
' Lay the page out for a given width; return content height. Derives every rect and stores
' the label rects for Paint. cxPanel is handed in -- do not read the width off the window.
' ========================================================================================
function frmOptionsCompiler_Layout( byval hPage as HWND, byval cxPanel as long ) as long

    if gCompBuilt = false then return 0

    dim as long padLeft  = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight = AfxScaleX(OPTROW_PADRIGHT)
    dim as long labelGap = AfxScaleX(OPTROW_LABELGAP)
    dim as long contentR = cxPanel - padRight
    dim as long fullW    = contentR - padLeft
    if fullW < 0 then fullW = 0

    ' Measured line height of the base font (labels and field text share it).
    dim as long textH = 0
    dim as HDC hDC = GetDC( hPage )
    if hDC then
        dim as HFONT hOld = SelectObject( hDC, OptionsFont_Base() )
        dim as TEXTMETRICW tm : GetTextMetricsW( hDC, @tm )
        SelectObject( hDC, hOld )
        ReleaseDC( hPage, hDC )
        textH = tm.tmHeight
    end if

    dim as long fieldH   = textH + AfxScaleY(COMP_FIELDPAD) * 2
    dim as long labelGapY = AfxScaleY(COMP_LABELGAP)
    dim as long sectionGap = AfxScaleY(COMP_SECTIONGAP)
    dim as long y = AfxScaleY(OPTROW_PADTOP)

    gCompLabels.Clear( )

    ' ---- a field label above its control ---------------------------------------------
    #macro FIELDLABEL( cap )
        frmOptionsCompiler_AddLabel( (cap), padLeft, y, contentR, y + textH )
        y += textH + labelGapY
    #endmacro

    ' Toolchains list.
    FIELDLABEL( LL(267) & ":" )
    dim as long listH = AfxScaleY(COMP_LISTHEIGHT)
    SetWindowPos( ghCompToolchains, 0, padLeft, y, fullW, listH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += listH + sectionGap

    ' Compiler switches.
    FIELDLABEL( LL(132) )
    SetWindowPos( ghCompSwitches, 0, padLeft, y, fullW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += fieldH + sectionGap

    ' Include paths.
    FIELDLABEL( LL(131) )
    SetWindowPos( ghCompIncludes, 0, padLeft, y, fullW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += fieldH + sectionGap

    ' ---- two toggle rows (label left, toggle right), same shape as the table rows -----
    #macro TOGGLEROW( hToggle, cap )
        scope
            dim as long tw, th
            PsToggle_GetIdealSize( (hToggle), tw, th )
            dim as long rowH = AfxScaleY(OPTROW_HEIGHT)
            if th + AfxScaleY(8) > rowH then rowH = th + AfxScaleY(8)
            if textH + AfxScaleY(8) > rowH then rowH = textH + AfxScaleY(8)
            dim as long tx = contentR - tw
            dim as long ty = y + (rowH - th) \ 2
            SetWindowPos( (hToggle), 0, tx, ty, tw, th, SWP_NOZORDER or SWP_NOACTIVATE )
            dim as long lr = tx - labelGap
            if lr < padLeft then lr = padLeft
            frmOptionsCompiler_AddLabel( (cap), padLeft, y, lr, y + rowH )
            y += rowH
        end scope
    #endmacro

    TOGGLEROW( ghCompRunCmd, LL(211) )
    TOGGLEROW( ghCompBeep, LL(262) )

    function = y + AfxScaleY(OPTROW_PADBOTTOM)
end function


' ========================================================================================
' Paint the page's labels. Background is filled by the caller first.
' ========================================================================================
sub frmOptionsCompiler_Paint( byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    if gCompBuilt = false then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( theme.panel.text.backcolor )
    p->b->SetForeColor( theme.panel.text.forecolor )
    PaintPageLabels( p, gCompLabels )
end sub
