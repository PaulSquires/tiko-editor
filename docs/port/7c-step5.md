# Phase 7c, step 5 — the Functions panel, and the thread that was not needed

Step 4 ported the Bookmarks panel and produced the port's first per-form number, **1.35**,
explicitly a floor. The handoff then put **threading in PsPlatform** first for step 5 — the
largest single blocker, on the grounds that the Functions panel needs `clsScanMgr` and
PsPlatform exposes no threading at all.

**Measuring killed that before a line was written**, and that is the headline:

* The panel reads **`gSymDb`**, not the scanner (`frmFunctions.inc:448`).
* **`clsSymbolDb` and `PARSERESULTSET` are already in `app/`.** The whole data path — parse
  result → indexes → symbol database → panel — was inside the portable layer already.
* The parse is **one DLL call**. Everything else in `clsScanMgr`'s 544 lines is queueing,
  locking, retiring and thread lifetime.
* **`gAppNotify.RequestBufferScan` was already a seam field**, already called by
  `clsDocument.LoadDiskFile`, and stubbed in this shell.

So the panel did not need a thread. It needed that stub filled in.

---

## THE MEASUREMENT

`fbcparser_scan_text`, on the UI thread, on real files:

| file | bytes | lines | ms | symbols |
| --- | --- | --- | --- | --- |
| `shellscan.bi` | 6,580 | 109 | 5 | 17 |
| `shellpanel.bi` | 22,910 | 390 | 4 | 53 |
| `clsSymbolDb.inc` | 41,470 | 887 | 20 | 211 |
| `clsDocument.inc` | 70,691 | 1,615 | 14 | 518 |
| `tikoshell.bas` | 260,442 | 4,139 | **18** | 624 |

Stable on a second run (19 and 13 for the last two), and **not proportional to size** — 260 KB
in 18ms while 41 KB took 20ms, so the cost is dominated by something other than the file.

**A BUFFER-TIER PARSE ON THE UI THREAD IS IMPERCEPTIBLE**, and the debounce means it happens
when typing stops rather than per keystroke. **The threading case that survives is the PROJECT
tier** — every file in a project at once — which this binary has no notion of.

---

## What the panel cost

| | |
| --- | --- |
| ported from | `frmFunctions.inc`'s loader, file list and click arm — **125 code lines** |
| replaced by | `shellpanel.bi` additions — **73 code lines** |
| the scanner | `clsScanMgr.inc` is **382 code lines**; `shellscan.bi` is **112** |
| assertions added | 35 (306 → **341**) |
| total diff | **1,004 insertions** across 8 files |
| commits | 4 code + this one |

**BOTH RATIOS ARE UNDER 1, AND THE NUMBER FLATTERS THE WORK.** The shell's panel does less:
one pane instead of three behind an icon strip, a name instead of a name and a prototype, no
tooltips, no flat/tree toggle, no Explorer filter — and, below, only one file's symbols at a
time. Step 4's 1.35 was a *whole* form ported faithfully; this is a *subset* ported cleanly.
The honest reading is that **the per-form cost depends far more on what the platform already
provides than on the form's size** — 382 lines of `clsScanMgr` became 112 because most of it
was thread machinery, and none of that is a property of the Functions panel.

---

## The real limit, and it is not the thread

**`gSymDb`'s BUFFER TIER HOLDS EXACTLY ONE RESULT SET**, and `InstallSet` replaces. The shell
fills only that tier, so **the symbols in the database are always the last file scanned** —
and the Functions panel, which loops over every open tab, can only ever find rows for one of
them.

tiko does not have this limit: its **project tier** covers every file in the project, scanned
once. That is what makes tiko's Functions panel list an entire workspace.

So the shell scans on tab switch (tiko's own site, `clsTopTabCtl.inc:271`), which keeps the
panel describing the document in front of the user and costs one parse per switch. **The
Functions pane here is "the active file's procedures", not "the project's".** Asserted, not
merely described, so that a future project tier fails those assertions and says so.

**THAT — not threading — IS WHAT THE NEXT STEP OF THIS PANEL NEEDS.**

---

## The list collapses while a function is half-typed — and that is tiko's behaviour

Reported by the author on the running binary: typing `function paul() as long` at the top of a
file leaves **only `paul`** in the Functions pane until `end function` is typed.

**Measured rather than reasoned about.** The same file, scanned twice:

| file | symbols |
| --- | --- |
| two complete `sub`s | **2** |
| the same two, with an unterminated `function` above them | **1** |

fbcParser treats everything after an unterminated `function` as that function's **body**, so the
procedures below it stop existing. The panel is faithfully showing what the parser returned.

**tiko DOES THE SAME.** `clsSymbolDb.inc:297` suppresses the PROJECT tier for whichever file the
BUFFER scan is rooted at — so while a file is being edited, the buffer's answer is the only
answer there too, project scan or not. This is not a shell divergence; it is what "parse the
buffer as you type" costs, and it corrects itself on the next complete parse.

**LEFT ALONE, DELIBERATELY** (author's decision, 2026-08-10). The two mitigations both cost more
than the flicker: keeping the previous list when a scan finds fewer symbols also hides a genuine
deletion, and refusing to install a result that carries diagnostics freezes the list for any file
the parser complains about at all. Either would also be a behaviour this port then owes tiko, or
a divergence to explain forever.

---

## What is NOT verified

**~~NOTHING IN THIS STEP HAS BEEN SEEN ON SCREEN.~~ CONFIRMED BY THE AUTHOR, 2026-08-10:** the
Functions pane lists a file's procedures, and **typing updates it** — which is the first time a
key has been pressed in this binary in any commit of this port, and therefore the first run of
the whole path from a keystroke through `PsSciNotify` into the debounce and out to a refreshed
list. Every piece of that was asserted separately; none of it had ever run end to end.

**Still unseen:** clicking a function row (asserted by row number only), and the panel at any
scale but the author's.

**The parse timings are from the console, not from a stopwatch on the UI.** 18ms is measured
inside `ShellScan_Buffer`; whether a tab switch *feels* instant is not something the number
proves.

**What did not port from `frmFunctions`:** `ExpandAll`/`CollapseAll` (no commands wired), the
paint callback (the widget paints itself), the tooltip callback, the flat/tree toggle, and the
Explorer filter (there is no Explorer). **`NavHistory` still does not come with the jumps** —
`modNavHistory.inc` is not linkable in this binary (it reaches `gTTabCtl` and
`hWndActiveScintilla` at five sites), which step 4 recorded when the bookmark commands hit the
same wall.

**The shell's `FilenameOriginalCase` is the identity**, so the symbol database and TODO store —
both keyed by filename — would treat two spellings of one path as two files. Real, and not
reachable today because every path comes from the same place.

---

## What is verified, and by what

| claim | how |
| --- | --- |
| the seam fires the scan on its own | open a file, `gSymDb` has its procedures — nothing in the test asks |
| declarations are not listed | a `declare sub ProbeGamma()` in the probe; reverting the skip shows it as a fourth row |
| the debounce restarts on edits and NOT on styling | the policy driven exhaustively, pure |
| the timer fires once per pause | PsTimer's synthetic clock, no sleeping |
| the pane switch | both commands through `OnMenuCommand` |
| clicking a function jumps to its body | driven by row number; the header still goes nowhere |
| the buffer tier's one-set limit | two documents, asserted both ways |
| the layout did not move | dump byte-identical at every commit |

**Seven rules were reverted. Five went red immediately; two needed the TEST improved before
they would**, and both are recorded in place:

* **The timer's one-shot behaviour is double-covered.** `PsTimerKillProc` in the fire handler
  and `bRepeat = false` on the arm each achieve it, so breaking *either* leaves "fires once and
  is gone" green. Breaking **both** turns it red. Both are kept, for a reason the comment
  states.
* **The declare-only skip could not bite** until the probe file gained a
  `declare sub ProbeGamma()`. With it, reverting the skip shows the declaration as a fourth
  row.

---

## Three defects, and where each came from

1. **The scanner read whatever the view was showing.** `GetText` reads the *active* view and
   this binary has one view for every tab, so scanning a background document parsed the
   foreground one and filed the symbols under the wrong name. **The suite found this** — a tier
   assertion printed `tab1=3` for a file with no procedures — which is notable because in
   steps 3 and 4 every defect came from running the program. **It is the same defect the
   bookmarks loader already documents**, written two commits earlier and not carried across.
2. **A guard that guarded nothing.** The declare-only skip survived being reverted, because the
   probe file had no declaration in it.
3. **Three assertions measuring the setup rather than the code** — a stale scan order, three
   stale rows behind `SetMode`'s deliberate early-out, and a `ShellTabs_Show` to the tab that
   was already current. One lesson: **a suite whose blocks share global state asserts on what
   the previous block left.**

---

## What step 6 has to decide

1. **A project tier, or accept "the active file" as the Functions pane's scope.** This is the
   panel's real limit and the first thing a user would notice.
2. **Threading — now with a number against it.** Nothing in this step needed it. The case is
   the project tier and anything else that parses more than one file at a time.
3. **The three `PsListTree` gaps from step 4**, still open and still worked around at the call
   site.
4. **Encoding detection on read**, still outstanding from step 3.
5. **An interactive pass on the two panels**, which is the one thing this step has none of.
