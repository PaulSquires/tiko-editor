''    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
''    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
''
''    This program is free software: you can redistribute it and/or modify
''    it under the terms of the GNU General Public License as published by
''    the Free Software Foundation, either version 3 of the License, or
''    (at your option) any later version.
''
''    This program is distributed in the hope that it will be useful,
''    but withOUT any WARRANTY; without even the implied warranty of
''    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
''    GNU General Public License for more details.

''
''
''   modCompileErrors.inc
''

#include once "modCompile.bi"
#include once "modCompileErrors.bi"
#include once "frmStatusBar.bi"
#include once "PsStatusBar.bi"
' This file is compiled BEFORE tiko.bas reaches the custom-control block, so the header for
' any control it calls has to be hand-included here -- the same reason modMenus.inc and
' modRoutines.inc hand-include theirs. Needed for the Output tab bar (PsSelectBar_SetCurSel).
#include once "PsSelectBar.bi"


''
''  Set the statusbar text and icon for a good/bad compile
''
function SetCompileStatusBarMessage( _
            byref wszText as wstring, _
            byval hIconCompile as long _
            ) as LRESULT
    dim as boolean bIsSpinner = iif( PsLeft(wszText, 1) = "#", true, false )
            
    dim as DWSTRING wszPanelText = wszText
    
    if bIsSpinner then
        ' Current message includes the Spinner so if a spinner is not
        ' already active then start the timer now.
        if gApp.bShowSpinner = false then
            gApp.SpinnerTimerID = 101
            SetTimer(HWND_FRMMAIN, gApp.SpinnerTimerID, 80, null)   ' 80 ms
        end if
        gApp.bShowSpinner = true
        wszPanelText = PsMid(wszText, 2)
        
    else
        ' Current message does not include the Spinner so if a spinner is
        ' already active then stop the timer now.
        if gApp.bShowSpinner = true then
            KillTimer( HWND_FRMMAIN, gApp.SpinnerTimerID )
        end if
        gApp.bShowSpinner = false
        gApp.SpinnerCurrentFrame = 0
        gApp.SpinnerTimerID = 0
        gApp.hIconPanel = 0
    end if

    gApp.hIconPanel = hIconCompile
    
    PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, COMPILE_STATUS_PANEL, wszPanelText )

    function = 0
end function


''
''  Reset all Scintilla editing cursors
''
function ResetScintillaCursors() as long
    dim as long nCount = gTTabCtl.GetItemCount 

    for i as long = 0 to nCount - 1
        if gTTabCtl.IsValidTab(i) = false then continue for
        dim as clsDocument ptr pDoc = gTTabCtl.GetpDoc(i)
        if pDoc then 
            SciExec( DocView(pDoc, 0), SCI_SETCURSOR, SC_CURSORNORMAL, 0 )
            SciExec( DocView(pDoc, 1), SCI_SETCURSOR, SC_CURSORNORMAL, 0 )
        end if
    next
    function = 0
end function


'' July 2017, attempted to capture STDERR output from a running FB compiled
'' program in order to capture runtime errors. This code works okay on Windows 10
'' but does not PRINT on Windows 7. Also, dkl posted in FB forum that FB prints
'' to STDOUT rather than STDERR. This is too bad.
''
'' Returns 0 on success, non-zero if the child could not be started. Callers currently
'' ignore it; it is honest anyway, because the old code returned 0 from BOTH the success
'' path and its own bail-out, so a caller that started checking would have been misled.
''
function RedirConsoleToFile( _
            byval wszExe as DWSTRING, _
            byval wszCmdLine as DWSTRING, _
            byref sConsoleText as string, _
            byref nExitCode as long _
            ) as long

    ' Assume failure until the child says otherwise. If it never starts, or its exit code
    ' cannot be read, "unknown" must not read as "succeeded" -- that conflation is the whole
    ' defect this parameter exists to close.
    nExitCode = -1

    ' A SPACE IN THE COMPILER PATH IS FINE, and the check that used to sit here -- which
    ' aborted the compile and told the user to reinstall tiko somewhere without spaces --
    ' was working around a misdiagnosis. Its own comment said "once I wrap the string in
    ' double quotes then the call to CreateProcessW fails", which is true and is the point:
    ' lpApplicationName must be the UNQUOTED exact path, and spaces in it have never been a
    ' problem. The quoting rule belongs to lpCommandLine's argv[0], which this function did
    ' not supply at all until BuildChildCommandLine above. So the restriction that kept tiko
    ' out of "C:\Program Files" existed to dodge a bug that was somewhere else entirely.
    '
    ' (The MinGW guidance about spaces that was quoted here is real, but it is about the
    '  gcc backend's own tooling, not about starting a process, and the default backend is
    '  gas64 now.)
    dim as DWSTRING wszFullCmd = BuildChildCommandLine( wszExe, wszCmdLine )

    ' Create the child process and read from its standard output
    dim ProcessInfo as PROCESS_INFORMATION
    dim StartupInf  as STARTUPINFO

    ' continue on with the newer CreateProcess compile approach.
    dim SecurityAttribute as SECURITY_ATTRIBUTES
    ' HANDLE, not PHANDLE. These hold handles, not pointers to handles; the old declaration
    ' worked only because both are pointer-width, and CreatePipe's parameters are
    ' HANDLE ptr, which @hChildStdOutRead satisfies either way. Naming the real type is
    ' what stops the next reader assuming an indirection that is not there.
    dim hChildStdOutRead  as HANDLE
    dim hChildStdOutWrite as HANDLE
    dim dwReadBytes       as long
    dim sBuffer as string * 4096
    
    ' Set the bInheritHandle flag so pipe handles are inherited.
    SecurityAttribute.nLength              = sizeof(SECURITY_ATTRIBUTES)
    SecurityAttribute.bInheritHandle       = true
    SecurityAttribute.lpSecurityDescriptor = null

    ' Create a pipe for the child process's STDOUT.
    if CreatePipe( @hChildStdOutRead, @hChildStdOutWrite, @SecurityAttribute, byval 0 ) = false then
        ' Fell through to "function = 0" before, i.e. reported SUCCESS after doing nothing.
        return 1
    else
        StartupInf.cb         = sizeof(STARTUPINFO)
        GetStartupInfo(@StartupInf )  
        StartupInf.hStdError  = hChildStdOutWrite   
        StartupInf.hStdOutput = hChildStdOutWrite  
        StartupInf.dwFlags    = STARTF_USESTDHANDLES or STARTF_USESHOWWINDOW
        StartupInf.wShowWindow = SW_HIDE
        
        ' lpApplicationName is the UNQUOTED exact path (quoting it is what fails);
        ' lpCommandLine carries the QUOTED path back as argv[0] followed by the switches.
        ' See BuildChildCommandLine above for why the switches cannot be passed alone.
        dim errCode as long
        errCode = CreateProcessW( _
                    wszExe,  _          ' Create the child process.
                    wszFullCmd, _       ' Command line -- argv[0] + switches
                    byval 0,     _      ' Process security attributes
                    byval 0,     _      ' Primary thread security attributes
                    true,        _      ' Handles are inherited
                    byval 0,     _      ' Creation flags  
                    byval 0,     _      ' Use parent's environment
                    byval 0,     _      ' Use parent's current directory
                    @StartupInf,  _     ' STARTUPINFO pointer
                    @ProcessInfo) 
        
        if errCode <> 0 then            

            CloseHandle( hChildStdOutWrite ) ' to avoid ReadFile hanging at the end.
            do
                if ReadFile( hChildStdOutRead, byval varptr(sBuffer), 4096, @dwReadBytes, byval 0 ) = false then 
                    '? "ERROR ReadFile', "GetLastError: "; GetLastError
                    exit do
                end if  
                sConsoleText = sConsoleText & left(sBuffer, dwReadBytes)
                '(at this stage we could SAVE the buffer that we've read from the child to a file, but 
                ' thats an extra ReadFile+WriteFile compared to the direct-to-hFile method)
            loop
            
        else
            ' The old message here blamed spaces in the path, which was never the cause.
            ' Report what the system actually said instead.
            AfxMsg( "Could not start the compiler." & vbcrlf & vbcrlf & _
                    wszExe & vbcrlf & vbcrlf & _
                    Win32ErrorText( GetLastError() ) )
            if hChildStdOutRead  then CloseHandle( hChildStdOutRead )
            if hChildStdOutWrite then CloseHandle( hChildStdOutWrite )
            return 1
        end if
            
    end if

    ' ---- the child's EXIT CODE, which nothing used to read --------------------------------
    ' Build success was decided purely by scraping the compiler's stdout (ParseLogForError),
    ' and its "good compile" flag is set merely by seeing the FreeBASIC banner -- i.e. by the
    ' compiler having STARTED. A compiler that then failed without printing a line matching
    ' the scraper's pattern was reported as a SUCCESSFUL build, and the stale output binary
    ' was run or debugged. Measured: fbc returns 0 on success (warnings included) and 1 on
    ' error, so the exit code is authoritative where the text is only a heuristic.
    '
    ' The wait is bounded rather than INFINITE. The read loop above ends when the pipe
    ' breaks, which happens once every inherited write handle is closed -- normally because
    ' the child and its descendants exited. A child that closed stdout and kept running
    ' would previously just let tiko carry on; an INFINITE wait here would turn that into a
    ' hang, so a timeout keeps the old behaviour and simply leaves the code unknown.
    if ProcessInfo.hProcess then
        if WaitForSingleObject( ProcessInfo.hProcess, 30000 ) = WAIT_OBJECT_0 then
            dim as DWORD dwCode
            if GetExitCodeProcess( ProcessInfo.hProcess, @dwCode ) then nExitCode = clng(dwCode)
        end if
    end if

    ' Close handles to the child process and its primary thread.
    ' Some applications might keep these handles to monitor the status 
    ' of the child process, for example.
    if ProcessInfo.hProcess then CloseHandle( ProcessInfo.hProcess )
    if ProcessInfo.hThread  then CloseHandle( ProcessInfo.hThread )
    if hChildStdOutRead     then CloseHandle( hChildStdOutRead )

    function = 0
end function


sub compile_thread( byval userdata as any ptr )
    dim as CompileThreadParams ptr ctp = cast(CompileThreadParams ptr, userdata)
    
    ' Create a compile process and redirect the output to a text file.
    dim as string sConsoleText = ctp->sConsoleText
    dim as long nExitCode
    RedirConsoleToFile( ctp->wszExe, ctp->wszCmdLine, sConsoleText, nExitCode )
    ctp->sConsoleText = sConsoleText
    ctp->nExitCode    = nExitCode

    ' LAST act, and it must stay last: the waiter treats this as "every field above is
    ' written". (It then also joins the thread before reading them, because ctp is one of
    ' its own locals.)
    if ctp->hDoneEvent then SetEvent( ctp->hDoneEvent )
end sub


''
''
function RunEXE( _
            byval pDocMain as clsDocument ptr, _
            byref wszFileExe as DWSTRING, _
            byref wszParam as DWSTRING _
            ) as long
                            
    dim as DWSTRING wszPath = PsPathDirWithSep(wszFileExe)
    dim as DWSTRING wszBatchFile = RUNBATCHFILE
    
    ' if option is specified to launch command window prior to running the
    ' compiled program then we do so by executing a shell to a batch file.
    ' This allows being able to see any runtime errors that might be thrown.
    ' Otherwise, simple shell to the program.
    if gConfig.RunViaCommandWindow then
        PsFileDelete( wszBatchFile )
        ' One buffer, one write -- same shape as clsConfig and modKeyBindings.
        '
        ' ANSI WITH NO BOM, WHICH MATTERS MORE HERE THAN ELSEWHERE: cmd.exe reads a
        ' .bat in the console codepage and a BOM at the top is executed as a command,
        ' which fails with a line noise error before anything runs. That is what
        ' CTextStream.Create wrote without its bUnicode flag, and it is what
        ' TextFile_WriteAnsi writes.
        dim as DWSTRING wszBat
        wszBat &= "@echo off" & vbcrlf
        wszBat &= PsLeft(wszPath, 2) & vbcrlf                            ' correct drive
        wszBat &= "cd " & chr(34) & wszPath & chr(34) & vbcrlf           ' correct folder
        wszBat &= chr(34) & wszFileExe & chr(34) & " " & wszParam & vbcrlf
        wszBat &= "pause" & vbcrlf
        TextFile_WriteAnsi( wszBatchFile, wszBat )
        wszFileExe = wszBatchFile
        wszParam = ""
    end if
    

    dim ShExecInfo as SHELLEXECUTEINFOW  

    ' Run the EXE
    with ShExecInfo
        .cbSize       = len(SHELLEXECUTEINFOW)
        .fMask        = SEE_MASK_NOCLOSEPROCESS
        .hwnd         = 0
        .lpVerb       = null
        .lpFile       = wszFileExe
        .lpParameters = wszParam   
        .lpDirectory  = 0
        .nShow        = SW_SHOW
        .hInstApp     = 0 
    end with
    ShellExecuteEx(@ShExecInfo)

    function = 0
end function


' ========================================================================================
' Set the cursor to the error position based on the selected line in:
' (1) Compiler Results
' (2) TODO
' It is shared purely by HWND parameter -- it takes any PsListTree whose column 1 is a
' 1-based line number and column 2 a filename, which every pane frmOutput_CreateListView
' builds satisfies. Do not specialize it: it also has two direct callers.
' ========================================================================================
function SetDocumentErrorPosition ( byval hLV as HWND ) as long

    dim wszErrorLine as wstring * MAX_PATH
    dim wszErrorFile as wstring * MAX_PATH
    
    dim as long nCurSel = PsListTree_GetCurSel(hLV)
    if nCurSel < 0 then return 0 
    
    dim pDoc as clsDocument ptr 

    ' Get the line number and filename of the selected item
    wszErrorLine = PsListTree_GetCellText(hLV, nCurSel, 1)
    wszErrorFile = PsListTree_GetCellText(hLV, nCurSel, 2)
    dim as long nLineNum = val(wszErrorLine) - 1   ' because visually the editor is one based line numbers

    ' if we are doing a QuickRun then it must be on the currently loaded
    ' and active file in the editor, therefore simply use that pDoc.
    if gApp.IsQuickRun then
        pDoc = gTTabCtl.GetActiveDocumentPtr()
        if pDoc then wszErrorFile = pDoc->DiskFilename 
    end if
    
    pDoc = OpenSelectedDocument( wszErrorFile, "", nLineNum )
    
    function = 0
end function


''
''
function SetLogFileTextbox( byval pCompile as COMPILE_TYPE ptr ) as long
    
    ' Construct the text that will be displayed:
    ' - FBC version, exe name, file size, compile time
    ' - raw log file output
    ' - compile command line used
    
    if PsLen(pCompile->wszOutputMsg) then pCompile->wszOutputMsg += vbcrlf
    
    dim as DWSTRING wszText = _
        pCompile->wszOutputMsg + _
        L(142,"Command Line") + ": " + vbcrlf + _
        pCompile->wszFullCommandLine + vbcrlf + vbcrlf + _
        pCompile->wszFullLogFile 
                
    AfxSetWindowText( GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), wszText )

    ' Reset the type parameters so subsequent compiles will not append
    function = 0
end function


''
''
function ParseLogForError ( _
            byval pCompile as COMPILE_TYPE ptr, _
            byref wsLogSt as DWSTRING, _
            byval bAllowSuccessMessage as boolean, _
            byval fCompilingObjFiles as boolean _
            ) as boolean
    
    ' returns true if error has been detected. This signals to the calling code
    ' that we can break out of performing any further compiles (eg. modules).
    dim wszTemp as DWSTRING

    dim as long Parenthesis_Start, Parenthesis_End, Error_Start       
    dim as long i, NumLines, nextLine, r, nCount, nListViewLine
    dim as long NumWarnings, NumErrors, NumDirectives, IsError, IsWarning
    dim as long nFirstErrorLine = -1
    dim as HWND hLV
    dim as DWSTRING wDQ, wst1, wst2, wst3, wst, wst_ucase, wszOutputMsg, wszText        
    dim as boolean fGoodCompile

    ' Force switching to "Compiler Log File" tab after a failed compile. We do this in situations
    ' where no Filename/ErrorLine is populated in the "Compiler Results" listbox. This happens
    ' in situations like failed Resource File compiling, failed linking.
    dim as boolean fSwitchToLogFile   

    wDQ = wchr(34) ' unicode double quotes

    pCompile->EndTime = timer

    hLV = HWND_FRMOUTPUT_LVRESULTS
    PsListTree_Clear( hLV )
    
    ' On some Windows systems with tight security policies, the CreateChildProcess used
    ' by the sub RedirConsoleToFile will fail. This seems to happen mostly when trying
    ' to compile using the FB 64 bit compiler on a 32-bit version of Windows. for this
    ' reason we will do a manual check here rather than relying on detecting the error
    ' through the log file string.
    ' Updated 2025: Tiko is a pure 64-bit program so it won't run on 32-bit Windows, therefore
    ' we should never get the following error anyway like used to happen on WinFBE.
    if pCompile->bCompile64 then   ' trying to compile a 64-bit program
        if AfxWindowsBitness() <> 64 then  ' Windows is not 64 bit
            wsLogSt = "This version of the FreeBASIC compiler is not compatible with the version "   & _
                      "of Windows you're running. Check your computer's system information to see "  & _
                      "whether you need a x86 (32-bit) or x64 (64-bit) version of the program, and " & _
                      "then contact the software publisher."  
            pCompile->wszFullLogFile = wsLogSt
        end if
    end if

    dim as boolean bReadingCompileOutput = false
    
    ' Parse the log string.
    NumLines = PsStrParseCount( wsLogSt, vbCrLf)
    
    for nextLine = 1 to NumLines                   
    
        wst = PsTrim(PsStrParse(wsLogSt, nextLine, vbCrLf))                 
        wst_ucase = PsUCase(wst)
        
        ' Deal with the situation where we might be trying to compile a 64-bit application
        ' from a 32-bit operating system.
        if PsLeft(wst_ucase, 16) = "THIS VERSION OF " then
            fGoodCompile = false
            fSwitchToLogFile = true
            wszText = "" & chr(9) & "0" & chr(9) & "" & chr(9) & "compiling FAILED: Windows not 64-bit (refer to log file)"
            ListBox_AddTabbedRow( hLV, wszText )
            wszOutputMsg = wsLogSt: exit For
            exit for
        end if   

        ' Save the FB version and copyright information. 
        if PsLeft(wst_ucase, 19) = "FREEBASIC COMPILER " then
            fGoodCompile = true
            wszOutputMsg = wszOutputMsg & wst & vbCrLf: continue For
        end if        
        if PsLeft(wst_ucase, 13) = "COPYRIGHT (C)" then
            fGoodCompile = true
            wszOutputMsg = wszOutputMsg & wst & vbCrLf: continue For
        end if        
        
        ' Check to see if an error occurred in compiling a resource script.
        ' if there was a bad source name passed to the compiler (for 
        ' example, missing .rc file), then the output at the end of the
        ' log file is like the following:
        '
        ' Error!
        ' Could Not Open source file (p.RC)
        ' OBJ file Not made
        ' compiling resource FAILED: Error Code 1
        '
        ' Very sorry, but GoRC had a problem.
                                            
        ' Check to see if linking failed
        if (PsLeft(wst_ucase, 6) = "ERROR!") or _
            (left(wst_ucase, 34) = "VERY SORRY, BUT GORC HAD A PROBLEM") then
            wszText = "" & chr(9) & "0" & chr(9) & "" & chr(9) & "compiling resource FAILED: Error Code 1 (refer to log file)"
            ListBox_AddTabbedRow( hLV, wszText )
            fSwitchToLogFile = true
            NumErrors += 1
        elseif (PsLeft(wst_ucase, 5) = "ERROR") then
            wszText = "" & chr(9) & "0" & chr(9) & "" & chr(9) & "compiling FAILED: Error Code 1 (refer to log file)"
            ListBox_AddTabbedRow( hLV, wszText )
            fSwitchToLogFile = true
            NumErrors += 1
        elseif (PsLeft(wst_ucase, 19) = "COMPILING C FAILED:") then
            wszText = "" & chr(9) & "0" & chr(9) & "" & chr(9) & "compiling C FAILED: Error Code 1 (refer to log file)"
            ListBox_AddTabbedRow( hLV, wszText )
            fSwitchToLogFile = true
            NumErrors += 1
        end if
        
        if instr(wst_ucase, "LINKING FAILED:") then 
            wszText = "" & chr(9) & "0" & chr(9) & "" & chr(9) & wst & " (refer to log file)"
            ListBox_AddTabbedRow( hLV, wszText )
            fSwitchToLogFile = true
            NumErrors += 1
        end if
    
        ' Check for the "linking: " line because that line identifies the actual output filename being created
        ' If creating just a single compiled Module then obviously there is no linking step so this deeper dive
        ' to find the true OutputFilename never happens and will still be set to the default for the project
        ' which is most likely an EXE based on the Main file name.
        if PsLeft(wst_ucase, 9) = "LINKING: " then 
            i = PsInStr(wst, " -o ")
            if i then
                wszTemp = PsMid(wst, i + 4)
                i = PsInStr(wszTemp, wDQ & " ")
                if i then pCompile->OutputFilename = PsStrRemove(PsLeft(wszTemp, i), wDQ)
                ' if pCompile->OutputFilename is a relative filename then convert it to full filename.
                pCompile->OutputFilename = PsStrReplace( pCompile->OutputFilename, "/", "\" )
                if PsPathIsRelative( pCompile->OutputFilename ) then 
                    pCompile->OutputFilename = PsPathJoin( pCompile->MainFolder, pCompile->OutputFilename )
                end if
            end if      
        end if
        
        ' Determine if we are reading any lines in the log file that appear after the "compiling: " line
        ' but before the "assembling: ". Anything there is data output by #print preprocessor statements.
        if PsLeft(wst_ucase, 11) = "COMPILING: " then 
            bReadingCompileOutput = true
            continue for
        elseif PsLeft(wst_ucase, 13) = "COMPILING C: " then 
            bReadingCompileOutput = true
            continue for
        elseif PsLeft(wst_ucase, 12) = "ASSEMBLING: " then 
            bReadingCompileOutput = false
        elseif PsLeft(wst_ucase, 14) = "COMPILING RC: " then 
            bReadingCompileOutput = false
        elseif PsLeft(wst_ucase, 9) = "LINKING: " then 
            bReadingCompileOutput = false
        end if

        if bReadingCompileOutput then

            if PsLen(wst) = 0 then continue for

            ' Check for any compiler warnings
            Error_Start = 0
            IsWarning = instr(wst_ucase, ") WARNING ")
            if IsWarning then Error_Start = IsWarning
            IsError   = instr(wst_ucase, ") ERROR ") 
            if IsError then Error_Start = IsError
            
            if (Error_Start > 0) then 
                'sample warning message
                'c:\freebasic\test.bas(1394) warning 1(1): Passing scalar as pointer, at parameter 2 (hwndOldFocus) of ONSETFOCUS()
                'sample error message
                'c:\freebasic\test.bas(17) error 41: Variable not declared, kjljjada in 'kjljjada Error'

                ' Determine the line number. Error_Start variable holds the position of the closing
                ' parenthesis of the line number. We simply need to reverse search from there for the
                ' opening parenthesis.
                ' 2018-08-29: Code updated to handle case of embedded parenthesis in file name
                Parenthesis_end   = Error_Start
                Parenthesis_Start = PsInStrRev(wst, "(", Parenthesis_End)
                
                if (Parenthesis_Start < Parenthesis_End) andalso _
                    (Parenthesis_end <= Error_Start) then
            
                    wst1 = PsLeft(wst, Parenthesis_Start-1)   ' filename
                    wst2 = PsMid( wst, Parenthesis_Start + 1, Parenthesis_end - Parenthesis_Start - 1)  ' line# 
                    wst3 = PsMid( wst, Error_Start + 1)  ' error message

                    wszText = "" & chr(9) & wst2 & chr(9) & wst1 & chr(9) & wst3
                    ListBox_AddTabbedRow( hLV, wszText )
                    fSwitchToLogFile = false

                    if IsWarning then 
                        NumWarnings += 1
                    elseif IsError then
                        if nFirstErrorLine = -1 then
                            nFirstErrorLine = NumWarnings + NumErrors
                        end if
                        NumErrors += 1
                    end if            
                end if

            else
'                wszText = "" & chr(9) & "" & chr(9) & "" & chr(9) & wst
'                ListBox_AddTabbedRow( hLV, wszText )
                NumDirectives += 1
            end if

        end if
    
        nListViewLine = NumWarnings + NumErrors + NumDirectives
    next
    
    ' if there were no errors but the fGoodCompile flag was not set to true then the log file
    ' did not contain the required FB copyright notice. Something must have went wrong, like trying
    ' to call the 64 bit compiler using a 32 bit operating system. Show the message to the user via
    ' the Output window (log file).
    if fGoodCompile = false then NumErrors += 1

    ' ---- THE EXIT CODE IS THE AUTHORITY -------------------------------------------------
    ' Everything above is a heuristic over the compiler's stdout, and fGoodCompile in
    ' particular is set by seeing the FreeBASIC banner -- it means "the compiler ran", not
    ' "the build worked". So a failure whose diagnostic does not match any pattern here
    ' scored zero errors and the build was reported GOOD, after which the stale output was
    ' run or handed to the debugger.
    '
    ' A non-zero exit with no error line found is exactly that case: report it, and send the
    ' user to the log file, which is the only place the real reason exists. -1 means the code
    ' could not be read at all (the child never started, or outlived the bounded wait).
    if (pCompile->nLastExitCode <> 0) andalso (NumErrors = 0) then
        NumErrors += 1
        fSwitchToLogFile = true
        dim as DWSTRING wszWhy = iif( pCompile->nLastExitCode = -1, _
                L(510, "the compiler did not report an exit code"), _
                L(511, "compiler exit code") & " " & str(pCompile->nLastExitCode) )
        ListBox_AddTabbedRow( hLV, "" & chr(9) & "0" & chr(9) & "" & chr(9) & _
                L(512, "Compile FAILED") & " (" & wszWhy & ") -- " & _
                L(513, "see the Compiler Log File tab") )
    end if
    
    if (fSwitchToLogFile = false) andalso (NumDirectives > 0) then
        fSwitchToLogFile = true
    end if


    dim as DWSTRING wszIconCompileMsg

    ' if the EXE/DLL/Object output file was never created then throw an error
    if fCompilingObjFiles = false then
        if (NumWarnings = 0) andalso (NumErrors = 0) then
            if PsFileSize(pCompile->OutputFilename) = 0 then 
                NumErrors += 1
            end if
        end if
    end if
    
    ' In all cases where warnings and/or errors exist, we will show the Compiler Results listview
    if (NumWarnings > 0) orelse (NumErrors > 0) orelse (NumDirectives > 0) then
    
        wszIconCompileMsg = _
                L(155, "Errors")   & " " & str(NumErrors)   & "  " & _
                L(154, "Warnings") & " " & str(NumWarnings) & _
                "  [" & _
                PsLocalDateStr() & "  " & _
                PsLocalTimeStr() & _
                "]"
        pCompile->wszOutputMsg = L(188,"Failed Compile")  & " (" & wszIconCompileMsg & ")" & vbcrlf
        pCompile->wszStatusBarMessage = wszIconCompileMsg
        SetCompileStatusBarMessage( wszIconCompileMsg, ghIconBad )
       
        ' only beep on errors
        if NumErrors > 0 then MessageBeep(MB_ICONWARNING)
        
        ' Position the Compiler Log to the first error/warning
        PsListTree_SetCurSel( hLV, 0 )
        
        ' if fGoodCompile is false then something unusual occurred so better show the log file by default
        if fGoodCompile = false then
            frmOutput_PositionWindows()
        end if
      
        ' Reposition to the correct tab. SetCurTab is silent, so PositionWindows below
        ' (which ends in ShowHideOutputControls) is what swaps the visible pane.
        frmOutput_SetCurTab( iif( fSwitchToLogFile, OUTPUT_TAB_LOGFILE, OUTPUT_TAB_RESULTS ) )
        frmOutput_PositionWindows()
        
        ' Only retore the Output window if Errors exist. Warnings only will not trigger
        ' the Output window to open but the user can see how many warnings exist via the
        ' statusbar results message and then they can manually open the Output window
        ' to view the warning message (simply clicking on the statusbar message should
        ' do that).
        if NumErrors > 0 then frmOutput_RestorePanel()
        
        ' Set to error line position only after all windows have been shown and resized
        SetDocumentErrorPosition( hLV )
    end if

    if (NumErrors = 0) andalso (bAllowSuccessMessage = true) then 
        ' we are not compiling modules
        dim as long nFileSize = PsFileSize(pCompile->OutputFilename)
        wszIconCompileMsg = _
                L(155, "Errors") & " " & str(NumErrors) & "  " & _
                L(154, "Warnings") & " " & str(NumWarnings) & _
                "  [ " & PsPathName(pCompile->OutputFilename) & "  " & _
                rtrim(AfxStrFormatKBSize(nFileSize)) & " (" & _
                wstr(nFileSize) & " " & L(160,"bytes") & ") ]"
        pCompile->wszStatusBarMessage = wszIconCompileMsg         
        SetCompileStatusBarMessage( wszIconCompileMsg, ghIconGood )

        if gConfig.DisableCompileBeep = false then MessageBeep(MB_OK)
       
'        dim as double ElapsedTime = (pCompile->EndTime-pCompile->StartTime) * 1000
'        dim as DWSTRING wszElapsedTime 
'        dim as DWSTRING wszTimeUnits = L(159,"seconds")
'        if ElapsedTime < 1000 then  
'            ' less than a second so display milliseconds
'            wszElapsedTime = format(ElapsedTime, "0.000")
'            'wszTimeUnits = L(160,"milliseconds")
'        else
'            ' over 1000 milliseconds so display using seconds
'            wszElapsedTime = format(ElapsedTime / 1000, "0.0")
'        end if
        
        dim as double ElapsedTime = (pCompile->EndTime-pCompile->StartTime) 
        dim as DWSTRING wszElapsedTime = _
            iif( ElapsedTime > 1, format(ElapsedTime, "0.0"), format(ElapsedTime, "0.000") )
        dim as DWSTRING wszTimeUnits = L(159,"seconds")

        pCompile->wszOutputMsg = _   ' this gets output to the Output window
            L(187,"Successful Compile")  & " (" & _
            L(155,"Errors")              & " " & str(NumErrors) & " " & _
            L(154,"Warnings")            & " " & str(NumWarnings) & ")" & vbcrlf & vbcrlf & _
            L(156,"Primary Source:")     & " " & pCompile->MainFilename & vbCrLf & _
            L(157,"Target Compilation:") & " " & pCompile->OutputFilename & " (" & _
                                    rtrim(AfxStrFormatKBSize(nFileSize)) & ", " & _
                                    str(nFileSize) & " " & L(160,"bytes") & ")" & vbcrlf & _
            L(158,"Compile Time:")       & " " & wszElapsedTime & _
                                    " " & wszTimeUnits & " (" & _
                                    PsLocalDateStr() & " " & _
                                    PsLocalTimeStr() & ")" & vbcrlf
    end if


    ' Update the log file text box in the frmOutput window
    SetLogFileTextbox( pCompile )

    ' if Errors have occurred then return true. We will allow processing
    ' to continue if only Warnings occurred.
    ' There were errors then do not allow running the EXE (warnings are okay)
    if NumErrors > 0 then 
        pCompile->RunAfterCompile = false 
        return true
    end if

    
    function = false
end function




' ########################################################################################
' Env-gated: TIKO_COMPILECMD_SELFTEST=1
'
' Asserts the ONE rule that governs BuildChildCommandLine: the child discards the first
' token of lpCommandLine, so token 0 must be the program name and never a switch.
'
' This exists because the bug it guards against was completely silent. tiko passed the bare
' switches, the child ate "-m", and the only symptom was a compiler flag quietly not
' applying -- no warning, no error, nothing for an assertion to catch, because there was no
' assertion and no suite for this file at all.
'
' A self-test cannot launch a process, so what is asserted is the STRING. That is enough
' precisely because the OS behaviour it encodes was verified separately with a harness that
' does launch one (results quoted in BuildChildCommandLine's header).
' ########################################################################################

' Split a command line the way the child's startup code does: whitespace separated, double
' quotes group. Independent of the composer, so a shared bug cannot hide behind both.
private sub CompileCmd_Tokenize( byval wszCmd as DWSTRING, tokens() as DWSTRING, byref nCount as long )
    nCount = 0
    dim as string s = wszCmd
    dim as long i = 1, n = len(s)
    dim as boolean bInQuote
    dim as string cur
    dim as boolean bHave

    while i <= n
        dim as string c = mid(s, i, 1)
        if c = chr(34) then
            bInQuote = not bInQuote
            bHave = true
        elseif (c = " ") andalso (bInQuote = false) then
            if bHave then
                tokens(nCount) = cur : nCount += 1 : cur = "" : bHave = false
            end if
        else
            cur &= c : bHave = true
        end if
        i += 1
    wend
    if bHave then tokens(nCount) = cur : nCount += 1
end sub


sub CompileCmd_RunSelfTest()

    if environ("TIKO_COMPILECMD_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro CMDASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
            dim as DWSTRING wszMsg = msg
            sMsg = "  FAIL: " & wszMsg.Utf8
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_COMPILECMD_SELFTEST ---"

    dim as DWSTRING tok(0 to 63)
    dim as long nTok

    ' ---- 1. the rule itself: token 0 is the exe, never a switch -------------------------
    dim as DWSTRING wszExe = "C:\fb\fbc64.exe"
    dim as DWSTRING wszCmd = BuildChildCommandLine( wszExe, "-m ""prog"" mod1.o -v -w all" )
    CompileCmd_Tokenize( wszCmd, tok(), nTok )
    CMDASSERT( nTok = 7, "expected 7 tokens, got " & nTok )
    CMDASSERT( tok(0) = wszExe, "token 0 is not the exe: [" & tok(0) & "]" )
    CMDASSERT( PsLeft(tok(0), 1) <> "-", "token 0 is a SWITCH -- the child will eat it" )
    ' Everything the caller supplied must survive, in order, one slot later.
    CMDASSERT( tok(1) = "-m",     "the -m flag was lost (this was the shipped bug)" )
    CMDASSERT( tok(2) = "prog",   "main module name wrong: [" & tok(2) & "]" )
    CMDASSERT( tok(3) = "mod1.o", "object file lost" )
    CMDASSERT( tok(6) = "all",    "trailing argument lost" )

    ' ---- 2. the module path, whose leading space used to be load-bearing ----------------
    ' code_CompileModules space-pads CompileFlags. That accident is what kept its first
    ' switch alive. It must no longer matter -- assert BOTH spellings behave identically.
    dim as DWSTRING wszPadded   = "  -w all -O2  -c -b ""x.bas"" -v -o ""x.o"""
    dim as DWSTRING wszTrimmed  = PsTrim( wszPadded )
    dim as DWSTRING tok2(0 to 63) : dim as long nTok2

    CompileCmd_Tokenize( BuildChildCommandLine( wszExe, wszPadded ),  tok(),  nTok )
    CompileCmd_Tokenize( BuildChildCommandLine( wszExe, wszTrimmed ), tok2(), nTok2 )
    CMDASSERT( nTok = nTok2, "padded and trimmed flags give different token counts (" & _
                             nTok & " vs " & nTok2 & ") -- the leading space is still load-bearing" )
    dim as boolean bSame = (nTok = nTok2)
    if bSame then
        for i as long = 0 to nTok - 1
            if tok(i) <> tok2(i) then bSame = false : exit for
        next
    end if
    CMDASSERT( bSame, "trimming the flags changes the child's argv -- the old silent-flag-loss bug" )
    CMDASSERT( tok(1) = "-w", "first user switch is not argv[1]: [" & tok(1) & "]" )

    ' ---- 3. a space in the compiler path must NOT split ---------------------------------
    ' This is what the deleted "install without spaces" check was really about: an unquoted
    ' argv[0] would break into two tokens and swallow the first real switch instead.
    dim as DWSTRING wszSpaced = "C:\Program Files\tiko\fbc64.exe"
    CompileCmd_Tokenize( BuildChildCommandLine( wszSpaced, "-c -v" ), tok(), nTok )
    CMDASSERT( nTok = 3, "a path with a space produced " & nTok & " tokens, expected 3" )
    CMDASSERT( tok(0) = wszSpaced, "spaced exe path split: [" & tok(0) & "]" )
    CMDASSERT( tok(1) = "-c", "first switch lost to a spaced exe path: [" & tok(1) & "]" )

    ' ---- 4. the -m pair -----------------------------------------------------------------
    ' These four assertions exist because of a REAL LINK FAILURE, not a hypothetical: once
    ' argv[0] was supplied and -m started working, the call site was still handing it a full
    ' path WITH extension and never passing the main source as an input. fbc marked no main
    ' module and compiled no entry point -- "undefined reference to `WinMain'". Reproduced on
    ' a real two-module project, and each form below was built and linked to confirm it.
    dim as DWSTRING wszMainArgs = BuildMainModuleArgs( "mainprog", "C:\proj\mainprog.bas" )
    CompileCmd_Tokenize( BuildChildCommandLine( wszExe, wszMainArgs & " obj\helper.o" ), tok(), nTok )
    CMDASSERT( nTok = 5, "-m pair gave " & nTok & " tokens, expected 5" )
    CMDASSERT( tok(1) = "-m", "-m is not the first switch" )
    ' The -m ARGUMENT must be a bare module name. A path or an extension matches no module,
    ' which is the exact shape that failed to link.
    ' FB does not process escapes in an unprefixed literal, so "\" IS one backslash --
    ' writing "\\" here would search for two and pass on every real path.
    CMDASSERT( PsInStr( tok(2), "\" ) = 0, "-m argument carries a PATH: [" & tok(2) & "]" )
    CMDASSERT( PsInStr( tok(2), "." ) = 0,  "-m argument carries an EXTENSION: [" & tok(2) & "]" )
    ' ...and the main source must ALSO be present as an input, or nothing compiles the
    ' entry point. -m only marks; it does not add.
    CMDASSERT( tok(3) = "C:\proj\mainprog.bas", _
               "main source is not an input file: [" & tok(3) & "]" )
    CMDASSERT( PsRight( tok(3), 4 ) = ".bas", "main input lost its extension" )

    ' ---- 5. no parameters at all --------------------------------------------------------
    CompileCmd_Tokenize( BuildChildCommandLine( wszExe, "" ), tok(), nTok )
    CMDASSERT( nTok = 1, "empty params gave " & nTok & " tokens, expected 1" )
    CMDASSERT( tok(0) = wszExe, "empty params lost the exe" )

    ' ---- 6. THE EXIT CODE, against a REAL child process ---------------------------------
    ' Build success used to be decided purely by scraping stdout, and the "good compile"
    ' flag that drove it is set merely by seeing the FreeBASIC banner -- i.e. by the compiler
    ' having STARTED. So these assertions are the ones that matter: they run an actual
    ' process with a known exit status and check it is reported.
    '
    ' cmd.exe by FULL PATH, because RedirConsoleToFile passes lpApplicationName, which does
    ' NOT search PATH -- a bare "cmd.exe" would fail to launch and prove nothing.
    dim as DWSTRING wszCmdExe = environ("COMSPEC")
    if PsLen( wszCmdExe ) = 0 then wszCmdExe = "C:\Windows\System32\cmd.exe"

    if PsFileExists( wszCmdExe ) = false then
        ? "  NOTE: no COMSPEC; the exit-code assertions did NOT run."
    else
        dim as string sOut
        dim as long nRc, nExit

        nRc = RedirConsoleToFile( wszCmdExe, "/c exit 0", sOut, nExit )
        CMDASSERT( nRc = 0, "a child that started was reported as a launch failure" )
        CMDASSERT( nExit = 0, "a successful child reported exit code " & nExit )

        sOut = ""
        nRc = RedirConsoleToFile( wszCmdExe, "/c exit 7", sOut, nExit )
        CMDASSERT( nExit = 7, "a child that exited 7 reported " & nExit & _
                              " -- a failing compiler would read as SUCCESS" )

        ' stdout must still be captured; the wait was added after the read loop, and putting
        ' it before would deadlock on a child that fills the pipe buffer.
        sOut = ""
        RedirConsoleToFile( wszCmdExe, "/c echo tiko_marker", sOut, nExit )
        CMDASSERT( instr( sOut, "tiko_marker" ) > 0, _
                   "stdout was lost: [" & left(sOut, 40) & "]" )
        CMDASSERT( nExit = 0, "echo reported exit code " & nExit )
    end if
    ' NOT asserted here: the launch-failure path (nExitCode = -1). It raises a modal AfxMsg,
    ' which would hang an unattended run. Its "unknown is not success" behaviour is instead
    ' guaranteed structurally -- nExitCode is initialised to -1 before anything can fail.

    ' ---- 7. one build at a time ---------------------------------------------------------
    ' code_Compile pumps messages while it waits for its worker, so F5 is live throughout a
    ' build. Without a guard a second code_Compile ran on top of the first -- two
    ' COMPILE_TYPEs, two threads, racing on chdir and the object folder.
    '
    ' The second assertion is the sensitive one. code_Cleanup calls BuildService_End, so if
    ' the guard were missing, code_Compile would fall through, reach cleanup on some later
    ' failure, and leave the state IDLE. Finding it still RUNNING is what proves the function
    ' returned at the guard rather than somewhere further in.
    scope
        dim as boolean bWasRunning = BuildService_IsRunning()
        if bWasRunning = false then BuildService_Begin()
        dim as boolean bRc = code_Compile( IDM_COMPILE )
        CMDASSERT( bRc = false, "code_Compile started a second build while one was running" )
        CMDASSERT( BuildService_IsRunning() = true, _
                   "code_Compile ran past its guard (BuildService_End was reached by cleanup)" )
        ' Begin is a test-and-set: claiming an already-claimed slot must fail.
        CMDASSERT( BuildService_Begin() = false, _
                   "BuildService_Begin handed out a second claim while a build was running" )
        if bWasRunning = false then BuildService_End()
        CMDASSERT( BuildService_IsRunning() = false, "BuildService_End did not release the slot" )
    end scope

    sMsg = "TIKO_COMPILECMD_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
