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

'  modFileWatch.bi  --  noticing that an open document changed on disk underneath us, and
'                       either reloading it silently or asking.
'
'  WHAT THIS REPLACES
'    frmMain_OnActivateApp used to carry both halves of this inline: a moved/deleted sweep and
'    a changed-on-disk sweep, each ending in a raw Win32 MessageBox. Detection only ever
'    happened on WM_ACTIVATEAPP -- i.e. when you alt-tabbed back -- so a file rewritten while
'    tiko had focus went unnoticed until you left the app and came back. The sweeps live here
'    now, the boxes are PsMessageBox, and a timer drives the changed-file half continuously.
'
'  WHY A POLL AND NOT ReadDirectoryChangesW
'    A directory notification would remove the WAIT, not the WORK: it reports that SOMETHING
'    in a directory changed, so each open document's timestamp still has to be compared to
'    decide what. That leaves latency as the only win, and latency is the one thing this
'    feature does not want. A compiler writing a file emits several write events; acting on
'    the first reads a half-written buffer, and a prompt would appear mid-build. Against that,
'    notifications cost a worker thread, one handle per distinct directory of every open
'    document, the 64-handle WaitForMultipleObjects ceiling, and known unreliability over UNC
'    paths. Notepad++ only polls; Emacs polls whenever notifications are unavailable.
'
'    THE DECISION IS ISOLATED BEHIND FileWatch_CheckNow(). A future notification thread would
'    PostMessage a debounce and call that one function; nothing else here would change.
'
'  THE SETTLE RULE (the part that is easy to delete by accident)
'    On the timer path a difference is not acted on the first time it is seen. The observed
'    disk time is parked in pDoc->WatchPendingTime and acted on only once a later tick reads
'    the SAME time back -- so a file must stop changing before it is reloaded. That is what
'    keeps a silent reload from reading a partially written file, and it is the mitigation
'    that made polling the right call in the first place. The activation path deliberately
'    does NOT settle: the user has been away, the write is long finished, and waiting another
'    two seconds there would only look broken.
'
'  WHAT IS DELIBERATELY *NOT* ON THE TIMER
'    The moved/deleted sweep. It closes the document or removes it from the project, and build
'    tools routinely write a file by deleting and renaming -- so a poll that ran it would
'    close the user's document during an ordinary compile. It stays on activation only, where
'    a missing file has had time to come back. Same behaviour as before, new message box.
'
'  SILENT IS NEVER ALLOWED TO DISCARD EDITS
'    gConfig.DetectExternalFileChanges only ever suppresses the prompt for a buffer with no
'    unsaved changes. A modified buffer always asks, whatever the setting says -- the rule
'    Emacs, Sublime and Notepad++ all draw the same line at.

#pragma once


' The poll interval. Two seconds is invisible for a silent reload and, combined with the
' settle rule above, gives a write up to ~4s to finish before anything is read.
#define FILEWATCH_POLL_MS  2000

' Timer id on HWND_FRMMAIN, which already owns IDT_PARSER_DEBOUNCE (501, modDeclares.bi) and
' IDT_THEME_COALESCE (502, modThemeApply.bi). Keep these unique across that set.
const IDT_FILEWATCH = 503


' What a sweep decides to do about one document. FileWatch_Decide is a PURE function of five
' booleans and returns one of these -- which is the only reason the policy is assertable
' without a message pump, a disk, or a modal box. Nothing about it may start reading gConfig
' or a clsDocument directly.
enum FW_ACTION
    FW_ACTION_NONE = 0      ' in step, or the file is gone -- leave it alone
    FW_ACTION_PARK          ' changed, but not settled yet: remember the time and wait
    FW_ACTION_RELOAD        ' re-read it without asking
    FW_ACTION_ASK           ' put the question to the user
end enum

declare function FileWatch_Decide( byval bTimeDiffers   as boolean, _
                                   byval bRequireSettled as boolean, _
                                   byval bMatchesPending as boolean, _
                                   byval bOptionOn      as boolean, _
                                   byval bDirty         as boolean ) as long
declare sub      FileWatch_RunSelfTest()
declare sub      FileWatch_ApplyConfig()
declare sub      FileWatch_Stop()
declare function FileWatch_CheckNow() as long
declare function FileWatch_ScanChanged( byval bRequireSettled as boolean ) as long
declare function FileWatch_ScanMissing() as long
declare function FileWatch_IsDocModified( byval pDoc as clsDocument ptr ) as boolean
declare function FileWatch_ReloadInPlace( byval pDoc as clsDocument ptr, _
                                          byval bForceFocus as boolean = false ) as long
