# Phase 7c, step 20 — the pane switcher

Step 19 said *"PsPlatform has no PsIconPanel equivalent — a control-sized job"*. **It was
false**, and this step is the follow-through: `src/frmPanelMenu.inc`, 199 lines, ported.

`shellpanel.bi` had carried *"the strip is not ported"* since step 8.

---

## The one place the port is SIMPLER than the original

tiko's items are `PSICON_COMMAND`, and its custom painter hand-checks each item's id against
the current pane to draw the highlight (`frmPanelMenu.inc:74-78`).

**`PsIconPanel` carries selection ON THE ITEM** and has `SelectExclusive`. Three `PSICON_TOGGLE`
items give the active-pane highlight from the built-in painter, with **no host painting at all**.

That is the first time in twenty steps the toolkit has made a thing shorter rather than longer.

## Two decisions worth the words

**The strip follows the MODE, not the click.** `SelectExclusive` is driven from
`ShellPanel_SetMode`. Driving it from the click handler would light the right button whenever the
user pressed one and leave it **stale** whenever the pane changed another way — the View menu, an
accelerator, `ShellPanel_Reload`, or step 19's `IDM_VIEWEXPLORER`. Four such paths already.

**One command path.** `ShellPanelMenu_OnClick` resolves the item to the menu id it carries and
hands it to `OnMenuCommand`. A pane button and its View-menu entry cannot come apart, and whatever
a future item needs — an enable rule, a state — is written once.

## The glyphs go in as bytes

```
g.Utf8 = chr(&hEE, &hA2, &hA9)   ' U+E8A9  Explorer
```

not `dim as DWSTRING g = !""`, which is the shape that cost this port a
`STATUS_HEAP_CORRUPTION` — PsCore had no constructor from a native `wstring`, fbc silently bound
the `zstring ptr` overload, and **the crash site moved every time a print was added to find it**.
PsCore has the constructor now; `.Utf8` is what the gallery uses, it is unambiguous at the call
site, and the codepoint stays visible in the comment.

**Whether they RENDER is not settled here.** `PsIconPanel` draws the string in the *surface's*
font — there is no per-widget face — so a Segoe MDL2 private-use codepoint arrives only if step
12's fallback chain resolves a face covering it, and PUA is what `FontLink\SystemLink` is least
likely to list. No assertion in this binary can see a glyph.

---

## One row on tiko's side is two on the shell's

The dump gains a `PANELMENU` row and `PANEL` shortens by exactly that much. **Not a difference and
not a class-1 rounding:** `HWND_FRMPANEL` is a *container* in tiko and `modLayoutDump` dumps the
container, where the shell puts both on the surface root.

```
tiko    PANEL      0,52,413,854   (413x802)
shell   PANELMENU  0,52,413,105   (413x53)
        PANEL      0,105,413,854  (413x749)      53 + 749 = 802
```

**Asserted as the union rather than by editing the oracle's `PANEL` row.** The union is the
property tiko pins; either rect alone can be wrong in a way the other cancels, and a number
hand-copied into the oracle would hide precisely that — which is what the oracle's own
*"regenerate it, don't hand-edit it"* is for. `PANEL_RIGHT` needs no second case: the strip spans
the panel's width, so it moves with the panel.

---

## THE FIRST THREE REVERTS ALL CAME BACK GREEN

This is the step's real content.

| revert | result |
| --- | --- |
| remove the `SyncToMode` call | **417/0** |
| make the items `PSICON_COMMAND`, so nothing can latch | **417/0** |
| set the strip's height to **zero** | **417/0** |

The geometry assertions written for the new band are **relations** — *strip above tree*, *both
spanning the panel's width*, *union equals tiko's band* — and **a zero-height strip satisfies
every one of them.** `0 + 802 = 802`.

That is step 1's finding word for word: *"the shell shipped a commit whose UI was visibly unscaled
while 21 assertions passed — every one of them a RELATION between rectangles, and relations hold
perfectly at the wrong scale."* Nineteen steps later, in a file that quotes it.

**What was missing was a NUMBER and a BEHAVIOUR.** Nine assertions added — three items, ids by
`FindItemByID` rather than by position, all three latching, the highlight moving with the mode,
the highlight moving when the change came from **the menu**, and a click going through the same
path — plus one assertion that the strip's height is `SH_PANELMENU_H` scaled.

The same four reverts now give **422/4, 421/5, 425/1, 425/1**.

---

## Deferred, and it is one gap class rather than several

`PsIconPanel` has `OnClick` and nothing else — **no per-item paint callback, no tooltip callback**
— where tiko's has both. Same shape as `PsListTree`'s replace-only paint hook from step 19.

**The pattern across three controls is one gap: PsPlatform's controls carry the model and the
default painting, and what they lack is host PAINT and TOOLTIP hooks.** `PsTooltip`, `PsTextBox`
and `PsPopupHost` all exist; what is missing is the controls routing to them. That is a much
smaller and more coherent thing than the five unrelated gaps step 19 claimed.

Also deferred: tiko's right-hand command group — Find in Project, Save All, Debug, Compile, Build
& Execute — whose ids have no handlers in this binary.

---

## Verification

| | |
| --- | --- |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **414 → 426** |
| `_check_selftests` | 33 suites, 20,328 passed, 0 failed; `settings/` and `tiko.tiko` unchanged |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_scihost`, `_check_package` | green |
| PsPlatform `build.cmd check` | 48 suites, 0 failures |
| layout oracle | **regenerated**, and every state's only change is `PANELMENU` plus the shortened `PANEL` |
| revert-to-red | four rules, each failing its own assertions — after three of the four had come back green against the first draft |

**NOT VERIFIED BY ME:** what the strip looks like, and **whether the glyphs render at all**. Both
are the author's, and the second is the more likely to disappoint — if they come out as boxes the
pane switching still works and the fix is an explicit glyph face, which is its own step.

---

## What is left

1. **The interactive pass** — the strip's appearance and the Explorer's.
2. **The host paint/tooltip hook class** on `PsListTree` and `PsIconPanel`, which unblocks the
   Explorer's glyph, its action icons, the strip's real look and tooltips everywhere.
3. **Label edit** on `PsListTree`, wired to the `PsTextBox` that already exists, and the Explorer's
   folder new / rename / delete behind it.
4. **Run step 18's Linux scripts.**
5. **fontconfig** — still never executed anywhere.
