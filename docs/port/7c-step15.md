# Phase 7c, step 15 — the dialog suites, and two reasons that were never true

Step 14 gated everything that runs at tiko **startup**. Five dialogs each ran a layout self-test
on Show and could not be gated, because each owns a `GetMessage` loop and nothing closed it.

`frmAbout` and `frmFormatOptions` had already solved that — `PostMessage(WM_CLOSE)` behind an env
var. **The pattern existed and had simply never been applied to the other four.**

**25 report lines → 33. 19,965 assertions → 20,328.**

---

## The first run found three failures in suites that had never run

**User Tools: "the Tool Name field is in the tab order".** Not a UI defect. With no tools
configured, `LoadDetail(-1)` hides the whole detail column and a hidden control is not a tab stop.
**The assertion twelve lines below it already carries the `if nWas >= 0` guard for exactly this
case**; this one did not, so it failed for a configuration rather than for a defect.

**frmAbout, twice: `LICENSE-PROPRIETARY.txt` is not beside the executable**, so the About box had
been quietly showing its fallback notice — which is precisely what that assertion was written to
catch, and it had never been run.

The author's call: **the proprietary licence is deprecated, `LICENSE` is the only one.** So the
file reference, `LICPAGE_ENUM`, the reader's branch and the two-panel *GPL-3 / Proprietary*
selector are all gone. A selector over one item is dead UI — step 13's judgement on the Character
Set combo.

> **Lang id 397 "Proprietary" STAYED, at this point.** I blanked it, then found three live
> references: it labelled the hero pill describing the *components*, which was still true while
> that pill existed. Caught by grepping before committing, not after. It is blanked for real
> further down — see "And then the screenshot", which is what removed the pill.

---

## Two claims about why these hooks exist. Both measured. Both wrong.

**Three files said** a killed run *"loses every buffered line — including the self-test's own
report"*, and that this is why AUTOCLOSE and AUTOEXIT exist.

**False.** `_check_selftests.bat` tree-killed tiko on every run and got all 25 report lines. A
78-byte log survived a kill under PowerShell redirection *and* under `cmd`'s `>`.

**My replacement was also false.** I wrote that a modal dialog owns the loop, so every suite after
it never runs. The revert-to-red meant to prove it showed the opposite: removing an AUTOCLOSE, all
**33 suites still reported**. `GetMessage` takes messages for every window in the thread, so the
later suites are dispatched from inside the dialog's own loop.

**What is actually true:** without the hook the process never exits. The run falls back on the
gate's kill backstop, takes the full poll ceiling — **40s against 20s, measured** — and returns an
exit code that means nothing.

All seven copies now say that, including the four I had just written the wrong version into.

---

## Two gate defects the dialogs forced out

**About's AUTOCLOSE closed the whole editor.** It posted `WM_CLOSE` to the **main** window so that
a run testing About alone would terminate. With everything armed it raced
`MSG_USER_PROCESS_COMMANDLINE` and won: the Explorer, Workspace and Find in Project suites — **169
assertions** — silently stopped running, and the gate's report simply had three fewer lines in it.
Nothing said why. Shutdown is its own concern now, `TIKO_SELFTEST_EXIT`, posted after the last
suite.

**A clean exit saves the config.** Making tiko exit properly — so the gate stops tree-killing it —
triggered tiko's normal save-on-exit, exactly as closing the editor by hand does. The first green
run **rewrote `settings.ini` with the gate's own session**. The tree-kill had been hiding it.

The gate backs both `settings.ini` and `tiko.tiko` up and puts them back; verified byte-identical
across a run. Suppressing tiko's save-on-exit would have been changing the **product** to suit a
test.

---

## And then the screenshot

The author looked at the result and found two things no assertion could: the tab still read
**"Licenses"** for a page that now shows one, and the hero still carried a **"Proprietary"** pill
beside "GPL v3".

Both are gone. Id 362 is singular in all six `.lang` files with a real translation in each
(`Lizenz`, `Licencia`, `Licence`, `Lisens`, `许可证`), and **id 397 is now genuinely unused and
blanked** — never renumbered, so the slot is free.

> **THE ORDER MATTERED.** I blanked 397 one commit earlier, on the reasoning that the proprietary
> licence was gone, and had to put it back: it still had three live references. It labelled the
> hero pill, which describes the *components* and was still true at that point. The commit that
> removed the pill is what made blanking it correct. **Grepping before committing is what caught
> it, not the gate** — a lang id with no references is not a failure anywhere.

The pill row also stopped depending on a translation: it was three pills and the widest was the
translated word. Both survivors are literals, so it can only overflow now if the version string or
the DPI grows. The fit assertion stays — the row is painted, so an overflow is still clipped in
silence.

`frmAbout` 27 → 26 assertions: the id-397 check had nothing left to assert.

---

## Verification

| gate | result |
| --- | --- |
| `_check_selftests.bat` | **33 suites, 20,328 passed, 0 failed** |
| user state after a run | `settings.ini` and `tiko.tiko` **byte-identical** |
| `_check_scihost`, `_check_app_layer`, `_check_shell`, `_check_package` | green |
| `_check_app_standalone` | 18 clean, **debt 0, no baseline** |
| shell `--selftest` | **395**, encoding **48** |
| both binaries | exit 0, **0 warnings** |

**NOT VERIFIED BY ME:** how the About box looks, and anything on Linux. The author's
screenshot of the first version is what produced the section above.

---

## What this step is really about

Step 14's lesson was that a suite nothing runs is indistinguishable from a suite that passes.
**Step 15 is the same lesson one level up: a REASON nothing tests is indistinguishable from a
reason that is true.**

The buffering claim survived long enough to be copied into three files. My replacement for it
survived about twenty minutes — because a revert-to-red happened to test it. The difference
between those two lifetimes is not care. It is whether anything ran.

---

## What is left

1. **Font fallback's remaining gaps** — per-face metric normalisation, non-ASCII family names, the
   Linux `.ttc` face index. Recorded in [`7c-step12.md`](7c-step12.md).
2. **Linux.** Nothing in phase 7c has been executed on that platform at any point.
