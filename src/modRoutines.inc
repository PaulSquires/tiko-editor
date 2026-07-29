'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#include once "modRoutines.bi"
#include once "frmOutput.bi"
#include once "frmOptionsCompiler.bi"
#include once "frmOptionsLocal.bi"
#include once "frmProjectOptions.bi"
#include once "frmUserTools.bi"
#include once "frmMain.bi"
' This module is included before the control implementations, so the PsListTree family's
' headers are pulled in by hand -- the same prerequisite dance modMenus.inc does.
#include once "PsVScrollBar.bi"
#include once "PsColumnHeader.bi"
#include once "PsListTree.bi"


' ========================================================================================
' Center text vertically in the RichEdit Find/Replace controls.
' ========================================================================================
function RichEditCenterSingleLineText( byval hRichEdit as HWND ) as long
    dim as RECT rc
    GetClientRect(hRichEdit, @rc)

    dim as HFONT hFont = ghFont(INFOFONT_11)  ' cast(HFONT, SendMessage(hRichEdit, WM_GETFONT, 0, 0))
    dim as HDC hdc = GetDC(hRichEdit)
    dim as HFONT hOldFont = SelectObject(hdc, hFont)

    dim as TEXTMETRIC tm
    GetTextMetrics(hdc, @tm)

    SelectObject(hdc, hOldFont)
    ReleaseDC(hRichEdit, hdc)

    dim as long topMargin = ((rc.bottom - rc.top) - tm.tmHeight) / 2
    if (topMargin < 0) then topMargin = 0

    rc.top = topMargin
    SendMessage( hRichEdit, EM_SETRECT, 0, cast(LPARAM, @rc) )
    
    return 0
end function



' ========================================================================================
' Add a row to a multi-column PsListTree from a chr(9)-delimited string.
'
' Every caller that used to feed frmListView_AddString already builds its row in this shape
' ("col0" & chr(9) & "col1" & ...), so this keeps those call sites a one-word change. Field
' 0 becomes the row's own text (column 0's cell, per the PsListTree contract) and fields 1..N
' become cells 1..N.
'
' Returns the new row index, or -1 if the row could not be added.
' ========================================================================================
function ListBox_AddTabbedRow( byval hCtl as HWND, byval wszTabbed as DWSTRING ) as long
    dim as long nFields = AfxStrParseCount( wszTabbed, chr(9) )
    if nFields < 1 then nFields = 1

    ' AfxStrParse is 1-based, so field i+1 is column i -- the same mapping
    ' frmListView_GetItemText used.
    dim as long nRow = PsListTree_AddString( hCtl, AfxStrParse( wszTabbed, 1, chr(9) ) )
    if nRow < 0 then return -1

    for i as long = 1 to nFields - 1
        PsListTree_SetCellText( hCtl, nRow, i, AfxStrParse( wszTabbed, i + 1, chr(9) ) )
    next

    return nRow
end function


' ========================================================================================
' Shared listview-style PsListTree callbacks. All four of tiko's multi-column lists -- the
' Output panel's compiler results, search results and TODO panes, and the debugger's
' Variables pane -- use these; they all draw with the ghOutput colors and the same layout,
' so there is one painter rather than four copies.
'
' Ported from the old frmListView_OnDrawItem. Two faithfulness notes:
'   - That code computed foreclr as iif(IsHot, ForeColorHot, ForeColorHot) -- a degenerate
'     ternary, so the non-hot ForeColor was never actually used and every row drew in
'     ForeColorHot. Reproduced here deliberately, to keep the migration visually identical;
'     switch the else branch to ghOutput.ForeColor if the original intent is wanted.
'   - Column 2 (the filename) gets DT_PATH_ELLIPSIS, as before.
' ========================================================================================
sub ListBox_LVPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    ' The old control highlighted the SELECTED row only; PsListTree tracks hover separately,
    ' so honor both.
    dim as boolean bHot = (p->isSelected orelse p->isHot)
    dim as COLORREF backclr = iif( bHot, ghOutput.ForeColorScrollBar, ghOutput.BackColor )
    p->b->SetForeColor( ghOutput.ForeColorHot )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(GUIFONT_9) )

    ' No columns defined -> single full-width cell (the classic PsListTree contract).
    if p->columnCount <= 0 then
        dim as RECT rcText = p->rc
        rcText.left += pWindow->ScaleX(4)
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )
        exit sub
    end if

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += pWindow->ScaleX(4)      ' the same slight left inset as before
        dim as long flags = DT_LEFT
        if i = 2 then flags = flags or DT_PATH_ELLIPSIS   ' for long filenames
        p->b->PaintText( p->cells[i].wszText, @rcText, flags )
    next
end sub


' Header band painter. Required -- without one the header does not render at all.
sub ListBox_LVPaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    dim as COLORREF backclr = ghOutput.BackColor
    if p->isHot orelse p->isResizeHot then backclr = ghOutput.ForeColorScrollBar
    p->b->SetForeColor( ghOutput.ForeColorHot )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += pWindow->ScaleX(4)
    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' A one-pixel divider on the trailing edge, so columns read as separated.
    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( ghOutput.ScrollBarDivider )
    p->b->PaintRect( @rcDiv )
end sub


' Double-click on a column divider: best fit = the widest cell in that column, plus the
' same padding the debugger's own measure loop uses. Return <= 0 to decline.
function ListBox_LVAutoSizeColumn( byval hHeader as HWND, byval idx as integer ) as integer
    dim as HWND hCtl = GetParent( hHeader )
    if hCtl = 0 then return 0

    dim as long nBest = getTextWidth( hCtl, PsListTree_GetColumnText( hCtl, idx ), GUIFONT_9 )
    for r as integer = 0 to PsListTree_GetCount( hCtl ) - 1
        dim as long w = getTextWidth( hCtl, PsListTree_GetCellText( hCtl, r, idx ), GUIFONT_9 )
        if w > nBest then nBest = w
    next

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    dim as long nPad = 10
    if pWindow then return pWindow->ScaleX( nBest + nPad )
    return nBest + nPad
end function


' ========================================================================================
' If multiple editor instances is disallowed then bring the current active
' instance to the foreground and pass it whatever command line was intended
' for this instance.
' ========================================================================================
function SpawnPreviousInstance() as boolean
    dim as HWND hWindow = FindWindow(APPCLASSNAME, 0)
    if hWindow then
        SetForegroundWindow(hWindow)
        ' Send the command line to the original editor to be processed
        dim as DWSTRING wszPath 
        dim as DWSTRING wszArg  
        dim as DWSTRING wszSendString  

        dim as long i = 1
        do
            wszArg = AfxCommand(i)
            if len(wszArg) = 0 then exit do
            
            ' Remove any double quotes from the argument.
            wszPath = AfxStrRemove( wszArg, wchr(34) )
            
            ' if no path exists for the file then add the current folder
            wszPath = AfxStrPathname( "PATH", wszArg )
            if len(wszPath) = 0 then wszArg = AfxGetExePathName & wszArg 
            
            wszSendString = wszSendString & _
                            iif(len(wszSendString), ";", "") & _
                            wszArg
            i += 1
        loop
        
        dim as COPYDATASTRUCT cds
        cds.dwData = IDM_COPYDATA_COMMANDLINE
        cds.cbData = (len(wszSendString) + 1) * 2  ' unicode
        cds.lpData = wszSendString.vptr
        SendMessage(hWindow, WM_COPYDATA, 0, cast(LPARAM, @cds))
        return true
    end if

    function = false
end function
    

' ========================================================================================
' Reload a Document based on filename.
' - Used in Replace in Files
' - Used in WM_ACTIVATEAPP to reload externally modified file.
' ========================================================================================
function ReloadDocument( byref wszFilename as wstring ) as long
    dim pDoc as clsDocument ptr
    pDoc = gApp.GetDocumentPtrByFilename( wszFilename )

    if pDoc then
        pDoc->DestroyScintillaWindows()
        pDoc->LoadDiskFile( wszFilename )
        pDoc->AssignTextBuffer()
        frmMain_PositionWindows()
    end if
    
    function = 0
end function


' ========================================================================================
' Show context help or general contents if word does not exist
' ========================================================================================
public function ShowContextHelp( byval id as long ) as long

    dim HtmlHelpW as function ( byval hwndCaller as HWND, _
                                byval pswzFile as wstring ptr, _
                                byval uCommand as UNIT, _
                                byval dwData as DWORD_PTR _
                                ) as HWND

    dim as DWSTRING wszHelpFilename, wszLabel
    
    ' Ensure that the CurDrive parameter is converted if applicable
    gConfig.CompilerHelpFile = ProcessFromCurdriveApp( gConfig.CompilerHelpFile ) 

    select case id
        case IDM_HELP_FB
            wszLabel = "FreeBASIC "
            ' Convert relative path to absolute path if needed.
            if AfxPathIsRelative(gConfig.CompilerHelpFile) then
                wszHelpFilename = AfxPathCombine(AfxGetExePathName, gConfig.CompilerHelpFile)
            else
                wszHelpFilename = gConfig.CompilerHelpFile
            end if
    end select

    if AfxFileExists(wszHelpFilename) = 0 then
        MessageBoxW( HWND_FRMMAIN, wszLabel & L(244,"Help file not found."), L(201,"Error"), _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        exit function
    end if


    HtmlHelpW = dylibsymbol( gpHelpLib, "HtmlHelpW" )

    if (gpHelpLib = 0) orelse (HtmlHelpW = 0) then
        MessageBox( HWND_FRMMAIN, L(243,"Error loading HtmlHelp."), L(201,"Error"), _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        exit function
    end if

    ' if we are currently in an active document then attempt to lookup the 
    ' word immediately under the caret.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    
    if pDoc = 0 then
        HtmlHelpW( 0, wszHelpFilename, HH_DISPLAY_TOC, null )
        exit function
    end if

    dim wszKeyword as wstring * MAX_PATH
    
    wszKeyword = wstr( pDoc->GetWord )
    
    dim li as HH_AKLINK
    With li
        .cbStruct     = sizeof(HH_AKLINK) 
        .fReserved    = false 
        .pszKeywords  = @wszKeyword
        .pszUrl       = null
        .pszMsgText   = null
        .pszMsgTitle  = null
        .pszWindow    = null
        .fIndexOnFail = false 
    end With
    
    ' Open the help and show the topic
    HtmlHelpW( 0, wszHelpFilename, HH_DISPLAY_TOC, null )   '<-- needed?
    
    if HtmlHelpW( 0, wszHelpFilename, HH_KEYWORD_LOOKUP, cast(DWORD_PTR, @li) ) = 0 then
        ' Normal case search failed, try a ucase search
        wszKeyword     = ucase(wszKeyword)
        li.pszKeywords = @wszKeyword
        HtmlHelpW( 0, wszHelpFilename, HH_KEYWORD_LOOKUP, cast(DWORD_PTR, @li) )
    end if

    function = 0
    
end function


' ========================================================================================
' Get the width of the text in unscaled pixels because we feed this value
' to the pWindow create control function that will then scale up the value.
' ========================================================================================
function getTextWidth( _
            byval hwnd as HWND, _
            byref wszText as wstring, _
            byval FontIndex as long _
            ) as long
    
    dim size as SIZEL
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit function
    
    dim as HFONT _hFont = ghFont(FontIndex)
    dim as HFONT oldFont 
    
    dim as HDC hDC = GetDC(hwnd)
    
    SaveDC(hDC)
    if _hFont then oldFont = SelectObject(hDC, _hFont)
    GetTextExtentpoint32( hDC, @wszText, len(wszText), @size )
    SelectObject(hDC, oldFont)
    RestoreDC(hDC, -1)
    
    ReleaseDC(hwnd, hDC)
    
    function = pWindow->UnScaleX(size.cx)

end function


' ========================================================================================
' return temporary file name
' ========================================================================================
function GetTemporaryFilename( _
            byref wszFolder as wstring, _
            byref wszExtension as wstring _
            ) as string

    dim wszTempFilename as wstring * MAX_PATH
    if GetTempFileName( @wszFolder, "TMP", 0, @wszTempFilename ) then
        ' Delete the temp file that gets created b/c we will create it ourselves based on the 
        ' returned filename.
        AfxDeleteFile( wszTempFilename )
        if len(wszExtension) then 
            wszTempFilename = left(wszTempFilename, len(wszTempFilename) -  3) & wszExtension
        end if
    end if      
    function = wszTempFilename
end function


' ========================================================================================
' Get the Scintilla value for a character sets
' ========================================================================================
function GetFontCharSetID(byref wzCharsetName as DWSTRING ) as long

    if len(wzCharsetName) = 0 then return SC_CHARSET_DEFAULT
    
    select case wzCharsetName
        case "Default"       : function = SC_CHARSET_DEFAULT
        case "Ansi"          : function = SC_CHARSET_ANSI
        case "Arabic"        : function = SC_CHARSET_ARABIC
        case "Baltic"        : function = SC_CHARSET_BALTIC
        case "Chinese Big 5" : function = SC_CHARSET_CHINESEBIG5
        case "East Europe"   : function = SC_CHARSET_EASTEUROPE
        case "GB 2312"       : function = SC_CHARSET_GB2312
        case "Greek"         : function = SC_CHARSET_GREEK
        case "Hangul"        : function = SC_CHARSET_HANGUL
        case "Hebrew"        : function = SC_CHARSET_HEBREW
        case "Johab"         : function = SC_CHARSET_JOHAB
        case "Mac"           : function = SC_CHARSET_MAC
        case "OEM"           : function = SC_CHARSET_OEM
        case "Russian"       : function = SC_CHARSET_RUSSIAN
        case "Shiftjis"      : function = SC_CHARSET_SHIFTJIS
        case "Symbol"        : function = SC_CHARSET_SYMBOL
        case "Thai"          : function = SC_CHARSET_THAI
        case "Turkish"       : function = SC_CHARSET_TURKISH
        case "Vietnamese"    : function = SC_CHARSET_VIETNAMESE
    end select

end function


' ========================================================================================
' Snapshot the FULL byte extent of a Scintilla buffer.
'
' SCI_GETCHARACTERPOINTER hands back a pointer whose valid length is SCI_GETLENGTH, NOT
' strlen: the buffer can legitimately contain embedded NUL bytes, and reading it with *psz
' (as every save path used to) truncates silently at the first one. Always copy exactly
' SCI_GETLENGTH bytes.
' ========================================================================================
function Scintilla_GetTextBytes( byval hEdit as hwnd ) as string
    dim as long nLen = SciExec( hEdit, SCI_GETLENGTH, 0, 0 )
    if nLen <= 0 then return ""
    dim as any ptr psz = cast( any ptr, SciExec( hEdit, SCI_GETCHARACTERPOINTER, 0, 0 ) )
    if psz = 0 then return ""
    dim as string s = string( nLen, 0 )
    memcpy( strptr(s), psz, nLen )
    return s
end function


' ========================================================================================
' Length-explicit codepage conversions. These are the ONLY conversion primitives the
' encoding code should use: every call passes an explicit byte/char count (never -1), so
' an embedded NUL is preserved rather than acting as a terminator, and the destination
' byte count is computed by the API rather than guessed. AfxNova's DWSTRING conversion
' properties all pass -1 and stop at the first NUL, which is why they are avoided here.
' ========================================================================================

' Decode an ANSI or UTF-8 byte buffer into a UTF-16LE buffer held in a FreeBASIC string
' (2 bytes per code unit). nWideChars receives the number of UTF-16 code units.
private function BytesToWide( byref sBuffer as string, byval nCodePage as long, byref nWideChars as long ) as string
    nWideChars = 0
    if len(sBuffer) = 0 then return ""
    dim as long wlen = MultiByteToWideChar( nCodePage, 0, strptr(sBuffer), len(sBuffer), NULL, 0 )
    if wlen <= 0 then return ""
    dim as string sWide = string( wlen * 2, 0 )
    MultiByteToWideChar( nCodePage, 0, strptr(sBuffer), len(sBuffer), cast(LPWSTR, strptr(sWide)), wlen )
    nWideChars = wlen
    return sWide
end function

' Encode a UTF-16LE buffer (nWideChars code units) to ANSI bytes. bLossy is set true when
' any character had no representation in the active ANSI codepage (WC_NO_BEST_FIT_CHARS
' disables approximate substitution so the flag is meaningful).
private function WideToAnsi( byref sWide as string, byval nWideChars as long, byref bLossy as boolean ) as string
    bLossy = false
    if nWideChars = 0 then return ""
    dim as long alen = WideCharToMultiByte( CP_ACP, WC_NO_BEST_FIT_CHARS, cast(LPCWSTR, strptr(sWide)), nWideChars, NULL, 0, NULL, NULL )
    if alen <= 0 then return ""
    dim as string sAnsi = string( alen, 0 )
    dim as long usedDefault = 0
    WideCharToMultiByte( CP_ACP, WC_NO_BEST_FIT_CHARS, cast(LPCWSTR, strptr(sWide)), nWideChars, strptr(sAnsi), alen, NULL, @usedDefault )
    bLossy = (usedDefault <> 0)
    return sAnsi
end function

' Encode a UTF-16LE buffer (nWideChars code units) to UTF-8 bytes.
private function WideToUtf8( byref sWide as string, byval nWideChars as long ) as string
    if nWideChars = 0 then return ""
    dim as long u8len = WideCharToMultiByte( CP_UTF8, 0, cast(LPCWSTR, strptr(sWide)), nWideChars, NULL, 0, NULL, NULL )
    if u8len <= 0 then return ""
    dim as string sUtf8 = string( u8len, 0 )
    WideCharToMultiByte( CP_UTF8, 0, cast(LPCWSTR, strptr(sWide)), nWideChars, strptr(sUtf8), u8len, NULL, NULL )
    return sUtf8
end function


' ========================================================================================
' The display name of a FILE_ENCODING_* value -- the SINGLE source for all four strings.
'
' There were two hardcoded copies of this list before (the statusbar panel in
' frmMain_SetStatusbar and the statusbar popup in ShowFileEncodingContextMenu), and the
' Options page's "encoding for new files" combo would have made a third. They are now all
' driven from here, which is what makes the combo's ITEM ORDER and the FILE_ENCODING_*
' VALUES provably the same thing: the combo is filled by walking ANSI..UTF16_BOM through
' this function, so the selected index IS the encoding constant and there is no mapping
' table anywhere to fall out of step.
'
' NOT localized, deliberately: these are the encodings' own names, and both call sites this
' replaces spelled them in English under every language file. Changing that is a separate
' decision from adding a setting, and would need all six .lang files either way.
'
' An unknown value answers "ANSI", matching the `case else` the statusbar has always had --
' encoding 0 is the one a zeroed/garbage field lands on.
' ========================================================================================
function Doc_EncodingName( byval nEnc as long ) as DWSTRING
    select case nEnc
        case FILE_ENCODING_UTF8      : return "UTF-8"
        case FILE_ENCODING_UTF8_BOM  : return "UTF-8 (BOM)"
        case FILE_ENCODING_UTF16_BOM : return "UTF-16 (BOM)"
        case else                    : return "ANSI"
    end select
end function


' ========================================================================================
' Produce the exact bytes to write to disk for a document, BOM included.
'
'   sBuffer       - the raw Scintilla bytes (from Scintilla_GetTextBytes)
'   bSourceIsUtf8 - the CURRENT in-editor layout, read from SCI_GETCODEPAGE (NOT from
'                   FileEncoding, which may already have been changed to the target)
'   nTargetEnc    - one of FILE_ENCODING_*
'   bLossy        - out: true if the target could not represent every character (ANSI only)
'
' Pure: string in, string out, no window and no globals. This is the single writer that
' clsDocument.SaveFile / SaveTempFile and frmMainFile's duplicate-file path all share; it
' is also what the Tier-A self-test drives directly.
'
' Note UTF-16 does NOT go through DWSTRING: the wide buffer BytesToWide produced already IS
' the UTF-16LE payload, and its byte length is code-units * 2. The old code handed
' DWSTRING.m_BufferLen (a COUNT OF UTF-16 CHARACTERS) to CFileStream.Write's BYTE-count
' parameter, writing exactly half the file and truncating every UTF-16 save mid-character.
' ========================================================================================
function Doc_EncodeForDisk( _
            byref sBuffer as string, _
            byval bSourceIsUtf8 as boolean, _
            byval nTargetEnc as long, _
            byref bLossy as boolean _
            ) as string

    bLossy = false
    dim as long nWide
    dim as string sWide = BytesToWide( sBuffer, iif(bSourceIsUtf8, CP_UTF8, CP_ACP), nWide )

    select case nTargetEnc
        case FILE_ENCODING_ANSI
            return WideToAnsi( sWide, nWide, bLossy )

        case FILE_ENCODING_UTF8
            return WideToUtf8( sWide, nWide )

        case FILE_ENCODING_UTF8_BOM
            return chr(&HEF, &HBB, &HBF) & WideToUtf8( sWide, nWide )

        case FILE_ENCODING_UTF16_BOM
            ' FF FE (little-endian BOM) + the wide payload, byte length = code-units * 2.
            return chr(&HFF, &HFE) & left( sWide, nWide * 2 )

        case else
            return WideToUtf8( sWide, nWide )
    end select

end function


' ========================================================================================
' Convert the current in-editor text buffer to the byte layout the specified encoding
' needs (ANSI codepage 0, or UTF-8 codepage 65001), and relabel the document.
'
' Returns TRUE whenever the document's encoding actually changed -- INCLUDING a pure relabel
' (e.g. UTF-8 -> UTF-8 BOM), because a relabel still changes what is written to disk (a BOM
' added/removed, UTF-8 vs UTF-16), so the caller must mark the document dirty and let it be
' saved. Returns FALSE only when nothing changed: the document is already that encoding, or a
' lossy ANSI conversion was cancelled by the user.
'
' Scintilla holds exactly two layouts, ANSI or UTF-8; UTF-16 is a DISK format only and its
' in-memory representation is UTF-8. So UTF8 / UTF8_BOM / UTF16_BOM all share one in-editor
' layout, and switching between them is a relabel, not a conversion.
'
' INVARIANT E1: on return, SCI_GETCODEPAGE = iif(FileEncoding = ANSI, 0, SC_CP_UTF8), and
' this function is the ONLY code permitted to move either side of that on a populated
' document (ApplyProperties establishes it on load; nothing else may touch the codepage).
' ========================================================================================
function ConvertTextBuffer( _
            byval pDoc as clsDocument ptr, _
            byval nNewEncoding as long _
            ) as boolean

    if pDoc = 0 then return false
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then return false

    dim as boolean bHaveUtf8 = ( SciExec( hEdit, SCI_GETCODEPAGE, 0, 0 ) = SC_CP_UTF8 )
    dim as boolean bWantUtf8 = ( nNewEncoding <> FILE_ENCODING_ANSI )

    ' Pure relabel: the in-memory layout already matches the target (e.g. UTF-8 -> UTF-8 BOM,
    ' or UTF-8 BOM -> UTF-16 BOM). No byte conversion, only the disk format changes -- but
    ' that IS a change (the BOM must reach disk), so report it as one when the label moved.
    if bHaveUtf8 = bWantUtf8 then
        dim as boolean bChanged = ( pDoc->FileEncoding <> nNewEncoding )
        pDoc->FileEncoding = nNewEncoding
        return bChanged
    end if

    ' Save caret and first-visible line as LINE + COLUMN. Byte offsets shift when the
    ' encoding changes (ANSI grows to UTF-8), so a saved byte position would land in the
    ' wrong place, possibly mid-character.
    dim as long nPos       = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nPos, 0 )
    dim as long nCaretCol  = SciExec( hEdit, SCI_GETCOLUMN, nPos, 0 )
    dim as long nFirstLine = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0 )

    ' Decode the current buffer to UTF-16, then re-encode to the wanted layout.
    dim as string sBuffer = Scintilla_GetTextBytes( hEdit )
    dim as long nWide
    dim as string sWide = BytesToWide( sBuffer, iif(bHaveUtf8, CP_UTF8, CP_ACP), nWide )

    dim as string sNew
    dim as long nNewCodePage
    if bWantUtf8 then
        sNew = WideToUtf8( sWide, nWide )
        nNewCodePage = SC_CP_UTF8
    else
        dim as boolean bLossy
        sNew = WideToAnsi( sWide, nWide, bLossy )
        if bLossy then
            ' Converting to ANSI will destroy characters outside the system ANSI codepage,
            ' and SetText empties the undo buffer, so there is no going back. Confirm first.
            dim as long nAnswer = MessageBoxW( HWND_FRMMAIN, _
                    L(484,"This document contains characters that cannot be represented in ANSI. " & _
                          "Converting will replace them and cannot be undone. Convert anyway?"), _
                    L(485,"Convert to ANSI"), _
                    MB_OKCANCEL or MB_ICONEXCLAMATION or MB_DEFBUTTON2 )
            if nAnswer <> IDOK then return false
        end if
        nNewCodePage = 0
    end if

    ' Apply in the safe order: clear, THEN set the codepage on the empty buffer, THEN insert
    ' the already-converted bytes. Setting the codepage first over the old bytes would make
    ' Scintilla re-validate UTF-8 and fix up positions over the wrong layout.
    SciExec( hEdit, SCI_CLEARALL, 0, 0 )
    SciExec( hEdit, SCI_SETCODEPAGE, nNewCodePage, 0 )
    pDoc->SetText( sNew )

    ' Assign the label LAST, only after the bytes are in place, so a cancel above leaves the
    ' document wholly untouched (INVARIANT E1 preserved on every exit path).
    pDoc->FileEncoding = nNewEncoding

    ' Restore caret and scroll by line + column.
    dim as long nRestore = SciExec( hEdit, SCI_FINDCOLUMN, nCaretLine, nCaretCol )
    SciExec( hEdit, SCI_SETSEL, nRestore, nRestore )
    SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, nFirstLine, 0 )

    return true
end function



' ========================================================================================
' NOTE: Utf8ToAnsi / AnsiToUtf8 / UnicodeToUtf8 were REMOVED here. They converted through
' AfxNova DWSTRING, whose conversion properties pass -1 to Win32 and therefore stop at the
' first embedded NUL, silently truncating. All encoding conversion now goes through the
' length-explicit Doc_EncodeForDisk / BytesToWide / WideToAnsi / WideToUtf8 above.
' ========================================================================================


' ========================================================================================
' Determine if a string is likely UTF-8 encoded.
'
' NOTE: pure ASCII passes UTF-8 validation, so an ASCII file is labelled FILE_ENCODING_UTF8
' here, never ANSI. This is harmless (ASCII is byte-identical in both, and Doc_EncodeForDisk
' writes the same bytes either way) and is deliberately left as-is: making ASCII report ANSI
' would change the on-disk save format of every existing plain-text file in every project.
' ========================================================================================
function isUTF8encoded( byref s as string ) as boolean
    if len(s) = 0 then return false

    ' Optional: Detect UTF-8 BOM
    if len(s) >= 3 then
        if asc(s,1)=&hEF and asc(s,2)=&hBB and asc(s,3)=&hBF then return true
    end if

    dim as long wlen = MultiByteToWideChar( _
        CP_UTF8, _
        MB_ERR_INVALID_CHARS, _   ' strict validation
        strptr(s), _
        len(s), _
        NULL, _
        0)

    return (wlen <> 0)
end function


'function isUTF8encoded(byref text as string) as boolean
'    dim as ubyte ptr p = strptr(text)
'    dim as integer nlen = len(text)
'    dim as integer i = 0
'    dim as boolean hasUTF8Sequences = false

'    while i < nlen
'        dim as ubyte c = p[i]

'        if c < &h80 then
'            ' ASCII character
'            i += 1

'        elseif (c and &hE0) = &hC0 then
'            ' 2-byte UTF-8 sequence
'            if i + 1 >= nlen then return false
'            if (p[i + 1] and &hC0) <> &h80 then return false
'            if c < &hC2 then return false
'            hasUTF8Sequences = true
'            i += 2

'        elseif (c and &hF0) = &hE0 then
'            ' 3-byte UTF-8 sequence
'            if i + 2 >= nlen then return false
'            if (p[i + 1] and &hC0) <> &h80 then return false
'            if (p[i + 2] and &hC0) <> &h80 then return false
'            hasUTF8Sequences = true
'            i += 3

'        elseif (c and &hF8) = &hF0 then
'            ' 4-byte UTF-8 sequence
'            if i + 3 >= nlen then return false
'            if (p[i + 1] and &hC0) <> &h80 then return false
'            if (p[i + 2] and &hC0) <> &h80 then return false
'            if (p[i + 3] and &hC0) <> &h80 then return false
'            hasUTF8Sequences = true
'            i += 4

'        else
'            ' Invalid byte for UTF-8
'            return false
'        end if
'    wend

'    return hasUTF8Sequences
'end function


' ========================================================================================
' Open a disk file and read it into a string (ANSI or UTF8)
' ========================================================================================
function GetFileToString( _
            byref wszFilename as const wstring, _
            byref txtBuffer as string, _
            byval pDoc as clsDocument ptr _
            ) as boolean
    
    if pDoc = 0 then return true
    if AfxFileExists(wszFilename) = false then return true
    
    ' Load the entire file into a string
    dim dwCount as DWORD, dwFileSize as DWORD, dwHighSize as DWORD, dwBytesRead as DWORD
    dim hFile as HANDLE = CreateFileW(@wszFileName, GENERIC_READ, FILE_SHARE_READ, null, _
                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, null)
    if hFile = INVALID_HANDLE_VALUE then return true
    dwFileSize = GetFileSize(hFile, @dwHighSize)
    txtBuffer = string(dwFileSize, 0)
    dim bSuccess as long = ReadFile(hFile, strptr(txtBuffer), dwFileSize, @dwBytesRead, null)
    CloseHandle(hFile)
    if bSuccess = false then return true

    pDoc->FileEncoding = FILE_ENCODING_ANSI

    ' Check for BOM signatures. left() clamps on a string shorter than the count, so a 1- or
    ' 2-byte file cannot match a longer BOM -- no truncated-BOM guard is needed.
    dim as boolean bUtf16BE = false
    if left(txtBuffer, 3) = chr(&HEF, &HBB, &HBF) then
        ' UTF-8 with BOM
        pDoc->FileEncoding = FILE_ENCODING_UTF8_BOM
        txtBuffer = mid(txtBuffer, 4)   ' bypass the BOM
    elseif left(txtBuffer, 2) = chr(&HFF, &HFE) then
        ' UTF-16 little-endian BOM
        pDoc->FileEncoding = FILE_ENCODING_UTF16_BOM
        txtBuffer = mid(txtBuffer, 3)   ' bypass the BOM
    elseif left(txtBuffer, 2) = chr(&HFE, &HFF) then
        ' UTF-16 big-endian BOM. Same on-disk encoding class; byte-swap to LE below so the
        ' one UTF-16 decode path handles both. (Was previously read as ANSI -> interleaved NULs.)
        pDoc->FileEncoding = FILE_ENCODING_UTF16_BOM
        bUtf16BE = true
        txtBuffer = mid(txtBuffer, 3)   ' bypass the BOM
    else
        if isUTF8encoded(txtBuffer) then
            pDoc->FileEncoding = FILE_ENCODING_UTF8
        end if
    end if

   select case pDoc->FileEncoding
      case FILE_ENCODING_ANSI
         ' No conversion needed. clsDocument ApplyProperties will *not*
         ' set the editor to UTF8 code.

      case FILE_ENCODING_UTF8, FILE_ENCODING_UTF8_BOM
         ' No conversion needed. clsDocument ApplyProperties will set
         ' the editor to UTF8 code.

      case FILE_ENCODING_UTF16_BOM
         ' Guard an odd byte count (a corrupt or truncated UTF-16 file): drop the stray
         ' trailing byte rather than reading one past the end of the last code unit.
         if (len(txtBuffer) and 1) then txtBuffer = left(txtBuffer, len(txtBuffer) - 1)

         ' Big-endian: swap each 2-byte code unit in place to little-endian.
         if bUtf16BE then
            dim as ubyte ptr pb = cast(ubyte ptr, strptr(txtBuffer))
            for i as long = 0 to len(txtBuffer) - 2 step 2
               dim as ubyte t = pb[i] : pb[i] = pb[i+1] : pb[i+1] = t
            next
         end if

         ' The buffer now holds a UTF-16LE payload; decode it to UTF-8 for the editor with an
         ' explicit code-unit count (len \ 2), so an embedded NUL does not terminate it early.
         txtBuffer = WideToUtf8( txtBuffer, len(txtBuffer) \ 2 )

    end select

    function = false
end function


' ========================================================================================
' Try to complete the #include filename so that it resolves to a valid path
' Used when right clicking on filename in the editor.
' Used when parsing source code file so that module compiling can check if embedded
' include files have changed and requires the module bas file to recompile.
' ========================================================================================
function CompleteIncludeFilename( byval pDoc as clsDocument ptr, byval sFilename as string ) as string
    dim wszPath             as DWSTRING
    dim wszCompilerPath     as DWSTRING
    dim wszText             as DWSTRING
    dim wszCompleteFilename as DWSTRING
    dim i                   as long

    dim as long idxBuild = frmBuildConfig_getActiveBuildIndex()
    if idxBuild = -1 then idxBuild = 0
    if gConfig.Builds(idxBuild).Is32bit then wszCompilerPath = gConfig.FBWINcompiler32
    if gConfig.Builds(idxBuild).Is64bit then wszCompilerPath = gConfig.FBWINcompiler64
    wszCompilerPath = ProcessFromCurdriveApp(wszCompilerPath)
    wszCompilerPath = AfxStrPathname( "PATH", wszCompilerPath ) & "inc"

    ' Convert relative path to absolute path if needed.
    if AfxPathIsRelative(wszCompilerPath) then
        wszCompilerPath = AfxPathCombine(AfxGetExePathName, wszCompilerPath)
    end if

    ' remove any comments at the end of the line
    i = instr(sFilename, "'")
    if i then sFilename = left(sFilename, i-1)
    sFilename = trim(sFilename, any chr(32,34))  ' remove spaces and double quotes
    wszPath = AfxStrPathname( "PATH", pDoc->DiskFilename )

    if AfxFileExists(wszPath & sFilename) then 
        wszCompleteFilename = wszPath & sFilename
    elseif AfxFileExists(sFilename) then 
        wszCompleteFilename = sFilename
    elseif AfxFileExists(AfxGetCurDir & "\" & sFilename) then 
        wszCompleteFilename = AfxGetCurDir & "\" & sFilename
    elseif AfxFileExists(AfxGetExePathName & sFilename) then 
        wszCompleteFilename = AfxGetExePathName & sFilename
    elseif AfxFileExists(str(wszCompilerPath) & "\" & sFilename) then 
        wszCompleteFilename = str(wszCompilerPath) & "\" & sFilename
    else
        ' Iterate the optional semi colon separated list of compiler include paths
        dim as DWSTRING includePath
        dim as long numpaths = AfxStrParseCount(gConfig.CompilerIncludes, ";")
        for i as long = 1 to numpaths
            includePath = rtrim(AfxStrParse(gConfig.CompilerIncludes, i, ";")) & "\" & sFilename
            if AfxFileExists(includePath) then
                wszCompleteFilename = includePath
                exit for
            end if
        next
    end if
    wszCompleteFilename = AfxStrReplace(wszCompleteFilename, "/", "\")
    wszCompleteFilename = AfxStrReplace(wszCompleteFilename, "\\", "\")

    function = wszCompleteFilename
end function


' ========================================================================================
' Determine if current line is a valid #Include filename
' ========================================================================================
function IsCurrentLineIncludeFilename() as boolean
    ' Determine if the text under the current line is a valid #Include filename
    ' and return true if it is. if F6 was pressed then the calling program can
    ' simply open/load the gApp.IncludeFilename. if the right click popup menu
    ' is to be shown then simply add the option to open this file.

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
    
    dim as long nLine  = pDoc->GetCurrentLineNumber()
    dim as string sLine = ltrim(pDoc->GetLine(nLine))
    
    dim as string sFilename 

    if left(ucase(sLine), 9) = "#INCLUDE " then sFilename = mid(sLine, 10)
    if left(ucase(sLine), 14) = "#INCLUDE ONCE " then sFilename = mid(sLine, 15)

    gApp.IncludeFilename = ""   
    if len(sFilename) then
        gApp.IncludeFilename = CompleteIncludeFilename( pDoc, sFilename )
    end if
    
    function = AfxFileExists( gApp.IncludeFilename ) 

end function


' ========================================================================================
' Get the original case filename as stored by Windows (used when converting the GDB
' returned filename from uppercase to original case).
' ========================================================================================
function AfxFilenameOriginalCase( byval filename as DWSTRING ) as DWSTRING
    dim as long bufSize = 1024
    dim as DWSTRING buffer = wspace(bufSize \ 2)

    dim as HANDLE hfile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, _
                                null, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null)

    if hfile = INVALID_HANDLE_VALUE then exit function

    dim as DWORD result = GetFinalPathNameByHandleW( hfile, *buffer, bufSize, FILE_NAME_NORMALIZED )

    if (result > 0) andalso (result < bufSize) then
        ' Good data returned but still need to remove the mapping characters
        if left(buffer, 4) = "\\?\" then buffer = mid(buffer, 5)
    else
        ' error: could not retrieve path. 
    end if

    ' clean up
    CloseHandle(hfile)
    
    function = buffer
end function


' ========================================================================================
' Generic open document handler for when function ListBox item selected or Explorer Treeview
' ========================================================================================
function OpenSelectedDocument( _
            byref wszFilename as wstring, _
            byref wszFunctionName as wstring = "", _
            byval nLineNumber as long = -1 _
            ) as clsDocument ptr

    ' This function is called in the following situations:
    '   1. When a selection is made in the Function List.
    '   2. When a selection is made through the Explorer treeview.
    '   3. When a Find In Files line is selected.
    '   4. When a Goto Definition word is clicked on.
    '   5. When a compile error occurs and need to position to the error line.
    '   6. When right-click select #Include file to open. 
    
    ' if incoming FunctionName then search for its filename and line number.
    if len( wszFunctionName ) andalso (nLineNumber = -1) then
        dim as SYMBOLREF refProc = gSymDb.FindProc( wszFunctionName )
        if refProc.IsValid then
            dim as wstring ptr pFile = gSymDb.SymFile( refProc )
            if pFile then
                wszFilename = *pFile
                nLineNumber = gSymDb.SymBodyLine( refProc )
                if nLineNumber = 0 then nLineNumber = gSymDb.SymLine( refProc )
                nLineNumber -= 1                ' Scintilla lines are 0-based
                if nLineNumber < 0 then nLineNumber = 0
            end if
        end if
    end if

    ' Normalize scan-sourced UPPERCASED names to their on-disk casing before the
    ' name can become a document's DiskFilename (and with it the tab caption).
    dim as DWSTRING wszOpenName = FilenameOriginalCase( wszFilename )

    ' Not all documents exist on disk file. for example, a QuickRun file will exist in
    ' the editor but may never have a disk footprint. We need to search the project to
    ' determine if the filename has a pDoc already associated with it. If it does, then
    ' pass that pDoc rather than looking for a disk filename.
    dim pDoc as clsDocument ptr
    pDoc = gApp.GetDocumentPtrByFilename( wszOpenName )

    if pDoc = 0 then
        if AfxFileExists(wszOpenName) = false then exit function
        pDoc = gApp.CreateEmptyDocument()
        pDoc->LoadDiskFile( wszOpenName )
    end if

    if pDoc then
        ' if the document is already open and loaded then simply switch to
        ' that document in the top tabcontrol, otherwise load the Explorer 
        ' files list, create a new tab, and switch to it.
        dim as long iTab = gTTabCtl.GetTabIndexByDocumentPtr( pDoc )

        ' Add the new document to the top tabcontrol
        if iTab = -1 then iTab = gTTabCtl.AddTab( pDoc )  
        pDoc->AssignTextBuffer()
        gTTabCtl.SetFocusTab(iTab)

        ' Set the top line to display in the editor. I chose to start 3 lines before the
        ' function just to make it visually more appealing.
        ' do not reposition if incoming LineNumber is -1 because that value represents
        ' the caller specifically not wanting a repositioning.
        if nLineNumber <> -1 then 
            dim as hwnd hEdit = pDoc->hwndActiveScintilla
            ' ensure that the line is visible (not folded)
            if SciExec( hEdit, SCI_GETLINEVISIBLE, nLineNumber, 0) = false then
                ' unfold the block that contains this hidden line
                pDoc->FoldToggle( nLineNumber )
            end if
            SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, max(nLineNumber - 3, 0), 0) 
            SciExec( hEdit, SCI_GOTOLINE, nLineNumber, 0) 
            pDoc->CenterCurrentLine
        end if
    end if 
    
    function = pDoc
end function


' ========================================================================================
' Get the original case filename as stored by Windows. The fbcParser scans intern
' filenames UPPERCASED, so any scan-sourced name that reaches the UI (panel
' captions, tab captions, TODO rows, search results) is normalized through here.
' Returns the input unchanged when the file cannot be opened (deleted files,
' the scanner's synthetic UNTITLED names) or the path cannot be resolved.
' ========================================================================================
function FilenameOriginalCase( byval wszFilename as DWSTRING ) as DWSTRING
    dim as long bufSize = 1024
    dim as DWSTRING buffer = wspace(bufSize \ 2)

    dim as HANDLE hFile = CreateFileW( wszFilename, GENERIC_READ, FILE_SHARE_READ, _
                                       null, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null )
    if hFile = INVALID_HANDLE_VALUE then return wszFilename

    dim as DWORD result = GetFinalPathNameByHandleW( hFile, buffer.vptr, bufSize, FILE_NAME_NORMALIZED )
    CloseHandle( hFile )

    if (result > 0) andalso (result < bufSize) then
        buffer = left( buffer, result )      ' drop the unwritten buffer tail
        ' Good data returned but still need to remove the mapping characters
        if left( buffer, 4 ) = "\\?\" then buffer = mid( buffer, 5 )
        return buffer
    end if

    ' error: could not retrieve the path - display the incoming name as-is
    return wszFilename
end function


' ========================================================================================
' Process prefix {CURDRIVE} and convert to current drive letter.
' ========================================================================================
function ProcessToCurdriveApp( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the editor. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim as DWSTRING wszText = AfxGetExePathName

    dim wszCurDrive as DWSTRING = lcase(left(wszText, 3))  ' eg. D:\

    ' if the incoming filename is a relative file name then the following test
    ' will have no effect.
    if lcase(left(wszFilename, 3)) = wszCurDrive then 
        wszFilename = wstr("{CURDRIVE}") & mid(wszFilename, 2)
    end if

    return wszFilename
end function


' ========================================================================================
' Process current drive to prefix {CURDRIVE} 
' ========================================================================================
function ProcessFromCurdriveApp( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the editor. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim as DWSTRING wszText = AfxGetExePathName

    if ucase(left(wszFilename, 10)) = wstr("{CURDRIVE}") then 
        wszFilename = left(wszText, 1) & mid(wszFilename, 11)
    end if

    return wszFilename
end function


' ========================================================================================
' Process prefix {CURDRIVE} and convert to current drive letter.
' ========================================================================================
function ProcessToCurdriveProject( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the project file. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim wszText as DWSTRING = gApp.ProjectFilename

    dim wszCurDrive as DWSTRING = lcase(left(wszText, 3))  ' eg. D:\

    ' if the incoming filename is a relative file name then the following test
    ' will have no effect.
    if lcase(left(wszFilename, 3)) = wszCurDrive then 
        wszFilename = wstr("{CURDRIVE}") & mid(wszFilename, 2)
    end if

    return wszFilename
end function


' ========================================================================================
' Process current drive to prefix {CURDRIVE} 
' ========================================================================================
function ProcessFromCurdriveProject( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the project file. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim wszText as DWSTRING = gApp.ProjectFilename
    
    if ucase(left(wszFilename, 10)) = wstr("{CURDRIVE}") then 
        wszFilename = left(wszText, 1) & mid(wszFilename, 11)
    end if

    return wszFilename
end function


' ========================================================================================
' Displays the FileOpenDialog.
' The returned pointer must be freed with CoTaskMemFree
' ========================================================================================
function AfxIFileOpenDialogW( _
            byval hwndOwner as HWND, _
            byval idButton as long _
            ) as wstring ptr

    dim hr as long
    dim CLSID_FileOpenDialog as CLSID = (&hDC1C5A9C, &hE88A, &h4DDE, {&hA5, &hA1, &h60, &hF8, &h2A, &h20, &hAE, &hF7})
    dim IID_IFileOpenDialog as GUID   = (&hD57C7288, &hD4AD, &h4768, {&hBE, &h02, &h9D, &h96, &h95, &h32, &hD9, &h60})

    ' Create an instance of the FileOpenDialog object
    dim pofd as IFileOpenDialog ptr
    hr = CoCreateInstance(@CLSID_FileOpenDialog, null, CLSCTX_INPROC_SERVER, @IID_IFileOpenDialog, @pofd)
    if pofd = null then return null

    ' Set the file types depending on the button pushed that calls this open dialog
    dim rgFileTypes(1 To 5) as COMDLG_FILTERSPEC

    select case idButton
        case IDM_LOADSESSION  
            rgFileTypes(1).pszName = @wstr("Session files")
            rgFileTypes(1).pszSpec = @wstr("*.session")
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 2, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(426,"Load Session"))

        case IDM_PROJECTOPEN
            rgFileTypes(1).pszName = @L(216,"Project files")
            rgFileTypes(1).pszSpec = @wstr("*" & APPEXTENSION)
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 2, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(216,"Project files"))
        
        case IDM_INSERTFILE
            rgFileTypes(1).pszName = @L(106,"Source files")
            rgFileTypes(2).pszName = @L(77,"Code files")
            rgFileTypes(3).pszName = @L(78,"Header files")
            rgFileTypes(4).pszName = @L(209,"Resource files")
            rgFileTypes(5).pszName = @L(79,"All files")
            rgFileTypes(1).pszSpec = @wstr("*.bas;*.bi;*.inc;*.rc")
            rgFileTypes(2).pszSpec = @wstr("*.bas;*.inc")
            rgFileTypes(3).pszSpec = @wstr("*.bi")
            rgFileTypes(4).pszSpec = @wstr("*.rc")
            rgFileTypes(5).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 5, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(80,"Insert File"))

        case IDC_FRMOPTIONSLOCAL_CMDLOCALIZATION  '1012
            rgFileTypes(1).pszName = @L(102,"Localization files")
            rgFileTypes(1).pszSpec = @wstr("*.lang")
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 2, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(103,"Open Localization File"))
    
            dim as DWSTRING wszDefaultFolder = AfxGetExePathName & wstr("settings\languages\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = pofd->lpVtbl->SetFolder(pofd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

        case IDC_FRMUSERTOOLS_CMDBROWSEEXE
            rgFileTypes(1).pszName = @L(79,"All files")
            rgFileTypes(1).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 1, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(4,"Open File"))
    
            dim as DWSTRING wszDefaultFolder = AfxGetExePathName & wstr("settings\tools\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = pofd->lpVtbl->SetFolder(pofd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

        case IDC_FRMOPTIONSCOMPILER_CMDFBHELPFILE 
            rgFileTypes(1).pszName = @L(104,"Help file")
            rgFileTypes(1).pszSpec = @wstr("*.chm")
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 2, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(105,"Find Help File"))

            dim as DWSTRING wszDefaultFolder = AfxGetExePathName & wstr("settings\help\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = pofd->lpVtbl->SetFolder(pofd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

    end select
    
    ' Display the dialog
    hr = pofd->lpVtbl->Show(pofd, hwndOwner)
    hr = pofd->lpVtbl->SetOptions(pofd, FOS_NOCHANGEDIR)

    ' Get the result
    dim pItem as IShellItem ptr
    dim pwszName as wstring ptr
    if SUCCEEDED(hr) then
        hr = pofd->lpVtbl->GetResult(pofd, @pItem)
        if SUCCEEDED(hr) then
            hr = pItem->lpVtbl->GetDisplayName(pItem, SIGDN_FILESYSPATH, @pwszName)
            function = pwszName
        end if
    end if

    ' Cleanup
    if pItem then pItem->lpVtbl->Release(pItem)
    if pofd then pofd->lpVtbl->Release(pofd)

end function


' ========================================================================================
' Displays the FileOpenDialog (multiple selection)
' returns a pointer to the IShellItemArray collection.
' ========================================================================================
function AfxIFileOpenDialogMultiple( _
            byval hwndOwner as hwnd, _
            byval idButton as long _
            ) as IShellItemArray ptr

    ' Create an instance of the FileOpenDialog interface
    dim hr as long
    dim pofd as IFileOpenDialog ptr
    hr = CoCreateInstance( @CLSID_FileOpenDialog, null, CLSCTX_INPROC_SERVER, _
                                @IID_IFileOpenDialog, @pofd)
    if pofd = null then return null

    ' Set the file types
    dim rgFileTypes(1 To 5) as COMDLG_FILTERSPEC

    select case idButton
        case IDM_FILEOPEN
            rgFileTypes(1).pszName = @L(106,"Source files")
            rgFileTypes(2).pszName = @L(77,"Code files")
            rgFileTypes(3).pszName = @L(78,"Header files")
            rgFileTypes(4).pszName = @L(209,"Resource files")
            rgFileTypes(5).pszName = @L(79,"All files")
            rgFileTypes(1).pszSpec = @wstr("*.bas;*.bi;*.inc;*.rc")
            rgFileTypes(2).pszSpec = @wstr("*.bas;*.inc")
            rgFileTypes(3).pszSpec = @wstr("*.bi")
            rgFileTypes(4).pszSpec = @wstr("*.rc")
            rgFileTypes(5).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 5, @rgFileTypes(1))
    end select

    ' Set the title of the dialog
    hr = pofd->lpVtbl->SetTitle(pofd, L(248,"Open file"))

    ' Set the default folder to display in the open dialog
    dim wszDefaultFolder as wstring * MAX_PATH
    if gApp.wszLastOpenFolder = "" then 
        if AfxFileExists( gApp.ProjectFilename ) then 
            wszDefaultFolder = AfxStrPathName( "PATH", gApp.ProjectFilename )
        else
            wszDefaultFolder = AfxGetCurDir
        end if
    else
        wszDefaultFolder = gApp.wszLastOpenFolder
    end if
    
    dim pFolder as IShellItem ptr
    SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
    if pFolder then
        hr = pofd->lpVtbl->SetFolder(pofd, pFolder)
        if SUCCEEDED(hr) then
            pFolder->lpVtbl->Release(pFolder)
        end if      
    end if
    
    ' Allow multiselection
    hr = pofd->lpVtbl->SetOptions(pofd, FOS_ALLOWMULTIselect or FOS_NOCHANGEDIR or FOS_FILEMUSTEXIST)
    ' Display the dialog
    hr = pofd->lpVtbl->Show(pofd, hwndOwner)

    ' Get the result
    dim pItemArray as IShellItemArray ptr
    if SUCCEEDED(hr) then
        hr = pofd->lpVtbl->GetResults(pofd, @pItemArray)
        function = pItemArray
    end if

    if pofd then pofd->lpVtbl->Release(pofd)

end function


' ========================================================================================
' Displays the FileSaveDialog
' The returned pointer must be freed with CoTaskMemFree
' ========================================================================================
function AfxIFileSaveDialog( _
            byval hwndOwner as hwnd, _
            byval pwszFileName as wstring ptr, _    ' full path and filename
            byval pwszDefExt as wstring ptr, _
            byval id as long = 0, _
            byval sigdnName as SIGDN = SIGDN_FILESYSPATH _
            ) as wstring ptr

    ' // Create an instance of the IFileSaveDialog interface
    dim rgFileTypes(1 To 5) as COMDLG_FILTERSPEC 
    dim hr as long
    dim psfd as IFileSaveDialog ptr
    hr = CoCreateInstance(@CLSID_FileSaveDialog, null, CLSCTX_INPROC_SERVER, @IID_IFileSaveDialog, @psfd)
    if psfd = null then return null

    dim as DWSTRING wszFilename, wszFilePath 
    
    ' Add extensions if it does not already exist as part of the filename
    wszFilename = AfxStrPathname( "NAMEX", *pwszFileName )
    if len(wszFilename) then 
        if AfxStrPathname( "EXTN", wszFilename ) = "" then
            if len(*pwszDefExt) then
            wszFilename = wszFilename & "." & *pwszDefExt
            end if   
        end if   
    end if
    
    if AfxFileExists( *pwszFileName ) then
        wszFilePath = AfxStrPathName( "PATH", *pwszFileName )
    else   
        ' Set the default folder to save the file
        if gApp.wszLastOpenFolder then 
            wszFilePath = gApp.wszLastOpenFolder
        else
            ' New file being saved try to default to the project folder
            if AfxFileExists( gApp.ProjectFilename ) then
                wszFilePath = AfxStrPathname( "PATH", gApp.ProjectFilename )
            end if
        end if
        gApp.wszLastOpenFolder = wszFilePath
    end if

    ' Set the file types
    select case id
        case IDM_SAVESESSION
            rgFileTypes(1).pszName = @wstr("Session files")
            rgFileTypes(1).pszSpec = @wstr("*.session")
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            ' // Set the title of the dialog
            hr = psfd->lpVtbl->SetTitle(psfd, L(425,"Save Session"))

        case IDM_FILERENAME
            rgFileTypes(1).pszName = @L(79,"All files")
            rgFileTypes(1).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 1, @rgFileTypes(1))
            ' // Set the title of the dialog
            hr = psfd->lpVtbl->SetTitle(psfd, L(21,"Rename"))

        case IDC_FRMPROJECTOPTIONS_CMDSELECT, IDM_PROJECTSAVE, IDM_PROJECTSAVEAS
            rgFileTypes(1).pszName = @L(216,"Project files")
            rgFileTypes(1).pszSpec = @wstr("*" & APPEXTENSION)
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            ' // Set the title of the dialog
            hr = psfd->lpVtbl->SetTitle(psfd, L(185,"Save Project As..."))

        case IDC_FRMOPTIONSLOCAL_CMDNEW
            rgFileTypes(1).pszName = @L(102,"Localization files")
            rgFileTypes(1).pszSpec = @wstr("*.lang")
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            psfd->lpVtbl->SetTitle(psfd, L(8,"Save As..."))

            dim as DWSTRING wszDefaultFolder = AfxGetExePathName & wstr("settings\languages\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = psfd->lpVtbl->SetFolder(psfd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

        case IDC_THEMES_SAVEAS
            rgFileTypes(1).pszName = @L(486,"Themes")
            rgFileTypes(1).pszSpec = @wstr("*.theme")
            rgFileTypes(2).pszName = @L(79,"All files")
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            psfd->lpVtbl->SetTitle(psfd, L(8,"Save As..."))

            dim as DWSTRING wszDefaultFolder = AfxGetExePathName & wstr("settings\themes\")
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = psfd->lpVtbl->SetFolder(psfd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if
            end if

        case else
            rgFileTypes(1).pszName = @L(106,"Source files")
            rgFileTypes(2).pszName = @L(77,"Code files")
            rgFileTypes(3).pszName = @L(78,"Header files")
            rgFileTypes(4).pszName = @L(209,"Resource files")
            rgFileTypes(5).pszName = @L(79,"All files")
            rgFileTypes(1).pszSpec = @wstr("*.bas;*.bi;*.inc;*.rc")
            rgFileTypes(2).pszSpec = @wstr("*.bas;*.inc")
            rgFileTypes(3).pszSpec = @wstr("*.bi")
            rgFileTypes(4).pszSpec = @wstr("*.rc")
            rgFileTypes(5).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 5, @rgFileTypes(1))
            psfd->lpVtbl->SetTitle(psfd, L(8,"Save As..."))
            if pwszDefExt then 
                if *pwszDefExt = "inc" then
                    psfd->lpVtbl->SetFileTypeIndex(psfd, 2)
                end if
            end if
    end select
    
    ' // Set the file name
    hr = psfd->lpVtbl->SetFileName(psfd, wszFileName)
    ' // Set the extension
    hr = psfd->lpVtbl->SetDefaultExtension(psfd, pwszDefExt)

    ' // Set the default folder to display in the save dialog
    if (id <> IDC_FRMOPTIONSLOCAL_CMDNEW) andalso (id <> IDC_THEMES_SAVEAS) then
        if len(wszFilePath ) then
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszFilePath, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = psfd->lpVtbl->SetFolder(psfd, pFolder)
                if SUCCEEDED(hr) then
                pFolder->lpVtbl->Release(pFolder)
                end if      
            end if
        end if
    end if
    
    ' // Display the dialog
    hr = psfd->lpVtbl->Show(psfd, hwndOwner)

    ' // Get the result
    dim pItem as IShellItem ptr
    dim pwszName as wstring ptr
    if SUCCEEDED(hr) then
        hr = psfd->lpVtbl->GetResult(psfd, @pItem)
        if SUCCEEDED(hr) then
            hr = pItem->lpVtbl->GetDisplayName(pItem, sigdnName, @pwszName)
            function = pwszName
        end if
    end if
    ' // Cleanup
    if pItem then pItem->lpVtbl->Release(pItem)
    if psfd then psfd->lpVtbl->Release(psfd)

end function


' ========================================================================================
' Inserts an item at a specific location in the ListView.
' ========================================================================================
function LV_InsertItem( _
            byval hwndControl as HWND, _
            byval iRow        as long, _         
            byval iColumn     as long, _
            byval pwszText    as wstring ptr, _
            byval lParam      as LPARAM = 0 _
            ) as boolean

    dim lvi as LVITEMW
    lvi.iItem     = iRow
    lvi.iSubItem  = iColumn 
    lvi.pszText   = pwszText
    lvi.lParam    = lParam
    if iColumn = 0 then
        lvi.mask = LVIF_TEXT or LVIF_PARAM or LVIF_IMAGE 
        function = SendMessage( hwndControl, LVM_INSERTITEM, 0, cast(LPARAM, @lvi) )
    else 
        lvi.mask = LVIF_TEXT or LVIF_IMAGE
        function = SendMessage( hwndControl, LVM_SETITEM, 0, cast(LPARAM, @lvi) )
    end if
end function


' ========================================================================================
' Retrieves the text of a ListView item.
' ========================================================================================
function LV_GetItemText( _
            byval hwndControl as HWND, _
            byval iRow as long, _
            byval iColumn as long, _
            byval pwszText as wstring ptr, _
            byval nTextMax as long _
            ) as boolean

    if pwszText = 0 then return false
    if nTextMax = 0 then return false
    dim lvi as LVITEMW

    lvi.mask       = LVIF_TEXT
    lvi.iItem      = iRow
    lvi.iSubItem   = iColumn 
    lvi.pszText    = pwszText
    lvi.cchTextMax = nTextMax
        
    function = SendMessage( hwndControl, LVM_GETITEM, 0, cast(LPARAM, @lvi) )
end function


' ========================================================================================
' Set the text for the specified row and col item
' ========================================================================================
function LV_SetItemText( _
            byval hwndControl as HWND, _
            byval iRow as long, _
            byval iColumn as long, _
            byval pwszText as wstring ptr, _
            byval nTextMax as long _
            ) as long

    dim li as LV_ITEM
    li.mask       = LVIF_TEXT
    li.iItem      = iRow
    li.iSubItem   = iColumn 
    li.pszText    = pwszText
    li.cchTextMax = nTextMax
    function = SendMessage( hwndControl, LVM_SETITEM, 0, cast(LPARAM, @li) )
end function


' ========================================================================================
' Load a .lang localization file from disk and populate the localization array
' The IsEnglish parameter is used when we want to populate the gLangEnglish global
' array that is used in the editor startup code.
' ========================================================================================
function LoadLocalizationFile( _
            byref wszFileName as DWSTRING, _
            byval IsEnglish as boolean = false _
            ) as boolean

    ' default that the file failed to load
    function = false
    if AfxFileExists( wszFileName ) = 0 then exit function

    dim as BSTRING wst, wKey, wData
    dim nKey  as long
    dim nData as long  
    dim i     as long
    
    dim pStream as CTextStream
    if pStream.OpenUnicode( wszFileName ) <> S_OK then exit function
    
    do until pStream.EOS
        wst = pStream.ReadLine
        
        if len(wst) = 0 then continue Do
        if left(wst, 1) = "'" then continue Do
        
        i = instr(wst, ":")
        if i = 0 then continue Do
        
        wKey = "": wData = "": nData = 0

        wKey  = left(wst, i-1)
        wData = mid(**wst, i+1)    ' MID causes problems with Chinese data so ** is used.
        
        nKey  = val(wKey)
        nData = val(wData)

        if ucase(wKey) = "MAXIMUM" then
            ' resize the global dynamic array
            if IsEnglish then
                redim gLangEnglish(nData) as wstring * MAX_PATH
            else   
                redim LL(nData) as wstring * MAX_PATH
            end if
        else
            ' this should be a key/value pair line in the format:
            ' 00001:value
            ' Ensure that we add the value to the array within the valid
            ' boundaries of the array.
            if IsEnglish then
                if (nKey >= lbound(gLangEnglish)) andalso (nKey <= ubound(gLangEnglish)) then
                    ' Use ** to ensure that cyrillic langauge gets converted correctly. FB intrinsic
                    ' functions (RTRIM) automatically convert those incorrectly when using CBSTR or DWSTRING.
                    gLangEnglish(nKey) = rtrim(**AfxStrParse(wData, 1, ";"), any chr(9,32))
                end if
            else
                if (nKey >= lbound(LL)) andalso (nKey <= ubound(LL)) then
                    ' Remove any comments from end of the line. Comments begin with
                    ' a semicolon character.
                    ' Use ** to ensure that cyrillic langauge gets converted correctly. FB intrinsic
                    ' functions (RTRIM) automatically convert those incorrectly when using CBSTR or DWSTRING.
                    LL(nKey) = rtrim(**AfxStrParse(wData, 1, ";"), any chr(9,32))
                    ' if the local phrase is empty then fill it using the English version.
                    if len(LL(nKey)) = 0 then 
                        if (nKey >= lbound(gLangEnglish)) andalso (nKey <= ubound(gLangEnglish)) then
                            LL(nKey) = gLangEnglish(nKey)
                        end if
                    end if   
                end if
            end if
        end if   
            
    loop
    pStream.Close
    
    function = true
end function


' ========================================================================================
' Get the full process image name
' ========================================================================================
function GetProcessImageName( _
            byval pe32w as PROCESSENTRY32W ptr, _
            byval pwszExeName as wstring ptr _
            ) as long

    dim dwSize as long
    dim hProcess as HANDLE 
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION or PROCESS_VM_READ, 1, pe32w->th32ProcessID)
    if hProcess then
        dwSize = MAX_PATH
        
'      Without using dynamic loading...      
'      QueryFullProcessImageNameW( hProcess, 0, pwszExeName, @dwSize ) 
'      CloseHandle hProcess

        ' QueryFullProcessImageNameW is only available in Vista or higher. Try to dynamically load the 
        ' function because statically linking to it will cause a runtime error if the editor is run using WinXP.
        dim as any ptr hLib = dylibload("Kernel32")
        if hLib then
            dim MyQueryFullProcessImageName as function( byval hProcess as HANDLE, byval dwFlags as DWORD, byval lpExeName as LPWSTR, byval lpdwSize as PDWORD) as WINBOOL
            MyQueryFullProcessImageName = dylibsymbol( hLib, "QueryFullProcessImageNameW" )
            if MyQueryFullProcessImageName then
                MyQueryFullProcessImageName( hProcess, 0, pwszExeName, @dwSize ) 
                CloseHandle hProcess
            end if
            DyLibFree(hLib)
        end if

    end if
    function = 0
end function


' ========================================================================================
' Checks if the program that we are going to compile is already running
' ========================================================================================
function findprocessid( byval processname as wstring ptr ) as dword
   dim pe32 as PROCESSENTRY32W
    pe32.dwSize = sizeof(PROCESSENTRY32W)

    dim hsnapshot as handle
    hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if hsnapshot = INVALID_HANDLE_VALUE then return 0

    if Process32FirstW(hsnapshot, @pe32) <> 0 then
        do
            if lcase(wstr(*processname)) = lcase(wstr(pe32.szExeFile)) then
                dim pid as dword = pe32.th32ProcessID
                CloseHandle(hsnapshot)
                return pid
            end if
        loop while Process32NextW(hsnapshot, @pe32) <> 0
    end if

    CloseHandle(hsnapshot)
    return 0
end function

function IsProcessRunning( byval wszExeFileName as DWSTRING ) as boolean
    wszExeFileName = AfxStrPathname("NAMEX", wszExeFileName)
    if ucase(wszExeFileName) = "TIKO.EXE" then return false
    dim as dword pid = findprocessid( wszExeFileName )
    dim as HANDLE hprocess = OpenProcess(SYNCHRONIZE, FALSE, pid)
    dim as boolean bExists = iif( hprocess = 0, false, true )
    CloseHandle( hprocess )
    return bExists
end function


' ========================================================================================
' Determine the EXE name of the currently active document or project.
' ========================================================================================
function GetRunExecutableFilename() as DWSTRING
    ' Used by the top menu to determine if the Run Executable option is available
    dim wszFilename as DWSTRING
    dim wszOptions as DWSTRING
    dim pDocMain as clsDocument ptr 

    dim as long idxBuild = frmBuildConfig_getActiveBuildIndex()
    dim as boolean bInstring = false
    
    pDocMain = iif( gApp.IsProjectActive, gApp.GetMainDocumentPtr, gTTabCtl.GetActiveDocumentPtr)
    if pDocMain = 0 then return ""
    
    if idxBuild > -1 then 
        wszOptions = gConfig.Builds(idxBuild).wszOptions 

        if gApp.IsProjectActive then
            if gConfig.Builds(idxBuild).Is32bit then
                wszOptions = wszOptions + " " + gApp.ProjectOther32
            end if   
            if gConfig.Builds(idxBuild).Is64bit then
                wszOptions = wszOptions + " " + gApp.ProjectOther64
            end if   
        else
            wszOptions = wszOptions + " " + gConfig.CompilerSwitches
        end if
        wszOptions = " " + ucase(wszOptions) + " "
        
        if instr(wszOptions, wstr(" -DLL "))   then return ""
        if instr(wszOptions, wstr(" -DYLIB ")) then return ""
        if instr(wszOptions, wstr(" -LIB "))   then return ""
        
    end if
    
    ' Need to check the compiler options to see if the -x switch is used. That switch specifically
    ' names the output file.
    dim y as long = instr(wszOptions, wstr(" -X "))
    if y then
        y = y + 4  ' skip over the switch itself
        for i as long = y to len(wszOptions)
            ' iterate to the beginning of the next switch or end of the string
            ' skip over spaces that are part of a filename string
            if wszOptions[i-1] = 34 then bInstring = not bInstring  ' double quotes
            if bInstring then continue for
            if wszOptions[i-1] = 45 then  ' dash   
                wszFilename = mid(wszOptions, y, i-y-1)
                exit for
            end if   
        next
        ' There was no other switch so we made it to the end of the string
        if len(wszFilename) = 0 then wszFilename = mid(wszOptions, y)
    end if
    
    ' Has a filename been determined yet? if it has then it possibly does not have
    ' a path assigned to it so we should add one.
    if len(wszFilename) then
        if len(AfxStrPathname("PATH", wszFilename)) = 0 then
            wszFilename = AfxStrPathname("PATH", pDocMain->DiskFilename) + wszFilename
        end if
    else
        ' Default 
        wszFilename = AfxStrPathname("PATH", pDocMain->DiskFilename) + _
                    AfxStrPathname("NAME", pDocMain->DiskFilename) + _
                    wstr(".exe")
    end if

    return wszFilename

end function


' ========================================================================================
' Convert Tiko version to whole number for comparison to file versions being loaded.
' ========================================================================================
function ConvertTikoVersion( byref wszVersion as wstring ) as long
   dim as DWSTRING wszPart1, wszPart2, wszPart3
   
   wszPart1 = AfxStrLSet( AfxStrParse(wszVersion, 1, "."), 3, "0" )
   wszPart2 = AfxStrLSet( AfxStrParse(wszVersion, 2, "."), 3, "0" )
   wszPart3 = AfxStrRSet( AfxStrParse(wszVersion, 3, "."), 3, "0" )
   
   function = val(wszPart1 + wszPart2 + wszPart3) 
end function


' ========================================================================================
' Check PlanetSquires server for latest Tiko version.
' ========================================================================================
sub DoCheckForUpdates( byval param as any ptr ) 
   
    ''  Contact the PlanetSquires server and download the text file containing
    ''  the latest Tiko version number. 

    dim as DWSTRING wszServerFile = "tiko_version.txt"
    dim as DWSTRING wszFilename
    dim as DWSTRING wszLatestVersion
    dim as DWSTRING wszMsg
    dim as string st
    dim as boolean bFailedCheck = false

    if AfxWinHttpCheckPlatform() = false then
        ' This platform does not support the Windows HTTP Services (WinHTTP)
        ' print "Windows HTTP Services (WinHTTP) not supported on this platform"
        exit sub
    end if
   
    dim pWHttp as CWinHttpRequest
   
    wszFilename = AfxGetExePathName & wszServerFile

    ' Open an HTTP connection to an HTTP resource (synchronous mode)
    if pWHttp.Open( "GET", "https://www.planetsquires.com/" & wszServerFile ) <> S_OK then
        exit sub
    end if

    ' Send HTTP request and wait 5 seconds for response 
    pWHttp.Send

    if pWHttp.WaitForResponse(5) = 0 then
        exit sub
    end if

    ' Open a file stream and save the downloaded file
    st = pWHttp.GetResponseBody
    dim pFileStream as CFileStream
    if pFileStream.Open(wszFilename, STGM_CREATE or STGM_WRITE) = S_OK then
        pFileStream.WriteTextA(st)
        pFileStream.Close
    end if

    ' Open the downloaded file and check for the version number. It is possible that the socket
    ' will open but fail and send request but security parameters of the system will prevent data
    ' from being received resulting in a zero byte file. 
    dim pStream as CTextStream 
    if pStream.Open(wszFilename, IOMODE_FORREADING) = S_OK then 
        do until pStream.EOS
            st = pStream.ReadLine
            if left( st, 15 ) = "latest_version=" then
                wszLatestVersion = trim(mid( st, 16 ))
                exit do
           end if
        loop
        pStream.Close
    end if
    
    AfxDeleteFile( wszFilename )
      
    if len(wszLatestVersion) = 0 then exit sub
    
    ' Check the installed vs. available version numbers   
    dim as long latestVersion = ConvertTikoVersion(wszLatestVersion)
    dim as long installedVersion = ConvertTikoVersion(APPVERSION)

'print "wszLatestVersion = "; wszLatestVersion
'print "latestVersion: "; latestVersion, "installedVersion: "; installedVersion

    AfxDeleteFile( wszFilename ) 

    if installedVersion < latestVersion then
        gApp.IsUpdateAvailable = true
    end if
   
end sub
