'' ========================================================================================
'' tikoshell -- phase 7c's shell binary. A window, tiko's menubar, and one stubbed body.
''
'' ---- WHAT THIS BINARY IS FOR -----------------------------------------------------------
''
'' D2 was decided as Shape A: SDL3 on both platforms, no second Win32 backend, and frmMain
'' becomes a PsSurface + widget tree. That conversion is 49 forms and ~45,000 lines and is
'' un-shippable in the middle by construction, so it is built HERE, as a second translation
'' unit, rather than inside tiko.bas. tiko.exe keeps building unchanged from tiko.bas at
'' every commit on this branch -- which is the only regression guard a branch that cannot be
'' merged can have, and it costs nothing.
''
'' ---- WHY A SECOND TU AND NOT A PsPlatform DEMO -----------------------------------------
''
'' A demo would be cheaper: PsPlatform's build driver is convention-over-configuration and
'' picks up demos/<name>/<name>.bas with no edit to the driver at all. The corollary is that
'' it cannot be added WITHOUT enrolling it in `build all` -- so a shell mid-edit would break
'' PsPlatform's own gate for reasons that have nothing to do with PsPlatform, and the
'' un-shippable half of this work would live on the repository that has to stay shippable.
'' It also has to include tiko's app/ layer, which would invert the dependency.
''
'' ---- THE THING THIS FILE ESCAPES, AND IT IS THE POINT ----------------------------------
''
'' NO `namespace PsC`. tiko.bas fences PsPlatform's UI headers inside one, and its comment
'' (tiko.bas:76-90) records that the fence outlived the DWSTRING problem it was built for:
'' BOTH sides have a PsBufferPaint, and PsCore's paint backend and tiko's PsImage both define
'' PsBgrToArgb, so lifting those six headers to global scope inside tiko produces 17
'' `Duplicated definition` errors and one `UDT's with methods must have unique names`.
''
'' None of that applies here, because this TU includes none of tiko's frm* or Ps* files. So
'' the shell gets PsPlatform's UI at global scope and carries zero PsC. prefixes on day one.
'' That is 7c's end state, obtained free, and it is unobtainable inside tiko.bas.
''
'' ---- SDL3, WHICH tiko HAS NEVER LINKED ------------------------------------------------
''
'' The Win32 host bridge exists precisely so tiko keeps its own window and message loop, and
'' tiko.exe does not call PsPlatformInit at any point. This binary needs one extra include
'' root and no new library flag -- see _compile_shell.bat.
''
'' ---- WHAT IS HERE, AND WHAT IS NOT -----------------------------------------------------
''
'' HERE: the seven-step pump, a window sized and scaled from the display, tiko's own eight
'' menu titles built out of app/modMenuDefinitions, a status bar, a tab bar, and ONE stub
'' standing in for the whole document area.
''
'' NOT HERE: any real dock panel, the editor, accelerators, modal dialogs, or a document
'' model. The layout is three docked bands, not frmMain's band walk -- that is commit 8.
''
'' ---- THREE DEFECTS COPIED FROM demos/ideshell, AND FOUND THE HARD WAY ------------------
''
'' ideshell is the nearest prior art and this file started as a copy of its setup. It has
'' three bugs, all of which came across, and each was found by something different:
''
''   1. IT NEVER ANSWERS OnOpenRequest, so its menubar drops nothing. Found by reading
''      PsMenuBar.bi, which says the bar asks rather than opens. Fixed from gallery2.
''   2. IT NEVER SETS surf.hWin. Found by reading PsModalHost, which needs it to parent a
''      dialog. Costs nothing today and would cost a parentless message box later.
''   3. IT NEVER SCALES. Found by LOOKING AT THE WINDOW -- nothing else would have. The
''      bands were right, because PsDocker takes the factor as an argument; the font and
''      every widget's internal metrics were not. See ApplyScale.
''
'' The third is the one worth remembering: the self-test was 21 green while the UI was
'' visibly wrong, because everything it asserted was a RELATION between rectangles and all
'' of those relations hold perfectly at the wrong scale.
'' ========================================================================================

#include once "crt/stddef.bi"

'' ---- PsCore's core layer ---------------------------------------------------------------
'' The same five tiko.bas takes, and here they need no ordering note: tiko has to put them
'' AFTER AfxNova's headers because both declare a DWSTRING and the unqualified name means
'' whichever came last. There is no AfxNova in this TU, so PsCore's is the only DWSTRING
'' there has ever been and nothing can shadow it.
#include once "core/DWString.inc"
#include once "core/PsStr.inc"
#include once "core/PsPath.inc"
#include once "core/PsFile.inc"
#include once "core/PsEncoding.inc"

'' ---- PsPlatform, AT GLOBAL SCOPE -------------------------------------------------------
'' No `namespace PsC`. See the header above for why this TU can do that and tiko.bas cannot.
#include once "platform/PlatformInit.inc"
#include once "ui/core/PsDispatch.inc"
#include once "ui/core/PsPaintWalk.inc"
#include once "ui/core/PsCursorSync.inc"
#include once "ui/core/PsMenuHost.inc"
#include once "ui/core/PsTheme.inc"
#include once "ui/core/PsLayout.inc"
#include once "ui/controls/PsMenuBar.inc"
#include once "ui/controls/PsStatusBar.inc"
#include once "ui/controls/PsTabBar.inc"
#include once "scintilla/PsTextEngineC.inc"

'' ---- tiko's application layer ----------------------------------------------------------
'' Every app\*.bi that tiko.bas includes, IN tiko.bas's OWN ORDER. That order is
'' load-bearing and alphabetical is wrong -- modFormat.bi must precede clsConfig.bi, and
'' clsSymbolDb.bi needs FBCP_KIND_* out of fbcParser.bi. _check_app_standalone.bat does not
'' hardcode the list either; it greps it back out of tiko.bas for exactly this reason, so
'' tiko.bas stays the single place that has to get it right.
''
'' THIS IS THE FIRST TIME THE LAYER HAS BEEN COMPILED AS ONE TRANSLATION UNIT. The gate
'' compiles each app\*.inc SEPARATELY, against PsCore alone -- so what it proves is that no
'' file needs AfxNova, not that the sixteen headers agree with each other, and not that any
'' of them survives PsPlatform's UI being in scope as well. Both of those are new here.
#include once "app/fbcParser.bi"
#include once "app/debugParser.bi"
#include once "app/modLocalization.bi"
#include once "app/modPaths.bi"
#include once "app/modMenuIds.bi"
#include once "app/modMenuDefinitions.bi"
#include once "app/modAppState.bi"
#include once "app/modNavHistory.bi"
#include once "app/modFormat.bi"
#include once "app/clsConfig.bi"
'' The constructor. app/clsConfig.bi carries `dim shared gConfig`, so including the header
'' instantiates the object and this TU has to link a constructor for it. It used to live in
'' src/clsConfig.inc -- the shell -- which is what made the app layer unlinkable on its own,
'' and is the one defect this commit found. See app/clsConfig.inc for the whole story.
#include once "app/clsConfig.inc"
#include once "app/modProjectFolders.bi"
#include once "app/clsSymbolDb.bi"
#include once "app/modUnusedSymbols.bi"
#include once "app/modIniParse.bi"
#include once "app/modEncodingSelfTest.bi"
#include once "app/modSaveSelfTest.bi"

'' ---- app-layer BODIES the shell drives directly ----------------------------------------
'' modTextFile and modLocalization are what turn L(id) from an empty string into a phrase;
'' modMenuDefinitions is the menu vocabulary itself -- getMenuText, getMenuAccelText and
'' buildTopMenuDefinitions, which fill gTopMenu with the whole static structure.
''
'' All three moved into app/ for this commit, and all three were already PsCore-only. See
'' app/modLocalization.inc for why the loader in particular had to come down.
#include once "app/modTextFile.inc"
#include once "app/modLocalization.inc"
#include once "app/modAppState.inc"

'' ---- ONE STUB, AND IT IS TEMPORARY BY DESIGN -------------------------------------------
'' modMenuDefinitions.inc's createToolsMenuShortcut composes a User Tools shortcut LABEL by
'' asking the key vocabulary what a stored key name means. That lives in modKeyBindings.inc,
'' shell-side, and it is NOT movable the way the localization loader was: it reaches
'' frmKeyboard_AccelKeyToValue, which resolves OEM key names through
'' VkKeyScanEx(GetKeyboardLayout(0)) -- live Win32, and layout-dependent besides.
''
'' That function is exactly what commits 5-7 replace. PsKey grows the missing numpad and
'' punctuation keys, PsAccel takes over the vocabulary with PHYSICAL rather than
'' layout-dependent semantics (the decision taken on 2026-08-09), and this stub goes with it.
''
'' UNTIL THEN, USER TOOL MENU ROWS RENDER WITH NO SHORTCUT TEXT. Returning 0 is what the real
'' function returns for an unrecognised name, so the label is blank rather than wrong. It is
'' also the third entry on _check_app_standalone's link-debt baseline, which is where the
'' obligation is recorded rather than only here.
#include once "app/modMenuDefinitions.inc"

'' NOT `private`, and AFTER the include: app/modMenuDefinitions.inc:22 pulls
'' ../modKeyBindings.bi -- the app layer reaching UP into the shell by relative path, which
'' no token ratchet can see because a path is not an identifier -- so the declaration is
'' already in scope by the time this is reached and a second one would collide.
function KeyBindings_PickListKeyToValue( byval wszString as DWSTRING ) as long
    return 0
end function


'' ========================================================================================
'' MODES
''   (none)       open the window
''   --selftest   build the tree, lay out, assert geometry, print a count, exit non-zero on
''                failure. WINDOWLESS, NOT HEADLESS -- SDL's video subsystem is initialised
''                because the text engine needs it. No window is shown.
'' ========================================================================================

'' DESIGN UNITS, not pixels. Clamped to the display before the window is created; see below.
const SH_W = 1100
const SH_H = 700

'' Band heights, DESIGN units. tiko does not carry constants for these -- frmMain reads the
'' heights back OUT of the windows with AfxGetWindowHeight, because each control sizes itself
'' from its font. A widget tree cannot do that before it is laid out, so the shell states
'' them, and commit 8 reconciles them against the oracle's measured menubar-h / statusbar-h /
'' toptabs-h rather than against these guesses.
const SH_MENUBAR_H = 30
const SH_TABS_H    = 36
const SH_STATUS_H  = 26

'' The UI font, in DESIGN pixels. Reopened at PsScaleBy(SH_FONT_PX, scale) once the window
'' reports its display's factor -- the text engine rasterises at a fixed pixel size and has
'' no idea what scale means, so this is the host's job and nothing else does it.
const SH_FONT_PX = 14

dim shared as PsTextEngine  g_te
dim shared as PsBufferPaint g_pnt
dim shared as PsSurface ptr g_pSurf
dim shared as PsMenuHost    g_menus
dim shared as long g_nPass, g_nFail

'' Set by OnBarOpen. The menubar does not open anything itself -- it ASKS -- so the only
'' thing a windowless run can check is that something answered, and with what.
dim shared as long g_nBarOpenCalls
dim shared as any ptr g_pLastBarMenu

'' The font path, shared because ApplyScale reopens the engine and both the windowed path
'' and the self-test go through it.
dim shared as string g_sFont
'' The pixel size the engine is currently open at. Tracked here because the text engine has
'' no query for it -- and reopening it unconditionally on every resize would rebuild the
'' atlas for nothing.
dim shared as long g_nFontPx


'' ---------------------------------------------------------------------------------------
'' ShellStub -- a panel that is IN THE TREE, IN THE RIGHT PLACE, WITH NOTHING BEHIND IT.
''
'' Every dock panel 7c has to place starts as one of these. Deliberately NOT an empty real
'' control: an empty PsListTree looks perfectly correct in the wrong band, whereas a rect
'' that paints "OUTPUT 0,486,1100,160" tells you where it actually is. The bounds are drawn
'' because reading them off the screen is the fastest way to find a band that is one
'' scrollbar out.
'' ---------------------------------------------------------------------------------------
type ShellStub extends PsWidget
    sName as DWSTRING
    role  as long = PSTHEME_BACKGROUNDALT
    declare constructor(byval szName as zstring ptr, byval nRole as long)
    declare sub OnPaint(byval p as PsBufferPaint_ ptr)
end type

constructor ShellStub(byval szName as zstring ptr, byval nRole as long)
    this.sName.Utf8 = *szName
    this.role = nRole
    '' Stubs take no focus. A Tab that stopped on a placeholder would be a Tab order the
    '' real panel will not have, and the traversal is tree order -- see PsDispatch.
    this.bFocusable = false
end constructor

sub ShellStub.OnPaint(byval p as PsBufferPaint_ ptr)
    if p = 0 then exit sub

    '' Bounds-relative, like every control: the walker has already set the origin.
    dim as PsRect rcAll
    rcAll.x = 0 : rcAll.y = 0 : rcAll.w = this.bounds.w : rcAll.h = this.bounds.h

    p->SetBackColor( PsThemeRoleColor(this.role) )
    p->PaintRect( @rcAll )
    p->SetPenColor( PsThemeRoleColor(PSTHEME_BORDER) )
    p->PaintBorderRect( @rcAll, 1 )

    p->SetForeColor( PsThemeRoleColor(PSTHEME_FOREGROUND) )
    p->PaintText( this.sName, @rcAll, PSTF_CENTER or PSTF_VCENTER )

    dim as DWSTRING sGeom
    sGeom.Utf8 = str(this.bounds.x) & "," & str(this.bounds.y) & _
                 "," & str(this.bounds.w) & "x" & str(this.bounds.h)
    dim as PsRect rcGeom = rcAll
    rcGeom.y = rcAll.h - this.ScaleY(18) - this.ScaleY(4)
    rcGeom.h = this.ScaleY(18)
    p->SetForeColor( PsThemeRoleColor(PSTHEME_FOREGROUNDDIM) )
    p->PaintText( sGeom, @rcGeom, PSTF_CENTER or PSTF_VCENTER )
end sub


'' ---------------------------------------------------------------------------------------
'' THE MENUS, BUILT FROM tiko's OWN VOCABULARY.
''
'' gTopMenu is a FLAT array of (nParentID, nID, nChildID, isDisabled, isSeparator) rows that
'' buildTopMenuDefinitions fills; the tree is implied by nParentID. Captions come from
'' getMenuText(id), which returns caption and accelerator packed with a chr(9) between them,
'' and which resolves through L() -- so nothing here renders until a .lang file is loaded.
''
'' CAPS CHECKED BEFORE BUILDING AGAINST THEM, because overflowing one silently drops rows:
''   8 bar titles          vs PSMB_MAX_ITEMS   = 32
''   20 items (View, the largest dropdown) vs PSMENU_MAX_ITEMS = 128
''   3 levels deep         vs PSMENU_MAX_DEPTH = 8
'' MRU is a SUBMENU rather than an inline block, and gConfig.MRU is fixed at 10, so the File
'' menu does not grow with use. gConfig.Tools IS unbounded -- `Tools(any)` -- so a user with
'' more than 128 user tools would overflow the Tools popup. Recorded, not fixed: it is a
'' pre-existing property of tiko's own menu and nothing here makes it worse.
'' ---------------------------------------------------------------------------------------
dim shared as ShellStub  ptr g_body
dim shared as PsMenuBar  ptr g_menubar
dim shared as PsStatusBar ptr g_status
dim shared as PsTabBar   ptr g_tabs

'' getMenuText packs "caption" chr(9) "accel". Split rather than shown raw -- PsPopupMenu
'' paints the accelerator right-aligned in its own column and takes it as a third argument.
sub SplitMenuText( byval nId as long, byref sCap as DWSTRING, byref sAccel as DWSTRING )
    dim as DWSTRING wszText = getMenuText( nId )
    dim as long i = PsInStr( wszText, chr(9) )
    if i > 0 then
        sCap   = PsLeft( wszText, i - 1 )
        sAccel = PsMid( wszText, i + 1 )
    else
        sCap   = wszText
        sAccel = ""
    end if
end sub

function BuildDropDown( byval nParentId as long ) as PsPopupMenu ptr
    dim as PsPopupMenu ptr pM = new PsPopupMenu
    for i as long = lbound(gTopMenu) to ubound(gTopMenu)
        if gTopMenu(i).nParentID <> nParentId then continue for
        if gTopMenu(i).isSeparator then
            pM->AddSeparator()
        else
            dim as DWSTRING sCap, sAccel
            SplitMenuText( gTopMenu(i).nID, sCap, sAccel )
            dim as long idx = pM->AddItem( sCap, gTopMenu(i).nID, sAccel )
            '' A row with a child id is a SUBMENU. Recursion is what makes the depth cap
            '' relevant; tiko goes three levels and the cap is eight.
            if (idx >= 0) andalso (gTopMenu(i).nChildID <> 0) then
                pM->AttachSubMenu( idx, BuildDropDown(gTopMenu(i).nChildID) )
            end if
        end if
    next
    return pM
end function

'' THE BAR DOES NOT OPEN ANYTHING ITSELF -- IT ASKS. PsMenuBar.bi:37-42 says so, and this
'' callback is the half ideshell never wrote, which is why its menubar drops nothing. Taken
'' from gallery2.bas:326-348, which does it correctly.
sub OnBarOpen( byval pBar as any ptr, byval idx as long, byval pMenu as any ptr, _
               byref rcItem as PsRect, byval ud as any ptr )
    g_nBarOpenCalls += 1
    g_pLastBarMenu  = pMenu
    if g_pSurf = 0 then exit sub
    '' The bar hands over the dropdown it wants shown, so there is nothing to look up --
    '' AddItem took ownership of it and the bar knows which one belongs to the title.
    dim as PsPopupMenu ptr pM = cptr( PsPopupMenu ptr, pMenu )
    if pM = 0 then exit sub

    '' The rect arrives in the BAR's coordinates; the anchor wants the surface's, so add
    '' where the bar sits. A dropdown belongs directly under the title it came from.
    dim as PsRect rc = rcItem
    if g_menubar then
        rc.x += g_menubar->bounds.x
        rc.y += g_menubar->bounds.y
    end if
    g_menus.OpenRoot( g_pSurf, rc, pM )
end sub

sub OnMenuCommand( byval pMenu as any ptr, byval nId as long, byval ud as any ptr )
    '' Commit 4 has no commands to dispatch -- the ids are tiko's and every handler behind
    '' them is in the shell. Printed so the wiring is visible rather than silently inert.
    print "tikoshell: menu command " & str(nId)
end sub


'' ---------------------------------------------------------------------------------------
'' The tree. Chrome is real; the body is still one stub. Commit 8 replaces the body with
'' the band walk and the rest of the panels.
'' ---------------------------------------------------------------------------------------
sub BuildTree( byref surf as PsSurface )
    dim as PsWidget ptr root = new PsWidget
    surf.SetRoot( root )                      '' the surface takes ownership

    buildTopMenuDefinitions()

    g_menubar = new PsMenuBar
    for id as long = IDC_MENUBAR_FILE to IDC_MENUBAR_HELP
        dim as DWSTRING sCap, sAccel
        SplitMenuText( id, sCap, sAccel )
        dim as PsPopupMenu ptr pM = BuildDropDown( id )
        pM->OnCommand( @OnMenuCommand )
        '' AddItem TAKES OWNERSHIP of the dropdown, and hands it back through OnOpenRequest
        '' when the title is clicked -- so the host keeps no table of its own.
        g_menubar->AddItem( sCap, pM )
    next
    g_menubar->OnOpenRequest( @OnBarOpen )
    root->AddChild( g_menubar )

    g_tabs = new PsTabBar
    root->AddChild( g_tabs )

    g_body = new ShellStub( @"document area", PSTHEME_BACKGROUND )
    root->AddChild( g_body )

    g_status = new PsStatusBar
    '' tiko's statusbar has seven panels; their CONTENT is frmMain_SetStatusbar's job and
    '' none of that is here. The panels exist so the band has a real height to lay out to.
    g_status->AddPanel( "" )
    root->AddChild( g_status )
end sub

sub LayoutAll( byref surf as PsSurface )
    dim as single f = surf.fScale
    dim as PsDocker dk = PsDocker( PsRc(0, 0, surf.w, surf.h), f )

    '' The three docked bands, top and bottom, exactly as frmMain_PositionWindows does them:
    '' menubar pinned full width at the top, statusbar full width at the bottom, tab bar
    '' under the menubar. What is left is the document area.
    dk.DockTop( g_menubar, SH_MENUBAR_H )
    dk.DockTop( g_tabs, SH_TABS_H )
    dk.DockBottom( g_status, SH_STATUS_H )

    dim as PsRect rcWork = dk.Fill()
    if g_body then g_body->SetBounds( rcWork )
end sub

'' ---------------------------------------------------------------------------------------
'' APPLYING A SCALE IS THREE THINGS, AND MISSING ANY ONE READS AS "the UI is not scaled".
''
'' Written once and called from BOTH the windowed path and the self-test, deliberately. An
'' assertion that drives its own copy of the steps proves the steps work; it does not prove
'' the application performs them, which is exactly the bug this shell shipped with in its
'' first windowed run.
''
''   1. the FONT reopens at the scaled pixel size -- the text engine rasterises at a fixed
''      size and has no notion of scale, so nothing else will do this
''   2. surf.fScale, which is what PsDocker multiplies the band constants by
''   3. PropagateScaleChanged, because each widget caches its own factor and scales its
''      internal metrics from that -- a tree never told keeps 1.0 and paints unscaled
''      padding inside perfectly scaled bands
'' ---------------------------------------------------------------------------------------
sub ApplyScale( byref surf as PsSurface, byval f as single )
    if f <= 0 then f = 1.0

    dim as long px = PsScaleBy( SH_FONT_PX, f )
    if px <> g_nFontPx then
        TE_Free( g_te )
        if TE_Init( g_te, strptr(g_sFont), px ) = 0 then
            print "tikoshell: could not reopen the font at " & str(px) & "px"
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

'' PsPlatform's assets, reached from _shell\ two levels up. tiko does not vendor a copy --
'' PsCore's whole value is that the toolkit and the application share one implementation,
'' and that applies to the font the text engine measures with as much as to the code.
function FontPath() as string
    dim as string sExe = exepath()
    for i as integer = 0 to len(sExe) - 1
        if sExe[i] = asc("\") then sExe[i] = asc("/")
    next
    return sExe & "/../../PsPlatform/assets/fonts/CascadiaCode.ttf"
end function


'' ======================================================================== main
    dim as boolean bSelfTest = false
    for i as integer = 1 to __FB_ARGC__ - 1
        if command(i) = "--selftest" then bSelfTest = true
    next

    if PsPlatformInit() = false then
        print "tikoshell: the platform would not start"
        end 1
    end if
    if PsRenderInit() = false then
        print "tikoshell: the render backend would not start"
        end 1
    end if
    PsTextEngineInstallApi()
    g_sFont = FontPath()
    if TE_Init( g_te, strptr(g_sFont), SH_FONT_PX ) = 0 then
        print "tikoshell: the text engine would not start -- " & g_sFont
        end 1
    end if
    g_nFontPx = SH_FONT_PX

    '' ---- THE LANGUAGE TABLE, BEFORE THE TREE -----------------------------------------
    '' Not optional and not cosmetic. L(id,"default") is `#Define L(e,s) LL(e)` -- a raw
    '' index into a table this fills, with the default DISCARDED -- so a menubar built
    '' before this runs renders eight blank titles and says nothing about why.
    ''
    '' English first, always, exactly as tiko.bas:497-520 does it: a non-English file with
    '' a missing entry is filled from the English one, so a partial translation renders as
    '' English rather than as blanks.
    ''
    '' PsExePath is _shell\, so settings\ is one level UP. tiko.exe sits in the project root
    '' and resolves it directly; this binary deliberately does not, because two executables
    '' in one directory would share libpsscintilla.dll -- see _compile_shell.bat.
    dim as DWSTRING sLangDir = PsExePath & "..\settings\languages\"
    if LoadLocalizationFile( sLangDir & "english.lang", true ) = false then
        print "tikoshell: could not load " & (sLangDir & "english.lang").Utf8
        end 1
    end if
    if LoadLocalizationFile( sLangDir & "english.lang", false ) = false then
        print "tikoshell: could not load the active language"
        end 1
    end if

    dim as PsSurface surf
    g_pSurf     = @surf
    surf.fScale = 1.0
    surf.pText  = cptr( PsTextEngine_ ptr, @g_te )
    g_pnt.pText = @g_te
    surf.Resize( SH_W, SH_H )
    BuildTree( surf )
    LayoutAll( surf )

    '' ---------------------------------------------------------------------- selftest
    if bSelfTest then
        print "--- tikoshell selftest ---"

        Check "the tree is built", (surf.pRoot <> 0)
        Check "  menubar, tabs, body, statusbar", (surf.pRoot->ChildCount() = 4), _
              str(surf.pRoot->ChildCount())

        '' ---- THE MENU VOCABULARY CAME FROM tiko ---------------------------------------
        '' The point of the commit. These titles are read out of app/modMenuDefinitions
        '' through getMenuText -> L() -> the .lang table, so a caption here is proof that
        '' all three loaded. A blank one means the language file did not, which is the
        '' failure L() cannot report on its own.
        Check "eight menubar titles", (g_menubar->GetCount() = 8), str(g_menubar->GetCount())
        Check "  built from tiko's own gTopMenu rows", (ubound(gTopMenu) > 100), _
              "gTopMenu rows: " & str(ubound(gTopMenu) + 1)
        scope
            dim as DWSTRING sCap, sAccel
            SplitMenuText( IDC_MENUBAR_FILE, sCap, sAccel )
            Check "  the File title resolved through L()", (PsLen(sCap) > 0), sCap.Utf8
        end scope
        Check "every title has a dropdown", _
              (g_menubar->GetMenu(0) <> 0) andalso (g_menubar->GetMenu(7) <> 0)

        '' ---- THE DROP ACTUALLY HAPPENS, AND THIS IS THE ASSERTION ideshell NEEDED ------
        '' PsMenuBar does not open anything itself -- it ASKS, through OnOpenRequest, and a
        '' host that never answers has a menubar that looks perfect and drops nothing.
        '' ideshell is exactly that, to this day. So the wiring is driven here rather than
        '' left to an interactive pass: OpenMenu -> OnBarOpen -> g_menus.OpenRoot, and the
        '' host is asked whether a menu is actually up.
        '' WHAT IS ASSERTED IS THAT THE HOST ANSWERS, not that a popup window appears.
        '' PsPopupHost.OpenAt returns FALSE when the parent surface has no hWin, so a real
        '' drop cannot happen windowlessly and this mode has no window by design. That is a
        '' limit of the toolkit, not of the test, and it is stated rather than worked around.
        ''
        '' It still catches ideshell's defect exactly, because ideshell's defect is that the
        '' callback was never wired at all -- so the bar asks and nothing answers. Here the
        '' callback records that it ran and which dropdown it was handed.
        Check "the bar asked the host to open File", (g_nBarOpenCalls = 0)
        g_menubar->OpenMenu( 0 )
        Check "  OnOpenRequest fired", (g_nBarOpenCalls = 1), str(g_nBarOpenCalls)
        Check "  and was handed File's own popup", _
              (g_pLastBarMenu = g_menubar->GetMenu(0))
        Check "  the bar considers the menu open", (g_menubar->IsMenuOpen() = true)
        g_menus.CloseAll()
        g_menubar->NotifyClosed()

        '' ---- THE DOCKED BANDS, NUMERICALLY -------------------------------------------
        '' "It looks docked" is not a test, and a bar one pixel short leaves a seam nobody
        '' sees until something scrolls behind it.
        Check "the menubar owns the top edge", (g_menubar->bounds.y = 0)
        Check "  full width", (g_menubar->bounds.w = surf.w), str(g_menubar->bounds.w)
        Check "the tab bar sits under it", _
              (g_tabs->bounds.y = g_menubar->bounds.y + g_menubar->bounds.h)
        Check "the status bar owns the bottom", _
              (g_status->bounds.y + g_status->bounds.h = surf.h), _
              str(g_status->bounds.y + g_status->bounds.h) & " vs " & str(surf.h)

        '' NO GAP AND NO OVERLAP down the middle. The weak version of commit 8's
        '' coverage/disjointness pair, on the four children that exist so far.
        Check "the body starts at the tab bar's bottom", _
              (g_body->bounds.y = g_tabs->bounds.y + g_tabs->bounds.h), _
              str(g_body->bounds.y)
        Check "  and ends at the status bar's top", _
              (g_body->bounds.y + g_body->bounds.h = g_status->bounds.y), _
              str(g_body->bounds.y + g_body->bounds.h) & " vs " & str(g_status->bounds.y)
        Check "  the four bands tile the surface exactly", _
              (g_menubar->bounds.h + g_tabs->bounds.h + g_body->bounds.h + _
               g_status->bounds.h = surf.h), _
              str(g_menubar->bounds.h) & "+" & str(g_tabs->bounds.h) & "+" & _
              str(g_body->bounds.h) & "+" & str(g_status->bounds.h) & " vs " & str(surf.h)

        '' A RESIZE MUST RE-LAY-OUT, and the bands must still tile. The chrome heights are
        '' fixed, so the body is what absorbs the change -- which is the invariant the
        '' document area depends on and commit 8 extends to twenty children.
        surf.Resize( 640, 480 )
        LayoutAll( surf )
        Check "a resize re-lays out", (g_body->bounds.w = 640), str(g_body->bounds.w)
        Check "  the chrome heights did not move", _
              (g_menubar->bounds.h = SH_MENUBAR_H) andalso _
              (g_status->bounds.h = SH_STATUS_H)
        Check "  and the body absorbed the difference", _
              (g_body->bounds.h = 480 - SH_MENUBAR_H - SH_TABS_H - SH_STATUS_H), _
              str(g_body->bounds.h)
        surf.Resize( SH_W, SH_H )
        LayoutAll( surf )

        '' ---- DPI. EVERY BAND SCALES, AND SO DOES WHAT IS INSIDE IT --------------------
        '' The first windowed run of this shell was visibly unscaled, and the reason is a
        '' class of bug that is invisible at 1.0: the bands came from PsDocker, which takes
        '' the factor as an argument and was right, while every widget kept its own cached
        '' 1.0 and painted unscaled padding inside them. So both halves are asserted --
        '' the band heights AND the tree's own idea of the scale.
        scope
            dim as long hMenu1 = g_menubar->bounds.h
            ApplyScale( surf, 1.5 )
            LayoutAll( surf )

            Check "at 1.5 the menubar band scales", _
                  (g_menubar->bounds.h = PsScaleBy(SH_MENUBAR_H, 1.5)), _
                  str(hMenu1) & " -> " & str(g_menubar->bounds.h)
            Check "  the status bar still owns the bottom", _
                  (g_status->bounds.y + g_status->bounds.h = surf.h)
            Check "  the bands still tile exactly", _
                  (g_menubar->bounds.h + g_tabs->bounds.h + g_body->bounds.h + _
                   g_status->bounds.h = surf.h)
            '' THE HALF THAT WAS ACTUALLY BROKEN. A widget scales its own metrics from the
            '' factor it was told, so a tree that is never told reports 1.0 forever while
            '' the layout around it grows.
            Check "  and the WIDGETS were told, not just the layout", _
                  (g_menubar->ScaleY(100) = PsScaleBy(100, 1.5)), _
                  str(g_menubar->ScaleY(100)) & " vs " & str(PsScaleBy(100, 1.5))

            ApplyScale( surf, 1.0 )
            LayoutAll( surf )
            Check "  back at 1.0 the band returns", (g_menubar->bounds.h = hMenu1)
        end scope

        '' NO WINDOW WAS CREATED, and the surface says so. hWin is the marker PsModalHost
        '' reads to find a dialog's parent, so a surface that acquired one by accident here
        '' would be a real defect rather than an untidy test.
        Check "the surface is windowless", (surf.hWin = 0)

        print ""
        print "  " & g_nPass & " passed, " & g_nFail & " failed"
        TE_Free( g_te )
        PsPlatformShutdown()
        if g_nFail > 0 then end 1
        end 0
    end if

    '' ---------------------------------------------------------------------- the window
    dim as PsSurfaceDesc desc
    desc.kind        = PSSURF_TOPLEVEL
    desc.w           = SH_W
    desc.h           = SH_H
    desc.title.Utf8  = "tiko -- phase 7c shell"
    desc.bResizable  = true
    dim as PsSurfaceHandle win = g_plat.window.Create( @desc )
    if win = 0 then
        print "tikoshell: no window"
        end 1
    end if

    '' MARRYING THE SURFACE TO THE WINDOW IS FOUR STEPS, AND THE FIRST VERSION OF THIS FILE
    '' GOT TWO OF THEM WRONG -- both copied from ideshell, which has the same defects today.
    ''
    '' SH_W/SH_H are DESIGN units. PsSurfaceDesc takes PIXELS, so a 1100x700 request lands
    '' as 1100x700 physical, which on this 1.75 display is 629x400 design units -- a window
    '' two-thirds the size asked for, whose chrome then looks tiny inside it. What the
    '' window wants is the design size SCALED.
    surf.hWin = win

    dim as single f = g_plat.window.ScaleOf( win )
    if f <= 0 then f = 1.0

    '' 1. FONT, fScale AND THE TREE, all three, through the one routine the self-test also
    ''    calls -- see ApplyScale. Doing it by hand here is how the first version missed
    ''    two of the three.
    ApplyScale( surf, f )

    '' 2. THE WINDOW IS SIZED FROM THE DESIGN SIZE TIMES THE SCALE, then clamped to the
    ''    display. The clamp compares PIXELS with PIXELS -- ideshell clamps its DESIGN size
    ''    against usable PIXELS, which are only the same number at 100%.
    scope
        dim as long nWantW = PsScaleBy( SH_W, f )
        dim as long nWantH = PsScaleBy( SH_H, f )
        dim as PsMonitorInfo mi
        if g_plat.monitors.Describe(0, @mi) then
            '' 90% of the USABLE area, not the full bounds: usable already has the taskbar
            '' out of it, and a window exactly its size has no margin left to grab.
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

    print "tikoshell: " & str(surf.w) & "x" & str(surf.h) & " at scale " & str(surf.fScale)

    '' ---------------------------------------------------------------------- the pump
    '' THE SEVEN STEPS, in this order, from ideshell.bas:741-800. Every host in PsPlatform
    '' writes this loop out longhand -- there is no PsEventLoop -- so it is copied rather
    '' than invented, and the order is not negotiable:
    ''
    ''   Ticks -> PsTimerService -> Wait(PsTimerWaitMs) -> route by SURFACE, then by kind
    ''   -> popup hosts get RouteEvent FIRST and surf.Dispatch only if they decline
    ''   -> PsSurfaceSyncCursor -> paint if damaged -> every popup host's PresentIfDamaged.
    ''
    '' NEVER `continue do` past the PresentIfDamaged at the bottom: a popup that is damaged
    '' and never presented is a menu that is open and invisible.
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
                        '' window has moved to a display with another factor -- but it
                        '' cannot reopen the FONT, which the text engine holds at a fixed
                        '' pixel size. So the scale is read back afterwards and the font
                        '' follows it. Without this, dragging the window between a 100% and
                        '' a 175% monitor rescales every rect and leaves the text behind.
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
                    else
                        g_menus.RouteEvent( @ev )
                    end if

                case else
                    if bMine = false then
                        g_menus.RouteEvent( @ev )
                    else
                        if g_menus.RouteEvent( @ev ) = false then surf.Dispatch( @ev )
                    end if
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

        g_menus.PresentIfDamaged( g_pnt )
    loop

    if pix <> 0 then deallocate(pix)
    g_plat.window.Destroy( win )
    TE_Free( g_te )
    PsPlatformShutdown()
    end 0
