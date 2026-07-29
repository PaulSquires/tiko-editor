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

' ========================================================================================
' Every standalone (non-menubar) popup menu in tiko. Declarations only -- the
' implementation is modContextMenus.inc, included late in tiko.bas because the select
' callbacks call into the OnCommand_* handlers and the panel loaders.
'
' These replace the TrackPopupMenu/CreatePopupMenu/AppendMenu builders that used to live
' in modMenus.inc. The behavioural difference that drove the shape of this API: a
' PsPopupMenu is NOT modal. Show returns immediately, so anything that used to run after
' TrackPopupMenu returned now lives in that popup's select callback.
' ========================================================================================

' Create every context popup. Call once, after frmMenuBar_Show (which owns the theme
' registry these popups are enrolled in).
declare sub ContextMenus_Init()

' Message-pump hook. Standalone chains are NOT covered by PsMenuBar_FilterMessage (that
' one stands down unless the BAR has a dropdown open), so without this call a context
' menu has no keyboard navigation and never dismisses on an outside click.
declare function ContextMenu_FilterMessage( byval pMsg as MSG ptr ) as boolean

' Close any open context chain. SILENT (no callbacks), matching the control's own
' programmatic-setter rule. killAllPopupMenus() funnels through here.
declare sub ContextMenu_CloseAll()

' Give a PsTextBox's own built-in right-click menu the topmenu's colors and enrol it in
' the theme registry. Call once per PsTextBox_Create -- the control creates its popup
' itself, but styling it is the host's job.
declare sub ThemeTextBoxContextMenu( byval hTextBox as HWND )

' ----------------------------------------------------------------------------------------
' The individual menus. Each rebuilds its items from current state and shows itself; the
' resulting command is handled in this module's own callbacks.
' ----------------------------------------------------------------------------------------
declare sub ShowBuildConfigContextMenu()
declare sub ShowTabsInfoFileTypeContextMenu()
declare sub ShowFileEncodingContextMenu()
declare sub ShowLineEndingsContextMenu()
declare sub ShowSpacesContextMenu()
declare sub ShowScintillaContextMenu()
declare sub ShowExplorerContextMenu( byval pDoc as clsDocument ptr )
declare sub ShowPanelActionContextMenu( byval rcButton as RECT )
declare sub ShowTopTabCtlContextMenu( byval idx as long )
declare sub ShowTopTabsListContextMenu( byval rcButton as RECT )

' The Output panel's Compiler Results list. Copy (the selected rows, joined with CRLF) and
' Select All. Both act on HWND_FRMOUTPUT_LVRESULTS directly rather than posting a command:
' IDM_COPY / IDM_SELECTALL are the EDITOR's commands and would act on Scintilla.
declare sub ShowOutputResultsContextMenu()
