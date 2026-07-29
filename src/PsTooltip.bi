'    PsTooltip - reusable owner-drawn tooltip control
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT ANY WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#pragma once


' ========================================================================================
' PsTooltip -- an owner-drawn replacement for the comctl32 tooltip.
'
' A WS_POPUP window that appears near the cursor after a dwell, shows text (optionally
' wrapped, optionally under a title and an icon glyph), fades in, and goes away again. It
' takes PsPopupMenu's non-activating window contract and adds a layered window for the fade.
'
' WHAT MAKES IT DIFFERENT FROM ITS SIBLINGS
'
'   IT IS NOT PLACED BY ITS HOST. Every other control in this family is a rectangle the host
'   sizes and positions; this one decides its own size from its content and its own position
'   from the cursor. There is no SetWindowPos contract, and GetIdealSize is informational.
'
'   IT ATTACHES TO ANOTHER CONTROL AND WATCHES IT. PsTooltip_Create( hCtrl ) creates the tip
'   AND attaches it, so adopting it is one call at the same point in a control's Create where
'   the comctl32 tooltip used to be built:
'
'       pBtn->hToolTip = PsTooltip_Create( hCtrl )        ' was PsButton_AddTooltip( hCtrl )
'
'   IT DOES NOT SUBCLASS THE ATTACHED CONTROL. comctl32 uses TTF_SUBCLASS, and Learnings.md
'   records that extra subclass as a contributing cause of the unreliable WM_MOUSELEAVE that
'   forces every hot-tracking sibling to carry a 100 ms poll timer. This control watches the
'   cursor instead, so it adds nothing to that pressure.
'
'   THERE IS NO PUMP OBLIGATION. No FilterMessage, nothing to add to a message loop -- the
'   dwell, the click dismissal and the fade all run off one shared timer. PsButton, PsCheckBox
'   and PsProgressBar are the only other controls in the family that can say this, and it is
'   the property that makes PsTooltip adoptable without touching a host's loop.
'
' THE SHARED CLOCK
'
'   Every attached instance is driven by ONE process-wide 100 ms timer on a hidden
'   message-only window, created with the first attach and destroyed with the last. It cannot
'   be armed by a mouse message the way a sibling's hot-track timer is (this control never
'   sees the attached window's messages), so it has to run unprompted -- which is exactly why
'   there is one of it rather than one per instance. A form with twenty tips costs one timer.
'
'   The tick rate rises while a fade is running and drops back afterwards, so a 120 ms fade
'   is smooth without polling the cursor 60 times a second when nothing is happening.
'
' SINGLE-THREADED, like every control in this family. UI thread only.
'
' HOST OBLIGATIONS -- there are only two, and both are PsBufferPaint's, not this control's:
'   1. Bracket the message loop with AfxGdipInit / AfxGdipShutdown.
'   2. Never name an identifier `ok`. GDI+'s Status enum defines Ok = 0 in namespace AfxNova,
'      and every host says `using AfxNova`. The family convention is bOK.
' ========================================================================================


' Guards a stale HWND whose slot 0 now belongs to some other window. The clock tick walks a
' registry asynchronously to whatever the host is doing, so "is this still one of ours?" is a
' question that genuinely gets asked here -- the same reason PsOptionButton carries one.
#define PSTOOLTIP_SIGNATURE   &h54495054    ' 'TIPT'

' --- geometry defaults, in UNSCALED pixels. All are DPI-scaled once, at Create. -----------
#define PSTOOLTIP_DEFAULT_PADX         10
#define PSTOOLTIP_DEFAULT_PADY          7
#define PSTOOLTIP_DEFAULT_CURVATURE     6    ' corner ellipse DIAMETER, GDI's vocabulary
#define PSTOOLTIP_DEFAULT_STEMW        14
#define PSTOOLTIP_DEFAULT_STEMH         7
#define PSTOOLTIP_DEFAULT_GLYPHCELL    20
#define PSTOOLTIP_DEFAULT_GLYPHGAP      8
#define PSTOOLTIP_DEFAULT_TITLEGAP      4
#define PSTOOLTIP_DEFAULT_MOVETOL       3    ' cursor jitter tolerated while dwelling

' --- timing defaults, in MILLISECONDS -----------------------------------------------------
' Derived at Create from GetDoubleClickTime(), which is comctl32's own rule for TTDT_INITIAL /
' TTDT_AUTOPOP / TTDT_RESHOW. A tip that appears on a different beat from every other tip on
' the machine reads as broken, so the system setting is the right source.
#define PSTOOLTIP_DEFAULT_FADEMS      120

' The fade wants a finer clock than the dwell does. Both are shared across every instance.
#define PSTOOLTIP_CLOCK_IDLE_MS       100
#define PSTOOLTIP_CLOCK_FADE_MS        15


' Frame style. Orthogonal to everything else: a balloon is the same tip with a stem.
enum
    TIP_STYLE_RECT    = 0        ' rounded rectangle, no stem (the default)
    TIP_STYLE_BALLOON = 1        ' rounded rectangle with a triangular stem at the target
end enum

' Placement, relative to the anchor handed to the show call.
'
' THE STEM IS ALWAYS VERTICAL, and that falls out of this list rather than being a separate
' decision: every mode here puts the tip above or below its anchor, never beside it, so the
' stem only ever leaves the top or the bottom edge. A left/right stem would need a horizontal
' placement mode, and there is deliberately none -- a tooltip beside its target is not a shape
' anyone asked for, and supporting it would make the window's height depend on a side that is
' not known until after placement has run.
enum
    TIP_ALIGN_CURSOR = 0         ' below-right of the cursor, cleared by the cursor's height
    TIP_ALIGN_BELOW  = 1         ' below the anchor rect, flipping above when there is no room
    TIP_ALIGN_ABOVE  = 2         ' above the anchor rect, flipping below when there is no room
end enum

' Named parts, for the render probes.
enum
    TIP_PART_CLIENT = 0          ' the whole window, stem band included
    TIP_PART_BODY   = 1          ' the rounded frame, stem band excluded
    TIP_PART_GLYPH  = 2
    TIP_PART_TITLE  = 3
    TIP_PART_TEXT   = 4
    TIP_PART_STEM   = 5          ' the stem band only; empty in TIP_STYLE_RECT
end enum


' Five colours, which is as many as a tooltip has surfaces.
'
' THESE ARE INHERITED FROM THE FAMILY'S DARK PALETTE, NOT MATCHED TO A REFERENCE. There is no
' screenshot for this control. They have been looked at and accepted, which is a weaker claim
' than "matched" and worth keeping distinct -- PsButton's and PsCheckBox's defaults carry the
' same caveat.
type PSTOOLTIP_COLORS
    BackColor    as COLORREF = BGR( 45, 50, 58)   ' the body fill
    BorderColor  as COLORREF = BGR( 78, 84, 94)   ' the frame, and the stem's outline
    ForeColor    as COLORREF = BGR(212,217,226)   ' the message
    TitleColor   as COLORREF = BGR(255,255,255)   ' the title line
    GlyphColor   as COLORREF = BGR( 86,156,214)   ' the icon glyph
end type


' Everything the painter needs. Every rect is precomputed by LayoutTooltip -- never re-derive
' one from another, and in particular never re-apply the padding to rcBody to get rcText,
' because the title row and the glyph cell can both change underneath you.
type PSTOOLTIP_PAINTINFO
    hTooltip     as HWND               ' the tip window
    hAttached    as HWND               ' the control it is serving, so a callback can query it
    b            as PsBufferPaint ptr  ' the tip's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed ---
    rcClient     as RECT               ' the whole window, INCLUDING the stem band
    ' The rounded frame. In TIP_STYLE_RECT this is rcClient; in TIP_STYLE_BALLOON it is
    ' rcClient with nStemH taken off whichever edge the stem leaves from. Paint the body
    ' against THIS, never against rcClient, or the frame swallows the stem band.
    rcBody       as RECT
    rcGlyph      as RECT               ' the declared glyph cell. EMPTY when there is no glyph
    rcTitle      as RECT               ' the title SPAN. EMPTY when there is no title
    rcText       as RECT               ' the message span -- see the caution below
    ' --- The stem. Client coordinates, ready to hand to PaintPolygon. ---
    ptStem(0 to 2) as POINT
    bHasStem     as boolean            ' false in TIP_STYLE_RECT, and when there is no room
    bStemOnTop   as boolean            ' true = the tip is BELOW its target and points up
    ' --- What to draw ---
    wszText      as DWSTRING           ' the RESOLVED message, not the authored field
    wszTitle     as DWSTRING
    wszGlyph     as DWSTRING
    nStyle       as long               ' TIP_STYLE_*
    nCurvature   as long               ' corner ellipse DIAMETER
    nBorderWidth as long               ' RAW pixels -- PaintRoundOutline scales the pen itself
    nToolIndex   as long               ' -1 for a whole-control tip
    nTextFlags   as DWORD              ' the DrawText flags the message was MEASURED with
end type

type PSTOOLTIP_MESSAGEINFO
    hTooltip  as HWND
    hAttached as HWND
    uMsg      as UINT
    wParam    as WPARAM
    lParam    as LPARAM
end type


' Draw the whole tip, INSTEAD OF the built-in painter. Paint through p->b -- do not touch the
' screen DC.
'
' Nothing has been drawn when you are called: unlike most siblings, this control does NOT
' pre-fill the background for you, because the fill and the frame are one rounded shape and
' filling the client rect first would put square corners behind it.
'
' THREE CONTRACTS WORTH HONOURING:
'   - Draw the message with p->nTextFlags and the SAME font you handed to SetFont. The window
'     was sized by measuring with both; a different font or different flags means the size lies
'     and the text clips. This is the exact contract PsOptionButton's own demo broke -- it
'     measured in the regular font and painted in bold.
'   - Paint the body against rcBody, NOT rcClient. rcClient includes the stem band.
'   - DO NOT reach for PaintBorderRect or PaintRoundBorderRect to draw the frame. They FILL
'     unconditionally before stroking, so used as an outline they erase everything beneath --
'     a mistake this family has now made FIVE separate times (PsToggle, PsComboBox,
'     PsNumericUpDown, PsDatePicker where it rendered a whole popup calendar as one flat blue
'     rectangle, and PsOptionButton where it survived the inherited tone threshold).
'     PaintRoundOutline is the one that strokes without filling.
'     PsTooltip_CountRenderedTones exists so you can ASSERT you have not done it.
type TIP_PaintCallbackSub as sub( byval p as PSTOOLTIP_PAINTINFO ptr )

' Observe the tip window's messages. Return TRUE to suppress the control's default handling.
'
' BE AWARE HOW LITTLE ARRIVES HERE, so you do not go looking for something that cannot come.
' The tip is WS_EX_NOACTIVATE, answers WM_MOUSEACTIVATE with MA_NOACTIVATE and WM_NCHITTEST
' with HTTRANSPARENT, and is never focused -- so it receives no mouse messages at all (they
' pass through to whatever is underneath), no keyboard, and no activation. In practice this
' sees WM_PAINT, WM_ERASEBKGND, WM_WINDOWPOSCHANGED and the destroy pair.
'
' The user-visible events worth hooking are on TIP_ShowCallbackFunc below, not here.
'
' CAUTION: the result is IGNORED for WM_DESTROY and WM_NCDESTROY, which the control needs for
' its own teardown -- a callback that suppressed one would leak the state block and leave a
' dead entry in the shared registry.
type TIP_MessageCallbackFunc as function( byval m as PSTOOLTIP_MESSAGEINFO ptr ) as boolean

' Supply the tip text on demand -- called only when a tip is ABOUT TO SHOW, never on a timer
' tick that decides not to. Consulted only when the tip has no authored text of its own.
' Return "" for no tooltip, which is also how a host suppresses one entirely -- and doing so
' suppresses the rest of that hover, so you are consulted once per hover rather than on every
' 100 ms tick for as long as the cursor rests.
'
' idx is the value last handed to PsTooltip_SetToolIndex, or -1 for a whole-control tip. It is
' a LONG here, unifying the integer/long split across the six per-item typedefs in the
' controls this replaces (PsListBox and PsColumnHeader and PsStatusBar and PsTabBar used
' integer; PsSelectBar and PsIconPanel used long).
'
' THERE IS NO CAPTION FALLBACK, because a tooltip has no caption of its own to fall back to.
' Note that this is a real behaviour change from PsListBox, PsColumnHeader, PsStatusBar and
' PsTabBar, which fall back to the ITEM's own text and therefore sprout a tip on every row
' repeating what is already under the cursor -- tiko carries three separate callbacks that
' exist only to return "" and turn that off (Learnings.md). A host that wants the old
' behaviour returns the item text from here.
type TIP_TooltipCallbackFunc as function( byval hTooltip as HWND, _
                                          byval hAttached as HWND, _
                                          byval idx as long ) as DWSTRING

' The tip is about to appear, or has just gone away.
'
' THE SHOWING EDGE IS A VETO: return FALSE and the tip does not appear. It runs AFTER the text
' has been resolved and the window sized, but BEFORE anything is shown, so a host can suppress
' a tip on state the control cannot see. Return TRUE to allow it.
'
' A VETO COVERS THE WHOLE HOVER, NOT ONE ATTEMPT. The dwell conditions are still true on the
' next 100 ms tick, so without this the attempt would simply repeat and the tip would appear a
' tenth of a second later anyway -- the veto honoured, and completely ineffective. Refusing a
' show therefore suppresses the tip until the cursor LEAVES the attached control, which is what
' "no tip for this hover" has to mean. It also means you are consulted once per hover rather
' than ten times a second.
'
' Changing the tool index clears that suppression: a new item is a new offer, so vetoing one
' cell does not silence the rest of the control.
'
' The hiding edge's result is ignored -- a tip that is already on screen cannot be un-hidden.
'
' Fires for PROGRAMMATIC shows and hides too, because it reports a window-state transition
' rather than a user action (PsComboBox's DropDownCallback precedent). Both edges are paired:
' a vetoed show fires no hide.
type TIP_ShowCallbackFunc as function( byval hTooltip as HWND, _
                                       byval hAttached as HWND, _
                                       byval idx as long, _
                                       byval bShowing as boolean ) as boolean


type PSTOOLTIP
    ' MUST BE FIRST and must be checked before any other field is read. See
    ' PSTOOLTIP_SIGNATURE above.
    dwSignature      as DWORD = PSTOOLTIP_SIGNATURE
    hWin             as HWND        ' the tip popup
    hAttached        as HWND        ' the control being watched. 0 = detached, never shows
    idc_Tooltip      as long

    ' --- text -----------------------------------------------------------------------------
    ' The host's AUTHORED message. "" means ask TooltipCallback, then show nothing.
    wszText          as DWSTRING
    wszTitle         as DWSTRING
    wszGlyph         as DWSTRING
    ' The RESOLVED message actually on screen. It must NOT be the same field as wszText: that
    ' one is the host's authored text, and a callback's answer written there would silently
    ' become stored state. Four of the eleven controls this replaces get that right and six
    ' reuse the authored field -- this is the right side of that split.
    wszTipBuf        as DWSTRING
    nToolIndex       as long = -1   ' per-item selector; -1 = whole control

    ' --- fonts. BORROWED, never created and never deleted here. -----------------------------
    ' Not one of these may be called hFont: FreeBASIC is case-insensitive and a field named
    ' hFont shadows the HFONT type for the rest of the type body, so the SECOND such field
    ' fails to compile with an error naming the type (Learnings.md; PsDatePicker hit it with
    ' exactly three font fields).
    hTextFont        as HFONT
    hTitleFont       as HFONT
    hGlyphFont       as HFONT

    ' --- appearance -------------------------------------------------------------------------
    colors           as PSTOOLTIP_COLORS
    nStyle           as long        ' TIP_STYLE_*
    ' Caps the TEXT WRAP, not the window. 0 = no wrap at all (comctl32's default), in which
    ' case only embedded newlines break a line. A long title can still make the window wider
    ' than this: the width is a max() over the bands, not a clamp on the result.
    nMaxWidth        as long
    nPadX            as long
    nPadY            as long
    nCurvature       as long
    nBorderWidth     as long
    nStemW           as long
    nStemH           as long
    nGlyphCell       as long
    nGlyphGap        as long
    nTitleGap        as long

    ' --- timing, all in milliseconds ----------------------------------------------------
    nInitialDelay    as long
    nAutoPopDelay    as long        ' 0 = never expires
    nReshowDelay     as long
    nFadeTime        as long        ' 0 = no fade, show and hide instantly
    nMoveTolerance   as long

    ' --- layout, all DERIVED. Lazy: mutators set the flag, the next consume point rebuilds. --
    bLayoutDirty     as boolean = true
    nIdealW          as long
    nIdealH          as long        ' INCLUDES the stem band in TIP_STYLE_BALLOON
    rcBody           as RECT
    rcGlyph          as RECT
    rcTitle          as RECT
    rcText           as RECT
    nTextFlags       as DWORD       ' the flags the message was measured with

    ' --- runtime ---------------------------------------------------------------------------
    ptStem(0 to 2)   as POINT
    bHasStem         as boolean
    bStemOnTop       as boolean
    ptTarget         as POINT       ' SCREEN point the stem aims at
    ptLastCursor     as POINT
    dwRestSince      as DWORD       ' GetTickCount when the cursor came to rest. 0 = not resting
    dwShownAt        as DWORD
    dwHiddenAt       as DWORD
    dwFadeStart      as DWORD
    nFadePhase       as long        ' 0 = settled, 1 = fading in, 2 = fading out
    nAlpha           as long        ' 0..255, what was last pushed to the layered window
    bVisible         as boolean
    ' Set when a mouse button goes down over the attached control, cleared when the cursor
    ' leaves it. comctl32's behaviour: a click suppresses the tip until you leave and come
    ' back, rather than letting it reappear on top of what you just clicked.
    bSuppressed      as boolean

    ' --- callbacks, all optional -------------------------------------------------------------
    PaintCallback    as TIP_PaintCallbackSub
    MessageCallback  as TIP_MessageCallbackFunc
    TooltipCallback  as TIP_TooltipCallbackFunc
    ShowCallback     as TIP_ShowCallbackFunc

    declare sub LayoutTooltip()
    declare sub Refresh()
end type


' ========================================================================================
' PURE FUNCTIONS
'
' Everything that decides WHERE something goes, or HOW FAR ALONG a fade is, lives here rather
' than inline in the tick or the show path. That is what lets the placement law, the stem
' geometry, the fade ramp and the show decision be asserted over their whole ranges with no
' window, no timer and no message pump -- PsProgressBar's ComputeBarLength / ComputeMarqueeOffset
' / ComputeBlockCount and PsMessageBox's ComputeOrigin.
' ========================================================================================

' Place a tip of tipW x tipH against rcAnchor, clamped into rcWork.
'
' rcWork is a monitor WORK AREA, and rcAnchor is in the same (screen) space. For
' TIP_ALIGN_CURSOR, rcAnchor is the degenerate rect at the cursor hotspot and nCursorOffset is
' how far below it to clear -- GetSystemMetrics(SM_CYCURSOR) in the real call, passed in here
' so this stays pure.
'
' The vertical rule is PsPopupMenu's: when there is no room below, FLIP ABOVE the anchor
' rather than clamping up over it.
declare function PsTooltip_ComputeOrigin( _
            byval rcAnchor as RECT, _
            byval tipW as long, _
            byval tipH as long, _
            byval rcWork as RECT, _
            byval nAlign as long, _
            byval nCursorOffset as long _
            ) as POINT

' True when the tip at ptOrigin sits BELOW its anchor -- i.e. the stem leaves the top edge.
' Split out of ComputeOrigin so the caller does not have to re-derive the flip decision.
declare function PsTooltip_StemOnTop( _
            byval rcAnchor as RECT, _
            byval ptOrigin as POINT, _
            byval nAlign as long _
            ) as boolean

' The stem's three vertices, in the tip's CLIENT coordinates, ready for PaintPolygon.
'
' nTargetX is the target's x in client coordinates; the base is centred on it and then clamped
' so the stem cannot grow out of a rounded corner. Returns FALSE when the client is too narrow
' to seat a stem at all, in which case pts is left alone and the caller draws a plain frame.
declare function PsTooltip_ComputeStem( _
            byval cxClient as long, _
            byval cyClient as long, _
            byval bStemOnTop as boolean, _
            byval nStemW as long, _
            byval nStemH as long, _
            byval nTargetX as long, _
            byval nCurvature as long, _
            byval pts as POINT ptr _
            ) as boolean

' The fade ramp, 0..255. nFadeMs <= 0 collapses to an instant transition.
declare function PsTooltip_ComputeAlpha( _
            byval nElapsed as long, _
            byval nFadeMs as long, _
            byval bFadingIn as boolean _
            ) as long

' The whole truth table for whether a tip may appear. Every condition that can veto a show is
' here and nowhere else, so the table is assertable in one place.
declare function PsTooltip_ShouldShow( _
            byval bOver as boolean, _
            byval bResting as boolean, _
            byval bHasText as boolean, _
            byval bButtonDown as boolean, _
            byval bSuppressed as boolean _
            ) as boolean


' ========================================================================================
' PUBLIC API
' ========================================================================================

' --- lifecycle ---------------------------------------------------------------------------
' Create a tip AND attach it to hAttach in one call, so adopting this control is a one-line
' change at the point where a comctl32 tooltip used to be built. The popup's owner is derived
' as GetAncestor( hAttach, GA_ROOT ).
'
' The tip is owned by whoever created it and must be destroyed by them -- DestroyWindow in the
' attached control's WM_NCDESTROY, exactly where the comctl32 one was destroyed.
declare function PsTooltip_Create( byval hAttach as HWND, byval CtrlID as long = 0 ) as HWND
declare sub      PsTooltip_Attach( byval hTooltip as HWND, byval hAttach as HWND )
declare sub      PsTooltip_Detach( byval hTooltip as HWND )
declare function PsTooltip_GetAttached( byval hTooltip as HWND ) as HWND

' --- text ---------------------------------------------------------------------------------
declare sub      PsTooltip_SetText( byval hTooltip as HWND, byval wszText as DWSTRING )
declare function PsTooltip_GetText( byval hTooltip as HWND ) as DWSTRING
declare sub      PsTooltip_SetTitle( byval hTooltip as HWND, byval wszTitle as DWSTRING )
declare function PsTooltip_GetTitle( byval hTooltip as HWND ) as DWSTRING
' A single character from the glyph font handed to SetFonts. "" removes it.
declare sub      PsTooltip_SetGlyph( byval hTooltip as HWND, byval wszGlyph as DWSTRING )
declare function PsTooltip_GetGlyph( byval hTooltip as HWND ) as DWSTRING
' Which item the tip is currently describing. Call this from the attached control's hot-item
' tracking -- it is the same call site that sends TTM_POP today. Changing it while a tip is on
' screen HIDES it, so the next dwell re-resolves the text for the new item, and the short
' reshow delay applies rather than the full initial one.
declare sub      PsTooltip_SetToolIndex( byval hTooltip as HWND, byval idx as long )
declare function PsTooltip_GetToolIndex( byval hTooltip as HWND ) as long

' --- appearance -----------------------------------------------------------------------------
declare sub      PsTooltip_SetColors( byval hTooltip as HWND, byval pColors as PSTOOLTIP_COLORS ptr )
declare sub      PsTooltip_GetColors( byval hTooltip as HWND, byval pColors as PSTOOLTIP_COLORS ptr )
' All three fonts are BORROWED. The caller keeps ownership and must not delete one while the
' tip is alive. hTitleFont 0 falls back to hTextFont; hGlyphFont 0 falls back to hTextFont.
declare sub      PsTooltip_SetFonts( byval hTooltip as HWND, byval hText as HFONT, _
                                     byval hTitle as HFONT = 0, byval hGlyph as HFONT = 0 )
declare sub      PsTooltip_SetStyle( byval hTooltip as HWND, byval nStyle as long )
declare function PsTooltip_GetStyle( byval hTooltip as HWND ) as long
' 0 = no wrap. Caps the text wrap, not the window -- see nMaxWidth above.
declare sub      PsTooltip_SetMaxWidth( byval hTooltip as HWND, byval nMaxWidth as long )
declare function PsTooltip_GetMaxWidth( byval hTooltip as HWND ) as long
declare sub      PsTooltip_SetPadding( byval hTooltip as HWND, byval nX as long, byval nY as long )
declare sub      PsTooltip_SetCurvature( byval hTooltip as HWND, byval nCurvature as long )
declare sub      PsTooltip_SetBorderWidth( byval hTooltip as HWND, byval nBorderWidth as long )
declare sub      PsTooltip_SetStemSize( byval hTooltip as HWND, byval nW as long, byval nH as long )
declare sub      PsTooltip_SetGlyphCell( byval hTooltip as HWND, byval nCell as long, byval nGap as long )

' --- timing, all in milliseconds ----------------------------------------------------------
declare sub      PsTooltip_SetInitialDelay( byval hTooltip as HWND, byval nMs as long )
declare sub      PsTooltip_SetAutoPopDelay( byval hTooltip as HWND, byval nMs as long )
declare sub      PsTooltip_SetReshowDelay( byval hTooltip as HWND, byval nMs as long )
declare sub      PsTooltip_SetFadeTime( byval hTooltip as HWND, byval nMs as long )
declare sub      PsTooltip_SetMoveTolerance( byval hTooltip as HWND, byval nPixels as long )

' --- process-wide defaults ------------------------------------------------------------------
' Every tip a control creates for itself is created lazily and out of the host's sight, so a
' host that wants ONE look across a form cannot reach them one at a time. These set what
' PsTooltip_Create applies to each new tip. Call them once, at startup, BEFORE any tip exists;
' a tip already created is not retro-fitted (PsTooltip_ApplyDefaults is the door for that).
'
' Each is independently armed: a field never set keeps the value Create derives for it, which
' is why these are separate calls rather than one struct. That matters most for the delays --
' Create derives all three from GetDoubleClickTime(), and a zeroed struct would silently
' replace a machine-appropriate 500ms with 0.
'
' The fonts are BORROWED exactly as PsTooltip_SetFonts borrows them: the host keeps ownership
' and must outlive every tip. That is the one real hazard in this block, since the host is
' promising it to controls it never sees.
declare sub      PsTooltip_SetDefaultColors( byval pColors as PSTOOLTIP_COLORS ptr )
declare sub      PsTooltip_SetDefaultFonts( byval hText as HFONT, _
                                            byval hTitle as HFONT = 0, byval hGlyph as HFONT = 0 )
declare sub      PsTooltip_SetDefaultStyle( byval nStyle as long )
declare sub      PsTooltip_SetDefaultMaxWidth( byval nMaxWidth as long )
declare sub      PsTooltip_SetDefaultDelays( byval nInitialMs as long, _
                                             byval nAutoPopMs as long = -1, _
                                             byval nReshowMs as long = -1 )
' Clear every armed default. New tips fall back to Create's own derivation again.
declare sub      PsTooltip_ClearDefaults()
' Apply the armed defaults to one existing tip. Create calls this itself; a host calls it to
' re-theme a tip that already exists.
declare sub      PsTooltip_ApplyDefaults( byval hTooltip as HWND )

' --- the system's own tooltip font ------------------------------------------------------------
' What WINDOWS draws a tooltip in: SPI_GETNONCLIENTMETRICS's lfStatusFont, which is the
' status-bar/tooltip face. This control creates no fonts -- the family rule -- so this reads the
' setting and hands back the numbers; building the HFONT stays the host's job and so does owning
' it.
'
' It is here rather than in a host because the reason to want it is the same reason PsTooltip
' derives its delays from GetDoubleClickTime(): a tip drawn in a different face and size from
' every other tip on the machine reads as broken. A host that then wants its own font simply
' does not call this.
'
' Returns FALSE if the system refuses, or if what it reports is unusable (no face name, or no
' derivable point size). The struct is left untouched in that case, so a caller falls back to a
' face of its own rather than to an empty LOGFONT.
'
' USE pointSize, NOT pixelHeight, AND THE DIFFERENCE IS NOT COSMETIC.
' lfHeight comes back in PIXELS at the process's current DPI. Every font-building call that
' takes a size in POINTS -- AfxNova's CWindow.CreateFont among them -- DPI-scales what it is
' given, so handing it the pixel height asks for a font ~1.75x too large on a 175% display AND
' LOOKS CORRECT AT 100%, which is how that bug ships. Points are DPI-neutral, so the round trip
' pixels -> points -> CreateFont lands back on the size the system asked for. Measured on a 175%
' display: LOGPIXELSY 168, pixelHeight 21, pointSize 9.
'
' lfWeight is reported as well as isBold because FW_BOLD is a THRESHOLD: a system face at
' FW_SEMIBOLD would be flattened to "not bold" and rebuilt at FW_NORMAL if only the boolean
' survived. Build from lfWeight; read isBold when a yes/no is genuinely what you want.
type PSTOOLTIP_SYSTEMFONT
    wszFaceName  as DWSTRING
    pointSize    as long = 0      ' derived from pixelHeight against the screen's LOGPIXELSY
    pixelHeight  as long = 0      ' abs(lfHeight), exactly as the system reported it
    lfWeight     as long = 0      ' the raw LOGFONT weight -- build from THIS
    isBold       as boolean = false
    isItalic     as boolean = false
end type

declare function PsTooltip_GetSystemFont( byref info as PSTOOLTIP_SYSTEMFONT ) as boolean

' --- manual drive. These BYPASS the dwell entirely; the tick still auto-hides them. --------
' Show at the current cursor position.
declare function PsTooltip_Show( byval hTooltip as HWND ) as boolean
' Show anchored to a SCREEN rect. This is how a per-item tip gets pinned under its own row or
' cell rather than following the cursor.
declare function PsTooltip_ShowForRect( byval hTooltip as HWND, byval rcAnchor as RECT, _
                                        byval nAlign as long = TIP_ALIGN_BELOW ) as boolean
declare sub      PsTooltip_Hide( byval hTooltip as HWND )
declare function PsTooltip_IsVisible( byval hTooltip as HWND ) as boolean

' --- callbacks -------------------------------------------------------------------------------
declare sub      PsTooltip_SetPaintCallback( byval hTooltip as HWND, byval userfunc as TIP_PaintCallbackSub )
declare sub      PsTooltip_SetMessageCallback( byval hTooltip as HWND, byval userfunc as TIP_MessageCallbackFunc )
declare sub      PsTooltip_SetTooltipCallback( byval hTooltip as HWND, byval userfunc as TIP_TooltipCallbackFunc )
declare sub      PsTooltip_SetShowCallback( byval hTooltip as HWND, byval userfunc as TIP_ShowCallbackFunc )

' --- introspection ---------------------------------------------------------------------------
' The size the tip WOULD be for its current text. Informational: nothing sizes this control
' from outside. Forces the pending layout, so it is always current.
declare sub      PsTooltip_GetIdealSize( byval hTooltip as HWND, byref nW as long, byref nH as long )
declare function PsTooltip_GetPartRect( byval hTooltip as HWND, byval nPart as long ) as RECT

' --- render probes. PUBLIC ON PURPOSE. ---------------------------------------------------------
' A host that replaces the painter should be able to assert that its render is not flooding the
' control and that a state change actually reaches the pixels. Both drive the REAL paint path
' -- PsTooltip_Render, the same function WM_PAINT calls -- into an offscreen buffer, which is
' the split that matters: PsDatePicker shipped a probe that passed at 59 tones while its popup
' rendered as one flat rectangle, because it drove a per-cell callback instead of the painter.
'
' DO NOT INHERIT A TONE THRESHOLD FROM A SIBLING. PsButton's and PsCheckBox's `> 1` floor passed
' on a completely destroyed render in both PsMessageBox and PsOptionButton, because the number
' depends entirely on what else happens to be inside the part rect. Measure a healthy value and
' a flooded one for YOUR painter and calibrate between them.
' Drive exactly ONE tick of the dwell/hide/fade state machine with injected inputs, instead of
' waiting for the shared timer and reading the real cursor.
'
' WHY THIS IS PUBLIC. Everything that decides WHEN a tip appears lives in the tick: the dwell,
' the reshow-versus-initial delay choice, the movement tolerance, click suppression, auto-pop,
' the fade, and the rule that a refused show suppresses the rest of the hover. None of it was
' reachable by an assertion -- the tick needs a live message pump and a real mouse -- and a
' defect duly shipped there (a veto that held for exactly one tick and then retried). This is
' the seam that closes that gap, and it is the same move as PsMessageBox_LayoutForTest.
'
' It is a TEST seam, not a control channel: calling it from application code fights the shared
' timer, which is still running and will tick with the real cursor a moment later.
'
' bOver is injected rather than derived. The real clock works it out from WindowFromPoint and
' the attached window's rect; a test cannot fake either, so it states the answer.
'
' PASS REAL TIMESTAMPS, offset from one GetTickCount() base. Hide bookkeeping stamps itself
' from the real clock, so a synthetic dwNow near zero would compare against a real tick count
' and the unsigned arithmetic would wrap.
declare sub      PsTooltip_TickForTest( byval hTooltip as HWND, byval ptCursor as POINT, _
                                        byval bOver as boolean, byval bButtonDown as boolean, _
                                        byval dwNow as DWORD )

declare function PsTooltip_CountRenderedTones( byval hTooltip as HWND, byval nPart as long ) as long
declare function PsTooltip_HashRenderedPart( byval hTooltip as HWND, byval nPart as long ) as ulong
