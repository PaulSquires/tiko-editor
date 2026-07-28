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

    ' The project's files are on disk and settled - refresh the project-tier scan
    gScanMgr.RequestProjectScan()

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
    ' PANEL (5) Spacing
    ' PANEL (6) File Encoding (Ansi, UTF-8, etc)
    ' PANEL (7) Line Endings

'    ' blank out the current statusbar values  
'    for i as long = lbound(gSBPanels) to ubound(gSBPanels)
'        gSBPanels(i).wszText = ""
'    next


    if (gApp.IsProjectLoading) orelse (gApp.IsFileLoading) then
    elseif pDoc <> 0 then
        dim as HWND hEdit = pDoc->hWndActiveScintilla
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

        ' PANEL Line/Col/Sel
        PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, GOTO_PANEL, wszText )

        ' Update the vertical scrollbars (SetRange inside re-derives and repaints)
        frmEditorVScroll_UpdateScrollBars(pDoc)

        ' PANEL Themes dialog
        wszText = " " & wszIconThemes & " "
        PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, THEMES_DIALOG_PANEL, wszText )

        ' PANEL Build Configuration dialog
        PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, BUILD_DIALOG_PANEL, wszIconAddFileButton )

        ' PANEL Build Configuration
        if gApp.GetDocumentCount then
            wszText = frmBuildConfig_GetSelectedBuildDescription()
            PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, BUILD_POPUP_PANEL, wszText )
        end if
        
        ' PANEL Spacing
        wszText = "Spaces: " & gConfig.TabSize
        PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, SPACES_PANEL, wszText )
        
        ' PANEL File Encoding (Ansi, UTF-8, UTF-16) -- DISPLAY ONLY.
        ' This must NOT call SCI_SETCODEPAGE. The status bar refreshes on every caret move,
        ' and issuing a codepage change here reinterprets the document's existing bytes under
        ' a new layout without converting them -- which corrupted the buffer the instant
        ' FileEncoding was relabelled (INVARIANT E1). The codepage is owned by ApplyProperties
        ' (on load) and ConvertTextBuffer (on an explicit user switch), nowhere else.
        select case pDoc->FileEncoding
            case FILE_ENCODING_UTF8      : wszText = "UTF-8"
            case FILE_ENCODING_UTF8_BOM  : wszText = "UTF-8 (BOM)"
            case FILE_ENCODING_UTF16_BOM : wszText = "UTF-16 (BOM)"
            case else                    : wszText = "ANSI"
        end select
        PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, UTF_PANEL, wszText )

        ' PANEL Line Endings
        select case SciExec( hEdit, SCI_GETEOLMODE, 0, 0 )
            case SC_EOL_CRLF: wszText = "CRLF"  '(0)
            case SC_EOL_CR:   wszText = "CR"    '(1)
            case SC_EOL_LF:   wszText = "LF"    '(2)
            case else:        wszText = ""    
        end select 
        PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, CRLF_PANEL, wszText )
        
        if SciExec( hEdit, SCI_GETREADONLY, 0, 0 ) then
            wszText = L(222,"Read Only")
            PsStatusBar_SetText( HWND_FRMMAIN_STATUSBAR, COMPILE_STATUS_PANEL, wszText )
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
' Paint the editor split bar. The control's client rect is the GRAB area
' (SPLITTER_GRAB); the visible band is the narrower SPLITSIZE, centered in it, so the bar
' looks the same as it always did while being easier to grab. 
' ========================================================================================
sub frmMain_SplitterPaint( byval p as PSSPLITTER_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b

    ' The sliver either side of the band is the form's own background.
    b->SetBackColor( ghMain.BackColor )
    b->PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    dim as long nBand = SPLITSIZE
    if pWindow then nBand = pWindow->ScaleX(SPLITSIZE)

    dim as RECT rc = p->rcClient
    if p->nOrientation = PSSPLITTER_VERTICAL then
        dim as long nInset = ((rc.right - rc.left) - nBand) \ 2
        if nInset < 0 then nInset = 0
        rc.left += nInset : rc.right -= nInset
    else
        dim as long nInset = ((rc.bottom - rc.top) - nBand) \ 2
        if nInset < 0 then nInset = 0
        rc.top += nInset : rc.bottom -= nInset
    end if

    b->SetBackColor( ghEditor.Divider )
    b->PaintRect( @rc )
end sub


' ========================================================================================
' The user dragged the editor split bar. The control has already moved itself; we write the
' new position back into the active document and re-lay out the panes around it.
'
' BEGIN/END bracket the drag: pDoc->bSizing is what modMsgPump reads to skip its
' horizontal-scrollbar probing mid-resize, and the horizontal bars are hidden for the
' duration to reduce flicker -- both exactly what the old WM_LBUTTONDOWN/UP pair did.
' ========================================================================================
sub frmMain_SplitterMoved( _
            byval hSplitter as HWND, _
            byval newPos    as integer, _
            byval nPhase    as integer _
            )

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub

    select case nPhase
        case PSSPLITTER_PHASE_BEGIN
            pDoc->bSizing = true
            frmMain_HideHorizScrollBars()   ' hide to reduce flickering

        case PSSPLITTER_PHASE_MOVE
            if hSplitter = HWND_FRMMAIN_SPLITV then
                pDoc->SplitX = newPos
            else
                pDoc->SplitY = newPos
            end if
            frmMain_PositionWindows()

        case PSSPLITTER_PHASE_END
            pDoc->bSizing = false
            frmMain_PositionWindows()
    end select
end sub


' ========================================================================================
' Double-click the split bar closes the split.
'
' This replaces the whole single-vs-double click timer dance (pDoc->idTimer,
' doubleClickReceived, frmMain_TimerProc, HandleSingleClick, HandleDoubleClick): the
' control's class carries CS_DBLCLKS, so the disambiguation is free. Claiming the message
' means no drag starts; the control keeps the capture it already took and releases it on the
' trailing WM_LBUTTONUP.
' ========================================================================================
function frmMain_SplitterMessage( byval m as PSSPLITTER_MESSAGEINFO ptr ) as boolean
    if m->uMsg <> WM_LBUTTONDBLCLK then return false

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return false

    pDoc->bSizing = false
    pDoc->SplitX = 0
    pDoc->SplitY = 0
    pDoc->EditorSplitMode = SplitNone
    pDoc->hWndActiveScintilla = pDoc->hWindow(0)
    frmMain_PositionWindows()
    return true
end function


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)

    b.SetBackColor( ghMain.BackColor )

    b.PaintClientRect()

    ' The splitter between the two edit windows is a real window now (PsSplitter) and paints
    ' itself -- see frmMain_SplitterPaint.

    ' If the top tab control is visible then paint the area between the
    ' right edge of the toptabs menu and the client right edge.
    if IsWindowVisible( HWND_FRMMAIN_TOPTABSMENU ) then
        dim as RECT rcMenu = AfxGetWindowRect( HWND_FRMMAIN_TOPTABSMENU )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(POINT ptr, @rcMenu), 2 )
        rcMenu.left = rcMenu.right
        rcMenu.right = b.rcClient.right
        b.SetBackColor( ghTopTabs.BackColor )
        b.PaintRect( @rcMenu )
        b.SetPenColor( ghEditor.Divider )
        b.PaintLine( 2, rcMenu.left, rcMenu.bottom, rcMenu.right, rcMenu.bottom )
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
    
    ' The bar is a fixed-size window now, so its width is a constant rather than something
    ' derived from where the split pane happened to land.
    dim as long nSplitSize = pWindow->ScaleX(SPLITTER_GRAB)
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

    ' The bottom pane starts BELOW the bar. Previously it started exactly at SplitY and the
    ' bar was drawn into the horizontal-scrollbar gap above it -- which worked only because
    ' the bar was a painted rect and not a window. Now that it is a real window, the bottom
    ' pane clears it, the same way the right pane already cleared the vertical bar.
    dim as long nSplitSize = pWindow->ScaleY(SPLITTER_GRAB)
    dim as long nTop = Max( rcDoc.top, pDoc->SplitY + iif( pDoc->EditorSplitMode = SplitTopBottom, nSplitSize, 0 ) )

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
    dim as long nLeft, nTop
    dim as boolean IsProjectSearchTab = false
    
    dim as RECT rc

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    
    ' Get the entire client area
    GetClientRect( HWND_FRMMAIN, @rc )
    
    dim as long nHeightMenuBar    = AfxGetWindowHeight( HWND_FRMMAIN_MENUBAR )
    dim as long nHeightStatusBar  = AfxGetWindowHeight( HWND_FRMMAIN_STATUSBAR )
    dim as long nHeightOutput     = AfxGetWindowHeight( HWND_FRMOUTPUT )
    dim as long nWidthPanel       = AfxGetWindowWidth( HWND_FRMPANEL )
    dim as long nWidthTopTabsMenu = AfxGetWindowWidth( HWND_FRMMAIN_TOPTABSMENU )
    
    dim as long nHeightTabControl, nHeightTabInfoPanel, nHeightTabFindPanel, nHeightTabReplacePanel

    ' Set the top menu menubar into place 
    SetWindowPos( HWND_FRMMAIN_MENUBAR, 0, _
                    0, 0, rc.right - rc.left, nHeightMenuBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW ) 

    ' Set the statusbar into place 
    SetWindowPos( HWND_FRMMAIN_STATUSBAR, 0, _
                    0, rc.bottom - nHeightStatusBar, rc.right - rc.left, nHeightStatusBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW ) 
    
    nLeft = rc.left 
    nTop = rc.top + nHeightMenuBar
    
    ' Set the Panel pane into place if applicable, with its splitter bar on the panel's
    ' INNER edge -- right of the panel when docked left, left of it when docked right.
    ' The bar's strip is taken from the CONTENT area, so the panel keeps its width and
    ' gConfig.ShowPanelWidth round-trips unchanged.
    '
    ' nWidthPanel keeps meaning the true panel width; nPanelReserve (panel + bar) is what
    ' the content area gives up, and is what every downstream "where does the content start
    ' or end" computation uses.
    dim as long nGrabPanel = pWindow->ScaleX(SPLITTER_GRAB)
    dim as long nPanelReserve = 0
    if IsWindowVisible(HWND_FRMPANEL) then
        ' Limits mirror by dock side: the panel floors at PANEL_MIN_WIDTH and the content
        ' at PANEL_MIN_CONTENT, which swap which end of the range they occupy.
        dim as long nBarPos
        if gConfig.ExplorerPositionRight then
            PsSplitter_SetRange( HWND_FRMMAIN_SPLITPANEL, _
                                pWindow->ScaleX(PANEL_MIN_CONTENT), _
                                rc.right - nGrabPanel - pWindow->ScaleX(PANEL_MIN_WIDTH) )
            PsSplitter_SetPos( HWND_FRMMAIN_SPLITPANEL, rc.right - nWidthPanel - nGrabPanel )
            nBarPos = PsSplitter_GetPos( HWND_FRMMAIN_SPLITPANEL )   ' take back the clamp
            nWidthPanel = rc.right - nBarPos - nGrabPanel
        else
            PsSplitter_SetRange( HWND_FRMMAIN_SPLITPANEL, _
                                pWindow->ScaleX(PANEL_MIN_WIDTH), _
                                rc.right - nGrabPanel - pWindow->ScaleX(PANEL_MIN_CONTENT) )
            PsSplitter_SetPos( HWND_FRMMAIN_SPLITPANEL, nWidthPanel )
            nBarPos = PsSplitter_GetPos( HWND_FRMMAIN_SPLITPANEL )   ' take back the clamp
            nWidthPanel = nBarPos
        end if
        if nWidthPanel < 0 then nWidthPanel = 0

        if gConfig.ExplorerPositionRight then nLeft = rc.right - nWidthPanel
        SetWindowPos( HWND_FRMPANEL, 0, _
                    nLeft, nTop, _
                    nWidthPanel, _
                    rc.Bottom - nHeightStatusBar - nHeightMenuBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW )
        frmPanel_PositionWindows()
        SetWindowPos( HWND_FRMMAIN_SPLITPANEL, 0, _
                    nBarPos, nTop, _
                    nGrabPanel, _
                    rc.Bottom - nHeightStatusBar - nHeightMenuBar, _
                    SWP_NOZORDER or SWP_SHOWWINDOW )
        nPanelReserve = nWidthPanel + nGrabPanel
        nLeft = nPanelReserve
        if gConfig.ExplorerPositionRight then nLeft = rc.left
    Else
        nWidthPanel = 0
        ShowWindow( HWND_FRMMAIN_SPLITPANEL, SW_HIDE )
    end if

'    dim as long nTabIdx = gTTabCtl.GetCurSel()
'    if gTTabCtl.IsValidTab(nTabIdx) then
'        if gTTabCtl.tabs(nTabIdx).docType = DocumentType.ProjectSearch then
'            IsProjectSearchTab = true
'        end if
'    end if
    
    ' if items exist in the top tabcontrol then show the tab control and account for its height
    if (gTTabCtl.GetItemCount() = 0) then
        nHeightTabControl = 0
        nHeightTabInfoPanel = 0
        nHeightTabFindPanel = 0
        nHeightTabReplacePanel = 0
        ShowWindow( HWND_FRMMAIN_TOPTABS, SW_HIDE )
        ShowWindow( HWND_FRMMAIN_TOPTABSMENU, SW_HIDE )
    end if

    if (gTTabCtl.GetItemCount() > 0) then
        nHeightTabControl = AfxGetWindowHeight(HWND_FRMMAIN_TOPTABS)
        dim as integer nLeftMenu = rc.right - pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR) - nWidthTopTabsMenu
        SetWindowPos( HWND_FRMMAIN_TOPTABSMENU, 0, _
                    nLeftMenu, nTop, _
                    nWidthTopTabsMenu, nHeightTabControl, _
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        SetWindowPos( HWND_FRMMAIN_TOPTABS, 0, _
                    nLeft, nTop, _
                    nLeftMenu - nPanelReserve, nHeightTabControl, _
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        nTop += nHeightTabControl
    end if
        
    if gFind.bShowInfoPanel then
        nHeightTabInfoPanel = pWindow->ScaleY(TOPTABS_INFO_HEIGHT)
        SetWindowPos( HWND_FRMMAIN_TOPTABSINFO, 0, _
                    nLeft, nTop, _     
                    rc.Right - nPanelReserve, nHeightTabInfoPanel, _  
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        frmTopTabsInfo_PositionWindows()    
        nTop += nHeightTabInfoPanel
    else    
        ShowWindow( HWND_FRMMAIN_TOPTABSINFO, SW_HIDE )
        ' If Info panel is not open then move the Find panel down slightly
        ' in order to give it a top margin.
        nTop += pWindow->ScaleY(8)
    end if

    if gFind.bShowFindPanel then
        nHeightTabFindPanel = pWindow->ScaleY(TOPTABS_FIND_HEIGHT)
        SetWindowPos( HWND_FRMMAIN_TOPTABSFIND, 0, _
                    nLeft, nTop, _     
                    rc.Right - nPanelReserve, nHeightTabFindPanel, _  
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        frmTopTabsFind_PositionWindows()    
        nTop += nHeightTabFindPanel
    else    
        ShowWindow( HWND_FRMMAIN_TOPTABSFIND, SW_HIDE )
    end if
    
    if (gFind.bShowReplacePanel = true) orelse _
        (IsProjectSearchTab = true andalso gFind.bProjectReplaceActive = true) then
        nHeightTabReplacePanel = pWindow->ScaleY(TOPTABS_REPLACE_HEIGHT)
        SetWindowPos( HWND_FRMMAIN_TOPTABSREPLACE, 0, _
                    nLeft, nTop, _     
                    rc.Right - nPanelReserve, nHeightTabReplacePanel, _  
                    SWP_SHOWWINDOW or SWP_NOZORDER )
        frmTopTabsReplace_PositionWindows()    
        nTop += nHeightTabReplacePanel
    else    
        ShowWindow( HWND_FRMMAIN_TOPTABSREPLACE, SW_HIDE )
    end if
                

    ' Set the Output pane into place if applicable, with its splitter bar in the strip
    ' directly above it. The strip is taken off the EDITOR area, so the panel keeps exactly
    ' the height it had (and gConfig.ShowOutputPanelHeight round-trips unchanged).
    '
    ' nOutputReserve is what the editor area gives up: panel + bar, or nothing at all when
    ' the panel is hidden.
    dim as long nGrabOut = pWindow->ScaleY(SPLITTER_GRAB)
    dim as long nOutputReserve = 0
    if IsWindowVisible(HWND_FRMOUTPUT) then
        ' Limits are ours to compute: the panel floors at its tabs height (as before), and
        ' the editor now floors too, so the panel can no longer be dragged up until the
        ' edit area is a sliver.
        PsSplitter_SetRange( HWND_FRMMAIN_SPLITOUTPUT, _
                            nTop + pWindow->ScaleY(OUTPUT_MIN_EDITOR), _
                            rc.bottom - nHeightStatusbar - pWindow->ScaleY(OUTPUT_TABS_HEIGHT) - nGrabOut )
        PsSplitter_SetPos( HWND_FRMMAIN_SPLITOUTPUT, _
                          rc.bottom - nHeightStatusbar - nHeightOutput - nGrabOut )
        ' Take the clamp back and re-derive the panel height from it, so the bar and the
        ' panel can never disagree about where the boundary is.
        dim as long nBarPos = PsSplitter_GetPos( HWND_FRMMAIN_SPLITOUTPUT )
        nHeightOutput = rc.bottom - nHeightStatusbar - (nBarPos + nGrabOut)
        if nHeightOutput < 0 then nHeightOutput = 0

        SetWindowPos( HWND_FRMMAIN_SPLITOUTPUT, 0, _
                    nLeft, nBarPos, rc.Right - nLeft, nGrabOut, _
                    SWP_NOZORDER or SWP_SHOWWINDOW )
        SetWindowPos( HWND_FRMOUTPUT, 0, _
                    nLeft, nBarPos + nGrabOut, _
                    rc.Right - nLeft, _
                    nHeightOutput, _
                    SWP_NOZORDER or SWP_SHOWWINDOW )
        frmOutput_PositionWindows()
        nOutputReserve = nHeightOutput + nGrabOut
    else
        nHeightOutput = 0
        ShowWindow( HWND_FRMMAIN_SPLITOUTPUT, SW_HIDE )
    end if

    if pDoc = 0 then
        frmMain_HideScrollBars()
        ' No document -> no split chrome either.
        ShowWindow( HWND_FRMMAIN_SPLITV, SW_HIDE )
        ShowWindow( HWND_FRMMAIN_SPLITH, SW_HIDE )
    end if
    
    if pDoc then 
        ' Position the Scintilla child edit windows
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
        dim as RECT rcDoc
        
        ' Reduce the height of the client area by the size of the statusbar and menubar.
        rcDoc.left   = nLeft
        rcDoc.top    = nTop 
        rcDoc.right  = rc.right
        rcDoc.bottom = rc.bottom - nHeightStatusBar - nOutputReserve
        if gConfig.ExplorerPositionRight then rcDoc.right = rc.right - nPanelReserve
        pDoc->rcDocSplitArea = rcDoc

        
        ' Editor split bar. Two PsSplitter instances exist (one per orientation, because
        ' orientation is fixed at creation); at most ONE is shown, following the active
        ' document's split mode. The limits are ours to compute -- the control cannot know
        ' about the editor scrollbars flanking the panes -- and are the same bounds the old
        ' WM_MOUSEMOVE drag clamped to.
        dim as long nGrabV = pWindow->ScaleX(SPLITTER_GRAB)
        dim as long nGrabH = pWindow->ScaleY(SPLITTER_GRAB)
        dim as long nEditScrollW = pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR)

        if pDoc->EditorSplitMode = SplitNone then
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW )
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_HIDE )
            ShowWindow( pDoc->hWindow(1), SW_HIDE )
            ShowWindow( HWND_FRMMAIN_SPLITV, SW_HIDE )
            ShowWindow( HWND_FRMMAIN_SPLITH, SW_HIDE )
            frmMain_PositionMainDocBottom(pDoc, rcDoc)
        end if
        if pDoc->EditorSplitMode = SplitLeftRight then
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW )
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_SHOW )
            ShowWindow( HWND_FRMMAIN_SPLITH, SW_HIDE )
            PsSplitter_SetRange( HWND_FRMMAIN_SPLITV, _
                                rcDoc.left + nEditScrollW, _
                                rcDoc.right - nEditScrollW - nGrabV )
            PsSplitter_SetPos( HWND_FRMMAIN_SPLITV, pDoc->SplitX )
            pDoc->SplitX = PsSplitter_GetPos( HWND_FRMMAIN_SPLITV )   ' take back the clamp
            SetWindowPos( HWND_FRMMAIN_SPLITV, 0, _
                pDoc->SplitX, rcDoc.top, nGrabV, rcDoc.bottom - rcDoc.top, _
                SWP_NOZORDER or SWP_SHOWWINDOW )
            frmMain_PositionSplitDocLeft(pDoc, rcDoc)
            frmMain_PositionMainDocRight(pDoc, rcDoc)
        end if
        if pDoc->EditorSplitMode = SplitTopBottom then
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW )
            ShowWindow( HWND_FRMEDITOR_VSCROLLBAR(1), SW_SHOW )
            ShowWindow( HWND_FRMMAIN_SPLITV, SW_HIDE )
            PsSplitter_SetRange( HWND_FRMMAIN_SPLITH, _
                                rcDoc.top, _
                                rcDoc.bottom - nGrabH )
            PsSplitter_SetPos( HWND_FRMMAIN_SPLITH, pDoc->SplitY )
            pDoc->SplitY = PsSplitter_GetPos( HWND_FRMMAIN_SPLITH )   ' take back the clamp
            SetWindowPos( HWND_FRMMAIN_SPLITH, 0, _
                rcDoc.left, pDoc->SplitY, rcDoc.right - rcDoc.left, nGrabH, _
                SWP_NOZORDER or SWP_SHOWWINDOW )
            frmMain_PositionSplitDocTop(pDoc, rcDoc)
            frmMain_PositionMainDocBottom(pDoc, rcDoc)
        end if

            
        ' Update editor vertical scrollbars (SetRange inside re-derives and repaints)
        frmEditorVScroll_UpdateScrollBars( pDoc )
        
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
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0

    SetFocus hWndContext

    ' The editor's right-click menu is a PsPopupMenu (modContextMenus.inc). It positions
    ' itself at the cursor and verifies that the cursor really is over the active
    ' Scintilla window -- that guard used to live in CreateScintillaContextMenu, which
    ' signalled "don't show" by returning a null HMENU.
    ShowScintillaContextMenu()

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
    ' The tab bar owns the current tab; the config field is just its persisted copy.
    gConfig.ShowOutputPanelIndex = PsSelectBar_GetCurSel( HWND_FRMOUTPUT_SELECTBAR )
    
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

    ' Stop the background scan worker (bounded join) before the window and
    ' its message pump go away - no MSG_USER_PARSE_COMPLETE may arrive after this.
    gScanMgr.Shutdown()

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

    ' Hide the main application window because we are about to attempt to shutdown
    ' the background scanning thread. Sometimes a lag seems to happen from the time
    ' request is made to when the main application gets destroyed which leaves a
    ' kind of look of the application in various stages of repainting as various
    ' child windows get destroyed. Do this AFTER the config is saved because it
    ' affects positioning.
    ShowWindow( hwnd, SW_HIDE )
    
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


' The editor split bar's WM_MOUSEMOVE / WM_LBUTTONUP / WM_LBUTTONDBLCLK handlers used to
' live here, along with the 100 ms timer that told a single click from a double click
' (frmMain_TimerProc, HandleSingleClick, HandleDoubleClick). The PsSplitter control owns all
' of it now: the drag, the mouse capture, the IDC_SIZEWE/IDC_SIZENS cursor, the hover state,
' and -- because its window class carries CS_DBLCLKS -- the click disambiguation, so the
' double-click that closes the split arrives directly at frmMain_SplitterMessage.


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

    ' The splitter hit-test that used to follow lives in the PsSplitter control now.

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
            if wParam = IDT_PARSER_DEBOUNCE then
                ' Typing has paused - hand the active document to the scan worker
                KillTimer( hwnd, IDT_PARSER_DEBOUNCE )
                gScanMgr.RequestBufferScan( gTTabCtl.GetActiveDocumentPtr() )
            end if
            if wParam = IDT_THEME_COALESCE then
                ' A live theme edit has settled. This timer lives here rather than on the
                ' Options dialog because the dialog's own pump is an all-window GetMessage,
                ' so a frmMain timer still arrives while it is up -- and unlike the dialog,
                ' frmMain is always alive.
                KillTimer( hwnd, IDT_THEME_COALESCE )
                Theme_OnCoalesceTimer()
            end if
            
        case WM_NCACTIVATE
            ' if wParam is false then the system wants to draw an inactive title bar. we
            ' will prevent this action if a popup menu from the menubar is active, or if
            ' the frmSearchSymbol fuzzy finder is active.
            if wParam = false then
                if gApp.KeepTitleBarActive then
                    ' Lie to DefWindowProc and tell it to keep painting as active.
                    return DefWindowProc(hwnd, uMsg, TRUE, -1)  ' -1 avoids repainting glitches
                end if
            end if

        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if
            
        case WM_SETCURSOR
            ' Only the busy cursor is frmMain's to set. The two drag arms that used to
            ' head this chain (a hand-rolled splitter/listview drag, and a top-tab drag)
            ' are gone with their globals: every drag site is a reusable control now --
            ' PsSplitter, PsListTree, PsTabBar -- and each sets its own cursor from its own
            ' WM_SETCURSOR, so frmMain must not answer for them.
            if (gApp.IsCompiling = true) or _
               (gApp.IsProjectLoading = true) or _
               (gApp.IsFileLoading = true) then
                SetCursor( LoadCursor(0, IDC_WAIT) )
                return true
            end if

        ' (No WM_CAPTURECHANGED here any more: its whole body was clearing
        '  gApp.bDragTabActive, which nothing set and nothing else read. Capture is now
        '  owned and released by the controls that take it.)

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

        case WM_SETFOCUS
            frmMain_SetFocusToCurrentCodeWindow()

        case MSG_USER_SETFOCUS
            ' Set focus to current Scintilla window and update the document
            ' display such as Line#, Col#, Filename, etc.
            frmMain_SetStatusbar()
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then 
                ' Ensure that the correct Explorer listbox entry is selected
                frmExplorer_SelectItemData( pDoc )
                SetFocus( pDoc->hWndActiveScintilla )
            end if

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
            ' wParam carries the AUTOCOMPLETE_NOTIFY_* reason for the rebuild.
            return ShowAutocompleteList( clng(wParam) )

        case MSG_USER_PARSE_COMPLETE
            ' A background fbcParser scan finished: swap it into gSymDb (atomic,
            ' on this UI thread) and hand the displaced set back to the worker
            ' to be freed under the same-thread contract.
            dim as PARSERESULTSET ptr pRSet = gScanMgr.TakeCompleted( clng(wParam) )
            if pRSet then
                dim as long nTier = pRSet->tier
                dim as string sTrace = "FBCP complete: tier=" & nTier & _
                                       " rc=" & pRSet->scanRc & " ms=" & pRSet->scanMs
                if pRSet->pResult then
                    sTrace &= "  symbols=" & pRSet->pResult->symbolCount & _
                              " files=" & pRSet->pResult->fileCount & _
                              " diags=" & pRSet->pResult->diagCount
                end if

                ' Discard a result whose root no longer matches the app's current
                ' state instead of installing it. Closing a project saves dirty
                ' files, and a save REQUESTS a project scan - so the closed
                ' project's scan can complete and arrive here after ProjectClose
                ' already evicted its sets; installing it would resurrect the
                ' stale Functions/Bookmarks panels. Same for a buffer scan of a
                ' document that is no longer the active one.
                dim as boolean bStale = false
                if nTier = ScanTierProject then
                    dim as clsDocument ptr pMainDoc = gApp.GetMainDocumentPtr()
                    if pMainDoc = 0 then
                        bStale = true
                    else
                        bStale = ( SymDb_FileNameEq( pRSet->wszRootFile, pMainDoc->DiskFilename ) = false )
                    end if
                else
                    dim as clsDocument ptr pActiveDoc = gTTabCtl.GetActiveDocumentPtr()
                    if pActiveDoc = 0 then
                        bStale = true
                    else
                        bStale = ( SymDb_FileNameEq( pRSet->wszRootFile, ScanMgr_GetRootName(pActiveDoc) ) = false )
                    end if
                end if
                if bStale then sTrace &= "  DISCARDED (stale root)"
                print sTrace

                if bStale then
                    gScanMgr.RetireResult( pRSet )
                else
                    dim as PARSERESULTSET ptr pOld = gSymDb.InstallSet( nTier, pRSet )
                    if pOld then gScanMgr.RetireResult( pOld )

                    ' A fresh scan can add/remove files and procs: refresh the
                    ' Functions panel when it is showing (the panel-show paths
                    ' reload it anyway when it becomes visible later).
                    if IsWindowVisible( HWND_FRMFUNCTIONS ) then LoadFunctionsFiles()

                    ' Both tiers update the TODO store now (buffer scans re-scan the
                    ' active file's text; project scans sweep every user file on
                    ' disk) - refresh the panel either way.
                    frmOutput_UpdateToDoListview()
                end if
            end if

        ' (MSG_USER_APPENDEQUALSSIGN is gone. Its only sender was the SCN_AUTOCSELECTION
        '  handler, which padded an "=" out to " = " when that fillup character had just
        '  accepted a completion. "=" no longer commits anything, so there is nothing to
        '  reformat.)

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
    
    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL) 
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)
    ghFont(ITALICFONT_10)  = pWindow->CreateFont(GUIFIXEDFONT, 10, FW_NORMAL, CTRUE)
    ghFont(INFOFONT_11)    = pWindow->CreateFont(GUIFIXEDFONT, 11, FW_NORMAL)
    ghFont(SYMBOLFONT_9)   = pWindow->CreateFont(SYMBOLFONT, 9, FW_NORMAL)
    ghFont(SYMBOLFONT_10)  = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_12)  = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)
    ghFont(SYMBOLFONT_20)  = pWindow->CreateFont(SYMBOLFONT, 20, FW_NORMAL)
    ghFont(GUIFONT_11)     = pWindow->CreateFont(GUIFONT, 11, FW_NORMAL)
    ghFont(GUIFONT_12)     = pWindow->CreateFont(GUIFONT, 12, FW_NORMAL)
    ghFont(GUIFONTBOLD_11) = pWindow->CreateFont(GUIFONT, 11, FW_HEAVY)
    ' FW_SEMIBOLD, not FW_HEAVY: at 26pt heavy reads as shouting, and Segoe UI's semibold is
    ' the weight Windows itself uses for large display text.
    ghFont(GUIFONT_26)     = pWindow->CreateFont(GUIFONT, 26, FW_SEMIBOLD)
    ghFont(FIXEDFONT_26)   = pWindow->CreateFont(GUIFIXEDFONT, 26, FW_BOLD)

    HWND_FRMMAIN = pWindow->Create( null, APPNAME, @frmMain_WndProc )
    
    ' Load the currently selected theme (this needs a valid pWindow HWND_FRMMAIN to exists)
    LoadThemeFile()
    ' Env-gated; returns immediately unless TIKO_THEME_SELFTEST=1. Runs here because it needs
    ' a fully parsed theme, and it snapshots/restores gTheme around its destructive cases.
    Theme_RunSelfTest()
    OptionsRows_RunBindTest()
    ' Env-gated (TIKO_ENCODING_SELFTEST=1). Pure byte-exact + disk round-trip; needs no
    ' document window, so it is safe to run here at startup.
    Encoding_RunSelfTest()
    
    ' Set the small and large icon for the main window (must be set after main window is created)
    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' Load the good and bad compile "icons"
    ghIconGood = 1  
    ghIconBad  = 2 
    
    ' Load the North/South and East/West cursor images
    ghCursorSizeNS = LoadImage( Null, MAKEINTRESOURCEW(OCR_SIZENS), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE or LR_SHARED )
    ghCursorSizeWE = LoadImage( Null, MAKEINTRESOURCEW(OCR_SIZEWE), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE or LR_SHARED )

    ' Rendering-backend geometry self-test. Runs here because it needs a CWindow-backed
    ' HWND (PaintLine asks it for the DPI scale) but nothing else -- no theme, no config.
    if environ("PSBUFFERPAINT_SELFTEST") = "1" then PsBufferPaint_RunSelfTest( HWND_FRMMAIN )

    ' Build the keyboard accelerators
    frmKeyboard_BuildAcceleratorTable()

    ' Key vocabulary self-test. AFTER the build, because that is what populates gKeys() --
    ' run before it and the defaults-table assertions would walk an empty array and pass.
    KeyBindings_RunSelfTest()
    frmKeyboard_RunSelfTest()
    frmKeyboardEdit_RunSelfTest()
    ' Command Line dialog geometry. Pure -- no window, no theme, no config -- so unlike the
    ' Options/User Tools suites it needs no AUTOOPEN companion to reach it.
    frmCommandLine_RunSelfTest()
    ' Open the Keyboard Shortcuts dialog immediately, so its layout/filter self-test can be
    ' run without a human driving the menu. The dialog is MODAL, so startup stops here until
    ' it is closed -- that is what this flag is for, and it is why it is not on by default.
    if environ("TIKO_KEYBOARD_AUTOOPEN") = "1" then frmKeyboard_Show( HWND_FRMMAIN )

    ' Same arrangement for the User Tools dialog: its self-test measures real windows, so it
    ' only runs when the dialog is opened, and this is how it gets opened without a human
    ' driving the menu. Also modal -- startup stops here until it is closed.
    if environ("TIKO_USERTOOLS_AUTOOPEN") = "1" then frmUserTools_Show( HWND_FRMMAIN )

    ' And the same for the About box, whose layout self-test asks the one question a
    ' fixed-size dialog can be asked -- does every page fit -- and therefore needs its real
    ' windows. Also modal: startup stops here until it is closed.
    if environ("TIKO_ABOUT_AUTOOPEN") = "1" then
        frmAbout_Show( HWND_FRMMAIN )
        ' With TIKO_ABOUT_AUTOCLOSE the dialog closes itself, so shut the whole editor down
        ' too and let the run terminate on its own. Posted rather than sent: the main message
        ' loop has not started yet, so this is picked up the moment it does.
        if environ("TIKO_ABOUT_AUTOCLOSE") = "1" then PostMessage( HWND_FRMMAIN, WM_CLOSE, 0, 0 )
    end if

    ' Create the main application topmenu menubar
    frmMenuBar_Show( HWND_FRMMAIN )

    ' Create every standalone context menu. After frmMenuBar_Show, which owns the popup
    ' theme registry these enrol themselves in.
    ContextMenus_Init()

    ' Create the code-completion popup. Once, here: it is a singleton that outlives every
    ' document, and it needs the theme (LoadThemeFile above) and ghFont() already built.
    frmAutoComplete_Init()

    ' Create the main application top tabs control
    frmTopTabs_Show( HWND_FRMMAIN )
    frmTopTabsMenu_Show( HWND_FRMMAIN ) 
    frmTopTabsInfo_Show( HWND_FRMMAIN )
    frmTopTabsFind_Show( HWND_FRMMAIN )
    frmTopTabsReplace_Show( HWND_FRMMAIN )
    
    ' Create the main application statusbar
    frmStatusBar_Show( HWND_FRMMAIN )

    ' Create the various child windows
    frmPanel_Show( HWND_FRMMAIN )
    frmOutput_Show( HWND_FRMMAIN )
    frmEditorHScroll_Show( HWND_FRMMAIN )
    frmEditorVScroll_Show( HWND_FRMMAIN )

    ' Editor split bars. One per orientation because PsSplitter fixes its orientation at
    ' creation; PositionWindows shows at most one, following the active document's split
    ' mode. Both start hidden and zero-sized -- PositionWindows places them.
    HWND_FRMMAIN_SPLITV = PsSplitter_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPLITV, PSSPLITTER_VERTICAL )
    HWND_FRMMAIN_SPLITH = PsSplitter_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPLITH, PSSPLITTER_HORIZONTAL )
    for i as long = 0 to 1
        dim as HWND hBar = iif( i = 0, HWND_FRMMAIN_SPLITV, HWND_FRMMAIN_SPLITH )
        PsSplitter_SetPaintCallback( hBar, @frmMain_SplitterPaint )
        PsSplitter_SetPosChangedCallback( hBar, @frmMain_SplitterMoved )
        PsSplitter_SetMessageCallback( hBar, @frmMain_SplitterMessage )
    next

    ' Output panel splitter: sits between the editor area and the Output panel. Also a
    ' frmMain child, because frmMain owns that layout.
    HWND_FRMMAIN_SPLITOUTPUT = PsSplitter_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPLITOUTPUT, PSSPLITTER_HORIZONTAL )
    PsSplitter_SetPaintCallback( HWND_FRMMAIN_SPLITOUTPUT, @frmOutput_SplitterPaint )
    PsSplitter_SetPosChangedCallback( HWND_FRMMAIN_SPLITOUTPUT, @frmOutput_SplitterMoved )
    PsSplitter_SetMessageCallback( HWND_FRMMAIN_SPLITOUTPUT, @frmOutput_SplitterMessage )

    ' Explorer panel splitter: sits on the panel's inner edge, whichever side it is docked.
    ' No message callback -- this bar has no double-click gesture.
    HWND_FRMMAIN_SPLITPANEL = PsSplitter_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPLITPANEL, PSSPLITTER_VERTICAL )
    PsSplitter_SetPaintCallback( HWND_FRMMAIN_SPLITPANEL, @frmPanel_SplitterPaint )
    PsSplitter_SetPosChangedCallback( HWND_FRMMAIN_SPLITPANEL, @frmPanel_SplitterMoved )

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

    ' Open the Environment Options dialog immediately, so its page-fit and focus/tab-order
    ' self-tests can be run without a human driving the menu. Companion to TIKO_KEYBOARD_AUTOOPEN,
    ' and here for the same reason: TIKO_OPTIONS_SELFTEST only runs when the dialog is OPENED, and
    ' the focus half of it cannot be reached any other way. The dialog is MODAL, so startup stops
    ' here until it is closed -- which is why it is not on by default.
    '
    ' It sits AHEAD of the three PostMessage calls below, deliberately: the dialog runs its own
    ' pump, which would otherwise pick those up and start processing the command line and the
    ' update check underneath an open modal dialog.
    if environ("TIKO_OPTIONS_AUTOOPEN") = "1" then frmOptions_Show( HWND_FRMMAIN )

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

        ' The whole topmenu system (bar highlight, dropdowns, submenus): keyboard
        ' navigation, and dismissal on outside clicks. Consumes every plain key while
        ' a dropdown is open (menu semantics) but never WM_SYSKEY*, so the Alt+letter
        ' accelerators below keep working. Replaces handleMouseTopMenu and
        ' handleKeysTopMenu.
        if PsMenuBar_FilterMessage( HWND_FRMMAIN_MENUBAR, @uMsg ) then continue do

        ' The same service for the standalone context menus. It is a SEPARATE call
        ' because the bar's filter stands down unless the BAR has a dropdown open --
        ' without this line a context menu would have no keyboard navigation and would
        ' never dismiss on an outside click.
        if ContextMenu_FilterMessage( @uMsg ) then continue do

        ' And for the PsTextBox controls' own built-in Cut/Copy/Paste menu, which is a
        ' PsPopupMenu too since the TrackPopupMenu migration. One call covers every
        ' textbox in the app.
        if PsTextBox_FilterMessage( @uMsg ) then continue do

        ' The code-completion popup. Unlike every filter above it, this one consumes ONLY
        ' the navigation and commit keys -- ordinary typing MUST reach Scintilla, which is
        ' what drives the incremental filtering. It sits here for two reasons: ahead of
        ' handleEscKeyModeless, which would otherwise close the Find panel on the Escape
        ' meant for the popup; and ahead of the TranslateAccelerator chain, so Tab commits
        ' instead of firing IDM_INDENTBLOCK.
        if frmAutoComplete_FilterMessage( @uMsg ) then continue do

        if handleAltKeyMenuBar(uMsg) then continue do

        ' Close with ESC key: Find/Replace, FindInFiles, FunctionList window
        if handleEscKeyModeless(uMsg) then continue do

        ' Handle ENTER, TAB keys for active Find/Replace
        if handleKeysFindReplace(uMsg) then continue do

'        ' Handle ENTER for Scintilla control (beautify current line)
'        if handleScintillaENTER(uMsg) then continue do

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

