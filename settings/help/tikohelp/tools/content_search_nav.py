# -*- coding: utf-8 -*-
"""Searching, Navigation and Editing Productivity sections."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   placeholder, diagram)

# ==========================================================================
section("searching", "Searching", "find")
# ==========================================================================

FR = ""
FR += p(
    "Find and Replace work within the <strong>current document</strong>. To search every "
    'file in the workspace, use <a href="find-in-project.html">Find in Project</a> instead.'
)

FR += h2("Find")
FR += ol([
    "Press %s, or choose %s." % (kbd("Ctrl", "F"), menu("Search", "Find…")),
    "Type what you are looking for. If text was selected when you opened Find, it is used "
    "as the initial search term.",
    "Press %s to go to the next match, %s for the previous one."
    % (kbd("Enter"), kbd("Shift", "Enter")),
    "Press %s to close the Find bar. The search term is remembered." % kbd("Esc"),
], steps=True)
FR += important(
    "%s and %s repeat the last search from anywhere in the editor — you do not need the "
    "Find bar open. This is the fastest way to step through matches while editing."
    % (kbd("F3"), kbd("Shift", "F3"))
)

FR += h2("Search options")
FR += table(
    ["Option", "Effect"],
    [
        ("Match case", "<code>Print</code> no longer matches <code>print</code>. Off by "
         "default, which suits FreeBASIC's case-insensitive keywords."),
        ("Whole word", "Matches only complete words: <code>count</code> will not match "
         "<code>counter</code> or <code>rowcount</code>."),
        ("Regular expression", "Treats the search text as a pattern rather than literal "
         'text. See <a href="regular-expressions.html">Regular expressions</a>.'),
        ("Search up", "Searches backwards from the caret rather than forwards."),
        ("Wrap around", "Continues from the top when the search reaches the end of the "
         "document."),
    ],
    key_first=True,
)
FR += todo(
    "Confirm the exact option set and labels on the Find and Replace bars against the "
    "shipping build, and replace the placeholder below with a real screenshot.",
    title="TODO — verify Find bar options",
)
FR += placeholder("Find bar", "Screenshot of the Find bar with its options",
                  caption="Replace with a capture of the Find bar showing the search "
                          "field and option toggles.")

FR += h2("Replace")
FR += p("Press %s, or choose %s." % (kbd("Ctrl", "H"), menu("Search", "Replace…")))
FR += table(
    ["Button", "What it does"],
    [
        ("Find Next", "Moves to the next match without changing anything."),
        ("Replace", "Replaces the current match, then finds the next one. This is the "
         "safe, reviewable option."),
        ("Replace All", "Replaces every match in the document in one step, as a single "
         "undoable action."),
    ],
    key_first=True,
)
FR += warn(
    "Replace All is a single %s away if it goes wrong — but check your options first. A "
    "Replace All with <strong>Whole word</strong> off is the classic way to turn every "
    "<code>id</code> into <code>identifier</code> inside longer words." % kbd("Ctrl", "Z")
)
FR += tip(
    "Before a large Replace All, run the same search with %s to see how many places it "
    "will touch and whether they are all what you expect." % kbd("Ctrl", "Shift", "F")
)

FR += h2("Incremental searching")
FR += p(
    "The Find bar highlights matches as you type, so you can see whether a search term is "
    "going to be too broad before you commit to it. Refine the term and the highlighting "
    "updates."
)

FR += h2("Search history")
FR += p(
    "Recent search and replacement terms are kept in the drop-down beside each field. Use "
    "the down-arrow key in an empty field to bring back your last search."
)

FR += h2("Related topics")
FR += ul([
    '<a href="find-in-project.html">Find in Project</a>',
    '<a href="regular-expressions.html">Regular expressions</a>',
    '<a href="multiple-selections.html">Multiple selections</a> — an alternative for '
    "small, local edits.",
])

page("find-replace", "Find and Replace", "searching",
     "Searching within the current document, the search options, replacing matches, and "
     "search history.",
     FR,
     keywords="find search replace replace all find next find previous f3 incremental "
              "match case whole word wrap search up history")

# --------------------------------------------------------------------------

FP = ""
FP += p(
    "Find in Project searches every file in the current workspace and collects the results "
    "in the Output panel, where each match is a clickable link to its line."
)
FP += ol([
    "Press %s, or choose %s."
    % (kbd("Ctrl", "Shift", "F"), menu("Search", "Find in Project…")),
    "Enter the search text and set any options you need.",
    "Start the search. The Output panel switches to its <strong>Search results</strong> "
    "tab.",
    "Click any result to open that file and jump to the line.",
], steps=True)

FP += h2("Options")
FP += p(
    "Find in Project offers the same matching options as Find — case sensitivity, whole "
    "word and regular expressions — plus filters over which files are searched."
)
FP += todo(
    "Confirm the exact filter controls (file masks, folder scope, include/exclude "
    "patterns) offered by the Find in Project dialog in the shipping build, and document "
    "each one.",
    title="TODO — verify Find in Project filters",
)
FP += placeholder("Find in Project", "Screenshot of the Find in Project dialog",
                  caption="Replace with a capture of the dialog showing its options and "
                          "filter fields.")

FP += h2("Reading the results")
FP += p(
    "Each result row shows the file, the line number and the matching line's text. The "
    "list is multi-column — click a header to sort by that column, which is a quick way to "
    "group every hit in one file together."
)
FP += note(
    "Results stay in the panel until your next project search, so you can work through a "
    "long list at your own pace. Switching tabs in the Output panel does not clear them."
)

FP += h2("Typical uses")
FP += ul([
    "<strong>Before renaming anything.</strong> Search for the identifier first to see "
    "every place it appears, including comments and strings that a rename would miss.",
    "<strong>Finding where a procedure is called from.</strong> Goto Definition (%s) goes "
    "the other way; this finds the callers." % kbd("F12"),
    "<strong>Auditing.</strong> Search for a pattern such as <code>TODO</code>, "
    "<code>HACK</code> or a deprecated API name across the whole codebase.",
])
FP += tip(
    "Tiko already collects <code>TODO</code> comments automatically into the Output "
    "panel's TODO tab — you do not need to search for those."
)

FP += h2("Related topics")
FP += ul([
    '<a href="find-replace.html">Find and Replace</a>',
    '<a href="regular-expressions.html">Regular expressions</a>',
    '<a href="output-panel.html">Output panel</a>',
    '<a href="projects-overview.html">Projects and workspaces</a> — what "the project" '
    "means for a search.",
])

page("find-in-project", "Find in Project", "searching",
     "Searching every file in the workspace at once, and working through the results in "
     "the Output panel.",
     FP,
     keywords="find in project search project search all files grep results output panel "
              "ctrl shift f")

# --------------------------------------------------------------------------

RE = ""
RE += p(
    "Turning on the <strong>Regular expression</strong> option changes the search text "
    "from something matched literally into a <em>pattern</em>. Patterns let you find "
    "things you could not otherwise describe — any number, any line starting with a "
    "comment, an identifier followed by an opening parenthesis."
)
RE += warn(
    "Regular-expression syntax varies between tools. The table below covers constructs "
    "that are near-universal and safe to rely on. Confirm the exact dialect against the "
    "shipping build before depending on anything more exotic."
)
RE += todo(
    "Confirm which regular-expression engine Tiko uses for Find and for Find in Project "
    "(the Scintilla built-in engine and its C++ <code>std::regex</code> option behave "
    "differently), and document the supported syntax precisely.",
    title="TODO — confirm the regular expression dialect",
)

RE += h2("Pattern basics")
RE += table(
    ["Pattern", "Matches", "Example"],
    [
        ("<code>.</code>", "Any single character",
         "<code>b.t</code> matches <code>bat</code>, <code>bit</code>, <code>but</code>"),
        ("<code>[abc]</code>", "Any one of the listed characters",
         "<code>[0-9]</code> matches any digit"),
        ("<code>[^abc]</code>", "Any character <em>not</em> listed", ""),
        ("<code>*</code>", "Zero or more of the preceding item",
         "<code>ab*c</code> matches <code>ac</code>, <code>abc</code>, <code>abbc</code>"),
        ("<code>+</code>", "One or more of the preceding item", ""),
        ("<code>?</code>", "Zero or one of the preceding item", ""),
        ("<code>^</code>", "Start of a line",
         "<code>^Dim</code> matches <code>Dim</code> only at the line start"),
        ("<code>$</code>", "End of a line", ""),
        ("<code>\\d</code>", "A digit", ""),
        ("<code>\\w</code>", "A word character (letter, digit or underscore)", ""),
        ("<code>\\s</code>", "Whitespace", ""),
        ("<code>\\</code>", "Escapes the next character",
         "<code>\\.</code> matches a literal full stop"),
        ("<code>(…)</code>", "A group, captured for use in the replacement", ""),
        ("<code>|</code>", "Either alternative",
         "<code>Sub|Function</code> matches either word"),
    ],
    key_first=True,
)

RE += h2("Using captures in a replacement")
RE += p(
    "Text captured by a group in the search pattern can be inserted into the replacement, "
    "normally with <code>\\1</code> for the first group, <code>\\2</code> for the second "
    "and so on. This is what makes regular-expression replace powerful rather than merely "
    "clever."
)
RE += code("""
Find:     Print (.+)
Replace:  Debug.Write \\1

' Before
Print "starting"
Print count

' After
Debug.Write "starting"
Debug.Write count
""", lang="text", title="A capture-and-reuse replacement", numbered=False)

RE += h2("Practical FreeBASIC patterns")
RE += table(
    ["Goal", "Pattern"],
    [
        ("Lines that are only a comment", "<code>^\\s*'</code>"),
        ("Any Sub or Function declaration", "<code>^\\s*(Sub|Function)\\s+\\w+</code>"),
        ("A number literal", "<code>\\b\\d+\\b</code>"),
        ("Trailing whitespace", "<code>\\s+$</code>"),
        ("An empty line", "<code>^$</code>"),
        ("A call to a specific procedure", "<code>\\bDrawBox\\s*\\(</code>"),
    ],
)
RE += tip(
    "Test a pattern with Find first, then switch to Replace once you are satisfied it "
    "matches exactly what you intended. A pattern that is slightly too greedy can rewrite "
    "far more than you expect."
)
RE += note(
    "For trailing whitespace specifically, the formatter's <strong>Trim trailing "
    "whitespace</strong> rule is safer and more convenient than a regular-expression "
    'replace. See <a href="code-formatting.html">Code formatting</a>.'
)

RE += h2("Related topics")
RE += ul([
    '<a href="find-replace.html">Find and Replace</a>',
    '<a href="find-in-project.html">Find in Project</a>',
    '<a href="glossary.html">Glossary</a>',
])

page("regular-expressions", "Regular expressions", "searching",
     "Pattern-based searching: the common syntax, using captured groups in replacements, "
     "and practical patterns for FreeBASIC source.",
     RE,
     keywords="regular expression regex pattern search wildcard capture group replace "
              "backreference anchor character class")

# ==========================================================================
section("navigation", "Navigation", "compass")
# ==========================================================================

NV = ""
NV += p(
    "Tiko gives you several ways to move around, from the whole-project symbol search down "
    "to stepping between procedures in one file. Learning two or three of these makes far "
    "more difference to day-to-day speed than any other feature."
)

NV += h2("Goto Definition")
NV += p(
    "Put the caret on any symbol — a procedure name, a type, a variable — and press %s. "
    "Tiko jumps to where that symbol is defined, opening the file if necessary."
    % kbd("F12")
)
NV += p(
    "This works across the whole project because Tiko's parser has already indexed every "
    "file. It is the single most useful navigation command in the editor."
)
NV += note(
    "For a procedure with a separate declaration and implementation, Goto Definition takes "
    "you to the implementation — the code, rather than the prototype."
)

NV += h2("Navigation history")
NV += p(
    "Every jump is recorded, so you can retrace your steps the way you would in a browser."
)
NV += table(
    ["Command", "Shortcut"],
    [
        ("Go Back", kbd("Alt", "←")),
        ("Go Forward", kbd("Alt", "→")),
    ],
    key_first=True,
)
NV += tip(
    "%s then %s is the core navigation loop: dive into a definition, read it, and come "
    "straight back to where you were. Learn this pair first."
    % (kbd("F12"), kbd("Alt", "←"))
)

NV += h2("Search Symbol")
NV += p(
    "%s opens Search Symbol — a filter box over every symbol in the project. Start typing "
    "a procedure or type name and the list narrows as you type; choose an entry to jump "
    "straight to it." % kbd("Ctrl", "P")
)
NV += p(
    "Use this when you know <em>what</em> you are looking for but not which file it is in. "
    "It is the closest thing Tiko has to a universal \"go to anything\" command."
)
NV += todo(
    "Confirm whether Search Symbol also matches file names and line numbers, or symbols "
    "only, and document its matching rules (substring, prefix or fuzzy).",
    title="TODO — verify Search Symbol matching",
)

NV += h2("Moving between procedures")
NV += table(
    ["Command", "Shortcut"],
    [
        ("Next procedure", kbd("Ctrl", "PgDn")),
        ("Previous procedure", kbd("Ctrl", "PgUp")),
    ],
    key_first=True,
)
NV += p(
    "These step through the subs and functions of the current file in order — a quick way "
    "to skim a file's structure without leaving the keyboard."
)

NV += h2("Goto Line")
NV += p(
    "Compiler messages and colleagues both talk in line numbers. To jump to a specific "
    "line, use the Goto command and enter the number."
)
NV += todo(
    "Confirm the exact Goto Line command and its default shortcut in the shipping build "
    "and document it here.",
    title="TODO — verify the Goto Line command",
)
NV += note(
    "Clicking a compiler error in the Output panel jumps to the right line directly, so "
    "you rarely need to type a line number by hand."
)

NV += h2("Switching between related files")
NV += p(
    "Tiko knows about the conventional relationships between the files of a FreeBASIC "
    "project, and can hop between them:"
)
NV += table(
    ["Command", "Shortcut", "Goes to"],
    [
        ("Goto Header File", kbd("Ctrl", "Shift", "H"),
         "The header (<code>.bi</code>) matching the current source file."),
        ("Goto Code File", kbd("Ctrl", "Shift", "C"),
         "The source (<code>.bas</code>) matching the current header."),
        ("Goto Main File", kbd("Ctrl", "Shift", "M"),
         "The project's main module."),
        ("Goto Resource File", kbd("Ctrl", "Shift", "R"),
         "The project's resource script."),
    ],
    key_first=True,
)
NV += tip(
    "%s and %s form a toggle between a module and its header — the pairing you move "
    "between most often while writing code."
    % (kbd("Ctrl", "Shift", "H"), kbd("Ctrl", "Shift", "C"))
)

NV += h2("Switching between open files")
NV += ul([
    "%s and %s cycle through the open tabs."
    % (kbd("Ctrl", "Tab"), kbd("Ctrl", "Shift", "Tab")),
    "The tab list button at the end of the tab strip lists every open document.",
    "Clicking a file in the Explorer brings its tab forward if it is already open.",
])

NV += h2("Related topics")
NV += ul([
    '<a href="symbol-navigation.html">Symbols and the function list</a>',
    '<a href="bookmarks.html">Bookmarks</a>',
    '<a href="find-in-project.html">Find in Project</a>',
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
])

page("navigation", "Navigation", "navigation",
     "Goto Definition, navigation history, symbol search, procedure stepping, and jumping "
     "between related and open files.",
     NV,
     keywords="navigation goto definition f12 go back go forward search symbol ctrl p "
              "goto line next function previous function header file main file resource "
              "file switch tabs")

# --------------------------------------------------------------------------

SY = ""
SY += p(
    "Tiko continuously parses your source in the background and keeps a database of the "
    "symbols it finds — procedures and their parameters, types, enumerations, variables — "
    "each with the file, line and column where it was declared. Several features read "
    "from that one database."
)

SY += h2("What the parser extracts")
SY += table(
    ["Symbol kind", "Recorded"],
    [
        ("Subs and functions", "Name, parameters, return type, and where defined."),
        ("Types and unions", "Name, members, and where defined."),
        ("Enumerations", "Name and members."),
        ("Variables", "Name, type, and scope — including procedure locals."),
    ],
    key_first=True,
)
SY += note(
    "Parsing runs on a worker thread, so it never blocks typing. Results refresh as you "
    "edit — you do not need to save a file for its symbols to be picked up."
)

SY += h2("The function list")
SY += p(
    "%s shows every procedure in the current file in the side panel. Click one to jump to "
    "it. It is the fastest way to move around a long file, and it doubles as an outline "
    "of the file's structure." % kbd("F4")
)
SY += p(
    "The list follows the active document as you switch tabs, and refreshes as you type. "
    "Expand-all and collapse-all commands are available for the tree."
)

SY += h2("Unused symbols")
SY += p(
    "%s reports symbols the parser found declared but could not find used. It is a useful "
    "periodic tidy-up: dead procedures, leftover variables and stale types."
    % menu("Debug", "Unused Symbols…")
)
SY += warn(
    "Treat the report as a list of candidates, not a list of certainties. A symbol reached "
    "only through a function pointer, a conditional-compilation branch or an external "
    "reference can look unused while being essential."
)

SY += h2("Code tips")
SY += p(
    "When you type an opening parenthesis after a procedure name, Tiko shows a "
    "<strong>code tip</strong> — a small popup giving that procedure's parameter list. It "
    "closes when you type the closing parenthesis, press %s, or move the caret off the "
    "line." % kbd("Esc")
)
SY += p(
    "Code tips read the same symbol database, so they work for your own procedures as well "
    "as library ones. Turn them off with the <strong>Code tips</strong> option."
)

SY += h2("Related topics")
SY += ul([
    '<a href="navigation.html">Navigation</a>',
    '<a href="autocomplete.html">Autocomplete and code tips</a>',
    '<a href="side-panels.html">Side panels</a>',
])

page("symbol-navigation", "Symbols and the function list", "navigation",
     "How Tiko's background parser indexes your code, and the features that read from it: "
     "the function list, code tips and the unused-symbols report.",
     SY,
     keywords="symbol symbols parser function list outline procedures types enums "
              "variables unused symbols code tips index")

# ==========================================================================
section("productivity", "Editing Productivity", "bolt")
# ==========================================================================

AC = ""
AC += p(
    "Autocomplete offers completions as you type, drawn from the symbols Tiko has parsed "
    "out of your project plus the keyword lists for the current language."
)

AC += h2("Using autocomplete")
AC += ol([
    "Start typing an identifier. Once there is enough to narrow the field, a list appears "
    "beneath the caret.",
    "Keep typing to filter it further — the list narrows with every character.",
    "Move through the list with %s and %s." % (kbd("↑"), kbd("↓")),
    "Press %s or %s to accept the highlighted entry."
    % (kbd("Enter"), kbd("Tab")),
    "Press %s to dismiss the list and carry on typing." % kbd("Esc"),
], steps=True)
AC += note(
    "Typing <code>(</code> also accepts the highlighted entry when it is a procedure that "
    "takes parameters — and, because the parenthesis is inserted too, the code tip showing "
    "the parameter list appears immediately."
)

AC += h2("What is offered")
AC += ul([
    "Procedures, types, enumerations and variables from your project.",
    "Language keywords from the keyword lists in <code>settings\\keywords</code>.",
    "Entries are sorted alphabetically.",
])
AC += p(
    'The keyword lists are editable — see <a href="syntax-highlighting.html">Syntax '
    "highlighting</a>. The same lists drive highlighting and autocomplete, so a keyword "
    "you add is offered as well as coloured."
)

AC += h2("Settings")
AC += dl([
    ("Auto complete", "Turns the completion list on and off."),
    ("Code tips", "Turns parameter hints on and off."),
    ("Character auto completion",
     "Automatically inserts the closing member of a pair when you type an opening "
     "quotation mark, parenthesis or bracket."),
])

AC += h2("Code tips")
AC += p(
    "A code tip appears when you open a parenthesis after a known procedure name and shows "
    "its parameters. It follows the caret as you type arguments and disappears when the "
    "call is complete."
)
AC += p("A code tip closes when you:")
AC += ul([
    "type the closing parenthesis;",
    "press %s;" % kbd("Esc"),
    "move the caret off the line, or back before the opening parenthesis;",
    "switch to another document.",
])
AC += tip(
    "If a tip does not appear for one of your own procedures, the parser may not have seen "
    "the file yet — it is indexed as part of the project, so check the file is actually in "
    'the workspace. See <a href="project-files.html">Project files</a>.'
)

AC += h2("Related topics")
AC += ul([
    '<a href="symbol-navigation.html">Symbols and the function list</a>',
    '<a href="syntax-highlighting.html">Syntax highlighting</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("autocomplete", "Autocomplete and code tips", "productivity",
     "The completion list and parameter hints: how to use them, what they offer, and the "
     "settings that control them.",
     AC,
     keywords="autocomplete auto complete completion intellisense code tips calltip "
              "parameter hints character auto completion keywords")

# --------------------------------------------------------------------------

LO = ""
LO += p(
    "These commands act on whole lines or blocks, and save a great deal of selecting and "
    "dragging. Most work with no selection at all, acting on the current line."
)

LO += h2("Line commands")
LO += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Duplicate Line", kbd("Ctrl", "D"),
         "Copies the current line and inserts the copy below it."),
        ("Delete Line", kbd("Ctrl", "Y"), "Removes the current line entirely."),
        ("Move Line Up", kbd("Alt", "↑"),
         "Swaps the current line with the one above. Moves the whole selection if there "
         "is one."),
        ("Move Line Down", kbd("Alt", "↓"), "Swaps it with the line below."),
        ("Select Line", kbd("Ctrl", "L"), "Selects the whole current line."),
        ("New line below", kbd("Ctrl", "Enter"),
         "Opens a line beneath the current one from anywhere on it."),
    ],
    key_first=True,
)
LO += tip(
    "%s is the fastest way to reorder statements or move a procedure — select the lines "
    "and hold the shortcut down." % kbd("Alt", "↑")
)

LO += h2("Commenting")
LO += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Comment Block", kbd("Ctrl", "/"),
         "Comments out every selected line, or the current line."),
        ("UnComment Block", kbd("Ctrl", "Shift", "/"), "Removes the comment marks again."),
    ],
    key_first=True,
)
LO += p(
    "The comment character used matches the current file's language — an apostrophe for "
    "FreeBASIC, <code>//</code> for C-family files."
)

LO += h2("Changing case")
LO += table(
    ["Command", "Shortcut", "Result"],
    [
        ("Uppercase", kbd("Ctrl", "Alt", "U"), "<code>COUNT</code>"),
        ("Lowercase", kbd("Ctrl", "Alt", "L"), "<code>count</code>"),
        ("Mixed case", kbd("Ctrl", "Alt", "X"), "<code>Count</code>"),
    ],
    key_first=True,
)
LO += p("These act on the selection. With nothing selected, there is nothing to convert.")
LO += note(
    "To normalise the case of <em>keywords</em> throughout a file, use the formatter's "
    'case rules instead — see <a href="code-formatting.html">Code formatting</a>. These '
    "commands change whatever is selected, keyword or not."
)

LO += h2("Indenting blocks")
LO += p(
    "%s and %s indent and unindent every line of a multi-line selection. See "
    '<a href="indentation.html">Indentation</a>.' % (kbd("Tab"), kbd("Shift", "Tab"))
)

LO += h2("Sorting, joining and trimming lines")
LO += todo(
    "Confirm whether the shipping build provides Sort Lines, Join Lines, Transpose Lines "
    "and a standalone Trim Trailing Whitespace command, and document them here. The "
    "formatter provides trailing-whitespace trimming as a rule; a separate editor command "
    "may or may not exist.",
    title="TODO — confirm sort / join / transpose / trim commands",
)
FMT_LINK = ('The formatter\'s <strong>Trim trailing whitespace</strong> rule removes '
            'trailing spaces across a document — see '
            '<a href="code-formatting.html">Code formatting</a>.')
LO += p(FMT_LINK)

LO += h2("Related topics")
LO += ul([
    '<a href="editing-basics.html">Editing basics</a>',
    '<a href="code-formatting.html">Code formatting</a>',
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
])

page("line-operations", "Line and block operations", "productivity",
     "Duplicating, deleting, moving and selecting lines; commenting blocks; and converting "
     "the case of a selection.",
     LO,
     keywords="duplicate line delete line move line up down comment block uncomment "
              "select line uppercase lowercase mixed case sort join transpose trim")

# --------------------------------------------------------------------------

SH = ""
SH += p(
    "Syntax highlighting colours your code according to what each piece of it means — "
    "keywords, strings, comments, numbers, types. Tiko highlights through Lexilla, the "
    "lexer library that accompanies Scintilla."
)

SH += h2("Turning it on and off")
SH += p(
    "The <strong>Syntax highlighting</strong> option controls it globally. It is on by "
    "default; turning it off displays everything in one colour, which can be marginally "
    "faster on extremely large files."
)

SH += h2("Choosing the language for a file")
SH += p(
    "Tiko picks a language from the file's extension. The status bar shows the current "
    "choice and lets you override it for that document — useful for a file with an unusual "
    "extension, or a fragment you want highlighted as something else."
)

SH += h2("Keyword lists")
SH += p(
    "The words highlighted as keywords come from plain text files in "
    "<code>settings\\keywords</code>. These are editable, and this is how you teach Tiko "
    "about an API it does not know about — a third-party library's function names, for "
    "example."
)
SH += p("Those same lists do three jobs at once:")
SH += ul([
    "they decide what gets highlighted as a keyword;",
    "they supply entries to the autocomplete list;",
    "they provide the canonical spelling used by the formatter's keyword-case rules and by "
    "the editor's proper-case display.",
])
SH += important(
    "Because the canonical spelling comes from these files, enter each keyword the way you "
    "want to see it written — <code>ByVal</code>, <code>ScreenRes</code>, "
    "<code>MessageBox</code> — rather than in lower case. Matching itself is not "
    "case-sensitive, so the spelling in the file is purely about presentation."
)
SH += p(
    "The keyword pages in the options dialog let you edit these lists without leaving the "
    "editor."
)
SH += todo(
    "List the exact keyword list files shipped in <code>settings\\keywords</code> and what "
    "each one covers, and document the keyword editor pages in the options dialog.",
    title="TODO — enumerate keyword lists",
)

SH += h2("Keyword case display")
SH += p(
    "The <strong>Keyword case</strong> setting controls how keywords are <em>displayed</em> "
    "— unchanged, upper case, lower case or proper case. It is a display setting only: it "
    "never changes a byte in your file."
)
SH += important(
    "This is a different thing from the formatter's <strong>Case keywords</strong> rule, "
    "which rewrites the file. The two can legitimately disagree — you might display "
    "keywords in proper case while leaving the file exactly as written."
)

SH += h2("Colours")
SH += p(
    "Every syntactic element gets its colour from the current theme. To change one, use "
    'the theme editor — see <a href="theme-editor.html">The theme editor</a>.'
)

SH += h2("Related topics")
SH += ul([
    '<a href="theme-editor.html">The theme editor</a>',
    '<a href="autocomplete.html">Autocomplete</a>',
    '<a href="code-formatting.html">Code formatting</a>',
])

page("syntax-highlighting", "Syntax highlighting", "productivity",
     "How Tiko colours code, choosing the language for a file, editing the keyword lists, "
     "and the difference between displaying and rewriting keyword case.",
     SH,
     keywords="syntax highlighting colours lexer lexilla keywords keyword lists language "
              "keyword case proper case")

# --------------------------------------------------------------------------

BR = ""
BR += p(
    "With <strong>Brace highlighting</strong> on, placing the caret beside a parenthesis, "
    "bracket or brace highlights it and its partner. If the partner is missing, the "
    "unmatched one is marked in a different colour."
)
BR += p(
    "This is the quickest way to find an unbalanced expression — put the caret on the "
    "opening parenthesis and see where the highlight lands."
)
BR += note(
    "Highlighting is off by default. Turn it on in %s."
    % menu("File", "Settings", "Options…")
)

BR += h2("Occurrence highlighting")
BR += p(
    "The related <strong>Occurrence highlighting</strong> option highlights every other "
    "appearance of the word under the caret. Put the caret on a variable and every use of "
    "it in view is marked, which makes tracing a value through a procedure much easier."
)
BR += tip(
    "Occurrence highlighting is a fast, local alternative to running a search — but it is "
    "a visual aid only. Before you rename anything, use Find in Project to see every "
    "occurrence including the ones off screen."
)

BR += h2("Related topics")
BR += ul([
    '<a href="view-options.html">Display and view options</a>',
    '<a href="code-folding.html">Code folding</a>',
    '<a href="find-in-project.html">Find in Project</a>',
])

page("brace-matching", "Brace matching and occurrence highlighting", "productivity",
     "Matching parentheses and brackets, spotting unbalanced ones, and highlighting every "
     "occurrence of the word under the caret.",
     BR,
     keywords="brace matching bracket parenthesis highlight unmatched occurrence "
              "highlighting word under caret")

# --------------------------------------------------------------------------

CFM = ""
CFM += p(
    "The code formatter rewrites your source to a consistent style: indentation, keyword "
    "case, spacing around operators and commas, blank lines, trailing whitespace. Every "
    "rule can be turned on or off individually."
)

CFM += h2("The four commands")
CFM += table(
    ["Command", "Shortcut", "Scope"],
    [
        ("Format Document", kbd("Shift", "Alt", "F"), "The whole of the active file."),
        ("Format Selection", "—", "Only the selected lines."),
        ("Format All Open Documents", "—", "Every document currently open."),
        ("Format Project", "—", "Every file in the workspace."),
    ],
    key_first=True,
)
CFM += p("All four live on %s." % menu("Edit", "Format"))

CFM += h2("What the formatter will never do")
CFM += important(
    "<strong>The formatter never moves a line break.</strong> It will not split "
    "<code>If x Then y</code> into a block, will not join lines, and will not break a long "
    "line. Where your statements begin and end is your decision, not the formatter's. The "
    "only exception is the blank-line rules, which can add or remove empty lines."
)
CFM += p(
    "This boundary is what makes the formatter safe to run on a file you did not write."
)

CFM += h2("How it protects your code")
CFM += p("Every format runs two checks before anything reaches your buffer:")
CFM += ol([
    "<strong>Token equivalence.</strong> The formatted text is broken back into tokens and "
    "compared with the original. If a single non-whitespace token differs, the format is "
    "rejected.",
    "<strong>Stability.</strong> The result is formatted a second time. If the second pass "
    "differs from the first, the format is rejected.",
], steps=True)
CFM += p(
    "If either check fails, your original text is left exactly as it was. A formatter that "
    "cannot prove it preserved your code does not touch it."
)

CFM += h2("The rules")
CFM += p(
    "Configure these in %s, which shows a live preview using the real "
    "formatting engine, so you can see the effect of a rule before committing to it."
    % menu("Edit", "Format", "Format Options…")
)
CFM += table(
    ["Rule", "Effect"],
    [
        ("Reindent", "Recompute the indentation of every line from block structure."),
        ("Indent Case", "Indent <code>Case</code> labels inside a "
         "<code>Select Case</code> block."),
        ("Continuation indent", "How far to indent a continued line."),
        ("Case keywords", "Rewrite keywords to their canonical spelling from the keyword "
         "lists."),
        ("Case directives", "Rewrite preprocessor directives to canonical case."),
        ("Case types", "Rewrite type names to canonical case."),
        ("Space operators", "Ensure a space either side of binary operators."),
        ("Space after comma", "Ensure a space after each comma."),
        ("Parenthesis spacing", "Control spacing just inside parentheses."),
        ("Spaces before comment", "Minimum spacing before a trailing comment."),
        ("Trim trailing whitespace", "Remove spaces and tabs at the end of every line."),
        ("Maximum blank lines", "Collapse runs of blank lines to at most this many."),
        ("Blank lines around procedures", "Ensure separation between procedures."),
    ],
    key_first=True,
)

CFM += h2("Formatting as you work")
CFM += p("Two rules can run the formatter automatically:")
CFM += dl([
    ("Format on Enter",
     "Formats the line you just completed when you press " + kbd("Enter") + ". It only "
     "ever touches that one line, never a line below the caret."),
    ("Format on paste",
     "Formats text you paste so it matches the surrounding style rather than the style it "
     "came from."),
])
CFM += p("Both are off by default.")

CFM += h2("Formatting and undo")
CFM += p(
    "A format is a single undo action, separate from your typing — so %s rejects the "
    "format and keeps everything you typed before it. Formatting also makes minimal "
    "per-line edits rather than replacing the whole buffer, which is why your bookmarks, "
    "breakpoints and fold points survive it." % kbd("Ctrl", "Z")
)

CFM += h2("Format Project")
CFM += p(
    "Format Project formats every file in the workspace. Because a workspace's file set is "
    "the set of loaded documents, this formats open documents — including any whose tab "
    "you have closed — and writes nothing to disk on its own. Save afterwards as usual."
)

CFM += h2("Related topics")
CFM += ul([
    '<a href="indentation.html">Indentation</a>',
    '<a href="syntax-highlighting.html">Syntax highlighting</a> — where canonical keyword '
    "spellings come from.",
    '<a href="dialog-reference.html#format-options">Format Options dialog</a>',
])

page("code-formatting", "Code formatting", "productivity",
     "The configurable code formatter: the four commands, the rules, the guarantees it "
     "makes about your source, and formatting automatically on Enter or paste.",
     CFM,
     keywords="format formatter prettify beautify reindent format document format "
              "selection format project keyword case spacing blank lines trim trailing "
              "whitespace format on enter format on paste")
