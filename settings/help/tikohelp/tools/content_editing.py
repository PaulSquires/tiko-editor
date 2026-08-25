# -*- coding: utf-8 -*-
"""Editing Text section."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   placeholder, diagram)

section("editing", "Editing Text", "edit")

# ==========================================================================
# Editing basics
# ==========================================================================

EB = ""
EB += p(
    "Tiko Editor's editing surface is built on Scintilla, so if you have used Notepad++, SciTE or "
    "any other Scintilla-based editor, the fundamentals will already feel familiar."
)

EB += h2("Moving the caret")
EB += table(
    ["Movement", "Keys"],
    [
        ("One character / line", kbd("←") + " " + kbd("→") + " " + kbd("↑") + " " + kbd("↓")),
        ("One word", kbd("Ctrl", "←") + " / " + kbd("Ctrl", "→")),
        ("Start / end of line", kbd("Home") + " / " + kbd("End")),
        ("Start / end of document", kbd("Ctrl", "Home") + " / " + kbd("Ctrl", "End")),
        ("One screen", kbd("PgUp") + " / " + kbd("PgDn")),
        ("Next / previous procedure", kbd("Ctrl", "PgDn") + " / " + kbd("Ctrl", "PgUp")),
        ("Matching brace", "See " + '<a href="brace-matching.html">Brace matching</a>.'),
    ],
    key_first=True,
)
EB += note(
    "%s is smart: it moves to the first non-blank character of the line, and pressing it "
    "again moves to column one. This is what you want almost every time on indented code."
    % kbd("Home")
)
EB += p(
    "The <strong>Confine caret</strong> option keeps the caret inside the text rather than "
    "letting it move into the empty space past the end of a line. It is on by default. See "
    '<a href="configuration-reference.html">Configuration reference</a>.'
)

EB += h2("Selecting text")
EB += h3("With the keyboard")
EB += p(
    "Hold %s and use any movement key. The selection extends from where the caret was to "
    "where it lands, so %s selects to the end of the document."
    % (kbd("Shift"), kbd("Ctrl", "Shift", "End"))
)
EB += h3("With the mouse")
EB += ul([
    "<strong>Drag</strong> to select a range.",
    "<strong>Double-click</strong> selects a word.",
    "<strong>Triple-click</strong> selects a line.",
    "<strong>Click, then Shift-click</strong> elsewhere to select everything between.",
    "<strong>Click in the left margin</strong> to select that line.",
])
EB += h3("Selection commands")
EB += table(
    ["Command", "Shortcut", "Notes"],
    [
        ("Select All", kbd("Ctrl", "A"), "The whole document."),
        ("Select Line", kbd("Ctrl", "L"),
         "The current line including its line ending. Repeat to extend downward."),
    ],
    key_first=True,
)

EB += h2("Clipboard")
EB += table(
    ["Command", "Shortcut", "Behaviour"],
    [
        ("Cut", kbd("Ctrl", "X"), "With no selection, cuts the whole current line."),
        ("Copy", kbd("Ctrl", "C"), "With no selection, copies the whole current line."),
        ("Paste", kbd("Ctrl", "V"),
         "Replaces the selection if there is one, otherwise inserts at the caret."),
        ("Delete Line", kbd("Ctrl", "Y"), "Removes the line entirely."),
    ],
    key_first=True,
)
EB += tip(
    "Cut and Copy acting on the whole line when nothing is selected is a real time-saver: "
    "to move a line, put the caret on it, press %s, move, and press %s."
    % (kbd("Ctrl", "X"), kbd("Ctrl", "V"))
)
EB += p(
    "Pasting into the editor pastes plain text. Rich formatting from a word processor or "
    "web page is discarded, so pasted code arrives as code."
)

EB += h2("Undo and redo")
EB += p(
    "%s undoes and %s redoes, with no practical limit within an editing session. Undo "
    "history belongs to a document and is discarded when you close it."
    % (kbd("Ctrl", "Z"), kbd("Ctrl", "Shift", "Z"))
)
EB += important(
    "Some operations that rewrite an entire buffer — notably changing a document's "
    "encoding — can clear the undo history. Save before you change encoding."
)

EB += h2("Inserting")
EB += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Insert File", kbd("Ctrl", "I"),
         "Inserts the contents of a file at the caret."),
        ("New line below", kbd("Ctrl", "Enter"),
         "Opens a new line beneath the current one and moves there, wherever the caret "
         "is on the line."),
        ("Overwrite mode", kbd("Insert"),
         "Toggles between inserting and overwriting. Nothing in the interface reports "
         "which mode you are in — watch what typing does, or press " + kbd("Insert") +
         " again if the caret starts replacing characters."),
    ],
    key_first=True,
)

EB += h2("Related topics")
EB += ul([
    '<a href="multiple-selections.html">Multiple selections and column mode</a>',
    '<a href="line-operations.html">Line and block operations</a>',
    '<a href="indentation.html">Indentation</a>',
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
])

page("editing-basics", "Editing basics", "editing",
     "Caret movement, selecting text, the clipboard, undo and redo, and insert mode.",
     EB,
     keywords="edit caret cursor movement selection select all select line clipboard cut "
              "copy paste undo redo insert overwrite home end")

# ==========================================================================
# Multiple selections / column mode
# ==========================================================================

MS = ""
MS += p(
    "Tiko Editor supports editing in more than one place at once. There are two related "
    "mechanisms, and knowing which one you want saves a lot of fiddling."
)

MS += h2("Column (rectangular) selection")
MS += p(
    "A column selection covers a rectangle of characters rather than a run of text. It is "
    "the right tool when the thing you want to change lines up vertically."
)
MS += table(
    ["Method", "How"],
    [
        ("Mouse", "Hold " + kbd("Alt") + " and drag."),
        ("Keyboard", "Hold " + kbd("Alt", "Shift") + " and use the arrow keys."),
    ],
    key_first=True,
)
MS += p(
    "With a column selected, typing replaces the rectangle on every line at once, and "
    "%s deletes it. A <em>zero-width</em> column selection — one made without moving "
    "horizontally — gives you a caret on each line, so typing inserts the same text into "
    "every one." % kbd("Delete")
)
MS += code("""
' Before: select the zero-width column at the start of these lines
Dim As Integer width
Dim As Integer height
Dim As Integer depth

' Then type "Static " once:
Static Dim As Integer width
Static Dim As Integer height
Static Dim As Integer depth
""", lang="fb", title="Column editing")

MS += h2("Multi-cursor editing")
MS += p(
    "Multiple selections are independent of one another — unlike a column selection, they "
    "need not line up. Each carries its own caret, and typing goes into all of them at "
    "once."
)
MS += table(
    ["Action", "How"],
    [
        ("Add another caret", "Hold " + kbd("Ctrl") + " and click."),
        ("Add another selection", "Hold " + kbd("Ctrl") + " and drag."),
        ("Add a whole word", "Hold " + kbd("Ctrl") + " and double-click it."),
        ("Type into every caret", "Just type — each caret receives what you type."),
        ("Delete at every caret", kbd("Backspace") + " or " + kbd("Delete") + "."),
        ("Collapse back to one caret", "Click anywhere without " + kbd("Ctrl") + "."),
    ],
    key_first=True,
)
MS += p(
    "Word movement, %s and %s act at each caret independently, so carets sitting at "
    "different offsets on different lines still do the right thing individually while you "
    "edit them together." % (kbd("Home"), kbd("End"))
)
MS += p(
    "Additional carets and selections are drawn in their own theme colour so they stay "
    'visible against your background. See <a href="theme-editor.html">The theme '
    "editor</a>."
)
MS += note(
    "Tiko Editor has no \"select next occurrence\" command that grows a multi-selection one match "
    "at a time — you place the carets yourself with %s. To change every occurrence of "
    'something, use <a href="find-replace.html">Replace</a>: beyond a handful of places it '
    "is both quicker and reviewable." % kbd("Ctrl")
)
MS += tip(
    "Multiple selections shine for renaming a local variable within one procedure: select "
    "each occurrence with %s-drag and type the new name once. For a project-wide rename, "
    'use <a href="find-replace.html">Replace</a> instead — it is safer and reviewable.'
    % kbd("Ctrl")
)

MS += h2("Which should I use?")
MS += table(
    ["Task", "Use"],
    [
        ("Add a prefix or suffix to a run of consecutive lines", "Column selection"),
        ("Delete a vertical strip, such as an old indent level", "Column selection"),
        ("Edit occurrences scattered around the file", "Multiple selections"),
        ("Change the same word everywhere in a file or project", "Find and Replace"),
    ],
)

MS += h2("Related topics")
MS += ul([
    '<a href="editing-basics.html">Editing basics</a>',
    '<a href="find-replace.html">Find and Replace</a>',
    '<a href="line-operations.html">Line and block operations</a>',
])

page("multiple-selections", "Multiple selections and column mode", "editing",
     "Editing in several places at once: rectangular column selections and independent "
     "multiple carets.",
     MS,
     keywords="multiple selections multi cursor multi caret column mode rectangular "
              "selection alt drag vertical edit")

# ==========================================================================
# Indentation
# ==========================================================================

IN = ""
IN += p(
    "Indentation settings are per-installation rather than per-file, and live in "
    "%s." % menu("File", "Settings", "Options…")
)

IN += h2("Tabs versus spaces")
IN += p(
    "The <strong>Tab indents with spaces</strong> setting decides what the %s key actually "
    "inserts:" % kbd("Tab")
)
IN += table(
    ["Setting", "Pressing Tab inserts", "Use when"],
    [
        ("On (default)", "Spaces, up to the next tab stop.",
         "You want the file to look identical everywhere, regardless of the reader's "
         "tab width."),
        ("Off", "A literal tab character.",
         "Your project's convention is tabs, or you are matching an existing file."),
    ],
)
IN += p(
    "<strong>Tab size</strong> sets how many columns a tab stop occupies. The default is 4."
)
IN += important(
    "Changing these settings affects what you type from now on. It does not convert what "
    'is already in the file — use the <a href="code-formatting.html">formatter</a> for that.'
)

IN += h2("Indenting blocks")
IN += table(
    ["Command", "Shortcut", "Behaviour"],
    [
        ("Indent block", kbd("Tab"),
         "With a multi-line selection, indents every selected line by one level."),
        ("Unindent block", kbd("Shift", "Tab"),
         "Removes one level of indent from every selected line."),
    ],
    key_first=True,
)
IN += note(
    "With no selection, %s inserts an indent at the caret as usual. The block behaviour "
    "only applies when lines are selected." % kbd("Tab")
)

IN += h2("Automatic indentation")
IN += p(
    "With <strong>Auto indentation</strong> on — the default — pressing %s carries the "
    "current line's indent forward to the new line, so you stay in position while typing "
    "nested code." % kbd("Enter")
)
IN += p(
    "Tiko Editor goes further for FreeBASIC. When you press %s after a line that opens a block, "
    "it can insert the matching closing statement for you and place the caret between "
    "them:" % kbd("Enter")
)
IN += code("""
' Type this and press Enter at the end of the line:
If count > 0 Then

' Tiko Editor produces:
If count > 0 Then
    |          <- caret lands here
End If
""", lang="fb", title="Automatic block completion")
IN += p(
    "This applies to <code>If</code>, <code>For</code>, <code>Do</code>, "
    "<code>While</code>, <code>Select Case</code>, <code>Sub</code>, "
    "<code>Function</code>, <code>Type</code>, <code>Scope</code> and the other block "
    "constructs. Tiko Editor scans forward from the line you are on to check whether the block "
    "is <em>already</em> closed, and inserts nothing if it is."
)
IN += p(
    "Two related options control the detail:"
)
IN += ul([
    "<strong>Auto complete</strong> — turn this off and %s carries the indent forward but "
    "inserts no closing statement." % kbd("Enter"),
    "<strong>For/Next variable</strong> — when a <code>For</code> block is completed, "
    "include the loop variable on the <code>Next</code> line.",
])

IN += h2("Indent guides")
IN += p(
    "Indent guides are faint vertical lines showing each indent level, which makes deeply "
    "nested code much easier to read. Turn them on in the options dialog; their colour "
    "comes from the current theme."
)

IN += h2("Converting existing indentation")
IN += p(
    "To change the indentation of code that already exists, use the formatter: "
    "%s reindents the whole file according to your current settings, and "
    "%s does the same for just the selected lines."
    % (menu("Edit", "Format", "Format Document"), menu("Edit", "Format", "Format Selection"))
)
IN += p(
    'The formatter can also trim trailing whitespace. See '
    '<a href="code-formatting.html">Code formatting</a>.'
)

IN += h2("Related topics")
IN += ul([
    '<a href="code-formatting.html">Code formatting</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
    '<a href="editing-basics.html">Editing basics</a>',
])

page("indentation", "Indentation", "editing",
     "Tabs versus spaces, tab size, block indenting, automatic indentation and block "
     "completion, and indent guides.",
     IN,
     keywords="indent indentation tabs spaces tab size auto indent smart indent block "
              "indent unindent indent guides for next variable auto complete")

# ==========================================================================
# Encoding
# ==========================================================================

EN = ""
EN += p(
    "Every text file is a sequence of bytes, and an <em>encoding</em> is the rule for "
    "turning those bytes into characters. Choose the wrong one and accented letters, "
    "symbols and non-Latin scripts come out as garbage."
)

EN += h2("Encodings Tiko Editor works with")
EN += p(
    "Four, and the names below are exactly what the status bar shows:"
)
EN += table(
    ["Shown as", "Encoding", "Notes"],
    [
        ("<code>UTF-8</code>", "UTF-8, no byte-order mark",
         "The modern default. Represents every Unicode character and is byte-compatible "
         "with ASCII for plain English text. Use this unless you have a reason not to."),
        ("<code>UTF-8 (BOM)</code>", "UTF-8 with a byte-order mark",
         "The same encoding, preceded by a marker identifying it. Some tools require the "
         "marker; others choke on it."),
        ("<code>UTF-16 (BOM)</code>", "UTF-16 with a byte-order mark",
         "Two bytes per character. Common in Windows APIs, less so in source files. Tiko Editor "
         "always writes the byte-order mark for UTF-16 — there is no BOM-less variant."),
        ("<code>ANSI</code>", "The system code page",
         "The legacy Windows encoding for your locale. Fine for plain ASCII, unreliable "
         "for anything else."),
    ],
    key_first=True,
)

EN += h2("Seeing and changing the encoding")
EN += p(
    "The status bar shows the current document's encoding. Click it to convert the "
    "document to a different encoding."
)
EN += warn(
    "Converting to a narrower encoding can lose characters permanently. Converting a UTF-8 "
    "file containing, say, Greek text to ANSI will destroy that text. Save a copy first if "
    "you are unsure."
)
EN += p("Two settings govern the defaults:")
EN += dl([
    ("New file encoding", "The encoding given to files you create."),
    ("Unicode encoding", "How Tiko Editor treats files whose encoding it cannot determine."),
])

EN += h2("Line endings")
EN += p("Text files mark the end of a line in one of three ways:")
EN += table(
    ["Style", "Bytes", "Convention"],
    [
        ("CRLF", "<code>0D 0A</code>", "Windows. The default for new files."),
        ("LF", "<code>0A</code>", "Unix, Linux, macOS. Common in cross-platform projects "
         "and in Git repositories."),
        ("CR", "<code>0D</code>", "Classic Mac OS. Rare today."),
    ],
    key_first=True,
)
EN += p(
    "The status bar shows the current style; click it to convert the whole document. Tiko Editor "
    "detects the style when it opens a file and preserves it on save, so opening a "
    "Unix-style file and saving it does not silently rewrite every line."
)
EN += tip(
    "Mixed line endings in one file cause odd-looking diffs and confuse some compilers. If "
    "a file behaves strangely, converting it to a single consistent style is a cheap fix."
)

EN += h2("Unicode in source code")
EN += p(
    "FreeBASIC source is normally plain ASCII, but string literals and comments may "
    "contain anything. If your program handles non-English text, save the source as UTF-8 "
    "and be explicit in code about how those strings are encoded at runtime — the file "
    "encoding and the runtime encoding are separate questions."
)

EN += h2("Related topics")
EN += ul([
    '<a href="troubleshooting.html">Troubleshooting</a> — fixing files that display '
    "incorrectly.",
    '<a href="configuration-reference.html">Configuration reference</a>',
    '<a href="glossary.html">Glossary</a>',
])

page("encoding", "Encoding and line endings", "editing",
     "Character encodings, UTF-8 and Unicode, byte-order marks, and the three line-ending "
     "conventions — how to see and change each.",
     EN,
     keywords="encoding utf-8 utf8 unicode ansi bom byte order mark codepage line endings "
              "crlf lf cr newline convert")

# ==========================================================================
# View options
# ==========================================================================

VW = ""
VW += p(
    "These options change how your code is displayed without changing a byte of it. Most "
    "live in %s; the zoom commands are on the View menu."
    % menu("File", "Settings", "Options…")
)

VW += h2("Margins")
VW += table(
    ["Margin", "Shows", "Setting"],
    [
        ("Line numbers", "The number of each line.", "Line numbering"),
        ("Left margin", "A narrow gutter for bookmarks and breakpoint markers.",
         "Left margin"),
        ("Fold margin", "Fold markers beside collapsible blocks.", "Fold margin"),
    ],
    key_first=True,
)
VW += p(
    "Click the left margin beside a line to select it. If <strong>Click toggles "
    "breakpoint</strong> is on, clicking the margin sets or clears a breakpoint instead — "
    'convenient while debugging. See <a href="breakpoints.html">Breakpoints</a>.'
)

VW += h2("Visual guides")
VW += dl([
    ("Highlight current line",
     "Tints the line the caret is on, so you never lose it in a large file."),
    ("Right edge",
     "Draws a vertical line at a chosen column — 80 by default — as a reminder of your "
     "preferred maximum line length. Turn it on with <strong>Right edge</strong> and set "
     "the column with <strong>Right edge position</strong>."),
    ("Indent guides",
     "Faint vertical lines marking each level of indentation."),
    ("Brace highlighting",
     "Highlights the brace or bracket matching the one beside the caret, and marks "
     "unmatched ones. See <a href=\"brace-matching.html\">Brace matching</a>."),
    ("Occurrence highlighting",
     "Highlights every other occurrence of the word under the caret."),
])

VW += h2("Zoom")
VW += table(
    ["Command", "Shortcut"],
    [
        ("Zoom In", kbd("Ctrl", "+")),
        ("Zoom Out", kbd("Ctrl", "-")),
        ("Zoom Reset", kbd("Ctrl", "0")),
    ],
    key_first=True,
)
VW += p(
    "Zoom is a temporary display change — it does not alter the configured font size, and "
    "%s returns to it. %s and the scroll wheel zoom too." % (kbd("Ctrl", "0"), kbd("Ctrl"))
)
VW += tip(
    "Zoom out to get an overview of a file's shape when you are looking for a particular "
    "block, then reset. It is faster than scrolling."
)

VW += h2("Scrolling")
VW += ul([
    "The mouse wheel scrolls vertically; %s and the wheel scrolls horizontally."
    % kbd("Shift"),
    "The <strong>Position middle</strong> option keeps the caret line vertically centred "
    "when you jump to a line, rather than leaving it at the very top or bottom of the view.",
    "Both scrollbars are drawn by the editor and follow your theme.",
])

VW += h2("Minimap")
VW += p(
    "<strong>Tiko Editor has no minimap</strong> — the miniature overview of a whole file that "
    "some editors show beside the scrollbar. For an overview of a file's structure, use "
    "<strong>Fold All</strong> (" + kbd("Shift", "F8") + ") or the function list (" +
    kbd("F4") + ") instead; both are quicker to read than a thumbnail of the text."
)

VW += h2("Related topics")
VW += ul([
    '<a href="code-folding.html">Code folding</a>',
    '<a href="syntax-highlighting.html">Syntax highlighting</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("view-options", "Display and view options", "editing",
     "Margins, line numbers, indent guides, the right-edge marker, zoom and scrolling.",
     VW,
     keywords="view display margins line numbers fold margin right edge highlight current "
              "line indent guides zoom scroll no minimap occurrence highlight")

# ==========================================================================
# Code folding
# ==========================================================================

CF = ""
CF += p(
    "Folding collapses a block of code to a single line so you can hide detail you are not "
    "working on. Nothing is removed — folding is purely a display state."
)

CF += h2("Folding with the mouse")
CF += p(
    "With the fold margin enabled, a marker appears beside every foldable block. Click it "
    "to collapse or expand that block. A collapsed block shows a marker you can click "
    "again to reopen it."
)

CF += h2("Folding commands")
CF += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Toggle Current Fold Point", kbd("F8"),
         "Collapses or expands the block containing the caret."),
        ("Toggle Current And All Below", kbd("Ctrl", "F8"),
         "Folds the current block and every block nested inside it."),
        ("Fold All", kbd("Shift", "F8"),
         "Collapses every block in the document — an instant outline of the file."),
        ("Unfold All", kbd("Ctrl", "Shift", "F8"), "Expands everything."),
    ],
    key_first=True,
)
CF += tip(
    "%s followed by expanding just the procedure you want is a fast way to orient "
    "yourself in an unfamiliar file. The %s function list does the same job without "
    "changing the view." % (kbd("Shift", "F8"), kbd("F4"))
)
CF += note(
    "Which constructs are foldable comes from the syntax lexer for the file's language. "
    "For FreeBASIC that means procedures, types, and the usual block statements."
)
CF += p(
    "Fold state is a view property, not part of the file. Searching finds text inside "
    "folded blocks, and jumping to a line inside one expands it automatically."
)

CF += h2("Related topics")
CF += ul([
    '<a href="view-options.html">Display and view options</a>',
    '<a href="navigation.html">Navigation</a>',
])

page("code-folding", "Code folding", "editing",
     "Collapsing and expanding blocks of code to hide detail, with the mouse or the "
     "keyboard.",
     CF,
     keywords="fold folding collapse expand outline fold all unfold all fold margin "
              "fold point")

# ==========================================================================
# Bookmarks
# ==========================================================================

BM = ""
BM += p(
    "A bookmark marks a line you want to come back to. Bookmarks are ideal while working "
    "across several places in a large file — set one at each, then cycle between them."
)

BM += h2("Commands")
BM += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Toggle Bookmark", kbd("Ctrl", "F2"),
         "Sets a bookmark on the current line, or clears the one already there."),
        ("Next Bookmark", kbd("F2"), "Jumps to the next bookmark, wrapping at the end."),
        ("Previous Bookmark", kbd("Shift", "F2"), "Jumps to the previous one."),
        ("Clear Bookmarks", kbd("Ctrl", "Shift", "F2"), "Removes them all."),
        ("View Bookmarks List", kbd("Shift", "F4"),
         "Shows every bookmark in the side panel."),
    ],
    key_first=True,
)
BM += p("A bookmarked line is marked in the left margin.")

BM += h2("The bookmarks list")
BM += p(
    "The bookmarks panel lists every bookmark with its file and line. Click an entry to "
    "jump there. Because it spans files, it doubles as a lightweight to-do list for a "
    "task that touches several places in the project."
)

BM += h2("Bookmarks and TODO comments")
BM += p(
    "Bookmarks and TODO comments solve related problems in different ways:"
)
BM += table(
    ["", "Bookmarks", "TODO comments"],
    [
        ("Stored", "In the editor's session state.", "In the source file itself."),
        ("Visible to others", "No.", "Yes — anyone reading the code sees them."),
        ("Where listed", "Bookmarks panel.", "Output panel, TODO tab."),
        ("Best for", "Navigating while you work.", "Recording work that still needs doing."),
    ],
)
BM += tip(
    "Use bookmarks for the next ten minutes and TODO comments for next week."
)

BM += h2("Related topics")
BM += ul([
    '<a href="navigation.html">Navigation</a>',
    '<a href="side-panels.html">Side panels</a>',
    '<a href="output-panel.html">Output panel</a> — the TODO list.',
])

page("bookmarks", "Bookmarks", "editing",
     "Marking lines to return to, cycling through them, and the bookmarks list panel.",
     BM,
     keywords="bookmark bookmarks toggle next previous clear list marker margin todo")
