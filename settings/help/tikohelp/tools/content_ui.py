# -*- coding: utf-8 -*-
"""User Interface section."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui, figure_img,
                   placeholder, diagram)

section("ui", "User Interface", "window")

# ==========================================================================
# Main window
# ==========================================================================

MW = ""
MW += figure_img(
    "assets/img/tiko_light.png",
    "The Tiko main window in a light theme, with the Explorer open on the left and the "
    "Output panel docked below the editor.",
    alt="Tiko main window using a light theme",
)

MW += h2("Anatomy of the window")
MW += p(
    "Tiko's window is built from seven regions. Everything else — dialogs, popups, "
    "tooltips — appears on top of these."
)
MW += table(
    ["Region", "Purpose", "Show / hide"],
    [
        ("Menu bar", "Every command in the application, grouped into eight menus.",
         "Always visible"),
        ("Panel icon strip", "Buttons that switch the side panel and run common "
         "commands such as Save, Find, Compile and Debug.", "With the side panel"),
        ("Side panel", "Hosts the Explorer, the function list and the bookmarks list.",
         kbd("Ctrl", "B")),
        ("Document tabs", "One tab per open file, with a close button and a tab list.",
         "Always visible"),
        ("Editor surface", "Where you edit. Can be split into two views.",
         "Always visible"),
        ("Output panel", "Compiler results, search results, TODO items and Notes.",
         kbd("Ctrl", "F9")),
        ("Status bar", "Caret position, insert mode, encoding, line endings and "
         "language.", "Always visible"),
    ],
)

MW += h2("Resizing and layout")
MW += p(
    "The side panel and the Output panel are separated from the editor by draggable "
    "splitter bars. Point at a splitter, and the cursor changes to a resize arrow; drag "
    "to change the split."
)
MW += ul([
    "Drag the vertical splitter to widen or narrow the side panel.",
    "Drag the horizontal splitter to change the height of the Output panel.",
    "Double-click the Output panel's tab strip to minimise it to just its tabs; "
    "single-click a tab to restore it.",
    "The Explorer can sit on either side of the window — use %s."
    % menu("View", "Move Explorer Window Left/Right"),
    "%s restores the main window to its default size."
    % menu("View", "Restore Main Window Size"),
])
MW += note(
    "Layout is remembered between sessions: panel widths, the Output panel height, which "
    "panels were open, and the main window's position and maximised state are all stored "
    'in <code>settings.ini</code>. See <a href="settings-files.html">Settings and '
    "configuration files</a>."
)

MW += h2("The title bar")
MW += p(
    "The window caption shows the active document and, when the workspace has been named, "
    "the project. An untitled workspace shows no project name — it is still a project, it "
    'just has not been saved under a name yet. See <a href="projects-overview.html">'
    "Projects and workspaces</a>."
)

MW += h2("Keyboard focus")
MW += p(
    "Focus moves with %s between the editor and the panels, and clicking anywhere gives "
    "that region focus. If focus ever ends up somewhere unexpected, %s returns it to the "
    "editing window." % (kbd("Tab"), kbd("Ctrl", "`"))
)

MW += h2("Related topics")
MW += ul([
    '<a href="menu-bar.html">Menu bar</a>',
    '<a href="side-panels.html">Side panels</a>',
    '<a href="output-panel.html">Output panel</a>',
    '<a href="tabs-and-splits.html">Tabs and split views</a>',
    '<a href="status-bar.html">Status bar</a>',
])

page("main-window", "The main window", "ui",
     "A guided tour of Tiko's interface: the seven regions of the main window, how to "
     "resize them, and how the layout is remembered.",
     MW,
     keywords="main window interface layout regions panels splitter resize ui tour")

# ==========================================================================
# Menu bar
# ==========================================================================

MB = ""
MB += p(
    "The menu bar carries every command in Tiko. It is drawn by the editor rather than by "
    "Windows, so it follows your chosen theme, but it behaves like a standard Windows "
    "menu: click to open, arrow keys to move, %s to activate, %s to close."
    % (kbd("Enter"), kbd("Esc"))
)

MB += h2("The eight menus")
MB += table(
    ["Menu", "Contents"],
    [
        ('<a href="menu-reference.html#file-menu">File</a>',
         "New, Open, Open Recent, Close, Save, Save As, Rename, Duplicate, Settings, "
         "User Tools and Exit."),
        ('<a href="menu-reference.html#edit-menu">Edit</a>',
         "Undo and Redo, clipboard commands, line operations, commenting, selection and "
         "the Format submenu."),
        ('<a href="menu-reference.html#search-menu">Search</a>',
         "Search Symbol, Find, Find Next/Previous, Find in Project, Replace, Goto "
         "Definition, navigation history, procedure navigation and bookmarks."),
        ('<a href="menu-reference.html#view-menu">View</a>',
         "Panel visibility, zoom, editor splitting, code folding and window layout."),
        ('<a href="menu-reference.html#project-menu">Project</a>',
         "New, Open, Recent Projects, Close, Save, Save As, Add Files and Project "
         "Options."),
        ('<a href="menu-reference.html#compile-menu">Compile</a>',
         "Build and Execute, Compile, Rebuild All, Compile Module, Quick Run, Run "
         "Executable and Command Line."),
        ('<a href="menu-reference.html#debug-menu">Debug</a>',
         "Start/Continue, Break, Stop, the stepping commands, Run to Cursor, breakpoints "
         "and Unused Symbols."),
        ('<a href="menu-reference.html#help-menu">Help</a>', "Help Center and About."),
    ],
)

MB += h2("Reading a menu item")
MB += p("Each item can carry three pieces of information:")
MB += ul([
    "<strong>The command name</strong> — what it does. A name ending in an ellipsis "
    "(<code>…</code>) opens a dialog rather than acting immediately.",
    "<strong>The keyboard shortcut</strong>, shown right-aligned. This always reflects "
    "your <em>current</em> bindings, so if you remap a command the menu updates to match.",
    "<strong>A submenu arrow</strong> — Open Recent, Settings, User Tools, Format and "
    "Recent Projects each open a further menu.",
])
MB += note(
    "Items that cannot apply right now are shown dimmed. Most Debug commands, for example, "
    "are only available while a debugging session is running."
)

MB += h2("Dynamic menus")
MB += p("Four submenus are rebuilt each time you open them:")
MB += ul([
    "%s — the files you opened most recently." % menu("File", "Open Recent"),
    "%s — the workspaces you opened most recently." % menu("Project", "Recent Projects"),
    "%s — the external tools you have defined." % menu("File", "User Tools"),
    "%s — reflects whether a debugging session is active, showing "
    "<strong>Start Debugging</strong> or <strong>Continue Debugging</strong>."
    % menu("Debug"),
])
MB += p(
    "Both recent lists hold up to ten entries and end with <strong>Clear this list</strong>. "
    "An empty list shows a single dimmed <strong>(Empty)</strong> entry."
)

MB += h2("Compact menus")
MB += p(
    "Menu rows can be drawn at a tighter height. Turn on <strong>Compact menus</strong> in "
    "%s if you prefer denser menus or work on a small screen. Context menus always use the "
    "compact height — they are short, transient lists rather than something you browse."
    % menu("File", "Settings", "Options…")
)

MB += h2("Related topics")
MB += ul([
    '<a href="menu-reference.html">Menu reference</a> — every command, in detail.',
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
    '<a href="context-menus.html">Context menus</a>',
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>',
])

page("menu-bar", "Menu bar", "ui",
     "How Tiko's menus are organised, how to read a menu item, and which menus rebuild "
     "themselves as you work.",
     MB,
     keywords="menu bar menus file edit search view project compile debug help "
              "accelerator shortcut compact menus")

# ==========================================================================
# Panel icon strip / toolbar
# ==========================================================================

TB = ""
TB += p(
    "Tiko does not have a traditional wide toolbar across the top of the window. Instead a "
    "narrow <strong>icon strip</strong> runs down the side of the window, beside the side "
    "panel. It does the same job in less space."
)
TB += p("The strip is split into two groups:")
TB += ul([
    "<strong>Panel selectors</strong> at one end — Explorer, Functions, Bookmarks and "
    "Settings. These <em>latch</em>: the active panel stays highlighted, and an underline "
    "marks which one you are looking at.",
    "<strong>Commands</strong> at the other end — Find, Save, Debug, Compile and Build. "
    "These are momentary buttons that fire and return to normal.",
])
TB += note(
    "Hover any button to see a tooltip naming the command and its keyboard shortcut."
)

TB += h2("Buttons")
TB += table(
    ["Button", "Action", "Shortcut"],
    [
        ("Explorer", "Show the project Explorer in the side panel.", kbd("Ctrl", "F4")),
        ("Functions", "Show the function list for the current file.", kbd("F4")),
        ("Bookmarks", "Show the bookmarks list.", kbd("Shift", "F4")),
        ("Settings", "Open the options dialog.", kbd("Ctrl", ",")),
        ("Find", "Open the Find bar.", kbd("Ctrl", "F")),
        ("Save", "Save the current document.", kbd("Ctrl", "S")),
        ("Debug", "Start or continue debugging.", kbd("F6")),
        ("Compile", "Compile without running.", kbd("Ctrl", "F5")),
        ("Build", "Build and execute.", kbd("F5")),
    ],
    key_first=True,
)
TB += todo(
    "Confirm the exact button set and their order against the shipping build, and add a "
    "close-up screenshot of the icon strip.",
    title="TODO — verify icon strip contents",
)
TB += placeholder("Panel icon strip", "Close-up screenshot of the icon strip",
                  caption="Replace with a capture of the icon strip showing both groups.")

TB += h2("Hiding the strip")
TB += p(
    "The icon strip is part of the side panel, so %s hides and shows both together. With "
    "them hidden, the editor takes the whole window width." % kbd("Ctrl", "B")
)

TB += h2("Related topics")
TB += ul([
    '<a href="side-panels.html">Side panels</a>',
    '<a href="user-tools.html">User tools</a> — add your own external commands.',
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
])

page("toolbar", "Toolbar and icon strip", "ui",
     "Tiko's compact icon strip: which buttons switch panels, which run commands, and how "
     "to hide it.",
     TB,
     keywords="toolbar icon strip buttons panel selector commands find save compile "
              "build debug")

# ==========================================================================
# Side panels
# ==========================================================================

SP = ""
SP += p(
    "The side panel is a single dock that shows one of three views at a time. Switch "
    "between them with the icon strip or their keyboard shortcuts."
)
SP += table(
    ["View", "Shows", "Shortcut"],
    [
        ("Explorer", "The files in the current workspace, grouped by category.",
         kbd("Ctrl", "F4")),
        ("Functions", "Every procedure in the current file or project.", kbd("F4")),
        ("Bookmarks", "Every bookmark you have set.", kbd("Shift", "F4")),
    ],
    key_first=True,
)
SP += p(
    "%s shows and hides the whole panel, including the icon strip. The panel can be docked "
    "on either side of the window via %s."
    % (kbd("Ctrl", "B"), menu("View", "Move Explorer Window Left/Right"))
)

SP += h2("Explorer")
SP += p(
    "The Explorer is a tree of the files in your workspace, grouped under five category "
    "headers. The headers are always present, whether or not they currently hold files."
)
SP += ul([
    "<strong>Click</strong> a file to open it, or to bring its tab forward if it is "
    "already open.",
    "<strong>Right-click</strong> for a context menu of file commands.",
    "Use the expand-all and collapse-all commands to open or close the whole tree at once.",
    "While the workspace is untitled, a pinned <strong>Save as Project…</strong> row sits "
    "at the top — the quickest way to give the workspace a name.",
])
SP += note(
    "Closing a file's tab does not remove it from the workspace. Files are removed "
    'explicitly. See <a href="project-files.html">Adding and removing project files</a>.'
)
SP += placeholder("Explorer panel", "Screenshot of the Explorer tree with categories",
                  caption="Replace with a capture of the Explorer showing the five "
                          "category headers and several files.")

SP += h2("Functions")
SP += p(
    "The function list shows the procedures Tiko's parser found in your code — subs, "
    "functions, and their containing types. Click an entry to jump to its definition."
)
SP += ul([
    "The list follows the active document as you switch tabs.",
    "It refreshes as you type, because parsing runs in the background.",
    "Expand-all and collapse-all commands are available for the tree.",
])
SP += tip(
    "The function list is the fastest way to navigate a long file. For jumping across the "
    "<em>whole project</em>, use Search Symbol (%s) instead." % kbd("Ctrl", "P")
)

SP += h2("Bookmarks")
SP += p(
    "The bookmarks list gathers every bookmark in one place, showing the file and line of "
    "each. Click one to jump to it."
)
SP += p(
    "Set and clear bookmarks with %s, and cycle through them with %s and %s. "
    "%s clears them all."
    % (kbd("Ctrl", "F2"), kbd("F2"), kbd("Shift", "F2"), kbd("Ctrl", "Shift", "F2"))
)

SP += h2("Related topics")
SP += ul([
    '<a href="project-explorer.html">Project Explorer</a> — the Explorer in depth.',
    '<a href="navigation.html">Navigation</a>',
    '<a href="bookmarks.html">Bookmarks</a>',
])

page("side-panels", "Side panels", "ui",
     "The Explorer, function list and bookmarks views that share Tiko's side dock, and how "
     "to switch, resize and hide them.",
     SP,
     keywords="side panel explorer functions bookmarks dock tree navigation")

# ==========================================================================
# Output panel
# ==========================================================================

OP = ""
OP += p(
    "The Output panel is docked below the editor and carries everything Tiko has to tell "
    "you, on separate tabs. Toggle it with %s." % kbd("Ctrl", "F9")
)
OP += table(
    ["Tab", "Contents"],
    [
        ("Compiler", "Build output — the command line used, compiler messages, and the "
         "result. Errors and warnings are clickable."),
        ("Search results", "Matches from Find in Project. Each row is a file, line and "
         "the matching text; click to jump there."),
        ("TODO", "Every <code>TODO</code> comment found in the current file or project, "
         "collected automatically."),
        ("Notes", "A free-text scratchpad saved with the workspace. Use it for anything "
         "you want to remember about this project."),
    ],
    key_first=True,
)

OP += h2("Working with results")
OP += ul([
    "<strong>Click a row</strong> to open that file and jump to the line.",
    "Compiler errors carry the file, line and message exactly as the compiler reported "
    "them.",
    "The Search results and TODO lists are multi-column and sortable by clicking a column "
    "header.",
])

OP += h2("Minimising")
OP += p(
    "Double-click the tab strip to collapse the panel down to just its tabs, reclaiming "
    "the vertical space while leaving the tabs within reach. Click any tab to restore it. "
    "Drag the splitter above the panel to set a specific height."
)
OP += note(
    "The panel's height, which tab was active, and whether it was open or minimised are "
    "all remembered between sessions."
)

OP += h2("Notes")
OP += p(
    "The Notes tab is a plain multi-line text field stored with the workspace, not with any "
    "one file. It survives closing and reopening the project. Use it for build reminders, "
    "outstanding questions or a scratch list."
)

OP += h2("Related topics")
OP += ul([
    '<a href="compiler-errors.html">Compiler errors and warnings</a>',
    '<a href="find-in-project.html">Find in Project</a>',
    '<a href="building.html">Building programs</a>',
])

page("output-panel", "Output panel", "ui",
     "The docked panel below the editor: compiler output, search results, collected TODO "
     "comments and project notes.",
     OP,
     keywords="output panel compiler results search results todo notes errors warnings "
              "minimise dock")

# ==========================================================================
# Tabs and splits
# ==========================================================================

TS = ""
TS += p(
    "Every open document gets a tab above the editor. The tab strip scrolls when there are "
    "more tabs than fit."
)
TS += h2("Working with tabs")
TS += table(
    ["Action", "How"],
    [
        ("Switch to a tab", "Click it, or use " + kbd("Ctrl", "Tab") + " and " +
         kbd("Ctrl", "Shift", "Tab") + "."),
        ("Close a tab", "Click its × button, or press " + kbd("Ctrl", "W") + "."),
        ("Close every tab", kbd("Ctrl", "Shift", "W")),
        ("Reorder tabs", "Drag a tab along the strip."),
        ("Scroll the strip", "Use the scroll buttons at the end of the strip."),
        ("List all open files", "Open the tab list button at the end of the strip — "
         "useful when more files are open than fit."),
        ("Tab context menu", "Right-click a tab for close, save and file commands."),
    ],
    key_first=True,
)
TS += note(
    "A tab marks its document as modified until you save. Closing a modified document "
    "prompts you first."
)

TS += h2("Split views")
TS += p(
    "The editor can be split into two views of the <em>same</em> document, so you can look "
    "at two parts of one file at once — a declaration at the top and its use hundreds of "
    "lines further down, for instance. Edits in either view appear immediately in the other."
)
TS += table(
    ["Command", "Shortcut"],
    [
        ("Toggle split left/right", kbd("Ctrl", "\\")),
        ("Toggle split top/bottom", kbd("Ctrl", "Shift", "\\")),
    ],
    key_first=True,
)
TS += p(
    "Both commands toggle: run the same one again to return to a single view. Drag the "
    "splitter between the two views to change their relative sizes."
)
TS += tip(
    "Split top/bottom suits reading code; split left/right suits comparing two procedures "
    "side by side on a wide screen."
)

TS += h2("Related topics")
TS += ul([
    '<a href="navigation.html">Navigation</a> — moving between files and symbols.',
    '<a href="main-window.html">The main window</a>',
])

page("tabs-and-splits", "Tabs and split views", "ui",
     "Managing document tabs, and splitting the editor into two synchronised views of the "
     "same file.",
     TS,
     keywords="tabs document tabs close reorder tab list split view split editor "
              "left right top bottom")

# ==========================================================================
# Status bar
# ==========================================================================

SB = ""
SB += p(
    "The status bar along the bottom of the window reports the state of the active "
    "document. Several of its fields are interactive."
)
SB += table(
    ["Field", "Shows", "Click to"],
    [
        ("Caret position", "Current line and column number.",
         "Nothing — informational."),
        ("Insert mode", "Whether typing inserts or overwrites.",
         "Toggle, or press " + kbd("Insert") + "."),
        ("Encoding", "The document's character encoding, such as UTF-8.",
         "Change the encoding — see " +
         '<a href="encoding.html">Encoding</a>.'),
        ("Line endings", "CRLF (Windows), LF (Unix) or CR.",
         "Change the line-ending style."),
        ("Language", "The syntax highlighting scheme in use.",
         "Choose a different language for this file."),
        ("Build configuration", "The active build configuration.",
         "Switch configuration — see " +
         '<a href="build-configurations.html">Build configurations</a>.'),
    ],
)
SB += todo(
    "Confirm the exact field set, order and click behaviour of the status bar against the "
    "shipping build, and add an annotated close-up screenshot.",
    title="TODO — verify status bar fields",
)
SB += placeholder("Status bar", "Annotated close-up of the status bar",
                  caption="Replace with a capture of the status bar with each field "
                          "labelled.")

SB += h2("Related topics")
SB += ul([
    '<a href="encoding.html">Encoding and line endings</a>',
    '<a href="build-configurations.html">Build configurations</a>',
    '<a href="main-window.html">The main window</a>',
])

page("status-bar", "Status bar", "ui",
     "What each status bar field reports, and which ones you can click to change a "
     "document setting.",
     SB,
     keywords="status bar line column caret position insert overwrite encoding line "
              "endings crlf language build configuration")

# ==========================================================================
# Context menus
# ==========================================================================

CM = ""
CM += p(
    "Right-clicking almost anything in Tiko opens a context menu relevant to what is under "
    "the pointer. Context menus are drawn by the editor and follow your theme, and they "
    "always use the compact row height."
)
CM += table(
    ["Right-click on", "You get"],
    [
        ("The editor surface", "Clipboard commands, selection, commenting, formatting, "
         "Goto Definition and bookmark commands."),
        ("A document tab", "Close, close others, save, and file commands for that tab."),
        ("The Explorer tree", "Open, remove from project, rename and file commands."),
        ("The function list", "Jump to definition and list commands."),
        ("The Output panel", "Copy, clear and result-list commands."),
        ("A text field in a dialog", "Cut, copy, paste, select all and undo."),
    ],
)
CM += note(
    "Context menus support full keyboard navigation: arrow keys to move, %s to choose, "
    "%s to dismiss. They also close when you click outside them."
    % (kbd("Enter"), kbd("Esc"))
)
CM += todo(
    "Enumerate the exact items on each context menu from the shipping build and replace "
    "the summary table above with per-menu item lists.",
    title="TODO — enumerate context menu items",
)

CM += h2("Related topics")
CM += ul([
    '<a href="menu-reference.html">Menu reference</a>',
    '<a href="dialog-reference.html">Dialog reference</a>',
])

page("context-menus", "Context menus", "ui",
     "The right-click menus available across the editor, panels, tabs and dialogs.",
     CM,
     keywords="context menu right click popup menu editor tab explorer output")

# ==========================================================================
# Dialogs
# ==========================================================================

DG = ""
DG += p(
    "Tiko's dialogs are owner-drawn, so they follow your theme rather than the system grey "
    "of standard Windows dialogs. They behave conventionally in every other respect."
)
DG += h2("Common behaviour")
DG += ul([
    "%s activates the default button; %s cancels and closes without saving changes."
    % (kbd("Enter"), kbd("Esc")),
    "%s and %s move between controls. The focused control is outlined."
    % (kbd("Tab"), kbd("Shift", "Tab")),
    "<strong>Cancel means cancel.</strong> Dialogs edit a working copy of your settings, "
    "so closing with Cancel leaves everything exactly as it was.",
    "Dialogs remember the page you last had open where that makes sense.",
])
DG += important(
    "Because settings dialogs work on a copy, nothing you change takes effect until you "
    "choose <strong>OK</strong>. Conversely, once you choose OK the change is written to "
    "<code>settings.ini</code> immediately."
)

DG += h2("The main dialogs")
DG += table(
    ["Dialog", "Opened from", "Purpose"],
    [
        ('<a href="dialog-reference.html#options">Options</a>',
         menu("File", "Settings", "Options…"),
         "Every editor behaviour, across eight pages."),
        ('<a href="dialog-reference.html#themes">Themes</a>',
         menu("File", "Settings", "Themes…"),
         "Choose and edit colour themes."),
        ('<a href="dialog-reference.html#build-config">Build Configurations</a>',
         menu("File", "Settings", "Build Configurations…"),
         "Define named sets of compiler switches."),
        ('<a href="dialog-reference.html#keyboard">Keyboard Shortcuts</a>',
         menu("File", "Settings", "Keyboard Shortcuts…"),
         "Remap any command."),
        ('<a href="dialog-reference.html#user-tools">User Tools</a>',
         menu("File", "User Tools…"),
         "Define external programs to run from the menu."),
        ('<a href="dialog-reference.html#format-options">Format Options</a>',
         menu("Edit", "Format", "Format Options…"),
         "Configure the code formatter, with a live preview."),
        ('<a href="dialog-reference.html#project-options">Project Options</a>',
         menu("Project", "Project Options…"),
         "Per-project compiler options and build output settings."),
        ('<a href="dialog-reference.html#find">Find and Replace</a>',
         kbd("Ctrl", "F") + " / " + kbd("Ctrl", "H"),
         "Search within the current document."),
        ('<a href="dialog-reference.html#find-in-project">Find in Project</a>',
         kbd("Ctrl", "Shift", "F"),
         "Search across every file in the workspace."),
        ('<a href="dialog-reference.html#about">About</a>', menu("Help", "About"),
         "Version, credits and licence."),
    ],
)

DG += h2("Related topics")
DG += ul([
    '<a href="dialog-reference.html">Dialog reference</a> — every dialog, control by '
    "control.",
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("dialogs", "Dialog boxes", "ui",
     "How Tiko's dialogs behave, and an index of the main ones with what each is for.",
     DG,
     keywords="dialog dialogs options themes build configurations keyboard user tools "
              "format options project options find replace about cancel ok")
