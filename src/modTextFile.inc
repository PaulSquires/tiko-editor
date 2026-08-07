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

'  These began inside clsConfig, because settings.ini was the first file to need them. They
'  are not clsConfig's business: nine other files read or write a whole text file the same
'  way, and two of them -- modThemes and modIniParse -- are included BEFORE clsConfig, so
'  they could not see the helpers at all. fbc is single-pass and include order is the whole
'  of its visibility rule.

#pragma once

' ========================================================================================
' modTextFile -- reading and writing whole text files, on PsCore's PsFile.
'
' These replaced CTextStream everywhere in tiko. CTextStream is the Scripting
' FileSystemObject behind a COM wrapper.
' It did three things at once: opened the file, decided its encoding, and split it into
' lines -- and the encoding decision was a BOOLEAN, `bUnicode`, which is why settings.ini
' is UTF-16 and the keyword files are whatever the ANSI codepage happens to be.
'
' Here the three are separate: PsFile moves the bytes, PsEncoding decides the encoding,
' PsStrParse splits the lines. The write is ONE call, so a settings.ini half-written by an
' early return in the middle of 166 WriteLines is no longer a state this program can reach.
'
' THE ON-DISK FORMAT DOES NOT CHANGE. settings.ini stays UTF-16LE with a BOM, byte for
' byte -- verified by writing it with both implementations and comparing the files.
'
' The two DWSTRINGs meet at .Utf8, which both types have and which is lossless in both
' directions. That is the same bridge modEncoding uses for the document save path.
' ========================================================================================
private function TextFile_PsPath( byval wszPath as DWSTRING ) as DWSTRING
    dim as DWSTRING d
    d.Utf8 = wszPath.Utf8
    return d
end function

' Reads a text file and hands back its LINES. The encoding is DETECTED from the BOM, so the
' UTF-16 settings.ini and the ANSI keyword files both come back correct without the caller
' saying which is which -- which CTextStream's bUnicode flag made every caller say, and get
' wrong the moment a file changed encoding.
private function TextFile_ReadLines( byval wszPath as DWSTRING, lines() as DWSTRING ) as boolean
    erase lines
    dim as boolean bOk
    dim as string sBytes = PsFileReadAll( TextFile_PsPath(wszPath), bOk )
    if bOk = false then return false

    dim as PsEncodingId encUsed
    dim as DWSTRING dwAll = PsEncDecodeAuto( sBytes, encUsed )
    dim as DWSTRING wszAll
    wszAll.Utf8 = dwAll.Utf8

    ' A file that ends with a line terminator yields a final EMPTY element, exactly as
    ' ReadLine + EOS did: that loop stopped at end-of-stream and never produced a phantom
    ' last line. Every caller skips empty lines anyway, and both keyword readers and the
    ' settings reader are asserted against the real files.
    dim as long n = PsStrParseCount( wszAll, vbcrlf )
    redim lines( 0 to n - 1 )
    for i as long = 1 to n
        lines(i - 1) = PsStrParse( wszAll, i, vbcrlf )
    next
    return true
end function

' Reads a whole text file as one string, replacing CTextStream.ReadAll.
private function TextFile_ReadAll( byval wszPath as DWSTRING ) as DWSTRING
    dim as boolean bOk
    dim as string sBytes = PsFileReadAll( TextFile_PsPath(wszPath), bOk )
    if bOk = false then return ""
    dim as PsEncodingId encUsed
    dim as DWSTRING dwAll = PsEncDecodeAuto( sBytes, encUsed )
    dim as DWSTRING r
    r.Utf8 = dwAll.Utf8
    return r
end function

' Writes text as UTF-16LE with a BOM -- what CTextStream.Create(path, true, true) produced
' and what every settings.ini already on disk is.
private function TextFile_WriteUtf16( byval wszPath as DWSTRING, byref wszText as DWSTRING ) as boolean
    dim as DWSTRING d
    d.Utf8 = wszText.Utf8
    dim as boolean bLossy
    dim as string sBytes = PsEncEncode( d, PSENC_UTF16LE_BOM, bLossy )
    return PsFileWriteAll( TextFile_PsPath(wszPath), sBytes )
end function

' Writes text as the ANSI codepage with no BOM -- what CTextStream.Create(path) produced for
' the keyword files, which are plain lists and were never Unicode.
private function TextFile_WriteAnsi( byval wszPath as DWSTRING, byref wszText as DWSTRING ) as boolean
    dim as DWSTRING d
    d.Utf8 = wszText.Utf8
    dim as boolean bLossy
    dim as string sBytes = PsEncEncode( d, PSENC_ANSI, bLossy )
    return PsFileWriteAll( TextFile_PsPath(wszPath), sBytes )
end function

