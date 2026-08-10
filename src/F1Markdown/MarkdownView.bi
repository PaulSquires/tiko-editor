'' ========================================================================================
'' MarkdownView -- the widget that draws a laid-out page. One PsWidget, one PsVScrollBar.
''
'' ---- WHY NOT PsScrollPanel -------------------------------------------------------------
''
'' PsScrollPanel scrolls by moving a taller PAGE WIDGET: page->bounds.y = -nPos. That means a
'' widget as tall as the document, and a reference page in this corpus lays out to 40,000
'' pixels. Nothing needs to exist for the 39,000 of them that are off screen. PsListTree owns
'' its PsVScrollBar directly for the same reason and this mirrors it: the scroll position is
'' an integer, OnPaint draws the lines that intersect the damage rect, and there is no second
'' widget at all.
''
'' ---- THE THEME DOES NOT REACH THIS ----------------------------------------------------
''
'' PsThemeApply walks the WIDGET tree, so it calls OnThemeChanged here -- but everything this
'' widget draws is resolved from a colour table it fills ITSELF, in that override. The page
'' model carries kinds, never colours. Miss the override and you get a perfectly themed shell
'' with an unthemed slab in the middle of it, which is the exact failure ideshell's header
'' warns about for Scintilla.
''
'' ---- TEXT GOES ROUND PsBufferPaint ----------------------------------------------------
''
'' PaintText can only draw a vertically centred single line in ONE engine, and a markdown line
'' mixes engines and needs an exact shared baseline. So decorations go through PsBufferPaint
'' -- which applies the widget origin -- and text goes straight to g_rb.DrawText_ at an
'' absolute baseline computed from Abs_. That is what PaintText itself does internally.
'' ========================================================================================

#pragma once

'' Fired when a link is activated. The href is raw, exactly as the document wrote it: the
'' view has no opinion about what an .md path or an http url means.
type MdLinkProc as sub( byval pView as any ptr, byref sHref as string, byval userdata as any ptr )

type MarkdownView extends PsWidget
    '' HEAP, not members. MdDoc and MdPage each carry four variable-length arrays, and a
    '' widget with virtuals is not the place to find out how fbc feels about that.
    pDoc  as MdDoc ptr
    pPage as MdPage ptr

    sSource  as string        '' UTF-8 markdown, kept so a width change can re-lay out
    sBaseDir as string        '' resolves relative image paths; ends with a separator

    pScroll as PsVScrollBar ptr
    nScroll as long           '' content y at the top of the viewport
    nLaidW  as long           '' the width pPage was laid out for; -1 forces a relayout
    nHotRun as long           '' the link run under the pointer, or -1
    '' The code slab under the pointer, and whether the pointer is on its Copy button. Both
    '' are decoration indices rather than block indices, because that is what OnPaint walks.
    nHotCode  as long
    bOnCopy   as boolean
    nCopiedAt as long         '' the slab that most recently said "Copied"; -1 for none

    clrBack      as PsColor
    clrFore      as PsColor
    clrDim       as PsColor
    clrLink      as PsColor
    clrRule      as PsColor
    clrCodeBg    as PsColor
    clrCodeFore  as PsColor
    clrInlCodeBg as PsColor
    clrQuoteBar  as PsColor
    clrTableHead as PsColor
    clrTableLine as PsColor
    clrTok(0 to 6) as PsColor      '' indexed by MdTokKind

    pfnLink   as MdLinkProc
    pLinkData as any ptr

    declare constructor()
    declare destructor()

    '' Parses, lays out and scrolls to the top. sBase must end with a separator.
    declare sub SetMarkdown( byref sUtf8 as string, byref sBase as string )
    declare sub OnLink( byval pfn as MdLinkProc, byval userdata as any ptr = 0 )

    declare function CopyRect( byval nDeco as long ) as PsRect
    declare function CodeAt( byval px as long, byval py as long ) as long
    declare sub ScrollTo( byval nY as long )
    declare sub ScrollBy( byval nDelta as long )
    declare function GetScroll() as long
    declare function ContentHeight() as long
    declare function ContentWidth() as long

    declare sub OnLayout()
    declare sub OnPaint( byval p as PsBufferPaint_ ptr )
    declare function OnEvent( byval ev as PsEvent ptr ) as boolean
    declare sub OnThemeChanged()
    declare sub OnScaleChanged( byval fScale as single )
end type
