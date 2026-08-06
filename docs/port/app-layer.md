# `src/app/` — the layer that does not know it is on Windows

26 files, ~4,100 lines. What Phase 8 carries to Linux unchanged: the symbol
database, encoding conversion, the ini parser, project folders, the fuzzy
matcher, the build service, navigation history, unused-symbol analysis, the
menu and theme *definitions*, and the debugParser/fbcParser integration.

Enforced by `_check_app_layer.bat`, which fails on any Win32 or AfxNova
identifier. Run it before pushing.

## How the list was chosen

By measurement, not opinion: every `.bas`/`.bi`/`.inc` in `src/` was scanned for
the Win32 and AfxNova vocabulary, and the ones with zero hits moved.

**Then the ratchet rejected three of them**, which is the useful part. The
initial scan matched whole identifiers; the ratchet reads code with comments
stripped, and caught what a token-level regex could not:

| file | why it stayed behind |
| --- | --- |
| `clsTopTabCtl.inc` | `AfxRedrawWindow`, `IsWindowVisible`, `HWND_FRMMAIN_FIND` — the last is one token, so `\bHWND\b` never matched it |
| `modEncoding.inc` | `MessageBoxW`, an `as hwnd` local, and — found later — `MultiByteToWideChar`, `WideCharToMultiByte`, `FormatMessageW`, `CreateFileW`, `GetLastError` |
| `modThemeTypes.bi` | `#include "AfxNova/DWSTRING.bi"` |

`modThemeTypes.bi` is the interesting one: its *only* dependency is the DWSTRING
include. It becomes portable for free when the type swap lands, and is the
natural first file to re-test then.

### The prompts came out of `modEncoding.inc`

`modEncodingUi.bi`/`.inc` now own the three questions the encoding code asks:
`Doc_ConfirmLossySave`, `Doc_ReportWriteFailure`, and `Doc_ConfirmAnsiConversion`
— the last extracted from inside `ConvertTextBuffer`, where a single function
both decided *whether* to ask and owned *what asking looked like*.

The dependency now runs one way: logic names a question, UI answers it. The
`.bi` is included before `modEncoding.inc` precisely because the logic calls
into it, which makes the direction visible in the include order.

**This did NOT make `modEncoding.inc` portable, and it was never going to.**
What remains calls `MultiByteToWideChar`, `WideCharToMultiByte`,
`FormatMessageW`, `CreateFileW` and `GetLastError`. Those need `PsEncoding` and
`PsFile` rather than a move — rewriting the save path is a bigger and riskier
job than separating the dialogs from it, and this makes that job possible to do
on its own.

### The ratchet's vocabulary was incomplete, and two more files came back

The first version of the banned list was a hand-written sample of the Win32
surface, and it behaved like one: it passed 28 files, two of which call
`MultiByteToWideChar`, `WideCharToMultiByte` or `SciExec`.

    modEncodingSelfTest.inc   tests the Win32-based conversions directly
    modUnusedSymbols.inc      SciExec — tiko's own Scintilla wrapper, but it
                              takes an HWND, which is the same dependency
                              under another name

Both moved back; the list is wider now. It is still a sample. Widen it whenever
something slips through, and read a green run as evidence rather than proof.

## Why the checker is a program

It was a `findstr` loop first, and reported three violations that were not:

    ghMenuBar            matched HMENU
    FlushPendingEdit     matched HPEN
    "wParam carries..."  matched WPARAM, in a comment

Substring matching cannot tell a token from a fragment, and a checker that cries
wolf gets switched off. `src/_check_app_layer.bas` matches whole words and
strips comments, so every line it prints is a real dependency.

It also **fails when it scans zero files**. The first version resolved its
directory with a trailing separator — which fbc's `Dir(..., fbDirectory)` will
not match — found nothing, and reported a clean run. That would have passed CI
forever. Silence is not success.

## What is NOT in here yet

The big portable subsystems — the document model, undo, search, settings, the
editor core — are still in `src/` because they carry `HWND` in their signatures
rather than in their logic. Moving them is the same work as 7c and belongs with
it.

## Interactive verification

The save path was exercised by hand on 2026-08-06, after the writer moved to
`PsFileWriteAll`/`PsFileReplace` and the encoder to `PsC.PsEncDecode`/
`PsC.PsEncEncode`. Saving works.

That closes the caveat those two commits carried. It matters more than the
suite numbers do here: `TIKO_SAVE_SELFTEST` drives `Doc_WriteToDisk` directly,
so it exercises the `ReplaceFileW` branch but not the `MoveFileExW` fallback
that every Save As and every new file takes — the two branches are chosen by
whether the destination already exists, and only a real Save As picks the
second.
