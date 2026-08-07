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
'' ---- THIS FILE IS DONE, AND IT IS NOT SCAFFOLDING -------------------------
''
'' It was written in two steps. Step 1 made DocView a pure forward to
'' `pDoc->hWindow(i)`, so that converting 142 call sites to it provably changed
'' nothing and any movement in the 27-suite oracle was a conversion mistake
'' rather than a design difference. Step 2 was to be "move the storage here,
'' because clsDocument still declares its views as HWND and so cannot close
'' into src/app".
''
'' STEP 2 IS MOOT AND IS NOT COMING. clsDocument now stores both the views and
'' m_hWndActiveScintilla as `any ptr` -- see the comments on those members --
'' so it already names no Win32 type, and `_check_app_standalone` compiling
'' src/app against PsCore alone is the proof. The blocker step 2 existed to
'' remove was removed another way.
''
'' What is left is PERMANENT: DocView is the ONE PLACE the portable `any ptr`
'' becomes a shell-side HWND, and the one place the null and bounds guard below
'' lives. Inlining it back into the 142 sites would delete that guard and
'' scatter the cast. Editor windows belong to the shell forever, and so does
'' this seam.
''
'' ---- ALL 142 EXTERNAL USES ARE READS --------------------------------------
''
'' Measured, not assumed: no code outside clsDocument assigns to hWindow. So
'' there is a getter and no setter -- creation and destruction stay where they
'' are, in clsDocument.CreateScintillaWindows / DestroyScintillaWindows.
'' ===========================================================================

#pragma once

'' idx is 0 for the main view, 1 for the split. Returns 0 for a null document or
'' an out-of-range index rather than faulting: callers routinely ask about a
'' document that is mid-teardown, and every one of them already tests the result
'' with IsWindow.
declare function DocView( byval pDoc as clsDocument ptr, byval idx as long ) as HWND
