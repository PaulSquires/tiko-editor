# -*- coding: utf-8 -*-
"""Searching, Navigation and Editing Productivity sections."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   figure_img, placeholder, diagram)

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

FR += h2("The Find bar")
FR += figure_img(
    "assets/img/find-bar.png",
    "The Find bar. Six icons flank the search box; three of them latch on and stay lit.",
    alt="The Tiko Find bar")
FR += table(
    ["Icon", "What it does", "Latches?"],
    [
        ("Match Case", "Require the same capitalisation. Off by default, which suits "
         "FreeBASIC's case-insensitive keywords.", "Yes"),
        ("Match Whole Words", "Match complete words only: <code>count</code> will not "
         "match <code>counter</code> or <code>rowcount</code>.", "Yes"),
        ("Selection", "Search only within the selected text rather than the whole "
         "document.", "Yes"),
        ("Toggle Replace", "Show or hide the replacement field, turning the Find bar into "
         "the Replace bar.", "No"),
        ("Search Previous", "Jump to the previous match (" + kbd("Shift", "F3") + ").",
         "No"),
        ("Search Next", "Jump to the next match (" + kbd("F3") + ").", "No"),
    ],
    key_first=True,
)
FR += p(
    "A latching icon stays lit while its option is on; click it again to turn it off. "
    "The three latching options are the whole of Tiko's matching behaviour — there is "
    "nothing else to configure."
)

FR += h3("Searching within a selection")
FR += p(
    "The <strong>Selection</strong> option confines the search to the text you have "
    "selected, which is the safe way to run a Replace All over one procedure instead of a "
    "whole file."
)
FR += important(
    "Tiko sets this up for you. Open the Find bar with a <strong>multi-line</strong> "
    "selection and Selection switches on automatically with the search box left empty — "
    "because \"search inside this selection\" begins with no search term by definition. "
    "Open it with a <strong>single-line</strong> selection and that text becomes the "
    "search term instead."
)
FR += note(
    "Closing the Find bar clears the Selection latch along with the highlighting, so it "
    "cannot come back lit against a selection that no longer exists."
)

FR += important(
    "<strong>Tiko does not support regular expressions.</strong> Searches are literal "
    "text, refined by the three options above. If you need pattern matching — extracting "
    "captured groups, matching optional or repeated text — do it in a tool built for it "
    "and bring the results back."
)

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
    '<a href="multiple-selections.html">Multi-cursor editing</a> — an alternative for '
    "small, local edits.",
])

page("find-replace", "Find and Replace", "searching",
     "Searching within the current document, the search options, replacing matches, and "
     "search history.",
     FR,
     keywords="find search replace replace all find next find previous f3 incremental "
              "match case whole word match whole words selection search selection toggle "
              "replace find bar icons no regular expressions")

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
    "Find in Project uses the <strong>same two matching options as Find</strong>, and "
    "nothing more:"
)
FP += table(
    ["Option", "Effect"],
    [
        ("Match Case", "Require the same capitalisation."),
        ("Match Whole Words", "Match complete words only, not fragments inside longer "
         "ones."),
    ],
    key_first=True,
)
FP += important(
    "<strong>There are no file filters.</strong> No masks, no folder scope, no "
    "include/exclude patterns — and no regular expressions. Find in Project searches every "
    "file in the workspace, every time, for literal text."
)
FP += p(
    "That makes the workspace itself the only scope control you have. If a search returns "
    "more than you want, the answer is a more specific search term, or a project holding "
    'fewer files — see <a href="projects-overview.html">Projects and workspaces</a>.'
)
FP += figure_img(
    "assets/img/find-in-project.png",
    "Find in Project searches every file in the workspace and collects the matches in the "
    "Output panel.",
    alt="The Find in Project dialog")

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
FP += p(
    "A very large result set is capped. When that happens the match count is shown with a "
    "<code>+</code> after it, meaning \"at least this many\" — narrow the search term to "
    "see them all."
)

FP += h2("Typical uses")
FP += ul([
    "<strong>Before renaming anything.</strong> Search for the identifier first to see "
    "every place it appears, including comments and strings that a rename would miss.",
    "<strong>Finding where a procedure is called from.</strong> Goto Definition (%s) goes "
    "the other way; this finds the callers." % kbd("F12"),
    "<strong>Auditing.</strong> Search for a term such as <code>TODO</code>, "
    "<code>HACK</code> or a deprecated API name across the whole codebase.",
])
FP += tip(
    "Tiko already collects <code>TODO</code> comments automatically into the Output "
    "panel's TODO tab — you do not need to search for those."
)

FP += h2("Related topics")
FP += ul([
    '<a href="find-replace.html">Find and Replace</a>',
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

NV += h2("Search Symbol — the fuzzy finder")
NV += p(
    "%s opens Search Symbol, a popup that appears just under the tab strip. It is Tiko's "
    "universal \"go to anything\" command, and it is a <strong>fast fuzzy finder</strong> "
    "rather than a plain substring filter." % kbd("Ctrl", "P")
)

NV += h3("What it searches")
NV += p("One list, built from two sources and ranked together:")
NV += ul([
    "<strong>Every file the symbol database knows about</strong> — including include "
    "files you have never opened in the editor. This is a superset of the files with tabs "
    "open.",
    "<strong>Every procedure and type in the project</strong> — subs, functions, types and "
    "enumerations, shown by their qualified name, so a method inside a type appears under "
    "its full name rather than on its own.",
])
NV += p(
    "Leave the box empty and it simply lists all the files, which makes it a file switcher "
    "as well as a symbol finder."
)

NV += h3("How fuzzy matching works")
NV += p(
    "You do not have to type a contiguous piece of the name. The characters you type need "
    "only appear <strong>in order</strong> somewhere in it — so <code>fpm</code> finds "
    "<code>frmPanelMenu</code>, and <code>sdoc</code> finds "
    "<code>SaveDocument</code>."
)
NV += p(
    "Every candidate is then <strong>scored</strong>, and the list is sorted best-first. "
    "The scoring favours matches that look deliberate:"
)
NV += ul([
    "characters at the start of a word, or just after an underscore or a capital, count "
    "for more than characters in the middle of one;",
    "consecutive matched characters score better than scattered ones;",
    "a shorter name matching the same letters outranks a longer one.",
])
NV += p(
    "In the results, <strong>the characters your search matched are highlighted</strong> "
    "within each name, so you can see at a glance why a row is in the list and whether the "
    "top hit is the one you meant."
)
NV += tip(
    "Type the initials of a camel-case or underscore-separated name. Three or four "
    "well-chosen letters usually put the item you want at the top of the list — that is "
    "what fuzzy matching is for, and it is much faster than typing a prefix."
)
NV += note(
    "The result list is capped, so an extremely broad search shows the best matches rather "
    "than everything. Type another character or two and the item you want rises into view."
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

NV += h2("Going to a line number")
NV += important(
    "<strong>There is no Goto Line dialog.</strong> Tiko has no command that asks you to "
    "type a line number, because in practice you never need one — everything that knows "
    "about a line number takes you there directly."
)
NV += table(
    ["You have", "Do this"],
    [
        ("A compiler error or warning",
         "Click the row in the Output panel. It opens the file and puts the caret on the "
         "line."),
        ("A search result from Find in Project",
         "Click the row. Same behaviour."),
        ("A TODO comment", "Click it in the Output panel's TODO tab."),
        ("A procedure or type name", "Search Symbol (" + kbd("Ctrl", "P") + "), or " +
         kbd("F12") + " on the name itself."),
        ("A bookmark", kbd("F2") + " cycles through them; the bookmarks list shows the "
         "file and line of each."),
        ("A rough idea of where it is",
         "The function list (" + kbd("F4") + ") to jump by procedure, or " +
         kbd("Ctrl", "PgDn") + " / " + kbd("Ctrl", "PgUp") + " to step between them."),
    ],
)
NV += p(
    "The status bar always shows the current line and column, so you can still tell "
    "someone else where you are."
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

SY += h2("The Unused Symbols report")
SY += p(
    "%s scans the project and reports every symbol that was declared but never read. It is "
    "a periodic tidy-up tool: dead procedures, leftover variables, parameters nothing "
    "passes, stale types." % menu("Debug", "Unused Symbols…")
)

SY += h3("The three statuses")
SY += p(
    "A row only appears for a symbol with <strong>zero reads</strong>, but there are three "
    "quite different reasons that can happen, and the report distinguishes them."
)
SY += table(
    ["Status", "Means", "What to do"],
    [
        ("<strong>Dead</strong>", "No references at all — never read, never written.",
         "Usually safe to delete. The clearest win in the report."),
        ("<strong>Write-only</strong>",
         "Assigned to, but its value is never read afterwards.",
         "Often a real bug rather than dead code: a result computed and then dropped, or "
         "an assignment to the wrong variable."),
        ("<strong>Unknown</strong>",
         "The reference counts cannot be trusted for this symbol — it is exported, part of "
         "an overloaded set, or a constructor or destructor.",
         "Shown for your judgement, never asserted to be dead. Check by hand."),
    ],
    key_first=True,
)
SY += important(
    "<strong>Write-only is the status worth reading carefully.</strong> Dead code is "
    "untidy; a variable that is written and never read is frequently a mistake that "
    "compiles cleanly and runs wrongly."
)

SY += h3("Filtering by kind")
SY += p(
    "Six toggles filter the report by what kind of thing each symbol is. The grouping is "
    "deliberately coarser than the compiler's — you think in terms of \"parameters\", not "
    "in terms of sub-parameters and function-parameters separately."
)
SY += table(
    ["Kind", "Covers"],
    [
        ("Variables", "Locals, module-level and global variables."),
        ("Procedures", "Subs and functions."),
        ("Parameters", "Procedure parameters nothing reads."),
        ("Types", "Type and union declarations."),
        ("Fields", "Members of a type."),
        ("Constants", "Named constants and enumeration members."),
    ],
    key_first=True,
)
SY += p(
    "Your choice of toggles is remembered between sessions. Turning off Parameters and "
    "Fields is a common first move — they are the noisiest categories on a large codebase."
)

SY += h3("Reading and sorting the list")
SY += p("The report has five columns:")
SY += table(
    ["Column", "Shows"],
    [
        ("File", "The file the symbol was declared in."),
        ("Line", "Its line number."),
        ("Class", "Which of the six kinds it is."),
        ("Name", "The symbol's name, qualified where it belongs to a type or namespace."),
        ("Status", "Dead, write-only or unknown."),
    ],
    key_first=True,
)
SY += ul([
    "<strong>Click a column header</strong> to sort by it. Line numbers and reference "
    "counts sort numerically rather than as text, so line 9 comes before line 10.",
    "Sorting is stable with a deterministic tiebreak, so re-clicking a header cannot "
    "shuffle rows that compare equal.",
    "A filter box matches case-insensitively against any displayed column.",
    "<strong>Click a row</strong> to open that file at the declaration.",
])

SY += h3("How far to trust it")
SY += warn(
    "Treat the report as a list of <strong>candidates</strong>, not a list of certainties. "
    "The counts come from one scan of the project, so a symbol referenced only from a file "
    "that scan never reached will read as unused."
)
SY += p("In particular, be careful with:")
SY += ul([
    "<strong>Exported procedures</strong> — used by callers outside this project "
    "entirely, which is why they are reported as <em>unknown</em> rather than dead.",
    "<strong>Anything reached through a function pointer</strong> or a callback, where no "
    "reference to the name appears at the call site.",
    "<strong>Code behind conditional compilation</strong> that this build excluded.",
    "<strong>Symbols used only from a resource script</strong> or another non-source "
    "file.",
])
SY += note(
    "If any open document has unsaved edits when you run the report, Tiko names those "
    "files and warns you that their line numbers may be stale — it reports rather than "
    "refusing to run. Save first if you intend to work from the line numbers."
)
SY += tip(
    "Run it before a release rather than during active work, sort by Status to bring the "
    "write-only rows together, and read those first."
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
    "Autocomplete offers completions drawn from the symbols Tiko has parsed out of your "
    "project plus the keyword lists for the current language. Some lists appear on their "
    "own as you type; the general word list you ask for with %s." % kbd("Ctrl", "Space")
)

AC += h2("Ctrl+Space — complete the word", anchor="ctrl-space")
AC += important(
    "<strong>%s is the keyboard trigger for the word completion list.</strong> Ordinary "
    "typing deliberately does not raise it, so if you want a completion for a partly typed "
    "identifier, this is how you ask for one." % kbd("Ctrl", "Space")
)
AC += p(
    "Type as much of a name as you care to — or none of it — and press %s. The list opens "
    "with everything that matches, and filters itself as you carry on typing."
    % kbd("Ctrl", "Space")
)
AC += p("Tiko only claims the keystroke when it is genuinely useful:")
AC += table(
    ["Situation", "What happens"],
    [
        ("The caret is in the editor", "The word list opens. This is the normal case."),
        ("The list is already open",
         "Nothing is rebuilt — it is already filtering as you type — but the keystroke is "
         "still consumed, so no stray character reaches your document."),
        ("Autocomplete is turned off in the options",
         "The keystroke is ignored, and " + kbd("Ctrl", "Space") + " does nothing."),
        ("The caret is in the Find box or the Notes pane",
         "Those are ordinary text fields and keep their own " + kbd("Ctrl", "Space") + "."),
        (kbd("Ctrl", "Alt", "Space"),
         "Left alone. On several keyboard layouts that combination is AltGr, so it belongs "
         "to the editor as a normal keystroke."),
    ],
)
AC += note(
    "Because Tiko consumes the keystroke rather than passing it on, %s never inserts a "
    "stray character — which is what an unclaimed one would otherwise do."
    % kbd("Ctrl", "Space")
)
AC += tip(
    "Press %s with no partial word typed to see everything available at that point. It is "
    "a quick way to remind yourself of a name you half-remember."
    % kbd("Ctrl", "Space")
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
    ("Auto complete", "Turns the completion list on and off. With it off, "
     + kbd("Ctrl", "Space") + " does nothing either."),
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

LO += h2("Commands Tiko does not have")
LO += p(
    "For completeness, because they exist in some other editors and are worth not hunting "
    "for: Tiko has <strong>no Sort Lines, Join Lines or Transpose Lines command</strong>, "
    "and no standalone Trim Trailing Whitespace command."
)
LO += p("Trailing whitespace is handled two other ways:")
LO += table(
    ["Route", "What it does", "Where"],
    [
        ("<strong>Strip line ending whitespace when saving</strong>",
         "Removes trailing spaces and tabs from every line each time you save. Set it "
         "once and forget it.",
         menu("File", "Settings", "Options…") + " ▸ Advanced Code Editor"),
        ("<strong>Trim trailing whitespace</strong>",
         "A formatter rule, applied when you run one of the Format commands.",
         menu("Edit", "Format", "Format Options…")),
    ],
    key_first=True,
)
LO += tip(
    "Turning on <strong>Strip line ending whitespace when saving</strong> is the "
    "low-effort option — it keeps a file clean without you ever running a command, and it "
    "keeps trailing-space noise out of your diffs."
)

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
              "select line uppercase lowercase mixed case no sort join transpose "
              "strip trailing whitespace on save")

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
    "Tiko picks a syntax scheme from the file's extension. There is no language selector on "
    "the status bar, so a file with an unusual extension is highlighted according to that "
    "extension."
)

SH += h2("Keyword lists")
SH += p(
    "The words highlighted as keywords come from three plain-text files in "
    "<code>settings" + chr(92) + "keywords</code>. Each has its own page in "
    + menu("File", "Settings", "Options…") + ", so you can edit them without leaving the "
    "editor."
)
SH += table(
    ["Options page", "File", "Holds"],
    [
        ("FreeBASIC Keywords", "<code>freebasic_keywords.txt</code>",
         "The FreeBASIC language itself — statements, types, operators and the "
         "<code>#</code> and <code>$</code> directives."),
        ("Windows API Keywords", "<code>winapi_keywords.txt</code>",
         "Win32 API function names, from <code>AbortDoc</code> onwards. Several thousand "
         "of them."),
        ("Extra Keywords", "<code>extra_keywords.txt</code>",
         "Everything else you want recognised — third-party library and framework names. "
         "This is the one to add your own to."),
    ],
    key_first=True,
)
SH += p(
    "The files are simply whitespace-separated lists of names, so they are easy to edit or "
    "generate. A fourth file, <code>freebasic_keywords_default.txt</code>, holds the "
    "shipped baseline so the FreeBASIC list can be restored if you edit it into a corner."
)
SH += p("Those lists do three jobs at once:")
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
SH += tip(
    "Adding your library's procedure names to <strong>Extra Keywords</strong> is one of "
    "the highest-value five minutes you can spend in Tiko: those names then colour "
    "correctly <em>and</em> appear in autocomplete throughout the project."
)
SH += note(
    "A separate file in the same folder, <code>codetips.ini</code>, supplies the parameter "
    "hints shown for built-in FreeBASIC keywords — entries such as "
    "<code>ABS=Abs(number)</code>. Your own procedures get their code tips from the parser "
    'instead. See <a href="autocomplete.html">Autocomplete and code tips</a>.'
)

SH += h2("Keyword case display")
SH += p(
    "The <strong>Keyword case</strong> setting controls how keywords are "
    "<em>displayed</em>. It is a display setting only: it never changes a byte in your file."
)
SH += table(
    ["Setting", "Keywords appear as"],
    [
        ("Original case", "Exactly as you typed them. The default."),
        ("Proper case", "The canonical spelling from the keyword lists — so "
         "<code>byval</code> displays as <code>ByVal</code> however you typed it."),
        ("Upper case", "<code>BYVAL</code>"),
        ("Lower case", "<code>byval</code>"),
    ],
    key_first=True,
)
SH += p(
    "<strong>Proper case</strong> is the interesting one: it lets you type in whatever case "
    "you find quickest and still read consistently-cased code, without a single byte of the "
    "file changing. That is why the spelling you enter in the keyword lists matters."
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
