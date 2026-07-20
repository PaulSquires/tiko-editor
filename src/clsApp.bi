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

type clsApp
    private: 
        
    public:
        pDocList                   as clsDocument ptr   ' Single linked list of loaded files
        pfnCreateLexerfn           as CreateLexerFn
        PreventConfigLoad          as boolean           ' temporarily suppress messagepump checking (set during Load/Save config)
        SuppressNotify             as boolean           ' temporarily suppress Scintilla notifications
        KeepTitleBarActive         as boolean           ' a popup surface (menubar popup, Search Symbol) owns
                                                        ' activation; frmMain's WM_NCACTIVATE keeps painting
                                                        ' the caption as active
        bDragTabActive             as boolean           ' a tab in the top tabcontrol is being dragged
        ptDragTabPrev              as POINT             ' used for comparing delta movement of sursor during tab dragging
        bDragActive                as boolean           ' splitter drag is currently active
        idTimerOutputPanel         as long = 110        ' used for handling single/double click of Output window splitter bar
        doubleClickReceived        as boolean           ' used for handling single/double click of Output window splitter bar
        hWndPanel                  as HWND              ' the panel being split left/right or up/down
        IncludeFilename            as DWSTRING
        NonProjectNotes            as DWSTRING             ' Save/load from config file
'        wszPanelText               as DWSTRING             ' Current file loading or being compiled (for statusbar updating)
 '       FileLoadingCount           as long              ' Track count of files loading for statusbar display
        hIconPanel                 as long              ' Success/failure of most previous compile (for Statusbar updating)
        IsNewProjectFlag           as boolean
        IsProjectLoading           as boolean           ' Project loading. Disable some screen updating.
        IsFileLoading              as boolean           ' File loading. Disable some screen updating.
        IsCompiling                as boolean           ' File/Project currently being compiled (spinning mouse cursor).
        IsShutDown                 as boolean           ' App is currently closing
        wszCommandLine             as DWSTRING             ' non-project commandline (not saved to file)
        wszLastOpenFolder          as DWSTRING             ' remembers the last opened folder for the Open Dialog
        wszQuickRunFilename        as DWSTRING
        IsQuickRun                 as boolean           ' Set in modCompile and used in SetDocumentErrorPosition()
        
        bShowSpinner               as boolean           ' Display the loading/compiling spinner
        SpinnerCurrentFrame        as long = 0          ' The index of the icon to use when timer fires.
        SpinnerTimerID             as long = 101       
        
        IsUpdateAvailable          as boolean = false   ' Set in DoCheckForUpdates() for server check for updated version available
        
        IsDebuggerActive           as boolean
        DebugTimerID               as long = 102
        
        IsSessionActive            as boolean
        SessionName                as DWSTRING
        SessionFilename            as DWSTRING
        
        IsProjectActive            as boolean
        ProjectBuild               as string            ' default build configuration for the project (GUID)
        ProjectName                as DWSTRING
        ProjectFilename            as DWSTRING
        ProjectOther32             as DWSTRING             ' compile flags 32 bit compiler
        ProjectOther64             as DWSTRING             ' compile flags 64 bit compiler
        ProjectNotes               as DWSTRING             ' Save/Load from project file
        ProjectCommandLine         as DWSTRING
        ProjectManifest            as long              ' T/F create a generic resource and manifest file

        declare function IsValidDocumentPointer( byval pDocSearch as clsDocument ptr ) as boolean
        declare function RemoveAllSelectionAttributes() as long
        declare function CreateEmptyDocument( byval IsNewFile as boolean = false ) as clsDocument ptr   
        declare function AddNewDocument() as clsDocument ptr 
        declare function RemoveDocument( byval pDoc as clsDocument ptr ) as long
        declare function RemoveAllDocuments() as long
        declare function GetDocumentCount() as long
        declare function GetDocumentPtrByWindow( byval hWindow as hwnd) as clsDocument ptr
        declare function GetDocumentPtrByFilename( Byref wszName as wstring ) as clsDocument ptr
        declare function GetMainDocumentPtr() as clsDocument ptr
        declare function GetResourceDocumentPtr() as clsDocument ptr
        declare function GetSourceDocumentPtr( byval pDocIn as clsDocument ptr ) as clsDocument ptr
        declare function GetHeaderDocumentPtr( byval pDocIn as clsDocument ptr ) as clsDocument ptr
        declare function SaveProject( byval bSaveas as boolean = False ) as boolean
        declare function ProjectSetFileType( byval pDoc as clsDocument ptr, byval wszFiletype as DWSTRING ) as LRESULT
        declare function GetProjectCompiler() as long
        
end type

