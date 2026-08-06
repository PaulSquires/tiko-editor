# 7d: the footing, measured

The Win32 host bridge landed in PsPlatform (`src/platform/win32host/`), which
breaks the cycle 7d ran into. This is what measuring the ground under 7d turned
up before writing the window class.

Gate: `_check_scihost.bat` â€” 10 assertions, green.

## 1. tiko can host a PsSciView without SDL3

Proven, not argued: `_check_scihost.bas` builds a real `PsSciView` inside a
translation unit that already has `windows.bi` and AfxNova in it, round-trips
text through `SCI_SETTEXT`/`SCI_GETLENGTH`, and links against **blend2d and
psscintilla only**. `strings` on the probe shows no SDL3 import.

That was the whole point of the bridge. If SDL3 had come along anyway it would
have bought nothing.

## 2. `namespace PsC` does not extend to the C bindings

**This is new, and it is why 7d is not simply "the encoder trick again".**

fbc mangles an `extern "C"` block declared inside `namespace PsC` as
`PSC::bl_context_save`, which matches nothing in libblend2d. Five undefined
references, **at link time only** â€” the compile is clean.

`PsEncoding` is pure FreeBASIC, so the encoder never hit this. `PsSciView` pulls
in Blend2D, FreeType, HarfBuzz and the vendored Scintilla fork, and every one of
those is an `extern "C"` surface.

The fix is ordering, not redesign: hoist `bind/Blend2D.bi`, `bind/FreeType.bi`,
`bind/HarfBuzz.bi` and `scintilla/PsScintilla.bi` to **global scope** before
opening the namespace. `#include once` then makes the inner includes no-ops, and
the FreeBASIC code â€” which is what needs PsCore's `DWSTRING` â€” stays inside.

## 3. The bridge could not hand-declare its own GDI imports

`PsWin32Host` declared `CreateCompatibleDC` and friends itself, to avoid
dragging `windows.bi` into every host. Against a host that *already has*
`windows.bi` that emits the same C symbol with a different type â€”
`void* CreateCompatibleDC(void*)` against `HDC__* CreateCompatibleDC(HDC__*)` â€”
and gcc rejects the pair outright.

Fixed on the PsPlatform side by including `windows.bi` there. No isolation cost:
the bridge lives under `src/platform/`, which is exactly where the ratchet
allows Win32.

Neither 2 nor 3 was findable by reading. Both took about a minute to find by
compiling.

## 4. The document sharing forces Find-in-Project into the same step

`modFindProject.inc:503` takes a Scintilla `Document*` out of the main editor
with `SCI_GETDOCPOINTER` and hands it to an FIP excerpt view with
`SCI_SETDOCPOINTER`. A document from the **vendored fork** cannot be given to
the **stock Scintilla DLL**: different code, different vtables, different heap.

It would not fail loudly. It would corrupt.

So converting `clsDocument`'s editor **forces** the FIP pool and the scratch
window to convert with it. That is a real constraint on 7d's scope, and it was
not in the plan.

The good news is that it is cheap, because of how FIP talks to its windows:

| file | `SciExec` | `CreateWindowEx` | other windowing |
| --- | --- | --- | --- |
| `frmFindInProject.inc` | 97 | 1 | 26 `IsWindow`, 10 `SetFocus`, 4 `SetWindowPos` |
| `modFindProject.inc` | 41 | 1 | 14 `IsWindow` |

## 5. `SciExec` is the lever

    #Define SciExec(h, m, w, l) SendMessage(h, m, w, CAST(LPARAM, l))

Every one of the ~212 external editor calls is a `SendMessage` of an `SCI_*`
message to the HWND. **A host window class that routes `msg >= SCI_START` to
`SciPs_Send` carries all of them unchanged** â€” no call site edited.

That reframes 7d. The measured surface is:

    hWindow(   uses outside clsDocument:      142
    hWndActiveScintilla sites:                 ~90
    SciExec / SciMsg sites:                   ~212
    CreateWindowEx(0, "Scintilla", ...):         4   <-- the actual work

Four creation sites, converted together because of finding 4.

## The window class, and the four sites — done

`src/frmSciHost.bi` / `.inc` registers `tikoSciHost`; per-window state in
`GWLP_USERDATA`; `WM_CREATE` builds the surface, the `PsSciView` and the bridge;
`PSEV_NOTIFY` becomes a synthesised `SCNotification` sent to the parent as
`WM_NOTIFY`. One branch is the whole of finding 5:

```
if (nMsg >= SCI_START) andalso (nMsg < 5000) then
    return SciPs_Send(pSt->pView->pSci, nMsg, wParam, lParam)
```

Four creation sites converted: `clsDocument`'s two, FIP's 16-slot pool, the FIP
scratch. `SciMsg` is `cast(Scintilla_DirectFunction, @SciPs_Send)`. **No other
call site was edited.**

### What the ceiling had to be

`< 4000` was wrong. **Scintilla's lexer messages live above 4000** —
`SCI_SETILEXER` 4033, `SCI_GETLEXER` 4002, `SCI_SETKEYWORDS` 4005. With a 4000
ceiling the editor worked and simply had no lexer, which looks exactly like a
theme that forgot its colours.

### Six bugs the gates caught, in order

1. **Synthetic key messages carry no scancode** — `lParam = 0`. The headless
   suite's own helper always built one, so it could not see this.
2. **The surface had no focus.** `PsSurface` routes keys to `pFocus` and drops
   them when there is none.
3. **The view was not the root.** `PsSurface.Resize` sizes only the root, so a
   view under a bare `PsWidget` kept bounds `0x0` and painted nothing — a blank
   black pane. The pixel test passed throughout, because it called
   `SciPs_PaintTo` directly and so called `SciPs_SetSize` directly too.
4. **The lexer messages** (above).
5. **`LogPixelsY` hardcoded to 96**, so a DPI-aware host rendered small.
6. **The class name was a narrow literal** in a Unicode build.

## Verification

- **All 27 oracle suites identical** between the pre-7d build and this one,
  paired, with state snapshot and restored.
- `_check_scihost.bat` — 26 assertions including an **A/B against a stock
  Scintilla window in the same process**: same styles, same lexer, matching
  line height (within 1px) and matching style bytes.
- `_check_package.bat` — tiko runs with **only the Windows directories on
  `PATH`**.
- Confirmed by hand: rendering, typing, caret, syntax colouring, font size.

## Packaging — done

`_package.bat` stages five DLLs beside the exe; they are committed, as
`Scintilla64.dll` and `Lexilla64.dll` already were. `_run_tiko.bat` is deleted —
nothing needs to be on `PATH` any more.

The five are the transitive import closure of `tiko.exe` minus the Windows
system DLLs, from `objdump -p`: the fork, Blend2D, FreeType, HarfBuzz and
**`libwinpthread-1.dll`**. Not SDL3 — the closure confirms what the bridge was
for — and not the four harfbuzz variants.

The earlier `0xC0000139` was an **incomplete copy, not a broken one**.
`libwinpthread-1.dll` lives only in `build\out\win64` while the other four are
also in `deps\out\win64in`; copying from the deps directory left it out, the
loader found another mingw's copy on `PATH`, and the ABI mismatch reported a
missing *entry point* rather than a missing *file*.

## Not verified

- **Untried by hand:** selection, autocompletion, the context menu, scrolling,
  the split view, and the Find-in-Project excerpt panes.
- **Teardown is not asserted.** The one mutation that survived; the obvious
  assertion was vacuous and was deleted rather than reworded.
- **`PsWin32Host` and `namespace PsC` remain scaffolding**, due for deletion at
  7c along with `PsCompat.bi`.
