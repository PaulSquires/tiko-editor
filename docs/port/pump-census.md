# The pump census

Every message loop in tiko, every claim point in `frmMain`'s chain, and what each becomes under
PsPlatform. Fire counts come from `docs/port/pump-census/` — two driven sessions, not from
reading.

**This page's job is to answer one question: does the pump COLLAPSE, or does it port?** Step 1's
report named the pump as where the 14–20 week estimate's variance lives, and named nothing that
would settle it. The answer below is *collapse*, with one substantial residue, and the residue is
not where the plan expected it.

---

## First, the counts were wrong — all three versions of them

| source | pumps | `IsDialogMessage` |
| --- | --- | --- |
| `7c-step1.md`, from the original survey | 15 | 13 |
| this step's plan, from a grep | 19 | 16 |
| **actual** | **17** in tiko (+1 in debugParser's test harness) | **15** |

The plan's grep counted comment lines as sites. The first survey missed the `W`-suffixed API
forms entirely — `PsColorPicker` and `PsMessageBox` each own a `GetMessageW` pump and an
`IsDialogMessageW`, and neither appeared in any earlier list. **That is the fourth time a
hand-written list against this surface has come back short**, after the three vocabulary-ratchet
gaps `7c-starting-position.md` records. Re-derive, never cite.

The 17 are not one kind of thing, and separating them is most of the work:

| kind | n | what it is |
| --- | --- | --- |
| the main pump | 1 | `frmMain.inc` — the ordered filter chain |
| dialog pumps | 12 | one per modal form, all near-identical |
| control-owned modal pumps | 2 | `PsColorPicker`, `PsMessageBox` |
| drain loops | 2 | `frmFindInProject` — bounded `PeekMessage`, no filters |

---

## `frmMain`'s chain, claim by claim

Counts are claims in each driven session. Run 1 was ordinary editing; run 2 was driven
deliberately at the cold paths.

| # | claim point | run 1 | run 2 | verdict | becomes |
| --- | --- | --- | --- | --- | --- |
| 1 | `handleMouseShowScrollBar` | 0 | 0 | dissolves | widget hover state, in-tree |
| 2 | `PsMenuBar_FilterMessage` | 0 | **17** | dissolves | `g_menus.RouteEvent` — already in the shell |
| 3 | `ContextMenu_FilterMessage` | 0 | 0 | dissolves | `PsPopupHost.RouteEvent` |
| 4 | `PsTextBox_FilterMessage` | 0 | 0 | dissolves | `PsPopupHost` — one host, not one call per control |
| 5 | `PsListTree_FilterMessage` | **2** | 0 | dissolves | nothing: there is no `IsDialogMessage` to beat |
| 6 | `frmExplorer_FilterMessage` | **1** | 0 | **survives, as a RULE** | focus-before-accelerator precedence |
| 7 | `frmAutoComplete_FilterMessage` | **5** | 0 | **survives** | `PsPopupHost.RouteEvent` |
| 8 | `Codetip_FilterMessage` | 0 | 0 | **survives** | `PsTipHost.RouteEvent` |
| 9 | `handleCtrlSpaceAutoComplete` | 0 | **1** | dissolves | one `PsAccel` entry |
| 10 | `handleAltKeyMenuBar` | 0 | 0 | dissolves | `TryAltMnemonic` — already in the shell |
| 11 | `handleEscKeyModeless` | **1** | **1** | deferred | needs the real Find/Replace bars |
| 12 | `handleKeysFindReplace` | 0 | **2** | deferred | needs the real Find/Replace bars |
| 13 | `accel:main` | **4** | **5** | survives | `PsAccelTable` #1 |
| 14 | `accel:usertools` | 0 | 0 | survives | `PsAccelTable` #2 |
| 15 | `accel:buildconfig` | 0 | 0 | survives | `PsAccelTable` #3 |
| 16 | `isdlg:` float retarget | 0 | 0 | dissolves | per-surface routing on `ev.surface` |

**Eight of the sixteen never fired in either session** (1, 3, 4, 8, 10, 14, 15, 16). **That is a
fact about the sessions, not about the code**, and no row above is marked dead on that basis. A
context menu is a gesture nobody made; the User Tools and Build Config accelerator tables need a
bound tool and a configured project. Where a verdict rests on a zero, it rests on reading the
call site, and the row says which.

**Two rows are corroborated by the trace rather than by their own comment**, which is worth more
than the rest of the table: `frmExplorer` claimed exactly `VK_F2` and `PsListTree` claimed
exactly `VK_RETURN` — the two keys their comments say they exist to intercept ahead of the
accelerator chain and `IsDialogMessage`.

**The accelerator stack is nearly cold.** Nine claims across two sessions, all in table 1. Three
stacked `HACCEL`s and two of them never fired.

---

## The `IsDialogMessage` finding, which is the load-bearing one

Run 1 read **`IsDialogMessage claims 4115 of 5517`** — 75%. Taken at face value that is a very
large hole, because `IsDialogMessage` is the one mechanism in the chain with no PsPlatform
analogue at all.

**It is not a hole, and the capture gave itself away.** Its claim list contained `WM_PAINT`,
`WM_TIMER` and `WM_MOUSEMOVE`. Had those been swallowed the editor would not have repainted, and
it repainted throughout. `IsDialogMessage` **dispatches what it processes** — the API contract
forbids passing a handled message on to `TranslateMessage` or `DispatchMessage` — so a TRUE
return means *"I was the dispatcher"*, not *"I consumed it"*. Undivided, the number measures
message volume.

Run 2 splits it. The quantity that matters is the **keyboard** share, because dialog-key
semantics are the part with no analogue and everything else is dispatch `surf.Dispatch` already
does:

| pump | messages | `IsDialogMessage` **keyboard** | share | and the descriptors are |
| --- | --- | --- | --- | --- |
| `frmMain` | 7167 | **53** | **0.7%** | `'D'`, `'W'`, `'S'`, `'T'`, `WM_CHAR printable` — **ordinary typing**, dispatched |
| `frmOptions` | 1614 | **87** | 5.4% | `VK_TAB`, `Shift+VK_TAB`, `VK_UP/DOWN/RIGHT`, `VK_RETURN`, `WM_CHAR ctrl-9` — **traversal** |
| `frmAbout` | 127 | **7** | 5.5% | `VK_TAB`, `VK_RETURN` — traversal |

**`DispatchMessage:KEYBOARD` is ZERO in every pump.** Every keyboard message that reached the end
of a chain went through `IsDialogMessage`, which is why the undivided count looked alarming and
why the split was necessary to say anything at all.

**So the conclusion is a location, not a size: `IsDialogMessage`'s irreplaceable behaviour lives
in the DIALOGS, and not in `frmMain`.** In the main pump it is a dispatcher wearing a filter's
name — 0.7% keyboard, and that 0.7% is someone typing into Scintilla. In a dialog it is doing
real traversal work that something must reproduce.

That is the opposite of where the plan put the risk, and it is good news: `frmMain` becomes a
`PsSurface` pump needing no analogue, and the analogue that IS needed is scoped to one dialog
shape, which is exactly what this step's input box exists to build and prove.

### What the trace still cannot tell you

`IsDialogMessage` returns one boolean for *"I acted on it"* and *"I merely dispatched it"* alike,
so the keyboard counts above are an **upper bound** on real traversal, not a measurement of it.
`frmMain`'s 53 is visibly almost all typing. `frmOptions`' 87 is visibly almost all traversal.
Neither is separable from outside the API, and no instrument short of hooking the dialog manager
would do better.

---

## The dialog pumps, and why they are one problem and not twelve

All twelve are the same shape: two to four control filters, then `IsDialogMessage`, then
dispatch. `frmOptions.inc:1296` states the reason they exist at all:

> *"CWindow.DoEvents cannot be used: it has no `*_FilterMessage` hooks and this dialog hosts
> controls that require them."*

That requirement is a consequence of Win32 dispatching by `HWND`. A control that needs a key
before `IsDialogMessage` eats it must be offered every message in the app's loop, because nothing
else will offer it one. **PsPlatform dispatches by tree: the focused widget gets the key first by
construction, and there is no `IsDialogMessage` to beat.** There are ZERO `FilterMessage` sites
anywhere in PsPlatform — verified by grep across the tree — and that absence is the design, not
a gap.

**The trace supports this and does not prove it.** `frmOptions` took 1614 messages in run 2, with
heavy Tab/arrow/Enter use, and **not one of its three control filters fired** — nor did
`frmUserTools`' two across 323 messages. But the gestures that would fire them are specific
(opening a combo dropdown; editing a Localization row in place) and were not made. These rows are
**unexercised**, and the verdict rests on the call sites and on PsPlatform's design, not on the
zeros.

| pump | verdict | becomes |
| --- | --- | --- |
| the 12 dialog pumps | dissolve | one `PsModalHost.Run` each; their control filters have no analogue and need none |
| `PsColorPicker`, `PsMessageBox` | dissolve | `PsModalHost` — `PsMessageBox` is **already ported** in PsPlatform |
| `frmFindInProject` ×2 | dissolve | bounded drains ("let the UI breathe"), not interactive pumps |

---

## What this does to the estimate

**Seventeen pumps become one pump and a call.** One `PsSurface` loop; fourteen `PsModalHost.Run`
call sites; two drains. Sixteen claim points in `frmMain` become four host `RouteEvent` calls the
shell **already makes**, three `PsAccelTable`s walked in a loop, one precedence rule, and two
rows deferred with the Find bars.

**The 14–20 week estimate is not re-opened upward by this, and the specific risk it named is
reduced.** `d2-decision.md` and `7c-step1.md` both called the pump collapse the largest single
unknown. It is now a known quantity with one piece of real work in it:

> **the dialog traversal policy** — Tab and Shift+Tab, the default button, Escape as cancel, and
> arrow-walking a group. `PsSurface.FocusNext` exists; a default/cancel policy does not.

That work is **once**, not fourteen times, because all twelve dialogs want the same policy. It is
this step's commit 4.

**What this page does NOT re-measure is the other 45,000 lines.** The pump was one of several
named unknowns and it is the only one this census touches. Nothing here says anything about the
48 forms, and a reader taking "the pump collapses" as "7c got shorter" is reading a claim this
page does not make.
