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
#include once "app/modEncoding.bi"
#include once "app/modPaths.bi"
#include once "app/modUpdateCheck.bi"



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
declare function getTextWidth( byval hwnd as HWND, byval wszText as DWSTRING, byval FontIndex as long ) as long
declare function SpawnPreviousInstance() as boolean
' Bounded read of a WM_COPYDATA payload -- see its header. Pure, so it is assertable
' without a second process. Returns "" for anything malformed.
declare function SafeCopyDataString( byval pData as any ptr, byval cbData as ulong ) as DWSTRING
declare sub CopyData_RunSelfTest()
declare function ReloadDocument( byval wszFilename as DWSTRING ) as long
declare function GetTemporaryFilename( byval wszFolder as DWSTRING, byval wszExtension as DWSTRING) as string
declare function GetFontCharSetID(byref wzCharsetName as DWSTRING ) as long
' Scintilla_GetTextBytes / Scintilla_StripTrailingWhitespace moved to
' app/modSciText.bi -- see there for the repaint the caller now owns.
' NEITHER of these could move to app/ -- see modRoutines.inc for what each is bound to.
' clsDocument reaches them as gAppHost.LoadFileText and gAppHost.ResolveIncludePath.
declare function CompleteIncludeFilename( byval pDoc as clsDocument ptr, byval wszFilename as string ) as string
' GetFileToString's declaration went with its body in 7c step 9 -- app/modEncoding.inc's
' Doc_ReadFromDisk is the reader now, for both binaries.
declare function IsCurrentLineIncludeFilename() as boolean
declare function OpenSelectedDocument( byval wszFilename as DWSTRING, byval wszFunctionName as DWSTRING = "", byval nLineNumber as long = -1 ) as clsDocument ptr
declare function AfxIFileOpenDialogW( byval hwndOwner as HWND, byval idButton as long) as wstring Ptr
declare function AfxIFileOpenDialogMultiple( byval hwndOwner as HWND, byval idButton as long) as IShellItemArray ptr
declare function AfxIFileSaveDialog( byval hwndOwner as HWND, byval pwszFileName as wstring Ptr, byval pwszDefExt as wstring Ptr, byval id as long = 0, byval sigdnName as SIGDN = SIGDN_FILESYSPATH ) as wstring Ptr
' LoadLocalizationFile's DECLARATION moved with its body to app/modLocalization.bi/.inc.
' Left as a note rather than deleted silently: this file is where callers look for it, and
' the reason it is no longer here is the reason the app layer exists.
declare function GetProcessImageName( byval pe32w as PROCESSENTRY32W ptr, byval pwszExeName as wstring ptr ) as long
declare function IsProcessRunning( byval wszExeFileName as DWSTRING ) as boolean
declare function GetRunExecutableFilename() as DWSTRING

' Add a chr(9)-delimited row to a multi-column PsListTree (see modRoutines.inc).
declare function ListBox_AddTabbedRow( byval hCtl as HWND, byval wszTabbed as DWSTRING ) as long

'' A window's text as a PsCore DWSTRING. Replaces AfxGetWindowText, whose only
'' problem was that it returns AfxNova's OWN DWSTRING and so needed AfxW() at
'' every call -- see modAfxBridge.bi. The window is the shell's, so reading it
'' with Win32 directly is not a layering violation; taking the TEXT back through
'' a second string type was the thing worth removing.
declare function WindowText( byval hWin as HWND ) as DWSTRING
