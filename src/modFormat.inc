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
' CODE FORMATTER -- the engine.
'
' PURE TEXT IN, TEXT OUT. Nothing here touches a window, a document, a Scintilla view or
' any global other than gConfig (for the keyword vocabulary and nothing else). That is
' deliberate and load-bearing: the live preview in frmFormatOptions, the self-test, and
' the offline Format Project path all call Format_Text, so none of them can drift from
' what actually happens to the user's buffer.
'
' ARCHITECTURE -- a lossless token list plus an indent stack, NOT an AST.
'
' FreeBASIC is line-oriented: one statement per line, explicit terminators (End If, Next,
' End Sub), and line breaks are the programmer's own, marked with a trailing underscore.
' The expensive half of an AST formatter -- the width-driven layout algebra that decides
' WHERE to break a line -- solves a problem this language does not have. Building a parse
' tree to learn that "End If" dedents one level would be enormous overkill. The closest
' shipping analogue is VB.NET's Pretty Listing, which is line-oriented for exactly this
' reason.
'
' What we borrow from the lossless-tree formatters instead is the token list: EVERY byte
' of the input lands in exactly one token, comments and whitespace included. Concatenating
' an untouched list reproduces the input byte for byte. Two consequences:
'
'   1. Comment preservation is free. A comment is a token that gets copied through; there
'      is no "attach the comments back onto the tree" problem, which the Go team called
'      the single biggest mistake in gofmt's original design.
'   2. The semantic safety check is a one-liner. Compare the non-whitespace token streams
'      before and after: if they are equal, the program cannot have changed meaning. That
'      is the cheap equivalent of Black's assert_equivalent with no AST anywhere.
'
' THE FORMATTER NEVER MOVES A LINE BREAK. It rewrites whitespace and keyword casing and
' nothing else -- no splitting "If x Then y" into a block, no joining, no ":" separation.
' That boundary is Rubberduck's (the open-source VB6 Smart Indenter), and it is what makes
' the token-stream check above a complete statement of safety rather than an approximation.
' The one rule that adds or removes anything is BlankLinesAroundProc, which inserts and
' deletes blank lines; see FMT_TokensMatch's bIgnoreEOL parameter, which exists solely for
' that rule and is the one place the safety net is deliberately widened.
'
' FAILURE POLICY: format what we can. A line the scanner cannot classify is passed through
' byte for byte and formatting continues around it. The engine never refuses a whole file
' over one odd line -- but if either safety check fails it returns the ORIGINAL text
' unchanged, so a caller that ignores the bChanged flag still cannot corrupt a buffer.
' ========================================================================================


' ----------------------------------------------------------------------------------------
' Line classification, from the first significant word(s) of the line.
' ----------------------------------------------------------------------------------------
enum
    FMTLINE_PLAIN = 0
    FMTLINE_OPENER         ' If..Then, For, Sub, Type, Select Case, ...
    FMTLINE_CLOSER         ' End If, Next, Loop, Wend, End Sub, ...
    FMTLINE_MIDDLE         ' Else, ElseIf
    FMTLINE_CASE           ' Case, Case Else
    FMTLINE_DIRECTIVE      ' # or $ -- pinned to column 0
    FMTLINE_BLANK
    FMTLINE_MLCOMMENT      ' a line sitting INSIDE a /' '/ block: indent left alone
    FMTLINE_CONTINUED      ' a line whose predecessor ended with _
end enum


' Block kinds pushed on the indent stack. Only SELECT needs to be told apart, because Case
' labels are placed relative to their Select rather than to the running depth.
enum
    FMTBLK_GENERIC = 0
    FMTBLK_SELECT
end enum


type FMTBLOCK
    kind      as long
    ownDepth  as long      ' the depth the OPENING line itself was emitted at
end type


' ----------------------------------------------------------------------------------------
' The keyword vocabulary, bucketed by first character so a lookup scans ~250 bytes rather
' than the whole 6KB list. Built lazily from gConfig on first use and thrown away by
' FMT_InvalidateVocabulary when the user edits a keyword page.
'
' The lists are LOWERCASED HERE, for matching, but they arrive carrying CANONICAL CASING
' (ByVal, ScreenRes, MessageBox) and that is deliberate -- FMT_BuildProperTable reads the
' same text to answer FMTCASE_PROPER.
'
' This was the other way round until tiko's Lexilla was changed: WordList::Set now
' lower-cases the word LIST as well as the token (WordList.cxx), where it used to compare
' bytes exactly against a token that had already been lowered -- so a mixed-case entry
' matched nothing and silently killed that keyword's highlighting. One set of files now
' serves highlighting, SC_CASE_PROPER rendering and this formatter alike, which is what
' stops the editor and the formatter disagreeing about how a keyword is spelled.
' ----------------------------------------------------------------------------------------
dim shared gFmtVocabReady   as long = false
dim shared gFmtKeywordBuk(0 to 26) as string
dim shared gFmtTypeBlob     as string
dim shared gFmtProperBuk(0 to 26) as string   ' "word=ProperSpelling " pairs
dim shared gFmtProperReady  as long = false


' ========================================================================================
' Bucket index for a word: 1..26 for a-z, 0 for everything else (#, $, _, digits).
' ========================================================================================
private function FMT_Bucket( byref sWord as string ) as long
    if len(sWord) = 0 then return 0
    dim as long ch = asc(lcase(left(sWord, 1)))
    if (ch >= 97) andalso (ch <= 122) then return ch - 96
    return 0
end function


' ========================================================================================
' Build the bucketed keyword lookup from gConfig's three keyword lists.
'
' Only the FreeBASIC list feeds the KEYWORD vocabulary. The WinAPI and Extra lists are
' deliberately NOT included: they are user-editable identifier lists, and recasing an
' identifier the user typed is a different and much more opinionated act than recasing a
' language keyword. Leaving them out means "Case: keywords" can never rewrite a name that
' belongs to the user's own code or to somebody's library.
' ========================================================================================
private sub FMT_BuildVocabulary()
    if gFmtVocabReady then exit sub

    for i as long = 0 to 26
        gFmtKeywordBuk(i) = " "
    next

    dim as string sList = lcase( gConfig.FBKeywords )
    ' Normalize every run of whitespace to a single space so the split below is uniform.
    dim as string sNorm = ""
    dim as long bPrevWasSpace = true
    for i as long = 1 to len(sList)
        dim as long ch = asc(mid(sList, i, 1))
        if (ch = 32) orelse (ch = 9) orelse (ch = 13) orelse (ch = 10) then
            if bPrevWasSpace = false then sNorm &= " "
            bPrevWasSpace = true
        else
            sNorm &= chr(ch)
            bPrevWasSpace = false
        end if
    next
    sNorm = trim(sNorm)

    dim as long nStart = 1
    do
        dim as long nSpace = instr(nStart, sNorm, " ")
        dim as string sWord
        if nSpace = 0 then
            sWord = mid(sNorm, nStart)
        else
            sWord = mid(sNorm, nStart, nSpace - nStart)
        end if
        if len(sWord) > 0 then
            dim as long b = FMT_Bucket(sWord)
            gFmtKeywordBuk(b) &= sWord & " "
        end if
        if nSpace = 0 then exit do
        nStart = nSpace + 1
    loop

    ' Intrinsic type names. A fixed list rather than a slice of the keyword file, because
    ' "which words are types" is a language fact that does not change when the user edits
    ' their keyword list -- and the Casing page offers types as a separate vocabulary
    ' precisely so they can be styled differently from control-flow keywords.
    gFmtTypeBlob = " byte ubyte short ushort integer uinteger long ulong longint ulongint " & _
                   "single double string zstring wstring boolean any pointer ptr " & _
                   "sub function unsigned signed const "

    gFmtVocabReady = true
end sub


' ========================================================================================
' Discard the cached vocabulary. Call after the user edits a keyword page.
' ========================================================================================
sub FMT_InvalidateVocabulary()
    gFmtVocabReady = false
    gFmtProperReady = false
end sub


' ========================================================================================
' Build the lowercase -> canonical-spelling map used by FMTCASE_PROPER.
' ========================================================================================
private sub FMT_BuildProperTable()
    if gFmtProperReady then exit sub

    for i as long = 0 to 26
        gFmtProperBuk(i) = " "
    next

    ' CANONICAL SPELLINGS COME STRAIGHT FROM THE KEYWORD FILES, which carry them.
    '
    ' This used to read a separate settingsormatb_keyword_case.txt, because the keyword
    ' lists were all lower case and the lexer compared bytes exactly -- a mixed-case entry
    ' would never match and would silently kill that keyword's highlighting. That is no
    ' longer true: WordList::Set now lower-cases the LIST as well as the token, so one set
    ' of files serves highlighting, SC_CASE_PROPER rendering and this formatter alike.
    '
    ' Reading the same text clsDocument hands to SCI_SETCASEKEYWORDS is what keeps the
    ' formatter from disagreeing with the editor about how a keyword is spelled -- a
    ' second table would drift the first time somebody edited a Keywords page.
    ' A keyword whose file spelling is still lower case falls back to Capitalized, exactly
    ' as the renderer falls back to camel case.
    dim as string sData = gConfig.FBKeywords & " " & _
                          gConfig.WinApiKeywords & " " & _
                          gConfig.ExtraKeywords

    dim as long nStart = 1
    dim as long nLen = len(sData)
    while nStart <= nLen
        while nStart <= nLen
            dim as long ch = asc(mid(sData, nStart, 1))
            if (ch <> 32) andalso (ch <> 9) andalso (ch <> 13) andalso (ch <> 10) then exit while
            nStart += 1
        wend
        if nStart > nLen then exit while

        dim as long nEnd = nStart
        while nEnd <= nLen
            dim as long ch = asc(mid(sData, nEnd, 1))
            if (ch = 32) orelse (ch = 9) orelse (ch = 13) orelse (ch = 10) then exit while
            nEnd += 1
        wend

        dim as string sWord = mid(sData, nStart, nEnd - nStart)
        nStart = nEnd + 1

        ' Only words that actually CARRY casing earn an entry. A list that is still all
        ' lower case adds nothing here and leaves every word on the Capitalized fallback,
        ' which is the same answer for less memory.
        if (len(sWord) > 0) andalso (sWord <> lcase(sWord)) then
            dim as string sKey = lcase(sWord)
            dim as long b = FMT_Bucket(sKey)
            ' First spelling wins, so FBKeywords beats a later WinApi/Extra duplicate.
            if instr( gFmtProperBuk(b), " " & sKey & "=" ) = 0 then
                gFmtProperBuk(b) &= sKey & "=" & sWord & " "
            end if
        end if
    wend

    gFmtProperReady = true
end sub


' ========================================================================================
' Classify a word into one of the four casing vocabularies.
' ========================================================================================
private function FMT_ClassifyWord( byref sWord as string ) as long
    if len(sWord) = 0 then return FMTVOCAB_NONE
    FMT_BuildVocabulary()

    dim as string sLower = lcase(sWord)
    dim as string sProbe = " " & sLower & " "

    dim as long ch1 = asc(left(sLower, 1))
    if (ch1 = 35) orelse (ch1 = 36) then      ' # or $
        return FMTVOCAB_DIRECTIVE
    end if

    if instr(gFmtTypeBlob, sProbe) > 0 then return FMTVOCAB_TYPE

    dim as long b = FMT_Bucket(sLower)
    if instr(gFmtKeywordBuk(b), sProbe) > 0 then return FMTVOCAB_KEYWORD

    return FMTVOCAB_NONE
end function


' ========================================================================================
' Apply a casing mode to one word.
'
' FMTCASE_PROPER consults the spelling table and falls back to Capitalized. A word the
' table does not know is therefore still recased consistently rather than left in whatever
' state it happened to be in -- a formatter that silently skipped unknown words would
' produce a file with two casing conventions in it, which is worse than either.
' ========================================================================================
private function FMT_ApplyCase( byref sWord as string, byval nMode as long ) as string
    select case nMode
        case FMTCASE_LOWER
            return lcase(sWord)
        case FMTCASE_UPPER
            return ucase(sWord)
        case FMTCASE_PROPER
            FMT_BuildProperTable()
            dim as string sLower = lcase(sWord)
            dim as long b = FMT_Bucket(sLower)
            dim as long nPos = instr(gFmtProperBuk(b), " " & sLower & "=")
            if nPos > 0 then
                dim as long nFrom = nPos + len(sLower) + 2
                dim as long nTo = instr(nFrom, gFmtProperBuk(b), " ")
                if nTo > nFrom then return mid(gFmtProperBuk(b), nFrom, nTo - nFrom)
            end if
            ' Fallback: Capitalized. Handles the # and $ prefixes by capitalizing the
            ' first LETTER rather than the first character, so #include becomes #Include.
            dim as string sOut = sLower
            for i as long = 1 to len(sOut)
                dim as long c = asc(mid(sOut, i, 1))
                if (c >= 97) andalso (c <= 122) then
                    mid(sOut, i, 1) = chr(c - 32)
                    exit for
                end if
            next
            return sOut
    end select
    return sWord
end function


' ========================================================================================
' Character predicates.
' ========================================================================================
private function FMT_IsDigit( byval ch as long ) as long
    return iif( (ch >= 48) andalso (ch <= 57), true, false )
end function

private function FMT_IsAlpha( byval ch as long ) as long
    if (ch >= 65) andalso (ch <= 90) then return true
    if (ch >= 97) andalso (ch <= 122) then return true
    return false
end function

private function FMT_IsWordChar( byval ch as long ) as long
    if FMT_IsAlpha(ch) then return true
    if FMT_IsDigit(ch) then return true
    if ch = 95 then return true            ' _
    return false
end function


' ========================================================================================
' LEXER. Produces the lossless token list described in this file's header.
'
' Every byte of sText lands in exactly one token, so concatenating tokens(0..n-1).text
' reproduces sText exactly. The self-test asserts that directly, because it is the property
' every safety guarantee downstream rests on.
' ========================================================================================
function FMT_Lex( byref sText as string, tokens() as FMTTOKEN ) as long
    dim as long nLen = len(sText)
    dim as long nCount = 0

    if nLen = 0 then
        redim tokens(0 to 0)
        return 0
    end if

    ' Worst case is one token per byte; growing by doubling would cost a copy per growth
    ' on a large file, and a file's byte count is a hard ceiling we already know.
    redim tokens(0 to nLen)

    dim as long i = 1
    dim as long nMLDepth = 0               ' /' '/ nesting -- FreeBASIC nests these
    dim as long bStmtStart = true          ' no significant token yet on this statement

    while i <= nLen
        dim as long ch = asc(mid(sText, i, 1))
        dim as long nStart = i

        ' -- inside a nested block comment ------------------------------------------------
        if nMLDepth > 0 then
            ' Consume up to (but not including) the next EOL, tracking nesting so the
            ' comment ends in the right place.
            while i <= nLen
                dim as long c = asc(mid(sText, i, 1))
                if (c = 13) orelse (c = 10) then exit while
                if (c = 47) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 39) then
                    nMLDepth += 1 : i += 2 : continue while       ' /'
                end if
                if (c = 39) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 47) then
                    nMLDepth -= 1 : i += 2
                    if nMLDepth = 0 then exit while
                    continue while
                end if
                i += 1
            wend
            if i > nStart then
                tokens(nCount).kind = FMTTOK_MLCOMMENT
                tokens(nCount).text = mid(sText, nStart, i - nStart)
                nCount += 1
                continue while
            end if
            ' NOTHING was consumed, which means we are sitting on the line break that ends
            ' this comment's line. Fall through to the EOL handler below rather than looping
            ' -- a `continue while` here re-tests the same byte forever. (Found by the
            ' standalone harness hanging, not by any assertion: an infinite loop inside the
            ' lexer never reaches a CHECK.)
        end if

        ' -- end of line ------------------------------------------------------------------
        if (ch = 13) orelse (ch = 10) then
            if (ch = 13) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 10) then
                i += 2
            else
                i += 1
            end if
            tokens(nCount).kind = FMTTOK_EOL
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            bStmtStart = true
            continue while
        end if

        ' -- whitespace -------------------------------------------------------------------
        if (ch = 32) orelse (ch = 9) then
            while i <= nLen
                dim as long c = asc(mid(sText, i, 1))
                if (c <> 32) andalso (c <> 9) then exit while
                i += 1
            wend
            tokens(nCount).kind = FMTTOK_WS
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            continue while
        end if

        ' -- opening a block comment ------------------------------------------------------
        if (ch = 47) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 39) then
            nMLDepth = 1
            i += 2
            while i <= nLen
                dim as long c = asc(mid(sText, i, 1))
                if (c = 13) orelse (c = 10) then exit while
                if (c = 47) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 39) then
                    nMLDepth += 1 : i += 2 : continue while
                end if
                if (c = 39) andalso (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 47) then
                    nMLDepth -= 1 : i += 2
                    if nMLDepth = 0 then exit while
                    continue while
                end if
                i += 1
            wend
            tokens(nCount).kind = FMTTOK_MLCOMMENT
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            bStmtStart = false
            continue while
        end if

        ' -- single line comment ----------------------------------------------------------
        if ch = 39 then                                  ' '
            while i <= nLen
                dim as long c = asc(mid(sText, i, 1))
                if (c = 13) orelse (c = 10) then exit while
                i += 1
            wend
            tokens(nCount).kind = FMTTOK_COMMENT
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            continue while
        end if

        ' -- string literal, including the !"..." and $"..." escaped forms -----------------
        dim as long bStringPrefix = false
        if ((ch = 33) orelse (ch = 36)) andalso (i < nLen) then       ' ! or $
            if asc(mid(sText, i + 1, 1)) = 34 then bStringPrefix = true
        end if
        if (ch = 34) orelse bStringPrefix then
            if bStringPrefix then i += 1
            i += 1                                        ' past the opening quote
            while i <= nLen
                dim as long c = asc(mid(sText, i, 1))
                if (c = 13) orelse (c = 10) then exit while
                if c = 34 then
                    ' A doubled quote is an embedded quote, not a terminator.
                    if (i < nLen) andalso (asc(mid(sText, i + 1, 1)) = 34) then
                        i += 2 : continue while
                    end if
                    i += 1
                    exit while
                end if
                i += 1
            wend
            tokens(nCount).kind = FMTTOK_STRING
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            bStmtStart = false
            continue while
        end if

        ' -- &h / &o / &b radix literals --------------------------------------------------
        if (ch = 38) andalso (i < nLen) then              ' &
            dim as long c2 = asc(lcase(mid(sText, i + 1, 1)))
            if (c2 = 104) orelse (c2 = 111) orelse (c2 = 98) then      ' h o b
                i += 2
                while i <= nLen
                    dim as long c = asc(mid(sText, i, 1))
                    if (FMT_IsWordChar(c) = false) then exit while
                    i += 1
                wend
                tokens(nCount).kind = FMTTOK_NUMBER
                tokens(nCount).text = mid(sText, nStart, i - nStart)
                nCount += 1
                bStmtStart = false
                continue while
            end if
        end if

        ' -- decimal number ---------------------------------------------------------------
        if FMT_IsDigit(ch) orelse ((ch = 46) andalso (i < nLen) andalso FMT_IsDigit(asc(mid(sText, i + 1, 1)))) then
            dim as long bSeenDot = false
            while i <= nLen
                dim as long c = asc(mid(sText, i, 1))
                if FMT_IsDigit(c) then
                    i += 1
                elseif (c = 46) andalso (bSeenDot = false) then
                    bSeenDot = true : i += 1
                else
                    exit while
                end if
            wend
            ' Exponent. The sign here belongs to the literal and must never be spaced as
            ' though it were a binary operator, which is why it is consumed into the token.
            if i <= nLen then
                dim as long c = asc(lcase(mid(sText, i, 1)))
                if (c = 101) orelse (c = 100) then        ' e or d
                    dim as long nSave = i
                    dim as long j = i + 1
                    if j <= nLen then
                        dim as long cs = asc(mid(sText, j, 1))
                        if (cs = 43) orelse (cs = 45) then j += 1
                    end if
                    if (j <= nLen) andalso FMT_IsDigit(asc(mid(sText, j, 1))) then
                        i = j
                        while (i <= nLen) andalso FMT_IsDigit(asc(mid(sText, i, 1)))
                            i += 1
                        wend
                    else
                        i = nSave
                    end if
                end if
            end if
            ' Type suffix (!, #, &, %, and the u/l/ll family).
            while i <= nLen
                dim as long c = asc(lcase(mid(sText, i, 1)))
                if (c = 33) orelse (c = 35) orelse (c = 38) orelse (c = 37) orelse _
                   (c = 117) orelse (c = 108) orelse (c = 102) then
                    i += 1
                else
                    exit while
                end if
            wend
            tokens(nCount).kind = FMTTOK_NUMBER
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            bStmtStart = false
            continue while
        end if

        ' -- # or $ directive name --------------------------------------------------------
        if ((ch = 35) orelse (ch = 36)) andalso bStmtStart then
            i += 1
            while (i <= nLen) andalso FMT_IsWordChar(asc(mid(sText, i, 1)))
                i += 1
            wend
            tokens(nCount).kind = FMTTOK_DIRECTIVE
            tokens(nCount).text = mid(sText, nStart, i - nStart)
            nCount += 1
            bStmtStart = false
            continue while
        end if

        ' -- word, or a lone trailing underscore = line continuation ----------------------
        if FMT_IsWordChar(ch) andalso (FMT_IsDigit(ch) = false) then
            while (i <= nLen) andalso FMT_IsWordChar(asc(mid(sText, i, 1)))
                i += 1
            wend
            dim as string sWord = mid(sText, nStart, i - nStart)

            ' A bare _ is a continuation marker only when nothing but whitespace and a
            ' comment follows it on the line. Anywhere else it is a legal identifier.
            if sWord = "_" then
                dim as long j = i
                dim as long bOnlyTrailing = true
                while j <= nLen
                    dim as long c = asc(mid(sText, j, 1))
                    if (c = 13) orelse (c = 10) then exit while
                    if (c = 32) orelse (c = 9) then
                        j += 1 : continue while
                    end if
                    if c = 39 then exit while             ' a trailing comment is allowed
                    bOnlyTrailing = false
                    exit while
                wend
                if bOnlyTrailing then
                    tokens(nCount).kind = FMTTOK_CONT
                    tokens(nCount).text = sWord
                    nCount += 1
                    continue while
                end if
            end if

            ' Rem starts a comment, but only as the first word of a statement -- "rem" is
            ' otherwise a perfectly ordinary identifier.
            if bStmtStart andalso (lcase(sWord) = "rem") then
                while i <= nLen
                    dim as long c = asc(mid(sText, i, 1))
                    if (c = 13) orelse (c = 10) then exit while
                    i += 1
                wend
                tokens(nCount).kind = FMTTOK_COMMENT
                tokens(nCount).text = mid(sText, nStart, i - nStart)
                nCount += 1
                bStmtStart = false
                continue while
            end if

            tokens(nCount).kind = FMTTOK_WORD
            tokens(nCount).text = sWord
            nCount += 1
            bStmtStart = false
            continue while
        end if

        ' -- operator / punctuation -------------------------------------------------------
        dim as string sTwo = ""
        if i < nLen then sTwo = mid(sText, i, 2)
        select case sTwo
            case "<=", ">=", "<>", "->", "+=", "-=", "*=", "/=", "\=", "^=", "&=", "=>"
                i += 2
            case else
                i += 1
        end select
        tokens(nCount).kind = FMTTOK_OP
        tokens(nCount).text = mid(sText, nStart, i - nStart)
        nCount += 1
        ' A colon ends the statement, so the next word can legitimately be Rem or a # line.
        if tokens(nCount - 1).text = ":" then
            bStmtStart = true
        else
            bStmtStart = false
        end if
    wend

    return nCount
end function


' ========================================================================================
' True when a token carries program meaning -- i.e. everything except whitespace, comments
' and line breaks. This is the predicate the semantic safety check is built on.
' ========================================================================================
private function FMT_IsSignificant( byval nKind as long ) as long
    select case nKind
        case FMTTOK_WS, FMTTOK_COMMENT, FMTTOK_MLCOMMENT, FMTTOK_EOL
            return false
    end select
    return true
end function


' ========================================================================================
' SAFETY CHECK 1 -- semantic equivalence, the cheap version.
'
' Lex both texts and compare their non-whitespace token streams. If they are identical
' then the formatter moved whitespace and changed letter case and did nothing else, which
' for a formatter that never moves a line break is a complete statement that the program
' did not change. This is Black's assert_equivalent without the AST.
'
' Word tokens are compared case-INSENSITIVELY, because recasing keywords is exactly what
' this formatter is for and FreeBASIC is a case-insensitive language. String literals are
' compared byte-exactly -- their case is program data, not syntax.
'
' bIgnoreEOL widens the comparison to also skip line breaks. It exists for one rule only,
' BlankLinesAroundProc, which legitimately inserts and deletes blank lines. Everywhere
' else it is passed false, because a formatter that has silently eaten a line break should
' be caught, not excused.
' ========================================================================================
function FMT_TokensMatch( byref sBefore as string, _
                          byref sAfter as string, _
                          byval bIgnoreEOL as long ) as long
    dim as FMTTOKEN tokA(any), tokB(any)
    dim as long nA = FMT_Lex( sBefore, tokA() )
    dim as long nB = FMT_Lex( sAfter, tokB() )

    dim as long ia = 0, ib = 0
    do
        ' Advance both sides to their next comparable token.
        while ia < nA
            dim as long k = tokA(ia).kind
            dim as long bSkip = false
            if FMT_IsSignificant(k) = false then bSkip = true
            if (k = FMTTOK_EOL) andalso (bIgnoreEOL = false) then bSkip = false
            if bSkip = false then exit while
            ia += 1
        wend
        while ib < nB
            dim as long k = tokB(ib).kind
            dim as long bSkip = false
            if FMT_IsSignificant(k) = false then bSkip = true
            if (k = FMTTOK_EOL) andalso (bIgnoreEOL = false) then bSkip = false
            if bSkip = false then exit while
            ib += 1
        wend

        if (ia >= nA) andalso (ib >= nB) then return true
        if (ia >= nA) orelse (ib >= nB) then return false

        if tokA(ia).kind <> tokB(ib).kind then return false
        if tokA(ia).kind = FMTTOK_STRING then
            if tokA(ia).text <> tokB(ib).text then return false
        else
            if lcase(tokA(ia).text) <> lcase(tokB(ib).text) then return false
        end if

        ia += 1
        ib += 1
    loop
end function


' ----------------------------------------------------------------------------------------
' One source line, as a span of the token array.
' ----------------------------------------------------------------------------------------
type FMTLINE
    nFirst      as long        ' first token index of the line
    nLast       as long        ' last token index EXCLUDING the EOL token (-1 if empty)
    sEOL        as string      ' "" on a final line with no terminator
    nClass      as long
    bInML       as long        ' the line begins inside a block comment
    bContinued  as long        ' the line ends with a _ continuation marker
    sOut        as string      ' the formatted body, filled in by the passes
    nDepth      as long
end type


' ========================================================================================
' The set of binary operators that SpaceAroundOperators applies to.
'
' Deliberately excludes ".", "->", "@", brackets and punctuation -- member access and
' punctuation are not operators, and spacing them would be wrong rather than merely a
' matter of taste.
' ========================================================================================
private function FMT_IsBinaryOp( byref sOp as string ) as long
    select case sOp
        case "=", "+", "-", "*", "/", "\", "^", "<", ">", "<=", ">=", "<>", "&", _
             "+=", "-=", "*=", "/=", "\=", "^=", "&=", "=>"
            return true
    end select
    return false
end function


' ========================================================================================
' Decide whether a + - * or @ at this point is UNARY rather than binary.
'
' Getting this wrong is the classic beautifier bug: "x = -1" becomes "x = - 1", and
' "*pDest = 5" becomes "* pDest = 5". The test is what PRECEDES the operator -- a value
' (identifier, literal, closing bracket) means binary; anything else means unary.
' ========================================================================================
private function FMT_IsUnaryContext( byref sPrevText as string, byval nPrevKind as long ) as long
    if nPrevKind = -1 then return true                    ' start of line
    select case nPrevKind
        case FMTTOK_NUMBER, FMTTOK_STRING
            return false
        case FMTTOK_OP
            ' A value ends with a closing bracket; everything else leaves us expecting one.
            if (sPrevText = ")") orelse (sPrevText = "]") then return false
            return true
        case FMTTOK_WORD
            ' An identifier is a value; a keyword ("Return -1", "To -5", "Step -1") is not.
            if FMT_ClassifyWord(sPrevText) = FMTVOCAB_NONE then return false
            return true
        case FMTTOK_DIRECTIVE
            return true
    end select
    return false
end function


' ========================================================================================
' Classify a line from its first significant word(s), and note whether it opens a Select.
'
' Every judgement here is made on the FIRST word of the statement (plus the second where
' "End x" or a Type typedef needs it), never on a word found anywhere in the line. That is
' what keeps "Dim As Sub() p" from reading as a Sub block and "Exit For" from reading as a
' For block -- both of which a naive keyword search gets wrong.
' ========================================================================================
private function FMT_ClassifyLine( tokens() as FMTTOKEN, _
                                   byval nFirst as long, _
                                   byval nLast as long, _
                                   byref bIsSelect as long ) as long
    bIsSelect = false
    if nLast < nFirst then return FMTLINE_BLANK

    dim as long nSig = -1
    dim as string sW1 = "", sW2 = "", sW3 = ""
    dim as long nWords = 0
    dim as long bAnyContent = false

    for i as long = nFirst to nLast
        dim as long k = tokens(i).kind
        if k = FMTTOK_WS then continue for
        if (k = FMTTOK_COMMENT) orelse (k = FMTTOK_MLCOMMENT) then
            bAnyContent = true
            continue for
        end if
        bAnyContent = true
        if nSig = -1 then nSig = i
        if (k = FMTTOK_WORD) andalso (nWords < 3) then
            nWords += 1
            if nWords = 1 then sW1 = lcase(tokens(i).text)
            if nWords = 2 then sW2 = lcase(tokens(i).text)
            if nWords = 3 then sW3 = lcase(tokens(i).text)
        end if
    next

    if bAnyContent = false then return FMTLINE_BLANK
    if nSig = -1 then return FMTLINE_PLAIN            ' comment-only line

    if tokens(nSig).kind = FMTTOK_DIRECTIVE then return FMTLINE_DIRECTIVE
    if tokens(nSig).kind <> FMTTOK_WORD then return FMTLINE_PLAIN

    ' Last significant word, for the multi-line "If ... Then" test.
    dim as string sLastWord = ""
    for i as long = nLast to nFirst step -1
        dim as long k = tokens(i).kind
        if (k = FMTTOK_WS) orelse (k = FMTTOK_COMMENT) orelse (k = FMTTOK_MLCOMMENT) then continue for
        if k = FMTTOK_CONT then continue for
        if k = FMTTOK_WORD then sLastWord = lcase(tokens(i).text)
        exit for
    next

    ' -- closers -------------------------------------------------------------------------
    select case sW1
        case "next", "loop", "wend", "endif"
            return FMTLINE_CLOSER
        case "end"
            select case sW2
                case "if", "select", "sub", "function", "property", "operator", "type", _
                     "enum", "union", "with", "scope", "namespace", "constructor", _
                     "destructor", "extern"
                    return FMTLINE_CLOSER
            end select
            ' A bare "End" terminates the program; it closes no block.
            return FMTLINE_PLAIN
    end select

    ' -- middles -------------------------------------------------------------------------
    if (sW1 = "else") orelse (sW1 = "elseif") then return FMTLINE_MIDDLE
    if sW1 = "case" then return FMTLINE_CASE

    ' -- openers -------------------------------------------------------------------------
    select case sW1
        case "if"
            ' Only a line ENDING in Then opens a block. "If x Then y" is a complete
            ' statement and must not indent what follows it.
            if sLastWord = "then" then return FMTLINE_OPENER
            return FMTLINE_PLAIN
        case "for", "do", "while", "with", "scope", "namespace", "enum", "union", "extern"
            return FMTLINE_OPENER
        case "select"
            bIsSelect = true
            return FMTLINE_OPENER
        case "sub", "function", "property", "operator", "constructor", "destructor"
            return FMTLINE_OPENER
        case "type"
            ' "Type name As other" is a typedef, not a block. Anything else -- "Type name",
            ' "Type name Extends base", "Type name Field = 4", or a bare anonymous "Type"
            ' inside a Union -- opens one.
            if (nWords >= 3) andalso (sW3 = "as") then return FMTLINE_PLAIN
            return FMTLINE_OPENER
        case "public", "private", "static"
            ' These prefix a Sub/Function declaration; look one word further along.
            select case sW2
                case "sub", "function", "property", "operator", "constructor", "destructor"
                    return FMTLINE_OPENER
            end select
            return FMTLINE_PLAIN
    end select

    return FMTLINE_PLAIN
end function


' ========================================================================================
' Build one line's formatted body -- casing and spacing, no indentation.
'
' The separator policy is PRESERVE BY DEFAULT: between two tokens we keep whether the user
' had whitespace there, normalized to a single space, and only override at the specific
' junctions a rule names. That is deliberately conservative. Deriving every separator from
' scratch instead would mean making a hundred judgement calls the user never asked for,
' and that is how a beautifier ends up fighting its owner.
' ========================================================================================
private function FMT_BuildBody( tokens() as FMTTOKEN, _
                                byval nFirst as long, _
                                byval nLast as long, _
                                byref rules as FORMAT_RULES, _
                                byval nClass as long ) as string
    if nLast < nFirst then return ""

    ' Collect the line's non-whitespace tokens, remembering which had whitespace in front.
    dim as long nMax = nLast - nFirst + 2
    dim as long idx(0 to nMax)
    dim as long hadWS(0 to nMax)
    dim as long nCnt = 0
    dim as long bPendingWS = false

    for i as long = nFirst to nLast
        if tokens(i).kind = FMTTOK_WS then
            bPendingWS = true
            continue for
        end if
        idx(nCnt) = i
        hadWS(nCnt) = bPendingWS
        bPendingWS = false
        nCnt += 1
    next
    if nCnt = 0 then return ""

    ' A directive line is passed through with its spacing untouched. Its grammar is not the
    ' language's -- "#define FOO(x) ((x)+1)" is a macro body, and normalizing operators
    ' inside it can change what the macro expands to.
    dim as long bDirective = iif( nClass = FMTLINE_DIRECTIVE, true, false )

    dim as string sOut = ""

    for n as long = 0 to nCnt - 1
        dim as long t = idx(n)
        dim as long k = tokens(t).kind
        dim as string sTxt = tokens(t).text

        ' -- casing ----------------------------------------------------------------------
        if k = FMTTOK_WORD then
            select case FMT_ClassifyWord(sTxt)
                case FMTVOCAB_KEYWORD
                    sTxt = FMT_ApplyCase( sTxt, rules.CaseKeywords )
                case FMTVOCAB_TYPE
                    sTxt = FMT_ApplyCase( sTxt, rules.CaseTypes )
            end select
        elseif k = FMTTOK_DIRECTIVE then
            sTxt = FMT_ApplyCase( sTxt, rules.CaseDirectives )
        end if

        ' -- separator -------------------------------------------------------------------
        if n = 0 then
            sOut = sTxt
            continue for
        end if

        dim as long p = idx(n - 1)
        dim as long kPrev = tokens(p).kind
        dim as string sPrev = tokens(p).text

        ' Start from what the user wrote.
        dim as string sSep = iif( hadWS(n), " ", "" )

        ' Two adjacent words must be separated or they fuse into one identifier.
        if (kPrev = FMTTOK_WORD) orelse (kPrev = FMTTOK_NUMBER) then
            if (k = FMTTOK_WORD) orelse (k = FMTTOK_NUMBER) then sSep = " "
        end if

        if bDirective = false then
            ' Comma and semicolon: never a space before, optionally one after.
            if (k = FMTTOK_OP) andalso ((sTxt = ",") orelse (sTxt = ";")) then
                sSep = ""
            elseif (kPrev = FMTTOK_OP) andalso ((sPrev = ",") orelse (sPrev = ";")) then
                if rules.SpaceAfterComma then sSep = " "
            end if

            ' Binary operators.
            if rules.SpaceAroundOperators then
                if (k = FMTTOK_OP) andalso FMT_IsBinaryOp(sTxt) then
                    dim as long bUnary = false
                    if (sTxt = "-") orelse (sTxt = "+") orelse (sTxt = "*") then
                        bUnary = FMT_IsUnaryContext( sPrev, kPrev )
                    end if
                    if bUnary = false then sSep = " "
                end if
                if (kPrev = FMTTOK_OP) andalso FMT_IsBinaryOp(sPrev) then
                    dim as long bPrevUnary = false
                    if (sPrev = "-") orelse (sPrev = "+") orelse (sPrev = "*") then
                        dim as long kPP = -1
                        dim as string sPP = ""
                        if n >= 2 then
                            kPP = tokens(idx(n - 2)).kind
                            sPP = tokens(idx(n - 2)).text
                        end if
                        bPrevUnary = FMT_IsUnaryContext( sPP, kPP )
                    end if
                    if bPrevUnary then
                        sSep = ""
                    else
                        sSep = " "
                    end if
                end if
            end if

            ' Member access and address-of bind tight, always.
            if (k = FMTTOK_OP) andalso ((sTxt = ".") orelse (sTxt = "->")) then sSep = ""
            if (kPrev = FMTTOK_OP) andalso ((sPrev = ".") orelse (sPrev = "->") orelse (sPrev = "@")) then sSep = ""

            ' Parenthesis interiors.
            select case rules.ParenSpacing
                case FMTPAREN_ADD
                    if (sPrev = "(") andalso (sTxt <> ")") then sSep = " "
                    if (sTxt = ")") andalso (sPrev <> "(") then sSep = " "
                case FMTPAREN_REMOVE
                    if sPrev = "(" then sSep = ""
                    if sTxt = ")" then sSep = ""
            end select
        end if

        ' Trailing comment. Applied on every line kind including directives -- the spacing
        ' in front of a comment cannot change what the code does.
        if ((k = FMTTOK_COMMENT) orelse (k = FMTTOK_MLCOMMENT)) andalso (rules.SpacesBeforeComment > 0) then
            sSep = space(rules.SpacesBeforeComment)
        end if

        sOut &= sSep & sTxt
    next

    return sOut
end function


' ========================================================================================
' The indent string for a given depth.
'
' Local rather than modRoutines' FillString, which reaches through gTTabCtl to the active
' document -- this engine has to work with no document open at all.
' ========================================================================================
private function FMT_IndentString( byval nDepth as long, _
                                   byval nWidth as long, _
                                   byval bUseTabs as long ) as string
    if nDepth <= 0 then return ""
    if bUseTabs then return string( nDepth, chr(9) )
    if nWidth <= 0 then nWidth = 4
    return space( nDepth * nWidth )
end function


' ========================================================================================
' The shipped defaults.
'
' Casing ships as LEAVE and the two line-count rules ship OFF, deliberately: a formatter
' that rewrites a user's whole file the first time they press the shortcut, in ways they
' never asked for, is one they switch off and never switch back on. Indentation and
' trailing whitespace are the two rules essentially nobody disagrees with, so those are on.
' ========================================================================================
function Format_DefaultRules() as FORMAT_RULES
    dim as FORMAT_RULES r
    return r
end function


' ========================================================================================
' THE FORMATTER.
'
' The semantic safety check runs here, and a failure returns the ORIGINAL text. A caller
' that ignores bChanged therefore still cannot corrupt a buffer -- which matters, because
' the on-Enter and on-paste hooks run on the typing hot path where nobody is watching.
' ========================================================================================
function Format_TextAtDepth( byref sText as string, _
                             byref rules as FORMAT_RULES, _
                             byval nStartDepth as long, _
                             byref bChanged as long, _
                             byval nIndentWidth as long = 4, _
                             byval bUseTabs as long = false ) as string
    bChanged = false
    if len(sText) = 0 then return sText

    dim as FMTTOKEN tokens(any)
    dim as long nTok = FMT_Lex( sText, tokens() )
    if nTok = 0 then return sText

    ' -- group tokens into lines ---------------------------------------------------------
    dim as long nLineCap = 8
    for i as long = 0 to nTok - 1
        if tokens(i).kind = FMTTOK_EOL then nLineCap += 1
    next
    dim as FMTLINE lines(0 to nLineCap)
    dim as long nLines = 0
    dim as long nCur = 0

    while nCur < nTok
        dim as long nStart = nCur
        while (nCur < nTok) andalso (tokens(nCur).kind <> FMTTOK_EOL)
            nCur += 1
        wend
        lines(nLines).nFirst = nStart
        lines(nLines).nLast = nCur - 1
        if nCur < nTok then
            lines(nLines).sEOL = tokens(nCur).text
            nCur += 1
        else
            lines(nLines).sEOL = ""
        end if
        nLines += 1
    wend

    ' -- classify, and walk the indent stack ---------------------------------------------
    dim as FMTBLOCK stack(0 to nLines + 2)
    dim as long nSP = 0
    dim as long nDepth = nStartDepth
    if nDepth < 0 then nDepth = 0

    dim as long bPrevContinued = false
    dim as long bInML = false

    for n as long = 0 to nLines - 1
        dim as long nFirst = lines(n).nFirst
        dim as long nLast = lines(n).nLast

        lines(n).bInML = bInML

        dim as long bEndsCont = false
        for i as long = nLast to nFirst step -1
            dim as long k = tokens(i).kind
            if (k = FMTTOK_WS) orelse (k = FMTTOK_COMMENT) then continue for
            if k = FMTTOK_CONT then bEndsCont = true
            exit for
        next
        lines(n).bContinued = bEndsCont

        ' Does a block comment run on past the end of this line? The lexer stops an
        ' MLCOMMENT token at the line break, so an unclosed one here means the next line
        ' starts inside the comment.
        for i as long = nFirst to nLast
            if tokens(i).kind = FMTTOK_MLCOMMENT then
                dim as string sC = tokens(i).text
                dim as long nOpen = iif( bInML, 1, 0 )
                dim as long j = 1
                while j <= len(sC) - 1
                    if mid(sC, j, 2) = "/'" then
                        nOpen += 1 : j += 2 : continue while
                    end if
                    if mid(sC, j, 2) = "'/" then
                        nOpen -= 1 : j += 2 : continue while
                    end if
                    j += 1
                wend
                bInML = iif( nOpen > 0, true, false )
            end if
        next

        dim as long bIsSelect = false
        dim as long nClass

        if lines(n).bInML then
            nClass = FMTLINE_MLCOMMENT
        elseif bPrevContinued then
            nClass = FMTLINE_CONTINUED
        else
            nClass = FMT_ClassifyLine( tokens(), nFirst, nLast, bIsSelect )
        end if
        lines(n).nClass = nClass

        ' -- depth for THIS line, and the running depth after it -------------------------
        dim as long nLineDepth = nDepth

        select case nClass
            case FMTLINE_CLOSER
                if nSP > 0 then
                    nSP -= 1
                    nLineDepth = stack(nSP).ownDepth
                    nDepth = stack(nSP).ownDepth
                else
                    ' No opener on our stack. In a FRAGMENT (Format Selection, the paste
                    ' hook) that is the normal case -- the block this line closes was opened
                    ' above the selection -- so step out one level. Clamped at zero, which
                    ' is also what makes a genuinely stray End If in a whole file harmless
                    ' rather than pushing everything after it off the left margin.
                    nLineDepth = nDepth - 1
                    if nLineDepth < 0 then nLineDepth = 0
                    nDepth = nLineDepth
                end if

            case FMTLINE_MIDDLE
                if nSP > 0 then
                    nLineDepth = stack(nSP - 1).ownDepth
                    nDepth = nLineDepth + 1
                end if

            case FMTLINE_CASE
                if (nSP > 0) andalso (stack(nSP - 1).kind = FMTBLK_SELECT) then
                    nLineDepth = stack(nSP - 1).ownDepth
                    if rules.IndentCase then nLineDepth += 1
                    nDepth = nLineDepth + 1
                end if

            case FMTLINE_DIRECTIVE
                ' Pinned to column 0, and depth-NEUTRAL: code inside a #If region indents
                ' exactly as though the directive were not there. That is the only policy
                ' that cannot be confused by a #If whose arms open a block in one branch
                ' and close it in another.
                nLineDepth = 0

            case FMTLINE_CONTINUED
                nLineDepth = nDepth + rules.ContinuationIndent

            case FMTLINE_OPENER
                stack(nSP).kind = iif( bIsSelect, FMTBLK_SELECT, FMTBLK_GENERIC )
                stack(nSP).ownDepth = nLineDepth
                nSP += 1
                nDepth = nLineDepth + 1
        end select

        if nLineDepth < 0 then nLineDepth = 0
        lines(n).nDepth = nLineDepth

        bPrevContinued = bEndsCont
    next

    ' -- render each line ----------------------------------------------------------------
    for n as long = 0 to nLines - 1
        dim as long nClass = lines(n).nClass

        if nClass = FMTLINE_MLCOMMENT then
            ' Inside a block comment the content is prose, not code. Reindenting it would
            ' silently rewrite somebody's ASCII diagram or commented-out block.
            dim as string sRaw = ""
            for i as long = lines(n).nFirst to lines(n).nLast
                sRaw &= tokens(i).text
            next
            lines(n).sOut = sRaw
            continue for
        end if

        if nClass = FMTLINE_BLANK then
            if rules.TrimTrailing then
                lines(n).sOut = ""
            else
                dim as string sRaw = ""
                for i as long = lines(n).nFirst to lines(n).nLast
                    sRaw &= tokens(i).text
                next
                lines(n).sOut = sRaw
            end if
            continue for
        end if

        dim as string sBody = FMT_BuildBody( tokens(), lines(n).nFirst, lines(n).nLast, rules, nClass )

        if rules.TrimTrailing then
            sBody = rtrim( sBody, any chr(32, 9) )
        end if

        if rules.Reindent then
            lines(n).sOut = FMT_IndentString( lines(n).nDepth, nIndentWidth, bUseTabs ) & sBody
        else
            ' Keep the user's own leading whitespace verbatim.
            dim as string sLead = ""
            for i as long = lines(n).nFirst to lines(n).nLast
                if tokens(i).kind <> FMTTOK_WS then exit for
                sLead &= tokens(i).text
            next
            lines(n).sOut = sLead & sBody
        end if
    next

    ' -- blank line policy ---------------------------------------------------------------
    ' The only pass that changes the LINE COUNT, which is why FMT_TokensMatch has to be
    ' told to ignore line breaks whenever either of these two rules is active.
    dim as long bLineCountMayChange = false
    if rules.MaxBlankLines > 0 then bLineCountMayChange = true
    if rules.BlankLinesAroundProc >= 0 then bLineCountMayChange = true

    dim as string sEOLDefault = chr(13) & chr(10)
    for n as long = 0 to nLines - 1
        if len(lines(n).sEOL) > 0 then
            sEOLDefault = lines(n).sEOL
            exit for
        end if
    next

    dim as string sResult = ""

    if bLineCountMayChange = false then
        for n as long = 0 to nLines - 1
            sResult &= lines(n).sOut & lines(n).sEOL
        next
    else
        dim as long nRun = 0
        dim as long nEmitted = 0
        dim as string sPending = ""

        for n as long = 0 to nLines - 1
            dim as long bBlank = false
            if len( trim(lines(n).sOut, any chr(32, 9)) ) = 0 then bBlank = true

            if bBlank then
                nRun += 1
                if (rules.MaxBlankLines > 0) andalso (nRun > rules.MaxBlankLines) then
                    continue for                              ' swallow the surplus
                end if
                dim as string sE = lines(n).sEOL
                if len(sE) = 0 then sE = sEOLDefault
                sPending &= lines(n).sOut & sE
                continue for
            end if

            ' A non-blank line. If it opens a top-level procedure or type and we are not at
            ' the top of the file, guarantee the requested separation in front of it.
            if (rules.BlankLinesAroundProc >= 0) andalso (nEmitted > 0) then
                dim as long bIsProc = false
                if (lines(n).nClass = FMTLINE_OPENER) andalso (lines(n).nDepth = 0) then
                    ' TOP-LEVEL only -- a nested Scope or With is part of a procedure's
                    ' body, not a new section of the file.
                    dim as long nSig = -1
                    for i as long = lines(n).nFirst to lines(n).nLast
                        dim as long k = tokens(i).kind
                        if (k = FMTTOK_WS) orelse (k = FMTTOK_COMMENT) orelse (k = FMTTOK_MLCOMMENT) then continue for
                        nSig = i
                        exit for
                    next
                    if nSig >= 0 then
                        select case lcase(tokens(nSig).text)
                            case "sub", "function", "type", "property", "operator", _
                                 "constructor", "destructor", "public", "private", "static"
                                bIsProc = true
                        end select
                    end if
                end if
                if bIsProc then
                    ' Count the blank lines already carried, and top up to the requested N.
                    dim as long nHave = 0
                    for q as long = 1 to len(sPending)
                        if mid(sPending, q, 1) = chr(10) then nHave += 1
                    next
                    for e as long = nHave + 1 to rules.BlankLinesAroundProc
                        sPending &= sEOLDefault
                    next
                end if
            end if

            sResult &= sPending
            sPending = ""
            sResult &= lines(n).sOut & lines(n).sEOL
            nEmitted += 1
            nRun = 0
        next

        ' Trailing blank lines survive as written -- truncating the end of a file is a
        ' change the user did not ask for and would not notice in a diff hunk.
        sResult &= sPending
    end if

    ' -- safety --------------------------------------------------------------------------
    ' The program must not have changed. For a formatter that never moves a line break,
    ' equality of the non-whitespace token streams is a complete statement of that -- the
    ' cheap equivalent of Black's assert_equivalent, with no AST anywhere.
    if FMT_TokensMatch( sText, sResult, bLineCountMayChange ) = false then
        bChanged = false
        return sText
    end if

    bChanged = iif( sResult <> sText, true, false )
    return sResult
end function


' ========================================================================================
' Whole-text entry point. Adds the STABILITY check, which Format_TextAtDepth deliberately
' does not run: a fragment formatted at a supplied depth is not expected to be a fixed
' point of the whole-file formatter, but a whole file must be.
' ========================================================================================
function Format_Text( byref sText as string, _
                      byref rules as FORMAT_RULES, _
                      byref bChanged as long, _
                      byval nIndentWidth as long = 4, _
                      byval bUseTabs as long = false ) as string
    dim as long bDidChange = false
    dim as string sOnce = Format_TextAtDepth( sText, rules, 0, bDidChange, nIndentWidth, bUseTabs )

    if bDidChange = false then
        bChanged = false
        return sText
    end if

    dim as long bTwice = false
    dim as string sAgain = Format_TextAtDepth( sOnce, rules, 0, bTwice, nIndentWidth, bUseTabs )
    if sAgain <> sOnce then
        ' Not a fixed point. Refuse, rather than hand back a result the user would have to
        ' format a second time to settle -- an unstable formatter produces output nobody
        ' can review, because the next run yields a fresh diff.
        bChanged = false
        return sText
    end if

    bChanged = true
    return sOnce
end function


' ========================================================================================
' Block depth at the start of a given 0-based line. Seeds Format_TextAtDepth for Format
' Selection and for the paste hook, both of which see a fragment whose depth cannot be
' recovered from the fragment itself.
' ========================================================================================
function Format_DepthAtLine( byref sText as string, byval nLine as long ) as long
    if nLine <= 0 then return 0

    dim as FMTTOKEN tokens(any)
    dim as long nTok = FMT_Lex( sText, tokens() )
    if nTok = 0 then return 0

    dim as FMTBLOCK stack(0 to nTok + 2)
    dim as long nSP = 0
    dim as long nDepth = 0
    dim as long nCurLine = 0
    dim as long nCur = 0
    dim as long bPrevContinued = false
    dim as FORMAT_RULES rules = Format_DefaultRules()

    while (nCur < nTok) andalso (nCurLine < nLine)
        dim as long nStart = nCur
        while (nCur < nTok) andalso (tokens(nCur).kind <> FMTTOK_EOL)
            nCur += 1
        wend
        dim as long nLast = nCur - 1
        if nCur < nTok then nCur += 1

        dim as long bEndsCont = false
        for i as long = nLast to nStart step -1
            dim as long k = tokens(i).kind
            if (k = FMTTOK_WS) orelse (k = FMTTOK_COMMENT) then continue for
            if k = FMTTOK_CONT then bEndsCont = true
            exit for
        next

        if bPrevContinued = false then
            dim as long bIsSelect = false
            select case FMT_ClassifyLine( tokens(), nStart, nLast, bIsSelect )
                case FMTLINE_CLOSER
                    if nSP > 0 then
                        nSP -= 1
                        nDepth = stack(nSP).ownDepth
                    end if
                case FMTLINE_MIDDLE
                    if nSP > 0 then nDepth = stack(nSP - 1).ownDepth + 1
                case FMTLINE_CASE
                    if (nSP > 0) andalso (stack(nSP - 1).kind = FMTBLK_SELECT) then
                        nDepth = stack(nSP - 1).ownDepth + iif( rules.IndentCase, 1, 0 ) + 1
                    end if
                case FMTLINE_OPENER
                    stack(nSP).kind = iif( bIsSelect, FMTBLK_SELECT, FMTBLK_GENERIC )
                    stack(nSP).ownDepth = nDepth
                    nSP += 1
                    nDepth += 1
            end select
        end if

        bPrevContinued = bEndsCont
        nCurLine += 1
    wend

    if nDepth < 0 then nDepth = 0
    return nDepth
end function


' ========================================================================================
' ENV-GATED SELF-TEST -- TIKO_FORMAT_SELFTEST=1
'
' WHAT THIS SUITE CANNOT SEE, stated up front rather than left as a flattering number:
' it is PURE. It calls Format_Text and nothing else, so it says nothing whatever about
' whether the menu command is wired up, whether the caret survives, whether Scintilla
' markers are preserved, or whether the dialog's preview pane is connected to this engine
' at all. Stub Format_Document to a no-op and every assertion below stays green. The
' Scintilla half of that evidence lives in modFormatApply's own checks and, mostly, in the
' author's interactive pass.
'
' The assertion most likely to be VACUOUS is the token-preservation one: written against
' whitespace-normalized TEXT rather than against the token stream it passes on a great many
' broken formatters. It is A/B'd for exactly that reason.
' ========================================================================================
function Format_RunSelfTest() as long

    if environ("TIKO_FORMAT_SELFTEST") <> "1" then return 0

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

    dim as string NL = chr(13) & chr(10)
    print "Format engine self-test"

    ' ====================================================================================
    ' 1. THE LEXER IS LOSSLESS.
    '
    ' Every guarantee downstream rests on this one property: concatenating the token texts
    ' must reproduce the input byte for byte. If it does not, the safety check is comparing
    ' something other than the program.
    ' ====================================================================================
    scope
        dim as string samples(0 to 8) = { _
            "dim as long i = 0" & NL & "if i > 1 then" & NL & "print i" & NL & "end if" & NL, _
            "x = -1 : y = a - b" & NL, _
            "s = ""he said """"hi"""" ok""  ' trailing" & NL, _
            "/' block" & NL & "   still block '/ code" & NL, _
            "#include once ""foo.bi""" & NL, _
            "n = &hFF + &b1010 + 1.5e-3" & NL, _
            "call foo( a, _" & NL & "          b )" & NL, _
            "rem an old style comment" & NL & "Print 1" & NL, _
            "" }

        for i as long = 0 to 8
            dim as FMTTOKEN tk(any)
            dim as long n = FMT_Lex( samples(i), tk() )
            dim as string sBack = ""
            for j as long = 0 to n - 1
                sBack &= tk(j).text
            next
            dim as string sName = "lex is lossless on sample " & str(i)
            CHECK( sBack = samples(i), sName )
        next
    end scope

    ' ====================================================================================
    ' 2. REINDENT from block structure.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false

        dim as string sFrom = "dim as long i" & NL & _
                            "if i > 1 then" & NL & _
                            "print i" & NL & _
                            "else" & NL & _
                            "print 0" & NL & _
                            "end if" & NL
        dim as string sWant = "dim as long i" & NL & _
                              "if i > 1 then" & NL & _
                              "    print i" & NL & _
                              "else" & NL & _
                              "    print 0" & NL & _
                              "end if" & NL
        CHECK( Format_Text( sFrom, r, bCh ) = sWant, "If/Else/End If indents one level" )

        ' A SINGLE-LINE If is a complete statement and must not indent what follows it.
        ' This is the classic beautifier bug -- a naive "line contains If" test cascades
        ' every subsequent line one level further right, forever.
        dim as string sIn2 = "if i > 1 then print i" & NL & "print 2" & NL
        CHECK( Format_Text( sIn2, r, bCh ) = sIn2, "single-line If does not open a block" )

        ' Exit For must not read as a For block, and Dim As Sub() must not read as a Sub.
        dim as string sIn3 = "for i = 1 to 10" & NL & "exit for" & NL & "next" & NL
        dim as string sWant3 = "for i = 1 to 10" & NL & "    exit for" & NL & "next" & NL
        CHECK( Format_Text( sIn3, r, bCh ) = sWant3, "Exit For does not open a block" )

        dim as string sIn4 = "dim as sub() p" & NL & "print 1" & NL
        CHECK( Format_Text( sIn4, r, bCh ) = sIn4, "Dim As Sub() does not open a block" )

        ' "Type x As y" is a typedef; "Type x" opens a block. Telling them apart needs the
        ' THIRD word, which is why FMT_ClassifyLine collects three.
        dim as string sIn5 = "type myint as long" & NL & "print 1" & NL
        CHECK( Format_Text( sIn5, r, bCh ) = sIn5, "Type typedef does not open a block" )

        dim as string sIn6 = "type foo" & NL & "as long x" & NL & "end type" & NL
        dim as string sWant6 = "type foo" & NL & "    as long x" & NL & "end type" & NL
        CHECK( Format_Text( sIn6, r, bCh ) = sWant6, "Type block indents its fields" )

        ' A bare "End" terminates the program and closes nothing.
        dim as string sIn7 = "if x then" & NL & "end" & NL & "end if" & NL
        dim as string sWant7 = "if x then" & NL & "    end" & NL & "end if" & NL
        CHECK( Format_Text( sIn7, r, bCh ) = sWant7, "bare End closes no block" )
    end scope

    ' ====================================================================================
    ' 3. SELECT CASE, with and without IndentCase.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false

        dim as string sFrom = "select case x" & NL & _
                            "case 1" & NL & _
                            "foo" & NL & _
                            "case else" & NL & _
                            "bar" & NL & _
                            "end select" & NL

        r.IndentCase = true
        dim as string sWantOn = "select case x" & NL & _
                                "    case 1" & NL & _
                                "        foo" & NL & _
                                "    case else" & NL & _
                                "        bar" & NL & _
                                "end select" & NL
        CHECK( Format_Text( sFrom, r, bCh ) = sWantOn, "Select Case with IndentCase on" )

        r.IndentCase = false
        dim as string sWantOff = "select case x" & NL & _
                                 "case 1" & NL & _
                                 "    foo" & NL & _
                                 "case else" & NL & _
                                 "    bar" & NL & _
                                 "end select" & NL
        CHECK( Format_Text( sFrom, r, bCh ) = sWantOff, "Select Case with IndentCase off" )
    end scope

    ' ====================================================================================
    ' 4. PREPROCESSOR -- pinned to column 0, and depth-neutral.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false

        dim as string sFrom = "sub foo()" & NL & _
                            "    #if defined(__FB_WIN32__)" & NL & _
                            "print 1" & NL & _
                            "    #else" & NL & _
                            "print 2" & NL & _
                            "    #endif" & NL & _
                            "end sub" & NL
        dim as string sWant = "sub foo()" & NL & _
                              "#if defined(__FB_WIN32__)" & NL & _
                              "    print 1" & NL & _
                              "#else" & NL & _
                              "    print 2" & NL & _
                              "#endif" & NL & _
                              "end sub" & NL
        CHECK( Format_Text( sFrom, r, bCh ) = sWant, "# directives pin to column 0, body indents normally" )

        ' A #If whose two arms are UNBALANCED -- one opens a block the other does not. This
        ' is precisely the case that makes treating directives as blocks unworkable, and the
        ' depth-neutral policy is what survives it.
        dim as string sIn2 = "#if A" & NL & "if x then" & NL & "#else" & NL & "if y then" & NL & "#endif" & NL & "foo" & NL & "end if" & NL
        dim as long bCh2 = false
        dim as string sOut2 = Format_Text( sIn2, r, bCh2 )
        CHECK( FMT_TokensMatch( sIn2, sOut2, false ), "unbalanced #If arms do not corrupt the token stream" )
    end scope

    ' ====================================================================================
    ' 5. UNBALANCED BLOCKS must not drive the indent negative.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false
        dim as string sFrom = "end if" & NL & "end if" & NL & "print 1" & NL
        dim as string sOut = Format_Text( sFrom, r, bCh )
        CHECK( instr( sOut, NL & " " ) = 0, "stray End If leaves no negative or phantom indent" )
        CHECK( FMT_TokensMatch( sFrom, sOut, false ), "stray End If preserves the token stream" )
    end scope

    ' ====================================================================================
    ' 6. TABS vs SPACES.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false
        dim as string sFrom = "if x then" & NL & "foo" & NL & "end if" & NL

        dim as string sSpaces = Format_Text( sFrom, r, bCh, 4, false )
        CHECK( instr( sSpaces, NL & "    foo" ) > 0, "spaces mode indents with 4 spaces" )

        dim as string sTwo = Format_Text( sFrom, r, bCh, 2, false )
        CHECK( instr( sTwo, NL & "  foo" ) > 0, "indent width is honoured" )

        dim as string sTabs = Format_Text( sFrom, r, bCh, 4, true )
        CHECK( instr( sTabs, NL & chr(9) & "foo" ) > 0, "tabs mode indents with one tab per level" )
    end scope

    ' ====================================================================================
    ' 7. KEYWORD CASING, split by vocabulary.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        r.Reindent = false
        dim as long bCh = false

        dim as string sFrom = "DIM as LONG i" & NL

        r.CaseKeywords = FMTCASE_LOWER
        r.CaseTypes = FMTCASE_LEAVE
        CHECK( instr( Format_Text( sFrom, r, bCh ), "dim" ) > 0, "keywords lowercase" )

        r.CaseKeywords = FMTCASE_UPPER
        CHECK( instr( Format_Text( sFrom, r, bCh ), "DIM" ) > 0, "keywords uppercase" )

        ' Types are a SEPARATE vocabulary -- styling them differently from control-flow
        ' keywords is the whole point of the four-way split.
        ' PROPER CASE reads its spellings from the keyword files themselves, which carry
        ' canonical casing (ByVal, ScreenRes, MessageBox). Asserted against a REAL word
        ' rather than a synthetic one, because the whole claim is that the formatter and
        ' the editor agree about how a keyword is spelled -- and the editor's source is
        ' this same list.
        r.CaseKeywords = FMTCASE_PROPER
        r.CaseTypes = FMTCASE_LEAVE
        CHECK( instr( Format_Text( "dim byval x" & NL, r, bCh ), "ByVal" ) > 0, _
               "Proper Case uses the keyword file's canonical spelling" )
        ' And the fallback: a word the list holds only in lower case is Capitalized, not
        ' left alone -- a file with two casing conventions in it is worse than either.
        CHECK( instr( Format_Text( "IF x THEN" & NL, r, bCh ), "If" ) > 0, _
               "Proper Case falls back to Capitalized" )

        r.CaseKeywords = FMTCASE_LOWER
        r.CaseTypes = FMTCASE_UPPER
        dim as string sMixed = Format_Text( sFrom, r, bCh )
        CHECK( (instr( sMixed, "dim" ) > 0) andalso (instr( sMixed, "LONG" ) > 0), _
               "types recase independently of keywords" )

        ' Casing must NEVER reach inside a string literal.
        r.CaseKeywords = FMTCASE_UPPER
        r.CaseTypes = FMTCASE_UPPER
        dim as string sStr = "s = ""dim as long"" ' dim as long" & NL
        dim as string sStrOut = Format_Text( sStr, r, bCh )
        CHECK( instr( sStrOut, """dim as long""" ) > 0, "string literal contents are never recased" )
        CHECK( instr( sStrOut, "' dim as long" ) > 0, "comment contents are never recased" )
    end scope

    ' ====================================================================================
    ' 8. SPACING.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        r.Reindent = false
        dim as long bCh = false

        CHECK( Format_Text( "a=b+c" & NL, r, bCh ) = "a = b + c" & NL, "binary operators are spaced" )
        CHECK( Format_Text( "foo(a,b)" & NL, r, bCh ) = "foo(a, b)" & NL, "a space follows a comma" )

        ' UNARY minus. "x = -1" must not become "x = - 1" -- the classic beautifier bug.
        CHECK( Format_Text( "x=-1" & NL, r, bCh ) = "x = -1" & NL, "unary minus after = is not spaced" )
        CHECK( Format_Text( "foo(-1,-2)" & NL, r, bCh ) = "foo(-1, -2)" & NL, "unary minus after ( and , is not spaced" )
        CHECK( Format_Text( "x=a-1" & NL, r, bCh ) = "x = a - 1" & NL, "binary minus after an identifier IS spaced" )
        CHECK( Format_Text( "x=for_count-1" & NL, r, bCh ) = "x = for_count - 1" & NL, _
               "binary minus after an identifier that merely starts with a keyword" )

        ' Pointer dereference is unary too.
        CHECK( Format_Text( "*p=5" & NL, r, bCh ) = "*p = 5" & NL, "leading pointer deref is not spaced" )

        ' An exponent's sign belongs to the literal, not to the expression.
        CHECK( Format_Text( "x=1.5e-3" & NL, r, bCh ) = "x = 1.5e-3" & NL, "exponent sign is part of the number" )

        ' Member access binds tight.
        CHECK( Format_Text( "p->fld=x.y" & NL, r, bCh ) = "p->fld = x.y" & NL, "member access is never spaced" )

        ' Parenthesis interiors, both directions.
        r.ParenSpacing = FMTPAREN_ADD
        CHECK( Format_Text( "foo(a, b)" & NL, r, bCh ) = "foo( a, b )" & NL, "paren spacing: add" )
        CHECK( Format_Text( "foo()" & NL, r, bCh ) = "foo()" & NL, "paren spacing: empty parens stay empty" )
        r.ParenSpacing = FMTPAREN_REMOVE
        CHECK( Format_Text( "foo( a, b )" & NL, r, bCh ) = "foo(a, b)" & NL, "paren spacing: remove" )
        r.ParenSpacing = FMTPAREN_LEAVE

        ' A directive line's spacing is passed through -- normalizing operators inside a
        ' macro body can change what the macro expands to.
        dim as string sMacro = "#define SQ(x) ((x)*(x))" & NL
        CHECK( Format_Text( sMacro, r, bCh ) = sMacro, "directive line spacing is left alone" )

        ' Trailing comment spacing.
        r.SpacesBeforeComment = 2
        CHECK( Format_Text( "x = 1 ' hi" & NL, r, bCh ) = "x = 1  ' hi" & NL, "trailing comment spacing" )
        r.SpacesBeforeComment = 0
    end scope

    ' ====================================================================================
    ' 9. THINGS THE FORMATTER MUST LEAVE ALONE.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false

        ' A block comment's interior is prose -- an ASCII diagram in one must survive.
        dim as string sFrom = "/'" & NL & "      +---+" & NL & "      | x |" & NL & "'/" & NL
        CHECK( Format_Text( sFrom, r, bCh ) = sFrom, "block comment interior is not reindented" )

        ' A continuation line indents relative to its statement.
        r.ContinuationIndent = 1
        dim as string sCont = "if x then" & NL & "foo( a, _" & NL & "b )" & NL & "end if" & NL
        dim as string sWantC = "if x then" & NL & "    foo( a, _" & NL & "        b )" & NL & "end if" & NL
        CHECK( Format_Text( sCont, r, bCh ) = sWantC, "continuation line gets ContinuationIndent extra levels" )

        ' A line the scanner cannot classify passes through rather than aborting the file.
        dim as string sOdd = "if x then" & NL & "@@@ ~~~ ???" & NL & "end if" & NL
        dim as string sOddOut = Format_Text( sOdd, r, bCh )
        CHECK( FMT_TokensMatch( sOdd, sOddOut, false ), "an unclassifiable line survives intact" )
    end scope

    ' ====================================================================================
    ' 10. BLANK LINE RULES -- the only ones that change the line count.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as long bCh = false

        r.MaxBlankLines = 2
        dim as string sFrom = "a" & NL & NL & NL & NL & NL & "b" & NL
        dim as string sWant = "a" & NL & NL & NL & "b" & NL
        CHECK( Format_Text( sFrom, r, bCh ) = sWant, "consecutive blank lines are capped" )
        r.MaxBlankLines = 0

        r.BlankLinesAroundProc = 1
        dim as string sIn2 = "dim as long g" & NL & "sub foo()" & NL & "end sub" & NL
        dim as string sWant2 = "dim as long g" & NL & NL & "sub foo()" & NL & "end sub" & NL
        CHECK( Format_Text( sIn2, r, bCh ) = sWant2, "a blank line is inserted before a top-level Sub" )

        ' Already-separated declarations must not accumulate another blank line on every
        ' run -- that would make the formatter non-idempotent, which Format_Text refuses.
        dim as long bCh3 = false
        CHECK( Format_Text( sWant2, r, bCh3 ) = sWant2, "existing separation is not doubled" )

        ' A NESTED block is part of a procedure body, not a new section of the file.
        dim as string sIn4 = "sub foo()" & NL & "    dim as long i" & NL & "    scope" & NL & "    end scope" & NL & "end sub" & NL
        dim as long bCh4 = false
        dim as string sOut4 = Format_Text( sIn4, r, bCh4 )
        CHECK( instr( sOut4, NL & NL & "    scope" ) = 0, "a nested Scope gets no blank line inserted" )
        r.BlankLinesAroundProc = -1
    end scope

    ' ====================================================================================
    ' 11. IDEMPOTENCE and TOKEN PRESERVATION across a realistic file, under EVERY rule
    '     turned on at once.
    '
    '     format(format(x)) == format(x) is Black's assert_stable, which gofmt and
    '     Uncrustify do not implement at all. Format_Text enforces it internally by
    '     REFUSING an unstable result, so these assertions also prove that refusal has not
    '     silently become the normal path -- which is why bChanged is asserted too.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        r.CaseKeywords = FMTCASE_UPPER
        r.CaseTypes = FMTCASE_LOWER
        r.CaseDirectives = FMTCASE_LOWER
        r.ParenSpacing = FMTPAREN_ADD
        r.SpacesBeforeComment = 2
        r.MaxBlankLines = 2
        r.BlankLinesAroundProc = 1

        dim as string sSrc = _
            "#include once ""windows.bi""" & NL & _
            "" & NL & _
            "type PERSON" & NL & _
            "as zstring*32 name" & NL & _
            "as long age" & NL & _
            "end type" & NL & _
            "function greet( byval p as PERSON ptr ) as long" & NL & _
            "dim as long i,n=0" & NL & _
            "if p=0 then return -1" & NL & _
            "for i=0 to 10" & NL & _
            "select case i" & NL & _
            "case 0" & NL & _
            "n+=1   ' first" & NL & _
            "case else" & NL & _
            "n=n-1" & NL & _
            "end select" & NL & _
            "next" & NL & _
            "print ""done: "" & n" & NL & _
            "return n" & NL & _
            "end function" & NL

        dim as long bCh = false
        dim as string sOnce = Format_Text( sSrc, r, bCh )
        CHECK( bCh, "the realistic sample actually changed (the refusal path is not the normal one)" )

        dim as long bCh2 = false
        dim as string sTwice = Format_Text( sOnce, r, bCh2 )
        CHECK( sTwice = sOnce, "format(format(x)) == format(x) -- byte exact" )
        CHECK( bCh2 = false, "a second format reports no change" )

        ' The semantic check, stated directly rather than relying on Format_Text having run
        ' it internally.
        CHECK( FMT_TokensMatch( sSrc, sOnce, true ), "the non-whitespace token stream is preserved" )

        ' And a NEGATIVE, so the assertion above cannot be passing vacuously: a text that
        ' genuinely differs must be REJECTED by the same comparison. Without this pair, a
        ' FMT_TokensMatch that always returned true would look perfect.
        CHECK( FMT_TokensMatch( sSrc, sSrc & "extra" & NL, true ) = false, _
               "token match REJECTS an added token" )
        CHECK( FMT_TokensMatch( "a = 1" & NL, "a = 2" & NL, false ) = false, _
               "token match REJECTS a changed literal" )
        CHECK( FMT_TokensMatch( "s = ""Hi""" & NL, "s = ""hi""" & NL, false ) = false, _
               "token match REJECTS a recased STRING (case is data, not syntax)" )
        CHECK( FMT_TokensMatch( "DIM x" & NL, "dim x" & NL, false ), _
               "token match ACCEPTS a recased keyword (FreeBASIC is case-insensitive)" )
        CHECK( FMT_TokensMatch( "a" & NL & "b" & NL, "a b" & NL, false ) = false, _
               "token match REJECTS a swallowed line break when not ignoring EOL" )
    end scope

    ' ====================================================================================
    ' 12. THE ENGINE IS PURE. Same input plus same rules must give the same output no
    '     matter what else has happened -- there is no hidden state to accumulate.
    ' ====================================================================================
    scope
        dim as FORMAT_RULES r = Format_DefaultRules()
        dim as string sFrom = "if x then" & NL & "foo" & NL & "end if" & NL
        dim as long b1 = false, b2 = false
        dim as string sA = Format_Text( sFrom, r, b1 )
        dim as string sB = Format_Text( sFrom, r, b2 )
        CHECK( sA = sB, "the engine is deterministic" )
        ' DepthAtLine reports the RUNNING depth entering a line, not the depth that line
        ' will be rendered at. Those differ on every closer, and the distinction is the
        ' whole contract: Format_TextAtDepth is handed the former and works out the latter
        ' itself. (This assertion originally expected 0 on the End If line -- it was named
        ' for a behaviour the function neither has nor should have.)
        CHECK( Format_DepthAtLine( sFrom, 1 ) = 1, "DepthAtLine: inside the If block" )
        CHECK( Format_DepthAtLine( sFrom, 2 ) = 1, "DepthAtLine: entering End If, still inside" )
        CHECK( Format_DepthAtLine( sFrom, 0 ) = 0, "DepthAtLine: 0 at the top of the file" )

        ' And the case that pairing actually exists to serve: a SELECTION that starts in the
        ' middle of a block. Handed the depth entering the fragment, the formatter must
        ' indent the body at that depth and step the closer back out -- even though the
        ' opener is nowhere in the text it was given.
        dim as long bSel = false
        dim as string sFrag = "foo" & NL & "end if" & NL
        dim as string sFragWant = "    foo" & NL & "end if" & NL
        CHECK( Format_TextAtDepth( sFrag, r, 1, bSel ) = sFragWant, _
               "a fragment ending in a closer dedents even with no opener in view" )

        ' The same closer in a WHOLE file, with nothing open, must NOT drag the following
        ' lines leftward -- it is clamped at zero instead.
        dim as long bStray = false
        dim as string sStray = "end if" & NL & "print 1" & NL
        CHECK( Format_TextAtDepth( sStray, r, 0, bStray ) = sStray, _
               "a stray closer at depth 0 is clamped, not decremented" )
    end scope

    dim as string sReport = "Format engine self-test: " & str(nPass) & " passed, " & str(nFail) & " failed"
    print sReport

    return nFail
end function
