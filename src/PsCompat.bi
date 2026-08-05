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
