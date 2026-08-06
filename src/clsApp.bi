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
        ' ---- MODE FLAGS: every one of these is a SPAN, not a state ----------------------
        ' Each is set true, something happens, and it is set false again. Two rules follow,
        ' and both have been broken here before:
        '
        '   1. EVERY EXIT FROM THE SPAN MUST CLEAR IT, including early returns. LoadConfigFile
        '      and SaveConfigFile each had an early "return true" between the set and the
        '      clear, and because the only reader of PreventConfigLoad short-circuits on it,
        '      ONE unreadable settings.ini killed config-change detection for the whole
        '      session, silently. Both are wrappers around a body now, so no exit can leak.
        '      Verified 2026-08-03: no other span here contains an early return.
        '
        '   2. THEY DO NOT NEST. Each is a plain boolean, so an inner clear ends an outer
        '      span early. SuppressNotify is the one to watch -- it has seven set/clear
        '      pairs across five files. Verified 2026-08-03: every span makes only leaf
        '      calls (SciExec, SciMsg, SetWindowRedraw, string helpers), so none can nest
        '      today. If a span ever grows a call that reaches another one, this must become
        '      a counter; a boolean cannot express it.
        '
        ' These were NOT grouped into an APP_MODES type with paired accessors, which the
        ' audit proposed. With rule 1 discharged and rule 2 currently unreachable, the
        ' accessors would enforce nothing, and the grouping alone is a rename across every
        ' call site -- churn on working code with no property gained. Revisit if a span
        ' grows an early return or a nested call.
        PreventConfigLoad          as boolean           ' temporarily suppress messagepump checking (set during Load/Save config)
        SuppressNotify             as boolean           ' temporarily suppress Scintilla notifications
        KeepTitleBarActive         as boolean           ' a popup surface (menubar popup, Search Symbol) owns
                                                        ' activation; frmMain's WM_NCACTIVATE keeps painting
                                                        ' the caption as active
        ' No hand-rolled drag state lives here any more. Every drag site is a reusable
        ' control that owns its own capture and cursor -- the four splitters and the
        ' Output/Explorer bars are PsSplitter, the list panes are PsListTree, the top tab
        ' strip is PsTabBar -- so bDragActive, bDragTabActive and ptDragTabPrev went, along
        ' with hWndPanel and the single/double-click timer fields before them.
        IncludeFilename            as DWSTRING
        ' MIGRATION ONLY. Loaded from settings.ini's [Notes] block and read in exactly one
        ' place: Workspace_EstablishUntitled hands it to ProjectNotes the first time an
        ' untitled workspace is created with no file on disk. Nothing else may read or
        ' write it.
        '
        ' REMOVAL CRITERION, because "once no settings.ini in the wild still carries the
        ' section" was not one -- it named no condition anyone could evaluate, which is how
        ' migration-only fields become permanent.
        '
        ' SaveConfigFile NO LONGER EMITS THE SECTION (clsConfig.inc, and
        ' TIKO_SAVE_SELFTEST asserts it by writing a settings.ini and scanning it). So the
        ' migration is strictly one-way: any settings.ini that has been saved ONCE by
        ' tiko 1.3.2 or later has already lost the block, and every ordinary session saves
        ' on exit. The field is therefore dead for any user who has run a current build
        ' even once.
        '
        ' DELETE THIS FIELD, its loader in LoadConfigFileBody, and its one reader in
        ' frmMainProject, at the SECOND feature release after 1.3.2 -- by then a user who
        ' skipped a release has still been migrated. Nothing else needs to be checked.
        NonProjectNotes            as DWSTRING
'        wszPanelText               as DWSTRING             ' Current file loading or being compiled (for statusbar updating)
 '       FileLoadingCount           as long              ' Track count of files loading for statusbar display
        hIconPanel                 as long              ' Success/failure of most previous compile (for Statusbar updating)
        IsNewProjectFlag           as boolean
        IsProjectLoading           as boolean           ' Project loading. Disable some screen updating.
        IsFileLoading              as boolean           ' File loading. Disable some screen updating.
        ' IsCompiling is GONE. Build state belongs to modBuildService now -- it was one
        ' boolean in this flag bag whose only reader was WM_SETCURSOR, so it drove the busy
        ' cursor while looking like a guard against a second build.
        IsShutDown                 as boolean           ' App is currently closing
        wszLastOpenFolder          as DWSTRING             ' remembers the last opened folder for the Open Dialog
        wszQuickRunFilename        as DWSTRING
        IsQuickRun                 as boolean           ' Set in modCompile and used in SetDocumentErrorPosition()
        
        bShowSpinner               as boolean           ' Display the loading/compiling spinner
        SpinnerCurrentFrame        as long = 0          ' The index of the icon to use when timer fires.
        SpinnerTimerID             as long = 101       
        
        ' IsUpdateAvailable and IsDebuggerActive MOVED to app/modAppState.bi. Neither
        ' has anything to do with a window, and both are read by menus -- which is
        ' app-layer code that cannot see this type. Same move, same reason, as
        ' IsCompiling -> modBuildService.
        
        DebugTimerID               as long = 102
        
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
        declare function SaveProject( byval bSaveas as boolean = False, byref wszForcedName as wstring = "" ) as boolean
        declare function ProjectSetFileType( byval pDoc as clsDocument ptr, byval wszFiletype as DWSTRING ) as LRESULT
        declare function GetProjectCompiler() as long
        declare function IsProjectNamed() as boolean

end type

