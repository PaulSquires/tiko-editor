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

' ========================================================================================
' modNavHistory  --  implementation. See modNavHistory.bi for the model.
' ========================================================================================

#include once "app/modNavHistory.bi"
#include once "modRoutines.bi"


' ========================================================================================
' Empty the history. Called from the project load path in clsConfig, which
' closes every open document underneath us -- the entries would still be *valid* (they are
' filenames now, not pointers) but they would point into the workspace the user just left.
' ========================================================================================
sub NavHistory_Clear()
    erase gNavEntries
    gNavCursor = -1
    gNavPendingActive = false
end sub


' ========================================================================================
' How many entries the history holds.
' ========================================================================================
function NavHistory_Count() as long
    function = ubound(gNavEntries) - lbound(gNavEntries) + 1
end function


' ========================================================================================
' Can we step back / forward. These drive the menu greying as well as the commands.
' ========================================================================================
function NavHistory_CanBack() as boolean
    function = ( gNavCursor > 0 )
end function

function NavHistory_CanForward() as boolean
    function = ( gNavCursor >= 0 ) andalso ( gNavCursor < ubound(gNavEntries) )
end function


' ========================================================================================
' Is the stored caret offset still meaningful? An absolute character offset survives only
' while the document is untouched, so the document length at capture time is the test. When
' it has moved, the offset points at whatever text has since slid into that spot and the
' LINE is the better answer.
' ========================================================================================
function NavHistory_UseExactPosition( _
            byref e as NAVHISTORY_ENTRY, _
            byval nDocLength as long _
            ) as boolean
    function = ( e.nDocLength = nDocLength )
end function


' ========================================================================================
' Append an entry, and with it every rule the history has: a new navigation truncates the
' forward trail, a departure from where we already are updates in place rather than growing
' the list, and the list is capped from the front.
'
' Returns TRUE if the list grew, FALSE if the current entry was merely refreshed.
'
' ALL of the array arithmetic lives here on purpose -- it is the only part of this module
' that can be asserted without a window, and every producer (jumps, tab clicks, flushed
' edits) reaches the history through it, so they cannot each get the rules subtly different.
' ========================================================================================
function NavHistory_AppendEntry( byref e as NAVHISTORY_ENTRY ) as boolean

    if PsLen(e.wszFilename) = 0 then exit function

    ' A new navigation kills whatever we could previously have gone Forward to.
    if gNavCursor < ubound(gNavEntries) then
        if gNavCursor < 0 then
            erase gNavEntries
        else
            redim preserve gNavEntries( gNavCursor )
        end if
    end if

    ' Same file AND same line as where we already are: refresh in place. This is what stops
    ' repeated Find Next presses, or three edits on one line, filling the history with
    ' near-identical entries.
    if gNavCursor >= 0 then
        dim as boolean bSameFile = ( PsUCase(gNavEntries(gNavCursor).wszFilename) = PsUCase(e.wszFilename) )
        if bSameFile andalso ( gNavEntries(gNavCursor).nLine = e.nLine ) then
            gNavEntries(gNavCursor) = e
            exit function
        end if
    end if

    redim preserve gNavEntries( ubound(gNavEntries) + 1 )
    gNavEntries( ubound(gNavEntries) ) = e

    ' Cap: drop the OLDEST entry. Shift down one and shrink.
    if NavHistory_Count() > NAVHISTORY_MAX then
        for i as long = lbound(gNavEntries) to ubound(gNavEntries) - 1
            gNavEntries(i) = gNavEntries(i + 1)
        next
        redim preserve gNavEntries( ubound(gNavEntries) - 1 )
    end if

    gNavCursor = ubound(gNavEntries)

    function = true
end function


' ========================================================================================
' Capture the live caret position of the active document into an entry. Returns FALSE when
' there is nothing to capture (no active document, or one with no disk name yet).
' ========================================================================================
function NavHistory_CaptureLive( byref e as NAVHISTORY_ENTRY ) as boolean

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
    if len(pDoc->DiskFilename) = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then exit function

    e.wszFilename = pDoc->DiskFilename
    e.nPosition   = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    e.nLine       = SciExec( hEdit, SCI_LINEFROMPOSITION, e.nPosition, 0 )
    e.nFirstLine  = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0 )
    e.nDocLength  = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )

    function = true
end function


' ========================================================================================
' Should we be recording at all right now?
'
' The suppression counter is the load-bearing one: NavHistory_Goto drives
' OpenSelectedDocument, which is itself a recording site, so without this a Back would
' record its own destination and the cursor would never actually move.
'
' The loading flags matter just as much in practice -- opening a session pushes every
' document in it through OpenSelectedDocument, which would arrive at the desktop with a
' history full of startup noise.
' ========================================================================================
function NavHistory_IsRecordingAllowed() as boolean
    if gNavSuppress <> 0 then exit function
    if gApp.IsProjectLoading then exit function
    if gApp.IsFileLoading then exit function
    if gApp.SuppressNotify then exit function
    function = true
end function


' ========================================================================================
' Record the position we are LEAVING. Called at the top of every navigation.
' ========================================================================================
sub NavHistory_RecordJump()
    if NavHistory_IsRecordingAllowed() = false then exit sub

    ' Any edit location still pending belongs in the history ahead of this jump.
    NavHistory_FlushPendingEdit()

    dim as NAVHISTORY_ENTRY e
    if NavHistory_CaptureLive( e ) then NavHistory_AppendEntry( e )
end sub


' ========================================================================================
' Record the position we have ARRIVED at. Called at the bottom of every navigation, so that
' a later Back has somewhere to come from and a Forward has somewhere to return to.
' ========================================================================================
sub NavHistory_NoteArrival()
    if NavHistory_IsRecordingAllowed() = false then exit sub

    dim as NAVHISTORY_ENTRY e
    if NavHistory_CaptureLive( e ) then NavHistory_AppendEntry( e )
end sub


' ========================================================================================
' Bring the current entry up to date with the live caret before we step away from it.
'
' The caret moves constantly without recording anything (arrow keys, clicking in the text),
' so by the time the user presses Alt+Left the entry describing "here" is stale. Refresh it
' -- but ONLY when it really is the same document, and WITHOUT touching the forward trail,
' which is what makes this different from RecordJump.
' ========================================================================================
sub NavHistory_RefreshCurrent()
    if gNavCursor < 0 then exit sub

    dim as NAVHISTORY_ENTRY e
    if NavHistory_CaptureLive( e ) = false then exit sub

    if PsUCase(gNavEntries(gNavCursor).wszFilename) = PsUCase(e.wszFilename) then
        gNavEntries(gNavCursor) = e
    end if
end sub


' ========================================================================================
' Navigate to a history entry. Reuses OpenSelectedDocument, which already handles every
' awkward case -- document not open (create + load from disk), open but with no tab (AddTab),
' and the target line sitting inside a collapsed fold.
'
' Called with nLineNumber = -1 deliberately: that tells OpenSelectedDocument not to
' reposition, because we do it ourselves below with more information than a line number.
' ========================================================================================
sub NavHistory_Goto( byval idx as long )

    if ( idx < lbound(gNavEntries) ) orelse ( idx > ubound(gNavEntries) ) then exit sub

    ' EVERYTHING inside this bracket must not record. OpenSelectedDocument and the tab
    ' switch it performs are both recording sites.
    gNavSuppress += 1

    dim pDoc as clsDocument ptr = OpenSelectedDocument( gNavEntries(idx).wszFilename )

    if pDoc then
        dim as HWND hEdit = pDoc->hWndActiveScintilla
        dim as long nDocLength = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )

        if NavHistory_UseExactPosition( gNavEntries(idx), nDocLength ) then
            SciExec( hEdit, SCI_GOTOPOS, gNavEntries(idx).nPosition, 0 )
        else
            ' The file has been edited since we were here. The absolute offset is
            ' meaningless now; the line is the best answer we have.
            dim as long nLine = gNavEntries(idx).nLine
            if SciExec( hEdit, SCI_GETLINEVISIBLE, nLine, 0 ) = false then
                pDoc->FoldToggle( nLine )
            end if
            SciExec( hEdit, SCI_GOTOLINE, nLine, 0 )
        end if

        SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, gNavEntries(idx).nFirstLine, 0 )
    end if

    gNavSuppress -= 1
end sub


' ========================================================================================
' Edit tracking. SCN_MODIFIED stamps the location; nothing is appended yet, because a run of
' typing on one line has to collapse to a single entry.
' ========================================================================================
sub NavHistory_NoteEdit( byval pDoc as clsDocument ptr )
    if NavHistory_IsRecordingAllowed() = false then exit sub
    if pDoc = 0 then exit sub
    if len(pDoc->DiskFilename) = 0 then exit sub

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then exit sub

    dim as long nPosition = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nLine     = SciExec( hEdit, SCI_LINEFROMPOSITION, nPosition, 0 )

    ' Moved to a different edit location: the previous one is now history.
    if gNavPendingActive then
        dim as boolean bSameFile = ( PsUCase(gNavPending.wszFilename) = ucase(pDoc->DiskFilename) )
        if ( bSameFile = false ) orelse ( gNavPending.nLine <> nLine ) then
            NavHistory_FlushPendingEdit()
        end if
    end if

    gNavPending.wszFilename = pDoc->DiskFilename
    gNavPending.nPosition   = nPosition
    gNavPending.nLine       = nLine
    gNavPending.nFirstLine  = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0 )
    gNavPending.nDocLength  = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    gNavPendingActive       = true
end sub


' ========================================================================================
' The debounce. Called from the caret-changed branch of SCN_UPDATEUI: once the caret has
' left the line being edited, that edit location has finished and becomes an entry.
' ========================================================================================
sub NavHistory_CheckPendingEdit( byval pDoc as clsDocument ptr )
    if gNavPendingActive = false then exit sub
    if NavHistory_IsRecordingAllowed() = false then exit sub
    if pDoc = 0 then exit sub

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then exit sub

    dim as long nPosition = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nLine     = SciExec( hEdit, SCI_LINEFROMPOSITION, nPosition, 0 )

    dim as boolean bSameFile = ( PsUCase(gNavPending.wszFilename) = ucase(pDoc->DiskFilename) )
    if ( bSameFile = false ) orelse ( gNavPending.nLine <> nLine ) then
        NavHistory_FlushPendingEdit()
    end if
end sub


' ========================================================================================
' Move the pending edit location into the history.
' ========================================================================================
sub NavHistory_FlushPendingEdit()
    if gNavPendingActive = false then exit sub
    gNavPendingActive = false
    NavHistory_AppendEntry( gNavPending )
end sub


' ========================================================================================
' Go Back / Go Forward.
' ========================================================================================
function NavHistory_Back() as long
    ' Flush BEFORE the test, not after. An edit location that has not been flushed yet is
    ' still "where we are" -- with one entry in the list and a pending edit further down the
    ' same file, testing first would refuse a Back that is perfectly legitimate.
    NavHistory_FlushPendingEdit()
    if NavHistory_CanBack() = false then exit function
    NavHistory_RefreshCurrent()
    gNavCursor -= 1
    NavHistory_Goto( gNavCursor )
    function = 0
end function

function NavHistory_Forward() as long
    if NavHistory_CanForward() = false then exit function
    NavHistory_RefreshCurrent()
    gNavCursor += 1
    NavHistory_Goto( gNavCursor )
    function = 0
end function


' ========================================================================================
' Env-gated self-test: TIKO_NAVHISTORY_SELFTEST=1
'
' MOSTLY HEADLESS, and honest about what that costs. Everything from here to the
' suppression section is pure array arithmetic -- the cursor, the forward truncation, the
' cap and the dedup -- which is genuinely all of the logic this feature can get wrong
' *silently*. It reaches NONE of the capture (no Scintilla window exists at startup) and
' none of the rendering, so it says nothing about whether Alt+Left lands the caret in the
' right place. The interactive pass is the load-bearing evidence for that half.
'
' The ONE test here that crosses a real call is the last one, and it exists because the
' suppression bracket in NavHistory_Goto fails silently: without it, Back records its own
' destination and the cursor never advances, with a clean build and (with a purely pure
' suite) a green report.
' ========================================================================================
dim shared gNavTestPass as long
dim shared gNavTestFail as long

private sub Nav_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gNavTestPass += 1
    else
        gNavTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

private function Nav_MakeEntry( _
            byval sFile as string, _
            byval nLine as long, _
            byval nPosition as long = 0, _
            byval nDocLength as long = 1000 _
            ) as NAVHISTORY_ENTRY
    dim as NAVHISTORY_ENTRY e
    e.wszFilename = sFile
    e.nLine       = nLine
    e.nPosition   = nPosition
    e.nFirstLine  = 0
    e.nDocLength  = nDocLength
    function = e
end function

sub NavHistory_RunSelfTest()
    if environ("TIKO_NAVHISTORY_SELFTEST") <> "1" then exit sub

    gNavTestPass = 0
    gNavTestFail = 0
    print "--- TIKO_NAVHISTORY_SELFTEST ---"

    dim as NAVHISTORY_ENTRY e

    ' --- the empty state ---------------------------------------------------------------
    NavHistory_Clear()
    Nav_Check( "empty: count is 0", NavHistory_Count() = 0, str(NavHistory_Count()) )
    Nav_Check( "empty: cursor is -1", gNavCursor = -1, str(gNavCursor) )
    Nav_Check( "empty: cannot go back", NavHistory_CanBack() = false )
    Nav_Check( "empty: cannot go forward", NavHistory_CanForward() = false )

    ' --- growing the list --------------------------------------------------------------
    ' ONE entry is not enough to go back to: the single entry IS where we are.
    Nav_Check( "append A reports growth", NavHistory_AppendEntry( Nav_MakeEntry( "a.bas", 10 ) ) = true )
    Nav_Check( "one entry: count is 1", NavHistory_Count() = 1, str(NavHistory_Count()) )
    Nav_Check( "one entry: cursor is 0", gNavCursor = 0, str(gNavCursor) )
    Nav_Check( "one entry: cannot go back", NavHistory_CanBack() = false )
    Nav_Check( "one entry: cannot go forward", NavHistory_CanForward() = false )

    NavHistory_AppendEntry( Nav_MakeEntry( "b.bas", 20 ) )
    Nav_Check( "two entries: count is 2", NavHistory_Count() = 2, str(NavHistory_Count()) )
    Nav_Check( "two entries: cursor is 1", gNavCursor = 1, str(gNavCursor) )
    Nav_Check( "two entries: can go back", NavHistory_CanBack() = true )
    Nav_Check( "two entries: still cannot go forward", NavHistory_CanForward() = false )

    NavHistory_AppendEntry( Nav_MakeEntry( "c.bas", 30 ) )
    Nav_Check( "three entries: cursor tracks the end", gNavCursor = 2, str(gNavCursor) )

    ' --- the cursor, stepped by hand (Back/Forward themselves need a window) -----------
    gNavCursor = 0
    Nav_Check( "cursor at the start: cannot go back", NavHistory_CanBack() = false )
    Nav_Check( "cursor at the start: CAN go forward", NavHistory_CanForward() = true )
    gNavCursor = 2
    Nav_Check( "cursor at the end: can go back", NavHistory_CanBack() = true )
    Nav_Check( "cursor at the end: cannot go forward", NavHistory_CanForward() = false )

    ' --- a new navigation truncates the forward trail -----------------------------------
    ' This is the whole difference between a browser history and a plain stack.
    gNavCursor = 0
    NavHistory_AppendEntry( Nav_MakeEntry( "d.bas", 40 ) )
    Nav_Check( "truncate: count drops to 2", NavHistory_Count() = 2, str(NavHistory_Count()) )
    Nav_Check( "truncate: cursor is the new entry", gNavCursor = 1, str(gNavCursor) )
    Nav_Check( "truncate: the surviving entry is the one at the cursor", _
        gNavEntries(0).wszFilename = "a.bas", gNavEntries(0).wszFilename )
    Nav_Check( "truncate: forward is gone", NavHistory_CanForward() = false )

    ' --- dedup: BOTH file and line must match ------------------------------------------
    Nav_Check( "dedup: same file and line does not grow", _
        NavHistory_AppendEntry( Nav_MakeEntry( "d.bas", 40, 999 ) ) = false )
    Nav_Check( "dedup: count unchanged", NavHistory_Count() = 2, str(NavHistory_Count()) )
    ' ...but it DOES refresh in place, which is how a departure point stays current.
    Nav_Check( "dedup: the entry was refreshed in place", _
        gNavEntries(1).nPosition = 999, str(gNavEntries(1).nPosition) )
    Nav_Check( "dedup: same file, DIFFERENT line grows", _
        NavHistory_AppendEntry( Nav_MakeEntry( "d.bas", 41 ) ) = true )
    Nav_Check( "dedup: different file, same line grows", _
        NavHistory_AppendEntry( Nav_MakeEntry( "e.bas", 41 ) ) = true )
    Nav_Check( "dedup: filename compare is case-insensitive", _
        NavHistory_AppendEntry( Nav_MakeEntry( "E.BAS", 41 ) ) = false )

    ' --- an entry with no filename is refused ------------------------------------------
    ' An unsaved document has no DiskFilename, and a nameless entry could never be
    ' navigated back to.
    dim as long nBefore = NavHistory_Count()
    Nav_Check( "no filename: refused", NavHistory_AppendEntry( Nav_MakeEntry( "", 5 ) ) = false )
    Nav_Check( "no filename: count unchanged", NavHistory_Count() = nBefore, str(NavHistory_Count()) )

    ' --- the cap trims from the FRONT --------------------------------------------------
    NavHistory_Clear()
    for i as long = 1 to NAVHISTORY_MAX + 10
        NavHistory_AppendEntry( Nav_MakeEntry( "f" & i & ".bas", i ) )
    next
    Nav_Check( "cap: count stops at the maximum", NavHistory_Count() = NAVHISTORY_MAX, str(NavHistory_Count()) )
    Nav_Check( "cap: cursor still points at the end", gNavCursor = NAVHISTORY_MAX - 1, str(gNavCursor) )
    ' The OLDEST entry is the one dropped, so the newest must survive and the first must not.
    Nav_Check( "cap: the newest entry survived", _
        gNavEntries(gNavCursor).nLine = NAVHISTORY_MAX + 10, str(gNavEntries(gNavCursor).nLine) )
    Nav_Check( "cap: the oldest entries were dropped from the front", _
        gNavEntries(0).nLine = 11, str(gNavEntries(0).nLine) )

    ' --- exact position vs fall back to the line ---------------------------------------
    ' The offset is only trustworthy while the document is the length it was at capture.
    e = Nav_MakeEntry( "g.bas", 12, 500, 8000 )
    Nav_Check( "position: same length uses the exact offset", _
        NavHistory_UseExactPosition( e, 8000 ) = true )
    Nav_Check( "position: a longer document falls back to the line", _
        NavHistory_UseExactPosition( e, 8001 ) = false )
    Nav_Check( "position: a shorter document falls back to the line", _
        NavHistory_UseExactPosition( e, 7999 ) = false )

    ' --- Clear really clears -----------------------------------------------------------
    gNavPendingActive = true
    NavHistory_Clear()
    Nav_Check( "clear: count is 0", NavHistory_Count() = 0, str(NavHistory_Count()) )
    Nav_Check( "clear: cursor is -1", gNavCursor = -1, str(gNavCursor) )
    Nav_Check( "clear: the pending edit is dropped too", gNavPendingActive = false )

    ' --- the suppression guard ---------------------------------------------------------
    ' PRECONDITION. If recording were already disallowed here (a loading flag still set),
    ' every assertion below would pass for a reason that has nothing to do with the guard.
    Nav_Check( "PRECONDITION: recording is allowed at this point", _
        NavHistory_IsRecordingAllowed() = true )

    gNavSuppress += 1
    Nav_Check( "suppress: recording is refused while set", NavHistory_IsRecordingAllowed() = false )
    gNavSuppress -= 1
    Nav_Check( "suppress: recording is allowed again once cleared", NavHistory_IsRecordingAllowed() = true )

    ' --- THE WITNESS ACROSS THE CALL ---------------------------------------------------
    ' NavHistory_Goto drives OpenSelectedDocument, which records at its top. Nothing about
    ' that is observable from a pure function, so this drives the real Goto and watches a
    ' piece of state that only the recording path touches: a pending edit is flushed by
    ' NavHistory_RecordJump and by nothing else.
    '
    ' The target file deliberately does not exist -- OpenSelectedDocument bails at its
    ' PsFileExists test, AFTER the RecordJump we are testing for, so this needs no window
    ' and opens nothing.
    NavHistory_Clear()
    NavHistory_AppendEntry( Nav_MakeEntry( "C:\__tiko_navhistory_selftest_no_such_file__.bas", 1 ) )
    gNavPending       = Nav_MakeEntry( "h.bas", 77 )
    gNavPendingActive = true
    dim as long nCountBefore = NavHistory_Count()

    NavHistory_Goto( 0 )

    Nav_Check( "goto: the pending edit was NOT flushed (recording was suppressed)", _
        gNavPendingActive = true )
    Nav_Check( "goto: no entry was appended during the navigation", _
        NavHistory_Count() = nCountBefore, str(NavHistory_Count()) )
    Nav_Check( "goto: the suppression counter is balanced afterwards", _
        gNavSuppress = 0, str(gNavSuppress) )

    ' Out-of-range indices are refused rather than trapped.
    NavHistory_Goto( -1 )
    NavHistory_Goto( 99 )
    Nav_Check( "goto: an out-of-range index is a no-op", gNavSuppress = 0, str(gNavSuppress) )

    NavHistory_Clear()

    print "--- TIKO_NAVHISTORY_SELFTEST: " & gNavTestPass & " passed, " & gNavTestFail & " failed ---"
end sub
