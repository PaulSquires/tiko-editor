# Open questions in the Tiko Editor help

**All questions are answered — 0 TODO callouts remain in the built help.**

This file is now a record rather than a worklist: each item below keeps the answer it was
closed with, so you can see what was verified and how. Items marked *ANSWERED FROM SOURCE*
were confirmed by reading the Tiko Editor source; the rest were answered by the author.

After answering, the fix goes in `tools/content_*.py` (never the HTML), then:

```bash
cd C:\dev\tikohelp\tools && python build.py
```

Find them all in the built site with `grep -l 'callout todo' *.html`.

---

## A. Options dialog — open it and read down the pages

### ~~A1. Page names and order~~ — ANSWERED
Nine pages: General Options, Code Editor, Advanced Code Editor, Editor Font, Compiler Setup,
Localization, FreeBASIC Keywords, Windows API Keywords, Extra Keywords. My list had eight and
included a **Colors** page that does not exist — colours live in the Themes dialog, now noted
explicitly. **Documented.**

### ~~A2. Compiler page fields~~ — ANSWERED
Tiko Editor ships a FreeBASIC toolchain. Toolchains are subfolders of `toolchains\` each
holding both `fbc32.exe` and `fbc64.exe`; the page lists them, and selecting one sets both
compiler paths at once. Page controls: toolchain list, compiler switches, include paths,
run-via-command-window, disable-compile-beep. Architecture comes from the build
configuration, not the toolchain. **Documented.**

### ~~A3. Whitespace display and word wrap~~ — CLOSED, NOT NEEDED
Section removed entirely at the author's request, along with the TODO.

### ~~A4. Enumerated setting values~~ — ANSWERED FROM SOURCE
All four confirmed, and **KeywordCase was wrong in every value**:

| Setting | Real values |
|---|---|
| `KeywordCase` | 0 lower, 1 upper, 2 proper (canonical spelling), **3 original case** (default) |
| `NewFileEncoding` / `UnicodeEncoding` | 0 ANSI, 1 UTF-8, 2 UTF-8 (BOM), 3 UTF-16 (BOM) |
| `UnusedKindMask` | bitmask: variables 1, procedures 2, parameters 4, types 8, fields 16, constants 32 (63 = all) |

I had documented KeywordCase as "0 unchanged, 1 upper, 2 lower, 3 proper". The syntax
highlighting page has been corrected to match. **Documented.**

| Setting | Default | I assumed |
|---|---|---|
| `KeywordCase` | `3` | 0 unchanged, 1 upper, 2 lower, 3 proper |
| `NewFileEncoding` | `1` | an encoding id — which? |
| `UnicodeEncoding` | `0` | an encoding id — which? |
| `UnusedKindMask` | `63` | bitmask of the 6 `UNUSED_KIND` values (63 = all six) |

### ~~A5. Supported encodings~~ — ANSWERED
Exactly four, and the docs now use the names the status bar shows: `UTF-8`,
`UTF-8 (BOM)`, `UTF-16 (BOM)` and `ANSI`. There is no BOM-less UTF-16 — Tiko Editor always
writes the mark for UTF-16. **Documented.**

### ~~A6. Keyword list files~~ — ANSWERED
Three, one per Options page: `freebasic_keywords.txt` (the language), `winapi_keywords.txt`
(Win32 API names), `extra_keywords.txt` (everything else — the one to add your own to).
`freebasic_keywords_default.txt` is the shipped baseline for restoring the first, and
`codetips.ini` supplies parameter hints for built-in keywords. **Documented.**

---

## B. Find, Replace and Find in Project

### ~~B1. Find bar options~~ — ANSWERED
Six icons: Match Case, Match Whole Words, Selection, Toggle Replace, Search Previous,
Search Next. The first three latch. Opening Find with a multi-line selection auto-enables
Selection and clears the search box. **Documented.**

### ~~B2. Find in Project filters~~ — ANSWERED
**There are none.** Find in Project takes the same two options as Find — Match Case and
Match Whole Words — and searches every file in the workspace. My "file masks, folder scope,
include/exclude patterns" was invention and is gone. Confirmed in source: `FindProject_Search`
accepts exactly `bMatchCase` and `bWholeWord`. **Documented.**

### ~~B3. Regular expression dialect~~ — MOOT
**Tiko Editor does not support regular expressions at all.** Confirmed in source: only
`SCFIND_MATCHCASE` and `SCFIND_WHOLEWORD` are ever passed to the search engine. The
`regular-expressions` page has been deleted and every reference across the site corrected.

---

## C. Explorer and projects

### ~~C1. Removing a file from a project~~ — ANSWERED
Right-click the file in the Explorer, or its tab if open, and choose **Remove from
project**. Never deletes from disk. Source added three conditions: the command only appears
once the project is **named**; on an untitled workspace closing the tab is what removes the
file; and cancelling the unsaved-changes prompt abandons the removal. **Documented**, and
the absolute "closing a tab never removes a file" claim has been qualified in five places.

### ~~C2. Setting the main module / resource file~~ — ANSWERED
Right-click the file in the Explorer **or on its tab** and pick from Main file / Header file
/ Module file / Resource file / Normal file — the current type is marked. Dragging between
Explorer groups does the same. Multi-select retypes several at once. Source detail: setting a
new Main or Resource moves the previous one to **Normal**. **Documented.**

### ~~C3. Project Options fields~~ — CLOSED, NOT NEEDED
The dialog is self-explanatory; its fields are labelled plainly and behave as their names
suggest. TODO removed from the page, with a short note saying as much.

---

## D. Other dialogs

### ~~D1. Theme save commands~~ — ANSWERED
**Clone** an existing theme, **Edit** the copy, and use the **pencil icon** beside the
description to name it. List view has Edit / Clone / Delete / Set as active; editor view has
the pencil and Back. Source detail: only `default_dark` and `default_light` are protected
(Edit and Delete disabled) — the other twelve shipped themes are editable in place; a clone
is tagged "(cloned)"; and deleting the last theme reseeds `default_dark` from a backup.
**Documented.**

### ~~D2. User tool substitution codes~~ — ANSWERED
Four, exactly as the field's tooltip lists them: `<P>` project name, `<S>` main source file,
`<W>` word at the caret, `<E>` compiled exe/dll/lib. Case-insensitive. Source detail: `<P>`
is empty on an untitled workspace and `<S>` arrives already quoted.

**There is no code for the current file or a folder path** — which invalidated the Lesson 10
exercise ("Open project folder"), now rewritten to `explorer.exe /select,<E>`. **Documented.**

The Lesson 10 TODO was removed at the author's request; that exercise now just tells the
reader to hover the Parameters field for the codes, so nothing is left dangling. Only the
`user-tools` reference table is still outstanding.

### ~~D3. Shipped build configurations~~ — ANSWERED
All twelve documented with their switches (Win32/Win64 x GUI/Console x Release/Debug, plus
DLL and static library targets), including a table explaining what each switch does.
**Documented.**

---

## E. Interface details

### ~~E1. Status bar fields~~ — ANSWERED
Seven panels, left to right: **caret position** (inert readout, adds a selected-character
count), **compile status** (success/fail/spinner, also shows "Read Only"; opens the Output
window), **Theme Designer**, **build configuration**, **Spaces** (tab size), **encoding**,
**line endings**. All but the first two are click-to-change. My version had two fields that
do not exist — insert mode and language — both now removed sitewide. **Documented.**

### ~~E2. Context menu items~~ — CLOSED, NOT NEEDED
TODO removed at the author's request. The summary table of which menu appears where stays.

### ~~E3. Per-monitor DPI behaviour~~ — CLOSED, NOT NEEDED
TODO removed at the author's request. The short High-DPI displays section stays — it says
only that Tiko Editor scales its interface to the display's DPI, which is safe and accurate.

---

## F. Editor commands — do these exist?

### ~~F1. Sort / Join / Transpose / Trim~~ — ANSWERED
None of Sort Lines, Join Lines or Transpose Lines exists, and there is no standalone trim
command. Trailing whitespace has two routes: the **Strip line ending whitespace when saving**
option on the Advanced Code Editor page, and the formatter's Trim trailing whitespace rule.
**Documented**, including the absence, which is worth stating.

### ~~F2. Minimap~~ — ANSWERED
There is no minimap. Now stated definitively on the view-options page and in the FAQ,
pointing at Fold All and the function list instead. **Documented.**

---

Two questions are left: **A4** (four enumerated setting values) and **D2** (user tool substitution codes).
