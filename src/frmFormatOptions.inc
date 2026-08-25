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

' ========================================================================================
' FORMAT OPTIONS dialog.
'
' A fixed 900x620 modal in the frmBuildConfig / frmProjectOptions idiom: a PsListTree
' category nav down the left, the selected category's rule rows in the middle, and a
' read-only preview on the right that RE-RUNS THE REAL FORMATTER on every change.
'
' THE PREVIEW IS THE POINT OF THIS DIALOG. Every IDE surveyed that does formatter settings
' well -- Visual Studio, IntelliJ, Eclipse, UniversalIndentGUI -- feeds its preview from the
' actual engine rather than from a picture, and that is only cheap here because modFormat is
' pure text in / text out. It costs one function call. The payoff is that the preview cannot
' lie about what a rule does, and the dialog doubles as an interactive test harness for the
' engine: a rule that looks wrong here IS wrong.
'
' TRIGGERS ARE ON THEIR OWN PAGE, separate from the rule pages. Every IDE surveyed separates
' WHAT the rules are from WHEN they are applied (VS's Formatting > General, IntelliJ's Smart
' Keys, Eclipse's Save Actions), and mixing a "format while I type" checkbox in among the
' spacing rules invites turning the whole feature off to escape one behaviour.
'
' Cancel is a true no-op: gFmtWork is seeded before a single control exists and gConfig is
' not touched unless OK commits.
' ========================================================================================


' ---- callback typedefs, declared here so the .bi can name no Ps* type --------------------
declare sub frmFormatOptions_NavPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
declare sub frmFormatOptions_NavSelChange( byval hList as HWND, byval nRow as integer )
declare sub frmFormatOptions_ToggleChanged( byval hToggle as HWND, byval bChecked as boolean )
declare sub frmFormatOptions_ComboChanged( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
declare sub frmFormatOptions_NumericChanged( byval hCtrl as HWND, byval dValue as double )
declare sub frmFormatOptions_ButtonClick( byval hButton as HWND, byval id as long )
declare function frmFormatOptions_NavTooltip( byval hList as HWND, byval nRow as integer, byval nCol as integer ) as DWSTRING


' ---- row kinds ---------------------------------------------------------------------------
enum
    FMTROW_TOGGLE = 0
    FMTROW_NUMERIC
    FMTROW_COMBO
end enum


' ----------------------------------------------------------------------------------------
' One rule row.
'
' pField points at the `long` inside gFmtWork that this row edits, which is what keeps the
' seed, the read-back and the preview refresh from each needing their own switch statement
' over every rule. nOffset exists for the one field whose UI index and stored value differ:
' BlankLinesAroundProc uses -1 as its "leave alone" sentinel, so its combo index is the
' value plus one.
' ----------------------------------------------------------------------------------------
type FMTROW
    nPage     as long = FMTPAGE_NONE
    nKind     as long = FMTROW_TOGGLE
    nLabelID  as long = 0                 ' index into LL() -- the L() macro's first argument
    pField    as long ptr = 0             ' -> the field inside gFmtWork this row edits
    nMin      as long = 0                 ' FMTROW_NUMERIC only
    nMax      as long = 0                 ' FMTROW_NUMERIC only
    nOffset   as long = 0                 ' stored value = control index - nOffset
    wszItems  as DWSTRING                 ' FMTROW_COMBO only, chr(9) delimited
    hCtrl     as HWND = 0
    rc        as RECT                     ' DERIVED by the layout, never set from outside
    rcLabel   as RECT
end type


' ---- module state ------------------------------------------------------------------------
dim shared gFmtWork      as FORMAT_RULES      ' the staging copy -- Cancel discards it
dim shared gFmtRows(any) as FMTROW
dim shared gFmtRowCount  as long
dim shared gFmtPage      as long
dim shared gFmtLoading   as long              ' suppresses the change callbacks during a seed
dim shared ghFmtPreview  as HWND
dim shared ghFmtOK       as HWND
dim shared ghFmtCancel   as HWND
dim shared ghFmtReset    as HWND
dim shared ghFmtFontFixed as HFONT
dim shared gFmtCombos(any) as HWND            ' every combo, for the pump's FilterMessage
dim shared gFmtComboCount as long


' ========================================================================================
' Register one rule row.
' ========================================================================================
private sub frmFormatOptions_AddRow( byval nPage as long, _
                                     byval nKind as long, _
                                     byval nLabelID as long, _
                                     byval pField as long ptr, _
                                     byval nMin as long = 0, _
                                     byval nMax as long = 0, _
                                     byref wszItems as DWSTRING = "", _
                                     byval nOffset as long = 0 )
    redim preserve gFmtRows(0 to gFmtRowCount)
    with gFmtRows(gFmtRowCount)
        .nPage    = nPage
        .nKind    = nKind
        .nLabelID = nLabelID
        .pField   = pField
        .nMin     = nMin
        .nMax     = nMax
        .nOffset  = nOffset
        .wszItems = wszItems
        .hCtrl    = 0
    end with
    gFmtRowCount += 1
end sub


' ========================================================================================
' Build the rule table.
'
' Note what is NOT here: indent WIDTH and tabs-versus-spaces. Those already exist as editor
' settings with their own UI, and the formatter reads them from the document (SCI_GETINDENT)
' and gConfig.TabIndentSpaces. Repeating them on this page would let a file be formatted to
' an indent width the editor itself then disagrees with.
' ========================================================================================
private sub frmFormatOptions_BuildRowTable()
    gFmtRowCount = 0
    redim gFmtRows(0 to 0)

    dim as DWSTRING wszCase = L(473,"Leave alone") & chr(9) & L(474,"lowercase") & chr(9) & _
                              L(475,"UPPERCASE") & chr(9) & L(476,"Proper Case")
    dim as DWSTRING wszParen = L(473,"Leave alone") & chr(9) & L(477,"Add spaces") & chr(9) & _
                               L(478,"Remove spaces")
    dim as DWSTRING wszBlank = L(473,"Leave alone") & chr(9) & "0" & chr(9) & "1" & chr(9) & "2"

    ' -- Indentation ---------------------------------------------------------------------
    frmFormatOptions_AddRow( FMTPAGE_INDENT, FMTROW_TOGGLE, 479, @gFmtWork.Reindent )
    frmFormatOptions_AddRow( FMTPAGE_INDENT, FMTROW_TOGGLE, 480, @gFmtWork.IndentCase )
    frmFormatOptions_AddRow( FMTPAGE_INDENT, FMTROW_NUMERIC, 481, @gFmtWork.ContinuationIndent, 0, 4 )

    ' -- Casing ---------------------------------------------------------------------------
    frmFormatOptions_AddRow( FMTPAGE_CASING, FMTROW_COMBO, 482, @gFmtWork.CaseKeywords, 0, 0, wszCase )
    frmFormatOptions_AddRow( FMTPAGE_CASING, FMTROW_COMBO, 483, @gFmtWork.CaseTypes, 0, 0, wszCase )
    frmFormatOptions_AddRow( FMTPAGE_CASING, FMTROW_COMBO, 484, @gFmtWork.CaseDirectives, 0, 0, wszCase )

    ' -- Spacing ---------------------------------------------------------------------------
    frmFormatOptions_AddRow( FMTPAGE_SPACING, FMTROW_TOGGLE, 485, @gFmtWork.SpaceAroundOperators )
    frmFormatOptions_AddRow( FMTPAGE_SPACING, FMTROW_TOGGLE, 486, @gFmtWork.SpaceAfterComma )
    frmFormatOptions_AddRow( FMTPAGE_SPACING, FMTROW_COMBO, 487, @gFmtWork.ParenSpacing, 0, 0, wszParen )
    frmFormatOptions_AddRow( FMTPAGE_SPACING, FMTROW_NUMERIC, 488, @gFmtWork.SpacesBeforeComment, 0, 8 )

    ' -- Blank lines -----------------------------------------------------------------------
    frmFormatOptions_AddRow( FMTPAGE_BLANKLINES, FMTROW_TOGGLE, 489, @gFmtWork.TrimTrailing )
    frmFormatOptions_AddRow( FMTPAGE_BLANKLINES, FMTROW_NUMERIC, 490, @gFmtWork.MaxBlankLines, 0, 10 )
    ' The one row whose control index and stored value differ: -1 is the leave-alone
    ' sentinel, so index 0 stores -1, index 1 stores 0, and so on.
    frmFormatOptions_AddRow( FMTPAGE_BLANKLINES, FMTROW_COMBO, 491, @gFmtWork.BlankLinesAroundProc, 0, 0, wszBlank, 1 )

    ' -- Triggers ---------------------------------------------------------------------------
    frmFormatOptions_AddRow( FMTPAGE_TRIGGERS, FMTROW_TOGGLE, 492, @gFmtWork.FormatOnEnter )
    frmFormatOptions_AddRow( FMTPAGE_TRIGGERS, FMTROW_TOGGLE, 493, @gFmtWork.FormatOnPaste )
end sub


' ========================================================================================
' The preview sample for a category.
'
' Each is chosen so the rules on THAT page visibly bite. A canned sample that happens to
' contain nothing the selected rule affects reads as "this option does nothing", which is
' the failure mode of a preview pane that shows one generic snippet for everything.
' ========================================================================================
private function frmFormatOptions_SampleFor( byval nPage as long ) as string
    dim as string NL = chr(13) & chr(10)

    select case nPage
        case FMTPAGE_CASING
            return "DIM as LONG count" & NL & _
                   "#INCLUDE once ""windows.bi""" & NL & _
                   "IF count > 0 THEN" & NL & _
                   "PRINT ""text is never recased""" & NL & _
                   "END IF" & NL

        case FMTPAGE_SPACING
            return "dim as long i,n" & NL & _
                   "n=(i*2)+1" & NL & _
                   "i=-1   ' unary minus stays tight" & NL & _
                   "foo(a,b)" & NL & _
                   "p->field=x.y" & NL

        case FMTPAGE_BLANKLINES
            return "dim as long g   " & NL & NL & NL & NL & _
                   "sub foo()" & NL & _
                   "print 1" & NL & _
                   "end sub" & NL

        case FMTPAGE_TRIGGERS
            ' Nothing on this page changes the OUTPUT, so a formatted sample would be
            ' identical before and after and would imply the toggles do nothing. Explain
            ' instead. This string is deliberately not run through the engine.
            return ""

        case else       ' FMTPAGE_INDENT
            return "sub demo( byval n as long )" & NL & _
                   "dim as long i" & NL & _
                   "if n > 0 then" & NL & _
                   "select case n" & NL & _
                   "case 1" & NL & _
                   "print ""one""" & NL & _
                   "case else" & NL & _
                   "print ""many""" & NL & _
                   "end select" & NL & _
                   "for i = 1 to n" & NL & _
                   "foo( i, _" & NL & _
                   "n )" & NL & _
                   "next" & NL & _
                   "#if defined(__FB_WIN32__)" & NL & _
                   "print ""win""" & NL & _
                   "#endif" & NL & _
                   "end if" & NL & _
                   "end sub" & NL
    end select
end function


' ========================================================================================
' Re-run the REAL formatter over the current page's sample and show the result.
'
' This is a direct call into the same Format_Text the menu command uses -- not a
' reimplementation, and not a stored picture. That is what makes the preview trustworthy,
' and it is only possible because the engine is pure.
' ========================================================================================
private sub frmFormatOptions_RefreshPreview()
    if ghFmtPreview = 0 then exit sub

    if gFmtPage = FMTPAGE_TRIGGERS then
        PsTextBox_SetText( ghFmtPreview, L(494,"These settings decide WHEN formatting runs, not what it does. The rules on the other pages are what get applied.") & _
                           chr(13) & chr(10) & chr(13) & chr(10) & _
                           L(495,"Formatting while you type only ever touches the line you just finished, never a line below the caret, and never removes a line. Both actions are a separate undo step, so Ctrl+Z rejects the formatting and keeps what you typed or pasted.") )
        exit sub
    end if

    dim as string sSample = frmFormatOptions_SampleFor( gFmtPage )
    dim as long bChanged = false
    dim as string sOut = Format_Text( sSample, gFmtWork, bChanged, 4, false )

    ' Format_Text returns the input unchanged when it changes nothing OR when a safety check
    ' refuses the result. Showing the original is right in both cases: the preview's job is
    ' to show what the file would look like afterwards, and refusing means "unchanged".
    PsTextBox_SetText( ghFmtPreview, sOut )
end sub


' ========================================================================================
' Seed every control on the current page from gFmtWork.
'
' gFmtLoading suppresses the change callbacks throughout -- otherwise seeding a control
' fires its handler, which writes the value it just read straight back and refreshes the
' preview once per row.
' ========================================================================================
private sub frmFormatOptions_SeedControls()
    gFmtLoading = true
    for i as long = 0 to gFmtRowCount - 1
        if gFmtRows(i).hCtrl = 0 then continue for
        if gFmtRows(i).pField = 0 then continue for
        dim as long nVal = *gFmtRows(i).pField
        select case gFmtRows(i).nKind
            case FMTROW_TOGGLE
                PsToggle_SetChecked( gFmtRows(i).hCtrl, iif( nVal <> 0, true, false ) )
            case FMTROW_NUMERIC
                PsNumericUpDown_SetValue( gFmtRows(i).hCtrl, nVal )
            case FMTROW_COMBO
                PsComboBox_SetCurSel( gFmtRows(i).hCtrl, nVal + gFmtRows(i).nOffset )
        end select
    next
    gFmtLoading = false
end sub


' ========================================================================================
' Find the row that owns a control handle.
' ========================================================================================
private function frmFormatOptions_RowFromCtrl( byval hCtrl as HWND ) as long
    for i as long = 0 to gFmtRowCount - 1
        if gFmtRows(i).hCtrl = hCtrl then return i
    next
    return -1
end function


' ========================================================================================
' ---- change callbacks. Each writes ONE field and refreshes the preview. -----------------
' ========================================================================================
private sub frmFormatOptions_ToggleChanged( byval hToggle as HWND, byval bChecked as boolean )
    if gFmtLoading then exit sub
    dim as long i = frmFormatOptions_RowFromCtrl( hToggle )
    if i < 0 then exit sub
    if gFmtRows(i).pField = 0 then exit sub
    *gFmtRows(i).pField = iif( bChecked, 1, 0 )
    frmFormatOptions_RefreshPreview()
end sub


private sub frmFormatOptions_NumericChanged( byval hCtrl as HWND, byval dValue as double )
    if gFmtLoading then exit sub
    dim as long i = frmFormatOptions_RowFromCtrl( hCtrl )
    if i < 0 then exit sub
    if gFmtRows(i).pField = 0 then exit sub
    *gFmtRows(i).pField = clng( dValue )
    frmFormatOptions_RefreshPreview()
end sub


private sub frmFormatOptions_ComboChanged( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if gFmtLoading then exit sub
    dim as long i = frmFormatOptions_RowFromCtrl( hCombo )
    if i < 0 then exit sub
    if gFmtRows(i).pField = 0 then exit sub
    *gFmtRows(i).pField = idxNew - gFmtRows(i).nOffset
    frmFormatOptions_RefreshPreview()
end sub


' ========================================================================================
' Nav selection. Shows the chosen page and re-runs the preview against ITS sample.
' ========================================================================================
private sub frmFormatOptions_ShowPage( byval nPage as long )
    gFmtPage = nPage
    for i as long = 0 to gFmtRowCount - 1
        if gFmtRows(i).hCtrl = 0 then continue for
        ShowWindow( gFmtRows(i).hCtrl, iif( gFmtRows(i).nPage = nPage, SW_SHOW, SW_HIDE ) )
    next
    frmFormatOptions_RefreshPreview()
    AfxRedrawWindow( HWND_FRMFORMATOPTIONS )
end sub


private sub frmFormatOptions_NavSelChange( byval hList as HWND, byval nRow as integer )
    dim as long nPage = PsListTree_GetItemData( hList, nRow )
    if nPage = FMTPAGE_NONE then exit sub
    frmFormatOptions_ShowPage( nPage )
end sub


' ========================================================================================
' Nav row painter. Selected row gets the accent fill; nothing hot-tracks.
' ========================================================================================
private sub frmFormatOptions_NavPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as long clrBack = theme.toptabs.texthot.backcolor
    dim as long clrText = theme.panel.text.forecolor
    if p->isSelected then
        clrBack = theme.panel.text.backcolor
        clrText = theme.panel.text.forecolor
    end if
    p->b->SetForeColor( clrText )
    p->b->SetBackColor( clrBack )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(16)
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
end sub


private function frmFormatOptions_NavTooltip( byval hList as HWND, byval nRow as integer, byval nCol as integer ) as DWSTRING
    ' Suppressed deliberately. Unset, PsListTree falls back to the row's own text and shows
    ' every category name back at you a moment after you read it.
    return ""
end function


' ========================================================================================
' ---- OK / Cancel / Reset ---------------------------------------------------------------
' ========================================================================================
private sub frmFormatOptions_ButtonClick( byval hButton as HWND, byval id as long )
    if hButton = ghFmtOK then
        ' The ONLY place gConfig is written. One struct assignment, which is also why the
        ' rules live on clsConfig as a nested type rather than as sixteen loose fields.
        gConfig.Format = gFmtWork
        gConfig.SaveConfigFile()
        PostMessage( HWND_FRMFORMATOPTIONS, WM_CLOSE, 0, 0 )

    elseif hButton = ghFmtCancel then
        PostMessage( HWND_FRMFORMATOPTIONS, WM_CLOSE, 0, 0 )

    elseif hButton = ghFmtReset then
        gFmtWork = Format_DefaultRules()
        frmFormatOptions_SeedControls()
        frmFormatOptions_RefreshPreview()
    end if
end sub


' ========================================================================================
' Layout. Every rect is DERIVED here and stored, so the painter and the self-test both read
' exactly the numbers this produced rather than each recomputing them.
' ========================================================================================
private sub frmFormatOptions_PositionWindows( byval hwnd as HWND )
    if hwnd = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )
    dim as long cx = rcClient.right
    dim as long cy = rcClient.bottom

    dim as long nNavW    = AfxScaleX(FRMFORMATOPTIONS_NAV_W)
    dim as long nPrevW   = AfxScaleX(FRMFORMATOPTIONS_PREVIEW_W)
    dim as long nTitleH  = AfxScaleY(FRMFORMATOPTIONS_TITLE_H)
    dim as long nFootH   = AfxScaleY(FRMFORMATOPTIONS_FOOTER_H)
    dim as long nMargin  = AfxScaleX(FRMFORMATOPTIONS_MARGIN)
    dim as long nRowH    = AfxScaleY(FRMFORMATOPTIONS_ROW_H)
    dim as long nComboW  = AfxScaleX(FRMFORMATOPTIONS_COMBO_W)
    dim as long nNumericW = AfxScaleX(FRMFORMATOPTIONS_NUMERIC_W)
    dim as long nToggleW = AfxScaleX(FRMFORMATOPTIONS_TOGGLE_W)
    ' The Preview caption sits in a band exactly one NAV ROW tall, so it lines up with the
    ' category list beside it and with the first rule row.
    dim as long nCapH    = AfxScaleY(FRMFORMATOPTIONS_NAVROW_H)
    dim as long nGap     = AfxScaleX(FRMFORMATOPTIONS_LABELGAP)
    dim as long nBtnW    = AfxScaleX(FRMFORMATOPTIONS_BUTTON_W)
    dim as long nBtnH    = AfxScaleY(FRMFORMATOPTIONS_BUTTON_H)

    dim as long yBody = nTitleH
    dim as long hBody = cy - nTitleH - nFootH

    ' -- nav strip, full body height ------------------------------------------------------
    if HWND_FRMFORMATOPTIONS_NAV then
        SetWindowPos( HWND_FRMFORMATOPTIONS_NAV, 0, 0, yBody, nNavW, hBody, SWP_NOZORDER )
    end if

    ' -- preview, pinned right ------------------------------------------------------------
    dim as long xPrev = cx - nPrevW - nMargin
    if ghFmtPreview then
        ' Below the caption band, not level with it. The caption used to be drawn in the
        ' 20px ABOVE the box, which at this y put it partly over the title band's hairline.
        SetWindowPos( ghFmtPreview, 0, xPrev, yBody + nMargin + nCapH, _
                      nPrevW, hBody - (nMargin * 2) - nCapH, SWP_NOZORDER )
    end if

    ' -- rule rows, in the middle column ---------------------------------------------------
    dim as long xRows = nNavW + nMargin
    dim as long wRows = xPrev - nMargin - xRows
    dim as long y = yBody + nMargin

    ' Every page starts at the same y, so switching pages does not make the rows jump.
    for nPage as long = FMTPAGE_INDENT to FMTPAGE_COUNT
        y = yBody + nMargin
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).nPage <> nPage then continue for

            gFmtRows(i).rc.left   = xRows
            gFmtRows(i).rc.top    = y
            gFmtRows(i).rc.right  = xRows + wRows
            gFmtRows(i).rc.bottom = y + nRowH

            ' The control is pinned to the RIGHT of the row and the label takes the span
            ' that is left -- PsButton's model. So a long caption borrows the room a narrow
            ' control is not using, instead of ellipsizing against a fixed gutter.
            '
            ' The width is the one THIS row's control actually needs, not a single column
            ' width shared by all three kinds. Sharing one meant a toggle row's label
            ' stopped short of a combo's right edge two rows away and ellipsized against
            ' space nothing was occupying.
            '
            ' AND THE HEIGHT IS ASKED FOR, NOT ASSUMED. Every one of these controls measures
            ' itself against its own font and reports an ideal size; a hardcoded height is a
            ' guess that happens to be right at one DPI. It was wrong here: a numeric field
            ' forced to AfxScaleY(26) is 46px at 175% where the control needs 56, and since
            ' PsNumericUpDown insets its value cell vertically inside that frame, the digit
            ' had nowhere to go and rendered with its top and bottom sliced off. Nothing
            ' about it looked wrong at 100%.
            dim as long wCtrl = nComboW
            dim as long hCtrl = AfxScaleY(26)
            select case gFmtRows(i).nKind
                case FMTROW_TOGGLE  : wCtrl = nToggleW
                case FMTROW_NUMERIC : wCtrl = nNumericW
            end select

            if gFmtRows(i).hCtrl then
                dim as long iw = 0, ih = 0
                select case gFmtRows(i).nKind
                    case FMTROW_TOGGLE  : PsToggle_GetIdealSize( gFmtRows(i).hCtrl, iw, ih )
                    case FMTROW_NUMERIC : PsNumericUpDown_GetIdealSize( gFmtRows(i).hCtrl, iw, ih )
                    case FMTROW_COMBO   : PsComboBox_GetIdealSize( gFmtRows(i).hCtrl, iw, ih )
                end select
                ' The ideal height is a FLOOR, never a ceiling -- a control shorter than its
                ' own text clips, taller merely has air around it.
                if ih > hCtrl then hCtrl = ih
                ' Width is the other way round: take the ideal when it is NARROWER than the
                ' column, because every pixel handed back goes to the label beside it. The
                ' numeric asked for 134 where the column offered 158.
                if (iw > 0) andalso (iw < wCtrl) then wCtrl = iw
            end if

            gFmtRows(i).rcLabel.left   = xRows
            gFmtRows(i).rcLabel.top    = y
            gFmtRows(i).rcLabel.right  = gFmtRows(i).rc.right - wCtrl - nGap
            gFmtRows(i).rcLabel.bottom = y + nRowH

            if gFmtRows(i).hCtrl then
                SetWindowPos( gFmtRows(i).hCtrl, 0, _
                              gFmtRows(i).rc.right - wCtrl, _
                              y + ((nRowH - hCtrl) \ 2), _
                              wCtrl, hCtrl, SWP_NOZORDER )
            end if
            y += nRowH
        next
    next

    ' -- footer buttons, right aligned ------------------------------------------------------
    dim as long yBtn = cy - nFootH + ((nFootH - nBtnH) \ 2)
    dim as long xBtn = cx - nMargin - nBtnW
    if ghFmtCancel then SetWindowPos( ghFmtCancel, 0, xBtn, yBtn, nBtnW, nBtnH, SWP_NOZORDER )
    xBtn -= (nBtnW + AfxScaleX(8))
    if ghFmtOK then SetWindowPos( ghFmtOK, 0, xBtn, yBtn, nBtnW, nBtnH, SWP_NOZORDER )
    ' Reset sits hard LEFT, away from the pair -- it is destructive of the user's edits and
    ' must not be a mis-click away from OK.
    if ghFmtReset then SetWindowPos( ghFmtReset, 0, nMargin, yBtn, AfxScaleX(150), nBtnH, SWP_NOZORDER )
end sub


' ========================================================================================
' Painter: the background, the title band, the rule labels and the two hairlines.
' ========================================================================================
private sub frmFormatOptions_OnPaint( byval hwnd as HWND )
    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )

    dim pb as PsBufferPaint
    pb.BeginDoubleBuffer( hwnd, hdc, rcClient )

    pb.SetBackColor( theme.panel.text.backcolor )
    pb.PaintRect( @rcClient )

    dim as long nTitleH = AfxScaleY(FRMFORMATOPTIONS_TITLE_H)
    dim as long nFootH  = AfxScaleY(FRMFORMATOPTIONS_FOOTER_H)
    dim as long nMargin = AfxScaleX(FRMFORMATOPTIONS_MARGIN)

    ' -- title band -----------------------------------------------------------------------
    dim as RECT rcTitle = type( 0, 0, rcClient.right, nTitleH )
    pb.SetBackColor( theme.panel.text.backcolor )
    pb.PaintRect( @rcTitle )
    dim as RECT rcTitleText = rcTitle
    rcTitleText.left += nMargin
    pb.SetFont( ghFont(GUIFONTBOLD_11) )
    ' Plain foreground, never the accent. ui.accent falls back to toptabs.iconselected, a
    ' dark navy in default_dark and a PALE tint in default_light -- a heading painted in it
    ' is legible in one theme and invisible in the other. The hairline does the separating.
    pb.SetForeColor( theme.panel.text.forecolor )
    pb.PaintText( L(470,"Format Options"), @rcTitleText, DT_LEFT )

    dim as RECT rcRule = type( 0, nTitleH - AfxScaleY(1), rcClient.right, nTitleH )
    pb.SetBackColor( theme.ui.divider )
    pb.PaintRect( @rcRule )

    ' -- footer hairline --------------------------------------------------------------------
    dim as RECT rcFootRule = type( 0, rcClient.bottom - nFootH, rcClient.right, rcClient.bottom - nFootH + AfxScaleY(1) )
    pb.SetBackColor( theme.ui.divider )
    pb.PaintRect( @rcFootRule )

    ' -- rule labels for the visible page ---------------------------------------------------
    pb.SetFont( ghFont(GUIFONT_11) )
    pb.SetForeColor( theme.panel.text.forecolor )
    pb.SetBackColor( theme.panel.text.backcolor )
    for i as long = 0 to gFmtRowCount - 1
        if gFmtRows(i).nPage <> gFmtPage then continue for
        pb.PaintText( L(gFmtRows(i).nLabelID, ""), @gFmtRows(i).rcLabel, _
                      DT_LEFT or DT_END_ELLIPSIS or DT_VCENTER or DT_SINGLELINE )
    next

    ' -- preview caption ---------------------------------------------------------------------
    if ghFmtPreview then
        dim as RECT rcPrev
        GetWindowRect( ghFmtPreview, @rcPrev )
        MapWindowPoints( 0, hwnd, cast(POINT ptr, @rcPrev), 2 )
        ' Exactly one nav-row tall, so it reads as a heading of the same weight as a
        ' category row rather than as a cramped label stuck to the top of the box.
        dim as RECT rcCap = type( rcPrev.left, rcPrev.top - AfxScaleY(FRMFORMATOPTIONS_NAVROW_H), _
                                  rcPrev.right, rcPrev.top )
        pb.SetFont( ghFont(GUIFONTBOLD_11) )
        pb.PaintText( L(496,"Preview"), @rcCap, DT_LEFT or DT_VCENTER or DT_SINGLELINE )
    end if

    pb.EndDoubleBuffer()
    EndPaint( hwnd, @ps )
end sub


' ========================================================================================
' Theme. The controls COPY colours at Set time, so this re-applies after every build and on
' a theme change. The OptionsTheme_Fill* helpers are shared with the Environment Options
' dialog so a toggle here and a toggle there cannot drift apart.
' ========================================================================================
private sub frmFormatOptions_ApplyTheme( byval hwnd as HWND )
    if hwnd = 0 then exit sub

    if HWND_FRMFORMATOPTIONS_NAV then
        PsListTree_SetBackColor( HWND_FRMFORMATOPTIONS_NAV, theme.toptabs.texthot.backcolor )
        PsListTree_SetScrollBarColors( HWND_FRMFORMATOPTIONS_NAV, theme.panel.scrollbar.backcolor, _
                                       theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    end if

    for i as long = 0 to gFmtRowCount - 1
        if gFmtRows(i).hCtrl = 0 then continue for
        select case gFmtRows(i).nKind
            case FMTROW_TOGGLE
                dim as PSTOGGLE_COLORS tc
                OptionsTheme_FillToggle( @tc )
                PsToggle_SetColors( gFmtRows(i).hCtrl, @tc )
            case FMTROW_NUMERIC
                dim as PSNUMERICUPDOWN_COLORS nc
                OptionsTheme_FillNumeric( @nc )
                PsNumericUpDown_SetColors( gFmtRows(i).hCtrl, @nc )
            case FMTROW_COMBO
                dim as PSCOMBOBOX_COLORS cc
                dim as PSPOPUPMENU_COLORS pc
                OptionsTheme_FillCombo( @cc, @pc )
                PsComboBox_SetColors( gFmtRows(i).hCtrl, @cc )
                PsComboBox_SetListColors( gFmtRows(i).hCtrl, @pc )
        end select
    next

    scope
        dim as PSBUTTON_COLORS bc
        OptionsTheme_FillButton( @bc )
        if ghFmtOK     then PsButton_SetColors( ghFmtOK, @bc )
        if ghFmtCancel then PsButton_SetColors( ghFmtCancel, @bc )
        if ghFmtReset  then PsButton_SetColors( ghFmtReset, @bc )
    end scope

    if ghFmtPreview then OptionsTheme_ApplyTextBox( ghFmtPreview )

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )
end sub


' ========================================================================================
' WndProc.
' ========================================================================================
private function frmFormatOptions_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                           byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case WM_CREATE
            ' Modality: the parent is disabled for the life of the dialog.
            EnableWindow( GetParent(hwnd), false )
            return 0

        case WM_ERASEBKGND
            return 1

        case WM_PAINT
            frmFormatOptions_OnPaint( hwnd )
            return 0

        case WM_SIZE
            frmFormatOptions_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            if loword(wParam) = IDCANCEL then
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                return 0
            end if

        case WM_CLOSE
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            ' Zero every handle and drop the staging copy. Nothing is saved here -- the
            ' commit happens on the OK path only, which is what makes Cancel a true no-op.
            HWND_FRMFORMATOPTIONS = 0
            HWND_FRMFORMATOPTIONS_NAV = 0
            ghFmtPreview = 0 : ghFmtOK = 0 : ghFmtCancel = 0 : ghFmtReset = 0
            gFmtRowCount = 0
            erase gFmtRows
            gFmtComboCount = 0
            erase gFmtCombos
            PostQuitMessage(0)
            return 0
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Show the dialog. Blocks until it closes.
' ========================================================================================
function frmFormatOptions_Show( byval hWndParent as HWND ) as LRESULT

    ' Seed the staging copy BEFORE any control exists. gConfig is not touched again unless
    ' OK commits, which is the whole of why Cancel is a no-op.
    gFmtWork = gConfig.Format
    gFmtPage = FMTPAGE_INDENT
    gFmtLoading = false
    gFmtComboCount = 0
    redim gFmtCombos(0 to 0)

    frmFormatOptions_BuildRowTable()

    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMFORMATOPTIONS = pWindow->Create( hWndParent, L(470,"Format Options"), _
        @frmFormatOptions_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMFORMATOPTIONS_CLIENT_W, FRMFORMATOPTIONS_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' A fixed-pitch face for the preview -- code shown in the UI font does not read as code,
    ' and indentation in a proportional font is genuinely hard to judge, which is the one
    ' thing this pane exists to show. There is no cached ghFont entry at this size.
    ghFmtFontFixed = pWindow->CreateFont( GUIFIXEDFONT, 10, FW_NORMAL )

    ' ---- CREATION ORDER IS THE TAB ORDER -------------------------------------------------
    ' These sit on an ordinary CWindow, not a dialog template, so there is no ordinal to
    ' set: GetNextDlgTabItem walks the child Z-ORDER and Z-order is creation order. Built
    ' here as nav -> rule rows -> preview -> Reset -> OK -> Cancel, i.e. THE WORK FIRST in
    ' reading order and the chrome after.

    ' -- nav ---------------------------------------------------------------------------------
    HWND_FRMFORMATOPTIONS_NAV = PsListTree_Create( HWND_FRMFORMATOPTIONS, IDC_FRMFORMATOPTIONS_NAV )
    PsListTree_SetPaintCallback( HWND_FRMFORMATOPTIONS_NAV, @frmFormatOptions_NavPaintRow )
    PsListTree_SetSelChangeCallback( HWND_FRMFORMATOPTIONS_NAV, @frmFormatOptions_NavSelChange )
    PsListTree_SetTooltipCallback( HWND_FRMFORMATOPTIONS_NAV, @frmFormatOptions_NavTooltip )
    PsListTree_SetFont( HWND_FRMFORMATOPTIONS_NAV, ghFont(GUIFONT_11) )
    ' UNSCALED -- PsListTree scales the row height internally, so a pre-scaled value is
    ' scaled twice.
    PsListTree_SetRowHeight( HWND_FRMFORMATOPTIONS_NAV, FRMFORMATOPTIONS_NAVROW_H )

    ' NAV ORDER IS NOT THE PAGE-ID ORDER, deliberately. When To Format leads because it is
    ' the decision a new user has to make first -- whether this runs by hand or while they
    ' type -- and the rule pages follow. The page ids are unchanged; only the display order
    ' moved, and nothing reads the ids positionally.
    PsListTree_AddString( HWND_FRMFORMATOPTIONS_NAV, L(501,"When To Format"), FMTPAGE_TRIGGERS )
    PsListTree_AddString( HWND_FRMFORMATOPTIONS_NAV, L(497,"Indentation"), FMTPAGE_INDENT )
    PsListTree_AddString( HWND_FRMFORMATOPTIONS_NAV, L(498,"Casing"), FMTPAGE_CASING )
    PsListTree_AddString( HWND_FRMFORMATOPTIONS_NAV, L(499,"Spacing"), FMTPAGE_SPACING )
    PsListTree_AddString( HWND_FRMFORMATOPTIONS_NAV, L(500,"Blank Lines"), FMTPAGE_BLANKLINES )

    ' -- rule rows ---------------------------------------------------------------------------
    for i as long = 0 to gFmtRowCount - 1
        dim as long nID = IDC_FRMFORMATOPTIONS_FIRSTROW + i
        select case gFmtRows(i).nKind
            case FMTROW_TOGGLE
                gFmtRows(i).hCtrl = PsToggle_Create( HWND_FRMFORMATOPTIONS, nID )
                PsToggle_SetCheckChangedCallback( gFmtRows(i).hCtrl, @frmFormatOptions_ToggleChanged )

            case FMTROW_NUMERIC
                gFmtRows(i).hCtrl = PsNumericUpDown_Create( HWND_FRMFORMATOPTIONS, nID )
                PsNumericUpDown_SetFont( gFmtRows(i).hCtrl, ghFont(GUIFONT_11) )
                PsNumericUpDown_SetDecimalPlaces( gFmtRows(i).hCtrl, 0 )
                PsNumericUpDown_SetRange( gFmtRows(i).hCtrl, gFmtRows(i).nMin, gFmtRows(i).nMax )
                PsNumericUpDown_SetValueChangedCallback( gFmtRows(i).hCtrl, @frmFormatOptions_NumericChanged )

            case FMTROW_COMBO
                gFmtRows(i).hCtrl = PsComboBox_Create( HWND_FRMFORMATOPTIONS, nID )
                PsComboBox_SetFont( gFmtRows(i).hCtrl, ghFont(GUIFONT_11) )
                dim as long nStart = 1
                dim as string sItems = gFmtRows(i).wszItems
                do
                    dim as long nTab = instr( nStart, sItems, chr(9) )
                    dim as string sItem
                    if nTab = 0 then
                        sItem = mid( sItems, nStart )
                    else
                        sItem = mid( sItems, nStart, nTab - nStart )
                    end if
                    if len(sItem) then PsComboBox_AddItem( gFmtRows(i).hCtrl, sItem )
                    if nTab = 0 then exit do
                    nStart = nTab + 1
                loop
                PsComboBox_SetSelChangeCallback( gFmtRows(i).hCtrl, @frmFormatOptions_ComboChanged )
                ' Remembered so the pump can offer each one its FilterMessage -- a combo
                ' whose dropdown never sees the pump has no keyboard navigation and does not
                ' close on an outside click.
                redim preserve gFmtCombos(0 to gFmtComboCount)
                gFmtCombos(gFmtComboCount) = gFmtRows(i).hCtrl
                gFmtComboCount += 1
        end select
    next

    ' -- preview -------------------------------------------------------------------------------
    ghFmtPreview = PsTextBox_Create( HWND_FRMFORMATOPTIONS, IDC_FRMFORMATOPTIONS_PREVIEW, true )
    PsTextBox_SetFont( ghFmtPreview, ghFmtFontFixed )
    PsTextBox_SetReadOnly( ghFmtPreview, true )

    ' -- buttons --------------------------------------------------------------------------------
    ghFmtReset = PsButton_Create( HWND_FRMFORMATOPTIONS, IDC_FRMFORMATOPTIONS_CMDRESET )
    PsButton_SetText( ghFmtReset, L(502,"Reset to Defaults") )
    PsButton_SetFont( ghFmtReset, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghFmtReset, @frmFormatOptions_ButtonClick )

    ghFmtOK = PsButton_Create( HWND_FRMFORMATOPTIONS, IDC_FRMFORMATOPTIONS_CMDOK )
    PsButton_SetText( ghFmtOK, L(0,"OK") )
    PsButton_SetFont( ghFmtOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( ghFmtOK, true )
    PsButton_SetClickCallback( ghFmtOK, @frmFormatOptions_ButtonClick )

    ghFmtCancel = PsButton_Create( HWND_FRMFORMATOPTIONS, IDC_FRMFORMATOPTIONS_CMDCANCEL )
    PsButton_SetText( ghFmtCancel, L(1,"Cancel") )
    PsButton_SetFont( ghFmtCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghFmtCancel, @frmFormatOptions_ButtonClick )

    ShowWindow( HWND_FRMFORMATOPTIONS_NAV, SW_SHOW )
    ShowWindow( ghFmtPreview, SW_SHOW )
    ShowWindow( ghFmtReset, SW_SHOW )
    ShowWindow( ghFmtOK, SW_SHOW )
    ShowWindow( ghFmtCancel, SW_SHOW )

    frmFormatOptions_ApplyTheme( HWND_FRMFORMATOPTIONS )
    frmFormatOptions_PositionWindows( HWND_FRMFORMATOPTIONS )
    frmFormatOptions_SeedControls()
    ' Select row 0 and show WHATEVER PAGE ROW 0 HOLDS, read back from the list rather than
    ' named here. Hardcoding the landing page is what lets a reordered nav highlight one row
    ' while displaying another -- and reordering is exactly what just happened.
    PsListTree_SetCurSel( HWND_FRMFORMATOPTIONS_NAV, 0 )
    frmFormatOptions_ShowPage( PsListTree_GetItemData( HWND_FRMFORMATOPTIONS_NAV, 0 ) )

    ShowWindow( HWND_FRMFORMATOPTIONS, SW_SHOW )
    UpdateWindow( HWND_FRMFORMATOPTIONS )

    ' IsDialogMessage ignores Tab until a child already has focus, so an ordinary CWindow
    ' host has to place it itself.
    SetFocus( HWND_FRMFORMATOPTIONS_NAV )

    frmFormatOptions_RunSelfTest( true )

    ' Unattended close, so the layout half above can run without a human. Same reason as the
    ' AUTOEXIT hook in frmMain: this dialog is modal and owns its own GetMessage loop, so a
    ' run that is killed rather than closed never flushes the buffered console output that
    ' IS the suite result. Posted rather than sent -- the loop below has to start first.
    if environ("TIKO_FORMATOPTIONS_AUTOCLOSE") = "1" then
        PostMessage( HWND_FRMFORMATOPTIONS, WM_CLOSE, 0, 0 )
    end if

    ' ---- message loop ---------------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents, which has no *_FilterMessage hooks -- and this
    ' dialog owes two.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. The preview textbox owns a PsPopupMenu context menu; without this it
        ' has no keyboard navigation and never closes on an outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' MANDATORY, per combo. Each dropdown is a second top-level window.
        dim as long bEaten = false
        for i as long = 0 to gFmtComboCount - 1
            if gFmtCombos(i) = 0 then continue for
            if PsComboBox_FilterMessage( gFmtCombos(i), @uMsg ) then
                bEaten = true
                exit for
            end if
        next
        if bEaten then continue while

        ' The numeric fields wrap a PsTextBox two levels down, so they owe the same call.
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).nKind <> FMTROW_NUMERIC then continue for
            if gFmtRows(i).hCtrl = 0 then continue for
            if PsNumericUpDown_FilterMessage( @uMsg ) then
                bEaten = true
                exit for
            end if
        next
        if bEaten then continue while

        ' AFTER the filters, so an open dropdown or context menu takes Escape first rather
        ' than the whole dialog closing out from under it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMFORMATOPTIONS, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMFORMATOPTIONS, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if ghFmtFontFixed then
        DeleteObject( ghFmtFontFixed )
        ghFmtFontFixed = 0
    end if

    delete pWindow
    return 0
end function


' ========================================================================================
' ENV-GATED SELF-TEST -- TIKO_FORMAT_SELFTEST=1.
'
' Two halves. The HEADLESS half runs from frmMain and covers the localized ids and the row
' table's own invariants. The LAYOUT half runs from Show and measures real windows at the
' real DPI, so it only executes when the dialog is actually opened.
'
' WHAT NEITHER HALF CAN SEE, said plainly: whether the preview pane is showing anything
' correct. It asserts that the preview is WIRED to Format_Text and that the text changes
' when a rule changes -- but a formatter producing nonsense would satisfy both. That the
' output is right is the engine suite's job, and that it LOOKS right is the author's.
'
' A layout suite is also structurally blind to a GAP: nothing here overlaps, so a page with
' a dead band through the middle of it passes every assertion below. Reading the render is
' the only thing that catches that.
' ========================================================================================
sub frmFormatOptions_RunSelfTest( byval bLayoutHalf as boolean )

    if environ("TIKO_FORMAT_SELFTEST") <> "1" then exit sub

    static as long nPass, nFail

    #macro CHECK( _cond_, _name_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            dim as string sFailMsg = "  FAIL: " & _name_
            print sFailMsg
        end if
    #endmacro

    if bLayoutHalf = false then
        nPass = 0 : nFail = 0
        print "frmFormatOptions self-test (headless)"

        ' ---- the localized ids -----------------------------------------------------------
        ' L(id,"default") DISCARDS the default -- it is #Define L(e,s) LL(e). An id with no
        ' entry in english.lang renders as an EMPTY STRING, so a rule label, a combo choice
        ' or a whole page comes up BLANK with nothing in the build to say so.
        ' THE IDS ARE THE ONES THIS DIALOG ACTUALLY CALLS -- 470 (its own caption),
        ' 473..478 (the combo choices), 479..493 (one per rule row), 494..495 (the two
        ' explanatory paragraphs), 496..501 (Preview plus the five nav pages) and 502
        ' (Reset to Defaults). They were 638..669 until the id block was renumbered DOWN
        ' BY 168; this list was not renumbered with it, so every assertion below was
        ' reading LL() PAST ITS UPPER BOUND of 521 and grading itself on whatever heap
        ' happened to follow the array -- which is why the pass/fail split moved between
        ' runs of an UNCHANGED binary and moved again depending on which other suites had
        ' run first. LangIdIsSet bounds-checks, so a stale id now fails deterministically.
        dim as long ids(0 to 30) = { 470, 473, 474, 475, 476, 477, 478, 479, 480, 481, _
                                     482, 483, 484, 485, 486, 487, 488, 489, 490, 491, _
                                     492, 493, 494, 495, 496, 497, 498, 499, 500, 501, _
                                     502 }
        for i as long = 0 to 30
            dim as string sName = "lang id " & str(ids(i)) & " is not blank"
            CHECK( LangIdIsSet(ids(i)), sName )
        next

        ' AND WHAT THEY RESOLVED TO, printed rather than asserted. NOT decoration: the
        ' is-not-blank check passes under EVERY language file even when that language has the slot
        ' empty, because LoadLocalizationFile fills a blank key with the English text. So
        ' the assertions above cannot tell a translation from the English fallback, and
        ' running under french.lang and seeing them green proves only that the ids exist.
        dim as string sShow = "  resolved: 497=" & LL(497) & " | 479=" & LL(479) & " | 476=" & LL(476)
        print sShow

        ' ---- the row table ----------------------------------------------------------------
        scope
            dim as FORMAT_RULES rSave = gFmtWork
            frmFormatOptions_BuildRowTable()

            ' 15 is not an arbitrary count: FORMAT_RULES has exactly 15 fields and every one
            ' of them is exposed here. So this doubles as "no rule is unreachable from the
            ' UI" -- a field with no row can only be changed by hand-editing settings.ini,
            ' which is the failure this guards against when a rule is added later.
            CHECK( gFmtRowCount = 15, "every one of the 15 rule fields has a row" )

            ' Every row must point at a real field and carry a label, or it edits nothing
            ' and renders blank -- both silent failures.
            dim as long bAllBound = true, bAllLabelled = true
            for i as long = 0 to gFmtRowCount - 1
                if gFmtRows(i).pField = 0 then bAllBound = false
                if gFmtRows(i).nLabelID = 0 then bAllLabelled = false
            next
            CHECK( bAllBound, "every row is bound to a field in gFmtWork" )
            CHECK( bAllLabelled, "every row carries a label id" )

            ' No two rows may edit the SAME field -- that would make one of them silently
            ' overwrite the other on every change.
            dim as long bNoDupes = true
            for i as long = 0 to gFmtRowCount - 1
                for j as long = i + 1 to gFmtRowCount - 1
                    if gFmtRows(i).pField = gFmtRows(j).pField then bNoDupes = false
                next
            next
            CHECK( bNoDupes, "no two rows edit the same field" )

            ' Every page must actually have rows behind it, or its nav entry opens a blank
            ' panel.
            dim as long bEveryPageUsed = true
            for p as long = FMTPAGE_INDENT to FMTPAGE_COUNT
                dim as long n = 0
                for i as long = 0 to gFmtRowCount - 1
                    if gFmtRows(i).nPage = p then n += 1
                next
                if n = 0 then bEveryPageUsed = false
            next
            CHECK( bEveryPageUsed, "every nav page has at least one rule row" )

            ' The one row whose control index and stored value differ. Index 0 must mean the
            ' -1 leave-alone sentinel, and getting this backwards silently turns leave-alone
            ' into zero-blank-lines -- which DELETES blank lines rather than
            ' preserving them.
            dim as long nOff = -1
            for i as long = 0 to gFmtRowCount - 1
                if gFmtRows(i).pField = @gFmtWork.BlankLinesAroundProc then nOff = gFmtRows(i).nOffset
            next
            CHECK( nOff = 1, "BlankLinesAroundProc carries the +1 index offset" )
            CHECK( 0 - nOff = -1, "combo index 0 maps to the leave-alone sentinel" )

            gFmtWork = rSave
        end scope

        ' ---- the defaults are conservative -------------------------------------------------
        ' A formatter that rewrites a user's whole file the first time they press the
        ' shortcut, in ways they never asked for, is one they turn off and never turn back
        ' on. These four assertions are what stop a future edit quietly changing that.
        scope
            dim as FORMAT_RULES d = Format_DefaultRules()
            CHECK( d.CaseKeywords = FMTCASE_LEAVE, "casing ships as Leave alone" )
            CHECK( d.MaxBlankLines = 0, "blank-line capping ships off" )
            CHECK( d.BlankLinesAroundProc = -1, "procedure spacing ships off" )
            CHECK( d.FormatOnPaste = 0, "format-on-paste ships off" )
        end scope

        dim as string sReport1 = "frmFormatOptions self-test (headless): " & str(nPass) & " passed, " & str(nFail) & " failed"
        print sReport1
        exit sub
    end if

    ' ==== LAYOUT HALF -- real windows, real DPI ==========================================
    print "frmFormatOptions self-test (layout)"

    dim as RECT rcClient
    GetClientRect( HWND_FRMFORMATOPTIONS, @rcClient )
    CHECK( rcClient.right > 0 andalso rcClient.bottom > 0, "the dialog has a client area" )

    ' Every control exists and is a child of this dialog. A control stranded with no parent
    ' or never created answers correctly to everything and simply never appears -- which no
    ' arithmetic assertion can see.
    scope
        dim as long bAllMade = true
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).hCtrl = 0 then bAllMade = false
            if IsWindow( gFmtRows(i).hCtrl ) = 0 then bAllMade = false
        next
        CHECK( bAllMade, "every rule row has a real control window" )
        CHECK( IsWindow(ghFmtPreview) <> 0, "the preview window exists" )
        CHECK( IsWindow(HWND_FRMFORMATOPTIONS_NAV) <> 0, "the nav list exists" )
        CHECK( IsWindow(ghFmtOK) <> 0 andalso IsWindow(ghFmtCancel) <> 0, "OK and Cancel exist" )
    end scope

    ' Nothing on the visible page may overlap the preview or run off the right edge.
    scope
        dim as RECT rcPrev
        GetWindowRect( ghFmtPreview, @rcPrev )
        MapWindowPoints( 0, HWND_FRMFORMATOPTIONS, cast(POINT ptr, @rcPrev), 2 )

        dim as long bClear = true
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).nPage <> gFmtPage then continue for
            if gFmtRows(i).rc.right > rcPrev.left then bClear = false
            if gFmtRows(i).rc.right > rcClient.right then bClear = false
        next
        CHECK( bClear, "no rule row overlaps the preview or the right edge" )

        CHECK( rcPrev.right <= rcClient.right, "the preview is inside the client area" )
        CHECK( rcPrev.top >= AfxScaleY(FRMFORMATOPTIONS_TITLE_H), "the preview clears the title band" )
    end scope

    ' The label span must be positive on every row. A control pinned right with a caption
    ' taking what is left means a wide enough control leaves the label NEGATIVE room, and
    ' DrawText then clips with DT_END_ELLIPSIS and reports nothing at all.
    scope
        dim as long bLabelsFit = true
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).rcLabel.right <= gFmtRows(i).rcLabel.left then bLabelsFit = false
        next
        CHECK( bLabelsFit, "every rule label has a positive span" )
    end scope

    ' AND THAT THE TEXT ACTUALLY FITS IN THAT SPAN, which is a different claim and the one
    ' that matters. A positive span says nothing about whether the caption is being clipped:
    ' DrawText with DT_END_ELLIPSIS truncates silently and reports nothing at all, so the
    ' only way to know is to MEASURE the string against the rect. (frmBuildConfig's General
    ' page needed exactly this assertion for exactly this reason.)
    '
    ' This is also what makes the per-kind control widths above load-bearing rather than
    ' cosmetic: with one shared column width, the longest toggle caption overflows here.
    scope
        dim as HDC hdcM = GetDC( HWND_FRMFORMATOPTIONS )
        dim as HFONT hOld = SelectObject( hdcM, ghFont(GUIFONT_11) )

        dim as long bFits = true
        dim as long nWorstOver = 0
        dim as string sWorst = ""
        for i as long = 0 to gFmtRowCount - 1
            dim as DWSTRING wszLabel = L( gFmtRows(i).nLabelID, "" )
            dim as SIZE sz
            GetTextExtentPoint32W( hdcM, wszLabel, len(wszLabel), @sz )
            dim as long nHave = gFmtRows(i).rcLabel.right - gFmtRows(i).rcLabel.left
            if sz.cx > nHave then
                bFits = false
                if (sz.cx - nHave) > nWorstOver then
                    nWorstOver = sz.cx - nHave
                    sWorst = str(sz.cx) & "px into " & str(nHave) & "px: " & wszLabel
                end if
            end if
        next

        SelectObject( hdcM, hOld )
        ReleaseDC( HWND_FRMFORMATOPTIONS, hdcM )

        CHECK( bFits, "every rule label FITS its span, unclipped" )
        ' Name the worst offender rather than just failing -- "a label is clipped" sends you
        ' hunting; "486px into 403px: <caption>" tells you which one and by how much.
        if bFits = false then
            dim as string sOver = "    worst: " & sWorst
            print sOver
        end if
    end scope

    ' The Preview caption band is exactly one nav row tall, so the heading lines up with the
    ' category list beside it. Asserted because the two numbers live in different functions
    ' -- the layout reserves the band, the painter draws into it -- and nothing else would
    ' notice them drifting apart.
    scope
        dim as RECT rcP
        GetWindowRect( ghFmtPreview, @rcP )
        MapWindowPoints( 0, HWND_FRMFORMATOPTIONS, cast(POINT ptr, @rcP), 2 )
        dim as long nBand = rcP.top - (AfxScaleY(FRMFORMATOPTIONS_TITLE_H) + AfxScaleX(FRMFORMATOPTIONS_MARGIN))
        ' Truncated into a long FIRST, exactly as the layout does. AfxScaleY returns a
        ' float: at 175% AfxScaleY(30) is 52.5, the layout stores 52, and comparing the
        ' stored value against the inline expression fails 52 = 52.5 forever. The same
        ' expression evaluated at two precisions is not the same number.
        dim as long nWantBand = AfxScaleY(FRMFORMATOPTIONS_NAVROW_H)
        CHECK( nBand = nWantBand, "the Preview caption band is one nav row tall" )
        ' And that it clears the title band's hairline, which the original 20px-above-the-box
        ' caption did not at this y.
        dim as long nTitleBand = AfxScaleY(FRMFORMATOPTIONS_TITLE_H)
        CHECK( rcP.top - nWantBand >= nTitleBand, "the Preview caption clears the title band" )
    end scope

    ' EVERY CONTROL IS AT LEAST ITS OWN IDEAL HEIGHT, and fits inside its row.
    '
    ' This is the assertion that was missing when a hardcoded AfxScaleY(26) gave the numeric
    ' fields 46px where they needed 56: the control still answered every query correctly, the
    ' layout still placed it without overlapping anything, and the only symptom was a digit
    ' with its top and bottom sliced off -- invisible to arithmetic, and invisible at 100%
    ' DPI where the two numbers happen to agree.
    scope
        dim as long bTallEnough = true, bInRow = true
        dim as long nWorstShort = 0
        dim as string sTight = ""
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).hCtrl = 0 then continue for
            dim as long iw = 0, ih = 0
            select case gFmtRows(i).nKind
                case FMTROW_TOGGLE  : PsToggle_GetIdealSize( gFmtRows(i).hCtrl, iw, ih )
                case FMTROW_NUMERIC : PsNumericUpDown_GetIdealSize( gFmtRows(i).hCtrl, iw, ih )
                case FMTROW_COMBO   : PsComboBox_GetIdealSize( gFmtRows(i).hCtrl, iw, ih )
            end select

            dim as RECT rcC
            GetWindowRect( gFmtRows(i).hCtrl, @rcC )
            dim as long nH = rcC.bottom - rcC.top

            ' Report the WORST shortfall, not the last one found. The first version kept
            ' the last and, under an A/B that broke every row at once, printed a 1px toggle
            ' while the numeric fields beside it were short by 10 -- an understated number
            ' that sends you looking at the wrong control.
            if nH < ih then
                bTallEnough = false
                if (ih - nH) > nWorstShort then
                    nWorstShort = ih - nH
                    sTight = "row " & str(i) & " is " & str(nH) & "px, needs " & str(ih) & "px"
                end if
            end if
            if nH > (gFmtRows(i).rc.bottom - gFmtRows(i).rc.top) then bInRow = false
        next
        CHECK( bTallEnough, "every control is at least its own ideal height" )
        if bTallEnough = false then
            dim as string sMsgT = "    " & sTight
            print sMsgT
        end if
        CHECK( bInRow, "no control is taller than the row that holds it" )
    end scope

    ' The nav list leads with When To Format, and the dialog OPENS on whatever row 0 holds.
    ' Asserted as an agreement rather than as two constants: a reordered nav that still
    ' hardcoded its landing page would highlight one row and display another.
    scope
        CHECK( PsListTree_GetItemData( HWND_FRMFORMATOPTIONS_NAV, 0 ) = FMTPAGE_TRIGGERS, _
               "When To Format is the first nav row" )
        CHECK( PsListTree_GetCount( HWND_FRMFORMATOPTIONS_NAV ) = FMTPAGE_COUNT, _
               "every page has a nav row" )
    end scope

    ' Rows must not run past the footer -- a page that overflows loses its last rule with
    ' nothing to say so.
    scope
        dim as long yFooter = rcClient.bottom - AfxScaleY(FRMFORMATOPTIONS_FOOTER_H)
        dim as long bFits = true
        for i as long = 0 to gFmtRowCount - 1
            if gFmtRows(i).rc.bottom > yFooter then bFits = false
        next
        CHECK( bFits, "no page overflows into the footer" )
    end scope

    ' THE PREVIEW IS ACTUALLY WIRED TO THE ENGINE. Asserted by changing a rule and reading
    ' the pane back: if RefreshPreview were stubbed, or bound to a stored sample instead of
    ' Format_Text, the text would not move.
    scope
        dim as FORMAT_RULES rSave = gFmtWork
        dim as long nSavePage = gFmtPage

        gFmtPage = FMTPAGE_INDENT
        gFmtWork.Reindent = 0
        frmFormatOptions_RefreshPreview()
        dim as string sOff = PsTextBox_GetText( ghFmtPreview )

        gFmtWork.Reindent = 1
        frmFormatOptions_RefreshPreview()
        dim as string sOn = PsTextBox_GetText( ghFmtPreview )

        CHECK( len(sOff) > 0 andalso len(sOn) > 0, "the preview pane holds text" )
        CHECK( sOff <> sOn, "toggling a rule changes what the preview shows" )

        gFmtWork = rSave
        gFmtPage = nSavePage
        frmFormatOptions_RefreshPreview()
    end scope

    dim as string sReport2 = "frmFormatOptions self-test: " & str(nPass) & " passed, " & str(nFail) & " failed"
    print sReport2
end sub
