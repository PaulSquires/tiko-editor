''
''  PsMessageBox.bi  --  owner-drawn modal message box (caption + icon + wrapped text + buttons)
''

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0 in namespace
' AfxNova, and every host in this family says "using AfxNova" -- so ANY identifier named "ok"
' becomes a duplicate definition. The family convention is bOK.
#include once "PsBufferPaint.bi"
' The footer buttons ARE PsButtons. This control is the first in the family to be a genuine
' container of siblings, so unlike every other .bi here this one pulls in another control.
#include once "PsButton.bi"

' Polling timer that guarantees the close button's hot-tracking is cleared when the mouse
' leaves. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits. Timer IDs are
' per-window, so every instance can share this id.
#define IDT_CMESSAGEBOX_HOTTRACK   &hCB91
#define PSMESSAGEBOX_HOTTRACK_MS    100

' The most buttons the footer will hold. Not an arbitrary cap: a message box that needs a
' fourth choice wants a real dialog, and the layout arithmetic below is written against a
' fixed-size array precisely so there is no index fix-up code to get wrong.
#define PSMESSAGEBOX_MAXBUTTONS      3

' Control ids handed to the footer buttons. The HOST's return ids are set separately with
' PsButton_SetID, so these never enter anyone's command stream -- they exist only so
' GetDlgItem / GetNextDlgTabItem have something to report.
#define IDC_CMESSAGEBOX_BUTTON_BASE 8100

' Default geometry, in unscaled pixels. Everything except the THICKNESS values is DPI-scaled
' once at Create; every setter afterwards takes raw pixels and the caller scales (family rule).
#define PSMESSAGEBOX_DEFAULT_CAPTIONHEIGHT  34
#define PSMESSAGEBOX_DEFAULT_CAPTIONPADX    14
#define PSMESSAGEBOX_DEFAULT_CLOSEWIDTH     44
#define PSMESSAGEBOX_DEFAULT_BODYPADX       20
#define PSMESSAGEBOX_DEFAULT_BODYPADY       20
#define PSMESSAGEBOX_DEFAULT_FOOTERPADX     20
#define PSMESSAGEBOX_DEFAULT_FOOTERPADY     14
#define PSMESSAGEBOX_DEFAULT_ICONWIDTH      32
#define PSMESSAGEBOX_DEFAULT_ICONHEIGHT     32
#define PSMESSAGEBOX_DEFAULT_ICONGAP        16
#define PSMESSAGEBOX_DEFAULT_BUTTONGAP       8
#define PSMESSAGEBOX_DEFAULT_BUTTONMINWIDTH 88
#define PSMESSAGEBOX_DEFAULT_MAXWIDTH      420
#define PSMESSAGEBOX_DEFAULT_MINWIDTH      300
#define PSMESSAGEBOX_DEFAULT_BORDERTHICK     1     ' not DPI-scaled -- a hairline stays a hairline
#define PSMESSAGEBOX_DEFAULT_DIVIDERTHICK    1     ' likewise

' The glyphs the icon kinds resolve to (Segoe Fluent Icons / Segoe MDL2 Assets codepoints), and
' the close button's. They are #defines rather than literals buried in the resolver so a host
' that ships a different icon font can see exactly what to remap.
#define PSMESSAGEBOX_GLYPH_INFO      &hE946      ' Info
#define PSMESSAGEBOX_GLYPH_WARNING   &hE7BA      ' Warning  (the triangle in the reference shot)
#define PSMESSAGEBOX_GLYPH_ERROR     &hEA39      ' ErrorBadge
#define PSMESSAGEBOX_GLYPH_QUESTION  &hE9CE      ' StatusCircleQuestionMark
#define PSMESSAGEBOX_GLYPH_CLOSE     &hE8BB      ' ChromeClose


' The icon on the left. Each kind resolves to a glyph AND to its own colour -- see
' PsMessageBox_ResolveGlyph / PsMessageBox_ResolveIconColor, which are PURE FUNCTIONS so the whole
' mapping can be asserted rather than only observed inside a paint.
'
' MBX_ICON_NONE is not "no artwork available", it is a real layout state: the icon cell and its
' gap are not charged at all, so the text starts at the body padding and the box is narrower.
enum
    MBX_ICON_NONE = 0
    MBX_ICON_INFO
    MBX_ICON_WARNING
    MBX_ICON_ERROR
    MBX_ICON_QUESTION
end enum

' Ready-made button sets for PsMessageBox_AddPreset and PsMessageBox_Show. Every one of them is
' just a sequence of PsMessageBox_AddButton calls plus a SetDefaultButton and a SetCancelID --
' there is nothing a preset can express that a host cannot write out by hand, which is the point
' of having free-form AddButton underneath.
'
' The captions are ENGLISH LITERALS. A localised host uses AddButton directly; presets are a
' convenience, not a localisation layer.
enum
    MBX_BTN_OK = 0                  ' [OK]                                default OK,   cancel OK
    MBX_BTN_OKCANCEL                ' [OK] [Cancel]                       default OK,   cancel Cancel
    MBX_BTN_YESNO                   ' [Yes] [No]                          default Yes,  cancel No
    MBX_BTN_YESNOCANCEL             ' [Yes] [No] [Cancel]                 default Yes,  cancel Cancel
    MBX_BTN_RETRYCANCEL             ' [Retry] [Cancel]                    default Retry,cancel Cancel
    MBX_BTN_SAVE_DONTSAVE_CANCEL    ' [Save] [Don't Save] [Cancel]        default Save, cancel Cancel
end enum

' Which rect PsMessageBox_CountRenderedTones looks at. See that function for why it exists.
enum
    MBX_PART_CAPTION = 0
    MBX_PART_BODY
    MBX_PART_FOOTER
    MBX_PART_TEXT
    MBX_PART_ICON
    MBX_PART_CLOSE
end enum


' Colors for the built-in painter. Copied on Set.
'
' THE DEFAULTS ARE DARK, inherited from PSBUTTON_COLORS' palette rather than matched to the light
' reference screenshot. A box built with no colour calls therefore looks like the rest of this
' family, which is what a host of these controls actually wants; the demo shows a light set.
'
' THE CLOSE BUTTON IS NOT A PsButton (it would be a fourth tabstop), so it carries its own three
' colour pairs here. Its hot pair is the Windows close-red rather than the theme accent, because
' that colour is the one universally understood "this discards" signal.
'
' THERE IS NO DISABLED MOOD ANYWHERE IN THIS STRUCT. A message box is modal and everything on it
' is live by definition -- a disabled button on a box the user cannot leave would be a trap.
type MBX_COLORS
    ' --- the window as a whole ---
    BorderColor           as COLORREF = BGR( 70, 76, 86)   ' 1px outline around the whole box
    ' --- caption band ---
    CaptionBackColor      as COLORREF = BGR( 44, 49, 58)
    CaptionForeColor      as COLORREF = BGR(200,205,214)
    CloseBackColor        as COLORREF = BGR( 44, 49, 58)   ' = CaptionBackColor: invisible until hot
    CloseForeColor        as COLORREF = BGR(200,205,214)
    CloseBackColorHot     as COLORREF = BGR(196, 43, 28)
    CloseForeColorHot     as COLORREF = BGR(255,255,255)
    CloseBackColorPressed as COLORREF = BGR(200, 70, 60)
    CloseForeColorPressed as COLORREF = BGR(255,255,255)
    ' --- body ---
    BackColor             as COLORREF = BGR( 33, 37, 43)
    ForeColor             as COLORREF = BGR(200,205,214)   ' the message text
    ' --- footer ---
    FooterBackColor       as COLORREF = BGR( 40, 44, 51)
    DividerColor          as COLORREF = BGR( 55, 60, 69)   ' hairline above the footer
    ' --- the four icon kinds. MBX_ICON_NONE has no colour because it has no glyph. ---
    IconColorInfo         as COLORREF = BGR( 97,175,239)
    IconColorWarning      as COLORREF = BGR(229,192,123)
    IconColorError        as COLORREF = BGR(224,108,117)
    IconColorQuestion     as COLORREF = BGR( 97,175,239)
end type


' Everything the painter needs. Every rect is precomputed by LayoutBox -- never re-derive one
' from another, and in particular never re-apply the body padding to rcBody to get rcText,
' because the icon cell and the icon gap can change underneath you.
'
' THE BUTTONS ARE NOT IN HERE. They are real child windows that paint themselves; a paint
' callback draws the box AROUND them and must not try to draw them.
type MBX_PAINTINFO
    hMsgBox        as HWND              ' the box, so the callback can query it
    b              as PsBufferPaint ptr  ' the box's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutBox ---
    rcClient       as RECT              ' the whole client area
    rcCaption      as RECT              ' the caption band, full width
    rcCaptionText  as RECT              ' the title's box: rcCaption minus padding and rcClose
    rcClose        as RECT              ' the X. EMPTY only if the caption band has no height
    rcBody         as RECT              ' between the caption and the footer, full width
    rcIcon         as RECT              ' the icon cell. EMPTY for MBX_ICON_NONE with no override
    ' THE WRAP BOX, NOT THE INK. rcText is what DrawTextW was measured against and what it will
    ' be drawn into, so its width is the wrap width. Its HEIGHT is the measured height of the
    ' wrapped text, which is not the same as the body's height.
    rcText         as RECT
    rcFooter       as RECT              ' the button band, full width
    ' --- State: the control decides these, you only render. ---
    isCloseHot     as boolean           ' the mouse is over the X
    isClosePressed as boolean           ' a live left press on the X AND the cursor is still on it
    isActive       as boolean           ' the box is the active window
    ' --- What to draw ---
    wszCaption     as DWSTRING
    wszText        as DWSTRING
    wszGlyph       as DWSTRING          ' already resolved from the kind (or the host override)
    clrIcon        as COLORREF          ' likewise
end type

type MBX_MESSAGEINFO
    hMsgBox    as HWND
    uMsg       as UINT
    wParam     as WPARAM
    lParam     as LPARAM
end type


' Draw the whole box, INSTEAD OF the built-in painter. Paint through p->b (the box's double
' buffer for this repaint) -- do not touch the screen DC.
'
' The control has already filled the client with BackColor before calling you, so a callback
' that only wants to add something on top does not have to repaint the background.
'
' TWO CONTRACTS WORTH HONOURING:
'   - Draw the message with the SAME font you handed to PsMessageBox_SetFont, and with
'     DT_WORDBREAK. The box's HEIGHT was measured with that font and that flag; a different
'     either means the height lies and the text clips.
'   - DO NOT reach for PaintBorderRect to draw the window outline or the footer divider. It
'     FILLS unconditionally, so used as an outline it erases everything beneath it -- a mistake
'     this family has now made three separate times (PsToggle, PsComboBox, PsNumericUpDown), every
'     time by copying a sibling's callback. PaintRoundOutline and PaintLine stroke without
'     filling. PsMessageBox_CountRenderedTones exists to let you assert you have not done it.
type MBX_PaintCallbackSub as sub( byval p as MBX_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' CAUTION: the result is IGNORED for three messages.
'   WM_LBUTTONUP        - the box holds mouse capture across a press on the X, and the
'                         up-message is what releases it: a callback that suppressed it would
'                         strand capture (the PsListBox bug recorded in Learnings.md).
'   WM_CLOSE            - a modal box MUST be able to end. Suppressing the close would leave the
'                         nested message loop spinning with the parent disabled, which is an
'                         unrecoverable hang rather than a refused action. Veto the dismissal by
'                         not offering a cancel path, not by eating this.
'   WM_DESTROY          - likewise: by then the loop has already ended.
' For every other message TRUE suppresses the control's default handling.
type MBX_MessageCallbackFunc as function( byval m as MBX_MESSAGEINFO ptr ) as boolean


type PSMESSAGEBOX
    hWin            as HWND
    hParent         as HWND = 0        ' disabled for the duration of DoModal. 0 is legal.
    ' --- Content ---
    wszCaption      as DWSTRING        ' the title. "" draws an empty band, it does not remove it
    wszText         as DWSTRING        ' the message. Wrapped, left-justified
    wszGlyph        as DWSTRING        ' set only by PsMessageBox_SetGlyph (host override)
    bGlyphOverride  as boolean = false
    clrIconOverride as COLORREF = 0
    bIconOverride   as boolean = false
    nIconKind       as long = MBX_ICON_NONE
    bBeep           as boolean = true  ' MessageBeep for the kind, before the box is shown
    ' --- Buttons. A FIXED array, not a dynamic one: the cap is 3 by contract, so there is no
    '     insert/delete/reorder and none of the stored-index fix-up code the collection siblings
    '     carry exists here (PsSelectBar's and PsIconPanel's static-by-contract rule). ---
    hButtons(0 to PSMESSAGEBOX_MAXBUTTONS - 1) as HWND
    nButtonCount    as long = 0
    nDefaultIndex   as long = -1       ' -1 = none. Paints the accent border AND claims Enter
    nFocusIndex     as long = -1       ' -1 = "the default, else the first"
    bFocusExplicit  as boolean = false ' has SetFocusButton been called? (see SetDefaultButton)
    nCancelID       as long = 0
    bCancelIDSet    as boolean = false ' unset => the LAST button's id
    ' --- Modal state ---
    nResult         as long = 0
    bRunning        as boolean = false ' the nested loop's condition. Cleared by any dismissal
    ' --- Close-button state. Single-valued: there is exactly one X. ---
    isCloseHot      as boolean = false
    isClosePressed  as boolean = false
    hotTimerOn      as boolean = false
    colors          as MBX_COLORS
    btnColors       as PSBUTTON_COLORS  ' pushed into every button as it is added
    ' Caller-supplied fonts (caller owns all three). NOT named hFont: FreeBASIC is
    ' case-insensitive, so a member called hFont would shadow the TYPE name HFONT inside every
    ' member procedure of this type. See C:\dev\Learnings.md.
    '
    ' hTextFont is the MEASURING font -- it drives the wrap and therefore the box's height.
    hTextFont       as HFONT
    hCaptionFont    as HFONT           ' unset falls back to hTextFont
    hGlyphFont      as HFONT           ' the MESSAGE ICON's font. Unset falls back to hTextFont
    ' The close X's font, SEPARATE from the icon's and not an oversight. The icon cell is ~32px
    ' and the X is a ~10px caption glyph; one font cannot serve both, and since this family
    ' never creates fonts (the caller owns them), the control cannot derive a smaller one
    ' itself. Unset falls back to hGlyphFont, which is the right answer only if the host has
    ' sized that font for the X rather than for the icon.
    hCloseFont      as HFONT
    ' --- Layout inputs. All pixels; DPI-scaled once at Create, raw thereafter. ---
    nCaptionHeight  as long = PSMESSAGEBOX_DEFAULT_CAPTIONHEIGHT
    nCaptionPadX    as long = PSMESSAGEBOX_DEFAULT_CAPTIONPADX
    nCloseWidth     as long = PSMESSAGEBOX_DEFAULT_CLOSEWIDTH
    nBodyPadX       as long = PSMESSAGEBOX_DEFAULT_BODYPADX
    nBodyPadY       as long = PSMESSAGEBOX_DEFAULT_BODYPADY
    nFooterPadX     as long = PSMESSAGEBOX_DEFAULT_FOOTERPADX
    nFooterPadY     as long = PSMESSAGEBOX_DEFAULT_FOOTERPADY
    ' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
    ' that never name the real problem -- hence nIconWidth, nBoxWidth and friends throughout.
    nIconWidth      as long = PSMESSAGEBOX_DEFAULT_ICONWIDTH
    nIconHeight     as long = PSMESSAGEBOX_DEFAULT_ICONHEIGHT
    nIconGap        as long = PSMESSAGEBOX_DEFAULT_ICONGAP
    nButtonGap      as long = PSMESSAGEBOX_DEFAULT_BUTTONGAP
    nButtonMinWidth as long = PSMESSAGEBOX_DEFAULT_BUTTONMINWIDTH
    nMaxWidth       as long = PSMESSAGEBOX_DEFAULT_MAXWIDTH
    nMinWidth       as long = PSMESSAGEBOX_DEFAULT_MINWIDTH
    nBorderThick    as long = PSMESSAGEBOX_DEFAULT_BORDERTHICK    ' NOT DPI-scaled
    nDividerThick   as long = PSMESSAGEBOX_DEFAULT_DIVIDERTHICK   ' NOT DPI-scaled
    ' --- Layout outputs. Rects are DERIVED, never set from outside; LayoutBox() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any size query) runs it,
    '     which coalesces a burst of mutations into ONE measuring pass. ---
    nTextBlockW     as long = 0     ' derived: the width the wrapped text actually used
    nTextBlockH     as long = 0     ' derived: the height it needs at that width
    nButtonH        as long = 0     ' derived: the tallest button's ideal height
    nFooterH        as long = 0     ' derived
    nBodyH          as long = 0     ' derived
    nIdealW         as long = 0     ' derived: the whole box
    nIdealH         as long = 0     ' derived
    rcCaption       as RECT         ' derived
    rcCaptionText   as RECT         ' derived
    rcClose         as RECT         ' derived
    rcBody          as RECT         ' derived
    rcIcon          as RECT         ' derived
    rcText          as RECT         ' derived -- the WRAP BOX, see MBX_PAINTINFO
    rcFooter        as RECT         ' derived
    rcButtons(0 to PSMESSAGEBOX_MAXBUTTONS - 1) as RECT   ' derived; applied by PositionButtons
    bLayoutDirty    as boolean = true
    PaintCallback   as MBX_PaintCallbackSub      ' optional; replaces the built-in painter
    MessageCallback as MBX_MessageCallbackFunc   ' optional

    declare sub      LayoutBox()
    declare function HasIcon() as boolean
    declare function ResolveCancelID() as long
    declare function ResolveFocusIndex() as long
    declare sub      CancelClosePress()
    declare sub      Refresh()
end type


' Is there an icon cell at all? Tested as a named member rather than inline, because the
' measuring pass, the placement pass and the painter must all agree about it and three copies of
' the same test is three places to disagree.
'
' A HOST GLYPH OVERRIDE BEATS MBX_ICON_NONE. Setting a glyph is an unambiguous request for an
' icon, so it turns the cell on rather than being silently ignored because the kind is NONE.
function PSMESSAGEBOX.HasIcon() as boolean
    if this.bGlyphOverride andalso (PsLen( this.wszGlyph ) > 0) then return true
    return (this.nIconKind <> MBX_ICON_NONE)
end function


' What Esc / the X / Alt+F4 return.
'
' UNSET DEFAULTS TO THE LAST BUTTON, which is Cancel in every preset and in the reference
' screenshot. That is a convention, not a law -- a host whose last button is destructive MUST
' call PsMessageBox_SetCancelID. With no buttons at all there is nothing to name, so 0.
function PSMESSAGEBOX.ResolveCancelID() as long
    if this.bCancelIDSet then return this.nCancelID
    if this.nButtonCount <= 0 then return 0
    return PsButton_GetID( this.hButtons( this.nButtonCount - 1 ) )
end function


' Which button gets the focus when the box opens.
'
' THE ORDER MATTERS AND IS THE DOCUMENTED CONTRACT: an explicit SetFocusButton wins, else the
' default button, else the first. That is what makes SetDefaultButton alone do the right thing
' for the common case (the reference screenshot, where Save is both) while still allowing
' "Cancel focused, Delete default".
function PSMESSAGEBOX.ResolveFocusIndex() as long
    if this.nButtonCount <= 0 then return -1
    if this.bFocusExplicit andalso (this.nFocusIndex >= 0) andalso _
       (this.nFocusIndex < this.nButtonCount) then return this.nFocusIndex
    if (this.nDefaultIndex >= 0) andalso (this.nDefaultIndex < this.nButtonCount) then
        return this.nDefaultIndex
    end if
    return 0
end function


' Forget any live press on the X WITHOUT touching the capture. Releasing capture is the
' WndProc's job, and only on the up-message or WM_DESTROY.
sub PSMESSAGEBOX.CancelClosePress()
    this.isClosePressed = false
end sub


' Mark the layout stale and request a repaint. Every mutator routes through here, which is what
' makes layout lazy: a burst of setters costs one measuring pass, not N.
sub PSMESSAGEBOX.Refresh()
    this.bLayoutDirty = true
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' Measure the message, then assign every rect. This is the only place box geometry is produced;
' painting, child placement, sizing and every public rect query consume it.
'
'   iconBlock    = hasIcon ? nIconWidth + nIconGap : 0
'   availW       = nMaxWidth - 2*nBodyPadX - iconBlock          the WRAP width
'   textW/textH  = DrawTextW( DT_CALCRECT or DT_WORDBREAK ) at availW
'   btnW(i)      = max( PsButton ideal width , nButtonMinWidth )
'   nButtonH     = the TALLEST button's ideal height
'   nFooterH     = nButtonCount ? 2*nFooterPadY + nButtonH : 0
'   nBodyH       = 2*nBodyPadY + max( textH , hasIcon ? nIconHeight : 0 )
'
'   nIdealW      = max( 2*nBodyPadX + iconBlock + textW ,
'                       2*nFooterPadX + sum(btnW) + (n-1)*nButtonGap ,
'                       nCaptionPadX + captionTextW + nCloseWidth ,
'                       nMinWidth )
'   nIdealH      = nCaptionHeight + nBodyH + nFooterH
'
' THE BOX SHRINKS TO FIT. nMaxWidth constrains the WRAP, and DT_CALCRECT reports the width the
' text actually used -- so "Delete this file?" gives a small box rather than one padded out to
' the maximum. That is what a real MessageBox does, and it costs nothing extra to do.
'
' nMaxWidth IS NOT A CAP ON THE WINDOW. Three long-captioned buttons legitimately make the box
' wider than the wrap width; the max() above is what lets that happen instead of clipping them.
'
' THE BUTTONS SHARE ONE HEIGHT (the tallest) BUT NOT ONE WIDTH. A row of buttons must sit on one
' baseline or it reads as broken, whereas Windows sizes each button to its own caption above a
' minimum -- which is exactly what the reference screenshot shows (Save / Don't Save / Cancel are
' three different widths).
'
' WHY THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area at all, and DoModal calls PsMessageBox_GetIdealSize to decide how big to MAKE the
' window in the first place. Returning 0 until it had already been sized would be a
' chicken-and-egg trap (PsButton and PsComboBox dodge it by this same ordering).
'
' OVERFLOW: when the client is smaller than ideal the rects are computed HONESTLY rather than
' squeezed (the family rule) -- rcText and rcCaptionText collapse, clamped to EMPTY rather than
' going negative, and the buttons keep their sizes and run off the left edge. Nothing here can
' make that happen (DoModal always sizes to ideal); it matters only for a host that resized the
' box itself.
' ========================================================================================
sub PSMESSAGEBOX.LayoutBox()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    ' ---- the measuring pass ---------------------------------------------------------------
    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font -- never
    ' "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hTextUse = this.hTextFont
    if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hTextUse )

    ' Resolved ONCE for the whole pass rather than re-tested at each of the sites below, so the
    ' box cannot be measured one way and placed another.
    dim as boolean bIcon = this.HasIcon()
    dim as long iconBlock = 0
    if bIcon then iconBlock = this.nIconWidth + this.nIconGap

    dim as long availW = this.nMaxWidth - (2 * this.nBodyPadX) - iconBlock
    ' A floor, not a correction: a host that sets an absurd nMaxWidth gets a narrow column of
    ' text rather than a DT_CALCRECT called with a negative width, whose behaviour is undefined.
    if availW < 40 then availW = 40

    this.nTextBlockW = 0
    this.nTextBlockH = 0
    if PsLen( this.wszText ) then
        dim as RECT rcCalc
        SetRect( @rcCalc, 0, 0, availW, 0 )
        ' DT_CALCRECT with DT_WORDBREAK is the whole auto-height mechanism: it returns the
        ' height the wrapped text needs AND the width it actually used. DT_NOPREFIX matches
        ' PsBufferPaint.PaintText, which forces it -- without it here, a message containing "&"
        ' would measure one width and draw another.
        DrawTextW( hDC, cast( LPCWSTR, this.wszText.Wz() ), PsLen( this.wszText ), @rcCalc, _
                   DT_CALCRECT or DT_WORDBREAK or DT_LEFT or DT_NOPREFIX or DT_EXPANDTABS )
        this.nTextBlockW = rcCalc.right - rcCalc.left
        this.nTextBlockH = rcCalc.bottom - rcCalc.top
    end if

    ' The title only contributes a WIDTH FLOOR -- the caption band's height is declared, not
    ' measured, so a title too tall for it clips rather than growing the box.
    dim as long capTextW = 0
    if PsLen( this.wszCaption ) then
        dim as HFONT hCapUse = this.hCaptionFont
        if hCapUse = 0 then hCapUse = hTextUse
        SelectObject( hDC, hCapUse )
        dim as SIZE sz
        if GetTextExtentPoint32W( hDC, cast( LPCWSTR, this.wszCaption.Wz() ), _
                                  PsLen( this.wszCaption ), @sz ) then capTextW = sz.cx
        SelectObject( hDC, hTextUse )
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' ---- the buttons, measured through their own public API --------------------------------
    ' PsButton_GetIdealSize forces the button's own pending layout, so this is valid even for a
    ' button added moments ago and never sized -- which is always the case here.
    dim btnW(0 to PSMESSAGEBOX_MAXBUTTONS - 1) as long
    this.nButtonH = 0
    dim as long sumBtnW = 0
    for i as long = 0 to this.nButtonCount - 1
        dim as long bw, bh
        PsButton_GetIdealSize( this.hButtons(i), bw, bh )
        if bw < this.nButtonMinWidth then bw = this.nButtonMinWidth
        btnW(i) = bw
        sumBtnW += bw
        if bh > this.nButtonH then this.nButtonH = bh
    next

    this.nFooterH = 0
    dim as long footerW = 0
    if this.nButtonCount > 0 then
        this.nFooterH = (2 * this.nFooterPadY) + this.nButtonH
        footerW = (2 * this.nFooterPadX) + sumBtnW + _
                  ((this.nButtonCount - 1) * this.nButtonGap)
    end if

    ' ---- the ideal size, which does not depend on the client area --------------------------
    dim as long bodyContentH = this.nTextBlockH
    if bIcon then
        if this.nIconHeight > bodyContentH then bodyContentH = this.nIconHeight
    end if
    this.nBodyH = (2 * this.nBodyPadY) + bodyContentH

    dim as long contentW = (2 * this.nBodyPadX) + iconBlock + this.nTextBlockW
    dim as long captionW = this.nCaptionPadX + capTextW + this.nCloseWidth

    this.nIdealW = contentW
    if footerW  > this.nIdealW then this.nIdealW = footerW
    if captionW > this.nIdealW then this.nIdealW = captionW
    if this.nMinWidth > this.nIdealW then this.nIdealW = this.nMinWidth
    this.nIdealH = this.nCaptionHeight + this.nBodyH + this.nFooterH

    ' ---- placement, which does -------------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0; DoModal sizes us from the ideal computed above). Placement
    ' is impossible, so leave the rects alone and ask to be run again -- but note this bail
    ' comes AFTER the ideal size. See the header.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    ' --- caption band ---
    SetRect( @this.rcCaption, 0, 0, clientW, this.nCaptionHeight )
    dim as long closeLeft = clientW - this.nCloseWidth
    if closeLeft < 0 then closeLeft = 0
    SetRect( @this.rcClose, closeLeft, 0, clientW, this.nCaptionHeight )
    dim as long capRight = this.rcClose.left
    if capRight < this.nCaptionPadX then capRight = this.nCaptionPadX
    SetRect( @this.rcCaptionText, this.nCaptionPadX, 0, capRight, this.nCaptionHeight )

    ' --- footer band ---
    if this.nFooterH > 0 then
        SetRect( @this.rcFooter, 0, clientH - this.nFooterH, clientW, clientH )
    else
        SetRectEmpty( @this.rcFooter )
    end if

    ' --- body ---
    dim as long bodyBottom = clientH - this.nFooterH
    if bodyBottom < this.nCaptionHeight then bodyBottom = this.nCaptionHeight
    SetRect( @this.rcBody, 0, this.nCaptionHeight, clientW, bodyBottom )

    ' The icon and the text are each v-centred on the TALLER of the two, so a one-line message
    ' sits level with a 32px icon instead of clinging to its top edge, and a long message keeps
    ' the icon beside its first lines rather than floating at the top of a tall block.
    dim as long contentTop = this.rcBody.top + this.nBodyPadY

    if bIcon then
        dim as long iy = contentTop + ((bodyContentH - this.nIconHeight) \ 2)
        SetRect( @this.rcIcon, this.nBodyPadX, iy, _
                 this.nBodyPadX + this.nIconWidth, iy + this.nIconHeight )
    else
        SetRectEmpty( @this.rcIcon )
    end if

    if this.nTextBlockH > 0 then
        dim as long tLeft  = this.nBodyPadX + iconBlock
        dim as long tRight = clientW - this.nBodyPadX
        ' Degenerate rather than negative when the client is too narrow (see OVERFLOW above).
        if tRight < tLeft then tRight = tLeft
        dim as long ty = contentTop + ((bodyContentH - this.nTextBlockH) \ 2)
        SetRect( @this.rcText, tLeft, ty, tRight, ty + this.nTextBlockH )
    else
        SetRectEmpty( @this.rcText )
    end if

    ' --- the buttons, right-aligned, laid out from the right edge inwards ---
    for i as long = 0 to PSMESSAGEBOX_MAXBUTTONS - 1
        SetRectEmpty( @this.rcButtons(i) )
    next
    if this.nButtonCount > 0 then
        dim as long bx = clientW - this.nFooterPadX
        dim as long by = this.rcFooter.top + this.nFooterPadY
        for i as long = this.nButtonCount - 1 to 0 step -1
            SetRect( @this.rcButtons(i), bx - btnW(i), by, bx, by + this.nButtonH )
            bx = bx - btnW(i) - this.nButtonGap
        next
    end if
end sub


' ========================================================================================
' Which glyph does an icon kind draw?  A PURE FUNCTION on purpose: it takes no control pointer
' and touches no global, so the whole mapping can be asserted directly (PsButton_ResolveMood's
' precedent) rather than only being observable from inside a WM_PAINT.
'
' The HOST OVERRIDE is deliberately NOT folded in here -- that lives in the painter, so this
' function answers exactly one question and answers it the same way every time.
' ========================================================================================
function PsMessageBox_ResolveGlyph( byval nKind as long ) as DWSTRING
    select case nKind
    case MBX_ICON_INFO     : return wchr( PSMESSAGEBOX_GLYPH_INFO )
    case MBX_ICON_WARNING  : return wchr( PSMESSAGEBOX_GLYPH_WARNING )
    case MBX_ICON_ERROR    : return wchr( PSMESSAGEBOX_GLYPH_ERROR )
    case MBX_ICON_QUESTION : return wchr( PSMESSAGEBOX_GLYPH_QUESTION )
    end select
    return ""
end function


' ========================================================================================
' And which colour?  Pure for the same reason. MBX_ICON_NONE answers with the body's own
' foreground rather than with an arbitrary sentinel: nothing will be drawn with it, and a
' sentinel would be a colour a careless painter could actually use.
' ========================================================================================
function PsMessageBox_ResolveIconColor( _
            byval nKind   as long, _
            byval pColors as MBX_COLORS ptr _
            ) as COLORREF

    if pColors = 0 then return 0
    select case nKind
    case MBX_ICON_INFO     : return pColors->IconColorInfo
    case MBX_ICON_WARNING  : return pColors->IconColorWarning
    case MBX_ICON_ERROR    : return pColors->IconColorError
    case MBX_ICON_QUESTION : return pColors->IconColorQuestion
    end select
    return pColors->ForeColor
end function


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A MODAL MESSAGE BOX: an owner-drawn caption band with a title and a close X, a body holding an
' optional icon and a wrapped left-justified message, and a footer holding one to three PsButtons.
'
' IT BLOCKS
'   PsMessageBox_DoModal runs its own nested GetMessage loop and does not return until the box is
'   dismissed. That is the one big departure from every other control in this family, which are
'   passive and pumped by their host.
'
' THERE IS NO PUMP OBLIGATION
'   Precisely BECAUSE it owns its own loop, the box calls IsDialogMessage itself -- so Tab, the
'   focus ring and Space/Enter all work with nothing added to the host's pump. Unlike PsComboBox,
'   PsNumericUpDown and PsTextBox there is no PsMessageBox_FilterMessage.
'
' IT IS A CONTAINER, AND THAT IS WHY IT CARRIES WS_EX_CONTROLPARENT
'   The exact opposite of the PsButton fix. The tabstops here are the child buttons, so the dialog
'   manager MUST descend into this window. Do not "correct" this to 0; see the per-sibling table
'   in C:\dev\Learnings.md, and the self-test, which asserts the flag both ways.
'
' THE PARENT IS DISABLED FOR THE DURATION
'   EnableWindow(hParent, FALSE) at the top of DoModal, restored BEFORE the box is destroyed --
'   destroying an owned window while its owner is still disabled hands activation to another
'   application. hParent = 0 is legal: nothing is disabled and the box centres on the screen.
'
' NO TOOLTIPS
'   By request. There is no tooltip window, no MBX_TooltipCallback and no hover text anywhere.
'
' NO Ctrl+C
'   The real Win32 MessageBox copies its text to the clipboard on Ctrl+C. This one does not: the
'   message is drawn with DrawTextW and there is no text object to select from.
'
' LIFETIME
'   PsMessageBox_DoModal DESTROYS the box before returning, so the handle is dead afterwards. A
'   box created and then abandoned without DoModal must be destroyed by the host with
'   DestroyWindow. The three fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation and the modal loop.
'
'   Create makes a hidden, zero-sized WS_POPUP window. Configure it, then DoModal, which
'     measures, sizes, centres, shows, pumps, and returns the id of whatever dismissed it.
'   DoModal returns the clicked button's id, or -- for Esc / the X / Alt+F4 / a WM_CLOSE from
'     anywhere -- whatever PSMESSAGEBOX.ResolveCancelID() answers.
'   Show is Create + configure + DoModal in one call, for the common case.
' ----------------------------------------------------------------------------------------
declare function PsMessageBox_Create( byval hWndParent as HWND ) as HWND
declare function PsMessageBox_DoModal( byval hMsgBox as HWND ) as long
declare function PsMessageBox_Show( byval hWndParent as HWND, _
                                   byval Text as DWSTRING, _
                                   byval Caption as DWSTRING, _
                                   byval nIconKind as long = MBX_ICON_NONE, _
                                   byval nPreset as long = MBX_BTN_OK ) as long

' ----------------------------------------------------------------------------------------
' Content.  All SILENT.
'
'   SetCaption is the title in the band. It is NOT the window text -- unlike PsButton, this
'     control does not alias WM_SETTEXT, because a message box is not something generic dialog
'     code walks looking for captions.
'   SetText is the message. Embedded CRLF is honoured as well as the automatic wrap.
'   SetIconKind picks one of the five kinds; SetGlyph and SetIconColor override the glyph and
'     the colour that kind resolved to. A glyph override TURNS THE ICON ON even when the kind is
'     MBX_ICON_NONE -- setting a glyph is an unambiguous request for one.
'   SetBeep(false) suppresses the MessageBeep that otherwise fires for the kind just before the
'     box is shown.
' ----------------------------------------------------------------------------------------
declare function PsMessageBox_GetCaption( byval hMsgBox as HWND ) as DWSTRING
declare sub      PsMessageBox_SetCaption( byval hMsgBox as HWND, byval Text as DWSTRING )
declare function PsMessageBox_GetText( byval hMsgBox as HWND ) as DWSTRING
declare sub      PsMessageBox_SetText( byval hMsgBox as HWND, byval Text as DWSTRING )
declare function PsMessageBox_GetIconKind( byval hMsgBox as HWND ) as long
declare sub      PsMessageBox_SetIconKind( byval hMsgBox as HWND, byval nKind as long )
declare sub      PsMessageBox_SetGlyph( byval hMsgBox as HWND, byval Glyph as DWSTRING )
declare sub      PsMessageBox_SetIconColor( byval hMsgBox as HWND, byval clr as COLORREF )
declare sub      PsMessageBox_SetBeep( byval hMsgBox as HWND, byval bBeep as boolean )

' ----------------------------------------------------------------------------------------
' Buttons.  One to three, laid out left to right in the order added and right-aligned in the
' footer. Tab order follows the order added.
'
'   AddButton returns the PsButton's HWND so a host can reach past this API for anything the
'     box does not expose (a glyph on one button, a different colour set for a destructive
'     choice). It returns 0 if the box already holds PSMESSAGEBOX_MAXBUTTONS.
'   The id is YOUR value and is what DoModal returns. It is never used as a command id.
'   AddPreset adds a whole set AND sets the default button and the cancel id to match. It is
'     additive, so calling it on a box that already has buttons is a host error.
'   SetDefaultButton paints the accent border, claims Enter box-wide, AND takes the initial
'     focus -- unless SetFocusButton is called AFTER it.
'   SetCancelID overrides what Esc / the X / Alt+F4 return. Unset, that is the LAST button's id.
' ----------------------------------------------------------------------------------------
declare function PsMessageBox_AddButton( byval hMsgBox as HWND, byval Text as DWSTRING, byval id as long ) as HWND
declare sub      PsMessageBox_AddPreset( byval hMsgBox as HWND, byval nPreset as long )
declare function PsMessageBox_GetButtonCount( byval hMsgBox as HWND ) as long
declare function PsMessageBox_GetButtonHandle( byval hMsgBox as HWND, byval idx as long ) as HWND
declare function PsMessageBox_GetDefaultButton( byval hMsgBox as HWND ) as long
declare sub      PsMessageBox_SetDefaultButton( byval hMsgBox as HWND, byval idx as long )
declare function PsMessageBox_GetFocusButton( byval hMsgBox as HWND ) as long
declare sub      PsMessageBox_SetFocusButton( byval hMsgBox as HWND, byval idx as long )
declare function PsMessageBox_GetCancelID( byval hMsgBox as HWND ) as long
declare sub      PsMessageBox_SetCancelID( byval hMsgBox as HWND, byval id as long )

' ----------------------------------------------------------------------------------------
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
' Create-time defaults are scaled for you). The two THICKNESS values should not be scaled at
' all: a hairline stays a hairline.
'
'   SetMaxWidth is the WRAP width, not a hard cap on the window: if the footer's buttons need
'     more room than that, the box is wider. Text is what the maximum constrains.
'   SetMinWidth is a floor on the whole box.
'   GetIdealSize forces a pending layout and returns the measured size of the whole box. Like
'     PsButton's, it is valid BEFORE the window has ever been sized -- which here is load-bearing,
'     because DoModal sizes the window FROM it.
'   The rect queries force a pending layout, so results are always current. They return FALSE if
'     the box has no geometry yet. An absent part's rect is EMPTY and the query still returns
'     TRUE -- empty is the honest answer.
' ----------------------------------------------------------------------------------------
declare sub      PsMessageBox_SetMaxWidth( byval hMsgBox as HWND, byval nMaxWidth as long )
declare sub      PsMessageBox_SetMinWidth( byval hMsgBox as HWND, byval nMinWidth as long )
declare sub      PsMessageBox_SetCaptionHeight( byval hMsgBox as HWND, byval nHeight as long )
declare sub      PsMessageBox_SetBodyPadding( byval hMsgBox as HWND, byval nX as long, byval nY as long )
declare sub      PsMessageBox_SetFooterPadding( byval hMsgBox as HWND, byval nX as long, byval nY as long )
declare sub      PsMessageBox_SetIconSize( byval hMsgBox as HWND, byval nIconWidth as long, byval nIconHeight as long )
declare sub      PsMessageBox_SetIconGap( byval hMsgBox as HWND, byval nGap as long )
declare sub      PsMessageBox_SetButtonGap( byval hMsgBox as HWND, byval nGap as long )
declare sub      PsMessageBox_SetButtonMinWidth( byval hMsgBox as HWND, byval nMinWidth as long )
declare sub      PsMessageBox_SetBorderThickness( byval hMsgBox as HWND, byval nThickness as long )
declare sub      PsMessageBox_GetIdealSize( byval hMsgBox as HWND, byref nBoxWidth as long, byref nBoxHeight as long )
declare function PsMessageBox_GetCaptionRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
declare function PsMessageBox_GetCloseRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
declare function PsMessageBox_GetBodyRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
declare function PsMessageBox_GetIconRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
declare function PsMessageBox_GetTextRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
declare function PsMessageBox_GetFooterRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
declare function PsMessageBox_GetButtonRect( byval hMsgBox as HWND, byval idx as long, byref rc as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  SetColors copies the whole struct. GetColors fills one out, so the
' read-modify-write "change one color" idiom is Get, assign, Set.
'
'   SetButtonColors is pushed into every button that exists NOW and into every button added
'     LATER, so the order of the two calls does not matter.
'
' ALL THREE fonts are BORROWED, never owned: keep them alive and destroy them yourself.
'   SetFont        - the MESSAGE font, and the MEASURING font. Changing it re-wraps, which
'                    changes the box's height. Also pushed into the buttons.
'   SetCaptionFont - the title's font. Unset falls back to the message font.
'   SetGlyphFont   - the MESSAGE ICON's font (normally Segoe Fluent Icons). Unset falls back to
'                    the message font, which draws the codepoints as missing-glyph boxes -- so
'                    in practice a host that wants an icon must set this.
'   SetCloseGlyphFont - the close X's font, SEPARATE because the icon cell is ~32px and the X is
'                    a ~10px caption glyph. Unset falls back to the icon font, which will draw a
'                    comically large X if that font was sized for the icon. Two handles, because
'                    this family never creates fonts and so cannot derive a smaller one itself.
' ----------------------------------------------------------------------------------------
declare sub      PsMessageBox_GetColors( byval hMsgBox as HWND, byval pColors as MBX_COLORS ptr )
declare sub      PsMessageBox_SetColors( byval hMsgBox as HWND, byval pColors as MBX_COLORS ptr )
declare sub      PsMessageBox_SetButtonColors( byval hMsgBox as HWND, byval pColors as PSBUTTON_COLORS ptr )
declare function PsMessageBox_GetFont( byval hMsgBox as HWND ) as HFONT
declare sub      PsMessageBox_SetFont( byval hMsgBox as HWND, byval hTextFont as HFONT )
declare function PsMessageBox_GetCaptionFont( byval hMsgBox as HWND ) as HFONT
declare sub      PsMessageBox_SetCaptionFont( byval hMsgBox as HWND, byval hCaptionFont as HFONT )
declare function PsMessageBox_GetGlyphFont( byval hMsgBox as HWND ) as HFONT
declare sub      PsMessageBox_SetGlyphFont( byval hMsgBox as HWND, byval hGlyphFont as HFONT )
declare function PsMessageBox_GetCloseGlyphFont( byval hMsgBox as HWND ) as HFONT
declare sub      PsMessageBox_SetCloseGlyphFont( byval hMsgBox as HWND, byval hCloseFont as HFONT )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback   - draw the whole box INSTEAD OF the built-in painter. The child buttons
'                     still paint themselves.
'   MessageCallback - observe messages; TRUE suppresses the box's default handling (IGNORED for
'                     WM_LBUTTONUP, WM_CLOSE and WM_DESTROY).
' ----------------------------------------------------------------------------------------
declare sub      PsMessageBox_SetPaintCallback( byval hMsgBox as HWND, byval usersub as MBX_PaintCallbackSub )
declare sub      PsMessageBox_SetMessageCallback( byval hMsgBox as HWND, byval userfunc as MBX_MessageCallbackFunc )

' ----------------------------------------------------------------------------------------
' Plumbing.
'
'   CountRenderedTones - render the box OFFSCREEN with its current painter and return how many
'                 distinct colours land inside one of its rects (capped at 64). Nothing in the
'                 control calls it; it exists to make one specific defect measurable.
'
'                 THE DEFECT: PsBufferPaint.PaintBorderRect FILLS before it strokes, so a paint
'                 callback that reaches for it to draw the window outline or the footer divider
'                 floods everything beneath and the box renders as one solid block. That has
'                 shipped three times in this family (PsToggle, PsComboBox, PsNumericUpDown), every
'                 time from a copied callback, and it survives a glance because the flood colour
'                 is a real colour from the control. A wiped part is literally ONE tone, so
'                 "count > 1" is an unambiguous assertion where a screenshot is a judgement.
'
'                 A HOST THAT WRITES ITS OWN PaintCallback SHOULD ASSERT THIS. That is why it is
'                 public rather than private to the self-test (PsButton's precedent).
'
'                 The PAINTINFO is built by the same routine WM_PAINT uses, because a probe that
'                 hand-builds that struct and leaves one field zeroed silently skips the thing it
'                 is measuring -- that produced a near-miss false PASS the first time this was
'                 written for PsButton (Learnings.md). Returns 0 if the box has no geometry yet,
'                 so call it AFTER PsMessageBox_LayoutForTest.
'
'   LayoutForTest - size the box to its ideal size WITHOUT showing it or entering the modal
'                 loop, so geometry can be asserted and the tone probe can run. There is no
'                 other way in: every rect this control owns depends on the client area, and
'                 the client area is only established by DoModal.
'
'   RunSelfTest - geometry, icon-resolution, cancel/focus-resolution and tab-navigability
'                 assertions, gated on the PSMESSAGEBOX_SELFTEST env var. Call it from the host
'                 before the message loop. It never enters a modal loop.
' ----------------------------------------------------------------------------------------
declare sub      PsMessageBox_LayoutForTest( byval hMsgBox as HWND )
declare function PsMessageBox_CountRenderedTones( byval hMsgBox as HWND, byval nPart as long ) as long
declare sub      PsMessageBox_RunSelfTest( byval hWndParent as HWND )
