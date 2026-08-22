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