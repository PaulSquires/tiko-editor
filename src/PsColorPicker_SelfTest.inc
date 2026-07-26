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
dim shared as long     gCLR_AssignCount
dim shared as long     gCLR_AssignIdx

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


' ---- a fixed demo palette, so the PALETTE tab has something to be ------------------------
' Six named colours, which is the size a theme's %MACRO block actually is. TWO OF THEM ARE THE
' SAME COLOUR under different names, deliberately: that is what makes the "arrowing onto the
' second of a duplicate pair does not snap back to the first" case testable, and it is a real
' shape (a theme that defines %accent and %link identically).
const CLR_TESTPALETTE_COUNT = 6
dim shared as DWSTRING  gCLR_TestPalName(0 to CLR_TESTPALETTE_COUNT - 1)
dim shared as COLORREF  gCLR_TestPalColor(0 to CLR_TESTPALETTE_COUNT - 1)

private function CLRTest_PaletteCount( byval hCtrl as HWND ) as long
    return CLR_TESTPALETTE_COUNT
end function

private function CLRTest_PaletteEntry( _
            byval hCtrl    as HWND, _
            byval idx      as long, _
            byref wszName  as DWSTRING, _
            byref clr      as COLORREF _
            ) as boolean

    if (idx < 0) orelse (idx >= CLR_TESTPALETTE_COUNT) then return false
    wszName = gCLR_TestPalName(idx)
    clr     = gCLR_TestPalColor(idx)
    return true
end function

private sub CLRTest_PaletteAssign( byval hCtrl as HWND, byval idx as long )
    gCLR_AssignCount += 1
    gCLR_AssignIdx = idx
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
    gCLR_AssignCount = 0
    gCLR_AssignIdx = -1

    gCLR_TestPalName(0) = "accent"      : gCLR_TestPalColor(0) = BGR( 97,175,239)
    gCLR_TestPalName(1) = "background"  : gCLR_TestPalColor(1) = BGR( 40, 44, 52)
    gCLR_TestPalName(2) = "foreground"  : gCLR_TestPalColor(2) = BGR(171,178,191)
    gCLR_TestPalName(3) = "warning"     : gCLR_TestPalColor(3) = BGR(229,192,123)
    gCLR_TestPalName(4) = "error"       : gCLR_TestPalColor(4) = BGR(224,108,117)
    ' A DUPLICATE of entry 0 under another name -- see the note on the table above.
    gCLR_TestPalName(5) = "link"        : gCLR_TestPalColor(5) = BGR( 97,175,239)

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

        dim as boolean bF = PsColorPicker_FieldAccepts( CLR_FIELD_HEX, asc("f"), 3 )
        dim as boolean bF2 = PsColorPicker_FieldAccepts( CLR_FIELD_HEX, asc("F"), 3 )
        dim as boolean bG = PsColorPicker_FieldAccepts( CLR_FIELD_HEX, asc("g"), 3 )
        dim as boolean bHexOK = (bF andalso bF2 andalso (bG = false))
        CLRTest_Check( "the hex field takes a-f in either case and stops at g", bHexOK, _
                       str(bF) & "/" & str(bF2) & "/" & str(bG) )

        dim as boolean bH0 = PsColorPicker_FieldAccepts( CLR_FIELD_HEX, asc("#"), 0 )
        dim as boolean bH3 = PsColorPicker_FieldAccepts( CLR_FIELD_HEX, asc("#"), 3 )
        dim as boolean bHash = (bH0 andalso (bH3 = false))
        CLRTest_Check( "the hex field takes # only at position 0", bHash, str(bH0) & "/" & str(bH3) )

        dim as boolean bLen = ((PsColorPicker_FieldMaxLen(CLR_FIELD_R) = 3) andalso _
                               (PsColorPicker_FieldMaxLen(CLR_FIELD_HEX) = 7))
        CLRTest_Check( "field lengths are 3 for a channel and 7 for hex", bLen, _
                       str(PsColorPicker_FieldMaxLen(CLR_FIELD_R)) & "/" & str(PsColorPicker_FieldMaxLen(CLR_FIELD_HEX)) )
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

        ' END TO END: does the dialog manager actually reach it? The two assertions above are
        ' about bits; this one asks the thing that was really broken.
        dim as HWND hFound = GetNextDlgTabItem( hWndParent, 0, FALSE )
        dim as boolean bReach = false
        dim as HWND hWalk = hFound
        for i as long = 0 to 63
            if hWalk = 0 then exit for
            if hWalk = h then bReach = true : exit for
            hWalk = GetNextDlgTabItem( hWndParent, hWalk, FALSE )
            if hWalk = hFound then exit for
        next
        CLRTest_Check( "GetNextDlgTabItem reaches the control from its parent", bReach, _
                       "first=&h" & hex(cast(uinteger, hFound)) )
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

        ' The tab cells must TILE the strip: no gap, no overlap, and the last one meets the right
        ' edge. Accumulating a per-tab width instead of computing edges from the strip leaves a
        ' rounding remainder here, which is visible as a sliver of background at the end.
        dim as long nTabs = pClr->TabCount()
        dim as boolean bTile = ((pClr->rcTab(0).left = pClr->rcTabs.left) andalso _
                                (pClr->rcTab(nTabs - 1).right = pClr->rcTabs.right))
        for i as long = 0 to nTabs - 2
            if pClr->rcTab(i).right <> pClr->rcTab(i + 1).left then bTile = false
        next
        CLRTest_Check( "the tab cells tile the strip with no gap or overlap", bTile, _
                       "n=" & str(nTabs) & " " & CLRTest_RectStr(pClr->rcTab(0)) & " .. " & _
                       CLRTest_RectStr(pClr->rcTab(nTabs - 1)) )

        CLRTest_Check( "three tabs without the palette", (nTabs = 3), str(nTabs) )

        CLRTest_Check( "rcMatrix is EXACTLY cols x rows of cells", _
                       (((pClr->rcMatrix.right - pClr->rcMatrix.left) = (pClr->nMatrixCols * pClr->nCellW)) andalso _
                        ((pClr->rcMatrix.bottom - pClr->rcMatrix.top) = (pClr->nMatrixRows * pClr->nCellH))), _
                       CLRTest_RectStr(pClr->rcMatrix) )
    end scope

    scope
        ' THE FIXED-BODY CLAIM, which is the layout's load-bearing decision: an EMBEDDED control
        ' must not resize as the user browses tabs, or the host's layout moves underneath it.
        dim as long nBodyH = pClr->rcBody.bottom - pClr->rcBody.top
        dim as long nSeenW = pClr->nIdealW
        dim as long nSeenH = pClr->nIdealH
        dim as boolean bFixed = true
        dim as string sHeights = str(nBodyH)

        PsColorPicker_ShowPaletteTab( h, true )
        for t as long = CLR_TAB_WEB to CLR_TAB_PALETTE
            PsColorPicker_SetTab( h, t )
            pClr->LayoutColorPicker()
            dim as long bh = pClr->rcBody.bottom - pClr->rcBody.top
            if bh <> nBodyH then bFixed = false
            if t > CLR_TAB_WEB then sHeights &= "/" & str(bh)
            dim as boolean bIdealSame = ((pClr->nIdealW = nSeenW) andalso (pClr->nIdealH = nSeenH))
            if bIdealSame = false then bFixed = false
        next
        CLRTest_Check( "the body height and the ideal size are identical on all four tabs", _
                       bFixed, "heights=" & sHeights )

        ' MUTUAL EXCLUSION: whichever body rect the current tab does not use is EMPTY, which is
        ' what lets a paint callback branch on IsRectEmpty instead of re-deriving the mapping.
        dim as boolean bExcl = true
        dim as string sExcl
        for t as long = CLR_TAB_WEB to CLR_TAB_PALETTE
            PsColorPicker_SetTab( h, t )
            pClr->LayoutColorPicker()
            dim as boolean bM = (IsRectEmpty( @pClr->rcMatrix ) = 0)
            dim as boolean bS = (IsRectEmpty( @pClr->rcSwatches ) = 0)
            dim as boolean bWant = (t = CLR_TAB_CUSTOM)
            if (bM <> bWant) orelse (bS = bWant) then bExcl = false
            sExcl &= str(t) & ":" & str(bM) & str(bS) & " "
        next
        CLRTest_Check( "exactly one of rcMatrix / rcSwatches is non-empty on every tab", bExcl, sExcl )

        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        pClr->LayoutColorPicker()
        dim as boolean bAssignOn = (IsRectEmpty( @pClr->rcAssign ) = 0)
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        pClr->LayoutColorPicker()
        dim as boolean bAssignOff = (IsRectEmpty( @pClr->rcAssign ) <> 0)
        CLRTest_Check( "the Assign button exists on the palette tab and nowhere else", _
                       (bAssignOn andalso bAssignOff), str(bAssignOn) & "/" & str(bAssignOff) )

        PsColorPicker_ShowPaletteTab( h, false )
        pClr->LayoutColorPicker()
        CLRTest_Check( "hiding the palette tab drops the strip back to three", _
                       (pClr->TabCount() = 3), str(pClr->TabCount()) )
    end scope

    scope
        ' The fields. Enum order IS visual order -- see the note on CLR_FIELD_*.
        pClr->LayoutColorPicker()
        dim as boolean bRow1 = ((pClr->rcFieldBox(CLR_FIELD_R).left < pClr->rcFieldBox(CLR_FIELD_G).left) andalso _
                                (pClr->rcFieldBox(CLR_FIELD_G).left < pClr->rcFieldBox(CLR_FIELD_B).left))
        CLRTest_Check( "R, G and B run left to right in enum order", bRow1, _
                       str(pClr->rcFieldBox(CLR_FIELD_R).left) & "/" & _
                       str(pClr->rcFieldBox(CLR_FIELD_G).left) & "/" & _
                       str(pClr->rcFieldBox(CLR_FIELD_B).left) )

        dim as boolean bTops = ((pClr->rcFieldBox(CLR_FIELD_R).top = pClr->rcFieldBox(CLR_FIELD_G).top) andalso _
                                (pClr->rcFieldBox(CLR_FIELD_G).top = pClr->rcFieldBox(CLR_FIELD_B).top))
        CLRTest_Check( "the three channel fields share one top edge", bTops, str(pClr->rcFieldBox(CLR_FIELD_R).top) )

        dim as boolean bSecondRow = (pClr->rcFieldBox(CLR_FIELD_HEX).top > pClr->rcFieldBox(CLR_FIELD_R).top)
        CLRTest_Check( "the hex field sits on the second row", bSecondRow, _
                       str(pClr->rcFieldBox(CLR_FIELD_HEX).top) & " vs " & str(pClr->rcFieldBox(CLR_FIELD_R).top) )

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

        ' Alpha adds a group to row 2 but no row, so the height must NOT move. If it did, a host
        ' that flipped alpha per key would see its layout jump.
        CLRTest_Check( "showing alpha does not change the ideal HEIGHT", _
                       (pClr->nIdealH = nHBefore), str(nHBefore) & " -> " & str(pClr->nIdealH) )

        ' A is before Hex, matching the enum. Getting this backwards would make Tab walk right
        ' then left across one row.
        dim as boolean bAOrder = (pClr->rcFieldBox(CLR_FIELD_A).left < pClr->rcFieldBox(CLR_FIELD_HEX).left)
        CLRTest_Check( "the alpha field sits before the hex field, matching the enum", bAOrder, _
                       str(pClr->rcFieldBox(CLR_FIELD_A).left) & " vs " & str(pClr->rcFieldBox(CLR_FIELD_HEX).left) )

        PsColorPicker_ShowAlpha( h, false )
        pClr->LayoutColorPicker()
        CLRTest_Check( "the ideal width returns to its old value when alpha is hidden again", _
                       (pClr->nIdealW = nWBefore), str(nWBefore) & " -> " & str(pClr->nIdealW) )
    end scope

    scope
        pClr->LayoutColorPicker()
        dim as boolean bStack = ((pClr->rcCurrent.top = pClr->rcInitial.bottom) andalso _
                                 (pClr->rcCurrent.left = pClr->rcInitial.left) andalso _
                                 (pClr->rcCurrent.right = pClr->rcInitial.right))
        CLRTest_Check( "the two previews are stacked and share a width", bStack, _
                       CLRTest_RectStr(pClr->rcInitial) & " / " & CLRTest_RectStr(pClr->rcCurrent) )

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
        ' THE WEB GRID'S SHORT LAST ROW. 48 colours in 8 columns divides exactly, so the table is
        ' padded out here instead: the assertion is that an index PAST the count is refused even
        ' though its cell is inside the grid rect. That is the case a naive
        ' row*cols+col would get wrong, and the palette tab (6 entries, one per row) is where it
        ' would show first.
        PsColorPicker_ShowPaletteTab( h, true )
        PsColorPicker_SetPaletteCallbacks( h, @CLRTest_PaletteCount, @CLRTest_PaletteEntry, @CLRTest_PaletteAssign )
        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        pClr->LayoutColorPicker()

        dim as boolean bRows = true
        for i as long = 0 to CLR_TESTPALETTE_COUNT - 1
            dim as long cy = pClr->rcSwatches.top + (i * pClr->nRowH) + (pClr->nRowH \ 2)
            dim as long cx = pClr->rcSwatches.left + 4
            if PsColorPicker_HitBody( pClr, cx, cy ) <> i then bRows = false
        next
        CLRTest_Check( "every palette row hit-tests to its own index", bRows, _
                       "rows=" & str(CLR_TESTPALETTE_COUNT) & " rowH=" & str(pClr->nRowH) )

        dim as long cyPast = pClr->rcSwatches.top + (CLR_TESTPALETTE_COUNT * pClr->nRowH) + 2
        CLRTest_Check( "a click below the last palette row hits nothing", _
                       (PsColorPicker_HitBody( pClr, pClr->rcSwatches.left + 4, cyPast ) = -1), _
                       "y=" & str(cyPast) & " list=" & CLRTest_RectStr(pClr->rcSwatches) )

        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()
        dim as long nWeb = PsColorPicker_WebColorCount()
        dim as boolean bWeb = true
        for i as long = 0 to nWeb - 1
            dim as long c = i mod pClr->nSwatchCols
            dim as long r = i \ pClr->nSwatchCols
            dim as long cx = pClr->rcSwatches.left + (c * pClr->nSwatchW) + (pClr->nSwatchW \ 2)
            dim as long cy = pClr->rcSwatches.top + (r * pClr->nSwatchH) + (pClr->nSwatchH \ 2)
            if PsColorPicker_HitBody( pClr, cx, cy ) <> i then bWeb = false
        next
        CLRTest_Check( "every web swatch hit-tests to its own index", bWeb, "count=" & str(nWeb) )

        ' The web table must FIT the body it is drawn in -- six rows of swatches inside the
        ' matrix's height. A table that outgrew the panel would be clipped silently, so the
        ' constraint is asserted rather than remembered.
        dim as long nWebRows = (nWeb + pClr->nSwatchCols - 1) \ pClr->nSwatchCols
        CLRTest_Check( "the web table fits inside the fixed body height", _
                       ((nWebRows * pClr->nSwatchH) <= (pClr->rcBody.bottom - pClr->rcBody.top)), _
                       "rows=" & str(nWebRows) & " needs=" & str(nWebRows * pClr->nSwatchH) & _
                       " has=" & str(pClr->rcBody.bottom - pClr->rcBody.top) )

        PsColorPicker_SetTab( h, CLR_TAB_SYSTEM )
        pClr->LayoutColorPicker()
        dim as long nSys = PsColorPicker_SystemColorCount()
        dim as long nSysRows = (nSys + pClr->nSwatchCols - 1) \ pClr->nSwatchCols
        CLRTest_Check( "the system table fits inside the fixed body height", _
                       ((nSysRows * pClr->nSwatchH) <= (pClr->rcBody.bottom - pClr->rcBody.top)), _
                       "count=" & str(nSys) & " rows=" & str(nSysRows) )

        ' The tail of a short last row. 31 system colours in 8 columns leaves one cell empty in
        ' row 3, and that cell is INSIDE the grid rect -- so only the index-vs-count test refuses
        ' it. This is the assertion the palette case above cannot make.
        dim as long cTail = nSys mod pClr->nSwatchCols
        dim as long rTail = nSys \ pClr->nSwatchCols
        dim as long txc = pClr->rcSwatches.left + (cTail * pClr->nSwatchW) + (pClr->nSwatchW \ 2)
        dim as long tyc = pClr->rcSwatches.top + (rTail * pClr->nSwatchH) + (pClr->nSwatchH \ 2)
        dim as boolean bTailPrecond = (cTail <> 0)
        CLRTest_Check( "precondition: the system grid really does have a short last row", _
                       bTailPrecond, "count=" & str(nSys) & " cols=" & str(pClr->nSwatchCols) )
        CLRTest_Check( "an empty cell past the end of a short last row hits nothing", _
                       (PsColorPicker_HitBody( pClr, txc, tyc ) = -1), _
                       "cell " & str(cTail) & "," & str(rTail) )
    end scope

    scope
        ' OVERFLOW: THE PAINTER CLIPS, SO THE HIT TEST MUST CLIP IDENTICALLY.
        '
        ' This case exists because an A/B found the guard that used to stand here was protecting
        ' nothing: it was a PtIn bound on the grid rect, and removing it failed NO assertion,
        ' because CellFromPoint's own index bounds already covered every point the suite tested.
        ' What it did not cover -- and what nobody screenshots -- is a control sized NARROWER
        ' than ideal, where the right-hand column of swatches is clipped out of the render and a
        ' click on the blank strip they used to occupy still picked them.
        '
        ' The control is deliberately sized so the sixth column is PARTIALLY visible: its centre
        ' is inside the grid rect (so a PtIn test passes) while its right edge is outside (so the
        ' painter skips it). That is the one point that tells the two guards apart.
        dim as long nSaveW, nSaveH
        PsColorPicker_GetIdealSize( h, nSaveW, nSaveH )

        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        dim as long nNarrow = (2 * pClr->nPad) + (5 * pClr->nSwatchW) + ((3 * pClr->nSwatchW) \ 4)
        SetWindowPos( h, 0, 0, 0, nNarrow, nSaveH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
        pClr->LayoutColorPicker()

        dim as long cxCut = pClr->rcSwatches.left + (5 * pClr->nSwatchW) + (pClr->nSwatchW \ 2)
        dim as long cyCut = pClr->rcSwatches.top + (pClr->nSwatchH \ 2)

        ' Precondition, asserted rather than assumed: the probe point really is inside the grid
        ' rect and really does belong to a cell the painter drops. A point that failed the plain
        ' bounds test would make this case pass for the wrong reason.
        dim as boolean bPre = (PsColorPicker_PtIn( pClr->rcSwatches, cxCut, cyCut ) andalso _
                               ((pClr->rcSwatches.left + (6 * pClr->nSwatchW)) > pClr->rcSwatches.right))
        CLRTest_Check( "precondition: the probe point is inside the grid but on a CLIPPED cell", _
                       bPre, "x=" & str(cxCut) & " grid=" & CLRTest_RectStr(pClr->rcSwatches) )

        CLRTest_Check( "a partially-clipped swatch is not clickable (the painter skips it too)", _
                       (PsColorPicker_HitBody( pClr, cxCut, cyCut ) = -1), _
                       "hit=" & str( PsColorPicker_HitBody( pClr, cxCut, cyCut ) ) )

        ' ...and the cells that DO fit are still clickable, so the clip is not simply refusing
        ' everything.
        dim as long cxOK = pClr->rcSwatches.left + (4 * pClr->nSwatchW) + (pClr->nSwatchW \ 2)
        CLRTest_Check( "a fully visible swatch beside it is still clickable", _
                       (PsColorPicker_HitBody( pClr, cxOK, cyCut ) = 4), _
                       "hit=" & str( PsColorPicker_HitBody( pClr, cxOK, cyCut ) ) )

        SetWindowPos( h, 0, 0, 0, nSaveW, nSaveH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
        pClr->LayoutColorPicker()
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

        PsColorPicker_ShowPaletteTab( h, false )
        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        CLRTest_Check( "SetTab refuses the palette tab while it is hidden", _
                       (PsColorPicker_GetTab(h) <> CLR_TAB_PALETTE), str(PsColorPicker_GetTab(h)) )

        PsColorPicker_ShowPaletteTab( h, true )
        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        PsColorPicker_ShowPaletteTab( h, false )
        CLRTest_Check( "hiding the palette tab while it is current falls back to Custom", _
                       (PsColorPicker_GetTab(h) = CLR_TAB_CUSTOM), str(PsColorPicker_GetTab(h)) )
        PsColorPicker_ShowPaletteTab( h, true )
    end scope

    scope
        ' GetPaletteSel answers -1 off the palette tab, which is the honest answer given the
        ' selection is one field shared by every tab.
        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        PsColorPicker_SetColor( h, gCLR_TestPalColor(3) )
        dim as long nSelOn = PsColorPicker_GetPaletteSel( h )
        PsColorPicker_SetTab( h, CLR_TAB_CUSTOM )
        dim as long nSelOff = PsColorPicker_GetPaletteSel( h )
        CLRTest_Check( "GetPaletteSel finds the entry matching the colour, and answers -1 off-tab", _
                       ((nSelOn = 3) andalso (nSelOff = -1)), str(nSelOn) & "/" & str(nSelOff) )

        ' SyncBodySel picks the FIRST match, which for the duplicate pair means entry 0 and not
        ' entry 5. Stated as an assertion rather than left as an accident, because the arrow-key
        ' handler re-asserts nBodySel precisely to work around it.
        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        PsColorPicker_SetColor( h, gCLR_TestPalColor(0) )
        CLRTest_Check( "a duplicated palette colour resolves to the FIRST entry that holds it", _
                       (PsColorPicker_GetPaletteSel(h) = 0), str(PsColorPicker_GetPaletteSel(h)) )

        ' ...and arrowing off it lands on the next entry rather than snapping back.
        PsColorPicker_SetPaletteSel( h, 5 )
        PsColorPicker_NavigateBody( pClr, 1 )
        CLRTest_Check( "arrowing off a duplicated entry moves on rather than snapping back", _
                       (PsColorPicker_GetPaletteSel(h) <> 0), str(PsColorPicker_GetPaletteSel(h)) )
    end scope

    scope
        ' The Assign button reports the intent and does nothing else. It is the deliberate
        ' counterpart to detach-on-edit: without it a palette can only ever decay.
        PsColorPicker_SetTab( h, CLR_TAB_PALETTE )
        pClr->LayoutColorPicker()
        PsColorPicker_SetPaletteSel( h, 2 )
        gCLR_AssignCount = 0
        gCLR_AssignIdx = -1
        dim as RECT rcA = pClr->rcAssign
        CLRTest_ClickAt( h, (rcA.left + rcA.right) \ 2, (rcA.top + rcA.bottom) \ 2 )
        dim as boolean bAssign = ((gCLR_AssignCount = 1) andalso (gCLR_AssignIdx = 2))
        CLRTest_Check( "clicking Assign reports the selected entry exactly once", bAssign, _
                       "n=" & str(gCLR_AssignCount) & " idx=" & str(gCLR_AssignIdx) )

        ' Nothing selected, nothing reported. A control that fired with idx = -1 would make every
        ' host write the same guard.
        PsColorPicker_SetPaletteSel( h, -1 )
        gCLR_AssignCount = 0
        CLRTest_ClickAt( h, (rcA.left + rcA.right) \ 2, (rcA.top + rcA.bottom) \ 2 )
        CLRTest_Check( "Assign reports nothing when no entry is selected", _
                       (gCLR_AssignCount = 0), "n=" & str(gCLR_AssignCount) )
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
        ' A SWATCH CLICK is a settled choice: one END, no BEGIN. The contrast with the drag above
        ' is the whole reason CLR_PHASE_* exists.
        PsColorPicker_SetTab( h, CLR_TAB_WEB )
        pClr->LayoutColorPicker()
        CLRTest_ResetNotify()
        dim as long cx = pClr->rcSwatches.left + (pClr->nSwatchW \ 2) + (3 * pClr->nSwatchW)
        dim as long cy = pClr->rcSwatches.top + (pClr->nSwatchH \ 2)
        CLRTest_ClickAt( h, cx, cy )

        dim as boolean bOne = ((gCLR_NotifyCount = 1) andalso (gCLR_NotifyPhase(0) = CLR_PHASE_END))
        CLRTest_Check( "a swatch click emits exactly one END and no BEGIN", bOne, _
                       "n=" & str(gCLR_NotifyCount) & " phase=" & str(gCLR_NotifyPhase(0)) )
        CLRTest_Check( "the swatch click adopted that swatch's colour", _
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
        ' The hex field, end to end through the real key path.
        pClr->LayoutColorPicker()
        dim as RECT rcH = pClr->rcFieldBox(CLR_FIELD_HEX)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcH.left + rcH.right) \ 2, (rcH.top + rcH.bottom) \ 2 ) )
        CLRTest_Check( "clicking the hex field moves the caret there", _
                       (pClr->nFocusField = CLR_FIELD_HEX), str(pClr->nFocusField) )

        pClr->wszEdit = ""
        pClr->nCaret = 0
        CLRTest_TypeString( h, "#FF8000" )
        CLRTest_Key( h, VK_RETURN )
        CLRTest_Check( "the hex field commits #RRGGBB", _
                       (PsColorPicker_GetColor(h) = BGR(255,128,0)), _
                       CLRTest_ClrStr(PsColorPicker_GetColor(h)) )

        ' Seven characters is the whole of "#RRGGBB"; an eighth is refused.
        pClr->wszEdit = ""
        pClr->nCaret = 0
        CLRTest_TypeString( h, "#ABCDEF12" )
        CLRTest_Check( "the hex field stops at seven characters", _
                       (pClr->wszEdit = "#ABCDEF"), "'" & pClr->wszEdit & "'" )
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
        CLRTest_Check( "Tab from B skips the hidden alpha field and lands on hex", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) = CLR_FIELD_HEX), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) ) )

        PsColorPicker_ShowAlpha( h, true )
        CLRTest_Check( "Tab from B reaches the alpha field once it is shown", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) = CLR_FIELD_A), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_B, 1 ) ) )

        ' OFF THE END IN BOTH DIRECTIONS. This is what makes the control a well-behaved member of
        ' a dialog rather than a Tab trap.
        CLRTest_Check( "Tab off the last field reports -1 rather than wrapping", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_HEX, 1 ) = -1), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_HEX, 1 ) ) )
        CLRTest_Check( "Shift+Tab off the first field reports -1 rather than wrapping", _
                       (PsColorPicker_NextField( pClr, CLR_FIELD_R, -1 ) = -1), _
                       str( PsColorPicker_NextField( pClr, CLR_FIELD_R, -1 ) ) )

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

        dim as RECT rcH = pClr->rcFieldBox(CLR_FIELD_HEX)
        SendMessageW( h, WM_LBUTTONDOWN, 0, CLRTEST_LP( (rcH.left + rcH.right) \ 2, (rcH.top + rcH.bottom) \ 2 ) )
        dim as long nTabOnHex = CLRTest_DlgCode( h, WM_KEYDOWN, VK_TAB )
        CLRTest_Check( "Tab is NOT claimed on the last field, so the host's tab order takes over", _
                       (nTabOnHex = 0), "&h" & hex(nTabOnHex) )

        dim as long nRet = CLRTest_DlgCode( h, WM_KEYDOWN, VK_RETURN )
        CLRTest_Check( "Enter IS claimed while a field has the caret", _
                       ((nRet and DLGC_WANTALLKEYS) <> 0), "&h" & hex(nRet) )

        ' A NEGATIVE ASSERTION, so a future edit that "restores family consistency" by claiming
        ' Escape turns this suite red rather than silently breaking every host dialog's Cancel key.
        dim as long nEsc = CLRTest_DlgCode( h, WM_KEYDOWN, VK_ESCAPE )
        CLRTest_Check( "Escape is deliberately NOT claimed, so a host dialog can still cancel", _
                       (nEsc = 0), "&h" & hex(nEsc) )

        SendMessageW( h, WM_KILLFOCUS, 0, 0 )
        dim as long nRetNoField = CLRTest_DlgCode( h, WM_KEYDOWN, VK_RETURN )
        CLRTest_Check( "Enter is NOT claimed once no field has the caret", _
                       (nRetNoField = 0), "&h" & hex(nRetNoField) )
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

    DestroyWindow( h )

    dim as string sLine = "--- PsColorPicker self-test: " & str(gCLR_TestCount - gCLR_FailCount) & _
                          " passed, " & str(gCLR_FailCount) & " failed (" & str(gCLR_TestCount) & " assertions) ---"
    print sLine
    print ""

end sub
