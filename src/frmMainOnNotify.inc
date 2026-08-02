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

#include once "clsApp.bi"
#include once "clsDocument.bi"
#include once "frmMain.bi"
#include once "frmMainOnNotify.bi"


' ========================================================================================
' Handle Character Autocompletion if that editor option is active
' ========================================================================================
function frmMain_InsertMatchedChars( _
            byval pDoc as clsDocument ptr, _
            byval ch as long _
            ) as boolean
    
    if gConfig.CharacterAutoComplete = 0 then exit function
    
    if pDoc = 0 then exit function
    
    dim as any ptr pSci = pDoc->GetActiveScintillaPtr()
    if pSci = 0 then exit function
    
    dim as long caretPos = SciMsg( pSci, SCI_GETCURRENTPOS, 0, 0)
    dim as long docStart = caretPos = 1
    dim as long docend   = caretPos = SciMsg( pSci, SCI_GETTEXTLENGTH, 0, 0)

    ' Get the styling of the current line to determine if we are in a 
    ' multiline or single line comment block then abort the autoinsert.
    select case SciMsg( pSci, SCI_GETSTYLEAT, caretPos, 0)
        case SCE_B_MULTILINECOMMENT, SCE_B_COMMENT
            exit function
    end select 

    dim as long nCharPrev = iif( docStart, _
                        SciMsg( pSci, SCI_GETCHARAT, caretPos, 0), _
                        SciMsg( pSci, SCI_GETCHARAT, caretPos-2, 0) )
    
    dim as long nCharNext = SciMsg( pSci, SCI_GETCHARAT, caretPos, 0)

    dim as boolean isCharPrevBlank = iif( instr(chr(nCharPrev), any chr(32, 9, 10, 13)), true, false )

    dim as boolean isCharNextBlank = iif( instr(chr(nCharNext), any chr(32, 9, 10, 13)), true, false ) 
    if nCharNext = docend then isCharNextBlank = true

    dim as boolean isEnclosed = false
    if (nCharPrev = asc("(") and nCharNext = asc(")") ) or _
        (nCharPrev = asc("{") and nCharNext = asc("}") ) or _
        (nCharPrev = asc("[") and nCharNext = asc("]") ) then
        isEnclosed = true
    end if                      
                            
    dim as boolean isSpaceEnclosed = false
    if (nCharPrev = asc("(") and isCharNextBlank ) or _
        (isCharPrevBlank and nCharNext = asc(")") ) or _
        (nCharPrev = asc("{") and isCharNextBlank)  or _
        (isCharPrevBlank and nCharNext = asc("}") ) or _
        (nCharPrev = asc("[") and isCharNextBlank)  or _
        (isCharPrevBlank and nCharNext = asc("]") ) then
        isSpaceEnclosed = true
    end if   

    dim as boolean isCharOrString = false
    if (isCharPrevBlank and isCharNextBlank) or isEnclosed or isSpaceEnclosed then
        isCharOrString = true
    end if

    dim as boolean charNextIsCharOrString = iif( instr(chr(nCharNext), any chr(34,92)), true, false )

    dim as wstring * 10 wszText
        
    select case chr(ch)
        case "("
            if (charNextIsCharOrString) then return false
            wszText = ")"
            SciMsg( pSci, SCI_INSERTTEXT, caretPos, cast(LPARAM, @wszText) )
            
        case "{"
            if (charNextIsCharOrString) then return false
            wszText = "}"
            SciMsg( pSci, SCI_INSERTTEXT, caretPos, cast(LPARAM, @wszText) )
            
        case "["
            if (charNextIsCharOrString) then return false
            wszText = "]"
            SciMsg( pSci, SCI_INSERTTEXT, caretPos, cast(LPARAM, @wszText) )
            
        case chr(34)
            '// 0x22 = "
            if (nCharPrev = 34) and (nCharNext = 34) then
            SciMsg( pSci, SCI_DELETERANGE, caretPos, 1)
            SciMsg( pSci, SCI_GOTOPOS, caretPos, 0)
            return false
            end if

            if (isCharOrString) then
            wszText = chr(34)
            SciMsg( pSci, SCI_INSERTTEXT, caretPos, cast(LPARAM, @wszText) )
            end if    
        
    end select 

    function = true
end function



' ========================================================================================
' Brace matching 
' ========================================================================================

type BRACE_INFO
    found    as boolean = false
    position as integer = -1
    chrcode  as integer = 0
end type

' check if character is an opening brace
function is_open_brace(byval ch as integer) as boolean
    return iif(instr("([{", chr(ch)), true, false) 
end function

' check if character is a closing brace
function is_close_brace(byval ch as integer) as boolean
    return iif(instr(")]}", chr(ch)), true, false) 
end function

' get matching brace character
function get_matching_brace(byval ch as integer) as integer
    dim as string brace
    select case chr(ch)
        case "(": brace = ")"
        case ")": brace = "("
        case "[": brace = "]"
        case "]": brace = "["
        case "{": brace = "}"
        case "}": brace = "{"
    end select
    return asc(brace)
end function

function style_is_comment_or_string( byval hwnd as HWND, byval npos as integer ) as boolean
    dim as integer nstyle = SendMessage(hwnd, SCI_GETSTYLEAT, npos, 0)
    select case nstyle
        case SCE_B_COMMENT, SCE_B_MULTILINECOMMENT, SCE_B_STRING
            return true
        case else
            return false
    end select
    return false
end function

function SetBraceHighlights( byval hwnd as HWND, byval startpos as integer, byval endpos as integer ) as long
    if endpos >= 0 then
        ' highlight matching braces (even if on different lines)
        SciExec( hwnd, SCI_INDICSETFORE, 9, ghEditor.ForeColorBracegood )
        SciExec( hwnd, SCI_BRACEHIGHLIGHT, startpos, endpos )
    else
        ' highlight unmatched brace
        SciExec( hwnd, SCI_INDICSETFORE, 9, ghEditor.ForeColorBracebad ) 
        SciExec( hwnd, SCI_BRACEBADLIGHT, startpos, 0 )
    end if

    function = 0
end function  


function find_starting_brace( byval hwnd as HWND, byval current_pos as long ) as BRACE_INFO
    dim as integer current_line = SendMessage(hwnd, SCI_LINEFROMPOSITION, current_pos, 0)
    dim as integer line_start   = SendMessage(hwnd, SCI_POSITIONFROMLINE, current_line, 0)
    dim as BRACE_INFO brace

    ' There are 3 different brace types (round, square, curly). Iterate from current
    ' position to the start of the line. For every ending brace increment the push
    ' state of that brace type. When we encounter an opening brace, if the push state
    ' is zero then we've found the starting brace.
    dim as integer round_stack = 0
    dim as integer square_stack = 0
    dim as integer curly_stack = 0
    dim as boolean exit_search = false

    dim as integer npos = current_pos - 1
    
    do while npos >= 0

        if style_is_comment_or_string(hwnd, npos) then 
            npos -= 1: continue do
        end if

        ' If we make it to the start of the line then we should examine the previous
        ' line to see if it has a line continuation character "_". If it does, then
        ' load that line and continue searching.
        dim as integer ch = SendMessage(hwnd, SCI_GETCHARAT, npos, 0)
        
        if (npos < line_start) then
            ' skip whitespace, tab, cr, lf
            if (ch = 32) orelse (ch = 9) orelse (ch = 10) orelse (ch = 13) then
                npos -= 1: continue do
            end if
            if ch <> 95 then exit do   ' first char is not an underscore
            if ch = 95 then            ' we have found underscore at end of the line
                current_line = SendMessage(hwnd, SCI_LINEFROMPOSITION, npos, 0)
                line_start   = SendMessage(hwnd, SCI_POSITIONFROMLINE, current_line, 0)
                npos -= 1: continue do
            end if
        end if
        
        select case chr(ch)
            case "(": round_stack  -= 1
            case ")": round_stack  += 1
            case "[": square_stack -= 1
            case "]": square_stack += 1
            case "{": curly_stack  -= 1
            case "}": curly_stack  += 1
        end select

        ' Check if we have a good match on one of the brace types
        if (round_stack < 0) orelse (square_stack < 0) orelse (curly_stack < 0) then
            brace.found = true: brace.chrcode = ch: brace.position = npos
            exit do
        end if
        
        npos -= 1
        
    loop

    return brace
end function


function find_matching_brace( byval hwnd as HWND, byval npos as integer, byval chrcode as integer ) as BRACE_INFO
    dim as integer text_length = SendMessage(hwnd, SCI_GETLENGTH, 0, 0)
    dim as integer matching_char = get_matching_brace(chrcode)
    dim as integer depth = 1
    dim as integer direction = iif(is_open_brace(chrcode), 1, -1)
    dim as integer search_pos = npos + direction
    dim as BRACE_INFO match_brace

    while (search_pos >= 0) andalso (search_pos < text_length)
        ' Skip if in comment/string
        if style_is_comment_or_string(hwnd, search_pos) = false then
            dim as byte ch = SendMessage(hwnd, SCI_GETCHARAT, search_pos, 0)
            
            if ch = chrcode then
                depth += 1
            elseif ch = matching_char then
                depth -= 1
                if depth = 0 then
                    match_brace.found = true
                    match_brace.position = search_pos
                    exit while
                end if
            end if
            
        end if    
        search_pos += direction
    wend
    
    return match_brace
end function


function frmMain_DoBraceMatching( byval hwnd as HWND ) as long
    if gConfig.BraceHighlight = false then
        SciExec( hwnd, SCI_BRACEHIGHLIGHT, -1, -1 )
        return false
    end if         

    dim as integer current_pos  = SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0)
    dim as integer current_line = SendMessage(hwnd, SCI_LINEFROMPOSITION, current_pos, 0)
    dim as integer line_start   = SendMessage(hwnd, SCI_POSITIONFROMLINE, current_line, 0)
    
    ' clear previous brace highlighting
    SendMessage(hwnd, SCI_BRACEHIGHLIGHT, -1, -1)

    ' Scan left from current position to start of line until an unbalanced
    ' open brace is found.
    dim as BRACE_INFO open_brace
    dim as BRACE_INFO match_brace
    
    open_brace = find_starting_brace( hwnd, current_pos )
    
    if open_brace.found then
        match_brace = find_matching_brace( hwnd, open_brace.position, open_brace.chrcode )
        SetBraceHighlights( hwnd, open_brace.position, match_brace.position )
    end if

    ' also check if caret is adjacent to a brace (common case)
    if current_pos > 0 then
        if style_is_comment_or_string(hwnd, current_pos) = false then
            dim as integer ch_before = SendMessage(hwnd, SCI_GETCHARAT, current_pos - 1, 0)
            if is_open_brace(ch_before) orelse is_close_brace(ch_before) then
                match_brace = find_matching_brace(hwnd, current_pos - 1, ch_before)
                SetBraceHighlights( hwnd, current_pos - 1, match_brace.position )
            end if
        end if    
    end if
    
    ' check character at current position
    if style_is_comment_or_string(hwnd, current_pos) = false then
        dim as integer ch_at = SendMessage(hwnd, SCI_GETCHARAT, current_pos, 0)
        if is_open_brace(ch_at) orelse is_close_brace(ch_at) then
            match_brace = find_matching_brace(hwnd, current_pos, ch_at)
            SetBraceHighlights( hwnd, current_pos, match_brace.position )
        end if
    end if
    
    function = 0
end function



' ========================================================================================
' Process Scintilla Notifications
' ========================================================================================
function Scintilla_OnNotify( _
            byval hwnd as HWND, _
            byval pNSC as SCNOTIFICATION ptr _
            ) as long

    if pNSC = 0 then exit function
    
    dim pDoc as clsDocument ptr 

    dim as HWND hEdit
    dim as long nLine, nFoldLevel

    select case pNSC->hdr.code
            
        case SCN_UPDATEUI    
            if gApp.SuppressNotify then return true
            pDoc = gApp.GetDocumentPtrByWindow( pNSC->hdr.hwndFrom )
            if pDoc then 
                if pDoc->IsValidScintillaID( pNSC->hdr.idFrom ) then
                    frmMain_SetStatusbar
                                    
                    ' Update the vertical scrollbars because mousewheel may have been
                    ' used to scroll a split window without keyboard focus.
                    ' (SetRange inside re-derives and repaints.)
                    if pDoc->EditorSplitMode <> SplitNone then
                        frmEditorVScroll_UpdateScrollBars(pDoc)
                    end if

                    ' Keep any open code tip honest: dismiss it when the caret has left
                    ' the call, follow it when the view scrolls. DELIBERATELY OUTSIDE the
                    ' caret-changed guard below -- a wheel scroll moves where the tip
                    ' belongs without moving the caret at all.
                    Codetip_SyncToCaret( pDoc )

                    ' Has the caret changed position
                    var caretPos = SciExec( pDoc->hWndActiveScintilla, SCI_GETCURRENTPOS, 0, 0)

                    if pDoc->lastCaretPos <> caretPos then
                        pDoc->lastCaretPos = caretPos

                        ' Update any occurrence highlights
                        if gConfig.OccurrenceHighlight then
                            static as string sCurWord, sPrevWord
                            sCurWord = pDoc->GetWord()
                            if sCurWord <> sPrevWord then
                                frmMain_HighlightWord( pDoc, sCurWord )
                                sPrevWord = sCurWord
                            end if
                        end if
                        
                        ' We are typing in the editor so ensure that no active index
                        ' or focus remains on the menubar
                        if PsMenuBar_GetActiveIndex( HWND_FRMMAIN_MENUBAR ) <> -1 then
                            PsMenuBar_SetActiveIndex( HWND_FRMMAIN_MENUBAR, -1 )
                        end if

                        ' Update any brace highlighting
                        frmMain_DoBraceMatching( pDoc->hWndActiveScintilla )

                        ' The navigation-history edit debounce: the caret has moved, so if
                        ' it has now LEFT the line that was being edited, that edit location
                        ' is finished and becomes a history entry. This is what makes a run
                        ' of typing collapse to one entry instead of one per keystroke.
                        NavHistory_CheckPendingEdit( pDoc )
                    end if
                end if
            end if
            return true
            
        
        case SCN_MODIFYATTEMPTRO
            ' Attempting to modify a read-only document.
            MessageBeep(MB_ICONWARNING)
                    
                    
        case SCN_MODIFIED
            if gApp.SuppressNotify then return true

            ' Show line and column. Only do on modification of text otherwise we will have
            ' a huge slowdown when notifications sent for UI updates.
            
            if ( pNSC->modificationType and SC_MOD_INSERTTEXT) orelse _
                    (pNSC->modificationType and SC_MOD_DELETETEXT) then

                ' Set parsing flag to true. We use a parse flag rather than depending on the dirty
                ' flags because the file could stay dirty for a long time before it gets saved and 
                ' every call to the parser would happen even in cases where only directional 
                ' movement has occured with no text modifications at all.
               
                ' Get the pDoc from the Scintilla HWND because this is the most reliable
                ' method of knowing which scintilla window sent the notification (rather than
                ' using gTTabCtl.GetActiveDocumentPtr. Possible that a control is sending the message
                ' as it is loading and is not yet the active document ptr.
                pDoc = gApp.GetDocumentPtrByWindow( pNSC->hdr.hwndFrom )
                if pDoc then 

                    frmMain_SetStatusbar
                            
                    ' If text was pasted ensure that the scrollbars are updated because it could be the 
                    ' situation where there no scrollbars (new file) and the amount of text pasted would 
                    ' overflow the currtent page linecount thereby necessitating the scrollbars. 
                    dim as long nShowScrollbar = iif( pDoc->GetLineCount > pDoc->LinesPerPage(0), SW_SHOW, SW_HIDE )
                    if (IsWindowVisible(HWND_FRMEDITOR_VSCROLLBAR(0)) = 0) andalso (nShowScrollbar = SW_SHOW) then
                        ShowWindow(HWND_FRMEDITOR_VSCROLLBAR(0), SW_SHOW)
                        AfxRedrawWindow(HWND_FRMEDITOR_VSCROLLBAR(0))
                    elseif (IsWindowVisible(HWND_FRMEDITOR_VSCROLLBAR(0))) andalso (nShowScrollbar = SW_HIDE) then    
                        ShowWindow(HWND_FRMEDITOR_VSCROLLBAR(0), SW_HIDE)
                    end if


                    ' Restart the background-parse debounce timer (same id restarts it)
                    if (gApp.IsProjectLoading = false) andalso (gApp.IsFileLoading = false) then
                        SetTimer( HWND_FRMMAIN, IDT_PARSER_DEBOUNCE, PARSER_DEBOUNCE_MS, 0 )
                    end if

                    ' Stamp this as the pending edit location for the navigation history.
                    ' Nothing is appended yet -- SCN_UPDATEUI appends it once the caret
                    ' leaves this line, so typing here costs no array work.
                    NavHistory_NoteEdit( pDoc )
                    if (pNSC->modificationType and SC_MOD_DELETETEXT) then
                        hEdit = pDoc->hWndActiveScintilla
                        if frmAutoComplete_IsOpen() then
                            ' The popup is up and the user backspaced: narrow it against
                            ' the shortened word, or close it if they backed out of the
                            ' word entirely. This is what Scintilla's SCN_AUTOCCHARDELETED
                            ' used to tell us. Safe to do inline -- SC_MOD_DELETETEXT
                            ' fires AFTER the text is gone, and re-filtering only touches
                            ' our own window.
                            UpdateAutocompleteFilter()
                        else
                            ' Do a check to see if we have backspaced to a point where there is a
                            ' period to try to popup an autocomplete or codetip.
                            dim as long nPos, nChar
                            nPos  = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 ) - 1
                            nChar = SciExec( hEdit, SCI_GETCHARAT, nPos, 0 )
                            select case chr(nChar)
                                case ".", ">"    '  Show autocomplete list for TYPE variables
                                    ' dot "." or ">"  part of a pointer. Still posted rather
                                    ' than called: showing a window from inside a Scintilla
                                    ' notification is asking for re-entrancy.
                                    pDoc->AutoCTriggerStartPos = nPos
                                    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
                                    PostMessage( HWND_FRMMAIN, MSG_USER_SHOWAUTOCOMPLETE, AUTOCOMPLETE_NOTIFY_NONE, 0 )
                            end select
                        end if
                    end if
                end if
            end if
            
            
        ' Hovering a variable while the debugger is stopped shows its value. Scintilla's
        ' dwell timer does the timing, so there is no timer of our own here; the datatip
        ' itself is a hand-driven PsTooltip that is off the shared dwell clock.
        case SCN_DWELLSTART
            pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then frmDebug_ShowDataTip( pDoc->hWndActiveScintilla, pNSC->position )

        case SCN_DWELLEND
            frmDebug_HideDataTip()

        case SCN_MARGINCLICK
            ' Folder margin
            pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then 
                hEdit      = pDoc->hWndActiveScintilla
                nLine      = SciExec(hEdit, SCI_LINEFROMPOSITION, pNSC->position, 0)
                nFoldLevel = SciExec(hEdit, SCI_GETFOLDLEVEL, nLine, 0)
                select case pNSC->margin 
                    case 1   ' left margin (Toggle BreakPoint or Bookmark)
                        if gConfig.ClickToggleBreakpoint <> 0 then
                            pDoc->ToggleBreakPoint(nLine)
                        else
                            pDoc->ToggleBookmark(nLine)
                        end if        
                    case 2   ' fold margin
                        ' if is the head line...
                        if (nFoldLevel and SC_FOLDLEVELHEADERFLAG) <> 0 then
                            pDoc->FoldToggle(nLine) 
                        end if   
                end select 
            end if           
                        

        ' (The SCN_AUTOCCANCELLED / SCN_AUTOCSELECTION / SCN_AUTOCCHARDELETED handlers are
        '  gone with Scintilla's autocomplete listbox. Their work now lives in
        '  frmAutoComplete: cancellation in frmAutoComplete_Cancel, the insertion in
        '  frmAutoComplete_Commit, and the character-deleted rebuild in
        '  UpdateAutocompleteFilter, driven from the SC_MOD_DELETETEXT branch above.)


        case SCN_CHARADDED
            if gApp.SuppressNotify then exit function
            pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc = 0 then exit function
            hEdit = pDoc->hWndActiveScintilla
        
            ' Restart the background-parse debounce timer (same id restarts it)
            SetTimer( HWND_FRMMAIN, IDT_PARSER_DEBOUNCE, PARSER_DEBOUNCE_MS, 0 )

            ' Attempt to do a Character Autocompletion if that editor option is active.
            frmMain_InsertMatchedChars( pDoc, pNSC->ch )
            
            select case chr(pNSC->ch)
                case ".", ">"    '  Show autocomplete list for TYPE variables
                    ' dot "." or ">"  part of a pointer. Any popup already up belongs to
                    ' the word BEFORE the dot, so close it and start the member list from
                    ' scratch. Still posted rather than called: showing a window from
                    ' inside a Scintilla notification is asking for re-entrancy.
                    frmAutoComplete_Cancel()
                    pDoc->AutoCTriggerStartPos = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0)
                    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
                    if PostMessage(HWND_FRMMAIN, MSG_USER_SHOWAUTOCOMPLETE, AUTOCOMPLETE_NOTIFY_NONE, 0) then exit function

                case ")"      ' Close and active code tip
                    frmAutoComplete_Cancel()
                    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
                    ' Was SCI_CALLTIPCANCEL. The tip is a tiko-owned PsTooltip now, so
                    ' this is the only thing that closes it on ")".
                    Codetip_Hide()

                case "(", ","      ' Show code tip
                    ' "(" and "=" were Scintilla fillups: they used to ACCEPT the
                    ' highlighted item. They no longer do -- they type through, the popup
                    ' closes, and the codetip for the call takes its place.
                    frmAutoComplete_Cancel()
                    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
                    if PostMessage(HWND_FRMMAIN, MSG_USER_SHOWAUTOCOMPLETE, AUTOCOMPLETE_NOTIFY_NONE, 0) then exit function

                case chr(13), chr(10)  ' ENTER KEY PRESSED
                    ' Scintilla notifies one SCN_CHARADDED per EOL character, so a CRLF
                    ' document reports chr(13) then chr(10), while an LF document (EOL
                    ' mode is auto-detected from the file) reports only chr(10). Act on
                    ' the chr(10) only in LF mode, otherwise CRLF would insert twice.
                    if chr(pNSC->ch) = chr(10) then
                        if SciExec( hEdit, SCI_GETEOLMODE, 0, 0 ) <> SC_EOL_LF then exit function
                    end if
                    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
                    AttemptAutoInsert()
                    
                case chr(32)  ' Space key pressed (Insert AutoComplete constructs if applicable)
                    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
                    if ShowAutocompleteList() then exit function

                case else   ' all other letters
                    ' Only keep an ALREADY-OPEN popup in sync with continued typing.
                    ' Ordinary typing never opens a new popup: completion triggers
                    ' only on ".", "->" (member) and the keyword contexts handled on
                    ' space (" as ", " new ", constructor/destructor/extends).
                    ' Narrowing filters the set already in the popup rather than going
                    ' back to gSymDb for it.
                    if frmAutoComplete_IsOpen() then
                        if UpdateAutocompleteFilter() then exit function
                    end if

            end select 
            
    end select 

    function = 0
end Function
   

' ========================================================================================
' Process WM_NOTIFY message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnNotify( _
            byval hwnd as HWND, _
            byval id as long, _
            byval pNMHDR as NMHDR ptr _
            ) as LRESULT

    ' ------------------------------------------------------------------------------------
    ' THE FIND IN PROJECT REMAP, AND IT HAS TO BE HERE -- above the active-document gate
    ' below, and above the SuppressNotify test inside it.
    '
    ' That gate reads gTTabCtl.GetActiveDocumentPtr(), which is NULL while the Find in
    ' Project tab is the active tab -- which is precisely the situation an edit typed into a
    ' result excerpt happens in. Anything placed inside the gate would never run for it.
    '
    ' The document is resolved from the SENDER (GetDocumentPtrByWindow), not from the active
    ' tab, so the attribution is correct no matter which view the edit came from. Every view
    ' bound to a document is one of its watchers, so an edit made in an excerpt still
    ' produces this notification from the document's OWN windows -- which is why the excerpt
    ' views themselves need no notifications at all.
    ' ------------------------------------------------------------------------------------
    if pNMHDR->code = SCN_MODIFIED then
        dim as SCNOTIFICATION ptr pNotify = cast( SCNOTIFICATION ptr, pNMHDR )
        dim as clsDocument ptr pDocMod = gApp.GetDocumentPtrByWindow( pNMHDR->hwndFrom )
        if pDocMod then
            if environ("TIKO_FINDPROJ_TRACE") = "1" then
                dim as string sT = "SCNMOD: from=" & hex(cast(integer, pNMHDR->hwndFrom)) & _
                    " w0=" & hex(cast(integer, pDocMod->hWindow(0))) & _
                    " w1=" & hex(cast(integer, pDocMod->hWindow(1))) & _
                    " type=" & hex(pNotify->modificationType) & _
                    " pos=" & str(pNotify->position) & " len=" & str(pNotify->length)
                print sT
            end if
            ' ONE VIEW ONLY, AND THIS IS NOT AN OPTIMISATION -- IT IS THE CORRECTNESS FIX.
            '
            ' Every document owns TWO Scintilla windows sharing ONE Scintilla document
            ' (clsDocument.CreateScintillaWindows always creates the split view, visible or
            ' not), both children of frmMain and both carrying SC_MOD_INSERTTEXT /
            ' SC_MOD_DELETETEXT in their event mask. So a single edit arrives here TWICE,
            ' identical in type, position and length -- and remapping applies it twice, which
            ' drags every later offset in the file out of place and kills matches that the
            ' edit never touched. Measured: one Replace in the Find in Project tab killed TWO
            ' matches, and the failure is silent -- the buffer is edited correctly, only the
            ' model is wrong, so it shows up as results quietly going missing.
            if pNMHDR->hwndFrom = pDocMod->hWindow(0) then
                FindProject_OnDocumentModified( pDocMod, pNotify->modificationType, _
                                                pNotify->position, pNotify->length, _
                                                pNotify->linesAdded )
            end if
        end if
    end if

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        if pDoc->IsValidScintillaID(id) then
            ' Process Scintilla control notification messages. First, check to see if the notifications
            ' have been suppressed by a bulk modification activity that should only update the screen
            ' at the end of its process (eg. moving large numbers of selected lines up or down).
            if gApp.SuppressNotify then exit function
            Scintilla_OnNotify hwnd, cast(SCNOTIFICATION ptr, pNMHDR)
            exit function
        end if
    end if

    function = 0
end function

