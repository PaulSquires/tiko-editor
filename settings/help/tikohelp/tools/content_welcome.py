# -*- coding: utf-8 -*-
"""Welcome section: home page, product overview, quick start."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui, figure_img,
                   placeholder, diagram, icon)

section("welcome", "Welcome", "home")

# ==========================================================================
# Home
# ==========================================================================

HOME = """
<div class="hero">
  <p class="hero-lede">Tiko is a fast, native Windows code editor built for FreeBASIC
  development — with a project system, an integrated compiler workflow, a real debugger
  and a symbol engine that understands your code. It edits any text-based language
  equally well.</p>
  <div class="hero-actions">
    <a class="btn" href="quick-start.html">Start the 25-minute Quick Start %s</a>
    <a class="btn secondary" href="what-is-tiko.html">What is Tiko?</a>
    <a class="btn secondary" href="tutorial.html">Full tutorial</a>
  </div>
</div>
""" % icon("arrow-right")

HOME += figure_img(
    "assets/img/tiko_dark.png",
    "Tiko editing a FreeBASIC project. The Explorer sits on the left, the editor fills the "
    "centre, and the Output panel is docked below.",
    alt="The Tiko editor main window using a dark theme",
)

HOME += h2("Start here")
HOME += cards([
    ("quick-start.html", "rocket", "Quick Start",
     "Install, open a file, edit, search, build and run — in about 25 minutes."),
    ("what-is-tiko.html", "home", "What is Tiko?",
     "The product in a page: what it does, who it is for, and its design philosophy."),
    ("tutorial.html", "graduation", "Ten-lesson tutorial",
     "A progressive course from your first file through to advanced editing."),
    ("main-window.html", "window", "Tour the interface",
     "Every part of the main window, named and explained with diagrams."),
])

HOME += h2("Explore the documentation")
HOME += cards([
    ("editing-basics.html", "edit", "Editing text",
     "Cursors, selections, clipboard, indentation, encoding and folding."),
    ("find-replace.html", "find", "Searching",
     "Find, Replace, Find in Project, search options and search history."),
    ("navigation.html", "compass", "Navigation",
     "Goto Definition, symbol search, bookmarks, function list and history."),
    ("autocomplete.html", "bolt", "Productivity",
     "Autocomplete, code tips, the formatter, and line and block commands."),
    ("projects-overview.html", "folder", "Projects",
     "The workspace model, the Explorer, project files and project options."),
    ("building.html", "build", "Building programs",
     "Compiler setup, build configurations, running, and error navigation."),
    ("debugging.html", "bug", "Debugging",
     "Breakpoints, stepping, watches, the call stack and the debugger panes."),
    ("themes.html", "sliders", "Customization",
     "Themes, fonts, syntax colours, keyboard shortcuts, layout and settings."),
])

HOME += h2("Reference")
HOME += cards([
    ("keyboard-shortcuts.html", "keyboard", "Keyboard shortcuts",
     "Every default shortcut, grouped by category, in printable tables."),
    ("menu-reference.html", "list", "Menu reference",
     "Every menu and every command, with shortcuts and related topics."),
    ("configuration-reference.html", "sliders", "Configuration reference",
     "Every setting: purpose, default, accepted values and related options."),
    ("command-reference.html", "list", "Command reference",
     "Commands with their identifiers, menu locations and shortcuts."),
    ("dialog-reference.html", "window", "Dialog reference",
     "Every dialog box and what each control does."),
    ("faq.html", "help", "FAQ",
     "Around fifty common questions with short, direct answers."),
    ("tips-and-tricks.html", "bolt", "Tips and tricks",
     "Power-user workflows, hidden features and keyboard and mouse tricks."),
    ("troubleshooting.html", "help", "Troubleshooting",
     "Fixes for start-up failures, compiler problems, encoding and performance."),
    ("glossary.html", "book", "Glossary",
     "Plain-language definitions of the terms used throughout these pages."),
    ("doc-index.html", "list", "Index",
     "A complete alphabetical index of topics and features."),
])

HOME += h2("How to use this help system")
HOME += ul([
    "Press %s anywhere to open search. Results are ranked, and your search terms are "
    "highlighted on the page you land on." % kbd("Ctrl", "K"),
    "The sidebar groups every topic into collapsible sections. It remembers which "
    "sections you left open.",
    "Long pages carry an <strong>On this page</strong> outline on the right that tracks "
    "your scroll position.",
    "Use <strong>Previous</strong> and <strong>Next</strong> at the foot of each page to "
    "read the documentation straight through, front to back.",
    "Switch between light and dark with the theme button in the header. Your choice is "
    "remembered; if you never choose, the site follows your operating system.",
    "Every page is print-ready — use your browser's Print command to produce a clean "
    "copy without navigation furniture.",
])

HOME += note(
    "This help system works entirely offline. It is plain HTML, CSS and JavaScript with "
    "no external requests, so you can copy the folder anywhere — a USB stick, a network "
    "share, a documentation server — and open <code>index.html</code> in any modern browser."
)

page("index", "Tiko Editor Documentation", "welcome",
     "Everything you need to install, learn and master Tiko — the native Windows code "
     "editor for FreeBASIC development.",
     HOME,
     keywords="home start documentation help tiko editor freebasic")

# ==========================================================================
# What is Tiko?
# ==========================================================================

WHAT = ""
WHAT += h2("What is Tiko?")
WHAT += p(
    "Tiko is a programmer's code editor for Microsoft Windows, written by Paul Squires "
    "(PlanetSquires Software) and released under the GNU General Public License v3. It is "
    "designed first and foremost for <strong>FreeBASIC</strong> development, and it ships "
    "with the pieces that make that work end to end: a project system, compiler "
    "integration, a build configuration manager, a source-level debugger and a symbol "
    "engine that reads your code as you type."
)
WHAT += p(
    "It is not limited to FreeBASIC. Tiko is a competent general-purpose text and code "
    "editor, so C, C++, resource scripts, INI files, plain text and anything else you "
    "point it at open and edit normally."
)
WHAT += p(
    "Tiko is a <strong>native Win32 application</strong>. There is no runtime to install, "
    "no browser engine behind the editor surface, and no background service. The editing "
    "surface is built on <strong>Scintilla</strong>, the same proven editing component "
    "used by Notepad++ and SciTE."
)

WHAT += h2("Why use Tiko?")
WHAT += h3("It is fast, and stays fast")
WHAT += p(
    "Tiko starts quickly and keeps responding on large files. Source parsing happens on a "
    "background worker thread and its results are handed back to the user interface, so "
    "scanning a big project never blocks your typing."
)
WHAT += h3("It understands FreeBASIC specifically")
WHAT += p(
    "Generic editors treat BASIC as coloured text. Tiko parses it. The symbol engine "
    "extracts procedures and their parameters, types, enumerations and variables — with "
    "the file, line and column of each — which is what powers Goto Definition, the "
    "function list, code tips and autocomplete."
)
WHAT += h3("The whole build cycle lives in one window")
WHAT += p(
    "Set up your compiler once, then edit, compile, run and debug without leaving the "
    "editor. Compiler errors appear in the Output panel; clicking one jumps to the "
    "offending line."
)
WHAT += h3("It is genuinely configurable")
WHAT += p(
    "Fourteen bundled themes, a full theme editor, complete keyboard remapping, a code "
    "formatter with per-rule control, user-defined external tools and six interface "
    "languages — all stored in plain files next to the executable."
)

WHAT += h2("Key features")
WHAT += table(
    ["Area", "What you get"],
    [
        ("Editing",
         "Scintilla editing surface, syntax highlighting, code folding, brace matching, "
         "multiple selections, column selection, bookmarks, split views and zoom."),
        ("Code intelligence",
         "Autocomplete, code tips (parameter hints), Goto Definition (%s), symbol search "
         "(%s), a live function list, and back/forward navigation history."
         % (kbd("F12"), kbd("Ctrl", "P"))),
        ("Search",
         "Find and Replace in the current file, Find in Project across every project file, "
         "whole-word and case-sensitive matching, and searching within a selection."),
        ("Projects",
         "A single workspace model — every set of open files is a project — with an "
         "Explorer tree, per-project options and per-project notes and TODO lists."),
        ("Building",
         "Compiler configuration, named build configurations, Build and Execute, Compile, "
         "Rebuild All, Quick Run, and clickable compiler errors."),
        ("Debugging",
         "A native source-level debugger: breakpoints, stepping, run to cursor, a call "
         "stack, watch expressions, locals and globals, and data tips on hover."),
        ("Formatting",
         "A configurable code formatter that can reindent, normalise keyword case and "
         "spacing, and run on demand, on Enter or on paste."),
        ("Customization",
         "Themes, editor fonts, full keyboard remapping, user tools, localization and "
         "an options dialog covering every editor behaviour."),
    ],
)

WHAT += h2("Philosophy")
WHAT += p("A few principles show up repeatedly in how Tiko behaves:")
WHAT += ul([
    "<strong>Native, not emulated.</strong> Tiko is a Win32 program that draws its own "
    "controls. It looks and behaves like a Windows application because it is one.",
    "<strong>Everything is a file you own.</strong> Settings, themes, keyboard bindings, "
    "language files and keyword lists are plain text files in the <code>settings</code> "
    "folder beside the executable. You can read them, back them up, copy them to another "
    "machine or put them in version control.",
    "<strong>Portable by default.</strong> Tiko resolves its files relative to its own "
    "location, not to your user profile or the registry. Copy the folder to a USB stick "
    "and it takes your entire configuration with it.",
    "<strong>No surprises with your source.</strong> The formatter never moves a line "
    "break on its own, and it verifies that a reformat preserved every token before it "
    "touches your buffer.",
    "<strong>Nothing phones home.</strong> Tiko can check for updates if you ask it to; "
    "otherwise it makes no network requests.",
])

WHAT += h2("System requirements")
WHAT += dl([
    ("Operating system", "Microsoft Windows. Tiko is a native Win32 application; "
     "both 32-bit and 64-bit builds are supported."),
    ("Disk space", "A few tens of megabytes for the editor, its DLLs and its settings."),
    ("Runtime", "None. There is no .NET, Java or scripting runtime dependency."),
    ("For building programs", "Nothing extra. Tiko ships with the most recent FreeBASIC "
     "toolchain already installed and selected. You can add other toolchains and switch "
     'between them — see <a href="compiler-setup.html">Compiler setup</a>.'),
    ("For debugging", "A program compiled with debug information. See "
     '<a href="debugging.html">Debugging</a>.'),
])

WHAT += note(
    "Tiko ships the Scintilla and Lexilla editing libraries and its parser and debugger "
    "engines as DLLs alongside <code>tiko.exe</code>. Keep them together — the editor "
    "loads them from its own directory."
)

WHAT += h2("Where things live")
WHAT += p(
    "Everything Tiko reads and writes sits under the folder holding "
    "<code>tiko.exe</code>:"
)
WHAT += code("""
tiko.exe                        The editor
Scintilla64.dll                 Editing component
Lexilla64.dll                   Syntax lexers
fbcParser.dll                   Symbol / code-intelligence engine
debugParser.dll                 Debug engine
SegoeFluentIcons.ttf            Icon font used by the interface
settings\\
    settings.ini                All editor settings
    keybindings.ini             Your keyboard shortcut overrides
    default.tiko                The untitled workspace
    themes\\                     Colour themes (*.theme)
    languages\\                  Interface translations (*.lang)
    keywords\\                   Syntax highlighting keyword lists
    help\\                       Bundled help topics
""", lang="text", title="Installation layout", numbered=False)

WHAT += tip(
    "Because configuration is entirely file-based, moving to a new machine is a folder "
    'copy. See <a href="settings-files.html">Settings and configuration files</a>.'
)

WHAT += h2("Related topics")
WHAT += ul([
    '<a href="quick-start.html">Quick Start</a> — get productive in 25 minutes.',
    '<a href="main-window.html">The main window</a> — a guided tour of the interface.',
    '<a href="tutorial.html">Tutorial</a> — the full ten-lesson course.',
    '<a href="glossary.html">Glossary</a> — terminology used across this documentation.',
])

page("what-is-tiko", "What is Tiko?", "welcome",
     "An overview of the editor: what it does, why you might choose it, its design "
     "philosophy, and what it needs to run.",
     WHAT,
     keywords="about overview introduction philosophy requirements freebasic scintilla "
              "gpl planetsquires paul squires native win32")

# ==========================================================================
# Quick Start
# ==========================================================================

QS = ""
QS += note(
    "This walkthrough takes 20–30 minutes and touches every part of the editor you will "
    "use daily. If you would rather learn in smaller pieces with exercises, take the "
    '<a href="tutorial.html">ten-lesson tutorial</a> instead — it covers the same ground '
    "more slowly."
)

QS += h2("1. Install and launch")
QS += p(
    "Tiko does not use an installer. Unpack the distribution into any folder you can "
    "write to and run <code>tiko.exe</code> from there."
)
QS += ol([
    "Unpack the archive into a folder — for example <code>C:\\tiko</code>. Avoid "
    "<code>C:\\Program Files</code>: Tiko writes its settings beside the executable, and "
    "that location is protected.",
    "Double-click <code>tiko.exe</code>.",
    "The main window opens with an empty untitled workspace.",
], steps=True)
QS += warn(
    "Run <code>tiko.exe</code> from the folder you unpacked, not from a copy you moved "
    "elsewhere on its own. Tiko finds its themes, settings and keyword files relative to "
    "its own location, so a stray copy starts with no theme and no configuration."
)

QS += h2("2. Get your bearings")
QS += p("The window has five regions. Spend a moment locating each one.")
QS += diagram("""
<style>
  .dg-bg { fill: var(--c-bg-sunken); }
  .dg-box { fill: var(--c-surface); stroke: var(--c-border-strong); stroke-width: 1.5; }
  .dg-accent { fill: var(--c-accent-soft); stroke: var(--c-accent); stroke-width: 1.5; }
  .dg-t { fill: var(--c-text); font-family: var(--font-sans); font-size: 13px; }
  .dg-s { fill: var(--c-text-mute); font-family: var(--font-sans); font-size: 11px; }
  .dg-n { fill: var(--c-accent-ink); font-family: var(--font-sans); font-size: 12px; font-weight: 700; }
  .dg-c { fill: var(--c-accent); }
</style>
<rect x="0" y="0" width="800" height="380" class="dg-bg"/>
<rect x="20" y="18" width="760" height="30" class="dg-accent"/>
<text x="34" y="38" class="dg-t">File  Edit  Search  View  Project  Compile  Debug  Help</text>
<circle cx="742" cy="33" r="11" class="dg-c"/><text x="738" y="38" class="dg-n">1</text>

<rect x="20" y="56" width="228" height="34" class="dg-box"/>
<text x="30" y="78" class="dg-s">▤  ƒ  ⚑  ⚙</text>
<text x="170" y="78" class="dg-s">🔍 💾 ▶</text>
<circle cx="230" cy="72" r="11" class="dg-c"/><text x="226" y="77" class="dg-n">2</text>

<rect x="20" y="96" width="228" height="226" class="dg-box"/>
<text x="34" y="118" class="dg-t">Explorer</text>
<text x="34" y="140" class="dg-s">project files</text>
<text x="34" y="158" class="dg-s">functions</text>
<text x="34" y="176" class="dg-s">bookmarks</text>
<circle cx="230" cy="112" r="11" class="dg-c"/><text x="226" y="117" class="dg-n">3</text>

<rect x="254" y="56" width="526" height="34" class="dg-box"/>
<text x="268" y="78" class="dg-t">main.bas  ×    utils.bi  ×</text>
<circle cx="762" cy="72" r="11" class="dg-c"/><text x="758" y="77" class="dg-n">4</text>

<rect x="254" y="96" width="526" height="140" class="dg-box"/>
<text x="268" y="120" class="dg-s">1</text><text x="292" y="120" class="dg-t">' editor surface</text>
<text x="268" y="142" class="dg-s">2</text><text x="292" y="142" class="dg-t">Print "Hello, world!"</text>
<text x="268" y="164" class="dg-s">3</text><text x="292" y="164" class="dg-t">Sleep</text>
<circle cx="762" cy="112" r="11" class="dg-c"/><text x="758" y="117" class="dg-n">5</text>

<rect x="254" y="244" width="526" height="78" class="dg-box"/>
<text x="268" y="266" class="dg-t">Compiler  Search results  TODO  Notes</text>
<text x="268" y="290" class="dg-s">Build succeeded.</text>
<circle cx="762" cy="260" r="11" class="dg-c"/><text x="758" y="265" class="dg-n">6</text>

<rect x="20" y="330" width="760" height="28" class="dg-box"/>
<text x="34" y="349" class="dg-s">Ln 2, Col 24</text>
<text x="470" y="349" class="dg-s">Win64 Console (Debug)   Spaces: 4   UTF-8   CRLF</text>
<circle cx="742" cy="344" r="11" class="dg-c"/><text x="738" y="349" class="dg-n">7</text>
""", "The Tiko main window. <strong>1</strong> Menu bar · <strong>2</strong> Panel icon "
     "strip, running across the top of the side panel · "
     "<strong>3</strong> Side panel (Explorer, Functions, Bookmarks) · "
     "<strong>4</strong> Document tabs · <strong>5</strong> Editor surface · "
     "<strong>6</strong> Output panel · <strong>7</strong> Status bar.")

QS += tip(
    "Two shortcuts worth learning immediately: %s toggles the side panel and %s toggles "
    "the Output panel. Both give the editor the whole window when you need room."
    % (kbd("Ctrl", "B"), kbd("Ctrl", "F9"))
)

QS += h2("3. Create your first file")
QS += ol([
    "Choose %s or press %s. A new untitled document opens in a new tab."
    % (menu("File", "New"), kbd("Ctrl", "N")),
    "Type the short program below. Notice that keywords colour themselves as you type, "
    "and that pressing %s after <code>For</code> automatically indents the next line."
    % kbd("Enter"),
], steps=True)
QS += code("""
' hello.bas - a first FreeBASIC program
Dim As Integer i

For i = 1 To 5
    Print "Line "; i
Next i

Print "Press any key to finish."
Sleep
""", lang="fb", title="hello.bas")

QS += h2("4. Save, and Save As")
QS += ol([
    "Press %s. Because the file has never been saved, the Save As dialog opens."
    % kbd("Ctrl", "S"),
    "Choose a folder, name the file <code>hello.bas</code>, and save. The tab caption "
    "changes from <em>Untitled</em> to the file name.",
    "Type another line, then press %s again. This time it saves silently — no dialog."
    % kbd("Ctrl", "S"),
    "To write a copy under a different name, use %s. The editor then continues editing "
    "the <em>new</em> file, not the original." % menu("File", "Save As…"),
], steps=True)
QS += note(
    "An asterisk or modified marker on a tab means the file has unsaved changes. "
    "%s saves every modified document at once." % kbd("Ctrl", "Shift", "S")
)

QS += h2("5. Reopen recent work")
QS += p(
    "Tiko keeps two separate recent lists, and the distinction matters:"
)
QS += ul([
    "%s — individual files you have opened." % menu("File", "Open Recent"),
    "%s — whole workspaces, restoring every file that was open."
    % menu("Project", "Recent Projects"),
])
QS += p(
    "Each list ends with <strong>Clear this list</strong> if you want to reset it. Tiko "
    "also reopens your last workspace automatically when it starts, so in normal use you "
    "rarely need either list."
)

QS += h2("6. Edit: undo, clipboard, selection")
QS += p("The basics behave exactly as they do in every Windows editor:")
QS += table(
    ["Action", "Shortcut", "Notes"],
    [
        ("Undo", kbd("Ctrl", "Z"), "Unlimited within a session."),
        ("Redo", kbd("Ctrl", "Shift", "Z"), "Reapplies what Undo removed."),
        ("Cut", kbd("Ctrl", "X"), "With no selection, acts on the whole line."),
        ("Copy", kbd("Ctrl", "C"), ""),
        ("Paste", kbd("Ctrl", "V"), ""),
        ("Select all", kbd("Ctrl", "A"), ""),
        ("Select the current line", kbd("Ctrl", "L"), "Repeat to extend downward."),
        ("Delete the current line", kbd("Ctrl", "Y"), "No selection needed."),
        ("Duplicate the current line", kbd("Ctrl", "D"), ""),
        ("Move the line up or down", kbd("Alt", "↑") + " / " + kbd("Alt", "↓"),
         "Moves the selection if there is one."),
    ],
    key_first=True,
)

QS += h3("Selecting text")
QS += ul([
    "<strong>Drag</strong> with the mouse, or hold %s and use the arrow keys." % kbd("Shift"),
    "<strong>Double-click</strong> selects a word; <strong>triple-click</strong> selects a line.",
    "<strong>Column (rectangular) selection:</strong> hold %s while dragging, or hold "
    "%s and use the arrow keys. Typing then edits every selected line at once."
    % (kbd("Alt"), kbd("Alt", "Shift")),
    "<strong>Multiple selections:</strong> hold %s and drag or click to add further "
    "selections, then type to edit them all together." % kbd("Ctrl"),
])
QS += tip(
    "Column mode is the quickest way to add the same prefix to a run of lines — select "
    "the zero-width column down the left edge and type once."
)

QS += h2("7. Search and replace")
QS += ol([
    "Press %s to open Find. Type <code>Print</code>." % kbd("Ctrl", "F"),
    "Press %s to jump to the next match and %s for the previous one — these work even "
    "after the Find bar has closed." % (kbd("F3"), kbd("Shift", "F3")),
    "Press %s for Replace. Enter what to find and what to put in its place, then use "
    "<strong>Replace</strong> for one match at a time or <strong>Replace All</strong> "
    "for every match." % kbd("Ctrl", "H"),
    "Press %s for <strong>Find in Project</strong> to search every file in the workspace "
    "at once. Results collect in the Output panel; click one to jump straight to it."
    % kbd("Ctrl", "Shift", "F"),
], steps=True)
QS += note(
    "Find offers three latching options — <strong>Match Case</strong>, <strong>Match Whole "
    "Words</strong> and <strong>Selection</strong>. Tiko does not support regular "
    'expressions. See <a href="find-replace.html">Find and Replace</a>.'
)

QS += h2("8. Jump around the file")
QS += table(
    ["Goal", "How"],
    [
        ("Go to a symbol or file anywhere in the project", kbd("Ctrl", "P") +
         " opens the fuzzy finder; type a few letters of a procedure, type or file name."),
        ("Go to a specific line",
         "There is no Goto Line dialog — click the compiler error, search result or "
         "bookmark instead. See " + '<a href="navigation.html">Navigation</a>.'),
        ("Jump to a definition", "Put the caret on a name and press " + kbd("F12") + "."),
        ("Come back again", kbd("Alt", "←") + " goes back, " + kbd("Alt", "→") +
         " goes forward."),
        ("Next / previous procedure", kbd("Ctrl", "PgDn") + " / " + kbd("Ctrl", "PgUp")),
        ("Toggle a bookmark", kbd("Ctrl", "F2") + "; " + kbd("F2") +
         " and " + kbd("Shift", "F2") + " cycle through them."),
        ("See every procedure in the file", kbd("F4") + " opens the function list."),
    ],
    key_first=True,
)

QS += h2("9. Check your compiler")
QS += important(
    "<strong>There is nothing to install.</strong> Tiko ships with the most recent "
    "FreeBASIC toolchain already set up, so you can skip this step entirely and go "
    "straight to building. It is here so you know where the setting lives."
)
QS += p(
    "Compiler toolchains are subfolders of the <code>toolchains\\</code> folder beside "
    "<code>tiko.exe</code>, and each one holds both <code>fbc32.exe</code> and "
    "<code>fbc64.exe</code>. To see which is selected, or to switch to another you have "
    "installed:"
)
QS += ol([
    "Open %s." % menu("File", "Settings", "Options…"),
    "Select the <strong>Compiler</strong> page.",
    "The list shows every toolchain found in <code>toolchains\\</code>. Click one to "
    "select it.",
    "Choose <strong>OK</strong>.",
], steps=True)
QS += note(
    "Whether you build 32-bit or 64-bit is decided by the <strong>build "
    "configuration</strong>, not by the toolchain — every toolchain contains both "
    'compilers. See <a href="build-configurations.html">Build configurations</a>.'
)
QS += figure_img(
    "assets/img/options-compiler.png",
    "The Compiler page of the options dialog. This is a one-off: set it once and every "
    "project uses it.",
    alt="The Compiler page of the Tiko options dialog")

QS += h2("10. Build and run")
QS += p("With a compiler configured, the whole cycle is three keys:")
QS += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Build and Execute", kbd("F5"),
         "Compiles, and runs the result if the compile succeeded. The command you will "
         "use most."),
        ("Compile", kbd("Ctrl", "F5"), "Compiles without running."),
        ("Rebuild All", kbd("Ctrl", "Alt", "F5"), "Rebuilds everything from scratch."),
        ("Quick Run", kbd("Ctrl", "Shift", "F5"),
         "Compiles and runs the current file on its own, ignoring the project."),
        ("Run Executable", kbd("Shift", "F5"),
         "Runs the last successful build without recompiling."),
        ("Compile Module", kbd("Ctrl", "F7"), "Compiles only the current module."),
    ],
    key_first=True,
)
QS += ol([
    "Press %s." % kbd("F5"),
    "The Output panel opens on its <strong>Compiler</strong> tab and shows the build.",
    "If the build succeeds, your program runs.",
    "If it fails, each error appears as a line in the panel. <strong>Click an error</strong> "
    "to jump straight to the file and line that caused it.",
], steps=True)
QS += tip(
    "Quick Run (%s) is ideal for a scratch file — it builds and runs just that file with "
    "no project set-up at all." % kbd("Ctrl", "Shift", "F5")
)

QS += h2("11. Debug")
QS += p(
    "Tiko has a real source-level debugger built in, not just a console window."
)
QS += ol([
    "Click in the margin beside a line, or press %s, to set a breakpoint. A marker "
    "appears in the margin." % kbd("F9"),
    "Press %s to start debugging. The program runs until it reaches your breakpoint and "
    "then stops, with the current line marked." % kbd("F6"),
    "Step through the code: %s steps into calls, %s steps over them, %s steps out of the "
    "current procedure." % (kbd("F11"), kbd("F10"), kbd("Shift", "F11")),
    "Hover the mouse over a variable to see its value, or read the debugger panes — "
    "globals, locals, the call stack and your watch expressions.",
    "Press %s to stop debugging." % kbd("Shift", "F6"),
], steps=True)
QS += note(
    "Debugging needs a build that carries debug information. Tiko selects an appropriate "
    'debug build automatically. See <a href="debugging.html">Debugging</a> for the '
    "full story."
)

QS += h2("12. Make it yours")
QS += p("Two quick customizations before you finish:")
QS += h3("Pick a theme")
QS += ol([
    "Open %s (or press %s)." % (menu("File", "Settings", "Themes…"), kbd("Ctrl", "Shift", "T")),
    "Choose from the fourteen bundled themes — light and dark variants of several "
    "families.",
    "Apply, and the whole interface changes with it.",
], steps=True)
QS += h3("Set the editor font")
QS += ol([
    "Open %s." % menu("File", "Settings", "Options…"),
    "On the editor page, choose the font name, size and character set. The default is "
    "Consolas at 11 point.",
    "Choose <strong>OK</strong>.",
], steps=True)
QS += tip(
    "%s and %s zoom the editor text temporarily without changing the configured font "
    "size; %s resets the zoom." % (kbd("Ctrl", "+"), kbd("Ctrl", "-"), kbd("Ctrl", "0"))
)

QS += h2("13. Close down")
QS += ul([
    "%s closes the current document; %s closes them all."
    % (kbd("Ctrl", "W"), kbd("Ctrl", "Shift", "W")),
    "%s exits Tiko. You are prompted to save anything modified." % kbd("Alt", "F4"),
    "Your workspace — which files were open, and the window layout — is restored the "
    "next time you start.",
])

QS += h2("What now?")
QS += cards([
    ("tutorial.html", "graduation", "Take the full tutorial",
     "Ten lessons with exercises, from first file to advanced editing."),
    ("main-window.html", "window", "Learn the interface properly",
     "Every panel, tab, dialog and status field explained."),
    ("keyboard-shortcuts.html", "keyboard", "Learn the shortcuts",
     "The complete list, grouped by category and ready to print."),
    ("tips-and-tricks.html", "bolt", "Pick up power-user habits",
     "Workflows and hidden features that repay the time."),
])

page("quick-start", "Quick Start", "welcome",
     "Install Tiko, learn the interface, and work through editing, searching, building, "
     "running and debugging a program — in about 25 minutes.",
     QS,
     keywords="quick start getting started install launch new file save open build run "
              "compile debug tutorial beginner first steps")
