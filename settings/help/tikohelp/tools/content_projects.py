# -*- coding: utf-8 -*-
"""Projects section."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   placeholder, diagram)

section("projects", "Projects", "folder")

# ==========================================================================
# Overview / the workspace model
# ==========================================================================

PO = ""
PO += important(
    "<strong>In Tiko, every workspace is a project.</strong> There is no \"no project "
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
    "Tiko reopens your last workspace automatically when it starts.",
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
    "documents; Save Project saves the list of them. Tiko prompts for unsaved documents "
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
    "Files in a project are grouped into five categories, shown as headers in the "
    "Explorer. The headers are always present, whether or not they currently contain "
    "anything."
)
PO += table(
    ["Category", "Typically"],
    [
        ("Main", "The single module holding your program's entry point."),
        ("Modules", "The rest of your <code>.bas</code> source files."),
        ("Headers", "Your <code>.bi</code> include files."),
        ("Resource", "The resource script (<code>.rc</code>) and its dependencies."),
        ("Other", "Anything else you want to keep with the project — notes, data files, "
         "build scripts."),
    ],
    key_first=True,
)
PO += p(
    "Tiko assigns a category when you add a file, based on its extension. The first "
    "<code>.bas</code> file to enter a workspace that has no main module becomes the main "
    "module; later ones are added as ordinary modules. You can change any file's category "
    "afterwards."
)
PO += todo(
    "Confirm the exact category names shown in the Explorer and the precise commands for "
    "changing a file's category in the shipping build.",
    title="TODO — verify category names and the change-category command",
)

PO += h2("Related topics")
PO += ul([
    '<a href="project-explorer.html">Project Explorer</a>',
    '<a href="project-files.html">Adding and removing files</a>',
    '<a href="project-options.html">Project options</a>',
    '<a href="building.html">Building programs</a>',
])

page("projects-overview", "Projects and workspaces", "projects",
     "Tiko's single workspace model: why every set of open files is already a project, how "
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
    "Files are grouped under the five category headers described in "
    '<a href="projects-overview.html">Projects and workspaces</a>. Every header is always '
    "shown, so an empty category is visible as an empty group rather than absent — which "
    "makes it obvious where a file would go."
)
PE += p(
    "While the workspace is untitled, a pinned <strong>Save as Project…</strong> row sits "
    "at the top of the tree. It disappears once the project has a name."
)
PE += placeholder("Project Explorer", "Screenshot of the Explorer tree",
                  caption="Replace with a capture of the Explorer showing the category "
                          "headers, several files, and the pinned Save as Project row.")

PE += h2("Working in the Explorer")
PE += table(
    ["Action", "How"],
    [
        ("Open a file", "Click it. If it is already open, its tab comes forward."),
        ("Expand or collapse a group", "Click the twisty beside it."),
        ("Expand or collapse everything", "Use the expand-all and collapse-all commands."),
        ("File commands", "Right-click for the context menu."),
        ("Move the panel to the other side", menu("View", "Move Explorer Window Left/Right")),
        ("Hide the panel", kbd("Ctrl", "B")),
    ],
    key_first=True,
)
PE += note(
    "<strong>Closing a file's tab does not remove it from the project.</strong> The tab "
    "and the project membership are separate things — closing the tab just stops "
    "displaying the file. It is still listed in the Explorer, still searched by Find in "
    "Project, and still compiled."
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
    "Tiko categorises each added file by its extension. A <code>.bas</code> file entering "
    "a workspace that has no main module yet becomes the main module."
)

PF += h2("Removing files")
PF += p(
    "Removing a file from a project takes it out of the project's file list. It does "
    "<strong>not</strong> delete the file from disk."
)
PF += warn(
    "Do not confuse closing with removing. %s closes a tab and leaves the file in the "
    "project; removing it from the Explorer takes it out of the project but leaves the "
    "file on disk." % kbd("Ctrl", "W")
)
PF += todo(
    "Confirm the exact Explorer context-menu command used to remove a file from a project, "
    "and whether Tiko offers to delete the file from disk as well.",
    title="TODO — verify the remove-file command",
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

PF += h2("The main module and the resource file")
PF += p(
    "A project designates one file as its <strong>main module</strong> — the one with the "
    "program entry point, which the build commands compile. One file can also be marked as "
    "the <strong>resource script</strong>. Jump to either at any time with %s and %s."
    % (kbd("Ctrl", "Shift", "M"), kbd("Ctrl", "Shift", "R"))
)
PF += todo(
    "Document the exact procedure for changing which file is the main module or the "
    "resource file in the shipping build.",
    title="TODO — verify how to set the main module",
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
    "to your program when Tiko runs it. Set them here rather than remembering to type them "
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
PP += todo(
    "Confirm the exact fields on each page of the Project Options dialog in the shipping "
    "build and replace the summaries above with a field-by-field table.",
    title="TODO — enumerate Project Options fields",
)
PP += placeholder("Project Options", "Screenshot of the Project Options dialog",
                  caption="Replace with a capture showing the three sections and their "
                          "fields.")

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
