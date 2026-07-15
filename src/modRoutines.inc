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
#include once "frmListView.bi"


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
' Get the pData pointer stored in UserData(0) of the pWindow
' ========================================================================================
function frmListView_GetPointer( byval hLV as HWND ) as LISTVIEW_DATA_TYPE ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hLV)
    if pWindow = 0 then return 0
    
    ' The ListView's data is stored in the pWindow CWindow UserData(0).
    if pWindow->UserData(0) = 0 then return 0
    
    return cast(LISTVIEW_DATA_TYPE ptr, pWindow->UserData(0))
end function


' ========================================================================================
' frmListView_GetSelection
' ========================================================================================
function frmListView_GetSelection( byval hLV as HWND ) as long
    dim as HWND hListBox = GetDlgItem( hLV, IDC_FRMLISTVIEW_LISTBOX )
    return ListBox_GetCurSel( hListBox )
end function


' ========================================================================================
' frmListView_GetItemText
' ========================================================================================
function frmListView_GetItemText( byval hLV as HWND, byval nLineNum as long, byval nColNum as long ) as DWSTRING
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    dim as DWSTRING wszText
    if (nLineNum >= lbound(pData->rowData)) andalso _
        (nLineNum <= ubound(pData->rowData)) then
         wszText = pData->rowData(nLineNum)
    end if
    return AfxStrParse( wszText, nColNum + 1, chr(9) )
end function


' ========================================================================================
' frmListView_SetItemText
' ========================================================================================
function frmListView_SetItemText( byval hLV as HWND, _
        byval nLineNum as long, byval nColNum as long, byval wszText as DWSTRING ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    dim as DWSTRING wszExistingText = pData->rowData( nLineNum )
    dim as DWSTRING wszNewText

    dim as long nCount = AfxStrParseCount( wszExistingText, chr(9) )
    for i as long = 0 to nCount - 1
        if i = nColNum then
            wszNewText = wszNewText & wszText & chr(9)
        else    
            wszNewText = wszNewText & AfxStrParse( wszExistingText, i + 1, chr(9) ) & chr(9)
        end if
    next
    wszNewText = rtrim(wszNewText, chr(9))

    pData->rowData(nLineNum) = wszNewText

    return 0
end function


' ========================================================================================
' frmListView_DeleteAllItems
' ========================================================================================
function frmListView_DeleteAllItems( byval hLV as HWND ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    erase pData->rowData
    frmListView_SetCount( hLV, 0 )
    ' Hide the ownerdraw listboxes because the wm_drawitem will not fire if
    ' no items exist thereby causing white space to show.
    ShowWindow( pData->hListBox, SW_HIDE )
    return 0
end function


' ========================================================================================
' frmListView_GetCount
' ========================================================================================
function frmListView_GetCount( byval hLV as HWND ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    return ubound(pData->rowData) + 1
end function


' ========================================================================================
' frmListView_SetCount
' ========================================================================================
function frmListView_SetCount( byval hLV as HWND, byval itemCount as long ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    SendMessage(pData->hListBox, LB_SETCOUNT, itemCount, 0)
    return 0
end function


' ========================================================================================
' frmListView_SelectItem
' ========================================================================================
function frmListView_SelectItem( byval hLV as HWND, byval nLineNum as long ) as long
    dim as HWND hListBox = GetDlgItem( hLV, IDC_FRMLISTVIEW_LISTBOX )
    return ListBox_SetCurSel( hListBox, nLineNum )
end function


' ========================================================================================
' frmListView_AddString
' ========================================================================================
function frmListView_AddString( byval hLV as HWND, byval wszText as DWSTRING ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    dim as long ub = ubound(pData->rowData) + 1
    redim preserve pData->rowData(ub)
    pData->rowData(ub) = wszText

    frmListView_SetCount( hLV, ub + 1 )
    return 0
end function


' ========================================================================================
' frmListView_AddColumn
' ========================================================================================
function frmListView_AddColumn( _
            byval hLV as HWND, _
            byval nColNum as long, _
            byval wszText as DWSTRING, _
            byval nWidth as long _
            ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    dim as long ub = ubound(pData->ColData)
    if nColNum > ub then
        redim preserve pData->ColData(nColNum)
    end if
    pData->ColData(nColNum).wszText = wszText
    pData->ColData(nColNum).nWidth = nWidth
    
    return 0
end function



' ========================================================================================
' frmListView_SetColumnWidth
' ========================================================================================
function frmListView_SetColumnWidth( _
            byval hLV as HWND, _
            byval nColNum as long, _
            byval nWidth as long _
            ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    if (nColNum >= lbound(pData->ColData)) andalso _
        (nColNum <= ubound(pData->ColData)) then
        pData->ColData(nColNum).nWidth = nWidth
    end if
        
    return 0
end function


' ========================================================================================
' frmListView_SetForeColors
' ========================================================================================
function frmListView_SetForeColors( _
            byval hLV as HWND, _
            byval ForeColor as COLORREF, _
            byval ForeColorHot as COLORREF _
            ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    pData->ForeColor = ForeColor
    pData->ForeColorHot = ForeColorHot
    
    return 0
end function


' ========================================================================================
' frmListView_SetBackColors
' ========================================================================================
function frmListView_SetBackColors( _
            byval hLV as HWND, _
            byval BackColor as COLORREF, _
            byval BackColorHot as COLORREF _
            ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    pData->BackColor = BackColor
    pData->BackColorHot = BackColorHot
    
    return 0
end function


' ========================================================================================
' frmListView_SetScrollBarColors
' ========================================================================================
function frmListView_SetScrollBarColors( _
            byval hLV as HWND, _
            byval ScrollBarBackColor as COLORREF, _
            byval ScrollBarForeColor as COLORREF, _
            byval ScrollBarDividerColor as COLORREF _
            ) as long
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then return 1
    
    pData->BackColorScrollBar = ScrollBarBackColor
    pData->ForeColorScrollBar = ScrollBarForeColor
    pData->DividerColorScrollBar = ScrollBarDividerColor
    
    return 0
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
' Determine if the mouse cursor is currently over the incoming window
' ========================================================================================
function isMouseOverWindow( byval hChild as HWND ) as boolean
    dim as point pt
    GetCursorPos( @pt )
    dim as HWND hWindow = WindowFromPoint(pt)
    if hWindow = hChild then return true
end function

' ========================================================================================
' Determine if the mouse cursor is currently over client RECT area
' ========================================================================================
function isMouseOverRECT( byval hWin as HWND, byval rc as RECT ) as boolean
    dim as point pt
    GetCursorPos( @pt )
    MapWindowpoints( hWin, HWND_DESKTOP, cast( point ptr, @rc), 2 )
    if PtInRect( @rc, pt ) then return true
end function

' ========================================================================================
' Disable all modeless windows belonging to frmMain so that the popup modal is truly modal.
' ========================================================================================
function DisableAllModeless() as long
    ' No need to enable/disable the modeless Help form.
    EnableWindow(HWND_FRMMAIN, false)
    function = 0
end function


' ========================================================================================
' Enable all modeless windows belonging to frmMain.
' ========================================================================================
function EnableAllModeless() as long
    ' No need to enable/disable the modeless Help form.
    EnableWindow(HWND_FRMMAIN, true)
    function = 0
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
' Convert the current text buffer to the specified encoding and redisplay the text.
' ========================================================================================
function ConvertTextBuffer( _
            byval pDoc as clsDocument ptr, _
            byval FileEncoding as long _
            ) as long
                            
    if pDoc = 0 then exit function

    dim as hwnd hEdit = pDoc->hWndActiveScintilla
   
    ' Save the current file position and first visible line
    dim nFirstLine as long = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0) 
    dim nPos as long = SciExec(hEdit, SCI_GETCURRENTPOS, 0, 0)

    pDoc->FileEncoding = FileEncoding
    dim as zstring ptr psz = cast( zstring ptr, SciExec(hEdit, SCI_GETCHARACTERPOINTER, 0, 0) )
    dim as long sciCodePage = SciExec(hEdit, SCI_GETCODEPAGE, 0, 0)   ' 0 or SC_CP_UTF8 
    dim as string txtBuffer 
   
    ' Convert buffer to specified file encoding
    select case FileEncoding
        case FILE_ENCODING_ANSI
            if sciCodePage = 0 then  ' already in ANSI format  
                exit function
            else   
                ' need to convert from UTF8 to ANSI
                txtBuffer = Utf8ToAnsi(*psz)
                SciExec(hEdit, SCI_SETCODEPAGE, 0, 0 )
            end if    

        case FILE_ENCODING_UTF8_BOM, FILE_ENCODING_UTF16_BOM
            if sciCodePage = SC_CP_UTF8 then  ' already in unicode format
                exit function
            else
                ' need to convert from ANSI to UTF8
                txtBuffer = AnsiToUtf8(*psz)
                SciExec(hEdit, SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
            end if    

    end select
   
    ' Set the new buffer
    pDoc->SetText(txtBuffer)
    
    SciExec(hEdit, SCI_SETFIRSTVISIBLELINE, nFirstLine, 0) 
    SciExec(hEdit, SCI_GOTOPOS, nPos, 0)
   
    function = 0
end function   


' ========================================================================================
' Maps UTF-8 string to Ansi string.
' ========================================================================================
function Utf8ToAnsi( byref sUtf8 as string ) as string
    dim dws as DWSTRING = DWSTRING(sUtf8, CP_UTF8)
    dim ansiStr as string = dws
    return ansiStr

'    dim as long wlen, alen
'    dim as string sUnicode, sAnsi

'    if len(sUtf8) = 0 then return ""

'    '-----------------------------------------
'    ' Step 1: UTF-8 ? UTF-16 (size query)
'    '-----------------------------------------
'    wlen = MultiByteToWideChar( _
'                CP_UTF8, _
'                0, _
'                strptr(sUtf8), _
'                len(sUtf8), _
'                NULL, _
'                0)

'    if wlen = 0 then return ""

'    sUnicode = string(wlen * 2, 0)

'    MultiByteToWideChar( _
'        CP_UTF8, _
'        0, _
'        strptr(sUtf8), _
'        len(sUtf8), _
'        cast(LPWSTR, strptr(sUnicode)), _
'        wlen)

'    '-----------------------------------------
'    ' Step 2: UTF-16 ? ANSI (size query)
'    '-----------------------------------------
'    alen = WideCharToMultiByte( _
'                CP_ACP, _
'                0, _
'                cast(LPDWSTRING, strptr(sUnicode)), _
'                wlen, _
'                NULL, _
'                0, _
'                NULL, _
'                NULL)

'    if alen = 0 then return ""

'    sAnsi = string(alen, 0)

'    WideCharToMultiByte( _
'        CP_ACP, _
'        0, _
'        cast(LPDWSTRING, strptr(sUnicode)), _
'        wlen, _
'        strptr(sAnsi), _
'        alen, _
'        NULL, _
'        NULL)

'    return sAnsi
end function


' ========================================================================================
' Maps Unicode character string to a UTF-8 string.
' ========================================================================================
function UnicodeToUtf8( byref wzUnicode as DWSTRING ) as string
    dim strUtf8 as string = wzUnicode.utf8
    return strUtf8 
    
'    dim as long u8len
'    dim as string sUtf8

'    ' Step 1: get required UTF-8 size (including null terminator)
'    u8len = WideCharToMultiByte( _
'                CP_UTF8, _
'                0, _
'                cast(LPDWSTRING, *wzUnicode), _
'                -1, _
'                NULL, _
'                0, _
'                NULL, _
'                NULL)

'    if u8len = 0 then return ""

'    ' Step 2: allocate buffer
'    sUtf8 = string(u8len, 0)

'    ' Step 3: convert
'    WideCharToMultiByte( _
'        CP_UTF8, _
'        0, _
'        cast(LPDWSTRING, *wzUnicode), _
'        -1, _
'        strptr(sUtf8), _
'        u8len, _
'        NULL, _
'        NULL)

'    ' Remove null terminator
'    return left(sUtf8, u8len - 1)
end function


' ========================================================================================
' Maps Ansi character string to a UTF-8 string.
' ========================================================================================
function AnsiToUtf8( byref sAnsi as string ) as string
    dim dws as DWSTRING = sAnsi
    dim strUtf8 as string = dws.utf8
    return strUtf8

'    dim as integer wlen, u8len
'    dim as string sUnicode, sUtf8

'    if len(sAnsi) = 0 then return ""

'    ' Step 1: ANSI ? UTF-16 (size query)
'    wlen = MultiByteToWideChar(CP_ACP, 0, strptr(sAnsi), len(sAnsi), NULL, 0)
'    if wlen = 0 then return ""

'    sUnicode = string(wlen * 2, 0)

'    if MultiByteToWideChar(CP_ACP, 0, strptr(sAnsi), len(sAnsi), cast(LPWSTR, strptr(sUnicode)), wlen) = 0 then
'        return ""
'    end if

'    ' Step 2: UTF-16 ? UTF-8 (size query)
'    u8len = WideCharToMultiByte(CP_UTF8, 0, cast(LPDWSTRING, strptr(sUnicode)), wlen, NULL, 0, NULL, NULL)
'    if u8len = 0 then return ""

'    sUtf8 = string(u8len, 0)

'    if WideCharToMultiByte(CP_UTF8, 0, cast(LPDWSTRING, strptr(sUnicode)), wlen, strptr(sUtf8), u8len, NULL, NULL) = 0 then
'        return ""
'    end if

'    return sUtf8
end function


' ========================================================================================
' Determine if a string is likely UTF-8 encoded
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

    ' Check for BOM signatures
    if left(txtBuffer, 3) = chr(&HEF, &HBB, &HBF) then
        ' UTF8 BOM encoded 
        pDoc->FileEncoding = FILE_ENCODING_UTF8_BOM
        txtBuffer = mid(txtBuffer, 4)   ' bypass the BOM
    elseif left(txtBuffer, 2) = chr(&HFF, &HFE) then
        ' UTF16 BOM (little endian) encoded
        pDoc->FileEncoding = FILE_ENCODING_UTF16_BOM 
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
         ' Convert the whole buffer to UTF-16 unicode string
         dim as DWSTRING wszText = string(len(txtBuffer),0)
         MemCpy( CAST(any PTR, wszText.m_pBuffer), strptr(txtBuffer), len(txtBuffer))

         ' Convert to UTF8 so it can display in the editor
         txtBuffer = UnicodeToUtf8(wszText)

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
    
    ' if incoming FunctionName then search for filename and line number.
    dim pData as DB2_DATA ptr    
    if len( wszFunctionName ) andalso (nLineNumber = -1) then
        ' Search for function, sub, or property (get/set)
        pData = gdb2.dbFindFunction( wszFunctionName, wszFilename) 
        if pData then
            wszFilename = pData->fileName
            nLineNumber = pData->nLineStart
        end if
    end if
    
    ' Not all documents exist on disk file. for example, a QuickRun file will exist in
    ' the editor but may never have a disk footprint. We need to search the project to
    ' determine if the filename has a pDoc already associated with it. If it does, then
    ' pass that pDoc rather than looking for a disk filename.
    dim pDoc as clsDocument ptr
    pDoc = gApp.GetDocumentPtrByFilename( wszFilename )

    if pDoc = 0 then
        if AfxFileExists(wszFilename) = false then exit function
        pDoc = gApp.CreateEmptyDocument()
        pDoc->LoadDiskFile( wszFilename )
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
    if id <> IDC_FRMOPTIONSLOCAL_CMDNEW then
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
' Calcluate the client area at bottom of Listbox not covered by a row (needed to manually
' paint the unused area in WM_ERASEBKGRD messages to avoid flicker.
' ========================================================================================
function GetListBoxEmptyClientArea( byval hListBox as HWND ) as RECT
    dim as RECT rc: GetClientRect( hListBox, @rc )
    ' if the number of lines in the listbox is less than the number per page then 
    ' calculate from last item to bottom of listbox, otherwise calculate based on
    ' the mod of the lineheight to listbox height so we can color the partial line
    ' that won't be displayed at the bottom of the list.
    dim as RECT rcItem
    SendMessage( hListBox, LB_GETITEMRECT, 0, cast(LPARAM, @rcItem) )
    dim as long itemHeight = rcItem.bottom - rcItem.top
    dim as long NumItems = ListBox_GetCount(hListBox)
    dim as long ItemsPerPage = ( rc.bottom \ itemHeight )
    dim as long nTopIndex = SendMessage( hListBox, LB_GETTOPINDEX, 0, 0 ) 
    dim as long visible_rows = 0

    if NumItems > 0 then
        ItemsPerPage = (rc.bottom - rc.top) / itemHeight
        dim as long bottom_index = (nTopIndex + ItemsPerPage)
        if bottom_index >= NumItems then bottom_index = NumItems - 1
        visible_rows = (bottom_index - nTopIndex) + 1
    end if

    rc.top = visible_rows * itemHeight 
    if rc.top > rc.bottom then rc.top = rc.bottom
    
    return rc
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
