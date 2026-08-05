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

'  modFileWatch.inc  --  see modFileWatch.bi for why this is a poll, why the moved/deleted
'                        sweep is not on it, and what the settle rule is protecting.

#pragma once


' Re-entry guard, shared by the timer and the activation sweep. It has to cover BOTH, not just
' the timer: PsMessageBox runs a nested GetMessage loop, and a frmMain timer still arrives
' while one is up (the same fact IDT_THEME_COALESCE relies on to survive the Options dialog's
' pump) -- so without this, one prompt would stack another every two seconds.
dim shared as boolean gFileWatchBusy


' ========================================================================================
' Equality for two file times. Kept as one function so the "has it changed" test and the
' "has it settled" test cannot drift apart.
'
' NOW EXACT, AND THEREFORE STRICTER. This used to convert BOTH SIDES through
' AfxFileTimeToVariantTime and compare the doubles -- carried over from the original inline
' check rather than chosen, and lossy: a VARIANT time counts DAYS as a double, so two file
' times under about a millisecond apart compared EQUAL.
'
' A PsFileTime is 100-nanosecond ticks as a plain longint, so this is now a direct integer
' comparison. That is the right direction: an mtime comes from the filesystem and does not
' drift, so two reads of an unmodified file give identical ticks, and the only thing the
' coarsening could ever have hidden is a real sub-millisecond change.
'
' ON LINUX, PsFileWriteTime has ONE-SECOND resolution -- fbc's FileDateTime is all there is
' short of hand-declaring struct stat. Two writes inside the same second will compare equal
' here. Not introduced by this change and not fixable at this layer; recorded so it is not
' rediscovered as a mysterious missed reload.
' ========================================================================================
private function FileWatch_SameTime( byval ft1 as PsFileTime, byval ft2 as PsFileTime ) as boolean
    return (ft1 = ft2)
end function


' ========================================================================================
' THE WHOLE POLICY, as a pure function. Every rule this feature has lives here:
'
'   * nothing to do unless the disk time differs from the one we recorded
'   * on the timer path, a difference must be seen TWICE with the same time before it is
'     acted on -- so a file being written is parked, not read
'   * silent reload requires the option ON *and* a clean buffer; a modified buffer always asks
'
' Kept free of gConfig and clsDocument on purpose: it is the one part of this module that can
' be exhaustively asserted, and it is where getting it wrong loses a user's edits.
' ========================================================================================
function FileWatch_Decide( byval bTimeDiffers   as boolean, _
                           byval bRequireSettled as boolean, _
                           byval bMatchesPending as boolean, _
                           byval bOptionOn      as boolean, _
                           byval bDirty         as boolean ) as long

    if bTimeDiffers = false then return FW_ACTION_NONE

    ' Timer path: the first sighting only parks the time.
    if bRequireSettled then
        if bMatchesPending = false then return FW_ACTION_PARK
    end if

    ' SILENT IS NEVER ALLOWED TO DISCARD EDITS. Both conditions, not either.
    if bOptionOn then
        if bDirty = false then return FW_ACTION_RELOAD
    end if

    return FW_ACTION_ASK
end function


' ========================================================================================
' Arm or disarm the poll to match gConfig.DetectExternalFileChanges. Idempotent -- SetTimer
' on an existing id just resets it -- so it is safe to call from startup and from the Options
' dialog's OK without tracking whether the timer is already running.
'
' WITH THE OPTION OFF THE TIMER DOES NOT EXIST, and behaviour is exactly what it was before
' this module: detection on WM_ACTIVATEAPP only, always prompting.
' ========================================================================================
sub FileWatch_ApplyConfig()
    if HWND_FRMMAIN = 0 then exit sub

    if gConfig.DetectExternalFileChanges then
        SetTimer( HWND_FRMMAIN, IDT_FILEWATCH, FILEWATCH_POLL_MS, 0 )
    else
        KillTimer( HWND_FRMMAIN, IDT_FILEWATCH )
    end if
end sub


' ========================================================================================
' Unconditional teardown, for frmMain's WM_DESTROY.
' ========================================================================================
sub FileWatch_Stop()
    if HWND_FRMMAIN then KillTimer( HWND_FRMMAIN, IDT_FILEWATCH )
end sub


' ========================================================================================
' THE TIMER TICK, and the single entry point a future notification thread would call.
'
' Gated on frmMain being the foreground window, which does three jobs at once: it stops us
' prompting over another application the user is working in, it stops us running while any of
' our own modal dialogs is up (Options, Themes, a PsMessageBox), and it means a change made
' while tiko was in the background is handled by the activation sweep instead -- which is the
' path that has always handled it.
' ========================================================================================
function FileWatch_CheckNow() as long
    if gConfig.DetectExternalFileChanges = false then return 0
    if GetForegroundWindow() <> HWND_FRMMAIN then return 0

    FileWatch_ScanChanged( true )      ' true = require the timestamp to have settled
    return 0
end function


' ========================================================================================
' Does this document have unsaved edits? Both Scintilla windows are asked, because a split
' edit shares one buffer but either half can be the one that was typed into.
' ========================================================================================
function FileWatch_IsDocModified( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then return false

    dim as boolean bMod0 = cbool( SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0 )
    dim as boolean bMod1 = cbool( SciExec( pDoc->hWindow(1), SCI_GETMODIFY, 0, 0 ) <> 0 )

    ' Written as two locals rather than one orelse chain: fbc warning 38 fires on mixing
    ' boolean and non-boolean operands, and SciExec returns an integer.
    if bMod0 then return true
    if bMod1 then return true
    return false
end function


' ========================================================================================
' A themed [Yes] [No] [Cancel] box asking whether to reload. Returns IDYES / IDNO / IDCANCEL.
'
' The buttons are localized rather than taken from PsMessageBox_AddPreset( MBX_BTN_YESNOCANCEL ),
' whose text is hardcoded English (PsMessageBox.inc). That is a real consequence of moving off
' the Win32 MessageBox, which got OS-localized buttons for free: we own the words now -- and
' TikoMsgBox is where they live.
' ========================================================================================
private function FileWatch_AskReload( byval pDoc as clsDocument ptr ) as long
    return TikoMsgBox( HWND_FRMMAIN, _
                       pDoc->DiskFilename & vbCrLf & vbCrLf & _
                       L(206, "File was changed by another application. Reload it?"), _
                       L(207, "File Changed"), _
                       TMB_ICON_QUESTION, TMB_YESNOCANCEL )
end function


' ========================================================================================
' A themed [OK] box reporting that the file is gone. Replaces the MB_ICONINFORMATION box.
' ========================================================================================
private function FileWatch_ReportMissing( byval pDoc as clsDocument ptr ) as long
    return TikoMsgBox( HWND_FRMMAIN, _
                       pDoc->DiskFilename & vbCrLf & vbCrLf & _
                       L(222, "This document has been deleted or is no longer available."), _
                       L(207, "File Changed"), _
                       TMB_ICON_INFO, TMB_OK )
end function


' ========================================================================================
' Re-read the file, putting the view back where it was.
'
' ReloadDocument DESTROYS AND RECREATES both Scintilla windows, so everything the user can see
' -- caret, scroll, bookmarks, breakpoints, folds -- is lost unless it is carried across by
' hand. That is survivable for the old explicit "Reload it? Yes" but not for a SILENT reload,
' where the file would appear to jump to the top on its own. The capture/restore below is the
' same sequence, in the same order, that the workspace restore in clsConfig.inc performs.
'
' Positions are clamped to the new length: the file is allowed to have got shorter.
'
' FOCUS HAS TO BE PUT BACK BY HAND, and the two callers need different rules.
'   bForceFocus = true  (after the prompt) the message box took focus, so the editor cannot
'                       have it and there is nothing to test -- give it back unconditionally.
'                       Without this the file reloads and the caret is nowhere.
'   bForceFocus = false (silent reload)    only restore it if this document's Scintilla HAD
'                       focus. A silent reload must not YANK focus out of the Find bar or the
'                       Explorer panel just because a file changed underneath -- those windows
'                       are not the ones being destroyed here, so their focus is still valid.
' ========================================================================================
function FileWatch_ReloadInPlace( byval pDoc as clsDocument ptr, _
                                  byval bForceFocus as boolean = false ) as long
    if pDoc = 0 then return 0

    ' Sampled BEFORE the windows are destroyed -- afterwards the handles are stale and GetFocus
    ' reports whatever the system moved to.
    dim as HWND hFocus = GetFocus()
    dim as boolean bHadFocus = false
    if hFocus = pDoc->hWindow(0) then bHadFocus = true
    if hFocus = pDoc->hWindow(1) then bHadFocus = true

    dim as long nFirst0 = SciExec( pDoc->hWindow(0), SCI_GETFIRSTVISIBLELINE, 0, 0 )
    dim as long nPos0   = SciExec( pDoc->hWindow(0), SCI_GETCURRENTPOS, 0, 0 )
    dim as long nFirst1 = SciExec( pDoc->hWindow(1), SCI_GETFIRSTVISIBLELINE, 0, 0 )
    dim as long nPos1   = SciExec( pDoc->hWindow(1), SCI_GETCURRENTPOS, 0, 0 )

    dim as string sBookmarks   = pDoc->GetBookmarks()
    dim as string sBreakPoints = pDoc->GetBreakPoints()
    dim as string sFoldPoints  = pDoc->GetFoldPoints()

    ' Which half had focus. hWndActiveScintilla is a property, so it is read before the
    ' windows are destroyed and re-applied by handle INDEX afterwards, not by handle.
    dim as long nFocusEdit = 0
    if pDoc->hWndActiveScintilla = pDoc->hWindow(1) then nFocusEdit = 1

    ReloadDocument( pDoc->DiskFilename )

    dim as long nLen = SciExec( pDoc->hWindow(0), SCI_GETLENGTH, 0, 0 )
    if nPos0 > nLen then nPos0 = nLen
    if nPos1 > nLen then nPos1 = nLen

    ' Scroll first, then the caret: SCI_GOTOPOS scrolls the caret into view, so doing it last
    ' means a caret that is still on screen leaves the restored scroll position alone.
    SciExec( pDoc->hWindow(0), SCI_SETFIRSTVISIBLELINE, nFirst0, 0 )
    SciExec( pDoc->hWindow(0), SCI_GOTOPOS, nPos0, 0 )
    SciExec( pDoc->hWindow(1), SCI_SETFIRSTVISIBLELINE, nFirst1, 0 )
    SciExec( pDoc->hWindow(1), SCI_GOTOPOS, nPos1, 0 )

    pDoc->SetBookmarks( sBookmarks )
    pDoc->SetBreakPoints( sBreakPoints )
    pDoc->SetFoldPoints( sFoldPoints )

    pDoc->hWndActiveScintilla = pDoc->hWindow(nFocusEdit)

    ' PostMessage-based (MSG_USER_SETFOCUS), so it lands after this reload -- and after the
    ' message box has fully torn down on the prompt path -- rather than fighting either.
    if bForceFocus then
        frmMain_SetFocusToCurrentCodeWindow()
    elseif bHadFocus then
        frmMain_SetFocusToCurrentCodeWindow()
    end if

    return 0
end function


' ========================================================================================
' Sweep every open document for one whose disk file has been rewritten.
'
'   bRequireSettled = true   (timer)      a difference must be seen TWICE with the same disk
'                                         time before it is acted on -- see modFileWatch.bi.
'   bRequireSettled = false  (activation) act on first sight, as this code always has.
'
' Returns IDCANCEL if the user cancelled out of a prompt, so the caller can stop.
' ========================================================================================
function FileWatch_ScanChanged( byval bRequireSettled as boolean ) as long
    if gApp.GetDocumentCount = 0 then return 0
    if gFileWatchBusy then return 0
    gFileWatchBusy = true

    dim as long nOutcome = 0
    dim pDoc as clsDocument ptr = gApp.pDocList

    do until pDoc = 0
        dim pDocNext as clsDocument ptr = pDoc->pDocNext

        if pDoc->IsNewFlag then
            pDoc = pDocNext: continue do
        end if

        ' A file that has gone missing is not a file that changed. Leave it entirely alone --
        ' the activation sweep owns that case, and a build writing via delete+rename passes
        ' through exactly this state.
        if PsFileExists( pDoc->DiskFilename ) = false then
            pDoc = pDocNext: continue do
        end if

        dim as PsFileTime ft = PsFileWriteTime( pDoc->DiskFilename )

        ' Every rule is in FileWatch_Decide. This loop only gathers the five facts it needs and
        ' carries out the verdict.
        dim as boolean bTimeDiffers   = cbool( FileWatch_SameTime( ft, pDoc->DateFileTime ) = false )
        dim as boolean bMatchesPending = FileWatch_SameTime( ft, pDoc->WatchPendingTime )
        dim as boolean bOptionOn      = cbool( gConfig.DetectExternalFileChanges <> 0 )
        dim as boolean bDirty         = FileWatch_IsDocModified( pDoc )

        dim as long nAction = FileWatch_Decide( bTimeDiffers, bRequireSettled, _
                                                bMatchesPending, bOptionOn, bDirty )

        select case nAction
            case FW_ACTION_NONE
                ' In step. Drop any parked time so a later change starts the settle count fresh.
                pDoc->WatchPendingTime = pDoc->DateFileTime

            case FW_ACTION_PARK
                pDoc->WatchPendingTime = ft

            case FW_ACTION_RELOAD
                FileWatch_ReloadInPlace( pDoc )
                pDoc->DateFileTime = PsFileWriteTime( pDoc->DiskFilename )
                pDoc->WatchPendingTime = pDoc->DateFileTime

            case FW_ACTION_ASK
                ' Bring the file into view before asking about it -- the user cannot judge the
                ' question otherwise. This is what the original code did.
                OpenSelectedDocument( pDoc->DiskFilename )

                dim as long res = FileWatch_AskReload( pDoc )

                if res = IDCANCEL then
                    nOutcome = IDCANCEL
                    exit do
                end if
                ' true: the box had focus, so the editor must be given it back explicitly.
                if res = IDYES then FileWatch_ReloadInPlace( pDoc, true )

                ' Accept the disk time either way, so a declined reload is not asked about
                ' again on every tick.
                pDoc->DateFileTime = PsFileWriteTime( pDoc->DiskFilename )
                pDoc->WatchPendingTime = pDoc->DateFileTime
        end select

        pDoc = pDocNext
    loop

    gFileWatchBusy = false
    return nOutcome
end function


' ========================================================================================
' Sweep for documents whose file has been moved or deleted, and close or unproject them.
' ACTIVATION ONLY -- never the timer. See modFileWatch.bi.
'
' Lifted from frmMain_OnActivateApp unchanged except for the message box. The outer do/loop
' the original wrapped this in was a no-op -- its condition tested pDoc, which is always 0 by
' the time the inner loop ends -- so it ran exactly once and is dropped.
' ========================================================================================
function FileWatch_ScanMissing() as long
    if gApp.GetDocumentCount = 0 then return 0
    if gFileWatchBusy then return 0
    gFileWatchBusy = true

    dim pDoc as clsDocument ptr = gApp.pDocList
    dim pDocNext as clsDocument ptr

    do until pDoc = 0
        pDocNext = pDoc->pDocNext

        if pDoc->IsNewFlag then
            pDoc = pDocNext: continue do
        end if

        if PsFileExists( pDoc->DiskFilename ) = false then
            ' Ensure that the file is open and displayed
            OpenSelectedDocument( pDoc->DiskFilename )
            FileWatch_ReportMissing( pDoc )

            ' Close the file or remove from project.
            ' The file is gone from disk, so it leaves the workspace entirely -- closing
            ' its tab would now leave a row in the Explorer pointing at nothing.
            OnCommand_ProjectRemove( 0, pDoc )  ' the first param (id) is not used.
        end if

        pDoc = pDocNext
    loop

    gFileWatchBusy = false
    return 0
end function


' ========================================================================================
' SELF-TEST                                             (TIKO_FILEWATCH_SELFTEST=1)
'
' Headless -- it drives FileWatch_Decide only, which is why it can be exhaustive. Called from
' frmMain's WM_CREATE.
'
' WHAT THIS SUITE CANNOT SEE, said here rather than left as a flattering number:
'   * that the timer is ever armed, or fires
'   * that FileWatch_ReloadInPlace restores the caret, scroll, bookmarks or folds -- it needs a
'     real Scintilla window and a real file
'   * that either message box renders, or that its buttons carry the localized text
'   * that focus lands back on the code window after a reload, or that a SILENT reload leaves
'     focus alone when it was somewhere else -- both need a real focused window
'   * the foreground-window gate in FileWatch_CheckNow
' All of that is the interactive pass's, and the reload-in-place restore is the riskiest part
' of this change precisely because nothing here reaches it.
' ========================================================================================
sub FileWatch_RunSelfTest()

    if environ("TIKO_FILEWATCH_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0

    #macro FWCHK( cond, label )
        scope
            dim as string sMsg = label
            if (cond) then
                nPass += 1
            else
                nFail += 1
                ? "FAIL  " & sMsg
            end if
        end scope
    #endmacro

    ? ""
    ? "--- TIKO_FILEWATCH_SELFTEST ---"

    ' ---- an unchanged timestamp is never any kind of event, whatever else is true ----------
    ' 16 cases: the other four booleans cannot resurrect a file that did not change.
    for i as long = 0 to 15
        dim as boolean b1 = cbool( (i and 1) <> 0 )
        dim as boolean b2 = cbool( (i and 2) <> 0 )
        dim as boolean b3 = cbool( (i and 4) <> 0 )
        dim as boolean b4 = cbool( (i and 8) <> 0 )
        FWCHK( FileWatch_Decide( false, b1, b2, b3, b4 ) = FW_ACTION_NONE, _
               "unchanged time is NONE, case " & str(i) )
    next

    ' ---- THE SETTLE RULE (timer path) -----------------------------------------------------
    ' First sighting of a change parks it, regardless of the option or the dirty flag: a file
    ' still being written must not be read even when a silent reload would be allowed.
    FWCHK( FileWatch_Decide( true, true, false, true,  false ) = FW_ACTION_PARK, _
           "timer, unsettled, option on, clean -> PARK" )
    FWCHK( FileWatch_Decide( true, true, false, false, false ) = FW_ACTION_PARK, _
           "timer, unsettled, option off, clean -> PARK" )
    FWCHK( FileWatch_Decide( true, true, false, true,  true  ) = FW_ACTION_PARK, _
           "timer, unsettled, option on, dirty -> PARK" )
    FWCHK( FileWatch_Decide( true, true, false, false, true  ) = FW_ACTION_PARK, _
           "timer, unsettled, option off, dirty -> PARK" )

    ' Once the same time is read back, the normal rules apply.
    FWCHK( FileWatch_Decide( true, true, true, true,  false ) = FW_ACTION_RELOAD, _
           "timer, settled, option on, clean -> RELOAD" )
    FWCHK( FileWatch_Decide( true, true, true, true,  true  ) = FW_ACTION_ASK, _
           "timer, settled, option on, DIRTY -> ASK" )
    FWCHK( FileWatch_Decide( true, true, true, false, false ) = FW_ACTION_ASK, _
           "timer, settled, option off, clean -> ASK" )

    ' ---- THE ACTIVATION PATH DOES NOT SETTLE ----------------------------------------------
    ' Same inputs as the PARK cases above, but bRequireSettled false: it acts on first sight.
    ' This is the pair that pins the two paths apart -- if bRequireSettled were ignored, the
    ' four assertions above and these four could not both hold.
    FWCHK( FileWatch_Decide( true, false, false, true,  false ) = FW_ACTION_RELOAD, _
           "activation, unsettled, option on, clean -> RELOAD (no parking)" )
    FWCHK( FileWatch_Decide( true, false, false, false, false ) = FW_ACTION_ASK, _
           "activation, unsettled, option off, clean -> ASK" )
    FWCHK( FileWatch_Decide( true, false, false, true,  true  ) = FW_ACTION_ASK, _
           "activation, unsettled, option on, DIRTY -> ASK" )
    FWCHK( FileWatch_Decide( true, false, false, false, true  ) = FW_ACTION_ASK, _
           "activation, unsettled, option off, dirty -> ASK" )

    ' ---- A DIRTY BUFFER IS NEVER SILENTLY RELOADED ----------------------------------------
    ' The one rule that loses data if it breaks. Asserted over every combination of the two
    ' path booleans, so no route through the function can reach RELOAD while dirty.
    for i as long = 0 to 3
        dim as boolean bSettleReq = cbool( (i and 1) <> 0 )
        dim as boolean bPending   = cbool( (i and 2) <> 0 )
        FWCHK( FileWatch_Decide( true, bSettleReq, bPending, true, true ) <> FW_ACTION_RELOAD, _
               "dirty buffer is never RELOAD, case " & str(i) )
    next

    ' ---- THE OPTION IS WHAT MAKES SILENCE POSSIBLE ----------------------------------------
    ' With it off, no combination reaches RELOAD -- i.e. default-off really is the old
    ' always-prompt behaviour and not merely a quieter version of it.
    for i as long = 0 to 7
        dim as boolean bSettleReq = cbool( (i and 1) <> 0 )
        dim as boolean bPending   = cbool( (i and 2) <> 0 )
        dim as boolean bDirty     = cbool( (i and 4) <> 0 )
        FWCHK( FileWatch_Decide( true, bSettleReq, bPending, false, bDirty ) <> FW_ACTION_RELOAD, _
               "option off never reaches RELOAD, case " & str(i) )
    next

    ' ---- the default really is off --------------------------------------------------------
    scope
        dim as clsConfig cfgFresh
        FWCHK( cfgFresh.DetectExternalFileChanges = 0, _
               "gConfig default for DetectExternalFileChanges is FALSE" )
    end scope

    ' ---- the three localized strings the boxes need resolve to something ------------------
    ' L() DISCARDS ITS DEFAULT (modDeclares.bi), so a missing .lang id renders a BLANK button
    ' or a blank message with nothing else reporting it.
    FWCHK( len(LL(94))  > 0, "localized id 94 (Yes) is not empty" )
    FWCHK( len(LL(95))  > 0, "localized id 95 (No) is not empty" )
    FWCHK( len(LL(206)) > 0, "localized id 206 (changed prompt) is not empty" )
    FWCHK( len(LL(207)) > 0, "localized id 207 (caption) is not empty" )
    FWCHK( len(LL(222)) > 0, "localized id 222 (deleted notice) is not empty" )
    FWCHK( len(LL(93))  > 0, "localized id 93 (the option's caption) is not empty" )

    ' A blank id passes the check above under ANY language file, because LoadLocalizationFile
    ' fills an empty translation with the English text -- so print the two new ones rather than
    ' claiming a language-independent pass. Under english.lang these read Yes/No.
    dim as string sShow = "NOTE  id 94=" & str(LL(94)) & " | id 95=" & str(LL(95))
    ? sShow

    dim as string sRpt = "--- file watch: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sRpt

end sub
