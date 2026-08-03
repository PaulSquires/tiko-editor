# Screenshots still needed

13 placeholder figures are live in the built help. They cover **12 unique images** — the
Options ▸ Compiler page appears on two pages and reuses one file.

Everything goes in **`assets/img/`**, alongside the two real captures already there
(`tiko_dark.png`, `tiko_light.png`, both 2150×1478).

---

## The list

Use these exact filenames — the build wiring expects them.

| # | Page | Filename | Capture | Crop to |
|---|------|----------|---------|---------|
| 1 | `toolbar` | `panel-icon-strip.png` | The icon strip across the top of the side panel, showing the left group (Explorer, Functions, Bookmarks, Settings) and the right group (Find in Project, Save All, Debug, Compile, Build). | Tight — the strip plus a few px of the panel below |
| 2 | `status-bar` | `status-bar.png` | The status bar with all fields visible: line/column, insert mode, encoding, line endings, language, build configuration. | Tight — the bar only, full window width |
| 3 | `side-panels` | `explorer-panel.png` | The Explorer with all five category headers (Main, Resource, Header, Module, Normal) and several files. | The side panel column only |
| 4 | `project-explorer` | `explorer-tree.png` | The Explorer on an **untitled** workspace, so the pinned *Save as Project…* row shows. Include at least one user-created folder under Module or Header. | The side panel column only |
| 5 | `find-replace` | `find-bar.png` | The Find bar open, with the search field and every option toggle visible. | Tight — the bar, plus a little editor above/below |
| 6 | `find-in-project` | `find-in-project.png` | The Find in Project dialog, showing its options and file-filter fields. | The dialog window only |
| 7 | `compiler-setup` + `quick-start` | `options-compiler.png` | Options ▸ Compiler page with every field populated. One file, used twice. | The dialog window only |
| 8 | `project-options` | `project-options.png` | Project Options showing all three sections (Project, Compiler options, Build output). | The dialog window only |
| 9 | `build-configurations` | `build-configurations.png` | The dialog with the configuration list on the left, the General page, and the compiler-switch checklist. | The dialog window only |
| 10 | `keyboard-customization` | `assign-shortcut.png` | The Assign Shortcut dialog: capture field, Ctrl/Alt/Shift switches, key list. Ideally mid-conflict so the refusal message shows. | The dialog window only |
| 11 | `user-tools` | `user-tools.png` | User Tools with at least two tools defined, showing the list and the detail fields. | The dialog window only |
| 12 | `theme-editor` | `theme-editor.png` | The theme editor with the key list and the colour picker open (Web tab, R/G/B/A rows visible). | The dialog, including the open picker |

---

## Size and resolution

**It matters, but the tolerance is wide and your current captures already clear it.**

The rule: every figure is displayed at **up to 820 CSS pixels wide** (the content column).
For crisp text on a high-DPI screen it needs **twice that in real pixels**.

| | Value |
|---|---|
| Display width in the page | 820 CSS px max |
| **Target capture width** | **≥ 1640 px** |
| Comfortable range | 1600 – 2600 px wide |
| Existing captures | 2150 px — ideal, don't change the approach |
| Too small | under ~1200 px wide: text goes soft |

Height does not matter — it's whatever the thing you're capturing is.

### The practical rules

1. **Capture at your native DPI and don't resize.** Your display gives you ~2× for free.
   That's the whole trick.
2. **Never upscale.** Enlarging a small capture looks worse than a small capture.
3. **Crop tight to the subject.** This is the one that changes legibility most. A dialog
   cropped to its own edges renders far larger — and more readable — than the same dialog
   sitting inside a full-desktop screenshot, because the page scales whatever you give it
   to the same 820 px column. Never include the desktop or other windows.
4. **PNG, not JPEG.** UI text needs lossless; JPEG puts ringing around glyphs.
5. **Pick one theme and stay with it.** `default_dark` matches the hero image on the home
   page. The help site has its own light/dark toggle, but the screenshots don't follow it —
   consistency between images matters more than matching the reader's theme.
6. **File size**: 150–400 KB each is normal at these dimensions. All 12 will add roughly
   2–3 MB, which is fine for an offline folder.

### Capture checklist

- Real code on screen, not `Untitled` — a FreeBASIC file that looks like actual work
- No personal paths in title bars or fields if the docs may be published
- Dialogs filled in with plausible values, not left empty
- Nothing mid-hover or mid-drag unless the figure is specifically about that state

---

## Where they go, and wiring them up

Drop the files here:

```
C:\dev\tikohelp\assets\img\
```

They are referenced from the pages as `assets/img/<filename>.png`.

**Dropping the files in is not enough on its own** — the HTML is generated. Each placeholder
is a `placeholder(...)` call in `tools/content_*.py` that has to become a `figure_img(...)`
call, then:

```bash
cd C:\dev\tikohelp\tools && python build.py
```

Two ways to do that:

- **Manual** — swap each call yourself. `figure_img("assets/img/status-bar.png", "caption
  text")` replaces `placeholder("Status bar", "...", caption="...")`.
- **Automatic** — the build can be taught to use `assets/img/<expected-name>.png` when the
  file exists and fall back to the placeholder when it doesn't. Then adding a screenshot is
  just dropping the file in and rebuilding, and partial progress renders correctly.

The second is a small change to `build.py` and the 13 call sites. Say the word and it's done.

---

## Not the same thing: the 23 TODO callouts

Separate from these figures, 23 purple TODO boxes mark **behaviour** I could not confirm
from the source — exact field labels, the regex dialect, parameter substitution codes. Those
need answers in text, not pictures. `grep -l 'callout todo' *.html` lists the pages.
