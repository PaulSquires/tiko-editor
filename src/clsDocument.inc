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


''
''  clsDocument (Class to handle everything related to a Scintilla editing window)
''

#include once "clsDocument.bi"
#include once "frmTopTabs.bi"
#include once "frmBuildConfig.bi"
#include once "modMRU.bi"


''
''
destructor clsDocument
    ' Delete any manually allocated scintilla edit windows in the editor
    this.DestroyScintillaWindows()
    ' Destroy any manually allocated memory for #include files
    this.RemoveAllIncludeDocuments()
end destructor


''
''  Returns true/False indicating whether user manual changes were made
''  outside of whatever edit changes Scintilla makes.
''
property clsDocument.UserModified() as boolean
    return m_UserModified
end property

property clsDocument.UserModified( byval nModified as boolean ) 
    m_UserModified = nModified
end property



' ========================================================================================
' Determines if the IDC_SCINTILLA id being checked belongs to any of the 
' currently defined Scintilla windows (needed for splitter windows).
' ========================================================================================
function clsDocument.IsValidScintillaID( byval idScintilla as long ) as boolean
    for i as long = lbound(this.hWindow) to ubound(this.hWindow)
        if IDC_SCINTILLA + i = idScintilla then
            return true
        end if
    next
end function


''
''
function clsDocument.GetActiveScintillaPtr() as any ptr
    dim as HWND hEdit = this.hWndActiveScintilla
    for i as long = lbound(this.hWindow) to ubound(this.hWindow)
        if this.hWindow(i) = hEdit then return pSci(i)
    next
    ' if no other matches then return 
    function = pSci(0)
end function


property clsDocument.hWndActiveScintilla() as HWND
    if m_hWndActiveScintilla = 0 then m_hWndActiveScintilla = this.hWindow(0)
    property = m_hWndActiveScintilla
end property

property clsDocument.hWndActiveScintilla(byval hWindow as HWND)
    m_hWndActiveScintilla = hWindow
end property



' ========================================================================================
' Set the Project Filetype for this document depending on file extension.
' ========================================================================================
function clsDocument.SetProjectFileType() as long
    ' Give this document a default project type depending on its file extension.
    ' We also allow user to select MAIN or NORMAL for non-project files.
    if (this.IsNewFlag = false) andalso (this.ProjectFileType = FILETYPE_UNDEFINED) then
        gApp.ProjectSetFileType( @this, this.ProjectFileType )
    end if
    function = 0
end function


' ========================================================================================
' Set the members of the DocInfo structure. 
' Used for restoring the caret position of the document on load/save.
' ========================================================================================
function clsDocument.SetDocInfo() as long
    this.docData.bIsValidData = true
    if IsWindow(this.hWindow(0)) then    
       this.docData.wszBookmarks = this.GetBookmarks()
       this.docData.wszBreakPoints = this.GetBreakPoints()
       this.docData.wszFoldPoints =this.GetFoldPoints()
       this.docData.nFirstLine = SendMessage( this.hWindow(0), SCI_GETFIRSTVISIBLELINE, 0, 0) 
       this.docData.nPosition = SendMessage( this.hWindow(0), SCI_GETCURRENTPOS, 0, 0)  
       this.docData.nFirstLine1 = 0
       this.docData.nPosition1 = 0
       this.docData.nSplitMode = this.EditorSplitMode
       this.docData.nSplitPositionX = this.SplitX
       this.docData.nSplitPositionY = this.SplitY
       this.docData.nFocusEdit = iif(this.hWndActiveScintilla = this.hWindow(0), 0, 1)
    end if
    if IsWindow(this.hWindow(1)) then    
       this.docData.nFirstLine1 = SendMessage( this.hWindow(1), SCI_GETFIRSTVISIBLELINE, 0, 0) 
       this.docData.nPosition1 = SendMessage( this.hWindow(1), SCI_GETCURRENTPOS, 0, 0)  
    end if
    function = 0
end function    
    

' ========================================================================================
' Destroy existing Scintilla editing windows for this document
' Called from Destructor and from ReloadDocument (because in order to force the new
' text to be set to the scintilla window they need to be recreated).
' ========================================================================================
function clsDocument.DestroyScintillaWindows() as long
    ' THE CHOKE POINT for "this document's buffer is going away". Find in Project holds a
    ' reference to the Scintilla document and may have excerpt views bound to it, so it has to
    ' let go BEFORE the windows die.
    '
    ' It is hooked here rather than in clsApp because this is the only place all three routes
    ' pass through: RemoveDocument, RemoveAllDocuments (which does NOT call RemoveDocument --
    ' that is the project-close path), and ReloadDocument, which destroys the Scintilla
    ' document while the clsDocument ptr survives and which the file watcher drives with no
    ' user action at all.
    FindProject_OnDocumentClosing( @this )

    for i as long = lbound(this.hWindow) to ubound(this.hWindow)
        if this.hWindow(i) then 
            DestroyWindow( this.hWindow(i) )
            this.hWindow(i) = 0
        end if
    next
    function = 0
end function


' ========================================================================================
' Create the Scintilla editing windows for this document
' ========================================================================================
function clsDocument.CreateScintillaWindows() as long
    ' Creates two Scintilla editing windows (initially not visible).
    for i as long = lbound(this.hWindow) to ubound(this.hWindow)
        this.hWindow(i) = CreateWindowEx( 0, "Scintilla", "", _
                        WS_CHILD or WS_TABSTOP or WS_CLIPCHILDREN, _
                        0,0,0,0, HWND_FRMMAIN, _
                        cast(HMENU, IDC_SCINTILLA+i), GetModuleHandle(null), null)

        SendMessage( this.hWindow(i), SCI_SETMODEVENTMASK, _
                        SC_MOD_INSERTTEXT or SC_MOD_DELETETEXT, 0 )
    
        ' Initialize our direct access to the Scintilla code windows. This is much faster than
        ' using SendMessage to the window. Only need to initialize once no matter how many
        ' code windows that are eventually opened.
        if IsWindow(this.hWindow(i)) then
            ' NOTE: In my testing, need to only set the Scintilla lexer to the base editing
            ' window only and NOT both split windows. Also need to do this immediately after
            ' the window is created and do not send the message again afterwards.
            ' Also, every window must have a separate new call to CreateLexer. We can not
            ' just get one lexer and then try to share it amongst multiple new windows. When
            ' a window is destroyed then the pointer would be as well causing other existing
            ' windows to GPF.
            if i = 0 then
                ' Load my custom Tiko lexer based on VB lexer from Lexilla and feed it into Scintilla
                dim as any ptr pLexer = gApp.pfnCreateLexerfn( "tiko" )
                SendMessage( this.hWindow(i), SCI_SETILEXER, 0, cast(LPARAM, pLexer) )
            end if
            if SciMsg = 0 then
                SciMsg = cast( Scintilla_Directfunction, SendMessage( this.hWindow(0), SCI_GETDIRECTFUNCTION, 0, 0 ) )
            end if
            ' Call the direct function for speed purposes rather than relying on the traditional SendMessage method.
            pSci(i) = cast(any ptr, SendMessage( this.hWindow(i), SCI_GETDIRECTPOINTER, 0, 0 )) 
        end if
    next

    ' Disable scintilla vertical scroll bar (wParam = 1 to enable)
    SciMsg( pSci(0), SCI_SETVSCROLLBAR, 0, 0 )
    SciMsg( pSci(0), SCI_SETHSCROLLBAR, 0, 0 )
    SciMsg( pSci(1), SCI_SETVSCROLLBAR, 0, 0 )
    SciMsg( pSci(1), SCI_SETHSCROLLBAR, 0, 0 )
    
    ' Get the document pointer from our main control and assign it to the other split windows
    dim as any ptr pDoc = cast(any ptr, SciMsg(pSci(0), SCI_GETDOCPOINTER, 0, 0))
    if pDoc then SciMsg( pSci(1), SCI_SETDOCPOINTER, 0, cast(LPARAM, pDoc)) 
            
    function = 0

end function


' ========================================================================================
' Assign/Move the TextBuffer into the Scintilla editing window for this document
' ========================================================================================
function clsDocument.AssignTextBuffer() as long
    ' If a valid scintilla window already exists then this means that a previous
    ' function has already assigned the buffer text to the scintilla window so we
    ' can not do it a second time/
    if IsWindow(this.hWindow(0)) then exit function
    
    if IsWindow(this.hWindow(0)) = 0 then
        this.CreateScintillaWindows()
        ' Take this opportunity to determine the text line endings
        if instr(this.TextBuffer, chr(13,10)) then
            SciMsg( pSci(0), SCI_SETEOLMODE, SC_EOL_CRLF, 0)
        elseif instr(this.TextBuffer, chr(10)) then
            SciMsg( pSci(0), SCI_SETEOLMODE, SC_EOL_LF, 0)
        elseif instr(this.TextBuffer, chr(13)) then
            SciMsg( pSci(0), SCI_SETEOLMODE, SC_EOL_CR, 0)
        end if
    end if
    if IsWindow(this.hWindow(0)) then
        ' Establish the codepage on the still-EMPTY buffer before inserting text, so the
        ' bytes are never interpreted under the wrong layout (INVARIANT E1). ApplyProperties
        ' below sets the same value again idempotently; doing it here removes the reliance on
        ' SetText happening to be byte-transparent. Codepage is per-view, so set both splits.
        dim as long nInitCP = iif( this.FileEncoding = FILE_ENCODING_ANSI, 0, SC_CP_UTF8 )
        SciMsg( pSci(0), SCI_SETCODEPAGE, nInitCP, 0 )
        SciMsg( pSci(1), SCI_SETCODEPAGE, nInitCP, 0 )
        this.SetText(this.TextBuffer)
        this.TextBuffer = ""
        ' Set the previously saved positions
        if this.docData.bIsValidData then
            this.SplitX = this.docData.nSplitPositionX
            this.SplitY = this.docData.nSplitPositionY
            this.EditorSplitMode = this.docData.nSplitMode
            this.SetBookmarks(this.docData.wszBookmarks)
            this.SetBreakPoints(this.docData.wszBreakPoints)
            this.SetFoldPoints(this.docData.wszFoldPoints)
            SciExec( this.hWindow(0), SCI_SETFIRSTVISIBLELINE, this.docData.nFirstLine, 0) 
            SciExec( this.hWindow(0), SCI_GOTOPOS, this.docData.nPosition, 0) 
            SciExec( this.hWindow(1), SCI_SETFIRSTVISIBLELINE, this.docData.nFirstLine1, 0) 
            SciExec( this.hWindow(1), SCI_GOTOPOS, this.docData.nPosition1, 0) 
        end if
        this.ApplyProperties()
    end if
    function = 0
end function


''
''
function clsDocument.LoadDiskFile( _
            byref wszFile as wstring, _
            byval bForceParse as boolean = true, _
            byval bIsTemplate as boolean = false _
            ) as long
    ' if a disk file was specified then open it and load it into the editor
    if len(wszFile) = 0 then exit function   
    if AfxFileExists(wszFile) = 0 then exit function
        
    ' this will be an UTF-8 encoded string
    dim sText as string  

    if GetFileToString(wszFile, sText, @this) = false then
        this.DateFileTime = AfxGetFileLastWriteTime( wszFile )
    end if

    this.TextBuffer = sText 
    
    ' If this is text being loaded to be used as a Template then do not
    ' set the diskfilename. The original CreateEmptyDocument would have
    ' already set the filename to Untitled.
    if bIsTemplate = false then
        this.DiskFilename = wszFile
    end if
    
    ' Kick a background symbol scan for the freshly loaded text (skipped
    ' during bulk project loading - the project-tier scan covers those files)
    if (gApp.IsProjectLoading = false) andalso (bIsTemplate = false) then
        gScanMgr.RequestBufferScan( @this )
    end if

    function = 0
end function
    

''
''
function clsDocument.CreateCodeWindow( _
            byval hWndParent as HWND, _
            byval IsNewFile  as boolean, _     
            byval IsTemplate as boolean = false, _
            byref wszFile    as wstring = "" _
            ) as HWND   

    this.CreateScintillaWindows()
    this.LoadDiskFile( wszFile )
    
    ' Set the IsNewFlag after the above load file code. This is because the load
    ' code could load a file as template but is then still considered a "new file"/
    this.IsNewFlag = IsNewFile
    if IsNewFile then
        this.nextFileNum = this.nextFileNum + 1
        this.DiskFilename = "Untitled" & this.nextFileNum
    end if
    this.ProjectFileType = FILETYPE_UNDEFINED
    
    ' All newly opened files have dirty modified buffer status set to false
    ' unless this is a template being opened whereby the user will need to save
    ' it under a new name.
    if IsTemplate then
        this.UserModified = IsTemplate
    end if
        
    ' Set the text and apply properties to the edit window
    this.AssignTextBuffer()

    ' Update the most recently used file list (only for non-IsNewFlag files)
    ' Only add file to MRU list if it is not part of a named Project.
    if gApp.IsProjectNamed() = false then
        UpdateMRUList(wszFile)
    end if

    function = this.hWindow(0)
end function

''
''
function clsDocument.FindReplace( _
            byval strFindText as string, _
            byval strReplaceText as string _
            ) as long

    ' return Value: new position if successful; -1 if text not found.
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim as long findFlags = SCFIND_MATCHCASE or SCFIND_WHOLEWORD
    dim as long startPos  = SciMsg( pSci, SCI_GETCURRENTPOS, 0, 0)
    dim as long endPos    = SciMsg( pSci, SCI_GETTEXTLENGTH, 0, 0)
    dim as long newPos

    ' Set the start and end positions, and search flags, and finally do the search
    SciMsg( pSci, SCI_SETTARGETSTART, startPos, 0)
    SciMsg( pSci, SCI_SETTARGETEND, endPos, 0)
    SciMsg( pSci, SCI_SETSEARCHFLAGS, findFlags, 0)

    ' Search the text to replace
    newPos = SciMsg( pSci, SCI_SEARCHINTARGET, len(strFindText), cast(LPARAM, strptr(strFindText)) )
    
    ' return -1 if not found
    if newPos = - 1 then return -1
    
    gApp.SuppressNotify = true
    ' Position the caret and select the text
    SciMsg( pSci, SCI_SETCURRENTPOS, newPos, 0)
    SciMsg( pSci, SCI_GOTOPOS, newPos, 0)
    SciMsg( pSci, SCI_SETSELECTIONSTART, newPos, 0)
    SciMsg( pSci, SCI_SETSELECTIONEND, newPos + len(strFindText), 0)

    ' Replace the selection (SCI_REPLACESEL fails if text is "" so use Cut instead for that scenario)                                
    if len(strReplaceText) = 0 then
        SciMsg( pSci, SCI_CUT, 0, 0 )
    else
        SciMsg( pSci, SCI_REPLACESEL, 0, cast(LPARAM, strptr(strReplaceText)) )
    end if
    gApp.SuppressNotify = false
    
    ' return the new position
    function = newPos

end function

''
''
function clsDocument.InsertFile() as boolean
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Display the Open File Dialog
    dim pwszName as wstring ptr = AfxIFileOpenDialogW(HWND_FRMMAIN, IDM_INSERTFILE)
    if pwszName then
        ' save the main file encoding because GetFileTostring may change it
        dim as string sText
        GetFileTostring(*pwszName, sText, @this)
        SciMsg( pSci, SCI_INSERTTEXT, -1, cast(LPARAM, strptr(sText)) )
        CoTaskMemFree pwszName
        this.ApplyProperties()
    end if
    
    function = 0
end function


''
''
function clsDocument.SaveFile( byval bSaveAs as boolean = false ) as boolean
    dim sText        as string
    dim wszFilename  as wstring * MAX_PATH
    dim wszExtension as wstring * MAX_PATH
    dim wszText      as wstring * MAX_PATH

    ' if this is a new Untitled document then set flag to display SaveAs dialog.
    if this.IsNewFlag then bSaveAs = true
        
    wszFilename = this.Diskfilename
        
    if bSaveAs then
        ' Display the Save File Dialog
        dim pwszName as wstring ptr = AfxIFileSaveDialog(HWND_FRMMAIN, @wszFilename, @wszExtension, IDM_FILESAVEAS)
        if pwszName then
            wszFilename = *pwszName
            CoTaskMemFree(pwszName)
        else
            return true  ' file save cancelled
        end if
    end if

    dim as any ptr pSci = this.GetActiveScintillaPtr()
    
    ' if pSci does not exist then this file exists in the Explorer but has not yet been
    ' displayed in order to create a Scintilla window. Simply need to exit.
    if pSci = 0 then return true
    
    ' Encode the full Scintilla buffer to the document's target disk encoding (BOM included)
    ' through the single shared writer, then write it in one call. See Doc_EncodeForDisk for
    ' why the buffer is read by SCI_GETLENGTH and why UTF-16 no longer routes through DWSTRING.
    dim as long sciCodePage = SciMsg(pSci, SCI_GETCODEPAGE, 0, 0)   ' 0 or SC_CP_UTF8

    dim as DWSTRING wszSaveFilename
    if AfxFileExists(wszFilename) then AfxDeleteFile(wszFilename)
    wszSaveFilename = wszFilename

    dim pStream as CFileStream
    if pStream.Open(wszSaveFilename, STGM_CREATE or STGM_WRITE) = S_OK then
        dim as boolean bLossy
        dim as string sOut = Doc_EncodeForDisk( Scintilla_GetTextBytes(this.hWindow(0)), _
                                                (sciCodePage = SC_CP_UTF8), this.FileEncoding, bLossy )
        pStream.Write strptr(sOut), len(sOut)
    end if
    pStream.Close

    this.DiskFilename = wszFilename
    this.DateFileTime = AfxGetFileLastWriteTime( wszFilename )
    
    ' if this was a new document then it needs to be saved to Recent File list.
    if this.IsNewFlag then
        if gApp.IsProjectNamed() = false then
            UpdateMRUList(wszFilename)
        end if
    end if
    this.IsNewFlag = false
    
    ' Set the current state of the document to unmodified
    this.UserModified = false   
    SciMsg( pSci, SCI_SETSAVEPOINT, 0, 0)
    
    function = false
end function


''
''
function clsDocument.SaveTempFile( byval wszFilename as DWSTRING ) as boolean
    ' Save this document as a temp disk file in order to Quick Run
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    
    ' if pSci does not exist then this file exists in the Explorer but has not yet been
    ' displayed in order to create a Scintilla window. Simply need to exit.
    if pSci = 0 then return true
    
    ' Encode the full Scintilla buffer through the single shared writer (same path as SaveFile).
    dim as long sciCodePage = SciMsg(pSci, SCI_GETCODEPAGE, 0, 0)   ' 0 or SC_CP_UTF8

    dim as DWSTRING wszSaveFilename
    if AfxFileExists(wszFilename) then AfxDeleteFile(wszFilename)
    wszSaveFilename = wszFilename

    dim pStream as CFileStream
    if pStream.Open(wszSaveFilename, STGM_CREATE or STGM_WRITE) = S_OK then
        dim as boolean bLossy
        dim as string sOut = Doc_EncodeForDisk( Scintilla_GetTextBytes(this.hWindow(0)), _
                                                (sciCodePage = SC_CP_UTF8), this.FileEncoding, bLossy )
        pStream.Write strptr(sOut), len(sOut)
    end if
    pStream.Close

    function = false
end function


''
''
function clsDocument.GetTextRange( _
            byval cpMin as long, _
            byval cpMax as long _
            ) as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim p      as long
    dim buffer as string
    dim txtrg  as SCI_TEXTRANGE
    txtrg.chrg.cpMin = cpMin
    txtrg.chrg.cpMax = cpMax
    buffer = space(cpMax - cpMin + 1)
    txtrg.lpstrText = strptr(buffer)
    SciMsg(pSci, SCI_GETTEXTRANGE, 0, cast(LPARAM, @txtrg))
    p = instr(buffer, chr(0))
    if p then buffer = left(buffer, p - 1)
    function = buffer
end function

''
''
function clsDocument.ChangeSelectionCase( byval fcase as long) as long 
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim startSelPos as long     ' Starting position
    dim endSelPos   as long     ' Ending position
    dim strText     as string   ' Selected text
    dim i           as long 

    ' fcase = 1 (upper case), 2 (lower case), 3 (mixed case)
    if (fcase < 1) or (fcase > 3) then exit function
    
    ' if startSelPos and endSelPos are the same there is not selection,
    startSelPos = SciMsg( pSci, SCI_GETSELECTIONSTART, 0, 0)
    endSelPos   = SciMsg( pSci, SCI_GETSELECTIONEND, 0, 0)
    if startSelPos = endSelPos then exit function
    
    ' Retrieve the text
    strText = this.GetTextRange(startSelPos, endSelPos)
    
    ' Convert it to upper, lower case, or mixed case
    if fcase = 1 then
        strText = ucase(strText)
    elseif fcase = 2 then
        strText = lcase(strText)
    elseif fcase = 3 then
        ' Always uppercase the first character regardless
        strText = LCase(strText)
        mid(strText,1,1) = ucase(mid(strText,1,1))
        dim as string prevChar
        for i as long = 2 to len(strText)
            prevChar = mid(strText,i-1,1)
            select case prevChar
            case chr(13), chr(10), " "
                mid(strText,i,1) = ucase(mid(strText,i,1))
            end select
        next
    end if
    
    ' Replace the selected text
    SciMsg( pSci, SCI_REPLACESEL, 0, cast(LPARAM, strptr(strText)))

    function = 0
end function


''
''
function clsDocument.RemoveSelectionAttributes() as long
    ' Remove all Attribute #8 selection highlights from this document.
    dim as long startPos, endPos
    SendMessage( this.hWindow(0), SCI_INDICSETSTYLE, 8, INDIC_STRAIGHTBOX)
    SendMessage( this.hWindow(0), SCI_SETINDICATORCURRENT, 8, 0)
    SendMessage( this.hWindow(0), SCI_TARGETWHOLEDOCUMENT, 0, 0)
    startPos = SendMessage( this.hWindow(0), SCI_GETTARGETSTART, 0, 0)
    endPos =  SendMessage( this.hWindow(0), SCI_GETTARGETEND, 0, 0)
    SendMessage( this.hWindow(0), SCI_INDICATORCLEARRANGE, startPos, cast(LPARAM, endPos))
    function = 0
end function


''
''
function clsDocument.SetMarkerHighlight() as long
    ' Set a marker that will highlight the background of the current selection. This
    ' is used when we are attempting to search a selection. We want the current search
    ' area to be a different color than the regular highlighted text because any
    ' search results are colored using the normal highlight colors so we need them
    ' to stand out from the selected range.
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    SciMsg( pSci, SCI_MARKERDELETEALL, MARKER_OCCURRENCES, 0) ' Remove any existing before drawing the new highlight
    dim as long startPos, endPos, startLine, endLine
    this.GetSelectedLineRange( startLine, endLine, startPos, endPos )
    if endLine <> startLine then
        SciMsg( pSci, SCI_MARKERDEFINE, MARKER_OCCURRENCES, SC_MARK_BACKGROUND )  ' define as marker #10
        SciMsg( pSci, SCI_SETMARGINMASKN, 4, &H400 )   ' set margin mask to allow SC_MARK_BACKGROUND 
        for i as long = startLine to endLine
            function = SciMsg( pSci, SCI_MARKERADD, i, MARKER_OCCURRENCES)    ' add defined marker #10 to each line
        next
        ' set backcolor of marker MARKER_OCCURRENCES
        dim as COLORREF clr = ghEditor.BackColorSelection
        SciMsg( pSci, SCI_MARKERSETBACK, MARKER_OCCURRENCES, clr) 
    end if
end function

''
''
function clsDocument.RemoveMarkerHighlight() as long
    ' Remove any markers that were set in the document that signify a highlighted range.
    ' This is used when we are attempting to search a selection. 
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    function = SciMsg( pSci, SCI_MARKERDELETEALL, MARKER_OCCURRENCES, 0)  ' delete all marker #10
end function

''
''
function clsDocument.FirstMarkerHighlight() as long
    ' Get the first line with marker #10 highlight
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim as long markerMask 
    markerMask = bitset(markerMask, MARKER_OCCURRENCES)
    function = SciMsg( pSci, SCI_MARKERNEXT, 0, markerMask)
end function

''
''
function clsDocument.LastMarkerHighlight() as long
    ' Get the first line with marker #10 highlight
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim as long nLastPos = SciMsg( pSci, SCI_GETTEXTLENGTH, 0, 0)
    dim as long nLastLine = SciMsg( pSci, SCI_LINEFROMPOSITION, nLastPos, 0)
    dim as long markerMask 
    markerMask = bitset(markerMask, MARKER_OCCURRENCES)
    function = SciMsg( pSci, SCI_MARKERPREVIOUS, nLastLine, markerMask)
end function

''
''
function clsDocument.HasMarkerHighlight() as boolean
    ' true/False if selection markers exist in the document search for marker #10
    function = iif(this.FirstMarkerHighlight = -1, false, true)
end function

''
''
function clsDocument.GetCurrentLineNumber() as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim nPos as long = SciMsg( pSci, SCI_GETCURRENTPOS, 0, 0)
    function = SciMsg( pSci, SCI_LINEFROMPOSITION, nPos, 0)
end function

''
''
function clsDocument.SelectLine( byval nLineNum as long ) as long
    ' select the incoming nLineNum. if nLineNum is negative then select the current line
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    nLineNum = iif( nLineNum < 0, this.GetCurrentLineNumber, nLineNum)
    dim nStartPos as long = SciMsg( pSci, SCI_POSITIONFROMLINE, nLineNum, 0)
    dim nEndPos   as long = SciMsg( pSci, SCI_GETLINEENDPOSITION, nLineNum, 0)
    SciMsg( pSci, SCI_SETSELECTIONSTART, nStartPos, 0)
    SciMsg( pSci, SCI_SETSELECTIONEND, nEndPos, 0)
    function = 0
end function

''
''
function clsDocument.GetLine( byval nLine as long ) as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim nLen   as long
    dim buffer as string
    nLen = SciMsg( pSci, SCI_LINELENGTH, nLine , 0)
    if nLen < 1 then exit function
    buffer = space(nLen)
    SciMsg( pSci, SCI_GETLINE, nLine, cast(LPARAM, strptr(buffer)))
    function = rtrim(buffer, any chr(13,10))
end function

''
''
function clsDocument.SetLine( byval nLineNum as long, byval sText as string) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim nStartPos as long = SciMsg( pSci, SCI_POSITIONFROMLINE, nLineNum, 0)
    dim nEndPos   as long = SciMsg( pSci, SCI_GETLINEENDPOSITION, nLineNum, 0)
    SciMsg( pSci, SCI_SETTARGETSTART, nStartPos, 0)
    SciMsg( pSci, SCI_SETTARGETEND, nEndPos, 0)
    SciMsg( pSci, SCI_REPLACETARGET, len(sText), cast(LPARAM, strptr(sText))) 
    function = 0
end function

''
''
function clsDocument.GetSelText() as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim nLen as long
    dim buffer as string
    nLen = SciMsg( pSci, SCI_GETSELTEXT, 0, 0)
    if nLen < 1 then exit function
    buffer = space(nLen)
    SciMsg( pSci, SCI_GETSELTEXT, 0, cast(LPARAM, strptr(buffer)))
    function = buffer
end function

''
''
function clsDocument.GetText() as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim nLen   as long
    dim buffer as string
    nLen = SciMsg( pSci, SCI_GETLENGTH, 0 , 0)
    if nLen < 1 then exit function
    buffer = space(nLen+1)
    SciMsg( pSci, SCI_GETTEXT, nLen+1, cast(LPARAM, strptr(buffer)) )
    function = buffer
end function

''
''
function clsDocument.SetText( byref sText as const string ) as long 
    ' Set the text into the scintilla buffer and make the buffer not dirty
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    SciMsg( pSci, SCI_SETTEXT, 0, cast(LPARAM, strptr(sText)) )
    SciMsg( pSci, SCI_SETSAVEPOINT, 0, 0)
    SciMsg( pSci, SCI_EMPTYUNDOBUFFER, 0, 0)
    this.UserModified = false
    SciMsg( pSci, SCI_COLOURISE, 0, -1 )
    function = 0
end function


''
''
function clsDocument.AppendText( byref sText as const string ) as long 
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    SciMsg( pSci, SCI_APPENDTEXT, len(sText), cast(LPARAM, strptr(sText)) )
    SciMsg( pSci, SCI_COLOURISE, 0, -1 )
    function = 0
end function


''
''
function clsDocument.CenterCurrentLine() as long 
    ' Center the current line to the middle of the visible screen. This is useful
    ' when searching and finding text. The found text will always display in the 
    ' middle of the screen. 
    if gConfig.PositionMiddle = 0 then exit function
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim as long numLinesOnScreen = SciMsg( pSci, SCI_LINESONSCREEN, 0, 0)
    dim as long nMiddle = (this.GetCurrentLineNumber - (numLinesOnScreen / 2) + 1)
    SciMsg( pSci, SCI_SETFIRSTVISIBLELINE, nMiddle, 0)
    function = 0
end function



''
''  IsFunctionLine
''
''  Determine if incoming line number is the start of a valid sub/function.
''  By default we assume every line will be a valid function in order to
''  make the comparisons easier. The value returned is the position 
''  immediately after the declaration (0 if not found). This is useful
''  because this function can then also be used for determining the 
''  actual sub/function name rather than having to code a separate function
''  to do the same comparisons.
''  
function clsDocument.IsFunctionLine( byval lineNum as long ) as long
    
    ' default that no function declaration found
    dim as long position = 0

    ' trim and remove double spaces and replace TABs with single space
    dim as string lineText = this.GetLine( lineNum )
    if len( lineText ) < 4 then exit function
    lineText = ltrim(ucase( AfxStrShrink(lineText, chr(32,9)) ))
    if len( lineText ) < 4 then exit function
        
    if left( lineText, 9 ) = "FUNCTION " then
        ' bypass any line with = that is a "function = " line.
        dim as string subText = ltrim( mid(lineText, 9) )
        if left( subText, 1 ) <> "=" then position = 10
    elseif left(lineText, 17) = "PRIVATE FUNCTION " then
        position = 18   
    elseif left(lineText, 16) = "PUBLIC FUNCTION " then
        position = 17
    elseif left(lineText, 4)  = "SUB " then
        position = 5
    elseif left(lineText, 12) = "PRIVATE SUB " then
        position = 13
    elseif left(lineText, 11) = "PUBLIC SUB " then
        position = 12
    elseif left(lineText, 9)  = "PROPERTY " then
        ' bypass any line with = that is a "PROPERTY = " line.
        dim as string subText = ltrim( mid(lineText, 9) )
        if left( subText, 1 ) <> "=" then position = 10
    elseif left(lineText, 17) = "PRIVATE PROPERTY " then
        position = 18
    elseif left(lineText, 12) = "CONSTRUCTOR " then
        position = 13
    elseif left(lineText, 11) = "DESTRUCTOR " then
        position = 12
        
    ' if we encounter one of these end statements then our cursor must 
    ' be positioned between functions (ie we're at the module level).   
    elseif left(lineText, 12) = "END FUNCTION" then
        position = -1
    elseif left(lineText, 7)  = "END SUB" then
        position = -1
    elseif left(lineText, 15) = "END CONSTRUCTOR" then
        position = -1
    elseif left(lineText, 14) = "END DESTRUCTOR" then
        position = -1
    elseif left(lineText, 12) = "END PROPERTY" then
        position = -1

    end if

    function = position
end function


' ========================================================================================
' Determine the sub/function name based on the current editing position 
' within the file. This is needed by CodeTips (DereferenceLine) and for code navigation
' in the editor to set the functions ComboBox to the correct item.
' ========================================================================================
function clsDocument.GetCurrentFunctionName( _
            byref sfunctionName as string, _
            byref nGetSet as ClassProperty _
            ) as long
    
    dim as string lineText
    dim as string funcName 
    dim as string funcParams
    
    dim as long curLine
    dim as long position
    
    ' search up the file until we find the start of a sub/function or start of file.
    curLine = this.GetCurrentLineNumber
    nGetSet = ClassProperty.None
    
    for i as long = curLine to 0 step -1
        position = this.IsfunctionLine( i )
        
        if position = -1 then 
            ' We found an "end SUB", "end function", "end PROPERTY" line. Keep looking for 
            ' the very next instance of SUB/function and then break out of loop
            ' with that function name.
            if i = this.GetCurrentLineNumber then 
            continue for
            else
            exit for
            end if
            
        elseif position = 0 then
            ' Just a regular line... keep looking...
        
        elseif position > 0 then
            ' We found a valid SUB/function line so process it
            ' trim and remove double spaces and replace TABs with single space
            lineText = ltrim(ucase( AfxStrShrink(this.GetLine(i), chr(32,9)) ))
            funcName = ltrim(mid(lineText, position))
            funcName = AfxStrParseAny( funcName, 1, " (" )
            
            ' if this is a Property then we need to differentiate between a Get/Set
            if ( left(lineText, 9)  = "PROPERTY " ) orelse _
            ( left(lineText, 17) = "PRIVATE PROPERTY " ) then
            ' if funcParams exist then this must be a Set property. Need to sanitize the
            ' property parameters first. Need to get the starting ( and the ending ) and
            ' evaluate the text between it. Not as easy as using Parse because there could
            ' be embedded array() parameters and the property could end in something like 
            ' as long, etc.
            dim as string st
            dim as string sFuncParams = lineText
            dim as long p1, p2
            p1 = instr( sFuncParams, "(" )
            p2 = InstrRev( sFuncParams, ")" )
            if ( p1 = 0 ) orelse (p2 = 0 ) then
                st = ""
            elseif p2 > p1 then
                st = mid( sFuncParams, p1, p2 - p1 )
            end if
            st = trim( st, any "( )" )
            if len( st ) then
                nGetSet = ClassProperty.Setter
            else   
                nGetSet = ClassProperty.Getter
            end if

            elseif ( left(lineText, 12)  = "CONSTRUCTOR " ) then
            nGetSet = ClassProperty.ctor
            
            elseif ( left(lineText, 11)  = "DESTRUCTOR " ) then
            nGetSet = ClassProperty.dtor
            end if   

            exit for
        end if
    next

    sfunctionName = funcName
    nGetSet = nGetSet
    
    function = 0
end function


''
''  GotoNextfunction
''
''  Go to the next sub/function in the document
''
function clsDocument.GotoNextFunction() as long
    dim as long curLine  = this.GetCurrentLineNumber
    dim as long maxLines = this.GetLineCount - 1
    dim as long newLine  = curLine
    
    for i as long = curLine + 1 to maxLines
        if this.IsFunctionLine( i ) > 0 then
            newLine = i
            exit for
        end if
    next

    ' if we have found a new line then reposition to that line
    if newLine <> curLine then
        dim as any ptr pSci = this.GetActiveScintillaPtr()
        SciMsg( pSci, SCI_SETFIRSTVISIBLELINE, newLine - 5, 0) 
        SciMsg( pSci, SCI_GOTOLINE, newLine, 0) 
    end if
    
    function = 0
end function


''
''  GotoPrevfunction
''
''  Go to the previous sub/function in the document
''
function clsDocument.GotoPrevFunction() as long
    dim as long curLine  = this.GetCurrentLineNumber
    dim as long newLine  = curLine
    
    for i as long = curLine - 1 to 0 step -1
        if this.IsFunctionLine( i ) > 0 then
            newLine = i
            exit for
        end if
    next

    ' if we have found a new line then reposition to that line
    if newLine <> curLine then
        dim as any ptr pSci = this.GetActiveScintillaPtr()
        SciMsg( pSci, SCI_SETFIRSTVISIBLELINE, newLine - 5, 0) 
        SciMsg( pSci, SCI_GOTOLINE, newLine, 0) 
    end if
    
    function = 0
end function


''
''
function clsDocument.GetSelectedLineRange( _
            byref startLine as long, _
            byref endLine   as long, _
            byref startPos  as long, _
            byref endPos    as long _
            ) as long 
    
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    startPos  = SciMsg( pSci, SCI_GETSELECTIONSTART, 0, 0) 
    endPos    = SciMsg( pSci, SCI_GETSELECTIONEND, 0, 0) 
    startLine = SciMsg( pSci, SCI_LINEFROMPOSITION, startPos, 0) 
    endLine   = SciMsg( pSci, SCI_LINEFROMPOSITION, endPos, 0) 

    dim nCol as long = SciMsg( pSci, SCI_GETCOLUMN, endPos, 0)
    if (nCol = 0) andalso (endLine > startLine) then endLine = endLine - 1

    function = 0
end function


''
''
function clsDocument.BlockComment( byval flagBlock as boolean ) as long
    dim i           as long        ' loop counter
    dim firstPos    as long        ' Starting position of the line
    dim startPos    as long        ' Starting position of selection
    dim endPos      as long        ' Ending position of selection
    dim startLine   as long        ' Starting line number
    dim endLine     as long        ' Ending line number
    dim nPos        as long        ' Position
    dim strText     as string      ' Portion of the line to replace
    dim strNewText  as string      ' new text to insert or delete
    dim nCount      as long        ' Number of "'" added or removed
    
    ' flagBlock
    '   true   comment selected lines
    '   false  uncomment selected lines
    
    dim blockChars as string = "'"
    
    ' If this is a resource file then use different block characters
    if lcase(AfxStrPathname( "EXTN", this.DiskFilename)) = ".rc" then
        blockChars = "//"
    end if
    
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    this.GetSelectedLineRange( startLine, endLine, startPos, endPos )
    
    gApp.SuppressNotify = true
    SetWindowRedraw( this.hWndActiveScintilla, false )

    SciMsg( pSci, SCI_BEGINUNDOACTION, 0, 0)
    for i = startLine to endLine
        strText = this.GetLine(i)   
        if flagBlock = false then   
            ' unblock comment
            ' Bypass leading spaces and tabs in order to find the firstpos
            firstpos = -1
            for ii as integer = 0 to len(strText) - 1
                if (strText[ii] <> 32) andalso (strText[ii] <> 9) then 
                    firstpos = ii: exit for
                end if
            next
            if firstpos = -1 then continue for
                
            if mid(strText, firstpos + 1, len(blockChars)) <> blockChars then
                continue For
            else   
                firstpos = SciMsg( pSci, SCI_POSITIONFROMLINE, i, 0 ) + firstpos
                SciMsg( pSci, SCI_DELETERANGE, firstpos, cast(LPARAM, len(blockChars)) )
            end if
            nCount += 1
        else
            ' block comment
            if len(rtrim(strText)) then
                strNewText = blockChars
                firstpos = SciMsg( pSci, SCI_POSITIONFROMLINE, i, 0 )
                SciMsg( pSci, SCI_INSERTTEXT, firstpos, cast(LPARAM, strptr(strNewText)) )
                nCount += 1
            end if   
        end if
    next
    SciMsg( pSci, SCI_ENDUNDOACTION, 0, 0)

    if startPos <> endPos then
        SciMsg( pSci, SCI_SETSELECTIONSTART, startPos, 0)
        SciMsg( pSci, SCI_SETSELECTIONEND, endPos + iif(flagBlock, nCount, -nCount), 0)
    else
        SciMsg( pSci, SCI_SETSELECTIONSTART, endPos + iif(flagBlock, nCount, -nCount), 0)
        SciMsg( pSci, SCI_SETSELECTIONEND, endPos + iif(flagBlock, nCount, -nCount), 0)
    end if

    SetWindowRedraw( this.hWndActiveScintilla, true )
    gApp.SuppressNotify = false
    AfxRedrawWindow( this.hWndActiveScintilla )
    
    function = 0
end function


''
''
function clsDocument.CurrentLineUp() as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim currentLine as long = this.GetCurrentLineNumber()
    if (currentLine <> 0) then
        SciMsg( pSci, SCI_BEGINUNDOACTION, 0, 0)
        currentLine = currentLine -1
        SciMsg( pSci, SCI_LINETRANSPOSE, 0, 0)
        SciMsg( pSci, SCI_GOTOLINE, currentLine, 0)
        SciMsg( pSci, SCI_ENDUNDOACTION, 0, 0)
    end if
    function = 0
end function


''
''
function clsDocument.GetLineCount() as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    function = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0)
end function


''
''
function clsDocument.CurrentLineDown() as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim currentLine as long = this.GetCurrentLineNumber()
    if currentLine <> SciMsg( pSci, SCI_GETLINECOUNT, 0, 0) - 1 then
        SciMsg( pSci, SCI_BEGINUNDOACTION, 0, 0)
        currentLine = currentLine + 1
        SciMsg( pSci, SCI_GOTOLINE, currentLine, 0)
        SciMsg( pSci, SCI_LINETRANSPOSE, 0, 0)
        SciMsg( pSci, SCI_ENDUNDOACTION, 0, 0)
    end if
    function = 0
end function
    
''
''
function clsDocument.MoveCurrentLines( byval flagMoveDown as boolean ) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    if flagMoveDown then
        SciMsg( pSci, SCI_MOVESELECTEDLINESDOWN, 0, 0)
    else   
        SciMsg( pSci, SCI_MOVESELECTEDLINESUP, 0, 0)
    end if
    function = 0
end function


''
''
function clsDocument.NewLineBelowCurrent() as long
    ' From anywhere on the current line creates a new line immediately
    ' below the current line and positions the cursor to the start of
    ' that new line.
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    SciMsg( pSci, SCI_LINEEND, 0, 0)
    SciMsg( pSci, SCI_NEWLINE, 0, 0)
    function = 0
end function


''
''
function clsDocument.ToggleMarker( byval nLine as long, byval MarkerType as long ) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim markers as long  ' must be a 32 bit value
    markers = SciMsg( pSci, SCI_MARKERGET, nLine, 0 ) 
    
    ' check if bookmark marker is present (bit mask check)
    if (markers and (1 shl MarkerType)) then
        ' bookmark exists, remove it
        SciMsg( pSci, SCI_MARKERDELETE, nLine, MarkerType )
    else
        ' bookmark doesn't exist, add it
        SciMsg( pSci, SCI_MARKERADD, nLine, MarkerType )
    end if
    function = 0
end function

''
''
function clsDocument.ToggleBookmark( byval nLine as long ) as long
    return this.ToggleMarker( nLine, MARKER_BOOKMARK )
end function

''
''
function clsDocument.ToggleBreakPoint( byval nLine as long ) as long
    ' If the Debugger is active then simply make it a breakpoint
    ' rather than a persistemt breakpoint that will get saved between sessions.
    if gApp.IsDebuggerActive = true then
        ' Tell the engine directly. The gdb path needed a two-message round trip to delete
        ' one breakpoint (-break-list, then match the reply, then -break-delete), and its
        ' add path was the one site of four that forgot the 0-based to 1-based conversion,
        ' so a breakpoint set mid-session landed a line early.
        dim as any ptr pSci = this.GetActiveScintillaPtr()
        dim as long fMark = SciMsg( pSci, SCI_MARKERGET, nLine, 0)  ' 32 bit value
        dim as wstring * 520 wszFile = this.DiskFilename
        dim as long si = debugparser_source_index( @wszFile )
        if si >= 0 then
            if bit(fMark, MARKER_BREAKPOINT) then
                debugparser_remove_breakpoint( si, nLine + 1 )
            else
                dim as long lnExec = debugparser_next_executable_line( si, nLine + 1 )
                if lnExec > 0 then debugparser_add_breakpoint( si, lnExec )
            end if
        end if
    end if
    
    this.ToggleMarker( nLine, MARKER_BREAKPOINT )

    return 0
end function

''
''
function clsDocument.NextBookmark() as long 
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim fMark as long  ' 32 bit value
    dim nCurLine as long = this.GetCurrentLineNumber() 
    dim nLine as long = this.GetCurrentLineNumber() + 1  ' start line
    fMark = bitset(fMark, MARKER_BOOKMARK)
    nLine = SciMsg( pSci, SCI_MARKERNEXT, nLine, fMark)
    if nLine > -1 then
        SciMsg( pSci, SCI_GOTOLINE, nLine, 0)
    else
        nLine = SciMsg( pSci, SCI_MARKERNEXT, nLine, fMark)
        if nLine > -1 then
            SciMsg( pSci, SCI_GOTOLINE, nLine, 0)
        end if
    end if
    if nLine <> -1 then
        if nLine <> nCurLine then this.CenterCurrentLine()
    end if
    function = 0 
end function

''
''
function clsDocument.PrevBookmark() as long 
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim fMark  as long  ' 32 bit value
    dim nCurLine as long = this.GetCurrentLineNumber() 
    dim nLine  as long = this.GetCurrentLineNumber() - 1   ' start line
    dim nLines as long = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0) - 1
    fMark = bitset(fMark, MARKER_BOOKMARK)
    nLine = SciMsg( pSci, SCI_MARKERPREVIOUS, nLine, fMark)
    if nLine > -1 then
        SciMsg( pSci, SCI_GOTOLINE, nLine, 0)
    else
        nLine = SciMsg( pSci, SCI_MARKERPREVIOUS, nLines, fMark)
        if nLine > -1 then
            SciMsg( pSci, SCI_GOTOLINE, nLine, 0)
        end if
    end if
    if nLine <> -1 then
        if nLine <> nCurLine then this.CenterCurrentLine()
    end if
    function = 0 
end function

''
''   
function clsDocument.FoldToggle( byval nLine as long ) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim nFoldLevel as long = SciMsg( pSci, SCI_GETFOLDLEVEL, nLine, 0)

    if (nFoldLevel and SC_FOLDLEVELHEADERFLAG) = 0 then
        ' Get the number of the head line of the procedure or function
        nLine = SciMsg( pSci, SCI_GETFOLDPARENT, nLine, 0) 
    end if
    if nLine > -1 then
        SciMsg( pSci, SCI_TOGGLEFOLD, nLine, 0) 
        SciMsg( pSci, SCI_GOTOLINE, nLine, 0)
    end if

    function = nLine
end function

''
''
function clsDocument.FoldAll() as long

    dim i          as long    
    dim nLines     as long    
    dim nFoldLevel as long
    
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Force the lexer to style the whole document
    SciMsg( pSci, SCI_COLOURISE, -1, 0)

    nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0)

    for i = 0 to nLines
        ' if we are in the head line ...
        nFoldLevel = SciMsg( pSci, SCI_GETFOLDLEVEL, i, 0)
        if (nFoldLevel and SC_FOLDLEVELNUMBERMASK) = SC_FOLDLEVELBASE then
            if SciMsg( pSci, SCI_GETFOLDEXPANDED, i, 0) then
            SciMsg( pSci, SCI_TOGGLEFOLD, i, 0) 
            end if
        end if
    next

    function = 0
end function

''
''
function clsDocument.UnFoldAll() as long
    dim i          as long    
    dim nLines     as long    
    dim nFoldLevel as long
    
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Force the lexer to style the whole document
    SciMsg( pSci, SCI_COLOURISE, -1, 0 )

    nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0)

    for i = 0 to nLines
        ' if we are in the head line ...
        nFoldLevel = SciMsg( pSci, SCI_GETFOLDLEVEL, i, 0)
        if (nFoldLevel and SC_FOLDLEVELNUMBERMASK) = SC_FOLDLEVELBASE then
            if SciMsg( pSci, SCI_GETFOLDEXPANDED, i, 0) = 0 then
            SciMsg( pSci, SCI_TOGGLEFOLD, i, 0) 
            end if
        end if
    next

    function = 0
end function

''
''
function clsDocument.FoldToggleOnwards( byval nLine as long) as long
    ' Toggles the curent fold point and all folds below it that are of greater depth
    dim i          as long    
    dim nLines     as long    
    dim nFoldLevel as long
    dim nFoldLevelBase as long
    dim FoldState  as long
    
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Force the lexer to style the whole document
    SciMsg( pSci, SCI_COLOURISE, -1, 0 )

    nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0)

    ' Toggle the first sub or function
    nLine = this.FoldToggle(nLine)
    nFoldLevelBase = SciMsg( pSci, SCI_GETFOLDLEVEL, nLine, 0)
    
    ' Determine whether the fold is expanded or not
    FoldState = SciMsg( pSci, SCI_GETFOLDEXPANDED, nLine, 0)

    for i = nLine to nLines
        nFoldLevel = SciMsg( pSci, SCI_GETFOLDLEVEL, i, 0)
        if nFoldLevel > nFoldLevelBase then
            ' if the state is different ...
            if SciMsg( pSci, SCI_GETFOLDEXPANDED, i, 0) <> FoldState then
            SciMsg( pSci, SCI_TOGGLEFOLD, i, 0) 
            end if
        end if
    next

    function = 0
end function

''
''
function clsDocument.ConvertEOL( byval nMode as long) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    function = SciMsg( pSci, SCI_CONVERTEOLS, nMode, 0)
    SciMsg( pSci, SCI_SETEOLMODE, nMode, 0)
end function

''
''
function clsDocument.TabsToSpaces() as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim as long i, n, nLen, nLines, TabSize, nLineNumber 
    dim as string strText, strBuffer

    ' Get the current line
    nLineNumber = this.GetCurrentLineNumber
    ' Get the tab size
    TabSize = SciMsg( pSci, SCI_GETTABWIDTH, 0, 0)
    if TabSize < 1 then exit function
    ' Get the length of the text
    nLen  = SciMsg( pSci, SCI_GETTEXTLENGTH, 0, 0) 
    ' Get the number of lines
    nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0)
    ' Remove tabs, line by line
    for i = 0 to nLines - 1
        strText = this.GetLine(i)
        n = 1
        Do
            n = instr(n, strText, chr(9))
            if n > 0 then 
            strText = left(strText, n - 1) & space(TabSize) & mid(strText, n + 1)
            n += 1
            end if   
        loop until n = 0
        strBuffer = strBuffer & strText & chr(13,10)
    next
    ' Set the new text
    this.SetText(strBuffer)
    ' Set the caret position
    SciMsg( pSci, SCI_GOTOLINE, nLineNumber, 0) 

    function = 0
end function


''
''
function clsDocument.IsMultiLineSelection() as boolean
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    dim as long startPos, endPos, startLine, endLine
    startPos  = SciMsg( pSci, SCI_GETSELECTIONSTART, 0, 0) 
    endPos    = SciMsg( pSci, SCI_GETSELECTIONEND, 0, 0) 
    startLine = SciMsg( pSci, SCI_LINEFROMPOSITION, startPos, 0) 
    endLine   = SciMsg( pSci, SCI_LINEFROMPOSITION, endPos, 0) 
    if endLine <> startLine then return true
end function

 
''
''
' ========================================================================================
' Apply every PER-VIEW property to one Scintilla view.
'
' Extracted verbatim from the loop that used to sit inside ApplyProperties, changing only
' pSci(i) -> pSciView. It is a METHOD rather than a free function precisely so that every
' "this." reference in the body stays valid and the extraction is a one-identifier change.
'
' It exists because STYLES ARE PER-VIEW IN SCINTILLA while the LEXER IS PER-DOCUMENT: a
' second view onto a document inherits its lexer for free, but renders with default styles
' until this has been run against it. Find in Project's excerpt views are exactly that, so
' they call this too -- see FipPool_Bind.
'
' The gConfig-derived locals are recomputed here rather than passed in, which is what keeps
' the signature to the one thing that actually varies.
' ========================================================================================
sub clsDocument.ApplyPropertiesToView( byval pSciView as any ptr )
    if pSciView = 0 then exit sub

    dim nPixels     as long
    dim bitsNeeded  as long
    dim wFileExt    as wstring * MAX_PATH
    dim strFontName as string
    dim nFontExtraSpace as long
    dim nFontSize   as long
    dim nFontcase   as long
    dim rxRatio     as single = 1
    dim ryRatio     as single = 1

    ' DPI ratios come from the DOCUMENT's own window, not the caller's: an excerpt view is
    ' parented elsewhere, and a plain registered class would silently give a ratio of 1 and
    ' put every margin at the wrong width above 100%.
    dim pWindow as CWindow ptr = AfxCWindowOwnerPtr(this.hWindow(0))
    if pWindow then
        rxRatio = pWindow->rxRatio
        ryRatio = pWindow->ryRatio
    end if

    strFontName = str(gConfig.EditorFontname)
    nFontSize   = val(gConfig.EditorFontsize)
    nFontExtraSpace = val(gConfig.FontExtraSpace)

    select case gConfig.KeywordCase
        case 0:  nFontcase = SC_CASE_LOWER
        case 1:  nFontcase = SC_CASE_UPPER
        case 2:  nFontcase = SC_CASE_CAMEL
        case 3:  nFontcase = SC_CASE_MIXED    ' original case
    end select


		SciMsg( pSciView, SCI_SETFONTQUALITY, SC_EFF_QUALITY_LCD_OPTIMIZED, 0)

        ' Add Brace Highlighting functionality:
        ' We use indicators because the lexer can overwrite the colors set STYLE_BRACELIGHT and
        ' STYLE_BRACEBAD once they are outputted to the screen.
        ' First, we set the style of the indicator number we want to use to the Box style
        SciMsg( pSciView, SCI_INDICSETSTYLE, 9, INDIC_BOX )
        'SciMsg( pSciView, SCI_INDICSETSTYLE, 9, INDIC_PLAIN) 
        ' then, we make that indicator the current one
        SciMsg( pSciView, SCI_SETINDICATORCURRENT, 9, 0 )
        ' Lastly, we apply the indicator (in this case), to a single char
        SciMsg( pSciView, SCI_BRACEHIGHLIGHTINDICATOR, 1, 9 )  ' use indicator for SCI_BRACEHIGHLIGHTINDICATOR
        SciMsg( pSciView, SCI_BRACEBADLIGHTINDICATOR, 1, 9 )   ' and SCI_BRACEBADLIGHTINDICATOR

        SciMsg( pSciView, SCI_STYLESETFONT, STYLE_DEFAULT, cast(LPARAM, strptr(strFontName)) )
        SciMsg( pSciView, SCI_STYLESETSIZE, STYLE_DEFAULT, nFontSize )
        SciMsg( pSciView, SCI_SETEXTRAASCENT, nFontExtraSpace, 0 )
        SciMsg( pSciView, SCI_SETEXTRADESCENT, nFontExtraSpace, 0 )
        SciMsg( pSciView, SCI_STYLESETCHARACTERSET, STYLE_DEFAULT, GetFontCharSetID(gConfig.EditorFontCharset) )

        SciMsg( pSciView, SCI_STYLESETFORE, STYLE_DEFAULT, ghEditor.ForeColorText)
        SciMsg( pSciView, SCI_STYLESETBACK, STYLE_DEFAULT, ghEditor.BackColorText)
        SciMsg( pSciView, SCI_STYLESETBOLD, STYLE_DEFAULT, ghEditor.TextBold )
        SciMsg( pSciView, SCI_STYLESETITALIC, STYLE_DEFAULT, ghEditor.TextItalic )
        SciMsg( pSciView, SCI_STYLESETUNDERLINE, STYLE_DEFAULT, ghEditor.TextUnderline )
        SciMsg( pSciView, SCI_STYLECLEARALL, 0, 0 )  ' Copies global style to all others
            
        ' Set the style for the AutoComplete popup list
        SciMsg( pSciView, SCI_STYLESETFONT, STYLE_AUTOCOMPLETE, cast(LPARAM, pWindow->DefaultFontName) )
        SciMsg( pSciView, SCI_STYLESETSIZE, STYLE_AUTOCOMPLETE, pWindow->DefaultFontSize)
        SciMsg( pSciView, SCI_STYLESETCHARACTERSET, STYLE_AUTOCOMPLETE, GetFontCharSetID(gConfig.EditorFontCharset) )

        ' define bookmark marker (white circle)
        SciMsg( pSciView, SCI_MARKERDEFINE, MARKER_BOOKMARK, SC_MARK_VERTICALBOOKMARK )
        SciMsg( pSciView, SCI_MARKERSETFORE, MARKER_BOOKMARK, ghEditor.ForeColorBookmark)
        SciMsg( pSciView, SCI_MARKERSETBACK, MARKER_BOOKMARK, ghEditor.BackColorBookmark )
        
        ' define breakpoint marker (red circle)
        SciMsg( pSciView, SCI_MARKERDEFINE, MARKER_BREAKPOINT, SC_MARK_CIRCLE )
        SciMsg( pSciView, SCI_MARKERSETFORE, MARKER_BREAKPOINT, ghEditor.ForeColorBreakpoint )
        SciMsg( pSciView, SCI_MARKERSETBACK, MARKER_BREAKPOINT, ghEditor.BackColorBreakpoint )

        ' Arm the mouse dwell notification. Without a time set Scintilla's default is
        ' SC_TIME_FOREVER, SCN_DWELLSTART never arrives, and the debugger's hover datatips
        ' silently never appear. Half the double-click time, so it follows the machine's own
        ' setting rather than a number invented here.
        '
        ' (Carried in from the debugger branch during the merge: it was added inside the loop
        '  this method was extracted FROM, so it belongs here now. Find in Project's excerpt
        '  views get it too, which is harmless -- they raise no datatips of their own.)
        SciMsg( pSciView, SCI_SETMOUSEDWELLTIME, GetDoubleClickTime() \ 2, 0 )

        ' define debugger cuurent line marker (yellow arrow)
        SciMsg( pSciView, SCI_MARKERDEFINE, MARKER_DEBUGGER_CURLINE, SC_MARK_SHORTARROW )
        SciMsg( pSciView, SCI_MARKERSETFORE, MARKER_DEBUGGER_CURLINE, ghEditor.ForeColorDebugLine )
        SciMsg( pSciView, SCI_MARKERSETBACK, MARKER_DEBUGGER_CURLINE, ghEditor.BackColorDebugLine )
        ''
        ''  MARGIN 0: Line Numbering (defaults to width 0)
        nPixels = SciMsg( pSciView, SCI_TEXTWIDTH, 0, cast(LPARAM, @"_99999"))
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER )
        SciMsg( pSciView, SCI_STYLESETFORE, STYLE_LINENUMBER, ghEditor.ForeColorLinenumbers )
        SciMsg( pSciView, SCI_STYLESETBACK, STYLE_LINENUMBER, ghEditor.BackColorLinenumbers )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 0, iif(gConfig.LineNumbering, nPixels, 0) )
        SciMsg( pSciView, SCI_STYLESETBOLD, STYLE_LINENUMBER, ghEditor.LineNumbersBold )
        SciMsg( pSciView, SCI_STYLESETITALIC, STYLE_LINENUMBER, ghEditor.LineNumbersItalic )
        SciMsg( pSciView, SCI_STYLESETUNDERLINE, STYLE_LINENUMBER, ghEditor.LineNumbersUnderline )
    
        ''
        ''  MARGIN 1: Non-Folding symbols (defaults to width 16) (Bookmark symbol, etc) (will be same color as line numbering)
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 1, SC_MARGIN_TEXT )
        SciMsg( pSciView, SCI_SETMARGINSENSITIVEN, 1, 1 )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 1, iif(gConfig.LeftMargin, 16 * rxRatio, 0) )
                    
        ''
        ''  MARGIN 2: Folding symbols (defaults to width 0)
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL )
        SciMsg( pSciView, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS )
        SciMsg( pSciView, SCI_SETFOLDMARGINCOLOUR, Ctrue, ghEditor.ForeColorFoldmargin )
        SciMsg( pSciView, SCI_SETFOLDMARGINHICOLOUR, Ctrue, ghEditor.ForeColorFoldmargin )
        SciMsg( pSciView, SCI_SETMARGINSENSITIVEN, 2, 1 )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 2, iif(gConfig.FoldMargin, 16 * rxRatio, 0) )

        ''
        ''  MARGIN 3: Small margin to offset left margins from actual text (4 pixels)
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 3, SC_MARGIN_TEXT )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 3, 4 * rxRatio )
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 3, SC_MARGIN_FORE )
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 3, SC_MARGIN_BACK )
                
        ''
        ''  CONFINE CARET to TEXT
        if gConfig.ConfineCaret then
            SciMsg( pSciView, SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION, 0 )
        else
            SciMsg( pSciView, SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION or SCVS_USERACCESSIBLE, 0 )
        end if
    
        ''
        ''  TABS as SPACES
        if gConfig.TabIndentSpaces then
            SciMsg( pSciView, SCI_SETUSETABS, false, 0 )
        else
            SciMsg( pSciView, SCI_SETUSETABS, Ctrue, 0 )
        end if
        
        ''
        ''  SELECTIONS FILL ENTIRE SCREEN SPACE
        SciMsg( pSciView, SCI_SETSELEOLFILLED, Ctrue, 0 )
            
        ''
        ''  TAB WIDTH 
        SciMsg( pSciView, SCI_SETTABWIDTH, val(gConfig.TabSize), 0 )
        SciMsg( pSciView, SCI_SETINDENT, val(gConfig.TabSize), 0 )
    
        ''
        ''  INDENTATION GUIDES
        if gConfig.IndentGuides then
            SciMsg( pSciView, SCI_SETINDENTATIONGUIDES, Ctrue, 0)
        else
            SciMsg( pSciView, SCI_SETINDENTATIONGUIDES, false, 0)
        end if
        SciMsg( pSciView, SCI_STYLESETFORE, STYLE_INDENTGUIDE, ghEditor.ForeColorIndentguides )
        SciMsg( pSciView, SCI_STYLESETBACK, STYLE_INDENTGUIDE, ghEditor.BackColorIndentguides )
        
        ''
        ''  CARET
        SciMsg( pSciView, SCI_SETCARETFORE, ghEditor.ForeColorCaret, 0 )
        SciMsg( pSciView, SCI_SETCARETWIDTH, 3, 0 )   '(1,2,3  Default is 1)

        ''
        ''  SHOW CARET LINE
        if gConfig.HighlightCurrentLine then
            SciMsg( pSciView, SCI_SETCARETLINEVISIBLE, Ctrue, 0 )
            SciMsg( pSciView, SCI_SETCARETLINEBACK, ghEditor.BackColorCurrentline, 0 )
        else
            SciMsg( pSciView, SCI_SETCARETLINEVISIBLE, false, 0 )
        end if
        
        ''
        ''  SELECTION COLORS
        SciMsg( pSciView, SCI_SETSELFORE, Ctrue, ghEditor.ForeColorSelection )
        SciMsg( pSciView, SCI_SETSELBACK, Ctrue, ghEditor.BackColorSelection )
    
        ''
        ''  MULTIPLE SELECTIONS
        SciMsg( pSciView, SCI_SETMULTIPLESELECTION, Ctrue, 0 ) 
                
        ''
        ''  ALWAYS KEEP THE CARET LINE VISIBLE
        SciMsg( pSciView, SCI_SETCARETLINEVISIBLEALWAYS, Ctrue, 0 )
        
        ''
        ''  DISABLE RIGHT CLICK POPUP MENU
        SciMsg( pSciView, SCI_USEPOPUP, false, 0 )

        ''
        ''  IDENTIFY CHARACTERS to BE USED IN WORDS
        SciMsg( pSciView, SCI_SETWORDCHARS, 0, cast(LPARAM, @"~_\abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") )


        ''  UNICODE (UTF-8 encoding)
        if this.FileEncoding = FILE_ENCODING_ANSI then
            SciMsg( pSciView, SCI_SETCODEPAGE, 0, 0 )
        else
            ' UTF8 or UTF16 would have been converted to UTF8 in order to display in the editor.
            SciMsg( pSciView, SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
        end if

    
        '' RIGHT EDGE COLUMN
        SciMsg( pSciView, SCI_SETEDGEMODE, iif(gConfig.RightEdge, EDGE_LINE, EDGE_NONE), 0 )
        SciMsg( pSciView, SCI_SETEDGECOLUMN, val(gConfig.RightEdgePosition), 0 )

        ''
        ''  OTHER
        SciMsg( pSciView, SCI_SETADDITIONALSELECTIONTYPING, true, 0 )
    

        ''
        ''  APPLY ALL LANGUAGE SPECIFIC SYNTAX COLORING
        wFileExt = AfxStrPathname( "EXTN", this.DiskFilename )
        wFileExt = ucase(wFileExt)

        if cbool(wFileExt = ".BAS") orelse cbool(wFileExt = ".INC") _
            orelse cbool(wFileExt = ".BI") orelse (this.IsNewFlag = true) then

            bitsNeeded = SciMsg( pSciView, SCI_GETSTYLEBITSNEEDED, 0, 0)
            SciMsg( pSciView, SCI_SETSTYLEBITS, bitsNeeded, 0 )

            ' Set FreeBASIC Keywords
            if len(gConfig.FBKeywords) then
                SciMsg( pSciView, SCI_SETKEYWORDS, 0, cast(LPARAM, strptr(gConfig.FBKeywords)) )
            end if

            ' Set Windows Api Keywords
            if len(gConfig.WinApiKeywords) then
                SciMsg( pSciView, SCI_SETKEYWORDS, 1, cast(LPARAM, strptr(gConfig.WinApiKeywords)) )
            end if

            ' Set Extra Keywords
            if len(gConfig.ExtraKeywords) then
                SciMsg( pSciView, SCI_SETKEYWORDS, 2, cast(LPARAM, strptr(gConfig.ExtraKeywords)) )
            end if
            
            if gConfig.SyntaxHighlighting then
            ' Set the Default text colors
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_DEFAULT, ghEditor.ForeColorText)
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_DEFAULT, ghEditor.BackColorText)
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_DEFAULT, ghEditor.TextBold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_DEFAULT, ghEditor.TextItalic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_DEFAULT, ghEditor.TextUnderline )

            ' Set the Multiline Comments style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_MULTILINECOMMENT, ghEditor.ForeColorComments )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_MULTILINECOMMENT, ghEditor.BackColorComments )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_MULTILINECOMMENT, ghEditor.CommentsBold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_MULTILINECOMMENT, ghEditor.CommentsItalic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_MULTILINECOMMENT, ghEditor.CommentsUnderline )

            ' Set the Comments style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_COMMENT, ghEditor.ForeColorComments )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_COMMENT, ghEditor.BackColorComments )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_COMMENT, ghEditor.CommentsBold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_COMMENT, ghEditor.CommentsItalic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_COMMENT, ghEditor.CommentsUnderline )

            ' Set the Keywords style (FreeBasic)
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_KEYWORD, ghEditor.ForeColorKeyword )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_KEYWORD, ghEditor.BackColorKeyword )
            SciMsg( pSciView, SCI_STYLESETCASE, SCE_B_KEYWORD, nFontcase )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_KEYWORD, ghEditor.KeywordBold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_KEYWORD, ghEditor.KeywordItalic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_KEYWORD, ghEditor.KeywordUnderline )

            ' Set the Keywords style (Windows Api)
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_KEYWORD2, ghEditor.ForeColorKeyword2 )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_KEYWORD2, ghEditor.BackColorKeyword2 )
            SciMsg( pSciView, SCI_STYLESETCASE, SCE_B_KEYWORD2, nFontcase )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_KEYWORD2, ghEditor.KeywordBold2 )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_KEYWORD2, ghEditor.KeywordItalic2 )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_KEYWORD2, ghEditor.KeywordUnderline2 )

            ' Set the Keywords style (Extra Keywords)
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_KEYWORD3, ghEditor.ForeColorKeyword3 )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_KEYWORD3, ghEditor.BackColorKeyword3 )
            SciMsg( pSciView, SCI_STYLESETCASE, SCE_B_KEYWORD3, nFontcase )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_KEYWORD3, ghEditor.KeywordBold3 )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_KEYWORD3, ghEditor.KeywordItalic3 )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_KEYWORD3, ghEditor.KeywordUnderline3 )

            ' Set the Numbers style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_NUMBER, ghEditor.ForeColorNumbers )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_NUMBER, ghEditor.BackColorNumbers )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_NUMBER, ghEditor.NumbersBold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_NUMBER, ghEditor.NumbersItalic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_NUMBER, ghEditor.NumbersUnderline)

            ' Set the Operators style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_OPERATOR, ghEditor.ForeColorOperators )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_OPERATOR, ghEditor.BackColorOperators )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_OPERATOR, ghEditor.OperatorsBold)
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_OPERATOR, ghEditor.OperatorsItalic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_OPERATOR, ghEditor.OperatorsUnderline)

'            ' Set the "date" style. We use this for @()[]{} brace characters
'            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_DATE, ghEditor.ForeColorBraceGood)
'            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_DATE, ghEditor.BackColorText)
'            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_DATE, ghEditor.TextBold )
'            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_DATE, ghEditor.TextItalic )
'            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_DATE, ghEditor.TextUnderline )

            ' Set the Preprocessor style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_PREPROCESSOR, ghEditor.ForeColorPreprocessor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_PREPROCESSOR, ghEditor.BackColorPreprocessor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_PREPROCESSOR, ghEditor.PreprocessorBold)
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_PREPROCESSOR, ghEditor.PreprocessorItalic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_PREPROCESSOR, ghEditor.PreprocessorUnderline)

            ' Set the strings style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_STRING, ghEditor.ForeColorStrings )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_STRING, ghEditor.BackColorStrings )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_STRING, ghEditor.StringsBold)
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_STRING, ghEditor.StringsItalic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_STRING, ghEditor.StringsUnderline)

            end if
        end if
    
        ''
        ''  CODE FOLDING
        if gConfig.FoldMargin then
            ' Enable folding of the procedures and functions
            SciMsg( pSciView, SCI_SETPROPERTY, cast(WPARAM, @"fold"), cast(LPARAM, @"1") )

            ' Initialize fold symbols for folding - Box tree
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN,    SC_MARK_BOXMINUS )
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER,        SC_MARK_BOXPLUS )
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB,     SC_MARK_VLINE)
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL,    SC_MARK_LCORNER)
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND,     SC_MARK_BOXPLUSCONNECTED)
            'SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY)   ' SC_MARK_BOXMINUSCONNECTED
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED)
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER)

            ' Draw line below if not expanded
            SciMsg( pSciView, SCI_SETFOLDFLAGS, 16, 0 )

            ' Colors for folders closed and folders opened
            dim as COLORREF clrFore = ghEditor.ForeColorFoldsymbol
            dim as COLORREF clrBack = ghEditor.BackColorFoldsymbol

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, clrFore)

            SciMsg( pSciView, SCI_MARKERENABLEHIGHLIGHT, false, 0)
        
        else
            ' Disable folding of the procedures and functions
            SciMsg( pSciView, SCI_SETPROPERTY, cast(WPARAM, @"fold"), cast(LPARAM, @"0") )
        end if
    
end sub

function clsDocument.ApplyProperties() as long
    dim nCount      as long 
    dim i           as long
    dim nPixels     as long  
    dim bitsNeeded  as long 
    dim wFileExt    as wstring * MAX_PATH
    dim strFontName as string 
    dim nFontExtraSpace as long  
    dim nFontSize   as long  
    dim nFontcase   as long 
    dim rxRatio     as single = 1
    dim ryRatio     as single = 1

    
    if pSci(0) = 0 then exit function

    ' Determine the pWindow parent of the Scintilla window in order
    ' to ensure that DPI ratios are correctly used.
    dim pWindow as CWindow ptr = AfxCWindowOwnerPtr(this.hWindow(0))
    if pWindow then
        rxRatio = pWindow->rxRatio
        ryRatio = pWindow->ryRatio
    end if   
    
    ' if this is a read-only file then set the flag so that the document can not be edited.
    if AfxIsReadOnlyFile( this.DiskFilename ) then
        SciMsg( pSci(0), SCI_SETREADONLY, 1, 0)
    else
        SciMsg( pSci(0), SCI_SETREADONLY, 0, 0)
    end if

    strFontName = str(gConfig.EditorFontname)
    nFontSize   = val(gConfig.EditorFontsize)
    nFontExtraSpace = val(gConfig.FontExtraSpace)
    
    select case gConfig.KeywordCase
        case 0:  nFontcase = SC_CASE_LOWER
        case 1:  nFontcase = SC_CASE_UPPER
        case 2:  nFontcase = SC_CASE_CAMEL   
        case 3:  nFontcase = SC_CASE_MIXED    ' original case
    end select            
    
    
    ' Must apply all settings/styles to each Scintilla split window to ensure that
    ' they all appear and act the same. 
    
    ' One call per view. The body of this loop is now ApplyPropertiesToView, which the Find
    ' in Project excerpt views also call -- styles are per-view, so a view that never gets
    ' them renders with Scintilla's defaults.
    for i as long = lbound(pSci) to ubound(pSci)
        this.ApplyPropertiesToView( pSci(i) )
    next
    
    function = 0
end function


''
''
function clsDocument.GetWord( byval curPos as long = -1 ) as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Get word at the specified location or under the cursor
    dim as long x, y, p
    dim as string buffer 

    ' Retrieve the current position
    if curPos = -1 then curPos = SciMsg( pSci, SCI_GETCURRENTPOS, 0, 0)
    ' Retrieve the starting and ending position of the word
    x = SciMsg( pSci, SCI_WORDSTARTPOSITION, curPos, true)
    y = SciMsg( pSci, SCI_WORDENDPOSITION, curPos, false)
    if y > x then
        ' Text range
        buffer = this.GetTextRange(x, y)
        ' Remove the $NUL
        p = instr(buffer, chr(0))
        if p then buffer = left(buffer, p - 1)
    end if
    buffer = AfxStrRemoveAny( buffer, chr(13, 10, 34) & "()%," )
    function = buffer

end function

''
''
function clsDocument.GetMarkers( byval MarkerType as long ) as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Retrieve all marker positions in the document and return it
    ' as a comma delimited string to be saved to project file.
    dim as string buffer
    dim as long fMark  ' 32 bit value
    dim as long nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0) 

    for i as long = 0 to nLines - 1
        fMark = SciMsg( pSci, SCI_MARKERGET, i, 0)
        if bit(fMark, MarkerType) then
            buffer = buffer & i & ","
        end if
    next

    function = rtrim(buffer, ",")

end function

''
''
function clsDocument.GetBookmarks() as string
    return this.GetMarkers( MARKER_BOOKMARK )
end function

''
''
function clsDocument.GetBreakPoints() as string
    return this.GetMarkers( MARKER_BREAKPOINT )
end function

''
''
function clsDocument.SetMarkers( byval sMarkers as string, byval MarkerType as long ) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Set all marker positions in the document and based on the 
    ' incoming comma delimited string retrieved from a project file.
    sMarkers = trim(sMarkers)
    if len(sMarkers) = 0 then exit function
    
    dim as long nCount = AfxStrParseCount(sMarkers, ",")
    dim as long nLine
    
    for i as long = 1 to nCount
        nLine = val( AfxStrParse(sMarkers, i, ",") )
        SciMsg( pSci, SCI_MARKERADD, nLine, MarkerType )
    next

    function = 0

end function

''
''
function clsDocument.SetBookmarks( byval sBookmarks as string ) as long
    return this.SetMarkers( sBookmarks, MARKER_BOOKMARK )
end function

''
''
function clsDocument.SetBreakPoints( byval sBreakPoints as string ) as long
    return this.SetMarkers( sBreakPoints, MARKER_BREAKPOINT )
end function

''
''
function clsDocument.GetFoldPoints() as string
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Retrieve all folding positions in the document and return it
    ' as a comma delimited string to be saved to project file.
    dim as string buffer
    dim as long nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0) 

    for i as long = 0 to nLines - 1
        dim nFoldLevel as long = SciMsg( pSci, SCI_GETFOLDLEVEL, i, 0)
        if (nFoldLevel and SC_FOLDLEVELHEADERFLAG) then
            if SciMsg( pSci, SCI_GETFOLDEXPANDED, i, 0) = 0 then
            buffer = buffer & i & ","
            end if   
        end if
    next

    function = rtrim(buffer, ",")

end function

''
''
function clsDocument.SetFoldPoints( byval sFoldPoints as string ) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Set all folding positions in the document and based on the 
    ' incoming comma delimited string retrieved from a project file.
    sFoldPoints = trim(sFoldPoints)
    if len(sFoldPoints) = 0 then exit function
    
    dim as long nCount = AfxStrParseCount(sFoldPoints, ",")
    dim as long nLine
    
    for i as long = 1 to nCount
        nLine = val( AfxStrParse(sFoldPoints, i, ",") )
        SciMsg( pSci, SCI_FOLDLINE, nLine, SC_FOLDACTION_CONTRACT) 
    next

    function = 0

end function

''
''
function clsDocument.LineDuplicate() as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' Duplicate the current caret line, or an entire block of
    ' code should a selection be active.
    dim as long startSelPos = SciMsg( pSci, SCI_GETSELECTIONSTART, 0, 0)
    dim as long endSelPos   = SciMsg( pSci, SCI_GETSELECTIONEND, 0, 0)
    if startSelPos = endSelPos then   ' no selection
        ' Simply duplicate the line that the caret is on.
        SciMsg( pSci, SCI_LINEDUPLICATE, 0, 0)
    else
        SciMsg( pSci, SCI_SELECTIONDUPLICATE, 0, 0)
    end if
    function = 0
end function


''
''
function clsDocument.LinesPerPage( byval idxWindow as long ) as long
    dim as any ptr pSci = this.GetActiveScintillaPtr()
    ' SCI_LINESONSCREEN is not a reliable count of lines per screen because
    ' it does not take into account any extra ascent or descent value.
    dim as long nLineHeight = SciMsg( pSci, SCI_TEXTHEIGHT, 0, 0 )
    dim as RECT rc = AfxGetWindowRect(this.hWindow(idxWindow))
    MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(POINT ptr, @rc), 2 )
    dim as long nHeight = rc.bottom - rc.top
    function = (nHeight / nLineHeight)
end function


''
''
function clsDocument.CompileDirectives( Directives() as COMPILE_DIRECTIVES ) as long
    ' Search the source code for any user embedded compiler directives.
    dim ub       as long    
    dim i        as long    
    dim nLines   as long    
    dim st       as string
    dim st_ucase as string
    
    dim sText as string  ' this will be an UTF-8 encoded string
    
    dim as any ptr pSci = this.GetActiveScintillaPtr()

    if pSci = 0 then exit function

    nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0)

    for i = 0 to nLines - 1
        st = ltrim(this.GetLine(i))
            
        if left(st, 1) <> "'" then continue for  ' must start with a comment character
        st = ltrim(mid(st, 2))

        if len(st) < 10 then continue for
        st_ucase = ucase(st)
        
        ub = ubound(Directives)
        
        ' '#CONSOLE ON|OFF
        if left(st_ucase, 11) = "#CONSOLE ON" then
            redim preserve Directives(ub+1)
            Directives(ub+1).DirectiveFlag = IDM_CONSOLE
        elseif left(st_ucase, 12) = "#CONSOLE OFF" then   
            redim preserve Directives(ub+1)
            Directives(ub+1).DirectiveFlag = IDM_GUI
        end if

        ' '#RESOURCE "filename.rc"
        if left(st_ucase, 10) = "#RESOURCE " then
            redim preserve Directives(ub+1)
            Directives(ub+1).DirectiveFlag = IDM_RESOURCE
            st = mid(st, 11)
            Directives(ub+1).DirectiveText = AfxStrExtract(st, chr(34), chr(34))
        end if
        
        ' '#LINKMODULES "module1.bas" "module2.bas"
        if left(st_ucase, 12) = "#LINKMODULES" then
            redim preserve Directives(ub+1)
            Directives(ub+1).DirectiveFlag = IDM_LINKMODULES
            Directives(ub+1).DirectiveText = mid(st, 13)
        end if
            
    next

    function = 0
end function


''
''
function clsDocument.GetIncludeDocumentCount() as long
    dim pDoc as clsDocument ptr = this.pDocIncludeList
    dim nCount as long = 0
    do until pDoc = 0
        nCount = nCount + 1
        pDoc = pDoc->pDocNext
    loop
    function = nCount
end function


''
''
function clsDocument.AddNewIncludeDocument() as clsDocument ptr 
    ' Add it to the start of the linked list
    dim pDoc as clsDocument ptr = new clsDocument
    pDoc->pDocNext = this.pDocIncludeList
    this.pDocIncludeList = pDoc
    function = pDoc
end function


''
''
function clsDocument.RemoveAllIncludeDocuments() as long
    ' Remove from pDocIncludeList
    dim pDoc as clsDocument ptr = this.pDocIncludeList
    dim pDocNext as clsDocument ptr = this.pDocIncludeList

    do until pDoc = 0
        pDocNext = pDoc->pDocNext
        ' Release memory associated with this pDoc
        delete pDoc
        pDoc = pDocNext
    loop
    this.pDocIncludeList = 0
    
    function = 0
end function


''
''
function clsDocument.AddIncludeFromParse( byval sLine as string ) as boolean
    dim as string sFilename

    if left(ucase(sLine), 9) = "#INCLUDE " then sFilename = mid(sLine, 10)
    if left(ucase(sLine), 14) = "#INCLUDE ONCE " then sFilename = mid(sLine, 15)

    if len(sFilename) = 0 then return false
    
    sFilename = CompleteIncludeFilename( @this, sFilename )
    if AfxFileExists(sFilename) = false then return false
    
    ' At this point, the filename exists so save it to our list of include files
    ' for this document that will have its filetime check durign compile to 
    ' determine if the .bas module file should be recompiled.
    dim as clsDocument ptr pDoc = this.AddNewIncludeDocument()
    if pDoc then
        pDoc->DiskFilename = sFilename
        pDoc->DateFileTime = AfxGetFileLastWriteTime(pDoc->DiskFilename)
    end if

    function = 0
end function
