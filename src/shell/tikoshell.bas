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


    '' Binds g_plat and starts SDL. FALSE here means the backend could not initialise --
    '' which on this commit is the whole question being asked, so it is reported rather
    '' than merely returned.
    if PsPlatformInit() = false then
        print "tikoshell: PsPlatformInit FAILED"
        end 1
    end if

    print "tikoshell: backend " & g_plat.caps.backendName.Utf8

    '' A PsSurface is a plain value type -- no Create, no Show, and no window until a host
    '' marries it to one through g_plat.window.Create. Constructed here only to prove the
    '' widget core links alongside the platform layer; hWin stays NULL, which is the
    '' documented headless state.
    dim as PsSurface surf
    surf.Resize( 800, 600 )

    print "tikoshell: surface " & str(surf.w) & "x" & str(surf.h) & _
          "  root " & str(cast(integer, surf.pRoot)) & _
          "  hWin " & str(cast(integer, surf.hWin))

    PsPlatformShutdown()

    print "tikoshell: OK"
    end 0
