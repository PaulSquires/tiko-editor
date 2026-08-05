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

#include once "frmAbout.bi"

' ========================================================================================
' Which page is showing. Owned here rather than read back from the select bar, because the
' hero and the About page are PAINTED and WM_PAINT must be able to answer the question
' without touching a control.
' ========================================================================================
dim shared as long gAboutPage

' Which licence the Licenses page is showing. Held rather than read back from the sub-strip
' because the strip is created after the first SetText and a re-theme must not disturb it.
dim shared as long gAboutLicPage

' The wordmark on the icon plate. Two lowercase letters, NOT the application icon: the .ico
' is a detailed raster that reads as clutter at 72px on a plate this size, and it is already
' on the taskbar and the caption bar of every tiko window. Drawn as text in the editor's
' keyword colour, so it costs no image decode, no GDI+ object and no resource lookup, and it
' recolours with the theme for free.
#define FRMABOUT_WORDMARK   wstr("tk")


' ========================================================================================
' THE LICENCE TEXT
'
' READ FROM THE SHIPPED FILE, not held as a string literal. The dialog used to carry a
' four-paragraph paraphrase of the GPL notice; with tiko dual-licensed there would now be
' two such paraphrases, and a summary in the source is a second statement of a licence
' that drifts from the file the moment either is edited. The file beside the exe is the
' one that ships and the one that governs, so it is the one shown.
'
' DELIBERATELY NOT LOCALIZED, for two independent reasons that both still hold.
'
' The first is legal: these are the licences' own texts. The FSF asks that the GPL be
' reproduced verbatim, and a community translation of a licence is not that licence.
'
' The second is mechanical and is worth knowing before putting ANY prose in a .lang file:
' LoadLocalizationFile runs every value through PsStrParse(wData, 1, ";"), which treats a
' semicolon as the start of a trailing comment and DISCARDS the rest of the line. The GPL's
' warranty paragraph contains one ("WITHOUT ANY WARRANTY; without even..."), so stored as a
' localized string it would silently lose two thirds of itself, with nothing in the build,
' the warnings or the assertions to say so.
'
' A MISSING FILE RETURNS A NOTICE, NEVER AN EMPTY FIELD. An install that dropped the
' licence files would otherwise show a blank page with nothing to say why -- and the
' fallback names the file, so the report that comes back is actionable.
' ========================================================================================
private function frmAbout_LicenseText( byval nWhich as long ) as DWSTRING

    dim as DWSTRING nl = chr(13) & chr(10)

    dim as DWSTRING wszFile = iif( nWhich = LICPAGE_PROPRIETARY, _
                                   FRMABOUT_FILE_PROPRIETARY, FRMABOUT_FILE_GPL )
    dim as DWSTRING wszPath = PsExePath & wszFile

    if PsFileExists( wszPath ) then
        dim pStream as CTextStream
        if pStream.Open( wszPath ) = S_OK then
            dim as DWSTRING wszText = pStream.ReadAll
            pStream.Close
            if PsLen(wszText) > 0 then return wszText
        end if
    end if

    ' RETURN throughout, not "function =": mixing the two styles is unsupported for a type
    ' with constructors, and the early exit above is what a DWSTRING return costs here.
    return APPNAME & " -- " & APPCOPYRIGHT & nl & nl & _
        "The licence file " & wszFile & " was not found beside the program, or could not " & _
        "be read." & nl & nl & _
        "tiko is licensed under the GNU General Public License version 3 or later, with " & _
        "the exception in LICENSE-EXCEPTION.txt. The fbcParser and debugParser components " & _
        "are proprietary and are covered by LICENSE-PROPRIETARY.txt. All three files are " & _
        "distributed with tiko and are available at " & FRMABOUT_URL_GITHUB & "."

end function


' ========================================================================================
' THE CREDITS ROLL
'
' A table rather than six hand-placed labels, which is precisely what the dialog this
' replaces got wrong: each contributor there was a LABEL at a literal Y, so adding one meant
' editing coordinates. Here a new line is a new row.
'
' The "for" text is localized and the name is not -- a person's name does not translate.
' ========================================================================================
private sub frmAbout_BuildCredits()

    if HWND_FRMABOUT_CREDITS = 0 then exit sub

    PsListTree_Clear( HWND_FRMABOUT_CREDITS )

    dim as long nRow

    nRow = PsListTree_AddString( HWND_FRMABOUT_CREDITS, "Jos" & wchr(233) & " Roca", 0 )
    PsListTree_SetCellText( HWND_FRMABOUT_CREDITS, nRow, 1, L(370,"AfxNova framework") )

    nRow = PsListTree_AddString( HWND_FRMABOUT_CREDITS, "Paul Doe", 1 )
    PsListTree_SetCellText( HWND_FRMABOUT_CREDITS, nRow, 1, L(371,"Ideas and testing") )

    nRow = PsListTree_AddString( HWND_FRMABOUT_CREDITS, "ImortisInglorian", 2 )
    PsListTree_SetCellText( HWND_FRMABOUT_CREDITS, nRow, 1, L(371,"Ideas and testing") )

    nRow = PsListTree_AddString( HWND_FRMABOUT_CREDITS, "Laurent GRAS (SARG)", 3 )
    PsListTree_SetCellText( HWND_FRMABOUT_CREDITS, nRow, 1, L(371,"Ideas and testing") )

    nRow = PsListTree_AddString( HWND_FRMABOUT_CREDITS, "Neil Hodgson", 4 )
    PsListTree_SetCellText( HWND_FRMABOUT_CREDITS, nRow, 1, L(372,"Scintilla editing component") )

    nRow = PsListTree_AddString( HWND_FRMABOUT_CREDITS, "FreeBASIC Development Team", 5 )
    PsListTree_SetCellText( HWND_FRMABOUT_CREDITS, nRow, 1, L(373,"The compiler tiko is built on and for") )


    ' EVERY ROW IS NON-SELECTABLE. This is a roll of names, not a picker: there is nothing to
    ' do with a selected contributor, so a highlight that follows the mouse is an affordance
    ' that promises an action the dialog does not have.
    '
    ' bSelectable = false blocks the mouse, the keyboard AND the programmatic setters, so it
    ' is the whole answer for selection. It does NOT stop hot tracking -- the control still
    ' reports isHot on the row under the cursor -- which is why the row painter ignores both
    ' flags rather than relying on this alone. The two have to be turned off in two places.
    for i as long = 0 to PsListTree_GetCount( HWND_FRMABOUT_CREDITS ) - 1
        PsListTree_SetRowSelectable( HWND_FRMABOUT_CREDITS, i, false )
    next

end sub


' ========================================================================================
' Row painter for the Credits list.
'
' The left column is drawn in the EDITOR'S KEYWORD colour, which is the one small thing that
' makes this box read as belonging to a code editor. It is also why the painter is written
' out here rather than reusing ListBox_LVPaintRow from modRoutines.inc: that one paints the
' Output panel's palette and has no concept of a keyword-coloured column.
' ========================================================================================
private sub frmAbout_CreditsPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub

    ' p->isSelected AND p->isHot ARE DELIBERATELY IGNORED. Every row is painted the same way,
    ' so the roll reads as a table of credits rather than as a list you are meant to pick from.
    ' Marking the rows non-selectable (frmAbout_BuildCredits) stops selection; it does NOT
    ' stop the control reporting isHot for the row under the cursor, so the highlight has to
    ' be refused here as well. Reading either flag is what would put it back.
    p->b->SetBackColor( theme.panel.text.backcolor )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_11) )

    if p->columnCount <= 0 then exit sub

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(10)

        ' Column 0 -- the contributor -- takes the editor's keyword colour. That one detail is
        ' what makes the box read as belonging to a code editor.
        dim as COLORREF clr = iif( i = 0, theme.editor.keyword1.forecolor, theme.ui.field.forecolor )

        p->b->SetForeColor( clr )
        p->b->PaintText( p->cells[i].wszText, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next

end sub


' ========================================================================================
' Header band painter. REQUIRED -- without one the header does not render at all.
' ========================================================================================
private sub frmAbout_CreditsPaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub

    p->b->SetBackColor( theme.panel.text.backcolor )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(10)
    p->b->SetForeColor( theme.panel.text.forecolor )
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.ui.divider )
    p->b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' Both tooltip callbacks return "" to SUPPRESS tips.
'
' With no callback set, PsListTree shows the row's own text back at the cursor and
' PsColumnHeader shows the column's own caption. Every cell here is fully visible, so the
' fallback would pop a tip repeating what is already under the pointer. These are TWO
' SEPARATE opt-outs -- suppressing the rows does nothing for the header band.
' ========================================================================================
private function frmAbout_CreditsTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    return ""
end function

private function frmAbout_CreditsHeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' The four band rectangles, derived from the client rect. Every consumer -- the painter, the
' positioner and the self-test -- goes through this, so there is one description of the
' layout rather than three that can drift.
' ========================================================================================
private sub frmAbout_GetBands( _
            byval hwnd as HWND, _
            byref rcHero as RECT, _
            byref rcTabs as RECT, _
            byref rcBody as RECT, _
            byref rcFoot as RECT _
            )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim as long heroH = AfxScaleY(FRMABOUT_HERO_H)
    dim as long tabsH = AfxScaleY(FRMABOUT_TABS_H)
    dim as long footH = AfxScaleY(FRMABOUT_FOOTER_H)

    rcHero = type<RECT>( 0, 0, rc.right, heroH )
    rcTabs = type<RECT>( 0, heroH, rc.right, heroH + tabsH )
    rcBody = type<RECT>( 0, heroH + tabsH, rc.right, rc.bottom - footH )
    rcFoot = type<RECT>( 0, rc.bottom - footH, rc.right, rc.bottom )

end sub


' ========================================================================================
' The icon plate's rect, in client coords. Shared by the painter and the self-test so there
' is one description of where the plate is rather than two that can drift.
' ========================================================================================
private sub frmAbout_GetPlateRect( byval rcHero as RECT ptr, byref rcPlate as RECT )

    dim as long plateW = AfxScaleX(FRMABOUT_PLATE)
    dim as long plateH = AfxScaleY(FRMABOUT_PLATE)

    rcPlate.left   = AfxScaleX(FRMABOUT_MARGIN)
    rcPlate.top    = rcHero->top + (((rcHero->bottom - rcHero->top) - plateH) \ 2)
    rcPlate.right  = rcPlate.left + plateW
    rcPlate.bottom = rcPlate.top + plateH

end sub


' ========================================================================================
' Measure a pill: its text width plus the padding on both sides.
'
' DT_CALCRECT through PaintTextEx, because PaintText forces DT_VCENTER or DT_SINGLELINE and
' would fight the flag. PaintTextEx passes the caller's flags through untouched, which is
' exactly what this needs -- and it measures in the font the buffer currently holds, so the
' caller must SetFont first.
' ========================================================================================
private function frmAbout_MeasurePill( _
            byval pb as PsBufferPaint ptr, _
            byval wszText as DWSTRING _
            ) as long

    dim as RECT rc
    rc.right = 4000
    pb->PaintTextEx( wszText, @rc, DT_LEFT or DT_SINGLELINE or DT_CALCRECT )
    function = (rc.right - rc.left) + (AfxScaleX(FRMABOUT_PILL_PADX) * 2)

end function


' ========================================================================================
' Draw one pill. Filled = the accent; ghost = a divider outline over whatever is behind it.
'
' The curvature is the pill's own HEIGHT, i.e. an ellipse diameter equal to the height, which
' is what makes the ends true semicircles. PaintRoundOutline strokes WITHOUT filling --
' PaintRoundBorderRect would fill the gradient back over first, which is the mistake this
' family has made five times.
' ========================================================================================
private sub frmAbout_PaintPill( _
            byval pb as PsBufferPaint ptr, _
            byval rc as RECT ptr, _
            byval wszText as DWSTRING, _
            byval bFilled as boolean _
            )

    dim as long curve = rc->bottom - rc->top

    if bFilled then
        pb->SetBackColor( theme.ui.accent )
        pb->PaintRoundRect( rc, curve )
    else
        pb->SetPenColor( theme.ui.divider )
        pb->PaintRoundOutline( rc, curve, 1 )
    end if

    pb->SetForeColor( theme.panel.text.forecolor )
    pb->PaintText( wszText, rc, DT_CENTER or DT_VCENTER or DT_SINGLELINE )

end sub


' ========================================================================================
' The hero band: gradient, icon plate, application name, tagline, two pills.
' ========================================================================================
private sub frmAbout_PaintHero( byval pb as PsBufferPaint ptr, byval rcHero as RECT ptr )

    ' A HORIZONTAL wash from the accent into the page colour, so the band ends flush with the
    ' tab strip below it and the two read as one surface. Running the ramp to the full width
    ' keeps it a TINT of the page rather than a block of colour -- which is what makes it
    ' survive an accent that is nearly white (default_light) as well as one that is nearly
    ' black (default_dark).
    pb->PaintGradientRect( rcHero, theme.ui.accent, theme.panel.text.backcolor, LinearGradientModeHorizontal )

    dim as long mx = AfxScaleX(FRMABOUT_MARGIN)

    ' ---- the icon plate --------------------------------------------------------------
    dim as RECT rcPlate
    frmAbout_GetPlateRect( rcHero, rcPlate )

    pb->SetBackColor( theme.ui.field.backcolor )
    pb->PaintRoundRect( @rcPlate, AfxScaleX(FRMABOUT_PLATE_CURVE) )
    ' The ring is a DIVIDER, not the accent: an accent hairline is invisible against the pale
    ' accent of the light theme.
    pb->SetPenColor( theme.ui.divider )
    pb->PaintRoundOutline( @rcPlate, AfxScaleX(FRMABOUT_PLATE_CURVE), 1 )

    ' The wordmark, centred on the PLATE rather than on an inset rect: DT_CENTER/DT_VCENTER
    ' already centre the ink, and insetting first would only shrink the box the same two
    ' letters get centred in.
    pb->SetFont( ghFont(FIXEDFONT_26) )
    pb->SetForeColor( theme.editor.keyword1.forecolor )
    pb->PaintText( FRMABOUT_WORDMARK, @rcPlate, DT_CENTER or DT_VCENTER or DT_SINGLELINE )

    ' ---- the text column -------------------------------------------------------------
    dim as long tx = rcPlate.right + AfxScaleX(FRMABOUT_HERO_GAP)

    ' The name is measured, not guessed, so the tagline and the pills sit under whatever
    ' height 26pt actually comes out at on this machine's DPI.
    pb->SetFont( ghFont(GUIFONT_26) )
    dim as RECT rcName
    rcName.right = 4000
    pb->PaintTextEx( APPNAME, @rcName, DT_LEFT or DT_SINGLELINE or DT_CALCRECT )
    dim as long nameH = rcName.bottom - rcName.top

    pb->SetFont( ghFont(GUIFONT_11) )
    dim as RECT rcTag
    rcTag.right = 4000
    pb->PaintTextEx( "Xy", @rcTag, DT_LEFT or DT_SINGLELINE or DT_CALCRECT )
    dim as long lineH = rcTag.bottom - rcTag.top

    dim as long pillH = AfxScaleY(FRMABOUT_PILL_H)
    dim as long gap1  = AfxScaleY(6)
    dim as long gap2  = AfxScaleY(14)

    dim as long blockH = nameH + gap1 + lineH + gap2 + pillH
    dim as long y      = ((rcHero->bottom - rcHero->top) - blockH) \ 2

    dim as RECT r
    r.left = tx : r.right = rcHero->right - mx

    r.top = y : r.bottom = y + nameH
    pb->SetFont( ghFont(GUIFONT_26) )
    pb->SetForeColor( theme.ui.field.forecolor )
    pb->PaintText( APPNAME, @r, DT_LEFT or DT_VCENTER or DT_SINGLELINE )

    y += nameH + gap1
    r.top = y : r.bottom = y + lineH
    pb->SetFont( ghFont(GUIFONT_11) )
    pb->SetForeColor( theme.panel.text.forecolor )
    pb->PaintText( L(359,"Programmer's code editor for the FreeBASIC compiler"), _
                   @r, DT_LEFT or DT_VCENTER or DT_SINGLELINE )

    ' ---- the three pills ---------------------------------------------------------------
    '
    ' BOTH LICENCES ARE NAMED HERE, not just the GPL. The hero is the one thing every user
    ' of this dialog reads, and a lone "GPL v3" pill on a program that ships two proprietary
    ' DLLs states something that is not true of the whole product.
    y += lineH + gap2

    dim as DWSTRING wszVersion = APPVERSION & "  " & wchr(183) & "  " & trim(APPBITS, any " ()")
    dim as DWSTRING wszLicense = "GPL v3"
    dim as DWSTRING wszPropr   = L(397,"Proprietary")

    dim as long w1 = frmAbout_MeasurePill( pb, wszVersion )
    dim as long w2 = frmAbout_MeasurePill( pb, wszLicense )
    dim as long w3 = frmAbout_MeasurePill( pb, wszPropr )

    dim as RECT rcPill
    rcPill.left   = tx
    rcPill.top    = y
    rcPill.right  = tx + w1
    rcPill.bottom = y + pillH
    frmAbout_PaintPill( pb, @rcPill, wszVersion, true )

    rcPill.left   = rcPill.right + AfxScaleX(FRMABOUT_PILL_GAP)
    rcPill.right  = rcPill.left + w2
    frmAbout_PaintPill( pb, @rcPill, wszLicense, false )

    rcPill.left   = rcPill.right + AfxScaleX(FRMABOUT_PILL_GAP)
    rcPill.right  = rcPill.left + w3
    frmAbout_PaintPill( pb, @rcPill, wszPropr, false )

end sub


' ========================================================================================
' Where the pill row ends. Split out of the painter so the self-test can ask whether three
' pills still fit the hero at this DPI in this language -- the painter measures them and
' then draws, and a pill that ran off the edge would simply be clipped in silence.
' ========================================================================================
private function frmAbout_PillRowRight( byval pb as PsBufferPaint ptr, byval rcHero as RECT ptr ) as long

    dim as long tx = AfxScaleX(FRMABOUT_MARGIN) + AfxScaleX(FRMABOUT_PLATE) + AfxScaleX(FRMABOUT_HERO_GAP)

    pb->SetFont( ghFont(GUIFONT_11) )

    dim as DWSTRING wszVersion = APPVERSION & "  " & wchr(183) & "  " & trim(APPBITS, any " ()")

    dim as long w = frmAbout_MeasurePill( pb, wszVersion )
    w += AfxScaleX(FRMABOUT_PILL_GAP) + frmAbout_MeasurePill( pb, "GPL v3" )
    w += AfxScaleX(FRMABOUT_PILL_GAP) + frmAbout_MeasurePill( pb, L(397,"Proprietary") )

    function = tx + w

end function


' ========================================================================================
' The About page. Painted, not built from controls -- it is a paragraph and three rows, and
' three PsTextBoxes would be three tab stops the user has no reason to visit.
' ========================================================================================
private sub frmAbout_PaintAboutPage( byval pb as PsBufferPaint ptr, byval rcBody as RECT ptr )

    dim as long mx  = AfxScaleX(FRMABOUT_MARGIN)
    dim as long pad = AfxScaleY(FRMABOUT_BODY_PAD)

    pb->SetFont( ghFont(GUIFONT_11) )

    ' ---- the description paragraph ---------------------------------------------------
    ' DT_WORDBREAK needs PaintTextEx: PaintText forces DT_SINGLELINE and structurally cannot
    ' wrap. Measured first with DT_CALCRECT so the rows below start under however many lines
    ' the translated text actually took.
    dim as DWSTRING wszDesc = L(360,"A fast, native Win32 editor for FreeBASIC -- project management, symbol browsing, integrated build and debug -- in one executable with no runtime to install.")

    dim as RECT rcText
    rcText.left   = mx
    rcText.top    = rcBody->top + pad + AfxScaleY(4)
    rcText.right  = rcBody->right - mx
    rcText.bottom = rcBody->bottom

    dim as RECT rcCalc = rcText
    pb->PaintTextEx( wszDesc, @rcCalc, DT_LEFT or DT_WORDBREAK or DT_CALCRECT )
    dim as long descH = rcCalc.bottom - rcCalc.top

    rcText.bottom = rcText.top + descH
    pb->SetForeColor( theme.panel.text.forecolor )
    pb->PaintTextEx( wszDesc, @rcText, DT_LEFT or DT_WORDBREAK )

    ' ---- the key / value rows --------------------------------------------------------
    dim as long y     = rcText.bottom + AfxScaleY(18)
    dim as long rowH  = AfxScaleY(FRMABOUT_KV_ROWH)
    dim as long keyW  = AfxScaleX(FRMABOUT_KV_KEYW)

    dim as DWSTRING wszKey(2), wszVal(2)
    wszKey(0) = L(365,"Build date")      : wszVal(0) = __DATE_ISO__
    wszKey(1) = L(366,"Copyright")       : wszVal(1) = APPCOPYRIGHT
    wszKey(2) = L(367,"Settings folder") : wszVal(2) = PsExePath & "settings"

    for i as long = 0 to 2
        dim as RECT rcRow
        rcRow.left   = mx
        rcRow.top    = y
        rcRow.right  = mx + keyW
        rcRow.bottom = y + rowH

        pb->SetForeColor( theme.editor.keyword1.forecolor )
        pb->PaintText( wszKey(i), @rcRow, DT_LEFT or DT_VCENTER or DT_SINGLELINE )

        rcRow.left  = mx + keyW
        rcRow.right = rcBody->right - mx
        pb->SetForeColor( theme.ui.field.forecolor )
        pb->PaintText( wszVal(i), @rcRow, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

        y += rowH
    next

end sub


' ========================================================================================
' The dialog's own chrome. Everything else on screen belongs to a child control.
' ========================================================================================
private function frmAbout_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    ' The whole client in the page colour first. The hero paints over its own band.
    buffer.SetBackColor( theme.panel.text.backcolor )
    buffer.PaintRect( @rc )

    dim as RECT rcHero, rcTabs, rcBody, rcFoot
    frmAbout_GetBands( hwnd, rcHero, rcTabs, rcBody, rcFoot )

    frmAbout_PaintHero( @buffer, @rcHero )

    if gAboutPage = ABOUTPAGE_ABOUT then
        frmAbout_PaintAboutPage( @buffer, @rcBody )
    end if

    ' The footer hairline. A DIVIDER, for the same reason the plate's ring is one.
    dim as RECT rcDiv = rcFoot
    rcDiv.bottom = rcDiv.top + AfxScaleY(1)
    buffer.SetBackColor( theme.ui.divider )
    buffer.PaintRect( @rcDiv )

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Swap the body page.
'
' The two body controls are SHOWN AND HIDDEN, never created and destroyed: they are built
' once so the Credits list keeps its selection and the License field its scroll position
' across a round trip through the tabs.
' ========================================================================================
private sub frmAbout_ShowPage( byval hwnd as HWND, byval nPage as long )

    if (nPage < ABOUTPAGE_ABOUT) orelse (nPage > ABOUTPAGE_LICENSE) then exit sub

    gAboutPage = nPage

    ShowWindow( HWND_FRMABOUT_CREDITS, iif( nPage = ABOUTPAGE_CREDITS, SW_SHOW, SW_HIDE ) )
    ShowWindow( HWND_FRMABOUT_LICENSE, iif( nPage = ABOUTPAGE_LICENSE, SW_SHOW, SW_HIDE ) )
    ' The GPL-3 / Proprietary strip and the field's scrollbar both belong to the Licenses
    ' page and travel with it. The strip in particular MUST be hidden off the page: it sits
    ' on top of the tab strip, so leaving it up would paint two licence captions over the
    ' right-hand end of a tab row that has nothing to do with them.
    ShowWindow( HWND_FRMABOUT_LICSEL,     iif( nPage = ABOUTPAGE_LICENSE, SW_SHOW, SW_HIDE ) )
    ShowWindow( HWND_FRMABOUT_LICVSCROLL, iif( nPage = ABOUTPAGE_LICENSE, SW_SHOW, SW_HIDE ) )

    ' The About page is PAINTED, so leaving a page has to invalidate the body band -- hiding a
    ' child window does not repaint what was under it.
    dim as RECT rcHero, rcTabs, rcBody, rcFoot
    frmAbout_GetBands( hwnd, rcHero, rcTabs, rcBody, rcFoot )
    InvalidateRect( hwnd, @rcBody, true )

end sub


' ========================================================================================
' Tab strip. USER-DRIVEN CHANGES ONLY -- PsSelectBar does not fire this for SetCurSel, which
' is what lets frmAbout_Show seed the current tab without this running.
' ========================================================================================
private sub frmAbout_TabChanged( byval hSelectBar as HWND, byval idxOld as long, byval idxNew as long )
    frmAbout_ShowPage( GetParent(hSelectBar), idxNew )
end sub


' ========================================================================================
' Swap which licence the Licenses page shows.
'
' The text is re-read from disk on every switch rather than cached in two DWSTRINGs. The
' files are small, this runs on a mouse click, and holding no copy means the dialog cannot
' show a licence that disagrees with the file shipped beside it. The scroll position going
' back to the top is correct here: it is a different document, not the same one re-shown.
' ========================================================================================
private sub frmAbout_ShowLicense( byval nWhich as long )

    if HWND_FRMABOUT_LICENSE = 0 then exit sub
    if (nWhich < LICPAGE_GPL) orelse (nWhich > LICPAGE_PROPRIETARY) then exit sub

    gAboutLicPage = nWhich
    PsTextBox_SetText( HWND_FRMABOUT_LICENSE, frmAbout_LicenseText( nWhich ) )

end sub


' ========================================================================================
' The GPL-3 / Proprietary strip. USER-DRIVEN ONLY, like the tab strip above it.
' ========================================================================================
private sub frmAbout_LicenseTabChanged( byval hSelectBar as HWND, byval idxOld as long, byval idxNew as long )
    frmAbout_ShowLicense( idxNew )
end sub


' ========================================================================================
' THE LICENCE FIELD'S SCROLLBAR
'
' PsTextBox does not draw one. It scrolls itself (wheel, caret, SetText) and reports the
' state through a PsVScrollBar-shaped API in LINE units; supplying the bar is the host's
' job. This is frmOutput's Notes/Log wiring exactly, and there is no reason for it to be
' different -- the GPL alone is ~35KB, so a page with no bar gives the reader no idea how
' much document is under them, or any way to move through it but the wheel.
'
' Two callbacks and no shared state between them. The textbox reports a fresh range on
' every scroll; the bar reports a new first line on every drag or page. During a thumb
' drag the round trip echoes the same position straight back -- consistent by
' construction, so there is nothing to guard against oscillation.
' ========================================================================================
sub frmAbout_LicenseScrollChanged( byval hTextBox as HWND )
    if HWND_FRMABOUT_LICVSCROLL = 0 then exit sub
    dim as integer nTotalLines, nLinesPerPage, nFirstVisible
    PsTextBox_GetVScrollInfo( hTextBox, nTotalLines, nLinesPerPage, nFirstVisible )
    PsVScrollBar_SetRange( HWND_FRMABOUT_LICVSCROLL, nTotalLines, nLinesPerPage, nFirstVisible )
end sub

sub frmAbout_LicenseVScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    if HWND_FRMABOUT_LICENSE = 0 then exit sub
    PsTextBox_ScrollToLine( HWND_FRMABOUT_LICENSE, newPos )
end sub

' Flat track, a divider on the inner edge and a flat thumb, matching the Output panel's
' bars. Reads the theme live, so a theme switch needs only a repaint. The thumb rect is
' EMPTY when there is nothing to scroll, which is what draws the bare track for a licence
' shorter than the field.
sub frmAbout_LicenseVScrollPaint( byval p as PSVSCROLLBAR_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b
    b->SetBackColor( theme.panel.scrollbar.backcolor )
    b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetPenColor( theme.ui.divider )
        b->PaintLine( 1, p->rcClient.left, p->rcClient.top, p->rcClient.left, p->rcClient.bottom )
        b->SetBackColor( theme.panel.scrollbar.forecolor )
        b->PaintRect( @p->rcThumb )
    end if
end sub


' ========================================================================================
' The three footer buttons.
' ========================================================================================
private sub frmAbout_ButtonClickCallback( byval hButton as HWND, byval id as long )

    dim as HWND hwnd = GetParent(hButton)

    select case id
        case IDC_FRMABOUT_CMDWEBSITE
            ShellExecute( NULL, "open", FRMABOUT_URL_WEBSITE, NULL, NULL, SW_SHOWNORMAL )

        case IDC_FRMABOUT_CMDGITHUB
            ShellExecute( NULL, "open", FRMABOUT_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL )

        case IDC_FRMABOUT_CMDCLOSE
            SendMessage( hwnd, WM_CLOSE, 0, 0 )
    end select

end sub


' ========================================================================================
' Push the current theme into every child that carries its own colours.
' ========================================================================================
private sub frmAbout_ApplyTheme( byval hwnd as HWND )

    ' ---- the tab strip ----------------------------------------------------------------
    ' BackColorHot and BackColorSelect are left EQUAL to BackColor deliberately: this bar is
    ' meant to read flat, with the accent line under the word as the only mark of the current
    ' tab. That is PsSelectBar's own default shape, restated here because the theme's values
    ' would otherwise not be flat.
    dim as PSSELECTBAR_COLORS sc
    sc.BackColor         = theme.panel.text.backcolor
    sc.ForeColor         = theme.panel.text.forecolor
    sc.BackColorHot      = theme.panel.text.backcolor
    sc.ForeColorHot      = theme.panel.texthot.forecolor
    sc.BackColorSelect   = theme.panel.text.backcolor
    sc.ForeColorSelect   = theme.ui.field.forecolor
    sc.ForeColorDisabled = theme.topmenu.textdisabled.forecolor
    sc.IndicatorColor    = theme.ui.accent
    PsSelectBar_SetColors( HWND_FRMABOUT_TABS, @sc )

    ' ---- the GPL-3 / Proprietary strip -------------------------------------------------
    ' The SAME struct, deliberately: it is the same widget one level down, and giving the
    ' inner strip its own palette would make the page look like it had two unrelated tab
    ' bars rather than one subordinate to the other. Size is what separates them (a smaller
    ' font and a shorter band), not colour.
    PsSelectBar_SetColors( HWND_FRMABOUT_LICSEL, @sc )

    ' ---- the Credits list -------------------------------------------------------------
    PsListTree_SetBackColor( HWND_FRMABOUT_CREDITS, theme.panel.text.backcolor )
    PsListTree_SetScrollBarColors( HWND_FRMABOUT_CREDITS, theme.panel.scrollbar.backcolor, _
                                   theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    PsListTree_SetBorderColor( HWND_FRMABOUT_CREDITS, theme.ui.divider )

    ' ---- the License field ------------------------------------------------------------
    PsTextBox_SetBackColor( HWND_FRMABOUT_LICENSE, theme.ui.field.backcolor )
    PsTextBox_SetOuterBackColor( HWND_FRMABOUT_LICENSE, theme.panel.text.backcolor )
    PsTextBox_SetForeColor( HWND_FRMABOUT_LICENSE, theme.ui.field.forecolor )
    PsTextBox_SetBorderColor( HWND_FRMABOUT_LICENSE, theme.ui.divider )
    ' EQUAL to the plain border on purpose. A read-only field that brightens its frame when
    ' focused is advertising an edit that cannot happen.
    PsTextBox_SetFocusBorderColor( HWND_FRMABOUT_LICENSE, theme.ui.divider )

    ' ---- the three buttons ------------------------------------------------------------
    ' Same recipe as frmKeyboard_ApplyTheme and OptionsTheme_FillButton, spelled out again
    ' rather than shared: those live in files the unity build includes in an order that does
    ' not reach here. Keep the three in step by eye.
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
    ' The default button's accent border. PsButton applies this LAST and with no mood test, so
    ' it survives hover and press for free.
    bc.DefaultBorderColor  = theme.ui.accent

    dim as HWND hBtn(2) = { GetDlgItem(hwnd, IDC_FRMABOUT_CMDWEBSITE), _
                            GetDlgItem(hwnd, IDC_FRMABOUT_CMDGITHUB), _
                            GetDlgItem(hwnd, IDC_FRMABOUT_CMDCLOSE) }
    for i as long = 0 to 2
        if hBtn(i) = 0 then continue for
        PsButton_SetColors( hBtn(i), @bc )
        PsButton_Refresh( hBtn(i) )
    next

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' Position the children. The dialog is fixed-size, so this runs once after creation.
' ========================================================================================
private sub frmAbout_PositionWindows( byval hwnd as HWND )

    dim as RECT rcHero, rcTabs, rcBody, rcFoot
    frmAbout_GetBands( hwnd, rcHero, rcTabs, rcBody, rcFoot )
    if (rcBody.right <= 0) orelse (rcBody.bottom <= rcBody.top) then exit sub

    dim as long mx  = AfxScaleX(FRMABOUT_MARGIN)
    dim as long pad = AfxScaleY(FRMABOUT_BODY_PAD)

    ' ---- the tab strip ----------------------------------------------------------------
    ' Full width so its own background covers the band; the panels' left padding is what
    ' lines the first word up with the margin.
    SetWindowPos( HWND_FRMABOUT_TABS, 0, 0, rcTabs.top, _
                  rcTabs.right, rcTabs.bottom - rcTabs.top, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the body band's inner rect ----------------------------------------------------
    ' The Credits list takes it whole; the Licenses page divides it below. (This used to say
    ' "the two body controls, sharing one rect", which stopped being true when the licence
    ' strip arrived.)
    dim as long bx = mx
    dim as long by = rcBody.top + pad
    dim as long bw = rcBody.right - (mx * 2)
    dim as long bh = (rcBody.bottom - pad) - by
    if bh < 0 then bh = 0

    SetWindowPos( HWND_FRMABOUT_CREDITS, 0, bx, by, bw, bh, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the Licenses page: the field and its scrollbar --------------------------------
    ' Together they fill the body rect the Credits list occupies whole -- the licence
    ' selector does not live in the body at all. The bar takes the right edge and the field
    ' gives up exactly that width, so the two never overlap and no pixel of the band is
    ' unclaimed.
    '
    ' The width is COMPUTED, not read back: a PsVScrollBar is created 0x0 and has no width
    ' to query before its first positioning. Same design width the Output panel's bars use.
    dim as long vsW = AfxScaleX(SCROLLBAR_WIDTH_EDITOR)
    if vsW > bw then vsW = 0

    SetWindowPos( HWND_FRMABOUT_LICENSE, 0, bx, by, bw - vsW, bh, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( HWND_FRMABOUT_LICVSCROLL, 0, bx + bw - vsW, by, vsW, bh, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    ' RE-PUSH THE RANGE, because lines-per-page is a function of the height just assigned.
    ' The licence text is seeded during creation, before this runs, so the range the textbox
    ' reported then was measured against a field still 0x0 -- a thumb sized for a page of no
    ' lines. Nothing else would correct it: the dialog is fixed-size, so this is the only
    ' layout pass there is.
    frmAbout_LicenseScrollChanged( HWND_FRMABOUT_LICENSE )

    ' ---- the GPL-3 / Proprietary strip, ON THE TAB STRIP'S OWN LINE --------------------
    '
    ' Same band as the page tabs, pushed to the right margin, so the row reads "About |
    ' Credits | Licenses ............ GPL-3 | Proprietary" -- one line, two ends. It sat
    ' under the tabs before and read as a second row of them competing for the same job.
    '
    ' It OVERLAPS the tab strip, which spans the full width and paints its own background.
    ' That works because both carry the same palette (see frmAbout_ApplyTheme) and because
    ' this window is explicitly raised to the TOP of the sibling Z-order below -- creation
    ' order already puts it there, but relying on that would make a later re-ordering of
    ' frmAbout_Show silently paint the tab strip's background over these two captions.
    '
    ' Width is the two captions and nothing more, so the empty middle of the line belongs to
    ' the tab strip underneath and stays inert. Clamped to the body: a translation wide
    ' enough to exceed it would otherwise be positioned at a negative x and lose its FIRST
    ' caption -- the one that starts selected -- off the left edge.
    dim as long selW = PsSelectBar_GetIdealWidth( HWND_FRMABOUT_LICSEL )
    if selW > bw then selW = bw

    ' Right edge = the body's right margin, which is the field's right border to the pixel.
    SetWindowPos( HWND_FRMABOUT_LICSEL, HWND_TOP, (bx + bw) - selW, rcTabs.top, _
                  selW, rcTabs.bottom - rcTabs.top, SWP_NOACTIVATE )

    ' ---- the footer buttons, laid out right to left -----------------------------------
    dim as long btnW = AfxScaleX(FRMABOUT_BTN_W)
    dim as long btnH = AfxScaleY(FRMABOUT_BTN_H)
    dim as long bgap = AfxScaleX(FRMABOUT_BTN_GAP)
    dim as long btnY = rcFoot.top + (((rcFoot.bottom - rcFoot.top) - btnH) \ 2)
    dim as long x    = rcFoot.right - AfxScaleX(24) - btnW

    dim as HWND hOrder(2) = { GetDlgItem(hwnd, IDC_FRMABOUT_CMDCLOSE), _
                              GetDlgItem(hwnd, IDC_FRMABOUT_CMDGITHUB), _
                              GetDlgItem(hwnd, IDC_FRMABOUT_CMDWEBSITE) }
    for i as long = 0 to 2
        if hOrder(i) = 0 then continue for
        SetWindowPos( hOrder(i), 0, x, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
        x -= (btnW + bgap)
    next

end sub


' ========================================================================================
' LAYOUT SELF-TEST                                          TIKO_ABOUT_SELFTEST=1
'
' The dialog is fixed-size and nothing scrolls, so "does the content fit" is a real question
' with a checkable answer -- which is the whole reason the size was fixed. Runs only when the
' dialog is open, because every assertion needs live windows.
'
' Trace text goes through a `dim as string` first: PRINT on a DWSTRING, or on a
' `string & wstring` concat, interleaves nulls.
' ========================================================================================
dim shared as long gAboutAssertPass, gAboutAssertFail

#macro ABOUTASSERT( cond, msg )
    if (cond) then
        gAboutAssertPass += 1
    else
        gAboutAssertFail += 1
        scope
            dim as string s = "  FAIL: " & msg
            ? s
        end scope
    end if
#endmacro

private sub frmAbout_RunLayoutSelfTest( byval hwnd as HWND )

    if environ("TIKO_ABOUT_SELFTEST") <> "1" then exit sub

    gAboutAssertPass = 0
    gAboutAssertFail = 0
    ? ""
    ? "=== frmAbout layout self-test ==="

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim as RECT rcHero, rcTabs, rcBody, rcFoot
    frmAbout_GetBands( hwnd, rcHero, rcTabs, rcBody, rcFoot )

    ' ---- the bands tile the client exactly, with no gap and no overlap ----------------
    ABOUTASSERT( rcHero.top = 0, "the hero does not start at the top of the client" )
    ABOUTASSERT( rcHero.bottom = rcTabs.top, "a gap between the hero and the tab strip" )
    ABOUTASSERT( rcTabs.bottom = rcBody.top, "a gap between the tab strip and the body" )
    ABOUTASSERT( rcBody.bottom = rcFoot.top, "a gap between the body and the footer" )
    ABOUTASSERT( rcFoot.bottom = rc.bottom, "the footer does not reach the bottom of the client" )

    ' ---- the tab strip has exactly the three pages, in enum order --------------------
    ABOUTASSERT( PsSelectBar_GetCount( HWND_FRMABOUT_TABS ) = 3, _
                 "the tab strip does not have exactly three panels" )
    ABOUTASSERT( PsSelectBar_GetIdealWidth( HWND_FRMABOUT_TABS ) <= rcTabs.right, _
                 "the three tab captions are wider than the dialog" )

    ' ---- the credits roll -------------------------------------------------------------
    ABOUTASSERT( PsListTree_GetCount( HWND_FRMABOUT_CREDITS ) = 6, _
                 "the credits roll is not six rows" )
    ' Six rows at FRMABOUT_ROW_H plus the header must fit the body WITHOUT a scrollbar. This
    ' is the assertion that fails first if a contributor is added and nobody re-measures.
    scope
        dim as long needH = AfxScaleY(FRMABOUT_ROW_H) * (6 + 1)
        dim as RECT rcList
        GetWindowRect( HWND_FRMABOUT_CREDITS, @rcList )
        dim as long haveH = rcList.bottom - rcList.top
        ABOUTASSERT( needH <= haveH, _
                     "the credits roll needs " & str(needH) & "px and has " & str(haveH) )
    end scope

    ' ---- both body pages occupy the SAME rect -----------------------------------------
    ' The Licenses page is two windows -- field plus scrollbar -- so the claim is that the
    ' PAIR tiles the rect the Credits list takes whole: same top, same bottom, same outer
    ' edges, and the seam between them shared exactly. They are shown one at a time, so a
    ' divergence is invisible until someone switches tabs and the panel jumps.
    scope
        dim as RECT r1, r2, rV
        GetWindowRect( HWND_FRMABOUT_CREDITS, @r1 )
        GetWindowRect( HWND_FRMABOUT_LICENSE, @r2 )
        GetWindowRect( HWND_FRMABOUT_LICVSCROLL, @rV )

        ABOUTASSERT( (r1.top = r2.top) andalso (r1.bottom = r2.bottom) andalso _
                     (r1.top = rV.top) andalso (r1.bottom = rV.bottom), _
                     "the Credits and Licenses pages do not share the body's top and bottom" )
        ABOUTASSERT( r1.left = r2.left, "the licence field is not on the body's left margin" )
        ABOUTASSERT( rV.right = r1.right, "the scrollbar does not reach the body's right margin" )
        ' The seam. An off-by-one here is a hairline of dialog background showing between the
        ' field's border and the bar, or a bar overlapping the border it sits against --
        ' neither of which any "inside the band" test would notice.
        ABOUTASSERT( r2.right = rV.left, _
                     "a gap or overlap between the licence field and its scrollbar" & _
                     " [field.right=" & str(r2.right) & " bar.left=" & str(rV.left) & "]" )
        ABOUTASSERT( (rV.right - rV.left) > 0, "the licence scrollbar has no width" )

        ' THE ONLY ASSERTION HERE THAT IS NOT ABOUT A RECT, and the only one that touches the
        ' wiring: the GPL is ~670 lines and the field shows a few dozen, so the bar must be
        ' carrying a range that needs a thumb. It can only know that if the textbox's
        ' ScrollChangedCallback reached PsVScrollBar_SetRange with a real page size -- which
        ' is a chain of three calls that a correctly positioned, correctly coloured, entirely
        ' inert scrollbar would pass every other assertion in this suite without.
        ABOUTASSERT( PsVScrollBar_NeedsThumb( HWND_FRMABOUT_LICVSCROLL ), _
                     "the licence scrollbar has no thumb for a ~670 line document" )
    end scope

    ' ---- the licence strip shares the TAB STRIP'S line --------------------------------
    '
    ' Two select bars on one band, overlapping, so the assertions that matter are about the
    ' space BETWEEN them. Neither control knows the other exists.
    scope
        ABOUTASSERT( PsSelectBar_GetCount( HWND_FRMABOUT_LICSEL ) = 2, _
                     "the licence strip does not have exactly two panels" )

        dim as RECT rS, rL
        GetWindowRect( HWND_FRMABOUT_LICSEL, @rS )
        ' The BODY's rect, taken from the Credits list -- the licence field is narrower than
        ' the body by its scrollbar, so measuring the right margin against the field would
        ' be measuring it against the wrong edge.
        GetWindowRect( HWND_FRMABOUT_CREDITS, @rL )
        MapWindowPoints( HWND_DESKTOP, hwnd, cast(LPPOINT, @rS), 2 )
        MapWindowPoints( HWND_DESKTOP, hwnd, cast(LPPOINT, @rL), 2 )

        ' It fills the tab band exactly, which is what puts its captions on the same baseline
        ' as the page tabs rather than merely near them.
        ABOUTASSERT( (rS.top = rcTabs.top) andalso (rS.bottom = rcTabs.bottom), _
                     "the licence strip does not fill the tab band" & _
                     " [strip=" & str(rS.top) & ".." & str(rS.bottom) & _
                     " band=" & str(rcTabs.top) & ".." & str(rcTabs.bottom) & "]" )

        ' Right edge on the body's right margin -- which is the scrollbar's outer edge now,
        ' not the field's. That is what makes the right-aligned end of the line look
        ' deliberate rather than a few pixels adrift.
        ABOUTASSERT( rS.right = rL.right, _
                     "the licence strip does not end on the body's right margin" )

        ' THE COLLISION TEST, and the reason this pair needs assertions at all. The page tabs
        ' grow leftward from 0 and this strip grows rightward from the margin; a long
        ' translation of either can close the gap, and the overlap would silently paint one
        ' set of captions over the other. Compared against the TAB STRIP'S IDEAL WIDTH, not
        ' its window, because that window is full-width by design and would make the test
        ' fail always.
        dim as long tabsW = PsSelectBar_GetIdealWidth( HWND_FRMABOUT_TABS )
        ABOUTASSERT( rS.left >= tabsW, _
                     "the licence strip overlaps the page tabs" & _
                     " [strip.left=" & str(rS.left) & " tabs need=" & str(tabsW) & "]" )

        ' Against the BODY, not against the strip's own window: the strip is SIZED from
        ' GetIdealWidth, so comparing the two would be restating the assignment.
        ABOUTASSERT( PsSelectBar_GetIdealWidth( HWND_FRMABOUT_LICSEL ) <= (rL.right - rL.left), _
                     "the two licence captions are wider than the body" )
    end scope

    ' ---- BOTH LICENCE FILES ACTUALLY SHIP ----------------------------------------------
    ' The page reads them from the exe's directory at display time, and a missing file
    ' returns a readable fallback rather than a blank field -- which is right for the user
    ' and useless as evidence, because the dialog then looks fine while showing no licence
    ' at all. This is the assertion that can tell the difference.
    scope
        dim as DWSTRING wszG = PsExePath & FRMABOUT_FILE_GPL
        dim as DWSTRING wszP = PsExePath & FRMABOUT_FILE_PROPRIETARY
        ABOUTASSERT( PsFileExists( wszG ) <> 0, "LICENSE is not beside the executable" )
        ABOUTASSERT( PsFileExists( wszP ) <> 0, "LICENSE-PROPRIETARY.txt is not beside the executable" )

        dim as DWSTRING wszTextG = frmAbout_LicenseText( LICPAGE_GPL )
        dim as DWSTRING wszTextP = frmAbout_LicenseText( LICPAGE_PROPRIETARY )
        ABOUTASSERT( PsLen(wszTextG) > 1000, _
                     "the GPL text is only " & str(PsLen(wszTextG)) & " chars -- the fallback?" )
        ABOUTASSERT( PsLen(wszTextP) > 1000, _
                     "the proprietary text is only " & str(PsLen(wszTextP)) & " chars -- the fallback?" )
        ' The two panels must not be showing the same document. Both files existing does not
        ' establish that the reader is keyed on the panel: an ignored parameter would pass
        ' every assertion above.
        ABOUTASSERT( wszTextG <> wszTextP, "both licence panels show the same text" )
    end scope

    ' ---- the three hero pills fit the hero ---------------------------------------------
    ' The pill row is PAINTED, so an overflow is clipped in silence. It grew a third pill and
    ' the widest of the three is a translated word.
    scope
        dim as RECT rcTmp
        dim as HDC hdcTmp = GetDC( hwnd )
        dim as PsBufferPaint pbTmp
        SetRect( @rcTmp, 0, 0, rc.right, rcHero.bottom )
        pbTmp.BeginDoubleBuffer( hwnd, hdcTmp, rcTmp )
        dim as long xEnd = frmAbout_PillRowRight( @pbTmp, @rcHero )
        pbTmp.EndDoubleBuffer()
        ReleaseDC( hwnd, hdcTmp )
        ABOUTASSERT( xEnd <= (rc.right - AfxScaleX(FRMABOUT_MARGIN)), _
                     "the pill row ends at " & str(xEnd) & " past the margin at " & _
                     str(rc.right - AfxScaleX(FRMABOUT_MARGIN)) )
    end scope

    ' ---- the footer buttons are inside the footer and in the right order --------------
    scope
        dim as HWND hW = GetDlgItem(hwnd, IDC_FRMABOUT_CMDWEBSITE)
        dim as HWND hG = GetDlgItem(hwnd, IDC_FRMABOUT_CMDGITHUB)
        dim as HWND hC = GetDlgItem(hwnd, IDC_FRMABOUT_CMDCLOSE)
        ABOUTASSERT( (hW <> 0) andalso (hG <> 0) andalso (hC <> 0), "a footer button is missing" )

        dim as RECT rW, rG, rC
        GetWindowRect( hW, @rW ) : GetWindowRect( hG, @rG ) : GetWindowRect( hC, @rC )
        ABOUTASSERT( rW.right <= rG.left, "Website is not left of GitHub" )
        ABOUTASSERT( rG.right <= rC.left, "GitHub is not left of Close" )

        dim as RECT rClient = rC
        MapWindowPoints( HWND_DESKTOP, hwnd, cast(LPPOINT, @rClient), 2 )
        ABOUTASSERT( rClient.top >= rcFoot.top, "a footer button sits above the footer band" )
        ABOUTASSERT( rClient.bottom <= rcFoot.bottom, "a footer button overflows the footer band" )
    end scope

    ' ---- the icon plate ----------------------------------------------------------------
    ' Square, and fully inside the hero band. Square matters because the plate is stroked as
    ' a ROUNDED RECT with one curvature: on a non-square cell the two corner radii stop
    ' matching and it reads as a lozenge. The plate is 96 unscaled in both axes, but AfxScaleX
    ' and AfxScaleY are separate functions, so squareness is a property to check rather than
    ' one to assume.
    scope
        dim as RECT rcPlate
        frmAbout_GetPlateRect( @rcHero, rcPlate )
        ABOUTASSERT( (rcPlate.right - rcPlate.left) = (rcPlate.bottom - rcPlate.top), _
                     "the icon plate is not square" )
        ABOUTASSERT( (rcPlate.top >= rcHero.top) andalso (rcPlate.bottom <= rcHero.bottom), _
                     "the icon plate overflows the hero band" )
    end scope

    ' ---- every localized id this dialog owns actually resolves --------------------------
    ' L(id,"default") DISCARDS the default -- an id with no entry in english.lang renders as
    ' an EMPTY STRING, so a missing caption is a blank tab, not a build error.
    scope
        dim as long nBlank = 0
        for i as long = 359 to 374
            if len(LL(i)) = 0 then
                nBlank += 1
                dim as string s = "  (blank localization id " & str(i) & ")"
                ? s
            end if
        next
        ABOUTASSERT( nBlank = 0, "one or more of ids 370-385 is missing from english.lang" )
        ' 408 is this dialog's one id outside that run -- claimed from the blank slots rather
        ' than appended, so it does not sit beside its neighbours and would be missed by the
        ' loop above. It is used TWICE (the hero pill and the sub-strip), so a blank value
        ' costs two captions.
        ABOUTASSERT( len(LL(397)) > 0, "localization id 397 (Proprietary) is blank" )
    end scope

    scope
        dim as string s = "frmAbout: " & str(gAboutAssertPass) & " passed, " & str(gAboutAssertFail) & " failed"
        ? s
    end scope
    ? ""

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmAbout_WndProc( _
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
            return frmAbout_OnPaint( hwnd )

        case WM_SIZE
            frmAbout_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes through its click callback, not WM_COMMAND. What still arrives
            ' here is IDCANCEL, which IsDialogMessage synthesises for the Escape key.
            select case loword(wParam)
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case WM_CLOSE
            ' Enables parent window keeping parent's zorder
            EnableWindow(GetParent(hwnd), true)
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMABOUT         = 0
            HWND_FRMABOUT_TABS    = 0
            HWND_FRMABOUT_CREDITS = 0
            HWND_FRMABOUT_LICENSE = 0
            HWND_FRMABOUT_LICSEL  = 0
            HWND_FRMABOUT_LICVSCROLL = 0
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmAbout_Show
' ========================================================================================
function frmAbout_Show( byval hWndParent as HWND ) as LRESULT

    gAboutPage = ABOUTPAGE_ABOUT

    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMABOUT = pWindow->Create( hWndParent, L(71,"About") & " " & APPNAME, _
        @frmAbout_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMABOUT_CLIENT_W, FRMABOUT_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the tab strip, CREATED FIRST --------------------------------------------------
    '
    ' CREATION ORDER IS THE TAB ORDER. These controls live on an ordinary CWindow, not a
    ' dialog template, so there is no ordinal to set: GetNextDlgTabItem walks the child
    ' Z-ORDER, and Z-order is creation order.
    '
    ' PsSelectBar itself takes no focus (it is mouse-only), so what this really fixes is the
    ' order of everything after it.
    HWND_FRMABOUT_TABS = PsSelectBar_Create( HWND_FRMABOUT, IDC_FRMABOUT_TABS )
    PsSelectBar_SetFont( HWND_FRMABOUT_TABS, ghFont(GUIFONT_11) )
    ' The first panel's left padding is what puts "About" on the dialog's margin, since the
    ' bar spans the full client width.
    PsSelectBar_SetPadding( HWND_FRMABOUT_TABS, AfxScaleX(14), AfxScaleX(14) )
    PsSelectBar_AddPanel( HWND_FRMABOUT_TABS, L(71,"About") )
    PsSelectBar_AddPanel( HWND_FRMABOUT_TABS, L(361,"Credits") )
    ' PLURAL. tiko ships under two licences and this page shows both; a singular caption
    ' over a two-panel strip reads as a mistake before it reads as a licence statement.
    PsSelectBar_AddPanel( HWND_FRMABOUT_TABS, L(362,"Licenses") )
    PsSelectBar_SetPanelPadding( HWND_FRMABOUT_TABS, 0, _
                                 AfxScaleX(FRMABOUT_MARGIN), AfxScaleX(14) )
    PsSelectBar_SetIndicator( HWND_FRMABOUT_TABS, AfxScaleY(2), AfxScaleY(6) )
    PsSelectBar_SetSelChangeCallback( HWND_FRMABOUT_TABS, @frmAbout_TabChanged )

    ' ---- the Credits list --------------------------------------------------------------
    HWND_FRMABOUT_CREDITS = PsListTree_Create( HWND_FRMABOUT, IDC_FRMABOUT_CREDITS )
    PsListTree_AddColumn( HWND_FRMABOUT_CREDITS, L(368,"Contributor"), pWindow->ScaleX(FRMABOUT_COL_WHO) )
    PsListTree_AddColumn( HWND_FRMABOUT_CREDITS, L(369,"For"), pWindow->ScaleX(200) )
    PsListTree_SetFillColumn( HWND_FRMABOUT_CREDITS, 1 )
    PsListTree_ShowHeader( HWND_FRMABOUT_CREDITS, true )
    PsListTree_SetPaintCallback( HWND_FRMABOUT_CREDITS, @frmAbout_CreditsPaintRow )
    PsListTree_SetHeaderPaintCallback( HWND_FRMABOUT_CREDITS, @frmAbout_CreditsPaintHeader )
    PsListTree_SetTooltipCallback( HWND_FRMABOUT_CREDITS, @frmAbout_CreditsTooltip )
    PsListTree_SetHeaderTooltipCallback( HWND_FRMABOUT_CREDITS, @frmAbout_CreditsHeaderTooltip )
    PsListTree_SetFont( HWND_FRMABOUT_CREDITS, ghFont(GUIFONT_11) )
    ' UNSCALED: PsListTree scales what it is handed, so a pre-scaled value is scaled twice.
    PsListTree_SetRowHeight( HWND_FRMABOUT_CREDITS, FRMABOUT_ROW_H )
    ' RAW PIXELS and deliberately not DPI-scaled -- a chrome hairline should stay a hairline.
    ' PsListTree is the odd one out in its own API here: RowHeight above IS scaled internally.
    PsListTree_SetBorderWidth( HWND_FRMABOUT_CREDITS, 1 )

    ' ---- the GPL-3 / Proprietary strip, ABOVE the field --------------------------------
    ' Created before the field for the same reason the tab strip is created first: creation
    ' order is Z-order is tab order.
    '
    ' A PsSelectBar rather than two painted labels, because "a flat row of text labels,
    ' exactly one current, hot-tracked, reporting user clicks" is exactly what that control
    ' is -- and the alternative is hit-testing and hover-tracking two rects by hand in the
    ' dialog's own WM_MOUSEMOVE.
    HWND_FRMABOUT_LICSEL = PsSelectBar_Create( HWND_FRMABOUT, IDC_FRMABOUT_LICSEL )
    PsSelectBar_SetFont( HWND_FRMABOUT_LICSEL, ghFont(GUIFONT_11) )
    ' THE SAME PADDING AND INDICATOR AS THE TAB STRIP, because they are now one line: the two
    ' halves must space their words alike and underline them at the same height, or the row
    ' reads as two controls that happened to collide.
    PsSelectBar_SetPadding( HWND_FRMABOUT_LICSEL, AfxScaleX(14), AfxScaleX(14) )
    ' "GPL-3" is an identifier, not prose -- untranslated on purpose, like the "GPL v3" pill.
    PsSelectBar_AddPanel( HWND_FRMABOUT_LICSEL, "GPL-3" )
    PsSelectBar_AddPanel( HWND_FRMABOUT_LICSEL, L(397,"Proprietary") )
    PsSelectBar_SetPanelPadding( HWND_FRMABOUT_LICSEL, 0, 0, AfxScaleX(14) )
    ' NO RIGHT PADDING ON THE LAST PANEL. The strip is sized to its ideal width and pushed
    ' against the body's right margin, so trailing padding would show up as the last caption
    ' floating short of the edge it is meant to line up with.
    PsSelectBar_SetPanelPadding( HWND_FRMABOUT_LICSEL, 1, AfxScaleX(14), 0 )
    PsSelectBar_SetIndicator( HWND_FRMABOUT_LICSEL, AfxScaleY(2), AfxScaleY(6) )
    PsSelectBar_SetSelChangeCallback( HWND_FRMABOUT_LICSEL, @frmAbout_LicenseTabChanged )

    ' ---- the License field -------------------------------------------------------------
    ' Multiline (creation-time, not a setter) and read-only. This is the one control that
    ' makes PsTextBox_FilterMessage mandatory in the pump below.
    HWND_FRMABOUT_LICENSE = PsTextBox_Create( HWND_FRMABOUT, IDC_FRMABOUT_LICENSE, true )
    PsTextBox_SetFont( HWND_FRMABOUT_LICENSE, ghFont(GUIFONT_11) )
    PsTextBox_SetBorderWidth( HWND_FRMABOUT_LICENSE, 1 )
    PsTextBox_SetCornerRadius( HWND_FRMABOUT_LICENSE, 0 )
    PsTextBox_SetMargins( HWND_FRMABOUT_LICENSE, AfxScaleX(10), AfxScaleX(10) )
    PsTextBox_SetReadOnly( HWND_FRMABOUT_LICENSE, true )

    ' ---- the field's vertical scrollbar ------------------------------------------------
    ' Created BEFORE the first SetText below: the textbox's scroll callback fires from
    ' SetText, and with no bar yet there would be nothing to push the initial range into --
    ' the licence would come up with a full document and an empty-looking bar until the
    ' first wheel notch.
    HWND_FRMABOUT_LICVSCROLL = PsVScrollBar_Create( HWND_FRMABOUT, IDC_FRMABOUT_LICVSCROLL )
    PsVScrollBar_SetScrollCallback( HWND_FRMABOUT_LICVSCROLL, @frmAbout_LicenseVScrollCallback )
    PsVScrollBar_SetPaintCallback( HWND_FRMABOUT_LICVSCROLL, @frmAbout_LicenseVScrollPaint )
    PsTextBox_SetScrollChangedCallback( HWND_FRMABOUT_LICENSE, @frmAbout_LicenseScrollChanged )

    ' AFTER SetReadOnly, and that order is not cosmetic: the field is seeded through the same
    ' frmAbout_ShowLicense the strip's callback uses, so there is ONE path that puts a licence
    ' on screen and the seeded state cannot disagree with a clicked one.
    frmAbout_ShowLicense( LICPAGE_GPL )

    ' ---- the three footer buttons ------------------------------------------------------
    dim as HWND hWebsite = PsButton_Create( HWND_FRMABOUT, IDC_FRMABOUT_CMDWEBSITE )
    PsButton_SetText( hWebsite, L(363,"Website") )
    PsButton_SetFont( hWebsite, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( hWebsite, @frmAbout_ButtonClickCallback )

    dim as HWND hGitHub = PsButton_Create( HWND_FRMABOUT, IDC_FRMABOUT_CMDGITHUB )
    PsButton_SetText( hGitHub, L(364,"GitHub") )
    PsButton_SetFont( hGitHub, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( hGitHub, @frmAbout_ButtonClickCallback )

    dim as HWND hClose = PsButton_Create( HWND_FRMABOUT, IDC_FRMABOUT_CMDCLOSE )
    PsButton_SetText( hClose, L(135,"Close") )
    PsButton_SetFont( hClose, ghFont(GUIFONT_11) )
    PsButton_SetDefault( hClose, true )
    PsButton_SetClickCallback( hClose, @frmAbout_ButtonClickCallback )

    ShowWindow( HWND_FRMABOUT_TABS, SW_SHOW )
    ShowWindow( hWebsite, SW_SHOW )
    ShowWindow( hGitHub, SW_SHOW )
    ShowWindow( hClose, SW_SHOW )

    frmAbout_BuildCredits()
    frmAbout_ApplyTheme( HWND_FRMABOUT )
    frmAbout_PositionWindows( HWND_FRMABOUT )
    ' Seeds the page WITHOUT going through the select bar, whose SelChange fires for user
    ' action only. This is also what hides both body controls, which are created visible.
    frmAbout_ShowPage( HWND_FRMABOUT, ABOUTPAGE_ABOUT )

    ShowWindow( HWND_FRMABOUT, SW_SHOW )
    UpdateWindow( HWND_FRMABOUT )

    ' IsDialogMessage ignores Tab until a child already holds the focus, and an ordinary
    ' CWindow has no dialog manager to arrange that. (See the WS_EX_CONTROLPARENT section of
    ' C:\dev\Learnings.md.) Focus starts on Close: it is the default button and the only
    ' thing most people open this box to press.
    SetFocus( hClose )

    frmAbout_RunLayoutSelfTest( HWND_FRMABOUT )

    ' Close the dialog immediately, so a headless run reaches the end of the process and the
    ' console output actually flushes. Without this the modal loop below blocks forever
    ' waiting for a human, and a run that is killed rather than exited loses every buffered
    ' line -- including the self-test's own report.
    if environ("TIKO_ABOUT_AUTOCLOSE") = "1" then
        PostMessage( HWND_FRMABOUT, WM_CLOSE, 0, 0 )
    end if

    ' ---- message loop -------------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents: that has no *_FilterMessage hooks, and the License
    ' field's right-click menu is a PsPopupMenu that needs one.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. Without it that menu has no keyboard navigation and never closes on an
        ' outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' ESCAPE closes -- handled explicitly, and AFTER the popup filter above so an open
        ' context menu closes on Escape first rather than the whole dialog going with it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMABOUT, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMABOUT, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    function = 0

end function
