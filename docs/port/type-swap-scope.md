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
| AfxNova/Win32 boundary â€” CTextStream, GDI+, `.vptr`, `GetTextExtentPoint32W` | ~59 | `.Wz()` at each site |
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

---

# Re-measured, 2026-08-06 (after 7c's app-layer work)

The numbers above were taken before the app layer was closed, the localization
tables became DWSTRING, `PsVal`/`PsStr`/`PsFile` landed and `PsCompat.bi` grew.
Re-run: replace `PsCompat.bi` with PsCore's three headers in `tiko.bas`, change
nothing else, build with `-maxerr 99999`.

    1029 errors reported
    1010 excluding the "Illegal inside functions" cascade

**Essentially unchanged.** Everything that landed since addressed other things.

| kind | count |
| --- | --- |
| Type mismatch | 362 |
| Invalid data types, before `:` | 206 |
| Invalid assignment/conversion | 153 |
| Invalid data types | 95 |
| the rest | ~194 |

Concentrated: `clsConfig.inc` 200, `modRoutines.inc` 87, `modKeyBindings.inc` 84,
`modCompileErrors.inc` 38.

## Two findings that change the plan

**1. It is 28 signatures, not ~266.** The table above counted ERROR SITES. The
functions behind them are 28 declarations taking a plain `wstring` parameter, 13
of them in `app/clsSymbolDb.bi`. Converting a signature fixes every caller at
once, so the surface is far smaller than the error count suggests.

But the 13 in `clsSymbolDb` cannot be converted yet — measured, not guessed —
because their bodies hand the parameter to helpers that walk fbcParser's
NUL-terminated pool by `wstring ptr`, and getting a buffer pointer out of a
DWSTRING is `.sptr` under AfxNova and `Wz()` under PsCore, with `Wz()`
Windows-only *by design*. And of the other 15, several are Win32-boundary
functions — `getTextWidth` feeds `GetTextExtentPoint32`, `GetFileToString` feeds
`CreateFileW` — so converting them today adds a `.sptr` at the boundary and buys
nothing until the swap lands. Attempted, measured, reverted.

**2. `.Utf8` EXISTS ON BOTH TYPES, so the biggest category is convertible
BEFORE the swap.** AfxNova's DWSTRING has `Utf8()` as a property returning
`STRING` via `WideCharToMultiByte(CP_UTF8, ...)`; PsCore's has the same shape.
So the ~270 `dim as string s = <DWSTRING>` and `select case <DWSTRING>` sites can
take `.Utf8` today, compile under AfxNova, and be verified against the oracle —
each batch shrinking what the swap has to do in one go.

**WITH ONE CAVEAT THAT IS NOT A DETAIL.** Today those sites convert implicitly,
and fbc's `wstring` -> `string` conversion goes through the **ANSI codepage**.
`.Utf8` is UTF-8. So the conversion is a real encoding change for any non-ASCII
text, in the direction the port is going, and it should be landed where the
encoding suites can see it rather than quietly across 270 sites at once.

## The shape this gives the swap

Not one commit, still — but for a better reason than before. The path is:

1. the `.Utf8` sites, in batches, oracle-verified (the caveat above)
2. the 15 non-clsSymbolDb signatures, minus the Win32-boundary ones
3. the swap itself, against whatever remains
4. `clsSymbolDb` and the Win32-boundary functions, which need `Wz()` and so can
   only move once PsCore's DWSTRING is the one in scope
