# Phase 7c, step 6 — the project tier, and the number that changed step 5's answer

Step 5 added a symbol scan and measured it at **4–20ms**, concluding that a synchronous parse
was imperceptible and threading was unjustified. Step 6 added the project tier — 39 lines of
code — and measured that.

**The measurement overturned step 5's conclusion.**

---

## THE MEASUREMENT

| root | files reached | symbols | buffer scan | project scan |
| --- | --- | --- | --- | --- |
| `tikoshell.bas` | 5 | 710 | 19ms | 19ms |
| `tiko.bas` | **134** | 4,496 | **1,244ms** | **1,212ms** |

Second run on `tiko.bas`: 1,206ms and 1,225ms. Stable.

**STEP 5'S NUMBER WAS RIGHT FOR WHAT IT MEASURED AND WRONG AS A GENERAL CLAIM.** It only ever
scanned files whose `#include`s did not resolve. **`fbcparser_scan_text` follows includes too** —
the *buffer* scan of `tiko.bas` reports the same 4,496 symbols from the same 134 files as the
*project* scan of it.

**So the cost is not a property of the tier. It is a property of the include graph, and both
tiers pay it.**

### What that means for this binary, plainly

Editing `tiko.bas` in this shell would stall **1.2 seconds** every time typing paused (the
debounce), again on **every tab switch**, and **twice at startup** (buffer + project).

**That is the first hard evidence for a thread in six steps.** It did not come from an argument,
and it did not come from the step that was supposed to produce it — step 5 asked the question and
got an answer that happened to be true only of small files.

---

## What the tier bought

`gSymDb`'s buffer tier holds one result set, so before this the Functions pane could only ever
list **the file you were looking at**. Now:

* a project scan follows `#include`s from disk, so other files are in the database;
* `RecomputeContrib` (`clsSymbolDb.inc:290-317`) suppresses the project tier's copy of whichever
  file the **buffer** owns, so the active file keeps its live-as-you-type symbols and everything
  else comes from the project set;
* **the panel needed no change at all** — `EnumProcsInFile` has always searched both tiers.

The whole commit is **one function and two call sites**: 39 code lines in `shellscan.bi`, plus
the triggers where tiko has them (after the command line's files open, `frmMain.inc:63`; after a
successful save, `frmMainFile.inc:241`).

### The root and the include paths — tiko's rule, degraded honestly

Root is `gApp.GetMainDocumentPtr()` — step 5's `SetProjectFileType` assigns `FILETYPE_MAIN` to
the first `.bas` opened, which is exactly what tiko roots at (`clsScanMgr.inc:311`). Include
paths are the root file's own directory **only**: tiko adds `gConfig.CompilerIncludes` and the
configured compiler's `inc` directory, and this binary never loads `settings.ini`, so both are
empty — the same state tiko is in for a loose file outside a project.

That is why `tikoshell.bas` reaches 5 files and `tiko.bas` reaches 134: tiko's includes are all
relative to its own directory, and the shell's are not.

---

## What is NOT verified

**The pane has not been seen showing two files.** The rows are asserted; the screen is not.

**Nothing has been typed into a 134-file document.** The 1.2 seconds is measured inside the
scanner and printed to a console — whether it *feels* like a freeze, and whether SDL's event
queue survives it intact, is unmeasured.

**The project tier reads from DISK.** A background tab with unsaved edits contributes its last
*saved* symbols. tiko has the same property and reconciles it the same way — by rescanning after
a save — but a file edited and not saved shows stale entries under its own heading.

**No include-path configuration exists**, so a real project (one whose sources live in several
directories) would reach fewer files here than in tiko.

---

## What is verified, and by what

| claim | how |
| --- | --- |
| a project scan reaches the included file | two probes, one `#include`ing the other |
| the non-active tab has symbols again | `EnumProcsInFile` on the file the buffer tier evicted |
| the pane lists BOTH files | 5 rows — two headers, three procedures |
| **no duplicates** | each file's procedures counted exactly once; a wrong merge shows as doubles, not a crash |
| the buffer tier still owns the active file | the tab-switch rescan assertions from step 5, unchanged |
| the layout did not move | dump byte-identical |

**Reverting the project scan fails three assertions and prints `tab0=0 tab1=1`** — the one-file
limit returning, which is the cleanest possible demonstration that the tier is what fixed it.

### Two assertions that did their job by failing

Step 5 asserted the one-file limit and left a note: *"if a project tier ever arrives these fail
and say so."* It arrived. They failed. They are replaced by what is now true.

**That is what an assertion about a known limitation is FOR** — not to defend the limitation, but
to make its removal impossible to do quietly.

---

## What step 7 has to decide

1. **Threading, and now there is a number.** 1.2 seconds on the UI thread, on the debounce, the
   tab switch and startup. This has been item 4 on the handoff's list, demoted in step 5 for
   want of evidence; the evidence exists now. PsPlatform surfaces no threading service at all,
   though SDL3's `SDL_CreateThread`, `SDL_CreateMutex` and `SDL_CreateCondition` are already
   vendored.
2. **Or bound the work instead of moving it.** A parse that followed no includes would be fast
   and would list one file — which is what step 5 shipped. The choice between "make it async"
   and "make it smaller" is a product decision, not a porting one.
3. **The three `PsListTree` gaps** from step 4, still open and still worked around.
4. **Encoding detection on read**, still outstanding from step 3.
