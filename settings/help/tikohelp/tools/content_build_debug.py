# -*- coding: utf-8 -*-
"""Building Programs and Debugging sections."""

from build import (section, page, h2, h3, h4, p, ul, ol, dl, code, table, cards,
                   note, tip, warn, important, todo, kbd, menu, ui,
                   figure_img, placeholder, diagram)

# ==========================================================================
section("building", "Building Programs", "build")
# ==========================================================================

CS = ""
CS += important(
    "<strong>Tiko Editor already comes with a compiler.</strong> The most recent FreeBASIC "
    "toolchain ships with the editor and is installed and selected out of the box, so "
    "there is normally nothing to set up — you can build and run from the moment you "
    "unpack it."
)
CS += p(
    "You only need this page if you want to <em>add</em> a toolchain — an older FreeBASIC "
    "version to check compatibility against, a newer one to try, or a variant built with a "
    "different backend — and switch between them."
)

CS += h2("How toolchains are organised")
CS += p(
    "Compiler toolchains live as <strong>subfolders of the <code>toolchains\\</code> "
    "folder</strong>, beside <code>tiko-editor.exe</code>. Each subfolder is one complete "
    "toolchain, and you may install as many as you like."
)
CS += code("""
tiko-editor.exe
toolchains\\
    FreeBASIC-1.10.1-winlibs-gcc-9.3.0\\     <- ships with Tiko Editor
        fbc32.exe                            <- both executables are required
        fbc64.exe
        bin\\  inc\\  lib\\  examples\\  doc\\
    FreeBASIC-1.09.0\\                        <- add as many as you like
        fbc32.exe
        fbc64.exe
        ...
""", lang="text", title="The toolchains folder", numbered=False)
CS += important(
    "<strong>Each toolchain must have both <code>fbc32.exe</code> and "
    "<code>fbc64.exe</code> in its root folder.</strong> Tiko Editor always resolves both from "
    "the one folder you select, because the 32-bit or 64-bit choice is made per build, not "
    "per toolchain — see below."
)

CS += h2("Adding a toolchain")
CS += ol([
    "Unpack the FreeBASIC distribution into a new subfolder of <code>toolchains\\</code>. "
    "Name the folder something you will recognise in a list — the version number is the "
    "obvious choice.",
    "Check that <code>fbc32.exe</code> and <code>fbc64.exe</code> are both directly in "
    "that folder, not one level down.",
    "Open %s and select the <strong>Compiler</strong> page." % menu("File", "Settings", "Options…"),
    "The new toolchain appears in the list — Tiko Editor scans the folder each time the page "
    "opens, so nothing needs registering.",
], steps=True)

CS += h2("Choosing which toolchain to use")
CS += ol([
    "Open %s." % menu("File", "Settings", "Options…"),
    "Select the <strong>Compiler</strong> page.",
    "Click the toolchain you want in the list.",
    "Choose <strong>OK</strong>.",
], steps=True)
CS += figure_img(
    "assets/img/options-compiler.png",
    "The Compiler page. The list shows every subfolder of <code>toolchains\\</code>; "
    "selecting one points the editor at both of its compilers at once.",
    alt="The Compiler page of the Tiko Editor options dialog")
CS += note(
    "Selecting a toolchain sets the paths to <em>both</em> its compilers together. You "
    "never pick an individual <code>.exe</code>, which is why the two files must both be "
    "present."
)

CS += h2("The rest of the Compiler page")
CS += table(
    ["Control", "What it does"],
    [
        ("Toolchain list", "Every subfolder of <code>toolchains\\</code>. The selected one "
         "supplies both compilers."),
        ("Compiler switches", "Extra switches added to <em>every</em> build, whatever the "
         "project or build configuration. Normally left empty — switches usually belong "
         'in a <a href="build-configurations.html">build configuration</a> or in '
         '<a href="project-options.html">project options</a> instead.'),
        ("Include paths", "Additional directories searched for <code>#include</code> "
         "files, on top of the toolchain's own <code>inc</code> folder."),
        ("Run via command window", "Launch your compiled program through a command window "
         "rather than directly. Useful for a console program that exits immediately, so "
         "you can still read its output."),
        ("Disable compile beep", "Suppress the sound Tiko Editor makes when a build finishes."),
    ],
    key_first=True,
)

CS += h2("32-bit and 64-bit")
CS += important(
    "<strong>Architecture is chosen by the build configuration, not by the toolchain.</strong> "
    "Because every toolchain provides both <code>fbc32.exe</code> and "
    "<code>fbc64.exe</code>, switching between a Win32 and a Win64 build is a matter of "
    'selecting a different <a href="build-configurations.html">build configuration</a> — '
    "Tiko Editor then invokes the matching compiler from the toolchain you selected."
)
CS += p(
    "If your project links against a DLL or static library of a particular architecture, "
    "the two must match: a 64-bit program cannot load a 32-bit DLL."
)

CS += h2("Checking that it works")
CS += ol([
    "Create a new file and type a one-line program: <code>Print \"ok\" : Sleep</code>",
    "Save it.",
    "Press %s." % kbd("F5"),
    "The Output panel should show the compiler being invoked, and your program should run.",
], steps=True)
CS += warn(
    "If the build fails immediately with a message about the compiler not being found, the "
    "problem is Tiko Editor's configuration rather than your code. Check that a toolchain is "
    "selected on the Compiler page, and that the folder it names still contains both "
    "<code>fbc32.exe</code> and <code>fbc64.exe</code>. See "
    '<a href="troubleshooting.html">Troubleshooting</a>.'
)

CS += h2("Compiler switches")
CS += p("Switches reach the compiler from four places, which combine:")
CS += ol([
    "The active <a href=\"build-configurations.html\">build configuration</a> — the "
    "reusable set, such as debug or release, and the one that decides 32- or 64-bit.",
    "The <a href=\"project-options.html\">project's own compiler options</a> — switches "
    "specific to this project.",
    "The <strong>Compiler switches</strong> field on the Compiler options page — applied "
    "to every build in every project. Use sparingly.",
    "Anything Tiko Editor must add itself, such as the output file name.",
], steps=True)
CS += tip(
    "%s shows the exact command line Tiko Editor will use. When a build does something you did "
    "not expect, look there first — it usually settles the question immediately."
    % menu("Compile", "Command Line…")
)

CS += h2("Related topics")
CS += ul([
    '<a href="building.html">Building and running</a>',
    '<a href="build-configurations.html">Build configurations</a>',
    '<a href="project-options.html">Project options</a>',
])

page("compiler-setup", "Compiler setup", "building",
     "Tiko Editor ships with a FreeBASIC toolchain ready to use. How toolchains are organised, "
     "how to add more, and how the 32- and 64-bit choice is really made.",
     CS,
     keywords="compiler setup toolchain toolchains folder fbc32 fbc64 freebasic bundled "
              "included compiler configuration 32-bit 64-bit switches include paths "
              "run via command window compile beep add toolchain")

# --------------------------------------------------------------------------

BD = ""
BD += p(
    "Tiko Editor's build commands all live on the %s menu and all report into the Output panel's "
    "<strong>Compiler</strong> tab." % menu("Compile")
)

BD += h2("The commands")
BD += table(
    ["Command", "Shortcut", "What it does", "Use when"],
    [
        ("Build and Execute", kbd("F5"),
         "Compiles the project, then runs the result if the compile succeeded.",
         "Almost always. This is the main command."),
        ("Compile", kbd("Ctrl", "F5"), "Compiles without running.",
         "You want to check that it builds, without launching anything."),
        ("Rebuild All", kbd("Ctrl", "Alt", "F5"),
         "Rebuilds everything from scratch, ignoring anything already built.",
         "After changing compiler switches, or when a build behaves inconsistently."),
        ("Compile Module", kbd("Ctrl", "F7"), "Compiles just the current module.",
         "Checking one file compiles, without building the whole project."),
        ("Quick Run", kbd("Ctrl", "Shift", "F5"),
         "Compiles and runs the current file alone, ignoring the project.",
         "Testing a scratch file or a small experiment."),
        ("Run Executable", kbd("Shift", "F5"),
         "Runs the last successful build without recompiling.",
         "Running again after a build you already did."),
        ("Command Line…", "—",
         "Shows the compiler command line Tiko Editor will use.",
         "Diagnosing why a build behaves unexpectedly."),
    ],
    key_first=True,
)
BD += tip(
    "<strong>Quick Run</strong> is worth remembering. It builds and runs the current file "
    "on its own with no project set-up, which makes Tiko Editor a perfectly good scratchpad for "
    "trying out a language feature."
)

BD += h2("Saving before a build")
BD += p(
    "The <strong>Compile autosave</strong> option — on by default — saves modified "
    "documents automatically before each build. It prevents the classic confusion of "
    "building the previous version of a file and wondering why your change had no effect."
)

BD += h2("What happens during a build")
BD += ol([
    "Modified files are saved, if Compile autosave is on.",
    "Tiko Editor assembles the command line from the build configuration, the project options and "
    "the file set.",
    "The Output panel switches to the Compiler tab and shows the command being run.",
    "Compiler output is captured line by line as it arrives.",
    "The result is reported. Errors and warnings become clickable rows.",
    "If the build succeeded and the command runs the program, it launches.",
], steps=True)

BD += h2("Running your program")
BD += p(
    "Your program runs as a separate process. Console programs get their own console "
    "window; GUI programs open their own window. Command-line arguments come from the "
    'project options — see <a href="project-options.html">Project options</a>.'
)
BD += note(
    "Because the program is a separate process, closing Tiko Editor does not close it, and a "
    "crash in your program cannot take the editor down with it."
)

BD += h2("Related topics")
BD += ul([
    '<a href="compiler-errors.html">Compiler errors and warnings</a>',
    '<a href="build-configurations.html">Build configurations</a>',
    '<a href="debugging.html">Debugging</a>',
])

page("building", "Building and running", "building",
     "The build commands and what each is for, saving before a build, and what happens "
     "while a build runs.",
     BD,
     keywords="build compile execute run rebuild all quick run run executable compile "
              "module command line f5 autosave")

# --------------------------------------------------------------------------

BC = ""
BC += p(
    "A build configuration is a named set of compiler switches you can switch between — "
    "most commonly a <strong>Debug</strong> configuration carrying debug information and a "
    "<strong>Release</strong> one carrying optimisation. Open the manager with "
    "%s or %s."
    % (menu("File", "Settings", "Build Configurations…"), kbd("F7"))
)

BC += h2("What is in a configuration")
BC += table(
    ["Part", "Purpose"],
    [
        ("Description", "The name shown in the menus and the status bar."),
        ("Compiler switches", "The switches this configuration passes to the compiler, "
         "chosen from a checklist rather than typed."),
        ("Keyboard shortcut", "An optional accelerator that activates this configuration."),
        ("Default marker", "Marks the configuration used when a project does not "
         "specify one."),
    ],
    key_first=True,
)

BC += h2("Using the dialog")
BC += p("The dialog lists your configurations on the left and their settings on the right.")
BC += table(
    ["Command", "What it does"],
    [
        ("Add", "Creates a new configuration."),
        ("Delete", "Removes the selected one."),
        ("Move up / Move down", "Reorders the list — the order they appear in menus."),
    ],
    key_first=True,
)
BC += p(
    "The settings side has two pages: <strong>General</strong> for the description, "
    "shortcut and behaviour, and <strong>Compiler switches</strong> for the switch "
    "checklist. Switches that are mutually exclusive are grouped, so choosing one clears "
    "the others in that group."
)
BC += figure_img(
    "assets/img/build-configurations.png",
    "The Build Configurations dialog. Configurations are listed on the left; the right "
    "side holds the General settings and the compiler-switch checklist.",
    alt="The Build Configurations dialog")

BC += h2("Shortcuts and clashes")
BC += p(
    "A configuration can have its own keyboard shortcut so you can switch without opening "
    "any menu. If a shortcut cannot work — because the key name is not recognised, or "
    "because an editor command or user tool already claims it — Tiko Editor simply leaves that "
    "configuration unassigned rather than giving you a shortcut that silently never fires."
)
BC += note(
    "Where two things want the same keystroke, the first one to claim it keeps it. Assign "
    "a different key to the loser."
)

BC += h2("Switching configuration")
BC += ul([
    "Click the build configuration field in the status bar.",
    "Use the configuration's own keyboard shortcut, if you gave it one.",
    "Choose it from the build configuration menu.",
])
BC += warn(
    "After switching configuration, use <strong>Rebuild All</strong> (%s) rather than an "
    "incremental build. Object files produced under the old switches are not compatible "
    "with the new ones." % kbd("Ctrl", "Alt", "F5")
)

BC += h2("Debug builds")
BC += p(
    "Debugging needs a build that carries debug information — the <code>-g</code> switch. "
    "The four <strong>(Debug)</strong> configurations above set it, so in normal use you "
    "select the one matching your program (Win64 Console (Debug), say) and press %s. See "
    '<a href="debugging.html">Debugging</a>.' % kbd("F6")
)
BC += p(
    "Tiko Editor ships with twelve configurations covering the usual combinations — 32- and "
    "64-bit, GUI and console, release and debug, plus DLL and static library targets:"
)
BC += table(
    ["Configuration", "Switches", "Architecture"],
    [
        ("Win32 GUI (Release)", "<code>-s gui</code>", "32-bit"),
        ("Win32 GUI (Debug)", "<code>-g -exx -s gui</code>", "32-bit"),
        ("Win32 Console (Release)", "<code>-s console</code>", "32-bit"),
        ("Win32 Console (Debug)", "<code>-g -exx -s console</code>", "32-bit"),
        ("Win32 Windows DLL", "<code>-s gui -dll -export</code>", "32-bit"),
        ("Win32 Static Library", "<code>-lib</code>", "32-bit"),
        ("Win64 GUI (Release)", "<code>-s gui -gen gcc</code>", "64-bit"),
        ("Win64 GUI (Debug)", "<code>-g -exx -s gui -gen gas64</code>", "64-bit"),
        ("Win64 Console (Release)", "<code>-s console -gen gcc</code>", "64-bit"),
        ("Win64 Console (Debug)", "<code>-g -exx -s console -gen gas64</code>", "64-bit"),
        ("Win64 Windows DLL", "<code>-s gui -dll -export -gen gcc</code>", "64-bit"),
        ("Win64 Static Library", "<code>-lib</code>", "64-bit"),
    ],
    key_first=True,
)
BC += h3("Reading those switches")
BC += table(
    ["Switch", "Meaning"],
    [
        ("<code>-g</code>", "Emit debug information. <strong>Required for debugging</strong> "
         "— this is what makes the Debug configurations debuggable."),
        ("<code>-exx</code>", "Full runtime error checking: array bounds, null pointers, "
         "file I/O. Slower, and worth every cycle while developing."),
        ("<code>-s gui</code>", "A windowed program, with no console attached."),
        ("<code>-s console</code>", "A console program."),
        ("<code>-dll -export</code>", "Build a DLL and export its public symbols."),
        ("<code>-lib</code>", "Build a static library rather than an executable."),
        ("<code>-gen gcc</code>", "Use the gcc backend — slower to compile, better "
         "optimised. Used by the 64-bit release configurations."),
        ("<code>-gen gas64</code>", "Use the gas64 backend — much faster to compile. Used "
         "by the 64-bit debug configurations, where build speed matters most."),
    ],
    key_first=True,
)
BC += important(
    "<strong>The configuration decides the architecture, not the toolchain.</strong> Every "
    "toolchain carries both <code>fbc32.exe</code> and <code>fbc64.exe</code>, so choosing "
    "a Win32 or Win64 configuration is what selects which compiler runs. See "
    '<a href="compiler-setup.html">Compiler setup</a>.'
)
BC += note(
    "The DLL and static library configurations are marked to stay out of the quick popup "
    "list, since they are not what most projects build. They are still selectable in this "
    "dialog."
)

BC += h2("Related topics")
BC += ul([
    '<a href="compiler-setup.html">Compiler setup</a>',
    '<a href="project-options.html">Project options</a>',
    '<a href="keyboard-customization.html">Customizing keyboard shortcuts</a>',
])

page("build-configurations", "Build configurations", "building",
     "Named sets of compiler switches: creating them, giving them shortcuts, switching "
     "between them, and why a rebuild follows a switch.",
     BC,
     keywords="build configuration debug release compiler switches f7 default "
              "configuration shortcut rebuild")

# --------------------------------------------------------------------------

CE = ""
CE += p(
    "When a build fails, the compiler's messages appear in the Output panel's "
    "<strong>Compiler</strong> tab. Tiko Editor parses them so each becomes a link into your code."
)

CE += h2("Navigating to an error")
CE += p(
    "<strong>Click any error row</strong> to open that file and put the caret on the line "
    "the compiler named. Work down the list from the top."
)
CE += important(
    "Fix the <strong>first</strong> error first, then rebuild. A single early mistake — an "
    "unclosed block, a missing include — routinely produces dozens of later errors that "
    "vanish the moment the first is fixed. Chasing them individually wastes time."
)

CE += h2("Errors and warnings")
CE += table(
    ["Kind", "Meaning", "Action"],
    [
        ("Error", "The compiler could not produce output.", "Must be fixed."),
        ("Warning", "The code compiled, but something looks suspicious.",
         "Should be read. Warnings often identify real bugs — an uninitialised variable, "
         "a suspicious conversion."),
    ],
    key_first=True,
)
CE += tip(
    "Build with the compiler's full warning switch turned on in your debug configuration. "
    "A warning you never see cannot help you."
)

CE += h2("Reading a message")
CE += p("A compiler message normally carries four things:")
CE += ul([
    "the <strong>file</strong> it occurred in — not necessarily the one you were editing;",
    "the <strong>line</strong>, and often the column;",
    "the <strong>severity</strong>, error or warning;",
    "the <strong>text</strong> describing the problem.",
])
CE += note(
    "The reported line is where the compiler <em>noticed</em> the problem, which is not "
    "always where the problem is. An unterminated block is reported at the end of the "
    "file, while the actual mistake is wherever the block was opened."
)

CE += h2("Common FreeBASIC build failures")
CE += table(
    ["Message resembles", "Usual cause"],
    [
        ("Variable not declared", "A typo, a missing <code>Dim</code>, or a missing "
         "<code>#include</code> for the header that declares it."),
        ("Expected ‘End If’ / ‘Next’ / ‘End Sub’", "A block opened and never closed — "
         "look above the reported line."),
        ("Duplicated definition", "The same name declared twice, or a header included "
         "twice without <code>#include once</code>."),
        ("File not found", "An <code>#include</code> path that is wrong, or an include "
         "directory missing from the compiler switches."),
        ("Undefined reference (at link time)", "A procedure declared but never "
         "implemented, or a library not linked."),
    ],
)

CE += h2("Related topics")
CE += ul([
    '<a href="output-panel.html">Output panel</a>',
    '<a href="building.html">Building and running</a>',
    '<a href="troubleshooting.html">Troubleshooting</a>',
    '<a href="debugging.html">Debugging</a> — for problems that only appear at run time.',
])

page("compiler-errors", "Compiler errors and warnings", "building",
     "Reading build output, jumping to the offending line, the difference between errors "
     "and warnings, and the failures you will meet most often.",
     CE,
     keywords="compiler errors warnings build failed output panel navigate error click "
              "error line number link")

# ==========================================================================
section("debugging", "Debugging", "bug")
# ==========================================================================

DB = ""
DB += p(
    "Tiko Editor has a real source-level debugger. It runs your program under its own control, so "
    "you can stop it at a chosen line, step through it a statement at a time, and look at "
    "the value of any variable while it is stopped."
)
DB += p(
    "The debugger reads the debug information the FreeBASIC compiler emits directly, so "
    "the program you debug is the same binary you built — not a differently-generated one."
)

DB += h2("Before you start")
DB += ol([
    "Select a build configuration that produces debug information — a Debug configuration.",
    "Build the project (%s) so the executable is current." % kbd("Ctrl", "F5"),
    "Set at least one breakpoint, otherwise the program simply runs to completion.",
], steps=True)

DB += h2("Starting and stopping")
DB += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Start Debugging", kbd("F6"),
         "Builds if needed, launches your program under the debugger and runs it until it "
         "hits a breakpoint."),
        ("Continue Debugging", kbd("F6"),
         "The same command once a session is running: resumes from where it stopped."),
        ("Break", kbd("Ctrl", "F6"),
         "Interrupts a running program at the next line of your source."),
        ("Stop Debugging", kbd("Shift", "F6"),
         "Ends the session and terminates the program."),
    ],
    key_first=True,
)
DB += note(
    "<strong>Break</strong> stops at the next line of <em>your</em> code, so a program "
    "sitting inside a long <code>Sleep</code> or blocked in a library call will not stop "
    "until it returns to your source. <strong>Stop Debugging</strong> works at any time."
)

DB += h2("Stepping")
DB += table(
    ["Command", "Shortcut", "What it does"],
    [
        ("Step", kbd("F11"),
         "Executes the current line. If it calls one of your procedures, stops on that "
         "procedure's first line."),
        ("Step Over", kbd("F10"),
         "Executes the current line, running any call it makes to completion without "
         "stopping inside."),
        ("Step Out", kbd("Shift", "F11"),
         "Runs until the current procedure returns, then stops at the caller."),
        ("Run to Cursor", kbd("Ctrl", "F10"),
         "Resumes and stops when execution reaches the line the caret is on — a "
         "breakpoint you do not have to set and clear."),
    ],
    key_first=True,
)
DB += tip(
    "Step Over (%s) is the one to reach for by default. Use Step (%s) only when you "
    "actually want to go inside the call — otherwise you spend your time in code you were "
    "not investigating." % (kbd("F10"), kbd("F11"))
)
DB += p(
    "The line about to execute is marked in the editor, and the marker follows you as you "
    "step."
)

DB += h2("The debugger window")
DB += p("While debugging, four panes show you the state of your program:")
DB += diagram("""
<style>
  .db-bg { fill: var(--c-bg-sunken); }
  .db-box { fill: var(--c-surface); stroke: var(--c-border-strong); stroke-width: 1.5; }
  .db-h { fill: var(--c-accent-soft); stroke: var(--c-accent); stroke-width: 1.5; }
  .db-t { fill: var(--c-heading); font-family: var(--font-sans); font-size: 13px; font-weight: 600; }
  .db-s { fill: var(--c-text-mute); font-family: var(--font-mono); font-size: 11px; }
</style>
<rect x="0" y="0" width="800" height="360" class="db-bg"/>

<rect x="20" y="16" width="376" height="26" class="db-h"/>
<text x="32" y="34" class="db-t">Globals</text>
<rect x="20" y="42" width="376" height="130" class="db-box"/>
<text x="32" y="64" class="db-s">gConfig        CONFIG_TYPE</text>
<text x="32" y="84" class="db-s">gAppName       "tiko"</text>
<text x="32" y="104" class="db-s">▸ gBuffer(0..999)   [1000 elements]</text>

<rect x="20" y="184" width="376" height="26" class="db-h"/>
<text x="32" y="202" class="db-t">Locals</text>
<rect x="20" y="210" width="376" height="130" class="db-box"/>
<text x="32" y="232" class="db-s">i              12</text>
<text x="32" y="252" class="db-s">total          438.75</text>
<text x="32" y="272" class="db-s">name           "widget"</text>
<text x="32" y="292" class="db-s">▸ rc           RECT</text>

<rect x="410" y="16" width="370" height="26" class="db-h"/>
<text x="422" y="34" class="db-t">Call stack</text>
<rect x="410" y="42" width="370" height="130" class="db-box"/>
<text x="422" y="64" class="db-s">ComputeTotal   main.bas:142</text>
<text x="422" y="84" class="db-s">ProcessRow     main.bas:98</text>
<text x="422" y="104" class="db-s">main           main.bas:31</text>

<rect x="410" y="184" width="370" height="26" class="db-h"/>
<text x="422" y="202" class="db-t">Watch</text>
<rect x="410" y="210" width="370" height="130" class="db-box"/>
<text x="422" y="232" class="db-s">count          12          ×</text>
<text x="422" y="252" class="db-s">rows(i).total  438.75      ×</text>
<text x="422" y="272" class="db-s">&lt;click to add a watch&gt;</text>
""", "The four debugger panes. Globals and Locals on the left, the call stack and your "
     "watch expressions on the right. Every divider is draggable, and the layout is "
     "remembered between sessions.")

DB += table(
    ["Pane", "Shows"],
    [
        ("Globals", "Variables visible throughout the program."),
        ("Locals", "Variables in the procedure currently in scope. Click a call stack "
         "frame to see that frame's locals instead."),
        ("Call stack", "The chain of calls that reached the current line, innermost "
         "first. Click a frame to look at its scope."),
        ("Watch", "Expressions you asked to track. Click the placeholder row to add one, "
         "click a row to edit it, and use the × column to delete it."),
    ],
    key_first=True,
)

DB += h2("Inspecting values")
DB += ul([
    "<strong>Hover</strong> the mouse over a variable in the editor to see its value in a "
    "data tip.",
    "<strong>Expand</strong> a structure or array with its twisty to see its members or "
    "elements.",
    "<strong>Large arrays are grouped</strong> into ranges — a ten-thousand-element array "
    "appears as a hundred groups of a hundred rather than ten thousand rows — so you can "
    "drill down to one element without scrolling past the rest.",
    "<strong>Values that changed</strong> since the program last stopped are shown in a "
    "different colour, which makes stepping through a loop far easier to follow.",
])
DB += note(
    "Values are read while your program is stopped, so they are always consistent. "
    "Expanding a structure or adding a watch does not resume the program."
)

DB += h2("Related topics")
DB += ul([
    '<a href="breakpoints.html">Breakpoints</a>',
    '<a href="watches.html">Watches and the call stack</a>',
    '<a href="build-configurations.html">Build configurations</a>',
    '<a href="troubleshooting.html">Troubleshooting</a>',
])

page("debugging", "Debugging", "debugging",
     "Running your program under Tiko Editor's debugger: starting and stopping, stepping, the "
     "four debugger panes, and inspecting values.",
     DB,
     keywords="debug debugger start debugging continue break stop step into step over "
              "step out run to cursor f6 f10 f11 panes globals locals call stack watch "
              "data tip")

# --------------------------------------------------------------------------

BP = ""
BP += p(
    "A breakpoint tells the debugger to stop your program when it reaches a particular "
    "line. Everything else in debugging follows from being stopped somewhere useful."
)

BP += h2("Setting a breakpoint")
BP += table(
    ["Method", "How"],
    [
        ("Keyboard", "Put the caret on the line and press " + kbd("F9") + "."),
        ("Menu", menu("Debug", "Toggle Breakpoint")),
        ("Mouse", "Click the left margin beside the line — if <strong>Click toggles "
         "breakpoint</strong> is enabled in the options."),
    ],
    key_first=True,
)
BP += p(
    "A marker appears in the margin. The same command removes it again. "
    "%s clears every breakpoint in the project."
    % menu("Debug", "Delete All Breakpoints")
)

BP += h2("Breakpoints on lines that generate no code")
BP += important(
    "Not every line becomes machine code. Comments, blank lines and bare declarations "
    "produce nothing to stop at. Rather than accepting a breakpoint that would silently "
    "never fire, Tiko Editor moves it forward to the next line that does generate code."
)
BP += p(
    "If a breakpoint appears to jump one or two lines when you set it, that is what "
    "happened — and it is doing you a favour."
)

BP += h2("Breakpoints and rebuilding")
BP += p(
    "Breakpoints are stored by file and line, so they survive editing and rebuilding. "
    "Inserting lines above a breakpoint moves the marker with the code, because the marker "
    "lives in the editor's line markers rather than at a fixed line number."
)
BP += note(
    "Breakpoints set before a session starts are armed when the program launches, so you "
    "can prepare the breakpoints you want and then press %s once." % kbd("F6")
)

BP += h2("Run to Cursor")
BP += p(
    "%s resumes and stops when execution reaches the caret's line. It is the right tool "
    "when you want to stop somewhere just once — no breakpoint to set, and none to "
    "remember to remove afterwards." % kbd("Ctrl", "F10")
)

BP += h2("Choosing where to break")
BP += ul([
    "<strong>Just before the suspect code</strong>, not on it — so you can inspect the "
    "inputs before they are used.",
    "<strong>Inside the branch you doubt</strong>. If a breakpoint in a branch never "
    "fires, you have learned that the branch is not taken, which is often the bug.",
    "<strong>At the top of a procedure</strong> when you want to know whether it is called "
    "at all, and with what.",
])
BP += tip(
    "Stopping inside a loop that runs thousands of times gets tedious quickly. Break at "
    "the line <em>after</em> the loop and inspect the result, or put the breakpoint inside "
    "an <code>If</code> that only matches the case you care about."
)

BP += h2("Related topics")
BP += ul([
    '<a href="debugging.html">Debugging</a>',
    '<a href="watches.html">Watches and the call stack</a>',
    '<a href="view-options.html">Display and view options</a> — the margin settings.',
])

page("breakpoints", "Breakpoints", "debugging",
     "Setting, clearing and placing breakpoints, why they sometimes move to the next line, "
     "and how they survive edits and rebuilds.",
     BP,
     keywords="breakpoint toggle breakpoint f9 delete all breakpoints margin click "
              "executable line run to cursor")

# --------------------------------------------------------------------------

WA = ""
WA += p(
    "While your program is stopped, the debugger panes let you look at anything in scope. "
    "This page covers the two panes that need most explanation: the watch list and the "
    "call stack."
)

WA += h2("Watches")
WA += p(
    "A watch is an expression the debugger evaluates and displays every time the program "
    "stops. Use one for a value you want to follow across many steps, rather than hunting "
    "for it in the Locals pane each time."
)
WA += ol([
    "Click the <strong>&lt;click to add a watch&gt;</strong> row in the Watch pane.",
    "Type the expression — a variable name, a structure member, an array element.",
    "Press %s." % kbd("Enter"),
], steps=True)
WA += table(
    ["Action", "How"],
    [
        ("Edit a watch", "Click it, or press " + kbd("F2") + " on it, and type."),
        ("Delete a watch", "Click the × in the delete column at the end of the row."),
        ("Expand a structure or array", "Click its twisty."),
    ],
    key_first=True,
)
WA += note(
    "A watch that is not in scope at the current line cannot be evaluated, and the pane "
    "says so rather than showing a misleading value. Step into a procedure where it "
    "<em>is</em> in scope and it starts reporting again."
)
WA += tip(
    "Watch an expression, not just a variable — <code>rows(i).total</code> tells you far "
    "more inside a loop than <code>i</code> and <code>rows</code> separately."
)

WA += h2("Changed values")
WA += p(
    "Values that changed since the program last stopped are drawn in a distinct colour. "
    "When you are stepping through a loop this turns a wall of numbers into a short list "
    "of things that actually moved."
)
WA += p(
    "The highlight is cleared when the program resumes, so each stop shows what changed "
    "since the previous one."
)

WA += h2("The call stack")
WA += p(
    "The call stack shows how execution reached the current line: the procedure you are "
    "in, the one that called it, and so on out to your program's entry point."
)
WA += p(
    "<strong>Click a frame</strong> to look at that point in the chain. The editor jumps "
    "to the call site and the Locals pane switches to that frame's variables — which is "
    "how you answer \"what were the arguments when this was called?\""
)
WA += important(
    "Clicking a frame changes only what you are <em>looking at</em>. It does not unwind "
    "the stack or change where execution will resume — that is still the innermost frame."
)

WA += h2("Reading a stack after a crash")
WA += p(
    "When a program fails inside a library routine, the top frames are often code you did "
    "not write. Read <em>down</em> the stack to the first frame in your own source: that "
    "is the call that passed in whatever caused the failure, and it is where to look first."
)

WA += h2("Arrays and structures")
WA += ul([
    "Structures expand to their members, and members that are themselves structures "
    "expand further.",
    "Arrays report their bounds, and expand into elements.",
    "Large arrays are grouped into ranges so you can navigate to one element without "
    "scrolling past thousands of others.",
    "Dynamic arrays are read using their runtime descriptor, so their current bounds are "
    "reported rather than a guess.",
])
WA += warn(
    "A dynamic array read before its <code>ReDim</code> has executed has no valid contents "
    "yet. The debugger reports that rather than displaying whatever happens to be in "
    "memory."
)

WA += h2("Related topics")
WA += ul([
    '<a href="debugging.html">Debugging</a>',
    '<a href="breakpoints.html">Breakpoints</a>',
    '<a href="symbol-navigation.html">Symbols</a>',
])

page("watches", "Watches and the call stack", "debugging",
     "Tracking expressions across steps, reading changed-value highlighting, and using the "
     "call stack to see how execution arrived where it is.",
     WA,
     keywords="watch watches expression call stack frame locals globals changed values "
              "array structure expand dynamic array scope")
