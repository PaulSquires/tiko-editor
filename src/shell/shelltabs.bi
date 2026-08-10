'' ========================================================================================
'' shelltabs -- the shell's own tab model. NEW CODE, NOT A PORT.
''
'' 7c step 3's plan had commit 6 as "clsTopTabCtl wired to the existing PsTabBar". Measuring
'' it killed that: clsTopTabCtl is a FACADE over tiko's PsTabBar HWND control, every field it
'' has is control geometry, the clsDocument ptr for each tab is stored INSIDE that control
'' and read back with PsTabBar_GetItemData, and it reaches AfxNova transitively through
'' PsBufferPaint.bi. It stays in tiko. See docs/port/document-model-blockers.md.
''
'' So this binary needs its own, and the portable half it builds on is already in the app
'' layer: clsApp.pDocList is the document list, and clsDocument carries the per-view caret
'' and scroll position that a switch has to preserve.
''
'' ---- ONE VIEW PAIR, MANY DOCUMENTS -----------------------------------------------------
''
'' tiko creates a Scintilla window per document per view -- two windows each, shown and
'' hidden as tabs change. THIS SHELL DOES NOT, and the difference is deliberate rather than
'' a shortcut.
''
'' Its layout places exactly one editor rect and one split rect. A window per document would
'' mean N pairs of widgets in the tree, all but one hidden, every one of them laid out --
'' for no visible difference, because only one is ever on screen.
''
'' Scintilla's own mechanism for this is the DOCUMENT POINTER. A document is a buffer with a
'' reference count, independent of any view; SCI_SETDOCPOINTER points a view at one. The
'' shell already relies on exactly this for the SPLIT view, which shows one document in two
'' views -- this is the same mechanism used the other way round.
''
'' ---- WHAT A SWITCH MUST PRESERVE, AND WHY IT IS NOT AUTOMATIC --------------------------
''
'' A Scintilla document remembers its text, not where the user was looking at it. Caret and
'' first-visible-line belong to the VIEW, and there is one view for all tabs -- so switching
'' without saving them lands every tab at the top with the caret at zero.
''
'' clsDocument already has the fields for this (docData.nPosition, nFirstLine) because tiko
'' stores the same thing for its own reasons. They are read and written here.
'' ========================================================================================

#pragma once

const SH_MAX_DOCS = 32

type ShellTabEntry
    pDoc     as clsDocument ptr
    '' The Scintilla DOCUMENT this tab owns -- not a view. Reference-counted by Scintilla;
    '' created with SCI_CREATEDOCUMENT at a refcount of 1 and released on close.
    pSciDoc  as any ptr
    nPos     as long          '' caret, saved on the way out
    nFirst   as long          '' first visible line, likewise
end type

'' ---- THERE IS NO sPath FIELD HERE ANY MORE, and its removal is the point of commit 1.
''
'' It held a copy of pDoc->DiskFilename so that "is this file already open?" could be
'' answered from this array alone. That was TWO SOURCES OF TRUTH for one fact, and the field
'' had already grown the maintenance the second source always needs: a re-read after Save As,
'' because Save As renames the document and a stale copy would send the next Ctrl+S to the
'' old path.
''
'' gApp.pDocList is the answer now, which is what tiko has always used.

dim shared g_tabDocs(0 to SH_MAX_DOCS - 1) as ShellTabEntry
dim shared as long g_nTabDocs
dim shared as long g_nTabCur = -1


'' Capture where the user was, so returning to this tab returns to the same place.
private sub ShellTabs_SaveView( byval idx as long )
    if (idx < 0) orelse (idx >= g_nTabDocs) then exit sub
    if g_view = 0 then exit sub
    g_tabDocs(idx).nPos   = SciMsg( g_view->pSci, SCI_GETCURRENTPOS, 0, 0 )
    g_tabDocs(idx).nFirst = SciMsg( g_view->pSci, SCI_GETFIRSTVISIBLELINE, 0, 0 )
end sub


'' Point the single view at this tab's document and put the user back where they were.
sub ShellTabs_Show( byval idx as long )
    if (idx < 0) orelse (idx >= g_nTabDocs) then exit sub
    if g_view = 0 then exit sub
    if idx = g_nTabCur then exit sub

    if g_nTabCur >= 0 then ShellTabs_SaveView( g_nTabCur )

    g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idx).pSciDoc) )

    '' THE SPLIT FOLLOWS. It is a second view of whatever the main view is showing, which is
    '' the property the layout's split modes depend on -- letting it keep the outgoing
    '' document would put two different files on screen under one tab.
    if g_view2 <> 0 then
        g_view2->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idx).pSciDoc) )
    end if

    '' RESTORE FIRST-VISIBLE AFTER THE CARET, not before. SCI_GOTOPOS scrolls to reveal the
    '' caret, so setting the scroll first and the caret second undoes the scroll.
    SciMsg( g_view->pSci, SCI_GOTOPOS, g_tabDocs(idx).nPos, 0 )
    SciMsg( g_view->pSci, SCI_SETFIRSTVISIBLELINE, g_tabDocs(idx).nFirst, 0 )

    g_nTabCur = idx

    '' ---- FOCUS FOLLOWS THE TAB, and without this the caret vanishes on every switch.
    ''
    '' Clicking a tab gives the TAB BAR the focus -- it is a focusable widget and it was
    '' just clicked. The editor then has no focus, so Scintilla stops drawing a caret, and
    '' the user has clicked "go to this file" and been handed a window they cannot type in.
    ''
    '' SetFocus is also what makes the restored caret position visible: the position was
    '' always set below, but an unfocused view draws no caret to show it at.
    if (g_pSurf <> 0) andalso (g_view <> 0) then g_pSurf->SetFocus( g_view )

    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' PsTabBar's selection callback.
private sub ShellTabs_OnSelect( byval pBar as any ptr, byval idx as long, byval ud as any ptr )
    ShellTabs_Show( idx )
end sub


'' Which tab is showing this document, or -1. A POINTER COMPARE over the array, and pure --
'' it reads nothing else, so --selftest can drive it with addresses that are never
'' dereferenced.
''
'' This is the array's ONE job now: tab ORDER. Which documents exist is gApp's.
function ShellTabs_IndexOfDoc( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then return -1
    for i as long = 0 to g_nTabDocs - 1
        if g_tabDocs(i).pDoc = pDoc then return i
    next
    return -1
end function


'' The tab already showing this path, or -1.
''
'' THE LOOKUP IS gApp's, not ours. clsApp.GetDocumentPtrByFilename already answers "is this
'' file open?" for tiko, and it is already case-insensitive (clsApp.inc:147) -- Windows paths
'' are, and the same file opened under two spellings would otherwise become two documents
'' over one path, where saving either silently discards the other's edits.
''
'' Delegating means the two binaries cannot disagree about what "already open" means, and it
'' is what let the sPath field go.
function ShellTabs_FindByPath( byval wszPath as DWSTRING ) as long
    return ShellTabs_IndexOfDoc( gApp.GetDocumentPtrByFilename( wszPath ) )
end function


'' The document the user is looking at, or null when nothing is open.
function ShellTabs_CurrentDoc() as clsDocument ptr
    if (g_nTabCur < 0) orelse (g_nTabCur >= g_nTabDocs) then return 0
    return g_tabDocs(g_nTabCur).pDoc
end function


'' Opens a file as a new tab. Returns its index, or -1.
''
'' AN ALREADY-OPEN FILE IS SHOWN, NOT REOPENED, which is what tiko does through
'' gApp.GetDocumentPtrByFilename (frmMainFile.inc:110). Without it the same file gets two
'' clsDocuments and two Scintilla documents, and saving one silently discards the other's
'' edits -- there is no conflict to notice, because both believe they own the path.
function ShellTabs_Open( byval wszPath as DWSTRING ) as long
    dim as long idxOpen = ShellTabs_FindByPath( wszPath )
    if idxOpen >= 0 then
        ShellTabs_Show( idxOpen )
        if g_tabs <> 0 then g_tabs->SetCurSel( idxOpen )
        print "tikoshell: " & PsPathName( wszPath ).Utf8 & " is already open (tab " & idxOpen & ")"
        return idxOpen
    end if

    if g_nTabDocs >= SH_MAX_DOCS then
        print "tikoshell: too many open documents (cap " & SH_MAX_DOCS & ")"
        return -1
    end if
    if g_view = 0 then return -1

    dim as long idx = g_nTabDocs

    '' A DOCUMENT OF ITS OWN, created before clsDocument loads anything into it. Scintilla
    '' hands back a buffer with a refcount of 1; pointing a view at it does not transfer
    '' ownership, which is why the release on close is ours to do.
    g_tabDocs(idx).pSciDoc = cast( any ptr, g_view->Msg( SCI_CREATEDOCUMENT, 0, 0 ) )
    if g_tabDocs(idx).pSciDoc = 0 then
        print "tikoshell: Scintilla would not create a document for " & wszPath.Utf8
        return -1
    end if

    '' Point the view at it BEFORE loading, or clsDocument fills whichever document the view
    '' happened to be showing -- which is the previous tab's.
    g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idx).pSciDoc) )
    g_nTabCur = idx

    '' ---- THE ORDER HERE IS THE WHOLE THING, and getting it wrong shows a file that
    '' opens, tabs correctly, and is EMPTY.
    ''
    '' LoadDiskFile only fills clsDocument.TextBuffer -- it never touches Scintilla.
    '' AssignTextBuffer is what pushes that buffer in, and it CREATES THE VIEWS ITSELF:
    ''
    ''     "If a valid scintilla window already exists then this means that a previous
    ''      function has already assigned the buffer text to the scintilla window so we
    ''      can not do it a second time"
    ''     if gAppHost.IsViewAlive(this.hWindow(0)) then exit function
    ''
    '' So calling CreateScintillaWindows first -- which looks like the obvious setup step --
    '' makes that guard fire and the assignment never happens. It is a guard against
    '' DOUBLE assignment, and an explicit create is indistinguishable from a first one.
    '' Two tabs opened and both were blank.
    ''
    '' ---- gApp.CreateEmptyDocument, NOT `new clsDocument`, and that one word is commit 1.
    ''
    '' A bare `new` produced a document NOTHING IN THE APP LAYER COULD SEE. gApp.pDocList is
    '' how the model answers "which documents exist" -- GetDocumentPtrByFilename walks it,
    '' IsValidDocumentPointer walks it, and every panel tiko has walks it -- and in this
    '' binary it was permanently EMPTY. The tabs worked, so nothing said so.
    ''
    '' CreateEmptyDocument(false) links the document into that list and sets ProjectFileType
    '' to FILETYPE_UNDEFINED. It does NOT touch FileEncoding: that happens only on the
    '' IsNewFile arm, which this is not (clsApp.inc:47-61). So the only behaviour this adds
    '' to the shell's own path is the category, and SetProjectFileType below is what tiko
    '' calls to fill it in from the extension (frmMainFile.inc:117).
    dim as clsDocument ptr pDoc = gApp.CreateEmptyDocument()
    pDoc->LoadDiskFile( wszPath )
    pDoc->SetProjectFileType()
    pDoc->AssignTextBuffer()
    pDoc->ApplyProperties()

    '' REPORTED, because an empty document looks exactly like a working one from outside:
    '' the tab appears, the title is right, and the pane is blank. This is what the blank-tab
    '' bug above would have shown as 0 bytes.
    print "tikoshell: " & PsPathName( wszPath ).Utf8 & " -- " & _
          str( SciMsg( g_view->pSci, SCI_GETLENGTH, 0, 0 ) ) & " bytes"

    g_tabDocs(idx).pDoc   = pDoc
    g_tabDocs(idx).nPos   = 0
    g_tabDocs(idx).nFirst = 0
    g_nTabDocs += 1

    if g_tabs <> 0 then
        '' The file's name, not its path. PsPathFile is PsCore's, so this is the same call
        '' the app layer would make.
        g_tabs->AddTab( PsPathName( wszPath ), idx )
        g_tabs->SetCurSel( idx )
    end if

    return idx
end function


'' Saves the current tab. TRUE when the bytes reached disk.
''
'' ---- clsDocument.SaveFile's RESULT IS INVERTED, and this is the only place in the shell
'' that has to know it. FALSE means the save SUCCEEDED; TRUE means cancelled, or refused, or
'' failed to write. The convention is tiko's and app/clsDocument.inc:452-458 states it in
'' place ("Same inverted convention as SaveFile: FALSE = the bytes are on disk"), so it is
'' translated here rather than propagated -- every caller above this line reads TRUE as good.
''
'' NOTHING IS REPORTED ON FAILURE HERE, and that is not a hole: the document model has
'' already said so through the host, either as the lossy-save prompt the user cancelled or as
'' ReportWriteFailure. A second message would be the shell guessing which one happened.
function ShellTabs_Save( byval bIsSaveAs as boolean ) as boolean
    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then
        print "tikoshell: nothing to save -- no document is open"
        return false
    end if

    if pDoc->SaveFile( bIsSaveAs ) then return false

    '' STRAIGHT OFF THE DOCUMENT, because Save As is a RENAME: SaveFile writes the picked path
    '' into DiskFilename, and a tab left captioned with the old name is a tab that lies about
    '' which file the next Ctrl+S overwrites. There used to be a copy of the path in the entry
    '' to update here as well -- gApp holds the only one now, and this is one of the two
    '' places that no longer has to remember it exists.
    if g_tabs <> 0 then
        g_tabs->SetText( g_nTabCur, PsPathName( pDoc->DiskFilename ) )
    end if

    print "tikoshell: saved " & DWSTRING( pDoc->DiskFilename ).Utf8
    return true
end function


'' Wire the bar's callbacks. Called once, after the tab bar is in the tree.
sub ShellTabs_Install()
    if g_tabs = 0 then exit sub
    g_tabs->OnSelect( @ShellTabs_OnSelect, 0 )
end sub
