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
' The shell's windows, and the staging copy.
'
' File-local. Nothing outside this dialog has any reason to reach a control on it, and the
' one thing that IS shared -- the resource/manifest creation -- goes through a function.
' ========================================================================================
dim shared HWND_FRMPROJECTOPTIONS   as HWND
dim shared ghPOPath                 as HWND
dim shared ghPOSelect               as HWND
dim shared ghPOCommandLine          as HWND
dim shared ghPOOther32              as HWND
dim shared ghPOOther64              as HWND
dim shared ghPOManifest             as HWND
dim shared ghPOOK                   as HWND
dim shared ghPOCancel               as HWND

dim shared gPOWork    as PROJECTOPTIONS_WORK
dim shared gPOLoading as boolean      ' true while seeding the controls: suppresses the callbacks


' ========================================================================================
' The derived geometry.
'
' PositionWindows WRITES this and OnPaint READS it -- the painter never recomputes an x or a
' y of its own. That is the frmBuildConfig rule, and it exists because the label column and
' the field column have to agree about where the boundary between them is: two independent
' walks of the same constants drift the moment one of them is edited and the other is not.
' ========================================================================================
type PROJECTOPTIONS_LAYOUT
    bValid      as boolean
    xLabel      as long                       ' left edge of the label gutter
    xField      as long                       ' left edge of the four fields
    xFieldRight as long                       ' right edge of the fields, "..." and the toggle
    xRight      as long                       ' the right margin
    rowH        as long
    yRow(0 to PO_ROW_COUNT-1)  as long        ' top of each row's stride
    yHdr(0 to PO_HDR_COUNT-1)  as long        ' top of each section header's caption
    yHint       as long                       ' the one hint line, under the Compiler header
    yFooter     as long                       ' top of the footer band (= the footer rule)
end type
dim shared gPOLayout as PROJECTOPTIONS_LAYOUT


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
        ' Parented to the Project Options dialog, which is up and modal at the one call site
        ' (frmProjectOptions_SaveProjectOptions) -- a box owned by a disabled ancestor comes up
        ' behind it. This was MessageBox(0), i.e. ownerless, which the Win32 box got away with.
        TikoMsgBox( HWND_FRMPROJECTOPTIONS, _
                    "The following files(s) are missing from the Settings folder. " + _
                    "Please reinstall the application." & vbcrlf & vbcrlf & _
                    wszFilesMissing, _
                    L(164,"Error"), TMB_ICON_WARNING, TMB_OK )
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
' Seed the staging copy from the live project.
'
' Runs BEFORE any control exists, so everything the dialog shows comes from gPOWork and
' gApp is not read again.
' ========================================================================================
private sub frmProjectOptions_LoadWork( byval IsNewProject as boolean )

    if IsNewProject then
        ' A new project starts empty except for the manifest, which defaults on -- the
        ' behaviour the dialog this replaces had.
        gPOWork.ProjectPath = ""
        gPOWork.CommandLine = ""
        gPOWork.Other32     = ""
        gPOWork.Other64     = ""
        gPOWork.Manifest    = true
    else
        gPOWork.ProjectPath = gApp.ProjectFilename
        gPOWork.CommandLine = gApp.ProjectCommandLine
        gPOWork.Other32     = gApp.ProjectOther32
        gPOWork.Other64     = gApp.ProjectOther64
        gPOWork.Manifest    = (gApp.ProjectManifest <> 0)
    end if

end sub


' ========================================================================================
' Write the staging copy into gApp. THE ONLY WRITER OF PROJECT STATE ON THE OK PATH.
'
' Split out of SaveProjectOptions purely so it can be ASSERTED. The rest of that function
' closes documents, shows message boxes and writes the project file, none of which a headless
' self-test can call -- which meant the regression this rebuild exists for (the two fields the
' old save blanked) had no test that actually touched the code under test. An assertion that
' cannot reach the thing it is named for is the shape this workspace keeps rediscovering.
'
' NOTE WHAT IS ABSENT: gApp.ProjectNotes. The dialog this replaces ended its save with
'
'     gApp.ProjectNotes       = ""
'     gApp.ProjectCommandLine = ""
'
' unconditionally, so every OK wiped the Output panel's Notes tab for the project and the run
' arguments set from frmMainCompile. Notes belongs to frmOutput and is not this dialog's to
' write; the command line now comes from the field that exists for it.
' ========================================================================================
private sub frmProjectOptions_CommitWork()

    gApp.IsProjectActive = true
    gApp.ProjectFilename = gPOWork.ProjectPath
    gApp.ProjectName     = AfxStrPathname( "NAMEX", gApp.ProjectFilename )

    gApp.ProjectOther32     = gPOWork.Other32
    gApp.ProjectOther64     = gPOWork.Other64
    gApp.ProjectManifest    = iif( gPOWork.Manifest, 1, 0 )
    gApp.ProjectCommandLine = gPOWork.CommandLine
    gApp.ProjectBuild       = frmBuildConfig_GetSelectedBuildGUID()

end sub


' ========================================================================================
' Save all options for the Project
'
' TWO FIELDS THIS FUNCTION USED TO DESTROY ARE NOW LEFT ALONE. It ended with
'
'     gApp.ProjectNotes       = ""
'     gApp.ProjectCommandLine = ""
'
' unconditionally, so opening this dialog and pressing OK wiped the Output panel's Notes tab
' for the project AND the run arguments set from frmMainCompile -- every time, with nothing
' reporting it. Notes is now simply not written here (it belongs to frmOutput), and the
' command line is written from the field that now exists for it.
' ========================================================================================
function frmProjectOptions_SaveProjectOptions( byval hwnd as HWND ) as BOOLEAN

    ' The staging copy is the source, not the controls -- the change callbacks keep it
    ' current, so there is nothing to harvest here.
    if len(gPOWork.ProjectPath) = 0 then
        ' Parented to THIS dialog, not frmMain: this window is up and modal, and a box owned by
        ' a disabled ancestor comes up behind it.
        TikoMsgBox( hwnd, L(180,"Invalid project path specified."), L(164,"Error"), _
                        TMB_ICON_INFO, TMB_OK )
        return False
    end if

    if gApp.IsNewProjectFlag then
        ' Need to close the current workspace at this point before loading and
        ' displaying the new project.
        if Workspace_SaveAndCloseAll(hwnd) = false then exit function
    end if

    ' Load the new or modified data into the Project variables
    frmProjectOptions_CommitWork()

    ' Create the resource file and manifest if applicable
    if gApp.ProjectManifest then frmProjectOptions_CreateResourceManifest()

    ' Save the actual Project data to disk.
    gConfig.ProjectSaveToFile()

    function = true
end function


' ========================================================================================
' The "..." browse button.
' ========================================================================================
private sub frmProjectOptions_DoBrowse()

    dim as DWSTRING wszProjectName = ""
    if gApp.IsNewProjectFlag = false then wszProjectName = gApp.ProjectName

    dim pwszName as WString Ptr = AfxIFileSaveDialog( HWND_FRMPROJECTOPTIONS, wszProjectName, _
                                                      @wstr(APPEXTENSION), IDC_FRMPROJECTOPTIONS_CMDSELECT )
    if pwszName then
        gPOWork.ProjectPath = *pwszName
        CoTaskMemFree pwszName
        ' Writing the control fires the change callback, which would write the same value
        ' straight back. Harmless, but the flag keeps the "only user edits move the model"
        ' rule true here as well as everywhere else.
        gPOLoading = true
        PsTextBox_SetText( ghPOPath, gPOWork.ProjectPath )
        gPOLoading = false
    end if

end sub


' ========================================================================================
' Button clicks. PsButton reports through this callback, not WM_COMMAND.
' ========================================================================================
private sub frmProjectOptions_ButtonClick( byval hButton as HWND, byval id as long )

    select case id
        case IDC_FRMPROJECTOPTIONS_CMDSELECT
            frmProjectOptions_DoBrowse()

        case IDC_FRMPROJECTOPTIONS_CMDOK
            if frmProjectOptions_SaveProjectOptions( HWND_FRMPROJECTOPTIONS ) then
                SendMessage( HWND_FRMPROJECTOPTIONS, WM_CLOSE, 0, 0 )
            end if

        case IDC_FRMPROJECTOPTIONS_CMDCANCEL
            ' OnCommand_ProjectNew reads IsNewProjectFlag to decide whether to go on and open
            ' what this dialog created. Cancel has to clear it, or a project that was never
            ' saved is opened from a filename that was never set.
            gApp.IsNewProjectFlag = false
            SendMessage( HWND_FRMPROJECTOPTIONS, WM_CLOSE, 0, 0 )
    end select

end sub


' ========================================================================================
' A field changed. The staging copy follows the controls; gApp does not.
' ========================================================================================
private sub frmProjectOptions_TextChanged( byval hTextBox as HWND )

    if gPOLoading then exit sub

    select case GetDlgCtrlID( hTextBox )
        case IDC_FRMPROJECTOPTIONS_TXTPROJECTPATH: gPOWork.ProjectPath = PsTextBox_GetText( hTextBox )
        case IDC_FRMPROJECTOPTIONS_TXTCOMMANDLINE: gPOWork.CommandLine = PsTextBox_GetText( hTextBox )
        case IDC_FRMPROJECTOPTIONS_TXTOPTIONS32:   gPOWork.Other32     = PsTextBox_GetText( hTextBox )
        case IDC_FRMPROJECTOPTIONS_TXTOPTIONS64:   gPOWork.Other64     = PsTextBox_GetText( hTextBox )
    end select

end sub


' ========================================================================================
' Push the staging copy into the controls.
'
' Split out of Show for the same reason CommitWork was split out of the save: so the
' self-test can DRIVE it. The first version of the three "the controls agree with the
' staging copy" assertions read the controls after Show had seeded them -- and an A/B proved
' they were VACUOUS in the one configuration they ever run in, because TIKO_..._AUTOOPEN
' starts with no project open, so the staged value and an unseeded field are both "" and
' agree for a reason that has nothing to do with the seeding. Deleting the seed left all
' three green. They now call this with sentinel values instead.
'
' gPOLoading is what stops the change callbacks writing the staging copy straight back --
' the family's "programmatic setters are silent" rule, enforced host-side because PsTextBox
' fires its change callback for a SetText the same as for a keystroke.
' ========================================================================================
private sub frmProjectOptions_SeedControls()

    gPOLoading = true
    PsTextBox_SetText( ghPOPath,        gPOWork.ProjectPath )
    PsTextBox_SetText( ghPOCommandLine, gPOWork.CommandLine )
    PsTextBox_SetText( ghPOOther32,     gPOWork.Other32 )
    PsTextBox_SetText( ghPOOther64,     gPOWork.Other64 )
    PsToggle_SetChecked( ghPOManifest,  gPOWork.Manifest )
    gPOLoading = false

end sub


private sub frmProjectOptions_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    if gPOLoading then exit sub
    gPOWork.Manifest = isChecked
end sub


' ========================================================================================
' Position the shell's children, and derive every rect the painter will read.
'
' The dialog is fixed-size, so this runs once after creation. It is still driven from WM_SIZE
' rather than called inline, because that is the one message guaranteed to arrive after the
' client area is real.
' ========================================================================================
private sub frmProjectOptions_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long mx      = AfxScaleX(FRMPROJECTOPTIONS_MARGIN)
    dim as long rowH    = AfxScaleY(FRMPROJECTOPTIONS_ROWH)
    dim as long fieldH  = AfxScaleY(FRMPROJECTOPTIONS_FIELD_H)
    dim as long browseW = AfxScaleX(FRMPROJECTOPTIONS_BROWSE_W)
    dim as long browseG = AfxScaleX(FRMPROJECTOPTIONS_BROWSE_GAP)
    dim as long hdrH    = AfxScaleY(FRMPROJECTOPTIONS_HDR_TEXT_H)

    gPOLayout.xLabel      = mx
    gPOLayout.xField      = mx + AfxScaleX(FRMPROJECTOPTIONS_LABEL_W)
    gPOLayout.xRight      = rc.right - mx
    ' ONE NUMBER FOR THE RIGHT EDGE. The browse gutter is charged on every row, including the
    ' rows that have no button and the toggle row, so the fields, the "..." and the toggle all
    ' end at the same x. Deriving it three times would let them drift.
    gPOLayout.xFieldRight = gPOLayout.xRight - browseW - browseG
    gPOLayout.rowH        = rowH
    gPOLayout.yFooter     = rc.bottom - AfxScaleY(FRMPROJECTOPTIONS_FOOTER_H)

    ' ---- the walk down the page --------------------------------------------------------
    dim as long y = AfxScaleY(FRMPROJECTOPTIONS_TOP)

    gPOLayout.yHdr(PO_HDR_PROJECT) = y
    y += hdrH + AfxScaleY(1) + AfxScaleY(FRMPROJECTOPTIONS_HDR_AFTER)
    gPOLayout.yRow(PO_ROW_PATH)        = y : y += rowH
    gPOLayout.yRow(PO_ROW_COMMANDLINE) = y : y += rowH

    y += AfxScaleY(FRMPROJECTOPTIONS_SECTION_GAP)
    gPOLayout.yHdr(PO_HDR_COMPILER) = y
    y += hdrH + AfxScaleY(1) + AfxScaleY(FRMPROJECTOPTIONS_HDR_AFTER_HINT)
    gPOLayout.yHint = y
    y += AfxScaleY(FRMPROJECTOPTIONS_HINT_H)
    gPOLayout.yRow(PO_ROW_OTHER32) = y : y += rowH
    gPOLayout.yRow(PO_ROW_OTHER64) = y : y += rowH

    y += AfxScaleY(FRMPROJECTOPTIONS_SECTION_GAP)
    gPOLayout.yHdr(PO_HDR_OUTPUT) = y
    y += hdrH + AfxScaleY(1) + AfxScaleY(FRMPROJECTOPTIONS_HDR_AFTER)
    gPOLayout.yRow(PO_ROW_MANIFEST) = y

    gPOLayout.bValid = true

    ' ---- the fields --------------------------------------------------------------------
    ' Each is vertically centred in its row's stride; the stride is what the label is drawn
    ' into, so the label and its field cannot drift apart vertically either.
    dim as long fieldW = gPOLayout.xFieldRight - gPOLayout.xField

    #macro PLACEFIELD( _h_, _rowidx_ )
        SetWindowPos( _h_, 0, gPOLayout.xField, _
                      gPOLayout.yRow(_rowidx_) + (rowH - fieldH) \ 2, _
                      fieldW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
    #endmacro

    PLACEFIELD( ghPOPath,        PO_ROW_PATH )
    PLACEFIELD( ghPOCommandLine, PO_ROW_COMMANDLINE )
    PLACEFIELD( ghPOOther32,     PO_ROW_OTHER32 )
    PLACEFIELD( ghPOOther64,     PO_ROW_OTHER64 )

    SetWindowPos( ghPOSelect, 0, gPOLayout.xFieldRight + browseG, _
                  gPOLayout.yRow(PO_ROW_PATH) + (rowH - fieldH) \ 2, _
                  browseW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the toggle --------------------------------------------------------------------
    ' Right-aligned to xFieldRight, which is the whole point of reserving the gutter on this
    ' row: the toggle's right edge is the fields' right edge, not the margin.
    dim as long tw = 0, th = 0
    PsToggle_GetIdealSize( ghPOManifest, tw, th )
    SetWindowPos( ghPOManifest, 0, gPOLayout.xFieldRight - tw, _
                  gPOLayout.yRow(PO_ROW_MANIFEST) + (rowH - th) \ 2, _
                  tw, th, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- footer ------------------------------------------------------------------------
    dim as long btnW = AfxScaleX(FRMPROJECTOPTIONS_BTN_W)
    dim as long btnH = AfxScaleY(FRMPROJECTOPTIONS_BTN_H)
    dim as long gap  = AfxScaleX(FRMPROJECTOPTIONS_GAP)
    dim as long btnY = gPOLayout.yFooter + (AfxScaleY(FRMPROJECTOPTIONS_FOOTER_H) - btnH) \ 2

    SetWindowPos( ghPOCancel, 0, gPOLayout.xRight - btnW, btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghPOOK, 0, gPOLayout.xRight - (btnW * 2) - gap, btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Everything that is not a control: the three section headers and their rules, the hint, the
' five row labels, and the footer rule.
'
' SECTION HEADERS ARE PLAIN ghPanel.ForeColor, NOT THE ACCENT. ui.accent falls back to
' toptabs.iconselected, which is a dark navy in default_dark and a PALE TINT (204,233,252) in
' default_light -- opposite ends of the scale. The workspace rule is that the accent is a
' FILL and never a foreground, and an accent-coloured header would be legible in one theme
' and invisible in the other. The hairline under each caption does the separating instead.
' ========================================================================================
private function frmProjectOptions_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    if gPOLayout.bValid = false then
        buffer.EndDoubleBuffer()
        EndPaint( hwnd, @ps )
        return 0
    end if

    dim as RECT rcT
    dim as long hdrH = AfxScaleY(FRMPROJECTOPTIONS_HDR_TEXT_H)

    ' ---- the three section headers -----------------------------------------------------
    #macro SECTIONHDR( _idx_, _text_ )
        rcT.left   = gPOLayout.xLabel
        rcT.right  = gPOLayout.xRight
        rcT.top    = gPOLayout.yHdr(_idx_)
        rcT.bottom = rcT.top + hdrH
        buffer.SetFont( ghFont(GUIFONTBOLD_11) )
        buffer.SetForeColor( ghPanel.ForeColor )
        buffer.PaintText( _text_, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
        rcT.top    = rcT.bottom + AfxScaleY(2)
        rcT.bottom = rcT.top + AfxScaleY(1)
        buffer.SetBackColor( ghEditor.Divider )
        buffer.PaintRect( @rcT )
        buffer.SetBackColor( ghPanel.BackColor )
    #endmacro

    SECTIONHDR( PO_HDR_PROJECT,  L(27,"Project") )
    SECTIONHDR( PO_HDR_COMPILER, L(31,"Compiler options") )
    SECTIONHDR( PO_HDR_OUTPUT,   L(32,"Build output") )

    ' ---- the hint ----------------------------------------------------------------------
    rcT.left   = gPOLayout.xLabel
    rcT.right  = gPOLayout.xRight
    rcT.top    = gPOLayout.yHint
    rcT.bottom = rcT.top + AfxScaleY(FRMPROJECTOPTIONS_HINT_H)
    buffer.SetFont( ghFont(GUIFONT_11) )
    buffer.SetForeColor( ghPanel.ForeColorHot )
    buffer.PaintText( L(39,"Added to the switches from the selected build configuration."), _
                      @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

    ' ---- the row labels ----------------------------------------------------------------
    buffer.SetForeColor( ghPanel.ForeColor )

    ' The four field labels run from the margin to the field, less a gap. The MANIFEST label
    ' is the exception and spans the whole row up to the toggle -- "Create resource file and
    ' manifest" is a sentence, not a field name, and does not fit in a 150px gutter.
    #macro ROWLABEL( _rowidx_, _right_, _text_ )
        rcT.left   = gPOLayout.xLabel
        rcT.right  = _right_
        rcT.top    = gPOLayout.yRow(_rowidx_)
        rcT.bottom = rcT.top + gPOLayout.rowH
        buffer.PaintText( _text_, @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
    #endmacro

    dim as long xLabelRight = gPOLayout.xField - AfxScaleX(FRMPROJECTOPTIONS_GAP)

    ROWLABEL( PO_ROW_PATH,        xLabelRight, L(196,"Project path") )
    ROWLABEL( PO_ROW_COMMANDLINE, xLabelRight, L(35,"Command line") )
    ROWLABEL( PO_ROW_OTHER32,     xLabelRight, L(33,"32-bit compiler") )
    ROWLABEL( PO_ROW_OTHER64,     xLabelRight, L(34,"64-bit compiler") )

    scope
        dim as long tw = 0, th = 0
        PsToggle_GetIdealSize( ghPOManifest, tw, th )
        ROWLABEL( PO_ROW_MANIFEST, gPOLayout.xFieldRight - tw - AfxScaleX(FRMPROJECTOPTIONS_GAP), _
                  L(244,"Create resource file and manifest") )
    end scope

    ' ---- the footer rule ---------------------------------------------------------------
    rcT.left   = gPOLayout.xLabel
    rcT.right  = gPOLayout.xRight
    rcT.top    = gPOLayout.yFooter
    rcT.bottom = rcT.top + AfxScaleY(1)
    buffer.SetBackColor( ghEditor.Divider )
    buffer.PaintRect( @rcT )

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Push the current theme into every child that carries its own colours.
' ========================================================================================
private sub frmProjectOptions_ApplyTheme( byval hwnd as HWND )

    OptionsTheme_ApplyTextBox( ghPOPath )
    OptionsTheme_ApplyTextBox( ghPOCommandLine )
    OptionsTheme_ApplyTextBox( ghPOOther32 )
    OptionsTheme_ApplyTextBox( ghPOOther64 )

    dim as PSTOGGLE_COLORS tc
    OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghPOManifest, @tc )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    dim as HWND hBtn(2) = { ghPOSelect, ghPOOK, ghPOCancel }
    for i as long = 0 to 2
        if hBtn(i) = 0 then continue for
        PsButton_SetColors( hBtn(i), @bc )
        PsButton_Refresh( hBtn(i) )
    next

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' A child's rect in the DIALOG's client coordinates, for the layout self-test.
' ========================================================================================
private function frmProjectOptions_ChildRect( byval hChild as HWND ) as RECT
    dim as RECT rc
    if hChild = 0 then return rc
    GetWindowRect( hChild, @rc )
    dim as POINT pt1 = type( rc.left, rc.top )
    dim as POINT pt2 = type( rc.right, rc.bottom )
    ScreenToClient( HWND_FRMPROJECTOPTIONS, @pt1 )
    ScreenToClient( HWND_FRMPROJECTOPTIONS, @pt2 )
    rc.left = pt1.x : rc.top = pt1.y
    rc.right = pt2.x : rc.bottom = pt2.y
    return rc
end function


' ========================================================================================
' TIKO_PROJECTOPTIONS_SELFTEST=1
'
' Two halves, frmBuildConfig's split. The HEADLESS half needs no window and asserts the
' localized ids and the staging round trip. The LAYOUT half runs from Show once the controls
' exist, and measures the REAL windows at the REAL DPI rather than the unscaled constants.
'
' THE LAYOUT HALF IS STRUCTURALLY BLIND TO A GAP -- it can tell you nothing overlaps, nothing
' is clipped and every rect is inside its band, and all of that stays green on a page with a
' 200px dead band through the middle of it. That is measured, not assumed: frmBuildConfig's
' one real layout defect was exactly such a band and its 74-assertion suite stayed green on
' it. A gap is not a violation. Looking at the render is still the only check for one.
' ========================================================================================
sub frmProjectOptions_RunSelfTest( byval bLayoutHalf as boolean )

    if environ("TIKO_PROJECTOPTIONS_SELFTEST") <> "1" then exit sub

    static as long nPass, nFail

    #macro CHECK( _cond_, _name_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            dim as string sMsg = "  FAIL: " & _name_
            print sMsg
        end if
    #endmacro

    if bLayoutHalf = false then
        nPass = 0 : nFail = 0
        print "frmProjectOptions self-test (headless)"

        ' ---- the localized ids -----------------------------------------------------------
        ' L(id,"default") DISCARDS the default: an id with no entry in english.lang renders as
        ' an EMPTY STRING, so a caption or a whole section header comes up blank with nothing
        ' in the build or the warnings to say so.
        dim as long ids(0 to 10) = { 27, 31, 32, 33, 34, 35, 39, 80, 196, 244, 180 }
        for i as long = 0 to 10
            dim as string sName = "lang id " & str(ids(i)) & " is not blank"
            CHECK( len(LL(ids(i))) > 0, sName )
        next

        ' AND WHAT THEY RESOLVED TO, printed rather than asserted -- the value is
        ' language-dependent, so there is nothing to compare it against.
        '
        ' THIS IS NOT DECORATION. "id is not blank" passes under EVERY language file even when
        ' that language has the slot empty, because LoadLocalizationFile fills a blank key with
        ' the English text. So the assertion above cannot tell a translated phrase from an
        ' untranslated one, and running the suite under french.lang and seeing it green proves
        ' only that the ids exist. Reading these four lines is what distinguishes the two.
        scope
            dim as string sL = "  resolved: " & str(27) & "=" & LL(27) & " | " & _
                               str(31) & "=" & LL(31) & " | " & str(35) & "=" & LL(35)
            print sL
            dim as string sL2 = "  resolved: " & str(80) & "=" & LL(80)
            print sL2
        end scope

        ' ---- the staging round trip ------------------------------------------------------
        ' Save the caller's project state, drive the staging copy, restore. This runs from
        ' frmMain at startup where no project is open, but the save/restore makes that a
        ' property of the test rather than of when it happens to be called.
        scope
            dim as DWSTRING sPath = gApp.ProjectFilename, sCmd = gApp.ProjectCommandLine
            dim as DWSTRING s32 = gApp.ProjectOther32, s64 = gApp.ProjectOther64
            dim as long nMan = gApp.ProjectManifest
            dim as DWSTRING sNotes = gApp.ProjectNotes
            dim as DWSTRING sName = gApp.ProjectName
            dim as string   sBuild = gApp.ProjectBuild
            dim as boolean  bActive = gApp.IsProjectActive

            gApp.ProjectFilename    = "C:\dev\selftest\x.tiko"
            gApp.ProjectCommandLine = "--run-args"
            gApp.ProjectOther32     = "-a32"
            gApp.ProjectOther64     = "-a64"
            gApp.ProjectManifest    = 1
            gApp.ProjectNotes       = "keep me"

            frmProjectOptions_LoadWork( false )
            CHECK( gPOWork.ProjectPath = "C:\dev\selftest\x.tiko", "LoadWork copies the path" )
            CHECK( gPOWork.CommandLine = "--run-args",             "LoadWork copies the command line" )
            CHECK( gPOWork.Other32 = "-a32",                       "LoadWork copies Other32" )
            CHECK( gPOWork.Other64 = "-a64",                       "LoadWork copies Other64" )
            CHECK( gPOWork.Manifest = true,                        "LoadWork copies the manifest flag" )

            ' Editing the staging copy must not reach gApp. This is what Cancel relies on.
            gPOWork.Other32 = "MUTATED"
            CHECK( gApp.ProjectOther32 = "-a32", "editing the staging copy does not touch gApp" )

            ' A NEW project seeds empty with the manifest ON.
            frmProjectOptions_LoadWork( true )
            CHECK( len(gPOWork.ProjectPath) = 0, "a new project starts with an empty path" )
            CHECK( gPOWork.Manifest = true,      "a new project defaults the manifest on" )

            ' ---- THE REGRESSION THAT MATTERS ---------------------------------------------
            ' The dialog this replaces ended its save with gApp.ProjectNotes = "" and
            ' gApp.ProjectCommandLine = "", unconditionally, so every OK destroyed both.
            '
            ' This DRIVES THE COMMIT rather than inspecting state around it. The first version
            ' of these two assertions did the latter -- they read gApp after a LoadWork and
            ' would have stayed green with the wipe put straight back, because nothing in the
            ' test ever reached the code that does the wiping.
            gPOWork.ProjectPath = "C:\dev\selftest\y.tiko"
            gPOWork.CommandLine = "--committed"
            gPOWork.Other32     = "-b32"
            gPOWork.Other64     = "-b64"
            gPOWork.Manifest    = false
            gApp.ProjectNotes   = "keep me"

            frmProjectOptions_CommitWork()

            CHECK( gApp.ProjectNotes = "keep me",           "the commit leaves ProjectNotes alone" )
            CHECK( gApp.ProjectCommandLine = "--committed", "the commit WRITES the command line" )
            CHECK( gApp.ProjectFilename = "C:\dev\selftest\y.tiko", "the commit writes the path" )
            CHECK( gApp.ProjectOther32 = "-b32",            "the commit writes Other32" )
            CHECK( gApp.ProjectOther64 = "-b64",            "the commit writes Other64" )
            CHECK( gApp.ProjectManifest = 0,                "the commit writes the manifest flag" )
            ' "y.tiko", not "y": NAMEX keeps the extension, which is why the window caption
            ' reads "Project Options - tiko.tiko" rather than "- tiko".
            CHECK( gApp.ProjectName = "y.tiko",             "the commit derives the project name" )

            ' EVERY field CommitWork writes is restored, IsProjectActive included -- it sets
            ' that unconditionally, and leaving it true would hand the rest of the session a
            ' project that does not exist.
            gApp.ProjectFilename    = sPath
            gApp.ProjectCommandLine = sCmd
            gApp.ProjectOther32     = s32
            gApp.ProjectOther64     = s64
            gApp.ProjectManifest    = nMan
            gApp.ProjectNotes       = sNotes
            gApp.ProjectName        = sName
            gApp.ProjectBuild       = sBuild
            gApp.IsProjectActive    = bActive
        end scope

        dim as string sHdr = "  headless: " & str(nPass) & " passed, " & str(nFail) & " failed"
        print sHdr
        exit sub
    end if

    ' ------------------------------------------------------------------------------------
    ' The layout half.
    ' ------------------------------------------------------------------------------------
    print "frmProjectOptions self-test (layout, real DPI)"

    dim as RECT rcClient
    GetClientRect( HWND_FRMPROJECTOPTIONS, @rcClient )

    CHECK( gPOLayout.bValid, "the layout has been derived" )
    CHECK( rcClient.right = AfxScaleX(FRMPROJECTOPTIONS_CLIENT_W), "client width matches the constant" )
    CHECK( rcClient.bottom = AfxScaleY(FRMPROJECTOPTIONS_CLIENT_H), "client height matches the constant" )

    ' THE WALK ENDS INSIDE THE PAGE. This is the assertion that catches a forgotten re-sum of
    ' CLIENT_H after a row or gap constant is edited -- without it the manifest row simply
    ' falls off the bottom edge, silently, and only above 100% DPI.
    dim as long yBodyEnd = gPOLayout.yRow(PO_ROW_MANIFEST) + gPOLayout.rowH
    CHECK( yBodyEnd <= gPOLayout.yFooter, "the body walk ends above the footer rule" )
    CHECK( gPOLayout.yFooter < rcClient.bottom, "the footer rule is inside the client" )

    ' The rows march down the page in order, each exactly one stride below the last within a
    ' section. Between sections there is more, which is the section gap.
    CHECK( gPOLayout.yRow(PO_ROW_COMMANDLINE) - gPOLayout.yRow(PO_ROW_PATH) = gPOLayout.rowH, _
           "Command line is one stride below Project path" )
    CHECK( gPOLayout.yRow(PO_ROW_OTHER64) - gPOLayout.yRow(PO_ROW_OTHER32) = gPOLayout.rowH, _
           "the 64-bit row is one stride below the 32-bit row" )
    CHECK( gPOLayout.yRow(PO_ROW_OTHER32) > gPOLayout.yRow(PO_ROW_COMMANDLINE) + gPOLayout.rowH, _
           "the compiler section starts below the project section, with a gap" )

    ' Headers are above their own first row, and in order.
    CHECK( gPOLayout.yHdr(PO_HDR_PROJECT) < gPOLayout.yRow(PO_ROW_PATH), "Project header is above its row" )
    CHECK( gPOLayout.yHdr(PO_HDR_COMPILER) < gPOLayout.yHint, "Compiler header is above the hint" )
    CHECK( gPOLayout.yHint < gPOLayout.yRow(PO_ROW_OTHER32), "the hint is above the 32-bit row" )
    CHECK( gPOLayout.yHdr(PO_HDR_OUTPUT) < gPOLayout.yRow(PO_ROW_MANIFEST), "Build output header is above its row" )
    CHECK( gPOLayout.yHdr(PO_HDR_PROJECT) < gPOLayout.yHdr(PO_HDR_COMPILER), "headers are in page order (1)" )
    CHECK( gPOLayout.yHdr(PO_HDR_COMPILER) < gPOLayout.yHdr(PO_HDR_OUTPUT), "headers are in page order (2)" )

    ' ---- THE RIGHT EDGE IS ONE NUMBER ---------------------------------------------------
    ' The four fields and the toggle all end at xFieldRight, and the "..." ends at the margin.
    ' This is the assertion the reserved-gutter decision exists for: with the gutter charged
    ' only on the rows that have a button, the toggle would sit 42px further right and this
    ' would be the only thing that noticed.
    dim as RECT rcPath = frmProjectOptions_ChildRect( ghPOPath )
    dim as RECT rcCmd  = frmProjectOptions_ChildRect( ghPOCommandLine )
    dim as RECT rc32   = frmProjectOptions_ChildRect( ghPOOther32 )
    dim as RECT rc64   = frmProjectOptions_ChildRect( ghPOOther64 )
    dim as RECT rcTog  = frmProjectOptions_ChildRect( ghPOManifest )
    dim as RECT rcSel  = frmProjectOptions_ChildRect( ghPOSelect )

    CHECK( rcPath.right = gPOLayout.xFieldRight, "the path field ends at the field edge" )
    CHECK( rcCmd.right  = gPOLayout.xFieldRight, "the command line field ends at the field edge" )
    CHECK( rc32.right   = gPOLayout.xFieldRight, "the 32-bit field ends at the field edge" )
    CHECK( rc64.right   = gPOLayout.xFieldRight, "the 64-bit field ends at the field edge" )
    CHECK( rcTog.right  = gPOLayout.xFieldRight, "the manifest toggle ends at the field edge" )
    CHECK( rcSel.right  = gPOLayout.xRight,      "the browse button ends at the right margin" )
    CHECK( rcSel.left   >= rcPath.right,         "the browse button does not overlap the path field" )

    ' Non-empty, because a zeroed rect satisfies every containment test above vacuously.
    CHECK( rcPath.right > rcPath.left, "the path field has width" )
    CHECK( rcTog.right > rcTog.left,   "the toggle has width" )

    ' Left edges agree, and the label gutter is real.
    CHECK( rcPath.left = gPOLayout.xField, "the fields start at the field column" )
    CHECK( rc64.left = gPOLayout.xField,   "every field starts at the same x" )
    CHECK( gPOLayout.xField > gPOLayout.xLabel, "there is a label gutter" )

    ' ---- everything is inside the page --------------------------------------------------
    #macro INSIDEPAGE( _rc_, _name_ )
        CHECK( (_rc_.left >= 0) andalso (_rc_.right <= rcClient.right) andalso _
               (_rc_.top >= 0) andalso (_rc_.bottom <= gPOLayout.yFooter), _name_ )
    #endmacro

    INSIDEPAGE( rcPath, "the path field is inside the page band" )
    INSIDEPAGE( rcCmd,  "the command line field is inside the page band" )
    INSIDEPAGE( rc32,   "the 32-bit field is inside the page band" )
    INSIDEPAGE( rc64,   "the 64-bit field is inside the page band" )
    INSIDEPAGE( rcTog,  "the toggle is inside the page band" )
    INSIDEPAGE( rcSel,  "the browse button is inside the page band" )

    ' Fields do not overlap each other vertically.
    CHECK( rcPath.bottom <= rcCmd.top, "the path and command line fields do not overlap" )
    CHECK( rcCmd.bottom  <= rc32.top,  "the command line and 32-bit fields do not overlap" )
    CHECK( rc32.bottom   <= rc64.top,  "the 32-bit and 64-bit fields do not overlap" )
    CHECK( rc64.bottom   <= rcTog.top, "the 64-bit field and the toggle do not overlap" )

    ' ---- the footer ---------------------------------------------------------------------
    dim as RECT rcOK     = frmProjectOptions_ChildRect( ghPOOK )
    dim as RECT rcCancel = frmProjectOptions_ChildRect( ghPOCancel )
    CHECK( rcCancel.right = gPOLayout.xRight, "Cancel ends at the right margin" )
    CHECK( rcOK.right < rcCancel.left, "OK is to the LEFT of Cancel" )
    CHECK( rcOK.top >= gPOLayout.yFooter, "OK is in the footer band" )
    CHECK( rcCancel.bottom <= rcClient.bottom, "Cancel is inside the client" )

    ' ---- the seed, driven with SENTINELS ------------------------------------------------
    '
    ' NOT read off whatever Show happened to seed. This dialog is reached by the auto-open
    ' flag with no project loaded, so every staged string is "" -- and an assertion that an
    ' empty field matches an empty model passes just as happily with the seeding deleted.
    ' Measured, not supposed: that A/B was run and left the earlier version at 69/69 green.
    '
    ' The sentinels are DISTINCT per field, so a seed that wrote the right value into the
    ' wrong control fails too, and the toggle is driven to TRUE because false is the state it
    ' is already in.
    scope
        dim as PROJECTOPTIONS_WORK saved = gPOWork

        gPOWork.ProjectPath = "SENTINEL-path"
        gPOWork.CommandLine = "SENTINEL-cmd"
        gPOWork.Other32     = "SENTINEL-32"
        gPOWork.Other64     = "SENTINEL-64"
        gPOWork.Manifest    = true
        frmProjectOptions_SeedControls()

        CHECK( PsTextBox_GetText( ghPOPath ) = "SENTINEL-path",       "the seed fills the path field" )
        CHECK( PsTextBox_GetText( ghPOCommandLine ) = "SENTINEL-cmd", "the seed fills the command line field" )
        CHECK( PsTextBox_GetText( ghPOOther32 ) = "SENTINEL-32",      "the seed fills the 32-bit field" )
        CHECK( PsTextBox_GetText( ghPOOther64 ) = "SENTINEL-64",      "the seed fills the 64-bit field" )
        CHECK( PsToggle_GetChecked( ghPOManifest ) = true,            "the seed sets the toggle" )

        ' AND THE SEED IS SILENT. Writing a control fires its change callback exactly as a
        ' keystroke does, so without the gPOLoading guard the seed would write the staging
        ' copy back over itself -- harmless here, but it is the guard that keeps "only user
        ' edits move the model" true, and this is the only thing that would notice it going.
        gPOWork.Other32 = "UNTOUCHED"
        frmProjectOptions_SeedControls()
        CHECK( gPOWork.Other32 = "UNTOUCHED", "the seed does not write the staging copy back" )

        gPOWork = saved
        frmProjectOptions_SeedControls()
        CHECK( PsTextBox_GetText( ghPOPath ) = gPOWork.ProjectPath, "the sentinels are undone" )
    end scope

    dim as string sRep = "  total: " & str(nPass) & " passed, " & str(nFail) & " failed"
    print sRep

end sub


' ========================================================================================
' frmProjectOptions Window procedure
' ========================================================================================
private function frmProjectOptions_WndProc( _
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
            return frmProjectOptions_OnPaint( hwnd )

        case WM_SIZE
            frmProjectOptions_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes through its click callback, not WM_COMMAND. What still arrives
            ' here is IDCANCEL, which IsDialogMessage synthesises for the Escape key.
            select case loword(wParam)
                case IDCANCEL
                    gApp.IsNewProjectFlag = false
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case WM_SYSCOMMAND
            ' The X and Alt+F4. IsNewProjectFlag must come down here too, or
            ' OnCommand_ProjectNew goes on to open a project that was never created.
            if (wParam and &hFFF0) = SC_CLOSE then
                gApp.IsNewProjectFlag = false
            end if

        case WM_CLOSE
            ' NOTHING IS SAVED HERE. The commit is on the OK path only, so reaching this by
            ' any route leaves gApp and the project file exactly as they were found.
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMPROJECTOPTIONS = 0
            ghPOPath = 0 : ghPOSelect = 0 : ghPOCommandLine = 0
            ghPOOther32 = 0 : ghPOOther64 = 0 : ghPOManifest = 0
            ghPOOK = 0 : ghPOCancel = 0
            gPOLayout.bValid = false
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmProjectOptions_Show
' ========================================================================================
function frmProjectOptions_Show( _
            byval hwndParent as HWND, _
            byval IsNewProject as boolean _
            ) as LRESULT

    ' Seed the staging copy BEFORE any control exists -- everything below is built from
    ' gPOWork, and gApp is not touched again unless OK commits.
    gApp.IsNewProjectFlag = IsNewProject
    frmProjectOptions_LoadWork( IsNewProject )

    dim as DWSTRING wszTitle = L(177,"Project Options")
    if IsNewProject then
        wszTitle = wszTitle & " - " & L(178,"New Project")
    else
        wszTitle = wszTitle & " - " & gApp.ProjectName
    end if

    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMPROJECTOPTIONS = pWindow->Create( hwndParent, wszTitle, _
        @frmProjectOptions_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMPROJECTOPTIONS_CLIENT_W, FRMPROJECTOPTIONS_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hwndParent )

    ' ---- CREATION ORDER IS THE TAB ORDER ----------------------------------------------
    '
    ' These controls live on an ordinary CWindow, not a dialog template, so there is no
    ' ordinal to set: GetNextDlgTabItem walks the child Z-ORDER, and Z-order is creation
    ' order. Built here in READING order -- path, browse, command line, 32, 64, manifest --
    ' with OK and Cancel last, so Tab walks the work and then the chrome.

    ' LEFT PADDING IS SCALED BY THE HOST. PsTextBox documents its margins as RAW pixels and
    ' passes them verbatim to EM_SETMARGINS, so AfxScaleX is ours to apply.
    #macro MAKEFIELD( _h_, _id_ )
        _h_ = PsTextBox_Create( HWND_FRMPROJECTOPTIONS, _id_ )
        PsTextBox_SetFont( _h_, ghFont(GUIFONT_11) )
        PsTextBox_SetMargins( _h_, AfxScaleX(8), AfxScaleX(6) )
        PsTextBox_SetChangeCallback( _h_, @frmProjectOptions_TextChanged )
    #endmacro

    MAKEFIELD( ghPOPath, IDC_FRMPROJECTOPTIONS_TXTPROJECTPATH )

    ghPOSelect = PsButton_Create( HWND_FRMPROJECTOPTIONS, IDC_FRMPROJECTOPTIONS_CMDSELECT )
    PsButton_SetText( ghPOSelect, "..." )
    PsButton_SetFont( ghPOSelect, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghPOSelect, @frmProjectOptions_ButtonClick )

    MAKEFIELD( ghPOCommandLine, IDC_FRMPROJECTOPTIONS_TXTCOMMANDLINE )
    PsTextBox_SetCueBannerText( ghPOCommandLine, L(80,"arguments passed when the project is run") )

    MAKEFIELD( ghPOOther32, IDC_FRMPROJECTOPTIONS_TXTOPTIONS32 )
    MAKEFIELD( ghPOOther64, IDC_FRMPROJECTOPTIONS_TXTOPTIONS64 )

    ghPOManifest = PsToggle_Create( HWND_FRMPROJECTOPTIONS, IDC_FRMPROJECTOPTIONS_TOGMANIFEST )
    PsToggle_SetCheckChangedCallback( ghPOManifest, @frmProjectOptions_ToggleChanged )

    ghPOOK = PsButton_Create( HWND_FRMPROJECTOPTIONS, IDC_FRMPROJECTOPTIONS_CMDOK )
    PsButton_SetText( ghPOOK, L(0,"OK") )
    PsButton_SetFont( ghPOOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( ghPOOK, true )
    PsButton_SetClickCallback( ghPOOK, @frmProjectOptions_ButtonClick )

    ghPOCancel = PsButton_Create( HWND_FRMPROJECTOPTIONS, IDC_FRMPROJECTOPTIONS_CMDCANCEL )
    PsButton_SetText( ghPOCancel, L(1,"Cancel") )
    PsButton_SetFont( ghPOCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghPOCancel, @frmProjectOptions_ButtonClick )

    ' ---- seed the controls from the staging copy --------------------------------------
    frmProjectOptions_SeedControls()

    ShowWindow( ghPOPath, SW_SHOW )
    ShowWindow( ghPOSelect, SW_SHOW )
    ShowWindow( ghPOCommandLine, SW_SHOW )
    ShowWindow( ghPOOther32, SW_SHOW )
    ShowWindow( ghPOOther64, SW_SHOW )
    ShowWindow( ghPOManifest, SW_SHOW )
    ShowWindow( ghPOOK, SW_SHOW )
    ShowWindow( ghPOCancel, SW_SHOW )

    frmProjectOptions_ApplyTheme( HWND_FRMPROJECTOPTIONS )
    frmProjectOptions_PositionWindows( HWND_FRMPROJECTOPTIONS )

    ShowWindow( HWND_FRMPROJECTOPTIONS, SW_SHOW )
    UpdateWindow( HWND_FRMPROJECTOPTIONS )

    ' Focus the path field: it is the one field that must be filled, and for a new project it
    ' is empty. It also has to be set explicitly for a second, unrelated reason --
    ' IsDialogMessage ignores Tab until a child already holds the focus, and an ordinary
    ' CWindow has no dialog manager to arrange that. (See the WS_EX_CONTROLPARENT section of
    ' C:\dev\Learnings.md.)
    SetFocus( ghPOPath )

    frmProjectOptions_RunSelfTest( true )

    ' ---- message loop -----------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents: that has no *_FilterMessage hooks, and this dialog
    ' owes one. PsToggle and PsButton add none.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. The four text fields each own a PsPopupMenu context menu; without this it
        ' has no keyboard navigation and never closes on an outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' ESCAPE cancels -- explicitly, and AFTER the filter above so an open context menu
        ' closes on Escape first rather than the whole dialog going with it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            gApp.IsNewProjectFlag = false
            SendMessage( HWND_FRMPROJECTOPTIONS, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMPROJECTOPTIONS, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    function = 0

end function
