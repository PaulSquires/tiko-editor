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
    strCodeTip = PsStrReplace(strCodeTip, "  ", " ") 

    ' If this is only a small CodeTip then don't bother trying
    ' to format it into multiple lines.
    if len(strCodeTip) < 75 then return strCodeTip

    if instr(strCodeTip, "( ") then
        strCodeTip = PsStrReplace(strCodeTip, "( ", "(  ")
    elseif instr(strCodeTip, "(") then
        strCodeTip = PsStrReplace(strCodeTip, "(", "(  ")
    end if
    p = instr(strCodeTip, "(")
    if p then
        if mid(strCodeTip, p + 1, 1) <> ")" andalso mid(strCodeTip, p + 1, 2) <> " )" then
            strCodeTip = PsStrReplace(strCodeTip, "(", "( _" & vblf)
            IF instr(strCodeTip, " )") then
            strCodeTip = PsStrReplace(strCodeTip, ")", "_" & vblf & ")")
            elseif instr(strCodeTip, ")") then
            strCodeTip = PsStrReplace(strCodeTip, ")", " _" & vblf & ")")
            end if
        end if
    end if
    strCodeTip = PsStrReplace(strCodeTip, ", ", ",")
    strCodeTip = PsStrReplace(strCodeTip, " ,", ",")
    strCodeTip = PsStrReplace(strCodeTip, ",", ", _" & vblf & "  ")
    
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
    for i = 0 to PsLen(st) - 1
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
        sLine = PsStrShrink(sLine, chr(32,9))
        if left(ucase(sLine), 5) = "WITH " then
            sWithVariable = PsStrParse(sLine, 2, " ")
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
    sLine = PsStrReplace(sLine, "->", ".")
    
    ' The active document's identity in the symbol database (same naming rule the
    ' buffer scan uses), and the caret line in fbcParser's 1-based line space
    ' (Scintilla lines are 0-based).
    dim as DWSTRING wszDocFile = ScanMgr_GetRootName( pDoc )
    dim as long nCaretLine = pDoc->GetCurrentLineNumber + 1

    dim as long numParts = PsStrParseCount(sLine, ".")
    dim parts(1 to numParts) as DWSTRING
    for i as long = 1 to numParts
        parts(i) = PsStrParse(sLine, i, ".")
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
        if PsLen(sName) then codeTip = FindIntrinsicCalltip( sName )
    end if

    if len(codeTip) then
        codeTip = FormatCodetip(codeTip)
        ' Was SCI_CALLTIPSHOW. Now a tiko-owned PsTooltip so the tip is drawn in the
        ' editor theme (Scintilla's calltip was never themed at all -- there is no
        ' SCI_CALLTIPSETBACK call anywhere in this app) and so dismissal is ours to
        ' decide. curPos is the position of the "(", which is what the tip anchors to
        ' and what the caret must not move back past. See modCodetipTip.bi.
        return Codetip_Show( pDoc, codeTip, curPos )
    else
        pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
    end if
    
    return false
end function

    
' The scratch buffer every candidate build fills. Module-level rather than a local
' because it holds AUTOCOMPLETE_MAX_ITEMS records of three DWSTRINGs each: as a local it
' would construct and destruct 3000 dynamic strings on every keystroke that opens a
' popup. frmAutoComplete_Show takes its own copy, so reusing this between builds is safe.
dim shared gAutoCBuild(0 to AUTOCOMPLETE_MAX_ITEMS - 1) as AUTOC_ITEM


' ========================================================================================
' Append one candidate, deduped on its uppercased name.
'
' This replaces the old ExistsInAutocompleteList, which searched a growing pipe-delimited
' string with instr - quadratic in BOTH the item count and the accumulated string length.
' ========================================================================================
sub AddAutocompleteItem( _
            items() as AUTOC_ITEM, _
            byref nItems as long, _
            byref wszName as DWSTRING, _
            byref wszDetail as DWSTRING, _
            byval nKind as long, _
            byval bHasParams as boolean = false _
            )
    if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit sub
    if len(PsRTrim(wszName)) = 0 then exit sub

    dim as DWSTRING wszSort = PsUCase(wszName)
    for i as long = 0 to nItems - 1
        if items(i).wszSort = wszSort then exit sub
    next

    items(nItems).wszName     = wszName
    items(nItems).wszSort     = wszSort
    items(nItems).wszDetail   = wszDetail
    items(nItems).nKind       = nKind
    items(nItems).bHasParams  = bHasParams
    nItems += 1
end sub


' ========================================================================================
' Does a calltip signature carry any parameters? Everything between the first "(" and
' the last ")"; blank means a no-argument call like "Foo()".
'
' Used for the FB intrinsics, whose parameter lists exist only as pre-formatted calltip
' TEXT (gFBIntrinsics) -- parsed symbols answer the same question structurally through
' gSymDb.ProcHasParams.
' ========================================================================================
function CalltipHasParams( byref wszCalltip as DWSTRING ) as boolean
    dim as long p = PsInStr( wszCalltip, "(" )
    if p = 0 then return false
    dim as long q = PsInStrRev( wszCalltip, ")" )
    if q <= p then return false
    return ( len(trim(PsMid( wszCalltip, p + 1, q - p - 1 ))) > 0 )
end function


' ========================================================================================
' Add a parsed symbol, snapshotting everything the popup will need to draw it.
'
' Nothing that reaches the popup may point back into the symbol database: a background
' parse completing installs a new result set and frees the displaced one, so the
' SYMBOLREF - and the wstring ptrs hanging off it - would dangle while the popup was
' still open. Hence the copies here.
' ========================================================================================
sub AddAutocompleteSymbol( _
            items() as AUTOC_ITEM, _
            byref nItems as long, _
            byval r as SYMBOLREF _
            )
    dim as wstring ptr pName = gSymDb.SymName( r )
    if pName = 0 then exit sub

    ' The detail column: for a FUNCTION this is the return type, for a variable, field or
    ' parameter its declared type. A SUB and a bare TYPE have none, and get an empty
    ' column rather than a placeholder.
    dim as DWSTRING wszDetail
    dim as wstring ptr pType = gSymDb.SymTypeText( r )
    if pType then wszDetail = *pType

    ' Only a callable can be accepted by "(", so only a callable is asked.
    dim as long nKind = gSymDb.SymKind( r )
    dim as boolean bHasParams = false
    select case nKind
        case FBCP_KIND_SUB, FBCP_KIND_FUNCTION
            bHasParams = gSymDb.ProcHasParams( r )
    end select

    AddAutocompleteItem( items(), nItems, *pName, wszDetail, nKind, bHasParams )
end sub


' ========================================================================================
' Sort the built candidate set alphabetically, case-insensitively (wszSort is already
' the uppercased name). Scintilla did this for us via SCI_AUTOCSETORDER.
'
' Note the BUILD order still matters even though the display order is alphabetical: it
' decides who survives the AUTOCOMPLETE_MAX_ITEMS cap, which is why locals and the
' guaranteed keyword list are added before the capped global enumeration.
' ========================================================================================
sub SortAutocompleteItems( items() as AUTOC_ITEM, byval lo as long, byval hi as long )
    if lo >= hi then exit sub
    dim as long i = lo, j = hi
    dim as DWSTRING pivot = items( (lo + hi) \ 2 ).wszSort
    do
        while items(i).wszSort < pivot : i += 1 : wend
        while items(j).wszSort > pivot : j -= 1 : wend
        if i <= j then
            swap items(i), items(j)
            i += 1 : j -= 1
        end if
    loop while i <= j
    if lo < j then SortAutocompleteItems( items(), lo, j )
    if hi > i then SortAutocompleteItems( items(), i, hi )
end sub


' ========================================================================================
' The keyword table: gConfig.FBKeywords + gConfig.ExtraKeywords split into words ONCE.
'
' It used to be re-split on every popup, and that was the multi-second delay before a
' list appeared. extra_keywords.txt is ~220 KB / 12,500 words, and the old code
'   - ran PsStrReplaceAny over the whole buffer to normalize CR/LF/TAB (MEASURED
'     2026-08-03: 2.9 s -- it is three MID() passes over 220,000 characters), then
'   - called PsStrParse once per word. That rescans from character 1 every call, AND
'     takes its argument byref as a CONST WSTRING, so each call also converted the whole
'     220 KB ANSI buffer to UTF-16.
' Together: 3.1 s to reach the 1000-item cap on "dim d as " + space, and far worse with a
' typed prefix, where almost nothing matches, the cap is never hit and all 12,500 fields
' are walked.
'
' The uppercased form and the has-parameters answer are resolved here too, so a popup
' costs one allocation-free prefix test per word and nothing else.
' ========================================================================================
type AUTOC_KEYWORD
    wszName    as DWSTRING
    wszUp      as DWSTRING     ' uppercased, so the prefix test allocates nothing
    bHasParams as boolean
end type

redim shared gKwCache(any) as AUTOC_KEYWORD
dim shared gKwCacheCount as long = -1      ' -1 = never built
dim shared gKwCacheSrcFB as string         ' the sources it was built from, for the
dim shared gKwCacheSrcEx as string         ' rebuild test below


' Split one whitespace-separated word list onto the end of the cache. Scans the ANSI
' bytes directly -- the keyword files are plain ASCII word lists, and going through
' wstring here is what made the old path quadratic.
private sub AutoC_AppendKeywordSource( byref sSrc as string )
    dim as long n = len(sSrc)
    dim as long p = 0
    while p < n
        while p < n
            dim as ubyte ch = sSrc[p]
            if (ch = 32) orelse (ch = 13) orelse (ch = 10) orelse (ch = 9) then
                p += 1
            else
                exit while
            end if
        wend
        dim as long s = p
        while p < n
            dim as ubyte ch = sSrc[p]
            if (ch = 32) orelse (ch = 13) orelse (ch = 10) orelse (ch = 9) then exit while
            p += 1
        wend
        if p > s then
            if gKwCacheCount > ubound(gKwCache) then
                redim preserve gKwCache( 0 to (gKwCacheCount * 2) + 15 )
            end if
            dim as string st = mid( sSrc, s + 1, p - s )
            gKwCache(gKwCacheCount).wszName = st
            gKwCache(gKwCacheCount).wszUp   = PsUCase(st)
            ' An FB intrinsic (LEN, MID, INSTR, ...) is a keyword that takes arguments, so
            ' "(" should accept it just as it accepts a parsed proc. Ordinary keywords
            ' (IF, DIM, ...) have no intrinsic calltip and answer false.
            gKwCache(gKwCacheCount).bHasParams = CalltipHasParams( FindIntrinsicCalltip( st ) )
            gKwCacheCount += 1
        end if
    wend
end sub


' Rebuild only when the config strings have actually changed, so an edit in
' Options > Keywords still takes effect on the next popup with nothing to invalidate by
' hand. The comparison is a memcmp of two 220 KB buffers -- microseconds against the
' seconds the split itself costs.
private sub AutoC_EnsureKeywordCache()
    dim as boolean bSame = (gKwCacheCount >= 0)
    if bSame then bSame = (gKwCacheSrcFB = gConfig.FBKeywords)
    if bSame then bSame = (gKwCacheSrcEx = gConfig.ExtraKeywords)
    if bSame then exit sub

    erase gKwCache
    redim gKwCache( 0 to 255 )
    gKwCacheCount = 0
    AutoC_AppendKeywordSource( gConfig.FBKeywords )
    AutoC_AppendKeywordSource( gConfig.ExtraKeywords )
    gKwCacheSrcFB = gConfig.FBKeywords
    gKwCacheSrcEx = gConfig.ExtraKeywords
end sub


' Case-insensitive prefix test with NO temporary string: both sides are already
' uppercase (wszUp is built that way, and wszMatchWord is uppercase everywhere it is
' produced), so this is a straight character walk. left() would allocate a DWSTRING per
' word, 12,500 of them per popup.
'
' The pointers are USHORT PTR, not WSTRING PTR. Indexing a wstring ptr does NOT yield the
' code unit -- p[i] reads as 0 for every i, so a prefix walk written that way silently
' matches nothing and every keyword vanishes from the popup. Verified 2026-08-03: over
' "GETWINDOWRECT", pw[0..4] = 0,0,0,0,0 where the same buffer through a ushort ptr reads
' 71,69,84,87,73.
private function AutoC_HasPrefixUp( _
            byref wszUp as DWSTRING, _
            byref wszPrefix as DWSTRING, _
            byval nLen as long _
            ) as boolean
    if PsLen(wszUp) < nLen then return false
    dim as ushort ptr pUp  = cast(ushort ptr, wszUp.vptr)
    dim as ushort ptr pPre = cast(ushort ptr, wszPrefix.vptr)
    if (pUp = 0) orelse (pPre = 0) then return false
    for i as long = 0 to nLen - 1
        if pUp[i] <> pPre[i] then return false
    next
    return true
end function


' ========================================================================================
' Append the always-available FB keywords and data types to an autocomplete list.
' These are the words loaded at startup from settings\keywords\freebasic_keywords.txt
' and extra_keywords.txt (gConfig.FBKeywords / gConfig.ExtraKeywords). They are a
' guaranteed baseline: the popup must never depend on these being discovered by the
' parser in the sources or includes.
' ========================================================================================
sub AppendKeywordsToList( _
            items() as AUTOC_ITEM, _
            byref nItems as long, _
            byref wszMatchWord as DWSTRING _
            )
    AutoC_EnsureKeywordCache()
    dim as long nLenMatchWord = PsLen(wszMatchWord)
    for i as long = 0 to gKwCacheCount - 1
        if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit sub
        dim as boolean bMatch = true
        if nLenMatchWord then bMatch = AutoC_HasPrefixUp( gKwCache(i).wszUp, wszMatchWord, nLenMatchWord )
        if bMatch then
            AddAutocompleteItem( items(), nItems, gKwCache(i).wszName, "", _
                                 AUTOC_KIND_KEYWORD, gKwCache(i).bHasParams )
        end if
    next
end sub


' ========================================================================================
' Show Autocomplete list
' ========================================================================================
function ShowAutocompleteList( byval Notification as long = 0) as boolean
    
    if gConfig.AutoComplete = false then exit function

    dim as long curPos, nCol, nLenMatchWord
    dim as string st, sDot
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
    if frmAutoComplete_IsOpen() then
        if nLenMatchWord = 0 then
            frmAutoComplete_Cancel()
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
    if (notification = AUTOCOMPLETE_NOTIFY_CHARDELETED) then
        bShowPopup = true
    else
        dim as boolean bNotOpen = (frmAutoComplete_IsOpen() = false)
        if bNotOpen andalso (pDoc->AutoCompleteType = AUTOCOMPLETE_NONE) then
            bShowPopup = true
        end if
    end if

    if bShowPopup = true then
        ' Comment out any comment line so the popup isn't activated when in a comment.
        sLine = MaskStringCharacters(sLine)
        sLine = RemoveComments(sLine)
        if (notification <> AUTOCOMPLETE_NOTIFY_CHARDELETED) then
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
            frmAutoComplete_Cancel()
            exit function
        end if
    end if

    ' The candidate set. gAutoCBuild is a shared scratch buffer; nItems is its logical
    ' length. The ADD order below is deliberate - it decides who survives the
    ' AUTOCOMPLETE_MAX_ITEMS cap - even though the popup ends up sorted alphabetically.
    dim as long nItems

    select case pDoc->AutoCompleteType

        case AUTOCOMPLETE_DIM_AS

            ' Only things that can legally FOLLOW "as" belong here: the builtin data
            ' types below, plus the parsed TYPE / UNION / ENUM / TYPEDEF names further
            ' down. The FB keyword files are deliberately NOT offered - after "as", IF,
            ' PRINT, GOSUB and the other ~10,000 words in them are noise, and they used
            ' to fill the 1000-item cap before the type names got a chance at it.
            if bTypesOnly = false then
                ' The standard builtin data types. "any" and "const" are not types but
                ' they are legal in this position ("as any ptr", "as const long"), and
                ' they used to reach the popup only through the keyword files.
                dim as string sStdTypes = _
                    "any|const|" & _
                    "boolean|byte|ubyte|short|ushort|integer|uinteger|long|ulong|longint|ulongint|single|" & _
                    "double|string|wstring|zstring|true|false|DWSTRING"
                dim as long nCount = PsStrParseCount(sStdTypes, "|")
                for i as long = 1 to nCount
                    st = PsStrParse(sStdTypes, i, "|")
                    dim as boolean bMatch = true
                    if nLenMatchWord then bMatch = ( left(PsUCase(st), nLenMatchWord) = pDoc->wszMatchWord )
                    if bMatch then
                        AddAutocompleteItem( gAutoCBuild(), nItems, st, "", AUTOC_KIND_STDTYPE )
                    end if
                next
            end if

            ' All TYPE/UNION/ENUM/TYPEDEF names matching the typed prefix. For
            ' CONSTRUCTOR/DESTRUCTOR completion (bTypesNotPreloaded) only offer
            ' types defined outside the toolchain include folder.
            '
            ' EnumPrefix returns user-file symbols ahead of the toolchain's, so the
            ' AUTOCOMPLETE_MAX_ITEMS cap below truncates toolchain types rather than the
            ' project's own. Do not re-order here - that policy lives in one place.
            dim rs() as SYMBOLREF
            dim as long n = gSymDb.EnumPrefix( pDoc->wszMatchWord, SYMMASK_TYPE, rs() )
            dim as DWSTRING wszIncPath
            if bTypesNotPreloaded then wszIncPath = gSymDb.ToolchainIncludeRoot()
            for i as long = 0 to n - 1
                if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit for
                if PsLen(wszIncPath) then
                    dim as wstring ptr pFile = gSymDb.SymFile( rs(i) )
                    if pFile = 0 then continue for
                    if ucase(left(*pFile, PsLen(wszIncPath))) = wszIncPath then continue for
                end if
                AddAutocompleteSymbol( gAutoCBuild(), nItems, rs(i) )
            next


        case AUTOCOMPLETE_TYPE
            ' Resolve the expression before the "." / "->" and enumerate the
            ' members of its TYPE (fields and member procs; the EXTENDS chain is
            ' walked inside EnumMembers).
            dim as SYMBOLREF refExpr = DereferenceLine( pDoc, sDot, curPos - 1 )
            dim as SYMBOLREF refType = SymRefTypeOf( refExpr )

            ' TIKO_MEMBER_TRACE=1: name which of the four steps failed. This chain gives
            ' the user NOTHING when it breaks -- no popup, no message -- and every step
            ' can fail for a different reason (the file missing from both sets, the caret
            ' outside any proc body, the variable not scanned yet, the TYPE suppressed by
            ' the merge masks), so a silent "return false" is untraceable from outside.
            if environ("TIKO_MEMBER_TRACE") = "1" then
                dim as string sT = "MEMBER: file=" & ScanMgr_GetRootName( pDoc ) & _
                    " line=" & str(pDoc->GetCurrentLineNumber + 1) & " trigger='" & sDot & "'"
                if refExpr.IsValid then
                    sT &= "  expr=OK " & *gSymDb.SymName(refExpr)
                    dim as wstring ptr pTT = gSymDb.SymTypeText(refExpr)
                    if pTT then sT &= " type='" & *pTT & "'"
                else
                    sT &= "  expr=MISS"
                end if
                if refType.IsValid then
                    sT &= "  resolved=OK " & *gSymDb.SymName(refType)
                else
                    sT &= "  resolved=MISS"
                end if
                dim as long nBuf = 0, nProj = 0
                if gSymDb.BufferSet() then
                    if gSymDb.BufferSet()->pResult then nBuf = gSymDb.BufferSet()->pResult->symbolCount
                end if
                if gSymDb.ProjectSet() then
                    if gSymDb.ProjectSet()->pResult then nProj = gSymDb.ProjectSet()->pResult->symbolCount
                end if
                sT &= "  bufferSyms=" & str(nBuf) & " projectSyms=" & str(nProj)
                print sT
            end if

            if refType.IsValid = false then return false

            dim rs() as SYMBOLREF
            dim as long n = gSymDb.EnumMembers( refType, rs() )
            if environ("TIKO_MEMBER_TRACE") = "1" then print "  members=" & str(n)
            for i as long = 0 to n - 1
                if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit for
                st = *gSymDb.SymName( rs(i) )
                dim as boolean bMatch = true
                if nLenMatchWord then bMatch = ( left(PsUCase(st), nLenMatchWord) = pDoc->wszMatchWord )
                if bMatch then
                    AddAutocompleteSymbol( gAutoCBuild(), nItems, rs(i) )
                end if
            next


        case AUTOCOMPLETE_WORD
            ' Recompute the match word as a strict identifier: the shared scan at
            ' the top of this function stops only at "." ">" and space, so on
            ' rebuilds it can pick up "(" and other punctuation.
            dim as string sWordPrefix = GetTrailingIdentifier( sLine )
            if len(sWordPrefix) = 0 then
                frmAutoComplete_Cancel()
                exit function
            end if
            pDoc->wszMatchWord = sWordPrefix
            nLenMatchWord = len(sWordPrefix)

            dim as DWSTRING wszDocFile = ScanMgr_GetRootName( pDoc )
            dim as long nCaretLine = pDoc->GetCurrentLineNumber + 1
            dim rs() as SYMBOLREF

            ' Locals and params of the enclosing proc first
            dim as long n = gSymDb.EnumLocalsInScope( wszDocFile, nCaretLine, rs() )
            for i as long = 0 to n - 1
                st = *gSymDb.SymName( rs(i) )
                dim as boolean bMatch = ( left(PsUCase(st), nLenMatchWord) = pDoc->wszMatchWord )
                if bMatch then
                    AddAutocompleteSymbol( gAutoCBuild(), nItems, rs(i) )
                end if
            next

            ' Always-available FB keywords/data types from the keyword files,
            ' independent of anything the parser found. After locals (most
            ' relevant first for the dedupe) but before the capped global enum.
            AppendKeywordsToList( gAutoCBuild(), nItems, pDoc->wszMatchWord )

            ' Then global procs, types, variables, constants and enum values.
            ' Locals of OTHER procs also carry kind VAR/CONST - skip them via the
            ' LOCAL flag (this scope's locals were already added above).
            dim as long wordMask = SYMMASK_PROC or SYMMASK_TYPE or SYMMASK_VAR or (1 shl FBCP_KIND_ENUMVAL)
            n = gSymDb.EnumPrefix( pDoc->wszMatchWord, wordMask, rs() )
            for i as long = 0 to n - 1
                if nItems >= AUTOCOMPLETE_MAX_ITEMS then exit for
                if (gSymDb.SymFlags( rs(i) ) and FBCP_SYMBFLAG_LOCAL) <> 0 then continue for
                AddAutocompleteSymbol( gAutoCBuild(), nItems, rs(i) )
            next

    end select

    if nItems > 0 then
        ' Alphabetical, case-insensitive - what SCI_AUTOCSETORDER used to do for us.
        SortAutocompleteItems( gAutoCBuild(), 0, nItems - 1 )
        ' Save the code editor starting position
        pDoc->AutoCStartPos = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0)
        frmAutoComplete_Show( pDoc, gAutoCBuild(), nItems )
        return true
    else
        if (notification <> AUTOCOMPLETE_NOTIFY_CHARDELETED) then
            frmAutoComplete_Cancel()
        end if
    end if

    function = true

end function


' ========================================================================================
' Recompute pDoc->wszMatchWord - the prefix an OPEN popup filters on - from the caret.
'
' Two rules, matching the two the rebuild path uses: in AUTOCOMPLETE_WORD mode the word
' is a strict identifier, everywhere else it is whatever runs back from the caret to a
' "." ">" or space (so "g.Foo" yields "FOO", the member prefix).
' ========================================================================================
function RefreshAutocompleteMatchWord( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then return 0

    dim as HWND hEdit   = pDoc->hWndActiveScintilla
    dim as long curPos  = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0)
    dim as long nCol    = SciExec(hEdit, SCI_GETCOLUMN, curPos, 0)
    dim as string sLine = left( pDoc->GetLine(pDoc->GetCurrentLineNumber), nCol )

    if pDoc->AutoCompleteType = AUTOCOMPLETE_WORD then
        pDoc->wszMatchWord = GetTrailingIdentifier( sLine )
    else
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
    end if

    ' The commit path derives the word's start as AutoCStartPos - PsLen(wszMatchWord), so
    ' the two must be updated together or the replacement range drifts left one character
    ' per keystroke. The old rebuild-every-key path got this for free.
    pDoc->AutoCStartPos = curPos

    return len(pDoc->wszMatchWord)
end function


' ========================================================================================
' Keep an OPEN popup in sync with continued typing or a backspace.
'
' This is the filter-in-place path: the candidate set was built once when the popup
' opened, so a keystroke only re-runs the prefix test over it - no second trip through
' gSymDb. Returns FALSE when the popup was closed (nothing left to show).
' ========================================================================================
function UpdateAutocompleteFilter() as boolean
    if frmAutoComplete_IsOpen() = false then return false

    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then
        frmAutoComplete_Cancel()
        return false
    end if

    ' Backspacing past the "." or "->" that opened a member list ends the completion
    ' rather than widening it back out. Only the member modes carry a meaningful
    ' AutoCTriggerStartPos - the word and "dim as" modes fall back to the empty-prefix
    ' test below.
    dim as boolean bMemberMode = (pDoc->AutoCompleteType = AUTOCOMPLETE_TYPE)
    if bMemberMode then
        dim as long curPos = SciExec( pDoc->hWndActiveScintilla, SCI_GETCURRENTPOS, 0, 0 )
        if curPos <= pDoc->AutoCTriggerStartPos then
            frmAutoComplete_Cancel()
            pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
            return false
        end if
    end if

    dim as long nLen = RefreshAutocompleteMatchWord( pDoc )

    ' In word mode an empty prefix means the caret is no longer inside an identifier.
    dim as boolean bWordMode = (pDoc->AutoCompleteType = AUTOCOMPLETE_WORD)
    if bWordMode andalso (nLen = 0) then
        frmAutoComplete_Cancel()
        pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
        return false
    end if

    if frmAutoComplete_ReFilter( pDoc ) = false then
        frmAutoComplete_Cancel()
        pDoc->AutoCompleteType = AUTOCOMPLETE_NONE
        return false
    end if

    return true
end function

