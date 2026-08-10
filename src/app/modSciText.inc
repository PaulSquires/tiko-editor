'' ========================================================================================
'' modSciText -- reading and trimming a Scintilla buffer. Portable.
''
'' Split out of modRoutines, which is 25 unrelated declarations carrying twelve Win32
'' violations across nine of them. These two carried none of those: what tied them to the
'' shell was their PARAMETER -- an editor HWND -- and the SciExec macro underneath it.
''
'' ---- SciExec AND SciMsg ARE NOT THE SAME CALL, and that is the whole risk here ---------
''
''     SciExec(hWnd, ...)  ->  SendMessage  ->  SciPs_Send(pSci, ...) + SciHost_FlushDamage
''     SciMsg(pSci, ...)   ->                   SciPs_Send(pSci, ...)
''
'' frmSciHost.inc's SCI_* branch calls SciPs_Send AND THEN FLUSHES, because a message sent
'' from tiko's own code arrives with no input message in flight -- "the only chance to
'' notice what it changed", as the comment there puts it. FlushDamage is an InvalidateRect
'' when the surface has damage.
''
'' So converting SciExec to SciMsg DROPS A REPAINT, and whether that matters depends
'' entirely on the function:
''
''   * Scintilla_GetTextBytes is READ-ONLY -- SCI_GETLENGTH and SCI_GETCHARACTERPOINTER
''     dirty nothing, so there was never any damage to flush. Safe.
''   * Scintilla_StripTrailingWhitespace EDITS: SCI_DELETERANGE in a loop, SCI_GOTOPOS and
''     SCI_SETFIRSTVISIBLELINE after it. And it runs during SAVE. A lost repaint would leave
''     the user looking at text the file no longer contains, until something unrelated
''     happened to invalidate the window.
''
'' THE CALLER INVALIDATES, ONCE. clsDocument.SaveFile calls gAppHost.InvalidateView after
'' the strip returns -- one repaint at the point the edit is finished, in place of sixteen
'' during it, which is also strictly less work than what it replaces.
''
'' ANY FUTURE CALLER OF THE SECOND FUNCTION OWES THAT REPAINT. There is exactly one today.
'' ========================================================================================

#pragma once

' ========================================================================================
' Snapshot the FULL byte extent of a Scintilla buffer.
'
' SCI_GETCHARACTERPOINTER hands back a pointer whose valid length is SCI_GETLENGTH, NOT
' strlen: the buffer can legitimately contain embedded NUL bytes, and reading it with *psz
' (as every save path used to) truncates silently at the first one. Always copy exactly
' SCI_GETLENGTH bytes.
' ========================================================================================
function Scintilla_GetTextBytes( byval pSci as any ptr ) as string
    dim as long nLen = SciMsg( pSci, SCI_GETLENGTH, 0, 0 )
    if nLen <= 0 then return ""
    dim as any ptr psz = cast( any ptr, SciMsg( pSci, SCI_GETCHARACTERPOINTER, 0, 0 ) )
    if psz = 0 then return ""
    dim as string s = string( nLen, 0 )
    memcpy( strptr(s), psz, nLen )
    return s
end function


' ========================================================================================
' Delete the spaces and tabs sitting between the last non-blank character of every line and
' that line's end. Returns the number of lines that were changed.
'
' It edits the SCINTILLA BUFFER rather than the byte string on its way to disk, and that is
' the decision here. Stripping only the outgoing bytes would leave the buffer holding
' whitespace the file no longer has -- the document would look saved while differing from
' what is on disk, which then reloads differently and shows up in any external diff. Editing
' the buffer keeps the two identical, and gives the user an undoable edit they can see.
'
' Positions are BYTE offsets, and both characters searched for are ASCII, so the scan is
' safe on a UTF-8 buffer: no continuation byte can be mistaken for a space or a tab.
' Line positions are re-read on every iteration because each delete shifts everything after
' it, so walking forwards is correct.
'
' THE CALLER REPAINTS -- see this file's header.
' ========================================================================================
function Scintilla_StripTrailingWhitespace( byval pSci as any ptr ) as long
    if pSci = 0 then return 0
    if SciMsg( pSci, SCI_GETREADONLY, 0, 0 ) then return 0

    dim as long nLines = SciMsg( pSci, SCI_GETLINECOUNT, 0, 0 )
    if nLines <= 0 then return 0

    ' The caret is restored by LINE and COLUMN rather than by position: a delete before it
    ' moves the position, and the caret can also be sitting inside the whitespace that is
    ' about to disappear. Same for the first visible line, which a strip never scrolls.
    dim as long nCaretPos = SciMsg( pSci, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciMsg( pSci, SCI_LINEFROMPOSITION, nCaretPos, 0 )
    dim as long nCaretCol = SciMsg( pSci, SCI_GETCOLUMN, nCaretPos, 0 )
    dim as long nFirstVisible = SciMsg( pSci, SCI_GETFIRSTVISIBLELINE, 0, 0 )

    dim as long nChanged = 0

    SciMsg( pSci, SCI_BEGINUNDOACTION, 0, 0 )

    for i as long = 0 to nLines - 1
        dim as long nStart = SciMsg( pSci, SCI_POSITIONFROMLINE, i, 0 )
        ' Excludes the line's EOL characters, so the EOL convention is never touched.
        dim as long nEnd = SciMsg( pSci, SCI_GETLINEENDPOSITION, i, 0 )
        dim as long nTrim = nEnd
        do while nTrim > nStart
            dim as long ch = SciMsg( pSci, SCI_GETCHARAT, nTrim - 1, 0 )
            if (ch <> 32) andalso (ch <> 9) then exit do
            nTrim -= 1
        loop
        if nTrim < nEnd then
            SciMsg( pSci, SCI_DELETERANGE, nTrim, nEnd - nTrim )
            nChanged += 1
        end if
    next

    SciMsg( pSci, SCI_ENDUNDOACTION, 0, 0 )

    if nChanged > 0 then
        dim as long nNewPos = SciMsg( pSci, SCI_FINDCOLUMN, nCaretLine, nCaretCol )
        ' The caret's column may no longer exist on the shortened line, and FINDCOLUMN then
        ' answers past the line end; clamp so it never lands on the next line.
        dim as long nLineEnd = SciMsg( pSci, SCI_GETLINEENDPOSITION, nCaretLine, 0 )
        if nNewPos > nLineEnd then nNewPos = nLineEnd
        SciMsg( pSci, SCI_GOTOPOS, nNewPos, 0 )
        SciMsg( pSci, SCI_SETFIRSTVISIBLELINE, nFirstVisible, 0 )
    end if

    function = nChanged
end function
