'' ========================================================================================
'' mdLayout.inc -- the implementation. See mdLayout.bi for the layering and the wrap rule.
''
'' EVERY METRIC BELOW IS A DESIGN UNIT and goes through Sc(). PsDocker is not in play here --
'' this is inside a widget's content, not a band walk -- so the scaling is explicit, and the
'' one rule is that no literal pixel ever reaches a coordinate.
'' ========================================================================================

#pragma once
#include once "mdLayout.bi"
'' PsImage is not pulled in by any of the controls the shell uses, and it is not a widget --
'' it is a decoded bitmap with a natural size, which is exactly what laying an image out needs.
#include once "ui/controls/PsImage.inc"

'' ---- metrics, DESIGN units -------------------------------------------------------------
private const MDL_PAD_X       = 22     '' content margin, both sides
private const MDL_PAD_TOP     = 14
private const MDL_PAD_BOTTOM  = 28
private const MDL_PARA_GAP    = 11
private const MDL_HEAD_TOP    = 20
private const MDL_HEAD_BOT    = 7
private const MDL_HEAD_RULE   = 6      '' gap between an h1/h2 and its underline
private const MDL_LIST_GAP    = 4      '' between items of one list
private const MDL_LIST_MARK   = 20     '' the gutter a bullet or ordinal sits in
private const MDL_CODE_PAD    = 9      '' inside the slab
private const MDL_CODE_GAP    = 11     '' outside it
private const MDL_QUOTE_IND   = 16     '' per nesting level
private const MDL_QUOTE_BAR   = 3
private const MDL_RULE_GAP    = 12
private const MDL_CELL_PAD    = 8
private const MDL_CELL_MIN    = 54     '' a column is never squeezed below this
private const MDL_ROW_GAP     = 5      '' above and below a cell's text
private const MDL_TABLE_GAP   = 12
private const MDL_IMG_GAP     = 10
private const MDL_LEADING     = 3      '' added to ascent+descent for the line box

'' A page with more images than this drops the rest rather than growing an array of a UDT
'' that owns a Blend2D handle. The corpus's busiest page has four.
private const MDL_MAX_IMAGES  = 64

dim shared as single g_mdlScale = 1.0

private function Sc( byval n as long ) as long
    return PsScaleBy( n, g_mdlScale )
end function


'' ---------------------------------------------------------------------------------------
'' the image cache
'' ---------------------------------------------------------------------------------------
type MdImgEntry
    sPath as string
    bOk   as boolean
    img   as PsImage
end type

'' A FIXED array, not a growing one: PsImage owns a BLImageCore, and `redim preserve` over a
'' UDT that owns a foreign handle is a bet this file does not need to take.
dim shared as MdImgEntry g_mdImg(0 to MDL_MAX_IMAGES - 1)
dim shared as long g_mdImgCount

sub MdLayoutFreeImages()
    for i as long = 0 to g_mdImgCount - 1
        if g_mdImg(i).bOk then g_mdImg(i).img.Dispose()
        g_mdImg(i).bOk = false
        g_mdImg(i).sPath = ""
    next
    g_mdImgCount = 0
end sub

function MdLayoutImageCount() as long
    return g_mdImgCount
end function

function MdLayoutImagePtr( byval nIndex as long ) as any ptr
    if (nIndex < 0) orelse (nIndex >= g_mdImgCount) then return 0
    if g_mdImg(nIndex).bOk = false then return 0
    '' The BLImageCore itself, not the PsImage wrapper -- PaintImage takes the handle.
    return @g_mdImg(nIndex).img.img_
end function

sub MdLayoutImageSize( byval nIndex as long, byref nW as long, byref nH as long )
    nW = 0 : nH = 0
    if (nIndex < 0) orelse (nIndex >= g_mdImgCount) then exit sub
    if g_mdImg(nIndex).bOk = false then exit sub
    nW = g_mdImg(nIndex).img.NaturalWidth()
    nH = g_mdImg(nIndex).img.NaturalHeight()
end sub

'' Loads once per page and hands back an index. A missing file is cached as a FAILURE too,
'' so a broken link is not retried on every relayout.
private function LoadImage( byref sPath as string, byref nW as long, byref nH as long ) as long
    for i as long = 0 to g_mdImgCount - 1
        if g_mdImg(i).sPath = sPath then
            nW = g_mdImg(i).img.NaturalWidth()
            nH = g_mdImg(i).img.NaturalHeight()
            if g_mdImg(i).bOk = false then nW = 0 : nH = 0
            return i
        end if
    next
    if g_mdImgCount >= MDL_MAX_IMAGES then
        nW = 0 : nH = 0
        return -1
    end if
    dim as long n = g_mdImgCount
    g_mdImg(n).sPath = sPath
    dim as DWSTRING sDw
    sDw.Utf8 = sPath
    g_mdImg(n).bOk = g_mdImg(n).img.Load( sDw )
    if g_mdImg(n).bOk then
        nW = g_mdImg(n).img.NaturalWidth()
        nH = g_mdImg(n).img.NaturalHeight()
    else
        nW = 0 : nH = 0
    end if
    g_mdImgCount += 1
    return n
end function


'' ---------------------------------------------------------------------------------------
'' the arenas
'' ---------------------------------------------------------------------------------------
sub MdPageClear( byref page as MdPage )
    page.nLine = 0 : page.nRun = 0 : page.nDeco = 0
    page.nHeight = 0 : page.nWidth = 0 : page.nCodeMaxW = 0
    page.nContentRight = 0
end sub

private function AddRun( byref page as MdPage ) as long
    if page.nRun > ubound(page.run) then
        redim preserve page.run(0 to (page.nRun + 1) * 2)
    end if
    with page.run(page.nRun)
        .nFont = MDF_BODY : .nKind = MDRK_TEXT : .nTok = MDT_TEXT : .nInl = -1
        .x = 0 : .w = 0 : .sText = ""
    end with
    page.nRun += 1
    return page.nRun - 1
end function

private function AddLine( byref page as MdPage ) as long
    if page.nLine > ubound(page.line) then
        redim preserve page.line(0 to (page.nLine + 1) * 2)
    end if
    with page.line(page.nLine)
        .y = 0 : .h = 0 : .nBaseline = 0 : .nFirstRun = 0 : .nRunCount = 0
    end with
    page.nLine += 1
    return page.nLine - 1
end function

private sub AddDeco( byref page as MdPage, byval nKind as long, byval x as long, _
                     byval y as long, byval w as long, byval h as long, _
                     byval nImage as long = -1 )
    if page.nDeco > ubound(page.deco) then
        redim preserve page.deco(0 to (page.nDeco + 1) * 2)
    end if
    with page.deco(page.nDeco)
        .nKind = nKind : .x = x : .y = y : .w = w : .h = h : .nImage = nImage
    end with
    page.nDeco += 1
end sub


'' ---------------------------------------------------------------------------------------
'' measuring
'' ---------------------------------------------------------------------------------------
'' 26.6 fixed point throughout the wrapper, converted once at the point a coordinate is
'' stored. Rounding each word to whole pixels first accumulates a visible drift over a long
'' line -- about a pixel every eight words at 15px.
private function Meas64( byval nFont as long, byref s as string ) as long
    if len(s) = 0 then return 0
    return TE_MeasureText64( *MdFontPtr(nFont), s )
end function

private function SpaceW64( byval nFont as long ) as long
    return TE_MeasureText64( *MdFontPtr(nFont), " " )
end function


'' ---------------------------------------------------------------------------------------
'' a segment: one styled piece of a paragraph, before wrapping
'' ---------------------------------------------------------------------------------------
type MdSeg
    nFont  as long
    nKind  as long
    nInl   as long
    sText  as string
    bBreak as boolean      '' a hard <br> after this segment
end type

private sub AddSeg( segs() as MdSeg, byref n as long, byval nFont as long, _
                    byval nKind as long, byval nInl as long, byref sText as string, _
                    byval bBreak as boolean )
    if n > ubound(segs) then redim preserve segs(0 to (n + 1) * 2)
    segs(n).nFont  = nFont
    segs(n).nKind  = nKind
    segs(n).nInl   = nInl
    segs(n).sText  = sText
    segs(n).bBreak = bBreak
    n += 1
end sub

'' MdDoc inline slice -> segments. nHeadFont overrides the font for a heading, where the
'' whole line is one face and the bold/italic bits must not switch it to the body size.
private sub SegsFromInline( byref doc as MdDoc, byval nFirst as long, byval nCount as long, _
                            segs() as MdSeg, byref nSegs as long, byval nHeadFont as long )
    nSegs = 0
    for k as long = nFirst to nFirst + nCount - 1
        dim as long nStyle = doc.inl(k).nStyle
        if (nStyle and MDS_IMAGE) <> 0 then continue for   '' handled as a deco, not a run

        dim as long nFont
        if nHeadFont >= 0 then
            '' Inside a heading, only CODE gets to change the face -- a monospaced token in an
            '' h2 is meaningful, a smaller bold is not.
            if (nStyle and MDS_CODE) <> 0 then nFont = MDF_MONO else nFont = nHeadFont
        else
            nFont = MdFontForInline( nStyle )
        end if

        dim as long nKind = MDRK_TEXT
        if (nStyle and MDS_CODE) <> 0 then nKind = MDRK_CODE
        if (nStyle and MDS_LINK) <> 0 then nKind = MDRK_LINK

        AddSeg( segs(), nSegs, nFont, nKind, k, doc.inl(k).sText, doc.inl(k).bBreak )
    next
end sub

'' The unwrapped width, in 26.6. Table columns are sized from this.
private function SegsNatural64( segs() as MdSeg, byval nSegs as long ) as long
    dim as long nW = 0
    for i as long = 0 to nSegs - 1
        nW += Meas64( segs(i).nFont, segs(i).sText )
    next
    return nW
end function

'' The widest single WORD, in 26.6. This is a column's real floor: squeeze it narrower and
'' the wrapper has no legal break left and starts splitting mid-word, which turned
'' "AfxStrClipLeft" into "AfxStrCli / pLeft" in the AfxNova reference tables. Natural width
'' says what a column WANTS; this says what it cannot go below without lying about the text.
private function SegsWidestWord64( segs() as MdSeg, byval nSegs as long ) as long
    dim as long nMax = 0
    for i as long = 0 to nSegs - 1
        dim as string s = segs(i).sText
        dim as long a = 0
        while a < len(s)
            while (a < len(s)) andalso (s[a] = 32)
                a += 1
            wend
            dim as long b = a
            while (b < len(s)) andalso (s[b] <> 32)
                b += 1
            wend
            if b > a then
                dim as long w = Meas64( segs(i).nFont, mid(s, a + 1, b - a) )
                if w > nMax then nMax = w
            end if
            a = b
        wend
    next
    return nMax
end function


'' ---------------------------------------------------------------------------------------
'' THE WRAPPER
''
'' State in a UDT because FreeBASIC has no nested procedures and the alternative is six
'' byref parameters on four helpers.
'' ---------------------------------------------------------------------------------------
type MdWrap
    nX        as long      '' left edge, content coordinates
    nMaxW     as long      '' the wrap width from nX
    nHang     as long      '' continuation-line indent, for hanging list text
    nY        as long      '' top of the CURRENT line
    nPenX64   as long      '' pen position within the line, 26.6, relative to the line's left
    nLineLeft as long      '' nX, or nX + nHang on a continuation
    nFirstRun as long
    nCurRun   as long      '' the run being extended, or -1
    nMaxAsc   as long
    nMaxDesc  as long
    bEmpty    as boolean   '' nothing placed on this line yet
    bFirst    as boolean   '' the next flush ends the FIRST line of the block
    nFallback as long      '' the font an empty line takes its height from
end type

private sub WrapBegin( byref st as MdWrap, byref page as MdPage, byval nX as long, _
                       byval nMaxW as long, byval nHang as long, byval nY as long, _
                       byval nFallback as long )
    st.nX = nX : st.nMaxW = nMaxW : st.nHang = nHang : st.nY = nY
    st.nPenX64 = 0 : st.nLineLeft = nX
    st.nFirstRun = page.nRun : st.nCurRun = -1
    st.nMaxAsc = 0 : st.nMaxDesc = 0
    st.bEmpty = true : st.bFirst = true
    st.nFallback = nFallback
end sub

private sub WrapNoteFont( byref st as MdWrap, byval nFont as long )
    dim as PsTextEngine ptr te = MdFontPtr(nFont)
    if te->ascent  > st.nMaxAsc  then st.nMaxAsc  = te->ascent
    if te->descent > st.nMaxDesc then st.nMaxDesc = te->descent
end sub

'' Close the current line. Emits an MdLine covering every run added since the last flush and
'' advances nY. An EMPTY line still gets a height -- a lone <br> is a blank line, not nothing.
private sub WrapFlush( byref st as MdWrap, byref page as MdPage )
    if st.nMaxAsc = 0 then WrapNoteFont( st, st.nFallback )

    dim as long nL = AddLine( page )
    with page.line(nL)
        .y         = st.nY
        .nBaseline = st.nMaxAsc
        .h         = st.nMaxAsc + st.nMaxDesc + Sc(MDL_LEADING)
        .nFirstRun = st.nFirstRun
        .nRunCount = page.nRun - st.nFirstRun
    end with
    st.nY += page.line(nL).h

    st.nFirstRun = page.nRun
    st.nCurRun   = -1
    st.nPenX64   = 0
    st.nMaxAsc   = 0
    st.nMaxDesc  = 0
    st.bEmpty    = true
    st.bFirst    = false
    st.nLineLeft = st.nX + st.nHang
end sub

'' A piece that must not be broken and does not participate in wrapping: a bullet, an
'' ordinal. Placed at the pen and the pen advances past it.
private sub WrapPrefix( byref st as MdWrap, byref page as MdPage, byval nFont as long, _
                        byval nKind as long, byref sText as string )
    dim as long nW64 = Meas64( nFont, sText )
    dim as long r = AddRun( page )
    with page.run(r)
        .nFont = nFont : .nKind = nKind : .nInl = -1
        .x = st.nLineLeft + (st.nPenX64 shr 6)
        .w = (nW64 + 63) shr 6
        .sText = sText
    end with
    st.nPenX64 += nW64
    st.bEmpty = false
    st.nCurRun = -1                 '' never extended by a following word
    WrapNoteFont( st, nFont )
end sub

'' Extend the current run, or start a new one when the segment changed.
private sub WrapPut( byref st as MdWrap, byref page as MdPage, byref seg as MdSeg, _
                     byref sPiece as string, byval nW64 as long )
    dim as boolean bNew = (st.nCurRun < 0)
    if bNew = false then
        with page.run(st.nCurRun)
            if (.nFont <> seg.nFont) orelse (.nKind <> seg.nKind) orelse _
               (.nInl <> seg.nInl) then bNew = true
        end with
    end if
    if bNew then
        st.nCurRun = AddRun( page )
        with page.run(st.nCurRun)
            .nFont = seg.nFont : .nKind = seg.nKind : .nInl = seg.nInl
            .x = st.nLineLeft + (st.nPenX64 shr 6)
            .w = 0 : .sText = ""
        end with
    end if
    page.run(st.nCurRun).sText &= sPiece
    st.nPenX64 += nW64
    '' Re-measured from the whole accumulated string rather than summed, because THIS width
    '' is what the view draws and hit-tests against -- the summing shortcut is for deciding
    '' where to break, not for the final box.
    page.run(st.nCurRun).w = (Meas64(seg.nFont, page.run(st.nCurRun).sText) + 63) shr 6
    st.bEmpty = false
    WrapNoteFont( st, seg.nFont )
end sub

private sub WrapSeg( byref st as MdWrap, byref page as MdPage, byref seg as MdSeg )
    dim as string s = seg.sText
    dim as long nLen = len(s)
    dim as long nSpace64 = SpaceW64( seg.nFont )
    dim as long nAvail64 = st.nMaxW shl 6

    dim as long i = 0
    while i < nLen
        '' leading spaces of this token
        dim as long nSpaces = 0
        while (i < nLen) andalso (s[i] = 32)
            nSpaces += 1
            i += 1
        wend
        dim as long nWordStart = i
        while (i < nLen) andalso (s[i] <> 32)
            i += 1
        wend
        if i = nWordStart then
            '' trailing spaces with no word after them. A space at the end of a run is a real
            '' separator between two styled pieces -- "a **b**" -- so it is placed rather
            '' than dropped, but never at the start of a line.
            if (nSpaces > 0) andalso (st.bEmpty = false) then
                WrapPut( st, page, seg, space(nSpaces), nSpace64 * nSpaces )
            end if
            exit while
        end if

        dim as string sWord = mid(s, nWordStart + 1, i - nWordStart)
        dim as long nWord64 = Meas64( seg.nFont, sWord )
        dim as long nLead64 = 0
        if (nSpaces > 0) andalso (st.bEmpty = false) then nLead64 = nSpace64 * nSpaces

        if (st.bEmpty = false) andalso (st.nPenX64 + nLead64 + nWord64 > nAvail64) then
            WrapFlush( st, page )
            nLead64 = 0
        end if

        '' A word wider than the whole column, alone on its line: broken mid-word, byte by
        '' byte, never inside a UTF-8 sequence. PsTextWrap's rule, and the alternative is a
        '' line that overflows the view silently.
        if st.bEmpty andalso (nWord64 > nAvail64) andalso (nAvail64 > 0) then
            dim as long k = 0
            while k < len(sWord)
                dim as long nTake = 0
                dim as long nAcc64 = 0
                while k + nTake < len(sWord)
                    '' advance to the end of this UTF-8 character
                    dim as long nCh = 1
                    dim as long c = sWord[k + nTake]
                    if c >= 240 then
                        nCh = 4
                    elseif c >= 224 then
                        nCh = 3
                    elseif c >= 192 then
                        nCh = 2
                    end if
                    dim as string sTry = mid(sWord, k + 1, nTake + nCh)
                    dim as long nTry64 = Meas64( seg.nFont, sTry )
                    if (nTry64 > nAvail64) andalso (nTake > 0) then exit while
                    nTake += nCh
                    nAcc64 = nTry64
                wend
                if nTake = 0 then nTake = 1
                WrapPut( st, page, seg, mid(sWord, k + 1, nTake), nAcc64 )
                k += nTake
                if k < len(sWord) then WrapFlush( st, page )
            wend
        else
            if nLead64 > 0 then WrapPut( st, page, seg, space(nSpaces), nLead64 )
            WrapPut( st, page, seg, sWord, nWord64 )
        end if
    wend

    if seg.bBreak then WrapFlush( st, page )
end sub

'' Returns the y BELOW the last line.
private function WrapEnd( byref st as MdWrap, byref page as MdPage ) as long
    if (st.bEmpty = false) orelse cbool(page.nRun > st.nFirstRun) orelse st.bFirst then
        WrapFlush( st, page )
    end if
    return st.nY
end function

'' The whole of the common case: wrap a segment list at (nX, nY) and hand back the bottom.
private function WrapSegs( byref page as MdPage, segs() as MdSeg, byval nSegs as long, _
                           byval nX as long, byval nY as long, byval nMaxW as long, _
                           byval nHang as long, byval nFallback as long ) as long
    dim as MdWrap st
    WrapBegin( st, page, nX, nMaxW, nHang, nY, nFallback )
    for i as long = 0 to nSegs - 1
        WrapSeg( st, page, segs(i) )
    next
    return WrapEnd( st, page )
end function

'' The inline-code pill. Emitted AFTER wrapping, by walking the lines just produced, because
'' a code run's final x and width are not known until it has been placed and possibly split
'' across a break.
private sub CodePills( byref page as MdPage, byval nLineFrom as long )
    for l as long = nLineFrom to page.nLine - 1
        for r as long = page.line(l).nFirstRun to _
                        page.line(l).nFirstRun + page.line(l).nRunCount - 1
            if page.run(r).nKind = MDRK_CODE then
                AddDeco( page, MDD_INLCODEBG, page.run(r).x - Sc(2), _
                         page.line(l).y, page.run(r).w + Sc(4), page.line(l).h )
            end if
        next
    next
end sub


'' ---------------------------------------------------------------------------------------
'' THE BLOCK WALK
'' ---------------------------------------------------------------------------------------
sub MdLayoutPage( byref doc as MdDoc, byref page as MdPage, byval nWidth as long, _
                  byval fScale as single, byref sBaseDir as string )

    MdPageClear( page )
    g_mdlScale  = iif(fScale > 0, fScale, 1.0)
    page.nWidth = nWidth

    dim as long nLeft0  = Sc(MDL_PAD_X)
    dim as long nRight0 = nWidth - Sc(MDL_PAD_X)
    if nRight0 <= nLeft0 then nRight0 = nLeft0 + Sc(MDL_CELL_MIN)

    page.nContentRight = nRight0
    dim as long nY = Sc(MDL_PAD_TOP)
    dim as MdSeg segs(any)
    dim as long nSegs = 0

    dim as long i = 0
    while i < doc.nBlk
        dim as long nQuote = doc.blk(i).nQuote
        dim as long nQInd  = nQuote * Sc(MDL_QUOTE_IND)
        dim as long nLeft  = nLeft0 + nQInd
        dim as long nMaxW  = nRight0 - nLeft
        if nMaxW < Sc(MDL_CELL_MIN) then nMaxW = Sc(MDL_CELL_MIN)
        dim as long nQuoteTop = nY

        select case doc.blk(i).kind

        '' ---- heading -----------------------------------------------------------------
        case MDBK_HEADING
            if page.nLine > 0 then nY += Sc(MDL_HEAD_TOP)
            dim as long nFont = MdFontForHeading( doc.blk(i).nLevel )
            SegsFromInline( doc, doc.blk(i).nFirstInl, doc.blk(i).nInlCount, segs(), nSegs, nFont )
            dim as long nFrom = page.nLine
            nY = WrapSegs( page, segs(), nSegs, nLeft, nY, nMaxW, 0, nFont )
            CodePills( page, nFrom )
            '' h1 and h2 carry an underline, which is what makes a long reference page
            '' skimmable. h3 and below do not -- the size difference is enough and a page of
            '' rules reads as a form.
            if doc.blk(i).nLevel <= 2 then
                nY += Sc(MDL_HEAD_RULE)
                AddDeco( page, MDD_RULE, nLeft, nY, nMaxW, Sc(1) )
                nY += Sc(1)
            end if
            nY += Sc(MDL_HEAD_BOT)
            i += 1

        '' ---- paragraph ---------------------------------------------------------------
        case MDBK_PARAGRAPH
            '' An image standing alone in its paragraph -- which is how every image in the
            '' corpus appears -- becomes a deco rather than a run.
            dim as boolean bDidImage = false
            for k as long = doc.blk(i).nFirstInl to doc.blk(i).nFirstInl + doc.blk(i).nInlCount - 1
                if (doc.inl(k).nStyle and MDS_IMAGE) <> 0 then
                    dim as long nIw, nIh
                    dim as string sPath = doc.inl(k).sHref
                    '' Absolute and remote paths are left alone; everything else resolves
                    '' against the document's own folder.
                    dim as boolean bRel = true
                    if instr(sPath, "://") > 0 then bRel = false
                    if (len(sPath) > 1) andalso (mid(sPath, 2, 1) = ":") then bRel = false
                    if (len(sPath) > 0) andalso ((sPath[0] = 47) orelse (sPath[0] = 92)) then bRel = false
                    dim as long nImg = -1
                    if bRel then
                        nImg = LoadImage( sBaseDir & sPath, nIw, nIh )
                    else
                        nImg = LoadImage( sPath, nIw, nIh )
                    end if
                    if (nIw > 0) andalso (nIh > 0) then
                        nY += Sc(MDL_IMG_GAP)
                        '' Never enlarged past its natural size -- a 32px icon stretched to
                        '' the pane width is worse than a 32px icon.
                        dim as long nDw = nIw, nDh = nIh
                        if nDw > nMaxW then
                            nDh = clng(nIh * (nMaxW / nIw))
                            nDw = nMaxW
                        end if
                        AddDeco( page, MDD_IMAGE, nLeft, nY, nDw, nDh, nImg )
                        nY += nDh + Sc(MDL_IMG_GAP)
                        bDidImage = true
                    end if
                end if
            next

            SegsFromInline( doc, doc.blk(i).nFirstInl, doc.blk(i).nInlCount, segs(), nSegs, -1 )
            if nSegs > 0 then
                dim as long nFrom = page.nLine
                nY = WrapSegs( page, segs(), nSegs, nLeft, nY, nMaxW, 0, MDF_BODY )
                CodePills( page, nFrom )
                nY += Sc(MDL_PARA_GAP)
            elseif bDidImage = false then
                nY += Sc(MDL_PARA_GAP)
            end if
            i += 1

        '' ---- rule --------------------------------------------------------------------
        case MDBK_RULE
            nY += Sc(MDL_RULE_GAP)
            AddDeco( page, MDD_RULE, nLeft, nY, nMaxW, Sc(1) )
            nY += Sc(1) + Sc(MDL_RULE_GAP)
            i += 1

        '' ---- a fenced or indented code block -----------------------------------------
        case MDBK_CODE
            nY += Sc(MDL_CODE_GAP)
            dim as long nSlabTop = nY
            nY += Sc(MDL_CODE_PAD)

            dim as long nLang = MdLangFromName( doc.blk(i).sLang )
            dim as long nState = MDH_STATE_NONE
            dim as MdSpan spans(0 to 511)
            dim as PsTextEngine ptr teM = MdFontPtr(MDF_MONO)
            dim as long nLineH = teM->ascent + teM->descent + Sc(MDL_LEADING)
            dim as long nCodeLeft = nLeft + Sc(MDL_CODE_PAD)

            '' Split on newlines here rather than storing a line array in the block: a fence
            '' is re-split once per relayout, and relayout happens on a width change only.
            dim as string sBody = doc.blk(i).sText
            dim as long nStart = 0
            dim as long p = 0
            while p <= len(sBody)
                if (p = len(sBody)) orelse (sBody[p] = 10) then
                    dim as string sCodeLine = mid(sBody, nStart + 1, p - nStart)
                    dim as long nSp = MdHilite( nLang, sCodeLine, nState, spans(), 512 )
                    dim as long nL = AddLine( page )
                    page.line(nL).y = nY
                    page.line(nL).h = nLineH
                    page.line(nL).nBaseline = teM->ascent
                    page.line(nL).nFirstRun = page.nRun
                    dim as long nPen64 = 0
                    for k as long = 0 to nSp - 1
                        dim as string sTok = mid(sCodeLine, spans(k).nStart + 1, spans(k).nLen)
                        dim as long r = AddRun( page )
                        with page.run(r)
                            .nFont = MDF_MONO : .nKind = MDRK_FENCE
                            .nTok  = spans(k).nKind : .nInl = -1
                            .x = nCodeLeft + (nPen64 shr 6)
                            .sText = sTok
                            .w = (Meas64(MDF_MONO, sTok) + 63) shr 6
                        end with
                        nPen64 += Meas64( MDF_MONO, sTok )
                    next
                    page.line(nL).nRunCount = page.nRun - page.line(nL).nFirstRun
                    dim as long nRowW = nCodeLeft + (nPen64 shr 6)
                    if nRowW > page.nCodeMaxW then page.nCodeMaxW = nRowW
                    nY += nLineH
                    nStart = p + 1
                end if
                p += 1
            wend

            nY += Sc(MDL_CODE_PAD)
            AddDeco( page, MDD_CODEBG, nLeft, nSlabTop, nMaxW, nY - nSlabTop )
            nY += Sc(MDL_CODE_GAP)
            i += 1

        '' ---- a list item -------------------------------------------------------------
        case MDBK_LISTITEM
            dim as long nMark = Sc(MDL_LIST_MARK)
            dim as long nInd  = doc.blk(i).nLevel      '' the source indent, in characters
            dim as long nItemX = nLeft + (nInd \ 2) * nMark
            SegsFromInline( doc, doc.blk(i).nFirstInl, doc.blk(i).nInlCount, segs(), nSegs, -1 )

            dim as MdWrap st
            dim as long nFrom = page.nLine
            WrapBegin( st, page, nItemX, nRight0 - nItemX, nMark, nY, MDF_BODY )
            dim as string sMark
            if doc.blk(i).bOrdered then
                sMark = str(doc.blk(i).nOrdinal) & ". "
            else
                '' U+2022 BULLET, written as its UTF-8 bytes: fbc processes backslash escapes
                '' in this translation unit, so the escaped form would not survive.
                sMark = chr(226) & chr(128) & chr(162) & "  "
            end if
            WrapPrefix( st, page, MDF_BODY, MDRK_MARKER, sMark )
            for k as long = 0 to nSegs - 1
                WrapSeg( st, page, segs(k) )
            next
            nY = WrapEnd( st, page )
            CodePills( page, nFrom )
            nY += Sc(MDL_LIST_GAP)
            i += 1

        '' ---- a table -----------------------------------------------------------------
        '' Gathered whole, because a column width is a property of every row at once and a
        '' per-block walk could not see them.
        case MDBK_TABLEROW
            dim as long nFirstRow = i
            dim as long nRows = 0
            dim as long nCols = 0
            while (i < doc.nBlk) andalso (doc.blk(i).kind = MDBK_TABLEROW) andalso _
                  (doc.blk(i).nQuote = nQuote)
                if doc.blk(i).nCellCount > nCols then nCols = doc.blk(i).nCellCount
                nRows += 1
                i += 1
            wend
            if nCols < 1 then nCols = 1

            '' 1. natural width of every column, from the unwrapped cell text
            dim as long colW(0 to nCols - 1)
            for c as long = 0 to nCols - 1
                colW(c) = 0
            next
            dim as long colMin(0 to nCols - 1)
            for c as long = 0 to nCols - 1
                colMin(c) = 0
            next
            for r as long = nFirstRow to nFirstRow + nRows - 1
                dim as long nHead = iif(doc.blk(r).bHeaderRow, MDF_BODY_B, -1)
                for c as long = 0 to doc.blk(r).nCellCount - 1
                    dim as long nCell = doc.blk(r).nFirstCell + c
                    SegsFromInline( doc, doc.cell(nCell).nFirstInl, _
                                    doc.cell(nCell).nInlCount, segs(), nSegs, nHead )
                    dim as long nW = (SegsNatural64(segs(), nSegs) + 63) shr 6
                    if nW > colW(c) then colW(c) = nW
                    dim as long nM = (SegsWidestWord64(segs(), nSegs) + 63) shr 6
                    if nM > colMin(c) then colMin(c) = nM
                next
            next

            '' 2. fit them to the available width. A table narrower than the pane is
            '' EXPANDED to fill it -- a two-column reference table floating at 40% of the
            '' width looks like a rendering failure.
            ''
            '' A COLUMN IS NEVER SQUEEZED BELOW ITS WIDEST WORD. Proportional sizing alone
            '' gave the AfxNova reference tables a name column narrower than the names in it,
            '' and the wrapper -- correctly, having no legal break left -- split them:
            '' "AfxStrClipLeft" rendered as "AfxStrCli / pLeft". The floor is capped at half
            '' the table so that one very long token cannot starve every other column.
            dim as long nPadTotal = nCols * 2 * Sc(MDL_CELL_PAD)
            dim as long nAvail = (nRight0 - nLeft) - nPadTotal
            if nAvail < nCols * Sc(MDL_CELL_MIN) then nAvail = nCols * Sc(MDL_CELL_MIN)
            for c as long = 0 to nCols - 1
                if colMin(c) < Sc(MDL_CELL_MIN) then colMin(c) = Sc(MDL_CELL_MIN)
                if colMin(c) > nAvail \ 2 then colMin(c) = nAvail \ 2
            next

            dim as long nNat = 0
            for c as long = 0 to nCols - 1
                nNat += colW(c)
            next
            if nNat < 1 then nNat = 1
            dim as long nUsed = 0
            for c as long = 0 to nCols - 1
                colW(c) = clng(colW(c) * (nAvail / nNat))
                if colW(c) < colMin(c) then colW(c) = colMin(c)
                nUsed += colW(c)
            next

            '' Holding the floors can push the total past the width. The overflow is taken
            '' back only from columns that are ABOVE their own floor, proportionally to the
            '' slack they have -- so the wide description column gives way and the narrow
            '' name column does not.
            if nUsed > nAvail then
                dim as long nSlack = 0
                for c as long = 0 to nCols - 1
                    nSlack += colW(c) - colMin(c)
                next
                if nSlack > 0 then
                    dim as long nCut = nUsed - nAvail
                    if nCut > nSlack then nCut = nSlack
                    dim as long nDone = 0
                    for c as long = 0 to nCols - 1
                        dim as long nHave = colW(c) - colMin(c)
                        dim as long nTake = clng(nCut * (nHave / nSlack))
                        if nTake > nHave then nTake = nHave
                        colW(c) -= nTake
                        nDone += nTake
                    next
                    '' Rounding leaves a few pixels; take them from the widest column that
                    '' still has slack rather than spreading a second pass over all of them.
                    dim as long nLeftOver = nCut - nDone
                    while nLeftOver > 0
                        dim as long nBest = -1
                        for c as long = 0 to nCols - 1
                            if colW(c) > colMin(c) then
                                if (nBest < 0) orelse (colW(c) > colW(nBest)) then nBest = c
                            end if
                        next
                        if nBest < 0 then exit while
                        colW(nBest) -= 1
                        nLeftOver -= 1
                    wend
                end if
                nUsed = 0
                for c as long = 0 to nCols - 1
                    nUsed += colW(c)
                next
            end if

            '' Rounding pushes the total off by a pixel or two; the last column absorbs it, so
            '' the right edge is exact rather than nearly right. Only when it has room to.
            if (nAvail - nUsed) > 0 then colW(nCols - 1) += nAvail - nUsed

            nY += Sc(MDL_TABLE_GAP)
            dim as long nTableTop = nY

            for r as long = nFirstRow to nFirstRow + nRows - 1
                dim as long nRowTop = nY
                dim as long nRowBot = nY
                dim as long nCellX = nLeft
                dim as long nHead = iif(doc.blk(r).bHeaderRow, MDF_BODY_B, -1)
                dim as long nFrom = page.nLine

                for c as long = 0 to nCols - 1
                    dim as long nTextX = nCellX + Sc(MDL_CELL_PAD)
                    if c < doc.blk(r).nCellCount then
                        dim as long nCell = doc.blk(r).nFirstCell + c
                        SegsFromInline( doc, doc.cell(nCell).nFirstInl, _
                                        doc.cell(nCell).nInlCount, segs(), nSegs, nHead )
                        if nSegs > 0 then
                            dim as long nBot = WrapSegs( page, segs(), nSegs, nTextX, _
                                                         nRowTop + Sc(MDL_ROW_GAP), colW(c), _
                                                         0, iif(nHead >= 0, nHead, MDF_BODY) )
                            if nBot > nRowBot then nRowBot = nBot
                        end if
                    end if
                    nCellX += colW(c) + 2 * Sc(MDL_CELL_PAD)
                next

                if nRowBot = nRowTop then nRowBot = nRowTop + MdFontPtr(MDF_BODY)->lineHeight
                nRowBot += Sc(MDL_ROW_GAP)
                CodePills( page, nFrom )

                if doc.blk(r).bHeaderRow then
                    AddDeco( page, MDD_TABLEHEADBG, nLeft, nRowTop, nRight0 - nLeft, _
                             nRowBot - nRowTop )
                end if
                AddDeco( page, MDD_TABLELINE, nLeft, nRowBot, nRight0 - nLeft, Sc(1) )
                nY = nRowBot + Sc(1)
            next

            '' A single line above the first row closes the box at the top; the per-row lines
            '' have already closed every other edge that matters.
            AddDeco( page, MDD_TABLELINE, nLeft, nTableTop, nRight0 - nLeft, Sc(1) )
            nY += Sc(MDL_TABLE_GAP)

        case else
            i += 1
        end select

        '' The quote bar spans everything this block produced, at every level it is nested
        '' in -- so a paragraph three deep gets three bars, which is how the nesting is
        '' visible at all.
        if (nQuote > 0) andalso (nY > nQuoteTop) then
            for q as long = 1 to nQuote
                AddDeco( page, MDD_QUOTEBAR, nLeft0 + (q - 1) * Sc(MDL_QUOTE_IND), _
                         nQuoteTop, Sc(MDL_QUOTE_BAR), nY - nQuoteTop )
            next
        end if
    wend

    page.nHeight = nY + Sc(MDL_PAD_BOTTOM)
end sub


function MdPageHitRun( byref page as MdPage, byval px as long, byval py as long ) as long
    for l as long = 0 to page.nLine - 1
        if (py >= page.line(l).y) andalso (py < page.line(l).y + page.line(l).h) then
            for r as long = page.line(l).nFirstRun to _
                            page.line(l).nFirstRun + page.line(l).nRunCount - 1
                if (px >= page.run(r).x) andalso (px < page.run(r).x + page.run(r).w) then
                    return r
                end if
            next
            return -1
        end if
    next
    return -1
end function

function MdPageBlockY( byref page as MdPage, byval nY as long ) as long
    '' Deliberately trivial: the caller already has a content y and this exists so the view
    '' never reaches into page.line itself.
    if nY < 0 then return 0
    if nY > page.nHeight then return page.nHeight
    return nY
end function
