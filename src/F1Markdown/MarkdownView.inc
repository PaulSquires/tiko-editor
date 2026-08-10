'' ========================================================================================
'' MarkdownView.inc -- the implementation. See MarkdownView.bi for the three design notes.
'' ========================================================================================

#pragma once
#include once "MarkdownView.bi"

private const MDV_SB_W     = 11     '' design units
private const MDV_WHEEL    = 54     '' design units per notch -- about three body lines
private const MDV_KEYSTEP  = 40
'' The Copy button on a code fence, DESIGN units.
private const MDV_COPY_W   = 52
private const MDV_COPY_H   = 20
private const MDV_COPY_PAD = 6

private sub MdvOnScroll( byval pSB as any ptr, byval nPos as long, byval ud as any ptr )
    dim as MarkdownView ptr pV = cptr( MarkdownView ptr, ud )
    if pV = 0 then exit sub
    if pV->nScroll <> nPos then
        pV->nScroll = nPos
        pV->Invalidate()
    end if
end sub

constructor MarkdownView()
    '' fbc zeroes neither New nor a stack Dim -- only the constructor runs -- so every field
    '' that is not a managed type is set here explicitly.
    this.pDoc     = new MdDoc
    this.pPage    = new MdPage
    this.nScroll  = 0
    this.nLaidW   = -1
    this.nHotRun  = -1
    this.nHotCode = -1
    this.bOnCopy  = false
    this.nCopiedAt = -1
    this.pfnLink  = 0
    this.pLinkData = 0
    this.bFocusable    = true
    this.bClipsChildren = true

    this.pScroll = new PsVScrollBar
    if this.pScroll <> 0 then
        this.pScroll->bVisible = false
        this.pScroll->OnScroll( @MdvOnScroll, @this )
        this.AddChild( this.pScroll )        '' TRANSFERS OWNERSHIP
    end if

    this.OnThemeChanged()
end constructor

destructor MarkdownView()
    '' pScroll is NOT deleted here: AddChild took ownership and PsWidget's destructor frees
    '' the child list. Deleting it as well is a double free.
    if this.pDoc  <> 0 then delete this.pDoc  : this.pDoc  = 0
    if this.pPage <> 0 then delete this.pPage : this.pPage = 0
end destructor

sub MarkdownView.OnLink( byval pfn as MdLinkProc, byval userdata as any ptr )
    this.pfnLink   = pfn
    this.pLinkData = userdata
end sub

sub MarkdownView.OnThemeChanged()
    '' BY HAND, and this is the override the header warns about. PsThemeApply reaches this
    '' widget; it cannot reach the page model, which carries kinds rather than colours.
    this.clrBack      = PsThemeColorK( "markdown.background",  PSTHEME_BACKGROUND )
    this.clrFore      = PsThemeColorK( "markdown.foreground",  PSTHEME_FOREGROUND )
    this.clrDim       = PsThemeColorK( "markdown.dim",         PSTHEME_FOREGROUNDDIM )
    this.clrLink      = PsThemeColorK( "markdown.link",        PSTHEME_ACCENT )
    this.clrRule      = PsThemeColorK( "markdown.rule",        PSTHEME_BORDER )
    this.clrCodeBg    = PsThemeColorK( "markdown.code.background", PSTHEME_BACKGROUNDALT )
    this.clrCodeFore  = PsThemeColorK( "markdown.code.foreground", PSTHEME_FOREGROUND )
    this.clrInlCodeBg = PsThemeColorK( "markdown.inlinecode.background", PSTHEME_BACKGROUNDALT )
    this.clrQuoteBar  = PsThemeColorK( "markdown.quotebar",    PSTHEME_ACCENT )
    this.clrTableHead = PsThemeColorK( "markdown.table.header", PSTHEME_BACKGROUNDALT )
    this.clrTableLine = PsThemeColorK( "markdown.table.line",  PSTHEME_BORDER )

    '' The syntax roles every tiko .theme already sets, so a fence is highlighted in the
    '' same colours the editor uses for the same language.
    this.clrTok(MDT_TEXT)     = this.clrCodeFore
    this.clrTok(MDT_KEYWORD)  = PsThemeColorK( "markdown.code.keyword", PSTHEME_KEYWORD )
    this.clrTok(MDT_COMMENT)  = PsThemeColorK( "markdown.code.comment", PSTHEME_COMMENT )
    this.clrTok(MDT_STRING)   = PsThemeColorK( "markdown.code.string",  PSTHEME_STRING )
    this.clrTok(MDT_NUMBER)   = PsThemeColorK( "markdown.code.number",  PSTHEME_NUMBER )
    this.clrTok(MDT_OPERATOR) = PsThemeColorK( "markdown.code.operator", PSTHEME_OPERATOR )
    this.clrTok(MDT_PREPROC)  = PsThemeColorK( "markdown.code.preprocessor", PSTHEME_PREPROCESSOR )

    this.Invalidate()
end sub

sub MarkdownView.OnScaleChanged( byval fScale as single )
    '' The FONTS are reopened by the host, once, for the whole application -- ten atlases are
    '' not rebuilt per widget. What must happen here is a relayout, because every metric in
    '' the page was scaled at the time it was produced.
    this.nLaidW = -1
    this.InvalidateLayout()
end sub

'' The Copy button for a slab, in CONTENT coordinates. Top-right, inside the slab, so it
'' never covers the first line of code -- which is the line most worth reading.
function MarkdownView.CopyRect( byval nDeco as long ) as PsRect
    dim as PsRect rc
    if (this.pPage = 0) orelse (nDeco < 0) orelse (nDeco >= this.pPage->nDeco) then return rc
    with this.pPage->deco(nDeco)
        rc.w = this.ScaleX(MDV_COPY_W)
        rc.h = this.ScaleY(MDV_COPY_H)
        rc.x = .x + .w - rc.w - this.ScaleX(MDV_COPY_PAD)
        rc.y = .y + this.ScaleY(MDV_COPY_PAD)
    end with
    return rc
end function

'' The code slab under a content point, or -1.
function MarkdownView.CodeAt( byval px as long, byval py as long ) as long
    if this.pPage = 0 then return -1
    for d as long = 0 to this.pPage->nDeco - 1
        with this.pPage->deco(d)
            if .nKind = MDD_CODEBG then
                if (px >= .x) andalso (px < .x + .w) andalso _
                   (py >= .y) andalso (py < .y + .h) then return d
            end if
        end with
    next
    return -1
end function

function MarkdownView.ContentWidth() as long
    dim as long w = this.bounds.w
    if (this.pScroll <> 0) andalso this.pScroll->bVisible then w -= this.ScaleX(MDV_SB_W)
    if w < 1 then w = 1
    return w
end function

function MarkdownView.ContentHeight() as long
    if this.pPage = 0 then return 0
    return this.pPage->nHeight
end function

function MarkdownView.GetScroll() as long
    return this.nScroll
end function

sub MarkdownView.ScrollTo( byval nY as long )
    dim as long nMax = this.ContentHeight() - this.bounds.h
    if nMax < 0 then nMax = 0
    dim as long n = nY
    if n > nMax then n = nMax
    if n < 0 then n = 0
    if n = this.nScroll then exit sub
    this.nScroll = n
    if this.pScroll <> 0 then this.pScroll->SetPos( n )   '' SILENT
    this.Invalidate()
end sub

sub MarkdownView.ScrollBy( byval nDelta as long )
    this.ScrollTo( this.nScroll + nDelta )
end sub

sub MarkdownView.SetMarkdown( byref sUtf8 as string, byref sBase as string )
    this.sSource  = sUtf8
    this.sBaseDir = sBase
    '' Images are cached per PAGE, so the previous document's are released here rather than
    '' accumulating across a session of navigation.
    MdLayoutFreeImages()
    if this.pDoc <> 0 then MdParse( this.sSource, *this.pDoc )
    this.nScroll = 0
    this.nHotRun = -1
    this.nLaidW  = -1
    this.InvalidateLayout()
    this.Invalidate()
end sub

sub MarkdownView.OnLayout()
    if (this.pDoc = 0) orelse (this.pPage = 0) then exit sub
    if (this.bounds.w <= 0) orelse (this.bounds.h <= 0) then exit sub

    dim as single f = this.SurfaceScale()
    dim as long nSbW = this.ScaleX(MDV_SB_W)

    '' TWO PASSES, AND THE SECOND IS NOT OPTIONAL. Showing the scrollbar narrows the content,
    '' which re-wraps every paragraph, which changes the height -- and a document that is one
    '' line taller than the viewport only WITHOUT the bar would otherwise flicker between the
    '' two states on every layout. Deciding twice and stopping settles it.
    for pass as long = 0 to 1
        dim as long nW = this.bounds.w
        if (this.pScroll <> 0) andalso this.pScroll->bVisible then nW -= nSbW
        if nW < 1 then nW = 1

        if nW <> this.nLaidW then
            MdLayoutPage( *this.pDoc, *this.pPage, nW, f, this.sBaseDir )
            this.nLaidW = nW
        end if

        dim as boolean bNeed = (this.pPage->nHeight > this.bounds.h)
        if this.pScroll = 0 then exit for
        if this.pScroll->bVisible = bNeed then exit for
        this.pScroll->bVisible = bNeed
        this.nLaidW = -1
    next

    if this.pScroll <> 0 then
        if this.pScroll->bVisible then
            this.pScroll->SetBounds( PsRc(this.bounds.w - nSbW, 0, nSbW, this.bounds.h) )
            this.pScroll->SetRange( this.pPage->nHeight, this.bounds.h, this.nScroll )
        end if
    end if

    '' A window that grew can leave the view scrolled past the end.
    dim as long nMax = this.ContentHeight() - this.bounds.h
    if nMax < 0 then nMax = 0
    if this.nScroll > nMax then
        this.nScroll = nMax
        if this.pScroll <> 0 then this.pScroll->SetPos( this.nScroll )
    end if
end sub


sub MarkdownView.OnPaint( byval p as PsBufferPaint_ ptr )
    if p = 0 then exit sub
    dim as PsBufferPaint ptr q = cptr( PsBufferPaint ptr, p )
    if this.pPage = 0 then exit sub

    dim as PsRect rcAll = PsRc(0, 0, this.bounds.w, this.bounds.h)
    q->SetBackColor( this.clrBack )
    q->PaintRect( @rcAll )

    dim as long nCw = this.ContentWidth()
    '' CLIPPED TO THE LAYOUT'S CONTENT BOX, NOT TO THE CLIENT WIDTH. A code fence does not
    '' wrap by design, so a long line runs past the right margin -- and with the client
    '' width as the clip it escapes its own slab and reaches the window edge, which reads as
    '' a painting bug rather than as content that continues.
    dim as long nClipW = nCw
    if (this.pPage->nContentRight > 0) andalso (this.pPage->nContentRight < nClipW) then
        nClipW = this.pPage->nContentRight
    end if
    dim as PsRect rcContent = PsRc(0, 0, nClipW, this.bounds.h)
    dim as PsRect rcClip = q->Abs_( @rcContent )
    if (rcClip.w <= 0) orelse (rcClip.h <= 0) then exit sub

    '' The absolute origin, taken once. Text goes to the backend in surface coordinates, and
    '' this is the only conversion in the file.
    dim as PsRect rcZero = PsRc(0, 0, 0, 0)
    dim as PsRect rcOrg = q->Abs_( @rcZero )

    dim as long nTop = this.nScroll
    dim as long nBot = this.nScroll + this.bounds.h

    g_rb.PushClip( @rcClip )

    '' ---- decorations first, in the order they were emitted -----------------------------
    '' A table's header band must land under its text, and the code slab under its lines.
    for d as long = 0 to this.pPage->nDeco - 1
        with this.pPage->deco(d)
            if (.y + .h < nTop) orelse (.y > nBot) then continue for
            dim as PsRect rc = PsRc(.x, .y - this.nScroll, .w, .h)
            select case .nKind
                case MDD_CODEBG
                    q->SetBackColor( this.clrCodeBg )
                    q->PaintRect( @rc )
                case MDD_INLCODEBG
                    q->SetBackColor( this.clrInlCodeBg )
                    q->PaintRect( @rc )
                case MDD_RULE, MDD_TABLELINE
                    q->SetBackColor( iif(.nKind = MDD_RULE, this.clrRule, this.clrTableLine) )
                    q->PaintRect( @rc )
                case MDD_QUOTEBAR
                    q->SetBackColor( this.clrQuoteBar )
                    q->PaintRect( @rc )
                case MDD_TABLEHEADBG
                    q->SetBackColor( this.clrTableHead )
                    q->PaintRect( @rc )
                case MDD_IMAGE
                    dim as any ptr pImg = MdLayoutImagePtr( .nImage )
                    if pImg <> 0 then
                        dim as long nIw, nIh
                        MdLayoutImageSize( .nImage, nIw, nIh )
                        '' STRETCH into a rect whose aspect the layout already matched to the
                        '' source -- ASPECT here would letterbox inside an exact fit.
                        q->PaintImage( pImg, nIw, nIh, @rc, PSIMGFIT_STRETCH )
                    else
                        q->SetPenColor( this.clrRule )
                        q->PaintBorderRect( @rc, 1 )
                    end if
            end select
        end with
    next

    '' ---- text --------------------------------------------------------------------------
    for l as long = 0 to this.pPage->nLine - 1
        with this.pPage->line(l)
            if (.y + .h < nTop) orelse (.y > nBot) then continue for
            dim as long nBase = rcOrg.y + (.y - this.nScroll) + .nBaseline
            for r as long = .nFirstRun to .nFirstRun + .nRunCount - 1
                dim as long nClr
                select case this.pPage->run(r).nKind
                    case MDRK_LINK      : nClr = this.clrLink
                    case MDRK_CODE      : nClr = this.clrCodeFore
                    case MDRK_MARKER    : nClr = this.clrDim
                    case MDRK_TABLEHEAD : nClr = this.clrFore
                    case MDRK_FENCE
                        dim as long t = this.pPage->run(r).nTok
                        if (t < 0) orelse (t > 6) then t = MDT_TEXT
                        nClr = this.clrTok(t)
                    case else           : nClr = this.clrFore
                end select
                g_rb.DrawText_( MdFontPtr(this.pPage->run(r).nFont), _
                                rcOrg.x + this.pPage->run(r).x, nBase, _
                                this.pPage->run(r).sText, nClr )
                '' The underline under a hovered link. Drawn rather than carried in the font,
                '' because there is no underlined face in the set and adding four more
                '' engines to underline a word would be absurd.
                if (r = this.nHotRun) andalso (this.pPage->run(r).nKind = MDRK_LINK) then
                    dim as PsRect rcU = PsRc( this.pPage->run(r).x, _
                                              (.y - this.nScroll) + .nBaseline + this.ScaleY(2), _
                                              this.pPage->run(r).w, this.ScaleY(1) )
                    q->SetBackColor( this.clrLink )
                    q->PaintRect( @rcU )
                end if
            next
        end with
    next

    '' ---- the Copy button, LAST -----------------------------------------------------------
    '' After the text, because it sits over the slab's top-right corner and a long first line
    '' would otherwise be drawn on top of it. Only ever for the slab under the pointer: a
    '' button on every fence at once is clutter on a page that is mostly fences.
    if (this.nHotCode >= 0) andalso (this.nHotCode < this.pPage->nDeco) then
        dim as PsRect rcC = this.CopyRect( this.nHotCode )
        if (rcC.y + rcC.h >= nTop) andalso (rcC.y <= nBot) then
            rcC.y -= this.nScroll
            q->SetBackColor( iif(this.bOnCopy, this.clrRule, this.clrCodeBg) )
            q->PaintRoundRect( @rcC, this.ScaleY(8) )
            q->SetPenColor( this.clrRule )
            q->PaintRoundOutline( @rcC, this.ScaleY(8), 1 )
            dim as DWSTRING sCap
            if this.nCopiedAt = this.nHotCode then
                sCap = PsText("Copied")
            else
                sCap = PsText("Copy")
            end if
            q->SetForeColor( iif(this.bOnCopy, this.clrFore, this.clrDim) )
            q->PaintText( sCap, @rcC, PSTF_CENTER or PSTF_VCENTER )
        end if
    end if

    g_rb.PopClip()
end sub


function MarkdownView.OnEvent( byval ev as PsEvent ptr ) as boolean
    if ev = 0 then return false
    if this.pPage = 0 then return false

    select case ev->kind

    case PSEV_MOUSE_WHEEL
        '' HandleWheel is the scrollbar's own accumulator, and it exists because a precision
        '' touchpad reports fractions of a notch. Rounding here instead would make trackpad
        '' scrolling stutter, which is the bug PsEvent.bi warns about.
        if (this.pScroll <> 0) andalso this.pScroll->bVisible then
            if this.pScroll->HandleWheel( ev->mouse.wheelY ) then
                this.ScrollTo( this.pScroll->GetPos() )
                return true
            end if
        end if
        this.ScrollBy( clng(-ev->mouse.wheelY * this.ScaleY(MDV_WHEEL)) )
        return true

    case PSEV_MOUSE_MOVE
        '' ev->mouse IS ALREADY LOCAL. PsDispatch gives every delivery its own copy of the
        '' event with the coordinates translated into the receiving widget's space
        '' (PsDispatch.inc:26-30), so calling FromSurface here translates a SECOND time and
        '' every hit test lands one widget-origin away from the pointer. That is what it did
        '' until it was driven by hand: link hover, link clicks and the copy button were all
        '' silently dead, and no headless assertion could have seen it because none of them
        '' goes through dispatch.
        dim as PsPoint pt
        pt.x = ev->mouse.x
        pt.y = ev->mouse.y
        '' The code slab and its button, tracked on the same move that tracks links.
        scope
            dim as long nCode = this.CodeAt( pt.x, pt.y + this.nScroll )
            dim as boolean bOn = false
            if nCode >= 0 then
                dim as PsRect rcC = this.CopyRect( nCode )
                dim as long nCy = pt.y + this.nScroll
                bOn = (pt.x >= rcC.x) andalso (pt.x < rcC.x + rcC.w) andalso _
                      (nCy >= rcC.y) andalso (nCy < rcC.y + rcC.h)
            end if
            if (nCode <> this.nHotCode) orelse (bOn <> this.bOnCopy) then
                '' Leaving a slab clears "Copied" -- the acknowledgement belongs to the moment
                '' of the click, and a button that still says Copied ten minutes later is
                '' claiming something about the clipboard it cannot know.
                if nCode <> this.nHotCode then this.nCopiedAt = -1
                this.nHotCode = nCode
                this.bOnCopy  = bOn
                this.Invalidate()
            end if
        end scope

        dim as long r = MdPageHitRun( *this.pPage, pt.x, pt.y + this.nScroll )
        if r >= 0 then
            if this.pPage->run(r).nKind <> MDRK_LINK then r = -1
        end if
        if this.bOnCopy then this.cursor = PSCUR_HAND
        if r <> this.nHotRun then
            this.nHotRun = r
            '' PSCUR_HAND, and the surface syncs it after dispatch -- the widget only states
            '' what it wants.
            if r >= 0 then this.cursor = PSCUR_HAND else this.cursor = PSCUR_DEFAULT
            this.Invalidate()
        end if
        return false

    case PSEV_MOUSE_LEAVE
        if (this.nHotRun <> -1) orelse (this.nHotCode <> -1) then
            this.nHotRun  = -1
            this.nHotCode = -1
            this.bOnCopy  = false
            this.nCopiedAt = -1
            this.cursor = PSCUR_DEFAULT
            this.Invalidate()
        end if
        return false

    case PSEV_MOUSE_DOWN
        if ev->mouse.button <> PSBTN_LEFT then return false
        '' ev->mouse IS ALREADY LOCAL. PsDispatch gives every delivery its own copy of the
        '' event with the coordinates translated into the receiving widget's space
        '' (PsDispatch.inc:26-30), so calling FromSurface here translates a SECOND time and
        '' every hit test lands one widget-origin away from the pointer. That is what it did
        '' until it was driven by hand: link hover, link clicks and the copy button were all
        '' silently dead, and no headless assertion could have seen it because none of them
        '' goes through dispatch.
        dim as PsPoint pt
        pt.x = ev->mouse.x
        pt.y = ev->mouse.y
        if this.pSurface <> 0 then cptr(PsSurface ptr, this.pSurface)->SetFocus( @this )

        '' The Copy button, BEFORE link hit-testing: it sits over the slab and a code line
        '' underneath it must not win the click.
        if this.bOnCopy andalso (this.nHotCode >= 0) then
            dim as long b = this.pPage->deco(this.nHotCode).nBlock
            if (this.pDoc <> 0) andalso (b >= 0) andalso (b < this.pDoc->nBlk) then
                dim as DWSTRING sCode
                sCode.Utf8 = this.pDoc->blk(b).sText
                g_plat.clip.SetText( sCode )
                this.nCopiedAt = this.nHotCode
                this.Invalidate()
            end if
            return true
        end if

        dim as long r = MdPageHitRun( *this.pPage, pt.x, pt.y + this.nScroll )
        if r < 0 then return true
        if this.pPage->run(r).nKind <> MDRK_LINK then return true
        dim as long nInl = this.pPage->run(r).nInl
        if (nInl < 0) orelse (nInl >= this.pDoc->nInl) then return true
        if this.pfnLink <> 0 then
            this.pfnLink( @this, this.pDoc->inl(nInl).sHref, this.pLinkData )
        end if
        return true

    case PSEV_KEY_DOWN
        '' Consuming a PSEV_KEY_DOWN means no PSEV_TEXT_INPUT is delivered, so there is no
        '' "swallow the WM_CHAR or it beeps" obligation here -- that rule is Win32's and it
        '' does not exist in this toolkit.
        select case ev->key.key
            case PSKEY_DOWN     : this.ScrollBy(  this.ScaleY(MDV_KEYSTEP) ) : return true
            case PSKEY_UP       : this.ScrollBy( -this.ScaleY(MDV_KEYSTEP) ) : return true
            case PSKEY_PAGEDOWN : this.ScrollBy(  this.bounds.h - this.ScaleY(24) ) : return true
            case PSKEY_PAGEUP   : this.ScrollBy( -(this.bounds.h - this.ScaleY(24)) ) : return true
            case PSKEY_HOME     : this.ScrollTo( 0 ) : return true
            case PSKEY_END      : this.ScrollTo( this.ContentHeight() ) : return true
        end select
        return false
    end select

    return false
end function
