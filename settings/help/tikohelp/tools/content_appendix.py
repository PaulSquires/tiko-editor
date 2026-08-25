# -*- coding: utf-8 -*-
"""Appendix: FAQ, tips, troubleshooting, glossary and the alphabetical index."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui, esc, faq,
                   placeholder, diagram)

section("appendix", "Help and Reference", "help")

# ==========================================================================
# FAQ
# ==========================================================================

FAQ_ITEMS = [
    # --- Getting started ---
    ("How do I install Tiko Editor?",
     "<p>Unpack the distribution into a folder you can write to and run "
     "<code>tiko-editor.exe</code>. There is no installer. Do not use "
     "<code>C:\\Program Files</code> — Tiko Editor writes its settings beside the executable, and "
     "that folder is write-protected.</p>"),
    ("Does Tiko Editor need .NET or any other runtime?",
     "<p>No. It is a native Win32 application. The DLLs it needs ship alongside it.</p>"),
    ("Can I run Tiko Editor from a USB stick?",
     "<p>Yes. Nothing is stored in the registry or your user profile, so the folder is "
     "self-contained. See <a href=\"settings-files.html\">Settings and configuration "
     "files</a>.</p>"),
    ("Where does Tiko Editor store my settings?",
     "<p>In the <code>settings</code> folder beside <code>tiko-editor.exe</code> — chiefly "
     "<code>settings.ini</code> and <code>keybindings.ini</code>.</p>"),
    ("Is Tiko Editor free? What licence is it under?",
     "<p>Tiko Editor is released under the GNU General Public License v3. See "
     + menu("Help", "About") + " for the full text and credits.</p>"),

    # --- Appearance ---
    ("How do I enable dark mode?",
     "<p>Press " + kbd("Ctrl", "Shift", "T") + " to open the Themes dialog and choose a "
     "dark theme — <code>default_dark</code>, <code>studio_dark</code>, "
     "<code>midnight</code> and several others. The whole application follows, not just "
     "the editor.</p>"),
    ("How do I change the font?",
     "<p>" + menu("File", "Settings", "Options…") + ", then the font settings. Set the "
     "name, size, character set and extra line spacing. Use a monospaced font.</p>"),
    ("The text is too small. Can I just make it bigger temporarily?",
     "<p>Yes — " + kbd("Ctrl", "+") + " and " + kbd("Ctrl", "-") + " zoom, and " +
     kbd("Ctrl", "0") + " resets. Zoom does not change the configured font size.</p>"),
    ("Can I make my own theme?",
     "<p>Yes. Select a theme you like, click <strong>Clone</strong>, then "
     "<strong>Edit</strong> the copy. Use the pencil icon beside the description to "
     "name it. See <a href=\"theme-editor.html\">The theme editor</a>.</p>"),
    ("Why are Edit and Delete greyed out for a theme?",
     "<p>You have <code>default_dark</code> or <code>default_light</code> selected. "
     "Those two are protected so the editor always has a theme to fall back on. Clone "
     "one and edit the copy instead — every other theme, including the twelve other "
     "shipped ones, can be edited directly.</p>"),
    ("How do I show line numbers?",
     "<p>Turn on <strong>Line numbering</strong> in " +
     menu("File", "Settings", "Options…") + ". It is on by default.</p>"),
    ("How do I show indent guides or a right-margin line?",
     "<p>Both are options in " + menu("File", "Settings", "Options…") + " — "
     "<strong>Indent guides</strong>, and <strong>Right edge</strong> with "
     "<strong>Right edge position</strong> for the column.</p>"),

    # --- Editing ---
    ("How do I edit several lines at once?",
     "<p>Two ways. Hold " + kbd("Alt") + " and drag for a <strong>column "
     "selection</strong> when the places line up vertically; hold " + kbd("Ctrl") + " and "
     "click, drag or double-click to add <strong>independent carets</strong> when they do "
     "not. Typing then goes into every caret at once. See "
     "<a href=\"multiple-selections.html\">Multi-cursor editing and column mode</a>.</p>"),
    ("Is there a “select next occurrence” command for multi-cursor editing?",
     "<p>No. You place each caret yourself with " + kbd("Ctrl") + "-click. For changing "
     "every occurrence of something, Replace is quicker and lets you review each "
     "change.</p>"),
    ("How do I comment out a block of code?",
     "<p>Select the lines and press " + kbd("Ctrl", "/") + ". " +
     kbd("Ctrl", "Shift", "/") + " uncomments.</p>"),
    ("How do I move a line up or down?",
     "<p>" + kbd("Alt", "↑") + " and " + kbd("Alt", "↓") + ". They move the whole "
     "selection if there is one.</p>"),
    ("How do I delete a line without selecting it?",
     "<p>" + kbd("Ctrl", "Y") + ". " + kbd("Ctrl", "D") + " duplicates one.</p>"),
    ("Does Tiko Editor use tabs or spaces?",
     "<p>Whichever you choose. <strong>Tab indents with spaces</strong> in the options "
     "decides what " + kbd("Tab") + " inserts, and <strong>Tab size</strong> sets the "
     "width. See <a href=\"indentation.html\">Indentation</a>.</p>"),
    ("How do I convert an existing file between tabs and spaces?",
     "<p>Set your preference, then run " + menu("Edit", "Format", "Format Document") +
     ". The formatter reindents the file to the current settings.</p>"),
    ("Can I reformat my code automatically?",
     "<p>Yes — the formatter can run on demand, on " + kbd("Enter") + ", or on paste. See "
     "<a href=\"code-formatting.html\">Code formatting</a>.</p>"),
    ("Will the formatter reorganise my code?",
     "<p>No. It never moves a line break, and it verifies that every token survived before "
     "it touches your buffer. If it cannot prove it preserved your code, it leaves the "
     "text alone.</p>"),

    # --- Searching and navigation ---
    ("How do I search across an entire project?",
     "<p>" + kbd("Ctrl", "Shift", "F") + " — Find in Project. Results appear in the "
     "Output panel, and clicking one jumps to it.</p>"),
    ("How do I repeat the last search?",
     "<p>" + kbd("F3") + " forwards, " + kbd("Shift", "F3") + " backwards. Neither needs "
     "the Find bar open.</p>"),
    ("Replace All changed things I did not want. What now?",
     "<p>Press " + kbd("Ctrl", "Z") + " — the whole Replace All is one undoable action. "
     "Then turn on <strong>Whole word</strong> and try again.</p>"),
    ("How do I jump to where a function is defined?",
     "<p>Put the caret on its name and press " + kbd("F12") + ". " + kbd("Alt", "←") +
     " brings you back.</p>"),
    ("How do I find a function when I do not know which file it is in?",
     "<p>" + kbd("Ctrl", "P") + " — Search Symbol. It is a fuzzy finder over every "
     "procedure, type <em>and</em> file in the project, so the letters you type only have "
     "to appear in order: <code>fpm</code> finds <code>frmPanelMenu</code>. Matches are "
     "ranked best-first with the matched characters highlighted. See "
     "<a href=\"navigation.html\">Navigation</a>.</p>"),
    ("Is there a Goto Line command?",
     "<p>No — Tiko Editor has no Goto Line dialog, because nothing needs one. Click a compiler "
     "error, a search result, a TODO entry or a bookmark and it takes you straight to the "
     "line. The status bar always shows where you are.</p>"),
    ("How do I get a completion list on demand?",
     "<p>Press " + kbd("Ctrl", "Space") + ". It is the only keyboard trigger for the word "
     "list — ordinary typing deliberately does not raise it. See "
     "<a href=\"autocomplete.html#ctrl-space\">Autocomplete</a>.</p>"),
    ("How do I see all the functions in the current file?",
     "<p>" + kbd("F4") + " opens the function list in the side panel.</p>"),
    ("How do I switch between a module and its header?",
     "<p>" + kbd("Ctrl", "Shift", "H") + " goes to the header and " +
     kbd("Ctrl", "Shift", "C") + " back to the code file.</p>"),
    ("Does Tiko Editor support regular expressions?",
     "<p><strong>No.</strong> Searches are literal text. The only matching options are "
     "Match Case, Match Whole Words and Selection, in both Find and Find in Project.</p>"),
    ("What are the icons on the Find bar?",
     "<p>Six: <strong>Match Case</strong>, <strong>Match Whole Words</strong>, "
     "<strong>Selection</strong>, <strong>Toggle Replace</strong>, <strong>Search "
     "Previous</strong> and <strong>Search Next</strong>. The first three latch on and stay "
     "lit; the other three act immediately.</p>"),
    ("How do I search only part of a file?",
     "<p>Select the text and turn on <strong>Selection</strong> on the Find bar. If you "
     "open Find with a multi-line selection already made, Tiko Editor turns Selection on for you "
     "and leaves the search box empty.</p>"),

    # --- Projects ---
    ("Do I have to create a project to use Tiko Editor?",
     "<p>No. Loose files already form an untitled project, saved automatically, so your "
     "work reopens next time. Naming it is optional.</p>"),
    ("How do I turn my open files into a real project?",
     "<p>Click the pinned <strong>Save as Project…</strong> row at the top of the "
     "Explorer, or choose " + menu("Project", "Save Project As…") + ".</p>"),
    ("I closed a tab. Did I remove the file from my project?",
     "<p>In a <strong>named</strong> project, no — closing a tab only stops displaying the "
     "file. It is still in the Explorer, still searched and still compiled. On an "
     "<strong>untitled</strong> workspace it is the opposite: closing the tab does take the "
     "file out, because there is no project to remove it from.</p>"),
    ("How do I remove a file from a project?",
     "<p>Right-click it in the Explorer — or right-click its tab if it is open — and choose "
     "<strong>Remove from project</strong>. The file is not deleted from disk. The command "
     "only appears once the project has a name.</p>"),
    ("Why is there no Remove from project command?",
     "<p>The workspace is still untitled, so there is no project to remove the file from. "
     "Save it as a project first, or just close the tab — on an untitled workspace that "
     "takes the file out of the Explorer.</p>"),
    ("What are the five groups in the Explorer?",
     "<p>Main, Resource, Header, Module and Normal. They are permanent, and every project "
     "file sits in exactly one of them. <strong>Main and Resource hold exactly one file "
     "each</strong> — the entry point and the resource script. The other three take any "
     "number, and can contain folders you create yourself.</p>"),
    ("Can I organise project files into folders?",
     "<p>Yes, within Header, Module and Normal. These are Tiko Editor's own folders, not folders "
     "on disk. Deleting one dissolves it — its contents move up to the parent — so nothing "
     "is ever lost.</p>"),
    ("How do I change which file is the main module?",
     "<p>Right-click the file — in the Explorer or on its tab — and choose <strong>Main "
     "file</strong>. You can also drag it onto the Main group in the Explorer. Whichever "
     "file was Main before is moved to <strong>Normal</strong>, not deleted.</p>"),
    ("How do I change a file's type or category?",
     "<p>Right-click it in the Explorer or on its tab and pick from <strong>Main "
     "file</strong>, <strong>Header file</strong>, <strong>Module file</strong>, "
     "<strong>Resource file</strong> or <strong>Normal file</strong> — the current one is "
     "marked. Dragging it between Explorer groups does the same. Select several files "
     "first to retype them all at once.</p>"),
    ("How do I add an existing file to a project?",
     "<p>" + menu("Project", "Add Files to Project…") + " (" + kbd("Ctrl", "F11") +
     "), or simply open it — opening a file adds it to the current workspace.</p>"),
    ("What is the difference between project options and build configurations?",
     "<p>Project options belong to one project — its include paths, libraries and "
     "command-line arguments. Build configurations are reusable sets of switches, such as "
     "debug versus release, shared across projects.</p>"),

    # --- Building ---
    ("Do I need to install FreeBASIC separately?",
     "<p>No. Tiko Editor ships with the most recent FreeBASIC toolchain already installed and "
     "selected, so you can build and run straight away.</p>"),
    ("How do I install an additional compiler toolchain?",
     "<p>Unpack it into its own subfolder of <code>toolchains\\</code> beside "
     "<code>tiko-editor.exe</code>, making sure both <code>fbc32.exe</code> and "
     "<code>fbc64.exe</code> sit in that folder's root. It then appears in the list on the "
     "Compiler page of " + menu("File", "Settings", "Options…") + ". You can install as "
     "many as you like and switch between them.</p>"),
    ("Tiko Editor says the compiler was not found.",
     "<p>Check the Compiler page of " + menu("File", "Settings", "Options…") + ": either "
     "no toolchain is selected, or the folder it names no longer contains both "
     "<code>fbc32.exe</code> and <code>fbc64.exe</code>. This is a configuration problem, "
     "not a problem with your code.</p>"),
    ("How do I switch between building 32-bit and 64-bit?",
     "<p>Choose a different build configuration — Win32 or Win64. Every toolchain carries "
     "both compilers, so the configuration is what decides which one runs. See "
     "<a href=\"build-configurations.html\">Build configurations</a>.</p>"),
    ("What is the difference between Build and Execute, Compile and Quick Run?",
     "<p>Build and Execute (" + kbd("F5") + ") compiles the project and runs it. Compile "
     "(" + kbd("Ctrl", "F5") + ") only compiles. Quick Run (" +
     kbd("Ctrl", "Shift", "F5") + ") compiles and runs the current file alone, ignoring "
     "the project.</p>"),
    ("How do I jump to a compiler error?",
     "<p>Click the error row in the Output panel. Always fix the first error and rebuild "
     "— later errors are often consequences of it.</p>"),
    ("How do I pass command-line arguments to my program?",
     "<p>Set them in " + menu("Project", "Project Options…") + ". They are used whenever "
     "Tiko Editor runs your program.</p>"),
    ("How do I see the exact compiler command line?",
     "<p>" + menu("Compile", "Command Line…") + ".</p>"),
    ("My build succeeds but the program behaves as it did before I edited it.",
     "<p>A file was not saved before the build. Turn on <strong>Compile "
     "autosave</strong>, which does it for you.</p>"),

    # --- Debugging ---
    ("How do I set a breakpoint?",
     "<p>Put the caret on the line and press " + kbd("F9") + ", or click the left margin "
     "if <strong>Click toggles breakpoint</strong> is enabled.</p>"),
    ("My breakpoint moved to a different line.",
     "<p>The line you chose generates no executable code — a comment, a blank line or a "
     "bare declaration — so Tiko Editor moved the breakpoint to the next line that does. It "
     "prevents a breakpoint that would silently never fire.</p>"),
    ("Debugging will not start.",
     "<p>The executable needs debug information. Select a Debug build configuration and "
     "rebuild.</p>"),
    ("What is the difference between Step and Step Over?",
     "<p>Step (" + kbd("F11") + ") enters procedures the line calls. Step Over (" +
     kbd("F10") + ") runs them to completion without stopping inside. Use Step Over by "
     "default.</p>"),
    ("How do I watch a variable while stepping?",
     "<p>Click the placeholder row in the Watch pane and type the expression. It is "
     "re-evaluated at every stop, and changed values are highlighted.</p>"),
    ("Break does not interrupt my program.",
     "<p>Break stops at the next line of your source, so a program blocked inside "
     "<code>Sleep</code> or a library call will not stop until it returns. Stop Debugging "
     "(" + kbd("Shift", "F6") + ") works at any time.</p>"),

    # --- Customization ---
    ("How do I customize keyboard shortcuts?",
     "<p>" + kbd("Ctrl", "K") + " opens the Keyboard Shortcuts dialog. Filter for the "
     "command, choose <strong>Modify</strong> and press the keystroke you want. See "
     "<a href=\"keyboard-customization.html\">Customizing keyboard shortcuts</a>.</p>"),
    ("Can I disable a default shortcut without replacing it?",
     "<p>Yes. A suppressed default is recorded in <code>keybindings.ini</code> with a "
     "<code>(DISABLED)</code> marker and shown as disabled in the dialog.</p>"),
    ("How do I run an external program from Tiko Editor?",
     "<p>Define a user tool in " + menu("File", "User Tools…") + ". Tools appear on the "
     "User Tools menu and can have their own shortcuts.</p>"),
    ("Can I use Tiko Editor in another language?",
     "<p>Yes. Choose a language file on the Localization page of the options dialog. The "
     "change takes effect at the next start-up.</p>"),
    ("How do I move my settings to another computer?",
     "<p>Copy the <code>settings</code> folder. That is everything — preferences, "
     "shortcuts, themes, languages and keyword lists.</p>"),
    ("How do I reset everything to defaults?",
     "<p>Close Tiko Editor, rename <code>settings\\settings.ini</code>, and start again. A fresh "
     "file is created with default values.</p>"),

    # --- Files ---
    ("How do I recover unsaved work?",
     "<p>Tiko Editor has no crash-recovery store, so unsaved changes are lost if it exits "
     "abnormally. Turn on <strong>Auto save files</strong> in the options to have modified "
     "documents saved at an interval. Save often, and use version control for anything "
     "that matters.</p>"),
    ("My file shows strange characters instead of accented letters.",
     "<p>It is being read with the wrong encoding. Change it from the status bar — UTF-8 "
     "is usually right. See <a href=\"encoding.html\">Encoding and line endings</a>.</p>"),
    ("How do I change a file's line endings?",
     "<p>Click the line-ending field in the status bar and choose CRLF, LF or CR.</p>"),
    ("Can Tiko Editor tell me when a file changes outside the editor?",
     "<p>Yes — turn on <strong>Detect external file changes</strong> in the options.</p>"),
    ("Can I open a file as a starting template without editing the original?",
     "<p>Yes — " + menu("File", "Open File As Template…") + " opens its contents as a new "
     "untitled document.</p>"),

    # --- Misc ---
    ("Can I run two copies of Tiko Editor at once?",
     "<p>Yes, if <strong>Multiple instances</strong> is enabled. Note that instances "
     "sharing a folder share one <code>settings.ini</code>, and the last one to close "
     "wins.</p>"),
    ("What does the Unused Symbols report tell me?",
     "<p>It lists declared symbols with zero reads, in three flavours: <strong>dead</strong> "
     "(no references at all), <strong>write-only</strong> (assigned but never read — often "
     "a real bug) and <strong>unknown</strong> (counts cannot be trusted, such as exported "
     "procedures). Filter by six kinds, sort any column, and click a row to jump to it. See "
     "<a href=\"symbol-navigation.html\">Symbols and the function list</a>.</p>"),
    ("Can I trust Unused Symbols to tell me what is safe to delete?",
     "<p>Treat it as candidates, not certainties. Counts come from one scan, so anything "
     "reached through a function pointer, excluded by conditional compilation, or called "
     "from outside the project can look unused while being essential.</p>"),
    ("Does Tiko Editor work with languages other than FreeBASIC?",
     "<p>Yes. It edits any text file, and highlights the languages its lexers support. The "
     "project system, symbol engine and debugger are FreeBASIC-specific.</p>"),
    ("Is there a minimap?",
     "<p>No. Use <strong>Fold All</strong> (" + kbd("Shift", "F8") + ") or the function "
     "list (" + kbd("F4") + ") for an overview of a file.</p>"),
    ("What is the Help Center?",
     "<p>The documentation browser built into Tiko Editor, opened with " + kbd("F1") + ". It "
     "searches for the selected text, or the word under the caret.</p>"),
    ("How do I report a bug?",
     "<p>Note your version from " + menu("Help", "About") + " and describe the steps that "
     "reproduce the problem. The About box links to the project's website and source "
     "repository.</p>"),
]

FQ = ""
FQ += p(
    "Common questions, grouped roughly by subject. Use %s to search this page, or "
    "%s to search the whole documentation." % (kbd("Ctrl", "F"), kbd("Ctrl", "K"))
)
FQ += faq(FAQ_ITEMS)
FQ += h2("Still stuck?")
FQ += ul([
    '<a href="troubleshooting.html">Troubleshooting</a> — for things that are broken '
    "rather than merely unclear.",
    '<a href="glossary.html">Glossary</a> — if a term is unfamiliar.',
    '<a href="doc-index.html">Index</a> — to find the page that covers a feature.',
])

page("faq", "Frequently asked questions", "appendix",
     "Around sixty short answers to the questions that come up most often, from "
     "installation through to debugging and customization.",
     FQ,
     keywords="faq questions answers help common problems how do i")

# ==========================================================================
# Tips and tricks
# ==========================================================================

TT = ""
TT += p(
    "Habits and shortcuts that repay the time it takes to learn them, roughly in order of "
    "how much difference they make."
)

TT += h2("The five habits worth building first")
TT += ol([
    "<strong>%s then %s.</strong> Jump to a definition, read it, come straight back. This "
    "one pair replaces most scrolling and most file-switching."
    % (kbd("F12"), kbd("Alt", "←")),
    "<strong>Search once, then %s.</strong> Close the Find bar and step through matches "
    "with a single key while you edit." % kbd("F3"),
    "<strong>%s for anything you can name.</strong> If you know what a procedure is "
    "called, you do not need to know where it lives." % kbd("Ctrl", "P"),
    "<strong>Cut and copy with nothing selected.</strong> Both act on the whole line, so "
    "moving a line is %s, click, %s." % (kbd("Ctrl", "X"), kbd("Ctrl", "V")),
    "<strong>%s before you read an unfamiliar file.</strong> Fold everything, see the "
    "structure, then expand what matters." % kbd("Shift", "F8"),
], steps=True)

TT += h2("Editing tricks")
TT += ul([
    "<strong>Zero-width column selections insert.</strong> %s-drag straight down without "
    "moving sideways to get one caret per line, then type once to prefix every line."
    % kbd("Alt"),
    "<strong>%s is smart.</strong> It goes to the first non-blank character; press it "
    "again for column one. On indented code that is almost always what you meant."
    % kbd("Home"),
    "<strong>%s opens a line below from anywhere on the current one.</strong> No need to "
    "press %s first." % (kbd("Ctrl", "Enter"), kbd("End")),
    "<strong>%s repeatedly extends the selection downward</strong>, so three presses "
    "selects three lines." % kbd("Ctrl", "L"),
    "<strong>Hold %s or %s to reorder several lines at once</strong> — select them "
    "first and the whole block moves." % (kbd("Alt", "↑"), kbd("Alt", "↓")),
    "<strong>Paste is always plain text.</strong> Code copied from a web page or a "
    "document arrives without its formatting.",
])

TT += h2("Search tricks")
TT += ul([
    "<strong>Select text before pressing %s</strong> and it becomes the search term."
    % kbd("Ctrl", "F"),
    "<strong>Use Find in Project as a preview for Replace All.</strong> Search first, see "
    "how many places would change and whether they are all what you expect, then replace.",
    "<strong>Select first, then search.</strong> Opening Find with a multi-line selection "
    "turns on <strong>Selection</strong> automatically, so a Replace All is confined to "
    "the region you highlighted — the safe way to rename inside one procedure only.",
    "<strong>Watch for latched icons.</strong> Match Case, Match Whole Words and Selection "
    "stay lit until you turn them off, and are the usual reason a search stops finding "
    "what you expect.",
])

TT += h2("Navigation tricks")
TT += ul([
    "<strong>%s and %s toggle between a module and its header</strong> — the pair you move "
    "between most while writing code." % (kbd("Ctrl", "Shift", "H"), kbd("Ctrl", "Shift", "C")),
    "<strong>%s and %s step between procedures</strong> without leaving the keyboard."
    % (kbd("Ctrl", "PgDn"), kbd("Ctrl", "PgUp")),
    "<strong>Bookmarks for now, TODO comments for later.</strong> Bookmarks are private "
    "and transient; TODO comments live in the source and are visible to everyone.",
    "<strong>The navigation history survives file switches</strong>, so %s works even "
    "after you have wandered through several files." % kbd("Alt", "←"),
])

TT += h2("Mouse tricks")
TT += ul([
    "<strong>%s + wheel</strong> zooms; <strong>%s + wheel</strong> scrolls horizontally."
    % (kbd("Ctrl"), kbd("Shift")),
    "<strong>Double-click selects a word; triple-click selects a line.</strong>",
    "<strong>Click in the left margin</strong> to select a line — or to toggle a "
    "breakpoint, if you enable that option.",
    "<strong>Double-click the Output panel's tab strip</strong> to minimise it, and click "
    "a tab to bring it back.",
    "<strong>Drag tabs</strong> to reorder your documents.",
])

TT += h2("Build and debug tricks")
TT += ul([
    "<strong>Quick Run (%s) turns Tiko Editor into a scratchpad.</strong> New file, ten lines, "
    "one keystroke — no project needed." % kbd("Ctrl", "Shift", "F5"),
    "<strong>Run to Cursor (%s) instead of a temporary breakpoint.</strong> Nothing to set "
    "and nothing to remember to clear." % kbd("Ctrl", "F10"),
    "<strong>Watch expressions, not variables.</strong> <code>rows(i).total</code> tells "
    "you far more inside a loop than its parts separately.",
    "<strong>Put a breakpoint inside the branch you doubt.</strong> If it never fires, you "
    "have learned that the branch is not taken — which is frequently the bug.",
    "<strong>%s answers most \"why did it build like that?\" questions</strong> in one "
    "glance." % menu("Compile", "Command Line…"),
])

TT += h2("Configuration tricks")
TT += ul([
    "<strong>Back up the <code>settings</code> folder.</strong> It is your entire setup, "
    "and restoring it is a copy.",
    "<strong>Keep two installations</strong> if you work on projects with different "
    "conventions — separate folders have entirely separate settings.",
    "<strong>Give your build configurations shortcuts</strong> so switching between debug "
    "and release is one keystroke.",
    "<strong>Bind the commands that ship unbound.</strong> Format Selection, Project "
    "Options and the panel expand/collapse commands all have no default shortcut and are "
    "free for you to claim.",
    "<strong>Add your library's function names to the keyword lists.</strong> They then "
    "highlight <em>and</em> appear in autocomplete.",
])

TT += h2("Lesser-known features")
TT += table(
    ["Feature", "Why it is useful"],
    [
        ("Open File As Template", "Start from boilerplate without any risk to the original "
         "file."),
        ("Insert File (" + kbd("Ctrl", "I") + ")",
         "Drop a licence header or a block of boilerplate into the current file."),
        ("Notes tab", "A scratchpad saved with the project — build reminders, open "
         "questions."),
        ("Unused Symbols", "A periodic tidy-up: dead procedures and stale variables."),
        ("Duplicate (file)", "Fork a file to experiment without touching the original."),
        (kbd("Ctrl", "`"), "Return focus to the editor from wherever it has ended up."),
        (menu("View", "Restore Main Window Size"),
         "Rescue a window that has ended up off screen or badly sized."),
    ],
    key_first=True,
)

TT += h2("Related topics")
TT += ul([
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
    '<a href="tutorial.html">Tutorial</a>',
    '<a href="faq.html">FAQ</a>',
])

page("tips-and-tricks", "Tips and tricks", "appendix",
     "Power-user habits, keyboard and mouse tricks, and the features people most often "
     "miss.",
     TT,
     keywords="tips tricks productivity power user workflow hidden features shortcuts "
              "habits")

# ==========================================================================
# Troubleshooting
# ==========================================================================

TS = ""
TS += p(
    "Symptoms, causes and fixes. Work down each list — the first item is the most common "
    "cause."
)

TS += h2("Tiko Editor will not start, or starts wrong")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("It starts with no colours and the menus look wrong.",
         "You launched a copy of <code>tiko-editor.exe</code> away from its <code>settings</code> "
         "folder. Run it from the folder you unpacked."),
        ("It reports a missing DLL.",
         "The editing, parser or debugger DLLs are not beside the executable. Restore the "
         "full distribution."),
        ("Settings are forgotten every time.",
         "Tiko Editor cannot write to its own folder. Move it out of <code>C:\\Program "
         "Files</code>, or grant write permission."),
        ("The window opens off screen or at a strange size.",
         "Stored geometry from another monitor arrangement. Choose " +
         menu("View", "Restore Main Window Size") + ", or close Tiko Editor and delete the "
         "<code>[Startup]</code> geometry keys from <code>settings.ini</code>."),
        ("A change I made in the options had no effect.",
         "You may have two installations and be launching the other one. Each folder has "
         "its own settings — check which <code>tiko-editor.exe</code> you ran."),
    ],
)

TS += h2("Compiler and build problems")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("“Compiler not found”, or the build does nothing.",
         "Open the Compiler page of " + menu("File", "Settings", "Options…") + ". Either "
         "no toolchain is selected, or the selected folder no longer holds both "
         "<code>fbc32.exe</code> and <code>fbc64.exe</code> — both are required."),
        ("A toolchain I installed does not appear in the list.",
         "It must be a <em>direct</em> subfolder of <code>toolchains\\</code>, with the "
         "two compiler executables in that folder's root rather than one level down in a "
         "<code>bin</code> folder. Reopen the options dialog — the folder is rescanned "
         "each time the page opens."),
        ("The build fails with errors that make no sense.",
         "Fix the first error and rebuild. One early mistake commonly produces a cascade."),
        ("The build succeeds but runs old behaviour.",
         "A file was not saved. Enable <strong>Compile autosave</strong>."),
        ("Builds behave inconsistently after changing switches.",
         "Stale object files. Use <strong>Rebuild All</strong> (" +
         kbd("Ctrl", "Alt", "F5") + ")."),
        ("The program window flashes and disappears.",
         "Your program ended immediately. Add <code>Sleep</code> at the end, or run it "
         "from a console."),
        ("Linker errors about a library or an architecture mismatch.",
         "A 32-bit program cannot link a 64-bit library. Check whether you are building "
         "with a Win32 or a Win64 build configuration — that, not the toolchain, decides "
         "the architecture."),
    ],
)

TS += h2("Debugger problems")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("Debugging will not start.",
         "The executable carries no debug information. Select a Debug build configuration "
         "and rebuild."),
        ("Breakpoints are never hit.",
         "The code is not reached, or the executable is stale — rebuild. Confirm by "
         "putting a breakpoint on the first line of the program."),
        ("A breakpoint moved when I set it.",
         "That is intended: the line generated no executable code, so it moved to the next "
         "line that did."),
        ("Break does not stop the program.",
         "It stops at the next line of your source; a program blocked in a library call "
         "will not stop until it returns. Use " + kbd("Shift", "F6") + " to stop outright."),
        ("A variable shows an implausible value.",
         "It may not be initialised yet, or you are looking at the wrong stack frame. "
         "Check which frame is selected in the call stack."),
        ("Stepping lands in code I did not write.",
         "You used " + kbd("F11") + " on a library call. " + kbd("Shift", "F11") +
         " steps back out."),
    ],
)

TS += h2("File and encoding problems")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("Accented or non-Latin characters display as garbage.",
         "Wrong encoding. Change it from the status bar; UTF-8 is usually correct."),
        ("A file will not save.",
         "It is read-only, open in another program, or on a disconnected network drive. "
         "Use " + menu("File", "Save As…") + " to write it elsewhere and rescue your "
         "work."),
        ("Line endings look wrong in another tool.",
         "Mixed or unexpected line endings. Convert the file from the status bar."),
        ("A file changed on disk and Tiko Editor did not notice.",
         "Enable <strong>Detect external file changes</strong> in the options."),
        ("Unsaved work was lost after a crash.",
         "There is no crash-recovery store. Enable <strong>Auto save files</strong>, and "
         "use version control."),
    ],
)

TS += h2("Search problems")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("A search finds nothing that is obviously there.",
         "<strong>Match Case</strong>, <strong>Match Whole Words</strong> or "
         "<strong>Selection</strong> is still latched on from an earlier search — look for "
         "a lit icon on the Find bar."),
        ("Find in Project misses files.",
         "Those files are not in the workspace. Add them with " +
         menu("Project", "Add Files to Project…") + "."),
        ("A search only looks at part of the file.",
         "The <strong>Selection</strong> option is latched on, confining the search to the "
         "selected text. Click the icon to turn it off."),
        ("Replace All changed too much.",
         kbd("Ctrl", "Z") + " undoes the whole operation. Enable <strong>Match Whole "
         "Words</strong>, or select a region and use <strong>Selection</strong>, then "
         "repeat."),
    ],
)

TS += h2("Editor behaviour")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("Autocomplete or code tips do not appear.",
         "They are disabled in the options, or the file declaring the symbol is not in the "
         "workspace so the parser has not indexed it."),
        ("Goto Definition does nothing.",
         "Same cause — the defining file must be part of the project."),
        ("The function list is empty.",
         "The active file has no procedures the parser recognised, or its extension is not "
         "one Tiko Editor parses as FreeBASIC source."),
        ("Typing feels slow on a very large file.",
         "Turn off occurrence highlighting and, if necessary, syntax highlighting. Splitting "
         "a very large file is usually the better answer."),
        (kbd("Tab") + " inserts the wrong thing.",
         "Check <strong>Tab indents with spaces</strong> and <strong>Tab size</strong>."),
        ("Code alignment looks wrong.",
         "You are using a proportional font. Choose a monospaced one."),
    ],
)

TS += h2("Appearance and layout")
TS += table(
    ["Symptom", "Cause and fix"],
    [
        ("Text is unreadable in a theme.",
         "Switch theme, or adjust the offending colour in the theme editor. A contrast "
         "theme is the quickest fix."),
        ("A panel has disappeared.",
         kbd("Ctrl", "B") + " for the side panel, " + kbd("Ctrl", "F9") +
         " for the Output panel."),
        ("The Output panel is a thin strip.",
         "It is minimised. Click a tab to restore it, or drag its splitter."),
        ("A custom theme was replaced after an update.",
         "Shipped themes may be overwritten by an update. Save customised themes under a "
         "new name."),
        ("The layout is unusable after resizing.",
         menu("View", "Restore Main Window Size") + " returns the window to its default."),
    ],
)

TS += h2("Starting over")
TS += p("When something is badly wrong and you want a clean slate:")
TS += ol([
    "Close Tiko Editor.",
    "Rename <code>settings\\settings.ini</code> to <code>settings.ini.bak</code>. Rename "
    "rather than delete, so you can put it back.",
    "Start Tiko Editor. A fresh settings file is created.",
    "If the problem is specifically keyboard-related, delete "
    "<code>settings\\keybindings.ini</code> instead. If it is workspace-related, delete "
    "<code>settings\\default.tiko</code>.",
], steps=True)

TS += h2("Reporting a problem")
TS += p("Include:")
TS += ul([
    "the version from %s;" % menu("Help", "About"),
    "your Windows version, and whether you run a 32- or 64-bit Tiko Editor;",
    "the exact steps that reproduce the problem;",
    "what you expected and what happened instead;",
    "any message text, copied rather than described.",
])

TS += h2("Related topics")
TS += ul([
    '<a href="faq.html">FAQ</a>',
    '<a href="settings-files.html">Settings and configuration files</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("troubleshooting", "Troubleshooting", "appendix",
     "Symptoms, causes and fixes for start-up, build, debugger, file, search, editing and "
     "layout problems.",
     TS,
     keywords="troubleshooting problems fix broken wont start compiler not found encoding "
              "slow performance search not working theme settings reset crash recovery")

# ==========================================================================
# Glossary
# ==========================================================================

GLOSSARY = [
    ("Accelerator", "A keyboard shortcut that runs a command directly, such as " +
     kbd("Ctrl", "S") + ". See <a href=\"keyboard-shortcuts.html\">Keyboard "
     "shortcuts</a>."),
    ("ANSI", "The legacy single-byte Windows encoding for a locale. Adequate for plain "
     "ASCII, unreliable for anything else. See <a href=\"encoding.html\">Encoding</a>."),
    ("Autocomplete", "A list of possible completions offered as you type an identifier. "
     "See <a href=\"autocomplete.html\">Autocomplete and code tips</a>."),
    ("BOM (byte-order mark)", "A few bytes at the start of a file recording its encoding. "
     "Some tools require one; others cannot cope with it."),
    ("Bookmark", "A marked line you can jump back to. Stored in the editor, not in the "
     "file. See <a href=\"bookmarks.html\">Bookmarks</a>."),
    ("Breakpoint", "A marker telling the debugger to stop your program when execution "
     "reaches that line. See <a href=\"breakpoints.html\">Breakpoints</a>."),
    ("Build configuration", "A named set of compiler switches, such as Debug or Release. "
     "See <a href=\"build-configurations.html\">Build configurations</a>."),
    ("Call stack", "The chain of procedure calls that reached the current line, innermost "
     "first. See <a href=\"watches.html\">Watches and the call stack</a>."),
    ("Caret", "The blinking text insertion point. Distinct from the mouse cursor."),
    ("Code tip", "A popup showing a procedure's parameters, displayed when you open its "
     "call parenthesis. Sometimes called a call tip."),
    ("Column selection", "A rectangular selection covering the same columns on several "
     "lines. See <a href=\"multiple-selections.html\">Multiple selections and column "
     "mode</a>."),
    ("Compiler", "The program that turns source code into an executable. Tiko Editor drives the "
     "FreeBASIC compiler; it does not include one."),
    ("CRLF / LF / CR", "The three line-ending conventions: Windows, Unix and classic Mac "
     "OS. See <a href=\"encoding.html\">Encoding and line endings</a>."),
    ("Data tip", "The value of a variable shown when you hover over it in the editor "
     "while debugging."),
    ("Encoding", "The rule mapping bytes in a file to characters — UTF-8, ANSI and so on."),
    ("Fold", "A collapsed block of code, shown as a single line. Folding is a display "
     "state and does not change the file. See <a href=\"code-folding.html\">Code "
     "folding</a>."),
    ("FreeBASIC", "The BASIC-dialect compiler Tiko Editor is built around. Free, open source and "
     "self-hosting."),
    ("Gutter / margin", "The narrow strips left of the text holding line numbers, "
     "bookmark and breakpoint markers, and fold markers."),
    ("Help Center", "Tiko Editor's built-in documentation browser, opened with " + kbd("F1") +
     "."),
    ("Keyword", "A word with special meaning in a language, such as <code>Dim</code> or "
     "<code>Function</code>. Keyword lists live in <code>settings\\keywords</code>."),
    ("Lexer", "The component that breaks source into tokens so it can be coloured. Tiko Editor "
     "uses Lexilla. See <a href=\"syntax-highlighting.html\">Syntax highlighting</a>."),
    ("Localization", "Presenting the interface in another language, from a "
     "<code>.lang</code> file. See <a href=\"localization.html\">Localization</a>."),
    ("Main module", "The file in a project holding the program's entry point."),
    ("Monospaced font", "A font in which every character is the same width. Required for "
     "code alignment to work."),
    ("Multiple selections", "Several independent selections or carets edited at once."),
    ("Parser", "The component that reads your source and extracts its symbols. Tiko Editor's runs "
     "in the background and powers Goto Definition, autocomplete and the function list."),
    ("Project", "A named workspace: a set of files plus its options, stored in a "
     "<code>.tiko</code> file. See <a href=\"projects-overview.html\">Projects and "
     "workspaces</a>."),
    ("Regular expression", "A pattern language for describing sets of strings, offered by "
     "some editors for advanced searching. <strong>Tiko Editor does not support them</strong> — "
     "its searches are literal text, refined by Match Case, Match Whole Words and "
     "Selection."),
    ("Resource script", "A <code>.rc</code> file describing icons, menus, dialogs and "
     "version information compiled into a Windows program."),
    ("Scintilla", "The open-source editing component Tiko Editor's editing surface is built on, "
     "also used by Notepad++ and SciTE."),
    ("Side panel", "The dock beside the editor hosting the Explorer, function list and "
     "bookmarks. Toggle with " + kbd("Ctrl", "B") + "."),
    ("Splitter", "A draggable bar between two regions of the window."),
    ("Step into / over / out", "The three ways of advancing a stopped program: entering "
     "calls, running them to completion, or running until the current procedure returns."),
    ("Symbol", "A named thing in your code — a procedure, type, enumeration or variable — "
     "that the parser has indexed."),
    ("Syntax highlighting", "Colouring code according to what each part of it means."),
    ("TODO comment", "A comment marking work still to be done. Tiko Editor collects them into "
     "the Output panel's TODO tab."),
    ("Token", "The smallest meaningful unit of source — an identifier, keyword, operator "
     "or literal. The formatter compares tokens to prove it preserved your code."),
    ("Unicode", "The standard assigning a number to every character in every writing "
     "system. UTF-8 and UTF-16 are ways of storing those numbers as bytes."),
    ("User tool", "An external program you can run from Tiko Editor's menu. See "
     "<a href=\"user-tools.html\">User tools</a>."),
    ("UTF-8", "The dominant Unicode encoding. Byte-compatible with ASCII for plain English "
     "text, and able to represent every Unicode character."),
    ("Watch", "An expression the debugger re-evaluates every time your program stops."),
    ("Whole word", "A search option requiring a match to be a complete word rather than "
     "part of a longer one."),
    ("Workspace", "The set of files currently open, which in Tiko Editor is always a project — "
     "named or untitled."),
    ("Zoom", "A temporary change to displayed text size. It does not alter the configured "
     "font size."),
]

GL = ""
GL += p("Terms used throughout this documentation, in alphabetical order.")
GL += '<dl class="glossary">'
for term, definition in GLOSSARY:
    GL += "<dt>%s</dt><dd>%s</dd>" % (esc(term), definition)
GL += "</dl>"
GL += h2("Related topics")
GL += ul([
    '<a href="doc-index.html">Index</a>',
    '<a href="faq.html">FAQ</a>',
])

page("glossary", "Glossary", "appendix",
     "Plain-language definitions of the editing, project, build and debugging terms used "
     "across this documentation.",
     GL,
     keywords="glossary terminology definitions terms vocabulary")

# ==========================================================================
# Alphabetical index
# ==========================================================================

INDEX_ENTRIES = [
    ("About dialog", "dialog-reference.html#about"),
    ("Accelerators", "keyboard-shortcuts.html"),
    ("Add files to project", "project-files.html"),
    ("ANSI encoding", "encoding.html"),
    ("Autocomplete", "autocomplete.html"),
    ("Autocomplete word (Ctrl+Space)", "autocomplete.html#ctrl-space"),
    ("Auto indentation", "indentation.html"),
    ("Auto save", "configuration-reference.html#cfg-autosavefiles"),
    ("Backing up settings", "settings-files.html"),
    ("Block completion", "indentation.html"),
    ("BOM (byte-order mark)", "encoding.html"),
    ("Bookmarks", "bookmarks.html"),
    ("Bookmarks list", "side-panels.html"),
    ("Brace matching", "brace-matching.html"),
    ("Breadcrumbs", "main-window.html"),
    ("Breakpoints", "breakpoints.html"),
    ("Build and Execute", "building.html"),
    ("Build configurations", "build-configurations.html"),
    ("Build output", "output-panel.html"),
    ("Call stack", "watches.html"),
    ("Caret movement", "editing-basics.html"),
    ("Case conversion", "line-operations.html"),
    ("Character set", "editor-appearance.html"),
    ("Clipboard", "editing-basics.html"),
    ("Close file", "editing-basics.html"),
    ("Code folding", "code-folding.html"),
    ("Code formatting", "code-formatting.html"),
    ("Code tips", "autocomplete.html"),
    ("Column selection", "multiple-selections.html"),
    ("Command line (compiler)", "compiler-setup.html"),
    ("Command reference", "command-reference.html"),
    ("Comment block", "line-operations.html"),
    ("Compile", "building.html"),
    ("Compiler errors", "compiler-errors.html"),
    ("Compiler setup", "compiler-setup.html"),
    ("Configuration reference", "configuration-reference.html"),
    ("Confine caret", "configuration-reference.html#cfg-confinecaret"),
    ("Context menus", "context-menus.html"),
    ("CRLF and LF", "encoding.html"),
    ("Current line highlight", "view-options.html"),
    ("Data tips", "debugging.html"),
    ("Debugging", "debugging.html"),
    ("Delete line", "line-operations.html"),
    ("Dialogs", "dialogs.html"),
    ("Dialog reference", "dialog-reference.html"),
    ("Duplicate line", "line-operations.html"),
    ("Encoding", "encoding.html"),
    ("Explorer", "project-explorer.html"),
    ("Folders (in the Explorer)", "projects-overview.html"),
    ("File categories", "projects-overview.html"),
    ("Exporting settings", "settings-files.html"),
    ("FAQ", "faq.html"),
    ("Find", "find-replace.html"),
    ("Find in Project", "find-in-project.html"),
    ("Find Next", "find-replace.html"),
    ("Fold All", "code-folding.html"),
    ("Fold margin", "view-options.html"),
    ("Fonts", "editor-appearance.html"),
    ("Format Document", "code-formatting.html"),
    ("Format on Enter", "code-formatting.html"),
    ("Format Options dialog", "dialog-reference.html#format-options"),
    ("Format Project", "code-formatting.html"),
    ("Function list", "symbol-navigation.html"),
    ("Fuzzy finder (Ctrl+P)", "navigation.html"),
    ("Glossary", "glossary.html"),
    ("Go Back / Go Forward", "navigation.html"),
    ("Goto Definition", "navigation.html"),
    ("Goto Header File", "navigation.html"),
    ("Help Center", "what-is-tiko.html"),
    ("High-DPI displays", "editor-appearance.html"),
    ("Highlight occurrences", "brace-matching.html"),
    ("Indentation", "indentation.html"),
    ("Indent guides", "view-options.html"),
    ("Insert file", "editing-basics.html"),
    ("Installation", "quick-start.html"),
    ("Keyboard customization", "keyboard-customization.html"),
    ("Keyboard shortcuts", "keyboard-shortcuts.html"),
    ("Keybindings file", "settings-files.html"),
    ("Keyword case", "syntax-highlighting.html"),
    ("Keyword lists", "syntax-highlighting.html"),
    ("Language (interface)", "localization.html"),
    ("Language (syntax)", "syntax-highlighting.html"),
    ("Lexer", "syntax-highlighting.html"),
    ("Line endings", "encoding.html"),
    ("Line numbers", "view-options.html"),
    ("Line operations", "line-operations.html"),
    ("Localization", "localization.html"),
    ("Main module", "project-files.html"),
    ("Main window", "main-window.html"),
    ("Margins", "view-options.html"),
    ("Menu bar", "menu-bar.html"),
    ("Menu reference", "menu-reference.html"),
    ("Minimap", "view-options.html"),
    ("Move line", "line-operations.html"),
    ("Multiple instances", "configuration-reference.html#cfg-multipleinstances"),
    ("Multiple selections", "multiple-selections.html"),
    ("Multi-cursor editing", "multiple-selections.html"),
    ("Navigation", "navigation.html"),
    ("Notes tab", "output-panel.html"),
    ("Open file", "quick-start.html"),
    ("Open File As Template", "project-files.html"),
    ("Open Recent", "quick-start.html"),
    ("Options dialog", "dialog-reference.html#options"),
    ("Output panel", "output-panel.html"),
    ("Overwrite mode", "editing-basics.html"),
    ("Panel icon strip", "toolbar.html"),
    ("Portable installation", "settings-files.html"),
    ("Printing this help", "keyboard-shortcuts.html"),
    ("Project Explorer", "project-explorer.html"),
    ("Project options", "project-options.html"),
    ("Projects", "projects-overview.html"),
    ("Quick Run", "building.html"),
    ("Quick Start", "quick-start.html"),
    ("Rebuild All", "building.html"),
    ("Recent Files", "quick-start.html"),
    ("Recent Projects", "projects-overview.html"),
    ("Redo", "editing-basics.html"),
    ("Regular expressions (not supported)", "glossary.html"),
    ("Search options", "find-replace.html"),
    ("Selection (search option)", "find-replace.html"),
    ("Rename file", "project-files.html"),
    ("Replace", "find-replace.html"),
    ("Replace All", "find-replace.html"),
    ("Resetting settings", "settings-files.html"),
    ("Resource file", "project-files.html"),
    ("Right edge marker", "view-options.html"),
    ("Run executable", "building.html"),
    ("Run to Cursor", "breakpoints.html"),
    ("Save / Save As", "quick-start.html"),
    ("Save as Project", "projects-overview.html"),
    ("Scintilla", "glossary.html"),
    ("Scrolling", "view-options.html"),
    ("Search Symbol", "navigation.html"),
    ("Symbol search (fuzzy)", "navigation.html"),
    ("Selecting text", "editing-basics.html"),
    ("Settings files", "settings-files.html"),
    ("Side panels", "side-panels.html"),
    ("Split views", "tabs-and-splits.html"),
    ("Status bar", "status-bar.html"),
    ("Stepping (debugger)", "debugging.html"),
    ("Symbols", "symbol-navigation.html"),
    ("Syntax highlighting", "syntax-highlighting.html"),
    ("System requirements", "what-is-tiko.html"),
    ("Tabs (documents)", "tabs-and-splits.html"),
    ("Tabs versus spaces", "indentation.html"),
    ("Tab size", "indentation.html"),
    ("Themes", "themes.html"),
    ("Theme editor", "theme-editor.html"),
    ("Tips and tricks", "tips-and-tricks.html"),
    ("TODO comments", "output-panel.html"),
    ("Toolbar", "toolbar.html"),
    ("Troubleshooting", "troubleshooting.html"),
    ("Tutorial", "tutorial.html"),
    ("Undo", "editing-basics.html"),
    ("Unicode", "encoding.html"),
    ("Unused Symbols", "symbol-navigation.html"),
    ("Unused Symbols report", "symbol-navigation.html"),
    ("Write-only symbols", "symbol-navigation.html"),
    ("User tools", "user-tools.html"),
    ("UTF-8", "encoding.html"),
    ("Watches", "watches.html"),
    ("Whitespace display", "view-options.html"),
    ("Whole word search", "find-replace.html"),
    ("Word wrap", "view-options.html"),
    ("Workspace", "projects-overview.html"),
    ("Zoom", "view-options.html"),
]

IX = ""
IX += p(
    "Every topic and feature, alphabetically. Use %s to search the whole documentation "
    "instead." % kbd("Ctrl", "K")
)

# Group by first letter
groups = {}
for term, href in INDEX_ENTRIES:
    letter = term[0].upper()
    groups.setdefault(letter, []).append((term, href))

letters = sorted(groups.keys())
IX += '<nav class="az-nav" aria-label="Jump to letter">'
for letter in letters:
    IX += '<a href="#idx-%s">%s</a>' % (letter, letter)
IX += "</nav>"

for letter in letters:
    IX += '<div class="index-group">'
    IX += h2(letter, anchor="idx-%s" % letter)
    IX += '<ul class="index-list">'
    for term, href in sorted(groups[letter], key=lambda x: x[0].lower()):
        IX += '<li><a href="%s">%s</a></li>' % (href, esc(term))
    IX += "</ul></div>"

IX += h2("See also")
IX += ul([
    '<a href="glossary.html">Glossary</a> — definitions rather than links.',
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
    '<a href="faq.html">FAQ</a>',
])

page("doc-index", "Index", "appendix",
     "A complete alphabetical index of the topics and features covered in this "
     "documentation.",
     IX,
     keywords="index alphabetical a-z topics list contents")
