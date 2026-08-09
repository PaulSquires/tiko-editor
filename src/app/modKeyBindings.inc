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

'  app/modKeyBindings.inc  --  the binding array, the 112 defaults, and the ini writer.
'
'  Split out of src/modKeyBindings.inc along the line between DATA and what has to ask
'  Windows -- see app/modKeyBindings.bi for why. What stayed behind is the name -> virtual
'  key vocabulary, which resolves punctuation through VkKeyScanEx against the live keyboard
'  layout, plus the self-test and the Win32 accelerator install.
'
'  THE 112 DEFAULTS ARE THE POINT OF THE MOVE. Phase 7c's shell binary builds a PsAccelTable
'  out of them, and getMenuAccelText -- which is app-layer and walks gKeys to put "Ctrl+S"
'  beside a menu row -- returned blank for every row while the array lived in the shell.

#pragma once

#include once "app/modKeyBindings.bi"
#include once "app/modMenuIds.bi"
#include once "app/modTextFile.inc"

' ========================================================================================
' Save the gKeys() keybinding array to disk file
'
' FORMAT (unchanged, and a user's existing file must keep loading):
'     IDM_FILESAVE:Ctrl+S
'     IDM_DUPLICATELINE(DISABLED):Ctrl+Shift+D
' The key is the IDM_* NAME, never its numeric value -- renumbering a constant in the
' source must not silently invalidate every mapping a user has saved.
' ========================================================================================
function frmKeyboard_SaveKeyBindings( byval wszFilename as DWSTRING ) as long
    ' One buffer, one write -- the same shape clsConfig now uses, and for the same
    ' reason: a failure partway through a per-line stream leaves a truncated
    ' keybindings.ini that looks saved. The file is plain ASCII with no BOM, which
    ' is what CTextStream.Create wrote without the bUnicode flag, so it is written
    ' back the same way.
    dim as DWSTRING wszOut
    dim as DWSTRING wszLine
    for i as long = lbound(gKeys) to ubound(gKeys)
        wszLine = gKeys(i).wszMsgString

        if gKeys(i).bDefaultDisabled then
            wszLine = wszLine & "(DISABLED)"
        end if

        wszLine = wszLine & ":" & gKeys(i).wszUserKeys
        wszOut &= wszLine & vbcrlf
    next
    TextFile_WriteAnsi( wszFilename, wszOut )
    function = 0
end function


' ========================================================================================
' Load a default keybinding into the global array
' ========================================================================================
function frmKeyBoard_AddKeyBinding ( _
            byval wszCategory as DWSTRING, _
            byval idAction as long, _
            byval wszMsgString as DWSTRING, _
            byval wszDescription as DWSTRING, _
            byval wszDefaultKeys as DWSTRING _
            ) as long

    dim as long ub = ubound(gKeys) + 1
    redim preserve gKeys(ub) as KEYBINDINGS_TYPE

    with gKeys(ub)
        .wszCategory = wszCategory
        .idAction = idAction
        .wszMsgString = wszMsgString
        .wszDescription = wszDescription
        .wszDefaultKeys = wszDefaultKeys
    end with

    function = 0
end function


' ========================================================================================
' Load a user defined keybinding ini file to override existing global key.
' ========================================================================================
function frmKeyBoard_AddKeyBinding_User ( _
            byval wszMsgString as DWSTRING, _
            byval wszUserKeys as DWSTRING, _
            byval bDisabled as boolean _
            ) as long

    ' Search the array to see if the id exists. Update it for found.
    dim as long nFoundIdx = -1

    for i as long = lbound(gKeys) to ubound(gKeys)
        if gKeys(i).wszMsgString = wszMsgString then
            nFoundIdx = i
            exit for
        end if
    next

    if nFoundIdx <> -1 then
        with gKeys(nFoundIdx)
            ' Update the entry with any user defined keybinding or if the user has
            ' disabled the Default keyboard shortcut.
            .wszUserKeys = wszUserKeys
            .bDefaultDisabled = bDisabled
        end with
    end if

    function = 0
end function


' ========================================================================================
' ' Create the default keyboard bindings / shortcut mappings
' ========================================================================================
function frmKeyboard_CreateDefaultKeyBindings() as long
    erase gKeys

    '' FILE MENU
    frmKeyBoard_AddKeyBinding( "File", IDM_FILENEW, "IDM_FILENEW", "New file", "Ctrl+N" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILEOPEN, "IDM_FILEOPEN", "Open file(s)", "Ctrl+O" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILEOPENASTEMPLATE, "IDM_FILEOPENASTEMPLATE", "Open file(s) as template", "" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILECLOSE, "IDM_FILECLOSE", "Close file", "Ctrl+W" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILECLOSEALL, "IDM_FILECLOSEALL", "Close all files", "Ctrl+Shift+W" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILESAVE, "IDM_FILESAVE", "Save file", "Ctrl+S" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILESAVEAS, "IDM_FILESAVEAS", "Save file as a different name", "" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILESAVEALL, "IDM_FILESAVEALL", "Save all files", "Ctrl+Shift+S" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILERENAME, "IDM_FILERENAME", "Rename file", "" )
    frmKeyBoard_AddKeyBinding( "File", IDM_FILEDUPLICATE, "IDM_FILEDUPLICATE", "Duplicate file", "" )
    ' "Comma", not ",". The vocabulary's canonical name for VK_OEM_COMMA is the word (gKeyNames
    ' offers no "," entry at all), and the canonical-spelling assertion below compares the
    ' defaults table against exactly that. AccelKeyToValue accepts both spellings, so the
    ' shortcut fired either way -- this was a consistency failure, not a broken key.
    ' Nothing migrates: keybindings.ini stores only wszUserKeys, never a default.
    frmKeyBoard_AddKeyBinding( "File", IDM_OPTIONSDIALOG, "IDM_OPTIONSDIALOG", "Options", "Ctrl+Comma" )
    frmKeyBoard_AddKeyBinding( "File", IDM_THEMES, "IDM_THEMES", "Theme Editor", "Ctrl+Shift+T" )
    frmKeyBoard_AddKeyBinding( "File", IDM_KEYBOARDSHORTCUTS, "IDM_KEYBOARDSHORTCUTS", "Keyboard shortcuts", "Ctrl+K" )
    frmKeyBoard_AddKeyBinding( "File", IDM_BUILDCONFIG, "IDM_BUILDCONFIG", "Build configurations", "F7" )
    frmKeyBoard_AddKeyBinding( "File", IDM_EXIT, "IDM_EXIT", "Exit application", "Alt+F4" )

    '' EDIT MENU
    frmKeyBoard_AddKeyBinding( "Edit", IDM_UNDO, "IDM_UNDO", "Undo", "Ctrl+Z" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_REDO, "IDM_REDO", "Redo", "Ctrl+Shift+Z" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_CUT, "IDM_CUT", "Cut selection to the clipboard", "Ctrl+X" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_COPY, "IDM_COPY", "Copy selection to the clipboard", "Ctrl+C" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_PASTE, "IDM_PASTE", "Paste clipboard contents", "Ctrl+V" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_DELETELINE, "IDM_DELETELINE", "Delete the current editor line", "Ctrl+Y" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_DUPLICATELINE, "IDM_DUPLICATELINE", "Duplicate line", "Ctrl+D" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_MOVELINEUP, "IDM_MOVELINEUP", "Move line up", "Alt+Up" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_MOVELINEDOWN, "IDM_MOVELINEDOWN", "Move line down", "Alt+Down" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_COMMENTBLOCK, "IDM_COMMENTBLOCK", "Comment block", "Ctrl+/" )   'VK_OEM_2
    frmKeyBoard_AddKeyBinding( "Edit", IDM_UNCOMMENTBLOCK, "IDM_UNCOMMENTBLOCK", "UnComment block", "Ctrl+Shift+/" )  ' VK_OEM_2
    frmKeyBoard_AddKeyBinding( "Edit", IDM_SELECTLINE, "IDM_SELECTLINE", "Select line", "Ctrl+L" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_SELECTALL, "IDM_SELECTALL", "Select all", "Ctrl+A" )
    ' The following are non-visual (no topmenu item)
    frmKeyBoard_AddKeyBinding( "Edit", IDM_FINDNEXTACCEL, "IDM_FINDNEXTACCEL", "Find next", "F3" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_FINDPREVACCEL, "IDM_FINDPREVACCEL", "Find previous", "Shift+F3" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_INDENTBLOCK, "IDM_INDENTBLOCK", "Indent block", "TAB" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_UNINDENTBLOCK, "IDM_UNINDENTBLOCK", "UnIndent block", "Shift+TAB" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_INSERTFILE, "IDM_INSERTFILE", "Insert file", "Ctrl+I" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_TOUPPERCASE, "IDM_TOUPPERCASE", "Change selection to uppercase", "Ctrl+Alt+U" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_TOLOWERCASE, "IDM_TOLOWERCASE", "Change selection to lowercase", "Ctrl+Alt+L" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_TOMIXEDCASE, "IDM_TOMIXEDCASE", "Change selection to mixedcase", "Ctrl+Alt+X" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_NEWLINEBELOWCURRENT, "IDM_NEWLINEBELOWCURRENT", "Insert new line below current line", "Ctrl+Enter" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_SETFOCUSEDITOR, "IDM_SETFOCUSEDITOR", "Set keyboard focus to the editing window", "Ctrl+Tilde" ) ' VK_OEM_3
    ' Code formatter. Shift+Alt+F is VS Code's Format Document and the closest thing to a
    ' cross-editor convention; the other three ship unbound rather than inventing one.
    frmKeyBoard_AddKeyBinding( "Edit", IDM_FORMATDOCUMENT, "IDM_FORMATDOCUMENT", "Format document", "Shift+Alt+F" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_FORMATSELECTION, "IDM_FORMATSELECTION", "Format selection", "" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_FORMATALLDOCS, "IDM_FORMATALLDOCS", "Format all open documents", "" )
    frmKeyBoard_AddKeyBinding( "Edit", IDM_FORMATPROJECT, "IDM_FORMATPROJECT", "Format project", "" )

    '' SEARCH MENU
    frmKeyBoard_AddKeyBinding( "Search", IDM_SEARCHSYMBOL, "IDM_SEARCHSYMBOL", "Search Symbol", "Ctrl+P" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_FIND, "IDM_FIND", "Find", "Ctrl+F" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_FINDINPROJECT, "IDM_FINDINPROJECT", "Find in Project", "Ctrl+Shift+F" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_REPLACE, "IDM_REPLACE", "Replace", "Ctrl+H" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTODEFINITION, "IDM_GOTODEFINITION", "Goto the symbol definition", "F12" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOBACK, "IDM_GOBACK", "Go back to the previous position", "Alt+Left" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOFORWARD, "IDM_GOFORWARD", "Go forward to the next position", "Alt+Right" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTONEXTFUNCTION, "IDM_GOTONEXTFUNCTION", "Goto next sub/function", "Ctrl+PgDn" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOPREVFUNCTION, "IDM_GOTOPREVFUNCTION", "Goto previous sub/function", "Ctrl+PgUp" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKTOGGLE, "IDM_BOOKMARKTOGGLE", "Toggle bookmark", "Ctrl+F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKNEXT, "IDM_BOOKMARKNEXT", "Goto the next bookmark", "F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKPREV, "IDM_BOOKMARKPREV", "Goto the previous bookmark", "Shift+F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKCLEARALL, "IDM_BOOKMARKCLEARALL", "Clear bookmarks", "Ctrl+Shift+F2" )
    ' The following are non-visual (no topmenu item)
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOHEADERFILE, "IDM_GOTOHEADERFILE", "Goto header file", "Ctrl+Shift+H" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOSOURCEFILE, "IDM_GOTOSOURCEFILE", "Goto code file", "Ctrl+Shift+C" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOMAINFILE, "IDM_GOTOMAINFILE", "Goto main file", "Ctrl+Shift+M" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTORESOURCEFILE, "IDM_GOTORESOURCEFILE", "Goto resource file", "Ctrl+Shift+R" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTONEXTTAB, "IDM_GOTONEXTTAB", "Goto next open editor tab", "Ctrl+Tab" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOPREVTAB, "IDM_GOTOPREVTAB", "Goto previous open editor tab", "Ctrl+Shift+Tab" )

    '' VIEW MENU
    frmKeyBoard_AddKeyBinding( "View", IDM_VIEWSIDEPANEL, "IDM_VIEWSIDEPANEL", "View Side Panel", "Ctrl+B" )
    frmKeyBoard_AddKeyBinding( "View", IDM_VIEWEXPLORER, "IDM_VIEWEXPLORER", "View Explorer window", "Ctrl+F4" )
    frmKeyBoard_AddKeyBinding( "View", IDM_VIEWOUTPUT, "IDM_VIEWOUTPUT", "View Output window", "Ctrl+F9" )
    frmKeyBoard_AddKeyBinding( "View", IDM_FUNCTIONLIST, "IDM_FUNCTIONLIST", "View function List", "F4" )
    frmKeyBoard_AddKeyBinding( "View", IDM_BOOKMARKSLIST, "IDM_BOOKMARKSLIST", "View Bookmarks List", "Shift+F4" )
    frmKeyBoard_AddKeyBinding( "View", IDM_ZOOMIN, "IDM_ZOOMIN", "Zoom In", "Ctrl+Plus" )     ' VK_OEM_PLUS
    frmKeyBoard_AddKeyBinding( "View", IDM_ZOOMOUT, "IDM_ZOOMOUT", "Zoom Out", "Ctrl+Minus" )   ' VK_OEM_MINUS
    frmKeyBoard_AddKeyBinding( "View", IDM_ZOOMRESET, "IDM_ZOOMRESET", "Zoom Reset", "Ctrl+0" )   ' Ctrl+ZERO
    frmKeyBoard_AddKeyBinding( "View", IDM_SPLITLEFTRIGHT, "IDM_SPLITLEFTRIGHT", "Toggle Split Editor Left/Right", "Ctrl+\" )
    frmKeyBoard_AddKeyBinding( "View", IDM_SPLITTOPBOTTOM, "IDM_SPLITTOPBOTTOM", "Toggle Split Editor Top/Bottom", "Ctrl+Shift+\" )
    frmKeyBoard_AddKeyBinding( "View", IDM_FOLDTOGGLE, "IDM_FOLDTOGGLE", "Toggle current fold point", "F8" )
    frmKeyBoard_AddKeyBinding( "View", IDM_FOLDBELOW, "IDM_FOLDBELOW", "Fold current line and all below", "Ctrl+F8" )
    frmKeyBoard_AddKeyBinding( "View", IDM_FOLDALL, "IDM_FOLDALL", "Fold all lines", "Shift+F8" )
    frmKeyBoard_AddKeyBinding( "View", IDM_UNFOLDALL, "IDM_UNFOLDALL", "Unfold all lines", "Ctrl+Shift+F8" )
    ' The following are non-visual (no topmenu item)
    frmKeyBoard_AddKeyBinding( "View", IDM_VIEWNOTES, "IDM_VIEWNOTES", "Notes for this file or project", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_VIEWTODO, "IDM_VIEWTODO", "List of TODO's for this file or project", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_EXPLORER_EXPANDALL, "IDM_EXPLORER_EXPANDALL", "Expand all nodes in the Explorer window", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_EXPLORER_COLLAPSEALL, "IDM_EXPLORER_COLLAPSEALL", "Collapse all nodes in the Explorer window", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_FUNCTIONS_EXPANDALL, "IDM_FUNCTIONS_EXPANDALL", "Expand all nodes in the function List window", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_FUNCTIONS_COLLAPSEALL, "IDM_FUNCTIONS_COLLAPSEALL", "Collapse all nodes in the function List window", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_BOOKMARKS_EXPANDALL, "IDM_BOOKMARKS_EXPANDALL", "Expand all nodes in the Bookmarks window", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_BOOKMARKS_COLLAPSEALL, "IDM_BOOKMARKS_COLLAPSEALL", "Collapse all nodes in the Bookmarks window", "" )
    frmKeyBoard_AddKeyBinding( "View", IDM_CLOSEPANEL, "IDM_CLOSEPANEL", "Close left side panel", "" )

    '' PROJECT MENU
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTNEW, "IDM_PROJECTNEW", "New project", "" )
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTSAVE, "IDM_PROJECTSAVE", "Save project", "" )
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTSAVEAS, "IDM_PROJECTSAVEAS", "Save project as a different name ", "" )
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTOPEN, "IDM_PROJECTOPEN", "Open project", "" )
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTFILESADD, "IDM_PROJECTFILESADD", "Add files to project", "Ctrl+F11" )
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTCLOSE, "IDM_PROJECTCLOSE", "Close project", "" )
    frmKeyBoard_AddKeyBinding( "Project", IDM_PROJECTOPTIONS, "IDM_PROJECTOPTIONS", "Project options", "" )

    '' COMPILE MENU
    frmKeyBoard_AddKeyBinding( "Compile", IDM_BUILDEXECUTE, "IDM_BUILDEXECUTE", "Build and Execute", "F5" )
    frmKeyBoard_AddKeyBinding( "Compile", IDM_COMPILE, "IDM_COMPILE", "Compile the project or the current active file", "Ctrl+F5" )
    frmKeyBoard_AddKeyBinding( "Compile", IDM_REBUILDALL, "IDM_REBUILDALL", "Rebuild all", "Ctrl+Alt+F5" )
    frmKeyBoard_AddKeyBinding( "Compile", IDM_COMPILEMODULE, "IDM_COMPILEMODULE", "Compile Module", "Ctrl+F7" )
    frmKeyBoard_AddKeyBinding( "Compile", IDM_QUICKRUN, "IDM_QUICKRUN", "Quick Run current active file", "Ctrl+Shift+F5" )
    frmKeyBoard_AddKeyBinding( "Compile", IDM_RUNEXE, "IDM_RUNEXE", "Run executable", "Shift+F5" )
    frmKeyBoard_AddKeyBinding( "Compile", IDM_COMMANDLINE, "IDM_COMMANDLINE", "Command line", "" )

    '' DEBUG MENU
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STARTDEBUGGING, "IDM_DEBUG_STARTDEBUGGING", "Start Debugging", "F6" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STOPDEBUGGING, "IDM_DEBUG_STOPDEBUGGING", "Stop Debugging", "Shift+F6" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_PAUSE, "IDM_DEBUG_PAUSE", "Break", "Ctrl+F6" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STEPINTO, "IDM_DEBUG_STEPINTO", "Step Into", "F11" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STEPOVER, "IDM_DEBUG_STEPOVER", "Step Over", "F10" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STEPOUT, "IDM_DEBUG_STEPOUT", "Step Out", "Shift+F11" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_RUNTOCURSOR, "IDM_DEBUG_RUNTOCURSOR", "Run to Cursor", "Ctrl+F10" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_TOGGLEBREAKPOINT, "IDM_DEBUG_TOGGLEBREAKPOINT", "Toggle Breakpoint", "F9" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_DELETEALLBREAKPOINTS, "IDM_DEBUG_DELETEALLBREAKPOINTS", "Delete All Breakpoints", "Ctrl+Shift+F9" )

    '' HELP MENU
    ' The STRING id changed with the command, and that is the intended migration: an
    ' existing keybindings.ini carrying "IDM_HELP_FB:F1" no longer matches any binding and
    ' is dropped, so the new default F1 applies. A user who had rebound F1 loses that once.
    frmKeyBoard_AddKeyBinding( "Help", IDM_HELP_CENTER, "IDM_HELP_CENTER", "Help Center (searches the selection, or the word under the caret)", "F1" )
    ' NO DEFAULT KEY. F1 is the reference lookup and there is no second obvious key for
    ' this one, so it ships menu-only -- the binding exists purely so the Keyboard
    ' Shortcuts dialog can offer it, which is the only way a user can give it one.
    frmKeyBoard_AddKeyBinding( "Help", IDM_HELP_TIKO, "IDM_HELP_TIKO", "Tiko Editor Help (the editor's own documentation)", "" )

    ' Load the keybindings settings file. This will update the gKeys array with any
    ' user defined key mappings.
    dim as DWSTRING wszFilename = PsExePath & "settings\keybindings.ini"

    redim as DWSTRING lines(any)
    if TextFile_ReadLines( wszFilename, lines() ) then
        for nLine as long = lbound(lines) to ubound(lines)
            dim as DWSTRING wst = lines(nLine)
            wst = PsTrim(wst)
            if PsLen(wst) = 0 then continue for

            ' Each keybinding entry has fields separated by a colon
            if PsInStr( wst, ":" ) = 0 then continue for

            dim as DWSTRING wszMsgString = PsStrParse(wst, 1, ":")

            ' The MsgString could have an embedded (DISABLED) flag indicating that the
            ' default key combination should be disabled.
            dim as boolean bDisabled = false
            dim as long i
            i = PsInStr(wszMsgString, "(DISABLED)")
            if i then
                bDisabled = true
                wszMsgString = PsLeft(wszMsgString, i-1 )
            end if

            dim as DWSTRING wszUserKeys = PsTrim(PsStrParse(wst, 2, ":"))

            frmKeyBoard_AddKeyBinding_User( wszMsgString, wszUserKeys, bDisabled )
        next
    end if

    ' Lastly, save the keybindings settings file (we do this in case the file never existed
    ' in the first place so now we will have a default keybindings file).
    frmKeyboard_SaveKeyBindings( wszFilename )

    function = 0
end function
