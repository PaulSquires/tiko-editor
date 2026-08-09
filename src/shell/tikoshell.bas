'' ========================================================================================
'' tikoshell -- phase 7c's shell binary. COMMIT 1: it links and it starts. Nothing else yet.
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
'' ---- WHAT COMMIT 1 DELIBERATELY DOES NOT DO --------------------------------------------
''
'' No window, no widgets, no event loop, no app layer. Its entire job is to retire one
'' unknown -- whether tiko's build can link SDL3 at all -- BEFORE anything is written that
'' depends on the answer. tiko has never linked SDL3: the Win32 host bridge exists precisely
'' so tiko keeps its own window and message loop, and tiko.exe does not call PsPlatformInit
'' at any point. It turns out to need one extra include root and no new library flag; see
'' _compile_shell.bat.
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

dim shared as PsTextEngine  g_te
dim shared as PsBufferPaint g_pnt
dim shared as PsSurface ptr g_pSurf
dim shared as PsMenuHost    g_menus
dim shared as long g_nPass, g_nFail


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
'' The tree. ONE full-window stub for now; commit 8 replaces this with the real band walk.
'' ---------------------------------------------------------------------------------------
dim shared as ShellStub ptr g_body

sub BuildTree( byref surf as PsSurface )
    dim as PsWidget ptr root = new PsWidget
    surf.SetRoot( root )                      '' the surface takes ownership

    g_body = new ShellStub( @"tiko shell -- nothing here yet", PSTHEME_BACKGROUND )
    root->AddChild( g_body )
end sub

sub LayoutAll( byref surf as PsSurface )
    if g_body then g_body->SetBounds( PsRc(0, 0, surf.w, surf.h) )
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
    dim as string sFont = FontPath()
    if TE_Init( g_te, strptr(sFont), 14 ) = 0 then
        print "tikoshell: the text engine would not start -- " & sFont
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
        Check "  one child so far", (surf.pRoot->ChildCount() = 1), _
              str(surf.pRoot->ChildCount())
        Check "the body fills the surface", _
              (g_body->bounds.x = 0) andalso (g_body->bounds.y = 0) andalso _
              (g_body->bounds.w = surf.w) andalso (g_body->bounds.h = surf.h), _
              str(g_body->bounds.w) & "x" & str(g_body->bounds.h)

        '' A RESIZE MUST RE-LAY-OUT. Cheap to assert and the whole reason the layout is a
        '' function of the surface rather than of a constant -- commit 8 leans on this.
        surf.Resize( 640, 480 )
        LayoutAll( surf )
        Check "a resize re-lays out", _
              (g_body->bounds.w = 640) andalso (g_body->bounds.h = 480), _
              str(g_body->bounds.w) & "x" & str(g_body->bounds.h)
        surf.Resize( SH_W, SH_H )
        LayoutAll( surf )

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
    '' THE REQUESTED SIZE IS CLAMPED TO THE DISPLAY. SH_W/SH_H are DESIGN units and the
    '' platform takes them as its own, so on a 1.75-scale display 1100x700 comes back as
    '' 1925x1225 -- larger than the screen, with the bottom edge unreachable and no way to
    '' drag it into view. Copied from ideshell, which is the only demo in that tree that
    '' does this; the others get away with it only because their defaults are small enough.
    dim as long nWantW = SH_W, nWantH = SH_H
    scope
        dim as PsMonitorInfo mi
        if g_plat.monitors.Describe(0, @mi) then
            '' 90% of the USABLE area, not the full bounds: usable already has the taskbar
            '' out of it, and a window exactly its size has no margin left to grab.
            dim as long wMax = clng(mi.usable.w * 0.9)
            dim as long hMax = clng(mi.usable.h * 0.9)
            if (wMax > 320) andalso (nWantW > wMax) then nWantW = wMax
            if (hMax > 240) andalso (nWantH > hMax) then nWantH = hMax
        end if
    end scope

    dim as PsSurfaceDesc desc
    desc.kind        = PSSURF_TOPLEVEL
    desc.w           = nWantW
    desc.h           = nWantH
    desc.title.Utf8  = "tiko -- phase 7c shell"
    desc.bResizable  = true
    dim as PsSurfaceHandle win = g_plat.window.Create( @desc )
    if win = 0 then
        print "tikoshell: no window"
        end 1
    end if

    '' MARRYING THE SURFACE TO THE WINDOW IS THREE MANUAL STEPS and hWin is the one ideshell
    '' forgets. PsModalHost reads it to parent a dialog, so a shell that skips it gets
    '' parentless message boxes much later and for no visible reason.
    surf.hWin = win
    scope
        dim as single sc = g_plat.window.ScaleOf(win)
        if sc > 0 then surf.fScale = sc
    end scope
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
                        surf.Dispatch( @ev )
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
