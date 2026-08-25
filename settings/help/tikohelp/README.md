# Tiko Editor — Help System

A complete offline documentation website for the Tiko Editor. Open `index.html` in any
modern browser. No server, no build tools, no external requests.

## Using it

Just open `index.html`. Everything works from `file://` — search included, because the
search index is loaded as a plain `<script>` rather than fetched.

- `Ctrl+K` or `/` opens search
- The theme follows the OS until you choose one with the header button
- Every page prints cleanly (navigation, sidebar and buttons are suppressed)

## Layout

```
index.html            Home page
*.html                68 topic pages, flat, one per topic
assets/
    styles.css        All styling, including light/dark themes and print rules
    script.js         Theme, nav, search, TOC, syntax highlighting, copy buttons
    search-index.js   GENERATED — do not edit
    img/              Screenshots
tools/
    build.py          The generator
    content_*.py      The documentation content
```

The pages are generated. **Edit the content modules, not the HTML.**

## Rebuilding

```
cd tools
python build.py
```

Requires Python 3 and nothing else. It rewrites every `.html` file and
`assets/search-index.js`.

## Adding a page

Content lives in `tools/content_*.py`. Each module registers sections and pages against
the generator, which supplies the shell — sidebar, breadcrumbs, previous/next, on-page
table of contents, search entry.

```python
from build import section, page, h2, p, ul, code, table, note, tip, kbd, menu

BODY  = h2("Overview")
BODY += p("What this feature does and why it is useful.")
BODY += tip("Something worth knowing.")
BODY += code('Print "hello"', lang="fb", title="example.bas")

page("my-slug", "My page title", "editing",     # section id
     "One-sentence summary shown under the heading and in search results.",
     BODY,
     keywords="extra search terms not in the body")
```

Page order within a section, and the previous/next reading order, follow registration
order. Import order of the modules in `build.py`'s `main()` sets section order.

### Authoring helpers

`h2 h3 h4 p ul ol dl code table figure_img placeholder diagram cards faq dl`
plus `note tip warn important todo` for callouts and `kbd menu ui` for inline
UI conventions. `code(..., lang=)` accepts `fb`, `c`, `ini` and `text`.

### Marking unknowns

Anything not verified against the shipping build is marked with a `todo(...)` callout,
which renders as a distinct purple box. There are currently **27** of them and **13**
placeholder figures. Search the built site for `callout todo` to find them all:

```
grep -l 'callout todo' *.html
```

## What is verified

Menus, commands, default keyboard shortcuts and `settings.ini` keys were taken from the
Tiko Editor source (`modMenuDefinitions.inc`, `modKeyBindings.inc`, `settings/settings.ini`) and
are accurate. Behavioural details that could not be confirmed from source are marked with
TODO callouts rather than guessed at.
