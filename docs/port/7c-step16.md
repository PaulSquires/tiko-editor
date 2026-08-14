# Phase 7c, step 16 — phase 7c's own code meets Linux

**`ALL GREEN [linux64]` — 48 suites, 0 build failures, 0 test failures**, on Fedora 42 with
GCC 15.

**"THE FIRST LINUX RUN" WOULD BE WRONG, AND I SAID IT SEVERAL TIMES BEFORE CHECKING.**
`docs/STATUS.md` and `README.md` in PsPlatform record native Fedora from the start: Gates 0–4
were closed there, the `hellotext` pixel digest `8CEED5802EF1431A` is pinned as matching on
Windows, WSL2 Ubuntu **and Fedora**, and Gate 4 was closed *"after two rounds on native Fedora"*
with an interactive pass at fractional scale that found two real bugs.

What had never run on Linux is **phase 7c's own additions** — and, specifically, everything
behind Scintilla. `STATUS.md`'s own table says it plainly: `Scintilla spike | 15/15 | 15/15 | not
run`. That is why all five defects below cluster where they do.

Five defects, and **not one of them was in the portable code**. Every one was in a binding, a
build script or a gate — and every one sits in or behind the Scintilla work, which is exactly the
part Fedora had never seen.

---

## What was already right

Worth saying first, because it is the part that could have gone badly and did not.

**`structsizes` passed on the first run.** `FT_FaceRec` = 248, `FT_GlyphSlotRec` = 304 on LP64.
The header's long warning about `long` being 4 bytes on LLP64 and 8 on LP64 is correct in every
field it describes.

**The render digest matched byte for byte.** `377B903CA1166763`, the same value Windows produces —
and `STATUS.md` had it as `pending` for Fedora, so this is the column filling in rather than a
first proof.

**`-z noexecstack` was already there**, with a comment saying it is *"a Fedora problem
specifically, not a cosmetic warning"* — written before anyone had a Fedora box.

---

## The five defects

### 1. The clone URL

`check-host.sh` told a new Linux host to `git clone .../platform.git`. The repository is
`PsPlatform.git`. **The single command on the "set up a new machine" path, and it 404s** — found
the first time anyone followed those instructions.

### 2. `FT_Long` is 8 bytes on Linux and the bindings said 4

The same `long` trap this file's header describes at length, **in the function signatures instead
of the structs**. It had been applied to every field and to no parameter.

FreeBASIC's `long` is 32 bits everywhere; C's is 32 on Windows and 64 on Linux, and `FT_Long` is a
typedef of it. So `byval face_index as long` is right on Windows and half the width the Linux ABI
expects.

**The symptom is why it took a Linux run to find.** Passing `0` works — a zero in the low half
reads as zero either way — so `TE_Init` and every suite that merely opens a font passed. Passing
`-1` does not: in 32 bits with an undefined upper half it is 4294967295, so the face-enumeration
probe failed for **every** name.

```
ok      a wrong name is refused          <-- PASSED, for the wrong reason
FAILED  the real family name IS accepted
FAILED    matched case-insensitively
```

**The pass is the interesting half.** *"Refuse a name that is not in the file"* is satisfied by
refusing everything, so half that pair was green on a platform where the feature did not work at
all.

`FT_Get_Char_Index`'s `charcode` is `FT_ULong` and had the same defect. It had been **working by
luck** — every codepoint is below 2³² and the SysV ABI zero-extends a 32-bit unsigned argument.

### 3. `<cstdint>` before the vendored Scintilla headers

```
ScintillaTypes.h:701:18: error: 'intptr_t' does not name a type
   using Position = intptr_t;
```

`ScintillaTypes.h` and `Geometry.h` use `intptr_t` without including `<cstdint>`. That compiles
wherever an earlier header drags it in transitively — every MinGW build this project has ever done
— and does not on a modern libstdc++ where those transitive includes were pruned.

**`ScintillaPs.cxx` already had the include, which is exactly why that file compiled and
`PlatPs.cxx` did not.** Same headers, same directory, one line apart in behaviour.

Fixed in our files, not the vendored header: patching vendor code is a merge conflict every
Scintilla update, and the include is correct in these files regardless.

### 4. `check` never built the library its own suites link

`build check` walked `tests/` and nothing else. The two suites that link `libpsscintilla` — `pssci`
and `pstec` — were built against whatever archive happened to be lying about.

**On Windows this is invisible**, because the library is always already there from ordinary work.
So the gate looked like it covered it.

On a clean Linux tree it is not there, and **the failure reads as a test defect rather than a
missing dependency**: `pssci` reported eleven undefined references to `SciPs_Tick`,
`SciPs_TickerRunning` and friends — symbols whose source file had compiled perfectly well.

> **THAT COST TWO ROUNDS.** The evidence was in the first log: symbols from a file that compiled
> cannot be undefined unless the archive was never made. I read past it twice and went looking at
> the C ABI instead.

### And a fifth, found by the author: `libstdc++-static`

```
/usr/bin/ld: cannot find -lstdc++
/usr/bin/ld: have you installed the static version of the stdc++ library ?
```

Fedora ships `libstdc++.a` in a separate package and does not pull it in with `gcc-c++`. Debian's
`build-essential` does, which is why every earlier check — all on Ubuntu — passed.

**`check-host.sh` had already said "Host is ready".** And it is needed only at the *very last
link*, after SDL3, FreeType, HarfBuzz and Blend2D have all been built from source — so the cost of
that script being wrong was a full dependency build before anything mentioned it.

Now checked with `g++ -print-file-name=libstdc++.a`, which echoes the name back unchanged when it
cannot find the file.

---

## Verification

| | |
| --- | --- |
| Fedora 42, GCC 15, `bash build.sh check` | **48 suites, 0 build failures, 0 test failures** |
| `structsizes` (LP64) | `FT_FaceRec` 248, `FT_GlyphSlotRec` 304 |
| render digest | `377B903CA1166763` — **identical to Windows** |
| Windows `build.cmd check`, after every fix | **48 suites, 0 failures** |

**NOT VERIFIED, and this is the important half:** *nothing has been run on Linux with a window
open.* `build.sh check` is headless. SDL3 built both the X11 and Wayland backends, and neither has
been asked to display anything. The fontconfig path in `PsFont.inc` still has never executed —
`psfont` is Windows-gated and prints NOT VERIFIED on Linux.

---

## What this step is really about

Fifteen steps of this port have found the same shape over and over: **a claim nothing tested.** A
comment, a handoff entry, a suite nobody ran, a reason nobody checked.

**AND I COMMITTED IT MYSELF IN THIS STEP.** I called this "the first Linux run" repeatedly, in
chat and in a commit message, without opening `STATUS.md` — which says on its second screen that
Gates 0–4 were closed on native Fedora and pins a digest as matching there. The handoff entry I
inherited said "nothing in phase 7c", which is true and narrower; I widened it while repeating it.

The accurate version is more interesting anyway. Fedora had seen the toolkit and had never seen
**Scintilla** — `STATUS.md` says `not run` in that row — and every one of the five defects lives
in or behind that work. The `FT_Long` binding was wrong the day it was typed, but the path that
exposes it (`face_index = -1`) only arrived in step 12. The clone URL has never worked.

**And the portable code was fine.** Every defect was in a binding, a build script or a gate — in
the machinery around the port rather than the port. That is the genuinely good news in this step,
and it is only knowable because something finally ran.

---

## What is left

1. **Linux with a window.** Nothing has been displayed. X11 and Wayland are both built and neither
   has been exercised; `demos/` exist and none has been run.
2. **`tikoshell` on Linux.** The portable binary lives in the *tiko* repo and needs both trees
   checked out side by side. It has never been built for Linux at all.
3. **fontconfig.** Still never executed. It is the one part of step 12 that has no evidence behind
   it whatsoever.
4. **Font fallback's remaining gaps** — per-face metric normalisation, non-ASCII family names, the
   Linux `.ttc` face index. Recorded in [`7c-step12.md`](7c-step12.md).
