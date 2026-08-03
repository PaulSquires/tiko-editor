' ########################################################################################
' modProjectTrust.bi
'
' A .tiko file decides how the project is BUILT and RUN, not merely which files to open:
'
'   ProjectOther32 / ProjectOther64  -> appended verbatim to the fbc command line
'                                       (modCompile.inc), and fbc accepts -l, -p, -Wl and
'                                       -Wc, which reach the linker and gcc
'   ProjectCommandLine               -> ShellExecuteEx lpParameters when the build is run
'
' So opening a .tiko someone else wrote and pressing F5 runs their choices. That is the
' same threat model that made VS Code and Visual Studio add Workspace Trust, and tiko had
' no equivalent: a project file was obeyed on sight.
'
' THE RULE, chosen to be as close to invisible as possible:
'
'   1. A project this install SAVED is trusted, permanently and silently. Anything the
'      author creates or edits therefore never prompts -- which is the whole point, because
'      a prompt that fires on ordinary work is one the user learns to dismiss.
'   2. A project carrying NO build settings and NO run arguments never prompts either.
'      There is nothing in it to execute.
'   3. Everything else asks, once, and remembers the answer per path.
'
' The user can answer "open the files but discard the build settings", which is the useful
' middle: the project opens, and the parts that would run something are blanked.
'
' NOT ADDRESSED HERE, deliberately, so it is not mistaken for solved: File= entries are
' still opened without restriction, including UNC paths (which authenticate outbound to
' whoever hosts them). Constraining those is a separate change with its own trade-offs --
' project files legitimately reference shared and absolute locations.
' ########################################################################################

#pragma once

' Result of the prompt.
const PROJTRUST_OPEN        = 0   ' open, honour the build settings, remember
const PROJTRUST_OPEN_SAFE   = 1   ' open, DISCARD the build settings, remember
const PROJTRUST_CANCEL      = 2   ' do not open

' Pure: does this combination carry anything that can execute? Split out so the rule can be
' asserted without a file or a dialog -- it is the whole of what decides to interrupt.
declare function ProjectTrust_NeedsPrompt( byval wszOther32 as DWSTRING, _
                                           byval wszOther64 as DWSTRING, _
                                           byval wszCmdLine as DWSTRING ) as boolean

declare function ProjectTrust_IsTrusted( byval wszFile as DWSTRING ) as boolean
declare sub      ProjectTrust_Add( byval wszFile as DWSTRING )

' Read ONLY the three risky values out of a .tiko, without loading it. Used to decide before
' anything is mutated -- by the time the real parse has run, documents are already open.
declare function ProjectTrust_ScanFile( byval wszFile as DWSTRING, _
                                        byref wszOther32 as DWSTRING, _
                                        byref wszOther64 as DWSTRING, _
                                        byref wszCmdLine as DWSTRING ) as boolean

' The whole decision: returns one of the PROJTRUST_* values. Silent (returns OPEN) for a
' trusted path or a project with nothing to execute.
declare function ProjectTrust_Check( byval wszFile as DWSTRING ) as long

declare sub ProjectTrust_RunSelfTest()
