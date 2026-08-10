'' ========================================================================================
'' mdParse.inc -- the implementation. See mdParse.bi for what this is a port OF and why.
''
'' EVERY MATCHER BELOW IS ONE REGEX FROM mdlite.py, NAMED AFTER IT. FreeBASIC has no regex,
'' so each is hand-written, and the comment above it carries the pattern it implements --
'' because the only way to review this file is against that one.
''
'' ONE GLOBAL RULE: all delimiters are ASCII and every UTF-8 continuation byte is >= 0x80,
'' so byte-wise scanning can never land inside a multi-byte sequence. That is what makes an
'' 8-bit scan over UTF-8 text safe here, and it stops being true the moment a non-ASCII
'' delimiter is added.
'' ========================================================================================

#pragma once
#include once "mdParse.bi"


'' ---------------------------------------------------------------------------------------
'' character classes
'' ---------------------------------------------------------------------------------------
private function MdIsWs( byval ch as long ) as boolean
    '' \s -- space, tab, CR, LF, FF, VT. The line splitter has already removed CR and LF,
    '' but the class is written out in full so a matcher used on raw text is still right.
    return (ch = 32) orelse (ch = 9) orelse (ch = 13) orelse (ch = 10) orelse _
           (ch = 12) orelse (ch = 11)
end function

private function MdIsWordCh( byval ch as long ) as boolean
    '' [A-Za-z0-9_] -- the class both \b and the _emphasis_ lookarounds test against.
    return ((ch >= 48) andalso (ch <= 57)) orelse _
           ((ch >= 65) andalso (ch <= 90)) orelse _
           ((ch >= 97) andalso (ch <= 122)) orelse (ch = 95)
end function

private function MdIsLangCh( byval ch as long ) as boolean
    '' [A-Za-z0-9_+-] -- the fence info string.
    return MdIsWordCh(ch) orelse (ch = 43) orelse (ch = 45)
end function

'' Byte at i, or -1 past either end. Every lookaround in this file goes through this, so an
'' index one off the end reads as "no character" rather than reading the terminator.
private function MdChAt( byref s as string, byval i as long ) as long
    if (i < 0) orelse (i >= len(s)) then return -1
    return s[i]
end function

'' Leading whitespace, in characters.
private function MdLeadWs( byref s as string ) as long
    dim as long i = 0
    while (i < len(s)) andalso MdIsWs(s[i])
        i += 1
    wend
    return i
end function

'' Leading SPACES only -- \x20, not \t. RE_INDENTED_CODE is ` {4,}` and a tab does not count.
private function MdLeadSp( byref s as string ) as long
    dim as long i = 0
    while (i < len(s)) andalso (s[i] = 32)
        i += 1
    wend
    return i
end function

private function MdTrimWs( byref s as string ) as string
    dim as long a = 0, b = len(s) - 1
    while (a <= b) andalso MdIsWs(s[a])
        a += 1
    wend
    while (b >= a) andalso MdIsWs(s[b])
        b -= 1
    wend
    if b < a then return ""
    return mid(s, a + 1, b - a + 1)
end function

'' .strip("\n") -- newlines only, which is what the fence body uses. Leading and trailing
'' SPACES inside a code block are content and must survive.
private function MdTrimNl( byref s as string ) as string
    dim as long a = 0, b = len(s) - 1
    while (a <= b) andalso (s[a] = 10)
        a += 1
    wend
    while (b >= a) andalso (s[b] = 10)
        b -= 1
    wend
    if b < a then return ""
    return mid(s, a + 1, b - a + 1)
end function

private function MdLowerAsc( byref s as string ) as string
    dim as string r = s
    for i as long = 0 to len(r) - 1
        if (r[i] >= 65) andalso (r[i] <= 90) then r[i] += 32
    next
    return r
end function


'' ---------------------------------------------------------------------------------------
'' the arenas
''
'' Doubling growth, and the first redim happens on the first Add because a `(any)` array
'' starts with ubound -1. fbc zeroes neither New nor a stack Dim -- but `redim` on an array
'' of a UDT DOES run its default constructor for the new elements, which is what keeps the
'' `string` fields valid.
'' ---------------------------------------------------------------------------------------
sub MdDocClear( byref doc as MdDoc )
    doc.nBlk  = 0
    doc.nInl  = 0
    doc.nCell = 0
    doc.nNote = 0
    '' The arrays keep their capacity on purpose: the viewer re-parses on every navigation,
    '' and giving the storage back each time would make every page load a fresh set of
    '' allocations. The stale contents past the counts are never read.
end sub

private function AddInl( byref doc as MdDoc, byval nStyle as long, _
                         byref sText as string, byref sHref as string ) as long
    if doc.nInl > ubound(doc.inl) then
        redim preserve doc.inl(0 to (doc.nInl + 1) * 2)
    end if
    with doc.inl(doc.nInl)
        .nStyle = nStyle
        .sText  = sText
        .sHref  = sHref
        .bBreak = false
    end with
    doc.nInl += 1
    return doc.nInl - 1
end function

private function AddBlk( byref doc as MdDoc, byval kind as long, byval nQuote as long ) as long
    if doc.nBlk > ubound(doc.blk) then
        redim preserve doc.blk(0 to (doc.nBlk + 1) * 2)
    end if
    with doc.blk(doc.nBlk)
        .kind = kind : .nLevel = 0 : .nQuote = nQuote
        .bOrdered = false : .nOrdinal = 0 : .bHeaderRow = false
        .nFirstInl = 0 : .nInlCount = 0
        .nFirstCell = 0 : .nCellCount = 0
        .sText = "" : .sLang = "" : .sAnchor = ""
    end with
    doc.nBlk += 1
    return doc.nBlk - 1
end function

private function AddCell( byref doc as MdDoc, byval nFirst as long, byval nCount as long ) as long
    if doc.nCell > ubound(doc.cell) then
        redim preserve doc.cell(0 to (doc.nCell + 1) * 2)
    end if
    doc.cell(doc.nCell).nFirstInl = nFirst
    doc.cell(doc.nCell).nInlCount = nCount
    doc.nCell += 1
    return doc.nCell - 1
end function

private sub AddNote( byref doc as MdDoc, byval nLine as long, _
                     byref sWhat as string, byref sLine as string )
    if doc.nNote > ubound(doc.note) then
        redim preserve doc.note(0 to (doc.nNote + 1) * 2)
    end if
    doc.note(doc.nNote).nLine = nLine
    doc.note(doc.nNote).sWhat = sWhat
    doc.note(doc.nNote).sLine = sLine
    doc.nNote += 1
end sub


'' ---------------------------------------------------------------------------------------
'' block matchers -- one per regex, in mdlite's own order
'' ---------------------------------------------------------------------------------------

'' RE_HEADING = ^(#{1,6})\s+(.*?)\s*#*\s*$
'' No leading \s*: a heading starts in column 0. `#{1,6}` then `\s+` is why "#foo" and a
'' seven-hash line are both paragraphs -- count at most six, then DEMAND whitespace.
private function MatchHeading( byref sLine as string, byref nLevel as long, _
                               byref sRest as string ) as boolean
    dim as long i = 0
    while (i < len(sLine)) andalso (sLine[i] = 35) andalso (i < 6)
        i += 1
    wend
    if i = 0 then return false
    if (i >= len(sLine)) orelse (MdIsWs(sLine[i]) = false) then return false
    nLevel = i
    while (i < len(sLine)) andalso MdIsWs(sLine[i])
        i += 1
    wend
    dim as string sBody = mid(sLine, i + 1)
    '' The tail is `\s*#*\s*$`, applied in that order: trailing space, then closing hashes,
    '' then the space between the text and them.
    sBody = MdTrimWs(sBody)
    dim as long b = len(sBody) - 1
    while (b >= 0) andalso (sBody[b] = 35)
        b -= 1
    wend
    sBody = left(sBody, b + 1)
    sRest = MdTrimWs(sBody)
    return true
end function

'' RE_FENCE = ^\s*(`{3,}|~{3,})\s*([A-Za-z0-9_+-]*)\s*$
private function MatchFence( byref sLine as string, byref chMark as long, _
                             byref nMarkLen as long, byref sLang as string ) as boolean
    dim as long i = MdLeadWs(sLine)
    if i >= len(sLine) then return false
    dim as long ch = sLine[i]
    if (ch <> 96) andalso (ch <> 126) then return false      '' ` or ~
    dim as long nStart = i
    while (i < len(sLine)) andalso (sLine[i] = ch)
        i += 1
    wend
    if (i - nStart) < 3 then return false
    chMark = ch
    nMarkLen = i - nStart
    while (i < len(sLine)) andalso MdIsWs(sLine[i])
        i += 1
    wend
    dim as long nLangStart = i
    while (i < len(sLine)) andalso MdIsLangCh(sLine[i])
        i += 1
    wend
    sLang = mid(sLine, nLangStart + 1, i - nLangStart)
    while (i < len(sLine)) andalso MdIsWs(sLine[i])
        i += 1
    wend
    if i <> len(sLine) then return false
    return true
end function

'' RE_TABLE_ROW = ^\s*\|
private function IsTableRow( byref sLine as string ) as boolean
    dim as long i = MdLeadWs(sLine)
    if i >= len(sLine) then return false
    return (sLine[i] = 124)
end function

'' RE_TABLE_SEP = ^\s*\|[\s:|-]+\|?\s*$
'' The trailing \|? is redundant -- | is already inside the class -- so this reduces to:
'' after the first bar there must be at least one character and every one must be in the set.
private function IsTableSep( byref sLine as string ) as boolean
    dim as long i = MdLeadWs(sLine)
    if (i >= len(sLine)) orelse (sLine[i] <> 124) then return false
    i += 1
    if i >= len(sLine) then return false
    for j as long = i to len(sLine) - 1
        dim as long c = sLine[j]
        if (MdIsWs(c) = false) andalso (c <> 58) andalso (c <> 124) andalso (c <> 45) then
            return false
        end if
    next
    return true
end function

'' RE_HR = ^\s*(-{3,}|\*{3,}|_{3,})\s*$
'' All three or more of ONE character. `- - -` is not a rule under this pattern, and neither
'' is `--*`; both fall through to the list matcher and the paragraph respectively.
private function IsHr( byref sLine as string ) as boolean
    dim as long i = MdLeadWs(sLine)
    if i >= len(sLine) then return false
    dim as long ch = sLine[i]
    if (ch <> 45) andalso (ch <> 42) andalso (ch <> 95) then return false
    dim as long nStart = i
    while (i < len(sLine)) andalso (sLine[i] = ch)
        i += 1
    wend
    if (i - nStart) < 3 then return false
    while (i < len(sLine)) andalso MdIsWs(sLine[i])
        i += 1
    wend
    return (i = len(sLine))
end function

'' RE_UL = ^(\s*)[*+-]\s+(.*)$
private function MatchUl( byref sLine as string, byref nIndent as long, _
                          byref sRest as string ) as boolean
    dim as long i = MdLeadWs(sLine)
    if i >= len(sLine) then return false
    dim as long ch = sLine[i]
    if (ch <> 42) andalso (ch <> 43) andalso (ch <> 45) then return false
    dim as long nBullet = i
    i += 1
    if (i >= len(sLine)) orelse (MdIsWs(sLine[i]) = false) then return false
    while (i < len(sLine)) andalso MdIsWs(sLine[i])
        i += 1
    wend
    nIndent = nBullet
    sRest = mid(sLine, i + 1)
    return true
end function

'' RE_OL = ^(\s*)\d+[.)]\s+(.*)$
private function MatchOl( byref sLine as string, byref nIndent as long, _
                          byref sRest as string ) as boolean
    dim as long i = MdLeadWs(sLine)
    dim as long nStart = i
    while (i < len(sLine)) andalso (sLine[i] >= 48) andalso (sLine[i] <= 57)
        i += 1
    wend
    if i = nStart then return false
    if i >= len(sLine) then return false
    if (sLine[i] <> 46) andalso (sLine[i] <> 41) then return false     '' . or )
    i += 1
    if (i >= len(sLine)) orelse (MdIsWs(sLine[i]) = false) then return false
    while (i < len(sLine)) andalso MdIsWs(sLine[i])
        i += 1
    wend
    nIndent = nStart
    sRest = mid(sLine, i + 1)
    return true
end function

'' RE_INDENTED_CODE = ^ {4,}\S
private function IsIndentedCode( byref sLine as string ) as boolean
    dim as long i = MdLeadSp(sLine)
    if i < 4 then return false
    if i >= len(sLine) then return false
    return (MdIsWs(sLine[i]) = false)
end function

'' RE_BLOCKQUOTE = ^ {0,3}>\s?(.*)$
private function MatchBlockquote( byref sLine as string, byref sRest as string ) as boolean
    dim as long i = MdLeadSp(sLine)
    if i > 3 then return false
    if (i >= len(sLine)) orelse (sLine[i] <> 62) then return false
    i += 1
    '' \s? -- ONE optional space, not a run. The rest of the indentation is content.
    if (i < len(sLine)) andalso MdIsWs(sLine[i]) then i += 1
    sRest = mid(sLine, i + 1)
    return true
end function

'' _starts_block
private function StartsBlock( byref sLine as string ) as boolean
    dim as long nL, nI, nMark, nMarkLen
    dim as string sTmp
    if MatchHeading(sLine, nL, sTmp) then return true
    if MatchFence(sLine, nMark, nMarkLen, sTmp) then return true
    if IsTableRow(sLine) then return true
    if IsHr(sLine) then return true
    if MatchUl(sLine, nI, sTmp) then return true
    if MatchOl(sLine, nI, sTmp) then return true
    if IsIndentedCode(sLine) then return true
    if MatchBlockquote(sLine, sTmp) then return true
    return false
end function

'' RE_SUSPICIOUS -- structural shapes this parser does not implement. Reported, never
'' silently folded into a paragraph.
private function Suspicious( byref sLine as string ) as string
    dim as long i = MdLeadWs(sLine)
    dim as string s = mid(sLine, i + 1)

    '' ^\s*\[\^[^\]]+\]:  footnote definition -- tested BEFORE the link definition, which
    '' would otherwise claim it: [^1]: matches both, and the footnote is the specific one.
    if (len(s) >= 2) andalso (s[0] = 91) andalso (s[1] = 94) then
        dim as long j = 2
        while (j < len(s)) andalso (s[j] <> 93)
            j += 1
        wend
        if (j > 2) andalso (j + 1 < len(s)) andalso (s[j] = 93) andalso (s[j+1] = 58) then
            return "footnote definition"
        end if
    end if

    '' ^\s*\[[^\]]+\]:\s  reference-style link definition
    if (len(s) >= 1) andalso (s[0] = 91) then
        dim as long j = 1
        while (j < len(s)) andalso (s[j] <> 93)
            j += 1
        wend
        dim as boolean bRefTail = (j > 1) andalso (j + 2 < len(s)) andalso _
                                  (s[j] = 93) andalso (s[j+1] = 58)
        if bRefTail andalso MdIsWs(s[j+2]) then
            return "reference-style link definition"
        end if
    end if

    '' ^\s*={3,}\s*$  setext heading underline
    scope
        dim as long j = 0
        while (j < len(s)) andalso (s[j] = 61)
            j += 1
        wend
        if j >= 3 then
            dim as long k = j
            while (k < len(s)) andalso MdIsWs(s[k])
                k += 1
            wend
            if k = len(s) then return "setext heading underline"
        end if
    end scope

    '' ^\s*\|.*\|\s*\{  table attribute block
    if (len(s) >= 1) andalso (s[0] = 124) then
        dim as long nBrace = -1
        for j as long = len(s) - 1 to 1 step -1
            if s[j] = 123 then nBrace = j : exit for
        next
        if nBrace > 0 then
            dim as long k = nBrace - 1
            while (k >= 1) andalso MdIsWs(s[k])
                k -= 1
            wend
            if (k >= 1) andalso (s[k] = 124) then return "table attribute block"
        end if
    end if

    return ""
end function


'' ---------------------------------------------------------------------------------------
'' anchors and slugs
'' ---------------------------------------------------------------------------------------

'' _ANCHOR_TAG = <a\s+name="([^"]*)"\s*>\s*</a>   (case-insensitive)
'' Removes the whole pair and hands back the captured name. mdlite uses this twice on a
'' heading: once to find the explicit id, once to get the visible text.
private function StripAnchorTags( byref sSrc as string, byref sNameOut as string ) as string
    dim as string sOut = ""
    dim as long i = 0
    dim as long n = len(sSrc)
    sNameOut = ""

    while i < n
        dim as boolean bTook = false
        if sSrc[i] = 60 then                                   '' '<'
            dim as long j = i + 1
            if (j < n) andalso ((sSrc[j] = 97) orelse (sSrc[j] = 65)) then     '' a or A
                j += 1
                dim as long nWs = j
                while (j < n) andalso MdIsWs(sSrc[j])
                    j += 1
                wend
                if j > nWs then                                '' \s+ is required
                    if MdLowerAsc(mid(sSrc, j + 1, 5)) = "name=" then
                        j += 5
                        if (j < n) andalso (sSrc[j] = 34) then  '' '"'
                            j += 1
                            dim as long nValStart = j
                            while (j < n) andalso (sSrc[j] <> 34)
                                j += 1
                            wend
                            if j < n then
                                dim as string sVal = mid(sSrc, nValStart + 1, j - nValStart)
                                j += 1
                                while (j < n) andalso MdIsWs(sSrc[j])
                                    j += 1
                                wend
                                if (j < n) andalso (sSrc[j] = 62) then       '' '>'
                                    j += 1
                                    while (j < n) andalso MdIsWs(sSrc[j])
                                        j += 1
                                    wend
                                    if MdLowerAsc(mid(sSrc, j + 1, 4)) = "</a>" then
                                        j += 4
                                        if len(sNameOut) = 0 then sNameOut = sVal
                                        i = j
                                        bTook = true
                                    end if
                                end if
                            end if
                        end if
                    end if
                end if
            end if
        end if
        if bTook = false then
            sOut += chr(sSrc[i])
            i += 1
        end if
    wend
    return MdTrimWs(sOut)
end function

'' github_slug, minus the NFKD normalisation fbc has no unicode tables for. The difference
'' is confined to accented characters: python decomposes "é" and keeps the "e", this drops
'' it, because _SLUG_STRIP removes everything outside [a-z0-9 _-] either way. No heading in
'' the AfxNova, PsControls or tiko corpora contains one -- recorded so that the day one does,
'' the anchor that does not match is explained rather than mysterious.
function MdSlug( byref sText as string ) as string
    dim as string sIgnored
    dim as string s = MdLowerAsc( MdTrimWs( StripAnchorTags(sText, sIgnored) ) )
    dim as string sOut = ""
    for i as long = 0 to len(s) - 1
        dim as long c = s[i]
        '' [^a-z0-9 _-] removed, and only THEN spaces become dashes -- so "A, B" is "a-b"
        '' and not "a,-b".
        if ((c >= 97) andalso (c <= 122)) orelse ((c >= 48) andalso (c <= 57)) orelse _
           (c = 32) orelse (c = 95) orelse (c = 45) then
            if c = 32 then sOut += "-" else sOut += chr(c)
        end if
    next
    return sOut
end function


'' ---------------------------------------------------------------------------------------
'' THE INLINE SCANNER
''
'' mdlite applies its inline rules as SEVEN SEQUENTIAL SUBSTITUTIONS over the whole string,
'' in this order: code spans (stashed so nothing later touches them), the tag allowlist,
'' image, link, bold, italic, underscore-em, bare url. A left-to-right scanner that tries
'' the same constructs in the same order at each position produces the same result and
'' nests properly, which the substitution chain only does by accident.
''
'' The lookarounds read the WHOLE string, not the slice being scanned -- MdChAt takes absolute
'' indices for exactly that reason. Python's regexes run on the whole string too, so a
'' recursion that forgot this would disagree with it at every boundary.
'' ---------------------------------------------------------------------------------------

'' Tags the corpus genuinely uses as markup. Anything else in angle brackets is prose --
'' <ENTER>, <width>, <ADD>, <example@example.com> -- and renders literally.
private function AllowedTag( byref sName as string ) as boolean
    select case MdLowerAsc(sName)
        case "a", "br", "ul", "li", "ins", "h1", "b", "i", "em", "strong", "code"
            return true
    end select
    return false
end function

private sub ScanInline( byref s as string, byval nFrom as long, byval nTo as long, _
                        byval nStyle as long, byref sHref as string, byref doc as MdDoc )

    dim as string sBuf = ""
    dim as long nTagStyle = 0        '' bits contributed by open allowlisted tags

    '' Flush the literal buffer as one run. Called before every construct, so a run boundary
    '' only ever appears where the styling actually changes.
    #macro FLUSH()
        if len(sBuf) > 0 then
            AddInl( doc, nStyle or nTagStyle, sBuf, sHref )
            sBuf = ""
        end if
    #endmacro

    dim as long i = nFrom
    while i < nTo
        dim as long ch = s[i]

        '' ---- 1. CODE SPANS FIRST. RE_CODE_SPAN = (`+)(.+?)\1 -------------------------
        '' Their contents must not be touched by any later rule, which is why mdlite stashes
        '' them before anything else runs.
        if ch = 96 then
            dim as long nTicks = 0, k = i
            while (k < nTo) andalso (s[k] = 96)
                nTicks += 1 : k += 1
            wend
            '' `\1` is that many backticks, with no assertion about what follows, so the
            '' first such run closes the span.
            dim as long nClose = -1
            dim as long j = k
            while j + nTicks <= nTo
                dim as boolean bAll = true
                for m as long = 0 to nTicks - 1
                    if s[j + m] <> 96 then bAll = false : exit for
                next
                if bAll then nClose = j : exit while
                j += 1
            wend
            '' `.+?` -- the body must be non-empty.
            if (nClose > k) then
                FLUSH()
                dim as string sCode = MdTrimWs( mid(s, k + 1, nClose - k) )
                AddInl( doc, (nStyle or nTagStyle) or MDS_CODE, sCode, sHref )
                i = nClose + nTicks
                continue while
            end if
        end if

        '' ---- 2. THE TAG ALLOWLIST ----------------------------------------------------
        if ch = 60 then                                        '' '<'
            dim as long j = i + 1
            dim as boolean bClose = false
            if (j < nTo) andalso (s[j] = 47) then bClose = true : j += 1
            dim as long nNameStart = j
            while (j < nTo) andalso MdIsWordCh(s[j])
                j += 1
            wend
            dim as string sName = mid(s, nNameStart + 1, j - nNameStart)
            if (len(sName) > 0) andalso AllowedTag(sName) then
                '' [^>]* then '>'
                dim as long nEnd = j
                while (nEnd < nTo) andalso (s[nEnd] <> 62)
                    nEnd += 1
                wend
                if nEnd < nTo then
                    dim as string sLow = MdLowerAsc(sName)
                    dim as long nBit = 0
                    select case sLow
                        case "b", "strong" : nBit = MDS_BOLD
                        case "i", "em"     : nBit = MDS_ITALIC
                        case "code"        : nBit = MDS_CODE
                    end select
                    if sLow = "br" then
                        '' A hard break BELONGS TO THE RUN BEFORE IT. An empty run carrying
                        '' the flag would be a zero-width thing the layout engine has to
                        '' measure; the only time one is unavoidable is a leading <br>.
                        FLUSH()
                        if doc.nInl > 0 then
                            doc.inl(doc.nInl - 1).bBreak = true
                        else
                            dim as long nIdx = AddInl( doc, nStyle or nTagStyle, "", sHref )
                            doc.inl(nIdx).bBreak = true
                        end if
                    elseif nBit <> 0 then
                        FLUSH()
                        if bClose then
                            nTagStyle = nTagStyle and (not nBit)
                        else
                            nTagStyle = nTagStyle or nBit
                        end if
                    end if
                    '' <a>, <ul>, <li>, <ins>, <h1> and every closing tag with no bit fall
                    '' through to here: consumed, contributing nothing, contents kept. The
                    '' corpus's <a> is <a name> -- an anchor, not a link -- and phase 1's
                    '' decision list puts #anchor navigation out of v1.
                    i = nEnd + 1
                    continue while
                end if
            end if
            '' Not an allowlisted tag: literal text. This is the <ENTER> case.
        end if

        '' ---- 3. IMAGE. RE_IMAGE = !\[([^\]]*)\]\(([^)\s]*)(?:\s+"[^"]*")?\) ----------
        if (ch = 33) andalso (MdChAt(s, i + 1) = 91) then        '' '!' '['
            dim as long nAltEnd = i + 2
            while (nAltEnd < nTo) andalso (s[nAltEnd] <> 93)
                nAltEnd += 1
            wend
            if (nAltEnd < nTo) andalso (MdChAt(s, nAltEnd + 1) = 40) then
                dim as long nSrcStart = nAltEnd + 2
                dim as long k = nSrcStart
                while k < nTo
                    if s[k] = 41 then exit while
                    if MdIsWs(s[k]) then exit while
                    k += 1
                wend
                dim as string sSrc = mid(s, nSrcStart + 1, k - nSrcStart)
                '' the optional  "title"
                while (k < nTo) andalso MdIsWs(s[k])
                    k += 1
                wend
                if (k < nTo) andalso (s[k] = 34) then
                    k += 1
                    while (k < nTo) andalso (s[k] <> 34)
                        k += 1
                    wend
                    if k < nTo then k += 1
                end if
                if (k < nTo) andalso (s[k] = 41) then
                    FLUSH()
                    dim as string sAlt = mid(s, i + 3, nAltEnd - (i + 2))
                    AddInl( doc, (nStyle or nTagStyle) or MDS_IMAGE, sAlt, sSrc )
                    i = k + 1
                    continue while
                end if
            end if
        end if

        '' ---- 4. LINK. RE_LINK = \[([^\]]*)\]\(([^)\s]*)(?:\s+"[^"]*")?\) ------------
        if ch = 91 then                                        '' '['
            dim as long nLabEnd = i + 1
            while (nLabEnd < nTo) andalso (s[nLabEnd] <> 93)
                nLabEnd += 1
            wend
            if (nLabEnd < nTo) andalso (MdChAt(s, nLabEnd + 1) = 40) then
                dim as long nHrefStart = nLabEnd + 2
                dim as long k = nHrefStart
                while k < nTo
                    if s[k] = 41 then exit while
                    if MdIsWs(s[k]) then exit while
                    k += 1
                wend
                dim as string sTarget = mid(s, nHrefStart + 1, k - nHrefStart)
                while (k < nTo) andalso MdIsWs(s[k])
                    k += 1
                wend
                if (k < nTo) andalso (s[k] = 34) then
                    k += 1
                    while (k < nTo) andalso (s[k] <> 34)
                        k += 1
                    wend
                    if k < nTo then k += 1
                end if
                if (k < nTo) andalso (s[k] = 41) then
                    FLUSH()
                    '' An empty href renders as the bare label -- mdlite's `if not href`
                    '' branch, and the corpus relies on it for a handful of dead links.
                    if len(sTarget) = 0 then
                        ScanInline( s, i + 1, nLabEnd, nStyle or nTagStyle, sHref, doc )
                    else
                        ScanInline( s, i + 1, nLabEnd, _
                                    (nStyle or nTagStyle) or MDS_LINK, sTarget, doc )
                    end if
                    i = k + 1
                    continue while
                end if
            end if
        end if

        '' ---- 5. BOLD. RE_BOLD = \*\*(?=\S)(.+?)(?<=\S)\*\* --------------------------
        if (ch = 42) andalso (MdChAt(s, i + 1) = 42) then
            dim as long nBody = i + 2
            if (nBody < nTo) andalso (MdIsWs(s[nBody]) = false) then
                dim as long nClose = -1
                dim as long j = nBody + 1
                while j + 1 < nTo
                    if cbool((s[j] = 42) andalso (s[j+1] = 42)) andalso _
                       (MdIsWs(s[j-1]) = false) then
                        nClose = j : exit while
                    end if
                    j += 1
                wend
                if nClose > nBody then
                    FLUSH()
                    ScanInline( s, nBody, nClose, (nStyle or nTagStyle) or MDS_BOLD, sHref, doc )
                    i = nClose + 2
                    continue while
                end if
            end if
        end if

        '' ---- 6. ITALIC. RE_ITALIC = (?<!\*)\*(?=[^\s*])([^*\n]+?)(?<=\S)\*(?!\*) ----
        '' The body admits no '*' at all, so the closing star is simply the next one -- and
        '' that is what keeps `**bold**` from being read as two italics.
        if (ch = 42) andalso (MdChAt(s, i - 1) <> 42) then
            dim as long nBody = i + 1
            if (nBody < nTo) andalso (MdIsWs(s[nBody]) = false) andalso (s[nBody] <> 42) then
                dim as long j = nBody
                while (j < nTo) andalso (s[j] <> 42) andalso (s[j] <> 10)
                    j += 1
                wend
                if cbool((j < nTo) andalso (s[j] = 42) andalso (j > nBody)) andalso _
                   (MdIsWs(s[j-1]) = false) andalso cbool(MdChAt(s, j + 1) <> 42) then
                    FLUSH()
                    ScanInline( s, nBody, j, (nStyle or nTagStyle) or MDS_ITALIC, sHref, doc )
                    i = j + 1
                    continue while
                end if
            end if
        end if

        '' ---- 7. UNDERSCORE EM ------------------------------------------------------
        '' RE_UNDERSCORE_EM = (?<![A-Za-z0-9_])_(?=\S)([^_\n]+?)(?<=\S)_(?![A-Za-z0-9_])
        '' The two lookarounds are the whole point: they are why snake_case_names survive.
        if (ch = 95) andalso (MdIsWordCh(MdChAt(s, i - 1)) = false) then
            dim as long nBody = i + 1
            if (nBody < nTo) andalso (MdIsWs(s[nBody]) = false) then
                dim as long j = nBody
                while (j < nTo) andalso (s[j] <> 95) andalso (s[j] <> 10)
                    j += 1
                wend
                if cbool((j < nTo) andalso (s[j] = 95) andalso (j > nBody)) andalso _
                   (MdIsWs(s[j-1]) = false) andalso _
                   (MdIsWordCh(MdChAt(s, j + 1)) = false) then
                    FLUSH()
                    ScanInline( s, nBody, j, (nStyle or nTagStyle) or MDS_ITALIC, sHref, doc )
                    i = j + 1
                    continue while
                end if
            end if
        end if

        '' ---- 8. BARE URL. RE_BARE_URL = (?<!["'(=<])\bhttps?://[^\s<>)\]"']+ --------
        if (ch = 104) orelse (ch = 72) then                     '' h or H
            dim as long chPrev = MdChAt(s, i - 1)
            dim as boolean bBlocked = (chPrev = 34) orelse (chPrev = 39) orelse _
                                      (chPrev = 40) orelse (chPrev = 61) orelse (chPrev = 60)
            '' \b before an h: the previous character must not be a word character, or the
            '' url is the tail of something else.
            if (bBlocked = false) andalso (MdIsWordCh(chPrev) = false) then
                dim as string sPre = MdLowerAsc( mid(s, i + 1, 8) )
                dim as long nSkip = 0
                if left(sPre, 7) = "http://"  then nSkip = 7
                if left(sPre, 8) = "https://" then nSkip = 8
                if nSkip > 0 then
                    dim as long j = i + nSkip
                    while j < nTo
                        dim as long c = s[j]
                        if MdIsWs(c) orelse (c = 60) orelse (c = 62) orelse (c = 41) orelse _
                           (c = 93) orelse (c = 34) orelse (c = 39) then exit while
                        j += 1
                    wend
                    if j > i + nSkip then
                        FLUSH()
                        dim as string sUrl = mid(s, i + 1, j - i)
                        AddInl( doc, (nStyle or nTagStyle) or MDS_LINK, sUrl, sUrl )
                        i = j
                        continue while
                    end if
                end if
            end if
        end if

        '' ---- nothing matched: one literal byte ---------------------------------------
        sBuf += chr(ch)
        i += 1
    wend

    FLUSH()
end sub

'' Set for the duration of a heading-only parse. A module flag rather than a parameter
'' threaded through six recursive functions, because every one of them would carry it
'' unchanged and exactly one place reads it.
dim shared as boolean g_mdHeadOnly

'' Parse `sText` into the arena and hand back the slice. bForce is what a HEADING passes: it
'' is the one block kind the corpus scan wants.
private sub ParseInlineRange( byref sText as string, byref doc as MdDoc, _
                              byref nFirst as long, byref nCount as long, _
                              byval bForce as boolean = false )
    nFirst = doc.nInl
    nCount = 0
    if g_mdHeadOnly andalso (bForce = false) then exit sub
    dim as string sNone = ""
    ScanInline( sText, 0, len(sText), 0, sNone, doc )
    nCount = doc.nInl - nFirst
end sub


'' ---------------------------------------------------------------------------------------
'' table rows
'' ---------------------------------------------------------------------------------------

'' _split_row: strip, drop one leading and one trailing bar, split on bar, strip each.
private function SplitRow( byref sLine as string, cells() as string ) as long
    dim as string s = MdTrimWs(sLine)
    if (len(s) > 0) andalso (s[0] = 124) then s = mid(s, 2)
    if (len(s) > 0) andalso (s[len(s) - 1] = 124) then s = left(s, len(s) - 1)

    dim as long n = 0
    dim as long nStart = 0
    for i as long = 0 to len(s)
        if (i = len(s)) orelse (s[i] = 124) then
            if n > ubound(cells) then redim preserve cells(0 to (n + 1) * 2)
            cells(n) = MdTrimWs( mid(s, nStart + 1, i - nStart) )
            n += 1
            nStart = i + 1
        end if
    next
    return n
end function


'' ---------------------------------------------------------------------------------------
'' THE BLOCK WALK -- mdlite's render(), in mdlite's order.
'' ---------------------------------------------------------------------------------------
private sub ParseLines( lines() as string, byval nFirst as long, byval nLast as long, _
                        byval nQuote as long, byref doc as MdDoc )
    dim as long i = nFirst
    dim as string sTmp

    while i <= nLast
        dim as string sLine = lines(i)
        if len(MdTrimWs(sLine)) = 0 then
            i += 1
            continue while
        end if

        '' ---- fenced code ----------------------------------------------------------
        dim as long chMark, nMarkLen
        dim as string sLang
        if MatchFence(sLine, chMark, nMarkLen, sLang) then
            i += 1
            dim as string sBody = ""
            dim as boolean bFirst = true
            while i <= nLast
                dim as long chC, nLenC
                dim as string sLangC
                if MatchFence(lines(i), chC, nLenC, sLangC) then
                    '' Closes only on the SAME character and at least as long. A ``` inside
                    '' a ~~~ block is content, and that is not hypothetical -- the corpus
                    '' documents markdown.
                    if (chC = chMark) andalso (nLenC >= nMarkLen) then
                        i += 1
                        exit while
                    end if
                end if
                if bFirst then bFirst = false else sBody += chr(10)
                sBody += lines(i)
                i += 1
            wend
            dim as long b = AddBlk( doc, MDBK_CODE, nQuote )
            doc.blk(b).sText = MdTrimNl(sBody)
            doc.blk(b).sLang = MdLowerAsc(sLang)
            continue while
        end if

        '' ---- heading --------------------------------------------------------------
        dim as long nLevel
        if MatchHeading(sLine, nLevel, sTmp) then
            dim as string sExplicit
            dim as string sVisible = StripAnchorTags( sTmp, sExplicit )
            dim as long b = AddBlk( doc, MDBK_HEADING, nQuote )
            doc.blk(b).nLevel = nLevel
            if len(sExplicit) > 0 then
                doc.blk(b).sAnchor = sExplicit
            else
                doc.blk(b).sAnchor = MdSlug(sVisible)
            end if
            dim as long nF, nC
            ParseInlineRange( sVisible, doc, nF, nC, true )
            doc.blk(b).nFirstInl = nF
            doc.blk(b).nInlCount = nC
            i += 1
            continue while
        end if

        '' ---- table, with a separator row ------------------------------------------
        if IsTableRow(sLine) andalso cbool(i < nLast) andalso IsTableSep(lines(i + 1)) then
            dim as long nRowFirst = doc.nBlk
            dim as long nWidest = 0
            dim as string cells(any)

            dim as long nH = SplitRow( sLine, cells() )
            if nH > nWidest then nWidest = nH
            dim as long b = AddBlk( doc, MDBK_TABLEROW, nQuote )
            doc.blk(b).bHeaderRow = true
            doc.blk(b).nFirstCell = doc.nCell
            for c as long = 0 to nH - 1
                dim as long nF, nC
                ParseInlineRange( cells(c), doc, nF, nC )
                AddCell( doc, nF, nC )
            next
            doc.blk(b).nCellCount = nH

            i += 2
            while cbool(i <= nLast) andalso IsTableRow(lines(i)) andalso _
                  (IsTableSep(lines(i)) = false)
                dim as long nR = SplitRow( lines(i), cells() )
                if nR > nWidest then nWidest = nR
                dim as long b2 = AddBlk( doc, MDBK_TABLEROW, nQuote )
                doc.blk(b2).nFirstCell = doc.nCell
                for c as long = 0 to nR - 1
                    dim as long nF, nC
                    ParseInlineRange( cells(c), doc, nF, nC )
                    AddCell( doc, nF, nC )
                next
                doc.blk(b2).nCellCount = nR
                i += 1
            wend

            '' _render_table pads every row to the widest, so the layout engine is handed a
            '' rectangle and never has to ask whether a cell exists.
            for r as long = nRowFirst to doc.nBlk - 1
                while doc.blk(r).nCellCount < nWidest
                    dim as long nF, nC
                    dim as string sEmpty = ""
                    ParseInlineRange( sEmpty, doc, nF, nC )
                    AddCell( doc, nF, nC )
                    doc.blk(r).nCellCount += 1
                wend
            next
            continue while
        end if

        '' ---- table, with NO separator row -----------------------------------------
        '' The corpus has a few and dropping them would lose real content. The first row
        '' becomes the header, exactly as mdlite does it.
        if IsTableRow(sLine) then
            dim as long nRowFirst = doc.nBlk
            dim as long nWidest = 0
            dim as string cells(any)
            dim as boolean bFirstRow = true
            while (i <= nLast) andalso IsTableRow(lines(i))
                if IsTableSep(lines(i)) = false then
                    dim as long nR = SplitRow( lines(i), cells() )
                    if nR > nWidest then nWidest = nR
                    dim as long b = AddBlk( doc, MDBK_TABLEROW, nQuote )
                    doc.blk(b).bHeaderRow = bFirstRow
                    doc.blk(b).nFirstCell = doc.nCell
                    for c as long = 0 to nR - 1
                        dim as long nF, nC
                        ParseInlineRange( cells(c), doc, nF, nC )
                        AddCell( doc, nF, nC )
                    next
                    doc.blk(b).nCellCount = nR
                    bFirstRow = false
                end if
                i += 1
            wend
            for r as long = nRowFirst to doc.nBlk - 1
                while doc.blk(r).nCellCount < nWidest
                    dim as long nF, nC
                    dim as string sEmpty = ""
                    ParseInlineRange( sEmpty, doc, nF, nC )
                    AddCell( doc, nF, nC )
                    doc.blk(r).nCellCount += 1
                wend
            next
            continue while
        end if

        '' ---- horizontal rule ------------------------------------------------------
        if IsHr(sLine) then
            AddBlk( doc, MDBK_RULE, nQuote )
            i += 1
            continue while
        end if

        '' ---- blockquote -----------------------------------------------------------
        '' mdlite recurses into a sub-document and wraps the html; a flat arena has nowhere
        '' to put one, so the inner lines are re-parsed at nQuote + 1 instead. Notes and
        '' headings surface through the same doc either way, which is the property that
        '' mattered: nothing inside a callout gets swallowed.
        if MatchBlockquote(sLine, sTmp) then
            dim as string inner(any)
            dim as long nInner = 0
            while i <= nLast
                if MatchBlockquote(lines(i), sTmp) = false then exit while
                if nInner > ubound(inner) then redim preserve inner(0 to (nInner + 1) * 2)
                inner(nInner) = sTmp
                nInner += 1
                i += 1
            wend
            if nInner > 0 then ParseLines( inner(), 0, nInner - 1, nQuote + 1, doc )
            continue while
        end if

        '' ---- lists ----------------------------------------------------------------
        dim as long nIndent
        if MatchUl(sLine, nIndent, sTmp) orelse MatchOl(sLine, nIndent, sTmp) then
            dim as boolean bOrdered = MatchOl(sLine, nIndent, sTmp)
            dim as long nNum = 0
            dim as long nLastBlk = -1
            while i <= nLast
                dim as long nInd2
                dim as string sItem
                dim as boolean bGot
                if bOrdered then
                    bGot = MatchOl(lines(i), nInd2, sItem)
                else
                    bGot = MatchUl(lines(i), nInd2, sItem)
                end if
                if bGot = false then
                    '' A plain continuation line belongs to the current item -- lazy
                    '' continuation, and the corpus leans on it for wrapped bullets.
                    if cbool((nLastBlk >= 0) andalso (len(MdTrimWs(lines(i))) > 0)) andalso _
                       (StartsBlock(lines(i)) = false) then
                        doc.blk(nLastBlk).sText += " " & MdTrimWs(lines(i))
                        i += 1
                        continue while
                    end if
                    exit while
                end if
                nNum += 1
                dim as long b = AddBlk( doc, MDBK_LISTITEM, nQuote )
                doc.blk(b).bOrdered = bOrdered
                doc.blk(b).nOrdinal = nNum
                '' The INDENT, not a nesting depth. mdlite emits one flat <ul> and ignores
                '' it entirely; it is carried here because the layout engine can indent
                '' from it, and discarding it in the parser would make that impossible.
                doc.blk(b).nLevel = nInd2
                '' Parked in sText until the item is complete, because a continuation line
                '' arrives after the block already exists and the inline arena is append-only.
                doc.blk(b).sText = MdTrimWs(sItem)
                nLastBlk = b
                i += 1
            wend
            '' Now every item's text is final, so the inline runs can be built.
            for b as long = 0 to doc.nBlk - 1
                if (doc.blk(b).kind = MDBK_LISTITEM) andalso (doc.blk(b).nInlCount = 0) andalso _
                   (len(doc.blk(b).sText) > 0) then
                    dim as long nF, nC
                    ParseInlineRange( doc.blk(b).sText, doc, nF, nC )
                    doc.blk(b).nFirstInl = nF
                    doc.blk(b).nInlCount = nC
                end if
            next
            continue while
        end if

        '' ---- indented code --------------------------------------------------------
        if IsIndentedCode(sLine) then
            dim as string sBody = ""
            dim as boolean bFirst = true
            while i <= nLast
                dim as boolean bIsCode = IsIndentedCode(lines(i))
                dim as boolean bBlank  = (len(MdTrimWs(lines(i))) = 0)
                if (bIsCode = false) andalso (bBlank = false) then exit while
                '' A blank line ends the block unless the NEXT line is still code -- which
                '' is how an indented block survives its own paragraph breaks.
                if bBlank andalso (cbool(i >= nLast) orelse _
                                   (IsIndentedCode(lines(i + 1)) = false)) then
                    exit while
                end if
                if bFirst then bFirst = false else sBody += chr(10)
                if left(lines(i), 4) = "    " then
                    sBody += mid(lines(i), 5)
                else
                    sBody += MdTrimWs(lines(i))
                end if
                i += 1
            wend
            dim as long b = AddBlk( doc, MDBK_CODE, nQuote )
            doc.blk(b).sText = MdTrimWs(sBody)
            doc.blk(b).sLang = ""
            continue while
        end if

        '' ---- structural shapes this parser does not implement -----------------------
        scope
            dim as string sWhat = Suspicious(sLine)
            if len(sWhat) > 0 then AddNote( doc, i + 1, sWhat, sLine )
        end scope

        '' ---- paragraph -------------------------------------------------------------
        scope
            dim as string sPara = ""
            dim as boolean bAny = false
            while cbool((i <= nLast) andalso (len(MdTrimWs(lines(i))) > 0)) andalso _
                  (StartsBlock(lines(i)) = false)
                if bAny then sPara += " "
                sPara += MdTrimWs(lines(i))
                bAny = true
                i += 1
            wend
            if bAny then
                dim as long b = AddBlk( doc, MDBK_PARAGRAPH, nQuote )
                dim as long nF, nC
                ParseInlineRange( sPara, doc, nF, nC )
                doc.blk(b).nFirstInl = nF
                doc.blk(b).nInlCount = nC
            else
                '' A line we flagged but cannot render. Skip it so the walk makes progress
                '' rather than spinning -- mdlite's own guard, and worth keeping: an
                '' infinite loop in a help viewer is a hang with no message.
                i += 1
            end if
        end scope
    wend
end sub


sub MdParse( byref sMarkdown as string, byref doc as MdDoc, _
             byval bHeadingsOnly as boolean )
    MdDocClear( doc )
    g_mdHeadOnly = bHeadingsOnly

    '' \r\n and bare \r normalised, then split. Written out rather than routed through
    '' PsStrSplit because that one speaks DWSTRING and this model is UTF-8 bytes throughout.
    dim as string lines(any)
    dim as long n = 0
    dim as long nStart = 0
    dim as long i = 0
    dim as long nLen = len(sMarkdown)

    while i <= nLen
        dim as boolean bEnd = (i = nLen)
        dim as long ch = 0
        if bEnd = false then ch = sMarkdown[i]
        if bEnd orelse (ch = 10) orelse (ch = 13) then
            if n > ubound(lines) then redim preserve lines(0 to (n + 1) * 2)
            lines(n) = mid(sMarkdown, nStart + 1, i - nStart)
            n += 1
            if (bEnd = false) andalso (ch = 13) andalso (i + 1 < nLen) andalso _
               (sMarkdown[i + 1] = 10) then i += 1
            nStart = i + 1
        end if
        i += 1
    wend

    if n > 0 then ParseLines( lines(), 0, n - 1, 0, doc )
    '' Cleared unconditionally. Left set, the NEXT page would render as a document with no
    '' words in it and nothing would say why.
    g_mdHeadOnly = false
end sub


'' ---------------------------------------------------------------------------------------
'' THE DUMP -- an oracle, not a report. Terse and stable on purpose: the tests compare
'' against these strings, so a cosmetic change here is a test change.
'' ---------------------------------------------------------------------------------------
private function DumpInl( byref doc as MdDoc, byval nFirst as long, byval nCount as long ) as string
    dim as string r = ""
    for k as long = nFirst to nFirst + nCount - 1
        dim as string sSty = ""
        with doc.inl(k)
            if (.nStyle and MDS_BOLD)   then sSty += "b"
            if (.nStyle and MDS_ITALIC) then sSty += "i"
            if (.nStyle and MDS_CODE)   then sSty += "c"
            if (.nStyle and MDS_LINK)   then sSty += "L"
            if (.nStyle and MDS_IMAGE)  then sSty += "M"
            if len(sSty) = 0 then sSty = "-"
            r += "[" & sSty & ":" & .sText
            if len(.sHref) > 0 then r += "->" & .sHref
            if .bBreak then r += "|br"
            r += "]"
        end with
    next
    return r
end function

function MdDumpBlock( byref doc as MdDoc, byval i as long ) as string
    if (i < 0) orelse (i >= doc.nBlk) then return ""
    dim as string q = ""
    if doc.blk(i).nQuote > 0 then q = "q" & str(doc.blk(i).nQuote) & " "

    select case doc.blk(i).kind
        case MDBK_HEADING
            return q & "h" & str(doc.blk(i).nLevel) & " #" & doc.blk(i).sAnchor & " " & _
                   DumpInl( doc, doc.blk(i).nFirstInl, doc.blk(i).nInlCount )
        case MDBK_PARAGRAPH
            return q & "p " & DumpInl( doc, doc.blk(i).nFirstInl, doc.blk(i).nInlCount )
        case MDBK_RULE
            return q & "hr"
        case MDBK_CODE
            dim as string sL = doc.blk(i).sLang
            if len(sL) = 0 then sL = "-"
            '' Newlines shown as \n so one block is always one dump line.
            dim as string sB = ""
            for k as long = 0 to len(doc.blk(i).sText) - 1
                if doc.blk(i).sText[k] = 10 then sB += "\n" else sB += chr(doc.blk(i).sText[k])
            next
            return q & "code:" & sL & " {" & sB & "}"
        case MDBK_LISTITEM
            dim as string sK = "ul"
            if doc.blk(i).bOrdered then sK = "ol" & str(doc.blk(i).nOrdinal)
            return q & sK & "@" & str(doc.blk(i).nLevel) & " " & _
                   DumpInl( doc, doc.blk(i).nFirstInl, doc.blk(i).nInlCount )
        case MDBK_TABLEROW
            dim as string r = q & iif(doc.blk(i).bHeaderRow, "th", "td")
            for c as long = doc.blk(i).nFirstCell to _
                            doc.blk(i).nFirstCell + doc.blk(i).nCellCount - 1
                r += " |" & DumpInl( doc, doc.cell(c).nFirstInl, doc.cell(c).nInlCount )
            next
            return r
    end select
    return "?"
end function
