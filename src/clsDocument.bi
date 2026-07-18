'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General public License for more details.

#pragma once

#include once "frmDebug.bi"

'  Scintilla Control identifiers
#define IDC_SCINTILLA              100

' File encodings
#define FILE_ENCODING_ANSI         0
#define FILE_ENCODING_UTF8         1
#define FILE_ENCODING_UTF8_BOM     2
#define FILE_ENCODING_UTF16_BOM    3

#define FILETYPE_UNDEFINED        "0"
#define FILETYPE_MAIN             "1"
#define FILETYPE_MODULE           "2"
#define FILETYPE_NORMAL           "3"
#define FILETYPE_RESOURCE         "4"
#define FILETYPE_HEADER           "5"

' define marker numbers (0-31 available)
#define MARKER_BOOKMARK            1
#define MARKER_BREAKPOINT          2
#define MARKER_OCCURRENCES         3
#define MARKER_DEBUGGER_CURLINE    4


' Structure that holds all of the user embedded compiler directives
' in the source code. Currently, only the main source file is searched
' for the '#CONSOLE ON|OFF directive but others can be added as needed.
type COMPILE_DIRECTIVES
    DirectiveFlag as long              ' IDM_GUI, IDM_CONSOLE, IDM_RESOURCE
    DirectiveText as DWSTRING             ' resource filename, link modules
end type

' Forward references
type clsDocument_ as clsDocument

' Enum used to distinguish bewteen a sub/function and Property Get/Set
enum ClassProperty
    None   = 0   ' sub/function
    Getter = 1
    Setter = 2
    ctor   = 3   ' constructor
    dtor   = 4   ' destructor
end enum

' type that holds data for all project files as it they are loaded from
' the project file.
type PROJECT_FILELOAD_DATA
    bIsValidData    as boolean      ' True set on projectload. Checked in AssignTextBuffer.
    bLoadInTab      as boolean
    wszFilename     as DWSTRING        ' full path and filename
    wszFiletype     as DWSTRING        ' pDoc->ProjectFileType
    wszBookmarks    as DWSTRING        ' pDoc->GetBookmarks()
    wszBreakPoints  as DWSTRING        ' pDoc->GetBreakPoints()
    wszFoldPoints   as DWSTRING        ' pDoc->GetFoldPoints()
    nFirstLine      as long         ' first line of main view 
    nPosition       as long         ' current position of main view
    nFirstLine1     as long         ' first line of second view 
    nPosition1      as long         ' current position of second view
    nSplitMode      as long         ' pDoc->SplitMode
    nSplitPositionX as long         ' pDoc->SplitX
    nSplitPositionY as long         ' pDoc->SplitY
    nFocusEdit      as long         ' View 0 or View 1
end type

type SELECTION_INFO
    isInitialized   as boolean = false
    startline       as long
    endline         as long
    startpos        as long
    endpos          as long
end type

enum SPLIT_MODE
    SplitNone       = 0
    SplitLeftRight  = 1
    SplitTopBottom  = 2
end enum


type clsDocument
    private:
    ' 2 Scintilla direct pointers to accommodate split editing
    m_hWndActiveScintilla as HWND
    m_UserModified        as boolean               ' user manually changes state outside of Scintilla changes
        
    public:
    pDocNext              as clsDocument_ ptr = 0  ' pointer to next document in linked list 
    pDocIncludeList       as clsDocument_ ptr = 0  ' Single linked list of #include files
    IsNewFlag             as boolean
    TextBuffer            as string
    
    docData               as PROJECT_FILELOAD_DATA    ' loaded from project files
    
    ' 2 Scintilla controls to accommodate split editing
    ' hWindow(0) is our MAIN control (main bottom)
    ' hWindow(1) is our split control (top or left)
    hWindow(1)            as HWND   ' Scintilla split edit windows 
    pSci(1)               as any ptr      
    
    ' Code document related
    ProjectFiletype       as DWSTRING = FILETYPE_UNDEFINED
    DiskFilename          as wstring * MAX_PATH
    DateFileTime          as FILETIME  
    bHasFunctions         as boolean = false    ' FunctionList to determine if click will display the File
    FileEncoding          as long = FILE_ENCODING_UTF8   ' new files always UTF8 encoded by default (no BOM)       
    bNeedsParsing         as boolean            ' Document requires to be parsed due to changes.
    DocumentBuild         as string             ' specific build configuration to use for this document
    wszMatchWord          as DWSTRING           ' for the incremental autocomplete search
    AutoCompletetype      as long               ' AUTOC_DIMAS, AUTOC_TYPE
    AutoCStartPos         as long
    AutoCTriggerStartPos  as long
    lastCaretPos          as long               ' used for checking in SCN_UPDATEUI
    lastXOffsetPos        as long               ' used for checking in SCN_UPDATEUI (horizontal offset)
    LastCharTyped         as long               ' used to test for BACKSPACE resetting the autocomplete popup.
    wszEOL                as DWSTRING              ' used in replace in files when constructing each line of new file
    CurrentSelection      as SELECTION_INFO     ' set during scintilla wm_notfy and used in Find/Replace dialog
    
    ' Following used for split edit views
    rcSplitButton         as RECT               ' Split gripper vertical for Scintilla window (client coordinates)
    SplitY                as long               ' Y coordinate of top/bottom splitter
    SplitX                as long               ' X coordinate of left/right splitter
    EditorSplitMode       as SPLIT_MODE = SplitNone
    bSizing               as boolean
    ptPrev                as POINT
    rcDocSplitArea        as rect               ' entire rc client area for one or both edit windows + scrollbars
    ptClick               as POINT              ' Used for differentiating between single/double clicks
    idTimer               as long = 100         ' Used for differentiating between single/double clicks
    doubleClickReceived   as boolean = false    ' Used for differentiating between single/double clicks
    
    static NextFileNum as long
    
    declare property hWndActiveScintilla() as hwnd
    declare property hWndActiveScintilla(byval hWindow as hwnd)
    
    declare property UserModified() as boolean
    declare property UserModified( byval nModified as boolean )
    
    declare function SetDocInfo() as long
    declare function SetProjectFileType() as long
    declare function SetDocumentBuild() as long
    declare function ParseDocument() as boolean
    declare function IsValidScintillaID( byval idScintilla as long ) as boolean
    declare function GetActiveScintillaPtr() as any ptr
    declare function AssignTextBuffer() as long
    declare function LoadDiskFile( byref wszFile as wstring, byval bForceParse as boolean = true, byval bIsTemplate as boolean = false ) as long
    declare function DestroyScintillaWindows() as long
    declare function CreateScintillaWindows() as long
    declare function CreateCodeWindow( byval hWndParent as HWnd, byval IsNewFile as boolean, byval IsTemplate as boolean = False, byref wszFileName as wstring = "") as HWnd
    declare function FindReplace( byval strFindText as string, byval strReplaceText as string ) as long
    declare function InsertFile() as boolean
    declare function SaveFile(byval bSaveAs as boolean = false ) as boolean
    declare function SaveTempFile( byval wszFilename as DWSTRING ) as boolean
    declare function ApplyProperties() as long
    declare function GetTextRange( byval cpMin as long, byval cpMax as long) as string
    declare function ChangeSelectionCase( byval fCase as long) as long 
    declare function GetCurrentLineNumber() as long
    declare function SelectLine( byval nLineNum as long ) as long
    declare function GetLine( byval nLine as long) as string
    declare function IsFunctionLine( byval lineNum as long ) as long
    declare function GotoNextFunction() as long
    declare function GotoPrevFunction() as long
    declare function GetLineCount() as long
    declare function GetSelText() as string
    declare function GetText() as string
    declare function SetText( byref sText as const string ) as long 
    declare function SetLine( byval nLineNum as long, byval sNewText as string) as long
    declare function AppendText( byref sText as const string ) as long 
    declare function CenterCurrentLine() as long 
    declare function GetSelectedLineRange( byref startLine as long, byref endLine as long, byref startPos as long, byref endPos as long ) as long 
    declare function BlockComment( byval flagBlock as boolean ) as long
    declare function CurrentLineUp() as long
    declare function CurrentLineDown() as long
    declare function MoveCurrentLines( byval flagMoveDown as boolean ) as long
    declare function NewLineBelowCurrent() as long
    declare function ToggleMarker( byval nLine as long, byval MarkerType as long ) as long
    declare function ToggleBookmark( byval nLine as long ) as long
    declare function NextBookmark() as long 
    declare function PrevBookmark() as long 
    declare function GetMarkers( byval MarkerType as long ) as string
    declare function SetMarkers( byval sMarkers as string, byval MarkerType as long ) as long
    declare function GetBookmarks() as string
    declare function SetBookmarks( byval sBookmarks as string ) as long
    declare function GetBreakPoints() as string
    declare function SetBreakPoints( byval sBreakPoints as string ) as long
    declare function ToggleBreakPoint( byval nLine as long ) as long
    declare function FoldToggle( byval nLine as long ) as long
    declare function FoldAll() as long
    declare function UnFoldAll() as long
    declare function FoldToggleOnwards( byval nLine as long) as long
    declare function ConvertEOL( byval nMode as long) as long
    declare function TabsToSpaces() as long
    declare function GetWord( byval curPos as long = -1 ) as string
    declare function GetFoldPoints() as string
    declare function SetFoldPoints( byval sFoldPoints as string ) as long
    declare function GetCurrentFunctionName( byref sFunctionName as string, byref nGetSet as ClassProperty ) as long
    declare function LineDuplicate() as long
    declare function SetMarkerHighlight() as long
    declare function RemoveMarkerHighlight() as long
    declare function IsMultiLineSelection() as boolean
    declare function RemoveSelectionAttributes() as long
    declare function HasMarkerHighlight() as boolean
    declare function FirstMarkerHighlight() as long
    declare function LastMarkerHighlight() as long
    declare function LinesPerPage( byval idxWindow as long ) as long
    declare function CompileDirectives( Directives() as COMPILE_DIRECTIVES) as long
    declare function GetIncludeDocumentCount() as long
    declare function AddNewIncludeDocument() as clsDocument ptr 
    declare function RemoveAllIncludeDocuments() as long
    declare function AddIncludeFromParse( byval sLine as string ) as boolean

    declare destructor
end type
dim clsDocument.NextFileNum as long = 0
