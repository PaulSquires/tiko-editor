# Phase 7c, step 11 — the editor's font setting starts working

The author opened a UTF-8 Korean file and every glyph was a box.

**It was misdiagnosed twice before anyone opened the renderer.** First as an encoding bug — it was
not; the bytes were always intact and Notepad proved it. Then as the Options *Character Set*
setting — inert, because `PlatPs.cxx` never reads `lfCharSet`. Both diagnoses were argued from
behaviour and from comments.

The question that found it was the author's:

> *"Why did this work on the original Tiko using the same Consolas font?"*

---

## What was actually wrong

**The editor rendered in `C:\Windows\Fonts\consola.ttf`, hard-coded, and nothing in Options could
change it.**

| | |
| --- | --- |
| `PlatPs.cxx:103-106` | `FontPs` receives Scintilla's `FontParameters`, which carries the face name, and uses **only `fp.size`**. The face comes from a process-global `g_fontPath`. |
| `frmSciHost.inc:20` | `SciHost_SetFontPath` existed and **had no caller**, so the path was always the `consola.ttf` fallback. |

So `SCI_STYLESETFONT` — the call every editor in the world uses to choose a font — went nowhere.
Picking a font in Options changed a config value, a combo box, and nothing else.

**The old build was not better by accident.** tiko drew through `Scintilla64.dll`, the Win32
platform layer, which honoured the face name *and* font-linked to cover glyphs the face lacked.
`libpsscintilla` replaces that layer wholesale with `PlatPs.cxx` → `PsTextEngine` →
FreeType/HarfBuzz/Blend2D, and **both** capabilities went with it. This step restores the first.

---

## What was done

**`SciHost_ResolveFontFile`** — family name to font file, from the registry, where Windows keeps
the mapping (`"Consolas (TrueType)" = "consola.ttf"`).

* **HKCU is read first and it is not optional.** Fonts installed without admin rights live there
  and only there; an HKLM-only lookup reports them missing.
* **Exact match after stripping the parentheses.** `"Consolas Bold"` must not satisfy a request
  for `"Consolas"`, or the bold face silently becomes the regular one for the whole editor.
* **The fallback is load-bearing, not politeness.** `TE_Init` returning 0 makes
  `SciHost_GlobalInit` return false, which is an editor window that never appears. Every failure
  path returns Consolas.

**`SciHost_ApplyConfiguredFont`** pushes the result to **both** places, because either alone is
useless: `g_sSciHostFont` is what new views get, `PlatPs_SetFontPath` is what existing views read
when they realise a face.

**No views are recreated.** `ViewStyle::Refresh` clears the font cache on any restyle and
re-realises from the current path, so the walk Options already performs does the work. That
matters: `DestroyScintillaWindows` takes the Scintilla `Document*` with it, losing undo, markers,
folds and the FIP pool's `ADDREFDOCUMENT` references.

**Three call sites**, and the third is the one that would have been missed: after `LoadConfigFile`;
in the Options apply path **before** `Theme_ApplyAll` rather than after, or the font changes one
apply late; and in `ReloadConfigFileTest`, which runs the same walk when `settings.ini` changes on
disk — miss that and the font follows the dialog but not a file edit.

---

## Deliberate changes from the plan

**Collections are not matched.** The plan said: if a family resolves to a `.ttc`, use it and record
the limitation. Several CJK families ship as one `.ttc` under a value name listing several
families, and `TE_Init` opens face **index 0** — very likely a different family from the one asked
for. So such a name fails to match and falls back. **A readable fallback beats a confident wrong
answer**, and rendering the wrong typeface while reporting success is the worse failure.

**It lives in `modViewStyle.inc`, not `frmSciHost.inc`.** `_check_scihost.bas` compiles
`frmSciHost.inc` with a deliberately minimal include set, and it rejected two attempts to put this
there — first for AfxNova's `Ps*` string helpers, then for `gConfig`. The resolver itself stayed
and uses FreeBASIC's own `wstring` built-ins; the applier moved to the file that already reads
`EditorFontname`.

---

## Verification

**The gate that mattered was the author's**, and it passed: Options → Colors → Font → Malgun
Gothic → OK renders the Korean **live, with no restart**, and switching back to Consolas brings
the boxes back — which is the proof the setting is read on every apply rather than once at
startup.

**11 assertions** in a new env-gated suite (`TIKO_FONTFILE_SELFTEST=1`), which I could not run:
it needs the GUI. **The author ran it, and its first run failed** — see below.

`_check_scihost`, `_check_app_layer`, `_check_shell` green; debt 1; shell suite 383; encoding
suite 48; PsPlatform 47 suites; tiko warning-free.

### The assertion that was wrong when it was written

```
FAIL: a STYLE name matched as if it were a family
```

It asserted that `ResolveFontFile("Consolas Bold")` should fall back, reasoning that a style is
not a family. But `"Consolas Bold (TrueType)"` is a real value name under the Fonts key and this
function maps **name → file** — resolving it is correct. **The code was right and the suite was
wrong**, and it had never been run when it was written.

It now asserts the property that is actually load-bearing: `"Consolas"` and `"Consolas Bold"`
resolve to **different** files. A prefix match would answer `consolab.ttf` for both.

---

## What is NOT fixed, and both are recorded

**FONT FALLBACK STILL DOES NOT EXIST.** `PsTextEngine` holds one `FT_Face`
(`PsTextEngine.bi:99-100`), loaded by the single `FT_New_Face` in the paint layer
(`PsTextEngine.inc:24`). A codepoint the chosen font lacks is still a box. The old GDI build
font-linked; this does not. **It is a toolkit problem, not an editor one** — every PsPlatform
control paints through that engine, so a non-Latin filename in a tab bar tofus identically, on
every platform. Handoff item 9.

**BOLD AND ITALIC ARE IGNORED, EVERYWHERE.** `FontPs` drops `fp.weight` and `fp.italic`, and
`PsTextEngine` has no weight concept at all — no `FT_Outline_Embolden`, no oblique matrix. **Every
bold style in every theme currently renders regular.** Scintilla asks correctly; nothing below
listens. Fixing it means a family resolving to four files and `FontPs` keying its engine on
`(path, pxSize)` derived from `(faceName, bold, italic, size)` — `F1Markdown`'s `mdFonts.inc`
design, generalised. New handoff item.

**The `.ttc` face index**, above.

**The Options "Character Set" combo is still dead UI** — handoff item 10, unchanged. It was live
under `Scintilla64.dll`.

---

## What step 12 has to decide

1. **The font subsystem, properly**: fallback chain + bold/italic. They are the same layer and
   the same work, and fixing one without the other still leaves boxes or still leaves bold flat.
2. **`clsTopTabCtl`**, still the only thing holding the ratchet at 1.
3. **The Character Set combo**: wire it or remove it.
4. **Two tiers, one worker.** Still serialising.
