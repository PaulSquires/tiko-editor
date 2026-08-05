# `src/app/` — the layer that does not know it is on Windows

28 files, 4,933 lines. What Phase 8 carries to Linux unchanged: the symbol
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
| `modEncoding.inc` | `MessageBoxW` and an `as hwnd` local — it asks the user about encoding conflicts |
| `modThemeTypes.bi` | `#include "AfxNova/DWSTRING.bi"` |

`modThemeTypes.bi` is the interesting one: its *only* dependency is the DWSTRING
include. It becomes portable for free when the type swap lands, and is the
natural first file to re-test then.

`modEncoding.inc` is 476 lines of genuinely portable conversion with three user
prompts in it. Splitting the prompts out is a small, worthwhile job and is not
this commit.

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
