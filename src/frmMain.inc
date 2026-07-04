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

#include once "frmMain.bi"
#include once "clsDocument.bi"
#include once "frmProjectOptions.bi"
#include once "frmBuildConfig.bi"
#include once "frmUserTools.bi"
#include once "frmExplorer.bi"
#include once "frmPanel.bi"
#include once "frmStatusBar.bi"
#include once "modMRU.bi"
#include once "modAutoInsert.bi"
#include once "modMenus.bi"
#include once "modCompile.bi"

    
' ========================================================================================
' Attempt to open specified project. 
' ========================================================================================
function frmMain_OpenProjectSafely( _
            byval hwnd as HWND, _
            byref wszProjectFileName as const wstring _
            ) as boolean

    ' if a Project is not active then we need to save the current non-project notes
    ' when this file is closed. It is possible that this file is being closed and
    ' a project is being opened.
    if gApp.IsProjectActive = false then
        gApp.NonProjectNotes = AfxGetWindowText(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES))
        gConfig.SaveConfigFile
    end if

    FindReplaceControls_Hide()

    ' if a project is open then close any files that may be open in the editor. No need to
    ' do this if a new project is being created/opened because that function has already
    ' performed this operation. Doing so again will cause the Recent Files/Projects panel
    ' to flash on the screen.
    if gApp.IsNewProjectFlag = false then
        if gApp.IsProjectActive then 
            if OnCommand_ProjectClose(hwnd) = false then exit function
        elseif gApp.IsSessionActive then
            if gConfig.CloseSessionFile( gApp.SessionFilename) = false then exit function
        else   
            if OnCommand_FileClose(HWnd, EFC_CLOSEALL) = false then exit function
        end if
        ' Clear any previous info from the Output windows
        frmOutput_ResetAllControls()
    end if
            
    ' Open the project
    if gConfig.ProjectLoadFromFile(wszProjectFileName) then
        ' Update the most recently used project list 
        UpdateMRUProjectList(wszProjectFileName)
    end if
    gApp.IsProjectActive = true
    
    ' Position all of the controls into place
    frmPanel_PositionWindows
    frmMain_PositionWindows

    function = true
end function


' ========================================================================================
' Update the main form statusbar. This is the only routine that updates
' the statusbar in the entire program.
' ========================================================================================
function frmMain_SetStatusbar() as long

    ' Update the statusbar with the current Line/Col position
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    
    dim wszText as wstring * MAX_PATH
    
    ' PANEL (0) Line/Col/Sel 
    ' PANEL (1) Compiling Results -or- Filename being parsed
    ' PANEL (2) Theme selection (dialog popup)
    ' PANEL (3) Build Configuration (dialog popup)
    ' PANEL (4) Build Configuration (quick select)
    ' PANEL (5) Filetype (Normal/Main/Resource/Module)
    ' PANEL (6) Spacing
    ' PANEL (7) File Encoding (Ansi, UTF-8, etc)
    ' PANEL (8) Line Endings

    ' blank out the current statusbar values  
    for i as long = lbound(gSBPanels) to ubound(gSBPanels)
        gSBPanels(i).wszText = ""
    next

    ' PANEL Compiling Results -or- Filename being parsed
    gSBPanels(COMPILE_STATUS_PANEL).wszText = gApp.wszPanelText

    if (gApp.IsProjectLoading) orelse (gApp.IsFileLoading) then
        gSBPanels(COMPILE_STATUS_PANEL).wszText = L(66,"Parsing") & _
                ": (" & gApp.FileLoadingCount & ")  " & gApp.wszPanelText
        
    elseif pDoc <> 0 then
        dim as HWND hEdit = pDoc->hWndActiveScintilla
        wszText = ""
        dim as long curPos, nLine, nCol
        dim as long startPos, endPos, startLine, endLine, nLines 
        ' Retrieve the information and show it in the status bar
        curPos    = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0)
        nLine     = SciExec( hEdit, SCI_LINEFROMPOSITION, curPos, 0) 
        nCol      = SciExec( hEdit, SCI_GETCOLUMN, curPos, 0) 
        pDoc->GetSelectedLineRange(startLine, endLine, startPos, endPos )

        wszText = " Ln " & str(nLine + 1) & ", Col " & str(nCol + 1) 
        if endPos - startPos then  ' there is selected text
            wszText = wszText + " (" + str(endPos - startPos) + " selected)"
        end if 

        ' Update the vertical scrollbar
        frmEditorVScroll_calcVThumbRect(pDoc)
        AfxRedrawWindow( iif( hEdit = pDoc->hWindow(0), HWND_FRMEDITOR_VSCROLLBAR(0), HWND_FRMEDITOR_VSCROLLBAR(1)) )
        
        ' PANEL Line/Col/Sel
        gSBPanels(GOTO_PANEL).wszText = wszText

        ' PANEL Themes dialog
        gSBPanels(THEMES_DIALOG_PANEL).wszText = " " + wszIconThemes + " "

        ' PANEL Build Configuration dialog
        gSBPanels(BUILD_DIALOG_PANEL).wszText = wszIconAddFileButton 

        ' PANEL Build Configuration
        if gApp.GetDocumentCount then
            gSBPanels(BUILD_POPUP_PANEL).wszText = frmBuildConfig_GetSelectedBuildDescription()
        end if
        
        ' PANEL Spacing
        gSBPanels(SPACES_PANEL).wszText = "Spaces: " & gConfig.TabSize
        
        ' PANEL File Encoding (Ansi, UTF-8, UTF-16)
        wszText = ""
        select case pDoc->FileEncoding
            case FILE_ENCODING_UTF8
                wszText = "UTF-8"   
                SciExec( hEdit, SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
            case FILE_ENCODING_UTF8_BOM
                wszText = "UTF-8 (BOM)"   
                SciExec( hEdit, SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
            case FILE_ENCODING_UTF16_BOM
                wszText = "UTF-16 (BOM)"   
                SciExec( hEdit, SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
            case else
                wszText = "ANSI"  
                SciExec( hEdit, SCI_SETCODEPAGE, 0, 0 )
        end select 
        gSBPanels(UTF_PANEL).wszText = wszText

        ' PANEL Line Endings
        select case SciExec( hEdit, SCI_GETEOLMODE, 0, 0 )
            case SC_EOL_CRLF: wszText = "CRLF"  '(0)
            case SC_EOL_CR:   wszText = "CR"    '(1)
            case SC_EOL_LF:   wszText = "LF"    '(2)
        end select 
        gSBPanels(CRLF_PANEL).wszText = wszText
        
        if SciExec( hEdit, SCI_GETREADONLY, 0, 0 ) then
            gSBPanels(COMPILE_STATUS_PANEL).wszText = L(222,"Read Only")
        end if
        
    end if
    

    ' MAIN WINDOW CAPTION 
    wszText = iif( gApp.GetDocumentCount, APPNAMESHORT, APPNAME )
    if (gApp.IsProjectActive = true) orelse (gApp.IsProjectLoading = true) then 
        wszText = wszText & " - [" & gApp.ProjectName & "]"
    else
        if gApp.IsSessionActive = true then 
            wszText = wszText & " - [" & gApp.SessionName & "]"
        end if
    end if
    if pDoc then 
        wszText = wszText & " - [" & pDoc->DiskFilename & "]"
        if SciExec( pDoc->hWndActiveScintilla, SCI_GETREADONLY, 0, 0 ) then
            wszText = wszText & " - [" & L(222,"Read Only") & "]"
        end if
    end if
    if wszText <> AfxGetWindowText( HWND_FRMMAIN ) then AfxSetWindowText( HWND_FRMMAIN, wszText )

    ' TAB CONTROL FILENAME ( * modified flag )
    gTTabCtl.SetTabText(-1)  ' this will only repaint if text has changed
    ' repaint the tab to ensure that the unsaved indicator changes
    AfxRedrawWindow( HWND_FRMMAIN_TOPTABS )

    ' Call function to calculate the size/position of the panels and also paint the statusbar
    frmStatusBar_PositionWindows()

    function = 0
end Function


' ========================================================================================
' Open any Session or Project on editor startup
' ========================================================================================
function frmMain_RestoreSessionOrProject() as long
    ' Only restore session if that option is active. The session file may contain
    ' a reference to a project if the last session was a project.
    ' Only restore the session if no files were already opened via the command line.
    if gTTabCtl.GetItemCount = 0 then
        if gConfig.RestoreSession then 
            if AfxFileExists( gConfig.wszLastActiveSession ) then
                gConfig.LoadSessionFile( gConfig.wszLastActiveSession )
            else
                gConfig.LoadDefaultSessionFile()
            end if   
        end if
    end if
    
    function = 0 
end function


' ========================================================================================
' Process an array of files (from commandline or dragdrop)
' ========================================================================================
function frmMain_ProcessFilesArray( _
            byval hwnd as HWND, _
            wszFiles() as DWSTRING _
            ) as long

    dim wszExt as wstring * MAX_PATH
    dim as DWSTRING wszArg
    dim as integer numProcessed
    
    for i as integer = lbound(wszFiles) to ubound(wszFiles)
        wszArg = wszFiles(i)
        if AfxFileExists(wszArg) = 0 then continue for

        ' We have a valid filename so determine what type it is.
        wszExt = AfxStrPathname( "EXTN", wszArg )
        wszExt = lcase(wszExt)
        
        select case wszExt
        case APPEXTENSION    ' project file
            numProcessed += 1
            ' Pass the info to our generic project open function to handle everything.
            frmMain_OpenProjectSafely(HWND_FRMMAIN, wszArg) 

        case ".session"      ' session file
            numProcessed += 1
            gConfig.LoadSessionFile(wszArg)
            
        case else   ' .bas, .bi, .rc, etc...
            ' Test to see if the file is already loaded in the editor. if it is, then
            ' bypass loading it again thereby creating multiple ghost instances.
            numProcessed += 1
            dim pDoc as clsDocument ptr
            pDoc = OpenSelectedDocument( wszArg )
            if pDoc then pDoc->SetProjectFileType()
        end select    

    next
    
    function = numProcessed
end function


' ========================================================================================
' Process any command line that was passed to the editor
' ========================================================================================
function frmMain_ProcessCommandLine( byval hwnd as HWND ) as long

    ' The incoming command line may contain a regular file to open or a project file.
    
    ' Command: A space-separated list of all command-line arguments is returned. When the 
    '          command line is parsed for arguments, everything between double quotes in 
    '          the parameter list will be considered as a single parameter, and is returned 
    '          with the double quotes.
    '          A value of zero (0) returns the name of the executable; and values of 
    '          one (1) and greater return each command-line argument.

    ' Uuse AfxCommand which is a unicode aware replacement for FB's
    ' built in COMMAND function (that is not unicode compliant).
    
    if len(AfxCommand(1)) = 0 then 
        ' no command line files/project specified by the user may be wanting
        ' to restore a previous session/project.
        frmMain_RestoreSessionOrProject()
        exit function
    end if
    
    dim wszPath as wstring * MAX_PATH
    dim wszArg  as wstring * MAX_PATH
    dim as integer numProcessed
    dim wszFiles(any) as DWSTRING
    
    if IsIconic(hwnd) then 
        dim WinPla as WINDOWPLACEMENT
        with WinPla
            .Length = sizeof(WinPla)
            .rcNormalPosition.Left   = gConfig.StartupLeft
            .rcNormalPosition.Top    = gConfig.StartupTop
            .rcNormalPosition.Right  = gConfig.StartupRight
            .rcNormalPosition.Bottom = gConfig.StartupBottom
            .showCmd = iif( gConfig.StartupMaximized, SW_MAXIMIZE, SW_SHOWNORMAL )
        end with
        SetWindowPlacement(hwnd, @WinPla)
    end if
    SetForegroundWindow(hwnd)
    
    dim as long i = 1
    do
        wszArg = AfxCommand(i)
        if len(wszArg) = 0 then exit do
        
        ' Remove any double quotes from the argument.
        wszPath = AfxStrRemove( wszArg, wchr(34) )
        
        ' if no path exists for the file then add the current folder
        wszPath = AfxStrPathname( "PATH", wszArg )
        if len(wszPath) = 0 then wszArg = AfxGetExePathName & wszArg 
        
        dim as integer ub = ubound(wszFiles) + 1
        redim preserve wszFiles(ub)
        wszFiles(ub) = wszArg
        
        i += 1
    loop

    numProcessed = frmMain_ProcessFilesArray(hwnd, wszFiles())
    
    ' If no files or project was loaded from the command line then check if the user 
    ' had session/project restore option.
    if numProcessed = 0 then
        frmMain_RestoreSessionOrProject()
    end if    

    LoadExplorerFiles()
    LoadFunctionsFiles()

    function = 0
end function   



' ========================================================================================
' Determine if the incoming character is a brace character
' ========================================================================================
function frmMain_HighlightWord( _
            byval pDoc as clsDocument ptr, _
            byref text as string _
            ) as long

    if pDoc = 0 then exit function
    
    dim as any ptr pSci = pDoc->GetActiveScintillaPtr()
    if pSci = 0 then exit function

    '// Indicators 0-7 could be in use by a lexer
    '// Indicator 8 is used by Find/Replace
    '// Indicator 9 is used for Brace Highlighting
    '// so we'll use indicator 10 to highlight words.
    dim as long NUM = 10

    '// Remove all uses of our Occurrence indicator
    dim as long nLength = SciMsg( pSci, SCI_GETTEXTLENGTH, 0, 0)
    SciMsg( pSci, SCI_SETINDICATORCURRENT, 10, 0)
    SciMsg( pSci, SCI_INDICATORCLEARRANGE, 0, nLength)

    if len(ltrim(text)) = 0 then exit function

    '// Update indicator appearance
    SciMsg( pSci, SCI_INDICSETSTYLE, NUM, INDIC_STRAIGHTBOX )
    SciMsg( pSci, SCI_INDICSETFORE, NUM, ghEditor.ForeColorOccurrence ) 
    SciMsg( pSci, SCI_INDICSETALPHA, NUM, 80 )   

    '// Search the document
    SciMsg( pSci, SCI_TARGETWHOLEDOCUMENT, 0, 0)
    SciMsg( pSci, SCI_SETSEARCHFLAGS, SCFIND_WHOLEWORD, 0)

    dim as long numfound = 0
	dim as long startPos = 0
    dim as long r
    do 
        r = SciMsg( pSci, SCI_SEARCHINTARGET, len(text), cast(LPARAM, strptr(text)))
        if r = -1 then exit do
        
        numfound = numfound + 1
		
		SciMsg( pSci, SCI_SETINDICATORVALUE, NUM, 0 )
        SciMsg( pSci, SCI_INDICATORFILLRANGE, r, len(text))
        startPos = r + len(text)
        
        ' Adjust the searching positions
        SciMsg( pSci, SCI_SETTARGETSTART, startPos, 0)
        SciMsg( pSci, SCI_SETTARGETEND, nLength, 0)
    loop

    '// if only the current word was found then we don't want any highlighting
	'// because the effect would be that the current word highlights as we type.
	'// Remove all uses of our indicator
    if numfound <= 1 then
		nLength = SciMsg( pSci, SCI_GETTEXTLENGTH, 0, 0)
		SciMsg( pSci, SCI_SETINDICATORCURRENT, 10, 0)
		SciMsg( pSci, SCI_INDICATORCLEARRANGE, 0, nLength)
	end if
	
    function = 0
end function


' ========================================================================================
' Set focus to currently active Scintilla window
' ========================================================================================
function frmMain_SetFocusToCurrentCodeWindow() as long
    ' Post a message to the main form CUSTOM handler that will
    ' set focus to the currently active Scintilla code window. We
    ' use PostMessage to ensure that all all other windows 
    ' messages are finished processing.
    PostMessage( HWND_FRMMAIN, MSG_USER_SETFOCUS, 0, 0 )
    function = 0
end Function


' ========================================================================================
' Calculate the height or width size of the document splitter (if applicable)
' Also sets the pDoc-rcSplitButton RECT
' This is used in WM_PAINT via pDoc-rcSplitButton
' ========================================================================================
function frmMain_CalcSplitRect( byval pDoc as clsDocument ptr ) as long
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function

    dim as long nSplitSize
    
    if pDoc->EditorSplitMode = SplitLeftRight then
        dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
        ' Calculate the full RECT if the TOP or LEFT document window is displaying
        if IsWindowVisible( pDoc->hWindow(1) ) then 
            dim as RECT rc
            GetWindowRect( pDoc->hWindow(1), @rc )
            rc.left = rc.right
            if IsWindowVisible( HWND_FRMEDITOR_VSCROLLBAR(1) ) then
                rc.left = rc.left + AfxGetWindowWidth( HWND_FRMEDITOR_VSCROLLBAR(1) )
            end if   
            rc.right = rc.left + pWindow->ScaleX(SPLITSIZE)
            rc.bottom = rc.bottom + pWindow->ScaleY(SCROLLBAR_HEIGHT)
            MapWindowpoints( HWND_DESKTOP, HWND_FRMMAIN, cast( point ptr, @rc), 2 )
            pDoc->rcSplitButton = rc   ' must convert to client coordinates
            nSplitSize = rc.right - rc.left
        end if
    end if

    if pDoc->EditorSplitMode = SplitTopBottom then
        dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
        ' Calculate the full RECT if the TOP or LEFT document window is displaying
        if IsWindowVisible( pDoc->hWindow(1) ) then 
            dim as RECT rc = AfxGetWindowRect(pDoc->hWindow(1))
            rc.top = rc.bottom
            rc.bottom = rc.top + pWindow->ScaleY(SPLITSIZE)
            rc.right = rc.right + pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR)
            MapWindowpoints( HWND_DESKTOP, HWND_FRMMAIN, cast( point ptr, @rc), 2 )
            pDoc->rcSplitButton = rc   ' must convert to client coordinates
            nSplitSize = rc.bottom - rc.top
        end if
    end if

    function = nSplitSize
end function


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as clsDoubleBuffer b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColors( ghMain.BackColor, ghMain.BackColor )
    b.PaintClientRect()

    ' Draw any splitter between the two edit windows
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr
    if pDoc then
        if pDoc->EditorSplitMode <> SplitNone then
            frmMain_CalcSplitRect(pDoc)
            b.SetBackColors( ghEditor.Divider, ghEditor.Divider )
            b.PaintRect( @pDoc->rcSplitButton )
        end if    
    end if
    
    b.EndDoubleBuffer()
    
    function = 0
end function


' ========================================================================================
' Hide the vertical & horizontal editor scrollbars
' ========================================================================================
function frmMain_HideScrollBars() as LRESULT
    ShowWindow( HWND_FRMEDITOR_HSCROLLBAR(0), SW_HIDE )
    ShowWindow( HWND_FRMEDITOR_HSCROLLBAR(1), SW_HIDE )
    ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_HIDE )
    ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_HIDE )
    function = 0
end function


' ========================================================================================
' Hide the horizontal editor scrollbars (done during splitter panel resizing)
' ========================================================================================
function frmMain_HideHorizScrollBars() as LRESULT
    ShowWindow( HWND_FRMEDITOR_HSCROLLBAR(0), SW_HIDE )
    ShowWindow( HWND_FRMEDITOR_HSCROLLBAR(1), SW_HIDE )
    function = 0
end function


' ========================================================================================
' Position Left split document window
' ========================================================================================
function frmMain_PositionSplitDocLeft( byval pDoc as clsDocument ptr, byval rcDoc as RECT ) as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function
    
    dim as long iVScrollbarWidth = AfxGetWindowWidth(HWND_FRMEDITOR_VSCROLLBAR(0)) 
    if IsWindowVisible(HWND_FRMEDITOR_VSCROLLBAR(0)) = 0 then iVScrollbarWidth = 0
    dim as long iHScrollbarHeight = pWindow->ScaleY(SCROLLBAR_HEIGHT)

    ' Always reduce the height of the edit window by the height of the horizontal
    ' scrollbar regardless if that scrollbar is visible. This ensures that the vertical
    ' scrollbar will not be "janky" due to the edit window height being adjusted 
    ' shorter everytime the horizontal scrollbar appears.
    ' Position the document window
    dim as long nWidth = pDoc->SplitX - rcDoc.left - iVScrollbarWidth
    dim as long nHeight = rcDoc.bottom - rcDoc.top
    SetWindowPos( pDoc->hWindow(1), 0, _
                rcDoc.left, rcDoc.top, _
                nWidth, nHeight  - iHScrollbarHeight, _
                SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )

    if IsWindowVisible(HWND_FRMEDITOR_HSCROLLBAR(1)) then
        SetWindowPos( HWND_FRMEDITOR_HSCROLLBAR(1), 0, _ 
                        rcDoc.left, rcDoc.bottom - iHScrollbarHeight, _
                        nWidth, iHScrollbarHeight, _
                        SWP_NOZORDER or SWP_NOREDRAW )
    end if
    
    ' Position the VScrollBar if necessary
    if iVScrollbarWidth then
        SetWindowPos( HWND_FRMEDITOR_VSCROLLBAR(1), 0, _ 
                        rcDoc.left + nWidth, rcDoc.top, _
                        iVScrollbarWidth, nHeight, _ 
                        SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )
        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(1))
    end if
    
    function = 0
end function


' ========================================================================================
' Position Top split document window
' ========================================================================================
function frmMain_PositionSplitDocTop( byval pDoc as clsDocument ptr, byval rcDoc as RECT ) as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function

    dim as long iVScrollbarWidth = AfxGetWindowWidth(HWND_FRMEDITOR_VSCROLLBAR(0)) 
    dim as long iHScrollbarHeight = pWindow->ScaleY(SCROLLBAR_HEIGHT)

    ' Always reduce the height of the edit window by the height of the horizontal
    ' scrollbar regardless if that scrollbar is visible. This ensures that the vertical
    ' scrollbar will not be "janky" due to the edit window height being adjusted 
    ' shorter everytime the horizontal scrollbar appears.
    ' Position the document window
    SetWindowPos( pDoc->hWindow(1), 0, _
                rcDoc.left, rcDoc.top, _
                rcDoc.right - rcDoc.left - iVScrollbarWidth, _
                pDoc->SplitY - rcDoc.top - iHScrollbarHeight, _ 
                SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )

    ' Position the TOP view horizontal scroll bar
    if IsWindowVisible(HWND_FRMEDITOR_HSCROLLBAR(1)) then
        SetWindowPos( HWND_FRMEDITOR_HSCROLLBAR(1), 0, _ 
                        rcDoc.left, pDoc->SplitY - iHScrollbarHeight, _
                        rcDoc.right - rcDoc.left - iVScrollbarWidth, iHScrollbarHeight, _
                        SWP_NOZORDER )
    end if
    
    ' Position the TOP view vertical scroll bar
    ' Determine if the VScrollBar should be displayed
    if pDoc->GetLineCount > pDoc->LinesPerPage(1) then
        SetWindowPos( HWND_FRMEDITOR_VSCROLLBAR(1), 0, _ 
                        rcDoc.right - iVScrollbarWidth, _
                        rcDoc.top, _
                        iVScrollbarWidth, _
                        AfxGetWindowHeight(pDoc->hWindow(1)), _ 
                        SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )
        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(1))
    end if
    
    function = 0
end function


' ========================================================================================
' Position the MAIN editing document to the right
' ========================================================================================
function frmMain_PositionMainDocRight( byval pDoc as clsDocument ptr, byval rcDoc as RECT ) as LRESULT
    ' Position the MAIN editing document taking into account any split windows
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function
    
    dim as long nSplitSize = frmMain_CalcSplitRect(pDoc)
    dim as long nLeft = Max( rcDoc.left, pDoc->SplitX + nSplitSize )

    dim as long iVScrollbarWidth = AfxGetWindowWidth(HWND_FRMEDITOR_VSCROLLBAR(0)) 
    if IsWindowVisible(HWND_FRMEDITOR_VSCROLLBAR(0)) = 0 then iVScrollbarWidth = 0
    dim as long iHScrollbarHeight = pWindow->ScaleY(SCROLLBAR_HEIGHT)

    ' Always reduce the height of the edit window by the height of the horizontal
    ' scrollbar regardless if that scrollbar is visible. This ensures that the vertical
    ' scrollbar will not be "janky" due to the edit window height being adjusted 
    ' shorter everytime the horizontal scrollbar appears.
    dim as long nWidth = rcDoc.right - nLeft - iVScrollbarWidth
    dim as long nHeight = rcDoc.bottom - rcDoc.top
    SetWindowPos( pDoc->hWindow(0), 0, _
                nLeft, rcDoc.top, _
                nWidth, nHeight - iHScrollbarHeight, _
                SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )

    if IsWindowVisible(HWND_FRMEDITOR_HSCROLLBAR(0)) then
        SetWindowPos( HWND_FRMEDITOR_HSCROLLBAR(0), 0, _ 
                        nLeft, rcDoc.bottom - iHScrollbarHeight, _
                        nWidth, iHScrollbarHeight, _
                        SWP_NOZORDER )
    end if
    
    if iVScrollbarWidth then 
        SetWindowPos( HWND_FRMEDITOR_VSCROLLBAR(0), 0, _ 
                    nLeft + nWidth, rcDoc.top, _
                    iVScrollbarWidth, nHeight, _
                    SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )
        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(0))
    end if

    function = 0
end function


' ========================================================================================
' Position the MAIN editing document to the bottom
' ========================================================================================
function frmMain_PositionMainDocBottom( byval pDoc as clsDocument ptr, byval rcDoc as RECT ) as LRESULT
    ' Position the MAIN editing document taking into account any split windows
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function

    dim as long nTop = Max( rcDoc.top, pDoc->SplitY )

    dim as long iVScrollbarWidth = AfxGetWindowWidth(HWND_FRMEDITOR_VSCROLLBAR(0)) 
    dim as long iHScrollbarHeight = pWindow->ScaleY(SCROLLBAR_HEIGHT)

    ' Always reduce the height of the edit window by the height of the horizontal
    ' scrollbar regardless if that scrollbar is visible. This ensures that the vertical
    ' scrollbar will not be "janky" due to the edit window height being adjusted 
    ' shorter everytime the horizontal scrollbar appears.
    SetWindowPos( pDoc->hWindow(0), 0, _
                rcDoc.left, nTop, _
                rcDoc.right - rcDoc.left - iVScrollbarWidth, _
                rcDoc.bottom - nTop - iHScrollbarHeight, _
                SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )

    ' Position the MAIN view horizontal scroll bar
    if IsWindowVisible(HWND_FRMEDITOR_HSCROLLBAR(0)) then
        SetWindowPos( HWND_FRMEDITOR_HSCROLLBAR(0), 0, _ 
                    rcDoc.left, rcDoc.bottom - iHScrollbarHeight, _
                    rcDoc.right - rcDoc.left - iVScrollbarWidth, iHScrollbarHeight, _
                    SWP_NOZORDER )
    end if

    ' Position the MAIN view vertical scroll bar
    ' Determine if the VScrollBar should be displayed
    if pDoc->GetLineCount > pDoc->LinesPerPage(0) then
        SetWindowPos( HWND_FRMEDITOR_VSCROLLBAR(0), 0, _ 
                    rcDoc.right - iVScrollbarWidth, nTop, _
                    iVScrollbarWidth, _
                    AfxGetWindowHeight(pDoc->hWindow(0)) + iHScrollbarHeight, _ 
                    SWP_SHOWWINDOW or SWP_NOZORDER or SWP_NOOWNERZORDER or SWP_NOACTIVATE )
        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(0))
    else
        ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_HIDE )
    end if

    function = 0
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmMain_PositionWindows() as LRESULT
    dim as HWND hEdit
    dim as long nHeightTabControl, nHeightTabInfoControl, nLeft, nTop
    dim as RECT rc

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    
    ' Get the entire client area
    GetClientRect( HWND_FRMMAIN, @rc )
    
    dim as long nHeightMenuBar   = AfxGetWindowHeight( HWND_FRMMAIN_MENUBAR )
    dim as long nHeightStatusBar = AfxGetWindowHeight( HWND_FRMMAIN_STATUSBAR )
    dim as long nHeightOutput    = AfxGetWindowHeight( HWND_FRMOUTPUT )
    dim as long nWidthPanel      = AfxGetWindowWidth( HWND_FRMPANEL )

    ' Set the top menu menubar into place 
    SetWindowPos( HWND_FRMMAIN_MENUBAR, 0, _
                    0, 0, rc.right - rc.left, nHeightMenuBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW ) 

    ' Set the statusbar into place 
    SetWindowPos( HWND_FRMMAIN_STATUSBAR, 0, _
                    0, rc.bottom - nHeightStatusBar, rc.right - rc.left, nHeightStatusBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW ) 
    
    nLeft = rc.Left 
    nTop = rc.top + nHeightMenuBar
    
    ' Set the Panel pane into place if applicable
    if IsWindowVisible(HWND_FRMPANEL) then
        if gConfig.ExplorerPositionRight then nLeft = rc.right - nWidthPanel
        SetWindowPos( HWND_FRMPANEL, 0, _
                    nLeft, nTop, _
                    nWidthPanel, _
                    rc.Bottom - nHeightStatusBar - nHeightMenuBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW ) 
        frmPanel_PositionWindows
        nLeft = nWidthPanel
        if gConfig.ExplorerPositionRight then nLeft = rc.left
    Else
        nWidthPanel = 0
    end if


    ' if items exist in the top tabcontrol then show the tab control and account for its height
    if gTTabCtl.GetItemCount = 0 then
        ShowWindow( HWND_FRMMAIN_TOPTABS, SW_HIDE )
        ShowWindow( HWND_FRMMAIN_TOPTABSINFO, SW_HIDE )
        nHeightTabControl = 0
        nHeightTabInfoControl = 0
    else
        nHeightTabControl = AfxGetWindowHeight(HWND_FRMMAIN_TOPTABS)
        SetWindowPos( HWND_FRMMAIN_TOPTABS, 0, _
                    nLeft, nTop, _
                    rc.Right - nWidthPanel, nHeightTabControl, _
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        nHeightTabInfoControl = pWindow->ScaleY(gTTabCtl.InfoPanelHeight)
        SetWindowPos( HWND_FRMMAIN_TOPTABSINFO, 0, _
                    nLeft, nTop + nHeightTabControl, _     
                    rc.Right - nWidthPanel, nHeightTabInfoControl, _  
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        frmTopTabs_PositionWindows()   ' also calls to frmTopTabsInfo_PositionWindows
    end if
    

    ' Set the Output pane into place if applicable
    if IsWindowVisible(HWND_FRMOUTPUT) then
        SetWindowPos( HWND_FRMOUTPUT, 0, _
                    nLeft, rc.Bottom - nHeightStatusbar - nHeightOutput, _
                    rc.Right - nLeft, _
                    nHeightOutput, _
                    SWP_NOZORDER or SWP_SHOWWINDOW ) 
        frmOutput_PositionWindows()
    else
        nHeightOutput = 0
    end if

    if pDoc = 0 then frmMain_HideScrollBars() 
    
    if pDoc then 
        ' Position the Scintilla child edit windows
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
        dim as RECT rcDoc
        
        ' Reduce the height of the client area by the size of the statusbar and menubar.
        rcDoc.left   = nLeft
        rcDoc.top    = nTop + nHeightTabControl + nHeightTabInfoControl 
        rcDoc.right  = rc.right
        rcDoc.bottom = rc.bottom - nHeightStatusBar - nHeightOutput
        if gConfig.ExplorerPositionRight then rcDoc.right = rc.right - nWidthPanel
        pDoc->rcDocSplitArea = rcDoc

        
        if pDoc->EditorSplitMode = SplitNone then 
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW )
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_HIDE )
            ShowWindow( pDoc->hWindow(1), SW_HIDE )      
            frmMain_PositionMainDocBottom(pDoc, rcDoc)
        end if
        if pDoc->EditorSplitMode = SplitLeftRight then
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW )
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_SHOW )
            frmMain_PositionSplitDocLeft(pDoc, rcDoc)
            frmMain_PositionMainDocRight(pDoc, rcDoc)
        end if
        if pDoc->EditorSplitMode = SplitTopBottom then
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW )
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_SHOW )
            frmMain_PositionSplitDocTop(pDoc, rcDoc)
            frmMain_PositionMainDocBottom(pDoc, rcDoc)
        end if

            
        ' Update editor vertical scrollbars
        frmEditorVScroll_calcVThumbRect( pDoc )
        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(0))
        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(1))
        
        ' Esure that the splitter paints
        AfxRedrawWindow(HWND_FRMMAIN)
    end if

    ' Ensure that the correct notes are shown
    frmOutput_ShowNotes()

    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    ' delete any previous temporary quick run files should they exist
    gApp.wszQuickRunFilename = AfxGetExePathName & QUICKRUNEXE
    AfxDeleteFile( gApp.wszQuickRunFilename )

    ' Enable drag and drop files
    DragAcceptFiles hwnd, CTRUE
    
    '  Message cracker macro expects a true to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end Function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmMain_PositionWindows
    end if
    function = 0
end Function


' ========================================================================================
' Process WM_ACTIVATEAPP message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnActivateApp( _
            byval hwnd as HWND, _
            byval fActivate as boolean, _
            byval dwThreadId as DWORD _
            ) as LRESULT

    ' Reset top menubar active selection if we lose or gain app focus.
    ' killAllPopupMenus resets gActiveMenuBarIndex and redraws menubar
    killAllPopupMenus()   
    
    ' If app is losing focus then there is nothing we need to do
    if fActivate = false then exit function

    if gApp.GetDocumentCount = 0 then exit function

    ' Re-entry guard
    static bInActivateApp as boolean
    if bInActivateApp then exit function
    bInActivateApp = true

    ' App is gaining focus. 
    ' (1) Check for files that may have been moved or delete.
    ' (2) Check if files modified by external means.
    
    
    ' (1) Check for files that may have been moved or delete.
    dim pDoc as clsDocument ptr = gApp.pDocList
    dim pDocNext as clsDocument ptr
    
    do
        pDoc = gApp.pDocList
        do until pDoc = 0
            pDocNext = pDoc->pDocNext
            
            if pDoc->IsNewFlag then
                pDoc = pDocNext: continue do
            end if

            if AfxFileExists(pDoc->DiskFilename) = false then
                ' Ensure that the file is open and displayed
                OpenSelectedDocument( pDoc->DiskFilename )
                MessageBox( HWND_FRMMAIN, _
                                pDoc->DiskFilename & vbCrLf & _ 
                                L(286, "This document has been deleted or is no longer available."), _
                                L(267, "File Changed"), MB_ICONINFORMATION or MB_OK)
                ' Close the file or remove from project.
                if gApp.IsProjectActive then 
                    OnCommand_ProjectRemove( 0, pDoc )  ' the first param (id) is not used.
                else
                    ' This can not be an IsNew file so we don't have to worry about the pDoc
                    ' becoming invalid during the FileClose.
                    OnCommand_FileClose( HWND_FRMMAIN, EFC_CLOSECURRENT )
                end if    
            end if    

            pDoc = pDocNext
        loop
    loop until pDoc = 0


    ' (2) Check if files modified by external means.
    pDoc = gApp.pDocList

    do until pDoc = 0
        ' Compare the disk file date time to the value currently stored in document class. 
        ' Bypass any 'new' untitled files.
        dim as FILETIME ft = AfxGetFileLastWriteTime(pDoc->DiskFilename)
        
        if (AfxFileTimeToVariantTime(ft) <> AfxFileTimeToVariantTime(pDoc->DateFileTime)) andalso _
            (pDoc->IsNewFlag <> true) then
            
            OpenSelectedDocument( pDoc->DiskFilename )
            dim as long res = MessageBox( HWND_FRMMAIN, _
                            pDoc->DiskFilename & vbCrLf & _ 
                            L(266, "File was changed by another application. Reload it?"), _
                            L(267, "File Changed"), MB_ICONQUESTION or MB_YESNOCANCEL) 
                
            if res = IDCANCEL then
                bInActivateApp = false
                exit function
            end if
            
            if res = IDYES then
                ReloadDocument(pDoc->DiskFilename)
            end if

        end if
        pDoc->DateFileTime = AfxGetFileLastWriteTime( pDoc->DiskFilename )
        pDoc = pDoc->pDocNext
    loop

    bInActivateApp = false

    function = 0
end Function


' ========================================================================================
' Process WM_CONTEXTMENU message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnContextMenu( _
            byval hwnd as HWND, _
            byval hwndContext as HWND, _
            byval xPos as long, _  
            byval yPos as long _
            ) as LRESULT
    dim hPopupMenu as HMENU 
    dim pt         as point 
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0

    SetFocus hWndContext
    pt.x = xPos
    pt.y = yPos
    ScreenToClient hWndContext, @pt
    hPopupMenu = CreateScintillaContextMenu()
    GetCursorPos @pt
    TrackPopupMenu hPopupMenu, 0, pt.x, pt.y, 0, hwnd, 0
    DestroyMenu hPopupMenu

    function = 0
end Function


' ========================================================================================
' Process WM_DROPFILES message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnDropFiles( _
            byval hwnd as HWND, _
            byval hDrop as HDROP _
            ) as LRESULT

    ' Get the number of dropped files
    dim as long nCount = DragQueryFile(hDrop, &HFFFFFFFF, Null, 0)
    if nCount = 0 then exit function
    
    dim as long i, nLen
    dim wszPath as wstring * MAX_PATH
    dim wszFiles(any) as DWSTRING

    for i = 0 To nCount - 1
        nLen = DragQueryFile(hDrop, i, @wszPath, MAX_PATH)
        ' Make sure it's a file, not a folder
        dim fd as WIN32_FIND_DATAW
        dim hFind as HANDLE = FindFirstFileW(@wszPath, @fd)
        if hFind <> INVALID_HANDLE_VALUE then
            FindClose hFind
            if (fd.dwFileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> FILE_ATTRIBUTE_DIRECTORY then
                dim as integer ub = ubound(wszFiles) + 1
                redim preserve wszFiles(ub)
                wszFiles(ub) = wszPath
            end if
        end if
    Next
    
    DragFinish hDrop

    if frmMain_ProcessFilesArray(hwnd, wszFiles()) then
        LoadExplorerFiles()
        LoadFunctionsFiles()
    end if
    
    function = 0
end Function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnClose( byval hwnd as HWND ) as LRESULT

    ' if configuration option to confirm closing editor is active then ask now.
    if gConfig.AskExit then
        if MessageBox( HWND_FRMMAIN, L(275,"Are you sure you want to exit?"), L(276,"Confirm"), _
                    MB_YESNOCANCEL or MB_ICONQUESTION or MB_DEFBUTTON1 ) <> IDYES then
            return true
        end if            
    end if
    
    ' Set global shutdown flag that will bypass removing nodes from project
    ' treeview, etc. Those tasks simply slow down the exiting of the program.
    gApp.IsShutdown = true

    ' Save whether the Side panel should be shown the next time the program is run.
    ' Also save the panel width. 
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    gConfig.ShowPanel = IsWindowVisible(HWND_FRMPANEL)
    gConfig.ShowPanelWidth = pWindow->UnScaleX(AfxGetWindowWidth( HWND_FRMPANEL ))

    ' Save whether the Output panel should be shown the next time the program is run.
    ' Also save the panel height. 
    gConfig.ShowOutputPanel = IsWindowVisible(HWND_FRMOUTPUT)
    gConfig.ShowOutputPanelHeight = pWindow->UnScaleY(gConfig.ShowOutputPanelHeight)
    'gConfig.ShowOutputPanelMinimized this is set when double clicking an Output panel tab.
    gConfig.ShowOutputPanelIndex = gOutputTabsCurSel
    
    ' if a project is already open then save/close it.
    if gApp.IsProjectActive then 
        if OnCommand_ProjectClose(hwnd) = false then return 0
    elseif gApp.IsSessionActive then 
        ' if a session is already open then save/close it.
        if gConfig.SaveSessionFile(gApp.SessionFilename) = false then return 0
    else
        ' Save a default session file that will open if Settings option checked to load
        ' last project/session but no last used project or session exits.
        if gConfig.SaveDefaultSessionFile() = 0 then return 0
    end if
    
    ' Close any open files asking to save any that are dirty 
    if OnCommand_FileClose(HWnd, EFC_CLOSEALL) = 0 then return 0

    DestroyWindow(hwnd)
    
    function = 0
end Function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT

    ' no more config changed checks while shutting down
    gApp.PreventConfigLoad = true   

    ' output the config settings to disk file
    gConfig.SaveConfigFile

    ' delete any temporary quick run files should they exist
    ' filename set in compile process and could be different location than the
    ' default filename as tested for in wm_create.
    if AfxFileExists(gApp.wszQuickRunFilename) then AfxDeleteFile( gApp.wszQuickRunFilename )
    
    if gApp.SpinnerTimerID <> 0 then
        KillTimer( HWND_FRMMAIN, gApp.SpinnerTimerID )
    end if

    ' Disable drag and drop files
    DragAcceptFiles hwnd, false
    
    PostQuitMessage(0)
    
    function = 0
end Function


' ========================================================================================
' Process WM_MOUSEMOVE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnMouseMove( _
            byval hwnd as HWND, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    ' HITTEST (DOCUMENT SPLITTER)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
        if pWindow = 0 then exit function
        
        dim as Rect rc
        dim as point pt = (x, y)
        if pDoc->bSizing then
            dim as long nSplitSize = pWindow->ScaleY(SPLITSIZE)
            dim as long nScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR)
            dim as long maxLeft = pDoc->rcDocSplitArea.left + nScrollWidth
            dim as long maxTop = pDoc->rcDocSplitArea.top 
            dim as long maxRight = pDoc->rcDocSplitArea.right - nScrollWidth - nSplitSize
            dim as long maxBottom = pDoc->rcDocSplitArea.bottom - nSplitSize
            if (pDoc->EditorSplitMode = SplitTopBottom) andalso (pt.y <> pDoc->ptPrev.y) then
                pDoc->SplitY = pDoc->SplitY + (pt.y - pDoc->ptPrev.y)
                ' Don't let the split go past the top or bottom of the edit area
                if pDoc->SplitY <= maxTop then pDoc->SplitY = maxTop
                if pDoc->SplitY >= maxBottom then pDoc->SplitY = maxBottom
            end if   
            if (pDoc->EditorSplitMode = SplitLeftRight) andalso (pt.x <> pDoc->ptPrev.x) then
                pDoc->SplitX = pDoc->SplitX + (pt.x - pDoc->ptPrev.x)
                ' Don't let the split go past the left or right of the edit area
                if pDoc->SplitX <= maxLeft then pDoc->SplitX = maxLeft
                if pDoc->SplitX >= maxRight then pDoc->SplitX = maxRight
            end if   
            frmMain_PositionWindows
            pDoc->ptPrev = pt
        else
            frmMain_CalcSplitRect(pDoc)
            if PtInRect(@pDoc->rcSplitButton, pt) then 
                SetCursor( iif(pDoc->EditorSplitMode = SplitTopBottom, ghCursorSizeNS, ghCursorSizeWE ))
            end if   
        end if
    end if
            
    function = 0
end function


' ========================================================================================
' Process WM_LBUTTONDOWN message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnLButtonDown( _
            byval hwnd as HWND, _
            byval fDoubleClick as boolean, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long
                                        
    ' if main window area clicked then close any active top menus
    killAllPopupMenus()
    
    ' HITTEST (DOCUMENT SPLITTER)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        dim as point pt = (x, y)
        if PtInRect(@pDoc->rcSplitButton, pt) then 
            pDoc->bSizing = true
            frmMain_HideHorizScrollBars()   ' hide to reduce flickering
                     
            if pDoc->EditorSplitMode = SplitTopBottom then
                pDoc->SplitY = pDoc->rcSplitButton.top  
                pDoc->ptPrev.y = pt.y
                SetCursor(ghCursorSizeNS)
            end if
            if pDoc->EditorSplitMode = SplitLeftRight then
                pDoc->SplitX = pDoc->rcSplitButton.left  
                pDoc->ptPrev.x = pt.x
                SetCursor(ghCursorSizeWE)
            end if

            SetCapture( HWND_FRMMAIN )
        end if
    end if

    function = 0
end function


sub frmMain_HandleSingleClick(hwnd as HWND)
    ' HITTEST (DOCUMENT SPLITTER)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        if pDoc->bSizing then
            pDoc->bSizing = false
            frmMain_PositionWindows
            ReleaseCapture
        end if
    end if   
    SetCursor( LoadCursor( null, IDC_ARROW ))
end sub


sub frmMain_HandleDoubleClick(hwnd as HWND)
    ' HITTEST (DOCUMENT SPLITTER)
    ' Are we over a split edit area (toggle off the split)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        dim as point pt = pDoc->ptClick 
        if PtInRect(@pDoc->rcSplitButton, pt) then 
            pDoc->bSizing = false
            pDoc->SplitX = 0
            pDoc->SplitY = 0
            pDoc->EditorSplitMode = SplitNone
            ReleaseCapture
            frmMain_PositionWindows
        end if
    end if
end sub


' ========================================================================================
' Timer callback function for differentiating between single/double click
' ========================================================================================
sub frmMain_TimerProc(hwnd as HWND, uMsg as UINT, idEvent as UINT_PTR, dwTime as DWORD)
    KillTimer(hwnd, idEvent)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        if pDoc->doubleClickReceived = false then
            frmMain_HandleSingleClick(hwnd)
        end if
    end if
end sub

            
' ========================================================================================
' Process WM_LBUTTONUP message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnLButtonUp( _
            byval hwnd as HWND, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        pDoc->ptClick.x = x
        pDoc->ptClick.y = y
        ' Start a timer to delay single-click action and distinguish between
        ' the single click and doubleclick
        pDoc->doubleClickReceived = false
        SetTimer(hwnd, pDoc->idTimer, 100, @frmMain_TimerProc)
        ReleaseCapture
    end if
            
    function = 0
end function


' ========================================================================================
' Process WM_LBUTTONDBLCLK message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnLButtonDblClk( _
            byval hwnd as HWND, _
            byval fDoubleClick as boolean, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    if gApp.bDragActive then exit function

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        KillTimer(hwnd, pDoc->idTimer)
        pDoc->doubleClickReceived = true
        frmMain_HandleDoubleClick(hwnd)
    end if
                
    function = 0
end function


' ========================================================================================
' frmMain Window procedure
' ========================================================================================
function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,        frmMain_OnCreate)
        HANDLE_MSG (hwnd, WM_PAINT,         frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_SIZE,          frmMain_OnSize)
        HANDLE_MSG (hwnd, WM_CLOSE,         frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmMain_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,       frmMain_OnCommand)
        HANDLE_MSG (hwnd, WM_NOTIFY,        frmMain_OnNotify)
        HANDLE_MSG (hwnd, WM_CONTEXTMENU,   frmMain_OnContextMenu)
        HANDLE_MSG (hwnd, WM_ACTIVATEAPP,   frmMain_OnActivateApp)
        HANDLE_MSG (hwnd, WM_DROPFILES,     frmMain_OnDropFiles)
        HANDLE_MSG (hwnd, WM_LBUTTONDOWN,   frmMain_OnLButtonDown)
        HANDLE_MSG (hwnd, WM_LBUTTONUP,     frmMain_OnLButtonUp)
        HANDLE_MSG (hwnd, WM_MOUSEMOVE,     frmMain_OnMouseMove)
        HANDLE_MSG (hwnd, WM_LBUTTONDBLCLK, frmMain_OnLButtonDblClk)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN = 0
        return 0

        case WM_ERASEBKGND
            return true      ' prevents painting the background
            
        case WM_TIMER
            ' Handle any Spinner (used when compiling)
            if wParam = gApp.SpinnerTimerID then
                ' Update Spinner display frame 
                gApp.SpinnerCurrentFrame = (gApp.SpinnerCurrentFrame + 1) mod 8
                frmMain_SetStatusbar()
                AfxRedrawWindow( HWND_FRMMAIN_STATUSBAR )
            end if
            
        case WM_NCACTIVATE
            ' if wParam is false then the system wants to draw an inactive title bar. we
            ' will prevent this action if a popup menu from the menubar is active.
            if wParam = false then
                if gPrevent_WM_NCACTIVATE then return false
            end if
            
        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( HWND, WM_CLOSE, wParam, lParam )
                exit function
            end if
            
        case WM_SETCURSOR
            if gApp.bDragActive = true then
                    ' Either the horiz or vert splitter bar is being resized. We handle
                    ' setting the mouse cursor in the splitter_mousemove function.
                    return true
                
                elseif gApp.bDragTabActive = true then 
                    ' We handle the cursor in the TabControl subclass. Don't allow the
                    ' main window to change our cursor. return true.
                    return true

                elseif (gApp.IsCompiling = true) or _
                    (gApp.IsProjectLoading = true) or _
                    (gApp.IsFileLoading = true) then
                    SetCursor( LoadCursor(0, IDC_WAIT) )
                    return true
            end if
            
        case WM_CAPTURECHANGED
            gApp.bDragTabActive = false
            
            
        case WM_COPYDATA      ' used during processing of commandline
            dim pDataToGet as COPYDATASTRUCT ptr = cast(COPYDATASTRUCT ptr, lParam)
            if pDataToGet then
                if pDataToGet->dwData = IDM_COPYDATA_COMMANDLINE then  
                    dim as DWSTRING wszCommandLine = *cast(wstring ptr, pDataToGet->lpData)
                    
                    dim as integer numfiles = AfxStrParseCount(wszCommandLine, ";")
                    dim wszFiles(numfiles) as DWSTRING

                    for i as integer = 1 to numfiles
                        wszFiles(i) = AfxStrParse(wszCommandLine, i, ";")
                    next
                    if frmMain_ProcessFilesArray(hwnd, wszFiles()) then
                        LoadExplorerFiles()
                        LoadFunctionsFiles()
                    end if
                end if
            end if
            

        ''  CUSTOM MESSAGES

        case MSG_USER_TOPTABS_CHANGING
            ' Hide the current tab
            gTTabCtl.DisplayScintilla( gTTabCtl.CurSel, false )
            exit function
            
        case MSG_USER_TOPTABS_CHANGED
            ' Show the new tab
            gTTabCtl.DisplayScintilla( gTTabCtl.CurSel, true )
            frmMain_PositionWindows()
            dim pDoc as clsDocument ptr = gTTabCtl.tabs(gTTabCtl.CurSel).pDoc
            if pDoc then
                AfxRedrawWindow( pDoc->hWindow(0) )  ' makes the code window display quicker especially when loading Project.
                AfxRedrawWindow( pDoc->hWindow(1) )  ' makes the code window display quicker especially when loading Project.
            end if
            ' We have moved to a new Tab. If Find is open then highlight any search phrase.   
            FindReplace_HighlightSearches()
            ' Set the focus to the Scintilla window. This will call frmMain_SetStatusbar.
            frmMain_SetFocusToCurrentCodeWindow()
            exit function


        case WM_SETFOCUS
            frmMain_SetFocusToCurrentCodeWindow

        case MSG_USER_SETFOCUS
            ' Set focus to current Scintilla window and update the document
            ' display such as Line#, Col#, Filename, etc.
            frmMain_SetStatusbar()
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then SetFocus( pDoc->hWndActiveScintilla )

        case MSG_USER_PROCESS_COMMANDLINE 
            ' process any command line arguments that may have been passed to the program.
            frmMain_ProcessCommandLine( hwnd )

        case MSG_USER_PROCESS_STARTUPUSERTOOLS
            ' process any User Tools that must display immediately after the main editor
            ' user interface is created and displayed.
            
            ' Wait a bit to ensure that the editor window is visible... This loop
            ' is mostly never necessary but better to check just to be consistent.
            dim as double nStartTime = timer
            do
                if IsWindowVisible( HWND_FRMMAIN ) then exit do
                ' if we've waited more than 5 seconds then break out to avoid an infite loop.
                if (timer - nStartTime) > 5 then exit do
            loop

            ' Only invoke any User Tools that have action for startup
            for y as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
                if gConfig.Tools(y).Action = USERTOOL_ACTION_EDITORSTARTUP then
                    frmUserTools_ExecuteUserTool(null, y )
                end if   
            next

        case MSG_USER_PROCESS_CHECKFORUPDATE
            ' Check for any Tiko updated version (use a separate thread in case check hangs)
            if gConfig.CheckUpdates = false then exit function
            dim as any ptr pThread = ThreadCreate( @DoCheckForUpdates )
            ThreadWait( pThread )
            frmMenuBar_PositionWindows()

        case MSG_USER_SHOWAUTOCOMPLETE
            return ShowAutocompleteList()

        case MSG_USER_APPENDEQUALSSIGN
            ' The = key was used to terminate a popup autocomplete. Take that
            ' character and format it with a space before and a space after.
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                dim as hwnd hEdit = pDoc->hWndActiveScintilla
                dim as long nPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0)
                dim as string sWord = " = "
                SciExec( hEdit, SCI_SETSEL, nPos-1, nPos)
                SciExec( hEdit, SCI_REPLACESEL, 0, cast(LPARAM, strptr(sWord)))
                nPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0)
                SciExec( hEdit, SCI_SETSEL, nPos, nPos)
            end if   
                
    end select 

    ' for messages that we don't deal with
    function = DefWindowProc(HWnd, uMsg, wParam, lParam)

end Function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)
    
    ghFont(GUIFONT_9)     = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL) 
    ghFont(GUIFONT_10)    = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(INFOFONT_11)   = pWindow->CreateFont(GUIFIXEDFONT, 11, FW_NORMAL)
    ghFont(SYMBOLFONT_9)  = pWindow->CreateFont(SYMBOLFONT, 9, FW_NORMAL)
    ghFont(SYMBOLFONT_10) = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_12) = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( null, APPNAME, @frmMain_WndProc )
    
    ' Load the currently selected theme (this needs a valid pWindow HWND_FRMMAIN to exists)
    LoadThemeFile()
    
    ' Set the small and large icon for the main window (must be set after main window is created)
    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' Load the good and bad compile "icons"
    ghIconGood = 1  
    ghIconBad  = 2 
    
    ' Load the North/South and East/West cursor images
    ghCursorSizeNS = LoadImage( Null, MAKEINTRESOURCEW(OCR_SIZENS), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE or LR_SHARED )
    ghCursorSizeWE = LoadImage( Null, MAKEINTRESOURCEW(OCR_SIZEWE), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE or LR_SHARED )

    ' Build the keyboard accelerators
    frmKeyboard_BuildAcceleratorTable()

    ' Create the main application topmenu menubar
    frmMenuBar_Show( HWND_FRMMAIN )

    ' Create the main application top tabs control
    frmTopTabs_Show( HWND_FRMMAIN )
    frmTopTabsInfo_Show( HWND_FRMMAIN )

    ' Create the main application statusbar
    frmStatusBar_Show( HWND_FRMMAIN )

    ' Create the various child windows
    frmPanel_Show( HWND_FRMMAIN )
    frmOutput_Show( HWND_FRMMAIN )
    frmEditorHScroll_Show( HWND_FRMMAIN )
    frmEditorVScroll_Show( HWND_FRMMAIN )

    ' Create the UserTools accelerator table
    frmUserTools_CreateAcceleratorTable()            

    ' Create the Build Configurations accelerator table
    frmBuildConfig_CreateAcceleratorTable()            
    
    ' SET STARTUP POSITION
    ' if no valid window size exists then set to the default working area of the screen
    if (gConfig.StartupRight = 0) orelse (gConfig.StartupBottom = 0) then
        ' Retrieve the size of the working area
        dim rc as Rect = pWindow->GetWorkArea                
        gConfig.StartupRight  = rc.Right
        gConfig.StartupBottom = rc.Bottom
    end if
    
    dim WinPla as WINDOWPLACEMENT
    with WinPla
        .Length = sizeof(WinPla)
        .rcNormalPosition.Left   = gConfig.StartupLeft
        .rcNormalPosition.Top    = gConfig.StartupTop
        .rcNormalPosition.Right  = gConfig.StartupRight
        .rcNormalPosition.Bottom = gConfig.StartupBottom
        .showCmd = iif( gConfig.StartupMaximized, SW_MAXIMIZE, SW_SHOWNORMAL )
    end with
    SetWindowPlacement( pWindow->hWindow, @WinPla )
        
    ' Ensure the window is placed on screen should the user had changed 
    ' the logical ordering of a multiple display setup.
    AfxForceVisibleDisplay( pWindow->hWindow )
    
    UpdateWindow( pWindow->hWindow )

    ' Post a message to process the application's command line as applicable.
    PostMessage( pWindow->hWindow, MSG_USER_PROCESS_COMMANDLINE, 0, 0 )

    ' Post a message to process any User Tools that must start after the editor displays.
    PostMessage( pWindow->hWindow, MSG_USER_PROCESS_STARTUPUSERTOOLS, 0, 0 )

    ' Post a message to check for any Tiko updated version.
    PostMessage( pWindow->hWindow, MSG_USER_PROCESS_CHECKFORUPDATE, 0, 0 )

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        if handleMouseShowScrollBar(uMsg) then continue do
        if handleMouseTopMenu(uMsg) then continue do
        if handleAltKeyMenuBar(uMsg) then continue do

        ' Close with ESC key: Find/Replace, FindInFiles, FunctionList window
        if handleEscKeyModeless(uMsg) then continue do

        ' Handle ENTER key for active FindReplace dialog
        if handleKeysFindReplace(uMsg) then continue do
        
        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
         if (ghAccelUserTools = 0) orelse (TranslateAccelerator(pWindow->hWindow, ghAccelUserTools, @uMsg) = 0) then 
          if (ghAccelBuildConfigurations = 0) orelse (TranslateAccelerator(pWindow->hWindow, ghAccelBuildConfigurations, @uMsg) = 0) then 

            ' Prevent any < asc(32) characters from making their way to Scintilla where they 
            ' get shown as an embedded control graphic. Allow backspace.
            if (uMsg.message = WM_CHAR) then
                if (uMsg.wParam < 32) andalso (uMsg.wParam <> 8) then continue do
                ' Save the pdoc->LastCharTyped to use in our autocomplete popups
                dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
                If pDoc then pDoc->LastCharTyped = uMsg.wParam
            end if

            ' Save the pdoc->LastCharTyped to use in our autocomplete popups
            if (uMsg.message = WM_KEYDOWN) then
                dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
                if pDoc then pdoc->LastCharTyped = uMsg.wParam
            end if
            
            ' if topmenus or menubar are active then process all keyboard input for them rather than 
            ' passing it off to the system to process.
            if handleKeysTopMenu(uMsg) then continue do

            ' Was ENTER key pressed while scrolling the Explorer listbox via keyboard
            if handleKeysExplorerListBox(uMsg) then continue do
            
            ' Check if the settings.ini file was changed by an external program.
            ' Reload the config file in case a user has automated a change to it since
            ' the application started (for example, changing the compiler path).
            gConfig.ReloadConfigFileTest()
            
            if IsDialogMessage(pWindow->hWindow, @uMsg) = 0 then
                TranslateMessage @uMsg    ' Translates virtual-key messages into character messages.
                DispatchMessage @uMsg     ' Dispatches a message to a window procedure.
            end if

          end if  ' accelerators build configurations
         end if  ' accelerators usertools 
        end if  ' accelerators
    loop  ' message loop
    
    function = uMsg.wParam


    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

    if ghAccelUserTools then DestroyAcceleratorTable( ghAccelUserTools )
    if ghAccelBuildConfigurations then DestroyAcceleratorTable( ghAccelBuildConfigurations )
    
end function

