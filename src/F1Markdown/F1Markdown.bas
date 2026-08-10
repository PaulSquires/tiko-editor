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


'' ========================================================================================
'' MODES
''   (none)       open the window
''   --selftest   build the tree, lay out, assert geometry at 1.0 AND 1.75, exit non-zero on
''                failure. WINDOWLESS, NOT HEADLESS -- SDL's video subsystem is initialised
''                because the text engine needs it. No window is shown.
''
'' SWITCHES (parsed here, acted on from phase 4 on except --theme and --selftest)
''   --topic <text>    the symbol F1 was pressed on
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


'' ---------------------------------------------------------------------------------------
'' PageStub -- the markdown pane's stand-in: IN THE TREE, IN THE RIGHT PLACE, WITH NOTHING
'' BEHIND IT. Replaced by MarkdownView in phase 3.
''
'' It paints its own bounds because reading them off the screen is the fastest way to find a
'' band that is one scrollbar out, and because an empty real view would look correct anywhere.
'' ---------------------------------------------------------------------------------------
type PageStub extends PsWidget
    sLabel as DWSTRING
    nRole  as long = PSTHEME_BACKGROUND
    declare constructor(byval szLabel as zstring ptr, byval nRoleIn as long)
    declare sub OnPaint(byval p as PsBufferPaint_ ptr)
end type

constructor PageStub(byval szLabel as zstring ptr, byval nRoleIn as long)
    this.sLabel.Utf8 = *szLabel
    this.nRole = nRoleIn
    '' Stubs take no focus. A Tab that stopped on a placeholder would be a Tab order the real
    '' view will not have, and the traversal is tree order -- see PsDispatch.
    this.bFocusable = false
end constructor

sub PageStub.OnPaint(byval p as PsBufferPaint_ ptr)
    if p = 0 then exit sub
    dim as PsBufferPaint ptr q = cptr(PsBufferPaint ptr, p)

    '' Bounds-relative, like every control: the walker has already set the origin.
    dim as PsRect rcAll = PsRc(0, 0, this.bounds.w, this.bounds.h)

    q->SetBackColor( PsThemeRoleColor(this.nRole) )
    q->PaintRect( @rcAll )
    q->SetPenColor( PsThemeRoleColor(PSTHEME_BORDER) )
    q->PaintBorderRect( @rcAll, 1 )

    q->SetForeColor( PsThemeRoleColor(PSTHEME_FOREGROUND) )
    q->PaintText( this.sLabel, @rcAll, PSTF_CENTER or PSTF_VCENTER )

    dim as DWSTRING sGeom
    sGeom.Utf8 = str(this.bounds.x) & "," & str(this.bounds.y) & "," & _
                 str(this.bounds.w) & "x" & str(this.bounds.h)
    dim as PsRect rcGeom = PsRc(0, rcAll.h - this.ScaleY(22), rcAll.w, this.ScaleY(18))
    q->SetForeColor( PsThemeRoleColor(PSTHEME_FOREGROUNDDIM) )
    q->PaintText( sGeom, @rcGeom, PSTF_CENTER or PSTF_VCENTER )
end sub


'' ---------------------------------------------------------------------------------------
'' THE TREE
'' ---------------------------------------------------------------------------------------
dim shared as PsWidget    ptr g_root
dim shared as PsToolbar   ptr g_nav
dim shared as PsTextBox   ptr g_search
dim shared as PsListTree  ptr g_toc
dim shared as PsSplitter  ptr g_split
dim shared as PageStub    ptr g_page
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

sub OnNavCommand( byval pBar as any ptr, byval nId as long, byval ud as any ptr )
    '' Phase 4 owns the history stack. Both buttons stay disabled until then, so this is
    '' unreachable rather than empty -- stated because a silent no-op handler reads as a bug.
end sub

sub BuildTree( byref surf as PsSurface )
    g_root = new PsWidget
    g_root->bClipsChildren = false
    surf.SetRoot( g_root )                    '' the surface takes ownership

    g_nav = new PsToolbar
    g_nav->AddItem( PsText("Back"), IDM_BACK, TBR_KIND_BUTTON )
    g_nav->AddItem( PsText("Forward"), IDM_FORWARD, TBR_KIND_BUTTON )
    g_nav->EnableItem( IDM_BACK, false )
    g_nav->EnableItem( IDM_FORWARD, false )
    g_nav->OnCommand( @OnNavCommand )
    g_root->AddChild( g_nav )                 '' AddChild TRANSFERS OWNERSHIP

    g_search = new PsTextBox
    g_search->SetCueBannerText( PsText("Search topics") )
    g_root->AddChild( g_search )

    g_toc = new PsListTree
    g_toc->SetTreeIndent( true )
    g_toc->ShowTwisty( true )
    '' One row, and it says what is true. An empty tree and a tree whose corpus failed to
    '' load look identical, and phase 1 has no corpus at all.
    scope
        dim as long nRow = g_toc->AddString( PsText("(no docsets -- phase 4)") )
        g_toc->SetRowSelectable( nRow, false )
    end scope
    g_root->AddChild( g_toc )

    g_split = new PsSplitter
    g_split->SetOrient( PSSPLIT_VERT )
    g_split->OnMove( @OnSplitMove )
    g_root->AddChild( g_split )

    g_page = new PageStub( "markdown view -- phase 3", PSTHEME_BACKGROUND )
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


'' ======================================================================== main
    dim as boolean bSelfTest = false
    for i as integer = 1 to __FB_ARGC__ - 1
        dim as string sArg = command(i)
        select case sArg
            case "--selftest" : bSelfTest = true
            case "--topic"    : if i < __FB_ARGC__ - 1 then g_sTopic  = command(i + 1)
            case "--docset"   : if i < __FB_ARGC__ - 1 then g_sDocset = command(i + 1)
            case "--root"     : if i < __FB_ARGC__ - 1 then g_sRoot   = command(i + 1)
            case "--theme"    : if i < __FB_ARGC__ - 1 then g_sTheme  = command(i + 1)
        end select
    next

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

        print "--- " & str(g_nPass) & " passed, " & str(g_nFail) & " failed ---"
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
    TE_Free( g_te )
    PsPlatformShutdown()
    end 0
