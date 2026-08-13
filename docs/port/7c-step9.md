# Phase 7c, step 9 — one reader, and ANSI stops being a layout

The shell has **saved** since step 3 and could not **decode** until now.
`ShellHost_LoadFileText` read bytes, called them UTF-8, and never touched
`pDoc->FileEncoding`. A UTF-16LE file was therefore not merely mis-saved — it was **mojibake on
screen**, the bytes handed to Scintilla as UTF-8, NULs and all, before anyone tried to save it.

`clsApp.inc` said the UTF-8 default was safe *"because LoadDiskFile immediately sniffs the file's
real BOM."* True of tiko. False here, for six steps.

---

## THE BLOCKER HAD ALREADY BEEN REMOVED. AGAIN.

`modRoutines.inc` carried this note against `GetFileToString`:

> *"GetFileToString decodes UTF-16 through WideToUtf8, a PRIVATE helper in modEncoding.inc that
> is WideCharToMultiByte. The WRITE path is already portable (Doc_EncodeForDisk, Doc_WriteToDisk
> are in app/); only reading still needs Win32."*

**Every sentence of that was true and the conclusion was wrong.** Reading did not need Win32; it
needed `PsEncDecodeAuto`, which has implemented tiko's exact ladder — UTF-8 BOM first, then
`FF FE`, then `FE FF`, then a strict UTF-8 validation, else ANSI — for as long as PsEncoding has
existed, with **53 assertions** over it in `tests/psencoding`.

That is the **third consecutive step** to find this:

| step | the item | what was already there |
| --- | --- | --- |
| 7 | — | `PSEV_USER` + a thread-safe `Post`, written for a worker, never called |
| 8 | `FilenameOriginalCase` "needs a PsCore canonical-path call first" | `PsFileRealCase` |
| 8 | the shell's bit-packed row slot | `itemData2`, in the struct, zeroed, unreachable |
| 9 | "only reading still needs Win32" | `PsEncDecodeAuto` |

**Each note was accurate about the code in front of it and was never checked against the library
it was ruling out.** Three of the four were re-read at an audit and re-counted rather than
re-tested. That is the finding of this step; the reader is the artefact.

---

## What changed

**One reader.** `Doc_ReadFromDisk` sits in `app/modEncoding.inc` beside the `Doc_WriteToDisk`
that was already there. `GetFileToString`'s seventy lines of `CreateFileW`, hand-written BOM
ladder, endian-swap loop and `WideCharToMultiByte` are gone. Both binaries call the same
function, so there is one answer to *"what encoding is this file"* instead of two feeding one
filename-keyed document model.

**ANSI became a disk format.** Decided with the author, and it is the largest behaviour change of
the port so far.

> tiko has read ANSI files one way and written them another **since long before this port**. The
> writer always encodes CP-1252; the reader passed bytes through untouched for Scintilla to
> interpret under **codepage 0, the system codepage**. On any machine whose ANSI codepage is not
> CP-1252, an ANSI file did not round-trip, and nothing said so.

```
E1 (retired)   SCI_GETCODEPAGE = iif(FileEncoding = ANSI, 0, SC_CP_UTF8), and
               ConvertTextBuffer is the ONLY code permitted to move either side.
E2 (now)       SCI_GETCODEPAGE IS SC_CP_UTF8 ON EVERY POPULATED DOCUMENT, ALWAYS.
               FileEncoding says what to WRITE and never what is in memory.
```

E1 was enforced in **five places that each had to remember the rule**. Nothing needs permission
to maintain E2 because nothing needs to do anything.

`ConvertTextBuffer` **was** the conversion and is now a relabel: sixty lines to six. No decode, no
re-encode, no `SCI_CLEARALL`, no `SetText`, and no restoring the caret by line and column because
no byte offset moves.

### A confirmation dialog went with it

Switching a document to ANSI used to ask first, because the switch **destroyed characters
immediately** and `SetText` emptied the undo buffer — its default button was Cancel for that
reason. Nothing is destroyed now; the loss, if any, happens at the write, and the **lossy-save
prompt already guards that at the moment it is true.**

**One prompt where there were two, and it is user-visible**: choosing Edit → Encoding → ANSI on a
document full of CJK no longer warns at the click. It warns at the save, and the document
survives a cancel there completely.

Its two localization ids are left unused rather than renumbered. The self-test assertions that
guarded them were **retargeted** onto the prompt that still exists — they had been checking that
a dead dialog's strings were present.

---

## The suite that could not move, and why moving it mattered

`modEncodingSelfTest` sat in `src/` because it named `WideCharToMultiByte`, `CFileStream` and
`GetFileToString`. All three died in commits 1–3.

**It had not been run once during this step until commit 4.** tiko can only run it from inside a
started GUI behind `TIKO_ENCODING_SELFTEST`; the portable shell runs it **headlessly** from
`--selftest`. Commits 1–3 went in without it. That is the argument for the move, as evidence
rather than as a preference.

### The oracle got STRONGER, not merely portable

Its independent oracle was `WideCharToMultiByte` **deliberately** — code the subject does not
share, so a bug cannot hide behind both sides of a comparison.

**The obvious port would have destroyed exactly that.** Reaching for `DWSTRING.Utf8` or
`PsEncEncode` makes the oracle the same implementation as the subject, and every assertion below
it would then prove only that PsEncoding agrees with itself. It is twenty lines written out from
the UTF-8 rules instead: independent of the old Win32 helper **and** the new PsCore one, and
portable as well.

`modSaveSelfTest` **stays** in `src/`, deliberately. Its header says `SaveSelfTest_ReadAll` uses
raw `CreateFileW` *on purpose*, so the reader and the writer under test are not the same layer —
the identical argument. Preserved rather than tidied away.

### And it found a hole in a gate

`_check_app_standalone` builds its prelude from the `app/*.bi` files `tiko.bas` names
**directly**, and `tiko.bas` names `app/modEncoding.inc` only. So `Doc_EncodeForDisk` had never
been in scope for that check and nothing had ever needed it to be. Fixed in the moved file rather
than in the gate: **a translation unit that states its own dependencies cannot be broken by
someone else's include order.**

---

## A defect found next to the one being fixed

The seam's `LoadFileText` had **no agreed polarity**. tiko's implementation forwarded
`GetFileToString`, which returns **false on success**; the shell's returned **true**;
`LoadDiskFile` tested for **false**.

So a successful load in the shell **never stamped `DateFileTime`** — the value the file-watch and
the reload prompt compare against, meaning a shell document could never look stale. Invisible
while there was one implementation; live from the moment there were two. `true` now means
success, stated where the field is declared rather than at one implementation.

---

## What is NOT verified

**`ConvertTextBuffer` is not covered by anything.** It is tiko-side and the shell never links it,
so the relabel — and whether it still marks the document dirty — is the author's interactive
pass, through Edit → Encoding.

**Nor is the removed prompt.** That a document full of CJK now warns at the save instead of at
the click is asserted nowhere.

**tiko's own binary was never observed reading a UTF-16 file.** Both binaries call the same
function and the shell's 374 assertions exercise it, but tiko's GUI was not driven.

**A non-Western system codepage is the case that cannot be tested from this machine**, and it is
the case the ANSI change exists for. On a Western install CP-1252 and the system codepage are the
same mapping and nothing moves.

**UTF-16BE round-trips to LE, silently.** tiko's enum has no BE member and `PsEncEncode` refuses
to write one, so an honest id would be a label no save could honour. Pre-existing — tiko's old
reader kept the endianness in a local that died with the call — and now inherited by a second
binary. **The real fix is in PsEncoding.**

---

## Revert-to-red

| commit | rules | red |
| --- | --- | --- |
| the reader | 4 | **4** (4, 4, 2, 1 failures) |
| tiko's reader | — | covered by the shell's, which calls the same function |
| ANSI as a disk format | 3 | **2** (4 failures; 1 failure reporting `(0)`, the old codepage) |
| the suite moves | — | 44 assertions that had not run at all |

**The decode revert's message is the bug itself**: *"the text decodes to UTF-8 (8 bytes)"* — the
UTF-16 bytes with NULs, where four UTF-8 bytes belong.

**One did not go red** and it is the `ConvertTextBuffer` relabel, for the reason above: the shell
cannot reach it. Every step since 5 has had at least one of these, and the note is worth more
than a forced assertion.

---

## Counts, 2026-08-11

| gate | count |
| --- | --- |
| shell `--selftest` | 357 → **374** |
| the encoding suite, now run by the shell | **44**, 0 failed |
| `_check_app_standalone` | **18 clean**, 0 errors, debt **2** |
| `_check_app_layer` | **48 files** |
| `_check_shell` | 5 files |
| `_check_scihost` | green |
| tiko `_compile_fast` | 0 warnings |

---

## What step 10 has to decide

1. **UTF-16BE, properly.** `PsEncEncode` cannot write it; until it can, no honest encoding id
   exists for a file tiko reads correctly and rewrites in the other byte order.
2. **`AppHostServices.LoadFileText` is close to pointless now.** Both implementations are one
   call to `Doc_ReadFromDisk` plus a `FileEncoding` assignment. What is left at the seam is the
   platform's error text.
3. **`clsTopTabCtl`: portable rewrite, or a Win32 facade forever?**
4. **Two tiers, one worker.** Still serialising.
5. **The two remaining link-debt bodies**, one of which is an `app/` file reaching UP into the
   shell by relative path.
