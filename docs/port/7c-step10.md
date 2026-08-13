# Phase 7c, step 10 — three items the handoff called blocked, and none of them was

Step 9's report ended on a table: four items across three steps had been blocked on things that
had already been removed, because each note was accurate about the code in front of it and was
never checked against the library it ruled out.

**Then I did it again, in that same step.** `PsEncoding.bi` described `PSENC_UTF16BE_BOM` as
*"decoded, never written"*. `PsEncEncode`'s big-endian arm is twelve lines below it and has always
existed. I read the comment, did not open the function, and wrote the conclusion into a source
comment, a commit message, `7c-step9.md` and `HANDOFF.md`:

> *"an honest FILE_ENCODING_UTF16BE_BOM would be a label no save could honour."*

**Three of this step's four code commits were caused by a false comment, and two of those
comments were load-bearing for decisions already taken.**

---

## The three items, and what each was actually blocked on

| the note said | the truth |
| --- | --- |
| `PSENC_UTF16BE_BOM` is *"decoded, never written"* | `PsEncEncode` has a complete BE arm; `psencoding` round-trips it |
| menu ids are *"persisted in keybindings.ini"*, so inserting one reassigns shortcuts | `keybindings.ini` stores the **NAME**; `app/modKeyBindings.bi:48` says renumbering must not invalidate a user's file |
| `app/` needs `PickListKeyToValue`, which needs a `VK_*` | that function's own header says **membership** is the validity test, never the non-zero return |

Each was quoted correctly by whoever read it. None survived being tested.

---

## What changed

**UTF-16BE has an encoding id.** Until now tiko detected and decoded a big-endian file correctly
and then labelled it `FILE_ENCODING_UTF16_BOM`, so the next save wrote **the other byte order** —
a silent format change on a file the user only opened and re-saved. The id is 4, appended, because
several loops and `clsConfig`'s range check walk `ANSI..the-last-id` and rely on index == value.

> **THE "ONE REAL TRAP" IN THE PLAN WAS ALSO A FALSE COMMENT.** `modMenuIds.bi` warned twice that
> ids are persisted as numbers, and **two earlier placement decisions were made defensively
> against it**. `frmKeyboard_SaveKeyBindings` writes `wszMsgString` — `"IDM_FILESAVE"` — and the
> loader matches that string.
>
> What *does* constrain placement is `frmMain_EditTopMenuStates`, which walks
> `IDM_EDIT_START..IDM_EDIT_END` to disable the Edit menu wholesale. A command outside that range
> is never disabled with the rest. So `IDM_UTF16BEBOM` sits inside it — for the real reason.

**`AppHostServices.LoadFileText` is gone; the seam is 19 fields.** A seam field exists to let the
two binaries **differ**. Step 9 made both implementations the same four statements around
`Doc_ReadFromDisk`, which is in `app/` — at which point the field was an indirection with a
lifetime, an install line and two bodies to keep in step. It had already grown a defect from
exactly that: the two implementations disagreed about whether `true` meant success.

**The app layer stops reaching up; link debt is 1.** `app/modMenuDefinitions.inc` included a
**shell** header by relative path — invisible to a token scan, and the last structural reason the
layer was not self-contained. The membership test and the ~90-line vocabulary moved down; the
virtual-key half stayed in the shell, where the keyboard layout is.

---

## Two live defects found beside the work

**Inserting a file relabelled the document.** `clsDocument.InsertFile` passed `@this` into the
reader, so the **inserted** file's encoding was written onto the **host** document: insert a
UTF-16 file into a UTF-8 one and the next save rewrote the whole thing in a container the user
never chose. The comment above it read *"save the main file encoding because GetFileTostring may
change it"* — and nothing saved it. **Live in tiko today, until this step.**

**A big-endian file was silently rewritten little-endian.** Above; it is the item, not a side
effect, but it is worth naming as a defect rather than as a missing feature.

---

## The assertions that mattered

**In `psencoding`.** Two green assertions already covered big-endian — *"big-endian round-trips"*
and *"BE and LE bytes differ"* — and **neither says out loud that encoding happens**. A round trip
is symmetric enough to read as a decode-side property; "differs" is satisfied by any two unequal
strings. So:

```
BE is LE with every byte pair swapped
```

which cannot be true unless `PsEncEncode` wrote big-endian bytes. **It is the sentence the false
comment should not have been able to survive.**

**In the shell.** The four encoding reads now drive `clsDocument.LoadDiskFile` — the real caller —
rather than the seam that used to stand between them. And the new key-vocabulary assertions run in
a binary with **no keyboard layout, no `VkKeyScanEx` and no `VK_*`**, which is half of what they
prove.

---

## What is NOT verified

**The Edit → Encoding menu has not been opened.** The new UTF-16 BE row, its tick state, and that
existing shortcuts still work are the author's pass. The menu-id concern turned out to be
groundless, but "groundless in theory" is not the same as "checked".

**No UTF-16BE file has been round-tripped through the GUI.** The shell asserts the bytes; tiko
shares the code and was not driven.

**`ConvertTextBuffer` is still covered by nothing** — unchanged from step 9, and still tiko-side.

**The `.lang` files were not touched and did not need to be** — `Doc_EncodingName` is deliberately
unlocalized, verified rather than assumed.

---

## Revert-to-red

| commit | rules | red |
| --- | --- | --- |
| the false comment | 1 | **1** (2 failures) |
| the encoding id | 3 | **3** (3, 1, 1) |
| the seam field | 1 | **1** (8 failures) |
| the vocabulary | 1 | **1** (1 failure) |

**All of them, for the first time in six steps.** The one that nearly was not is the display name:
reverting it initially changed nothing, because `Doc_EncodingName`'s `case else` is `"ANSI"` and a
missing arm is silent — it puts the word ANSI beside a UTF-16 file. That earned an assertion
rather than a note, and now fails with `(ANSI)`.

---

## Counts, 2026-08-11

| gate | count |
| --- | --- |
| shell `--selftest` | 374 → **389** |
| the encoding suite | 44 → **46** |
| `psencoding` | 53 → **54** |
| PsPlatform `build.cmd check` | **47 suites**, 0 failures |
| `_check_app_standalone` | 18 clean, 0 errors, **debt 1** (baseline 1) |
| `_check_app_layer` | 48 files |
| `_check_shell` | 5 files |
| tiko `_compile_fast` | 0 warnings |

---

## What step 11 has to decide

1. **`clsTopTabCtl`, which is now the only thing holding the ratchet at 1.**
   `clsConfig::ProjectSaveToFile` calls `gTTabCtl` at four sites. The shell's replacement
   (`shelltabs.bi`) already exists, so the question is not "rewrite it" but **"can tiko adopt the
   shell's tab model"** — a change to its editor-per-document architecture, not a port.
2. **Two tiers, one worker.** Still serialising.
3. **Encoding still has one hole**: nothing asserts what happens when a file changes on disk
   between the read and the save.
4. **A habit, not an item.** Five false comments in two steps, three of them load-bearing. Every
   one was found by opening the code it described. That is cheap and nobody was doing it.
