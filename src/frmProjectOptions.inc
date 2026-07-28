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

#include once "frmProjectOptions.bi"
#include once "frmBuildConfig.bi"


' ========================================================================================
' Create a generic resource file and manifest for the Project
' ========================================================================================
function frmProjectOptions_CreateResourceManifest() as Long

    dim as DWSTRING wszProjectPath      = AfxStrPathname("PATH", gApp.ProjectFilename)
    dim as DWSTRING wszManifest         = wszProjectPath + "manifest.xml"
    dim as DWSTRING wszResource         = wszProjectPath + "resource.rc"
    dim as DWSTRING wszManifestTemplate = AfxGetExePathName + "settings\backup_manifest.xml"
    dim as DWSTRING wszResourceTemplate = AfxGetExePathName + "settings\backup_resource.rc" 
    
    dim as DWSTRING wszFilesMissing
    
    if AfxFileExists(wszManifest) = false then 
        if AfxFileExists(wszManifestTemplate) = false then 
            wszFilesMissing = wszFilesMissing & _
                            space(5) & "- " & AfxStrPathname( "NAMEX", wszManifestTemplate ) & vbcrlf
        else
            AfxCopyFile( wszManifestTemplate, wszManifest, true )
        end if
    end if
    
    if AfxFileExists(wszResource) = false then 
        if AfxFileExists(wszResourceTemplate) = false then 
            wszFilesMissing = wszFilesMissing & _
                            space(5) & "- " & AfxStrPathname( "NAMEX", wszResourceTemplate ) & vbcrlf
        else
            AfxCopyFile( wszResourceTemplate, wszResource, true )
        end if   
    end if
    
    if len(wszFilesMissing) then
        MessageBox( 0, _
                    "The following files(s) are missing from the Settings folder. " + _
                    "Please reinstall the application." & vbcrlf & vbcrlf & _ 
                    wszFilesMissing, _
                    "Error", _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        exit function
    end if
    
    
    ' Add the resource file to the project if it does not already exist in the project
    if gApp.GetDocumentPtrByFilename(wszResource) = 0 then
        dim pDoc as clsDocument ptr
        pDoc = gApp.CreateEmptyDocument()
        pDoc->LoadDiskFile( wszResource )
        gApp.ProjectSetFileType(pDoc, FILETYPE_RESOURCE)
    end if
    
    function = 0
end function
    

' ========================================================================================
' Save all options for the Project
' ========================================================================================
function frmProjectOptions_SaveProjectOptions( byval hwnd as HWND ) as BOOLEAN    
    
    dim wText as wstring * MAX_PATH  
    
    ' Determine if the project path was specified
    wText = AfxGetWindowText(GetDlgItem(hwnd, IDC_FRMPROJECTOPTIONS_TXTPROJECTPATH))
    if len(wText) = 0 then
        MessageBox( hwnd, L(217,"Invalid project path specified."), L(201,"Error"), _
                        MB_OK or MB_ICONINFORMATION or MB_DEFBUTTON1 )
        return False
    end if

    if gApp.IsNewProjectFlag then 
        ' Need to close any open files or project at this point before loading
        ' and displaying the new project.
        if gApp.IsProjectActive then
            if OnCommand_ProjectClose(hwnd) = false then exit function
        else
            if OnCommand_FileClose(hwnd, EFC_CLOSEALL) = false then exit function
        end if
    end if

    ' Load the new or modified data into the Project variables
    gApp.IsProjectActive = true
    gApp.ProjectFilename = wText
    gApp.ProjectName = AfxStrPathname( "NAMEX", gApp.ProjectFilename )

    gApp.ProjectOther32     = AfxGetWindowText(GetDlgItem(hwnd, IDC_FRMPROJECTOPTIONS_TXTOPTIONS32))
    gApp.ProjectOther64     = AfxGetWindowText(GetDlgItem(hwnd, IDC_FRMPROJECTOPTIONS_TXTOPTIONS64))
    gApp.ProjectManifest    = Button_GetCheck(GetDlgItem(hwnd, IDC_FRMPROJECTOPTIONS_CHKMANIFEST))
    gApp.ProjectBuild       = frmBuildConfig_GetSelectedBuildGUID()
    gApp.ProjectNotes       = ""
    gApp.ProjectCommandLine = ""
    
    
    ' Create the resource file and manifest if applicable
    if gApp.ProjectManifest then frmProjectOptions_CreateResourceManifest()

    ' Save the actual Project data to disk.
    gConfig.ProjectSaveToFile()   

    function = true   
end function



' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmProjectOptions
' ========================================================================================
function frmProjectOptions_OnCommand( _
            byval hwnd as HWND, _
            byval id as Long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
        case IDC_FRMPROJECTOPTIONS_CMDSELECT
            if codeNotify = BN_CLICKED then
                dim as DWSTRING wszProjectName = ""
                if gApp.IsNewProjectFlag = false then wszProjectName = gApp.ProjectName
                dim pwszName as WString Ptr = AfxIFileSaveDialog(hwnd, wszProjectName, @wstr(APPEXTENSION), IDC_FRMPROJECTOPTIONS_CMDselect)
                if pwszName then
                    AfxSetWindowText( GetDlgItem(hwnd, IDC_FRMPROJECTOPTIONS_TXTPROJECTPATH), pwszName )
                    CoTaskMemFree pwszName
                end if
                exit function
            end if
            
        case IDOK
            if codeNotify = BN_CLICKED then
                if frmProjectOptions_SaveProjectOptions(hwnd) then
                    SendMessage hwnd, WM_CLOSE, 0, 0
                end if
                exit function
            end if
        
        case IDCANCEL
            if codeNotify = BN_CLICKED then
                gApp.IsNewProjectFlag = false
                SendMessage hwnd, WM_CLOSE, 0, 0
                exit function
            end if

    end select

    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmProjectOptions
' ========================================================================================
function frmProjectOptions_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    ' This is a modal popup window so disable the parent window
    EnableWindow(GetParent(hwnd), false)

    '  Message cracker macro expects a True to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmProjectOptions
' ========================================================================================
function frmProjectOptions_OnClose( byval hwnd as HWND ) as LRESULT
    ' Enables parent window keeping parent's zorder
    EnableWindow(GetParent(hwnd), true)
    DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmProjectOptions
' ========================================================================================
function frmProjectOptions_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage(0)
    function = 0
end function


' ========================================================================================
' frmProjectOptions Window procedure
' ========================================================================================
function frmProjectOptions_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,   frmProjectOptions_OnCreate)
        HANDLE_MSG (hwnd, WM_CLOSE,    frmProjectOptions_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,  frmProjectOptions_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,  frmProjectOptions_OnCommand)

        ' Handle the WM_SYSCOMMAND so we know that the dialog was closed through
        ' clicking on the "X" button or Alt+F4. We ensure that IsNewProject is
        ' set to false otherwise the OnCommand_ProjectNew() calling function
        ' will GPF.
        case WM_SYSCOMMAND
            if (wParam and  &hFFF0) = SC_CLOSE then
                ' User clicked the X or pressed Alt+F4
                gApp.IsNewProjectFlag = false
            end if

    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmProjectOptions_Show
' ========================================================================================
function frmProjectOptions_Show( _
            byval hwndParent as HWND, _
            byval IsNewProject as boolean _
            ) as LRESULT

    dim wszTitle as wstring * MAX_PATH
    dim as long nLeft, nTop, nWidth, nHeight, nMidPoint
    
    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    dim as hwnd hForm = _
    pWindow->Create( hwndParent, "", @frmProjectOptions_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT)
    pWindow->SetClientSize(600, 342)
    pWindow->Center(pWindow->hWindow, hwndParent)
        
    dim as RECT rcClient
    GetClientRect( hForm, @rcClient )
    nMidPoint = pWindow->UnScaleX((rcClient.Right / 2))
    
    pWindow->AddControl("LABEL", , IDC_FRMPROJECTOPTIONS_LABEL1, L(240,"Project Path"), 26, 15, 235, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT, _
        WS_EX_LEFT or WS_EX_LTRREADING)                                              
    dim as hwnd hProjectPath = _
    pWindow->AddControl("TEXTBOX", , IDC_FRMPROJECTOPTIONS_TXTPROJECTPATH, "", 26, 35, 508, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)     
    pWindow->AddControl("BUTTON", , IDC_FRMPROJECTOPTIONS_CMDselect, "...", 544, 33, 37, 24, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)

    nTop    = 95 
    nWidth  = 555
    nHeight = 20
    pWindow->AddControl("LABEL", , IDC_FRMPROJECTOPTIONS_LABEL5, L(283,"Specify compiler options in addition to the selected build configuration") & ":", 26, nTop, 555, nHeight, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT, _
        WS_EX_LEFT or WS_EX_LTRREADING)

    pWindow->AddControl("LABEL", , IDC_FRMPROJECTOPTIONS_LABEL2, L(241,"Other Options (32-bit compiler)"), 26, 121, 555, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    dim as hwnd hProjectOther32 = _
    pWindow->AddControl("TEXTBOX", , IDC_FRMPROJECTOPTIONS_TXTOPTIONS32, "", 26, 141, 555, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    
    pWindow->AddControl("LABEL", , IDC_FRMPROJECTOPTIONS_LABEL4, L(242,"Other Options (64-bit compiler)"), 26, 170, 555, 18, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_LEFT, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    dim as hwnd hProjectOther64 = _
    pWindow->AddControl("TEXTBOX", , IDC_FRMPROJECTOPTIONS_TXTOPTIONS64, "", 26, 190, 555, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    nWidth = iIf(IsNewProject, 280, 555) 
    dim as hwnd hManifest = _
    pWindow->AddControl("CHECKBOX", , IDC_FRMPROJECTOPTIONS_CHKMANIFEST, L(308,"Create resource file and manifest"), 26, 222, nWidth, 20, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_NOTIFY or BS_AUTOCHECKBOX or BS_LEFT or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    
    pWindow->AddControl("LABEL", , IDC_FRMPROJECTOPTIONS_LABEL3, "", 10, 282, 572, 2, _
        WS_CHILD or WS_VISIBLE or SS_LEFT or SS_NOTIFY or SS_SUNKEN, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDOK, L(0,"&OK"), 423, 297, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_DEFPUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->AddControl("BUTTON", , IDCANCEL, L(1,"&Cancel"), 508, 297, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)

    wszTitle = L(214,"Project Options")

    gApp.IsNewProjectFlag = IsNewProject
    if IsNewProject then
        ' Set the default values for a new project
        wszTitle = wszTitle & " - " & L(215, "New Project")
        Button_SetCheck( hManifest, true)
    else
        wszTitle = wszTitle + " - " + gApp.ProjectName
        AfxSetWindowText( hProjectPath, gApp.ProjectFilename )
        AfxSetWindowText( hProjectOther32, gApp.ProjectOther32 )
        AfxSetWindowText( hProjectOther64, gApp.ProjectOther64 )
        Button_SetCheck( hManifest, gApp.ProjectManifest)
    end if
    AfxSetWindowText( hForm, wszTitle )

    SetFocus GetDlgItem(hForm, IDOK)
    
    ' Process Windows messages(modal)
    function = pWindow->DoEvents(SW_SHOW)
    
    ' Delete the frmProjectOptions CWindow class manually allocated memory 
    if pWindow then delete(pWindow)

end function
