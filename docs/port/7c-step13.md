# Phase 7c, step 13 — the live list empties

Four items closed. **Two of them were not work**, and a third took a quarter of the code its
entry predicted, because the blocker it named had already stopped being one.

---

## The last link debt, and the tab control was never the blocker

`_check_app_standalone` had sat at **debt 1** for three steps with this against it:

> *"What remains is `clsConfig::ProjectSaveToFile`, and THAT one is blocked on something real: it
> calls `gTTabCtl` — `clsTopTabCtl`, Win32 to its bones — at four sites. It cannot close until the
> tab control does, which is the largest open item in the port."*

**`clsTopTabCtl` was not touched.** A project file records *which documents are open and in what
order*, and that is a question about a list. Four fields:

```
TabCount  TabDocAt  TabActiveIndex  TabIndexOfDoc
```

Both implementations already existed — tiko's `clsTopTabCtl` and the shell's tab array, built in
step 3 commit 6 — which is the arrangement every other difference in this seam already has.

**Five calls became four fields, and the fifth became logic.** `SaveActiveTabIndex` was twenty
lines of counting inside a Win32 control, with no Win32 in them, and the rule it encodes is
subtle: a project records only documents with a file on disk, so the saved index counts positions
in the **written** list, not in the tab bar. Made a fifth seam field it would exist twice, in two
hosts, with nothing checking they agreed. It is `Tabs_SaveActiveIndex` in `app/`, and the shell's
suite reaches it.

**Closing it exposed the next one, inside the same commit.** With `ProjectSaveToFile` in `app/`,
`ProcessToCurdriveProject` became undefined; it moved too — pure `gApp` and PsCore, on the same
*"moves as soon as something needs it"* list `ProcessFromCurdriveApp` was on in step 5. **1 → 1 →
0.** That is exactly what the counter is for.

**The shell's stub went, and its removal was not planned.** The shell failed to compile the moment
the move landed: its `"no project system yet"` body had become a duplicate definition. Both
binaries run the same project writer now.

**The baseline is GONE, not set to 0.** The file said to delete it when the count reached zero, and
a baseline of 0 would have made the next undefined symbol report as *"debt"* rather than fail.

`ProjectLoadFromFile` deliberately did **not** move — `GetCursor`/`SetCursor`,
`LoadExplorerFiles`, `frmMain_*`, `frmOutput_*`, `frmUserTools_*`, `frmBuildConfig_*`. It is shell
code and it is not a link debt.

## `clsTopTabCtl`: the tree had already answered

The item asked *"portable rewrite, or a Win32 facade forever?"*. Neither: **there are two
implementations behind one seam**, `src/shell/shelltabs.bi` (311 lines on `PsTabBar`) and
`src/clsTopTabCtl.inc` (424 lines of Win32), and that is the pattern the whole port runs on. It
was a facade only while something in `app/` had to reach through it, which stopped being true
above.

## The Character Set combo comes out

Nineteen GDI charsets in a control that had done nothing since the Scintilla swap —
`SCI_STYLESETCHARACTERSET` reaches `PlatPs.cxx`, which renders through FreeType and has never
consulted `lfCharSet`.

**Step 12 removed its last possible purpose.** A charset was GDI's way of saying which face should
cover a script; the coverage chain answers that per codepoint now, from Windows' own font-link
table, without being told.

**It was not harmless while it sat there**, which is why removing beat leaving. A control that
cannot affect anything gets tried FIRST when something looks wrong — and it was, costing two wrong
diagnoses during the Korean investigation before anyone opened the renderer.

The `settings.ini` **read** goes as well as the write: an existing file keeps its line, an
unrecognised key is ignored, and nothing has to rewrite anyone's settings. **Lang id 286 is
blanked in all six files and never renumbered** — a blank is a free slot the next phrase claims;
renumbering silently shifts every id after it. Control id 9403 is likewise retired rather than
reused.

## Two tiers, one worker — the question was wrong

Open for six steps on the observation that a buffer scan and a project scan serialise. It is not a
scheduling decision. `fbcParser.bi:166-169`:

> *"THREADING CONTRACT: the engine is a single global compiler instance. Exactly one scan may run
> at a time, and all calls must come from the same thread … a second concurrent call returns
> `FBCP_E_BUSY` as a safety net, not as a synchronization mechanism."*

fbcParser is a fork of the fbc front end, with module-level state in `symb.bas`, `fb.bas` and
`lex.bas`. A second worker would block on the same global instance or corrupt it. **The
same-thread half of that contract was already visible from both worker loops** — it is why the
retire queue exists, in both binaries — and neither loop said what it was a consequence of. Both
say now.

The `1.3s + 1.3s` figure it was argued from does have a source (`7c-step7.md`) and is beside the
point: whatever the number, a second worker cannot overlap two scans it is not allowed to start.

---

## Verification

| gate | result |
| --- | --- |
| `_check_app_standalone` | **18 clean, 0 errors, NO BASELINE** — a plain failure now |
| `_check_scihost`, `_check_app_layer`, `_check_shell`, `_check_package` | green; 48 app files, 5 shell files |
| tiko `_compile_fast`, `_compile_shell` | exit 0, **0 warnings** |
| shell `--selftest` | 383 → **395** |
| encoding suite | **48** |
| `TIKO_FONTFILE_SELFTEST` | **13** |
| `TIKO_THEME_SELFTEST` | **929** |
| PsPlatform `build.cmd check` | **48 suites** (untouched this step) |

Three rules reverted to red, each failing for its own reason: the saved index reduced to the tab
index, the untitled-active case removed, and `TabDocAt`'s bounds check removed.

### Two things the suites caught that the code did not

**The first draft of the tab assertions was wrong.** It assumed the files opened earlier in
`--selftest` were still tabbed; they are not, because that scope restores the tab state it found.
Three assertions failed and **one passed for the wrong reason** — *"TabDocAt agrees with the
array"* compared 0 with 0. The block opens its own tabs now.

**Two files had picked up mixed line endings from an append**, and the anchor assert in the
revert-to-red script is what caught it — the second time that guard has earned its place.

---

## A failure that is NOT this step's, and is in no gate list

**`TIKO_OPTIONS_SELFTEST` reports "11 passed, 6 failed", and did so before this step.** Verified
by stashing the whole charset commit, rebuilding and re-running: the same six, line for line.

```
FAIL: no row carries label id 216 (Ask before exiting)
FAIL: no row carries label id 275 (Restore previous session)
FAIL: no row carries label id 129 (Use Compact Menu Interface)
FAIL: no row carries label id 88  (Check for Updates)
FAIL: Show line numbers row shows 0 but gConfig holds -1 -- ROW/FIELD MISMATCH
FAIL: no row carries label id 81  (Default encoding for new files)
```

**It is failing because nothing runs it.** It is not in the gate table, not in `_check_*`, and
only fires behind an environment variable inside a started GUI. That is the same shape as every
stale claim this port has found, in a suite rather than in prose.

Not fixed here: a six-assertion failure in the options binding is its own investigation, not a
rider on a deletion. **Step 14's first item.**

---

## What is left

**The live list is empty.** Every item on it has been closed, and four of the eleven closed as
*"the recorded blocker was not one"*.

Step 14 has no inherited list, so it starts from:

1. **The options bind suite**, above — six real failures, and the gate coverage that let them sit.
2. **Font fallback's remaining gaps** — per-face metric normalisation, non-ASCII family names,
   the Linux `.ttc` face index. All recorded in `7c-step12.md`.
3. **Linux.** Nothing on that platform has been executed at any point in phase 7c.
