# Phase 7c, step 3 — the document model leaves the shell

Step 1 built a shell that lays out like tiko. Step 2 collapsed the pump. Step 3 asks the
question those two deferred: **can tiko's document model be separated from Win32 at all, and if
it can, does a second host actually drive it?**

Both yes. `clsDocument` and `clsApp` are in `src/app` — **9,901 lines across 46 files** that
compile against PsCore alone — and `_shell\tikoshell.exe` opens files, tabs between them, and
**saves them to disk through the same `clsDocument.SaveFile` tiko calls**, with no AfxNova and
no `HWND` anywhere in its half.

**THAT IS ONE LAYER PROVED PORTABLE, NOT 7c GETTING SHORTER.** 7c is still 48 forms and
~45,000 lines. The document model is the layer underneath them; nothing on this page measures a
single form.

---

## What is NOT verified, and it is again the longer half

**EVERY USER-VISIBLE DEFECT IN THIS STEP WAS FOUND BY THE AUTHOR RUNNING THE PROGRAM.** Five
of them, in three sessions, each reported within a minute of opening the binary. Not one was
caught by 27 tiko suites, 46 PsPlatform suites, three ratchets, or the 241 shell assertions —
all of which were green while every one of them was live. That sentence is the most important
one on this page and it is the same finding step 2 ended on.

**NOTHING IN THE SAVE PATH HAS BEEN DRIVEN ON SCREEN IN THE SHELL.** Open, Save, Save As, the
file picker, the lossy-save prompt and the write-failure box are all unexercised by a person as
of this commit. What is asserted is composition — captions, buttons, defaults, cancel ids — and
the refusal paths. That a box *looks* right, wraps, centres, or returns the id the user pressed
is covered by nothing.

**THE SHELL DOES NOT DETECT ENCODINGS ON READ.** `ShellHost_LoadFileText` reads bytes with
`PsFileReadAll` and hands them over as UTF-8. tiko's read path decodes UTF-16 through
`WideCharToMultiByte` and stayed in the shell for that reason. **A UTF-16 file opened in
tikoshell and saved will not round-trip**, and now that saving works, that is reachable.

**THE WRITE PATH IS SHARED; THE PROOF THAT IT IS CORRECT IS STILL tiko's.** `TIKO_SAVE_SELFTEST`
(44) and `TIKO_ENCODING_SELFTEST` (43) run against `tiko.exe`. Both suites name Win32 directly —
`CreateFileW`, `WideCharToMultiByte` — and **deliberately did not move into `app/`**; the
candidate-mode checker found 17 violations across the two files in one run and refused the move
that step 3's plan opened with. So the shell's save is gated by suites that never execute inside
the shell.

**THE LINK DEBT IS 4, NOT 0.** `_check_app_standalone` links the whole layer against PsCore and
four bodies still live outside it — `FilenameOriginalCase` (real Win32), `TodoStore_RemoveFile`,
`KeyBindings_PickListKeyToValue` (declared in a *shell* header, which no token ratchet can see),
and `clsConfig::ProjectSaveToFile` (the class is split). The shell supplies its own for three of
them. The layer is portable *given four bodies a host must provide*, which is a weaker claim
than "portable".

**`clsTopTabCtl`, `clsScanMgr` and `modDocViews` did not move and are not going to as they
stand.** The first is a facade over a Win32 `PsTabBar` whose item data *is* the document list;
the second's worker thread is woken and stopped with Win32 event objects and **PsPlatform has no
threading or synchronisation service at all**. The shell has its own tab model (258 lines) and no
background parsing. See [`document-model-blockers.md`](document-model-blockers.md).

**THE SHELL HAS ONE VIEW PAIR AND tiko HAS ONE PER DOCUMENT.** Deliberate — the shell switches
tabs with `SCI_SETDOCPOINTER` rather than showing and hiding N widgets — but it means the shell
does not exercise the document model's view lifecycle the way tiko does.

**No file has been closed.** `ShellHost_CloseTab` prints. Nothing in the binary calls it.

---

## What is verified, and by what

| claim | how |
| --- | --- |
| the layer needs no AfxNova and no Win32 | `_check_app_layer` — vocabulary, 46 files |
| …and the compiler agrees, file by file | `_check_app_standalone` — 16 bodies compiled separately against PsCore alone |
| …and the whole layer LINKS | part two of the same gate, debt 4 (baseline, 6 call sites) |
| the shell reaches no Win32 shell header | `_check_shell`, 3 files |
| a second host can fill the seam | `tikoshell` implements 20 services + 11 notifications; `AppHost_IsComplete` asserted |
| the save path still works | `TIKO_SAVE_SELFTEST` 44/0, `TIKO_ENCODING_SELFTEST` 43/0 |
| tiko did not change behaviour | paired 27-suite sweep at the commits that touched shared code — byte-identical captures |
| the layout did not move | shell dump byte-identical to `layout-oracle/` at every commit |
| PsPlatform is not broken | `build check` 46 suites; `_check_scihost` after each PsPlatform commit |

**241 shell assertions, up from 194.** Every new rule was reverted to check its assertion went
red. The two added in commit 7 both bit; the two in commit 6 split — the bounds guard *crashes*
rather than failing, and the re-show guard is unasserted and says so in place.

---

## What step 3 found, which is the useful part

### 1. Three sizings of this work were wrong, and the compiler settled it

The plan said "`clsDocument` and `clsTopTabCtl` move into `app/`" and sized it from a grep: 15
blockers, then 40, then 40 different ones. **All three were wrong, two of them mine and made
with the checker rather than by hand.** Work stopped and everything was re-measured by
*compiling* — three commits of measurement (`83ced89b1`, `3324fb6c5`, `f2380fb7a`) before a line
of the move was written. That produced the two findings that changed the shape of the step:
`clsTopTabCtl` is not a document-model class, and `clsScanMgr` needs a platform capability that
does not exist.

**The measurement is the transferable part.** Every previous number came from a text search over
a language whose type names carry no prefix, and every one of them was wrong in a direction that
looked plausible.

### 2. The seam split in two, and the split earned its keep immediately

`AppHostServices` — the host **answers**, 20 fields. `AppHostNotify` — the document **tells**
the host, 11 fields. The load-bearing difference: **every `Notify` field can be safely no-op'd
and no `Services` field can.** `tikoshell` stubs six notifications legitimately — it has no TODO
pane, no Explorer, no MRU lists — and implements every service for real, because a stub there is
a lie the caller acts on. The first host to depend on that distinction was written the commit
after the split.

52 `gAppHost.` and 23 `gAppNotify.` call sites now exist inside `app/`, where Win32 calls used
to be.

### 3. `SciExec` and `SciMsg` are not the same function and the difference reaches the user

`SciExec(hWnd, …)` is a macro over `SendMessage`; `SciMsg(pSci, …)` calls Scintilla's direct
pointer. **409 and 410 sites respectively**, today
(`grep -ro "SciExec(" src | wc -l`). Converting the buffer helpers exposed a behaviour nobody
had written down: `SendMessage` flushes damage per message, the direct pointer does not — so a
strip-trailing-whitespace that used to repaint itself sixteen times now repaints never. One
`InvalidateView` after the edit replaces all sixteen. **A conversion that looks purely mechanical
changed what the screen does.**

### 4. Five defects, and what each one says about the gates

| defect | what any suite saw |
| --- | --- |
| both tabs opened blank | 223 assertions green — `AssignTextBuffer` creates views itself, so calling `CreateScintillaWindows` first made its own double-assignment guard fire |
| no caret on open, and none after a tab switch | green — focus is not asserted anywhere |
| clicks landed up-and-left of the pointer | green — `DispBubbleMouse` already localises coordinates and `PsSciView` subtracted its origin **again**. Invisible in tiko, whose views sit at (0,0). First host to put a `PsSciView` inside a larger tree |
| arrow cursor over text | green — no suite looks at a cursor |
| text doubled in size | green — my "fix" for the click offset. The shell pre-scales what Scintilla is told; `PlatPs_SetDpi` scaled it again |

**Three of the five were in PsPlatform, not in tiko**, and all three had been reachable by every
host in the tree for as long as the widgets have existed. Same shape as step 2's menus.

### 5. A crash that reported every assertion as ok

My own assertion block — added to prove the blank-tab fix — segfaulted the suite on the way out
for two commits. `SCI_GETDOCPOINTER` takes no reference, so pointing the view at a scratch
document freed the original, and restoring it walked freed memory. **It printed every assertion
as passing and then died**, so the only symptom was a missing summary line — and I was reading
that line with `grep -E "passed,"`, which prints *nothing* when the process dies first. I read an
empty grep as a pass more than once, and a fabricated count reached a commit message because of
it. Fixed with `SCI_ADDREFDOCUMENT`/`RELEASEDOCUMENT` and by running the suite three times and
reading the exit code.

**AN EMPTY GREP IS NOT A PASS.** Assert the count, not the absence of a failure line.

### 6. Two gates that measured nothing, both mine

* **A lossy-save assertion drove the live surface.** Harmless while the implementation was a
  `print`; the moment commit 7 gave it a real message box, that line would have put a modal on
  screen in a windowless run and blocked forever. Caught while writing the box, not by a run.
* **A paired 27-suite sweep compared HEAD with itself.** The BEFORE build rewrites `tiko.exe`,
  which is tracked, so `git stash pop` refused — and a failing native command does not throw
  under `$ErrorActionPreference`. The script built "AFTER" from the same tree and produced two
  identical captures. **An empty paired diff between a tree and itself is indistinguishable from
  a clean one.** The rerun asserts a marker string present → absent → present across the stages
  and checks the pop's exit code.

And a third, smaller: a scripted revert-to-red patch matched nothing because its pattern ended
in `\n` and the file is CRLF. One rule read as unasserted when the patch had simply never
applied.

---

## The shape the shell is now in

| | |
| --- | --- |
| `src/app` | 46 files, 9,901 lines, PsCore only, link debt 4 |
| `src/shell` | 3 files, 4,465 lines — `tikoshell.bas`, `shellhost.bi`, `shelltabs.bi` |
| what it does | lays out like tiko at every state, menus, accelerators, two dialogs, a file picker, N tabbed documents, open and save |
| what it does not | no Find, no Explorer, no project system, no background parsing, no encoding detection, no close |

---

## What step 4 needs to decide

1. **Encoding detection on read** is the first thing that makes the shell's file handling
   honest. It is `WideCharToMultiByte` today; PsCore has `PsEncoding`.
2. **Threading.** `clsScanMgr` is blocked on a platform service that does not exist, and it is
   not the only thing in tiko that will be. This is a PsPlatform decision, not a tiko one.
3. **Whether `clsTopTabCtl` gets a portable rewrite or stays a Win32 facade forever.** The shell
   proves the *model* half is small — 258 lines including comments.
4. **The four link-debt bodies.** Three are trivially host-supplied; `FilenameOriginalCase` needs
   a PsCore canonical-path call first.

**And one process decision, which this step earned the right to state plainly:** every remaining
milestone should be driven by hand before it is called done. Every claim on this page that a
suite supports survived; every claim about what the user sees came from a person opening the
binary.
