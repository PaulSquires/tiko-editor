'' ========================================================================================
'' mdParse -- markdown to a flat block model. NO UI, NO PAINTING, NO PLATFORM CALLS.
''
'' ---- THIS IS A PORT, NOT A DESIGN ------------------------------------------------------
''
'' C:\dev\HelpCenter\tools\helpgen\mdlite.py is the spec, and it is a MEASURED one: its
'' header records the subset counted across all 137 AfxNova topics rather than guessed --
'' 29.4k table rows, 21.4k **bold**, 8.9k code fences, 13 indented code blocks, 5 _emphasis_.
'' Both this and the web Help Center therefore render the same corpus by the same rules, and
'' a divergence between them is a bug in one of them rather than a matter of taste.
''
'' THE BLOCK ORDER IS LOAD-BEARING and copied exactly: fence, heading, table-with-separator,
'' table-without, rule, blockquote, list, indented code, suspicious, paragraph. Moving `rule`
'' above `heading` costs nothing; moving it below `list` turns every `---` into a bullet.
''
'' ---- WHAT IS DELIBERATELY DIFFERENT ----------------------------------------------------
''
'' 1. NO HTML, and no escaping. mdlite escapes everything and then puts allowlisted tags
''    back, because it serves a public page and document content must not inject markup.
''    Nothing here is served, so the allowlist survives for the OTHER reason it exists: the
''    corpus is full of angle-bracket prose -- <ENTER>, <width>, <ADD>, <example@example.com>
''    -- which is not markup and must render literally. Tag in the allowlist means styling;
''    anything else in angle brackets is text. Same rule, different action.
''
'' 2. fbc `string` HOLDING UTF-8, not DWSTRING, everywhere in the model. Two reasons, and
''    the second is the one that decided it: TE_DrawText and TE_MeasureText take a UTF-8
''    `string` and all their offsets are UTF-8 BYTE offsets, so a DWSTRING model would be
''    converted back on every measurement; and DWSTRING has a constructor, which makes
''    `redim preserve` over an arena of them a risk this does not need to take.
''    All delimiters below are ASCII, and every UTF-8 continuation byte is >= 0x80, so
''    byte-wise scanning cannot land inside a multi-byte sequence.
''
'' 3. BLOCKQUOTES ARE A DEPTH FIELD, not a nested document. mdlite recurses and wraps the
''    result in <blockquote>; a flat arena has nowhere to put a sub-document, so the inner
''    lines are re-parsed with nQuote + 1 and every block carries how deep it sits.
''
'' 4. NO ANCHOR RESOLUTION. mdlite exists at all because it needs a resolve_link hook to
''    rewrite #anchor into a site URL. Phase 4 navigates .md paths directly and phase 1's
''    decision list puts #anchor links out of v1, so hrefs are carried verbatim.
'' ========================================================================================

#pragma once

'' ---- inline style bits ----------------------------------------------------------------
'' Bits, not an enum: a run can be bold AND code AND inside a link at once, and the corpus
'' does all three.
const MDS_BOLD   = 1
const MDS_ITALIC = 2
const MDS_CODE   = 4
const MDS_LINK   = 8
const MDS_IMAGE  = 16

'' ---- block kinds ----------------------------------------------------------------------
enum MdBlockKind
    MDBK_PARAGRAPH = 0
    MDBK_HEADING
    MDBK_CODE          '' fenced or indented; sLang empty for indented
    MDBK_RULE
    MDBK_LISTITEM      '' one per <li>; bOrdered and nOrdinal say which list it is in
    MDBK_TABLEROW      '' bHeaderRow marks the <th> row
end enum

'' ---- one styled run of text -----------------------------------------------------------
type MdInline
    nStyle as long           '' MDS_* bits
    sText  as string         '' UTF-8. For MDS_IMAGE this is the alt text.
    sHref  as string         '' link target or image source; empty otherwise
    '' A <br> that FOLLOWS this run. Carried on the run rather than emitted as an empty run
    '' so that the layout engine never has to measure a zero-width nothing.
    bBreak as boolean
end type

'' ---- one table cell: a slice of the inline arena ---------------------------------------
type MdCell
    nFirstInl as long
    nInlCount as long
end type

'' ---- one block ------------------------------------------------------------------------
type MdBlock
    kind       as long
    nLevel     as long       '' heading 1-6; for a list item, its indent level
    nQuote     as long       '' blockquote nesting depth; 0 = not quoted
    bOrdered   as boolean
    nOrdinal   as long       '' the number an ordered item renders with
    bHeaderRow as boolean

    nFirstInl  as long       '' slice of doc.inl -- every kind except CODE and TABLEROW
    nInlCount  as long
    nFirstCell as long       '' slice of doc.cell -- TABLEROW only
    nCellCount as long

    sText      as string     '' CODE only: the body, verbatim, newline separated
    sLang      as string     '' CODE only: the fence language, lowercased; may be empty
    sAnchor    as string     '' HEADING only: the explicit <a name> or the github slug
end type

'' ---- a line that looks structural but that this parser does not implement --------------
'' Reported rather than quietly folded into a paragraph, which is the single most valuable
'' property mdlite has: a mis-render fails loudly instead of losing content.
type MdNote
    nLine as long            '' 1-based
    sWhat as string
    sLine as string
end type

type MdDoc
    blk(any)  as MdBlock
    inl(any)  as MdInline
    cell(any) as MdCell
    note(any) as MdNote
    nBlk  as long
    nInl  as long
    nCell as long
    nNote as long
end type

declare sub MdDocClear( byref doc as MdDoc )

'' THE ENTRY POINT. sMarkdown is UTF-8; \r\n and bare \r are normalised. doc is cleared
'' first, so the same MdDoc can be reused across pages without leaking the previous one.
declare sub MdParse( byref sMarkdown as string, byref doc as MdDoc )

'' Exposed for the tests and for phase 4's heading index, which wants the slug rule without
'' re-deriving it.
declare function MdSlug( byref sText as string ) as string

'' A one-line-per-block textual dump. The --dump-md mode prints this; the tests compare
'' against it. Deliberately terse and stable -- it is an oracle, not a report.
declare function MdDumpBlock( byref doc as MdDoc, byval i as long ) as string
