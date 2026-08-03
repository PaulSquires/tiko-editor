'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#pragma once

''
''  COMPILE_TYPE
''  Handle information related to the currnet compile process 
''
type COMPILE_TYPE
    MainFilename           as DWSTRING   ' main source file (full path and file.ext)
    MainName               as DWSTRING   ' main source file (Name only, no extension)
    MainFolder             as DWSTRING   ' main source folder 
    ResourceFile           as DWSTRING   ' full path and file.ext to resource file (if applicable) 
    TempResourceFile       as DWSTRING   ' full path and file.ext to temporary resource file (if applicable) 
    OutputFilename         as DWSTRING   ' resulting exe/dll/lib name 
    CompilerPath           as DWSTRING   ' full path and file.ext to fbc.exe
    ObjFolder              as DWSTRING   ' *.o for all modules (set depending on 32/64 bit) (full path)
    ObjFolderShort         as DWSTRING   ' ".\" & APPEXTENSION & "\"
    ObjID                  as DWSTRING   ' "32" or "64" appended to object name
    LinkModules            as DWSTRING   ' From code embedded #LINKMODULES directive
    CompileFlags           as DWSTRING
    CompileIncludes        as DWSTRING   ' Additional user defined include paths
    wszFullCommandLine     as DWSTRING   ' Command line sent to the FB compiler
    wszFullLogFile         as DWSTRING   ' Full log file returned from the FB compiler
    wszOutputMsg           as DWSTRING   ' Additional info during compile process (time/filesize)
    RunAfterCompile        as boolean
    StartTime              as double
    EndTime                as double
    CompileID              as long       ' Type of compile (wID). Needed in case frmOutput listview later clicked on.
    bCompile32             as boolean    ' flag indicating 32-bit compile process
    bCompile64             as boolean    ' flag indicating 64-bit compile process
    pDocMain               as clsDocument ptr  ' pointer to main document to be compiler
    ObjectFilename         as DWSTRING   ' Set when compiling a Module and used afterwards to construct StatusBar message
    hCurSave               as HCURSOR    ' Cursor saved/restored during compile process
    ' The process-wide current directory, saved on entry and restored by code_Cleanup.
    ' code_Compile chdir's to the main file's folder because fbc is handed RELATIVE paths
    ' (ObjFolderShort is ".\<ext>\") that the child resolves against the directory it
    ' inherits. That chdir was never undone, so every compile permanently moved the whole
    ' process into a user project folder.
    wszCurDirSave          as DWSTRING
    wszStatusBarMessage    as DWSTRING   ' Success/Fail statusbar message
    ' Exit code of the LAST child compiler process. fbc returns 0 on success (warnings
    ' included) and non-zero on error -- measured, not assumed. It is the authority on
    ' whether a build failed; ParseLogForError's text scraping only LOCATES the error.
    ' Before this existed, a compiler that failed without printing a line the scraper
    ' recognised was reported as a successful build and the stale output was then run.
    nLastExitCode          as long
end type

type CompileThreadParams
    pCompile            as COMPILE_TYPE ptr
    wszExe              as DWSTRING
    wszCmdLine          as DWSTRING
    sConsoleText        as string 
    nExitCode           as long       ' out: the child's exit code (see COMPILE_TYPE)
    ' Signalled by the worker as its LAST act. Replaces a bCompileThreadComplete boolean
    ' that the UI thread polled in a spin loop -- a cross-thread read with no barrier, and
    ' a whole core burned for the length of every build.
    hDoneEvent          as HANDLE
end type

' Allocate/teardown of the compile state. Declared (they were only defined) so the save
' self-test can drive the real pair rather than a copy of them.
declare function code_Initialize() as COMPILE_TYPE ptr
declare function code_Cleanup( byval pCompile as COMPILE_TYPE ptr ) as boolean
declare sub compile_thread( byval userdata as any ptr )
declare function code_Compile( byval wID as long ) as boolean
declare function AddPathsToLinkModules( byval modules as DWSTRING ) as DWSTRING



