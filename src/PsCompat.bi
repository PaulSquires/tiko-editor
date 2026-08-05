'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' PsCompat.bi -- SCAFFOLDING. Delete it when the DWSTRING swap lands.
''
'' Phase 7a converts tiko's 557 fbc-intrinsic call sites on DWSTRING-typed
'' identifiers -- 277 Len, 90 UCase, 65 Left, 42 Mid, 39 InStr and the tail --
'' to the Ps* family. The conversion has to happen BEFORE `DWSTRING` changes
'' meaning, for one reason:
''
''     Len(x) ON PsCore's DWSTRING COMPILES AND RETURNS 24.
''
'' It is a UDT with no cast to wstring, so fbc's Len falls back to SizeOf and
'' answers the size of the descriptor. Not an error, not a warning -- a plausible
'' small integer, at 277 sites. Every other intrinsic in that list fails loudly;
'' this one does not, and it is the reason the order matters.
''
'' So: convert first, while AfxNova's DWSTRING is still in force and the
'' compiler can check every site, and run the 28 self-test suites against the
'' result. Then swap the type, and these forwarders are deleted -- PsStr.bi's
'' real implementations take over, with identical semantics.
''
'' EVERY FUNCTION HERE IS A FORWARD TO THE INTRINSIC IT REPLACES. That is the
'' whole point: at this stage the conversion must change NOTHING, so that any
'' movement in the self-test numbers is a conversion mistake and not a
'' behavioural difference. The behavioural switch happens later, on its own
'' commit, where it can be attributed.
''
'' They take `byref as const wstring` because AfxNova's DWSTRING extends wstring
'' and binds to it -- which is also how AfxStr's own functions are declared. They
'' RETURN DWSTRING rather than wstring because fbc cannot return a bare wstring
'' by value, and DWSTRING is what tiko already receives from every AfxStr call.
'' ===========================================================================

#pragma once

'' 1-BASED, exactly as fbc's intrinsics are, and exactly as PsStr.bi's versions
'' are. Making either 0-based would shift all 557 sites by one, and both forms
'' compile AND run.
private function PsLen(byref w as const wstring) as uinteger
    return len(w)
end function

private function PsLeft(byref w as const wstring, byval n as integer) as DWSTRING
    return left(w, n)
end function

private function PsRight(byref w as const wstring, byval n as integer) as DWSTRING
    return right(w, n)
end function

private function PsMid(byref w as const wstring, byval nStart as integer, _
                       byval nLen as integer = -1) as DWSTRING
    '' fbc's Mid with no count runs to the end, and -1 is how that is spelled
    '' here so the two-argument form keeps working.
    if nLen < 0 then return mid(w, nStart)
    return mid(w, nStart, nLen)
end function

'' 0 FOR NOT FOUND, which is fbc's convention and PsStr.bi's. A -1 convention
'' would make every `if InStr(...) > 0` site silently true.
'' OVERLOADED, so both fbc forms survive the conversion. fbc needs the keyword
'' on the first of a set -- without it the second is a duplicated definition.
private function PsInStr overload (byref w as const wstring, byref sFind as const wstring) as integer
    return instr(w, sFind)
end function

private function PsInStr(byval nStart as integer, byref w as const wstring, _
                         byref sFind as const wstring) as integer
    return instr(nStart, w, sFind)
end function

private function PsInStrRev overload (byref w as const wstring, byref sFind as const wstring) as integer
    return instrrev(w, sFind)
end function

private function PsInStrRev(byref w as const wstring, byref sFind as const wstring, _
                            byval nStart as integer) as integer
    return instrrev(w, sFind, nStart)
end function

private function PsUCase(byref w as const wstring) as DWSTRING
    return ucase(w)
end function

private function PsLCase(byref w as const wstring) as DWSTRING
    return lcase(w)
end function

private function PsTrim overload (byref w as const wstring) as DWSTRING
    return trim(w)
end function

'' The two-argument form, which trims a SUBSTRING rather than whitespace.
'' fbc's `trim(s, Any "chars")` has no forwardable equivalent -- ANY is syntax,
'' not an argument -- so the three sites using it are left as intrinsics and
'' listed in docs/port/ for the swap commit to deal with.
private function PsTrim(byref w as const wstring, byref sWhat as const wstring) as DWSTRING
    return trim(w, sWhat)
end function

private function PsLTrim overload (byref w as const wstring) as DWSTRING
    return ltrim(w)
end function

'' The two-argument form, which trims a SUBSTRING rather than whitespace.
'' fbc's `ltrim(s, Any "chars")` has no forwardable equivalent -- ANY is syntax,
'' not an argument -- so the three sites using it are left as intrinsics and
'' listed in docs/port/ for the swap commit to deal with.
private function PsLTrim(byref w as const wstring, byref sWhat as const wstring) as DWSTRING
    return ltrim(w, sWhat)
end function

private function PsRTrim overload (byref w as const wstring) as DWSTRING
    return rtrim(w)
end function

'' The two-argument form, which trims a SUBSTRING rather than whitespace.
'' fbc's `rtrim(s, Any "chars")` has no forwardable equivalent -- ANY is syntax,
'' not an argument -- so the three sites using it are left as intrinsics and
'' listed in docs/port/ for the swap commit to deal with.
private function PsRTrim(byref w as const wstring, byref sWhat as const wstring) as DWSTRING
    return rtrim(w, sWhat)
end function

'' ===========================================================================
'' The AfxStr* replacements. Same rule as above: forwarders, so the conversion
'' changes nothing and the self-tests stay the oracle.
'' ===========================================================================

private function PsStrParse(byref w as const wstring, byval nPosition as integer, _
                            byref sDelim as const wstring) as DWSTRING
    return AfxStrParse(w, nPosition, sDelim)
end function

private function PsStrParseCount(byref w as const wstring, byref sDelim as const wstring) as long
    return AfxStrParseCount(w, sDelim)
end function

private function PsStrReplace(byref w as const wstring, byref sFind as const wstring, _
                              byref sWith as const wstring) as DWSTRING
    return AfxStrReplace(w, sFind, sWith)
end function

private function PsStrRemove(byref w as const wstring, byref sFind as const wstring) as DWSTRING
    return AfxStrRemove(w, sFind)
end function

private function PsStrShrink(byref w as const wstring, byref sMask as const wstring = " ") as DWSTRING
    return AfxStrShrink(w, sMask)
end function

private function PsStrParseAny(byref w as const wstring, byval nPosition as integer, _
                               byref sDelim as const wstring) as DWSTRING
    return AfxStrParseAny(w, nPosition, sDelim)
end function

private function PsStrReplaceAny(byref w as const wstring, byref sMatch as const wstring, _
                                 byref sWith as const wstring) as DWSTRING
    return AfxStrReplaceAny(w, sMatch, sWith)
end function

private function PsStrRemoveAny(byref w as const wstring, byref sMatch as const wstring) as DWSTRING
    return AfxStrRemoveAny(w, sMatch)
end function

private function PsStrLSet(byref w as const wstring, byval nLength as integer, _
                           byref sPad as const wstring = " ") as DWSTRING
    return AfxStrLSet(w, nLength, sPad)
end function

private function PsStrRSet(byref w as const wstring, byval nLength as integer, _
                           byref sPad as const wstring = " ") as DWSTRING
    return AfxStrRSet(w, nLength, sPad)
end function

private function PsStrDelete(byref w as const wstring, byval nStart as integer, _
                             byval nCount as integer) as DWSTRING
    return AfxStrDelete(w, nStart, nCount)
end function

private function PsStrClipLeft(byref w as const wstring, byval nCount as integer) as DWSTRING
    return AfxStrClipLeft(w, nCount)
end function

'' NOT A FORWARDER, DELIBERATELY -- the one function in this file that does not
'' preserve current behaviour, because current behaviour is wrong.
''
'' AfxStrParseCountAny advances its scan by the LENGTH OF THE DELIMITER SET
'' after each hit. The set is a set, and every hit is one character, so with a
'' two-character set it steps over whatever follows a delimiter: "1;;2" counts
'' 2 fields where ParseAny can plainly return 3. A count that disagrees with the
'' parse it is counting cannot be relied on by either.
''
'' Implemented correctly HERE rather than at the type swap, so that if either of
'' tiko's two call sites depends on the old answer, the 28 self-test suites say
'' so NOW -- while this commit is the only thing that changed.
private function PsStrParseCountAny(byref w as const wstring, byref sDelim as const wstring) as long
    dim as long nCount = 1
    for i as integer = 1 to len(w)
        if instr(sDelim, mid(w, i, 1)) > 0 then nCount += 1
    next
    return nCount
end function

'' ===========================================================================
'' The file and path replacements.
''
'' AfxStrPathName's four modes map ONE FOR ONE onto PsPath, but only after
'' reading its source rather than its name -- three of the four are not what a
'' reasonable guess would give:
''
''   "PATH"  -> PsPathDirWithSep   the directory INCLUDING its trailing
''                                 separator. PsPathDir excludes it, and the
''                                 34 sites here all concatenate a filename on.
''   "NAME"  -> PsPathStem         the name WITHOUT its extension, despite the
''                                 mode being called NAME.
''   "NAMEX" -> PsPathName         the name WITH its extension.
''   "EXTN"  -> PsPathExt          the extension INCLUDING its dot.
''
'' Two of those -- the separator and the dot -- are the exact "changes answers,
'' not builds" hazard this whole phase is sequenced around.
''
'' The parameters below are NON-const wstring, unlike the string family above.
'' AfxStrPathName and its neighbours declare their path argument as plain
'' `BYREF ... AS WSTRING`, and a const reference will not bind to it.
''
'' The ones taking a POINTER (AfxFileExists, AfxDeleteFile and friends) do take
'' const, and they have to: several tiko call sites hold their path as
'' `byref ... as const wstring` and would not compile otherwise.
'' ===========================================================================

private function PsPathDirWithSep(byref w as wstring) as DWSTRING
    return AfxStrPathName("PATH", w)
end function
private function PsPathStem(byref w as wstring) as DWSTRING
    return AfxStrPathName("NAME", w)
end function
private function PsPathName(byref w as wstring) as DWSTRING
    return AfxStrPathName("NAMEX", w)
end function
private function PsPathExt(byref w as wstring) as DWSTRING
    return AfxStrPathName("EXTN", w)
end function

private function PsExePath() as DWSTRING
    return AfxGetExePathName()
end function
private function PsFileExists(byref w as const wstring) as boolean
    return AfxFileExists(w)
end function
private function PsFileDelete(byref w as const wstring) as boolean
    return AfxDeleteFile(w)
end function
private function PsPathIsRelative(byref w as const wstring) as boolean
    return AfxPathIsRelative(w)
end function
private function PsPathJoin(byref a as const wstring, byref b as const wstring) as DWSTRING
    return AfxPathCombine(a, b)
end function
private function PsFileCopy(byref a as wstring, byref b as wstring, _
                            byval bFailIfExists as boolean = false) as boolean
    return AfxCopyFile(a, b, bFailIfExists)
end function
private function PsPathRelativeTo(byref sPath as wstring, _
                                  byref sBaseDir as wstring) as DWSTRING
    return AfxPathRelativePathTo(sBaseDir, FILE_ATTRIBUTE_DIRECTORY, sPath, 0)
end function
private function PsCurDir() as DWSTRING
    return AfxGetCurDir()
end function
private function PsFileRename(byref a as const wstring, byref b as const wstring) as boolean
    return AfxRenameFile(a, b)
end function
private function PsFileSize(byref w as wstring) as longint
    return AfxGetFileSize(w)
end function
'' NO PsFileRealCase FORWARDER. AfxFilenameOriginalCase is TIKO'S OWN function
'' (modPaths.inc), not AfxNova's -- a pure move made during an earlier cleanup,
'' and it is declared after this file is included. Its two call sites map onto
'' PsFileRealCase at the type swap, where the whole of modPaths.inc gets
'' revisited anyway.
private function PsFileIsDir(byref w as const wstring) as boolean
    return AfxPathIsDirectory(w)
end function
private function PsFileIsReadOnly(byref w as wstring) as boolean
    return AfxIsReadOnlyFile(w)
end function
private function PsDirCreate(byref w as const wstring) as boolean
    return AfxCreateDirectory(w)
end function
