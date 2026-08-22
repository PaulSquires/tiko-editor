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


'' ========================================================================================
'' THE PANEL HAS A MODE NOW.
''
'' tiko's side panel is THREE PANES behind a PsIconPanel strip -- Explorer, Functions,
'' Bookmarks -- switched by IDM_VIEWEXPLORER / IDM_FUNCTIONLIST / IDM_BOOKMARKSLIST. This
'' binary has ONE PsListTree and, since 7c step 19, ALL THREE of those panes. The STRIP is
'' still not ported, and the three commands switch a mode instead.
''
'' THIS COMMENT SAID "PsPlatform HAS NO PsIconPanel" AND THAT WAS FALSE, for one commit.
'' src/ui/controls/PsIconPanel.bi is exactly the activity-bar strip tiko uses -- AddItem,
'' AddSeparator, SetSelected, OnClick, HitTestIndex -- it is covered by tests/pslists and
'' demoed in demos/gallery, and its own header names tiko as the source of its model. The
'' claim was made from PsListTree's callback list without looking at the controls directory.
'' Porting the strip is a step's worth of wiring, not a control's worth of building.
''
'' NO NEW MENU IDS AND NO NEW .lang ENTRIES: all three commands already exist in
'' app/modMenuIds.bi and all three already have captions in all six language files.
''
'' THIS BLOCK MOVED TO THE TOP OF THE FILE IN STEP 19. It used to sit halfway down, which
'' was fine while nothing above it named a mode; ShellPanel_LineOf reads g_panelMode now,
'' and fbc reports that as "Variable not declared" at a line that is plainly correct.
'' ========================================================================================
enum ShellPanelMode
    SHPANEL_BOOKMARKS = 0
    SHPANEL_FUNCTIONS
    '' THE THIRD PANE, 7c step 19. tiko's is the FIRST pane a user ever sees; this binary
    '' could not offer it until now, which is the whole reason step 8 had to argue about
    '' which pane to start on.
    SHPANEL_EXPLORER
end enum


'' ---------------------------------------------------------------------------------------
'' WHAT AN EXPLORER ROW IS, carried in slot 2. frmExplorer.bi's enum, unchanged in meaning
'' and unchanged in VALUE -- there is no reason for the two binaries to disagree about a
'' number that appears in both.
''
''   EXPKIND_ROOT     slot 1 = index into gConfig.Cat()
''   EXPKIND_FOLDER   slot 1 = index into gProjectFolders
''   EXPKIND_FILE     slot 1 = index into g_panelFiles     <-- NOT a clsDocument ptr
''   EXPKIND_PROMOTE  slot 1 = 0
''
'' AN INDEX, NOT A POINTER, and that is this binary's rule rather than a translation
'' accident: tiko stores `cast(integer, pDoc)` in the slot, and a pointer parked inside a
'' list control outlives nothing safely. shelltabs.bi and both existing loaders are
'' index-based for the same reason.
''
'' EXPKIND_NONE = 0 DELIBERATELY, and the reason is tiko's: slot 2 defaults to zero for any
'' row added without one, so whatever sits at zero is what an un-tagged row silently claims.
'' The safe thing to claim is "not a file". Putting EXPKIND_FILE at zero would make every
'' un-tagged row present its slot 1 as a file index.
'' ---------------------------------------------------------------------------------------
enum ShellExpKind
    EXPKIND_NONE    = 0
    EXPKIND_ROOT    = 1
    EXPKIND_FOLDER  = 2
    EXPKIND_FILE    = 3
    EXPKIND_PROMOTE = 4
end enum

'' FUNCTIONS AT STARTUP, changed in step 8, and the report that caused it is worth keeping:
'' "the functions pane (or explorer pane) is not visible when the program starts up. nothing
'' displays."
''
'' A BOOKMARKS PANE ON A FRESHLY OPENED FILE IS EMPTY BY CONSTRUCTION -- no bookmark exists
'' until someone sets one -- so the first pane a user ever saw could only be blank. tiko's
'' first pane is the EXPLORER, which this binary does not have; Functions is the first pane
'' it has that tiko also has, and the only one with something to show before the user has
'' done anything.
dim shared as ShellPanelMode g_panelMode = SHPANEL_FUNCTIONS




'' ---------------------------------------------------------------------------------------
'' THE PANEL'S FILE TABLE.
''
'' A row's first slot is an index INTO THIS, not a tab index -- which is the change 7c step 8
'' made and the reason the pane can list a file that is not open. tiko's panel is built the
'' same way (gFuncPanelFiles, frmFunctions.inc:337) and for the same reason.
''
'' WHY NOT STORE THE PATH IN THE ROW: a list control's data slot is an integer. Storing a
'' DWSTRING ptr there would mean owning its lifetime across every Clear, every reload and
'' every theme change, to save one array.
''
'' REBUILT BY WHICHEVER LOADER RAN, so all three loaders agree on what SLOT 1 carries and
'' ShellPanel_GotoRow needs no mode of its own to read it. The bookmarks loader registers the
'' open documents; the functions loader registers everything the symbol database knows; the
'' explorer loader registers every document in the workspace.
''
'' SLOT 2 IS THE HALF THAT IS NOT MODE-AGNOSTIC -- see ShellPanel_LineOf.
'' ---------------------------------------------------------------------------------------
const SHP_MAX_FILES = 1024

dim shared g_panelFiles(0 to SHP_MAX_FILES - 1) as DWSTRING
dim shared as long g_nPanelFiles

sub ShellPanel_ResetFiles()
    for i as long = 0 to g_nPanelFiles - 1
        g_panelFiles(i).Clear()
    next
    g_nPanelFiles = 0
end sub

'' Appends and returns the index, or -1 when the table is full. NOT de-duplicated: each
'' loader adds a file once, in the order it means to display it, and a de-dupe here would
'' hide a loader that had lost track of that.
function ShellPanel_AddFile( byval wszPath as DWSTRING ) as long
    if g_nPanelFiles >= SHP_MAX_FILES then return -1
    g_panelFiles(g_nPanelFiles) = wszPath
    g_nPanelFiles += 1
    return g_nPanelFiles - 1
end function

function ShellPanel_PathOf( byval nRow as long ) as DWSTRING
    if g_panel = 0 then return DWSTRING()
    dim as long i = cast(long, g_panel->GetItemData( nRow ))
    if (i < 0) orelse (i >= g_nPanelFiles) then return DWSTRING()
    return g_panelFiles(i)
end function


'' ---------------------------------------------------------------------------------------
'' THE ROW'S TWO DATA SLOTS: slot 1 is an index into g_panelFiles, slot 2 is the line.
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

'' The RAW slot, for anything that wants the index itself rather than what it points at.
function ShellPanel_FileIdxOf( byval nRow as long ) as long
    if g_panel = 0 then return -1
    return cast(long, g_panel->GetItemData( nRow ))
end function

'' THE TAB THIS ROW'S FILE IS OPEN IN, or -1 when it is not open at all -- which is now a
'' normal answer rather than an error, and is exactly what GotoRow branches on.
function ShellPanel_TabOf( byval nRow as long ) as long
    dim as DWSTRING wszPath = ShellPanel_PathOf( nRow )
    if PsLen( wszPath ) = 0 then return -1
    return ShellTabs_FindByPath( wszPath )
end function

'' ---- SLOT 2 MEANS A LINE ONLY IN TWO OF THE THREE MODES -- 7c step 19 -----------------
''
'' The Explorer needs slot 2 for the row KIND, and there are only two slots. So what slot 2
'' holds is a function of the panel MODE, exactly as what slot 1 holds is a function of the
'' KIND -- which is frmExplorer.bi's own rule, one level up.
''
'' THE GUARD IS HERE RATHER THAN AT EVERY CALLER because ShellPanel_GotoRow reads this on
'' every click. Without it a click on an Explorer file row would read EXPKIND_FILE, which is
'' 3, and jump the caret to line 3 of the file it just opened -- a defect that looks like a
'' scroll bug and is a units bug.
function ShellPanel_LineOf( byval nRow as long ) as long
    if g_panel = 0 then return 0
    if g_panelMode = SHPANEL_EXPLORER then return 0
    return cast(long, g_panel->GetItemData2( nRow ))
end function


'' The row's KIND, and the mirror of the above: meaningful only in Explorer mode, where an
'' invalid row and an un-tagged row both read EXPKIND_NONE.
function ShellPanel_KindOf( byval nRow as long ) as ShellExpKind
    if g_panel = 0 then return EXPKIND_NONE
    if g_panelMode <> SHPANEL_EXPLORER then return EXPKIND_NONE
    if g_panel->IsValidRow( nRow ) = false then return EXPKIND_NONE
    return cast(ShellExpKind, cast(long, g_panel->GetItemData2( nRow )))
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
''
'' ---- AND IN EXPLORER MODE, WHEN THE ROW IS NOT A FILE -- 7c step 19 ------------------
''
'' The other two panes have exactly one kind of clickable row, so "is it a header" was the
'' whole test. The Explorer has four kinds and THREE OF THEM ARE NOT PLACES: a root group,
'' a folder, and the Save-as-Project prompt. None of them is a header -- the group is a
'' plain parent on purpose -- so IsHeader lets all three through, and slot 1 would then be
'' read as a file index. A folder index of 2 would have opened whatever g_panelFiles(2)
'' happened to be: a real file, silently the wrong one.
'' ---------------------------------------------------------------------------------------
function ShellPanel_GotoRow( byval nRow as long ) as boolean
    if g_panel = 0 then return false
    if (nRow < 0) orelse (nRow >= g_panel->GetCount()) then return false
    '' A HEADER IS A FILENAME, NOT A PLACE. Clicking one collapses it; jumping somewhere
    '' would fight that.
    if g_panel->IsHeader( nRow ) then return false
    if g_panelMode = SHPANEL_EXPLORER then
        if ShellPanel_KindOf( nRow ) <> EXPKIND_FILE then return false
    end if

    dim as DWSTRING wszPath = ShellPanel_PathOf( nRow )
    dim as long     nLine   = ShellPanel_LineOf( nRow )
    if PsLen( wszPath ) = 0 then return false

    '' ---- RESOLVE THE FILE, AND IT MAY NOT BE OPEN ------------------------------------
    ''
    '' A REAL BRANCH, NOT A FALLTHROUGH. Until step 8 this called ShellTabs_Show with the
    '' row's tab index and read ShellTabs_CurrentDoc afterwards. Show is a SILENT no-op for
    '' an out-of-range index (shelltabs.bi:78), so a row naming a file with no tab would
    '' have jumped to a line in WHATEVER TAB HAPPENED TO BE CURRENT -- the wrong file, no
    '' error, no clue. The Functions pane lists unopened files now, so that row exists.
    ''
    '' tiko does the same thing at the same point: match the document, and fall back to
    '' opening it from disk (frmFunctions.inc:191-204).
    dim as long idxTab = ShellTabs_FindByPath( wszPath )
    if idxTab >= 0 then
        '' ShellTabs_Show is a no-op when the tab is already current, which is the common
        '' case -- and when it is not, it is what points the single view at the right
        '' document before anything below reads or moves a caret in it.
        ShellTabs_Show( idxTab )
    else
        '' OPENS FROM DISK. Note what ShellTabs_Open does NOT do: it sets g_nTabCur itself
        '' (shelltabs.bi:203) without going through ShellTabs_Show, so a Show afterwards
        '' would early-return and the caret work below is this function's to do -- which it
        '' was already doing, for its own reasons.
        idxTab = ShellTabs_Open( wszPath )
        if idxTab < 0 then return false
    end if

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


'' The right-click handler lives at the bottom, with the folder commands it opens a menu
'' for; ShellPanel_Install is near the top and needs its address.
declare sub ShellExplorer_OnContext( byval pList as any ptr, byval nRow as long, _
                                     byval nX as long, byval nY as long, byval ud as any ptr )
declare function ShellExplorer_PaintOverlay( byval pi as PsLtPaintInfo ptr ) as boolean
declare function ShellExplorer_OnBeginEdit( byval pList as any ptr, byval nRow as long, _
                                            byval ud as any ptr ) as boolean
declare function ShellExplorer_OnEndEdit( byval pList as any ptr, byval nRow as long, _
                                          byref sText as DWSTRING, _
                                          byval ud as any ptr ) as boolean
declare function ShellExplorer_RenameFolder( byval nRow as long ) as boolean
'' The action icons, 7c step 24. The overlay chains to the painter and the painter needs the
'' rects, and both live at the bottom with the rest of the icon code.
declare sub ShellExplorer_IconRects( byval nRow as long, byref rcRow as PsRect, _
                                     byref rcAdd as PsRect, byref rcRen as PsRect, _
                                     byref rcDel as PsRect )
declare function ShellExplorer_PaintIcons( byval pi as PsLtPaintInfo ptr ) as boolean
declare function ShellExplorer_OnRowClick( byval pList as any ptr, byval nRow as long, _
                                           byval nX as long, byval nY as long, _
                                           byval ud as any ptr ) as boolean
declare function ShellExplorer_OnDrop( byval pList as any ptr, byval nSrcRow as long, _
                                       byval nTargetRow as long, byval bOnTarget as boolean, _
                                       byval ud as any ptr ) as boolean


'' Wire the panel's callbacks. Called once, beside ShellTabs_Install.
sub ShellPanel_Install()
    if g_panel = 0 then exit sub
    g_panel->OnSelChange( @ShellPanel_OnRowSelected,  0 )
    g_panel->OnActivate(  @ShellPanel_OnRowActivated, 0 )
    '' THE RIGHT BUTTON, new in 7c step 21 and new in PsListTree in the same step. Installed
    '' for every mode; the handler bails unless the pane is the Explorer, so Bookmarks and
    '' Functions are unaffected and get PsListTree's fixed right-click behaviour for free.
    g_panel->OnContextMenu( @ShellExplorer_OnContext, 0 )
    '' THE ROW GLYPH, new in 7c step 22 on the hook step 21 added. Installed for every mode
    '' and drawn in none but the Explorer -- the handler bails on the mode, so Bookmarks and
    '' Functions keep the plain rows they have always had.
    g_panel->OnPaintOverlay( @ShellExplorer_PaintOverlay )
    '' LABEL EDITING, new in 7c step 23 and new in PsListTree in the same step. Installed
    '' for every mode; the begin callback answers "is this a folder row", which is false in
    '' Bookmarks and Functions, so neither pane becomes editable.
    g_panel->OnBeginEdit( @ShellExplorer_OnBeginEdit, 0 )
    g_panel->OnEndEdit( @ShellExplorer_OnEndEdit, 0 )
    '' THE LEFT CLICK, new in 7c step 24 and new in PsListTree in the same step. It claims
    '' ONLY an action icon on the hot Explorer row and declines everything else, so
    '' selection, folding and the other two panes are untouched.
    g_panel->OnRowClick( @ShellExplorer_OnRowClick, 0 )
    '' DRAG AND DROP, new in 7c step 25. The handler returns FALSE on every path -- it
    '' either moved something in gProjectFolders or refused to, and in neither case may the
    '' tree reorder rows that are a VIEW of that table. SetDragReorder is what turns the
    '' gesture on at all, and it is on for the Explorer only.
    g_panel->OnDrop( @ShellExplorer_OnDrop, 0 )
    '' AND THE GESTURE ITSELF, matched to the mode the panel starts in.
    ''
    '' ANOTHER EARLY-OUT: ShellPanel_SetMode sets this on every switch, so removing this
    '' line leaves the suite green -- the first switch into the Explorer repairs it. Kept
    '' for the pane the binary OPENS on, which is the one case SetMode never sees, and
    '' labelled so it does not read as the place the rule lives.
    g_panel->SetDragReorder( g_panelMode = SHPANEL_EXPLORER )
    '' NO ZEBRA STRIPES. Reported by the author -- "the bookmark rows colored weird". The
    '' rows here are a file's bookmarks, not records, and tiko's own panel paints every row
    '' the same. Set ONCE, at install: SetAltRows is a flag the control keeps, unlike the
    '' clrRowAlt = clrBack flatten this replaces, which OnThemeChanged undid on every theme
    '' load and had to be reapplied after each one.
    g_panel->SetAltRows( false )
end sub


'' BOTH loaders are forward-declared, not just the functions one. ShellPanel_Reload
'' dispatches to a pair defined below it, and a declaration for one of two is how you get
'' "Variable not declared, ShellBookmarks_Load" pointing at a sub that is plainly there
'' three hundred lines further down.
declare sub ShellBookmarks_Load()
declare sub ShellFunctions_Load()
declare sub ShellExplorer_Load()


'' Rebuild whatever the panel is currently showing. THE ONE ENTRY POINT anything outside
'' this file should use -- a caller that picked a loader directly would show the wrong list
'' the moment the mode changed under it.
sub ShellPanel_Reload()
    select case g_panelMode
        case SHPANEL_FUNCTIONS : ShellFunctions_Load()
        case SHPANEL_EXPLORER  : ShellExplorer_Load()
        case else              : ShellBookmarks_Load()
    end select
end sub


'' Switch panes. Silent when the mode has not changed -- reloading a list the user is
'' already looking at would lose their scroll position for nothing.
'' ---------------------------------------------------------------------------------------
'' THE STRIP FOLLOWS THE MODE, NOT THE CLICK, and that distinction is the whole of it.
''
'' Driving SelectExclusive from the click handler would light the right button whenever the
'' user pressed one and leave it STALE whenever the pane changed any other way -- the View
'' menu, an accelerator, or ShellPanel_Reload deciding for itself. There are already three
'' such paths and step 19 added a fourth.
''
'' So the mode is the single source of truth and this is called from ShellPanel_SetMode.
'' FindItemByID rather than a positional index: the items' ORDER is a layout decision and
'' nothing here should depend on it.
'' ---------------------------------------------------------------------------------------
sub ShellPanelMenu_SyncToMode( byval m as ShellPanelMode )
    if g_panelMenu = 0 then exit sub
    dim as long id = IDM_FUNCTIONLIST
    select case m
        case SHPANEL_EXPLORER  : id = IDM_VIEWEXPLORER
        case SHPANEL_BOOKMARKS : id = IDM_BOOKMARKSLIST
    end select
    dim as long idx = g_panelMenu->FindItemByID( id )
    if idx >= 0 then g_panelMenu->SelectExclusive( idx )
end sub


sub ShellPanel_SetMode( byval m as ShellPanelMode )
    if m = g_panelMode then exit sub
    g_panelMode = m
    ShellPanelMenu_SyncToMode( m )
    '' ---- DRAGGING IS THE EXPLORER'S ALONE, 7c step 25 ---------------------------------
    '' A bookmark's position in its file and a procedure's position in the symbol database
    '' are FACTS, not preferences -- dragging one somewhere else would either be undone by
    '' the next reload or, worse, look like it had worked. Only the Explorer has an order
    '' that means anything a user can change, and even there the change is to
    '' gProjectFolders rather than to the rows.
    ''
    '' Set on the MODE rather than once at install, because the answer differs per pane.
    if g_panel <> 0 then g_panel->SetDragReorder( m = SHPANEL_EXPLORER )
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

    ShellPanel_ResetFiles()
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
            dim as long idxFile = ShellPanel_AddFile( pDoc->DiskFilename )
            dim as DWSTRING wszText = PsPathName( pDoc->DiskFilename )
            g_panel->AddHeader( wszText, idxFile, 0 )

            dim as long nCount = PsStrParseCount( sBookmarks, "," )
            for i as long = 1 to nCount
                dim as long nLineNum = PsVal( PsStrParse( sBookmarks, i, "," ) )
                '' ltrim, like tiko: the line's indentation is not information in a list
                '' this narrow, and every row would start at a different column.
                dim as string sDescription = ltrim( pDoc->GetLine(nLineNum) )
                wszText.Utf8 = sDescription
                g_panel->AddString( wszText, idxFile, ShellPanel_ClampLine(nLineNum) )
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
'' 1. THE FILE LIST IS THE WHOLE DATABASE, since 7c step 8. It used to be the open tabs,
''    and on a real project that meant the pane showed ONE heading -- the author opened
''    src\tiko.bas, whose 4,496 symbols live in 133 files it #includes, and got an empty
''    pane. tiko walks gSymDb.EnumUserFiles and then keeps whatever the EXPLORER is
''    displaying (frmFunctions_BuildFileList, frmFunctions.inc:340); there is no Explorer
''    here, so the filter has nothing to consult and the list is UNFILTERED -- which makes
''    it larger than tiko's, not merely different.
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


'' Insertion sort over the DISPLAY names, which is what the user reads -- tiko sorts the
'' same array with a quicksort (QuickSortFilenames, frmFunctions.inc:284) that lives in its
'' Win32 half and is not reachable from here. A hundred-odd files is nothing to insert.
private sub ShellFunctions_SortFiles( files() as DWSTRING, byval nCount as long )
    for i as long = 1 to nCount - 1
        dim as DWSTRING tmp = files(i)
        dim as DWSTRING keyT = PsUCase( PsPathName( tmp ) )
        dim as long j = i - 1
        do while (j >= 0) andalso (PsUCase( PsPathName( files(j) ) ) > keyT)
            files(j + 1) = files(j)
            j -= 1
        loop
        files(j + 1) = tmp
    next
end sub


sub ShellFunctions_Load()
    if g_panel = 0 then exit sub

    dim as long nCurSel   = g_panel->GetCurSel()
    dim as long nTopIndex = g_panel->GetTopIndex()

    ShellPanel_ResetFiles()
    g_panel->clear()
    g_panel->BeginUpdate()

    '' ---- THE FILE LIST, WHICH IS THE WHOLE POINT OF THIS COMMIT ------------------------
    ''
    '' EnumUserFiles answers for BOTH TIERS and honours the merge suppression, so the active
    '' file's live-as-you-type symbols and every other file's last-scanned ones arrive in one
    '' list with nothing counted twice. It has always been able to do this; the pane simply
    '' never asked.
    ''
    '' EVERY NAME GOES THROUGH FilenameOriginalCase. They come straight out of the parser's
    '' string pool, which holds them UPPERCASED -- so without this the headings read
    '' TIKO.BAS and CLSDOCUMENT.BI. tiko normalises at the same point, for the same reason
    '' (frmFunctions.inc:346), and the shell could not until step 8 made that function real.
    redim as DWSTRING files( 0 to 63 )
    dim as long nFiles = gSymDb.EnumUserFiles( files() )
    for i as long = 0 to nFiles - 1
        files(i) = FilenameOriginalCase( files(i) )
    next
    ShellFunctions_SortFiles( files(), nFiles )

    '' A HEADER PER FILE, procedures beneath it, sorted by display name. Files with no
    '' procedures are skipped rather than shown empty -- an #include of nothing but constants
    '' is most of a real project's file list and a heading per one would bury the code.
    for i as long = 0 to nFiles - 1
        dim as DWSTRING wszFile = files(i)
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

        '' REGISTERED ONLY ONCE IT HAS ROWS, so the table holds no entry that no row points
        '' at. NOTHING OBSERVABLE DEPENDS ON THIS and the suite says so: registering early
        '' instead fails no assertion, because the table is only ever read THROUGH a row's
        '' slot and an unreferenced entry is invisible. It is here because a file list with
        '' junk in it is a file list the next reader has to reason about, not because it
        '' fixes a defect -- which is a weaker claim than the first draft of this comment
        '' made, and the revert is what corrected it.
        dim as long idxFile = ShellPanel_AddFile( wszFile )
        if idxFile < 0 then exit for

        g_panel->AddHeader( PsPathName( wszFile ), idxFile, 0 )
        for r as long = 0 to nKeep - 1
            g_panel->AddString( rows(r).sName, idxFile, ShellPanel_ClampLine(rows(r).nLine) )
        next
    next

    g_panel->EndUpdate()

    g_panel->SetTopIndex( nTopIndex )
    nCurSel = iif( nCurSel > g_panel->GetCount() - 1, g_panel->GetCount() - 1, nCurSel )
    g_panel->SetCurSel( nCurSel )

    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' =======================================================================================
'' THE EXPLORER PANE -- 7c step 19. A port of frmExplorer.inc's LoadExplorerFiles (1208)
'' and frmExplorer_AddFolderLevel (1136), which between them are the whole read-only half
'' of tiko's largest form.
''
'' ---- WHAT PORTED WITH NO CHANGE AT ALL, WHICH IS THE MODEL ---------------------------
''
'' gProjectFolders, ProjectFolders_ParentPath, ProjectFolders_LeafName,
'' ProjectFolders_Exists, gConfig.Cat, gApp.pDocList, gApp.IsProjectNamed,
'' clsDocument.docData.wszFolder and ProjectFiletype are all in app/ already. Not one of
'' them needed a line. The port is the CONTROL half and the row scheme.
''
'' ---- AND WHAT DID NOT: THE ROW CARRIES AN INDEX -------------------------------------
''
'' tiko puts a clsDocument POINTER in a file row. This puts a g_panelFiles index, for the
'' reason the two existing loaders already give -- and it costs nothing, because
'' ShellPanel_GotoRow resolves an index to a path and opens the file WHETHER OR NOT it is
'' currently in a tab. That was written for the Functions pane in step 8 and is exactly
'' what an Explorer click needs.
'' =======================================================================================


'' The gConfig.Cat() index a document is displayed under, or -1. tiko's
'' frmExplorer_CatIndexForDoc, unchanged.
private function ShellExplorer_CatIndexForDoc( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then return -1
    dim as DWSTRING wszFileType = pDoc->ProjectFiletype
    for ii as long = CATINDEX_MAIN to ubound(gConfig.Cat)
        if wszFileType = gConfig.Cat(ii).idFileType then return ii
    next
    return -1
end function


'' ---------------------------------------------------------------------------------------
'' Forget any document folder that does not name a folder that actually exists.
''
'' tiko's frmExplorer_NormalizeDocFolders, and its reasoning transfers whole: the tree is
'' built by walking folders and asking which files sit in each, so a file naming a folder
'' that is not in the table is never asked for by anybody. It disappears from the pane while
'' remaining in the workspace.
''
'' Run from the LOADER rather than from the project loader, so every path that can
'' invalidate a folder is covered without each having to remember.
'' ---------------------------------------------------------------------------------------
private sub ShellExplorer_NormalizeDocFolders()
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if PsLen( pDoc->docData.wszFolder ) then
            dim as long catIndex = ShellExplorer_CatIndexForDoc( pDoc )
            if ProjectFolders_Exists( catIndex, pDoc->docData.wszFolder ) = false then
                pDoc->docData.wszFolder = ""
            end if
        end if
        pDoc = pDoc->pDocNext
    loop
end sub


'' Insertion sort by DiskFilename, ascending, ASCII-folded. tiko uses a quicksort here; a
'' folder level holds the files of one directory of one project, so the sort's cost is
'' irrelevant and its simplicity is not -- the same call the folder half already makes.
private sub ShellExplorer_SortDocs( pDocs() as clsDocument ptr, byval nCount as long )
    for i as long = 1 to nCount - 1
        dim as clsDocument ptr pHold = pDocs(i)
        dim as DWSTRING wszHold = PsUCase( pHold->DiskFilename )
        dim as long j = i - 1
        do while j >= 0
            if PsUCase( pDocs(j)->DiskFilename ) <= wszHold then exit do
            pDocs(j + 1) = pDocs(j)
            j -= 1
        loop
        pDocs(j + 1) = pHold
    next
end sub


'' ---------------------------------------------------------------------------------------
'' Emit one level of a group's tree beneath nParentRow: the folders directly inside
'' wszParentPath, each recursed into, and then the files that sit at this level.
''
'' FOLDERS BEFORE FILES, each half alphabetical -- tiko's order, derived on every load
'' rather than persisted, because drag/drop chooses which folder a file lands in and never
'' where it sits among its siblings.
''
'' AddNode, NOT AddString, for the files. AddString derives its level from the PRECEDING
'' row, so with the group a plain parent it would put every file at level 0 -- a flat list
'' with no subtree to collapse, unable to express a nested folder at all. tiko's comment
'' says the same thing about its own control, and PsListTree behaves the same way.
''
'' Recursion depth is the folder depth the user created, which is unbounded by design.
'' ---------------------------------------------------------------------------------------
private sub ShellExplorer_AddFolderLevel( byval catIndex as long, _
                                          byval wszParentPath as DWSTRING, _
                                          byval nParentRow as long )
    if g_panel = 0 then exit sub

    '' ---- the folders directly inside this level ---------------------------------------
    redim nFolders(any) as long
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        if gProjectFolders(i).catIndex <> catIndex then continue for
        if PsUCase( ProjectFolders_ParentPath( gProjectFolders(i).wszPath ) ) <> _
           PsUCase( wszParentPath ) then continue for
        dim as long ub = ubound(nFolders) + 1
        redim preserve nFolders(ub)
        nFolders(ub) = i
    next

    for i as long = lbound(nFolders) + 1 to ubound(nFolders)
        dim as long nHold = nFolders(i)
        dim as DWSTRING wszHold = PsUCase( ProjectFolders_LeafName( gProjectFolders(nHold).wszPath ) )
        dim as long j = i - 1
        do while j >= lbound(nFolders)
            if PsUCase( ProjectFolders_LeafName( gProjectFolders(nFolders(j)).wszPath ) ) <= wszHold then exit do
            nFolders(j + 1) = nFolders(j)
            j -= 1
        loop
        nFolders(j + 1) = nHold
    next

    for i as long = lbound(nFolders) to ubound(nFolders)
        dim as long nIdx = nFolders(i)
        '' The row shows the LEAF name; the full path is recovered from the table through
        '' the slot-1 index, so nothing has to parse a caption back into a path.
        dim as long nRow = g_panel->AddNode( nParentRow, _
                               ProjectFolders_LeafName( gProjectFolders(nIdx).wszPath ), _
                               nIdx, EXPKIND_FOLDER )
        ShellExplorer_AddFolderLevel( catIndex, gProjectFolders(nIdx).wszPath, nRow )
    next

    '' ---- the files at this level ------------------------------------------------------
    redim pDocs(any) as clsDocument ptr
    dim as long nDocs = 0
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if pDoc->ProjectFiletype = gConfig.Cat(catIndex).idFileType then
            if PsUCase( pDoc->docData.wszFolder ) = PsUCase( wszParentPath ) then
                redim preserve pDocs(0 to nDocs)
                pDocs(nDocs) = pDoc
                nDocs += 1
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    ShellExplorer_SortDocs( pDocs(), nDocs )

    for i as long = 0 to nDocs - 1
        '' REGISTERED IN THE FILE TABLE, which is what makes the row clickable: GotoRow
        '' reads slot 1 as an index into it and opens the path whether or not it is tabbed.
        dim as long nFile = ShellPanel_AddFile( pDocs(i)->DiskFilename )
        if nFile < 0 then exit for
        g_panel->AddNode( nParentRow, PsPathName( pDocs(i)->DiskFilename ), _
                          nFile, EXPKIND_FILE )
    next
end sub


'' ---------------------------------------------------------------------------------------
'' Rebuild the Explorer pane. tiko's LoadExplorerFiles.
''
'' WHAT DID NOT COME ACROSS: the hide/show dance around the load. tiko hides the control
'' while it fills so the empty listbox does not flash white, and its own comment is mostly
'' about the ways that goes wrong -- a control left hidden, a visibility read that walks the
'' ancestor chain. BeginUpdate/EndUpdate is what PsListTree offers for the same purpose and
'' it suppresses the REPAINT rather than the window, so there is no state to restore and no
'' way to leave the pane hidden.
''
'' CATINDEX_FILES IS SKIPPED, as in tiko: it stays in the Cat() array -- two menu paths
'' index that array by an offset from a command id, and the table is persisted in
'' settings.ini besides -- and nothing is ever displayed under it.
'' ---------------------------------------------------------------------------------------
sub ShellExplorer_Load()
    if g_panel = 0 then exit sub

    dim as long nCurSel   = g_panel->GetCurSel()
    dim as long nTopIndex = g_panel->GetTopIndex()

    ShellPanel_ResetFiles()
    g_panel->clear()
    g_panel->BeginUpdate()

    '' While the workspace is untitled, offer to make it a real project. Pinned at index 0,
    '' above every group, and present only until the user names it.
    ''
    '' THE ROW IS INERT HERE, and deliberately: EXPKIND_PROMOTE has no handler in this
    '' binary because Save As is not ported. It is emitted so the pane's SHAPE matches
    '' tiko's -- an assertion that counts rows would otherwise disagree for a reason that
    '' has nothing to do with the tree.
    if gApp.IsProjectNamed() = false then
        g_panel->AddString( L(398,"Save as Project..."), 0, EXPKIND_PROMOTE )
    end if

    '' Before any group row is emitted: a file whose folder cannot be resolved must be
    '' pulled back to its group's top level, or the walk below never asks for it.
    ShellExplorer_NormalizeDocFolders()

    for ii as long = CATINDEX_MAIN to ubound(gConfig.Cat)
        '' A ROOT GROUP IS A PARENT, NOT A HEADER, and tiko's reason holds for PsListTree
        '' too: a header toggles on a click ANYWHERE in it, and does so before the host is
        '' offered the event -- so the host cannot own the gesture. As a plain parent
        '' nothing toggles by itself.
        ''
        '' NON-SELECTABLE for the second half of it: a root group must never be draggable,
        '' and drag eligibility is computed from selectability. Headers get that exclusion
        '' for free; plain parents do not. Nothing here depends on selecting a group row.
        dim as long nRootRow = g_panel->AddNode( -1, gConfig.Cat(ii).wszDescription, _
                                                 ii, EXPKIND_ROOT )
        g_panel->SetRowSelectable( nRootRow, false )

        '' The group's whole subtree, from its top level (""). A group that allows no
        '' folders simply has none in the table, so Main and Resource need no special case.
        ShellExplorer_AddFolderLevel( ii, "", nRootRow )
    next

    g_panel->EndUpdate()

    '' Restore the view as it was found. Clamped, because a reload can shorten the tree --
    '' a folder deleted, a file closed -- and SetCurSel past the end is a silent no-op that
    '' would leave the OLD selection highlighted on a row that now means something else.
    g_panel->SetTopIndex( nTopIndex )
    if nCurSel > g_panel->GetCount() - 1 then nCurSel = g_panel->GetCount() - 1
    g_panel->SetCurSel( nCurSel )

    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' ---------------------------------------------------------------------------------------
'' SELECT THE ROW FOR A PATH, and scroll it into view. tiko's frmExplorer_SelectItemData
'' (frmExplorer.inc:89), with its document pointer replaced by a path -- the row carries an
'' index into g_panelFiles, so the comparison is on what the index resolves to.
''
'' RETURNS FALSE WHEN THE ROW IS NOT VISIBLE, which is tiko's intent and looks like a
'' refusal until you see the alternative: SetCurSel on a row inside a collapsed subtree
'' selects something the user cannot see, and the pane then scrolls to a blank place.
'' Expanding the tree under them to satisfy a tab switch is worse.
''
'' ---- AND tiko's OWN GUARD NEVER FIRES, WHICH IS WHY THIS ONE IS NOT A COPY ------------
''
'' frmExplorer.inc:109 reads `if PsListTree_IsCollapsed(hCtrl, i) then return false`, and
'' IsCollapsed asks whether ROW i IS ITSELF COLLAPSED -- whether its own subtree is folded
'' shut. A FILE ROW IS A LEAF. It has no subtree, so it is never collapsed, so that test is
'' false for every row it is ever applied to and the guard has never refused anything in
'' either binary.
''
'' The question actually being asked is "can the user see this row", and the answer is the
'' VISIBLE MAP: ModelToVisible returns -1 for a row hidden under a folded ancestor, at any
'' depth. Ported as IsCollapsed first, and an assertion written for the ported behaviour is
'' what said so -- collapsing the group left the guard entirely unmoved.
''
'' EnsureVisible RATHER THAN tiko's top-index arithmetic. tiko computes ItemsPerPage, tests
'' whether the row is in range and otherwise sets the top index to i-8 so the selection
'' lands a little down the list. PsListTree has EnsureVisible, which is the same intent
'' expressed once; the eight-row lead is the part that does not survive, and it is a
'' cosmetic difference rather than a behavioural one.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_SelectPath( byval wszPath as DWSTRING ) as boolean
    if g_panel = 0 then return false
    if g_panelMode <> SHPANEL_EXPLORER then return false
    if PsLen( wszPath ) = 0 then return false

    '' Already there. Checked first, as tiko does, so a tab switch onto the file the pane
    '' is already showing does not scroll the pane.
    ''
    '' AND STILL VISIBLE, which tiko's version does not ask. The selection survives a
    '' collapse -- it is a model index and the fold does not clear it -- so without this the
    '' early-out reports "yes, it is selected" for a row that is no longer on screen, and
    '' the visibility test below never runs. Found by asserting the refusal: collapsing the
    '' group left SelectPath returning true, and the guard it was meant to reach was three
    '' lines further down and unreachable.
    dim as long nCurSel = g_panel->GetCurSel()
    if (nCurSel >= 0) andalso (g_panel->ModelToVisible( nCurSel ) >= 0) then
        if ShellPanel_KindOf( nCurSel ) = EXPKIND_FILE then
            if PsUCase( ShellPanel_PathOf( nCurSel ) ) = PsUCase( wszPath ) then return true
        end if
    end if

    dim as long nCount = g_panel->GetCount()
    for i as long = 0 to nCount - 1
        if ShellPanel_KindOf( i ) <> EXPKIND_FILE then continue for
        if PsUCase( ShellPanel_PathOf( i ) ) <> PsUCase( wszPath ) then continue for
        if g_panel->ModelToVisible( i ) < 0 then return false
        g_panel->EnsureVisible( i )
        g_panel->SetCurSel( i )
        if g_pSurf <> 0 then g_pSurf->InvalidateAll()
        return true
    next

    return false
end function


'' ---------------------------------------------------------------------------------------
'' Does this file appear in the Explorer? tiko's frmExplorer_IsFileDisplayed, and its
'' header applies here word for word: this is the ONE place the display rule is stated, so
'' anything wanting to mirror it cannot drift from what the loader actually renders.
''
'' Two conditions, both imposed by the loader's own loop: the file must be a document in the
'' workspace at all, and its ProjectFiletype must name one of the categories that loop walks
'' -- CATINDEX_FILES is deliberately outside the range, so a document under it is displayed
'' nowhere.
''
'' READS THE MODEL, NOT THE CONTROL, which is why it answers correctly with the pane showing
'' Functions or Bookmarks. A version that walked the rows would answer "no" for every file
'' whenever the pane happened to be on another mode.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_IsFileDisplayed( byval wszFilename as DWSTRING ) as boolean
    if PsLen( wszFilename ) = 0 then return false
    dim pDoc as clsDocument ptr = gApp.GetDocumentPtrByFilename( wszFilename )
    if pDoc = 0 then return false
    return ( ShellExplorer_CatIndexForDoc( pDoc ) >= 0 )
end function


'' =======================================================================================
'' THE EXPLORER'S FOLDER COMMANDS -- 7c step 21.
''
'' The model half is entirely app/modProjectFolders.inc, which has been linked into this
'' binary since step 19. What arrived in step 21 is PsListTree reporting a right-click at
'' all -- it used to select the row, arm a drag and swallow the event.
''
'' ---- WHAT DID NOT COME ACROSS: BeginEdit ----------------------------------------------
''
'' tiko's NewFolder ends by opening the in-place editor on the row it just made, so the
'' user names the folder immediately. PsListTree has no editor yet -- that is PsTextBox's
'' to lend and step 22's to wire -- so this stops one line earlier. The unique-name loop
'' below is tiko's OWN, and it already produces "New Folder", "New Folder (2)" and so on,
'' which is a usable name rather than a placeholder. RENAME IS ABSENT FROM THE MENU rather
'' than greyed: a disabled item promises a feature.
'' =======================================================================================

'' The folder path a row denotes, and the category it lives under. tiko's
'' frmExplorer_FolderPathFromRow, with the pointer read replaced by the slot-1 index.
'' A ROOT GROUP RETURNS "" WITH A VALID catIndex -- that is the group's top level, not a
'' failure, and it is what lets New Folder work on a group row.
private function ShellExplorer_FolderPathFromRow( byval nRow as long, _
                                                  byref catIndex as long ) as DWSTRING
    catIndex = -1
    if g_panel = 0 then return DWSTRING()
    select case ShellPanel_KindOf( nRow )
        case EXPKIND_ROOT
            catIndex = cast(long, g_panel->GetItemData( nRow ))
            return DWSTRING()
        case EXPKIND_FOLDER
            dim as long idx = cast(long, g_panel->GetItemData( nRow ))
            if idx < lbound(gProjectFolders) then return DWSTRING()
            if idx > ubound(gProjectFolders) then return DWSTRING()
            catIndex = gProjectFolders(idx).catIndex
            return gProjectFolders(idx).wszPath
    end select
    return DWSTRING()
end function


'' ---------------------------------------------------------------------------------------
'' Create a folder under the row's level. Returns its table index, or -1.
''
'' RETURNS THE INDEX rather than nothing, because the suite has no mouse and the only way
'' to assert "the right folder was made in the right place" is to be handed it.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_NewFolder( byval nRow as long ) as long
    dim as long catIndex
    dim as DWSTRING wszParent = ShellExplorer_FolderPathFromRow( nRow, catIndex )
    if catIndex < 0 then return -1
    '' ANOTHER EARLY-OUT THAT IS NOT THE GUARD. ProjectFolders_Add tests the same thing on
    '' its first line (modProjectFolders.inc:225), so removing this leaves the suite green
    '' and the behaviour correct. Kept because the menu asks the same question before
    '' OFFERING the command and the two must agree, and labelled so that nobody reads the
    '' shell layer as the place the rule lives. IT LIVES IN THE MODEL, which was written
    '' defensively long before any of this had a caller -- and that, rather than anything
    '' added in this step, is what actually protects Main and Resource.
    if ProjectFolders_CatAllowsFolders( catIndex ) = false then return -1

    '' tiko's uniquifier, unchanged. It matters more here than it does there: tiko opens an
    '' editor straight afterwards so the generated name is a two-second placeholder, and
    '' this binary has no editor, so the name it picks is the name that stays.
    dim as DWSTRING wszBase = L(462, "New Folder")
    dim as DWSTRING wszName = wszBase
    dim as long nTry = 2
    do while ProjectFolders_Exists( catIndex, ProjectFolders_Combine( wszParent, wszName ) )
        wszName = wszBase & " (" & str(nTry) & ")"
        nTry += 1
    loop

    dim as DWSTRING wszNew = ProjectFolders_Combine( wszParent, wszName )
    if ProjectFolders_Add( catIndex, wszNew ) < 0 then return -1

    '' The container has to be OPEN or the new row lands inside a collapsed subtree and the
    '' user is told nothing happened. tiko expands for the same reason, one line before
    '' opening its editor.
    if g_panel <> 0 then
        if g_panel->IsCollapsed( nRow ) then g_panel->ExpandRow( nRow )
    end if

    ShellExplorer_Load()

    '' The rebuild invalidated every row index, so the new row is found by WHAT IT IS.
    dim as long nIdx = ProjectFolders_Find( catIndex, wszNew )
    if (g_panel <> 0) andalso (nIdx >= 0) then
        for i as long = 0 to g_panel->GetCount() - 1
            if ShellPanel_KindOf(i) <> EXPKIND_FOLDER then continue for
            if cast(long, g_panel->GetItemData(i)) = nIdx then
                '' ---- AND THE TAIL tiko HAS, RESTORED IN STEP 23 ------------------------
                '' tiko ends NewFolder by opening the editor on the row it just made, so the
                '' user names the folder immediately. Step 21 stopped one line short because
                '' PsListTree had no editor; this is the line. The generated name is a
                '' placeholder again rather than the name that stays.
                g_panel->SetCurSel( i )
                g_panel->BeginEdit( i )
                exit for
            end if
        next
    end if
    return nIdx
end function


'' ---------------------------------------------------------------------------------------
'' Delete the folder this row denotes. Its contents move UP one level.
''
'' tiko's frmExplorer_DeleteFolder, ported whole, and its header is worth repeating because
'' it explains the absence of a confirmation: NOTHING IS DESTROYED. Child files and child
'' folders re-attach to the deleted folder's parent, which is a dissolve rather than a
'' delete. Root groups are unreachable -- the menu offers this on a folder row only -- which
'' is what keeps the five categories permanent.
''
'' The path is resolved BEFORE the table is touched: ProjectFolders_RemoveAt compacts, so a
'' row's stored folder index is stale the instant anything is removed.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_DeleteFolder( byval nRow as long ) as boolean
    '' A CHEAP EARLY-OUT, AND IT IS NOT THE GUARD. Removing this line leaves the suite
    '' green, because ShellExplorer_FolderPathFromRow answers catIndex = -1 for every kind
    '' it does not handle -- a file row, the Save-as-Project prompt, an invalid index -- and
    '' the test below catches all of them. Kept for the reader and for the cost, and
    '' labelled so that nobody narrowing FolderPathFromRow later assumes this covers them.
    if ShellPanel_KindOf( nRow ) <> EXPKIND_FOLDER then return false

    dim as long catIndex
    dim as DWSTRING wszPath = ShellExplorer_FolderPathFromRow( nRow, catIndex )
    if catIndex < 0 then return false
    if PsLen( wszPath ) = 0 then return false

    '' Dissolving IS rebasing onto the parent: descendants lose one level, documents follow,
    '' and the folder's own record maps onto its parent and is dropped as a duplicate. ONE
    '' implementation shared with rename rather than a second path walk that could disagree.
    ProjectFolders_Rebase( catIndex, wszPath, ProjectFolders_ParentPath( wszPath ), _
                           gApp.pDocList, gConfig.Cat(catIndex).idFileType )

    ShellExplorer_Load()
    return true
end function


'' ---------------------------------------------------------------------------------------
'' THE ROW THE MENU WILL ACT ON, remembered rather than re-derived when it closes.
''
'' tiko's gExpMenuRow, and its reason ports word for word: the popup is NOT modal, so the
'' command callback runs LATER -- and every one of these commands rebuilds the tree, so an
'' index resolved at that point would be meaningless. Cleared on the way in, so a second
'' delivery cannot act twice.
'' ---------------------------------------------------------------------------------------
dim shared as long g_expMenuRow = -1


'' ---------------------------------------------------------------------------------------
'' The Explorer's right-click. Installed as PsListTree's context callback, which did not
'' exist until 7c step 21 -- before that the control selected the row, armed a drag and
'' swallowed the event, so this could not be written at all.
''
'' NO MENU ON A FILE OR ON EMPTY SPACE. tiko offers a different menu for a document row
'' (close, save, remove from project) and this binary has none of those commands, so an
'' empty popup would be worse than none. A group or a folder gets the folder menu; nRow = -1
'' -- below the last row -- gets nothing, which is narrower than tiko and honest.
''
'' RENAME IS NOT OFFERED. PsListTree has no in-place editor yet; a greyed item promises a
'' feature and an absent one does not.
'' ---------------------------------------------------------------------------------------
sub ShellExplorer_OnContext( byval pList as any ptr, byval nRow as long, _
                             byval nX as long, byval nY as long, byval ud as any ptr )
    if g_panel = 0 then exit sub
    if g_pSurf = 0 then exit sub
    if g_panelMode <> SHPANEL_EXPLORER then exit sub

    dim as ShellExpKind kind = ShellPanel_KindOf( nRow )
    if (kind <> EXPKIND_ROOT) andalso (kind <> EXPKIND_FOLDER) then exit sub

    '' A category that allows no folders offers no folder commands -- Main and Resource hold
    '' one file each by construction. ProjectFolders_CatAllowsFolders is the one place that
    '' rule lives, so asking it here cannot drift from what NewFolder will do.
    dim as long catIndex
    ShellExplorer_FolderPathFromRow( nRow, catIndex )
    if catIndex < 0 then exit sub
    if ProjectFolders_CatAllowsFolders( catIndex ) = false then exit sub

    g_expMenuRow = nRow

    dim as PsPopupMenu ptr pM = new PsPopupMenu
    if pM = 0 then exit sub
    pM->AddItem( L(462,"New Folder"), IDM_EXPLORER_NEWFOLDER )
    if kind = EXPKIND_FOLDER then
        '' RENAME IS BACK, since step 23. It was absent rather than greyed while
        '' PsListTree had no editor -- a disabled item promises a feature.
        pM->AddItem( L(464,"Rename Folder"), IDM_EXPLORER_RENAMEFOLDER )
        pM->AddSeparator()
        pM->AddItem( L(463,"Delete Folder"), IDM_EXPLORER_DELETEFOLDER )
    end if

    '' ---- WIDGET COORDINATES TO SURFACE COORDINATES ------------------------------------
    '' The callback reports where the click was IN THE LIST; OpenRoot anchors in the
    '' surface. Adding the panel's origin is the whole conversion, and getting it wrong puts
    '' the menu in the top-left corner of the window rather than under the pointer -- which
    '' looks like the menu opening on the wrong thing rather than in the wrong place.
    dim as PsRect rc
    rc.x = g_panel->bounds.x + nX
    rc.y = g_panel->bounds.y + nY
    rc.w = 1 : rc.h = 1
    g_menus.OpenRoot( g_pSurf, rc, pM )
end sub


'' ---------------------------------------------------------------------------------------
'' The folder commands, dispatched from OnMenuCommand once the menu has closed.
''
'' TRUE when the id was one of ours, so the caller can fall through for everything else.
'' Reads g_expMenuRow rather than the live selection, and clears it first -- see the note on
'' the variable.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_FolderCommand( byval nId as long ) as boolean
    select case nId
        case IDM_EXPLORER_NEWFOLDER, IDM_EXPLORER_DELETEFOLDER, IDM_EXPLORER_RENAMEFOLDER
        case else : return false
    end select

    dim as long nRow = g_expMenuRow
    g_expMenuRow = -1
    if nRow < 0 then return true          '' ours, and there is nothing to do with it

    select case nId
        case IDM_EXPLORER_NEWFOLDER    : ShellExplorer_NewFolder( nRow )
        case IDM_EXPLORER_DELETEFOLDER : ShellExplorer_DeleteFolder( nRow )
        case IDM_EXPLORER_RENAMEFOLDER : ShellExplorer_RenameFolder( nRow )
    end select
    return true
end function


'' =======================================================================================
'' THE EXPLORER'S ROW GLYPH -- 7c step 22, on the overlay hook step 21 added to PsListTree.
''
'' ---- WHAT tiko DRAWS, AND WHY THIS DRAWS LESS ----------------------------------------
''
'' frmExplorer_PaintCallback replaces the row wholesale and paints THREE things in the icon
'' column: a chevron for a container, a dot for a file, and the caption. It has to paint the
'' chevron itself because tiko deliberately leaves SetTreeIndent OFF and does its own indent
'' arithmetic (frmExplorer.inc:405).
''
'' THIS PANEL HAS SetTreeIndent AND ShowTwisty ON, so PsListTree already draws the chevron
'' and already reserves the band it sits in. What is left is the ONE thing the control has
'' no notion of: a file row's document glyph. So the overlay draws a single character, on
'' one kind of row, and everything else on screen is the control's.
''
'' ---- U+00B7 IS NOT A PRIVATE-USE CODEPOINT, AND THAT IS THE POINT --------------------
''
'' tiko's own comment on wszIconDocument (modDeclares.bi:332) says "use the regular Segoe UI
'' font for this one" -- it is MIDDLE DOT, present in every text font, not a Segoe MDL2 glyph
'' from the private-use area. So unlike the pane switcher's icons this needs nothing from the
'' font fallback chain and cannot come out as a box.
'' =======================================================================================

'' The glyph a row kind shows, or "" for a row that shows none.
''
'' A PURE FUNCTION OF THE KIND, and split out for the same reason PsModalRouteEvent was in
'' step 2: the painter needs a compositor and this does not, so the DECISION is assertable
'' even though the drawing is not. Every kind is listed, including the ones that draw
'' nothing, so a new kind added later fails the exhaustive assertion rather than silently
'' inheriting "no glyph".
function ShellExplorer_GlyphFor( byval kind as ShellExpKind ) as DWSTRING
    dim as DWSTRING g
    select case kind
        case EXPKIND_FILE
            '' U+00B7 MIDDLE DOT, as UTF-8 bytes. tiko's wszIconDocument.
            g.Utf8 = chr(&hC2, &hB7)
        case EXPKIND_ROOT, EXPKIND_FOLDER
            '' Nothing: PsListTree's own twisty is already in this band, and a second glyph
            '' beside it would be two controls for one piece of state.
        case EXPKIND_PROMOTE, EXPKIND_NONE
            '' Nothing. tiko paints the promote row in the accent colour instead of giving
            '' it a glyph -- which an OVERLAY cannot do, because the caption is already on
            '' screen by the time it runs. Recorded rather than approximated.
    end select
    return g
end function


'' ---------------------------------------------------------------------------------------
'' Draw it. Runs AFTER the control has painted the row, which is what the overlay hook is
'' for -- the pre-paint hook replaces the row and would mean re-implementing the background,
'' the hot/selected cascade, the twisty, the indent and the text.
''
'' THE COLOUR IS THE ROW'S OWN, handed over in the paint info. Without that field the glyph
'' would need its own resolution of the same cascade and would be the wrong colour on
'' exactly the rows that are easiest not to look at -- the selected one and the hot one.
''
'' ---- THE BAND IS APPROXIMATED, AND HERE IS THE APPROXIMATION -------------------------
''
'' nIndent is the text's x offset and already includes the reserved twisty band, so the band
'' is what sits immediately to its left. Its WIDTH is the control's nTwisty, which is not
'' exposed -- so this uses the row's HEIGHT instead, which is the same order and is what the
'' band is sized from by convention. The glyph is centred in whatever rect that gives, so a
'' few pixels of difference is invisible; if it ever stops being, the fix is a getter rather
'' than arithmetic here.
'' ---------------------------------------------------------------------------------------
private function ShellExplorer_PaintOverlay( byval pi as PsLtPaintInfo ptr ) as boolean
    if pi = 0 then return false
    if g_panelMode <> SHPANEL_EXPLORER then return false

    '' THE ICONS SHARE THIS HOOK. PsListTree has ONE overlay slot, so the two painters are
    '' chained here rather than each asking for their own -- which also fixes their order:
    '' the icons must come last, because they clear the band they sit in.
    ShellExplorer_PaintIcons( pi )

    dim as DWSTRING g = ShellExplorer_GlyphFor( ShellPanel_KindOf( pi->nRow ) )
    if PsLen( g ) = 0 then return false

    dim as PsRect rc
    rc.h = pi->rc.h
    rc.w = pi->rc.h
    rc.y = pi->rc.y
    rc.x = pi->rc.x + pi->nIndent - rc.w
    if rc.x < pi->rc.x then rc.x = pi->rc.x
    if rc.w <= 0 then return false

    pi->p->SetForeColor( pi->clrText )
    pi->p->PaintText( g, @rc, PSTF_CENTER or PSTF_VCENTER )
    return false
end function


'' ---------------------------------------------------------------------------------------
'' The action icons, painted over the hot row. Also an overlay, and it runs after the row
'' glyph above so the two cannot fight over the buffer state.
''
'' ---- THE CAPTION IS ALREADY ON SCREEN, WHICH IS THIS HOOK'S ONE REAL LIMIT ------------
''
'' tiko clips the text short of the icons BEFORE drawing it (frmExplorer.inc:400) -- it
'' replaces the whole row, so it can. An overlay cannot: by the time it runs, a long folder
'' name has already been painted, and PaintText ellipsizes without reporting that it did.
''
'' So the icons paint their own band of the row's OWN background first. The result is the
'' same -- a long caption ends where the icons begin -- and the mechanism is different, so
'' it is worth saying rather than leaving the next reader to wonder why there is a fill
'' here. clrBack is the resolved colour the row was actually painted with, which is why
'' PsLtPaintInfo carries it.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_PaintIcons( byval pi as PsLtPaintInfo ptr ) as boolean
    if pi = 0 then return false
    if g_panelMode <> SHPANEL_EXPLORER then return false
    '' THE HOT ROW ONLY. A column of these down every row would compete with the filenames
    '' and read as chrome rather than as controls -- and they have no idle appearance to get
    '' wrong, because they are only ever drawn in the hot colour.
    if pi->bHot = false then return false

    dim as PsRect rcAdd, rcRen, rcDel
    ShellExplorer_IconRects( pi->nRow, pi->rc, rcAdd, rcRen, rcDel )
    if (rcAdd.w = 0) andalso (rcRen.w = 0) andalso (rcDel.w = 0) then return false

    '' The band the icons occupy, cleared to the row's own background so a long caption
    '' cannot run underneath them.
    dim as long xLeft = pi->rc.x + pi->rc.w
    if (rcAdd.w > 0) andalso (rcAdd.x < xLeft) then xLeft = rcAdd.x
    if (rcRen.w > 0) andalso (rcRen.x < xLeft) then xLeft = rcRen.x
    if (rcDel.w > 0) andalso (rcDel.x < xLeft) then xLeft = rcDel.x
    dim as PsRect rcBand
    rcBand.x = xLeft
    rcBand.y = pi->rc.y
    rcBand.w = (pi->rc.x + pi->rc.w) - xLeft
    rcBand.h = pi->rc.h
    if rcBand.w > 0 then
        pi->p->SetBackColor( pi->clrBack )
        pi->p->PaintRect( @rcBand )
    end if

    pi->p->SetForeColor( pi->clrText )
    dim as DWSTRING g
    if rcAdd.w > 0 then
        g.Utf8 = chr(&hEE, &hA3, &hB4)   '' U+E8F4  new folder  (tiko wszIconNewFolder)
        pi->p->PaintText( g, @rcAdd, PSTF_CENTER or PSTF_VCENTER )
    end if
    if rcRen.w > 0 then
        g.Utf8 = chr(&hEE, &hA2, &hAC)   '' U+E8AC  rename      (tiko wszIconRename)
        pi->p->PaintText( g, @rcRen, PSTF_CENTER or PSTF_VCENTER )
    end if
    if rcDel.w > 0 then
        g.Utf8 = chr(&hEE, &h9D, &h8D)   '' U+E74D  trash       (tiko wszIconTrash)
        pi->p->PaintText( g, @rcDel, PSTF_CENTER or PSTF_VCENTER )
    end if
    return false
end function


'' =======================================================================================
'' FOLDER RENAME -- 7c step 23, on the label editor PsListTree gained in the same step.
''
'' Both callbacks are tiko's, almost verbatim: frmExplorer_BeginLabelEditCallback is one
'' line and frmExplorer_EndLabelEditCallback is twenty of ProjectFolders_* calls, every one
'' of which has been in app/ since before this pane existed.
'' =======================================================================================

'' Only a folder row. A group's caption comes from gConfig.Cat and a file's from the disk;
'' neither is the user's to type over, and the Save-as-Project row is not a name at all.
private function ShellExplorer_OnBeginEdit( byval pList as any ptr, byval nRow as long, _
                                            byval ud as any ptr ) as boolean
    return ( ShellPanel_KindOf( nRow ) = EXPKIND_FOLDER )
end function


'' ---------------------------------------------------------------------------------------
'' Accept or refuse a renamed folder. tiko's frmExplorer_EndLabelEditCallback.
''
'' REFUSES SILENTLY rather than complaining: an empty name, a name carrying a path
'' separator, and a name already taken by a sibling. Returning FALSE leaves the caption as
'' it was -- which for a folder just created means it keeps its default name, a legal
'' outcome rather than a failure.
'' ---------------------------------------------------------------------------------------
private function ShellExplorer_OnEndEdit( byval pList as any ptr, byval nRow as long, _
                                          byref sText as DWSTRING, _
                                          byval ud as any ptr ) as boolean
    dim as long catIndex
    dim as DWSTRING wszOld = ShellExplorer_FolderPathFromRow( nRow, catIndex )
    if catIndex < 0 then return false
    if PsLen( wszOld ) = 0 then return false

    dim as DWSTRING wszName = PsTrim( sText )
    if ProjectFolders_IsValidName( wszName ) = false then return false

    '' UNCHANGED APART FROM CASE IS NOT A RENAME, and must not be refused as a
    '' self-collision. tiko's comment, and a real trap: the sibling test below would
    '' otherwise find the folder itself and reject every attempt to fix a capital letter.
    if PsUCase( wszName ) = PsUCase( ProjectFolders_LeafName( wszOld ) ) then return true

    dim as DWSTRING wszNew = ProjectFolders_Combine( ProjectFolders_ParentPath( wszOld ), _
                                                     wszName )
    if ProjectFolders_Exists( catIndex, wszNew ) then return false

    ProjectFolders_Rebase( catIndex, wszOld, wszNew, _
                           gApp.pDocList, gConfig.Cat(catIndex).idFileType )

    '' The control would write the new caption into the row it has, but every row index is
    '' about to be rebuilt from the table anyway -- and THE TABLE IS NOW THE TRUTH. Safe to
    '' call from inside the callback because EndEdit clears its own state before running
    '' this; see PsListTree.EndEdit.
    ShellExplorer_Load()
    return true
end function


'' ---------------------------------------------------------------------------------------
'' Open the editor on a folder row.
''
'' ITS OWN ENTRY POINT rather than a bare BeginEdit at each call site, and tiko gives the
'' reason: BeginEdit is happy to open on a row that is not the current one, which leaves the
'' selection highlight somewhere else entirely while the user types. There are two call
'' sites here -- the context menu and the tail of New Folder -- and tiko has three.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_RenameFolder( byval nRow as long ) as boolean
    if g_panel = 0 then return false
    if ShellPanel_KindOf( nRow ) <> EXPKIND_FOLDER then return false
    g_panel->SetCurSel( nRow )
    return g_panel->BeginEdit( nRow )
end function


'' =======================================================================================
'' THE ACTION ICONS -- 7c step 24, the last of tiko's Explorer.
''
'' Up to three glyph buttons at the right of the HOT row: add, rename, delete. All three
'' commands already existed; what arrived in step 24 is PsListTree handing a host a LEFT
'' click with its position, which is what makes a button inside a row possible at all.
'' =======================================================================================

enum ShellExpIcon
    EXPICON_NONE   = 0
    EXPICON_ADD    = 1
    EXPICON_RENAME = 2
    EXPICON_DELETE = 3
end enum

'' UNSCALED, like every other layout constant in this binary; scaled at the point of use.
const SHP_ICON_UNITS = 22
const SHP_ICONPAD_UNITS = 2


'' ---------------------------------------------------------------------------------------
'' Which action icons a row offers, and where they sit.
''
'' ONE IMPLEMENTATION, CALLED BY BOTH THE PAINTER AND THE HIT TEST -- tiko says so in its
'' own header and it is the whole reason this is a function rather than two blocks of
'' arithmetic. Two copies drift, and the way they drift is an icon that is drawn one place
'' and clicked another, which reads as the button not working.
''
'' This version is better placed to honour it than tiko's. tiko's hit test has to REBUILD
'' the row rect from the control's client rect and hope it matches what the painter drew
'' into; here the overlay is handed pi->rc and the click can ask RowRect, so both callers
'' pass in the same rect from the same source.
''
'' EMPTY RECTS MEAN "NOT OFFERED", which is how both callers test them.
'' ---------------------------------------------------------------------------------------
sub ShellExplorer_IconRects( byval nRow as long, byref rcRow as PsRect, _
                             byref rcAdd as PsRect, byref rcRen as PsRect, _
                             byref rcDel as PsRect )
    rcAdd.x = 0 : rcAdd.y = 0 : rcAdd.w = 0 : rcAdd.h = 0
    rcRen = rcAdd
    rcDel = rcAdd
    if g_panel = 0 then exit sub

    dim as boolean bWantAdd = false, bWantRen = false, bWantDel = false
    select case ShellPanel_KindOf( nRow )
        case EXPKIND_ROOT
            '' A GROUP CAN BE ADDED TO BUT NEVER RENAMED OR DELETED. Its caption comes from
            '' gConfig.Cat and its existence is what every file's ProjectFiletype names --
            '' the five categories are structural, not user data. And only where the
            '' category takes folders at all: Main and Resource hold one file each.
            bWantAdd = ProjectFolders_CatAllowsFolders( cast(long, g_panel->GetItemData(nRow)) )
        case EXPKIND_FOLDER
            bWantAdd = true
            bWantRen = true
            bWantDel = true
    end select
    if (bWantAdd = false) andalso (bWantRen = false) andalso (bWantDel = false) then exit sub

    dim as single f = 1.0
    if g_pSurf <> 0 then f = g_pSurf->fScale
    dim as long nIcon = PsScaleBy( SHP_ICON_UNITS, f )
    dim as long nPad  = PsScaleBy( SHP_ICONPAD_UNITS, f )
    dim as long xRight = rcRow.x + rcRow.w - nPad

    '' LAID OUT FROM THE RIGHT INWARDS, so reading order left-to-right ends up Add, Rename,
    '' Delete. Each row right-aligns its OWN set, so a group row -- which has only Add --
    '' puts it at the margin rather than three columns in where a folder's Add sits. tiko's
    '' choice, with tiko's reason: a ragged left edge across rows with different icon counts
    '' reads worse than a straight right one.
    if bWantDel then
        rcDel.x = xRight - nIcon : rcDel.y = rcRow.y : rcDel.w = nIcon : rcDel.h = rcRow.h
        xRight = rcDel.x
    end if
    if bWantRen then
        rcRen.x = xRight - nIcon : rcRen.y = rcRow.y : rcRen.w = nIcon : rcRen.h = rcRow.h
        xRight = rcRen.x
    end if
    if bWantAdd then
        rcAdd.x = xRight - nIcon : rcAdd.y = rcRow.y : rcAdd.w = nIcon : rcAdd.h = rcRow.h
    end if
end sub


'' Which icon, if any, sits under x on this row. ONLY X IS COMPARED: the icons span the
'' row's full height, so being on the row at all is established by having a row index.
function ShellExplorer_IconHitTest( byval nRow as long, byval nX as long ) as ShellExpIcon
    if g_panel = 0 then return EXPICON_NONE
    dim as PsRect rcRow
    if g_panel->RowRect( nRow, @rcRow ) = false then return EXPICON_NONE

    dim as PsRect rcAdd, rcRen, rcDel
    ShellExplorer_IconRects( nRow, rcRow, rcAdd, rcRen, rcDel )

    if rcDel.w > 0 then
        if (nX >= rcDel.x) andalso (nX < rcDel.x + rcDel.w) then return EXPICON_DELETE
    end if
    if rcRen.w > 0 then
        if (nX >= rcRen.x) andalso (nX < rcRen.x + rcRen.w) then return EXPICON_RENAME
    end if
    if rcAdd.w > 0 then
        if (nX >= rcAdd.x) andalso (nX < rcAdd.x + rcAdd.w) then return EXPICON_ADD
    end if
    return EXPICON_NONE
end function


'' ---------------------------------------------------------------------------------------
'' The left click, offered by PsListTree before it does anything of its own.
''
'' CLAIMS ONLY AN ICON. Everything else on the row -- the caption, the twisty band, the
'' empty space to the left of the icons -- is the tree's, and returning true for any of it
'' would break selection and folding for the sake of three buttons.
''
'' A COMMAND MAY REBUILD THE TREE UNDER US, which is why the row is resolved to what it
'' MEANS before anything is run: NewFolder and DeleteFolder both call ShellExplorer_Load,
'' and an index read afterwards would name a different row.
'' ---------------------------------------------------------------------------------------
function ShellExplorer_OnRowClick( byval pList as any ptr, byval nRow as long, _
                                   byval nX as long, byval nY as long, _
                                   byval ud as any ptr ) as boolean
    if g_panelMode <> SHPANEL_EXPLORER then return false
    if nRow < 0 then return false

    '' THE ICONS ARE ON THE HOT ROW ONLY, and the hit test has to agree with the painter
    '' about that or a click lands on a button nobody can see. bHot is the painter's rule;
    '' nHotRow is the same fact, read from the other side.
    if g_panel = 0 then return false
    if g_panel->nHotRow <> nRow then return false

    dim as ShellExpIcon hit = ShellExplorer_IconHitTest( nRow, nX )
    if hit = EXPICON_NONE then return false

    select case hit
        case EXPICON_ADD    : ShellExplorer_NewFolder( nRow )
        case EXPICON_RENAME : ShellExplorer_RenameFolder( nRow )
        case EXPICON_DELETE : ShellExplorer_DeleteFolder( nRow )
    end select
    return true
end function


'' =======================================================================================
'' DRAG AND DROP -- 7c step 25, and the last of frmExplorer.inc.
''
'' A port of frmExplorer_CanDropCallback, whose NAME IS A MISNOMER worth restating here: it
'' is not a validator that permits the control's reorder. It performs the whole move itself
'' and returns FALSE every single time, because the Explorer's rows are a VIEW of
'' gProjectFolders and gApp.pDocList -- reordering them would be reordering a rendering.
'' PsListTree's OnDrop is shaped around exactly that, and this returns false too.
''
'' ---- ONE SOURCE ROW, AND THE REASON IS THE SELECTION MODE ----------------------------
''
'' tiko's handler carries a snapshot loop over p->srcRows because its Explorer is
'' multi-select. This panel is PSLT_SEL_SINGLE, so a multi-row drag cannot happen, and the
'' loop would be dead code that looked like a feature. It defers WITH the selection mode
'' that would produce it.
'' =======================================================================================
function ShellExplorer_OnDrop( byval pList as any ptr, byval nSrcRow as long, _
                               byval nTargetRow as long, byval bOnTarget as boolean, _
                               byval ud as any ptr ) as boolean
    '' FALSE THROUGHOUT: every exit from here means "the tree must not reorder", whether
    '' this moved something or refused to. There is no path on which a row order change
    '' would be right.
    if g_panelMode <> SHPANEL_EXPLORER then return false
    if g_panel = 0 then return false
    if nSrcRow < 0 then return false

    '' ---- WHERE IS THIS GOING? --------------------------------------------------------
    dim as long catDest = -1
    dim as DWSTRING wszDest
    select case ShellPanel_KindOf( nTargetRow )
        case EXPKIND_ROOT, EXPKIND_FOLDER
            wszDest = ShellExplorer_FolderPathFromRow( nTargetRow, catDest )
        case EXPKIND_FILE
            '' INTO THE DROPPED-ON FILE'S OWN CONTAINER, which is what dropping a file on
            '' another file means. Refusing it instead would make the target's own row a
            '' dead zone in the middle of the folder it belongs to.
            dim as DWSTRING wszT = ShellPanel_PathOf( nTargetRow )
            dim pT as clsDocument ptr = gApp.GetDocumentPtrByFilename( wszT )
            if pT = 0 then return false
            catDest = ShellExplorer_CatIndexForDoc( pT )
            wszDest = pT->docData.wszFolder
        case else
            return false        '' the Save-as-Project row, or past the last row
    end select
    '' UNTESTED RATHER THAN REDUNDANT, and the distinction is worth the line. Removing this
    '' leaves the suite green -- but not because something below repeats it, as with the two
    '' folder guards further down. It is because no fixture here can produce the state that
    '' reaches it: a document whose ProjectFiletype matches no category at all. The guard is
    '' real and this suite cannot see it.
    if catDest < 0 then return false

    '' ---- SNAPSHOT THE SOURCE BEFORE ANYTHING MOVES -----------------------------------
    '' Row indices stop meaning anything the moment the model changes, so every decision
    '' below is taken against a pointer and a path captured here. tiko's rule, and it
    '' matters more here than there: ShellExplorer_Load rebuilds the whole tree.
    dim as ShellExpKind srcKind = ShellPanel_KindOf( nSrcRow )
    dim pSrcDoc as clsDocument ptr = 0
    dim as DWSTRING wszSrcFolder
    dim as long catSrc = -1

    select case srcKind
        case EXPKIND_FILE
            pSrcDoc = gApp.GetDocumentPtrByFilename( ShellPanel_PathOf( nSrcRow ) )
            if pSrcDoc = 0 then return false
            catSrc = ShellExplorer_CatIndexForDoc( pSrcDoc )
        case EXPKIND_FOLDER
            wszSrcFolder = ShellExplorer_FolderPathFromRow( nSrcRow, catSrc )
            if PsLen( wszSrcFolder ) = 0 then return false
        case else
            return false        '' a group and the promote row are not draggable things
    end select

    '' ---- VALIDATE EVERYTHING BEFORE MOVING ANYTHING ----------------------------------
    '' A refused drop must change NOTHING, so nothing is applied until the whole gesture is
    '' known to be legal. tiko's reason for the ordering, and it survives with one source
    '' as well as with many: a partial move is silent.
    if srcKind = EXPKIND_FOLDER then
        '' A folder cannot enter a group that holds no folders.
        if ProjectFolders_CatAllowsFolders( catDest ) = false then return false
        '' ---- AND THESE TWO ARE EARLY-OUTS, NOT THE GUARD ------------------------------
        '' ProjectFolders_MoveFolder tests both on its own lines 409-412, under the same
        '' catFrom = catTo condition and in the same order -- so removing either leaves the
        '' behaviour correct and the suite green, which is how they were found.
        ''
        '' THAT IS THE THIRD PAIR OF REDUNDANT SHELL-SIDE GUARDS IN THIS PORT (step 21 found
        '' two), and the pattern is worth more than the lines: modProjectFolders was written
        '' DEFENSIVELY long before it had a caller, so a handler ported from tiko re-states
        '' rules the model already enforces. Kept, because the port is a port and tiko's
        '' shape is the reference -- but labelled, so nobody reads this layer as the place
        '' the rules live.
        if catSrc = catDest then
            if PsUCase( wszSrcFolder ) = PsUCase( wszDest ) then return false
            if ProjectFolders_IsDescendantOf( wszDest, wszSrcFolder ) then return false
        end if
    end if

    '' MAIN AND RESOURCE HOLD EXACTLY ONE FILE. A folder there is refused outright; a single
    '' file is allowed and DEMOTES the incumbent, which is the gesture whose result the user
    '' can actually see.
    if (catDest = CATINDEX_MAIN) orelse (catDest = CATINDEX_RESOURCE) then
        if srcKind = EXPKIND_FOLDER then return false
    end if

    '' ---- APPLY -----------------------------------------------------------------------
    if srcKind = EXPKIND_FOLDER then
        '' The documents inside the subtree are collected BEFORE the move: afterwards their
        '' folders name the destination and nothing identifies them as having come from the
        '' source. Only needed for a CROSS-CATEGORY move, which is the only case that
        '' changes their type.
        redim moved(any) as clsDocument ptr
        dim as long nMoved = 0
        if catSrc <> catDest then
            dim pScan as clsDocument ptr = gApp.pDocList
            do until pScan = 0
                if ShellExplorer_CatIndexForDoc( pScan ) = catSrc then
                    dim as DWSTRING wszF = pScan->docData.wszFolder
                    dim as boolean bInside = ( PsUCase(wszF) = PsUCase(wszSrcFolder) )
                    if ProjectFolders_IsDescendantOf( wszF, wszSrcFolder ) then bInside = true
                    if bInside then
                        redim preserve moved(0 to nMoved)
                        moved(nMoved) = pScan
                        nMoved += 1
                    end if
                end if
                pScan = pScan->pDocNext
            loop
        end if

        if ProjectFolders_MoveFolder( catSrc, wszSrcFolder, catDest, wszDest, _
                                      gApp.pDocList, gConfig.Cat(catSrc).idFileType ) then
            for j as long = 0 to nMoved - 1
                if moved(j) then gApp.ProjectSetFileType( moved(j), gConfig.Cat(catDest).idFileType )
            next
        end if
    else
        '' THE TYPE CHANGE GOES THROUGH ProjectSetFileType rather than assigning the field,
        '' because that is what carries the Main/Resource demotion -- so a drag inherits the
        '' context menu's rule instead of restating it and drifting from it.
        if catSrc <> catDest then
            gApp.ProjectSetFileType( pSrcDoc, gConfig.Cat(catDest).idFileType )
        end if
        pSrcDoc->docData.wszFolder = wszDest
    end if

    ShellExplorer_Load()
    return false
end function
