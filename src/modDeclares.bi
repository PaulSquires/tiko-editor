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

#define IDC_MENUBAR_FILE      1000
#define IDC_MENUBAR_EDIT      1001
#define IDC_MENUBAR_SEARCH    1002
#define IDC_MENUBAR_VIEW      1003
#define IDC_MENUBAR_PROJECT   1004
#define IDC_MENUBAR_COMPILE   1005
#define IDC_MENUBAR_DEBUG     1006  
#define IDC_MENUBAR_HELP      1007
#define IDC_MENUBAR_UPDATE    1008

''  Menu message identifiers
enum
    '' USER MESSAGES
    MSG_USER_SETFOCUS = WM_USER + 1     ' 1024 + 1
    MSG_USER_PROCESS_COMMANDLINE 
    MSG_USER_PROCESS_STARTUPUSERTOOLS
    MSG_USER_PROCESS_CHECKFORUPDATE
    MSG_USER_SHOWAUTOCOMPLETE      ' wParam = AUTOCOMPLETE_NOTIFY_*
    MSG_USER_LOAD_EXPLORERFILES
    MSG_USER_LOAD_FUNCTIONLISTFILES
    MSG_USER_LOAD_BOOKMARKSFILES
    MSG_USER_LOAD_FUNCTIONSFILES
    MSG_USER_SHOW_KEYBOARDEDIT
    ' Companion to MSG_USER_SHOW_KEYBOARDEDIT, for the User Tools dialog's Assign Shortcut
    ' editor. Both exist so the sub-dialog can be opened from a posted message rather than
    ' only from a button click -- see TIKO_USERTOOLS_AUTOASSIGN.
    MSG_USER_SHOW_TOOLKEY
    MSG_USER_RICHEDIT_SELECTALL
    MSG_USER_PARSE_COMPLETE        ' wParam = SCAN_TIER; posted by the fbcParser scan worker
    
    '' FILE
    IDM_FILE_START
    IDM_FILENEW, IDM_FILEOPEN, IDM_FILEOPENASTEMPLATE
    IDM_FILEOPEN_EXPLORERLISTBOX
    IDM_FILECLOSE, IDM_FILECLOSE_EXPLORERLISTBOX
    IDM_FILECLOSEALL, IDM_FILECLOSEALLOTHERS, IDM_CLOSEALLFORWARD, IDM_CLOSEALLBACKWARD
    IDM_FILESAVE, IDM_FILESAVE_EXPLORERLISTBOX
    IDM_FILESAVEAS, IDM_FILESAVEAS_EXPLORERLISTBOX 
    IDM_FILERENAME, IDM_FILERENAME_EXPLORERLISTBOX
    IDM_FILESAVEALL
    IDM_FILEDUPLICATE, IDM_FILEDUPLICATE_EXPLORERLISTBOX
    IDM_LOADSESSION, IDM_SAVESESSION, IDM_CLOSESESSION
    IDM_MRU, IDM_MRUFILES, IDM_MRUSESSION, IDM_MRUSESSIONFILES
    IDM_OPENINCLUDE, IDM_KEYBOARDSHORTCUTS 
    IDM_OPTIONS, IDM_OPTIONSDIALOG, IDM_BUILDCONFIG, IDM_BUILDCONFIG_POPUP, IDM_THEMES
    IDM_USERTOOLS, IDM_USERTOOLSDIALOG
    IDM_EXIT
    IDM_FILE_END
    
    '' EDIT 
    IDM_EDIT_START
    IDM_UNDO, IDM_REDO
    IDM_CUT, IDM_COPY, IDM_PASTE, IDM_INSERTFILE
    IDM_FILEENCODING, IDM_ANSI, IDM_UTF8, IDM_UTF8BOM, IDM_UTF16BOM
    IDM_DELETELINE, IDM_DELETE, 
    IDM_INDENTBLOCK, IDM_UNINDENTBLOCK, IDM_COMMENTBLOCK, IDM_UNCOMMENTBLOCK
    IDM_DUPLICATELINE, IDM_MOVELINEUP, IDM_MOVELINEDOWN, IDM_NEWLINEBELOWCURRENT
    IDM_TOUPPERCASE, IDM_TOLOWERCASE, IDM_TOMIXEDCASE
    IDM_LINEENDINGS, IDM_EOLTOCRLF, IDM_EOLTOLF
    IDM_SELECTLINE, IDM_TABSTOSPACES
    IDM_SPACES, IDM_SELECTALL
    IDM_EDIT_END
    
    '' SEARCH
    IDM_SEARCH_START
    IDM_SEARCHSYMBOL
    IDM_FIND, IDM_FINDNEXT, IDM_FINDPREV
    IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
    IDM_FINDNEXTACCEL, IDM_FINDPREVACCEL 
    IDM_FINDINPROJECT, IDM_REPLACE, IDM_TOGGLEREPLACE
    IDM_GOTODEFINITION, IDM_LASTPOSITION
    IDM_CLOSETAB, IDM_GOTONEXTFUNCTION, IDM_GOTOPREVFUNCTION
    IDM_GOTONEXTTAB, IDM_GOTOPREVTAB, IDM_EDITORSPLIT, IDM_TABSLIST
    IDM_GOTOHEADERFILE, IDM_GOTOSOURCEFILE, IDM_GOTOMAINFILE, IDM_GOTORESOURCEFILE
    IDM_BOOKMARKTOGGLE, IDM_BOOKMARKNEXT, IDM_BOOKMARKPREV, IDM_BOOKMARKCLEARALL
    IDM_BOOKMARKCLEARALLDOCS, IDM_CLEARALLBOOKMARKNODE, IDM_REMOVEBOOKMARKNODE
    IDM_GOTONEXTCOMPILEERROR, IDM_GOTOPREVCOMPILEERROR
    IDM_SETFOCUSEDITOR, IDM_GOTOLINE
    IDM_SEARCH_END
    
    '' VIEW
    IDM_VIEW_START
    IDM_VIEWSIDEPANEL, IDM_VIEWEXPLORER, IDM_VIEWOUTPUT, IDM_FUNCTIONLIST
    IDM_BOOKMARKSLIST, IDM_SPLITLEFTRIGHT, IDM_SPLITTOPBOTTOM
    IDM_ZOOMIN, IDM_ZOOMOUT, IDM_ZOOMRESET
    IDM_FOLDTOGGLE, IDM_FOLDBELOW, IDM_FOLDALL, IDM_UNFOLDALL
    IDM_VIEWTODO, IDM_VIEWNOTES, IDM_RESTOREMAIN, IDM_EXPLORERPOSITION
    IDM_VIEW_END
    
    '' PROJECT
    IDM_PROJECT_START
    IDM_PROJECTNEW, IDM_PROJECTMANAGER, IDM_PROJECTOPEN, IDM_MRUPROJECT, IDM_MRUPROJECTFILES
    IDM_PROJECTCLOSE, IDM_PROJECTSAVE, IDM_PROJECTSAVEAS, IDM_PROJECTFILESADD, IDM_PROJECTOPTIONS  
    IDM_PROJECTFILETYPE, IDM_REMOVEFILEFROMPROJECT, IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX
    IDM_PROJECT_END
        
    '' COMPILE
    IDM_COMPILE_START
    IDM_BUILDEXECUTE, IDM_COMPILE, IDM_REBUILDALL, IDM_RUNEXE, IDM_QUICKRUN, IDM_COMPILEMODULE
    IDM_COMMANDLINE
    IDM_COMPILE_END
    
    '' DEBUG
    IDM_DEBUG_START
    IDM_DEBUG_STARTDEBUGGING
    IDM_DEBUG_STOPDEBUGGING
    IDM_DEBUG_STEPINTO
    IDM_DEBUG_STEPOVER
    IDM_DEBUG_STEPOUT
    IDM_DEBUG_RUNTOCURSOR
    IDM_DEBUG_TOGGLEBREAKPOINT
    IDM_DEBUG_DELETEALLBREAKPOINTS
    IDM_DEBUG_END
    
    
    '' HELP
    IDM_HELP_START
    IDM_HELP_FB, IDM_HELP_TIKO, IDM_ABOUT
    IDM_HELP_END
    
    '' OTHER
    IDM_SETFILEMAIN
    IDM_SETFILERESOURCE
    IDM_SETFILEHEADER
    IDM_SETFILEMODULE
    IDM_SETFILENORMAL
    IDM_SETFILEMAIN_EXPLORERTREEVIEW
    IDM_SETFILERESOURCE_EXPLORERTREEVIEW
    IDM_SETFILEHEADER_EXPLORERTREEVIEW
    IDM_SETFILEMODULE_EXPLORERTREEVIEW
    IDM_SETFILENORMAL_EXPLORERTREEVIEW
    IDM_EXPLORER_EXPANDALL 
    IDM_EXPLORER_COLLAPSEALL 
    IDM_FUNCTIONS_EXPANDALL 
    IDM_FUNCTIONS_COLLAPSEALL 
    IDM_BOOKMARKS_EXPANDALL 
    IDM_BOOKMARKS_COLLAPSEALL 
    IDM_FUNCTIONS_VIEWASTREE
    IDM_FUNCTIONS_VIEWASLIST
    IDM_SETCATEGORY
    IDM_CLOSEPANEL
    IDM_COPYDATA_COMMANDLINE     ' IPC message sending commandline between instances of the editor
        
    IDM_MRUCLEAR, IDM_MRUSESSIONCLEAR, IDM_MRUPROJECTCLEAR
    IDM_CONSOLE, IDM_GUI, IDM_RESOURCE, IDM_LINKMODULES   ' used for compiler directives in code
    IDM_32BIT, IDM_64BIT   ' mainly used for identifying compiler associated with a project
end enum

#define IDM_USERTOOLSLIST    4000
#define IDM_USERTOOLSBASE    4001
#define IDM_MRUBASE          5000  ' Windows id of MRU items 1 to 10 (located under File menu)
#define IDM_MRUPROJECTBASE   6000  ' Windows id of MRUPROJECT items 1 to 10 (located under Project menu)
#define IDM_MRUSESSIONBASE   7000  ' Windows id of MRUSESSION items 1 to 10 (located under File menu)
#define IDM_BUILDCONFIGBASE  8000
' The AfxIFileSaveDialog selector for cloning a theme -- picks the *.theme filter and the
' settings\themes\ folder in modRoutines. Defined HERE (loaded before modRoutines.inc) rather
' than in frmThemes.bi (loaded after it), which the save dialog cannot see.
#define IDC_THEMES_SAVEAS    9650


'  Global window handles
dim shared as HWND HWND_FRMMAIN, HWND_FRMRECENT, HWND_FRMMAIN_STATUSBAR
dim shared as HWND HWND_FRMOUTPUT, HWND_FRMOUTPUT_LVRESULTS, HWND_FRMOUTPUT_LVSEARCH
dim shared as HWND HWND_FRMOUTPUT_LVTODO, HWND_FRMOUTPUT_VSCROLL
' The tab strip: a PsSelectBar (the five tabs) and a one-item PsIconPanel (the "X"), sized
' side by side. HWND_FRMOUTPUT_SELECTBAR also HOLDS the current tab -- see frmOutput.bi.
dim shared as HWND HWND_FRMOUTPUT_SELECTBAR, HWND_FRMOUTPUT_CLOSE
dim shared as HWND HWND_FRMMAIN_MENUBAR
' Editor split bars: one per orientation, created once and shown one at a time to follow
' whichever split mode the ACTIVE document is in (PsSplitter's orientation is fixed at
' creation, and only one document is active).
dim shared as HWND HWND_FRMMAIN_SPLITV, HWND_FRMMAIN_SPLITH
' Output panel splitter. Also a child of frmMain, not of frmOutput: it sits BETWEEN the
' editor area and the panel, and frmMain owns that layout.
dim shared as HWND HWND_FRMMAIN_SPLITOUTPUT
' Explorer panel splitter. Sits on the panel's inner edge -- which side that is depends on
' gConfig.ExplorerPositionRight, but it is a vertical bar either way, so one control serves
' both dock sides.
dim shared as HWND HWND_FRMMAIN_SPLITPANEL
dim shared as HWND HWND_FRMHELPVIEWER, HWND_FRMHELPVIEWER_LEFTPANEL, HWND_FRMHELPVIEWER_RIGHTPANEL
dim shared as HWND HWND_FRMHELPVIEWER_VSCROLLBAR2
dim shared as HWND HWND_FRMHELPVIEWER_SPLITTER
' The General Options and Code Editor pages are table-driven now (modOptionsRows.inc) and
' have no child forms of their own, so HWND_FRMOPTIONSGENERAL / ...EDITOR / ...EDITOR2 are
' gone. "Advanced Code Editor" is no longer a separate page at all: its settings sit at the
' bottom of the one scrolling Code Editor page.
dim shared as HWND HWND_FRMOPTIONS
' The entire Environment Options dialog is panel-hosted / table-driven now: every page's
' controls live on the scroll panel, so none of the old HWND_FRMOPTIONS* child-form globals
' remain.
dim shared as HWND HWND_FRMFINDINPROJECT, HWND_FRMSEARCHSYMBOL
dim shared as HWND HWND_FRMBUILDCONFIG, HWND_FRMUSERTOOLS, HWND_FRMABOUT
dim shared as HWND HWND_FRMABOUT_TABS, HWND_FRMABOUT_CREDITS, HWND_FRMABOUT_LICENSE
dim shared as HWND HWND_FRMKEYBOARD, HWND_FRMKEYBOARDEDIT
dim shared as HWND HWND_FRMDEBUG, HWND_FRMDEBUG_OUTPUT

dim shared as HWND HWND_FRMMAIN_TOPTABS, HWND_FRMMAIN_TOPTABSMENU
dim shared as HWND HWND_FRMMAIN_TOPTABSINFO, HWND_FRMMAIN_FIND, HWND_FRMMAIN_REPLACE
dim shared as HWND HWND_FRMEXPLORER
dim shared as HWND HWND_FRMFUNCTIONS
dim shared as HWND HWND_FRMBOOKMARKS
dim shared as HWND HWND_FRMPANEL, HWND_FRMPANEL_MENU, HWND_FRMPANEL_VSCROLLBAR
dim shared as HWND HWND_FRMAUTOCOMPLETE
dim shared as HWND HWND_FRMEDITOR_HSCROLLBAR(1)
dim shared as HWND HWND_FRMEDITOR_VSCROLLBAR(1)

dim shared as long ghIconGood, ghIconBad
dim shared as HCURSOR ghCursorSizeNS
dim shared as HCURSOR ghCursorSizeWE

' Create a dynamic array that will hold all localization words/phrases while
' a language is being edited in frmOptionsLocal. Also create a global array
' that holds the english phrases. When a localization is loaded, any missing
' translations are replaced with the english version.
redim shared gLangEnglish(any) as wstring * MAX_PATH
redim shared gLocalPhrases(any) as wstring * MAX_PATH
dim shared gLocalPhrasesEdit as boolean   ' a localization language is being edited. 

' Create a dynamic array that will hold all localization words/phrases. This
' array is resized and loaded using the LoadLocalizationFile function.
redim shared LL(any) as wstring * MAX_PATH

' Define a macro that allows the user to specify the LL array subscript and
' also a descriptive label (that is ignored), and return the LL array value.
#Define L(e,s) LL(e)

#Define SetFocusScintilla  PostMessage( HWND_FRMMAIN, MSG_USER_SETFOCUS, 0, 0 )
#Define SciExec(h, m, w, l) SendMessage(h, m, w, CAST(LPARAM, l))

#DEFINE GUIFONT      wstr("Segoe UI")
#DEFINE GUIFIXEDFONT wstr("Consolas")
#DEFINE SYMBOLFONT   wstr("Segoe Fluent Icons")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONTBOLD_10   2
#DEFINE ITALICFONT_10    3
#DEFINE INFOFONT_11      4
#DEFINE SYMBOLFONT_9     5
#DEFINE SYMBOLFONT_10    6
#DEFINE SYMBOLFONT_12    7
#DEFINE SYMBOLFONT_20    8
' Larger GUI faces, added for the Environment Options dialog (whose base font is 11pt).
' MAXFONTS is both the array bound and the bound of the DeleteObject loop in frmMain, so a
' new entry here is freed without touching the cleanup.
#DEFINE GUIFONT_11       9
#DEFINE GUIFONT_12       10
#DEFINE GUIFONTBOLD_11   11
' The About box's application name, and the only face in this table above 12pt. Nothing else
' in tiko needs it; it exists so that one line can carry the dialog.
#DEFINE GUIFONT_26       12
' The "tk" wordmark on the About box's icon plate. A FIXED face, deliberately: the mark is
' two lowercase letters standing in for an app icon, and Consolas gives them the even weight
' and the flat terminals that read as a logotype. Segoe UI at the same size reads as a word.
#DEFINE FIXEDFONT_26     13
#DEFINE MAXFONTS         14

dim shared ghFont(MAXFONTS) as HFONT


''
''  Save information related to Find/Replace and Find in Files operations
''
type FINDREPLACE_TYPE
    bFirstTimeInvoked   as boolean = true
    hCueBannerFont      as HFONT
    foundCount          as long 
    txtFind             as DWSTRING
    txtReplace          as DWSTRING
    txtFindCombo(10)    as DWSTRING
    txtReplaceCombo(10) as DWSTRING
    txtFilesCombo(10)   as DWSTRING
    txtFolderCombo(10)  as DWSTRING
    txtLastFind         as DWSTRING
    txtFiles            as DWSTRING         ' *.*, *.bas, etc (FindInFolder)
    txtFolder           as DWSTRING         ' start search from this folder (FindInFolder)
    nSearchSubFolders   as long          ' search sub folders as well (FindInFolder)
    nWholeWord          as long          ' find/replace whole word search
    nMatchCase          as long          ' match case when searching
    nSelection          as long          ' search only selected text
    nPreserve           as long          ' search only selected text
    nSearchCurrentDoc   as long
    nSearchAllOpenDocs  as long
    nSearchProject      as long
    wszResults          as DWSTRING = "0/0"
    bShowInfoPanel      as boolean = true
    bShowFindPanel      as boolean = false
    bShowReplacePanel   as boolean = false
    bProjectReplaceActive as boolean = false
    ' The Find bar's seven icon rects (Match Case, Whole Word, Toggle Replace, Selection,
    ' Prev, Next, Close) and its divider are three PsIconPanels now (frmFind.inc) -- the
    ' controls own those cells, so there is nothing left for the host to store. rcResults
    ' stays: the results count is still painted by the bar itself.
    ' ...and the Replace bar's three (Preserve Case, Replace, Replace All) are two more
    ' (frmReplace.inc). rcResults is the only icon-strip rect left in this type, because the
    ' results count is still painted by the Find bar itself.
    rcResults           as RECT
end type
dim shared gFind as FINDREPLACE_TYPE
dim shared gFindInFiles as FINDREPLACE_TYPE


'' Last position in document. Used when "Last Position" menu option is selected.
type LASTPOSITION_TYPE
    pDoc       as clsDocument Ptr
    nFirstLine as long     ' first visible line on screen
    nPosition  as long     ' Position in Scintilla document where caret is positioned
end type
dim shared gLastPosition(any) as LASTPOSITION_TYPE


' (The menubar's own item/state globals are gone: the PsMenuBar control owns its items,
'  rects, hover and active state -- see frmMenuBar.inc.)

' array that holds the names of all fonts on the target system
dim shared gFontNames( any ) as DWSTRING


const MENUITEM_HEIGHT = 24
const EXPLORERITEM_HEIGHT = 22
const MENUBAR_HEIGHT = 30
const OUTPUT_TABS_HEIGHT = 40
' Width reserved at the right of the Output tab strip for the close "X" PsIconPanel. The
' glyph cell itself is smaller; the surplus is the breathing room the old hand-drawn rect
' got from its 10px right margin.
const OUTPUT_CLOSE_WIDTH = 40
const PANEL_ICON_HEIGHT = 24

' Debounce timer on HWND_FRMMAIN: restarted on every editor modification;
' when it fires, the active document is handed to the fbcParser scan worker.
const IDT_PARSER_DEBOUNCE = 501
const PARSER_DEBOUNCE_MS  = 500

const TOPTABS_HEIGHT = 36
const TOPTABS_INFO_HEIGHT = 40   
const TOPTABS_FIND_HEIGHT = 40   
const TOPTABS_REPLACE_HEIGHT = 40

const MENUITEM_HEIGHT_COMPACT = 20
const EXPLORERITEM_HEIGHT_COMPACT = 19 
const MENUBAR_HEIGHT_COMPACT = 28 

const STATUSBAR_HEIGHT = 26
const SCROLLBAR_WIDTH_PANEL = 7
const SCROLLBAR_WIDTH_EDITOR = 12
const SCROLLBAR_HEIGHT = 12
const SCROLLBAR_MINTHUMBSIZE = 30
const SPLITSIZE = 4


type TOPMENU_TYPE
    nParentID   as long
    nID         as long
    nChildID    as long
    isDisabled  as boolean
    isSeparator as boolean
    isChecked   as boolean
end type
redim shared gTopMenu(any) as TOPMENU_TYPE

dim shared as wstring * 10 _
    wszIconChevronLeft, wszIconChevronRight, wszIconChevronUp, wszIconChevronDown, _
    wszIconDocument, wszIconUpArrow, wszIconDownArrow, wszIconSelection, wszIconCheckmark, _
    wszIconClose, wszIconDirty, wszIconCompileResult, wszIconMatchCase, wszIconWholeWord, _
    wszIconPreserveCase, wszIconReplace, wszIconReplaceAll, wszIconMoreActions, wszIconAddFileButton, _
    wszIconExplorer, wszIconFunctions, wszIconBookmarks, _
    wszIconCompile, wszIconBuildExecute, wszIconDebug, _
    wszIconSplitEditor, wszIconSplitLeftRight, wszIconSplitTopBottom, wszIconThemes, _
    wszIconSettings, wszIconCheckBoxEmpty, wszIconCheckBoxMarked, _
    wszIconContinue, wszIconStop, wszIconStepNext, wszIconStepOver, wszIconStepOut, wszIconRunToCursor, _
    wszIconSave, wszIconFind, wszIconToggleReplace, _
    wszIconGotoMain, wszIconGotoHeader, wszIconGotoSource, _
    wszSearchFile, wszSearchFunction, wszSearchType, wszSearchEnum, _
    wszAutoCGlyphProc, wszAutoCGlyphType, wszAutoCGlyphEnum, wszAutoCGlyphVar, wszAutoCGlyphKeyword


' Braille spinner patterns - large clockwise rotation
' DWSTRING, not "wstring * 2": the -gen gas64 backend CORRUPTS static wstring array
' initializers (measured 2026-07-27 -- it wrote 0:0 for six of these eight frames, and
' widening the field to "wstring * 3" silenced the warning while leaving the data wrong).
' DWSTRING is a UDT with constructors, so the array is built at runtime instead of being
' emitted as static data, which sidesteps the backend bug entirely. gcc and gas64 now
' produce identical code points. Consumers take the element itself, not its address.
dim shared spinner(0 to 7) as DWSTRING => { _
    wstr(!"\u28F4"), _ ' ⠾
    wstr(!"\u28F2"), _ ' ⠽
    wstr(!"\u28B6"), _ ' ⠻
    wstr(!"\u2837"), _ ' ⠷
    wstr(!"\u282F"), _ ' ⠯
    wstr(!"\u281F"), _ ' ⠟
    wstr(!"\u28D4"), _ ' ⠮
    wstr(!"\u28F0")  _ ' ⠼
}
' Symbol characters display in top menus, frmExplorer, and tab control

'"Segoe Fluent Icons"
    wszIconClose             = !"\uE10A"      ' light X  
    wszIconChevronLeft       = !"\uE012"
    wszIconChevronRight      = !"\uE013" 
    wszIconChevronUp         = !"\uE014"
    wszIconChevronDown       = !"\uE015"
    wszIconDocument          = !"\u00B7"     ' small dot (use the regular Segue UI font for this one)
    wszIconUpArrow           = !"\uF0AD"     ' up arrow
    wszIconDownArrow         = !"\uF0AE"     ' down arrow
    wszIconSelection         = !"\uE14C"     ' selection icon
    wszIconCheckmark         = !"\uE001"     ' checkmark
    wszIconDirty             = !"\u2981"     ' larger dot
    wszIconCompileResult     = !"\uECCC"     ' larger circle  
    wszIconMatchCase         = "Aa"          ' match case
    wszIconWholeWord         = "W"           ' whole word
    wszIconPreserveCase      = "AB"          ' preserve case
    wszIconReplace           = !"\uE14B"     ' replace
    wszIconReplaceAll        = !"\uE299"     ' replace all
    wszIconMoreActions       = !"\uE10C"     ' ...
    wszIconAddFileButton     = !"\uE109"     ' plus sign (thicker)
    wszIconExplorer          = !"\uE8A9"
    wszIconFunctions         = !"\uE8BC"
    wszIconBookmarks         = !"\uE723"
    wszIconCompile           = !"\uE74C"
    wszIconBuildExecute      = !"\uE768"  
    wszIconSplitLeftRight    = !"\uF57C"   
    wszIconSplitTopBottom    = !"\uF16E"   
    wszIconSplitEditor       = wszIconSplitLeftRight
    wszIconSettings          = !"\uE713"   
    wszIconThemes            = !"\uE771"   
    wszIconCheckBoxEmpty     = !"\uE739"
    wszIconCheckBoxMarked    = !"\uE73A"
    wszIconSave              = !"\uE74E"      ' Save (diskette)
    wszIconFind              = !"\uE721"      ' Search (magnifying glass)
    wszIconToggleReplace     = !"\uE8B4"      ' Toggle Replace
    wszIconGotoMain          = "M"
    wszIconGotoHeader        = "H"
    wszIconGotoSource        = "C"

    ' Use GUI Italiac font for these identifiers.
    wszSearchFile            = "d"            ' "disk"
    wszSearchFunction        = "f"
    wszSearchType            = "T"
    wszSearchEnum            = "e"

    ' Debugger Icons
    wszIconDebug             = !"\uE893"     ' next outline
    wszIconContinue          = !"\uF5B0"     ' play solid
    wszIconStop              = !"\uEE95"     ' stop solid
    wszIconStepNext          = !"\uF0AF"     ' right arrow
    wszIconStepOver          = !"\uEE35"     ' reply mirrored
    wszIconStepOut           = wszIconUpArrow
    wszIconRunToCursor       = !"\uE623"     ' next solid

    ' Autocomplete popup kind markers. Plain geometric characters drawn in the regular
    ' GUI font, NOT Segoe Fluent Icons: the popup already switches colour per kind, and
    ' a missing icon-font glyph would render as a tofu box on every row of the list.
    wszAutoCGlyphProc        = !"\u0192"     ' small f with hook - sub/function
    wszAutoCGlyphType        = !"\u25A0"     ' filled square - type/union/typedef
    wszAutoCGlyphEnum        = !"\u25B2"     ' filled triangle - enum/enum value
    wszAutoCGlyphVar         = !"\u25CF"     ' filled circle - var/const/param/field
    wszAutoCGlyphKeyword     = !"\u25C6"     ' filled diamond - keyword/data type
    
    
    
    
