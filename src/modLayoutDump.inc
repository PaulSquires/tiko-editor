'' ========================================================================================
'' modLayoutDump -- implementation. See modLayoutDump.bi for what this is and why.
'' ========================================================================================

'' The client size every dump is taken at. Arbitrary, but it has to be SOMETHING fixed, and
'' these two are large enough that no band hits its clamp: the panel floors at PANEL_MIN_WIDTH
'' and the editor at OUTPUT_MIN_EDITOR, and a dump taken against a clamped layout would record
'' the clamp rather than the arithmetic. 1400x900 clears both at every scale up to 2.0.
const LAYOUTDUMP_W = 1400
const LAYOUTDUMP_H = 900

'' ----------------------------------------------------------------------------------------
'' One child's rectangle, in frmMain CLIENT coordinates.
''
'' A HIDDEN window prints "hidden" and NOT a rectangle, deliberately. Windows keeps the last
'' position of a hidden window, so SetWindowPos leaves stale-but-plausible numbers behind and
'' a dump that printed them would record a rectangle for a band that is not on screen. The
'' visibility IS part of the layout contract -- half of frmMain_PositionWindows is deciding
'' it -- so it is what gets compared.
'' ----------------------------------------------------------------------------------------
private sub LayoutDump_Child( byval szName as zstring ptr, byval h as HWND )

    if h = 0 then
        print "  " & *szName & " absent"
        exit sub
    end if

    if IsWindowVisible(h) = 0 then
        print "  " & *szName & " hidden"
        exit sub
    end if

    dim as RECT rc
    GetWindowRect( h, @rc )
    '' Screen -> frmMain client. MapWindowPoints takes the RECT as two POINTs, which is what
    '' the cast is for; ScreenToClient twice would do the same thing in twice the lines.
    MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(LPPOINT, @rc), 2 )

    print "  " & *szName & " " & _
          str(rc.left) & "," & str(rc.top) & "," & str(rc.right) & "," & str(rc.bottom) & _
          " (" & str(rc.right - rc.left) & "x" & str(rc.bottom - rc.top) & ")"

end sub


'' ----------------------------------------------------------------------------------------
'' Every child frmMain_PositionWindows positions, in a FIXED order -- the order it positions
'' them in, not z-order, which moves. A diff of two dumps has to be a diff of the numbers.
'' ----------------------------------------------------------------------------------------
private sub LayoutDump_Emit( byval szState as zstring ptr )

    print
    print "[" & *szState & "]"

    LayoutDump_Child( @"MENUBAR",      HWND_FRMMAIN_MENUBAR )
    LayoutDump_Child( @"STATUSBAR",    HWND_FRMMAIN_STATUSBAR )
    LayoutDump_Child( @"PANEL",        HWND_FRMPANEL )
    LayoutDump_Child( @"SPLITPANEL",   HWND_FRMMAIN_SPLITPANEL )
    LayoutDump_Child( @"TOPTABS",      HWND_FRMMAIN_TOPTABS )
    LayoutDump_Child( @"TOPTABSMENU",  HWND_FRMMAIN_TOPTABSMENU )
    LayoutDump_Child( @"TOPTABSINFO",  HWND_FRMMAIN_TOPTABSINFO )
    LayoutDump_Child( @"FIND",         HWND_FRMMAIN_FIND )
    LayoutDump_Child( @"REPLACE",      HWND_FRMMAIN_REPLACE )
    LayoutDump_Child( @"SPLITOUTPUT",  HWND_FRMMAIN_SPLITOUTPUT )
    LayoutDump_Child( @"OUTPUT",       HWND_FRMOUTPUT )
    LayoutDump_Child( @"FINDINPROJECT", HWND_FRMFINDINPROJECT )
    LayoutDump_Child( @"SPLITV",       HWND_FRMMAIN_SPLITV )
    LayoutDump_Child( @"SPLITH",       HWND_FRMMAIN_SPLITH )

    '' The document's two editor views and their scrollbars. DocView casts the portable
    '' any ptr once and guards the index; the scrollbars are plain globals.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        LayoutDump_Child( @"EDIT0", DocView(pDoc, 0) )
        LayoutDump_Child( @"EDIT1", DocView(pDoc, 1) )
    else
        print "  EDIT0 no-document"
        print "  EDIT1 no-document"
    end if

    LayoutDump_Child( @"HSCROLL0", HWND_FRMEDITOR_HSCROLLBAR(0) )
    LayoutDump_Child( @"HSCROLL1", HWND_FRMEDITOR_HSCROLLBAR(1) )
    LayoutDump_Child( @"VSCROLL0", HWND_FRMEDITOR_VSCROLLBAR(0) )
    LayoutDump_Child( @"VSCROLL1", HWND_FRMEDITOR_VSCROLLBAR(1) )

end sub


'' ----------------------------------------------------------------------------------------
'' Lay out, then dump. Every state below goes through frmMain_PositionWindows rather than
'' computing anything of its own -- the whole point is that the editor is the authority.
'' ----------------------------------------------------------------------------------------
private sub LayoutDump_State( byval szState as zstring ptr )
    frmMain_PositionWindows()
    LayoutDump_Emit( szState )
end sub


sub LayoutDump_Run()

    if environ("TIKO_LAYOUT_DUMP") <> "1" then exit sub

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then
        print "LAYOUT DUMP: no frmMain window -- nothing to dump"
        end 1
    end if

    '' ---- pin the client size ----------------------------------------------------------
    '' Sized by the OUTER window and then measured, rather than computed through
    '' AdjustWindowRectEx: the frame is themed, DPI-dependent and the window manager may
    '' decline part of the request. What the client ACTUALLY came out as is what the shell
    '' has to reproduce, so that is what gets printed.
    dim as RECT rcWin, rcCli
    GetWindowRect( HWND_FRMMAIN, @rcWin )
    GetClientRect( HWND_FRMMAIN, @rcCli )
    dim as long nChromeW = (rcWin.right - rcWin.left) - rcCli.right
    dim as long nChromeH = (rcWin.bottom - rcWin.top) - rcCli.bottom

    SetWindowPos( HWND_FRMMAIN, 0, 0, 0, _
                  LAYOUTDUMP_W + nChromeW, LAYOUTDUMP_H + nChromeH, _
                  SWP_NOMOVE or SWP_NOZORDER )
    GetClientRect( HWND_FRMMAIN, @rcCli )

    '' ---- the header: everything a comparison depends on that is not a rectangle --------
    print "TIKO LAYOUT ORACLE"
    print "client " & str(rcCli.right) & "x" & str(rcCli.bottom)
    print "scaleX(100) " & str(pWindow->ScaleX(100)) & "  scaleY(100) " & str(pWindow->ScaleY(100))
    print "menubar-h " & str(AfxGetWindowHeight(HWND_FRMMAIN_MENUBAR))
    print "statusbar-h " & str(AfxGetWindowHeight(HWND_FRMMAIN_STATUSBAR))
    print "panel-w " & str(AfxGetWindowWidth(HWND_FRMPANEL))
    print "toptabsmenu-w " & str(AfxGetWindowWidth(HWND_FRMMAIN_TOPTABSMENU))
    print "toptabs-h " & str(AfxGetWindowHeight(HWND_FRMMAIN_TOPTABS))
    print "output-h " & str(AfxGetWindowHeight(HWND_FRMOUTPUT))
    print "tabs " & str(gTTabCtl.GetItemCount())

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()

    '' ---- the states -------------------------------------------------------------------
    '' Driven the way the UI drives them -- ShowWindow on the panel HWNDs, the gFind flags
    '' for the three bars -- because frmMain_PositionWindows reads IsWindowVisible and those
    '' flags, and a state poked in any other way is a state the editor cannot be in.
    ''
    '' Each state sets ONLY what it names and restores it before the next, so the states are
    '' independent and the file can be read in any order.

    dim as boolean bInfo0    = gFind.bShowInfoPanel
    dim as boolean bFind0    = gFind.bShowFindPanel
    dim as boolean bReplace0 = gFind.bShowReplacePanel
    dim as long    nRight0   = gConfig.ExplorerPositionRight

    gFind.bShowInfoPanel = false : gFind.bShowFindPanel = false : gFind.bShowReplacePanel = false
    ShowWindow( HWND_FRMPANEL,  SW_SHOW )
    ShowWindow( HWND_FRMOUTPUT, SW_SHOW )
    LayoutDump_State( @"BASE panel=left output=docked bars=none" )

    ShowWindow( HWND_FRMPANEL, SW_HIDE )
    LayoutDump_State( @"PANEL_HIDDEN" )
    ShowWindow( HWND_FRMPANEL, SW_SHOW )

    '' The mirror. gConfig.ShowPanelWidth must round-trip: the splitter strip is taken from
    '' the CONTENT area on both sides, so the panel keeps its width and only its edge moves.
    gConfig.ExplorerPositionRight = true
    frmPanel_PositionWindows()
    LayoutDump_State( @"PANEL_RIGHT" )
    gConfig.ExplorerPositionRight = nRight0
    frmPanel_PositionWindows()

    ShowWindow( HWND_FRMOUTPUT, SW_HIDE )
    LayoutDump_State( @"OUTPUT_HIDDEN" )
    ShowWindow( HWND_FRMOUTPUT, SW_SHOW )

    '' The three conditional bands, one at a time and then together. Individually because a
    '' band that is off by its own height is invisible when a neighbour absorbs the error;
    '' together because the ScaleY(8) top margin the ELSE arm of the info band adds only
    '' shows up in the combinations that do not have it.
    gFind.bShowInfoPanel = true
    LayoutDump_State( @"BAR_INFO" )
    gFind.bShowInfoPanel = false

    gFind.bShowFindPanel = true
    LayoutDump_State( @"BAR_FIND" )
    gFind.bShowFindPanel = false

    gFind.bShowReplacePanel = true
    LayoutDump_State( @"BAR_REPLACE" )
    gFind.bShowReplacePanel = false

    gFind.bShowInfoPanel = true : gFind.bShowFindPanel = true : gFind.bShowReplacePanel = true
    LayoutDump_State( @"BAR_ALL" )
    gFind.bShowInfoPanel = false : gFind.bShowFindPanel = false : gFind.bShowReplacePanel = false

    '' The two split modes. Both need a document, and say so rather than dumping a state
    '' that silently fell back to SplitNone -- a dump whose absence is unexplained is worse
    '' than no dump, because it reads as a layout that has no split.
    if pDoc = 0 then
        print
        print "[SPLIT_LEFTRIGHT] SKIPPED -- no document open"
        print "[SPLIT_TOPBOTTOM] SKIPPED -- no document open"
        print "  Pass a file on the command line: tiko.exe <file>"
    else
        dim as SPLIT_MODE nMode0 = pDoc->EditorSplitMode
        dim as long nSplitX0 = pDoc->SplitX
        dim as long nSplitY0 = pDoc->SplitY

        '' SETTING THE MODE IS NOT ENOUGH, and the first version of this file got it wrong.
        '' SplitX/SplitY are the split POSITION and default to 0, so a mode set on its own
        '' dumps a degenerate split -- one pane of zero extent, and its scrollbar hanging off
        '' the left edge of the content area. It looks like a layout defect and is not one.
        ''
        '' OnCommand_ViewSplit (frmMainView.inc:76-125) sets the mode, lays out ONCE so the
        '' unsplit view has a real rectangle to halve, and then takes the midpoint of it.
        '' That order is the whole trick and it is reproduced here. What is NOT reproduced is
        '' the Scintilla scroll/caret sync that follows it -- SCI_SETFIRSTVISIBLELINE and
        '' SCI_GOTOPOS move the TEXT, not the windows, and nothing here reads them.
        pDoc->EditorSplitMode = SplitLeftRight
        frmMain_PositionWindows()
        dim as RECT rcV = AfxGetWindowRect( DocView(pDoc, 0) )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(LPPOINT, @rcV), 2 )
        pDoc->SplitX = rcV.left + (rcV.right - rcV.left) / 2
        LayoutDump_State( @"SPLIT_LEFTRIGHT" )

        pDoc->EditorSplitMode = SplitTopBottom
        pDoc->SplitX = 0
        frmMain_PositionWindows()
        dim as RECT rcH = AfxGetWindowRect( DocView(pDoc, 0) )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(LPPOINT, @rcH), 2 )
        '' \ not /: / is floating point in FB and assigning it to a long rounds. Same note
        '' as the one on frmMainView.inc:118, and for the same reason.
        pDoc->SplitY = (rcH.bottom - rcH.top) \ 2
        LayoutDump_State( @"SPLIT_TOPBOTTOM" )

        pDoc->EditorSplitMode = nMode0
        pDoc->SplitX = nSplitX0
        pDoc->SplitY = nSplitY0
    end if

    '' ---- restore, then leave ----------------------------------------------------------
    gFind.bShowInfoPanel    = bInfo0
    gFind.bShowFindPanel    = bFind0
    gFind.bShowReplacePanel = bReplace0
    frmMain_PositionWindows()

    print
    print "LAYOUT DUMP COMPLETE"

    '' Block-buffered when redirected -- see the header. A killed process loses the lot.
    end 0

end sub
