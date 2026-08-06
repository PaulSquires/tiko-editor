'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' ===========================================================================
'' modDocViews -- where a document's editor WINDOWS live. Shell-side, forever.
''
'' ---- WHY THIS EXISTS ------------------------------------------------------
''
'' 7d swapped what sits BEHIND the editor HWND -- a PsSciView instead of a
'' Scintilla window -- and deliberately kept the HWND, which is how ~212 SciExec
'' sites, 142 hWindow uses and ~90 hWndActiveScintilla sites survived unedited.
''
'' IT DID NOT MAKE clsDocument PORTABLE, and the recorded plan assumed it would.
'' clsDocument.bi still declares `hWindow(1) as HWND` and
'' `m_hWndActiveScintilla as HWND`, so it still names Win32 and is still
'' shell-side -- and src/app cannot close around a document model on the other
'' side of the boundary. That is the actual blocker for 7c, unchanged.
''
'' ---- WHAT THIS IS, AND WHAT IT WILL BE ------------------------------------
''
'' STEP 1 (this file today): DocView(pDoc, i) FORWARDS to pDoc->hWindow(i). Every
'' one of the 142 external sites is converted to call it, and the conversion
'' provably changes nothing -- which is what makes it safe to do in one pass and
'' verifiable against the 27-suite oracle.
''
'' STEP 2 (next): the storage moves HERE, into a shell-side table keyed by
'' document. clsDocument then holds only pSci(0..1) -- an opaque `any ptr` the
'' 398 pSci uses already treat as opaque -- names no Win32 type, and can close
'' into src/app.
''
'' This is the same forwarding-scaffold technique that carried 1413 call sites
'' through the PsCore conversion with no behaviour change, and for the same
'' reason: it separates "move every caller" from "change what the callee does",
'' so a failure can only be one or the other.
''
'' UNLIKE PsCompat.bi, THIS FILE IS NOT SCAFFOLDING. Editor windows belong to
'' the shell permanently; only the FORWARDING is temporary.
''
'' ---- ALL 142 EXTERNAL USES ARE READS --------------------------------------
''
'' Measured, not assumed: no code outside clsDocument assigns to hWindow. So
'' there is a getter and no setter, and step 2 does not have to find writers
'' scattered through the shell -- creation and destruction stay where they are,
'' in clsDocument.CreateScintillaWindows / DestroyScintillaWindows.
'' ===========================================================================

#pragma once

'' idx is 0 for the main view, 1 for the split. Returns 0 for a null document or
'' an out-of-range index rather than faulting: callers routinely ask about a
'' document that is mid-teardown, and every one of them already tests the result
'' with IsWindow.
declare function DocView( byval pDoc as clsDocument ptr, byval idx as long ) as HWND
