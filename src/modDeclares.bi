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

' The MENU IDS, TOPMENU_TYPE and gTopMenu MOVED to app\modMenuIds.bi -- they are
' application vocabulary, not Win32, and modMenuDefinitions.inc needs them.
'
' WATCH THE ENUM BELOW. It ends at 1038 and app\modMenuIds.bi starts at 1039,
' because the two used to be ONE enum and the menu ids are persisted in
' keybindings.ini as numbers. Adding a MSG_USER_* message here COLLIDES with
' IDM_FILE_START. fbc's preprocessor cannot evaluate an enum constant, so there
' is no compile-time guard -- only this note.


''  Menu message identifiers
enum
    '' USER MESSAGES
    MSG_USER_SETFOCUS = WM_USER + 1     ' 1024 + 1
    MSG_USER_PROCESS_COMMANDLINE 
    MSG_USER_PROCESS_STARTUPUSERTOOLS
    MSG_USER_PROCESS_CHECKFORUPDATE
    MSG_USER_SHOWAUTOCOMPLETE      ' wParam = AUTOCOMPLETE_NOTIFY_*
    MSG_USER_LOAD_EXPLORERFILES
    MSG_USER_LOAD_FUNCTIONLISTFILES
    MSG_USER_LOAD_BOOKMARKSFILES
    MSG_USER_LOAD_FUNCTIONSFILES
    MSG_USER_SHOW_KEYBOARDEDIT
    ' Companion to MSG_USER_SHOW_KEYBOARDEDIT, for the User Tools dialog's Assign Shortcut
    ' editor. Both exist so the sub-dialog can be opened from a posted message rather than
    ' only from a button click -- see TIKO_USERTOOLS_AUTOASSIGN.
    MSG_USER_SHOW_TOOLKEY
    MSG_USER_RICHEDIT_SELECTALL
    MSG_USER_PARSE_COMPLETE        ' wParam = SCAN_TIER; posted by the fbcParser scan worker
    ' Posted by the update-check worker as its last act. The check used to be joined
    ' immediately by its caller, which blocked startup for as long as WinHTTP took; the
    ' thread is now genuinely asynchronous and reports back through this.
    MSG_USER_UPDATECHECK_COMPLETE
    
end enum

' The AfxIFileSaveDialog selector for cloning a theme -- picks the *.theme filter and the
' settings\themes\ folder in modRoutines. Defined HERE (loaded before modRoutines.inc) rather
' than in frmThemes.bi (loaded after it), which the save dialog cannot see.
#define IDC_THEMES_SAVEAS    9650


'  Global window handles
dim shared as HWND HWND_FRMMAIN, HWND_FRMRECENT, HWND_FRMMAIN_STATUSBAR
dim shared as HWND HWND_FRMOUTPUT, HWND_FRMOUTPUT_LVRESULTS
dim shared as HWND HWND_FRMOUTPUT_LVTODO, HWND_FRMOUTPUT_VSCROLL
' The tab strip: a PsSelectBar (the four tabs) and a one-item PsIconPanel (the "X"), sized
' side by side. HWND_FRMOUTPUT_SELECTBAR also HOLDS the current tab -- see frmOutput.bi,
' and note that a tab's identity there is its panel ID, not its panel index.
' Two PsIconPanels rather than one, because PsIconPanel is STATIC BY CONTRACT and has no
' hide/delete API: the undock icon lives in its own control so the "X" can be hidden with a
' plain ShowWindow while the panel is floating (where "close" has no meaning in a window
' that already has a caption X). They abut and paint the same background, so the seam is
' invisible -- frmPanelMenu's left/right pattern.
dim shared as HWND HWND_FRMOUTPUT_SELECTBAR, HWND_FRMOUTPUT_UNDOCK, HWND_FRMOUTPUT_CLOSE
' The Output panel's floating frame. Owned by HWND_FRMMAIN, WS_EX_TOOLWINDOW, and while it
' exists HWND_FRMOUTPUT is a CHILD OF IT rather than of frmMain. The panel window itself is
' never destroyed and recreated, so every pane's content survives an undock/dock cycle.
' IsWindow on this handle is the single source of truth for the dock state.
dim shared as HWND HWND_FRMOUTPUTFLOAT
dim shared as HWND HWND_FRMMAIN_MENUBAR
' Editor split bars: one per orientation, created once and shown one at a time to follow
' whichever split mode the ACTIVE document is in (PsSplitter's orientation is fixed at
' creation, and only one document is active).
dim shared as HWND HWND_FRMMAIN_SPLITV, HWND_FRMMAIN_SPLITH
' Output panel splitter. Also a child of frmMain, not of frmOutput: it sits BETWEEN the
' editor area and the panel, and frmMain owns that layout.
dim shared as HWND HWND_FRMMAIN_SPLITOUTPUT
' Explorer panel splitter. Sits on the panel's inner edge -- which side that is depends on
' gConfig.ExplorerPositionRight, but it is a vertical bar either way, so one control serves
' both dock sides.
dim shared as HWND HWND_FRMMAIN_SPLITPANEL
' The Help Center: one modeless, non-owned top-level window hosting a WebView2 pane. Its
' only child is the pane's host window, which frmHelpCenter owns -- so unlike the
' frmHelpViewer it replaced, there is nothing else to track here.
dim shared as HWND HWND_FRMHELPCENTER
' The General Options and Code Editor pages are table-driven now (modOptionsRows.inc) and
' have no child forms of their own, so HWND_FRMOPTIONSGENERAL / ...EDITOR / ...EDITOR2 are
' gone. "Advanced Code Editor" is no longer a separate page at all: its settings sit at the
' bottom of the one scrolling Code Editor page.
dim shared as HWND HWND_FRMOPTIONS
' The entire Options dialog is panel-hosted / table-driven now: every page's
' controls live on the scroll panel, so none of the old HWND_FRMOPTIONS* child-form globals
' remain.
dim shared as HWND HWND_FRMFINDINPROJECT, HWND_FRMSEARCHSYMBOL

' The Find in Project tab has no clsDocument, but its excerpt views ARE editable views onto
' real ones -- so Edit commands have to be able to find them. Answers the focused excerpt's
' window and its document, or 0/null when the caret is not in one.
'
' Declared here rather than in frmFindInProject.bi because modMenus.inc and frmMainEdit.inc
' both need it and are compiled long before that file.
declare function frmFindInProject_ActiveEdit( byref pDocOut as clsDocument ptr ) as HWND
dim shared as HWND HWND_FRMBUILDCONFIG, HWND_FRMUSERTOOLS, HWND_FRMABOUT
dim shared as HWND HWND_FRMABOUT_TABS, HWND_FRMABOUT_CREDITS, HWND_FRMABOUT_LICENSE
dim shared as HWND HWND_FRMABOUT_LICSEL, HWND_FRMABOUT_LICVSCROLL
dim shared as HWND HWND_FRMKEYBOARD, HWND_FRMKEYBOARDEDIT
dim shared as HWND HWND_FRMDEBUG, HWND_FRMDEBUG_LVGLOBALS, HWND_FRMDEBUG_LVLOCALS
dim shared as HWND HWND_FRMDEBUG_LVSTACK, HWND_FRMDEBUG_LVWATCH
dim shared as HWND HWND_FRMDEBUG_SPLITMAIN, HWND_FRMDEBUG_SPLITLEFT, HWND_FRMDEBUG_SPLITRIGHT
dim shared as HWND HWND_FRMUNUSED, HWND_FRMUNUSED_LIST, HWND_FRMUNUSED_FILTER

dim shared as HWND HWND_FRMMAIN_TOPTABS, HWND_FRMMAIN_TOPTABSMENU
dim shared as HWND HWND_FRMMAIN_TOPTABSINFO, HWND_FRMMAIN_FIND, HWND_FRMMAIN_REPLACE
dim shared as HWND HWND_FRMEXPLORER
dim shared as HWND HWND_FRMFUNCTIONS
dim shared as HWND HWND_FRMBOOKMARKS
dim shared as HWND HWND_FRMPANEL, HWND_FRMPANEL_MENU, HWND_FRMPANEL_VSCROLLBAR
' The panel menu is TWO PsIconPanels rather than one bar with a spring: PsIconPanel
' declares its spacing and justifies the whole run as a block, so the left-hand and
' right-hand icon groups are separate controls sharing the strip.
dim shared as HWND HWND_FRMPANEL_MENU_RIGHT
dim shared as HWND HWND_FRMAUTOCOMPLETE
dim shared as HWND HWND_FRMEDITOR_HSCROLLBAR(1)
dim shared as HWND HWND_FRMEDITOR_VSCROLLBAR(1)

dim shared as long ghIconGood, ghIconBad
dim shared as HCURSOR ghCursorSizeNS
dim shared as HCURSOR ghCursorSizeWE

' The localization tables and the L() macro MOVED to app\modLocalization.bi.
' Localization is an app-layer concern -- every app file with user-facing text
' needs L(), and this header is full of HWNDs. The arrays were also declared
' `wstring * MAX_PATH`, so the table holding every translated string in the
' program could not be declared without windows.bi.

#Define SetFocusScintilla  PostMessage( HWND_FRMMAIN, MSG_USER_SETFOCUS, 0, 0 )
#Define SciExec(h, m, w, l) SendMessage(h, m, w, CAST(LPARAM, l))

#DEFINE GUIFONT      wstr("Segoe UI")
#DEFINE GUIFIXEDFONT wstr("Consolas")
#DEFINE SYMBOLFONT   wstr("Segoe Fluent Icons")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONTBOLD_10   2
#DEFINE ITALICFONT_10    3
#DEFINE INFOFONT_11      4
#DEFINE SYMBOLFONT_9     5
#DEFINE SYMBOLFONT_10    6
#DEFINE SYMBOLFONT_12    7
#DEFINE SYMBOLFONT_20    8
' Larger GUI faces, added for the Options dialog (whose base font is 11pt).
' MAXFONTS is both the array bound and the bound of the DeleteObject loop in frmMain, so a
' new entry here is freed without touching the cleanup.
#DEFINE GUIFONT_11       9
#DEFINE GUIFONT_12       10
#DEFINE GUIFONTBOLD_11   11
' The About box's application name, and the only face in this table above 12pt. Nothing else
' in tiko needs it; it exists so that one line can carry the dialog.
#DEFINE GUIFONT_26       12
' The "tk" wordmark on the About box's icon plate. A FIXED face, deliberately: the mark is
' two lowercase letters standing in for an app icon, and Consolas gives them the even weight
' and the flat terminals that read as a logotype. Segoe UI at the same size reads as a word.
#DEFINE FIXEDFONT_26     13
' The SYSTEM tooltip face, read from SPI_GETNONCLIENTMETRICS rather than named here -- see
' PsTooltip_GetSystemFont. Unlike every other entry in this table these are not a face plus a
' size we chose; they are whatever the user's Windows theme says a tip is drawn in, so on a
' machine with a non-default appearance setting they will not be Segoe UI 9pt.
' They live in ghFont() so the existing DeleteObject loop frees them.
#DEFINE TOOLTIPFONT      14
#DEFINE TOOLTIPFONTBOLD  15
#DEFINE MAXFONTS         16

dim shared ghFont(MAXFONTS) as HFONT


''
''  Save information related to Find/Replace and Find in Files operations
''
type FINDREPLACE_TYPE
    bFirstTimeInvoked   as boolean = true
    hCueBannerFont      as HFONT
    foundCount          as long 
    txtFind             as DWSTRING
    txtReplace          as DWSTRING
    txtFindCombo(10)    as DWSTRING
    txtReplaceCombo(10) as DWSTRING
    txtFilesCombo(10)   as DWSTRING
    txtFolderCombo(10)  as DWSTRING
    txtLastFind         as DWSTRING
    txtFiles            as DWSTRING         ' *.*, *.bas, etc (FindInFolder)
    txtFolder           as DWSTRING         ' start search from this folder (FindInFolder)
    nSearchSubFolders   as long          ' search sub folders as well (FindInFolder)
    nWholeWord          as long          ' find/replace whole word search
    nMatchCase          as long          ' match case when searching
    nSelection          as long          ' search only selected text
    nPreserve           as long          ' search only selected text
    nSearchCurrentDoc   as long
    nSearchAllOpenDocs  as long
    nSearchProject      as long
    wszResults          as DWSTRING = "0/0"
    bShowInfoPanel      as boolean = true
    bShowFindPanel      as boolean = false
    bShowReplacePanel   as boolean = false
    bProjectReplaceActive as boolean = false
    ' The Find bar's seven icon rects (Match Case, Whole Word, Toggle Replace, Selection,
    ' Prev, Next, Close) and its divider are three PsIconPanels now (frmFind.inc) -- the
    ' controls own those cells, so there is nothing left for the host to store. rcResults
    ' stays: the results count is still painted by the bar itself.
    ' ...and the Replace bar's three (Preserve Case, Replace, Replace All) are two more
    ' (frmReplace.inc). rcResults is the only icon-strip rect left in this type, because the
    ' results count is still painted by the Find bar itself.
    rcResults           as RECT
end type
dim shared gFind as FINDREPLACE_TYPE
dim shared gFindInFiles as FINDREPLACE_TYPE


' (LASTPOSITION_TYPE / gLastPosition are gone. The navigation history that replaced them
'  lives in modNavHistory.bi -- it keys on the filename rather than a clsDocument ptr, which
'  is what let the old array dangle whenever a file was closed.)


' (The menubar's own item/state globals are gone: the PsMenuBar control owns its items,
'  rects, hover and active state -- see frmMenuBar.inc.)

' array that holds the names of all fonts on the target system
dim shared gFontNames( any ) as DWSTRING


const MENUITEM_HEIGHT = 24
const EXPLORERITEM_HEIGHT = 22
const MENUBAR_HEIGHT = 30
' OUTPUT_TABS_HEIGHT MOVED DOWN to app/modAppConstants.bi, which this file now includes.
' Not because it stopped being a layout constant -- everything that puts it on screen is
' still up here -- but because clsConfig's constructor defaults ShowOutputPanelHeight to
' five of them, and that constructor has to link without the shell. See app/clsConfig.inc.
#include once "app/modAppConstants.bi"
' Floor for the Output panel's USER-SET height (gConfig.ShowOutputPanelHeight), in unscaled
' units. Deliberately ABOVE OUTPUT_TABS_HEIGHT: a stored height equal to the tabs row is
' indistinguishable from the minimized size, so restoring from it would leave the panel
' looking minimized while the flag said otherwise. Two rows is the smallest height at which
' the panel shows any content at all.
const OUTPUT_PANEL_MIN_HEIGHT = OUTPUT_TABS_HEIGHT * 2
' Width reserved at the right of the Output tab strip for the close "X" PsIconPanel. The
' glyph cell itself is smaller; the surplus is the breathing room the old hand-drawn rect
' got from its 10px right margin.
' The cell each Output strip icon gets. The GLYPH is 20 unscaled and centred in it, so this
' number IS the spacing: at 40 the two icons sat a full icon-width apart and read as two
' unrelated controls rather than a pair.
const OUTPUT_CLOSE_WIDTH = 30
const PANEL_ICON_HEIGHT = 24

' Debounce timer on HWND_FRMMAIN: restarted on every editor modification;
' when it fires, the active document is handed to the fbcParser scan worker.
const IDT_PARSER_DEBOUNCE = 501
const PARSER_DEBOUNCE_MS  = 500

const TOPTABS_HEIGHT = 36
const TOPTABS_INFO_HEIGHT = 40   
const TOPTABS_FIND_HEIGHT = 40   
const TOPTABS_REPLACE_HEIGHT = 40

const MENUITEM_HEIGHT_COMPACT = 20
const EXPLORERITEM_HEIGHT_COMPACT = 19 
const MENUBAR_HEIGHT_COMPACT = 28 

const STATUSBAR_HEIGHT = 26
const SCROLLBAR_WIDTH_PANEL = 7
const SCROLLBAR_WIDTH_EDITOR = 12
const SCROLLBAR_HEIGHT = 12
const SCROLLBAR_MINTHUMBSIZE = 30
const SPLITSIZE = 4



dim shared as wstring * 10 _
    wszIconChevronLeft, wszIconChevronRight, wszIconChevronUp, wszIconChevronDown, _
    wszIconDocument, wszIconUpArrow, wszIconDownArrow, wszIconSelection, wszIconCheckmark, _
    wszIconClose, wszIconDirty, wszIconCompileResult, wszIconMatchCase, wszIconWholeWord, _
    wszIconPreserveCase, wszIconReplace, wszIconReplaceAll, wszIconMoreActions, wszIconAddFileButton, _
    wszIconExplorer, wszIconFunctions, wszIconBookmarks, _
    wszIconCompile, wszIconBuildExecute, wszIconDebug, _
    wszIconSplitEditor, wszIconThemes, _
    wszIconSettings, wszIconCheckBoxEmpty, wszIconCheckBoxMarked, _
    wszIconContinue, wszIconStop, wszIconStepNext, wszIconStepOver, wszIconStepOut, wszIconRunToCursor, _
    wszIconPause, wszIconSortAsc, wszIconSortDesc, wszIconTrash, wszIconNewFolder, wszIconRename, _
    wszIconSave, wszIconFind, wszIconToggleReplace, _
    wszIconGotoMain, wszIconGotoHeader, wszIconGotoSource, _
    wszIconUndock, wszIconDock, _
    wszSearchFile, wszSearchFunction, wszSearchType, wszSearchEnum, _
    wszAutoCGlyphProc, wszAutoCGlyphType, wszAutoCGlyphEnum, wszAutoCGlyphVar, wszAutoCGlyphKeyword


' Braille spinner patterns - large clockwise rotation
' DWSTRING, not "wstring * 2": the -gen gas64 backend CORRUPTS static wstring array
' initializers (measured 2026-07-27 -- it wrote 0:0 for six of these eight frames, and
' widening the field to "wstring * 3" silenced the warning while leaving the data wrong).
' DWSTRING is a UDT with constructors, so the array is built at runtime instead of being
' emitted as static data, which sidesteps the backend bug entirely. gcc and gas64 now
' produce identical code points. Consumers take the element itself, not its address.
dim shared spinner(0 to 7) as DWSTRING => { _
    wstr(!"\u28F4"), _ ' ⠾
    wstr(!"\u28F2"), _ ' ⠽
    wstr(!"\u28B6"), _ ' ⠻
    wstr(!"\u2837"), _ ' ⠷
    wstr(!"\u282F"), _ ' ⠯
    wstr(!"\u281F"), _ ' ⠟
    wstr(!"\u28D4"), _ ' ⠮
    wstr(!"\u28F0")  _ ' ⠼
}
' Symbol characters display in top menus, frmExplorer, and tab control

'"Segoe Fluent Icons"
    wszIconClose             = !"\uE10A"      ' light X
    ' The Output panel's undock/dock icon. PLACEHOLDERS: chosen from the Fluent set without
    ' having seen them rendered at SYMBOLFONT_10 beside the X, so they are the author's to
    ' swap during the interactive pass. Escapes, never pasted characters -- a pasted glyph in
    ' a BOM-less file is read as three Latin-1 bytes with no build error.
    wszIconUndock            = !"\uE8A7"      ' open in new window
    wszIconDock              = !"\uE944"      ' back to window
    wszIconChevronLeft       = !"\uE012"
    wszIconChevronRight      = !"\uE013" 
    wszIconChevronUp         = !"\uE014"
    wszIconChevronDown       = !"\uE015"
    wszIconDocument          = !"\u00B7"     ' small dot (use the regular Segue UI font for this one)
    wszIconUpArrow           = !"\uF0AD"     ' up arrow
    wszIconDownArrow         = !"\uF0AE"     ' down arrow
    wszIconSelection         = !"\uE14C"     ' selection icon
    wszIconCheckmark         = !"\uE001"     ' checkmark
    wszIconDirty             = !"\u2981"     ' larger dot
    wszIconCompileResult     = !"\uECCC"     ' larger circle  
    wszIconMatchCase         = "Aa"          ' match case
    wszIconWholeWord         = "W"           ' whole word
    wszIconPreserveCase      = "AB"          ' preserve case
    wszIconReplace           = !"\uE14B"     ' replace
    wszIconReplaceAll        = !"\uE299"     ' replace all
    wszIconMoreActions       = !"\uE10C"     ' ...
    wszIconAddFileButton     = !"\uE109"     ' plus sign (thicker)
    wszIconExplorer          = !"\uE8A9"
    wszIconFunctions         = !"\uE8BC"
    wszIconBookmarks         = !"\uE723"
    wszIconCompile           = !"\uE74C"
    wszIconBuildExecute      = !"\uE768"  
    ' One fixed glyph: the button opens a menu offering both axes, so it no longer
    ' swaps between a left/right and a top/bottom icon depending on the Alt key.
    wszIconSplitEditor       = !"\uF57C"
    wszIconSettings          = !"\uE713"   
    wszIconThemes            = !"\uE771"   
    wszIconCheckBoxEmpty     = !"\uE739"
    wszIconCheckBoxMarked    = !"\uE73A"
    wszIconSave              = !"\uE74E"      ' Save (diskette)
    wszIconFind              = !"\uE721"      ' Search (magnifying glass)
    wszIconToggleReplace     = !"\uE8B4"      ' Toggle Replace
    wszIconGotoMain          = "M"
    wszIconGotoHeader        = "H"
    wszIconGotoSource        = "C"

    ' Use GUI Italiac font for these identifiers.
    wszSearchFile            = "d"            ' "disk"
    wszSearchFunction        = "f"
    wszSearchType            = "T"
    wszSearchEnum            = "e"

    ' Debugger Icons
    wszIconDebug             = !"\uE893"     ' next outline
    wszIconContinue          = !"\uF5B0"     ' play solid
    wszIconStop              = !"\uEE95"     ' stop solid
    wszIconStepNext          = !"\uF0AF"     ' right arrow
    wszIconStepOver          = !"\uEE35"     ' reply mirrored
    wszIconStepOut           = wszIconUpArrow
    wszIconRunToCursor       = !"\uE623"     ' next solid

    ' Column sort indicators, drawn by the host because PsColumnHeader never sorts and
    ' therefore never draws one. Escapes, NOT pasted characters -- a pasted U+E70E in a
    ' BOM-less file is read as three Latin-1 bytes and renders as mojibake (PsToolbar).
    wszIconSortAsc           = !"\uE70E"     ' chevron up
    wszIconSortDesc          = !"\uE70D"     ' chevron down
    wszIconTrash             = !"\uE74D"     ' trashcan - delete the selected watch
    wszIconRename            = !"\uE8AC"     ' rename - Explorer rename-folder icon
    wszIconNewFolder         = !"\uE8F4"     ' new folder - Explorer add-folder icon. Its own
                                             ' define rather than reusing wszIconAddFileButton,
                                             ' which is the generic plus and is also the
                                             ' debugger's Add Watch and a statusbar glyph.

    ' Autocomplete popup kind markers. Plain geometric characters drawn in the regular
    ' GUI font, NOT Segoe Fluent Icons: the popup already switches colour per kind, and
    ' a missing icon-font glyph would render as a tofu box on every row of the list.
    wszAutoCGlyphProc        = !"\u0192"     ' small f with hook - sub/function
    wszAutoCGlyphType        = !"\u25A0"     ' filled square - type/union/typedef
    wszAutoCGlyphEnum        = !"\u25B2"     ' filled triangle - enum/enum value
    wszAutoCGlyphVar         = !"\u25CF"     ' filled circle - var/const/param/field
    wszAutoCGlyphKeyword     = !"\u25C6"     ' filled diamond - keyword/data type
    
    
    
    
