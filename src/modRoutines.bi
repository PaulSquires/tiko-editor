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

#pragma once

' modRoutines was a junk drawer; three coherent groups were split out of it. These are
' forwarded from here so the ~40 files that include modRoutines.bi did not have to change.
#include once "modEncoding.bi"
#include once "modPaths.bi"
#include once "modUpdateCheck.bi"



' Size = 32 bytes
TYPE HH_AKLINK 
    cbStruct     as long         ' int       cbStruct;     // sizeof this structure
    fReserved    as boolean      ' BOOL      fReserved;    // must be FALSE (really!)
    pszKeywords  as wstring ptr  ' LPCTSTR   pszKeywords;  // semi-colon separated keywords
    pszUrl       as wstring ptr  ' LPCTSTR   pszUrl;       // URL to jump to if no keywords found (may be NULL)
    pszMsgText   as wstring ptr  ' LPCTSTR   pszMsgText;   // Message text to display in MessageBox if pszUrl is NULL and no keyword match
    pszMsgTitle  as wstring ptr  ' LPCTSTR   pszMsgTitle;  // Message text to display in MessageBox if pszUrl is NULL and no keyword match
    pszWindow    as wstring ptr  ' LPCTSTR   pszWindow;    // Window to display URL in
    fIndexOnFail as boolean      ' BOOL      fIndexOnFail; // Displays index if keyword lookup fails.
END TYPE

#Define HH_DISPLAY_TOPIC   0000 
#Define HH_DISPLAY_TOC     0001
#Define HH_KEYWORD_LOOKUP  0013
#Define HH_HELP_CONTEXT    0015


declare function RichEditCenterSingleLineText( byval hRichEdit as HWND ) as long
declare function CompleteIncludeFilename( byval pDoc as clsDocument ptr, byval wszFilename as string ) as string
declare function getTextWidth( byval hwnd as HWND, byref wszText as wstring, byval FontIndex as long ) as long
declare function SpawnPreviousInstance() as boolean
' Bounded read of a WM_COPYDATA payload -- see its header. Pure, so it is assertable
' without a second process. Returns "" for anything malformed.
declare function SafeCopyDataString( byval pData as any ptr, byval cbData as ulong ) as DWSTRING
declare sub CopyData_RunSelfTest()
declare function ReloadDocument( byref wszFilename as wstring ) as long
declare function GetTemporaryFilename( byref wszFolder as wstring, byref wszExtension as wstring) as string
declare function GetFontCharSetID(byref wzCharsetName as DWSTRING ) as long
declare function Scintilla_GetTextBytes( byval hEdit as hwnd ) as string
declare function Scintilla_StripTrailingWhitespace( byval hEdit as hwnd ) as long
declare function GetFileToString( byref wszFilename as const wstring, byref txtBuffer as string, byval pDoc as clsDocument ptr ) as boolean
declare function IsCurrentLineIncludeFilename() as boolean
declare function OpenSelectedDocument( byref wszFilename as wstring, byref wszFunctionName as wstring = "", byval nLineNumber as long = -1 ) as clsDocument ptr
declare function AfxIFileOpenDialogW( byval hwndOwner as HWND, byval idButton as long) as wstring Ptr
declare function AfxIFileOpenDialogMultiple( byval hwndOwner as HWND, byval idButton as long) as IShellItemArray ptr
declare function AfxIFileSaveDialog( byval hwndOwner as HWND, byval pwszFileName as wstring Ptr, byval pwszDefExt as wstring Ptr, byval id as long = 0, byval sigdnName as SIGDN = SIGDN_FILESYSPATH ) as wstring Ptr
declare function LoadLocalizationFile( byref wszFileName as DWSTRING, byval IsEnglish as boolean = false ) as boolean
declare function GetProcessImageName( byval pe32w as PROCESSENTRY32W ptr, byval pwszExeName as wstring ptr ) as long
declare function IsProcessRunning( byval wszExeFileName as DWSTRING ) as boolean
declare function GetRunExecutableFilename() as DWSTRING

' Add a chr(9)-delimited row to a multi-column PsListTree (see modRoutines.inc).
declare function ListBox_AddTabbedRow( byval hCtl as HWND, byval wszTabbed as DWSTRING ) as long
