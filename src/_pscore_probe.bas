'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' _pscore_probe -- does PsCore build inside TIKO's toolchain?
''
'' Phase 7a replaces AfxNova's string, file, path and time surface with PsCore.
'' Before a single one of tiko's ~500 call sites is touched, this answers the
'' question that would otherwise be discovered halfway through the swap:
''
''     can tiko's compiler, with tiko's flags, build PsCore at all?
''
'' It is a separate program on purpose. PsCore and AfxNova BOTH declare a type
'' called DWSTRING and they cannot coexist in one translation unit -- so this
'' cannot be a #include added to tiko.bas, and the swap cannot be done
'' file-by-file inside the unity build either. That is a real constraint on how
'' 7a.2 and 7a.3 proceed and it is better known now than later.
''
'' The two DWSTRINGs are not merely different implementations of one idea:
''
''   AfxNova   TYPE DWSTRING EXTENDS WSTRING -- so Len/Left/Mid/InStr/UCase
''             apply for free, and fbc's wstring is 2 bytes on Windows and 4 on
''             Linux, which is exactly why it cannot cross.
''   PsCore    an explicit UShort buffer, 2 bytes everywhere, with NO cast to
''             wstring. The intrinsics therefore do NOT apply, and PsStr.bi
''             carries 1-based PsLen/PsLeft/PsMid/PsInStr/PsUCase with fbc's
''             exact semantics so the 557 intrinsic sites are a textual
''             substitution rather than 557 judgement calls.
''
'' Build:  fbc64 -i ..\..\PsPlatform\src _pscore_probe.bas
'' Or:     _compile_probe.bat
'' ===========================================================================

#include once "core/PsStr.inc"
#include once "core/PsPath.inc"
#include once "core/PsFile.inc"

dim shared as long g_nPass, g_nFail

sub Check(byref sWhat as string, byval bOk as boolean, byref sNote as string = "")
    if bOk then
        g_nPass += 1
        print "  ok      " & sWhat & iif(len(sNote) > 0, "  (" & sNote & ")", "")
    else
        g_nFail += 1
        print "  FAILED  " & sWhat & iif(len(sNote) > 0, "  (" & sNote & ")", "")
    end if
end sub

function D(byval z as zstring ptr) as DWSTRING
    dim as DWSTRING r
    r.Utf8 = *z
    return r
end function

print ""
print "=== PsCore, built by tiko's toolchain ==================================="
print ""

print "  --- the string family tiko's 557 intrinsic sites become ---"
'' 1-BASED, and identical to fbc's own intrinsics. If these were 0-based the
'' substitution would shift every one of those sites by one -- and both forms
'' compile AND run, so nothing downstream would catch it.
Check "PsLen counts code units", (PsLen(D("hello")) = 5)
Check "PsLeft is 1-based",       (PsLeft(D("hello"), 2).Utf8 = "he")
Check "PsMid is 1-based",        (PsMid(D("hello"), 2, 3).Utf8 = "ell")
Check "PsInStr returns a 1-based position", (PsInStr(D("hello"), D("ll")) = 3)
Check "  and 0 for not-found, as fbc does",  (PsInStr(D("hello"), D("zz")) = 0)
Check "PsUCase folds",           (PsUCase(D("hello")).Utf8 = "HELLO")

print ""
print "  --- the Afx* replacements 7a.2 and 7a.3 use ---"
Check "PsStrParse is 1-based",   (PsStrParse(D("a,b,c"), 2, D(",")).Utf8 = "b")
Check "PsStrReplace replaces all", (PsStrReplace(D("a-b-c"), D("-"), D("+")).Utf8 = "a+b+c")
Check "PsPathName",              (PsPathName(D("C:/a/b/c.txt")).Utf8 = "c.txt")
Check "PsPathExt",               (PsPathExt(D("C:/a/b/c.txt")).Utf8 = ".txt")
Check "PsPathDir",               (PsPathDir(D("C:/a/b/c.txt")).Utf8 = "C:/a/b")

'' THE ONE WITH A CONTRACT tiko depends on at 65 sites: the trailing separator.
scope
    dim as DWSTRING sDir = PsExePath()
    Check "PsExePath ends with a separator", _
        (sDir.Length > 0) andalso (sDir.At(sDir.Length - 1) = asc("/")), sDir.Utf8
end scope

'' And that the file layer actually reaches the disk from here.
scope
    dim as DWSTRING sSelf = PsExeFile()
    Check "PsFileExists finds this executable", PsFileExists(sSelf), sSelf.Utf8
    Check "  with a non-zero write time", (PsFileWriteTime(sSelf) > 0)
end scope

print ""
print "  " & g_nPass & " passed, " & g_nFail & " failed"
print ""
if g_nFail > 0 then end 1
end 0
