# Themes

How colour works in tiko, and how to change it.

---

## The short version

A theme is a plain text file in `settings\themes` with a `.theme` extension. Fourteen ship with
the editor. You switch, clone and edit them from **Settings → Themes…**, or by clicking the pen
icon in the status bar.

Every colour in the application — the editor, the panels, the menus, the dialogs, the tooltips —
comes from the active theme. There are no hard-coded colours.

---

## The two layers

This is the one concept worth understanding, because everything else follows from it.

**Layer 1 — the palette.** Twenty-one named *roles*: `background`, `foreground`, `border`,
`accent`, `keyword`, `comment`, `string`, and so on. These say what a colour *means*.

**Layer 2 — the keys.** About seventy individual settings: `editor.keyword1`, `panel.texthot`,
`tooltip.border`, `statusbar.text`. These say where a colour *goes*.

Every key falls back to a role. A key only carries a colour of its own if you give it one:

```
the key itself   →   its role   →   tiko's built-in default
```

So a theme that defines nothing but the palette is complete and valid — that is how eleven of the
shipped themes are written, in about 35 lines each. It also means a setting added to tiko in a
future version is themed correctly in your theme automatically, instead of arriving black until
you edit the file.

**Practical consequence:** to restyle the whole editor, edit the palette. To depart from the
palette in one place, edit that key.

---

## The Themes dialog

**Settings → Themes…** (or the pen icon in the status bar).

### Picking a theme

The list shows every `.theme` file, its description, and which one is active. Select one and:

| | |
|---|---|
| **Set as active** | use this theme |
| **Edit** | open it in the colour editor |
| **Clone** | copy it under a new name |
| **Delete** | remove the file |
| **Close** | leave the dialog |

`default_dark` and `default_light` are **protected**: they cannot be edited or deleted. Clone one
first. If you try to edit a protected theme, tiko offers to fork it for you and edits the copy.

### Editing a theme

The editor has three tabs.

#### Palette

The twenty-one roles. Select a row and either click **Choose Color…**, click the swatch, or type
into the **R/G/B** boxes.

Editing a role moves **every key that inherits it** — which is the fast way to retheme.

A role marked *default* is one this theme has not chosen; it is using tiko's built-in value. Give
it a colour and the marker goes away.

#### Syntax Colors / Interface Colors

The individual keys — syntax styles on one tab, everything else on the other.

Each row shows the key name, then for each channel the **role it is inheriting** (in grey) beside
its colour swatch:

```
editor.keyword1        keyword  [FG][BG]  background
```

A channel with no role shown has been given a colour of its own. That is the whole two-layer model
on one line: grey word = inherited, no word = overridden.

Select a row to edit it. **Foreground…** and **Background…** open the colour picker; each has its
own **R/G/B** row beneath it, and its own **↺** button to discard the override and go back to
inheriting the role.

Syntax keys also have **Bold / Italic / Underline** toggles and, where it applies, **Opacity**.

### Saving

Changes apply live, so you see them on the real editor as you work.

| | |
|---|---|
| **Save** | write the changes to the `.theme` file |
| **Revert** | discard them and go back to the file on disk |
| **Back** | return to the theme list |

Editing a protected theme forks it to a new file first — you will be asked before anything
changes.

---

## Editing a `.theme` file by hand

The files are plain UTF-8 text and tiko re-reads them on load. The format:

```
general.description: My Theme
general.appearance: dark          # dark or light -- drives the window title bar

# the palette
role.background: 40,44,52
role.foreground: 171,178,191
role.accent: 62,68,81
role.keyword: 198,120,221

# a key that departs from the palette
editor.comments.foreground: 106,115,131
editor.comments.italic: true
```

- Colours are `red,green,blue`, each 0–255. A fourth number is alpha, where the setting supports
  it (selection and occurrence highlights).
- `#` starts a comment.
- A key is `namespace.key.channel` — channel is `foreground`, `background`, `bold`, `italic` or
  `underline`.
- Anything you leave out inherits. **A file containing only `general.*` and `role.*` is a complete
  theme.**

Save the file, then re-select the theme in the dialog to reload it.

> **Note on `%MACRO`.** Older themes could define named colours as `%NAME: r,g,b`. That has been
> replaced by the role palette, which does the same job and is inherited from. Existing files
> using `%MACRO` still load correctly; the first time you save one through the dialog it is
> converted. There is no reason to write new ones.

---

## The shipped themes

| File | Description |
|---|---|
| `default_dark`, `default_light` | tiko's own — protected |
| `studio_dark`, `studio_light` | after VS Code Dark+ / Light+ |
| `slate_dark`, `slate_light` | after One Dark / One Light |
| `sepia_dark`, `sepia_light` | after Solarized |
| `arctic` | after Nord |
| `retro_warm` | after Gruvbox |
| `midnight` | after Tokyo Night |
| `neon` | after Monokai |
| `contrast_dark`, `contrast_light` | high contrast |

Each is re-implemented from published colour values; the original project and its licence are
credited in the file header.

**Only `default_dark` and `default_light` are protected.** The other twelve can be edited in
place, and the only way back to the shipped version is to reinstall or restore the file from
source control — so clone before experimenting.
