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
'  THE SHAPE IS TWO MUTUALLY EXCLUSIVE VIEWS of one page, under a drawn heading naming the
'  current language:
'
'    PICKING  a LANGUAGES list of everything in settings\languages, full page height, with
'             Select / New / Edit / Delete stacked at the top of the strip beside it.
'    EDITING  a Back button, and a PHRASE list IN THEIR PLACE whose third column is edited in
'             place -- double-click, ENTER or F2.
'
'  Nothing is shared between the two, which is what lets each list take the whole page instead
'  of sharing it. They are two states of one page rather than two windows: a child dialog would
'  need its own creation, theming, layout and pump handling and would still be showing and
'  hiding the same controls at the end of it.
'
'  TWO THINGS THAT USED TO BE HERE AND ARE NOT.
'    1. Select no longer opens a shell file dialog. The languages list IS the picker, so Select /
'       Edit / Delete all act on the row highlighted there rather than on gLocalCurrentFile. New
'       keeps its Save-As dialog -- it has to name a file that does not exist yet.
'    2. The English and translation PsTextBoxes are gone, and the two drawn labels over them with
'       them. The translation is typed into the phrase list's third column, which means
'       PsListTree_EnableLabelEdit -- and THAT is a pump obligation: frmOptions_Show must call
'       PsListTree_FilterMessage ahead of both its Escape handler and IsDialogMessage.
'
'  THE HEIGHT BUDGET IS THE CONSTRAINT. The page container is a fixed 680 x 510 unscaled and does
'  not scroll; frmOptions_LayoutPlainPage DISCARDS the height this returns, so an overflow is
'  silent and only TIKO_OPTIONS_SELFTEST's fit test reports it. BOTH views therefore size their
'  list as the EXACT REMAINDER of the container rather than to a row count -- overflow is
'  unreachable by construction, and each list is as tall as its view allows.
'
'  The file I/O and the gLocalPhrases / gLocalPhrasesEdit business logic are carried over from the
'  old page unchanged.

#include once "frmOptionsLocal.bi"
#include once "frmOptions.bi"          ' HWND_FRMOPTIONS, frmOptions_ThemedMsgBox
#include once "modOptionsRows.bi"      ' gOptWork, shared padding + theme helpers
#include once "modRoutines.bi"         ' AfxIFileOpenDialogW / SaveDialog, ListBox_AddTabbedRow
#include once "clsConfig.bi"


' Fixed metrics, UNSCALED.
#define LOCAL_ROWHEIGHT      22        ' PsListTree's own default; named so the budget can use it
#define LOCAL_HEADERHEIGHT   24        ' PsListTree's own default, likewise
#define LOCAL_SECTIONGAP     14
#define LOCAL_HINTGAP         6        ' phrase list -> the "click a cell" hint under it
#define LOCAL_BTNWIDTH       84
#define LOCAL_BTNHEIGHT      30
#define LOCAL_BTNGAP          8        ' between stacked buttons AND between list and stack
#define LOCAL_LANGNAMEW     180        ' languages list, column 0 ("Language")
#define LOCAL_ACTIVEW       90        ' languages list, column 2 (the "Active" marker)
#define LOCAL_PHRASEIDXW     60        ' phrase list, column 0 ("Index")
#define LOCAL_PHRASEENGW    230        ' phrase list, column 1 ("English Phrase")
' Left inset of a cell's TEXT inside its column, UNSCALED. Used by both painters AND handed to
' PsListTree_SetEditTextInset, so the in-place editor's text lands on the same x as the text it
' replaces. Two literals that had to agree was how they came to disagree.
#define LOCAL_CELLINSET       6

' Phrase-list columns. Named because the third one is the only editable cell in the dialog and
' three separate places have to agree on which it is.
#define LOCALCOL_INDEX        0
#define LOCALCOL_ENGLISH      1
#define LOCALCOL_TRANSLATE    2


type LOCAL_LABEL
    rc      as RECT
    caption as DWSTRING
end type


' THE SELECTED LANGUAGE -- the one the editor will run in, written to gConfig.LocalizationFile
' on OK. ONLY the Select button changes it (and Delete, when it removes the selected file).
dim shared gLocalCurrentFile as DWSTRING
' The language currently OPEN FOR EDITING, which is a different question -- editing French to
' fix a typo must not switch the application into French. This is what LocalEditCheck writes
' back to disk. "" when nothing is being edited.
dim shared gLocalEditFile    as DWSTRING
' Has the SELECTED language's own file been rewritten during this dialog session? That -- not
' "was anything edited" -- is what decides whether the running application needs restarting:
' editing French while the editor runs in English changes nothing this session.
dim shared gLocalSelectedEdited as boolean
dim shared gLocalTransLabel  as DWSTRING     ' language name, used for the third column's caption
dim shared gLocalShowEdit    as boolean      ' the phrase list is visible
dim shared gLocalBuilt       as boolean

dim shared ghLocalSelect    as HWND
dim shared ghLocalNew       as HWND
dim shared ghLocalEdit      as HWND
dim shared ghLocalDelete    as HWND
dim shared ghLocalClose     as HWND      ' shown only while a language is open for editing
dim shared ghLocalLanguages as HWND
dim shared ghLocalList      as HWND

' Filename per languages-list row -- the list shows a prettified name, so the row index cannot be
' turned back into a file without this. Same shape as frmThemes' gThemeFilenames().
dim shared gLocalLangFiles(any) as DWSTRING

dim shared gLocalLabels(any) as LOCAL_LABEL


' Forward declarations for the internal helpers used before their definition.
declare sub frmOptionsLocal_DisableEnglish()
declare function frmOptionsLocal_LoadLocalizationFile( byref wszFilename as wstring ) as long
declare sub frmOptionsLocal_ClearGlobalArray()
declare function frmOptionsLocal_LoadLanguages() as long
declare sub frmOptionsLocal_FillTranslationColumn()
declare sub frmOptionsLocal_MarkActiveRow()


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
    gLocalEditFile    = ""
    gLocalSelectedEdited = false
    gLocalTransLabel  = PsPathStem(gConfig.LocalizationFile )
    gLocalPhrasesEdit = false
    gLocalShowEdit    = false
end sub


' ========================================================================================
' Forget all state. Controls live on the page container and die with it, so their handles are
' stale on the next open -- Build must not short-circuit on a stale gLocalBuilt.
' ========================================================================================
sub frmOptionsLocal_Reset()
    gLocalBuilt       = false
    ghLocalSelect     = 0
    ghLocalNew        = 0
    ghLocalEdit       = 0
    ghLocalDelete     = 0
    ghLocalClose      = 0
    gLocalEditFile    = ""
    ghLocalLanguages  = 0
    ghLocalList       = 0
    erase gLocalLabels
    erase gLocalLangFiles
end sub


' ========================================================================================
' If a localization file is currently being edited then save it. (Unchanged logic; the current
' filename comes from gLocalCurrentFile and the phrase rows from the PsListTree.)
' ========================================================================================
function frmOptionsLocal_LocalEditCheck() as long
    if gLocalPhrasesEdit = false then exit function

    dim as DWSTRING wszLocalName = gLocalEditFile
    dim as DWSTRING wszFileName = PsExePath + wstr("settings\languages\" + wszLocalName)
    dim as DWSTRING wszTemp, wszPhrase

    ' Never write over the English reference file.
    if ucase(trim(PsPathName(wszFilename ))) = "ENGLISH.LANG" then exit function
    ' No edit target, no write. Defensive: an empty name would leave wszFileName pointing at
    ' the languages FOLDER, and the first thing below it does is PsFileDelete.
    if PsLen(wszLocalName) = 0 then exit function
    if ghLocalList = 0 then exit function

    dim as long nCount = PsListTree_GetCount( ghLocalList )

    PsFileDelete(wszFileName)

    dim pStream as CTextStream
    if pStream.OpenForOutputW(wszFilename, true) = S_OK then

        wszTemp = _
        "' Localization file for " & ucase(PsPathStem(wszLocalName)) & " language" & vbcrlf & _
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

        ' KEY:PHRASE AND NOTHING ELSE. Each line used to carry the English original after a
        ' ";", padded out to column 60 -- a translator's crib sheet written into the data
        ' file. It cost more than it gave: the English is right there in the editor's own
        ' left column, the padding made every line 60+ characters of mostly spaces, and only
        ' three of the six shipped files had ever been through this writer, so the set
        ' disagreed with itself about its own format.
        '
        ' BOTH LOADERS STILL SPLIT ON ";" (here and in modRoutines) so a file written by an
        ' older build still reads correctly. That is also why this was safe to remove: the
        ' comment was never part of the value, and no phrase may contain a ";" -- a
        ' constraint the loaders have always imposed.
        for i as long = 0 to nCount - 1
            dim as DWSTRING wszIndex = PsListTree_GetCellText( ghLocalList, i, LOCALCOL_INDEX )
            dim as long idx = val(wszIndex)
            wszPhrase = PsStrRSet(wszIndex, 5, "0") + wstr(":") + gLocalPhrases(idx)
            pStream.WriteLine PsRTrim(wszPhrase)
        next
    end if
    pStream.Close

    ' The file that was just written IS the one the editor runs in, so its new contents
    ' will not be seen until the next start. Recorded here, at the one place a write
    ' actually happens, rather than inferred later from "an edit was open".
    if ucase(gLocalEditFile) = ucase(gLocalCurrentFile) then gLocalSelectedEdited = true

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

    if PsFileExists( wszFileName ) = 0 then exit function

    dim as BSTRING wst, wKey, wData
    dim as long nKey, nData, i

    dim pStream as CTextStream
    if pStream.OpenUnicode( wszFileName ) <> S_OK then exit function

    frmOptionsLocal_ClearGlobalArray()

    do until pStream.EOS
        wst = pStream.ReadLine

        if PsLen(wst) = 0 then continue do
        if PsLeft(wst, 1) = "'" then continue do

        i = PsInStr(wst, ":")
        if i = 0 then continue do

        wKey = "": wData = "": nData = 0

        wKey  = PsLeft(wst, i-1)
        wData = PsMid(wst, i+1)    ' MID causes problems with Chinese data so ** is used.

        nKey  = val(wKey)
        nData = val(wData)

        ' MAXIMUM for a non-English file does not redimension gLocalPhrases -- that array is
        ' always sized to the main ENGLISH array.
        if PsUCase(wKey) <> "MAXIMUM" then
            if (nKey >= lbound(gLocalPhrases)) andalso (nKey <= ubound(gLocalPhrases)) then
                ' Use ** so cyrillic gets converted correctly; FB intrinsics (RTRIM) convert
                ' those incorrectly through CBSTR / DWSTRING.
                gLocalPhrases(nKey) = rtrim(**PsStrParse(wData, 1, ";"), any chr(9,32))
                ' ENGLISH IS THE CANONICAL REFERENCE. An id the English file has no string for
                ' is an unused slot, so every other language is blank there too -- a translation
                ' sitting in one is stale data, not content, and is dropped on load. Deliberate;
                ' don't "fix" it into keeping the text.
                if gLangEnglish(nKey) = "" then gLocalPhrases(nKey) = ""
                ' NO ENGLISH FALLBACK HERE. gLocalPhrases holds the TRANSLATIONS and nothing
                ' else, so blank means "not translated yet" -- which is what the third column
                ' should show and what LocalEditCheck should write. Filling blanks with the
                ' English text made every saved file a copy of english.lang with a few words
                ' changed, and made "New" behave like a clone. The fallback belongs at RUNTIME:
                ' LoadLocalizationFile in modRoutines.inc already substitutes English for any
                ' key whose local string is empty, which is the only place it is needed.
            end if
        end if
    loop
    pStream.Close

    ' Keys past the end of the local file are left EMPTY -- untranslated, which is the truth.
    ' They used to be filled from English, so a file that stopped at key 200 came back with
    ' 300 English phrases appended and saved them all to disk on the way out.
    ' ClearGlobalArray above already blanked them, so there is nothing to do.

    function = 0
end function


' ========================================================================================
' "chinese_simplified.lang" -> "Chinese Simplified". Display only -- gLocalLangFiles() carries
' the filename, and every file operation goes through that.
' ========================================================================================
private function frmOptionsLocal_PrettyName( byval wszFilename as DWSTRING ) as DWSTRING
    dim as DWSTRING wszName = PsPathStem(wszFilename )
    dim as DWSTRING wszOut = ""
    dim as boolean bStart = true

    for i as long = 1 to PsLen(wszName)
        dim as DWSTRING wszCh = PsMid(wszName, i, 1)
        if wszCh = "_" then
            wszOut += " "
            bStart = true
        else
            wszOut += iif( bStart, PsUCase(wszCh), PsLCase(wszCh) )
            bStart = false
        end if
    next

    return wszOut
end function


' ========================================================================================
' Fill the languages list from settings\languages. Returns the row that matches
' gLocalCurrentFile, or -1. Same shape as frmThemes_LoadThemes.
' ========================================================================================
private function frmOptionsLocal_LoadLanguages() as long
    if ghLocalLanguages = 0 then return -1

    PsListTree_BeginUpdate( ghLocalLanguages )
    PsListTree_Clear( ghLocalLanguages )
    erase gLocalLangFiles

    dim as long idxMatch = -1
    dim hSearch as HANDLE
    dim WFD as WIN32_FIND_DATAW
    dim as DWSTRING wszPath = PsExePath + "settings\languages\"

    hSearch = FindFirstFile( wszPath + "*", @WFD )
    if hSearch <> INVALID_HANDLE_VALUE then
        do
            dim as DWSTRING wszFile = WFD.cFileName
            ' Directories and anything that is not a .lang are skipped. FindFirstFile is given
            ' "*" rather than "*.lang" because the 8.3 alias matching on a wildcard extension
            ' also returns names that merely START with .lang -- the same reason frmThemes and
            ' frmOptionsCompiler filter in code.
            dim as boolean bDir = ((WFD.dwFileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0)
            dim as boolean bLang = (ucase(PsPathExt(wszFile )) = ".LANG")
            if (bDir = false) andalso (bLang = true) then
                dim as long idx = PsListTree_AddString( ghLocalLanguages, frmOptionsLocal_PrettyName( wszFile ) )
                if idx >= 0 then
                    PsListTree_SetCellText( ghLocalLanguages, idx, 1, wszFile )
                    redim preserve gLocalLangFiles(idx) as DWSTRING
                    gLocalLangFiles(idx) = wszFile
                    if ucase(gLocalCurrentFile) = PsUCase(wszFile) then idxMatch = idx
                end if
            end if
        loop while FindNextFile( hSearch, @WFD )
        FindClose( hSearch )
    end if

    PsListTree_EndUpdate( ghLocalLanguages )

    frmOptionsLocal_MarkActiveRow()

    if idxMatch >= 0 then
        PsListTree_SetCurSel( ghLocalLanguages, idxMatch )    ' silent
        ' ...but do NOT let it scroll. SetCurSel brings the row into view, so a configured
        ' language part-way down the alphabet opened the list already scrolled -- with the
        ' languages above it off the top of a list that had room for all of them. The list is
        ' read from the first row, so it starts there whatever happens to be selected.
        PsListTree_SetTopIndex( ghLocalLanguages, 0 )
    end if
    return idxMatch
end function


' ========================================================================================
' The filename behind the highlighted languages row, or "" if nothing is highlighted.
' ========================================================================================
' The languages-list row carrying a given filename, or -1. Used after a reload to put the
' highlight back on a file the row indices have moved under.
private function frmOptionsLocal_RowForFile( byval wszFile as DWSTRING ) as long
    if PsLen(wszFile) = 0 then return -1
    for i as long = lbound(gLocalLangFiles) to ubound(gLocalLangFiles)
        if ucase(gLocalLangFiles(i)) = PsUCase(wszFile) then return i
    next
    return -1
end function


' Put the "Active" marker on the row holding the SELECTED language and nowhere else.
'
' Called from every place the selection can move, not just the list reload: Select changes
' gLocalCurrentFile without rebuilding the list, so a marker written only at load time would
' still be sitting on the previous language.
private sub frmOptionsLocal_MarkActiveRow()
    if ghLocalLanguages = 0 then exit sub
    ' BeginUpdate/EndUpdate, NOT AfxRedrawWindow. PsListTree_SetCellText writes the model and
    ' does not repaint -- unlike most of the control's mutators it never calls NotifyChange --
    ' and AfxRedrawWindow on this handle redraws the CONTAINER, while the rows are painted by
    ' its surface child. So the marker moved in the model and nothing on screen changed until
    ' some later repaint happened to come along. EndUpdate calls Refresh, which is what actually
    ' rebuilds and repaints the surface.
    PsListTree_BeginUpdate( ghLocalLanguages )
    for i as long = lbound(gLocalLangFiles) to ubound(gLocalLangFiles)
        dim as boolean bActive = (ucase(gLocalLangFiles(i)) = ucase(gLocalCurrentFile))
        ' if/else, not iif: iif cannot return a UDT, and DWSTRING is one.
        dim as DWSTRING wszMark = ""
        if bActive then wszMark = L(324,"Active")
        PsListTree_SetCellText( ghLocalLanguages, i, 2, wszMark )
    next
    PsListTree_EndUpdate( ghLocalLanguages )
end sub


private function frmOptionsLocal_SelectedFile() as DWSTRING
    ' RETURN throughout, never 'function =' -- fbc refuses the two styles mixed in a function
    ' returning a type with constructors, and DWSTRING is one.
    if ghLocalLanguages = 0 then return ""
    dim as long idx = PsListTree_GetCurSel( ghLocalLanguages )
    if idx < lbound(gLocalLangFiles) then return ""
    if idx > ubound(gLocalLangFiles) then return ""
    return gLocalLangFiles(idx)
end function


' ========================================================================================
' Write gLocalPhrases into the phrase list's third column, and retitle that column for the
' language being edited. Called whenever a file is loaded into gLocalPhrases.
' ========================================================================================
private sub frmOptionsLocal_FillTranslationColumn()
    if ghLocalList = 0 then exit sub

    PsListTree_SetColumnText( ghLocalList, LOCALCOL_TRANSLATE, _
                              frmOptionsLocal_PrettyName( gLocalTransLabel ) & " Phrase" )

    PsListTree_BeginUpdate( ghLocalList )
    dim as long nCount = PsListTree_GetCount( ghLocalList )
    for i as long = 0 to nCount - 1
        dim as long idx = val( PsListTree_GetCellText( ghLocalList, i, LOCALCOL_INDEX ) )
        if (idx >= lbound(gLocalPhrases)) andalso (idx <= ubound(gLocalPhrases)) then
            PsListTree_SetCellText( ghLocalList, i, LOCALCOL_TRANSLATE, gLocalPhrases(idx) )
        end if
    next
    PsListTree_EndUpdate( ghLocalList )
end sub


' ========================================================================================
' THE PAGE HAS TWO MUTUALLY EXCLUSIVE VIEWS and this is the switch between them.
'
'   PICKING  -- the languages list and its four commands.
'   EDITING  -- the Back button and the phrase list, IN THEIR PLACE.
'
' Nothing is shared between the two, so the phrase list gets the whole page instead of the
' scraps left under a languages list it cannot interact with. They are two states of one page
' rather than two windows: a child dialog would need its own creation, theming, layout and
' pump handling, and would still be showing and hiding the same controls at the end of it.
' ========================================================================================
private sub frmOptionsLocal_ApplyEditVisibility()
    dim as long nEdit = iif( gLocalShowEdit, SW_SHOW, SW_HIDE )
    dim as long nPick = iif( gLocalShowEdit, SW_HIDE, SW_SHOW )

    if ghLocalLanguages then ShowWindow( ghLocalLanguages, nPick )
    if ghLocalSelect then ShowWindow( ghLocalSelect, nPick )
    if ghLocalNew then ShowWindow( ghLocalNew, nPick )
    if ghLocalEdit then ShowWindow( ghLocalEdit, nPick )
    if ghLocalDelete then ShowWindow( ghLocalDelete, nPick )

    if ghLocalList then ShowWindow( ghLocalList, nEdit )
    if ghLocalClose then ShowWindow( ghLocalClose, nEdit )
end sub


' ========================================================================================
' Enable/disable the commands and decide whether the phrase list is visible.
'
' THE RULE MOVED WITH THE BUTTONS. It used to read gLocalCurrentFile, because that was the only
' file the page knew about. Select / Edit / Delete now act on the row highlighted in the
' languages list, so the rule reads THAT -- english.lang highlighted means Edit and Delete are
' refused, which is the same protection pointed at the thing the buttons operate on.
' ========================================================================================
sub frmOptionsLocal_DisableEnglish()
    if gLocalBuilt = false then exit sub

    dim as DWSTRING wszSelected = frmOptionsLocal_SelectedFile()
    dim as boolean bEnglish = iif( PsUCase(wszSelected) = "ENGLISH.LANG", true, false )
    dim as boolean bHaveSel = iif( PsLen(wszSelected) > 0, true, false )

    ' Select and New are always available; Select needs something highlighted to select.
    PsButton_SetEnabled( ghLocalSelect, bHaveSel )
    PsButton_SetEnabled( ghLocalNew, true )

    if bEnglish orelse (bHaveSel = false) then
        ' The English reference file cannot be edited or deleted.
        PsButton_SetEnabled( ghLocalEdit, false )
        PsButton_SetEnabled( ghLocalDelete, false )
    else
        PsButton_SetEnabled( ghLocalDelete, true )
        ' Edit is available only when not already editing.
        PsButton_SetEnabled( ghLocalEdit, iif( gLocalPhrasesEdit, false, true ) )
    end if

    ' The phrase list follows whether a file is OPEN for editing, which is independent of what
    ' happens to be highlighted -- highlighting english.lang mid-edit must not tear the list away.
    gLocalShowEdit = gLocalPhrasesEdit

    frmOptionsLocal_ApplyEditVisibility()

    ' The page's content height changed with the phrase list's visibility, so re-flow it.
    frmOptions_RelayoutCurrentPage()
end sub


' ========================================================================================
' The languages list selection changed (mouse or keyboard) -- re-evaluate the commands.
' ========================================================================================
private sub frmOptionsLocal_LangSelChange( byval hList as HWND, byval row as integer )
    frmOptionsLocal_DisableEnglish()
end sub


' ========================================================================================
' A language row's default action -- double-click, or ENTER on the row -- is Edit.
'
' It goes through PsButton_Click rather than calling the handler, and that is the point:
' PsButton_Click refuses a DISABLED button, so the "english.lang cannot be edited" and "not
' while already editing" rules are enforced by the button's own enabled state. Re-testing them
' here would be a second copy of a rule that already exists, free to drift.
' ========================================================================================
private sub frmOptionsLocal_LangActivate( byval hList as HWND, byval row as integer )
    PsButton_Click( ghLocalEdit )
end sub


' ========================================================================================
' A phrase row's default action -- DOUBLE-CLICK, or ENTER -- opens the translation editor over
' the third column. Both arrive here through the one activate callback, so the two gestures
' cannot drift apart. F2 is the control's own and is pointed at the same column by
' PsListTree_SetEditColumn.
'
' Single click was tried first and is deliberately gone: a list you cannot click through without
' opening an editor is unusable for reading, and selecting a row to look at it is the commoner
' intent. The x coordinate is not tested either -- an even earlier version required the click to
' land inside the third column, so clicking the English text you had just read did nothing.
' ========================================================================================
private sub frmOptionsLocal_PhraseActivate( byval hList as HWND, byval row as integer )
    if gLocalPhrasesEdit = false then exit sub
    PsListTree_BeginEdit( hList, row, LOCALCOL_TRANSLATE )
end sub


' ========================================================================================
' Refuse to edit a row the English file has no string for.
'
' English is the canonical reference: an id it leaves blank is an unused slot, and the loader
' drops any translation found in one. Without this veto the row still ACCEPTED typing and the
' text still reached the file -- it was simply gone again on the next run, with nothing said.
' The veto is the control's own BeginLabelEdit hook, so it covers every route into the editor
' (double-click, ENTER and F2) rather than just the one the host wires up.
' ========================================================================================
private function frmOptionsLocal_PhraseBeginEdit( byval hList as HWND, byval row as integer ) as boolean
    dim as long idx = val( PsListTree_GetCellText( hList, row, LOCALCOL_INDEX ) )
    if (idx < lbound(gLangEnglish)) orelse (idx > ubound(gLangEnglish)) then return false
    return iif( len(gLangEnglish(idx)) > 0, true, false )
end function


' ========================================================================================
' An in-place edit committed (ENTER, or focus loss). Store into the phrase array against the
' row's own index. This is what the translation textbox's EN_CHANGE handler used to do.
'
' Returns FALSE for any column but the third, which REFUSES the write and leaves the cell alone:
' F2 opens column 0 whatever the host's gesture opened, and the index column is not the user's
' to change.
' ========================================================================================
private function frmOptionsLocal_ListEndEdit( byval hList as HWND, byval row as integer, _
                                              byval newText as DWSTRING ) as boolean
    if PsListTree_GetEditCol( hList ) <> LOCALCOL_TRANSLATE then return false

    dim as long idx = val( PsListTree_GetCellText( hList, row, LOCALCOL_INDEX ) )
    if (idx < lbound(gLocalPhrases)) orelse (idx > ubound(gLocalPhrases)) then return false

    gLocalPhrases(idx) = newText
    return true
end function


' ========================================================================================
' Command buttons. Select / Edit / Delete act on the highlighted languages row.
' ========================================================================================
private sub frmOptionsLocal_SelectClick( byval hButton as HWND, byval id as long )
    ' Save any in-progress edit first.
    frmOptionsLocal_LocalEditCheck

    dim as DWSTRING wszFile = frmOptionsLocal_SelectedFile()
    if PsLen(wszFile) = 0 then exit sub

    dim as DWSTRING wszFull = PsExePath & wstr("settings\languages\") & wszFile
    frmOptionsLocal_LoadLocalizationFile( wszFull )
    gLocalPhrasesEdit = false
    gLocalCurrentFile = wszFile          ' THE one place a user choice becomes the selection
    gLocalTransLabel  = PsPathStem(wszFile )
    frmOptionsLocal_MarkActiveRow()      ' the marker follows it
    PsListTree_SetCurSel( ghLocalList, -1 )
    frmOptionsLocal_DisableEnglish()
end sub

private sub frmOptionsLocal_NewClick( byval hButton as HWND, byval id as long )
    frmOptionsLocal_LocalEditCheck

    ' Still a Save-As dialog: this has to name a file that does not exist yet, so there is
    ' nothing in the languages list to pick. modRoutines switches on the control id to get the
    ' *.lang filter and the settings\languages default folder -- don't change that id.
    dim wzFilename as wstring * MAX_PATH
    dim pwszName as wstring ptr = AfxIFileSaveDialog( HWND_FRMOPTIONS, @wzFilename, @wstr("lang"), IDC_FRMOPTIONSLOCAL_CMDNEW )
    if pwszName then
        frmOptionsLocal_ClearGlobalArray()
        ' Opens the new file for EDITING. It does not become the selected language -- same
        ' rule as Edit: a brand-new, empty translation is the last thing to switch the
        ' application into. Select it afterwards if that is what you want.
        gLocalEditFile    = PsPathName(*pwszName )
        gLocalTransLabel  = PsPathStem(*pwszName )
        gLocalPhrasesEdit = true
        ' NO list reload here. The Save-As dialog picked a NAME; nothing has been written yet,
        ' so a reload at this point finds the same files as before and the new one appears to
        ' vanish. The list is refreshed by Back, after LocalEditCheck has created the file.
        frmOptionsLocal_FillTranslationColumn()
        frmOptionsLocal_DisableEnglish()
        PsListTree_SetCurSel( ghLocalList, 0 )
        CoTaskMemFree( pwszName )
    end if
end sub

private sub frmOptionsLocal_EditClick( byval hButton as HWND, byval id as long )
    dim as DWSTRING wszFile = frmOptionsLocal_SelectedFile()
    if PsLen(wszFile) = 0 then exit sub

    dim as DWSTRING wszLanguage = PsExePath & wstr("settings\languages\") & wszFile
    frmOptionsLocal_LoadLocalizationFile( wszLanguage )
    ' EDITING ONLY. This deliberately does NOT touch gLocalCurrentFile: opening French to fix
    ' a typo used to switch the whole application into French on OK, because one variable was
    ' doing both jobs. Double-click and ENTER route through this same handler.
    gLocalEditFile    = wszFile
    gLocalTransLabel  = PsPathStem(wszLanguage )
    gLocalPhrasesEdit = true
    frmOptionsLocal_FillTranslationColumn()
    frmOptionsLocal_DisableEnglish()
    PsListTree_SetCurSel( ghLocalList, 0 )
    ' Focus goes to the phrase list, so ENTER and F2 work without a click first. It has to come
    ' AFTER DisableEnglish, which is what makes the list visible -- SetFocus on a hidden window
    ' is refused outright and silently.
    SetFocus( ghLocalList )
end sub

' Leave the phrase editor and go back to picking a language. The edit is SAVED first --
' LocalEditCheck is the same writer OK uses, so closing and pressing OK produce the same file
' rather than two different ideas of "committed".
private sub frmOptionsLocal_CloseClick( byval hButton as HWND, byval id as long )
    ' Remembered across the save, because the reload below needs it and the clear loses it.
    dim as DWSTRING wszWasEditing = gLocalEditFile

    frmOptionsLocal_LocalEditCheck            ' this is what actually CREATES a New file
    gLocalPhrasesEdit = false
    gLocalEditFile    = ""        ' nothing is open now; the SELECTION is untouched

    ' RELOAD THE LANGUAGES LIST. A file created by New does not exist on disk until the write
    ' above -- AfxIFileSaveDialog only picks a NAME -- so New cannot put it in the list itself,
    ' and it was missing until the dialog was closed and reopened.
    frmOptionsLocal_LoadLanguages()
    ' Leave the highlight on the file just edited, so a new language can be Selected without
    ' hunting for it. LoadLanguages highlights the SELECTED language, which is not the same
    ' thing and, for a file just created, is never it.
    dim as long idxWas = frmOptionsLocal_RowForFile( wszWasEditing )
    if idxWas >= 0 then PsListTree_SetCurSel( ghLocalLanguages, idxWas )    ' silent

    frmOptionsLocal_DisableEnglish()          ' hides the phrase list and re-flows the page
    SetFocus( ghLocalLanguages )
end sub

private sub frmOptionsLocal_DeleteClick( byval hButton as HWND, byval id as long )
    dim as DWSTRING wszFile = frmOptionsLocal_SelectedFile()
    if PsLen(wszFile) = 0 then exit sub
    if PsUCase(wszFile) = "ENGLISH.LANG" then exit sub

    ' Was MB_YESNOCANCEL; OK/Cancel is behaviourally identical here (No and Cancel both abort)
    ' and, unlike PsMessageBox's English-only Yes/No preset, stays localized.
    if frmOptions_ThemedMsgBox( L(258,"Are you sure you want to delete?"), _
                                L(214,"Confirm"), TMB_ICON_QUESTION, TMB_OKCANCEL ) <> IDOK then
        exit sub
    end if

    dim as DWSTRING wszFileName = PsExePath + wstr("settings\languages\") + wszFile
    frmOptionsLocal_ClearGlobalArray()
    PsFileDelete( wszFileName )
    PsListTree_SetCurSel( ghLocalList, -1 )
    ' Falls back to English only when the file just deleted WAS the selection -- deleting some
    ' other language has no business changing which one the editor runs in.
    if ucase(gLocalCurrentFile) = PsUCase(wszFile) then gLocalCurrentFile = wstr("english.lang")
    gLocalEditFile    = ""
    gLocalTransLabel  = wstr("english")
    gLocalPhrasesEdit = false
    frmOptionsLocal_LoadLanguages()              ' the deleted file has to leave the list
    frmOptionsLocal_DisableEnglish()
end sub


' ========================================================================================
' No tooltips on either list, rows or headers.
'
' With NO callback the control falls back to the row's -- or the column's -- own text, so every
' row would sprout a tip repeating what is already on screen under the cursor. Returning "" is
' how a host says "none"; there is no separate disable switch. Both signatures are needed
' because the header is a different control with its own callback type.
' ========================================================================================
private function frmOptionsLocal_NoTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    return ""
end function

private function frmOptionsLocal_NoHeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' Paint one row of either list. Both are multi-column, so one painter serves both.
' ========================================================================================
private sub frmOptionsLocal_ListPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    else
        backclr = theme.panel.text.backcolor
        foreclr = theme.panel.text.forecolor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )

    if p->columnCount <= 0 then
        dim as RECT rcText = p->rc
        rcText.left += AfxScaleX(LOCAL_CELLINSET)
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
        exit sub
    end if

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(LOCAL_CELLINSET)
        p->b->PaintText( p->cells[i].wszText, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub


' ========================================================================================
' Paint the column header band.
' ========================================================================================
private sub frmOptionsLocal_HeaderPaint( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr = theme.panel.text.backcolor
    if p->isHot orelse p->isResizeHot then backclr = theme.panel.texthot.backcolor
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( theme.panel.texthot.forecolor )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(LOCAL_CELLINSET)
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.ui.divider )
    p->b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' Build the controls onto hPage. Idempotent.
' ========================================================================================
sub frmOptionsLocal_Build( byval hPage as HWND )

    if gLocalBuilt then exit sub

    ' ---- languages list ---------------------------------------------------------------
    ghLocalLanguages = PsListTree_Create( hPage, IDC_FRMOPTIONSLOCAL_LSTLANGUAGES )
    PsListTree_AddColumn( ghLocalLanguages, "Language", AfxScaleX(LOCAL_LANGNAMEW) )
    PsListTree_AddColumn( ghLocalLanguages, "File", AfxScaleX(160) )
    ' Status column, DELIBERATELY UNCAPTIONED: it holds a marker, not a field, and a heading
    ' over one word of text on one row reads as a column that is mostly empty rather than as
    ' a note about the row it is on. File stays the fill column, so this sits at the right
    ' edge and the layout above it is unchanged.
    PsListTree_AddColumn( ghLocalLanguages, "", AfxScaleX(LOCAL_ACTIVEW) )
    PsListTree_SetFillColumn( ghLocalLanguages, 1 )
    PsListTree_ShowHeader( ghLocalLanguages, true )
    PsListTree_SetPaintCallback( ghLocalLanguages, @frmOptionsLocal_ListPaint )
    PsListTree_SetHeaderPaintCallback( ghLocalLanguages, @frmOptionsLocal_HeaderPaint )
    PsListTree_SetSelChangeCallback( ghLocalLanguages, @frmOptionsLocal_LangSelChange )
    PsListTree_SetTooltipCallback( ghLocalLanguages, @frmOptionsLocal_NoTooltip )
    PsListTree_SetHeaderTooltipCallback( ghLocalLanguages, @frmOptionsLocal_NoHeaderTooltip )
    ' Double-click or ENTER on a language row = Edit. Setting this is also what makes the
    ' control claim ENTER for this list at all.
    PsListTree_SetActivateCallback( ghLocalLanguages, @frmOptionsLocal_LangActivate )
    PsListTree_SetFont( ghLocalLanguages, OptionsFont_Base() )

    ' ---- command buttons --------------------------------------------------------------
    ghLocalSelect = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDLOCALIZATION )
    PsButton_SetText( ghLocalSelect, L(121,"Select") )
    PsButton_SetFont( ghLocalSelect, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalSelect, @frmOptionsLocal_SelectClick )

    ghLocalNew = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDNEW )
    ' Its own id, NOT 3. Id 3 is the File menu's "New File" and has to keep saying that.
    PsButton_SetText( ghLocalNew, L(321,"New") )
    PsButton_SetFont( ghLocalNew, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalNew, @frmOptionsLocal_NewClick )

    ghLocalEdit = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDEDIT )
    PsButton_SetText( ghLocalEdit, L(14,"Edit") )
    PsButton_SetFont( ghLocalEdit, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalEdit, @frmOptionsLocal_EditClick )

    ghLocalDelete = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDDELETE )
    PsButton_SetText( ghLocalDelete, L(219,"Delete") )
    PsButton_SetFont( ghLocalDelete, OptionsFont_Base() )
    PsButton_SetClickCallback( ghLocalDelete, @frmOptionsLocal_DeleteClick )

    ' Back out of the editing view. Icon AND caption: the glyph alone would be the only
    ' unlabelled control on the page, and the caption alone reads as a command rather than as
    ' a way out. E72B is Segoe Fluent's Back arrow; the glyph font is the SEPARATE paint-time
    ' font the family requires, never the text font.
    ghLocalClose = PsButton_Create( hPage, IDC_FRMOPTIONSLOCAL_CMDCLOSE )
    PsButton_SetText( ghLocalClose, L(323,"Back") )
    PsButton_SetFont( ghLocalClose, OptionsFont_Base() )
    PsButton_SetGlyphFont( ghLocalClose, ghFont(SYMBOLFONT_10) )
    PsButton_SetGlyphLeft( ghLocalClose, !"\uE72B" )
    PsButton_SetClickCallback( ghLocalClose, @frmOptionsLocal_CloseClick )

    ' ---- phrase list ------------------------------------------------------------------
    ghLocalList = PsListTree_Create( hPage, IDC_FRMOPTIONSLOCAL_LSTPHRASES )
    PsListTree_AddColumn( ghLocalList, "Index", AfxScaleX(LOCAL_PHRASEIDXW) )
    PsListTree_AddColumn( ghLocalList, "English Phrase", AfxScaleX(LOCAL_PHRASEENGW) )
    PsListTree_AddColumn( ghLocalList, "Phrase", AfxScaleX(LOCAL_PHRASEENGW) )
    PsListTree_SetFillColumn( ghLocalList, LOCALCOL_TRANSLATE )
    PsListTree_ShowHeader( ghLocalList, true )
    PsListTree_SetPaintCallback( ghLocalList, @frmOptionsLocal_ListPaint )
    PsListTree_SetHeaderPaintCallback( ghLocalList, @frmOptionsLocal_HeaderPaint )
    PsListTree_SetTooltipCallback( ghLocalList, @frmOptionsLocal_NoTooltip )
    PsListTree_SetHeaderTooltipCallback( ghLocalList, @frmOptionsLocal_NoHeaderTooltip )
    PsListTree_SetFont( ghLocalList, OptionsFont_Base() )
    ' In-place editing of the third column. THIS IS THE PUMP OBLIGATION -- frmOptions_Show calls
    ' PsListTree_FilterMessage for it, ahead of its own Escape handler and IsDialogMessage.
    PsListTree_EnableLabelEdit( ghLocalList, true )
    ' Double-click and ENTER both go through the activate callback. SetEnterEdits is deliberately
    ' NOT used: it would give ENTER its own separate path into the editor, and the two gestures
    ' would then be two implementations of one behaviour.
    PsListTree_SetActivateCallback( ghLocalList, @frmOptionsLocal_PhraseActivate )
    ' F2 is the control's own gesture and it consumes WM_KEYDOWN itself, so the only way to point
    ' it at the translation column is to name that column. Without this it opens column 0 -- the
    ' read-only index.
    PsListTree_SetEditColumn( ghLocalList, LOCALCOL_TRANSLATE )
    PsListTree_SetEndLabelEditCallback( ghLocalList, @frmOptionsLocal_ListEndEdit )
    PsListTree_SetBeginLabelEditCallback( ghLocalList, @frmOptionsLocal_PhraseBeginEdit )
    ' The caret goes to the END of the existing translation rather than selecting it: these
    ' edits are amendments to a phrase that is already there, not the explorer-style rename the
    ' control defaults to, where the first keystroke is meant to replace everything.
    PsListTree_SetEditSelectAll( ghLocalList, false )
    ' Same inset the painters use, so the text does not shift sideways when an edit opens.
    PsListTree_SetEditTextInset( ghLocalList, AfxScaleX(LOCAL_CELLINSET) )

    ' ---- populate the phrase list from the English reference (once) -------------------
    redim gLocalPhrases( ubound(gLangEnglish) )
    frmOptionsLocal_LoadLocalizationFile( gLocalCurrentFile )
    PsListTree_BeginUpdate( ghLocalList )
    for i as long = lbound(gLangEnglish) to ubound(gLangEnglish)
        dim as DWSTRING wKey = PsStrRSet( str(i), 5, "0" )
        ListBox_AddTabbedRow( ghLocalList, wKey & chr(9) & gLangEnglish(i) )
    next
    PsListTree_EndUpdate( ghLocalList )

    gLocalBuilt = true
    frmOptionsLocal_LoadLanguages()
    frmOptionsLocal_FillTranslationColumn()
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
    PsButton_SetColors( ghLocalClose, @bc )  : PsButton_Refresh( ghLocalClose )

    PsListTree_SetBackColor( ghLocalLanguages, theme.panel.text.backcolor )
    PsListTree_SetScrollBarColors( ghLocalLanguages, theme.panel.scrollbar.backcolor, _
                                 theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    OptionsTheme_ApplyListBorder( ghLocalLanguages )

    PsListTree_SetBackColor( ghLocalList, theme.panel.text.backcolor )
    PsListTree_SetScrollBarColors( ghLocalList, theme.panel.scrollbar.backcolor, _
                                 theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    OptionsTheme_ApplyListBorder( ghLocalList )
    ' The in-place editor lives inside the control and is created lazily, so it is themed
    ' through the list rather than directly -- same four colours OptionsTheme_ApplyTextBox
    ' gives every other text field on the page, so the editor matches them. Applied here in
    ' ApplyTheme, not at build, so a theme change reaches it too.
    PsListTree_SetEditColors( ghLocalList, theme.ui.field.backcolor, theme.ui.field.forecolor, _
                              theme.ui.divider, theme.panel.texthot.forecolor )
end sub


' ========================================================================================
' Show or hide the page. Languages list + buttons always; the phrase list per gLocalShowEdit.
' ========================================================================================
sub frmOptionsLocal_Show( byval bShow as boolean )
    if gLocalBuilt = false then exit sub
    if bShow then
        ' Which controls appear is the VIEW's business, not this one's -- the page has two
        ' views and only one of them is on screen at a time.
        frmOptionsLocal_ApplyEditVisibility()
    else
        ShowWindow( ghLocalLanguages, SW_HIDE )
        ShowWindow( ghLocalSelect, SW_HIDE )
        ShowWindow( ghLocalNew, SW_HIDE )
        ShowWindow( ghLocalEdit, SW_HIDE )
        ShowWindow( ghLocalDelete, SW_HIDE )
        ShowWindow( ghLocalList, SW_HIDE )
        ShowWindow( ghLocalClose, SW_HIDE )
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
' Lay out for a given width; return content height.
'
' The phrase list gets THE EXACT REMAINDER of the viewport rather than a constant height. The
' page container does not scroll and frmOptions_LayoutPlainPage discards this return value, so a
' fixed height that happened to overflow would simply be clipped with nothing to report it.
' Sizing off the container's own client height makes overflow unreachable and makes the list as
' tall as the page allows. BOTH views size their list that way now -- neither has anything
' underneath it, so neither has a reason to stop short.
' ========================================================================================
function frmOptionsLocal_Layout( byval hPage as HWND, byval cxPanel as long ) as long

    if gLocalBuilt = false then return 0

    dim as long padLeft  = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight = AfxScaleX(OPTROW_PADRIGHT)
    dim as long contentR = cxPanel - padRight
    dim as long fullW    = contentR - padLeft
    if fullW < 0 then fullW = 0

    ' Base-font line height, for the heading.
    dim as long textH = 0
    dim as HDC hDC = GetDC( hPage )
    if hDC then
        dim as HFONT hOld = SelectObject( hDC, OptionsFont_Base() )
        dim as TEXTMETRICW tm : GetTextMetricsW( hDC, @tm )
        SelectObject( hDC, hOld )
        ReleaseDC( hPage, hDC )
        textH = tm.tmHeight
    end if

    dim as long sectionGap = AfxScaleY(LOCAL_SECTIONGAP)
    dim as long padBottom  = AfxScaleY(OPTROW_PADBOTTOM)
    dim as long y = AfxScaleY(OPTROW_PADTOP)

    erase gLocalLabels

    ' ---- heading: the language the editor is actually set to -------------------------
    frmOptionsLocal_AddLabel( L(124,"Localization") & ": " & frmOptionsLocal_PrettyName( gLocalCurrentFile ), _
                              padLeft, y, contentR, y + textH )
    y += textH + sectionGap

    dim as long btnW = AfxScaleX(LOCAL_BTNWIDTH)
    dim as long btnH = AfxScaleY(LOCAL_BTNHEIGHT)
    dim as long btnGap = AfxScaleY(LOCAL_BTNGAP)
    dim as RECT rcPage : GetClientRect( hPage, @rcPage )
    dim as long pageH = rcPage.bottom - rcPage.top

    if gLocalShowEdit = false then
        ' ---- PICKING VIEW: languages list with the command stack to its right ------------
        ' The list takes the whole page below the heading, exactly as the phrase list does in
        ' the other view. There is nothing underneath it any more -- the phrase list moved out
        ' of this view entirely -- so a fixed row count would leave the bottom half of the page
        ' empty and cap the languages on show for no reason.
        dim as long langW = fullW - btnW - AfxScaleX(LOCAL_BTNGAP)
        if langW < 0 then langW = 0
        dim as long langH = pageH - y - padBottom
        dim as long minLangH = AfxScaleY(LOCAL_HEADERHEIGHT) + AfxScaleY(LOCAL_ROWHEIGHT)
        if langH < minLangH then langH = minLangH

        SetWindowPos( ghLocalLanguages, 0, padLeft, y, langW, langH, SWP_NOZORDER or SWP_NOACTIVATE )

        ' The stack stays at the TOP of that band rather than spreading down it: these are
        ' commands acting on the list, not a column of its own.
        dim as long bx = contentR - btnW
        dim as long by = y
        SetWindowPos( ghLocalSelect, 0, bx, by, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : by += btnH + btnGap
        SetWindowPos( ghLocalNew,    0, bx, by, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : by += btnH + btnGap
        SetWindowPos( ghLocalEdit,   0, bx, by, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : by += btnH + btnGap
        SetWindowPos( ghLocalDelete, 0, bx, by, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE ) : by += btnH

        ' The list is full height, so it is always the taller of the two now -- but take the max
        ' anyway rather than assume it, because the stack grows if a command is ever added and
        ' the assumption would fail silently.
        dim as long bandH = langH
        if (by - y) > bandH then bandH = by - y
        y += bandH
    else
        ' ---- EDITING VIEW: Back, then the phrase list filling everything below it --------
        ' The languages list and its four commands are HIDDEN (ApplyEditVisibility), so this
        ' view starts at the same y they would have and the phrase list gets the whole page.
        SetWindowPos( ghLocalClose, 0, padLeft, y, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
        y += btnH + sectionGap

        ' Whatever is left of the container, never a constant -- MINUS the hint that sits under
        ' the list, which has to be reserved here or the list would take the space and push the
        ' hint off the page.
        '
        ' hintGapPx is ROUNDED ONCE, into a long, and used from this variable in both the
        ' reservation and the advance after the list. AfxScaleY returns a FRACTIONAL value
        ' (6 -> 10.5 at 175%), so calling it twice on the same constant rounds twice -- 10.5
        ' went down inside the hintH sum and up inside the y advance, and the page overflowed
        ' its container by exactly one pixel.
        dim as long hintGapPx = AfxScaleY(LOCAL_HINTGAP)
        dim as long hintH = hintGapPx + textH
        dim as long listH = pageH - y - hintH - padBottom
        dim as long minH = AfxScaleY(LOCAL_HEADERHEIGHT) + AfxScaleY(LOCAL_ROWHEIGHT)
        if listH < minH then listH = minH

        SetWindowPos( ghLocalList, 0, padLeft, y, fullW, listH, SWP_NOZORDER or SWP_NOACTIVATE )
        y += listH + hintGapPx

        ' The hint is the same sentence whatever language is open -- the third column's own
        ' header already names it ("French Phrase"), so repeating the name here said it twice.
        frmOptionsLocal_AddLabel( L(322,"Double click the row to edit the new phrase"), _
                                  padLeft, y, contentR, y + textH )
        y += textH
    end if

    function = y + padBottom
end function


' ========================================================================================
' Paint the drawn labels. Background is filled by the caller first.
' ========================================================================================
sub frmOptionsLocal_Paint( byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    if gLocalBuilt = false then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( theme.panel.text.backcolor )
    p->b->SetForeColor( theme.panel.text.forecolor )
    for i as long = lbound(gLocalLabels) to ubound(gLocalLabels)
        dim as RECT rc = gLocalLabels(i).rc
        p->b->PaintText( gLocalLabels(i).caption, @rc, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub
