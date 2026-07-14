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


#DEFINE IDC_FRMKEYBOARD_LIST1                1000
#DEFINE IDC_FRMKEYBOARD_CMDMODIFY            1001
#DEFINE IDC_FRMKEYBOARD_CMDCLEAR             1002
#DEFINE IDC_FRMKEYBOARD_LBLCONFLICT          1003
                                                
#define IDC_FRMKEYBOARDEDIT_CHKCTRL          1100
#define IDC_FRMKEYBOARDEDIT_CHKALT           1101
#define IDC_FRMKEYBOARDEDIT_CHKSHIFT         1102
#define IDC_FRMKEYBOARDEDIT_CHKDISABLE       1103
#define IDC_FRMKEYBOARDEDIT_LABEL1           1104
#define IDC_FRMKEYBOARDEDIT_LABEL2           1105
#define IDC_FRMKEYBOARDEDIT_LABEL3           1106
#define IDC_FRMKEYBOARDEDIT_COMBOACCEL       1107
#define IDC_FRMKEYBOARDEDIT_CHKDISABLED      1108

type KEYBINDINGS_TYPE
    idAction         as long         ' IDM_* message
    wszMsgString     as DWSTRING     ' "IDM_SAVE", "IDM_SAVEAS", etc 
    wszCategory      as DWSTRING
    wszDescription   as DWSTRING
    wszDefaultKeys   as DWSTRING
    wszUserKeys      as DWSTRING
    bDefaultDisabled as boolean = false
end type

dim shared gKeys(any) as KEYBINDINGS_TYPE
dim shared gKeysEdit as KEYBINDINGS_TYPE

declare function frmKeyboard_Show( byval hWndParent as HWND ) as LRESULT
declare function frmKeyboard_SaveKeyBindings( byval wszFilename as DWSTRING ) as long
declare function frmKeyBoard_AddKeyBinding ( _
            byval wszCategory as DWSTRING, _
            byval idAction as long, _
            byval wszMsgString as DWSTRING, _
            byval wszDescription as DWSTRING, _
            byval wszDefaultKeys as DWSTRING _
            ) as long
declare function frmKeyBoard_AddKeyBinding_User ( _
            byval wszMsgString as DWSTRING, _
            byval wszUserKeys as DWSTRING, _
            byval bDisabled as boolean _
            ) as long
declare function frmKeyboard_CheckForKeyConflict ( byval wszKeys as DWSTRING, byval nSkipIndex as long ) as long

