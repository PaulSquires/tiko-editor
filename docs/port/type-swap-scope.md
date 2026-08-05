# The DWSTRING type swap: measured, and why it is not one commit

Phase 7a converted 1097 call sites to the Ps* family. The last step -- swapping
`DWSTRING` itself from AfxNova's to PsCore's -- was attempted, measured, and
backed out. This records what it costs, so the decision is made on numbers.

## The experiment

Replace `#include "PsCompat.bi"` with PsCore's three headers in `tiko.bas`, and
build. Nothing else changed.

    1459 errors      first attempt
    1338 errors      after adding DWSTRING's binary `&` to PsCore
     844 errors      excluding the "Illegal inside functions" cascade, which is
                     fbc losing its place after an earlier error rather than
                     844 separate problems

## What those 844 actually are

| kind | count | needs |
| --- | --- | --- |
| `dim as string s = <DWSTRING>`, `select case <DWSTRING>` | ~270 | `.Utf8` at each site |
| tiko's own functions taking `byref as wstring`, handed a DWSTRING | ~266 | those APIs to take DWSTRING |
| `val(<DWSTRING>)` | 44 | a `PsVal` in PsCore |
| AfxNova/Win32 boundary — CTextStream, GDI+, `.vptr`, `GetTextExtentPoint32W` | ~59 | `.Wz()` at each site |
| intrinsics on an EXPRESSION rather than a plain identifier | 22 | the same conversion the 557-site pass did, with a smarter match |
| miscellaneous | rest | judgement |

## The finding

**The first two rows are the shell conversion, not a string swap.**

AfxNova's DWSTRING extends `wstring`, so it converts implicitly to fbc's
`string` and binds to every `byref as wstring` parameter. PsCore's does neither
-- deliberately, and measured: Phase 1.4 rejected that cast at 66x to 2663x.

tiko's *internal* APIs are written in terms of `wstring` at several hundred
sites. Those signatures have to become DWSTRING for the swap to land, and
changing a function's parameter type changes every one of its callers. That is
Phase 7c work -- the shell conversion -- arriving early and disguised as a
string-library change.

## What was kept

Everything except the swap itself. The 1097 conversions are real, are committed,
and are verified against the 27 stable self-test suites. `PsCompat.bi` still
forwards them to AfxNova, so tiko builds and behaves exactly as it did.

That is the point of having built it as scaffolding: the branch holds a
completed, verified two-thirds of 7a rather than a broken attempt at all of it.

## What PsCore gained on the way, and kept

Found by trying, not by review, and all of it verified on both platforms:

* four name collisions with `windows.bi` (`PS_DOT`, `WIN32_FIND_DATAW`, six
  `FindFirstFileW`-family declarations, and `OpenFile` -- Win32 has one)
* `DWSTRING.Wz()`, the zero-copy Win32 boundary
* binary `&`, minus the two literal-on-the-left overloads that broke `"?" & k`
  in six unrelated suites
* `PsPathDirWithSep`, `PsPathIsRelative`

## Recommended order from here

1. `PsVal` in PsCore (44 sites, mechanical).
2. The 22 expression-form intrinsics (mechanical).
3. Then **7b** -- move the portable core into `src/app/` as the plan sequences
   it -- and swap DWSTRING inside that layer, where the `wstring` APIs are
   fewest.
4. The shell's `wstring` signatures move with 7c, where they belong.
