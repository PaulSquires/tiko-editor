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
' CODE FORMATTER -- the Scintilla bridge.
'
' modFormat.inc is pure text in / text out and knows nothing about a buffer. Everything
' that has to touch a real editor lives here: the caret, the scroll position, the undo
' grouping, the minimal-edit computation, and the four commands.
'
' THE EDITOR CONTRACT. Each of these is a fix for a failure documented in a shipping
' editor, not a precaution invented here:
'
'   * MINIMAL EDITS, never a whole-buffer replace. tiko's bookmarks, breakpoints, fold
'     points and the debugger's current-line arrow are all Scintilla MARKERS. A marker
'     moves correctly with a ranged edit but is destroyed by SCI_SETTEXT, so formatting a
'     document by replacing its text would silently clear every breakpoint in it. The
'     line-by-line path below exists for that reason and not for speed.
'
'   * CARET BY LINE AND COLUMN, never by position -- Scintilla_StripTrailingWhitespace's
'     discipline (modRoutines.inc), for the same reason: an edit before the caret moves
'     every position after it, and the caret can be sitting inside whitespace that is
'     about to disappear. SCI_FINDCOLUMN restores it, clamped to the line end because the
'     column may no longer exist on a line that got shorter.
'
'   * ONE UNDO ACTION per command, and its OWN undo step rather than merged into the
'     user's typing. That is VS Code's choice over Roslyn's, and ReSharper documents it as
'     a feature: Ctrl+Z rejects the format and keeps what you typed or pasted.
'
'   * SCROLL POSITION restored. A format that scrolls the view is indistinguishable from
'     one that moved your code.
' ========================================================================================


' ========================================================================================
' Byte-exact snapshot of a Scintilla range.
'
' Offsets into SCI_GETCHARACTERPOINTER and copies an explicit byte count, which is the
' same discipline Scintilla_GetTextBytes documents in modRoutines: the buffer can hold
' embedded NULs, and reading it as a C string truncates at the first one. SCI_GETTEXTRANGE
' would work too but needs a Sci_TextRange struct and gives nothing extra here.
' ========================================================================================
private function Scintilla_GetTextRangeBytes( byval hEdit as hwnd, _
                                              byval nFrom as long, _
                                              byval nTo as long ) as string
    if hEdit = 0 then return ""
    dim as long nDocLen = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    if nFrom < 0 then nFrom = 0
    if nTo > nDocLen then nTo = nDocLen
    if nTo <= nFrom then return ""

    dim as any ptr psz = cast( any ptr, SciExec( hEdit, SCI_GETCHARACTERPOINTER, 0, 0 ) )
    if psz = 0 then return ""

    dim as long nLen = nTo - nFrom
    dim as string s = string( nLen, 0 )
    memcpy( strptr(s), cast(ubyte ptr, psz) + nFrom, nLen )
    return s
end function


' ========================================================================================
' Is this document one the formatter should touch?
'
' The same .BAS/.INC/.BI-or-new test clsDocument.ApplyPropertiesToView uses to decide
' whether to apply FreeBASIC styling at all -- so the formatter's idea of "a FreeBASIC
' file" cannot drift from the lexer's. A .RC resource script and a plain text file are
' both legitimately open in tiko and neither is FreeBASIC.
' ========================================================================================
function Format_IsFormattableDocument( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then return false
    if pDoc->hWndActiveScintilla = 0 then return false
    if SciExec( pDoc->hWndActiveScintilla, SCI_GETREADONLY, 0, 0 ) then return false

    if pDoc->IsNewFlag then return true

    dim as string sExt = ucase( AfxStrPathname( "EXTN", pDoc->DiskFilename ) )
    select case sExt
        case ".BAS", ".INC", ".BI"
            return true
    end select
    return false
end function


' ========================================================================================
' The rules and the indent settings currently in force.
'
' Indent width and tabs-vs-spaces come from the DOCUMENT (SCI_GETINDENT) and gConfig
' rather than from the rule set, because they are already user settings with their own UI
' -- duplicating them on the Format page would let a file be formatted to an indent width
' the editor then disagrees with.
' ========================================================================================
private sub Format_GetContext( byval hEdit as hwnd, _
                               byref rules as FORMAT_RULES, _
                               byref nIndentWidth as long, _
                               byref bUseTabs as long )
    rules = gConfig.Format
    nIndentWidth = SciExec( hEdit, SCI_GETINDENT, 0, 0 )
    if nIndentWidth <= 0 then nIndentWidth = 4
    bUseTabs = iif( gConfig.TabIndentSpaces, false, true )
end sub


' ========================================================================================
' Split text into lines, keeping each line's EOL with it so a rebuild is byte-exact.
' ========================================================================================
private function Format_SplitLines( byref sText as string, lines() as string ) as long
    dim as long nLen = len(sText)
    dim as long nCount = 0

    if nLen = 0 then
        redim lines(0 to 0)
        return 0
    end if

    dim as long nCap = 1
    for i as long = 1 to nLen
        if asc(mid(sText, i, 1)) = 10 then nCap += 1
    next
    redim lines(0 to nCap + 1)

    dim as long nStart = 1
    dim as long i = 1
    while i <= nLen
        dim as long c = asc(mid(sText, i, 1))
        if (c = 13) orelse (c = 10) then
            dim as long nEnd = i
            if (c = 13) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 10) then nEnd += 1
            lines(nCount) = mid(sText, nStart, nEnd - nStart + 1)
            nCount += 1
            i = nEnd + 1
            nStart = i
        else
            i += 1
        end if
    wend
    if nStart <= nLen then
        lines(nCount) = mid(sText, nStart)
        nCount += 1
    end if

    return nCount
end function


' ========================================================================================
' Strip a line's trailing EOL characters, leaving the body.
' ========================================================================================
private function Format_LineBody( byref sLine as string ) as string
    dim as long n = len(sLine)
    while n > 0
        dim as long c = asc(mid(sLine, n, 1))
        if (c <> 13) andalso (c <> 10) then exit while
        n -= 1
    wend
    return left(sLine, n)
end function


' ========================================================================================
' Apply formatted text back to a run of lines, as the SMALLEST set of edits that gets
' there. Returns the number of lines actually written.
'
' Two paths, and the split matters:
'
'   * SAME LINE COUNT (every rule except the two blank-line ones) -- replace only the
'     individual lines whose text differs, walking BACKWARDS so that earlier line
'     positions are still valid after each edit. Every marker in the document survives,
'     including on the lines being rewritten, because the line itself is never deleted.
'
'   * DIFFERENT LINE COUNT -- lines are being inserted or removed, so per-line replacement
'     cannot express it. Fall back to replacing the smallest span that differs, found by
'     matching the common prefix and suffix. Markers outside that span still survive; ones
'     inside it are on lines that genuinely moved.
' ========================================================================================
private function Format_ApplyLines( byval hEdit as hwnd, _
                                    byval nFirstLine as long, _
                                    byval nLastLine as long, _
                                    byref sNew as string ) as long
    dim as string oldLines(any), newLines(any)

    dim as long nPosStart = SciExec( hEdit, SCI_POSITIONFROMLINE, nFirstLine, 0 )
    dim as long nPosEnd
    dim as long nDocLines = SciExec( hEdit, SCI_GETLINECOUNT, 0, 0 )
    if nLastLine >= nDocLines - 1 then
        nPosEnd = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    else
        nPosEnd = SciExec( hEdit, SCI_POSITIONFROMLINE, nLastLine + 1, 0 )
    end if
    if nPosEnd < nPosStart then return 0

    dim as string sOld = Scintilla_GetTextRangeBytes( hEdit, nPosStart, nPosEnd )
    if sOld = sNew then return 0

    dim as long nOld = Format_SplitLines( sOld, oldLines() )
    dim as long nNew = Format_SplitLines( sNew, newLines() )

    dim as long nWritten = 0

    if nOld = nNew then
        ' -- per-line, backwards ---------------------------------------------------------
        for i as long = nOld - 1 to 0 step -1
            if oldLines(i) = newLines(i) then continue for
            dim as long nLine = nFirstLine + i
            dim as long nLS = SciExec( hEdit, SCI_POSITIONFROMLINE, nLine, 0 )
            dim as long nLE = SciExec( hEdit, SCI_GETLINEENDPOSITION, nLine, 0 )
            dim as string sBody = Format_LineBody( newLines(i) )
            SciExec( hEdit, SCI_SETTARGETSTART, nLS, 0 )
            SciExec( hEdit, SCI_SETTARGETEND, nLE, 0 )
            SciExec( hEdit, SCI_REPLACETARGET, len(sBody), cast(LPARAM, strptr(sBody)) )
            nWritten += 1
        next
        return nWritten
    end if

    ' -- differing line counts: replace the smallest differing span ----------------------
    dim as long nPre = 0
    while (nPre < nOld) andalso (nPre < nNew) andalso (oldLines(nPre) = newLines(nPre))
        nPre += 1
    wend
    dim as long nSuf = 0
    while (nSuf < nOld - nPre) andalso (nSuf < nNew - nPre) andalso _
          (oldLines(nOld - 1 - nSuf) = newLines(nNew - 1 - nSuf))
        nSuf += 1
    wend

    dim as string sMid = ""
    for i as long = nPre to nNew - 1 - nSuf
        sMid &= newLines(i)
    next

    dim as long nRepStart = SciExec( hEdit, SCI_POSITIONFROMLINE, nFirstLine + nPre, 0 )
    dim as long nRepEnd
    dim as long nAfterLine = nFirstLine + nOld - nSuf
    if nAfterLine >= SciExec( hEdit, SCI_GETLINECOUNT, 0, 0 ) then
        nRepEnd = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    else
        nRepEnd = SciExec( hEdit, SCI_POSITIONFROMLINE, nAfterLine, 0 )
    end if
    if nRepEnd < nRepStart then nRepEnd = nRepStart

    SciExec( hEdit, SCI_SETTARGETSTART, nRepStart, 0 )
    SciExec( hEdit, SCI_SETTARGETEND, nRepEnd, 0 )
    SciExec( hEdit, SCI_REPLACETARGET, len(sMid), cast(LPARAM, strptr(sMid)) )

    return nNew - nPre - nSuf
end function


' ========================================================================================
' Format a run of lines of one document, preserving the caret and the scroll position.
' Returns the number of lines changed.
'
' nFirstLine/nLastLine are 0-based and inclusive. Pass -1/-1 for the whole document.
' ========================================================================================
function Format_ApplyToDocument( byval pDoc as clsDocument ptr, _
                                 byval nFirstLine as long, _
                                 byval nLastLine as long ) as long
    if Format_IsFormattableDocument( pDoc ) = false then return 0
    dim as hwnd hEdit = pDoc->hWndActiveScintilla

    dim as long nDocLines = SciExec( hEdit, SCI_GETLINECOUNT, 0, 0 )
    if nDocLines <= 0 then return 0
    dim as long bWholeDoc = false
    if nFirstLine < 0 then
        nFirstLine = 0 : nLastLine = nDocLines - 1 : bWholeDoc = true
    end if
    if nLastLine >= nDocLines then nLastLine = nDocLines - 1
    if nLastLine < nFirstLine then return 0

    dim as FORMAT_RULES rules
    dim as long nIndentWidth, bUseTabs
    Format_GetContext( hEdit, rules, nIndentWidth, bUseTabs )

    ' -- the text to format --------------------------------------------------------------
    dim as long nPosStart = SciExec( hEdit, SCI_POSITIONFROMLINE, nFirstLine, 0 )
    dim as long nPosEnd
    if nLastLine >= nDocLines - 1 then
        nPosEnd = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    else
        nPosEnd = SciExec( hEdit, SCI_POSITIONFROMLINE, nLastLine + 1, 0 )
    end if
    dim as string sSrc = Scintilla_GetTextRangeBytes( hEdit, nPosStart, nPosEnd )
    if len(sSrc) = 0 then return 0

    ' -- format ---------------------------------------------------------------------------
    dim as long bChanged = false
    dim as string sOut

    if bWholeDoc then
        ' The whole-file entry point, which additionally requires the result to be a FIXED
        ' POINT -- format(format(x)) == format(x). A fragment cannot be held to that.
        sOut = Format_Text( sSrc, rules, bChanged, nIndentWidth, bUseTabs )
    else
        ' A selection's block depth cannot be recovered from the selection itself, so it is
        ' computed from the top of the file and handed in.
        dim as string sWhole = Scintilla_GetTextBytes( hEdit )
        dim as long nDepth = Format_DepthAtLine( sWhole, nFirstLine )
        sOut = Format_TextAtDepth( sSrc, rules, nDepth, bChanged, nIndentWidth, bUseTabs )
    end if

    if bChanged = false then return 0

    ' -- caret and scroll, captured before any edit ----------------------------------------
    dim as long nCaretPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nCaretPos, 0 )
    dim as long nCaretCol = SciExec( hEdit, SCI_GETCOLUMN, nCaretPos, 0 )
    dim as long nFirstVisible = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0 )

    ' -- apply, as ONE undo action ---------------------------------------------------------
    SciExec( hEdit, SCI_BEGINUNDOACTION, 0, 0 )
    dim as long nChanged = Format_ApplyLines( hEdit, nFirstLine, nLastLine, sOut )
    SciExec( hEdit, SCI_ENDUNDOACTION, 0, 0 )

    if nChanged > 0 then
        ' Restore by LINE and COLUMN. The column may no longer exist on a line that lost
        ' its leading whitespace, and SCI_FINDCOLUMN then answers past the line end, so it
        ' is clamped or the caret lands on the following line.
        dim as long nTotal = SciExec( hEdit, SCI_GETLINECOUNT, 0, 0 )
        if nCaretLine >= nTotal then nCaretLine = nTotal - 1
        if nCaretLine < 0 then nCaretLine = 0
        dim as long nNewPos = SciExec( hEdit, SCI_FINDCOLUMN, nCaretLine, nCaretCol )
        dim as long nLineEnd = SciExec( hEdit, SCI_GETLINEENDPOSITION, nCaretLine, 0 )
        if nNewPos > nLineEnd then nNewPos = nLineEnd
        SciExec( hEdit, SCI_GOTOPOS, nNewPos, 0 )
        SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, nFirstVisible, 0 )
    end if

    return nChanged
end function


' ========================================================================================
' Format Document.
' ========================================================================================
function Format_Document() as long
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0
    return Format_ApplyToDocument( pDoc, -1, -1 )
end function


' ========================================================================================
' Format Selection. Falls back to the caret's own line when nothing is selected, which is
' what every editor with this command does -- a no-op would just look broken.
' ========================================================================================
function Format_Selection() as long
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then return 0

    dim as long nSelStart = SciExec( hEdit, SCI_GETSELECTIONSTART, 0, 0 )
    dim as long nSelEnd = SciExec( hEdit, SCI_GETSELECTIONEND, 0, 0 )
    dim as long nFirst = SciExec( hEdit, SCI_LINEFROMPOSITION, nSelStart, 0 )
    dim as long nLast = SciExec( hEdit, SCI_LINEFROMPOSITION, nSelEnd, 0 )

    ' A selection ending exactly at the start of a line does not include that line -- the
    ' user dragged to the line break, not into the text below it.
    if (nLast > nFirst) andalso (nSelEnd = SciExec( hEdit, SCI_POSITIONFROMLINE, nLast, 0 )) then
        nLast -= 1
    end if

    return Format_ApplyToDocument( pDoc, nFirst, nLast )
end function


' ========================================================================================
' Format All Open Documents.
'
' Skips the Find-in-Project results tab (it is an excerpt view, not a file), anything
' read-only, and anything that is not FreeBASIC. Each document is formatted through the
' normal undoable buffer path, so every one of them can be individually undone.
' ========================================================================================
function Format_AllOpenDocuments( byref nFilesChanged as long ) as long
    dim as long nTotalLines = 0
    nFilesChanged = 0

    for i as long = 0 to gTTabCtl.GetItemCount - 1
        if gTTabCtl.IsValidTab(i) = false then continue for
        if gTTabCtl.IsFindTab(i) then continue for
        dim as clsDocument ptr pDoc = gTTabCtl.GetpDoc(i)
        if Format_IsFormattableDocument( pDoc ) = false then continue for
        dim as long n = Format_ApplyToDocument( pDoc, -1, -1 )
        if n > 0 then
            nTotalLines += n
            nFilesChanged += 1
        end if
    next

    return nTotalLines
end function


' ========================================================================================
' FORMAT ON ENTER.
'
' Called from SCN_CHARADDED the instant Enter is seen, and BEFORE AttemptAutoInsert, which
' is left completely unchanged: it keeps computing the new line's indent and inserting the
' matching End If / Next exactly as it does today. This reformats only the line the caret
' just LEFT.
'
' THE CONTRACT, and every clause is a fix for a documented editor bug rather than a
' precaution invented here:
'
'   * Only the completed line. Never a line below the caret -- the failure that VS Code's
'     format-on-type path cancels outright rather than filtering, and that cpptools closed
'     as "not planned".
'
'   * NEVER delete a line. clangd sets MaxEmptyLinesToKeep = 1000 on this path with the
'     comment "Never remove lines in response to pressing enter!", because a formatter that
'     collapses a whitespace-only line races the editor's own auto-indent and produces the
'     cursor-jumps-back-a-line bug. So the two line-count rules are forced OFF here, and
'     the caret's own line is never touched.
'
'   * It must be CHEAP. This is the typing hot path -- Roslyn has a filed bug where the VB
'     line-commit handler hangs the editor while typing. One line is lexed, not the file.
'     The block depth it needs comes from the buffer and is the only non-local cost.
' ========================================================================================
sub Format_OnEnterPressed( byval pDoc as clsDocument ptr )
    if gConfig.Format.FormatOnEnter = 0 then exit sub
    if Format_IsFormattableDocument( pDoc ) = false then exit sub

    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    dim as long nCaretPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nCaretPos, 0 )

    ' The line the user just finished is the one ABOVE the caret.
    dim as long nLine = nCaretLine - 1
    if nLine < 0 then exit sub

    dim as long nLS = SciExec( hEdit, SCI_POSITIONFROMLINE, nLine, 0 )
    dim as long nLE = SciExec( hEdit, SCI_GETLINEENDPOSITION, nLine, 0 )
    if nLE <= nLS then exit sub                     ' blank line: nothing to do, and
                                                    ' deleting it is exactly what must not
                                                    ' happen here

    dim as string sLine = Scintilla_GetTextRangeBytes( hEdit, nLS, nLE )
    if len(trim(sLine, any chr(32, 9))) = 0 then exit sub

    dim as FORMAT_RULES rules
    dim as long nIndentWidth, bUseTabs
    Format_GetContext( hEdit, rules, nIndentWidth, bUseTabs )

    ' Force the two line-count rules off -- see the header.
    rules.MaxBlankLines = 0
    rules.BlankLinesAroundProc = -1

    dim as string sWhole = Scintilla_GetTextBytes( hEdit )
    dim as long nDepth = Format_DepthAtLine( sWhole, nLine )

    dim as long bChanged = false
    dim as string sOut = Format_TextAtDepth( sLine, rules, nDepth, bChanged, nIndentWidth, bUseTabs )
    if bChanged = false then exit sub
    if len(trim(sOut, any chr(32, 9))) = 0 then exit sub       ' never empty a line

    ' The caret is on the NEXT line, so this edit is entirely behind it and its line and
    ' column are both unaffected -- but the position is not, which is why nothing here
    ' saves or restores a raw position.
    dim as long nCaretCol = SciExec( hEdit, SCI_GETCOLUMN, nCaretPos, 0 )

    SciExec( hEdit, SCI_BEGINUNDOACTION, 0, 0 )
    SciExec( hEdit, SCI_SETTARGETSTART, nLS, 0 )
    SciExec( hEdit, SCI_SETTARGETEND, nLE, 0 )
    SciExec( hEdit, SCI_REPLACETARGET, len(sOut), cast(LPARAM, strptr(sOut)) )
    SciExec( hEdit, SCI_ENDUNDOACTION, 0, 0 )

    dim as long nNewPos = SciExec( hEdit, SCI_FINDCOLUMN, nCaretLine, nCaretCol )
    dim as long nEnd = SciExec( hEdit, SCI_GETLINEENDPOSITION, nCaretLine, 0 )
    if nNewPos > nEnd then nNewPos = nEnd
    SciExec( hEdit, SCI_GOTOPOS, nNewPos, 0 )
end sub


' ========================================================================================
' FORMAT ON PASTE.
'
' Ships OFF. A paste can produce a large and surprising diff, and pasted code is usually a
' fragment whose indent context is ambiguous -- which is why VS Code defaults this off too
' and why ReSharper's equivalent does indentation only.
'
' Reformats ONLY the lines the paste landed on, using the block depth at the insertion
' point. The whole thing -- the paste and the format -- is one undo action, so Ctrl+Z
' removes the pasted text rather than leaving it behind reformatted.
' ========================================================================================
sub Format_OnPaste( byval pDoc as clsDocument ptr, byval nPosBefore as long, byval nLenBefore as long )
    if gConfig.Format.FormatOnPaste = 0 then exit sub
    if Format_IsFormattableDocument( pDoc ) = false then exit sub

    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    dim as long nLenAfter = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    if nLenAfter <= nLenBefore then exit sub          ' nothing was inserted

    dim as long nFirst = SciExec( hEdit, SCI_LINEFROMPOSITION, nPosBefore, 0 )
    dim as long nLast = SciExec( hEdit, SCI_LINEFROMPOSITION, nPosBefore + (nLenAfter - nLenBefore), 0 )
    if nLast < nFirst then exit sub

    Format_ApplyToDocument( pDoc, nFirst, nLast )
end sub


' ========================================================================================
' ---- COMMAND HANDLERS ------------------------------------------------------------------
' ========================================================================================

function OnCommand_FormatDocument() as LRESULT
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if Format_IsFormattableDocument( pDoc ) = false then
        TikoMsgBox( HWND_FRMMAIN, L(471,"This document cannot be formatted. The code formatter works on FreeBASIC source files (.bas, .bi, .inc) that are not read-only."), _
                    L(465,"Format"), TMB_ICON_INFO, TMB_OK )
        return 0
    end if
    dim as long n = Format_Document()
    
    if n = 0 then
        ' Say so rather than doing nothing visible: silence is indistinguishable from a
        ' command that failed, and "already formatted" is the common case on a second run.
        TikoMsgBox( HWND_FRMMAIN, L(472,"No changes were needed -- this document already matches your formatting rules."), L(465,"Format"), TMB_ICON_INFO, TMB_OK )
    end if
    return 0
end function


function OnCommand_FormatSelection() as LRESULT
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if Format_IsFormattableDocument( pDoc ) = false then return 0
    Format_Selection()
    
    return 0
end function


function OnCommand_FormatAllOpenDocuments() as LRESULT
    dim as long nFiles = 0
    dim as long nLines = Format_AllOpenDocuments( nFiles )
    

    dim as string sMsg = L(421,"Formatting complete.") & vbcrlf & vbcrlf & _
                         L(423,"Documents changed:") & " " & str(nFiles) & vbcrlf & _
                         L(424,"Lines changed:") & " " & str(nLines) & vbcrlf & vbcrlf & _
                         L(425,"Each document can be undone individually with Ctrl+Z.")
    TikoMsgBox( HWND_FRMMAIN, sMsg, L(465,"Format"), TMB_ICON_INFO, TMB_OK )
    return 0
end function


' ========================================================================================
' FORMAT PROJECT.
'
' NOTHING HERE IS WRITTEN TO DISK, AND THAT IS NOT A LIMITATION -- IT FALLS OUT OF TIKO'S
' WORKSPACE MODEL. This command was designed expecting to meet project files that were not
' open, read and rewrite them directly, and leave a .bak beside each because a file that
' was never open has no undo. None of that is reachable: gApp.pDocList is "single linked
' list of LOADED files" (clsApp.bi) and it is exactly what clsConfig.ProjectSaveToFile
' walks to write the project's file records. The project's file set IS the loaded document
' list. There is no closed project file to open, so there is no unrecoverable write to
' protect against, and a .bak would be a backup of nothing.
'
' So every file goes through the ordinary undoable buffer path and Ctrl+Z works on all of
' them. What this command adds over Format All Open Documents is real but narrow: it walks
' the DOCUMENT list rather than the tab list, so it also reaches documents that are loaded
' in the project but whose tab the user has closed -- and in this workspace model closing
' a tab never removes a file.
'
' A file whose format is REFUSED by the engine's safety checks is silently left alone,
' which is right -- but it is counted and reported separately, because "0 changed" and
' "3 unchanged" mean very different things to somebody deciding whether to trust this
' command a second time.
' ========================================================================================
function OnCommand_FormatProject() as LRESULT

    ' Count first, so the confirmation can say how many files it is about to touch. A
    ' prompt that cannot tell you the size of what you are agreeing to is not a prompt.
    dim as long nCandidates = 0
    dim as clsDocument ptr pDoc = gApp.pDocList
    while pDoc
        if Format_IsFormattableDocument( pDoc ) then nCandidates += 1
        pDoc = pDoc->pDocNext
    wend

    if nCandidates = 0 then
        TikoMsgBox( HWND_FRMMAIN, L(426,"There are no FreeBASIC source files in this workspace that can be formatted."), _
                    L(465,"Format"), TMB_ICON_INFO, TMB_OK )
        return 0
    end if

    dim as string sAsk = L(427,"Format every FreeBASIC file in this workspace?") & " (" & str(nCandidates) & ")" & vbcrlf & vbcrlf & _
                         L(428,"Nothing is written to disk. Every file is changed in the editor only, so each one can be undone with Ctrl+Z and you decide what to save.")
    if TikoMsgBox( HWND_FRMMAIN, sAsk, L(465,"Format"), TMB_ICON_QUESTION, TMB_YESNO ) <> IDYES then
        return 0
    end if

    dim as long nChanged = 0, nUnchanged = 0, nLines = 0

    ' Walk the DOCUMENT list, not the tab list -- that is the whole difference between this
    ' command and Format All Open Documents.
    pDoc = gApp.pDocList
    while pDoc
        dim as clsDocument ptr pNext = pDoc->pDocNext      ' captured first: formatting must
                                                           ' not be able to strand the walk
        if Format_IsFormattableDocument( pDoc ) then
            dim as long n = Format_ApplyToDocument( pDoc, -1, -1 )
            if n > 0 then
                nChanged += 1
                nLines += n
            else
                nUnchanged += 1
            end if
        end if
        pDoc = pNext
    wend

    dim as string sMsg = L(421,"Formatting complete.") & vbcrlf & vbcrlf & _
                         L(423,"Documents changed:") & " " & str(nChanged) & vbcrlf & _
                         L(424,"Lines changed:") & " " & str(nLines) & vbcrlf & _
                         L(430,"Already formatted:") & " " & str(nUnchanged) & vbcrlf & vbcrlf & _
                         L(425,"Each document can be undone individually with Ctrl+Z.")

    TikoMsgBox( HWND_FRMMAIN, sMsg, L(465,"Format"), TMB_ICON_INFO, TMB_OK )
    return 0
end function


' ========================================================================================
' ENV-GATED SELF-TEST -- TIKO_FORMAT_SELFTEST=1 (the same switch as the engine's).
'
' This half exists for ONE thing the pure engine suite structurally cannot cover: the
' localized strings. L(id,"default") DISCARDS its default -- it is #Define L(e,s) LL(e) --
' so an id with no entry in english.lang renders as an EMPTY STRING. A menu item, a whole
' confirmation prompt or a message box then comes up BLANK, with nothing in the build, the
' warnings or any other assertion to say so.
'
' What it still cannot reach: everything Scintilla. No caret is restored here, no marker is
' checked, no undo step is taken, and no command is dispatched. That evidence is the
' author's interactive pass.
' ========================================================================================
sub Format_RunApplySelfTest()

    if environ("TIKO_FORMAT_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0

    #macro CHECK( _cond_, _name_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            dim as string sFailMsg = "  FAIL: " & _name_
            print sFailMsg
        end if
    #endmacro

    print "Format apply self-test (localization)"

    ' THE IDS ARE THE ONES THIS FILE ACTUALLY CALLS -- 421..430 (the completion and
    ' progress strings) and 465..472 (the whole Format menu group plus its two message
    ' boxes). They were 589..640 until the id block was renumbered DOWN BY 168; the list
    ' here was not renumbered with it, so every assertion below was reading LL() PAST ITS
    ' UPPER BOUND of 521 and grading itself on heap garbage. Hence LangIdIsSet, which
    ' bounds-checks: a stale id now fails every time instead of once in a while.
    dim as long ids(0 to 15) = { 421, 423, 424, 425, 426, 427, 428, 430, _
                                 465, 466, 467, 468, 469, 470, 471, 472 }
    for i as long = 0 to 15
        dim as string sName = "lang id " & str(ids(i)) & " is not blank"
        CHECK( LangIdIsSet(ids(i)), sName )
    next

    ' AND WHAT THEY RESOLVED TO, printed rather than asserted. This is NOT decoration:
    ' "id is not blank" passes under EVERY language file even when that language has the
    ' slot empty, because LoadLocalizationFile fills a blank key with the English text. So
    ' the assertions above cannot tell a real translation from the English fallback, and
    ' running under french.lang and seeing them green proves only that the ids exist.
    ' Reading this line is what distinguishes the two.
    dim as string sShow = "  resolved: 465=" & LL(465) & " | 466=" & LL(466) & " | 430=" & LL(430)
    print sShow

    dim as string sReport = "Format apply self-test: " & str(nPass) & " passed, " & str(nFail) & " failed"
    print sReport
end sub
