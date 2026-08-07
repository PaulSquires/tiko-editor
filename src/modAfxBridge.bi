'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' modAfxBridge.bi -- THE ONE-WAY DOOR BACK FROM AfxNova.
''
'' The DWSTRING swap makes PsCore's DWSTRING the only meaning of that name in
'' tiko. AfxNova is still linked, still creates the windows and still owns the
'' RichEdit, the clipboard, the COM file dialogs and CWindow -- and its
'' functions return AfxNova's OWN DWSTRING, which is a different type that
'' happens to share a name.
''
'' TWO DIRECTIONS, TWO SPELLINGS, AND THEY ARE NOT SYMMETRIC:
''
''   tiko -> AfxNova     `*x.Wz()`     PsCore hands out a wstring ptr; AfxNova's
''                                     parameters are `byref as wstring`, so the
''                                     pointer is dereferenced at the call.
''   AfxNova -> tiko     `AfxW(...)`   THIS FILE. AfxNova's DWSTRING extends
''                                     wstring, so it binds to the parameter
''                                     below and PsCore constructs from it.
''
'' fbc will not chain those two conversions on its own -- `dim as DWSTRING x =
'' AfxGetWindowText(h)` is an error, not a copy -- which is the whole reason
'' this function exists rather than a comment saying "it just works".
''
'' IT IS A COPY, AND DELIBERATELY. AfxNova's return is a temporary; taking a
'' pointer into it and keeping it would be a dangling read the moment the
'' statement ended.
''
'' THIS FILE IS THE MEASURE OF HOW MUCH AfxNova IS LEFT. `git grep -c AfxW(`
'' is the count of places tiko still takes text back from it, and the file is
'' deleted -- not rewritten -- when that count reaches zero.
'' ===========================================================================

#pragma once

'' NON-CONST `byref as wstring`, AND THAT IS NOT A STYLE CHOICE -- it is the rule at the
'' top of CLAUDE.md. AfxNova's DWSTRING bound to a CONST reference and then copied corrupts
'' the process heap, and AfxNova's own AfxStr functions declare their string parameter
'' non-const for exactly this reason. Written `as const` first; tiko then failed to start,
'' with no message and no faulting line.
private function AfxW overload (byref w as wstring) as DWSTRING
    return w
end function
