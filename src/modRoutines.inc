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

    ' \ not /: / is floating point in FB and assigning it to a long rounds.
    dim as long topMargin = ((rc.bottom - rc.top) - tm.tmHeight) \ 2
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
    dim as long nFields = PsStrParseCount( wszTabbed, chr(9) )
    if nFields < 1 then nFields = 1

    ' PsStrParse is 1-based, so field i+1 is column i -- the same mapping
    ' frmListView_GetItemText used.
    dim as long nRow = PsListTree_AddString( hCtl, PsStrParse( wszTabbed, 1, chr(9) ) )
    if nRow < 0 then return -1

    for i as long = 1 to nFields - 1
        PsListTree_SetCellText( hCtl, nRow, i, PsStrParse( wszTabbed, i + 1, chr(9) ) )
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
'     switch the else branch to theme.output.text.forecolor if the original intent is wanted.
'   - Column 2 (the filename) gets DT_PATH_ELLIPSIS, as before.
' ========================================================================================
sub ListBox_LVPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    ' The old control highlighted the SELECTED row only; PsListTree tracks hover separately,
    ' so honor both.
    dim as boolean bHot = (p->isSelected orelse p->isHot)
    dim as COLORREF backclr = iif( bHot, theme.output.scrollbar.forecolor, theme.output.text.backcolor )
    p->b->SetForeColor( theme.output.texthot.forecolor )
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

    dim as COLORREF backclr = theme.output.text.backcolor
    if p->isHot orelse p->isResizeHot then backclr = theme.output.scrollbar.forecolor
    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += pWindow->ScaleX(4)
    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' A one-pixel divider on the trailing edge, so columns read as separated.
    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.output.scrollbardivider )
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
            if PsLen(wszArg) = 0 then exit do
            
            ' Remove any double quotes from the argument.
            wszPath = PsStrRemove( wszArg, wchr(34) )
            
            ' if no path exists for the file then add the current folder
            wszPath = PsPathDirWithSep(wszArg )
            if PsLen(wszPath) = 0 then wszArg = PsExePath & wszArg 
            
            wszSendString = wszSendString & _
                            iif(PsLen(wszSendString), ";", "") & _
                            wszArg
            i += 1
        loop
        
        dim as COPYDATASTRUCT cds
        cds.dwData = IDM_COPYDATA_COMMANDLINE
        cds.cbData = (PsLen(wszSendString) + 1) * 2  ' unicode
        cds.lpData = wszSendString.vptr
        SendMessage(hWindow, WM_COPYDATA, 0, cast(LPARAM, @cds))
        return true
    end if

    function = false
end function
    

' ========================================================================================
' Safely read the wide string out of a WM_COPYDATA payload.
'
' WM_COPYDATA IS AN OPEN DOOR. Any process on the same desktop can send it to tiko's main
' window; nothing about it is privileged. The handler used to do:
'
'     dim as DWSTRING wszCommandLine = *cast(wstring ptr, pDataToGet->lpData)
'
' which dereferences another process's buffer as a NUL-terminated string without checking
' lpData for null, without consulting cbData at all, and without any guarantee a terminator
' exists inside the mapped copy. A payload that is simply not terminated reads off the end
' of the mapping -- an access violation, i.e. any unprivileged process can close the editor
' at will. The resulting string is then treated as a list of files to open.
'
' Every rule here is about staying inside cbData:
'   - a null pointer, or fewer than 2 bytes, cannot hold even one UTF-16 code unit
'   - an odd trailing byte cannot form a code unit, so integer division drops it
'   - the scan for the terminator NEVER passes cbData\2, and the result is terminated by
'     this function rather than trusted from the sender
'   - the length is capped: a command line is bounded by the OS at 32767 characters, so
'     anything longer is not one, whatever the sender claims
'
' Pure -- pointer and length in, string out -- so all of that is assertable without a second
' process. Returns "" for anything malformed.
'
' NOTE: this validates the CONTENT, not the SENDER. tiko still acts on a file list from any
' process that asks. Restricting that (checking the sender's image path) is a separate
' decision and is deliberately not made here.
' ========================================================================================
function SafeCopyDataString( byval pData as any ptr, byval cbData as ulong ) as DWSTRING
    const MAX_COPYDATA_CHARS = 32767      ' the OS command-line limit

    if pData = 0 then return ""
    if cbData < 2 then return ""

    dim as ulong nMax = cbData \ 2        ' an odd trailing byte cannot be a code unit
    if nMax > MAX_COPYDATA_CHARS then nMax = MAX_COPYDATA_CHARS

    ' Find the terminator WITHIN the declared length. A well-formed sender includes one
    ' (SpawnPreviousInstance sends (len+1)*2); a hostile or buggy one may not, and that is
    ' precisely the case the old code read past.
    dim as ushort ptr p = cast(ushort ptr, pData)
    dim as ulong n = 0
    while n < nMax
        if p[n] = 0 then exit while
        n += 1
    wend
    if n = 0 then return ""

    ' Copy into our own buffer and terminate it ourselves.
    dim as wstring ptr pBuf = allocate( (n + 1) * 2 )
    if pBuf = 0 then return ""
    memcpy( pBuf, pData, n * 2 )
    pBuf[n] = 0
    dim as DWSTRING wszOut = *pBuf
    deallocate( pBuf )

    return wszOut
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
    GetTextExtentpoint32( hDC, @wszText, PsLen(wszText), @size )
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
        PsFileDelete( wszTempFilename )
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

    if PsLen(wzCharsetName) = 0 then return SC_CHARSET_DEFAULT
    
    ' .Utf8 on the selector -- `select case <DWSTRING>` binds today only because
    ' AfxNova's DWSTRING extends WSTRING. Every `case` label below is ASCII, so
    ' the ANSI-vs-UTF-8 difference between the two conversions cannot change a
    ' match. See clsConfig.inc for the long form.
    select case wzCharsetName.Utf8
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
' Delete the spaces and tabs sitting between the last non-blank character of every line and
' that line's end. Returns the number of lines that were changed.
'
' It edits the SCINTILLA BUFFER rather than the byte string on its way to disk, and that is
' the decision here. Stripping only the outgoing bytes would leave the buffer holding
' whitespace the file no longer has -- the document would look saved while differing from
' what is on disk, which then reloads differently and shows up in any external diff. Editing
' the buffer keeps the two identical, and gives the user an undoable edit they can see.
'
' Positions are BYTE offsets, and both characters searched for are ASCII, so the scan is
' safe on a UTF-8 buffer: no continuation byte can be mistaken for a space or a tab.
' Line positions are re-read on every iteration because each delete shifts everything after
' it, so walking forwards is correct.
' ========================================================================================
function Scintilla_StripTrailingWhitespace( byval hEdit as hwnd ) as long
    if hEdit = 0 then return 0
    if SciExec( hEdit, SCI_GETREADONLY, 0, 0 ) then return 0

    dim as long nLines = SciExec( hEdit, SCI_GETLINECOUNT, 0, 0 )
    if nLines <= 0 then return 0

    ' The caret is restored by LINE and COLUMN rather than by position: a delete before it
    ' moves the position, and the caret can also be sitting inside the whitespace that is
    ' about to disappear. Same for the first visible line, which a strip never scrolls.
    dim as long nCaretPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nCaretLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nCaretPos, 0 )
    dim as long nCaretCol = SciExec( hEdit, SCI_GETCOLUMN, nCaretPos, 0 )
    dim as long nFirstVisible = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0 )

    dim as long nChanged = 0

    SciExec( hEdit, SCI_BEGINUNDOACTION, 0, 0 )

    for i as long = 0 to nLines - 1
        dim as long nStart = SciExec( hEdit, SCI_POSITIONFROMLINE, i, 0 )
        ' Excludes the line's EOL characters, so the EOL convention is never touched.
        dim as long nEnd = SciExec( hEdit, SCI_GETLINEENDPOSITION, i, 0 )
        dim as long nTrim = nEnd
        do while nTrim > nStart
            dim as long ch = SciExec( hEdit, SCI_GETCHARAT, nTrim - 1, 0 )
            if (ch <> 32) andalso (ch <> 9) then exit do
            nTrim -= 1
        loop
        if nTrim < nEnd then
            SciExec( hEdit, SCI_DELETERANGE, nTrim, nEnd - nTrim )
            nChanged += 1
        end if
    next

    SciExec( hEdit, SCI_ENDUNDOACTION, 0, 0 )

    if nChanged > 0 then
        dim as long nNewPos = SciExec( hEdit, SCI_FINDCOLUMN, nCaretLine, nCaretCol )
        ' The caret's column may no longer exist on the shortened line, and FINDCOLUMN then
        ' answers past the line end; clamp so it never lands on the next line.
        dim as long nLineEnd = SciExec( hEdit, SCI_GETLINEENDPOSITION, nCaretLine, 0 )
        if nNewPos > nLineEnd then nNewPos = nLineEnd
        SciExec( hEdit, SCI_GOTOPOS, nNewPos, 0 )
        SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, nFirstVisible, 0 )
    end if

    function = nChanged
end function


' ========================================================================================
' Open a disk file and read it into a string (ANSI or UTF8)
' ========================================================================================
function GetFileToString( _
            byref wszFilename as const wstring, _
            byref txtBuffer as string, _
            byval pDoc as clsDocument ptr _
            ) as boolean
    
    if pDoc = 0 then return true
    if PsFileExists(wszFilename) = false then return true
    
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
    wszCompilerPath = PsPathDirWithSep(wszCompilerPath ) & "inc"

    ' Convert relative path to absolute path if needed.
    if PsPathIsRelative(wszCompilerPath) then
        wszCompilerPath = PsPathJoin(PsExePath, wszCompilerPath)
    end if

    ' remove any comments at the end of the line
    i = instr(sFilename, "'")
    if i then sFilename = left(sFilename, i-1)
    sFilename = trim(sFilename, any chr(32,34))  ' remove spaces and double quotes
    wszPath = PsPathDirWithSep(pDoc->DiskFilename )

    if PsFileExists(wszPath & sFilename) then 
        wszCompleteFilename = wszPath & sFilename
    elseif PsFileExists(sFilename) then 
        wszCompleteFilename = sFilename
    elseif PsFileExists(PsCurDir & "\" & sFilename) then 
        wszCompleteFilename = PsCurDir & "\" & sFilename
    elseif PsFileExists(PsExePath & sFilename) then 
        wszCompleteFilename = PsExePath & sFilename
    ' NO str() ON THE DWSTRING. It converted wide -> ANSI purely to join two strings,
    ' so a compiler path with a non-ASCII character was mangled on the way through
    ' for no reason. Dropping it keeps the expression wide end to end, and it is
    ' also what the swap needs: PsCore's DWSTRING has no str().
    elseif PsFileExists(wszCompilerPath & "\" & sFilename) then 
        wszCompleteFilename = wszCompilerPath & "\" & sFilename
    else
        ' Iterate the optional semi colon separated list of compiler include paths
        dim as DWSTRING includePath
        dim as long numpaths = PsStrParseCount(gConfig.CompilerIncludes, ";")
        for i as long = 1 to numpaths
            includePath = PsRTrim(PsStrParse(gConfig.CompilerIncludes, i, ";")) & "\" & sFilename
            if PsFileExists(includePath) then
                wszCompleteFilename = includePath
                exit for
            end if
        next
    end if
    wszCompleteFilename = PsStrReplace(wszCompleteFilename, "/", "\")
    wszCompleteFilename = PsStrReplace(wszCompleteFilename, "\\", "\")
 
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
    
    function = PsFileExists( gApp.IncludeFilename ) 

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

    ' Every one of those six is a navigation, so this is the one choke point where the
    ' position being LEFT gets recorded for Go Back. Both calls are no-ops while
    ' NavHistory_Goto is driving us (see NavHistory_IsRecordingAllowed) -- otherwise a Back
    ' would record its own destination and never advance.
    NavHistory_RecordJump()

    ' if incoming FunctionName then search for its filename and line number.
    if PsLen( wszFunctionName ) andalso (nLineNumber = -1) then
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
        if PsFileExists(wszOpenName) = false then exit function
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

        ' Where we have arrived, so a later Go Back has somewhere to come from.
        NavHistory_NoteArrival()
    end if

    function = pDoc
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
        case IDM_PROJECTOPEN
            ' .sptr, NOT @. LL is a DWSTRING array now, so the address of an element is a
            ' DWSTRING ptr rather than the WSTRING ptr pszName wants -- fbc reports that as
            ' "Suspicious pointer assignment" and carries on. sptr() hands back the buffer
            ' itself, which is the same pointer @ used to yield when the entries were
            ' fixed-width wstrings, and it stays valid as long as LL is not reloaded.
            rgFileTypes(1).pszName = L(177,"Project files").sptr
            rgFileTypes(1).pszSpec = @wstr("*" & APPEXTENSION)
            rgFileTypes(2).pszName = L(76,"All files").sptr
            rgFileTypes(2).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 2, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(177,"Project files"))
        
        case IDM_INSERTFILE
            rgFileTypes(1).pszName = L(102,"Source files").sptr
            rgFileTypes(2).pszName = L(74,"Code files").sptr
            rgFileTypes(3).pszName = L(75,"Header files").sptr
            rgFileTypes(4).pszName = L(170,"Resource files").sptr
            rgFileTypes(5).pszName = L(76,"All files").sptr
            rgFileTypes(1).pszSpec = @wstr("*.bas;*.bi;*.inc;*.rc")
            rgFileTypes(2).pszSpec = @wstr("*.bas;*.inc")
            rgFileTypes(3).pszSpec = @wstr("*.bi")
            rgFileTypes(4).pszSpec = @wstr("*.rc")
            rgFileTypes(5).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 5, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(77,"Insert File"))

        case IDC_FRMOPTIONSLOCAL_CMDLOCALIZATION  '1012
            rgFileTypes(1).pszName = L(98,"Localization files").sptr
            rgFileTypes(1).pszSpec = @wstr("*.lang")
            rgFileTypes(2).pszName = L(76,"All files").sptr
            rgFileTypes(2).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 2, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(99,"Open Localization File"))
    
            dim as DWSTRING wszDefaultFolder = PsExePath & wstr("settings\languages\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = pofd->lpVtbl->SetFolder(pofd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

        case IDC_FRMUSERTOOLS_CMDBROWSEEXE
            rgFileTypes(1).pszName = L(76,"All files").sptr
            rgFileTypes(1).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 1, @rgFileTypes(1))
            ' Set the title of the dialog
            hr = pofd->lpVtbl->SetTitle(pofd, L(4,"Open File"))
    
            dim as DWSTRING wszDefaultFolder = PsExePath & wstr("settings\tools\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = pofd->lpVtbl->SetFolder(pofd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

        ' (The IDC_FRMOPTIONSCOMPILER_CMDFBHELPFILE case is gone with the control that
        '  raised it: a *.chm filter, a "Find Help File" title and a settings\help start
        '  folder, for a path nothing ever opened. The Help Center replaced the .chm.)

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
            rgFileTypes(1).pszName = L(102,"Source files").sptr
            rgFileTypes(2).pszName = L(74,"Code files").sptr
            rgFileTypes(3).pszName = L(75,"Header files").sptr
            rgFileTypes(4).pszName = L(170,"Resource files").sptr
            rgFileTypes(5).pszName = L(76,"All files").sptr
            rgFileTypes(1).pszSpec = @wstr("*.bas;*.bi;*.inc;*.rc")
            rgFileTypes(2).pszSpec = @wstr("*.bas;*.inc")
            rgFileTypes(3).pszSpec = @wstr("*.bi")
            rgFileTypes(4).pszSpec = @wstr("*.rc")
            rgFileTypes(5).pszSpec = @wstr("*.*")
            pofd->lpVtbl->SetFileTypes(pofd, 5, @rgFileTypes(1))
    end select

    ' Set the title of the dialog
    hr = pofd->lpVtbl->SetTitle(pofd, L(196,"Open file"))

    ' Set the default folder to display in the open dialog
    dim wszDefaultFolder as wstring * MAX_PATH
    if gApp.wszLastOpenFolder = "" then 
        if PsFileExists( gApp.ProjectFilename ) then 
            wszDefaultFolder = PsPathDirWithSep(gApp.ProjectFilename )
        else
            wszDefaultFolder = PsCurDir
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
    wszFilename = PsPathName(*pwszFileName )
    if PsLen(wszFilename) then 
        if PsPathExt(wszFilename ) = "" then
            if len(*pwszDefExt) then
            wszFilename = wszFilename & "." & *pwszDefExt
            end if   
        end if   
    end if
    
    if PsFileExists( *pwszFileName ) then
        wszFilePath = PsPathDirWithSep(*pwszFileName )
    else   
        ' Set the default folder to save the file
        ' PsLen, not the implicit cast. `if <DWSTRING> then` relies on AfxNova's
        ' cast-to-boolean, which PsCore's type does not have -- and it reads as a
        ' null test when it is an is-it-empty test.
        if PsLen(gApp.wszLastOpenFolder) then 
            wszFilePath = gApp.wszLastOpenFolder
        else
            ' New file being saved try to default to the project folder
            if PsFileExists( gApp.ProjectFilename ) then
                wszFilePath = PsPathDirWithSep(gApp.ProjectFilename )
            end if
        end if
        gApp.wszLastOpenFolder = wszFilePath
    end if

    ' Set the file types
    select case id
        case IDM_FILERENAME
            rgFileTypes(1).pszName = L(76,"All files").sptr
            rgFileTypes(1).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 1, @rgFileTypes(1))
            ' // Set the title of the dialog
            hr = psfd->lpVtbl->SetTitle(psfd, L(21,"Rename"))

        case IDC_FRMPROJECTOPTIONS_CMDSELECT, IDM_PROJECTSAVE, IDM_PROJECTSAVEAS
            rgFileTypes(1).pszName = L(177,"Project files").sptr
            rgFileTypes(1).pszSpec = @wstr("*" & APPEXTENSION)
            rgFileTypes(2).pszName = L(76,"All files").sptr
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            ' // Set the title of the dialog
            hr = psfd->lpVtbl->SetTitle(psfd, L(149,"Save Project As..."))

        case IDC_FRMOPTIONSLOCAL_CMDNEW
            rgFileTypes(1).pszName = L(98,"Localization files").sptr
            rgFileTypes(1).pszSpec = @wstr("*.lang")
            rgFileTypes(2).pszName = L(76,"All files").sptr
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            psfd->lpVtbl->SetTitle(psfd, L(8,"Save As..."))

            dim as DWSTRING wszDefaultFolder = PsExePath & wstr("settings\languages\") 
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = psfd->lpVtbl->SetFolder(psfd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if      
            end if

        case IDC_THEMES_SAVEAS
            rgFileTypes(1).pszName = L(318,"Themes").sptr
            rgFileTypes(1).pszSpec = @wstr("*.theme")
            rgFileTypes(2).pszName = L(76,"All files").sptr
            rgFileTypes(2).pszSpec = @wstr("*.*")
            psfd->lpVtbl->SetFileTypes(psfd, 2, @rgFileTypes(1))
            psfd->lpVtbl->SetTitle(psfd, L(8,"Save As..."))

            dim as DWSTRING wszDefaultFolder = PsExePath & wstr("settings\themes\")
            dim pFolder as IShellItem ptr
            SHCreateItemFromParsingName (wszDefaultFolder, null, @IID_IShellItem, @pFolder)
            if pFolder then
                hr = psfd->lpVtbl->SetFolder(psfd, pFolder)
                if SUCCEEDED(hr) then
                    pFolder->lpVtbl->Release(pFolder)
                end if
            end if

        case else
            rgFileTypes(1).pszName = L(102,"Source files").sptr
            rgFileTypes(2).pszName = L(74,"Code files").sptr
            rgFileTypes(3).pszName = L(75,"Header files").sptr
            rgFileTypes(4).pszName = L(170,"Resource files").sptr
            rgFileTypes(5).pszName = L(76,"All files").sptr
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
    if PsFileExists( wszFileName ) = 0 then exit function

    dim as BSTRING wst, wKey, wData
    dim nKey  as long
    dim nData as long  
    dim i     as long
    
    redim as DWSTRING lines_(any)
    if TextFile_ReadLines( wszFileName, lines_() ) = false then exit function
    
    for nLine_ as long = lbound(lines_) to ubound(lines_)
        wst = lines_(nLine_)
        
        if PsLen(wst) = 0 then continue for
        if PsLeft(wst, 1) = "'" then continue for
        
        i = PsInStr(wst, ":")
        if i = 0 then continue for
        
        wKey = "": wData = "": nData = 0

        wKey  = PsLeft(wst, i-1)
        wData = mid(**wst, i+1)    ' MID causes problems with Chinese data so ** is used.
        
        nKey  = val(wKey)
        nData = val(wData)

        if PsUCase(wKey) = "MAXIMUM" then
            ' resize the global dynamic array
            if IsEnglish then
                ' NO `as <type>` CLAUSE, AND THAT IS LOAD-BEARING. These said
                ' `as wstring * MAX_PATH`, restating the element type -- and once
                ' the shared arrays became DWSTRING, restating the OLD type made
                ' fbc declare a LOCAL array of that name instead of resizing the
                ' shared one. The loader then filled the local, the global stayed
                ' at ubound -1, and every localized string in the program rendered
                ' blank. Clean build, no warning, no error.
                '
                ' A bare redim resizes the array that is in scope, which is the
                ' one this is meant to fill.
                redim gLangEnglish(nData)
            else   
                redim LL(nData)
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
                    gLangEnglish(nKey) = rtrim(**PsStrParse(wData, 1, ";"), any chr(9,32))
                end if
            else
                if (nKey >= lbound(LL)) andalso (nKey <= ubound(LL)) then
                    ' Remove any comments from end of the line. Comments begin with
                    ' a semicolon character.
                    ' Use ** to ensure that cyrillic langauge gets converted correctly. FB intrinsic
                    ' functions (RTRIM) automatically convert those incorrectly when using CBSTR or DWSTRING.
                    LL(nKey) = rtrim(**PsStrParse(wData, 1, ";"), any chr(9,32))
                    ' if the local phrase is empty then fill it using the English version.
                    if len(LL(nKey)) = 0 then 
                        if (nKey >= lbound(gLangEnglish)) andalso (nKey <= ubound(gLangEnglish)) then
                            LL(nKey) = gLangEnglish(nKey)
                        end if
                    end if   
                end if
            end if
        end if   
            
    next
    
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
    wszExeFileName = PsPathName(wszExeFileName)
    if PsUCase(wszExeFileName) = "TIKO.EXE" then return false
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
    
    ' Main if there is one, otherwise the active document. Collapsing this to the project
    ' arm would silently return null for any workspace with no designated Main, which is
    ' every workspace of loose files. Same rule as code_SetMainFile.
    pDocMain = gApp.GetMainDocumentPtr()
    if pDocMain = 0 then pDocMain = gTTabCtl.GetActiveDocumentPtr()
    if pDocMain = 0 then return ""
    
    if idxBuild > -1 then 
        wszOptions = gConfig.Builds(idxBuild).wszOptions 

        ' The workspace's own switches. gConfig.CompilerSwitches is only the SEED for a
        ' new workspace now (applied in Workspace_EstablishUntitled), not a second store
        ' consulted at build time.
        if gConfig.Builds(idxBuild).Is32bit then
            wszOptions = wszOptions + " " + gApp.ProjectOther32
        end if
        if gConfig.Builds(idxBuild).Is64bit then
            wszOptions = wszOptions + " " + gApp.ProjectOther64
        end if
        wszOptions = " " + PsUCase(wszOptions) + " "
        
        if PsInStr(wszOptions, wstr(" -DLL "))   then return ""
        if PsInStr(wszOptions, wstr(" -DYLIB ")) then return ""
        if PsInStr(wszOptions, wstr(" -LIB "))   then return ""
        
    end if
    
    ' Need to check the compiler options to see if the -x switch is used. That switch specifically
    ' names the output file.
    dim y as long = PsInStr(wszOptions, wstr(" -X "))
    if y then
        y = y + 4  ' skip over the switch itself
        for i as long = y to PsLen(wszOptions)
            ' iterate to the beginning of the next switch or end of the string
            ' skip over spaces that are part of a filename string
            if wszOptions[i-1] = 34 then bInstring = not bInstring  ' double quotes
            if bInstring then continue for
            if wszOptions[i-1] = 45 then  ' dash   
                wszFilename = PsMid(wszOptions, y, i-y-1)
                exit for
            end if   
        next
        ' There was no other switch so we made it to the end of the string
        if PsLen(wszFilename) = 0 then wszFilename = PsMid(wszOptions, y)
    end if
    
    ' Has a filename been determined yet? if it has then it possibly does not have
    ' a path assigned to it so we should add one.
    if PsLen(wszFilename) then
        if PsLen(PsPathDirWithSep(wszFilename)) = 0 then
            wszFilename = PsPathDirWithSep(pDocMain->DiskFilename) + wszFilename
        end if
    else
        ' Default 
        wszFilename = PsPathDirWithSep(pDocMain->DiskFilename) + _
                    PsPathStem(pDocMain->DiskFilename) + _
                    wstr(".exe")
    end if

    return wszFilename

end function


' ########################################################################################
' Env-gated: TIKO_COPYDATA_SELFTEST=1
'
' Asserts SafeCopyDataString stays inside cbData. The case that matters is the UNTERMINATED
' buffer: that is what the old handler read past, and it is unreachable by inspection --
' the only way to know a reader stops is to hand it something with no terminator and a hard
' boundary after it.
'
' The buffer is deliberately allocated with a GUARD PATTERN after the declared length. If
' the reader overruns, it consumes the guard and the assertion on the returned length fails,
' rather than the test merely "not crashing" -- which proves nothing, since reading a few
' bytes past a heap allocation usually succeeds.
'
' (Lives here because SafeCopyDataString does. When modRoutines is split -- it is a
'  junk drawer and known to be -- both should move together.)
' ########################################################################################
sub CopyData_RunSelfTest()

    if environ("TIKO_COPYDATA_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro CDASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
            dim as DWSTRING wszMsg = msg
            sMsg = "  FAIL: " & wszMsg.Utf8
            ? sMsg
        end if
    #endmacro

    ? "--- TIKO_COPYDATA_SELFTEST ---"

    ' ---- malformed payloads are refused, not dereferenced -------------------------------
    CDASSERT( len( SafeCopyDataString( 0, 100 ) ) = 0, "a NULL lpData was dereferenced" )
    CDASSERT( len( SafeCopyDataString( 0, 0 ) ) = 0,   "NULL/0 was dereferenced" )

    scope
        dim as wstring * 16 w = "hello"
        CDASSERT( len( SafeCopyDataString( @w, 0 ) ) = 0, "cbData=0 still returned text" )
        CDASSERT( len( SafeCopyDataString( @w, 1 ) ) = 0, _
                  "cbData=1 (half a code unit) still returned text" )

        ' ---- the normal case: sender includes the terminator, as SpawnPreviousInstance does
        CDASSERT( SafeCopyDataString( @w, (PsLen(w) + 1) * 2 ) = "hello", _
                  "a well-formed payload did not round-trip" )

        ' ---- cbData SHORTER than the string: the declared length wins, not the terminator
        CDASSERT( SafeCopyDataString( @w, 3 * 2 ) = "hel", _
                  "read past a cbData that was shorter than the buffer" )

        ' ---- an odd trailing byte cannot form a code unit and must be dropped ------------
        CDASSERT( SafeCopyDataString( @w, 3 * 2 + 1 ) = "hel", _
                  "an odd cbData was rounded UP, reading half a code unit past the end" )
    end scope

    ' ---- THE ONE THAT MATTERS: no terminator anywhere inside cbData ---------------------
    scope
        const N = 8
        ' N code units of text, then a guard the reader must never reach.
        dim as ushort ptr p = allocate( (N + 4) * 2 )
        if p = 0 then
            ? "  NOTE: allocation failed; the overrun assertions did NOT run."
        else
            for i as long = 0 to N - 1
                p[i] = asc("A") + i
            next
            for i as long = N to N + 3
                p[i] = asc("Z")            ' guard: NOT a terminator, so an overrun eats it
            next

            dim as DWSTRING wszGot = SafeCopyDataString( p, N * 2 )
            CDASSERT( PsLen( wszGot ) = N, _
                      "unterminated payload returned " & PsLen(wszGot) & " chars, expected " & N & _
                      " -- the reader ran past cbData" )
            CDASSERT( wszGot = "ABCDEFGH", "unterminated payload returned [" & wszGot & "]" )
            CDASSERT( PsInStr( wszGot, "Z" ) = 0, _
                      "the guard pattern was consumed: the reader overran the declared length" )
            deallocate( p )
        end if
    end scope

    ' ---- an empty (terminator-first) payload is not text --------------------------------
    scope
        dim as ushort z(0 to 1) = { 0, 0 }
        CDASSERT( len( SafeCopyDataString( @z(0), 4 ) ) = 0, _
                  "a payload starting with a terminator returned text" )
    end scope

    ' ---- the update server's reply is a REMOTE input, so parse it defensively ----------
    ' It used to be written to a predictable file in the program directory and read back,
    ' and whatever came back was fed straight to ConvertTikoVersion -- including a captive
    ' portal's HTML login page, which is what a laptop on hotel wifi actually receives.
    CDASSERT( ParseLatestVersion( "latest_version=1.2.3" & chr(10) ) = "1.2.3", _
              "the normal reply did not parse" )
    CDASSERT( ParseLatestVersion( "junk" & chr(13,10) & "latest_version=2.0.1" & chr(13,10) ) = "2.0.1", _
              "CRLF and a leading junk line broke the parse" )
    CDASSERT( ParseLatestVersion( "latest_version=  3.4.5  " ) = "3.4.5", _
              "surrounding blanks were not trimmed" )
    CDASSERT( ParseLatestVersion( "LATEST_VERSION=9.9.9" ) = "9.9.9", _
              "the key is case sensitive" )
    CDASSERT( ParseLatestVersion( "latest_version=1.2.3" ) = "1.2.3", _
              "a reply with no trailing newline was dropped" )

    CDASSERT( ParseLatestVersion( "" ) = "", "an empty reply produced a version" )
    CDASSERT( ParseLatestVersion( "<html><body>Sign in</body></html>" ) = "", _
              "an HTML page produced a version -- this is the captive-portal case" )
    CDASSERT( ParseLatestVersion( "other_key=1.2.3" ) = "", "the wrong key produced a version" )
    CDASSERT( ParseLatestVersion( "latest_version=" ) = "", "an empty value produced a version" )
    CDASSERT( ParseLatestVersion( "latest_version=<script>" ) = "", _
              "a non-numeric value reached ConvertTikoVersion" )
    CDASSERT( ParseLatestVersion( "latest_version=99999999999999999999" ) = "99999999999999999999", _
              "a long numeric value was mangled rather than passed on for comparison" )

    ' ---- FB integer vs floating division, which has cost this tree a real bug ----------
    ' "/" is FLOATING POINT in FreeBASIC and assigning the result to an integer ROUNDS --
    ' to even, at that -- where every one of these sites wants truncation. NumTabsFromSpaces
    ' shipped that way and produced two tabs where one was meant. These assertions do not
    ' guard any particular call site; they pin the LANGUAGE RULE, so that a future reader
    ' who "tidies" a \ back to a / has something that fails.
    scope
        dim as long a1 = 3 / 2      ' 1.5 -> banker's rounding gives 2
        dim as long a2 = 3 \ 2      ' truncates to 1
        CDASSERT( a2 = 1, "3 \ 2 should truncate to 1, got " & a2 )
        CDASSERT( a1 <> a2, "/ and \ agreed on 3/2 -- the rounding hazard this guards is gone, " & _
                            "or the compiler changed (a1=" & a1 & ")" )
        dim as long b1 = 5 / 2, b2 = 5 \ 2
        CDASSERT( b2 = 2, "5 \ 2 should be 2, got " & b2 )
        CDASSERT( b1 = 2, "5 / 2 rounded to " & b1 & ", expected 2 (round-half-to-even)" )
        dim as long c1 = -3 / 2, c2 = -3 \ 2
        CDASSERT( c2 = -1, "-3 \ 2 should truncate toward zero to -1, got " & c2 )
        CDASSERT( c1 = -2, "-3 / 2 rounded to " & c1 & ", expected -2" )
    end scope

    sMsg = "TIKO_COPYDATA_SELFTEST: " & nPass & " passed, " & nFail & " failed."
    ? sMsg
end sub
