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

'  frmOptionsLocal.inc  --  the Localization page (see frmOptionsLocal.bi).
'
'  Panel-hosted, like Compiler Setup. The phrase list is a PsListTree with a column header
'  band; the four commands are PsButtons; the English and translation fields are PsTextBoxes.
'  The file I/O and the gLocalPhrases / gLocalPhrasesEdit business logic are carried over
'  from the old child form unchanged.

#include once "frmOptionsLocal.bi"
#include once "frmOptions.bi"          ' HWND_FRMOPTIONS, frmOptions_ThemedMsgBox
#include once "modOptionsRows.bi"      ' gOptWork, shared padding + theme helpers
#include once "modRoutines.bi"         ' AfxIFileOpenDialogW / SaveDialog, ListBox_AddTabbedRow
#include once "clsConfig.bi"


' Fixed heights, UNSCALED.
#define LOCAL_LISTHEIGHT    190
#define LOCAL_FIELDPAD       11
#define LOCAL_LABELGAP        4
#define LOCAL_SECTIONGAP     14
#define LOCAL_BTNWIDTH       84
#define LOCAL_BTNHEIGHT      30
#define LOCAL_BTNGAP          8


type LOCAL_LABEL
    rc      as RECT
    caption as DWSTRING
end type


dim shared gLocalCurrentFile as DWSTRING     ' the .lang filename the page points at
dim shared gLocalTransLabel   as DWSTRING     ' language name shown above the translate box
dim shared gLocalShowEdit      as boolean      ' the phrase-editing area is visible
dim shared gLocalBuilt         as boolean

dim shared ghLocalSelect   as HWND
dim shared ghLocalNew      as HWND
dim shared ghLocalEdit     as HWND
dim shared ghLocalDelete   as HWND
dim shared ghLocalList     as HWND
dim shared ghLocalEnglish  as HWND
dim shared ghLocalTranslate as HWND

dim shared gLocalLabels(any) as LOCAL_LABEL


' Forward declarations for the internal helpers used before their definition.
declare sub frmOptionsLocal_DisableEnglish()
declare sub frmOptionsLocal_UpdateTextBoxes()
declare function frmOptionsLocal_LoadLocalizationFile( byref wszFilename as wstring ) as long
declare sub frmOptionsLocal_ClearGlobalArray()


' ========================================================================================
' This module owns exactly the Localization page.
' ========================================================================================
function frmOptionsLocal_OwnsPage( byval nPage as long ) as boolean
    function = iif( nPage = OPTPAGE_LOCAL, true, false )
end function


' ========================================================================================
' Seed the page state from gConfig when the dialog opens (before anything is built).
' ========================================================================================
sub frmOptionsLocal_SeedFromConfig()
    gLocalCurrentFile = gConfig.LocalizationFile
    gLocalTransLabel  = AfxStrPathName( "NAME", gConfig.LocalizationFile )
    gLocalPhrasesEdit = false
    gLocalShowEdit    = false
end sub


' ========================================================================================
' Forget all state. Controls live on the scroll panel page and die with it, so their
' handles are stale on the next open -- Build must not short-circuit on a stale gLocalBuilt.
' ========================================================================================
sub frmOptionsLocal_Reset()
    gLocalBuilt      = false
    ghLocalSelect    = 0
    ghLocalNew       = 0
    ghLocalEdit      = 0
    ghLocalDelete    = 0
    ghLocalList      = 0
    ghLocalEnglish   = 0
    ghLocalTranslate = 0
    erase gLocalLabels
end sub


' ========================================================================================
' If a localization file is currently being edited then save it. (Unchanged logic; the
' current filename now comes from gLocalCurrentFile and the phrase rows from the PsListTree.)
' ========================================================================================
function frmOptionsLocal_LocalEditCheck() as long
    if gLocalPhrasesEdit = false then exit function

    dim as DWSTRING wszLocalName = gLocalCurrentFile
    dim as DWSTRING wszFileName = AfxGetExePathName + wstr("settings\languages\" + wszLocalName)
    dim as DWSTRING wszTemp, wszPhrase

    ' Never write over the English reference file.
    if ucase(trim(AfxStrPathname( "NAMEX", wszFilename ))) = "ENGLISH.LANG" then exit function
    if ghLocalList = 0 then exit function

    dim as long nCount = PsListTree_GetCount( ghLocalList )

    AfxDeleteFile(wszFileName)

    dim pStream as CTextStream
    if pStream.OpenForOutputW(wszFilename, true) = S_OK then

        wszTemp = _
        "' Localization file for " & ucase(AfxStrPathName("NAME", wszLocalName)) & " language" & vbcrlf & _
        "'" & vbcrlf & _
        "' The first element to exist must be called MAXIMUM and needs to be set to" & vbcrlf & _
        "' the value of the last key in this file." & vbcrlf & _
        "'" & vbcrlf & _
        "' This file should be created and saved using UTF-16 encoding (unicode)." & vbcrlf & _
        "'" & vbcrlf & _
        "' Each line represents a key/value pair describing the position to store the" & vbcrlf & _
        "' localized word/phrase into the localization array used by the editor. Simple." & vbcrlf & vbcrlf & _
        "MAXIMUM:" & wstr(nCount) & vbcrlf & vbcrlf
        pStream.WriteLine(wszTemp)

        for i as long = 0 to nCount - 1
            dim as DWSTRING wszIndex = PsListTree_GetCellText( ghLocalList, i, 0 )
            dim as long idx = val(wszIndex)
            wszTemp = wspace(1024)
            wszPhrase = AfxStrRSet(wszIndex, 5, "0") + wstr(":") + gLocalPhrases(idx)
            ' Use ** version of DWSTRING on MID statements per the documentation.
            mid(**wszTemp, 1) = wszPhrase
            if len(gLangEnglish(idx)) then
                ' Always attempt to insert the ENGLISH comment at position 60. If the local
                ' phrase is too long then simply put the ENGLISH comment after it.
                dim as long nPos = iif( len(wszPhrase) < 60, 60, len(wszPhrase) + 5)
                mid(**wszTemp, nPos) = "; " & gLangEnglish(idx)
            end if
            pStream.WriteLine rtrim(wszTemp)
        next
    end if
    pStream.Close

    function = 0
end function


' ========================================================================================
' Clear the global localization phrase array. (Unchanged.)
' ========================================================================================
sub frmOptionsLocal_ClearGlobalArray()
    for i as long = lbound(gLocalPhrases) to ubound(gLocalPhrases)
        gLocalPhrases(i) = ""
    next
end sub


' ========================================================================================
' Load the selected localization file into the global array. (Unchanged from the old page.)
' ========================================================================================
function frmOptionsLocal_LoadLocalizationFile( byref wszFilename as wstring ) as long

    if AfxFileExists( wszFileName ) = 0 then exit function

    dim as BSTRING wst, wKey, wData
    dim as long nKey, nData, i, maxKey

    dim pStream as CTextStream
    if pStream.OpenUnicode( wszFileName ) <> S_OK then exit function

    frmOptionsLocal_ClearGlobalArray()

    do until pStream.EOS
        wst = pStream.ReadLine

        if len(wst) = 0 then continue do
        if left(wst, 1) = "'" then continue do

        i = instr(wst, ":")
        if i = 0 then continue do

        wKey = "": wData = "": nData = 0

        wKey  = left(wst, i-1)
        wData = mid(**wst, i+1)    ' MID causes problems with Chinese data so ** is used.

        nKey  = val(wKey)
        nData = val(wData)

        ' MAXIMUM for a non-English file does not redimension gLocalPhrases -- that array is
        ' always sized to the main ENGLISH array.
        if ucase(wKey) <> "MAXIMUM" then
            if (nKey >= lbound(gLocalPhrases)) andalso (nKey <= ubound(gLocalPhrases)) then
                ' Use ** so cyrillic gets converted correctly; FB intrinsics (RTRIM) convert
                ' those incorrectly through CBSTR / DWSTRING.
                gLocalPhrases(nKey) = rtrim(**AfxStrParse(wData, 1, ";"), any chr(9,32))
                if gLangEnglish(nKey) = "" then gLocalPhrases(nKey) = ""
                if gLocalPhrases(nKey) = "" then gLocalPhrases(nKey) = gLangEnglish(nKey)
                if nKey > maxKey then maxKey = nKey
            end if
        end if
    loop
    pStream.Close

    ' The local file may hold fewer keys than the English file; copy the English versions
    ' over for the remaining keys.
    for i = maxKey + 1 to ubound(gLangEnglish)
        gLocalPhrases(i) = gLangEnglish(i)
    next

    function = 0
end function


' ========================================================================================
' Show or hide the phrase-editing area's real controls, per gLocalShowEdit. (The English /
' translate LABELS are drawn and are gated in Paint by the same flag.)
' ========================================================================================
private sub frmOptionsLocal_ApplyEditVisibility()
    dim as long nCmd = iif( gLocalShowEdit, SW_SHOW, SW_HIDE )
    if ghLocalList then ShowWindow( ghLocalList, nCmd )
    if ghLocalEnglish then ShowWindow( ghLocalEnglish, nCmd )
    if ghLocalTranslate then ShowWindow( ghLocalTranslate, nCmd )
end sub


' ========================================================================================
' Enable/disable the commands and decide whether the edit area is visible, from whether the
' English reference file is selected and whether a file is being edited. (Ported from the
' old frmOptionsLocal_DisableEnglish; the groupbox-caption read is now gLocalCurrentFile.)
' ========================================================================================
sub frmOptionsLocal_DisableEnglish()
    if gLocalBuilt = false then exit sub

    dim as DWSTRING wszFilename = ucase( gLocalCurrentFile )
    dim as boolean bEnglish = iif( AfxStrPathname( "NAMEX", wszFilename ) = "ENGLISH.LANG", true, false )

    ' Select and New are always available.
    PsButton_SetEnabled( ghLocalSelect, true )
    PsButton_SetEnabled( ghLocalNew, true )

    if bEnglish then
        ' The English reference file cannot be edited or deleted.
        PsButton_SetEnabled( ghLocalEdit, false )
        PsButton_SetEnabled( ghLocalDelete, false )
        gLocalShowEdit = false
    else
        PsButton_SetEnabled( ghLocalDelete, true )
        ' Edit is available only when not already editing.
        PsButton_SetEnabled( ghLocalEdit, iif( gLocalPhrasesEdit, false, true ) )
        gLocalShowEdit = gLocalPhrasesEdit
    end if

    frmOptionsLocal_ApplyEditVisibility()

    ' The page's content height changed with the edit area's visibility, so re-flow it.
    frmOptions_RelayoutCurrentPage()
end sub


' ========================================================================================
' Reflect the selected phrase row into the English / translation textboxes. (Ported; the
' listview reads become PsListTree cell reads.)
' ========================================================================================
sub frmOptionsLocal_UpdateTextBoxes()
    if ghLocalList = 0 then exit sub
    dim as long nCurSel = PsListTree_GetCurSel( ghLocalList )
    if nCurSel < 0 then exit sub

    dim as DWSTRING wszIndex   = PsListTree_GetCellText( ghLocalList, nCurSel, 0 )
    dim as DWSTRING wszEnglish = PsListTree_GetCellText( ghLocalList, nCurSel, 1 )
    dim as long idx = val(wszIndex)
    if (idx >= lbound(gLocalPhrases)) andalso (idx <= ubound(gLocalPhrases)) then
        PsTextBox_SetText( ghLocalEnglish, wszEnglish )               ' silent
        PsTextBox_SetText( ghLocalTranslate, gLocalPhrases(idx) )     ' silent
    end if
end sub


' ========================================================================================
' The phrase list selection changed (mouse or keyboard).
' ========================================================================================
private sub frmOptionsLocal_ListSelChange( byval hList as HWND, byval row as integer )
    frmOptionsLocal_UpdateTextBoxes()
end sub


' ========================================================================================
' The translation textbox changed (user typing only -- SetText is silent). Store into the
' phrase array against the selected row's index. (Ported from the EN_CHANGE handler.)
' ========================================================================================
private sub frmOptionsLocal_TranslateChanged( byval hTextBox as HWND )
    if ghLocalList = 0 then exit sub
    dim as long nCurSel = PsListTree_GetCurSel( ghLocalList )
    if nCurSel < 0 then exit sub
    dim as DWSTRING wszIndex = PsListTree_GetCellText( ghLocalList, nCurSel, 0 )
    dim as long idx = val(wszIndex)
    if (idx >= lbound(gLocalPhrases)) andalso (idx <= ubound(gLocalPhrases)) then
        gLocalPhrases(idx) = PsTextBox_GetText( ghLocalTranslate )
    end if
end sub


' ========================================================================================
' Command buttons. Ported from frmOptionsLocal_OnCommand; control reads/writes swapped and
' the file-open/save owner is now HWND_FRMOPTIONS.
' ========================================================================================
private sub frmOptionsLocal_SelectClick( byval hButton as HWND, byval id as long )
    ' Save any in-progress edit first.
    frmOptionsLocal_LocalEditCheck

    dim pwszName as wstring ptr = AfxIFileOpenDialogW( HWND_FRMOPTIONS, IDC_FRMOPTIONSLOCAL_CMDLOCALIZATION )
    if pwszName then
        frmOptionsLocal_LoadLocalizationFile( *pwszName )
        gLocalPhrasesEdit = false
        PsListTree_SetCurSel( ghLocalList, -1 )
        PsTextBox_SetText( ghLocalEnglish, "" )
        PsTextBox_SetText( ghLocalTranslate, "" )
        gLocalCurrentFile = AfxStrPathname( "NAMEX", *pwszName )
        gLocalTransLabel  = AfxStrPathName( "NAME", *pwszName )
        frmOptionsLocal_DisableEnglish()
        CoTaskMemFree( pwszName )
    end if
end sub

private sub frmOptionsLocal_NewClick( byval hButton as HWND, byval id as long )
    frmOptionsLocal_LocalEditCheck

    dim wzFilename as wstring * MAX_PATH
    dim pwszName as wstring ptr = AfxIFileSaveDialog( HWND_FRMOPTIONS, @wzFilename, @wstr("lang"), IDC_FRMOPTIONSLOCAL_CMDNEW )
    if pwszName then
        frmOptionsLocal_ClearGlobalArray()
        gLocalCurrentFile = AfxStrPathname( "NAMEX", *pwszName )
        gLocalTransLabel  = AfxStrPathName( "NAME", *pwszName )
        gLocalPhrasesEdit = true
        frmOptionsLocal_DisableEnglish()
        PsListTree_SetCurSel( ghLocalList, 0 )
        frmOptionsLocal_UpdateTextBoxes()
        SetFocus( PsTextBox_GetRichEditHandle( ghLocalTranslate ) )
        CoTaskMemFree( pwszName )
    end if
end sub

private sub frmOptionsLocal_EditClick( byval hButton as HWND, byval id as long )
    dim as DWSTRING wszLanguage = _
        AfxGetExePathName & wstr("settings\languages\") & gLocalCurrentFile
    frmOptionsLocal_LoadLocalizationFile( wszLanguage )
    gLocalTransLabel = AfxStrPathName( "NAME", wszLanguage )
    gLocalPhrasesEdit = true
    frmOptionsLocal_DisableEnglish()
    PsListTree_SetCurSel( ghLocalList, 0 )
    frmOptionsLocal_UpdateTextBoxes()
    SetFocus( PsTextBox_GetRichEditHandle( ghLocalTranslate ) )
end sub

private sub frmOptionsLocal_DeleteClick( byval hButton as HWND, byval id as long )
    ' Was MB_YESNOCANCEL; OK/Cancel is behaviourally identical here (No and Cancel both
    ' abort) and, unlike PsMessageBox's English-only Yes/No preset, stays localized.
    if frmOptions_ThemedMsgBox( L(366,"Are you sure you want to delete?"), _
                                L(276,"Confirm"), MBX_ICON_QUESTION, MBX_BTN_OKCANCEL ) <> IDOK then
        exit sub
    end if

    dim as DWSTRING wszFileName = AfxGetExePathName + wstr("settings\languages\") + gLocalCurrentFile
    frmOptionsLocal_ClearGlobalArray()
    AfxDeleteFile( wszFileName )
    PsListTree_SetCurSel( ghLocalList, -1 )
    PsTextBox_SetText( ghLocalEnglish, "" )
    PsTextBox_SetText( ghLocalTranslate, "" )
    gLocalCurrentFile = wstr("english.lang")
    gLocalTransLabel  = wstr("english")
    gLocalPhrasesEdit = false
    frmOptionsLocal_DisableEnglish()
end sub


' ========================================================================================
' Paint one phrase row (two columns: index, English phrase).
' ========================================================================================
private sub frmOptionsLocal_ListPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )

    if p->columnCount <= 0 then
        dim as RECT rcText = p->rc
        rcText.left += AfxScaleX(6)
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
        exit sub
    end if

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(6)
        p->b->PaintText( p->cells[i].wszText, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub


' ========================================================================================
' Paint the column header band.
' ========================================================================================
private sub frmOptionsLocal_HeaderPaint( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr = ghPanel.BackColor
    if p->isHot orelse p->isResizeHot then backclr = ghPanel.BackColorHot
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( ghPanel.ForeColorHot )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(6)
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( ghEditor.Divider )
    p->b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' Build the controls onto hPage. Idempotent.
' ========================================================================================
sub frmOptionsLocal_Build( byval hPage as HWND )

    if gLocalBuilt then exit sub

    ' ---- command buttons -------------------------------------------------------------
    ghLocalSelect = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDLOCALIZATION )
    PsButton_SetText( ghLocalSelect, L(126,"Select") )
    PsButton_SetFont( ghLocalSelect, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalSelect, @frmOptionsLocal_SelectClick )

    ghLocalNew = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDNEW )
    PsButton_SetText( ghLocalNew, L(3,"New") )
    PsButton_SetFont( ghLocalNew, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalNew, @frmOptionsLocal_NewClick )

    ghLocalEdit = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDEDIT )
    PsButton_SetText( ghLocalEdit, L(14,"Edit") )
    PsButton_SetFont( ghLocalEdit, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalEdit, @frmOptionsLocal_EditClick )

    ghLocalDelete = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDDELETE )
    PsButton_SetText( ghLocalDelete, L(282,"Delete") )
    PsButton_SetFont( ghLocalDelete, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalDelete, @frmOptionsLocal_DeleteClick )

    ' ---- phrase list -----------------------------------------------------------------
    ghLocalList = PsListTree_Create( hPage, IDC_FRMOPTIONSLOCAL_LSTPHRASES )
    PsListTree_AddColumn( ghLocalList, "Index", AfxScaleX(60) )
    PsListTree_AddColumn( ghLocalList, "English Phrase", AfxScaleX(280) )
    PsListTree_SetFillColumn( ghLocalList, 1 )
    PsListTree_ShowHeader( ghLocalList, true )
    PsListTree_SetPaintCallback( ghLocalList, @frmOptionsLocal_ListPaint )
    PsListTree_SetHeaderPaintCallback( ghLocalList, @frmOptionsLocal_HeaderPaint )
    PsListTree_SetSelChangeCallback( ghLocalList, @frmOptionsLocal_ListSelChange )
    PsListTree_SetFont( ghLocalList, OptionsFont_Base() )

    ' ---- English (readonly) and translation text fields ------------------------------
    ghLocalEnglish = PsTextBox_Create( hPage, IDC_FRMOPTIONSLOCAL_TXTENGLISH )
    PsTextBox_SetReadOnly( ghLocalEnglish, true )

    ghLocalTranslate = PsTextBox_Create( hPage, IDC_FRMOPTIONSLOCAL_TXTTRANSLATE )
    PsTextBox_SetChangeCallback( ghLocalTranslate, @frmOptionsLocal_TranslateChanged )

    ' ---- populate the phrase list from the English reference (once) -------------------
    redim gLocalPhrases( ubound(gLangEnglish) )
    frmOptionsLocal_LoadLocalizationFile( gLocalCurrentFile )
    PsListTree_BeginUpdate( ghLocalList )
    for i as long = lbound(gLangEnglish) to ubound(gLangEnglish)
        dim as DWSTRING wKey = AfxStrRset( str(i), 5, "0" )
        ListBox_AddTabbedRow( ghLocalList, wKey & chr(9) & gLangEnglish(i) )
    next
    PsListTree_EndUpdate( ghLocalList )

    gLocalBuilt = true
    frmOptionsLocal_ApplyTheme()
    frmOptionsLocal_DisableEnglish()
end sub


' ========================================================================================
' Theme the controls.
' ========================================================================================
sub frmOptionsLocal_ApplyTheme()
    if gLocalBuilt = false then exit sub

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    PsButton_SetColors( ghLocalSelect, @bc ) : PsButton_Refresh( ghLocalSelect )
    PsButton_SetColors( ghLocalNew, @bc )    : PsButton_Refresh( ghLocalNew )
    PsButton_SetColors( ghLocalEdit, @bc )   : PsButton_Refresh( ghLocalEdit )
    PsButton_SetColors( ghLocalDelete, @bc ) : PsButton_Refresh( ghLocalDelete )

    PsListTree_SetBackColor( ghLocalList, ghPanel.BackColor )
    PsListTree_SetScrollBarColors( ghLocalList, ghPanel.BackColorScrollBar, _
                                 ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )

    OptionsTheme_ApplyTextBox( ghLocalEnglish )
    OptionsTheme_ApplyTextBox( ghLocalTranslate )
end sub


' ========================================================================================
' Show or hide the page. Buttons + filename label always; the edit area per gLocalShowEdit.
' ========================================================================================
sub frmOptionsLocal_Show( byval bShow as boolean )
    if gLocalBuilt = false then exit sub
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ShowWindow( ghLocalSelect, nCmd )
    ShowWindow( ghLocalNew, nCmd )
    ShowWindow( ghLocalEdit, nCmd )
    ShowWindow( ghLocalDelete, nCmd )
    if bShow then
        frmOptionsLocal_ApplyEditVisibility()
    else
        ShowWindow( ghLocalList, SW_HIDE )
        ShowWindow( ghLocalEnglish, SW_HIDE )
        ShowWindow( ghLocalTranslate, SW_HIDE )
    end if
end sub


' ========================================================================================
' Record one drawn label.
' ========================================================================================
private sub frmOptionsLocal_AddLabel( byval wszCaption as DWSTRING, _
                                      byval nLeft as long, byval nTop as long, _
                                      byval nRight as long, byval nBottom as long )
    dim as long idx = ubound(gLocalLabels) + 1
    redim preserve gLocalLabels(idx) as LOCAL_LABEL
    gLocalLabels(idx).rc = type<RECT>( nLeft, nTop, nRight, nBottom )
    gLocalLabels(idx).caption = wszCaption
end sub


' ========================================================================================
' Lay out for a given width; return content height. Only the visible portion is laid out
' and measured, so the scroll region shrinks when the edit area is hidden.
' ========================================================================================
function frmOptionsLocal_Layout( byval hPage as HWND, byval cxPanel as long ) as long

    if gLocalBuilt = false then return 0

    dim as long padLeft  = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight = AfxScaleX(OPTROW_PADRIGHT)
    dim as long contentR = cxPanel - padRight
    dim as long fullW    = contentR - padLeft
    if fullW < 0 then fullW = 0

    ' Base-font line height, for the labels and the field heights.
    dim as long textH = 0
    dim as HDC hDC = GetDC( hPage )
    if hDC then
        dim as HFONT hOld = SelectObject( hDC, OptionsFont_Base() )
        dim as TEXTMETRICW tm : GetTextMetricsW( hDC, @tm )
        SelectObject( hDC, hOld )
        ReleaseDC( hPage, hDC )
        textH = tm.tmHeight
    end if

    dim as long fieldH    = textH + AfxScaleY(LOCAL_FIELDPAD) * 2
    dim as long labelGapY = AfxScaleY(LOCAL_LABELGAP)
    dim as long sectionGap = AfxScaleY(LOCAL_SECTIONGAP)
    dim as long y = AfxScaleY(OPTROW_PADTOP)

    erase gLocalLabels

    ' ---- command button row ----------------------------------------------------------
    dim as long btnW = AfxScaleX(LOCAL_BTNWIDTH)
    dim as long btnH = AfxScaleY(LOCAL_BTNHEIGHT)
    dim as long btnGap = AfxScaleX(LOCAL_BTNGAP)
    dim as long bx = padLeft
    SetWindowPos( ghLocalSelect, 0, bx, y, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : bx += btnW + btnGap
    SetWindowPos( ghLocalNew,    0, bx, y, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : bx += btnW + btnGap
    SetWindowPos( ghLocalEdit,   0, bx, y, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : bx += btnW + btnGap
    SetWindowPos( ghLocalDelete, 0, bx, y, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += btnH + sectionGap

    ' ---- current localization file ---------------------------------------------------
    frmOptionsLocal_AddLabel( L(129,"Localization") & ": " & gLocalCurrentFile, _
                              padLeft, y, contentR, y + textH )
    y += textH + sectionGap

    ' ---- edit area (only when a non-English file is being edited) ---------------------
    if gLocalShowEdit then
        dim as long listH = AfxScaleY(LOCAL_LISTHEIGHT)
        SetWindowPos( ghLocalList, 0, padLeft, y, fullW, listH, SWP_NOZORDER or SWP_NOACTIVATE )
        y += listH + sectionGap

        frmOptionsLocal_AddLabel( "English", padLeft, y, contentR, y + textH )
        y += textH + labelGapY
        SetWindowPos( ghLocalEnglish, 0, padLeft, y, fullW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
        y += fieldH + sectionGap

        frmOptionsLocal_AddLabel( gLocalTransLabel, padLeft, y, contentR, y + textH )
        y += textH + labelGapY
        SetWindowPos( ghLocalTranslate, 0, padLeft, y, fullW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
        y += fieldH
    end if

    function = y + AfxScaleY(OPTROW_PADBOTTOM)
end function


' ========================================================================================
' Paint the drawn labels. Background is filled by the caller first.
' ========================================================================================
sub frmOptionsLocal_Paint( byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    if gLocalBuilt = false then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( ghPanel.BackColor )
    p->b->SetForeColor( ghPanel.ForeColor )
    for i as long = lbound(gLocalLabels) to ubound(gLocalLabels)
        dim as RECT rc = gLocalLabels(i).rc
        p->b->PaintText( gLocalLabels(i).caption, @rc, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub
