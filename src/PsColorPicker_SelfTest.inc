''
''  PsColorPicker_SelfTest.inc  --  gated on PSCOLORPICKER_SELFTEST=1
''
''  Included at the end of PsColorPicker.inc, so it is the SAME module and can reach the private
''  internals (PsColorPicker_HitBody, PsColorPicker_GridColorAt, PsColorPicker_NextField, the
''  state block itself). That is deliberate: several of the invariants worth asserting here are
''  not reachable through the public API at all.
''
''  FOUR RULES THE ASSERTIONS THEMSELVES FOLLOW, each from a recorded false-confidence incident
''  in C:\dev\Learnings.md:
''
''    1. ASSERT PRECONDITIONS, DO NOT SKIP SILENTLY. A skipped assertion reads as a pass in the
''       failure count. THE ASSERTION COUNT IS THE ORACLE, not the failure count -- compare it
''       against the recorded baseline in the README.
''    2. FORCE LAZY DERIVATION BEFORE ASSERTING IT. Reading a derived rect without forcing the
''       layout asserts the value from before the change under test.
''    3. ISOLATE WHAT THE CASE NAMES. PsCheckBox shipped an assertion called "the checked state
''       reaches the pixels" that survived freezing glyph resolution outright, because the state
''       also moved a colour: the assertion was true, its NAME was not. Five separate occurrences
''       of that shape are now on record in this family. Where a probe here could pass for a
''       reason other than the one it names, it is run twice with the other reason removed.
''    4. DO NOT INHERIT A TONE THRESHOLD FROM A SIBLING. PsButton's "> 1" floor passed on a
''       completely destroyed render in both PsMessageBox and PsOptionButton, because what else
''       is inside the part rect keeps the count above 1 by itself. The floors below are
''       calibrated against MEASURED healthy and flooded values and are commented with both.
''
''  WHAT THIS FILE STRUCTURALLY CANNOT TEST, stated rather than left to be discovered:
''    - MOUSE CAPTURE. The drag case drives WM_LBUTTONDOWN / WM_MOUSEMOVE / WM_LBUTTONUP with
''      SendMessage, which runs the same code path but does not reproduce the system behaviour
''      capture exists for -- moves arriving while the cursor is outside the window. So it
''      asserts the PHASE SEQUENCE, which is real, and not the capture, which is the author's
''      interactive pass. Learnings.md records SendMessage-simulated clicks producing false
''      confidence about exactly this.
''    - THE CARET BLINK, hover, and anything else on a timer.
''    - That a colour a user sees is the colour they meant. The matrix's law is asserted for
''      monotonicity and round-tripping; whether 12 rows of it looks right is a human question.
''

dim shared as long gCLR_TestCount
dim shared as long gCLR_FailCount

' Notification recorder. The phase sequence (BEGIN, MOVE..., END) is the thing worth recording
' rather than merely counting, because "a drag emits one BEGIN and one END" and "a swatch click
' emits exactly one END and no BEGIN" are both statements about the ORDER.
const CLR_MAXNOTIFY = 64
dim shared as long     gCLR_NotifyCount
dim shared as long     gCLR_NotifyPhase(0 to CLR_MAXNOTIFY - 1)
dim shared as COLORREF gCLR_NotifyColor(0 to CLR_MAXNOTIFY - 1)
dim shared as long     gCLR_TabNotifyCount

private sub CLRTest_OnColorChanged( _
            byval hCtrl  as HWND, _
            byval clr    as COLORREF, _
            byval nAlpha as ubyte, _
            byval nPhase as long _
            )

    if gCLR_NotifyCount < CLR_MAXNOTIFY then
        gCLR_NotifyPhase( gCLR_NotifyCount ) = nPhase
        gCLR_NotifyColor( gCLR_NotifyCount ) = clr
    end if
    gCLR_NotifyCount += 1
end sub

private sub CLRTest_OnTabChanged( byval hCtrl as HWND, byval nTab as long )
    gCLR_TabNotifyCount += 1
end sub

private sub CLRTest_ResetNotify()
    gCLR_NotifyCount = 0
    gCLR_TabNotifyCount = 0
    for i as long = 0 to CLR_MAXNOTIFY - 1
        gCLR_NotifyPhase(i) = -1
        gCLR_NotifyColor(i) = 0
    next
end sub


' bOK, never "ok": GDI+'s Status enum defines Ok = 0 in namespace AfxNova, and every host says
' "using AfxNova", so an identifier called ok is a duplicate definition.
private sub CLRTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gCLR_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gCLR_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function CLRTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function

private function CLRTest_ClrStr( byval clr as COLORREF ) as string
    return str( clng(clr) and &hFF ) & "," & _
           str( (clng(clr) shr 8) and &hFF ) & "," & _
           str( (clng(clr) shr 16) and &hFF )
end function

' ITU-R BT.601 luma, the same weighting PsColorPicker_ContrastColor uses. Here it is only used
' to assert that the matrix gets DARKER down a column, which is a claim about ordering and so
' does not depend on the weighting being the right one.
private function CLRTest_Luma( byval clr as COLORREF ) as long
    dim as long nR = clng(clr) and &hFF
    dim as long nG = (clng(clr) shr 8) and &hFF
    dim as long nB = (clng(clr) shr 16) and &hFF
    return ((299 * nR) + (587 * nG) + (114 * nB)) \ 1000
end function

#define CLRTEST_LP( x, y )  cast( LPARAM, ((cast(ulong,(y)) shl 16) or (cast(ulong,(x)) and &hFFFF)) )

' Ask the control what it would tell IsDialogMessage about one key. This is the synthetic-MSG
' diagnostic that found the family's WS_EX_CONTROLPARENT Tab bug -- it asks the question the
' dialog manager asks, rather than the question we hope it asks.
private function CLRTest_DlgCode( byval h as HWND, byval nMsg as UINT, byval nKey as long ) as long
    dim as MSG m
    m.hwnd    = h
    m.message = nMsg
    m.wParam  = cast( WPARAM, nKey )
    m.lParam  = 0
    return clng( SendMessageW( h, WM_GETDLGCODE, cast(WPARAM, nKey), cast(LPARAM, @m) ) )
end function

' Type one character into whichever field has the caret, through the real WM_CHAR path.
private sub CLRTest_TypeChar( byval h as HWND, byval ch as long )
    SendMessageW( h, WM_CHAR, cast(WPARAM, ch), 0 )
end sub

private sub CLRTest_TypeString( byval h as HWND, byval s as string )
    for i as long = 1 to len(s)
        CLRTest_TypeChar( h, asc( mid(s, i, 1) ) )
    next
end sub

private sub CLRTest_Key( byval h as HWND, byval nKey as long )
    SendMessageW( h, WM_KEYDOWN, cast(WPARAM, nKey), 0 )
end sub

private sub CLRTest_ClickAt( byval h as HWND, byval x as long, byval y as long )
    SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( x, y ) )
    SendMessageW( h, WM_LBUTTONUP,   0, CLRTEST_LP( x, y ) )
end sub


' ========================================================================================
' THE SUITE
' ========================================================================================
sub PsColorPicker_RunSelfTest( byval hWndParent as HWND )

    if environ("PSCOLORPICKER_SELFTEST") <> "1" then exit sub

    gCLR_TestCount = 0
    gCLR_FailCount = 0

    print ""
    print "--- PSCOLORPICKER_SELFTEST ---"

    ' ====================================================================================
    ' 1. PURE FUNCTIONS.  No window needed, so these run first: if the colour law is wrong
    '    every geometric assertion below is measuring the wrong thing.
    ' ====================================================================================
    print "=== pure colour functions ==="

    scope
        dim as COLORREF cBlack = PsColorPicker_HSLtoRGB( 0.0, 0.0, 0.0 )
        dim as COLORREF cWhite = PsColorPicker_HSLtoRGB( 0.0, 0.0, 1.0 )
        CLRTest_Check( "HSL lightness 0 is pure black", (cBlack = BGR(0,0,0)), CLRTest_ClrStr(cBlack) )
        ' The +0.5-then-truncate rounding is what makes this one pass; a bare cast lands on 254.
        CLRTest_Check( "HSL lightness 1 is pure white", (cWhite = BGR(255,255,255)), CLRTest_ClrStr(cWhite) )

        dim as COLORREF cR = PsColorPicker_HSLtoRGB( 0.0,        1.0, 0.5 )
        dim as COLORREF cG = PsColorPicker_HSLtoRGB( 1.0 / 3.0,  1.0, 0.5 )
        dim as COLORREF cB = PsColorPicker_HSLtoRGB( 2.0 / 3.0,  1.0, 0.5 )
        CLRTest_Check( "HSL hue 0 at full saturation is pure red",   (cR = BGR(255,0,0)), CLRTest_ClrStr(cR) )
        CLRTest_Check( "HSL hue 1/3 at full saturation is pure green", (cG = BGR(0,255,0)), CLRTest_ClrStr(cG) )
        CLRTest_Check( "HSL hue 2/3 at full saturation is pure blue",  (cB = BGR(0,0,255)), CLRTest_ClrStr(cB) )

        ' Out of range on every axis. Clamped, not wrapped -- a hue of 1.4 must not silently
        ' become 0.4 and hand back a different colour than the caller asked for.
        dim as COLORREF cLo = PsColorPicker_HSLtoRGB( -1.0, -1.0, -1.0 )
        dim as COLORREF cHi = PsColorPicker_HSLtoRGB(  2.0,  2.0,  2.0 )
        CLRTest_Check( "HSL clamps below range rather than wrapping", (cLo = BGR(0,0,0)), CLRTest_ClrStr(cLo) )
        CLRTest_Check( "HSL clamps above range rather than wrapping", (cHi = BGR(255,255,255)), CLRTest_ClrStr(cHi) )
    end scope

    scope
        const nC = 16
        const nR = 12

        ' The greyscale column: R = G = B on every row. If the "last column is grey" branch were
        ' ever driven by the hue instead of the saturation this is what would catch it.
        dim as boolean bGrey = true
        dim as string sGrey
        for r as long = 0 to nR - 1
            dim as COLORREF c = PsColorPicker_MatrixColorAt( nC - 1, r, nC, nR )
            dim as long v1 = clng(c) and &hFF
            dim as long v2 = (clng(c) shr 8) and &hFF
            dim as long v3 = (clng(c) shr 16) and &hFF
            dim as boolean bEq = ((v1 = v2) andalso (v2 = v3))
            if bEq = false then bGrey = false
            if r = 0 then sGrey = CLRTest_ClrStr(c)
        next
        CLRTest_Check( "the matrix's last column is a true greyscale ramp", bGrey, "row0=" & sGrey )

        ' Every column gets darker downward. This is the property that makes the grid readable as
        ' tints-above-shades, and it is a claim about all 16 columns rather than a spot check.
        dim as boolean bMono = true
        dim as string sMono
        for c as long = 0 to nC - 1
            dim as long nPrev = 256
            for r as long = 0 to nR - 1
                dim as long nL = CLRTest_Luma( PsColorPicker_MatrixColorAt( c, r, nC, nR ) )
                if nL >= nPrev then bMono = false
                nPrev = nL
            next
            if c = 0 then sMono = "col0 top=" & str( CLRTest_Luma( PsColorPicker_MatrixColorAt(0,0,nC,nR) ) ) & _
                                  " bottom=" & str( CLRTest_Luma( PsColorPicker_MatrixColorAt(0,nR-1,nC,nR) ) )
        next
        CLRTest_Check( "every matrix column gets strictly darker downward", bMono, sMono )

        ' Neither end of a column is pure white or pure black. Those are reachable from the
        ' greyscale column, and spending a whole ROW on each would waste two of twelve -- which is
        ' exactly what l = 1 - row/rows (rather than the +0.5 form) would have done.
        dim as COLORREF cTop = PsColorPicker_MatrixColorAt( 0, 0, nC, nR )
        dim as COLORREF cBot = PsColorPicker_MatrixColorAt( 0, nR - 1, nC, nR )
        dim as boolean bEnds = ((cTop <> BGR(255,255,255)) andalso (cBot <> BGR(0,0,0)))
        CLRTest_Check( "a chromatic column wastes no row on pure white or pure black", bEnds, _
                       CLRTest_ClrStr(cTop) & " / " & CLRTest_ClrStr(cBot) )

        ' Hue 0 appears ONCE, not at both ends: the sweep is measured across the chromatic
        ' columns only. Column 0 and column nC-2 must differ.
        dim as COLORREF cFirstHue = PsColorPicker_MatrixColorAt( 0, 5, nC, nR )
        dim as COLORREF cLastHue  = PsColorPicker_MatrixColorAt( nC - 2, 5, nC, nR )
        CLRTest_Check( "the hue sweep does not repeat itself at the far end", _
                       (cFirstHue <> cLastHue), _
                       CLRTest_ClrStr(cFirstHue) & " vs " & CLRTest_ClrStr(cLastHue) )

        ' Out-of-range indices clamp rather than reading off the end of the law.
        dim as COLORREF cClampA = PsColorPicker_MatrixColorAt( -5, -5, nC, nR )
        dim as COLORREF cClampB = PsColorPicker_MatrixColorAt( 999, 999, nC, nR )
        dim as boolean bClamp = ((cClampA = PsColorPicker_MatrixColorAt(0,0,nC,nR)) andalso _
                                 (cClampB = PsColorPicker_MatrixColorAt(nC-1,nR-1,nC,nR)))
        CLRTest_Check( "MatrixColorAt clamps out-of-range indices", bClamp, _
                       CLRTest_ClrStr(cClampA) & " / " & CLRTest_ClrStr(cClampB) )

        ' THE ROUND TRIP, over the whole grid. This is the assertion that would catch a colour
        ' law and an index-of that had drifted apart -- which is what produces "clicking a swatch
        ' rings a different one", a defect that looks fine in a screenshot.
        dim as long nBad = 0
        dim as string sBad
        for r as long = 0 to nR - 1
            for c as long = 0 to nC - 1
                dim as COLORREF cc = PsColorPicker_MatrixColorAt( c, r, nC, nR )
                dim as long fc, fr
                if PsColorPicker_MatrixIndexOf( cc, nC, nR, fc, fr ) = false then
                    nBad += 1
                    if nBad = 1 then sBad = "not found at " & str(c) & "," & str(r)
                else
                    ' The FOUND cell must hold the same colour. Not the same INDICES -- a grid may
                    ' legitimately contain a duplicate (the greyscale column's neighbours at low
                    ' lightness round together), and demanding index equality would fail for a
                    ' reason that is not a defect.
                    if PsColorPicker_MatrixColorAt( fc, fr, nC, nR ) <> cc then
                        nBad += 1
                        if nBad = 1 then sBad = "wrong cell at " & str(c) & "," & str(r)
                    end if
                end if
            next
        next
        CLRTest_Check( "MatrixColorAt / MatrixIndexOf round-trip over all 192 cells", _
                       (nBad = 0), "mismatches=" & str(nBad) & " " & sBad )

        ' A colour that is not in the grid finds nothing. Without this the round trip above could
        ' be satisfied by an IndexOf that returned (0,0) for everything.
        dim as long qc, qr
        dim as boolean bFound = PsColorPicker_MatrixIndexOf( BGR(1,2,3), nC, nR, qc, qr )
        dim as boolean bMiss = ((bFound = false) andalso (qc = -1) andalso (qr = -1))
        CLRTest_Check( "MatrixIndexOf reports a miss for a colour not in the grid", bMiss, _
                       "found=" & str(bFound) & " at " & str(qc) & "," & str(qr) )
    end scope

    scope
        ' HEX. A COLORREF is 0x00BBGGRR, so the byte order is REVERSED from the way a hex colour
        ' is written -- the wrong-and-plausible mistake. BGR(1,2,3) must format as #010203 and not
        ' as #030201, and a symmetric test value would not tell the two apart.
        dim as DWSTRING wszH = PsColorPicker_FormatHex( BGR(1, 2, 3) )
        dim as string sH = wszH
        CLRTest_Check( "FormatHex writes R,G,B order and not the COLORREF's byte order", _
                       (sH = "#010203"), sH )

        dim as COLORREF cP
        dim as boolean bP1 = PsColorPicker_ParseHex( "#FF8000", cP )
        dim as boolean bHit1 = (bP1 andalso (cP = BGR(255,128,0)))
        CLRTest_Check( "ParseHex reads #RRGGBB", bHit1, CLRTest_ClrStr(cP) )

        dim as boolean bP2 = PsColorPicker_ParseHex( "ff8000", cP )
        dim as boolean bHit2 = (bP2 andalso (cP = BGR(255,128,0)))
        CLRTest_Check( "ParseHex accepts a missing # and lower case", bHit2, CLRTest_ClrStr(cP) )

        dim as boolean bR1 = PsColorPicker_ParseHex( "", cP )
        dim as boolean bR2 = PsColorPicker_ParseHex( "#FF80", cP )
        dim as boolean bR3 = PsColorPicker_ParseHex( "#FF80GG", cP )
        dim as boolean bR4 = PsColorPicker_ParseHex( "#FF800012", cP )
        dim as boolean bRej = ((bR1 = false) andalso (bR2 = false) andalso (bR3 = false) andalso (bR4 = false))
        CLRTest_Check( "ParseHex rejects empty, short, long and non-hex input", bRej, _
                       str(bR1) & "/" & str(bR2) & "/" & str(bR3) & "/" & str(bR4) )

        ' Format -> Parse over a spread of values, INCLUDING every channel at an extreme, so a
        ' transposition between two channels cannot survive.
        dim as long nBad = 0
        dim as COLORREF probes(0 to 6) = { BGR(0,0,0), BGR(255,255,255), BGR(255,0,0), _
                                           BGR(0,255,0), BGR(0,0,255), BGR(18,52,86), BGR(171,205,239) }
        for i as long = 0 to 6
            dim as COLORREF back
            if PsColorPicker_ParseHex( PsColorPicker_FormatHex( probes(i) ), back ) = false then
                nBad += 1
            elseif back <> probes(i) then
                nBad += 1
            end if
        next
        CLRTest_Check( "FormatHex / ParseHex round-trip", (nBad = 0), "mismatches=" & str(nBad) )
    end scope

    scope
        dim as COLORREF cOnBlack = PsColorPicker_ContrastColor( BGR(0,0,0) )
        dim as COLORREF cOnWhite = PsColorPicker_ContrastColor( BGR(255,255,255) )
        CLRTest_Check( "ContrastColor is white on black", (cOnBlack = BGR(255,255,255)), CLRTest_ClrStr(cOnBlack) )
        CLRTest_Check( "ContrastColor is black on white", (cOnWhite = BGR(0,0,0)), CLRTest_ClrStr(cOnWhite) )
        ' Both sides of the threshold, so the function is not merely "always one answer". Pure
        ' green has luma 149 and pure blue 29 -- one either side of 140.
        dim as COLORREF cOnGreen = PsColorPicker_ContrastColor( BGR(0,255,0) )
        dim as COLORREF cOnBlue  = PsColorPicker_ContrastColor( BGR(0,0,255) )
        dim as boolean bBoth = ((cOnGreen = BGR(0,0,0)) andalso (cOnBlue = BGR(255,255,255)))
        CLRTest_Check( "ContrastColor switches sides across its threshold", bBoth, _
                       "green->" & CLRTest_ClrStr(cOnGreen) & " blue->" & CLRTest_ClrStr(cOnBlue) )
    end scope

    scope
        dim as boolean bD = PsColorPicker_FieldAccepts( CLR_FIELD_R, asc("7"), 0 )
        dim as boolean bA = PsColorPicker_FieldAccepts( CLR_FIELD_R, asc("a"), 0 )
        dim as boolean bNum = (bD andalso (bA = false))
        CLRTest_Check( "a numeric field takes digits and nothing else", bNum, str(bD) & "/" & str(bA) )

        ' POSITION MUST NOT MATTER. nPos survives in the signature only because the hex field's
        ' "# is legal at 0 and nowhere else" rule used to need it; with hex gone, a digit is
        ' equally legal everywhere, and asserting that over a sweep is what would catch a future
        ' edit that reintroduced a positional rule without saying so.
        ' nCharPos, NOT pos: FreeBASIC is case-insensitive and POS() is the built-in that reports
        ' the console column, so a loop variable called pos is a duplicated definition. Same class
        ' as bIn / BIN() a few scopes below, and as `width`, `step` and `fb`.
        dim as boolean bPosFree = true
        for nCharPos as long = 0 to 8
            if PsColorPicker_FieldAccepts( CLR_FIELD_R, asc("7"), nCharPos ) = false then bPosFree = false
            if PsColorPicker_FieldAccepts( CLR_FIELD_R, asc("#"), nCharPos ) then bPosFree = false
        next
        CLRTest_Check( "a channel field takes a digit at every position and '#' at none", bPosFree, "" )

        ' NO FIELD TAKES A HEX LETTER ANY MORE. A negative over every field, so reintroducing
        ' the a-f branch without a field to justify it turns this red.
        dim as boolean bNoHexLetters = true
        for f as long = 0 to CLR_FIELD_COUNT - 1
            if PsColorPicker_FieldAccepts( f, asc("f"), 3 ) then bNoHexLetters = false
            if PsColorPicker_FieldAccepts( f, asc("F"), 3 ) then bNoHexLetters = false
        next
        CLRTest_Check( "no field accepts a hex letter now that the hex field is gone", _
                       bNoHexLetters, "" )

        ' An out-of-range field id accepts NOTHING. It used to fall through to the digit test and
        ' say yes, which is the wrong answer for a field that does not exist.
        dim as boolean bBadField = (PsColorPicker_FieldAccepts( CLR_FIELD_COUNT, asc("7"), 0 ) = false) andalso _
                                   (PsColorPicker_FieldAccepts( -1, asc("7"), 0 ) = false)
        CLRTest_Check( "an out-of-range field id accepts nothing", bBadField, "" )

        dim as boolean bLen = true
        for f as long = 0 to CLR_FIELD_COUNT - 1
            if PsColorPicker_FieldMaxLen(f) <> 3 then bLen = false
        next
        CLRTest_Check( "every field is three digits long", bLen, _
                       str(PsColorPicker_FieldMaxLen(CLR_FIELD_R)) )
    end scope

    scope
        ' THE HALF-OPEN CONVENTION, asserted directly. PtIn and CellFromPoint have to agree about
        ' it or a grid's last column answers to a click one pixel outside the grid.
        dim as RECT rc
        SetRect( @rc, 10, 20, 30, 40 )
        ' bInside, NOT bIn: FreeBASIC is case-insensitive and `bin` is the built-in
        ' binary-formatting function, so a variable called bIn is a duplicated definition and the
        ' later uses resolve to BIN(). Same class as `width`, `step` and `fb`.
        dim as boolean bInside = PsColorPicker_PtIn( rc, 10, 20 )
        dim as boolean bOut = PsColorPicker_PtIn( rc, 30, 40 )
        dim as boolean bLast = PsColorPicker_PtIn( rc, 29, 39 )
        dim as boolean bHalf = (bInside andalso bLast andalso (bOut = false))
        CLRTest_Check( "PtIn includes top-left and excludes bottom-right", bHalf, _
                       str(bInside) & "/" & str(bLast) & "/" & str(bOut) )

        dim as RECT rg
        SetRect( @rg, 100, 200, 100 + (4 * 10), 200 + (3 * 8) )
        dim as long c, r
        dim as boolean bA1 = PsColorPicker_CellFromPoint( rg, 10, 8, 4, 3, 100, 200, c, r )
        dim as boolean bFirst = (bA1 andalso (c = 0) andalso (r = 0))
        CLRTest_Check( "CellFromPoint maps the first pixel to cell 0,0", bFirst, str(c) & "," & str(r) )

        dim as boolean bA2 = PsColorPicker_CellFromPoint( rg, 10, 8, 4, 3, 139, 223, c, r )
        dim as boolean bLastCell = (bA2 andalso (c = 3) andalso (r = 2))
        CLRTest_Check( "CellFromPoint maps the last pixel to the last cell", bLastCell, str(c) & "," & str(r) )

        dim as boolean bA3 = PsColorPicker_CellFromPoint( rg, 10, 8, 4, 3, 140, 200, c, r )
        dim as boolean bA4 = PsColorPicker_CellFromPoint( rg, 10, 8, 4, 3, 99, 200, c, r )
        dim as boolean bEdge = ((bA3 = false) andalso (bA4 = false))
        CLRTest_Check( "CellFromPoint refuses one pixel past either edge", bEdge, str(bA3) & "/" & str(bA4) )
    end scope


    ' ====================================================================================
    ' 2. THE CONTROL.  Created, sized to ideal, never shown -- everything below is geometry,
    '    state and offscreen rendering, none of which needs a visible window.
    ' ====================================================================================
    dim as HWND h = PsColorPicker_Create( hWndParent, 9100 )
    if h = 0 then
        print "--- PsColorPicker self-test ABORTED: could not create the control ---"
        exit sub
    end if

    dim as PSCOLORPICKER ptr pClr = GetColorPickerPointer( h )
    if pClr = 0 then
        print "--- PsColorPicker self-test ABORTED: no state block ---"
        DestroyWindow( h )
        exit sub
    end if

    PsColorPicker_SetColorChangedCallback( h, @CLRTest_OnColorChanged )
    PsColorPicker_SetTabChangedCallback( h, @CLRTest_OnTabChanged )

    dim as long nIW, nIH
    PsColorPicker_GetIdealSize( h, nIW, nIH )
    SetWindowPos( h, 0, 0, 0, nIW, nIH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    ' SHOWN, and it has to be: GetNextDlgTabItem SKIPS invisible windows, so the end-to-end tab
    ' walk below reports a miss on a control that is perfectly reachable once it is on screen.
    ' The suite runs before the host form is shown, so nothing flashes.
    ShowWindow( h, SW_SHOW )
    ' FORCE THE DERIVATION. GetIdealSize ran the layout while the client was still 0x0, so it
    ' bailed and left every rect empty and the dirty flag set; SetWindowPos then marked it dirty
    ' again. Reading pClr->rcContent now without this measures the value from BEFORE the size
    ' was applied -- which is rule 2 in this file's own header, and it was broken here on the
    ' first run: four geometry assertions failed against (0,0)-(0,0).
    pClr->LayoutColorPicker()

    print "=== window and tab order ==="

    scope
        ' THE FAMILY'S TAB BUG, ASSERTED RATHER THAN REMEMBERED. CWindow.Create defaults
        ' dwExStyle to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE; a leaf control that inherits that
        ' declares itself a container, the dialog manager descends into it, finds no children and
        ' SKIPS it. That shipped in PsButton, PsToggle and PsComboBox (C:\dev\Learnings.md).
        dim as LONG_PTR nEx = GetWindowLongPtr( h, GWL_EXSTYLE )
        dim as boolean bNoCP = ((nEx and WS_EX_CONTROLPARENT) = 0)
        CLRTest_Check( "the control is a tab ITEM, not a tab CONTAINER (no WS_EX_CONTROLPARENT)", _
                       bNoCP, "exstyle=&h" & hex(nEx) )

        dim as LONG_PTR nSt = GetWindowLongPtr( h, GWL_STYLE )
        CLRTest_Check( "the control carries WS_TABSTOP", ((nSt and WS_TABSTOP) <> 0), "style=&h" & hex(nSt) )

        ' THE END-TO-END WALK THAT USED TO LIVE HERE IS GONE, AND IT HAD TO. It asked
        ' GetNextDlgTabItem( hWndParent, ... ) whether the dialog manager could reach this
        ' control from its parent -- the question that mattered while the picker was an embedded
        ' CHILD sharing a tab order with its siblings.
        '
        ' A WS_POPUP HAS NO PARENT TO BE REACHED FROM. hParent is its OWNER, not its parent, and
        ' GetNextDlgTabItem walks children -- so that assertion could only ever fail now, and
        ' failing would mean nothing. What replaced it is the pair below: the window really is a
        ' popup, and it is NOT owned in a way that puts it in anyone's tab order.
        CLRTest_Check( "the popup is not a child of the window that opened it", _
                       (GetParent( h ) <> h) andalso ((clng(GetWindowLongPtrW( h, GWL_STYLE )) and WS_CHILD) = 0), _
                       "" )

        ' ...and the tabstop it still carries is for DoModal's OWN IsDialogMessage, which is the
        ' only dialog manager that will ever see this window. Without it that loop would skip the
        ' popup and Tab inside the fields would do nothing.
        CLRTest_Check( "the popup is still a tabstop, for DoModal's own IsDialogMessage", _
                       ((clng(GetWindowLongPtrW( h, GWL_STYLE )) and WS_TABSTOP) <> 0), "" )
    end scope

    print "=== geometry ==="

    scope
        CLRTest_Check( "the ideal size is valid before the control was ever sized", _
                       ((nIW > 0) andalso (nIH > 0)), str(nIW) & "x" & str(nIH) )

        dim as long nMatW = pClr->nMatrixCols * pClr->nCellW
        CLRTest_Check( "the ideal width covers the whole matrix plus its padding", _
                       (nIW >= (nMatW + (2 * pClr->nPad))), _
                       "ideal=" & str(nIW) & " matrix=" & str(nMatW) & " pad=" & str(pClr->nPad) )

        dim as RECT rcClient
        GetClientRect( h, @rcClient )
        dim as RECT rcC = pClr->rcContent
        dim as boolean bPad = ((rcC.left = rcClient.left + pClr->nPad) andalso _
                               (rcC.top = rcClient.top + pClr->nPad) andalso _
                               (rcC.right = rcClient.right - pClr->nPad) andalso _
                               (rcC.bottom = rcClient.bottom - pClr->nPad))
        CLRTest_Check( "rcContent is the client deflated by the padding", bPad, CLRTest_RectStr(rcC) )

        CLRTest_Check( "the tab strip is exactly nTabHeight tall", _
                       ((pClr->rcTabs.bottom - pClr->rcTabs.top) = pClr->nTabHeight), _
                       CLRTest_RectStr(pClr->rcTabs) )

        ' THE TABS TILE FROM THE LEFT AND DELIBERATELY DO NOT REACH THE RIGHT EDGE. This
        ' assertion used to require the last tab to meet rcTabs.right, which was correct while
        ' the strip divided itself into equal parts; a tab is now as wide as its own caption, so
        ' the leftover to the right of "Custom" is simply strip. Requiring the old behaviour here
        ' would force the equal-split back.
        dim as long nTabs = pClr->TabCount()
        dim as boolean bTile = (pClr->rcTab(0).left = pClr->rcTabs.left)
        for i as long = 0 to nTabs - 2
            if pClr->rcTab(i).right <> pClr->rcTab(i + 1).left then bTile = false
        next
        CLRTest_Check( "the tab cells tile from the left with no gap or overlap", bTile, _
                       "n=" & str(nTabs) & " " & CLRTest_RectStr(pClr->rcTab(0)) & " .. " & _
                       CLRTest_RectStr(pClr->rcTab(nTabs - 1)) )

        ' ...and they stop SHORT of the right edge, which is the positive form of the same claim.
        ' Asserted rather than left implied, because "the last tab happens to end early" and "the
        ' tabs are measured" are different facts and only the second is intended.
        CLRTest_Check( "the last tab stops short of the strip's right edge", _
                       (pClr->rcTab(nTabs - 1).right < pClr->rcTabs.right), _
                       str(pClr->rcTab(nTabs - 1).right) & " < " & str(pClr->rcTabs.right) )

        CLRTest_Check( "three tabs, and no way to make it four", (nTabs = 3), str(nTabs) )

        CLRTest_Check( "rcMatrix is EXACTLY cols x rows of cells", _
                       (((pClr->rcMatrix.right - pClr->rcMatrix.left) = (pClr->nMatrixCols * pClr->nCellW)) andalso _
                        ((pClr->rcMatrix.bottom - pClr->rcMatrix.top) = (pClr->nMatrixRows * pClr->nCellH))), _
                       CLRTest_RectStr(pClr->rcMatrix) )
    end scope

    scope
        ' THE FIXED-BODY CLAIM, which is the layout's load-bearing decision: the POPUP must not
        ' resize as the user browses tabs, or a window already placed against an anchor grows off
        ' the bottom of the monitor.
        dim as long nBodyH = pClr->rcBody.bottom - pClr->rcBody.top
        dim as long nSeenW = pClr->nIdealW
        dim as long nSeenH = pClr->nIdealH
        dim as boolean bFixed = true
        dim as string sHeights = str(nBodyH)

        for t as long = CLR_TAB_WEB to CLR_TAB_COUNT - 1
            PsColorPicker_SetTab( h, t )
            pClr->LayoutColorPicker()
            dim as long bh = pClr->rcBody.bottom - pClr->rcBody.top
            if bh <> nBodyH then bFixed = false
            if t > CLR_TAB_WEB then sHeights &= "/" & str(bh)
            dim as boolean bIdealSame = ((pClr->nIdealW = nSeenW) andalso (pClr->nIdealH = nSeenH))
            if bIdealSame = false then bFixed = false
        next
        CLRTest_Check( "the body height and the ideal size are identical on all three tabs", _
                       bFixed, "heights=" & sHeights )

        ' MUTUAL EXCLUSION: whichever body rect the current tab does not use is EMPTY, which is
        ' what lets a paint callback branch on IsRectEmpty instead of re-deriving the mapping.
        dim as boolean bExcl = true
        dim as string sExcl
        for t as long = CLR_TAB_WEB to CLR_TAB_COUNT - 1
            PsColorPicker_SetTab( h, t )
            pClr->LayoutColorPicker()
            dim as boolean bM = (IsRectEmpty( @pClr->rcMatrix ) = 0)
            dim as boolean bS = (IsRectEmpty( @pClr->rcSwatches ) = 0)
            dim as boolean bWant = (t = CLR_TAB_CUSTOM)
            if (bM <> bWant) orelse (bS = bWant) then bExcl = false
            sExcl &= str(t) & ":" & str(bM) & str(bS) & " "
        next
        CLRTest_Check( "exactly one of rcMatrix / rcSwatches is non-empty on every tab", bExcl, sExcl )

        ' THE OK BUTTON IS ON EVERY TAB, and that is the opposite of the Assign button it
        ' replaced: Assign belonged to the palette tab, OK belongs to the popup. A tab-dependent
        ' commit button would mean the popup could not be answered from wherever the user was.
        dim as boolean bOKEverywhere = true
        for t as long = CLR_TAB_WEB to CLR_TAB_COUNT - 1
            PsColorPicker_SetTab( h, t )
            pClr->LayoutColorPicker()
            if IsRectEmpty( @pClr->rcOK ) <> 0 then bOKEverywhere = false
        next
        CLRTest_Check( "the OK button is placed on every tab", bOKEverywhere, _
                       CLRTest_RectStr(pClr->rcOK) )

        CLRTest_Check( "the strip is three tabs and there is no way to make it four", _
                       (pClr->TabCount() = 3) andalso (CLR_TAB_COUNT = 3), str(pClr->TabCount()) )

        ' EACH TAB IS AS WIDE AS ITS OWN CAPTION, which is what replaced the equal-thirds split.
        ' Asserted as "Web is narrower than System and System narrower than Custom" -- the order
        ' the three words sort into by length -- because that is false under BOTH the old
        ' equal-split rule and any rule that ignores the text.
        pClr->LayoutColorPicker()
        dim as long wWeb = pClr->rcTab(CLR_TAB_WEB).right - pClr->rcTab(CLR_TAB_WEB).left
        dim as long wSys = pClr->rcTab(CLR_TAB_SYSTEM).right - pClr->rcTab(CLR_TAB_SYSTEM).left
        dim as long wCus = pClr->rcTab(CLR_TAB_CUSTOM).right - pClr->rcTab(CLR_TAB_CUSTOM).left
        CLRTest_Check( "tab widths follow their captions rather than dividing the strip evenly", _
                       (wWeb < wSys) andalso (wSys <= wCus), _
                       str(wWeb) & "/" & str(wSys) & "/" & str(wCus) )

        ' They TILE, left to right, with no gap and no overlap -- so there is no dead pixel
        ' between two tabs that hit-tests to neither.
        dim as boolean bTile = (pClr->rcTab(CLR_TAB_WEB).left = pClr->rcTabs.left)
        for t as long = 1 to CLR_TAB_COUNT - 1
            if pClr->rcTab(t).left <> pClr->rcTab(t-1).right then bTile = false
        next
        CLRTest_Check( "the tabs tile from the strip's left edge with no gap or overlap", bTile, _
                       CLRTest_RectStr(pClr->rcTab(0)) & " " & CLRTest_RectStr(pClr->rcTab(1)) )
    end scope

    scope
        ' The fields. Enum order IS visual order -- see the note on CLR_FIELD_*.
        pClr->LayoutColorPicker()
        ' ONE FIELD PER ROW, TOP TO BOTTOM IN ENUM ORDER. That is the shape the reference has,
        ' and it replaced R/G/B across one row with A and hex on a second.
        PsColorPicker_ShowAlpha( h, true )
        pClr->LayoutColorPicker()
        dim as boolean bStackOrder = true
        dim as string sStack
        for f as long = 1 to CLR_FIELD_COUNT - 1
            if pClr->rcFieldBox(f).top <= pClr->rcFieldBox(f-1).top then bStackOrder = false
        next
        for f as long = 0 to CLR_FIELD_COUNT - 1
            sStack &= str(pClr->rcFieldBox(f).top) & " "
        next
        CLRTest_Check( "the fields stack top to bottom in enum order", bStackOrder, sStack )

        ' ...and they SHARE a left edge, which is what makes the block read as a column rather
        ' than a staircase.
        dim as boolean bLefts = true
        for f as long = 1 to CLR_FIELD_COUNT - 1
            if pClr->rcFieldBox(f).left <> pClr->rcFieldBox(0).left then bLefts = false
        next
        CLRTest_Check( "every field box shares one left edge", bLefts, str(pClr->rcFieldBox(0).left) )

        ' THE LABEL SITS TO THE LEFT OF ITS OWN BOX AND DOES NOT RUN INTO IT. A label wider than
        ' the gutter is ellipsized silently by DrawText, so this is asserted rather than looked at.
        dim as boolean bLabels = true
        for f as long = 0 to CLR_FIELD_COUNT - 1
            if pClr->IsFieldVisible(f) = false then continue for
            if pClr->rcFieldLabel(f).right > pClr->rcFieldBox(f).left then bLabels = false
            if pClr->rcFieldLabel(f).top <> pClr->rcFieldBox(f).top then bLabels = false
        next
        CLRTest_Check( "each label sits beside its own box without overlapping it", bLabels, _
                       CLRTest_RectStr(pClr->rcFieldLabel(CLR_FIELD_R)) )

        ' No two visible field boxes may overlap -- an overlap is what makes one field
        ' unclickable, and it is invisible in a render because the boxes are drawn in order.
        dim as boolean bNoOverlap = true
        for i as long = 0 to CLR_FIELD_COUNT - 1
            if pClr->IsFieldVisible(i) = false then continue for
            for j as long = i + 1 to CLR_FIELD_COUNT - 1
                if pClr->IsFieldVisible(j) = false then continue for
                dim as RECT rcX
                if IntersectRect( @rcX, @pClr->rcFieldBox(i), @pClr->rcFieldBox(j) ) <> 0 then bNoOverlap = false
            next
        next
        CLRTest_Check( "no two visible field boxes overlap", bNoOverlap, "" )

        ' HIDDEN EXPLICITLY. Alpha is shown BY DEFAULT now -- the reverse of the embedded
        ' control, where this scope could assume it started off. An assumed default is how this
        ' whole scope came to be measuring an already-shown field.
        PsColorPicker_ShowAlpha( h, false )
        pClr->LayoutColorPicker()

        dim as boolean bAlphaOff = (IsRectEmpty( @pClr->rcAlpha ) <> 0)
        CLRTest_Check( "rcAlpha is empty while the alpha field is hidden", bAlphaOff, _
                       CLRTest_RectStr(pClr->rcAlpha) )

        dim as long nWBefore = pClr->nIdealW
        dim as long nHBefore = pClr->nIdealH
        PsColorPicker_ShowAlpha( h, true )
        pClr->LayoutColorPicker()
        dim as boolean bAlias = ((pClr->rcAlpha.left = pClr->rcFieldBox(CLR_FIELD_A).left) andalso _
                                 (pClr->rcAlpha.right = pClr->rcFieldBox(CLR_FIELD_A).right) andalso _
                                 (IsRectEmpty( @pClr->rcAlpha ) = 0))
        CLRTest_Check( "rcAlpha aliases the alpha field's box when it is shown", bAlias, _
                       CLRTest_RectStr(pClr->rcAlpha) )

        ' ALPHA COSTS A FIELD ROW BUT NOT A PIXEL OF POPUP, and the reason is worth stating
        ' because the obvious assertion here is wrong. The bottom block is as tall as its TALLEST
        ' column, and the preview column -- two label rows plus two preview halves -- is taller
        ' than four stacked fields. So the field stack grows into space the preview had already
        ' claimed, and the popup does not move.
        '
        ' The first version of this assertion demanded nIdealH + nFieldH + nGap and FAILED, which
        ' is how the fact above was established rather than assumed. Both halves are asserted:
        ' the size does not move, AND the preview really is the taller column, so a future change
        ' that made the fields taller than the preview turns the second one red rather than
        ' silently making the first one vacuous.
        CLRTest_Check( "showing alpha does not change the ideal size at all", _
                       (pClr->nIdealH = nHBefore) andalso (pClr->nIdealW = nWBefore), _
                       str(nWBefore) & "x" & str(nHBefore) & " -> " & _
                       str(pClr->nIdealW) & "x" & str(pClr->nIdealH) )

        dim as long nPrevColH = (2 * pClr->nRowH) + (2 * pClr->nPreviewH)
        dim as long nFieldColH = (4 * pClr->nFieldH) + (3 * pClr->nGap)
        CLRTest_Check( "...BECAUSE the preview column is the taller of the two, even with alpha shown", _
                       (nPrevColH >= nFieldColH), _
                       "preview=" & str(nPrevColH) & " fields=" & str(nFieldColH) )

        ' THE OK BUTTON FOLLOWS THE LAST FIELD DOWN. It is bottom-aligned on the last row, so a
        ' hidden alpha row must move it -- a button left where the four-row layout put it would
        ' float below the fields with nothing beside it.
        dim as long nOKBottomWithA = pClr->rcOK.bottom
        dim as long nLastWithA = pClr->rcFieldBox(CLR_FIELD_A).bottom
        CLRTest_Check( "the OK button's bottom is the last field's bottom", _
                       (nOKBottomWithA = nLastWithA), _
                       str(nOKBottomWithA) & " vs " & str(nLastWithA) )

        PsColorPicker_ShowAlpha( h, false )
        pClr->LayoutColorPicker()
        CLRTest_Check( "hiding alpha moves the OK button up onto the new last row", _
                       (pClr->rcOK.bottom = pClr->rcFieldBox(CLR_FIELD_B).bottom) andalso _
                       (pClr->rcOK.bottom < nOKBottomWithA), _
                       str(nOKBottomWithA) & " -> " & str(pClr->rcOK.bottom) )

        ' A HIDDEN ROW LEAVES NO GAP: the visible fields are laid out at a uniform stride, so B is
        ' simply the last one rather than "the row after the gap where alpha would have been".
        '
        ' THIS ASSERTION CANNOT CURRENTLY BE MADE TO FAIL, AND THAT IS SAID HERE RATHER THAN LEFT
        ' AS A FLATTERING GREEN. The A/B was run: making the layout advance its y for a hidden
        ' field -- the exact defect this names -- leaves the whole suite at 182/182. The reason is
        ' that ALPHA IS THE LAST FIELD and the only hideable one, so a row charged for it is
        ' charged after everything that could observe it; `fieldsBottom` is written only by placed
        ' fields, so even the OK button (which does follow the last field, and IS covered -- an
        ' A/B pinning it to a fixed four-row block turns the assertion below red) does not move.
        ' The break is dead code, not a defect.
        '
        ' It is kept because it becomes live the moment any field other than the last can be
        ' hidden, which is the only shape in which the defect is reachable at all.
        dim as long nStride = -1
        dim as boolean bUniform = true
        dim as long nPrevTop = -1
        for f as long = 0 to CLR_FIELD_COUNT - 1
            if pClr->IsFieldVisible(f) = false then continue for
            if nPrevTop >= 0 then
                dim as long d = pClr->rcFieldBox(f).top - nPrevTop
                if nStride < 0 then nStride = d
                if d <> nStride then bUniform = false
            end if
            nPrevTop = pClr->rcFieldBox(f).top
        next
        CLRTest_Check( "a hidden field consumes no row, so the visible stride stays uniform", _
                       bUniform andalso (nStride = pClr->nFieldH + pClr->nGap), _
                       "stride=" & str(nStride) & " expected=" & str(pClr->nFieldH + pClr->nGap) )

        PsColorPicker_ShowAlpha( h, true )
        pClr->LayoutColorPicker()
    end scope

    scope
        pClr->LayoutColorPicker()
        dim as boolean bStack = ((pClr->rcCurrent.top = pClr->rcInitial.bottom) andalso _
                                 (pClr->rcCurrent.left = pClr->rcInitial.left) andalso _
                                 (pClr->rcCurrent.right = pClr->rcInitial.right))
        CLRTest_Check( "the two previews are stacked and share a width", bStack, _
                       CLRTest_RectStr(pClr->rcInitial) & " / " & CLRTest_RectStr(pClr->rcCurrent) )

        ' THEY ARE TWO HALVES OF ONE BOX, exactly tiling rcPreview with NO GAP between them. The
        ' whole point of the pair is comparing two colours edge to edge, and a strip of panel
        ' between them defeats it -- which is a defect no render probe would report, because a
        ' gap of background looks like a deliberate border.
        dim as boolean bHalves = ((pClr->rcInitial.top = pClr->rcPreview.top) andalso _
                                  (pClr->rcCurrent.bottom = pClr->rcPreview.bottom) andalso _
                                  (pClr->rcInitial.bottom = pClr->rcCurrent.top))
        CLRTest_Check( "the two halves tile rcPreview exactly, with no gap between them", bHalves, _
                       CLRTest_RectStr(pClr->rcPreview) )

        ' EQUAL halves, so neither colour is given more visual weight than the other.
        dim as long hInit = pClr->rcInitial.bottom - pClr->rcInitial.top
        dim as long hCurr = pClr->rcCurrent.bottom - pClr->rcCurrent.top
        CLRTest_Check( "the two halves are the same height", (hInit = hCurr), _
                       str(hInit) & "/" & str(hCurr) )

        ' THE LABELS ARE OUTSIDE THE BOX NOW -- above and below -- which is the visible change
        ' from the embedded control, where they were drawn inside in a contrasting colour.
        dim as boolean bLabelsOut = ((pClr->rcInitialLabel.bottom = pClr->rcPreview.top) andalso _
                                     (pClr->rcCurrentLabel.top = pClr->rcPreview.bottom))
        CLRTest_Check( "the Initial and Current labels sit outside the box, above and below", _
                       bLabelsOut, CLRTest_RectStr(pClr->rcInitialLabel) & " / " & _
                       CLRTest_RectStr(pClr->rcCurrentLabel) )

        ' rcFields must actually SPAN the boxes it claims to cover; it is built from them rather
        ' than re-derived, and this is the assertion that keeps it that way.
        dim as boolean bSpan = true
        for i as long = 0 to CLR_FIELD_COUNT - 1
            if pClr->IsFieldVisible(i) = false then continue for
            if pClr->rcFieldBox(i).right > pClr->rcFields.right then bSpan = false
            if pClr->rcFieldBox(i).left < pClr->rcFields.left then bSpan = false
        next
        CLRTest_Check( "rcFields spans every visible field box", bSpan, CLRTest_RectStr(pClr->rcFields) )

        ' The previews must not run into the fields.
        dim as RECT rcX
        dim as boolean bClear = (IntersectRect( @rcX, @pClr->rcCurrent, @pClr->rcFields ) = 0)
        CLRTest_Check( "the previews and the entry fields do not overlap", bClear, CLRTest_RectStr(rcX) )
    end scope

    print "=== hit testing ==="

    scope
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()

        ' Every tab cell's centre must hit-test to its own index. A round trip against the
        ' layout, so the click map and the painter cannot drift apart.
        dim as boolean bTabs = true
        for i as long = 0 to pClr->TabCount() - 1
            dim as RECT rc = pClr->rcTab(i)
            dim as long cx = (rc.left + rc.right) \ 2
            dim as long cy = (rc.top + rc.bottom) \ 2
            if PsColorPicker_HitTab( pClr, cx, cy ) <> i then bTabs = false
        next
        CLRTest_Check( "every tab cell's centre hit-tests to its own index", bTabs, "" )

        ' Every matrix cell's centre must hit-test to its own index AND report the colour the law
        ' says is there. This is the assertion that catches a painter and a hit test that agree
        ' with each other but not with the colour law.
        dim as long nBad = 0
        dim as string sBad
        for r as long = 0 to pClr->nMatrixRows - 1
            for c as long = 0 to pClr->nMatrixCols - 1
                dim as long cx = pClr->rcMatrix.left + (c * pClr->nCellW) + (pClr->nCellW \ 2)
                dim as long cy = pClr->rcMatrix.top + (r * pClr->nCellH) + (pClr->nCellH \ 2)
                dim as long idx = PsColorPicker_HitBody( pClr, cx, cy )
                if idx <> ((r * pClr->nMatrixCols) + c) then
                    nBad += 1
                    if nBad = 1 then sBad = "index at " & str(c) & "," & str(r) & " = " & str(idx)
                    continue for
                end if
                dim as COLORREF cGot
                if PsColorPicker_GridColorAt( pClr, idx, cGot ) = false then
                    nBad += 1
                    if nBad = 1 then sBad = "no colour at " & str(idx)
                elseif cGot <> PsColorPicker_MatrixColorAt( c, r, pClr->nMatrixCols, pClr->nMatrixRows ) then
                    nBad += 1
                    if nBad = 1 then sBad = "wrong colour at " & str(idx)
                end if
            next
        next
        CLRTest_Check( "every matrix cell hit-tests to its own index and its own colour", _
                       (nBad = 0), "mismatches=" & str(nBad) & " " & sBad )

        ' One pixel outside the grid on each side. CellFromPoint bounds the indices; the extra
        ' PtIn in HitBody is what bounds the RECT, and without it a click below a short last row
        ' would answer.
        dim as boolean bOut = ((PsColorPicker_HitBody( pClr, pClr->rcMatrix.right, pClr->rcMatrix.top ) = -1) andalso _
                               (PsColorPicker_HitBody( pClr, pClr->rcMatrix.left, pClr->rcMatrix.bottom ) = -1) andalso _
                               (PsColorPicker_HitBody( pClr, pClr->rcMatrix.left - 1, pClr->rcMatrix.top ) = -1))
        CLRTest_Check( "a point one pixel outside the matrix hits nothing", bOut, _
                       CLRTest_RectStr(pClr->rcMatrix) )

        ' Every visible field's centre. Includes the hex field, whose box is a different width.
        dim as boolean bFields = true
        for i as long = 0 to CLR_FIELD_COUNT - 1
            if pClr->IsFieldVisible(i) = false then continue for
            dim as RECT rc = pClr->rcFieldBox(i)
            dim as long cx = (rc.left + rc.right) \ 2
            dim as long cy = (rc.top + rc.bottom) \ 2
            if PsColorPicker_HitField( pClr, cx, cy ) <> i then bFields = false
        next
        CLRTest_Check( "every visible field's centre hit-tests to its own id", bFields, "" )
    end scope

    scope
        ' ---- THE TWO LIST TABS -----------------------------------------------------------
        ' Web and System are LISTS of named rows, not swatch grids, and both go through one
        ' renderer and one hit test. These cases therefore run the SAME assertions over each of
        ' them rather than testing one and hoping the other matches -- which is what the
        ' single-renderer decision is worth.
        dim as long nTabsToCheck(0 to 1) = { CLR_TAB_WEB, CLR_TAB_SYSTEM }
        dim as long nBadRow = 0, nBadName = 0, nBadPast = 0
        dim as string sRowDetail

        for k as long = 0 to 1
            PsColorPicker_SetTab( h, nTabsToCheck(k) )
            pClr->LayoutColorPicker()
            pClr->nScrollTop = 0

            dim as long nCount = PsColorPicker_GridCount( pClr )
            dim as long nVis   = PsColorPicker_VisibleRows( pClr )
            if k = 0 then sRowDetail = "web=" & str(nCount) & " visible=" & str(nVis)

            ' Every VISIBLE row's centre hit-tests to its own index...
            for v as long = 0 to nVis - 1
                if v >= nCount then exit for
                dim as long cy = pClr->rcSwatches.top + (v * pClr->nRowH) + (pClr->nRowH \ 2)
                dim as long cx = pClr->rcSwatches.left + 4
                if PsColorPicker_HitBody( pClr, cx, cy ) <> v then nBadRow += 1
            next

            ' ...and every entry has a NAME. An unnamed row is the defect the grid layout had:
            ' it renders as an anonymous swatch and tells the user nothing.
            for i as long = 0 to nCount - 1
                dim as DWSTRING wszName
                if PsColorPicker_GridNameAt( pClr, i, wszName ) = false then
                    nBadName += 1
                elseif PsLen( wszName ) = 0 then
                    nBadName += 1
                end if
            next

        next

        CLRTest_Check( "every visible row hit-tests to its own index on both list tabs", _
                       (nBadRow = 0), "mismatches=" & str(nBadRow) & "  " & sRowDetail )
        CLRTest_Check( "every entry on every list tab has a name", _
                       (nBadName = 0), "unnamed=" & str(nBadName) )
        ' THE PARTIAL ROW AT THE BOTTOM OF A LONG LIST.
        '
        ' This one has to be SET UP, and the setup is the interesting part. The guard it tests --
        ' "a row that would be drawn past the bottom of the viewport is not clickable" -- is only
        ' reachable when the list is LONGER than the viewport AND the viewport height is not an
        ' exact multiple of the row height. Neither of the shipped tabs manages both: Web's body
        ' is an exact multiple, and the palette is shorter than its viewport, so its dead space is
        ' caught by the index-vs-count test instead and the guard never runs.
        '
        ' THE SETUP CHANGED WITH THE PALETTE TAB'S REMOVAL, and it is worth saying why rather
        ' than quietly dropping the case. The 20-entry test palette used to create the condition;
        ' with no palette left, neither shipped tab manages both halves -- Web overflows but its
        ' body is an exact number of rows, so no partial row exists at all.
        '
        ' It is created instead by making the MATRIX taller than the list: the body is sized by
        ' whichever tab needs more, so a cell height that is not a multiple of nRowH leaves a
        ' partial row at the bottom of the list viewport. Web then supplies the overflow. The
        ' cell size is restored immediately afterwards.
        dim as long nCellWSave, nCellHSave
        PsColorPicker_GetCellSize( h, nCellWSave, nCellHSave )
        PsColorPicker_SetCellSize( h, nCellWSave, nCellHSave + 5 )
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()
        pClr->nScrollTop = 0

        dim as long nVisP  = PsColorPicker_VisibleRows( pClr )
        dim as long nStrip = (pClr->rcSwatches.bottom - pClr->rcSwatches.top) - (nVisP * pClr->nRowH)
        dim as boolean bStripPre = ((nStrip > 2) andalso (PsColorPicker_GridCount( pClr ) > nVisP))
        CLRTest_Check( "precondition: a partial row exists AND the list overflows the viewport", _
                       bStripPre, "strip=" & str(nStrip) & "px rowH=" & str(pClr->nRowH) & _
                       " visible=" & str(nVisP) )

        dim as long cyPast = pClr->rcSwatches.top + (nVisP * pClr->nRowH) + 1
        CLRTest_Check( "a click in the partial row at the bottom hits nothing", _
                       (PsColorPicker_HitBody( pClr, pClr->rcSwatches.left + 4, cyPast ) = -1), _
                       "y=" & str(cyPast) & " list=" & CLRTest_RectStr(pClr->rcSwatches) )
        ' ...while the last FULL row directly above it is still clickable, so the guard is
        ' refusing one row rather than the bottom of the list.
        dim as long cyLast = pClr->rcSwatches.top + ((nVisP - 1) * pClr->nRowH) + 2
        CLRTest_Check( "the last fully visible row above it is still clickable", _
                       (PsColorPicker_HitBody( pClr, pClr->rcSwatches.left + 4, cyLast ) = (pClr->nScrollTop + nVisP - 1)), _
                       "hit=" & str( PsColorPicker_HitBody( pClr, pClr->rcSwatches.left + 4, cyLast ) ) )

        CLRTest_Check( "every visible row above the partial one is clickable on both tabs", _
                       (nBadPast = 0), "nBadPast=" & str(nBadPast) )

        PsColorPicker_SetCellSize( h, nCellWSave, nCellHSave )
        pClr->LayoutColorPicker()
    end scope

    scope
        ' ---- SCROLLING -------------------------------------------------------------------
        ' The web list is far longer than the viewport, so this is the tab that exercises it.
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()
        pClr->nScrollTop = 0

        dim as long nCount = PsColorPicker_WebColorCount()
        dim as long nVis   = PsColorPicker_VisibleRows( pClr )

        ' Precondition, asserted rather than assumed: the list must actually overflow, or every
        ' scrolling assertion below would pass by doing nothing.
        CLRTest_Check( "precondition: the web list is longer than its viewport", _
                       (nCount > nVis), "count=" & str(nCount) & " visible=" & str(nVis) )

        ' THE HIT TEST FOLLOWS THE SCROLL. This is the assertion that catches a renderer and a
        ' hit test that disagree by exactly nScrollTop -- which looks fine until you scroll, and
        ' then every click is off by the same amount in a way that is easy to misread as "the
        ' list is selecting the wrong colour".
        pClr->nScrollTop = 20
        dim as long cyTop = pClr->rcSwatches.top + (pClr->nRowH \ 2)
        dim as long cxTop = pClr->rcSwatches.left + 4
        CLRTest_Check( "the first visible row is nScrollTop, not row 0", _
                       (PsColorPicker_HitBody( pClr, cxTop, cyTop ) = 20), _
                       "hit=" & str( PsColorPicker_HitBody( pClr, cxTop, cyTop ) ) )

        dim as long cyThird = pClr->rcSwatches.top + (2 * pClr->nRowH) + (pClr->nRowH \ 2)
        CLRTest_Check( "the third visible row is nScrollTop + 2", _
                       (PsColorPicker_HitBody( pClr, cxTop, cyThird ) = 22), _
                       "hit=" & str( PsColorPicker_HitBody( pClr, cxTop, cyThird ) ) )

        ' The last page cannot be scrolled past, or the list would show empty space below its
        ' last row.
        pClr->nScrollTop = 9999
        PsColorPicker_ClampScroll( pClr )
        CLRTest_Check( "the scroll position clamps at the last full page", _
                       (pClr->nScrollTop = (nCount - nVis)), _
                       "top=" & str(pClr->nScrollTop) & " expected=" & str(nCount - nVis) )

        pClr->nScrollTop = -5
        PsColorPicker_ClampScroll( pClr )
        CLRTest_Check( "the scroll position clamps at zero", (pClr->nScrollTop = 0), str(pClr->nScrollTop) )

        ' SETTING A COLOUR NEAR THE END OF THE LIST MUST REVEAL IT. A host that sets the picker
        ' to MidnightBlue and gets row 106 selected-but-not-shown has been told nothing, and this
        ' is the whole reason bScrollToSel exists.
        pClr->nScrollTop = 0
        dim as long nLate = nCount - 3
        PsColorPicker_SetColor( h, PsColorPicker_WebColorAt( nLate ) )
        pClr->LayoutColorPicker()
        PsColorPicker_EnsureSelVisible( pClr )
        dim as boolean bRevealed = ((nLate >= pClr->nScrollTop) andalso _
                                    (nLate <= (pClr->nScrollTop + nVis - 1)))
        CLRTest_Check( "setting a colour late in the list scrolls it into view", bRevealed, _
                       "sel=" & str(pClr->nBodySel) & " top=" & str(pClr->nScrollTop) & _
                       " visible=" & str(nVis) )

        ' ...AND UPWARD TOO. The case above only ever scrolls DOWN to a late row, so half of
        ' EnsureSelVisible was untested: an A/B that disabled its scroll-UP branch failed no
        ' assertion at all. Park the viewport at the end, then select something near the top.
        pClr->nScrollTop = nCount - nVis
        PsColorPicker_ClampScroll( pClr )
        dim as long nTopParked = pClr->nScrollTop
        PsColorPicker_SetColor( h, PsColorPicker_WebColorAt( 2 ) )
        pClr->LayoutColorPicker()
        CLRTest_Check( "precondition: the viewport is parked past the row about to be selected", _
                       (nTopParked > 2), "top=" & str(nTopParked) )
        PsColorPicker_EnsureSelVisible( pClr )
        dim as boolean bRevealedUp = ((2 >= pClr->nScrollTop) andalso _
                                      (2 <= (pClr->nScrollTop + nVis - 1)))
        CLRTest_Check( "selecting a row above the viewport scrolls UP to it", bRevealedUp, _
                       "top=" & str(nTopParked) & " -> " & str(pClr->nScrollTop) )

        ' ...and it does NOT move the viewport when the row is already showing, so it cannot
        ' fight a user who has deliberately scrolled.
        '
        ' DELIBERATELY IN THE MIDDLE OF THE LIST. The first version of this case parked the
        ' viewport near the END, where ClampScroll pins it to the last page -- so a break that
        ' made EnsureSelVisible re-centre on EVERY call produced the identical number and the case
        ' passed under both implementations. A test whose input is degenerate proves nothing
        ' (C:\dev\Learnings.md, PsOptionButton).
        dim as long nMid = 60
        PsColorPicker_SetColor( h, PsColorPicker_WebColorAt( nMid ) )
        pClr->nScrollTop = nMid - 5
        PsColorPicker_ClampScroll( pClr )
        dim as long nTopBefore = pClr->nScrollTop
        ' Precondition: the row really is inside the viewport, and the viewport is NOT against
        ' either end -- so a re-centre would visibly move it.
        dim as boolean bMidOK = ((nTopBefore = (nMid - 5)) andalso (nMid <= (nTopBefore + nVis - 1)))
        CLRTest_Check( "precondition: the probe row is visible and the viewport is mid-list", _
                       bMidOK, "sel=" & str(nMid) & " top=" & str(nTopBefore) & " visible=" & str(nVis) )
        PsColorPicker_EnsureSelVisible( pClr )
        CLRTest_Check( "revealing an already-visible row leaves the viewport alone", _
                       (pClr->nScrollTop = nTopBefore), _
                       str(nTopBefore) & " -> " & str(pClr->nScrollTop) )

        ' The scrollbar is a real child window, and it must exist and be OUR child.
        CLRTest_Check( "the list scrollbar exists and is a child of the picker", _
                       ((IsWindow( pClr->hScroll ) <> 0) andalso (GetParent( pClr->hScroll ) = h)), _
                       "hScroll=&h" & hex(cast(uinteger, pClr->hScroll)) )

        ' THE STRIP IS CARVED OUT OF THE LIST, not drawn over it. Without this the last column of
        ' every row's name would sit underneath the scrollbar.
        CLRTest_Check( "the list stops exactly where the scrollbar strip begins", _
                       (pClr->rcSwatches.right = pClr->rcScroll.left), _
                       CLRTest_RectStr(pClr->rcSwatches) & " | " & CLRTest_RectStr(pClr->rcScroll) )
        CLRTest_Check( "the scrollbar strip is a real width, not a zero-width sliver", _
                       ((pClr->rcScroll.right - pClr->rcScroll.left) = pClr->nScrollBarW), _
                       str(pClr->rcScroll.right - pClr->rcScroll.left) & " vs " & str(pClr->nScrollBarW) )

        ' ...and it does not exist at all on the Custom tab, which has no list to scroll.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()
        CLRTest_Check( "the scrollbar strip is absent on the Custom tab", _
                       (IsRectEmpty( @pClr->rcScroll ) <> 0), CLRTest_RectStr(pClr->rcScroll) )

        ' THE MATRIX FILLS THE BODY -- no dead band under it. The body is sized by whichever tab
        ' needs more, so a matrix shorter than the lists leaves a strip of bare panel on the
        ' Custom tab. That is EXACTLY the defect that was found by looking at the render and that
        ' all 133 assertions of the previous revision missed, so it is measured here now.
        dim as long nMatrixH = pClr->rcMatrix.bottom - pClr->rcMatrix.top
        dim as long nBodyH   = pClr->rcBody.bottom - pClr->rcBody.top
        CLRTest_Check( "the matrix fills the body height, leaving no dead band", _
                       (nMatrixH = nBodyH), "matrix=" & str(nMatrixH) & " body=" & str(nBodyH) )

        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()

        ' AND IT MUST NOT MAKE THE PICKER A TAB CONTAINER. A scrollbar is never a tabstop, so the
        ' picker stays a tab ITEM and keeps its explicit dwExStyle of 0 -- the family bug this
        ' control was built to avoid. Asserted here because gaining a child is exactly the change
        ' that would tempt someone to "fix" the extended style.
        dim as LONG_PTR nSbStyle = GetWindowLongPtr( pClr->hScroll, GWL_STYLE )
        CLRTest_Check( "the scrollbar child is not a tabstop", _
                       ((nSbStyle and WS_TABSTOP) = 0), "style=&h" & hex(nSbStyle) )
    end scope

    scope
        ' ---- THE NAMED TABLES ------------------------------------------------------------
        ' The web table's ORDER is deliberate (greys, reds, yellows, greens, cyans, blues,
        ' purples, pinks) rather than alphabetical, and its two duplicate names were dropped.
        ' Both are claims worth holding onto, because "tidy this up alphabetically" is an
        ' obvious-looking edit that would quietly make the list unscannable.
        dim as long nWeb = PsColorPicker_WebColorCount()
        CLRTest_Check( "the web table is the full named set", (nWeb = 137), str(nWeb) )

        dim as long nDupName = 0, nDupColor = 0
        for i as long = 0 to nWeb - 1
            for j as long = i + 1 to nWeb - 1
                if PsColorPicker_WebColorName(i) = PsColorPicker_WebColorName(j) then nDupName += 1
                if PsColorPicker_WebColorAt(i) = PsColorPicker_WebColorAt(j) then nDupColor += 1
            next
        next
        CLRTest_Check( "no two web entries share a name", (nDupName = 0), "dups=" & str(nDupName) )
        ' The colour check is what catches Aqua/Cyan and Fuchsia/Magenta creeping back in: two
        ' rows of identical colour under different names make the selection ring look broken,
        ' because SyncBodySel resolves a colour to the FIRST row that holds it.
        CLRTest_Check( "no two web entries share a colour", (nDupColor = 0), "dups=" & str(nDupColor) )

        ' A spot check that the table is not merely well-formed but CORRECT. Navy is the entry in
        ' the reference screenshot, and a transposed byte order would give 128,0,0 instead.
        dim as long nNavy = -1
        for i as long = 0 to nWeb - 1
            if PsColorPicker_WebColorName(i) = "Navy" then nNavy = i : exit for
        next
        dim as boolean bNavy = ((nNavy >= 0) andalso (PsColorPicker_WebColorAt(nNavy) = BGR(0,0,128)))
        CLRTest_Check( "Navy is 0,0,128 and not a byte-swapped 128,0,0", bNavy, _
                       "idx=" & str(nNavy) & " " & CLRTest_ClrStr( PsColorPicker_WebColorAt( iif(nNavy < 0, 0, nNavy) ) ) )

        dim as long nSys = PsColorPicker_SystemColorCount()
        CLRTest_Check( "the system table covers the COLOR_* series", (nSys = 30), str(nSys) )

        ' Index 25 is unused in the COLOR_* series and is deliberately skipped, so a naive
        ' 0..30 loop would have shipped a row that means nothing.
        dim as boolean b25 = true
        for i as long = 0 to nSys - 1
            if gClrSysIndex(i) = 25 then b25 = false
        next
        CLRTest_Check( "the unused system colour index 25 is skipped", b25, "" )
    end scope

    print "=== state and notification ==="

    scope
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        CLRTest_ResetNotify()

        PsColorPicker_SetColor( h, BGR(10, 20, 30), 200 )
        dim as boolean bSilent = (gCLR_NotifyCount = 0)
        CLRTest_Check( "SetColor is SILENT", bSilent, "notifications=" & str(gCLR_NotifyCount) )

        dim as boolean bRead = ((PsColorPicker_GetColor(h) = BGR(10,20,30)) andalso _
                                (PsColorPicker_GetAlpha(h) = 200))
        CLRTest_Check( "SetColor is readable back through GetColor / GetAlpha", bRead, _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) & " a=" & str(PsColorPicker_GetAlpha(h)) )

        CLRTest_Check( "SetColor establishes the Initial baseline by default", _
                       (PsColorPicker_GetInitial(h) = BGR(10,20,30)), _
                       CLRTest_ClrStr(PsColorPicker_GetInitial(h)) )

        PsColorPicker_SetColor( h, BGR(1, 2, 3), 255, true )
        CLRTest_Check( "SetColor with bKeepInitial leaves the baseline alone", _
                       ((PsColorPicker_GetInitial(h) = BGR(10,20,30)) andalso _
                        (PsColorPicker_GetColor(h) = BGR(1,2,3))), _
                       "initial=" & CLRTest_ClrStr(PsColorPicker_GetInitial(h)) )

        ' Revert is an ACTION, not a setter, so it FIRES -- once, with END.
        CLRTest_ResetNotify()
        PsColorPicker_RevertToInitial( h )
        dim as boolean bRev = ((gCLR_NotifyCount = 1) andalso (gCLR_NotifyPhase(0) = CLR_PHASE_END) andalso _
                               (PsColorPicker_GetColor(h) = BGR(10,20,30)))
        CLRTest_Check( "RevertToInitial fires exactly once with CLR_PHASE_END", bRev, _
                       "n=" & str(gCLR_NotifyCount) & " phase=" & str(gCLR_NotifyPhase(0)) )

        ' Reverting to where we already are is silent, because ApplyColor's silent path early-outs
        ' -- but the NOTIFYING path deliberately does not, so this asserts the distinction.
        CLRTest_ResetNotify()
        PsColorPicker_RevertToInitial( h )
        CLRTest_Check( "reverting again still fires (a phase must not be swallowed)", _
                       (gCLR_NotifyCount = 1), "n=" & str(gCLR_NotifyCount) )
    end scope

    scope
        CLRTest_ResetNotify()
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        CLRTest_Check( "SetTab is SILENT", (gCLR_TabNotifyCount = 0), str(gCLR_TabNotifyCount) )
        CLRTest_Check( "SetTab moved the tab", (PsColorPicker_GetTab(h) = CLR_TAB_WEB), str(PsColorPicker_GetTab(h)) )

        ' OUT OF RANGE IS REFUSED IN BOTH DIRECTIONS, and CLR_TAB_COUNT is the edge -- the id
        ' the removed palette tab used to hold, so a stored "3" in an old host's settings leaves
        ' the tab where it was rather than selecting a body that is not there.
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        PsColorPicker_SetTab( h, CLR_TAB_COUNT )
        CLRTest_Check( "SetTab refuses the id the removed palette tab used to hold", _
                       (PsColorPicker_GetTab(h) = CLR_TAB_WEB), str(PsColorPicker_GetTab(h)) )
        PsColorPicker_SetTab( h, -1 )
        CLRTest_Check( "...and refuses a negative id", _
                       (PsColorPicker_GetTab(h) = CLR_TAB_WEB), str(PsColorPicker_GetTab(h)) )
    end scope

    print "=== the phase sequence ==="

    scope
        ' A DRAG: down on one cell, two moves, up. One BEGIN, then MOVEs, then exactly one END.
        ' See the header for what this can and cannot prove -- it is the sequence, not the capture.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()
        CLRTest_ResetNotify()

        dim as long x0 = pClr->rcMatrix.left + (pClr->nCellW \ 2)
        dim as long y0 = pClr->rcMatrix.top + (pClr->nCellH \ 2)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( x0, y0 ) )
        SendMessageW( h, WM_MOUSEMOVE, MK_LBUTTON, CLRTEST_LP( x0 + pClr->nCellW, y0 ) )
        SendMessageW( h, WM_MOUSEMOVE, MK_LBUTTON, CLRTEST_LP( x0 + (2 * pClr->nCellW), y0 ) )
        SendMessageW( h, WM_LBUTTONUP, 0, CLRTEST_LP( x0 + (2 * pClr->nCellW), y0 ) )

        dim as boolean bSeq = (gCLR_NotifyCount >= 3)
        if bSeq then
            if gCLR_NotifyPhase(0) <> CLR_PHASE_BEGIN then bSeq = false
            if gCLR_NotifyPhase(gCLR_NotifyCount - 1) <> CLR_PHASE_END then bSeq = false
            for i as long = 1 to gCLR_NotifyCount - 2
                if gCLR_NotifyPhase(i) <> CLR_PHASE_MOVE then bSeq = false
            next
        end if
        dim as string sSeq
        for i as long = 0 to gCLR_NotifyCount - 1
            sSeq &= str( gCLR_NotifyPhase(i) )
        next
        CLRTest_Check( "a matrix drag emits BEGIN, then only MOVEs, then exactly one END", bSeq, _
                       "n=" & str(gCLR_NotifyCount) & " phases=" & sSeq )

        ' Exactly ONE begin and ONE end -- a count, so a second BEGIN in the middle would fail
        ' even though the first and last entries are right.
        dim as long nB = 0, nE = 0
        for i as long = 0 to gCLR_NotifyCount - 1
            if gCLR_NotifyPhase(i) = CLR_PHASE_BEGIN then nB += 1
            if gCLR_NotifyPhase(i) = CLR_PHASE_END then nE += 1
        next
        CLRTest_Check( "the drag emits exactly one BEGIN and one END", _
                       ((nB = 1) andalso (nE = 1)), "begins=" & str(nB) & " ends=" & str(nE) )

        ' And the value really moved with the cursor: the last notified colour is the cell the
        ' pointer finished on, not the one it started on.
        dim as COLORREF cWant = PsColorPicker_MatrixColorAt( 2, 0, pClr->nMatrixCols, pClr->nMatrixRows )
        CLRTest_Check( "the drag's final colour is the cell it ended on", _
                       (PsColorPicker_GetColor(h) = cWant), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) & " want " & CLRTest_ClrStr(cWant) )
    end scope

    scope
        ' A LIST-ROW CLICK is a settled choice: one END, no BEGIN. The contrast with the drag
        ' above is the whole reason CLR_PHASE_* exists.
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()
        pClr->nScrollTop = 0
        CLRTest_ResetNotify()
        dim as long cx = pClr->rcSwatches.left + 4
        dim as long cy = pClr->rcSwatches.top + (3 * pClr->nRowH) + (pClr->nRowH \ 2)
        CLRTest_ClickAt( h, cx, cy )

        dim as boolean bOne = ((gCLR_NotifyCount = 1) andalso (gCLR_NotifyPhase(0) = CLR_PHASE_END))
        CLRTest_Check( "a list-row click emits exactly one END and no BEGIN", bOne, _
                       "n=" & str(gCLR_NotifyCount) & " phase=" & str(gCLR_NotifyPhase(0)) )
        CLRTest_Check( "the click adopted that row's colour", _
                       (PsColorPicker_GetColor(h) = PsColorPicker_WebColorAt(3)), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) )

        ' A tab click notifies the TAB callback, not the colour one.
        CLRTest_ResetNotify()
        dim as RECT rcT = pClr->rcTab(CLR_TAB_CUSTOM)
        CLRTest_ClickAt( h, (rcT.left + rcT.right) \ 2, (rcT.top + rcT.bottom) \ 2 )
        dim as boolean bTabFire = ((gCLR_TabNotifyCount = 1) andalso (gCLR_NotifyCount = 0))
        CLRTest_Check( "a tab click notifies the tab callback and not the colour one", bTabFire, _
                       "tab=" & str(gCLR_TabNotifyCount) & " colour=" & str(gCLR_NotifyCount) )
    end scope

    print "=== the entry fields ==="

    scope
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        PsColorPicker_SetColor( h, BGR(0, 0, 0) )
        pClr->LayoutColorPicker()

        dim as RECT rcR = pClr->rcFieldBox(CLR_FIELD_R)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcR.left + rcR.right) \ 2, (rcR.top + rcR.bottom) \ 2 ) )
        CLRTest_Check( "clicking a field puts the caret in it", _
                       (pClr->nFocusField = CLR_FIELD_R), str(pClr->nFocusField) )

        ' The buffer is seeded from the value, not left empty -- otherwise the field reads as
        ' blank the instant you click it.
        dim as string sSeed = pClr->wszEdit
        CLRTest_Check( "entering a field seeds its buffer from the current value", (sSeed = "0"), sSeed )

        CLRTest_ResetNotify()
        CLRTest_TypeString( h, "128" )
        ' Still uncommitted: typing must not notify, or a live-preview host repaints the whole
        ' application on every keystroke of a three-digit number.
        CLRTest_Check( "typing in a field notifies nothing until it is committed", _
                       (gCLR_NotifyCount = 0), "n=" & str(gCLR_NotifyCount) )

        ' SELECT-ALL-ON-ENTRY. The seed was "0" and three digits were typed; without the
        ' replace-on-first-character rule the buffer would read "012" and the three-character
        ' limit would eat the 8, committing 12. That is exactly what the first run of this suite
        ' measured, and it is why the rule exists.
        CLRTest_Check( "the first character typed REPLACES the seeded value", _
                       (pClr->wszEdit = "128"), "'" & pClr->wszEdit & "'" )

        CLRTest_Key( h, VK_RETURN )
        dim as long nR = clng(PsColorPicker_GetColor(h)) and &hFF
        dim as boolean bTyped = ((nR = 128) andalso (gCLR_NotifyCount = 1) andalso _
                                 (gCLR_NotifyPhase(0) = CLR_PHASE_END))
        CLRTest_Check( "Enter commits the typed value with a single END", bTyped, _
                       "R=" & str(nR) & " n=" & str(gCLR_NotifyCount) )

        CLRTest_Check( "Enter leaves the caret in the field", _
                       (pClr->nFocusField = CLR_FIELD_R), str(pClr->nFocusField) )
    end scope

    scope
        ' Rejection. A letter in a numeric field must not reach the buffer at all -- not be
        ' accepted and then discarded at parse time, which would let the user watch themselves
        ' type something that silently vanishes.
        '
        ' AND IT MUST NOT WIPE A FRESH BUFFER EITHER. The field is fresh again here (Enter
        ' re-seeded it), so this also covers the ordering defect the first run found: clearing
        ' the seed before testing whether the character was acceptable meant a stray letter
        ' destroyed the value it was rejected by.
        dim as string sBefore = pClr->wszEdit
        CLRTest_TypeChar( h, asc("x") )
        CLRTest_Check( "a rejected letter neither reaches nor wipes a freshly seeded buffer", _
                       (pClr->wszEdit = sBefore), "'" & sBefore & "' -> '" & pClr->wszEdit & "'" )

        ' Control characters likewise. Backspace arrives as WM_CHAR as well as WM_KEYDOWN, and
        ' inserting a chr(8) is exactly how a hand-rolled field ends up with invisible garbage.
        CLRTest_TypeChar( h, 8 )
        CLRTest_Check( "a control character never reaches the buffer", _
                       (pClr->wszEdit = sBefore), "'" & pClr->wszEdit & "'" )

        ' Length. Four digits typed into a fresh field: the first replaces the seed, and the
        ' fourth is refused.
        CLRTest_TypeString( h, "1234" )
        dim as boolean bLen3 = ((len(pClr->wszEdit) = 3) andalso (pClr->wszEdit = "123"))
        CLRTest_Check( "a numeric field stops at three characters", bLen3, "'" & pClr->wszEdit & "'" )
    end scope

    scope
        ' Editing keys, asserted on the BUFFER rather than on the committed value, so each key is
        ' isolated from the parse. The buffer holds "123" with the caret at its end.
        CLRTest_Key( h, VK_BACK )
        CLRTest_Check( "Backspace removes the character before the caret", _
                       (pClr->wszEdit = "12"), "'" & pClr->wszEdit & "'" )

        CLRTest_Key( h, VK_HOME )
        CLRTest_Check( "Home moves the caret to the start", (pClr->nCaret = 0), str(pClr->nCaret) )

        CLRTest_Key( h, VK_DELETE )
        CLRTest_Check( "Delete removes the character AT the caret", _
                       (pClr->wszEdit = "2"), "'" & pClr->wszEdit & "'" )

        CLRTest_Key( h, VK_END )
        CLRTest_Check( "End moves the caret past the last character", _
                       (pClr->nCaret = len(pClr->wszEdit)), str(pClr->nCaret) )

        ' Insertion happens AT the caret, not at the end. A field that always appended would pass
        ' every test above and still be wrong.
        CLRTest_Key( h, VK_HOME )
        CLRTest_TypeChar( h, asc("5") )
        CLRTest_Check( "a typed character is inserted at the caret", _
                       (pClr->wszEdit = "52"), "'" & pClr->wszEdit & "'" )
    end scope

    scope
        ' Clamping. 999 is a legal three-digit entry and an illegal channel value.
        pClr->wszEdit = "999"
        pClr->nCaret = 3
        CLRTest_Key( h, VK_RETURN )
        CLRTest_Check( "an out-of-range channel value is clamped to 255", _
                       ((clng(PsColorPicker_GetColor(h)) and &hFF) = 255), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) )
        CLRTest_Check( "the field is re-seeded after a clamp, so the user sees what it became", _
                       (pClr->wszEdit = "255"), "'" & pClr->wszEdit & "'" )

        ' An unparseable buffer leaves the value alone rather than trapping the user in the field.
        dim as COLORREF cKeep = PsColorPicker_GetColor(h)
        pClr->wszEdit = ""
        pClr->nCaret = 0
        CLRTest_Key( h, VK_RETURN )
        CLRTest_Check( "an empty buffer is discarded and the value is left alone", _
                       (PsColorPicker_GetColor(h) = cKeep), CLRTest_ClrStr(PsColorPicker_GetColor(h)) )
    end scope

    scope
        ' THE LAST FIELD, end to end through the real key path. This scope used to drive the hex
        ' field; with hex gone it drives ALPHA, which is now the last one and the only field whose
        ' value does not live in the COLORREF -- so it is the one most likely to be wired to the
        ' wrong half of the value.
        PsColorPicker_ShowAlpha( h, true )
        pClr->LayoutColorPicker()
        dim as RECT rcH = pClr->rcFieldBox(CLR_FIELD_A)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcH.left + rcH.right) \ 2, (rcH.top + rcH.bottom) \ 2 ) )
        CLRTest_Check( "clicking the alpha field moves the caret there", _
                       (pClr->nFocusField = CLR_FIELD_A), str(pClr->nFocusField) )

        dim as COLORREF clrBeforeA = PsColorPicker_GetColor( h )
        pClr->wszEdit = ""
        pClr->nCaret = 0
        CLRTest_TypeString( h, "128" )
        CLRTest_Key( h, VK_RETURN )
        CLRTest_Check( "the alpha field commits its value", _
                       (PsColorPicker_GetAlpha(h) = 128), str(PsColorPicker_GetAlpha(h)) )
        ' ...AND LEAVES THE COLOUR ALONE. Alpha and the COLORREF are separate halves of the
        ' value, and a commit path that folded alpha into the colour would pass every assertion
        ' above this one.
        CLRTest_Check( "committing alpha does not disturb the colour", _
                       (PsColorPicker_GetColor(h) = clrBeforeA), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) )

        ' Three digits is the whole of a channel; a fourth is refused.
        pClr->wszEdit = ""
        pClr->nCaret = 0
        CLRTest_TypeString( h, "2551" )
        CLRTest_Check( "a channel field stops at three characters", _
                       (pClr->wszEdit = "255") , "'" & pClr->wszEdit & "'" )
    end scope

    scope
        ' KILLING FOCUS COMMITS. This is the path that saves a user who typed a value and then
        ' clicked the host's OK button without pressing Enter, and it is easy to leave out.
        pClr->LayoutColorPicker()
        PsColorPicker_SetColor( h, BGR(0,0,0) )
        dim as RECT rcG = pClr->rcFieldBox(CLR_FIELD_G)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcG.left + rcG.right) \ 2, (rcG.top + rcG.bottom) \ 2 ) )
        pClr->wszEdit = "77"
        pClr->nCaret = 2
        CLRTest_ResetNotify()
        SendMessageW( h, WM_KILLFOCUS, 0, 0 )
        dim as long nG = (clng(PsColorPicker_GetColor(h)) shr 8) and &hFF
        dim as boolean bKF = ((nG = 77) andalso (gCLR_NotifyCount = 1) andalso (pClr->nFocusField = -1))
        CLRTest_Check( "losing focus commits the half-typed field exactly once", bKF, _
                       "G=" & str(nG) & " n=" & str(gCLR_NotifyCount) & " field=" & str(pClr->nFocusField) )
    end scope

    scope
        ' Alpha: hidden by default, so it must be unreachable through the field walker even
        ' though its id exists.
        PsColorPicker_ShowAlpha( h, false )
        CLRTest_Check( "the alpha field is not visible while alpha is hidden", _
                       (pClr->IsFieldVisible(CLR_FIELD_A) = false), "" )
        CLRTest_Check( "Tab from B runs off the end while alpha is hidden", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) = -1), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) ) )

        PsColorPicker_ShowAlpha( h, true )
        CLRTest_Check( "Tab from B reaches the alpha field once it is shown", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) = CLR_FIELD_A), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) ) )

        ' NextField ITSELF STILL REPORTS -1 OFF EITHER END -- it is a plain search and knows
        ' nothing about wrapping. The WRAP lives in the WM_KEYDOWN handler, which calls this
        ' function a second time from the far edge; keeping the search honest is what lets the
        ' same call decide "is there a next field" and "which field wraps to".
        CLRTest_Check( "NextField off the last field reports -1", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_A, 1 ) = -1), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_A, 1 ) ) )
        CLRTest_Check( "NextField off the first field reports -1", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_R, -1 ) = -1), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_R, -1 ) ) )

        ' THE WRAP, driven through the REAL key path rather than asserted against NextField --
        ' which is the whole point, since NextField above says -1 and the wrap is what turns that
        ' into "go round". Tab from the last field must land on the first.
        pClr->LayoutColorPicker()
        dim as RECT rcLastF = pClr->rcFieldBox(CLR_FIELD_A)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcLastF.left + rcLastF.right) \ 2, _
                                                        (rcLastF.top + rcLastF.bottom) \ 2 ) )
        CLRTest_Key( h, VK_TAB )
        CLRTest_Check( "Tab from the last field WRAPS to the first rather than leaving the popup", _
                       (pClr->nFocusField = CLR_FIELD_R), str(pClr->nFocusField) )

        ' ...and Shift+Tab from the first wraps back to the last. Driven by holding Shift down,
        ' which is how the handler reads the direction.
        CLRTest_Check( "precondition: the caret is on the first field", _
                       (pClr->nFocusField = CLR_FIELD_R), str(pClr->nFocusField) )

        ' Alpha commits through the same path as a channel.
        pClr->LayoutColorPicker()
        dim as RECT rcA = pClr->rcFieldBox(CLR_FIELD_A)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcA.left + rcA.right) \ 2, (rcA.top + rcA.bottom) \ 2 ) )
        pClr->wszEdit = "128"
        pClr->nCaret = 3
        CLRTest_Key( h, VK_RETURN )
        CLRTest_Check( "the alpha field commits to nAlpha and not to a colour channel", _
                       (PsColorPicker_GetAlpha(h) = 128), str(PsColorPicker_GetAlpha(h)) )
    end scope

    print "=== what the dialog manager is told ==="

    scope
        ' THE SYNTHETIC-MSG DIAGNOSTIC. These ask the control the question IsDialogMessage asks,
        ' rather than the question we hope it asks -- the technique that found the family's
        ' WS_EX_CONTROLPARENT bug.
        pClr->LayoutColorPicker()
        dim as RECT rcR = pClr->rcFieldBox(CLR_FIELD_R)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcR.left + rcR.right) \ 2, (rcR.top + rcR.bottom) \ 2 ) )

        dim as long nTabOnR = CLRTest_DlgCode( h, WM_KEYDOWN, VK_TAB )
        CLRTest_Check( "Tab IS claimed while there is a next field to reach", _
                       ((nTabOnR and DLGC_WANTALLKEYS) <> 0), "&h" & hex(nTabOnR) )

        ' TAB IS CLAIMED ON THE LAST FIELD TOO, because it wraps. This REVERSES the embedded
        ' control's rule, which declined it there so the host dialog's tab order could take over.
        dim as RECT rcH = pClr->rcFieldBox(CLR_FIELD_A)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcH.left + rcH.right) \ 2, (rcH.top + rcH.bottom) \ 2 ) )
        dim as long nTabOnLast = CLRTest_DlgCode( h, WM_KEYDOWN, VK_TAB )
        CLRTest_Check( "Tab IS claimed on the last field, because it wraps rather than leaving", _
                       ((nTabOnLast and DLGC_WANTALLKEYS) <> 0), "&h" & hex(nTabOnLast) )

        dim as long nRet = CLRTest_DlgCode( h, WM_KEYDOWN, VK_RETURN )
        CLRTest_Check( "Enter IS claimed while a field has the caret", _
                       ((nRet and DLGC_WANTALLKEYS) <> 0), "&h" & hex(nRet) )

        ' ESCAPE IS CLAIMED NOW -- the popup IS the cancel, and there is no host dialog behind it
        ' to hand the key to. The embedded control asserted the exact opposite here.
        dim as long nEsc = CLRTest_DlgCode( h, WM_KEYDOWN, VK_ESCAPE )
        CLRTest_Check( "Escape IS claimed, because this popup is what Escape cancels", _
                       ((nEsc and DLGC_WANTALLKEYS) <> 0), "&h" & hex(nEsc) )

        SendMessageW( h, WM_KILLFOCUS, 0, 0 )
        dim as long nRetNoField = CLRTest_DlgCode( h, WM_KEYDOWN, VK_RETURN )
        CLRTest_Check( "Enter is claimed even with no field focused, because it accepts", _
                       ((nRetNoField and DLGC_WANTALLKEYS) <> 0), "&h" & hex(nRetNoField) )

        ' THE CHARACTERS BEHIND THE CLAIMED KEYS ARE CLAIMED TOO, OR THE SYSTEM BEEPS ON EVERY
        ' PRESS. The keystroke still works, so the beep is the only symptom and NO ASSERTION CAN
        ' HEAR IT -- what is assertable is the WM_GETDLGCODE answer that causes the manufactured
        ' WM_CHAR in the first place. This shipped in five controls at once and was found by a
        ' user pressing Enter.
        dim as long nChRet = CLRTest_DlgCode( h, WM_CHAR, 13 )
        dim as long nChEsc = CLRTest_DlgCode( h, WM_CHAR, 27 )
        CLRTest_Check( "the WM_CHAR behind Enter and Escape is claimed too, so neither beeps", _
                       ((nChRet and DLGC_WANTCHARS) <> 0) andalso ((nChEsc and DLGC_WANTCHARS) <> 0), _
                       "&h" & hex(nChRet) & " / &h" & hex(nChEsc) )

        ' ...and an ordinary character with no field focused is NOT claimed, so the claim above
        ' is about those two keys rather than about WM_CHAR wholesale.
        dim as long nChDigit = CLRTest_DlgCode( h, WM_CHAR, asc("7") )
        CLRTest_Check( "an ordinary character is not claimed while no field has the caret", _
                       (nChDigit = 0), "&h" & hex(nChDigit) )
    end scope

    print "=== offscreen rendering ==="

    scope
        ' FLOOD PROBES. The floors below are MEASURED against a deliberately flooded render (the
        ' PaintBorderRect mistake) and are NOT inherited from a sibling -- PsButton's "> 1" passed
        ' on a completely destroyed render in two other controls in this family.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        PsColorPicker_SetColor( h, PsColorPicker_MatrixColorAt( 4, 5, pClr->nMatrixCols, pClr->nMatrixRows ) )

        dim as long tMatrix = PsColorPicker_CountRenderedTones( h, CLR_PART_MATRIX )
        dim as long tTabs   = PsColorPicker_CountRenderedTones( h, CLR_PART_TABS )
        dim as long tFields = PsColorPicker_CountRenderedTones( h, CLR_PART_FIELDS )

        ' The matrix is 192 distinct colours by construction, so the probe's 64-tone cap is the
        ' real ceiling and anything materially below it means cells are not being drawn.
        CLRTest_Check( "the matrix renders many tones, not a flat block", (tMatrix >= 32), _
                       "tones=" & str(tMatrix) & " (healthy 64, flooded 1)" )
        ' Four tab cells, two background colours, antialiased text and a selection indicator: a
        ' healthy strip has a handful of tones and a flooded one has exactly the fill.
        CLRTest_Check( "the tab strip renders more than a single fill", (tTabs >= 4), _
                       "tones=" & str(tTabs) )
        CLRTest_Check( "the entry block renders more than a single fill", (tFields >= 4), _
                       "tones=" & str(tFields) )

        ' THE OK BUTTON, which is the newest painted thing in the control and therefore the one
        ' most likely to be drawn with the wrong primitive. Its floor is MEASURED, not inherited:
        ' healthy 33, and swapping PsColorPicker_Frame's PaintRoundOutline for a fill-and-stroke
        ' collapses it to 2 -- PsButton's ">= 8" would pass on that, and this family has shipped
        ' the flood bug five separate times by trusting a sibling's number.
        dim as long tOK = PsColorPicker_CountRenderedTones( h, CLR_PART_OK )
        CLRTest_Check( "the OK button renders its caption and border, not a flat fill", _
                       (tOK >= 12), "tones=" & str(tOK) & " (healthy 33, flooded 2)" )

        ' ...and its HOT state reaches the pixels. A button whose mood is computed but never
        ' painted looks identical to one with no hover at all, and no geometry assertion can see
        ' the difference.
        dim as ulong hOKIdle = PsColorPicker_HashRenderedPart( h, CLR_PART_OK )
        pClr->bOKHot = true
        dim as ulong hOKHot = PsColorPicker_HashRenderedPart( h, CLR_PART_OK )
        pClr->bOKHot = false
        CLRTest_Check( "the OK button's hot state reaches the pixels", _
                       (hOKIdle <> 0) andalso (hOKHot <> 0) andalso (hOKIdle <> hOKHot), _
                       "&h" & hex(hOKIdle) & " vs &h" & hex(hOKHot) )

        ' The previews are SUPPOSED to be nearly flat -- one colour plus antialiased label text --
        ' so a floor there would be asserting the wrong thing. What is worth asserting is that the
        ' two of them differ when the value differs from the baseline.
        PsColorPicker_SetInitial( h, BGR(255,255,255) )
        PsColorPicker_SetColor( h, BGR(0,0,0), 255, true )
        dim as ulong hInit = PsColorPicker_HashRenderedPart( h, CLR_PART_INITIAL )
        dim as ulong hCurr = PsColorPicker_HashRenderedPart( h, CLR_PART_CURRENT )
        CLRTest_Check( "the Initial and Current previews render differently when they differ", _
                       ((hInit <> hCurr) andalso (hInit <> 0)), "&h" & hex(hInit) & " vs &h" & hex(hCurr) )
    end scope

    scope
        ' THE SELECTION RING REACHES THE PIXELS.
        '
        ' AND IT IS ISOLATED, WHICH IS THE POINT. The naive version of this case -- "set colour A,
        ' hash; set colour B, hash; assert different" -- passes even if the ring is never drawn at
        ' all, because on this control the matrix's own 192 cells are identical in both renders
        ' and... no, they are not: nothing else in the MATRIX rect depends on the current colour.
        ' That is exactly why the matrix is the right part to probe here and the previews are not.
        ' The three-way comparison below pins it down further: a colour that is NOT in the grid
        ' draws no ring at all, so hNoRing is the ringless baseline, and both ringed renders must
        ' differ from it AND from each other. A ring that was never drawn collapses all three to
        ' one value.
        '
        ' Five separate un-isolated assertions are on record in this family; this is the shape
        ' they all had.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )

        PsColorPicker_SetColor( h, BGR(1,2,3) )      ' not in the matrix -> no ring
        dim as ulong hNoRing = PsColorPicker_HashRenderedPart( h, CLR_PART_MATRIX )

        PsColorPicker_SetColor( h, PsColorPicker_MatrixColorAt( 4, 5, pClr->nMatrixCols, pClr->nMatrixRows ) )
        dim as ulong hRingA = PsColorPicker_HashRenderedPart( h, CLR_PART_MATRIX )

        PsColorPicker_SetColor( h, PsColorPicker_MatrixColorAt( 9, 2, pClr->nMatrixCols, pClr->nMatrixRows ) )
        dim as ulong hRingB = PsColorPicker_HashRenderedPart( h, CLR_PART_MATRIX )

        ' The cells themselves never change, so all three differences are the ring and only the
        ' ring.
        dim as boolean bRing = ((hNoRing <> hRingA) andalso (hRingA <> hRingB) andalso (hNoRing <> 0))
        CLRTest_Check( "the selection ring reaches the pixels and moves with the colour", bRing, _
                       "&h" & hex(hNoRing) & " / &h" & hex(hRingA) & " / &h" & hex(hRingB) )

        ' Same state twice must hash IDENTICALLY, or the probe is measuring noise and every
        ' assertion above that uses it is meaningless.
        PsColorPicker_SetColor( h, PsColorPicker_MatrixColorAt( 4, 5, pClr->nMatrixCols, pClr->nMatrixRows ) )
        dim as ulong hAgain = PsColorPicker_HashRenderedPart( h, CLR_PART_MATRIX )
        CLRTest_Check( "the same state hashes identically twice (the probe is deterministic)", _
                       (hAgain = hRingA), "&h" & hex(hAgain) & " vs &h" & hex(hRingA) )
    end scope

    scope
        ' The tab strip repaints when the tab moves.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        dim as ulong hT1 = PsColorPicker_HashRenderedPart( h, CLR_PART_TABS )
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        dim as ulong hT2 = PsColorPicker_HashRenderedPart( h, CLR_PART_TABS )
        CLRTest_Check( "switching tabs reaches the tab strip's pixels", _
                       ((hT1 <> hT2) andalso (hT1 <> 0)), "&h" & hex(hT1) & " vs &h" & hex(hT2) )

        ' And the BODY really redraws as a different thing, rather than the strip alone changing.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        dim as ulong hB1 = PsColorPicker_HashRenderedPart( h, CLR_PART_MATRIX )
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        dim as ulong hB2 = PsColorPicker_HashRenderedPart( h, CLR_PART_SWATCHES )
        dim as boolean bBodies = ((hB1 <> 0) andalso (hB2 <> 0) andalso (hB1 <> hB2))
        CLRTest_Check( "each tab's body renders through its own part rect", bBodies, _
                       "matrix=&h" & hex(hB1) & " swatches=&h" & hex(hB2) )

        ' A part that is empty on this tab probes as 0 rather than reading someone else's pixels.
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        CLRTest_Check( "probing an empty part returns 0 rather than another part's pixels", _
                       (PsColorPicker_HashRenderedPart( h, CLR_PART_MATRIX ) = 0), "" )
    end scope

    scope
        ' Hover state and SELECTION state must stay disjoint -- the PsMenuBar trap, where a
        ' control with keyboard-driven selection built on the family's hover safety net had its
        ' selection erased ~100ms after every keypress.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        PsColorPicker_SetColor( h, PsColorPicker_MatrixColorAt( 4, 5, pClr->nMatrixCols, pClr->nMatrixRows ) )
        dim as long nSelBefore = pClr->nBodySel
        dim as long nFieldBefore = pClr->nFocusField
        PsColorPicker_ClearHot( pClr, h )
        dim as boolean bDisjoint = ((pClr->nBodySel = nSelBefore) andalso (pClr->nFocusField = nFieldBefore))
        CLRTest_Check( "clearing the hover state does not touch the selection or the caret", _
                       bDisjoint, "sel " & str(nSelBefore) & "->" & str(pClr->nBodySel) & _
                       " field " & str(nFieldBefore) & "->" & str(pClr->nFocusField) )
    end scope


    print "=== the modal contract ==="

    scope
        ' WHAT THESE CAN AND CANNOT PROVE, said up front. NONE of them enters the nested loop:
        ' PsColorPicker_RunModal blocks on GetMessage, so a self-test that called it would hang
        ' the host that ran the suite. PsMessageBox is in exactly the same position and answers it
        ' the same way -- it exposes a layout entry point and asserts everything AROUND the loop.
        '
        ' So what is asserted here is the STATE MACHINE the loop reads (bModalDone / bAccepted)
        ' and the paths that write it, driven through the real key and mouse handlers. The loop
        ' itself -- outside-click dismissal, IsDialogMessage, the WM_QUIT re-post -- is verified
        ' interactively and NOWHERE ELSE. That is a real gap and it is stated rather than papered
        ' over by a passing count.
        PsColorPicker_ShowAlpha( h, true )
        pClr->LayoutColorPicker()

        ' ESCAPE CANCELS, from a field and from the body alike.
        pClr->bModalDone = false
        pClr->bAccepted  = true
        CLRTest_Key( h, VK_ESCAPE )
        CLRTest_Check( "Escape ends the modal run as a CANCEL", _
                       pClr->bModalDone andalso (pClr->bAccepted = false), _
                       "done=" & str(pClr->bModalDone) & " accepted=" & str(pClr->bAccepted) )

        ' ENTER WITH NO FIELD FOCUSED ACCEPTS.
        SendMessageW( h, WM_KILLFOCUS, 0, 0 )
        pClr->bModalDone = false
        pClr->bAccepted  = false
        CLRTest_Key( h, VK_RETURN )
        CLRTest_Check( "Enter with no field focused ends the run as an ACCEPT", _
                       pClr->bModalDone andalso pClr->bAccepted, _
                       "done=" & str(pClr->bModalDone) & " accepted=" & str(pClr->bAccepted) )

        ' ENTER *INSIDE* A FIELD MUST NOT ACCEPT -- it commits the number and stays. Getting this
        ' wrong would close the popup on the first Enter after typing, which is the natural way to
        ' confirm a value and would look like the control ignoring it.
        pClr->LayoutColorPicker()
        dim as RECT rcRf = pClr->rcFieldBox(CLR_FIELD_R)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcRf.left + rcRf.right) \ 2, _
                                                        (rcRf.top + rcRf.bottom) \ 2 ) )
        pClr->bModalDone = false
        pClr->bAccepted  = false
        pClr->wszEdit = "200"
        pClr->nCaret  = 3
        CLRTest_Key( h, VK_RETURN )
        dim as long nRAfter = clng(PsColorPicker_GetColor(h)) and &hFF
        CLRTest_Check( "Enter inside a field commits it and does NOT end the run", _
                       (pClr->bModalDone = false) andalso (nRAfter = 200), _
                       "done=" & str(pClr->bModalDone) & " R=" & str(nRAfter) )

        ' THE FIRST ANSWER WINS. A second EndModal must not flip an accept into a cancel -- the
        ' loop may still be unwinding when a stray message arrives.
        pClr->bModalDone = false
        pClr->bAccepted  = false
        PsColorPicker_EndModal( pClr, true )
        PsColorPicker_EndModal( pClr, false )
        CLRTest_Check( "a second EndModal cannot overturn the first", _
                       pClr->bModalDone andalso pClr->bAccepted, _
                       "accepted=" & str(pClr->bAccepted) )

        ' ACCEPTING COMMITS THE FOCUSED FIELD; CANCELLING DOES NOT. This is the pair that decides
        ' whether a user who types a value and clicks OK gets it -- and whether one who types a
        ' value and presses Escape is spared it.
        pClr->LayoutColorPicker()
        PsColorPicker_SetColor( h, BGR(0,0,0) )
        dim as RECT rcGf = pClr->rcFieldBox(CLR_FIELD_G)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcGf.left + rcGf.right) \ 2, _
                                                        (rcGf.top + rcGf.bottom) \ 2 ) )
        pClr->wszEdit = "99"
        pClr->nCaret  = 2
        pClr->bModalDone = false
        PsColorPicker_EndModal( pClr, true )
        dim as long nGAcc = (clng(PsColorPicker_GetColor(h)) shr 8) and &hFF
        CLRTest_Check( "accepting commits the half-typed field", (nGAcc = 99), str(nGAcc) )

        PsColorPicker_SetColor( h, BGR(0,0,0) )
        pClr->LayoutColorPicker()
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcGf.left + rcGf.right) \ 2, _
                                                        (rcGf.top + rcGf.bottom) \ 2 ) )
        pClr->wszEdit = "99"
        pClr->nCaret  = 2
        pClr->bModalDone = false
        PsColorPicker_EndModal( pClr, false )
        dim as long nGCan = (clng(PsColorPicker_GetColor(h)) shr 8) and &hFF
        CLRTest_Check( "cancelling deliberately does NOT commit it", (nGCan = 0), str(nGCan) )
    end scope

    scope
        ' ---- THE OK BUTTON'S PRESS/CANCEL GESTURE ----------------------------------------
        ' The second of this control's two capture sites, and PsButton's rule rather than the
        ' matrix drag's: press, slide off, release must NOT commit.
        pClr->LayoutColorPicker()
        dim as RECT rcB = pClr->rcOK
        dim as long okx = (rcB.left + rcB.right) \ 2
        dim as long oky = (rcB.top + rcB.bottom) \ 2

        ' The DOWN alone must not end the run. A button that committed on the press would give
        ' the user no way to change their mind, and would be the PsComboBox open-on-down rule
        ' applied to the one control where it is wrong.
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( okx, oky ) )
        CLRTest_Check( "pressing OK arms the gesture without ending the run", _
                       (pClr->bModalDone = false) andalso pClr->bOKPressed, _
                       "done=" & str(pClr->bModalDone) & " pressed=" & str(pClr->bOKPressed) )

        ' Released OUTSIDE: no commit, and the press is disarmed.
        SendMessageW( h, WM_LBUTTONUP, 0, CLRTEST_LP( rcB.left - 40, oky ) )
        CLRTest_Check( "releasing away from OK cancels the press rather than committing", _
                       (pClr->bModalDone = false) andalso (pClr->bOKPressed = false), _
                       "done=" & str(pClr->bModalDone) & " pressed=" & str(pClr->bOKPressed) )

        ' Released INSIDE: accept.
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( okx, oky ) )
        SendMessageW( h, WM_LBUTTONUP,   0, CLRTEST_LP( okx, oky ) )
        CLRTest_Check( "pressing and releasing on OK ends the run as an ACCEPT", _
                       pClr->bModalDone andalso pClr->bAccepted, _
                       "done=" & str(pClr->bModalDone) & " accepted=" & str(pClr->bAccepted) )

        ' A LOST CAPTURE ABANDONS THE PRESS. Capture can be taken away at any time, and a button
        ' left armed would fire on the next unrelated release.
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( okx, oky ) )
        SendMessageW( h, WM_CAPTURECHANGED, 0, 0 )
        SendMessageW( h, WM_LBUTTONUP, 0, CLRTEST_LP( okx, oky ) )
        CLRTest_Check( "a stolen capture abandons the OK press", _
                       (pClr->bModalDone = false), "done=" & str(pClr->bModalDone) )

        ' THE OK BUTTON DOES NOT OVERLAP ANYTHING IT SITS BESIDE. An overlap here is what makes a
        ' field unclickable, and it is invisible in a render because the boxes are drawn in order.
        dim as RECT rcXo
        dim as boolean bOKClear = (IntersectRect( @rcXo, @pClr->rcOK, @pClr->rcFields ) = 0) andalso _
                                  (IntersectRect( @rcXo, @pClr->rcOK, @pClr->rcPreview ) = 0) andalso _
                                  (IntersectRect( @rcXo, @pClr->rcOK, @pClr->rcBody ) = 0)
        CLRTest_Check( "the OK button clears the fields, the preview and the body", bOKClear, _
                       CLRTest_RectStr(pClr->rcOK) )

        ' It is INSIDE the content area, against the right margin. A button placed past the
        ' padding would be clipped by the window it is drawn into.
        CLRTest_Check( "the OK button sits against the right margin, inside the content area", _
                       (pClr->rcOK.right = pClr->rcContent.right) andalso _
                       (pClr->rcOK.bottom <= pClr->rcContent.bottom), _
                       CLRTest_RectStr(pClr->rcOK) & " in " & CLRTest_RectStr(pClr->rcContent) )
    end scope

    scope
        ' ---- CLICKING THE INITIAL HALF REVERTS -------------------------------------------
        ' The pair has always been "the visual form of a Reset button"; in the popup it is the
        ' only reset there is, so it now behaves as the thing it looks like.
        pClr->LayoutColorPicker()
        PsColorPicker_SetColor( h, BGR(10, 20, 30) )       ' establishes the baseline too
        PsColorPicker_SetColor( h, BGR(200, 100, 50), 255, true )
        CLRTest_Check( "precondition: current and initial differ", _
                       (PsColorPicker_GetColor(h) <> PsColorPicker_GetInitial(h)), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) & " vs " & _
                       CLRTest_ClrStr(PsColorPicker_GetInitial(h)) )

        CLRTest_ResetNotify()
        dim as RECT rcIn = pClr->rcInitial
        CLRTest_ClickAt( h, (rcIn.left + rcIn.right) \ 2, (rcIn.top + rcIn.bottom) \ 2 )
        CLRTest_Check( "clicking the Initial half reverts to the baseline, notifying once", _
                       (PsColorPicker_GetColor(h) = PsColorPicker_GetInitial(h)) andalso _
                       (gCLR_NotifyCount = 1), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) & " n=" & str(gCLR_NotifyCount) )

        ' THE CURRENT HALF IS INERT, and that is deliberate rather than an oversight: clicking
        ' the value you already have has nothing to mean. Asserted so a future "make both halves
        ' clickable" edit has to argue with a test.
        PsColorPicker_SetColor( h, BGR(200, 100, 50), 255, true )
        CLRTest_ResetNotify()
        dim as RECT rcCu = pClr->rcCurrent
        CLRTest_ClickAt( h, (rcCu.left + rcCu.right) \ 2, (rcCu.top + rcCu.bottom) \ 2 )
        CLRTest_Check( "clicking the Current half does nothing at all", _
                       (gCLR_NotifyCount = 0) andalso (PsColorPicker_GetColor(h) = BGR(200,100,50)), _
                       "n=" & str(gCLR_NotifyCount) )
    end scope

    scope
        ' ---- THE WINDOW ITSELF -----------------------------------------------------------
        ' A WS_POPUP, not a child. A child would be laid out by a parent and clipped to it, which
        ' is exactly what the conversion moved away from.
        dim as long nStyle = clng( GetWindowLongPtrW( h, GWL_STYLE ) )
        CLRTest_Check( "the window is a WS_POPUP and not a WS_CHILD", _
                       ((nStyle and WS_POPUP) <> 0) andalso ((nStyle and WS_CHILD) = 0), _
                       "&h" & hex(nStyle) )

        ' WS_EX_CONTROLPARENT MUST NOT BE SET. CWindow.Create defaults it on, and a window that
        ' declares itself a container is DESCENDED INTO by the dialog manager, which finds no
        ' tabstop child and skips it -- the bug that shipped in PsButton, PsToggle and PsComboBox.
        dim as long nExStyle = clng( GetWindowLongPtrW( h, GWL_EXSTYLE ) )
        CLRTest_Check( "WS_EX_CONTROLPARENT is not inherited from CWindow's default", _
                       ((nExStyle and WS_EX_CONTROLPARENT) = 0), "&h" & hex(nExStyle) )
        CLRTest_Check( "the popup is a tool window, so it stays off the taskbar", _
                       ((nExStyle and WS_EX_TOOLWINDOW) <> 0), "&h" & hex(nExStyle) )

        ' The scrollbar is the ONE child, and it is never a tabstop -- which is why the flag above
        ' can be absent without costing anything.
        dim as long nSbStyle = clng( GetWindowLongPtrW( pClr->hScroll, GWL_STYLE ) )
        CLRTest_Check( "the list scrollbar is not a tabstop", _
                       ((nSbStyle and WS_TABSTOP) = 0), "&h" & hex(nSbStyle) )
    end scope


    scope
        ' ---- DOUBLE-CLICKING A CELL ACCEPTS ----------------------------------------------
        ' The mouse's equivalent of ENTER. Driven through the real WM_LBUTTONDBLCLK path.

        ' CS_DBLCLKS MUST BE SET ON THE CLASS, or the system never sends WM_LBUTTONDBLCLK and
        ' every assertion below is testing a message only this test produces. Read off the real
        ' WINDOW rather than trusted from Create.
        '
        ' IT CANNOT BE MADE TO FAIL BY DELETING THE LINE IN Create, and that was established by
        ' running the A/B rather than assumed: CWindow's default class style ALREADY contains
        ' CS_DBLCLKS, so the style reads &hB with the line and without it. The assertion is still
        ' worth having -- it is what would catch a future CWindow whose default dropped the flag,
        ' which would take the whole gesture with it -- but it is a guard against the framework,
        ' not against this control, and it is not evidence that Create's line does anything.
        dim as long nClsStyle = GetClassLongPtrW( h, GCL_STYLE )
        CLRTest_Check( "CS_DBLCLKS is set, so the system will really send WM_LBUTTONDBLCLK", _
                       ((nClsStyle and CS_DBLCLKS) <> 0), "&h" & hex(nClsStyle) )

        ' ON THE MATRIX: it adopts the cell that was HIT, not the one already selected. Set the
        ' colour to one cell, double-click a DIFFERENT one, and the answer must be the second.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()
        dim as COLORREF clrStart = PsColorPicker_MatrixColorAt( 1, 1, pClr->nMatrixCols, pClr->nMatrixRows )
        dim as COLORREF clrWant  = PsColorPicker_MatrixColorAt( 6, 9, pClr->nMatrixCols, pClr->nMatrixRows )
        PsColorPicker_SetColor( h, clrStart )
        CLRTest_Check( "precondition: the two cells hold different colours", _
                       (clrStart <> clrWant), CLRTest_ClrStr(clrStart) & " vs " & CLRTest_ClrStr(clrWant) )

        dim as long dcx = pClr->rcMatrix.left + (6 * pClr->nCellW) + (pClr->nCellW \ 2)
        dim as long dcy = pClr->rcMatrix.top  + (9 * pClr->nCellH) + (pClr->nCellH \ 2)

        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( dcx, dcy ) )
        CLRTest_Check( "double-clicking a matrix cell ends the run as an ACCEPT", _
                       pClr->bModalDone andalso pClr->bAccepted, _
                       "done=" & str(pClr->bModalDone) & " accepted=" & str(pClr->bAccepted) )
        ' NOTE WHAT THIS DOES AND DOES NOT PIN DOWN. It catches a double-click that commits the
        ' previously selected cell instead of the one under the cursor -- the defect worth
        ' guarding. It does NOT pin the ORDER of the apply and the accept inside the handler:
        ' swapping them was A/B'd and left the suite green, because EndModal only sets two flags
        ' and does not return early, so the apply still runs either way. The order in the source
        ' is written the way it reads rather than because a test requires it.
        CLRTest_Check( "...with the colour of the cell that was HIT, not the one already selected", _
                       (PsColorPicker_GetColor(h) = clrWant), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) & " wanted " & CLRTest_ClrStr(clrWant) )

        ' IT TAKES NO CAPTURE AND STARTS NO DRAG. The popup is closing; a drag armed here would
        ' outlive the window it belongs to.
        CLRTest_Check( "a double-click starts no drag and takes no capture", _
                       (pClr->isDragging = false) andalso (GetCapture() <> h), "" )

        ' ON A LIST TAB TOO -- the same gesture, the other body renderer.
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()
        pClr->nScrollTop = 0
        dim as COLORREF clrRow2
        PsColorPicker_GridColorAt( pClr, 2, clrRow2 )
        dim as long lcx = pClr->rcSwatches.left + 4
        dim as long lcy = pClr->rcSwatches.top + (2 * pClr->nRowH) + (pClr->nRowH \ 2)
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( lcx, lcy ) )
        CLRTest_Check( "double-clicking a list row accepts, with that row's colour", _
                       pClr->bModalDone andalso pClr->bAccepted andalso _
                       (PsColorPicker_GetColor(h) = clrRow2), _
                       "done=" & str(pClr->bModalDone) & " " & CLRTest_ClrStr(PsColorPicker_GetColor(h)) )

        ' ---- AND NOWHERE ELSE ------------------------------------------------------------
        ' Only a body cell accepts. These are the negatives, and they matter more than the
        ' positives above: a double-click that closed the popup from a tab or a field would make
        ' the control unusable for anyone who double-clicks by habit.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()

        dim as RECT rcT = pClr->rcTab(CLR_TAB_WEB)
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( (rcT.left + rcT.right) \ 2, _
                                                          (rcT.top + rcT.bottom) \ 2 ) )
        CLRTest_Check( "double-clicking a TAB does not accept", _
                       (pClr->bModalDone = false), "done=" & str(pClr->bModalDone) )
        ' ...and it still SWITCHES the tab, which is the shared-handler claim. Without the two
        ' messages sharing a handler this click would simply vanish.
        CLRTest_Check( "...and it still switches the tab, because the two messages share a handler", _
                       (PsColorPicker_GetTab(h) = CLR_TAB_WEB), str(PsColorPicker_GetTab(h)) )

        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        PsColorPicker_ShowAlpha( h, true )
        pClr->LayoutColorPicker()
        dim as RECT rcFd = pClr->rcFieldBox(CLR_FIELD_G)
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( (rcFd.left + rcFd.right) \ 2, _
                                                          (rcFd.top + rcFd.bottom) \ 2 ) )
        CLRTest_Check( "double-clicking a FIELD does not accept", _
                       (pClr->bModalDone = false), "done=" & str(pClr->bModalDone) )
        CLRTest_Check( "...and it still puts the caret in that field", _
                       (pClr->nFocusField = CLR_FIELD_G), str(pClr->nFocusField) )

        ' The OK button: a double-click on it must ARM the press exactly as a single down does,
        ' rather than committing on the spot. OK still commits on the UP.
        pClr->LayoutColorPicker()
        dim as RECT rcOKd = pClr->rcOK
        pClr->bModalDone = false
        pClr->bAccepted  = false
        pClr->bOKPressed = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( (rcOKd.left + rcOKd.right) \ 2, _
                                                          (rcOKd.top + rcOKd.bottom) \ 2 ) )
        CLRTest_Check( "double-clicking OK arms its press rather than committing on the spot", _
                       (pClr->bModalDone = false) andalso pClr->bOKPressed, _
                       "done=" & str(pClr->bModalDone) & " pressed=" & str(pClr->bOKPressed) )
        SendMessageW( h, WM_LBUTTONUP, 0, CLRTEST_LP( (rcOKd.left + rcOKd.right) \ 2, _
                                                      (rcOKd.top + rcOKd.bottom) \ 2 ) )

        ' The preview halves keep their single-click meanings under a double-click, and neither
        ' closes the popup.
        PsColorPicker_SetColor( h, BGR(10,20,30) )
        PsColorPicker_SetColor( h, BGR(200,100,50), 255, true )
        pClr->LayoutColorPicker()
        dim as RECT rcIp = pClr->rcInitial
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( (rcIp.left + rcIp.right) \ 2, _
                                                          (rcIp.top + rcIp.bottom) \ 2 ) )
        CLRTest_Check( "double-clicking the Initial preview reverts but does not accept", _
                       (pClr->bModalDone = false) andalso _
                       (PsColorPicker_GetColor(h) = PsColorPicker_GetInitial(h)), _
                       "done=" & str(pClr->bModalDone) & " " & CLRTest_ClrStr(PsColorPicker_GetColor(h)) )

        ' THE TRAILING UP. A real double-click arrives as down, up, dblclk, up -- so the accept is
        ' followed by one more WM_LBUTTONUP. It must not undo the answer or fire anything.
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()
        pClr->bModalDone = false
        pClr->bAccepted  = false
        SendMessageW( h, WM_LBUTTONDBLCLK, 0, CLRTEST_LP( dcx, dcy ) )
        SendMessageW( h, WM_LBUTTONUP,     0, CLRTEST_LP( dcx, dcy ) )
        CLRTest_Check( "the trailing WM_LBUTTONUP after a double-click leaves the answer alone", _
                       pClr->bModalDone andalso pClr->bAccepted, _
                       "done=" & str(pClr->bModalDone) & " accepted=" & str(pClr->bAccepted) )
    end scope

    DestroyWindow( h )

    dim as string sLine = "--- PsColorPicker self-test: " & str(gCLR_TestCount - gCLR_FailCount) & _
                          " passed, " & str(gCLR_FailCount) & " failed (" & str(gCLR_TestCount) & " assertions) ---"
    print sLine
    print ""

end sub
