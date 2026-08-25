# -*- coding: utf-8 -*-
"""Reference section, part 2: configuration and dialog references."""

from build import (page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui, esc,
                   placeholder, diagram)

# ==========================================================================
# Configuration reference
# ==========================================================================

CFG = ""
CFG += p(
    "Every setting Tiko Editor stores in <code>settings\\settings.ini</code>. Most are set "
    "through %s; they are documented here so you can read the file, "
    "compare two installations, or edit it in bulk."
    % menu("File", "Settings", "Options…")
)
CFG += important(
    "Booleans follow the BASIC convention: <strong><code>-1</code> means true</strong> and "
    "<strong><code>0</code> means false</strong>."
)
CFG += warn(
    "Close Tiko Editor before editing the file by hand. It rewrites the whole file on exit, which "
    "discards anything you changed while it was running."
)


def cfgrow(key, purpose, default, values, related=""):
    body = "<p>%s</p>" % purpose
    rows = [("Default", "<code>%s</code>" % esc(default)),
            ("Values", values)]
    if related:
        rows.append(("Related", related))
    return (h3("<code>%s</code>" % esc(key), anchor="cfg-" + key.lower())
            + body + dl(rows))


BOOL = "<code>-1</code> (true) or <code>0</code> (false)"

CFG += h2("[Editor] — general behaviour")
CFG += cfgrow("AskExit", "Prompt for confirmation when closing Tiko Editor.", "0", BOOL)
CFG += cfgrow("AutoSaveFiles", "Save modified documents automatically at an interval.",
              "0", BOOL, "<code>AutoSaveInterval</code>")
CFG += cfgrow("AutoSaveInterval", "Minutes between automatic saves.", "10",
              "A whole number of minutes", "<code>AutoSaveFiles</code>")
CFG += cfgrow("RestoreSession",
              "Reopen the workspace that was active when Tiko Editor last closed.", "-1", BOOL,
              "<code>LastActiveProjectFile</code>")
CFG += cfgrow("LastActiveProjectFile",
              "The project reopened at start-up. Maintained by Tiko Editor; you do not normally "
              "edit it.", "(empty)", "A path to a <code>.tiko</code> file")
CFG += cfgrow("MultipleInstances", "Allow more than one copy of Tiko Editor to run at once.",
              "-1", BOOL)
CFG += cfgrow("CompactMenus", "Draw menu rows at a tighter height.", "0", BOOL)
CFG += cfgrow("CheckUpdates", "Check whether a newer version is available.", "-1", BOOL)
CFG += cfgrow("DetectExternalFileChanges",
              "Notice when an open file is changed by another program and offer to "
              "reload it.", "0", BOOL)
CFG += cfgrow("LocalizationFile", "The interface language file to load.", "english.lang",
              "A file name in <code>settings\\languages</code>",
              '<a href="localization.html">Localization</a>')
CFG += cfgrow("Theme", "The colour theme to load.", "default_dark.theme",
              "A file name in <code>settings\\themes</code>",
              '<a href="themes.html">Themes</a>')
CFG += cfgrow("ExplorerPositionRight", "Dock the side panel on the right of the window.",
              "0", BOOL)

CFG += h2("[Editor] — code intelligence")
CFG += cfgrow("SyntaxHighlighting", "Colour code according to its syntax.", "-1", BOOL,
              '<a href="syntax-highlighting.html">Syntax highlighting</a>')
CFG += cfgrow("Codetips", "Show parameter hints when you open a call's parenthesis.",
              "-1", BOOL, '<a href="autocomplete.html">Autocomplete and code tips</a>')
CFG += cfgrow("AutoComplete", "Offer a completion list as you type identifiers.", "-1",
              BOOL, '<a href="autocomplete.html">Autocomplete and code tips</a>')
CFG += cfgrow("CharacterAutoComplete",
              "Insert the closing member of a pair when you type an opening quote, "
              "parenthesis or bracket.", "0", BOOL)
CFG += cfgrow("BraceHighlight",
              "Highlight the brace matching the one beside the caret, and mark unmatched "
              "ones.", "0", BOOL,
              '<a href="brace-matching.html">Brace matching</a>')
CFG += cfgrow("OccurrenceHighlight",
              "Highlight every other occurrence of the word under the caret.", "0", BOOL)
CFG += cfgrow("KeywordCase",
              "How keywords are <em>displayed</em>. This never changes the file — see "
              "<code>FmtCaseKeywords</code> for the rule that rewrites it.", "3",
              "<code>0</code> lower case, <code>1</code> upper case, <code>2</code> proper "
              "case (the canonical spelling from the keyword files), <code>3</code> "
              "original case — exactly as typed",
              "<code>FmtCaseKeywords</code>, "
              '<a href="syntax-highlighting.html">Syntax highlighting</a>')

CFG += h2("[Editor] — display")
CFG += cfgrow("LineNumbering", "Show the line number margin.", "-1", BOOL)
CFG += cfgrow("LeftMargin", "Show the left marker margin, used for bookmarks and "
              "breakpoints.", "-1", BOOL)
CFG += cfgrow("FoldMargin", "Show the fold margin with its collapse markers.", "0", BOOL,
              '<a href="code-folding.html">Code folding</a>')
CFG += cfgrow("HighlightCurrentLine", "Tint the line holding the caret.", "-1", BOOL)
CFG += cfgrow("IndentGuides", "Draw vertical guides at each indent level.", "0", BOOL)
CFG += cfgrow("RightEdge", "Draw a vertical line at a chosen column.", "0", BOOL,
              "<code>RightEdgePosition</code>")
CFG += cfgrow("RightEdgePosition", "The column the right-edge line is drawn at.", "80",
              "A column number", "<code>RightEdge</code>")
CFG += cfgrow("ConfineCaret",
              "Stop the caret moving into the empty space past the end of a line.", "-1",
              BOOL)
CFG += cfgrow("PositionMiddle",
              "Centre the target line vertically when jumping to a line.", "0", BOOL)
CFG += cfgrow("EditorFontname", "The editor typeface. Use a monospaced font.", "Consolas",
              "Any installed font name",
              '<a href="editor-appearance.html">Fonts and editor appearance</a>')
CFG += cfgrow("EditorFontsize", "Editor font size in points.", "11", "A point size")
CFG += cfgrow("EditorFontCharSet", "The character set the font is requested with.",
              "Default", "A character set name")
CFG += cfgrow("FontExtraSpace", "Extra pixels of space between lines.", "2",
              "A pixel count")

CFG += h2("[Editor] — typing and indentation")
CFG += cfgrow("TabSize", "How many columns a tab stop occupies.", "4",
              "A column count", '<a href="indentation.html">Indentation</a>')
CFG += cfgrow("TabIndentSpaces",
              "Insert spaces rather than a tab character when you press "
              + kbd("Tab") + ".", "-1", BOOL, "<code>TabSize</code>")
CFG += cfgrow("AutoIndentation",
              "Carry the current indent forward when you press " + kbd("Enter") + ", and "
              "complete block statements.", "-1", BOOL,
              "<code>FornextVariable</code>, <code>AutoComplete</code>")
CFG += cfgrow("FornextVariable",
              "Include the loop variable when completing a <code>For</code> block's "
              "<code>Next</code>.", "0", BOOL, "<code>AutoIndentation</code>")
CFG += cfgrow("StripTrailingWhitespace",
              "Remove trailing spaces and tabs from every line when saving. Shown as "
              "<strong>Strip line ending whitespace when saving</strong> on the Advanced "
              "Code Editor options page.", "0", BOOL,
              "<code>FmtTrimTrailing</code>")
CFG += cfgrow("ClickToggleBreakpoint",
              "Clicking the left margin sets or clears a breakpoint rather than selecting "
              "the line.", "0", BOOL, '<a href="breakpoints.html">Breakpoints</a>')

CFG += h2("[Editor] — encoding")
ENC_VALUES = ("<code>0</code> ANSI, <code>1</code> UTF-8, <code>2</code> UTF-8 (BOM), "
              "<code>3</code> UTF-16 (BOM)")
CFG += cfgrow("NewFileEncoding",
              "The encoding given to newly created files. The default, <code>1</code>, is "
              "UTF-8 without a byte-order mark.", "1",
              ENC_VALUES, '<a href="encoding.html">Encoding and line endings</a>')
CFG += cfgrow("UnicodeEncoding",
              "How Tiko Editor treats files whose encoding it cannot determine.", "0",
              ENC_VALUES, '<a href="encoding.html">Encoding and line endings</a>')

CFG += h2("[Editor] — formatter rules")
CFG += p(
    "Every key beginning <code>Fmt</code> is a code formatter rule, configured through "
    "%s. See <a href=\"code-formatting.html\">Code formatting</a>."
    % menu("Edit", "Format", "Format Options…")
)
CFG += table(
    ["Key", "Rule", "Default"],
    [
        ("<code>FmtReindent</code>", "Recompute the indentation of every line.",
         "<code>-1</code>"),
        ("<code>FmtIndentCase</code>", "Indent <code>Case</code> labels inside "
         "<code>Select Case</code>.", "<code>-1</code>"),
        ("<code>FmtContinuationIndent</code>",
         "Indent levels applied to a continued line.", "<code>1</code>"),
        ("<code>FmtCaseKeywords</code>",
         "Rewrite keywords to their canonical spelling.", "<code>0</code>"),
        ("<code>FmtCaseDirectives</code>",
         "Rewrite preprocessor directives to canonical case.", "<code>0</code>"),
        ("<code>FmtCaseTypes</code>", "Rewrite type names to canonical case.",
         "<code>0</code>"),
        ("<code>FmtSpaceOperators</code>", "Space either side of binary operators.",
         "<code>-1</code>"),
        ("<code>FmtSpaceAfterComma</code>", "A space after each comma.",
         "<code>-1</code>"),
        ("<code>FmtParenSpacing</code>", "Spacing just inside parentheses.",
         "<code>0</code>"),
        ("<code>FmtSpacesBeforeComment</code>",
         "Minimum spaces before a trailing comment.", "<code>0</code>"),
        ("<code>FmtTrimTrailing</code>", "Remove trailing whitespace.", "<code>-1</code>"),
        ("<code>FmtMaxBlankLines</code>",
         "Collapse runs of blank lines to at most this many.", "<code>0</code>"),
        ("<code>FmtBlankLinesAroundProc</code>",
         "Ensure blank lines separate procedures.", "<code>-1</code>"),
        ("<code>FmtFormatOnEnter</code>",
         "Format the completed line when you press " + kbd("Enter") + ".",
         "<code>0</code>"),
        ("<code>FmtFormatOnPaste</code>", "Format pasted text to match its surroundings.",
         "<code>0</code>"),
    ],
    key_first=True,
)

CFG += h2("[Editor] — building")
CFG += cfgrow("CompileAutosave", "Save modified documents before each build.", "-1", BOOL,
              '<a href="building.html">Building and running</a>')

CFG += h2("[Compiler]")
CFG += p(
    "The compiler section is written from the Compiler page of the options dialog. You "
    'normally change it there rather than by hand — see '
    '<a href="compiler-setup.html">Compiler setup</a>.'
)
CFG += cfgrow("FBWINCompiler32",
              "Full path to the 32-bit compiler. Tiko Editor rebuilds this from the toolchain you "
              "select; it is not edited directly.",
              "toolchains\\&lt;toolchain&gt;\\fbc32.exe",
              "A path to an <code>fbc32.exe</code>", "<code>FBWINCompiler64</code>")
CFG += cfgrow("FBWINCompiler64",
              "Full path to the 64-bit compiler, from the same selected toolchain.",
              "toolchains\\&lt;toolchain&gt;\\fbc64.exe",
              "A path to an <code>fbc64.exe</code>", "<code>FBWINCompiler32</code>")
CFG += cfgrow("CompilerBuild", "The active build configuration, stored by its identifier.",
              "(a GUID)", "A build configuration GUID",
              '<a href="build-configurations.html">Build configurations</a>')
CFG += cfgrow("CompilerSwitches",
              "Switches appended to every build in every project.", "(empty)",
              "Compiler switches, space separated")
CFG += cfgrow("CompilerIncludes",
              "Extra directories searched for <code>#include</code> files, beyond the "
              "toolchain's own <code>inc</code> folder.", "(empty)", "Directory paths")
CFG += cfgrow("RunViaCommandWindow",
              "Launch the compiled program through a command window rather than directly.",
              "0", BOOL)
CFG += cfgrow("DisableCompileBeep", "Suppress the sound played when a build finishes.",
              "0", BOOL)
CFG += note(
    "Compiler paths are stored with a <code>{CURDRIVE}</code> token in place of the drive "
    "letter, so an installation keeps working when the same folder is opened from a "
    "different drive — a USB stick that mounts as <code>E:</code> on one machine and "
    "<code>F:</code> on another."
)

CFG += h2("[Startup] — window and layout")
CFG += p(
    "These are maintained by Tiko Editor as you move and resize things. They are listed for "
    "completeness; edit them only to move a window that has ended up off screen — "
    "%s does the same thing more safely."
    % menu("View", "Restore Main Window Size")
)
CFG += table(
    ["Key", "Holds"],
    [
        ("<code>StartupLeft</code>, <code>StartupTop</code>, <code>StartupRight</code>, "
         "<code>StartupBottom</code>", "The main window's position and size."),
        ("<code>StartupMaximized</code>", "Whether the main window was maximised."),
        ("<code>HelpStartupLeft</code> … <code>HelpStartupMaximized</code>",
         "The same for the Help Center window."),
        ("<code>ShowPanel</code>", "Whether the side panel is visible."),
        ("<code>ShowPanelWidth</code>", "The side panel's width in pixels."),
        ("<code>ShowOutputPanel</code>", "Whether the Output panel is visible."),
        ("<code>ShowOutputPanelHeight</code>", "The Output panel's height in pixels."),
        ("<code>ShowOutputPanelMinimized</code>",
         "Whether the Output panel is collapsed to its tab strip."),
        ("<code>ShowOutputPanelIndex</code>", "Which Output panel tab was active."),
        ("<code>UnusedKindMask</code>",
         "Which symbol kinds the Unused Symbols report includes — a bitmask over the six "
         "kinds, in order: variables (1), procedures (2), parameters (4), types (8), "
         "fields (16), constants (32). The default <code>63</code> is all six."),
    ],
    key_first=True,
)
CFG += note(
    "<code>ShowOutputPanel</code> and <code>ShowOutputPanelMinimized</code> are separate "
    "on purpose. All four combinations are meaningful — including <em>closed, but reopens "
    "minimised</em>."
)

CFG += h2("Other files")
CFG += table(
    ["File", "Contents"],
    [
        ("<code>settings\\keybindings.ini</code>",
         "Keyboard shortcut overrides — see "
         '<a href="command-reference.html">Command reference</a>.'),
        ("<code>settings\\default.tiko</code>", "The untitled workspace."),
        ("<code>settings\\themes\\*.theme</code>", "Colour themes."),
        ("<code>settings\\languages\\*.lang</code>", "Interface translations."),
        ("<code>settings\\keywords\\*.txt</code>", "Syntax highlighting keyword lists."),
        ("<code>toolchains\\</code>",
         "One subfolder per compiler toolchain, each holding <code>fbc32.exe</code> and "
         "<code>fbc64.exe</code>. Not a settings file, but Tiko Editor scans it — see "
         '<a href="compiler-setup.html">Compiler setup</a>.'),
    ],
    key_first=True,
)

CFG += h2("Related topics")
CFG += ul([
    '<a href="settings-files.html">Settings and configuration files</a>',
    '<a href="dialog-reference.html">Dialog reference</a>',
    '<a href="troubleshooting.html">Troubleshooting</a>',
])

page("configuration-reference", "Configuration reference", "reference",
     "Every setting in settings.ini: what it does, its default, the values it accepts and "
     "the settings it relates to.",
     CFG,
     keywords="configuration reference settings.ini options keys defaults values editor "
              "startup formatter fmt boolean -1 0")

# ==========================================================================
# Dialog reference
# ==========================================================================

DR = ""
DR += p(
    "Every dialog in Tiko Editor and what its controls do. Dialogs edit a working copy of your "
    "settings, so <strong>Cancel</strong> always discards every change made since the "
    "dialog opened."
)

DR += h2("Options", anchor="options")
DR += p("Opened with %s or %s. Nine pages, listed down the left."
        % (menu("File", "Settings", "Options…"), kbd("Ctrl", ",")))
DR += table(
    ["Page", "Covers"],
    [
        ("General Options", "Start-up behaviour, autosave, update checking, multiple "
         "instances and menu density."),
        ("Code Editor", "Line numbers, margins, current-line highlighting, indent guides, "
         "the right-edge marker, tabs and indentation."),
        ("Advanced Code Editor", "Code tips, autocomplete, character auto-completion, "
         "auto-indentation and its For/Next option, brace and occurrence highlighting, and "
         "<strong>Strip line ending whitespace when saving</strong>."),
        ("Editor Font", "The editor font name, size, character set and extra line "
         "spacing."),
        ("Compiler Setup", "Which bundled toolchain to build with, plus global compiler "
         "switches, extra include paths, and two build behaviour toggles."),
        ("Localization", "The interface language, and the phrase editor."),
        ("FreeBASIC Keywords", "The FreeBASIC language keyword list."),
        ("Windows API Keywords", "The Windows API name list."),
        ("Extra Keywords", "Any further names you want highlighted and offered by "
         "autocomplete."),
    ],
    key_first=True,
)
DR += p("<strong>OK</strong> applies and saves; <strong>Cancel</strong> discards everything.")
DR += note(
    "There is no colours page here — every colour in Tiko Editor belongs to the theme. Use "
    "%s instead." % menu("File", "Settings", "Themes…")
)

DR += h3("Compiler Setup page", anchor="options-compiler")
DR += table(
    ["Control", "Purpose"],
    [
        ("Toolchain list", "Every subfolder of <code>toolchains\\</code>. Selecting one "
         "sets the paths to both its <code>fbc32.exe</code> and <code>fbc64.exe</code>."),
        ("Compiler switches", "Switches appended to every build in every project."),
        ("Include paths", "Extra directories searched for <code>#include</code> files."),
        ("Run via command window", "Launch the compiled program through a command window."),
        ("Disable compile beep", "Suppress the sound played when a build finishes."),
    ],
    key_first=True,
)
DR += p('See <a href="compiler-setup.html">Compiler setup</a>.')

DR += h2("Themes", anchor="themes")
DR += p("Opened with %s or %s."
        % (menu("File", "Settings", "Themes…"), kbd("Ctrl", "Shift", "T")))
DR += table(
    ["Control", "Purpose"],
    [
        ("Theme list", "The themes in <code>settings\\themes</code>, in three columns: "
         "Theme, Description and Active."),
        ("Edit", "Open the selected theme in the colour editor. Does not activate it. "
         "Disabled for <code>default_dark</code> and <code>default_light</code>."),
        ("Clone", "Copy the selected theme to a new file — the way to build on a "
         "protected theme."),
        ("Delete", "Delete the selected theme's file. Disabled for the two protected "
         "themes."),
        ("Set as active", "Switch the editor to the selected theme."),
        ("Pencil icon", "In the editor view, edit the theme's description."),
        ("Back", "Return from the editor view to the list."),
        ("Palette page", "The theme's semantic roles. Changing one changes everything "
         "that inherits from it."),
        ("Key pages", "Individual colour keys, grouped by the part of the interface they "
         "affect, each showing the role it falls back to."),
        ("Colour button", "Opens the colour picker for that key or role."),
        ("R / G / B / A fields", "Enter an exact colour value."),
    ],
    key_first=True,
)
DR += p('See <a href="theme-editor.html">The theme editor</a>.')

DR += h2("Build Configurations", anchor="build-config")
DR += p("Opened with %s or %s."
        % (menu("File", "Settings", "Build Configurations…"), kbd("F7")))
DR += table(
    ["Control", "Purpose"],
    [
        ("Configuration list", "Your configurations, with a marker on the default."),
        ("Add / Delete", "Create or remove a configuration."),
        ("Move up / Move down", "Reorder the list, which is the order shown in menus."),
        ("General page", "Description, keyboard shortcut and behaviour options."),
        ("Compiler switches page",
         "A checklist of compiler switches. Mutually exclusive switches are grouped."),
        ("Shortcut", "An optional accelerator. One that cannot work is left unassigned "
         "rather than silently failing."),
    ],
    key_first=True,
)
DR += p('See <a href="build-configurations.html">Build configurations</a>.')

DR += h2("Keyboard Shortcuts", anchor="keyboard")
DR += p("Opened with %s or %s."
        % (menu("File", "Settings", "Keyboard Shortcuts…"), kbd("Ctrl", "K")))
DR += table(
    ["Control", "Purpose"],
    [
        ("Filter box", "Narrows the list, matching across all four columns."),
        ("Command list", "Category, command, current shortcut and description. Conflicts "
         "are marked."),
        ("Modify", "Opens the assignment dialog for the selected command."),
        ("Reset", "Restores the selected command's default shortcut."),
        ("OK / Cancel", "Commit every change, or discard every change."),
    ],
    key_first=True,
)
DR += h3("Assign shortcut dialog")
DR += table(
    ["Control", "Purpose"],
    [
        ("Capture field", "Press the keystroke you want. A keystroke already in use is "
         "refused."),
        ("Ctrl / Alt / Shift switches", "Build a keystroke by hand."),
        ("Key list", "Choose the base key from the full list of key names."),
        ("OK", "Disabled while the current combination would clash."),
    ],
    key_first=True,
)
DR += p('See <a href="keyboard-customization.html">Customizing keyboard shortcuts</a>.')

DR += h2("User Tools", anchor="user-tools")
DR += p("Opened with %s." % menu("File", "User Tools…"))
DR += table(
    ["Control", "Purpose"],
    [
        ("Tool list", "Your tools, with their shortcuts. Reorder with the chevrons."),
        ("Add / Delete", "Create or remove a tool."),
        ("Description", "The text shown on the User Tools menu."),
        ("Program", "The executable to run."),
        ("Parameters", "Its command line. The field's tooltip lists the substitution "
         "codes."),
        ("Working folder", "The directory the program starts in."),
        ("Assign Shortcut", "Opens the same assignment dialog used for editor commands."),
    ],
    key_first=True,
)
DR += p('See <a href="user-tools.html">User tools</a>.')

DR += h2("Format Options", anchor="format-options")
DR += p("Opened with %s." % menu("Edit", "Format", "Format Options…"))
DR += table(
    ["Control", "Purpose"],
    [
        ("Page list", "Rule groups — indentation, case, spacing, blank lines, triggers."),
        ("Rule rows", "One toggle or value per rule."),
        ("Triggers page", "Format on Enter and Format on paste."),
        ("Live preview", "Sample code formatted by the real engine, updating as you "
         "change a rule."),
    ],
    key_first=True,
)
DR += p('See <a href="code-formatting.html">Code formatting</a>.')

DR += h2("Project Options", anchor="project-options")
DR += p("Opened with %s." % menu("Project", "Project Options…"))
DR += table(
    ["Section", "Purpose"],
    [
        ("Project", "Project identification and the command-line arguments passed to your "
         "program when Tiko Editor runs it."),
        ("Compiler options", "Extra compiler switches for this project."),
        ("Build output", "Where the built executable is written."),
    ],
    key_first=True,
)
DR += p('See <a href="project-options.html">Project options</a>.')

DR += h2("Find and Replace", anchor="find")
DR += table(
    ["Control", "Purpose"],
    [
        ("Find what", "The text to search for. Literal text — there is no regular "
         "expression mode."),
        ("Replace with", "The replacement text (Replace only)."),
        ("Match Case", "Make the search case-sensitive."),
        ("Match Whole Words", "Match complete words only."),
        ("Selection", "Confine the search to the selected text."),
        ("Toggle Replace", "Show or hide the replacement field."),
        ("Search Previous / Search Next", "Move to the previous or next match."),
        ("Replace", "Replace the current match and find the next."),
        ("Replace All", "Replace every match, as a single undoable action."),
    ],
    key_first=True,
)
DR += p('See <a href="find-replace.html">Find and Replace</a>.')

DR += h2("Find in Project", anchor="find-in-project")
DR += p("Opened with %s." % kbd("Ctrl", "Shift", "F"))
DR += p(
    "Offers the same two matching options as Find — Match Case and Match Whole Words. "
    "There are no file filters and no regular expressions: every file in the workspace is "
    "searched for literal text. Results go to the Output panel's Search results tab."
)
DR += p('See <a href="find-in-project.html">Find in Project</a>.')

DR += h2("About", anchor="about")
DR += p("Opened with %s." % menu("Help", "About"))
DR += p(
    "Three pages — About, Credits and License — with buttons linking to the website and "
    "the source repository. Shows the version you are running, which is the first thing to "
    "quote in a bug report."
)

DR += h2("Related topics")
DR += ul([
    '<a href="dialogs.html">Dialog boxes</a> — how dialogs behave generally.',
    '<a href="configuration-reference.html">Configuration reference</a>',
    '<a href="menu-reference.html">Menu reference</a>',
])

page("dialog-reference", "Dialog reference", "reference",
     "Every dialog box in Tiko Editor, control by control, with links to the topic that explains "
     "each in context.",
     DR,
     keywords="dialog reference options themes build configurations keyboard shortcuts "
              "user tools format options project options find replace find in project "
              "about controls")
