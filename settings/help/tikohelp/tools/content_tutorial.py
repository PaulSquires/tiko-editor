# -*- coding: utf-8 -*-
"""Tutorial section: overview plus ten progressive lessons."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui, lesson_meta,
                   placeholder, diagram)

section("tutorial", "Tutorial", "graduation")


def lesson(num, slug, title, goal, time_est, prereq, goals, body,
           mistakes, review, exercises, next_text, keywords):
    """Assemble one lesson page from its standard parts."""
    out = lesson_meta(goal, time_est, prereq)
    out += h2("What you will learn")
    out += ul(goals)
    out += body
    out += h2("Common mistakes")
    out += table(["Symptom", "Cause and fix"], mistakes)
    out += h2("Review")
    out += ul(review)
    out += h2("Practice")
    out += ol(exercises, steps=True)
    out += h2("What's next?")
    out += p(next_text)
    page(slug, "Lesson %d — %s" % (num, title), "tutorial",
         goal, out, keywords=keywords)


# ==========================================================================
# Tutorial overview
# ==========================================================================

TV = ""
TV += p(
    "This tutorial teaches Tiko from the beginning. Each lesson builds on the last, takes "
    "10–20 minutes, and ends with exercises that make the material stick. Work through it "
    "in order the first time."
)
TV += p(
    "You will build one small FreeBASIC program across the whole course, so by the end you "
    "will have used the editor the way you will use it every day: writing code, searching "
    "it, organising it into a project, building it, debugging it and tuning the editor to "
    "your taste."
)

TV += h2("Before you start")
TV += ul([
    "Tiko unpacked into a folder you can write to.",
    "A FreeBASIC compiler installed, if you want to do lessons 6 and 7. Lessons 1–5 and "
    "8–10 need no compiler at all.",
    "About two hours in total, or one lesson at a time.",
])
TV += note(
    "In a hurry? The <a href=\"quick-start.html\">Quick Start</a> covers the same ground in "
    "25 minutes without the exercises."
)

TV += h2("The lessons")
TV += cards([
    ("lesson-01.html", "rocket", "1 — Opening Tiko",
     "Install, launch, and learn the parts of the window. 10 minutes."),
    ("lesson-02.html", "edit", "2 — Your first file",
     "Create, type, save and reopen a file. 10 minutes."),
    ("lesson-03.html", "edit", "3 — Editing text",
     "Selections, clipboard, line operations and column mode. 20 minutes."),
    ("lesson-04.html", "find", "4 — Searching",
     "Find, Replace, Find in Project and regular expressions. 15 minutes."),
    ("lesson-05.html", "folder", "5 — Projects",
     "Turn loose files into a named project. 15 minutes."),
    ("lesson-06.html", "build", "6 — Building a program",
     "Configure the compiler, build, run and fix errors. 20 minutes."),
    ("lesson-07.html", "bug", "7 — Debugging",
     "Breakpoints, stepping, watches and the call stack. 20 minutes."),
    ("lesson-08.html", "sliders", "8 — Customization",
     "Themes, fonts and keyboard shortcuts. 15 minutes."),
    ("lesson-09.html", "bolt", "9 — Productivity shortcuts",
     "Navigation, autocomplete and the formatter. 15 minutes."),
    ("lesson-10.html", "graduation", "10 — Advanced editing",
     "Multiple selections, folding, split views and user tools. 20 minutes."),
])

TV += h2("Conventions used")
TV += table(
    ["Style", "Means"],
    [
        (kbd("Ctrl", "S"), "Press these keys together."),
        (menu("File", "Save"), "A menu path: open File, then choose Save."),
        ("<code>Print \"hi\"</code>", "Text to type, or code to read."),
        ("<strong>OK</strong>", "A button or control to click."),
    ],
    key_first=True,
)

page("tutorial", "Tutorial", "tutorial",
     "A ten-lesson course that takes you from installing Tiko to using its advanced "
     "editing features, with exercises throughout.",
     TV,
     keywords="tutorial lessons course learn beginner training walkthrough")

# ==========================================================================
# Lesson 1
# ==========================================================================

L1 = ""
L1 += h2("Launch Tiko")
L1 += ol([
    "Unpack the distribution into a folder you can write to — <code>C:\\tiko</code>, for "
    "example. Not <code>C:\\Program Files</code>: Tiko keeps its settings beside the "
    "executable and that folder is protected.",
    "Double-click <code>tiko.exe</code>.",
    "The main window opens on an empty untitled workspace.",
], steps=True)
L1 += note(
    "There is no installer and nothing is written to the registry. Removing Tiko later is "
    "deleting the folder."
)

L1 += h2("Find the seven regions")
L1 += p("Locate each of these before going on. You will use all of them.")
L1 += table(
    ["Region", "Where", "Try it"],
    [
        ("Menu bar", "Across the top.", "Open the File menu, then press " + kbd("Esc") + "."),
        ("Panel icon strip", "The narrow column of buttons at the side.",
         "Hover a button to see its tooltip."),
        ("Side panel", "Beside the icon strip.", "Press " + kbd("Ctrl", "B") +
         " twice to hide and show it."),
        ("Document tabs", "Above the editor.", "Empty until you open a file."),
        ("Editor", "The centre.", "Click in it and type something."),
        ("Output panel", "Below the editor.", "Press " + kbd("Ctrl", "F9") + " twice."),
        ("Status bar", "Along the bottom.", "Watch the line and column change as you "
         "move the caret."),
    ],
)

L1 += h2("Switch the side panel views")
L1 += p("The side panel shows one of three views. Try each:")
L1 += ul([
    "%s — the Explorer, showing your workspace files." % kbd("Ctrl", "F4"),
    "%s — the function list for the current file." % kbd("F4"),
    "%s — your bookmarks." % kbd("Shift", "F4"),
])
L1 += p("They are all empty for now. They fill up as you work.")

L1 += h2("Resize things")
L1 += ol([
    "Point at the boundary between the side panel and the editor. The cursor becomes a "
    "resize arrow.",
    "Drag it to make the panel wider, then narrower.",
    "Do the same with the boundary above the Output panel.",
    "Choose %s to put everything back." % menu("View", "Restore Main Window Size"),
], steps=True)

L1 += h2("Expected result")
L1 += p(
    "Tiko is running, you can name every region of the window, and you can show, hide and "
    "resize the panels."
)

lesson(
    1, "lesson-01", "Opening Tiko",
    "Install and launch Tiko, and learn the parts of its window.",
    "10 minutes", "Nothing — this is the starting point.",
    ["Where to unpack Tiko, and why the location matters.",
     "The seven regions of the main window.",
     "How to show, hide and resize the panels.",
     "How to switch between the three side panel views."],
    L1,
    [("Tiko starts but has no colours and menus look wrong.",
      "You ran a copy of <code>tiko.exe</code> on its own. Run it from the folder you "
      "unpacked, where its <code>settings</code> folder is."),
     ("Settings do not stick between runs.",
      "Tiko is installed somewhere it cannot write. Move the folder out of "
      "<code>C:\\Program Files</code>."),
     ("The side panel will not reappear.",
      "Press " + kbd("Ctrl", "B") + ", or choose " + menu("View", "View Side Panel") + ".")],
    ["Tiko is portable: unpack, run, delete.",
     kbd("Ctrl", "B") + " toggles the side panel, " + kbd("Ctrl", "F9") +
     " the Output panel.",
     "The side panel hosts three views: Explorer, Functions and Bookmarks.",
     "Panel sizes are remembered between sessions."],
    ["Hide both panels and confirm the editor fills the window.",
     "Move the Explorer to the other side with " +
     menu("View", "Move Explorer Window Left/Right") + ", then move it back.",
     "Open every menu in turn and read the commands. You do not need to remember them — "
     "just get a sense of what lives where."],
    'In <a href="lesson-02.html">Lesson 2</a> you create, save and reopen your first file.',
    "tutorial lesson 1 install launch window regions panels")

# ==========================================================================
# Lesson 2
# ==========================================================================

L2 = ""
L2 += h2("Create a file")
L2 += ol([
    "Press %s. A tab appears, named <em>Untitled</em>." % kbd("Ctrl", "N"),
    "Type the program below. Watch the keywords colour themselves as you type.",
], steps=True)
L2 += code("""
' greet.bas - Lesson 2
Dim As String name

Print "What is your name?"
Input name
Print "Hello, "; name; "!"

Sleep
""", lang="fb", title="greet.bas")
L2 += tip(
    "Type it rather than pasting it. You will see auto-indentation and syntax "
    "highlighting working, which is the point of the exercise."
)

L2 += h2("Save it")
L2 += ol([
    "Press %s. Because the file has never been saved, the Save As dialog opens."
    % kbd("Ctrl", "S"),
    "Create a folder for this tutorial — <code>C:\\tiko-tutorial</code> is fine.",
    "Name the file <code>greet.bas</code> and save.",
    "The tab caption changes from <em>Untitled</em> to <code>greet.bas</code>.",
], steps=True)
L2 += p(
    "Now change something — add a comment at the end — and press %s again. It saves "
    "straight away with no dialog, because the file now has a name." % kbd("Ctrl", "S")
)

L2 += h2("Save As, and what it really does")
L2 += ol([
    "Choose %s." % menu("File", "Save As…"),
    "Save it as <code>greet2.bas</code>.",
    "Look at the tab: it now reads <code>greet2.bas</code>.",
], steps=True)
L2 += important(
    "After Save As you are editing the <strong>new</strong> file. The original is closed "
    "and left as it was at its last save. This surprises people who expect Save As to make "
    "a backup copy and keep working on the original — it does the opposite."
)

L2 += h2("Close and reopen")
L2 += ol([
    "Press %s to close the document." % kbd("Ctrl", "W"),
    "Open %s. Both files are listed." % menu("File", "Open Recent"),
    "Choose <code>greet.bas</code>. It opens where you left it.",
], steps=True)
L2 += note(
    "Tiko also reopens whatever you had open when you last closed it — even though you "
    "have not created a project. Those loose files already <em>are</em> a project, an "
    'untitled one. Lesson 5 covers this properly.'
)

L2 += h2("Expected result")
L2 += p(
    "Two files in your tutorial folder, one of them open, and both listed under Open "
    "Recent."
)

lesson(
    2, "lesson-02", "Creating your first file",
    "Create, type, save and reopen a file, and understand what Save As does.",
    "10 minutes", "Lesson 1.",
    ["Creating a document and saving it for the first time.",
     "The difference between Save and Save As.",
     "Closing a document and reopening it from Open Recent.",
     "Why your files come back when you restart Tiko."],
    L2,
    [("Pressing " + kbd("Ctrl", "S") + " opens a dialog every time.",
      "You are pressing " + kbd("Ctrl", "Shift", "S") + " (Save All), or the document "
      "genuinely has not been saved yet."),
     ("The original file did not change after Save As.",
      "That is correct behaviour. Save As switches you to editing the new file."),
     ("The tab shows a modified marker that will not clear.",
      "The file has unsaved changes. Press " + kbd("Ctrl", "S") + "."),
     ("Open Recent is empty.",
      "Nothing has been opened yet in this installation — the list fills as you work.")],
    [kbd("Ctrl", "S") + " saves; " + kbd("Ctrl", "Shift", "S") + " saves everything.",
     "Save As moves you to the new file.",
     kbd("Ctrl", "W") + " closes a document; " + kbd("Ctrl", "Shift", "W") +
     " closes them all.",
     "Open Recent lists files; Recent Projects lists whole workspaces."],
    ["Create a third file, <code>notes.txt</code>, and type a few lines. Notice that it "
     "gets no syntax colouring — Tiko chose the language from the extension.",
     "Close every document with " + kbd("Ctrl", "Shift", "W") + ", then reopen "
     "<code>greet.bas</code> from Open Recent.",
     "Restart Tiko and confirm your open files come back."],
    'In <a href="lesson-03.html">Lesson 3</a> you learn to edit efficiently — selections, '
    "line operations and column editing.",
    "tutorial lesson 2 new file save save as open recent close")

# ==========================================================================
# Lesson 3
# ==========================================================================

L3 = ""
L3 += p("Open <code>greet.bas</code> and replace its contents with this:")
L3 += code("""
' inventory.bas - Lesson 3
Dim As String item1, item2, item3
Dim As Integer qty1, qty2, qty3

item1 = "bolt"
item2 = "nut"
item3 = "washer"

qty1 = 100
qty2 = 250
qty3 = 75

Print item1, qty1
Print item2, qty2
Print item3, qty3

Sleep
""", lang="fb", title="inventory.bas")
L3 += p("Save it as <code>inventory.bas</code>.")

L3 += h2("Selections")
L3 += ol([
    "<strong>Double-click</strong> <code>washer</code>. The word is selected.",
    "<strong>Triple-click</strong> the same line. The whole line is selected.",
    "Click at the start of the first <code>Print</code> line, then hold %s and press %s "
    "twice. Three lines are selected." % (kbd("Shift"), kbd("↓")),
    "Press %s to select everything, then click once to clear it." % kbd("Ctrl", "A"),
], steps=True)

L3 += h2("Line operations")
L3 += p("These work with no selection at all, which is what makes them fast.")
L3 += ol([
    "Put the caret anywhere on the <code>qty2 = 250</code> line.",
    "Press %s. A copy appears below." % kbd("Ctrl", "D"),
    "Press %s. The duplicate is removed again." % kbd("Ctrl", "Y"),
    "Press %s twice. The line moves up past its neighbours." % kbd("Alt", "↑"),
    "Press %s twice to put it back." % kbd("Alt", "↓"),
    "Press %s to select the whole line, then %s to copy it."
    % (kbd("Ctrl", "L"), kbd("Ctrl", "C")),
], steps=True)
L3 += tip(
    "With nothing selected, %s and %s act on the whole line. To move a line, press %s, "
    "click where you want it, and press %s."
    % (kbd("Ctrl", "X"), kbd("Ctrl", "C"), kbd("Ctrl", "X"), kbd("Ctrl", "V"))
)

L3 += h2("Column editing")
L3 += p("This is the technique worth practising until it is automatic.")
L3 += ol([
    "Hold %s and drag straight down the left edge of the three <code>Print</code> lines, "
    "without moving right. You now have three carets stacked vertically." % kbd("Alt"),
    "Type <code>' </code> — an apostrophe and a space. All three lines are commented at "
    "once.",
    "Press %s to undo it." % kbd("Ctrl", "Z"),
    "Now hold %s and drag across the <code>qty</code> in all three "
    "<code>qty1/2/3</code> declarations to select a rectangle." % kbd("Alt"),
    "Type <code>amount</code>. All three change together.",
    "Press %s to undo." % kbd("Ctrl", "Z"),
], steps=True)
L3 += note(
    "A zero-width column selection gives you a caret per line, which inserts. A column "
    "with width selects a rectangle, which replaces."
)

L3 += h2("Commenting")
L3 += ol([
    "Select the three <code>Print</code> lines.",
    "Press %s. All three are commented." % kbd("Ctrl", "/"),
    "Press %s. The comments are removed." % kbd("Ctrl", "Shift", "/"),
], steps=True)
L3 += p("This is easier than the column trick for commenting, and it knows the language's "
        "comment character. Use column mode for things that are not comments.")

L3 += h2("Expected result")
L3 += p(
    "<code>inventory.bas</code> is back to its original state, and you have used every "
    "editing technique above at least once."
)

lesson(
    3, "lesson-03", "Editing text",
    "Select, move, duplicate and comment code efficiently, including column editing.",
    "20 minutes", "Lessons 1–2.",
    ["Selecting by word, line and block.",
     "Duplicating, deleting and moving lines with no selection.",
     "Editing several lines at once with column mode.",
     "Commenting and uncommenting blocks."],
    L3,
    [("Dragging with " + kbd("Alt") + " selects normally.",
      "Press and hold " + kbd("Alt") + " <em>before</em> you press the mouse button."),
     ("Typing in a column selection replaced text I wanted to keep.",
      "Your selection had width. For inserting, drag straight down without moving "
      "sideways."),
     (kbd("Ctrl", "D") + " does nothing.",
      "The editor does not have focus. Click in it, or press " + kbd("Ctrl", "`") + "."),
     ("Comment block used the wrong comment character.",
      "The file's language is wrong for its extension. Check the language field in the "
      "status bar.")],
    [kbd("Ctrl", "D") + " duplicates, " + kbd("Ctrl", "Y") + " deletes, " +
     kbd("Alt", "↑") + "/" + kbd("Alt", "↓") + " moves a line.",
     "Cut and Copy act on the whole line when nothing is selected.",
     kbd("Alt") + "-drag gives a column selection; a zero-width one inserts on every line.",
     kbd("Ctrl", "/") + " and " + kbd("Ctrl", "Shift", "/") + " comment and uncomment."],
    ["Use column mode to change all three <code>item</code> variables to <code>part</code>.",
     "Reorder the three <code>Print</code> lines into reverse order using only " +
     kbd("Alt", "↑") + " and " + kbd("Alt", "↓") + ".",
     "Comment out the entire file, then uncomment it, using two keystrokes each way."],
    'In <a href="lesson-04.html">Lesson 4</a> you learn to find and replace text — in one '
    "file and across a whole project.",
    "tutorial lesson 3 editing selection clipboard duplicate move line column mode comment")

# ==========================================================================
# Lesson 4
# ==========================================================================

L4 = ""
L4 += p("Keep <code>inventory.bas</code> open for this lesson.")

L4 += h2("Find")
L4 += ol([
    "Press %s." % kbd("Ctrl", "F"),
    "Type <code>qty</code>. Matches are highlighted as you type.",
    "Press %s repeatedly to step through them." % kbd("Enter"),
    "Press %s to close the Find bar." % kbd("Esc"),
    "Now press %s. The search continues without the bar being open — and %s goes back."
    % (kbd("F3"), kbd("Shift", "F3")),
], steps=True)
L4 += important(
    "%s is the shortcut that saves the most time. Search once, then step through matches "
    "with one key while you edit." % kbd("F3")
)

L4 += h2("Whole word and case")
L4 += ol([
    "Search for <code>item1</code>. Three matches — the declaration, the assignment and "
    "the Print.",
    "Search for <code>item</code>. Many more, because it matches inside "
    "<code>item1</code>, <code>item2</code> and <code>item3</code>.",
    "Turn on <strong>Whole word</strong> and search for <code>item</code> again. No "
    "matches, because no variable is called exactly that.",
], steps=True)
L4 += p("That is the difference in a nutshell, and it is the setting most often responsible "
        "for a Replace All going wrong.")

L4 += h2("Replace")
L4 += ol([
    "Press %s." % kbd("Ctrl", "H"),
    "Find <code>qty</code>, replace with <code>quantity</code>.",
    "Press <strong>Replace</strong> once. One occurrence changes and the next is found.",
    "Press <strong>Replace All</strong>. Everything else changes at once.",
    "Press %s. The whole Replace All is undone in one step." % kbd("Ctrl", "Z"),
], steps=True)
L4 += tip(
    "Replace All is one undo away, but check <strong>Whole word</strong> first. Replacing "
    "<code>id</code> with <code>identifier</code> without it will happily rewrite the "
    "middle of every word containing <code>id</code>."
)

L4 += h2("Find in Project")
L4 += ol([
    "Press %s." % kbd("Ctrl", "Shift", "F"),
    "Search for <code>Print</code>.",
    "The Output panel opens on its Search results tab, listing every match in every file "
    "of the workspace.",
    "Click a result. That file opens with the caret on the line.",
], steps=True)
L4 += p(
    "With only two or three files this seems like overkill. On a real project it is how "
    "you answer \"where is this used?\" in a couple of seconds."
)

L4 += h2("A regular expression")
L4 += ol([
    "Open Find and turn on <strong>Regular expression</strong>.",
    "Search for <code>^Print</code> — the caret means <em>start of line</em>. Only the "
    "lines beginning with <code>Print</code> match.",
    "Now search for <code>\\d+</code> — one or more digits. Every number in the file "
    "matches.",
    "Turn the option off again when you have finished.",
], steps=True)
L4 += note(
    "Leaving <strong>Regular expression</strong> on catches people out later, when an "
    "ordinary search containing a full stop or a bracket behaves oddly."
)

L4 += h2("Expected result")
L4 += p(
    "The file is back to using <code>qty</code>, and you have used Find, Replace, Find in "
    "Project and a regular expression at least once each."
)

lesson(
    4, "lesson-04", "Searching",
    "Find and replace text in a file, search a whole project, and use a simple regular "
    "expression.",
    "15 minutes", "Lessons 1–3.",
    ["Finding text and repeating a search with " + kbd("F3") + ".",
     "What Whole word and Match case actually change.",
     "Replacing one match at a time, and all at once.",
     "Searching every file in the project.",
     "The idea behind regular expressions."],
    L4,
    [("Replace All changed far more than expected.",
      "<strong>Whole word</strong> was off. Press " + kbd("Ctrl", "Z") +
      " — it undoes the whole operation — and try again."),
     ("A search finds nothing that is clearly there.",
      "<strong>Match case</strong> or <strong>Regular expression</strong> is on from a "
      "previous search."),
     (kbd("F3") + " does nothing.",
      "No search has been made yet in this session. Open Find once first."),
     ("Find in Project returns nothing.",
      "The files are not in the workspace. Lesson 5 covers this.")],
    [kbd("Ctrl", "F") + " finds, " + kbd("F3") + " repeats, " + kbd("Ctrl", "H") +
     " replaces.",
     kbd("Ctrl", "Shift", "F") + " searches every file, with results in the Output panel.",
     "Whole word prevents matches inside longer words.",
     "Replace All is a single undoable action."],
    ["Use Replace to rename <code>item</code> to <code>part</code> throughout, then undo it.",
     "Use Find in Project to find every line containing <code>Dim</code>.",
     "Write a regular expression that finds every line ending in a digit. (Hint: "
     "<code>\\d$</code>.)"],
    'In <a href="lesson-05.html">Lesson 5</a> you turn these loose files into a real, named '
    "project.",
    "tutorial lesson 4 find replace search project regular expression whole word")

# ==========================================================================
# Lesson 5
# ==========================================================================

L5 = ""
L5 += important(
    "Before you start, understand the central idea: <strong>in Tiko you already have a "
    "project</strong>. The files you have been editing form an untitled one. This lesson "
    "gives it a name."
)

L5 += h2("Look at what you have")
L5 += ol([
    "Press %s to show the Explorer." % kbd("Ctrl", "F4"),
    "You will see the category headers — Main, Modules, Headers, Resource, Other — with "
    "your files under them.",
    "At the top there is a pinned <strong>Save as Project…</strong> row. That row is "
    "telling you the workspace has no name yet.",
], steps=True)

L5 += h2("Name the project")
L5 += ol([
    "Click <strong>Save as Project…</strong>, or choose %s."
    % menu("Project", "Save Project As…"),
    "Save it as <code>tutorial.tiko</code> in your tutorial folder.",
    "The pinned row disappears and the window caption now names the project.",
], steps=True)
L5 += note(
    "Nothing about your files changed. The same files are in the same workspace — it now "
    "has a name, appears in Recent Projects, and can be reopened deliberately."
)

L5 += h2("Add a file")
L5 += ol([
    "Press %s to create a new file." % kbd("Ctrl", "N"),
    "Type the code below and save it as <code>helpers.bi</code> in the same folder.",
], steps=True)
L5 += code("""
' helpers.bi - Lesson 5
#pragma once

Declare Function Describe( ByVal qty As Integer ) As String

Function Describe( ByVal qty As Integer ) As String
    If qty = 0 Then Return "out of stock"
    If qty < 100 Then Return "low"
    Return "plenty"
End Function
""", lang="fb", title="helpers.bi")
L5 += p(
    "Look at the Explorer. <code>helpers.bi</code> has been filed under Headers, because "
    "Tiko categorises by extension."
)

L5 += h2("Use it")
L5 += p("Edit <code>inventory.bas</code> so it includes and calls the helper:")
L5 += code("""
' inventory.bas - Lesson 5
#include once "helpers.bi"

Dim As String item1, item2, item3
Dim As Integer qty1, qty2, qty3

item1 = "bolt"  : qty1 = 100
item2 = "nut"   : qty2 = 250
item3 = "washer": qty3 = 75

Print item1, qty1, Describe( qty1 )
Print item2, qty2, Describe( qty2 )
Print item3, qty3, Describe( qty3 )

Sleep
""", lang="fb", title="inventory.bas")

L5 += h2("Closing a tab is not removing a file")
L5 += ol([
    "Close <code>helpers.bi</code> with %s." % kbd("Ctrl", "W"),
    "Look at the Explorer. The file is still listed.",
    "Click it. It opens again.",
], steps=True)
L5 += important(
    "A tab is a view of a file. Closing the tab stops displaying the file; it does not "
    "take it out of the project. It is still compiled, still searched by Find in Project, "
    "and still listed."
)

L5 += h2("Reopen the project")
L5 += ol([
    "Choose %s." % menu("Project", "Close Project"),
    "Everything closes and you are given a fresh untitled workspace — because there is "
    "always a workspace.",
    "Choose %s and pick <code>tutorial</code>." % menu("Project", "Recent Projects"),
    "Your files come back.",
], steps=True)

L5 += h2("Expected result")
L5 += p(
    "A named project, <code>tutorial.tiko</code>, containing <code>inventory.bas</code> "
    "and <code>helpers.bi</code>, which you can close and reopen at will."
)

lesson(
    5, "lesson-05", "Projects",
    "Turn the files you already have into a named project, add files to it, and reopen it.",
    "15 minutes", "Lessons 1–4.",
    ["Why every workspace is already a project.",
     "Naming a workspace with Save as Project.",
     "How files are categorised in the Explorer.",
     "Why closing a tab does not remove a file from the project."],
    L5,
    [("The Save as Project row is not there.",
      "The workspace already has a name. Check the window caption."),
     ("A new file went into the wrong category.",
      "Categories come from the file extension. Change the category from the Explorer's "
      "context menu."),
     ("Closing a tab seemed to lose a file.",
      "It is still in the Explorer. Click it to reopen."),
     ("Find in Project misses a file.",
      "That file is not in the workspace. Add it with " +
      menu("Project", "Add Files to Project…") + ".")],
    ["Every set of open files is a project; naming it is a save.",
     "The untitled workspace is restored automatically at start-up.",
     "Files are categorised by extension into Main, Modules, Headers, Resource and Other.",
     "Tabs and project membership are separate things."],
    ["Add <code>notes.txt</code> from Lesson 2 to the project and see which category it "
     "lands in.",
     "Close every tab, then reopen both source files from the Explorer.",
     "Close the project, create two throwaway files in the new untitled workspace, then "
     "reopen <code>tutorial</code> and confirm your work is intact."],
    'In <a href="lesson-06.html">Lesson 6</a> you compile and run the program. You need a '
    "FreeBASIC compiler installed for this one.",
    "tutorial lesson 5 project workspace save as project explorer categories add files")

# ==========================================================================
# Lesson 6
# ==========================================================================

L6 = ""
L6 += note(
    "This lesson needs a FreeBASIC compiler installed. If you do not have one, skip ahead "
    'to <a href="lesson-08.html">Lesson 8</a> and come back later.'
)

L6 += h2("Point Tiko at the compiler")
L6 += ol([
    "Open %s." % menu("File", "Settings", "Options…"),
    "Select the <strong>Compiler</strong> page.",
    "Browse to your FreeBASIC installation and select the compiler executable.",
    "Choose <strong>OK</strong>.",
], steps=True)
L6 += p("This is a one-off. Tiko remembers it for every project.")

L6 += h2("Build and run")
L6 += ol([
    "Make <code>inventory.bas</code> the active document.",
    "Press %s." % kbd("F5"),
    "The Output panel opens on its Compiler tab and shows the build.",
    "Your program runs in a console window. Type nothing — it prints and waits for a key.",
    "Press a key to close it.",
], steps=True)
L6 += p(
    "That single keystroke saved your files, compiled the project and ran the result."
)

L6 += h2("Break it deliberately")
L6 += p("Errors are more instructive than successes, so introduce one:")
L6 += ol([
    "In <code>inventory.bas</code>, change <code>Describe( qty1 )</code> to "
    "<code>Descrbe( qty1 )</code>.",
    "Press %s." % kbd("F5"),
    "The build fails, and the Output panel lists the error.",
    "<strong>Click the error row.</strong> Tiko jumps to the exact line.",
    "Fix the typo and press %s again. It builds." % kbd("F5"),
], steps=True)
L6 += important(
    "Always fix the <strong>first</strong> error and rebuild. One early mistake commonly "
    "produces a cascade of later errors that disappear on their own once it is fixed."
)

L6 += h2("The other build commands")
L6 += ol([
    "Press %s — Compile. It builds but does not run." % kbd("Ctrl", "F5"),
    "Press %s — Run Executable. It runs the build you just made, without recompiling."
    % kbd("Shift", "F5"),
    "Press %s — Quick Run. It compiles and runs the current file on its own, ignoring the "
    "project." % kbd("Ctrl", "Shift", "F5"),
], steps=True)
L6 += tip(
    "Quick Run makes Tiko a good scratchpad. Open a new file, type ten lines to test a "
    "language feature, and press %s — no project needed." % kbd("Ctrl", "Shift", "F5")
)

L6 += h2("See the command line")
L6 += p(
    "Choose %s to see exactly what Tiko passes to the compiler. When a build does "
    "something unexpected, this usually explains it in one glance."
    % menu("Compile", "Command Line…")
)

L6 += h2("Expected result")
L6 += p(
    "The program compiles and runs, printing three lines with a description beside each "
    "quantity, and you have navigated to a compiler error by clicking it."
)

lesson(
    6, "lesson-06", "Building a program",
    "Configure the compiler, build and run your project, and navigate compiler errors.",
    "20 minutes", "Lesson 5, plus a FreeBASIC compiler.",
    ["Setting the compiler path.",
     "Building and running with one keystroke.",
     "Reading build output and jumping to an error.",
     "When to use Compile, Run Executable and Quick Run."],
    L6,
    [("The build fails saying the compiler was not found.",
      "The compiler path is wrong. Go back to " + menu("File", "Settings", "Options…") +
      " and check it — this is a configuration error, not a code error."),
     ("The program window flashes and vanishes.",
      "Your program ended without waiting. Add <code>Sleep</code> at the end."),
     ("Errors point at lines that look fine.",
      "Fix the first error and rebuild; the rest often go with it. An unterminated block "
      "is reported far below where it was opened."),
     ("The build succeeds but runs old behaviour.",
      "A file was not saved. Check that <strong>Compile autosave</strong> is on.")],
    [kbd("F5") + " builds and runs; " + kbd("Ctrl", "F5") + " builds only.",
     "Clicking an error jumps to its line.",
     "Quick Run builds the current file alone, ignoring the project.",
     menu("Compile", "Command Line…") + " shows exactly what is passed to the compiler."],
    ["Introduce three different errors at once and practise fixing them from the top down.",
     "Remove the <code>#include</code> line and read the error you get.",
     "Use Quick Run on a brand-new scratch file that prints the numbers 1 to 10."],
    'In <a href="lesson-07.html">Lesson 7</a> you run the same program under the debugger '
    "and watch it work.",
    "tutorial lesson 6 build compile run errors compiler setup quick run f5")

# ==========================================================================
# Lesson 7
# ==========================================================================

L7 = ""
L7 += p(
    "Debugging is how you find out what your program is <em>actually</em> doing, rather "
    "than what you believe it is doing. Give it something worth watching first."
)
L7 += p("Replace <code>inventory.bas</code> with this loop version:")
L7 += code("""
' inventory.bas - Lesson 7
#include once "helpers.bi"

Dim As String items(1 To 3) = { "bolt", "nut", "washer" }
Dim As Integer qty(1 To 3) = { 100, 250, 75 }
Dim As Integer total = 0
Dim As Integer i

For i = 1 To 3
    total += qty(i)
    Print items(i), qty(i), Describe( qty(i) )
Next i

Print "Total:", total
Sleep
""", lang="fb", title="inventory.bas")

L7 += h2("Set a breakpoint and start")
L7 += ol([
    "Put the caret on the <code>total += qty(i)</code> line.",
    "Press %s. A marker appears in the margin." % kbd("F9"),
    "Press %s to start debugging." % kbd("F6"),
    "The program builds, starts and stops on your breakpoint. The current line is marked.",
], steps=True)
L7 += note(
    "If the breakpoint jumps to the next line when you set it, that is deliberate: the "
    "line you chose generates no executable code, so Tiko moved it somewhere it can "
    "actually stop."
)

L7 += h2("Look at the state")
L7 += p("With the program stopped, four panes tell you where you are:")
L7 += ol([
    "<strong>Locals</strong> shows <code>i</code>, <code>total</code> and the arrays. "
    "<code>i</code> should be 1 and <code>total</code> 0.",
    "<strong>Call stack</strong> shows the chain of calls that reached this line.",
    "<strong>Hover</strong> over <code>qty</code> in the editor to see a data tip.",
    "Expand the <code>qty</code> array with its twisty to see all three elements.",
], steps=True)

L7 += h2("Step, and watch it change")
L7 += ol([
    "Press %s (Step Over). The line executes and <code>total</code> becomes 100 — shown "
    "in a different colour, because it changed." % kbd("F10"),
    "Press %s a few more times to go round the loop. Watch <code>i</code> and "
    "<code>total</code> change on each pass." % kbd("F10"),
    "When you reach the <code>Describe(...)</code> call, press %s (Step) instead. You are "
    "now inside <code>Describe</code>, in <code>helpers.bi</code>." % kbd("F11"),
    "Look at the call stack: <code>Describe</code> sits above the main program.",
    "Press %s (Step Out) to return to the caller." % kbd("Shift", "F11"),
], steps=True)
L7 += important(
    "That is the whole distinction. %s goes <em>into</em> a call; %s runs it and stops "
    "after it. Use %s by default." % (kbd("F11"), kbd("F10"), kbd("F10"))
)

L7 += h2("Add a watch")
L7 += ol([
    "In the Watch pane, click <strong>&lt;click to add a watch&gt;</strong>.",
    "Type <code>total</code> and press %s." % kbd("Enter"),
    "Add a second watch: <code>qty(i)</code>.",
    "Step again. Both update on every stop, without you hunting for them.",
], steps=True)
L7 += tip(
    "Watch an <em>expression</em>, not just a variable. <code>qty(i)</code> tells you what "
    "the loop is looking at right now — far more useful inside a loop than "
    "<code>qty</code> and <code>i</code> separately."
)

L7 += h2("Finish the session")
L7 += ol([
    "Press %s to continue. The program runs to the next breakpoint hit." % kbd("F6"),
    "Press %s (Toggle Breakpoint) on the marked line to remove it." % kbd("F9"),
    "Press %s to continue. With no breakpoints left, the program runs to completion."
    % kbd("F6"),
    "Press %s if you ever need to end a session early." % kbd("Shift", "F6"),
], steps=True)

L7 += h2("Expected result")
L7 += p(
    "You have stopped the program mid-loop, watched variables change as you stepped, gone "
    "into a procedure and back out, and read the call stack."
)

lesson(
    7, "lesson-07", "Debugging",
    "Stop your program at a breakpoint, step through it, and inspect variables with "
    "watches and the call stack.",
    "20 minutes", "Lesson 6.",
    ["Setting a breakpoint and starting a session.",
     "The difference between Step, Step Over and Step Out.",
     "Reading the Locals, Globals, Call stack and Watch panes.",
     "Adding watch expressions and spotting changed values."],
    L7,
    [("The program runs to the end without stopping.",
      "No breakpoint was set, or it was on a line that generates no code. Set one on a "
      "line that does something."),
     ("Debugging will not start.",
      "The build does not carry debug information. Select a Debug build configuration and "
      "rebuild."),
     ("Stepping enters code I did not write.",
      "You pressed " + kbd("F11") + " on a library call. Press " + kbd("Shift", "F11") +
      " to step out, and use " + kbd("F10") + " next time."),
     ("A watch says the expression is not in scope.",
      "That variable is local to another procedure. It reports again once you are "
      "somewhere it exists."),
     ("Break does not interrupt the program.",
      "Break stops at the next line of your source. A program blocked in <code>Sleep</code> "
      "will not stop until it returns; use " + kbd("Shift", "F6") + " to stop outright.")],
    [kbd("F9") + " sets a breakpoint, " + kbd("F6") + " starts and continues.",
     kbd("F10") + " steps over, " + kbd("F11") + " steps in, " + kbd("Shift", "F11") +
     " steps out.",
     "Changed values are highlighted at each stop.",
     "Clicking a call stack frame shows that frame's locals without changing execution."],
    ["Put a breakpoint inside <code>Describe</code> and see which quantities reach it.",
     "Watch <code>items(i)</code> and step through the whole loop.",
     "Use Run to Cursor (" + kbd("Ctrl", "F10") + ") to reach the final "
     "<code>Print</code> without setting a breakpoint."],
    'In <a href="lesson-08.html">Lesson 8</a> you make the editor look and behave the way '
    "you want.",
    "tutorial lesson 7 debug breakpoint step over step into watch call stack locals")

# ==========================================================================
# Lesson 8
# ==========================================================================

L8 = ""
L8 += h2("Try the themes")
L8 += ol([
    "Press %s, or choose %s."
    % (kbd("Ctrl", "Shift", "T"), menu("File", "Settings", "Themes…")),
    "Try several themes — a dark one, a light one, a high-contrast one.",
    "Settle on one you like and choose <strong>OK</strong>.",
], steps=True)
L8 += p(
    "Notice that the whole application changed, not just the code area: the menus, panels, "
    "tabs and status bar all follow the theme."
)

L8 += h2("Set the font")
L8 += ol([
    "Open %s and go to the font settings."
    % menu("File", "Settings", "Options…"),
    "Change the size from 11 to 13. Choose <strong>OK</strong> and see the difference.",
    "Try a different monospaced font if you have one installed.",
    "Set <strong>extra line spacing</strong> to 3 and see how much easier dense code is "
    "to read.",
], steps=True)
L8 += important(
    "Use a <strong>monospaced</strong> font. Column selection, indent guides and the "
    "right-edge marker all assume every character is the same width."
)

L8 += h2("Zoom is not the font size")
L8 += ol([
    "Press %s three times, then %s three times." % (kbd("Ctrl", "+"), kbd("Ctrl", "-")),
    "Press %s to reset." % kbd("Ctrl", "0"),
], steps=True)
L8 += p(
    "Zoom is temporary and per-session; the font setting is permanent. Use zoom when you "
    "want a quick overview or need to read something closely, and the font setting for "
    "how you work all day."
)

L8 += h2("Turn on the display aids")
L8 += p("Open the options dialog and enable these, then look at your code:")
L8 += table(
    ["Option", "What you will see"],
    [
        ("Indent guides", "Faint vertical lines at each indent level."),
        ("Highlight current line", "The caret's line tinted."),
        ("Right edge", "A vertical line at column 80."),
        ("Brace highlight", "Matching parentheses highlighted as you move past them."),
        ("Occurrence highlight", "Every other use of the word under the caret marked."),
    ],
    key_first=True,
)
L8 += p("Keep the ones that help and turn off the ones that distract. There is no wrong answer.")

L8 += h2("Remap a shortcut")
L8 += ol([
    "Press %s to open the Keyboard Shortcuts dialog." % kbd("Ctrl", "K"),
    "Type <code>duplicate</code> in the filter box to find Duplicate Line.",
    "Select it and choose <strong>Modify</strong>.",
    "Press %s in the capture field." % kbd("Ctrl", "Shift", "D"),
    "Choose <strong>OK</strong>, then <strong>OK</strong> again.",
    "Test it in the editor. Then reopen the dialog, select the command and choose "
    "<strong>Reset</strong> to restore %s." % kbd("Ctrl", "D"),
], steps=True)
L8 += note(
    "Try assigning a keystroke that is already in use — %s, say. The dialog refuses it and "
    "tells you why, rather than silently taking it from the other command."
    % kbd("Ctrl", "S")
)

L8 += h2("See where it is all stored")
L8 += ol([
    "Close Tiko.",
    "Open <code>settings\\settings.ini</code> in Notepad and find "
    "<code>EditorFontsize</code> and <code>Theme</code>. Those are the changes you just "
    "made.",
    "Open <code>settings\\keybindings.ini</code>. If you left a remapping in place, it is "
    "the only thing in there — Tiko stores overrides, not defaults.",
    "Close both without saving, and start Tiko again.",
], steps=True)
L8 += tip(
    "Copy the <code>settings</code> folder to another machine and your entire setup goes "
    "with it — theme, shortcuts, preferences and all."
)

L8 += h2("Expected result")
L8 += p(
    "Tiko looks the way you want, you have remapped and reset a shortcut, and you know "
    "which files hold your configuration."
)

lesson(
    8, "lesson-08", "Customization",
    "Choose a theme and font, enable the display aids you like, and remap a keyboard "
    "shortcut.",
    "15 minutes", "Lessons 1–3. No compiler needed.",
    ["Switching themes and what a theme covers.",
     "Setting the editor font, size and line spacing.",
     "Why zoom and font size are different things.",
     "Remapping and resetting a keyboard shortcut.",
     "Which files hold your configuration."],
    L8,
    [("Code no longer lines up after changing font.",
      "You chose a proportional font. Pick a monospaced one."),
     ("A remapped shortcut does nothing.",
      "You closed the dialog with Cancel, which discards every change. Use OK."),
     ("A keystroke was refused.",
      "Another command already owns it. Choose a different one, or rebind the other "
      "command first."),
     ("Hand-edited settings.ini and the changes vanished.",
      "Tiko rewrites the file when it exits. Close Tiko before editing it.")],
    ["Themes cover the whole application, not just the editor.",
     "Zoom is temporary; the font setting is permanent.",
     "Every command can be remapped, and conflicts are refused rather than silently "
     "resolved.",
     "Everything is stored in the <code>settings</code> folder beside "
     "<code>tiko.exe</code>."],
    ["Try every bundled theme and keep the one you like best.",
     "Give Format Document (currently " + kbd("Shift", "Alt", "F") + ") a shortcut you "
     "find easier, then reset it.",
     "Back up your <code>settings</code> folder somewhere safe."],
    'In <a href="lesson-09.html">Lesson 9</a> you learn the navigation and productivity '
    "features that make day-to-day work fast.",
    "tutorial lesson 8 customization theme font shortcuts settings options")

# ==========================================================================
# Lesson 9
# ==========================================================================

L9 = ""
L9 += p(
    "This lesson is about speed. Everything here replaces something you can already do "
    "slowly."
)

L9 += h2("Goto Definition and back")
L9 += ol([
    "Open <code>inventory.bas</code> and put the caret on <code>Describe</code>.",
    "Press %s. Tiko opens <code>helpers.bi</code> at the function's definition." % kbd("F12"),
    "Press %s. You are back where you started." % kbd("Alt", "←"),
    "Press %s. Forward again." % kbd("Alt", "→"),
], steps=True)
L9 += important(
    "%s then %s is the core navigation loop — dive in, read, come back. If you learn one "
    "thing from this lesson, learn this pair." % (kbd("F12"), kbd("Alt", "←"))
)

L9 += h2("Search Symbol")
L9 += ol([
    "Press %s." % kbd("Ctrl", "P"),
    "Type <code>desc</code>. The list narrows to matching symbols.",
    "Choose <code>Describe</code> and you jump straight to it — without knowing or caring "
    "which file it is in.",
], steps=True)

L9 += h2("The function list")
L9 += ol([
    "Press %s to show the function list." % kbd("F4"),
    "Open <code>helpers.bi</code> and see <code>Describe</code> listed.",
    "Click it to jump there.",
], steps=True)
L9 += p("On a file with thirty procedures this replaces a great deal of scrolling.")

L9 += h2("Autocomplete and code tips")
L9 += ol([
    "In <code>inventory.bas</code>, start a new line and type <code>Desc</code>.",
    "The completion list appears. Press %s to accept <code>Describe</code>." % kbd("Tab"),
    "Type <code>(</code>. A code tip appears showing the parameter list.",
    "Type <code>qty(1))</code> and the tip closes.",
    "Delete the experimental line.",
], steps=True)
L9 += tip(
    "Typing <code>(</code> also accepts the highlighted completion — so <code>Desc</code> "
    "followed by <code>(</code> completes the name, inserts the parenthesis and shows the "
    "parameters in one keystroke."
)

L9 += h2("Bookmarks")
L9 += ol([
    "Put the caret on the <code>For</code> line and press %s." % kbd("Ctrl", "F2"),
    "Put the caret on the final <code>Print</code> and press %s again." % kbd("Ctrl", "F2"),
    "Press %s repeatedly to cycle between them." % kbd("F2"),
    "Press %s to see them listed in the side panel." % kbd("Shift", "F4"),
    "Press %s to clear them." % kbd("Ctrl", "Shift", "F2"),
], steps=True)

L9 += h2("The formatter")
L9 += ol([
    "Deliberately mangle the indentation of a few lines in <code>inventory.bas</code>.",
    "Press %s (Format Document)." % kbd("Shift", "Alt", "F"),
    "The indentation is repaired.",
    "Press %s. The format is undone in one step, and your typing is preserved."
    % kbd("Ctrl", "Z"),
], steps=True)
L9 += ol([
    "Open %s." % menu("Edit", "Format", "Format Options…"),
    "Turn on <strong>Case keywords</strong> and watch the live preview change.",
    "Choose <strong>Cancel</strong> to leave your settings as they were.",
], steps=True)
L9 += note(
    "The formatter never moves a line break. It will not turn <code>If x Then y</code> "
    "into a block or join lines — where your statements begin and end stays your decision."
)

L9 += h2("Jump between related files")
L9 += p(
    "With <code>inventory.bas</code> active, press %s to go to the header, and %s to come "
    "back." % (kbd("Ctrl", "Shift", "H"), kbd("Ctrl", "Shift", "C"))
)

L9 += h2("Expected result")
L9 += p(
    "You can move around the project by symbol name rather than by scrolling, and you have "
    "used autocomplete, code tips, bookmarks and the formatter."
)

lesson(
    9, "lesson-09", "Productivity shortcuts",
    "Navigate by symbol rather than by scrolling, and use autocomplete, bookmarks and the "
    "formatter.",
    "15 minutes", "Lessons 1–5.",
    ["Goto Definition and the navigation history.",
     "Finding any symbol in the project by name.",
     "Using autocomplete and reading code tips.",
     "Setting and cycling through bookmarks.",
     "Reformatting code safely."],
    L9,
    [(kbd("F12") + " does nothing.",
      "The caret is not on a symbol, or the file defining it is not in the project."),
     ("Autocomplete does not appear.",
      "It is turned off in the options, or too few characters have been typed."),
     ("A code tip shows nothing for my own procedure.",
      "The file declaring it is not in the workspace, so the parser has not indexed it."),
     ("The formatter changed more than expected.",
      "A rule you did not intend is enabled. Press " + kbd("Ctrl", "Z") +
      " and review " + menu("Edit", "Format", "Format Options…") + ".")],
    [kbd("F12") + " jumps to a definition, " + kbd("Alt", "←") + " comes back.",
     kbd("Ctrl", "P") + " finds any symbol in the project by name.",
     kbd("F4") + " lists the current file's procedures.",
     "The formatter verifies it preserved your code before it changes anything."],
    ["Navigate from every call in the project to its definition and back using only " +
     kbd("F12") + " and " + kbd("Alt", "←") + ".",
     "Add a second function to <code>helpers.bi</code> and check that it appears in "
     "autocomplete straight away.",
     "Set bookmarks in both files and cycle between them with " + kbd("F2") + "."],
    'In <a href="lesson-10.html">Lesson 10</a> you finish with the advanced editing '
    "features.",
    "tutorial lesson 9 navigation goto definition search symbol autocomplete bookmarks "
    "formatter")

# ==========================================================================
# Lesson 10
# ==========================================================================

L10 = ""
L10 += h2("Split the editor")
L10 += ol([
    "Open <code>inventory.bas</code>.",
    "Press %s to split top and bottom." % kbd("Ctrl", "Shift", "\\"),
    "Scroll one view to the top and the other to the bottom. You are looking at two parts "
    "of the same file at once.",
    "Type in one view and watch the change appear in the other — it is one document, not "
    "a copy.",
    "Press %s again to return to a single view." % kbd("Ctrl", "Shift", "\\"),
    "Try %s for a left/right split." % kbd("Ctrl", "\\"),
], steps=True)

L10 += h2("Fold the code")
L10 += ol([
    "Press %s (Fold All). Every block collapses and you see the file's outline."
    % kbd("Shift", "F8"),
    "Press %s on a folded block to expand just that one." % kbd("F8"),
    "Press %s to expand everything." % kbd("Ctrl", "Shift", "F8"),
], steps=True)
L10 += tip(
    "Fold All is a good way to get your bearings in an unfamiliar file — you see the "
    "structure before the detail."
)

L10 += h2("Multiple selections")
L10 += ol([
    "Hold %s and double-click <code>qty</code> in three different places." % kbd("Ctrl"),
    "You now have three independent selections.",
    "Type <code>count</code>. All three change at once.",
    "Press %s to undo, and click once to collapse to a single caret." % kbd("Ctrl", "Z"),
], steps=True)
L10 += p(
    "Multiple selections need not line up, which is what makes them different from column "
    "mode. Use column mode when things do line up, and this when they do not."
)

L10 += h2("Define a user tool")
L10 += ol([
    "Open %s and choose <strong>Add</strong>." % menu("File", "User Tools…"),
    "Description: <code>Open project folder</code>.",
    "Program: <code>explorer.exe</code>.",
    "Parameters: the substitution code for the current file's folder — the Parameters "
    "field's tooltip lists the codes available.",
    "Choose <strong>OK</strong>, then run it from %s." % menu("File", "User Tools"),
], steps=True)
L10 += todo(
    "Insert the exact parameter substitution code for the current file's directory once it "
    "has been confirmed against the shipping build.",
    title="TODO — supply the exact substitution code",
)

L10 += h2("Use the Output panel's TODO tab")
L10 += ol([
    "Add a comment to <code>helpers.bi</code>: <code>' TODO: handle negative "
    "quantities</code>.",
    "Open the Output panel (%s) and select the <strong>TODO</strong> tab."
    % kbd("Ctrl", "F9"),
    "Your comment is listed. Click it to jump there.",
], steps=True)
L10 += p(
    "TODO comments live in the source, so everyone who reads the code sees them — unlike "
    "bookmarks, which are yours alone."
)

L10 += h2("Take stock")
L10 += p("You have now used every major feature of the editor:")
L10 += table(
    ["Area", "Covered in"],
    [
        ("Files and the interface", "Lessons 1–2"),
        ("Editing", "Lessons 3, 10"),
        ("Searching", "Lesson 4"),
        ("Projects", "Lesson 5"),
        ("Building", "Lesson 6"),
        ("Debugging", "Lesson 7"),
        ("Customization", "Lesson 8"),
        ("Navigation and productivity", "Lesson 9"),
    ],
)

L10 += h2("Expected result")
L10 += p(
    "You are comfortable with split views, folding, multiple selections and user tools, "
    "and you have finished the tutorial."
)

lesson(
    10, "lesson-10", "Advanced editing",
    "Split views, code folding, multiple selections, user tools and the TODO list.",
    "20 minutes", "Lessons 1–9.",
    ["Viewing two parts of one file at once.",
     "Folding code to see a file's structure.",
     "Editing several unrelated places at once.",
     "Defining an external tool.",
     "Using TODO comments as a shared task list."],
    L10,
    [("The split views show different files.",
      "They cannot — a split shows one document twice. You switched tabs in one of them."),
     ("Folding markers are missing.",
      "The fold margin is turned off. Enable it in " +
      menu("File", "Settings", "Options…") + "."),
     (kbd("Ctrl") + "-click starts a new selection instead of adding one.",
      "Hold " + kbd("Ctrl") + " for the whole gesture, from before the click."),
     ("A user tool does nothing.",
      "Check the program path and the working folder. Try it with no parameters first."),
     ("TODO comments are not listed.",
      "The file is not in the workspace, or the comment does not match the expected "
      "form.")],
    ["A split shows one document in two views, kept in sync.",
     "Fold All gives you an instant outline of a file.",
     "Multiple selections handle places that do not line up; column mode handles places "
     "that do.",
     "User tools bring external programs into the editor."],
    ["Split the editor and use one half to read <code>Describe</code> while editing its "
     "caller in the other.",
     "Add TODO comments to both files and work through them from the Output panel.",
     "Define a user tool that runs a command you use often outside the editor."],
    'You have finished the tutorial. Next: browse the '
    '<a href="tips-and-tricks.html">Tips and tricks</a> for power-user workflows, keep the '
    '<a href="keyboard-shortcuts.html">keyboard shortcuts</a> to hand, and use the '
    '<a href="faq.html">FAQ</a> when a specific question comes up.',
    "tutorial lesson 10 advanced split view folding multiple selections user tools todo")
