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

''
''  AUTO-INDENT AND BLOCK COMPLETION
''
''  Runs from SCN_CHARADDED the instant ENTER is seen, after Format_OnEnterPressed. Two
''  jobs that the user controls separately:
''
''    gConfig.AutoIndentation - carry the previous line's indentation onto the new line,
''                              one level deeper after a block opener.
''    gConfig.AutoComplete    - also emit the matching closer (End If, Next, Loop, ...).
''
''  With AutoComplete OFF this module INDENTS AND NOTHING ELSE. It never deletes, rewrites
''  or re-wraps a line in that mode. That is a deliberate change from the older code, which
''  restructured the If / Select Case line either way and, on the Select path, destroyed
''  everything to the right of the caret while doing it.
''
''  ---------------------------------------------------------------------------------------
''  WHY THE "IS IT ALREADY CLOSED" TEST IS A SCOPED FORWARD SCAN AND NOT A FILE-WIDE COUNT
''  ---------------------------------------------------------------------------------------
''  The old CanCompleteBlockStatement counted "lines starting with IF" against "lines
''  starting with END IF" across the WHOLE document and inserted whenever starts > ends.
''  That answers "is the file balanced", which is not the question. It got the real question
''  wrong in both directions:
''
''    - one single-line "If x Then y" anywhere in the file inflated the start count, so a
''      block that WAS closed got a second End If;
''    - "EndIf" (one word) was not in its end table at all, so those blocks read as unclosed
''      forever;
''    - a bare "Do" / "While" / "Scope" opener was not counted (the table required a trailing
''      space) even though the caller acted on one, so counter and caller disagreed;
''    - "Type x As Integer" typedefs counted as Type blocks;
''    - lines inside a /' '/ comment block counted as code;
''    - and an imbalance a thousand lines away masked -- or invented -- one right here.
''
''  AutoInsert_NeedsBlockEnd instead walks FORWARD from the line that was just opened,
''  keeping a stack of the blocks it opens and closes on the way, and stops at the first
''  closer that belongs to no block below us. If that closer is ours, the block is already
''  closed and nothing is inserted; if it belongs to an enclosing block (we hit End Sub
''  while hunting for End If) the block is genuinely unclosed. Running off the end of the
''  file also means unclosed. The scan is bounded by that first unmatched closer, so it is
''  normally a few lines rather than the whole document.
''

#include once "modAutoInsert.bi"
#include once "frmFunctions.bi"


' ----------------------------------------------------------------------------------------
' Line kinds. Only OPENER and CLOSER move the block stack; MIDDLE (Else/ElseIf) and CASE are
' called out separately because the two re-alignment paths below need to recognise them.
' ----------------------------------------------------------------------------------------
const AIL_BLANK  = 0
const AIL_PLAIN  = 1
const AIL_OPENER = 2
const AIL_CLOSER = 3
const AIL_MIDDLE = 4
const AIL_CASE   = 5

' How deep a nesting the forward scan will track before it gives up and completes the block.
const AI_MAXNEST = 128


' ----------------------------------------------------------------------------------------
' ONE TABLE, used by BOTH the trigger test and the balance scanner.
'
' That sharing is the point of the table: the old code had the opener vocabulary written out
' by hand in seventeen if-blocks and the closer vocabulary written out again in a select
' case, and the two had already drifted (the counter demanded "DO " with a trailing space
' while the trigger accepted a bare "Do", so a bare Do was acted on but never counted).
'
' wszCloseW2  - second word of the "End xxx" form; "" when the block has no End form.
' wszCloseAlt - one-word closer (Next / Loop / Wend / EndIf); "" when there is none.
' ----------------------------------------------------------------------------------------
type AUTOINSERT_BLOCKDEF
    idBlock     as long
    wszOpen     as zstring * 16    ' opener keyword, UPPERCASE
    wszEnd      as zstring * 20    ' text inserted, in the casing we emit
    wszCloseW2  as zstring * 16    ' UPPERCASE
    wszCloseAlt as zstring * 16    ' UPPERCASE
    bReparse    as long            ' re-scan the symbol database after inserting.
                                   ' Sub and Function only, as before the refactor: this runs
                                   ' on a keystroke and the other openers do not add a symbol
                                   ' the Functions panel shows.
end type

dim shared gAIBlocks( any ) as AUTOINSERT_BLOCKDEF
dim shared gAIBlockCount as long = 0


' ========================================================================================
' Build the block table once. Built at runtime rather than declared as a static initialised
' array: the gas64 backend is known to corrupt static array initialisers (see CLAUDE.md),
' and FMT_BuildVocabulary in modFormat.inc sets the same precedent.
' ========================================================================================
private sub AutoInsert_BuildBlockTable()
    if gAIBlockCount > 0 then exit sub

    redim gAIBlocks( 0 to 16 )
    dim as long n = 0

    #macro AIBLOCK( _id_, _open_, _end_, _cw2_, _alt_, _reparse_ )
        gAIBlocks(n).idBlock     = _id_
        gAIBlocks(n).wszOpen     = _open_
        gAIBlocks(n).wszEnd      = _end_
        gAIBlocks(n).wszCloseW2  = _cw2_
        gAIBlocks(n).wszCloseAlt = _alt_
        gAIBlocks(n).bReparse    = _reparse_
        n += 1
    #endmacro

    '        block id                     opener         inserted end text   End xxx        one-word     reparse
    AIBLOCK( BLOCK_STATEMENT_IF,          "IF",          "end if",           "IF",          "ENDIF",     false )
    AIBLOCK( BLOCK_STATEMENT_FOR,         "FOR",         "next",             "",            "NEXT",      false )
    AIBLOCK( BLOCK_STATEMENT_SELECT,      "SELECT",      "end select",       "SELECT",      "",          false )
    AIBLOCK( BLOCK_STATEMENT_WHILE,       "WHILE",       "wend",             "",            "WEND",      false )
    AIBLOCK( BLOCK_STATEMENT_DO,          "DO",          "loop",             "",            "LOOP",      false )
    AIBLOCK( BLOCK_STATEMENT_FUNCTION,    "FUNCTION",    "end function",     "FUNCTION",    "",          true  )
    AIBLOCK( BLOCK_STATEMENT_SUB,         "SUB",         "end sub",          "SUB",         "",          true  )
    AIBLOCK( BLOCK_STATEMENT_PROPERTY,    "PROPERTY",    "end property",     "PROPERTY",    "",          false )
    AIBLOCK( BLOCK_STATEMENT_OPERATOR,    "OPERATOR",    "end operator",     "OPERATOR",    "",          false )
    AIBLOCK( BLOCK_STATEMENT_TYPE,        "TYPE",        "end type",         "TYPE",        "",          false )
    AIBLOCK( BLOCK_STATEMENT_WITH,        "WITH",        "end with",         "WITH",        "",          false )
    AIBLOCK( BLOCK_STATEMENT_ENUM,        "ENUM",        "end enum",         "ENUM",        "",          false )
    AIBLOCK( BLOCK_STATEMENT_UNION,       "UNION",       "end union",        "UNION",       "",          false )
    AIBLOCK( BLOCK_STATEMENT_CONSTRUCTOR, "CONSTRUCTOR", "end constructor",  "CONSTRUCTOR", "",          false )
    AIBLOCK( BLOCK_STATEMENT_DESTRUCTOR,  "DESTRUCTOR",  "end destructor",   "DESTRUCTOR",  "",          false )
    AIBLOCK( BLOCK_STATEMENT_SCOPE,       "SCOPE",       "end scope",        "SCOPE",       "",          false )
    AIBLOCK( BLOCK_STATEMENT_NAMESPACE,   "NAMESPACE",   "end namespace",    "NAMESPACE",   "",          false )

    #undef AIBLOCK
    gAIBlockCount = n
end sub


' ========================================================================================
' Table lookups. All three return an INDEX into gAIBlocks, or -1.
' ========================================================================================
private function AutoInsert_FindByID( byval idBlock as long ) as long
    for i as long = 0 to gAIBlockCount - 1
        if gAIBlocks(i).idBlock = idBlock then return i
    next
    return -1
end function

private function AutoInsert_FindByOpener( byref sWord as string ) as long
    if len(sWord) = 0 then return -1
    for i as long = 0 to gAIBlockCount - 1
        if gAIBlocks(i).wszOpen = sWord then return i
    next
    return -1
end function

' sW1 is the first word, sW2 the second (both UPPERCASE). Matches either the "End xxx"
' two-word form or a one-word closer.
private function AutoInsert_FindByCloser( byref sW1 as string, byref sW2 as string ) as long
    if sW1 = "END" then
        ' A bare "End" terminates the program; it closes no block.
        if len(sW2) = 0 then return -1
        for i as long = 0 to gAIBlockCount - 1
            if len(gAIBlocks(i).wszCloseW2) = 0 then continue for
            if gAIBlocks(i).wszCloseW2 = sW2 then return i
        next
        return -1
    end if
    for i as long = 0 to gAIBlockCount - 1
        if len(gAIBlocks(i).wszCloseAlt) = 0 then continue for
        if gAIBlocks(i).wszCloseAlt = sW1 then return i
    next
    return -1
end function


' ========================================================================================
' Drop an end-of-line comment, leaving string literals alone.
'
' Byte indexing rather than mid(): this runs over every line the forward scan touches, and
' mid() allocates a one-character string per call.
' ========================================================================================
private function AutoInsert_StripComment( byref sLine as string ) as string
    dim as long n = len(sLine)
    if n = 0 then return sLine

    dim as boolean bInString = false
    for i as long = 0 to n - 1
        dim as ulong ch = sLine[i]
        if bInString then
            ' FB escapes an embedded quote by doubling it, which this handles for free:
            ' the closing quote drops us out and the next one puts us straight back in.
            if ch = asc("""") then bInString = false
        else
            select case ch
                case asc("""")
                    bInString = true
                case asc("'")
                    return left(sLine, i)
                case asc("/")
                    if (i < n - 1) andalso (sLine[i + 1] = asc("'")) then return left(sLine, i)
            end select
        end if
    next
    return sLine
end function


' ========================================================================================
' The leading-whitespace COLUMN of a line, counting a tab as an advance to the next tab
' stop rather than as a flat nIndentSize. The old code added nIndentSize per tab, which is
' only right when the tabs are the whole of the indentation.
' ========================================================================================
private function AutoInsert_IndentColumn( byref sLine as string, _
                                          byval nIndentSize as long ) as long
    dim as long nCol = 0
    dim as long nStep = nIndentSize
    if nStep <= 0 then nStep = 4

    for i as long = 0 to len(sLine) - 1
        select case sLine[i]
            case 32:  nCol += 1
            case 9:   nCol = ((nCol \ nStep) + 1) * nStep
            case else: exit for
        end select
    next
    return nCol
end function


' ========================================================================================
' The leading-whitespace string that reaches a given column, in the user's chosen units.
'
' Replaces the old FillString/NumTabsFromSpaces pair, which had two defects: the conversion
' used "/" (floating-point division in FreeBASIC, so 6 spaces at indent 4 rounded UP to two
' tabs -- measured, 3/2 evaluates to 2), and it silently discarded any remainder. The caret
' was then advanced by a separately-computed count that could disagree with the string that
' was actually inserted. Callers now advance by len() of this string, so they cannot.
' ========================================================================================
private function AutoInsert_Indent( byval nColumn as long, _
                                    byval nIndentSize as long ) as string
    if nColumn <= 0 then return ""
    if gConfig.TabIndentSpaces then return space(nColumn)
    if nIndentSize <= 0 then return space(nColumn)
    return string(nColumn \ nIndentSize, chr(9)) & space(nColumn mod nIndentSize)
end function


' ========================================================================================
' Classify one line of source. idBlock is set for OPENER and CLOSER only.
'
' Every judgement is made on the first word of the statement (plus the second or third where
' "End xxx", a Public prefix or a Type typedef needs it) and never on a word found anywhere
' in the line -- the rule modFormat's FMT_ClassifyLine already follows, and what keeps
' "Dim As Sub() p" from reading as a Sub block and "Exit For" from reading as a For block.
' ========================================================================================
private function AutoInsert_ClassifyLine( byref sLineIn as string, _
                                          byref idBlock as long ) as long
    idBlock = -1

    dim as string sStmt = ucase( trim( AutoInsert_StripComment(sLineIn), any chr(32,9) ) )
    if len(sStmt) = 0 then return AIL_BLANK

    ' A preprocessor directive is never a block for our purposes: #If/#EndIf nest
    ' independently of the code around them and completing one from here would be wrong.
    if sStmt[0] = asc("#") then return AIL_PLAIN

    ' -- word scan: first three words, plus the last one for the "If ... Then" test --------
    dim as string w1, w2, w3, wLast
    dim as long nWords = 0
    dim as long i = 0
    dim as long n = len(sStmt)

    while i < n
        dim as ulong ch = sStmt[i]
        dim as boolean bWordChar = ((ch >= asc("A")) andalso (ch <= asc("Z"))) orelse _
                                   ((ch >= asc("0")) andalso (ch <= asc("9"))) orelse _
                                   (ch = asc("_")) orelse (ch = asc("$"))
        if bWordChar = false then
            i += 1
            continue while
        end if
        dim as long nStart = i
        while i < n
            ch = sStmt[i]
            dim as boolean bMore = ((ch >= asc("A")) andalso (ch <= asc("Z"))) orelse _
                                   ((ch >= asc("0")) andalso (ch <= asc("9"))) orelse _
                                   (ch = asc("_")) orelse (ch = asc("$"))
            if bMore = false then exit while
            i += 1
        wend
        dim as string sWord = mid(sStmt, nStart + 1, i - nStart)
        nWords += 1
        select case nWords
            case 1: w1 = sWord
            case 2: w2 = sWord
            case 3: w3 = sWord
        end select
        wLast = sWord
    wend

    if nWords = 0 then return AIL_PLAIN

    ' -- statements that name a block keyword but open nothing ---------------------------
    select case w1
        case "DECLARE", "EXIT", "CONTINUE", "GOTO", "GOSUB", "RETURN"
            return AIL_PLAIN
    end select

    ' -- closers -------------------------------------------------------------------------
    dim as long idx = AutoInsert_FindByCloser( w1, w2 )
    if idx >= 0 then
        idBlock = gAIBlocks(idx).idBlock
        return AIL_CLOSER
    end if
    ' "End" followed by something we do not know still closes SOMETHING, and a bare "End"
    ' closes nothing. Either way it is not an opener.
    if w1 = "END" then return AIL_PLAIN

    ' -- middles -------------------------------------------------------------------------
    if (w1 = "ELSE") orelse (w1 = "ELSEIF") then return AIL_MIDDLE
    if w1 = "CASE" then return AIL_CASE

    ' -- Public / Private / Static prefix on a procedure declaration ----------------------
    dim as string wOpen = w1, wThird = w3
    select case w1
        case "PUBLIC", "PRIVATE", "STATIC"
            select case w2
                case "SUB", "FUNCTION", "PROPERTY", "OPERATOR", "CONSTRUCTOR", "DESTRUCTOR"
                    wOpen  = w2
                    wThird = ""
                case else
                    return AIL_PLAIN
            end select
    end select

    ' -- openers -------------------------------------------------------------------------
    idx = AutoInsert_FindByOpener( wOpen )
    if idx < 0 then return AIL_PLAIN

    select case gAIBlocks(idx).idBlock
        case BLOCK_STATEMENT_IF
            ' Only a line ENDING in Then opens a block. "If x Then y" is a complete
            ' statement -- and counting those as openers is what produced the stray End If
            ' the old file-wide count was famous for.
            if wLast <> "THEN" then return AIL_PLAIN

        case BLOCK_STATEMENT_TYPE
            ' "Type name As other" is a typedef, not a block.
            if wThird = "AS" then return AIL_PLAIN

        case BLOCK_STATEMENT_FUNCTION
            ' "Function = expr" assigns the return value; it declares nothing. Test the
            ' text before the first "(" so a defaulted parameter cannot be mistaken for it.
            dim as string sHead = sStmt
            dim as long nParen = instr(sHead, "(")
            if nParen > 0 then sHead = left(sHead, nParen - 1)
            if instr(sHead, "=") > 0 then return AIL_PLAIN
    end select

    idBlock = gAIBlocks(idx).idBlock
    return AIL_OPENER
end function


' ========================================================================================
' Is this line nothing but a comment?
'
' The text test alone cannot see the inside of a /' '/ block, so where Scintilla has already
' styled the line we ask it -- tiko's Lexilla lexer has classified every byte and the answer
' is free. Beyond the styled region (Scintilla lexes lazily) we fall back to the text test,
' which is right for ' comments and merely incomplete for block ones.
' ========================================================================================
private function AutoInsert_LineIsComment( byval hEdit as HWND, _
                                           byval nLine as long, _
                                           byref sLine as string ) as boolean
    dim as string sTrim = ltrim(sLine, any chr(32,9))
    if len(sTrim) = 0 then return false
    if sTrim[0] = asc("'") then return true
    if left(sTrim, 2) = "/'" then return true

    dim as long nStart    = SciExec( hEdit, SCI_POSITIONFROMLINE, nLine, 0 )
    dim as long nEnd      = SciExec( hEdit, SCI_GETLINEENDPOSITION, nLine, 0 )
    dim as long nStyled   = SciExec( hEdit, SCI_GETENDSTYLED, 0, 0 )
    if nStart >= nStyled then return false

    for p as long = nStart to nEnd - 1
        if p >= nStyled then exit for
        dim as long ch = SciExec( hEdit, SCI_GETCHARAT, p, 0 )
        if (ch = 32) orelse (ch = 9) then continue for
        dim as long nStyle = SciExec( hEdit, SCI_GETSTYLEAT, p, 0 )
        return (nStyle = SCE_B_COMMENT) orelse (nStyle = SCE_B_MULTILINECOMMENT)
    next
    return false
end function


' ========================================================================================
' The forward scan's state, and one step of it.
'
' Split out from the document walk deliberately, and this is the ONLY interesting decision
' in the file: the walk needs a live Scintilla and cannot be driven from a self-test, while
' the rule it is applying is pure arithmetic over a stack of block ids. Feeding classified
' lines through AutoInsert_ScanStep is therefore assertable with no document open at all,
' which is what the suite at the bottom of this file does.
'
' Returns AISCAN_CONTINUE / AISCAN_INSERT / AISCAN_SKIP.
' ========================================================================================
const AISCAN_CONTINUE = 0
const AISCAN_INSERT   = 1
const AISCAN_SKIP     = 2

type AUTOINSERT_SCAN
    stack( 0 to AI_MAXNEST - 1 ) as long
    nSP as long
end type

private function AutoInsert_ScanStep( byref st as AUTOINSERT_SCAN, _
                                      byval nKind as long, _
                                      byval idFound as long, _
                                      byval idBlock as long ) as long

    if nKind = AIL_OPENER then
        ' Deeper than we are prepared to reason about: complete the block rather than
        ' guess. See the "cannot tell" rule in this function's caller.
        if st.nSP >= AI_MAXNEST then return AISCAN_INSERT
        st.stack(st.nSP) = idFound
        st.nSP += 1
        return AISCAN_CONTINUE
    end if

    if nKind <> AIL_CLOSER then return AISCAN_CONTINUE

    ' Which of the blocks opened below us does this closer belong to? Search from the
    ' innermost out, so mismatched code unwinds to the nearest plausible owner instead of
    ' derailing the scan.
    for j as long = st.nSP - 1 to 0 step -1
        if st.stack(j) = idFound then
            st.nSP = j
            return AISCAN_CONTINUE
        end if
    next

    ' It belongs to no block below us, so it closes either OUR block or one that encloses
    ' it. Either way the search ends on this line.
    if idFound = idBlock then return AISCAN_SKIP
    return AISCAN_INSERT
end function


' ========================================================================================
' Should a closer be inserted for the block opened on line nOpenLine?
'
' TRUE  - the block is not closed anywhere ahead of it; emit the End.
' FALSE - a matching closer is already there; emit nothing.
'
' See the header of this file for why this is a scoped forward scan. When the code ahead is
' malformed enough that the scan cannot tell -- it runs off the end of the file, or nests
' deeper than AI_MAXNEST -- it answers TRUE: running off the end genuinely does mean nothing
' closes this block, which is the ordinary case of typing at the bottom of a file.
' ========================================================================================
function AutoInsert_NeedsBlockEnd( byval pDoc as clsDocument ptr, _
                                   byval nOpenLine as long, _
                                   byval idBlock as long ) as boolean

    if pDoc = 0 then return false
    dim as boolean bAutoComplete = (gConfig.AutoComplete <> 0)
    if bAutoComplete = false then return false

    AutoInsert_BuildBlockTable()

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as long nLastLine = SciExec( hEdit, SCI_GETLINECOUNT, 0, 0 ) - 1

    dim as AUTOINSERT_SCAN st
    st.nSP = 0

    for i as long = nOpenLine + 1 to nLastLine
        dim as string sLine = pDoc->GetLine(i)
        if len(ltrim(sLine, any chr(32,9))) = 0 then continue for
        if AutoInsert_LineIsComment( hEdit, i, sLine ) then continue for

        dim as long idFound = -1
        dim as long nKind = AutoInsert_ClassifyLine( sLine, idFound )

        select case AutoInsert_ScanStep( st, nKind, idFound, idBlock )
            case AISCAN_INSERT: return true
            case AISCAN_SKIP:   return false
        end select
    next

    ' Ran off the end of the file: nothing closes this block.
    return true
end function


' ========================================================================================
' Walk backwards from nFromLine looking for the opener of idBlock that encloses it, and
' return its indentation column. Returns -1 when there is no such opener.
'
' Depth-aware, unlike the old "search back for the nearest line starting IF " which picked a
' nested block that had already been closed.
' ========================================================================================
private function AutoInsert_FindEnclosingOpenerColumn( byval pDoc as clsDocument ptr, _
                                                       byval hEdit as HWND, _
                                                       byval nFromLine as long, _
                                                       byval idBlock as long, _
                                                       byval nIndentSize as long, _
                                                       byval idStopKind as long, _
                                                       byref nSiblingColumn as long ) as long
    nSiblingColumn = -1

    dim as long nDepth = 0
    for i as long = nFromLine to 0 step -1
        dim as string sLine = pDoc->GetLine(i)
        if len(ltrim(sLine, any chr(32,9))) = 0 then continue for
        if AutoInsert_LineIsComment( hEdit, i, sLine ) then continue for

        dim as long idFound = -1
        dim as long nKind = AutoInsert_ClassifyLine( sLine, idFound )

        if (nKind = AIL_CLOSER) andalso (idFound = idBlock) then
            nDepth += 1
        elseif (nKind = AIL_OPENER) andalso (idFound = idBlock) then
            if nDepth = 0 then return AutoInsert_IndentColumn( sLine, nIndentSize )
            nDepth -= 1
        elseif (nKind = idStopKind) andalso (nDepth = 0) then
            ' The nearest sibling at our own level (a previous Case, say). Remember it but
            ' keep walking, so the caller can still learn where its opener is.
            if nSiblingColumn < 0 then
                nSiblingColumn = AutoInsert_IndentColumn( sLine, nIndentSize )
            end if
        end if
    next
    return -1
end function


' ========================================================================================
' Attempt to auto-indent, and to autocomplete a statement block.
' ========================================================================================
function AttemptAutoInsert() as long

    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0

    AutoInsert_BuildBlockTable()

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as long curPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nLine  = pDoc->GetCurrentLineNumber

    dim as string strCurLine     = pDoc->GetLine(nLine)
    dim as string strPrevLineRaw = pDoc->GetLine(nLine - 1)
    if len(strPrevLineRaw) = 0 then return 0

    ' Tab width and indent size are the same value here -- see pDoc->ApplyProperties.
    dim as long IndentSize = SciExec( hEdit, SCI_GETINDENT, 0, 0 )
    if IndentSize <= 0 then IndentSize = SciExec( hEdit, SCI_GETTABWIDTH, 0, 0 )
    if IndentSize <= 0 then IndentSize = 4

    dim as long nSpaces = AutoInsert_IndentColumn( strPrevLineRaw, IndentSize )

    dim as boolean bAutoIndent   = (gConfig.AutoIndentation <> 0)
    dim as boolean bAutoComplete = (gConfig.AutoComplete <> 0)
    if bAutoIndent = false then
        nSpaces = 0
        IndentSize = 0
    end if

    dim as string sOuter = AutoInsert_Indent( nSpaces, IndentSize )
    dim as string sInner = AutoInsert_Indent( nSpaces + IndentSize, IndentSize )
    dim as string strFill


    ''''''''''''
    ' COMMENTS
    '   Inside a /' '/ block, close it. Single-line ' comments fall through so that ENTER
    '   still parks the caret under the preceding mark.
    dim as string strPrevTrim = ucase( trim(strPrevLineRaw, any chr(32,9)) )
    select case SciExec( hEdit, SCI_GETSTYLEAT, curPos, 0 )
        case SCE_B_MULTILINECOMMENT
            if (left(strPrevTrim, 3) = "/' ") orelse (strPrevTrim = "/'") then
                strFill = sOuter & vbcrlf & sOuter & "'/"
                SciExec( hEdit, SCI_INSERTTEXT, curPos, strptr(strFill) )
                curPos += len(sOuter)
                SciExec( hEdit, SCI_SETSEL, curPos, curPos )
            end if
            return 0
        case SCE_B_COMMENT
            ' fall through
    end select


    ' Classify the line the caret just left. Everything below keys off this instead of the
    ' seventeen hand-written left()/ucase() tests the old file carried.
    dim as long idBlock = -1
    dim as long nKind = AutoInsert_ClassifyLine( strPrevLineRaw, idBlock )


    ''''''''''''
    ' ELSE / ELSEIF
    '   Line the Else up with the If it belongs to.
    if bAutoIndent andalso (nKind = AIL_MIDDLE) then
        dim as long nSibling = -1
        dim as long nCol = AutoInsert_FindEnclosingOpenerColumn( pDoc, hEdit, nLine - 2, _
                                                                 BLOCK_STATEMENT_IF, _
                                                                 IndentSize, -1, nSibling )
        if nCol >= 0 then
            dim as string sAt = AutoInsert_Indent( nCol, IndentSize )
            pDoc->SetLine( nLine - 1, sAt & trim(strPrevLineRaw, any chr(32,9)) )
            dim as string sBody = AutoInsert_Indent( nCol + IndentSize, IndentSize )
            pDoc->SetLine( nLine, sBody & ltrim(strCurLine, any chr(32,9)) )
            curPos = SciExec( hEdit, SCI_POSITIONFROMLINE, nLine, 0 ) + len(sBody)
            SciExec( hEdit, SCI_SETSEL, curPos, curPos )
            return 0
        end if
    end if


    ''''''''''''
    ' CASE / CASE ELSE
    '   Line up with the previous Case if there is one, otherwise with the Select Case that
    '   encloses it, honouring the formatter's IndentCase rule so the two agree.
    if bAutoIndent andalso (nKind = AIL_CASE) then
        dim as long nSibling = -1
        dim as long nCol = AutoInsert_FindEnclosingOpenerColumn( pDoc, hEdit, nLine - 2, _
                                                                 BLOCK_STATEMENT_SELECT, _
                                                                 IndentSize, AIL_CASE, nSibling )
        if nSibling >= 0 then
            nCol = nSibling
        elseif nCol >= 0 then
            if gConfig.Format.IndentCase then nCol += IndentSize
        end if
        if nCol >= 0 then
            dim as string sAt = AutoInsert_Indent( nCol, IndentSize )
            pDoc->SetLine( nLine - 1, sAt & trim(strPrevLineRaw, any chr(32,9)) )
            dim as string sBody = AutoInsert_Indent( nCol + IndentSize, IndentSize )
            pDoc->SetLine( nLine, sBody & ltrim(strCurLine, any chr(32,9)) )
            curPos = SciExec( hEdit, SCI_POSITIONFROMLINE, nLine, 0 ) + len(sBody)
            SciExec( hEdit, SCI_SETSEL, curPos, curPos )
            return 0
        end if
    end if


    ' Not a block opener: carry the previous line's indentation forward and stop.
    if nKind <> AIL_OPENER then
        strFill = sOuter
        if len(strFill) then SciExec( hEdit, SCI_ADDTEXT, len(strFill), strptr(strFill) )
        return 0
    end if

    dim as long idx = AutoInsert_FindByID( idBlock )
    if idx < 0 then return 0

    ' The one question the whole rewrite exists to answer correctly.
    dim as boolean bNeedsEnd = AutoInsert_NeedsBlockEnd( pDoc, nLine - 1, idBlock )


    ''''''''''''
    ' IF / THEN and SELECT CASE
    '   These two rewrite the current line rather than just prefixing it: the text the user
    '   pressed ENTER in front of has to move INSIDE the block being opened. That is only
    '   done when AutoComplete is on -- with it off this module indents and nothing else.
    '
    '   strCurLine is re-emitted on EVERY path here. The old Select handler deleted the line
    '   unconditionally and only put it back inside the "needs an End" branch, so with the
    '   block already closed -- or with AutoComplete simply switched off, which made the old
    '   CanCompleteBlockStatement return false -- everything right of the caret was lost.
    if bAutoComplete andalso (idBlock = BLOCK_STATEMENT_IF) then
        SciExec( hEdit, SCI_LINEDELETE, 0, 0 )
        strFill = sInner & ltrim(strCurLine, any chr(32,9)) & vbcrlf
        if bNeedsEnd then strFill &= sOuter & gAIBlocks(idx).wszEnd & vbcrlf
        SciExec( hEdit, SCI_ADDTEXT, len(strFill), strptr(strFill) )
        curPos += len(sInner)
        SciExec( hEdit, SCI_SETSEL, curPos, curPos )
        return 0
    end if

    if bAutoComplete andalso (idBlock = BLOCK_STATEMENT_SELECT) then
        SciExec( hEdit, SCI_LINEDELETE, 0, 0 )
        strFill = sInner & "case " & ltrim(strCurLine, any chr(32,9)) & vbcrlf
        if bNeedsEnd then strFill &= sOuter & gAIBlocks(idx).wszEnd & vbcrlf
        SciExec( hEdit, SCI_ADDTEXT, len(strFill), strptr(strFill) )
        curPos += len(sInner) + len("case ")
        SciExec( hEdit, SCI_SETSEL, curPos, curPos )
        return 0
    end if


    ''''''''''''
    ' EVERY OTHER OPENER
    '   Indent one level in, and append the closer when the block needs one. FOR is the only
    '   variation: it can carry the loop variable onto the Next.
    strFill = sInner
    if bNeedsEnd then
        dim as string sEndText = gAIBlocks(idx).wszEnd
        if (idBlock = BLOCK_STATEMENT_FOR) andalso gConfig.ForNextVariable then
            ' Original casing, not the uppercased copy -- this is the user's identifier.
            dim as string sFor = PsLTrim( PsStrShrink(strPrevLineRaw) )
            dim as string sVar = PsStrParseAny( sFor, 2, " =" )
            if len(sVar) then sEndText &= " " & sVar
        end if
        strFill &= vbcrlf & sOuter & sEndText & vbcrlf
    end if
    SciExec( hEdit, SCI_ADDTEXT, len(strFill), strptr(strFill) )
    curPos += len(sInner)
    SciExec( hEdit, SCI_SETSEL, curPos, curPos )

    if gAIBlocks(idx).bReparse then frmFunctions_ReparseFiles()

    return 0
end function


' ========================================================================================
' ENV-GATED SELF-TEST -- TIKO_AUTOINSERT_SELFTEST=1
'
' WHAT THIS SUITE CANNOT SEE, stated up front rather than left as a flattering number.
' It is PURE: it drives AutoInsert_ClassifyLine and AutoInsert_ScanStep over line ARRAYS,
' and touches no document and no Scintilla. So it says nothing at all about
'
'   - whether AttemptAutoInsert is still wired to SCN_CHARADDED,
'   - whether the text it builds reaches the buffer,
'   - where the caret lands afterwards,
'   - whether AutoInsert_LineIsComment's Scintilla-style path works (that needs a lexed
'     document; only its text fallback is reachable from here),
'   - or whether the Else / Case re-alignment paths rewrite the right lines.
'
' Stub AttemptAutoInsert to a bare "return 0" and every assertion below stays green. The
' evidence for all of the above is the author's interactive pass.
'
' What it DOES cover is the one thing the rewrite exists for: given the lines that follow a
' freshly-opened block, does the scanner decide correctly whether that block already has an
' end? Every regression named in this file's header is an assertion here.
' ========================================================================================
function AutoInsert_RunSelfTest() as long

    if environ("TIKO_AUTOINSERT_SELFTEST") <> "1" then return 0

    AutoInsert_BuildBlockTable()

    dim as long nPass = 0, nFail = 0

    #macro CHECK( _cond_, _name_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            dim as string sMsg = "  FAIL: " & _name_
            print sMsg
        end if
    #endmacro

    print "AutoInsert self-test"

    ' ------------------------------------------------------------------------------------
    ' Run the lines that FOLLOW an opener through the scanner and report what it decided.
    ' This is AutoInsert_NeedsBlockEnd with pDoc->GetLine swapped for an array.
    ' ------------------------------------------------------------------------------------
    #macro NEEDSEND( _result_, _idBlock_, _lines_... )
        scope
            dim as string aLines(...) = { _lines_ }
            dim as AUTOINSERT_SCAN st
            st.nSP = 0
            _result_ = true                    ' ran off the end == unclosed
            for i as long = lbound(aLines) to ubound(aLines)
                if len(ltrim(aLines(i), any chr(32,9))) = 0 then continue for
                dim as long idFound = -1
                dim as long k = AutoInsert_ClassifyLine( aLines(i), idFound )
                dim as long r = AutoInsert_ScanStep( st, k, idFound, _idBlock_ )
                if r = AISCAN_INSERT then
                    _result_ = true
                    exit for
                end if
                if r = AISCAN_SKIP then
                    _result_ = false
                    exit for
                end if
            next
        end scope
    #endmacro

    #macro KINDIS( _text_, _kind_, _id_, _name_ )
        scope
            dim as long idOut = -1
            dim as long k = AutoInsert_ClassifyLine( _text_, idOut )
            CHECK( (k = _kind_) andalso (idOut = _id_), _name_ )
        end scope
    #endmacro

    dim as boolean b


    ' ====================================================================================
    ' 1. CLASSIFICATION
    ' ====================================================================================
    KINDIS( "if x then",                 AIL_OPENER, BLOCK_STATEMENT_IF,     "If ... Then opens" )
    KINDIS( "    If x > 1 Then    ",     AIL_OPENER, BLOCK_STATEMENT_IF,     "If opener is case- and space-insensitive" )
    KINDIS( "if x then print 1",         AIL_PLAIN,  -1,                     "single-line If opens nothing" )
    KINDIS( "if x then ' a comment",     AIL_OPENER, BLOCK_STATEMENT_IF,     "a trailing comment does not hide the Then" )
    KINDIS( "end if",                    AIL_CLOSER, BLOCK_STATEMENT_IF,     "End If closes" )
    KINDIS( "endif",                     AIL_CLOSER, BLOCK_STATEMENT_IF,     "EndIf (one word) closes -- the old table had no entry for it" )
    KINDIS( "end",                       AIL_PLAIN,  -1,                     "bare End closes nothing" )
    KINDIS( "else",                      AIL_MIDDLE, -1,                     "Else is a middle" )
    KINDIS( "elseif y then",             AIL_MIDDLE, -1,                     "ElseIf is a middle" )
    KINDIS( "case 1",                    AIL_CASE,   -1,                     "Case is its own kind" )
    KINDIS( "case else",                 AIL_CASE,   -1,                     "Case Else is a Case" )

    KINDIS( "do",                        AIL_OPENER, BLOCK_STATEMENT_DO,     "bare Do opens -- the old counter required a trailing space" )
    KINDIS( "while",                     AIL_OPENER, BLOCK_STATEMENT_WHILE,  "bare While opens" )
    KINDIS( "scope",                     AIL_OPENER, BLOCK_STATEMENT_SCOPE,  "bare Scope opens" )
    KINDIS( "do while x > 1",            AIL_OPENER, BLOCK_STATEMENT_DO,     "Do While is a Do, not a While" )
    KINDIS( "loop until x",              AIL_CLOSER, BLOCK_STATEMENT_DO,     "Loop closes a Do" )
    KINDIS( "wend",                      AIL_CLOSER, BLOCK_STATEMENT_WHILE,  "Wend closes a While" )
    KINDIS( "next i",                    AIL_CLOSER, BLOCK_STATEMENT_FOR,    "Next closes a For" )
    KINDIS( "exit for",                  AIL_PLAIN,  -1,                     "Exit For is not a For" )
    KINDIS( "exit sub",                  AIL_PLAIN,  -1,                     "Exit Sub is not a Sub" )
    KINDIS( "continue do",               AIL_PLAIN,  -1,                     "Continue Do is not a Do" )

    KINDIS( "public sub Foo",            AIL_OPENER, BLOCK_STATEMENT_SUB,    "Public Sub opens -- the old left(,11) test could never match this" )
    KINDIS( "private sub Foo",           AIL_OPENER, BLOCK_STATEMENT_SUB,    "Private Sub opens" )
    KINDIS( "static function Foo as long", AIL_OPENER, BLOCK_STATEMENT_FUNCTION, "Static Function opens" )
    KINDIS( "public const X = 1",        AIL_PLAIN,  -1,                     "Public on a non-procedure opens nothing" )
    KINDIS( "declare sub Foo",           AIL_PLAIN,  -1,                     "Declare Sub opens nothing" )
    KINDIS( "declare function Foo as long", AIL_PLAIN, -1,                   "Declare Function opens nothing" )
    KINDIS( "end sub",                   AIL_CLOSER, BLOCK_STATEMENT_SUB,    "End Sub closes" )
    KINDIS( "end function",              AIL_CLOSER, BLOCK_STATEMENT_FUNCTION, "End Function closes" )

    KINDIS( "function = 1",              AIL_PLAIN,  -1,                     "Function = value is an assignment" )
    KINDIS( "function Foo( x as long = 1 ) as long", AIL_OPENER, BLOCK_STATEMENT_FUNCTION, "a defaulted parameter is not a Function = assignment" )
    KINDIS( "type x as integer",         AIL_PLAIN,  -1,                     "a Type typedef opens nothing" )
    KINDIS( "type MYTYPE",               AIL_OPENER, BLOCK_STATEMENT_TYPE,   "a Type block opens" )
    KINDIS( "type MYTYPE extends BASE",  AIL_OPENER, BLOCK_STATEMENT_TYPE,   "Type ... Extends opens" )
    KINDIS( "dim as sub() p",            AIL_PLAIN,  -1,                     "a Sub-typed variable is not a Sub" )

    KINDIS( "#if defined(X)",            AIL_PLAIN,  -1,                     "a preprocessor directive is not a block here" )
    KINDIS( "' if x then",               AIL_BLANK,  -1,                     "a commented-out opener is not an opener" )
    KINDIS( "print ""if x then""",       AIL_PLAIN,  -1,                     "an opener inside a string literal is not an opener" )
    KINDIS( "s = ""a"" ' if x then",     AIL_PLAIN,  -1,                     "a quote inside code does not swallow the comment mark" )
    KINDIS( "select case x",             AIL_OPENER, BLOCK_STATEMENT_SELECT, "Select Case opens" )
    KINDIS( "end select",                AIL_CLOSER, BLOCK_STATEMENT_SELECT, "End Select closes" )
    KINDIS( "",                          AIL_BLANK,  -1,                     "an empty line is blank" )
    KINDIS( "     ",                     AIL_BLANK,  -1,                     "a whitespace-only line is blank" )


    ' ====================================================================================
    ' 2. THE SCOPED FORWARD SCAN -- the reason the rewrite exists.
    ' ====================================================================================
    NEEDSEND( b, BLOCK_STATEMENT_IF, "print 1", "end if" )
    CHECK( b = false, "an If already closed gets no second End If" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "print 1", "end sub" )
    CHECK( b = true, "an If reached only by an enclosing End Sub needs an End If" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "print 1", "print 2" )
    CHECK( b = true, "running off the end of the file means unclosed" )

    ' THE HEADLINE REGRESSION. A single-line If below us used to inflate the file-wide start
    ' count and produce a stray End If even though this block was already closed.
    NEEDSEND( b, BLOCK_STATEMENT_IF, "if q then print 9", "end if" )
    CHECK( b = false, "a single-line If below does not invent a second End If" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "endif" )
    CHECK( b = false, "EndIf closes the block -- the old table did not know the word" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "if y then", "print 1", "end if" )
    CHECK( b = true, "a nested If consuming the only End If leaves ours open" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "if y then", "end if", "end if" )
    CHECK( b = false, "two End Ifs close both the nested If and ours" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "for i = 1 to 10", "next", "end if" )
    CHECK( b = false, "an intervening For/Next does not hide our End If" )

    NEEDSEND( b, BLOCK_STATEMENT_SUB, "if x then", "end if", "end sub" )
    CHECK( b = false, "a Sub containing a complete If is closed" )

    ' This assertion was written the other way round first, and the suite was right and the
    ' assertion was wrong. An End Sub is sitting right there; the unterminated If above it is
    ' a real defect in the user's code but it is not this function's to repair, and quietly
    ' adding a SECOND End Sub would make it worse. A closer that matches ours and belongs to
    ' no block below us closes us, whatever state the lines in between are in.
    NEEDSEND( b, BLOCK_STATEMENT_SUB, "if x then", "end sub" )
    CHECK( b = false, "an unterminated If inside does not earn the Sub a second End Sub" )

    NEEDSEND( b, BLOCK_STATEMENT_FOR, "print 1", "next" )
    CHECK( b = false, "Next closes our For" )
    NEEDSEND( b, BLOCK_STATEMENT_FOR, "print 1", "wend" )
    CHECK( b = true, "a Wend does not close our For" )
    NEEDSEND( b, BLOCK_STATEMENT_DO, "print 1", "loop" )
    CHECK( b = false, "Loop closes our Do" )
    NEEDSEND( b, BLOCK_STATEMENT_WHILE, "print 1", "wend" )
    CHECK( b = false, "Wend closes our While" )
    NEEDSEND( b, BLOCK_STATEMENT_SELECT, "case 1", "case else", "end select" )
    CHECK( b = false, "Case lines do not disturb the Select scan" )
    NEEDSEND( b, BLOCK_STATEMENT_SELECT, "case 1", "end sub" )
    CHECK( b = true, "an unclosed Select needs an End Select" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "print 1", "else", "print 2", "end if" )
    CHECK( b = false, "Else does not break the If scan" )
    NEEDSEND( b, BLOCK_STATEMENT_IF, "elseif y then", "end if" )
    CHECK( b = false, "ElseIf does not open a second If" )

    NEEDSEND( b, BLOCK_STATEMENT_IF, "' end if", "end sub" )
    CHECK( b = true, "a commented-out End If does not close the block" )
    NEEDSEND( b, BLOCK_STATEMENT_IF, "print ""end if""", "end sub" )
    CHECK( b = true, "an End If inside a string literal does not close the block" )

    NEEDSEND( b, BLOCK_STATEMENT_TYPE, "type alias as integer", "end type" )
    CHECK( b = false, "a typedef between us and our End Type does not consume it" )

    NEEDSEND( b, BLOCK_STATEMENT_SUB, "print 1", "end sub", "public sub Bar", "end sub" )
    CHECK( b = false, "our Sub is closed by the first End Sub" )
    NEEDSEND( b, BLOCK_STATEMENT_FUNCTION, "public sub Bar", "end sub", "end function" )
    CHECK( b = false, "a whole Sub nested below does not consume our End Function" )


    ' ====================================================================================
    ' 3. INDENT ARITHMETIC
    '
    ' The old NumTabsFromSpaces used "/" -- floating point in FreeBASIC -- so 6 columns at
    ' indent 4 rounded UP to two tabs, and any remainder was dropped silently. The caret was
    ' then advanced by a separately computed count that could disagree with the string that
    ' was actually inserted; callers now advance by len() of this string, so they cannot.
    ' ====================================================================================
    CHECK( AutoInsert_IndentColumn( "        x", 4 ) = 8,  "eight leading spaces is column 8" )
    CHECK( AutoInsert_IndentColumn( chr(9) & "x", 4 ) = 4, "one tab is one indent level" )
    CHECK( AutoInsert_IndentColumn( chr(9) & chr(9) & "x", 4 ) = 8, "two tabs are two levels" )
    CHECK( AutoInsert_IndentColumn( "  " & chr(9) & "x", 4 ) = 4,   "a tab advances to the next tab stop, it does not add a whole level" )
    CHECK( AutoInsert_IndentColumn( "x", 4 ) = 0,          "no leading whitespace is column 0" )
    CHECK( AutoInsert_IndentColumn( "", 4 ) = 0,           "an empty line is column 0" )
    CHECK( AutoInsert_IndentColumn( "   x  ", 0 ) = 3,     "a zero indent size does not divide by zero" )

    scope
        dim as long bSaveTabs = gConfig.TabIndentSpaces

        gConfig.TabIndentSpaces = true
        CHECK( AutoInsert_Indent( 6, 4 ) = space(6), "in spaces mode the fill is spaces" )
        CHECK( len(AutoInsert_Indent( 6, 4 )) = 6,   "in spaces mode the caret advance is the column" )

        gConfig.TabIndentSpaces = false
        CHECK( AutoInsert_Indent( 8, 4 ) = string(2, chr(9)), "in tab mode a whole number of levels is tabs" )
        ' 6 columns at indent 4 is one tab and two spaces. The old code answered TWO TABS,
        ' because 6/4 is 1.5 and FreeBASIC's "/" rounds rather than truncating.
        CHECK( AutoInsert_Indent( 6, 4 ) = chr(9) & space(2), "in tab mode a partial level keeps its remainder" )
        CHECK( AutoInsert_Indent( 0, 4 ) = "",       "column 0 fills nothing" )
        CHECK( AutoInsert_Indent( -1, 4 ) = "",      "a negative column fills nothing" )
        CHECK( AutoInsert_Indent( 5, 0 ) = space(5), "a zero indent size falls back to spaces" )

        gConfig.TabIndentSpaces = bSaveTabs
    end scope


    ' ====================================================================================
    ' 4. THE TABLE ITSELF
    '
    ' One table now drives both the trigger and the scanner, so every opener MUST have a
    ' closer the scanner can recognise. The old code kept the two vocabularies apart and
    ' they had already drifted.
    ' ====================================================================================
    CHECK( gAIBlockCount = 17, "every BLOCK_STATEMENT_* has a table row" )
    scope
        dim as long nBad = 0
        for i as long = 0 to gAIBlockCount - 1
            if PsLen(gAIBlocks(i).wszOpen) = 0 then nBad += 1
            if len(gAIBlocks(i).wszEnd) = 0 then nBad += 1
            if (len(gAIBlocks(i).wszCloseW2) = 0) andalso (len(gAIBlocks(i).wszCloseAlt) = 0) then nBad += 1
            if AutoInsert_FindByID( gAIBlocks(i).idBlock ) <> i then nBad += 1
        next
        CHECK( nBad = 0, "every row has an opener, an end text, a recognisable closer and a unique id" )

        ' The round trip that matters: the text we INSERT must classify back as the closer of
        ' the block we inserted it for. Nothing else stops the two halves drifting apart.
        nBad = 0
        for i as long = 0 to gAIBlockCount - 1
            dim as long idOut = -1
            dim as long k = AutoInsert_ClassifyLine( gAIBlocks(i).wszEnd, idOut )
            if (k <> AIL_CLOSER) orelse (idOut <> gAIBlocks(i).idBlock) then
                nBad += 1
                dim as string sMsg = "    round trip failed for [" & gAIBlocks(i).wszEnd & "]"
                print sMsg
            end if
        next
        CHECK( nBad = 0, "the end text we insert classifies back as that block's closer" )

        ' And every opener keyword must classify back as an opener of its own row, so a new
        ' row cannot be added that the trigger will never fire on.
        nBad = 0
        for i as long = 0 to gAIBlockCount - 1
            dim as long idOut = -1
            dim as string sOpen = gAIBlocks(i).wszOpen
            ' Give the two openers that need more than their keyword what they need.
            if gAIBlocks(i).idBlock = BLOCK_STATEMENT_IF then sOpen &= " x then"
            if gAIBlocks(i).idBlock = BLOCK_STATEMENT_TYPE then sOpen &= " tname"
            dim as long k = AutoInsert_ClassifyLine( sOpen, idOut )
            if (k <> AIL_OPENER) orelse (idOut <> gAIBlocks(i).idBlock) then
                nBad += 1
                dim as string sMsg = "    opener round trip failed for [" & sOpen & "]"
                print sMsg
            end if
        next
        CHECK( nBad = 0, "every opener keyword classifies back as an opener of its own row" )
    end scope

    dim as string sReport = "AutoInsert self-test: " & nPass & " passed, " & nFail & " failed"
    print sReport
    return nFail
end function
