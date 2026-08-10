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
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' PsTabBar's selection callback.
private sub ShellTabs_OnSelect( byval pBar as any ptr, byval idx as long, byval ud as any ptr )
    ShellTabs_Show( idx )
end sub


'' Opens a file as a new tab. Returns its index, or -1.
function ShellTabs_Open( byval wszPath as DWSTRING ) as long
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
    dim as clsDocument ptr pDoc = new clsDocument
    pDoc->LoadDiskFile( wszPath )
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


'' Wire the bar's callbacks. Called once, after the tab bar is in the tree.
sub ShellTabs_Install()
    if g_tabs = 0 then exit sub
    g_tabs->OnSelect( @ShellTabs_OnSelect, 0 )
end sub
