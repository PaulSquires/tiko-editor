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


'' ---------------------------------------------------------------------------------------
'' the corpus index
''
'' The RANKING is tested against a hand-built index rather than a fixture folder on disk. It
'' is the part with rules in it, and a folder would make the test depend on a scan that
'' --scan already exercises against 137 real documents.
'' ---------------------------------------------------------------------------------------
private sub IxDoc( byref ix as MdIndex, byref sTitle as string )
    if ix.nTopic > ubound(ix.topic) then redim preserve ix.topic(0 to (ix.nTopic + 1) * 2)
    with ix.topic(ix.nTopic)
        .kind = MDX_DOC : .sTitle = sTitle : .sName = sTitle
        .nParent = -1 : .nDepth = 1 : .nSort = -1 : .nRow = -1
    end with
    ix.nTopic += 1
    ix.nDocs += 1
end sub

private sub IxHead( byref ix as MdIndex, byval nTopic as long, byref sText as string )
    if ix.nHead > ubound(ix.head) then redim preserve ix.head(0 to (ix.nHead + 1) * 2)
    with ix.head(ix.nHead)
        .nTopic = nTopic : .nLevel = 2 : .sText = sText : .sAnchor = "" : .nY = -1 : .nBlock = 0
    end with
    ix.nHead += 1
end sub

sub MdRunIndexTests()
    print "  -- the index: ranked lookup --"
    dim as MdIndex ix
    dim as MdHit hits(0 to 31)

    IxDoc( ix, "CWindow" )                      '' 0 -- title is the symbol
    IxDoc( ix, "About Menus" )                  '' 1 -- only a heading matches
    IxDoc( ix, "CWindowEx" )                    '' 2 -- title prefix
    IxDoc( ix, "String Procedures" )            '' 3 -- headings only
    IxHead( ix, 1, "CWindow" )
    IxHead( ix, 2, "Notes" )
    IxHead( ix, 3, "AfxStrParseCount" )
    IxHead( ix, 3, "AfxStrParse" )

    dim as long n = MdIndexSearch( ix, "CWindow", hits(), 32 )
    Check "three topics match CWindow", (n = 3), str(n)
    Check "the page TITLED CWindow comes first", (n > 0) andalso (hits(0).nTopic = 0), _
          "first is " & str(iif(n > 0, hits(0).nTopic, -1))
    Check "an exact HEADING beats a title PREFIX", _
          (n > 2) andalso (hits(1).nTopic = 1) andalso (hits(2).nTopic = 2), _
          "order " & str(hits(0).nTopic) & "," & str(hits(1).nTopic) & "," & str(hits(2).nTopic)
    Check "a title match reports no heading", (n > 0) andalso (hits(0).nHead = -1)
    Check "a heading match reports which one", (n > 1) andalso (hits(1).nHead >= 0)

    '' The F1 case: a symbol that is a heading inside a page whose title is something else.
    n = MdIndexSearch( ix, "AfxStrParseCount", hits(), 32 )
    Check "an exact heading finds its page", _
          (n = 1) andalso (hits(0).nTopic = 3), str(n) & " hits"

    '' ONE ENTRY PER TOPIC. Two headings of one page matching must not crowd out other pages,
    '' and the entry must keep the BEST of the two rather than the first seen.
    n = MdIndexSearch( ix, "AfxStrParse", hits(), 32 )
    Check "one row per topic however many headings match", (n = 1), str(n) & " hits"
    Check "and it keeps the better score", _
          (n = 1) andalso (ix.head(hits(0).nHead).sText = "AfxStrParse"), _
          iif(n = 1, ix.head(hits(0).nHead).sText, "-")

    '' Within a band the shorter title wins, which is what makes searching "CWindow" offer
    '' CWindow before CWindowEx when neither is exact.
    n = MdIndexSearch( ix, "cwindowe", hits(), 32 )
    Check "matching is case-insensitive", (n = 1) andalso (hits(0).nTopic = 2), str(n)

    Check "an empty query matches nothing", (MdIndexSearch(ix, "", hits(), 32) = 0)
    Check "a query nothing contains matches nothing", _
          (MdIndexSearch(ix, "zzzz", hits(), 32) = 0)
end sub


'' ---------------------------------------------------------------------------------------
'' the index cache
''
'' Against a REAL fixture folder built in the temp directory, because every property worth
'' asserting here is about the filesystem: that the stamp notices an edit, that a corrupt
'' file is refused, that a partial load leaves nothing behind. A hand-built index could not
'' test any of them.
'' ---------------------------------------------------------------------------------------
sub MdRunCacheTests()
    print "  -- the index cache --"

    dim as DWSTRING sDir = PsPathJoin( PsKnownFolder(PSFOLDER_TEMP), PsText("f1md_test") )
    PsDirCreateTree( sDir )
    dim as DWSTRING sA = PsPathJoin( sDir, PsText("10-alpha.md") )
    dim as DWSTRING sB = PsPathJoin( sDir, PsText("20-beta.md") )
    PsFileWriteAll( sA, "# Alpha" & NL() & NL() & "## One" & NL() )
    PsFileWriteAll( sB, "# Beta" & NL() & NL() & "## Two" & NL() & "## Three" & NL() )
    MdCacheForget( sDir )

    '' A fresh root: no cache yet, so the load must decline rather than invent an empty one.
    scope
        dim as MdIndex ix
        Check "a missing cache does not load", (MdCacheLoad(ix, "fix", sDir) = false)
        Check "and leaves the index empty", (ix.nTopic = 0) andalso (ix.nHead = 0), _
              str(ix.nTopic) & "/" & str(ix.nHead)
    end scope

    dim as long nDocs0, nHead0, nTopic0
    scope
        dim as MdIndex ix
        MdIndexAddRoot( ix, "fix", sDir )
        nDocs0 = ix.nDocs : nHead0 = ix.nHead : nTopic0 = ix.nTopic
        Check "the fixture scanned", (nDocs0 = 2) andalso (nHead0 = 5), _
              str(nDocs0) & " docs, " & str(nHead0) & " headings"
        Check "MdCacheSave wrote it", MdCacheSave( ix, sDir, 0, 0 )
    end scope

    '' THE ROUND TRIP. Counts, titles, tree shape and heading text must all survive, because
    '' a cache that loses the parent links produces a flat TOC and nothing says why.
    scope
        dim as MdIndex ix
        Check "the cache loads", MdCacheLoad( ix, "fix", sDir )
        Check "the counts round-trip", _
              cbool(ix.nDocs = nDocs0) andalso cbool(ix.nHead = nHead0) andalso _
              cbool(ix.nTopic = nTopic0), _
              str(ix.nDocs) & "/" & str(ix.nHead) & "/" & str(ix.nTopic)
        Check "the docset label is the CALLER's", (ix.topic(0).sName = "fix"), ix.topic(0).sName
        Check "the numeric prefix still orders and hides", _
              cbool(ix.nTopic >= 3) andalso cbool(ix.topic(1).sTitle = "Alpha") andalso _
              cbool(ix.topic(2).sTitle = "Beta"), _
              ix.topic(1).sTitle & ", " & ix.topic(2).sTitle
        Check "parents survived", (ix.topic(1).nParent = 0) andalso (ix.topic(2).nParent = 0), _
              str(ix.topic(1).nParent) & "," & str(ix.topic(2).nParent)
        '' And the index it produces must answer the same questions the scan's would.
        dim as MdHit hits(0 to 15)
        dim as long n = MdIndexSearch( ix, "Three", hits(), 16 )
        Check "a cached heading is searchable", (n = 1) andalso (hits(0).nTopic = 2), str(n)
    end scope

    '' A SECOND ROOT BEHIND THE FIRST. Parent indices are stored relative to the root's own
    '' first topic; get that wrong and the second docset's tree hangs off the first one.
    scope
        dim as MdIndex ix
        MdIndexAddRoot( ix, "other", sDir )
        dim as long nBase = ix.nTopic
        Check "a cache loads behind an existing root", MdCacheLoad( ix, "fix", sDir )
        Check "and its parents are rebased", _
              (ix.nTopic > nBase) andalso (ix.topic(nBase + 1).nParent = nBase), _
              str(ix.topic(nBase + 1).nParent) & " want " & str(nBase)
    end scope

    '' STALENESS. The stamp is count + bytes + newest write time, so a file that gets LONGER
    '' must miss even if the clock has not moved.
    scope
        PsFileWriteAll( sB, "# Beta" & NL() & NL() & "## Two" & NL() & "## Three" & NL() & _
                            "## Four" & NL() )
        dim as MdIndex ix
        Check "an edited corpus misses", (MdCacheLoad(ix, "fix", sDir) = false)
        Check "and the miss leaves nothing behind", (ix.nTopic = 0), str(ix.nTopic)
    end scope

    '' A DELETED FILE must miss too -- the count and the byte total both move.
    scope
        dim as MdIndex ix
        MdIndexAddRoot( ix, "fix", sDir )
        MdCacheSave( ix, sDir, 0, 0 )
    end scope
    scope
        PsFileDelete( sA )
        dim as MdIndex ix
        Check "a deleted document misses", (MdCacheLoad(ix, "fix", sDir) = false)
    end scope

    '' CORRUPTION. Anything that is not this format at this version is refused, rather than
    '' being half-read into an index that then renders as missing documentation.
    scope
        PsFileWriteAll( MdCachePath(sDir), "garbage, and not even the right length" )
        dim as MdIndex ix
        Check "a corrupt cache is refused", (MdCacheLoad(ix, "fix", sDir) = false)
        Check "and leaves nothing behind", (ix.nTopic = 0), str(ix.nTopic)
    end scope

    '' Tidy up: the temp fixture is ours and leaving it would make the next run's stamp lie.
    MdCacheForget( sDir )
    PsFileDelete( sB )
    PsDirDelete( sDir )
end sub


'' ---------------------------------------------------------------------------------------
'' fonts
'' ---------------------------------------------------------------------------------------
sub MdRunFontTests()
    print "  -- fonts: ten engines, and they really are six faces --"
    dim as boolean bAll = true
    for i as long = 0 to MDF_COUNT - 1
        if MdFontPtr(i)->lineHeight <= 0 then bAll = false
    next
    Check "all ten engines opened", bAll, MdFontsMissing()

    '' THE VARIABLE-FONT TRAP, ASSERTED. A variable file loads and renders its default
    '' instance, so a "bold" opened from one measures IDENTICALLY to the regular and nothing
    '' errors anywhere. Comparing the two widths is the cheapest thing that can tell them
    '' apart, and it is the entire reason six static faces were shipped.
    dim as string sSpec = "Handgloves"
    dim as long nReg  = TE_MeasureText64( *MdFontPtr(MDF_BODY),   sSpec )
    dim as long nBold = TE_MeasureText64( *MdFontPtr(MDF_BODY_B), sSpec )
    dim as long nItal = TE_MeasureText64( *MdFontPtr(MDF_BODY_I), sSpec )
    Check "bold is a different face from regular", (nBold <> nReg), _
          str(nBold) & " vs " & str(nReg)
    Check "italic is a different face from regular", (nItal <> nReg), _
          str(nItal) & " vs " & str(nReg)

    '' And that the mono face really is monospaced. With the wrong file, code still renders
    '' -- just misaligned, which is the kind of wrong that survives a glance at a screenshot.
    dim as long nNarrow = TE_MeasureText64( *MdFontPtr(MDF_MONO), "iiii" )
    dim as long nWide   = TE_MeasureText64( *MdFontPtr(MDF_MONO), "MMMM" )
    Check "the mono face is monospaced", (nNarrow = nWide), _
          str(nNarrow) & " vs " & str(nWide)
    dim as long nPropN = TE_MeasureText64( *MdFontPtr(MDF_BODY), "iiii" )
    dim as long nPropW = TE_MeasureText64( *MdFontPtr(MDF_BODY), "MMMM" )
    Check "the body face is proportional", (nPropN <> nPropW), _
          str(nPropN) & " vs " & str(nPropW)

    Check "h1 is larger than the body", _
          (MdFontPtr(MDF_H1)->lineHeight > MdFontPtr(MDF_BODY)->lineHeight)
    Check "style bits pick the right engine", _
          (MdFontForInline(MDS_BOLD or MDS_ITALIC) = MDF_BODY_BI) andalso _
          (MdFontForInline(MDS_CODE or MDS_ITALIC) = MDF_MONO) andalso _
          (MdFontForHeading(6) = MDF_BODY_B)
end sub


'' One of every block kind, because a layout assertion about a paragraph proves nothing
'' about a table. Also what --demo renders, so the interactive pass and the tests look at
'' exactly the same document.
function MdDemoMarkdown() as string
    dim as string s
    s  = "# F1Markdown" & NL() & NL()
    s &= "A **native** markdown viewer with *emphasis*, `inline code` and a " & _
         "[link](CWindow.md) in one paragraph, long enough that it has to wrap at any " & _
         "sensible pane width and therefore exercises the run-aware wrapper across a " & _
         "style change." & NL() & NL()
    s &= "## Blocks" & NL() & NL()
    s &= "- a bullet" & NL() & "- another, with a continuation line" & NL()
    s &= "  that belongs to the same item" & NL() & NL()
    s &= "1. first" & NL() & "2. second" & NL() & NL()
    s &= "> a callout, quoted" & NL() & NL()
    s &= "```freebasic" & NL()
    s &= "dim as hwnd h = CreateWindowEx( 0, " & Chr(34) & "MyClass" & Chr(34) & ", _" & NL()
    s &= "    " & Chr(34) & "Title" & Chr(34) & ", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480 )  ' note" & NL()
    s &= "```" & NL() & NL()
    s &= "| Return | Meaning |" & NL()
    s &= "|--------|---------|" & NL()
    s &= "| HWND   | Handle to the new window |" & NL()
    s &= "| NULL   | Failed -- call **GetLastError** |" & NL() & NL()
    s &= "---" & NL() & NL()
    s &= "### Done" & NL() & NL()
    s &= "That is every block kind the parser emits." & NL()
    return s
end function


'' ---------------------------------------------------------------------------------------
'' layout
'' ---------------------------------------------------------------------------------------
sub MdRunLayoutTests()
    print "  -- layout --"
    dim as MdDoc doc
    dim as MdPage page
    dim as string sMd = MdDemoMarkdown()
    dim as string sBase = ""
    MdParse( sMd, doc )

    const LW = 700
    MdLayoutPage( doc, page, LW, 1.0, sBase )

    Check "the demo parsed", (doc.nBlk > 12), str(doc.nBlk) & " blocks"
    Check "lines were produced", (page.nLine > 12), str(page.nLine) & " lines"
    Check "the page has a height", (page.nHeight > 200), str(page.nHeight)
    Check "the width is recorded", (page.nWidth = LW)

    '' THE WRAP INVARIANT, and the one assertion that catches a wrapper measuring with the
    '' wrong engine: nothing except a code fence may extend past the width it was given.
    '' Fences are excluded BY DESIGN -- they do not wrap and the view clips them -- so they
    '' are counted separately rather than quietly skipped.
    dim as long nOver = 0, nFenceOver = 0, nWidest = 0
    for r as long = 0 to page.nRun - 1
        dim as long nRight = page.run(r).x + page.run(r).w
        if page.run(r).nKind = MDRK_FENCE then
            if nRight > LW then nFenceOver += 1
        else
            if nRight > nWidest then nWidest = nRight
            if nRight > LW then nOver += 1
        end if
    next
    Check "no wrapped run passes the layout width", (nOver = 0), _
          str(nOver) & " over, widest " & str(nWidest) & " of " & str(LW)
    Check "the demo fence fits too", (nFenceOver = 0), str(nFenceOver) & " over"

    '' A baseline outside its own line box draws on top of the line above.
    dim as long nBadLine = 0
    for l as long = 0 to page.nLine - 1
        if page.line(l).h <= 0 then nBadLine += 1
        if page.line(l).nBaseline <= 0 then nBadLine += 1
        if page.line(l).nBaseline > page.line(l).h then nBadLine += 1
    next
    Check "every line has a sane box", (nBadLine = 0), str(nBadLine) & " bad"

    '' Narrower means more lines. A wrapper that ignored its width passes everything above
    '' and fails this.
    dim as MdPage narrow
    MdLayoutPage( doc, narrow, 340, 1.0, sBase )
    Check "a narrower width wraps more", (narrow.nLine > page.nLine), _
          str(narrow.nLine) & " vs " & str(page.nLine)
    Check "and is therefore taller", (narrow.nHeight > page.nHeight), _
          str(narrow.nHeight) & " vs " & str(page.nHeight)

    dim as long nCodeBg = 0, nRule = 0, nQuote = 0, nTable = 0, nHead = 0
    for d as long = 0 to page.nDeco - 1
        select case page.deco(d).nKind
            case MDD_CODEBG      : nCodeBg += 1
            case MDD_RULE        : nRule += 1
            case MDD_QUOTEBAR    : nQuote += 1
            case MDD_TABLELINE   : nTable += 1
            case MDD_TABLEHEADBG : nHead += 1
        end select
    next
    Check "the fence got a slab", (nCodeBg = 1), str(nCodeBg)
    Check "h1 and h2 underlines plus the explicit rule", (nRule = 3), str(nRule) & " rules"
    Check "the blockquote got a bar", (nQuote = 1), str(nQuote)
    Check "the table got a header band and grid lines", _
          cbool(nHead = 1) andalso cbool(nTable = 4), _
          str(nHead) & " head, " & str(nTable) & " lines"

    '' The demo has bold, italic, code and a link in ONE paragraph, so all four must have
    '' survived parse, segmentation and wrapping to reach a run.
    dim as boolean bB = false, bI = false, bC = false, bL = false
    for r as long = 0 to page.nRun - 1
        if page.run(r).nFont = MDF_BODY_B then bB = true
        if page.run(r).nFont = MDF_BODY_I then bI = true
        if page.run(r).nFont = MDF_MONO   then bC = true
        if page.run(r).nKind = MDRK_LINK  then bL = true
    next
    Check "bold reached a run", bB
    Check "italic reached a run", bI
    Check "monospace reached a run", bC
    Check "a link run was produced", bL

    '' Hit-testing, which is the path a click on a link takes.
    dim as long nLinkRun = -1
    for r as long = 0 to page.nRun - 1
        if page.run(r).nKind = MDRK_LINK then nLinkRun = r : exit for
    next
    if nLinkRun >= 0 then
        dim as long nLineOf = -1
        for l as long = 0 to page.nLine - 1
            if cbool(nLinkRun >= page.line(l).nFirstRun) andalso _
               cbool(nLinkRun < page.line(l).nFirstRun + page.line(l).nRunCount) then
                nLineOf = l : exit for
            end if
        next
        dim as long nHit = -1
        if nLineOf >= 0 then
            nHit = MdPageHitRun( page, page.run(nLinkRun).x + 1, page.line(nLineOf).y + 1 )
            Check "the link hit-tests back to itself", (nHit = nLinkRun), _
                  "hit " & str(nHit) & ", want " & str(nLinkRun)
            Check "a point left of the content hits nothing", _
                  (MdPageHitRun( page, -5, page.line(nLineOf).y + 1 ) = -1)
        else
            Check "the link hit-tests back to itself", false, "run is on no line"
        end if
    else
        Check "the link hit-tests back to itself", false, "no link run"
    end if

    '' Every metric is a design unit, so the same document at 1.75 must be taller -- and the
    '' FONTS have to be reopened first or the text would not be, which is the four-part DPI
    '' change asserted from the page's side.
    MdFontsApplyScale( 1.75 )
    dim as MdPage big
    MdLayoutPage( doc, big, PsScaleBy(LW, 1.75), 1.75, sBase )
    Check "the page scales with the display", (big.nHeight > page.nHeight), _
          str(big.nHeight) & " vs " & str(page.nHeight)
    MdFontsApplyScale( 1.0 )
end sub
