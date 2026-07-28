''
''  PsColorPicker.inc  --  owner-drawn colour picker PANEL (tabs + matrix/swatches + fields)
''

#include once "PsColorPicker.bi"
' The scrollbar's IMPLEMENTATION, pulled in here rather than left to the host. `#include once`
' makes it a no-op in a host that already includes it for its own use (tiko does), and including
' it here means adopting this control is still just "add the files and include the .inc" -- a
' host that forgot it would get four undefined references at LINK time, which is the least
' helpful moment to find out.
#include once "PsVScrollBar.inc"


' Forward declarations: the mutators re-derive the body selection, and the WndProc uses the
' field engine well above where it is defined.
declare sub      PsColorPicker_SyncBodySel( byval pClr as PSCOLORPICKER ptr )
declare sub      PsColorPicker_CommitField( byval pClr as PSCOLORPICKER ptr )
declare function PsColorPicker_GridCount( byval pClr as PSCOLORPICKER ptr ) as long


' ========================================================================================
' THE TWO FIXED TABLES.
'
' NAMED colours, not anonymous swatches, and they are drawn as a LIST rather than a grid -- a
' named colour without its name is just a swatch, and the name is most of what these two tabs
' are for. (An earlier version of this control had them as 6x8 swatch grids. That was the wrong
' call: it fitted the panel neatly and told the user nothing.)
'
' The web table is the standard 137-entry named set in the ORDER the platform colour editors use
' -- greys, then reds through yellows, greens, cyans, blues, purples, pinks -- rather than
' alphabetically. Alphabetical order puts Azure next to Beige and makes the list impossible to
' scan for "something like this but darker", which is the actual task. The two duplicate names
' (Aqua = Cyan, Fuchsia = Magenta) are dropped: two rows of identical colour under different
' names make the selection ring look broken.
'
' System colours are read through GetSysColor at CALL time rather than cached, so a shell theme
' change is picked up on the next repaint without this control listening for WM_SYSCOLORCHANGE.
' Index 25 is skipped -- it is unused in the COLOR_* series.
'
' Written as PARALLEL ARRAYS of scalars rather than one array of a UDT: fbc's array initialisers
' are reliable for flat scalar lists and fussy about aggregates, and this file is vendored into
' hosts where a build failure is expensive to diagnose.
' ========================================================================================
dim shared as zstring * 21 gClrWebName(0 to 136) = { _
    "Black"               , "DimGray"             , "Gray"                , "DarkGray", _
    "Silver"              , "LightGray"           , "Gainsboro"           , "WhiteSmoke", _
    "White"               , "RosyBrown"           , "IndianRed"           , "Brown", _
    "Firebrick"           , "LightCoral"          , "Maroon"              , "DarkRed", _
    "Red"                 , "Snow"                , "MistyRose"           , "Salmon", _
    "Tomato"              , "DarkSalmon"          , "Coral"               , "OrangeRed", _
    "LightSalmon"         , "Sienna"              , "SeaShell"            , "Chocolate", _
    "SaddleBrown"         , "SandyBrown"          , "PeachPuff"           , "Peru", _
    "Linen"               , "Bisque"              , "DarkOrange"          , "BurlyWood", _
    "Tan"                 , "AntiqueWhite"        , "NavajoWhite"         , "BlanchedAlmond", _
    "PapayaWhip"          , "Moccasin"            , "Orange"              , "Wheat", _
    "OldLace"             , "FloralWhite"         , "DarkGoldenrod"       , "Cornsilk", _
    "Goldenrod"           , "PaleGoldenrod"       , "Khaki"               , "LemonChiffon", _
    "DarkKhaki"           , "Beige"               , "LightGoldenrodYellow", "Olive", _
    "Yellow"              , "LightYellow"         , "Ivory"               , "OliveDrab", _
    "YellowGreen"         , "DarkOliveGreen"      , "GreenYellow"         , "Chartreuse", _
    "LawnGreen"           , "DarkSeaGreen"        , "ForestGreen"         , "LimeGreen", _
    "LightGreen"          , "PaleGreen"           , "DarkGreen"           , "Green", _
    "Lime"                , "Honeydew"            , "SeaGreen"            , "MediumSeaGreen", _
    "SpringGreen"         , "MintCream"           , "MediumSpringGreen"   , "MediumAquamarine", _
    "Aquamarine"          , "Turquoise"           , "LightSeaGreen"       , "MediumTurquoise", _
    "DarkSlateGray"       , "PaleTurquoise"       , "Teal"                , "DarkCyan", _
    "Cyan"                , "LightCyan"           , "Azure"               , "DarkTurquoise", _
    "CadetBlue"           , "PowderBlue"          , "LightBlue"           , "DeepSkyBlue", _
    "SkyBlue"             , "LightSkyBlue"        , "SteelBlue"           , "AliceBlue", _
    "DodgerBlue"          , "SlateGray"           , "LightSlateGray"      , "LightSteelBlue", _
    "CornflowerBlue"      , "RoyalBlue"           , "MidnightBlue"        , "Lavender", _
    "Navy"                , "DarkBlue"            , "MediumBlue"          , "Blue", _
    "GhostWhite"          , "SlateBlue"           , "DarkSlateBlue"       , "MediumSlateBlue", _
    "MediumPurple"        , "BlueViolet"          , "Indigo"              , "DarkOrchid", _
    "DarkViolet"          , "MediumOrchid"        , "Thistle"             , "Plum", _
    "Violet"              , "Purple"              , "DarkMagenta"         , "Magenta", _
    "Orchid"              , "MediumVioletRed"     , "DeepPink"            , "HotPink", _
    "LavenderBlush"       , "PaleVioletRed"       , "Crimson"             , "Pink", _
    "LightPink" }

dim shared as COLORREF gClrWebColor(0 to 136) = { _
    BGR(  0,  0,  0)  , BGR(105,105,105)  , BGR(128,128,128)  , BGR(169,169,169), _
    BGR(192,192,192)  , BGR(211,211,211)  , BGR(220,220,220)  , BGR(245,245,245), _
    BGR(255,255,255)  , BGR(188,143,143)  , BGR(205, 92, 92)  , BGR(165, 42, 42), _
    BGR(178, 34, 34)  , BGR(240,128,128)  , BGR(128,  0,  0)  , BGR(139,  0,  0), _
    BGR(255,  0,  0)  , BGR(255,250,250)  , BGR(255,228,225)  , BGR(250,128,114), _
    BGR(255, 99, 71)  , BGR(233,150,122)  , BGR(255,127, 80)  , BGR(255, 69,  0), _
    BGR(255,160,122)  , BGR(160, 82, 45)  , BGR(255,245,238)  , BGR(210,105, 30), _
    BGR(139, 69, 19)  , BGR(244,164, 96)  , BGR(255,218,185)  , BGR(205,133, 63), _
    BGR(250,240,230)  , BGR(255,228,196)  , BGR(255,140,  0)  , BGR(222,184,135), _
    BGR(210,180,140)  , BGR(250,235,215)  , BGR(255,222,173)  , BGR(255,235,205), _
    BGR(255,239,213)  , BGR(255,228,181)  , BGR(255,165,  0)  , BGR(245,222,179), _
    BGR(253,245,230)  , BGR(255,250,240)  , BGR(184,134, 11)  , BGR(255,248,220), _
    BGR(218,165, 32)  , BGR(238,232,170)  , BGR(240,230,140)  , BGR(255,250,205), _
    BGR(189,183,107)  , BGR(245,245,220)  , BGR(250,250,210)  , BGR(128,128,  0), _
    BGR(255,255,  0)  , BGR(255,255,224)  , BGR(255,255,240)  , BGR(107,142, 35), _
    BGR(154,205, 50)  , BGR( 85,107, 47)  , BGR(173,255, 47)  , BGR(127,255,  0), _
    BGR(124,252,  0)  , BGR(143,188,143)  , BGR( 34,139, 34)  , BGR( 50,205, 50), _
    BGR(144,238,144)  , BGR(152,251,152)  , BGR(  0,100,  0)  , BGR(  0,128,  0), _
    BGR(  0,255,  0)  , BGR(240,255,240)  , BGR( 46,139, 87)  , BGR( 60,179,113), _
    BGR(  0,255,127)  , BGR(245,255,250)  , BGR(  0,250,154)  , BGR(102,205,170), _
    BGR(127,255,212)  , BGR( 64,224,208)  , BGR( 32,178,170)  , BGR( 72,209,204), _
    BGR( 47, 79, 79)  , BGR(175,238,238)  , BGR(  0,128,128)  , BGR(  0,139,139), _
    BGR(  0,255,255)  , BGR(224,255,255)  , BGR(240,255,255)  , BGR(  0,206,209), _
    BGR( 95,158,160)  , BGR(176,224,230)  , BGR(173,216,230)  , BGR(  0,191,255), _
    BGR(135,206,235)  , BGR(135,206,250)  , BGR( 70,130,180)  , BGR(240,248,255), _
    BGR( 30,144,255)  , BGR(112,128,144)  , BGR(119,136,153)  , BGR(176,196,222), _
    BGR(100,149,237)  , BGR( 65,105,225)  , BGR( 25, 25,112)  , BGR(230,230,250), _
    BGR(  0,  0,128)  , BGR(  0,  0,139)  , BGR(  0,  0,205)  , BGR(  0,  0,255), _
    BGR(248,248,255)  , BGR(106, 90,205)  , BGR( 72, 61,139)  , BGR(123,104,238), _
    BGR(147,112,219)  , BGR(138, 43,226)  , BGR( 75,  0,130)  , BGR(153, 50,204), _
    BGR(148,  0,211)  , BGR(186, 85,211)  , BGR(216,191,216)  , BGR(221,160,221), _
    BGR(238,130,238)  , BGR(128,  0,128)  , BGR(139,  0,139)  , BGR(255,  0,255), _
    BGR(218,112,214)  , BGR(199, 21,133)  , BGR(255, 20,147)  , BGR(255,105,180), _
    BGR(255,240,245)  , BGR(219,112,147)  , BGR(220, 20, 60)  , BGR(255,192,203), _
    BGR(255,182,193) }

dim shared as long gClrSysIndex(0 to 29) = { _
    0   , 1   , 2   , 3   , 4   , 5   , 6   , 7   , 8   , 9, _
    10  , 11  , 12  , 13  , 14  , 15  , 16  , 17  , 18  , 19, _
    20  , 21  , 22  , 23  , 24  , 26  , 27  , 28  , 29  , 30 }

dim shared as zstring * 26 gClrSysName(0 to 29) = { _
    "Scroll Bar"                , "Desktop"                   , "Active Caption", _
    "Inactive Caption"          , "Menu"                      , "Window", _
    "Window Frame"              , "Menu Text"                 , "Window Text", _
    "Caption Text"              , "Active Border"             , "Inactive Border", _
    "App Workspace"             , "Highlight"                 , "Highlight Text", _
    "Button Face"               , "Button Shadow"             , "Gray Text", _
    "Button Text"               , "Inactive Caption Text"     , "Button Highlight", _
    "3D Dark Shadow"            , "3D Light"                  , "Tooltip Text", _
    "Tooltip Background"        , "Hot Tracking"              , "Gradient Active Caption", _
    "Gradient Inactive Caption" , "Menu Highlight"            , "Menu Bar" }


function PsColorPicker_WebColorCount() as long
    return ubound(gClrWebColor) - lbound(gClrWebColor) + 1
end function

function PsColorPicker_WebColorAt( byval idx as long ) as COLORREF
    if (idx < 0) orelse (idx > ubound(gClrWebColor)) then return 0
    return gClrWebColor(idx)
end function

function PsColorPicker_WebColorName( byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut
    if (idx < 0) orelse (idx > ubound(gClrWebName)) then return wszOut
    wszOut = gClrWebName(idx)
    return wszOut
end function

function PsColorPicker_SystemColorCount() as long
    return ubound(gClrSysIndex) - lbound(gClrSysIndex) + 1
end function

function PsColorPicker_SystemColorAt( byval idx as long ) as COLORREF
    if (idx < 0) orelse (idx > ubound(gClrSysIndex)) then return 0
    return GetSysColor( gClrSysIndex(idx) )
end function

function PsColorPicker_SystemColorName( byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut
    if (idx < 0) orelse (idx > ubound(gClrSysName)) then return wszOut
    wszOut = gClrSysName(idx)
    return wszOut
end function


' ========================================================================================
' Is (x,y) inside rc? Written out rather than calling PtInRect so it is a PURE function of four
' longs and can be asserted without a window -- and so the half-open convention (left/top
' inclusive, right/bottom exclusive) is stated here once rather than assumed at each call site.
' It is the same convention the cell arithmetic uses, which is what keeps the two agreeing at a
' grid's last column.
' ========================================================================================
function PsColorPicker_PtIn( byval rc as RECT, byval x as long, byval y as long ) as boolean
    return ((x >= rc.left) andalso (x < rc.right) andalso (y >= rc.top) andalso (y < rc.bottom))
end function


' ========================================================================================
' Resolve a handle to its state block. UserData(0), like every sibling except PsOptionButton --
' this control holds no handles to other controls, so the registry that made that one different
' would be machinery with nothing to protect against.
' ========================================================================================
function GetColorPickerPointer( byval hColorPicker as HWND ) as PSCOLORPICKER ptr
    if hColorPicker = 0 then return 0
    dim as CWindow ptr pWindow = AfxCWindowPtr( hColorPicker )
    if pWindow = 0 then return 0
    dim as PSCOLORPICKER ptr pClr = cast( PSCOLORPICKER ptr, pWindow->UserData(0) )
    if pClr = 0 then return 0
    if pClr->dwSignature <> PSCOLORPICKER_SIGNATURE then return 0
    return pClr
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- three messages deliberately do not. See
' CLR_MessageCallbackFunc.
' ========================================================================================
private function PsColorPicker_CallMessage( _
            byval pClr   as PSCOLORPICKER ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pClr = 0 then return false
    if pClr->MessageCallback = 0 then return false

    dim as PSCOLORPICKER_MESSAGEINFO m
    m.hColorPicker = hwnd
    m.uMsg         = uMsg
    m.wParam       = wParam
    m.lParam       = lParam
    return pClr->MessageCallback( @m )
end function


' ========================================================================================
' How many cells does the CURRENT tab's body hold?
'
' The palette's count is pulled from the host on every call rather than cached, which is the
' contract stated on CLR_PaletteCountCallbackFunc: a palette that changes underneath must not
' need an invalidation protocol. That does mean the callback is invoked several times per
' repaint, so a host implementing it should make it cheap -- reading an array's ubound, not
' re-parsing a file.
' ========================================================================================
function PsColorPicker_GridCount( byval pClr as PSCOLORPICKER ptr ) as long
    if pClr = 0 then return 0
    select case pClr->nTab
    case CLR_TAB_CUSTOM
        return pClr->nMatrixCols * pClr->nMatrixRows
    case CLR_TAB_WEB
        return PsColorPicker_WebColorCount()
    case CLR_TAB_SYSTEM
        return PsColorPicker_SystemColorCount()
    case CLR_TAB_PALETTE
        if pClr->PaletteCountCallback = 0 then return 0
        return pClr->PaletteCountCallback( pClr->hWin )
    end select
    return 0
end function


' The colour of body cell idx on the current tab. FALSE when idx is out of range, or when the
' host's palette entry callback declines it -- a palette is allowed to have holes.
private function PsColorPicker_GridColorAt( _
            byval pClr as PSCOLORPICKER ptr, _
            byval idx  as long, _
            byref clr  as COLORREF _
            ) as boolean

    clr = 0
    if pClr = 0 then return false
    if idx < 0 then return false
    if idx >= PsColorPicker_GridCount( pClr ) then return false

    select case pClr->nTab
    case CLR_TAB_CUSTOM
        dim as long c = idx mod pClr->nMatrixCols
        dim as long r = idx \ pClr->nMatrixCols
        clr = PsColorPicker_MatrixColorAt( c, r, pClr->nMatrixCols, pClr->nMatrixRows )
        return true
    case CLR_TAB_WEB
        clr = PsColorPicker_WebColorAt( idx )
        return true
    case CLR_TAB_SYSTEM
        clr = PsColorPicker_SystemColorAt( idx )
        return true
    case CLR_TAB_PALETTE
        if pClr->PaletteEntryCallback = 0 then return false
        dim as DWSTRING wszName
        return pClr->PaletteEntryCallback( pClr->hWin, idx, wszName, clr )
    end select
    return false
end function


' The NAME of body entry idx on the current tab, or "" when the tab has no names (the matrix is
' a law, not a list -- its cells are unnamed by construction).
private function PsColorPicker_GridNameAt( _
            byval pClr as PSCOLORPICKER ptr, _
            byval idx  as long, _
            byref wszName as DWSTRING _
            ) as boolean

    wszName = ""
    if pClr = 0 then return false
    if idx < 0 then return false
    if idx >= PsColorPicker_GridCount( pClr ) then return false

    select case pClr->nTab
    case CLR_TAB_WEB
        wszName = PsColorPicker_WebColorName( idx )
        return true
    case CLR_TAB_SYSTEM
        wszName = PsColorPicker_SystemColorName( idx )
        return true
    case CLR_TAB_PALETTE
        if pClr->PaletteEntryCallback = 0 then return false
        dim as COLORREF clr
        return pClr->PaletteEntryCallback( pClr->hWin, idx, wszName, clr )
    end select
    return false
end function


' Is this tab's body a scrolling LIST of named rows, or the matrix? One test, because the
' renderer, the hit test and the scrollbar all have to agree about it.
private function PsColorPicker_IsListTab( byval pClr as PSCOLORPICKER ptr ) as boolean
    if pClr = 0 then return false
    return (pClr->nTab <> CLR_TAB_CUSTOM)
end function


' How many rows fit in the list viewport RIGHT NOW. Derived from the rect rather than from
' nListRows, because the palette tab gives up part of its height to the Assign button and the
' host may have sized the control shorter than ideal.
private function PsColorPicker_VisibleRows( byval pClr as PSCOLORPICKER ptr ) as long
    if pClr = 0 then return 0
    if pClr->nRowH <= 0 then return 0
    dim as long h = pClr->rcSwatches.bottom - pClr->rcSwatches.top
    if h <= 0 then return 0
    return h \ pClr->nRowH
end function


' Clamp the scroll position so the last page cannot be scrolled past. Called from every writer of
' nScrollTop, so "the list cannot show empty space below its last row" is one rule rather than
' four copies of it.
private sub PsColorPicker_ClampScroll( byval pClr as PSCOLORPICKER ptr )
    if pClr = 0 then exit sub
    dim as long nCount = PsColorPicker_GridCount( pClr )
    dim as long nVis = PsColorPicker_VisibleRows( pClr )
    dim as long nMax = nCount - nVis
    if nMax < 0 then nMax = 0
    if pClr->nScrollTop > nMax then pClr->nScrollTop = nMax
    if pClr->nScrollTop < 0 then pClr->nScrollTop = 0
end sub


' Push the current list state into the scrollbar child, and show or hide it.
'
' The STRIP stays reserved either way (see LayoutColorPicker); only the scrollbar WINDOW comes
' and goes, so a short list leaves a gap of panel background rather than re-flowing every row.
private sub PsColorPicker_SyncScrollBar( byval pClr as PSCOLORPICKER ptr )
    if pClr = 0 then exit sub
    if IsWindow( pClr->hScroll ) = 0 then exit sub

    if PsColorPicker_IsListTab( pClr ) = false then
        ShowWindow( pClr->hScroll, SW_HIDE )
        exit sub
    end if

    PsColorPicker_ClampScroll( pClr )

    dim as long nCount = PsColorPicker_GridCount( pClr )
    dim as long nVis   = PsColorPicker_VisibleRows( pClr )

    SetWindowPos( pClr->hScroll, 0, pClr->rcScroll.left, pClr->rcScroll.top, _
                  pClr->rcScroll.right - pClr->rcScroll.left, _
                  pClr->rcScroll.bottom - pClr->rcScroll.top, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    PsVScrollBar_SetRange( pClr->hScroll, nCount, nVis, pClr->nScrollTop )
    ShowWindow( pClr->hScroll, iif( PsVScrollBar_NeedsThumb( pClr->hScroll ), SW_SHOW, SW_HIDE ) )
end sub


' Scroll the selected row into view. THIS IS WHAT MAKES SetColor USEFUL ON A 137-ROW LIST: a host
' that sets the picker to "MidnightBlue" and gets row 106 selected but not shown has been told
' nothing. Only moves the viewport when the row is actually outside it, so it never fights a user
' who has deliberately scrolled away from the selection.
private sub PsColorPicker_EnsureSelVisible( byval pClr as PSCOLORPICKER ptr )
    if pClr = 0 then exit sub
    if PsColorPicker_IsListTab( pClr ) = false then exit sub
    if pClr->nBodySel < 0 then exit sub

    dim as long nVis = PsColorPicker_VisibleRows( pClr )
    if nVis <= 0 then exit sub

    if pClr->nBodySel < pClr->nScrollTop then
        pClr->nScrollTop = pClr->nBodySel
    elseif pClr->nBodySel > (pClr->nScrollTop + nVis - 1) then
        pClr->nScrollTop = pClr->nBodySel - nVis + 1
    end if
    PsColorPicker_ClampScroll( pClr )
    PsColorPicker_SyncScrollBar( pClr )
end sub


' ========================================================================================
' The list scrollbar moved. Resolved back to the owning picker through GetParent -- the
' scrollbar is our own child and nobody else's, so the parent is always us.
'
' It only moves the VIEWPORT. It never touches nBodySel, the colour or the caret: scrolling is
' looking, not choosing, and a list that selected whatever it scrolled past would be unusable
' with the wheel.
' ========================================================================================
private sub PsColorPicker_OnScroll( byval hScrollBar as HWND, byval newPos as integer )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( GetParent( hScrollBar ) )
    if pClr = 0 then exit sub
    if pClr->nScrollTop = clng(newPos) then exit sub
    pClr->nScrollTop = clng(newPos)
    PsColorPicker_ClampScroll( pClr )
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub


' ========================================================================================
' RE-DERIVE nBodySel FROM THE COLOUR.
'
' Called after every change of colour or tab, so the selection ring is a FUNCTION of the value
' rather than a second copy of it. A colour that is not on the current tab selects nothing (-1),
' which is why picking a shade off the matrix and then switching to Web shows no ring: nothing
' there is that colour, and inventing a nearest match would be the control answering a question
' the user did not ask.
'
' The palette scan asks the host for every entry, so it is O(n) in the palette's size on every
' colour change. Palettes here are the handful of named colours a theme defines; if that ever
' stops being true this is the line to look at first.
' ========================================================================================
sub PsColorPicker_SyncBodySel( byval pClr as PSCOLORPICKER ptr )
    if pClr = 0 then exit sub
    pClr->nBodySel = -1

    dim as long nCount = PsColorPicker_GridCount( pClr )
    for i as long = 0 to nCount - 1
        dim as COLORREF c
        if PsColorPicker_GridColorAt( pClr, i, c ) then
            if c = pClr->clrCurrent then
                pClr->nBodySel = i
                exit sub
            end if
        end if
    next
end sub


' ========================================================================================
' Notify the host that the value moved. USER ACTION ONLY -- every programmatic setter calls
' PsColorPicker_ApplyColor with bNotify FALSE.
' ========================================================================================
private sub PsColorPicker_Notify( byval pClr as PSCOLORPICKER ptr, byval nPhase as long )
    if pClr = 0 then exit sub
    if pClr->ColorChangedCallback = 0 then exit sub
    pClr->ColorChangedCallback( pClr->hWin, pClr->clrCurrent, pClr->nAlphaCurrent, nPhase )
end sub


' ========================================================================================
' THE ONE PLACE clrCurrent AND nAlphaCurrent ARE EVER WRITTEN.
'
' Mouse, keyboard, the entry fields and every host setter come through here, which is what makes
' "the selection ring, the previews and the field text always agree with the value" a property
' of the control rather than of each call site.
'
' A NO-OP IS STILL NOTIFIED WHEN A PHASE IS RUNNING. Dragging across two cells that happen to
' hold the same colour must still emit its MOVE, or a host counting BEGIN/MOVE/END pairs sees an
' unbalanced sequence. Only the SILENT path (bNotify false) early-outs on an unchanged value.
' ========================================================================================
private sub PsColorPicker_ApplyColor( _
            byval pClr    as PSCOLORPICKER ptr, _
            byval clr     as COLORREF, _
            byval nAlpha  as ubyte, _
            byval bNotify as boolean, _
            byval nPhase  as long _
            )

    if pClr = 0 then exit sub

    dim as boolean bSame = ((pClr->clrCurrent = clr) andalso (pClr->nAlphaCurrent = nAlpha))
    if bSame andalso (bNotify = false) then exit sub

    pClr->clrCurrent    = clr
    pClr->nAlphaCurrent = nAlpha
    PsColorPicker_SyncBodySel( pClr )
    pClr->bScrollToSel = true

    ' The edit buffer is re-seeded so a field that is being typed into cannot go on showing the
    ' previous value after a swatch click. If the caret was past the new end it is pulled back.
    if pClr->nFocusField >= 0 then
        select case pClr->nFocusField
        case CLR_FIELD_R : pClr->wszEdit = str( clng(clr) and &hFF )
        case CLR_FIELD_G : pClr->wszEdit = str( (clng(clr) shr 8) and &hFF )
        case CLR_FIELD_B : pClr->wszEdit = str( (clng(clr) shr 16) and &hFF )
        case CLR_FIELD_A : pClr->wszEdit = str( clng(nAlpha) )
        case else        : pClr->wszEdit = PsColorPicker_FormatHex( clr )
        end select
        if pClr->nCaret > len( pClr->wszEdit ) then pClr->nCaret = len( pClr->wszEdit )
        ' The value moved underneath the caret, so what is in the field is a fresh seed again and
        ' the next keystroke should replace it -- not append to a number the user never typed.
        pClr->bEditFresh = true
    end if

    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
    if bNotify then PsColorPicker_Notify( pClr, nPhase )
end sub


' ========================================================================================
' THE FIELDS.
'
' Text for a field is DERIVED from the value, never stored -- except for the single field the
' caret is in, whose in-progress text lives in wszEdit. That is the whole state machine: enter a
' field and wszEdit is seeded, leave it and wszEdit is parsed and discarded.
' ========================================================================================
private function PsColorPicker_FieldText( _
            byval pClr   as PSCOLORPICKER ptr, _
            byval nField as long _
            ) as DWSTRING

    dim as DWSTRING wszOut
    if pClr = 0 then return wszOut

    ' The field being edited shows what the user has typed, INCLUDING a half-finished or empty
    ' value. Showing the committed value instead would make backspacing look like it did
    ' nothing.
    if nField = pClr->nFocusField then
        wszOut = pClr->wszEdit
        return wszOut
    end if

    select case nField
    case CLR_FIELD_R : wszOut = str( clng(pClr->clrCurrent) and &hFF )
    case CLR_FIELD_G : wszOut = str( (clng(pClr->clrCurrent) shr 8) and &hFF )
    case CLR_FIELD_B : wszOut = str( (clng(pClr->clrCurrent) shr 16) and &hFF )
    case CLR_FIELD_A : wszOut = str( clng(pClr->nAlphaCurrent) )
    case CLR_FIELD_HEX : wszOut = PsColorPicker_FormatHex( pClr->clrCurrent )
    end select
    return wszOut
end function


private function PsColorPicker_FieldLabel( byval nField as long ) as DWSTRING
    dim as DWSTRING wszOut
    select case nField
    case CLR_FIELD_R   : wszOut = "R"
    case CLR_FIELD_G   : wszOut = "G"
    case CLR_FIELD_B   : wszOut = "B"
    case CLR_FIELD_A   : wszOut = "A"
    case CLR_FIELD_HEX : wszOut = "#"
    end select
    return wszOut
end function


' Start editing nField: seed the buffer from the value and put the caret at the end. Also starts
' the caret blink, which runs ONLY while a field is focused.
private sub PsColorPicker_BeginEdit( _
            byval pClr   as PSCOLORPICKER ptr, _
            byval nField as long _
            )

    if pClr = 0 then exit sub
    if pClr->IsFieldVisible( nField ) = false then exit sub

    ' Leaving a field commits it. Entering the SAME field again must not, or a click inside the
    ' field you are already editing would clamp a half-typed "2" to 2.
    if pClr->nFocusField <> nField then PsColorPicker_CommitField( pClr )

    pClr->nFocusField = nField
    ' Read AFTER nFocusField is set would return wszEdit itself; read it before, so the seed is
    ' the committed value.
    dim as long nSaved = pClr->nFocusField
    pClr->nFocusField = -1
    pClr->wszEdit = PsColorPicker_FieldText( pClr, nField )
    pClr->nFocusField = nSaved

    pClr->nCaret = len( pClr->wszEdit )
    pClr->bEditFresh = true          ' the next typed character replaces the seed -- see the field
    pClr->bCaretOn = true
    if pClr->caretTimerOn = false then
        dim as long nBlink = GetCaretBlinkTime()
        if nBlink <= 0 then nBlink = 530
        SetTimer( pClr->hWin, IDT_PSCOLORPICKER_CARET, nBlink, NULL )
        pClr->caretTimerOn = true
    end if
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub


' Stop editing: parse the buffer, apply it if it is valid, and notify ONCE with CLR_PHASE_END.
' An unparseable or empty buffer is DISCARDED SILENTLY and the value is left alone -- a picker
' that refused to let you leave a field until you fixed it would be worse than one that quietly
' puts the old number back, and the field is re-seeded so the user can see that it did.
private sub PsColorPicker_CommitField( byval pClr as PSCOLORPICKER ptr )
    if pClr = 0 then exit sub
    if pClr->nFocusField < 0 then exit sub

    dim as long nField = pClr->nFocusField
    dim as COLORREF clrNew = pClr->clrCurrent
    dim as ubyte    nANew  = pClr->nAlphaCurrent
    dim as boolean  bValid = false

    if nField = CLR_FIELD_HEX then
        dim as COLORREF c
        if PsColorPicker_ParseHex( pClr->wszEdit, c ) then
            clrNew = c
            bValid = true
        end if
    else
        dim as string s = trim( pClr->wszEdit )
        if len( s ) > 0 then
            dim as long v = clng( valint( s ) )
            if v < 0 then v = 0
            if v > 255 then v = 255
            dim as long nR = clng(pClr->clrCurrent) and &hFF
            dim as long nG = (clng(pClr->clrCurrent) shr 8) and &hFF
            dim as long nB = (clng(pClr->clrCurrent) shr 16) and &hFF
            select case nField
            case CLR_FIELD_R : nR = v
            case CLR_FIELD_G : nG = v
            case CLR_FIELD_B : nB = v
            case CLR_FIELD_A : nANew = cubyte(v)
            end select
            clrNew = BGR( nR, nG, nB )
            bValid = true
        end if
    end if

    ' Clear the caret BEFORE applying, so ApplyColor's re-seed of wszEdit does not run and leave
    ' a stale buffer behind on a field nobody is in any more.
    pClr->nFocusField = -1
    pClr->wszEdit = ""
    pClr->nCaret = 0

    if bValid then
        dim as boolean bMoved = ((clrNew <> pClr->clrCurrent) orelse (nANew <> pClr->nAlphaCurrent))
        if bMoved then
            PsColorPicker_ApplyColor( pClr, clrNew, nANew, true, CLR_PHASE_END )
        end if
    end if
end sub


' Which field does Tab reach from here? -1 means "off the end", and the WndProc turns that into
' NOT CLAIMING the key, so the host's own tab order takes over. That is the whole reason this is
' a search rather than a wrap.
private function PsColorPicker_NextField( _
            byval pClr   as PSCOLORPICKER ptr, _
            byval nFrom  as long, _
            byval nDelta as long _
            ) as long

    if pClr = 0 then return -1
    dim as long n = nFrom
    do
        n += nDelta
        if (n < 0) orelse (n >= CLR_FIELD_COUNT) then return -1
        if pClr->IsFieldVisible( n ) then return n
    loop
end function


' ========================================================================================
' PAINTING
'
' Nothing in here touches a GDI object or a device context except to MEASURE -- PsBufferPaint
' owns rendering. The one measuring DC is taken for the caret's x offset and only when a field
' actually has the caret, so an idle picker acquires nothing.
'
' THE PRIMITIVES ARE NOT INTERCHANGEABLE:
'   PaintRect          fills.
'   PaintRoundOutline  strokes WITHOUT filling. Every frame here.
'   PaintBorderRect    fills AND THEN strokes -- using it as a frame erases what is underneath.
'                      That is the mistake this family has made four separate times, and
'                      PsColorPicker_CountRenderedTones exists to assert it has not happened.
' ========================================================================================

' Frame a rect without filling it. One helper rather than the two-line pen-then-outline dance at
' the eleven sites that need it.
private sub PsColorPicker_Frame( _
            byval b   as PsBufferPaint ptr, _
            byval rc  as RECT, _
            byval clr as COLORREF, _
            byval nThick as long, _
            byval nCurv  as long _
            )

    if b = 0 then exit sub
    if nThick <= 0 then exit sub
    if (rc.right <= rc.left) orelse (rc.bottom <= rc.top) then exit sub
    b->SetPenColor( clr )
    b->PaintRoundOutline( @rc, nCurv, nThick )
end sub


' The x offset of the caret within a field box, measured in the control's own font. Returns the
' left inset when anything goes wrong, which puts the caret at the start of the text rather than
' leaving it undrawn -- a caret in the wrong place is a cosmetic bug, a missing one looks like
' the field is dead.
private function PsColorPicker_CaretX( _
            byval pClr as PSCOLORPICKER ptr, _
            byval rc   as RECT _
            ) as long

    dim as long nInset = 4
    if pClr = 0 then return rc.left + nInset

    dim as HDC hDC = GetDC( pClr->hWin )
    if hDC = 0 then return rc.left + nInset

    dim as HFONT hFontUse = pClr->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as long nOut = rc.left + nInset
    dim as long nPrefix = pClr->nCaret
    if nPrefix > len( pClr->wszEdit ) then nPrefix = len( pClr->wszEdit )
    if nPrefix > 0 then
        dim as DWSTRING wszPrefix = left( pClr->wszEdit, nPrefix )
        dim as SIZE sz
        if GetTextExtentPoint32W( hDC, wszPrefix.vptr, len( wszPrefix ), @sz ) then
            nOut += sz.cx
        end if
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( pClr->hWin, hDC )

    ' Never past the box: a long hex string in a narrow field would otherwise draw its caret on
    ' whatever sits to the right.
    if nOut > (rc.right - 2) then nOut = rc.right - 2
    return nOut
end function


private sub PsColorPicker_RenderTabs( _
            byval pClr as PSCOLORPICKER ptr, _
            byval b    as PsBufferPaint ptr _
            )

    dim as long nTabs = pClr->TabCount()
    dim as HFONT hFontUse = pClr->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    b->SetFont( hFontUse )

    for i as long = 0 to nTabs - 1
        dim as RECT rc = pClr->rcTab(i)
        if (rc.right <= rc.left) then continue for

        dim as boolean bSel = (i = pClr->nTab)
        dim as boolean bHot = (i = pClr->nHotTab)

        dim as COLORREF clrBack = pClr->colors.TabBackColor
        dim as COLORREF clrFore = pClr->colors.TabForeColor
        if bSel then
            clrBack = pClr->colors.TabBackColorSel
            clrFore = pClr->colors.TabForeColorSel
        elseif bHot then
            clrBack = pClr->colors.TabBackColorSel
        end if
        if pClr->isEnabled = false then clrFore = pClr->colors.TabForeColor

        b->SetBackColor( clrBack )
        b->PaintRect( @rc )

        dim as DWSTRING wszCaption
        select case i
        case CLR_TAB_WEB     : wszCaption = "Web"
        case CLR_TAB_SYSTEM  : wszCaption = "System"
        case CLR_TAB_CUSTOM  : wszCaption = "Custom"
        case CLR_TAB_PALETTE : wszCaption = "Palette"
        end select

        b->SetColors( clrFore, clrBack )
        b->PaintText( wszCaption, @rc, DT_CENTER or DT_END_ELLIPSIS )

        ' The selected tab is marked by a rule UNDER the word, PsSelectBar's look. Drawn as a
        ' filled rect rather than a pen line because it is a block of colour, not a hairline --
        ' so it scales with DPI here, where a pen would be scaled by PaintLine instead.
        if bSel then
            dim as RECT rcInd = rc
            rcInd.top = rcInd.bottom - (2 * pClr->nBorderThick)
            b->SetBackColor( pClr->colors.SelRingColor )
            b->PaintRect( @rcInd )
        end if
    next
end sub


' Only the MATRIX renders through here now -- the Web, System and Palette tabs are lists and go
' through PsColorPicker_RenderList. It does the same arithmetic PsColorPicker_CellFromPoint does,
' which is what stops the painter and the hit test disagreeing about where a cell is: a
' disagreement there produces "clicking one swatch rings its neighbour", which looks fine in a
' screenshot because both halves are individually right.
private sub PsColorPicker_RenderMatrix( _
            byval pClr as PSCOLORPICKER ptr, _
            byval b    as PsBufferPaint ptr _
            )

    dim as RECT rcGrid = pClr->rcMatrix
    dim as long nCellW = pClr->nCellW
    dim as long nCellH = pClr->nCellH
    dim as long nCols  = pClr->nMatrixCols
    if (nCols <= 0) orelse (nCellW <= 0) orelse (nCellH <= 0) then exit sub
    if (rcGrid.right <= rcGrid.left) orelse (rcGrid.bottom <= rcGrid.top) then exit sub

    dim as long nCount = PsColorPicker_GridCount( pClr )
    for i as long = 0 to nCount - 1
        dim as long c = i mod nCols
        dim as long r = i \ nCols

        dim as RECT rc
        SetRect( @rc, rcGrid.left + (c * nCellW), rcGrid.top + (r * nCellH), _
                      rcGrid.left + ((c + 1) * nCellW), rcGrid.top + ((r + 1) * nCellH) )
        ' CLIP RATHER THAN SCALE. A control sized smaller than ideal drops the cells that do not
        ' fit; it does not shrink them, because a half-width swatch is not a smaller swatch. The
        ' hit test applies the identical test -- see PsColorPicker_HitBody.
        if rc.bottom > rcGrid.bottom then exit for
        if rc.right > rcGrid.right then continue for

        dim as COLORREF clrCell
        if PsColorPicker_GridColorAt( pClr, i, clrCell ) = false then continue for

        b->SetBackColor( clrCell )
        b->PaintRect( @rc )

        dim as boolean bSel = (i = pClr->nBodySel)
        dim as boolean bHot = (i = pClr->nHotSwatch)
        if bSel orelse bHot then
            dim as RECT rcRing = rc
            InflateRect( @rcRing, -PSCOLORPICKER_RINGINSET, -PSCOLORPICKER_RINGINSET )
            dim as COLORREF clrRing = pClr->colors.HotRingColor
            ' Selection wins over hover: the ring under the cursor is transient and the one on
            ' the chosen colour is the answer, so on the cell that is both, the answer shows.
            if bSel then clrRing = pClr->colors.SelRingColor
            PsColorPicker_Frame( b, rcRing, clrRing, 2 * pClr->nBorderThick, 0 )
        end if
    next
end sub


' ========================================================================================
' THE LIST.  Web, System and Palette all render through here: one row per entry, a swatch on the
' left and the entry's NAME beside it, scrolled by whole rows.
'
' ONE RENDERER FOR THREE TABS, and that is the point rather than an economy. They differ only in
' where the rows come from -- PsColorPicker_GridColorAt and _GridNameAt already answer that -- so
' three renderers would be three chances for the Web tab to drift from the Palette tab in row
' height, swatch inset or highlight colour, and no assertion would notice, because each would be
' internally consistent.
'
' A NAMED COLOUR WITHOUT ITS NAME IS JUST A SWATCH. The first version of this control drew Web and
' System as 6x8 grids of anonymous squares: it fitted the panel neatly and told the user nothing,
' which is why the list format replaced it.
'
' SCROLLING IS BY WHOLE ROWS (nScrollTop is an index, not a pixel offset), so a half-row can never
' sit at the top of the viewport -- and the hit test stays a plain division.
' ========================================================================================
private sub PsColorPicker_RenderList( _
            byval pClr as PSCOLORPICKER ptr, _
            byval b    as PsBufferPaint ptr _
            )

    dim as RECT rcList = pClr->rcSwatches
    if (rcList.bottom <= rcList.top) orelse (rcList.right <= rcList.left) then exit sub
    if pClr->nRowH <= 0 then exit sub

    dim as HFONT hFontUse = pClr->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    b->SetFont( hFontUse )

    dim as long nCount = PsColorPicker_GridCount( pClr )
    dim as long nVis   = PsColorPicker_VisibleRows( pClr )

    for v as long = 0 to nVis - 1
        dim as long i = pClr->nScrollTop + v
        if i >= nCount then exit for

        dim as RECT rcRow
        SetRect( @rcRow, rcList.left, rcList.top + (v * pClr->nRowH), _
                         rcList.right, rcList.top + ((v + 1) * pClr->nRowH) )
        if rcRow.bottom > rcList.bottom then exit for

        dim as COLORREF clrEntry
        if PsColorPicker_GridColorAt( pClr, i, clrEntry ) = false then continue for
        dim as DWSTRING wszName
        PsColorPicker_GridNameAt( pClr, i, wszName )

        dim as boolean bSel = (i = pClr->nBodySel)
        dim as boolean bHot = (i = pClr->nHotSwatch)

        ' The SELECTED row is a filled highlight with its own text colour -- the listbox look --
        ' while a merely hot row gets the softer hover fill. Selection wins, as it does in the
        ' matrix. The highlight's text colour is DERIVED from the highlight rather than being a
        ' fifteenth colour field: a host that themes SelRingColor to something pale would
        ' otherwise get white-on-pale and no warning.
        dim as COLORREF clrRowBack = pClr->colors.BackColor
        dim as COLORREF clrRowFore = pClr->colors.ForeColor
        if bSel then
            clrRowBack = pClr->colors.SelRingColor
            clrRowFore = PsColorPicker_ContrastColor( pClr->colors.SelRingColor )
        elseif bHot then
            clrRowBack = pClr->colors.TabBackColorSel
            clrRowFore = pClr->colors.TabForeColorSel
        end if
        b->SetBackColor( clrRowBack )
        b->PaintRect( @rcRow )

        dim as RECT rcSw
        SetRect( @rcSw, rcRow.left + 3, rcRow.top + 3, rcRow.left + pClr->nSwatchW, rcRow.bottom - 3 )
        if rcSw.right > rcRow.right then rcSw.right = rcRow.right
        if rcSw.right > rcSw.left then
            b->SetBackColor( clrEntry )
            b->PaintRect( @rcSw )
            ' A framed swatch, because an entry whose colour matches the panel behind it would
            ' otherwise be an invisible row with a name floating beside nothing. White and Black
            ' are both in the web table, so this is the normal case rather than a corner one.
            PsColorPicker_Frame( b, rcSw, pClr->colors.BorderColor, pClr->nBorderThick, 0 )
        end if

        dim as RECT rcName = rcRow
        rcName.left = rcSw.right + pClr->nGap
        if rcName.left < rcName.right then
            b->SetColors( clrRowFore, clrRowBack )
            b->PaintText( wszName, @rcName, DT_LEFT or DT_END_ELLIPSIS )
        end if
    next

    ' The Assign button -- palette tab only. Reserved unconditionally by the layout, and drawn
    ' looking disabled when there is nothing selected to assign rather than hidden, because a
    ' button that appears and disappears as you click around the list reads as a glitch.
    if IsRectEmpty( @pClr->rcAssign ) = 0 then
        dim as boolean bArmed = (pClr->nBodySel >= 0)
        dim as COLORREF clrBack = pClr->colors.FieldBackColor
        if pClr->bHotAssign andalso bArmed then clrBack = pClr->colors.TabBackColorSel
        dim as COLORREF clrFore = pClr->colors.ForeColor
        if bArmed = false then clrFore = pClr->colors.TabForeColor

        dim as RECT rcA = pClr->rcAssign
        b->SetBackColor( clrBack )
        b->PaintRect( @rcA )
        PsColorPicker_Frame( b, rcA, pClr->colors.FieldBorderColor, pClr->nBorderThick, pClr->nCurvature )
        b->SetColors( clrFore, clrBack )
        b->PaintText( pClr->wszAssign, @rcA, DT_CENTER or DT_END_ELLIPSIS )
    end if
end sub


private sub PsColorPicker_RenderPreviews( _
            byval pClr as PSCOLORPICKER ptr, _
            byval b    as PsBufferPaint ptr _
            )

    ' Empty rect = selection-only mode, where there is no preview pair to draw. Gated on the
    ' RECT rather than on the flag so the painter follows the layout by construction.
    if IsRectEmpty( @pClr->rcInitial ) then exit sub

    dim as HFONT hFontUse = pClr->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    b->SetFont( hFontUse )

    dim as RECT rcI = pClr->rcInitial
    b->SetBackColor( pClr->clrInitial )
    b->PaintRect( @rcI )
    ' The label goes INSIDE the box in whichever of black/white is legible on it -- there is no
    ' room beside the previews, and two unlabelled rectangles do not tell you which is which.
    b->SetColors( PsColorPicker_ContrastColor( pClr->clrInitial ), pClr->clrInitial )
    b->PaintText( pClr->wszInitialLabel, @rcI, DT_CENTER or DT_END_ELLIPSIS )

    dim as RECT rcC = pClr->rcCurrent
    b->SetBackColor( pClr->clrCurrent )
    b->PaintRect( @rcC )
    b->SetColors( PsColorPicker_ContrastColor( pClr->clrCurrent ), pClr->clrCurrent )
    b->PaintText( pClr->wszCurrentLabel, @rcC, DT_CENTER or DT_END_ELLIPSIS )

    ' ONE frame around the PAIR, not two around the boxes: they are two readings of one value and
    ' a shared border is what says so.
    dim as RECT rcBoth = pClr->rcInitial
    rcBoth.bottom = pClr->rcCurrent.bottom
    PsColorPicker_Frame( b, rcBoth, pClr->colors.BorderColor, pClr->nBorderThick, 0 )
end sub


private sub PsColorPicker_RenderFields( _
            byval pClr as PSCOLORPICKER ptr, _
            byval b    as PsBufferPaint ptr _
            )

    dim as HFONT hFontUse = pClr->hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    b->SetFont( hFontUse )

    for i as long = 0 to CLR_FIELD_COUNT - 1
        if pClr->IsFieldVisible( i ) = false then continue for
        dim as RECT rcBox = pClr->rcFieldBox(i)
        if IsRectEmpty( @rcBox ) then continue for

        dim as RECT rcLab = pClr->rcFieldLabel(i)
        b->SetColors( pClr->colors.ForeColor, pClr->colors.BackColor )
        b->PaintText( PsColorPicker_FieldLabel( i ), @rcLab, DT_LEFT )

        b->SetBackColor( pClr->colors.FieldBackColor )
        b->PaintRect( @rcBox )

        dim as boolean bFocusedField = (i = pClr->nFocusField)
        dim as COLORREF clrBorder = pClr->colors.FieldBorderColor
        if bFocusedField then clrBorder = pClr->colors.FieldBorderColorSel
        PsColorPicker_Frame( b, rcBox, clrBorder, pClr->nBorderThick, pClr->nCurvature )

        dim as RECT rcText = rcBox
        rcText.left += 4
        rcText.right -= 4
        b->SetColors( pClr->colors.FieldForeColor, pClr->colors.FieldBackColor )
        b->PaintText( PsColorPicker_FieldText( pClr, i ), @rcText, DT_LEFT or DT_END_ELLIPSIS )

        ' The caret. Drawn as a filled 1px-wide rect rather than through PaintLine so it is not
        ' DPI-scaled twice -- a caret is a hairline and PaintLine would scale the pen.
        dim as boolean bShowCaret = (bFocusedField andalso pClr->bCaretOn andalso pClr->isFocused)
        if bShowCaret then
            dim as long cx = PsColorPicker_CaretX( pClr, rcText )
            dim as RECT rcCaret
            SetRect( @rcCaret, cx, rcBox.top + 3, cx + pClr->nBorderThick, rcBox.bottom - 3 )
            b->SetBackColor( pClr->colors.FieldForeColor )
            b->PaintRect( @rcCaret )
        end if
    next
end sub


' ========================================================================================
' Draw the whole control.
'
' The BACKGROUND is not drawn here -- PsColorPicker_OnPaint has already flooded the client with
' BackColor, which is what lets a host paint callback that only wants to add a foreground still
' sit on the right colour.
' ========================================================================================
sub PsColorPicker_RenderDefault( _
            byval pClr as PSCOLORPICKER ptr, _
            byval b    as PsBufferPaint ptr _
            )

    if (pClr = 0) orelse (b = 0) then exit sub
    if IsRectEmpty( @pClr->rcContent ) then exit sub

    PsColorPicker_RenderTabs( pClr, b )

    if PsColorPicker_IsListTab( pClr ) then
        PsColorPicker_RenderList( pClr, b )
    else
        PsColorPicker_RenderMatrix( pClr, b )
    end if

    PsColorPicker_RenderPreviews( pClr, b )
    PsColorPicker_RenderFields( pClr, b )

    ' THE CONTROL-LEVEL FOCUS RING, and only when no field has the caret. With a field focused
    ' its own accent border already says where the keyboard is going, and drawing both would make
    ' the picker look like it had focus twice.
    dim as boolean bRing = (pClr->isFocused andalso (pClr->nFocusField < 0))
    if bRing andalso (pClr->nFocusThick > 0) then
        dim as RECT rcRing = pClr->rcContent
        InflateRect( @rcRing, pClr->nFocusThick, pClr->nFocusThick )
        PsColorPicker_Frame( b, rcRing, pClr->colors.FocusRingColor, pClr->nFocusThick, pClr->nCurvature )
    end if
end sub


' ========================================================================================
' Fill a PAINTINFO from the control's current state and geometry. One place, so the WM_PAINT
' path and the two offscreen probes cannot describe the control differently.
' ========================================================================================
private sub PsColorPicker_FillPaintInfo( _
            byval pClr as PSCOLORPICKER ptr, _
            byval hwnd as HWND, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSCOLORPICKER_PAINTINFO ptr _
            )

    if (pClr = 0) orelse (p = 0) then exit sub

    p->hColorPicker = hwnd
    p->b            = b
    GetClientRect( hwnd, @p->rcClient )
    p->rcContent     = pClr->rcContent
    p->rcTabs        = pClr->rcTabs
    p->rcBody        = pClr->rcBody
    p->rcMatrix      = pClr->rcMatrix
    p->rcSwatches    = pClr->rcSwatches
    p->rcAssign      = pClr->rcAssign
    p->rcInitial     = pClr->rcInitial
    p->rcCurrent     = pClr->rcCurrent
    p->rcFields      = pClr->rcFields
    p->rcAlpha       = pClr->rcAlpha
    p->clrCurrent    = pClr->clrCurrent
    p->nAlphaCurrent = pClr->nAlphaCurrent
    p->clrInitial    = pClr->clrInitial
    p->nAlphaInitial = pClr->nAlphaInitial
    p->nTab          = pClr->nTab
    p->isEnabled     = pClr->isEnabled
    p->isFocused     = pClr->isFocused
    p->isDragging    = pClr->isDragging
    p->nFocusField   = pClr->nFocusField
end sub


private function PsColorPicker_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hwnd )
    if pClr = null then return 0

    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    ' Both of these depend on the tab's row count and on the viewport height, which the layout
    ' has only just settled -- so they run here rather than inside LayoutColorPicker, which lives
    ' in the .bi and cannot see PsColorPicker_GridCount.
    if pClr->bScrollToSel then
        pClr->bScrollToSel = false
        PsColorPicker_EnsureSelVisible( pClr )
    end if
    PsColorPicker_SyncScrollBar( pClr )

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )

    b.SetBackColor( pClr->colors.BackColor )
    b.PaintClientRect()

    if pClr->PaintCallback then
        dim p as PSCOLORPICKER_PAINTINFO
        PsColorPicker_FillPaintInfo( pClr, hwnd, @b, @p )
        pClr->PaintCallback( @p )
    else
        PsColorPicker_RenderDefault( pClr, @b )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' HIT TESTS.  All take CLIENT coordinates and all route through the same geometry the painter
' uses -- PsColorPicker_CellFromPoint for the grids, the stored rects for everything else.
' ========================================================================================

private function PsColorPicker_HitTab( byval pClr as PSCOLORPICKER ptr, byval x as long, byval y as long ) as long
    if pClr = 0 then return -1
    dim as long nTabs = pClr->TabCount()
    for i as long = 0 to nTabs - 1
        if PsColorPicker_PtIn( pClr->rcTab(i), x, y ) then return i
    next
    return -1
end function


private function PsColorPicker_HitField( byval pClr as PSCOLORPICKER ptr, byval x as long, byval y as long ) as long
    if pClr = 0 then return -1
    for i as long = 0 to CLR_FIELD_COUNT - 1
        if pClr->IsFieldVisible( i ) = false then continue for
        if PsColorPicker_PtIn( pClr->rcFieldBox(i), x, y ) then return i
    next
    return -1
end function


' The index of the body cell under the point, or -1. The three grid tabs go through
' CellFromPoint; the palette is a list, so its "cell" is a row.
private function PsColorPicker_HitBody( byval pClr as PSCOLORPICKER ptr, byval x as long, byval y as long ) as long
    if pClr = 0 then return -1
    dim as long nCount = PsColorPicker_GridCount( pClr )
    if nCount <= 0 then return -1

    ' ---- the three LIST tabs: a row lookup, offset by the scroll position -----------------
    if PsColorPicker_IsListTab( pClr ) then
        if PsColorPicker_PtIn( pClr->rcSwatches, x, y ) = false then return -1
        if pClr->nRowH <= 0 then return -1

        dim as long v = (y - pClr->rcSwatches.top) \ pClr->nRowH
        if v < 0 then return -1
        ' A row that would be drawn past the bottom of the viewport is not clickable either.
        ' Without this the last, half-visible row answers to a click on a strip of background
        ' that shows nothing -- and with a 137-row list there is almost always such a strip.
        if (pClr->rcSwatches.top + ((v + 1) * pClr->nRowH)) > pClr->rcSwatches.bottom then return -1

        ' nScrollTop is an INDEX, not a pixel offset, which is what keeps this a plain division.
        dim as long r = pClr->nScrollTop + v
        if (r < 0) orelse (r >= nCount) then return -1
        return r
    end if

    ' ---- the matrix ------------------------------------------------------------------------
    dim as RECT rcGrid = pClr->rcMatrix
    dim as long nCellW = pClr->nCellW
    dim as long nCellH = pClr->nCellH
    dim as long nCols  = pClr->nMatrixCols
    dim as long nRows  = pClr->nMatrixRows

    dim as long c, r
    if PsColorPicker_CellFromPoint( rcGrid, nCellW, nCellH, nCols, nRows, x, y, c, r ) = false then return -1

    ' A CELL THAT DOES NOT FIT ENTIRELY IS NOT CLICKABLE, because PsColorPicker_RenderMatrix does
    ' not draw it -- it clips a cell whose right or bottom edge falls outside the grid rather
    ' than drawing a partial one. The two tests are deliberately identical.
    '
    ' THIS IS WHERE THE HIT TEST AND THE PAINTER WOULD SILENTLY DISAGREE. It only bites when the
    ' host sizes the control NARROWER than ideal, which is exactly the case nobody screenshots:
    ' the right-hand column of swatches vanishes from the render, and without this a click on the
    ' blank strip they used to occupy still picks them. Found by A/B -- the guard that stood here
    ' before was a PtIn bound on the grid rect, which turned out to be provably redundant with
    ' CellFromPoint's own index bounds AND to miss this case entirely, so it passed every
    ' assertion while protecting nothing.
    dim as long cellR = rcGrid.left + ((c + 1) * nCellW)
    dim as long cellB = rcGrid.top  + ((r + 1) * nCellH)
    if (cellR > rcGrid.right) orelse (cellB > rcGrid.bottom) then return -1

    dim as long idx = (r * nCols) + c
    if idx >= nCount then return -1
    return idx
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if something changed.
'
' NOTE WHAT IT DOES NOT TOUCH: nBodySel, nFocusField and the caret. The family's hover safety
' net assumes hover is the only writer of the highlight, and a control with keyboard-driven
' selection on top of that pattern gets its selection erased ~100ms after every keypress -- the
' trap recorded in Learnings.md against PsMenuBar. Keeping hover state and selection state
' disjoint is what makes this control safe, and the self-test asserts it.
' ========================================================================================
private sub PsColorPicker_ClearHot( byval pClr as PSCOLORPICKER ptr, byval hwnd as HWND )
    if pClr = 0 then exit sub
    if pClr->hotTimerOn then
        KillTimer( hwnd, IDT_PSCOLORPICKER_HOTTRACK )
        pClr->hotTimerOn = false
    end if
    ' Hoisted rather than chained inline: mixing a comparison result with a boolean field in one
    ' orelse chain trips fbc's warning 38, which is a real rule and not noise (C:\dev\Learnings.md).
    dim as boolean bHotAny = ((pClr->nHotTab >= 0) orelse (pClr->nHotSwatch >= 0) orelse _
                              (pClr->nHotField >= 0))
    dim as boolean bAny = (bHotAny orelse pClr->bHotAssign)
    if bAny then
        pClr->nHotTab    = -1
        pClr->nHotSwatch = -1
        pClr->nHotField  = -1
        pClr->bHotAssign = false
        InvalidateRect( hwnd, NULL, FALSE )
    end if
end sub


' Move the body selection by one cell and adopt that cell's colour. Used by the arrow keys, which
' are the only way to drive this control without a mouse.
'
' It starts from cell 0 when nothing is selected, which is what makes the arrows usable after the
' host has set a colour that is not on the current tab -- otherwise the first press would have
' nothing to move from and the keyboard would appear dead.
private sub PsColorPicker_NavigateBody( byval pClr as PSCOLORPICKER ptr, byval nDelta as long )
    if pClr = 0 then exit sub
    if pClr->isEnabled = false then exit sub

    dim as long nCount = PsColorPicker_GridCount( pClr )
    if nCount <= 0 then exit sub

    dim as long idx = pClr->nBodySel
    if idx < 0 then
        idx = 0
    else
        idx += nDelta
        ' CLAMP, NOT WRAP. Wrapping a two-dimensional grid at the row ends is disorienting -- the
        ' cursor would jump from the right edge of one row to the left edge of the next on a
        ' RIGHT press, which is right for text and wrong for a colour grid.
        if idx < 0 then idx = 0
        if idx > (nCount - 1) then idx = nCount - 1
    end if

    dim as COLORREF c
    if PsColorPicker_GridColorAt( pClr, idx, c ) = false then exit sub
    ' Notified as a single END: an arrow press is a settled choice, not a drag.
    PsColorPicker_ApplyColor( pClr, c, pClr->nAlphaCurrent, true, CLR_PHASE_END )
    ' Re-asserted because SyncBodySel derives the selection from the COLOUR, and two cells of a
    ' grid may legitimately hold the same one (the greyscale column's ends, a palette with a
    ' duplicate). Without this, arrowing onto the second of a duplicate pair would snap the ring
    ' back to the first and the next press would go nowhere.
    pClr->nBodySel = idx
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub


' How far Up/Down moves: a whole row of matrix cells, or a single list row.
private function PsColorPicker_NavRowStep( byval pClr as PSCOLORPICKER ptr ) as long
    if pClr = 0 then return 1
    if PsColorPicker_IsListTab( pClr ) then return 1
    if pClr->nMatrixCols < 1 then return 1
    return pClr->nMatrixCols
end function


' ========================================================================================
' PsColorPicker Window procedure
'
' This control has NO CHILD WINDOWS -- that is the entire point of the design -- so there is
' nothing to subclass: every message is handled here.
'
' MOUSE CAPTURE IS TAKEN FOR ONE GESTURE, the matrix drag, and the family's test comes out
' positive for it: dragging consumes the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing. The
' full price is paid -- release on the up before any callback runs, WM_CAPTURECHANGED cancels,
' WM_DESTROY releases, and the callback's result is ignored for the up-message. Tabs, swatches,
' fields and the Assign button take none: they act on a single message and have no pairing to
' protect.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET. A rapid second click on a swatch is a legitimate re-pick,
' and on the matrix it would swallow the down that starts the second of two deliberate drags.
' ========================================================================================
function PsColorPicker_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsColorPicker_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer.
        return true

    case WM_SIZE
        if pClr then
            pClr->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_TIMER
        if pClr = 0 then exit select
        if wParam = IDT_PSCOLORPICKER_HOTTRACK then
            ' NOT while a drag is live: during capture the cursor is legitimately allowed to
            ' wander off, and WM_MOUSEMOVE is already maintaining the hover state for that.
            if pClr->isDragging = false then
                dim as RECT rcClient
                GetClientRect( hwnd, @rcClient )
                if isMouseOverRECT( hwnd, rcClient ) = false then
                    PsColorPicker_ClearHot( pClr, hwnd )
                end if
            end if
            return 0
        end if
        if wParam = IDT_PSCOLORPICKER_CARET then
            pClr->bCaretOn = (pClr->bCaretOn = false)
            ' Invalidate ONLY the field box. A whole-control invalidate twice a second would
            ' repaint 192 matrix cells for a two-pixel caret, which is exactly the kind of thing
            ' that reads as "the picker flickers".
            if pClr->nFocusField >= 0 then
                dim as RECT rc = pClr->rcFieldBox( pClr->nFocusField )
                InvalidateRect( hwnd, @rc, FALSE )
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pClr = 0 then exit select
        if pClr->isEnabled = false then exit select

        dim as long mx = cast( short, loword(lParam) )
        dim as long my = cast( short, hiword(lParam) )

        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        if isMouseOverRECT( hwnd, rcClient ) then
            dim tme as TrackMouseEvent
            tme.cbSize    = sizeof(TrackMouseEvent)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            if pClr->hotTimerOn = false then
                SetTimer( hwnd, IDT_PSCOLORPICKER_HOTTRACK, PSCOLORPICKER_HOTTRACK_MS, NULL )
                pClr->hotTimerOn = true
            end if
        end if

        if pClr->isDragging then
            ' A drag that has left the grid CLAMPS to the nearest cell rather than stopping, so
            ' sweeping off the right edge keeps picking down the row instead of freezing. That is
            ' what capture is for.
            dim as long cx = mx
            dim as long cy = my
            if cx < pClr->rcMatrix.left then cx = pClr->rcMatrix.left
            if cx > (pClr->rcMatrix.right - 1) then cx = pClr->rcMatrix.right - 1
            if cy < pClr->rcMatrix.top then cy = pClr->rcMatrix.top
            if cy > (pClr->rcMatrix.bottom - 1) then cy = pClr->rcMatrix.bottom - 1
            dim as long idx = PsColorPicker_HitBody( pClr, cx, cy )
            if idx >= 0 then
                dim as COLORREF c
                if PsColorPicker_GridColorAt( pClr, idx, c ) then
                    PsColorPicker_ApplyColor( pClr, c, pClr->nAlphaCurrent, true, CLR_PHASE_MOVE )
                end if
            end if
            if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0
            return 0
        end if

        dim as long nTab    = PsColorPicker_HitTab( pClr, mx, my )
        dim as long nSwatch = PsColorPicker_HitBody( pClr, mx, my )
        dim as long nField  = PsColorPicker_HitField( pClr, mx, my )
        dim as boolean bAssign = false
        if IsRectEmpty( @pClr->rcAssign ) = 0 then
            bAssign = PsColorPicker_PtIn( pClr->rcAssign, mx, my )
        end if

        dim as boolean bChanged = ((nTab <> pClr->nHotTab) orelse (nSwatch <> pClr->nHotSwatch) orelse _
                                   (nField <> pClr->nHotField))
        dim as boolean bAssignChanged = (bAssign <> pClr->bHotAssign)
        if bChanged orelse bAssignChanged then
            pClr->nHotTab    = nTab
            pClr->nHotSwatch = nSwatch
            pClr->nHotField  = nField
            pClr->bHotAssign = bAssign
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSEWHEEL
        ' Routed into the list's scrollbar by hand. Wheel messages go to the FOCUSED window, and
        ' PsVScrollBar never takes focus -- so without this the wheel would do nothing over a
        ' 137-row list. PsVScrollBar_HandleWheelDelta is public for exactly this purpose, and it
        ' owns the sub-notch accumulation that makes a precision touchpad scroll smoothly.
        if pClr = 0 then exit select
        if pClr->isEnabled = false then return 0
        if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0
        dim as boolean bHaveBar = (IsWindow( pClr->hScroll ) <> 0)
        if PsColorPicker_IsListTab( pClr ) andalso bHaveBar then
            if PsVScrollBar_HandleWheelDelta( pClr->hScroll, cast( short, hiword(wParam) ) ) then
                return 0
            end if
        end if

    case WM_MOUSELEAVE
        if pClr then
            if pClr->isDragging = false then PsColorPicker_ClearHot( pClr, hwnd )
            if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETCURSOR
        ' An I-beam over the entry fields, so they read as typeable. Only when the cursor is
        ' genuinely over one of our own fields -- LOWORD(lParam) is the hit-test code and is
        ' HTCLIENT for the client area.
        if pClr = 0 then exit select
        if loword(lParam) <> HTCLIENT then exit select
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        if PsColorPicker_HitField( pClr, pt.x, pt.y ) >= 0 then
            SetCursor( LoadCursor( NULL, IDC_IBEAM ) )
            return TRUE
        end if

    case WM_LBUTTONDOWN
        if pClr = 0 then return 0
        if pClr->isEnabled = false then return 0

        dim as long mx = cast( short, loword(lParam) )
        dim as long my = cast( short, hiword(lParam) )

        ' Focus first and unconditionally: clicking a control focuses it. This runs WM_SETFOCUS
        ' synchronously -- which is why the field dispatch below happens AFTER it, and why a
        ' click on the matrix ends with no field focused even though WM_SETFOCUS seeds one.
        SetFocus( hwnd )

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0

        dim as long nField = PsColorPicker_HitField( pClr, mx, my )
        if nField >= 0 then
            PsColorPicker_BeginEdit( pClr, nField )
            return 0
        end if

        ' Anything else takes the caret out of a field, and COMMITS what was typed there. A click
        ' elsewhere is not a cancel.
        PsColorPicker_CommitField( pClr )

        dim as long nTab = PsColorPicker_HitTab( pClr, mx, my )
        if nTab >= 0 then
            if nTab <> pClr->nTab then
                pClr->nTab = nTab
                ' A new tab starts at the top, then reveals its selection if it has one. Carrying
                ' the old tab's scroll position across would land a 5-row palette on row 106.
                pClr->nScrollTop = 0
                pClr->bScrollToSel = true
                PsColorPicker_SyncBodySel( pClr )
                pClr->Refresh()
                if pClr->TabChangedCallback then pClr->TabChangedCallback( hwnd, nTab )
            end if
            return 0
        end if

        dim as boolean bAssignHit = false
        if IsRectEmpty( @pClr->rcAssign ) = 0 then
            bAssignHit = PsColorPicker_PtIn( pClr->rcAssign, mx, my )
        end if
        if bAssignHit then
            ' Reported, never acted on: the control does not know what "assign" means to the
            ' host, only that the user asked for it.
            if (pClr->nBodySel >= 0) andalso (pClr->PaletteAssignCallback <> 0) then
                pClr->PaletteAssignCallback( hwnd, pClr->nBodySel )
            end if
            return 0
        end if

        dim as long idx = PsColorPicker_HitBody( pClr, mx, my )
        if idx >= 0 then
            dim as COLORREF c
            if PsColorPicker_GridColorAt( pClr, idx, c ) then
                if pClr->nTab = CLR_TAB_CUSTOM then
                    ' THE ONLY GESTURE THAT TAKES CAPTURE. BEGIN now; MOVEs while the button is
                    ' held; END on the release.
                    SetCapture( hwnd )
                    pClr->isDragging = true
                    PsColorPicker_ApplyColor( pClr, c, pClr->nAlphaCurrent, true, CLR_PHASE_BEGIN )
                else
                    PsColorPicker_ApplyColor( pClr, c, pClr->nAlphaCurrent, true, CLR_PHASE_END )
                end if
                pClr->nBodySel = idx      ' see PsColorPicker_NavigateBody for why
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            return 0
        end if

        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_LBUTTONUP
        if pClr = 0 then return 0

        ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window losing
        ' capture SYNCHRONOUSLY, and our handler for it cancels the drag -- so reading isDragging
        ' after the release would always find FALSE and the END would never be sent.
        dim as boolean bWasDragging = pClr->isDragging
        if GetCapture() = hwnd then ReleaseCapture()
        pClr->CancelDrag()

        ' The result is deliberately IGNORED here (see CLR_MessageCallbackFunc).
        PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam )

        if bWasDragging then
            PsColorPicker_Notify( pClr, CLR_PHASE_END )
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell). The
        ' gesture is over.
        '
        ' NO END IS SENT. A host that received BEGIN and then nothing has had its drag ABANDONED,
        ' which is different from finished, and inventing an END here would tell it the user
        ' settled on a colour they were merely passing over. Hosts that must balance the phases
        ' should treat a stolen capture the same way they treat a Cancel.
        if pClr then
            pClr->CancelDrag()
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business. No capture is taken
        ' for the right button, so an up can arrive without a down.
        if pClr then
            if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS
        if pClr then
            pClr->isFocused = true
            ' Arriving by Tab puts the caret in the first visible field, which is what makes the
            ' picker keyboard-usable at all. A CLICK also comes through here -- WM_LBUTTONDOWN
            ' calls SetFocus first -- and then clears or re-targets the field itself, so the two
            ' routes do not fight.
            if pClr->nFocusField < 0 then PsColorPicker_BeginEdit( pClr, CLR_FIELD_R )
            InvalidateRect( hwnd, NULL, FALSE )
            ' Result IGNORED: focus is a fact the system reports, not an action to veto.
            PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_KILLFOCUS
        if pClr then
            pClr->isFocused = false
            ' COMMIT ON THE WAY OUT. This is the path that saves a user who typed a value and
            ' then clicked the host's OK button without pressing Enter -- and it is why the
            ' callback's result is ignored here, since suppressing it would silently discard
            ' what they typed.
            PsColorPicker_CommitField( pClr )
            if pClr->caretTimerOn then
                KillTimer( hwnd, IDT_PSCOLORPICKER_CARET )
                pClr->caretTimerOn = false
            end if
            pClr->bCaretOn = false
            InvalidateRect( hwnd, NULL, FALSE )
            PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the keys we actually consume, and only for the specific message being asked
        ' about (lParam points at the MSG when it does). Claiming unconditionally would swallow
        ' Tab and break the navigation the tabstop opted into.
        '
        ' TAB IS CLAIMED ONLY WHEN THERE IS A NEXT FIELD TO GO TO. That single condition is what
        ' makes the picker a well-behaved member of a dialog: Tab walks R -> G -> B -> (A) ->
        ' Hex and then LEAVES, because at Hex there is no next field and the key is not claimed.
        '
        ' ESCAPE IS DELIBERATELY NOT CLAIMED. An uncommitted edit is worth less than a working
        ' Cancel key, and a host dialog that could not be dismissed because the caret happened to
        ' be in a field would be the worse bug. Enter IS claimed, but only while a field has the
        ' caret, so it commits the number rather than firing the default button out from under a
        ' half-typed value.
        if pClr = 0 then return 0
        if lParam = 0 then return 0
        dim as MSG ptr pMsg = cast( MSG ptr, lParam )
        if pMsg->message = WM_CHAR then
            if pClr->nFocusField >= 0 then return DLGC_WANTCHARS or DLGC_WANTALLKEYS
            return 0
        end if
        if pMsg->message <> WM_KEYDOWN then return 0
        select case pMsg->wParam
        case VK_TAB
            if pClr->nFocusField < 0 then return 0
            dim as long nDelta = 1
            if (GetKeyState( VK_SHIFT ) and &h8000) <> 0 then nDelta = -1
            if PsColorPicker_NextField( pClr, pClr->nFocusField, nDelta ) < 0 then return 0
            return DLGC_WANTALLKEYS
        case VK_RETURN
            if pClr->nFocusField < 0 then return 0
            return DLGC_WANTALLKEYS
        case VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END, VK_BACK, VK_DELETE
            return DLGC_WANTALLKEYS
        end select
        return 0

    case WM_KEYDOWN
        if pClr = 0 then exit select
        if pClr->isEnabled = false then return 0
        ' The host sees the key BEFORE it edits anything, so navigation can be vetoed.
        if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0

        ' ---- with the caret in a field: text editing ------------------------------------
        if pClr->nFocusField >= 0 then
            ' Any editing key says AMEND rather than REPLACE, so the seeded value stops being
            ' fresh and the next typed character is inserted instead of overwriting everything.
            select case wParam
            case VK_LEFT, VK_RIGHT, VK_HOME, VK_END, VK_BACK, VK_DELETE
                pClr->bEditFresh = false
            end select

            dim as long nLen = len( pClr->wszEdit )
            select case wParam
            case VK_LEFT
                if pClr->nCaret > 0 then pClr->nCaret -= 1
                pClr->bCaretOn = true
                InvalidateRect( hwnd, NULL, FALSE )
                return 0
            case VK_RIGHT
                if pClr->nCaret < nLen then pClr->nCaret += 1
                pClr->bCaretOn = true
                InvalidateRect( hwnd, NULL, FALSE )
                return 0
            case VK_HOME
                pClr->nCaret = 0
                pClr->bCaretOn = true
                InvalidateRect( hwnd, NULL, FALSE )
                return 0
            case VK_END
                pClr->nCaret = nLen
                pClr->bCaretOn = true
                InvalidateRect( hwnd, NULL, FALSE )
                return 0
            case VK_BACK
                if pClr->nCaret > 0 then
                    pClr->wszEdit = left( pClr->wszEdit, pClr->nCaret - 1 ) & mid( pClr->wszEdit, pClr->nCaret + 1 )
                    pClr->nCaret -= 1
                end if
                pClr->bCaretOn = true
                InvalidateRect( hwnd, NULL, FALSE )
                return 0
            case VK_DELETE
                if pClr->nCaret < nLen then
                    pClr->wszEdit = left( pClr->wszEdit, pClr->nCaret ) & mid( pClr->wszEdit, pClr->nCaret + 2 )
                end if
                pClr->bCaretOn = true
                InvalidateRect( hwnd, NULL, FALSE )
                return 0
            case VK_RETURN
                ' Commit and STAY in the field, re-seeded from the (possibly clamped) value, so
                ' the user can see what their "999" actually became.
                dim as long nField = pClr->nFocusField
                PsColorPicker_CommitField( pClr )
                PsColorPicker_BeginEdit( pClr, nField )
                return 0
            case VK_TAB
                dim as long nDelta = 1
                if (GetKeyState( VK_SHIFT ) and &h8000) <> 0 then nDelta = -1
                dim as long nNext = PsColorPicker_NextField( pClr, pClr->nFocusField, nDelta )
                if nNext >= 0 then
                    PsColorPicker_BeginEdit( pClr, nNext )
                    return 0
                end if
                ' Off the end: not consumed, so the dialog manager moves on. (We only get here
                ' at all if something bypassed WM_GETDLGCODE, which already declines this case.)
                exit select
            end select
            exit select
        end if

        ' ---- with no field focused: the arrows drive the body grid ----------------------
        select case wParam
        case VK_LEFT
            PsColorPicker_NavigateBody( pClr, -1 )
            return 0
        case VK_RIGHT
            PsColorPicker_NavigateBody( pClr, 1 )
            return 0
        case VK_UP
            ' A LIST moves one row; the MATRIX moves one whole row of cells. Left/Right above
            ' move by one entry on both, which on the matrix is the next cell along and on a list
            ' is the same thing Up/Down does -- harmless, and it means all four arrows work
            ' wherever the user happens to be.
            PsColorPicker_NavigateBody( pClr, -PsColorPicker_NavRowStep( pClr ) )
            return 0
        case VK_DOWN
            PsColorPicker_NavigateBody( pClr, PsColorPicker_NavRowStep( pClr ) )
            return 0
        end select

    case WM_CHAR
        if pClr = 0 then exit select
        if pClr->isEnabled = false then return 0
        if pClr->nFocusField < 0 then exit select
        if PsColorPicker_CallMessage( pClr, hwnd, uMsg, wParam, lParam ) then return 0

        dim as long ch = clng( wParam )
        ' Control characters never reach the buffer -- Backspace and Return arrive here as well
        ' as at WM_KEYDOWN, and inserting a chr(8) is exactly how a hand-rolled field ends up
        ' with invisible garbage in it.
        if ch < 32 then return 0

        ' THE ACCEPT TEST COMES FIRST, AND THE ORDER IS THE POINT. The first character typed
        ' after entering a field replaces the seeded value (see PSCOLORPICKER.bEditFresh) -- but
        ' only if it is a character this field would have taken anyway. Clearing the buffer
        ' before the test meant a stray letter WIPED the value it was rejected by, which is a
        ' worse outcome than either accepting or ignoring it. Measured on the first run.
        '
        ' The position handed to FieldAccepts is where the character will ACTUALLY land, which on
        ' a fresh buffer is 0 regardless of where the caret is sitting -- otherwise typing "#" to
        ' overwrite a full hex field would be refused for being at position 7.
        dim as long nPos = pClr->nCaret
        if pClr->bEditFresh then nPos = 0
        if PsColorPicker_FieldAccepts( pClr->nFocusField, ch, nPos ) = false then return 0

        if pClr->bEditFresh then
            pClr->wszEdit = ""
            pClr->nCaret = 0
            pClr->bEditFresh = false
        end if

        if len( pClr->wszEdit ) >= PsColorPicker_FieldMaxLen( pClr->nFocusField ) then return 0

        pClr->wszEdit = left( pClr->wszEdit, pClr->nCaret ) & chr( ch ) & mid( pClr->wszEdit, pClr->nCaret + 1 )
        pClr->nCaret += 1
        pClr->bCaretOn = true
        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly still gets the greyed rendering rather than a control that looks
        ' live but eats input.
        if pClr then
            pClr->isEnabled = (wParam <> 0)
            if pClr->isEnabled = false then
                PsColorPicker_ClearHot( pClr, hwnd )
                pClr->CancelDrag()
            end if
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_DESTROY
        if pClr then
            if pClr->hotTimerOn then
                KillTimer( hwnd, IDT_PSCOLORPICKER_HOTTRACK )
                pClr->hotTimerOn = false
            end if
            if pClr->caretTimerOn then
                KillTimer( hwnd, IDT_PSCOLORPICKER_CARET )
                pClr->caretTimerOn = false
            end if
            if GetCapture() = hwnd then ReleaseCapture()
            pClr->CancelDrag()
        end if

    case WM_NCDESTROY
        if pClr then
            ' The font is the host's and is left alone. This control owns no other window, no
            ' tooltip and no timer that outlives it.
            delete(pClr)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' PsColorPicker_Create
' ========================================================================================
function PsColorPicker_Create( byval hParent as HWND, byval id as long = 0 ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus, because its entry fields have to be typeable.
    '
    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND IT MUST BE. CWindow.Create defaults its
    ' dwExStyle to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control that supplies only
    ' dwStyle silently declares itself a CONTAINER: the dialog manager descends into it hunting
    ' for tabstops, finds no children, and SKIPS IT -- the control becomes unreachable by Tab.
    ' That shipped in PsButton's first commit and was then found in PsToggle and PsComboBox too
    ' (C:\dev\Learnings.md). It is ASSERTED in the self-test, not merely remembered.
    '
    ' This control is a LEAF despite looking like a panel: it has no child windows at all, which
    ' is the whole design. The test is "does this window have children the dialog manager should
    ' find?", and here the answer is no -- so 0 is right, and copying PsScrollPanel's
    ' WS_EX_CONTROLPARENT (which is right for IT, because it really does host the host's
    ' controls) would break Tab here.
    dim as HWND hCtrl = _
            pWindow->Create( hParent, "", @PsColorPicker_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header.

    dim as PSCOLORPICKER ptr pClr = new PSCOLORPICKER
    pClr->hWin            = hCtrl
    pClr->hParent         = hParent
    pClr->idc_ColorPicker = id
    pClr->id              = id

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule).
    '
    ' nBorderThick AND nFocusThick ARE DELIBERATELY ABSENT FROM THIS LIST. Both are handed to
    ' PaintRoundOutline, which DPI-scales the pen itself; scaling them here as well would give a
    ' two-pixel hairline at 175% where one was asked for. They DO end up DPI-aware -- the painter
    ' is simply the thing that does it. nCurvature IS scaled: it is a distance, and nothing
    ' downstream scales it.
    pClr->nPad        = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_PAD )
    pClr->nGap        = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_GAP )
    pClr->nTabHeight  = pWindow->ScaleY( PSCOLORPICKER_DEFAULT_TABHEIGHT )
    pClr->nCellW      = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_CELLW )
    pClr->nCellH      = pWindow->ScaleY( PSCOLORPICKER_DEFAULT_CELLH )
    pClr->nSwatchW    = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_SWATCHW )
    pClr->nScrollBarW = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_SCROLLW )
    pClr->nPreviewW   = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_PREVIEWW )
    pClr->nPreviewH   = pWindow->ScaleY( PSCOLORPICKER_DEFAULT_PREVIEWH )
    pClr->nFieldW     = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_FIELDW )
    pClr->nFieldH     = pWindow->ScaleY( PSCOLORPICKER_DEFAULT_FIELDH )
    pClr->nHexW       = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_HEXW )
    pClr->nLabelW     = pWindow->ScaleX( PSCOLORPICKER_DEFAULT_LABELW )
    pClr->nRowH       = pWindow->ScaleY( PSCOLORPICKER_DEFAULT_ROWH )
    pClr->nCurvature  = pWindow->ScaleY( PSCOLORPICKER_DEFAULT_CURVATURE )

    ' Defaults for the three host-supplied words. Assigned here rather than as TYPE field
    ' defaults so there is exactly one place they live.
    pClr->wszAssign       = "Assign"
    pClr->wszInitialLabel = "Initial"
    pClr->wszCurrentLabel = "Current"

    SetWindowLongPtr( hCtrl, GWLP_ID, id )
    pWindow->UserData(0) = cast(LONG_PTR, pClr)

    ' THE LIST SCROLLBAR -- the control's ONE child window. Created here and hidden; the layout
    ' positions it and PsColorPicker_SyncScrollBar decides per tab whether it is needed. Created
    ' AFTER UserData is set, because PsVScrollBar_Create can dispatch messages and our own
    ' WndProc must already be able to resolve its state block.
    pClr->hScroll = PsVScrollBar_Create( hCtrl, id + 1 )
    PsVScrollBar_SetScrollCallback( pClr->hScroll, @PsColorPicker_OnScroll )
    ShowWindow( pClr->hScroll, SW_HIDE )

    PsColorPicker_SyncBodySel( pClr )

    function = hCtrl

end function


' ========================================================================================
' Value.
' ========================================================================================
sub PsColorPicker_SetColor( _
            byval hCtrl        as HWND, _
            byval clr          as COLORREF, _
            byval nAlpha       as ubyte = 255, _
            byval bKeepInitial as boolean = false _
            )

    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub

    ' SILENT (bNotify false) -- the family's programmatic-setters-are-silent rule, and what makes
    ' it safe for a host to call this from inside its own ColorChangedCallback.
    if bKeepInitial = false then
        pClr->clrInitial    = clr
        pClr->nAlphaInitial = nAlpha
    end if
    PsColorPicker_ApplyColor( pClr, clr, nAlpha, false, CLR_PHASE_END )
    ' ApplyColor early-outs on an unchanged value, so the baseline change above would not repaint
    ' on its own.
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

function PsColorPicker_GetColor( byval hCtrl as HWND ) as COLORREF
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return 0
    return pClr->clrCurrent
end function

function PsColorPicker_GetAlpha( byval hCtrl as HWND ) as ubyte
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return 255
    return pClr->nAlphaCurrent
end function

sub PsColorPicker_SetInitial( byval hCtrl as HWND, byval clr as COLORREF, byval nAlpha as ubyte = 255 )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->clrInitial    = clr
    pClr->nAlphaInitial = nAlpha
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

function PsColorPicker_GetInitial( byval hCtrl as HWND ) as COLORREF
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return 0
    return pClr->clrInitial
end function

' An ACTION, not a setter (PsButton_Click's precedent), so it FIRES -- with a single END, because
' reverting is a settled choice and not a drag.
sub PsColorPicker_RevertToInitial( byval hCtrl as HWND )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    PsColorPicker_ApplyColor( pClr, pClr->clrInitial, pClr->nAlphaInitial, true, CLR_PHASE_END )
end sub


' ========================================================================================
' Alpha, tabs, palette selection.
' ========================================================================================
sub PsColorPicker_ShowAlpha( byval hCtrl as HWND, byval bShow as boolean )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if pClr->bShowAlpha = bShow then exit sub
    pClr->bShowAlpha = bShow
    ' The caret cannot be left in a field that just disappeared. Committed rather than dropped,
    ' because the user's typing is still the most recent thing they said.
    if (bShow = false) andalso (pClr->nFocusField = CLR_FIELD_A) then PsColorPicker_CommitField( pClr )
    pClr->Refresh()
end sub

function PsColorPicker_IsAlphaShown( byval hCtrl as HWND ) as boolean
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    return pClr->bShowAlpha
end function


sub PsColorPicker_SetSelectionOnly( byval hCtrl as HWND, byval bSelectionOnly as boolean )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if pClr->bSelectionOnly = bSelectionOnly then exit sub
    pClr->bSelectionOnly = bSelectionOnly
    ' The caret cannot be left in a field that just disappeared -- ShowAlpha's rule, for the
    ' same reason and with the same answer: commit rather than drop, because the user's typing
    ' is still the most recent thing they said.
    if bSelectionOnly andalso (pClr->nFocusField >= 0) then PsColorPicker_CommitField( pClr )
    pClr->Refresh()
end sub


function PsColorPicker_IsSelectionOnly( byval hCtrl as HWND ) as boolean
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    return pClr->bSelectionOnly
end function

sub PsColorPicker_SetTab( byval hCtrl as HWND, byval nTab as long )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if (nTab < CLR_TAB_WEB) orelse (nTab > CLR_TAB_PALETTE) then exit sub
    if (nTab = CLR_TAB_PALETTE) andalso (pClr->bShowPalette = false) then exit sub
    if pClr->nTab = nTab then exit sub
    pClr->nTab = nTab
    pClr->nScrollTop = 0
    pClr->bScrollToSel = true
    PsColorPicker_SyncBodySel( pClr )
    pClr->Refresh()
    ' SILENT: TabChangedCallback reports USER action, exactly as the colour callback does.
end sub

function PsColorPicker_GetTab( byval hCtrl as HWND ) as long
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return CLR_TAB_CUSTOM
    return pClr->nTab
end function

sub PsColorPicker_ShowPaletteTab( byval hCtrl as HWND, byval bShow as boolean )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if pClr->bShowPalette = bShow then exit sub
    pClr->bShowPalette = bShow
    ' Hiding the tab that is CURRENT would otherwise leave the control rendering a body whose tab
    ' is no longer on the strip -- visibly a bug, and one that only appears when a host turns the
    ' palette off at runtime.
    if (bShow = false) andalso (pClr->nTab = CLR_TAB_PALETTE) then
        pClr->nTab = CLR_TAB_CUSTOM
        pClr->nScrollTop = 0
        pClr->bScrollToSel = true
        PsColorPicker_SyncBodySel( pClr )
    end if
    pClr->Refresh()
end sub

function PsColorPicker_IsPaletteTabShown( byval hCtrl as HWND ) as boolean
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    return pClr->bShowPalette
end function

function PsColorPicker_GetPaletteSel( byval hCtrl as HWND ) as long
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return -1
    if pClr->nTab <> CLR_TAB_PALETTE then return -1
    return pClr->nBodySel
end function

sub PsColorPicker_SetPaletteSel( byval hCtrl as HWND, byval idx as long )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->nBodySel = idx
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub


' ========================================================================================
' Appearance.
' ========================================================================================
sub PsColorPicker_SetColors( byval hCtrl as HWND, byval pColors as PSCOLORPICKER_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->colors = *pColors
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

sub PsColorPicker_GetColors( byval hCtrl as HWND, byval pColors as PSCOLORPICKER_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    *pColors = pClr->colors
end sub

' The MEASURING font as well as the drawing one: it decides where the caret sits.
sub PsColorPicker_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->hTextFont = hFont
    pClr->Refresh()
end sub

function PsColorPicker_GetFont( byval hCtrl as HWND ) as HFONT
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return 0
    return pClr->hTextFont
end function

' byval, never "byref as const". A const-qualified DWSTRING reference that is then COPIED
' corrupts the process heap -- measured at 0xC0000374 in a 30-line program with no window
' (C:\dev\Learnings.md). Every DWSTRING parameter in this family is byval.
sub PsColorPicker_SetAssignText( byval hCtrl as HWND, byval Text as DWSTRING )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->wszAssign = Text
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

sub PsColorPicker_SetPreviewText( byval hCtrl as HWND, byval TextInitial as DWSTRING, byval TextCurrent as DWSTRING )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->wszInitialLabel = TextInitial
    pClr->wszCurrentLabel = TextCurrent
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

sub PsColorPicker_Refresh( byval hCtrl as HWND )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->Refresh()
end sub


' ========================================================================================
' State.
' ========================================================================================
function PsColorPicker_GetEnabled( byval hCtrl as HWND ) as boolean
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    return pClr->isEnabled
end function

sub PsColorPicker_SetEnabled( byval hCtrl as HWND, byval bEnabled as boolean )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if pClr->hWin = 0 then exit sub
    ' EnableWindow, not a cosmetic flag: the disable is then enforced by the system. It sends
    ' WM_ENABLE synchronously, and THAT is what updates isEnabled.
    EnableWindow( pClr->hWin, iif( bEnabled, TRUE, FALSE ) )
    InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

function PsColorPicker_GetFocused( byval hCtrl as HWND ) as boolean
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    return pClr->isFocused
end function


' ========================================================================================
' Geometry.
' ========================================================================================
sub PsColorPicker_GetIdealSize( byval hCtrl as HWND, byref cx as long, byref cy as long )
    cx = 0 : cy = 0
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    cx = pClr->nIdealW
    cy = pClr->nIdealH
end sub

sub PsColorPicker_GetMatrixSize( byval hCtrl as HWND, byref nCols as long, byref nRows as long )
    nCols = 0 : nRows = 0
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    nCols = pClr->nMatrixCols
    nRows = pClr->nMatrixRows
end sub

sub PsColorPicker_SetMatrixSize( byval hCtrl as HWND, byval nCols as long, byval nRows as long )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    ' Two columns minimum, because the last one is the greyscale ramp and the hue sweep is
    ' measured across the others -- one column would divide by zero in the colour law.
    if nCols < 2 then nCols = 2
    if nRows < 1 then nRows = 1
    pClr->nMatrixCols = nCols
    pClr->nMatrixRows = nRows
    PsColorPicker_SyncBodySel( pClr )
    pClr->Refresh()
end sub

sub PsColorPicker_GetCellSize( byval hCtrl as HWND, byref cx as long, byref cy as long )
    cx = 0 : cy = 0
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    cx = pClr->nCellW
    cy = pClr->nCellH
end sub

sub PsColorPicker_SetCellSize( byval hCtrl as HWND, byval cx as long, byval cy as long )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    if cx < 1 then cx = 1
    if cy < 1 then cy = 1
    pClr->nCellW = cx
    pClr->nCellH = cy
    pClr->Refresh()
end sub


' Force the pending layout, then hand back a copy. An assertion that reads a lazily-derived rect
' without forcing it measures the value from BEFORE the change it is testing (C:\dev\Learnings.md).
private function PsColorPicker_PartRect( byval pClr as PSCOLORPICKER ptr, byval nPart as long ) as RECT
    dim as RECT rc
    SetRectEmpty( @rc )
    if pClr = 0 then return rc
    select case nPart
    case CLR_PART_TABS     : rc = pClr->rcTabs
    case CLR_PART_MATRIX   : rc = pClr->rcMatrix
    case CLR_PART_SWATCHES : rc = pClr->rcSwatches
    case CLR_PART_INITIAL  : rc = pClr->rcInitial
    case CLR_PART_CURRENT  : rc = pClr->rcCurrent
    case CLR_PART_FIELDS   : rc = pClr->rcFields
    case CLR_PART_ALPHA    : rc = pClr->rcAlpha
    end select
    return rc
end function

function PsColorPicker_GetPartRect( byval hCtrl as HWND, byval nPart as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    rc = PsColorPicker_PartRect( pClr, nPart )
    return (IsRectEmpty( @rc ) = 0)
end function

function PsColorPicker_GetFieldRect( byval hCtrl as HWND, byval nField as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    if (nField < 0) orelse (nField >= CLR_FIELD_COUNT) then return false
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    rc = pClr->rcFieldBox(nField)
    return (IsRectEmpty( @rc ) = 0)
end function

function PsColorPicker_GetTabRect( byval hCtrl as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    if (idx < 0) orelse (idx > 3) then return false
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    rc = pClr->rcTab(idx)
    return (IsRectEmpty( @rc ) = 0)
end function


' The three public hit tests force the layout first, for the same reason the rect getters do.
function PsColorPicker_HitTestTab( byval hCtrl as HWND, byval x as long, byval y as long ) as long
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return -1
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    return PsColorPicker_HitTab( pClr, x, y )
end function

function PsColorPicker_HitTestField( byval hCtrl as HWND, byval x as long, byval y as long ) as long
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return -1
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    return PsColorPicker_HitField( pClr, x, y )
end function

function PsColorPicker_HitTestBody( byval hCtrl as HWND, byval x as long, byval y as long ) as long
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return -1
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    return PsColorPicker_HitBody( pClr, x, y )
end function


' ========================================================================================
' Callbacks.
' ========================================================================================
sub PsColorPicker_SetColorChangedCallback( byval hCtrl as HWND, byval usersub as CLR_ColorChangedCallbackSub )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->ColorChangedCallback = usersub
end sub

sub PsColorPicker_SetTabChangedCallback( byval hCtrl as HWND, byval usersub as CLR_TabChangedCallbackSub )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->TabChangedCallback = usersub
end sub

' All three at once: a palette with a count but no entry reader, or entries with no count, is a
' half-wired host that would render as an empty tab and give no clue why.
sub PsColorPicker_SetPaletteCallbacks( _
            byval hCtrl     as HWND, _
            byval fnCount   as CLR_PaletteCountCallbackFunc, _
            byval fnEntry   as CLR_PaletteEntryCallbackFunc, _
            byval subAssign as CLR_PaletteAssignCallbackSub _
            )

    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->PaletteCountCallback  = fnCount
    pClr->PaletteEntryCallback  = fnEntry
    pClr->PaletteAssignCallback = subAssign
    PsColorPicker_SyncBodySel( pClr )
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, FALSE )
end sub

sub PsColorPicker_SetMessageCallback( byval hCtrl as HWND, byval userfunc as CLR_MessageCallbackFunc )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->MessageCallback = userfunc
end sub

sub PsColorPicker_SetPaintCallback( byval hCtrl as HWND, byval usersub as CLR_PaintCallbackSub )
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then exit sub
    pClr->PaintCallback = usersub
    if pClr->hWin then InvalidateRect( pClr->hWin, NULL, TRUE )
end sub


' See the header in PsColorPicker.bi. The buffer comes from the PAINTINFO rather than being
' rebuilt, so this draws into the SAME buffer the callback is already holding -- calling it does
' not begin a second double-buffer cycle, and calling it twice simply draws twice.
'
' It deliberately does NOT re-flood the background: WM_PAINT already did that before the callback
' was invoked, and doing it again here would erase anything a host chose to draw BEFORE calling
' this rather than after.
sub PsColorPicker_RenderInfo( byval p as any ptr )
    if p = 0 then exit sub
    dim as PSCOLORPICKER_PAINTINFO ptr pi = cast( PSCOLORPICKER_PAINTINFO ptr, p )
    if pi->b = 0 then exit sub
    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( pi->hColorPicker )
    if pClr = 0 then exit sub
    PsColorPicker_RenderDefault( pClr, pi->b )
end sub


' ========================================================================================
' Render one part OFFSCREEN with the control's current painter and measure it two ways: the
' number of distinct colours in it (capped at 64) and an FNV-1a hash of its pixels.
'
' THREE THINGS HERE ARE LOAD-BEARING, all three from write-ups in Learnings.md:
'
'   1. The render goes through PsColorPicker_RenderDefault (or the host's callback) -- THE SAME
'      path WM_PAINT uses, with the PAINTINFO built by the same FillPaintInfo. A version of this
'      probe elsewhere in the family hand-built the struct and left a rect zeroed, the draw call
'      became a no-op, and the A/B came back identical. A probe that skips the code path it
'      exists to measure is worse than no probe.
'   2. isFocused is forced TRUE and the caret is forced ON, because the focus ring and the caret
'      are the two things drawn LAST and over the largest rects -- a probe run on an unfocused
'      control measures neither.
'   3. The buffer is given a CACHED memDC (the four-argument BeginDoubleBuffer), so
'      EndDoubleBuffer flushes GDI+ and blits WITHOUT deleting the bitmap underneath us. GDI+
'      batches: reading the bits before that flush samples a surface the shapes have not reached.
' ========================================================================================
private function PsColorPicker_ProbePart( _
            byval hCtrl  as HWND, _
            byval nPart  as long, _
            byref nTones as long, _
            byref nHash  as ulong _
            ) as boolean

    nTones = 0
    nHash  = 0

    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( hCtrl )
    if pClr = 0 then return false
    if pClr->bLayoutDirty then pClr->LayoutColorPicker()
    if pClr->bLayoutDirty then return false          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hCtrl, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return false

    dim as RECT rcPart = PsColorPicker_PartRect( pClr, nPart )
    if IsRectEmpty( @rcPart ) then return false

    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return false

    dim as HDC hRef = GetDC( hCtrl )
    if hRef = 0 then return false
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hCtrl, hRef )
        return false
    end if

    ' 32bpp top-down, matching what PsBufferPaint.SetupBitmap builds, so the pixels we read are
    ' laid out the way the renderer wrote them.
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = cw
        .biHeight      = -ch
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    dim as any ptr pBits
    dim as HBITMAP hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( hCtrl, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return false
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    dim as boolean bSavedFocus = pClr->isFocused
    dim as boolean bSavedCaret = pClr->bCaretOn
    pClr->isFocused = true
    pClr->bCaretOn  = true

    scope
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hCtrl, hMem, rcClient, hMem )

        b.SetBackColor( pClr->colors.BackColor )
        b.PaintClientRect()

        if pClr->PaintCallback then
            dim p as PSCOLORPICKER_PAINTINFO
            PsColorPicker_FillPaintInfo( pClr, hCtrl, @b, @p )
            pClr->PaintCallback( @p )
        else
            PsColorPicker_RenderDefault( pClr, @b )
        end if

        b.EndDoubleBuffer()
    end scope

    pClr->isFocused = bSavedFocus
    pClr->bCaretOn  = bSavedCaret

    ' Count distinct tones and hash the same pixels in one pass. The alpha byte is masked off:
    ' GDI text leaves it at 0 and GDI+ shapes leave it at 255, so an unmasked compare would
    ' report two tones for one colour -- and would make the hash depend on which renderer last
    ' touched a pixel rather than on what colour it is.
    dim tones(0 to 63) as ulong
    dim as long nSeen = 0
    dim as ulong h = &h811C9DC5          ' FNV-1a 32-bit offset basis
    const FNV_PRIME as ulong = &h01000193
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = rcPart.top to rcPart.bottom - 1
        for x as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(y * cw) + x] and &h00FFFFFF

            dim as boolean bFound = false
            for t as long = 0 to nSeen - 1
                if tones(t) = c then bFound = true : exit for
            next
            if bFound = false then
                if nSeen < 64 then
                    tones(nSeen) = c
                    nSeen += 1
                end if
            end if

            h = (h xor (c and &hFF)) * FNV_PRIME
            h = (h xor ((c shr 8) and &hFF)) * FNV_PRIME
            h = (h xor ((c shr 16) and &hFF)) * FNV_PRIME
        next
    next

    SelectObject( hMem, hOldBmp )
    DeleteObject( hbm )
    DeleteDC( hMem )

    nTones = nSeen
    nHash  = h
    return true
end function


' See the header in PsColorPicker.bi. A part wiped by a PaintBorderRect fill is literally ONE
' tone, so the useful signal is the count LEAVING 1 -- not the count being large.
'
' AND THE FLOOR IS PER-PART, MEASURED, NOT INHERITED. PsButton's and PsCheckBox's "> 1" threshold
' PASSED ON A COMPLETELY DESTROYED RENDER in PsMessageBox and again in PsOptionButton, because
' what else happens to be inside the part rect keeps the count above 1 on its own. The self-test
' calibrates its floors against measured healthy and flooded values; do not copy a number from a
' sibling.
function PsColorPicker_CountRenderedTones( byval hCtrl as HWND, byval nPart as long ) as long
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsColorPicker_ProbePart( hCtrl, nPart, nTones, nHash ) = false then return 0
    return nTones
end function

' Compare two hashes of the SAME part taken in two different states: equal means the state change
' never reached the surface. It can only prove difference, never correctness. (0 is also the
' failure return; an FNV-1a of a non-empty region landing exactly on 0 is possible in principle
' and has been accepted rather than worked around.)
function PsColorPicker_HashRenderedPart( byval hCtrl as HWND, byval nPart as long ) as ulong
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsColorPicker_ProbePart( hCtrl, nPart, nTones, nHash ) = false then return 0
    return nHash
end function


#include once "PsColorPicker_SelfTest.inc"
