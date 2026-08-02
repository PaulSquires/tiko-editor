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

'  frmUserTools.inc
'
'  MASTER / DETAIL, shaped like frmOptions and frmKeyboard: a fixed 900x620 shell, a 52px
'  title band, a 58px footer with OK / Cancel, and a PsListTree down the left. The list's own
'  four commands (Add, Delete, and the two reorder chevrons) live in the title band directly
'  above it -- frmKeyboard uses that band for its search box, and this dialog has none, so it
'  would otherwise be 52px of nothing.
'
'  THE DETAIL COLUMN IS THE OPTIONS DIALOG'S SETTINGS-ROW IDIOM: label left, control
'  right-aligned, one fixed row height, no rules between rows. It does not scroll, so it must
'  FIT -- see the sum in frmUserTools.bi and the fit assertions in the self-test.
'
'  NOTHING SELECTED HIDES THE DETAIL COLUMN rather than disabling it. Every field here is a
'  PsTextBox, a PsComboBox or a PsToggle, and only the last has an Enabled state that reads
'  clearly; a row of greyed-out-looking-but-not-really controls is worse than an empty page
'  with one sentence on it. The Localization page's two mutually exclusive views, at smaller
'  scale.
'
'  PUMP OBLIGATIONS, BOTH MANDATORY: PsTextBox_FilterMessage (four text fields, each owning a
'  PsPopupMenu context menu) and PsComboBox_FilterMessage (the Action combo). Drop either and
'  the affected popup has no keyboard navigation and never closes on an outside click.

#include once "frmUserTools.bi"
#include once "clsConfig.bi"
#include once "modMRU.bi"


' ----------------------------------------------------------------------------------------
' Live control handles. Cleared in WM_DESTROY -- the dialog is rebuilt from scratch on every
' open, so a stale handle here would be a dead window on the next one.
' ----------------------------------------------------------------------------------------
dim shared as HWND ghUTAdd, ghUTDelete, ghUTUp, ghUTDown
dim shared as HWND ghUTName, ghUTCommand, ghUTParams, ghUTFolder
dim shared as HWND ghUTBrowseExe, ghUTBrowseFolder, ghUTAssign
dim shared as HWND ghUTAction
dim shared as HWND ghUTTogPrompt, ghUTTogMinimized, ghUTTogWait, ghUTTogMenu
dim shared as HWND ghUTOK, ghUTCancel

' The Parameters field's substitution-code tip. A PsTooltip, and the FIRST ONE IN TIKO -- see
' frmUserTools_Show for why it is not a comctl32 tooltip.
dim shared as HWND ghUTParamsTip

' TRUE while the detail controls are being loaded from the staging record. Every field's
' change callback checks it: PsTextBox_SetText and PsComboBox_SetCurSel are documented silent,
' but PsToggle_SetChecked is too and this guard costs a byte -- what it prevents is a
' selection change writing the OUTGOING row's widget contents into the INCOMING row.
dim shared gUTLoading as boolean

' The staging index the detail column is currently showing, or -1 for none.
dim shared gUTCurrent as long = -1


' ========================================================================================
' Prepare and Execute a user tool.
'
' UNCHANGED by the dialog rewrite -- it reads gConfig.Tools() and knows nothing about any
' window.
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
        if TikoMsgBox( HWND_FRMMAIN, L(241,"Please confirm that you wish to run the following User Tool:") + vbcrlf + _
                        gConfig.Tools(nToolNum).wszDescription, L(218,"Confirm"), _
                        TMB_ICON_QUESTION, TMB_YESNOCANCEL ) <> IDYES then
            exit function
        end if
    end if

    if gConfig.Tools(nToolNum).IsMinimized then nWndStyle = SW_SHOWMINNOACTIVE

    ' Prepare the command line based on any substitute parameters
    if len(gConfig.Tools(nToolNum).wszParameters) then

        pDocCurrent = gTTabCtl.GetActiveDocumentPtr()

        ' Do the substitutions for the command line parameters
        ' <P> is the project's own name, so it stays empty for an untitled workspace --
        ' there is no name to substitute. <M> falls back to the active document when no
        ' Main is designated, matching every other main-doc consumer.
        if gApp.IsProjectNamed() then
            wszProjectName = AfxStrPathname("NAMEX", gApp.ProjectFilename)
        end if
        pDocMain = gApp.GetMainDocumentPtr()
        if pDocMain = 0 then pDocMain = pDocCurrent

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
' Create the keyboard shortcut text description for the Tools MENU.
'
' UNCHANGED. Note that the guard below is now belt AND braces: the previous dialog wrote the
' literal string "None" into a keyless tool the moment its row was selected, and this is what
' stopped that reaching the menu as "Ctrl+NONE". The new dialog stores "" instead (see
' frmAssignKey_ModelFromViews), but an existing settings.ini still holds "None" and this
' has to keep working for it.
' ========================================================================================
function createToolsMenuShortcut( byval nCtrlID as long ) as DWSTRING
    dim wszShortcut as DWSTRING

    if KeyBindings_PickListKeyToValue( gConfig.Tools(nCtrlID).wszKey ) = 0 then return ""

    if gConfig.Tools(nCtrlID).IsCtrl then wszShortcut = "Ctrl+"
    if gConfig.Tools(nCtrlID).IsShift then wszShortcut = wszShortcut + "Shift+"
    if gConfig.Tools(nCtrlID).IsAlt then wszShortcut = wszShortcut + "Alt+"
    wszShortcut = trim(wszShortcut + ucase(trim(gConfig.Tools(nCtrlID).wszKey)), any " +")

    return wszShortcut
end function



' ========================================================================================
' Create an accelerator table based on selections for the user tools.
'
' UNCHANGED.
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

        ' Identical to frmBuildConfig's: the tool's key is a BARE NAME picked from the
        ' vocabulary, and this carried the SAME private 48-arm select case (F1-F12, 0-9, A-Z)
        ' against a pick-list offering 89 names. The other 41 -- Tab, Enter, PgUp, PgDn, Home,
        ' End, the arrows, the Numpad keys, the punctuation keys -- left nKey = 0, the ACCEL
        ' entry was skipped, and the tool's shortcut saved, displayed in the User Tools menu
        ' and reloaded perfectly while never firing.
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
' The shortcut a staging row DISPLAYS. Composed, never stored -- see the .bi.
' ========================================================================================
function frmUserTools_ShortcutText( byval idx as long ) as DWSTRING
    if (idx < lbound(gToolsWork)) orelse (idx > ubound(gToolsWork)) then return ""

    dim as DWSTRING wszKey = trim(gToolsWork(idx).wszKey)
    ' "None" is the pick-list AFFORDANCE the previous dialog persisted, not a key. Treated as
    ' no shortcut here, and NOT as a dead one -- see frmUserTools_ShortcutIsDead.
    if len(wszKey) = 0 then return ""
    if ucase(wszKey) = "NONE" then return ""

    dim as DWSTRING wsz
    if gToolsWork(idx).IsCtrl  then wsz = wsz & "Ctrl+"
    if gToolsWork(idx).IsShift then wsz = wsz & "Shift+"
    if gToolsWork(idx).IsAlt   then wsz = wsz & "Alt+"
    return wsz & wszKey
end function


' ========================================================================================
' Does this tool carry a key that cannot fire?
'
' A name in settings.ini that KeyBindings_PickListKeyToValue does not resolve produces no
' ACCEL entry at all, so the shortcut saves, reloads and shows in the Tools menu perfectly
' and never once works. There is no build warning and no visible symptom -- which is the
' whole reason the list paints it in theme.compile.iconfail.
'
' THIS DIALOG CANNOT CREATE ONE. Both routes into a key (the capture field and the picker) go
' through gKeyNames, and KeyBindings_RunSelfTest already asserts every name in that vocabulary
' resolves. What this catches is LEGACY DATA: settings.ini written by an older tiko whose
' pick-list and whose resolver did not agree.
'
' "" and "None" are NOT dead -- they mean no shortcut, which is a legal state.
' ========================================================================================
function frmUserTools_ShortcutIsDead( byval idx as long ) as boolean
    if (idx < lbound(gToolsWork)) orelse (idx > ubound(gToolsWork)) then return false
    dim as DWSTRING wszKey = trim(gToolsWork(idx).wszKey)
    if len(wszKey) = 0 then return false
    if ucase(wszKey) = "NONE" then return false
    return iif( KeyBindings_PickListKeyToValue( wszKey ) = 0, true, false )
end function


' ========================================================================================
' Does this tool's shortcut already belong to something else?
'
' "" when it is free; otherwise a sentence naming the owner -- an editor command or another
' user tool. Answers for a tool merely SITTING in the list, which the assign dialog cannot:
' that dialog refuses to CREATE a clash, but a settings.ini written before this check existed
' can already hold one, and a user tool whose shortcut duplicates an editor command loses to
' it silently. The tool simply never runs and nothing says why.
'
' Defined up here, not beside frmUserTools_DoAssign where it is also used, because the ROW
' PAINTER needs it and that comes first in this file.
' ========================================================================================
private function frmUserTools_ShortcutClash( byval idx as long ) as DWSTRING
    if (idx < lbound(gToolsWork)) orelse (idx > ubound(gToolsWork)) then return ""
    dim as DWSTRING wszKey = trim(gToolsWork(idx).wszKey)
    if len(wszKey) = 0 then return ""
    if ucase(wszKey) = "NONE" then return ""
    return frmAssignKey_ClashText( wszKey, _
                                     iif( gToolsWork(idx).IsCtrl  <> 0, true, false ), _
                                     iif( gToolsWork(idx).IsAlt   <> 0, true, false ), _
                                     iif( gToolsWork(idx).IsShift <> 0, true, false ), _
                                     ASSIGNKEY_DOMAIN_TOOLS, idx )
end function


' ========================================================================================
' Seed the staging copy from the live tools. Called once, before any control exists.
' ========================================================================================
private sub frmUserTools_LoadWork()
    erase gToolsWork
    if ubound(gConfig.Tools) < 0 then exit sub
    redim gToolsWork( ubound(gConfig.Tools) )
    for i as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
        gToolsWork(i) = gConfig.Tools(i)
    next
end sub


' ========================================================================================
' Commit the staging copy.
'
' ORDER IS LOAD-BEARING, exactly as in frmKeyboard_CommitWork: gConfig.Tools FIRST, then the
' accelerator table, then disk. frmUserTools_CreateAcceleratorTable reads gConfig.Tools, so
' rebuilding it before the copy would reinstate the pre-edit shortcuts with nothing reporting
' it; SaveConfigFile likewise writes gConfig.
'
' The User Tools submenu needs no refresh -- it rebuilds itself from gConfig.Tools every time
' it opens.
' ========================================================================================
private sub frmUserTools_CommitWork()
    if ubound(gToolsWork) < 0 then
        erase gConfig.Tools
    else
        redim gConfig.Tools( ubound(gToolsWork) )
        for i as long = lbound(gToolsWork) to ubound(gToolsWork)
            gConfig.Tools(i) = gToolsWork(i)
        next
    end if
    frmUserTools_CreateAcceleratorTable()
    gConfig.SaveConfigFile()
end sub


' ========================================================================================
' Which staging row is highlighted? -1 for none.
'
' Through the row's itemData rather than its position, so it stays correct if the list ever
' grows a filter -- the shape frmKeyboard uses for the same reason.
' ========================================================================================
private function frmUserTools_SelectedWorkIndex() as long
    if HWND_FRMUSERTOOLS_LIST = 0 then return -1
    dim as long nRow = PsListTree_GetCurSel( HWND_FRMUSERTOOLS_LIST )
    if nRow < 0 then return -1
    dim as long idx = PsListTree_GetItemData( HWND_FRMUSERTOOLS_LIST, nRow )
    if (idx < lbound(gToolsWork)) orelse (idx > ubound(gToolsWork)) then return -1
    return idx
end function


' ========================================================================================
' Row painter.
'
' NOT the shared ListBox_LVPaintRow in modRoutines.inc: that one draws in the ghOutput
' palette for the Output panel's three lists, and this dialog is dressed like frmOptions. It
' also needs two states the shared painter has no concept of -- an unnamed tool and a dead
' shortcut.
' ========================================================================================
private sub frmUserTools_ListPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr = iif( p->isSelected, ghPanel.BackColorHot, ghPanel.BackColor )
    dim as COLORREF foreclr = iif( p->isSelected, ghPanel.ForeColorHot, ghPanel.ForeColor )

    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_11) )

    dim as long idx = PsListTree_GetItemData( HWND_FRMUSERTOOLS_LIST, p->itemID )
    if (idx < lbound(gToolsWork)) orelse (idx > ubound(gToolsWork)) then exit sub
    if p->columnCount <= 0 then exit sub

    ' A shortcut is BAD in either of two ways, and the cell is marked the same for both --
    ' they have the same consequence for the user (the tool's shortcut does not run the tool)
    ' and the detail pane below spells out which one it is.
    dim as boolean bBadKey = frmUserTools_ShortcutIsDead( idx )
    if bBadKey = false then bBadKey = (len(frmUserTools_ShortcutClash( idx )) > 0)

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(6)

        dim as DWSTRING wszCell = p->cells[i].wszText
        dim as COLORREF clr = foreclr

        select case i
            case 0
                ' A tool with no name is a legal state -- Add creates one, and the user names
                ' it afterwards. Show a placeholder rather than a blank row that looks like a
                ' rendering fault, and dim it so it does not read as the tool's actual name.
                if len(wszCell) = 0 then
                    wszCell = L(353,"(untitled)")
                    if p->isSelected = false then clr = ghPopup.ForeColorDisabled
                end if

            case 1
                ' The shortcut. Marked when it cannot fire; state readable as colour, and the
                ' detail pane spells it out in words for a reader who cannot see the colour.
                if bBadKey then clr = theme.compile.iconfail
        end select

        p->b->SetForeColor( clr )
        p->b->PaintText( wszCell, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next

end sub


' ========================================================================================
' Header band painter. REQUIRED -- without one the header does not render at all.
' ========================================================================================
private sub frmUserTools_ListPaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr = ghPanel.BackColor
    if p->isHot orelse p->isResizeHot then backclr = ghPanel.BackColorHot

    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(6)
    p->b->SetForeColor( ghPanel.ForeColorHot )
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    ' Hairline on the trailing edge so columns read as separated.
    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( ghEditor.Divider )
    p->b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' Show or hide the whole detail column, and load it from a staging row.
'
' nIdx = -1 hides everything and leaves the paint handler to draw the empty-state sentence.
' ========================================================================================
private sub frmUserTools_LoadDetail( byval nIdx as long )

    gUTCurrent = nIdx
    dim as long nShow = iif( nIdx < 0, SW_HIDE, SW_SHOW )

    ShowWindow( ghUTName, nShow )
    ShowWindow( ghUTCommand, nShow )
    ShowWindow( ghUTBrowseExe, nShow )
    ShowWindow( ghUTParams, nShow )
    ShowWindow( ghUTFolder, nShow )
    ShowWindow( ghUTBrowseFolder, nShow )
    ShowWindow( ghUTAssign, nShow )
    ShowWindow( ghUTAction, nShow )
    ShowWindow( ghUTTogPrompt, nShow )
    ShowWindow( ghUTTogMinimized, nShow )
    ShowWindow( ghUTTogWait, nShow )
    ShowWindow( ghUTTogMenu, nShow )

    ' Delete and the two reorder chevrons are meaningless with nothing selected, and each
    ' chevron is meaningless at its own end of the list. Add is never disabled.
    ' Through PsButton_SetEnabled rather than EnableWindow: the control repaints itself in its
    ' disabled palette from there, and a bare EnableWindow would leave it looking live.
    dim as boolean bHave = (nIdx >= 0)
    dim as boolean bCanDown = bHave andalso (nIdx < ubound(gToolsWork))
    PsButton_SetEnabled( ghUTDelete, bHave )
    PsButton_SetEnabled( ghUTUp,     iif( nIdx > 0, true, false ) )
    PsButton_SetEnabled( ghUTDown,   bCanDown )

    if nIdx < 0 then
        AfxRedrawWindow( HWND_FRMUSERTOOLS )
        exit sub
    end if

    ' EVERY SETTER BELOW IS SILENT (the family rule: programmatic setters do not notify), but
    ' the guard is raised anyway -- what it prevents is a loop, not a cosmetic glitch.
    gUTLoading = true

    PsTextBox_SetText( ghUTName,    gToolsWork(nIdx).wszDescription )
    PsTextBox_SetText( ghUTCommand, gToolsWork(nIdx).wszCommand )
    PsTextBox_SetText( ghUTParams,  gToolsWork(nIdx).wszParameters )
    PsTextBox_SetText( ghUTFolder,  gToolsWork(nIdx).wszWorkingFolder )

    dim as long nAction = gToolsWork(nIdx).Action
    if (nAction < 0) orelse (nAction > USERTOOL_ACTION_EDITORSTARTUP) then nAction = USERTOOL_ACTION_SELECTED
    PsComboBox_SetCurSel( ghUTAction, nAction )

    ' THE TYPE TRAP, in the reading direction: these fields are `as long` in clsConfig on
    ' purpose (the config writer emits them verbatim, so a real boolean would put
    ' "true"/"false" into settings.ini), and PsToggle deals in booleans.
    PsToggle_SetChecked( ghUTTogPrompt,    iif( gToolsWork(nIdx).IsPromptRun   <> 0, true, false ) )
    PsToggle_SetChecked( ghUTTogMinimized, iif( gToolsWork(nIdx).IsMinimized   <> 0, true, false ) )
    PsToggle_SetChecked( ghUTTogWait,      iif( gToolsWork(nIdx).IsWaitFinish  <> 0, true, false ) )
    PsToggle_SetChecked( ghUTTogMenu,      iif( gToolsWork(nIdx).IsDisplayMenu <> 0, true, false ) )

    gUTLoading = false

    AfxRedrawWindow( HWND_FRMUSERTOOLS )

end sub


' ========================================================================================
' (Re)fill the list from the staging copy.
'
' Selection is restored BY STAGING INDEX, not by row -- the two are the same today, but the
' reorder commands move rows under the selection and restoring "row 3" after a Move Up would
' follow the position rather than the tool.
' ========================================================================================
private sub frmUserTools_BuildList( byval nSelectIdx as long = -2 )

    if HWND_FRMUSERTOOLS_LIST = 0 then exit sub

    dim as long nKeepIdx = nSelectIdx
    if nKeepIdx = -2 then nKeepIdx = frmUserTools_SelectedWorkIndex()

    PsListTree_Clear( HWND_FRMUSERTOOLS_LIST )

    dim as long nSelectRow = -1
    for i as long = lbound(gToolsWork) to ubound(gToolsWork)
        ' Column 0 is the row's own text; the itemData carries the staging index.
        dim as long nRow = PsListTree_AddString( HWND_FRMUSERTOOLS_LIST, gToolsWork(i).wszDescription, i )
        if nRow < 0 then continue for
        PsListTree_SetCellText( HWND_FRMUSERTOOLS_LIST, nRow, 1, frmUserTools_ShortcutText(i) )
        if i = nKeepIdx then nSelectRow = nRow
    next

    if (nSelectRow < 0) andalso (PsListTree_GetCount( HWND_FRMUSERTOOLS_LIST ) > 0) then nSelectRow = 0
    if nSelectRow >= 0 then PsListTree_SetCurSel( HWND_FRMUSERTOOLS_LIST, nSelectRow )

    AfxRedrawWindow( HWND_FRMUSERTOOLS_LIST )

    ' SetCurSel is silent -- the family rule -- so the detail column has to be told.
    frmUserTools_LoadDetail( frmUserTools_SelectedWorkIndex() )

end sub


' ========================================================================================
' Refresh one row's cells after an edit, without rebuilding the whole list.
' ========================================================================================
private sub frmUserTools_RefreshRow( byval idx as long )
    if (HWND_FRMUSERTOOLS_LIST = 0) orelse (idx < 0) then exit sub
    for nRow as long = 0 to PsListTree_GetCount( HWND_FRMUSERTOOLS_LIST ) - 1
        if PsListTree_GetItemData( HWND_FRMUSERTOOLS_LIST, nRow ) <> idx then continue for
        PsListTree_SetText( HWND_FRMUSERTOOLS_LIST, nRow, gToolsWork(idx).wszDescription )
        PsListTree_SetCellText( HWND_FRMUSERTOOLS_LIST, nRow, 1, frmUserTools_ShortcutText(idx) )
        exit for
    next
    AfxRedrawWindow( HWND_FRMUSERTOOLS_LIST )
end sub


' ========================================================================================
' A theme-matched PsMessageBox owned by this dialog.
'
' Spelled out here rather than calling frmOptions_ThemedMsgBox: that one parents its box to
' HWND_FRMOPTIONS, which is 0 while this dialog is up, and a modal box with no owner does not
' disable the right window.
'
' Its nested modal loop runs INSIDE this dialog's own nested loop, which is inside frmMain's.
' That works -- the Options dialog does the same and it was confirmed interactively -- but it
' is the reason the box must be owned by a window that is actually up.
' ========================================================================================
private function frmUserTools_ThemedMsgBox( byval wszText as DWSTRING, _
                                            byval wszCaption as DWSTRING, _
                                            byval nIcon as long, _
                                            byval nButtons as long ) as long

    dim as long nDefault = 0
    if nButtons = TMB_OKCANCEL then nDefault = 1     ' Cancel: it guards a destructive action

    return TikoMsgBox( HWND_FRMUSERTOOLS, wszText, wszCaption, nIcon, nButtons, nDefault )
end function


' ========================================================================================
' List commands
' ========================================================================================

private sub frmUserTools_DoAdd()

    dim as long nAt = frmUserTools_SelectedWorkIndex()

    if ubound(gToolsWork) < 0 then
        redim gToolsWork(0)
        nAt = 0
    else
        redim preserve gToolsWork( ubound(gToolsWork) + 1 )
        if nAt < 0 then nAt = ubound(gToolsWork)
        ' Insert ABOVE the highlighted row, which is what the previous dialog's "Insert" did.
        for i as long = ubound(gToolsWork) to nAt + 1 step -1
            gToolsWork(i) = gToolsWork(i-1)
        next
    end if

    ' A fresh record. Assigned field by field rather than through a blank TYPE_TOOLS so that
    ' a field added to that type later shows up here as a compile-time reminder.
    gToolsWork(nAt).wszDescription   = ""
    gToolsWork(nAt).wszCommand       = ""
    gToolsWork(nAt).wszParameters    = ""
    gToolsWork(nAt).wszKey           = ""
    gToolsWork(nAt).wszWorkingFolder = ""
    gToolsWork(nAt).IsCtrl           = 0
    gToolsWork(nAt).IsAlt            = 0
    gToolsWork(nAt).IsShift          = 0
    gToolsWork(nAt).IsPromptRun      = 0
    gToolsWork(nAt).IsMinimized      = 0
    gToolsWork(nAt).IsWaitFinish     = 0
    gToolsWork(nAt).IsDisplayMenu    = 0
    gToolsWork(nAt).Action           = USERTOOL_ACTION_SELECTED

    frmUserTools_BuildList( nAt )
    ' Straight into the name field: an unnamed tool is the one thing the user must fix, and
    ' the list is showing it as "(untitled)" until they do.
    SetFocus( ghUTName )

end sub


private sub frmUserTools_DoDelete()

    dim as long idx = frmUserTools_SelectedWorkIndex()
    if idx < 0 then exit sub

    if frmUserTools_ThemedMsgBox( L(237,"Are you sure you want to delete this user tool?"), _
                                  L(218,"Confirm"), TMB_ICON_WARNING, TMB_OKCANCEL ) <> IDOK then exit sub

    if ubound(gToolsWork) = 0 then
        erase gToolsWork
        idx = -1
    else
        for i as long = idx to ubound(gToolsWork) - 1
            gToolsWork(i) = gToolsWork(i+1)
        next
        redim preserve gToolsWork( ubound(gToolsWork) - 1 )
        if idx > ubound(gToolsWork) then idx = ubound(gToolsWork)
    end if

    frmUserTools_BuildList( idx )
    SetFocus( HWND_FRMUSERTOOLS_LIST )

end sub


' Move the highlighted tool one place. nDelta is -1 or +1. The SELECTION FOLLOWS THE TOOL,
' not the position -- otherwise a second click on Move Up would move a different tool.
private sub frmUserTools_DoMove( byval nDelta as long )

    dim as long idx = frmUserTools_SelectedWorkIndex()
    if idx < 0 then exit sub

    dim as long nTo = idx + nDelta
    if (nTo < lbound(gToolsWork)) orelse (nTo > ubound(gToolsWork)) then exit sub

    swap gToolsWork(idx), gToolsWork(nTo)
    frmUserTools_BuildList( nTo )
    SetFocus( HWND_FRMUSERTOOLS_LIST )

end sub


' ========================================================================================
' Open the Assign Shortcut dialog on the highlighted tool.
' ========================================================================================
private sub frmUserTools_DoAssign()

    dim as long idx = frmUserTools_SelectedWorkIndex()
    if idx < 0 then exit sub

    gAssignKeyCaption = gToolsWork(idx).wszDescription
    ' Which domain and which row, so the clash test skips the tool's own current binding --
    ' without it, opening the dialog on a tool that already has Ctrl+1 reports that it clashes
    ' with itself and disables OK before the user has touched anything. The domain also tells
    ' the test to read gToolsWork (staging) for tools and gConfig.Builds (committed) for
    ' builds, which is right because the two dialogs cannot be open at once.
    gAssignKeyDomain  = ASSIGNKEY_DOMAIN_TOOLS
    gAssignKeyIndex   = idx
    gAssignKeyName    = gToolsWork(idx).wszKey
    gAssignKeyCtrl    = iif( gToolsWork(idx).IsCtrl  <> 0, true, false )
    gAssignKeyAlt     = iif( gToolsWork(idx).IsAlt   <> 0, true, false )
    gAssignKeyShift   = iif( gToolsWork(idx).IsShift <> 0, true, false )

    frmAssignKey_Show( HWND_FRMUSERTOOLS )

    ' Cancel leaves gAssignKey* exactly as set above, so this is a no-op on that path rather
    ' than a case that has to be detected.
    gToolsWork(idx).wszKey  = gAssignKeyName
    gToolsWork(idx).IsCtrl  = iif( gAssignKeyCtrl,  1, 0 )
    gToolsWork(idx).IsAlt   = iif( gAssignKeyAlt,   1, 0 )
    gToolsWork(idx).IsShift = iif( gAssignKeyShift, 1, 0 )

    ' BOTH sentinels, not just the index: leaving the domain set would make a later
    ' ASSIGNKEY_DOMAIN_NONE caller silently read staging instead of committed records.
    gAssignKeyDomain = ASSIGNKEY_DOMAIN_NONE
    gAssignKeyIndex  = -1

    frmUserTools_RefreshRow( idx )
    AfxRedrawWindow( HWND_FRMUSERTOOLS )
    SetFocus( ghUTAssign )

end sub


' ========================================================================================
' Control callbacks
' ========================================================================================

' PsButton does not post WM_COMMAND to its parent, so every button arrives here.
private sub frmUserTools_ButtonClick( byval hButton as HWND, byval id as long )
    select case id
        case IDC_FRMUSERTOOLS_CMDADD:    frmUserTools_DoAdd()
        case IDC_FRMUSERTOOLS_CMDDELETE: frmUserTools_DoDelete()
        case IDC_FRMUSERTOOLS_CMDUP:     frmUserTools_DoMove( -1 )
        case IDC_FRMUSERTOOLS_CMDDOWN:   frmUserTools_DoMove( +1 )
        case IDC_FRMUSERTOOLS_CMDASSIGN: frmUserTools_DoAssign()

        case IDC_FRMUSERTOOLS_CMDBROWSEEXE
            if gUTCurrent < 0 then exit sub
            ' The id is handed straight through: AfxIFileOpenDialogW picks its file-type
            ' filter and its title by button id.
            dim pwszName as wstring ptr = AfxIFileOpenDialogW( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDBROWSEEXE )
            if pwszName then
                ' SetText is silent, so staging is written here too rather than relying on the
                ' change callback that a user keystroke would have raised.
                PsTextBox_SetText( ghUTCommand, *pwszName )
                gToolsWork(gUTCurrent).wszCommand = *pwszName
                CoTaskMemFree( pwszName )
            end if

        case IDC_FRMUSERTOOLS_CMDBROWSEFOLDER
            if gUTCurrent < 0 then exit sub
            dim as DWSTRING wszFolder = AfxBrowseForFolder( HWND_FRMUSERTOOLS, L(232,"Working Folder"), curdir )
            if len(wszFolder) then
                PsTextBox_SetText( ghUTFolder, wszFolder )
                gToolsWork(gUTCurrent).wszWorkingFolder = wszFolder
            end if

        case IDC_FRMUSERTOOLS_CMDOK
            frmUserTools_CommitWork()
            SendMessage( HWND_FRMUSERTOOLS, WM_CLOSE, 0, 0 )

        case IDC_FRMUSERTOOLS_CMDCANCEL
            SendMessage( HWND_FRMUSERTOOLS, WM_CLOSE, 0, 0 )
    end select
end sub


' A PsTextBox changed through USER typing (SetText is silent). Route to the right field.
private sub frmUserTools_TextChanged( byval hTextBox as HWND )
    if gUTLoading then exit sub
    if gUTCurrent < 0 then exit sub

    if hTextBox = ghUTName then
        gToolsWork(gUTCurrent).wszDescription = PsTextBox_GetText( hTextBox )
        ' The list's first column IS this field, so it has to follow every keystroke.
        frmUserTools_RefreshRow( gUTCurrent )
    elseif hTextBox = ghUTCommand then
        gToolsWork(gUTCurrent).wszCommand = PsTextBox_GetText( hTextBox )
    elseif hTextBox = ghUTParams then
        gToolsWork(gUTCurrent).wszParameters = PsTextBox_GetText( hTextBox )
    elseif hTextBox = ghUTFolder then
        gToolsWork(gUTCurrent).wszWorkingFolder = PsTextBox_GetText( hTextBox )
    end if
end sub


' THE TYPE TRAP, in the writing direction: store a long 0/1, never the boolean itself.
private sub frmUserTools_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    if gUTLoading then exit sub
    if gUTCurrent < 0 then exit sub

    if hToggle = ghUTTogPrompt then
        gToolsWork(gUTCurrent).IsPromptRun = iif( isChecked, 1, 0 )
    elseif hToggle = ghUTTogMinimized then
        gToolsWork(gUTCurrent).IsMinimized = iif( isChecked, 1, 0 )
    elseif hToggle = ghUTTogWait then
        gToolsWork(gUTCurrent).IsWaitFinish = iif( isChecked, 1, 0 )
    elseif hToggle = ghUTTogMenu then
        gToolsWork(gUTCurrent).IsDisplayMenu = iif( isChecked, 1, 0 )
    end if
end sub


private sub frmUserTools_ActionChanged( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if gUTLoading then exit sub
    if gUTCurrent < 0 then exit sub
    gToolsWork(gUTCurrent).Action = idxNew
    frmUserTools_RefreshRow( gUTCurrent )
end sub


private sub frmUserTools_ListSelChange( byval hList as HWND, byval row as integer )
    frmUserTools_LoadDetail( frmUserTools_SelectedWorkIndex() )
end sub


' Double-click, or ENTER on the focused row. Setting this callback is ALSO what makes the
' control claim ENTER (per-message DLGC_WANTALLKEYS plus the WM_CHAR swallow that stops the
' system beeping on every press); without it Enter would reach IsDialogMessage and fire OK.
private sub frmUserTools_ListActivate( byval hList as HWND, byval row as integer )
    if frmUserTools_SelectedWorkIndex() >= 0 then SetFocus( ghUTName )
end sub


' NO TOOLTIPS, on the rows OR the header. With no callback PsListTree falls back to the row's
' own text and PsColumnHeader to the column's own caption, so hovering "Shortcut" would pop a
' tip reading "Shortcut". These are TWO SEPARATE OPT-OUTS and it is easy to fix one and leave
' the other.
private function frmUserTools_ListTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    return ""
end function

private function frmUserTools_HeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' Paint the dialog's own chrome: background, the detail column's labels, the parameter hint,
' the shortcut readout, the divider, and the empty-state sentence.
'
' KEEP THE RUNNING y IN STEP WITH frmUserTools_PositionWindows. The two walk the same rows,
' and the label rect a row draws into is the same rect the control on that row is placed in.
' ========================================================================================
private function frmUserTools_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    dim as long mx      = AfxScaleX(FRMUSERTOOLS_MARGIN)
    dim as long titleH  = AfxScaleY(FRMUSERTOOLS_TITLE_H)
    dim as long detailX = mx + AfxScaleX(FRMUSERTOOLS_LIST_W) + AfxScaleX(FRMUSERTOOLS_DETAIL_GAP)
    dim as long labelW  = AfxScaleX(FRMUSERTOOLS_LABEL_W)
    dim as long fieldX  = detailX + labelW
    dim as long rowH    = AfxScaleY(FRMUSERTOOLS_ROWH)
    dim as long xRight   = rc.right - mx

    ' NOTHING IS DRAWN IN THE TITLE BAND. It holds the list's four command buttons and no
    ' text -- a page title there would be the window caption repeated an inch below itself,
    ' which is exactly why frmKeyboard's was removed. (frmOptions keeps one because there the
    ' caption and the band genuinely say different things: the caption names the dialog and
    ' the band names the page.)

    dim as RECT rcT

    ' ---- the empty state --------------------------------------------------------------
    if gUTCurrent < 0 then
        rcT.left   = detailX
        rcT.right  = xRight
        rcT.top    = titleH
        rcT.bottom = rc.bottom - AfxScaleY(FRMUSERTOOLS_FOOTER_H)
        buffer.SetForeColor( ghPanel.ForeColor )
        buffer.SetFont( ghFont(GUIFONT_11) )
        buffer.PaintText( L(361,"Select a tool, or press Add to create one."), @rcT, _
                          DT_CENTER or DT_VCENTER or DT_SINGLELINE )
        buffer.EndDoubleBuffer()
        EndPaint( hwnd, @ps )
        return 0
    end if

    ' ---- the labelled rows ------------------------------------------------------------
    dim as long y = titleH + AfxScaleY(12)

    buffer.SetFont( ghFont(GUIFONT_11) )
    buffer.SetForeColor( ghPanel.ForeColor )

    #macro ROWLABEL( _text_ )
        rcT.left = detailX : rcT.right = fieldX - AfxScaleX(FRMUSERTOOLS_GAP)
        rcT.top  = y       : rcT.bottom = y + rowH
        buffer.PaintText( _text_, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
        y += rowH
    #endmacro

    ROWLABEL( L(230,"Tool Name") )
    ROWLABEL( L(231,"Command") )
    ROWLABEL( L(358,"Parameters") )

    ' THE SUBSTITUTION CODES ARE A TOOLTIP, NOT A LABEL. They used to be drawn here, on a line
    ' under the Parameters field -- which cost a row of the fixed page to a reference card the
    ' reader needs once, and still ellipsized: four codes with their meanings do not fit on one
    ' line of this column at any DPI. They are now a wrapped multi-line PsTooltip on the
    ' Parameters field itself (see frmUserTools_Show), where they are read by hovering the
    ' thing they apply to.

    ROWLABEL( L(232,"Working Folder") )

    ' ---- the shortcut row -------------------------------------------------------------
    ' The label, then the shortcut itself drawn as text where a field would be -- there is no
    ' control here, because the value is not editable in place. The Assign button is.
    dim as long yShortcut = y
    rcT.left = detailX : rcT.right = fieldX - AfxScaleX(FRMUSERTOOLS_GAP)
    rcT.top  = y       : rcT.bottom = y + rowH
    buffer.PaintText( L(349,"Shortcut"), @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

    dim as DWSTRING wszShortcut = frmUserTools_ShortcutText( gUTCurrent )
    dim as COLORREF clrShortcut = ghEditor.ForeColorText
    if len(wszShortcut) = 0 then
        wszShortcut = L(354,"(none)")
        clrShortcut = ghPopup.ForeColorDisabled
    elseif frmUserTools_ShortcutIsDead( gUTCurrent ) then
        ' Said in WORDS as well as in colour. The list marks the cell red; a reader who cannot
        ' see that gets nothing from it, and "this saves and never fires" is not something to
        ' encode in a hue alone.
        wszShortcut = wszShortcut & "   " & L(356,"This shortcut cannot fire -- reassign it.")
        clrShortcut = theme.compile.iconfail
    else
        ' ...and the same for a shortcut that is real but already spoken for. The assign dialog
        ' will not let one be created, so this only ever shows for a binding that pre-dates the
        ' check -- but that is exactly the case nothing else would report.
        dim as DWSTRING wszOwner = frmUserTools_ShortcutClash( gUTCurrent )
        if len(wszOwner) then
            wszShortcut = wszShortcut & "   " & wszOwner
            clrShortcut = theme.compile.iconfail
        end if
    end if
    rcT.left = fieldX : rcT.right = xRight - AfxScaleX(FRMUSERTOOLS_ASSIGN_W) - AfxScaleX(FRMUSERTOOLS_GAP)
    buffer.SetForeColor( clrShortcut )
    buffer.PaintText( wszShortcut, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
    buffer.SetForeColor( ghPanel.ForeColor )
    y += rowH

    ROWLABEL( L(359,"Action") )

    ' ---- the divider ------------------------------------------------------------------
    y += AfxScaleY(FRMUSERTOOLS_GAP)
    dim as RECT rcDiv
    rcDiv.left   = detailX
    rcDiv.right  = xRight
    rcDiv.top    = y
    rcDiv.bottom = y + AfxScaleY(1)
    buffer.SetBackColor( ghEditor.Divider )
    buffer.PaintRect( @rcDiv )
    buffer.SetBackColor( ghPanel.BackColor )
    y += AfxScaleY(FRMUSERTOOLS_GAP) + AfxScaleY(2)

    ' ---- the four option rows ---------------------------------------------------------
    ' A toggle's caption spans the WHOLE row less the toggle itself, not the label gutter --
    ' these read as sentences, and "Wait for the tool to finish before continuing" does not
    ' fit in 130px.
    #macro TOGGLELABEL( _text_ )
        rcT.left = detailX : rcT.right = xRight - AfxScaleX(56)
        rcT.top  = y       : rcT.bottom = y + rowH
        buffer.PaintText( _text_, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
        y += rowH
    #endmacro

    TOGGLELABEL( L(233,"Prompt for confirmation when Tool is invoked") )
    TOGGLELABEL( L(234,"Run minimized") )
    TOGGLELABEL( L(235,"Wait for Tool to complete before continuing") )
    TOGGLELABEL( L(236,"Display this item in the editor menu") )

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Position the shell's children. The dialog is fixed-size, so this runs once after creation.
' ========================================================================================
private sub frmUserTools_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long mx      = AfxScaleX(FRMUSERTOOLS_MARGIN)
    dim as long titleH  = AfxScaleY(FRMUSERTOOLS_TITLE_H)
    dim as long footerH = AfxScaleY(FRMUSERTOOLS_FOOTER_H)
    dim as long gap     = AfxScaleX(FRMUSERTOOLS_GAP)
    dim as long btnW    = AfxScaleX(FRMUSERTOOLS_BTN_W)
    dim as long btnH    = AfxScaleY(FRMUSERTOOLS_BTN_H)
    dim as long listW   = AfxScaleX(FRMUSERTOOLS_LIST_W)
    dim as long xRight   = rc.right - mx

    ' ---- the list's command strip, in the title band ----------------------------------
    dim as long cmdW   = AfxScaleX(FRMUSERTOOLS_CMD_W)
    dim as long glyphW = AfxScaleX(FRMUSERTOOLS_GLYPH_W)
    dim as long cmdY   = (titleH - btnH) \ 2
    dim as long x      = mx
    SetWindowPos( ghUTAdd, 0, x, cmdY, cmdW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    x += cmdW + gap
    SetWindowPos( ghUTDelete, 0, x, cmdY, cmdW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    x += cmdW + (gap * 2)
    SetWindowPos( ghUTUp, 0, x, cmdY, glyphW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    x += glyphW + AfxScaleX(4)
    SetWindowPos( ghUTDown, 0, x, cmdY, glyphW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the list ---------------------------------------------------------------------
    dim as long listH = rc.bottom - footerH - titleH
    if listH < 0 then listH = 0
    SetWindowPos( HWND_FRMUSERTOOLS_LIST, 0, mx, titleH, listW, listH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the detail column ------------------------------------------------------------
    dim as long detailX = mx + listW + AfxScaleX(FRMUSERTOOLS_DETAIL_GAP)
    dim as long fieldX  = detailX + AfxScaleX(FRMUSERTOOLS_LABEL_W)
    dim as long rowH    = AfxScaleY(FRMUSERTOOLS_ROWH)
    dim as long fieldH  = AfxScaleY(FRMUSERTOOLS_FIELD_H)
    dim as long browseW = AfxScaleX(FRMUSERTOOLS_BROWSE_W)
    dim as long fieldW  = xRight - fieldX
    dim as long y       = titleH + AfxScaleY(12)

    ' A field is vertically centred in its row; the row height is what the label is drawn
    ' into, so the two cannot drift apart.
    #macro PLACEFIELD( _h_, _w_ )
        SetWindowPos( _h_, 0, fieldX, y + (rowH - fieldH) \ 2, _w_, fieldH, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
    #endmacro

    PLACEFIELD( ghUTName, fieldW )
    y += rowH

    PLACEFIELD( ghUTCommand, fieldW - browseW - AfxScaleX(FRMUSERTOOLS_GAP) )
    SetWindowPos( ghUTBrowseExe, 0, xRight - browseW, y + (rowH - fieldH) \ 2, browseW, fieldH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    y += rowH

    PLACEFIELD( ghUTParams, fieldW )
    y += rowH

    PLACEFIELD( ghUTFolder, fieldW - browseW - AfxScaleX(FRMUSERTOOLS_GAP) )
    SetWindowPos( ghUTBrowseFolder, 0, xRight - browseW, y + (rowH - fieldH) \ 2, browseW, fieldH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    y += rowH

    ' The shortcut row carries no field -- the value is painted. Only the Assign button.
    dim as long assignW = AfxScaleX(FRMUSERTOOLS_ASSIGN_W)
    SetWindowPos( ghUTAssign, 0, xRight - assignW, y + (rowH - fieldH) \ 2, assignW, fieldH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    y += rowH

    ' The Action combo sizes itself to its WIDEST item, so picking never resizes it. It is
    ' right-aligned like every other control in this column rather than stretched to fieldW --
    ' PsComboBox has no "fill the space" mode and forcing one would defeat that sizing.
    dim as long cbW = 0, cbH = 0
    PsComboBox_GetIdealSize( ghUTAction, cbW, cbH )
    if cbW > fieldW then cbW = fieldW
    SetWindowPos( ghUTAction, 0, xRight - cbW, y + (rowH - cbH) \ 2, cbW, cbH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    y += rowH

    ' The divider, then the four toggles.
    y += (AfxScaleY(FRMUSERTOOLS_GAP) * 2) + AfxScaleY(1) + AfxScaleY(2)

    dim as long tw = 0, th = 0
    #macro PLACETOGGLE( _h_ )
        PsToggle_GetIdealSize( _h_, tw, th )
        SetWindowPos( _h_, 0, xRight - tw, y + (rowH - th) \ 2, tw, th, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
        y += rowH
    #endmacro

    PLACETOGGLE( ghUTTogPrompt )
    PLACETOGGLE( ghUTTogMinimized )
    PLACETOGGLE( ghUTTogWait )
    PLACETOGGLE( ghUTTogMenu )

    ' ---- footer -----------------------------------------------------------------------
    dim as long btnY = rc.bottom - footerH + (footerH - btnH) \ 2
    SetWindowPos( ghUTCancel, 0, xRight - btnW, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghUTOK, 0, xRight - (btnW * 2) - gap, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Push the current theme into every child that carries its own colours.
'
' Through the Options dialog's shared builders rather than a hand-copied recipe: this file is
' included after modOptionsRows.inc in the unity build, so unlike frmKeyboard it can simply
' call them and the palettes cannot drift.
' ========================================================================================
private sub frmUserTools_ApplyTheme( byval hwnd as HWND )

    PsListTree_SetBackColor( HWND_FRMUSERTOOLS_LIST, ghPanel.BackColor )
    PsListTree_SetScrollBarColors( HWND_FRMUSERTOOLS_LIST, ghPanel.BackColorScrollBar, _
                                   ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
    OptionsTheme_ApplyListBorder( HWND_FRMUSERTOOLS_LIST )

    OptionsTheme_ApplyTextBox( ghUTName )
    OptionsTheme_ApplyTextBox( ghUTCommand )
    OptionsTheme_ApplyTextBox( ghUTParams )
    OptionsTheme_ApplyTextBox( ghUTFolder )

    dim as PSTOGGLE_COLORS tc
    OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghUTTogPrompt,    @tc )
    PsToggle_SetColors( ghUTTogMinimized, @tc )
    PsToggle_SetColors( ghUTTogWait,      @tc )
    PsToggle_SetColors( ghUTTogMenu,      @tc )

    dim as PSCOMBOBOX_COLORS cc
    dim as PSPOPUPMENU_COLORS pc
    OptionsTheme_FillCombo( @cc, @pc )
    PsComboBox_SetColors( ghUTAction, @cc )
    PsComboBox_SetListColors( ghUTAction, @pc )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    dim as HWND hBtn(7) = { ghUTAdd, ghUTDelete, ghUTUp, ghUTDown, _
                            ghUTBrowseExe, ghUTBrowseFolder, ghUTAssign, ghUTOK }
    for i as long = 0 to 7
        if hBtn(i) = 0 then continue for
        PsButton_SetColors( hBtn(i), @bc )
        PsButton_Refresh( hBtn(i) )
    next
    PsButton_SetColors( ghUTCancel, @bc )
    PsButton_Refresh( ghUTCancel )

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' Is hCandidate hContainer, or anything nested inside it?
'
' Needed because every focusable thing on this dialog is a CONTAINER whose real tab stop is an
' inner child -- a PsTextBox's focus lives on the RichEdit two levels down, so
' GetFocus() = hTextBox is never true.
' ========================================================================================
private function frmUserTools_IsWithin( byval hCandidate as HWND, byval hContainer as HWND ) as boolean
    if (hCandidate = 0) orelse (hContainer = 0) then return false
    dim as HWND h = hCandidate
    do while h <> 0
        if h = hContainer then return true
        h = GetParent( h )
    loop
    return false
end function


' ========================================================================================
' A child's rect in the DIALOG's client coordinates.
'
' The self-test compares controls against the shell's own bands, so every measurement has to
' be in one coordinate space. GetWindowRect gives screen coordinates; two ScreenToClient calls
' bring both corners back. A HIDDEN window still reports its last placed rect here, which is
' why the fit assertions also check the rect is non-empty -- a zeroed rect "fits inside"
' everything and would pass vacuously.
' ========================================================================================
private function frmUserTools_ChildRect( byval hChild as HWND ) as RECT
    dim as RECT rc
    if hChild = 0 then return rc
    GetWindowRect( hChild, @rc )
    dim as POINT pt1 = type( rc.left, rc.top )
    dim as POINT pt2 = type( rc.right, rc.bottom )
    ScreenToClient( HWND_FRMUSERTOOLS, @pt1 )
    ScreenToClient( HWND_FRMUSERTOOLS, @pt2 )
    rc.left = pt1.x : rc.top = pt1.y
    rc.right = pt2.x : rc.bottom = pt2.y
    return rc
end function


' ========================================================================================
' DOES THE DETAIL COLUMN FIT, AND DOES THE STAGING COPY BEHAVE?  (TIKO_USERTOOLS_SELFTEST=1)
'
' Runs from frmUserTools_Show once the controls exist and the list has been filled, so the
' layout half measures the real windows at the real DPI rather than the unscaled constants.
'
' THE FIT HALF EXISTS BECAUSE THE DIALOG IS FIXED SIZE. Nothing here scrolls, so a detail row
' pushed past the viewport does not grow a scrollbar -- the last toggle simply falls off the
' bottom edge, silently, and only above 100% DPI where nobody is looking. Same reason
' frmOptions and frmKeyboard carry one.
' ========================================================================================
sub frmUserTools_RunSelfTest()

    if environ("TIKO_USERTOOLS_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro UTASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? "==== TIKO_USERTOOLS_SELFTEST ===="

    ' ---- 1. the shell -----------------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( HWND_FRMUSERTOOLS, @rcClient )
    dim as long cw = rcClient.right, ch = rcClient.bottom
    UTASSERT( cw = AfxScaleX(FRMUSERTOOLS_CLIENT_W), "client width is the declared width" )
    UTASSERT( ch = AfxScaleY(FRMUSERTOOLS_CLIENT_H), "client height is the declared height" )
    ' The dialog must NOT be resizable: every fit assertion below is meaningless if the user
    ' can drag the window smaller than the layout it was measured against.
    dim as long dwStyle = cast(long, GetWindowLongPtr( HWND_FRMUSERTOOLS, GWL_STYLE ))
    UTASSERT( (dwStyle and WS_THICKFRAME) = 0, "the dialog carries no WS_THICKFRAME" )

    ' ---- 2. the list ------------------------------------------------------------------
    dim as RECT rcList = frmUserTools_ChildRect( HWND_FRMUSERTOOLS_LIST )
    UTASSERT( rcList.top = AfxScaleY(FRMUSERTOOLS_TITLE_H), "the list starts below the title band" )
    UTASSERT( rcList.bottom <= ch - AfxScaleY(FRMUSERTOOLS_FOOTER_H), "the list clears the footer" )
    UTASSERT( PsListTree_GetColumnCount( HWND_FRMUSERTOOLS_LIST ) = 2, "the list has two columns" )

    ' The columns must fit the list's own width. They cannot grow a horizontal scrollbar, so
    ' an over-wide set simply loses the Shortcut column off the right edge.
    dim as long colTotal = 0
    for i as long = 0 to PsListTree_GetColumnCount( HWND_FRMUSERTOOLS_LIST ) - 1
        colTotal += PsListTree_GetColumnWidth( HWND_FRMUSERTOOLS_LIST, i )
    next
    dim as long listW = rcList.right - rcList.left
    UTASSERT( colTotal <= listW, "the two columns fit the list width (" & str(colTotal) & " <= " & str(listW) & ")" )

    ' ---- 3. THE DETAIL COLUMN FITS ----------------------------------------------------
    ' Measured off the real windows -- the LAST toggle's bottom edge against the footer. This
    ' is the assertion the whole fixed-size design exists to make possible.
    dim as RECT rcLast = frmUserTools_ChildRect( ghUTTogMenu )
    dim as long yLimit = ch - AfxScaleY(FRMUSERTOOLS_FOOTER_H)
    UTASSERT( rcLast.bottom <= yLimit, _
              "the last option row clears the footer (" & str(rcLast.bottom) & " <= " & str(yLimit) & ")" )

    ' ...and that it is genuinely INSIDE the viewport rather than merely above the footer:
    ' a hidden control reports a zeroed rect, which "fits inside" everything. This is the
    ' precondition that stops the assertion above passing vacuously.
    UTASSERT( rcLast.bottom > rcLast.top, "the last option row has a real height" )
    UTASSERT( rcLast.top >= AfxScaleY(FRMUSERTOOLS_TITLE_H), "the last option row starts below the title band" )

    ' Nothing in the detail column may overlap the list.
    dim as RECT rcName = frmUserTools_ChildRect( ghUTName )
    UTASSERT( rcName.left > rcList.right, "the detail column starts xRight of the list" )
    UTASSERT( rcName.right <= cw - AfxScaleX(FRMUSERTOOLS_MARGIN), "the name field clears the xRight margin" )

    ' The Action combo must not have grown past its column. PsComboBox sizes itself to its
    ' WIDEST item, and the fourth of those is a long sentence, so this is the row most likely
    ' to overflow when a translation is longer than the English.
    dim as RECT rcCombo = frmUserTools_ChildRect( ghUTAction )
    UTASSERT( rcCombo.left > rcList.right, "the Action combo stays out of the list column" )
    UTASSERT( rcCombo.right <= cw - AfxScaleX(FRMUSERTOOLS_MARGIN), "the Action combo clears the xRight margin" )

    ' ---- 4. the shortcut composer -----------------------------------------------------
    ' Driven against the staging array directly, which is why the array is saved and restored
    ' around it: this runs on a live dialog and must leave the user's tools untouched.
    dim as TYPE_TOOLS savedTools(any)
    dim as long nSavedUB = ubound(gToolsWork)
    if nSavedUB >= 0 then
        redim savedTools(nSavedUB)
        for i as long = 0 to nSavedUB
            savedTools(i) = gToolsWork(i)
        next
    end if

    redim gToolsWork(0)
    gToolsWork(0).wszKey = "F5"
    gToolsWork(0).IsCtrl = 1 : gToolsWork(0).IsShift = 0 : gToolsWork(0).IsAlt = 0
    UTASSERT( frmUserTools_ShortcutText(0) = "Ctrl+F5", "Ctrl+F5 composes" )
    gToolsWork(0).IsShift = 1 : gToolsWork(0).IsAlt = 1
    ' MODIFIER ORDER IS PINNED. It is what createToolsMenuShortcut emits into the Tools menu,
    ' and the two strings sit inches apart on screen.
    UTASSERT( frmUserTools_ShortcutText(0) = "Ctrl+Shift+Alt+F5", "the modifier order is Ctrl, Shift, Alt" )

    ' A keyless tool composes to NOTHING, modifiers or not. The version this replaces wrote
    ' the literal "None" into wszKey on every selection change and persisted it, which is what
    ' put "Ctrl+NONE" beside a tool in the menu.
    gToolsWork(0).wszKey = ""
    UTASSERT( frmUserTools_ShortcutText(0) = "", "a keyless tool composes to an empty string" )
    gToolsWork(0).wszKey = "None"
    UTASSERT( frmUserTools_ShortcutText(0) = "", "the persisted 'None' affordance composes to an empty string" )
    ' ...and is NOT reported as broken. "No shortcut" is a legal state; only an unresolvable
    ' name is a dead one.
    UTASSERT( frmUserTools_ShortcutIsDead(0) = false, "'None' is not a dead shortcut" )
    gToolsWork(0).wszKey = ""
    UTASSERT( frmUserTools_ShortcutIsDead(0) = false, "an empty key is not a dead shortcut" )

    ' THE DEAD-KEY TEST ITSELF. "PageDn" is the exact name the shipped pick-list once offered
    ' against a resolver that knows only "PGDN" -- a shortcut that saved, displayed and never
    ' fired. It must be reported, and the canonical spelling must not be.
    gToolsWork(0).wszKey = "PageDn"
    UTASSERT( frmUserTools_ShortcutIsDead(0) = true, "an unresolvable key name is reported dead" )
    gToolsWork(0).wszKey = "PGDN"
    UTASSERT( frmUserTools_ShortcutIsDead(0) = false, "the canonical spelling is not reported dead" )

    ' EVERY NAME THE ASSIGN DIALOG CAN PRODUCE RESOLVES. The picker offers gKeyNames verbatim
    ' and the capture field spells its keys with KeyBindings_VKToName, which draws from the
    ' same table -- so this is the assertion that says the dialog cannot MAKE a dead shortcut,
    ' as opposed to merely reporting the legacy ones.
    KeyBindings_EnsureKeyNames()
    dim as long nUnresolved = 0
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        gToolsWork(0).wszKey = gKeyNames(i).wszName
        if frmUserTools_ShortcutIsDead(0) then nUnresolved += 1
    next
    UTASSERT( nUnresolved = 0, "every pickable key name resolves (" & str(nUnresolved) & " did not)" )

    ' ---- 4b. THE CLASH RULE -----------------------------------------------------------
    ' Driven against the LIVE editor bindings, so it needs a keystroke tiko actually ships.
    ' Found rather than hardcoded: a hardcoded "Ctrl+S" would silently start testing nothing
    ' the day someone rebinds Save, and the assertion would stay green.
    dim as DWSTRING wszLiveKeys, wszLiveOwner
    for i as long = lbound(gKeys) to ubound(gKeys)
        dim as DWSTRING wszEff
        if len(gKeys(i).wszUserKeys) then
            wszEff = gKeys(i).wszUserKeys
        elseif gKeys(i).bDefaultDisabled = false then
            wszEff = gKeys(i).wszDefaultKeys
        end if
        ' A single-modifier binding, so the parts are unambiguous to reconstruct below.
        if left(ucase(wszEff),5) = "CTRL+" then
            if instr(mid(ucase(wszEff),6), "+") = 0 then
                wszLiveKeys  = mid(wszEff, 6)
                wszLiveOwner = gKeys(i).wszDescription
                exit for
            end if
        end if
    next

    UTASSERT( len(wszLiveKeys) > 0, "found a live Ctrl+<key> editor binding to test against" )
    if len(wszLiveKeys) then
        ' It clashes...
        dim as DWSTRING wszHit = frmAssignKey_ClashText( wszLiveKeys, true, false, false, _
                                                         ASSIGNKEY_DOMAIN_NONE, -1 )
        UTASSERT( len(wszHit) > 0, "Ctrl+" & wszLiveKeys & " is reported as taken by an editor command" )
        UTASSERT( instr(wszHit, wszLiveOwner) > 0, "the clash names the command that owns it" )

        ' ...and the SAME KEY WITHOUT THE MODIFIER DOES NOT. This is the assertion that stops
        ' the one above passing for the wrong reason: a clash test that ignored the modifiers
        ' entirely -- comparing bare key names -- would report a hit here too.
        dim as DWSTRING wszFree = frmAssignKey_ClashText( wszLiveKeys, false, true, true, _
                                                          ASSIGNKEY_DOMAIN_NONE, -1 )
        UTASSERT( len(wszFree) = 0, "Alt+Shift+" & wszLiveKeys & " is NOT reported as taken" )
    end if

    ' TOOL VERSUS TOOL, and the skip-self rule. Two staging rows carrying the identical
    ' binding: each must see the other, and neither may see itself.
    redim gToolsWork(1)
    gToolsWork(0).wszDescription = "AAA" : gToolsWork(0).wszKey = "F12" : gToolsWork(0).IsCtrl = 1
    gToolsWork(1).wszDescription = "BBB" : gToolsWork(1).wszKey = "F12" : gToolsWork(1).IsCtrl = 1
    UTASSERT( instr( frmAssignKey_ClashText("F12", true, false, false, ASSIGNKEY_DOMAIN_TOOLS, 0), "BBB" ) > 0, _
              "a tool sees the OTHER tool holding its shortcut" )
    UTASSERT( instr( frmAssignKey_ClashText("F12", true, false, false, ASSIGNKEY_DOMAIN_TOOLS, 0), "AAA" ) = 0, _
              "a tool does not report clashing with itself" )
    ' Differing by one modifier is not a clash.
    gToolsWork(1).IsShift = 1
    UTASSERT( len( frmAssignKey_ClashText("F12", true, false, false, ASSIGNKEY_DOMAIN_TOOLS, 0) ) = 0, _
              "Ctrl+F12 and Ctrl+Shift+F12 are not a clash" )
    ' A keyless tool cannot clash with anything, however its modifier flags are left.
    UTASSERT( len( frmAssignKey_ClashText("", true, false, false, ASSIGNKEY_DOMAIN_TOOLS, -1) ) = 0, _
              "a tool with no key never clashes" )

    ' ---- 4c. TOOLS AND BUILDS MUST SEE EACH OTHER --------------------------------------
    ' The whole reason frmUserToolKey was generalized into frmAssignKey, and nothing else in
    ' any suite tests it: before this, the clash test knew only about user tools, so a tool
    ' and a build configuration could hold the identical accelerator and BOTH dialogs would
    ' report the keystroke free. One of the two would then simply never fire.
    '
    ' Driven through the COMMITTED arrays on purpose -- that is the production path. A
    ' domain's own staging array is read only while its dialog is open, and the two dialogs
    ' cannot be open at once, so the cross-domain half always reads gConfig.
    scope
        dim as TYPE_TOOLS  keepTools(any)
        dim as TYPE_BUILDS keepBuilds(any)
        dim as long nT = ubound(gConfig.Tools), nB = ubound(gConfig.Builds)
        if nT >= 0 then
            redim keepTools(nT)
            for i as long = 0 to nT : keepTools(i) = gConfig.Tools(i) : next
        end if
        if nB >= 0 then
            redim keepBuilds(nB)
            for i as long = 0 to nB : keepBuilds(i) = gConfig.Builds(i) : next
        end if

        erase gToolsWork : erase gBuildsWork
        erase gConfig.Tools : erase gConfig.Builds

        ' FIND a keystroke nothing in the editor owns, rather than hardcoding one. The first
        ' version of this section used Ctrl+F11 and Ctrl+F10 and BOTH FAILED, because tiko
        ' binds them: part 1 answered with the editor command and the domain routing under
        ' test was never reached. Even a keystroke that happens to be free today would start
        ' testing nothing the day it got bound, and the assertion would stay green.
        ' Three modifiers, which the editor's own defaults use sparingly.
        dim as DWSTRING wszCandidates = "F11" & chr(9) & "F10" & chr(9) & "F9" & chr(9) & _
                                        "F8"  & chr(9) & "F7"  & chr(9) & "F6"
        dim as DWSTRING wszXKey
        for i as long = 1 to AfxStrParseCount( wszCandidates, chr(9) )
            dim as DWSTRING wszTry = AfxStrParse( wszCandidates, i, chr(9) )
            if len( frmAssignKey_ClashText( wszTry, true, true, true, _
                                            ASSIGNKEY_DOMAIN_NONE, -1 ) ) = 0 then
                wszXKey = wszTry : exit for
            end if
        next
        UTASSERT( len(wszXKey) > 0, "found an unbound keystroke to test cross-domain routing with" )

        if len(wszXKey) then
            ' A committed BUILD holding it is seen by a TOOLS-domain query, whose tools half
            ' reads the (empty) staging array and whose builds half reads gConfig.
            redim gConfig.Builds(0)
            gConfig.Builds(0).wszDescription = "ZZBUILD"
            gConfig.Builds(0).wszKey         = wszXKey
            gConfig.Builds(0).IsCtrl         = 1
            gConfig.Builds(0).IsAlt          = 1
            gConfig.Builds(0).IsShift        = 1
            UTASSERT( instr( frmAssignKey_ClashText(wszXKey, true, true, true, _
                             ASSIGNKEY_DOMAIN_TOOLS, -1), "ZZBUILD" ) > 0, _
                      "a tool sees a BUILD holding its shortcut" )

            ' And the mirror.
            redim gConfig.Tools(0)
            gConfig.Tools(0).wszDescription = "ZZTOOL"
            gConfig.Tools(0).wszKey         = wszXKey
            gConfig.Tools(0).IsCtrl         = 1
            gConfig.Tools(0).IsAlt          = 1
            gConfig.Tools(0).IsShift        = 1
            UTASSERT( instr( frmAssignKey_ClashText(wszXKey, true, true, true, _
                             ASSIGNKEY_DOMAIN_BUILDS, -1), "ZZTOOL" ) > 0, _
                      "a build sees a TOOL holding its shortcut" )

            ' Modifiers still matter across domains -- what stops the two above passing for
            ' the wrong reason. The QUERY is held fixed at the combination already proven
            ' free of any editor binding, and only the STORED record varies, so this cannot
            ' accidentally be re-testing part 1 either.
            gConfig.Tools(0).IsShift  = 0
            gConfig.Builds(0).IsShift = 0
            UTASSERT( len( frmAssignKey_ClashText(wszXKey, true, true, true, _
                           ASSIGNKEY_DOMAIN_BUILDS, -1) ) = 0, _
                      "a stored record differing by one modifier is not a cross-domain clash" )
        end if

        erase gConfig.Tools : erase gConfig.Builds
        if nT >= 0 then
            redim gConfig.Tools(nT)
            for i as long = 0 to nT : gConfig.Tools(i) = keepTools(i) : next
        end if
        if nB >= 0 then
            redim gConfig.Builds(nB)
            for i as long = 0 to nB : gConfig.Builds(i) = keepBuilds(i) : next
        end if
    end scope

    ' Restore.
    erase gToolsWork
    if nSavedUB >= 0 then
        redim gToolsWork(nSavedUB)
        for i as long = 0 to nSavedUB
            gToolsWork(i) = savedTools(i)
        next
    end if

    ' ---- 5. staging is genuinely separate ---------------------------------------------
    ' The whole point of gToolsWork: Cancel must be a no-op. Assert the two arrays are
    ' distinct storage rather than trusting the redim above to have made a copy.
    if (ubound(gToolsWork) >= 0) andalso (ubound(gConfig.Tools) >= 0) then
        dim as DWSTRING wszWas = gConfig.Tools(0).wszDescription
        gToolsWork(0).wszDescription = wszWas & "##selftest##"
        UTASSERT( gConfig.Tools(0).wszDescription = wszWas, "editing staging does not touch gConfig.Tools" )
        gToolsWork(0).wszDescription = wszWas
    end if

    ' ---- 6. tab order -----------------------------------------------------------------
    ' BY CONTAINMENT, not by handle: every focusable control here is a container whose real
    ' tab stop is an inner child, so GetNextDlgTabItem returns something INSIDE the control
    ' rather than the handle this file holds.
    dim as HWND hFirst = GetNextDlgTabItem( HWND_FRMUSERTOOLS, 0, false )
    UTASSERT( hFirst <> 0, "the dialog has at least one tab stop" )

    dim as long nStops = 0
    dim as boolean bSawList = false, bSawName = false, bSawOK = false
    dim as HWND h = hFirst
    do
        if h = 0 then exit do
        nStops += 1
        if frmUserTools_IsWithin( h, HWND_FRMUSERTOOLS_LIST ) then bSawList = true
        if frmUserTools_IsWithin( h, ghUTName ) then bSawName = true
        if frmUserTools_IsWithin( h, ghUTOK ) then bSawOK = true
        h = GetNextDlgTabItem( HWND_FRMUSERTOOLS, h, false )
        if nStops > 40 then exit do          ' a cycle that never returns to hFirst
    loop until h = hFirst

    UTASSERT( bSawList, "the tools list is in the tab order" )
    UTASSERT( bSawName, "the Tool Name field is in the tab order" )
    UTASSERT( bSawOK,   "the OK button is in the tab order" )
    UTASSERT( nStops <= 40, "the tab order is a closed cycle" )

    ' ---- 7. the detail column really is hidden when nothing is selected ---------------
    ' Exercised rather than reasoned about: LoadDetail(-1) then back. The list is left on the
    ' row it started on.
    dim as long nWas = gUTCurrent
    frmUserTools_LoadDetail( -1 )
    UTASSERT( IsWindowVisible( ghUTName ) = 0, "the name field hides when nothing is selected" )
    UTASSERT( IsWindowVisible( ghUTTogMenu ) = 0, "the last toggle hides when nothing is selected" )
    UTASSERT( IsWindowEnabled( ghUTDelete ) = 0, "Delete is disabled when nothing is selected" )
    frmUserTools_LoadDetail( nWas )
    if nWas >= 0 then
        UTASSERT( IsWindowVisible( ghUTName ) <> 0, "the name field comes back with a selection" )
    end if

    ? "==== USERTOOLS: " & str(nPass) & " passed, " & str(nFail) & " failed ===="

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmUserTools_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

        case WM_CREATE
            ' Modal: keep the rest of the app out while this is up.
            EnableWindow( GetParent(hwnd), false )
            return 0

        case WM_ERASEBKGND
            ' The whole client is painted in WM_PAINT through one buffer, so letting the
            ' system erase first would only flicker.
            return 1

        case WM_PAINT
            return frmUserTools_OnPaint( hwnd )

        case WM_SIZE
            frmUserTools_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes through its click callback, not WM_COMMAND. What still arrives
            ' here is IDCANCEL, which IsDialogMessage synthesises for the Escape key.
            select case loword(wParam)
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case MSG_USER_SHOW_TOOLKEY
            frmUserTools_DoAssign()
            return 0

        case WM_CLOSE
            ' NOTHING IS SAVED HERE. The commit is on the OK path only, so reaching this by
            ' any route discards gToolsWork() and leaves gConfig.Tools, settings.ini and the
            ' accelerator table exactly as they were found. The dialog this replaces had no
            ' Cancel at all -- its only button saved.
            ' Enables parent window keeping parent's zorder
            EnableWindow(GetParent(hwnd), true)
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMUSERTOOLS      = 0
            HWND_FRMUSERTOOLS_LIST = 0
            ghUTAdd = 0 : ghUTDelete = 0 : ghUTUp = 0 : ghUTDown = 0
            ghUTName = 0 : ghUTCommand = 0 : ghUTParams = 0 : ghUTFolder = 0
            ghUTBrowseExe = 0 : ghUTBrowseFolder = 0 : ghUTAssign = 0
            ghUTAction = 0
            ghUTTogPrompt = 0 : ghUTTogMinimized = 0 : ghUTTogWait = 0 : ghUTTogMenu = 0
            ghUTOK = 0 : ghUTCancel = 0
            ' The tip is a WS_POPUP OWNED by this window, not a child of it. Windows destroys
            ' owned popups with their owner, so this is belt and braces rather than the only
            ' route -- but it is what releases the shared poll clock deterministically instead
            ' of at whatever point the implicit teardown reaches it. IsWindow, because by the
            ' time this arrives the implicit path may already have run.
            if ghUTParamsTip then
                if IsWindow( ghUTParamsTip ) then DestroyWindow( ghUTParamsTip )
            end if
            ghUTParamsTip = 0
            gUTCurrent = -1
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmUserTools_Show
' ========================================================================================
function frmUserTools_Show( byval hWndParent as HWND ) as LRESULT

    ' Seed the staging copy BEFORE any control exists -- everything below is built from
    ' gToolsWork(), and gConfig.Tools() is not touched again unless OK commits.
    frmUserTools_LoadWork()
    gUTCurrent = -1
    gUTLoading = false

    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMUSERTOOLS = pWindow->Create( hWndParent, L(229,"User Tools"), _
        @frmUserTools_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMUSERTOOLS_CLIENT_W, FRMUSERTOOLS_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- CREATION ORDER IS THE TAB ORDER ----------------------------------------------
    '
    ' These controls live on an ordinary CWindow, not a dialog template, so there is no
    ' ordinal to set: GetNextDlgTabItem walks the child Z-ORDER, and Z-order is creation
    ' order. The order built here is
    '
    '     list -> Name -> Command -> [...] -> Parameters -> Folder -> [...] -> Assign
    '          -> Action -> the four toggles -> Add -> Delete -> Up -> Down -> OK -> Cancel
    '
    ' i.e. the LIST AND THE DETAIL COLUMN FIRST, in reading order, and the commands after --
    ' Tab should walk the work, not the chrome. Moving any block past another silently
    ' reorders the dialog.

    ' ---- the list ---------------------------------------------------------------------
    HWND_FRMUSERTOOLS_LIST = PsListTree_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_LIST )

    ' Column 0's width is a placeholder -- it is the FILL column and takes whatever the
    ' Shortcut column leaves.
    PsListTree_AddColumn( HWND_FRMUSERTOOLS_LIST, L(360,"Tool"), 100 )
    PsListTree_AddColumn( HWND_FRMUSERTOOLS_LIST, L(349,"Shortcut"), pWindow->ScaleX(FRMUSERTOOLS_COL_SHORTCUT) )
    PsListTree_SetFillColumn( HWND_FRMUSERTOOLS_LIST, 0 )      ' the tool name absorbs the slack
    PsListTree_ShowHeader( HWND_FRMUSERTOOLS_LIST, true )

    PsListTree_SetPaintCallback( HWND_FRMUSERTOOLS_LIST, @frmUserTools_ListPaintRow )
    PsListTree_SetHeaderPaintCallback( HWND_FRMUSERTOOLS_LIST, @frmUserTools_ListPaintHeader )
    PsListTree_SetSelChangeCallback( HWND_FRMUSERTOOLS_LIST, @frmUserTools_ListSelChange )
    PsListTree_SetActivateCallback( HWND_FRMUSERTOOLS_LIST, @frmUserTools_ListActivate )
    PsListTree_SetTooltipCallback( HWND_FRMUSERTOOLS_LIST, @frmUserTools_ListTooltip )
    PsListTree_SetHeaderTooltipCallback( HWND_FRMUSERTOOLS_LIST, @frmUserTools_HeaderTooltip )
    PsListTree_SetFont( HWND_FRMUSERTOOLS_LIST, ghFont(GUIFONT_11) )
    ' UNSCALED: PsListTree scales what it is handed, so a pre-scaled value is scaled twice.
    PsListTree_SetRowHeight( HWND_FRMUSERTOOLS_LIST, FRMUSERTOOLS_ROW_H )

    ' ---- the detail fields ------------------------------------------------------------
    ' LEFT PADDING IS SCALED HERE, BY THE HOST. PsTextBox documents its margins as RAW pixels
    ' and passes them verbatim to EM_SETMARGINS -- deliberately, because scaling them control-
    ' side would lie to EM_GETMARGINS readers and double-scale hosts that already scale. So
    ' AfxScaleX is ours to apply, unlike SetRowHeight on PsListTree, which is the opposite rule
    ' in the same family.
    #macro MAKEFIELD( _h_, _id_ )
        _h_ = PsTextBox_Create( HWND_FRMUSERTOOLS, _id_ )
        PsTextBox_SetFont( _h_, ghFont(GUIFONT_11) )
        PsTextBox_SetMargins( _h_, AfxScaleX(8), AfxScaleX(6) )
        PsTextBox_SetChangeCallback( _h_, @frmUserTools_TextChanged )
    #endmacro

    MAKEFIELD( ghUTName, IDC_FRMUSERTOOLS_TXTNAME )

    MAKEFIELD( ghUTCommand, IDC_FRMUSERTOOLS_TXTCOMMAND )
    ghUTBrowseExe = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDBROWSEEXE )
    PsButton_SetText( ghUTBrowseExe, "..." )
    PsButton_SetFont( ghUTBrowseExe, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghUTBrowseExe, @frmUserTools_ButtonClick )

    MAKEFIELD( ghUTParams, IDC_FRMUSERTOOLS_TXTPARAMETERS )

    ' ---- the substitution-code tip ----------------------------------------------------
    '
    ' A PsTooltip, and the first one in tiko. Three reasons it is not the comctl32 tooltip the
    ' rest of the editor uses:
    '
    '   1. IT WRAPS. A comctl32 tip is single-line until the host hand-sends
    '      TTM_SETMAXTIPWIDTH through the control's tooltip handle -- which tiko does in four
    '      places, one of them inside a per-tip callback. PsTooltip takes a max width and a
    '      title band as ordinary properties.
    '   2. IT DOES NOT SUBCLASS the window it serves. The target here is a PsTextBox, i.e. a
    '      container whose real input window is a RichEdit two levels down; a TTF_SUBCLASS tool
    '      on the container would never see the mouse, because the child has it.
    '      PsTooltip polls WindowFromPoint and accepts a hit on any DESCENDANT of the attached
    '      window, so the container is the right thing to attach to.
    '   3. IT ADDS NO PUMP OBLIGATION. This dialog already owes two filter calls; a third would
    '      have been the wrong direction.
    '
    ' Create ALSO attaches -- there is no separate PsTooltip_Attach call needed here.
    ghUTParamsTip = PsTooltip_Create( ghUTParams )
    PsTooltip_SetTitle( ghUTParamsTip, L(358,"Parameters") )
    ' ONE LOCALIZED ID PER LINE, joined here. A .lang entry is a single line of a UTF-16 text
    ' file and cannot carry an embedded newline, so a four-line tip cannot be one id -- and
    ' splitting it this way also lets a translator size each line independently.
    ' PsTooltip draws the message with PaintTextEx (no DT_SINGLELINE), so DrawText honours the
    ' line feeds; the max width wraps anything still too long for the widest of them.
    PsTooltip_SetText( ghUTParamsTip, _
        L(355,"<P>   project name")            & !"\n" & _
        L(362,"<S>   main source file")        & !"\n" & _
        L(363,"<W>   word at the caret")       & !"\n" & _
        L(364,"<E>   compiled exe / dll / lib") )
    ' 400, not 320: at 320 the longest line wrapped to a two-word orphan ("/ lib") on its own
    ' row, which reads as a fifth code. Measured off the render, not guessed.
    PsTooltip_SetMaxWidth( ghUTParamsTip, 400 )
    PsTooltip_SetFonts( ghUTParamsTip, ghFont(GUIFONT_11), ghFont(GUIFONTBOLD_11), ghFont(SYMBOLFONT_10) )
    scope
        dim as PSTOOLTIP_COLORS tipc
        tipc.BackColor   = ghPopup.BackColor
        tipc.BorderColor = ghEditor.Divider
        tipc.ForeColor   = ghPopup.ForeColor
        tipc.TitleColor  = ghPanel.ForeColorHot
        tipc.GlyphColor  = ghUI.AccentColor
        PsTooltip_SetColors( ghUTParamsTip, @tipc )
    end scope

    MAKEFIELD( ghUTFolder, IDC_FRMUSERTOOLS_TXTFOLDER )
    ghUTBrowseFolder = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDBROWSEFOLDER )
    PsButton_SetText( ghUTBrowseFolder, "..." )
    PsButton_SetFont( ghUTBrowseFolder, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghUTBrowseFolder, @frmUserTools_ButtonClick )

    ghUTAssign = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDASSIGN )
    PsButton_SetText( ghUTAssign, L(342,"Assign Shortcut") )
    PsButton_SetFont( ghUTAssign, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghUTAssign, @frmUserTools_ButtonClick )

    ghUTAction = PsComboBox_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_COMBOACTION )
    PsComboBox_SetFont( ghUTAction, ghFont(GUIFONT_11) )
    ' ORDER IS THE PERSISTED Action VALUE -- see USERTOOL_ACTION_* in the .bi. These four
    ' cannot be re-ordered for readability without rewriting every settings.ini in the world.
    PsComboBox_AddItem( ghUTAction, L(240,"Invoke only when selected by user") )
    PsComboBox_AddItem( ghUTAction, L(239,"Invoke during pre-compile") )
    PsComboBox_AddItem( ghUTAction, L(238,"Invoke during post-compile") )
    PsComboBox_AddItem( ghUTAction, L(265,"Invoke immediately after editor starts") )
    PsComboBox_SetCurSel( ghUTAction, USERTOOL_ACTION_SELECTED )
    PsComboBox_SetSelChangeCallback( ghUTAction, @frmUserTools_ActionChanged )

    #macro MAKETOGGLE( _h_, _id_ )
        _h_ = PsToggle_Create( HWND_FRMUSERTOOLS, _id_ )
        PsToggle_SetCheckChangedCallback( _h_, @frmUserTools_ToggleChanged )
    #endmacro

    MAKETOGGLE( ghUTTogPrompt,    IDC_FRMUSERTOOLS_TOGPROMPT )
    MAKETOGGLE( ghUTTogMinimized, IDC_FRMUSERTOOLS_TOGMINIMIZED )
    MAKETOGGLE( ghUTTogWait,      IDC_FRMUSERTOOLS_TOGWAIT )
    MAKETOGGLE( ghUTTogMenu,      IDC_FRMUSERTOOLS_TOGMENU )

    ' ---- the list's command strip -----------------------------------------------------
    ghUTAdd = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDADD )
    PsButton_SetText( ghUTAdd, L(350,"Add") )
    PsButton_SetFont( ghUTAdd, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghUTAdd, @frmUserTools_ButtonClick )

    ghUTDelete = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDDELETE )
    PsButton_SetText( ghUTDelete, L(223,"Delete") )
    PsButton_SetFont( ghUTDelete, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghUTDelete, @frmUserTools_ButtonClick )

    ' The two reorder buttons carry a GLYPH and no words: "Move up" beside "Move down" beside
    ' "Add" and "Delete" is four buttons of chrome over a list that has to fit in 340px. The
    ' meaning is a tooltip instead.
    '
    ' THE GLYPH IS THE CAPTION, NOT THE ICON, AND THAT IS DELIBERATE -- it departs from this
    ' workspace's usual rule that the symbol font is handed over as a SEPARATE paint-time font
    ' and never as the text font. PsButton PINS its icon cells to the padding and never moves
    ' them for the alignment (its own header says so, and PsComboBox pins its chevron the same
    ' way), so a glyph-only button built with SetGlyphLeft draws the glyph hard against the
    ' left padding, visibly off-centre in a 32px square. There is no icon alignment to set.
    ' Putting the glyph in the caption slot makes BTN_ALIGN_CENTER apply to it, which is the
    ' only route to a centred glyph -- and the convention it bends exists to stop a WORD being
    ' drawn in Segoe Fluent, which cannot happen when the caption IS a glyph.
    '
    ' The padding has to come down with it: the default is 12+12 of a 32px button, leaving 8px
    ' of caption span for a ~14px glyph. SetPadding stores RAW pixels (only the Create-time
    ' defaults are scaled by the control), so the scaling is ours.
    #macro MAKECHEVRON( _h_, _id_, _glyph_, _tip_ )
        _h_ = PsButton_Create( HWND_FRMUSERTOOLS, _id_ )
        PsButton_SetText( _h_, _glyph_ )
        PsButton_SetFont( _h_, ghFont(SYMBOLFONT_10) )
        PsButton_SetTextAlign( _h_, BTN_ALIGN_CENTER )
        PsButton_SetPadding( _h_, AfxScaleX(4), AfxScaleY(6), AfxScaleX(4), AfxScaleY(6) )
        PsButton_SetTooltipText( _h_, _tip_ )
        PsButton_SetClickCallback( _h_, @frmUserTools_ButtonClick )
    #endmacro

    MAKECHEVRON( ghUTUp,   IDC_FRMUSERTOOLS_CMDUP,   !"\uE70E", L(351,"Move up") )
    MAKECHEVRON( ghUTDown, IDC_FRMUSERTOOLS_CMDDOWN, !"\uE70D", L(352,"Move down") )

    ' ---- footer -----------------------------------------------------------------------
    ghUTOK = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDOK )
    PsButton_SetText( ghUTOK, L(0,"OK") )
    PsButton_SetFont( ghUTOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( ghUTOK, true )
    PsButton_SetClickCallback( ghUTOK, @frmUserTools_ButtonClick )

    ghUTCancel = PsButton_Create( HWND_FRMUSERTOOLS, IDC_FRMUSERTOOLS_CMDCANCEL )
    PsButton_SetText( ghUTCancel, L(1,"Cancel") )
    PsButton_SetFont( ghUTCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghUTCancel, @frmUserTools_ButtonClick )

    ' Only the always-present chrome is shown here. The detail column's visibility belongs to
    ' frmUserTools_LoadDetail, which BuildList calls -- showing them now would flash a page of
    ' empty fields when the user has no tools at all.
    ShowWindow( HWND_FRMUSERTOOLS_LIST, SW_SHOW )
    ShowWindow( ghUTAdd, SW_SHOW )
    ShowWindow( ghUTDelete, SW_SHOW )
    ShowWindow( ghUTUp, SW_SHOW )
    ShowWindow( ghUTDown, SW_SHOW )
    ShowWindow( ghUTOK, SW_SHOW )
    ShowWindow( ghUTCancel, SW_SHOW )

    frmUserTools_ApplyTheme( HWND_FRMUSERTOOLS )
    frmUserTools_PositionWindows( HWND_FRMUSERTOOLS )
    frmUserTools_BuildList()

    ShowWindow( HWND_FRMUSERTOOLS, SW_SHOW )
    UpdateWindow( HWND_FRMUSERTOOLS )

    ' Focus the list: the usual reason for opening this dialog is to run or edit a tool that
    ' already exists. It also has to be set explicitly for a second, unrelated reason --
    ' IsDialogMessage ignores Tab until a child already holds the focus, and an ordinary
    ' CWindow has no dialog manager to arrange that. (See the WS_EX_CONTROLPARENT section of
    ' C:\dev\Learnings.md.)
    SetFocus( HWND_FRMUSERTOOLS_LIST )

    ' AFTER the SetFocus above, deliberately: one of its assertions walks the tab order, and
    ' running it any earlier would sample it before anything had been focused at all.
    frmUserTools_RunSelfTest()

    ' Open the Assign Shortcut dialog straight away on the selected tool, so it can be looked
    ' at without a human clicking the button. Companion to TIKO_USERTOOLS_AUTOOPEN, and to
    ' frmKeyboard's TIKO_KEYBOARD_AUTOEDIT.
    if environ("TIKO_USERTOOLS_AUTOASSIGN") = "1" then
        PostMessage( HWND_FRMUSERTOOLS, MSG_USER_SHOW_TOOLKEY, 0, 0 )
    end if

    ' ---- message loop -----------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents: that has no *_FilterMessage hooks, and this dialog
    ' owes two of them.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. The four text fields each own a PsPopupMenu context menu; without this it
        ' has no keyboard navigation and never closes on an outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' MANDATORY, and it takes the CONTROL handle rather than working off a global -- so a
        ' second combo added to this dialog needs its own line here.
        if PsComboBox_FilterMessage( ghUTAction, @uMsg ) then continue while

        ' ESCAPE cancels -- handled explicitly, and AFTER the two popup filters above so an
        ' open context menu or dropdown closes on Escape first rather than the whole dialog
        ' going with it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMUSERTOOLS, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMUSERTOOLS, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    function = 0

end function
