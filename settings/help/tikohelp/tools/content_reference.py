# -*- coding: utf-8 -*-
"""Reference section: shortcuts, menus, commands, configuration, dialogs.

The shortcut and command tables are generated from KEYS below, which mirrors
Tiko Editor's own default key-binding table so that both reference pages stay
consistent with each other.
"""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui, esc,
                   placeholder, diagram)

section("reference", "Reference", "list")

# --------------------------------------------------------------------------
# The command table.
#   (category, command id, name, default shortcut, menu location, description)
# --------------------------------------------------------------------------

KEYS = [
    # ---- File ----
    ("File", "IDM_FILENEW", "New", "Ctrl+N", "File ▸ New",
     "Create a new, empty document."),
    ("File", "IDM_FILEOPEN", "Open…", "Ctrl+O", "File ▸ Open…",
     "Open one or more existing files."),
    ("File", "IDM_FILEOPENASTEMPLATE", "Open File As Template…", "",
     "File ▸ Open File As Template…",
     "Open a file's contents as a new untitled document, leaving the original untouched."),
    ("File", "IDM_FILECLOSE", "Close", "Ctrl+W", "File ▸ Close",
     "Close the active document. It stays in the project."),
    ("File", "IDM_FILECLOSEALL", "Close All", "Ctrl+Shift+W", "File ▸ Close All",
     "Close every open document."),
    ("File", "IDM_FILESAVE", "Save", "Ctrl+S", "File ▸ Save",
     "Save the active document. Prompts for a name if it has never been saved."),
    ("File", "IDM_FILESAVEAS", "Save As…", "", "File ▸ Save As…",
     "Save under a new name and continue editing the new file."),
    ("File", "IDM_FILESAVEALL", "Save All", "Ctrl+Shift+S", "File ▸ Save All",
     "Save every modified document."),
    ("File", "IDM_FILERENAME", "Rename…", "", "File ▸ Rename…",
     "Rename the file on disk and update the project."),
    ("File", "IDM_FILEDUPLICATE", "Duplicate", "", "File ▸ Duplicate",
     "Create a copy of the file and add it to the project."),
    ("File", "IDM_OPTIONSDIALOG", "Options…", "Ctrl+,",
     "File ▸ Settings ▸ Options…", "Open the main options dialog."),
    ("File", "IDM_THEMES", "Themes…", "Ctrl+Shift+T", "File ▸ Settings ▸ Themes…",
     "Choose and edit colour themes."),
    ("File", "IDM_KEYBOARDSHORTCUTS", "Keyboard Shortcuts…", "Ctrl+K",
     "File ▸ Settings ▸ Keyboard Shortcuts…", "Remap any command."),
    ("File", "IDM_BUILDCONFIG", "Build Configurations…", "F7",
     "File ▸ Settings ▸ Build Configurations…",
     "Manage named sets of compiler switches."),
    ("File", "IDM_USERTOOLSDIALOG", "User Tools…", "", "File ▸ User Tools…",
     "Define external programs to run from the menu."),
    ("File", "IDM_EXIT", "Exit", "Alt+F4", "File ▸ Exit",
     "Close Tiko Editor, prompting to save modified documents."),

    # ---- Edit ----
    ("Edit", "IDM_UNDO", "Undo", "Ctrl+Z", "Edit ▸ Undo", "Undo the last change."),
    ("Edit", "IDM_REDO", "Redo", "Ctrl+Shift+Z", "Edit ▸ Redo", "Reapply an undone change."),
    ("Edit", "IDM_CUT", "Cut", "Ctrl+X", "Edit ▸ Cut",
     "Cut the selection, or the whole line if nothing is selected."),
    ("Edit", "IDM_COPY", "Copy", "Ctrl+C", "Edit ▸ Copy",
     "Copy the selection, or the whole line if nothing is selected."),
    ("Edit", "IDM_PASTE", "Paste", "Ctrl+V", "Edit ▸ Paste",
     "Paste the clipboard as plain text."),
    ("Edit", "IDM_DELETELINE", "Delete Line", "Ctrl+Y", "Edit ▸ Delete Line",
     "Delete the current line."),
    ("Edit", "IDM_DUPLICATELINE", "Duplicate Line", "Ctrl+D", "Edit ▸ Duplicate Line",
     "Insert a copy of the current line below it."),
    ("Edit", "IDM_MOVELINEUP", "Move Line Up", "Alt+Up", "Edit ▸ Move Line Up",
     "Swap the current line or selection with the line above."),
    ("Edit", "IDM_MOVELINEDOWN", "Move Line Down", "Alt+Down", "Edit ▸ Move Line Down",
     "Swap the current line or selection with the line below."),
    ("Edit", "IDM_COMMENTBLOCK", "Comment Block", "Ctrl+/", "Edit ▸ Comment Block",
     "Comment out the selected lines."),
    ("Edit", "IDM_UNCOMMENTBLOCK", "UnComment Block", "Ctrl+Shift+/",
     "Edit ▸ UnComment Block", "Remove comment marks from the selected lines."),
    ("Edit", "IDM_SELECTLINE", "Select Line", "Ctrl+L", "Edit ▸ Select Line",
     "Select the whole current line."),
    ("Edit", "IDM_SELECTALL", "Select All", "Ctrl+A", "Edit ▸ Select All",
     "Select the entire document."),
    ("Edit", "IDM_INDENTBLOCK", "Indent Block", "Tab", "—",
     "Indent every line of a multi-line selection."),
    ("Edit", "IDM_UNINDENTBLOCK", "UnIndent Block", "Shift+Tab", "—",
     "Remove one indent level from every selected line."),
    ("Edit", "IDM_INSERTFILE", "Insert File", "Ctrl+I", "—",
     "Insert another file's contents at the caret."),
    ("Edit", "IDM_TOUPPERCASE", "Uppercase", "Ctrl+Alt+U", "—",
     "Convert the selection to upper case."),
    ("Edit", "IDM_TOLOWERCASE", "Lowercase", "Ctrl+Alt+L", "—",
     "Convert the selection to lower case."),
    ("Edit", "IDM_TOMIXEDCASE", "Mixed case", "Ctrl+Alt+X", "—",
     "Convert the selection to mixed case."),
    ("Edit", "IDM_NEWLINEBELOWCURRENT", "New Line Below", "Ctrl+Enter", "—",
     "Open a new line below the current one from anywhere on it."),
    ("Edit", "IDM_SETFOCUSEDITOR", "Focus Editor", "Ctrl+`", "—",
     "Return keyboard focus to the editing window."),
    ("Edit", "IDM_FORMATDOCUMENT", "Format Document", "Shift+Alt+F",
     "Edit ▸ Format ▸ Format Document", "Reformat the whole active file."),
    ("Edit", "IDM_FORMATSELECTION", "Format Selection", "",
     "Edit ▸ Format ▸ Format Selection", "Reformat only the selected lines."),
    ("Edit", "IDM_FORMATALLDOCS", "Format All Open Documents", "",
     "Edit ▸ Format ▸ Format All Open Documents", "Reformat every open document."),
    ("Edit", "IDM_FORMATPROJECT", "Format Project…", "",
     "Edit ▸ Format ▸ Format Project…", "Reformat every file in the workspace."),
    ("Edit", "IDM_FORMATOPTIONS", "Format Options…", "",
     "Edit ▸ Format ▸ Format Options…", "Configure the formatter, with a live preview."),

    # ---- Search ----
    ("Search", "IDM_SEARCHSYMBOL", "Search Symbol…", "Ctrl+P", "Search ▸ Search Symbol…",
     "Filter every symbol in the project and jump to one."),
    ("Search", "IDM_FIND", "Find…", "Ctrl+F", "Search ▸ Find…",
     "Search within the active document."),
    ("Search", "IDM_FINDNEXTACCEL", "Find Next", "F3", "Search ▸ Find Next",
     "Repeat the last search forwards."),
    ("Search", "IDM_FINDPREVACCEL", "Find Previous", "Shift+F3", "Search ▸ Find Previous",
     "Repeat the last search backwards."),
    ("Search", "IDM_FINDINPROJECT", "Find in Project…", "Ctrl+Shift+F",
     "Search ▸ Find in Project…", "Search every file in the workspace."),
    ("Search", "IDM_REPLACE", "Replace…", "Ctrl+H", "Search ▸ Replace…",
     "Find and replace within the active document."),
    ("Search", "IDM_GOTODEFINITION", "Goto Definition", "F12", "Search ▸ Goto Definition",
     "Jump to where the symbol under the caret is defined."),
    ("Search", "IDM_GOBACK", "Go Back", "Alt+Left", "Search ▸ Go Back",
     "Return to the previous position in the navigation history."),
    ("Search", "IDM_GOFORWARD", "Go Forward", "Alt+Right", "Search ▸ Go Forward",
     "Move forward again in the navigation history."),
    ("Search", "IDM_GOTONEXTFUNCTION", "Next Function", "Ctrl+PgDn",
     "Search ▸ Next Function", "Jump to the next procedure in the file."),
    ("Search", "IDM_GOTOPREVFUNCTION", "Previous Function", "Ctrl+PgUp",
     "Search ▸ Previous Function", "Jump to the previous procedure."),
    ("Search", "IDM_BOOKMARKTOGGLE", "Toggle Bookmark", "Ctrl+F2",
     "Search ▸ Toggle Bookmark", "Set or clear a bookmark on the current line."),
    ("Search", "IDM_BOOKMARKNEXT", "Next Bookmark", "F2", "Search ▸ Next Bookmark",
     "Jump to the next bookmark."),
    ("Search", "IDM_BOOKMARKPREV", "Previous Bookmark", "Shift+F2",
     "Search ▸ Previous Bookmark", "Jump to the previous bookmark."),
    ("Search", "IDM_BOOKMARKCLEARALL", "Clear Bookmarks", "Ctrl+Shift+F2",
     "Search ▸ Clear Bookmarks", "Remove every bookmark."),
    ("Search", "IDM_GOTOHEADERFILE", "Goto Header File", "Ctrl+Shift+H", "—",
     "Switch to the matching header file."),
    ("Search", "IDM_GOTOSOURCEFILE", "Goto Code File", "Ctrl+Shift+C", "—",
     "Switch to the matching source file."),
    ("Search", "IDM_GOTOMAINFILE", "Goto Main File", "Ctrl+Shift+M", "—",
     "Switch to the project's main module."),
    ("Search", "IDM_GOTORESOURCEFILE", "Goto Resource File", "Ctrl+Shift+R", "—",
     "Switch to the project's resource script."),
    ("Search", "IDM_GOTONEXTTAB", "Next Tab", "Ctrl+Tab", "—",
     "Activate the next open document."),
    ("Search", "IDM_GOTOPREVTAB", "Previous Tab", "Ctrl+Shift+Tab", "—",
     "Activate the previous open document."),

    # ---- View ----
    ("View", "IDM_VIEWSIDEPANEL", "View Side Panel", "Ctrl+B", "View ▸ View Side Panel",
     "Show or hide the side panel and its icon strip."),
    ("View", "IDM_VIEWEXPLORER", "View Explorer Window", "Ctrl+F4",
     "View ▸ View Explorer Window", "Show the project Explorer."),
    ("View", "IDM_VIEWOUTPUT", "View Output Window", "Ctrl+F9",
     "View ▸ View Output Window", "Show or hide the Output panel."),
    ("View", "IDM_FUNCTIONLIST", "View Function List", "F4", "View ▸ View Function List",
     "Show the function list for the active document."),
    ("View", "IDM_BOOKMARKSLIST", "View Bookmarks List", "Shift+F4",
     "View ▸ View Bookmarks List", "Show every bookmark in the side panel."),
    ("View", "IDM_ZOOMIN", "Zoom In", "Ctrl++", "View ▸ Zoom In",
     "Increase the displayed text size."),
    ("View", "IDM_ZOOMOUT", "Zoom Out", "Ctrl+-", "View ▸ Zoom Out",
     "Decrease the displayed text size."),
    ("View", "IDM_ZOOMRESET", "Zoom Reset", "Ctrl+0", "View ▸ Zoom Reset",
     "Return to the configured font size."),
    ("View", "IDM_SPLITLEFTRIGHT", "Toggle Split Editor Left/Right", "Ctrl+\\",
     "View ▸ Toggle Split Editor Left/Right",
     "Split the editor into two side-by-side views of the same file."),
    ("View", "IDM_SPLITTOPBOTTOM", "Toggle Split Editor Top/Bottom", "Ctrl+Shift+\\",
     "View ▸ Toggle Split Editor Top/Bottom", "Split the editor horizontally."),
    ("View", "IDM_FOLDTOGGLE", "Toggle Current Fold Point", "F8",
     "View ▸ Toggle Current Fold Point", "Collapse or expand the block at the caret."),
    ("View", "IDM_FOLDBELOW", "Toggle Current And All Below", "Ctrl+F8",
     "View ▸ Toggle Current And All Below", "Fold the current block and everything in it."),
    ("View", "IDM_FOLDALL", "Fold All", "Shift+F8", "View ▸ Fold All",
     "Collapse every block in the document."),
    ("View", "IDM_UNFOLDALL", "Unfold All", "Ctrl+Shift+F8", "View ▸ Unfold All",
     "Expand every block."),
    ("View", "IDM_EXPLORERPOSITION", "Move Explorer Window", "",
     "View ▸ Move Explorer Window Left/Right", "Dock the side panel on the other side."),
    ("View", "IDM_RESTOREMAIN", "Restore Main Window Size", "",
     "View ▸ Restore Main Window Size", "Return the main window to its default size."),
    ("View", "IDM_VIEWNOTES", "Notes", "", "Output panel ▸ Notes",
     "Show the workspace notes tab."),
    ("View", "IDM_VIEWTODO", "TODO", "", "Output panel ▸ TODO",
     "Show the collected TODO comments."),
    ("View", "IDM_EXPLORER_EXPANDALL", "Explorer: Expand All", "", "—",
     "Expand every node in the Explorer."),
    ("View", "IDM_EXPLORER_COLLAPSEALL", "Explorer: Collapse All", "", "—",
     "Collapse every node in the Explorer."),
    ("View", "IDM_FUNCTIONS_EXPANDALL", "Functions: Expand All", "", "—",
     "Expand every node in the function list."),
    ("View", "IDM_FUNCTIONS_COLLAPSEALL", "Functions: Collapse All", "", "—",
     "Collapse every node in the function list."),
    ("View", "IDM_BOOKMARKS_EXPANDALL", "Bookmarks: Expand All", "", "—",
     "Expand every node in the bookmarks list."),
    ("View", "IDM_BOOKMARKS_COLLAPSEALL", "Bookmarks: Collapse All", "", "—",
     "Collapse every node in the bookmarks list."),
    ("View", "IDM_CLOSEPANEL", "Close Side Panel", "", "—", "Close the side panel."),

    # ---- Project ----
    ("Project", "IDM_PROJECTNEW", "New Project…", "", "Project ▸ New Project…",
     "Close the workspace and start a fresh untitled one."),
    ("Project", "IDM_PROJECTOPEN", "Open Project…", "", "Project ▸ Open Project…",
     "Open a saved project file."),
    ("Project", "IDM_PROJECTCLOSE", "Close Project", "", "Project ▸ Close Project",
     "Close the workspace and its files."),
    ("Project", "IDM_PROJECTSAVE", "Save Project", "", "Project ▸ Save Project",
     "Write the project file."),
    ("Project", "IDM_PROJECTSAVEAS", "Save Project As…", "", "Project ▸ Save Project As…",
     "Save the project under a new name — also how an untitled workspace is named."),
    ("Project", "IDM_PROJECTFILESADD", "Add Files to Project…", "Ctrl+F11",
     "Project ▸ Add Files to Project…", "Add existing files to the workspace."),
    ("Project", "IDM_PROJECTOPTIONS", "Project Options…", "",
     "Project ▸ Project Options…", "Edit this project's options."),

    # ---- Compile ----
    ("Compile", "IDM_BUILDEXECUTE", "Build And Execute", "F5",
     "Compile ▸ Build And Execute", "Compile, then run if the compile succeeded."),
    ("Compile", "IDM_COMPILE", "Compile", "Ctrl+F5", "Compile ▸ Compile",
     "Compile without running."),
    ("Compile", "IDM_REBUILDALL", "Rebuild All", "Ctrl+Alt+F5", "Compile ▸ Rebuild All",
     "Rebuild everything from scratch."),
    ("Compile", "IDM_COMPILEMODULE", "Compile Module", "Ctrl+F7",
     "Compile ▸ Compile Module", "Compile only the current module."),
    ("Compile", "IDM_QUICKRUN", "Quick Run", "Ctrl+Shift+F5", "Compile ▸ Quick Run",
     "Compile and run the current file alone, ignoring the project."),
    ("Compile", "IDM_RUNEXE", "Run Executable", "Shift+F5", "Compile ▸ Run Executable",
     "Run the last successful build without recompiling."),
    ("Compile", "IDM_COMMANDLINE", "Command Line…", "", "Compile ▸ Command Line…",
     "Show the compiler command line Tiko Editor will use."),

    # ---- Debug ----
    ("Debug", "IDM_DEBUG_STARTDEBUGGING", "Start / Continue Debugging", "F6",
     "Debug ▸ Start Debugging", "Start a debugging session, or resume a stopped one."),
    ("Debug", "IDM_DEBUG_PAUSE", "Break", "Ctrl+F6", "Debug ▸ Break",
     "Interrupt the running program at the next source line."),
    ("Debug", "IDM_DEBUG_STOPDEBUGGING", "Stop Debugging", "Shift+F6",
     "Debug ▸ Stop Debugging", "End the session and terminate the program."),
    ("Debug", "IDM_DEBUG_STEPINTO", "Step", "F11", "Debug ▸ Step",
     "Execute one line, entering any procedure it calls."),
    ("Debug", "IDM_DEBUG_STEPOVER", "Step Over", "F10", "Debug ▸ Step Over",
     "Execute one line without stopping inside calls."),
    ("Debug", "IDM_DEBUG_STEPOUT", "Step Out", "Shift+F11", "Debug ▸ Step Out",
     "Run until the current procedure returns."),
    ("Debug", "IDM_DEBUG_RUNTOCURSOR", "Run to Cursor", "Ctrl+F10",
     "Debug ▸ Run to Cursor", "Resume and stop at the line holding the caret."),
    ("Debug", "IDM_DEBUG_TOGGLEBREAKPOINT", "Toggle Breakpoint", "F9",
     "Debug ▸ Toggle Breakpoint", "Set or clear a breakpoint on the current line."),
    ("Debug", "IDM_DEBUG_DELETEALLBREAKPOINTS", "Delete All Breakpoints",
     "Ctrl+Shift+F9", "Debug ▸ Delete All Breakpoints", "Remove every breakpoint."),
    ("Debug", "IDM_DEBUG_UNUSEDSYMBOLS", "Unused Symbols…", "",
     "Debug ▸ Unused Symbols…", "Report declared symbols the parser found no use of."),

    # ---- Help ----
    ("Help", "IDM_HELP_CENTER", "Help Center", "F1", "Help ▸ Help Center",
     "Open the Help Center, searching for the selection or the word under the caret."),
    ("Help", "IDM_ABOUT", "About", "", "Help ▸ About",
     "Show version, credits and licence information."),
]

CATEGORIES = ["File", "Edit", "Search", "View", "Project", "Compile", "Debug", "Help"]

CATEGORY_BLURB = {
    "File": "Creating, opening, saving and closing documents, plus the settings dialogs.",
    "Edit": "Undo and redo, the clipboard, line operations, commenting, case conversion "
            "and the code formatter.",
    "Search": "Finding text, navigating to symbols and definitions, and bookmarks.",
    "View": "Panel visibility, zoom, editor splitting and code folding.",
    "Project": "Creating, opening and saving projects, and managing their files.",
    "Compile": "Building, rebuilding and running your program.",
    "Debug": "Starting and controlling a debugging session, and breakpoints.",
    "Help": "The Help Center and version information.",
}


def fmt_keys(keys):
    if not keys:
        return '<span style="color:var(--c-text-mute)">—</span>'
    return " + ".join("<kbd>%s</kbd>" % esc(k) for k in keys.split("+"))


def fmt_menu(path):
    if path == "—":
        return '<span style="color:var(--c-text-mute)">No menu item</span>'
    parts = [esc(x.strip()) for x in path.split("▸")]
    return ('<span class="ui path">%s</span>'
            % '<span class="sep">›</span>'.join(parts))


# ==========================================================================
# Keyboard shortcuts
# ==========================================================================

KS = ""
KS += p(
    "Every shortcut below is the factory default. All of them can be changed — see "
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>. A command '
    "shown with a dash has no default shortcut, and is a good candidate if you need a free "
    "keystroke."
)
KS += tip(
    "This page prints cleanly. Use your browser's Print command for a reference sheet "
    "without the navigation."
)

KS += h2("The ten to learn first")
KS += p(
    "If you learn nothing else, learn these. They cover the great majority of daily use."
)
KS += table(
    ["Shortcut", "Command", "Why"],
    [
        (fmt_keys("Ctrl+S"), "Save", "Constantly."),
        (fmt_keys("Ctrl+F"), "Find", "The most-used navigation tool in any editor."),
        (fmt_keys("F3"), "Find Next", "Repeats the search without the Find bar open."),
        (fmt_keys("F12"), "Goto Definition", "Jump to where anything is defined."),
        (fmt_keys("Alt+Left"), "Go Back", "Return from that jump."),
        (fmt_keys("Ctrl+P"), "Search Symbol", "Jump anywhere in the project by name."),
        (fmt_keys("F5"), "Build And Execute", "The whole build cycle in one key."),
        (fmt_keys("F9"), "Toggle Breakpoint", "The start of every debugging session."),
        (fmt_keys("F6"), "Start Debugging", "Run under the debugger."),
        (fmt_keys("Ctrl+B"), "Toggle side panel", "Give the editor the whole window."),
    ],
    key_first=True,
)

for cat in CATEGORIES:
    rows = [(fmt_keys(k), esc(name), esc(desc))
            for c, cid, name, k, m, desc in KEYS if c == cat]
    KS += h2("%s commands" % cat)
    KS += p(CATEGORY_BLURB[cat])
    KS += table(["Shortcut", "Command", "Description"], rows, key_first=True)

KS += h2("Keys handled outside the command table")
KS += p(
    "These are handled by the editing surface itself, or claimed before the accelerator "
    "tables, so they do not appear in the Keyboard Shortcuts dialog and cannot be remapped "
    "there."
)
KS += table(
    ["Shortcut", "Action"],
    [
        (fmt_keys("Ctrl+Space"),
         "Raise the word autocomplete list. The only keyboard trigger for it — see "
         '<a href="autocomplete.html#ctrl-space">Autocomplete</a>.'),
        (fmt_keys("Home") + " / " + fmt_keys("End"), "Start / end of line."),
        (fmt_keys("Ctrl+Home") + " / " + fmt_keys("Ctrl+End"),
         "Start / end of document."),
        (fmt_keys("Ctrl+Left") + " / " + fmt_keys("Ctrl+Right"), "Previous / next word."),
        (fmt_keys("Shift") + " + any movement", "Extend the selection."),
        (fmt_keys("Alt") + " + drag", "Rectangular (column) selection."),
        (fmt_keys("Alt+Shift") + " + arrows", "Rectangular selection by keyboard."),
        (fmt_keys("Ctrl") + " + drag, click or double-click",
         "Add another selection or caret (multi-cursor editing)."),
        (fmt_keys("Insert"), "Toggle insert and overwrite mode."),
        (fmt_keys("Ctrl") + " + mouse wheel", "Zoom in and out."),
        (fmt_keys("Shift") + " + mouse wheel", "Scroll horizontally."),
    ],
    key_first=True,
)

KS += h2("Notation")
KS += ul([
    "<code>Ctrl+,</code> is the comma key — written <code>Comma</code> in the shortcut "
    "dialog's key list.",
    "<code>Ctrl+`</code> is the backtick or tilde key — written <code>Tilde</code>.",
    "<code>Ctrl++</code> and <code>Ctrl+-</code> are the plus and minus keys; the numeric "
    "keypad equivalents also work.",
])

KS += h2("Related topics")
KS += ul([
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>',
    '<a href="command-reference.html">Command reference</a>',
    '<a href="menu-reference.html">Menu reference</a>',
])

page("keyboard-shortcuts", "Keyboard shortcuts", "reference",
     "Every default keyboard shortcut in Tiko Editor, grouped by category and ready to print.",
     KS,
     keywords="keyboard shortcuts accelerators hotkeys keys reference print cheat sheet "
              "default bindings")

# ==========================================================================
# Menu reference
# ==========================================================================

MENUS = [
    ("file-menu", "File menu",
     "Document lifecycle — creating, opening, saving and closing — plus the settings "
     "dialogs, user tools and Exit.", "File"),
    ("edit-menu", "Edit menu",
     "Undo and redo, the clipboard, line operations, commenting, selection and the Format "
     "submenu.", "Edit"),
    ("search-menu", "Search menu",
     "Finding text, jumping to definitions and symbols, navigation history, procedure "
     "navigation and bookmarks.", "Search"),
    ("view-menu", "View menu",
     "What is visible and how it is displayed: panels, zoom, editor splitting, folding and "
     "window layout.", "View"),
    ("project-menu", "Project menu",
     "Creating, opening, saving and closing projects, and adding files to them.", "Project"),
    ("compile-menu", "Compile menu",
     "Every way of building and running your program.", "Compile"),
    ("debug-menu", "Debug menu",
     "Starting and controlling a debugging session, stepping, and breakpoints.", "Debug"),
    ("help-menu", "Help menu", "The Help Center and version information.", "Help"),
]

MR = ""
MR += p(
    "Every command on every menu, with its keyboard shortcut and a description. Commands "
    "whose names end in an ellipsis open a dialog."
)

for anchor, title, blurb, cat in MENUS:
    MR += h2(title, anchor=anchor)
    MR += p(blurb)
    rows = []
    for c, cid, name, k, m, desc in KEYS:
        if c != cat:
            continue
        rows.append((esc(name), fmt_keys(k), esc(desc)))
    MR += table(["Command", "Shortcut", "Description"], rows)

MR += h3("Submenus")
MR += table(
    ["Submenu", "On", "Contains"],
    [
        ("Open Recent", "File", "The ten most recently opened files, then "
         "<strong>Clear this list</strong>."),
        ("Settings", "File",
         "Options…, Themes…, Build Configurations…, Keyboard Shortcuts…"),
        ("User Tools", "File", "Your defined external tools, then <strong>User "
         "Tools…</strong> to manage them."),
        ("Format", "Edit", "Format Document, Format Selection, Format All Open Documents, "
         "Format Project…, Format Options…"),
        ("Recent Projects", "Project", "The ten most recently opened projects, then "
         "<strong>Clear this list</strong>."),
    ],
)

MR += h2("Commands with no menu item")
MR += p(
    "Some commands are reachable only by keyboard shortcut. They still appear in the "
    "Keyboard Shortcuts dialog and can be rebound."
)
MR += table(
    ["Command", "Shortcut", "Description"],
    [(esc(name), fmt_keys(k), esc(desc))
     for c, cid, name, k, m, desc in KEYS if m == "—"],
)

MR += h2("Related topics")
MR += ul([
    '<a href="command-reference.html">Command reference</a> — the same commands with '
    "their identifiers.",
    '<a href="keyboard-shortcuts.html">Keyboard shortcuts</a>',
    '<a href="menu-bar.html">Menu bar</a>',
])

page("menu-reference", "Menu reference", "reference",
     "Every menu and every command it holds, with shortcuts, descriptions and the "
     "submenus that rebuild themselves.",
     MR,
     keywords="menu reference file edit search view project compile debug help commands "
              "submenu open recent settings user tools format recent projects")

# ==========================================================================
# Command reference
# ==========================================================================

CR = ""
CR += p(
    "Every command in Tiko Editor, with its internal identifier, its default shortcut and where "
    "it lives. The identifier is the name used in "
    "<code>settings\\keybindings.ini</code> — useful when editing that file by hand or "
    "comparing configurations."
)
CR += note(
    "Commands are listed by category, matching the grouping in the Keyboard Shortcuts "
    "dialog."
)

for cat in CATEGORIES:
    rows = []
    for c, cid, name, k, m, desc in KEYS:
        if c != cat:
            continue
        rows.append((esc(name), "<code>%s</code>" % esc(cid), fmt_keys(k), fmt_menu(m)))
    CR += h2("%s" % cat, anchor="cmd-" + cat.lower())
    CR += table(["Command", "Command ID", "Shortcut", "Menu location"], rows)

CR += h2("Using command identifiers")
CR += p(
    "A line in <code>keybindings.ini</code> is a command identifier, a colon, and the "
    "keystroke:"
)
CR += code("""
IDM_DUPLICATELINE:Ctrl+Shift+D
IDM_FORMATSELECTION:Ctrl+Alt+F
IDM_BOOKMARKNEXT(DISABLED):
""", lang="ini", title="Binding commands by identifier", numbered=False)
CR += p(
    "A <code>(DISABLED)</code> marker suppresses that command's default shortcut without "
    "assigning a new one."
)
CR += warn(
    "An identifier Tiko Editor does not recognise is ignored, and the command keeps its default. "
    "Nothing reports the mistake, so check your spelling against the tables above."
)

CR += h2("Related topics")
CR += ul([
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>',
    '<a href="menu-reference.html">Menu reference</a>',
    '<a href="settings-files.html">Settings and configuration files</a>',
])

page("command-reference", "Command reference", "reference",
     "Every editor command with its internal identifier, default shortcut and menu "
     "location — and how to use identifiers in the keybindings file.",
     CR,
     keywords="command reference command id idm identifier keybindings.ini menu location "
              "shortcut disabled")
