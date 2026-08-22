# Phase 7c, step 30 — the four Selection rules come back from tiko

Four defects were reported against **tiko's** Selection icon over three rounds. The shell is the
*port* of that bar, and it had two of the four. They come back here, asserted, rather than being
rediscovered in a year when this binary is the one people use.

**This step is not a port of code. It is a port of four things that were learned by hand.**

---

## What came back

**One capture rule, in one place.** `ShellFind_CaptureSelection` said
`if isInitialized then exit sub` — the exact guard tiko had to have loosened. Opening the bar over
nothing fills the capture with an **empty range**, and selecting lines afterwards can never get past
it: the icon reads the empty range and refuses. It now refreshes when the **live** selection spans
lines, and only then — after the incremental search the live selection is the **last match**, one
line, and taking that would shrink "within these three lines" to "within this match".

The Selection arm had carried **its own copy** of that rule since step 29, and now just calls the
function. Two expressions of one rule is how they drift; the drift here was three rounds long.

**Disarming drops the text selection**, by request against tiko — and **only on the on→off
transition**, because a *refusal* lands in the same branch and collapsing a selection the user just
made because the button declined to arm is the opposite of the ask.

---

## Pinning a condition, not a branch

`bWasOn` is reverted **in both directions**: to `false` (never collapse) and to `true` (always
collapse). Each fails exactly one assertion — the collapse and the refusal respectively.

One revert would have proved only that the branch runs sometimes. **Two prove the condition is the
right one**, and this is the first place in the run where that distinction was worth the extra
assertion: every earlier revert flipped something that was either present or absent, where here the
wrong answer is a guard that is always true.

---

## Verification

| | |
| --- | --- |
| `_compile_fast` | exit 0, zero warnings — tiko did not move |
| `_compile_shell` | exit 0, one warning — the pre-existing step-22 warning 38 |
| `_run_shell --selftest` | **566 → 571** |
| the five `_check_*` gates | green |
| `_check_selftests` | 35 suites, 20,466, 0 failed |
| layout oracle | unchanged |

**Revert-to-red, four mutations, all caught:** capture early-return 568/3, capture always 567/4,
no collapse 570/1, collapse on refusal 570/1.

**Every one of the five new assertions drives the GESTURE** — open, select, click — because that is
what four rounds of bug reports established: a suite that sets `gFind` by hand cannot see any of
this. The two that tiko needed a real window for are asserted here headlessly, because the shell's
Selection path has no `SCEN_*` in it at all.

**NOT VERIFIED BY ME:** the shell's bar on screen. tiko's equivalents are confirmed by the author.

---

## What is left

1. **Tooltips on both bars**, and the fold icon (Find in Project only).
2. **Run step 18's Linux scripts** — three `.sh` files, unrun thirteen steps later.
3. **fontconfig** — still never executed anywhere.
4. **The `SCEN_FOCUS` pair** in PsPlatform, if the general selection restore is ever wanted.
