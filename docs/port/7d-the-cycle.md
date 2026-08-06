# Phase 7d, and the dependency cycle D2 creates

7d replaces `CreateWindowEx(0, "Scintilla", …)` with `PsSciView`. Attempting it
turns up a cycle, and the cycle is worth naming precisely because it is the
consequence of a decision that is still open.

## clsDocument is 95% portable already

    130 fields, of which HWND:              2
    pSci  (the opaque editor pointer):    398 uses
    SciMsg (through that pointer):        350 uses
    hWindow (the HWND):                    35 uses, down from 49

The plan's claim that the 801 `SciMsg` sites survive the port is confirmed
here at the source: the document model reaches its editor through an OPAQUE
POINTER already. `SciMsg(pSci(i), SCI_*, wp, lp)` does not care whether that
pointer came from `SCI_GETDIRECTPOINTER` or from `SciPs_Create`.

14 sites that sent SCI_ messages *through the HWND* are now direct-pointer
calls — behaviour-identical, faster by tiko's own measurement, and 14 fewer
HWND dependencies. What remains is genuinely windowing: create, destroy,
`IsWindow`, focus tracking, and the HWND→pSci lookup.

## The cycle

    7c (shell)  needs  app/ closed
    app/ closed needs  clsDocument portable
    clsDocument portable needs its HWNDs gone
    its HWNDs go when the editor is a PsSciView
    PsSciView is a PsWidget, needing a PsSurface — an SDL3 window
    an SDL3 window is the new shell  ->  7c

There is no ordering of 7c and 7d that breaks it, which is why the previous
document's recommendation to do 7d first was wrong. Both ends need the other.

## D2 is the mechanism

**D2 — SDL3 on both platforms, no Win32 backend** — is what closes the loop. A
`PsSurface` backed by an `HWND` would let a `PsSciView` sit inside `frmMain`'s
existing hierarchy: `clsDocument` could hold one *today*, `app/` could close,
and the panels could convert one at a time against a shell that still runs.

Without it the shell has to flip in a single step, with `frmMain`'s chrome and
the editor pane converted together and every dock panel stubbed — which is
exactly what the plan describes, and why it says the branch cannot be merged
until 7c completes.

That was decided as a forecast, before any of this existed. It is now the
difference between an incremental migration and a single 45,000-line jump, and
the evidence to re-decide it exists: Gate 6 has landed on Windows, WSL2 and
native Fedora/Wayland.

**A Win32 `PsSurface` backend would be scaffolding, not a product feature** —
deleted when 7c completes, exactly like `PsCompat.bi` and `namespace PsC`. The
same technique that carried 1413 call sites with no behaviour change would
carry the shell.

This is a decision, not a task. It is recorded here rather than taken.
