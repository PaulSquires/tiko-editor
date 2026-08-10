'' ========================================================================================
'' shellpanel -- the side panel's contents. A PORT of frmBookmarks.inc's loader, and the
'' first tiko FORM behaviour this binary has.
''
'' Everything before this commit was chrome (bands, menus, tabs) or model (documents, save).
'' This is the first thing that is neither: a panel that reads the document model and shows
'' it, which is what the other 48 forms are.
''
'' ---- WHAT PORTED UNCHANGED, WHICH IS MOST OF IT ----------------------------------------
''
'' The loop below is frmBookmarks.inc:195-249 with the control calls rewritten. The MODEL
'' half needed nothing: gApp.pDocList, pDoc->GetBookmarks(), pDoc->GetLine(),
'' PsPathName/PsStrParse/PsStrParseCount/PsVal are all in app/ or PsCore already, and this
'' file calls them exactly as tiko does.
''
'' The CONTROL half is a mechanical rewrite of one shape:
''
''     PsListTree_AddHeader( hCtrl, wszText, cast(integer, pDoc), 0 )   ' tiko, Win32
''     g_panel->AddHeader( wszText, ShellPanel_PackRow(idx, 0) )        ' here, widget
''
'' ---- THE TWO REAL DIFFERENCES, AND BOTH ARE THIS BINARY'S DOING ------------------------
''
'' 1. THE ROW CARRIES A TAB INDEX, NOT A DOCUMENT POINTER. tiko's list control has two data
''    slots per row (itemData + itemDataExtra) and uses them for (pDoc, line). PsPlatform's
''    has ONE. Checked before assuming: itemData has 20+ readers across five tiko forms and
''    itemDataExtra has three, so the second slot is the rare one -- and the slot is
''    `integer`, 64-bit on win64, while this shell is INDEX-based (g_tabDocs) rather than
''    pointer-based. So a tab index and a line number are two 32-bit values in one slot,
''    with no truncation and no pointer parked inside a list control.
''
'' 2. READING ANOTHER TAB'S BOOKMARKS MEANS POINTING THE VIEW AT IT. This is the cost of
''    "one view pair, many documents" (see shelltabs.bi) and it is the one place that design
''    is not free.
''
''    clsDocument.GetMarkers walks the ACTIVE VIEW: SCI_MARKERGET, line by line, through
''    GetActiveScintillaPtr. In tiko every document owns its own Scintilla window, so asking
''    any document for its bookmarks just works. Here there is ONE view, showing ONE
''    document -- so asking a background tab for its bookmarks returns THE FOREGROUND TAB'S,
''    and every group in the panel would list the same lines under a different filename.
''
''    The loader points the view at each document in turn and restores it at the end. The
''    reference discipline is not optional: SCI_GETDOCPOINTER takes no reference and
''    SCI_SETDOCPOINTER releases the outgoing document, so the original has to be held
''    across the loop or it is freed underneath the view -- the same defect that segfaulted
''    the self-test for two commits in step 3.
'' ========================================================================================

#pragma once


'' ---------------------------------------------------------------------------------------
'' THE ROW'S DATA SLOT: (tab index, line number) in one 64-bit integer.
''
'' Split out and named rather than inlined at the four call sites, because a shift-and-mask
'' written four times is four chances to get the mask wrong -- and a wrong mask here does
'' not crash, it sends a click to the wrong line of the wrong file.
''
'' HEADERS PACK THEIR LINE AS 0 and are told apart by PsListTree.IsHeader, which the control
'' already tracks. Encoding "this is a header" into the data as well would be a second
'' source of truth for something the control knows.
'' ---------------------------------------------------------------------------------------
const SHP_LINE_MASK = &hFFFFFFFFll

function ShellPanel_PackRow( byval nTab as long, byval nLine as long ) as integer
    '' CLAMPED, not packed as-is. Scintilla answers -1 for "no such line", and the mask
    '' below keeps the sign extension out of the tab half -- but the LINE would then read
    '' back as -1 and reach SelectLine. Asserted: reverting this line fails "a negative line
    '' clamps to zero" and nothing else, which is exactly the claim.
    if nLine < 0 then nLine = 0
    return (cast(integer, nTab) shl 32) or (cast(integer, nLine) and SHP_LINE_MASK)
end function

function ShellPanel_TabOf( byval nData as integer ) as long
    return cast(long, nData shr 32)
end function

function ShellPanel_LineOf( byval nData as integer ) as long
    return cast(long, nData and SHP_LINE_MASK)
end function


'' ---------------------------------------------------------------------------------------
'' GO TO A ROW'S BOOKMARK. The port of frmBookmarks_MessageCallback's WM_LBUTTONUP arm
'' (frmBookmarks.inc:113-125) plus the positioning recipe out of OpenSelectedDocument
'' (modRoutines.inc:685-697).
''
'' Split from the callbacks below so it can be driven by the self-test with a row number,
'' which is the only part of a click that is reachable without a mouse.
''
'' RETURNS FALSE when the row is a header or carries nothing -- tiko's `if IsHeader = false`
'' guard, and the reason a click on a filename row does not move the caret.
'' ---------------------------------------------------------------------------------------
function ShellPanel_GotoRow( byval nRow as long ) as boolean
    if g_panel = 0 then return false
    if (nRow < 0) orelse (nRow >= g_panel->GetCount()) then return false
    '' A HEADER IS A FILENAME, NOT A PLACE. Clicking one collapses it; jumping somewhere
    '' would fight that.
    if g_panel->IsHeader( nRow ) then return false

    dim as integer nData  = g_panel->GetItemData( nRow )
    dim as long    idxTab = ShellPanel_TabOf( nData )
    dim as long    nLine  = ShellPanel_LineOf( nData )

    '' SWITCH FIRST. ShellTabs_Show is a no-op when the tab is already current, which is the
    '' common case -- and when it is not, it is what points the single view at the right
    '' document before anything below reads or moves a caret in it.
    ShellTabs_Show( idxTab )

    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then return false
    dim as any ptr pSci = pDoc->GetActiveScintillaPtr()
    if pSci = 0 then return false

    '' ---- tiko's POSITIONING RECIPE, PORTED WHOLE. Each of the four steps earns its place:
    ''   1. A FOLDED LINE IS INVISIBLE, and GOTOLINE to a hidden line leaves the caret
    ''      somewhere the user cannot see. Unfold the block first.
    ''   2. Three lines of context above, which is tiko's own comment: "just to make it
    ''      visually more appealing".
    ''   3. GOTOLINE moves the caret.
    ''   4. CenterCurrentLine, because 2 put the line near the top and the eye wants it
    ''      nearer the middle when the jump came from somewhere else.
    if SciMsg( pSci, SCI_GETLINEVISIBLE, nLine, 0 ) = 0 then
        pDoc->FoldToggle( nLine )
    end if
    SciMsg( pSci, SCI_SETFIRSTVISIBLELINE, iif(nLine - 3 > 0, nLine - 3, 0), 0 )
    SciMsg( pSci, SCI_GOTOLINE, nLine, 0 )
    pDoc->CenterCurrentLine()

    '' THE FOCUS GOES TO THE EDITOR, which is the whole point of the gesture: the user asked
    '' to be taken somewhere, and being taken there with the focus left in the list means the
    '' next keystroke scrolls the list instead of typing. tiko does the same thing through
    '' frmMain_SetFocusToCurrentCodeWindow.
    if (g_pSurf <> 0) andalso (g_view <> 0) then g_pSurf->SetFocus( g_view )
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
    return true
end function


'' PsListTree's callbacks. BOTH are wired to the same handler, and the difference between
'' them is where this binary diverges from tiko.
''
'' ---- WHAT tiko DOES: jump on a single LEFT-BUTTON-UP, and nothing on arrow keys.
'' ---- WHAT PsListTree OFFERS: OnSelChange (any selection change -- click OR arrow) and
''      OnActivate (double-click or Enter). NEITHER is tiko's rule.
''
'' Wiring OnSelChange gives the single-click jump, which is the gesture that matters, AND
'' MAKES THE ARROW KEYS JUMP TOO -- a real divergence, named here rather than discovered.
'' It is defensible (arrowing a bookmark list to preview each one is a reasonable editor
'' behaviour) but it is NOT what tiko does, and it costs the panel its own keyboard
'' navigation: every arrow press moves the focus to the editor.
''
'' The fix belongs in PsPlatform -- a source argument on the callback, so a host can tell a
'' mouse selection from a keyboard one -- and that is a control change, not a port task.
''
'' SetCurSel IS SILENT (PsListTree.bi:343), which is what makes this safe at all: the loader
'' restores the selection after every reload, and a notifying setter would jump the editor
'' every time a bookmark was toggled.
private sub ShellPanel_OnRowSelected( byval pList as any ptr, byval nRow as long, _
                                      byval ud as any ptr )
    ShellPanel_GotoRow( nRow )
end sub


'' Wire the panel's callbacks. Called once, beside ShellTabs_Install.
sub ShellPanel_Install()
    if g_panel = 0 then exit sub
    g_panel->OnSelChange( @ShellPanel_OnRowSelected, 0 )
    g_panel->OnActivate(  @ShellPanel_OnRowSelected, 0 )
end sub


'' ---------------------------------------------------------------------------------------
'' NO ZEBRA STRIPES IN A BOOKMARK LIST.
''
'' REPORTED BY THE AUTHOR: "the bookmark rows colored weird". PsListTree paints every ODD
'' ROW in a second colour --
''
''     if (v and 1) = 1 then cBack = this.clrRowAlt      (PsListTree.inc:1384)
''
'' -- unconditionally, with no switch to turn it off. It suits a data grid and it is wrong
'' for this panel: the rows here are a file's bookmarks, not records, and tiko's own
'' bookmarks panel paints every row the same because it supplies its own row painter.
''
'' FLATTENED RATHER THAN REPAINTED. clrRowAlt is a public field, so setting it to clrBack
'' costs one line where an OnPaintRow callback would cost forty and would then own hot,
'' selected, header and twisty colours as well.
''
'' MUST BE CALLED AFTER EVERY THEME LOAD. PsListTree.OnThemeChanged re-reads both colours
'' from the theme (PsListTree.inc:1648-1653), so anything that applies a theme puts the
'' stripes back. That is a sharp edge, and the real fix is a SetAltRows(bOn) on the control
'' -- a PsPlatform change, recorded here rather than made from a port task.
'' ---------------------------------------------------------------------------------------
sub ShellPanel_ApplyTheme()
    if g_panel = 0 then exit sub
    g_panel->clrRowAlt = g_panel->clrBack
    g_panel->Invalidate()
end sub


'' ---------------------------------------------------------------------------------------
'' Empty the panel. tiko's ClearBookmarks, which is one call there and one call here.
'' ---------------------------------------------------------------------------------------
sub ShellPanel_Clear()
    if g_panel = 0 then exit sub
    g_panel->clear()
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' ---------------------------------------------------------------------------------------
'' Rebuild the bookmark list from every open document. The port of LoadBookmarksFiles.
''
'' FORWARD-DECLARED because the commands below call it and it is defined after them in
'' reading order -- toggling a bookmark reloads the panel.
'' ---------------------------------------------------------------------------------------
declare sub ShellBookmarks_Load()


'' ---------------------------------------------------------------------------------------
'' THE FOUR COMMANDS, ported from OnCommand_SearchBookmarks (frmMainSearch.inc:427).
''
'' ---- WHAT DID NOT COME WITH THEM: NavHistory --------------------------------------------
''
'' tiko wraps both jumps in NavHistory_RecordJump / NavHistory_NoteArrival so Back and
'' Forward can retrace them. modNavHistory.inc IS NOT LINKABLE HERE -- it reaches gTTabCtl
'' (the Win32 tab facade) and pDoc->hWndActiveScintilla, five sites of it. Only the HEADER
'' is in app/; the body stayed in the shell, and nothing noticed until something tried to
'' call it from the other binary.
''
'' So this shell's Next/Prev jump WITHOUT recording history, and that is a missing feature
'' rather than a difference of opinion. It is one of the things the per-form cost in
'' docs/port/7c-step4.md has to count.
'' ---------------------------------------------------------------------------------------
sub ShellBookmarks_Toggle()
    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then exit sub
    pDoc->ToggleBookmark( pDoc->GetCurrentLineNumber() )
    '' RELOAD, because the panel is a snapshot. tiko calls LoadBookmarksFiles here for the
    '' same reason -- there is no notification from the document when a marker changes.
    ShellBookmarks_Load()
end sub


sub ShellBookmarks_Next()
    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then exit sub
    pDoc->NextBookmark()
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


sub ShellBookmarks_Prev()
    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then exit sub
    pDoc->PrevBookmark()
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' Clear every bookmark in the CURRENT document.
''
'' ---- MARKER_BOOKMARK, NOT -1, AND THAT IS A DELIBERATE DIVERGENCE FROM tiko.
''
'' frmMainSearch.inc:449 passes -1 to SCI_MARKERDELETEALL, which is Scintilla's "every
'' marker type" -- so tiko's Clear All Bookmarks also deletes the document's BREAKPOINTS and
'' the debugger's current-line marker. Its own Clear-All-DOCUMENTS arm four lines below
'' passes MARKER_BOOKMARK, so the two halves of the same feature disagree.
''
'' This is the reading a command called "clear all bookmarks" should have. FLAGGED RATHER
'' THAN FIXED IN tiko: that is the author's call on their own editor, not a change to make
'' silently from a port.
sub ShellBookmarks_ClearCurrent()
    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then exit sub
    SciMsg( pDoc->GetActiveScintillaPtr(), SCI_MARKERDELETEALL, MARKER_BOOKMARK, 0 )
    ShellPanel_Clear()
end sub


'' COMPOSED SEPARATELY FROM BEING SHOWN, like BuildExitBox and the two save boxes: the
'' captions, the buttons and the cancel id are decided windowlessly and can be asserted;
'' PsMessageBoxShowModal needs a compositor and cannot.
''
'' Ids 248 and 214 exist in all six .lang files already -- no id is added here.
sub BuildClearAllBookmarksBox( byref box as PsMessageBox )
    box.SetCaption( L(214, "Confirm") )
    box.SetText( L(248, "Do you want to delete all bookmarks?") )
    box.SetIcon( MBX_ICON_QUESTION )
    box.AddButton( L(94, "Yes"),    MBX_ID_YES )
    box.AddButton( L(95, "No"),     MBX_ID_NO )
    box.AddButton( L(1,  "Cancel"), MBX_ID_CANCEL )
    '' DEFAULT = NO. tiko passes no explicit default here and gets the first button, which
    '' puts the destructive answer under a reflexive Return. This asks about EVERY open
    '' document at once, so it is the more destructive of the two.
    box.SetDefaultButton( 1 )
    box.SetCancelId( MBX_ID_CANCEL )
end sub


'' Clear bookmarks in EVERY open document. TRUE if it went ahead.
function ShellBookmarks_ClearAllDocs() as boolean
    '' NO SURFACE MEANS NO, not "assume yes". Same rule as ConfirmExit and the lossy-save
    '' prompt: the destructive answer is never the fallback.
    if g_pSurf = 0 then return false
    dim as PsMessageBox box
    BuildClearAllBookmarksBox( box )
    if PsMessageBoxShowModal( g_pSurf, box ) <> MBX_ID_YES then return false

    '' THE SAME DOC-POINTER WALK THE LOADER DOES, and for the same reason:
    '' SCI_MARKERDELETEALL acts on the view's CURRENT document, and this binary has one
    '' view. Without the switch this clears the foreground document N times and leaves every
    '' other document's bookmarks exactly where they were.
    dim as any ptr pWasDoc = 0
    '' CARET AND SCROLL SAVED TOO -- see ShellBookmarks_Load. Any walk that points the view
    '' at another document owes the user their position back, and this is the second of the
    '' two walks in this file.
    dim as long nWasPos = 0, nWasFirst = 0
    if g_view <> 0 then
        pWasDoc = cast( any ptr, g_view->Msg(SCI_GETDOCPOINTER, 0, 0) )
        if pWasDoc <> 0 then g_view->Msg( SCI_ADDREFDOCUMENT, 0, cast(integer, pWasDoc) )
        nWasPos   = g_view->Msg( SCI_GETCURRENTPOS, 0, 0 )
        nWasFirst = g_view->Msg( SCI_GETFIRSTVISIBLELINE, 0, 0 )
    end if

    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        dim as long idxTab = ShellTabs_IndexOfDoc( pDoc )
        if (idxTab >= 0) andalso (g_view <> 0) then
            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idxTab).pSciDoc) )
            SciMsg( g_view->pSci, SCI_MARKERDELETEALL, MARKER_BOOKMARK, 0 )
        end if
        pDoc = pDoc->pDocNext
    loop

    if (g_view <> 0) andalso (pWasDoc <> 0) then
        g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pWasDoc) )
        g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pWasDoc) )
        g_view->Msg( SCI_GOTOPOS, nWasPos, 0 )
        g_view->Msg( SCI_SETFIRSTVISIBLELINE, nWasFirst, 0 )
    end if

    ShellPanel_Clear()
    return true
end function


'' ---------------------------------------------------------------------------------------
sub ShellBookmarks_Load()
    if g_panel = 0 then exit sub

    '' NO ShowWindow(SW_HIDE) HERE. tiko hides the control for the duration "so that we
    '' don't get flicker from white background from the empty listbox" -- a Win32 problem
    '' with a Win32 fix. PsSurface repaints from a damage region once per frame, so the
    '' intermediate states are never on screen; BeginUpdate/EndUpdate below is the whole of
    '' what this needs, and it is what PsListTree.bi:282 asks bulk loads to use.
    dim as long nCurSel   = g_panel->GetCurSel()
    dim as long nTopIndex = g_panel->GetTopIndex()

    g_panel->clear()
    g_panel->BeginUpdate()

    '' ---- HOLD THE VIEW'S CURRENT DOCUMENT ACROSS THE WALK. See the header: the loop
    '' points the view at each document in turn, and SCI_SETDOCPOINTER releases whatever it
    '' was showing. Without the ADDREF the shell's own document is freed while the user is
    '' looking at it.
    dim as any ptr pWasDoc = 0
    '' ---- AND THE CARET AND SCROLL WITH IT, WHICH IS NOT THE SAME THING -----------------
    '' SCI_SETDOCPOINTER RE-ATTACHES A DOCUMENT AND RESETS THE VIEW'S CARET AND SCROLL.
    '' Position belongs to the VIEW, not to the document -- the same fact ShellTabs_Show
    '' exists to work around when switching tabs -- so pointing the view away and back is
    '' NOT a no-op, even when it is the same document both times.
    ''
    '' REPORTED BY THE AUTHOR: Ctrl+F2 set the bookmark and then threw the caret to line 1,
    '' column 1, because toggling reloads the panel and the reload walked the documents.
    dim as long nWasPos = 0, nWasFirst = 0
    if g_view <> 0 then
        pWasDoc = cast( any ptr, g_view->Msg(SCI_GETDOCPOINTER, 0, 0) )
        if pWasDoc <> 0 then g_view->Msg( SCI_ADDREFDOCUMENT, 0, cast(integer, pWasDoc) )
        '' ---- g_view->Msg, NOT SciMsg, AND THAT IS NOT A STYLE CHOICE.
        '' SciMsg is a FUNCTION POINTER bound by ShellHost_CreateView -- which does not run
        '' until the first document is created. This loader is called once at startup with
        '' no documents open, so SciMsg was NULL and the first version of this fix crashed
        '' the whole binary before the window appeared. PsSciView.Msg is a method and is
        '' always safe.
        nWasPos   = g_view->Msg( SCI_GETCURRENTPOS, 0, 0 )
        nWasFirst = g_view->Msg( SCI_GETFIRSTVISIBLELINE, 0, 0 )
    end if

    '' gApp.pDocList, exactly as tiko walks it -- the list this binary only started
    '' appearing in one commit ago. The tab INDEX comes from the shell's own array, which is
    '' the one thing tiko does not need: its rows carry the document pointer instead.
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        dim as long idxTab = ShellTabs_IndexOfDoc( pDoc )

        dim as string sBookmarks
        if (idxTab >= 0) andalso (g_view <> 0) then
            '' The view now shows THIS document, so GetBookmarks reads THIS document's
            '' markers rather than the foreground tab's.
            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idxTab).pSciDoc) )
            sBookmarks = pDoc->GetBookmarks()
        end if

        if len(sBookmarks) then
            dim as DWSTRING wszText = PsPathName( pDoc->DiskFilename )
            g_panel->AddHeader( wszText, ShellPanel_PackRow(idxTab, 0) )

            dim as long nCount = PsStrParseCount( sBookmarks, "," )
            for i as long = 1 to nCount
                dim as long nLineNum = PsVal( PsStrParse( sBookmarks, i, "," ) )
                '' ltrim, like tiko: the line's indentation is not information in a list
                '' this narrow, and every row would start at a different column.
                dim as string sDescription = ltrim( pDoc->GetLine(nLineNum) )
                wszText.Utf8 = sDescription
                g_panel->AddString( wszText, ShellPanel_PackRow(idxTab, nLineNum) )
            next
        end if

        pDoc = pDoc->pDocNext
    loop

    '' PUT THE VIEW BACK. Point away first, then release -- releasing the document the view
    '' is currently showing takes its refcount to zero and frees it under the view.
    if (g_view <> 0) andalso (pWasDoc <> 0) then
        g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pWasDoc) )
        g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pWasDoc) )
        '' PUT THE USER BACK. Caret first, then the scroll: SCI_GOTOPOS scrolls to reveal
        '' the caret, so setting first-visible before it would be undone -- the same
        '' ordering ShellTabs_Show documents.
        g_view->Msg( SCI_GOTOPOS, nWasPos, 0 )
        g_view->Msg( SCI_SETFIRSTVISIBLELINE, nWasFirst, 0 )
    end if

    g_panel->EndUpdate()

    '' Restore where the user was looking. CLAMPED, because the reload may have produced
    '' fewer rows than there were -- tiko does the same MIN for the same reason.
    g_panel->SetTopIndex( nTopIndex )
    nCurSel = iif( nCurSel > g_panel->GetCount() - 1, g_panel->GetCount() - 1, nCurSel )
    g_panel->SetCurSel( nCurSel )

    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub
