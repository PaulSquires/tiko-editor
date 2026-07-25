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

'  modThemeApply.bi  --  declarations for the theme APPLY layer.
'
'  WHY THIS IS A SEPARATE FILE FROM modThemes.inc
'    modThemes.inc is included at tiko.bas:83, before every Ps* control and every frm*
'    module, because clsConfig / modRoutines / clsDocument all read the gh* globals. It
'    therefore cannot call frmExplorer_ApplyTheme, PsListBox_Refresh or anything else -- none
'    of them exist yet. So the theme subsystem is split by include POSITION, not by taste:
'
'      modThemes.inc      (83)   pure data: parse, bind, fill, mutate, snapshot
'      modThemeApply.bi   (84)   these declarations, so anything later can call the engine
'      modThemeApply.inc  (173)  the apply layer, after every frmXxx_ApplyTheme is defined
'
'    The declarations have to arrive early because frmOptions.inc (150) drives the whole
'    thing and is included well before the implementation.

' Repaint one window AND its children. AfxRedrawWindow is InvalidateRect + UpdateWindow with
' no RDW_ALLCHILDREN, so it repaints only the form's own background -- the Ps* controls sitting
' on it keep their old pixels. Local to tiko because AfxNova is vendored and must not be
' patched as a side effect.
declare sub Theme_RedrawDeep( byval hwnd as HWND )

' Invalidate every registered window belonging to the given subsystems.
declare sub Theme_InvalidateSubsystems( byval nMask as ulong )

' Refill the globals, re-push colours into the controls of the named subsystems, then
' invalidate them. Push before invalidate, always -- the controls COPY colours at Set time,
' so an invalidate alone would just redraw them in the colours they already had.
declare sub Theme_ApplySubsystems( byval nMask as ulong )
declare sub Theme_ApplyAll()

' Swap the whole theme in memory and schedule a full apply. Does NOT touch gConfig -- this is
' the preview path, and Cancel has to be able to put it all back. Returns TRUE on error.
declare function Theme_PreviewFile( byval wszShortFilename as DWSTRING ) as boolean

' Coalescing request. Accumulates the mask and (re)arms the timer; the work happens on the
' next tick. Dragging inside a colour picker fires hundreds of these a second.
declare sub Theme_RequestApply( byval nMask as ulong )
declare sub Theme_OnCoalesceTimer()

' Timer lives on HWND_FRMMAIN: it always exists, it already owns IDT_PARSER_DEBOUNCE, and
' frmOptions' private pump is an all-window GetMessage so a frmMain timer is still dispatched
' while the dialog is up. A timer owned by the dialog would die mid-preview.
const IDT_THEME_COALESCE = 502
const THEME_COALESCE_MS  = 30
