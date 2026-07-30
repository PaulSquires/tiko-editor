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

#include once "frmDebug.bi"


' The debugger toolbar's tooltip. A PsTooltip rather than the comctl32 tip this window used to
' build by hand: it is themed, and it adds no pump obligation.
'
' The TOOL INDEX is the gDbgBtns() subscript directly -- no separate id enum is needed here,
' unlike the top-tab bars, because the hot regions already live in an array. -1 is "no button".
dim shared ghDebugTip as HWND


' ========================================================================================
' Supply the tooltip text for the button the cursor is over.
' ========================================================================================
' Runs once per hover, immediately before the tip is sized, which is why the theme push lives
' here (see modThemeApply.bi).
'
' getMenuText packs the caption and the shortcut into one string separated by a tab; the tab
' becomes " (" and a ")" closes it, exactly as the old WM_MOUSEHOVER arm did. A button with no
' shortcut still gets the parentheses -- unchanged behaviour, not an oversight of this refactor.
private function frmDebug_TipText( byval hTip as HWND, _
                                   byval hAttached as HWND, _
                                   byval idx as long ) as DWSTRING
    Theme_ApplyTooltip( hTip )
    if idx < lbound(gDbgBtns) orelse idx > ubound(gDbgBtns) then return ""
    dim as DWSTRING wszTip = gDbgBtns(idx).wszToolTip
    if len(wszTip) = 0 then return ""
    return AfxStrReplace( wszTip, wchr(9), " (" ) & ")"
end function


' ========================================================================================
' Which toolbar button is the cursor over? -1 for none.
' ========================================================================================
' Deliberately tests EVERY button, including ones with no caption -- that is what the old
' hover arm did. The hot-highlight loop in WM_MOUSEMOVE is the one that skips them.
private function frmDebug_TipHitTest( byval hwnd as HWND ) as long
    for i as long = lbound(gDbgBtns) to ubound(gDbgBtns)
        if isMouseOverRECT( hwnd, gDbgBtns(i).rc ) then return i
    next
    return -1
end function


' Enqueue - add message to the end of the queue
sub gdb_enqueue_message( byref response as string )
    dim newNode as GDBMessage ptr = new GDBMessage
    newNode->message = response
    newNode->pNextNode = 0
    
    if gdb_session.tail = 0 then
        ' Queue is empty
        gdb_session.head = newNode
        gdb_session.tail = newNode
    else
        ' Add to end
        gdb_session.tail->pNextNode = newNode
        gdb_session.tail = newNode
    end if
    
    gdb_session.count += 1
end sub

' Dequeue - remove and return message from front of queue
function gdb_dequeue_message() as string
    if gdb_session.head = 0 then
        return ""  ' Queue is empty
    end if
    
    dim tempNode as GDBMessage ptr = gdb_session.head
    dim msg as string = tempNode->message
    
    gdb_session.head = gdb_session.head->pNextNode
    if gdb_session.head = 0 then
        gdb_session.tail = 0  ' Queue is now empty
    end if
    
    delete tempNode
    gdb_session.count -= 1
    
    return msg
end function

' Check if queue is empty
function gdb_isMessageQueueEmpty() as boolean
    dim as boolean isEmpty
    MutexLock( gdb_session.hThreadMutex )
    isEmpty = (gdb_session.head = 0)
    MutexUnLock( gdb_session.hThreadMutex )
    return isEmpty
end function

' Clear all messages from queue
sub clearQueue()
    while gdb_isMessageQueueEmpty() = false
        gdb_dequeue_message()
    wend
end sub

' Initialize GDB session
function gdb_init( byval executable as string = "", byval cmdparams as string = "" ) as boolean
    dim as SECURITY_ATTRIBUTES sa
    dim as HANDLE hStdInRead, hStdInWrite
    dim as HANDLE hStdOutRead, hStdOutWrite
    dim as HANDLE hStdErrRead, hStdErrWrite
    dim as STARTUPINFO si
    dim as PROCESS_INFORMATION pi
    dim as string cmdline
    dim as integer ret
    
    ' Set up security attributes for pipe inheritance
    sa.nLength = sizeof(SECURITY_ATTRIBUTES)
    sa.bInheritHandle = TRUE
    sa.lpSecurityDescriptor = NULL
    
    ' Create pipes for stdin
    if CreatePipe( @hStdInRead, @hStdInWrite, @sa, 0 ) = 0 then return false
    if SetHandleInformation( hStdInWrite, HANDLE_FLAG_INHERIT, 0 ) = 0 then return false
    
    ' Create pipes for stdout
    if CreatePipe( @hStdOutRead, @hStdOutWrite, @sa, 0 ) = 0 then return false
    if SetHandleInformation( hStdOutRead, HANDLE_FLAG_INHERIT, 0 ) = 0 then return false
    
    ' Create pipes for stderr
    if CreatePipe( @hStdErrRead, @hStdErrWrite, @sa, 0 ) = 0 then return false
    if SetHandleInformation( hStdErrRead, HANDLE_FLAG_INHERIT, 0 ) = 0 then return false
    
    ' Set up startup info
    si.cb = sizeof(STARTUPINFO)
    si.hStdError   = hStdErrWrite
    si.hStdOutput  = hStdOutWrite
    si.hStdInput   = hStdInRead
    si.dwFlags     = STARTF_USESTDHANDLES or STARTF_USESHOWWINDOW
    si.wShowWindow = SW_HIDE
    
    ' Build command line
    dim as DWSTRING wszGDBexe = AfxGetExePathName & "\bin\gdb\gdb.exe"
    if len(executable) > 0 then
        cmdline = wszGDBexe & " -q -i=mi2 " & chr(34) & executable & chr(34) & " " & cmdparams
    else
        cmdline = wszGDBexe & " -q -i=mi2"
    end if

    ' Create the GDB process
    ret = CreateProcess( NULL, cmdline, NULL, NULL, TRUE, _
                         CREATE_NO_WINDOW, NULL, NULL, @si, @pi )
    
    if ret = 0 then
        dim as DWORD err_code = GetLastError()
        print "CreateProcess failed with error code: "; err_code
        select case err_code
            case 2
                print "Error: gdb.exe not found. Please ensure GDB is installed and in PATH."
            case 3
                print "Error: Path not found."
            case 5
                print "Error: Access denied."
            case 87
                print "Error: Invalid parameter."
            case 193
                print "Error: Not a valid Win32 application."
            case else
                print "Error: Unknown error code."
        end select
        
        CloseHandle(hStdInRead)
        CloseHandle(hStdInWrite)
        CloseHandle(hStdOutRead)
        CloseHandle(hStdOutWrite)
        CloseHandle(hStdErrRead)
        CloseHandle(hStdErrWrite)
        return false
    end if
    
       
    ' Close handles we don't need in parent process
    CloseHandle(hStdInRead)
    CloseHandle(hStdOutWrite)
    CloseHandle(hStdErrWrite)
    
    ' Store session info
    gdb_session.hProcess    = pi.hProcess
    gdb_session.hThread     = pi.hThread
    gdb_session.hStdInWrite = hStdInWrite
    gdb_session.hStdOutRead = hStdOutRead
    gdb_session.hStdErrRead = hStdErrRead
    gdb_session.dwProcessId = pi.dwProcessId
    gdb_session.initialized = true

    return true
end function


' Clear any pending output from pipe
sub gdb_clear_pipe()
    if gdb_session.initialized = false then return 

    dim as string buffer = space(4096)
    dim as DWORD bytes_read, bytes_avail
    
    ' Read and discard any pending data
    do
        if gdb_session.KillMessageThread = true then exit do
        
        if PeekNamedPipe( gdb_session.hStdOutRead, NULL, 0, NULL, @bytes_avail, NULL ) = 0 then exit do
        if bytes_avail = 0 then exit do
        
        if bytes_avail > 4096 then bytes_avail = 4096
        ReadFile( gdb_session.hStdOutRead, strptr(buffer), bytes_avail, @bytes_read, NULL )
    loop while bytes_avail > 0
end sub


' Send command to GDB (synchronous)
function gdb_send( byref cmd as string ) as boolean
    if gdb_session.initialized = false then return false
    
    dim as string full_cmd = cmd + chr(13) + chr(10)
    dim as DWORD bytes_written
    dim as integer ret
    
    ret = WriteFile( gdb_session.hStdInWrite, strptr(full_cmd), len(full_cmd), @bytes_written, NULL )
    FlushFileBuffers( gdb_session.hStdInWrite )

    return iif(ret <> 0 andalso bytes_written > 0, true, false)
end function


' Receive response from GDB (synchronous with timeout)
function gdb_receive() as boolean
    if gdb_session.initialized = false then return false
    
    dim as string buffer = space(4096)
    dim as DWORD bytes_read, bytes_avail
    dim as double start_time = timer
    dim as integer ret
    
    dim as integer timeout_ms = 500
    dim as string response = ""
    
    do
        if gdb_session.KillMessageThread = true then return false
        if gdb_session.initialized = false then return false
        
        ret = PeekNamedPipe( gdb_session.hStdOutRead, NULL, 0, NULL, @bytes_avail, NULL )
        
        if (ret <> 0) andalso (bytes_avail > 0) then
            if bytes_avail > 4096 then bytes_avail = 4096
            
            ret = ReadFile( gdb_session.hStdOutRead, strptr(buffer), bytes_avail, @bytes_read, NULL )
            
            if (ret <> 0) andalso (bytes_read > 0) then
                response = response & left(buffer, bytes_read)
                start_time = timer
            end if
        else
            exit do
        end if
        
        if (timer - start_time) * 1000 > timeout_ms then exit do
    loop

    if instr(response, "(gdb)") > 0 orelse instr(response, "^done") > 0 orelse _
       instr(response, "^running") > 0 orelse instr(response, "^error") > 0 orelse _
       instr(response, "*stopped") > 0 then
        ' Clear any old data first
        gdb_clear_pipe()

        ' Create a new message record and add it to the message queue. The WM_TIMER will
        ' check the message queue for any pending messages.
        MutexLock( gdb_session.hThreadMutex )
        gdb_enqueue_message( response )
        MutexUnLock( gdb_session.hThreadMutex )
    end if
        
    return false
end function


sub gdb_threadListener( byval userdata as any ptr )
    do until gdb_session.KillMessageThread = true
        gdb_receive() 
    loop    
end sub


' Close GDB session
sub gdb_close()
    if gdb_session.initialized then
        gdb_session.KillMessageThread = true

        ' Clear the message queue
        clearQueue()
    
        ' Try to quit gracefully
        gdb_send("quit")
        sleep 200
        
        ' Check if process is still running
        dim as DWORD exit_code
        if GetExitCodeProcess(gdb_session.hProcess, @exit_code) <> 0 then
            if exit_code = STILL_ACTIVE then
                ' Send quit again with confirmation disabled
                gdb_send("set confirm off")
                sleep 50
                gdb_send("quit")
                sleep 200
                
                ' If still running, force terminate
                if GetExitCodeProcess(gdb_session.hProcess, @exit_code) <> 0 then
                    if exit_code = STILL_ACTIVE then
                        TerminateProcess(gdb_session.hProcess, 0)
                        sleep 100
                    end if
                end if
            end if
        end if

        CloseHandle(gdb_session.hStdInWrite)
        CloseHandle(gdb_session.hStdOutRead)
        CloseHandle(gdb_session.hStdErrRead)
        CloseHandle(gdb_session.hThread)
        CloseHandle(gdb_session.hProcess)

        gdb_session = type<GDBSession>()
        
        gdb_session.initialized = false
    end if
end sub


function gdb_findResultValue( byref response as string, byref token as string ) as string
    ' Add to incoming token:   token="
    dim as string result
    
    token = token & "=" & chr(34)
    dim as integer iStart = instr( response, token )
    if iStart > 0 then
        iStart = iStart + len(token)
        dim as integer iEnd = instr( iStart, response, chr(34) )
        if iEnd > 0 then
            result = mid( response, iStart, iEnd - iStart ) 
        end if
    end if
    
    return result
end function


function gdb_findFunctionArguments( byref response as string, byref token as string ) as string
    ' Add to incoming token:   token="
    ' args=[{name="NUMHOURS$1",value="1500"},{name="NUMHOURS$1@entry",value="1500"}]
    dim as string result
    
    token = token & "=" & "["
    dim as long iStart = instr( response, token )
    if iStart > 0 then
        iStart = iStart + len(token)
        dim as long iEnd = instr( iStart, response, "]" )
        if iEnd > 0 then
            result = mid( response, iStart, iEnd - iStart ) 
        end if
    end if
    
    return result
end function


function gdb_loadBreakpointsArray( byref response as string ) as long
    dim as string token
    dim as long p1 
    dim as long p2
    
    p1 = instr( response, "body=" )
    if p1 = 0 then return 0
    p1 += 6
    
    erase gdb_session.breakpoint_array
    
    do
        dim as long numberId
        dim as long linenum
        dim as string func
        
        token = "number=" & chr(34)    
        p1 = instr( p1, response, token )
        if p1 = 0 then exit do
        p1 += len(token)
        numberId = val( mid(response, p1) )
        
        token = "func=" & chr(34)
        p1 = instr( p1, response, token )
        if p1 = 0 then exit do
        p1 += len(token)
        p2 = instr( p1, response, chr(34) )
        if p2 > 0 then
            func = mid( response, p1, p2 - p1 ) 
        end if    
        p1 = p2 + 1

        token = "line=" & chr(34)
        p1 = instr( p2, response, token )
        if p1 = 0 then exit do
        p1 += len(token)
        linenum = val( mid(response, p1) )
        
        ' Add the newly found breakpoint to the array
        dim as long ub = ubound(gdb_session.breakpoint_array) + 1
        redim preserve gdb_session.breakpoint_array(ub)
        gdb_session.breakpoint_array(ub).numberId = numberId
        gdb_session.breakpoint_array(ub).func     = func
        gdb_session.breakpoint_array(ub).linenum  = linenum
    loop
    
    return 0
end function


function gdb_deleteCurrentLineBreakpoint() as long
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr
    if pDoc = 0 then return 0
    
    dim as any ptr pSci = pDoc->GetActiveScintillaPtr()
    dim fMark as long  ' 32 bit value
    dim nCurLine as long = gdb_session.breakpoint_line_to_delete + 1  ' add 1 for GDB

    ' Find the breakpoint in the array based on the filename, func, and linenum.
    for i as long = lbound(gdb_session.breakpoint_array) to ubound(gdb_session.breakpoint_array)
        if gdb_session.breakpoint_array(i).linenum = nCurLine then
            if gdb_session.breakpoint_array(i).func = gdb_session.current_function_name then
                dim as string bp_cmd = "-break-delete " & str(gdb_session.breakpoint_array(i).numberId)
                gdb_send( bp_cmd )
                exit for
            end if        
        end if
    next
    
    gdb_session.current_debug_action = 0
    gdb_session.breakpoint_line_to_delete = 0
    
    return 0
end function

    
function gdb_enableBreaksInFunction() as long
    if gdb_session.disabled_breakpoints = "" then return 0
    
    dim as string send_command = "-break-enable"
    gdb_send( send_command & gdb_session.disabled_breakpoints )

    gdb_session.function_breakpoints = ""
    gdb_session.disabled_breakpoints = ""

    return 0
end function


function gdb_disableBreaksInFunction() as long
    ' Do a batch disable of the breakpoints
    dim as string send_command = "-break-disable"
    dim as string breakpoints

    for i as long = lbound(gdb_session.breakpoint_array) to ubound(gdb_session.breakpoint_array)
        select case gdb_session.current_debug_action
            case IDM_DEBUG_STEPOVER
                ' Disable all breakpoints EXCEPT those in the current function
                if gdb_session.function_breakpoints <> gdb_session.breakpoint_array(i).func then
                    breakpoints = breakpoints & " " & str(gdb_session.breakpoint_array(i).numberId)
                end if
            case IDM_DEBUG_STEPOUT    
                ' Disable all breakpoints ONLY for those in the current function
                if gdb_session.function_breakpoints = gdb_session.breakpoint_array(i).func then
                    breakpoints = breakpoints & " " & str(gdb_session.breakpoint_array(i).numberId)
                end if
        end select
    next
    gdb_session.disabled_breakpoints = breakpoints
    
    if len(breakpoints) then
        gdb_send( send_command & breakpoints )
    end if
        
    select case gdb_session.current_debug_action
        case IDM_DEBUG_STEPOVER
            gdb_send("-exec-next")
        case IDM_DEBUG_STEPOUT    
            gdb_send("-exec-finish")
    end select
    ' the response calls: gdb_enableBreaksInFunction to re-enable
    ' the function's breakpoints after we stepover or stepout.
    gdb_session.current_debug_action = 0

    return 0
end function


function gdb_getVariableDisplayName( byval varName as string ) as string
    dim as string varDisplayString = AfxStrExtract( 1, varName, "$" )
    if varDisplayString = varName then return varDisplayString
    if len(varName) = 0 then return ""
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as long r, startPos, endPos

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    ' Try to find the variable within the source code starting at the function line
    startPos = SciExec( hEdit, SCI_POSITIONFROMLINE, gdb_session.current_function_linenum, 0)
    endPos = SciExec( hEdit, SCI_GETLENGTH, 0, 0)

    dim as string txtFind = varDisplayString
    SciExec( hEdit, SCI_SETTARGETSTART, startPos, 0)
    SciExec( hEdit, SCI_SETTARGETEND, endPos, 0)
    r = SciExec( hEdit, SCI_SEARCHINTARGET, len(txtFind), strptr(txtFind))
    if r then
        varDisplayString = pDoc->GetTextRange( r, r + len(txtFind) )
    end if

    function = varDisplayString
end function


function gdb_getVariableDisplayValue( byval varValue as string ) as string
    dim as string varDisplayString = varValue

    ' Clean the response string
    dim as string st = varValue
    st = AfxStrReplace( st, "'#13#10'", "(CRLF)" )
    st = AfxStrReplace( st, "'#10'", "(LF)" )
    st = AfxStrReplace( st, "'#13'", "(CR)" )
    st = AfxStrReplace( st, "'#9'", "(TAB)" )
    st = AfxStrReplace( st, "\" & chr(34), chr(34) )
    varValue = st

    ' If dealing with a string then try to extract out the string
    if left(ucase(varValue), 7) = "DATA = " then
        dim as string token = " len = "
        dim as long nLen
        dim as long i = instr( varValue, token )
        if i then nLen = val( mid(varValue, i + len(token)) )

        dim as string wszExtractString = AfxStrExtract( 1, varValue, "'", "'" )
        if len(wszExtractString) then
            if nLen = 0 then nLen = len(wszExtractString)
            varDisplayString = chr(34) & wszExtractString & chr(34) & "  (len = " & str(nLen) & ")"
        end if    
    end if
    
    ' Try converting a Hex number to Decimal
    if left(varValue, 2) = "0x" then
        dim as string wszHexString = "&H" & mid(varValue, 3)
        varDisplayString = wszHexString & "  (dec = " & str(val(wszHexString)) & ")"
    end if

    function = varDisplayString
end function


function gdb_parseVariablesResponse( byref response as string ) as integer
    dim as integer i = 1
    dim as integer nItems = 0
    dim as integer p1 = 0
    dim as integer p2 = 0
    dim as string  keyValue
    dim as string  valValue

    dim hLV as HWND = HWND_FRMDEBUG_OUTPUT

    dim as DWSTRING wszText


    response = trim(response)
    if len(response) then
        
        do while i <= len(response)
            keyValue = ""
            valValue = ""
            
            '' find name="
            p1 = instr(i, response, "name=" & chr(34))
            if p1 = 0 then exit do
            p1 += 6
            p2 = instr(p1, response, chr(34))
            if p2 = 0 then exit do
            keyValue = mid(response, p1, p2 - p1)

            '' find value="
            p1 = instr(p2, response, "value=" & chr(34))
            if p1 = 0 then exit do
            p1 += 7
            ' Check if this Value is a list and if yes then adjust how we search for the terminating token.
            dim as string endToken = chr(34)
            if mid(response, p1, 1) = "{" then
                endToken += "}": p1 += 1
            end if
            p2 = instr(p1, response, endToken)
            if p2 = 0 then exit do
            valValue = mid(response, p1, p2 - p1)

            i = p2 + 1
            
            dim as boolean bAddVariable = true
            if left(keyValue, 3) = "vr$" then bAddVariable = false
            if left(keyValue, 4) = "TMP$" then bAddVariable = false
            if instr(valValue, "<optimized out>") then bAddVariable = false

            if (bAddVariable = true) andalso (gdb_session.is_new_variables = true) then
                redim preserve gdb_session.variable_array(nItems)
                gdb_session.variable_array(nItems).VarName  = keyValue
                gdb_session.variable_array(nItems).VarNameDisplay = gdb_getVariableDisplayName( keyValue )
                gdb_session.variable_array(nItems).VarValue = gdb_getVariableDisplayValue( valValue )
                nItems += 1
            else
                if (bAddVariable = true) andalso (gdb_session.is_new_variables = false) then
                    ' The variable list already exists so simply find the variable name and update its value
                    for i as long = lbound(gdb_session.variable_array) to ubound(gdb_session.variable_array)
                        if gdb_session.variable_array(i).varname = keyValue then
                            gdb_session.variable_array(i).varvalue = gdb_getVariableDisplayValue( valValue )
                            exit for
                        end if
                    next
                end if
            end if
            
        loop
    end if
    

    if gdb_session.is_new_variables = false then
        ' Simply update the value held in the ListView
        for i as long = lbound(gdb_session.variable_array) to ubound(gdb_session.variable_array) - 1
            for j as long = 0 to PsListTree_GetCount( hLV ) - 1
                if PsListTree_GetCellText(hLV, j, 1) = gdb_session.variable_array(i).VarName then
                    PsListTree_SetCellText(hLV, j, 3, gdb_session.variable_array(i).VarValue)
                    exit for
                end if
            next
        next    
    end if
    
   
    if gdb_session.is_new_variables = true then
        ' Add all of new array to the ListView
        dim as VariableType temp

        ' Simple bubble sort the variables
        for i as long = lbound(gdb_session.variable_array) to ubound(gdb_session.variable_array) - 1
            for j as long = i + 1 to ubound(gdb_session.variable_array)
                if gdb_session.variable_array(i).VarName > gdb_session.variable_array(j).VarName then
                    temp = gdb_session.variable_array(i)
                    gdb_session.variable_array(i) = gdb_session.variable_array(j)
                    gdb_session.variable_array(j) = temp
                end if
            next 
        next 

        PsListTree_Clear( hLV )

        for i as long = lbound(gdb_session.variable_array) to ubound(gdb_session.variable_array)
            wszText = "" & chr(9) & _
                        gdb_session.variable_array(i).VarName & chr(9) & _
                        gdb_session.variable_array(i).VarNameDisplay & chr(9) & _
                        gdb_session.variable_array(i).VarValue
            ListBox_AddTabbedRow( hLV, wszText )
        next
    end if


    ' Now set the flag that the array has variable names
    gdb_session.is_new_variables = false

    ' Set the column widths to the size of the max text widths
    dim as long nTextWidth
    dim as long nVarNameColWidth = getTextWidth( hLV, L(285, "Variable"), GUIFONT_9 )
    dim as long nVarNameDisplayColWidth = nVarNameColWidth
    dim as long nVarValueColWidth = getTextWidth( hLV, L(286, "Value"), GUIFONT_9 )
    
    for i as long = lbound(gdb_session.variable_array) to ubound(gdb_session.variable_array)
        ' Get the unscaled width of the text so that we can set the ListView column
        ' widths after all lines have been evaluated.
        nTextWidth = getTextWidth( hLV, gdb_session.variable_array(i).varname, GUIFONT_9 )
        nVarNameColWidth = MAX( nVarNameColWidth, nTextWidth )
        nTextWidth = getTextWidth( hLV, gdb_session.variable_array(i).VarNameDisplay, GUIFONT_9 )
        nVarNameDisplayColWidth = MAX( nVarNameDisplayColWidth, nTextWidth )
        nTextWidth = getTextWidth( hLV, gdb_session.variable_array(i).varvalue, GUIFONT_9 )
        nVarValueColWidth = MAX( nVarValueColWidth, nTextWidth )
    next
    ' Set the column widths based on the variable name and values
    ' Add a little extra padding to the widths
    nVarNameColWidth = nVarNameColWidth + 10
    nVarNameDisplayColWidth = nVarNameDisplayColWidth + 10
    nVarValueColWidth = nVarValueColWidth + 10
    PsListTree_SetColumnWidth( hLV, 1, AfxScaleX(nVarNameColWidth) )
    PsListTree_SetColumnWidth( hLV, 2, AfxScaleX(nVarNameDisplayColWidth) )
    PsListTree_SetColumnWidth( hLV, 3, AfxScaleX(nVarValueColWidth) )

    ' The old control had to be hidden when empty (its WM_DRAWITEM never fired, leaving a
    ' white area). PsListTree paints its own empty background, so the pane just stays shown --
    ' and it repaints itself on mutation, so there is no Refresh to call.
    ShowWindow( hLV, SW_SHOW )

    return nItems
end function


sub gdb_processResponse( byval response as string ) 
    dim as string file_name 
    dim as string function_name 
    dim as string function_args
    dim as long   line_number

LM( response )
LM( "" )

    if instr( response, "^error,msg=" ) then
LM( "ERROR:" )
        exit sub
    end if

    if instr( response, "^done,variables=") then 
        gdb_parseVariablesResponse( response )
        exit sub
    end if
    
    if instr( response, "^done,BreakpointTable=") then 
        gdb_loadBreakpointsArray( response )
        if gdb_session.current_debug_action = IDM_DELETE_CURRENT_LINE_BREAKPOINT then
            gdb_deleteCurrentLineBreakpoint()
        else
            gdb_session.function_breakpoints = gdb_session.current_function_name
            gdb_disableBreaksInFunction()
        end if    
        exit sub
    end if

    if instr( response, "*stopped,reason=" & chr(34) & "function-finished" & chr(34)) orelse _   ' StepOut
       instr( response, "*stopped,reason=" & chr(34) & "end-stepping-range" & chr(34)) then      ' StepOver
        gdb_enableBreaksInFunction()
        ' do not exit sub here because we want to check it again in the following If instr statements.
    end if

    ' Process hitting a Breakpoint
    if instr( response, "*stopped,reason=" & chr(34) & "breakpoint-hit" & chr(34)) orelse _
       instr( response, "*stopped,reason=" & chr(34) & "function-finished" & chr(34)) orelse _
       instr( response, "*stopped,reason=" & chr(34) & "end-stepping-range" & chr(34)) then
        ' Line numbers in GDB are 1 based whereas Scintilla is 0 based.
        line_number = val( gdb_findResultValue( response, "line" ) ) - 1
        file_name = gdb_findResultValue( response, "fullname" )
        file_name = AfxStrReplace( file_name, "\\", "\" )
        file_name = AfxStrReplace( file_name, "/", "\" )
        function_name = gdb_findResultValue( response, "func" )
        function_args = gdb_findFunctionArguments( response, "args" )

        ' If the current function name or filename has changed then reset the variable array.
        ' The new values for the array are set in gdb_parseVariablesResponse()
        if (file_name <> gdb_session.current_file_name) orelse _
            (function_name <> gdb_session.current_function_name) then
            erase gdb_session.variable_array
            gdb_session.is_new_variables = true
        end if    
        gdb_session.current_file_name = file_name
        gdb_session.current_function_name = function_name
        gdb_session.current_function_linenum = line_number

'gdb_send( "-var-create my_fbc_var * " & chr(34) & "$6FBCCTX" & chr(34) )
'gdb_send( "-var-create my_fbc_var @ " & chr(34) & "$6FBCCTX" & chr(34) )
'gdb_send( "-var-list-children --all-values my_fbc_var" )


'LM( "STOP: BREAKPOINT OR STEPPING" )
'LM( "  file_name:     " & file_name )
'LM( "  function_name: " & function_name )
'LM( "  function_args: " & function_args )
'LM( "  line_number:   " & str(line_number) )

        if (len(file_name) > 0) andalso (line_number >= 0) then
            ' Get the filename as represented in the gApp doc list. That filename is in correct
            ' lettercase format whereas the filename returned via GDB is all uppercase. If that
            ' source file has not yet been loaded in the editor then simply use the GDB returned name.
            dim as DWSTRING wszFilename = file_name
            dim as clsDocument ptr pDoc = gApp.GetDocumentPtrByFilename(wszFilename)
            if pDoc then 
                wszFilename = pDoc->DiskFilename
            else
                dim as DWSTRING wszTemp = AfxFilenameOriginalCase( file_name )
                if AfxFileExists(wszTemp) then wszFilename = wszTemp
            end if
            pDoc = OpenSelectedDocument( wszFilename, "", line_number)
            if pDoc then
                SciExec( pDoc->hWndActiveScintilla, SCI_MARKERDELETEALL, MARKER_DEBUGGER_CURLINE, 0 )
                pDoc->ToggleMarker( line_number, MARKER_DEBUGGER_CURLINE )
                ' Ask to get list of variables and values for current context
                gdb_send(  "-stack-list-variables --all-values" )
            end if    
        end if
    end if

    if instr( response, "*stopped,reason=" & chr(34) & "exited-normally" & chr(34)) orelse _
       instr( response, "*stopped,reason=" & chr(34) & "exited" & chr(34)) orelse _
       instr( response, "*stopped,reason=" & chr(34) & "exited-signalled" & chr(34)) then
        SetCompileStatusBarMessage( L(259,"Debug Target Exited Normally"), ghIconGood )   ' ghIconGood:green, ghIconBad: red
        SendMessage( HWND_FRMDEBUG, IDM_DEBUG_STOPDEBUGGING, 0, 0 )
    end if

    if instr( response, "*stopped,reason=" & chr(34) & "signal-received" & chr(34) & ",signal-name=" & chr(34) & "SIGSEGV" & chr(34)) then
        SetCompileStatusBarMessage( L(260,"Debug Target Terminated Unexpectantly"), ghIconBad )   ' ghIconGood:green, ghIconBad: red
        SendMessage( HWND_FRMDEBUG, IDM_DEBUG_STOPDEBUGGING, 0, 0 )
    end if

end sub


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmDebug_PositionWindows() as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit function
    
    ' Get the entire client area
    dim as RECT rc: GetClientRect(HWND_FRMDEBUG, @rc)

    dim as long i = 0
    dim as long nSpacer     = pWindow->ScaleX(4)
    dim as long nLeft       = rc.left + pWindow->ScaleY(20)
    dim as long nHeight     = pWindow->ScaleY(TOPTABS_HEIGHT)
    dim as long nPadding    = 10   ' 5 left 5 right
    dim as long nImageWidth = DEBUG_BUTTON_IMAGE_WIDTH

    i = DEBUG_BUTTONS.STOP_DEBUGGING
    gDbgBtns(i).nID        = IDM_DEBUG_STOPDEBUGGING
    gDbgBtns(i).wszToolTip = getMenuText(gDbgBtns(i).nID)
    gDbgBtns(i).wszIcon    = wszIconStop
    gDbgBtns(i).wszText    = L(258,"Stop")
    gDbgBtns(i).rc.Left    = nLeft
    gDbgBtns(i).rc.top     = rc.top
    gDbgBtns(i).rc.right   = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMDEBUG, gDbgBtns(i).wszText, GUIFONT_9) + nPadding + nImageWidth)
    gDbgBtns(i).rc.bottom  = rc.top + nHeight
    nLeft = gDbgBtns(i).rc.right + nSpacer

    i = DEBUG_BUTTONS.CONTINUE_DEBUGGING
    gDbgBtns(i).nID        = IDM_DEBUG_STARTDEBUGGING
    gDbgBtns(i).wszToolTip = getMenuText(gDbgBtns(i).nID)
    gDbgBtns(i).wszIcon    = wszIconContinue
    gDbgBtns(i).wszText    = L(257,"Continue")
    gDbgBtns(i).rc.Left    = nLeft
    gDbgBtns(i).rc.top     = rc.top
    gDbgBtns(i).rc.right   = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMDEBUG, gDbgBtns(i).wszText, GUIFONT_9) + nPadding + nImageWidth)
    gDbgBtns(i).rc.bottom  = rc.top + nHeight
    nLeft = gDbgBtns(i).rc.right + nSpacer

    i = DEBUG_BUTTONS.STEP_OVER
    gDbgBtns(i).nID        = IDM_DEBUG_STEPOVER
    gDbgBtns(i).wszToolTip = getMenuText(gDbgBtns(i).nID)
    gDbgBtns(i).wszIcon    = wszIconStepOver
    gDbgBtns(i).wszText    = L(248,"Step Over")
    gDbgBtns(i).rc.Left    = nLeft
    gDbgBtns(i).rc.top     = rc.top
    gDbgBtns(i).rc.right   = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMDEBUG, gDbgBtns(i).wszText, GUIFONT_9) + nPadding + nImageWidth)
    gDbgBtns(i).rc.bottom  = rc.top + nHeight
    nLeft = gDbgBtns(i).rc.right + nSpacer

    i = DEBUG_BUTTONS.STEP_INTO
    gDbgBtns(i).nID        = IDM_DEBUG_STEPINTO
    gDbgBtns(i).wszToolTip = getMenuText(gDbgBtns(i).nID)
    gDbgBtns(i).wszIcon    = wszIconStepNext
    gDbgBtns(i).wszText    = L(247,"Step Into")
    gDbgBtns(i).rc.Left    = nLeft
    gDbgBtns(i).rc.top     = rc.top
    gDbgBtns(i).rc.right   = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMDEBUG, gDbgBtns(i).wszText, GUIFONT_9) + nPadding + nImageWidth)
    gDbgBtns(i).rc.bottom  = rc.top + nHeight
    nLeft = gDbgBtns(i).rc.right + nSpacer

    i = DEBUG_BUTTONS.STEP_OUT
    gDbgBtns(i).nID        = IDM_DEBUG_STEPOUT
    gDbgBtns(i).wszToolTip = getMenuText(gDbgBtns(i).nID)
    gDbgBtns(i).wszIcon    = wszIconStepOut
    gDbgBtns(i).wszText    = L(255,"Step Out")
    gDbgBtns(i).rc.Left    = nLeft
    gDbgBtns(i).rc.top     = rc.top
    gDbgBtns(i).rc.right   = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMDEBUG, gDbgBtns(i).wszText, GUIFONT_9) + nPadding + nImageWidth)
    gDbgBtns(i).rc.bottom  = rc.top + nHeight
    nLeft = gDbgBtns(i).rc.right + nSpacer

    i = DEBUG_BUTTONS.RUN_TO_CURSOR
    gDbgBtns(i).nID        = IDM_DEBUG_RUNTOCURSOR
    gDbgBtns(i).wszToolTip = getMenuText(gDbgBtns(i).nID)
    gDbgBtns(i).wszIcon    = wszIconRunToCursor
    gDbgBtns(i).wszText    = L(256,"Run to Cursor")
    gDbgBtns(i).rc.Left    = nLeft
    gDbgBtns(i).rc.top     = rc.top
    gDbgBtns(i).rc.right   = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMDEBUG, gDbgBtns(i).wszText, GUIFONT_9) + nPadding + nImageWidth)
    gDbgBtns(i).rc.bottom  = rc.top + nHeight
    nLeft = gDbgBtns(i).rc.right + nSpacer

    AfxRedrawWindow( HWND_FRMDEBUG )

    dim as HWND hCtl 
    
    if HWND_FRMDEBUG_OUTPUT = 0 then
        ' Variables pane: a PsListTree with a PsColumnHeader band, sharing the Output panel's
        ' row/header painters (same ghOutput colors, same layout). The Value column fills
        ' the remaining width; the widths of the other two are recomputed from the data on
        ' every refresh, further down in gdb_ParseVariables.
        hCtl = PsListTree_Create( HWND_FRMDEBUG, IDC_FRMDEBUG_LVVARS )
            PsListTree_AddColumn( hCtl, "",                 pWindow->ScaleX(10) )
            PsListTree_AddColumn( hCtl, L(285, "Variable"), pWindow->ScaleX(75) )
            PsListTree_AddColumn( hCtl, "",                 pWindow->ScaleX(75) )
            PsListTree_AddColumn( hCtl, L(286, "Value"),    pWindow->ScaleX(250) )
            PsListTree_SetFillColumn( hCtl, 3 )
            PsListTree_ShowHeader( hCtl, true )
            PsListTree_SetPaintCallback( hCtl, @ListBox_LVPaintRow )
            PsListTree_SetHeaderPaintCallback( hCtl, @ListBox_LVPaintHeader )
            PsListTree_SetColumnAutoSizeCallback( hCtl, @ListBox_LVAutoSizeColumn )
            PsListTree_SetFont( hCtl, ghFont(GUIFONT_9) )
            PsListTree_SetBackColor( hCtl, ghOutput.BackColor )
            PsListTree_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, _
                                         ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )

        HWND_FRMDEBUG_OUTPUT = hCtl
    end if
    GetClientRect(HWND_FRMDEBUG, @rc)
    SetWindowPos( HWND_FRMDEBUG_OUTPUT, 0, 0, nHeight, rc.right, rc.bottom - nHeight, SWP_NOZORDER )
    
    function = 0
end function


' ========================================================================================
' frmDebug Window procedure
' ========================================================================================
function frmDebug_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as long prev_idxHot = -1
    static as long curr_idxHot = -1

    select case uMsg

        case WM_CLOSE
            DestroyWindow( hwnd )
            
        case WM_DESTROY
            HWND_FRMDEBUG_OUTPUT = 0

            ' The tip is a WS_POPUP owned by this window, not a child, so it does not go with
            ' the parent -- destroying it here also removes it from the shared clock's registry.
            if IsWindow(ghDebugTip) then DestroyWindow( ghDebugTip )
            ghDebugTip = 0

            KillTimer( HWND_FRMDEBUG, gApp.DebugTimerID )
            
            ' Kill the GDB process
            gdb_close()

            ' Save the current size of the debugger window so that we can re-use
            ' this position should th euser invoke the debugger again.
            dim WinPla as WINDOWPLACEMENT
            WinPla.Length = sizeof(WinPla)
            GetWindowPlacement( HWND_FRMDEBUG, @WinPla )
            With gDebugPos
                .nLeft   = WinPla.rcNormalPosition.Left 
                .nTop    = WinPla.rcNormalPosition.Top
                .nRight  = WinPla.rcNormalPosition.Right
                .nBottom = WinPla.rcNormalPosition.Bottom
                .bMaximized = iif( WinPla.showCmd = SW_MAXIMIZE, true, false )
                .bInitialized = true
            end With
            
            ' Reset the global gdb_session variable
            gdb_session = type<GDBSession>()
            
            
        case WM_NCDESTROY
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            gdb_session.KillMessageThread = true
            ThreadWait( gdb_session.hThreadMessages )
            MutexDestroy( gdb_session.hThreadMutex )

            gdb_session.hThreadMessages   = 0
            gdb_session.hThreadMutex      = 0
            gdb_session.KillMessageThread = false
            gdb_session.initialized       = false

            ' Remove any yellow arrow debugger current line marker
            dim pDoc as clsDocument ptr = gApp.pDocList
            do until pDoc = 0
                SciExec( pDoc->hWindow(0), SCI_MARKERDELETEALL, MARKER_DEBUGGER_CURLINE, 0 )
                pDoc = pDoc->pDocNext
            loop

            gApp.IsDebuggerActive = false
            
        case WM_TIMER
            if wParam = gApp.DebugTimerID then
                ' Check for any pending message
                if gdb_isMessageQueueEmpty() = false then
                    MutexLock( gdb_session.hThreadMutex )
                    dim as string message = gdb_dequeue_message()
                    MutexUnLock( gdb_session.hThreadMutex )
                    gdb_processResponse( message )    
                end if
            end if

        case WM_SIZE
            ' Position all of the child windows
            if wParam <> SIZE_MINIMIZED then
                frmDebug_PositionWindows()
                return 0
            end if

        case WM_ERASEBKGND
            return true

        case WM_LBUTTONDOWN
            SetCapture( hwnd )
            
        case WM_MOUSEMOVE
            ' TME_LEAVE only. TME_HOVER existed solely to drive the old WM_MOUSEHOVER tooltip
            ' arm; PsTooltip does its own dwell timing off the shared clock.
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)

            ' Silent when the index has not changed; on a change it hides any visible tip
            ' without a fade and clears a suppressed hover, so the new button gets a fresh offer.
            PsTooltip_SetToolIndex( ghDebugTip, frmDebug_TipHitTest( hwnd ) )

            dim as point pt: GetCursorPos( @pt )
            MapWindowPoints( HWND_DESKTOP, hwnd, cast( point ptr, @pt ), 1 )

            ' Are we over one of the buttons
            curr_idxHot = -1
            for i as long = lbound(gDbgBtns) to ubound(gDbgBtns)
                if PtInRect( @gDbgBtns(i).rc, pt ) then
                    ' Only make hot panels that have actual text
                    if len(gDbgBtns(i).wszText) then
                        curr_idxHot = i
                        gDbgBtns(i).isHot = true
                    end if   
                else   
                    gDbgBtns(i).isHot = false
                end if
            next
            if curr_idxHot <> prev_idxHot then
                AfxRedrawWindow( hwnd )
                prev_idxHot = curr_idxHot 
            end if

                
        case WM_MOUSELEAVE
            for i as long = lbound(gDbgBtns) to ubound(gDbgBtns)
                gDbgBtns(i).isHot = false
            next
            prev_idxHot = -1
            curr_idxHot = -1
            ' Was DestroyWindow(hToolTip) -- the comctl32 tip was torn down and rebuilt on the
            ' next move to force it to forget its state. Clearing the tool index is the whole
            ' of that job now, and the tip window survives the cursor leaving.
            PsTooltip_SetToolIndex( ghDebugTip, -1 )
            AfxRedrawWindow( hwnd )


        case WM_LBUTTONUP
            dim as point pt: GetCursorPos( @pt )
            MapWindowPoints( HWND_DESKTOP, hwnd, cast( point ptr, @pt ), 1 )
            SetCursor( LoadCursor( null, IDC_ARROW ))
            ReleaseCapture
            ' Are we over one of the status bar panels
            for i as long = lbound(gDbgBtns) to ubound(gDbgBtns)
                if PtInRect( @gDbgBtns(i).rc, pt ) then
                    PostMessage( HWND_FRMDEBUG, gDbgBtns(i).nID, 0, 0 )
                    exit for
                end if
            next   
            

        case WM_PAINT
            dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
            dim as PsBufferPaint b

            b.BeginDoubleBuffer(hwnd)

            ' Set default back and fore colors
            b.SetForeColor( ghTopTabs.ForeColor )
            b.SetBackColor( ghTopTabs.BackColor )

            ' Clear the entire back client area
            b.PaintClientRect()
          
            ' paint the panels
            for i as long = lbound(gDbgBtns) to ubound(gDbgBtns)
                dim as COLORREF ForeColor = ghTopTabs.ForeColorHot
                if gDbgBtns(i).nID = IDM_DEBUG_STARTDEBUGGING then ForeColor = ghGeneral.iconsuccess
                if gDbgBtns(i).nID = IDM_DEBUG_STOPDEBUGGING then ForeColor = ghGeneral.iconfail
                
                if i = curr_idxHot then
                    b.SetBackColor( ghTopTabs.BackColorHot )
                else
                    b.SetBackColor( ghTopTabs.BackColor )
                end if

                b.PaintRect( @gDbgBtns(i).rc )
                
                ' Paint the image
                b.SetForeColor( ForeColor )
                dim as long nImageWidth = pWindow->ScaleX(DEBUG_BUTTON_IMAGE_WIDTH)
                dim as RECT rcIcon = gDbgBtns(i).rc
                rcIcon.right = rcIcon.left + nImageWidth
                b.SetFont( ghFont(SYMBOLFONT_9) )
                b.PaintText( gDbgBtns(i).wszIcon, @rcIcon, DT_CENTER )
                
                ' Paint the text
                b.SetForeColor( ghTopTabs.ForeColorHot )
                dim as RECT rcText = gDbgBtns(i).rc
                rcText.left = rcIcon.right
                b.SetFont( ghFont(GUIFONT_9) )
                b.PaintText( gDbgBtns(i).wszText, @rcText, DT_CENTER )
            next
                                    
            b.EndDoubleBuffer()

            return 0

        case IDM_DEBUG_STARTDEBUGGING
LM( "IDM_DEBUG_STARTDEBUGGING" )
            gdb_send("-exec-continue")
        
        case IDM_DEBUG_STOPDEBUGGING
LM( "IDM_DEBUG_STOPDEBUGGING" )
            gdb_send("kill")
            sleep 100
            PostMessage( HWND_FRMDEBUG, WM_CLOSE, 0, 0 )
        
        case IDM_DEBUG_STEPINTO
LM( "IDM_DEBUG_STEPINTO" )
            gdb_send("-exec-step")
        
        case IDM_DEBUG_STEPOVER
LM( "IDM_DEBUG_STEPOVER" )
            ' Similar problem to STEPOUT, if breakpoints exist in subfunctions.
            ' Need to disable all breakpoints except those in the current function.
            gdb_session.current_debug_action = IDM_DEBUG_STEPOVER
LM( "gdb_send(gdb_session, -break-list)" )
            gdb_send("-break-list")   
        
        case IDM_DEBUG_STEPOUT
LM( "IDM_DEBUG_STEPOUT" )
            ' If subsequent breakpoints exist in the function then GDB will break on them
            ' prior to exiting the function. This is not the expected user experience. We
            ' want to immediately exit back to the calling code. 
            ' - List all breakpoints.    -break-list
            ' - Save the list of breakpoints that match the current_function_name.  
            ' - Temporarily disable those breakpoints.  -break-disable <bpnum>
            ' - Issue the -exec-finish
            ' - Re-enable the breakpoints.  -break-enable <bpnum>
            gdb_session.current_debug_action = IDM_DEBUG_STEPOUT
LM( "gdb_send(gdb_session, -break-list)" )
            gdb_send("-break-list")   
            ' the response calls: gdb_disableBreaksInFunction
            ' the "-exec-finish" is sent from gdb_disableBreaksInFunction after the breakpoints are disabled.

        case IDM_DEBUG_RUNTOCURSOR
LM( "IDM_DEBUG_RUNTOCURSOR" )
            ' Line numbers in GDB are 1 based whereas Scintilla is 0 based.
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                dim as string bp_line = str(pDoc->GetCurrentLineNumber() + 1)
                dim as string bp_cmd = "tbreak " & chr(34) & pDoc->DiskFilename & chr(34) & ":" & bp_line
                gdb_send(bp_cmd )
                gdb_send("-exec-continue")
            end if
            
    end select

    
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


function frmDebug_SetInitialBreakpoints( byref session as GDBSession ) as long
    'break main.c:25                    ' Line 25 in main.c
    'break utils.c:calculateSum         ' Function in utils.c
    'break "my file.c":10              ' File with spaces (use quotes)
    'break C:\path\to\file.c:42        ' Full path with line
    'break src/module.c:init           ' Relative path

    'tbreak main.c:50                  ' Temporary breakpoint
    'break main.c:25 if x > 10         ' Conditional breakpoint in file
    'rbreak main.c:.*                  ' Regex breakpoint (all functions in file)
    'info line main.c:25               ' Show address of line
    'list main.c:25                    ' Show source at line
    
    ' Iterate all pDoc in the project/files list
    dim as string sBreakpoints
    dim as string bp_cmd
    dim as string bp_file
    dim as string bp_line
    dim as long   nCount = 0
    dim as long   numBreakpoints = 0
    
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if pDoc->hWindow(0) <> null then 
            sBreakpoints = pDoc->GetBreakpoints()
        end if
        
        bp_file = pDoc->DiskFilename

        if len(sBreakpoints) then
            nCount = AfxStrParseCount( sBreakpoints, "," )
            for i as long = 1 to nCount
                ' Line numbers in GDB are 1 based whereas Scintilla is 0 based.
                bp_line = AfxStrParse( sBreakpoints, i, "," )
                bp_cmd = bp_cmd & "break " & chr(34) & bp_file & chr(34) & ":" & str(val(bp_line)+1) & chr(10)
                numBreakpoints += 1
            next
        end if
            
        pDoc = pDoc->pDocNext
    loop

    ' If no breakpoints exist then set a temporary breakpoint in the main file
    if numBreakpoints = 0 then
'LM( "Set Temp Breakpoint on Main")
        bp_cmd = "tbreak main"
'        bp_cmd = "-break-insert -t main"
        gdb_send( bp_cmd )
        
    else    
'LM( "Set Breakpoints:")
'LM( bp_cmd & chr(13,10) )
        bp_cmd = rtrim(bp_cmd, chr(10))
        gdb_send( bp_cmd )
    end if

    function = 0    
end function


' ========================================================================================
' frmDebug_Show
' ========================================================================================
function frmDebug_Show( byval hWndParent as HWND, byval executable as string, byval cmdparams as string ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMDEBUG = pWindow->Create( hWndParent, "DEBUGGER", @frmDebug_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_THICKFRAME  or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT )

    if gDebugPos.bInitialized then
        dim WinPla as WINDOWPLACEMENT
        with WinPla
            .Length = sizeof(WinPla)
            .rcNormalPosition.Left   = gDebugPos.nLeft
            .rcNormalPosition.Top    = gDebugPos.nTop
            .rcNormalPosition.Right  = gDebugPos.nRight
            .rcNormalPosition.Bottom = gDebugPos.nBottom
            .showCmd = iif( gDebugPos.bMaximized, SW_MAXIMIZE, SW_SHOWNORMAL )
        end with
        SetWindowPlacement( pWindow->hWindow, @WinPla )
    else
        pWindow->SetClientSize(625, 460)
        pWindow->Center(pWindow->hWindow, hWndParent)
    end if

    ' Set the small and large icon for the main window (must be set after main window is created)
    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' Attempt to apply the standard Windows dark theme to the non-client areas of the form.
    ' Reads the theme currently IN MEMORY, not gConfig -- see Theme_IsDark.
    Theme_ApplyDarkCaption( HWND_FRMDEBUG )

    ' Create ALSO attaches -- there is no separate PsTooltip_Attach call. Colours and fonts are
    ' pushed per hover from frmDebug_TipText, so nothing set here can go stale on a theme change.
    ghDebugTip = PsTooltip_Create( HWND_FRMDEBUG )
    PsTooltip_SetToolIndex( ghDebugTip, -1 )
    PsTooltip_SetTooltipCallback( ghDebugTip, @frmDebug_TipText )

    gApp.IsDebuggerActive = true
    
    frmDebug_PositionWindows()

    SetTimer(HWND_FRMDEBUG, gApp.DebugTimerID, 20, null)   ' 20 ms
    
    ' The debug executable has been sucessfully created so start the debug thread
    ' add feed any breakpoints into it.
    if gdb_init( executable, cmdparams ) then
        frmDebug_SetInitialBreakpoints( gdb_session )
        gdb_send("set confirm off" )
        gdb_send("set language pascal" )
        
        gdb_session.hThreadMutex = MutexCreate()
        gdb_session.hThreadMessages = ThreadCreate( @gdb_threadListener, @gdb_session )
        
    else
        TikoMsgBox( HWND_FRMMAIN, "Failed to initialize GDB. Ensure gdb.exe is installed correctly.", _
                    L(164,"Error"), TMB_ICON_ERROR, TMB_OK )
        DestroyWindow( HWND_FRMMAIN )
        exit function            
    end if
    
    gdb_send("-exec-run")
    
    ShowWindow( HWND_FRMDEBUG, SW_SHOW )
    
    return 0
end function





