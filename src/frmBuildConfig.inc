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

#include once "frmBuildConfig.bi"
#include once "clsConfig.bi"
#include once "clsTopTabCtl.bi"
#include once "clsDocument.bi"

type BUILD_OPTIONS
    groupid as long
    wszText as DWSTRING
end type

' Create an array that holds all options in the Build compiler options listbox. The
' description contains a parenthesis enclosed action.
dim shared gBuildOptions(25) as BUILD_OPTIONS
gBuildOptions(0).groupid  = 0
gBuildOptions(0).wszText  = "Subsystem to console (-s console)"
gBuildOptions(1).groupid  = 0
gBuildOptions(1).wszText  = "Subsystem to GUI (-s gui)"

gBuildOptions(2).groupid  = 1
gBuildOptions(2).wszText  = "Compiler 32-bit assembler backend (-gen gas)"
gBuildOptions(3).groupid  = 1
gBuildOptions(3).wszText  = "Compiler 64-bit assembler backend (-gen gas64)"
gBuildOptions(4).groupid  = 1
gBuildOptions(4).wszText  = "Compiler GCC backend (-gen gcc)"
gBuildOptions(5).groupid  = 1
gBuildOptions(5).wszText  = "Compiler LLVM backend (-gen llvm)"

gBuildOptions(6).groupid  = 2
gBuildOptions(6).wszText  = "Add error checking (-e)"
gBuildOptions(7).groupid  = 2
gBuildOptions(7).wszText  = "Add error checking with RESUME support (-ex)"
gBuildOptions(8).groupid  = 2
gBuildOptions(8).wszText  = "Same as -ex with array bounds and null pointer (-exx)"

gBuildOptions(9).groupid  = 3
gBuildOptions(9).wszText  = "Enable Assert and AssertWarn code generation (-eassert)"

gBuildOptions(10).groupid = 4
gBuildOptions(10).wszText = "Add debug information (-g)"

gBuildOptions(11).groupid = 5
gBuildOptions(11).wszText = "Optimization level 1 for gcc (-Wc -O1)"
gBuildOptions(12).groupid = 5
gBuildOptions(12).wszText = "Optimization level 2 for gcc (-Wc -O2)"
gBuildOptions(13).groupid = 5
gBuildOptions(13).wszText = "Optimization level 3 for gcc (-Wc -O3)"
gBuildOptions(14).groupid = 5
gBuildOptions(14).wszText = "Optimization favor size for gcc (-Wc -Os)"
gBuildOptions(15).groupid = 5
gBuildOptions(15).wszText = "Optimization favor speed for gcc (-Wc -Ofast)"

gBuildOptions(16).groupid = 6
gBuildOptions(16).wszText = "Language compatibility FreeBasic (-lang fb)"
gBuildOptions(17).groupid = 6
gBuildOptions(17).wszText = "Language compatibility FreeBasic Lite (-lang fblite)"
gBuildOptions(18).groupid = 6
gBuildOptions(18).wszText = "Language compatibility QuickBasic (-lang qb)"

gBuildOptions(19).groupid = 7
gBuildOptions(19).wszText = "Create DLL and import library (-dll)"
gBuildOptions(20).groupid = 7
gBuildOptions(20).wszText = "Create static library (-lib)"

gBuildOptions(21).groupid = 8
gBuildOptions(21).wszText = "Compile only, do not link (-c)"
gBuildOptions(22).groupid = 9
gBuildOptions(22).wszText = "Do not delete the object files (-C)"
gBuildOptions(23).groupid = 10
gBuildOptions(23).wszText = "Emit preprocessed output, do not compile (-pp)"

gBuildOptions(24).groupid = 11
gBuildOptions(24).wszText = "Compile into *.asm / *.c / *.ll files only, do not assemble or link (-r)"
gBuildOptions(25).groupid = 11
gBuildOptions(25).wszText = "Preserve generated intermediate *.asm / *.c / *.ll files (-R)"


' ========================================================================================
' Load all of the build descriptions into the listbox
' ========================================================================================
function frmBuildConfig_LoadBuildListBox( byval hParent as HWND ) as Long
    dim hList1 as HWND = GetDlgItem(hParent, IDC_FRMBUILDCONFIG_LIST1)

    ListBox_ResetContent(hList1)
    for i as long = lbound(gConfig.BuildsTemp) to ubound(gConfig.BuildsTemp)
        ListBox_AddString(hList1, gConfig.BuildsTemp(i).wszDescription.sptr)
    NEXT

    function = 0
end function


' ========================================================================================
' Determine the current active build index
' ========================================================================================
function frmBuildConfig_getActiveBuildIndex() as long
    ' determine the default build in case no other build overrides it
    dim as long nDefault
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).IsDefault then
            nDefault = i: exit for
        end if
    next

    ' determine if a project or previous config selection overrides the default
    dim as long idx = -1
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gApp.IsProjectActive then
            if gConfig.Builds(i).id = gApp.ProjectBuild then
                idx = i: exit for
            end if
        else
            if gConfig.CompilerBuild = gConfig.Builds(i).id then
                idx = i: exit for
            end if
        end if
    next

    if idx = -1 then idx = nDefault
    function = idx
end function


' ========================================================================================
' Return the string description of the currently selected build configuration
' ========================================================================================
function frmBuildConfig_GetSelectedBuildDescription() as DWSTRING
    dim as long nCurSel = frmBuildConfig_getActiveBuildIndex()
    if nCurSel > -1 then
        function = gConfig.Builds(nCurSel).wszDescription
    end if
end function


' ========================================================================================
' Return the string GUID of the currently selected build configuration
' ========================================================================================
function frmBuildConfig_GetSelectedBuildGUID() as string
    dim as long nCurSel = frmBuildConfig_getActiveBuildIndex()
    if nCurSel > -1 then
        function = gConfig.Builds(nCurSel).Id
    end if
end function


' ========================================================================================
' Return the string GUID of the default (if any) build configuration
' ========================================================================================
function frmBuildConfig_GetDefaultBuildGUID() as string
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).IsDefault then 
            return gConfig.Builds(i).Id
        end if   
    next
    function = ""
end function


' ========================================================================================
' Swap two entries in the build Listbox
' ========================================================================================
function frmBuildConfig_SwapListBoxItems( _
            byval Item1 as long, _
            byval Item2 as long _
            ) as long
    dim as hwnd hList1 = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_LIST1)
    
    ' Swap the array values
    swap gConfig.BuildsTemp(Item1), gConfig.BuildsTemp(Item2)
    
    ListBox_ReplaceString(hList1, Item1, gConfig.BuildsTemp(Item1).wszDescription)
    ListBox_ReplaceString(hList1, Item2, gConfig.BuildsTemp(Item2).wszDescription)

    function = 0
end function


' ========================================================================================
' Set the build description and compiler options depending on what listbox entry is selected
' ========================================================================================
function frmBuildConfig_SetBuildConfigTextboxes() as long
    dim as HWND hList1   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_LIST1)
    dim as HWND hList2   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_LSTOPTIONS)
    dim as HWND hCheck   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CHKISDEFAULT)
    dim as HWND hText1   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TXTDESCRIPTION)
    dim as HWND hText2   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TXTOPTIONS)
    dim as HWND hOpt32   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_OPT32)
    dim as HWND hOpt64   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_OPT64)
    dim as HWND hExcl    = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CHKEXCLUDEPOPUP)
    dim as HWND hIsCtrl  = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CHKCTRL)
    dim as HWND hIsAlt   = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CHKALT)
    dim as HWND hIsShift = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CHKSHIFT)
    dim as HWND hKey     = GetDlgItem( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_COMBOKEY)
    
    dim as long nCurSel = ListBox_GetCurSel(hList1)
    if nCurSel < 0 then 
        AfxSetWindowText( hText1, "")
        AfxSetWindowText( hText2, "")
        Button_SetCheck( hCheck, 0)
        CheckRadioButton(HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_OPT32, IDC_FRMBUILDCONFIG_OPT64, IDC_FRMBUILDCONFIG_OPT32)
        EnableWindow(hText1, false)
        EnableWindow(hText2, false)
        EnableWindow(hCheck, false)
        EnableWindow(hOpt32, false)
        EnableWindow(hOpt64, false)
        EnableWindow(hList2, false)
        EnableWindow(hExcl, false)
        EnableWindow(hIsCtrl, false)
        EnableWindow(hIsAlt, false)
        EnableWindow(hIsShift, false)
        EnableWindow(hKey, false)
    else
        AfxSetWindowText( hText1, gConfig.BuildsTemp(nCurSel).wszDescription)
        AfxSetWindowText( hText2, gConfig.BuildsTemp(nCurSel).wszOptions)
        Button_SetCheck( hCheck, gConfig.BuildsTemp(nCurSel).IsDefault)
        Button_SetCheck( hExcl, gConfig.BuildsTemp(nCurSel).ExcludeInPopup)

        Button_SetCheck( hIsCtrl, gConfig.BuildsTemp(nCurSel).IsCtrl)
        Button_SetCheck( hIsAlt, gConfig.BuildsTemp(nCurSel).IsAlt)
        Button_SetCheck( hIsShift, gConfig.BuildsTemp(nCurSel).IsShift)
        if len(gConfig.BuildsTemp(nCurSel).wszKey) = 0 then gConfig.BuildsTemp(nCurSel).wszKey = "None"
        dim as long sel = ComboBox_FindStringExact( hKey, -1, gConfig.BuildsTemp(nCurSel).wszKey.sptr )
        ComboBox_SetCurSel( hKey, sel )

        if gConfig.BuildsTemp(nCurSel).Is32bit then
            CheckRadioButton(HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_OPT32, IDC_FRMBUILDCONFIG_OPT64, IDC_FRMBUILDCONFIG_OPT32)
        else   
            CheckRadioButton(HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_OPT32, IDC_FRMBUILDCONFIG_OPT64, IDC_FRMBUILDCONFIG_OPT64)
        END IF

        EnableWindow(hText1, true)
        EnableWindow(hText2, true)
        EnableWindow(hCheck, true)
        EnableWindow(hOpt32, true)
        EnableWindow(hOpt64, true)
        EnableWindow(hList2, true)
        EnableWindow(hExcl, true)
        EnableWindow(hIsCtrl, true)
        EnableWindow(hIsAlt, true)
        EnableWindow(hIsShift, true)
        EnableWindow(hKey, true)
    end if

    ' Set the checkmarks in the OptionList Listbox by redrawing the Listbox
    AfxRedrawWindow(hList2)
        
    function = 0
end function
                

' ========================================================================================
' Create an accelerator table based on selections for the Build Configurations.
' ========================================================================================
function frmBuildConfig_CreateAcceleratorTable() as long
    dim NumKeys  as long
    dim nKey     as long
    dim fVirt    as long
        
    if ghAccelBuildConfigurations then DestroyAcceleratorTable(ghAccelBuildConfigurations)
    dim a(any) as ACCEL
        
    NumKeys = 0
    for y as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
            
        fVirt = FNOINVERT or FVIRTKEY
        
        if gConfig.Builds(y).IsCtrl then fVirt = fVirt or FCONTROL
        if gConfig.Builds(y).IsShift then fVirt = fVirt or FSHIFT
        if gConfig.Builds(y).IsAlt then fVirt = fVirt or FALT
            
        nKey = 0   ' important to reset
        select case ucase(trim(gConfig.Builds(y).wszKey))
            case "F1":  nKey = VK_F1
            case "F2":  nKey = VK_F2
            case "F3":  nKey = VK_F3
            case "F4":  nKey = VK_F4
            case "F5":  nKey = VK_F5
            case "F6":  nKey = VK_F6
            case "F7":  nKey = VK_F7
            case "F8":  nKey = VK_F8
            case "F9":  nKey = VK_F9
            case "F10": nKey = VK_F10
            case "F11": nKey = VK_F11
            case "F12": nKey = VK_F12
            case "0":   nKey = VK_0
            case "1":   nKey = VK_1
            case "2":   nKey = VK_2
            case "3":   nKey = VK_3
            case "4":   nKey = VK_4
            case "5":   nKey = VK_5
            case "6":   nKey = VK_6
            case "7":   nKey = VK_7
            case "8":   nKey = VK_8
            case "9":   nKey = VK_9
            case "A":   nKey = VK_A
            case "B":   nKey = VK_B
            case "C":   nKey = VK_C
            case "D":   nKey = VK_D
            case "E":   nKey = VK_E
            case "F":   nKey = VK_F
            case "G":   nKey = VK_G
            case "H":   nKey = VK_H
            case "I":   nKey = VK_I
            case "J":   nKey = VK_J
            case "K":   nKey = VK_K
            case "L":   nKey = VK_L
            case "M":   nKey = VK_M
            case "N":   nKey = VK_N
            case "O":   nKey = VK_O
            case "P":   nKey = VK_P  
            case "Q":   nKey = VK_Q  
            case "R":   nKey = VK_R  
            case "S":   nKey = VK_S  
            case "T":   nKey = VK_T  
            case "U":   nKey = VK_U  
            case "V":   nKey = VK_V  
            case "W":   nKey = VK_W
            case "X":   nKey = VK_X
            case "Y":   nKey = VK_Y
            case "Z":   nKey = VK_Z  
        end select
            
        if nKey > 0 then
            NumKeys = NumKeys + 1
            redim preserve a(1 To NumKeys) as ACCEL
            a(NumKeys).fVirt = fVirt
            a(NumKeys).key   = nKey
            a(NumKeys).cmd   = IDM_BUILDCONFIGBASE + y
        end if

    next                                    
        
    if NumKeys then
        ghAccelBuildConfigurations = CreateAcceleratorTable(cast(LPACCEL, @a(1)), NumKeys)
    end if

    function = 0
end Function


' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: frmBuildConfig
' ========================================================================================
function frmBuildConfig_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as long
    ' Set the height of the List box items. 
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then 
        lpmis->itemHeight = pWindow->ScaleY(FRMBUILDCONFIG_LISTBOX_LINEHEIGHT)
    end if
    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: frmBuildConfig
' ========================================================================================
function frmBuildConfig_OnDrawItem( _
            byval hwnd as HWND, _
            byval lpdis as const DRAWITEMSTRUCT ptr _
            ) as long

    dim as RECT rc
    dim as DWSTRING wszText, wszOptions, wszAction
    dim as boolean IsChecked = false
    
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit function
        
    if lpdis->itemID = -1 then exit function
    
    select case lpdis->itemAction
        case ODA_DRAWENTIRE, ODA_SELECT
            
            dim as long nSelBuild = ListBox_GetCurSel(GetDlgItem(hwnd, IDC_FRMBUILDCONFIG_LIST1))
            dim as long nSelOptions = ListBox_GetCurSel(GetDlgItem(hwnd, IDC_FRMBUILDCONFIG_LSTOPTIONS))
            
            if lpdis->CtlID = IDC_FRMBUILDCONFIG_LIST1 then
                wszText = gConfig.BuildsTemp(lpdis->itemID).wszDescription
            elseif lpdis->CtlID = IDC_FRMBUILDCONFIG_LSTOPTIONS then
                wszText = gBuildOptions(lpdis->itemID).wszText
                ' Get the selected item in the Builds listbox and then look at the OptionList for
                ' that build for each of the individual OptionList build options.
                if nSelBuild > -1 then 
                    wszOptions = " " & gConfig.BuildsTemp(nSelBuild).wszOptions & " "
                    wszAction  = " " & AfxStrExtract(gBuildOptions(lpdis->itemID).wszText, "(", ")") & " "
                    IsChecked = iif( instr(wszOptions, wszAction), true, false)
                end if
            end if
            
            dim as PsBufferPaint b

            b.BeginDoubleBuffer(lpdis->hwndItem, lpdis->hDC, lpdis->rcItem)
                
            dim as boolean IsHot = false
            if lpdis->CtlID = IDC_FRMBUILDCONFIG_LIST1 then
                if lpdis->itemID = nSelBuild then IsHot = true
            elseif lpdis->CtlID = IDC_FRMBUILDCONFIG_LSTOPTIONS then
                if lpdis->itemID = nSelOptions then IsHot = true
            end if
            
            dim as COLORREF backclr = iif( IsHot, GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_WINDOW) )
            dim as COLORREF foreclr = iif( IsHot, GetSysColor(COLOR_HIGHLIGHTTEXT), GetSysColor(COLOR_WINDOWTEXT) )
            b.SetForeColor( foreclr )
            b.SetBackColor( backclr )

            ' Create our rect that works with the entire line
            SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
            dim as RECT rcText = rc 
            dim as RECT rcBitmap = rc 
        
            ' Paint the entire background
            b.PaintRect( @rc )

            if lpdis->CtlID = IDC_FRMBUILDCONFIG_LIST1 then
                rcText.left += pWindow->ScaleX(10)
            elseif lpdis->CtlID = IDC_FRMBUILDCONFIG_LSTOPTIONS then
                b.SetFont( ghFont(SYMBOLFONT_9) )
                rcBitmap.right = rcBitmap.left + pWindow->ScaleX(20)
                dim as DWSTRING wszBitmap = iif(IsChecked, wszIconCheckBoxMarked, wszIconCheckBoxEmpty) 
                b.PaintText( wszBitmap, @rcBitmap, DT_CENTER )
                rcText.left = rcBitmap.right
            end if
            
            b.SetFont( ghFont(GUIFONT_9) )
            b.PaintText( wszText, @rcText, DT_LEFT )
            
            b.EndDoubleBuffer()
    
            return true
    
    end select

    function = 0
end function


' ========================================================================================
' Processes messages for the subclassed ListBox window.
' ========================================================================================
function frmBuildConfig_ProcessListboxClick( _
            byval hwnd as HWND, _
            byval nCurSel as long _
            ) as long
    if nCurSel = -1 then exit function
    
    ' Get the action text for this listbox item. 
    dim as DWSTRING wszText = AfxGetWindowText( GetDlgItem(HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TXTOPTIONS)) & " "
    dim as DWSTRING wszAction = AfxStrExtract(gBuildOptions(nCurSel).wszText, "(", ")") & " "
    dim as long groupid = gBuildOptions(nCurSel).groupid
    dim as boolean doAddAction = false
    
    ' If the action does not exist then set flag to add it after all other items in 
    ' the group have been removed.
    if instr(wszText, wszAction) = 0 then doAddAction = true

    ' Remove all actions with same groupid.
    for i as long = lbound(gBuildOptions) to ubound(gBuildOptions)
        if gBuildOptions(i).groupid = groupid then
            dim as DWSTRING wszRemove = AfxStrExtract(gBuildOptions(i).wszText, "(", ")") & " "
            wszText = AfxStrRemove(wszText, wszRemove)
        end if
    next

    if doAddAction then 
        wszText = trim(wszText) & " " & trim(wszAction)
    end if   
    
    AfxSetWindowText( GetDlgItem(HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TXTOPTIONS), wszText)
    AfxRedrawWindow( GetDlgItem(HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_LSTOPTIONS) )

    function = 0

end function


' ========================================================================================
' Empty client area of a plain owner-drawn system LISTBOX: the strip from below the last
' visible row to the bottom edge. Used by the subclass WM_ERASEBKGND below to fill just that
' gap (flicker reduction). This was an unprefixed helper in the old PsListTree.inc, back when
' that control wrapped a system listbox; the owner-drawn PsListTree no longer does and dropped
' it, so it lives here now -- its one remaining caller -- operating on frmBuildConfig's own
' raw LISTBOX.
' ========================================================================================
function GetListBoxEmptyClientArea( byval hListBox as HWND ) as RECT
    dim as RECT rc: GetClientRect( hListBox, @rc )
    ' Row height from LB_GETITEMHEIGHT is valid even with zero items (fixed-height
    ' owner-draw), unlike LB_GETITEMRECT(0) which fails when item 0 doesn't exist.
    dim as long itemHeight = SendMessage( hListBox, LB_GETITEMHEIGHT, 0, 0 )
    dim as long NumItems   = ListBox_GetCount(hListBox)
    dim as long nTopIndex  = SendMessage( hListBox, LB_GETTOPINDEX, 0, 0 )
    dim as long visible_rows = 0

    if (NumItems > 0) andalso (itemHeight > 0) then
        dim as long ItemsPerPage = (rc.bottom - rc.top) \ itemHeight
        dim as long bottom_index = nTopIndex + ItemsPerPage
        if bottom_index >= NumItems then bottom_index = NumItems - 1
        visible_rows = (bottom_index - nTopIndex) + 1
    end if

    rc.top = visible_rows * itemHeight
    if rc.top > rc.bottom then rc.top = rc.bottom

    return rc
end function


' ========================================================================================
' Processes messages for the subclassed ListBox window.
' ========================================================================================
function frmBuildConfig_ListBox_SubclassProc ( _
            byval hwnd   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval wParam as WPARAM, _               ' // First message parameter
            byval lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    
    select case uMsg

        case WM_ERASEBKGND
            ' Only erase the bottom portion of the listbox that extends from the last item
            ' to the bottom edge of the listbox. All other lines are already drawn. This helps
            ' reduce screen flicker.
            dim as RECT rc = GetListBoxEmptyClientArea( HWND )
            if rc.top < rc.bottom then
                dim as HDC hDC = cast(HDC, wParam)
                FillRect(hDC, @rc, GetSysColorBrush(COLOR_WINDOW))
            end if
            return true
            
        case WM_GETDLGCODE
            ' All keyboard input
            function = DLGC_WANTALLKEYS
            exit function

        case WM_KEYUP
            select case loword(wParam)
            case VK_SPACE
                if uIdSubclass = IDC_FRMBUILDCONFIG_LSTOPTIONS then
                    dim as long nCurSel = ListBox_GetCurSel(hwnd)
                    frmBuildConfig_ProcessListboxClick(hwnd, nCurSel)
                    exit function
                end if    
            case VK_ESCAPE
                PostMessage( GetParent(hwnd), WM_CLOSE, 0, 0 )
                exit function
            end select

        case WM_MOUSEMOVE
        case WM_LBUTTONDOWN
            
        case WM_LBUTTONUP
            if uIdSubclass = IDC_FRMBUILDCONFIG_LSTOPTIONS then
                ' Handle if the checkbox is clicked
                dim as long nCurSel = ListBox_GetCurSel(hwnd)
                if nCurSel = -1 then exit function
                dim pWindow as CWindow ptr = AfxCWindowPtr(GetParent(hwnd))
                if pWindow = 0 then exit function
                dim as RECT rc: SendMessage(hwnd, LB_GETITEMRECT, nCurSel, cast(LPARAM, @rc))
                ' The checkbox is the first 20x20 area (16x16 icon)
                rc.Left   = rc.Left + pWindow->ScaleX(2)
                rc.Top    = rc.Top  + pWindow->ScaleY(2)
                rc.Right  = rc.Left + pWindow->ScaleX(16)
                rc.Bottom = rc.Top  + pWindow->ScaleY(16)
                dim as point pt = (loword(lParam), hiword(lParam))
                if PtInRect(@rc, pt) then
                    frmBuildConfig_ProcessListboxClick(hwnd, nCurSel)
                end if
            end if    
            
        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass hwnd, @frmBuildConfig_ListBox_SubclassProc, uIdSubclass

    End Select

    ' Default processing of Windows messages
    Function = DefSubclassProc(hwnd, uMsg, wParam, lParam)

End Function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmBuildConfig
' ========================================================================================
function frmBuildConfig_OnCreate( _
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
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmBuildConfig
' ========================================================================================
function frmBuildConfig_OnCommand( _
            byval hwnd as HWND, _
            byval id as Long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    dim as HWND hList1 = GetDlgItem( hwnd, IDC_FRMBUILDCONFIG_LIST1)
    dim as long nCurSel = ListBox_GetCurSel(hList1)

    ' Convert a ListBox DoubleClick into pressing OK button action
    if (id = IDC_FRMBUILDCONFIG_LIST1) andalso (codeNotify = LBN_DBLCLK) then
        id = IDOK: codeNotify = BN_CLICKED
    end if
    
    select case id
        case IDC_FRMBUILDCONFIG_LIST1
            if codeNotify = LBN_SELCHANGE then
                frmBuildConfig_SetBuildConfigTextboxes()
            end if
            
        case IDC_FRMBUILDCONFIG_LSTOPTIONS
            if codeNotify = LBN_SELCHANGE then
                AfxRedrawWindow(hwndCtl)
            end if

        case IDC_FRMBUILDCONFIG_TXTDESCRIPTION
            if codeNotify = EN_CHANGE then
                ' Update the temp array and the Listbox
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).wszDescription = AfxGetWindowText(hwndCtl)
                    ListBox_ReplaceString(hList1, nCurSel, gConfig.BuildsTemp(nCurSel).wszDescription)
                end if
            end if
            
        case IDC_FRMBUILDCONFIG_TXTOPTIONS
            if codeNotify = EN_CHANGE then
                ' Update the temp array 
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).wszOptions = AfxGetWindowText(hwndCtl)
                    ' Refresh the Options listbox so any manually entered options will
                    ' now be checked in the listbox.
                    AfxRedrawWindow(GetDlgItem(hwnd, IDC_FRMBUILDCONFIG_LSTOPTIONS))
                end if
            end if
            
        case IDC_FRMBUILDCONFIG_OPT32
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    if Button_GetCheck(hwndCtl) then
                        gConfig.BuildsTemp(nCurSel).Is32bit = 1
                        gConfig.BuildsTemp(nCurSel).Is64bit = 0
                    else   
                        gConfig.BuildsTemp(nCurSel).Is32bit = 0
                        gConfig.BuildsTemp(nCurSel).Is64bit = 1
                    end if
                end if
            end if

        case IDC_FRMBUILDCONFIG_OPT64
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    if Button_GetCheck(hwndCtl) then
                        gConfig.BuildsTemp(nCurSel).Is32bit = 0
                        gConfig.BuildsTemp(nCurSel).Is64bit = 1
                    else   
                        gConfig.BuildsTemp(nCurSel).Is32bit = 1
                        gConfig.BuildsTemp(nCurSel).Is64bit = 0
                    end if
                end if
            end if

        case IDC_FRMBUILDCONFIG_CHKISDEFAULT
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).IsDefault = Button_GetCheck(hwndCtl)
                    ' Can only have 1 entry as the default so ensure all others are reset
                    if Button_GetCheck(hwndCtl) then
                        for i as long = lbound(gConfig.BuildsTemp) to ubound(gConfig.BuildsTemp)
                            if i <> nCurSel then gConfig.BuildsTemp(i).IsDefault = 0
                        next
                    end if
                end if
            end if

        case IDC_FRMBUILDCONFIG_CHKEXCLUDEPOPUP
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).ExcludeInPopup = Button_GetCheck(hwndCtl)
                end if
            end if

        case IDC_FRMBUILDCONFIG_CHKCTRL
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).IsCtrl = Button_GetCheck(hwndCtl)
                end if
            end if

        case IDC_FRMBUILDCONFIG_CHKALT
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).IsAlt = Button_GetCheck(hwndCtl)
                end if
            end if

        case IDC_FRMBUILDCONFIG_CHKSHIFT
            if codeNotify = BN_CLICKED then
                ' Update the temp array 
                if nCurSel > -1 then
                    gConfig.BuildsTemp(nCurSel).IsShift = Button_GetCheck(hwndCtl)
                end if
            end if

        case IDC_FRMBUILDCONFIG_COMBOKEY
            if codeNotify = CBN_SELCHANGE then
                if nCurSel > -1 then gConfig.BuildsTemp(nCurSel).wszKey = AfxGetWindowText(hwndCtl)
            end if

        case IDC_FRMBUILDCONFIG_CMDUP
            if codeNotify = BN_CLICKED then
                if nCurSel > 0 then
                    frmBuildConfig_SwapListboxItems(nCurSel, nCurSel-1)
                end if
            end if

        case IDC_FRMBUILDCONFIG_CMDDOWN
            if codeNotify = BN_CLICKED then
                if nCurSel < ListBox_GetCount(hList1)-1 then
                    frmBuildConfig_SwapListboxItems(nCurSel, nCurSel+1)
                end if
            end if

        case IDC_FRMBUILDCONFIG_CMDINSERT
            if codeNotify = BN_CLICKED then
                redim preserve gConfig.BuildsTemp(ubound(gConfig.BuildsTemp) + 1)
                if ubound(gConfig.BuildsTemp) = -1 then
                    redim gConfig.BuildsTemp(0)
                    nCurSel = 0
                else
                    if nCurSel = -1 then nCurSel = 0
                    ' insert the item above current entry in the internal array
                    for i as long = ubound(gConfig.BuildsTemp) to nCurSel + 1 step -1
                        gConfig.BuildsTemp(i) = gConfig.BuildsTemp(i-1)
                    next
                end if
                gConfig.BuildsTemp(nCurSel).Id = AfxGuidText(AfxGuid())
                gConfig.BuildsTemp(nCurSel).wszDescription = ""
                gConfig.BuildsTemp(nCurSel).wszOptions = ""
                gConfig.BuildsTemp(nCurSel).IsDefault = 0
                gConfig.BuildsTemp(nCurSel).Is32bit = 1
                gConfig.BuildsTemp(nCurSel).Is64bit = 0
                gConfig.BuildsTemp(nCurSel).ExcludeInPopup = 0
                ' reload the listbox
                frmBuildConfig_LoadBuildListBox(hwnd)
                nCurSel = Min(nCurSel, ubound(gConfig.BuildsTemp))
                ListBox_SetCurSel(hList1, nCurSel)
                frmBuildConfig_SetBuildConfigTextboxes()
                SetFocus hList1
            end if

        case IDC_FRMBUILDCONFIG_CMDDELETE
            if codeNotify = BN_CLICKED then
                if nCurSel > -1  then
                    if MessageBox( HWND, L(287, "Are you sure you want to delete this build configuration?"), L(276,"Confirm"), _
                                        MB_YESNOCANCEL or MB_ICONINFORMATION or MB_DEFBUTTON1 ) = IDYES then
                        if ubound(gConfig.BuildsTemp) = 0 then
                            erase gConfig.BuildsTemp
                            nCurSel = -1
                        else
                            ' remove the item from the internal array
                            for i as long = nCurSel to ubound(gConfig.BuildsTemp) - 1
                                gConfig.BuildsTemp(i) = gConfig.BuildsTemp(i+1)
                            next
                            redim preserve gConfig.BuildsTemp(ubound(gConfig.BuildsTemp)-1)
                        end if
                        ' reload the listbox
                        frmBuildConfig_LoadBuildListBox(hwnd)
                        nCurSel = Min(nCurSel, ubound(gConfig.BuildsTemp))
                        ListBox_SetCurSel(hList1, nCurSel)
                        frmBuildConfig_SetBuildConfigTextboxes()
                        SetFocus hList1
                    end if
                end if
            end if

        case IDOK
            If codeNotify = BN_CLICKED then
                ' Copy the temporary items to the main array  
                redim gConfig.Builds(ubound(gConfig.BuildsTemp))
                for i as long = lbound(gConfig.BuildsTemp) to ubound(gConfig.BuildsTemp)
                    gConfig.Builds(i) = gConfig.BuildsTemp(i)   
                next
                erase gConfig.BuildsTemp
                
                ' Set the active project or the active document to the selected build
                if nCurSel > -1 then 
                    If gApp.IsProjectActive then
                        gApp.ProjectBuild = gConfig.Builds(nCurSel).Id
                    else
                        dim pDocMain as clsDocument Ptr 
                        pDocMain = gTTabCtl.GetActiveDocumentPtr()
                        if pDocMain then pDocMain->DocumentBuild = gConfig.Builds(nCurSel).Id
                        gConfig.CompilerBuild = gConfig.Builds(nCurSel).Id
                    end if
                    frmMain_SetStatusbar
                end if
                
                frmBuildConfig_CreateAcceleratorTable            
                gConfig.SaveConfigFile
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
            
        case IDCANCEL
            If codeNotify = BN_CLICKED then
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
    end select

    function = 0
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmBuildConfig
' ========================================================================================
function frmBuildConfig_OnClose( byval hwnd as HWND ) as LRESULT
    ' Enables parent window keeping parent's zorder
    EnableAllModeless()
    DestroyWindow( hwnd )
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmBuildConfig
' ========================================================================================
function frmBuildConfig_OnDestroy( byval hwnd as HWND) as LRESULT
    HWND_FRMBUILDCONFIG = 0
    PostQuitMessage(0)
    function = 0
end function


' ========================================================================================
' frmBuildConfig Window procedure
' ========================================================================================
function frmBuildConfig_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,      frmBuildConfig_OnCreate)
        HANDLE_MSG (hwnd, WM_CLOSE,       frmBuildConfig_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,     frmBuildConfig_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,     frmBuildConfig_OnCommand)
        HANDLE_MSG (hwnd, WM_MEASUREITEM, frmBuildConfig_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,    frmBuildConfig_OnDrawItem)
    end select

    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmBuildConfig_Show
' ========================================================================================
function frmBuildConfig_Show( byval hWndParent as HWND ) as LRESULT
    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    dim as HFONT hFontSymbolLarge = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    dim as HWnd hForm = _
        pWindow->Create(hWndParent, L(277,"Build Configurations"), _
            @frmBuildConfig_WndProc, 0, 0, 0, 0, _
            WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize(675, 500)
    pWindow->Center(pWindow->hWindow, hWndParent)

    dim hListBox as hwnd = pWindow->AddControl("LISTBOX", , IDC_FRMBUILDCONFIG_LIST1, "", 10, 10, 218, 420, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or LBS_NOTIFY or _
        LBS_NOINTEGRALHEIGHT or LBS_OWNERDRAWFIXED or LBS_HASSTRINGS, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hListBox, @frmBuildConfig_ListBox_SubclassProc, IDC_FRMBUILDCONFIG_LIST1, @pWindow)

    pWindow->AddControl("LABEL", , IDC_FRMBUILDCONFIG_LABEL1, L(278,"Description") & ":", 240, 11, 120, 20, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMBUILDCONFIG_CHKISDEFAULT, L(280,"Set as default"), 424, 10, 230, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("TEXTBOX", , IDC_FRMBUILDCONFIG_TXTDESCRIPTION, "", 240, 32, 425, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("RADIOBUTTON", , IDC_FRMBUILDCONFIG_OPT32, "FBC 32-bit", 240, 56, 100, 20, _
        WS_VISIBLE or WS_TABSTOP or BS_AUTORADIOBUTTON or BS_LEFT or BS_VCENTER or WS_GROUP, _
        WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("LABEL", , IDC_FRMBUILDCONFIG_LABEL3, L(294,"Accelerator") & ":", 424, 56, 100, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("RADIOBUTTON", , IDC_FRMBUILDCONFIG_OPT64, "FBC 64-bit", 240, 76, 100, 20, _
        WS_VISIBLE or WS_TABSTOP or BS_AUTORADIOBUTTON or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->AddControl("CHECKBOX", , IDC_FRMBUILDCONFIG_CHKCTRL, "Ctrl", 424, 76, 46, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMBUILDCONFIG_CHKALT, "Alt", 470, 76, 42, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("CHECKBOX", , IDC_FRMBUILDCONFIG_CHKSHIFT, "Shift", 512, 76, 44, 19, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    dim as HWND hCombo 
    hCombo = pWindow->AddControl("COMBOBOX", , IDC_FRMBUILDCONFIG_COMBOKEY, "", 556, 74, 103, 200, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or CBS_DROPDOWNLIST, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    AddShortcutsToComboBox( hCombo )

    pWindow->AddControl("LABEL", , IDC_FRMBUILDCONFIG_LABEL2, L(279,"Compiler Options") & ":", 240, 102, 170, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT or SS_NOTIFY, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("TEXTBOX", , IDC_FRMBUILDCONFIG_TXTOPTIONS, "", 240, 123, 425, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    dim hLB as hwnd = pWindow->AddControl("LISTBOX", , IDC_FRMBUILDCONFIG_LSTOPTIONS, "", 240, 149, 425, 263, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or LBS_NOTIFY or _
        LBS_NOINTEGRALHEIGHT or LBS_OWNERDRAWFIXED or LBS_HASSTRINGS, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hLB, @frmBuildConfig_ListBox_SubclassProc, IDC_FRMBUILDCONFIG_LSTOPTIONS, @pWindow)

    pWindow->AddControl("CHECKBOX", , IDC_FRMBUILDCONFIG_CHKEXCLUDEPOPUP, _
        L(284,"Exclude build from status bar popup menu"), 240, 415, 425, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)


    pWindow->AddControl("BUTTON", , IDC_FRMBUILDCONFIG_CMDINSERT, L(281, "Insert"), 8, 450, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDC_FRMBUILDCONFIG_CMDDELETE, L(282, "Delete"), 87, 450, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    
    dim as HWND hCtrl = _
    pWindow->AddControl("BUTTON", , IDC_FRMBUILDCONFIG_CMDUP, wszIconChevronUp, 166, 450, 28, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    AfxSetWindowFont( hCtrl, hFontSymbolLarge, false )     

    hCtrl = _
    pWindow->AddControl("BUTTON", , IDC_FRMBUILDCONFIG_CMDDOWN, wszIconChevronDown, 199, 450, 28, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    AfxSetWindowFont( hCtrl, hFontSymbolLarge, false )     

    pWindow->AddControl("BUTTON", , IDOK, L(0,"OK"), 508, 450, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDCANCEL, L(1,"Cancel"), 590, 450, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)

    HWND_FRMBUILDCONFIG = hForm
    
    ' Copy all of the Builds to the BuildsTemp array because we will work with 
    ' temporary copies until the user hits OK.
    redim gConfig.BuildsTemp(ubound(gConfig.Builds))
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        gConfig.BuildsTemp(i) = gConfig.Builds(i)   
    next
    frmBuildConfig_LoadBuildListBox(hForm)
    
    ' Load the Compiler Options Listbox
    hCtrl = GetDlgItem(hForm, IDC_FRMBUILDCONFIG_LSTOPTIONS)
    for i as long = lbound(gBuildOptions) to ubound(gBuildOptions)
        ListBox_AddString(hCtrl, gBuildOptions(i).wszText.sptr)
    next

    
    ' Set the current build selection 
    dim as long nCurSel = frmBuildConfig_getActiveBuildIndex()
    dim hList as HWND = GetDlgItem(hForm, IDC_FRMBUILDCONFIG_LIST1)
    if nCurSel = -1 then nCurSel = 0 
    ListBox_SetCurSel(hList, nCurSel)
    frmBuildConfig_SetBuildConfigTextboxes()
    
    SetFocus GetDlgItem(hForm, IDC_FRMBUILDCONFIG_LIST1)
    
    ' Process Windows messages(modal)
    function = pWindow->DoEvents(SW_SHOW)
    
    ' Delete the frmBuildConfig CWindow class manually allocated memory 
    if pWindow then delete(pWindow)
    if hFontSymbolLarge then DeleteObject(hFontSymbolLarge)
    
end function

