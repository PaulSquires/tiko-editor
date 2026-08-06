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

But the 13 in `clsSymbolDb` cannot be converted yet � measured, not guessed �
because their bodies hand the parameter to helpers that walk fbcParser's
NUL-terminated pool by `wstring ptr`, and getting a buffer pointer out of a
DWSTRING is `.sptr` under AfxNova and `Wz()` under PsCore, with `Wz()`
Windows-only *by design*. And of the other 15, several are Win32-boundary
functions � `getTextWidth` feeds `GetTextExtentPoint32`, `GetFileToString` feeds
`CreateFileW` � so converting them today adds a `.sptr` at the boundary and buys
nothing until the swap lands. Attempted, measured, reverted.

**2. `.Utf8` EXISTS ON BOTH TYPES, so the biggest category is convertible
BEFORE the swap.** AfxNova's DWSTRING has `Utf8()` as a property returning
`STRING` via `WideCharToMultiByte(CP_UTF8, ...)`; PsCore's has the same shape.
So the ~270 `dim as string s = <DWSTRING>` and `select case <DWSTRING>` sites can
take `.Utf8` today, compile under AfxNova, and be verified against the oracle �
each batch shrinking what the swap has to do in one go.

**WITH ONE CAVEAT THAT IS NOT A DETAIL.** Today those sites convert implicitly,
and fbc's `wstring` -> `string` conversion goes through the **ANSI codepage**.
`.Utf8` is UTF-8. So the conversion is a real encoding change for any non-ASCII
text, in the direction the port is going, and it should be landed where the
encoding suites can see it rather than quietly across 270 sites at once.

## The shape this gives the swap

Not one commit, still � but for a better reason than before. The path is:

1. the `.Utf8` sites, in batches, oracle-verified (the caveat above)
2. the 15 non-clsSymbolDb signatures, minus the Win32-boundary ones
3. the swap itself, against whatever remains
4. `clsSymbolDb` and the Win32-boundary functions, which need `Wz()` and so can
   only move once PsCore's DWSTRING is the one in scope

---

# The swap attempted again, 2026-08-06. 711 -> 543, then reverted.

Three mechanical passes, applied to the swapped tree and measured after each:

| pass | sites | errors after |
| --- | --- | --- |
| start | | 711 |
| `.sptr` / `.vptr` -> `.Wz()` | 73 | 637 |
| `val()` -> `PsVal()` | 18 | 620 |
| `byref x as wstring` -> `byval x as DWSTRING` in tiko's OWN signatures | **137 across 23 files** | **543** |

**The signature pass is the biggest single lever there is** � 137 parameters removed
168 errors, because one signature fixes every caller at once. It is also the pass
that CANNOT be landed ahead of the swap: the bodies behind those parameters hand
them to Win32, and a DWSTRING buffer pointer is `.sptr` under AfxNova and `Wz()`
under PsCore. Same circle as everywhere else.

## What 543 is made of

* **~200 Win32/CRT boundary**, needing `.Wz()` on a specific argument:
  `CreateFileW`, `SetWindowTextW`, `SHCreateItemFromParsingName`, `chdir`,
  `FindFirstFile`, `WritePrivateProfileString`, `CTextStream.Create` in the two
  AfxNova files left, and the IFileOpenDialog / IFileSaveDialog vtable calls.
  Mechanical per site, but the *argument* differs each time.
* **~148 `Invalid assignment/conversion`** � the `.Utf8` class. Many are assertion
  text in self-tests and safe; some are data (`keys(i) = PsStrParse(...)` into a
  `string` array) and are an encoding decision each.
* **~72 `Invalid data types`**, plus a tail of small classes.

## Why it was reverted rather than committed part-way

A partly-swapped tree does not build, so there is nothing to verify against the
oracle and nothing safe to leave on the branch. That is the same reason the first
attempt was backed out, and it has not changed.

**What HAS changed is the size and the shape.** 1010 -> 711 was earned by work
that stands on its own and is committed; 711 -> 543 is three passes that take
minutes to re-apply from this table. The remainder is a few hundred individual
judgements, and it wants a dedicated run rather than the tail of a long one.

## The boundary pass, added the same day: 543 -> 512, converged

A fourth pass, scripted from fbc's own `at parameter N of FUNC()` text: find the
call, split its arguments, append `.Wz()` to the Nth. Multi-line aware � it joins
continuation lines until the parentheses balance and puts them back, comments and
all. Run to a fixed point:

    543 -> 526 -> 516 -> 514 -> 512 -> 512

**~70 sites, 31 errors, and then it stops.** That number corrects an estimate
made earlier on this page: "roughly 200 Win32/CRT boundary sites". That counted
every `error 58`, and most of those are NOT the Win32 boundary at all � they are
tiko functions taking `string`, and intrinsics.

### Why the rest cannot be scripted, which is the useful part

* **fbc names the RESOLVED callee, not the identifier written.**
  `AfxSetWindowText(h, wszText)` is reported as `parameter 2 of SETWINDOWTEXTW()`.
  Nothing on that line says `SetWindowTextW`, so name-matching cannot find it,
  and this is the normal case for every wrapper.
* **Statement forms have no parentheses.** `chdir wszDir` and
  `open <path> for output as #f` are reported like calls and are not.
* **43 distinct callees**, each with the argument in a different position.

## The position after this run

    1010  the swap, before any of this
     711  after the committed work -- select case, localization, CTextStream,
          the menu vocabulary, the app layer
     512  after four scripted passes that take minutes to re-apply

The 512 are: 168 `Type mismatch`, 148 `Invalid assignment/conversion` (the
`.Utf8` class, each an encoding decision), 73 `Invalid data types`, and a tail.
They are individual judgements, not a category with a recipe.

---

# The `.Utf8` sites, landed ahead of the swap. 153 -> 52.

Step 1 of the plan above, done and committed on the unswapped tree, so every batch
is oracle-verifiable. **All 27 suites are byte-identical to the pre-change capture,
end to end.**

## How the sites were found

The class cannot be found by grep -- it is "a DWSTRING flowing into an fbc `string`",
which is a type question, not a text one. So: swap the tree, build with
`-maxerr 99999`, keep the `error 181` lines, revert, and work the list on the tree
that still builds.

    #include once "PsCompat.bi"
      ->  core/DWString.inc, core/PsStr.inc, core/PsPath.inc, core/PsFile.inc,
          core/PsEncoding.inc

**That probe is not the same one the numbers above were taken with** -- it names five
headers rather than three, so its totals (718 before this work) are not comparable to
the 711/512 on this page. What IS comparable is the same probe run before and after:

    error 181   153  ->  52
    total       718  ->  617

## The finding: a THIRD of that class is not the `.Utf8` class at all

Of the 153, **101 had an fbc `string` on the receiving end** and took `.Utf8`. The
other 52 are `error 181` for a different reason and must not take it:

| the real destination | example | what it actually needs |
| --- | --- | --- |
| `wstring * MAX_PATH` | `frmMain`'s `wszText`, `modMRU`'s `wzFile` | nothing -- the swap |
| an LPWSTR struct field | `.lpFile = wszCommand` (SHELLEXECUTEINFO) | `.Wz()` |
| AfxNova's `BSTRING` | `dim as BSTRING wst` in `modRoutines`, `frmOptionsLocal` | deleted with AfxNova |
| an AfxNova-returning wrapper | `PsTextBox.inc`'s `return RichEdit_GetText(...)` | the swap |
| a const-qualified parameter | `PsUCase( wszName )` in `clsSymbolDb` | a signature change |

Reading "148 `Invalid assignment/conversion`" as "148 `.Utf8` sites" would have put
UTF-8 bytes into a `wstring * MAX_PATH` at about fifty sites. **The error code names
the symptom, not the class.**

## Three shapes the 101 took

**1. The assertion macros -- 55 of the 101, and ONE edit each.** Every self-test in
the tree spells its failure the same way:

    sMsg = "  FAIL: " & msg          ' msg is the CALLER's expression, DWSTRING-valued

fbc reports that at the *call* line, so the list showed 55 sites in 11 files. They are
11 macro bodies:

    dim as DWSTRING wszMsg = msg
    sMsg = "  FAIL: " & wszMsg.Utf8

`INIASSERT`, `CDASSERT`, `SAVEASSERT`, `CMDASSERT`, `ENCASSERT`, `OPTASSERT`,
`ENCROWCHK`, `THASSERT`, `UTASSERT`, `BCEQ`, `LAYASSERT`/`BCASSERT`. Assertion text,
so the ANSI -> UTF-8 change is the safe one this page already called safe.

**2. Mechanical `.Utf8` on internal text -- 46.** `modCodetips` (15), `clsDocument` (9),
`clsConfig` (5), `modThemes` (5), `modFormat` (4), `modFindReplace` (4),
`frmOptionsKeywords` (3), and one each in `modUpdateCheck` and `frmMainProject`.
FreeBASIC source tokens, calltips, keyword lists and build GUIDs -- ASCII in practice,
and UTF-8 is the direction the port is going.

**3. Two sites where `.Utf8` would have been the BUG, and the type widened instead.**

* `frmThemesPage`'s `PAINTROLE` held a role name in a `string` and then PAINTED it.
  UTF-8 bytes down an ANSI drawing path is mojibake. The local is a `DWSTRING` now,
  with `PsLeft`/`PsMid`/`PsLen` in place of the intrinsics.
* `GetThemeDescription` returned `string` -- a theme description round-tripped through
  the ANSI codepage -- and **every one of its five callers assigns it straight back
  into a DWSTRING.** It returns DWSTRING now.

## What is deliberately NOT done

`CompleteIncludeFilename` (`modRoutines:612`) is the worked example this page already
gives: a file path returned to an ANSI caller, where `.Utf8` is a bug. Fixing it means
`string` -> `DWSTRING` through its signature, its two callers, and `clsDocument.GetLine`
behind them. That is the signature pass, and it belongs with the swap.

---

# The PURE signatures, landed too. 617 -> 549.

Step 2 of the plan on this page, and the correction it needs: the earlier note says the
signature pass "CANNOT be landed ahead of the swap". **That is true only of the ones whose
bodies reach Win32.** Fourteen do not, and they went in on the same terms as everything
above -- gas64 clean, all 27 suites byte-identical, `_check_app_layer` and
`_check_scihost` green.

    617  after the .Utf8 work
    561  after 11 signatures
    549  after OpenSelectedDocument and frmMain_OpenProjectSafely

**14 signatures, 68 errors.** One declaration fixes every caller, which is why this is
still the best lever on the page.

| converted | was | errors |
| --- | --- | --- |
| `OpenSelectedDocument` | `byref wszFilename/wszFunctionName as wstring` | 11 |
| `GetDocumentPtrByFilename` | `byref as wstring` | 7 |
| `Workspace_Check` | `byref as wstring` x2 | 11 |
| `CLRTest_Check`, `Nav_Check` | `byval as string` | 9 |
| `SetCompileStatusBarMessage` | `byref as wstring` | 5 |
| `UnusedSymbols_CmpText` | `byref as const wstring` x2 | 7 |
| `frmOptionsLocal_LoadLocalizationFile`, `LoadDiskFile` | `byref as wstring` | 6 |
| `frmMain_RestoreWorkspace`, `frmMain_OpenProjectSafely` | `byref as const wstring` | 4 |
| `BuildRequest`, `ConvertTikoVersion` | `byref as wstring` | 3 |

## The one that was not mechanical

`OpenSelectedDocument`'s first parameter **was an out-parameter by accident.** Its FindProc
branch does `wszFilename = *pFile`, and a `byref wstring` carried that back out. Twelve of
the nineteen call sites pass `pDoc->DiskFilename` -- a document's own name, and its tab
caption.

It never fired there: the branch needs a function name AND `nLineNumber = -1`, exactly one
call site passes a function name, and it passes a real line number. So `byval` is safe, and
this is written down because the next reader of that branch deserves to know it was checked
rather than assumed.

## What is left, and why it is blocked

The remaining `error 58` parameters are the boundary, not the vocabulary:

* **`CWindow.Create`'s `wszTitle` -- 15 sites, the single biggest.** AfxNova's own
  signature. It needs `.Wz()`, which does not exist on AfxNova's DWSTRING.
* `GetTextWidth`, `CreateFont`, `SaveSelfTest_ReadAll` -- `GetTextExtentPoint32`,
  `CreateFontW`, `CreateFileW`. Same circle: `.sptr` today, `Wz()` after.
* the `clsSymbolDb` family (`FindProc`, `FindType`, `EnumPrefix`, `SymDb_FileNameEq`, ...)
  -- fbcParser's `wstring ptr` pool, exactly as measured earlier on this page.
* `Theme_SanitizeText` and `FormatCodetip` take `string` and INDEX IT BY BYTE. Widening
  them changes what an index means, so they are a judgement for the swap, not a signature
  swap.
