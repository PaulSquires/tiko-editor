'' ========================================================================================
'' mdIndex -- the corpus: a folder tree walked into topics, plus a heading index to search.
'' NO UI. NO WIDGET. Filesystem and strings only.
''
'' ---- TITLES AND HEADINGS COME FROM A REAL PARSE ---------------------------------------
''
'' Every file is read and run through MdParse at scan time. That is more work than grepping
'' for a leading "#", and it is the right work: a "# heading" inside a fenced code block is
'' not a heading, and a grep cannot tell. The corpus documents markdown, so those exist.
''
'' ---- SEARCH IS TITLES AND HEADINGS, NOT BODIES ----------------------------------------
''
'' A decision, not a limitation to be fixed later. F1 is pressed on a SYMBOL, and in a
'' reference corpus every symbol IS a heading -- so the heading index answers the question
'' F1 actually asks, in a fraction of the memory a body index would take. Prose that appears
'' in no heading is unfindable, and that is the trade.
''
'' ---- ORDER --------------------------------------------------------------------------
''
'' Numeric filename prefixes sort first and are HIDDEN in the display, so "10-intro.md"
'' renders as "Intro" and sits before "20-usage.md". Without a prefix, alphabetical by title.
'' A toc.txt in a folder overrides both: one name per line, in the order they should appear,
'' and anything not listed follows in the default order rather than disappearing.
'' ========================================================================================

#pragma once

const MDX_MAX_DEPTH = 12       '' a corpus nested deeper than this is a symlink loop

'' A node in the tree: a docset root, a folder, or a document.
enum MdNodeKind
    MDX_DOCSET = 0
    MDX_FOLDER
    MDX_DOC
end enum

type MdTopic
    kind    as long
    sPath   as DWSTRING     '' absolute; empty for a docset root that is only a label
    sName   as string       '' what the tree shows
    sTitle  as string       '' first H1 for a doc; = sName otherwise
    nParent as long         '' index into topic(), or -1
    nDepth  as long
    nSort   as long         '' the numeric filename prefix, or -1
    nRow    as long         '' the PsListTree row this became, or -1
end type

type MdHeading
    nTopic  as long
    nLevel  as long
    sText   as string
    sAnchor as string
    nY      as long         '' content y once the topic is laid out; -1 until then
    nBlock  as long         '' block ordinal, which is what survives a relayout
end type

type MdIndex
    topic(any) as MdTopic
    head(any)  as MdHeading
    nTopic as long
    nHead  as long
    nDocs  as long          '' documents only, for the status bar
    nSkipped as long        '' files that could not be read
end type

declare sub MdIndexClear( byref ix as MdIndex )

'' Adds one docset: sLabel is the tree's top-level caption, sRoot the folder to walk.
'' Returns the number of documents found. Safe to call repeatedly for several roots.
declare function MdIndexAddRoot( byref ix as MdIndex, byref sLabel as string, _
                                 byref sRoot as DWSTRING ) as long

'' ---- ranked lookup ---------------------------------------------------------------------
'' One result: a topic, and optionally the heading within it that matched.
type MdHit
    nTopic as long
    nHead  as long          '' -1 when the TITLE matched rather than a heading
    nScore as long          '' higher is better
end type

'' Fills hits() with up to nMax results, best first. Returns how many. Case-insensitive.
''
'' THE RANK ORDER IS THE WHOLE POINT, because F1 passes a symbol and the right page has to be
'' first: exact title, exact heading, title prefix, heading prefix, title substring, heading
'' substring. A page whose TITLE is the symbol always beats a page that merely mentions it.
declare function MdIndexSearch( byref ix as MdIndex, byref sQuery as string, _
                                hits() as MdHit, byval nMax as long ) as long

'' The topic whose file is sPath, or -1. Used to turn a followed .md link into a topic.
declare function MdIndexFindPath( byref ix as MdIndex, byref sPath as DWSTRING ) as long
