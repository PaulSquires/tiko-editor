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

#include once "frmUserTools.bi"
#include once "clsConfig.bi"
#include once "modMRU.bi"


' ========================================================================================
' Update the UserTools items in the topmenu array
' ========================================================================================


' ========================================================================================
' Prepare and Execute a user tool
' ========================================================================================
function frmUserTools_ExecuteUserTool( byval pCompile as COMPILE_TYPE ptr, byval nToolNum as long ) as long            
            
    dim ShExecInfo  as SHELLEXECUTEINFOW  
    dim pDocMain    as clsDocument ptr 
    dim pDocCurrent as clsDocument ptr 
    
    dim wszCommand       as DWSTRING
    dim wszParameters    as DWSTRING
    dim wszProjectName   as DWSTRING
    dim wszMainSource    as DWSTRING
    dim wszExeName       as DWSTRING
    dim wszWord          as DWSTRING
    dim wszWorkingFolder as DWSTRING 
    dim sCurrentDir      as string
    
    dim nWndStyle        as long = SW_SHOWNORMAL

    ' Make sure that the incoming Tool Number is valid
    if (nToolNum < lbound(gConfig.Tools)) orelse (nToolNum > ubound(gConfig.Tools)) then exit function
        
    if gConfig.Tools(nToolNum).IsPromptRun then
        if MessageBox( HWND_FRMMAIN, L(304,"Please confirm that you wish to run the following User Tool:") + vbcrlf + _
                        gConfig.Tools(nToolNum).wszDescription, L(276,"Confirm"), _
                        MB_ICONQUESTION or MB_YESNOCANCEL ) <> IDYES then 
            exit function
        end if               
    end if

    if gConfig.Tools(nToolNum).IsMinimized then nWndStyle = SW_SHOWMINNOACTIVE

    ' Prepare the command line based on any substitute parameters
    if len(gConfig.Tools(nToolNum).wszParameters) then
        
        pDocCurrent = gTTabCtl.GetActiveDocumentPtr()
        
        ' Do the substitutions for the command line parameters
        if gApp.IsProjectActive then 
            wszProjectName = AfxStrPathname("NAMEX", gApp.ProjectFilename)
            pDocMain = gApp.GetMainDocumentPtr()
        else
            pDocMain = pDocCurrent
        end if 

        if pDocMain then wszMainSource = chr(34) + pDocMain->DiskFilename + chr(34)
            
        wszParameters = gConfig.Tools(nToolNum).wszParameters
        wszParameters = AfxStrReplace(wszParameters, "<P>", wszProjectName)
        wszParameters = AfxStrReplace(wszParameters, "<p>", wszProjectName)
        wszParameters = AfxStrReplace(wszParameters, "<S>", wszMainSource)
        wszParameters = AfxStrReplace(wszParameters, "<s>", wszMainSource)

        dim as long idxBuild = frmBuildConfig_getActiveBuildIndex()
        if (idxBuild > -1) andalso (pDocMain <> 0) then
            if pCompile <> null then wszExeName = pCompile->OutputFilename
            if len(wszExeName) = 0 then
                wszExeName = AfxStrPathname("PATH", pDocMain->DiskFilename) + _
                                AfxStrPathname("NAME", pDocMain->DiskFilename) + _
                                ".exe"
                if instr(ucase(gConfig.Builds(idxBuild).wszOptions), " -DLL") orelse _
                    instr(ucase(gConfig.Builds(idxBuild).wszOptions), " -DYLIB") then 
                    wszExeName = AfxStrPathname("PATH", pDocMain->DiskFilename) + _
                                AfxStrPathname("NAME", pDocMain->DiskFilename) + _
                                ".dll"
                end if
                if instr(ucase(gConfig.Builds(idxBuild).wszOptions), " -LIB") then 
                    wszExeName = AfxStrPathname("PATH", pDocMain->DiskFilename) + _
                                AfxStrPathname("NAME", pDocMain->DiskFilename) + _
                                ".a"
                end if              
            end if
        end if
        wszParameters = AfxStrReplace(wszParameters, "<E>", wszExeName)
        wszParameters = AfxStrReplace(wszParameters, "<e>", wszExeName)
        
        if pDocCurrent then 
            ' Determine the word underneath the cursor
            wszWord = trim(pDocCurrent->GetWord, any " ()*&^%$#@!~`:;'><?/.,-+=\|}{[]")
        end if
        wszParameters = AfxStrReplace(wszParameters, "<W>", wszWord)
        wszParameters = AfxStrReplace(wszParameters, "<w>", wszWord)
        
    end if
        
    ' Change the working folder
    sCurrentDir = curdir
    ' Is the specified working folder valid? if it is not, then maybe try same path 
    ' but use the current drive.
    wszWorkingFolder = gConfig.Tools(nToolNum).wszWorkingFolder


    if len(wszWorkingFolder) then
        ' Convert relative path to absolute path if needed.
        if AfxPathIsRelative(wszWorkingFolder) then
            wszWorkingFolder = AfxPathCombine(AfxGetExePathName, wszWorkingFolder)
        end if
        if AfxPathIsDirectory(wszWorkingFolder) = false then
            wszWorkingFolder = left(AfxGetExePathName, 1) + mid(wszWorkingFolder, 2)
        end if
        chdir gConfig.Tools(nToolNum).wszWorkingFolder
    end if
        
    ' Is the specified exe command to execute valid? if it is not, then maybe try same path 
    ' but use the current drive.
    wszCommand = gConfig.Tools(nToolNum).wszCommand 
    ' Convert relative path to absolute path if needed.
    if AfxPathIsRelative(wszCommand) then
        wszCommand = AfxPathCombine(AfxGetExePathName, wszCommand)
    end if
    if AfxPathFileExists(wszCommand) = false then
        wszCommand = left(AfxGetExePathName, 1) + mid(wszCommand, 2)
    end if
    wszCommand = wchr(34) + wszCommand + wchr(34)

    with ShExecInfo
        .cbSize       = len(SHELLEXECUTEINFOW)
        .fMask        = SEE_MASK_NOCLOSEPROCESS
        .HWnd         = 0
        .lpVerb       = null
        .lpFile       = wszCommand
        .lpParameters = wszParameters   
        .lpDirectory  = 0
        .nShow        = nWndStyle
        .hInstApp     = 0 
    end with
    ShellExecuteEx(@ShExecInfo)
        
    if gConfig.Tools(nToolNum).IsWaitFinish then
        ' Give the process 100 ms 
        while WaitForSingleObject(ShExecInfo.hProcess, 100) = WAIT_TIMEOUT
        wend
    end if   

    ' Restore the current drive/directory
    chdir sCurrentDir

    function = 0    
end Function

    
' ========================================================================================
' Create the keyboard shortcut text description.
' ========================================================================================
function createToolsMenuShortcut( byval nCtrlID as long ) as DWSTRING
    dim wszShortcut as DWSTRING

    ' Advertise a shortcut only when one is actually registered. frmUserTools_SetTextboxes
    ' writes the literal "None" into a tool with no key the moment its row is selected, and
    ' that value persists to settings.ini -- so without this the User Tools menu renders the
    ' shortcut text "NONE" (or "Ctrl+NONE") beside the tool's name.
    if KeyBindings_PickListKeyToValue( gConfig.Tools(nCtrlID).wszKey ) = 0 then return ""

    if gConfig.Tools(nCtrlID).IsCtrl then wszShortcut = "Ctrl+"
    if gConfig.Tools(nCtrlID).IsShift then wszShortcut = wszShortcut + "Shift+"
    if gConfig.Tools(nCtrlID).IsAlt then wszShortcut = wszShortcut + "Alt+"
    wszShortcut = trim(wszShortcut + ucase(trim(gConfig.Tools(nCtrlID).wszKey)), any " +")
    
    return wszShortcut
end function



' ========================================================================================
' Create an accelerator table based on selections for the user tools.
' Also adds the Tools to the top menu.
' ========================================================================================
function frmUserTools_CreateAcceleratorTable() as long
    dim NumKeys  as long
    dim nKey     as long
    dim fVirt    as long
        
    if ghAccelUserTools then DestroyAcceleratorTable(ghAccelUserTools)
    dim a(any) as ACCEL
        
    NumKeys = 0
    for y as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
            
        fVirt = FNOINVERT or FVIRTKEY
        
        if gConfig.Tools(y).IsCtrl then fVirt = fVirt or FCONTROL
        if gConfig.Tools(y).IsShift then fVirt = fVirt or FSHIFT
        if gConfig.Tools(y).IsAlt then fVirt = fVirt or FALT
            
        ' Identical to frmBuildConfig's: the tool's key is a BARE NAME picked from
        ' AddShortcutsToComboBox, and this carried the SAME private 48-arm select case
        ' (F1-F12, 0-9, A-Z) against a pick-list offering 89 names. The other 41 -- Tab,
        ' Enter, PgUp, PgDn, Home, End, the arrows, the Numpad keys, the punctuation keys --
        ' left nKey = 0, the ACCEL entry was skipped, and the tool's shortcut saved,
        ' displayed in the User Tools menu and reloaded perfectly while never firing.
        '
        ' Do NOT "simplify" this to frmKeyboard_AccelKeyToValue: that one resolves the stored
        ' "None" affordance to plain N, and any stale name to its own first letter.
        nKey = KeyBindings_PickListKeyToValue( gConfig.Tools(y).wszKey )

        if nKey > 0 then
            NumKeys = NumKeys + 1
            redim preserve a(1 To NumKeys) as ACCEL
            a(NumKeys).fVirt = fVirt
            a(NumKeys).key   = nKey
            a(NumKeys).cmd   = IDM_USERTOOLSBASE + y
        end if

    next                                    
        
    if NumKeys then
        ghAccelUserTools = CreateAcceleratorTable(cast(LPACCEL, @a(1)), NumKeys)
    end if

    function = 0
end Function


' ========================================================================================
' Load all of the Tools descriptions into the listbox
' ========================================================================================
function frmUserTools_LoadListBox( byval hParent as HWND ) as long
    dim hList1 as HWND = GetDlgItem(hParent, IDC_FRMUSERTOOLS_LSTTOOLS)

    ListBox_ResetContent(hList1)
    for i as long = lbound(gConfig.ToolsTemp) to ubound(gConfig.ToolsTemp)
        ListBox_Addstring(hList1, gConfig.ToolsTemp(i).wszDescription.sptr)
    next

    function = 0
end function


' ========================================================================================
' Swap two entries in the Tools Listbox
' ========================================================================================
function frmUserTools_SwapListBoxItems( _
            byval Item1 as long, _
            byval Item2 as long _
            ) as long
    dim as HWND hList1 = GetDlgItem( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_LSTTOOLS)
    
    ' Swap the array values
    swap gConfig.ToolsTemp(Item1), gConfig.ToolsTemp(Item2)
    
    ListBox_Replacestring(hList1, Item1, gConfig.ToolsTemp(Item1).wszDescription)
    ListBox_Replacestring(hList1, Item2, gConfig.ToolsTemp(Item2).wszDescription)

    function = 0
end function


' ========================================================================================
' Set the UserTools textboxes and options depending on what listbox entry is selected
' ========================================================================================
function frmUserTools_SetTextboxes() as long
    dim as long nCurSel = ListBox_GetCurSel(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_LSTTOOLS))
    dim as boolean fEnabled = iif( nCurSel < 0, false, true)

    AfxSetWindowText(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTTOOLNAME), iif(fEnabled, gConfig.ToolsTemp(nCurSel).wszDescription, wstr("")))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTTOOLNAME), fEnabled)
    AfxSetWindowText(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTCOMMAND), iif(fEnabled, gConfig.ToolsTemp(nCurSel).wszCommand, wstr("")))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTCOMMAND), fEnabled)
    AfxSetWindowText(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTPARAMETERS), iif(fEnabled, gConfig.ToolsTemp(nCurSel).wszParameters, wstr("")))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTPARAMETERS), fEnabled)

    dim as HWND hCombo = GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_COMBOKEY)
    if len(gConfig.ToolsTemp(nCurSel).wszKey) = 0 then gConfig.ToolsTemp(nCurSel).wszKey = "None"
    dim as long sel = ComboBox_FindStringExact( hCombo, -1, gConfig.ToolsTemp(nCurSel).wszKey.sptr )
    ComboBox_SetCurSel( hCombo, sel )
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_COMBOKEY), fEnabled)

    AfxSetWindowText(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTWORKINGFOLDER), iif(fEnabled, gConfig.ToolsTemp(nCurSel).wszWorkingFolder, wstr("")))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_TXTWORKINGFOLDER), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKCTRL), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsCtrl, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKCTRL), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKALT), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsAlt, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKALT), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKSHIFT), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsShift, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKSHIFT), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKPROMPT), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsPromptRun, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKPROMPT), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKMINIMIZED), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsMinimized, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKMINIMIZED), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKWAIT), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsWaitFinish, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKWAIT), fEnabled)
    Button_SetCheck(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKDISPLAYMENU), iif(fEnabled, gConfig.ToolsTemp(nCurSel).IsDisplayMenu, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CHKDISPLAYMENU), fEnabled)
    ComboBox_SetCurSel(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_COMBOACTION), iif(fEnabled, gConfig.ToolsTemp(nCurSel).Action, 0))
    EnableWindow(GetDlgItem(HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_COMBOACTION), fEnabled)

    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmUserTools
' ========================================================================================
function frmUserTools_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    ' This is a modal popup window so disable the parent window
    DisableAllModeless()

    '  Message cracker macro expects a True to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end Function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmUserTools
' ========================================================================================
function frmUserTools_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    dim as HWND hList1 = GetDlgItem( hwnd, IDC_FRMUSERTOOLS_LSTTOOLS)
    dim as long nCurSel = ListBox_GetCurSel(hList1)

    select case id
        case IDC_FRMUSERTOOLS_LSTTOOLS
            if codeNotify = LBN_SELCHANGE then
                frmUserTools_SetTextboxes()
            end if
            
        case IDC_FRMUSERTOOLS_TXTTOOLNAME
            if codeNotify = EN_CHANGE then
                ' Update the temp array and the Listbox
                if nCurSel > -1 then
                    gConfig.ToolsTemp(nCurSel).wszDescription = AfxGetWindowText(hwndCtl)
                    ListBox_Replacestring(hList1, nCurSel, gConfig.ToolsTemp(nCurSel).wszDescription)
                end if
            end if
            
        case IDC_FRMUSERTOOLS_TXTCOMMAND
            if codeNotify = EN_CHANGE then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).wszCommand = AfxGetWindowText(hwndCtl)
            end if
            
        case IDC_FRMUSERTOOLS_CMDBROWSEEXE
            if codeNotify = BN_CLICKED then
                dim pwszName as wstring ptr = AfxIFileOpenDialogW(hwnd, IDC_FRMUSERTOOLS_CMDBROWSEEXE)
                if pwszName then
                    AfxSetWindowText( GetDlgItem(hwnd, IDC_FRMUSERTOOLS_TXTCOMMAND), pwszName )
                    CoTaskMemFree pwszName
                end if
            end if
            
        case IDC_FRMUSERTOOLS_TXTPARAMETERS
            if codeNotify = EN_CHANGE then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).wszParameters = AfxGetWindowText(hwndCtl)
            end if

        case IDC_FRMUSERTOOLS_COMBOKEY
            if codeNotify = CBN_SELCHANGE then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).wszKey = AfxGetWindowText(hwndCtl)
            end if

        case IDC_FRMUSERTOOLS_TXTWORKINGFOLDER
            if codeNotify = EN_CHANGE then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).wszWorkingFolder = AfxGetWindowText(hwndCtl)
            end if
                    
        case IDC_FRMUSERTOOLS_CMDBROWSEFOLDER
            if codeNotify = BN_CLICKED then
                dim as DWSTRING cwsFolder = AfxBrowseForFolder(hwnd, L(293,"Working Folder:"), curdir)
                if len(cwsFolder) then
                    AfxSetWindowText( GetDlgItem(hwnd, IDC_FRMUSERTOOLS_TXTWORKINGFOLDER), cwsFolder)
                end if
            end if
            
        case IDC_FRMUSERTOOLS_CHKCTRL
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsCtrl = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CHKALT
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsAlt = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CHKSHIFT
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsShift = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CHKPROMPT
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsPromptRun = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CHKMINIMIZED
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsMinimized = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CHKWAIT
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsWaitFinish = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CHKDISPLAYMENU
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).IsDisplayMenu = Button_GetCheck(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_COMBOACTION
            if codeNotify = CBN_SELCHANGE then
                if nCurSel > -1 then gConfig.ToolsTemp(nCurSel).Action = ComboBox_GetCurSel(hwndCtl) 
            end if

        case IDC_FRMUSERTOOLS_CMDUP
            if codeNotify = BN_CLICKED then
                if nCurSel > 0 then
                    frmUserTools_SwapListboxItems(nCurSel, nCurSel-1)
                end if
            end if

        case IDC_FRMUSERTOOLS_CMDDOWN
            if codeNotify = BN_CLICKED then
                if nCurSel < ListBox_GetCount(hList1)-1 then
                    frmUserTools_SwapListboxItems(nCurSel, nCurSel+1)
                end if
            end if

        case IDC_FRMUSERTOOLS_CMDINSERT
            if codeNotify = BN_CLICKED then
                if ubound(gConfig.ToolsTemp) = -1 then
                    redim gConfig.ToolsTemp(0)
                    nCurSel = 0
                Else
                    redim preserve gConfig.ToolsTemp(ubound(gConfig.ToolsTemp)+1)
                    if nCurSel = -1 then nCurSel = 0
                    ' insert the item above current entry in the internal array
                    for i as long = ubound(gConfig.ToolsTemp) to nCurSel + 1 step -1
                        gConfig.ToolsTemp(i) = gConfig.ToolsTemp(i-1)
                    next
                end if
                gConfig.ToolsTemp(nCurSel).wszDescription   = ""
                gConfig.ToolsTemp(nCurSel).wszCommand       = ""
                gConfig.ToolsTemp(nCurSel).wszParameters    = ""
                gConfig.ToolsTemp(nCurSel).wszKey           = ""
                gConfig.ToolsTemp(nCurSel).wszWorkingFolder = ""
                gConfig.ToolsTemp(nCurSel).IsCtrl           = 0
                gConfig.ToolsTemp(nCurSel).IsAlt            = 0
                gConfig.ToolsTemp(nCurSel).IsShift          = 0
                gConfig.ToolsTemp(nCurSel).IsPromptRun      = 0
                gConfig.ToolsTemp(nCurSel).IsMinimized      = 0
                gConfig.ToolsTemp(nCurSel).IsWaitFinish     = 0
                gConfig.ToolsTemp(nCurSel).IsDisplayMenu    = 0
                ' reload the listbox
                frmUserTools_LoadListBox(hwnd)
                nCurSel = Min(nCurSel, ubound(gConfig.ToolsTemp))
                ListBox_SetCurSel(hList1, nCurSel)
                frmUserTools_SetTextboxes()
                SetFocus GetDlgItem(hwnd, IDC_FRMUSERTOOLS_TXTTOOLNAME)
            end if

        case IDC_FRMUSERTOOLS_CMDDELETE
            if codeNotify = BN_CLICKED then
                if nCurSel > -1 then
                    if MessageBox( hwnd, L(300, "Are you sure you want to delete this user tool?"), L(276,"Confirm"), _
                                        MB_YESNOCANCEL or MB_ICONINFORMATION or MB_DEFBUTTON1 ) = IDYES then
                        if ubound(gConfig.ToolsTemp) = 0 then
                            erase gConfig.ToolsTemp
                            nCurSel = -1
                        else
                            ' remove the item from the internal array
                            for i as long = nCurSel to ubound(gConfig.ToolsTemp) - 1
                                gConfig.ToolsTemp(i) = gConfig.ToolsTemp(i+1)
                            next
                            redim preserve gConfig.ToolsTemp(ubound(gConfig.ToolsTemp)-1)
                        end if
                        ' reload the listbox
                        frmUserTools_LoadListBox(hwnd)
                        nCurSel = Min(nCurSel, ubound(gConfig.ToolsTemp))
                        ListBox_SetCurSel(hList1, nCurSel)
                        frmUserTools_SetTextboxes()
                        SetFocus hList1
                    end if
                end if
            end if

        case IDC_FRMUSERTOOLS_CMDOK    
            if codeNotify = BN_CLICKED then
                ' Copy the temporary items to the main array  
                redim gConfig.Tools(ubound(gConfig.ToolsTemp))
                for i as long = lbound(gConfig.ToolsTemp) to ubound(gConfig.ToolsTemp)
                    gConfig.Tools(i) = gConfig.ToolsTemp(i)   
                next
                erase gConfig.ToolsTemp
                frmUserTools_CreateAcceleratorTable
                gConfig.SaveConfigFile
                ' (no menu refresh needed: the User Tools submenu rebuilds itself
                '  from gConfig.Tools every time it opens)
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
                
        case IDCANCEL
            if codeNotify = BN_CLICKED then
                SendMessage( HWnd, WM_CLOSE, 0, 0 )
                exit function
            end if
    end Select

    function = 0
end Function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmUserTools
' ========================================================================================
function frmUserTools_OnClose( byval hwnd as HWND ) as LRESULT
    ' Enables parent window keeping parent's zorder
    EnableAllModeless()
    DestroyWindow( hwnd )
    function = 0
end Function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmUserTools
' ========================================================================================
function frmUserTools_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage(0)
    function = 0
end Function


' ========================================================================================
' frmUserTools Window procedure
' ========================================================================================
function frmUserTools_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,   frmUserTools_OnCreate)
        HANDLE_MSG (hwnd, WM_DESTROY,  frmUserTools_OnDestroy)
        HANDLE_MSG (hwnd, WM_CLOSE,    frmUserTools_OnClose)
        HANDLE_MSG (hwnd, WM_COMMAND,  frmUserTools_OnCommand)
    end Select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmUserTools_Show
' ========================================================================================
function frmUserTools_Show( byval hWndParent as HWND ) as LRESULT
    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    dim as HFONT hFontSymbolLarge = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    dim as HWND hForm = _
    pWindow->Create(hWndParent, L(289,"User Tools"), _
        @frmUserTools_WndProc, 0, 0, 683, 465, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetClientSize(677, 436)
    pWindow->Center

    pWindow->AddControl("LISTBOX", , IDC_FRMUSERTOOLS_LSTTOOLS, "", 9, 10, 218, 362, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or LBS_NOTIFY or LBS_NOINTEGRALHEIGHT, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDINSERT, L(281, "Insert"), 8, 388, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDDELETE, L(282, "Delete"), 87, 388, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    
    dim as HWND hCtrl = _
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDUP, wszIconChevronUp, 166, 388, 28, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    AfxSetWindowFont( hCtrl, hFontSymbolLarge, false )     

    hCtrl = _
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDDOWN, wszIconChevronDown, 199, 388, 28, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    AfxSetWindowFont( hCtrl, hFontSymbolLarge, false )     

    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL1, L(290,"Tool Name") & ":", 230, 25, 95, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_RIGHT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("TEXTBOX", , IDC_FRMUSERTOOLS_TXTTOOLNAME, "", 334, 22, 305, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL2, L(291,"Command") & ":", 230, 50, 95, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_RIGHT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("TEXTBOX", , IDC_FRMUSERTOOLS_TXTCOMMAND, "", 334, 47, 267, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDBROWSEEXE, "...", 610, 46, 30, 23, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL3, L(292,"Parameters"), 230, 75, 95, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_RIGHT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("TEXTBOX", , IDC_FRMUSERTOOLS_TXTPARAMETERS, "", 334, 72, 305, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL4, "<P> Project Name  <S> Main Source File  <W> Current Word  <E> Exe/DLL Name", 334, 101, 302, 36, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL5, L(293,"Working Folder") & ":", 230, 177, 95, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_RIGHT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("TEXTBOX", , IDC_FRMUSERTOOLS_TXTWORKINGFOLDER, "", 334, 174, 267, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDBROWSEFOLDER, "...", 610, 173, 30, 23, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL6, L(294,"Accelerator") & ":", 230, 205, 95, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_RIGHT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKCTRL, "Ctrl", 335, 202, 48, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKALT, "Alt", 391, 202, 42, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKSHIFT, "Shift", 441, 202, 48, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    dim as HWND hCombo 
    hCombo = pWindow->AddControl("COMBOBOX", , IDC_FRMUSERTOOLS_COMBOKEY, "", 498, 201, 103, 200, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or CBS_DROPDOWNLIST, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    AddShortcutsToComboBox( hCombo )

    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKPROMPT, L(295,"Prompt for confirmation when Tool is invoked"), _
        250, 267, 420, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKMINIMIZED, L(296,"Run minimized"), _
        250, 289, 420, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKWAIT, L(297,"Wait for Tool to complete before continuing"), _
        250, 311, 420, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMUSERTOOLS_CHKDISPLAYMENU, L(299,"Display this item in the editor menu"), _
        250, 333, 420, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    
    pWindow->AddControl("COMBOBOX", , IDC_FRMUSERTOOLS_COMBOACTION, "", 334, 228, 308, 22, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or CBS_DROPDOWNLIST, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL8, L(298,"Action:"), 230, 232, 95, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_RIGHT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("LABEL", , IDC_FRMUSERTOOLS_LABEL9, "", 238, 370, 425, 2, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT or SS_NOTIFY or SS_SUNKEN, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDC_FRMUSERTOOLS_CMDOK, L(0,"OK"), 509, 388, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDCANCEL, L(1,"Cancel"), 591, 388, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)

    HWND_FRMUSERTOOLS = hForm

    ' Copy all of the Tools to the ToolsTemp array because we will work with 
    ' temporary copies until the user hits OK.
    redim gConfig.ToolsTemp(ubound(gConfig.Tools))
    for i as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
        gConfig.ToolsTemp(i) = gConfig.Tools(i)   
    next
    frmUserTools_LoadListBox(hForm)

    ' Load the Actions combobox
    hCombo = GetDlgItem(hForm, IDC_FRMUSERTOOLS_COMBOACTION)
    ComboBox_Addstring( hCombo, @L(303,"Invoke only when selected by user") ) 
    ComboBox_Addstring( hCombo, @L(302,"Invoke during pre-compile") ) 
    ComboBox_Addstring( hCombo, @L(301,"Invoke during post-compile") ) 
    ComboBox_Addstring( hCombo, @L(402,"Invoke immediately after editor starts") ) 
    
    dim hList as HWND = GetDlgItem(hForm, IDC_FRMUSERTOOLS_LSTTOOLS)
    ListBox_SetCurSel(hList, 0)
    frmUserTools_SetTextboxes()
    SetFocus GetDlgItem(hForm, IDC_FRMUSERTOOLS_LSTTOOLS)

    ' Process Windows messages
    function = pWindow->DoEvents(SW_SHOW)
    
    ' Delete the frmUserTools CWindow class manually allocated memory 
    if pWindow then delete(pWindow)
    if hFontSymbolLarge then DeleteObject(hFontSymbolLarge)

    function = 0
end function

