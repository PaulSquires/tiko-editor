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
            if len(gKeys(i).wszUserKeys) then
                wszAccel = gKeys(i).wszUserKeys
            else
                if gKeys(i).bDefaultDisabled = false then
                    wszAccel = gKeys(i).wszDefaultKeys
                end if   
            end if
            exit for   
        end if
    next
    
    if len(wszAccel) then wszAccel = chr(9) & wszAccel
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
        case IDC_MENUBAR_DEBUG: wszText = L(245,"Debug")
        case IDC_MENUBAR_HELP: wszText = L(71,"Help")
        case IDC_MENUBAR_UPDATE
            if gApp.IsUpdateAvailable then
                wszText = L(97,"Update Available")
            end if

        case IDM_FILENEW: wszText = L(3,"New")
        case IDM_FILEOPEN: wszText = L(4,"Open") & "..." 
        case IDM_FILEOPENASTEMPLATE: wszText = L(12,"Open File As Template") & "..." 
        case IDM_MRU: return L(10,"Open Recent")
        case IDM_MRUBASE to IDM_MRUBASE + 9
            return gConfig.MRU( idMenu - IDM_MRUBASE )
        case IDM_MRUCLEAR: return L(188, "Clear this list")   
        case IDM_MRUFILES: return L(11,"(Empty)")
        case IDM_FILECLOSE: wszText = L(5,"Close") 
        case IDM_FILECLOSEALL: wszText = L(6,"Close All") 
        case IDM_FILESAVE: wszText = L(7,"Save")
        case IDM_FILESAVEAS: wszText = L(8,"Save As") & "..."
        case IDM_FILERENAME: wszText = L(21,"Rename") & "..."
        case IDM_FILEDUPLICATE: wszText = L(22,"Duplicate")
        case IDM_FILESAVEALL: wszText = L(9,"Save All")
        case IDM_SAVESESSION: wszText = L(277,"Save Session") & "..."
        case IDM_LOADSESSION: wszText = L(278,"Load Session") & "..."
        case IDM_CLOSESESSION: wszText = L(284,"Close Session")
        case IDM_MRUSESSION: wszText = L(181,"Recent Sessions")
        case IDM_MRUSESSIONBASE to IDM_MRUSESSIONBASE + 9
            return gConfig.MRUSESSION( idMenu - IDM_MRUSESSIONBASE )
        case IDM_MRUSESSIONCLEAR: return L(188, "Clear this list")   
        case IDM_MRUSESSIONFILES: return L(11,"(Empty)")
        case IDM_KEYBOARDSHORTCUTS: wszText = L(183,"Keyboard Shortcuts") & "..."
        case IDM_USERTOOLS: return L(229,"User Tools")
        case IDM_USERTOOLSDIALOG: wszText = L(229,"User Tools") & "..."
        case IDM_USERTOOLSBASE to IDM_USERTOOLSBASE + ubound(gConfig.Tools)
            dim as DWSTRING wszText = gConfig.Tools(idMenu - IDM_USERTOOLSBASE).wszDescription
            dim as DWSTRING wszShortcut = createToolsMenuShortcut(idMenu - IDM_USERTOOLSBASE)
            if len(wszShortcut) then wszText = wszText & chr(9) & wszShortcut
            return wszText
        case IDM_USERTOOLSLIST: return L(11,"(Empty)")
        case IDM_SETTINGS: return L(228,"Settings")
        case IDM_OPTIONSDIALOG: wszText = L(61,"Options") & "..."
        case IDM_THEMES: wszText = L(329,"Themes") & "..."
        case IDM_BUILDCONFIG: wszText = L(219,"Build Configurations") & "..."
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
             wszText = L(213, "Toggle Replace mode")
             idMenu = IDM_REPLACE
        case IDM_FINDPREV, IDM_FINDPREVACCEL     
             wszText = L(45, "Find Previous")
        case IDM_FINDNEXT, IDM_FINDNEXTACCEL     
             wszText = L(44, "Find Next")

        case IDM_GOTOPREVTAB: wszText = L(280, "Scroll Tabs Left")
        case IDM_GOTONEXTTAB: wszText = L(281, "Scroll Tabs Right")
        case IDM_TABSLIST: wszText = L(279, "Tab List")              
        case IDM_EDITORSPLIT
            ' The button opens a menu offering both axes, so the tooltip names neither.
            wszText = L(385,"Split Editor")

        case IDM_SEARCHSYMBOL: wszText = L(37,"Search Symbol") & "..."
        case IDM_FIND: wszText = L(43,"Find") & "..."
        case IDM_FINDINPROJECT: wszText = L(206,"Find in Project") & "..."
        case IDM_REPLACE: wszText = L(46,"Replace") & "..."
        case IDM_GOTODEFINITION: wszText = L(47,"Goto Definition")
        ' Index 48 was "Last Position", which this feature replaces -- reused rather than
        ' appended, and retranslated in all six .lang files.
        case IDM_GOBACK: wszText = L(48,"Go Back")
        case IDM_GOFORWARD: wszText = L(98,"Go Forward")
        case IDM_GOTONEXTFUNCTION: wszText = L(82,"Next Function")
        case IDM_GOTOPREVFUNCTION: wszText = L(83,"Previous Function")
        case IDM_GOTOHEADERFILE: wszText = L(192,"Goto Header File")
        case IDM_GOTOSOURCEFILE: wszText = L(193,"Goto Code File")
        case IDM_GOTOMAINFILE: wszText = L(194,"Goto Main File")
        case IDM_GOTORESOURCEFILE: wszText = L(195,"Goto Resource File")
        case IDM_BOOKMARKTOGGLE: wszText = L(49,"Toggle Bookmark")
        case IDM_BOOKMARKNEXT: wszText = L(50,"Next Bookmark")
        case IDM_BOOKMARKPREV: wszText = L(51,"Previous Bookmark")
        case IDM_BOOKMARKCLEARALL: wszText = L(52,"Clear Bookmarks")

        case IDM_VIEWSIDEPANEL: wszText = L(197,"View Side Panel")
        case IDM_VIEWEXPLORER: wszText = L(200,"View Explorer Window")
        case IDM_FUNCTIONLIST: wszText = L(184,"View Function List")
        case IDM_BOOKMARKSLIST: wszText = L(291,"View Bookmarks List")
        case IDM_VIEWOUTPUT: wszText = L(201,"View Output Window")
        case IDM_EXPLORERPOSITION:
            wszText = L(84,"Move Explorer Window") & " " & _
            iif(gConfig.ExplorerPositionRight, L(85, "Left"), L(86, "Right"))
        case IDM_SPLITLEFTRIGHT: wszText = L(67,"Toggle Split Editor Left/Right")
        case IDM_SPLITTOPBOTTOM: wszText = L(68,"Toggle Split Editor Top/Bottom")
        case IDM_ZOOMIN: wszText = L(58,"Zoom In")
        case IDM_ZOOMOUT: wszText = L(59,"Zoom Out")
        case IDM_ZOOMRESET: wszText = L(70,"Zoom Reset")
        case IDM_FOLDTOGGLE: wszText = L(54,"Toggle Current Fold Point")
        case IDM_FOLDBELOW: wszText = L(55,"Toggle Current And All Below")
        case IDM_FOLDALL: wszText = L(56,"Fold All")
        case IDM_UNFOLDALL: wszText = L(57,"Unfold All")
        case IDM_RESTOREMAIN: wszText = L(60,"Restore Main Window Size")
        case IDM_VIEWTODO: wszText = L(208,"TODO")
        case IDM_VIEWNOTES: wszText = L(209,"Notes")

        case IDM_PROJECTNEW: wszText = L(145,"New Project") & "..."
        case IDM_PROJECTOPEN: wszText = L(147,"Open Project") & "..." 
        case IDM_MRUPROJECT: wszText = L(182,"Recent Projects")
        case IDM_MRUPROJECTBASE to IDM_MRUPROJECTBASE + 9
            return gConfig.MRUPROJECT( idMenu - IDM_MRUPROJECTBASE )
        case IDM_MRUPROJECTCLEAR: return L(188, "Clear this list")   
        case IDM_MRUPROJECTFILES: return L(11,"(Empty)")
        case IDM_PROJECTCLOSE: wszText = L(148,"Close Project")
        case IDM_PROJECTSAVE: wszText = L(149,"Save Project")
        case IDM_PROJECTSAVEAS: wszText = L(150,"Save Project As") & "..."
        case IDM_PROJECTFILESADD: wszText = L(170,"Add Files to Project") & "..."
        case IDM_PROJECTOPTIONS: wszText = L(151,"Project Options") & "..."      

        case IDM_BUILDEXECUTE: wszText = L(64,"Build And Execute")
        case IDM_COMPILE: wszText = L(63,"Compile")
        case IDM_REBUILDALL: wszText = L(212,"Rebuild All")
        case IDM_QUICKRUN: wszText = L(225,"Quick Run")
        case IDM_RUNEXE: wszText = L(65,"Run Executable")
        case IDM_COMMANDLINE: wszText = L(66,"Command Line") & "..."
        case IDM_COMPILEMODULE: wszText = L(79,"Compile Module")

        case IDM_DEBUG_STARTDEBUGGING
            if gApp.IsDebuggerActive then
                wszText = L(253,"Continue Debugging")
            else    
                wszText = L(246,"Start Debugging")
            end if
        case IDM_DEBUG_STOPDEBUGGING: wszText = L(254,"Stop Debugging")
        case IDM_DEBUG_PAUSE: wszText = L(601,"Break")
        case IDM_DEBUG_STEPINTO: wszText = L(247,"Step Into")
        case IDM_DEBUG_STEPOVER: wszText = L(248,"Step Over")
        case IDM_DEBUG_STEPOUT: wszText = L(255,"Step Out")
        case IDM_DEBUG_RUNTOCURSOR: wszText = L(256,"Run to Cursor")
        case IDM_DEBUG_TOGGLEBREAKPOINT: wszText = L(249,"Toggle Breakpoint")
        case IDM_DEBUG_DELETEALLBREAKPOINTS: wszText = L(250,"Delete All Breakpoints")

        case IDM_HELP_CENTER: wszText = L(87,"Help Center")
        case IDM_ABOUT: wszText = L(72,"About")

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
    setTopMenuItem( IDC_MENUBAR_FILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_LOADSESSION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_SAVESESSION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_CLOSESESSION, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_MRUSESSION, IDM_MRUSESSIONFILES, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_USERTOOLS, IDM_USERTOOLSLIST, false, false )
    setTopMenuItem( IDC_MENUBAR_FILE, IDM_SETTINGS, IDM_OPTIONSDIALOG, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_OPTIONSDIALOG, 0, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_THEMES, 0, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_BUILDCONFIG, 0, false, false )
    setTopMenuItem( IDM_SETTINGS, IDM_KEYBOARDSHORTCUTS, 0, false, false )
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

    setTopMenuItem( IDC_MENUBAR_HELP, IDM_HELP_CENTER, 0, false, false )
    setTopMenuItem( IDC_MENUBAR_HELP, 0, 0, false, true )
    setTopMenuItem( IDC_MENUBAR_HELP, IDM_ABOUT, 0, false, false )
    
    function = 0
end function
