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

#include once "frmTopTabs.bi"


' ========================================================================================
' Make sure CurrentSelection describes something before anybody reasons about it.
' ========================================================================================
' CurrentSelection HAS EXACTLY TWO WRITERS and both are in frmMainOnCommand.inc: the
' editor's SCEN_KILLFOCUS fills it, and the editor's SCEN_SETFOCUS clears it. So while the
' editor HAS THE FOCUS the capture is always uninitialized -- and Ctrl+F is pressed with the
' editor focused.
'
' THAT MADE "SEARCH WITHIN THE SELECTION" UNREACHABLE FROM Ctrl+F. FindControls_Show tests
' isInitialized before believing the line range, so selecting two lines and pressing Ctrl+F
' read an empty capture: the multi-line branch never ran, the box was seeded with the
' selected TEXT instead of cleared, and Selection never armed. A multi-line string in a
' single-line box shows its first line, which is what the bar displayed.
'
' The same hole is under the Selection icon: clicking it does not necessarily take the focus
' off the editor first, so its own multi-line test read the same empty capture and the icon
' refused to latch.
'
' THIS IS WHAT SCEN_KILLFOCUS WOULD HAVE DONE, done at the moment it is needed rather than
' waiting for a focus change that has not happened yet. Guarded on isInitialized, so a
' capture that DID come from a real focus loss is never overwritten -- which matters,
' because by the time the incremental search has run the live selection is the last match.

' ========================================================================================
' The Selection icon's whole behaviour. Called by the icon, and by the harness.
' ========================================================================================
function FindReplace_ToggleSelection() as long
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0
    dim as boolean bWasOn = cbool( gFind.nSelection <> 0 )
    ' Clicking this icon does not necessarily take the focus off the editor, so the capture
    ' the test below reads may never have been filled.
    FindReplace_EnsureSelectionCaptured( pDoc )
    dim as boolean isOriginalSelectionMultiline = _
        (pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
    if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
    if (isOriginalSelectionMultiline) orelse (pDoc->HasMarkerHighlight) then
        gFind.nSelection = not gFind.nSelection
    else
        gFind.nSelection = false
    end if
    if gFind.nSelection then
        pDoc->SetMarkerHighlight
    else
        pDoc->RemoveMarkerHighlight
        ' TURNING IT OFF DROPS THE TEXT SELECTION AS WELL, by request: "editor remains
        ' selected. it should unselect." ONLY ON THE ON -> OFF TRANSITION -- a REFUSAL also
        ' lands in this branch, and collapsing a selection the user just made because the
        ' button declined to arm would be the opposite of what was asked for.
        if bWasOn then
            dim as HWND hEdit = pDoc->hWndActiveScintilla
            if IsWindow( hEdit ) then
                SciExec( hEdit, SCI_SETEMPTYSELECTION, _
                         SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 ), 0 )
            end if
        end if
    end if
    FindReplace_HighlightSearches( theme.editor.occurrence )
    return 0
end function


function FindReplace_EnsureSelectionCaptured( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then return 0
    if pDoc->CurrentSelection.isInitialized then return 0
    dim as long startline, endline, startpos, endpos
    pDoc->GetSelectedLineRange( startLine, endLine, startPos, endPos )
    pDoc->CurrentSelection.startline     = startline
    pDoc->CurrentSelection.endline       = endline
    pDoc->CurrentSelection.startpos      = startpos
    pDoc->CurrentSelection.endpos        = endpos
    pDoc->CurrentSelection.isInitialized = true
    return 0
end function


' ========================================================================================
' THE ONE DOOR for the Find/Replace bars' visibility.
' ========================================================================================
' Sets the flags, repositions, and restores focus. Every mutator goes through here, so a
' caller can no longer set state that nothing acts on -- which used to present as "the
' menu item did nothing", because frmMain_PositionWindows is the only code that turns
' these flags into windows and it was the CALLER's job to remember to run it.
'
' REPLACE IMPLIES FIND. A visible Replace bar with no Find bar above it is not a state the
' layout has; enforcing it in one place beats trusting five call sites to agree.
'
' IDEMPOTENT, AND THAT IS THE POINT OF THE EARLY RETURN. OnCommand_FileClose and
' OnCommand_ProjectClose both call the hide path unconditionally, on paths where nothing is
' open. That used to walk every open document, run a full re-layout, and -- the part worth
' avoiding -- call frmMain_SetFocusToCurrentCodeWindow while that very window was being
' destroyed.
function FindReplace_SetVisible( byval bFind as boolean, byval bReplace as boolean ) as long
    ' The bars are created once at startup, but WM_NCDESTROY zeroes these globals. Without
    ' the guard every GetDlgItem below silently returns 0 and the work is done on nothing.
    if IsWindow(HWND_FRMMAIN_FIND) = 0 then return 0
    if IsWindow(HWND_FRMMAIN_REPLACE) = 0 then return 0

    if bReplace then bFind = true

    if (gFind.bShowFindPanel = bFind) andalso (gFind.bShowReplacePanel = bReplace) then return 0

    dim as boolean bClosing = (gFind.bShowFindPanel = true) andalso (bFind = false)

    gFind.bShowInfoPanel    = true
    gFind.bShowFindPanel    = bFind
    gFind.bShowReplacePanel = bReplace

    if bClosing then
        ' Drop the search highlights AND the Selection latch. Leaving nSelection set was
        ' how a closed-and-reopened Find bar came back with Selection lit against a marker
        ' range that no longer described anything -- visible now that the button paints
        ' from this model rather than from a control's own state.
        gFind.nSelection = false
        dim as clsDocument ptr pDoc
        for i as integer = 0 to gTTabCtl.GetItemCount() - 1
            pDoc = gTTabCtl.GetpDoc(i)
            if pDoc = null then continue for
            pDoc->RemoveMarkerHighlight()
            pDoc->RemoveSelectionAttributes()
            ' AND THE CAPTURE GOES WITH THEM. Until FindReplace_EnsureSelectionCaptured
            ' existed, CurrentSelection was only ever filled on the editor's KILLFOCUS and
            ' was empty at every moment FindControls_Show looked at it -- so nothing here
            ' needed to clear it. NOW THAT Ctrl+F FILLS IT, a capture left behind is a
            ' multi-line range that no longer describes anything, and the next Ctrl+F arms
            ' Selection against it whatever is selected. Reported as "closing Find and
            ' ctrl+f a second time always has the icon lit".
            pDoc->CurrentSelection.isInitialized = false
        next
    end if

    frmMain_PositionWindows()

    ' Only on the closing edge: opening the bars puts focus in the find field (the caller's
    ' job), and a no-op call must not move focus at all.
    if bClosing then frmMain_SetFocusToCurrentCodeWindow()

    return 0
end function


' ========================================================================================
' Expand the frmTopTabsInfo area to allow for the Find controls.
' ========================================================================================
' Leaves the Replace bar exactly as it found it -- this used to clear bExpanded while
' leaving bShowReplacePanel set, which is how Ctrl+F followed by Ctrl+H became a keypress
' that did nothing.
function FindControls_Show() as long
    if IsWindow(HWND_FRMMAIN_FIND) = 0 then return 0

    ' pDoc can be null when the Find panel is being shown for Project Search.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        ' SEE FindReplace_EnsureSelectionCaptured. Without this the test below reads an
        ' uninitialized capture on every Ctrl+F, because the editor still has the focus.
        FindReplace_EnsureSelectionCaptured( pDoc )
        dim as boolean isOriginalSelectionMultiline = _
                        cbool(pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
        if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
        
        ' THE FIELD SHOWS THE SELECTION, INCLUDING WHEN THERE ISN'T ONE -- WITH ONE
        ' EXCEPTION ADDED BELOW, which is why this no longer says ALWAYS. Opening Find
        ' with nothing selected clears the box rather than re-offering the last phrase, which
        ' is by request: the box is a picture of the selection, so an empty selection is an
        ' empty box and Ctrl+F is always ready to be typed into. There used to be a bSeedField
        ' flag here that skipped the write in exactly that case; it is gone, and reinstating
        ' it puts the old term back.
        '
        ' The multi-line case clears the field for a DIFFERENT reason and is kept separate:
        ' it means "search within this selection", which starts with no term by definition.
        dim as string sFindText = pDoc->GetSelText
        if isOriginalSelectionMultiline then
            sFindText = ""
            gFind.nSelection = true
            pDoc->SetMarkerHighlight
        end if

        ' ---- ONE EXCEPTION: THE SAME WORD IN THE DOCUMENT'S CASE -------------------------
        '
        ' Type "afxnova", press Ctrl+H, and the field became "AfxNova" -- because the
        ' incremental search had already selected the match, and the selection is the
        ' document's casing rather than what was typed. The rule above is right; this is the
        ' one case where it takes something the user authored and hands back something they
        ' did not, for no gain: it is the SAME WORD and it will find the same matches.
        '
        ' EQUAL IGNORING CASE BUT NOT EQUAL EXACTLY. Both halves matter. An exact match is
        ' already a no-op below, and a genuinely different word must still reseed -- that is
        ' the behaviour the comment above records as requested and it is untouched.
        '
        ' BOTH COMPARISONS ARE LIKE-FOR-LIKE, and that is not fussiness. This file has been
        ' bitten twice by string/DWSTRING conversions that compile and mean something else:
        ' the fold is DWSTRING against DWSTRING (PsUCase returns one), and the exact test is
        ' string against string via .Utf8. Neither leaves fbc a conversion to choose.
        dim as string sTyped = gFind.txtFind.Utf8
        dim as boolean bCaseOnlyEcho = false
        if len(sFindText) then
            if (PsUCase(sFindText) = PsUCase(gFind.txtFind)) andalso _
               (sFindText <> sTyped) then bCaseOnlyEcho = true
        end if

        if bCaseOnlyEcho = false then
            ' One assignment for both paths, so the box and the model cannot disagree about
            ' what is being searched for -- the multi-line branch used to clear the box and
            ' leave gFind.txtFind holding the previous phrase.
            gFind.txtFind = sFindText

            dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
            if sFindText <> AfxGetWindowText(hCtl) then
                AfxSetWindowText( hCtl, sFindText )
                dim as long nLen = len(sFindText)
                Edit_SetSel( hCtl, nLen, nLen )
            end if
        end if
    end if
        
    return FindReplace_SetVisible( true, gFind.bShowReplacePanel )
end function


' ========================================================================================
' Expand the frmTopTabsInfo area to allow for the Replace controls.
' ========================================================================================
function ReplaceControls_Show() as long
    if IsWindow(HWND_FRMMAIN_REPLACE) = 0 then return 0

    ' Display the Replace controls
    AfxSetWindowText( GetDlgItem( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE ), "" )
    ' This is the ONLY writer of bProjectReplaceActive, which frmMain_PositionWindows reads
    ' to keep the Replace bar open on the Find in Project tab. Without it that read is a
    ' permanently-false disjunct.
    if gTTabCtl.IsFindTab( gTTabCtl.GetCurSel() ) then
        gFind.bProjectReplaceActive = true
    end if
    return FindReplace_SetVisible( true, true )
end function


' ========================================================================================
' Hide the Find/Replace controls and collapse the frmTopTabsInfo area.
' ========================================================================================
function FindReplaceControls_Hide() as long
    return FindReplace_SetVisible( false, false )
end function


' ========================================================================================
' Hide the frmTopTabsInfo panel.
' ========================================================================================
function TopTabsInfoPanel_Hide() as long
    gFind.bShowInfoPanel = false
    return 0
end function


' ========================================================================================
' Show the frmTopTabsInfo panel.
' ========================================================================================
function TopTabsInfoPanel_Show() as long
    gFind.bShowInfoPanel = true
    return 0
end function



' ========================================================================================
' THE SEARCH ITSELF MOVED TO app/modFindReplace.inc IN 7c STEP 26.
'
' isUpperCaseString, isLowerCaseString, FindReplace_CollectMatchPositions,
' FindReplace_UpdateResultsFromCaret, FindReplace_NextSelection,
' FindReplace_HighlightSearches and FindReplace_DoReplace all went down together: every
' one of them was SciExec on an HWND, and SciExec IS SendMessage -- which app/
' modScintilla.bi has declared in portable types since step 3 for exactly this.
'
' WHAT STAYED IS THE WINDOW MANAGEMENT ABOVE, and that is the split rather than a
' leftover: FindReplace_SetVisible and its siblings create, size and focus real windows,
' which is the half the shell REPLACES rather than ports.
'
' TWO SIGNATURES CHANGED, both to break a reach the app layer cannot make:
'   HighlightSearches and DoReplace take the occurrence COLOUR, because `theme` lives in
'   the shell's modThemeTypes.bi and moving the whole theme tree to satisfy one indicator
'   colour would have been the larger change by far.
' ========================================================================================

' ========================================================================================
' Env-gated harness: TIKO_FINDSEL_SELFTEST=1. Drives "search within the selection".
' ========================================================================================
' WRITTEN BECAUSE THE FEATURE WAS REPORTED BROKEN AND NOTHING HERE COULD HAVE SEEN IT.
' Select two or more lines, press Ctrl+F, and the box was seeded with the selected TEXT
' instead of cleared while Selection stayed dark. The cause is in
' FindReplace_EnsureSelectionCaptured's header: CurrentSelection is filled on the editor's
' KILLFOCUS and cleared on its SETFOCUS, so while the editor HAS the focus -- which is when
' Ctrl+F is pressed -- it is always uninitialized, and the multi-line test read nothing.
'
' IT NEEDS A REAL WINDOW, A REAL DOCUMENT AND A REAL BAR, which is why it is an env-gated
' harness in the family of TIKO_FINDPROJ_* rather than a pure suite. It runs from the same
' posted message they do, for the same reason: the session has to be restored first.
'
' THE SELECTION IS MADE THE WAY A MOUSE MAKES ONE -- column 0 of a line to column 0 of a
' later line -- because GetSelectedLineRange DECREMENTS endLine when the selection ends at
' column 0. A harness that selected mid-line would prove the range arithmetic it was not
' asking about and miss the case every real drag produces.
dim shared as long gFindSelPass, gFindSelFail

private sub FindSel_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gFindSelPass += 1
    else
        gFindSelFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

private sub FindSel_Pump()
    dim as MSG uMsg
    dim as long n = 0
    do while PeekMessage( @uMsg, null, 0, 0, PM_REMOVE )
        TranslateMessage( @uMsg )
        DispatchMessage( @uMsg )
        n += 1
        if n > 2000 then exit do
    loop
end sub

sub FindReplace_RunSelectionTest()
    if environ("TIKO_FINDSEL_SELFTEST") <> "1" then exit sub

    gFindSelPass = 0
    gFindSelFail = 0
    print "--- TIKO_FINDSEL_SELFTEST ---"

    dim as DWSTRING wszTemp = environ("TEMP")
    if PsLen(wszTemp) = 0 then wszTemp = "C:\"
    if PsRight(wszTemp, 1) <> "\" then wszTemp = wszTemp & "\"
    dim as DWSTRING wszFile = wszTemp & "tiko_findsel_test.bas"

    ' A buffer already open on this path beats the bytes on disk -- the FINDPROJ harnesses
    ' learned that from a killed run leaving the temp file in the restored session.
    dim pStale as clsDocument ptr = gApp.pDocList
    do until pStale = 0
        dim pNextStale as clsDocument ptr = pStale->pDocNext
        if PsUCase(pStale->DiskFilename) = PsUCase(wszFile) then
            if IsWindow( DocView(pStale, 0) ) then
                SciExec( DocView(pStale, 0), SCI_SETSAVEPOINT, 0, 0 )
            end if
            pStale->UserModified = false
            dim as long idxStale = gTTabCtl.GetTabIndexByDocumentPtr( pStale )
            if idxStale <> -1 then gTTabCtl.CloseTab( idxStale )
        end if
        pStale = pNextStale
    loop
    FindSel_Pump()

    ' SIX LINES, WITH THE TOKEN ON FIVE OF THEM AND TWO OF THOSE OUTSIDE THE RANGE the test
    ' selects. That is what makes "the search shrank" a real answer rather than a repeat of
    ' the whole-document count.
    dim as string sSeed = _
        "' Zeta line zero"     & vbcrlf & _
        "dim Zeta as long"     & vbcrlf & _
        "print ""Zeta"""       & vbcrlf & _
        "sub ZetaThree()"      & vbcrlf & _
        "end sub"              & vbcrlf & _
        "' Zeta line five"     & vbcrlf
    if PsFileWriteAll( wszFile, sSeed ) = false then
        print "  FAIL: could not create " & wszFile.Utf8
        exit sub
    end if

    dim as wstring * MAX_PATH wszOpen = *wszFile.Wz()
    dim pT as clsDocument ptr = OpenSelectedDocument( wszOpen )
    if pT = 0 then
        print "  FAIL: could not open the temporary file"
        exit sub
    end if
    FindSel_Pump()

    dim as HWND hEdit = pT->hWndActiveScintilla
    if IsWindow( hEdit ) = 0 then
        print "  FAIL: the temporary document has no editor window"
        exit sub
    end if

    ' ---- the whole-document count, for the comparison later ----------------------------
    FindReplaceControls_Hide()
    FindSel_Pump()
    pT->RemoveMarkerHighlight()
    gFind.nSelection = false
    gFind.nMatchCase = false
    gFind.nWholeWord = false
    gFind.txtFind = "Zeta"
    FindReplace_HighlightSearches( theme.editor.occurrence )
    dim as long nWhole = gFind.foundCount
    FindSel_Check( "setup: the whole file holds five Zetas", (nWhole = 5), str(nWhole) )

    ' ---- THE REPORTED GESTURE ----------------------------------------------------------
    ' Lines 1..3, selected as a drag does it: column 0 of line 1 to column 0 of line 4.
    SciExec( hEdit, SCI_SETSELECTIONSTART, SciExec( hEdit, SCI_POSITIONFROMLINE, 1, 0 ), 0 )
    SciExec( hEdit, SCI_SETSELECTIONEND,   SciExec( hEdit, SCI_POSITIONFROMLINE, 4, 0 ), 0 )

    ' AND THE CAPTURE IS THROWN AWAY, because that is the state the bug lives in: the editor
    ' has the focus, so SCEN_SETFOCUS cleared it and SCEN_KILLFOCUS has not run.
    pT->CurrentSelection.isInitialized = false
    gFind.txtFind = "Leftover"

    FindControls_Show()
    FindSel_Pump()

    FindSel_Check( "Ctrl+F captures the selection the editor still holds", _
        pT->CurrentSelection.isInitialized )
    FindSel_Check( "  and sees it as three lines", _
        (pT->CurrentSelection.endline - pT->CurrentSelection.startline) = 2, _
        str(pT->CurrentSelection.startline) & ".." & str(pT->CurrentSelection.endline) )
    ' A MULTI-LINE SELECTION IS A RANGE, NOT A PHRASE. The reported symptom was this box
    ' holding the selected text -- of which a single-line control shows the first line.
    FindSel_Check( "  the box is EMPTY, not seeded with the selected text", _
        (PsLen(gFind.txtFind) = 0), gFind.txtFind.Utf8 )
    ' `= true`, NOT `<> 0`: the engine compares against true, which is -1.
    FindSel_Check( "  Selection arms itself", (gFind.nSelection = true), str(gFind.nSelection) )
    FindSel_Check( "  and the marker highlight is down", pT->HasMarkerHighlight )

    ' ---- AND THE SEARCH IS ACTUALLY RESTRICTED -----------------------------------------
    ' The half the icon assertions cannot give: HighlightSearches narrows its target range
    ' to First/LastMarkerHighlight, so this is what the markers were for.
    gFind.txtFind = "Zeta"
    FindReplace_HighlightSearches( theme.editor.occurrence )
    FindSel_Check( "  and the search finds only the three inside it", _
        (gFind.foundCount = 3), str(gFind.foundCount) & "/" & str(nWhole) )

    ' ---- CLICKING THE ICON WHILE IT IS LIT MUST DISARM IT -------------------------------
    ' REPORTED: "immediately click icon. editor remains selected. it should unselect."
    ' Driven through the icon's own body, which is why that body was moved out of
    ' frmFind_IconClick -- a private sub no harness could reach, which is how both defects
    ' against this button got here.
    FindReplace_ToggleSelection()
    FindSel_Pump()
    FindSel_Check( "clicking the lit icon disarms it", _
        (gFind.nSelection = 0), str(gFind.nSelection) )
    FindSel_Check( "  and takes the shading off the editor", _
        (pT->HasMarkerHighlight = false) )
    FindSel_Check( "  and drops the text selection with it", _
        (SciExec( hEdit, SCI_GETSELECTIONSTART, 0, 0 ) = _
         SciExec( hEdit, SCI_GETSELECTIONEND, 0, 0 )), _
        str(SciExec( hEdit, SCI_GETSELECTIONSTART, 0, 0 )) & ".." & _
        str(SciExec( hEdit, SCI_GETSELECTIONEND, 0, 0 )) )
    gFind.txtFind = "Zeta"
    FindReplace_HighlightSearches( theme.editor.occurrence )
    FindSel_Check( "  and the search covers the whole file again", _
        (gFind.foundCount = nWhole), str(gFind.foundCount) & "/" & str(nWhole) )

    ' ---- AND A SECOND Ctrl+F WITH NO SELECTION MUST NOT ARM -----------------------------
    ' REPORTED: "closing Find and ctrl+f a second time always has the icon lit regardless if
    ' there is an editor selection."
    FindReplaceControls_Hide()
    FindSel_Pump()
    SciExec( hEdit, SCI_SETEMPTYSELECTION, 0, 0 )
    FindControls_Show()
    FindSel_Pump()
    FindSel_Check( "reopening with NO selection leaves the icon dark", _
        (gFind.nSelection = 0), str(gFind.nSelection) )
    FindSel_Check( "  and lays no shading", (pT->HasMarkerHighlight = false) )

    ' ---- CLOSING DROPS BOTH ------------------------------------------------------------
    FindReplaceControls_Hide()
    FindSel_Pump()
    FindSel_Check( "closing the bar drops the Selection latch", _
        (gFind.nSelection = 0), str(gFind.nSelection) )
    FindSel_Check( "  and the marker highlight with it", (pT->HasMarkerHighlight = false) )

    ' ---- teardown ----------------------------------------------------------------------
    gFind.txtFind = ""
    SciExec( hEdit, SCI_SETSAVEPOINT, 0, 0 )
    pT->UserModified = false
    dim as long idx = gTTabCtl.GetTabIndexByDocumentPtr( pT )
    if idx <> -1 then gTTabCtl.CloseTab( idx )
    FindSel_Pump()
    PsFileDelete( wszFile )

    print "--- TIKO_FINDSEL_SELFTEST: " & str(gFindSelPass) & " passed, " & _
          str(gFindSelFail) & " failed ---"
end sub
