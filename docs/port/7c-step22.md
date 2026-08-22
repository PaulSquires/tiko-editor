# Phase 7c, step 22 — the row glyph, and a decision wired to nothing

Step 21 held this back deliberately: the pane switcher's Segoe MDL2 glyphs were unverified, and
building a second feature on the same unproven assumption would have doubled the exposure.

**The author confirmed the glyphs render.** This is the follow-through.

---

## It draws less than tiko's painter, and that is the point

`frmExplorer_PaintCallback` replaces the row wholesale and paints three things in the icon column
— a chevron for a container, a dot for a file, and the caption. **It has to paint the chevron
itself**, because tiko deliberately leaves `SetTreeIndent` off and does its own indent arithmetic
(`frmExplorer.inc:405`).

This panel has `SetTreeIndent` and `ShowTwisty` **on**, so `PsListTree` already draws the chevron
and already reserves the band it sits in. What is left is the one thing the control has no notion
of: a file row's document glyph.

So the overlay draws **a single character, on one kind of row**, and everything else on screen is
the control's. Twenty-two steps in, that is what the toolkit having caught up looks like.

## U+00B7 is not a private-use codepoint

`wszIconDocument` is **MIDDLE DOT**, and tiko's own comment says so: *"use the regular Segoe UI
font for this one"*. Not a Segoe MDL2 glyph from the private-use area.

**The assertion checks the value, not "not empty".** That is the whole reason this glyph was safe
to write before any interactive pass, where the pane switcher's icons were not: a middle dot is in
every text font and cannot come out as a box. An assertion reading `PsLen(g) = 1` would have been
satisfied by a PUA character just as well, and would have said nothing about the only property
that mattered.

## The band is approximated, and the approximation is stated

`nIndent` is the text's x offset and already includes the reserved twisty band, so the band is
what sits immediately left of it. Its **width** is the control's `nTwisty`, which is not exposed —
so the overlay uses the row's height instead, which is the same order and is what the band is
sized from by convention. The glyph is centred in whatever rect that gives.

If that ever stops being invisible, **the fix is a getter rather than arithmetic here.**

---

## The finding: a correct decision wired to nothing

The glyph decision is a pure function of the row kind, split out for the reason
`PsModalRouteEvent` was in step 2 — the painter needs a compositor and the decision does not —
and asserted exhaustively, including the four kinds that draw nothing.

Two reverts bit immediately: giving folders the file glyph, and swapping the codepoint for a
private-use one.

**Then removing the `OnPaintOverlay` call left all five assertions green.**

They test a pure function that **nothing had to be calling**. A correct decision reaching no
painter is a pane with no icons and a suite that says otherwise — this port's oldest failure, one
layer along from where it usually appears.

**And the same argument applied to callbacks nobody had ever checked either.** Three assertions
added that the panel's hooks are installed at all: the overlay, the context handler, and the two
row callbacks that have been carrying every click since step 4. Each fails on its own removal:
451/1, 451/1, 451/1.

That the row callbacks were unchecked for eighteen steps is the part worth keeping. They work, so
nothing ever asked.

---

## Verification

| | |
| --- | --- |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **443 → 452** |
| `_check_selftests` | 33 suites, 20,328 passed, 0 failed; `settings/` and `tiko.tiko` unchanged |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_scihost`, `_check_package` | green |
| PsPlatform `build.cmd check` | 48 suites, 0 failures — unchanged this step |
| revert-to-red | five rules: folders get the glyph (448/1), a PUA codepoint (448/1), and each of the three installations (451/1 apiece) |

**NOT VERIFIED BY ME:** that the dot appears where it should. The decision is asserted and the
drawing is not — `PaintText` needs a compositor. The band's width is an approximation by
construction, so this is the assertion most likely to be satisfied while the screen is a few
pixels wrong.

---

## What is left

1. **The interactive pass** — the glyph's position, the context menu, the Explorer as a whole.
2. **Label edit** on `PsListTree`, wired to the `PsTextBox` that already exists, and folder rename
   behind it. The last Explorer feature with a toolkit prerequisite.
3. **The action icons** — add / delete on the hot row, which the overlay can now draw and the
   context menu already has commands for.
4. **Run step 18's Linux scripts.**
5. **fontconfig** — still never executed anywhere.
