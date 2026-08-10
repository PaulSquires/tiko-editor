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
'' Empty the panel. tiko's ClearBookmarks, which is one call there and one call here.
'' ---------------------------------------------------------------------------------------
sub ShellPanel_Clear()
    if g_panel = 0 then exit sub
    g_panel->clear()
    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub


'' ---------------------------------------------------------------------------------------
'' Rebuild the bookmark list from every open document. The port of LoadBookmarksFiles.
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
    if g_view <> 0 then
        pWasDoc = cast( any ptr, g_view->Msg(SCI_GETDOCPOINTER, 0, 0) )
        if pWasDoc <> 0 then g_view->Msg( SCI_ADDREFDOCUMENT, 0, cast(integer, pWasDoc) )
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
    end if

    g_panel->EndUpdate()

    '' Restore where the user was looking. CLAMPED, because the reload may have produced
    '' fewer rows than there were -- tiko does the same MIN for the same reason.
    g_panel->SetTopIndex( nTopIndex )
    nCurSel = iif( nCurSel > g_panel->GetCount() - 1, g_panel->GetCount() - 1, nCurSel )
    g_panel->SetCurSel( nCurSel )

    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
end sub
