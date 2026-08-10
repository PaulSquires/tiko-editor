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
''      bands were right, because PsDocker takes the factor as an argument; the WINDOW SIZE
''      and the FONT were not, and the tree was never invalidated. See ApplyScale.
''
'' The third is the one worth remembering: the self-test was 21 green while the UI was
'' visibly wrong, because everything it asserted was a RELATION between rectangles and all
'' of those relations hold perfectly at the wrong scale.
''
'' AND THE FIRST FIX FOR IT SHIPPED A VACUOUS ASSERTION, which is the same lesson twice.
'' It checked g_menubar->ScaleY(100) against PsScaleBy(100, 1.5) and called that "the
'' assertion that would have caught it". It would not have -- ScaleY reads the surface's
'' scale live and passes regardless of what the tree was told. See the note in the
'' self-test's scale section for what is and is not covered now.
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
#include once "ui/core/PsThemeLoadFile.inc"
#include once "ui/core/PsLayout.inc"
#include once "ui/core/PsAccel.inc"
#include once "ui/controls/PsMenuBar.inc"
#include once "ui/controls/PsStatusBar.inc"
#include once "ui/controls/PsTabBar.inc"
#include once "scintilla/PsTextEngineC.inc"
#include once "scintilla/PsSciView.inc"
#include once "scintilla/PsSciClipboard.inc"

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
'' The binding DATA -- gKeys and the 109 defaults. Split out of src/modKeyBindings.inc for
'' this commit: getMenuAccelText walks gKeys to caption a menu row, so without it every
'' accelerator column in the menus above rendered blank.
#include once "app/modKeyBindings.inc"

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

'' ---- THE COMMIT-4 STUB, NOW REAL -------------------------------------------------------
'' createToolsMenuShortcut composes a User Tools shortcut LABEL by asking what a stored key
'' name means. In commit 4 this returned 0 and every such row rendered with blank shortcut
'' text; it is backed by PsAccel now.
''
'' IT DOES NOT RETURN A VIRTUAL KEY. The shell's version returns a Win32 VK through
'' frmKeyboard_AccelKeyToValue and VkKeyScanEx; this returns a PsKey, which is a PHYSICAL
'' POSITION. Callers that only test it against zero -- which is all createToolsMenuShortcut
'' does -- cannot tell the difference. Anything that compared it to a VK_ constant would be
'' wrong, and nothing in this binary does.
''
'' NOT `private`, and AFTER the include: app/modMenuDefinitions.inc:22 pulls
'' ../modKeyBindings.bi -- the app layer reaching UP into the shell by relative path, which
'' no token ratchet can see because a path is not an identifier -- so the declaration is
'' already in scope by the time this is reached and a second one would collide.
function KeyBindings_PickListKeyToValue( byval wszString as DWSTRING ) as long
    '' "None" is a pick-list entry meaning "no shortcut", not a key. The shell's version
    '' tests for it explicitly and so must this, or it resolves to nothing anyway but for
    '' the wrong reason.
    if PsUCase(PsTrim(wszString)).Utf8 = "NONE" then return 0
    return PsAccelKeyFromName( wszString )
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

'' The accelerator table, built from tiko's own 109 default bindings, 85 of which carry a chord. Consulted in the pump
'' BETWEEN the menu host and surf.Dispatch -- see the loop for why that order is fixed.
dim shared as PsAccelTable g_accel
dim shared as long g_nAccelSkipped

'' The menubar titles' first letters, kept HERE because PsMenuBar has no GetCaption and
'' should not grow one for this: the host wrote the captions and is the natural owner of a
'' mnemonic policy it is faking anyway. See TryAltMnemonic.
dim shared as string g_barInitial(0 to 15)

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
dim shared as PsMenuBar  ptr g_menubar
dim shared as PsStatusBar ptr g_status

'' EVERY OTHER CHILD IS A STUB, and each is named as the ORACLE names it -- so a dump and a
'' self-test failure can be read against each other without a translation table.
''
'' g_tabs was a real PsTabBar in commit 4 and is a stub from this one. The layout now needs
'' a band whose height IT controls rather than one the control decides, and a stub paints
'' its own bounds, which is what makes a misplaced band visible without reading an
'' assertion. The real control returns when the tab MODEL does, well past step 1.
dim shared as ShellStub ptr g_tabs, g_topTabsMenu
dim shared as ShellStub ptr g_panel, g_splitPanel
dim shared as ShellStub ptr g_barInfo, g_barFind, g_barReplace
dim shared as ShellStub ptr g_splitOutput, g_output, g_fip

'' THE EDITOR, and the two scrollbars flanking it. tiko replaces Scintilla's own scrollbars
'' with its PsVScrollBar/PsHScrollBar, so the editor rect is the document rect LESS both --
'' see the reserve rule in the layout. The bars are stubs here: their GEOMETRY is layout and
'' belongs in this commit; wiring them to the view is not.
dim shared as PsSciView ptr g_view, g_view2
dim shared as ShellStub ptr g_vscroll, g_hscroll
dim shared as ShellStub ptr g_vscroll2, g_hscroll2
dim shared as ShellStub ptr g_splitV, g_splitH

'' The document rect: the one rectangle every band above conspires to produce. Kept so the
'' self-test can assert it directly, and commit 9 puts the editor in it.
dim shared as PsRect g_rcDoc

'' ---------------------------------------------------------------------------------------
'' THE THEME REACHES THE EDITOR ONLY IF THE HOST TAKES IT THERE.
''
'' PsThemeApply walks the WIDGET tree, and Scintilla's colours live in its own style table
'' behind SCI_* messages -- so the editor is the one thing a themed shell does not theme.
'' The symptom is a WHITE PANE in the middle of an otherwise perfect window, and it is what
'' ideshell found the day it was first composed with chrome.
''
'' Adapted from ideshell's StyleEditorFromTheme, INCLUDING its margin fix: STYLECLEARALL
'' does not reach STYLE_LINENUMBER, so an editor themed to the last glyph still shows a
'' white strip down its left edge. minieditor has that strip to this day.
'' ---------------------------------------------------------------------------------------
function ToBgr( byval c as PsColor ) as long
    '' SCINTILLA WANTS 0x00BBGGRR; PsColor is 0xAARRGGBB. Getting this wrong does not fail,
    '' it renders a plausible WRONG colour -- the hardest kind to notice in a palette you
    '' have never seen.
    return ((c and &hFF) shl 16) or (c and &hFF00) or ((c shr 16) and &hFF)
end function

sub StyleOneView( byval pV as PsSciView ptr, byref surf as PsSurface )
    if pV = 0 then exit sub
    if pV->pSci = 0 then exit sub

    dim as long bgrBack = ToBgr( PsThemeRoleColor(PSTHEME_BACKGROUNDALT) )
    dim as long bgrFore = ToBgr( PsThemeRoleColor(PSTHEME_FOREGROUND) )
    dim as long bgrSel  = ToBgr( PsThemeRoleColor(PSTHEME_SELECTION) )

    '' STYLE_DEFAULT, THEN STYLECLEARALL, THEN THE REST. Setting style 0 alone leaves every
    '' other style on Scintilla's built-in white -- a white page with correctly coloured
    '' text on it.
    pV->Msg( SCI_STYLESETFORE, STYLE_DEFAULT, bgrFore )
    pV->Msg( SCI_STYLESETBACK, STYLE_DEFAULT, bgrBack )
    pV->Msg( SCI_STYLECLEARALL )

    '' The caret and the selection are not styles, and STYLECLEARALL does not touch them.
    pV->Msg( SCI_SETCARETFORE, bgrFore )
    pV->Msg( SCI_SETSELBACK, 1, bgrSel )

    '' AND NEITHER IS THE MARGIN.
    pV->Msg( SCI_STYLESETFORE, STYLE_LINENUMBER, ToBgr(PsThemeRoleColor(PSTHEME_FOREGROUNDDIM)) )
    pV->Msg( SCI_STYLESETBACK, STYLE_LINENUMBER, ToBgr(PsThemeRoleColor(PSTHEME_BACKGROUND)) )
    pV->Msg( SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER )
    pV->Msg( SCI_SETMARGINWIDTHN, 0, PsScaleBy(38, surf.fScale) )
    pV->Msg( SCI_SETMARGINWIDTHN, 1, 0 )
    pV->Msg( SCI_SETMARGINWIDTHN, 2, 0 )
end sub

'' Both panes. A split view showing one document in two colour schemes would be a strange
'' thing to ship, and the second pane is created after the first is already styled.
sub StyleEditorFromTheme( byref surf as PsSurface )
    StyleOneView( g_view,  surf )
    StyleOneView( g_view2, surf )
end sub

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

'' Forward-declared: the body needs g_state and Shell_LayoutAll, which are declared
'' further down, and BuildDropDown up here needs the address.
declare sub OnMenuCommand( byval pMenu as any ptr, byval nId as long, byval ud as any ptr )
declare sub OnMenusClosed( byval pHost as any ptr, byval ud as any ptr )
declare sub OnBarCloseRequest( byval pBar as any ptr, byval ud as any ptr )

function BuildDropDown( byval nParentId as long ) as PsPopupMenu ptr
    dim as PsPopupMenu ptr pM = new PsPopupMenu

    '' NO pM->OnCommand HERE, and that is the fix rather than an omission. PsMenuHost takes
    '' every popup's single command slot so it can CLOSE THE CHAIN before the handler runs,
    '' and the application registers with the host instead -- g_menus.OnCommand, below.
    ''
    '' Setting it per-popup is what this file did first, and it was wrong twice over: the
    '' submenus never got one at all (so the MRU list, Settings, Format and the theme rows
    '' clicked to nothing), and the top-level ones overwrote the host's, so the menu stayed
    '' open and the bar stayed highlighted after every click.
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



'' ---------------------------------------------------------------------------------------
'' THE ACCELERATOR TABLE, FROM tiko's OWN BINDINGS.
''
'' gKeys carries 109 commands, each with a default chord and an optional user override, and
'' getMenuAccelText already renders that same pair beside a menu row. Feeding both from one
'' array is the point: a shortcut that fires and a label that says it fires cannot disagree.
''
'' SKIPPED ENTRIES ARE COUNTED RATHER THAN IGNORED. Most of the 109 have no default chord at
'' all, which is not a failure -- but a chord that fails to PARSE is, and the two would be
'' indistinguishable if the count were not kept. The self-test asserts it.
'' ---------------------------------------------------------------------------------------
sub BuildAccelerators()
    g_accel.Clear_()
    g_nAccelSkipped = 0

    for i as long = lbound(gKeys) to ubound(gKeys)
        '' The user's override wins, exactly as getMenuAccelText resolves it -- and a
        '' DISABLED default means no binding rather than the default one.
        dim as DWSTRING sChord = gKeys(i).wszUserKeys
        if PsLen(sChord) = 0 then
            if gKeys(i).bDefaultDisabled = false then sChord = gKeys(i).wszDefaultKeys
        end if
        if PsLen(sChord) = 0 then continue for

        if g_accel.AddText( sChord, gKeys(i).idAction ) = false then
            g_nAccelSkipped += 1
            print "tikoshell: could not parse binding " & gKeys(i).wszMsgString.Utf8 & _
                  " = " & sChord.Utf8
        end if
    next
end sub


'' ---------------------------------------------------------------------------------------
'' ALT+MNEMONIC, FAKED HOST-SIDE AND DELIBERATELY SO.
''
'' PsMenuBar.bi:44-48 makes Alt activation the host's, with the right reason: a global Alt
'' hook fights whatever else wants the key -- in tiko's case Scintilla -- so the host decides
'' what Alt means. Promoting a guess into the library is how you get an API you then unpick.
''
'' This is the guess: Alt plus the first letter of a title opens it. tiko's real policy is
'' handleAltKeyMenuBar plus an eight-deep filter chain plus Scintilla's own claim, and none
'' of that is understood in widget terms yet. The titles carry no "&" markers, so the FIRST
'' LETTER is all there is to match on -- which is why this is a fake and not a mnemonic
'' implementation. Two titles starting with the same letter would need the real thing.
'' ---------------------------------------------------------------------------------------
function TryAltMnemonic( byval ev as PsEvent ptr ) as boolean
    if ev = 0 then return false
    if ev->kind <> PSEV_KEY_DOWN then return false
    if (ev->key.modifiers and PSMOD_ALT) = 0 then return false
    if g_menubar = 0 then return false
    if (ev->key.key < PSKEY_A) orelse (ev->key.key > PSKEY_Z) then return false

    dim as string sWant = chr( asc("A") + (ev->key.key - PSKEY_A) )
    for i as long = 0 to g_menubar->GetCount() - 1
        if i > ubound(g_barInitial) then exit for
        if g_barInitial(i) = sWant then
            g_menubar->OpenMenu(i)
            return true
        end if
    next
    return false
end function


'' ---------------------------------------------------------------------------------------
'' The tree. Chrome is real; the body is still one stub. Commit 8 replaces the body with
'' the band walk and the rest of the panels.
'' ---------------------------------------------------------------------------------------
sub BuildTree( byref surf as PsSurface )
    dim as PsWidget ptr root = new PsWidget
    surf.SetRoot( root )                      '' the surface takes ownership

    buildTopMenuDefinitions()
    frmKeyboard_CreateDefaultKeyBindings()
    BuildAccelerators()

    g_menubar = new PsMenuBar
    for id as long = IDC_MENUBAR_FILE to IDC_MENUBAR_HELP
        dim as DWSTRING sCap, sAccel
        SplitMenuText( id, sCap, sAccel )
        if (id - IDC_MENUBAR_FILE) <= ubound(g_barInitial) then
            g_barInitial(id - IDC_MENUBAR_FILE) = ucase(left(sCap.Utf8, 1))
        end if
        dim as PsPopupMenu ptr pM = BuildDropDown( id )
        '' AddItem TAKES OWNERSHIP of the dropdown, and hands it back through OnOpenRequest
        '' when the title is clicked -- so the host keeps no table of its own.
        g_menubar->AddItem( sCap, pM )
    next
    g_menubar->OnOpenRequest( @OnBarOpen )

    '' THE HOST OWNS THE COMMAND SLOT, so this is where the application asks for it. It
    '' closes the chain first and then calls this -- which also means a handler is free to
    '' raise a modal dialog without leaving a menu floating over it.
    g_menus.OnCommand( @OnMenuCommand )

    '' AND THE BAR HAS TO BE TOLD THE MENU WENT AWAY, whether it went by a command, by
    '' Escape or by a click outside. Without this the bar still believes a menu is open --
    '' the title stays highlighted and the next hover re-opens it. PsMenuBar.bi:135-138 says
    '' so; nothing in PsPlatform was wiring it.
    g_menus.OnClosed( @OnMenusClosed )
    g_menubar->OnCloseRequest( @OnBarCloseRequest )
    root->AddChild( g_menubar )

    '' THE TAB BAR IS A STUB TOO, from this commit on. PsTabBar is a real control and was
    '' real in commit 4, but the layout now needs a band whose height it CONTROLS rather
    '' than one the control decides -- and a stub prints its own bounds, which is what makes
    '' a misplaced band visible without reading an assertion. The real PsTabBar comes back
    '' when the tab MODEL does, which is well past step 1.
    g_tabs = new ShellStub( @"TOPTABS", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_tabs )

    '' Every remaining child frmMain_PositionWindows places, named as the ORACLE names them
    '' so a dump and a self-test failure can be read against each other.
    g_topTabsMenu = new ShellStub( @"TOPTABSMENU", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_topTabsMenu )
    g_panel       = new ShellStub( @"PANEL",       PSTHEME_BACKGROUNDALT )
    root->AddChild( g_panel )
    g_splitPanel  = new ShellStub( @"",            PSTHEME_BORDER )
    root->AddChild( g_splitPanel )
    g_barInfo     = new ShellStub( @"TOPTABSINFO", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_barInfo )
    g_barFind     = new ShellStub( @"FIND",        PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_barFind )
    g_barReplace  = new ShellStub( @"REPLACE",     PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_barReplace )
    g_splitOutput = new ShellStub( @"",            PSTHEME_BORDER )
    root->AddChild( g_splitOutput )
    g_output      = new ShellStub( @"OUTPUT",      PSTHEME_BACKGROUNDALT )
    root->AddChild( g_output )
    g_fip         = new ShellStub( @"FINDINPROJECT", PSTHEME_BACKGROUNDALT )
    root->AddChild( g_fip )

    g_vscroll = new ShellStub( @"", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_vscroll )
    g_hscroll = new ShellStub( @"", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_hscroll )

    '' A REAL PsSciView. Created at a nominal size -- OnLayout tells Scintilla the real one,
    '' and Scintilla is TOLD its size rather than asked (PsSciView.inc:34-38).
    g_view = new PsSciView
    if g_view->Create( g_sFont, 400, 300 ) then
        PsSciUseSystemClipboard( g_view )
        dim as string sDoc = _
            "' tiko shell -- phase 7c" & chr(10) & _
            "'" & chr(10) & _
            "' The editor is a PsSciView in frmMain's document rect." & chr(10) & _
            "' The split modes are commit 10." & chr(10) & _
            "" & chr(10) & _
            "sub Main()" & chr(10) & _
            "    print " & chr(34) & "hello" & chr(34) & chr(10) & _
            "end sub" & chr(10)
        g_view->Msg( SCI_SETTEXT, 0, cast(integer, strptr(sDoc)) )
        g_view->Msg( SCI_SETCARETPERIOD, 530 )
        g_view->Msg( SCI_EMPTYUNDOBUFFER )
    end if
    root->AddChild( g_view )

    '' ---- THE SPLIT PANE, SHARING ONE DOCUMENT ------------------------------------------
    '' A SPLIT VIEW IS TWO VIEWS OF ONE DOCUMENT, not two documents. Scintilla's own
    '' mechanism for that is the doc pointer: create the second view, then point it at the
    '' first one's document. Without this the split shows an empty second file, which looks
    '' close enough to a split to be believed.
    ''
    '' SCI_SETDOCPOINTER ADDREFS, so both views hold the document and neither frees it out
    '' from under the other.
    g_view2 = new PsSciView
    if g_view2->Create( g_sFont, 400, 300 ) then
        PsSciUseSystemClipboard( g_view2 )
        '' PsScintilla.bi does not declare these two -- it carries what the toolkit's own
        '' controls need, and nothing in PsPlatform has wanted a second view of one
        '' document before. The values are Scintilla's and stable (Scintilla.h).
        const SCI_GETDOCPOINTER_ = 2357
        const SCI_SETDOCPOINTER_ = 2358
        if g_view <> 0 then
            g_view2->Msg( SCI_SETDOCPOINTER_, 0, g_view->Msg(SCI_GETDOCPOINTER_) )
        end if
        g_view2->Msg( SCI_SETCARETPERIOD, 530 )
    end if
    root->AddChild( g_view2 )

    g_vscroll2 = new ShellStub( @"", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_vscroll2 )
    g_hscroll2 = new ShellStub( @"", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_hscroll2 )

    '' Two bars, one per orientation, because orientation is fixed at creation -- tiko keeps
    '' two PsSplitter instances for the same reason and shows at most one.
    g_splitV = new ShellStub( @"", PSTHEME_BORDER )
    root->AddChild( g_splitV )
    g_splitH = new ShellStub( @"", PSTHEME_BORDER )
    root->AddChild( g_splitH )

    g_status = new PsStatusBar
    '' tiko's statusbar has seven panels; their CONTENT is frmMain_SetStatusbar's job and
    '' none of that is here. The panels exist so the band has a real height to lay out to.
    g_status->AddPanel( "" )
    root->AddChild( g_status )
end sub


'' ========================================================================================
'' ShellLayoutState -- WHAT frmMain's LAYOUT ACTUALLY DEPENDS ON.
''
'' THE INVERSION IS THE WORK IN THIS COMMIT, and it reads smaller than it is.
'' frmMain_PositionWindows reads its own inputs OUT OF ITS OWN WINDOWS --
'' AfxGetWindowHeight(HWND_FRMOUTPUT), IsWindowVisible(HWND_FRMPANEL),
'' AfxGetWindowWidth(HWND_FRMPANEL). The windows ARE the model, so the function cannot be
'' asked what it would do in a state that is not on screen right now.
''
'' Here the state is an argument. Shell_LayoutAll is a pure function of (w, h, fScale,
'' state), which is what makes twenty children assertable in sixteen configurations
'' without a display -- and it is what lets the self-test diff against the oracle.
''
'' EVERYTHING IS IN DESIGN UNITS. The four sizes below are stored unscaled and scaled at
'' use, exactly as ideshell stores splitter positions, so a DPI change moves no furniture.
'' ========================================================================================
type ShellLayoutState
    '' ---- PIXELS, NOT DESIGN UNITS, AND THAT IS NOT A CHOICE --------------------------
    '' frmMain reads all six of the sizes below with AfxGetWindowHeight/Width and uses them
    '' UNSCALED; only the layout CONSTANTS go through pWindow->ScaleX/Y. Storing these in
    '' design units and scaling them here would be a different layout, and the first
    '' version of this file did exactly that.
    ''
    '' The consequence is tiko's, not this port's: gConfig.ShowPanelWidth is written from a
    '' live pixel width, so a panel sized at 175% and reopened at 100% comes back a third
    '' narrower. Reproduced because the oracle records it; worth fixing one day, and not
    '' here.
    nPanelW        as long = 413      '' gConfig.ShowPanelWidth, as measured
    nOutputH       as long = 194      '' gConfig.ShowOutputPanelHeight, as measured

    '' Visibility, which frmMain reads with IsWindowVisible on the real windows.
    bPanelVisible  as boolean = true      '' gConfig.ShowPanel
    bExplorerRight as boolean = false     '' gConfig.ExplorerPositionRight
    bOutputVisible as boolean = true      '' gConfig.ShowOutputPanel
    '' ORTHOGONAL to bOutputVisible, and frmMain.inc:963-970 explains why: while undocked
    '' the panel IS visible -- it fills its own frame -- so IsWindowVisible alone would keep
    '' reserving space at the bottom of frmMain for a window that is no longer there.
    bOutputFloating as boolean = false

    bShowInfo      as boolean = false     '' gFind.bShowInfoPanel
    bShowFind      as boolean = false     '' gFind.bShowFindPanel
    bShowReplace   as boolean = false     '' gFind.bShowReplacePanel

    nTabCount      as long = 1            '' gTTabCtl.GetItemCount()

    '' The active document's split. tiko keeps these on clsDocument, not in config:
    '' EditorSplitMode and SplitX/SplitY, the latter two in PIXELS like everything else
    '' the layout measures.
    nSplitMode     as long = 0            '' 0 none, 1 left/right, 2 top/bottom
    nSplitX        as long = 0
    nSplitY        as long = 0
    bFipActive     as boolean = false     '' frmFindInProject_IsActive()

    '' MEASURED, NOT CONSTANT, and pixels for the same reason as the two above. frmMain
    '' reads these four out of the controls because each sizes itself from its font, and a
    '' widget tree cannot ask before it has laid out. The oracle prints all six in its
    '' header for exactly this reason: they are INPUTS to any comparison, not results of it.
    '' The defaults here are this machine at 1.75.
    nMenubarH      as long = 52
    nStatusH       as long = 46
    nTabsH         as long = 63
    nTopTabsMenuW  as long = 186
    '' The editor's vertical scrollbar width, measured too -- AfxGetWindowWidth in tiko.
    nVScrollW      as long = 23

    '' tiko shows the H bar only when the document is wider than the pane, which is a
    '' MODEL question this shell has no answer to. Its height is reserved either way; only
    '' whether the bar is painted follows this.
    bHScrollVisible as boolean = false
end type

dim shared as ShellLayoutState g_state

'' tiko's own layout constants, design units, from frmMain.bi and modDeclares.bi. Named
'' identically so the transliteration below can be read beside frmMain.inc:796-1092.
const SH_SPLITTER_GRAB        = 6
const SH_PANEL_MIN_WIDTH      = 236
const SH_PANEL_MIN_CONTENT    = 240
const SH_OUTPUT_MIN_EDITOR    = 120
const SH_OUTPUT_TABS_HEIGHT   = 40
const SH_SCROLLBAR_WIDTH_EDITOR = 12
const SH_TOPTABS_INFO_HEIGHT    = 40
const SH_TOPTABS_FIND_HEIGHT    = 40
const SH_TOPTABS_REPLACE_HEIGHT = 40
'' The top margin the info band's ELSE arm adds (frmMain.inc:915). Real behaviour, trivially
'' lost in a transliteration, and invisible without an assertion.
const SH_INFO_ABSENT_MARGIN   = 8
const SH_SCROLLBAR_HEIGHT     = 12
'' clsDocument.bi:102-104, so the state record reads like the original.
const SH_SPLIT_NONE      = 0
const SH_SPLIT_LEFTRIGHT = 1
const SH_SPLIT_TOPBOTTOM = 2

'' Clamp helper. THE CLAMP LIVES IN THE LAYOUT, NOT THE SPLITTER -- ideshell's contract,
'' and tiko's too: PsSplitter_SetRange/SetPos/GetPos is frmMain handing the control limits
'' it computed and then taking the clamped value back ("take back the clamp", twice in
'' frmMain.inc). The control cannot know about the editor scrollbars flanking the panes.
function ClampTo( byval v as long, byval lo as long, byval hi as long ) as long
    if hi < lo then return lo
    if v < lo then return lo
    if v > hi then return hi
    return v
end function

'' ========================================================================================
'' Shell_LayoutAll -- frmMain_PositionWindows (frmMain.inc:796-1092), as a pure function.
''
'' A TRANSLITERATION, deliberately, and it is meant to be read beside the original. The band
'' order, the clamp-and-re-derive dance, the reserve arithmetic and the two mirrorings are
'' the same; what changed is that the inputs arrive in `st` instead of being read back out
'' of the windows, and SetWindowPos became SetBounds.
''
'' NOT HERE: the three document split modes. frmMain delegates those to four helpers at
'' :608, :651, :696 and :743; they are commit 10, and until then the whole document rect
'' goes to one stub.
'' ========================================================================================
sub Shell_LayoutAll( byref surf as PsSurface, byref st as ShellLayoutState )
    dim as single f = surf.fScale
    dim as long W = surf.w
    dim as long H = surf.h

    '' The six measured sizes are PIXELS and are used as they arrive; only the SH_*
    '' constants below go through PsScaleBy. That asymmetry is frmMain's, exactly.
    dim as long nMenubarH = st.nMenubarH
    dim as long nStatusH  = st.nStatusH

    '' ---- the two pinned bands ----------------------------------------------------------
    g_menubar->SetBounds( PsRc(0, 0, W, nMenubarH) )
    g_status->SetBounds( PsRc(0, H - nStatusH, W, nStatusH) )

    dim as long nLeft = 0
    dim as long nTop  = nMenubarH

    '' ---- the side panel and its splitter, mirrorable -----------------------------------
    '' The bar sits on the panel's INNER edge, and its strip is taken from the CONTENT area
    '' so the panel keeps its width and ShowPanelWidth round-trips unchanged. nPanelW keeps
    '' meaning the true panel width; nPanelReserve (panel + bar) is what the content gives up.
    dim as long nGrabPanel    = PsScaleBy( SH_SPLITTER_GRAB, f )
    dim as long nPanelReserve = 0
    dim as long nPanelW       = st.nPanelW

    if st.bPanelVisible then
        dim as long nBarPos
        if st.bExplorerRight then
            nBarPos = ClampTo( W - nPanelW - nGrabPanel, _
                               PsScaleBy(SH_PANEL_MIN_CONTENT, f), _
                               W - nGrabPanel - PsScaleBy(SH_PANEL_MIN_WIDTH, f) )
            nPanelW = W - nBarPos - nGrabPanel
        else
            nBarPos = ClampTo( nPanelW, _
                               PsScaleBy(SH_PANEL_MIN_WIDTH, f), _
                               W - nGrabPanel - PsScaleBy(SH_PANEL_MIN_CONTENT, f) )
            nPanelW = nBarPos
        end if
        if nPanelW < 0 then nPanelW = 0

        dim as long nPanelX = nLeft
        if st.bExplorerRight then nPanelX = W - nPanelW
        dim as long nPanelH = H - nStatusH - nMenubarH

        g_panel->SetBounds( PsRc(nPanelX, nTop, nPanelW, nPanelH) )
        g_splitPanel->SetBounds( PsRc(nBarPos, nTop, nGrabPanel, nPanelH) )
        g_panel->bVisible = true
        g_splitPanel->bVisible = true

        nPanelReserve = nPanelW + nGrabPanel
        nLeft = nPanelReserve
        if st.bExplorerRight then nLeft = 0
    else
        nPanelW = 0
        g_panel->bVisible = false
        g_splitPanel->bVisible = false
    end if

    '' ---- the tab bar and its icon strip ------------------------------------------------
    dim as long nTabsH = 0
    if st.nTabCount = 0 then
        g_tabs->bVisible = false
        g_topTabsMenu->bVisible = false
    else
        nTabsH = st.nTabsH
        dim as long nMenuW = st.nTopTabsMenuW

        '' THE CONTENT AREA'S RIGHT EDGE, WHICH IS NOT ALWAYS W. Docked left the content
        '' runs to the client edge; docked RIGHT it stops at the panel. Same line the
        '' document rect computes for itself below, and here for the same reason.
        ''
        '' BOTH OF THESE USED TO BE WRONG, and the port reproduced it deliberately for two
        '' commits while the oracle recorded it: nLeftMenu came off the full client width
        '' and the bar's width was nLeftMenu - nPanelReserve, which is only nLeftMenu - nLeft
        '' when the panel is on the left. Docked right that put the icon strip ON TOP OF THE
        '' PANEL. Fixed in frmMain.inc first, then here; the oracle moved exactly one
        '' rectangle, which is how both sides are known to agree.
        dim as long nContentRight = W
        if st.bExplorerRight then nContentRight = W - nPanelReserve

        dim as long nLeftMenu = nContentRight - PsScaleBy(SH_SCROLLBAR_WIDTH_EDITOR, f) - nMenuW

        g_topTabsMenu->SetBounds( PsRc(nLeftMenu, nTop, nMenuW, nTabsH) )
        g_tabs->SetBounds( PsRc(nLeft, nTop, nLeftMenu - nLeft, nTabsH) )
        g_topTabsMenu->bVisible = true
        g_tabs->bVisible = true
        nTop += nTabsH
    end if

    '' ---- the three conditional bands ---------------------------------------------------
    '' Each spans the CONTENT width and advances nTop by its own height. The info band's
    '' ELSE arm is NOT a no-op: it adds a top margin so the find band is not flush against
    '' the tab bar. Trivially lost in a transliteration and invisible without an assertion.
    if st.bShowInfo then
        dim as long hBand = PsScaleBy( SH_TOPTABS_INFO_HEIGHT, f )
        g_barInfo->SetBounds( PsRc(nLeft, nTop, W - nPanelReserve, hBand) )
        g_barInfo->bVisible = true
        nTop += hBand
    else
        g_barInfo->bVisible = false
        nTop += PsScaleBy( SH_INFO_ABSENT_MARGIN, f )
    end if

    if st.bShowFind then
        dim as long hBand = PsScaleBy( SH_TOPTABS_FIND_HEIGHT, f )
        g_barFind->SetBounds( PsRc(nLeft, nTop, W - nPanelReserve, hBand) )
        g_barFind->bVisible = true
        nTop += hBand
    else
        g_barFind->bVisible = false
    end if

    if st.bShowReplace then
        dim as long hBand = PsScaleBy( SH_TOPTABS_REPLACE_HEIGHT, f )
        g_barReplace->SetBounds( PsRc(nLeft, nTop, W - nPanelReserve, hBand) )
        g_barReplace->bVisible = true
        nTop += hBand
    else
        g_barReplace->bVisible = false
    end if

    '' ---- the output panel and its splitter ---------------------------------------------
    '' Same clamp-and-re-derive as the side panel, and the same reason: the bar and the
    '' panel must not be able to disagree about where the boundary is.
    dim as long nGrabOut       = PsScaleBy( SH_SPLITTER_GRAB, f )
    dim as long nOutputReserve = 0
    dim as long nOutputH       = st.nOutputH

    '' A FLOATING panel reserves nothing and takes the else branch. The test is explicit
    '' rather than folded into the visibility one -- see the note on ShellLayoutState.
    dim as boolean bDockedAndVisible = (st.bOutputFloating = false) andalso st.bOutputVisible
    if bDockedAndVisible then
        dim as long nBarPos = ClampTo( H - nStatusH - nOutputH - nGrabOut, _
                                       nTop + PsScaleBy(SH_OUTPUT_MIN_EDITOR, f), _
                                       H - nStatusH - PsScaleBy(SH_OUTPUT_TABS_HEIGHT, f) - nGrabOut )
        nOutputH = H - nStatusH - (nBarPos + nGrabOut)
        if nOutputH < 0 then nOutputH = 0

        g_splitOutput->SetBounds( PsRc(nLeft, nBarPos, W - nLeft, nGrabOut) )
        g_output->SetBounds( PsRc(nLeft, nBarPos + nGrabOut, W - nLeft, nOutputH) )
        g_splitOutput->bVisible = true
        g_output->bVisible = true
        nOutputReserve = nOutputH + nGrabOut
    else
        nOutputH = 0
        g_splitOutput->bVisible = false
        g_output->bVisible = false
    end if

    '' ---- the document rect, which every band above exists to produce -------------------
    '' Computed for BOTH branches. frmMain used to build this inside its `if pDoc then`,
    '' which meant that with a document-less tab active nothing computed it at all.
    g_rcDoc.x = nLeft
    g_rcDoc.y = nTop
    dim as long nDocRight = W
    if st.bExplorerRight then nDocRight = W - nPanelReserve
    g_rcDoc.w = nDocRight - nLeft
    g_rcDoc.h = (H - nStatusH - nOutputReserve) - nTop
    if g_rcDoc.w < 0 then g_rcDoc.w = 0
    if g_rcDoc.h < 0 then g_rcDoc.h = 0

    '' Find in Project occupies exactly the document rect when its tab is active, and the
    '' editor is not shown at all then.
    if st.bFipActive then
        g_fip->SetBounds( g_rcDoc )
        g_fip->bVisible = true
        g_view->bVisible  = false
        g_vscroll->bVisible = false
        g_hscroll->bVisible = false
        exit sub
    end if
    g_fip->bVisible = false

    '' ---- THE EDITOR AND ITS TWO SCROLLBARS ---------------------------------------------
    '' frmMain_PositionMainDocBottom (frmMain.inc:743), the SplitNone arm. The split modes
    '' are commit 10; until then the whole document rect is one view.
    ''
    '' THE HORIZONTAL SCROLLBAR'S HEIGHT IS ALWAYS RESERVED, VISIBLE OR NOT, and the comment
    '' at frmMain.inc:759-762 says why: otherwise the VERTICAL scrollbar changes length every
    '' time the H bar appears, and it visibly jumps. A reserve that depends on visibility is
    '' the bug, not the fix.
    ''
    '' The vertical bar's WIDTH is measured -- AfxGetWindowWidth in tiko -- so it is state,
    '' like the six sizes above. Its height is the editor's PLUS the reserved H strip, so the
    '' two bars meet in the corner rather than leaving a notch.
    dim as long nVScrollW = st.nVScrollW
    dim as long nHScrollH = PsScaleBy( SH_SCROLLBAR_HEIGHT, f )
    dim as long nGrabV    = PsScaleBy( SH_SPLITTER_GRAB, f )
    dim as long nGrabH    = PsScaleBy( SH_SPLITTER_GRAB, f )

    '' NOT named L/T/R/B. `L` is tiko's LOCALIZATION MACRO -- #Define L(e,s) LL(e) in
    '' app/modLocalization.bi -- and fbc is case-insensitive, so a local called L turns
    '' every later L(id) in this file into a variable reference. It compiles far enough to
    '' be confusing.
    dim as long nDocL = g_rcDoc.x
    dim as long nDocT = g_rcDoc.y
    dim as long nDocR = g_rcDoc.x + g_rcDoc.w
    dim as long nDocB = g_rcDoc.y + g_rcDoc.h

    '' At most ONE bar is shown. tiko keeps two PsSplitter instances because orientation is
    '' fixed at creation, and follows the active document's mode.
    g_splitV->bVisible = false
    g_splitH->bVisible = false
    g_view2->bVisible  = false
    g_vscroll2->bVisible = false
    g_hscroll2->bVisible = false

    select case st.nSplitMode

    case SH_SPLIT_LEFTRIGHT
        '' frmMain.inc:1048 -- the bar is clamped between the two panes' scrollbar edges,
        '' and the clamped value is taken back so the bar and the panes cannot disagree.
        dim as long nSplitX = ClampTo( st.nSplitX, nDocL + nVScrollW, nDocR - nVScrollW - nGrabV )
        g_splitV->SetBounds( PsRc(nSplitX, nDocT, nGrabV, nDocB - nDocT) )
        g_splitV->bVisible = true

        '' frmMain_PositionSplitDocLeft (:608) -- the SPLIT pane, DocView(1), on the left.
        dim as long wLeft = nSplitX - nDocL - nVScrollW
        if wLeft < 0 then wLeft = 0
        g_view2->SetBounds( PsRc(nDocL, nDocT, wLeft, (nDocB - nDocT) - nHScrollH) )
        g_view2->bVisible = true
        g_vscroll2->SetBounds( PsRc(nDocL + wLeft, nDocT, nVScrollW, nDocB - nDocT) )
        g_vscroll2->bVisible = true
        g_hscroll2->SetBounds( PsRc(nDocL, nDocB - nHScrollH, wLeft, nHScrollH) )
        g_hscroll2->bVisible = st.bHScrollVisible

        '' frmMain_PositionMainDocRight (:696) -- the MAIN pane, DocView(0), on the right.
        '' Max() against rcDoc.left, exactly as the original: an unclamped SplitX could put
        '' the main pane left of the document area entirely.
        dim as long nLeft2 = nSplitX + nGrabV
        if nLeft2 < nDocL then nLeft2 = nDocL
        dim as long wRight = nDocR - nLeft2 - nVScrollW
        if wRight < 0 then wRight = 0
        g_view->SetBounds( PsRc(nLeft2, nDocT, wRight, (nDocB - nDocT) - nHScrollH) )
        g_view->bVisible = true
        g_vscroll->SetBounds( PsRc(nLeft2 + wRight, nDocT, nVScrollW, nDocB - nDocT) )
        g_vscroll->bVisible = true
        g_hscroll->SetBounds( PsRc(nLeft2, nDocB - nHScrollH, wRight, nHScrollH) )
        g_hscroll->bVisible = st.bHScrollVisible

    case SH_SPLIT_TOPBOTTOM
        dim as long nSplitY = ClampTo( st.nSplitY, nDocT, nDocB - nGrabH )
        g_splitH->SetBounds( PsRc(nDocL, nSplitY, nDocR - nDocL, nGrabH) )
        g_splitH->bVisible = true

        '' frmMain_PositionSplitDocTop (:651) -- DocView(1), above the bar.
        dim as long wPane = (nDocR - nDocL) - nVScrollW
        if wPane < 0 then wPane = 0
        dim as long hTop = nSplitY - nDocT - nHScrollH
        if hTop < 0 then hTop = 0
        g_view2->SetBounds( PsRc(nDocL, nDocT, wPane, hTop) )
        g_view2->bVisible = true
        '' AND THE TOP PANE'S SCROLLBAR DOES NOT SPAN THE RESERVED STRIP, while the bottom
        '' one below does. frmMain.inc:685 passes AfxGetWindowHeight(DocView(1)) and :783
        '' passes AfxGetWindowHeight(DocView(0)) + iHScrollbarHeight. The asymmetry is real
        '' and reproduced; the oracle records it as 95 against 395.
        g_vscroll2->SetBounds( PsRc(nDocL + wPane, nDocT, nVScrollW, hTop) )
        g_vscroll2->bVisible = true
        g_hscroll2->SetBounds( PsRc(nDocL, nSplitY - nHScrollH, wPane, nHScrollH) )
        g_hscroll2->bVisible = st.bHScrollVisible

        '' frmMain_PositionMainDocBottom (:743) with a non-zero nTop -- DocView(0), below.
        dim as long nTop2 = nSplitY + nGrabH
        if nTop2 < nDocT then nTop2 = nDocT
        dim as long hBot = nDocB - nTop2 - nHScrollH
        if hBot < 0 then hBot = 0
        g_view->SetBounds( PsRc(nDocL, nTop2, wPane, hBot) )
        g_view->bVisible = true
        g_vscroll->SetBounds( PsRc(nDocL + wPane, nTop2, nVScrollW, hBot + nHScrollH) )
        g_vscroll->bVisible = true
        g_hscroll->SetBounds( PsRc(nDocL, nDocB - nHScrollH, wPane, nHScrollH) )
        g_hscroll->bVisible = st.bHScrollVisible

    case else
        '' frmMain_PositionMainDocBottom with nTop = rcDoc.top. See the reserve note below.
        dim as long nEditW = g_rcDoc.w - nVScrollW
        dim as long nEditH = g_rcDoc.h - nHScrollH
        if nEditW < 0 then nEditW = 0
        if nEditH < 0 then nEditH = 0

        g_view->SetBounds( PsRc(nDocL, nDocT, nEditW, nEditH) )
        g_view->bVisible = true
        g_vscroll->SetBounds( PsRc(nDocL + nEditW, nDocT, nVScrollW, nEditH + nHScrollH) )
        g_vscroll->bVisible = true
        g_hscroll->SetBounds( PsRc(nDocL, nDocT + nEditH, nEditW, nHScrollH) )
        g_hscroll->bVisible = st.bHScrollVisible
    end select
end sub

'' The pump and the self-test both call this, so the live state is threaded from one place.
sub LayoutAll( byref surf as PsSurface )
    Shell_LayoutAll( surf, g_state )
end sub

sub OnMenusClosed( byval pHost as any ptr, byval ud as any ptr )
    if g_menubar then g_menubar->NotifyClosed()
end sub

'' THE OTHER DIRECTION, and a different callback for a reason. OnClosed is the HOST telling
'' the BAR its popup went away; this is the BAR asking the HOST to take it away. It fires
'' from PsMenuBar.CloseMenu, which runs when the user clicks the title that is ALREADY OPEN
'' -- PsMenuBar.inc:324-328 is explicit that "clicking the thing you just clicked" must
'' dismiss rather than reopen -- when the pointer leaves the bar sideways, and when a title
'' is disabled while its menu is up.
''
'' Unwired, the bar clears its own state and the dropdown simply stays on screen. Nothing in
'' PsPlatform was wiring it either; ideshell has the same fix.
''
'' NO LOOP: CloseAll fires OnClosed, which calls NotifyClosed, which clears the bar's fields
'' DIRECTLY without calling back (PsMenuBar.inc:195-200). CloseMenu also early-outs when the
'' bar already thinks it is closed.
sub OnBarCloseRequest( byval pBar as any ptr, byval ud as any ptr )
    g_menus.CloseAll()
end sub

sub OnMenuCommand( byval pMenu as any ptr, byval nId as long, byval ud as any ptr )
    '' Printed first, always, so a command ARRIVING is visible even when nothing is bound
    '' to it. Most ids reach nothing here and that is the design -- their handlers are in
    '' tiko.exe and need the document model this binary does not have.
    print "tikoshell: command " & str(nId) & "  (" & getMenuText(nId).Utf8 & ")"

    '' ---- THE VIEW TOGGLES THAT MAP ONTO ShellLayoutState -------------------------------
    '' These are the exception, and they earn it: every one is a field of the state record
    '' and needs no model at all. Without them the twelve layout states are reachable ONLY
    '' from --selftest, which is how the mirrored panel shipped for two commits with the
    '' icon strip drawn over it -- asserted by the oracle, never once looked at.
    ''
    '' tiko's own handlers are in frmMainView.inc and do far more than this: they move
    '' HWNDs, adjust SplitX when the panel width changes, and repaint. Here the layout is a
    '' pure function of the record, so flipping a field IS the whole operation.
    dim as boolean bHandled = true
    select case nId
        case IDM_EXPLORERPOSITION : g_state.bExplorerRight = not g_state.bExplorerRight
        case IDM_VIEWSIDEPANEL    : g_state.bPanelVisible  = not g_state.bPanelVisible
        case IDM_VIEWOUTPUT       : g_state.bOutputVisible = not g_state.bOutputVisible

        '' The split modes TOGGLE OFF when re-selected, exactly as OnCommand_ViewSplit does,
        '' and each sets its position from the unsplit pane's extent -- see RunLayoutDump for
        '' why the midpoint and the bare height/2 differ, which is tiko's inconsistency.
        case IDM_SPLITLEFTRIGHT
            if g_state.nSplitMode = SH_SPLIT_LEFTRIGHT then
                g_state.nSplitMode = SH_SPLIT_NONE
            else
                g_state.nSplitMode = SH_SPLIT_LEFTRIGHT
                g_state.nSplitX = 0
                if g_pSurf then Shell_LayoutAll( *g_pSurf, g_state )
                g_state.nSplitX = g_view->bounds.x + (g_view->bounds.w \ 2)
            end if
        case IDM_SPLITTOPBOTTOM
            if g_state.nSplitMode = SH_SPLIT_TOPBOTTOM then
                g_state.nSplitMode = SH_SPLIT_NONE
            else
                g_state.nSplitMode = SH_SPLIT_TOPBOTTOM
                g_state.nSplitY = 0
                if g_pSurf then Shell_LayoutAll( *g_pSurf, g_state )
                g_state.nSplitY = g_view->bounds.h \ 2
            end if

        case else : bHandled = false
    end select

    if bHandled andalso (g_pSurf <> 0) then
        LayoutAll( *g_pSurf )
        g_pSurf->InvalidateAll()
    end if
end sub


'' ========================================================================================
'' --dump-layout: THE SAME STATES THE ORACLE DUMPS, IN THE ORACLE'S FORMAT.
''
'' The self-test asserts RELATIONS -- this band above that one, the children tiling the
'' client area -- and relations are satisfied by a wrong-but-self-consistent layout. This
'' prints NUMBERS, so `diff` against docs/port/layout-oracle/ answers the question relations
'' cannot: are the bands where tiko puts them.
''
'' The state's six MEASURED sizes are set from the oracle's header rather than guessed. They
'' are inputs -- read out of live controls in tiko, and unavailable to a widget tree before
'' it lays out -- so a comparison that did not take them from the same place would be
'' comparing two different questions.
'' ========================================================================================
sub DumpChild( byval szName as zstring ptr, byval p as PsWidget ptr )
    if p = 0 then
        print "  " & *szName & " absent"
        exit sub
    end if
    if p->bVisible = false then
        print "  " & *szName & " hidden"
        exit sub
    end if
    '' The oracle prints left,top,RIGHT,BOTTOM and then the extent. PsRect is x,y,w,h.
    print "  " & *szName & " " & _
          str(p->bounds.x) & "," & str(p->bounds.y) & "," & _
          str(p->bounds.x + p->bounds.w) & "," & str(p->bounds.y + p->bounds.h) & _
          " (" & str(p->bounds.w) & "x" & str(p->bounds.h) & ")"
end sub

sub DumpState( byref surf as PsSurface, byval szState as zstring ptr )
    Shell_LayoutAll( surf, g_state )
    print
    print "[" & *szState & "]"
    DumpChild( @"MENUBAR",       g_menubar )
    DumpChild( @"STATUSBAR",     g_status )
    DumpChild( @"PANEL",         g_panel )
    DumpChild( @"SPLITPANEL",    g_splitPanel )
    DumpChild( @"TOPTABS",       g_tabs )
    DumpChild( @"TOPTABSMENU",   g_topTabsMenu )
    DumpChild( @"TOPTABSINFO",   g_barInfo )
    DumpChild( @"FIND",          g_barFind )
    DumpChild( @"REPLACE",       g_barReplace )
    DumpChild( @"SPLITOUTPUT",   g_splitOutput )
    DumpChild( @"OUTPUT",        g_output )
    DumpChild( @"FINDINPROJECT", g_fip )
    DumpChild( @"SPLITV",        g_splitV )
    DumpChild( @"SPLITH",        g_splitH )
    DumpChild( @"EDIT0",         g_view )
    DumpChild( @"EDIT1",         g_view2 )
    DumpChild( @"HSCROLL0",      g_hscroll )
    DumpChild( @"HSCROLL1",      g_hscroll2 )
    DumpChild( @"VSCROLL0",      g_vscroll )
    DumpChild( @"VSCROLL1",      g_vscroll2 )
end sub

sub RunLayoutDump( byref surf as PsSurface )
    '' The oracle's own inputs, from its header. Six measured sizes and the client size.
    surf.fScale = 1.75
    surf.Resize( 1400, 900 )
    g_state.nMenubarH     = 52
    g_state.nStatusH      = 46
    g_state.nPanelW       = 413
    g_state.nTopTabsMenuW = 186
    g_state.nTabsH        = 63
    g_state.nOutputH      = 194
    g_state.nTabCount     = 3
    if surf.pRoot then surf.pRoot->PropagateScaleChanged( surf.fScale )

    print "TIKO SHELL LAYOUT"
    print "client " & str(surf.w) & "x" & str(surf.h)
    print "scale " & str(surf.fScale)

    g_state.bPanelVisible = true : g_state.bExplorerRight = false
    g_state.bOutputVisible = true : g_state.bOutputFloating = false
    g_state.bShowInfo = false : g_state.bShowFind = false : g_state.bShowReplace = false
    g_state.bFipActive = false
    DumpState( surf, @"BASE panel=left output=docked bars=none" )

    g_state.bPanelVisible = false
    DumpState( surf, @"PANEL_HIDDEN" )
    g_state.bPanelVisible = true

    g_state.bExplorerRight = true
    DumpState( surf, @"PANEL_RIGHT" )
    g_state.bExplorerRight = false

    g_state.bOutputVisible = false
    DumpState( surf, @"OUTPUT_HIDDEN" )
    g_state.bOutputVisible = true

    g_state.bShowInfo = true
    DumpState( surf, @"BAR_INFO" )
    g_state.bShowInfo = false

    g_state.bShowFind = true
    DumpState( surf, @"BAR_FIND" )
    g_state.bShowFind = false

    g_state.bShowReplace = true
    DumpState( surf, @"BAR_REPLACE" )
    g_state.bShowReplace = false

    g_state.bShowInfo = true : g_state.bShowFind = true : g_state.bShowReplace = true
    DumpState( surf, @"BAR_ALL" )
    g_state.bShowInfo = false : g_state.bShowFind = false : g_state.bShowReplace = false

    '' The two split modes, positioned the way OnCommand_ViewSplit does it: set the mode,
    '' lay out ONCE so the unsplit view has a real rectangle, then halve THAT. Setting the
    '' mode without a position dumps a degenerate split -- the same trap the oracle itself
    '' fell into, recorded in modLayoutDump.inc.
    g_state.nSplitMode = SH_SPLIT_LEFTRIGHT
    g_state.nSplitX = 0
    Shell_LayoutAll( surf, g_state )
    '' MIDPOINT for X, exactly as OnCommand_ViewSplit does it (frmMainView.inc:88).
    g_state.nSplitX = g_view->bounds.x + (g_view->bounds.w \ 2)
    DumpState( surf, @"SPLIT_LEFTRIGHT" )

    g_state.nSplitMode = SH_SPLIT_TOPBOTTOM
    g_state.nSplitX = 0 : g_state.nSplitY = 0
    Shell_LayoutAll( surf, g_state )
    '' AND A BARE HEIGHT/2 FOR Y, which is NOT the midpoint and is tiko's, not a slip
    '' here: frmMainView.inc:118 is `pDoc->SplitY = (rc.bottom - rc.top) \ 2` while :88 is
    '' `rc.left + (rc.right - rc.left) / 2`. So a top/bottom split is only centred when the
    '' document rect starts at y = 0, which it never does -- the menubar and tab bar are
    '' above it. The bar lands high and the top pane is the smaller one.
    ''
    '' Reproduced because the oracle records it and the layout takes nSplitY as an absolute
    '' Y either way; where the CALLER puts it is the caller's business, and fixing that is
    '' a change to tiko rather than a port of it.
    g_state.nSplitY = g_view->bounds.h \ 2
    DumpState( surf, @"SPLIT_TOPBOTTOM" )

    g_state.nSplitMode = SH_SPLIT_NONE

    print
    print "LAYOUT DUMP COMPLETE"
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

    '' 4. AND THE EDITOR'S OWN FONT, which is a FOURTH thing and not the same as step 1.
    ''    PsTextEngine draws the WIDGETS; Scintilla keeps its own style table and its own
    ''    size, so reopening the engine leaves the editor's text at whatever it was created
    ''    with. The symptom is a perfectly scaled shell with tiny code in the middle of it,
    ''    which is what the author saw after commit 9 -- and minieditor is the only demo in
    ''    PsPlatform that does this, so ideshell has it too.
    ''
    ''    ORDER IS LOAD-BEARING: SetFontPixelSize ends in SCI_STYLECLEARALL, which
    ''    propagates STYLE_DEFAULT over every other style -- including the MARGIN colours
    ''    StyleEditorFromTheme sets AFTER its own CLEARALL. Font first, theme second, or the
    ''    white gutter strip comes back.
    dim as long pxEdit = PsScaleBy( SH_FONT_PX, f )
    if g_view  then g_view->SetFontPixelSize( pxEdit )
    if g_view2 then g_view2->SetFontPixelSize( pxEdit )
    StyleEditorFromTheme( surf )
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
    dim as boolean bDumpLayout = false
    for i as integer = 1 to __FB_ARGC__ - 1
        if command(i) = "--selftest" then bSelfTest = true
        if command(i) = "--dump-layout" then bDumpLayout = true
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
    dim as DWSTRING sLangDir = PsExePath & "../settings/languages/"
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

    '' ---- A REAL tiko THEME, not an inline palette -----------------------------------
    '' PsTheme reads tiko's own .theme format unchanged -- same keys, same roles, same
    '' key -> role -> built-in resolution -- so this is the whole point of that being
    '' true, exercised rather than asserted in the abstract. arctic.theme names no widget
    '' keys at all, which is the path EIGHT of tiko's ten themes take.
    scope
        '' FORWARD SLASHES, and not for portability. fbc is processing backslash escapes
        '' in this translation unit, so "..\settings	hemesrctic.theme" arrives with a TAB
        '' where 	 was and a BELL where  was -- the path in the error message reads
        '' "..\settings<tab>hemes<bell>rctic.theme". Windows takes / everywhere, and the
        '' sLangDir above got away with it only because none of its letters is an escape.
        dim as DWSTRING sTheme = PsExePath & "../settings/themes/arctic.theme"
        dim as long n = PsThemeLoadFile( sTheme )
        if n = 0 then
            print "tikoshell: no theme loaded from " & sTheme.Utf8
        end if
        PsThemeApply( surf.pRoot )
        StyleEditorFromTheme( surf )
    end scope

    LayoutAll( surf )

    '' ---------------------------------------------------------------------- dump
    if bDumpLayout then
        RunLayoutDump( surf )
        TE_Free( g_te )
        PsPlatformShutdown()
        end 0
    end if

    '' ---------------------------------------------------------------------- selftest
    if bSelfTest then
        print "--- tikoshell selftest ---"

        Check "the tree is built", (surf.pRoot <> 0)
        '' Twenty, and that is the whole of frmMain's child list bar the panel's own
        '' contents: the real menubar and statusbar, TWO real PsSciViews, and sixteen stubs.
        '' The plan called this "twenty children" before any of it was written.
        Check "  twenty children", (surf.pRoot->ChildCount() = 20), _
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

        '' ---- AND CLOSING THE CHAIN CLEARS THE BAR ------------------------------------
        '' Reported: after clicking a menu row the popup stayed up and the title stayed
        '' highlighted. Two causes, both of them missing wiring rather than logic:
        ''
        ''   PsMenuHostOnCommand existed in PsPlatform, said in its own comment that
        ''   "running something and leaving the menus up is the one behaviour no menu has",
        ''   and was NEVER INSTALLED. No menu in any host had ever closed itself.
        ''
        ''   And nothing told the BAR. PsMenuBar.bi:135-138 says the host must, or the bar
        ''   still believes a menu is open -- the title stays lit and the next hover
        ''   re-opens it. That is g_menus.OnClosed, wired in BuildTree.
        ''
        '' THE CALLBACK IS DRIVEN DIRECTLY, AND THE REASON IS A LIMIT WORTH STATING.
        '' g_menus.CloseAll() does NOT fire it here: PsMenuHost only notifies when
        '' something was actually open, and OpenRoot declined at the start of this section
        '' because PsPopupHost.OpenAt needs a real hWin. So the host never opened, never
        '' closes, and never calls back.
        ''
        '' What is asserted is therefore the SHELL's half -- that the callback clears the
        '' bar. That the HOST calls it is PsPlatform's half, and it is covered by the line
        '' this fix added to PsMenuHostWire rather than by anything here.
        g_menus.CloseAll()
        OnMenusClosed( 0, 0 )
        Check "the closed callback clears the bar's highlight", _
              (g_menubar->IsMenuOpen() = false), "IsMenuOpen"

        '' ---- AND THE ROUND TRIP TERMINATES ---------------------------------------------
        '' Two callbacks point at each other: the bar asks the host to close
        '' (OnCloseRequest -> CloseAll) and the host tells the bar it closed
        '' (OnClosed -> NotifyClosed). That is a loop unless one end stops calling back,
        '' and one does -- NotifyClosed clears the bar's fields DIRECTLY
        '' (PsMenuBar.inc:195-200) rather than going through CloseMenu.
        ''
        '' Asserted because the failure mode is a stack overflow on a mouse click, which is
        '' not something to discover interactively.
        g_menubar->OpenMenu( 0 )
        Check "the bar reports a menu open again", (g_menubar->IsMenuOpen() = true)
        g_menubar->CloseMenu()
        Check "  the bar's own close request completes", _
              (g_menubar->IsMenuOpen() = false)
        Check "  and does not leave the host open", (g_menus.IsOpen() = false)

        '' ---- HOVER-SWITCHING SURVIVES ITS OWN REOPEN ----------------------------------
        '' Reported: with a menu down, moving along the bar highlighted the next title but
        '' left the first title's dropdown up. Three components interacting, and no single
        '' one of them wrong:
        ''
        ''   PsMenuBar.OpenMenu sets bMenuOpen and nActive and THEN fires OnOpenRequest.
        ''   The host's OpenRoot begins by closing whatever is up.
        ''   That close reached OnClosed -> NotifyClosed, which cleared both fields.
        ''
        '' So the bar forgot the menu it was in the middle of opening, and PsMenuBar
        '' switches only while bMenuOpen is true -- the NEXT hover did nothing. Caused by
        '' wiring OnClosed two commits ago: the callback was right, and reporting an
        '' internal close as a real one was not. PsMenuHost suppresses it while reopening.
        ''
        '' AND THIS ASSERTION DOES NOT COVER THAT FIX. Stated rather than implied, because
        '' it passes with the suppression removed -- checked. OpenRoot never succeeds
        '' windowlessly, so nDepth stays 0, so CloseAll's `bWas` is false and pfnClosed
        '' never fires at all. The path the defect lives on is unreachable from here.
        ''
        '' What it DOES cover is the bar's own state machine across repeated OpenMenu calls,
        '' which is cheap and worth having. The fix itself is verified by the author moving
        '' a mouse, and by nothing else -- which is the third time in this file that the
        '' obvious assertion turned out to constrain nothing, and the third time the revert
        '' habit is what said so.
        scope
            g_menubar->OpenMenu( 0 )
            Check "with a menu open the bar says so", (g_menubar->IsMenuOpen() = true)

            '' The hover-switch: a second OpenMenu while the first is still up.
            g_menubar->OpenMenu( 1 )
            Check "  switching to the next title keeps the bar open", _
                  (g_menubar->IsMenuOpen() = true), "IsMenuOpen after switch"
            Check "    and it is now the SECOND title that is active", _
                  (g_menubar->GetActive() = 1), str(g_menubar->GetActive())

            '' And a third, because the failure only showed from the second switch on.
            g_menubar->OpenMenu( 2 )
            Check "  and again, so tracking does not decay", _
                  (g_menubar->IsMenuOpen() = true) andalso (g_menubar->GetActive() = 2), _
                  str(g_menubar->GetActive())

            g_menus.CloseAll()
            OnMenusClosed( 0, 0 )
            Check "  a REAL close still clears the bar", (g_menubar->IsMenuOpen() = false)
        end scope

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

        '' A RESIZE MUST RE-LAY-OUT, and the DOCUMENT RECT is what absorbs the change --
        '' every band above it is either pinned or a measured height.
        scope
            dim as long hDocBefore = g_rcDoc.h
            surf.Resize( 640, 480 )
            LayoutAll( surf )
            Check "a resize re-lays out", _
                  (g_menubar->bounds.w = 640), str(g_menubar->bounds.w)
            Check "  the statusbar follows the bottom edge", _
                  (g_status->bounds.y + g_status->bounds.h = 480)
            Check "  and the document rect absorbed the change", _
                  (g_rcDoc.h <> hDocBefore)
            surf.Resize( SH_W, SH_H )
            LayoutAll( surf )
        end scope

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

            '' THE MENUBAR BAND DOES NOT SCALE, AND THAT IS CORRECT NOW. Its height is a
            '' MEASURED pixel size -- frmMain reads it with AfxGetWindowHeight and uses it
            '' unscaled -- so a scale change moves the constants around it, not it. The
            '' assertion that stood here expected PsScaleBy(SH_MENUBAR_H, 1.5) and was
            '' describing the commit-4 layout, where the band was a design constant.
            Check "at 1.5 the MEASURED menubar height is unchanged", _
                  (g_menubar->bounds.h = hMenu1), _
                  str(hMenu1) & " -> " & str(g_menubar->bounds.h)
            Check "  the status bar still owns the bottom", _
                  (g_status->bounds.y + g_status->bounds.h = surf.h)
            '' What DOES move is everything expressed as a design constant. The info-absent
            '' margin is the smallest of them and the easiest to lose.
            Check "  but the scaled constants do move", _
                  (g_rcDoc.y = g_tabs->bounds.y + g_tabs->bounds.h + _
                               PsScaleBy(SH_INFO_ABSENT_MARGIN, 1.5)), _
                  str(g_rcDoc.y)
            '' THE HALF THAT WAS ACTUALLY BROKEN, AND IS ACTUALLY ASSERTABLE.
            Check "  and the font was reopened at the scaled size", _
                  (g_nFontPx = PsScaleBy(SH_FONT_PX, 1.5)), str(g_nFontPx) & "px"

            '' AND THE EDITOR'S OWN FONT, WHICH IS A DIFFERENT FONT. Reported by the author
            '' after commit 9: a perfectly scaled shell with tiny code in the middle of it.
            '' PsTextEngine draws the widgets; Scintilla keeps its own style table and its
            '' own size, so nothing above reaches it.
            ''
            '' Read back in POINTS, which is what Scintilla stores -- SetFontPixelSize
            '' converts with (px * 72 + 48) \\ 96, so the assertion has to convert too rather
            '' than compare a pixel size to a point size and always fail.
            scope
                '' Not in PsScintilla.bi -- the toolkit's controls have never read a style
                '' size back. Scintilla's value, from Scintilla.h.
                const SCI_STYLEGETSIZE_ = 2485
                dim as long pxWant = PsScaleBy(SH_FONT_PX, 1.5)
                dim as long ptWant = (pxWant * 72 + 48) \ 96
                Check "  and the EDITOR's font scaled too", _
                      (g_view->Msg(SCI_STYLEGETSIZE_, STYLE_DEFAULT) = ptWant), _
                      str(g_view->Msg(SCI_STYLEGETSIZE_, STYLE_DEFAULT)) & "pt vs " & str(ptWant)
                Check "    in BOTH panes", _
                      (g_view2->Msg(SCI_STYLEGETSIZE_, STYLE_DEFAULT) = ptWant)
                '' AND THE MARGIN SURVIVED IT. SetFontPixelSize ends in STYLECLEARALL, which
                '' would wipe STYLE_LINENUMBER if the theme were not reapplied after it.
                Check "    with the margin still themed", _
                      (g_view->Msg(SCI_STYLEGETBACK, STYLE_LINENUMBER) = _
                       ToBgr(PsThemeRoleColor(PSTHEME_BACKGROUND)))
            end scope

            '' NOT ASSERTED, and the reason matters because the obvious assertion is
            '' VACUOUS. The tempting one is
            ''     g_menubar->ScaleY(100) = PsScaleBy(100, 1.5)
            '' and this file carried it for one commit as "the assertion that would have
            '' caught the bug". It would not have: PsWidget.ScaleY calls SurfaceScale(),
            '' which reads surf.fScale LIVE (PsWidget.inc:435, :419), so it passes whether
            '' or not the tree was ever told. It tests the surface, not the tree.
            ''
            '' What PropagateScaleChanged actually does is InvalidateLayout across the
            '' subtree -- OnScaleChanged is an empty base with no override anywhere in
            '' PsPlatform today. Skipping it leaves a control holding geometry it computed
            '' at the old factor until something else invalidates it, and nothing exposed
            '' here can observe that from outside. It is called because the contract says
            '' to, and the assertion that would prove it does not exist.

            ApplyScale( surf, 1.0 )
            LayoutAll( surf )
            Check "  back at 1.0 the margin returns", _
                  (g_rcDoc.y = g_tabs->bounds.y + g_tabs->bounds.h + _
                               PsScaleBy(SH_INFO_ABSENT_MARGIN, 1.0)), str(g_rcDoc.y)
        end scope

        '' ---- ACCELERATORS, FROM tiko's OWN 109 BINDINGS -------------------------------
        Check "the binding table loaded", (ubound(gKeys) > 100), _
              str(ubound(gKeys) + 1) & " commands"
        Check "  and every chord in it parsed", (g_nAccelSkipped = 0), _
              str(g_nAccelSkipped) & " skipped"
        Check "  producing accelerators", (g_accel.Count() > 30), _
              str(g_accel.Count()) & " bound"

        '' THE SAME ARRAY FEEDS THE LABEL AND THE SHORTCUT, which is the reason for
        '' building the table out of gKeys rather than hand-listing chords: a menu row that
        '' says Ctrl+S and a key that does nothing is the failure this makes unreachable.
        scope
            dim as DWSTRING sLabel = getMenuAccelText( IDM_FILESAVE )
            Check "the Save menu row carries an accelerator label", (PsLen(sLabel) > 0), _
                  sLabel.Utf8
            dim as long k, m
            '' getMenuAccelText prefixes chr(9) -- it packs caption and accel for the
            '' menu painter -- so the tab has to come off before parsing.
            Check "  and the SAME chord resolves through PsAccel", _
                  PsAccelParse( PsMid(sLabel, 2), k, m ), sLabel.Utf8
            Check "  to the command the menu row names", _
                  (g_accel.FindKey(k, m) = IDM_FILESAVE), str(g_accel.FindKey(k, m))
        end scope

        '' A SYNTHETIC KEY PRESS REACHES THE COMMAND. This is the end-to-end claim: an
        '' event in, a tiko IDM_ id out, through the table the pump consults.
        scope
            dim as PsEvent ev
            ev.kind = PSEV_KEY_DOWN
            ev.key.key = PSKEY_S
            ev.key.modifiers = PSMOD_CTRL
            Check "Ctrl+S finds Save", (g_accel.Find(@ev) = IDM_FILESAVE), _
                  str(g_accel.Find(@ev))

            '' EXACT MATCHING, ON REAL DATA rather than on a two-entry fixture. Ctrl+Shift+S
            '' is Save As in tiko's defaults, so these two are a live pair.
            ev.key.modifiers = PSMOD_CTRL or PSMOD_SHIFT
            Check "  Ctrl+Shift+S is a DIFFERENT command", _
                  (g_accel.Find(@ev) <> IDM_FILESAVE), str(g_accel.Find(@ev))

            ev.key.modifiers = PSMOD_CTRL or PSMOD_ALT
            Check "  Ctrl+Alt+S is neither", (g_accel.Find(@ev) = 0), str(g_accel.Find(@ev))

            '' A key UP must not fire it a second time.
            ev.kind = PSEV_KEY_UP
            ev.key.modifiers = PSMOD_CTRL
            Check "  and the key UP does not fire it again", (g_accel.Find(@ev) = 0)
        end scope

        '' THE COMMIT-4 STUB IS REAL NOW. It returned 0 for everything, so every User Tools
        '' row rendered with blank shortcut text.
        Check "the pick-list resolver answers", _
              (KeyBindings_PickListKeyToValue(DWSTRING("F5")) = PSKEY_F5)
        Check "  and still treats None as no key", _
              (KeyBindings_PickListKeyToValue(DWSTRING("None")) = 0)

        '' ALT+MNEMONIC. Faked host-side; what is asserted is that it CLAIMS the event and
        '' opens something, not that the policy is tiko's -- it is not, and the file says so.
        scope
            g_nBarOpenCalls = 0
            dim as PsEvent ev
            ev.kind = PSEV_KEY_DOWN
            ev.key.key = PSKEY_F
            ev.key.modifiers = PSMOD_ALT
            Check "Alt+F claims the event", TryAltMnemonic(@ev)
            Check "  and asked the host to open a menu", (g_nBarOpenCalls = 1), _
                  str(g_nBarOpenCalls)
            g_menus.CloseAll() : g_menubar->NotifyClosed()

            '' A letter no title starts with must NOT be claimed, or Alt+anything swallows
            '' every keystroke the editor would otherwise get.
            ev.key.key = PSKEY_Z
            Check "  Alt+Z is not claimed", (TryAltMnemonic(@ev) = false)
            '' And without Alt it is an ordinary keypress.
            ev.key.key = PSKEY_F
            ev.key.modifiers = PSMOD_NONE
            Check "  plain F is not claimed", (TryAltMnemonic(@ev) = false)
        end scope

        '' ---- THE BAND WALK ------------------------------------------------------------
        '' Driven at the ORACLE's inputs so a failure here and a line in
        '' docs/port/layout-oracle/ are the same numbers.
        scope
            surf.fScale = 1.75
            surf.Resize( 1400, 900 )
            g_state.nMenubarH = 52 : g_state.nStatusH = 46 : g_state.nPanelW = 413
            g_state.nTopTabsMenuW = 186 : g_state.nTabsH = 63 : g_state.nOutputH = 194
            g_state.nTabCount = 3
            g_state.bPanelVisible = true : g_state.bExplorerRight = false
            g_state.bOutputVisible = true : g_state.bOutputFloating = false
            g_state.bShowInfo = false : g_state.bShowFind = false : g_state.bShowReplace = false
            g_state.bFipActive = false
            LayoutAll( surf )

            Check "menubar full width at the top", _
                  (g_menubar->bounds.y = 0) andalso (g_menubar->bounds.w = 1400)
            Check "statusbar owns the bottom", _
                  (g_status->bounds.y + g_status->bounds.h = 900)
            Check "the panel starts under the menubar", (g_panel->bounds.y = 52)
            Check "  and stops above the statusbar", _
                  (g_panel->bounds.y + g_panel->bounds.h = 854)
            Check "the splitter is on the panel's inner edge", _
                  (g_splitPanel->bounds.x = g_panel->bounds.x + g_panel->bounds.w), _
                  str(g_splitPanel->bounds.x)
            Check "the tab bar starts after the reserve", _
                  (g_tabs->bounds.x = g_splitPanel->bounds.x + g_splitPanel->bounds.w)
            Check "the output sits below its splitter", _
                  (g_output->bounds.y = g_splitOutput->bounds.y + g_splitOutput->bounds.h), _
                  str(g_output->bounds.y)
            Check "  and reaches the statusbar", _
                  (g_output->bounds.y + g_output->bounds.h = g_status->bounds.y)
            '' THE ORACLE'S OWN NUMBERS, not a relation: these are what tiko produces.
            Check "the document rect starts where tiko puts it", _
                  (g_rcDoc.y = 129), str(g_rcDoc.y)
            Check "  and ends where tiko ends it", _
                  (g_rcDoc.y + g_rcDoc.h = 649), str(g_rcDoc.y + g_rcDoc.h)
        end scope

        '' ---- COVERAGE AND DISJOINTNESS ------------------------------------------------
        '' THE LOAD-BEARING ASSERTION. Every per-widget relation above is satisfied by a
        '' layout with a hole in it or two children on top of each other; this is not.
        '' It also scales -- thirteen children now, and the same helper covers twenty.
        scope
            dim as PsWidget ptr kids(0 to 13) = { _
                g_menubar, g_status, g_panel, g_splitPanel, g_tabs, g_topTabsMenu, _
                g_barInfo, g_barFind, g_barReplace, g_splitOutput, g_output, g_fip, _
                g_view, g_vscroll }

            dim as long nOverlap = 0
            dim as string sFirst = ""
            for i as long = 0 to ubound(kids)
                if kids(i)->bVisible = false then continue for
                for j as long = i + 1 to ubound(kids)
                    if kids(j)->bVisible = false then continue for
                    dim as long ax1 = kids(i)->bounds.x, ax2 = ax1 + kids(i)->bounds.w
                    dim as long ay1 = kids(i)->bounds.y, ay2 = ay1 + kids(i)->bounds.h
                    dim as long bx1 = kids(j)->bounds.x, bx2 = bx1 + kids(j)->bounds.w
                    dim as long by1 = kids(j)->bounds.y, by2 = by1 + kids(j)->bounds.h
                    if (ax1 < bx2) andalso (bx1 < ax2) andalso _
                       (ay1 < by2) andalso (by1 < ay2) then
                        nOverlap += 1
                        if len(sFirst) = 0 then sFirst = str(i) & " x " & str(j)
                    end if
                next
            next
            Check "no two visible children overlap", (nOverlap = 0), _
                  iif(nOverlap > 0, "first: " & sFirst, "")

            '' COVERAGE, WITH THE TWO HOLES NAMED. Exact coverage is NOT an invariant of
            '' tiko's layout and asserting it was wrong -- the first version of this check
            '' failed by 14987 pixels, which turned out to be two DELIBERATE gaps:
            ''
            ''   the strip right of the top-tabs icon panel, SCROLLBAR_WIDTH_EDITOR wide.
            ''   frmMain_OnPaint paints it explicitly (frmMain.inc:539), so it is background
            ''   on purpose rather than a missing child.
            ''
            ''   the info band's top margin, ScaleY(8) tall, which its ELSE arm adds so the
            ''   find bar is not flush against the tab bar.
            ''
            '' Naming them is stronger than tolerating a shortfall: a THIRD hole is a
            '' failure, and each of these two is asserted at its own size.
            dim as longint nArea = 0
            for i as long = 0 to ubound(kids)
                if kids(i)->bVisible then
                    nArea += clngint(kids(i)->bounds.w) * clngint(kids(i)->bounds.h)
                end if
            next
            dim as longint nStrip  = clngint(surf.w - (g_topTabsMenu->bounds.x + g_topTabsMenu->bounds.w)) _
                                     * clngint(g_tabs->bounds.h)
            dim as longint nMargin = clngint(surf.w - g_rcDoc.x) _
                                     * clngint(PsScaleBy(SH_INFO_ABSENT_MARGIN, 1.75))

            '' THE THIRD GAP, AND IT IS THIS COMMIT'S MOST INTERESTING RULE. The horizontal
            '' scrollbar's height is reserved out of the editor WHETHER OR NOT THE BAR IS
            '' SHOWN (frmMain.inc:759-762) -- otherwise the VERTICAL bar changes length every
            '' time the H bar appears and visibly jumps. So with the bar hidden, its strip is
            '' a hole by design. A reserve that depended on visibility would close this gap
            '' and reintroduce the jank.
            dim as longint nHStrip = 0
            if g_hscroll->bVisible = false then
                nHStrip = clngint(g_view->bounds.w) * clngint(PsScaleBy(SH_SCROLLBAR_HEIGHT, 1.75))
            end if

            Check "  they cover the surface but for three deliberate gaps", _
                  (nArea + nStrip + nMargin + nHStrip = clngint(surf.w) * clngint(surf.h)), _
                  str(nArea) & " + " & str(nStrip) & " + " & str(nMargin) & " + " & _
                  str(nHStrip) & " vs " & str(clngint(surf.w) * clngint(surf.h))
            Check "    the strip right of the icon panel is the scrollbar reserve", _
                  (surf.w - (g_topTabsMenu->bounds.x + g_topTabsMenu->bounds.w) = _
                   PsScaleBy(SH_SCROLLBAR_WIDTH_EDITOR, 1.75)), _
                  str(surf.w - (g_topTabsMenu->bounds.x + g_topTabsMenu->bounds.w))
        end scope

        '' ---- THE CONDITIONAL BANDS ----------------------------------------------------
        scope
            '' Each hidden bar contributes ZERO and the next band moves up by exactly its
            '' height -- asserted as a difference, so a band that is off by its own height
            '' cannot hide behind a neighbour absorbing the error.
            dim as long yBase = g_rcDoc.y
            g_state.bShowFind = true : LayoutAll( surf )
            Check "showing the find bar pushes the document down by its height", _
                  (g_rcDoc.y = yBase + PsScaleBy(SH_TOPTABS_FIND_HEIGHT, 1.75)), _
                  str(yBase) & " -> " & str(g_rcDoc.y)
            g_state.bShowFind = false : LayoutAll( surf )
            Check "  and hiding it puts it back", (g_rcDoc.y = yBase)

            '' THE INFO BAND'S ELSE ARM IS NOT A NO-OP. It adds a top margin, so showing
            '' the info bar moves the document by its height MINUS that margin. This is the
            '' single most losable line in the transliteration.
            g_state.bShowInfo = true : LayoutAll( surf )
            Check "the info bar's margin is accounted for, not just its height", _
                  (g_rcDoc.y = yBase + PsScaleBy(SH_TOPTABS_INFO_HEIGHT, 1.75) _
                                     - PsScaleBy(SH_INFO_ABSENT_MARGIN, 1.75)), _
                  str(g_rcDoc.y)
            g_state.bShowInfo = false : LayoutAll( surf )

            '' THE MIRROR. gConfig.ShowPanelWidth must round-trip: the splitter strip comes
            '' off the CONTENT area on both sides, so the panel keeps its width.
            dim as long wLeft = g_panel->bounds.w
            g_state.bExplorerRight = true : LayoutAll( surf )
            Check "the panel keeps its width when mirrored", (g_panel->bounds.w = wLeft), _
                  str(wLeft) & " -> " & str(g_panel->bounds.w)
            Check "  and takes the right edge", _
                  (g_panel->bounds.x + g_panel->bounds.w = surf.w)
            Check "  with the splitter still on its INNER edge", _
                  (g_splitPanel->bounds.x + g_splitPanel->bounds.w = g_panel->bounds.x)
            Check "  and the document starting at 0", (g_rcDoc.x = 0)

            '' THE ICON STRIP MIRRORS TOO, and for two commits it did not. It came off the
            '' full client width, so docked right it was drawn ON TOP OF THE PANEL while the
            '' tab bar stopped short and left a hole. Nothing asserted the mirrored case at
            '' all -- the coverage check runs at the default state -- so the oracle found it
            '' and nothing else would have.
            Check "  the icon strip stays inside the content when mirrored", _
                  (g_topTabsMenu->bounds.x + g_topTabsMenu->bounds.w <= _
                   g_panel->bounds.x), _
                  str(g_topTabsMenu->bounds.x + g_topTabsMenu->bounds.w) & _
                  " vs panel at " & str(g_panel->bounds.x)
            Check "    with the tab bar meeting it, not stopping short", _
                  (g_tabs->bounds.x + g_tabs->bounds.w = g_topTabsMenu->bounds.x), _
                  str(g_tabs->bounds.x + g_tabs->bounds.w) & " vs " & _
                  str(g_topTabsMenu->bounds.x)
            '' And the same relation must still hold docked LEFT, which is the half the fix
            '' had to leave alone to the pixel.
            g_state.bExplorerRight = false : LayoutAll( surf )
            Check "    and the same holds docked left", _
                  (g_tabs->bounds.x + g_tabs->bounds.w = g_topTabsMenu->bounds.x), _
                  str(g_tabs->bounds.x + g_tabs->bounds.w) & " vs " & _
                  str(g_topTabsMenu->bounds.x)

            '' Tab count 0 hides both tab widgets and the document starts at the menubar
            '' plus the info band's margin.
            g_state.nTabCount = 0 : LayoutAll( surf )
            Check "no tabs hides the tab bar", (g_tabs->bVisible = false)
            Check "  and its icon strip", (g_topTabsMenu->bVisible = false)
            g_state.nTabCount = 3 : LayoutAll( surf )

            '' A FLOATING output reserves nothing -- the case IsWindowVisible alone gets
            '' wrong, because an undocked panel is still visible in its own frame.
            dim as long yDocked = g_rcDoc.y + g_rcDoc.h
            g_state.bOutputFloating = true : LayoutAll( surf )
            Check "a floating output reserves nothing", _
                  (g_rcDoc.y + g_rcDoc.h = surf.h - g_state.nStatusH), _
                  str(g_rcDoc.y + g_rcDoc.h)
            Check "  which is NOT what docked does", _
                  ((g_rcDoc.y + g_rcDoc.h) <> yDocked)
            g_state.bOutputFloating = false : LayoutAll( surf )

            '' Find in Project takes the document rect exactly, and the editor goes away.
            g_state.bFipActive = true : LayoutAll( surf )
            Check "Find in Project occupies the document rect", _
                  (g_fip->bounds.x = g_rcDoc.x) andalso (g_fip->bounds.y = g_rcDoc.y) andalso _
                  (g_fip->bounds.w = g_rcDoc.w) andalso (g_fip->bounds.h = g_rcDoc.h)
            Check "  and the editor is not shown", (g_view->bVisible = false)
            Check "  nor its scrollbars", (g_vscroll->bVisible = false)
            g_state.bFipActive = false : LayoutAll( surf )
        end scope

        '' Put the surface back for the checks that follow.
        surf.fScale = 1.0
        surf.Resize( SH_W, SH_H )
        if surf.pRoot then surf.pRoot->PropagateScaleChanged( 1.0 )
        LayoutAll( surf )

        '' ---- THE EDITOR SEAM ----------------------------------------------------------
        '' PsThemeApply walks the WIDGET tree and Scintilla's colours are behind SCI_*
        '' messages, so the editor is the one thing a themed shell does not theme. The
        '' symptom is a WHITE PANE in the middle of an otherwise perfect window, and no
        '' assertion about widgets can see it.
        Check "the editor was created", (g_view <> 0) andalso (g_view->pSci <> 0)

        '' EXACT, AND ON A COLOUR WHOSE CHANNELS ALL DIFFER. arctic.theme's backgroundalt is
        '' 59,66,82 -- as Scintilla BGR that is &h52423B and as RGB it would be &h3B4252.
        '' Asserting against a grey, a white or a black would pass with the channels
        '' swapped and leave every real theme reversed.
        Check "the editor background is the role, in Scintilla's BGR", _
              (g_view->Msg(SCI_STYLEGETBACK, STYLE_DEFAULT) = &h52423B), _
              hex(g_view->Msg(SCI_STYLEGETBACK, STYLE_DEFAULT))

        '' AND THE MARGIN, which STYLECLEARALL does not reach. An editor themed to the last
        '' glyph still shows a white strip down its left edge without this -- minieditor has
        '' that strip today, and it took composing an editor with chrome to notice, because
        '' filling the window makes it read as a border.
        Check "  and the MARGIN is themed too", _
              (g_view->Msg(SCI_STYLEGETBACK, STYLE_LINENUMBER) <> &hFFFFFF), _
              hex(g_view->Msg(SCI_STYLEGETBACK, STYLE_LINENUMBER))
        Check "    from the background role, not the editor's", _
              (g_view->Msg(SCI_STYLEGETBACK, STYLE_LINENUMBER) = _
               ToBgr(PsThemeRoleColor(PSTHEME_BACKGROUND)))

        '' ---- THE SCROLLBAR RESERVE ----------------------------------------------------
        scope
            surf.fScale = 1.75
            surf.Resize( 1400, 900 )
            g_state.nMenubarH = 52 : g_state.nStatusH = 46 : g_state.nPanelW = 413
            g_state.nTopTabsMenuW = 186 : g_state.nTabsH = 63 : g_state.nOutputH = 194
            g_state.nTabCount = 3 : g_state.bHScrollVisible = false
            LayoutAll( surf )

            '' THE RULE, NOT THE ABSOLUTE NUMBER. tiko puts the unsplit editor at 954x500 in
            '' this state and the shell puts it at 953x499 -- one pixel in each direction,
            '' inherited from the splitter-grab rounding that shifts the document rect
            '' itself (difference class 1 in docs/port/layout-oracle/README.md). Asserting
            '' 954 here would be asserting the oracle's ROUNDING, not the layout, and the
            '' checked-in dumps are where absolute numbers get compared.
            Check "the editor is the document rect less BOTH scrollbars", _
                  (g_view->bounds.w = g_rcDoc.w - g_state.nVScrollW) andalso _
                  (g_view->bounds.h = g_rcDoc.h - PsScaleBy(SH_SCROLLBAR_HEIGHT, 1.75)), _
                  str(g_view->bounds.w) & "x" & str(g_view->bounds.h) & _
                  " from " & str(g_rcDoc.w) & "x" & str(g_rcDoc.h)
            Check "  the vertical bar takes the width", _
                  (g_rcDoc.w - g_view->bounds.w = g_state.nVScrollW)
            Check "  and the horizontal one takes the height", _
                  (g_rcDoc.h - g_view->bounds.h = PsScaleBy(SH_SCROLLBAR_HEIGHT, 1.75))

            '' THE RESERVE DOES NOT DEPEND ON VISIBILITY. This is the assertion that would
            '' catch someone "fixing" the gap the coverage check names: the editor must be
            '' the same size with the bar shown and hidden, or the vertical bar jumps every
            '' time the document gets wider than the pane.
            dim as long hHidden = g_view->bounds.h
            g_state.bHScrollVisible = true
            LayoutAll( surf )
            Check "  and showing the H bar does NOT resize the editor", _
                  (g_view->bounds.h = hHidden), str(hHidden) & " -> " & str(g_view->bounds.h)
            g_state.bHScrollVisible = false
            LayoutAll( surf )

            '' The vertical bar spans the editor PLUS the reserved strip, so the two meet in
            '' the corner instead of leaving a notch.
            Check "the vertical bar reaches the corner", _
                  (g_vscroll->bounds.h = g_view->bounds.h + PsScaleBy(SH_SCROLLBAR_HEIGHT, 1.75)), _
                  str(g_vscroll->bounds.h)
            Check "  and sits against the editor's right edge", _
                  (g_vscroll->bounds.x = g_view->bounds.x + g_view->bounds.w)

            '' ---- THE SPLIT MODES ------------------------------------------------------
            '' At most one bar is shown, both panes clear it, and the two scrollbar columns
            '' line up. Driven at the oracle's inputs, still at 1.75.
            scope
                g_state.nSplitMode = SH_SPLIT_LEFTRIGHT
                g_state.nSplitX = 0
                LayoutAll( surf )
                g_state.nSplitX = g_view->bounds.x + (g_view->bounds.w \ 2)
                LayoutAll( surf )

                Check "left/right shows the vertical bar only", _
                      (g_splitV->bVisible = true) andalso (g_splitH->bVisible = false)
                Check "  both panes are visible", _
                      (g_view->bVisible = true) andalso (g_view2->bVisible = true)
                '' THE SPLIT PANE CLEARS THE BAR. It used to start exactly at SplitX with
                '' the bar painted into the scrollbar gap -- which worked only while the bar
                '' was a painted rect and not a window (frmMain.inc:749-752).
                Check "  the main pane starts AFTER the bar", _
                      (g_view->bounds.x = g_splitV->bounds.x + g_splitV->bounds.w), _
                      str(g_view->bounds.x)
                Check "  the split pane's scrollbar sits before the bar", _
                      (g_vscroll2->bounds.x + g_vscroll2->bounds.w = g_splitV->bounds.x)
                Check "  and the split pane before that", _
                      (g_view2->bounds.x + g_view2->bounds.w = g_vscroll2->bounds.x)
                Check "  the bar spans the document rect", _
                      (g_splitV->bounds.y = g_rcDoc.y) andalso _
                      (g_splitV->bounds.h = g_rcDoc.h), str(g_splitV->bounds.h)
                '' Both panes keep the H reserve, so the two are the same height.
                Check "  both panes are the same height", _
                      (g_view->bounds.h = g_view2->bounds.h), _
                      str(g_view->bounds.h) & " vs " & str(g_view2->bounds.h)

                g_state.nSplitMode = SH_SPLIT_TOPBOTTOM
                g_state.nSplitY = 0
                LayoutAll( surf )
                '' A BARE HEIGHT/2, not the midpoint -- see the note in RunLayoutDump.
                g_state.nSplitY = g_view->bounds.h \ 2
                LayoutAll( surf )

                Check "top/bottom shows the horizontal bar only", _
                      (g_splitH->bVisible = true) andalso (g_splitV->bVisible = false)
                Check "  the bottom pane starts AFTER the bar", _
                      (g_view->bounds.y = g_splitH->bounds.y + g_splitH->bounds.h), _
                      str(g_view->bounds.y)
                Check "  the top pane's H strip is above the bar", _
                      (g_hscroll2->bounds.y + g_hscroll2->bounds.h = g_splitH->bounds.y)

                '' THE ASYMMETRY, AND IT IS tiko's. The TOP pane's vertical scrollbar spans
                '' the pane ALONE; the bottom one spans its pane PLUS the reserved H strip
                '' (frmMain.inc:685 against :783). The oracle records it as 95 against 395.
                Check "  the TOP scrollbar does not span the H reserve", _
                      (g_vscroll2->bounds.h = g_view2->bounds.h), _
                      str(g_vscroll2->bounds.h) & " vs pane " & str(g_view2->bounds.h)
                Check "  but the BOTTOM one does", _
                      (g_vscroll->bounds.h = g_view->bounds.h + PsScaleBy(SH_SCROLLBAR_HEIGHT, 1.75)), _
                      str(g_vscroll->bounds.h) & " vs pane " & str(g_view->bounds.h)

                '' AND THE SPLIT IS NOT CENTRED, which is also tiko's: SplitY is a bare
                '' height/2 used as an absolute Y, so it only centres when the document rect
                '' starts at 0 -- and the menubar and tab bar are always above it.
                Check "  so the top pane is the SMALLER one", _
                      (g_view2->bounds.h < g_view->bounds.h), _
                      str(g_view2->bounds.h) & " vs " & str(g_view->bounds.h)

                '' Back to unsplit, and the second pane goes away entirely.
                g_state.nSplitMode = SH_SPLIT_NONE
                LayoutAll( surf )
                Check "unsplit hides both bars", _
                      (g_splitV->bVisible = false) andalso (g_splitH->bVisible = false)
                Check "  and the split pane", (g_view2->bVisible = false)
                Check "  leaving the editor the whole document rect less its bars", _
                      (g_view->bounds.w = g_rcDoc.w - g_state.nVScrollW)
            end scope

            surf.fScale = 1.0
            surf.Resize( SH_W, SH_H )
            if surf.pRoot then surf.pRoot->PropagateScaleChanged( 1.0 )
            LayoutAll( surf )
        end scope

        '' ---- THE VIEW COMMANDS ACTUALLY MOVE THE LAYOUT --------------------------------
        '' The author clicked View > Move Explorer Window Right and nothing happened, which
        '' was correct and useless: the command reached OnMenuCommand, printed, and stopped,
        '' because every handler behind these ids lives in tiko.exe. The ones that map onto
        '' ShellLayoutState need no model at all, so they are wired -- and asserted here
        '' through the SAME entry point a click uses, not through the state record.
        scope
            dim as boolean bWas = g_state.bExplorerRight
            OnMenuCommand( 0, IDM_EXPLORERPOSITION, 0 )
            Check "the panel-position command mirrors the panel", _
                  (g_state.bExplorerRight <> bWas)
            Check "  and the layout followed it", _
                  (g_panel->bounds.x + g_panel->bounds.w = surf.w), _
                  str(g_panel->bounds.x) & ".." & str(g_panel->bounds.x + g_panel->bounds.w)
            OnMenuCommand( 0, IDM_EXPLORERPOSITION, 0 )
            Check "  and toggles back", (g_state.bExplorerRight = bWas)

            OnMenuCommand( 0, IDM_VIEWSIDEPANEL, 0 )
            Check "the side-panel command hides the panel", (g_panel->bVisible = false)
            OnMenuCommand( 0, IDM_VIEWSIDEPANEL, 0 )
            Check "  and shows it again", (g_panel->bVisible = true)

            OnMenuCommand( 0, IDM_VIEWOUTPUT, 0 )
            Check "the output command hides the output", (g_output->bVisible = false)
            OnMenuCommand( 0, IDM_VIEWOUTPUT, 0 )
            Check "  and shows it again", (g_output->bVisible = true)

            '' The split commands TOGGLE OFF when re-selected, as tiko's do, and set their
            '' own position from the unsplit pane.
            OnMenuCommand( 0, IDM_SPLITLEFTRIGHT, 0 )
            Check "the split command splits", (g_splitV->bVisible = true)
            Check "  with both panes real", _
                  (g_view->bounds.w > 0) andalso (g_view2->bounds.w > 0), _
                  str(g_view->bounds.w) & " / " & str(g_view2->bounds.w)
            OnMenuCommand( 0, IDM_SPLITLEFTRIGHT, 0 )
            Check "  and re-selecting it closes the split", (g_splitV->bVisible = false)

            '' AN ID WITH NO HANDLER MUST NOT TOUCH THE LAYOUT. Most ids are in that state
            '' and will be for the whole of 7c; a fall-through that re-laid out anyway would
            '' hide a missing handler behind a working-looking repaint.
            dim as long yWas = g_rcDoc.y
            OnMenuCommand( 0, IDM_FILESAVE, 0 )
            Check "an unhandled command changes nothing", (g_rcDoc.y = yWas)
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
                        '' THE ORDER IS THE WHOLE DESIGN, and each step is here for a
                        '' different reason:
                        ''
                        ''   1. THE MENU HOST FIRST. An open menu owns the keyboard while
                        ''      it is up -- its arrows and Escape are navigation, not
                        ''      shortcuts -- so nothing below sees an event it claims.
                        ''   2. ALT+MNEMONIC NEXT, because Alt+F is not an accelerator and
                        ''      no table entry would match it.
                        ''   3. ACCELERATORS BEFORE Dispatch. A shortcut has to beat the
                        ''      focused control; put this after Dispatch and the editor
                        ''      types the character instead of running the command. That
                        ''      one line is what every host in this toolkit was writing by
                        ''      hand as an `if ev.key.key = ...` before PsAccel existed.
                        ''   4. Dispatch last -- the focused widget gets what is left.
                        if g_menus.RouteEvent( @ev ) = false then
                            if TryAltMnemonic( @ev ) = false then
                                dim as long nCmd = g_accel.Find( @ev )
                                if nCmd <> 0 then
                                    OnMenuCommand( 0, nCmd, 0 )
                                else
                                    surf.Dispatch( @ev )
                                end if
                            end if
                        end if
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
