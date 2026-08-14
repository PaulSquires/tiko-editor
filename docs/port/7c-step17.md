# Phase 7c, step 17 — Linux with a window

Step 16 was headless. This is the first time in 7c that anything was **displayed** on Linux, at
150% scale on Fedora Workstation, under both backends.

**Three defects were reported. They had three different verdicts, and telling them apart was the
entire step.**

| reported | verdict |
| --- | --- |
| ideshell: tabs click but do not change the text | **not a defect** — nothing was ever wired |
| ideshell: splitters draw but do not drag | **cross-platform, pre-existing** — never worked anywhere |
| minieditor: Ctrl+Space flashes and vanishes | **genuine Linux defect** |

---

## What passed

**`platformprobe`: 28/28 on Wayland.** The most informative output of the two steps:

* backend `wayland`; positioning and self-raise **correctly reported unavailable**, with
  assertions confirming the platform layer knows Wayland's constraints rather than assuming
  Windows'
* pixel density **1.5**, and `input is converted to pixels by the backend` — the Gate 4 fix from
  an earlier phase holding up, and why hit-testing was clean at 150%
* clipboard round-trips including non-ASCII; a worker-thread event reaches the pump with its
  payload

**`widgets` ran under X11 and Wayland**, and the author reports hit registration and slider
dragging correct at 150%.

---

## The tabs were never wired

`PsTabBar` has an `OnSelect` callback. **ideshell never sets one.** Three captions over a single
editor, with nothing connected to change the document — the highlight moving *is* the control
working.

Worth recording because it presented as a Linux defect and behaves identically on Windows. The
cost of finding out was one grep.

## The splitters had never worked, on any platform

`nMin` and `nMax` both default to 0, and `SetPos` clamped to `[nMin, nMax]` **unconditionally**.
So every position clamped to 0, `nPos` never changed, and the `OnMove` callback never fired.
**Nothing in the repository has ever called `SetRange` on a splitter** — checked across every demo
and every suite. The control has been frozen since it was written.

It looked like a Linux defect for two rounds because Fedora is simply where a human first tried to
drag one. **The author confirmed it fails identically on Windows**, and that single comparison is
what reclassified it.

**demos/ideshell needed no change**, which is the part worth keeping. It clamps host-side on
purpose — *"CLAMPED HERE, not in the splitter"* — and its header explains that a splitter reports
a position and never moves its neighbours. The demo was written correctly against the documented
contract; the library was the side that did not honour it. A splitter with no range is unbounded
now.

### The suite that should have caught it was already there, and passing

`tests/psdrag` drives the widget properly — real events, real `Dispatch`, capture asserted. Its
fixture opens with:

```
spl->SetRange(50, 350)
```

**which is the one thing no actual caller ever did.** The test was good; its fixture configured
the control into working order before testing it, so the broken default was invisible.

That is a new variant of this document's recurring shape. Not an untested claim — a **fixture that
hides the default**. Three assertions added for a splitter with no range; reverted to red, they
report `got 0, wanted 210` and the move never fired. `psdrag` 64 → 67.

## The autocompletion popup was a real port defect

Scintilla's autocompletion list is a **real window**. On Linux — X11 and Wayland alike — SDL gives
a popup the keyboard, so the editor receives a genuine focus-lost event the instant the list
appears. `PsSciDispatch` forwarded it to Scintilla, Scintilla cancels autocompletion on losing
focus, and the list dismissed itself in the frame it opened.

On Windows the popup does not take activation, the editor never loses focus, and the whole path is
dead code.

**The focus had not gone to another application.** It went to a window this editor owns, and for
Scintilla's purposes the editor still has it. `PSEV_FOCUS_LOSE` is no longer forwarded while our
own list is up.

`g_sciAcOwner` is claimed **before** `OpenAt`, not after: `OpenAt` creates the window, and the
focus-lost event can arrive before it returns.

---

## What actually solved it: the discriminating observation

**Two of my diagnoses died here, and the record matters more than the fixes.**

I called the popup **Wayland-specific** and predicted X11 would fix it. X11 behaved identically.
The prediction was the useful part — it was wrong in a way that could be seen in one run, which is
the only reason it did not survive.

Before that I had **not checked whether Ctrl+Space worked on Windows at all**, having called it a
Linux defect for two rounds on that assumption. minieditor uses the same SDL popup path on both
platforms; tiko does not, because it has its own `frmAutoComplete` and never exercises Scintilla's
ListBox.

What settled both was the author's aside:

> *"Right click popup menus work perfectly."*

Same `PsPopupHost`, same `PSSURF_POPUP`, same grab, same compositor. That ruled out the popup
machinery in one sentence and left the only thing that differs: a context menu's lifetime is not
tied to Scintilla's focus state, and the autocompletion list's is.

**And the two Windows runs are what separated the splitter from the popup.** Identical symptoms —
"works on Windows, broken on Linux" versus "broken on both" — and nothing but running both could
tell them apart. One was a port defect and one had never worked anywhere.

---

## Verification

| | |
| --- | --- |
| `platformprobe` on Wayland | **28/28**, density 1.5 |
| `widgets` | ran on **X11 and Wayland**; drag correct at 150% |
| `ideshell --selftest` | **48/48** — and it drives `OnSplitV` directly, so it never touched the bug |
| PsPlatform `build.cmd check` on Windows, after both fixes | **48 suites, 0 failures** |
| `tests/psdrag` | 64 → **67** |
| **author, on Fedora, after both fixes** | **autocompletion holds open; splitters drag** |

**NOT VERIFIED BY ME:** both fixes. Windows cannot see either bug — one is invisible there by
construction and the other was equally broken. Every confirmation in this step came from the
author running it.

**Still never executed anywhere:** the fontconfig path in `PsFont.inc`.

---

## What is left

1. **`tikoshell` on Linux.** The portable binary lives in the *tiko* repo and needs both trees
   checked out side by side. It has never been built for Linux.
2. **fontconfig.** Never executed on any machine.
3. **Font fallback's remaining gaps** — per-face metric normalisation, non-ASCII family names, the
   Linux `.ttc` face index. Recorded in [`7c-step12.md`](7c-step12.md).
