' ########################################################################################
' modBuildService.bi
'
' Owns whether a build is running, and composes the command line one is started with.
'
' WHAT THIS IS, AND WHAT IT DELIBERATELY IS NOT.
'
' The audit asked for a build SERVICE -- a state machine taking a request struct and posting
' progress, with the UI merely subscribing -- so that build orchestration stops being fused
' with windows, cursors and message boxes. That full separation is not done here, and the
' reason is worth writing down rather than leaving as a silent gap:
'
'   - the benefit it was recommended FOR has already been obtained another way. It was
'     wanted so the command-line construction could be asserted; those functions are pure
'     and live here now, with assertions covering them, without moving code_Compile.
'   - what remains is a large mechanical rewrite of the single path in this program that
'     cannot be exercised without a human pressing F5, and that path has already produced
'     one regression during this work. Restructuring it blind is how a second one happens.
'
' So this file takes the part that is both valuable and safe: build state gets an OWNER.
' It used to be gApp.IsCompiling -- one boolean in a 51-field flag bag, whose only reader
' for years was WM_SETCURSOR, so it drove the busy cursor and nothing else while LOOKING
' like a guard. Now there is one place that says whether a build is running, one way to
' start one, and one way to finish.
'
' BuildService_Begin is deliberately a TEST-AND-SET rather than a setter: "is a build
' running?" and "claim it" have to be one operation, or the guard is a race waiting for
' someone to widen the window between them. Everything here runs on the UI thread, so this
' is discipline rather than atomicity -- said plainly so nobody mistakes it for the latter.
' ########################################################################################

#pragma once

enum
    BUILD_IDLE = 0
    BUILD_RUNNING
end enum

declare function BuildService_State() as long
declare function BuildService_IsRunning() as boolean
' Claims the build slot. FALSE means one is already running and the caller must not proceed.
declare function BuildService_Begin() as boolean
declare sub      BuildService_End()

' Compose the lpCommandLine for a child process: quoted exe as argv[0], then the switches.
' The first token is ALWAYS eaten by the child, so switches cannot be passed alone -- see the
' definition, which records what was measured.
declare function BuildChildCommandLine( byval wszExe as DWSTRING, byval wszParams as DWSTRING ) as DWSTRING

' The "-m" pair for the main module: module NAME after -m, and the SOURCE FILE as an input.
' Both are required and they are not the same string.
declare function BuildMainModuleArgs( byval wszMainName as DWSTRING, byval wszMainFilename as DWSTRING ) as DWSTRING
