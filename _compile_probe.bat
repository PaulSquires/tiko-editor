@echo off
cd src
rem ---------------------------------------------------------------------------
rem THE PsCore PROBE -- Phase 7a's first question, kept answerable.
rem
rem Builds src\_pscore_probe.bas, which includes PsCore ALONE and asserts the
rem handful of contracts tiko's ~500 call sites are about to depend on. It is a
rem separate program and not part of the unity build, for a reason that shapes
rem the whole of 7a:
rem
rem     PsCore AND AfxNova BOTH DECLARE A TYPE CALLED DWSTRING, and the two
rem     cannot coexist in one translation unit.
rem
rem tiko is a unity build -- one fbc invocation on tiko.bas -- so there is no
rem file-by-file migration available. The DWSTRING include is swapped once, and
rem everything that uses it has to be right on the same commit. This probe is
rem what makes it possible to check PsCore builds under tiko's compiler and
rem flags WITHOUT that swap having happened yet.
rem
rem The two types are not two implementations of one idea:
rem   AfxNova  TYPE DWSTRING EXTENDS WSTRING, so Len/Left/Mid/InStr/UCase apply
rem            for free -- and fbc's wstring is 2 bytes on Windows, 4 on Linux,
rem            which is precisely why it cannot cross.
rem   PsCore   an explicit UShort buffer, 2 bytes everywhere, with no cast to
rem            wstring. PsStr.bi carries 1-based PsLen/PsLeft/PsMid/PsInStr/
rem            PsUCase with fbc's exact semantics instead.
rem
rem Exit code is the verdict.
rem ---------------------------------------------------------------------------
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -gen gas64 -i ..\..\PsPlatform\src -x ..\_pscore_probe.exe _pscore_probe.bas
if errorlevel 1 exit /b 1
cd ..
_pscore_probe.exe
