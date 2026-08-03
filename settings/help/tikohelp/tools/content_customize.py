# -*- coding: utf-8 -*-
"""Customization section."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   figure_img, placeholder, diagram)

section("customize", "Customization", "sliders")

# ==========================================================================
# Themes
# ==========================================================================

TH = ""
TH += p(
    "A theme sets every colour in Tiko — the editor's syntax colours, and the panels, "
    "menus, dialogs, tabs and status bar as well. Changing theme changes the whole "
    "application, not just the code area."
)

TH += h2("Choosing a theme")
TH += ol([
    "Open %s, or press %s."
    % (menu("File", "Settings", "Themes…"), kbd("Ctrl", "Shift", "T")),
    "Pick a theme from the list. ",
    "Choose <strong>OK</strong>. The interface redraws immediately.",
], steps=True)

TH += h2("The bundled themes")
TH += p("Tiko ships fourteen themes, in matched light and dark families:")
TH += table(
    ["Theme", "Character"],
    [
        ("<code>default_dark</code> / <code>default_light</code>",
         "Tiko's own palette. The starting point."),
        ("<code>studio_dark</code> / <code>studio_light</code>",
         "Familiar to anyone coming from a mainstream code editor."),
        ("<code>slate_dark</code> / <code>slate_light</code>",
         "Cool blue-grey, moderate contrast."),
        ("<code>sepia_dark</code> / <code>sepia_light</code>",
         "Warm, low-blue tones — easy on the eyes over long sessions."),
        ("<code>contrast_dark</code> / <code>contrast_light</code>",
         "High contrast, for difficult lighting or reduced vision."),
        ("<code>arctic</code>", "A cool, muted light palette."),
        ("<code>retro_warm</code>", "Warm earthy colours."),
        ("<code>midnight</code>", "A deep, saturated dark theme."),
        ("<code>neon</code>", "High-saturation accents on a dark background."),
    ],
    key_first=True,
)
TH += note(
    "Themes are files: each is a <code>.theme</code> file in "
    "<code>settings\\themes</code>. Adding a theme is copying a file into that folder; "
    "sharing one is sending that file to someone."
)

TH += h2("How a theme is built")
TH += p(
    "Tiko's themes work in two layers, which is what keeps them short and internally "
    "consistent."
)
TH += table(
    ["Layer", "What it is"],
    [
        ("Roles", "About twenty semantic slots — background, accent, accent foreground, "
         "and the syntax slots such as keyword, string, comment and number. A role says "
         "what a colour <em>means</em>."),
        ("Keys", "Individual colour settings for specific elements — "
         "<code>editor.divider</code>, <code>panel.text.backcolor</code> and so on. A key "
         "that is not given a value falls back to its role."),
    ],
    key_first=True,
)
TH += p(
    "Because of that fallback, a theme that defines only the roles is complete and valid. "
    "Several of the bundled themes are barely thirty lines long for exactly this reason. "
    "You override individual keys only where you want to depart from what the role implies."
)
TH += important(
    "Two roles are deliberately separate and easy to confuse. <strong>Accent</strong> is a "
    "<em>fill</em> colour — the background of a selected tab or a highlighted row. "
    "<strong>Accent foreground</strong> is <em>ink</em> — text drawn on top of that fill. "
    "Using one where the other belongs produces text that is invisible in one theme and "
    "fine in another."
)

TH += h2("Making a theme your own")
TH += p(
    "Start by <strong>cloning</strong> a theme you like, then edit the copy — see "
    '<a href="theme-editor.html">The theme editor</a>. Two themes, '
    "<code>default_dark</code> and <code>default_light</code>, cannot be edited or deleted "
    "at all; cloning is the way to build on those. The other twelve can be edited in "
    "place, but cloning still keeps your work clear of a file an update might replace."
)

TH += h2("Related topics")
TH += ul([
    '<a href="theme-editor.html">The theme editor</a>',
    '<a href="editor-appearance.html">Fonts and editor appearance</a>',
    '<a href="settings-files.html">Settings and configuration files</a>',
])

page("themes", "Themes", "customize",
     "Choosing among the fourteen bundled colour themes, and how Tiko's two-layer role and "
     "key model makes themes short and consistent.",
     TH,
     keywords="theme themes dark light colour color scheme default_dark studio slate "
              "sepia contrast arctic midnight neon roles keys accent")

# ==========================================================================
# Theme editor
# ==========================================================================

TE = ""
TE += p(
    "The theme editor lets you change any colour in Tiko and see the result immediately. "
    "Open it with %s or %s."
    % (menu("File", "Settings", "Themes…"), kbd("Ctrl", "Shift", "T"))
)

TE += h2("The palette page")
TE += p(
    "The first page shows the theme's <strong>roles</strong> — the semantic colours "
    "everything else inherits from. Changing a role changes every element that has not "
    "explicitly overridden it, which is the efficient way to retune a theme: adjust a "
    "dozen roles rather than a hundred keys."
)
TE += p(
    "A role the current theme has not chosen for itself is marked as using its built-in "
    "default, so you can see at a glance how much of the theme is genuinely authored."
)

TE += h2("The key pages")
TE += p(
    "The remaining pages group individual colour keys by the part of the interface they "
    "affect — the editor, the panels, the tabs, the output window, tooltips, dialogs. "
    "Every row shows, per colour channel, which role it currently inherits from, so you "
    "always know what a value will fall back to if you clear it."
)
TE += figure_img(
    "assets/img/theme-editor.png",
    "The theme editor with the colour picker open. Each row shows the role a colour "
    "inherits from, so you can always see what clearing a value will fall back to.",
    alt="The Tiko theme editor with the colour picker open")

TE += h2("Choosing a colour")
TE += p("Clicking a colour button opens the colour picker, which has three tabs:")
TE += table(
    ["Tab", "Offers"],
    [
        ("Web", "A matrix of tints and shades, swept across hue and lightness."),
        ("System", "The named colours Windows itself defines."),
        ("Custom", "A scrolling list of named colours."),
    ],
    key_first=True,
)
TE += p(
    "Below the matrix, the picker shows the initial and current colours side by side and "
    "offers R, G, B and alpha entry fields for exact values. Double-clicking a swatch "
    "chooses it and closes the picker in one action."
)
TE += note(
    "The picker is modal: the colour is applied when you choose <strong>OK</strong>, and "
    "cancelling leaves the previous colour untouched."
)

TE += h2("The two views")
TE += p("The Themes dialog has a list view and an editor view.")
TE += table(
    ["View", "Shows"],
    [
        ("List", "Every theme in three columns — <strong>Theme</strong>, "
         "<strong>Description</strong> and <strong>Active</strong> — with "
         "<strong>Edit</strong>, <strong>Clone</strong>, <strong>Delete</strong> and "
         "<strong>Set as active</strong> beside it, and <strong>Close</strong> below."),
        ("Editor", "The palette and key pages for one theme, with its description in the "
         "header and a <strong>Back</strong> button to return to the list."),
    ],
    key_first=True,
)
TE += note(
    "<strong>Edit does not activate a theme.</strong> You can edit one while continuing to "
    "work in another; use <strong>Set as active</strong> when you want to switch to it."
)

TE += h2("Making your own theme")
TE += p("The intended route is to clone an existing theme and edit the copy.")
TE += ol([
    "Select the theme you want to start from and click <strong>Clone</strong>.",
    "Give the copy a file name. Tiko adds the <code>.theme</code> extension if you leave "
    "it off, and will not let you overwrite the theme you are cloning.",
    "The copy's description is tagged <strong>(cloned)</strong> so you can tell it apart "
    "in the list.",
    "Select the copy and click <strong>Edit</strong>.",
    "Change whatever you like. Click the <strong>pencil icon</strong> beside the "
    "description in the header to give the theme a proper description of its own.",
    "Click <strong>Back</strong> when you are done, then <strong>Set as active</strong> "
    "to start using it.",
], steps=True)

TE += h3("The two themes you cannot edit")
TE += important(
    "<strong><code>default_dark</code> and <code>default_light</code> are protected.</strong> "
    "The <strong>Edit</strong> and <strong>Delete</strong> buttons are disabled while "
    "either is selected — they are the fallback the editor always has to be able to fall "
    "back to. <strong>Clone</strong> and <strong>Set as active</strong> stay available, so "
    "cloning is how you build on them."
)
TE += p(
    "Every other theme is fully editable and deletable, including the twelve other themes "
    "that ship with Tiko — <code>studio_*</code>, <code>slate_*</code>, "
    "<code>midnight</code>, <code>neon</code> and the rest. Protection matches the "
    "<code>default_</code> prefix only, so a theme of your own called "
    "<code>my_default_x.theme</code> is not protected."
)
TE += tip(
    "Even though the other shipped themes can be edited in place, clone before you "
    "customise one. An update may replace the file it ships, and a clone is out of that "
    "file's way."
)

TE += h3("Deleting themes")
TE += p(
    "<strong>Delete</strong> removes the selected theme's file. If you ever delete your "
    "way down to no themes at all, Tiko restores <code>default_dark.theme</code> from a "
    "backup copy it keeps — you cannot end up with an editor that has no theme to load."
)

TE += h2("Practical advice")
TE += ul([
    "<strong>Change roles first.</strong> Most of what looks wrong about a theme is a role, "
    "not a key.",
    "<strong>Check both text and background.</strong> A colour that reads well on the "
    "editor background may be illegible on a panel.",
    "<strong>Look at the hover and selection states.</strong> These are two backgrounds "
    "compared with each other rather than ink against a background, and they are the "
    "easiest thing to get wrong — a hover colour barely distinguishable from the row "
    "beneath it looks broken without looking obviously wrong.",
    "<strong>Test in the real interface</strong>, not just the preview: open a menu, a "
    "dialog and the Output panel before deciding you are finished.",
])

TE += h2("Related topics")
TE += ul([
    '<a href="themes.html">Themes</a>',
    '<a href="syntax-highlighting.html">Syntax highlighting</a>',
    '<a href="dialog-reference.html#themes">Themes dialog reference</a>',
])

page("theme-editor", "The theme editor", "customize",
     "Editing colours: the palette of semantic roles, the per-element key pages, and the "
     "colour picker.",
     TE,
     keywords="theme editor colour picker color picker roles palette keys rgb alpha web "
              "system custom swatch save theme")

# ==========================================================================
# Fonts / appearance
# ==========================================================================

FA = ""
FA += p(
    "Font settings live in %s. They apply to the editing surface; "
    "the rest of the interface uses the system font."
    % menu("File", "Settings", "Options…")
)

FA += h2("Editor font")
FA += dl([
    ("Font name", "The typeface used in the editor. The default is <strong>Consolas</strong>. "
     "Choose from the scrolling list, which previews each font in its own face."),
    ("Font size", "In points. The default is 11."),
    ("Character set", "The character set the font is requested with. Leave this at its "
     "default unless you are working with a script that needs a specific one."),
    ("Extra line spacing", "Additional space between lines, in pixels. A small amount — "
     "the default is 2 — makes dense code noticeably easier to read."),
])
FA += important(
    "Use a <strong>monospaced</strong> font. Code alignment, indent guides, the "
    "right-edge marker and column selection all assume every character is the same width. "
    "Consolas, Cascadia Mono, DejaVu Sans Mono and Courier New all qualify."
)
FA += tip(
    "If your font has a version with programming ligatures, it can make operators easier "
    "to read — but check that it does not disturb column alignment before adopting it for "
    "real work."
)

FA += h2("Zoom versus font size")
FA += p(
    "%s and %s zoom the display temporarily; %s resets it. The configured font size is "
    "unchanged, which is why zoom does not persist between sessions and the font setting "
    "does." % (kbd("Ctrl", "+"), kbd("Ctrl", "-"), kbd("Ctrl", "0"))
)

FA += h2("Interface density")
FA += ul([
    "<strong>Compact menus</strong> draws menu rows at a tighter height.",
    "Panel and Output panel sizes are set by dragging their splitters, and are remembered.",
    "The Explorer can sit on either side of the window.",
])

FA += h2("High-DPI displays")
FA += p(
    "Tiko scales its interface to the display's DPI setting, so it stays legible on "
    "high-resolution screens. Colours and fonts behave the same at any scale."
)

FA += h2("Related topics")
FA += ul([
    '<a href="themes.html">Themes</a>',
    '<a href="view-options.html">Display and view options</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("editor-appearance", "Fonts and editor appearance", "customize",
     "Choosing the editor font, size, character set and line spacing, and how zoom differs "
     "from the configured size.",
     FA,
     keywords="font editor font consolas font size character set charset line spacing "
              "extra space monospace zoom dpi compact menus")

# ==========================================================================
# Keyboard customization
# ==========================================================================

KC = ""
KC += p(
    "Every command in Tiko can be rebound. Open the Keyboard Shortcuts dialog with "
    "%s or %s."
    % (menu("File", "Settings", "Keyboard Shortcuts…"), kbd("Ctrl", "K"))
)

KC += h2("The dialog")
KC += p(
    "The dialog lists every command with four columns — the category, the command, its "
    "current shortcut and its description — above a filter box that searches all four. "
    "Type part of a command name or a key name to narrow the list."
)
KC += table(
    ["Button", "What it does"],
    [
        ("Modify", "Opens the assignment dialog for the selected command."),
        ("Reset", "Restores the selected command to its default shortcut."),
        ("OK", "Saves every change and rebuilds the accelerator table."),
        ("Cancel", "Discards every change. Nothing you did in the dialog takes effect."),
    ],
    key_first=True,
)
KC += note(
    "The dialog works on a copy of your bindings, so <strong>Cancel really cancels</strong> "
    "— you can experiment freely and back out."
)

KC += h2("Assigning a shortcut")
KC += ol([
    "Select the command and choose <strong>Modify</strong>.",
    "Either <strong>press the keystroke</strong> you want in the capture field, or build "
    "it by hand: toggle the Ctrl, Alt and Shift switches and pick the key from the list.",
    "If the keystroke is already taken, the dialog says so and refuses it rather than "
    "silently stealing it from the other command.",
    "Choose <strong>OK</strong> in the assignment dialog, then <strong>OK</strong> in the "
    "main dialog to commit.",
], steps=True)
KC += figure_img(
    "assets/img/assign-shortcut.png",
    "The shortcut assignment dialog. Press the keystroke you want in the capture field, "
    "or build it from the modifier switches and the key list.",
    alt="The Assign Shortcut dialog")

KC += h2("Conflicts")
KC += p(
    "A keystroke can only do one thing. Tiko checks for conflicts across editor commands, "
    "user tools and build configuration shortcuts, and marks any conflict it finds in the "
    "list."
)
KC += p(
    "Where a conflict is inherited from an older settings file, the first command to claim "
    "the keystroke keeps it and the others show as unassigned. Reassign the loser to "
    "something free."
)

KC += h2("Disabling a default")
KC += p(
    "A default shortcut can be suppressed without being replaced — useful when a "
    "keystroke conflicts with something outside Tiko, such as a global hotkey from another "
    "application. A suppressed default shows in the list marked as disabled."
)

KC += h2("Where bindings are stored")
KC += p(
    "Your changes go into <code>settings\\keybindings.ini</code>. Only your "
    "<em>overrides</em> are stored — the defaults live in the program, so the file stays "
    "small and readable:"
)
KC += code("""
IDM_FILESAVE:Ctrl+S
IDM_DUPLICATELINE:Ctrl+Shift+D
IDM_BOOKMARKTOGGLE(DISABLED):
""", lang="ini", title="settings\\keybindings.ini", numbered=False)
KC += p(
    "Each line names a command, then the keystroke you assigned. A "
    "<code>(DISABLED)</code> marker after the command name means its default is "
    "suppressed. Deleting the file restores every default."
)
KC += tip(
    "Copy <code>keybindings.ini</code> to another machine to take your shortcuts with you. "
    'See <a href="settings-files.html">Settings and configuration files</a>.'
)

KC += h2("Related topics")
KC += ul([
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts reference</a>',
    '<a href="user-tools.html">User tools</a>',
    '<a href="build-configurations.html">Build configurations</a>',
])

page("keyboard-customization", "Customizing keyboard shortcuts", "customize",
     "Rebinding any command, resolving conflicts, disabling a default, and where your "
     "bindings are stored.",
     KC,
     keywords="keyboard shortcuts customize rebind remap assign key conflict disabled "
              "default keybindings.ini reset accelerator")

# ==========================================================================
# User tools
# ==========================================================================

UT = ""
UT += p(
    "A user tool is an external program you can run from inside Tiko — a formatter, a "
    "version-control command, a packaging script, a documentation generator. Tools appear "
    "on %s and can have their own keyboard shortcuts."
    % menu("File", "User Tools")
)

UT += h2("Defining a tool")
UT += ol([
    "Open %s." % menu("File", "User Tools…"),
    "Choose <strong>Add</strong>.",
    "Fill in the description, the program to run, its parameters and its working folder.",
    "Optionally assign a keyboard shortcut.",
    "Choose <strong>OK</strong>.",
], steps=True)
UT += table(
    ["Field", "Purpose"],
    [
        ("Description", "The text shown on the User Tools menu."),
        ("Program", "The executable to run."),
        ("Parameters", "The command line passed to it. Substitution codes let you pass "
         "the current file, project or selection — see below."),
        ("Working folder", "The directory the program starts in."),
        ("Shortcut", "An optional accelerator."),
    ],
    key_first=True,
)
UT += figure_img(
    "assets/img/user-tools.png",
    "The User Tools dialog. Tools appear on the User Tools menu in the order shown here.",
    alt="The User Tools dialog")

UT += h2("Parameter substitution codes")
UT += p(
    "The <strong>Parameters</strong> field understands four codes. Each is replaced with a "
    "value from your current workspace when the tool runs. Hover the field in the dialog "
    "and its tooltip lists them too."
)
UT += table(
    ["Code", "Replaced with", "Notes"],
    [
        ("<code>&lt;P&gt;</code>", "The project name",
         "Empty on an untitled workspace — there is no name to substitute."),
        ("<code>&lt;S&gt;</code>", "The main source file",
         "Supplied already quoted, so a path containing spaces works without you adding "
         "quotation marks."),
        ("<code>&lt;W&gt;</code>", "The word at the caret",
         "Surrounding punctuation is stripped, so you get the identifier rather than the "
         "bracket beside it."),
        ("<code>&lt;E&gt;</code>", "The compiled EXE, DLL or LIB",
         "The output name from the active build configuration."),
    ],
    key_first=True,
)
UT += note(
    "The codes are case-insensitive — <code>&lt;p&gt;</code> works exactly as "
    "<code>&lt;P&gt;</code> does."
)
UT += h3("Examples")
UT += table(
    ["To do this", "Program", "Parameters"],
    [
        ("Show the built program in Explorer", "<code>explorer.exe</code>",
         "<code>/select,&lt;E&gt;</code>"),
        ("Open the main source in another editor", "your editor",
         "<code>&lt;S&gt;</code>"),
        ("Look up the word at the caret on the web", "<code>explorer.exe</code>",
         "a search URL ending <code>&lt;W&gt;</code>"),
    ],
    key_first=True,
)
UT += important(
    "There is <strong>no code for the current file</strong> or for a folder path. "
    "<code>&lt;S&gt;</code> gives the project's <em>main</em> source file, not whichever "
    "document you happen to be looking at."
)
UT += h2("Ordering and shortcuts")
UT += p(
    "Reorder tools with the up and down chevrons — the order in the list is the order on "
    "the menu. A tool's shortcut is checked against every editor command and every other "
    "tool, and one that would conflict is refused rather than silently overriding."
)

UT += h2("Ideas for tools")
UT += ul([
    "Run <code>git status</code> or <code>git diff</code> on the current project folder.",
    "Open the project folder in Explorer.",
    "Run a packaging or deployment batch file.",
    "Run an external linter or static analyser over the current file.",
    "Open the current file in another editor for a specialised task.",
])

UT += h2("Related topics")
UT += ul([
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>',
    '<a href="dialog-reference.html#user-tools">User Tools dialog reference</a>',
])

page("user-tools", "User tools", "customize",
     "Defining external programs you can launch from the menu or a shortcut, with "
     "parameters substituted from the current file or project.",
     UT,
     keywords="user tools external tools run program parameters substitution working "
              "folder shortcut menu")

# ==========================================================================
# Localization
# ==========================================================================

LZ = ""
LZ += p(
    "Tiko's interface text is loaded from a language file, so it can be displayed in "
    "another language — or reworded to your taste."
)

LZ += h2("Choosing a language")
LZ += ol([
    "Open %s." % menu("File", "Settings", "Options…"),
    "Select the <strong>Localization</strong> page.",
    "Select a language from the list and choose <strong>Select</strong>. The marked "
    "language is the active one.",
    "Choose <strong>OK</strong>.",
], steps=True)
LZ += note(
    "A language change takes effect the next time Tiko starts, since the interface is "
    "built from the language file as the program loads."
)

LZ += h2("How language files work")
LZ += p(
    "Each language is a <code>.lang</code> file in <code>settings\\languages</code>. Every "
    "piece of interface text has a numeric identifier, and a language file maps identifiers "
    "to text."
)
LZ += important(
    "<strong><code>english.lang</code> is the canonical file and the fallback for every "
    "other language.</strong> If a translation leaves an entry blank, the English text is "
    "used for that item — so a partial translation produces a mixed interface rather than "
    "blank menus."
)

LZ += h2("Editing a translation")
LZ += p(
    "The Localization page has two modes. Normally it lists the available languages with "
    "commands beside them; choosing <strong>Edit</strong> replaces that list with the "
    "phrase editor for the language you picked, giving the phrase list the whole page."
)
LZ += table(
    ["Command", "What it does"],
    [
        ("Select", "Makes that language the active interface language."),
        ("New", "Creates a new, empty language file."),
        ("Edit", "Opens that language's phrases for editing."),
        ("Delete", "Removes a language file."),
        ("Back", "Returns from the phrase editor to the language list."),
    ],
    key_first=True,
)
LZ += p(
    "In the phrase editor, each row shows the phrase identifier, the English text and your "
    "translation. Edit the translation column in place: double-click it, or press %s or "
    "%s on the row." % (kbd("Enter"), kbd("F2"))
)
LZ += important(
    "<strong>Selecting a language and editing one are different actions.</strong> Opening "
    "French to correct a typo does not switch Tiko into French — only <strong>Select</strong> "
    "does that."
)
LZ += warn(
    "Saving a language file from the editor rewrites the whole file. Comments and "
    "translator attribution at the top of a hand-written <code>.lang</code> file are not "
    "preserved. Keep a copy of any file you care about before editing it here."
)

LZ += h2("Creating a new translation")
LZ += ol([
    "Choose <strong>New</strong> and give the language a name and a file name.",
    "Choose <strong>Edit</strong> and work through the phrase list, filling in translations.",
    "Leave anything you have not translated blank — the English text is used for it.",
    "Choose <strong>Select</strong> to make it active, then restart Tiko.",
], steps=True)

LZ += h2("Related topics")
LZ += ul([
    '<a href="settings-files.html">Settings and configuration files</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("localization", "Localization", "customize",
     "Choosing the interface language, how language files and the English fallback work, "
     "and editing or creating a translation.",
     LZ,
     keywords="localization language translation lang file english fallback interface "
              "language phrase editor translate")

# ==========================================================================
# Settings files
# ==========================================================================

SF = ""
SF += p(
    "Everything Tiko remembers is a plain file next to <code>tiko.exe</code>. There is "
    "nothing in the registry and nothing in your user profile, which is what makes Tiko "
    "portable."
)

SF += h2("The files")
SF += table(
    ["File", "Holds"],
    [
        ("<code>settings\\settings.ini</code>",
         "Every editor setting: fonts, colours, editing behaviour, window geometry, panel "
         "layout, the compiler path and the recent-file lists."),
        ("<code>settings\\keybindings.ini</code>",
         "Your keyboard shortcut overrides. Only overrides — defaults live in the program."),
        ("<code>settings\\default.tiko</code>",
         "The untitled workspace, so loose files are restored on start-up."),
        ("<code>settings\\themes\\*.theme</code>", "Colour themes."),
        ("<code>settings\\languages\\*.lang</code>", "Interface translations."),
        ("<code>settings\\keywords\\*.txt</code>",
         "Syntax highlighting keyword lists, which also supply autocomplete entries and "
         "canonical keyword spellings."),
    ],
    key_first=True,
)

SF += h2("The settings file")
SF += p(
    "<code>settings.ini</code> is a conventional INI file, grouped into sections. It is "
    "written when you choose OK in a settings dialog and when Tiko exits."
)
SF += code("""
[Editor]
SyntaxHighlighting=-1
AutoComplete=-1
TabSize=4
TabIndentSpaces=-1
EditorFontname=Consolas
EditorFontsize=11
Theme=default_dark.theme
LocalizationFile=english.lang

[Startup]
StartupMaximized=-1
ShowPanel=1
ShowPanelWidth=236
ShowOutputPanel=0
""", lang="ini", title="settings\\settings.ini (extract)", numbered=False)
SF += important(
    "Boolean settings use the BASIC convention: <strong><code>-1</code> is true</strong> "
    "and <strong><code>0</code> is false</strong>. A setting written as <code>1</code> is "
    "not what a true value looks like here."
)
SF += p(
    'Every key is documented in the <a href="configuration-reference.html">configuration '
    "reference</a>."
)

SF += h2("Editing settings by hand")
SF += warn(
    "<strong>Close Tiko before editing <code>settings.ini</code>.</strong> The editor "
    "writes the whole file when it exits, so changes made while it is running are "
    "overwritten."
)
SF += p(
    "Most settings are better changed through the options dialog, which validates what you "
    "enter. Editing the file directly is useful for bulk changes and for values you want "
    "to copy between installations."
)

SF += h2("Backing up, and moving to another machine")
SF += p(
    "Copy the <code>settings</code> folder. That is the entire backup — settings, "
    "shortcuts, themes, languages and keyword lists."
)
SF += table(
    ["To transfer", "Copy"],
    [
        ("Everything", "The whole <code>settings</code> folder."),
        ("Just your shortcuts", "<code>settings\\keybindings.ini</code>"),
        ("Just a theme", "The <code>.theme</code> file from <code>settings\\themes</code>"),
        ("Just your editor preferences",
         "<code>settings\\settings.ini</code> — but see the note below."),
    ],
    key_first=True,
)
SF += note(
    "<code>settings.ini</code> also holds window positions and the recent-file lists, "
    "which are specific to a machine. Copying it wholesale to a computer with a different "
    "screen layout may put the window somewhere awkward; %s fixes that."
    % menu("View", "Restore Main Window Size")
)

SF += h2("Resetting to defaults")
SF += ol([
    "Close Tiko.",
    "Rename <code>settings.ini</code> to <code>settings.ini.bak</code> — rename rather "
    "than delete, so you can go back.",
    "Start Tiko. It creates a fresh file with default values.",
], steps=True)
SF += p(
    "To reset only the keyboard, delete <code>keybindings.ini</code> instead. To reset only "
    "the workspace, delete <code>default.tiko</code>."
)

SF += h2("Running from removable media")
SF += p(
    "Because nothing is stored outside its own folder, Tiko runs from a USB stick or a "
    "network share with your full configuration intact. Copy the folder, run "
    "<code>tiko.exe</code> from it, and the editor you get is the editor you set up."
)
SF += warn(
    "Do not install Tiko into <code>C:\\Program Files</code>. That location is "
    "write-protected for ordinary users, and Tiko needs to write its settings beside the "
    "executable."
)

SF += h2("Multiple installations")
SF += p(
    "Two copies of Tiko in different folders have completely separate settings, themes and "
    "workspaces. This is deliberate and useful — you might keep a customised installation "
    "for one project and a stock one for another — but it also means that "
    "<strong>which copy you launch decides which configuration you get</strong>. If a "
    "setting seems not to have taken effect, check which <code>tiko.exe</code> you ran."
)

SF += h2("Related topics")
SF += ul([
    '<a href="configuration-reference.html">Configuration reference</a>',
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>',
    '<a href="troubleshooting.html">Troubleshooting</a>',
])

page("settings-files", "Settings and configuration files", "customize",
     "Where Tiko stores everything it remembers, how to edit those files, back them up, "
     "move them between machines and reset them.",
     SF,
     keywords="settings.ini configuration files keybindings.ini portable backup export "
              "import reset defaults registry program files multiple installations")
