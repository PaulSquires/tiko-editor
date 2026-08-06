# 7d: the footing, measured

The Win32 host bridge landed in PsPlatform (`src/platform/win32host/`), which
breaks the cycle 7d ran into. This is what measuring the ground under 7d turned
up before writing the window class.

Gate: `_check_scihost.bat` — 10 assertions, green.

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
references, **at link time only** — the compile is clean.

`PsEncoding` is pure FreeBASIC, so the encoder never hit this. `PsSciView` pulls
in Blend2D, FreeType, HarfBuzz and the vendored Scintilla fork, and every one of
those is an `extern "C"` surface.

The fix is ordering, not redesign: hoist `bind/Blend2D.bi`, `bind/FreeType.bi`,
`bind/HarfBuzz.bi` and `scintilla/PsScintilla.bi` to **global scope** before
opening the namespace. `#include once` then makes the inner includes no-ops, and
the FreeBASIC code — which is what needs PsCore's `DWSTRING` — stays inside.

## 3. The bridge could not hand-declare its own GDI imports

`PsWin32Host` declared `CreateCompatibleDC` and friends itself, to avoid
dragging `windows.bi` into every host. Against a host that *already has*
`windows.bi` that emits the same C symbol with a different type —
`void* CreateCompatibleDC(void*)` against `HDC__* CreateCompatibleDC(HDC__*)` —
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
`SciPs_Send` carries all of them unchanged** — no call site edited.

That reframes 7d. The measured surface is:

    hWindow(   uses outside clsDocument:      142
    hWndActiveScintilla sites:                 ~90
    SciExec / SciMsg sites:                   ~212
    CreateWindowEx(0, "Scintilla", ...):         4   <-- the actual work

Four creation sites, converted together because of finding 4.

## What is next

A `tikoSciHost` window class:

- registers a class; per-window state in `GWLP_USERDATA`
- `WM_CREATE` builds the surface, the `PsSciView` and the bridge
- `msg >= SCI_START` → `SciPs_Send` (this is finding 5)
- `WM_PAINT` / `WM_SIZE` / input → the bridge
- `PSEV_NOTIFY` → a synthesised `SCNotification` sent to the parent as
  `WM_NOTIFY`, so `frmMainOnNotify`'s path is untouched

Then the four creation sites, and a **paired** self-test run — the oracle is 27
stable suites out of 28, and an absolute baseline is the wrong instrument.

## Not verified

- **tiko's own build has not changed yet.** `_compile_fast.bat` does not yet
  carry `-p`/`-l psscintilla`, and tiko.exe will need the DLLs on `PATH` or
  beside it. Packaging is untouched.
- **Nothing is wired.** No `PsSciView` exists in a running tiko. Everything
  above is a probe.
- **Copying the DLLs next to the exe does not work** — an incomplete copied set
  fails at load with `0xC0000139` naming no useful entry point. The gate points
  `PATH` at the real directories instead; tiko will have to solve this properly.
