'' ========================================================================================
'' mdLayout -- blocks and a width in, positioned lines out. NO WIDGET, NO EVENTS, NO WINDOW.
''
'' ---- WHAT IT DEPENDS ON, HONESTLY ------------------------------------------------------
''
'' mdParse and mdHilite are pure data. This is not: it needs FONT METRICS to place anything,
'' so it calls PsTextEngine, and it needs IMAGE DIMENSIONS, so it calls PsImage. Both are
'' PsPlatform's PAINT layer, which exists on Linux -- so this still ports by recompiling.
'' What it must never reach is the UI layer: no PsWidget, no PsSurface, no event, no colour.
'' Colours are the renderer's business and appear nowhere below; a run carries a KIND and
'' the view resolves it against the theme.
''
'' ---- THE WRAPPER IS THE POINT ---------------------------------------------------------
''
'' PsWrapText takes ONE PsTextEngine and therefore cannot wrap `a **bold** word` -- which is
'' most of the corpus. So the wrapper here is run-aware: it walks segments, each with its own
'' engine, and breaks between words across them.
''
'' IT MEASURES WORD BY WORD AND SUMS. Shaping the whole line at once would be marginally more
'' accurate across a space, and is not possible: the line is not one engine. The drift is
'' sub-pixel kerning at a word boundary. What is NOT done by summing is a caret position --
'' nothing here produces one, and phase 5's selection, if it comes, must use TE_XToIndex.
''
'' ---- CODE FENCES DO NOT WRAP ----------------------------------------------------------
''
'' A wrapped code sample is a wrong code sample: the line breaks stop meaning what the author
'' wrote. Fences are laid out at their natural width and CLIPPED by the view. page.nCodeMaxW
'' carries the widest so a horizontal scrollbar can be added without re-laying anything out.
'' ========================================================================================

#pragma once

'' What a run is FOR, which is how the view picks its colour. Never a colour here.
enum MdRunKind
    MDRK_TEXT = 0
    MDRK_LINK
    MDRK_CODE          '' `inline code` -- gets a subtle background too
    MDRK_FENCE         '' a line inside a fenced/indented block; nTok says which token
    MDRK_MARKER        '' a synthesized bullet or ordinal; never part of the document text
    MDRK_TABLEHEAD
end enum

enum MdDecoKind
    MDD_CODEBG = 0     '' the slab behind a fence
    MDD_INLCODEBG      '' the pill behind an inline code run
    MDD_RULE           '' a horizontal rule, or the underline beneath h1/h2
    MDD_QUOTEBAR       '' the vertical bar down the left of a blockquote
    MDD_TABLELINE      '' one horizontal grid line
    MDD_TABLEHEADBG
    MDD_IMAGE
end enum

type MdRun
    nFont as long      '' MdFontId
    nKind as long      '' MdRunKind
    nTok  as long      '' MdTokKind, MDRK_FENCE only
    nInl  as long      '' index into MdDoc.inl, or -1 when synthesized
    x     as long      '' left edge, content coordinates
    w     as long
    sText as string    '' UTF-8, exactly what gets drawn
end type

type MdLine
    y         as long  '' top, content coordinates -- 0 is the top of the document
    h         as long
    nBaseline as long  '' offset from y; the max ascent of the fonts on this line
    nFirstRun as long
    nRunCount as long
end type

type MdDeco
    nKind  as long
    x as long : y as long : w as long : h as long
    nImage as long     '' MDD_IMAGE: index into the layout's image cache, or -1
end type

type MdPage
    line(any) as MdLine
    run(any)  as MdRun
    deco(any) as MdDeco
    nLine as long
    nRun  as long
    nDeco as long

    nHeight   as long  '' total content height, including the bottom pad
    '' The right edge of the laid-out content. The view clips TEXT to this rather than to
    '' its own client width, which is what keeps a non-wrapping fence inside its slab
    '' instead of running out to the window edge.
    nContentRight as long
    nWidth    as long  '' the width this was laid out FOR; the view re-lays out when it moves
    nCodeMaxW as long  '' widest fence line, for a future horizontal scrollbar
end type

declare sub MdPageClear( byref page as MdPage )

'' THE ENTRY POINT. sBaseDir resolves relative image paths and must end with a separator.
'' fScale scales the metric constants; the FONTS are already open at the right size and are
'' not re-opened here -- that is MdFontsApplyScale's job and doing it twice would rebuild ten
'' atlases on every resize.
declare sub MdLayoutPage( byref doc as MdDoc, byref page as MdPage, _
                          byval nWidth as long, byval fScale as single, _
                          byref sBaseDir as string )

'' The run under a point in content coordinates, or -1. Used for link hit-testing and for
'' the hover cursor.
declare function MdPageHitRun( byref page as MdPage, byval px as long, byval py as long ) as long

'' Content y of the first line belonging to block nBlk, or -1. Phase 4's search jumps with it.
declare function MdPageBlockY( byref page as MdPage, byval nY as long ) as long

'' The image cache. Owned here because layout is what needs the natural size, and loading the
'' same file twice per page would be the obvious way to make scrolling slow.
'' The BLImageCore PsBufferPaint.PaintImage wants, plus its natural size. Two calls rather
'' than one returning a UDT, because a UDT returned by value through anything fbc might turn
'' into an indirect call is the landmine Platform.bi documents.
declare function MdLayoutImagePtr( byval nIndex as long ) as any ptr
declare sub MdLayoutImageSize( byval nIndex as long, byref nW as long, byref nH as long )
declare sub MdLayoutFreeImages()
declare function MdLayoutImageCount() as long
