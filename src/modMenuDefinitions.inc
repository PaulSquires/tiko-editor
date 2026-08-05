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

#include once "modMRU.bi"
#include once "frmKeyboard.bi"

' ========================================================================================
' Get the menu accelerator text (if one exists) for this menu item
' ========================================================================================
function getMenuAccelText( byval idMenu as long ) As DWSTRING
    dim as DWSTRING wszAccel
    ' Search the gKeys array for a match
    for i as long = lbound(gKeys) to ubound(gKeys)
        if gKeys(i).idAction = idMenu then
            if PsLen(gKeys(i).wszUserKeys) then
                wszAccel = gKeys(i).wszUserKeys
            else
                if gKeys(i).bDefaultDisabled = false then
                    wszAccel = gKeys(i).wszDefaultKeys
                end if   
            end if
            exit for   
        end if
    next
    
    if PsLen(wszAccel) then wszAccel = chr(9) & wszAccel
    return wszAccel
end function


' ========================================================================================
' Get the menu text based on the incoming control id
' ========================================================================================
Function getMenuText( byval idMenu as long ) As DWSTRING
    dim as DWSTRING wszText
    select case idMenu
        case 0: return "-"    ' this is a separator line
        case IDC_MENUBAR_FILE: wszText = L(2,"&File")
        case IDC_MENUBAR_EDIT: wszText =  L(14,"Edit")
        case IDC_MENUBAR_SEARCH: wszText = L(42,"Search")
        case IDC_MENUBAR_VIEW: wszText = L(53,"View")
        case IDC_MENUBAR_PROJECT: wszText = L(62,"Project")
        case IDC_MENUBAR_COMPILE: wszText = L(63,"Compile")
        case IDC_MENUBAR_DEBUG: wszText = L(241,"Debug")
        case IDC_MENUBAR_HELP: wszText = L(70,"Help")
        case IDC_MENUBAR_UPDATE
            if gApp.IsUpdateAvailable then
                wszText = L(96,"Update Available")
            end if

        case IDM_FILENEW: wszText = L(3,"New")
        case IDM_FILEOPEN: wszText = L(4,"Open") & "..." 
        case IDM_FILEOPENASTEMPLATE: wszText = L(12,"Open File As Template") & "..." 
        case IDM_MRU: return L(10,"Open Recent")
        case IDM_MRUBASE to IDM_MRUBASE + 9
            return gConfig.MRU( idMenu - IDM_MRUBASE )
        case IDM_MRUCLEAR: return L(185, "Clear this list")   
        case IDM_MRUFILES: return L(11,"(Empty)")
        case IDM_FILECLOSE: wszText = L(5,"Close") 
        case IDM_FILECLOSEALL: wszText = L(6,"Close All") 
        case IDM_FILESAVE: wszText = L(7,"Save")
        case IDM_FILESAVEAS: wszText = L(8,"Save As") & "..."
        case IDM_FILERENAME: wszText = L(21,"Rename") & "..."
        case IDM_FILEDUPLICATE: wszText = L(22,"Duplicate")
        case IDM_FILESAVEALL: wszText = L(9,"Save All")
        case IDM_KEYBOARDSHORTCUTS: wszText = L(180,"Keyboard Shortcuts") & "..."
        case IDM_USERTOOLS: return L(225,"User Tools")
        case IDM_USERTOOLSDIALOG: wszText = L(225,"User Tools") & "..."
        case IDM_USERTOOLSBASE to IDM_USERTOOLSBASE + ubound(gConfig.Tools)
            dim as DWSTRING wszText = gConfig.Tools(idMenu - IDM_USERTOOLSBASE).wszDescription
            dim as DWSTRING wszShortcut = createToolsMenuShortcut(idMenu - IDM_USERTOOLSBASE)
            if PsLen(wszShortcut) then wszText = wszText & chr(9) & wszShortcut
            return wszText
        case IDM_USERTOOLSLIST: return L(11,"(Empty)")
        case IDM_SETTINGS: return L(224,"Settings")
        case IDM_FORMATSUBMENU:   return L(465,"Format")
        case IDM_FORMATDOCUMENT:  wszText = L(466,"Format Document")
        case IDM_FORMATSELECTION: wszText = L(467,"Format Selection")
        case IDM_FORMATALLDOCS:   wszText = L(468,"Format All Open Documents")
        case IDM_FORMATPROJECT:   wszText = L(469,"Format Project") & "..."
        case IDM_FORMATOPTIONS:   wszText = L(470,"Format Options") & "..."
        case IDM_OPTIONSDIALOG: wszText = L(61,"Options") & "..."
        case IDM_THEMES: wszText = L(318,"Themes") & "..."
        case IDM_BUILDCONFIG: wszText = L(215,"Build Configurations") & "..."
        case IDM_EXIT: wszText = L(13,"Exit") 

        case IDM_UNDO: wszText = L(15,"Undo")
        case IDM_REDO: wszText = L(16,"Redo")
        case IDM_CUT: wszText = L(17,"Cut") 
        case IDM_COPY: wszText = L(18,"Copy")
        case IDM_PASTE: wszText = L(19,"Paste")
        case IDM_DELETELINE: wszText = L(20,"Delete Line")
        case IDM_COMMENTBLOCK: wszText = L(25,"Comment Block")
        case IDM_UNCOMMENTBLOCK: wszText = L(26,"UnComment Block")
        case IDM_DUPLICATELINE: wszText = L(28,"Duplicate Line")
        case IDM_MOVELINEUP: wszText = L(29,"Move Line Up")
        case IDM_MOVELINEDOWN: wszText = L(30,"Move Line Down")
        case IDM_SELECTLINE: wszText = L(41,"Select Line")
        case IDM_SELECTALL: wszText = L(40,"Select All")
            
        case IDM_TOGGLEREPLACE
             wszText = L(209, "Toggle Replace mode")
             idMenu = IDM_REPLACE
        case IDM_FINDPREV, IDM_FINDPREVACCEL     
             wszText = L(45, "Find Previous")
        case IDM_FINDNEXT, IDM_FINDNEXTACCEL     
             wszText = L(44, "Find Next")

        case IDM_GOTOPREVTAB: wszText = L(272, "Scroll Tabs Left")
        case IDM_GOTONEXTTAB: wszText = L(273, "Scroll Tabs Right")
        case IDM_TABSLIST: wszText = L(271, "Tab List")              
        case IDM_EDITORSPLIT
            ' The button opens a menu offering both axes, so the tooltip names neither.
            wszText = L(374,"Split Editor")

        case IDM_SEARCHSYMBOL: wszText = L(37,"Search Symbol") & "..."
        case IDM_FIND: wszText = L(43,"Find") & "..."
        case IDM_FINDINPROJECT: wszText = L(203,"Find in Project") & "..."
        case IDM_REPLACE: wszText = L(46,"Replace") & "..."
        case IDM_GOTODEFINITION: wszText = L(47,"Goto Definition")
        ' Index 48 was "Last Position", which this feature replaces -- reused rather than
        ' appended, and retranslated in all six .lang files.
        case IDM_GOBACK: wszText = L(48,"Go Back")
        case IDM_GOFORWARD: wszText = L(97,"Go Forward")
        case IDM_GOTONEXTFUNCTION: wszText = L(81,"Next Function")
        case IDM_GOTOPREVFUNCTION: wszText = L(82,"Previous Function")
        case IDM_GOTOHEADERFILE: wszText = L(189,"Goto Header File")
        case IDM_GOTOSOURCEFILE: wszText = L(190,"Goto Code File")
        case IDM_GOTOMAINFILE: wszText = L(191,"Goto Main File")
        case IDM_GOTORESOURCEFILE: wszText = L(192,"Goto Resource File")
        case IDM_BOOKMARKTOGGLE: wszText = L(49,"Toggle Bookmark")
        case IDM_BOOKMARKNEXT: wszText = L(50,"Next Bookmark")
        case IDM_BOOKMARKPREV: wszText = L(51,"Previous Bookmark")
        case IDM_BOOKMARKCLEARALL: wszText = L(52,"Clear Bookmarks")

        case IDM_VIEWSIDEPANEL: wszText = L(194,"View Side Panel")
        case IDM_VIEWEXPLORER: wszText = L(197,"View Explorer Window")
        case IDM_FUNCTIONLIST: wszText = L(181,"View Function List")
        case IDM_BOOKMARKSLIST: wszText = L(281,"View Bookmarks List")
        case IDM_VIEWOUTPUT: wszText = L(198,"View Output Window")
        case IDM_EXPLORERPOSITION:
            wszText = L(83,"Move Explorer Window") & " " & _
            iif(gConfig.ExplorerPositionRight, L(84, "Left"), L(85, "Right"))
        case IDM_SPLITLEFTRIGHT: wszText = L(67,"Toggle Split Editor Left/Right")
        case IDM_SPLITTOPBOTTOM: wszText = L(68,"Toggle Split Editor Top/Bottom")
        case IDM_ZOOMIN: wszText = L(58,"Zoom In")
        case IDM_ZOOMOUT: wszText = L(59,"Zoom Out")
        case IDM_ZOOMRESET: wszText = L(69,"Zoom Reset")
        case IDM_FOLDTOGGLE: wszText = L(54,"Toggle Current Fold Point")
        case IDM_FOLDBELOW: wszText = L(55,"Toggle Current And All Below")
        case IDM_FOLDALL: wszText = L(56,"Fold All")
        case IDM_UNFOLDALL: wszText = L(57,"Unfold All")
        case IDM_RESTOREMAIN: wszText = L(60,"Restore Main Window Size")
        case IDM_VIEWTODO: wszText = L(204,"TODO")
        case IDM_VIEWNOTES: wszText = L(205,"Notes")

        case IDM_PROJECTNEW: wszText = L(144,"New Project") & "..."
        case IDM_PROJECTOPEN: wszText = L(146,"Open Project") & "..." 
        case IDM_MRUPROJECT: wszText = L(179,"Recent Projects")
        case IDM_MRUPROJECTBASE to IDM_MRUPROJECTBASE + 9
            return gConfig.MRUPROJECT( idMenu - IDM_MRUPROJECTBASE )
        case IDM_MRUPROJECTCLEAR: return L(185, "Clear this list")   
        case IDM_MRUPROJECTFILES: return L(11,"(Empty)")
        case IDM_PROJECTCLOSE: wszText = L(147,"Close Project")
        case IDM_PROJECTSAVE: wszText = L(148,"Save Project")
        case IDM_PROJECTSAVEAS: wszText = L(149,"Save Project As") & "..."
        case IDM_PROJECTFILESADD: wszText = L(168,"Add Files to Project") & "..."
        case IDM_PROJECTOPTIONS: wszText = L(150,"Project Options") & "..."      

        case IDM_BUILDEXECUTE: wszText = L(64,"Build And Execute")
        case IDM_COMPILE: wszText = L(63,"Compile")
        case IDM_REBUILDALL: wszText = L(208,"Rebuild All")
        case IDM_QUICKRUN: wszText = L(221,"Quick Run")
        case IDM_RUNEXE: wszText = L(65,"Run Executable")
        case IDM_COMMANDLINE: wszText = L(66,"Command Line") & "..."
        case IDM_COMPILEMODULE: wszText = L(78,"Compile Module")

        case IDM_DEBUG_STARTDEBUGGING
            if gApp.IsDebuggerActive then
                wszText = L(249,"Continue Debugging")
            else    
                wszText = L(242,"Start Debugging")
            end if
        case IDM_DEBUG_STOPDEBUGGING: wszText = L(250,"Stop Debugging")
        case IDM_DEBUG_PAUSE: wszText = L(433,"Break")
        case IDM_DEBUG_STEPINTO: wszText = L(243,"Step")
        case IDM_DEBUG_STEPOVER: wszText = L(244,"Step Over")
        case IDM_DEBUG_STEPOUT: wszText = L(251,"Step Out")
        case IDM_DEBUG_RUNTOCURSOR: wszText = L(252,"Run to Cursor")
        case IDM_DEBUG_TOGGLEBREAKPOINT: wszText = L(245,"Toggle Breakpoint")
        case IDM_DEBUG_DELETEALLBREAKPOINTS: wszText = L(246,"Delete All Breakpoints")
        case IDM_DEBUG_UNUSEDSYMBOLS: wszText = L(418,"Unused Symbols...")

        case IDM_HELP_CENTER: wszText = L(86,"Help Center")
        case IDM_HELP_TIKO: wszText = L(519,"Tiko Editor Help")
        case IDM_ABOUT: wszText = L(71,"About")

        case else
            return ""
    end select
    
    wszText = wszText & getMenuAccelText( idMenu )
    return wszText
end function


' ========================================================================================
' Set the values for one element in the gTopMenu TYPE array
' ========================================================================================
function setTopMenuItem( _
            byval nParentID as long, _
            byval nID as long, _
            byval nChildID as long, _
            byval isDisabled as boolean, _
            byval isSeparator as boolean _
            ) as long
                            
    redim preserve gTopMenu(ubound(gTopMenu)+1) as TOPMENU_TYPE
    dim as long i = ubound(gTopMenu)

    gTopMenu(i).nParentID = nParentID
    gTopMenu(i).nID = nID
    gTopMenu(i).nChildID = nChildID
    gTopMenu(i).isDisabled = isDisabled
    gTopMenu(i).isSeparator = isSeparator
    
    function = 0

end function

' The gTopMenu array now holds only the STATIC menu structure; frmMenuBar.inc builds
' the PsPopupMenu trees from it at startup. The dynamic sections (MRU files/sessions/
' projects, User Tools) are rebuilt straight into their popups each time that submenu
' opens, so the old gTopMenu row-recycling machinery (setTopMenuMRUItem, getTopMenuPtr,
' clearMRUFilesItems, getNextFreeMRUindex, updateMRU*Items) is gone.
function buildTopMenuDefinitions() as long
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILENEW, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILEOPEN, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILEOPENASTEMPLATE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_MRU, IDM_MRUFILES, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILECLOSE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILECLOSEALL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILESAVE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILESAVEALL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILESAVEAS, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILERENAME, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_FILEDUPLICATE, 0, false, false )
    ' Sessions are gone -- Load/Save/Close Session and Recent Sessions sat between this
    ' separator and the one above it. Every workspace is a project now, so the last one
    ' always reopens with no menu verb at all. The separator stays: it divides the
    ' file-level commands from the tools/settings block below.
    setTopMenuItem( IDC_MENUBAR_FILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_SETTINGS, IDM_OPTIONSDIALOG, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_OPTIONSDIALOG, 0, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_THEMES, 0, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_BUILDCONFIG, 0, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_KEYBOARDSHORTCUTS, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_USERTOOLS, IDM_USERTOOLSLIST, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_EXIT, 0, false, false )
    
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_UNDO, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_REDO, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_CUT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_COPY, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_PASTE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_DELETELINE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_DUPLICATELINE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_MOVELINEUP, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_MOVELINEDOWN, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_COMMENTBLOCK, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_UNCOMMENTBLOCK, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_SELECTLINE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_SELECTALL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_EDIT, 0, 0, false, true )
    ' Code formatter, as a submenu -- four commands is enough to be worth grouping, and it
    ' keeps the Edit menu from growing a fourth screenful.
    setTopMenuItem( IDC_MENUBAR_EDIT, IDM_FORMATSUBMENU, IDM_FORMATDOCUMENT, false, false )
    setTopMenuItem( IDM_FORMATSUBMENU, IDM_FORMATDOCUMENT, 0, false, false )
    setTopMenuItem( IDM_FORMATSUBMENU, IDM_FORMATSELECTION, 0, false, false )
    setTopMenuItem( IDM_FORMATSUBMENU, 0, 0, false, true )
    setTopMenuItem( IDM_FORMATSUBMENU, IDM_FORMATALLDOCS, 0, false, false )
    setTopMenuItem( IDM_FORMATSUBMENU, IDM_FORMATPROJECT, 0, false, false )
    setTopMenuItem( IDM_FORMATSUBMENU, 0, 0, false, true )
    setTopMenuItem( IDM_FORMATSUBMENU, IDM_FORMATOPTIONS, 0, false, false )

    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_SEARCHSYMBOL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_FIND, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_FINDNEXTACCEL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_FINDPREVACCEL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_FINDINPROJECT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_REPLACE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_GOTODEFINITION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_GOBACK, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_GOFORWARD, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_GOTONEXTFUNCTION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_GOTOPREVFUNCTION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_BOOKMARKTOGGLE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_BOOKMARKNEXT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_BOOKMARKPREV, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_SEARCH, IDM_BOOKMARKCLEARALL, 0, false, false )

    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_VIEWSIDEPANEL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_VIEWEXPLORER, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_VIEWOUTPUT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_FUNCTIONLIST, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_BOOKMARKSLIST, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_ZOOMIN, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_ZOOMOUT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_ZOOMRESET, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_SPLITLEFTRIGHT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_SPLITTOPBOTTOM, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_FOLDTOGGLE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_FOLDBELOW, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_FOLDALL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_UNFOLDALL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_EXPLORERPOSITION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_VIEW, IDM_RESTOREMAIN, 0, false, false )

    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTNEW, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTOPEN, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_MRUPROJECT, IDM_MRUPROJECTFILES, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTCLOSE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTSAVEAS, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTSAVE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTFILESADD, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_PROJECT, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_PROJECT, IDM_PROJECTOPTIONS, 0, false, false )

    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_BUILDEXECUTE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_COMPILE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_REBUILDALL, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_COMPILEMODULE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_COMPILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_QUICKRUN, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_RUNEXE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_COMPILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_COMPILE, IDM_COMMANDLINE, 0, false, false )

    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_STARTDEBUGGING, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_PAUSE, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_STOPDEBUGGING, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_STEPINTO, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_STEPOVER, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_STEPOUT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_RUNTOCURSOR, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_TOGGLEBREAKPOINT, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_DELETEALLBREAKPOINTS, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_DEBUG, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_DEBUG, IDM_DEBUG_UNUSEDSYMBOLS, 0, false, false )

    setTopMenuItem( IDC_MENUBAR_HELP, IDM_HELP_CENTER, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_HELP, IDM_HELP_TIKO, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_HELP, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_HELP, IDM_ABOUT, 0, false, false )
    
    function = 0
end function
