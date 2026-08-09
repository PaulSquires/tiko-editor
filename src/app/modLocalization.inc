'' ========================================================================================
'' modLocalization -- the .lang LOADER.
''
'' modLocalization.bi declares the four tables; this fills them. They were in different
'' layers until 2026-08-09: the tables here, the loader in modRoutines.inc, shell-side.
'' The header said so and gave the reason -- "the app layer needs to READ the table, not to
'' load it" -- which was true for exactly as long as tiko.exe was the only binary.
''
'' Phase 7c's shell binary has to load it too, and without it every L() call returns an
'' EMPTY STRING rather than the English default: L(id,"default") is `#Define L(e,s) LL(e)`,
'' so the second argument is documentation at the call site and nothing more. A menubar
'' built against an unloaded table renders eight blank titles and says nothing about why.
''
'' The function moved rather than being duplicated because it was ALREADY app-layer clean:
'' PsFileExists, PsLen, PsLeft, PsMid, PsInStr, PsVal, PsUCase, PsStrTrimAny, PsStrParse and
'' TextFile_ReadLines, with not one AfxNova or Win32 token. Only its address had to change.
''
'' NOTHING ELSE MOVED. modRoutines.inc keeps the rest of its 40-odd functions; this is the
'' one the app layer's own declarations were already reaching for.
'' ========================================================================================

#pragma once

#include once "app/modLocalization.bi"
#include once "app/modTextFile.inc"
'  Load a .lang localization file from disk and populate the localization array.
'  IsEnglish fills gLangEnglish instead of LL -- the editor loads English first, so a
'  partial translation renders as English rather than as blanks.
function LoadLocalizationFile( _
            byref wszFileName as DWSTRING, _
            byval IsEnglish as boolean = false _
            ) as boolean

    ' default that the file failed to load
    function = false
    if PsFileExists( wszFileName ) = 0 then exit function

''  DWSTRING, NOT AfxNova's BSTRING. The `**` double-deref that used to appear below was
''  BSTRING's way of reaching the raw buffer so fbc's RTRIM would not mangle Cyrillic --
''  PsStrTrimAny takes a DWSTRING and needs no such thing.
    dim as DWSTRING wst, wKey, wData
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
        wData = PsMid(wst, i+1)
        
        nKey  = PsVal(wKey)
        nData = PsVal(wData)

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
                    gLangEnglish(nKey) = PsStrTrimAny(PsStrParse(wData, 1, ";"), chr(9,32))
                end if
            else
                if (nKey >= lbound(LL)) andalso (nKey <= ubound(LL)) then
                    ' Remove any comments from end of the line. Comments begin with
                    ' a semicolon character.
                    ' Use ** to ensure that cyrillic langauge gets converted correctly. FB intrinsic
                    ' functions (RTRIM) automatically convert those incorrectly when using CBSTR or DWSTRING.
                    LL(nKey) = PsStrTrimAny(PsStrParse(wData, 1, ";"), chr(9,32))
                    ' if the local phrase is empty then fill it using the English version.
                    if PsLen(LL(nKey)) = 0 then 
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
