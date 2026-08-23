# tiko

**A programmer's code editor for the FreeBASIC compiler.**

![tiko editor dark mode](/docs/screenshots/tiko_dark.png)

tiko is a native Win32 desktop application written in FreeBASIC. No .NET, no
Electron, no cross-platform abstraction layer — it talks to the Windows API
directly. It starts instantly, and it is the editor its own source code is
written in.

It ships as a complete package: the editor, the FreeBASIC compiler toolchain
(32- and 64-bit), José Roca's AfxNova library, the include files, and the
documentation. Unpack it and you are building.

tiko is the successor to [WinFBE](https://github.com/PaulSquires/WinFBE).

---

## Code intelligence from the actual compiler

Most editors understand your code through regular expressions and heuristics.
tiko doesn't.

Its symbol engine is the FreeBASIC compiler's own front end — lexer,
preprocessor, parser — built as a library with the code generator removed behind
a null IR backend. When tiko resolves a symbol, the thing answering is the same
parser that compiles the file. It sees what `fbc` sees, including declarations
behind `#if`, because the preprocessor and the expression parser are the real ones.

It is fast enough to do this continuously: `windows.bi` yields roughly 33,000
symbols in about 0.4 seconds. Unsaved buffers are parsed in memory, so
intelligence never lags behind what you just typed.

This drives code tips, autocompletion, Go To Definition, the Functions and TODO
panels, and project-wide symbol search.

## A debugger that debugs the binary you built

`fbc` emits debug information in a private stabs derivative that GDB cannot
read. Every FreeBASIC editor has worked around this the same way — quietly
switching the build to a different code generator when you press F5, so the
program you debug is not the program you built.

tiko's debug engine reads that format directly. Debug builds stay on the backend
your project actually uses. Beyond removing the lie, that also makes debug builds
several times faster to produce.

- Breakpoints, stepping, pause, and a live call stack
- Watch expressions and hover datatips
- Four docked panes — globals, locals, call stack, watches — with persisted layout
- UDTs expand as a real tree; arrays group adaptively, so a 10,000-element array
  is 224 rows instead of 10,000
- Values that changed since the last stop are highlighted
- One 64-bit engine debugs both 32- and 64-bit targets

## Editing

- Scintilla / Lexilla core, Unicode throughout, 64-bit, per-monitor DPI aware
- Configurable code formatter with keyword casing driven by the same keyword
  files that drive syntax highlighting — the editor and the formatter cannot
  disagree about how a keyword is spelled
- Auto-indent and block completion: type `If` and press Enter, get the `End If`
- Split editing, bookmarks, navigation history, brace and occurrence highlighting
- F1 on any symbol opens the bundled reference

## Projects and building

- Every workspace is a project; loose files are just an untitled one, and your
  last session always comes back
- Incremental compilation of code marked as a module
- Named build configurations with their own compiler switches and accelerators
- Windows resource files by simply naming the source file that is the resource
- Console and GUI targets, 32- or 64-bit, from the same project
- User-defined external tools

## Making it yours

The entire user interface is owner-drawn — a family of twenty-five custom
controls built for this application rather than inherited from comctl32. That
means every surface is themeable, not just the text.

- Full theme editor with a built-in colour picker; dark and light supplied
- Every keyboard shortcut is rebindable, with conflict detection
- Localized in English, French, Spanish, German, Norwegian, and Chinese
  (Simplified); adding a language is copying one text file

![tiko editor light mode](/docs/screenshots/tiko_light.png)

---

## Requirements

Windows 10 or later. The source uses modern Windows APIs and does not target
anything earlier.

## Building tiko

tiko builds with its own bundled toolchain and its own bundled copy of AfxNova.

```
build\_compile_fast.bat
```

Run the resulting `tiko-editor.exe` from the project root, not from `src\` — it
resolves settings, themes, keywords, and help relative to its own directory.

---

## License

**The editor is free software.** Copyright © 2016-2026 Paul Squires,
PlanetSquires Software, licensed under the **GNU General Public License version
3 or later** ([LICENSE](LICENSE)). That covers the entire published source tree.

Third-party components retain their own licenses: Scintilla and Lexilla
(Neil Hodgson), AfxNova (José Roca), and the FreeBASIC compiler and runtime.
