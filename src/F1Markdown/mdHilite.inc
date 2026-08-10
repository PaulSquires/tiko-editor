'' ========================================================================================
'' mdHilite.inc -- the implementation. See mdHilite.bi for scope and the state rule.
''
'' THE SPANS TILE THE LINE. Every branch below either emits a span covering the bytes it
'' consumed or falls through to the one-byte default, and the caller relies on that: a gap
'' would render as missing characters, and an overlap as doubled ones.
'' ========================================================================================

#pragma once
#include once "mdHilite.bi"

'' ---- keyword tables --------------------------------------------------------------------
'' Space-delimited and looked up with instr on " word ", which is O(table) per identifier and
'' entirely fast enough for a help page -- the largest fence in the corpus is 60 lines.
''
'' FreeBASIC's are LOWERCASE and matched case-insensitively, because fbc identifiers are
'' case-insensitive: `Dim`, `DIM` and `dim` are one keyword and all three appear in the docs.
private const FB_KEYWORDS = _
    " abs access alias allocate and andalso any append as asc asin asm atan2 atn base beep " & _
    "bin bit bitreset bitset bload boolean bsave byref byte byval call callocate case cast " & _
    "cbyte cdbl cdecl chain chdir chr cint circle class clear clng clngint close cls color " & _
    "command common cons const constructor continue cos cptr create csign csng csrlin cshort " & _
    "cubyte cuint culngint cunsg curdir cushort custom cvd cvi cvl cvlongint cvs data " & _
    "date dateadd datediff datepart dateserial datevalue day deallocate declare defbyte " & _
    "defdbl defint deflng deflongint defshort defsng defstr defubyte defuint defulongint " & _
    "defushort delete destructor dim dir do double draw dylibfree dylibload dylibsymbol " & _
    "else elseif encoding end enum environ eof eqv erase err error exec exepath exit exp " & _
    "export extern field fileattr filecopy filedatetime fileexists filelen fix flip for " & _
    "format frac fre freefile function get getjoystick getkey getmouse gosub goto hex hibyte " & _
    "hiword if iif imageconvert imagecreate imagedestroy imageinfo imp implements import " & _
    "inkey inp input instr instrrev int integer is isdate kill lbound lcase left len let " & _
    "lib line lobyte loc local locate lock lof log long longint loop loword lpos lprint lset " & _
    "ltrim mid mkd mkdir mki mkl mklongint mks mod month monthname multikey mutexcreate " & _
    "mutexdestroy mutexlock mutexunlock name namespace new next not now oct offsetof on open " & _
    "operator option or orelse out output overload paint palette pascal pcopy peek pmap point " & _
    "pointcoord pointer poke pos preserve preset print private procptr property protected " & _
    "pset ptr public put random randomize read reallocate redim rem reset restore resume " & _
    "return rgb rgba right rmdir rnd rset rtrim run sadd screen screencopy screencontrol " & _
    "screenevent screeninfo screenlist screenlock screenptr screenres screenset screensync " & _
    "screenunlock seek select setdate setenviron setmouse settime sgn shared shell shl short " & _
    "sin single sizeof sleep space spc sqr static stdcall step stop str string strptr sub " & _
    "swap system tan then this threadcall threadcreate threadwait time timer to trim true " & _
    "false type typeof ubound ubyte ucase uinteger ulong ulongint union unlock unsigned " & _
    "until ushort using va_arg va_first va_next val vallng valuint var view virtual wait " & _
    "wbin wchr wend where while width window windowtitle winput with wend write wspace wstr " & _
    "wstring xor zstring "

'' C and C++ together: the corpus's `c` and `cpp` fences are both Win32-flavoured and a
'' single table costs nothing. Win32 typedefs are included because a help page that
'' highlights `int` but not `HWND` looks broken rather than minimal.
private const C_KEYWORDS = _
    " alignas alignof asm auto bool break case catch char class const consteval constexpr " & _
    "const_cast continue decltype default delete do double dynamic_cast else enum explicit " & _
    "export extern false float for friend goto if inline int long mutable namespace new " & _
    "noexcept nullptr operator private protected public register reinterpret_cast return " & _
    "short signed sizeof static static_assert static_cast struct switch template this throw " & _
    "true try typedef typeid typename union unsigned using virtual void volatile wchar_t " & _
    "while size_t ssize_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t " & _
    "BOOL BYTE WORD DWORD LONG ULONG UINT INT LPARAM WPARAM LRESULT HRESULT HWND HDC HINSTANCE " & _
    "HMODULE HANDLE HMENU HBITMAP HBRUSH HPEN HFONT HICON HCURSOR LPSTR LPCSTR LPWSTR LPCWSTR " & _
    "TCHAR CHAR WCHAR VOID CONST NULL TRUE FALSE RECT POINT SIZE MSG WNDCLASS WNDCLASSEX " & _
    "CALLBACK WINAPI APIENTRY "

private function HIsWordCh( byval ch as long ) as boolean
    return ((ch >= 48) andalso (ch <= 57)) orelse ((ch >= 65) andalso (ch <= 90)) orelse _
           ((ch >= 97) andalso (ch <= 122)) orelse (ch = 95)
end function

private function HIsDigit( byval ch as long ) as boolean
    return (ch >= 48) andalso (ch <= 57)
end function

private function HLower( byref s as string ) as string
    dim as string r = s
    for i as long = 0 to len(r) - 1
        if (r[i] >= 65) andalso (r[i] <= 90) then r[i] += 32
    next
    return r
end function

function MdLangFromName( byref sLang as string ) as long
    select case HLower(sLang)
        case "freebasic", "fb", "basic", "bas", "vb", "vbnet" : return MDL_FB
        case "c", "cpp", "c++", "cc", "h", "hpp", "objc"      : return MDL_C
    end select
    return MDL_NONE
end function

private function IsKeyword( byval nLang as long, byref sWord as string ) as boolean
    if nLang = MDL_FB then
        return (instr(FB_KEYWORDS, " " & HLower(sWord) & " ") > 0)
    elseif nLang = MDL_C then
        '' C is case-SENSITIVE, so no folding here. That is the whole difference between the
        '' two lookups, and getting it wrong would highlight `Int` and `HWnd`.
        return (instr(C_KEYWORDS, " " & sWord & " ") > 0)
    end if
    return false
end function


function MdHilite( byval nLang as long, byref sLine as string, byref nState as long, _
                   spans() as MdSpan, byval nMax as long ) as long

    dim as long n = 0
    dim as long nLen = len(sLine)
    if nMax <= 0 then return 0

    '' Every emit goes through this, and it is the ONLY place that touches `n`. The last
    '' slot is reserved so an overflow can still hand back the rest of the line as text --
    '' see the header: no byte of the source may go missing.
    #macro EMIT(st, ln, kd)
        if n < nMax then
            spans(n).nStart = (st) : spans(n).nLen = (ln) : spans(n).nKind = (kd)
            n += 1
        end if
    #endmacro

    if (nLang = MDL_NONE) orelse (nLen = 0) then
        if nLen > 0 then
            EMIT(0, nLen, MDT_TEXT)
        end if
        return n
    end if

    dim as long i = 0

    '' ---- continuing a block comment from a previous line -------------------------------
    if nState = MDH_STATE_COMMENT then
        dim as long nEnd = -1
        if nLang = MDL_C then
            for j as long = 0 to nLen - 2
                if (sLine[j] = 42) andalso (sLine[j+1] = 47) then nEnd = j + 2 : exit for
            next
        else
            '' FreeBASIC closes with '/ -- apostrophe then slash.
            for j as long = 0 to nLen - 2
                if (sLine[j] = 39) andalso (sLine[j+1] = 47) then nEnd = j + 2 : exit for
            next
        end if
        if nEnd < 0 then
            EMIT(0, nLen, MDT_COMMENT)
            return n
        end if
        EMIT(0, nEnd, MDT_COMMENT)
        nState = MDH_STATE_NONE
        i = nEnd
    end if

    '' ---- a preprocessor / metacommand line ---------------------------------------------
    '' Both languages: the FIRST non-blank being # makes the whole line preprocessor. FB's
    '' #include, #define, #macro and #pragma all land here, and so does C's.
    scope
        dim as long k = i
        while (k < nLen) andalso ((sLine[k] = 32) orelse (sLine[k] = 9))
            k += 1
        wend
        if (k < nLen) andalso (sLine[k] = 35) andalso (i = 0) then
            if k > 0 then
                EMIT(0, k, MDT_TEXT)
            end if
            EMIT(k, nLen - k, MDT_PREPROC)
            return n
        end if
    end scope

    while i < nLen
        dim as long ch = sLine[i]

        '' ---- whitespace ----------------------------------------------------------------
        if (ch = 32) orelse (ch = 9) then
            dim as long j = i
            while (j < nLen) andalso ((sLine[j] = 32) orelse (sLine[j] = 9))
                j += 1
            wend
            EMIT(i, j - i, MDT_TEXT)
            i = j
            continue while
        end if

        '' ---- comments ------------------------------------------------------------------
        if nLang = MDL_FB then
            '' Block comment /' ... '/ -- tested BEFORE the line comment, or the ' in /' is
            '' read as a line comment and the block never opens.
            if (ch = 47) andalso (i + 1 < nLen) andalso (sLine[i+1] = 39) then
                dim as long nEnd = -1
                for j as long = i + 2 to nLen - 2
                    if (sLine[j] = 39) andalso (sLine[j+1] = 47) then nEnd = j + 2 : exit for
                next
                if nEnd < 0 then
                    EMIT(i, nLen - i, MDT_COMMENT)
                    nState = MDH_STATE_COMMENT
                    return n
                end if
                EMIT(i, nEnd - i, MDT_COMMENT)
                i = nEnd
                continue while
            end if
            '' ' to end of line. This covers '' as well, which is the house comment style.
            if ch = 39 then
                EMIT(i, nLen - i, MDT_COMMENT)
                return n
            end if
            '' rem to end of line, but only as a whole word.
            if ((ch = 82) orelse (ch = 114)) andalso (i + 2 < nLen) then
                if HLower(mid(sLine, i + 1, 3)) = "rem" then
                    if (i + 3 >= nLen) orelse (HIsWordCh(sLine[i+3]) = false) then
                        EMIT(i, nLen - i, MDT_COMMENT)
                        return n
                    end if
                end if
            end if
        else
            if (ch = 47) andalso (i + 1 < nLen) andalso (sLine[i+1] = 47) then
                EMIT(i, nLen - i, MDT_COMMENT)
                return n
            end if
            if (ch = 47) andalso (i + 1 < nLen) andalso (sLine[i+1] = 42) then
                dim as long nEnd = -1
                for j as long = i + 2 to nLen - 2
                    if (sLine[j] = 42) andalso (sLine[j+1] = 47) then nEnd = j + 2 : exit for
                next
                if nEnd < 0 then
                    EMIT(i, nLen - i, MDT_COMMENT)
                    nState = MDH_STATE_COMMENT
                    return n
                end if
                EMIT(i, nEnd - i, MDT_COMMENT)
                i = nEnd
                continue while
            end if
        end if

        '' ---- strings -------------------------------------------------------------------
        '' FreeBASIC doubles the quote to escape it; C uses a backslash. Getting this wrong
        '' colours the rest of the line as a string, which is the single most visible
        '' highlighting bug there is.
        if (ch = 34) orelse ((nLang = MDL_C) andalso (ch = 39)) then
            dim as long chQuote = ch
            dim as long j = i + 1
            dim as boolean bClosed = false
            while j < nLen
                if nLang = MDL_C then
                    if sLine[j] = 92 then                       '' backslash escape
                        j += 2
                        continue while
                    end if
                    if sLine[j] = chQuote then
                        j += 1 : bClosed = true : exit while
                    end if
                else
                    if sLine[j] = chQuote then
                        if (j + 1 < nLen) andalso (sLine[j+1] = chQuote) then
                            j += 2
                            continue while
                        end if
                        j += 1 : bClosed = true : exit while
                    end if
                end if
                j += 1
            wend
            '' An unterminated string ends at the line end. Neither language continues one
            '' across a newline without a continuation, and pretending otherwise would bleed
            '' the colour into the next line through nState.
            if bClosed = false then j = nLen
            EMIT(i, j - i, MDT_STRING)
            i = j
            continue while
        end if

        '' ---- numbers -------------------------------------------------------------------
        '' A digit, or a lead-in that only a number can start: &H / &O / &B in FreeBASIC.
        if HIsDigit(ch) orelse cbool((nLang = MDL_FB) andalso (ch = 38)) then
            dim as long j = i
            if ch = 38 then
                j += 1
                if j < nLen then j += 1                          '' the H, O or B
            end if
            while j < nLen
                dim as long c = sLine[j]
                dim as boolean bPart = HIsWordCh(c) orelse (c = 46)
                '' 1e+10 and 1.5e-3: a sign is part of the number only straight after an
                '' exponent marker.
                if (bPart = false) andalso _
                   cbool(((c = 43) orelse (c = 45)) andalso (j > i)) then
                    dim as long cPrev = sLine[j-1]
                    if (cPrev = 101) orelse (cPrev = 69) orelse (cPrev = 100) orelse (cPrev = 68) then
                        bPart = true
                    end if
                end if
                if bPart = false then exit while
                j += 1
            wend
            EMIT(i, j - i, MDT_NUMBER)
            i = j
            continue while
        end if

        '' ---- identifiers and keywords --------------------------------------------------
        if HIsWordCh(ch) then
            dim as long j = i
            while (j < nLen) andalso HIsWordCh(sLine[j])
                j += 1
            wend
            dim as string sWord = mid(sLine, i + 1, j - i)
            if IsKeyword(nLang, sWord) then
                EMIT(i, j - i, MDT_KEYWORD)
            else
                EMIT(i, j - i, MDT_TEXT)
            end if
            i = j
            continue while
        end if

        '' ---- operators and punctuation -------------------------------------------------
        scope
            dim as long j = i
            while j < nLen
                dim as long c = sLine[j]
                if HIsWordCh(c) orelse (c = 32) orelse (c = 9) orelse (c = 34) then exit while
                if (nLang = MDL_C) andalso (c = 39) then exit while
                if (nLang = MDL_FB) andalso (c = 39) then exit while
                if (c = 47) andalso (j + 1 < nLen) andalso _
                   ((sLine[j+1] = 42) orelse (sLine[j+1] = 39) orelse (sLine[j+1] = 47)) then exit while
                j += 1
            wend
            if j = i then j = i + 1              '' always make progress
            EMIT(i, j - i, MDT_OPERATOR)
            i = j
        end scope
    wend

    '' ---- overflow ---------------------------------------------------------------------
    '' If the line needed more spans than the caller allowed, the tail is handed back as one
    '' text span so the renderer still draws every byte -- unhighlighted, but present.
    if (n = nMax) andalso (i < nLen) then
        spans(nMax - 1).nStart = spans(nMax - 1).nStart
        dim as long nCovered = spans(nMax - 1).nStart + spans(nMax - 1).nLen
        if nCovered < nLen then
            spans(nMax - 1).nLen  = nLen - spans(nMax - 1).nStart
            spans(nMax - 1).nKind = MDT_TEXT
        end if
    end if

    return n
end function
