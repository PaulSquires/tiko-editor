# Phase 7c, step 26 — the find/replace engine moves down

`frmExplorer.inc` is ported. The next form worth having in the shell is the **Find bar** — the
most-used editor feature, a bar rather than a dialog, and `PsTextBox` and `PsIconPanel` are both
proven now.

**But its engine was in the shell.** This step moves it. **The bar is step 27**, which is the
pattern every earlier step used — `clsDocument` in step 3, `Doc_ReadFromDisk` in step 9,
`ProjectSaveToFile` in step 13 — for the same reason: a form ported on top of a model still in the
shell cannot be gated.

---

## The search half was already portable and nobody had noticed

Every function was `SciExec(hEdit, …)` on an `HWND`. **`SciExec` is `SendMessage`**, and
`app/modScintilla.bi` has declared the same call in portable types since step 3, saying so in its
own comment:

```
(any ptr, ulong, uinteger, integer) as integer
```

So the port is a rename at sixty-odd call sites and an `HWND` that becomes an `any ptr`.

**And each of the three shell reaches had a seam already waiting for it:**

| reach | where it went |
| --- | --- |
| `gTTabCtl.GetActiveDocumentPtr()` | `TabDocAt(TabActiveIndex())` — added in **step 13** for precisely this question |
| `pDoc->hWndActiveScintilla` | `GetActiveScintillaPtr()`, an `any ptr` since **step 3** |
| `SetWindowRedraw` | `gAppHost.SetViewRedraw` — **already existed and had never had a caller** |

One new field was needed: `RefreshFindBar`, a Notify.

## The type lost exactly two members

* **`hCueBannerFont as HFONT` — dead.** Nothing has read it since the cue banner became the text
  control's own business. **Deleted rather than moved**: carrying a dead Win32 handle into the
  portable layer to keep a diff small would have been the worse of the two choices.
* **`rcResults as RECT` — stayed**, as `gFindResultsRect`. Five reads in `frmFind.inc`, all
  layout, and a rectangle measured by a Win32 window belongs with the window. The same split
  `clsConfig` took in step 3.

Everything else in the 24-member type is `DWSTRING`, `long` and `boolean`.

## Two signatures changed, and one of them is about locale

**`HighlightSearches` and `DoReplace` take the occurrence colour.** They read
`theme.editor.occurrence` directly, and `theme` lives in the shell's `modThemeTypes.bi` — so the
alternative was moving the whole theme tree down to satisfy one indicator colour.

**`isupper` / `islower` are gone, tested inline as ASCII.** They are not declared in this layer,
and `crt/ctype.bi` would have brought back a **locale-dependent** answer for anything above 0x7F —
where `PsUCase` and `SymDb_NameEqW` both fold ASCII only and say so. A Preserve Case replace that
behaved differently depending on the C locale would be the odd one out, and the difference would
present as *"replace mangled my accented text"* on one machine and not another.

A non-ASCII character is therefore neither upper nor lower, so a string containing one falls to
the "mixed" arm and is replaced verbatim. That is the safe answer.

---

## The seam caught the new field before a single assertion ran

```
tikoshell: AppNotify.RefreshFindBar is not set (build error)
```

Exit 2, immediately. **`tikoshell`'s body for it is empty and correct** — it has no find bar, so
there is nothing to repaint — and that is the distinction this seam was built on: **a Notify field
can be legitimately empty; no Services field can.**

**An empty body is a decision. An unset pointer is an omission.** The completeness check exists so
the second cannot masquerade as the first, and this is the first time in twenty-six steps it has
had to say so about a field added in the same commit.

---

## Verification

| | |
| --- | --- |
| `_check_app_layer` | **48 → 50 files**, free of Win32 and AfxNova |
| `_check_app_standalone` | **18 → 19 clean**, 0 errors, debt 0 and still no baseline |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | 493, unchanged — the shell does not call the engine yet |
| `_check_selftests` | **33 / 20,328 in a clean tree** |
| `_check_shell`, `_check_scihost`, `_check_package` | green |
| revert-to-red | unsetting `RefreshFindBar` in the shell → `exit 2` and the field named, before any assertion |

**NOT VERIFIED BY ME, AND THIS IS THE STEP WHERE THAT MATTERS MOST SO FAR.** This is a move of
**live code that `tiko.exe` runs every time anyone presses Ctrl+F** — not an addition beside
existing behaviour. Every gate here proves it compiles, links, and leaves 20,328 assertions
standing. **None of them proves it still finds anything.**

The interactive pass this needs is narrow and specific:

* type in the find field — the count should read `n/m`, not `0/0`
* **F3 and Shift+F3**, including the wrap at each end
* **Match Case**, **Whole Word**, **Selection** — each should change the count
* **Replace** and **Replace All**, and **Preserve Case** on a mixed-case match
* switching tabs with a search active — the count should follow the document

**The one A/B break:** `DoReplace` read the two text boxes through `GetDlgItem` +
`AfxGetWindowText` and now reads `gFind.txtFind` / `txtReplace`. The field's change handler writes
those, so they agree by construction — but Replace is the path to watch.

---

## What is left

1. **The interactive pass above.**
2. **The Find bar in the shell** — step 27, and the engine is now reachable from it.
3. **Run step 18's Linux scripts** — three `.sh` files, unrun eight steps later.
4. **fontconfig** — still never executed anywhere.
