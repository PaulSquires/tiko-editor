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

#include once "clsApp.bi"
#include once "frmExplorer.bi"
#include once "modMRU.bi"


''
''
function clsApp.IsValidDocumentPointer( byval pDocSearch as clsDocument ptr ) as boolean
    dim pDoc as clsDocument ptr = this.pDocList
    do until pDoc = 0
        if pDoc = pDocSearch then return true
        pDoc = pDoc->pDocNext
    loop
    function = false
end function


''
''
function clsApp.GetDocumentCount() as long
    dim pDoc as clsDocument ptr = this.pDocList
    dim nCount as long = 0
    do until pDoc = 0
        nCount = nCount + 1
        pDoc = pDoc->pDocNext
    loop
    function = nCount
end function


''
''
function clsApp.CreateEmptyDocument( byval IsNewFile as boolean = false ) as clsDocument ptr   
    dim pDoc as clsDocument ptr = this.AddNewDocument()
    pDoc->IsNewFlag = IsNewFile
    if IsNewFile then
        pDoc->NextFileNum += 1
        pDoc->DiskFilename = "Untitled" & pDoc->NextFileNum
        ' THE SINGLE POINT where the "default encoding for new files" setting is honoured.
        ' It replaces the hardcoded FILE_ENCODING_UTF8 initialiser on clsDocument.FileEncoding,
        ' which is still the fallback for a document created any other way.
        '
        ' Nothing downstream needed changing: AssignTextBuffer derives the Scintilla codepage
        ' from FileEncoding on the still-empty buffer (INVARIANT E1), Doc_EncodeForDisk emits
        ' the matching BOM on the first save, and the statusbar panel reads the field. This is
        ' also correct for the one OTHER caller that passes true -- the template path in
        ' frmMainFile -- because LoadDiskFile immediately sniffs the file's real BOM and
        ' overwrites the field, so a template keeps the encoding it is stored in.
        pDoc->FileEncoding = gConfig.NewFileEncoding
    end if
    ' A NEW buffer gets a real category immediately. clsDocument.SetProjectFileType is a
    ' no-op while IsNewFlag is true, so an untitled buffer would otherwise keep
    ' FILETYPE_UNDEFINED -- and once the Explorer groups strictly by category it would
    ' match no header and VANISH from the list while keeping its tab. Files loaded from
    ' disk or from a project file are assigned their real type straight after this.
    if IsNewFile then
        pDoc->ProjectFileType = FILETYPE_NORMAL
    else
        pDoc->ProjectFileType = FILETYPE_UNDEFINED
    end if
    pDoc->SetDocumentBuild()
    function = pDoc
end function


''
''
function clsApp.AddNewDocument() as clsDocument ptr 
    ' Add it to the start of the linked list
    dim pDoc as clsDocument ptr = new clsDocument
    pDoc->pDocNext = this.pDocList
    this.pDocList = pDoc
    function = pDoc
end function


''
''
function clsApp.RemoveDocument( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then exit function
        
    ' Remove from pDocList. Find the node that points to the incoming pDoc
    ' and then point that node to pDoc->pDocNext
    dim pDocSearch as clsDocument ptr = this.pDocList

    if pDocSearch = pDoc then
        this.pDocList = pDoc->pDocNext
    else
        do until pDocSearch = 0
            if pDocSearch->pDocNext = pDoc then
                pDocSearch->pDocNext = pDoc->pDocNext
                exit do
            end if
            pDocSearch = pDocSearch->pDocNext
        loop
    end if

    ' Drop the closed document's TODO entries (keyed by the same name its
    ' buffer scans used) and refresh the panel
    TodoStore_RemoveFile( ScanMgr_GetRootName( pDoc ) )
    frmOutput_UpdateToDoListview()

    ' Release memory associated with this pDoc
    if pDoc then delete(pDoc)

    function = 0
end function


''
''
function clsApp.RemoveAllDocuments() as long
    ' Remove from pDocList
    dim pDoc as clsDocument ptr = this.pDocList
    dim pDocNext as clsDocument ptr = this.pDocList

    do until pDoc = 0
        TodoStore_RemoveFile( ScanMgr_GetRootName( pDoc ) )
        pDocNext = pDoc->pDocNext
        ' Release memory associated with this pDoc
        delete pDoc
        pDoc = pDocNext
    loop
    this.pDocList = 0
    frmOutput_UpdateToDoListview()

    function = 0
end function


''
''
function clsApp.GetDocumentPtrByFilename( Byref wszName as wstring ) as clsDocument ptr
    if len(wszName) = 0 then return 0
    dim pDoc as clsDocument ptr = this.pDocList
    do until pDoc = 0
        if ucase(pDoc->DiskFilename) = ucase(wszName) then return pDoc
        pDoc = pDoc->pDocNext
    loop
    function = 0
end function


''
''
function clsApp.GetMainDocumentPtr() as clsDocument ptr
    ' Get the Main document for the project
    dim pDoc as clsDocument ptr = this.pDocList
    do until pDoc = 0
        if pDoc->ProjectFileType = FILETYPE_MAIN then return pDoc
        pDoc = pDoc->pDocNext
    loop
    function = 0
end function


''
''
function clsApp.GetResourceDocumentPtr() as clsDocument ptr
    ' Get the Resource document for the active project
    dim pDoc as clsDocument ptr = this.pDocList
    do until pDoc = 0
        if pDoc->ProjectFileType = FILETYPE_RESOURCE then return pDoc
        pDoc = pDoc->pDocNext
    loop
    function = 0
end function


''
''
function clsApp.GetHeaderDocumentPtr( byval pDocIn as clsDocument ptr ) as clsDocument ptr
    ' Get the Header document related to the pDoc document. The Header
    ' is simply the same source file name but with a ".bi" extension.
    if pDocIn = 0 then exit function
    
    dim pDoc as clsDocument ptr
    
    dim as DWSTRING wszFilename = _
                    AfxStrPathname( "PATH", pDocIn->Diskfilename ) & _
                    AfxStrPathname( "NAME", pDocIn->Diskfilename ) & ".bi"
    pDoc = this.GetDocumentPtrByFilename( wszFilename )
    
    if ( pDoc <> 0 ) andalso ( this.IsProjectActive = true ) then
        if pDoc->ProjectFileType <> FILETYPE_HEADER then 
            pDoc = 0
        end if
    end if
    
    function = pDoc
end function


''
''
function clsApp.GetSourceDocumentPtr( byval pDocIn as clsDocument ptr ) as clsDocument ptr
    ' Get the Source document related to the pDoc document. The Header
    ' is simply the same file name but with a ".bas" or ".inc" extension.
    if pDocIn = 0 then exit function
    
    dim as DWSTRING wszFilename
    dim pDoc as clsDocument ptr
    
    wszFilename = AfxStrPathname( "PATH", pDocIn->Diskfilename ) & _
                    AfxStrPathname( "NAME", pDocIn->Diskfilename ) & ".bas"
    pDoc = this.GetDocumentPtrByFilename( wszFilename )
    
    if pDoc = 0 then
        wszFilename = AfxStrPathname( "PATH", pDocIn->Diskfilename ) & _
                    AfxStrPathname( "NAME", pDocIn->Diskfilename ) & ".inc"
        pDoc = this.GetDocumentPtrByFilename( wszFilename )
    end if
    
    if ( pDoc <> 0 ) andalso ( this.IsProjectActive = true ) then
        select case pDoc->ProjectFileType 
            case FILETYPE_MAIN, FILETYPE_MODULE, FILETYPE_NORMAL
            case else
            pDoc = 0
        end select
    end if
    
    function = pDoc
end function


''
''
function clsApp.GetProjectCompiler() as long
    ' Get the compiler associated with this project
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).Id = this.ProjectBuild then
            if gConfig.Builds(i).Is32Bit then return IDM_32BIT
            if gConfig.Builds(i).Is64Bit then return IDM_64BIT
        end if   
    NEXT
    function = 0
end function


''
''  Does the workspace have a project file the USER chose a name for?
''
''  This is deliberately weaker than "is a project open". Once the untitled workspace
''  lands, every workspace is a project and ProjectFilename is never empty -- so the only
''  question left worth asking is whether the user has named it. Three things need that
''  answer and nothing else does: the window caption, the Save-As dialog's default folder,
''  and whether a file open counts toward the Recent Files list.
''
''  Filenames are compared case-insensitively because Windows paths are.
''
function clsApp.IsProjectNamed() as boolean
    dim as boolean bHasFile  = ( len(this.ProjectFilename) > 0 )
    dim as boolean bUntitled = ( ucase(this.ProjectFilename) = ucase(gConfig.UntitledProjectFilename) )
    if bHasFile = false then return false
    if bUntitled then return false
    return true
end function


''
''
function clsApp.SaveProject( byval bSaveAs as boolean = false ) as boolean

    dim wFilename as wstring * MAX_PATH
    dim wText     as wstring * MAX_PATH  
    
    wFilename = this.ProjectFilename

    if bSaveAs then
        ' Offer the dialog only a name the user chose. The untitled workspace's path is an
        ' internal detail under settings\ and must never be presented as a starting point.
        ' Note this clears the LOCAL only -- wFilename still carries ProjectFilename on the
        ' plain-Save path below, which writes it straight back to this.ProjectFilename.
        if this.IsProjectNamed() = false then wFilename = ""
        ' Display the Save File Dialog
        ' Default extension is ours, not WinFBE's. Matches frmProjectOptions_SaveProjectOptions.
        dim pwszName as wstring ptr = AfxIFileSaveDialog(HWND_FRMMAIN, @wFilename, @wstr(APPEXTENSION), IDM_PROJECTSAVE)
        if pwszName then
            wFilename = *pwszName
            CoTaskMemFree(pwszName)
        else
            return false
        end if
    end if

    ' Do NOT delete the existing file first. ProjectSaveToFile opens the stream with
    ' CTextStream.Create( name, bOverwrite := true, ... ), which truncates on its own, so
    ' the delete bought nothing -- and if the subsequent Create failed it had already
    ' destroyed the user's project with no way back.
    this.ProjectFilename = wFilename
    this.ProjectName = AfxStrPathname( "NAMEX", wFilename )
    if len(wFilename) = 0 then return false

    ' Do not create the untitled workspace's file until it actually holds something. An
    ' install launched once and closed empty should leave nothing behind -- and the restore
    ' path reaches here with an empty untitled workspace on EVERY launch, because it
    ' establishes one and then immediately replaces it with the named project it came for.
    ' A named project is always written: an emptied project is a real state to persist.
    if this.IsProjectNamed() = false then
        if this.GetDocumentCount() = 0 then return true
    end if

    gConfig.ProjectSaveToFile()

    ' Also need to add this new project name to the MRU list -- but only for a project the
    ' user named. The untitled workspace's path is an internal file under settings\ and
    ' must never appear in Recent Projects.
    if this.IsProjectNamed() then UpdateMRUProjectList(wFilename)

    frmMain_PositionWindows

    function = true
end function
        

''
''
function clsApp.ProjectSetFileType( _
            byval pDoc as clsDocument ptr, _
            byval wszFileType as DWSTRING _       ' new filetype 
            ) as LRESULT

    if pDoc = 0 then exit function
    
    dim wzFileExt as wstring * MAX_PATH 

    wzFileExt = AfxStrPathname( "EXTN",  pDoc->DiskFilename )
    
    ' Determine if the document already exists in the project. If it does not then
    ' make a determination of a default file type for this new file being added to the project.
    dim bFound as boolean = false
    dim pDocSearch as clsDocument ptr 
    pDocSearch = this.pDocList
    do until pDocSearch = 0
        if (pDocSearch = pDoc) andalso (pDoc->ProjectFileType <> FILETYPE_UNDEFINED ) then
            bFound = true: exit do
        end if
        pDocSearch = pDocSearch->pDocNext
    loop
    if bFound = false then
        if pDoc->ProjectFileType = FILETYPE_UNDEFINED then
            select case ucase(wzFileExt)
            case ".BAS":  wszFileType = FILETYPE_NORMAL
            case ".RC":   wszFileType = FILETYPE_RESOURCE
            case ".BI":   wszFileType = FILETYPE_HEADER
            case ".INC":  wszFileType = FILETYPE_NORMAL
            case else:    wszFileType = FILETYPE_NORMAL
            end select   
        end if
    end if
    
    ' Do check to make sure that no other MAIN or RESOURCE exist because
    ' there can only be one unique MAIN and one unique RESOURCE per project.

    ' if we are setting a "Main" project file then we need to toggle any other Main 
    ' file to "Normal". There can only be one Main file. Likewise, there can only be 
    ' one "Resource" file.

    if wszFileType = FILETYPE_MAIN then
        pDocSearch = this.pDocList
        do until pDocSearch = 0
            if pDocSearch->ProjectFileType = FILETYPE_MAIN then 
                pDocSearch->ProjectFileType = FILETYPE_NORMAL
            end if
            pDocSearch = pDocSearch->pDocNext
        loop
    end if      
    if wszFileType = FILETYPE_RESOURCE then
        pDocSearch = this.pDocList
        do until pDocSearch = 0
            if pDocSearch->ProjectFileType = FILETYPE_RESOURCE then 
                pDocSearch->ProjectFileType = FILETYPE_NORMAL
            end if
            pDocSearch = pDocSearch->pDocNext
        loop
    end if      

    pDoc->ProjectFileType = wszFileType

    ' Refresh the TopTabsInfo to ensure that the file's type is displayed properly
    frmTopTabsInfo_PositionWindows()

    function = 0
end function


''
''
function clsApp.RemoveAllSelectionAttributes() as long
    ' Remove all Attribute #8 selection highlights from the documents. This 
    ' occurs when the FindReplace dialog is closed. Therefore we need to apply
    ' it to all documents in all projects.
    dim pDoc as clsDocument ptr = this.pDocList
    
    do until pDoc = 0
        pDoc->RemoveSelectionAttributes()
        pDoc = pDoc->pDocNext
    loop   
    function = 0
end function

''
''
function clsApp.GetDocumentPtrByWindow( byval hWindow as hwnd) as clsDocument ptr
    ' Find the pDoc pointer based on all the available hWindow 
    ' for any loaded document.
    dim pDoc as clsDocument ptr = this.pDocList
    if hWindow = null then exit function
    
    do until pDoc = 0
        ' Determine if the incoming hWindow matches the clsDocument hWindow 
        if (hWindow = pDoc->hWindow(0)) orelse _
            (hWindow = pDoc->hWindow(1)) then
            return pDoc
        end if   
        pDoc = pDoc->pDocNext
    loop   
    function = 0
end function



