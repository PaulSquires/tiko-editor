# Handoff — F1Markdown

tiko `feat/f1markdown` @ **`12d38a382`**, pushed to `origin/feat/f1markdown`.
PsPlatform **`main`** @ **`b4d7371`**. Builds warning-free on gas64; 184 self-test assertions
green; runs against a real corpus.

**Every number on this page was re-verified against a fresh run on 2026-08-10**, with the
command beside it. Re-run them before quoting them — the sister page in `docs/port/` records
three of its own figures rotting inside a single day, and nothing about this one is different.

## The one-paragraph version

F1Markdown is a native markdown help viewer: a folder of `.md` files as a contents tree on the
left, a rendered page on the right, a search box, and a `--topic` switch so tiko's F1 lands on
the right page. It exists because tiko's F1 currently leaves the application — `frmHelpCenter`
builds a URL and `ShellExecute`s a browser, since the WebView2 pane was deliberately removed
([`../port/webview2-decision.md`](../port/webview2-decision.md)). It is a **third translation
unit inside tiko**, built from `src/F1Markdown/F1Markdown.bas`, on PsPlatform rather than
Win32+AfxNova so the Linux port is a recompile. Phases 1–5 are done. **The only thing missing
is a corpus**: the author has not written one yet, and `f1markdown.ini` is the seam where it
plugs in.

## Where it is, and how to build and run it

| | |
| --- | --- |
| Source | `src/F1Markdown/` — 19 files, 6,982 lines (`wc -l src/F1Markdown/*`) |
| Binary | `F1Markdown.exe`, beside `tiko.exe`, tracked in git |
| Build | `_compile_f1markdown.bat` — gas64, like the other two |
| Config | `f1markdown.ini` beside the exe. **Gitignored**, generated with a commented starter on first run |
| Settings | `%APPDATA%\F1Markdown\settings.ini` — per-user, safe to delete |
| Cache | `%LOCALAPPDATA%\F1Markdown\*.f1x` — per-root index cache, safe to delete |
| Fonts | `assets/f1markdown/fonts/` — six **static** faces, tracked |

**THERE ARE THREE BINARIES IN tiko NOW**, not two: `tiko.exe`, `_shell\tikoshell.exe`, and
`F1Markdown.exe`. None builds the others. F1Markdown needs no `PATH` juggling because it sits
in the project root where the render DLLs already are — `SDL3.dll` was added there for it.

### The modes, and the two that need no window

```
F1Markdown.exe                       open the window
  --topic <text>                     the symbol F1 was pressed on
  --root <path>                      a one-off content root, replacing the configured list
  --docset <name>                    load only one of the configured roots
  --open <path>                      render one file, in or out of the index
  --theme <file>                     override tiko's theme for this run
  --rescan                           ignore the index cache once and rebuild it
  --selftest                         184 assertions, windowless, non-zero on failure
  --scan <path>                      walk a root: tree, counts, timings, and the ranked
                                     answer to --topic. NO window, NO font, NO SDL.
  --dump-md <path>                   one file's block model, one line per block, plus every
                                     line the parser could not classify. Same: no platform.
```

`--scan` and `--dump-md` run **before `PsPlatformInit`**, and that is deliberate: it is the
assertion that the document model and the index are pure data. If either ever needs the
platform, those two modes stop working and say so immediately.

## If you are picking this up cold

1. [`Learnings.md`](../../../Learnings.md) — the workspace's run-derived traps. Still the most
   useful page in the tree.
2. **"The landmines"** below. Every one cost real time and none is guessable from the code.
3. The `.bi` headers, in this order: `mdParse.bi`, `mdLayout.bi`, `MarkdownView.bi`. They carry
   the reasoning; the `.inc` files carry the code.
4. Run `F1Markdown.exe --scan C:\dev\HelpCenter\cache\AfxNova\docs --topic AfxStrParseCount`.
   That one command exercises the walker, the parser, the index, the ranking and the cache, and
   prints all of it, in about two seconds.

## The architecture, and the one rule

**Three layers, and the layering is what makes the Linux port a recompile.**

| Layer | Files | May touch |
| --- | --- | --- |
| Pure data | `mdParse`, `mdHilite`, `mdIndex`, `mdCache`, `mdConfig`, `mdMailbox` | filesystem, strings. **No platform at all** |
| Paint | `mdFonts`, `mdLayout` | `PsTextEngine`, `PsImage` — PsPlatform's paint layer, which exists on Linux |
| UI | `MarkdownView`, `F1Markdown.bas` | widgets, events, **and the only place that knows what a colour is** |

The page model carries **kinds**, never colours: `MDRK_LINK`, `MDD_CODEBG`, `MDT_KEYWORD`. The
view resolves them against the theme in `OnThemeChanged`. That is not tidiness — `PsThemeApply`
walks the *widget* tree and can never reach a page model, so a view that did not refill its own
table by hand would give you a perfectly themed shell with an unthemed slab in the middle.

### The parser is a port, not a design

`mdParse` is a line-for-line port of `C:\dev\HelpCenter\tools\helpgen\mdlite.py`, whose subset
was **measured** across the AfxNova corpus rather than guessed. Every matcher carries the regex
it implements in the comment above it, and **that is the only way to review the file**. The
block order is copied exactly. The consequence worth knowing: this viewer and the web Help
Center render one corpus by one set of rules, so a divergence between them is a bug in one of
them rather than a matter of taste.

Four deliberate differences are listed in `mdParse.bi`. The one that bites: **blockquotes are a
depth field**, not a nested document, because a flat arena has nowhere to put a sub-document.

## What is verified, and how

| Claim | Command | Result |
| --- | --- | --- |
| Assertions | `F1Markdown.exe --selftest` | 184 passed, 0 failed |
| Parser vs the corpus | `--dump-md` over 205 files | **0 unclassified lines** |
| AfxNova scan | `--scan …\AfxNova\docs` | 137 docs, 14,607 headings, 184 ms |
| …from cache | same | 18 ms |
| Three docsets, warm | run and read line 1 | 205 topics, 16,204 headings, **43 ms** |
| Ranking | `--scan … --topic AfxStrParseCount` | 8000, opens String Procedures |

**The self-test asserts geometry at 1.0 AND 1.75, and absolute rects rather than relations.**
`tikoshell.bas` records 21 green assertions coexisting with a visibly wrong window because every
one of them asserted a relation, and all those relations hold perfectly at the wrong scale.

**The font suite asserts the variable-font trap directly**: bold must *measure* differently from
regular, and the mono face must measure `iiii` and `MMMM` the same. A variable font would pass
every other test in this repository.

### What is NOT verified

- **Anything on Linux.** No Linux build has been attempted. The layering is designed for it; it
  is not evidence of it.
- **Tooltips.** Wired, and they do not appear — see below.
- **Long sessions.** The image cache is bounded at 64 per page and the nav stack at 128; neither
  bound has been reached in anger.
- **A corpus that does not exist yet.** Everything above was measured against AfxNova's docs,
  the PsControls READMEs and tiko's own `docs/` — none of which is the help content this viewer
  is ultimately for.

## The landmines

Every one of these cost time, and none is visible in the code that suffers from it.

**1. `PsDispatch` already localises mouse coordinates.** Every delivery gets its own copy of the
event with the coordinates translated into the receiving widget's space
(`PsDispatch.inc:26-30`). `MarkdownView` called `FromSurface` on top of that and translated a
second time, so every hit test landed one widget-origin away — about 400 px at 1.75. **Link
hover, link clicks and the copy button were all silently dead** through two phases. Test
`ev->mouse` against `LocalRect()` directly, as `PsButton` does.

**This is the second time this exact bug has been found in this toolkit.** PsPlatform's own head
commit is `b4d7371`, *"PsSciView: the mouse origin was subtracted twice"*. If you write a widget
that hit-tests, assume you have it until you have driven the pointer over it and looked.

**2. A byref argument into an array the callee can grow is a dangling reference.** `ScanDir`
passed `ix.topic(nF).sPath` byref into its own recursion; the first `AddTopic` inside redims that
arena, moves the storage, and the reference dangles. Access violation far from the cause. Copy to
a local first.

**3. `sIn` IS `SIN`.** fbc identifiers are case-insensitive and that one is a builtin. `IsWs` and
`TrimWs` also already exist at global scope in PsPlatform. Everything here is `Md`-prefixed for
that reason.

**4. Warning 38 fires on `boolean = false` as readily as on a bare boolean.** A chain mixing one
with integer comparisons needs `cbool()` or a hoisted local. Chasing this is not optional — the
house bar is a warning-free build.

**5. `PsEncDecodeAuto` returns UTF-16.** The obvious `PsEncDecodeAuto(bytes).Utf8` transcodes an
already-UTF-8 file twice to arrive back at the bytes it read. `MdReadUtf8` detects first; that
alone took the corpus scan from 293 ms to 188 ms.

**6. gas64 leaves an `.asm` beside every file it compiles.** One got committed as an empty
tracked file by a `git add -A`. `*.asm` is gitignored now.

**7. A multi-statement macro cannot live in a single-line `if … then`.** `EMIT` in `mdHilite`
expands to four statements; the one-line form silently breaks the parse two lines later.

## What is left

**Phase 6 — the corpus.** Not started, and deliberately: the content does not exist yet. When it
does, it is a `f1markdown.ini` edit and nothing else. The file documents its own format.

**PsTooltip.** `PsTipHost` is attached exactly as `gallery2` — its only other caller — attaches
it, and instrumenting both callbacks proves they fire and that the text resolves on hover. **The
popup surface never appears.** That puts the fault inside `PsTipHost` or `PsPopupHost`, not in
this file. The author has parked it. The wiring is left in place, commented, and the captions go
to the status bar meanwhile, which is what Explorer and Office do for an icon toolbar anyway.

**External links are copied to the clipboard, not opened.** PsPlatform has no open-url call;
`ShellExecute` here would be raw Win32 above the platform layer, which
`scripts/check-isolation.sh` exists to prevent, and it would need writing twice for Linux. If a
platform-layer `OpenUrl` ever lands, this is one line.

**In-page `#anchor` links** are out of v1 by decision. A link to one opens its page and stays put
rather than doing nothing.

**A fence does not wrap and is clipped.** `page.nCodeMaxW` already carries the widest line, so a
horizontal scrollbar can be added without re-laying anything out.

## Two things about this branch

**It carries the shell workstream too.** `feat/f1markdown` was cut from `feat/cross-platform`, so
three of the author's shell commits sit in this log, and phases 3 and 4 have some of his
in-progress `src/shell` and `docs/port` edits folded into them by a `git add -A`. **`git log --
<path>` is more reliable than reading commit subjects** when tracing who changed what — the same
warning the port's handoff gives, for the same reason.

**`f1markdown.ini` is gitignored.** A fresh clone generates the starter, which points at
`docs` and nothing else. If the viewer opens showing the built-in demo and says "no documents
found", that is the correct behaviour for an unconfigured install, not a fault.
