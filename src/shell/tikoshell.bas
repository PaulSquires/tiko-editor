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
#include once "platform/PlatformInit.inc"
#include once "ui/core/PsDispatch.inc"


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
