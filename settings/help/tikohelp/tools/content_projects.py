# -*- coding: utf-8 -*-
"""Projects section."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   figure_img, placeholder, diagram)

section("projects", "Projects", "folder")

# ==========================================================================
# Overview / the workspace model
# ==========================================================================

PO = ""
PO += important(
    "<strong>In Tiko Editor, every workspace is a project.</strong> There is no \"no project "
    "open\" state. If you simply open a few loose files, those files <em>are</em> a "
    "project — an <strong>untitled</strong> one. Naming it later is a save, not a "
    "conversion."
)
PO += p(
    "This one rule removes a great deal of the ceremony other editors ask for. You never "
    "have to create a project before you can work, and you never lose your work because "
    "you forgot to."
)

PO += h2("Named and untitled workspaces")
PO += table(
    ["", "Untitled workspace", "Named project"],
    [
        ("Created by", "Just opening or creating files.",
         "Choosing <strong>Save as Project…</strong> and giving it a name."),
        ("Stored in", "<code>settings\\default.tiko</code>, automatically.",
         "A <code>.tiko</code> file wherever you put it."),
        ("Reopens on start-up", "Yes.", "Yes."),
        ("Appears in Recent Projects", "No — it has no name.", "Yes."),
        ("Shown in the window caption", "No project name.", "The project name."),
        ("Has project options", "Yes.", "Yes."),
    ],
)
PO += p(
    "Because the untitled workspace is saved automatically, the files you had open are "
    "always restored the next time you start — whether or not you ever named the project."
)

PO += h2("Creating a project")
PO += h3("Promote the workspace you already have")
PO += p("This is the usual route, and the easiest:")
PO += ol([
    "Open the files you want to work on.",
    "Click the pinned <strong>Save as Project…</strong> row at the top of the Explorer, "
    "or choose %s." % menu("Project", "Save Project As…"),
    "Choose a folder and a name. The workspace becomes a named project containing exactly "
    "the files you already had.",
], steps=True)
PO += h3("Start from nothing")
PO += p(
    "%s closes the current workspace and starts a fresh untitled one. There is no dialog — "
    "you are simply given a clean slate." % menu("Project", "New Project…")
)

PO += h2("Opening a project")
PO += ul([
    "%s and choose a <code>.tiko</code> file." % menu("Project", "Open Project…"),
    "%s lists the ten most recent, and ends with "
    "<strong>Clear this list</strong>." % menu("Project", "Recent Projects"),
    "Tiko Editor reopens your last workspace automatically when it starts.",
])
PO += note(
    "Opening a project closes the current workspace first, prompting you to save anything "
    "modified. If you cancel that prompt, the open is abandoned and nothing changes."
)

PO += h2("Saving and closing")
PO += table(
    ["Command", "What it does"],
    [
        ("Save Project", "Writes the project file — its file list and options."),
        ("Save Project As…", "Writes it under a new name; that becomes the current "
         "project. Also how an untitled workspace gets a name."),
        ("Close Project", "Closes the workspace and every file in it, then starts a fresh "
         "untitled workspace — because there is always a workspace."),
    ],
    key_first=True,
)
PO += p(
    "Saving the <em>project</em> and saving your <em>files</em> are separate. %s saves "
    "documents; Save Project saves the list of them. Tiko Editor prompts for unsaved documents "
    "when it needs to." % kbd("Ctrl", "S")
)

PO += h2("What is in a project")
PO += ul([
    "The list of files that belong to it, with each file's category.",
    "Which file is the main module, and which is the resource script.",
    "Project options — compiler switches, build output settings and command-line "
    "arguments.",
    "Project notes and the collected TODO list.",
])
PO += note(
    "A <code>.tiko</code> project file is a plain text file. You can put it in version "
    "control alongside your source, and diffs of it are readable."
)

PO += h2("File categories")
PO += p(
    "Every file in a project sits in exactly one of five categories, shown as permanent "
    "group headers in the Explorer. All five are always present, whether or not they "
    "currently hold anything — an empty category is visible as an empty group, which makes "
    "it obvious where a file would go."
)
PO += table(
    ["Category", "Holds", "Limit"],
    [
        ("<strong>Main</strong>", "The module with your program's entry point — the file "
         "the build commands compile.", "Exactly one file"),
        ("<strong>Resource</strong>", "The resource script (<code>.rc</code>).",
         "Exactly one file"),
        ("<strong>Header</strong>", "Your <code>.bi</code> include files.", "Any number"),
        ("<strong>Module</strong>", "The rest of your <code>.bas</code> source files.",
         "Any number"),
        ("<strong>Normal</strong>", "Anything else you want kept with the project — "
         "notes, data files, build scripts.", "Any number"),
    ],
    key_first=True,
)
PO += important(
    "<strong>Main and Resource hold exactly one file each.</strong> That is what makes "
    "them meaningful — they name <em>the</em> entry point and <em>the</em> resource "
    "script. Setting a new one displaces the old, which moves to <strong>Normal</strong> "
    "rather than being lost."
)
PO += p(
    "Tiko Editor assigns a category from the file's extension when you add it. The first "
    "<code>.bas</code> file to enter a workspace with no main module becomes the main "
    "module; later ones become ordinary modules. To change a file's category afterwards, "
    "right-click it in the Explorer or on its tab and pick the type, or drag it to another "
    'group — see <a href="project-files.html">Adding and removing files</a>.'
)
PO += note(
    "The category captions are stored in the project, so you can rename them to suit a "
    "particular project's vocabulary. The five categories themselves are permanent and "
    "cannot be deleted or added to."
)

PO += h2("Folders inside a category")
PO += p(
    "The three unlimited categories — Header, Module and Normal — can be given "
    "<strong>folders</strong> to organise a project with many files. Main and Resource "
    "cannot: a folder holding one file would have nothing useful in it."
)
PO += table(
    ["Action", "How"],
    [
        ("Create a folder", "Use the add icon on a category header, or its context menu."),
        ("Rename a folder", "Click its rename icon, press " + kbd("F2") + ", or use the "
         "context menu. The name is edited in place."),
        ("Delete a folder", "Use its delete icon. See the note below about what happens "
         "to its contents."),
        ("Move files or folders", "Drag them within or between categories."),
    ],
    key_first=True,
)
PO += important(
    "<strong>Deleting a folder dissolves it — it never destroys anything.</strong> The "
    "files and any sub-folders inside it move up one level to the deleted folder's parent. "
    "That is why no confirmation is asked for: nothing can be lost."
)
PO += note(
    "These folders are Tiko Editor's own organisation, not folders on disk. A file's folder is "
    "remembered per category, so moving a file to a different category clears its folder — "
    "a folder under Module is not the same place as a folder under Normal."
)

PO += h2("Related topics")
PO += ul([
    '<a href="project-explorer.html">Project Explorer</a>',
    '<a href="project-files.html">Adding and removing files</a>',
    '<a href="project-options.html">Project options</a>',
    '<a href="building.html">Building programs</a>',
])

page("projects-overview", "Projects and workspaces", "projects",
     "Tiko Editor's single workspace model: why every set of open files is already a project, how "
     "to name one, and what a project contains.",
     PO,
     keywords="project workspace untitled named default.tiko save as project new project "
              "open project close project recent projects categories main module")

# ==========================================================================
# Explorer
# ==========================================================================

PE = ""
PE += p(
    "The Explorer is the side panel view that shows your workspace as a tree. Open it with "
    "%s or the Explorer button on the icon strip." % kbd("Ctrl", "F4")
)

PE += h2("Reading the tree")
PE += p(
    "Files are grouped under the five permanent category headers — <strong>Main</strong>, "
    "<strong>Resource</strong>, <strong>Header</strong>, <strong>Module</strong> and "
    "<strong>Normal</strong> — described in "
    '<a href="projects-overview.html">Projects and workspaces</a>. Every header is always '
    "shown, so an empty category is visible as an empty group rather than absent."
)
PE += p(
    "Header, Module and Normal can additionally contain folders you create yourself, so a "
    "large project need not be one flat list."
)
PE += p(
    "While the workspace is untitled, a pinned <strong>Save as Project…</strong> row sits "
    "at the top of the tree. It disappears once the project has a name."
)
PE += figure_img(
    "assets/img/explorer-tree.png",
    "The Explorer. Files sit under the five permanent category headers, and the pinned "
    "<strong>Save as Project…</strong> row appears while the workspace is still untitled.",
    alt="The Tiko Editor Project Explorer tree")

PE += h2("Working in the Explorer")
PE += table(
    ["Action", "How"],
    [
        ("Open a file", "Click it. If it is already open, its tab comes forward."),
        ("Expand or collapse a group", "Click the twisty beside it."),
        ("Expand or collapse everything", "Use the expand-all and collapse-all commands."),
        ("Move a file to another category or folder",
         "Drag it there, or right-click it and choose the file type."),
        ("Create, rename or delete a folder",
         "Use the icons on the row, its context menu, or " + kbd("F2") + " to rename."),
        ("File commands", "Right-click for the context menu."),
        ("Move the panel to the other side", menu("View", "Move Explorer Window Left/Right")),
        ("Hide the panel", kbd("Ctrl", "B")),
    ],
    key_first=True,
)
PE += note(
    "<strong>In a named project, closing a file's tab does not remove it.</strong> The tab "
    "and project membership are separate things — closing the tab just stops displaying "
    "the file. It is still listed in the Explorer, still searched by Find in Project, and "
    "still compiled. Use <strong>Remove from project</strong> to take it out. (On an "
    'untitled workspace, closing <em>is</em> removing — see <a href="project-files.html">'
    "Adding and removing files</a>.)"
)

PE += h2("Related topics")
PE += ul([
    '<a href="project-files.html">Adding and removing files</a>',
    '<a href="side-panels.html">Side panels</a>',
    '<a href="projects-overview.html">Projects and workspaces</a>',
])

page("project-explorer", "Project Explorer", "projects",
     "The workspace tree: reading its categories, opening files from it, and the commands "
     "available on its context menu.",
     PE,
     keywords="explorer project explorer tree categories side panel open file context "
              "menu expand collapse")

# ==========================================================================
# Adding / removing files
# ==========================================================================

PF = ""
PF += h2("Adding files")
PF += table(
    ["Route", "Use when"],
    [
        (menu("Project", "Add Files to Project…") + " (" + kbd("Ctrl", "F11") + ")",
         "Adding existing files. You can select several at once."),
        (menu("File", "New") + " (" + kbd("Ctrl", "N") + ")",
         "Creating a new file. It joins the workspace when you save it."),
        (menu("File", "Open…") + " (" + kbd("Ctrl", "O") + ")",
         "Opening an existing file. It joins the current workspace."),
    ],
)
PF += p(
    "Tiko Editor categorises each added file by its extension. A <code>.bas</code> file entering "
    "a workspace that has no main module yet becomes the main module."
)

PF += h2("Removing files")
PF += p(
    "Removing a file takes it out of the project's file list. It never deletes anything "
    "from disk."
)
PF += table(
    ["The file is", "Do this"],
    [
        ("Listed in the Explorer",
         "Right-click it and choose <strong>Remove from project</strong>."),
        ("Open in a tab",
         "Right-click the <em>tab</em> and choose <strong>Remove from project</strong>."),
    ],
    key_first=True,
)
PF += p(
    "Both routes do the same thing. If the file is open, Tiko Editor closes its tab first — so "
    "you get the usual prompt if it has unsaved changes. <strong>Cancelling that prompt "
    "abandons the removal</strong> and the file stays in the project."
)
PF += important(
    "<strong>Remove from project only appears while the project has a name.</strong> An "
    "untitled workspace has no project for a file to be removed <em>from</em>, so the "
    "command is withheld from both context menus — see below for what to do instead."
)

PF += h3("On an untitled workspace, closing is removing")
PF += p(
    "The general rule is that closing a tab leaves the file in the workspace. There are "
    "exactly two exceptions, and both exist so a file can always be got rid of somehow:"
)
PF += table(
    ["Case", "Closing the tab…"],
    [
        ("An <strong>untitled</strong> workspace",
         "…also removes the file from the Explorer. With no <strong>Remove from "
         "project</strong> command available, closing has to be what takes the row out, or "
         "the file could never leave."),
        ("A <strong>never-saved</strong> document",
         "…ends it for good. There is no file on disk to go back to, so keeping a row "
         "pointing at nothing would be meaningless."),
    ],
    key_first=True,
)
PF += tip(
    'Name the workspace — see <a href="projects-overview.html">Projects and '
    "workspaces</a> — and you get explicit control over membership: closing a tab then "
    "only hides a file, and removal becomes a deliberate act."
)
PF += note(
    "Removing a file refreshes both the Explorer and the Functions panel, since the "
    "function list is built from the workspace's files."
)

PF += h2("File commands")
PF += table(
    ["Command", "Menu", "What it does"],
    [
        ("Rename…", menu("File", "Rename…"),
         "Renames the file on disk and updates the project to match."),
        ("Duplicate", menu("File", "Duplicate"),
         "Makes a copy of the file and adds it to the project."),
        ("Open File As Template", menu("File", "Open File As Template…"),
         "Opens a file's contents as a new untitled document, leaving the original "
         "untouched — a starting point for a new file."),
        ("Insert File", kbd("Ctrl", "I"),
         "Inserts another file's contents into the current document at the caret."),
    ],
    key_first=True,
)
PF += tip(
    "<strong>Open File As Template</strong> is the clean way to start from boilerplate. "
    "Keep a skeleton <code>.bas</code> somewhere, open it as a template, and save the "
    "result under the new name — the skeleton itself is never at risk."
)

PF += h2("Setting a file's type")
PF += p(
    "A file's <strong>type</strong> is what decides which Explorer group it sits in, which "
    "file the build commands treat as the entry point, and which one is the resource "
    "script. You can change it at any time, by menu or by dragging."
)

PF += h3("From the context menu")
PF += ol([
    "Right-click the file — either its row in the <strong>Explorer</strong> or its "
    "<strong>tab</strong>, whichever is to hand.",
    "Choose the type from the popup menu.",
], steps=True)
PF += p("Five types are offered, with a mark beside the file's current one:")
PF += table(
    ["Type", "Explorer group", "How many"],
    [
        ("Main file", "Main", "One per project"),
        ("Header file", "Header", "Any number"),
        ("Module file", "Module", "Any number"),
        ("Resource file", "Resource", "One per project"),
        ("Normal file", "Normal", "Any number"),
    ],
    key_first=True,
)
PF += tip(
    "Select several files in the Explorer before right-clicking and the type applies to "
    "all of them at once. With more than one selected no type is marked as current, since "
    "there is no single answer to mark."
)

PF += h3("By dragging")
PF += p(
    "Drag a file in the Explorer from one group to another — or into a folder within a "
    "group. This does exactly what the menu does, and is quicker when the Explorer is "
    "already open."
)

PF += h3("What happens to the previous Main or Resource file")
PF += important(
    "<strong>Main and Resource hold one file each, so setting a new one displaces the "
    "old.</strong> The file that was Main (or Resource) is moved to <strong>Normal</strong> "
    "— it stays in the project, and nothing is deleted. If you want it back as a module, "
    "set its type again afterwards."
)
PF += note(
    "That is worth knowing before you promote a file: the previous main module does not "
    "return to Module, it lands in Normal."
)

PF += h3("Jumping to them")
PF += p(
    "Once set, %s goes to the main module and %s to the resource script from anywhere in "
    "the project." % (kbd("Ctrl", "Shift", "M"), kbd("Ctrl", "Shift", "R"))
)

PF += h2("Related topics")
PF += ul([
    '<a href="project-explorer.html">Project Explorer</a>',
    '<a href="project-options.html">Project options</a>',
    '<a href="building.html">Building programs</a>',
])

page("project-files", "Adding and removing files", "projects",
     "Getting files into and out of a project, renaming and duplicating them, templates, "
     "and designating the main module.",
     PF,
     keywords="add files remove file project files rename duplicate template insert file "
              "main module resource file category")

# ==========================================================================
# Project options
# ==========================================================================

PP = ""
PP += p(
    "Project options are settings that belong to <em>this project</em>, stored in its "
    "<code>.tiko</code> file rather than in your editor settings. Open them with "
    "%s." % menu("Project", "Project Options…")
)

PP += h2("The pages")
PP += h3("Project")
PP += p(
    "Identifies the project and holds its command-line arguments — the parameters passed "
    "to your program when Tiko Editor runs it. Set them here rather than remembering to type them "
    "each time."
)
PP += h3("Compiler options")
PP += p(
    "Extra compiler switches for this project, added to whatever the active build "
    'configuration supplies. Use this for switches that are inherent to the project — an '
    "include path, a library it links against — rather than switches that vary between "
    'debug and release, which belong in a '
    '<a href="build-configurations.html">build configuration</a>.'
)
PP += h3("Build output")
PP += p(
    "Where the compiler should put the executable it produces, and related output settings."
)
PP += note(
    "The fields are labelled plainly in the dialog and behave as their names suggest, so "
    "they are not listed individually here."
)
PP += figure_img(
    "assets/img/project-options.png",
    "The Project Options dialog, holding the settings that belong to this project rather "
    "than to your editor installation.",
    alt="The Project Options dialog")

PP += h2("Project options versus build configurations versus editor settings")
PP += table(
    ["Setting lives in", "Scope", "Examples"],
    [
        ("Editor settings (<code>settings.ini</code>)", "Your whole installation",
         "Font, theme, indentation, autocomplete, compiler path."),
        ("Build configuration", "Reusable across projects",
         "Debug versus release switches, optimisation level, target architecture."),
        ("Project options (<code>.tiko</code>)", "This project only",
         "Include paths for this project, its libraries, its command-line arguments, its "
         "output folder."),
    ],
)
PP += tip(
    "If you find yourself setting the same option in every project, it probably belongs in "
    "a build configuration instead."
)

PP += h2("Related topics")
PP += ul([
    '<a href="build-configurations.html">Build configurations</a>',
    '<a href="compiler-setup.html">Compiler setup</a>',
    '<a href="configuration-reference.html">Configuration reference</a>',
])

page("project-options", "Project options", "projects",
     "Per-project settings: command-line arguments, extra compiler switches and build "
     "output — and how they differ from build configurations and editor settings.",
     PP,
     keywords="project options command line arguments compiler options build output "
              "per project settings tiko file")
