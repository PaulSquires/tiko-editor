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

'  modKeyBindings.inc  --  implementation of the keyboard shortcut model. See the header
'  of modKeyBindings.bi for what lives here and why it is not in frmKeyboard.inc.

#include once "modKeyBindings.bi"


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
    dim pStream as CTextStream
    if pStream.Create( wszFilename ) = S_OK then
        dim as DWSTRING wszLine
        for i as long = lbound(gKeys) to ubound(gKeys)
            wszLine = gKeys(i).wszMsgString

            if gKeys(i).bDefaultDisabled then
                wszLine = wszLine & "(DISABLED)"
            end if

            wszLine = wszLine & ":" & gKeys(i).wszUserKeys
            pStream.WriteLine wszLine
        next
        pStream.Close
    end if
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
    frmKeyBoard_AddKeyBinding( "File", IDM_LOADSESSION, "IDM_LOADSESSION", "Load session", "" )
    frmKeyBoard_AddKeyBinding( "File", IDM_SAVESESSION, "IDM_SAVESESSION", "Save session", "" )
    frmKeyBoard_AddKeyBinding( "File", IDM_OPTIONSDIALOG, "IDM_OPTIONSDIALOG", "Environment options", "Ctrl+," )
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

    '' SEARCH MENU
    frmKeyBoard_AddKeyBinding( "Search", IDM_SEARCHSYMBOL, "IDM_SEARCHSYMBOL", "Search Symbol", "Ctrl+P" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_FIND, "IDM_FIND", "Find", "Ctrl+F" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_FINDINPROJECT, "IDM_FINDINPROJECT", "Find in Project", "Ctrl+Shift+F" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_REPLACE, "IDM_REPLACE", "Replace", "Ctrl+H" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTODEFINITION, "IDM_GOTODEFINITION", "Goto the symbol definition", "F12" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_LASTPOSITION, "IDM_LASTPOSITION", "Goto previous file position", "Shift+F12" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTONEXTFUNCTION, "IDM_GOTONEXTFUNCTION", "Goto next sub/function", "Ctrl+PgDn" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOPREVFUNCTION, "IDM_GOTOPREVFUNCTION", "Goto previous sub/function", "Ctrl+PgUp" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKTOGGLE, "IDM_BOOKMARKTOGGLE", "Toggle bookmark", "Ctrl+F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKNEXT, "IDM_BOOKMARKNEXT", "Goto the next bookmark", "F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKPREV, "IDM_BOOKMARKPREV", "Goto the previous bookmark", "Shift+F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_BOOKMARKCLEARALL, "IDM_BOOKMARKCLEARALL", "Clear bookmarks", "Ctrl+Shift+F2" )
    frmKeyBoard_AddKeyBinding( "Search", IDM_GOTOLINE, "IDM_GOTOLINE", "Goto line", "Ctrl+G" )
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
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STEPINTO, "IDM_DEBUG_STEPINTO", "Step Into", "F11" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STEPOVER, "IDM_DEBUG_STEPOVER", "Step Over", "F10" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_STEPOUT, "IDM_DEBUG_STEPOUT", "Step Out", "Shift+F11" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_RUNTOCURSOR, "IDM_DEBUG_RUNTOCURSOR", "Run to Cursor", "Ctrl+F10" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_TOGGLEBREAKPOINT, "IDM_DEBUG_TOGGLEBREAKPOINT", "Toggle Breakpoint", "F9" )
    frmKeyBoard_AddKeyBinding( "Debug", IDM_DEBUG_DELETEALLBREAKPOINTS, "IDM_DEBUG_DELETEALLBREAKPOINTS", "Delete All Breakpoints", "Ctrl+Shift+F9" )

    '' HELP MENU
    frmKeyBoard_AddKeyBinding( "Help", IDM_HELP_FB, "IDM_HELP_FB", "FreeBasic Help (context sensitive help)", "F1" )

    ' Load the keybindings settings file. This will update the gKeys array with any
    ' user defined key mappings.
    dim as DWSTRING wszFilename = AfxGetExePathName & "settings\keybindings.ini"

    dim pStream as CTextStream

    if pStream.Open( wszFilename ) = S_OK then
        do until pStream.EOS
            dim as DWSTRING wst = pStream.ReadLine
            wst = trim(wst)
            if len(wst) = 0 then continue Do

            ' Each keybinding entry has fields separated by a colon
            if instr( wst, ":" ) = 0 then continue do

            dim as DWSTRING wszMsgString = AfxStrParse(wst, 1, ":")

            ' The MsgString could have an embedded (DISABLED) flag indicating that the
            ' default key combination should be disabled.
            dim as boolean bDisabled = false
            dim as long i
            i = instr(wszMsgString, "(DISABLED)")
            if i then
                bDisabled = true
                wszMsgString = left(wszMsgString, i-1 )
            end if

            dim as DWSTRING wszUserKeys = trim(AfxStrParse(wst, 2, ":"))

            frmKeyBoard_AddKeyBinding_User( wszMsgString, wszUserKeys, bDisabled )
        Loop
        pStream.Close
    end if

    ' Lastly, save the keybindings settings file (we do this in case the file never existed
    ' in the first place so now we will have a default keybindings file).
    frmKeyboard_SaveKeyBindings( wszFilename )

    function = 0
end function


' ========================================================================================
' Convert a virtual key string (Ctrl, Alt, Shift) to virtual key value
' ========================================================================================
function frmKeyboard_VirtKeyToValue( byval wszString as DWSTRING ) as long
    select case ucase(wszString)
        case "CTRL":  return FCONTROL
        case "ALT":   return FALT
        case "SHIFT": return FSHIFT
    end select
    function = 0
end function


' ========================================================================================
' Converts a VK_OEM key to propery virtual key because of different keyboard layouts
' ========================================================================================
function frmKeyboard_OEMtoVirtKey( byval ch as ubyte ) as long
    ' declare function VkKeyScanExW(byval ch as WCHAR, byval dwhkl as HKL) as SHORT
    dim as HKL layout = GetKeyboardLayout(0)  '// Current layout
    dim as short result = VkKeyScanEx(ch, layout)
    dim as ubyte vkCode = lobyte(result)
    dim as ubyte shiftState = hibyte(result)
    return vkCode
end function


' ========================================================================================
' Convert an accelerator key string (F1, TAB, "A", etc..) to virtual key value
'
' THIS FUNCTION IS THE AUTHORITY ON THE VOCABULARY. gKeyNames() lists the names a user may
' PICK; this decides what any name MEANS, and KeyBindings_VKToName inverts it by asking it
' rather than by carrying a second table. Some names here are deliberately NOT offered in
' gKeyNames() -- the bare "+" for instance, which would be unparseable in a "Ctrl++"
' shortcut string, and is kept only so a hand-edited keybindings.ini still loads.
' ========================================================================================
function frmKeyboard_AccelKeyToValue( byval wszString as DWSTRING ) as long
    select case ucase(wszString)
        case "BACKSPACE": return VK_BACK
        case "TAB": return VK_TAB
        case "ENTER": return VK_RETURN
        case "ESCAPE": return VK_ESCAPE
        case "SPACEBAR": return VK_SPACE
        case "PGUP": return VK_PRIOR
        case "PGDN": return VK_NEXT
        case "END": return VK_END
        case "HOME": return VK_HOME
        case "LEFT": return VK_LEFT
        case "UP": return VK_UP
        case "RIGHT": return VK_RIGHT
        case "DOWN": return VK_DOWN
        case "INS": return VK_INSERT
        case "DEL": return VK_DELETE
        case "NUMPAD0": return VK_NUMPAD0
        case "NUMPAD1": return VK_NUMPAD1
        case "NUMPAD2": return VK_NUMPAD2
        case "NUMPAD3": return VK_NUMPAD3
        case "NUMPAD4": return VK_NUMPAD4
        case "NUMPAD5": return VK_NUMPAD5
        case "NUMPAD6": return VK_NUMPAD6
        case "NUMPAD7": return VK_NUMPAD7
        case "NUMPAD8": return VK_NUMPAD8
        case "NUMPAD9": return VK_NUMPAD9
        case "NUMPAD*": return VK_MULTIPLY
        case "NUMPAD+": return VK_ADD
        case "NUMPAD-": return VK_SUBTRACT
        case "NUMPAD.": return VK_DECIMAL
        case "NUMPAD/": return VK_DIVIDE
        case "F1": return VK_F1
        case "F2": return VK_F2
        case "F3": return VK_F3
        case "F4": return VK_F4
        case "F5": return VK_F5
        case "F6": return VK_F6
        case "F7": return VK_F7
        case "F8": return VK_F8
        case "F9": return VK_F9
        case "F10": return VK_F10
        case "F11": return VK_F11
        case "F12": return VK_F12
        case "+", "PLUS": return VK_OEM_PLUS
        case ",", "COMMA": return VK_OEM_COMMA
        case "-", "MINUS": return VK_OEM_MINUS
        case ".", "PERIOD": return VK_OEM_PERIOD
        case ";", "SEMICOLON"       ' VK_OEM_1
            return frmKeyboard_OEMtoVirtKey(asc(";"))
        case ":", "COLON"           ' VK_OEM_1
            return frmKeyboard_OEMtoVirtKey(asc(":"))
        case "/", "FORWARDSLASH"    ' VK_OEM_2
            return frmKeyboard_OEMtoVirtKey(asc("/"))
        case "?", "QUESTIONMARK"    ' VK_OEM_2
            return frmKeyboard_OEMtoVirtKey(asc("?"))
        case "`", "BACKTICK"        ' VK_OEM_3
            return frmKeyboard_OEMtoVirtKey(asc("`"))
        case "~", "TILDE"           ' VK_OEM_3
            return frmKeyboard_OEMtoVirtKey(asc("~"))
        case "[", "OPENSQUAREBRACKET" ' VK_OEM_4
            return frmKeyboard_OEMtoVirtKey(asc("["))
        case "{", "OPENCURLYBRACE"    ' VK_OEM_4
            return frmKeyboard_OEMtoVirtKey(asc("{"))
        case "\", "BACKSLASH"         ' VK_OEM_5
            return frmKeyboard_OEMtoVirtKey(asc("\"))
        case "|", "PIPE"              ' VK_OEM_5
            return frmKeyboard_OEMtoVirtKey(asc("|"))
        case "]", "CLOSESQUAREBRACKET"  ' VK_OEM_6
            return frmKeyboard_OEMtoVirtKey(asc("]"))
        case "}", "CLOSECURLYBRACE"     ' VK_OEM_6
            return frmKeyboard_OEMtoVirtKey(asc("}"))
        case "'", "SINGLEQUOTE"         ' VK_OEM_7
            return frmKeyboard_OEMtoVirtKey(asc("'"))
        case chr(34), "DOUBLEQUOTE"     ' VK_OEM_7
            return frmKeyboard_OEMtoVirtKey(34)
        case "0" to "9": return asc(wszString)
        case "A" to "Z": return asc(wszString)
    end select
    function = 0
end function


' ========================================================================================
' Build the accelerator key vocabulary -- ONCE, lazily.
'
' The order below is the order every pick-list shows AND the search order
' KeyBindings_VKToName uses, so it is also what makes one spelling canonical when several
' map to the same virtual key.
'
' EVERY NAME HERE MUST BE ONE frmKeyboard_AccelKeyToValue UNDERSTANDS. That is not a style
' rule: a name it does not recognise yields virtual key 0, AddAccelerator is skipped, and
' the binding saves and displays perfectly while never firing. "PageUp"/"PageDn" sat in
' the old hardcoded combobox list in exactly that state; they are "PgUp"/"PgDn" here.
' TIKO_KEYBOARD_SELFTEST asserts the round trip for the whole table.
' ========================================================================================
sub KeyBindings_EnsureKeyNames()

    if ubound(gKeyNames) >= 0 then exit sub

    ' Sized generously, then trimmed to what was actually added.
    redim gKeyNames(255) as KEYNAME_TYPE
    dim as long n = -1

    #macro ADDKEYNAME( _name_ )
        n += 1
        gKeyNames(n).wszName = _name_
    #endmacro

    ADDKEYNAME( "Backspace" )
    ADDKEYNAME( "Tab" )
    ADDKEYNAME( "Enter" )
    ADDKEYNAME( "Escape" )
    ADDKEYNAME( "Spacebar" )
    ADDKEYNAME( "PgUp" )
    ADDKEYNAME( "PgDn" )
    ADDKEYNAME( "End" )
    ADDKEYNAME( "Home" )
    ADDKEYNAME( "Left" )
    ADDKEYNAME( "Right" )
    ADDKEYNAME( "Up" )
    ADDKEYNAME( "Down" )
    ADDKEYNAME( "DEL" )
    ADDKEYNAME( "INS" )

    for i as long = 48 to 57    ' 0 to 9
        ADDKEYNAME( wchr(i) )
    next
    for i as long = 65 to 90    ' A to Z
        ADDKEYNAME( wchr(i) )
    next

    ADDKEYNAME( "Numpad0" )
    ADDKEYNAME( "Numpad1" )
    ADDKEYNAME( "Numpad2" )
    ADDKEYNAME( "Numpad3" )
    ADDKEYNAME( "Numpad4" )
    ADDKEYNAME( "Numpad5" )
    ADDKEYNAME( "Numpad6" )
    ADDKEYNAME( "Numpad7" )
    ADDKEYNAME( "Numpad8" )
    ADDKEYNAME( "Numpad9" )
    ADDKEYNAME( "Numpad*" )
    ADDKEYNAME( "Numpad+" )
    ADDKEYNAME( "Numpad-" )
    ADDKEYNAME( "Numpad." )
    ADDKEYNAME( "Numpad/" )

    ADDKEYNAME( "F1" )
    ADDKEYNAME( "F2" )
    ADDKEYNAME( "F3" )
    ADDKEYNAME( "F4" )
    ADDKEYNAME( "F5" )
    ADDKEYNAME( "F6" )
    ADDKEYNAME( "F7" )
    ADDKEYNAME( "F8" )
    ADDKEYNAME( "F9" )
    ADDKEYNAME( "F10" )
    ADDKEYNAME( "F11" )
    ADDKEYNAME( "F12" )

    ADDKEYNAME( "Tilde" )
    ADDKEYNAME( "Plus" )
    ADDKEYNAME( "Comma" )
    ADDKEYNAME( "Minus" )
    ADDKEYNAME( "Period" )
    ADDKEYNAME( "SingleQuote" )
    ADDKEYNAME( "SemiColon" )
    ADDKEYNAME( "/" )
    ADDKEYNAME( "\" )
    ADDKEYNAME( "[" )
    ADDKEYNAME( "]" )

    redim preserve gKeyNames(n) as KEYNAME_TYPE

end sub


' ========================================================================================
' Virtual key -> its canonical name, or "" when this key has no name in the vocabulary.
'
' DERIVED, NOT WRITTEN. It asks frmKeyboard_AccelKeyToValue rather than carrying a reverse
' table, which is what makes the round trip correct BY CONSTRUCTION: a name this returns is
' by definition one that maps back to the same key. It also inherits the layout-dependent
' VkKeyScanEx handling of the OEM keys for free -- same function, same layout, same session.
'
' Linear over ~110 entries and called once per keystroke; the cost is not worth a cache.
' ========================================================================================
function KeyBindings_VKToName( byval vk as long ) as DWSTRING
    if vk = 0 then return ""
    KeyBindings_EnsureKeyNames()
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        if frmKeyboard_AccelKeyToValue( gKeyNames(i).wszName ) = vk then
            return gKeyNames(i).wszName
        end if
    next
    return ""
end function


' ========================================================================================
' A BARE PICK-LIST NAME -> virtual key, for the hosts that store a key NAME and nothing else.
'
' frmBuildConfig and frmUserTools do not persist a binding string the way gKeys() does. They
' keep the modifiers in three booleans (IsCtrl/IsAlt/IsShift) and the key itself as whatever
' text AddShortcutsToComboBox put in their combobox, so there is never a "Ctrl+" prefix to
' parse. This turns that stored text back into an accelerator, and it is deliberately NOT
' frmKeyboard_AccelKeyToValue.
'
' TWO REASONS IT HAS TO BE ITS OWN FUNCTION, and note that both produce a WRONG accelerator
' rather than a missing one -- so neither has a visible symptom:
'
'   (1) "None" IS A PICK-LIST ENTRY. AddShortcutsToComboBox prepends it, and both dialogs
'       write it straight back into gConfig as the stored key meaning "no shortcut". Handed
'       to AccelKeyToValue it does not fall through to zero: that function ends with
'             case "A" to "Z": return asc(wszString)
'       which in FreeBASIC is a LEXICOGRAPHIC RANGE over the whole string, not a test for a
'       single letter. "NONE" sorts between "A" and "Z", so asc() takes its first character
'       and every keyless build configuration would quietly claim plain N.
'
'   (2) The same range swallows any stale or hand-edited name. A config carrying "PageUp"
'       from an older tiko resolves to VK_P -- colliding with Ctrl+P (Search Symbol) --
'       instead of doing nothing.
'
' So MEMBERSHIP IN gKeyNames() is the validity test here, never "the return is non-zero". A
' name this function cannot find in the vocabulary yields 0 and no accelerator is registered,
' which is the honest outcome for a name the pick-list does not offer.
' ========================================================================================
function KeyBindings_PickListKeyToValue( byval wszString as DWSTRING ) as long
    dim as DWSTRING wszName = trim(wszString)
    if len(wszName) = 0 then return 0

    ' The affordance, not a key. TEST 7 asserts the vocabulary holds nothing by this name,
    ' so this cannot shadow a real entry.
    if ucase(wszName) = "NONE" then return 0

    KeyBindings_EnsureKeyNames()

    ' Stored text is whatever the combobox held; the callers used to normalise with
    ' ucase(trim(...)), and their existing config files may carry either spelling.
    dim as DWSTRING wszUpper = ucase(wszName)

    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        if ucase(gKeyNames(i).wszName) = wszUpper then
            return frmKeyboard_AccelKeyToValue( gKeyNames(i).wszName )
        end if
    next

    return 0
end function


' ========================================================================================
' THE VOCABULARY SELF-TEST                                   (TIKO_KEYBOARD_SELFTEST=1)
'
' Needs no window, no theme and no message pump, so it runs from frmMain at startup rather
' than when the dialog opens -- which means it covers the accelerator table the editor is
' actually running on, not just the one the dialog would build.
'
' WHAT IT IS FOR. A shortcut name that frmKeyboard_AccelKeyToValue does not understand
' yields virtual key 0, AddAccelerator is skipped, and the binding then SAVES, DISPLAYS and
' SURVIVES A RELOAD perfectly while never once firing. There is no build warning and no
' visible symptom. That is the failure this file exists to make impossible, and it was live
' in the shipped code: the old hardcoded pick-list offered "PageUp" and "PageDn" against an
' AccelKeyToValue that knows only "PGUP" and "PGDN".
'
' ON WHAT IS DELIBERATELY *NOT* ASSERTED HERE. The obvious test --
'     KeyBindings_VKToName( AccelKeyToValue(name) )  maps back to the same VK
' -- is VACUOUS. VKToName is implemented by scanning this same table for the first entry
' whose AccelKeyToValue equals the VK, so that assertion reduces to "the entry the scan
' found is the entry the scan found". It can never fail, under any breakage. It was written,
' recognised as content-free, and removed rather than left to sit green. What carries the
' actual content is TEST 1 (every offered name resolves at all) and TEST 4 (the canonical
' spelling is pinned by name, independently of the search that produced it).
' ========================================================================================
sub KeyBindings_RunSelfTest()

    if environ("TIKO_KEYBOARD_SELFTEST") <> "1" then exit sub

    KeyBindings_EnsureKeyNames()

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro KBASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? ""
    ? "--- TIKO_KEYBOARD_SELFTEST (key vocabulary) ---"

    ' ---- TEST 1: no two names in the vocabulary mean the SAME virtual key. ----
    '
    ' READ THIS BEFORE WEAKENING IT BACK. The obvious version of this test -- "every offered
    ' name resolves to a non-zero virtual key" -- was written first and is ALMOST ENTIRELY
    ' VACUOUS, which an A/B pass proved: it stayed green on the very bug it was written for.
    '
    ' The reason is that frmKeyboard_AccelKeyToValue ends with
    '       case "A" to "Z": return asc(wszString)
    ' and in FreeBASIC that is a LEXICOGRAPHIC RANGE over the whole string, not a test for a
    ' single letter. "PAGEUP" sorts between "A" and "Z", so it matches, and asc() then takes
    ' the FIRST CHARACTER: "PageUp" resolves to 80 = VK_P. Any unrecognised name beginning
    ' with a letter silently resolves to its own initial. So the old hardcoded pick-list's
    ' "PageUp"/"PageDn" were not dead entries -- picking either bound the shortcut to plain
    ' P or plain P, colliding with Ctrl+P (Search Symbol). Worse than doing nothing, and
    ' invisible to a non-zero check.
    '
    ' Collision is the property that actually holds only when the table is right: two
    ' pick-list entries that produce one accelerator mean the list is lying about what it
    ' offers. It compares distinct entries against each other, so nothing can satisfy it by
    ' construction.
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        dim as long vkI = frmKeyboard_AccelKeyToValue( gKeyNames(i).wszName )
        dim as string sName = gKeyNames(i).wszName

        KBASSERT( vkI <> 0, "vocabulary name """ & sName & """ resolves to virtual key 0" )

        for j as long = i + 1 to ubound(gKeyNames)
            dim as string sOther = gKeyNames(j).wszName
            KBASSERT( vkI <> frmKeyboard_AccelKeyToValue( gKeyNames(j).wszName ), _
                      "vocabulary names """ & sName & """ and """ & sOther & _
                      """ both resolve to virtual key " & str(vkI) )
        next
    next

    ' ---- TEST 2: no duplicate names in the vocabulary. ----
    ' A duplicate would show twice in every pick-list and make the canonical-name search
    ' order ambiguous.
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        for j as long = i + 1 to ubound(gKeyNames)
            dim as string sName = gKeyNames(i).wszName
            KBASSERT( ucase(gKeyNames(i).wszName) <> ucase(gKeyNames(j).wszName), _
                      "duplicate vocabulary name """ & sName & """" )
        next
    next

    ' ---- TEST 3: a key with no name is reported as having no name. ----
    ' This is what makes the capture field able to REFUSE a keystroke rather than store
    ' something unusable. PrintScreen, F13, the Windows key and CapsLock are all real keys a
    ' user can press and none is expressible in the persistence format.
    KBASSERT( len( KeyBindings_VKToName( 0 ) ) = 0,        "VKToName(0) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h2C ) ) = 0,     "VKToName(VK_SNAPSHOT) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h7C ) ) = 0,     "VKToName(VK_F13) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h5B ) ) = 0,     "VKToName(VK_LWIN) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h14 ) ) = 0,     "VKToName(VK_CAPITAL) should be empty" )

    ' ---- TEST 4: the CANONICAL spelling, pinned by name. ----
    ' Several names map to one key ("+" and "Plus" both give VK_OEM_PLUS), and which one
    ' comes back is decided by the ORDER of gKeyNames(). Pinning the expected string here is
    ' what makes that order a decision rather than an accident -- reorder the table, or add
    ' the bare "+" to it, and this goes red. Unlike the round-trip form, these compare
    ' against a literal the search cannot supply, so they are not self-satisfying.
    KBASSERT( KeyBindings_VKToName( VK_PRIOR )    = "PgUp",  "VK_PRIOR should spell as PgUp" )
    KBASSERT( KeyBindings_VKToName( VK_NEXT )     = "PgDn",  "VK_NEXT should spell as PgDn" )
    KBASSERT( KeyBindings_VKToName( VK_TAB )      = "Tab",   "VK_TAB should spell as Tab" )
    KBASSERT( KeyBindings_VKToName( VK_RETURN )   = "Enter", "VK_RETURN should spell as Enter" )
    KBASSERT( KeyBindings_VKToName( VK_DELETE )   = "DEL",   "VK_DELETE should spell as DEL" )
    KBASSERT( KeyBindings_VKToName( VK_OEM_PLUS ) = "Plus",  "VK_OEM_PLUS should spell as Plus, not +" )
    KBASSERT( KeyBindings_VKToName( VK_ADD )      = "Numpad+", "VK_ADD should spell as Numpad+" )
    KBASSERT( KeyBindings_VKToName( asc("A") )    = "A",     "VK_A should spell as A" )
    KBASSERT( KeyBindings_VKToName( asc("7") )    = "7",     "VK_7 should spell as 7" )
    KBASSERT( KeyBindings_VKToName( VK_F12 )      = "F12",   "VK_F12 should spell as F12" )

    ' ---- TEST 5: every SHIPPED DEFAULT binding names its key CANONICALLY. ----
    '
    ' Not "resolves to non-zero" -- see TEST 1 for why that is nearly worthless here. The
    ' check is that the default's final segment round-trips to ITSELF: resolve it to a
    ' virtual key, ask what that key is properly called, and require the two spellings to
    ' agree. A typo'd "Ctrl+PageUp" resolves to VK_P, whose canonical name is "P", and "P"
    ' <> "PAGEUP" -- so the typo is caught, where a non-zero test waves it through.
    '
    ' This is NOT the vacuous round trip described in the header. That one compares a name
    ' against whatever the search returned; this compares the search's answer against a
    ' string the search never saw -- the one an author typed into the defaults table.
    for i as long = lbound(gKeys) to ubound(gKeys)
        if len(gKeys(i).wszDefaultKeys) = 0 then continue for

        dim as long nCount = AfxStrParseCount( gKeys(i).wszDefaultKeys, "+" )
        dim as DWSTRING wszFinal = AfxStrParse( gKeys(i).wszDefaultKeys, nCount, "+" )
        dim as DWSTRING wszCanon = KeyBindings_VKToName( frmKeyboard_AccelKeyToValue( wszFinal ) )

        dim as string sMsg   = gKeys(i).wszMsgString
        dim as string sKeys  = gKeys(i).wszDefaultKeys
        dim as string sCanon = wszCanon
        KBASSERT( (len(wszCanon) > 0) andalso (ucase(wszCanon) = ucase(wszFinal)), _
                  sMsg & " default """ & sKeys & """ final key is not canonical (got """ & _
                  sCanon & """)" )

        ' Every non-final segment must be a modifier. "Ctrl+Shft+D" would otherwise build an
        ' accelerator for plain D and no one would notice until it fired at the wrong time.
        for ii as long = 1 to nCount - 1
            dim as DWSTRING wszMod = AfxStrParse( gKeys(i).wszDefaultKeys, ii, "+" )
            dim as string sMod = wszMod
            KBASSERT( frmKeyboard_VirtKeyToValue( wszMod ) <> 0, _
                      sMsg & " default """ & sKeys & """ has an unrecognised modifier """ & sMod & """" )
        next
    next

    ' ---- TEST 6: the defaults table is internally consistent. ----
    ' Duplicate IDM_ names would make keybindings.ini ambiguous on load -- AddKeyBinding_User
    ' stops at the first match, so the second entry could never be given a user key.
    for i as long = lbound(gKeys) to ubound(gKeys)
        for j as long = i + 1 to ubound(gKeys)
            dim as string sMsg = gKeys(i).wszMsgString
            KBASSERT( gKeys(i).wszMsgString <> gKeys(j).wszMsgString, _
                      "duplicate binding message string """ & sMsg & """" )
        next
    next

    ' ---- TEST 7: the pick-list as frmBuildConfig and frmUserTools actually resolve it. ----
    '
    ' Those two dialogs store a BARE key name, not a binding string, so their accelerator
    ' build calls KeyBindings_PickListKeyToValue. Each of them used to carry its own private
    ' 48-arm select case (F1-F12, 0-9, A-Z) against a pick-list offering all of gKeyNames():
    ' picking any of the other names left nKey = 0, the ACCEL entry was skipped, and the
    ' shortcut then SAVED, DISPLAYED and SURVIVED A RELOAD while never once firing. Same
    ' failure TEST 1 exists for, in a second and third copy of the vocabulary.
    '
    ' THIS IS NOT A DUPLICATE OF TEST 1. That one goes through AccelKeyToValue; this goes
    ' through the function the two dialogs call, which additionally has to reject the "None"
    ' affordance and refuse names outside the vocabulary. Replace its body with a plain
    ' AccelKeyToValue call -- the obvious "simplification" -- and the "None" and "PageUp"
    ' assertions below go red while every assertion in TEST 1 stays green.
    redim as long vkPick( lbound(gKeyNames) to ubound(gKeyNames) )

    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        dim as string sName = gKeyNames(i).wszName
        vkPick(i) = KeyBindings_PickListKeyToValue( gKeyNames(i).wszName )

        KBASSERT( vkPick(i) <> 0, _
                  "pick-list name """ & sName & """ resolves to virtual key 0" )

        ' "None" is prepended by AddShortcutsToComboBox as an affordance meaning "no key at
        ' all". If it were ever also a vocabulary entry, PickListKeyToValue's early return
        ' would shadow it and that key would become unbindable.
        KBASSERT( ucase(gKeyNames(i).wszName) <> "NONE", _
                  "vocabulary must not contain a key named ""None"" -- it is the affordance" )
    next

    ' Distinct. Two pick-list entries producing one accelerator would mean the list is lying
    ' about what it offers, and the second one silently steals the first one's key.
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        for j as long = i + 1 to ubound(gKeyNames)
            dim as string sName  = gKeyNames(i).wszName
            dim as string sOther = gKeyNames(j).wszName
            KBASSERT( vkPick(i) <> vkPick(j), _
                      "pick-list names """ & sName & """ and """ & sOther & _
                      """ both resolve to virtual key " & str(vkPick(i)) )
        next
    next

    ' The affordance and the empty cases all mean NO KEY. gConfig stores the literal "None"
    ' -- both dialogs write it into the staging copy the moment a row with no shortcut is
    ' selected -- so this is the ordinary case, not an edge one.
    KBASSERT( KeyBindings_PickListKeyToValue( "None" ) = 0, "pick-list ""None"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "NONE" ) = 0, "pick-list ""NONE"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "" ) = 0,     "pick-list """" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "   " ) = 0,  "pick-list all-blank must resolve to 0" )

    ' A name the pick-list does NOT offer must resolve to 0 -- not to its own first letter.
    ' frmKeyboard_AccelKeyToValue("PageUp") returns 80 (VK_P) because of the "A" to "Z"
    ' lexicographic range described at TEST 1; going through the vocabulary is the only thing
    ' stopping that from becoming a live accelerator on plain P. "PageUp"/"PageDn" are the
    ' real historical spellings -- they were in the hardcoded combobox list this vocabulary
    ' replaced, so a user's settings.ini can still be carrying them today.
    KBASSERT( KeyBindings_PickListKeyToValue( "PageUp" ) = 0, "stale name ""PageUp"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "PageDn" ) = 0, "stale name ""PageDn"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "Insert" ) = 0, "unoffered name ""Insert"" must resolve to 0" )

    ' Case and surrounding space, because the old private tables normalised with
    ' ucase(trim(...)) and existing config files may carry either spelling.
    KBASSERT( KeyBindings_PickListKeyToValue( "pgup" )    = VK_PRIOR, "pick-list must be case-insensitive" )
    KBASSERT( KeyBindings_PickListKeyToValue( " PgUp " )  = VK_PRIOR, "pick-list must trim surrounding space" )

    ' Four spot values spanning the ranges the old 48-arm tables could NOT reach, pinned
    ' against literals so they cannot be satisfied by whatever the lookup happens to return.
    KBASSERT( KeyBindings_PickListKeyToValue( "Tab" )     = VK_TAB,   "pick-list ""Tab"" is VK_TAB" )
    KBASSERT( KeyBindings_PickListKeyToValue( "Numpad+" ) = VK_ADD,   "pick-list ""Numpad+"" is VK_ADD" )
    KBASSERT( KeyBindings_PickListKeyToValue( "Down" )    = VK_DOWN,  "pick-list ""Down"" is VK_DOWN" )
    KBASSERT( KeyBindings_PickListKeyToValue( "F12" )     = VK_F12,   "pick-list ""F12"" is VK_F12" )

    sLine = "  vocabulary: " & str(ubound(gKeyNames) - lbound(gKeyNames) + 1) & " names, " & _
            "bindings: " & str(ubound(gKeys) - lbound(gKeys) + 1) & " commands"
    ? sLine
    sLine = "--- TIKO_KEYBOARD_SELFTEST: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine
    ? ""

end sub


' ========================================================================================
' Build the main keyboard accelerator table
' ========================================================================================
function frmKeyboard_BuildAcceleratorTable() as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit function

    ' Create mapping array that equates the string representation of the
    ' IDM_* messages to their values. We use that array we saving and
    ' restoring the keybindings from file. We can not use the literal IDM
    ' value because if we modify any of the IDM source code values then the
    ' mappings stored in the keybindings file will instantly be incorrect.

    ' Create the default keyboard bindings / shortcut mappings.
    '
    ' NOTE THAT THIS RE-READS keybindings.ini FROM DISK and rebuilds gKeys() from it. Any
    ' caller applying an edit must therefore WRITE THE FILE FIRST and call this second, or
    ' the rebuild silently reinstates whatever was on disk before the edit.
    frmKeyboard_CreateDefaultKeyBindings()

    ' Destroy any existing accelerator table
    pWindow->DestroyAcceleratorTable

    ' The following ALWAYS get created and can not be altered because they are shortcuts
    ' to activate each of the top menus.
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_F,   IDC_MENUBAR_FILE )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_E,   IDC_MENUBAR_EDIT )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_S,   IDC_MENUBAR_SEARCH )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_V,   IDC_MENUBAR_VIEW )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_P,   IDC_MENUBAR_PROJECT )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_C,   IDC_MENUBAR_COMPILE )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_H,   IDC_MENUBAR_HELP )

    ' Create the accelerator table based on the data in the gKeys() array
    for i as long = lbound(gKeys) to ubound(gKeys)

        dim as DWSTRING wszKeys = gKeys(i).wszDefaultKeys

        ' wszUserKeys will override the wszDefaultKeys entry
        if len(gKeys(i).wszUserKeys) then
            wszKeys = gKeys(i).wszUserKeys
        else
            ' Only create the default keyboard short if it has not been disabled
            if gKeys(i).bDefaultDisabled then
                wszKeys = ""
            end if
        end if

        ' No need to proceed further if no User defined shortcut and Default keys disabled
        if len(wszKeys) = 0 then continue for

        ' convert the key combination into something that AddAcelerator can understand
        ' F5
        ' TAB
        ' Alt+F4
        ' Shift+F5
        ' Ctrl+F5
        ' Ctrl+Shift+F5
        ' etc...

        ' Parse for + signs
        dim as long nCount = AfxStrParseCount( wszKeys, "+" )
        dim as long nVirtValue = FVIRTKEY
        dim as long nAccelValue = 0

        for ii as long = nCount to 1 step -1
            ' The accelerator key will always be the last parse regardless of
            ' number of items in the string
            dim as DWSTRING wszString = AfxStrParse( wszKeys, ii, "+" )
            if ii = nCount then
                nAccelValue = frmKeyboard_AccelKeyToValue( wszString )
            else
                nVirtValue = nVirtValue or frmKeyboard_VirtKeyToValue( wszString )
            end if
        next
        if nAccelValue <> 0 then   ' value can be zero if no default or user key defined
            pWindow->AddAccelerator( nVirtValue, nAccelValue, gKeys(i).idAction )
        end if
    next

    ' Add the SHIFT+INSERT paste accelerator so that the Paste menu operations
    ' will fire correctly.
    pWindow->AddAccelerator( FVIRTKEY or FSHIFT, VK_INSERT, IDM_PASTE )

    pWindow->CreateAcceleratorTable()

    function = 0
end function


' ========================================================================================
' Add shortcuts to the specified combobox.
' (1) frmKeyboardEdit
' (2) frmBuildConfig
' (3) frmUserTools
'
' The last two hand this a RAW WIN32 COMBOBOX, so the signature is fixed. "None" is
' prepended here rather than living in gKeyNames(): it is a pick-list affordance meaning
' "no key at all", not a key, and it has no virtual-key value to round-trip.
' ========================================================================================
function AddShortcutsToComboBox( byval hCombo as HWND ) as long
    KeyBindings_EnsureKeyNames()

    ComboBox_AddString( hCombo, @wstr("None") )

    dim as DWSTRING wszText
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        wszText = gKeyNames(i).wszName
        ComboBox_AddString( hCombo, wszText.sptr )
    next

    function = 0
end function
