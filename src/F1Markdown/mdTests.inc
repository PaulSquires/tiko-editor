'' ========================================================================================
'' mdTests -- the parser and highlighter suites. Run by --selftest.
''
'' ---- WHY NOT PsTest --------------------------------------------------------------------
''
'' PsTest owns its own pass/fail counters, its own report line and its own exit code, so
'' running it inside a --selftest that already has all three would print two verdicts and
'' return one of them. The phase-1 Check() is extended with CheckEq instead -- same harness,
'' one count, one exit code. Nothing else about PsTest was wanted here.
''
'' ---- WHAT THESE ASSERT AGAINST ---------------------------------------------------------
''
'' MdDumpBlock, which is an oracle rather than a report: terse, stable, one line per block,
'' and the ONLY reason it exists. The expectations below were written from mdlite.py's
'' regexes -- not pasted back from this parser's output -- because a test written from the
'' implementation asserts that the code does what it does.
''
'' The highlighter's headline property is checked separately and structurally: THE SPANS
'' TILE THE LINE. Contiguous, in order, covering every byte. That one invariant catches the
'' whole class of tokeniser bug where a branch consumes bytes without emitting them, which
'' no amount of per-token expectation reliably finds.
'' ========================================================================================

#pragma once

sub CheckEq( byref sWhat as string, byref sGot as string, byref sWant as string )
    if sGot = sWant then
        g_nPass += 1
        print "  ok      " & sWhat
    else
        g_nFail += 1
        print "  FAILED  " & sWhat
        print "            want: " & sWant
        print "            got : " & sGot
    end if
end sub

'' The whole document as one string, blocks separated by " / ". One expectation per case
'' keeps a case readable, and a block that appears or vanishes fails the case it belongs to
'' rather than shifting every later one.
private function DumpAll( byref doc as MdDoc ) as string
    dim as string r = ""
    for i as long = 0 to doc.nBlk - 1
        if i > 0 then r += " / "
        r += MdDumpBlock( doc, i )
    next
    return r
end function

private sub CaseMd( byref sWhat as string, byref sMd as string, byref sWant as string )
    dim as MdDoc doc
    MdParse( sMd, doc )
    CheckEq( sWhat, DumpAll(doc), sWant )
end sub

'' chr(10) rather than an escape: fbc processes backslash escapes in this translation unit,
'' so a literal newline written the other way arrives as something else entirely.
private function NL() as string
    return chr(10)
end function


sub MdRunParserTests()
    print "  -- markdown: headings --"
    CaseMd "heading levels", _
           "# One" & NL() & "### Three", _
           "h1 #one [-:One] / h3 #three [-:Three]"
    CaseMd "seven hashes is a paragraph", _
           "####### x", _
           "p [-:####### x]"
    CaseMd "no space after the hash is a paragraph", _
           "#nospace", _
           "p [-:#nospace]"
    CaseMd "closing hashes are stripped", _
           "## Two ##", _
           "h2 #two [-:Two]"
    CaseMd "slug drops punctuation before spacing", _
           "# A, B", _
           "h1 #a-b [-:A, B]"
    CaseMd "an explicit anchor wins over the slug", _
           "# " & Chr(60) & "a name=" & Chr(34) & "AfxStr" & Chr(34) & Chr(62) & _
           Chr(60) & "/a" & Chr(62) & " Left", _
           "h1 #AfxStr [-:Left]"

    print "  -- markdown: inline --"
    CaseMd "bold, italic and code", _
           "a **b** c *d* e `f` g", _
           "p [-:a ][b:b][-: c ][i:d][-: e ][c:f][-: g]"
    CaseMd "bold is not two italics", _
           "**x**", _
           "p [b:x]"
    CaseMd "snake_case survives", _
           "call some_long_name now", _
           "p [-:call some_long_name now]"
    CaseMd "underscore emphasis between spaces", _
           "an _emphasis_ here", _
           "p [-:an ][i:emphasis][-: here]"
    CaseMd "a code span protects its contents", _
           "`**not bold**`", _
           "p [c:**not bold**]"
    CaseMd "double backticks fence a backtick", _
           "``a ` b``", _
           "p [c:a ` b]"
    CaseMd "a link carries its href", _
           "see [CWindow](cwindow.md) now", _
           "p [-:see ][L:CWindow->cwindow.md][-: now]"
    CaseMd "an empty href renders as the label", _
           "see [dead]() now", _
           "p [-:see ][-:dead][-: now]"
    CaseMd "a link label keeps its markup", _
           "[**bold link**](x.md)", _
           "p [bL:bold link->x.md]"
    CaseMd "a bare url becomes a link", _
           "go to https://example.com/a now", _
           "p [-:go to ][L:https://example.com/a->https://example.com/a][-: now]"
    CaseMd "an image carries alt and src", _
           "![a shot](img/x.png)", _
           "p [M:a shot->img/x.png]"
    CaseMd "an image title is dropped", _
           "![a](x.png " & Chr(34) & "t" & Chr(34) & ")", _
           "p [M:a->x.png]"

    print "  -- markdown: the tag allowlist --"
    CaseMd "a break attaches to the run before it", _
           "one" & Chr(60) & "br" & Chr(62) & "two", _
           "p [-:one|br][-:two]"
    CaseMd "b and i are markup", _
           Chr(60) & "b" & Chr(62) & "x" & Chr(60) & "/b" & Chr(62) & "y", _
           "p [b:x][-:y]"
    CaseMd "angle-bracket prose is literal", _
           "press " & Chr(60) & "ENTER" & Chr(62) & " twice", _
           "p [-:press " & Chr(60) & "ENTER" & Chr(62) & " twice]"

    print "  -- markdown: blocks --"
    CaseMd "a fence keeps its language and body", _
           "```basic" & NL() & "dim x" & NL() & "```", _
           "code:basic {dim x}"
    CaseMd "a tilde fence may contain backticks", _
           "~~~" & NL() & "```" & NL() & "~~~", _
           "code:- {```}"
    CaseMd "indented code loses four spaces", _
           "    dim x" & NL() & "    dim y", _
           "code:- {dim x" & Chr(92) & "ndim y}"
    CaseMd "three dashes is a rule, not a bullet", _
           "---", _
           "hr"
    CaseMd "three stars is a rule", _
           "***", _
           "hr"
    CaseMd "a dash and a space is a bullet", _
           "- one" & NL() & "- two", _
           "ul@0 [-:one] / ul@0 [-:two]"
    CaseMd "ordered items are numbered", _
           "1. one" & NL() & "1. two", _
           "ol1@0 [-:one] / ol2@0 [-:two]"
    CaseMd "a continuation line joins the item", _
           "- one" & NL() & "  and more" & NL() & "- two", _
           "ul@0 [-:one and more] / ul@0 [-:two]"
    CaseMd "a blockquote carries its depth", _
           "> quoted", _
           "q1 p [-:quoted]"
    CaseMd "a nested blockquote counts twice", _
           "> > deep", _
           "q2 p [-:deep]"
    CaseMd "paragraph lines are joined with a space", _
           "one" & NL() & "two", _
           "p [-:one two]"

    print "  -- markdown: tables --"
    CaseMd "a table with a separator", _
           "| a | b |" & NL() & "|---|---|" & NL() & "| 1 | 2 |", _
           "th |[-:a] |[-:b] / td |[-:1] |[-:2]"
    CaseMd "short rows are padded to the widest", _
           "| a | b |" & NL() & "|---|---|" & NL() & "| 1 |", _
           "th |[-:a] |[-:b] / td |[-:1] |"
    CaseMd "a table with no separator still renders", _
           "| a | b |" & NL() & "| 1 | 2 |", _
           "th |[-:a] |[-:b] / td |[-:1] |[-:2]"
    CaseMd "cell contents are inline-parsed", _
           "| **a** |" & NL() & "|---|" & NL() & "| `b` |", _
           "th |[b:a] / td |[c:b]"

    print "  -- markdown: reported, not swallowed --"
    scope
        dim as MdDoc doc
        MdParse( "[ref]: http://x.com", doc )
        CheckEq "a reference link definition is reported", _
                iif(doc.nNote = 1, doc.note(0).sWhat, "<none:" & str(doc.nNote) & ">"), _
                "reference-style link definition"
    end scope
    scope
        dim as MdDoc doc
        MdParse( "[^1]: a footnote", doc )
        CheckEq "a footnote definition is reported", _
                iif(doc.nNote = 1, doc.note(0).sWhat, "<none:" & str(doc.nNote) & ">"), _
                "footnote definition"
    end scope
    scope
        dim as MdDoc doc
        MdParse( "Title" & NL() & NL() & "=====", doc )
        CheckEq "a setext underline is reported", _
                iif(doc.nNote = 1, doc.note(0).sWhat, "<none:" & str(doc.nNote) & ">"), _
                "setext heading underline"
    end scope
    scope
        '' AND THE CASE THAT IS NOT REPORTED, asserted so the limit is recorded rather than
        '' discovered. An underline directly under its text is swallowed by the paragraph,
        '' because mdlite's paragraph loop runs until a line that STARTS a block and "=====" 
        '' is not one. The web Help Center renders this corpus the same way; a setext heading
        '' is a paragraph in both, and that is the point of porting rather than reinventing.
        dim as MdDoc doc
        MdParse( "Title" & NL() & "=====", doc )
        CheckEq "an underline directly under text is absorbed", DumpAll(doc), _
                "p [-:Title =====]"
    end scope
    scope
        '' The one that matters most: whatever else it did, it must not have lost the line.
        dim as MdDoc doc
        MdParse( "Title" & NL() & NL() & "=====", doc )
        Check "a reported line is still rendered", (doc.nBlk >= 1), _
              str(doc.nBlk) & " blocks"
    end scope

    print "  -- markdown: the arena survives reuse --"
    scope
        '' MdDocClear keeps the capacity and resets the counts. A stale block surviving into
        '' the next page would be the obvious way for that to be wrong, and the viewer
        '' re-parses on every navigation.
        dim as MdDoc doc
        MdParse( "# a" & NL() & "# b" & NL() & "# c", doc )
        dim as long nWas = doc.nBlk
        MdParse( "# z", doc )
        Check "re-parsing resets the counts", (doc.nBlk = 1), _
              "was " & str(nWas) & ", now " & str(doc.nBlk)
        CheckEq "and yields only the new document", DumpAll(doc), "h1 #z [-:z]"
    end scope
end sub


'' ---------------------------------------------------------------------------------------
'' the highlighter
'' ---------------------------------------------------------------------------------------
private function KindLetter( byval k as long ) as string
    select case k
        case MDT_KEYWORD  : return "K"
        case MDT_COMMENT  : return "C"
        case MDT_STRING   : return "S"
        case MDT_NUMBER   : return "N"
        case MDT_OPERATOR : return "O"
        case MDT_PREPROC  : return "P"
    end select
    return "."
end function

'' One letter per BYTE of the line, so an expectation reads as a ruler under the source and
'' a one-byte misattribution is visible rather than arithmetic.
private function HiliteMap( byval nLang as long, byref sLine as string, byref nState as long ) as string
    dim as MdSpan spans(0 to 255)
    dim as long n = MdHilite( nLang, sLine, nState, spans(), 256 )
    dim as string r = ""
    for i as long = 0 to n - 1
        for k as long = 1 to spans(i).nLen
            r += KindLetter( spans(i).nKind )
        next
    next
    return r
end function

private sub CaseHi( byref sWhat as string, byval nLang as long, _
                    byref sLine as string, byref sWant as string )
    dim as long nState = MDH_STATE_NONE
    CheckEq( sWhat, HiliteMap(nLang, sLine, nState), sWant )
end sub

'' THE INVARIANT. Runs over every line any test in this file uses.
private sub CheckTiles( byref sWhat as string, byval nLang as long, byref sLine as string )
    dim as MdSpan spans(0 to 255)
    dim as long nState = MDH_STATE_NONE
    dim as long n = MdHilite( nLang, sLine, nState, spans(), 256 )
    dim as long nAt = 0
    dim as boolean bOk = true
    for i as long = 0 to n - 1
        if spans(i).nStart <> nAt then bOk = false : exit for
        if spans(i).nLen <= 0 then bOk = false : exit for
        nAt += spans(i).nLen
    next
    if nAt <> len(sLine) then bOk = false
    Check "spans tile: " & sWhat, bOk, _
          "covered " & str(nAt) & " of " & str(len(sLine)) & " in " & str(n) & " spans"
end sub

sub MdRunHiliteTests()
    print "  -- highlighter: language names --"
    Check "freebasic maps to FB", (MdLangFromName("FreeBASIC") = MDL_FB)
    Check "cpp maps to C", (MdLangFromName("cpp") = MDL_C)
    Check "an unknown language is none", (MdLangFromName("rust") = MDL_NONE)

    print "  -- highlighter: FreeBASIC --"
    ''                                    dim as long i = 5 ' n
    CaseHi "keywords, number and comment", MDL_FB, _
           "dim as long i = 5 ' n", _
           "KKK.KK.KKKK...O.N.CCC"
    CaseHi "a doubled quote stays inside the string", MDL_FB, _
           "s = " & Chr(34) & "a" & Chr(34) & Chr(34) & "b" & Chr(34) & " ' t", _
           "..O.SSSSSS.CCC"
    CaseHi "an apostrophe comment beats the block open", MDL_FB, _
           "x = 1 ' /' not a block", _
           "..O.N.CCCCCCCCCCCCCCCC"
    CaseHi "a metacommand line is preprocessor", MDL_FB, _
           "#include once " & Chr(34) & "x.bi" & Chr(34), _
           "PPPPPPPPPPPPPPPPPPPP"
    CaseHi "hex literals are numbers", MDL_FB, _
           "n = &HFF", _
           "..O.NNNN"

    print "  -- highlighter: C --"
    CaseHi "keywords, number and line comment", MDL_C, _
           "int x = 5; // n", _
           "KKK...O.NO.CCCC"
    CaseHi "a backslash escape stays inside the string", MDL_C, _
           "s = " & Chr(34) & "a" & Chr(92) & Chr(34) & "b" & Chr(34) & ";", _
           "..O.SSSSSSO"
    CaseHi "case matters for C keywords", MDL_C, _
           "Int x", _
           "....."
    CaseHi "win32 typedefs are keywords", MDL_C, _
           "HWND h", _
           "KKKK.."

    print "  -- highlighter: block comments carry across lines --"
    scope
        dim as long nState = MDH_STATE_NONE
        CheckEq "C: the opening line", HiliteMap(MDL_C, "a /* one", nState), "..CCCCCC"
        Check   "C: state is carried", (nState = MDH_STATE_COMMENT)
        CheckEq "C: the middle line", HiliteMap(MDL_C, "still in", nState), "CCCCCCCC"
        CheckEq "C: the closing line", HiliteMap(MDL_C, "out */ x", nState), "CCCCCC.."
        Check   "C: state is cleared", (nState = MDH_STATE_NONE)
    end scope
    scope
        dim as long nState = MDH_STATE_NONE
        CheckEq "FB: the opening line", HiliteMap(MDL_FB, "a /' one", nState), "..CCCCCC"
        Check   "FB: state is carried", (nState = MDH_STATE_COMMENT)
        CheckEq "FB: the closing line", HiliteMap(MDL_FB, "out '/ x", nState), "CCCCCC.."
        Check   "FB: state is cleared", (nState = MDH_STATE_NONE)
    end scope

    print "  -- highlighter: the spans tile the line --"
    CheckTiles "fb declaration", MDL_FB, "dim as long i = 5 ' n"
    CheckTiles "fb string", MDL_FB, "s = " & Chr(34) & "a" & Chr(34) & Chr(34) & "b" & Chr(34)
    CheckTiles "fb unterminated string", MDL_FB, "s = " & Chr(34) & "oops"
    CheckTiles "fb operators", MDL_FB, "a(1) = b->c + 2"
    CheckTiles "fb empty", MDL_FB, ""
    CheckTiles "fb whitespace only", MDL_FB, "    "
    CheckTiles "c declaration", MDL_C, "int x = 5; // n"
    CheckTiles "c block comment", MDL_C, "a /* b */ c"
    CheckTiles "c unterminated block", MDL_C, "a /* b"
    CheckTiles "c char literal", MDL_C, "c = 'x';"
    CheckTiles "plain text, no language", MDL_NONE, "anything at all"
    CheckTiles "utf8 bytes survive", MDL_FB, "print " & Chr(34) & Chr(226) & Chr(134) & Chr(146) & Chr(34)
end sub
