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

'  modMenuIds.bi  --  the menu vocabulary: every command id, the menu-bar ids, and the
'                     top-menu table that describes the menus themselves.
'
'  MOVED OUT OF modDeclares.bi. None of it is Win32 -- a command id is application
'  vocabulary and TOPMENU_TYPE is six longs and a boolean -- but it sat in the shell's
'  grab-bag header, which is why modMenuDefinitions.inc could not be compiled without the
'  shell.
'
'  ---- THE ENUM WAS SPLIT, AND THE SPLIT HAD TO PRESERVE THE NUMBERS ----------------
'
'  The command ids used to share one enum with the MSG_USER_* window messages, which start
'  at WM_USER + 1. THE IDS ARE PERSISTED IN keybindings.ini AS NUMBERS, so moving them into
'  an enum of their own without pinning the start would have renumbered all of them and
'  silently reassigned every shortcut every user has ever set.
'
'  IDM_FILE_START is therefore pinned to 1039, which was MEASURED from the running build
'  before the move rather than reasoned about -- see the note beside it.

#pragma once

' ---- the menu bar's own ids -------------------------------------------------------------
#define IDC_MENUBAR_FILE      1000
#define IDC_MENUBAR_EDIT      1001
#define IDC_MENUBAR_SEARCH    1002
#define IDC_MENUBAR_VIEW      1003
#define IDC_MENUBAR_PROJECT   1004
#define IDC_MENUBAR_COMPILE   1005
#define IDC_MENUBAR_DEBUG     1006  
#define IDC_MENUBAR_HELP      1007
#define IDC_MENUBAR_UPDATE    1008
#define IDM_USERTOOLSLIST    4000
#define IDM_USERTOOLSBASE    4001
#define IDM_MRUBASE          5000  ' Windows id of MRU items 1 to 10 (located under File menu)
#define IDM_MRUPROJECTBASE   6000  ' Windows id of MRUPROJECT items 1 to 10 (located under Project menu)
#define IDM_BUILDCONFIGBASE  8000

' ---- the command ids ---------------------------------------------------------------------
enum
    '' FILE
    '' 1039 IS MEASURED, NOT CHOSEN, and it is why this enum has an explicit
    '' start. These ids are PERSISTED IN keybindings.ini AS NUMBERS, so any shift
    '' silently reassigns every shortcut the user has ever set -- the same hazard
    '' the comments below give for inserting an id in the middle, except that
    '' splitting the enum would have shifted ALL of them.
    ''
    '' The value comes from the enum this was cut out of: it followed 14
    '' MSG_USER_* entries starting at WM_USER + 1. Verified by printing
    '' IDM_FILE_START, IDM_EXIT and IDM_ZOOMRESET from the running build before
    '' the move -- 1039, 1070, 1159 -- and again after.
    ''
    '' A NEW MSG_USER_* MESSAGE WOULD COLLIDE WITH IDM_FILE_START. modDeclares.bi
    '' says so where that enum now ends; there is no compile-time guard because
    '' fbc's preprocessor cannot evaluate an enum constant.
    IDM_FILE_START = 1039
    IDM_FILENEW, IDM_FILEOPEN, IDM_FILEOPENASTEMPLATE
    IDM_FILEOPEN_EXPLORERLISTBOX
    IDM_FILECLOSE, IDM_FILECLOSE_EXPLORERLISTBOX
    IDM_FILECLOSEALL, IDM_FILECLOSEALLOTHERS, IDM_CLOSEALLFORWARD, IDM_CLOSEALLBACKWARD
    IDM_FILESAVE, IDM_FILESAVE_EXPLORERLISTBOX
    IDM_FILESAVEAS, IDM_FILESAVEAS_EXPLORERLISTBOX 
    IDM_FILERENAME, IDM_FILERENAME_EXPLORERLISTBOX
    IDM_FILESAVEALL
    IDM_FILEDUPLICATE, IDM_FILEDUPLICATE_EXPLORERLISTBOX
    IDM_MRU, IDM_MRUFILES
    IDM_OPENINCLUDE, IDM_KEYBOARDSHORTCUTS 
    IDM_SETTINGS, IDM_OPTIONSDIALOG, IDM_BUILDCONFIG, IDM_BUILDCONFIG_POPUP, IDM_THEMES
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
    ' CODE FORMATTER. Appended at the END of the Edit block rather than slotted in beside
    ' the other transforms: these ids are persisted in keybindings.ini, so inserting one in
    ' the middle renumbers every id after it and silently reassigns the user's shortcuts.
    IDM_FORMATSUBMENU
    IDM_FORMATDOCUMENT, IDM_FORMATSELECTION, IDM_FORMATALLDOCS, IDM_FORMATPROJECT
    ' The Format Options dialog. It lives on File > Settings beside IDM_OPTIONSDIALOG, but
    ' its id is declared HERE for the same persisted-id reason as above -- appending is
    ' safe, inserting it next to IDM_OPTIONSDIALOG would shift every id after that one.
    IDM_FORMATOPTIONS
    IDM_EDIT_END
    
    '' SEARCH
    IDM_SEARCH_START
    IDM_SEARCHSYMBOL
    IDM_FIND, IDM_FINDNEXT, IDM_FINDPREV
    IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
    IDM_FINDNEXTACCEL, IDM_FINDPREVACCEL
    ' Placed here DELIBERATELY -- outside both of the blanket re-enable ranges in
    ' frmMain_SearchTopMenuStates (IDM_SEARCHSYMBOL..IDM_FINDPREVACCEL and
    ' IDM_REPLACE..IDM_SETFOCUSEDITOR), so these two can be greyed on their own condition
    ' the way IDM_FINDINPROJECT below is. Inside a range, the loop would force them on.
    IDM_GOBACK, IDM_GOFORWARD
    IDM_FINDINPROJECT, IDM_REPLACE, IDM_TOGGLEREPLACE
    IDM_GOTODEFINITION
    IDM_CLOSETAB, IDM_GOTONEXTFUNCTION, IDM_GOTOPREVFUNCTION
    IDM_GOTONEXTTAB, IDM_GOTOPREVTAB, IDM_EDITORSPLIT, IDM_TABSLIST
    IDM_GOTOHEADERFILE, IDM_GOTOSOURCEFILE, IDM_GOTOMAINFILE, IDM_GOTORESOURCEFILE
    IDM_BOOKMARKTOGGLE, IDM_BOOKMARKNEXT, IDM_BOOKMARKPREV, IDM_BOOKMARKCLEARALL
    IDM_BOOKMARKCLEARALLDOCS, IDM_CLEARALLBOOKMARKNODE, IDM_REMOVEBOOKMARKNODE
    IDM_GOTONEXTCOMPILEERROR, IDM_GOTOPREVCOMPILEERROR
    ' IDM_SETFOCUSEDITOR must stay LAST in this block -- frmMain_SearchTopMenuStates
    ' re-enables IDM_REPLACE..IDM_SETFOCUSEDITOR as a range.
    IDM_SETFOCUSEDITOR
    IDM_SEARCH_END
    
    '' VIEW
    IDM_VIEW_START
    IDM_VIEWSIDEPANEL, IDM_VIEWEXPLORER, IDM_VIEWOUTPUT, IDM_FUNCTIONLIST
    IDM_BOOKMARKSLIST, IDM_SPLITLEFTRIGHT, IDM_SPLITTOPBOTTOM
    IDM_ZOOMIN, IDM_ZOOMOUT, IDM_ZOOMRESET
    IDM_FOLDTOGGLE, IDM_FOLDBELOW, IDM_FOLDALL, IDM_UNFOLDALL
    IDM_VIEWTODO, IDM_VIEWNOTES, IDM_RESTOREMAIN, IDM_EXPLORERPOSITION
    ' Undock the Output panel into its own floating window / dock it back. Reached only
    ' from the Output panel's own context menu and its undock icon -- deliberately NOT on
    ' the View menu and NOT in the WM_COMMAND dispatcher, because the toggle reparents
    ' HWND_FRMOUTPUT and must run while the gesture that asked for it is finished.
    IDM_OUTPUTDOCKTOGGLE
    IDM_VIEW_END
    
    '' PROJECT
    IDM_PROJECT_START
    IDM_PROJECTNEW, IDM_PROJECTMANAGER, IDM_PROJECTOPEN, IDM_MRUPROJECT, IDM_MRUPROJECTFILES
    IDM_PROJECTCLOSE, IDM_PROJECTSAVE, IDM_PROJECTSAVEAS, IDM_PROJECTFILESADD, IDM_PROJECTOPTIONS  
    IDM_PROJECTFILETYPE, IDM_REMOVEFILEFROMPROJECT, IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX
    ' Explorer folder commands. Handled by the Explorer folder popup's own select
    ' callback, NOT posted as WM_COMMAND -- they act on a remembered row index rather
    ' than on a document, so the generic dispatcher has nothing to give them.
    IDM_EXPLORER_NEWFOLDER, IDM_EXPLORER_RENAMEFOLDER, IDM_EXPLORER_DELETEFOLDER
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
    ' New with the native debugger. The gdb implementation had no way to interrupt a
    ' running program at all -- the only routes back were a breakpoint or Stop.
    IDM_DEBUG_PAUSE
    IDM_DEBUG_STEPINTO
    IDM_DEBUG_STEPOVER
    IDM_DEBUG_STEPOUT
    IDM_DEBUG_RUNTOCURSOR
    IDM_DEBUG_TOGGLEBREAKPOINT
    IDM_DEBUG_DELETEALLBREAKPOINTS
    ' Unused Symbols report. Sits in the Debug menu because it is an ANALYSIS of the
    ' project, next to the other things that read the whole project rather than edit it.
    IDM_DEBUG_UNUSEDSYMBOLS
    IDM_DEBUG_END
    
    
    '' HELP
    ' IDM_HELP_FB (the .chm through HtmlHelp) and the OLD IDM_HELP_TIKO (the RTF
    ' frmHelpViewer) were both replaced by the one WebView2 window. Nothing persists a raw
    ' IDM number -- keybindings.ini keys on the STRING id -- so re-using the name here for
    ' the hand-written HTML help costs nothing, and adding a slot mid-enum is safe for the
    ' same reason.
    IDM_HELP_START
    ' Two commands, ONE window: the Help Center is the generated reference (F1, searches
    ' the selection), IDM_HELP_TIKO is tiko's own documentation. Both are
    ' frmHelpCenter_Show with a different HELPCENTER_SITE_*.
    IDM_HELP_CENTER, IDM_HELP_TIKO, IDM_ABOUT
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
        
    IDM_MRUCLEAR, IDM_MRUPROJECTCLEAR
    IDM_CONSOLE, IDM_GUI, IDM_RESOURCE, IDM_LINKMODULES   ' used for compiler directives in code
    IDM_32BIT, IDM_64BIT   ' mainly used for identifying compiler associated with a project
end enum

' ---- the menus themselves -----------------------------------------------------------------
' The table modMenuDefinitions builds and frmMenuBar renders: parent, id, child, and the
' three states a row can be in. Pure data, and portable.
type TOPMENU_TYPE
    nParentID   as long
    nID         as long
    nChildID    as long
    isDisabled  as boolean
    isSeparator as boolean
    isChecked   as boolean
end type
redim shared gTopMenu(any) as TOPMENU_TYPE
