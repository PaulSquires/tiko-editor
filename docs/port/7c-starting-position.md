# Phase 7c: where it actually starts

7c converts tiko's shell — 48 `frmXxx` forms, **~45,000 lines**, a third of the
codebase — from `HWND` hierarchies to `PsSurface` + widget trees. The plan
estimates 14–20 weeks for Phase 7 and calls 7c its hard part. This records what
was measured before starting, so the first increment is chosen from evidence.

## The app layer does not compile standalone, and had never been asked to

Phase 7b created `src/app/` and a ratchet. Both were satisfied. But `src/app`
had only ever been compiled **as part of tiko**, where AfxNova is in scope — so
the property it exists to guarantee was never tested.

Compiling each file against PsCore alone:

    2 clean, 5 with errors

The failures are not architectural. They are:

* residual intrinsic sites the 1413-site conversion's patterns did not match
  (`instr(hpos, lhaystack, PsMid(...))` — the DWSTRING is argument *three*)
* `max()`, a tiko helper that lives outside `app/`
* `+` on DWSTRING, which PsCore did not have — now added

So the layer is *close*, not far. That is the useful shape of the finding.

## The ratchet had a third gap

It caught neither `CTextStream` nor its siblings: AfxNova's classes are named
`CTextStream`, `CFileStream`, `CWebView2`, so the `Afx`-prefix rule never sees
them. `modIniParse.inc` was in `app/` with a `dim pS as CTextStream` in it.

Found by the standalone compile, not by the ratchet. `app/` is 25 files now.

That is three vocabulary gaps in three audits — a hand-written list against a
surface with thousands of names. Read a green ratchet run as evidence, never as
proof.

## The recommended first increment

Not a form. **Make `src/app` compile against PsCore alone**, file by file, with
the standalone compile as the gate. It is bounded, it is measurable, every step
is verifiable, and it is a prerequisite for any shell work: the shell binary
will be a fresh translation unit with no AfxNova in it, so everything it
includes must already build that way.

Only then the plan's own step 1 — `frmMain`'s chrome and the editor pane, every
dock panel stubbed, as a runnable binary that is not merged.

## The decision that is still open

The plan's **D2** — SDL3 on both platforms, no Win32 backend — is what makes 7c
un-shippable in the middle: there is nothing to bridge a half-converted shell
to. That was decided as a forecast. Gate 6 has now landed on all three targets,
so it can be re-decided on evidence instead, and it should be, before 45,000
lines are committed to one shape.

> **TAKEN on 2026-08-09: Shape A — D2 HOLDS.** SDL3 on both platforms, no Win32
> backend. See [`d2-decision.md`](d2-decision.md) for the evidence and the costs.
>
> That page went through three answers before this one — "not yet", then "due but
> not mine", then taken — and the reason it took three is worth more than the
> result: **each round found that a fact it had reasoned from had moved.** The
> WebView2 constraint this paragraph used to call irreducible is now *removed from
> the tree*; `PsWin32Host` was said to have grown into most of a second backend and
> has not (it implements zero of the 18 entry points, and its own header says so);
> and the 25-control port was listed as pending when all 26 were done.
>
> The paragraph below is left as written, and every claim in it about what is
> blocked or impossible was wrong within weeks.
