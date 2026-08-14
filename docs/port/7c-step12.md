# Phase 7c, step 12 — the font subsystem: coverage fallback and real bold/italic

Handoff items **9** and **9b**, closed together because they were never two problems.

---

## They were one defect wearing two faces

`PlatPs.cxx` held a process-global font path and `FontPs` used `fp.size` and nothing else. The
face name, the weight and the italic flag — everything `SCI_STYLESETFONT`, `SCI_STYLESETBOLD` and
`SCI_STYLESETITALIC` had told Scintilla — went on the floor.

One path cannot say *"Consolas bold italic"*, and it cannot say *"…and if the glyph is missing,
try these"*. So:

| symptom | what it actually was |
| --- | --- |
| Every bold style in every theme rendered regular | the weight was dropped |
| A Korean file rendered as boxes | there was nowhere to put a second face |

**The platform layer had no way to ASK the host anything.** That is the thing this step changed;
both symptoms fell out of it.

---

## What was built

**`PsTextEngine` holds a chain of faces.** `faces(0)` is the primary — the font the setting names
— and the rest are consulted, in order, only for codepoints the primary lacks.

Three things that would have been wrong if assumed:

* **The cell table is per face.** Glyph ids are per-face numbers, so one shared table has
  Cascadia's gid 42 and Malgun Gothic's gid 42 overwrite each other — a corruption that renders as
  the wrong character rather than as an error. The atlas stays shared; coverage bitmaps do not care
  which file they came from.
* **Clusters are rebased onto the whole string.** HarfBuzz numbers each segment from zero and
  every caller treats a cluster as a byte offset into the full text. Without the shift the caret
  jumps to the start of the line on every click after a script change.
* **The run cache is flushed when a fallback is added.** Runs shaped against a shorter chain still
  hold the primary's `.notdef`, so the very line the new face was added to fix would carry on
  rendering boxes — with the fix installed — until something evicted it.

Metrics stay the primary's, deliberately: a fallback contributing to line height would reflow every
line in the file the moment one Korean word appeared.

**`PsFont` maps a name to a file.** Windows: the Fonts registry key, HKCU before HKLM — a font
installed without admin rights lives there and only there. The chain comes from
`FontLink\SystemLink`, which is *the table GDI itself used*; that is the whole point, because the
pre-port editor rendered Korean in Consolas without being asked to and this is the mechanism it did
it with. Linux: fontconfig, `dylibload`ed.

**`PlatPs` asks.** `PlatPs_SetFontResolver` and `PlatPs_SetFallbackChain`.
`PlatPs_SetFontPath` stays as the default, so every existing host and probe is unchanged.

**tiko installs it**, and its own copy of the registry walk — about 120 lines — is gone.

---

## `.ttc` collections, which step 11 recorded as unsupported

Windows' own font-link table is full of them: `MSGOTHIC.TTC,MS UI Gothic`. `FT_New_Face(…, 0, …)`
takes whichever family is first, which is a **different typeface reported as a success** — and
step 11 refused to match them at all rather than risk that.

The chain forced the issue, so it is now solved rather than dodged: entries carry `path|faceName`,
`TE_AddFallback` enumerates via `face_index = -1` and matches `family_name`, and **a name that is
not in the file is refused** rather than falling back to index 0 — the next chain entry may well
cover it.

**And the primary can be a collection too**, which is the half that is easy to miss. A user may
pick MS UI Gothic as their editor font. `TE_InitFace` and `CreateFace` exist for that.

---

## Measured, not chosen

`PSTE_MAX_FACES` was 8 until the SystemLink chain was read back on a real machine:

```
C:\Windows\Fonts\TAHOMA.TTF
C:\Windows\Fonts\MSGOTHIC.TTC|MS UI Gothic
C:\Windows\Fonts\MSJH.TTC|Microsoft JhengHei UI
C:\Windows\Fonts\MSYH.TTC|Microsoft YaHei UI
C:\Windows\Fonts\MALGUN.TTF
C:\Windows\Fonts\SIMSUN.TTC|SimSun
C:\Windows\Fonts\YUGOTHM.TTC|Yu Gothic UI
C:\Windows\Fonts\SEGUISYM.TTF
```

Eight entries plus the primary is nine. At 8 the symbol font fell off the end — **a chain that
fails at exactly the codepoints it was added for, and reports nothing.** Now 12.

(Meiryo, MingLiU and Gulim are in the registry's list and not on this machine; the existence check
drops them. That is the check earning its place, not a gap.)

---

## The revert-to-red pass caught the SUITES, twice

Nine new rules were reverted. Seven went red immediately. **The two that did not were the
interesting ones**, and both were the same failure this port has a table about — an assertion that
passes for a reason other than the one it claims.

**A prefix match left every style assertion green.** `resolve("Consolas") <> resolve("Consolas
Bold")` passed under a deliberately broken matcher, because `RegEnumValueW` happens to return
`Consolas` first. On a machine that enumerated them the other way the entire editor would render
bold and the suite would still be green. Now pinned where order cannot rescue it: `"Consola"` — a
strict prefix of a real family — must resolve to nothing.

**Removing the collection split dropped the suite from 35 assertions to 30 and still printed
"0 failed".** The skip was gated on the *answer*, so a broken split was indistinguishable from an
uninstalled font. It is now gated on the *file*.

A third was weak rather than wrong: the duplicate check looked at the first chain entry only, and
the first entry is the one with no scaled twin. Removing the dedup entirely stayed green.

**And the step-11 report said the font self-test could not be run here.** It can.
`_check_package` had already solved it — start the process, read the redirected log, tree-kill it.
11 → 13 assertions, run on every build since.

---

## Verification

| gate | result |
| --- | --- |
| PsPlatform `build.cmd check` | **48 suites**, 0 failures (`psfont` is new) |
| `pstext` | 47 → **78** |
| `psfont` | **38**, new |
| `pstec` | 16 → **18** |
| tiko `_compile_fast`, `_compile_shell` | exit 0, **0 warnings** |
| shell `--selftest` | **383**, encoding **48** |
| `TIKO_FONTFILE_SELFTEST` | 11 → **13** |
| `_check_scihost`, `_check_app_layer`, `_check_shell` | green; 48 app files, 5 shell files |
| `_check_app_standalone` | 18 clean, **debt 1, baseline 1** |
| `_check_package` | green **after re-staging** — `libpsscintilla.dll` changed and the check was right to notice |

**The pixel-digest render suites passing is the evidence that moving `TE_DrawText` onto the cache
changed no output.** It had been calling `TE_Shape` on every draw — re-shaping every visible line
on every caret blink — while the header calls that cache "not optional" and measures it at 33.7×.

**NOT VERIFIED BY ME:** anything visual, and everything on Linux. The fontconfig path compiles and
has never been executed; that is said in the header, in the suite and in the commit.

---

## Deliberately not done, and each is recorded

* **Synthetic bold and oblique.** Author's decision. A family with no bold cut renders regular.
  Emboldening an outline when a real bold exists elsewhere looks worse than not doing it, and every
  editor font in common use ships all four files.
* **No bundled fallback font.** Author's decision. SystemLink and fontconfig cover every realistic
  machine — Unifont would be 12 MB against the 0.7 MB fonts these repos ship.
* **Per-face metric normalisation.** A fallback face at the same pixel size can have a different
  cap height and sit visibly small or large beside the primary.
* **Non-ASCII family names.** Names cross `PsFont` as FreeBASIC `string`, so the Windows lookup
  narrows the registry's wide value names. English Windows loses nothing; a Korean or Japanese one
  can carry localised family names that will not match. Fixing it means `DWSTRING` throughout.
* **The `.ttc` face index on Linux.** `FcPatternGetInteger` is one more symbol and no struct — a
  small fix for whoever can test it.
* **The chrome engine `g_sciHostTE`.** `TE_Init` has no re-open path.
* **Emoji.** Colour glyph formats are a separate rasteriser path entirely.

---

## What step 13 has to decide

1. **`clsTopTabCtl`**, still the only thing holding the ratchet at 1.
2. **The Character Set combo.** It is now decidable in a way it was not: there is a font subsystem
   for it to mean something in. Wire it or remove it.
3. **Two tiers, one worker.** Still serialising.
