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

#include once "frmOptions.bi"
#include once "frmOptionsGeneral.bi"
#include once "frmOptionsEditor.bi"
#include once "frmOptionsEditor2.bi"
#include once "frmOptionsCompiler.bi"
#include once "frmOptionsColors.bi"
#include once "frmOptionsLocal.bi"
#include once "frmOptionsKeywords.bi"
#include once "frmOptionsKeywordsWinApi.bi"
#include once "frmOptionsKeywordsExtra.bi"
#include once "frmMain.bi"

#include once "clsConfig.bi"


' ========================================================================================
' Save all options to the gConfig class
' ========================================================================================
function frmOptions_SaveEditorOptions() as long   
    
    dim hwnd as HWND 
    dim wText as wstring * MAX_PATH  
    dim bChangesNextRun as boolean
    
    ' GENERAL OPTIONS   
    hwnd = HWND_FRMOPTIONSGENERAL
    gConfig.MultipleInstances    = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSGENERAL_CHKMULTIPLEINSTANCES) )
    gConfig.CompileAutosave      = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSGENERAL_CHKCOMPILEAUTOSAVE) )
    gConfig.AskExit              = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSGENERAL_CHKASKEXIT) )
    gConfig.RestoreSession       = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSGENERAL_CHKRESTORESESSION) )
    gConfig.CheckUpdates         = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSGENERAL_CHKUPDATES) )
    
    dim as long CompactMenus     = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSGENERAL_CHKCOMPACTMENUS) )
    gConfig.CompactMenus = CompactMenus


    ' EDITOR OPTIONS   
    hwnd = HWND_FRMOPTIONSEDITOR
    gConfig.SyntaxHighlighting    = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKSYNTAXHIGHLIGHTING) )
    gConfig.LeftMargin            = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKSHOWLEFTMARGIN) )
    gConfig.FoldMargin            = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKSHOWFOLDMARGIN) )
    gConfig.ConfineCaret          = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKCONFINECARET) )
    gConfig.LineNumbering         = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKLINENUMBERING) )
    gConfig.HighlightCurrentLine  = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKCURRENTLINE) )
    gConfig.TabIndentSpaces       = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKTABTOSPACES) )
    gConfig.IndentGuides          = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKINDENTGUIDES ) )
    gConfig.RightEdge             = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKSHOWRIGHTEDGE) )
    gConfig.PositionMiddle        = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKPOSITIONMIDDLE) )
    gConfig.ClickToggleBreakpoint = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_CHKTOGGLEBREAKPOINT) )

    gConfig.RightEdgePosition = AfxGetWindowText( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_TXTRIGHTEDGE) )
    gConfig.TabSize = AfxGetWindowText( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR_TXTTABSIZE) )
    gConfig.KeywordCase = ComboBox_GetCurSel( GetDlgItem( HWnd, IDC_FRMOPTIONSEDITOR_COMBOCASE))
    
    ' EDITOR OPTIONS2   
    hwnd = HWND_FRMOPTIONSEDITOR2
    gConfig.Codetips              = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKCODETIPS) )
    gConfig.AutoComplete          = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKAUTOCOMPLETE) )
    gConfig.CharacterAutoComplete = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKCHARAUTOCOMPLETE) )
    gConfig.AutoIndentation       = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKAUTOINDENTATION) )
    gConfig.ForNextVariable       = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKFORNEXTVARIABLE) )
    gConfig.BraceHighlight        = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKBRACEHIGHLIGHT) )
    gConfig.OccurrenceHighlight   = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSEDITOR2_CHKOCCURRENCEHIGHLIGHT) )


    ' If a localization file is currently being edited then save it now.
    frmOptionsLocal_LocalEditCheck

    ' If the localization has changed then display a message indicating that the changes
    ' will not be seen until the application is restarted.
    hwnd = HWND_FRMOPTIONSLOCAL
    wText = AfxGetWindowText( GetDlgItem(hwnd, IDC_FRMOPTIONSLOCAL_FRAMELOCALIZATION) )
    if (ucase(wText) <> ucase(gConfig.LocalizationFile)) orelse (gLocalPhrasesEdit = true) then
        bChangesNextRun = true
    end if
    gConfig.LocalizationFile = wText
    
    
    ' THEME COLOR/FONT OPTIONS
    hwnd = HWND_FRMOPTIONSCOLORS

    dim idx as Long 
    idx = ListBox_GetCurSel( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_LSTTHEMES) )
    if idx >= lbound(gThemeFilenames) andalso idx <= ubound(gThemeFilenames) then
        gConfig.ThemeShortFilename = gThemeFilenames(idx)
    end if   
    idx = ComboBox_GetCurSel( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_COMBOFONTNAME) )
    gConfig.EditorFontname = AfxGetComboBoxText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_COMBOFONTNAME), idx )
    idx = ComboBox_GetCurSel( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_COMBOFONTSIZE) )
    gConfig.EditorFontSize = AfxGetComboBoxText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_COMBOFONTSIZE), idx )  
    idx = ComboBox_GetCurSel( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_COMBOFONTCHARSET) )
    gConfig.EditorFontCharSet = AfxGetComboBoxText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOLORS_COMBOFONTCHARSET), idx )
    gConfig.FontExtraSpace = AfxGetWindowText( GetDlgItem( HWnd, IDC_FRMOPTIONSCOLORS_TXTEXTRASPACE))
    
    
    ' COMPILER OPTIONS
    hwnd = HWND_FRMOPTIONSCOMPILER
    
    ' Get the selected toolchain and then build the paths to the compilers
    dim as DWSTRING wszPath
    idx = ListBox_GetCurSel( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_LSTTOOLCHAINS) )
    if idx <> -1 then
        wszPath = AfxGetExePathName + "Toolchains\" + AfxGetListBoxText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_LSTTOOLCHAINS), idx )
        gConfig.FBWINCompiler32 = wszPath + "\fbc32.exe"
        gConfig.FBWINCompiler64 = wszPath + "\fbc64.exe"
    end if
    gConfig.CompilerSwitches = AfxGetWindowText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_TXTFBSWITCHES) )
    gConfig.CompilerIncludes = AfxGetWindowText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_TXTINCLUDES) )
    gConfig.CompilerHelpfile = AfxGetWindowText( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_TXTFBHELPFILE) )
    gConfig.RunViaCommandWindow = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_CHKRUNVIACOMMANDWINDOW) )
    gConfig.DisableCompileBeep = Button_GetCheck( GetDlgItem(hwnd, IDC_FRMOPTIONSCOMPILER_CHKDISABLECOMPILEBEEP) )
    
    
    ' KEYWORDS
    if gConfig.bKeywordsDirty then
        dim as HWND hTextBox = GetDlgItem(HWND_FRMOPTIONSKEYWORDS, IDC_FRMOPTIONSKEYWORDS_TXTKEYWORDS)
        gConfig.FBKeywords = AfxGetWindowText(hTextBox)
        hTextBox = GetDlgItem(HWND_FRMOPTIONSKEYWORDSWINAPI, IDC_FRMOPTIONSKEYWORDSWINAPI_TXTKEYWORDS)
        gConfig.WinApiKeywords = AfxGetWindowText(hTextBox)
        hTextBox = GetDlgItem(HWND_FRMOPTIONSKEYWORDSEXTRA, IDC_FRMOPTIONSKEYWORDSEXTRA_TXTKEYWORDS)
        gConfig.ExtraKeywords = AfxGetWindowText(hTextBox)
        gConfig.SaveKeywords
    end if
    
    ' Output the config settings to disk file
    gConfig.SaveConfigFile
    
    ' If Occurrences Highlighting is false then ensure that any highlights on the screen will be removed.
    dim pDoc as clsDocument ptr
    pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        frmMain_HighlightWord( pDoc, "" )
    end if
    
    if bChangesNextRun then
        MessageBox( HWND_FRMMAIN, _
                L(108,"Changes will be applied the next time the application is run."), _
                L(109,"Warning"), _
                MB_OK or MB_ICONINFORMATION or MB_DEFBUTTON1 or MB_APPLMODAL )
    end if
    
    function = 0
End function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmOptions
' ========================================================================================
function frmOptions_OnCreate( _
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
' Process WM_COMMAND message for window/dialog: frmOptions
' ========================================================================================
function frmOptions_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
        
        case IDC_FRMOPTIONS_CMDOK
            if codeNotify = BN_CLICKED then
                frmOptions_SaveEditorOptions()
                ' Reload the theme file in case it has changed
                LoadThemeFile()
                ' Refresh main background in case theme changed and no file open
                AfxRedrawWindow( HWND_FRMMAIN )
                ' Refresh the frmOutput controls (listviews and RichEdits)
                AfxRedrawWindow( HWND_FRMOUTPUT )
                frmOutput_SetControlColors()
                frmOutput_PositionWindows()
                ' Refresh the menubar. The rest of the GUI will refresh via frmMain_PositionWindows
                AfxRedrawWindow( HWND_FRMMAIN_MENUBAR )
                frmMenuBar_PositionWindows()
                
                ' Reposition main windows in case hide toolbar/statusbar selected
                frmMain_PositionWindows
                
                ' Apply the newly saved options to all open Scintilla windows
                dim pFrame as CWindow ptr
                dim pDoc as clsDocument Ptr = gApp.pDocList
                do until pDoc = 0
                    pDoc->ApplyProperties
                    pDoc = pDoc->pDocNext
                loop   
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
        
        case IDCANCEL
            if codeNotify = BN_CLICKED then
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
    
    end select

    function = 0
end function


' ========================================================================================
' Process WM_NOTIFY message for window/dialog: frmOptions
' ========================================================================================
function frmOptions_OnNotify( _
            byval hwnd as HWND, _
            byval id as long, _
            byval pNMHDR as NMHDR ptr _
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    
    select case id
    
        case IDC_FRMOPTIONS_TVWCATEGORIES
            if pNMHDR->code = TVN_SELCHANGED then
                dim as long nLeft, nTop
                dim as HWND hForm, hLabel

                static hCurrent as HWND   ' hwnd of currently display child Form
                    
                if hCurrent then ShowWindow hCurrent, SW_HIDE
                    
                dim lpNMTV as NM_TREEVIEW ptr = cast(NM_TREEVIEW ptr, pNMHDR)
                hLabel = GetDlgItem(hwnd, IDC_FRMOPTIONS_LBLCATEGORY)
                    
                ' Display the correct Category name and Child dialog. The child identifier is held in the lParam
                dim as long nBranchNum = TreeView_GetlParam( pNMHDR->hWndFrom, lpNMTV->itemNew.hItem)
                select case nBranchNum
                    case 0
                        AfxSetWindowText( hLabel, " " & L(226,"General Options")) 
                        hForm = HWND_FRMOPTIONSGENERAL
                    case 1
                        AfxSetWindowText( hLabel, " " & L(110,"Code Editor")) 
                        hForm = HWND_FRMOPTIONSEDITOR
                    case 2
                        AfxSetWindowText( hLabel, " " & L(107,"Advanced Code Editor")) 
                        hForm = HWND_FRMOPTIONSEDITOR2
                    case 3
                        AfxSetWindowText( hLabel, " " & L(111,"Themes and Fonts") )
                        hForm = HWND_FRMOPTIONSCOLORS
                    case 4
                        AfxSetWindowText( hLabel, " " & L(112,"Compiler Setup") )
                        hForm = HWND_FRMOPTIONSCOMPILER
                    case 5
                        AfxSetWindowText( hLabel, " " & L(129,"Localization") )
                        hForm = HWND_FRMOPTIONSLOCAL
                    case 6
                        AfxSetWindowText( hLabel, " " & L(177,"FreeBASIC Keywords") )
                        hForm = HWND_FRMOPTIONSKEYWORDS
                    case 7
                        AfxSetWindowText( hLabel, " " & L(435,"Windows Api Keywords") )
                        hForm = HWND_FRMOPTIONSKEYWORDSWINAPI
                    case 8
                        AfxSetWindowText( hLabel, " " & L(439,"Extra Keywords") )
                        hForm = HWND_FRMOPTIONSKEYWORDSEXTRA
                end select
                    
                ' Save the most recently opened tree branch 
                OptionsDialogLastOpened = nBranchNum

                
                ' Move the child Form into place. The child form will be displayed under the header label.
                dim rc as Rect
                GetWindowRect( hLabel, @rc )
                MapWindowPoints( null, hwnd, cast(LPPOINT, @rc), 2 )
                SetWindowPos hForm, HWND_TOP, _
                                rc.Left + pWindow->ScaleX(5), pWindow->ScaleY(50), _
                                pWindow->ScaleX(420), pWindow->ScaleY(350), _
                                SWP_SHOWWINDOW
                    
                InvalidateRect hwnd, byval null, true
                UpdateWindow hwnd
                    
                hCurrent = hForm
            end if

    end select

    function = 0
end function


' ========================================================================================
' Process WM_CTLCOLORSTATIC message for window/dialog: frmOptions
' ========================================================================================
function frmOptions_OnCtlColorStatic( _
            byval hwnd as HWND, _
            byval hdc as HDC, _
            byval hWndChild as HWND, _
            byval nType as long _
            ) as HBRUSH

    if hWndChild = GetDlgItem( hwnd, IDC_FRMOPTIONS_LBLCATEGORY ) then
        ' Set the category label to blue text 
        SetTextColor hdc, 8388608
        SetBkColor   hdc, GetSysColor( COLOR_BTNFACE )
        return GetSysColorBrush( COLOR_BTNFACE )
    end if
    
    function = 0      
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmOptions
' ========================================================================================
function frmOptions_OnClose( byval hwnd as HWND ) as LRESULT
    ' Enables parent window keeping parent's zorder
    EnableAllModeless()
    DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmOptions
' ========================================================================================
function frmOptions_OnDestroy( byval hwnd as HWND ) as LRESULT
    dim as HFONT hFont = AfxGetWindowFont(GetDlgItem(hwnd, IDC_FRMOPTIONS_LBLCATEGORY))
    DeleteFont(hFont)
    PostQuitMessage(0)
    function = 0
end function


' ========================================================================================
' frmOptions Window procedure
' ========================================================================================
function frmOptions_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,   frmOptions_OnCreate)
        HANDLE_MSG (hwnd, WM_CLOSE,    frmOptions_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,  frmOptions_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,  frmOptions_OnCommand)
        HANDLE_MSG (hwnd, WM_NOTIFY,   frmOptions_OnNotify)
        HANDLE_MSG (hwnd, WM_CTLCOLORSTATIC, frmOptions_OnCtlColorStatic)
    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmOptions_Show
' ========================================================================================
function frmOptions_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMOPTIONS = pWindow->Create( hWndParent, L(113,"Environment Options"), @frmOptions_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize(625, 460)
    pWindow->Center(pWindow->hWindow, hWndParent)
        
    dim as HWND hLabel = _
        pWindow->AddControl("LABEL", , IDC_FRMOPTIONS_LBLCATEGORY, "Category", 216, 10, 400, 30, _
            WS_CHILD or WS_VISIBLE or SS_LEFT or SS_CENTERIMAGE or SS_NOTIFY or SS_SUNKEN, _
            WS_EX_LEFT or WS_EX_LTRREADING)
    dim as HFONT hFont = pWindow->CreateFont("", 12, FW_BOLD)
    AfxSetWindowFont hLabel, hFont, true

    dim as HWND hWndTreeview = _
        pWindow->AddControl("TREEVIEW", , IDC_FRMOPTIONS_TVWCATEGORIES, "", 10, 10, 200, 385, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
    
    pWindow->AddControl("LABEL", , IDC_FRMOPTIONS_LABEL1, "", 8, 405, 600, 2, _
        WS_CHILD or WS_VISIBLE or SS_LEFT or SS_NOTIFY or SS_SUNKEN, _
        WS_EX_LEFT or WS_EX_LTRREADING)

    pWindow->AddControl("BUTTON", , IDC_FRMOPTIONS_CMDOK, L(0,"&OK"), 450, 417, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_DEFPUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    
    pWindow->AddControl("BUTTON", , IDCANCEL, L(1,"&Cancel"), 535, 417, 74, 28, _
        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_TEXT or BS_PUSHBUTTON or BS_NOTIFY or BS_CENTER or BS_VCENTER, _
        WS_EX_LEFT or WS_EX_LTRREADING)


    ' Reload the config file in case a user has automated a change to it since
    ' the application started (for example, changing the compiler path).
    gConfig.LoadConfigFile()

    ' Configure the TreeView
    dim hItems(8) as HTREEITEM 
    hItems(0) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(226,"General Options"),      0)
    hItems(1) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(110,"Code Editor"),          1)
    hItems(2) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(107,"Advanced Code Editor"), 2)
    hItems(3) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(111,"Colors and Fonts"),     3)
    hItems(4) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(112,"Compiler Setup"),       4)
    hItems(5) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(129,"Localization"),         5)
    hItems(6) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(177,"FreeBASIC Keywords"),   6)
    hItems(7) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(435,"Windows API Keywords"), 7)
    hItems(8) = TreeView_AppendItem (hwndTreeview, TVI_ROOT, L(439,"Extra Keywords"),       8)

    ' Load all of the child dialogs
    frmOptionsGeneral_Show( pWindow->hWindow )
    frmOptionsEditor_Show( pWindow->hWindow )
    frmOptionsEditor2_Show( pWindow->hWindow )
    frmOptionsColors_Show( pWindow->hWindow )
    frmOptionsCompiler_Show( pWindow->hWindow )
    frmOptionsLocal_Show( pWindow->hWindow )
    frmOptionsKeywords_Show( pWindow->hWindow )
    frmOptionsKeywordsWinApi_Show( pWindow->hWindow )
    frmOptionsKeywordsExtra_Show( pWindow->hWindow )

    ' Select the most previously open tree branch or first item "Code Editor" by default.
    TreeView_SelectItem( hWndTreeview, hItems(OptionsDialogLastOpened) )

    SetFocus hWndTreeview
    
    ' Process Windows messages
    function = pWindow->DoEvents(SW_SHOW)
    
    ' delete the frmOptions CWindow class manually allocated memory 
    if pWindow then delete(pWindow)

    ' delete the child windows releasing their manually allocated memory 
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSGENERAL):   if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSEDITOR):    if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSEDITOR2):   if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSCOLORS):    if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSCOMPILER):  if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSLOCAL):     if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSKEYWORDS):  if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSKEYWORDSWINAPI):  if pWindow then delete(pWindow)
    pWindow = AfxCWindowPtr(HWND_FRMOPTIONSKEYWORDSEXTRA):   if pWindow then delete(pWindow)

end function





