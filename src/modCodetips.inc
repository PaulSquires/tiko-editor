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

#include once "modCodetips.bi"
#include once "clsDocument.bi"


' ========================================================================================
' Format the codetip prior to showing it
' ========================================================================================
function FormatCodetip( byval strCodeTip as string ) as string
    dim as long p

    ' remove multiple duplicate spaces 
    strCodeTip = AfxStrReplace(strCodeTip, "  ", " ") 

    ' If this is only a small CodeTip then don't bother trying
    ' to format it into multiple lines.
    if len(strCodeTip) < 75 then return strCodeTip

    if instr(strCodeTip, "( ") then
        strCodeTip = AfxStrReplace(strCodeTip, "( ", "(  ")
    elseif instr(strCodeTip, "(") then
        strCodeTip = AfxStrReplace(strCodeTip, "(", "(  ")
    end if
    p = instr(strCodeTip, "(")
    if p then
        if mid(strCodeTip, p + 1, 1) <> ")" andalso mid(strCodeTip, p + 1, 2) <> " )" then
            strCodeTip = AfxStrReplace(strCodeTip, "(", "( _" & vblf)
            IF instr(strCodeTip, " )") then
            strCodeTip = AfxStrReplace(strCodeTip, ")", "_" & vblf & ")")
            elseif instr(strCodeTip, ")") then
            strCodeTip = AfxStrReplace(strCodeTip, ")", " _" & vblf & ")")
            end if
        end if
    end if
    strCodeTip = AfxStrReplace(strCodeTip, ", ", ",")
    strCodeTip = AfxStrReplace(strCodeTip, " ,", ",")
    strCodeTip = AfxStrReplace(strCodeTip, ",", ", _" & vblf & "  ")
    
    function = strCodeTip
end function


' ========================================================================================
' Change everything between quotes to # symbols
' ========================================================================================
function MaskStringCharacters( byval st as string) as string
    ' Iterate the line and change everything between quotes to # symbols. This
    ' ensures that we correctly deal with strings that have embedded single 
    ' quote characters.
    dim as long i 
    dim as boolean binstring = false
    for i = 0 to len(st) - 1
        if st[i] = 34 then binstring = not(binstring)
        if binstring then  
            if st[i] <> 34 then st[i] = 35   ' # symbol
        end if
    NEXT
    function = st
end function


' ========================================================================================
' Removes a single line comment.
' ========================================================================================
function RemoveComments( byval st as string) as string
    function = AfxStrExtract( 1, st, "'")
end function  


' ========================================================================================
' Determine what variable relates to the "With" statement
' ========================================================================================
function DetermineWithVariable( byval pDoc as clsDocument ptr) as string
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    dim as long nCurLine = pDoc->GetCurrentLineNumber - 1
    dim as string sLine, sWithVariable 
    
    for i as long = nCurLine to 0 step -1
        sLine = ltrim(pDoc->GetLine(i))
        ' Remove double spaces and replace TABs with single space
        sLine = AfxStrShrink(sLine, chr(32,9))
        if left(ucase(sLine), 5) = "WITH " then
            sWithVariable = AfxStrParse(sLine, 2, " ")
            exit for
        end if
    next
    
    function = sWithVariable
end function    


' ========================================================================================
' Resolve a SYMBOLREF to the TYPE whose members can be enumerated: a TYPE/UNION/ENUM
' ref is its own type; anything else (variable, param, field, member function)
' resolves through its type text (e.g. "CLSDOCUMENT PTR" -> the CLSDOCUMENT record).
' ========================================================================================
function SymRefTypeOf( byval r as SYMBOLREF ) as SYMBOLREF
    dim as SYMBOLREF refMiss
    refMiss.idx = -1
    if r.IsValid = false then return refMiss
    select case gSymDb.SymKind( r )
        case FBCP_KIND_TYPE, FBCP_KIND_UNION, FBCP_KIND_ENUM
            return r
    end select
    dim as wstring ptr pTypeText = gSymDb.SymTypeText( r )
    if pTypeText = 0 then return refMiss
    return gSymDb.ResolveTypeText( *pTypeText )
end function


' ========================================================================================
' The identifier (letters, digits, underscore) that ends sText, uppercased.
' Returns "" if the text does not end in an identifier character.
' ========================================================================================
function GetTrailingIdentifier( byref sText as string ) as string
    dim as long nStart = len(sText) + 1
    for i as long = len(sText) to 1 step -1
        dim as long ch = asc(mid(sText, i, 1))
        dim as boolean bIdent = ((ch >= 65) andalso (ch <= 90)) orelse _
                                ((ch >= 97) andalso (ch <= 122)) orelse _
                                ((ch >= 48) andalso (ch <= 57)) orelse (ch = 95)
        if bIdent = false then exit for
        nStart = i
    next
    if nStart > len(sText) then return ""
    function = ucase(mid(sText, nStart))
end function


' ========================================================================================
' Take the current line and determine what variable is being referenced.
' Dereferences expressions like:  g.MyFunction, g.ot.rc, g->MyFunction
' Used for determining what popup autocomplete list or codetip to display.
' Returns the SYMBOLREF of the last element that resolved: each hop resolves the
' current element's TYPE and looks the next part up as a member of it. A final
' part that fails to resolve is treated as a partially typed word and the previous
' element's ref is returned (the autocomplete rebuild path passes such lines in).
' ========================================================================================
function DereferenceLine( _
            byval pDoc as clsDocument ptr, _
            byval sTrigger as string, _
            byval nPosition as long _
            ) as SYMBOLREF

    dim as SYMBOLREF refMiss
    refMiss.idx = -1

    if pDoc = 0 then return refMiss

    dim as HWND hEdit   = pDoc->hWndActiveScintilla
    dim as long curPos  = nPosition  
    dim as long nCol    = SciExec(hEdit, SCI_GETCOLUMN, curPos, 0) 
    dim as string sLine = trim(left(pDoc->GetLine(pDoc->GetCurrentLineNumber), nCol+1), any chr(32,9))

    ' Comment out any comment line so the popup isn't activated when in a comment. 
    sLine = MaskStringCharacters(sLine)
    sLine = RemoveComments(sLine)

    ' Test to ensure that the incoming sTrigger character still exists after
    ' the commenting of the line. If it does then we know that the line
    ' is good and we can proceed to dereference elements on the line.
    ' The identifer that triggers an autocomplete list for a TYPE variable
    ' is either a dot "." or a pointer dereference "->"
    if right(sLine, len(sTrigger)) <> sTrigger then return refMiss
    sLine = rtrim(sLine, sTrigger)


    ' Proceed to parse the line. Find the beginning of the line in order to
    ' isolate the expression to dereference. The expression might contain an
    ' array reference so that would have to be removed, however the "(" can
    ' also signify the start position of the expression. For example:
    ' st = myFunction(myvar(3).myvalue)
    ' Simply reverse searching for the "(" would fail because the array "()"
    ' also needs to be taken into account. Therefore, search in reverse and
    ' take note of when the ")" is encountered in order to then match it with
    ' an "(".
    ' Need to alo handle situations where there are multiple () array references
    ' in the expression. For example:  ListView1.Item(10).rc(5)
    dim as long nStart, nEnd
    do
        do
            nEnd = instrRev(sLine, ")", -1)
            if nEnd = 0 then exit do
            nStart = instrRev(sLine, "(", nEnd)
            if nStart = 0 then exit do
            ' Remove any array parenthesis.
            sLine = left(sLine, nStart-1) & mid(sLine, nEnd+1)
        LOOP
        if (nStart = 0) or (nEnd = 0) then exit do
    loop
    
    ' All of the array parenthesis should now be removed so now we can simply
    ' search for the start of the line.
    dim as long i = instrRev(sLine, any " (*[&@", -1)
    if i then sLine = mid(sLine, i+1)
    
    ' Need to check if this line is part of a WITH / END WITH block.
    if (len(sLine) = 0) orelse (left(sLine, 1) = ".") then
        sLine = DetermineWithVariable(pDoc) & sLine
    end if
    
    ' Make it easier to parse by converting line to uppercase
    sLine = ucase(sLine)
    
    ' Convert all "->" pointer references to "." dots to make parsing easier
    sLine = AfxStrReplace(sLine, "->", ".")
    
    ' The active document's identity in the symbol database (same naming rule the
    ' buffer scan uses), and the caret line in fbcParser's 1-based line space
    ' (Scintilla lines are 0-based).
    dim as DWSTRING wszDocFile = ScanMgr_GetRootName( pDoc )
    dim as long nCaretLine = pDoc->GetCurrentLineNumber + 1

    dim as long numParts = AfxStrParseCount(sLine, ".")
    dim parts(1 to numParts) as DWSTRING
    for i as long = 1 to numParts
        parts(i) = AfxStrParse(sLine, i, ".")
    next

    dim as SYMBOLREF refCur = refMiss

    for curPart as long = 1 to numParts
        dim as SYMBOLREF refNext = refMiss

        if curPart = 1 then
            ' Scope-aware variable first: locals/params of the enclosing proc
            ' (including the implicit THIS), then module-level of this file, then
            ' SHARED anywhere. Fall back to a proc name, then a TYPE name (for
            ' static accesses like MYTYPE.member).
            refNext = gSymDb.FindVariable( parts(1), wszDocFile, nCaretLine )
            if refNext.IsValid = false then refNext = gSymDb.FindProc( parts(1) )
            if refNext.IsValid = false then refNext = gSymDb.FindType( parts(1) )
        else
            dim as SYMBOLREF refType = SymRefTypeOf( refCur )
            if refType.IsValid then
                refNext = gSymDb.FindMemberOf( refType, parts(curPart) )
            end if
        end if

        if refNext.IsValid = false then
            if curPart = numParts then return refCur
            return refMiss
        end if
        refCur = refNext
    next

    return refCur

end function

    
' ========================================================================================
' Show codetips
' ========================================================================================
function ShowCodetip( _
            byval pDoc as clsDocument ptr, _
            byval bCommaTrigger as boolean = false _
            ) as boolean

    if gConfig.CodeTips = false then exit function

    dim as string codeTip
    dim as HWND hEdit  = pDoc->hWndActiveScintilla
    dim as long curPos = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0)

    if bCommaTrigger then
        ' It is possible that a codetip had been displayed but the user moved off of the
        ' current line and thereby closed the codetip. If user goes back to the line and
        ' presses a comma to enter another parameter for function, then we should do a 
        ' test to see if we can redisplay the codetip popup again.
        ' Need to also handle situations where there are multiple () array references
        ' in the expression. For example:  ListView1.Item(10).rc(5)
        dim as long nCol = SciExec(hEdit, SCI_GETCOLUMN, curPos, 0) 
        dim as string sLine = rtrim(left(pDoc->GetLine(pDoc->GetCurrentLineNumber), nCol+1))
        dim as boolean bInClosed
        for i as long = len(sLine) -1 to 0 step -1
            if sLine[i] = asc(")") then 
            bInClosed = true: continue for
            end if
            if sLine[i] = asc("(") then
                if bInClosed = true then
                    bInClosed = false
                else
                    exit for
                end if
            end if         
            curPos = curPos - 1
        next
    end if

    dim as SYMBOLREF refProc = DereferenceLine(pDoc, "(", curPos-1)
    if refProc.IsValid then
        select case gSymDb.SymKind( refProc )
            case FBCP_KIND_SUB, FBCP_KIND_FUNCTION
                codeTip = gSymDb.BuildCalltip( refProc )
        end select
    end if

    if len(codeTip) = 0 then
        ' FB intrinsic fallback (LEN, MID, ...): the identifier before the "("
        dim as long nParenCol = SciExec(hEdit, SCI_GETCOLUMN, curPos-1, 0)
        dim as string sName = GetTrailingIdentifier( left(pDoc->GetLine(pDoc->GetCurrentLineNumber), nParenCol) )
        if len(sName) then codeTip = FindIntrinsicCalltip( sName )
    end if

    if len(codeTip) then
        codeTip = FormatCodetip(codeTip)
        SciExec( hEdit, SCI_CALLTIPSHOW, curPos, strptr(codeTip))
        return true
    else
        pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
    end if
    
    return false
end function

    
' ========================================================================================
' Display the actual Autocomplete popup list window
' ========================================================================================
function ShowAutoCompletePopup( _
            byval hEdit as HWND, _
            byref sList as string _
            ) as Long
    if len(sList) = 0 then exit function
    dim as string sFillups = "(=."
                                
    SciExec(hEdit, SCI_AUTOCSETFILLUPS, 0, strptr(sFillups))  ' characters that also select and close the popup
    SciExec(hEdit, SCI_AUTOCSETSEPARATOR, 124, 0)             ' Pipe symbol as separator
    SciExec(hEdit, SCI_AUTOCSETORDER, 1, 0)                   ' List must be sorted alphabetically
    SciExec(hEdit, SCI_AUTOCSETIGNORECASE, 1, 0)
    SciExec(hEdit, SCI_AUTOCSETMAXHEIGHT, 9, 0)
    SciExec(hEdit, SCI_AUTOCSHOW, 0, strptr(sList))
    SciExec(hEdit, SCI_AUTOCSETOPTIONS, SC_AUTOCOMPLETE_FIXED_SIZE, 0)

    ' Ensure that the edit window has focus because an Output window such as Compiler Output
    ' or Notes may have had edit focus. When a popup code completion activates then that 
    ' other edit window could possibly receive the keyboard input
    SetFocus( hEdit )
                            
    function = 0
end function


' ========================================================================================
' Don't add duplicates in the Autocomplete list    
' ========================================================================================
function ExistsInAutocompleteList( _
            byref sList as string, _
            byref sMatch as string _
            ) as boolean
    if rtrim(sMatch) = "" then return true
    dim as string sLookFor = "|" & ucase(sMatch) & "|"
    if instr( "|" & ucase(sList) & "|", sLookFor ) then return true
    return false
end function


' ========================================================================================
' Append the always-available FB keywords and data types to an autocomplete list.
' These are the words loaded at startup from settings\keywords\freebasic_keywords.txt
' and extra_keywords.txt (gConfig.FBKeywords / gConfig.ExtraKeywords). They are a
' guaranteed baseline: the popup must never depend on these being discovered by the
' parser in the sources or includes. Reads gConfig live, so edits made in the
' Options keywords dialogs take effect on the next popup with no cache to invalidate.
' ========================================================================================
sub AppendKeywordsToList( _
            byref sList as string, _
            byref wszMatchWord as DWSTRING, _
            byref nItems as long _
            )
    ' Both files are plain whitespace-separated word lists; normalize CR/LF/TAB
    ' to spaces so a single space-parse walks every word.
    dim as string sWords = AfxStrReplaceAny( _
            gConfig.FBKeywords & " " & gConfig.ExtraKeywords, _
            chr(13) & chr(10) & chr(9), " " )
    dim as long nLenMatchWord = len(wszMatchWord)
    dim as long nCount = AfxStrParseCount(sWords, " ")
    for i as long = 1 to nCount
        if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit sub
        dim as string st = AfxStrParse(sWords, i, " ")
        if len(st) = 0 then continue for
        dim as boolean bMatch = true
        if nLenMatchWord then bMatch = ( left(ucase(st), nLenMatchWord) = wszMatchWord )
        if bMatch then
            if ExistsInAutocompleteList(sList, st) = false then
                sList = sList & st & "|"
                nItems += 1
            end if
        end if
    next
end sub


' ========================================================================================
' Show Autocomplete list
' ========================================================================================
function ShowAutocompleteList( byval Notification as long = 0) as boolean
    
    if gConfig.AutoComplete = false then exit function

    dim as long curPos, nCol, nLenMatchWord
    dim as string sList, st, sDot
    dim as boolean bTypesOnly, bTypesNotPreloaded

    dim pDoc as clsDocument ptr

    pDoc = gTTabCtl.GetActiveDocumentPtr()
    If pDoc = 0 then exit function
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla
                
    ' Retrieve the position
    curPos = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0) 
    nCol   = SciExec(hEdit, SCI_GETCOLUMN, curPos, 0) 
    
    dim as string sLine = left( pDoc->GetLine(pDoc->GetCurrentLineNumber), nCol )
    dim as string sLine_ucase

    ' Get the current entered word. We get the text between the "." or ">" and the current
    ' editing position. We do this rather than pDoc->GetWord because text may already
    ' exist after the current editing position.
    pDoc->wszMatchWord = ""
    for i as long = len(sLine) to 1 step -1
        dim as string ch = mid(sLine, i, 1)
        select case ch
            case ".", ">", " "
            exit for
            case else
               pDoc->wszMatchWord = ucase(ch) & pDoc->wszMatchWord   
        end select
    next
    nLenMatchWord = len(pDoc->wszMatchWord)

    ' If an autocomplete is active but now there is no match word then
    ' a space or tab must have been pressed so autocomplete cancel and now exit.
    if SciExec(hEdit, SCI_AUTOCACTIVE, 0, 0) then
        if nLenMatchWord = 0 then 
            SciExec( hEdit, SCI_AUTOCCANCEL, 0, 0)
            exit function
        end if   
    end if


    ' Get the styling of the current line to determine if we are in a 
    ' multiline or single line comment block then abort the autoinsert.
    select case SciExec(hEdit, SCI_GETSTYLEAT, curPos, 0)
        case SCE_B_MULTILINECOMMENT, SCE_B_COMMENT
            exit function
    end select

    ' If no active autocomplete then create the list based on the current underlying match word.
    ' also continue to display the popup if we have simply backspaced while a popup is open.
    dim as boolean bShowPopup = false
    if (notification = SCN_AUTOCCHARDELETED) then
        bShowPopup = true
    else   
        if (SciExec(hEdit, SCI_AUTOCACTIVE, 0, 0) = 0) andalso _
            (pDoc->AutoCompleteType = AUTOCOMPLETE_NONE) then
            bShowPopup = true
        end if
    end if

    if bShowPopup = true then     
        ' Comment out any comment line so the popup isn't activated when in a comment.
        sLine = MaskStringCharacters(sLine)
        sLine = RemoveComments(sLine)
        if (notification <> SCN_AUTOCCHARDELETED) then
            pDoc->wszMatchWord = ""
            nLenMatchWord = 0         
        end if
        sLine_ucase = ucase(sLine)

        if right(sLine_ucase, 4) = " AS " then 
            ' okay
            bTypesOnly = false
            bTypesNotPreloaded = false  ' all types
            pDoc->AutoCompleteType = AUTOCOMPLETE_DIM_AS

        elseif right(sLine_ucase, 1) = "." then 
            sDot = "."  ' okay
            pDoc->AutoCompleteType = AUTOCOMPLETE_TYPE

        elseif right(sLine_ucase, 2) = "->" then
            sDot = "->"   
            pDoc->AutoCompleteType = AUTOCOMPLETE_TYPE

        elseif right(sLine_ucase, 1) = "(" then 
            ShowCodetip(pDoc)
            exit function 

        elseif right(sLine_ucase, 1) = "," then 
            ShowCodetip(pDoc, true)
            exit function 

        elseif right(sLine_ucase, 5) = " NEW " then 
            ' okay
            bTypesOnly = false
            bTypesNotPreloaded = false  ' all types
            pDoc->AutoCompleteType = AUTOCOMPLETE_DIM_AS

        elseif right(sLine_ucase, 12) = "CONSTRUCTOR " then 
            ' okay
            bTypesOnly = true
            bTypesNotPreloaded = true  ' only TYPES that are in our source.
            pDoc->AutoCompleteType = AUTOCOMPLETE_DIM_AS

        elseif right(sLine_ucase, 11) = "DESTRUCTOR " then 
            ' okay
            bTypesOnly = true
            bTypesNotPreloaded = true
            pDoc->AutoCompleteType = AUTOCOMPLETE_DIM_AS

        elseif right(sLine_ucase, 9) = " EXTENDS " then
            ' okay
            bTypesOnly = true
            bTypesNotPreloaded = false  ' all types
            pDoc->AutoCompleteType = AUTOCOMPLETE_DIM_AS

        elseif pDoc->AutoCompleteType = AUTOCOMPLETE_NONE then
            ' Bare identifier: complete from symbols and locals in scope once at
            ' least two characters have been typed. Skip when the word follows a
            ' "." or "->" (that context belongs to the member autocomplete), and
            ' never override a sticky mode on a backspace rebuild.
            dim as string sWordPrefix = GetTrailingIdentifier( sLine )
            if len(sWordPrefix) >= 2 then
                dim as string sBefore = left(sLine, len(sLine) - len(sWordPrefix))
                dim as boolean bMemberContext = (right(sBefore, 1) = ".") orelse (right(sBefore, 2) = "->")
                dim as long chFirst = asc(left(sWordPrefix, 1))
                dim as boolean bStartsWithDigit = (chFirst >= 48) andalso (chFirst <= 57)
                dim as boolean bWordContext = (bMemberContext = false) andalso (bStartsWithDigit = false)
                if bWordContext then
                    pDoc->wszMatchWord = sWordPrefix
                    nLenMatchWord = len(sWordPrefix)
                    pDoc->AutoCompleteType = AUTOCOMPLETE_WORD
                end if
            end if
        end if

        ' Check if it is a "FOR i AS LONG" type of statement
        if instr(sLine, "FOR ") then
            SciExec( hEdit, SCI_AUTOCCANCEL, 0, 0)
            exit function
        end if
    end if
    
    sList = "|"

    select case pDoc->AutoCompleteType

        case AUTOCOMPLETE_DIM_AS
            dim as long nItems

            if bTypesOnly = false then
                ' The standard builtin data types
                dim as string sStdTypes = _
                    "boolean|byte|ubyte|short|ushort|integer|uinteger|long|ulong|longint|ulongint|single|" & _
                    "double|string|wstring|zstring|true|false|DWSTRING"
                dim as long nCount = AfxStrParseCount(sStdTypes, "|")
                for i as long = 1 to nCount
                    st = AfxStrParse(sStdTypes, i, "|")
                    dim as boolean bMatch = true
                    if nLenMatchWord then bMatch = ( left(ucase(st), nLenMatchWord) = pDoc->wszMatchWord )
                    if bMatch then
                        if ExistsInAutocompleteList(sList, st) = false then
                            sList = sList & st & "|"
                        end if
                    end if
                next
            end if

            if bTypesOnly = false then
                ' Always offer every FB keyword and data type from the keyword
                ' files - added BEFORE the parsed symbols so the guaranteed words
                ' can never be crowded out by the item cap in a large project.
                AppendKeywordsToList( sList, pDoc->wszMatchWord, nItems )
            end if

            ' All TYPE/UNION/ENUM/TYPEDEF names matching the typed prefix. For
            ' CONSTRUCTOR/DESTRUCTOR completion (bTypesNotPreloaded) only offer
            ' types defined outside the toolchain include folder.
            dim rs() as SYMBOLREF
            dim as long n = gSymDb.EnumPrefix( pDoc->wszMatchWord, SYMMASK_TYPE, rs() )
            dim as DWSTRING wszIncPath
            if bTypesNotPreloaded andalso (len(gConfig.FBWINCompiler64) > 0) then
                wszIncPath = ucase( AfxStrPathname( "PATH", ProcessFromCurdriveApp( gConfig.FBWINCompiler64 ) ) & "inc" )
            end if
            for i as long = 0 to n - 1
                if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit for
                if len(wszIncPath) then
                    dim as wstring ptr pFile = gSymDb.SymFile( rs(i) )
                    if pFile = 0 then continue for
                    if ucase(left(*pFile, len(wszIncPath))) = wszIncPath then continue for
                end if
                st = *gSymDb.SymName( rs(i) )
                if ExistsInAutocompleteList(sList, st) = false then
                    sList = sList & st & "|"
                    nItems += 1
                end if
            next


        case AUTOCOMPLETE_TYPE
            ' Resolve the expression before the "." / "->" and enumerate the
            ' members of its TYPE (fields and member procs; the EXTENDS chain is
            ' walked inside EnumMembers).
            dim as SYMBOLREF refExpr = DereferenceLine( pDoc, sDot, curPos - 1 )
            dim as SYMBOLREF refType = SymRefTypeOf( refExpr )
            if refType.IsValid = false then return false

            dim rs() as SYMBOLREF
            dim as long n = gSymDb.EnumMembers( refType, rs() )
            dim as long nItems
            for i as long = 0 to n - 1
                if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit for
                st = *gSymDb.SymName( rs(i) )
                dim as boolean bMatch = true
                if nLenMatchWord then bMatch = ( left(ucase(st), nLenMatchWord) = pDoc->wszMatchWord )
                if bMatch then
                    if ExistsInAutocompleteList(sList, st) = false then
                        sList = sList & st & "|"
                        nItems += 1
                    end if
                end if
            next


        case AUTOCOMPLETE_WORD
            ' Recompute the match word as a strict identifier: the shared scan at
            ' the top of this function stops only at "." ">" and space, so on
            ' rebuilds it can pick up "(" and other punctuation.
            dim as string sWordPrefix = GetTrailingIdentifier( sLine )
            if len(sWordPrefix) = 0 then
                SciExec( hEdit, SCI_AUTOCCANCEL, 0, 0)
                exit function
            end if
            pDoc->wszMatchWord = sWordPrefix
            nLenMatchWord = len(sWordPrefix)

            dim as DWSTRING wszDocFile = ScanMgr_GetRootName( pDoc )
            dim as long nCaretLine = pDoc->GetCurrentLineNumber + 1
            dim rs() as SYMBOLREF
            dim as long nItems

            ' Locals and params of the enclosing proc first
            dim as long n = gSymDb.EnumLocalsInScope( wszDocFile, nCaretLine, rs() )
            for i as long = 0 to n - 1
                st = *gSymDb.SymName( rs(i) )
                dim as boolean bMatch = ( left(ucase(st), nLenMatchWord) = pDoc->wszMatchWord )
                if bMatch then
                    if ExistsInAutocompleteList(sList, st) = false then
                        sList = sList & st & "|"
                        nItems += 1
                    end if
                end if
            next

            ' Always-available FB keywords/data types from the keyword files,
            ' independent of anything the parser found. After locals (most
            ' relevant first for the dedupe) but before the capped global enum.
            AppendKeywordsToList( sList, pDoc->wszMatchWord, nItems )

            ' Then global procs, types, variables, constants and enum values.
            ' Locals of OTHER procs also carry kind VAR/CONST - skip them via the
            ' LOCAL flag (this scope's locals were already added above).
            dim as long wordMask = SYMMASK_PROC or SYMMASK_TYPE or SYMMASK_VAR or (1 shl FBCP_KIND_ENUMVAL)
            n = gSymDb.EnumPrefix( pDoc->wszMatchWord, wordMask, rs() )
            for i as long = 0 to n - 1
                if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit for
                if (gSymDb.SymFlags( rs(i) ) and FBCP_SYMBFLAG_LOCAL) <> 0 then continue for
                st = *gSymDb.SymName( rs(i) )
                if ExistsInAutocompleteList(sList, st) = false then
                    sList = sList & st & "|"
                    nItems += 1
                end if
            next

    end select
    
    sList = trim(sList, any "| ")
    if len(sList) then
        ' Save the code editor starting position 
        pDoc->AutoCStartPos = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0)
        ShowAutoCompletePopup(hEdit, sList)
        return true
    else
        if (notification <> SCN_AUTOCCHARDELETED) then
            SciExec( hEdit, SCI_AUTOCCANCEL, 0, 0)
        end if   
    end if 

    function = true

end function

