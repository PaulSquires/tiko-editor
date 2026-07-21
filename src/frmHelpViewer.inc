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

#include once "frmHelpViewer.bi"


' The topics list is a CListBox, which owns and drives its OWN CVScrollBar internally
' (created, ranged, positioned and auto-hidden by the control). The hand-written
' frmHelpViewer_UpdateScrollBar / _VScrollCallback pair that used to push item counts into
' a separate bar is gone with it; only the help TEXT still needs that wiring, below.


' ========================================================================================
' Push the help-text CTextBox scroll state into its CVScrollBar, in LINE units --
' the CVScrollBar-shaped scroll API the control was built with (GetVScrollInfo ->
' SetRange). Called from the textbox's ScrollChangedCallback (typing can't happen --
' read-only -- but wheel scrolling and programmatic SetText both fire it) and from
' PositionWindows (a resize changes the lines-per-page).
' ========================================================================================
function frmHelpViewer_UpdateTextScrollBar() as boolean
    if IsWindow( HWND_FRMHELPVIEWER_VSCROLLBAR2 ) = 0 then exit function
    dim as integer nTotalLines, nLinesPerPage, nFirstVisible
    CTextBox_GetVScrollInfo( HWND_FRMHELPVIEWER_RIGHTPANEL, nTotalLines, nLinesPerPage, nFirstVisible )
    CVScrollBar_SetRange( HWND_FRMHELPVIEWER_VSCROLLBAR2, nTotalLines, nLinesPerPage, nFirstVisible )
    function = 0
end function


' Vertical scroll state of the help text may have changed: push the fresh range.
sub frmHelpViewer_TextScrollChanged( byval hTextBox as HWND )
    frmHelpViewer_UpdateTextScrollBar()
end sub


' The user scrolled the text CVScrollBar (thumb drag or track paging): scroll the
' help text to that first line. The ScrollToLine -> EN_UPDATE round trip echoes the
' same position back through TextScrollChanged -- consistent by construction.
sub frmHelpViewer_TextVScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    CTextBox_ScrollToLine( HWND_FRMHELPVIEWER_RIGHTPANEL, newPos )
end sub


' ========================================================================================
' Custom paint keeping the old visuals: flat track + flat thumb in the ghPanel colors,
' no hover brightening. Colors are read live, so a theme switch only needs a repaint.
' Shared by both bars (topics list and help text).
' ========================================================================================
sub frmHelpViewer_VScrollPaint( byval p as CVSCROLLBAR_PAINTINFO ptr )
    dim as clsDoubleBuffer ptr b = p->b
    b->SetBackColor( ghPanel.BackColorScrollBar )
    b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetBackColor( ghPanel.ForeColorScrollBar )
        b->PaintRect( @p->rcThumb )
    end if
end sub


' ========================================================================================
' Paint one topics row.
' ========================================================================================
sub frmHelpViewer_ListPaint( byval p as CLISTBOX_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMHELPVIEWER)
    if pWindow = 0 then exit sub

    dim as COLORREF backclr = ghPanel.BackColor
    dim as COLORREF foreclr = ghPanel.ForeColor
    if p->isSelected orelse p->isHot then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as DWSTRING wszCaption = p->wszCaption
    dim as long nLeftMargin = pWindow->ScaleX(20)

    dim as RECT rcText = p->rc
    rcText.left = nLeftMargin

    ' A leading "-" marks a sub-topic: it gets a document glyph and the text shifts right.
    if left(ltrim(wszCaption), 1) = "-" then
        dim as RECT rcBitmap = rcText
        rcBitmap.left = rcText.left + pWindow->ScaleX(10)
        rcBitmap.right = rcBitmap.left + pWindow->ScaleX(10)
        p->b->SetFont( ghFont(GUIFONT_10) )   ' GUI rather than SYMBOL b/c of the dot icon
        p->b->PaintText( wszIconDocument, @rcBitmap, DT_CENTER )
        rcText.left = rcBitmap.right
    end if

    wszCaption = ltrim(wszCaption, any " -")
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( wszCaption, @rcText, DT_LEFT )
end sub


' ========================================================================================
' The user selected a different topic -- by clicking it OR with the arrow keys. Load it.
' Silent for CListBox_SetCurSel, which is why seeding the initial selection at startup does
' not load the file twice.
' ========================================================================================
sub frmHelpViewer_ListSelChange( byval hCtl as HWND, byval row as integer )
    if row < 0 then exit sub
    frmHelpViewer_LoadHelpFile( row )
end sub


' With no tooltip callback CListBox falls back to showing the row's own text, and the rows
' now carry their captions -- so the topics list would sprout tooltips it never had. Return
' "" to keep the previous behavior. Delete this (and the SetTooltipCallback call) to get
' tooltips for topic names that are too long for the panel.
function frmHelpViewer_ListTooltip( byval hCtl as HWND, byval row as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' Paint the splitter bar. The control's client rect is the GRAB area (HELPVIEWER_SPLITGRAB
' wide); the visible band is the narrower SPLITSIZE, centered in it, so the bar looks the
' same as it always did while being easier to grab. Colors are read live, so a theme switch
' only needs a repaint (same approach as frmHelpViewer_VScrollPaint above).
' ========================================================================================
sub frmHelpViewer_SplitterPaint( byval p as CSPLITTER_PAINTINFO ptr )
    dim as clsDoubleBuffer ptr b = p->b

    ' The gap either side of the band is the form's own background.
    b->SetBackColor( ghMain.BackColor )
    b->PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMHELPVIEWER )
    dim as long nBandWidth = SPLITSIZE
    if pWindow then nBandWidth = pWindow->ScaleX(SPLITSIZE)

    dim as RECT rc = p->rcClient
    dim as long nInset = ((rc.right - rc.left) - nBandWidth) \ 2
    if nInset < 0 then nInset = 0
    rc.left += nInset
    rc.right -= nInset

    dim as COLORREF clr = ghPanel.BackColor
    if p->isHot orelse p->isDragging then clr = ghPanel.BackColorHot
    b->SetBackColor( clr )
    b->PaintRect( @rc )
end sub


' ========================================================================================
' The user dragged the splitter. The control has already moved itself; all we do is lay out
' the panels around it. Every phase does the same thing here -- unlike the editor split,
' this window has no expensive chrome worth bracketing with BEGIN/END.
' ========================================================================================
sub frmHelpViewer_SplitterMoved( _
            byval hSplitter as HWND, _
            byval newPos    as integer, _
            byval nPhase    as integer _
            )
    frmHelpViewer_PositionWindows()
end sub


' ========================================================================================
' Load a specific Help file richtext file into the CTextBox.
' The help files on disk are RTF; the CTextBox is plain text by design (one themed
' fore/back color for all text). A hidden RichEdit does the RTF -> plain text
' conversion: stream the file in, read the plain text back out, hand it to the
' textbox. SetText is programmatic (silent for ChangeCallback) but still fires the
' ScrollChangedCallback, so the scrollbar re-ranges automatically.
' ========================================================================================
function frmHelpViewer_LoadHelpFile( byval nIndex as long ) as long
        if (nIndex >= lbound(gHelpViewer.Filenames)) andalso _
        (nIndex <= ubound(gHelpViewer.Filenames)) then
        dim as DWSTRING wszPath = AfxStrPathname("PATH", gConfig.HelpTableOfContents)
        dim as HWND hConvert = GetDlgItem( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_RTFCONVERT )
        RichEdit_LoadRtfFromFile( hConvert, wszPath & gHelpViewer.Filenames(nIndex) )
        CTextBox_SetText( HWND_FRMHELPVIEWER_RIGHTPANEL, AfxGetWindowText(hConvert) )
        SetWindowText( hConvert, wstr("") )   ' don't hold the document twice
        ' Ensure the view starts at the top of the file
        CTextBox_SetSel( HWND_FRMHELPVIEWER_RIGHTPANEL, 0, 0 )
        CTextBox_ScrollToLine( HWND_FRMHELPVIEWER_RIGHTPANEL, 0 )
    end if
    function = 0
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmHelpViewer_PositionWindows() as LRESULT
    
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMHELPVIEWER)
    if pWindow = 0 then exit function

    dim as RECT rcClient: GetClientRect(HWND_FRMHELPVIEWER, @rcClient)
    dim as long nClientHeight = rcClient.bottom - rcClient.top
    dim as long nClientWidth = rcClient.right - rcClient.left

    dim as long nGrabWidth   = pWindow->ScaleX(HELPVIEWER_SPLITGRAB)
    dim as long nVScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_PANEL)
    dim as long nMinPane     = pWindow->ScaleX(HELPVIEWER_MINPANE)

    ' The splitter's position is its own LEFT EDGE in this window's client coordinates, and
    ' it is the single source of truth for the layout -- the left panel's width is derived
    ' from it, not the other way round. The limits are ours to compute; the control never
    ' guesses them.
    '
    ' The topics CListBox carries its own scrollbar INSIDE its client rect (auto-hidden when
    ' the rows fit), so the left panel now runs edge-to-edge up to the bar -- there is no
    ' separate scrollbar strip in this form's layout. Only the help TEXT still has
    ' an external CVScrollBar, on the far right.
    CSplitter_SetRange( HWND_FRMHELPVIEWER_SPLITTER, _
                        nMinPane, _
                        nClientWidth - nVScrollWidth - nGrabWidth - nMinPane )
    dim as long nSplitPos = CSplitter_GetPos( HWND_FRMHELPVIEWER_SPLITTER )
    dim as long nLeftPanelWidth = nSplitPos

    SetWindowPos( HWND_FRMHELPVIEWER_LEFTPANEL, 0, _
        0, 0, nLeftPanelWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    SetWindowPos( HWND_FRMHELPVIEWER_SPLITTER, 0, _
        nSplitPos, 0, nGrabWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    ' Help text CTextBox, with its own CVScrollBar hugging the window's right edge.
    ' The width is floored at zero: on a window too narrow to hold both minimums the
    ' splitter pins at nMin (its range inverts) and this arithmetic would otherwise hand
    ' SetWindowPos a negative width.
    dim as long nRightPanelWidth = nClientWidth - nVScrollWidth - (nSplitPos + nGrabWidth)
    if nRightPanelWidth < 0 then nRightPanelWidth = 0
    SetWindowPos( HWND_FRMHELPVIEWER_RIGHTPANEL, 0, _
        nSplitPos + nGrabWidth, 0, _
        nRightPanelWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    SetWindowPos( HWND_FRMHELPVIEWER_VSCROLLBAR2, 0, _
        nClientWidth - nVScrollWidth, 0, nVScrollWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    ' Push the text range AFTER the resize so lines-per-page reflects the new height (the
    ' old code computed it before repositioning, going stale by one layout pass). The
    ' topics list re-ranges its own scrollbar from its own WM_SIZE.
    frmHelpViewer_UpdateTextScrollBar()
    
    ' Redraw the form background and allow a breath for some paint messages in order
    ' to reduce screen tearing on panel resizing.
    AfxRedrawWindow( HWND_FRMHELPVIEWER )
    AfxDoevents()   
    
    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmHelpViewer_PositionWindows()
    end if
    function = 0
end Function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
        case IDCANCEL
            if codeNotify = BN_CLICKED then
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
    
    end select

    function = 0
end function


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as clsDoubleBuffer b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( ghMain.BackColor )
    b.PaintClientRect()
    b.EndDoubleBuffer()
    
    function = 0
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnDestroy( byval hwnd as HWND ) as LRESULT
    ' Save the current screen coordinates of the Help Viewer
    gConfig.SaveConfigFile    

    function = 0
end function


' ========================================================================================
' frmOptions Window procedure
' ========================================================================================
function frmHelpViewer_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,           frmHelpViewer_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,          frmHelpViewer_OnPaint)
        HANDLE_MSG (hwnd, WM_CLOSE,          frmHelpViewer_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,        frmHelpViewer_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,        frmHelpViewer_OnCommand)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMHELPVIEWER = 0
        ' The CListBox/CVScrollBar/CTextBox/CSplitter children destroyed themselves in
        ' their own WM_NCDESTROY; reset the handles here (the Help window can be closed
        ' and reopened).
        HWND_FRMHELPVIEWER_LEFTPANEL = 0
        HWND_FRMHELPVIEWER_VSCROLLBAR2 = 0
        HWND_FRMHELPVIEWER_RIGHTPANEL = 0
        HWND_FRMHELPVIEWER_SPLITTER = 0
        return 0

        case WM_ERASEBKGND
            ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
            ' background erase here would only add flicker.
            return true

    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function
' ========================================================================================
' frmHelpViewer_Show
' ========================================================================================
function frmHelpViewer_Show( byval hWndParent as HWND ) as LRESULT

    ' Display the HelpViewer if it already exists
    if IsWindow( HWND_FRMHELPVIEWER ) then
        if IsIconic(HWND_FRMHELPVIEWER) then 
            ShowWindow( HWND_FRMHELPVIEWER, SW_NORMAL )
        end if
        AllowSetForegroundWindow(ASFW_ANY)
        SetForegroundWindow( HWND_FRMHELPVIEWER )
        exit function
    end if
    
    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    dim as DWSTRING wszText = APPNAME & " " & L(73,"Help")
    HWND_FRMHELPVIEWER = pWindow->Create( _
        hWndParent, wszText, @frmHelpViewer_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_OVERLAPPEDWINDOW or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->ClassStyle = CS_DBLCLKS

    ' Set the small and large icon for the main window (must be set after main window is created)
    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    dim as long nvalue = false
    if instr(gConfig.ThemeShortFilename, "_dark") then nvalue = true
    ' Attempt to apply the standard Windows dark theme to the non-client areas of the main form.
    DwmSetWindowAttribute(HWND_FRMHELPVIEWER, DWMWA_USE_IMMERSIVE_DARK_MODE, @nvalue, sizeof(nvalue))

    if (gConfig.HelpStartupRight = 0) orelse (gConfig.HelpStartupBottom = 0) then
        gConfig.HelpStartupRight  = pWindow->ScaleX(800)
        gConfig.HelpStartupBottom = pWindow->ScaleX(600)
        gConfig.HelpLeftPanelWidth = pWindow->ScaleX(250)
    end if
    
    dim WinPla as WINDOWPLACEMENT
    with WinPla
        .Length = sizeof(WinPla)
        .rcNormalPosition.Left   = gConfig.HelpStartupLeft
        .rcNormalPosition.Top    = gConfig.HelpStartupTop
        .rcNormalPosition.Right  = gConfig.HelpStartupRight
        .rcNormalPosition.Bottom = gConfig.HelpStartupBottom
        .showCmd = iif( gConfig.HelpStartupMaximized, SW_MAXIMIZE, SW_SHOWNORMAL )
    end with
    SetWindowPlacement( pWindow->hWindow, @WinPla )

    ' Topics list: the reusable CListBox control. It wraps the Win32 listbox, owns and
    ' auto-hides its own CVScrollBar INSIDE its client rect, tracks hover, and delegates row
    ' drawing to frmHelpViewer_ListPaint -- which is why the form no longer carries a
    ' WM_DRAWITEM/WM_MEASUREITEM pair, a listbox subclass, or a second scrollbar.
    ' PositionWindows sizes it, so no initial size is needed here.
    HWND_FRMHELPVIEWER_LEFTPANEL = _
        CListBox_Create( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_LEFTPANEL )
    CListBox_SetPaintCallback( HWND_FRMHELPVIEWER_LEFTPANEL, @frmHelpViewer_ListPaint )
    ' SelChange replaces the old LBN_SELCHANGE: it reports mouse AND keyboard moves.
    CListBox_SetSelChangeCallback( HWND_FRMHELPVIEWER_LEFTPANEL, @frmHelpViewer_ListSelChange )
    CListBox_SetTooltipCallback( HWND_FRMHELPVIEWER_LEFTPANEL, @frmHelpViewer_ListTooltip )
    CListBox_SetBackColor( HWND_FRMHELPVIEWER_LEFTPANEL, ghPanel.BackColor )
    CListBox_SetFont( HWND_FRMHELPVIEWER_LEFTPANEL, ghFont(GUIFONT_10) )
    CListBox_SetRowHeight( HWND_FRMHELPVIEWER_LEFTPANEL, EXPLORERITEM_HEIGHT )
    ' The bar is created/positioned/auto-hidden by the control; we only theme it, reusing
    ' the same painter as the help-text bar so both keep their existing look.
    CListBox_SetScrollBarWidth( HWND_FRMHELPVIEWER_LEFTPANEL, SCROLLBAR_WIDTH_PANEL )
    CListBox_SetScrollBarPaintCallback( HWND_FRMHELPVIEWER_LEFTPANEL, @frmHelpViewer_VScrollPaint )

    ' Splitter between the topics list and the help text: the reusable CSplitter control.
    ' Its position is its own left edge in this window's client coords -- which is now
    ' exactly the left panel's width, since the topics scrollbar lives inside the CListBox.
    ' PositionWindows sizes it and pushes the limits, which clamp this seed.
    HWND_FRMHELPVIEWER_SPLITTER = CSplitter_Create( HWND_FRMHELPVIEWER, _
        IDC_FRMHELPVIEWER_SPLITTER, CSPLITTER_VERTICAL )
    CSplitter_SetPaintCallback( HWND_FRMHELPVIEWER_SPLITTER, @frmHelpViewer_SplitterPaint )
    CSplitter_SetPosChangedCallback( HWND_FRMHELPVIEWER_SPLITTER, @frmHelpViewer_SplitterMoved )
    CSplitter_SetPos( HWND_FRMHELPVIEWER_SPLITTER, gConfig.HelpLeftPanelWidth )

    ' Help text panel: a multiline CTextBox (word-wrapped, control-implemented wheel
    ' scrolling, no native scrollbar -- an external CVScrollBar is wired through the
    ' control's scroll API below). Themed with the editor text colors -- the thing the
    ' old RichEdit never managed (its dead SetBkgndColor/EM_SETCHARFORMAT attempts sat
    ' commented out here). The fixed-pitch font keeps the help files' column-aligned
    ' shortcut tables lined up.
    HWND_FRMHELPVIEWER_RIGHTPANEL = _
        CTextBox_Create( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_RIGHTPANEL, true )
    CTextBox_SetFont( HWND_FRMHELPVIEWER_RIGHTPANEL, ghFont(INFOFONT_11) )
    CTextBox_SetBorderWidth( HWND_FRMHELPVIEWER_RIGHTPANEL, 0 )
    CTextBox_SetMargins( HWND_FRMHELPVIEWER_RIGHTPANEL, pWindow->ScaleX(40), pWindow->ScaleX(40) )
    CTextBox_SetReadOnly( HWND_FRMHELPVIEWER_RIGHTPANEL, true )
    CTextBox_SetForeColor( HWND_FRMHELPVIEWER_RIGHTPANEL, ghEditor.ForeColorText )
    CTextBox_SetBackColor( HWND_FRMHELPVIEWER_RIGHTPANEL, ghEditor.BackColorText )
    CTextBox_SetOuterBackColor( HWND_FRMHELPVIEWER_RIGHTPANEL, ghEditor.BackColorText )
    CTextBox_SetScrollChangedCallback( HWND_FRMHELPVIEWER_RIGHTPANEL, @frmHelpViewer_TextScrollChanged )
    CTextBox_SetMenuText( HWND_FRMHELPVIEWER_RIGHTPANEL, L(17,"Cut"), L(18,"Copy"), L(19,"Paste"), L(40,"Select All") )

    ' Help-text scrollbar, driven in LINE units through the CTextBox scroll API
    ' (ScrollChangedCallback -> GetVScrollInfo -> SetRange; ScrollCallback -> ScrollToLine).
    HWND_FRMHELPVIEWER_VSCROLLBAR2 = CVScrollBar_Create( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_VSCROLLBAR2 )
    CVScrollBar_SetScrollCallback( HWND_FRMHELPVIEWER_VSCROLLBAR2, @frmHelpViewer_TextVScrollCallback )
    CVScrollBar_SetPaintCallback( HWND_FRMHELPVIEWER_VSCROLLBAR2, @frmHelpViewer_VScrollPaint )

    ' Hidden RichEdit used only to convert the RTF help files to plain text for the
    ' CTextBox (see frmHelpViewer_LoadHelpFile). Never shown, never positioned.
    pWindow->AddControl("RICHEDIT", , IDC_FRMHELPVIEWER_RTFCONVERT, "", 0, 0, 0, 0, _
        WS_CHILD or ES_LEFT or ES_MULTILINE, _
        WS_EX_LEFT)

    ' Load the Help table of contents
    dim pStream as CTextStream
    if pStream.Open(gConfig.HelpTableOfContents) = S_OK then
        dim as DWSTRING wst
        dim as long idx
        do until pStream.EOS
            wst = pStream.ReadLine
            wst = trim(wst)
            if len(wst) = 0 then continue do
            
            if left(wst, 7) = "TOPIC: " then
                ' The row now carries its own caption (the paint callback reads
                ' p->wszCaption). The old code added an empty string and kept the text in a
                ' parallel gHelpViewer.Topics array purely so WM_DRAWITEM could find it;
                ' that array died with the handler.
                idx = CListBox_AddString( HWND_FRMHELPVIEWER_LEFTPANEL, mid(wst, 8) )
                redim preserve gHelpViewer.Filenames(idx)
                continue do
            end if

            if left(wst, 10) = "FILENAME: " then
                if (idx >= lbound(gHelpViewer.Filenames)) andalso _
                    (idx <= ubound(gHelpViewer.Filenames)) then
                    gHelpViewer.Filenames(idx) = mid(wst, 11)
                end if
                continue do
            end if
        loop
        pStream.Close
    end if
    
    ' SetCurSel is silent (programmatic), so the first topic is loaded explicitly here --
    ' exactly as before, and without the SelChange callback double-loading it.
    CListBox_SetCurSel( HWND_FRMHELPVIEWER_LEFTPANEL, 0 )
    frmHelpViewer_LoadHelpFile(0)

    frmHelpViewer_PositionWindows()

    ShowWindow( HWND_FRMHELPVIEWER, SW_SHOW )
    ' CListBox_Create gives the inner Win32 listbox the SAME control id as the container,
    ' so this reaches the focusable child rather than the wrapper.
    SetFocus( GetDlgItem( HWND_FRMHELPVIEWER_LEFTPANEL, IDC_FRMHELPVIEWER_LEFTPANEL ) )

    
    return 0
end function

