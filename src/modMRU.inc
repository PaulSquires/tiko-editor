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

#include once "modMRU.bi"

''
''

''
''


''
''


''
''


''
''


''
''
function OpenMRUFile( _
            byval hwnd as HWND, _
            byval wID as long _
            ) as long
    dim pDoc as clsDocument ptr
    dim wszFile as wstring * MAX_PATH

    wszFile = gConfig.MRU(wID - IDM_MRUBASE)
    
    pDoc = OpenSelectedDocument( wszFile )
    if pDoc then pDoc->SetProjectFileType

    LoadExplorerFiles()
    LoadFunctionsFiles()

    function = 0
end function


''
''
function ClearMRUlist( byval wID as long ) as long
    Select Case wID
        Case IDM_MRUCLEAR
            for i as long = 0 To 9
                gConfig.MRU(i) = ""
            next
        Case IDM_MRUPROJECTCLEAR
            for i as long = 0 To 9
                gConfig.MRUPROJECT(i) = ""
            next
    end Select
    function = 0
end function


''
''
function UpdateMRUList( byref wzFilename as wstring ) as long
    dim idxExistsAt as long = -1   ' index of where filename already exists 
    dim i           as long
    dim wzFile      as wstring * MAX_PATH
    
    if gApp.IsProjectLoading then exit function
    ' A NAMED project tracks its own files, so they do not belong in Recent Files.
    ' An untitled workspace is just loose files and must keep feeding this list --
    ' testing "is a project open" here would kill Recent Files outright once every
    ' workspace is a project.
    if gApp.IsProjectNamed() then exit function
     
    ' Search the 10 MRU items to see if the filename already exists and
    ' also for an available entry to put the filename in.
    for i = 0 To 9
        wzFile = gConfig.MRU(i)
        wzFile = ProcessFromCurdriveApp(wzFile)
        if ucase(wzFile) = PsUCase(wzFilename) then
            idxExistsAt = i:  exit for
        end if   
    next
    
    ' if does not already exist then we simply need to move all entries
    ' down and add at the beginning of the list. The original 10th entry
    ' will now drop off the list.
    if idxExistsAt = -1 then idxExistsAt = 9
    
    for i = idxExistsAt To 1 step -1
        gConfig.MRU(i) = gConfig.MRU(i-1)
    next
    gConfig.MRU(0) = wzFilename 
    
    ' Save the MRU to the configuration file. Only write the MRU items
    ' the ini file rather than overwriting the whole file. This enables
    ' the editor to work better with external tools that manually modify the
    ' settings.ini file.
    gConfig.WriteMRU

    function = 0
end function  


''  PROJECTS  ''

''
''
function OpenMRUProjectFile( _
            byval hwnd as HWND, _
            byval wID as long _
            ) as long
    dim wzFile as wstring * MAX_PATH
    wzFile = gConfig.MRUPROJECT(wID - IDM_MRUPROJECTBASE)
    frmMain_OpenProjectSafely(hwnd, wzFile) 
    function = 0
end function

    
''
''
function UpdateMRUProjectList( byval wszFilename as DWSTRING ) as long

    dim idxExistsAt as long = -1   ' index of where filename already exists 
    dim i           as long
    
    ' Search the 10 MRU items to see if the filename already exists and
    ' also for an available entry to put the filename in.
    for i = 0 To 9
        gConfig.MRUProject(i) = ProcessFromCurdriveApp(gConfig.MRUProject(i))
        if ucase(gConfig.MRUProject(i)) = PsUCase(wszFilename) then
            idxExistsAt = i:  exit For
        end if   
    next
    
    ' if does not already exist then we simply need to move all entries
    ' down and add at the beginning of the list. The original 10th entry
    ' will now drop off the list.
    if idxExistsAt = -1 then idxExistsAt = 9

    for i = idxExistsAt To 1 step -1
        gConfig.MRUProject(i) = gConfig.MRUProject(i-1)
    next
    gConfig.MRUProject(0) = wszFilename 

    ' Save the MRU to the configuration file. Only write the MRU items
    ' the ini file rather than overwriting the whole file. This enables
    ' the editor to work better with external tools that manually modify the
    ' settings.ini file.
    gConfig.WriteMRUProjects

    function = 0
end function  



