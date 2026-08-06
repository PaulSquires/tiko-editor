# Making `src/app` compile against PsCore alone

The gate is `_check_app_standalone.bat`: build each file in `app/` against
PsCore and the app layer's own headers, with **no AfxNova, no `windows.bi`, no
`src/` headers**. That is the property the layer exists to have, and it is what
the eventual shell binary — a fresh translation unit with no AfxNova in it —
will require.

    0 clean, 7 with errors

## The correction this forces

Phase 7b said `src/app` was "the layer that does not know it is on Windows". It
is not. It is **a directory of files that happen to contain no Win32 tokens**,
which is a weaker and different thing, and the grep-based ratchet can only ever
check the weaker one.

Two examples the compiler found in one run and three ratchet audits never could:

* `modFuzzy.inc` used `max()`. FreeBASIC has no `max`; it was coming from
  `win\windef.bi`. The file's own `#define max` sat commented out precisely
  *because* the unity build supplied one. **A file was depending on a Win32
  header macro**, and nothing about the name says so. Now restored and
  `#ifndef`-guarded.
* `modNavHistory.bi` includes `../clsDocument.bi` outright — a transitive
  dependency on the shell that no per-file token scan sees.

## What actually blocks closure: the document model

`clsConfig.bi`, `modNavHistory.bi` and `modProjectFolders.inc` all reach for
`clsDocument` and `gApp`. `clsDocument` holds Scintilla's `HWND`, so it is
shell-side — and it is also the centre of the app layer. **The layer cannot
close around a document model that is on the other side of the boundary.**

`clsDocument` stops being shell-side when its `HWND` becomes a `PsSciView`,
which is the plan's **7d**, the Scintilla swap. Phase 6 already built
`PsSciView` and Gate 6 proved it on three platforms, so 7d is ready to do.

**That reverses the plan's order.** It sequences 7c (shell) before 7d
(Scintilla). Measured, the dependency runs the other way:

    7d  clsDocument's HWND -> PsSciView
        |
        +-- app/ can then close: clsConfig, modNavHistory, modProjectFolders
            |
            +-- the shell binary can include app/, and 7c can begin

## Done here

* `FILE_ENCODING_*` moved to `app/modDocEncodingIds.bi`. Pure data that
  `clsConfig.bi` needed; following it into `clsDocument.bi` would have dragged
  the document model across.
* `modFuzzy.inc`: the `max` macro restored and guarded; one `instr` whose
  DWSTRING is argument three converted to `PsInStr`.
* The gate itself, and its exclusion of `modThemeKeys.bi` — an X-macro table of
  `THTEXTKEY(...)` rows whose macro the includer defines. Not a translation
  unit; excluding it is correct rather than a concession.

## Still open

`clsConfig.bi` needs `OUTPUT_TABS_HEIGHT`, `FRMDEBUG_DEFPCTMAIN` and
`FORMAT_RULES` — UI layout defaults stored in the config type. Moving those
into `app/` spreads UI knowledge into the portable layer; splitting `clsConfig`
does not. That is a design decision, not a mechanical fix, and it is the next
thing to settle.
