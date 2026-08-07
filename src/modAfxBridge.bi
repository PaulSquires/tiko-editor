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
''
'' ---- WHAT IS LEFT, AND WHY IT IS NOT 26 ANY MORE --------------------------
''
'' 24 -> 10. The ones that went were text tiko was routing through AfxNova for
'' no reason other than habit:
''
''   AfxGetWindowText   9 sites -> modRoutines' WindowText(). The window is the
''                      shell's own; only the RETURN TYPE ever needed a bridge.
''   AfxStrExtract      5 sites -> PsStrExtract, except the two "strip the
''                      comment" sites, which relied on AfxStrExtract returning
''                      THE WHOLE STRING when the delimiter is absent.
''                      PsStrExtract returns "" there, so those two are spelled
''                      out with PsInStr/PsLeft rather than swapped.
''
'' THE 10 THAT REMAIN ARE NOT LIKE THOSE. Every one reads text out of an
'' AfxNova SUBSYSTEM tiko has not replaced, so the bridge is not what is
'' holding them:
''
''   8  PsTextBox   RichEdit_GetText / RichEdit_GetSelText / AfxGetClipboardText
''                  -- goes when PsTextBox stops hosting AfxNova's RichEdit.
''   1  frmUserTools  AfxBrowseForFolder -- an AfxNova shell dialog.
''   1  modRoutines   AfxCommand -- the WIDE command line. fbc's `command()` is
''                  ANSI, so this needs CommandLineToArgvW and its own argument
''                  splitting, not a rename.
''
'' So the count no longer measures conversion laziness; it measures three
'' specific subsystems. Track those, not the number.
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
