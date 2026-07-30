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

#include once "frmFindInProject.bi"


' ========================================================================================
' True when the Find in Project tab is the tab currently showing.
' ========================================================================================
function frmFindInProject_IsActive() as boolean
    return gTTabCtl.IsFindTab( gTTabCtl.GetCurSel() )
end function


' ========================================================================================
' Paint the results surface.
'
' Colours come from the EDITOR palette, not the panel one: this window sits in the space a
' document's Scintilla windows occupy, and it is about to hold real Scintilla excerpt views
' of those same documents. Chrome drawn against a different background from the code inside
' it would read as broken.
' ========================================================================================
private sub frmFindInProject_OnPaint( byval hwnd as HWND )
    dim as PsBufferPaint b
    ' NOTE: BeginDoubleBuffer calls BeginPaint itself. Do not wrap this in another
    ' BeginPaint/EndPaint pair -- the outer one validates the update region and the buffer
    ' then paints into an empty rect, giving a blank window with a clean build.
    b.BeginDoubleBuffer( hwnd )

    dim as RECT rc: GetClientRect( hwnd, @rc )
    b.SetBackColor( ghEditor.BackColorText )
    b.PaintRect( @rc )

    ' Placeholder for the results list. Reuses the tab's own caption rather than minting a
    ' localized id that the real surface would immediately retire.
    dim as RECT rcText = rc
    rcText.top += AfxScaleY(20)
    rcText.left += AfxScaleX(20)
    b.SetFont( ghFont(GUIFONT_10) )
    b.SetForeColor( ghEditor.ForeColorText )
    b.SetBackColor( ghEditor.BackColorText )
    b.PaintText( L(206, "Find in Project"), @rcText, DT_LEFT )

    b.EndDoubleBuffer()
end sub


' ========================================================================================
' frmFindInProject window procedure
' ========================================================================================
private function frmFindInProject_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            ' Fully painted in WM_PAINT.
            return true

        case WM_PAINT
            frmFindInProject_OnPaint( hwnd )
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMFINDINPROJECT = 0
            return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Create the results window. Idempotent -- returns the existing handle if it already exists.
' ========================================================================================
function frmFindInProject_CreateWindow() as HWND
    if IsWindow( HWND_FRMFINDINPROJECT ) then return HWND_FRMFINDINPROJECT

    dim pWindow as CWindow ptr = new CWindow
    ' A child of frmMain, created hidden and sized by frmMain_PositionWindows -- exactly how
    ' a document's Scintilla windows are handled.
    '
    ' WS_EX_CONTROLPARENT is correct here and is NOT the mistake PsButton had to fix: this
    ' window really will hold child controls the dialog manager should recurse into (the
    ' pooled Scintilla excerpt views), and it never takes a tab stop of its own.
    HWND_FRMFINDINPROJECT = pWindow->Create( HWND_FRMMAIN, "", @frmFindInProject_WndProc, _
                    0, 0, 0, 0, _
                    WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                    WS_EX_CONTROLPARENT )

    return HWND_FRMFINDINPROJECT
end function


' ========================================================================================
' Size the surface into the document client area. rcDoc arrives in frmMain client
' coordinates -- the same rect frmMain_PositionWindows hands a document's editors.
' ========================================================================================
sub frmFindInProject_PositionWindows( byval rcDoc as RECT )
    if IsWindow( HWND_FRMFINDINPROJECT ) = 0 then exit sub

    SetWindowPos( HWND_FRMFINDINPROJECT, 0, _
                  rcDoc.left, rcDoc.top, _
                  rcDoc.right - rcDoc.left, rcDoc.bottom - rcDoc.top, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )
end sub


' ========================================================================================
' Env-gated self-test: TIKO_FINDPROJ_SELFTEST=1
'
' Drives the real Show and the real close path, because the thing under test is WIRING --
' the sentinel surviving GetpDoc, the outgoing editors actually hiding, the surface being
' sized, the menus' document count, and the session index. None of that is reachable from a
' pure function, and the build was clean before any of it worked.
' ========================================================================================
dim shared as long gFipTestPass, gFipTestFail

private sub frmFindInProject_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gFipTestPass += 1
    else
        gFipTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

' How many documents currently have a VISIBLE Scintilla window.
private function frmFindInProject_CountVisibleEditors() as long
    dim as long n = 0
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
            if IsWindowVisible( pDoc->hWindow(i) ) then n += 1
        next
        pDoc = pDoc->pDocNext
    loop
    function = n
end function

sub frmFindInProject_RunSelfTest()
    if environ("TIKO_FINDPROJ_SELFTEST") <> "1" then exit sub

    gFipTestPass = 0
    gFipTestFail = 0
    print "--- TIKO_FINDPROJ_SELFTEST ---"

    dim as long nTabsBefore    = gTTabCtl.GetItemCount()
    dim as long nDocTabsBefore = gTTabCtl.GetDocumentTabCount()
    dim as long nSelBefore     = gTTabCtl.GetCurSel()
    dim as long nVisBefore     = frmFindInProject_CountVisibleEditors()

    ' With no documents open, the "editors get hidden" and "document count is unchanged"
    ' assertions cannot distinguish a working fix from a broken one. Said out loud rather
    ' than left as a flattering green.
    if nDocTabsBefore = 0 then
        print "  NOTE: no document tabs open -- the hide-the-editors assertions are VACUOUS."
    end if

    frmFindInProject_Check( "baseline: no Find tab is open", _
        gTTabCtl.FindTabIndex() = -1, str(gTTabCtl.FindTabIndex()) )

    ' ---- open it ------------------------------------------------------------------------
    frmFindInProject_Show()

    dim as long idx = gTTabCtl.FindTabIndex()
    frmFindInProject_Check( "Show: the Find tab exists", idx <> -1, str(idx) )

    if idx <> -1 then
        ' THE SENTINEL INVARIANT. Every one of the ~70 "if pDoc then <dereference>" call
        ' sites in the app depends on these three answers being exactly this combination:
        ' a VALID tab, that IS the Find tab, whose pDoc reads back NULL.
        frmFindInProject_Check( "sentinel: the tab is a valid tab", _
            gTTabCtl.IsValidTab(idx) = true )
        frmFindInProject_Check( "sentinel: IsFindTab sees it", _
            gTTabCtl.IsFindTab(idx) = true )
        frmFindInProject_Check( "sentinel: GetpDoc answers NULL, not 1", _
            gTTabCtl.GetpDoc(idx) = 0 )
        frmFindInProject_Check( "sentinel: GetActiveDocumentPtr answers NULL", _
            gTTabCtl.GetActiveDocumentPtr() = 0 )

        frmFindInProject_Check( "Show: the Find tab became the current tab", _
            gTTabCtl.GetCurSel() = idx, str(gTTabCtl.GetCurSel()) & " vs " & str(idx) )

        ' The count split that keeps the File / Compile / Debug menus honest.
        frmFindInProject_Check( "counts: the tab count went UP by one", _
            gTTabCtl.GetItemCount() = nTabsBefore + 1, str(gTTabCtl.GetItemCount()) )
        frmFindInProject_Check( "counts: the DOCUMENT tab count did not move", _
            gTTabCtl.GetDocumentTabCount() = nDocTabsBefore, _
            str(gTTabCtl.GetDocumentTabCount()) & " vs " & str(nDocTabsBefore) )

        ' The session writers skip this tab, so the persisted index must name a tab that will
        ' actually be recreated -- i.e. it must land inside the DOCUMENT tab count.
        '
        ' The obvious form of this assertion, "nSave <> idx", is WRONG and was written that
        ' way first: with the Find tab as the only tab it sits at index 0 and the fallback is
        ' also 0, so it failed while nothing was broken. It could not tell "points at the
        ' Find tab" from "there is nothing else to point at". This form can.
        dim as long nSave = gTTabCtl.SaveActiveTabIndex()
        if nDocTabsBefore = 0 then
            frmFindInProject_Check( "session: with no documents, ActiveTab is 0", _
                nSave = 0, str(nSave) )
        else
            frmFindInProject_Check( "session: ActiveTab names a tab that will be restored", _
                (nSave >= 0) andalso (nSave < nDocTabsBefore), _
                str(nSave) & " of " & str(nDocTabsBefore) )
        end if

        frmFindInProject_Check( "layout: every document editor is hidden", _
            frmFindInProject_CountVisibleEditors() = 0, _
            str(frmFindInProject_CountVisibleEditors()) & " still visible" )

        ' THE DisplayScintilla FIX, and it needs a BARE SetFocusTab to be visible at all.
        '
        ' The assertion above does NOT test it: frmFindInProject_Show pairs TabFocusLost with
        ' SetFocusTab, and TabFocusLost hides the outgoing document through a VALID document
        ' index -- so pDocShow is non-null there and the hide-all loop runs even with the old
        ' early return in place. Measured: restoring that early return left this suite fully
        ' green until the sequence below was added.
        '
        ' Seven paths call SetFocusTab bare, and two of them can land on the Find tab: the
        ' tabs-list dropdown (modContextMenus) and the tab right-click. This reproduces that
        ' exact sequence -- select a document tab, then select the Find tab with no
        ' TabFocusLost -- which is where a null pDocShow reaches DisplayScintilla and the
        ' outgoing editors used to stay painted over the results.
        if nDocTabsBefore > 0 then
            dim as long nDocTab = -1
            for i as long = 0 to gTTabCtl.GetItemCount() - 1
                if gTTabCtl.GetpDoc(i) then nDocTab = i: exit for
            next
            if nDocTab <> -1 then
                gTTabCtl.SetFocusTab( nDocTab )
                frmFindInProject_Check( "bare switch: the document's editor is showing first", _
                    frmFindInProject_CountVisibleEditors() > 0, _
                    str(frmFindInProject_CountVisibleEditors()) )
                ' Deliberately NO TabFocusLost here -- that is the whole point.
                gTTabCtl.SetFocusTab( idx )
                frmFindInProject_Check( "bare switch: editors hide without TabFocusLost", _
                    frmFindInProject_CountVisibleEditors() = 0, _
                    str(frmFindInProject_CountVisibleEditors()) & " still visible" )
            end if
        else
            print "  NOTE: no document tabs -- the bare-SetFocusTab assertions are SKIPPED."
        end if

        ' The surface itself, sized by frmMain_PositionWindows off the hoisted rcDoc. Before
        ' that hoist the rect was computed only on the pDoc <> 0 path, so this was 0x0.
        frmFindInProject_Check( "layout: the results window exists", _
            IsWindow( HWND_FRMFINDINPROJECT ) <> 0 )
        frmFindInProject_Check( "layout: the results window is visible", _
            IsWindowVisible( HWND_FRMFINDINPROJECT ) <> 0 )

        dim as RECT rcSurf = AfxGetWindowRect( HWND_FRMFINDINPROJECT )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rcSurf), 2 )
        dim as RECT rcMain: GetClientRect( HWND_FRMMAIN, @rcMain )
        frmFindInProject_Check( "layout: the results window has a real size", _
            (rcSurf.right - rcSurf.left > 0) andalso (rcSurf.bottom - rcSurf.top > 0), _
            str(rcSurf.right - rcSurf.left) & " x " & str(rcSurf.bottom - rcSurf.top) )
        frmFindInProject_Check( "layout: it fits inside frmMain's client area", _
            (rcSurf.right <= rcMain.right) andalso (rcSurf.bottom <= rcMain.bottom), _
            str(rcSurf.right) & "," & str(rcSurf.bottom) & " in " & _
            str(rcMain.right) & "," & str(rcMain.bottom) )

        ' The bar is forced open, and the file-info strip stood down for it.
        frmFindInProject_Check( "bars: the Find bar was forced open", _
            gFind.bShowFindPanel = true )
        frmFindInProject_Check( "bars: the file-info strip stood down", _
            gFind.bShowInfoPanel = false )
    end if

    ' ---- close it -----------------------------------------------------------------------
    ' The real user path: the tab's X routes through CloseTab -> OnCommand_FileClose.
    if idx <> -1 then gTTabCtl.CloseTab( idx )

    frmFindInProject_Check( "close: the Find tab is gone", _
        gTTabCtl.FindTabIndex() = -1, str(gTTabCtl.FindTabIndex()) )
    frmFindInProject_Check( "close: the results window was destroyed", _
        IsWindow( HWND_FRMFINDINPROJECT ) = 0 )
    frmFindInProject_Check( "close: the tab count is back to baseline", _
        gTTabCtl.GetItemCount() = nTabsBefore, _
        str(gTTabCtl.GetItemCount()) & " vs " & str(nTabsBefore) )
    frmFindInProject_Check( "close: the file-info strip came back", _
        gFind.bShowInfoPanel = true )
    frmFindInProject_Check( "close: the Find bar was closed again", _
        gFind.bShowFindPanel = false )

    ' The editors have to come BACK. Closing the tab that was covering them is the half of
    ' the DisplayScintilla fix that a "did they hide" assertion cannot see.
    frmFindInProject_Check( "close: the document editors are visible again", _
        frmFindInProject_CountVisibleEditors() = nVisBefore, _
        str(frmFindInProject_CountVisibleEditors()) & " vs " & str(nVisBefore) )
    frmFindInProject_Check( "close: the previously active tab is current again", _
        gTTabCtl.GetCurSel() = nSelBefore, _
        str(gTTabCtl.GetCurSel()) & " vs " & str(nSelBefore) )

    ' ---- the search engine, against an independent oracle ---------------------------------
    ' THE PURE HALF IS BLIND TO WHETHER THE ENGINE FINDS ANYTHING. It only ever reaches the
    ' remap and window arithmetic, so stubbing the search out entirely leaves it fully green.
    ' What follows checks the real thing: it takes a phrase out of a real document's own text,
    ' counts the occurrences itself with plain string operations, and requires the engine to
    ' agree -- both on how many there are and on where each one is.
    dim as clsDocument ptr pTest = 0
    dim pWalk as clsDocument ptr = gApp.pDocList
    do until pWalk = 0
        if IsWindow( pWalk->hWindow(0) ) then pTest = pWalk: exit do
        pWalk = pWalk->pDocNext
    loop

    if pTest = 0 then
        print "  NOTE: no realized document -- the search-engine assertions are SKIPPED."
    else
        dim as string sText = pTest->GetText()

        ' Take the first run of >= 5 letters as the phrase, so this works against whatever
        ' file happens to be open rather than against a hardcoded needle.
        dim as string sPhrase
        dim as long nRun = 0, nRunStart = 0
        for i as long = 1 to len(sText)
            dim as long c = asc( mid(sText, i, 1) )
            dim as boolean bAlpha = ((c >= 65) andalso (c <= 90)) orelse ((c >= 97) andalso (c <= 122))
            if bAlpha then
                if nRun = 0 then nRunStart = i
                nRun += 1
                if nRun >= 5 then sPhrase = mid(sText, nRunStart, 5): exit for
            else
                nRun = 0
            end if
        next

        if len(sPhrase) = 0 then
            print "  NOTE: no usable phrase in the test document -- search assertions SKIPPED."
        else
            ' The oracle: count case-sensitively with instr, which shares no code with the
            ' engine (that goes through Scintilla's SCI_SEARCHINTARGET).
            dim as long nOracle = 0
            dim as long p = instr( sText, sPhrase )
            do while p > 0
                nOracle += 1
                p = instr( p + 1, sText, sPhrase )
            loop

            FindProject_Search( sPhrase, true, false )

            ' Find this document's group.
            dim as long nGrp = -1
            for g as long = 0 to gFip.nGroupCount - 1
                if gFip.groups(g).pDoc = pTest then nGrp = g: exit for
            next

            frmFindInProject_Check( "engine: the test document produced a group", _
                nGrp <> -1, "phrase [" & sPhrase & "] oracle " & str(nOracle) )

            if nGrp <> -1 then
                frmFindInProject_Check( "engine: match count agrees with the oracle", _
                    gFip.groups(nGrp).nMatchCount = nOracle, _
                    str(gFip.groups(nGrp).nMatchCount) & " vs " & str(nOracle) )

                ' EVERY recorded offset must actually point at the phrase in the raw bytes.
                ' This is the assertion that would catch a position off by a BOM, by a line
                ' ending, or by a UTF-8 unit -- none of which a count can see.
                dim as long nBadPos = 0
                dim as long m0 = gFip.groups(nGrp).nMatchFirst
                for i as long = m0 to m0 + gFip.groups(nGrp).nMatchCount - 1
                    if mid( sText, gFip.matches(i).nStart + 1, len(sPhrase) ) <> sPhrase then
                        nBadPos += 1
                    end if
                next
                frmFindInProject_Check( "engine: every offset points at the phrase", _
                    nBadPos = 0, str(nBadPos) & " wrong of " & str(gFip.groups(nGrp).nMatchCount) )

                ' Structural contract of the coalescing: every match belongs to exactly one
                ' block, and blocks run in ascending line order without overlapping.
                dim as long nSumMatches = 0
                dim as long nOutOfOrder  = 0
                dim as long nPrevLast    = -1
                dim as long b0 = gFip.groups(nGrp).nBlockFirst
                for b as long = b0 to b0 + gFip.groups(nGrp).nBlockCount - 1
                    nSumMatches += gFip.blocks(b).nMatchCount
                    if gFip.blocks(b).nFirstLine <= nPrevLast then nOutOfOrder += 1
                    nPrevLast = gFip.blocks(b).nFirstLine + gFip.blocks(b).nLineCount - 1
                next
                frmFindInProject_Check( "engine: every match belongs to exactly one block", _
                    nSumMatches = gFip.groups(nGrp).nMatchCount, _
                    str(nSumMatches) & " vs " & str(gFip.groups(nGrp).nMatchCount) )
                frmFindInProject_Check( "engine: blocks ascend and never overlap", _
                    nOutOfOrder = 0, str(nOutOfOrder) & " out of order" )
                frmFindInProject_Check( "engine: every block shows at least the context window", _
                    gFip.blocks(b0).nLineCount >= 1, str(gFip.blocks(b0).nLineCount) )

                ' The remap, driven through the REAL hook rather than the pure function: an
                ' insertion at the very start of the file must shift every match in it.
                dim as long nFirstStart = gFip.matches(m0).nStart
                FindProject_OnDocumentModified( pTest, SC_MOD_INSERTTEXT, 0, 7, 0 )
                frmFindInProject_Check( "remap: the hook shifted a match by the insert length", _
                    gFip.matches(m0).nStart = nFirstStart + 7, _
                    str(gFip.matches(m0).nStart) & " vs " & str(nFirstStart + 7) )

                ' ...and the suppression flag the replace loop will rely on really suppresses.
                dim as long nNow = gFip.matches(m0).nStart
                gFipSuppressRemap = true
                FindProject_OnDocumentModified( pTest, SC_MOD_INSERTTEXT, 0, 7, 0 )
                gFipSuppressRemap = false
                frmFindInProject_Check( "remap: gFipSuppressRemap blocks the hook", _
                    gFip.matches(m0).nStart = nNow, _
                    str(gFip.matches(m0).nStart) & " vs " & str(nNow) )
            end if

            ' Leave no results behind: this suite runs at startup, not on the user's command.
            FindProject_Clear()
        end if
    end if

    print "--- TIKO_FINDPROJ_SELFTEST: " & str(gFipTestPass) & " passed, " & _
          str(gFipTestFail) & " failed ---"
end sub


' ========================================================================================
' frmFindInProject_Show
'
' Reached from the Search menu, the Ctrl+Shift+F accelerator and the Explorer panel's
' magnifier icon. There is at most ONE Find in Project tab: a second invocation re-selects
' the existing one rather than stacking another.
' ========================================================================================
function frmFindInProject_Show() as LRESULT
    frmFindInProject_CreateWindow()

    dim as long idx = gTTabCtl.FindTabIndex()

    if idx = -1 then
        ' TAB_FINDINPROJECT goes into the itemData slot that normally holds a clsDocument
        ' ptr. AddTab calls SetTabText, which leaves the caption alone for a tab with no
        ' document -- which is what lets the caption below stick.
        idx = PsTabBar_AddTab( gTTabCtl.hWindow, L(206, "Find in Project"), TAB_FINDINPROJECT )
        if idx = -1 then return 0
    end if

    gTTabCtl.TabFocusLost( gTTabCtl.GetCurSel() )
    gTTabCtl.SetFocusTab( idx )

    function = 0
end function
