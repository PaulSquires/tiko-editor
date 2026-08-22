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
'' The first modal. PsModalHost.Run had NO CALLER anywhere in either tree before this --
'' PsModalRoute.bi records it as "exercised exactly once, interactively, by one message box
'' in one demo", and that demo no longer calls it; gallery2 includes the header and stops
'' there. So this binary is its first and only caller, and everything Run does that
'' tests/psmodalhost cannot reach is being executed here for the first time.
#include once "ui/core/PsModalHost.inc"
#include once "ui/controls/PsMenuBar.inc"
#include once "ui/controls/PsStatusBar.inc"
#include once "ui/controls/PsTabBar.inc"
'' The side panel. Its absence was NOT a compile error where you would expect one -- with
'' the type unknown, `dim shared as PsListTree ptr` parsed and every USE of the variable
'' reported "Expected End-of-Line", seven errors deep into the file and none of them at the
'' declaration. Worth knowing before hunting the wrong line.
#include once "ui/controls/PsListTree.inc"
'' The side panel's icon strip -- the pane switcher -- new in 7c step 19b/20.
''
'' STEP 19 SAID PsPlatform HAD NO PsIconPanel AND THAT WAS FALSE. It is covered by
'' tests/pslists, demoed in demos/gallery, and its own header names tiko as the source of
'' its model. The claim was made from PsListTree's callback list without opening the
'' controls directory -- the handoff's blocker table has three new rows about it.
#include once "ui/controls/PsIconPanel.inc"
'' The worker thread, new in 7c step 7. The scan takes 1.2 seconds on a large include graph
'' and used to take it on this thread; PsThread is what moves it, and g_plat.events.Post --
'' thread-safe, and written for exactly this -- is what brings the result back.
#include once "ui/core/PsThread.inc"
'' For the input box: a prompt, a field and two buttons. There is no label control in
'' PsPlatform, so the prompt is painted by the dialog root itself.
#include once "ui/controls/PsButton.inc"
#include once "ui/controls/PsTextBox.inc"
'' modScintilla.bi BEFORE PsPlatform's scintilla headers, and THE ORDER IS LOAD-BEARING --
'' tiko.bas carries the same note for the same reason. tiko #Defines all 117 SCI_* constants;
'' PsPlatform declares them as `const` behind #ifndef guards, and guards only work in this
'' direction. With PsScintilla.bi first the const already exists and every #Define becomes a
'' duplicate, which no guard on the library side can prevent. Reversing these two lines
'' produces a screen of "Duplicated definition, SCI_ADDTEXT".
#include once "app/modScintilla.bi"
#include once "scintilla/PsTextEngineC.inc"
#include once "scintilla/PsSciView.inc"
#include once "scintilla/PsSciClipboard.inc"
'' The editor's notifications, new in 7c step 5. SCNotification becomes PSEV_NOTIFY through
'' a trampoline; the shell listens for text edits and restarts the parse debounce. Until
'' this commit nothing in this binary heard from Scintilla at all.
#include once "scintilla/PsSciNotify.inc"

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
#include once "app/modSciText.bi"
#include once "app/modEncoding.bi"
#include once "app/modDocEncodingIds.bi"
#include once "app/clsDocument.bi"
#include once "app/modAppHost.bi"
#include once "app/modAppHost.inc"
#include once "app/clsApp.bi"
#include once "app/clsConfig.bi"
'' The constructor. app/clsConfig.bi carries `dim shared gConfig`, so including the header
'' instantiates the object and this TU has to link a constructor for it. It used to live in
'' src/clsConfig.inc -- the shell -- which is what made the app layer unlinkable on its own,
'' and is the one defect this commit found. See app/clsConfig.inc for the whole story.
#include once "app/clsConfig.inc"
#include once "app/modProjectFolders.bi"
#include once "app/modFindReplace.bi"
#include once "app/clsSymbolDb.bi"
#include once "app/modUnusedSymbols.bi"
#include once "app/modIniParse.bi"
#include once "app/modEncodingSelfTest.bi"
#include once "app/modEncodingSelfTest.inc"
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
#include once "app/modEncoding.inc"
#include once "app/modSciText.inc"
#include once "app/clsDocument.inc"
#include once "app/clsApp.inc"
'' THE SYMBOL DATABASE'S BODY, new in 7c step 5. Its HEADER has been included since step 4
'' -- clsSymbolDb.bi carries PARSERESULTSET, which nothing was building until the shell
'' gained a scanner. Adding the .inc is what makes InstallSet and BuildIndexes link, and it
'' is app-layer code: one of the sixteen files _check_app_standalone already compiles
'' against PsCore alone.
#include once "app/clsSymbolDb.inc"
'' ProcessFromCurdriveApp, which clsSymbolDb.inc calls at three sites to find the compiler's
'' inc directory. Its body moved down from src/modPaths.inc in this commit -- pure PsCore,
'' and on the standalone gate's own list of things that move when something needs them.
#include once "app/modPaths.inc"

'' THE FOLDER TABLE'S BODY, since 7c step 19 -- the Explorer pane walks it. Only the HEADER
'' was here before, which was enough while nothing called anything in it: a header that
'' declares and a body that is never compiled link cleanly right up to the first caller, and
'' then report undefined references to functions whose source plainly exists. Step 16 spent
'' two rounds on exactly that shape.
#include once "app/modProjectFolders.inc"
'' The find/replace ENGINE, in app/ since 7c step 26. The bar in this file writes gFind and
'' calls into it; it does not search.
#include once "app/modFindReplace.inc"


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

'' ---------------------------------------------------------------------------------------
'' THREE ACCELERATOR TABLES, IN tiko's PRECEDENCE ORDER.
''
'' frmMain.inc walks pWindow->AccelHandle, then ghAccelUserTools, then
'' ghAccelBuildConfigurations, as three nested `orelse` conditions. PsAccelTable is a TYPE,
'' so the nesting disappears: three instances and a loop, and the precedence IS the order.
''
'' THE SECOND AND THIRD ARE EMPTY IN THIS BINARY, and that is worth stating rather than
'' discovering. They are built from gConfig.Tools and gConfig.Builds, and the shell does not
'' load settings.ini -- so the code below is exercised only by the self-test, which fills
'' them synthetically to assert the precedence rule. The census found both tables cold in two
'' driven sessions of the real editor as well (docs/port/pump-census.md), so an empty table
'' here costs no coverage that the editor was providing.
dim shared as PsAccelTable g_accel        '' tiko's pWindow->AccelHandle, from gKeys
dim shared as PsAccelTable g_accelTools   '' tiko's ghAccelUserTools, from gConfig.Tools
dim shared as PsAccelTable g_accelBuilds  '' tiko's ghAccelBuildConfigurations, from gConfig.Builds
dim shared as long g_nAccelSkipped

'' Set by the Exit command once the user has confirmed. The pump owns `bRunning` as a local,
'' and a command handler cannot reach it -- so the answer is parked here and read at the top
'' of the next iteration rather than the pump variable being promoted to a global for one
'' caller.
dim shared as boolean g_bQuitRequested

'' Files named on the command line, each opened as a tab once the views exist.
dim shared as DWSTRING g_sOpenPaths(0 to 31)
dim shared as long g_nOpenPaths

'' What the Command Line dialog edits. tiko keeps this in gApp.ProjectCommandLine, but clsApp
'' is still in src/ rather than app/, so the shell cannot reach it -- see the IDM_COMMANDLINE
'' handler, which says so at the point it matters.
dim shared as DWSTRING g_sCommandLine

'' The menubar titles' first letters, kept HERE because PsMenuBar has no GetCaption and
'' should not grow one for this: the host wrote the captions and is the natural owner of a
'' mnemonic policy it is faking anyway. See TryAltMnemonic.
dim shared as string g_barInitial(0 to 15)

'' The font path, shared because ApplyScale reopens the engine and both the windowed path
'' and the self-test go through it.
dim shared as string g_sFont
'' FORWARD: the DPI-change path reopens the text engine and must rebuild the symbol
'' fallback chain with it, and that runs well before this is defined.
declare sub ShellFonts_AddSymbolFallbacks()
declare function ShellFonts_OpenEngine( byval px as long ) as long
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

'' FALSE UNTIL EVERY CHILD EXISTS. Read by ShellHost_RelayoutMain, which must drop a
'' relayout that arrives while BuildTree is still constructing -- see the end of BuildTree
'' for the access violation that paid for this.
dim shared as boolean g_bTreeReady

'' TRUE while a document is being FILLED. shelltabs.bi sets it around AssignTextBuffer and
'' shellscan.bi reads it as the debounce's loading guard -- and it lives here because those
'' two files are included in that order, so neither can own a declaration the other needs
'' first. tiko's equivalent is gApp.IsFileLoading, which is driven by tiko's own open path.
dim shared as boolean g_bScanSuppressed

'' EVERY OTHER CHILD IS A STUB, and each is named as the ORACLE names it -- so a dump and a
'' self-test failure can be read against each other without a translation table.
''
'' g_tabs IS A REAL PsTabBar AGAIN, as its stub comment promised: "the real control returns
'' when the tab MODEL does". The model is shelltabs.bi and this is that commit.
''
'' THE LAYOUT STILL OWNS THE BAND'S HEIGHT. That was the reason for the stub -- a control
'' that decides its own height would not match the oracle, which pins TOPTABS at the height
'' tiko MEASURED. Shell_LayoutAll drives it through SetBounds from g_state.nTabsH exactly as
'' it drove the stub, so the control never gets to choose.
dim shared as PsTabBar ptr g_tabs
dim shared as ShellStub ptr g_topTabsMenu

'' ---- g_panel IS A REAL PsListTree NOW, and the stub's own comment argued against this
'' until this commit: "Deliberately NOT an empty real control: an empty PsListTree looks
'' perfectly correct in the wrong band, whereas a rect that paints OUTPUT 0,486,1100,160
'' tells you where it actually is."
''
'' That was right while the band was unproven. IT IS PROVEN NOW -- the panel's rectangle is
'' pinned by docs/port/layout-oracle/ at four states and by six assertions in the band walk,
'' so the diagnostic the stub existed to provide has been spent. The band cannot move
'' without the dump failing, whatever is painted inside it.
''
'' THE LAYOUT STILL OWNS THE RECT, exactly as it does for g_tabs: Shell_LayoutAll drives it
'' through SetBounds from g_state.nPanelW, so the control never gets to choose its own size.
dim shared as PsListTree ptr g_panel

'' ---- THE PANE SWITCHER, 7c step 20 -----------------------------------------------------
'' tiko's frmPanelMenu: a strip of glyph buttons across the top of the side panel. Its two
'' PsIconPanels are a LEFT group (the three panes plus Options) and a RIGHT group of build
'' commands; this binary carries the left group's three pane buttons, because the right
'' group's ids -- Debug, Compile, Build & Execute, Find in Project, Save All -- have no
'' handlers here.
''
'' TOGGLE ITEMS, WHERE tiko USES COMMAND ITEMS, and this is the one place the port is
'' SIMPLER than the original. tiko highlights the active pane inside its own painter, by
'' checking each item's id against the current pane (frmPanelMenu.inc:74-78). PsIconPanel
'' carries selection ON THE ITEM and has SelectExclusive, so the built-in painter shows the
'' active pane with no host painting at all.
dim shared as PsIconPanel ptr g_panelMenu

dim shared as ShellStub ptr g_splitPanel
dim shared as ShellStub ptr g_barInfo
'' g_barFind IS DECLARED WITH ITS TYPE, further down, and not here with its siblings -- the
'' ShellFindBar type needs gFind and the app headers, so it cannot be defined this early and
'' `dim shared as <unknown> ptr` is not an error in fbc, it is a variable of no type whose
'' every USE then fails somewhere else entirely.
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

    '' ---- MARGIN 1 IS THE SYMBOL MARGIN, AND ITS WIDTH IS NOT COSMETIC ------------------
    ''
    '' REPORTED BY THE AUTHOR: bookmarked lines came out with a highlighted BACKGROUND
    '' across the whole line. tiko puts a small icon in the margin instead.
    ''
    '' Neither is a colour bug. IT IS SCINTILLA'S DOCUMENTED FALLBACK: a marker that no
    '' margin displays is drawn by changing the background colour of its line. This margin
    '' had width 0 and the marker had never been defined, so every bookmark painted as a
    '' stripe -- and it would have done the same for breakpoints and the debugger's current
    '' line the moment anything set one.
    ''
    '' MARGIN 1 IS SC_MARGIN_TEXT HERE BECAUSE IT IS IN tiko (modViewStyle.inc:155-157). A
    '' margin's default mask is every non-folder marker, which is exactly the set that
    '' belongs in it, so no SETMARGINMASKN is needed -- and margin 2 keeps SC_MASK_FOLDERS
    '' for the fold symbols, which is why folding is not affected by any of this.
    pV->Msg( SCI_SETMARGINTYPEN, 1, SC_MARGIN_TEXT )
    pV->Msg( SCI_SETMARGINSENSITIVEN, 1, 1 )
    pV->Msg( SCI_SETMARGINWIDTHN, 1, PsScaleBy(16, surf.fScale) )
    pV->Msg( SCI_SETMARGINWIDTHN, 2, 0 )

    '' ---- THE MARKERS THEMSELVES, WHICH NOTHING IN THIS BINARY HAD EVER DEFINED ---------
    '' Same shapes tiko uses (modViewStyle.inc:118-140). The COLOURS come from PsTheme
    '' roles rather than tiko's theme.editor.bookmark fields, because the .theme files this
    '' shell loads carry no bookmark keys at all -- arctic.theme is one of the eight that
    '' name no widget keys, so a lookup would fall back to a role regardless. Asking for the
    '' role directly says so instead of pretending there is a key.
    pV->Msg( SCI_MARKERDEFINE,  MARKER_BOOKMARK, SC_MARK_VERTICALBOOKMARK )
    pV->Msg( SCI_MARKERSETFORE, MARKER_BOOKMARK, ToBgr(PsThemeRoleColor(PSTHEME_ACCENTFORE)) )
    pV->Msg( SCI_MARKERSETBACK, MARKER_BOOKMARK, ToBgr(PsThemeRoleColor(PSTHEME_ACCENT)) )

    pV->Msg( SCI_MARKERDEFINE,  MARKER_BREAKPOINT, SC_MARK_CIRCLE )
    pV->Msg( SCI_MARKERSETFORE, MARKER_BREAKPOINT, ToBgr(PsThemeRoleColor(PSTHEME_ERROR)) )
    pV->Msg( SCI_MARKERSETBACK, MARKER_BREAKPOINT, ToBgr(PsThemeRoleColor(PSTHEME_ERROR)) )
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

'' =======================================================================================
'' THE FIND BAR -- 7c step 27, and the first thing in this binary that SEARCHES.
''
'' A port of frmFind.inc's shape rather than its code: tiko's is 662 lines, most of them a
'' hand-rolled painter, a tooltip backend and three PsIconPanels laid out against a rect the
'' tab control owns. What carries across is the ARRANGEMENT -- field, toggles, navigation,
'' count -- and every piece of it is a control this port has already proven.
''
'' ---- WHAT THIS BAR DOES NOT DO, AND WHY IT IS NOT A GAP -------------------------------
''
'' It does not search. app/modFindReplace.inc does, since 7c step 26, and this bar only ever
'' writes gFind and calls it. That split is the whole reason the engine moved down first: a
'' bar that carried the search would have to be ported again for every host.
''
'' ---- THE TOGGLES CARRY NO MENU ID, AND THAT IS NOT AN OVERSIGHT ------------------------
''
'' Prev, Next and Close resolve to IDM_FINDPREV / IDM_FINDNEXT / IDM_FIND and go through
'' OnMenuCommand, so a button and its View-menu entry cannot come apart -- the rule step 20
'' established for the pane switcher. Match Case and Whole Word have NO menu id in tiko
'' either: they are state on gFind, read by the engine, and inventing ids for them would
'' mean inventing .lang entries for a menu nothing shows them in.
'' =======================================================================================

'' Shell-local item ids for the two toggles. NEGATIVE, deliberately: PsIconPanel hands the
'' id straight back and IDM_* are positive, so a toggle can never be mistaken for a command
'' by a handler that forgets to check which panel it came from.
const SHFIND_ID_MATCHCASE = -1
const SHFIND_ID_WHOLEWORD = -2
'' SELECTION -- 7c step 29. Same strip, same rule about the sign.
const SHFIND_ID_SELECTION = -3

'' The results text's band, unscaled. Wide enough for "999/999" at the bar's font,
'' which is the widest thing it has to hold before the count stops being useful.
const SHFIND_RESULTS_UNITS = 64
const SHFIND_PAD_UNITS  = 6
const SHFIND_ICON_UNITS = 24


type ShellFindBar extends PsWidget
    pField  as PsTextBox ptr
    pToggle as PsIconPanel ptr
    pNav    as PsIconPanel ptr
    declare constructor()
    declare virtual sub OnLayout()
    declare virtual sub OnPaint(byval p as PsBufferPaint_ ptr)
end type

'' THE FIND BAND, and it is a real bar since 7c step 27 rather than a stub -- the first
'' thing in this binary that searches. Its rect is still the LAYOUT's, exactly as g_panel's
'' and g_tabs' are: a control that chose its own height could not be checked against the
'' oracle.
dim shared as ShellFindBar ptr g_barFind

'' FORWARD, because the Selection arm of ShellFind_OnIcon calls it and its body lives with
'' the Replace bar -- which is where the reason for it was found, one step later.
declare sub ShellFind_SyncToggles()
'' FORWARD for the same reason: the Selection arm captures before it decides, and the
'' body lives beside ShellFind_SetVisible where it needs LayoutAll's neighbours.
declare sub ShellFind_CaptureSelection()

declare sub ShellFind_OnFieldChange(byval pTb as any ptr, byval ud as any ptr)
declare sub ShellFind_OnFieldEnter(byval pTb as any ptr, byval ud as any ptr)
declare sub ShellFind_OnIcon(byval pPanel as any ptr, byval nIndex as long, byval ud as any ptr)


constructor ShellFindBar()
    base()
    '' THE BAR ITSELF TAKES NO FOCUS. Its FIELD does, and a Tab that stopped on the
    '' container first would be a Tab order tiko does not have.
    this.bFocusable = false

    this.pField = new PsTextBox
    if this.pField <> 0 then
        this.AddChild( this.pField )
        this.pField->OnChange( @ShellFind_OnFieldChange, 0 )
        '' ENTER IS FIND NEXT, which is what every editor does and what tiko's pump does
        '' through handleKeysFindReplace. Taking it here means the bar never needs a key
        '' filter of its own.
        this.pField->OnEnterPressed( @ShellFind_OnFieldEnter, 0 )
    end if

    '' TOGGLES, NOT COMMANDS -- the one place step 20 found the toolkit simpler than tiko.
    '' tiko paints its own latch because its items are commands; PsIconPanel carries
    '' selection ON THE ITEM, so "Match Case is on" is the control's state and the built-in
    '' painter shows it.
    this.pToggle = new PsIconPanel
    if this.pToggle <> 0 then
        this.AddChild( this.pToggle )
        '' HORIZONTAL. PsIconPanel DEFAULTS TO A COLUMN -- bVertical = true, the
        '' activity-bar case it was built for -- and every strip in this shell is a
        '' ROW. Not setting it drew the items straight down and out through the
        '' bottom of the band, which is what the first screenshot of this window
        '' actually showed; the tofu was only the more obvious half.
        this.pToggle->bVertical = false
        scope
            dim as DWSTRING g
            '' ---- THESE ARE tiko'S OWN, AND THEY ARE LITERAL TEXT ------------------
            '' Step 27 used U+E8B1 and U+E8B2 here and labelled them "tiko
            '' wszIconMatchCase / wszIconWholeWord". THAT ATTRIBUTION WAS INVENTED:
            '' modDeclares.bi:313 says wszIconMatchCase = "Aa" and :314 says
            '' wszIconWholeWord = "W" -- plain strings, which is why tiko gives the field
            '' panel GUIFONT_9 and the nav panel SYMBOLFONT_9. Step 28 took the trouble to
            '' copy "AB" for Preserve Case and this pair was guessed two steps earlier.
            ''
            '' Found by a SCREENSHOT of tiko's bar reading "Aa  W" while this file claimed
            '' those were codepoints -- not by any gate.
            this.pToggle->AddItem( DWSTRING("Aa"), SHFIND_ID_MATCHCASE, 0, PSICON_TOGGLE )
            this.pToggle->AddItem( DWSTRING("W"),  SHFIND_ID_WHOLEWORD, 0, PSICON_TOGGLE )
            g.Utf8 = chr(&hEE, &h85, &h8C)   '' U+E14C  selection    (tiko wszIconSelection)
            this.pToggle->AddItem( g, SHFIND_ID_SELECTION, 0, PSICON_TOGGLE )
        end scope
        this.pToggle->OnClick( @ShellFind_OnIcon, 0 )
    end if

    this.pNav = new PsIconPanel
    if this.pNav <> 0 then
        this.AddChild( this.pNav )
        '' HORIZONTAL. PsIconPanel DEFAULTS TO A COLUMN -- bVertical = true, the
        '' activity-bar case it was built for -- and every strip in this shell is a
        '' ROW. Not setting it drew the items straight down and out through the
        '' bottom of the band, which is what the first screenshot of this window
        '' actually showed; the tofu was only the more obvious half.
        this.pNav->bVertical = false
        scope
            dim as DWSTRING g
            '' ---- tiko'S OWN, CHECKED AGAINST modDeclares.bi THIS TIME -------------
            '' Step 27 used E015, E013 and E011 and called them "chevron up / chevron down /
            '' close". Two were the WRONG GLYPH and the third does not exist in tiko's table:
            '' E015 is ChevronDOWN, E011 is nothing, and tiko's find bar draws
            '' ChevronLeft/ChevronRight for previous/next with E10A for close. All three sat
            '' in this file as tofu, so nothing on screen contradicted the comment either.
            g.Utf8 = chr(&hEE, &h80, &h92)   '' U+E012  ChevronLeft   (tiko, FINDTIP_UPARROW)
            this.pNav->AddItem( g, IDM_FINDPREV, 0, PSICON_COMMAND )
            g.Utf8 = chr(&hEE, &h80, &h93)   '' U+E013  ChevronRight  (tiko, FINDTIP_DOWNARROW)
            this.pNav->AddItem( g, IDM_FINDNEXT, 0, PSICON_COMMAND )
            g.Utf8 = chr(&hEE, &h84, &h8A)   '' U+E10A  light X       (tiko wszIconClose)
            this.pNav->AddItem( g, IDM_FIND, 0, PSICON_COMMAND )
        end scope
        this.pNav->OnClick( @ShellFind_OnIcon, 0 )
    end if
end constructor


'' ---------------------------------------------------------------------------------------
'' Field, toggles, [results], navigation -- left to right, with the two icon strips sized to
'' their contents and the field taking what is left.
''
'' THE RESULTS TEXT IS NOT A CONTROL. It is painted by the bar, exactly as tiko paints it
'' into gFind.rcResults, because it is one string that changes on every keystroke and a
'' widget for it would be a widget to invalidate.
'' ---------------------------------------------------------------------------------------
sub ShellFindBar.OnLayout()
    dim as single f = 1.0
    if this.pSurface <> 0 then f = this.pSurface->fScale
    dim as long pad  = PsScaleBy( SHFIND_PAD_UNITS, f )
    dim as long icon = PsScaleBy( SHFIND_ICON_UNITS, f )
    dim as long h    = this.bounds.h
    dim as long y    = 0

    '' ---- THE CELL SIZE IS PUSHED IN, AND THE WIDTH IS ASKED FOR --------------------
    '' These two lines used to be `icon * GetCount()`, and the control's own cell is
    '' nCellSize = 36 against SHFIND_ICON_UNITS = 24 -- so every strip was laid out THREE
    '' CELLS WIDE INSIDE A TWO-AND-A-BIT-CELL RECT and the last icon hung outside its own
    '' panel. Same shape as the Replace field's rect: a number derived twice instead of
    '' asked for once.
    dim as long nNavW = 0, nTogW = 0
    if this.pNav <> 0 then
        this.pNav->nCellSize = SHFIND_ICON_UNITS
        nNavW = this.pNav->ContentExtent()
    end if
    if this.pToggle <> 0 then
        this.pToggle->nCellSize = SHFIND_ICON_UNITS
        nTogW = this.pToggle->ContentExtent()
    end if

    '' Navigation hard right; the results text sits to ITS left and is measured in OnPaint.
    if this.pNav <> 0 then
        this.pNav->SetBounds( PsRc(this.bounds.w - pad - nNavW, y, nNavW, h) )
    end if
    '' Toggles immediately after the field.
    dim as long xTog = this.bounds.w - pad - nNavW - PsScaleBy(SHFIND_RESULTS_UNITS, f) - nTogW
    if this.pToggle <> 0 then
        this.pToggle->SetBounds( PsRc(xTog, y, nTogW, h) )
    end if
    if this.pField <> 0 then
        dim as long w = xTog - pad - pad
        if w < 0 then w = 0
        this.pField->SetBounds( PsRc(pad, y + pad \ 2, w, h - pad) )
    end if
end sub


sub ShellFindBar.OnPaint(byval p as PsBufferPaint_ ptr)
    if p = 0 then exit sub
    dim as PsRect rcAll
    rcAll.x = 0 : rcAll.y = 0 : rcAll.w = this.bounds.w : rcAll.h = this.bounds.h
    p->SetBackColor( PsThemeRoleColor(PSTHEME_BACKGROUNDRAISED) )
    p->PaintRect( @rcAll )

    '' The "3/17". Between the toggles and the navigation, which is where tiko puts it and
    '' why its own close icon sits in a panel of its own.
    if this.pNav = 0 then exit sub
    dim as single f = 1.0
    if this.pSurface <> 0 then f = this.pSurface->fScale
    dim as PsRect rcR
    rcR.w = PsScaleBy( SHFIND_RESULTS_UNITS, f )
    rcR.x = this.pNav->bounds.x - this.bounds.x - rcR.w
    rcR.y = 0
    rcR.h = this.bounds.h
    if rcR.x < 0 then exit sub
    p->SetForeColor( PsThemeRoleColor(PSTHEME_FOREGROUNDDIM) )
    p->PaintText( gFind.wszResults, @rcR, PSTF_CENTER or PSTF_VCENTER )
end sub


'' ---------------------------------------------------------------------------------------
'' The occurrence colour, which the engine takes as a parameter since 7c step 26.
''
'' "editor.occurrence" IS tiko's OWN THEME KEY -- app/modThemeKeys.bi:93 -- so a tiko .theme
'' file drives the highlight here with nothing re-authored, which is the same property
'' PsTheme was built for and the reason step 26 made this a parameter rather than moving the
'' whole theme tree down.
private function ShellFind_OccurrenceColour() as ulong
    return PsThemeColorK( "editor.occurrence", PSTHEME_SELECTION )
end function


'' Typing re-runs the search. tiko's frmFind_TextBoxChange, minus the error-state repaint
'' the bar has no red for yet.
''
'' gFind.txtFind IS WRITTEN HERE AND NOWHERE ELSE IN THIS BINARY. Step 26 spent two defects
'' on that rule: the engine reads state, not controls, so the control has to write it.
sub ShellFind_OnFieldChange(byval pTb as any ptr, byval ud as any ptr)
    dim as PsTextBox ptr tb = cast(PsTextBox ptr, pTb)
    if tb = 0 then exit sub
    gFind.txtFind = tb->GetText()
    FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
end sub


'' Enter in the field is Find Next, through the SAME id the menu and the icon use.
sub ShellFind_OnFieldEnter(byval pTb as any ptr, byval ud as any ptr)
    OnMenuCommand( 0, IDM_FINDNEXT, 0 )
end sub


'' ---------------------------------------------------------------------------------------
'' One handler for both strips, because the id says which it was.
''
'' A NEGATIVE ID IS A TOGGLE and a positive one is a command -- see the constants. That is
'' what lets one callback serve two panels without asking which panel called it, and it is
'' why the toggle ids were made negative rather than 1 and 2.
'' ---------------------------------------------------------------------------------------
sub ShellFind_OnIcon(byval pPanel as any ptr, byval nIndex as long, byval ud as any ptr)
    dim as PsIconPanel ptr pnl = cast(PsIconPanel ptr, pPanel)
    if pnl = 0 then exit sub
    if pnl->IsValidItem( nIndex ) = false then exit sub
    dim as long id = pnl->items(nIndex).id

    select case id
        '' ---- SELECTION, 7c step 29 -------------------------------------------------
        '' THE ONLY TOGGLE ON EITHER BAR THAT CAN REFUSE TO LATCH. "Search within the
        '' selection" needs a selection to be within: tiko turns it on only when the
        '' ORIGINAL selection spanned lines, or when a marker highlight is already down,
        '' and forces it off otherwise. The control has latched itself by the time this
        '' runs, so the refusal has to be pushed back -- which is what SyncToggles is for
        '' and the reason it now knows about three ids rather than two.
        case SHFIND_ID_SELECTION
            scope
                dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
                dim as boolean bWasOn = cbool( gFind.nSelection <> 0 )
                if pDoc <> 0 then
                    '' ---- RE-CAPTURE FROM A LIVE MULTI-LINE SELECTION ----------------
                    '' THE BAR IS ALREADY OPEN WHEN THIS RUNS, and the capture happens at
                    '' SHOW time -- so selecting lines AFTER opening Find was invisible to
                    '' the rule below and the icon simply refused. tiko does not have the
                    '' hole because it re-captures on EVERY editor focus loss, which is
                    '' the mechanism this port replaced with a single show-time read.
                    ''
                    '' ONLY WHEN THE LIVE ONE SPANS LINES. After a search the live
                    '' selection is the last match, one line, and re-capturing that would
                    '' throw away the range the user actually chose.
                    ShellFind_CaptureSelection()

                    dim as boolean bMulti = _
                        cbool(pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
                    if pDoc->CurrentSelection.isInitialized = false then bMulti = false
                    '' ---- true, NOT 1 -----------------------------------------------
                    '' THE ENGINE TESTS `gFind.nSelection = true`, and in FreeBASIC that
                    '' is -1. This wrote 1, so `1 = -1` was false and the restricted-range
                    '' branch NEVER RAN: the icon lit and the search still covered the
                    '' whole document. Every assertion in step 29 tested `<> 0`, which is
                    '' the shape of assertion that cannot see this -- and the author found
                    '' it in one gesture.
                    if bMulti orelse pDoc->HasMarkerHighlight then
                        gFind.nSelection = iif( gFind.nSelection <> 0, false, true )
                    else
                        gFind.nSelection = false
                    end if
                    if gFind.nSelection then
                        '' ---- THE RESTORE, AND IT IS NOT OPTIONAL ------------------
                        '' TWO DIFFERENT SELECTIONS ARE IN PLAY HERE and the assertion
                        '' below found it: the RULE above reads CurrentSelection, the
                        '' captured one, but SetMarkerHighlight reads the LIVE selection
                        '' -- and by the time anyone clicks this icon the incremental
                        '' search has moved the live one onto the last match. Arming
                        '' Selection would mark that match's line instead of the range
                        '' the user chose, or, when the match is one line, mark nothing
                        '' at all and leave the flag set against no markers.
                        ''
                        '' THIS IS TIKO'S SCEN_SETFOCUS RESTORE, applied at the one
                        '' moment it is needed rather than on every focus change. The
                        '' header above says the restore is not ported; that is still
                        '' true of the general case, and this is the exception it names.
                        dim as any ptr pS = pDoc->GetActiveScintillaPtr()
                        if pS <> 0 then
                            SciMsg( pS, SCI_SETSELECTIONSTART, pDoc->CurrentSelection.startpos, 0 )
                            SciMsg( pS, SCI_SETSELECTIONEND,   pDoc->CurrentSelection.endpos, 0 )
                        end if
                        pDoc->SetMarkerHighlight()
                    else
                        pDoc->RemoveMarkerHighlight()
                        '' TURNING IT OFF DROPS THE TEXT SELECTION TOO, by request against
                        '' tiko. ONLY ON THE ON -> OFF TRANSITION: a REFUSAL lands in this
                        '' same branch, and collapsing a selection the user just made
                        '' because the button declined to arm is the opposite of the ask.
                        if bWasOn then
                            dim as any ptr pE = pDoc->GetActiveScintillaPtr()
                            if pE <> 0 then
                                SciMsg( pE, SCI_SETEMPTYSELECTION, _
                                        SciMsg( pE, SCI_GETCURRENTPOS, 0, 0 ), 0 )
                            end if
                        end if
                    end if
                    FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                end if
                ShellFind_SyncToggles()
            end scope

        case SHFIND_ID_MATCHCASE, SHFIND_ID_WHOLEWORD
            '' THE CONTROL HAS ALREADY LATCHED ITSELF -- a TOGGLE item flips on the click
            '' before the callback runs -- so the flag is READ FROM THE ITEM rather than
            '' flipped here. Two sources for one piece of state is what step 26 was about.
            dim as boolean bOn = pnl->GetSelected( nIndex )
            if id = SHFIND_ID_MATCHCASE then
                gFind.nMatchCase = iif( bOn, 1, 0 )
            else
                gFind.nWholeWord = iif( bOn, 1, 0 )
            end if
            '' Re-search with the new flags. tiko does the same and it is what makes the
            '' count change the instant a toggle is clicked -- the defect step 26's
            '' interactive pass opened with.
            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )

        case else
            '' Prev, Next and Close all carry a real menu id.
            OnMenuCommand( 0, id, 0 )
    end select
end sub

'' =======================================================================================
'' THE REPLACE BAR -- 7c step 28.
''
'' frmReplace.inc is 444 lines and is the second half of a pair; nearly all of the
'' difference from what follows is the same hand-rolled painter and tooltip backend the
'' Find bar's port left behind. What carries across is the ARRANGEMENT -- field, Preserve
'' Case INSIDE the field's frame, Replace and Replace All outside it -- plus the two rules
'' that only exist because the two bars are a pair.
''
'' ---- REPLACE IMPLIES FIND, AND IT IS NOT A CONVENIENCE --------------------------------
''
'' tiko's FindReplace_SetVisible opens with `if bReplace then bFind = true`, and the reason
'' is that the replace ENGINE reads gFind.txtFind. A Replace bar without a Find bar is a box
'' that replaces the empty string. Enforced in BOTH directions here: showing Replace shows
'' Find, and hiding Find hides Replace.
''
'' ---- Ctrl+H FOCUSES THE **FIND** FIELD ------------------------------------------------
''
'' Not the Replace field, which is the surprising half, and it is tiko's behaviour verbatim:
'' OnCommand_SearchReplaceDialog ends by focusing IDC_FRMFIND_TXTFIND. It follows from the
'' rule above -- with no search term there is nothing to replace, so the term is what has to
'' be typed first.
''
'' ---- AND PRESERVE CASE IS STEP 27'S QUESTION, ANSWERED THE OTHER WAY -------------------
''
'' tiko makes it IP_KIND_COMMAND and paints the latch from gFind.nPreserve, and frmReplace_
'' IconPaint says why: "the control never keeps a second copy of state FindReplace_DoReplace
'' already reads. Asking the model at paint time cannot go stale."
''
'' THAT IS A REAL DEFECT IN THE FIND BAR AS STEP 27 SHIPPED IT. PsIconPanel carries selection
'' ON THE ITEM, so a PSICON_TOGGLE is exactly the second copy tiko refused -- and tiko HAS a
'' site that breaks it: frmFindInProject.inc:3140 clears nMatchCase and nWholeWord behind the
'' bar's back. Not live in this binary yet, so it would have arrived silently WITH Find in
'' Project, one step after anybody could remember why the latch was a copy.
''
'' Answered by keeping the toggle and adding ShellFind_SyncToggles / ShellReplace_SyncToggle:
'' a push from the model into the items at every show. The model is still the one truth; the
'' copy is refreshed from it at the one moment it can be looked at.
'' =======================================================================================

'' Negative, for the reason the Find bar's two are: PsIconPanel hands the id straight back,
'' IDM_* are positive, and one handler serves both of this bar's strips.
const SHREPL_ID_PRESERVE = -1


type ShellReplaceBar extends PsWidget
    pField    as PsTextBox ptr
    pPreserve as PsIconPanel ptr
    pActions  as PsIconPanel ptr
    declare constructor()
    declare virtual sub OnLayout()
    declare virtual sub OnPaint(byval p as PsBufferPaint_ ptr)
end type

'' THE REPLACE BAND, real since 7c step 28. Declared here and not with its ShellStub
'' siblings for the same reason g_barFind is not: its type needs gFind and the app headers,
'' so it cannot exist that early in the file.
dim shared as ShellReplaceBar ptr g_barReplace

declare sub ShellReplace_OnFieldChange(byval pTb as any ptr, byval ud as any ptr)
declare sub ShellReplace_OnFieldEnter(byval pTb as any ptr, byval ud as any ptr)
declare sub ShellReplace_OnIcon(byval pPanel as any ptr, byval nIndex as long, byval ud as any ptr)


constructor ShellReplaceBar()
    base()
    this.bFocusable = false

    this.pField = new PsTextBox
    if this.pField <> 0 then
        this.AddChild( this.pField )
        this.pField->OnChange( @ShellReplace_OnFieldChange, 0 )
        '' ENTER IS REPLACE, which is what tiko's own tooltip promises -- "Replace (Enter)"
        '' -- and what handleKeysFindReplace does with it in the pump.
        this.pField->OnEnterPressed( @ShellReplace_OnFieldEnter, 0 )
    end if

    '' PRESERVE CASE. "AB" is tiko's own glyph for it (wszIconPreserveCase) and it is LITERAL
    '' TEXT, not a symbol codepoint -- which is why tiko needs two font handles here, one per
    '' panel. This needs none: PsTextEngine's fallback chain resolves A and B in the primary
    '' UI face and the PUA glyphs below in the symbol face, per character.
    this.pPreserve = new PsIconPanel
    if this.pPreserve <> 0 then
        this.AddChild( this.pPreserve )
        '' HORIZONTAL. PsIconPanel DEFAULTS TO A COLUMN -- bVertical = true, the
        '' activity-bar case it was built for -- and every strip in this shell is a
        '' ROW. Not setting it drew the items straight down and out through the
        '' bottom of the band, which is what the first screenshot of this window
        '' actually showed; the tofu was only the more obvious half.
        this.pPreserve->bVertical = false
        this.pPreserve->AddItem( DWSTRING("AB"), SHREPL_ID_PRESERVE, 0, PSICON_TOGGLE )
        this.pPreserve->OnClick( @ShellReplace_OnIcon, 0 )
    end if

    this.pActions = new PsIconPanel
    if this.pActions <> 0 then
        this.AddChild( this.pActions )
        '' HORIZONTAL. PsIconPanel DEFAULTS TO A COLUMN -- bVertical = true, the
        '' activity-bar case it was built for -- and every strip in this shell is a
        '' ROW. Not setting it drew the items straight down and out through the
        '' bottom of the band, which is what the first screenshot of this window
        '' actually showed; the tofu was only the more obvious half.
        this.pActions->bVertical = false
        scope
            dim as DWSTRING g
            g.Utf8 = chr(&hEE, &h85, &h8B)   '' U+E14B  replace      (tiko wszIconReplace)
            this.pActions->AddItem( g, IDM_REPLACENEXT, 0, PSICON_COMMAND )
            g.Utf8 = chr(&hEE, &h8A, &h99)   '' U+E299  replace all  (tiko wszIconReplaceAll)
            this.pActions->AddItem( g, IDM_REPLACEALL, 0, PSICON_COMMAND )
        end scope
        this.pActions->OnClick( @ShellReplace_OnIcon, 0 )
    end if
end constructor


'' ---------------------------------------------------------------------------------------
'' THE FIELD'S RECT IS THE FIND FIELD'S RECT, asked for rather than re-derived.
''
'' tiko does the same in two lines: rcReplaceTextRect.left and .right are ASSIGNED from
'' rcFindTextRect. Recomputing the same arithmetic here would make "the two fields line up" a
'' coincidence that holds until one of the two bars gains an icon; borrowing the rect makes
'' it structural. The fallback is this bar's own margins, for a layout that runs before the
'' Find bar has one.
'' ---------------------------------------------------------------------------------------
sub ShellReplaceBar.OnLayout()
    dim as single f = 1.0
    if this.pSurface <> 0 then f = this.pSurface->fScale
    dim as long pad  = PsScaleBy( SHFIND_PAD_UNITS, f )
    dim as long icon = PsScaleBy( SHFIND_ICON_UNITS, f )
    dim as long h    = this.bounds.h

    dim as long xField = pad
    dim as long wField = this.bounds.w - pad - pad
    if wField < 0 then wField = 0
    if (g_barFind <> 0) andalso (g_barFind->pField <> 0) then
        if g_barFind->pField->bounds.w > 0 then
            '' A CHILD'S bounds ARE ITS PARENT'S COORDINATES, not the surface's, so
            '' this is taken as-is. The first draft subtracted g_barFind->bounds.x to
            '' "convert" it and produced -413 against 6 -- caught by the assertion that
            '' the two left edges match, which is the only reason the sign was noticed.
            ''
            '' IT IS ONLY VALID BECAUSE THE TWO BANDS SHARE A LEFT EDGE AND A WIDTH:
            '' Shell_LayoutAll gives both PsRc(nLeft, ..., W - nPanelReserve, ...). If a
            '' band ever gains its own inset, this borrows the wrong origin -- which the
            '' shared-left-edge assertion would then say out loud.
            xField = g_barFind->pField->bounds.x
            wField = g_barFind->pField->bounds.w
        end if
    end if

    '' Preserve Case sits INSIDE the field's frame, so it comes OUT OF the field's width
    '' rather than being placed after it -- tiko's "make the width of the textbox smaller
    '' because we want to visually fit the Preserve Case icon into the Replace text rect".
    dim as long nPres = 0
    if this.pPreserve <> 0 then
        this.pPreserve->nCellSize = SHFIND_ICON_UNITS
        nPres = this.pPreserve->ContentExtent()
    end if
    dim as long wBox = wField - nPres
    if wBox < 0 then wBox = 0

    if this.pField <> 0 then
        this.pField->SetBounds( PsRc(xField, pad \ 2, wBox, h - pad) )
    end if
    if this.pPreserve <> 0 then
        this.pPreserve->SetBounds( PsRc(xField + wBox, 0, nPres, h) )
    end if
    if this.pActions <> 0 then
        this.pActions->nCellSize = SHFIND_ICON_UNITS
        dim as long nAct = this.pActions->ContentExtent()
        this.pActions->SetBounds( PsRc(xField + wField + pad, 0, nAct, h) )
    end if
end sub


'' No results text on this bar. The count belongs to the SEARCH, and the Find bar carrying it
'' is always open when this one is -- which is the implication above paying for itself.
sub ShellReplaceBar.OnPaint(byval p as PsBufferPaint_ ptr)
    if p = 0 then exit sub
    dim as PsRect rcAll
    rcAll.x = 0 : rcAll.y = 0 : rcAll.w = this.bounds.w : rcAll.h = this.bounds.h
    p->SetBackColor( PsThemeRoleColor(PSTHEME_BACKGROUNDRAISED) )
    p->PaintRect( @rcAll )
end sub


'' gFind.txtReplace IS WRITTEN HERE AND NOWHERE ELSE, and step 26 is what that sentence cost:
'' DoReplace moved into app/, could no longer read the control, and read gFind.txtReplace
'' instead -- which nothing had ever written. Replace and Replace All duly replaced every
'' match with an empty string, through every gate this port has.
sub ShellReplace_OnFieldChange(byval pTb as any ptr, byval ud as any ptr)
    dim as PsTextBox ptr tb = cast(PsTextBox ptr, pTb)
    if tb = 0 then exit sub
    gFind.txtReplace = tb->GetText()
end sub


sub ShellReplace_OnFieldEnter(byval pTb as any ptr, byval ud as any ptr)
    OnMenuCommand( 0, IDM_REPLACENEXT, 0 )
end sub


sub ShellReplace_OnIcon(byval pPanel as any ptr, byval nIndex as long, byval ud as any ptr)
    dim as PsIconPanel ptr pnl = cast(PsIconPanel ptr, pPanel)
    if pnl = 0 then exit sub
    if pnl->IsValidItem( nIndex ) = false then exit sub
    dim as long id = pnl->items(nIndex).id

    if id = SHREPL_ID_PRESERVE then
        '' Read FROM THE ITEM, which the control latched before this ran -- the same rule the
        '' Find bar's toggles follow, and the reason SyncToggle exists to push the other way.
        gFind.nPreserve = iif( pnl->GetSelected( nIndex ), 1, 0 )
    else
        '' Replace and Replace All carry real menu ids, so the icon and the Search menu are
        '' one path -- step 20's rule, and the third bar to follow it.
        OnMenuCommand( 0, id, 0 )
    end if
end sub


'' ---- THE PUSH FROM THE MODEL INTO THE ITEMS -------------------------------------------
'' See this bar's header. A PSICON_TOGGLE is a second copy of state the engine already owns,
'' and the model is the copy that is true. These run at every show -- the only moment a latch
'' can be looked at -- so a flag changed behind a bar's back cannot be seen stale.
sub ShellFind_SyncToggles()
    if g_barFind = 0 then exit sub
    if g_barFind->pToggle = 0 then exit sub
    dim as long i = g_barFind->pToggle->FindItemByID( SHFIND_ID_MATCHCASE )
    if i >= 0 then g_barFind->pToggle->SetSelected( i, cbool(gFind.nMatchCase <> 0) )
    i = g_barFind->pToggle->FindItemByID( SHFIND_ID_WHOLEWORD )
    if i >= 0 then g_barFind->pToggle->SetSelected( i, cbool(gFind.nWholeWord <> 0) )
    '' SELECTION IS THE ONE THAT NEEDED THIS FIRST. Its handler can refuse the latch the
    '' control has already applied, so this is not only a stale-state guard for it -- it is
    '' the path by which the refusal reaches the icon.
    i = g_barFind->pToggle->FindItemByID( SHFIND_ID_SELECTION )
    if i >= 0 then g_barFind->pToggle->SetSelected( i, cbool(gFind.nSelection <> 0) )
end sub

sub ShellReplace_SyncToggle()
    if g_barReplace = 0 then exit sub
    if g_barReplace->pPreserve = 0 then exit sub
    dim as long i = g_barReplace->pPreserve->FindItemByID( SHREPL_ID_PRESERVE )
    if i >= 0 then g_barReplace->pPreserve->SetSelected( i, cbool(gFind.nPreserve <> 0) )
end sub




'' ---- THE PANE SWITCHER'S CLICK, 7c step 20 ------------------------------------------
'' The strip and the menu are ONE command path. The click resolves the item to the menu id
'' it carries and hands it to OnMenuCommand, so a pane button and its View-menu entry cannot
'' come apart -- and anything a future item needs (an enable rule, a state) is written once.
''
'' IT LIVES HERE, BELOW OnMenuCommand'S DECLARATION, and not up beside g_panelMenu
'' where it reads more naturally. Adding a second `declare sub OnMenuCommand` there --
'' the identical prototype, spelled identically -- is error 4, Duplicated definition,
'' reported at the line of the OLDER declaration. fbc does not merge identical
'' prototypes.
private sub ShellPanelMenu_OnClick( byval pPanel as any ptr, byval nIndex as long, _
                                    byval ud as any ptr )
    dim as PsIconPanel ptr p = cast( PsIconPanel ptr, pPanel )
    if p = 0 then exit sub
    if p->IsValidItem( nIndex ) = false then exit sub
    dim as long id = p->items(nIndex).id
    if id = 0 then exit sub
    OnMenuCommand( 0, id, 0 )
end sub
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
'' "Ctrl+Shift+F5" from the three booleans and the stored name, in PsAccelParse's own order.
'' Returns an empty string for the two cases that mean NO BINDING: a blank name, and the
'' literal "None", which tiko's pick list offers as an affordance and
'' KeyBindings_PickListKeyToValue maps to zero for the same reason.
function ComposeChord( byval bCtrl as boolean, _
                       byval bAlt as boolean, _
                       byval bShift as boolean, _
                       byval sKey as DWSTRING ) as DWSTRING
    dim as DWSTRING sName = PsTrim(sKey)
    if PsLen(sName) = 0 then return ""
    if PsUCase(sName) = "NONE" then return ""

    dim as DWSTRING sOut
    if bCtrl  then sOut &= "Ctrl+"
    if bAlt   then sOut &= "Alt+"
    if bShift then sOut &= "Shift+"
    return sOut & sName
end function

declare sub BuildAccelTable2and3()

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

    BuildAccelTable2and3()
end sub


'' ---------------------------------------------------------------------------------------
'' TABLES 2 AND 3, COMPOSED AS CHORD TEXT RATHER THAN RESOLVED TO KEY CODES.
''
'' tiko builds these with KeyBindings_PickListKeyToValue, which turns a pick-list name into a
'' Win32 VK for CreateAcceleratorTable. There is no VK here, so the modifier booleans and the
'' stored name are recomposed into the chord string PsAccelParse already reads.
''
'' THAT REUSE IS SAFE FOR A REASON ALREADY ON RECORD, not by assumption:
'' KeyBindings_PickListKeyToValue resolves against gKeyNames, and the self-test's group F
'' asserts that EVERY gKeyNames entry parses to a non-zero PsKey. The two vocabularies were
'' checked against each other in step 1; this only spends that evidence.
''
'' A name that does not parse is COUNTED, like the main table's, because the failure tiko
'' documents here is silent: frmBuildConfig.inc:501 records that an unresolved name made
'' CreateAcceleratorTable skip the entry, so the shortcut saved, displayed and reloaded
'' perfectly while never once firing.
sub BuildAccelTable2and3()
    g_accelTools.Clear_()
    g_accelBuilds.Clear_()

    for y as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
        dim as DWSTRING sChord = ComposeChord( gConfig.Tools(y).IsCtrl, _
                                               gConfig.Tools(y).IsAlt, _
                                               gConfig.Tools(y).IsShift, _
                                               gConfig.Tools(y).wszKey )
        if PsLen(sChord) = 0 then continue for
        if g_accelTools.AddText( sChord, IDM_USERTOOLSBASE + y ) = false then
            g_nAccelSkipped += 1
        end if
    next

    for y as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        dim as DWSTRING sChord = ComposeChord( gConfig.Builds(y).IsCtrl, _
                                               gConfig.Builds(y).IsAlt, _
                                               gConfig.Builds(y).IsShift, _
                                               gConfig.Builds(y).wszKey )
        if PsLen(sChord) = 0 then continue for
        if g_accelBuilds.AddText( sChord, IDM_BUILDCONFIGBASE + y ) = false then
            g_nAccelSkipped += 1
        end if
    next
end sub


'' ---------------------------------------------------------------------------------------
'' THE CONSUMED-KEY RULE, AND IT IS tiko's WM_CHAR GUARD PORTED -- as something else.
''
'' frmMain.inc:2239 drops any WM_CHAR below 32 that is not backspace, or Scintilla renders it
'' as an embedded control graphic. That guard exists because Win32 manufactures the character
'' message from the keystroke INDEPENDENTLY of who handled the key, so a chord claimed by an
'' accelerator still produces a WM_CHAR for whoever holds focus.
''
'' PsEvent.bi:17-21 says that whole bug class is deleted here, and it is -- but only for
'' claims made INSIDE the surface. PsSurface.bKeyConsumed is set by PsDispatch when a widget
'' consumes a PSEV_KEY_DOWN, and the next PSEV_TEXT_INPUT is then dropped
'' (PsDispatch.inc:266-276).
''
'' THIS PUMP CLAIMS KEYS OUTSIDE THE SURFACE. The menu host, the Alt mnemonic and the
'' accelerator tables all run BEFORE surf.Dispatch, so the surface never sees those key-downs,
'' never sets the flag, and the paired text event is delivered to the editor anyway. The rule
'' is not deleted for a host that filters ahead of dispatch -- it is inherited, and it has to
'' be honoured by hand.
''
'' A `< 32` test would be the WRONG port. SDL's text-input path commits printable text, not
'' control characters, so the literal symptom tiko guards against cannot arise here. What can
'' arise is a bound letter chord firing its command AND typing its letter.
sub NoteKeyConsumed( byref surf as PsSurface, byval ev as PsEvent ptr )
    if ev = 0 then exit sub
    if ev->kind <> PSEV_KEY_DOWN then exit sub
    surf.bKeyConsumed = true
end sub


'' ---------------------------------------------------------------------------------------
'' THE PRECEDENCE WALK. tiko's three nested `orelse` conditions, flattened.
''
'' Returns the first non-zero id, so a chord bound in more than one table fires the EARLIER
'' table's command -- which is what the nesting did and is not incidental: a user tool bound
'' to a chord the main table already owns is shadowed, not ambiguous.
function AccelFind( byval ev as PsEvent ptr ) as long
    dim as long nCmd = g_accel.Find( ev )
    if nCmd <> 0 then return nCmd
    nCmd = g_accelTools.Find( ev )
    if nCmd <> 0 then return nCmd
    return g_accelBuilds.Find( ev )
end function


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
'' The shell's own tab model -- NEW code, not a port. clsTopTabCtl is a facade over tiko's
'' HWND tab control and stays there; see the file's header.
''
'' BEFORE BuildTree, which opens the command line's files as tabs. Everything it needs --
'' the view globals, SciMsg, clsDocument -- is declared above; it touches nothing from
'' shellhost.bi, so it does not have to wait for it.
#include once "shelltabs.bi"
'' The side panel's contents -- the first tiko FORM behaviour in this binary. AFTER
'' shelltabs.bi, which it needs for ShellTabs_IndexOfDoc and the tab array: a bookmark row
'' carries the tab it belongs to, and reading a background tab's markers means pointing the
'' single view at that tab's Scintilla document.
'' ---- THE PANEL BEFORE THE SCANNER, and the order reversed in 7c step 5 commit 3.
''
'' It used to be scanner-then-panel, on the reasoning that the panel would read what the
'' scanner produced. It does -- but through gSymDb, which is neither file's. The actual
'' dependency runs the OTHER WAY: a finished scan refreshes the Functions list, so
'' shellscan.bi calls ShellPanel_Reload and reads g_panelMode.
''
'' shellpanel.bi needs nothing from shellscan.bi at all.
#include once "shellpanel.bi"
#include once "shellscan.bi"


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

    '' THE REAL TAB BAR. Its height is still the LAYOUT'S -- see the note on g_tabs.
    g_tabs = new PsTabBar
    root->AddChild( g_tabs )

    '' Every remaining child frmMain_PositionWindows places, named as the ORACLE names them
    '' so a dump and a self-test failure can be read against each other.
    g_topTabsMenu = new ShellStub( @"TOPTABSMENU", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_topTabsMenu )
    '' THE SIDE PANEL, REAL. Twisty and indent on, because what goes in it is a two-level
    '' list -- a header row per file, bookmark rows beneath -- which is the shape
    '' frmBookmarks builds with AddHeader/AddString and the shape ideshell's explorer uses.
    ''
    '' NOTHING FILLS IT IN THIS COMMIT, deliberately. An empty real control in the right
    '' band is the thing the layout oracle can still check byte-for-byte; adding content and
    '' the widget swap in one commit would leave a dump difference with two possible causes.
    g_panel = new PsListTree
    g_panel->SetTreeIndent( true )
    g_panel->ShowTwisty( true )
    root->AddChild( g_panel )

    '' ---- THE PANE SWITCHER ------------------------------------------------------------
    '' THE IDS ARE tiko's AND SO ARE THE GLYPHS. Segoe MDL2 Assets private-use codepoints,
    '' copied from modDeclares.bi:346 rather than re-chosen, so the two binaries cannot
    '' drift apart on what an Explorer icon looks like.
    ''
    '' WHETHER THEY RENDER IS NOT SETTLED HERE. PsIconPanel draws whatever DWSTRING it is
    '' given in the SURFACE's font -- there is no per-widget face, PsTextEngine sets one per
    '' surface -- so a PUA codepoint arrives only if step 12's fallback chain resolves a
    '' face that covers it, and PUA is exactly what FontLink\SystemLink is least likely to
    '' list. No assertion in this file can see a glyph. If they come out as boxes the pane
    '' switching still works and the fix is an explicit glyph face, which is its own step.
    '' ---- THE GLYPHS GO IN AS UTF-8 BYTES, NOT AS A WIDE LITERAL ---------------------
    '' `dim as DWSTRING g = !"\uE8A9"` is the shape that cost this port a
    '' STATUS_HEAP_CORRUPTION: PsCore had no constructor from a native wstring, fbc
    '' silently bound the zstring ptr overload, and the crash landed in an allocation
    '' nowhere near a string and MOVED every time a print was added to find it. PsCore has
    '' the constructor now -- but .Utf8 is what the gallery uses, it is unambiguous at the
    '' call site, and the codepoint is visible in the comment either way.
    g_panelMenu = new PsIconPanel
    '' HORIZONTAL, and the band above says so: SH_PANELMENU_H is a HEIGHT and the strip
    '' spans the panel's full WIDTH. PsIconPanel defaults to a column, so without this
    '' the three items ran down the panel and out of their own rect -- and the step 20
    '' assertion that pins the band's height could not see it, because the band was
    '' right and the CONTENT was sideways.
    g_panelMenu->bVertical = false
    scope
        dim as DWSTRING g
        g.Utf8 = chr(&hEE, &hA2, &hA9)   '' U+E8A9  Explorer   (tiko wszIconExplorer)
        g_panelMenu->AddItem( g, IDM_VIEWEXPLORER,  0, PSICON_TOGGLE )
        g.Utf8 = chr(&hEE, &hA2, &hBC)   '' U+E8BC  Functions  (tiko wszIconFunctions)
        g_panelMenu->AddItem( g, IDM_FUNCTIONLIST,  0, PSICON_TOGGLE )
        g.Utf8 = chr(&hEE, &h9C, &hA3)   '' U+E723  Bookmarks  (tiko wszIconBookmarks)
        g_panelMenu->AddItem( g, IDM_BOOKMARKSLIST, 0, PSICON_TOGGLE )
    end scope
    g_panelMenu->OnClick( @ShellPanelMenu_OnClick, 0 )
    root->AddChild( g_panelMenu )

    g_splitPanel  = new ShellStub( @"",            PSTHEME_BORDER )
    root->AddChild( g_splitPanel )
    g_barInfo     = new ShellStub( @"TOPTABSINFO", PSTHEME_BACKGROUNDRAISED )
    root->AddChild( g_barInfo )
    g_barFind     = new ShellFindBar
    root->AddChild( g_barFind )
    g_barReplace  = new ShellReplaceBar
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

    '' ---- THE DOCUMENT, IF ONE WAS NAMED ------------------------------------------------
    '' THE FIRST TIME clsDocument DRIVES ANYTHING THAT IS NOT tiko.exe. It asks the host for
    '' its views (which hands back the two above rather than creating any), binds its
    '' Scintilla pointers, and loads the file through gAppHost.LoadFileText -- PsFileReadAll
    '' here, CreateFileW in tiko.
    ''
    '' AFTER the views are in the tree, deliberately: CreateScintillaWindows asks for view 0
    '' AND view 1, so both globals have to be non-null before it runs.

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

    '' ---- THE DOCUMENT, IF ONE WAS NAMED ON THE COMMAND LINE ----------------------------
    '' THE FIRST TIME clsDocument HAS DRIVEN ANYTHING THAT IS NOT tiko.exe. It asks the host
    '' for its views -- ShellHost_CreateView hands back the two above rather than making a
    '' third -- binds their Scintilla pointers, and reads the file through
    '' gAppHost.LoadFileText, which is PsFileReadAll here and CreateFileW in tiko.
    ''
    '' AFTER BOTH VIEWS ARE IN THE TREE, deliberately: CreateScintillaWindows asks for view 0
    '' and view 1 in one loop, so a null second global would bind half a document.
    '' ---- THE SCAN WORKER STARTS BEFORE ANY FILE OPENS (7c step 7) ---------------------
    '' Opening a document requests a buffer scan, and a request with no worker to take it
    '' would sit in the slot until something else happened to signal -- which, at startup,
    '' is nothing.
    ShellScan_StartWorker()

    ShellTabs_Install()
    ShellPanel_Install()
    '' The editor's notifications. Until this call nothing in this binary heard from
    '' Scintilla at all -- no SCN_MODIFIED, no SCN_CHARADDED, nothing. It is what restarts
    '' the parse debounce as the user types.
    ShellScan_Install()

    '' ---- THE EDITOR STARTS FOCUSED ------------------------------------------------------
    '' Nothing did this, so the shell opened with focus NOWHERE and the caret never appeared
    '' until something else was clicked. PsSciView has been focusable since it was written
    '' (bFocusable = TRUE); no host had ever said which widget should start with it.
    ''
    '' The same omission PsModalHost had, one level up: a surface with no focus is a keyboard
    '' that does nothing, and it looks like a dead window rather than an error.
    ''
    '' The FIRST tab if any opened, so that a document that IS on screen is the one taking
    '' keys; the editor either way, because it is the only thing here worth typing into.
    if g_view <> 0 then surf.SetFocus( g_view )

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

    '' ---- THE TREE IS COMPLETE. Nothing may lay it out before this line ------------------
    '' A HALF-BUILT TREE IS WHAT A RELAYOUT FAULTS ON, and it cost a windowed binary that
    '' died at startup with an access violation and no output at all.
    ''
    '' Opening a document fires notifications: SetProjectFileType -> ProjectSetFileType ->
    '' gAppNotify.RelayoutTopTabs -> LayoutAll, which walks EVERY child. The open loop used
    '' to sit above, before g_vscroll2, g_hscroll2, g_splitV, g_splitH and g_status existed,
    '' so LayoutAll dereferenced four null pointers.
    ''
    '' Two changes, and both are wanted. The loop moved down here, which is where "open the
    '' command line's files" always belonged -- it is the last thing the tree needs. And
    '' ShellHost_RelayoutMain drops any relayout arriving before this flag is set, because
    '' the app layer notifies from wherever it likes and no host can be expected to survive
    '' laying out children it has not made yet.
    g_bTreeReady = true

    '' ---- THE DOCUMENTS NAMED ON THE COMMAND LINE ---------------------------------------
    '' THE FIRST TIME clsDocument HAS DRIVEN ANYTHING THAT IS NOT tiko.exe. It asks the host
    '' for its views -- ShellHost_CreateView hands back the two above rather than making a
    '' third -- binds their Scintilla pointers, and reads the file through
    '' gAppHost.LoadFileText, which is PsFileReadAll here and CreateFileW in tiko.
    for i as long = 0 to g_nOpenPaths - 1
        if ShellTabs_Open( g_sOpenPaths(i) ) >= 0 then
            print "tikoshell: loaded " & g_sOpenPaths(i).Utf8
        end if
    next

    '' ---- THE PROJECT SCAN, ONCE THE FILES ARE OPEN (7c step 6) -------------------------
    '' tiko's site is frmMain.inc:63, right after the workspace loads, and for the same
    '' reason: the scan is rooted at the MAIN document, so it cannot run until there is one.
    ''
    '' It costs a disk-rooted parse of the include graph at startup -- the number
    '' docs/port/7c-step6.md exists to report.
    ShellScan_Project()

    '' The bookmarks panel starts filled: a document reopened from the command line can
    '' already carry bookmarks in its .tiko session data, and an empty panel beside a
    '' bookmarked document would be a panel that lies.
    ShellBookmarks_Load()

    '' THE EDITOR TAKES THE FOCUS LAST, after the opens: ShellTabs_Show hands focus to the
    '' view as each tab appears, but ShellBookmarks_Load runs after it and the panel is
    '' focusable now (commit 2). Whoever ends up focused, the editor is what should be.
    if g_view <> 0 then surf.SetFocus( g_view )
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
'' The pane-switcher strip's height, UNSCALED, like every other constant here -- the layout
'' scales it through PsScaleBy at the point of use. tiko's strip is the same band
'' (frmPanel_PositionWindows) and this is its measured height.
const SH_PANELMENU_H          = 30
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

        '' ---- THE PANE SWITCHER TAKES A BAND OFF THE TOP -- 7c step 20 ----------------
        '' tiko's frmPanel puts the strip above the tree and gives the tree what is left;
        '' this is that split. The strip spans the panel's full width, so it moves with the
        '' panel when the Explorer is docked right without any second case.
        ''
        '' THE HEIGHT IS THE LAYOUT'S, not the control's -- the same rule the header on
        '' g_panel and g_tabs states: a control that chooses its own height cannot be
        '' checked against an oracle that pins the band.
        dim as long nMenuH = PsScaleBy( SH_PANELMENU_H, f )
        if nMenuH > nPanelH then nMenuH = nPanelH
        g_panelMenu->SetBounds( PsRc(nPanelX, nTop, nPanelW, nMenuH) )
        g_panel->SetBounds( PsRc(nPanelX, nTop + nMenuH, nPanelW, nPanelH - nMenuH) )
        g_splitPanel->SetBounds( PsRc(nBarPos, nTop, nGrabPanel, nPanelH) )
        g_panelMenu->bVisible = true
        g_panel->bVisible = true
        g_splitPanel->bVisible = true

        nPanelReserve = nPanelW + nGrabPanel
        nLeft = nPanelReserve
        if st.bExplorerRight then nLeft = 0
    else
        nPanelW = 0
        g_panelMenu->bVisible = false
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

'' =======================================================================================
'' THE SELECTION CAPTURE -- 7c step 29, and it is NOT ported from where tiko keeps it.
''
'' tiko fills pDoc->CurrentSelection from SCEN_KILLFOCUS: the Find bar is a separate HWND,
'' so opening it takes the focus off the editor, and the selection as it was at that instant
'' is what the bar has to reason about. It restores it on SCEN_SETFOCUS.
''
'' THE SHELL HAS NO SUCH MOMENT and building one would mean a focus hook on the view --
'' PSEV_FOCUS_LOSE exists and PsSciView does not handle it, so that would be a PsPlatform
'' change. It is also unnecessary: nothing about FOCUS clobbers the selection. What clobbers
'' it is the incremental search that runs after the bar opens. So this reads the live
'' selection AT SHOW TIME, one instruction before anything can disturb it, and the
'' isInitialized latch means a second show over an open bar does not overwrite the original.
''
'' WHAT IS NOT PORTED WITH IT: the SETFOCUS restore, as a general rule. Nothing here puts
'' the original selection back when the caret returns to the editor, so after a search the
'' live selection is the last match rather than what the user had.
''
'' AND THAT IS NOT COSMETIC -- it has one place where it BITES, which an assertion found
'' rather than this comment predicting it. TWO NOTIONS OF "the selection" are in play: the
'' Selection toggle's RULE reads CurrentSelection, while SetMarkerHighlight reads the LIVE
'' one. So the restore IS done, at that one moment, inside the Selection arm of
'' ShellFind_OnIcon. See the note there.
'' =======================================================================================
sub ShellFind_CaptureSelection()
    dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
    if pDoc = 0 then exit sub

    dim as long sl, el, sp, ep
    pDoc->GetSelectedLineRange( sl, el, sp, ep )

    '' ---- A LIVE MULTI-LINE SELECTION IS NEWER NEWS THAN ANY CAPTURE ------------------
    '' THIS SAID `if isInitialized then exit sub` UNTIL tiko GREW THE SAME FUNCTION AND
    '' THE SAME BUG. Opening the bar with nothing selected fills the capture with an EMPTY
    '' range, and selecting lines afterwards can then never get past the guard -- the
    '' Selection icon reads the empty range and refuses.
    ''
    '' ONLY WHEN THE LIVE ONE SPANS LINES, which is the whole reason a capture exists:
    '' after the incremental search the live selection is the LAST MATCH, one line, and
    '' taking that would shrink "within these three lines" to "within this match".
    ''
    '' The Selection arm used to carry its own copy of this rule. It does not now -- one
    '' rule, one place, and the arm just calls this.
    if pDoc->CurrentSelection.isInitialized andalso (el <= sl) then exit sub
    pDoc->CurrentSelection.startline    = sl
    pDoc->CurrentSelection.endline      = el
    pDoc->CurrentSelection.startpos     = sp
    pDoc->CurrentSelection.endpos       = ep
    pDoc->CurrentSelection.isInitialized = true
end sub


'' ---------------------------------------------------------------------------------------
'' Seed the field from the selection -- tiko's FindControls_Show, the half step 27 deferred.
''
'' IT WAS DEFERRED BECAUSE OF THE EXCEPTION, not because of the rule. The rule is that the
'' box is a PICTURE OF THE SELECTION, so opening Find with nothing selected CLEARS it rather
'' than re-offering the last phrase -- by request, and the reason the old bSeedField flag is
'' gone from tiko. Porting that without the exception below would have re-imported a bug
'' that was fixed in step 26, which is why the two arrive together.
''
'' THE EXCEPTION: type "afxnova", press Ctrl+H, and the field came back "AfxNova" -- the
'' incremental search had already selected the match, and a selection carries the DOCUMENT's
'' casing, not what was typed. Same word, same matches, and the user's own text replaced for
'' no gain. Skipped when the two differ ONLY in case: equal folded, unequal exactly.
''
'' BOTH COMPARISONS ARE LIKE-FOR-LIKE, and tiko's comment says why in a sentence worth
'' keeping: this code has been bitten twice by string/DWSTRING conversions that compile and
'' mean something else. The fold is DWSTRING against DWSTRING; the exact test is string
'' against string through .Utf8. Neither leaves fbc a conversion to choose.
'' ---------------------------------------------------------------------------------------
sub ShellFind_SeedFromSelection()
    dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
    if pDoc = 0 then exit sub

    dim as boolean bMulti = _
        cbool(pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
    if pDoc->CurrentSelection.isInitialized = false then bMulti = false

    dim as string sFindText = pDoc->GetSelText
    if bMulti then
        '' A MULTI-LINE SELECTION IS NOT A PHRASE, it is a RANGE. The box clears for a
        '' different reason from the empty case above, and the same gesture arms Selection.
        sFindText = ""
        '' true, NOT 1 -- see the note in ShellFind_OnIcon. The engine compares against
        '' `true`, which is -1.
        gFind.nSelection = true
        pDoc->SetMarkerHighlight()
    end if

    dim as string sTyped = gFind.txtFind.Utf8
    dim as boolean bCaseOnlyEcho = false
    if len(sFindText) then
        if (PsUCase(sFindText) = PsUCase(gFind.txtFind)) andalso _
           (sFindText <> sTyped) then bCaseOnlyEcho = true
    end if

    if bCaseOnlyEcho = false then
        '' ONE ASSIGNMENT FOR BOTH PATHS, so the box and the model cannot disagree about
        '' what is being searched for. tiko's multi-line branch used to clear the box and
        '' leave gFind.txtFind holding the previous phrase.
        gFind.txtFind = sFindText
        if g_barFind <> 0 then
            if g_barFind->pField <> 0 then g_barFind->pField->SetText( gFind.txtFind )
        end if
    end if
end sub


'' ---------------------------------------------------------------------------------------
'' Show or hide the bar, and put the caret where the user expects it.
sub ShellFind_SetVisible( byval bShow as boolean )
    '' OPENING AN ALREADY-OPEN BAR SEEDS NOTHING. Ctrl+H routes through here to satisfy
    '' "Replace implies Find", so without this test a Ctrl+F followed by a Ctrl+H would
    '' reseed the field from whatever the first search had selected -- which is the exact
    '' gesture the case-only echo was reported against.
    dim as boolean bWasShown = g_state.bShowFind

    if bShow andalso (bWasShown = false) then
        ShellFind_CaptureSelection()
        ShellFind_SeedFromSelection()
    end if

    g_state.bShowFind = bShow
    gFind.bShowFindPanel = bShow

    '' HIDING FIND HIDES REPLACE -- the other half of tiko's `if bReplace then bFind = true`,
    '' which says the pair cannot exist with only the lower bar. Written INLINE rather than as
    '' a call to ShellReplace_SetVisible, because that one calls back into this one to show
    '' Find and the pair would recurse. Two flags is the whole body it would have run anyway.
    if bShow = false then
        g_state.bShowReplace = false
        gFind.bShowReplacePanel = false

        '' ---- AND CLOSING DROPS THE HIGHLIGHTS AND THE SELECTION LATCH ------------------
        '' tiko's bClosing branch, verbatim in effect. Leaving nSelection set is how a
        '' closed-and-reopened Find bar came back with Selection lit against a marker range
        '' that no longer described anything -- and this port would show that MORE readily
        '' than tiko does, because the icon here is a latch the model has to reach.
        ''
        '' EVERY DOCUMENT, not the active one: the highlights were painted into whichever
        '' buffers were searched, and a tab switched away from keeps them otherwise.
        gFind.nSelection = false
        if gAppHost.TabCount andalso gAppHost.TabDocAt then
            for i as long = 0 to gAppHost.TabCount() - 1
                dim pD as clsDocument ptr = gAppHost.TabDocAt( i )
                if pD = 0 then continue for
                pD->RemoveMarkerHighlight()
                pD->RemoveSelectionAttributes()
                pD->CurrentSelection.isInitialized = false
            next
        end if
        ShellFind_SyncToggles()
    end if

    '' THE LATCHES ARE PUSHED FROM THE MODEL AT EVERY SHOW. See the Replace bar's header:
    '' a PSICON_TOGGLE is a second copy of a flag the engine owns, and this is the moment it
    '' can be seen.
    if bShow then
        ShellFind_SyncToggles()
        ShellReplace_SyncToggle()
    end if

    if g_pSurf <> 0 then
        LayoutAll( *g_pSurf )
        if bShow andalso (g_barFind <> 0) then
            if g_barFind->pField <> 0 then
                g_pSurf->SetFocus( g_barFind->pField )
                g_barFind->pField->SelectAll()
            end if
        elseif (bShow = false) andalso (g_view <> 0) then
            '' CLOSING RETURNS THE FOCUS TO THE EDITOR. A bar that hides while holding the
            '' focus leaves the caret nowhere -- the same defect ShellTabs_Show documents
            '' for the tab bar.
            g_pSurf->SetFocus( g_view )
        end if
        g_pSurf->InvalidateAll()
    end if
end sub


'' ---------------------------------------------------------------------------------------
'' Show or hide the Replace bar. See ShellReplaceBar's header for the two rules in here.
''
'' SHOWING IT SHOWS FIND, and the Find call comes FIRST so its layout has run before this
'' bar's OnLayout borrows the Find field's rect. Then the focus is put back on the FIND
'' field, which ShellFind_SetVisible has just done -- so the only thing left to say is what
'' happens on the way OUT.
'' ---------------------------------------------------------------------------------------
sub ShellReplace_SetVisible( byval bShow as boolean )
    if bShow then
        ShellFind_SetVisible( true )
    end if

    dim as boolean bWasShown = g_state.bShowReplace
    g_state.bShowReplace = bShow
    gFind.bShowReplacePanel = bShow
    if bShow then ShellReplace_SyncToggle()

    '' ---- OPENING CLEARS THE REPLACEMENT, AND CLEARS IT IN BOTH PLACES ------------------
    '' tiko's ReplaceControls_Show blanks the BOX on every open -- and only the box.
    '' Programmatic setters are silent, so gFind.txtReplace keeps the previous replacement
    '' while the field shows empty, and the next Replace uses a term nothing on screen names.
    '' Both are cleared here. It is a deliberate divergence, and it is the one-source-of-
    '' truth rule this pair already runs on rather than a new idea.
    if bShow andalso (bWasShown = false) then
        gFind.txtReplace = DWSTRING("")
        if g_barReplace <> 0 then
            if g_barReplace->pField <> 0 then g_barReplace->pField->SetText( DWSTRING("") )
        end if
    end if

    if g_pSurf <> 0 then
        LayoutAll( *g_pSurf )
        if bShow andalso (g_barFind <> 0) andalso (g_barFind->pField <> 0) then
            '' Ctrl+H PUTS THE CARET IN THE **FIND** FIELD, not this bar's. tiko's
            '' OnCommand_SearchReplaceDialog ends on exactly that line, and the reason is the
            '' implication above: with no term there is nothing for a replacement to replace.
            g_pSurf->SetFocus( g_barFind->pField )
            g_barFind->pField->SelectAll()
        elseif (bShow = false) andalso (g_state.bShowFind = false) andalso (g_view <> 0) then
            '' CLOSING ONLY RETURNS THE CARET WHEN THE PAIR IS GONE. Closing Replace while
            '' Find stays open must leave the focus where the user put it, not yank it into
            '' the editor -- the Find bar's own close does the yank because nothing is left.
            g_pSurf->SetFocus( g_view )
        end if
        g_pSurf->InvalidateAll()
    end if
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

'' ========================================================================================
'' ========================================================================================
'' THE FILE DIALOG, MADE SYNCHRONOUS -- IN THE SHELL, AND DELIBERATELY NOT IN PsPlatform.
''
'' PsPlatform's dialogs take a CALLBACK, and Platform.bi states the reason as a position
'' rather than an accident:
''
''     "ASYNCHRONOUS BY NATURE [...] A blocking wrapper would have to pump events internally
''      and re-enter the application's own dispatch, which is exactly the re-entrancy hazard
''      that moving off SendMessage was meant to remove."
''
'' That objection is to a LIBRARY imposing re-entrancy on every host. It is not an objection
'' to an application deciding, for its own pump, that it will block here -- which is what
'' this is. PsPlatform is not modified by this commit at all.
''
'' AND THE SEAM ALREADY DEMANDS IT. AppHostServices.AskOpenPath and AskSavePath return a
'' boolean and fill a path, because clsDocument.SaveFile and InsertFile use the answer on the
'' next line. Making those asynchronous would rewrite the document model this step has just
'' finished making portable.
''
'' ---- WHAT THIS IS, PRECISELY ----------------------------------------------------------
''
'' A THIRD NESTED PUMP in a process that already has two, and the previous two produced two
'' silent defects between them. So it is built the way PsModalRoute was: the DECISIONS come
'' out into a pure function a headless suite can drive exhaustively, and only the acting is
'' left in the loop.
''
'' It differs from PsModalHost in one way that matters: THERE IS NO WINDOW OF OURS TO
'' DISPATCH TO. The dialog belongs to the operating system. So every event is either the
'' quit, the owner's resize, or something to drop -- there is no "give it to the dialog" arm.
'' ========================================================================================
enum ShellFileDlgAction
    '' Not ours, or not interesting. The native dialog owns the user's attention.
    SHFD_DROP = 0

    '' End the wait AND re-post the quit. Identical in spirit to PSMODAL_END_REPOST_QUIT,
    '' and wrong in the same way if forgotten: the wait would end, the application would
    '' carry on, and the quit would be gone.
    SHFD_ABORT_REPOST_QUIT

    '' The shell was resized behind the dialog. Handle it -- a window that comes back the
    '' wrong size after a file dialog is a visible bug, and compositors do deliver these.
    SHFD_RESIZE_SELF
end enum

'' bMine is "this event belongs to the shell's own window", computed by the caller the same
'' way the main pump does: (ev->surface = win) orelse (ev->surface = 0).
function ShellFileDlgRoute( byval ev as PsEvent ptr, byval bMine as boolean ) as ShellFileDlgAction
    '' A null is not a crash. The loop only calls this after Wait returned TRUE, which is
    '' exactly why it is asserted -- a table that faults on a null cannot be fuzzed.
    if ev = 0 then return SHFD_DROP

    select case ev->kind
        case PSEV_QUIT
            '' bMine NOT consulted, for PsModalRoute's reason: a quit is the application
            '' going away, and whose window it was addressed to does not change that.
            return SHFD_ABORT_REPOST_QUIT

        case PSEV_CLOSE
            '' THE SHELL'S OWN CLOSE ENDS EVERYTHING. Unlike a modal dialog there is no
            '' window of ours to close instead -- the file dialog is the OS's, and closing
            '' that produces a callback rather than an event.
            if bMine then return SHFD_ABORT_REPOST_QUIT
            return SHFD_DROP

        case PSEV_RESIZE
            if bMine then return SHFD_RESIZE_SELF
            return SHFD_DROP
    end select

    return SHFD_DROP
end function


'' ---- the callback's landing area -------------------------------------------------------
'' File-scope because the callback is a plain sub pointer invoked by SDL, on SDL's stack,
'' carrying only a userdata pointer -- and Platform.bi is explicit that a non-trivial UDT
'' must not cross that boundary. Two fbc codegen bugs have already been hit doing so.
dim shared as boolean  g_bDlgPending
dim shared as boolean  g_bDlgOk
dim shared as DWSTRING g_sDlgPath

private sub ShellDlgDone( byval result as PsDialogResult, _
                          byval paths as zstring ptr ptr, _
                          byval nPaths as long, _
                          byval userdata as any ptr )
    g_bDlgOk = false
    g_sDlgPath = ""
    '' THREE OUTCOMES, NOT TWO. Platform.bi keeps PSDLG_ERROR distinct from PSDLG_CANCELLED
    '' so a broken portal cannot masquerade as a user decision. Both mean "no path" to the
    '' caller, but only one of them is the user's doing, and only one is worth printing.
    if result = PSDLG_OK then
        if (paths <> 0) andalso (nPaths > 0) then
            if paths[0] <> 0 then
                '' COPIED, NOT KEPT. SDL frees the array on return.
                g_sDlgPath.Utf8 = *paths[0]
                g_bDlgOk = true
            end if
        end if
    elseif result = PSDLG_ERROR then
        print "tikoshell: the file dialog could not be shown (no portal or backend)"
    end if
    g_bDlgPending = false
end sub


'' Blocks until the user answers. TRUE if a path came back.
'' bIsSave, NOT bSave: fbc has a Bsave statement and identifiers are case-insensitive, so a
'' parameter called bSave is a "Duplicated definition" and every use of it is then reported
'' as "No matching overloaded function, BSAVE". Exactly the family _check_app_layer.bas
'' records for bIn against Bin().
function ShellAskPath( byval bIsSave as boolean, byref sOut as DWSTRING ) as boolean
    if g_pSurf = 0 then return false
    if g_plat.dialogs.Available() = false then
        '' "An application should disable the affordance rather than open nothing" --
        '' Platform.bi. This shell has no affordance to disable, so it says so instead.
        print "tikoshell: no file dialog is available on this system"
        return false
    end if

    g_bDlgPending = true
    g_bDlgOk      = false
    g_sDlgPath    = ""

    if bIsSave then
        g_plat.dialogs.SaveFile( g_pSurf->hWin, @ShellDlgDone, 0 )
    else
        g_plat.dialogs.OpenFile( g_pSurf->hWin, @ShellDlgDone, 0, false )
    end if

    '' ---- the nested pump. Its only job is to keep the process answering until the
    '' callback fires.
    dim as PsEvent ev
    dim as boolean bAborted = false
    do while g_bDlgPending
        dim as ulongint nNow = g_plat.events.Ticks()
        PsTimerService( nNow )

        if g_plat.events.Wait( @ev, PsTimerWaitMs(nNow, 30) ) then
            dim as boolean bMine = (ev.surface = g_pSurf->hWin)
            if ev.surface = 0 then bMine = true

            select case ShellFileDlgRoute( @ev, bMine )
                case SHFD_ABORT_REPOST_QUIT
                    '' RE-POSTED, NOT CONSUMED.
                    g_plat.events.Post( PSEV_QUIT, 0, 0 )
                    bAborted = true
                    exit do

                case SHFD_RESIZE_SELF
                    g_pSurf->Dispatch( @ev )
                    LayoutAll( *g_pSurf )
                    g_pSurf->InvalidateAll()

                case else
                    '' SHFD_DROP. The OS dialog has the user; dispatching into a window
                    '' they cannot reach is what PsModalRoute warns about.
            end select
        end if

        '' NO PAINT HERE, and that is not an omission. The shell's back buffer lives in the
        '' main pump's locals, which this function cannot reach. A resize above marks damage
        '' and the main loop repaints on its very next iteration once this returns.
    loop

    if bAborted then return false
    if g_bDlgOk = false then return false
    sOut = g_sDlgPath
    return true
end function


'' THE INPUT BOX -- AND THE DIALOG KEY POLICY, WHICH IS WHAT REPLACES IsDialogMessage.
''
'' docs/port/pump-census.md put the whole residue of the pump collapse here. Fifteen
'' IsDialogMessage calls do one job that PsPlatform has no analogue for, and the census
'' measured which part of that job is real: in frmMain its keyboard share is 53 messages in
'' 7167 and is visibly ordinary typing being dispatched, while in frmOptions it is 87 in 1614
'' and the descriptors are VK_TAB, Shift+VK_TAB, the arrows and VK_RETURN. The irreplaceable
'' behaviour lives in DIALOGS.
''
'' So this is the piece the census said would be needed ONCE rather than fourteen times, and
'' building it here is the test of that claim.
''
'' WHAT PsPlatform ALREADY DOES, and must not be re-implemented: TAB. PsDispatch.inc:247-255
'' gives Tab to the focused widget FIRST and only moves focus if nothing wanted it, which is
'' the rule a text editor needs. FocusNext walks the tree in document order skipping anything
'' not focusable, enabled and visible. None of that is this file's business.
''
'' WHAT IS LEFT IS ENTER AND ESCAPE, and they are application policy in any toolkit: which
'' button is default, and what cancel means. That is the whole of the gap.
''
'' AND IT STAYS IN THE SHELL FOR NOW. It is a candidate to move into PsPlatform once a
'' second dialog wants it -- but PsMenuBar.bi:44-48 already set the precedent for not moving
'' a seam on one caller's guess, and this has exactly one caller.
'' ========================================================================================
const SH_DLG_PAD     = 20     '' design units, like every literal in this file
const SH_DLG_GAP     = 12
const SH_DLG_FIELDH  = 28
const SH_DLG_BTNW    = 88
const SH_DLG_BTNH    = 28
const SH_DLG_PROMPTH = 22
const SH_DLG_MINW    = 360

type ShellInputBox extends PsWidget
    sPrompt   as DWSTRING
    pField    as PsTextBox ptr
    pOK       as PsButton ptr
    pCancel   as PsButton ptr

    '' 0 while the box is still up. The pump's done-predicate reads this, so it is the
    '' single piece of state that ends the dialog.
    nResult   as long

    declare constructor()
    declare sub OnPaint(byval p as PsBufferPaint_ ptr)
    declare sub OnLayout()
    declare function MeasureDesired() as PsSize
    declare function OnEvent(byval ev as PsEvent ptr) as boolean
end type

'' The two buttons and the two keys all land here, so "what OK means" is written once.
sub ShellInputBox_Finish( byval pBox as ShellInputBox ptr, byval nId as long )
    if pBox = 0 then exit sub
    pBox->nResult = nId
end sub

sub ShellInputBox_OnOK( byval pBtn as any ptr, byval ud as any ptr )
    ShellInputBox_Finish( cptr(ShellInputBox ptr, ud), MBX_ID_OK )
end sub

sub ShellInputBox_OnCancel( byval pBtn as any ptr, byval ud as any ptr )
    ShellInputBox_Finish( cptr(ShellInputBox ptr, ud), MBX_ID_CANCEL )
end sub

constructor ShellInputBox()
    this.bFocusable = false      '' the container is not a stop; its children are

    this.pField = new PsTextBox
    this.pOK    = new PsButton
    this.pCancel = new PsButton

    this.pOK->SetText( L(0, "OK") )
    this.pCancel->SetText( L(1, "Cancel") )

    '' ADDED IN TAB ORDER, because traversal IS tree order (PsDispatch's FocusNext walks the
    '' tree, not a tabindex). Field, then OK, then Cancel -- so the order the dialog is read
    '' in and the order Tab visits are the same thing by construction rather than by a
    '' separate list that can drift out of step with the layout.
    this.AddChild( this.pField )
    this.AddChild( this.pOK )
    this.AddChild( this.pCancel )

    this.pOK->OnClick( @ShellInputBox_OnOK, @this )
    this.pCancel->OnClick( @ShellInputBox_OnCancel, @this )
end constructor

function ShellInputBox.MeasureDesired() as PsSize
    '' MEASURED, NOT GUESSED, and it is measured HERE rather than by the caller because
    '' PsModalHost.Run asks only after attaching this to a surface -- which is the documented
    '' trap in PsModalHost.bi: asked while detached, every string measures zero because there
    '' is no font and the scale is 1.0.
    dim as PsSize sz
    sz.w = this.ScaleX(SH_DLG_MINW)
    sz.h = this.ScaleY(SH_DLG_PAD) + this.ScaleY(SH_DLG_PROMPTH) + this.ScaleY(SH_DLG_GAP) + _
           this.ScaleY(SH_DLG_FIELDH) + this.ScaleY(SH_DLG_GAP) + this.ScaleY(SH_DLG_BTNH) + _
           this.ScaleY(SH_DLG_PAD)
    return sz
end function

sub ShellInputBox.OnLayout()
    dim as long nPad = this.ScaleX(SH_DLG_PAD)
    dim as long nGap = this.ScaleY(SH_DLG_GAP)
    dim as long nY   = this.ScaleY(SH_DLG_PAD) + this.ScaleY(SH_DLG_PROMPTH) + nGap

    dim as PsRect rc
    rc.x = nPad : rc.y = nY
    rc.w = this.bounds.w - (nPad * 2) : rc.h = this.ScaleY(SH_DLG_FIELDH)
    if this.pField then this.pField->SetBounds( rc )

    dim as long nBtnW = this.ScaleX(SH_DLG_BTNW)
    dim as long nBtnH = this.ScaleY(SH_DLG_BTNH)
    dim as long nBtnY = rc.y + rc.h + nGap

    '' Cancel rightmost, OK to its left -- the Windows order, and the order the buttons were
    '' ADDED in is the opposite. That is deliberate and is the one place tab order and visual
    '' order legitimately differ: Tab reaches OK first because OK is the likely answer.
    dim as PsRect rcC
    rcC.x = this.bounds.w - nPad - nBtnW : rcC.y = nBtnY
    rcC.w = nBtnW : rcC.h = nBtnH
    if this.pCancel then this.pCancel->SetBounds( rcC )

    dim as PsRect rcO = rcC
    rcO.x = rcC.x - this.ScaleX(SH_DLG_GAP) - nBtnW
    if this.pOK then this.pOK->SetBounds( rcO )
end sub

sub ShellInputBox.OnPaint(byval p as PsBufferPaint_ ptr)
    if p = 0 then exit sub

    dim as PsRect rcAll
    rcAll.x = 0 : rcAll.y = 0 : rcAll.w = this.bounds.w : rcAll.h = this.bounds.h
    p->SetBackColor( PsThemeRoleColor(PSTHEME_BACKGROUND) )
    p->PaintRect( @rcAll )
    p->SetPenColor( PsThemeRoleColor(PSTHEME_BORDER) )
    p->PaintBorderRect( @rcAll, 1 )

    '' The prompt. No label control exists, so the container draws it.
    dim as PsRect rcP
    rcP.x = this.ScaleX(SH_DLG_PAD)
    rcP.y = this.ScaleY(SH_DLG_PAD)
    rcP.w = this.bounds.w - (this.ScaleX(SH_DLG_PAD) * 2)
    rcP.h = this.ScaleY(SH_DLG_PROMPTH)
    p->SetForeColor( PsThemeRoleColor(PSTHEME_FOREGROUND) )
    p->PaintText( this.sPrompt, @rcP, PSTF_LEFT or PSTF_VCENTER )
end sub

'' ---------------------------------------------------------------------------------------
'' THE POLICY ITSELF, and it is four lines because Tab is not its job.
''
'' This runs as the ROOT's OnEvent, so it sees a key only after the focused widget declined
'' it -- PsDispatch bubbles from the focus outward. That ordering is what makes Enter safe:
'' a multi-line field that wants Enter takes it first, exactly as Tab works.
function ShellInputBox.OnEvent(byval ev as PsEvent ptr) as boolean
    if ev = 0 then return false
    if ev->kind <> PSEV_KEY_DOWN then return false

    select case ev->key.key
        case PSKEY_RETURN, PSKEY_KP_ENTER
            '' THE DEFAULT BUTTON. Not "the focused button" -- Enter means OK wherever focus
            '' happens to be, which is the whole point of a default.
            ShellInputBox_Finish( @this, MBX_ID_OK )
            return true
        case PSKEY_ESCAPE
            ShellInputBox_Finish( @this, MBX_ID_CANCEL )
            return true
    end select
    return false
end function


'' Returns OK or Cancel; the text comes back through sText, which is left untouched on
'' cancel -- tiko's frmInputBox_Show has the same shape (frmInputBox.bi:113).
private function ShellInputBox_IsDone(byval ud as any ptr) as boolean
    dim as ShellInputBox ptr p = cptr(ShellInputBox ptr, ud)
    if p = 0 then return true
    return (p->nResult <> 0)
end function

'' THE WRITE-BACK RULE, SPLIT OUT SO IT CAN BE ASSERTED. tiko is explicit about it
'' (frmMainCompile.inc:34-36): "Cancel returns the (possibly edited) text too, but the
'' IDCANCEL says to disregard it." Left inline in ShellInputBoxShow the rule was unreachable
'' -- reverting it to write back on Cancel as well left all 169 assertions green, checked --
'' because everything around it needs a window. Out here it is a pure function of the answer.
function ShellInputBox_Commit( byval nRes as long, _
                               byref sOut as DWSTRING, _
                               byval sField as DWSTRING ) as boolean
    if nRes <> MBX_ID_OK then return false
    sOut = sField
    return true
end function


function ShellInputBoxShow( byval sCaption as DWSTRING, _
                            byval sPrompt as DWSTRING, _
                            byref sText as DWSTRING ) as long
    if g_pSurf = 0 then return MBX_ID_CANCEL

    '' HEAP, NOT STACK, and deliberately -- unlike the message box. PsModalHost hands the
    '' root to a PsSurface, and until PsPlatform 61f56bb that surface DELETED it on teardown:
    '' the message box was a local and the process died on every dismissal. That is fixed,
    '' and this is still allocated here because the dialog owns three child widgets whose
    '' lifetime is the tree's -- so the tree is freed once, explicitly, at the end.
    dim as ShellInputBox ptr pBox = new ShellInputBox
    pBox->sPrompt = sPrompt
    if pBox->pField then pBox->pField->SetText( sText )

    dim as PsModalHost host
    dim as long nRes = MBX_ID_CANCEL
    dim as boolean bRan = host.Run( g_pSurf, pBox, sCaption, @ShellInputBox_IsDone, pBox )
    if bRan then
        nRes = pBox->nResult
        if pBox->pField <> 0 then
            ShellInputBox_Commit( nRes, sText, pBox->pField->GetText() )
        end if
    end if

    delete pBox
    return nRes
end function


'' ---------------------------------------------------------------------------------------
'' THE FIRST MODAL DIALOG IN THE PORT.
''
'' tiko's own Exit confirmation, frmMain.inc:1282-1287: with gConfig.AskExit set it asks
'' L(213) under caption L(214) with a question icon and Yes/No/Cancel, and anything but Yes
'' cancels the close. Reproduced here because a REAL command whose real behaviour is a
'' message box exercises Run() in the shape the port will actually use it.
''
'' THE BUTTONS ARE BUILT WITH AddButton, NOT AddPreset. PsMessageBox.bi:122-124 is explicit
'' that preset captions are English literals and "a localised host uses AddButton" -- and
'' tiko is a localised host. Ids 94, 95 and 1 already exist in all six .lang files, so this
'' adds no id and cannot render blank.
''
'' THE CANCEL ID IS SET EXPLICITLY even though the last button IS Cancel and the control
'' would default to it. The header calls that a convention rather than a law and warns that a
'' host whose last button is destructive must set it; relying on the default would make this
'' code wrong the day somebody reorders the buttons.
''
'' WINDOWLESS, THIS RETURNS CANCEL AND DOES NOT QUIT. PsMessageBoxShowModal returns the
'' resolved cancel id when the surface cannot be created, which --selftest relies on: it has
'' no window, so the box cannot be shown, and the safe answer is the one that keeps running.
'' COMPOSING IS SPLIT FROM SHOWING so the composition can be asserted. Everything above --
'' the localised captions, the button ids, the cancel id -- is decided here and is reachable
'' windowlessly; only PsMessageBoxShowModal needs a compositor. Without the split, the one
'' part of this that a suite CAN check would sit behind the part it cannot.
sub BuildExitBox( byref box as PsMessageBox )
    box.SetCaption( L(214, "Confirm") )
    box.SetText( L(213, "Are you sure you want to exit?") )
    box.SetIcon( MBX_ICON_QUESTION )

    box.AddButton( L(94, "Yes"),    MBX_ID_YES )
    box.AddButton( L(95, "No"),     MBX_ID_NO )
    box.AddButton( L(1,  "Cancel"), MBX_ID_CANCEL )
    box.SetDefaultButton( 0 )
    box.SetCancelId( MBX_ID_CANCEL )
end sub


function ConfirmExit() as boolean
    if g_pSurf = 0 then return false
    dim as PsMessageBox box
    BuildExitBox( box )
    return (PsMessageBoxShowModal( g_pSurf, box ) = MBX_ID_YES)
end function


'' tikoshell's half of the app-host seam -- THE SECOND IMPLEMENTATION of a record that had
'' only ever been filled by AfxNova. HERE, this late, because its bodies call StyleOneView,
'' ShellAskPath, LayoutAll and the g_view / g_pSurf / g_sFont globals, every one of which
'' is declared above this line.
#include once "shellhost.bi"


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
        '' The two commands in this binary that raise a real dialog. bHandled stays true so
        '' the relayout below runs either way: the modal painted over the shell, and the
        '' damage it left is the shell's to repair.
        case IDM_EXIT             : if ConfirmExit() then g_bQuitRequested = true

        '' tiko's frmMainCompile.inc:32-42, ported whole: seed the box with the current
        '' command line, and write it back ONLY on OK -- Cancel returns the edited text too,
        '' and the id is what says to disregard it.
        ''
        '' THE VALUE LIVES IN A SHELL GLOBAL, not gApp.ProjectCommandLine, because clsApp is
        '' still in src/ and has not moved down into app/. That is a real difference from
        '' tiko and it is here rather than hidden: the DIALOG is what this commit is proving,
        '' and the field it edits is a stand-in.
        case IDM_COMMANDLINE
            scope
                dim as DWSTRING sText = g_sCommandLine
                if ShellInputBoxShow( L(142, "Command Line"), _
                                      L(143, "Enter command line arguments:"), _
                                      sText ) = MBX_ID_OK then
                    g_sCommandLine = sText
                    print "tikoshell: command line is now [" & g_sCommandLine.Utf8 & "]"
                end if
            end scope

        '' ---- OPEN AND SAVE, THROUGH THE DOCUMENT MODEL -----------------------------------
        '' The point of these three is that NONE of the work is here. Open asks the host for
        '' a path and hands it to the tab model; Save calls clsDocument.SaveFile, which is
        '' app-layer code shared with tiko byte for byte and reaches the disk, the encoder,
        '' the lossy prompt and the write-failure report entirely through the seam.
        ''
        '' tiko's OnCommand_FileOpen (frmMainFile.inc:46) opens MULTIPLE files and this opens
        '' one: g_plat.dialogs.OpenFile is asked with bMultiple = false, because ShellAskPath
        '' returns a single path. A real difference, said here rather than left to be found.
        case IDM_FILEOPEN
            scope
                dim as DWSTRING sPick
                if ShellAskPath( false, sPick ) then ShellTabs_Open( sPick )
            end scope

        '' SAVE AS IS THE SAME CALL WITH ONE FLAG, exactly as tiko routes it
        '' (frmMainOnCommand.inc:209-221). SaveFile raises the Save As dialog itself, through
        '' gAppHost.AskSavePath -- so the shell never opens it here, and an UNTITLED document
        '' saved with plain Save still gets the picker, because SaveFile forces bSaveAs on a
        '' new document before anything else.
        '' ---- A SAVE RESCANS THE PROJECT, which is tiko's own site (frmMainFile.inc:241).
        '' The project tier reads from DISK, so until a file is saved its symbols there are
        '' the last saved ones -- a save is the moment that stops being true. Without this a
        '' background tab would keep listing procedures the file no longer has.
        ''
        '' ONLY WHEN THE BYTES REACHED DISK: ShellTabs_Save answers FALSE for a cancelled
        '' picker or a failed write, and rescanning after either would re-read a file that
        '' did not change.
        case IDM_FILESAVE
            if ShellTabs_Save( false ) then ShellScan_Project()
        case IDM_FILESAVEAS
            if ShellTabs_Save( true ) then ShellScan_Project()

        '' ---- THE BOOKMARK COMMANDS, ported from OnCommand_SearchBookmarks --------------
        '' The first tiko FORM commands this binary answers. Each one is a clsDocument call
        '' and a panel reload -- see shellpanel.bi, including what did NOT come with them
        '' (NavHistory, whose body is not linkable here) and the one deliberate divergence
        '' (Clear All uses MARKER_BOOKMARK where tiko passes -1 and takes the breakpoints
        '' with it).
        '' ---- THE PANE SWITCH ------------------------------------------------------------
        '' tiko's icon strip is not ported (there is one PsListTree here, not three panes),
        '' so these two commands switch a MODE. Both ids and both captions already exist --
        '' nothing is added to modMenuIds.bi or to any .lang file.
        case IDM_BOOKMARKSLIST : ShellPanel_SetMode( SHPANEL_BOOKMARKS )
        case IDM_FUNCTIONLIST  : ShellPanel_SetMode( SHPANEL_FUNCTIONS )
        case IDM_VIEWEXPLORER  : ShellPanel_SetMode( SHPANEL_EXPLORER )

        '' ---- FIND, 7c step 27 ----------------------------------------------------------
        '' IDM_FIND TOGGLES, which is tiko's behaviour and the reason the bar's own close
        '' icon carries this id rather than one of its own: closing and reopening are the
        '' same gesture and there is no second path to keep in step.
        case IDM_FIND
            ShellFind_SetVisible( g_state.bShowFind = false )

        case IDM_FINDNEXT, IDM_FINDNEXTACCEL, IDM_FINDPREV, IDM_FINDPREVACCEL
            '' THE HIGHLIGHT RUNS FIRST. NextSelection walks the INDICATORS, so a term that
            '' has not been highlighted yet has nothing to walk -- which is why tiko calls
            '' the two in this order at every one of its own call sites.
            scope
                dim as boolean bNext = (nId = IDM_FINDNEXT) orelse (nId = IDM_FINDNEXTACCEL)
                if g_view <> 0 then
                    FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                    dim as long nStart = SciMsg( g_view->pSci, SCI_GETCURRENTPOS, 0, 0 )
                    FindReplace_NextSelection( nStart, bNext, true )
                    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
                end if
            end scope

        '' ---- REPLACE, 7c step 28 -------------------------------------------------------
        '' Toggles, like IDM_FIND, and for the same reason.
        case IDM_REPLACE
            ShellReplace_SetVisible( g_state.bShowReplace = false )

        '' ---- THE THREE REPLACE ACTIONS -------------------------------------------------
        '' foundCount = 0 IS TIKO'S FIRST LINE in OnCommand_SearchReplaceActions, and it is
        '' kept for FIDELITY, not because its necessity could be shown. A first draft of the
        '' comment here asserted that "DoReplace with no matches operates on wherever the
        '' caret happens to be" -- INVENTED, and the revert-to-red pass said so: deleting the
        '' guard left the suite at 537/0. DoReplace already refuses an empty term, and with a
        '' term that matches nothing both of its branches find nothing to do.
        ''
        '' Left in because tiko has it and this is a port; recorded here because an
        '' unexplained guard that nothing can break is how a real one gets deleted later.
        ''
        '' THE ARGUMENT ORDER IS THE ONE STEP 26 GOT WRONG: (fReplaceAll, fMovenext, colour).
        '' Replace-previous does not move on, Replace-next does, Replace All does both.
        case IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
            scope
                if gFind.foundCount <> 0 then
                    dim as ulong clr = ShellFind_OccurrenceColour()
                    if nId = IDM_REPLACEPREV then
                        FindReplace_DoReplace( false, false, clr )
                    elseif nId = IDM_REPLACENEXT then
                        FindReplace_DoReplace( false, true, clr )
                    else
                        FindReplace_DoReplace( true, true, clr )
                    end if
                    '' RE-HIGHLIGHT AFTER, because the replacement changed the buffer under
                    '' the indicators -- tiko does the same at every one of its call sites,
                    '' and the count on the Find bar is what goes stale without it.
                    FindReplace_HighlightSearches( clr )
                    if g_pSurf <> 0 then g_pSurf->InvalidateAll()
                end if
            end scope

        '' ---- THE EXPLORER'S FOLDER COMMANDS, 7c step 21 --------------------------------
        '' Delegated rather than handled here, because they act on a REMEMBERED ROW: the
        '' popup is not modal, so this runs after it closed, and both commands rebuild the
        '' tree -- an index resolved at this point would name a different row. tiko keeps
        '' the same variable for the same reason (gExpMenuRow, modContextMenus.inc:491).
        case IDM_EXPLORER_NEWFOLDER, IDM_EXPLORER_DELETEFOLDER
            ShellExplorer_FolderCommand( nId )

        case IDM_BOOKMARKTOGGLE   : ShellBookmarks_Toggle()
        case IDM_BOOKMARKNEXT     : ShellBookmarks_Next()
        case IDM_BOOKMARKPREV     : ShellBookmarks_Prev()
        case IDM_BOOKMARKCLEARALL : ShellBookmarks_ClearCurrent()
        case IDM_BOOKMARKCLEARALLDOCS : ShellBookmarks_ClearAllDocs()

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
    DumpChild( @"PANELMENU",     g_panelMenu )
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

    '' NO PlatPs_SetDpi HERE, and the attempt is worth recording. It was added on the theory
    '' that Scintilla was hit-testing at 96 dpi while drawing at 175% -- and it DOUBLED the
    '' text size, because this shell already pre-scales everything Scintilla is told: the
    '' font size it is created with, and SCI_SETMARGINWIDTHN through PsScaleBy. Telling it
    '' the DPI as well scales the same thing twice.
    ''
    '' tiko calls PlatPs_SetDpi because ITS font sizes are in POINTS, which Scintilla then
    '' scales. Ours are already in pixels for this display. The mouse bug is elsewhere.

    dim as long px = PsScaleBy( SH_FONT_PX, f )
    if px <> g_nFontPx then
        TE_Free( g_te )
        '' TE_Free DROPS THE FALLBACKS WITH THE ENGINE, which is why this goes through
        '' ShellFonts_OpenEngine rather than TE_Init: without the chain every icon turns to
        '' tofu the first time the window crosses to a monitor at another scale -- and only
        '' then, which is the kind of thing reported as "the icons vanished" months later.
        if ShellFonts_OpenEngine( px ) = 0 then
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
'' ---------------------------------------------------------------------------------------
'' THE ICON FACE, AND WHY IT IS VENDORED RATHER THAN BORROWED.
''
'' TE_Init opens CascadiaCode.ttf and NOTHING ELSE, so until this existed every private-use
'' codepoint this shell drew came out as a TOFU BOX: the pane switcher since step 20, the
'' Explorer's action icons, and both bars' chevrons since steps 27-29. A comment in this file
'' asserted the fallback chain resolved them. The chain is real; nothing had been put in it.
''
'' POINTING AT C:/Windows/Fonts/SegoeIcons.ttf WAS HALF A FIX. "Segoe Fluent Icons" is
'' licensed for use ON Windows, not for redistribution with an application, and a subset is
'' still its outlines -- so that arrangement leaves Linux, which phase 7c has been building
'' for since step 16, drawing boxes.
''
'' PsIcons.ttf IS MICROSOFT'S OWN MIT-LICENSED FLUENT UI SYSTEM ICONS, subset to the glyphs
'' this application uses and REMAPPED ONTO TIKO'S CODEPOINTS -- see
'' PsPlatform/assets/fonts/build-psicons.py. 2,312 bytes for nine glyphs. Every call site
'' still says U+E8A9 for the Explorer icon, so tiko (Segoe, Windows-only) and this shell
'' (PsIcons, everywhere) share one vocabulary and one set of constants.
''
'' AND SEGOE IS NOT KEPT AS A SECOND FALLBACK, deliberately. It would make a codepoint nobody
'' added to build-psicons.py render on Windows and box on Linux -- the exact asymmetry that
'' hid the missing chain for eleven steps. A missing icon should be missing on both.
'' ---- ONE DOOR, BECAUSE TWO CALLS IS ONE TOO MANY --------------------------------------
'' The first version had each caller do TE_Init and then remember to add the fallback, in
'' two places. Deleting the STARTUP one left the suite green: the harness runs at 175%, where
'' the DPI path reopens the engine on the way in and rebuilds the chain anyway. AT 100%
'' NOTHING WOULD HAVE REOPENED IT and every icon would have been a box with every assertion
'' still passing -- a defect visible only on the displays the suite does not run on.
''
'' Not asserted, REMOVED: opening the engine and filling its chain are one call now.
function ShellFonts_OpenEngine( byval px as long ) as long
    if TE_Init( g_te, strptr(g_sFont), px ) = 0 then return 0
    ShellFonts_AddSymbolFallbacks()
    return 1
end function


sub ShellFonts_AddSymbolFallbacks()
    '' Beside CascadiaCode.ttf, and reached the same way: PsPlatform's assets, not a copy.
    '' PsCore's whole value is that the toolkit and the application share one implementation,
    '' and that applies to the icon face as much as to the code.
    '' PsPathDirWithSep, and DWSTRING throughout until the last step -- g_sFont is a plain
    '' string because TE_Init wants a zstring ptr, so the conversion happens once, here,
    '' rather than being smuggled through an operator that has no overload for the mix.
    dim as DWSTRING wszIcons = PsPathDirWithSep( DWSTRING(g_sFont) ) & "PsIcons.ttf"
    dim as string sIcons = wszIcons.Utf8
    if TE_AddFallback( g_te, strptr(sIcons), "PsIcons" ) = false then
        '' LOUD, because the alternative is a window full of boxes and no explanation, and
        '' this is the one failure in the font path that still produces a running program.
        print "tikoshell: no icon face -- every icon will be a box: " & sIcons
    end if
end sub


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
        '' Anything that is not a switch is a file to open. First one wins -- this binary
        '' has one document until commit 6 gives it tabs.
        '' Anything that is not a switch is a file to open, and EVERY one of them is --
        '' this binary has tabs from this commit on.
        if left(command(i), 2) <> "--" then
            if g_nOpenPaths <= ubound(g_sOpenPaths) then
                g_sOpenPaths(g_nOpenPaths) = command(i)
                g_nOpenPaths += 1
            end if
        end if
    next

    '' THE APP-HOST SEAM, filled before anything can open a document -- and CHECKED, because
    '' every field is required and a null one would surface as an empty editor rather than an
    '' error. Both records, separately: one check over two would pass a half-filled host.
    ShellHost_Install()
    if AppHost_IsComplete() = false then
        print "tikoshell: AppHost." & AppHost_FirstMissing() & " is not set (build error)"
        end 2
    end if
    if AppNotify_IsComplete() = false then
        print "tikoshell: AppNotify." & AppNotify_FirstMissing() & " is not set (build error)"
        end 2
    end if

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
    if ShellFonts_OpenEngine( SH_FONT_PX ) = 0 then
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

    '' ---- THE EXPLORER CATEGORIES, AFTER THE LANGUAGE AND NOT BEFORE -- 7c step 19 -----
    ''
    '' Six filetype ids and six captions, and the captions are L() lookups -- so this cannot
    '' live in the constructor and cannot run before the block above. tiko.bas calls it at
    '' the same point and says the same thing.
    ''
    '' WITHOUT IT ubound(gConfig.Cat) IS -1 AND THE EXPLORER RENDERS NOTHING. Every category
    '' loop runs zero times, which is not an error and not a warning: it is an empty tree,
    '' and an empty tree is exactly what a workspace with no files in it looks like.
    gConfig.SetCategoryDefaults()

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
        '' NOTHING TO REDO AFTER PsThemeApply ANY MORE. This used to re-flatten the panel's
        '' alternating-row colour, because applying a theme runs OnThemeChanged on every
        '' widget and PsListTree's re-read clrRowAlt from it. SetAltRows is a flag the control
        '' keeps, set once at install, and a theme load cannot undo it.
    end scope

    LayoutAll( surf )

    '' ---------------------------------------------------------------------- dump
    if bDumpLayout then
        RunLayoutDump( surf )
        ShellScan_StopWorker()
        TE_Free( g_te )
        PsPlatformShutdown()
        end 0
    end if

    '' ---------------------------------------------------------------------- selftest
    if bSelfTest then
        print "--- tikoshell selftest ---"

        Check "the tree is built", (surf.pRoot <> 0)
        '' TWENTY-ONE SINCE 7c STEP 20 added the pane switcher. It was twenty from the
        '' plan -- written before any of it was -- through eighteen steps, so the number
        '' moving is worth a line rather than a silent edit: the menubar and statusbar, TWO
        '' real PsSciViews, the panel and its icon strip, and fifteen stubs.
        Check "  twenty-one children", (surf.pRoot->ChildCount() = 21), _
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

        '' ---- GROUP G: THREE TABLES, AND WHAT THE PUMP OWES THE SURFACE ----------------
        '' tiko stacks three HACCELs in a fixed order (frmMain.inc:2235-2237). The census
        '' found tables 2 and 3 cold in two driven sessions, and they are EMPTY in this
        '' binary because the shell does not load settings.ini -- so the precedence rule is
        '' asserted against tables filled here, or it is not asserted at all.
        scope
            g_accelTools.Clear_()
            g_accelBuilds.Clear_()

            '' Ctrl+S is already the MAIN table's, for IDM_FILESAVE. Binding it in table 2
            '' as well is the case the nesting decides: earlier table wins, and the later
            '' binding is SHADOWED rather than ambiguous.
            Check "table 2 accepts a chord the main table already owns", _
                  g_accelTools.AddText( DWSTRING("Ctrl+S"), IDM_USERTOOLSBASE + 0 )
            '' THE CHORD IS ASSERTED FREE BEFORE IT IS USED. Ctrl+Shift+F9 was the first
            '' pick and the main table already owns it (id 1206) -- so the two assertions
            '' below failed on a correct walk. A test that hard-codes an "obviously unused"
            '' chord is a test that breaks when gKeys grows, and reports it as a defect in
            '' the walk.
            Check "  Ctrl+Alt+Shift+F12 is unbound in the main table", _
                  (g_accel.FindKey(PSKEY_F12, PSMOD_CTRL or PSMOD_ALT or PSMOD_SHIFT) = 0)
            Check "  table 3 takes one of its own", _
                  g_accelBuilds.AddText( DWSTRING("Ctrl+Alt+Shift+F12"), IDM_BUILDCONFIGBASE + 0 )

            dim as PsEvent ev
            ev.kind = PSEV_KEY_DOWN
            ev.key.key = PSKEY_S
            ev.key.modifiers = PSMOD_CTRL
            Check "  and the MAIN table still wins Ctrl+S", _
                  (AccelFind(@ev) = IDM_FILESAVE), str(AccelFind(@ev))

            '' The shadowed entry is still THERE -- the walk skipped it, it was not dropped.
            '' Asserted because "table 2 never got the event" and "table 2 has no entry" are
            '' different bugs with the same symptom.
            Check "  the shadowed table-2 entry still exists", _
                  (g_accelTools.FindKey(PSKEY_S, PSMOD_CTRL) = IDM_USERTOOLSBASE + 0)

            ev.key.key = PSKEY_F12
            ev.key.modifiers = PSMOD_CTRL or PSMOD_ALT or PSMOD_SHIFT
            Check "  a table-3 chord reaches table 3", _
                  (AccelFind(@ev) = IDM_BUILDCONFIGBASE + 0), str(AccelFind(@ev))

            '' Table 2 unshadowed, to prove the walk reaches it at all rather than merely
            '' falling through to 3.
            Check "  Ctrl+Alt+F11 is unbound in the main table too", _
                  (g_accel.FindKey(PSKEY_F11, PSMOD_CTRL or PSMOD_ALT) = 0)
            Check "  table 2 takes a chord nobody else has", _
                  g_accelTools.AddText( DWSTRING("Ctrl+Alt+F11"), IDM_USERTOOLSBASE + 1 )
            ev.key.key = PSKEY_F11
            ev.key.modifiers = PSMOD_CTRL or PSMOD_ALT
            Check "  and the walk finds it", _
                  (AccelFind(@ev) = IDM_USERTOOLSBASE + 1), str(AccelFind(@ev))

            g_accelTools.Clear_()
            g_accelBuilds.Clear_()
            ev.key.key = PSKEY_F12
            ev.key.modifiers = PSMOD_CTRL or PSMOD_ALT or PSMOD_SHIFT
            Check "  cleared, the walk finds nothing", (AccelFind(@ev) = 0), str(AccelFind(@ev))
        end scope

        '' ComposeChord, which is what makes tables 2 and 3 reuse PsAccelParse instead of
        '' resolving to a VK the way tiko must.
        scope
            Check "a chord composes in PsAccelParse's order", _
                  (ComposeChord(true, true, true, DWSTRING("F5")) = DWSTRING("Ctrl+Alt+Shift+F5"))
            Check "  no modifiers is the bare name", _
                  (ComposeChord(false, false, false, DWSTRING("F5")) = DWSTRING("F5"))
            '' Both of tiko's no-binding spellings, which are NOT the same case: an empty
            '' cell, and the literal the pick list offers.
            Check "  a blank name is no binding", _
                  (PsLen(ComposeChord(true, false, false, DWSTRING(""))) = 0)
            Check "  and None is no binding either", _
                  (PsLen(ComposeChord(true, false, false, DWSTRING("None"))) = 0)
            '' The composed text has to survive the round trip, or table 2 silently holds
            '' nothing -- the exact failure frmBuildConfig.inc:501 records.
            dim as PsAccelTable t
            Check "  and what it composes, PsAccel parses", _
                  t.AddText( ComposeChord(true, false, true, DWSTRING("F5")), 4242 )
            Check "    to the chord it named", _
                  (t.FindKey(PSKEY_F5, PSMOD_CTRL or PSMOD_SHIFT) = 4242)
        end scope

        '' THE CONSUMED-KEY RULE. This is the one that would have caught the real defect:
        '' the pump claims keys BEFORE surf.Dispatch, so PsDispatch never sets bKeyConsumed
        '' and the paired PSEV_TEXT_INPUT still reaches the editor.
        scope
            dim as PsEvent ev
            ev.kind = PSEV_KEY_DOWN
            ev.key.key = PSKEY_S
            ev.key.modifiers = PSMOD_CTRL

            surf.bKeyConsumed = false
            NoteKeyConsumed( surf, @ev )
            Check "a key claimed outside the surface is reported to it", surf.bKeyConsumed

            '' A key UP is not a claim. Reporting one would suppress the NEXT keystroke's
            '' text, which is a dropped character rather than a doubled one -- the same rule
            '' failing the other way.
            surf.bKeyConsumed = false
            ev.kind = PSEV_KEY_UP
            NoteKeyConsumed( surf, @ev )
            Check "  but a key UP is not", (surf.bKeyConsumed = false)

            surf.bKeyConsumed = false
            ev.kind = PSEV_TEXT_INPUT
            NoteKeyConsumed( surf, @ev )
            Check "  and neither is a text event", (surf.bKeyConsumed = false)
            surf.bKeyConsumed = false
        end scope

        '' ---- THE FIRST MODAL: WHAT IS REACHABLE WITHOUT A COMPOSITOR ------------------
        '' Almost nothing about a modal dialog is assertable here, and the report says so at
        '' length. What IS assertable is the box's COMPOSITION, which is why BuildExitBox is
        '' split out from ConfirmExit: the captions, the ids and the dismissal rules are
        '' decided windowlessly and only the showing needs a display.
        scope
            dim as PsMessageBox box
            BuildExitBox( box )

            Check "the exit box carries three buttons", (box.GetButtonCount() = 3), _
                  str(box.GetButtonCount())
            Check "  Yes first, because it is the default", (box.ButtonId(0) = MBX_ID_YES)
            Check "  then No", (box.ButtonId(1) = MBX_ID_NO)
            Check "  then Cancel", (box.ButtonId(2) = MBX_ID_CANCEL)
            Check "  Escape resolves to Cancel", (box.ResolveCancelId() = MBX_ID_CANCEL), _
                  str(box.ResolveCancelId())

            '' AND THAT ONE IS VACUOUS AGAINST SetCancelId, checked by removing the call:
            '' still 146/0, because Cancel IS the last button and the control's unset
            '' convention resolves to the last button anyway. Said here rather than left for
            '' a reader to discover.
            ''
            '' What the explicit SetCancelId actually defends against is REORDERING, so that
            '' is what gets asserted -- on a box built the wrong way round, where the
            '' convention alone would answer "Yes" to a question the user escaped out of.
            scope
                dim as PsMessageBox rev
                rev.AddButton( L(1,  "Cancel"), MBX_ID_CANCEL )
                rev.AddButton( L(95, "No"),     MBX_ID_NO )
                rev.AddButton( L(94, "Yes"),    MBX_ID_YES )
                Check "  unset, the convention would make Escape mean the LAST button", _
                      (rev.ResolveCancelId() = MBX_ID_YES), str(rev.ResolveCancelId())
                rev.SetCancelId( MBX_ID_CANCEL )
                Check "  set, Escape means Cancel wherever Cancel sits", _
                      (rev.ResolveCancelId() = MBX_ID_CANCEL), str(rev.ResolveCancelId())
            end scope
            Check "  and focus starts on the default", (box.ResolveFocusIndex() = 0), _
                  str(box.ResolveFocusIndex())

            '' THE BLANK-RENDER TRAP. L(id, "default") DISCARDS the default -- an id missing
            '' from a .lang file renders as an empty string with nothing to say so. A modal
            '' asking an empty question above three blank buttons is the failure mode, and it
            '' is invisible to every other assertion in this file.
            Check "  every localised string in it resolved", _
                  ((PsLen(L(213,"")) > 0) andalso (PsLen(L(214,"")) > 0) andalso _
                   (PsLen(L(94,"")) > 0) andalso (PsLen(L(95,"")) > 0) andalso _
                   (PsLen(L(1,"")) > 0))

            '' THE RESULT PATH, WHICH THE MODAL DEFECT MADE WORTH ASSERTING. The box ends
            '' the whole application whichever button dismisses it, and one candidate cause
            '' was "GetResult always answers Yes". These rule that out without a window:
            '' whatever id is dismissed with is the id that comes back.
            scope
                dim as PsMessageBox b2
                BuildExitBox( b2 )
                b2.Dismiss( MBX_ID_NO )
                Check "  dismissing with No reports No", _
                      (b2.GetResult() = MBX_ID_NO), str(b2.GetResult())
                Check "    and the box knows it is dismissed", b2.IsDismissed()

                dim as PsMessageBox b3
                BuildExitBox( b3 )
                b3.Dismiss( MBX_ID_CANCEL )
                Check "  dismissing with Cancel reports Cancel", _
                      (b3.GetResult() = MBX_ID_CANCEL), str(b3.GetResult())

                dim as PsMessageBox b4
                BuildExitBox( b4 )
                b4.Dismiss( MBX_ID_YES )
                Check "  and only Yes is Yes", _
                      (b4.GetResult() = MBX_ID_YES), str(b4.GetResult())
            end scope

            '' ---- GROUP H: THE DIALOG KEY POLICY -------------------------------------
            '' The census said the pump's whole residue was this, and that it would be
            '' needed once rather than fourteen times. Most of a modal is unreachable
            '' headlessly -- but the TRAVERSAL is not, because PsSurface.FocusNext walks the
            '' widget tree and needs no window at all. So the part that replaces
            '' IsDialogMessage is the part that CAN be asserted, which is a better split
            '' than this step had any right to expect.
            scope
                dim as PsSurface dlg
                dlg.pText = surf.pText
                dlg.fScale = surf.fScale
                dlg.Resize( 400, 200 )

                dim as ShellInputBox ptr pIB = new ShellInputBox
                pIB->sPrompt = DWSTRING("prompt")
                dlg.SetRoot( pIB )
                dim as PsRect rcAll = PsRc(0, 0, dlg.w, dlg.h)
                pIB->SetBounds( rcAll )

                '' INITIAL FOCUS. The dialog opened with focus nowhere, so the field could
                '' not be typed into until the user pressed Tab -- reported by the author.
                '' Win32's dialog manager does this on WM_INITDIALOG and PsModalHost.Run
                '' did not do it at all, so EVERY dialog through it opened cold.
                ''
                '' Fixed in PsPlatform, not here, because it is host policy. What is
                '' asserted here is the SHAPE of the rule -- with nothing focused, the
                '' first focusable widget in tree order is the field -- which is exactly
                '' what Run's `if pFocus = 0 then FocusNext()` resolves to.
                dlg.SetFocus( 0 )
                Check "with focus nowhere, the first stop is the FIELD", _
                      (dlg.FocusNext() andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pField)))
                Check "  so a dialog opened cold lands on something typable", _
                      (dlg.pFocus <> 0)

                '' TAB ORDER IS TREE ORDER, which is why the constructor adds the children
                '' in the order it does rather than keeping a separate index that could
                '' drift away from the layout.
                dlg.SetFocus( 0 )
                Check "Tab reaches the field first", _
                      (dlg.FocusNext() andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pField)))
                Check "  then OK", _
                      (dlg.FocusNext() andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pOK)))
                Check "  then Cancel", _
                      (dlg.FocusNext() andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pCancel)))
                Check "  and then it wraps rather than sticking", _
                      (dlg.FocusNext() andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pField)))

                '' Shift+Tab is the same walk backwards. Asserted because a traversal that
                '' only works forwards is a dialog you can enter and not leave.
                Check "  Shift+Tab goes back to Cancel", _
                      (dlg.FocusNext(true) andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pCancel)))
                Check "  and again to OK", _
                      (dlg.FocusNext(true) andalso (dlg.pFocus = cptr(PsWidget ptr, pIB->pOK)))

                '' THE CONTAINER IS NOT A TAB STOP. It is focusable-by-default in PsWidget,
                '' and a root that took focus would put a stop on the dialog background.
                Check "  and the container itself is never a stop", (pIB->bFocusable = false)

                '' ---- ENTER AND ESCAPE, which is the actual policy --------------------
                dim as PsEvent ev
                ev.kind = PSEV_KEY_DOWN

                pIB->nResult = 0
                ev.key.key = PSKEY_RETURN
                Check "Enter is claimed by the dialog", pIB->OnEvent(@ev)
                Check "  and means OK wherever focus sits", (pIB->nResult = MBX_ID_OK), _
                      str(pIB->nResult)

                pIB->nResult = 0
                ev.key.key = PSKEY_ESCAPE
                Check "Escape is claimed", pIB->OnEvent(@ev)
                Check "  and means Cancel", (pIB->nResult = MBX_ID_CANCEL), str(pIB->nResult)

                '' An ordinary key is NOT the dialog's. If the root claimed everything, the
                '' field could never be typed into -- and the root only sees a key the
                '' focused widget declined, so claiming broadly here is invisible until a
                '' control stops wanting something.
                pIB->nResult = 0
                ev.key.key = PSKEY_A
                Check "  but an ordinary key is left alone", (pIB->OnEvent(@ev) = false)
                Check "    and decides nothing", (pIB->nResult = 0)

                '' A key UP must not fire the default. Enter pressed in some other window
                '' and released over this one would otherwise commit the dialog.
                pIB->nResult = 0
                ev.kind = PSEV_KEY_UP
                ev.key.key = PSKEY_RETURN
                Check "  and a key UP never commits", (pIB->OnEvent(@ev) = false)
                Check "    still undecided", (pIB->nResult = 0)

                '' The done-predicate is what ends the nested pump, so nResult and "is it
                '' finished" have to agree -- a box that decided but never reported would
                '' hang the dialog with its answer already chosen.
                pIB->nResult = 0
                Check "the pump keeps running while undecided", _
                      (ShellInputBox_IsDone(pIB) = false)
                pIB->nResult = MBX_ID_CANCEL
                Check "  and stops once decided", ShellInputBox_IsDone(pIB)

                '' THE WRITE-BACK RULE. Cancel must leave the caller's variable ALONE even
                '' though the field holds edited text -- tiko says so in as many words.
                scope
                    dim as DWSTRING sVar = "original"
                    Check "Cancel does not write the edited text back", _
                          (ShellInputBox_Commit(MBX_ID_CANCEL, sVar, DWSTRING("edited")) = false)
                    Check "  and leaves the caller's value untouched", _
                          (sVar = DWSTRING("original")), sVar.Utf8

                    Check "OK does write it back", _
                          ShellInputBox_Commit(MBX_ID_OK, sVar, DWSTRING("edited"))
                    Check "  and the caller sees the edit", _
                          (sVar = DWSTRING("edited")), sVar.Utf8

                    '' An empty edit is a REAL value, not a no-op: clearing the command line
                    '' and pressing OK has to clear it.
                    Check "  and clearing the field commits the clear", _
                          ShellInputBox_Commit(MBX_ID_OK, sVar, DWSTRING(""))
                    Check "    so the value is now empty", (PsLen(sVar) = 0), sVar.Utf8
                end scope

                '' The surface owns this tree -- it built nothing and was handed it, but
                '' unlike PsModalHost this scope intends the surface to free it, so SetRoot
                '' is correct here and DetachRoot would leak.
                dlg.SetRoot( 0 )
            end scope

            '' ---- GROUP I: TWO PUMPS IN ONE PROCESS -----------------------------------
            '' Step 1 never had this interaction. While a modal is up, PsModalHost.Run owns
            '' delivery and the shell's outer pump is BLOCKED inside OnMenuCommand -- so the
            '' filter chain in that loop cannot run at all. That is structural, not a rule
            '' this file enforces, and it is the reason the menubar goes inert: not because
            '' anything disables it, but because nothing is reading its events.
            ''
            '' What can go wrong is the routing INSIDE the nested pump, and that is a pure
            '' function. tests/psmodalhost asserts it over PsPlatform's vocabulary; this
            '' asserts it over the kinds THIS shell actually produces, because a hole in the
            '' table shows up as a whole class of event going to the wrong place.
            scope
                dim as PsEvent ev

                '' The keyboard, which is what "the menubar is inert" means in practice.
                '' Addressed to the dialog it is dispatched; addressed to the shell behind
                '' it, DROPPED -- not relayed. A key that reached the shell while a modal
                '' was up would run an accelerator behind the user's back.
                ev.kind = PSEV_KEY_DOWN
                Check "with a modal up, a key for the DIALOG is dispatched", _
                      (PsModalRouteEvent(@ev, true) = PSMODAL_DISPATCH)
                Check "  and a key for the SHELL is dropped, not relayed", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_DROP)

                ev.kind = PSEV_TEXT_INPUT
                Check "  typing goes to the dialog", _
                      (PsModalRouteEvent(@ev, true) = PSMODAL_DISPATCH)
                Check "  and never behind it", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_DROP)

                '' The mouse. Alt+F and a click on the menubar are the author's two checks,
                '' and this is the half of them that is reachable here.
                ev.kind = PSEV_MOUSE_DOWN
                Check "  a click on the shell behind the modal is dropped", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_DROP)
                ev.kind = PSEV_MOUSE_MOVE
                Check "  and so is a move over it", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_DROP)

                '' A resize of the OWNER is dropped rather than dispatched. Relaying one
                '' into a surface the user cannot reach is what rendered the popup demo at
                '' triple size, per PsModalRoute.bi.
                ev.kind = PSEV_RESIZE
                Check "  the owner's resize is dropped", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_DROP)
                Check "  but the dialog's own is acted on", _
                      (PsModalRouteEvent(@ev, true) = PSMODAL_RESIZE_SELF)

                '' THE QUIT, WHICH IS THE ONE THAT LOOKS LIKE A HANG WHEN IT IS WRONG.
                '' Swallowed, the box ends and the application runs on with its main window
                '' gone. bMine is deliberately not consulted.
                ev.kind = PSEV_QUIT
                Check "  a quit ends the box AND is re-posted", _
                      (PsModalRouteEvent(@ev, true) = PSMODAL_END_REPOST_QUIT)
                Check "    whoever it was addressed to", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_END_REPOST_QUIT)

                '' The two closes diverge, and must: the dialog's own X is a cancel, the
                '' SHELL's close ends everything.
                ev.kind = PSEV_CLOSE
                Check "  the dialog's own close is a cancel", _
                      (PsModalRouteEvent(@ev, true) = PSMODAL_CLOSE_SELF)
                Check "  but closing the SHELL behind it ends everything", _
                      (PsModalRouteEvent(@ev, false) = PSMODAL_END_REPOST_QUIT)
            end scope

            '' AND WHAT A QUIT DURING A MODAL MUST NOT DO: commit the dialog's answer.
            '' Run returns TRUE in that case -- it created its surface and pumped -- so the
            '' caller cannot tell "the user answered" from "the application is going away"
            '' by the return value alone. It has to read the RESULT, and an undismissed box
            '' reports none.
            scope
                dim as PsMessageBox bq
                BuildExitBox( bq )
                Check "a box killed by a quit has no result", (bq.GetResult() = 0), _
                      str(bq.GetResult())
                Check "  so the exit is not confirmed", (bq.GetResult() <> MBX_ID_YES)
                Check "  and it never looked dismissed", (bq.IsDismissed() = false)

                '' Same rule on the input box, where the cost of getting it wrong is worse:
                '' committing text the user never approved because the app was closing.
                dim as DWSTRING sVar = "original"
                Check "and a quit commits no text either", _
                      (ShellInputBox_Commit(0, sVar, DWSTRING("edited")) = false)
                Check "  leaving the value alone", (sVar = DWSTRING("original")), sVar.Utf8
            end scope

            '' ---- GROUP L: THE TAB MODEL ------------------------------------------------
            '' NEW code rather than a port, so nothing upstream vouches for it. What IS
            '' reachable windowlessly is the bookkeeping -- which document a tab maps to, and
            '' that a switch saves and restores the view position. The Scintilla side of a
            '' switch is not: SCI_SETDOCPOINTER needs a live view.
            scope
                '' The bar is real from this commit, so it can be driven directly.
                Check "the tab bar is a real control now, not a stub", (g_tabs <> 0)
                Check "  and starts empty in a windowless run", (g_tabs->GetCount() = 0), _
                      str(g_tabs->GetCount())

                '' ITEM DATA IS THE TAB-TO-DOCUMENT MAP, and it is the one thing a tab bar
                '' must not lose. tiko stores the clsDocument ptr itself; this shell stores
                '' an INDEX into its own table, because the pointer would then live in two
                '' places and one of them would go stale on close.
                g_tabs->AddTab( DWSTRING("alpha.bas"), 0 )
                g_tabs->AddTab( DWSTRING("beta.bas"),  1 )
                Check "  two tabs added", (g_tabs->GetCount() = 2), str(g_tabs->GetCount())
                Check "  and the second reads back its name", _
                      (g_tabs->GetText(1) = DWSTRING("beta.bas")), g_tabs->GetText(1).Utf8

                '' SetCurSel is documented SILENT -- it must NOT fire OnSelect, or selecting
                '' a tab programmatically would re-enter the switch that selected it.
                g_tabs->SetCurSel( 1 )
                Check "  selection follows SetCurSel", (g_tabs->GetCurSel() = 1), _
                      str(g_tabs->GetCurSel())

                g_tabs->clear()
                Check "  and clear empties it", (g_tabs->GetCount() = 0)
            end scope

            '' ---- THE SIDE PANEL IS A REAL CONTROL TOO --------------------------------
            '' Same shape as the tab bar's block above, and here for the same reason: the
            '' widget has to be shown WORKING in this binary, not merely compiling in it.
            '' PsListTree is exercised by tests/pstree and by one demo; a control can be
            '' green in both and still be wired wrong by its third host.
            scope
                Check "the side panel is a real control now, not a stub", (g_panel <> 0)
                Check "  and starts empty in a windowless run", (g_panel->GetCount() = 0), _
                      str(g_panel->GetCount())

                '' THE TWO-LEVEL SHAPE the bookmarks list needs: a header row per file with
                '' rows beneath it. Asserted through the tree API rather than by reading the
                '' flags back, because what matters is that a child KNOWS its parent -- that
                '' is what the click handler in commit 5 will walk.
                dim as long nHdr = g_panel->AddHeader( DWSTRING("probe.bas"), 0 )
                dim as long nRow = g_panel->AddNode( nHdr, DWSTRING("  line 12"), 0 )
                Check "  a header and a child row are added", _
                      (g_panel->GetCount() = 2), str(g_panel->GetCount())
                Check "  the header knows it is one", g_panel->IsHeader( nHdr )
                Check "  and the child knows its parent", _
                      (g_panel->GetParent(nRow) = nHdr), str(g_panel->GetParent(nRow))

                '' COLLAPSING HIDES THE CHILD WITHOUT DELETING IT -- the distinction the
                '' loader depends on, since it restores the top index across a reload and a
                '' visible index is not a model index.
                g_panel->CollapseRow( nHdr )
                Check "  collapsing hides the child", (g_panel->GetVisibleCount() = 1), _
                      str(g_panel->GetVisibleCount())
                Check "    without deleting it", (g_panel->GetCount() = 2)
                g_panel->ExpandRow( nHdr )
                Check "    and expanding brings it back", (g_panel->GetVisibleCount() = 2)

                g_panel->clear()
                Check "  and clear empties it", (g_panel->GetCount() = 0)
            end scope

            '' THE BLANK-TAB BUG, ASSERTED. Two tabs opened, both titled correctly, both
            '' EMPTY -- and nothing in this suite noticed, because the failure is entirely
            '' inside Scintilla. It cost a screenshot to find.
            ''
            '' LoadDiskFile only fills clsDocument.TextBuffer. AssignTextBuffer is what
            '' pushes it in, AND IT CREATES THE VIEWS ITSELF -- so its own guard,
            ''     if gAppHost.IsViewAlive(this.hWindow(0)) then exit function
            '' fires against an explicit CreateScintillaWindows and the text never arrives.
            '' A guard against DOUBLE assignment cannot tell an eager caller from a second
            '' one.
            ''
            '' Driven on a SCRATCH Scintilla document so the editor the rest of this suite
            '' looks at is left exactly as it was.
            scope
                '' ADDREF BEFORE LOOKING AWAY. SCI_GETDOCPOINTER hands back the current
                '' document WITHOUT taking a reference, and SCI_SETDOCPOINTER releases the
                '' outgoing one -- so pointing the view elsewhere takes the shell's original
                '' document to a refcount of zero and FREES it. Restoring it afterwards then
                '' walks memory Scintilla has already given back.
                ''
                '' That is what segfaulted this suite twice, AFTER printing every assertion
                '' as ok: the report was perfect and the process died on the way out, which
                '' is why an empty result line was the only symptom and I read past it.
                dim as any ptr pWasDoc = cast( any ptr, g_view->Msg(SCI_GETDOCPOINTER, 0, 0) )
                if pWasDoc <> 0 then g_view->Msg( SCI_ADDREFDOCUMENT, 0, cast(integer, pWasDoc) )
                dim as any ptr pScratch = cast( any ptr, g_view->Msg(SCI_CREATEDOCUMENT, 0, 0) )
                Check "a scratch document can be created", (pScratch <> 0)

                if pScratch <> 0 then
                    g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pScratch) )

                    '' THE RIGHT ORDER: buffer, then assign.
                    dim as clsDocument ptr pGood = new clsDocument
                    pGood->TextBuffer = "hello"
                    pGood->AssignTextBuffer()
                    Check "  AssignTextBuffer puts the buffer into the view", _
                          (SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0) = 5), _
                          str(SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0))
                    delete pGood

                    '' THE WRONG ORDER, which is what shipped for one build: create first
                    '' and the assignment is skipped, silently.
                    dim as any ptr pScratch2 = cast( any ptr, g_view->Msg(SCI_CREATEDOCUMENT, 0, 0) )
                    if pScratch2 <> 0 then
                        g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pScratch2) )
                        dim as clsDocument ptr pBad = new clsDocument
                        pBad->TextBuffer = "hello"
                        pBad->CreateScintillaWindows()
                        pBad->AssignTextBuffer()
                        Check "  and creating the views FIRST makes it a silent no-op", _
                              (SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0) = 0), _
                              str(SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0))
                        delete pBad
                    end if

                    '' POINT THE VIEW AWAY BEFORE RELEASING, and this order is the whole of
                    '' it. SCI_RELEASEDOCUMENT drops the reference; releasing the document
                    '' the view is CURRENTLY SHOWING takes its refcount to zero and frees it
                    '' underneath the view, and the next SCI_SETDOCPOINTER then walks freed
                    '' memory. The first version of this block released each scratch document
                    '' while it was still current and segfaulted the suite AFTER printing
                    '' every assertion as ok -- which is why the crash was missed twice: the
                    '' report was perfect and the process died on the way out.
                    g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pWasDoc) )
                    '' The view holds it again, so drop the reference taken above.
                    if pWasDoc <> 0 then g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pWasDoc) )
                    if pScratch2 <> 0 then
                        g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pScratch2) )
                    end if
                    g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pScratch) )
                end if
            end scope

            '' THE SWITCH'S GUARDS, and the two of them are NOT equally well covered --
            '' checked by reverting each:
            ''
            ''   * THE BOUNDS GUARD BITES, fatally. Removing it does not fail an assertion,
            ''     it CRASHES the run: ShellTabs_Show(999) indexes g_tabDocs past its end.
            ''     So "does nothing" below really means "does not take the process with it",
            ''     which is worth having even though the report reads the same either way.
            ''
            ''   * THE RE-SHOW GUARD IS UNASSERTED, and removing it changes nothing here.
            ''     With no documents open, the other guards absorb the call before it can
            ''     matter. What that guard actually prevents is a redundant
            ''     SCI_SETDOCPOINTER and a SaveView of the tab being re-shown, and neither is
            ''     observable without a live view. Said here rather than left to be found.
            scope
                dim as long nWas = g_nTabCur
                g_nTabCur = 5
                ShellTabs_Show( -1 )
                Check "showing a negative tab does nothing", (g_nTabCur = 5)
                ShellTabs_Show( 999 )
                Check "  and neither does one past the end", (g_nTabCur = 5)
                ShellTabs_Show( 5 )
                Check "  and re-showing the current tab is a no-op", (g_nTabCur = 5)
                g_nTabCur = nWas
            end scope

            '' ---- GROUP K: THE APP-HOST SEAM, FILLED BY A SECOND HOST --------------------
            '' Until this binary, AppHostServices had exactly one implementation and the
            '' record was a seam in name only -- nothing showed it could be filled by
            '' anything but AfxNova. These assert that it IS filled, field by field, because
            '' a null one is not a compile error: it is a crash at the moment a document
            '' first needs it.
            Check "the services record is complete", AppHost_IsComplete(), _
                  AppHost_FirstMissing()
            Check "  and so is the notification record", AppNotify_IsComplete(), _
                  AppNotify_FirstMissing()

            '' ---- A RELAYOUT ARRIVING BEFORE THE TREE EXISTS MUST BE DROPPED -----------
            '' THIS SUITE COULD NOT SEE THE DEFECT THIS GUARDS, and that is the reason it is
            '' worth having. Opening a document from inside BuildTree fires
            '' gAppNotify.RelayoutTopTabs, which laid out a tree whose last five children
            '' had not been constructed -- an access violation at startup, no output, and
            '' 300 assertions still passing, because the suite opens its files AFTER
            '' BuildTree has finished and never reproduces the ordering.
            ''
            '' What is checkable is the guard itself: with the flag down, a relayout changes
            '' nothing. The bounds are moved first so that a LayoutAll would put them back.
            scope
                dim as boolean bWasReady = g_bTreeReady
                dim as PsRect  rcWas     = g_status->bounds

                g_status->SetBounds( PsRc(1, 2, 3, 4) )
                g_bTreeReady = false
                gAppNotify.RelayoutMain()
                Check "a relayout before the tree is ready is dropped", _
                      (g_status->bounds.x = 1) andalso (g_status->bounds.w = 3), _
                      str(g_status->bounds.x) & "," & str(g_status->bounds.w)

                '' AND IS OBEYED ONCE IT IS. Without this the assertion above is satisfied
                '' by a RelayoutMain that never works at all.
                g_bTreeReady = true
                gAppNotify.RelayoutMain()
                Check "  and obeyed once it is", (g_status->bounds.w <> 3), _
                      str(g_status->bounds.w)

                g_status->SetBounds( rcWas )
                g_bTreeReady = bWasReady
            end scope

            '' SPOT-CHECKS ON THE ONES THAT ANSWER SOMETHING, because "non-null" and
            '' "correct" are different claims and only the first is checked above.
            Check "  CreateView hands back the editor the layout already built", _
                  (gAppHost.CreateView(0) = cast(any ptr, g_view))
            Check "    and view 1 is the SPLIT, not a third view", _
                  (gAppHost.CreateView(1) = cast(any ptr, g_view2))
            Check "  a live view reports alive", gAppHost.IsViewAlive( g_view )
            Check "    and a null one does not", (gAppHost.IsViewAlive(0) = false)
            Check "  the view's Scintilla pointer is the view's own", _
                  (gAppHost.ViewSciPointer(g_view) = g_view->pSci)

            '' ---- THE LOSSY-SAVE PROMPT -------------------------------------------------
            '' THIS ASSERTION WAS A HANG WAITING TO HAPPEN and commit 7 is what armed it. It
            '' used to read `ConfirmLossySave(0, "x", 0) = false` against the LIVE g_pSurf,
            '' which was safe only because the implementation printed and returned. Now it
            '' raises a real modal, and a real modal in a windowless run blocks forever
            '' waiting for a button nobody will press -- the identical trap ConfirmExit
            '' documents forty lines below. The surface is nulled, or the hook is set, in
            '' every case here.
            scope
                dim as PsSurface ptr pWas = g_pSurf

                '' THE TEST HOOK, WHICH IS THE APP LAYER'S, NOT THE SHELL'S.
                '' gLossySaveTestAnswer lives in app/modEncoding.bi and TIKO_SAVE_SELFTEST
                '' drives tiko's save path through it. Honouring it here is what lets that
                '' suite mean the same thing against either host.
                gLossySaveTestAnswer = -1
                Check "  the lossy-save hook can force a refusal", _
                      (gAppHost.ConfirmLossySave(0, DWSTRING("x"), 0) = false)
                gLossySaveTestAnswer = 1
                Check "    and an acceptance, without a box either way", _
                      gAppHost.ConfirmLossySave(0, DWSTRING("x"), 0)
                gLossySaveTestAnswer = 0

                '' NO SURFACE MEANS REFUSE, NOT ASSUME. The convenient answer here discards
                '' characters the user never agreed to lose, so the fallback is the safe one.
                g_pSurf = 0
                Check "  and with nothing to ask with, a lossy save is refused", _
                      (gAppHost.ConfirmLossySave(0, DWSTRING("x"), 0) = false)

                '' THE COMPOSITION, which is everything about these two boxes that does not
                '' need a compositor: captions, buttons, the default and the cancel id. Split
                '' out of the show for exactly this reason -- see shellhost.bi.
                scope
                    dim as PsMessageBox box
                    BuildLossySaveBox( box, DWSTRING("C:/x/a.bas"), 0 )
                    Check "  the lossy box offers OK and Cancel", _
                          (box.GetButtonCount() = 2), str(box.GetButtonCount())
                    Check "    with OK first", (box.ButtonId(0) = MBX_ID_OK)
                    '' THE LOAD-BEARING ONE. A destructive box whose default is OK loses
                    '' characters to a reflexive Return, and nothing on screen would say so.
                    Check "    and CANCEL as the default, because this one destroys text", _
                          (box.ButtonId(box.GetDefaultButton()) = MBX_ID_CANCEL), _
                          str(box.GetDefaultButton())
                    Check "    Escape cancels", (box.ResolveCancelId() = MBX_ID_CANCEL)
                    '' L(516) and L(518) are ids 516/518 in all six .lang files. An id that
                    '' was NOT would render blank with nothing to say so -- the failure mode
                    '' the localization rule exists for -- so the text is checked non-empty
                    '' rather than assumed.
                    Check "    and neither caption nor text is blank", _
                          (len(box.sCaption) > 0) andalso (len(box.sText) > 0)
                end scope

                scope
                    dim as PsMessageBox box
                    BuildWriteFailedBox( box, DWSTRING("C:/x/a.bas"), DWSTRING("locked") )
                    Check "  the write-failure box is OK-only", _
                          (box.GetButtonCount() = 1), str(box.GetButtonCount())
                    Check "    and Escape dismisses it", (box.ResolveCancelId() = MBX_ID_OK)
                    '' The error text is what tells the user WHICH file and WHY, and it is
                    '' the argument rather than a fixed string, so it is checked for.
                    Check "    it names the file and the reason", _
                          (instr(box.sText.Utf8, "a.bas") > 0) andalso _
                          (instr(box.sText.Utf8, "locked") > 0)
                end scope

                g_pSurf = pWas
            end scope

            '' ---- GROUP L: OPEN AND SAVE ------------------------------------------------
            '' The commands themselves reach disk and a file dialog, so what is asserted is
            '' the part that decides WHETHER they do: the duplicate lookup, and the refusal
            '' to save when nothing is open.
            scope
                dim as long nWasCount = g_nTabDocs
                dim as long nWasCur   = g_nTabCur
                '' PUT BACK WHAT WAS THERE, rather than blanked. `--selftest` takes file
                '' arguments like any other run, so tabs 0 and 1 can be real open documents.
                dim as clsDocument ptr pWas0 = g_tabDocs(0).pDoc
                dim as clsDocument ptr pWas1 = g_tabDocs(1).pDoc

                '' ---- ADDRESSES THAT ARE NEVER DEREFERENCED, and that is what makes this
                '' assertable at all. ShellTabs_IndexOfDoc compares POINTERS and reads nothing
                '' through them, so two fabricated ones drive every branch without allocating
                '' a clsDocument -- and a version that started dereferencing would fault here
                '' rather than in front of a user.
                dim as clsDocument ptr pFake0 = cast( clsDocument ptr, 4096 )
                dim as clsDocument ptr pFake1 = cast( clsDocument ptr, 8192 )
                g_tabDocs(0).pDoc = pFake0
                g_tabDocs(1).pDoc = pFake1
                g_nTabDocs = 2

                Check "an open document is found by its tab", _
                      (ShellTabs_IndexOfDoc(pFake1) = 1), str(ShellTabs_IndexOfDoc(pFake1))
                Check "  the first one too", (ShellTabs_IndexOfDoc(pFake0) = 0)
                Check "  a document in no tab is not found", _
                      (ShellTabs_IndexOfDoc(cast(clsDocument ptr, 12288)) = -1)
                '' A null is what gApp.GetDocumentPtrByFilename returns for a file that is
                '' not open, so it arrives here on every miss. AN EMPTY SLOT IS SEEDED TO
                '' MAKE THIS BITE: without one, the loop finds no match anyway and the guard
                '' above it is doing nothing that could be observed. With one, a missing
                '' guard reports THAT TAB for every unknown file -- which reads as "already
                '' open" and quietly refuses to open anything else.
                g_tabDocs(1).pDoc = 0
                Check "  and a null matches nothing, even against an empty tab", _
                      (ShellTabs_IndexOfDoc(0) = -1), str(ShellTabs_IndexOfDoc(0))
                g_tabDocs(1).pDoc = pFake1

                '' THE PATH LOOKUP IS gApp's NOW, so what is asserted here is the DELEGATION:
                '' with an empty pDocList nothing is open, whatever this array holds. The
                '' case-insensitivity that used to be tested here belongs to
                '' clsApp.GetDocumentPtrByFilename and is tiko's, not the shell's.
                Check "  a path finds nothing while gApp's list is empty", _
                      (gApp.pDocList = 0) andalso _
                      (ShellTabs_FindByPath(DWSTRING("C:/dev/one.bas")) = -1)
                Check "  and neither does an empty path", _
                      (ShellTabs_FindByPath(DWSTRING("")) = -1)

                g_tabDocs(0).pDoc = pWas0
                g_tabDocs(1).pDoc = pWas1
                g_nTabDocs = nWasCount
                g_nTabCur  = nWasCur

                '' SAVE WITH NOTHING OPEN. The document model would fault on a null pDoc,
                '' and Ctrl+S is reachable from the very first frame -- before any file is.
                g_nTabCur = -1
                Check "saving with no document open is refused, not faulted on", _
                      (ShellTabs_Save(false) = false)
                Check "  and so is Save As", (ShellTabs_Save(true) = false)
                Check "  with no document to hand back", (ShellTabs_CurrentDoc() = 0)
                g_nTabCur = nWasCur
            end scope

            '' ---- GROUP M: A REAL FILE, THROUGH THE REAL OPEN PATH -----------------------
            '' EVERYTHING ELSE IN THIS SUITE DRIVES THE PIECES. This drives ShellTabs_Open
            '' itself, on a file that exists, and it is here because commit 1 changed HOW a
            '' document is created -- gApp.CreateEmptyDocument instead of `new clsDocument`.
            ''
            '' The defect that change fixes is invisible from outside: the tabs worked, the
            '' text appeared, and gApp.pDocList was permanently EMPTY, so every model-side
            '' walk in the app layer saw no documents at all. Nothing failed. Nothing could.
            ''
            '' Windowless is fine here: `--selftest` builds the whole tree, so g_view and
            '' g_tabs are real, and modSaveSelfTest already establishes that the temp
            '' directory is a legitimate place for a suite to put a file.
            ''
            '' ---- PsKnownFolder, NOT environ("TEMP") -- 7c step 18 --------------------
            '' All four scratch directories in this suite read %TEMP% directly until step
            '' 18. TEMP IS NOT SET ON LINUX, so each one resolved to a bare relative name
            '' and the suite scattered probe files into the current directory -- or failed
            '' to create them at all, which is the same as a suite that never ran.
            '' PsKnownFolder(PSFOLDER_TEMP) answers %TEMP% on Windows and $TMPDIR-or-/tmp
            '' on Linux, and it has existed in PsCore since before this file did.
            ''
            '' And '/' rather than '\' in every join below, per PsPath.bi's house rule:
            '' shared logic uses forward slashes and PsFile converts at the API boundary.
            scope
                dim as DWSTRING wszDir = PsPathJoin( PsKnownFolder( PSFOLDER_TEMP ), DWSTRING("tiko_shellopen") )
                PsDirCreate( wszDir )
                dim as DWSTRING wszFile = wszDir & "/open_probe.bas"
                '' ---- REAL SOURCE, NOT TWO LINES OF FILLER, since 7c step 5 ------------
                '' The probe used to be "' probe" and one print. It carries two PROCEDURES
                '' now because the scanner assertions below need something to find, and one
                '' file serves both: the earlier assertions only care that it has bytes and
                '' a line 1, which it still does.
                '' ---- AND A DECLARE-ONLY SYMBOL, since 7c step 5 commit 3 --------------
                '' ProbeGamma has no BODY. It is here so the Functions panel's
                '' "if nBodyLine <= 0 then continue for" has something to skip -- without
                '' it that guard could be deleted and every assertion stayed green, which
                '' is what the first revert-to-red pass showed.
                '' ---- AND IT INCLUDES THE SECOND PROBE, since 7c step 6 --------------
                '' The PROJECT tier follows #includes from disk, so this line is what puts
                '' a second file in the graph and lets the pane show more than one. The
                '' BUFFER tier passes no include paths at all, so its scan of this file
                '' does not resolve it -- which is why the three-symbol assertions above
                '' are unaffected.
                dim as boolean bWrote = PsFileWriteAll( wszFile, _
                        !"' probe\n#include once \"open_probe2.bas\"\n\n" & _
                        !"sub ProbeAlpha()\n  print 1\nend sub\n\n" & _
                        !"function ProbeBeta( byval n as long ) as long\n  return n\nend function\n\n" & _
                        !"declare sub ProbeGamma()\n" )

                if bWrote = false then
                    Check "the open probe could be written to %TEMP%", false, wszFile.Utf8
                else
                    dim as long nWasCount = g_nTabDocs
                    dim as long nWasCur   = g_nTabCur
                    '' The view is pointed at a new document by Open, so its current one is
                    '' held across the test -- SCI_GETDOCPOINTER takes no reference, and
                    '' restoring one that has been freed underneath us is what crashed this
                    '' suite for two commits in step 3.
                    dim as any ptr pWasDoc = 0
                    if g_view <> 0 then
                        pWasDoc = cast( any ptr, g_view->Msg(SCI_GETDOCPOINTER, 0, 0) )
                        if pWasDoc <> 0 then g_view->Msg( SCI_ADDREFDOCUMENT, 0, cast(integer, pWasDoc) )
                    end if

                    dim as long idx1 = ShellTabs_Open( wszFile )

                    '' ---- THE SCAN IS ASYNCHRONOUS SINCE 7c STEP 7 --------------------
                    '' Every assertion below used to read gSymDb on the line after asking
                    '' for a scan, which worked because the scan WAS the asking. Now the
                    '' request goes to a worker and this waits for it.
                    ''
                    '' A BOUNDED WAIT ON A CONDITION, not a fixed sleep: a slow machine
                    '' waits longer and still passes, and a broken worker fails the
                    '' assertions in bounded time instead of hanging the suite.
                    ShellScan_DrainFor( 5000 )
                    Check "a real file opens into a tab", (idx1 >= 0), str(idx1)
                    '' THE POINT OF THE COMMIT. Before it, this count was zero however many
                    '' files were open.
                    Check "  and the document is in gApp's list", _
                          (gApp.GetDocumentCount() = 1), str(gApp.GetDocumentCount())
                    Check "  which is the same document the tab holds", _
                          (gApp.pDocList = ShellTabs_CurrentDoc())
                    Check "  and it loaded its bytes", _
                          (SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0) > 0), _
                          str(SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0))
                    '' SetProjectFileType ran: a .bas is a real category, not UNDEFINED.
                    '' ProjectFileType is a DWSTRING holding "0".."5", not a number -- the
                    '' FILETYPE_* names are string #defines (clsDocument.bi:34-39), which is
                    '' why this reads .Utf8 rather than str().
                    Check "  and it was categorised by extension", _
                          (ShellTabs_CurrentDoc()->ProjectFileType <> FILETYPE_UNDEFINED), _
                          ShellTabs_CurrentDoc()->ProjectFileType.Utf8

                    '' ---- THE SCANNER, END TO END --------------------------------------
                    '' Opening a document calls gAppNotify.RequestBufferScan, which is no
                    '' longer a stub -- so by this line the file has already been parsed and
                    '' installed into gSymDb, WITHOUT anything here asking for it. That is
                    '' the assertion: the seam fires on its own.
                    ''
                    '' gSymDb IS GLOBAL STATE THIS SUITE NOW WRITES, like gApp.pDocList
                    '' before it. The cleanup below evicts what it installed.
                    scope
                        Check "  opening a file scanned it", (g_nScanCount > 0), _
                              str(g_nScanCount)
                        Check "    and the parse succeeded", (g_nLastScanMs >= 0)

                        '' THE PROCS ARE IN THE DATABASE, which is the whole reason the
                        '' Functions panel can exist -- it reads gSymDb, never the scanner.
                        '' THREE, NOT TWO: EnumProcsInFile returns DECLARATIONS as well as
                        '' definitions, and the probe file carries a declare-only
                        '' ProbeGamma. Filtering them is the PANEL's job (SymBodyLine = 0),
                        '' not the database's -- a distinction worth an assertion of its
                        '' own, since it is what a Functions list would otherwise get wrong.
                        dim rs() as SYMBOLREF
                        dim as long nProcs = gSymDb.EnumProcsInFile( wszFile, rs() )
                        Check "    and all three procedure symbols are in gSymDb", _
                              (nProcs = 3), str(nProcs)
                        scope
                            dim as long nWithBody = 0
                            for p as long = 0 to nProcs - 1
                                if gSymDb.SymBodyLine( rs(p) ) > 0 then nWithBody += 1
                            next
                            Check "      two of which have bodies", (nWithBody = 2), _
                                  str(nWithBody)
                        end scope

                        '' ---- THE SAME FILE, SPELLED THE OTHER WAY -- 7c step 18 ------
                        '' A path is not a string, and both of these lookups compared it
                        '' as one until step 18. Asserted rather than described, because
                        '' the failure is a Functions pane with no rows and a SECOND
                        '' clsDocument for a file already open -- neither of which is an
                        '' error anywhere.
                        ''
                        '' THE QUERY IS BUILT BY SWAPPING SEPARATORS, not by hardcoding a
                        '' spelling: on Linux the probe path has no backslash to swap, so
                        '' the swapped form is IDENTICAL to the original and this asserts
                        '' only that a path still matches itself. That is the honest thing
                        '' for it to assert there. The Windows run is where it bites.
                        scope
                            dim as DWSTRING wszAlt = wszFile
                            dim as ushort ptr pu = wszAlt.Units()
                            if pu <> 0 then
                                for i as uinteger = 0 to wszAlt.Length - 1
                                    if pu[i] = asc("/") then
                                        pu[i] = asc("\")
                                    elseif pu[i] = asc("\") then
                                        pu[i] = asc("/")
                                    end if
                                next
                            end if
                            '' AGAINST 3, NOT AGAINST nProcs. The first draft compared the
                            '' two lookups to each other and PASSED with the fix reverted:
                            '' the parser's file table holds a MIXED spelling -- the
                            '' include directory backslashed, the filename appended with
                            '' '/' -- so without the fold BOTH spellings miss and the
                            '' assertion compared 0 with 0. Caught by reverting to red,
                            '' which is the only reason it is not still in the file.
                            Check "    the same file with the other separator is the same file", _
                                  (gSymDb.EnumProcsInFile( wszAlt, rs() ) = 3), wszAlt.Utf8
                            Check "      and gApp agrees it is one document", _
                                  (gApp.GetDocumentPtrByFilename( wszAlt ) <> 0) andalso _
                                  (gApp.GetDocumentPtrByFilename( wszAlt ) = _
                                   gApp.GetDocumentPtrByFilename( wszFile ))
                        end scope

                    '' ---- THE FIND BAR, 7c step 27 ---------------------------------
                    ''
                    '' The first thing in this binary that SEARCHES, so the assertion
                    '' that matters is not the bar's shape -- it is a search over a
                    '' known buffer returning a known count.
                    ''
                    '' STEP 26 IS WHY THAT IS THE ONE WRITTEN FIRST. Its gates were
                    '' green through a find engine that could not update a count, could
                    '' not replace text, and replaced the wrong number of matches. A
                    '' bar whose widgets all exist and whose search finds nothing would
                    '' look identical from here.
                    scope
                        '' ---- THE SYMBOL FONT IS IN THE CHAIN, 7c step 31 --------
                        '' EVERY PUA GLYPH IN THIS BINARY WAS A TOFU BOX until this
                        '' step, because TE_Init opens CascadiaCode.ttf and the shell
                        '' added no fallback -- the pane switcher since step 20, the
                        '' Explorer's icons, and both bars' chevrons. A comment in this
                        '' file asserted the chain resolved them. Nobody had looked at
                        '' the window since step 22.
                        ''
                        '' ASSERTED ON THE FACE INDEX, not on "it did not crash": a
                        '' missing codepoint still shapes, to gid 0 in face 0, and every
                        '' painter draws that box without complaint. faceIdx > 0 is the
                        '' only thing that says a FALLBACK answered.
                        scope
                            dim as string sPua = chr(&hEE, &hA2, &hA9)   '' U+E8A9
                            dim as PsShapedRun ptr r = TE_ShapeCached( g_te, sPua )
                            Check "  a private-use glyph shapes to something", _
                                  (r <> 0) andalso (r->nGlyphs > 0)
                            if (r <> 0) andalso (r->nGlyphs > 0) then
                                Check "    resolved by a FALLBACK face, not the primary", _
                                      (r->glyphs[0].faceIdx > 0), str(r->glyphs[0].faceIdx)
                                Check "      and it is a real glyph, not notdef", _
                                      (r->glyphs[0].gid <> 0), str(r->glyphs[0].gid)
                            end if
                            '' AND THE PRIMARY STILL WINS FOR TEXT IT COVERS. A chain that
                            '' answered everything would put the UI font's own letters in
                            '' the icon face -- "Aa" and "AB" are literal text on these
                            '' bars, so that is not hypothetical.
                            dim as PsShapedRun ptr r2 = TE_ShapeCached( g_te, "Aa" )
                            if (r2 <> 0) andalso (r2->nGlyphs > 0) then
                                Check "    and ordinary letters still come from the primary", _
                                      (r2->glyphs[0].faceIdx = 0), str(r2->glyphs[0].faceIdx)
                            end if
                        end scope

                        Check "  the find bar is a real bar", (g_barFind <> 0)
                        Check "    with a field", (g_barFind->pField <> 0)
                        '' THREE SINCE 7c STEP 29 added Selection. Two through step 28.
                        Check "    three toggles", (g_barFind->pToggle <> 0) andalso _
                              (g_barFind->pToggle->GetCount() = 3), _
                              str(g_barFind->pToggle->GetCount())
                        Check "    and three navigation buttons", (g_barFind->pNav <> 0) andalso _
                              (g_barFind->pNav->GetCount() = 3), _
                              str(g_barFind->pNav->GetCount())

                        '' BOTH TOGGLES LATCH. A COMMAND item never selects, so Match
                        '' Case would read as permanently off and the flag the engine
                        '' reads would never change -- which is the shape of the defect
                        '' step 20 found in the pane switcher.
                        dim as boolean bBothToggle = true
                        for k as long = 0 to g_barFind->pToggle->GetCount() - 1
                            if g_barFind->pToggle->items(k).kind <> PSICON_TOGGLE then bBothToggle = false
                        next
                        Check "      and the toggles latch", bBothToggle

                        '' THE NAVIGATION CARRIES REAL MENU IDS, which is what makes a
                        '' button and its menu entry the same path rather than two.
                        Check "      while the navigation carries menu ids", _
                              (g_barFind->pNav->FindItemByID( IDM_FINDPREV ) >= 0) andalso _
                              (g_barFind->pNav->FindItemByID( IDM_FINDNEXT ) >= 0) andalso _
                              (g_barFind->pNav->FindItemByID( IDM_FIND ) >= 0)

                        '' ---- AND IT SEARCHES ------------------------------------
                        '' EVERY NUMBER BELOW IS DERIVED FROM THE PROBE FILE, and the
                        '' first draft guessed instead: it asserted 3 for "Probe"
                        '' because there are three procedures, and got 5. The engine was
                        '' right. The file this suite writes is
                        ''
                        ''     ' probe
                        ''     #include once "open_probe2.bas"
                        ''     sub ProbeAlpha() ... function ProbeBeta() ...
                        ''     declare sub ProbeGamma()
                        ''
                        '' so case-insensitive "Probe" is FIVE -- the comment, the
                        '' include's filename, and the three procedure names. Anything
                        '' editing that file has to move these, exactly as it already
                        '' has to move the two line numbers further up.
                        dim as DWSTRING sWas = gFind.txtFind
                        gFind.txtFind = DWSTRING("Probe")
                        gFind.nMatchCase = 0
                        gFind.nWholeWord = 0
                        gFind.bShowFindPanel = true
                        FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                        Check "    searching the probe file finds all five", _
                              (gFind.foundCount = 5), str(gFind.foundCount)

                        '' AND THE COUNT IS PUBLISHED, which is a separate fact from
                        '' having been computed: the engine writes gFind.wszResults and
                        '' asks the host to repaint. A number computed and never
                        '' published is exactly what step 26 shipped.
                        Check "      and publishes a count, not 0/0", _
                              (gFind.wszResults <> DWSTRING("0/0")), gFind.wszResults.Utf8

                        '' ---- WHOLE WORD CHANGES THE ANSWER ----------------------
                        '' Only the comment's "probe" stands alone. In the three names
                        '' it is a prefix, and in the include it is inside a filename --
                        '' so Whole Word takes five to ONE, which is a sharper statement
                        '' than "the number changed".
                        gFind.nWholeWord = 1
                        FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                        Check "    Whole Word keeps only the standalone one", _
                              (gFind.foundCount = 1), str(gFind.foundCount)
                        gFind.nWholeWord = 0

                        '' ---- MATCH CASE CHANGES THE ANSWER ----------------------
                        '' Lowercase "probe" occurs TWICE as typed -- the comment and
                        '' the include's filename -- and five times ignoring case.
                        gFind.txtFind = DWSTRING("probe")
                        gFind.nMatchCase = 1
                        FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                        Check "    Match Case keeps only the two written that way", _
                              (gFind.foundCount = 2), str(gFind.foundCount)
                        gFind.nMatchCase = 0
                        FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                        Check "      and without it the same term finds all five", _
                              (gFind.foundCount = 5), str(gFind.foundCount)

                        '' ---- THE FIELD IS THE ONE WRITER OF gFind.txtFind -------
                        '' Step 26 spent two defects on that rule. Driven through the
                        '' control's own callback, not by assigning the field, so this
                        '' asserts the WIRING as well as the assignment.
                        g_barFind->pField->SetText( DWSTRING("Alpha") )
                        ShellFind_OnFieldChange( g_barFind->pField, 0 )
                        Check "    typing in the field writes the search term", _
                              (gFind.txtFind = DWSTRING("Alpha")), gFind.txtFind.Utf8
                        Check "      and re-searches on the way", _
                              (gFind.foundCount = 1), str(gFind.foundCount)

                        '' ---- AND THE TOGGLE ACTUALLY MOVES THE FLAG -------------
                        '' Every assertion above sets gFind.nMatchCase DIRECTLY, so
                        '' none of them touches the handler that is supposed to. Proved
                        '' by reverting: deleting the assignment inside ShellFind_OnIcon
                        '' left the suite at 508/0. Fourth step running that an
                        '' installation or a wiring went unasserted because the policy
                        '' was driven straight.
                        ''
                        '' DRIVEN THROUGH THE HANDLER, with the item latched first --
                        '' the control flips a TOGGLE before the callback runs, and the
                        '' handler READS that rather than flipping again, so the fixture
                        '' has to do what the control would.
                        scope
                            dim as long iMC = g_barFind->pToggle->FindItemByID( SHFIND_ID_MATCHCASE )
                            Check "    the Match Case toggle is findable by id", (iMC >= 0), str(iMC)
                            gFind.nMatchCase = 0
                            g_barFind->pToggle->SetSelected( iMC, true )
                            ShellFind_OnIcon( g_barFind->pToggle, iMC, 0 )
                            Check "      and clicking it sets the flag the engine reads", _
                                  (gFind.nMatchCase <> 0), str(gFind.nMatchCase)
                            g_barFind->pToggle->SetSelected( iMC, false )
                            ShellFind_OnIcon( g_barFind->pToggle, iMC, 0 )
                            Check "        and clicking it again clears it", _
                                  (gFind.nMatchCase = 0), str(gFind.nMatchCase)
                        end scope

                        '' ---- THE CALLBACKS ARE INSTALLED ------------------------
                        '' Three steps running an installation went unasserted because
                        '' the policy was tested by calling the callback directly.
                        Check "    the field's change and enter hooks are installed", _
                              (g_barFind->pField->pfnChange <> 0) andalso _
                              (g_barFind->pField->pfnEnter <> 0)
                        Check "      as are both icon strips'", _
                              (g_barFind->pToggle->pfnClick <> 0) andalso _
                              (g_barFind->pNav->pfnClick <> 0)

                        gFind.txtFind = sWas
                        gFind.bShowFindPanel = false
                        gFind.foundCount = 0
                    end scope

                    '' ---- THE REPLACE BAR, 7c step 28 -----------------------------
                    ''
                    '' Same rule as the Find bar above: the assertion that matters is
                    '' not the bar's shape, it is A REAL REPLACE OVER A KNOWN BUFFER,
                    '' undone afterwards so the rest of the suite sees the file it
                    '' expects.
                    ''
                    '' STEP 26 SHIPPED A REPLACE THAT PUT AN EMPTY STRING IN EVERY
                    '' MATCH and every gate stayed green, because nothing had ever
                    '' written gFind.txtReplace. That is precisely what a bar-shape
                    '' assertion cannot see and this one can.
                    scope
                        Check "  the replace bar is a real bar", (g_barReplace <> 0)
                        Check "    with a field", (g_barReplace->pField <> 0)
                        Check "    a Preserve Case toggle", (g_barReplace->pPreserve <> 0) andalso _
                              (g_barReplace->pPreserve->GetCount() = 1), _
                              str(g_barReplace->pPreserve->GetCount())
                        Check "    and two actions", (g_barReplace->pActions <> 0) andalso _
                              (g_barReplace->pActions->GetCount() = 2), _
                              str(g_barReplace->pActions->GetCount())
                        Check "      which carry real menu ids", _
                              (g_barReplace->pActions->FindItemByID( IDM_REPLACENEXT ) >= 0) andalso _
                              (g_barReplace->pActions->FindItemByID( IDM_REPLACEALL ) >= 0)
                        Check "      and Preserve Case latches", _
                              (g_barReplace->pPreserve->items(0).kind = PSICON_TOGGLE)

                        '' ---- REPLACE IMPLIES FIND, BOTH WAYS --------------------
                        '' tiko's `if bReplace then bFind = true`, plus the half tiko
                        '' gets for free by having one function do both. Asserted
                        '' rather than described because a Replace bar over a hidden
                        '' Find bar replaces the empty string.
                        ShellFind_SetVisible( false )
                        ShellReplace_SetVisible( true )
                        Check "    showing Replace shows Find", g_state.bShowFind
                        Check "      and the model agrees", gFind.bShowFindPanel
                        ShellFind_SetVisible( false )
                        Check "    and hiding Find hides Replace", (g_state.bShowReplace = false)
                        Check "      and the model agrees there too", _
                              (gFind.bShowReplacePanel = false)

                        '' ---- THE FIELDS LINE UP, STRUCTURALLY -------------------
                        '' Not arithmetic that happens to match: the Replace field's
                        '' rect is ASKED OF the Find field, as tiko asks it of
                        '' rcFindTextRect.
                        ''
                        '' LAYOUT IS LAZY, AND THE FIRST DRAFT OF THIS ASSERTED 0 = 0.
                        '' SetBounds only marks the widget dirty; OnLayout runs from
                        '' EnsureLayout, which a windowless run never reaches because
                        '' nothing paints. The left-edge check PASSED that way and the
                        '' width check underneath it is the only reason anyone found
                        '' out -- step 24's finding, in a second control.
                        ''
                        '' DRIVEN FROM THE ROOT, in tree order, because this bar's
                        '' OnLayout READS the Find field's rect: laying this one out
                        '' alone would borrow a rect that is still zero.
                        ShellReplace_SetVisible( true )
                        if (g_pSurf <> 0) andalso (g_pSurf->pRoot <> 0) then
                            g_pSurf->pRoot->EnsureLayout()
                        end if
                        '' ---- EVERY STRIP IS A ROW, AND ITS ITEMS FIT IN IT ------
                        '' PsIconPanel DEFAULTS TO A COLUMN. Five strips in this shell
                        '' are rows and not one of them said so, which drew the items
                        '' straight down and out through the bottom of their own rect --
                        '' visible in the first screenshot of this window, invisible to
                        '' every assertion, including step 20's, which pins the BAND's
                        '' height. The band was right and the content was sideways.
                        ''
                        '' ASSERTED ON CONTAINMENT, NOT ONLY ON THE FLAG. The flag is the
                        '' wiring; a last item that lies inside the strip's own rect is
                        '' the CONSEQUENCE, and it stays true if the control ever grows
                        '' another way to be laid out wrong.
                        scope
                            dim as PsIconPanel ptr strips(0 to 4) = { _
                                g_barFind->pToggle, g_barFind->pNav, _
                                g_barReplace->pPreserve, g_barReplace->pActions, _
                                g_panelMenu }
                            dim as boolean bAllRows = true, bAllFit = true
                            dim as string sWhich = "", sFit = ""
                            for k as long = 0 to 4
                                dim as PsIconPanel ptr p = strips(k)
                                if p = 0 then continue for
                                if p->bVertical then
                                    bAllRows = false
                                    sWhich = sWhich & str(k) & " "
                                end if
                                if p->GetCount() > 0 then
                                    dim as PsRect ri = p->ItemRect( p->GetCount() - 1 )
                                    if ((ri.y + ri.h) > p->bounds.h) orelse _
                                       ((ri.x + ri.w) > p->bounds.w) then
                                        bAllFit = false
                                        sFit = sFit & str(k) & ":" & _
                                               str(ri.x + ri.w) & "x" & str(ri.y + ri.h) & _
                                               ">" & str(p->bounds.w) & "x" & _
                                               str(p->bounds.h) & " "
                                    end if
                                end if
                            next
                            Check "    all five icon strips are ROWS, not columns", _
                                  bAllRows, sWhich
                            Check "      and every strip's last item fits inside it", _
                                  bAllFit, sFit

                            '' ---- AND THE CELLS ARE THE SIZE THE BAR WAS DRAWN FOR --
                            '' PsIconPanel's own default is nCellSize = 36 against this
                            '' bar's 24, and CONTAINMENT ALONE CANNOT SEE THE DIFFERENCE:
                            '' the layout asks the control for its width, so a 36-unit
                            '' cell just makes a wider strip that still fits. Reverting
                            '' either half of the fix left 577/0 for exactly that reason.
                            '' This pins the SIZE, which is the part a screenshot showed
                            '' and no relation between two of the control's own numbers
                            '' ever will.
                            scope
                                dim as long want = PsScaleBy( SHFIND_ICON_UNITS, _
                                                              g_pSurf->fScale )
                                '' A ROW'S ITEM IS CELL-WIDE AND STRIP-TALL, not square.
                                '' The first draft of this demanded a square and read
                                '' 24x40 -- the control was right and the expectation was
                                '' mine, for the fifth time in this run of fixtures.
                                dim as PsRect r0 = g_barFind->pToggle->ItemRect( 0 )
                                Check "    and a cell is the bar's own icon size wide", _
                                      (r0.w = want), str(r0.w) & " wanted " & str(want)
                                Check "      and as tall as the strip", _
                                      (r0.h = g_barFind->pToggle->bounds.h), _
                                      str(r0.h) & " wanted " & _
                                      str(g_barFind->pToggle->bounds.h)
                            end scope
                        end scope

                        Check "    the find field has a real rect to borrow", _
                              (g_barFind->pField->bounds.w > 0), _
                              str(g_barFind->pField->bounds.w)
                        Check "      and the two fields share a left edge", _
                              (g_barReplace->pField->bounds.x = g_barFind->pField->bounds.x), _
                              str(g_barReplace->pField->bounds.x) & "/" & _
                              str(g_barFind->pField->bounds.x)
                        Check "      and Preserve Case eats into the field, not past it", _
                              (g_barReplace->pField->bounds.w > 0) andalso _
                              (g_barReplace->pField->bounds.w < g_barFind->pField->bounds.w), _
                              str(g_barReplace->pField->bounds.w) & "/" & _
                              str(g_barFind->pField->bounds.w)

                        '' ---- THE FIELD IS THE ONE WRITER OF gFind.txtReplace ----
                        '' Driven through the control's own callback. The defect this
                        '' guards is not hypothetical -- it is what step 26 shipped.
                        g_barReplace->pField->SetText( DWSTRING("ProbeZeta") )
                        ShellReplace_OnFieldChange( g_barReplace->pField, 0 )
                        Check "    typing in the field writes the replacement", _
                              (gFind.txtReplace = DWSTRING("ProbeZeta")), gFind.txtReplace.Utf8

                        '' ---- AND NOW A REAL REPLACE -----------------------------
                        '' ProbeAlpha appears ONCE, which this file controls -- and
                        '' unlike step 27's counts that number is derived from the same
                        '' search rather than from a glance at the file: the assertion
                        '' below it re-searches and demands the match is GONE.
                        scope
                            dim as DWSTRING sWasF = gFind.txtFind
                            gFind.nMatchCase = 0 : gFind.nWholeWord = 0
                            gFind.nPreserve = 0
                            gFind.txtFind = DWSTRING("ProbeAlpha")
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            Check "    ProbeAlpha is there exactly once to start with", _
                                  (gFind.foundCount = 1), str(gFind.foundCount)

                            '' REPLACE IS A TWO-PRESS GESTURE FROM A COLD CARET, and
                            '' the first draft of this assertion did not know that.
                            '' DoReplace's single-match branch opens by comparing the
                            '' SELECTION against the find phrase and, when they differ,
                            '' moves to the nearest match and RETURNS WITHOUT
                            '' REPLACING. So the first press selects and the second
                            '' replaces -- which is what the bar does under a user's
                            '' hands and what this now does: Find Next, then Replace.
                            OnMenuCommand( 0, IDM_FINDNEXT, 0 )
                            Check "      Find Next selects the match", _
                                  (PsUCase(FindReplace_ActiveDoc()->GetSelText) = _
                                   PsUCase("ProbeAlpha"))
                            OnMenuCommand( 0, IDM_REPLACENEXT, 0 )
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            Check "      and after Replace it is gone", _
                                  (gFind.foundCount = 0), str(gFind.foundCount)

                            gFind.txtFind = DWSTRING("ProbeZeta")
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            Check "        replaced by the REPLACEMENT, not by nothing", _
                                  (gFind.foundCount = 1), str(gFind.foundCount)

                            '' UNDONE, and the undo is asserted rather than assumed.
                            '' DoReplace wraps its edits in SCI_BEGINUNDOACTION, so one
                            '' undo is the whole thing -- and the rest of this suite
                            '' reads the probe file after this scope.
                            if g_view <> 0 then
                                SciMsg( g_view->pSci, SCI_UNDO, 0, 0 )
                                SciMsg( g_view->pSci, SCI_SETSAVEPOINT, 0, 0 )
                            end if
                            gFind.txtFind = DWSTRING("ProbeAlpha")
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            Check "      and undo puts the buffer back", _
                                  (gFind.foundCount = 1), str(gFind.foundCount)

                            '' ---- A REPLACE THAT MATCHES NOTHING ----------------
                            '' THIS DOES NOT COVER THE foundCount GUARD, and it is
                            '' labelled that way because it looked like it did:
                            '' deleting the guard leaves this green. What it does cover
                            '' is the whole path -- a term with no matches must not
                            '' disturb the buffer, guard or no guard.
                            gFind.txtFind = DWSTRING("NoSuchTermAnywhereAtAll")
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            OnMenuCommand( 0, IDM_REPLACEALL, 0 )
                            gFind.txtFind = DWSTRING("ProbeAlpha")
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            Check "    a term that matches nothing disturbs nothing", _
                                  (gFind.foundCount = 1), str(gFind.foundCount)

                            gFind.txtFind = sWasF
                        end scope

                        '' ---- PRESERVE CASE, THROUGH THE HANDLER -----------------
                        '' Step 27 learned this the hard way four steps running: an
                        '' assertion that sets the flag directly never touches the
                        '' handler that is supposed to.
                        scope
                            dim as long iP = g_barReplace->pPreserve->FindItemByID( SHREPL_ID_PRESERVE )
                            Check "    Preserve Case is findable by id", (iP >= 0), str(iP)
                            gFind.nPreserve = 0
                            g_barReplace->pPreserve->SetSelected( iP, true )
                            ShellReplace_OnIcon( g_barReplace->pPreserve, iP, 0 )
                            Check "      and clicking it sets the flag the engine reads", _
                                  (gFind.nPreserve <> 0), str(gFind.nPreserve)

                            '' ---- AND THE MODEL PUSHES BACK INTO THE LATCH -------
                            '' The gap tiko avoided by painting from gFind and this
                            '' port re-opened by using PSICON_TOGGLE. A flag cleared
                            '' behind the bar's back -- which frmFindInProject.inc:3140
                            '' does to the Find bar's two -- must not leave the icon lit.
                            gFind.nPreserve = 0
                            ShellReplace_SyncToggle()
                            Check "      and clearing the flag behind its back unlatches it", _
                                  (g_barReplace->pPreserve->GetSelected( iP ) = false)
                            gFind.nMatchCase = 1
                            ShellFind_SyncToggles()
                            scope
                                dim as long iMC2 = g_barFind->pToggle->FindItemByID( SHFIND_ID_MATCHCASE )
                                Check "        and the Find bar's toggles sync the same way", _
                                      g_barFind->pToggle->GetSelected( iMC2 )
                            end scope
                            gFind.nMatchCase = 0
                            ShellFind_SyncToggles()
                        end scope

                        '' ---- THE CALLBACKS ARE INSTALLED ------------------------
                        Check "    the replace field's change and enter hooks are installed", _
                              (g_barReplace->pField->pfnChange <> 0) andalso _
                              (g_barReplace->pField->pfnEnter <> 0)
                        Check "      as are both icon strips'", _
                              (g_barReplace->pPreserve->pfnClick <> 0) andalso _
                              (g_barReplace->pActions->pfnClick <> 0)

                        ShellFind_SetVisible( false )
                        gFind.txtReplace = DWSTRING("")
                        gFind.foundCount = 0
                    end scope

                    '' ---- SELECTION AND SEEDING, 7c step 29 -----------------------
                    ''
                    '' The two halves step 27 deferred, and they arrive together
                    '' because the SEEDING RULE and its EXCEPTION cannot be separated:
                    '' porting the rule alone re-imports the case-only echo that step
                    '' 26 fixed by request.
                    scope
                        dim as long iSel = g_barFind->pToggle->FindItemByID( SHFIND_ID_SELECTION )
                        Check "  Selection is on the toggle strip", (iSel >= 0), str(iSel)

                        dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
                        Check "    and there is a document to select in", (pDoc <> 0)

                        '' ---- IT REFUSES TO LATCH WITH NOTHING TO BE WITHIN -------
                        '' The only toggle on either bar that can say no. The control
                        '' has ALREADY latched itself by the time the handler runs, so
                        '' this asserts the push-back as much as the flag.
                        ShellFind_SetVisible( false )
                        gFind.nSelection = 0
                        pDoc->RemoveMarkerHighlight()
                        pDoc->CurrentSelection.isInitialized = false
                        g_barFind->pToggle->SetSelected( iSel, true )
                        ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                        Check "    Selection refuses to arm with no multi-line selection", _
                              (gFind.nSelection = 0), str(gFind.nSelection)
                        Check "      and the icon does not stay lit", _
                              (g_barFind->pToggle->GetSelected( iSel ) = false)

                        '' ---- AND ARMS WHEN THERE IS ONE --------------------------
                        '' CurrentSelection is what the rule reads, not the live
                        '' selection: tiko's whole reason for capturing it is that the
                        '' live one is about to be overwritten by the search.
                        pDoc->CurrentSelection.startline    = 0
                        pDoc->CurrentSelection.endline      = 2
                        '' REAL POSITIONS, because arming now RESTORES the live
                        '' selection from these before laying the markers -- a
                        '' zero-length range would mark nothing and the assertion under
                        '' it would be back to passing on an empty highlight.
                        pDoc->CurrentSelection.startpos     = 0
                        pDoc->CurrentSelection.endpos       = _
                            SciMsg( pDoc->GetActiveScintillaPtr(), SCI_POSITIONFROMLINE, 3, 0 )
                        pDoc->CurrentSelection.isInitialized = true
                        g_barFind->pToggle->SetSelected( iSel, true )
                        ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                        '' `= true`, NOT `<> 0`. THE ENGINE COMPARES AGAINST true,
                        '' which is -1, and this assertion said `<> 0` while the code
                        '' wrote 1 -- so it passed against a flag the engine could not
                        '' read. The icon lit and the search still covered the whole
                        '' document. Assert the VALUE the reader tests, not a property
                        '' of it that the defect preserves.
                        Check "    and arms against a multi-line one", _
                              (gFind.nSelection = true), str(gFind.nSelection)
                        Check "      laying down a marker highlight", pDoc->HasMarkerHighlight
                        Check "      with the icon lit", _
                              g_barFind->pToggle->GetSelected( iSel )

                        '' ---- CLOSING DROPS IT ------------------------------------
                        '' tiko's bClosing branch. A reopened bar with Selection lit
                        '' against a marker range that no longer describes anything is
                        '' the defect this prevents, and this port would show it more
                        '' readily than tiko because the icon is a latch.
                        ShellFind_SetVisible( false )
                        Check "    closing the bar drops the Selection latch", _
                              (gFind.nSelection = 0), str(gFind.nSelection)
                        Check "      and the marker highlight with it", _
                              (pDoc->HasMarkerHighlight = false)
                        Check "        and forgets the captured selection", _
                              (pDoc->CurrentSelection.isInitialized = false)

                        '' ---- THE SEEDING ------------------------------------------
                        '' Driven the way the bar is: select a real match in the real
                        '' buffer, then OPEN, and the field must be a picture of it.
                        scope
                            dim as any ptr pSci = pDoc->GetActiveScintillaPtr()
                            gFind.txtFind = DWSTRING("ProbeAlpha")
                            gFind.nMatchCase = 0 : gFind.nWholeWord = 0
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            FindReplace_NextSelection( 0, true, true )
                            Check "    a match is selected in the buffer", _
                                  (pDoc->GetSelText = "ProbeAlpha"), pDoc->GetSelText

                            '' A DIFFERENT WORD IN THE BOX, so the reseed has something
                            '' to overwrite and "it was already right" cannot pass this.
                            gFind.txtFind = DWSTRING("Zzz")
                            g_barFind->pField->SetText( DWSTRING("Zzz") )
                            ShellFind_SetVisible( true )
                            Check "      and opening the bar seeds the field from it", _
                                  (gFind.txtFind = DWSTRING("ProbeAlpha")), gFind.txtFind.Utf8
                            Check "        in the FIELD, not only in the model", _
                                  (g_barFind->pField->GetText() = DWSTRING("ProbeAlpha")), _
                                  g_barFind->pField->GetText().Utf8

                            '' ---- THE EXCEPTION -----------------------------------
                            '' Type "probealpha", press Ctrl+H, and the field came back
                            '' "ProbeAlpha" -- the selection carries the DOCUMENT's
                            '' casing, not what was typed. Same word, same matches, the
                            '' user's own text replaced for no gain.
                            ShellFind_SetVisible( false )
                            SciMsg( pSci, SCI_SETSELECTIONSTART, 0, 0 )
                            SciMsg( pSci, SCI_SETSELECTIONEND, 0, 0 )
                            gFind.txtFind = DWSTRING("ProbeAlpha")
                            FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                            FindReplace_NextSelection( 0, true, true )
                            gFind.txtFind = DWSTRING("probealpha")
                            g_barFind->pField->SetText( DWSTRING("probealpha") )
                            ShellFind_SetVisible( true )
                            Check "    the same word in the document's case is NOT echoed back", _
                                  (gFind.txtFind.Utf8 = "probealpha"), gFind.txtFind.Utf8

                            '' ---- AND A GENUINELY DIFFERENT WORD STILL RESEEDS ----
                            '' The exception is "equal folded AND unequal exactly". An
                            '' assertion that only proved the skip would pass just as
                            '' well against a seeder that had been deleted.
                            ShellFind_SetVisible( false )
                            gFind.txtFind = DWSTRING("Qqq")
                            g_barFind->pField->SetText( DWSTRING("Qqq") )
                            ShellFind_SetVisible( true )
                            Check "      but a different word does", _
                                  (gFind.txtFind = DWSTRING("ProbeAlpha")), gFind.txtFind.Utf8

                            '' ---- REOPENING AN OPEN BAR SEEDS NOTHING -------------
                            '' Ctrl+H routes through Find to satisfy "Replace implies
                            '' Find", and without the guard it would reseed from
                            '' whatever the first search selected -- the exact gesture
                            '' the echo was reported against.
                            gFind.txtFind = DWSTRING("Typed")
                            ShellFind_SetVisible( true )
                            Check "    reopening an already-open bar seeds nothing", _
                                  (gFind.txtFind = DWSTRING("Typed")), gFind.txtFind.Utf8

                            '' ---- AN EMPTY SELECTION EMPTIES THE BOX --------------
                            '' By request, and the reason tiko's bSeedField flag is
                            '' gone: the box is a picture of the selection, so no
                            '' selection is an empty box rather than the last phrase.
                            ShellFind_SetVisible( false )
                            SciMsg( pSci, SCI_SETEMPTYSELECTION, 0, 0 )
                            gFind.txtFind = DWSTRING("Leftover")
                            ShellFind_SetVisible( true )
                            Check "    and no selection empties it rather than re-offering", _
                                  (PsLen(gFind.txtFind) = 0), gFind.txtFind.Utf8
                        end scope

                        '' ---- AND THE CAPTURE ITSELF, DRIVEN FOR REAL -------------
                        '' EVERY ASSERTION ABOVE SETS CurrentSelection BY HAND, so none
                        '' of them touched ShellFind_CaptureSelection -- gutting it left
                        '' the suite at 556/0. Fifth step running (22, 23, 24, 27, 29)
                        '' that a wiring went unasserted because the policy was driven
                        '' straight.
                        ''
                        '' This makes a REAL multi-line selection in the buffer, closes
                        '' the bar so nothing is captured, and opens it -- which is the
                        '' whole gesture: capture, then seed, then arm.
                        scope
                            ShellFind_SetVisible( false )
                            dim as any ptr pS2 = pDoc->GetActiveScintillaPtr()
                            SciMsg( pS2, SCI_SETSELECTIONSTART, 0, 0 )
                            SciMsg( pS2, SCI_SETSELECTIONEND, _
                                    SciMsg( pS2, SCI_POSITIONFROMLINE, 3, 0 ), 0 )
                            gFind.txtFind = DWSTRING("Leftover")
                            ShellFind_SetVisible( true )
                            Check "    opening captures the live selection", _
                                  pDoc->CurrentSelection.isInitialized
                            Check "      spanning the lines it actually spanned", _
                                  (pDoc->CurrentSelection.endline > _
                                   pDoc->CurrentSelection.startline), _
                                  str(pDoc->CurrentSelection.startline) & ".." & _
                                  str(pDoc->CurrentSelection.endline)
                            '' A MULTI-LINE SELECTION IS A RANGE, NOT A PHRASE: the box
                            '' clears and Selection ARMS ITSELF, which is the one place
                            '' the feature turns on without anybody clicking it.
                            Check "    a multi-line selection empties the box", _
                                  (PsLen(gFind.txtFind) = 0), gFind.txtFind.Utf8
                            Check "      and arms Selection on its own", _
                                  (gFind.nSelection = true), str(gFind.nSelection)
                            Check "        with the icon lit to match", _
                                  g_barFind->pToggle->GetSelected( iSel )
                            '' ---- AND THE SEARCH ACTUALLY SHRINKS ---------------
                            '' THE ASSERTION THIS STEP WAS MISSING, and the author's
                            '' report is what asked for it: "searching does not honor
                            '' selected area". Every check above is about the FLAG and
                            '' the ICON. None looked at what the ENGINE does.
                            ''
                            '' AND THE RESTRICTION IS THE MARKERS, NOT THE FLAG. I had
                            '' read the nSelection test in UpdateResultsFromCaret as the
                            '' mechanism; it is not. HighlightSearches narrows its target
                            '' range to First/LastMarkerHighlight, so the flag decides
                            '' whether markers get LAID and the markers do the work.
                            '' The first draft of this scope asserted five matches and
                            '' got two -- because the previous scope's markers were still
                            '' down and the search was already restricted. THE FIXTURE
                            '' WAS DIRTY; the engine was right.
                            scope
                                dim as long nWhole, nInSel
                                pDoc->RemoveMarkerHighlight()
                                gFind.nSelection = false
                                gFind.txtFind = DWSTRING("Probe")
                                gFind.nMatchCase = 0 : gFind.nWholeWord = 0
                                FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                                nWhole = gFind.foundCount
                                Check "    unrestricted, the probe file holds five Probes", _
                                      (nWhole = 5), str(nWhole)

                                '' DRIVEN THROUGH THE ICON, over a LIVE selection made
                                '' while the bar is already open -- which is the gesture
                                '' that was broken: the capture runs at SHOW time, so
                                '' selecting afterwards was invisible and the icon simply
                                '' refused to arm.
                                '' THE CAPTURE IS THROWN AWAY FIRST, and without that
                                '' this assertion was vacuous: the previous scope had
                                '' left a multi-line CurrentSelection behind, so the rule
                                '' armed from THAT and disabling the re-capture changed
                                '' nothing -- 566/0 with the fix reverted. This is what
                                '' the real gesture looks like: the bar opened over no
                                '' selection, so nothing worth having was captured.
                                pDoc->CurrentSelection.isInitialized = false
                                SciMsg( pS2, SCI_SETSELECTIONSTART, 0, 0 )
                                SciMsg( pS2, SCI_SETSELECTIONEND, _
                                        SciMsg( pS2, SCI_POSITIONFROMLINE, 2, 0 ), 0 )
                                g_barFind->pToggle->SetSelected( iSel, true )
                                ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                                Check "    selecting AFTER the bar opened still arms it", _
                                      (gFind.nSelection = true), str(gFind.nSelection)
                                Check "      with markers actually down", _
                                      pDoc->HasMarkerHighlight
                                nInSel = gFind.foundCount
                                Check "      and the search finds FEWER of them", _
                                      (nInSel < nWhole) andalso (nInSel > 0), _
                                      str(nInSel) & "/" & str(nWhole)

                                '' AND OFF AGAIN RESTORES THE WHOLE DOCUMENT, which is
                                '' the half that proves the number above was the range
                                '' talking rather than the buffer having changed.
                                g_barFind->pToggle->SetSelected( iSel, false )
                                ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                                Check "    and turning it off searches all five again", _
                                      (gFind.foundCount = nWhole), str(gFind.foundCount)

                                '' ---- THE FOUR RULES tiko HAD TO LEARN BY HAND -----
                                '' Every one of these was reported against tiko by the
                                '' author and none was visible to a headless suite until
                                '' it was written to drive the GESTURE rather than the
                                '' flag. They are asserted here so the port does not have
                                '' to learn them a second time.

                                '' 1. OPEN FIRST, SELECT AFTERWARDS. The capture used to
                                '' return early whenever it was initialized, so opening
                                '' over nothing filled it with an EMPTY range that
                                '' selecting lines afterwards could never get past.
                                ShellFind_SetVisible( false )
                                SciMsg( pS2, SCI_SETEMPTYSELECTION, 0, 0 )
                                ShellFind_SetVisible( true )
                                Check "    opening over nothing leaves it dark", _
                                      (gFind.nSelection = 0), str(gFind.nSelection)
                                SciMsg( pS2, SCI_SETSELECTIONSTART, 0, 0 )
                                SciMsg( pS2, SCI_SETSELECTIONEND, _
                                        SciMsg( pS2, SCI_POSITIONFROMLINE, 2, 0 ), 0 )
                                g_barFind->pToggle->SetSelected( iSel, true )
                                ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                                Check "      and selecting AFTER it opened still arms it", _
                                      (gFind.nSelection = true), str(gFind.nSelection)

                                '' 2. A SEARCH MUST NOT STEAL THE RANGE. After any search
                                '' the live selection is the LAST MATCH, one line -- and a
                                '' capture that took it would shrink the armed range to
                                '' that match. This is the clause the guard exists FOR,
                                '' and in tiko removing it left the suite green.
                                gFind.txtFind = DWSTRING("Probe")
                                FindReplace_HighlightSearches( ShellFind_OccurrenceColour() )
                                FindReplace_NextSelection( 0, true, true )
                                ShellFind_CaptureSelection()
                                Check "    a search does not steal the captured range", _
                                      ((pDoc->CurrentSelection.endline - _
                                        pDoc->CurrentSelection.startline) > 0), _
                                      str(pDoc->CurrentSelection.startline) & ".." & _
                                      str(pDoc->CurrentSelection.endline)

                                '' 3. DISARMING DROPS THE TEXT SELECTION, by request.
                                SciMsg( pS2, SCI_SETSELECTIONSTART, 0, 0 )
                                SciMsg( pS2, SCI_SETSELECTIONEND, _
                                        SciMsg( pS2, SCI_POSITIONFROMLINE, 2, 0 ), 0 )
                                g_barFind->pToggle->SetSelected( iSel, false )
                                ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                                Check "    disarming drops the text selection with it", _
                                      (SciMsg( pS2, SCI_GETSELECTIONSTART, 0, 0 ) = _
                                       SciMsg( pS2, SCI_GETSELECTIONEND, 0, 0 )), _
                                      str(SciMsg( pS2, SCI_GETSELECTIONSTART, 0, 0 )) & ".." & _
                                      str(SciMsg( pS2, SCI_GETSELECTIONEND, 0, 0 ))

                                '' 4. BUT A REFUSAL MUST NOT. It lands in the same branch,
                                '' and collapsing a selection the user just made because
                                '' the button declined to arm is the opposite of the ask.
                                ShellFind_SetVisible( false )
                                ShellFind_SetVisible( true )
                                SciMsg( pS2, SCI_SETSELECTIONSTART, 0, 0 )
                                SciMsg( pS2, SCI_SETSELECTIONEND, 4, 0 )
                                pDoc->CurrentSelection.isInitialized = false
                                pDoc->RemoveMarkerHighlight()
                                gFind.nSelection = false
                                g_barFind->pToggle->SetSelected( iSel, true )
                                ShellFind_OnIcon( g_barFind->pToggle, iSel, 0 )
                                Check "    a REFUSAL leaves the text selection alone", _
                                      (SciMsg( pS2, SCI_GETSELECTIONEND, 0, 0 ) > _
                                       SciMsg( pS2, SCI_GETSELECTIONSTART, 0, 0 )), _
                                      str(SciMsg( pS2, SCI_GETSELECTIONSTART, 0, 0 )) & ".." & _
                                      str(SciMsg( pS2, SCI_GETSELECTIONEND, 0, 0 ))
                                gFind.nSelection = false
                                pDoc->RemoveMarkerHighlight()
                            end scope

                            ShellFind_SetVisible( false )
                        end scope

                        '' ---- OPENING REPLACE CLEARS BOTH COPIES ------------------
                        '' tiko blanks the BOX only, and programmatic setters are
                        '' silent -- so gFind.txtReplace keeps the previous replacement
                        '' while the field shows empty. A deliberate divergence.
                        ShellFind_SetVisible( false )
                        gFind.txtReplace = DWSTRING("Stale")
                        ShellReplace_SetVisible( true )
                        Check "    opening Replace clears the replacement in the model too", _
                              (PsLen(gFind.txtReplace) = 0), gFind.txtReplace.Utf8
                        Check "      and in the field", _
                              (PsLen(g_barReplace->pField->GetText()) = 0)

                        ShellFind_SetVisible( false )
                        gFind.foundCount = 0
                    end scope

                        '' A NAME AND A LINE, because a count alone would pass on two
                        '' entries that say nothing useful. The line is what commit 4's
                        '' click will jump to.
                        '' NAMED, ALL THREE. Built as one string and searched, because
                        '' EnumProcsInFile's order is the database's and not this test's to
                        '' assume -- an assertion that indexed rs(0) and rs(1) by position
                        '' would be asserting the enumeration order as a side effect.
                        if nProcs = 3 then
                            dim as DWSTRING sAll
                            for p as long = 0 to nProcs - 1
                                sAll = sAll & "|" & gSymDb.QualifiedName( rs(p) )
                            next
                            Check "    named, all of them", _
                                  (PsInStr(PsUCase(sAll), "PROBEALPHA") > 0) andalso _
                                  (PsInStr(PsUCase(sAll), "PROBEBETA") > 0) andalso _
                                  (PsInStr(PsUCase(sAll), "PROBEGAMMA") > 0), sAll.Utf8
                        end if

                        '' ---- THE FUNCTIONS PANEL ------------------------------------
                        '' The second pane, and the reason the scanner exists. It reads
                        '' gSymDb -- never the scanner, never the parser -- which is what
                        '' let it be ported without threading.
                        scope
                            '' A BOOKMARK FIRST, so that "switch back" has something to
                            '' show. Without it that assertion compared an empty list to an
                            '' empty list -- the assertions above this point end with the
                            '' document's bookmarks cleared, and the first version of this
                            '' block failed on a count of 0 that was entirely correct.
                            dim as clsDocument ptr pDocF = ShellTabs_CurrentDoc()
                            pDocF->ToggleBookmark( 1 )

                            ShellPanel_SetMode( SHPANEL_FUNCTIONS )
                            Check "  the panel switches to functions", _
                                  (g_panelMode = SHPANEL_FUNCTIONS)
                            '' One header for the file, one row per procedure with a body.
                            Check "    and lists both procedures under their file", _
                                  (g_panel->GetCount() = 3), str(g_panel->GetCount())
                            Check "      the first row being the file", g_panel->IsHeader(0)

                            '' SORTED BY NAME, which is tiko's order. ProbeBeta is declared
                            '' second in the file and sorts second here too, so the check
                            '' below would pass on file order alone -- it is the pair of
                            '' names being right that it actually asserts.
                            Check "      Alpha before Beta", _
                                  (PsInStr(PsUCase(g_panel->GetText(1)), "PROBEALPHA") > 0) andalso _
                                  (PsInStr(PsUCase(g_panel->GetText(2)), "PROBEBETA") > 0), _
                                  g_panel->GetText(1).Utf8 & " | " & g_panel->GetText(2).Utf8

                            '' THE ROWS CARRY THE SAME PACKED SLOT the bookmarks use, which
                            '' is what lets ShellPanel_GotoRow be reused untouched. The line
                            '' is 0-BASED here and 1-based in the database.
                            ''
                            '' 3, BECAUSE THE PROBE FILE'S LAYOUT IS LOAD-BEARING: a comment,
                            '' the #include added in step 6, a blank, then ProbeAlpha. Two
                            '' assertions carry line numbers derived from it -- this one and
                            '' the click below -- and both moved when the #include was added.
                            '' Anything editing that file has to move them again.
                            Check "      and a 0-based line in its own slot", _
                                  (ShellPanel_LineOf(1) = 3), _
                                  str(ShellPanel_LineOf(1))
                            Check "        alongside the tab it belongs to", _
                                  (ShellPanel_TabOf(1) = idx1), _
                                  str(ShellPanel_TabOf(1))

                            '' SWITCHING BACK RESTORES THE OTHER LIST rather than leaving
                            '' the functions on screen under a bookmarks mode.
                            ShellPanel_SetMode( SHPANEL_BOOKMARKS )
                            Check "    switching back shows bookmarks again", _
                                  (g_panelMode = SHPANEL_BOOKMARKS) andalso _
                                  (g_panel->GetCount() = 2), str(g_panel->GetCount())

                            '' AND THE COMMANDS DO IT, which is the only thing that proves
                            '' the two menu ids are wired to anything.
                            OnMenuCommand( 0, IDM_FUNCTIONLIST, 0 )
                            Check "    IDM_FUNCTIONLIST switches the pane", _
                                  (g_panelMode = SHPANEL_FUNCTIONS)
                            OnMenuCommand( 0, IDM_BOOKMARKSLIST, 0 )
                            Check "    IDM_BOOKMARKSLIST switches it back", _
                                  (g_panelMode = SHPANEL_BOOKMARKS)

                            '' ---- CLICKING A FUNCTION GOES THERE ---------------------
                            '' ShellPanel_GotoRow is MODE-AGNOSTIC and this is the
                            '' assertion for that: it reads the two data slots, and the
                            '' functions loader fills the same (tab, line) the bookmarks
                            '' loader does. No second handler, no second callback -- the
                            '' whole of commit 4's click support is that the two loaders
                            '' agree on what a row carries.
                            ShellPanel_SetMode( SHPANEL_FUNCTIONS )
                            scope
                                '' Row 2 is ProbeBeta. Scintilla line 7 -- see the note on
                                '' the packed-slot assertion above: the probe file's layout
                                '' is load-bearing and step 6's #include moved everything
                                '' below it down by two.
                                dim as boolean bWent = ShellPanel_GotoRow( 2 )
                                dim as long nAt = SciMsg( g_view->pSci, SCI_LINEFROMPOSITION, _
                                            SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0), 0 )
                                Check "    clicking a function jumps to its body", _
                                      bWent andalso (nAt = 7), str(nAt) & " wanted 7"

                                '' A HEADER IS STILL A FILENAME. Same guard, same reason,
                                '' and it has to hold in both modes because one handler
                                '' serves both.
                                Check "      and its file header still goes nowhere", _
                                      (ShellPanel_GotoRow(0) = false)
                            end scope


                            '' ---- THE EXPLORER PANE, 7c step 19 ----------------------
                            ''
                            '' The pane's SHAPE, not its pixels. Everything below is
                            '' reachable without a mouse because the loader and the row
                            '' scheme are separate from the painter -- which is the same
                            '' split the other two panes already have.
                            OnMenuCommand( 0, IDM_VIEWEXPLORER, 0 )
                            Check "  IDM_VIEWEXPLORER switches to the explorer", _
                                  (g_panelMode = SHPANEL_EXPLORER)

                            '' SIX CATEGORIES, FIVE OF THEM DISPLAYED. CATINDEX_FILES is
                            '' deliberately outside the loop's range, so the count is
                            '' ubound(Cat) - CATINDEX_MAIN + 1.
                            ''
                            '' THIS IS THE ASSERTION THAT WOULD HAVE CAUGHT AN EMPTY Cat,
                            '' and it is the reason it is written as a count rather than
                            '' as "more than zero rows": ubound(Cat) was -1 in this binary
                            '' until SetCategoryDefaults moved into the app layer, and the
                            '' pane rendered NOTHING with no error of any kind.
                            scope
                                dim as long nRoots = 0
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) = EXPKIND_ROOT then nRoots += 1
                                next
                                Check "    one root group per displayed category", _
                                      (nRoots = ubound(gConfig.Cat) - CATINDEX_MAIN + 1), _
                                      str(nRoots) & " of " & str(ubound(gConfig.Cat) + 1)
                            end scope

                            '' NO ROW IS UNTAGGED. EXPKIND_NONE is what an un-tagged row
                            '' silently reads as, and slot 1 would then be read as a file
                            '' index -- so "every row has a kind" is the assertion that
                            '' makes the scheme safe rather than merely intended.
                            scope
                                dim as long nUntagged = 0
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) = EXPKIND_NONE then nUntagged += 1
                                next
                                Check "    and every row carries a kind", (nUntagged = 0), _
                                      str(nUntagged) & " untagged"
                            end scope

                            '' A ROOT GROUP IS NOT SELECTABLE, which is what keeps it
                            '' undraggable -- drag eligibility is computed from
                            '' selectability, and a header would have got that for free
                            '' where a plain parent does not.
                            scope
                                dim as long nRoot = -1
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) = EXPKIND_ROOT then nRoot = r : exit for
                                next
                                Check "    a root group is not selectable", _
                                      (nRoot >= 0) andalso _
                                      (g_panel->GetRowSelectable(nRoot) = false), str(nRoot)
                                '' AND IT IS NOT A HEADER, which is the half that matters:
                                '' IsHeader is GotoRow's other guard, so a root that WAS a
                                '' header would be refused for the wrong reason and this
                                '' assertion would pass while the kind test did nothing.
                                Check "      and is a parent rather than a header", _
                                      (nRoot >= 0) andalso (g_panel->IsHeader(nRoot) = false)
                            end scope

                            '' ---- THE OPEN FILE IS IN THE TREE, EXACTLY ONCE ---------
                            scope
                                dim as long nHit = -1
                                dim as long nSeen = 0
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) <> EXPKIND_FILE then continue for
                                    if PsUCase(ShellPanel_PathOf(r)) <> PsUCase(wszFile) then continue for
                                    nSeen += 1
                                    if nHit < 0 then nHit = r
                                next
                                Check "    the open file appears once as a file row", _
                                      (nSeen = 1), str(nSeen)

                                '' SLOT 2 IS A KIND HERE, NOT A LINE, and this is the
                                '' assertion for the mode guard inside ShellPanel_LineOf.
                                '' Without it this reads EXPKIND_FILE -- which is 3 -- and
                                '' every click on a file row throws the caret to line 3.
                                Check "      and its slot 2 reads as a line of 0, not a kind", _
                                      (nHit >= 0) andalso (ShellPanel_LineOf(nHit) = 0), _
                                      str(ShellPanel_LineOf(nHit))

                                '' A FOLDER OR A GROUP IS NOT A PLACE. Three of the four
                                '' kinds must be refused, and NONE of them is a header --
                                '' so IsHeader alone lets all three through and slot 1
                                '' would be read as a file index. A category index of 2
                                '' would have opened g_panelFiles(2): a real file, and the
                                '' wrong one.
                                dim as long nRoot2 = -1
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) = EXPKIND_ROOT then nRoot2 = r : exit for
                                next
                                Check "      a group row goes nowhere", _
                                      (ShellPanel_GotoRow(nRoot2) = false)
                                Check "        but a file row does", _
                                      (nHit >= 0) andalso ShellPanel_GotoRow(nHit)

                                '' SELECTION FOLLOWS THE FILE, which is what a tab switch
                                '' drives. Asserted through the same entry point
                                '' ShellTabs_Show calls, with a row number to compare
                                '' against -- the only part of a tab click reachable here.
                                '' ---- THE RESULT IS TAKEN INTO A LOCAL FIRST, and
                                '' that is not tidiness. Written inline, this read
                                ''     Check "...", SelectPath(f) andalso (GetCurSel() = nHit), _
                                ''           str(GetCurSel()) & " wanted " & str(nHit)
                                '' and printed "ok ... (-1 wanted 2)" -- a PASSING
                                '' assertion whose own evidence contradicted it. fbc does
                                '' not promise the order in which it evaluates arguments,
                                '' and here it built the MESSAGE before running the call
                                '' that changes what the message reports. The assertion was
                                '' right and unreadable; anyone reading the log would have
                                '' gone looking for a bug that was not there.
                                dim as boolean bSel1 = ShellExplorer_SelectPath( wszFile )
                                dim as long    nSel1 = g_panel->GetCurSel()
                                Check "    SelectPath lands on that row", _
                                      bSel1 andalso (nSel1 = nHit), _
                                      str(nSel1) & " wanted " & str(nHit)
                                '' Idempotent, and tiko checks this first for the same
                                '' reason: re-selecting the row already selected must not
                                '' scroll the pane.
                                dim as boolean bSel2 = ShellExplorer_SelectPath( wszFile )
                                dim as long    nSel2 = g_panel->GetCurSel()
                                Check "      and again, without moving anything", _
                                      bSel2 andalso (nSel2 = nHit), _
                                      str(nSel2) & " wanted " & str(nHit)
                                Check "      while a path not in the workspace finds nothing", _
                                      (ShellExplorer_SelectPath(DWSTRING("C:/nowhere/x.bas")) = false)

                                '' ---- A ROW INSIDE A COLLAPSED SUBTREE IS REFUSED ----
                                ''
                                '' tiko's behaviour, and it looks like a refusal until you
                                '' see the alternative: selecting a row the user cannot see
                                '' scrolls the pane to a blank place, and expanding the
                                '' tree under them to satisfy a tab switch is worse.
                                ''
                                '' WRITTEN BECAUSE REVERTING THE GUARD CHANGED NOTHING.
                                '' Deleting `if IsCollapsed then return false` left the
                                '' suite at 411/0 -- no fixture here had ever collapsed
                                '' anything, so the guard was carried by prose alone. That
                                '' is this port's oldest finding wearing a new costume.
                                scope
                                    dim as long nOwner = g_panel->GetParent( nHit )
                                    do while (nOwner >= 0) andalso (g_panel->GetParent(nOwner) >= 0)
                                        nOwner = g_panel->GetParent( nOwner )
                                    loop
                                    Check "      the file sits under a collapsible group", _
                                          (nOwner >= 0) andalso g_panel->CanCollapse(nOwner), _
                                          str(nOwner)
                                    g_panel->CollapseRow( nOwner )
                                    Check "        and collapsing it makes SelectPath refuse", _
                                          (ShellExplorer_SelectPath( wszFile ) = false)
                                    g_panel->ExpandRow( nOwner )
                                    Check "        expanding it again restores the hit", _
                                          ShellExplorer_SelectPath( wszFile )
                                end scope
                            end scope


                            '' ---- THE FOLDER COMMANDS, 7c step 21 -------------------
                            ''
                            '' Driven by ROW rather than by a click, which is the only
                            '' part of a context menu reachable without a mouse -- the
                            '' same split ShellPanel_GotoRow already has.
                            ''
                            '' THE MENU ITSELF IS NOT ASSERTED and cannot be: OpenRoot
                            '' needs a compositor. What is asserted is everything the
                            '' menu would do once it closed.
                            ShellPanel_SetMode( SHPANEL_EXPLORER )
                            scope
                                '' A category that ALLOWS folders. Main and Resource hold
                                '' one file each by construction and refuse -- which is
                                '' itself asserted below, because "New Folder did nothing"
                                '' and "New Folder is not offered here" look identical
                                '' from outside.
                                dim as long nGroup = -1
                                dim as long nCat   = -1
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) <> EXPKIND_ROOT then continue for
                                    dim as long ci = cast(long, g_panel->GetItemData(r))
                                    if ProjectFolders_CatAllowsFolders( ci ) then
                                        nGroup = r : nCat = ci : exit for
                                    end if
                                next
                                Check "  a group that allows folders exists", _
                                      (nGroup >= 0) andalso (nCat >= 0), str(nGroup)

                                '' ---- AND ONE THAT DOES NOT, WHICH HAS TO REFUSE -----
                                '' Main and Resource hold one file each by construction,
                                '' and CATINDEX_FILES is displayed nowhere. Without this
                                '' the CatAllowsFolders guard is unasserted -- removing it
                                '' left the suite GREEN, because every other assertion here
                                '' picks a group that allows folders anyway.
                                ''
                                '' "New Folder did nothing" and "New Folder is not offered
                                '' here" look identical from outside, which is exactly why
                                '' the refusal needs pinning rather than trusting.
                                scope
                                    dim as long nNo = -1
                                    for r as long = 0 to g_panel->GetCount() - 1
                                        if ShellPanel_KindOf(r) <> EXPKIND_ROOT then continue for
                                        dim as long ci = cast(long, g_panel->GetItemData(r))
                                        if ProjectFolders_CatAllowsFolders( ci ) = false then
                                            nNo = r : exit for
                                        end if
                                    next
                                    Check "    a group that forbids folders exists too", _
                                          (nNo >= 0), str(nNo)
                                    dim as long nB = ProjectFolders_Count()
                                    Check "      and New Folder is refused there", _
                                          (ShellExplorer_NewFolder( nNo ) = -1)
                                    Check "        changing nothing", _
                                          (ProjectFolders_Count() = nB), str(ProjectFolders_Count())
                                end scope

                                dim as long nWas = ProjectFolders_Count()
                                dim as long nNew = ShellExplorer_NewFolder( nGroup )
                                Check "    New Folder makes one", (nNew >= 0), str(nNew)
                                Check "      and the table grew by exactly one", _
                                      (ProjectFolders_Count() = nWas + 1), _
                                      str(ProjectFolders_Count()) & " was " & str(nWas)
                                Check "      under the group it was asked on", _
                                      (nNew >= 0) andalso _
                                      (gProjectFolders(nNew).catIndex = nCat), str(nCat)

                                '' A SECOND ONE MUST NOT COLLIDE. tiko's uniquifier matters
                                '' more here than there: tiko opens an editor immediately
                                '' so its generated name is a placeholder, and this binary
                                '' has no editor, so the name it picks is the name that
                                '' stays. Without the loop, ProjectFolders_Add would refuse
                                '' the duplicate and "New Folder" would silently do nothing
                                '' the second time.
                                dim as long nNew2 = ShellExplorer_NewFolder( nGroup )
                                Check "    a second one gets its own name", _
                                      (nNew2 >= 0) andalso (nNew2 <> nNew), str(nNew2)
                                Check "      so the table grew again", _
                                      (ProjectFolders_Count() = nWas + 2), _
                                      str(ProjectFolders_Count())

                                '' THE NEW ROW IS SELECTED, which is what tiko does before
                                '' opening its editor -- and is the only feedback this
                                '' binary gives that anything happened.
                                scope
                                    dim as long nSel = g_panel->GetCurSel()
                                    Check "    and the new row is selected", _
                                          (nSel >= 0) andalso _
                                          (ShellPanel_KindOf(nSel) = EXPKIND_FOLDER) andalso _
                                          (cast(long, g_panel->GetItemData(nSel)) = nNew2), _
                                          str(nSel)
                                end scope

                                '' ---- DELETE IS A DISSOLVE ---------------------------
                                '' Nothing is destroyed: children re-attach to the parent.
                                '' That is why tiko asks for no confirmation, and it is
                                '' why the count falls by exactly one rather than by a
                                '' subtree.
                                dim as long nRow2 = -1
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) <> EXPKIND_FOLDER then continue for
                                    if cast(long, g_panel->GetItemData(r)) = nNew2 then nRow2 = r : exit for
                                next
                                Check "    the second folder has a row", (nRow2 >= 0), str(nRow2)
                                Check "      and deleting it succeeds", _
                                      ShellExplorer_DeleteFolder( nRow2 )
                                Check "        leaving the table one shorter", _
                                      (ProjectFolders_Count() = nWas + 1), _
                                      str(ProjectFolders_Count())

                                '' A FILE ROW IS NOT A FOLDER. DeleteFolder reads slot 1 as
                                '' a folder-table index, and on a file row that index is a
                                '' g_panelFiles index -- a small number that would name a
                                '' real and entirely unrelated folder.
                                scope
                                    dim as long nFileRow = -1
                                    for r as long = 0 to g_panel->GetCount() - 1
                                        if ShellPanel_KindOf(r) = EXPKIND_FILE then nFileRow = r : exit for
                                    next
                                    if nFileRow >= 0 then
                                        '' THE LOAD-BEARING GUARD, asserted directly.
                                        '' DeleteFolder tests the kind first, but removing
                                        '' that test leaves the suite GREEN -- proved by
                                        '' reverting it -- because FolderPathFromRow
                                        '' answers -1 for every kind it does not handle and
                                        '' the catIndex test catches them all. So the thing
                                        '' worth pinning is THAT, not the early-out.
                                        dim as long ciFile = 0
                                        ShellExplorer_FolderPathFromRow( nFileRow, ciFile )
                                        Check "    a file row denotes no category", _
                                              (ciFile = -1), str(ciFile)
                                        dim as long nBefore = ProjectFolders_Count()
                                        Check "      so deleting a FILE row is refused", _
                                              (ShellExplorer_DeleteFolder( nFileRow ) = false)
                                        Check "      and changes nothing", _
                                              (ProjectFolders_Count() = nBefore), _
                                              str(ProjectFolders_Count())
                                    end if
                                end scope

                                '' ---- FOLDER RENAME, 7c step 23 ------------------------
                                ''
                                '' The editor itself is PsPlatform's and asserted there.
                                '' What is asserted HERE is the Explorer's POLICY: which
                                '' rows may be renamed, and which names are accepted.
                                scope
                                    dim as long nF = -1
                                    for r as long = 0 to g_panel->GetCount() - 1
                                        if ShellPanel_KindOf(r) <> EXPKIND_FOLDER then continue for
                                        if cast(long, g_panel->GetItemData(r)) = nNew then nF = r : exit for
                                    next
                                    Check "    the first folder still has a row", (nF >= 0), str(nF)

                                    '' ---- WHO MAY BE RENAMED -------------------------
                                    Check "      a folder row may be renamed", _
                                          ShellExplorer_OnBeginEdit( g_panel, nF, 0 )
                                    Check "        a group row may not", _
                                          (ShellExplorer_OnBeginEdit( g_panel, nGroup, 0 ) = false)
                                    scope
                                        dim as long nFile2 = -1
                                        for r as long = 0 to g_panel->GetCount() - 1
                                            if ShellPanel_KindOf(r) = EXPKIND_FILE then nFile2 = r : exit for
                                        next
                                        if nFile2 >= 0 then
                                            Check "        nor a file row", _
                                                  (ShellExplorer_OnBeginEdit( g_panel, nFile2, 0 ) = false)
                                        end if
                                    end scope

                                    '' ---- WHICH NAMES ARE ACCEPTED -------------------
                                    '' Driven through the end-edit callback, which is where
                                    '' the policy lives and is reachable without a caret.
                                    dim as DWSTRING sWas = gProjectFolders(nNew).wszPath
                                    Check "      an empty name is refused", _
                                          (ShellExplorer_OnEndEdit( g_panel, nF, DWSTRING(""), 0 ) = false)
                                    Check "      a name with a separator is refused", _
                                          (ShellExplorer_OnEndEdit( g_panel, nF, DWSTRING("a/b"), 0 ) = false)
                                    Check "        and the folder is untouched by either", _
                                          (gProjectFolders(nNew).wszPath = sWas), _
                                          gProjectFolders(nNew).wszPath.Utf8

                                    '' A REAL RENAME. The table is the truth, so the path is
                                    '' what changes -- the row's caption is rebuilt from it.
                                    Check "      a good name is accepted", _
                                          ShellExplorer_OnEndEdit( g_panel, nF, DWSTRING("Renamed"), 0 )
                                    Check "        and the table carries it", _
                                          (ProjectFolders_Find( nCat, DWSTRING("Renamed") ) >= 0), _
                                          gProjectFolders(nNew).wszPath.Utf8

                                    '' ---- CASE-ONLY IS NOT A COLLISION ---------------
                                    '' tiko's trap, ported with its comment: without the
                                    '' early return the sibling test finds the folder ITSELF
                                    '' and every attempt to fix a capital letter is refused.
                                    scope
                                        dim as long nR = -1
                                        for r as long = 0 to g_panel->GetCount() - 1
                                            if ShellPanel_KindOf(r) <> EXPKIND_FOLDER then continue for
                                            if PsUCase(g_panel->GetText(r)) = PsUCase(DWSTRING("Renamed")) then nR = r : exit for
                                        next
                                        Check "      the renamed folder has a row", (nR >= 0), str(nR)
                                        Check "        and a case-only change is ACCEPTED", _
                                              ShellExplorer_OnEndEdit( g_panel, nR, DWSTRING("RENAMED"), 0 )
                                    end scope
                                end scope

                                '' ---- THE ACTION ICONS, 7c step 24 ---------------------
                                ''
                                '' The ARITHMETIC and the HIT TEST, which are pure and
                                '' assertable; where they land on screen is not.
                                ''
                                '' Both come from ONE function called by the painter and the
                                '' hit test alike -- tiko's rule, and the failure it prevents
                                '' is an icon drawn one place and clicked another, which
                                '' reads as the button not working rather than as a bug.
                                scope
                                    '' ---- THE PANE NEEDS A RECTANGLE, and until this line
                                    '' it did not have one. Shell_LayoutAll runs much later
                                    '' in this suite, so g_panel's bounds are 0x0 here --
                                    '' and RowRect answers FALSE with the rect ZEROED for a
                                    '' row it cannot place, which is the only reason the
                                    '' icon arithmetic looked wrong.
                                    ''
                                    '' Restored at the end of the scope: the geometry block
                                    '' below drives the real layout and would otherwise be
                                    '' asserting against a rectangle this fixture invented.
                                    dim as PsRect rcPanelWas = g_panel->bounds
                                    g_panel->SetBounds( PsRc(0, 0, 400, 600) )

                                    '' ---- AND CLOSE ANY OPEN EDITOR FIRST -------------
                                    '' NewFolder ends by opening one (step 23), and the
                                    '' folder-commands block above ran it twice -- so an
                                    '' editor is open when this scope starts. The first
                                    '' EnsureVisible then SCROLLS, the scroll commits the
                                    '' edit, the commit calls ShellExplorer_Load, and the
                                    '' rebuild lands in the middle of a block that had
                                    '' already resolved its row indices.
                                    ''
                                    '' Cancelled rather than committed: this fixture has no
                                    '' business renaming anything, and the block below wants
                                    '' the tree it measured.
                                    if g_panel->IsEditing() then g_panel->EndEdit( false )
                                    g_panel->Layout()

                                    dim as PsRect rcRow, rcAdd, rcRen, rcDel
                                    dim as long nFolderRow = -1
                                    for r as long = 0 to g_panel->GetCount() - 1
                                        if ShellPanel_KindOf(r) = EXPKIND_FOLDER then nFolderRow = r : exit for
                                    next
                                    Check "    a folder row exists to test icons on", _
                                          (nFolderRow >= 0), str(nFolderRow)

                                    '' ---- A FOLDER OFFERS ALL THREE ------------------
                                    '' THE ROW HAS TO BE ON SCREEN, and the first draft of
                                    '' this block did not check. RowRect answers FALSE for a
                                    '' row outside the viewport and leaves the rect ZEROED,
                                    '' so IconRects laid its icons out from a right edge of
                                    '' 0 -- at negative x -- and the hit test, which calls
                                    '' RowRect again, found nothing at all.
                                    ''
                                    '' Three assertions failed and TWO PASSED ON THE ZEROED
                                    '' RECT: "offers all three" and "in reading order" are
                                    '' about widths and relative order, and both hold
                                    '' perfectly at the wrong place. Relations again.
                                    g_panel->EnsureVisible( nFolderRow )
                                    Check "      the folder row is on screen", _
                                          g_panel->RowRect( nFolderRow, @rcRow ), _
                                          str(rcRow.x) & "," & str(rcRow.y) & _
                                          " " & str(rcRow.w) & "x" & str(rcRow.h)
                                    ShellExplorer_IconRects( nFolderRow, rcRow, rcAdd, rcRen, rcDel )
                                    Check "      a folder offers all three", _
                                          (rcAdd.w > 0) andalso (rcRen.w > 0) andalso (rcDel.w > 0)
                                    '' READING ORDER IS Add, Rename, Delete -- laid out from
                                    '' the RIGHT inwards, so this is what says the order did
                                    '' not come out mirrored.
                                    Check "        in reading order, left to right", _
                                          (rcAdd.x < rcRen.x) andalso (rcRen.x < rcDel.x), _
                                          str(rcAdd.x) & "," & str(rcRen.x) & "," & str(rcDel.x)
                                    Check "        and all inside the row", _
                                          (rcAdd.x >= rcRow.x) andalso _
                                          (rcDel.x + rcDel.w <= rcRow.x + rcRow.w)

                                    '' ---- A GROUP OFFERS ADD ONLY --------------------
                                    '' Its caption comes from gConfig.Cat and its existence
                                    '' is what every file's ProjectFiletype names, so it can
                                    '' be added to and never renamed or deleted.
                                    g_panel->EnsureVisible( nGroup )
                                    Check "      the group row is on screen too", _
                                          g_panel->RowRect( nGroup, @rcRow )
                                    ShellExplorer_IconRects( nGroup, rcRow, rcAdd, rcRen, rcDel )
                                    Check "      a group offers Add and nothing else", _
                                          (rcAdd.w > 0) andalso (rcRen.w = 0) andalso (rcDel.w = 0)
                                    '' AND IT SITS AT THE MARGIN, not three columns in where
                                    '' a folder's Add is. Each row right-aligns its OWN set;
                                    '' tiko chose that because a ragged left edge across rows
                                    '' with different counts reads worse than a straight
                                    '' right one, and this is the assertion for it.
                                    Check "        right-aligned, at the margin", _
                                          (rcAdd.x + rcAdd.w > rcRow.x + rcRow.w - _
                                           PsScaleBy(SHP_ICON_UNITS, 1.0) - _
                                           PsScaleBy(SHP_ICONPAD_UNITS, 1.0)), str(rcAdd.x)

                                    '' ---- A GROUP THAT FORBIDS FOLDERS OFFERS NONE ---
                                    scope
                                        dim as long nNo2 = -1
                                        for r as long = 0 to g_panel->GetCount() - 1
                                            if ShellPanel_KindOf(r) <> EXPKIND_ROOT then continue for
                                            if ProjectFolders_CatAllowsFolders( cast(long, g_panel->GetItemData(r)) ) = false then
                                                nNo2 = r : exit for
                                            end if
                                        next
                                        if nNo2 >= 0 then
                                            g_panel->RowRect( nNo2, @rcRow )
                                            ShellExplorer_IconRects( nNo2, rcRow, rcAdd, rcRen, rcDel )
                                            Check "      a group that forbids folders offers none", _
                                                  (rcAdd.w = 0) andalso (rcRen.w = 0) andalso (rcDel.w = 0)
                                        end if
                                    end scope

                                    '' ---- AND A FILE ROW OFFERS NONE -----------------
                                    scope
                                        dim as long nFile3 = -1
                                        for r as long = 0 to g_panel->GetCount() - 1
                                            if ShellPanel_KindOf(r) = EXPKIND_FILE then nFile3 = r : exit for
                                        next
                                        if nFile3 >= 0 then
                                            g_panel->RowRect( nFile3, @rcRow )
                                            ShellExplorer_IconRects( nFile3, rcRow, rcAdd, rcRen, rcDel )
                                            Check "      a file row offers none", _
                                                  (rcAdd.w = 0) andalso (rcRen.w = 0) andalso (rcDel.w = 0)
                                        end if
                                    end scope

                                    '' ---- THE HIT TEST AGREES WITH THE ARITHMETIC ----
                                    '' At each icon's CENTRE, and in the gap to their left.
                                    '' Sampling the centre rather than an edge on purpose:
                                    '' an off-by-one in the half-open range would pass at
                                    '' the centre and is not what this assertion is for --
                                    '' the boundaries are asserted by the caption test.
                                    g_panel->EnsureVisible( nFolderRow )
                                    g_panel->RowRect( nFolderRow, @rcRow )
                                    ShellExplorer_IconRects( nFolderRow, rcRow, rcAdd, rcRen, rcDel )
                                    Check "      the hit test finds Add", _
                                          (ShellExplorer_IconHitTest( nFolderRow, rcAdd.x + rcAdd.w \ 2 ) = EXPICON_ADD)
                                    Check "        and Rename", _
                                          (ShellExplorer_IconHitTest( nFolderRow, rcRen.x + rcRen.w \ 2 ) = EXPICON_RENAME)
                                    Check "        and Delete", _
                                          (ShellExplorer_IconHitTest( nFolderRow, rcDel.x + rcDel.w \ 2 ) = EXPICON_DELETE)
                                    Check "        and nothing where the caption is", _
                                          (ShellExplorer_IconHitTest( nFolderRow, rcRow.x + 4 ) = EXPICON_NONE)

                                    '' ---- THE CLICK CLAIMS AN ICON AND NOTHING ELSE --
                                    '' The row must be HOT or the icons are not on screen,
                                    '' and a hit test that disagreed with the painter about
                                    '' that would put a live button under an invisible one.
                                    g_panel->nHotRow = nFolderRow
                                    Check "      a click on the caption is NOT claimed", _
                                          (ShellExplorer_OnRowClick( g_panel, nFolderRow, rcRow.x + 4, 0, 0 ) = false)
                                    g_panel->nHotRow = -1
                                    Check "      nor is a click on an icon of a COLD row", _
                                          (ShellExplorer_OnRowClick( g_panel, nFolderRow, _
                                                                     rcAdd.x + rcAdd.w \ 2, 0, 0 ) = false)

                                    g_panel->SetBounds( rcPanelWas )
                                end scope

                                '' ---- DRAG AND DROP, 7c step 25 ------------------------
                                ''
                                '' The DECISIONS, driven by row. Dragging with a mouse is
                                '' PsPlatform's and asserted there; what is asserted here is
                                '' the Explorer's policy about what may land where.
                                ''
                                '' EVERY PATH RETURNS FALSE, because the rows are a VIEW of
                                '' gProjectFolders -- so "did it move anything" is the
                                '' question, never the return value.
                                scope
                                    dim as long nFold = -1, nFile4 = -1, nGrp = -1
                                    for r as long = 0 to g_panel->GetCount() - 1
                                        select case ShellPanel_KindOf(r)
                                            case EXPKIND_FOLDER : if nFold  < 0 then nFold  = r
                                            case EXPKIND_FILE   : if nFile4 < 0 then nFile4 = r
                                            case EXPKIND_ROOT   : if nGrp   < 0 then nGrp   = r
                                        end select
                                    next
                                    Check "    a folder, a file and a group all have rows", _
                                          (nFold >= 0) andalso (nFile4 >= 0) andalso (nGrp >= 0), _
                                          str(nFold) & "," & str(nFile4) & "," & str(nGrp)

                                    '' ---- A FILE LANDS IN THE FOLDER IT WAS DROPPED ON --
                                    ''
                                    '' THE PATH AND THE DESTINATION ARE CAPTURED FIRST, and
                                    '' the first draft did not: the drop calls
                                    '' ShellExplorer_Load, every row index is rebuilt, and
                                    '' the lookup afterwards read a row that now means
                                    '' something else. It reported "(no doc)" -- the
                                    '' handler's own rule about snapshotting the source,
                                    '' arriving in the assertion written to check it.
                                    scope
                                        dim as DWSTRING wszDropped = ShellPanel_PathOf( nFile4 )
                                        dim as long catF
                                        dim as DWSTRING wszFolder = ShellExplorer_FolderPathFromRow( nFold, catF )

                                        '' THE RETURN IS ALWAYS FALSE. Asserted once and
                                        '' explicitly, because every other assertion here
                                        '' reads it as meaningless and that is only safe if
                                        '' it really is.
                                        Check "    the handler never asks for a reorder", _
                                              (ShellExplorer_OnDrop( g_panel, nFile4, nFold, true, 0 ) = false)

                                        dim pD2 as clsDocument ptr = gApp.GetDocumentPtrByFilename( wszDropped )
                                        Check "      and the file now names that folder", _
                                              (pD2 <> 0) andalso _
                                              (PsUCase(pD2->docData.wszFolder) = PsUCase(wszFolder)), _
                                              iif(pD2 <> 0, pD2->docData.wszFolder.Utf8, "(no doc)")
                                    end scope

                                    '' ---- A GROUP IS NOT A DRAGGABLE THING --------------
                                    '' Its caption and its existence are structural. Dragging
                                    '' one would either be undone by the next load or appear
                                    '' to work, which is worse.
                                    scope
                                        dim as long nBefore = ProjectFolders_Count()
                                        ShellExplorer_OnDrop( g_panel, nGrp, nFold, true, 0 )
                                        Check "      dropping a GROUP changes nothing", _
                                              (ProjectFolders_Count() = nBefore), _
                                              str(ProjectFolders_Count())
                                    end scope

                                    '' ---- A FOLDER MAY NOT LAND ON ITSELF ---------------
                                    scope
                                        '' RE-FOUND, because the file drop above reloaded the
                                        '' tree and nFold has meant nothing since. Every
                                        '' assertion in this block that reused an index
                                        '' across a drop was wrong for the same reason -- the
                                        '' handler's own first rule, arriving in the suite
                                        '' written to check it, for the second time.
                                        dim as long nFold2 = -1
                                        for r as long = 0 to g_panel->GetCount() - 1
                                            if ShellPanel_KindOf(r) = EXPKIND_FOLDER then nFold2 = r : exit for
                                        next
                                        dim as long catF
                                        dim as DWSTRING wszWas = ShellExplorer_FolderPathFromRow( nFold2, catF )
                                        Check "      the folder row is still findable", _
                                              (nFold2 >= 0) andalso (PsLen(wszWas) > 0), wszWas.Utf8
                                        ShellExplorer_OnDrop( g_panel, nFold2, nFold2, true, 0 )
                                        '' BY PATH, NOT BY COUNT. The first draft compared
                                        '' ProjectFolders_Count(), and a MOVE PRESERVES THE
                                        '' COUNT -- so the assertion held whether the folder
                                        '' had moved or not, and reverting the guard came
                                        '' back green. The path is the thing that changes.
                                        Check "      a folder dropped on ITSELF does not move", _
                                              (ProjectFolders_Find( catF, wszWas ) >= 0), wszWas.Utf8
                                    end scope

                                    '' ---- NOR INTO ITS OWN DESCENDANT -------------------
                                    '' The one refusal that prevents CORRUPTION rather than a
                                    '' no-op: the folder would become its own ancestor and the
                                    '' tree walk would not terminate.
                                    scope
                                        dim as long nFold3 = -1
                                        for r as long = 0 to g_panel->GetCount() - 1
                                            if ShellPanel_KindOf(r) = EXPKIND_FOLDER then nFold3 = r : exit for
                                        next
                                        dim as long catP = -1
                                        dim as DWSTRING wszParent = ShellExplorer_FolderPathFromRow( nFold3, catP )
                                        if (catP >= 0) andalso (PsLen(wszParent) > 0) then
                                            dim as DWSTRING wszChild = ProjectFolders_Combine( wszParent, DWSTRING("Inner") )
                                            dim as long nIdxC = ProjectFolders_Add( catP, wszChild )
                                            Check "      a child folder exists to drop into", _
                                                  (nIdxC >= 0), str(nIdxC)
                                            ShellExplorer_Load()

                                            dim as long nChildRow = -1, nParentRow = -1
                                            for r as long = 0 to g_panel->GetCount() - 1
                                                if ShellPanel_KindOf(r) <> EXPKIND_FOLDER then continue for
                                                dim as long ci = cast(long, g_panel->GetItemData(r))
                                                if PsUCase(gProjectFolders(ci).wszPath) = PsUCase(wszChild) then nChildRow = r
                                                if PsUCase(gProjectFolders(ci).wszPath) = PsUCase(wszParent) then nParentRow = r
                                            next
                                            Check "        and both have rows", _
                                                  (nChildRow >= 0) andalso (nParentRow >= 0), _
                                                  str(nParentRow) & "," & str(nChildRow)

                                            ShellExplorer_OnDrop( g_panel, nParentRow, nChildRow, true, 0 )
                                            '' BY PATH, for the reason above -- and here it
                                            '' matters twice over, because a move INTO A
                                            '' DESCENDANT does not change the count either
                                            '' and would have gone entirely unnoticed.
                                            Check "        dropping a folder INTO ITS OWN CHILD is refused", _
                                                  (ProjectFolders_Find( catP, wszParent ) >= 0), _
                                                  wszParent.Utf8
                                            Check "          and the child is still under it", _
                                                  (ProjectFolders_Find( catP, wszChild ) >= 0), _
                                                  wszChild.Utf8
                                        end if
                                    end scope
                                end scope

                                '' ---- THE DROP BLOCK'S OWN LEFTOVER --------------------
                                '' It created a child folder to drop INTO, and nothing else
                                '' removes it: deleting its parent DISSOLVES rather than
                                '' deletes, so the child would survive one level up and the
                                '' count assertion below would report it. Removed by PATH,
                                '' because every row index in this block is long stale.
                                scope
                                    for cat as long = CATINDEX_MAIN to ubound(gConfig.Cat)
                                        dim as long idx = -1
                                        do
                                            idx = -1
                                            for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
                                                if gProjectFolders(i).catIndex <> cat then continue for
                                                if PsUCase( ProjectFolders_LeafName( gProjectFolders(i).wszPath ) ) = _
                                                   PsUCase( DWSTRING("Inner") ) then idx = i : exit for
                                            next
                                            if idx >= 0 then ProjectFolders_RemoveAt( idx )
                                        loop while idx >= 0
                                    next
                                    ShellExplorer_Load()
                                end scope

                                '' Clean up: the workspace is shared with everything after
                                '' this block, and a stray folder changes the tree's shape.
                                dim as long nRow1 = -1
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if ShellPanel_KindOf(r) <> EXPKIND_FOLDER then continue for
                                    if cast(long, g_panel->GetItemData(r)) = nNew then nRow1 = r : exit for
                                next
                                if nRow1 >= 0 then ShellExplorer_DeleteFolder( nRow1 )
                                Check "    and the suite leaves the folder table as it found it", _
                                      (ProjectFolders_Count() = nWas), _
                                      str(ProjectFolders_Count()) & " was " & str(nWas)
                            end scope

                            '' ---- THE ROW GLYPH, 7c step 22 -----------------------
                            ''
                            '' THE DECISION, NOT THE DRAWING. The painter needs a
                            '' compositor; which glyph a kind shows does not, which is why
                            '' it is a pure function -- the same split PsModalRouteEvent
                            '' got in step 2 for the same reason.
                            ''
                            '' EXHAUSTIVE OVER THE KINDS, including the four that draw
                            '' nothing. A kind added later and forgotten would otherwise
                            '' inherit "no glyph" silently, and an Explorer row with no
                            '' icon looks like a row that is still loading.
                            scope
                                dim as DWSTRING gFile = ShellExplorer_GlyphFor( EXPKIND_FILE )
                                Check "  a file row shows a glyph", (PsLen(gFile) = 1), _
                                      str(PsLen(gFile))
                                '' U+00B7 MIDDLE DOT, and asserted by CODEPOINT rather than
                                '' by "not empty": the whole reason this one is safe where
                                '' the pane switcher's are unproven is that it is not a
                                '' private-use character, and only its value says so.
                                Check "    which is U+00B7, not a private-use codepoint", _
                                      (gFile.At(0) = &hB7), hex(gFile.At(0))

                                Check "    a group shows none -- the twisty is already there", _
                                      (PsLen(ShellExplorer_GlyphFor(EXPKIND_ROOT)) = 0)
                                Check "    nor does a folder", _
                                      (PsLen(ShellExplorer_GlyphFor(EXPKIND_FOLDER)) = 0)
                                Check "    nor the Save-as-Project row", _
                                      (PsLen(ShellExplorer_GlyphFor(EXPKIND_PROMOTE)) = 0)
                                Check "    and an untagged row shows none", _
                                      (PsLen(ShellExplorer_GlyphFor(EXPKIND_NONE)) = 0)

                                '' ---- AND THE DECISION IS ACTUALLY WIRED TO A PAINTER --
                                '' Removing the OnPaintOverlay call left every assertion
                                '' above GREEN: they test a pure function that nothing had
                                '' to be calling. A correct decision reaching no painter is
                                '' a pane with no icons and a suite that says otherwise --
                                '' which is this port's oldest failure, one layer along.
                                ''
                                '' The two row callbacks get the same treatment, because
                                '' the same argument applies to them and neither was
                                '' checked either.
                                Check "    the overlay is installed on the panel", _
                                      (g_panel->pfnOverlay <> 0)
                                Check "      as is the context handler", _
                                      (g_panel->pfnContext <> 0)
                                Check "      and the two row callbacks", _
                                      (g_panel->pfnSelChange <> 0) andalso _
                                      (g_panel->pfnActivate <> 0)
                                '' THE EDIT PAIR, added in step 23 for the same reason and
                                '' after the same green revert: the rename policy is
                                '' asserted by calling the two callbacks DIRECTLY, so
                                '' removing the lines that install them changed nothing.
                                '' Twice in two steps, which is why this list exists.
                                Check "      and the label-edit pair", _
                                      (g_panel->pfnBeginEdit <> 0) andalso _
                                      (g_panel->pfnEndEdit <> 0)
                                Check "      and the left-click hook", _
                                      (g_panel->pfnClick <> 0)
                                Check "      and the drop hook", (g_panel->pfnDrop <> 0)
                                '' AND THE GESTURE IS ON, which is separate from the hook
                                '' being installed: with SetDragReorder false nothing can
                                '' ever be dragged and every drop assertion below would be
                                '' testing a handler nothing can reach.
                                Check "        with dragging enabled in this mode", _
                                      g_panel->bDragReorder
                            end scope

                            '' IsFileDisplayed READS THE MODEL, NOT THE ROWS -- so it
                            '' answers correctly with the pane on another mode, which is
                            '' the whole reason it is not a row walk. Asserted from
                            '' BOOKMARKS mode, where a row walk would answer false.
                            ShellPanel_SetMode( SHPANEL_BOOKMARKS )
                            Check "    IsFileDisplayed answers from the model, not the pane", _
                                  ShellExplorer_IsFileDisplayed( wszFile ) andalso _
                                  (g_panelMode = SHPANEL_BOOKMARKS)
                            Check "      and says no to a file not in the workspace", _
                                  (ShellExplorer_IsFileDisplayed(DWSTRING("C:/nowhere/x.bas")) = false)


                            '' ---- THE PANE SWITCHER, 7c step 20 ----------------------
                            ''
                            '' WRITTEN BECAUSE THREE REVERTS CAME BACK GREEN. Removing the
                            '' SyncToMode call, turning the items into COMMAND items so
                            '' nothing latches, and setting the strip's height to ZERO each
                            '' left the suite at 417/0. The geometry assertions in the
                            '' layout block are RELATIONS -- strip above tree, union equals
                            '' tiko's band -- and a zero-height strip satisfies every one
                            '' of them. That is step 1's finding word for word: relations
                            '' hold perfectly at the wrong size.
                            scope
                                Check "  the pane switcher has the three panes", _
                                      (g_panelMenu <> 0) andalso (g_panelMenu->GetCount() = 3), _
                                      str(g_panelMenu->GetCount())

                                '' BY ID, NOT BY POSITION. The order is a layout decision;
                                '' what must not drift is which glyph carries which command.
                                dim as long iExp = g_panelMenu->FindItemByID( IDM_VIEWEXPLORER )
                                dim as long iFun = g_panelMenu->FindItemByID( IDM_FUNCTIONLIST )
                                dim as long iBmk = g_panelMenu->FindItemByID( IDM_BOOKMARKSLIST )
                                Check "    each carrying its own menu id", _
                                      (iExp >= 0) andalso (iFun >= 0) andalso (iBmk >= 0), _
                                      str(iExp) & "," & str(iFun) & "," & str(iBmk)

                                '' EVERY ITEM MUST BE A TOGGLE. A COMMAND item never
                                '' latches -- PsIconPanel refuses it in SetSelected -- so
                                '' the active pane would never be lit and every assertion
                                '' below would be about a selection that cannot happen.
                                '' tiko's items ARE commands, and it hand-paints the
                                '' highlight instead; this is the one place the port is
                                '' simpler than the original, so it is worth pinning.
                                dim as boolean bAllToggle = true
                                for k as long = 0 to g_panelMenu->GetCount() - 1
                                    if g_panelMenu->items(k).kind <> PSICON_TOGGLE then bAllToggle = false
                                next
                                Check "    and all three latch", bAllToggle

                                '' ---- THE STRIP FOLLOWS THE MODE ---------------------
                                '' Driven through ShellPanel_SetMode, so this is the
                                '' assertion for the sync being CALLED as well as correct.
                                ShellPanel_SetMode( SHPANEL_EXPLORER )
                                Check "    explorer mode lights the explorer icon", _
                                      g_panelMenu->GetSelected( iExp )
                                Check "      and nothing else", _
                                      (g_panelMenu->GetSelected(iFun) = false) andalso _
                                      (g_panelMenu->GetSelected(iBmk) = false)

                                ShellPanel_SetMode( SHPANEL_BOOKMARKS )
                                Check "    switching mode moves the highlight", _
                                      g_panelMenu->GetSelected( iBmk ) andalso _
                                      (g_panelMenu->GetSelected(iExp) = false)

                                '' AND FROM THE MENU, which is the path that would leave the
                                '' strip stale if the sync were driven from the click
                                '' handler instead. This is the whole reason SyncToMode
                                '' hangs off SetMode and not off OnClick.
                                OnMenuCommand( 0, IDM_FUNCTIONLIST, 0 )
                                Check "    and the MENU moves it too, not just a click", _
                                      (g_panelMode = SHPANEL_FUNCTIONS) andalso _
                                      g_panelMenu->GetSelected( iFun ) andalso _
                                      (g_panelMenu->GetSelected(iBmk) = false)

                                '' ---- AND A CLICK IS THE SAME PATH -------------------
                                '' ShellPanelMenu_OnClick resolves the item to its id and
                                '' hands it to OnMenuCommand, so this asserts the wiring
                                '' rather than a second copy of the switch.
                                ShellPanelMenu_OnClick( g_panelMenu, iBmk, 0 )
                                Check "    clicking an icon switches the pane", _
                                      (g_panelMode = SHPANEL_BOOKMARKS) andalso _
                                      g_panelMenu->GetSelected( iBmk )
                            end scope

                            '' Leave the document as this block found it -- no bookmarks --
                            '' so the assertions after it see what they expect.
                            ShellPanel_SetMode( SHPANEL_BOOKMARKS )
                            pDocF->ToggleBookmark( 1 )
                            ShellPanel_Reload()
                        end scope

                        '' ONLY SOURCE FILES ARE PARSED. tiko's filter, ported: .bas, .bi,
                        '' .inc, and anything with no path at all (an unsaved buffer, which
                        '' is exactly when the panel should follow the typing).
                        ''
                        '' Driven by renaming the document rather than opening a second
                        '' file: the filter reads DiskFilename and nothing else, so this
                        '' reaches it without another tab to clean up afterwards.
                        '' pD, not pDoc: the bookmark block's pDoc belongs to a scope that
                        '' has already closed, and fbc reports an unknown name at statement
                        '' position as "Expected End-of-Line" -- five errors none of which
                        '' mentions the name being undefined.
                        dim as clsDocument ptr pD = ShellTabs_CurrentDoc()
                        dim as DWSTRING sNameWas = pD->DiskFilename
                        pD->DiskFilename = "C:/dev/notsource.txt"
                        Check "    a .txt is not scanned", (ShellScan_Buffer(pD) = false)
                        pD->DiskFilename = "C:/dev/notsource.bi"
                        Check "      but a .bi is", ShellScan_Buffer(pD)
                        '' That last one ASKED for a scan of a file called .bi that does
                        '' not exist under that name. Drained here so it cannot land in the
                        '' middle of a later assertion and replace the buffer tier.
                        ShellScan_DrainFor( 5000 )
                        pD->DiskFilename = *sNameWas.Wz()
                    end scope

                    '' THE SAME PATH AGAIN IS THE SAME TAB. Through gApp's lookup now, so
                    '' this is also what proves the delegation is wired rather than merely
                    '' compiled.
                    dim as long idx2 = ShellTabs_Open( wszFile )
                    Check "  reopening it returns the same tab", (idx2 = idx1), _
                          str(idx1) & " then " & str(idx2)
                    Check "    and creates no second document", _
                          (gApp.GetDocumentCount() = 1), str(gApp.GetDocumentCount())

                    '' ---- THE BOOKMARK LOADER, END TO END ---------------------------
                    '' Driven HERE rather than in its own scope because it needs exactly
                    '' what this one already built: a real document, in a tab, with a live
                    '' Scintilla view behind it. A loader asserted against an empty
                    '' pDocList would prove only that it does not crash on nothing.
                    scope
                        dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
                        pDoc->ToggleBookmark( 1 )
                        Check "  a bookmark is set on line 1", _
                              (pDoc->GetBookmarks() = "1"), pDoc->GetBookmarks()

                        ShellBookmarks_Load()
                        '' A HEADER FOR THE FILE AND A ROW FOR THE LINE. Two rows, not one:
                        '' the grouping is what makes bookmarks across several files
                        '' readable, and it is the shape the click handler walks.
                        Check "  the panel lists it under a file header", _
                              (g_panel->GetCount() = 2), str(g_panel->GetCount())
                        Check "    the first row is the header", g_panel->IsHeader(0)
                        Check "    and the second is not", (g_panel->IsHeader(1) = false)

                        '' THE PACKED SLOT, READ BACK OFF A REAL ROW. Everything below is
                        '' asserted on synthetic values too, but only this says the LOADER
                        '' packed what it claimed to.
                        Check "    the row carries its tab", _
                              (ShellPanel_TabOf(1) = idx1), _
                              str(ShellPanel_TabOf(1))
                        Check "    and its line", _
                              (ShellPanel_LineOf(1) = 1), _
                              str(ShellPanel_LineOf(1))

                        '' THE VIEW SURVIVED THE WALK. The loader points the single view at
                        '' each document in turn to read its markers -- see shellpanel.bi --
                        '' and a missing ADDREF frees the document the user is looking at.
                        ''
                        '' THIS ONE DOES NOT BITE WITH A SINGLE DOCUMENT OPEN, and saying so
                        '' is the honest half. Reverting the ADDREF leaves it green: the tab
                        '' entry created the document with SCI_CREATEDOCUMENT and the view
                        '' then took its own reference, so the count has slack and one
                        '' unbalanced release does not reach zero. The two-document block
                        '' below is where the reference discipline is actually exercised.
                        Check "    and the editor still holds its document", _
                              (SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0) > 0), _
                              str(SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0))

                        '' ---- THE LOADER MUST NOT MOVE THE CARET ---------------------
                        '' REPORTED BY THE AUTHOR: Ctrl+F2 set the bookmark and then threw
                        '' the caret back to line 1, column 1.
                        ''
                        '' The cause is the doc-pointer walk this loader does to read each
                        '' document's markers. SCI_SETDOCPOINTER RE-ATTACHES A DOCUMENT AND
                        '' RESETS THE VIEW'S CARET AND SCROLL -- position belongs to the
                        '' VIEW, not the document, which is the same fact ShellTabs_Show
                        '' exists to work around when switching tabs. Pointing away and
                        '' back is not a no-op.
                        ''
                        '' Written before the fix and confirmed FAILING, so this is a
                        '' reproduction rather than a description.
                        scope
                            dim as long nWant = SciMsg( g_view->pSci, SCI_POSITIONFROMLINE, 1, 0 )
                            SciMsg( g_view->pSci, SCI_GOTOPOS, nWant, 0 )
                            ShellBookmarks_Load()
                            Check "  the loader leaves the caret where it was", _
                                  (SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0) = nWant), _
                                  str(SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0)) & _
                                  " wanted " & str(nWant)
                            '' PUT IT BACK. This scope moved the caret to line 1 and the
                            '' assertions below say "the caret is at the top in a windowless
                            '' run" -- which was true until this block existed. Leaving it
                            '' moved failed two of them, on a bookmark toggled at the wrong
                            '' line, and neither failure would have named this scope.
                            g_view->Msg( SCI_GOTOPOS, 0, 0 )
                        end scope

                        '' A DOCUMENT WITH NO BOOKMARKS CONTRIBUTES NO HEADER -- tiko's
                        '' `if len(sBookmarks)` guard. Without it the panel lists every open
                        '' file whether or not it has anything in it.
                        pDoc->ToggleBookmark( 1 )
                        ShellBookmarks_Load()
                        Check "  clearing the bookmark empties the panel", _
                              (g_panel->GetCount() = 0), str(g_panel->GetCount())

                        '' ---- THROUGH THE COMMAND, not through the model. Everything
                        '' above calls clsDocument directly; this drives OnMenuCommand the
                        '' way a menu click or an accelerator does, which is the only path
                        '' that proves the ids are actually wired to something.
                        ''
                        '' The caret is at the top in a windowless run, so this toggles line
                        '' 0 -- which also exercises the packed value ZERO being a real row
                        '' rather than a hole.
                        OnMenuCommand( 0, IDM_BOOKMARKTOGGLE, 0 )
                        Check "  the Toggle COMMAND sets a bookmark", _
                              (pDoc->GetBookmarks() = "0"), pDoc->GetBookmarks()
                        Check "    and reloads the panel itself", _
                              (g_panel->GetCount() = 2), str(g_panel->GetCount())
                        Check "    with the row packed at line 0", _
                              (g_panel->GetItemData(1) = 0), str(g_panel->GetItemData(1))

                        '' ---- CLEAR ALL SPARES THE BREAKPOINTS, and this is the assertion
                        '' for a DELIBERATE DIVERGENCE FROM tiko rather than for a port.
                        ''
                        '' frmMainSearch.inc:449 passes -1 to SCI_MARKERDELETEALL, which is
                        '' Scintilla's "every marker type" -- so tiko's Clear All Bookmarks
                        '' also deletes breakpoints and the debugger's current-line marker.
                        '' Its own Clear-All-DOCUMENTS arm four lines below passes
                        '' MARKER_BOOKMARK, so the two halves disagree with each other.
                        ''
                        '' The marker is set directly rather than through ToggleBreakPoint,
                        '' which routes through the debugger engine when one is attached.
                        SciMsg( g_view->pSci, SCI_MARKERADD, 1, MARKER_BREAKPOINT )
                        Check "  a breakpoint marker is set on line 1", _
                              (bit(SciMsg(g_view->pSci, SCI_MARKERGET, 1, 0), MARKER_BREAKPOINT) <> 0)

                        '' Clear All on the current document, through its command.
                        OnMenuCommand( 0, IDM_BOOKMARKCLEARALL, 0 )
                        Check "  the Clear All COMMAND clears them", _
                              (pDoc->GetBookmarks() = ""), pDoc->GetBookmarks()
                        Check "    and empties the panel", (g_panel->GetCount() = 0), _
                              str(g_panel->GetCount())
                        Check "    but leaves the BREAKPOINT alone, unlike tiko's -1", _
                              (bit(SciMsg(g_view->pSci, SCI_MARKERGET, 1, 0), MARKER_BREAKPOINT) <> 0)
                        SciMsg( g_view->pSci, SCI_MARKERDELETEALL, MARKER_BREAKPOINT, 0 )

                        '' NEXT AND PREV WITH NO BOOKMARK AT ALL. Both are reachable from
                        '' the keyboard at any time -- F2 is bound -- and SCI_MARKERNEXT
                        '' answers -1, which NextBookmark must not turn into a GOTOLINE.
                        dim as long nPosWas = SciMsg( g_view->pSci, SCI_GETCURRENTPOS, 0, 0 )
                        OnMenuCommand( 0, IDM_BOOKMARKNEXT, 0 )
                        OnMenuCommand( 0, IDM_BOOKMARKPREV, 0 )
                        Check "  Next and Prev with no bookmarks move nothing", _
                              (SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0) = nPosWas), _
                              str(SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0))
                    end scope

                    '' ---- TWO DOCUMENTS, WHICH IS WHAT THE LOADER EXISTS FOR -------
                    '' Everything above runs with one file open, and one file cannot show
                    '' the defect this loader was written to avoid: clsDocument.GetMarkers
                    '' walks the ACTIVE VIEW, and this binary has ONE view for every tab, so
                    '' asking a BACKGROUND tab for its bookmarks returns the FOREGROUND
                    '' tab's -- every group in the panel listing the same lines under a
                    '' different filename.
                    ''
                    '' The two files carry bookmarks on DIFFERENT lines precisely so that
                    '' answer would be wrong in a way an assertion can see.
                    scope
                        '' ---- THE SECOND PROBE HAS A PROCEDURE NOW (7c step 6) ---------
                        '' It was four print statements, chosen so it contributed NOTHING
                        '' and the buffer tier's one-file limit could be asserted. The
                        '' project tier is what removes that limit, so the file needs
                        '' something to contribute -- and it keeps four lines, because the
                        '' bookmark assertions below put a mark on line 3.
                        '' ---- AND A THIRD PROBE THAT IS NEVER OPENED (7c step 8) -------
                        '' The pane lists every file the database knows, not the open tabs,
                        '' so the case that matters is a file with NO TAB. This one is
                        '' reached only through probe2's #include and is never handed to
                        '' ShellTabs_Open -- which is what makes the assertions below able
                        '' to fail if the pane quietly went back to listing tabs.
                        dim as DWSTRING wszFile3 = wszDir & "/open_probe3.bas"
                        PsFileWriteAll( wszFile3, _
                                !"' three\nsub ThirdProc()\n  print 3\nend sub\n" )

                        dim as DWSTRING wszFile2 = wszDir & "/open_probe2.bas"
                        if PsFileWriteAll( wszFile2, _
                                !"' two\n#include once \"open_probe3.bas\"\n" & _
                                !"sub SecondProc()\n  print 2\nend sub\n" ) then
                            dim as long idxB = ShellTabs_Open( wszFile2 )
                            ShellScan_DrainFor( 5000 )
                            Check "  a second file opens into its own tab", _
                                  (idxB > idx1), str(idx1) & " then " & str(idxB)
                            Check "    and gApp has both", _
                                  (gApp.GetDocumentCount() = 2), str(gApp.GetDocumentCount())

                            '' B is in the foreground now -- ShellTabs_Open selects what it
                            '' opens -- so A is the background document whose markers the
                            '' naive read would get wrong.
                            dim as clsDocument ptr pB = g_tabDocs(idxB).pDoc
                            dim as clsDocument ptr pA = g_tabDocs(idx1).pDoc
                            pB->ToggleBookmark( 3 )

                            '' A's bookmark is set while B is showing, which needs the view
                            '' pointed at A first -- the same dance the loader does.
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idx1).pSciDoc) )
                            pA->ToggleBookmark( 1 )
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idxB).pSciDoc) )

                            ShellBookmarks_Load()
                            '' Two headers, two rows.
                            Check "  both files appear in the panel", _
                                  (g_panel->GetCount() = 4), str(g_panel->GetCount())

                            '' ---- THE LOAD-BEARING ONE. Find each file's row and check the
                            '' LINE it carries. If the loader read the foreground document
                            '' for both, both rows say line 3 and the count above is still 4.
                            dim as long nLineA = -1, nLineB = -1
                            for r as long = 0 to g_panel->GetCount() - 1
                                if g_panel->IsHeader(r) then continue for
                                if ShellPanel_TabOf(r) = idx1 then nLineA = ShellPanel_LineOf(r)
                                if ShellPanel_TabOf(r) = idxB then nLineB = ShellPanel_LineOf(r)
                            next
                            Check "    the background tab's row carries ITS line", _
                                  (nLineA = 1), str(nLineA)
                            Check "    and the foreground tab's carries its own", _
                                  (nLineB = 3), str(nLineB)

                            '' BOTH DOCUMENTS STILL HAVE THEIR TEXT after the walk pointed
                            '' the view at each in turn. This is the reference discipline's
                            '' real test: with two documents there is no slack left.
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idx1).pSciDoc) )
                            dim as long nLenA = SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0)
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idxB).pSciDoc) )
                            dim as long nLenB = SciMsg(g_view->pSci, SCI_GETLENGTH, 0, 0)
                            Check "    and neither document was freed under the view", _
                                  (nLenA > 0) andalso (nLenB > 0), _
                                  str(nLenA) & " / " & str(nLenB)

                            '' ---- THE CLICK, DRIVEN BY ROW NUMBER --------------------
                            '' The gesture cannot be delivered here -- no mouse reaches a
                            '' windowless surface -- so ShellPanel_GotoRow is split out of
                            '' the callback and driven directly. What that leaves untested
                            '' is the WIRING (OnSelChange -> handler), and that is stated in
                            '' the commit rather than implied by these passing.
                            ''
                            '' The panel currently holds B's group and A's group. Jumping to
                            '' A's row from B being current is the case that matters: it has
                            '' to CHANGE TABS, not just move a caret.
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idxB).pSciDoc) )
                            g_nTabCur = idxB

                            dim as long nRowA = -1, nRowHdr = -1
                            for r as long = 0 to g_panel->GetCount() - 1
                                if g_panel->IsHeader(r) then
                                    if nRowHdr = -1 then nRowHdr = r
                                    continue for
                                end if
                                if ShellPanel_TabOf(r) = idx1 then nRowA = r
                            next

                            Check "  a header row goes nowhere", _
                                  (ShellPanel_GotoRow(nRowHdr) = false), str(nRowHdr)
                            Check "  and neither does a row that does not exist", _
                                  (ShellPanel_GotoRow(9999) = false)

                            '' THE CALL IS MADE INTO A LOCAL FIRST, and that is not style.
                            '' Written inline as `Check "...", GotoRow(r) andalso (g_nTabCur
                            '' = idx1), str(g_nTabCur)`, the NOTE printed "1 wanted 0" on a
                            '' PASSING assertion -- fbc had built the note argument before
                            '' evaluating the condition that changes what it reports. A
                            '' diagnostic that describes the state before the thing it is
                            '' diagnosing is worse than none.
                            dim as boolean bWent = ShellPanel_GotoRow( nRowA )
                            Check "  clicking the other file's bookmark switches tab", _
                                  bWent andalso (g_nTabCur = idx1), _
                                  str(g_nTabCur) & " wanted " & str(idx1)
                            '' AND LANDS ON THE LINE. The caret is what the user asked for;
                            '' the tab switch is only how it got there.
                            Check "    and lands on its line", _
                                  (SciMsg(g_view->pSci, SCI_LINEFROMPOSITION, _
                                          SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0), 0) = 1), _
                                  str(SciMsg(g_view->pSci, SCI_LINEFROMPOSITION, _
                                             SciMsg(g_view->pSci, SCI_GETCURRENTPOS, 0, 0), 0))

                            '' ---- THE TWO TIERS, AND WHAT THE PROJECT ONE FIXED --------
                            ''
                            '' UNTIL 7c STEP 6 THIS BLOCK ASSERTED THE OPPOSITE. It read
                            '' "only the last-scanned file has symbols" and "so the
                            '' functions pane is empty here", because the buffer tier holds
                            '' ONE set and this binary filled nothing else -- the pane could
                            '' only ever list the file in front of the user. Those
                            '' assertions carried a note saying that if a project tier ever
                            '' arrived they should fail and say so. It arrived; they did;
                            '' this is what replaced them.
                            scope
                                dim rsA() as SYMBOLREF

                                '' STILL TRUE, AND STILL THE POINT: the BUFFER tier holds
                                '' one file. Scanning B evicts A from it.
                                gAppNotify.RequestBufferScan( g_tabDocs(idxB).pDoc )
                                ShellScan_DrainFor( 5000 )

                                '' A's symbols survive anyway -- from the PROJECT tier,
                                '' which reached A as the root and B through A's #include.
                                ShellScan_Project()
                                ShellScan_DrainFor( 15000 )
                                Check "  a project scan reaches both files", _
                                      (g_nProjCount > 0) andalso (g_nLastProjMs >= 0), _
                                      str(g_nLastProjMs) & "ms"

                                dim as long nA = gSymDb.EnumProcsInFile( wszFile, rsA() )
                                Check "    so the non-active file has symbols again", _
                                      (nA = 3), str(nA) & " for the first file"

                                '' THE PANEL IS THE POINT. Two headers and three
                                '' procedures: ProbeAlpha and ProbeBeta from the root file,
                                '' SecondProc from the one it includes. Before this commit
                                '' the same call produced rows for one file at most.
                                ShellPanel_SetMode( SHPANEL_FUNCTIONS )
                                ShellPanel_Reload()
                                dim as string sSeen
                                for t as long = 0 to g_nTabDocs - 1
                                    dim rsT() as SYMBOLREF
                                    sSeen &= " tab" & t & "=" & _
                                        str(gSymDb.EnumProcsInFile( _
                                            g_tabDocs(t).pDoc->DiskFilename, rsT() ))
                                next
                                '' SEVEN ROWS SINCE STEP 8, not five: three headers and four
                                '' procedures. The third file is the one that is NOT OPEN,
                                '' and it is in the pane because the pane reads the symbol
                                '' database rather than the tab bar.
                                Check "    and the pane lists ALL THREE files", _
                                      (g_panel->GetCount() = 7), _
                                      str(g_panel->GetCount()) & " rows," & sSeen

                                '' ---- THE UNOPENED FILE, WHICH IS THE COMMIT ---------------
                                '' Find its row, and assert first that it really has no tab --
                                '' otherwise the open-on-click assertion below would pass by
                                '' doing nothing at all.
                                dim as long nRow3 = -1
                                for r as long = 0 to g_panel->GetCount() - 1
                                    if g_panel->IsHeader(r) then continue for
                                    if PsInStr( PsUCase(ShellPanel_PathOf(r)), _
                                                "OPEN_PROBE3" ) > 0 then nRow3 = r
                                next
                                Check "      the unopened file has a row of its own", _
                                      (nRow3 >= 0), str(nRow3)
                                Check "        and no tab, which is why it is the case that " & _
                                      "matters", (ShellPanel_TabOf(nRow3) = -1), _
                                      str(ShellPanel_TabOf(nRow3))

                                '' AND ITS NAME IS NOT SHOUTING. EnumUserFiles answers out of
                                '' the parser's string pool, which holds names UPPERCASED --
                                '' so without FilenameOriginalCase this heading reads
                                '' OPEN_PROBE3.BAS. Nothing else in this suite would notice.
                                dim as DWSTRING wszShown3 = ShellPanel_PathOf(nRow3)
                                Check "        with its name in the case the disk holds", _
                                      (PsPathName(wszShown3) = "open_probe3.bas"), _
                                      PsPathName(wszShown3).Utf8

                                '' CLICKING IT OPENS IT FROM DISK. Until step 8 GotoRow called
                                '' ShellTabs_Show with a tab index, and Show is a SILENT no-op
                                '' for a bad one -- so this row would have jumped to a line in
                                '' whatever tab was current, in the WRONG FILE, with no error.
                                dim as long nTabsWas = g_nTabDocs
                                Check "      clicking it succeeds", _
                                      (ShellPanel_GotoRow(nRow3) = true)
                                Check "        by opening it from disk", _
                                      (g_nTabDocs = nTabsWas + 1), _
                                      str(nTabsWas) & " -> " & str(g_nTabDocs)
                                Check "        and it now has a tab", _
                                      (ShellTabs_FindByPath( wszFile3 ) >= 0), _
                                      str(ShellTabs_FindByPath( wszFile3 ))

                                '' A SECOND CLICK MUST NOT OPEN A SECOND COPY. This is what
                                '' fails if FilenameOriginalCase ever hands back PsCore's
                                '' forward slashes: the document lookup misses and the file
                                '' is opened again, which looks like nothing at all.
                                dim as long nTabsNow = g_nTabDocs
                                ShellPanel_GotoRow(nRow3)
                                Check "        clicking again reuses the tab", _
                                      (g_nTabDocs = nTabsNow), _
                                      str(nTabsNow) & " -> " & str(g_nTabDocs)

                                '' NO DUPLICATES, which is the failure mode a wrong merge
                                '' produces -- RecomputeContrib suppresses the project
                                '' tier's copy of whichever file the buffer owns, and if it
                                '' did not, that file's procedures would appear TWICE
                                '' rather than not at all.
                                dim as long nB = gSymDb.EnumProcsInFile( _
                                        g_tabDocs(idxB).pDoc->DiskFilename, rsA() )
                                Check "      each file's procedures exactly once", _
                                      (nA = 3) andalso (nB = 1), _
                                      str(nA) & " and " & str(nB)

                                '' SWITCHING BACK TO A RESCANS IT -- ShellTabs_Show requests
                                '' a buffer scan, which is tiko's own site for it
                                '' (clsTopTabCtl.inc:271) and is what keeps the panel
                                '' describing the document in front of the user.
                                '' g_nTabCur IS MOVED OFF A FIRST. ShellTabs_Show returns
                                '' immediately when the tab is already current -- the
                                '' re-show guard -- so asking it to switch to the tab that
                                '' IS current scans nothing, and the first version of this
                                '' assertion read that as "the rescan does not work".
                                ShellTabs_Show( idxB )
                                ShellTabs_Show( idx1 )
                                ShellScan_DrainFor( 5000 )
                                Check "    switching back to the first tab rescans it", _
                                      (gSymDb.EnumProcsInFile(wszFile, rsA()) = 3), _
                                      str(gSymDb.EnumProcsInFile(wszFile, rsA()))
                                ShellPanel_Reload()
                                Check "      and the pane still lists all three", _
                                      (g_panel->GetCount() = 7), str(g_panel->GetCount())
                                ShellPanel_SetMode( SHPANEL_BOOKMARKS )
                            end scope

                            '' ---- CLOSE THE THIRD PROBE FIRST (7c step 8) --------------
                            '' The pane OPENED this one, by being clicked, so the suite has a
                            '' tab it did not ask for and has to close it or the "left gApp's
                            '' list as it found it" assertion at the end fails -- which is
                            '' exactly what it caught on the first run of this block, and is
                            '' the reason that assertion is worth its line.
                            ''
                            '' HIGHEST INDEX FIRST, because closing a lower one would move it.
                            scope
                                dim as long idxC = ShellTabs_FindByPath( wszFile3 )
                                if idxC >= 0 then
                                    dim as clsDocument ptr pC = g_tabDocs(idxC).pDoc
                                    ShellTabs_Show( idx1 )
                                    if g_tabs <> 0 then g_tabs->DeleteTab( idxC )
                                    if g_tabDocs(idxC).pSciDoc <> 0 then
                                        g_view->Msg( SCI_RELEASEDOCUMENT, 0, _
                                                     cast(integer, g_tabDocs(idxC).pSciDoc) )
                                    end if
                                    gApp.RemoveDocument( pC )
                                    g_tabDocs(idxC).pDoc    = 0
                                    g_tabDocs(idxC).pSciDoc = 0
                                    g_nTabDocs -= 1
                                    g_nTabCur  = idx1
                                end if
                            end scope

                            '' Tidy: drop B's bookmark, its tab and its document.
                            pB->ToggleBookmark( 3 )
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idx1).pSciDoc) )
                            pA->ToggleBookmark( 1 )
                            if g_tabs <> 0 then g_tabs->DeleteTab( idxB )
                            g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, g_tabDocs(idxB).pSciDoc) )
                            gApp.RemoveDocument( pB )
                            g_tabDocs(idxB).pDoc    = 0
                            g_tabDocs(idxB).pSciDoc = 0
                            g_nTabDocs -= 1
                            g_nTabCur  = idx1
                            ShellPanel_Clear()
                        end if
                        PsFileDelete( wszFile2 )
                        PsFileDelete( wszFile3 )
                    end scope

                    '' ---- CLEANUP, and it has to be complete: this suite runs before the
                    '' band walk, which asserts geometry against a tree this test has added
                    '' a tab to.
                    if (idx1 >= 0) andalso (g_tabs <> 0) then g_tabs->DeleteTab( idx1 )
                    if idx1 >= 0 then
                        if g_view <> 0 then
                            '' Point the view away BEFORE releasing, or the release frees the
                            '' document the view is showing and the next switch walks it.
                            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pWasDoc) )
                            if g_tabDocs(idx1).pSciDoc <> 0 then
                                g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, g_tabDocs(idx1).pSciDoc) )
                            end if
                            if pWasDoc <> 0 then g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pWasDoc) )
                        end if
                        '' ---- AND EVICT WHAT THE SCANNER PUT IN gSymDb.
                        '' A second global this suite writes, after gApp.pDocList. There is
                        '' no Clear on clsSymbolDb -- InstallSet(tier, 0) is the eviction,
                        '' and it hands back what it displaced for the caller to free, which
                        '' is the same contract ShellScan_Buffer honours.
                        scope
                            dim as PARSERESULTSET ptr pGone = gSymDb.InstallSet( ScanTierBuffer, 0 )
                            if pGone then
                                if pGone->pResult then fbcparser_free( pGone->pResult )
                                delete pGone
                            end if
                        end scope

                        '' gApp owns the clsDocument now, so gApp is what frees it.
                        gApp.RemoveDocument( g_tabDocs(idx1).pDoc )
                        g_tabDocs(idx1).pDoc    = 0
                        g_tabDocs(idx1).pSciDoc = 0
                    end if
                    g_nTabDocs = nWasCount
                    g_nTabCur  = nWasCur

                    Check "  and the suite left gApp's list as it found it", _
                          (gApp.GetDocumentCount() = 0), str(gApp.GetDocumentCount())
                end if

                PsFileDelete( wszFile )
            end scope

            '' ---- THE ROW'S TWO DATA SLOTS -----------------------------------------------
            '' SLOT 1 IS A FILE INDEX SINCE STEP 8, not a tab index -- ShellPanel_TabOf now
            '' resolves it through the panel's file table and answers -1 for a file with no
            '' tab, which is a normal answer rather than an error. These assertions are about
            '' the SLOTS THEMSELVES, so they read the raw one (ShellPanel_FileIdxOf); the
            '' resolution is asserted where there are real files and real tabs to resolve.
            '' THESE ASSERTIONS USED TO TEST A BIT-PACKING. PsPlatform's PsListTree had one
            '' data slot per row where tiko's Win32 control has two, so this shell shifted a
            '' tab index and a line number into a single 64-bit integer and carried a mask, a
            '' shift and a clamp to do it. Step 8 gave the control its second slot, so the
            '' encoding is gone and these were REWRITTEN rather than deleted -- what they
            '' guard is the row's identity, which still exists and still matters.
            ''
            '' A WRONG SLOT HERE DOES NOT CRASH. It sends a click to the wrong line of the
            '' wrong file, which looks like a navigation bug anywhere but here.
            scope
                if g_panel <> 0 then
                    g_panel->Clear()
                    dim as DWSTRING sR
                    sR.Utf8 = "row"
                    g_panel->AddString( sR, 7, 42 )
                    Check "a row round-trips its first slot", _
                          (ShellPanel_FileIdxOf(0) = 7), str(ShellPanel_FileIdxOf(0))
                    Check "  and its line", (ShellPanel_LineOf(0) = 42), _
                          str(ShellPanel_LineOf(0))

                    '' TAB 0, LINE 0 IS A LEGITIMATE ROW rather than an "unset" marker -- the
                    '' first line of the first tab. Anything treating 0 as absent would drop
                    '' exactly one bookmark and no other. It mattered more when both lived in
                    '' one integer that was then zero; it is still the value a fresh slot
                    '' holds, so it is still the one an "is it set" test would swallow.
                    g_panel->AddString( sR, 0, 0 )
                    Check "  file 0 line 0 is a real value, not a hole", _
                          (ShellPanel_FileIdxOf(1) = 0) andalso (ShellPanel_LineOf(1) = 0)

                    '' THE BIGGEST LINE AGAINST A NONZERO TAB, and the PAIRING is the point:
                    '' this is what fails if the two slots ever alias, or if either is
                    '' truncated on its way through the control.
                    ''
                    '' Its ancestor read `PackRow(0, huge)` and checked the tab was still 0.
                    '' IT SURVIVED THREE DELIBERATE BREAKAGES -- a 16-bit mask, a missing
                    '' clamp and a shl 16 -- because with the tab at ZERO it reads back zero
                    '' whatever the encoding does. The nonzero tab is why it can fail.
                    g_panel->AddString( sR, SH_MAX_DOCS - 1, 2147483647 )
                    Check "  the biggest line does not disturb the slot beside it", _
                          (ShellPanel_FileIdxOf(2) = SH_MAX_DOCS - 1), _
                          str(ShellPanel_FileIdxOf(2))
                    Check "    and reads back whole", _
                          (ShellPanel_LineOf(2) = 2147483647), str(ShellPanel_LineOf(2))

                    '' A NEGATIVE LINE IS STILL CLAMPED. It no longer sign-extends into the
                    '' tab -- there is nothing beside it to corrupt -- but Scintilla returns
                    '' -1 for "no such line" and -1 still reaches SelectLine, which is the
                    '' defect the clamp was written for and the reason it outlived the
                    '' encoding that hosted it.
                    Check "  a negative line clamps to zero", _
                          (ShellPanel_ClampLine(-1) = 0), str(ShellPanel_ClampLine(-1))
                    Check "    and a real line passes through", _
                          (ShellPanel_ClampLine(42) = 42), str(ShellPanel_ClampLine(42))
                    g_panel->AddString( sR, 3, ShellPanel_ClampLine(-1) )
                    Check "    a clamped row keeps its first slot intact", _
                          (ShellPanel_FileIdxOf(3) = 3) andalso (ShellPanel_LineOf(3) = 0)

                    g_panel->Clear()
                end if
            end scope

            '' ---- ENCODING, WHICH THIS SUITE HAD NOT ONE ASSERTION ABOUT -----------------
            '' The shell has SAVED since step 3 and could not DECODE until step 9, and
            '' nothing here noticed -- 357 assertions and none of them opened a file that
            '' was not UTF-8. These go through the real seam (gAppHost.LoadFileText ->
            '' Doc_ReadFromDisk), not the decoder directly, because psencoding already
            '' covers the decoder with 53 assertions and what was broken was the WIRING.
            scope
                dim as DWSTRING wszDirE = PsPathJoin( PsKnownFolder( PSFOLDER_TEMP ), DWSTRING("tiko_shellenc") )
                PsDirCreate( wszDirE )

                '' ---- UTF-16LE WITH A BOM, which is the case that was mojibake -----------
                '' Built byte by byte rather than by encoding a DWSTRING, so the test does
                '' not depend on the encoder to check the decoder.
                dim as DWSTRING wszU16 = wszDirE & "/probe_utf16.txt"
                dim as string sU16 = chr(&hFF) & chr(&hFE)          '' BOM
                sU16 &= chr(&h41) & chr(&h00)                       '' A
                sU16 &= chr(&hE9) & chr(&h00)                       '' e-acute
                sU16 &= chr(&h42) & chr(&h00)                       '' B
                if PsFileWriteAll( wszU16, sU16 ) then
                    dim as clsDocument ptr pE = gApp.CreateEmptyDocument()
                    if pE <> 0 then
                        '' THROUGH clsDocument.LoadDiskFile, the real caller. These used
                        '' to call gAppHost.LoadFileText -- a seam field deleted in step
                        '' 10, because both implementations had become the same call to
                        '' Doc_ReadFromDisk. Driving the real path is better coverage than
                        '' driving the indirection was.
                        dim as boolean bRead = (pE->LoadDiskFile( wszU16 ) = 0)
                        dim as string sGot = pE->TextBuffer
                        Check "a UTF-16LE file reads", bRead
                        '' THE ENCODING IS RECORDED. Without this the document keeps UTF-8
                        '' and the next save rewrites the container silently.
                        Check "  and the document knows it is UTF-16", _
                              (pE->FileEncoding = FILE_ENCODING_UTF16_BOM), _
                              str(pE->FileEncoding)
                        '' AND THE TEXT IS RIGHT: 4 UTF-8 bytes, because e-acute is two.
                        '' The old reader handed back 8 bytes with NULs in them, which is
                        '' what "mojibake on screen" means in an assertion.
                        Check "  the text decodes to UTF-8", (len(sGot) = 4), _
                              str(len(sGot)) & " bytes"
                        Check "    with the BOM consumed", _
                              (len(sGot) > 0) andalso (asc(left(sGot,1)) = &h41), _
                              str(iif(len(sGot) > 0, asc(left(sGot,1)), 0))

                        '' ROUND TRIP: saving must put the UTF-16 container back.
                        dim as boolean bLossy = false
                        dim as string sBack = Doc_EncodeForDisk( sGot, true, _
                                                    pE->FileEncoding, bLossy )
                        Check "  and it round-trips to the same bytes", (sBack = sU16), _
                              str(len(sBack)) & " vs " & str(len(sU16))
                        gApp.RemoveDocument( pE )
                    end if
                    PsFileDelete( wszU16 )
                end if

                '' ---- INSERTING A FILE MUST NOT RELABEL THE DOCUMENT ---------------------
                '' clsDocument.InsertFile passed @this straight into the reader, so the
                '' INSERTED file's encoding was written onto the HOST document -- insert a
                '' UTF-16 file into a UTF-8 one and the next save rewrote the whole thing in
                '' a container the user never chose. The comment there even said "save the
                '' main file encoding because GetFileTostring may change it", and nothing
                '' saved it.
                ''
                '' InsertFile itself needs a dialog, so what is asserted is the rule it now
                '' follows: reading a file reports its encoding as an OUT PARAMETER and
                '' touches no document.
                scope
                    dim as clsDocument ptr pH = gApp.CreateEmptyDocument()
                    if pH <> 0 then
                        pH->FileEncoding = FILE_ENCODING_UTF8
                        dim as DWSTRING wszIns = wszDirE & "/probe_insert.txt"
                        if PsFileWriteAll( wszIns, chr(&hFF) & chr(&hFE) & chr(&h41) & chr(&h00) ) then
                            dim as string sIns
                            dim as long nInsEnc = FILE_ENCODING_UTF8
                            dim as DWSTRING wszInsErr
                            Doc_ReadFromDisk( wszIns, sIns, nInsEnc, wszInsErr )
                            Check "reading reports the file's encoding OUT", _
                                  (nInsEnc = FILE_ENCODING_UTF16_BOM), str(nInsEnc)
                            Check "  and does NOT touch the document it was read for", _
                                  (pH->FileEncoding = FILE_ENCODING_UTF8), str(pH->FileEncoding)
                            PsFileDelete( wszIns )
                        end if
                        gApp.RemoveDocument( pH )
                    end if
                end scope

                '' ---- UTF-16 BIG ENDIAN, WHICH USED TO BE REWRITTEN AS LITTLE ------------
                '' Read correctly and SILENTLY RE-SAVED LITTLE ENDIAN until 7c step 10,
                '' because tiko had no id for it -- PsCore detected and decoded BE, and
                '' Doc_PsToEnc then folded it onto the LE id.
                dim as DWSTRING wszU16BE = wszDirE & "/probe_utf16be.txt"
                dim as string sU16BE = chr(&hFE) & chr(&hFF)        '' BOM, big endian
                sU16BE &= chr(&h00) & chr(&h41)                     '' A
                sU16BE &= chr(&h00) & chr(&hE9)                     '' e-acute
                sU16BE &= chr(&h00) & chr(&h42)                     '' B
                if PsFileWriteAll( wszU16BE, sU16BE ) then
                    dim as clsDocument ptr pE = gApp.CreateEmptyDocument()
                    if pE <> 0 then
                        pE->LoadDiskFile( wszU16BE )
                        dim as string sGot = pE->TextBuffer
                        Check "a UTF-16BE file gets its OWN encoding id", _
                              (pE->FileEncoding = FILE_ENCODING_UTF16BE_BOM), _
                              str(pE->FileEncoding)
                        Check "  and decodes to the same text as LE would", _
                              (len(sGot) = 4), str(len(sGot)) & " bytes"

                        '' THE ASSERTION THE WHOLE COMMIT EXISTS FOR. Round-tripping to the
                        '' ORIGINAL BYTES is what an id buys; folding onto the LE id also
                        '' passed both assertions above and then wrote a different file.
                        dim as boolean bLossyB = false
                        dim as string sBackB = Doc_EncodeForDisk( sGot, true, _
                                                    pE->FileEncoding, bLossyB )
                        Check "  and re-saves BIG endian, not little", (sBackB = sU16BE), _
                              "BOM " & hex(asc(left(sBackB,1)),2) & " " & _
                                       hex(asc(mid(sBackB,2,1)),2)

                        '' And the two ids are genuinely different files.
                        dim as string sAsLE = Doc_EncodeForDisk( sGot, true, _
                                                    FILE_ENCODING_UTF16_BOM, bLossyB )
                        Check "    which is NOT what the LE id produces", (sBackB <> sAsLE)

                        '' AND IT HAS A NAME OF ITS OWN. Doc_EncodingName's `case else` is
                        '' "ANSI", so a missing arm does not fail loudly -- it puts the
                        '' word ANSI beside a UTF-16 file in the status bar and ticks the
                        '' wrong row in the encoding menu.
                        Check "  and the new id has a name of its own", _
                              (Doc_EncodingName(FILE_ENCODING_UTF16BE_BOM) <> _
                               Doc_EncodingName(FILE_ENCODING_ANSI)) andalso _
                              (Doc_EncodingName(FILE_ENCODING_UTF16BE_BOM) <> _
                               Doc_EncodingName(FILE_ENCODING_UTF16_BOM)), _
                              Doc_EncodingName(FILE_ENCODING_UTF16BE_BOM).Utf8
                        gApp.RemoveDocument( pE )
                    end if
                    PsFileDelete( wszU16BE )
                end if

                '' ---- UTF-8 WITH A BOM: the label must survive, or the BOM is lost --------
                dim as DWSTRING wszU8B = wszDirE & "/probe_utf8bom.txt"
                if PsFileWriteAll( wszU8B, chr(&hEF) & chr(&hBB) & chr(&hBF) & "hello" ) then
                    dim as clsDocument ptr pE = gApp.CreateEmptyDocument()
                    if pE <> 0 then
                        pE->LoadDiskFile( wszU8B )
                        dim as string sGot = pE->TextBuffer
                        Check "a UTF-8 BOM file is labelled as one", _
                              (pE->FileEncoding = FILE_ENCODING_UTF8_BOM), str(pE->FileEncoding)
                        Check "  and the BOM is not in the text", (sGot = "hello"), _
                              str(len(sGot)) & " bytes"
                        gApp.RemoveDocument( pE )
                    end if
                    PsFileDelete( wszU8B )
                end if

                '' ---- ANSI: a byte no UTF-8 sequence can start ---------------------------
                '' 0xE9 alone is invalid UTF-8, so strict validation must reject it and the
                '' ladder must fall through to ANSI. A lenient validator would call this
                '' UTF-8 and the file would round-trip as garbage.
                dim as DWSTRING wszAnsi = wszDirE & "/probe_ansi.txt"
                if PsFileWriteAll( wszAnsi, "caf" & chr(&hE9) ) then
                    dim as clsDocument ptr pE = gApp.CreateEmptyDocument()
                    if pE <> 0 then
                        pE->LoadDiskFile( wszAnsi )
                        dim as string sGot = pE->TextBuffer
                        Check "a byte that cannot be UTF-8 falls through to ANSI", _
                              (pE->FileEncoding = FILE_ENCODING_ANSI), str(pE->FileEncoding)
                        gApp.RemoveDocument( pE )
                    end if
                    PsFileDelete( wszAnsi )
                end if

                '' ---- AN EMPTY FILE IS UTF-8, NOT ANSI -----------------------------------
                '' PsEncDetect's deliberate choice, and it matters: a new empty file that
                '' acquired a codepage would save its first typed character as CP-1252.
                dim as DWSTRING wszEmpty = wszDirE & "/probe_empty.txt"
                if PsFileWriteAll( wszEmpty, "" ) then
                    dim as clsDocument ptr pE = gApp.CreateEmptyDocument()
                    if pE <> 0 then
                        pE->LoadDiskFile( wszEmpty )
                        dim as string sGot = pE->TextBuffer
                        Check "an empty file is UTF-8, not ANSI", _
                              (pE->FileEncoding = FILE_ENCODING_UTF8), str(pE->FileEncoding)
                        gApp.RemoveDocument( pE )
                    end if
                    PsFileDelete( wszEmpty )
                end if

                '' ---- A MISSING FILE REPORTS FAILURE -------------------------------------
                '' This asserted the SEAM's polarity, which was wrong for six steps: tiko's
                '' implementation returned false on success and the shell's returned true,
                '' so LoadDiskFile -- testing for false -- stamped DateFileTime in one
                '' binary and never in the other. THE SEAM IS GONE in step 10, so what is
                '' left to assert is the reader's own status, which is where the meaning
                '' now lives.
                dim as DWSTRING wszGoneE = wszDirE & "/no_such_file.txt"
                scope
                    dim as string sGot
                    dim as long nGoneEnc = -1
                    dim as DWSTRING wszGoneErr
                    Check "a file that is not there reports DOCREAD_FAILED", _
                          (Doc_ReadFromDisk( wszGoneE, sGot, nGoneEnc, wszGoneErr ) = DOCREAD_FAILED)
                    Check "  and says why", (PsLen( wszGoneErr ) > 0), wszGoneErr.Utf8
                    '' AND IT LEAVES A SAFE ENCODING BEHIND rather than whatever the caller
                    '' happened to have in the variable -- a caller that ignores the status
                    '' must not then label a document -1.
                    Check "  and leaves a usable encoding, not the caller's junk", _
                          (nGoneEnc = FILE_ENCODING_UTF8), str(nGoneEnc)
                end scope

                '' AND THE CONSEQUENCE THE INVERSION HAD, asserted rather than described:
                '' LoadDiskFile stamps DateFileTime only on a SUCCESSFUL read, and with the
                '' polarity backwards the shell reached that line on failures and never on
                '' successes. The stamp is what the file-watch and the reload prompt compare
                '' against, so a zero here is a document that can never look stale.
                dim as DWSTRING wszStamp = wszDirE & "/probe_stamp.bas"
                if PsFileWriteAll( wszStamp, "' stamp" ) then
                    dim as clsDocument ptr pS = gApp.CreateEmptyDocument()
                    if pS <> 0 then
                        pS->LoadDiskFile( wszStamp )
                        Check "a successful load stamps DateFileTime", _
                              (pS->DateFileTime <> 0), str(pS->DateFileTime)
                        gApp.RemoveDocument( pS )
                    end if
                    PsFileDelete( wszStamp )
                end if
            end scope

            '' ---- THE KEY VOCABULARY, WHICH app/ COULD NOT ASK ABOUT UNTIL STEP 10 -------
            '' KeyBindings_IsPickListName is the membership half of PickListKeyToValue,
            '' split off so app/modMenuDefinitions.inc could stop including a SHELL header
            '' by relative path. THAT THIS RUNS AT ALL IS HALF THE ASSERTION: this binary
            '' has no keyboard layout, no VkKeyScanEx and no VK_* constants, and the
            '' function it replaced needed all three.
            scope
                Check "a vocabulary name is recognised", _
                      KeyBindings_IsPickListName( "Tab" )
                Check "  and matching is case-insensitive", _
                      KeyBindings_IsPickListName( "tab" ), _
                      "config files carry either spelling"

                '' THE TWO THE ORIGINAL HEADER NAMES. "None" was written into keyless tools
                '' by an older dialog; "PageUp" is a stale spelling of "PgUp" from an older
                '' tiko. Both must be refused, or a menu shows Ctrl+NONE and a binding
                '' silently never fires.
                Check "  a name the pick list never offered is refused", _
                      (KeyBindings_IsPickListName( "None" ) = false)
                Check "    including a stale spelling", _
                      (KeyBindings_IsPickListName( "PageUp" ) = false)
                Check "    and the empty string", _
                      (KeyBindings_IsPickListName( "" ) = false)

                '' IDEMPOTENT, which matters more now than it did: EnsureKeyNames is built
                '' lazily and its definition now sits in a different file from some of its
                '' callers.
                KeyBindings_EnsureKeyNames()
                dim as long nWas = ubound(gKeyNames)
                KeyBindings_EnsureKeyNames()
                Check "  EnsureKeyNames is idempotent across the layer split", _
                      (ubound(gKeyNames) = nWas) andalso (nWas > 0), str(ubound(gKeyNames))
            end scope

            '' ---- FilenameOriginalCase IS REAL NOW ---------------------------------------
            '' It returned its argument for five steps. psfile already asserts the CASE
            '' repair itself, against PsFileRealCase; what is shell-specific and asserted
            '' here is the two things wrapped around it, both of which break silently.
            scope
                dim as DWSTRING wszDirF = PsPathJoin( PsKnownFolder( PSFOLDER_TEMP ), DWSTRING("tiko_shellcase") )
                PsDirCreate( wszDirF )
                dim as DWSTRING wszProbeF = wszDirF & "/CaseProbe.bas"
                if PsFileWriteAll( wszProbeF, "' probe" & chr(13) & chr(10) ) then
                    '' THE CASE, end to end through the shell's own wrapper -- shouted the
                    '' way the parser's string pool shouts.
                    dim as DWSTRING wszShout = wszDirF & "/CASEPROBE.BAS"
                    dim as DWSTRING wszFixed = FilenameOriginalCase( wszShout )
                    Check "FilenameOriginalCase repairs a shouted name", _
                          (PsPathName(wszFixed) = "CaseProbe.bas"), PsPathName(wszFixed).Utf8

                    '' BACKSLASHES, NOT FORWARD ONES. PsCore's canonical form is '/' and
                    '' PsFileRealCase returns it that way; every consumer of this result
                    '' compares against paths that came from a command line or a dialog.
                    '' Forward slashes here do not fail loudly -- the document lookup simply
                    '' misses, and the pane opens a SECOND copy of an open file.
                    Check "  and hands back native separators", _
                          (PsInStr(wszFixed, "/") = 0), wszFixed.Utf8

                    '' THE FALLBACK. PsFileRealCase answers EMPTY for a path that is not
                    '' there, and the names this is asked about are routinely gone -- deleted
                    '' since the scan, or synthetic. Returning that empty string would blank
                    '' a filename in the panel and turn a symbol-database key into "".
                    dim as DWSTRING wszGone = wszDirF & "/no_such_file_at_all.bas"
                    Check "  a path that is not there comes back UNCHANGED", _
                          (FilenameOriginalCase( wszGone ) = wszGone), _
                          FilenameOriginalCase( wszGone ).Utf8

                    dim as DWSTRING wszEmpty
                    Check "  and an empty path stays empty rather than becoming a cwd", _
                          (PsLen( FilenameOriginalCase( wszEmpty ) ) = 0)

                    PsFileDelete( wszProbeF )
                end if
            end scope

            '' ---- THE TWO WORKAROUNDS THAT WENT WITH THE PACKING -------------------------
            '' Both were shell-side compensations for a control that could not express what
            '' the panel needed. Both are now one call at install, and these assert the
            '' property each workaround kept failing to hold.
            scope
                if g_panel <> 0 then
                    '' STRIPING SURVIVES A THEME LOAD NOW. The old fix set clrRowAlt = clrBack
                    '' and OnThemeChanged put it straight back, so it had to be reapplied
                    '' after every theme apply -- and the startup path above HAS applied one
                    '' by the time this runs, which is what makes this assertion worth having
                    '' rather than a restatement of the setter.
                    Check "striping is off, and a theme load did not undo it", _
                          (g_panel->bAltRows = false)

                    '' AND A PROGRAMMATIC SELECTION IS NOT A CLICK. Every reload restores the
                    '' selection through SetCurSel; the row handler acts only on
                    '' PSLT_SRC_MOUSE, so what this asserts is that a reload cannot leave the
                    '' panel in a state where the next notification is mistaken for one.
                    ''
                    '' WHAT IT DOES NOT ASSERT is the gate itself -- no mouse or key event
                    '' reaches a windowless surface, so "arrowing does not jump" is verified
                    '' by hand and stated as such rather than implied by this passing.
                    dim as DWSTRING sR
                    sR.Utf8 = "row"
                    g_panel->AddString( sR, 0, 0 )
                    g_panel->AddString( sR, 0, 1 )
                    g_panel->SetCurSel(1)
                    Check "a restored selection does not read as a mouse click", _
                          (g_panel->GetSelSource() <> PSLT_SRC_MOUSE)
                    g_panel->Clear()
                end if
            end scope

            '' ---- THE PARSE DEBOUNCE -----------------------------------------------------
            '' The POLICY is a pure function -- (notification, modification type, loading)
            '' -> (ignore | restart | scan) -- which is the only reason any of this is
            '' reachable without a keyboard. tiko's version of the same rule is spread over
            '' three files and can only be exercised by typing into a window.
            scope
                '' A REAL EDIT RESTARTS IT. Both halves: the character path and the
                '' modification path, which is what tiko arms from.
                Check "a typed character restarts the debounce", _
                      (ShellScan_DebouncePolicy(SCN_CHARADDED, 0, false) = SHDB_RESTART)
                Check "  and so does an insertion", _
                      (ShellScan_DebouncePolicy(SCN_MODIFIED, SC_MOD_INSERTTEXT, false) = SHDB_RESTART)
                Check "  and a deletion", _
                      (ShellScan_DebouncePolicy(SCN_MODIFIED, SC_MOD_DELETETEXT, false) = SHDB_RESTART)

                '' ---- THE LOAD-BEARING ONE. SCN_MODIFIED IS NOT "the text changed":
                '' Scintilla raises it for STYLING too, and this binary styles continuously.
                '' A policy that keyed on the bare code would rearm the timer on every
                '' colouring pass -- so the parse would either never fire, or fire for an
                '' edit that never happened.
                Check "  but STYLING does not, and that is the whole rule", _
                      (ShellScan_DebouncePolicy(SCN_MODIFIED, SC_MOD_CHANGESTYLE, false) = SHDB_IGNORE), _
                      str(ShellScan_DebouncePolicy(SCN_MODIFIED, SC_MOD_CHANGESTYLE, false))
                Check "    nor a marker change", _
                      (ShellScan_DebouncePolicy(SCN_MODIFIED, SC_MOD_CHANGEMARKER, false) = SHDB_IGNORE)

                '' The timer firing is the only thing that scans.
                Check "  the timer firing is what scans", _
                      (ShellScan_DebouncePolicy(0, 0, false) = SHDB_SCAN)

                '' NOTHING WHILE A FILE IS BEING FILLED. AssignTextBuffer inserts the whole
                '' document, which is a real edit by every test above -- so without this a
                '' load would arm a timer to re-parse the file it had just parsed.
                Check "  and NOTHING at all while a file is loading", _
                      (ShellScan_DebouncePolicy(SCN_CHARADDED, 0, true) = SHDB_IGNORE) andalso _
                      (ShellScan_DebouncePolicy(SCN_MODIFIED, SC_MOD_INSERTTEXT, true) = SHDB_IGNORE) andalso _
                      (ShellScan_DebouncePolicy(0, 0, true) = SHDB_IGNORE)

                '' An unknown notification is ignored rather than acted on -- the same
                '' default-safe shape as the file dialog's routing table.
                Check "  an unrelated notification does nothing", _
                      (ShellScan_DebouncePolicy(SCN_PAINTED, 0, false) = SHDB_IGNORE)
                Check "    and IGNORE is what a zeroed action holds", (SHDB_IGNORE = 0)
            end scope

            '' ---- THE TIMER ITSELF, ON THE SYNTHETIC CLOCK -------------------------------
            '' PsTimer's own suite drives a fake clock rather than sleeping, and so does
            '' this: 500ms of wall time in a self-test is 500ms nobody gets back, and a test
            '' that sleeps is a test that is flaky on a loaded machine.
            scope
                '' ---- START FROM A KNOWN STATE, and this is not defensive padding.
                '' The notification sink is LIVE by the time this runs -- ShellScan_Install
                '' ran in BuildTree -- so the edits earlier assertions make have already
                '' armed the debounce, and this scope's first version measured against a
                '' baseline it assumed was zero. It failed with "(2)" and "(1)", which says
                '' nothing about the timer and everything about the assumption.
                PsTimerKillProc( @g_sciSink, SH_TIMER_PARSE )

                dim as ulongint nT0 = PsTimerNow()
                dim as long nCountWas = PsTimerCount()

                ShellScan_ArmTimer()
                Check "arming the debounce registers exactly one timer", _
                      (PsTimerCount() = nCountWas + 1), str(PsTimerCount())

                '' RE-ARMING REPLACES, IT DOES NOT STACK. Every keystroke arms it, so a
                '' version that stacked would leak one timer per character typed --
                '' PsTimer.bi calls that out as the reason re-arm replaces.
                ShellScan_ArmTimer()
                ShellScan_ArmTimer()
                Check "  and re-arming replaces rather than stacks", _
                      (PsTimerCount() = nCountWas + 1), str(PsTimerCount())

                '' NOT DUE BEFORE THE PAUSE ELAPSES.
                PsTimerSetClock( nT0 )
                PsTimerService( nT0 + SH_PARSER_DEBOUNCE_MS - 1 )
                Check "  not due one millisecond early", _
                      (PsTimerCount() = nCountWas + 1), str(PsTimerCount())

                '' AND GONE AFTER IT FIRES, because ShellScan_DebounceFire kills it -- the
                '' proc form of PsTimer is not purged by anything else, and a timer that
                '' survived would parse again 500ms later, forever.
                PsTimerService( nT0 + SH_PARSER_DEBOUNCE_MS + 1 )
                Check "  fires once and is gone", (PsTimerCount() = nCountWas), _
                      str(PsTimerCount())

                PsTimerKillProc( @g_sciSink, SH_TIMER_PARSE )
            end scope

            '' ---- THE BOOKMARK COMMANDS WITH NOTHING OPEN --------------------------------
            '' Every one of these is reachable from the keyboard on the first frame -- F2 is
            '' a bound accelerator in tiko and the menu rows are not disabled here -- so
            '' each has to survive a null document rather than fault on it. The document
            '' model would dereference it immediately.
            scope
                dim as long nWasCur = g_nTabCur
                g_nTabCur = -1
                Check "no document open: Toggle does nothing", (ShellTabs_CurrentDoc() = 0)
                ShellBookmarks_Toggle()
                ShellBookmarks_Next()
                ShellBookmarks_Prev()
                ShellBookmarks_ClearCurrent()
                Check "  and Next, Prev and Clear All survive it", true
                g_nTabCur = nWasCur

                '' NO SURFACE MEANS THE DESTRUCTIVE ONE IS REFUSED, never assumed -- the
                '' same rule as ConfirmExit and the lossy-save prompt, and the same trap:
                '' called with a live surface this would raise a real modal in a windowless
                '' run and block forever.
                dim as PsSurface ptr pWas = g_pSurf
                g_pSurf = 0
                Check "  and Clear-All-Documents refuses with nothing to ask with", _
                      (ShellBookmarks_ClearAllDocs() = false)
                g_pSurf = pWas

                '' THE COMPOSITION, which is everything about that box a suite can reach.
                dim as PsMessageBox box
                BuildClearAllBookmarksBox( box )
                Check "  the clear-all box offers Yes, No and Cancel", _
                      (box.GetButtonCount() = 3), str(box.GetButtonCount())
                '' THE LOAD-BEARING ONE, and it is a DIVERGENCE FROM tiko: tiko passes no
                '' explicit default here, so the first button wins and Return deletes every
                '' bookmark in every open document.
                Check "    with NO as the default, not Yes", _
                      (box.ButtonId(box.GetDefaultButton()) = MBX_ID_NO), _
                      str(box.GetDefaultButton())
                Check "    and Escape cancels", (box.ResolveCancelId() = MBX_ID_CANCEL)
                Check "    neither caption nor text is blank", _
                      (len(box.sCaption) > 0) andalso (len(box.sText) > 0)
            end scope

            '' ---- GROUP J: THE FILE DIALOG'S NESTED PUMP --------------------------------
            '' A THIRD nested pump, in a process whose previous two produced two silent
            '' defects. Its decisions are a pure function for exactly that reason, and this
            '' drives the table exhaustively -- every kind, both values of bMine -- so a hole
            '' fails here rather than in front of a user with a dialog open.
            scope
                dim as PsEvent ev

                ev.kind = PSEV_QUIT
                Check "a quit during a file dialog aborts AND re-posts", _
                      (ShellFileDlgRoute(@ev, true) = SHFD_ABORT_REPOST_QUIT)
                Check "  whoever it was addressed to", _
                      (ShellFileDlgRoute(@ev, false) = SHFD_ABORT_REPOST_QUIT)

                '' THE ONE PLACE THIS DIFFERS FROM PsModalRoute, and it is the whole reason
                '' the table is separate: a modal dialog's own close is a CANCEL, because the
                '' dialog is ours. A file dialog is the OPERATING SYSTEM'S -- closing it
                '' produces a callback, not an event -- so the only close that can arrive
                '' here is the SHELL'S, and that ends everything.
                ev.kind = PSEV_CLOSE
                Check "the shell's own close ends the wait", _
                      (ShellFileDlgRoute(@ev, true) = SHFD_ABORT_REPOST_QUIT)
                Check "  and a close for anything else is dropped", _
                      (ShellFileDlgRoute(@ev, false) = SHFD_DROP)

                ev.kind = PSEV_RESIZE
                Check "the shell's resize is handled behind the dialog", _
                      (ShellFileDlgRoute(@ev, true) = SHFD_RESIZE_SELF)
                Check "  but nobody else's is", _
                      (ShellFileDlgRoute(@ev, false) = SHFD_DROP)

                '' EVERYTHING ELSE IS DROPPED, both ways. The OS dialog has the user, and
                '' dispatching into a window they cannot reach is what PsModalRoute warns
                '' about. Asserted over the kinds this shell actually produces rather than
                '' by inspection.
                dim as long kinds(0 to 7) = { PSEV_KEY_DOWN, PSEV_KEY_UP, PSEV_TEXT_INPUT, _
                                              PSEV_MOUSE_DOWN, PSEV_MOUSE_UP, PSEV_MOUSE_MOVE, _
                                              PSEV_MOUSE_WHEEL, PSEV_TIMER }
                dim as boolean bAllDropped = true
                for i as long = 0 to 7
                    ev.kind = kinds(i)
                    if ShellFileDlgRoute(@ev, true)  <> SHFD_DROP then bAllDropped = false
                    if ShellFileDlgRoute(@ev, false) <> SHFD_DROP then bAllDropped = false
                next
                Check "every other event kind is dropped, both ways", bAllDropped

                '' A null is unreachable from the loop, which is why it is asserted: a
                '' decision table that faults on one cannot be fuzzed.
                Check "  and a null event is dropped rather than faulted on", _
                      (ShellFileDlgRoute(0, true) = SHFD_DROP)

                '' DROP MUST BE THE ZERO VALUE. If the enum were reordered so that
                '' ABORT_REPOST_QUIT became what a zeroed variable holds, an uninitialised
                '' action would kill the application.
                Check "  and the safe action is the one a zeroed variable holds", _
                      (SHFD_DROP = 0)
            end scope

            '' NOT REACHABLE FROM HERE, and listed so the group is not read as complete:
            '' that an OPEN MENU is closed before its command runs -- PsMenuHost takes the
            '' popup's command slot precisely so it can close the chain first, and none of
            '' it is reachable because OpenRoot declines without a window. Nor that the OS
            '' half of modality holds, which is g_plat.window.SetModal and a compositor.

            '' NO SURFACE MEANS "DO NOT QUIT", and g_pSurf is nulled to get there.
            ''
            '' THIS HUNG THE SELF-TEST ON FIRST WRITE, and the reason is worth more than the
            '' assertion. `--selftest` is described in this file's header as WINDOWLESS, and
            '' that is true of the SHELL's surface -- but PsPlatformInit is called (PsSciView
            '' needs the text engine), so PsModalHost.Run can and does create a window of its
            '' own and pump it. Called with the live g_pSurf, ConfirmExit put a real modal on
            '' screen and blocked forever waiting for a button nobody was going to press.
            ''
            '' "Windowless" is a property of what this test BUILDS, not a guarantee about
            '' what it can be made to do. Any future assertion that reaches a Show/Run/DoModal
            '' has the same trap under it.
            scope
                dim as PsSurface ptr pWas = g_pSurf
                g_pSurf = 0
                Check "and with no surface the exit is refused, not assumed", _
                      (ConfirmExit() = false)
                Check "  so nothing requested a quit", (g_bQuitRequested = false)
                g_pSurf = pWas
            end scope
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
            '' THE STRIP STARTS UNDER THE MENUBAR AND THE TREE STARTS UNDER THE STRIP.
            '' This read `g_panel->bounds.y = 52` until step 20 put the pane switcher above
            '' it. Asserted as a PAIR rather than by moving the number: the property that
            '' matters is that the two are stacked with no gap and no overlap, and a lone
            '' `g_panel->bounds.y = 82` would hold just as well with the strip somewhere
            '' else entirely.
            Check "the panel strip starts under the menubar", (g_panelMenu->bounds.y = 52)
            Check "  and the tree starts under the strip", _
                  (g_panel->bounds.y = g_panelMenu->bounds.y + g_panelMenu->bounds.h), _
                  str(g_panel->bounds.y) & " vs " & _
                  str(g_panelMenu->bounds.y + g_panelMenu->bounds.h)
            Check "    both spanning the panel's width", _
                  (g_panelMenu->bounds.x = g_panel->bounds.x) andalso _
                  (g_panelMenu->bounds.w = g_panel->bounds.w)

            '' ---- AND TOGETHER THEY ARE tiko's PANEL BAND -- 7c step 20 ----------------
            ''
            '' THE ORACLE HAS ONE ROW WHERE THIS BINARY NOW HAS TWO, and that is not a
            '' mismatch: tiko's HWND_FRMPANEL is a CONTAINER whose children are the strip
            '' and the tree, and modLayoutDump dumps the container. This shell has no
            '' container -- both sit on the root -- so the thing that has to equal tiko's
            '' `PANEL 0,52,413,854` is their UNION.
            ''
            '' Asserted as the union rather than by editing the oracle's PANEL row, because
            '' the union is the property tiko actually pins. Either rect alone can be wrong
            '' in a way the other cancels, and a number copied into the oracle by hand would
            '' hide exactly that -- which is what the oracle README means by "regenerate it,
            '' don't hand-edit it".
            '' A NUMBER, NOT A RELATION, and it is here because the three relations above
            '' it all hold with a ZERO-HEIGHT strip -- proved by reverting the height to 0
            '' and watching the suite stay green. Step 1 recorded the same trap: twenty-one
            '' relation assertions passed while the UI was visibly unscaled.
            Check "    the strip is SH_PANELMENU_H, scaled", _
                  (g_panelMenu->bounds.h = PsScaleBy(SH_PANELMENU_H, 1.75)), _
                  str(g_panelMenu->bounds.h) & " wanted " & str(PsScaleBy(SH_PANELMENU_H, 1.75))
            Check "      and together they are tiko's whole panel band", _
                  (g_panelMenu->bounds.y = 52) andalso _
                  (g_panel->bounds.y + g_panel->bounds.h = 854) andalso _
                  (g_panelMenu->bounds.h + g_panel->bounds.h = 802), _
                  str(g_panelMenu->bounds.h) & " + " & str(g_panel->bounds.h)
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
            '' g_panelMenu JOINED THIS LIST IN STEP 20, and it had to: the strip takes its
            '' band out of the panel's rectangle, so leaving it out made the coverage sum
            '' short by exactly the strip's area and turned a correct layout into a failure
            '' naming three gaps when there are still only three.
            dim as PsWidget ptr kids(0 to 14) = { _
                g_menubar, g_status, g_panelMenu, g_panel, g_splitPanel, g_tabs, _
                g_topTabsMenu, g_barInfo, g_barFind, g_barReplace, g_splitOutput, _
                g_output, g_fip, g_view, g_vscroll }

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

        '' ---- THE SYMBOL MARGIN HAS A WIDTH, WHICH IS WHY BOOKMARKS ARE ICONS ----------
        '' REPORTED BY THE AUTHOR: a bookmarked line came out with its whole background
        '' highlighted, where tiko shows a small icon in the margin.
        ''
        '' NOT A COLOUR BUG. Scintilla draws a marker that NO MARGIN DISPLAYS by changing
        '' the background colour of its line -- documented fallback behaviour. Margin 1 had
        '' width 0 here, so every marker in this binary painted as a stripe, and breakpoints
        '' and the debugger's current line would have done the same as soon as anything set
        '' one.
        ''
        '' The width is what the suite can see. The ICON is not: no assertion in either repo
        '' looks at a pixel, so what SC_MARK_VERTICALBOOKMARK actually renders as is the
        '' author's to confirm.
        Check "  the symbol margin has a width, so markers are icons not stripes", _
              (g_view->Msg(SCI_GETMARGINWIDTHN, 1) > 0), _
              str(g_view->Msg(SCI_GETMARGINWIDTHN, 1))
        Check "    and it scales with the surface", _
              (g_view->Msg(SCI_GETMARGINWIDTHN, 1) = PsScaleBy(16, surf.fScale)), _
              str(g_view->Msg(SCI_GETMARGINWIDTHN, 1)) & " at " & str(surf.fScale)
        '' MARGIN 2 STAYS AT ZERO. It carries SC_MASK_FOLDERS in tiko and this binary has no
        '' folding UI; a width here would put an empty fold strip beside every document.
        Check "    while the fold margin stays closed", _
              (g_view->Msg(SCI_GETMARGINWIDTHN, 2) = 0), _
              str(g_view->Msg(SCI_GETMARGINWIDTHN, 2))

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
            ''
            '' IDM_FILENEW, NOT IDM_FILESAVE. This drove Save until commit 7 gave Save a
            '' handler -- at which point it still PASSED, because the layout does not move
            '' either way, and would have gone on passing while asserting nothing about the
            '' thing it names. New File is genuinely unhandled here; when it stops being,
            '' this line has to move again.
            dim as long yWas = g_rcDoc.y
            OnMenuCommand( 0, IDM_FILENEW, 0 )
            Check "an unhandled command changes nothing", (g_rcDoc.y = yWas)

            '' SAVE WITH NOTHING OPEN IS REACHED THROUGH THE COMMAND, not just through
            '' ShellTabs_Save -- Ctrl+S is live from the first frame, before any file is.
            OnMenuCommand( 0, IDM_FILESAVE, 0 )
            Check "  and neither does Save with no document", (g_rcDoc.y = yWas)
        end scope

        '' NO WINDOW WAS CREATED, and the surface says so. hWin is the marker PsModalHost
        '' reads to find a dialog's parent, so a surface that acquired one by accident here
        '' would be a real defect rather than an untidy test.
        Check "the surface is windowless", (surf.hWin = 0)

        '' ---- THE PROJECT FILE'S ACTIVE-TAB INDEX -----------------------------------
        '' Tabs_SaveActiveIndex was clsTopTabCtl.SaveActiveTabIndex until 7c step 13 --
        '' twenty lines of counting inside a Win32 control, with no Win32 in them, which
        '' nothing could reach to test. It is app-layer logic now and this binary runs it
        '' through its own tab array.
        ''
        '' THE RULE IS SUBTLE AND IS WHY IT IS NOT A SEAM FIELD: a project records only
        '' documents with a file on disk, so the saved index counts positions in the
        '' WRITTEN list, not in the tab bar. Made a fifth seam field it would exist twice,
        '' in two hosts, with nothing checking the two agreed.
        scope
            '' TWO TABS OF ITS OWN, and the first draft of this block did NOT do that --
            '' it assumed the files opened earlier in this suite were still tabbed. They
            '' are not; the earlier scope restores the tab state it found. Three
            '' assertions failed and one PASSED FOR THE WRONG REASON: "TabDocAt agrees
            '' with the array" compared 0 with 0.
            dim as DWSTRING wszDir = PsPathJoin( PsKnownFolder( PSFOLDER_TEMP ), DWSTRING("tiko_shelltabs") )
            PsDirCreate( wszDir )
            dim as DWSTRING wszA = wszDir & "/tabseam_a.bas"
            dim as DWSTRING wszB = wszDir & "/tabseam_b.bas"
            dim as boolean bW = PsFileWriteAll( wszA, !"' a\n" )
            bW = bW andalso PsFileWriteAll( wszB, !"' b\n" )

            if bW = false then
                Check "the tab-seam probes could be written to %TEMP%", false, wszA.Utf8
            else
                dim as long nWasCount = g_nTabDocs
                dim as long idxA = ShellTabs_Open( wszA )
                dim as long idxB = ShellTabs_Open( wszB )
                Check "two probe files opened into tabs", (idxA >= 0) andalso (idxB > idxA)

                '' ---- THE SEAM READS THE SHELL'S OWN ARRAY -------------------------
                Check "TabCount is the tab count", (gAppHost.TabCount() = g_nTabDocs)
                Check "  and it grew by two", (g_nTabDocs = nWasCount + 2)
                Check "TabDocAt returns the same pointer the array holds", _
                      (gAppHost.TabDocAt(idxA) = g_tabDocs(idxA).pDoc) andalso _
                      (gAppHost.TabDocAt(idxA) <> 0)

                '' OUT OF RANGE IS 0, BOTH ENDS. This is the fold that removed a separate
                '' IsValidTab call, so it is the assertion that keeps the fold honest.
                Check "a negative index is no document", (gAppHost.TabDocAt(-1) = 0)
                Check "  and so is one past the end", (gAppHost.TabDocAt(g_nTabDocs) = 0)

                Check "TabIndexOfDoc finds a tabbed document", _
                      (gAppHost.TabIndexOfDoc( g_tabDocs(idxB).pDoc ) = idxB)
                Check "  and answers -1 for one that is not tabbed", _
                      (gAppHost.TabIndexOfDoc( 0 ) = -1)

                '' ---- AND THE RULE THAT IS WHY THIS IS LOGIC, NOT A FIFTH FIELD -----
                '' A project records only documents with a file on disk, so the saved
                '' index counts positions in the WRITTEN list, not in the tab bar.
                dim as long nCur = gAppHost.TabActiveIndex()
                Check "the newly opened tab is the active one", (nCur = idxB)
                Check "with every tab on disk, saved index = tab index", _
                      (Tabs_SaveActiveIndex() = nCur)

                '' THE DISCRIMINATING CASE. Everything above passes under an
                '' implementation that simply returns the tab index.
                dim as clsDocument ptr pA = g_tabDocs(idxA).pDoc
                dim as boolean bWasA = pA->IsNewFlag
                pA->IsNewFlag = true
                Check "an untitled tab AHEAD of it is not counted", _
                      (Tabs_SaveActiveIndex() = nCur - 1)
                pA->IsNewFlag = bWasA

                '' An untitled ACTIVE tab has no honest answer, and 0 is the harmless one.
                dim as clsDocument ptr pB = g_tabDocs(idxB).pDoc
                dim as boolean bWasB = pB->IsNewFlag
                pB->IsNewFlag = true
                Check "an untitled ACTIVE tab lands on the first restored document", _
                      (Tabs_SaveActiveIndex() = 0)
                pB->IsNewFlag = bWasB
            end if
        end scope

        '' ---- AND tiko's OWN ENCODING SUITE, WHICH THIS BINARY CAN NOW RUN ----------
        '' 27 assertions that lived in src/ until 7c step 9 because they named
        '' WideCharToMultiByte, CFileStream and GetFileToString. They are in app/ now, so
        '' the portable shell runs them HEADLESSLY -- where tiko can only run them behind
        '' TIKO_ENCODING_SELFTEST inside a started GUI, which is why they had not been run
        '' once during this step until this line existed.
        ''
        '' IT KEEPS ITS OWN PASS/FAIL COUNTERS AND PRINTS ITS OWN LINE. Not merged into
        '' g_nPass: two suites with one counter is how a failure in the quiet one gets
        '' read as a rounding error in the loud one.
        setenviron "TIKO_ENCODING_SELFTEST=1"
        Encoding_RunSelfTest()

        print ""
        print "  " & g_nPass & " passed, " & g_nFail & " failed"
        ShellScan_StopWorker()
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
        '' A command handler cannot reach `bRunning`, so the Exit confirmation parks its
        '' answer and it is read here. Checked at the TOP: the handler ran inside the
        '' previous iteration, and the rest of that iteration -- relayout, paint, present --
        '' still had to complete or the shell would exit having left the modal's damage on
        '' screen for a frame.
        if g_bQuitRequested then exit do

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

                '' ---- A WORKER FINISHED (7c step 7) ----------------------------------
                '' The scan runs on another thread now and posts this when it has a result.
                '' PSEV_USER is documented as "application-defined, posted from worker
                '' threads" (PsEvent.bi:90) and this is its first use in either binary.
                ''
                '' EVERYTHING THE RESULT TOUCHES HAPPENS HERE, on the UI thread: gSymDb, the
                '' panel, the documents. The worker only handed over a pointer.
                ''
                '' NOT GUARDED BY bMine: a posted event has no window, so its surface is 0
                '' and the test above already treats that as "mine". Said out loud because
                '' the next person to add a case here will wonder.
                case PSEV_USER
                    if ev.user.code = SH_USER_SCAN_DONE then ShellScan_Collect()

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
                                dim as long nCmd = AccelFind( @ev )
                                if nCmd <> 0 then
                                    NoteKeyConsumed( surf, @ev )
                                    OnMenuCommand( 0, nCmd, 0 )
                                else
                                    surf.Dispatch( @ev )
                                end if
                            else
                                NoteKeyConsumed( surf, @ev )
                            end if
                        else
                            NoteKeyConsumed( surf, @ev )
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
    '' THE WORKER IS JOINED BEFORE THE RUNTIME GOES AWAY. A thread still running at exit
    '' reads globals that are being torn down, on a schedule nobody controls -- which is how
    '' a clean quit becomes an intermittent crash. All three exit paths do this.
    ShellScan_StopWorker()
    TE_Free( g_te )
    PsPlatformShutdown()
    end 0
