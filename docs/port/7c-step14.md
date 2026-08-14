# Phase 7c, step 14 — the suites nobody was running

Step 13 found `TIKO_OPTIONS_SELFTEST` reporting **11 passed, 6 failed**, and established it was
not step 13's doing. Investigating it produced two findings, and the second is much larger.

---

## The oracle was wrong, and its passes were worse than its failures

`OptionsRows_Init` and `OptionsRows_RunBindTest` disagreed about **every** label id. Checked
against `settings/languages/english.lang`, **the row table is right** — each id names exactly the
setting its trailing comment claims. The oracle's ids named something else:

| the oracle asked for | what that id actually is | the right id |
| --- | --- | --- |
| 216 "Ask before exiting" | **Description** | 212 |
| 275 "Restore previous session" | **Windows API Keywords** | 269 |
| 129 "Use Compact Menu Interface" | **Font (applies to all styles)** | 128 |
| 88 "Check for Updates" | **Cannot Open Browser** | 87 |
| 81 "Default encoding for new files" | **Next Function** | 80 |

Those five found no row and failed honestly. **The other four found a real row — the wrong one —
and passed.** `BINDCHK(115, MultipleInstances)` matched *Enable Auto Indentation* and agreed
because both happened to be `-1`. And the single `ROW/FIELD MISMATCH` the suite reported was its
own: id 120 is *Show Indentation guides*, value 0, compared against `LineNumbering`, value -1.

**The suite was describing itself, not the code.**

### The defect behind the defect

The macro compared a **value**. Every field it checks is a boolean stored as 0 or -1, so **any two
unrelated rows agree half the time.** It now pins three things per row:

* **The pointer.** `gOptRows(n).pField` must *be* `@gOptWork.<field>`. Coincidence cannot survive
  it — demonstrated rather than asserted: binding the *Allow multiple instances* row to
  `CompileAutosave`, which shares its default of `true`, is caught here and would have been
  invisible to the old check.
* **The value**, still — it proves `OptionsWork_Load` actually copied `gConfig`.
* **The visible text**, which is the only thing that could ever have caught the original defect.
  An id pointing at the wrong *string* is invisible to both a value check and a pointer check.
  English-only, and it prints **NOT VERIFIED** with a count when another `.lang` is loaded, rather
  than skipping quietly.

The list stays hand-written and **not** derived from `OptionsRows_Init`. That file's own header
says an oracle copied from the thing it checks proves nothing, and that rule had to survive the
repair.

**11 passed / 6 failed → 26 passed / 0 failed.**

---

## The larger finding: twenty-five report lines, no gate

`Theme`, `Encoding`, `Save`, `CompileCmd`, `CopyData`, `IniParse`, `FontFile`, `Debug`, `Unused`,
`Format` (×2), `AutoInsert`, `FormatOptions`, `Keyboard` (×3), `InputBox`, `Codetip`,
`NavHistory`, `FileWatch`, `FindProj` (×2), `Workspace`, `Explorer`, `Options` — **every one fires
only behind an environment variable inside a started GUI, and no `_check_*.bat` started one.**

Armed together they report **19,965 assertions**. That is what was sitting unrun, and it is why
one of them could fail indefinitely. **Correcting one oracle does not stop the next one.**

### `_check_selftests.bat`

The mechanism is `_check_package.bat`'s, deliberately: launch with output redirected, poll with a
ceiling rather than a fixed sleep, tree-kill by PID, `WaitForExit` before reading. A second way to
start a GUI from a batch file would be a second thing to keep working.

> Step 11's report said suites like these *"could not be run here"* because they need the GUI.
> Step 12 showed otherwise using exactly this trick. That correction is what made this gate
> obvious in step 14 — three steps after the claim was made.

**It parses every line matching `<n> passed, <n> failed`**, because the suites print in four
different shapes — dashes, a trailing full stop, equals signs, and bare. A parser keyed to one of
them silently ignores the rest, which is this gate's own failure mode in disguise.

**And it asserts a minimum suite count.** Summing failures alone is not a gate: a tiko that
crashes or exits before the self-test block prints nothing, sums to zero failures and passes.

**`MINSUITES=25` was measured, not counted.** The first draft said 21 — the number of suites the
source *looks* like it has — and the run reported 25, because the keyboard suite prints three
lines, `Format` two and `FindProj` two. A count of report *lines* is what this can actually check,
so that is what it checks.

Totals are printed, not just failures, so a **drop** in assertions is visible. A suite that
quietly stops running is the same defect as one that quietly fails.

---

## Verification

| gate | result |
| --- | --- |
| **`_check_selftests.bat`** | **25 suites, 19,965 passed, 0 failed** (new) |
| `_check_scihost`, `_check_app_layer`, `_check_shell`, `_check_package` | green |
| `_check_app_standalone` | 18 clean, **debt 0, no baseline** |
| tiko `_compile_fast`, `_compile_shell` | exit 0, 0 warnings |
| shell `--selftest` | **395**, encoding **48** |
| PsPlatform `build.cmd check` | **48 suites** (untouched this step) |

**Three rules reverted to red**, and the third is the one worth reading:

```
a row bound to its neighbour field     -> 25 suites, 19964 passed, 1 failed   exit 1
a label id pointing at another string  -> 25 suites, 19963 passed, 2 failed   exit 1
one suite stops running entirely       -> 24 suites, 19948 passed, 0 FAILED   exit 1
```

**The third has zero failures and the gate fails anyway.** Without the count rule that would have
been a pass — which is precisely how this step's subject came to exist.

**NOT VERIFIED BY ME:** anything visual, and anything on Linux.

---

## What this step is really about

Every stale claim this port has found so far was in **prose** — a comment, a handoff entry, a
commit message asserting a blocker that had already gone. Four of them in `_check_app_standalone`
alone.

This one was in a **suite**, and that is worse. Prose that rots is at least read. A suite that
rots reports a number, and a number carries authority that a sentence does not: *"11 passed"* was
believed for as long as it was printed, and eleven of those passes were false.

**The lesson is not "fix the oracle".** It is that a suite nothing runs is indistinguishable from
a suite that passes, and the only difference between the two is a gate.

---

## What is left

1. **Font fallback's remaining gaps** — per-face metric normalisation, non-ASCII family names, the
   Linux `.ttc` face index. All recorded in [`7c-step12.md`](7c-step12.md).
2. **The dialog-gated suites.** `_check_selftests.bat` covers what runs at STARTUP. The
   `*_AUTOOPEN` family — About, BuildConfig, Keyboard, UserTools, ProjectOptions, HelpCenter,
   FormatOptions, Unused — drives real dialogs and is not in it. Whether a gate can drive those
   without becoming interactive is an open question, and **it is exactly the question this step
   answered "no" to once already**, wrongly.
3. **Linux.** Nothing on that platform has been executed at any point in phase 7c.
