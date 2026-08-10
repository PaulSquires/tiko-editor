'' ========================================================================================
'' modAppHost -- THE SEAM THE DOCUMENT MODEL LEAVES THE SHELL THROUGH.
''
'' clsDocument and clsTopTabCtl are portable in every way that matters -- they store their
'' editor views as `any ptr`, and all 801 SciMsg call sites go through a function pointer
'' whose shape PsScintilla.bi deliberately preserves ("they are not edited, they are
'' relinked"). What keeps them in src/ is a short list of things they ask the WINDOW for:
'' make me a view, redraw it, how tall is it, what is its DPI ratio, ask the user for a path.
''
'' Those are host services, not document logic. This record is how they get asked for
'' without naming Win32 -- tiko fills it with the AfxNova bodies it already has, the shell
'' fills it with PsPlatform, and the document model stops caring which it got.
''
'' ---- WHY A RECORD OF FUNCTION POINTERS AND NOT AN #IFDEF -------------------------------
''
'' `_check_app_layer.bas` says it plainly for the whole layer: "Either the code belongs in
'' the shell, or it needs a PsPlatform abstraction. Not an #ifdef." Both binaries are built
'' from the same sources in the same tree; an #ifdef would mean the app layer is compiled
'' differently for each, and the standalone gate could then only ever check one of them.
''
'' ---- THE SAME IDIOM modDocViews ALREADY MADE PERMANENT ---------------------------------
''
'' modDocViews.bi is the ONE place a document's portable `any ptr` becomes a shell-side HWND,
'' and its header explains why inlining that back into 142 call sites would be wrong. This is
'' the same boundary seen from the other side: there, the shell reads a document's view; here,
'' a document asks the shell to act on one. Neither direction names the other's types.
''
'' ---- EVERY FIELD IS REQUIRED. A NULL IS A CRASH AT THE WORST MOMENT --------------------
''
'' There is no "if the host did not supply this, do something sensible" arm anywhere, and
'' there should not be: a document that silently skipped CreateView would present an empty
'' editor with no error. AppHost_IsComplete() below exists so a host can be checked ONCE, at
'' startup, instead of each call site guessing.
'' ========================================================================================

#pragma once

type AppHostServices
    '' ---- editor views ------------------------------------------------------------------
    '' Makes the nIndex'th view for a document (0 is the main pane, 1 the split) and returns
    '' the opaque handle clsDocument stores. tiko returns a tikoSciHost HWND; the shell
    '' returns a PsSciView. Neither side says which to the other.
    CreateView     as function(byval nIndex as long) as any ptr
    DestroyView    as sub(byval pView as any ptr)

    '' Whether the handle still refers to something. tiko's IsWindow; the shell's own
    '' bookkeeping. Documents routinely hold a view through teardown and every existing
    '' call site already tests before using.
    IsViewAlive    as function(byval pView as any ptr) as boolean

    '' Repaint now, rather than at the next idle. Used where a load would otherwise leave the
    '' pane stale for a visible moment.
    InvalidateView as sub(byval pView as any ptr)

    '' The view's DPI ratios. Two call sites need these to size margins, and both currently
    '' reach the owning CWindow to get them -- which is also why they cannot simply read a
    '' global: a view parented elsewhere would silently answer 1.0 and put every margin at
    '' the wrong width above 100%.
    ViewScale      as sub(byval pView as any ptr, byref rx as single, byref ry as single)

    '' The view's height in pixels. The one caller divides it by the line height to get lines
    '' per page.
    ''
    '' HEIGHT, NOT A RECT, and that is a simplification rather than a shortcut: the existing
    '' code takes AfxGetWindowRect, calls MapWindowPoints to translate screen coordinates to
    '' frmMain's client area, and then uses only (bottom - top). Translation cannot change a
    '' height, so the mapping never affected the answer.
    ViewHeight     as function(byval pView as any ptr) as long

    '' The OWNER window's UI font -- not the editor's. One caller styles the autocomplete
    '' popup with it, so the popup matches the application's chrome rather than the code face.
    '' Reached through the view for the same reason ViewScale is: it is the font of whatever
    '' window this view is parented to, not a process-wide constant.
    ''
    '' Two fields returning the parts separately, rather than one filling byref parameters,
    '' because the call site casts the name straight into a Scintilla message and matching
    '' that expression exactly is worth more here than a tidier signature.
    ViewUiFontName as function(byval pView as any ptr) as string
    ViewUiFontSize as function(byval pView as any ptr) as long

    '' ---- asking the user for a path -----------------------------------------------------
    '' TRUE if the user chose one. tiko uses the Afx IFileDialog wrappers; the shell will use
    '' PsPlatform's, which are asynchronous and get a synchronous wrapper of their own.
    AskOpenPath    as function(byref sOut as DWSTRING) as boolean
    AskSavePath    as function(byref sPath as DWSTRING, byref sExt as DWSTRING) as boolean

    '' ---- what the tab control asks the shell --------------------------------------------
    '' Whether the Find bar is up. The tab control clears selection highlighting on a switch
    '' unless a find is active.
    IsFindVisible  as function() as boolean

    '' Close the tab at this index, with everything that implies -- prompting to save, and
    '' removing the document. THE ONE FIELD HERE THAT IS A COMMAND RATHER THAN A SERVICE, and
    '' it is worth watching: if it turns out to need more of the shell than an index, the tab
    '' control is less portable than it measures and that is a reason to stop rather than to
    '' widen this record until it fits.
    CloseTab       as sub(byval nTabIdx as long)
end type

extern gAppHost as AppHostServices

'' TRUE only when every field is set. Called once at startup by both binaries -- see the note
'' above about why there is no per-call fallback.
declare function AppHost_IsComplete() as boolean

'' Which field is missing, for the message. Empty when the record is complete.
declare function AppHost_FirstMissing() as string
