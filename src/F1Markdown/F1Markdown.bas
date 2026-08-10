'' ========================================================================================
'' F1Markdown -- a native markdown help viewer. PHASE 1: the shell and nothing behind it.
''
'' ---- WHAT THIS BINARY IS FOR -----------------------------------------------------------
''
'' tiko's F1 leaves the application: frmHelpCenter.inc builds a URL and ShellExecutes a
'' browser, because the WebView2 pane was deliberately removed (docs/port/webview2-decision.md).
'' This is the CHM replacement that fills that gap -- a folder of .md files as a TOC tree on
'' the left and a rendered page on the right, taking a topic on the command line so F1-on-symbol
'' lands somewhere useful.
''
'' ---- WHY A SECOND TU IN tiko, AND NOT A PsPlatform DEMO --------------------------------
''
'' The same reason shell\tikoshell.bas gives: PsPlatform's build driver is
'' convention-over-configuration and cannot pick up demos/<name> WITHOUT enrolling it in
'' `build all`, so an app mid-edit would break PsPlatform's own gate for reasons that have
'' nothing to do with PsPlatform. This TU includes none of tiko's frm* or Ps* files, so it
'' gets PsPlatform's UI at global scope and carries no `namespace PsC` prefixes -- see
'' tikoshell.bas:22-33 for the 17 duplicate-definition errors that fence exists to prevent.
''
'' ---- WHAT IS HERE, AND WHAT IS NOT -----------------------------------------------------
''
'' HERE: the seven-step pump, a window sized and scaled from the display, three docked bands,
'' a real PsListTree / PsSplitter / PsTextBox / PsToolbar / PsStatusBar, theme loading, and a
'' self-test that asserts ABSOLUTE geometry at two scales.
''
'' NOT HERE: the markdown parser, the layout engine, the renderer, the corpus index, search,
'' history, settings persistence, or the mailbox. The right-hand pane is a stub that paints
'' its own bounds -- deliberately, and not as a placeholder to be tidied: an empty real view
'' looks perfectly correct in the wrong band, whereas a rect that prints "0,34,760,622" tells
'' you where it actually is.
''
'' ---- THE FONTS ARE NOT PsPlatform'S ----------------------------------------------------
''
'' PsPlatform ships assets/fonts/CascadiaCode.ttf and nothing else, and it is a VARIABLE font:
'' TE_Init is FT_New_Face(face 0) + FT_Set_Pixel_Sizes with no variation axis set, so a
'' variable file renders its default instance and a "bold" that silently is not bold. A
'' markdown page needs body regular/bold/italic/bold-italic and mono regular/bold, so six
'' STATIC faces live in ../assets/f1markdown/fonts instead. Phase 1 opens one of them; phase 3
'' opens all six, one PsTextEngine per (face, pixel size).
'' ========================================================================================

#include once "crt/stddef.bi"

'' ---- PsCore's core layer ---------------------------------------------------------------
'' No AfxNova in this TU, so PsCore's DWSTRING is the only one there has ever been and
'' nothing can shadow it. tiko.bas has to order these AFTER AfxNova for that reason; here
'' the order is free.
#include once "core/DWString.inc"
#include once "core/PsStr.inc"
#include once "core/PsPath.inc"
#include once "core/PsFile.inc"
#include once "core/PsEncoding.inc"

'' ---- PsPlatform, AT GLOBAL SCOPE -------------------------------------------------------
#include once "platform/PlatformInit.inc"
#include once "ui/core/PsDispatch.inc"
#include once "ui/core/PsPaintWalk.inc"
#include once "ui/core/PsCursorSync.inc"
#include once "ui/core/PsTheme.inc"
#include once "ui/core/PsThemeLoadFile.inc"
#include once "ui/core/PsLayout.inc"
#include once "ui/controls/PsListTree.inc"
#include once "ui/controls/PsSplitter.inc"
#include once "ui/controls/PsTextBox.inc"
#include once "ui/controls/PsToolbar.inc"
#include once "ui/controls/PsStatusBar.inc"

'' ---- the document model ----------------------------------------------------------------
'' No UI, no painting, no platform calls -- these two are the layer the Linux port carries
'' over unchanged, and they are included before anything that would tempt them otherwise.
#include once "mdParse.inc"
#include once "mdHilite.inc"
#include once "mdIndex.inc"

'' ---- the renderer ----------------------------------------------------------------------
'' mdFonts and mdLayout sit on PsPlatform's PAINT layer -- PsTextEngine and PsImage -- but
'' reach no widget, no surface and no colour. MarkdownView is the only file here that knows
'' what a theme is.
#include once "mdFonts.inc"
#include once "mdLayout.inc"
#include once "MarkdownView.inc"


'' ========================================================================================
'' MODES
''   (none)       open the window
''   --selftest   build the tree, lay out, assert geometry at 1.0 AND 1.75, then run the
''                parser and highlighter suites. Exit non-zero on failure. WINDOWLESS, NOT
''                HEADLESS -- SDL's video subsystem is initialised because the text engine
''                needs it. No window is shown.
''   --open <path>     render one .md file instead of the built-in demo
''   --scan <path>    walk one root, print the topic tree, the heading count, the scan
''                    time and the ranked answer to --topic if one was given, then exit.
''                    No window, no font, no SDL -- the index is filesystem and strings.
''   --dump-md <path>
''                parse one .md file and print the block model, one line per block, then
''                every line the parser could not classify. No window, no font, no SDL.
''
'' SWITCHES (parsed here, acted on from phase 4 on except --theme and --selftest)
''   --topic <text>    the symbol F1 was pressed on; fills the search box and opens the
''                     best match, which is the same path Enter in the box takes
''   --docset <name>   which configured root to search first
''   --root <path>     a one-off content root, overriding f1markdown.ini
''   --theme <file>    a .theme file; default settings/themes/default_dark.theme
'' ========================================================================================

'' DESIGN UNITS, not pixels. Scaled to the display before the window is created.
const FM_W = 1040
const FM_H = 680

'' Band heights and widths, DESIGN units.
const FM_TOOLBAR_H = 34
const FM_STATUS_H  = 24
const FM_SPLIT_W   = 4
const FM_TOC_W     = 232      '' the initial left pane; the splitter owns it afterwards
const FM_TOC_MIN   = 140
const FM_PAGE_MIN  = 260
const FM_SEARCH_W  = 300
const FM_PAD       = 4
const FM_GAP       = 6

'' The UI font, in DESIGN pixels. Reopened at PsScaleBy(FM_FONT_PX, scale) once the window
'' reports its display's factor -- the text engine rasterises at a fixed pixel size and has
'' no notion of scale, so this is the host's job and nothing else does it.
const FM_FONT_PX = 15

'' Toolbar command ids.
const IDM_BACK    = 101
const IDM_FORWARD = 102

dim shared as PsTextEngine  g_te
dim shared as PsBufferPaint g_pnt
dim shared as PsSurface ptr g_pSurf

dim shared as long g_nPass, g_nFail

'' The font path, shared because ApplyScale reopens the engine and both the windowed path
'' and the self-test go through it.
dim shared as string g_sFont
'' The pixel size the engine is currently open at. Tracked here because the text engine has
'' no query for it, and reopening it unconditionally on every resize would rebuild the atlas
'' for nothing.
dim shared as long g_nFontPx

'' The left pane width in DESIGN units. The splitter reports PIXELS in the parent's
'' coordinates, so its callback divides by the scale before storing here -- otherwise moving
'' the window to a 175% display would move the furniture as well.
dim shared as long g_nTocW = FM_TOC_W

'' Command line, parsed once at startup.
dim shared as DWSTRING g_sTopic, g_sDocset, g_sRoot, g_sTheme

'' ---- the corpus, and where we are in it ------------------------------------------------
dim shared as MdIndex g_ix
dim shared as long g_nCurTopic = -1
dim shared as boolean g_bSearchMode
'' The toolbar exists before its callbacks can safely touch it; UpdateNav is reachable from
'' OpenTopic, which the command line can call during startup.
dim shared as boolean g_navReady

'' BACK/FORWARD: an array and a cursor, which is the whole of it. Pushing truncates whatever
'' was ahead of the cursor -- following a link from three pages back throws away the branch
'' you left, exactly as a browser does.
const NAV_MAX = 128
dim shared as long g_navHist(0 to NAV_MAX - 1)
dim shared as long g_navCount
dim shared as long g_navAt = -1

const HITS_MAX = 200
dim shared as MdHit g_hits(0 to HITS_MAX - 1)


'' ---------------------------------------------------------------------------------------
'' AppRoot -- the root widget, which exists to PAINT.
''
'' A plain PsWidget draws nothing, and PsPaintWalk does not clear behind it, so whatever the
'' bands do not cover shows the uninitialised buffer -- black. The toolbar band is exactly
'' that: the nav buttons and the search box size themselves and the strip beside them belongs
'' to nobody. Invisible against a dark theme and glaring against a light one, which is how it
'' got found.
'' ---------------------------------------------------------------------------------------
type AppRoot extends PsWidget
    clrBack as PsColor
    declare constructor()
    declare sub OnPaint( byval p as PsBufferPaint_ ptr )
    declare sub OnThemeChanged()
end type

constructor AppRoot()
    this.bFocusable = false
    this.OnThemeChanged()
end constructor

sub AppRoot.OnThemeChanged()
    '' BACKGROUNDALT, the same role the TOC and the status bar take: the bands are chrome and
    '' only the document pane is the page.
    this.clrBack = PsThemeRoleColor( PSTHEME_BACKGROUNDALT )
    this.Invalidate()
end sub

sub AppRoot.OnPaint( byval p as PsBufferPaint_ ptr )
    if p = 0 then exit sub
    dim as PsBufferPaint ptr q = cptr( PsBufferPaint ptr, p )
    dim as PsRect rc = PsRc(0, 0, this.bounds.w, this.bounds.h)
    q->SetBackColor( this.clrBack )
    q->PaintRect( @rc )
end sub


'' ---------------------------------------------------------------------------------------
'' THE TREE
'' ---------------------------------------------------------------------------------------
dim shared as PsWidget    ptr g_root
dim shared as PsToolbar   ptr g_nav
dim shared as PsTextBox   ptr g_search
dim shared as PsListTree  ptr g_toc
dim shared as PsSplitter  ptr g_split
dim shared as MarkdownView ptr g_page
dim shared as PsStatusBar ptr g_status

declare sub LayoutAll( byref surf as PsSurface )

'' The splitter moves nothing itself -- it reports a position and the host re-docks. nPos is
'' in the PARENT's coordinates and in PIXELS; the body band starts at x = 0, so the pixel
'' width of the left pane IS nPos.
sub OnSplitMove( byval pSpl as any ptr, byval nPos as long, byval ud as any ptr )
    if g_pSurf = 0 then exit sub
    dim as single f = g_pSurf->fScale
    if f <= 0 then f = 1.0
    g_nTocW = clng(nPos / f)
    LayoutAll( *g_pSurf )
    g_pSurf->InvalidateAll()
end sub


'' ---------------------------------------------------------------------------------------
'' NAVIGATION
'' ---------------------------------------------------------------------------------------
sub Say( byref sMsg as string )
    if g_status = 0 then exit sub
    dim as DWSTRING s
    s.Utf8 = sMsg
    g_status->SetText( 0, s )
end sub

sub UpdateNav()
    if g_navReady = false then exit sub
    g_nav->EnableItem( IDM_BACK, (g_navAt > 0) )
    g_nav->EnableItem( IDM_FORWARD, cbool(g_navAt >= 0) andalso cbool(g_navAt < g_navCount - 1) )
end sub

private sub NavPush( byval nTopic as long )
    if (g_navAt >= 0) andalso (g_navHist(g_navAt) = nTopic) then exit sub
    g_navAt += 1
    if g_navAt >= NAV_MAX then
        '' Oldest out. A help session that visits 128 pages is real; growing the array without
        '' bound so it can never happen is not worth the memory.
        for i as long = 1 to NAV_MAX - 1
            g_navHist(i - 1) = g_navHist(i)
        next
        g_navAt = NAV_MAX - 1
    end if
    g_navHist(g_navAt) = nTopic
    g_navCount = g_navAt + 1
end sub

sub OpenTopic( byval nTopic as long, byval bPush as boolean )
    if (nTopic < 0) orelse (nTopic >= g_ix.nTopic) then exit sub
    '' A folder or a docset row is selectable and opens nothing. Silently, on purpose: the
    '' alternative is a status line that scolds you for clicking a folder.
    if g_ix.topic(nTopic).kind <> MDX_DOC then exit sub

    dim as boolean bOk = false
    dim as string sUtf8 = MdReadUtf8( g_ix.topic(nTopic).sPath, bOk )
    if bOk = false then
        Say( "cannot read " & g_ix.topic(nTopic).sPath.Utf8 )
        exit sub
    end if
    g_page->SetMarkdown( sUtf8, PsPathDirWithSep(g_ix.topic(nTopic).sPath).Utf8 )

    g_nCurTopic = nTopic
    if bPush then NavPush( nTopic )

    '' SetCurSel is SILENT, which is the only reason this does not call straight back into
    '' OnTocSel and reload the page it has just loaded.
    if (g_bSearchMode = false) andalso (g_ix.topic(nTopic).nRow >= 0) then
        g_toc->SetCurSel( g_ix.topic(nTopic).nRow )
        g_toc->EnsureVisible( g_ix.topic(nTopic).nRow )
    end if

    Say( g_ix.topic(nTopic).sPath.Utf8 )
    UpdateNav()
end sub

sub OnNavCommand( byval pBar as any ptr, byval nId as long, byval ud as any ptr )
    select case nId
        case IDM_BACK
            if g_navAt > 0 then
                g_navAt -= 1
                OpenTopic( g_navHist(g_navAt), false )
            end if
        case IDM_FORWARD
            if (g_navAt >= 0) andalso (g_navAt < g_navCount - 1) then
                g_navAt += 1
                OpenTopic( g_navHist(g_navAt), false )
            end if
    end select
end sub


'' ---------------------------------------------------------------------------------------
'' THE TOC
''
'' EVERY AddNode BELOW LANDS AT THE END OF THE TREE, so no nRow recorded earlier can be
'' invalidated by a later insert. That is not luck: AddNode inserts after nParent's whole
'' subtree, and mdIndex hands its topics over in PRE-ORDER, so the parent's last descendant
'' is always the row just added. Fill it in any other order and the recorded rows go stale
'' silently, which shows up as the tree selecting the wrong page.
'' ---------------------------------------------------------------------------------------
sub FillTree()
    if g_toc = 0 then exit sub
    g_bSearchMode = false
    g_toc->BeginUpdate()
    g_toc->clear()
    for i as long = 0 to g_ix.nTopic - 1
        dim as long nPar = -1
        if g_ix.topic(i).nParent >= 0 then nPar = g_ix.topic( g_ix.topic(i).nParent ).nRow
        dim as DWSTRING sTx
        sTx.Utf8 = g_ix.topic(i).sName
        g_ix.topic(i).nRow = g_toc->AddNode( nPar, sTx, i )
    next
    g_toc->EndUpdate()
    if g_nCurTopic >= 0 then
        if g_ix.topic(g_nCurTopic).nRow >= 0 then
            g_toc->SetCurSel( g_ix.topic(g_nCurTopic).nRow )
            g_toc->EnsureVisible( g_ix.topic(g_nCurTopic).nRow )
        end if
    end if
end sub

'' The tree becomes a FLAT RANKED LIST while a query is live. Filtering the tree in place
'' would leave folders standing with no visible children, and ranked order is the point --
'' a tree cannot show one.
function FillSearchResults( byref sQuery as string ) as long
    if g_toc = 0 then return 0
    dim as long n = MdIndexSearch( g_ix, sQuery, g_hits(), HITS_MAX )
    g_bSearchMode = true
    g_toc->BeginUpdate()
    g_toc->clear()
    for i as long = 0 to n - 1
        dim as long t = g_hits(i).nTopic
        dim as string sTx = g_ix.topic(t).sTitle
        '' When a HEADING matched rather than the title, show it too -- otherwise five hits
        '' inside one page read as five identical rows.
        if g_hits(i).nHead >= 0 then
            dim as string sH = g_ix.head( g_hits(i).nHead ).sText
            if lcase(sH) <> lcase(sTx) then sTx = sTx & "  -  " & sH
        end if
        dim as DWSTRING sDw
        sDw.Utf8 = sTx
        g_toc->AddNode( -1, sDw, t )
    next
    g_toc->EndUpdate()
    return n
end function

sub OnTocSel( byval pList as any ptr, byval nRow as long, byval ud as any ptr )
    if g_toc = 0 then exit sub
    if g_toc->IsValidRow( nRow ) = false then exit sub
    OpenTopic( clng(g_toc->GetItemData(nRow)), true )
end sub

sub OnSearchChange( byval pTb as any ptr, byval ud as any ptr )
    if g_search = 0 then exit sub
    dim as string q = trim( g_search->GetText().Utf8 )
    if len(q) = 0 then
        FillTree()
        Say( str(g_ix.nDocs) & " topics" )
        exit sub
    end if
    dim as long n = FillSearchResults( q )
    Say( str(n) & " match" & iif(n = 1, "", "es") & " for " & q )
end sub

'' Enter opens the best hit -- the SAME path --topic takes, so the keyboard and the command
'' line can never disagree about which page a symbol means.
sub OnSearchEnter( byval pTb as any ptr, byval ud as any ptr )
    if g_search = 0 then exit sub
    dim as string q = trim( g_search->GetText().Utf8 )
    if len(q) = 0 then exit sub
    dim as long n = MdIndexSearch( g_ix, q, g_hits(), HITS_MAX )
    if n > 0 then OpenTopic( g_hits(0).nTopic, true )
end sub


'' ---------------------------------------------------------------------------------------
'' FOLLOWING A LINK
'' ---------------------------------------------------------------------------------------
sub OnDocLink( byval pView as any ptr, byref sHref as string, byval ud as any ptr )
    if len(sHref) = 0 then exit sub
    dim as string sLow = lcase(sHref)

    '' EXTERNAL LINKS ARE COPIED, NOT OPENED, and that is a limitation rather than a stub.
    '' PsPlatform has no open-url call; ShellExecute here would be raw Win32 above the
    '' platform layer -- what scripts/check-isolation.sh exists to prevent -- and it would
    '' need writing a second time for the Linux port. The CLIPBOARD is in the platform
    '' interface, so the url reaches a browser in one paste.
    if (left(sLow, 7) = "http://") orelse (left(sLow, 8) = "https://") then
        dim as DWSTRING u
        u.Utf8 = sHref
        g_plat.clip.SetText( u )
        Say( "copied to the clipboard: " & sHref )
        exit sub
    end if

    '' The #anchor is stripped: in-page anchors are out of v1, so a link to one opens its
    '' page and stays where it is, rather than doing nothing at all.
    dim as string sPath = sHref
    dim as long nHash = instr(sPath, "#")
    if nHash > 0 then sPath = left(sPath, nHash - 1)
    if len(sPath) = 0 then
        Say( "in-page anchors are not in v1: " & sHref )
        exit sub
    end if

    if g_nCurTopic < 0 then exit sub
    dim as DWSTRING sRel
    sRel.Utf8 = sPath
    dim as DWSTRING sFull = PsPathCanonicalise( _
        PsPathJoin( PsPathDirWithSep(g_ix.topic(g_nCurTopic).sPath), sRel ) )
    dim as long t = MdIndexFindPath( g_ix, sFull )
    if t >= 0 then
        OpenTopic( t, true )
    else
        Say( "not in the index: " & sPath )
    end if
end sub


sub BuildTree( byref surf as PsSurface )
    g_root = new AppRoot
    g_root->bClipsChildren = false
    surf.SetRoot( g_root )                    '' the surface takes ownership

    g_nav = new PsToolbar
    g_nav->AddItem( PsText("Back"), IDM_BACK, TBR_KIND_BUTTON )
    g_nav->AddItem( PsText("Forward"), IDM_FORWARD, TBR_KIND_BUTTON )
    g_nav->EnableItem( IDM_BACK, false )
    g_nav->EnableItem( IDM_FORWARD, false )
    g_nav->OnCommand( @OnNavCommand )
    g_navReady = true
    g_root->AddChild( g_nav )                 '' AddChild TRANSFERS OWNERSHIP

    g_search = new PsTextBox
    g_search->SetCueBannerText( PsText("Search topics") )
    g_search->OnChange( @OnSearchChange )
    g_search->OnEnterPressed( @OnSearchEnter )
    g_root->AddChild( g_search )

    g_toc = new PsListTree
    g_toc->SetTreeIndent( true )
    g_toc->ShowTwisty( true )
    '' One row, and it says what is true. An empty tree and a tree whose corpus failed to
    '' load look identical, and phase 1 has no corpus at all.
    g_toc->OnSelChange( @OnTocSel )
    g_root->AddChild( g_toc )

    g_split = new PsSplitter
    g_split->SetOrient( PSSPLIT_VERT )
    g_split->OnMove( @OnSplitMove )
    g_root->AddChild( g_split )

    g_page = new MarkdownView
    g_page->OnLink( @OnDocLink )
    g_root->AddChild( g_page )

    g_status = new PsStatusBar
    g_status->AddPanel( PsText("Ready") )
    g_status->AddPanel( PsText("0 topics") )
    g_status->SetExpandPanel( 0 )
    g_root->AddChild( g_status )
end sub


'' ---------------------------------------------------------------------------------------
'' THE LAYOUT.
''
'' PsDocker for the three outer bands -- that is exactly what it is for, and it multiplies
'' the DESIGN constants by the scale so nothing here does. The body split is done in explicit
'' pixels instead, because the splitter's position is inherently a pixel and routing it back
'' through a design-unit Take would scale it twice (PsLayout.bi: Take* scales, Take*Px does
'' not; passing an already-scaled value to the plain form gives 2.25x at 1.5).
'' ---------------------------------------------------------------------------------------
sub LayoutAll( byref surf as PsSurface )
    if surf.pRoot = 0 then exit sub

    dim as single f = surf.fScale
    if f <= 0 then f = 1.0

    dim as PsRect rcRoot = PsRc(0, 0, surf.w, surf.h)
    g_root->SetBounds( rcRoot )

    dim as PsDocker dk = PsDocker( rcRoot, f )
    dim as PsRect rcTop  = dk.TakeTop( FM_TOOLBAR_H )
    dim as PsRect rcBot  = dk.TakeBottom( FM_STATUS_H )
    dim as PsRect rcBody = dk.Fill()

    g_status->SetBounds( rcBot )

    '' ---- the toolbar band ---------------------------------------------------------
    '' The toolbar sizes itself (IdealWidth is measured from its captions and the current
    '' font), so it is docked in PIXELS. The search box is a design width.
    scope
        dim as PsDocker dt = PsDocker( rcTop, f )
        dt.InsetXY( FM_PAD, FM_PAD )
        dt.SetGap( FM_GAP )
        dim as PsRect rcNav = dt.TakeLeftPx( g_nav->IdealWidth() )
        g_nav->SetBounds( rcNav )
        dim as long nSearchPx = PsScaleBy( FM_SEARCH_W, f )
        if nSearchPx > dt.remain.w then nSearchPx = dt.remain.w
        if nSearchPx < 0 then nSearchPx = 0
        g_search->SetBounds( dt.TakeLeftPx( nSearchPx ) )
    end scope

    '' ---- the body -----------------------------------------------------------------
    scope
        dim as long nSplitPx = PsScaleBy( FM_SPLIT_W, f )
        dim as long nMin = PsScaleBy( FM_TOC_MIN, f )
        dim as long nMax = rcBody.w - nSplitPx - PsScaleBy( FM_PAGE_MIN, f )

        dim as long nTocPx = PsScaleBy( g_nTocW, f )
        '' Clamped, and the order matters: on a window too narrow for both minimums, nMax
        '' falls below nMin and the pane collapses to the minimum rather than going negative.
        if nTocPx > nMax then nTocPx = nMax
        if nTocPx < nMin then nTocPx = nMin
        if nTocPx > rcBody.w - nSplitPx then nTocPx = rcBody.w - nSplitPx
        if nTocPx < 0 then nTocPx = 0

        dim as PsDocker db = PsDocker( rcBody, f )
        g_toc->SetBounds( db.TakeLeftPx( nTocPx ) )
        dim as PsRect rcSpl = db.TakeLeftPx( nSplitPx )
        g_split->SetBounds( rcSpl )
        g_page->SetBounds( db.Fill() )

        '' The splitter is told where it is and how far it may travel, in the parent's
        '' pixels. SetPos is not a move -- it is the bar agreeing with the layout that just
        '' placed it, and without it the first drag jumps by however far the two disagree.
        g_split->SetRange( rcBody.x + nMin, rcBody.x + iif(nMax > nMin, nMax, nMin) )
        g_split->SetPos( rcSpl.x )
    end scope

    '' SetBounds only marks layout dirty. Off a surface -- the self-test -- nothing would
    '' ever run the children's OnLayout without this.
    surf.pRoot->EnsureLayout()
end sub


'' ---------------------------------------------------------------------------------------
'' APPLYING A SCALE IS THREE THINGS, AND MISSING ANY ONE READS AS "the UI is not scaled".
''
''   1. the FONT reopens at the scaled pixel size -- the text engine rasterises at a fixed
''      size and has no notion of scale, so nothing else will do this
''   2. surf.fScale, which is what PsDocker multiplies the band constants by
''   3. PropagateScaleChanged, because each widget caches its own factor and scales its
''      internal metrics from that -- a tree never told keeps 1.0 and paints unscaled padding
''      inside perfectly scaled bands
''
'' Written once and called from BOTH the windowed path and the self-test, deliberately. An
'' assertion that drives its own copy of the steps proves the steps work; it does not prove
'' the application performs them, which is the bug ideshell shipped and tikoshell inherited.
'' ---------------------------------------------------------------------------------------
sub ApplyScale( byref surf as PsSurface, byval fIn as single )
    dim as single f = fIn
    if f <= 0 then f = 1.0

    dim as long px = PsScaleBy( FM_FONT_PX, f )
    if px <> g_nFontPx then
        TE_Free( g_te )
        if TE_Init( g_te, strptr(g_sFont), px ) = 0 then
            print "F1Markdown: could not reopen the font at " & str(px) & "px"
        end if
        g_nFontPx = px
    end if

    '' AND THE PAGE'S TEN, which is a FOURTH thing and not the same as step 1. g_te is what
    '' the CONTROLS measure with; the document has its own set and reopening one does nothing
    '' for the other. The symptom is a perfectly scaled shell with an unscaled page in it.
    MdFontsApplyScale( f )

    surf.fScale = f
    if surf.pRoot then surf.pRoot->PropagateScaleChanged( f )
end sub


sub Check( byref sWhat as string, byval bOk as boolean, byref sNote as string = "" )
    if bOk then
        g_nPass += 1
        print "  ok      " & sWhat & iif(len(sNote) > 0, "  (" & sNote & ")", "")
    else
        g_nFail += 1
        print "  FAILED  " & sWhat & iif(len(sNote) > 0, "  (" & sNote & ")", "")
    end if
end sub


'' The six static faces live beside tiko.exe, NOT in PsPlatform's assets -- see the header.
'' Forward slashes throughout: fbc processes backslash escapes in this translation unit, so
'' "..\assets\f1markdown" would arrive with a TAB where \f1 was.
function FontDir() as string
    dim as string sExe = exepath()
    for i as integer = 0 to len(sExe) - 1
        if sExe[i] = asc("\") then sExe[i] = asc("/")
    next
    return sExe & "/assets/f1markdown/fonts/"
end function

function ThemeDir() as string
    dim as string sExe = exepath()
    for i as integer = 0 to len(sExe) - 1
        if sExe[i] = asc("\") then sExe[i] = asc("/")
    next
    return sExe & "/settings/themes/"
end function


'' ---------------------------------------------------------------------------------------
'' ASSERT THE BANDS AT ONE SCALE. Called twice, at 1.0 and at 1.75.
''
'' ABSOLUTE rects, not relations between them. tikoshell.bas records 21 green self-tests
'' passing while the window was visibly wrong, because every one of them asserted a RELATION
'' and all those relations hold perfectly at the wrong scale. So each band is checked against
'' PsScaleBy(<the design constant>, f) -- a number the layout would have to be right to
'' produce -- and the font against g_nFontPx, the size the engine is ACTUALLY open at, rather
'' than against a widget's ScaleY, which reads the surface's scale live and passes regardless
'' of what the tree was told.
'' ---------------------------------------------------------------------------------------
sub CheckBands( byref surf as PsSurface, byval f as single )
    dim as string sAt = " @" & str(f)
    dim as long W = surf.w, H = surf.h
    dim as long hTop = PsScaleBy( FM_TOOLBAR_H, f )
    dim as long hBot = PsScaleBy( FM_STATUS_H, f )
    dim as long wSpl = PsScaleBy( FM_SPLIT_W, f )

    Check "font is open at the scaled size" & sAt, _
          (g_nFontPx = PsScaleBy(FM_FONT_PX, f)), _
          "engine " & str(g_nFontPx) & ", want " & str(PsScaleBy(FM_FONT_PX, f))
    Check "surface carries the scale" & sAt, (surf.fScale = f)
    Check "the tree was told the scale" & sAt, (g_page->SurfaceScale() = f)

    Check "toolbar band is top-left" & sAt, _
          (g_nav->bounds.y >= 0) andalso (g_nav->bounds.y < hTop) andalso (g_nav->bounds.x >= 0)
    Check "status band is bottom-aligned" & sAt, _
          (g_status->bounds.y = H - hBot) andalso (g_status->bounds.h = hBot), _
          str(g_status->bounds.y) & "+" & str(g_status->bounds.h) & " of " & str(H)
    Check "status band is full width" & sAt, _
          (g_status->bounds.x = 0) andalso (g_status->bounds.w = W)

    '' The three body panes tile the band with no gap and no overlap. Checked as arithmetic
    '' on absolute edges rather than as "is to the left of".
    Check "toc starts at the left edge" & sAt, (g_toc->bounds.x = 0)
    Check "toc top is the toolbar bottom" & sAt, (g_toc->bounds.y = hTop), _
          str(g_toc->bounds.y) & " want " & str(hTop)
    Check "toc bottom is the status top" & sAt, _
          (g_toc->bounds.y + g_toc->bounds.h = H - hBot)
    Check "splitter abuts the toc" & sAt, _
          (g_split->bounds.x = g_toc->bounds.x + g_toc->bounds.w)
    Check "splitter is the scaled thickness" & sAt, (g_split->bounds.w = wSpl), _
          str(g_split->bounds.w) & " want " & str(wSpl)
    Check "page abuts the splitter" & sAt, _
          (g_page->bounds.x = g_split->bounds.x + g_split->bounds.w)
    Check "page reaches the right edge" & sAt, _
          (g_page->bounds.x + g_page->bounds.w = W), _
          str(g_page->bounds.x + g_page->bounds.w) & " of " & str(W)
    Check "page fills the band vertically" & sAt, _
          (g_page->bounds.y = hTop) andalso (g_page->bounds.y + g_page->bounds.h = H - hBot)
    Check "splitter agrees with the layout" & sAt, _
          (g_split->GetPos() = g_split->bounds.x), _
          "pos " & str(g_split->GetPos()) & ", bounds " & str(g_split->bounds.x)

    '' The left pane is the design width TIMES the scale -- the one assertion that fails if
    '' the splitter's pixels were ever stored back as design units without dividing.
    Check "toc is the scaled design width" & sAt, _
          (g_toc->bounds.w = PsScaleBy(g_nTocW, f)), _
          str(g_toc->bounds.w) & " want " & str(PsScaleBy(g_nTocW, f))
end sub

'' AFTER Check, because that is what the suites report through. See mdTests.inc for why they
'' do not use PsTest.
#include once "mdTests.inc"


'' ---------------------------------------------------------------------------------------
'' --dump-md. Reads one file and prints the block model. Deliberately runs before the
'' platform is started: parsing is pure data and must not need SDL, a window or a font, and
'' a mode that proves that is worth more than a comment claiming it.
'' ---------------------------------------------------------------------------------------
sub RunDumpMd( byref sPath as DWSTRING )
    dim as boolean bOk = false
    dim as string sUtf8 = MdReadUtf8( sPath, bOk )
    if bOk = false then
        print "F1Markdown: cannot read " & sPath.Utf8
        end 1
    end if

    dim as MdDoc doc
    MdParse( sUtf8, doc )

    print "-- " & sPath.Utf8 & " --"
    print str(doc.nBlk) & " blocks, " & str(doc.nInl) & " runs, " & _
          str(doc.nCell) & " cells, " & str(doc.nNote) & " unclassified"
    for i as long = 0 to doc.nBlk - 1
        print MdDumpBlock( doc, i )
    next
    for i as long = 0 to doc.nNote - 1
        print "!! line " & str(doc.note(i).nLine) & ": " & doc.note(i).sWhat & _
              " -- " & doc.note(i).sLine
    next
end sub


'' ---------------------------------------------------------------------------------------
'' --scan. Walks a root and reports, before the platform starts -- which is the assertion
'' that the index needs no SDL, no window and no font either.
'' ---------------------------------------------------------------------------------------
sub RunScan( byref sRoot as DWSTRING, byref sTopic as DWSTRING )
    dim as DWSTRING sR = PsPathCanonicalise( sRoot )
    if PsFileIsDir( sR ) = false then
        print "F1Markdown: not a folder -- " & sR.Utf8
        end 1
    end if

    dim as double t0 = timer
    MdIndexAddRoot( g_ix, PsPathName(sR).Utf8, sR )
    dim as double nMs = (timer - t0) * 1000

    print "-- " & sR.Utf8 & " --"
    print str(g_ix.nDocs) & " documents, " & str(g_ix.nTopic) & " tree nodes, " & _
          str(g_ix.nHead) & " headings, " & str(g_ix.nSkipped) & " unreadable"
    print "scanned in " & format(nMs, "0") & " ms"

    for i as long = 0 to g_ix.nTopic - 1
        dim as string sInd = space( g_ix.topic(i).nDepth * 2 )
        dim as string sKind = "  "
        if g_ix.topic(i).kind = MDX_DOCSET then sKind = "* "
        if g_ix.topic(i).kind = MDX_FOLDER then sKind = "+ "
        print sInd & sKind & g_ix.topic(i).sName
    next

    if len(sTopic) > 0 then
        print ""
        print "-- ranked for " & sTopic.Utf8 & " --"
        dim as long n = MdIndexSearch( g_ix, sTopic.Utf8, g_hits(), HITS_MAX )
        if n = 0 then
            print "  no match"
        else
            for i as long = 0 to n - 1
                if i >= 10 then
                    print "  ... and " & str(n - 10) & " more"
                    exit for
                end if
                dim as string sWhere = "title"
                if g_hits(i).nHead >= 0 then sWhere = "h" & _
                    str(g_ix.head(g_hits(i).nHead).nLevel) & " " & _
                    g_ix.head(g_hits(i).nHead).sText
                print "  " & str(g_hits(i).nScore) & "  " & _
                      g_ix.topic(g_hits(i).nTopic).sTitle & "   [" & sWhere & "]"
            next
        end if
    end if
end sub


'' ======================================================================== main
    dim as boolean bSelfTest = false
    dim as DWSTRING g_sDumpMd, g_sOpen, g_sScan
    for i as integer = 1 to __FB_ARGC__ - 1
        dim as string sArg = command(i)
        select case sArg
            case "--selftest" : bSelfTest = true
            case "--dump-md"  : if i < __FB_ARGC__ - 1 then g_sDumpMd = command(i + 1)
            case "--scan"     : if i < __FB_ARGC__ - 1 then g_sScan   = command(i + 1)
            case "--open"     : if i < __FB_ARGC__ - 1 then g_sOpen   = command(i + 1)
            case "--topic"    : if i < __FB_ARGC__ - 1 then g_sTopic  = command(i + 1)
            case "--docset"   : if i < __FB_ARGC__ - 1 then g_sDocset = command(i + 1)
            case "--root"     : if i < __FB_ARGC__ - 1 then g_sRoot   = command(i + 1)
            case "--theme"    : if i < __FB_ARGC__ - 1 then g_sTheme  = command(i + 1)
        end select
    next

    '' BEFORE PsPlatformInit, and that is the assertion: the document model is pure data and
    '' needs no platform at all.
    if len(g_sDumpMd) > 0 then
        RunDumpMd( g_sDumpMd )
        end 0
    end if
    if len(g_sScan) > 0 then
        RunScan( g_sScan, g_sTopic )
        end 0
    end if

    if PsPlatformInit() = false then
        print "F1Markdown: the platform would not start"
        end 1
    end if
    if PsRenderInit() = false then
        print "F1Markdown: the render backend would not start"
        end 1
    end if

    g_sFont = FontDir() & "SourceSans3-Regular.ttf"
    if TE_Init( g_te, strptr(g_sFont), FM_FONT_PX ) = 0 then
        print "F1Markdown: the text engine would not start -- " & g_sFont
        end 1
    end if
    g_nFontPx = FM_FONT_PX

    '' The page's OWN ten, separate from the one the CONTROLS measure with. The controls take
    '' their engine from surf.pText and there is exactly one of those; the document needs ten
    '' and picks per run.
    if MdFontsInit( FontDir(), 1.0 ) = false then
        print "F1Markdown: missing font faces -- " & MdFontsMissing()
        print "  looked in " & FontDir()
        end 1
    end if

    dim as PsSurface surf
    g_pSurf     = @surf
    surf.fScale = 1.0
    '' TWO FIELDS, AND BOTH ARE REQUIRED. Missing surf.pText makes every caption measure 0;
    '' missing g_pnt.pText draws nothing. They are separate fields on separate objects and
    '' neither derives the other.
    surf.pText  = cptr( PsTextEngine_ ptr, @g_te )
    g_pnt.pText = @g_te
    surf.Resize( FM_W, FM_H )

    BuildTree( surf )

    '' ---- the theme ------------------------------------------------------------------
    '' tiko's own .theme format, unchanged -- PsTheme.bi states the format is tiko's
    '' deliberately and almost verbatim, same keys, same roles, same key -> role -> default
    '' resolution. So the fourteen files in settings/themes drive this binary as they are.
    scope
        dim as DWSTRING sTheme = g_sTheme
        if len(sTheme) = 0 then sTheme.Utf8 = ThemeDir() & "default_dark.theme"
        dim as long n = PsThemeLoadFile( sTheme )
        if n = 0 then print "F1Markdown: no theme loaded from " & sTheme.Utf8
        PsThemeApply( surf.pRoot )
    end scope

    '' ---- the corpus --------------------------------------------------------------------
    '' SYNCHRONOUS, and measured rather than assumed. The plan called for a background
    '' thread; PsPlatform has no thread abstraction at all, and the scan turned out to cost
    '' a fraction of the window's own startup -- so a thread would have been a second way to
    '' get things wrong in exchange for nothing. The cost is printed on every run, so the day
    '' a corpus makes it visible, it says so rather than just feeling slow.
    scope
        dim as double t0 = timer
        if len(g_sRoot) > 0 then
            dim as DWSTRING sR = PsPathCanonicalise( g_sRoot )
            MdIndexAddRoot( g_ix, PsPathName(sR).Utf8, sR )
        else
            '' tiko's own docs, beside the exe. Phase 5's f1markdown.ini replaces this with a
            '' named list of roots; until then one default beats none, because a viewer that
            '' opens empty looks broken.
            dim as DWSTRING sDocs
            sDocs.Utf8 = ThemeDir() & "../docs"
            sDocs = PsPathCanonicalise( sDocs )
            if PsFileIsDir( sDocs ) then MdIndexAddRoot( g_ix, "tiko", sDocs )
        end if
        FillTree()
        dim as DWSTRING sCount
        sCount.Utf8 = str(g_ix.nDocs) & " topics"
        g_status->SetText( 1, sCount )
        print "F1Markdown: " & str(g_ix.nDocs) & " topics, " & str(g_ix.nHead) & _
              " headings in " & format((timer - t0) * 1000, "0") & " ms" & _
              iif(g_ix.nSkipped > 0, " (" & str(g_ix.nSkipped) & " unreadable)", "")
    end scope

    '' ---- the first page ------------------------------------------------------------------
    scope
        if len(g_sOpen) > 0 then
            '' --open renders one file whether or not it is in the index, which is what makes
            '' it useful for looking at a document the corpus does not contain.
            dim as boolean bOk = false
            dim as string sUtf8 = MdReadUtf8( g_sOpen, bOk )
            if bOk then
                g_page->SetMarkdown( sUtf8, PsPathDirWithSep(g_sOpen).Utf8 )
                g_status->SetText( 0, g_sOpen )
            else
                print "F1Markdown: cannot read " & g_sOpen.Utf8
            end if
        elseif len(g_sTopic) > 0 then
            '' THE F1 PATH. The topic goes into the search box as well as being resolved, so
            '' what the viewer opened and why is visible rather than magic -- and the box is
            '' then ready to be edited if it guessed wrong.
            g_search->SetText( g_sTopic )
            dim as long n = MdIndexSearch( g_ix, g_sTopic.Utf8, g_hits(), HITS_MAX )
            if n > 0 then
                FillSearchResults( g_sTopic.Utf8 )
                OpenTopic( g_hits(0).nTopic, true )
                Say( str(n) & " match" & iif(n = 1, "", "es") & " for " & g_sTopic.Utf8 )
            else
                Say( "no match for " & g_sTopic.Utf8 )
            end if
        else
            dim as long nFirst = -1
            for i as long = 0 to g_ix.nTopic - 1
                if g_ix.topic(i).kind = MDX_DOC then nFirst = i : exit for
            next
            if nFirst >= 0 then
                OpenTopic( nFirst, true )
            else
                '' An empty corpus falls back to the demo rather than a blank pane, and says
                '' so, because "no documents found" is a configuration problem the user can
                '' act on and an empty window is not.
                dim as string sNone = ""
                g_page->SetMarkdown( MdDemoMarkdown(), sNone )
                Say( "no documents found -- showing the built-in demo" )
            end if
        end if
    end scope

    LayoutAll( surf )

    '' ---------------------------------------------------------------------- selftest
    if bSelfTest then
        print "--- F1Markdown selftest ---"

        Check "the tree is built", (surf.pRoot <> 0)
        Check "six children", (surf.pRoot->ChildCount() = 6), _
              str(surf.pRoot->ChildCount())
        Check "the surface is windowless", (surf.hWin = 0)
        Check "the font file opened", (g_te.lineHeight > 0), _
              "lineHeight " & str(g_te.lineHeight)

        '' THE FONT MUST NOT BE VARIABLE. A variable file loads and renders its default
        '' instance, so the bold face would silently not be bold and nothing would error.
        '' Phase 3 opens all six; phase 1 asserts the one it opened is a real static face by
        '' checking the metrics came out at the size asked for.
        Check "metrics match the requested size", _
              (g_te.ascent > 0) andalso (g_te.descent >= 0) andalso _
              (g_te.lineHeight >= g_te.ascent), _
              "asc " & str(g_te.ascent) & " desc " & str(g_te.descent) & _
              " lh " & str(g_te.lineHeight)

        print "  -- at 1.0 --"
        ApplyScale( surf, 1.0 )
        LayoutAll( surf )
        CheckBands( surf, 1.0 )

        '' 1.75 IS THE POINT OF THIS TEST. Everything above passes at 1.0 whether or not a
        '' single scaling step exists; the bands, the font and the tree only diverge here.
        print "  -- at 1.75 --"
        ApplyScale( surf, 1.75 )
        LayoutAll( surf )
        CheckBands( surf, 1.75 )

        '' And back, because ApplyScale short-circuits when the pixel size is unchanged and a
        '' one-way test never exercises the return path.
        print "  -- back at 1.0 --"
        ApplyScale( surf, 1.0 )
        LayoutAll( surf )
        CheckBands( surf, 1.0 )

        '' The document model, which owns no widget and cares about no scale. Run last so a
        '' geometry failure is not buried under a hundred parser lines.
        MdRunParserTests()
        MdRunHiliteTests()
        MdRunIndexTests()
        MdRunFontTests()
        MdRunLayoutTests()

        print "--- " & str(g_nPass) & " passed, " & str(g_nFail) & " failed ---"
        MdFontsFree()
        MdLayoutFreeImages()
        TE_Free( g_te )
        PsPlatformShutdown()
        if g_nFail > 0 then end 1
        end 0
    end if

    '' ---------------------------------------------------------------------- the window
    dim as PsSurfaceDesc desc
    desc.kind       = PSSURF_TOPLEVEL
    desc.w          = FM_W
    desc.h          = FM_H
    desc.title.Utf8 = "F1Markdown"
    desc.bResizable = true
    dim as PsSurfaceHandle win = g_plat.window.Create( @desc )
    if win = 0 then
        print "F1Markdown: no window"
        end 1
    end if

    '' MARRYING THE SURFACE TO THE WINDOW IS FOUR STEPS. Without hWin, PsPopupHost.OpenAt
    '' silently declines and every popup this app grows later never appears.
    surf.hWin = win

    dim as single f = g_plat.window.ScaleOf( win )
    if f <= 0 then f = 1.0

    '' 1. Font, fScale and the tree, all three, through the one routine the self-test also
    ''    calls. Doing it by hand here is how ideshell missed two of the three.
    ApplyScale( surf, f )

    '' 2. The window is sized from the DESIGN size TIMES the scale, then clamped to the
    ''    display -- and the clamp compares PIXELS with PIXELS. ideshell clamps its design
    ''    size against usable pixels, which are the same number only at 100%.
    scope
        dim as long nWantW = PsScaleBy( FM_W, f )
        dim as long nWantH = PsScaleBy( FM_H, f )
        dim as PsMonitorInfo mi
        if g_plat.monitors.Describe(0, @mi) then
            dim as long wMax = clng(mi.usable.w * 0.9)
            dim as long hMax = clng(mi.usable.h * 0.9)
            if (wMax > 320) andalso (nWantW > wMax) then nWantW = wMax
            if (hMax > 240) andalso (nWantH > hMax) then nWantH = hMax
        end if
        g_plat.window.SetSize( win, nWantW, nWantH )
    end scope

    '' 3. The surface takes the size the window ACTUALLY got, not the size requested.
    scope
        dim as PsSize sz
        g_plat.window.GetSize( win, @sz )
        surf.Resize( sz.w, sz.h )
    end scope

    LayoutAll( surf )
    surf.InvalidateAll()

    dim as long bufW = surf.w, bufH = surf.h
    dim as ulong ptr pix = callocate( bufW * bufH * 4 )
    g_plat.window.Show( win, true )

    print "F1Markdown: " & str(surf.w) & "x" & str(surf.h) & " at scale " & str(surf.fScale)

    '' ---------------------------------------------------------------------- the pump
    '' THE SEVEN STEPS, in this order. Every host in PsPlatform writes this loop out longhand
    '' -- there is no PsEventLoop -- so it is copied from ideshell/tikoshell rather than
    '' invented, and the order is not negotiable:
    ''
    ''   Ticks -> PsTimerService -> Wait(PsTimerWaitMs) -> route by SURFACE, then by kind
    ''   -> PsSurfaceSyncCursor -> paint if damaged.
    ''
    '' There is no menu host in this binary yet, so the popup-routing step of that sequence is
    '' absent rather than forgotten; phase 5's context menu puts it back.
    dim as PsEvent ev
    dim as boolean bRunning = true
    do while bRunning
        dim as ulongint nNow = g_plat.events.Ticks()
        PsTimerService( nNow )

        if g_plat.events.Wait( @ev, PsTimerWaitMs(nNow, 30) ) then
            '' Surface 0 means "not addressed to a window" -- quit, and anything global.
            dim as boolean bMine = (ev.surface = win)
            if ev.surface = 0 then bMine = true

            select case ev.kind
                case PSEV_QUIT
                    bRunning = false

                case PSEV_CLOSE
                    if bMine then bRunning = false

                case PSEV_RESIZE
                    if bMine then
                        '' Dispatch updates surf.fScale and re-propagates the tree when the
                        '' window moves to a display with another factor -- but it cannot
                        '' reopen the FONT, which the engine holds at a fixed pixel size. So
                        '' the scale is read back and the font follows it. Without this,
                        '' dragging between a 100% and a 175% monitor rescales every rect and
                        '' leaves the text behind.
                        dim as single fWas = surf.fScale
                        surf.Dispatch( @ev )
                        if surf.fScale <> fWas then ApplyScale( surf, surf.fScale )
                        if (surf.w <> bufW) orelse (surf.h <> bufH) then
                            bufW = surf.w : bufH = surf.h
                            if pix <> 0 then deallocate(pix)
                            pix = callocate( bufW * bufH * 4 )
                        end if
                        LayoutAll( surf )
                        surf.InvalidateAll()
                    end if

                case else
                    if bMine then surf.Dispatch( @ev )
            end select

            PsSurfaceSyncCursor( surf )
        end if

        if (surf.DamageCount() > 0) andalso (pix <> 0) then
            g_pnt.BeginFrame( pix, bufW, bufH, bufW * 4 )
            PsSurfacePaint( surf, g_pnt )
            g_pnt.EndFrame()
            dim as PsRect dmg = surf.DamageBounds()
            g_plat.window.Present( win, pix, bufW, bufH, bufW * 4, @dmg )
            surf.ClearDamage()
        end if
    loop

    if pix <> 0 then deallocate(pix)
    g_plat.window.Destroy( win )
    MdFontsFree()
    MdLayoutFreeImages()
    TE_Free( g_te )
    PsPlatformShutdown()
    end 0
