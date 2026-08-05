' ########################################################################################
' modBuildService.inc  -  see modBuildService.bi for what this owns and what it does not.
' ########################################################################################

#include once "modBuildService.bi"

' The single copy of "is a build running". It was gApp.IsCompiling: a boolean in the app
' flag bag whose only reader for years was WM_SETCURSOR, so it drove the busy cursor while
' looking like a guard. Nothing outside this file may write it.
dim shared as long gBuildState = BUILD_IDLE

function BuildService_State() as long
    return gBuildState
end function

function BuildService_IsRunning() as boolean
    return (gBuildState = BUILD_RUNNING)
end function

' TEST AND SET, not a setter. "Is a build running?" and "claim it" are one operation here so
' the guard cannot be split by a later edit. Everything calls this on the UI thread, so it
' is discipline rather than atomicity -- and saying which is the point.
function BuildService_Begin() as boolean
    if gBuildState <> BUILD_IDLE then return false
    gBuildState = BUILD_RUNNING
    return true
end function

sub BuildService_End()
    gBuildState = BUILD_IDLE
end sub




' ========================================================================================
' Compose the lpCommandLine for a child process.
'
' THE FIRST TOKEN OF lpCommandLine IS argv[0] AND IS DISCARDED BY THE CHILD, ALWAYS --
' even when lpApplicationName is supplied separately, which is how this file calls
' CreateProcessW. Windows hands the string through verbatim and the child's own startup
' code strips the leading program name. tiko passed the SWITCHES here with no argv[0] at
' all, so the child ate the first one.
'
' Measured, not reasoned about (harness: CreateProcessW into a program that dumps every
' argv slot):
'
'     cmdline = [-m "prog" mod1.o -v -w all]     ->  argv[0]=[-m]   <-- FLAG GONE
'                                                    argv[1]=[prog]
'     cmdline = [  -w all -O2 -c -b "x.bas"]     ->  argv[0]=[]     <-- survived, by luck
'                                                    argv[1]=[-w]
'
' So the project link lost its "-m" -- the flag two forum threads were consulted about,
' cited at its call site in modCompile.inc -- on every single build. And the module path
' was correct ONLY because code_CompileModules happens to space-pad CompileFlags: any tidy
' that trim()s that string would have started silently deleting the user's first build
' flag, with nothing in the build, the warnings or the assertions to say so.
'
' The exe is quoted because argv[0] is parsed by the ordinary quoting rules, and a path
' with a space in it would otherwise split and swallow the real first switch instead --
' i.e. reintroduce the bug in a form that only shows up on some machines.
'
' Pure: string in, string out. That is what lets TIKO_COMPILECMD_SELFTEST assert the rule
' without launching a compiler.
' ========================================================================================
function BuildChildCommandLine( _
            byval wszExe as DWSTRING, _
            byval wszParams as DWSTRING _
            ) as DWSTRING
    dim as DWSTRING wszQ = wchr(34)
    if PsLen( wszParams ) = 0 then return wszQ & wszExe & wszQ
    return wszQ & wszExe & wszQ & " " & wszParams
end function


' ========================================================================================
' The "-m" pair for the main module: the module NAME after -m, and the SOURCE FILE as an
' ordinary input. Both are required and they are not the same string.
'
' fbc's -m only MARKS which module holds the entry point; it does not add anything to the
' compile. So the main source must also appear as an input, with its extension -- which is
' exactly what the long-standing comment at the call site says, and exactly what the code
' there did not do. Pure, so the rule is assertable without running a compiler; see
' TIKO_COMPILECMD_SELFTEST, which exists because getting this wrong produced a link failure
' no string assertion could have predicted.
' ========================================================================================
function BuildMainModuleArgs( _
            byval wszMainName as DWSTRING, _
            byval wszMainFilename as DWSTRING _
            ) as DWSTRING
    dim as DWSTRING wszQ = wchr(34)
    return "-m " & wszQ & wszMainName & wszQ & " " & wszQ & wszMainFilename & wszQ
end function
