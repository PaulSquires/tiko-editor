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
''     g_panel->AddHeader( wszText, idx, 0 )                            ' here, widget
''
'' ---- THE TWO REAL DIFFERENCES, AND BOTH ARE THIS BINARY'S DOING ------------------------
''
'' 1. THE ROW CARRIES A TAB INDEX, NOT A DOCUMENT POINTER -- in TWO SLOTS, since 7c step 8.
''    tiko's control has itemData + itemDataExtra and uses them for (pDoc, line). PsPlatform's
''    had one, so this file bit-packed the pair into a single 64-bit integer and carried a
''    shift, a mask and a clamp to do it. PsListTree has the second slot now and the packing
''    is gone: the tab index is slot 1 and the line is slot 2, which is tiko's own shape.
''
''    It stays INDEX-based rather than pointer-based -- a tab index, not a clsDocument ptr --
''    because this shell is index-based throughout and a pointer parked inside a list control
''    outlives nothing safely.
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
'' THE ROW'S TWO DATA SLOTS: slot 1 is the tab index, slot 2 is the line number.
''
'' THE CLAMP SURVIVED THE UNPACKING. Scintilla answers -1 for "no such line", and -1 in a
'' slot of its own no longer corrupts the tab index the way it corrupted the packed one --
'' but it still reaches SelectLine, which is the defect the clamp was written for. So it
'' stays, as a function rather than as a side effect of an encoding.
''
'' HEADERS STORE THEIR LINE AS 0 and are told apart by PsListTree.IsHeader, which the control
'' already tracks. Encoding "this is a header" into the data as well would be a second
'' source of truth for something the control knows.
'' ---------------------------------------------------------------------------------------
function ShellPanel_ClampLine( byval nLine as long ) as long
    if nLine < 0 then return 0
    return nLine
end function

function ShellPanel_TabOf( byval nRow as long ) as long
    if g_panel = 0 then return -1
    return cast(long, g_panel->GetItemData( nRow ))
end function

function ShellPanel_LineOf( byval nRow as long ) as long
    if g_panel = 0 then return 0
    return cast(long, g_panel->GetItemData2( nRow ))
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

    dim as long idxTab = ShellPanel_TabOf( nRow )
    dim as long nLine  = ShellPanel_LineOf( nRow )

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


'' PsListTree's callbacks, AND THE ARROW KEYS DO NOT JUMP ANY MORE.
''
'' ---- WHAT tiko DOES: jump on a single LEFT-BUTTON-UP, and nothing on arrow keys.
'' ---- WHAT PsListTree OFFERS: OnSelChange (any selection change) and OnActivate
''      (double-click or Enter).
''
'' Until 7c step 8 OnSelChange could not tell the two apart, so wiring it bought the
'' single-click jump AND made every arrow press move the focus to the editor -- which cost
'' the panel its own keyboard navigation and was recorded here as a divergence rather than
'' discovered later.
''
'' GetSelSource closes it. The handler acts on a MOUSE selection and returns on a KEYBOARD
'' one, which is tiko's rule exactly. Enter still jumps, through OnActivate, so the list is
'' navigable by keyboard and still usable from it.
''
'' THIS MATTERS MORE THAN IT DID. The Functions pane lists files that are not open, and
'' choosing one OPENS IT FROM DISK -- so an arrow key that acted would load a file per
'' keypress on the way down a 134-row list.
''
'' SetCurSel IS SILENT (PsListTree.bi:343), which is what makes the reload path safe: the
'' loader restores the selection after every reload, and a notifying setter would jump the
'' editor every time a bookmark was toggled.
private sub ShellPanel_OnRowSelected( byval pList as any ptr, byval nRow as long, _
                                      byval ud as any ptr )
    dim as PsListTree ptr pLt = cast(PsListTree ptr, pList)
    if pLt = 0 then exit sub
    if pLt->GetSelSource() <> PSLT_SRC_MOUSE then exit sub
    ShellPanel_GotoRow( nRow )
end sub

'' Enter and double-click, where the source is not in question.
private sub ShellPanel_OnRowActivated( byval pList as any ptr, byval nRow as long, _
                                       byval ud as any ptr )
    ShellPanel_GotoRow( nRow )
end sub


'' Wire the panel's callbacks. Called once, beside ShellTabs_Install.
sub ShellPanel_Install()
    if g_panel = 0 then exit sub
    g_panel->OnSelChange( @ShellPanel_OnRowSelected,  0 )
    g_panel->OnActivate(  @ShellPanel_OnRowActivated, 0 )
    '' NO ZEBRA STRIPES. Reported by the author -- "the bookmark rows colored weird". The
    '' rows here are a file's bookmarks, not records, and tiko's own panel paints every row
    '' the same. Set ONCE, at install: SetAltRows is a flag the control keeps, unlike the
    '' clrRowAlt = clrBack flatten this replaces, which OnThemeChanged undid on every theme
    '' load and had to be reapplied after each one.
    g_panel->SetAltRows( false )
end sub


'' ========================================================================================
'' THE PANEL HAS A MODE NOW.
''
'' tiko's side panel is THREE PANES behind a PsIconPanel strip -- Explorer, Functions,
'' Bookmarks -- switched by IDM_VIEWEXPLORER / IDM_FUNCTIONLIST / IDM_BOOKMARKSLIST. This
'' binary has ONE PsListTree and two of those three panes, so the strip is not ported and
'' the two commands switch a mode instead.
''
'' NO NEW MENU IDS AND NO NEW .lang ENTRIES: both commands already exist in
'' app/modMenuIds.bi and both already have captions in all six language files.
'' ========================================================================================
enum ShellPanelMode
    SHPANEL_BOOKMARKS = 0
    SHPANEL_FUNCTIONS
end enum

dim shared as ShellPanelMode g_panelMode = SHPANEL_BOOKMARKS

'' BOTH loaders are forward-declared, not just the functions one. ShellPanel_Reload
'' dispatches to a pair defined below it, and a declaration for one of two is how you get
'' "Variable not declared, ShellBookmarks_Load" pointing at a sub that is plainly there
'' three hundred lines further down.
declare sub ShellBookmarks_Load()
declare sub ShellFunctions_Load()


'' Rebuild whatever the panel is currently showing. THE ONE ENTRY POINT anything outside
'' this file should use -- a caller that picked a loader directly would show the wrong list
'' the moment the mode changed under it.
sub ShellPanel_Reload()
    select case g_panelMode
        case SHPANEL_FUNCTIONS : ShellFunctions_Load()
        case else              : ShellBookmarks_Load()
    end select
end sub


'' Switch panes. Silent when the mode has not changed -- reloading a list the user is
'' already looking at would lose their scroll position for nothing.
sub ShellPanel_SetMode( byval m as ShellPanelMode )
    if m = g_panelMode then exit sub
    g_panelMode = m
    ShellPanel_Reload()
end sub


'' ---------------------------------------------------------------------------------------
'' Empty the panel. tiko's ClearBookmarks, which is one call there and one call here.
'' ---------------------------------------------------------------------------------------
sub ShellPanel_Clear()
    if g_panel = 0 then exit sub
    g_panel->clear()
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' (ShellBookmarks_Load's forward declaration moved up beside ShellFunctions_Load's when the
'' panel gained a mode -- ShellPanel_Reload needs both, and it sits above this point.)


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
    '' RELOAD THROUGH THE MODE, not straight into the bookmarks loader. Toggling a
    '' bookmark while the FUNCTIONS pane is showing must not replace it with a bookmark
    '' list -- the mode would then disagree with what is on screen, and the next pane
    '' switch would look like it did nothing.
    ''
    '' The panel is a snapshot either way. tiko calls LoadBookmarksFiles here for the
    '' same reason -- there is no notification from the document when a marker changes.
    ShellPanel_Reload()
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
    '' RELOAD, NOT CLEAR. With the markers gone the bookmarks list rebuilds empty, which is
    '' the same result -- and in FUNCTIONS mode a bare clear would wipe a list that has
    '' nothing to do with bookmarks.
    ShellPanel_Reload()
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

    '' Same reason as ClearCurrent: reload through the mode rather than clearing outright.
    ShellPanel_Reload()
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
            g_panel->AddHeader( wszText, idxTab, 0 )

            dim as long nCount = PsStrParseCount( sBookmarks, "," )
            for i as long = 1 to nCount
                dim as long nLineNum = PsVal( PsStrParse( sBookmarks, i, "," ) )
                '' ltrim, like tiko: the line's indentation is not information in a list
                '' this narrow, and every row would start at a different column.
                dim as string sDescription = ltrim( pDoc->GetLine(nLineNum) )
                wszText.Utf8 = sDescription
                g_panel->AddString( wszText, idxTab, ShellPanel_ClampLine(nLineNum) )
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


'' ========================================================================================
'' THE FUNCTIONS LIST -- the port of frmFunctions.inc's LoadFilesAndFunctions.
''
'' ---- WHERE THE DATA COMES FROM, WHICH IS THE WHOLE POINT -------------------------------
''
'' gSymDb. NOT the scanner, and not the parser. The panel asks the symbol database what
'' procedures a file has and where their bodies start; who filled that database, and on
'' which thread, is none of its business. That is why this panel could be ported without
'' threading, and it is the single most useful thing 7c step 5 established.
''
'' EnumProcsInFile, SymBodyLine and QualifiedName are all clsSymbolDb's, all in app/, and
'' all called here exactly as frmFunctions calls them.
''
'' ---- THREE DIFFERENCES FROM tiko, ALL OF THEM THIS BINARY'S SHAPE ----------------------
''
'' 1. THE FILE LIST IS THE OPEN TABS. tiko walks gSymDb.EnumUserFiles and keeps whatever
''    the EXPLORER is displaying (frmFunctions_BuildFileList). There is no Explorer here,
''    and the shell's equivalent of "the files the user has in front of them" is the tab
''    bar -- which is also what makes a row's tab index meaningful.
'' 2. THE ROW IS A NAME, NOT A NAME AND A PROTOTYPE. tiko packs "name%prototype" and its
''    control splits that into two cells; PsListTree stores one string per row and has no
''    separator convention, so the prototype has nowhere to go. It is what a tooltip would
''    carry, and tooltips are already on the not-ported list.
'' 3. TREE, NOT FLAT. tiko has both (ViewAsTree / ViewAsList) behind two commands this
''    binary does not have. The tree shape is its default, it is the shape the bookmarks
''    panel already uses, and it is what makes ShellPanel_GotoRow work unchanged.
'' ========================================================================================

'' One procedure, while the list for a file is being sorted.
type ShellFuncRow
    sName as DWSTRING
    nLine as long
end type


'' Insertion sort by name. tiko uses a quicksort over a far larger array (every proc in
'' every displayed file, flattened); this sorts ONE FILE's procedures, which is tens of
'' entries, and a quicksort's setup would cost more than the sort saves.
private sub ShellFunctions_SortByName( rows() as ShellFuncRow, byval nCount as long )
    for i as long = 1 to nCount - 1
        dim as ShellFuncRow tmp = rows(i)
        dim as long j = i - 1
        do while (j >= 0) andalso (PsUCase(rows(j).sName) > PsUCase(tmp.sName))
            rows(j + 1) = rows(j)
            j -= 1
        loop
        rows(j + 1) = tmp
    next
end sub


sub ShellFunctions_Load()
    if g_panel = 0 then exit sub

    dim as long nCurSel   = g_panel->GetCurSel()
    dim as long nTopIndex = g_panel->GetTopIndex()

    g_panel->clear()
    g_panel->BeginUpdate()

    '' A HEADER PER OPEN FILE, procedures beneath it. The tab order is the panel order,
    '' which is the order the user arranged themselves.
    for idxTab as long = 0 to g_nTabDocs - 1
        dim as clsDocument ptr pDoc = g_tabDocs(idxTab).pDoc
        if pDoc = 0 then continue for

        dim as DWSTRING wszFile = pDoc->DiskFilename
        if PsLen( wszFile ) = 0 then continue for

        dim rs() as SYMBOLREF
        dim as long nProcs = gSymDb.EnumProcsInFile( wszFile, rs() )
        if nProcs < 1 then continue for

        '' COLLECTED BEFORE BEING ADDED, because the rows are sorted by name and a list
        '' control cannot be sorted in place without moving its item data with it.
        redim as ShellFuncRow rows( 0 to nProcs - 1 )
        dim as long nKeep = 0
        for p as long = 0 to nProcs - 1
            '' A DECLARE-ONLY SYMBOL HAS NO BODY, and SymBodyLine answers 0 for it. tiko
            '' skips those (frmFunctions.inc:463) and so does this: a row that jumped to
            '' line 0 of the file would be a row that lies about where the code is.
            dim as long nBodyLine = gSymDb.SymBodyLine( rs(p) )
            if nBodyLine <= 0 then continue for
            rows(nKeep).sName = gSymDb.QualifiedName( rs(p) )
            '' SCINTILLA LINES ARE 0-BASED and the database's are 1-based -- tiko's own
            '' conversion, at the same place, with the same comment.
            rows(nKeep).nLine = nBodyLine - 1
            nKeep += 1
        next
        if nKeep < 1 then continue for

        ShellFunctions_SortByName( rows(), nKeep )

        g_panel->AddHeader( PsPathName( wszFile ), idxTab, 0 )
        for r as long = 0 to nKeep - 1
            g_panel->AddString( rows(r).sName, idxTab, ShellPanel_ClampLine(rows(r).nLine) )
        next
    next

    g_panel->EndUpdate()

    g_panel->SetTopIndex( nTopIndex )
    nCurSel = iif( nCurSel > g_panel->GetCount() - 1, g_panel->GetCount() - 1, nCurSel )
    g_panel->SetCurSel( nCurSel )

    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub
