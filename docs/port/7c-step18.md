# Phase 7c, step 18 — the Linux path, and a class no gate could see

The handoff has listed the same first item since step 16: **`tikoshell` has never been built for
Linux.** It lives in *this* repo, so it needs both trees side by side, and **there was not one
`.sh` in it.**

The scripts turned out to be the small half.

---

## What the investigation found before a script was written

`src/shell` and `src/app` are clean of Win32 **identifiers** — that is what `_check_app_layer`
and `_check_shell` gate, and both were green. They were full of Win32 **path separators** and
`%TEMP%`.

**Same shape as the fifth ratchet gap this port already recorded, one layer down.** That one was
`app/modMenuDefinitions.inc:22` reaching UP into the shell by relative path, invisible because *a
path is not an identifier*. A separator is not one either.

| | |
| --- | --- |
| `environ("TEMP")` | 4 sites, all in `--selftest`. **Unset on Linux**, so each resolved to a bare relative name |
| backslash path literals | ~25 in `shell/tikoshell.bas`, **10 in `src/app`** |
| separator-as-absoluteness | `shellscan.bi` ×3 — *"does this document have a path on disk"* answered by searching for `\` |

**`PsKnownFolder(PSFOLDER_TEMP)` already existed** (`PsFile.inc:482`) and already returned
`$TMPDIR`-or-`/tmp`. **`PsPathIsAbsolute` already existed.** Another pair of blockers whose second
half had moved before anyone looked — this page's most-repeated finding, and it repeated again.

## The one that was already broken on Windows

`clsConfig`'s seven settings paths read `PsExePath & "settings\settings.ini"`.

**`PsExePath` has returned FORWARD slashes with a trailing `/` on both platforms since it was
written** (`PsFile.inc:662`, and its header explains the trailing separator at length). So the
result was

```
C:/dev/tiko/settings\settings.ini
```

a mixed path, in the string handed to `WritePrivateProfileString`, for as long as the app layer
has existed. Windows opens it, so nothing ever said so. That also shrank the only A/B risk in
this step to nearly nothing: the profile API was **already** being given forward slashes.

---

## The defect the fix exposed, which is the real content of the step

Making the shell produce PsPath-correct paths took `--selftest` from **395/0 to 377/17**.

Two lookups compared paths as **raw strings**:

* **`clsApp.GetDocumentPtrByFilename`** — a miss opens a *second* `clsDocument` for a file already
  in a tab. Two Scintilla documents, and saving one discards the other's edits.
* **`SymDb_FileNameEq`** — a miss makes `EnumProcsInFile` return zero. A Functions pane with no
  rows and nothing to say why.

Both were survivable only because **every path in the process had been through
`FilenameOriginalCase`, which ends in `PsPathToNative` and hands back backslashes** —
`modPaths.inc` says so at length, under a heading that reads *"AND THE SEPARATORS GO BACK"*. It is
an accurate comment describing a convention that was one caller away from being wrong, and step 18
is the step that wrote that caller.

Two conventions, both documented, both correct on their own terms, meeting at a raw `=`.

**Case-insensitivity in `GetDocumentPtrByFilename` is Windows-only now.** On Linux `Makefile` and
`makefile` are two files, and folding them returns the wrong document with nothing to say so.

**The fold is in the comparison, not in a normalise of both sides.** `SymDb_FileIndexOf` runs it
once per entry in the file table — 134 entries for `tiko.bas` — and a `PsPathNormalise` per call
would allocate twice each.

---

## The assertion that was vacuous, and how it was caught

The two fixes above restored 395 without adding an assertion, so **nothing in the tree would have
caught the class coming back.** Two were added: the probe path spelled the other way round,
separator by separator, handed to both lookups.

**The first draft passed with the fix reverted.**

It compared the two lookups *to each other*. The parser's file table holds a **mixed** spelling —
the include directory backslashed, the filename appended with `/` — so with the fold removed
**both** spellings miss, and the assertion compared `0` with `0` and printed `ok` beside two
genuine failures on the lines above it.

```
FAILED      and all three procedure symbols are in gSymDb  (0)
FAILED        two of which have bodies  (0)
ok          the same file with the other separator is the same file   <-- 0 = 0
```

It compares against `3` now. **Fourth time in this port that the obvious assertion constrained
nothing, and the fourth time reverting the fix is what said so.**

**And the revert-to-red harness itself failed silently first.** The patch script matched a line
ending in `\n` against a CRLF file, changed nothing, rebuilt, and reported a green run — which
reads exactly like *"reverting the fix did not break anything"*. The assertion on the patch
application is the only reason that was one line of output rather than a conclusion.

---

## The gate

`_check_shell.ps1` gets two rules, over `src/shell` **and** `src/app`, because `app/` is the half
`tiko.exe` shares:

* a backslash inside a plain string literal
* `environ("TEMP")` / `("TMP")` — **which the first rule cannot see**, because it has no backslash
  in it

Three literals allowed **by their exact text, not by file**: `"\"` (the separator itself, in a
comparison — `modProjectFolders` accepts both on purpose and is correct), and `"Ctrl+\"` /
`"Ctrl+Shift+\"`, which are the backslash **key** in a binding name.

Escaped literals are skipped, and **the strip regex has to honour `\"` or it flags every
`!"…\"…\n"` line in the suite** — which it did on its first run, twice.

---

## The Linux scripts

| | |
| --- | --- |
| `src/fbcParser/build.sh` | `libfbcParser.so` + `tiko_fbctest`, 137 modules |
| `_compile_shell.sh` | `_shell/tikoshell` |
| `_run_shell.sh` | it, with `LD_LIBRARY_PATH` |

**fbcParser is fbc's own front end.** Every `__FB_WIN32__` in it is already a guarded branch beside
a Linux one, and not one module carries an `#inclib` — so the Windows-ness was in `build.bat`, not
in the code. `-pic`, because a shared object must be position-independent and ld's complaint names
an object file rather than the flag.

**No `-rpath` on the shell**, unlike PsPlatform's own driver: that stages its `.so` files beside
each binary and uses `$ORIGIN`, and this binary's libraries live in **another tree** at a path only
the author knows. `LD_LIBRARY_PATH`, exactly as the `.bat` sets `PATH`.

`-z noexecstack` everywhere, for the reason PsPlatform's `build.bas` gives at length.

**`.gitattributes` pins `*.sh` to LF.** This repo sets `core.autocrlf=true`, and a shebang script
with CRLF fails as `/usr/bin/env^M: bad interpreter` — naming a file that plainly exists.

---

## Verification

| | |
| --- | --- |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **395 → 397** |
| `_check_selftests` | **33 suites, 20,328 passed, 0 failed**; `settings/` and `tiko.tiko` unchanged |
| `_check_shell` | green — 5 shell files, **53 files for the separator rules** |
| `_check_app_layer` | 48 files | 
| `_check_app_standalone` | 18 clean, 0 errors, debt 0 |
| `_check_scihost`, `_check_package` | green |
| PsPlatform `build.cmd check` | **48 suites, 0 failures** |
| revert-to-red | drop the separator fold → **377/19**; drop the normalise in `GetDocumentPtrByFilename` → **384/13**; each new assertion fails for its own fix and not the other's. One path literal back to `\` → gate exit 1; one scratch directory back to `environ` → gate exit 1 |

**NOT VERIFIED, and it cannot be from here:** every line of the three `.sh` files, and both
Linux-only behaviours they exist to enable. No Windows session compiles a `.so`. What *was*
exercised is `bash -n` on all three and the guard chain in two — which says they parse and refuse
correctly, and says nothing about whether the link succeeds.

**The expectation that fbcParser compiles unchanged is an expectation.** If it does not, that
failure is the first real datum and is worth more than the expectation it contradicts.

**Still never executed anywhere:** the fontconfig path in `PsFont.inc`.

---

## What is left

1. **Run it.** `bash src/fbcParser/build.sh`, then `bash _compile_shell.sh`, then
   `bash _run_shell.sh --selftest`. Everything above is a prediction until that happens.
2. **fontconfig.** Never executed on any machine.
3. **Font fallback's remaining gaps** — per-face metric normalisation, non-ASCII family names, the
   Linux `.ttc` face index. Recorded in [`7c-step12.md`](7c-step12.md).
4. **The `*_AUTOOPEN` family** is still ungated — carried since step 14.
