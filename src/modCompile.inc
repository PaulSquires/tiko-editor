''    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
''    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
''
''    This program is free software: you can redistribute it and/or modify
''    it under the terms of the GNU General Public License as published by
''    the Free Software Foundation, either version 3 of the License, or
''    (at your option) any later version.
''
''    This program is distributed in the hope that it will be useful,
''    but WITHOUT any WARRANTY; without even the implied warranty of
''    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
''    GNU General Public License for more details.

''
''
''   modCompile.inc
''
''   Code here handles the entire compiling process. It prepares the necessary
''   command line data to pass to the applicable 32 or 64 version of the FBC
''   compiler. Compile time errors are parsed through analyis of the log file
''   and presented to the user via the Compiler Results listview and the Compiler
''   Log File text box.
''
''

#include once "modCompile.bi"
#include once "modCompileErrors.bi"
#include once "frmUserTools.bi"
#include once "frmDebug.bi"
#include once "frmOutput.bi"



' ========================================================================================
' Dynamically allocate the compile type and initialize it.
' ========================================================================================
function code_Initialize() as COMPILE_TYPE ptr
    ' Indicate in the StatusBar that we have started to compile
    SetCompileStatusBarMessage( L(294, "Generating compiler files") & "...", 0 )
    gApp.IsCompiling = true    

    dim pCompile as COMPILE_TYPE ptr = new COMPILE_TYPE
    if pCompile = null then return null

    ' Save the mouse cursor so that it can be restored after compiling.
    pCompile->hCurSave = GetCursor()

    return pCompile
end function


' ========================================================================================
' Tear down, reset, and deallocate the compile process.
' ========================================================================================
function code_Cleanup( byval pCompile as COMPILE_TYPE ptr ) as boolean
    gApp.IsCompiling = false
    
    ' Restore the mouse cursors
    SetCursor( pCompile->hCurSave )
    ResetScintillaCursors()

    if pCompile then 
        delete( pCompile )
        pCompile = null
    end if

    return true
end function


' ========================================================================================
' Get information about the currently selected compiler path. If there is a 
' problem then return false so that the compile process can terminate.
' ========================================================================================
function code_SetCompilerPath( byval pCompile as COMPILE_TYPE ptr ) as boolean
    ' Get the array index for the current selected build (project or document)
    dim as long idxBuild = frmBuildConfig_getActiveBuildIndex()
    if idxBuild = -1 then idxBuild = 0
    
    ' Convert the compiler path to reflect path on the current computer.
    if gConfig.Builds(idxBuild).Is32bit then
        pCompile->bCompile32 = true
        gConfig.FBWINCompiler32 = ProcessFromCurdriveApp( gConfig.FBWINCompiler32 )
        pCompile->CompilerPath = gConfig.FBWINcompiler32
    elseif gConfig.Builds(idxBuild).Is64bit then
        pCompile->bCompile64 = true
        gConfig.FBWINCompiler64 = ProcessFromCurdriveApp( gConfig.FBWINCompiler64 )
        pCompile->CompilerPath = gConfig.FBWINcompiler64
    end if

    pCompile->CompileFlags = gConfig.Builds(idxBuild).wszOptions

    ' Convert relative path to absolute path if needed.
    if AfxPathIsRelative(pCompile->CompilerPath) then
        pCompile->CompilerPath = AfxPathCombine(AfxGetExePathName, pCompile->CompilerPath)
    end if

    ' Check to see if the compiler exists
    if AfxFileExists(pCompile->CompilerPath) = false then 
        dim as DWSTRING wszMsg = _
            L(165,"Invalid defined compiler path.") & vbcrlf & _
            "Path: " & pCompile->CompilerPath
        SetCompileStatusBarMessage( "", 0 )
        TikoMsgBox( HWND_FRMMAIN, wszMsg, L(164,"Error"), _
                TMB_ICON_INFO, TMB_OK )
        code_Cleanup( pCompile )
        return false        
    end if
    
    return true
end function


' ========================================================================================
' Set Main file to be compiled. 
' A designated Main file always wins. With none, fall back to the active document -- which
' is what a workspace of loose files has always done, and is now the rule everywhere. Only
' when there is no Main AND no active document is there nothing to build.
' ========================================================================================
function code_SetMainFile( byval pCompile as COMPILE_TYPE ptr ) as boolean
    dim pDoc as clsDocument ptr
    pDoc = gApp.GetMainDocumentPtr()
    if pDoc = null then
        pDoc = gTTabCtl.GetActiveDocumentPtr()
    end if

    if pDoc = null then
        TikoMsgBox( HWND_FRMMAIN, L(171,"No Main file specified for the project."), L(164,"Error"), _
                        TMB_ICON_INFO, TMB_OK )
        return false
    end if
    pCompile->pDocMain = pDoc

    ' if this is a QuickRun then we need to copy the current source code to a temporary bas file
    ' and compile that bas file instead. 
    if pCompile->CompileID = IDM_QUICKRUN then  
        if AfxFileExists(pDoc->DiskFilename) then
            pCompile->MainFolder = AfxStrPathname("PATH", pDoc->DiskFilename)
        else    
            pCompile->MainFolder = AfxGetExePathName()
        end if

        pCompile->MainFilename = pCompile->MainFolder & QUICKRUNBAS
        pCompile->MainName     = AfxStrPathname("NAME", pCompile->MainFilename)
        
        pDoc->SaveTempFile( pCompile->MainFilename )

    else
        pCompile->MainFilename = pDoc->DiskFilename
        pCompile->MainName     = AfxStrPathname("NAME", pDoc->DiskFilename)
        pCompile->MainFolder   = AfxStrPathname("PATH", pDoc->DiskFilename)
    end if

    ' Need to determine the output filename in order to search for any existing running process.
    pCompile->OutputFilename = pCompile->MainFolder & pCompile->MainName & ".exe"
    if instr(" " & ucase(pCompile->CompileFlags), " -DLL") then
        pCompile->OutputFilename = pCompile->MainFolder & pCompile->MainName & ".dll"
    end if
    if instr(" " & ucase(pCompile->CompileFlags), " -DYLIB") then
        pCompile->OutputFilename = pCompile->MainFolder & pCompile->MainName & ".dll"
    end if
    if instr(" " & ucase(pCompile->CompileFlags), " -LIB") then
        pCompile->OutputFilename = pCompile->MainFolder & "lib" & pCompile->MainName & ".a"
    end if

    return true
end function


' ========================================================================================
' PRE-COMPILE UserTools 
' ========================================================================================
function code_PreCompileTools( byval pCompile as COMPILE_TYPE ptr ) as boolean
    for i as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
        if gConfig.Tools(i).Action = USERTOOL_ACTION_PRECOMPILE then
            frmUserTools_ExecuteUserTool( pCompile, i )
        end if   
    next
    return true
end function


' ========================================================================================
' POST-COMPILE UserTools 
' ========================================================================================
function code_PostCompileTools( byval pCompile as COMPILE_TYPE ptr ) as boolean
    for i as long = lbound(gConfig.Tools) to ubound(gConfig.Tools)
        if gConfig.Tools(i).Action = USERTOOL_ACTION_POSTCOMPILE then
            frmUserTools_ExecuteUserTool( pCompile, i )
        end if   
    next
    return true
end function


' ========================================================================================
' Expand any user defined "Additional Include Paths" 
' ========================================================================================
function code_AddPathsToLinkModules( byval pCompile as COMPILE_TYPE ptr ) as long
    ' SEARCH ORDER
    ' (1) Current folder
    ' (2) User defined Include paths
    ' (3) \inc folder for the selected compiler toolchain

    dim as DWSTRING wszResult  ' the final fully path qualified list of space separated module filenames
    dim as DWSTRING wszTemp
    
    dim as DWSTRING wszCurrentFolder = AfxStrPathname("PATH", pCompile->MainFilename)
    dim as DWSTRING wszCompilerIncFolder = AfxStrPathname("PATH", pCompile->CompilerPath) & "inc\"
    
    dim as DWSTRING wszLinkModules = AfxStrShrink(pCompile->LinkModules)  
    
    dim as long numModules = AfxStrParseCount(wszLinkModules, " ")
    for i as long = 1 to numModules
        dim as DWSTRING modulename = trim(AfxStrParse(wszLinkModules, i, " "))
        ' Remove any enclosing double quotes from the filename
        modulename = AfxStrRemove(modulename, chr(34))

        if len(modulename) = 0 then continue for

        ' (1) Current folder
        wszTemp = wszCurrentFolder & modulename
        if AfxFileExists(wszTemp) then
            wszResult = wszResult & chr(34) & wszTemp & chr(34) & " "
            continue for
        end if
    
        ' (2) User defined Include paths
        ' Iterate the optional semi colon separated list of compiler include paths
        pCompile->CompileIncludes = ""
        dim as DWSTRING includePath
        dim as long numpaths = AfxStrParseCount(gConfig.CompilerIncludes, ";")
        for i as long = 1 to numpaths
            includePath = rtrim(AfxStrParse(gConfig.CompilerIncludes, i, ";"))
            ' Remove any enclosing double quotes from the includepath
            includePath = AfxStrRemove(includePath, chr(34)) & "\"   ' ensure trailing backslash
            if len(includePath) then 
            wszTemp = includePath & modulename
            if AfxFileExists(wszTemp) then
                wszResult = wszResult & chr(34) & wszTemp & chr(34) & " "
                continue for: continue for
            end if
            end if
        next
    
        ' (3) \inc folder for the selected compiler toolchain
        wszTemp = wszCompilerIncFolder & modulename
        if AfxFileExists(wszTemp) then
            wszResult = wszResult & chr(34) & wszTemp & chr(34) & " "
            continue for
        end if
    next
    
    pCompile->LinkModules = wszResult
    
    return 0
end function


' ========================================================================================
' Search main source code for any user embedded compile directives. These will override
' anything that was set at the default or project level.
' ========================================================================================
function code_SearchForDirectives( byval pCompile as COMPILE_TYPE ptr ) as boolean
    redim directives(any) as COMPILE_DIRECTIVES
    pCompile->pDocMain->CompileDirectives(directives())
    
    dim as long n
    for i as long = lbound(directives) to ubound(directives)
        select case directives(i).DirectiveFlag 
            case IDM_CONSOLE
                n = instr(pCompile->CompileFlags, " -S GUI ")
                if n then pCompile->CompileFlags = AfxStrDelete(pCompile->CompileFlags, n, 8)
                pCompile->CompileFlags += " -s console "
            case IDM_GUI
                n = instr(pCompile->CompileFlags, " -S CONSOLE ")
                if n then pCompile->CompileFlags = AfxStrDelete(pCompile->CompileFlags, n, 12)
                pCompile->CompileFlags += " -s gui "
            case IDM_RESOURCE
                pCompile->ResourceFile = directives(i).DirectiveText
            case IDM_LINKMODULES
                pCompile->LinkModules += " " & directives(i).DirectiveText
        end select
    next
    ' Add paths to any found Link Modules in the code base.
    code_AddPathsToLinkModules(pCompile)

    return true
end function


' ========================================================================================
' if this is a project then we need to compile all of the modules first
' in order to create the necessary *.o object files
' ========================================================================================
' Does the workspace contain anything that compiles to its own object file? Used to decide
' whether the .tiko\ object folder is worth creating -- without this, building a single
' loose .bas would leave an empty .tiko\ directory sitting beside the user's file.
private function code_HasModuleDocument() as boolean
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if pDoc->ProjectFileType = FILETYPE_MODULE then return true
        pDoc = pDoc->pDocNext
    loop
    return false
end function


function code_CompileModules( byval pCompile as COMPILE_TYPE ptr ) as boolean

    ' No modules, nothing to pre-compile. The walk below would find nothing anyway; this
    ' just skips the debug/release object-invalidation bookkeeping with it.
    if code_HasModuleDocument() = false then return true

    dim as DWSTRING wDQ = wchr(34)
    
    ' Determine if we are switching from Release builds to Debug builds. This means
    ' that any existing obj's would be invalid and will be forced to rebuild.
    ' Look for the "_debug_active.txt" filename.
    dim as DWSTRING wszDebugFilename = pCompile->ObjFolder & "_debug_active.txt"
    dim as boolean bForceRebuild = false
    dim as boolean bDebugFileExists = AfxFileExists( wszDebugFilename )

    ' if only compiling this one module (null if all)
    dim pDocModule as clsDocument ptr
    if pCompile->CompileID = IDM_COMPILEMODULE then
        pDocModule = gTTabCtl.GetActiveDocumentPtr()
        bForceRebuild = true   ' always force a rebuild of a single Module.
    end if
    
    if pCompile->CompileID = IDM_DEBUG_STARTDEBUGGING then
        ' Debug build
        bForceRebuild = iif( bDebugFileExists, false, true )
        ' Create the debug active file for later checks
        dim pStream as CTextStream   '(utf16)
        if pStream.Create( wszDebugFilename, true, true ) = S_OK then 
            pStream.WriteLine "-- DEBUG OBJECT FILES --"
        end if
        pStream.Close
    else
        ' Release builds
        if bDebugFileExists then bForceRebuild = true
        ' Remove the existing debug check filename
        AfxDeleteFile( wszDebugFilename )
    end if

    pCompile->CompileFlags = " " & pCompile->CompileFlags & " "
    dim as string g_flag   = iif(instr(pCompile->CompileFlags, " -g "), " -g -nostrip ", "")
    dim as string e_flag   = iif(instr(pCompile->CompileFlags, " -e "), " -e ", "")
    dim as string ex_flag  = iif(instr(pCompile->CompileFlags, " -ex "), " -ex ", "")
    dim as string exx_flag = iif(instr(pCompile->CompileFlags, " -exx "), " -exx ", "")
    
    dim as long nFileCount   ' used to calculate when to fire an AfxDoEvents
    
    dim as boolean bOneModuleOnly = iif( pDocModule <> null, true, false )
    dim as boolean bBypass = false
    
    dim as clsDocument ptr pDoc = gApp.pDocList
    do until pDoc = null
        if pDoc->ProjectFileType = FILETYPE_MODULE then
            
            bBypass = false
            if (bOneModuleOnly = true) andalso (pDoc <> pDocModule) then
                bBypass = true
            end if

            if bBypass = false then
                ' Get the base name of the file for constructing the object filename
                pCompile->ObjectFilename = _
                    pCompile->ObjFolderShort & AfxStrPathname("NAME", pDoc->DiskFilename) & pCompile->ObjID

                ' Compare the source code file datetime to the object. if the source code
                ' date time is greater then we need to recompile it, otherwise we will simply
                ' link to the existing object file.
                dim as boolean bRebuildModule = false
               
                dim as FILETIME ft1 = AfxGetFileLastWriteTime(pDoc->DiskFilename) ' source file
                dim as FILETIME ft2 = AfxGetFileLastWriteTime(pCompile->ObjectFilename)  ' object file

                if (AfxFileTimeToVariantTime(ft1) > AfxFileTimeToVariantTime(ft2)) then bRebuildModule = true
                
                ' If rebuild is false then we still need to check any embedded #include files that
                ' may have changed since the document was parsed.
                dim pDocInclude as clsDocument ptr = pDoc->pDocIncludeList
                do until pDocInclude = null
                    ft1 = AfxGetFileLastWriteTime(pDocInclude->DiskFilename) ' current include file datetime
                    if AfxFileTimeToVariantTime(ft1) > AfxFileTimeToVariantTime(pDocInclude->DateFileTime) then 
                        bRebuildModule = true
                    end if    
                    pDocInclude->DateFileTime = ft1
                    pDocInclude = pDocInclude->pDocNext
                loop
                
                if (bRebuildModule = true) orelse _
                        (bForceRebuild = true) orelse _
                        (pCompile->CompileID = IDM_REBUILDALL) then
                    
                    nFileCount = nFileCount + 1
                    if (nFileCount mod 5) = 0 then AfxDoEvents
                    
                    ' Prepend with # so that spinner shows
                    dim as DWSTRING wst = L(166, "Compiling") + "... " + AfxStrPathname("NAMEX", pDoc->DiskFilename) 
                    SetCompileStatusBarMessage( wst, 0 )
                    
                    dim as DWSTRING wszCommand = pCompile->CompilerPath
                    dim as DWSTRING wszParams = _
                            g_flag & e_flag & ex_flag & exx_flag &  _
                            pCompile->CompileFlags + " " + _
                            " -c -b " + wDQ + pDoc->DiskFilename + wDQ + _
                            " -v -o " + wDQ + pCompile->ObjectFilename + wDQ 
                            
                    dim as string sConsoleText
                    RedirConsoleToFile( wszCommand, wszParams, sConsoleText )
                    
                    pCompile->wszFullCommandLine = wszCommand & " " & wszParams
                    pCompile->wszFullLogFile = sConsoleText
                    
                    ' If creating just a single compiled Module then obviously there is no linking step so the deeper dive
                    ' to find the true OutputFilename never happens and will still be set to the default for the project
                    ' which is most likely an EXE based on the Main file name.
                    if bOneModuleOnly then
                        pCompile->OutputFilename = pCompile->ObjectFilename
                    end if

                    ' Need to check to see if any errors occurred while compiling this object file. 
                    ' If yes, then stop processing object files and display the error. Allow a "success" 
                    ' message to be generated if we are just compiling one Module.
                    if ParseLogForError( pCompile, sConsoleText, bOneModuleOnly, true ) = true then
                        return false
                    else
                        ' If no error but we are only compiling one Module then we will also stop
                        ' here, but also set the StatusBar message to display the details of the 
                        ' Module that was just compiled.
                        if bOneModuleOnly then
                            return false    
                        end if
                    end if
                end if
            end if
        
        end if
        pDoc = pDoc->pDocNext
    loop

    return true
end function


' ========================================================================================
' Main module that handles the entire compile process
' ========================================================================================
function code_Compile( byval wID as long ) as boolean

    dim as DWSTRING wDQ = wchr(34) ' unicode double quotes

    ' Initialize the compile structure and return a pointer to be
    ' used throughout the remainder of the compiling process.
    dim pCompile as COMPILE_TYPE ptr = code_Initialize()
    if pCompile = null then
        code_Cleanup( pCompile )
        return false
    end if    
    
    
    ' Can only continue to compile if the primary source code file
    ' is not dirty and needs to be saved.
    if wID <> IDM_QUICKRUN then
        if gConfig.CompileAutosave then
            if OnCommand_FileSaveAll(HWND_FRMMAIN) then 
                code_Cleanup( pCompile )
                return false  ' user cancelled save
            end if   
        end if
    end if
    
    
    ' Gather information on the currently selected compiler path.
    if code_SetCompilerPath( pCompile ) = false then
        code_Cleanup( pCompile )
        return false        
    end if
        

    ' Set some compile flags depending on the type of compile requested.
    pCompile->CompileID = wID
    select case pCompile->CompileID
        case IDM_BUILDEXECUTE: pCompile->RunAfterCompile = true
        case IDM_QUICKRUN:     pCompile->RunAfterCompile = true
        case IDM_COMPILE:      pCompile->RunAfterCompile = false
    End select
    
    
    ' Determine if a Main file exists. If it does not, then we have no file to
    ' compile so we will end the compile process.
    if code_SetMainFile( pCompile ) = false then
        code_Cleanup( pCompile )
        return false        
    end if

    
    ' Run any precompile tools
    code_PreCompileTools( pCompile )
    
    
    ' If debugging, then ensure that we use the GCC compiler rather than GAS and
    ' also include the -g compile flag.
    if pCompile->CompileID = IDM_DEBUG_STARTDEBUGGING then
        ' Remove any -O optimization flags and add no inline flag.
        pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -Wc" )
        pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -O1" )
        pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -O2" )
        pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -O3" )
        pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -Os" )
        pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -Ofast" )
        ' Force a GAS backend. This used to force gcc, because gdb cannot read what the
        ' gas backends emit -- fbc's changelog describes that data as "for FBdebugger
        ' only". The cost was debugging a DIFFERENT binary from the one being built, with
        ' multidimensional arrays collapsed to one dimension and line numbers that did not
        ' correspond well. tiko now reads the format directly (debugParser.dll), so the
        ' debug build stays on the backend the project actually uses.
        dim as DWSTRING wszWantGen = iif( pCompile->bCompile32, wstr(" -gen gas"), wstr(" -gen gas64") )
        if instr( pCompile->CompileFlags, wszWantGen ) = 0 then
            ' Strip whatever backend was selected, then append the one we need. Done by
            ' removal rather than same-length substitution because " -gen gcc" and
            ' " -gen gas64" are different lengths and the old padding trick does not
            ' generalise in this direction.
            pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -gen gas64" )
            pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -gen gas" )
            pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -gen gcc" )
            pCompile->CompileFlags = AfxStrRemove( pCompile->CompileFlags, " -gen llvm" )
            pCompile->CompileFlags += wszWantGen
        end if
        pCompile->CompileFlags += " -g -exx -nostrip "
    end if


    ' Process any user specified Include Paths (per Options). These are added
    ' to CompileFlags and passed to the compiler using the -i directive.
    dim as DWSTRING includePath
    dim as long numpaths = AfxStrParseCount(gConfig.CompilerIncludes, ";")
    for i as long = 1 to numpaths
        includePath = rtrim(AfxStrParse(gConfig.CompilerIncludes, i, ";"))
        if len(includePath) then 
            pCompile->CompileIncludes += " -i " + wDQ + includePath + wDQ
        end if
    next


    ' Continue to generate info about specific paths related to the compile depending on whether
    ' a project is active. Resource file and Object path.
    ' Continue to build the CompileFlags that will be passed to the compiler.
    dim pDocResource as clsDocument ptr = gApp.GetResourceDocumentPtr()
    if pDocResource then pCompile->ResourceFile = pDocResource->DiskFilename

    pCompile->ObjFolder = pCompile->MainFolder & APPEXTENSION & "\"
    pCompile->ObjFolderShort = ".\" & APPEXTENSION & "\"

    ' The workspace's own switches. gConfig.CompilerSwitches is only the SEED for a new
    ' workspace now, applied in Workspace_EstablishUntitled, not a second store read here.
    if pCompile->bCompile32 then
        pCompile->ObjID = "32.o"
        pCompile->CompileFlags += " " + gApp.ProjectOther32 + " " + pCompile->CompileIncludes
    elseif pCompile->bCompile64 then
        pCompile->ObjID = "64.o"
        pCompile->CompileFlags += " " + gApp.ProjectOther64 + " " + pCompile->CompileIncludes
    end if

    ' Only create the object folder if something will actually be put in it. Gating this on
    ' "is a project open" used to serve that purpose; with every workspace a project, the
    ' honest test is whether there are module documents -- otherwise compiling one loose
    ' .bas creates an empty .tiko\ directory next to the user's file.
    if code_HasModuleDocument() then
        SHCreateDirectory( 0, pCompile->ObjFolder )
    end if


    ' Search main source code for any user embedded compile directives. These will override
    ' anything that was set at the default or project level.
    if code_SearchForDirectives( pCompile ) = false then
        code_Cleanup( pCompile )
        return false        
    end if


    ' Delete the quick run exe when Application ends.
    gApp.IsQuickRun = false
    if pCompile->CompileID = IDM_QUICKRUN then
        gApp.IsQuickRun = true
        gApp.wszQuickRunFilename = pCompile->OutputFilename
    end if    

        
    ' Need to test if the resulting application to be compiled is actually running
    ' in memory. This would cause the compile to fail.
    if IsProcessRunning( pCompile->OutputFilename ) then
        AfxDeleteFile( pCompile->OutputFilename )
        SetCompileStatusBarMessage( "", 0 )
        TikoMsgBox( HWND_FRMMAIN, L(163,"Program running") & "...", L(164,"Error"), _
                        TMB_ICON_INFO, TMB_OK )
        code_Cleanup( pCompile )
        return false        
    end if
        

    ' Finally, we can now start to do the compiling! 
    
    ' Indicate via mouse cursor to user to wait for compile process to end.
    SetCursor( LoadCursor(0, IDC_WAIT) )
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then SciExec( pDoc->hWindow(0), SCI_SETCURSOR, SC_CURSORWAIT, 0 )


    ' Change to the output code directory
    chdir pCompile->MainFolder
    
    
    ' Start our timer so we can track how long the compile took and display it in the results.
    pCompile->StartTime = timer
    

    ' if this is a project then we need to compile all of the modules first
    ' in order to create the necessary *.o object files. We also handle compiling
    ' just a single Module.
    if code_CompileModules( pCompile ) = false then
        code_Cleanup( pCompile )
        return false        
    end if

    
    ' Compile the Main file. if this is a project then we also need to link
    ' in all of the *.o object files.
    ' Per fxm post https://www.freebasic.net/forum/viewtopic.php?f=8&p=266126#p266126
    ' and https://www.freebasic.net/forum/viewtopic.php?f=3&t=28723&p=274431#p274431
    ' Do not specify file extension with -m parameter.
    ' So the main module file must be called twice in the command line:
    ' - after compile option '-m', but without specified extension,
    ' - and also like any module to compile, but there with its specified extension.
    ' 2026-02-16: Do not forcibly add a -x flag because an embedded #cmdline could have been used.
    dim as DWSTRING wszParams = "-m " + wDQ + pCompile->MainFilename + wDQ 

    if len(pCompile->LinkModules)  then wszParams += " " + pCompile->LinkModules
    if len(pCompile->ResourceFile) then wszParams += " " + wDQ + pCompile->ResourceFile + wDQ
    
    ' Ensure verbose compiler message output
    wszParams += " -v " + pCompile->CompileFlags 

    ' Add any .o module object files should they exist in the workspace. A workspace with
    ' no module documents appends nothing, so this needs no outer guard.
    dim as clsDocument ptr pDocMod = gApp.pDocList
    do until pDocMod = 0
        if pDocMod->ProjectFileType = FILETYPE_MODULE then
            ' Get the base name of the file for constructing the object filename
            wszParams += _
                " " + pCompile->ObjFolderShort + AfxStrPathname("NAME", pDocMod->DiskFilename) + pCompile->ObjID
        end if
        pDocMod = pDocMod->pDocNext
    loop
    

    ' Attempt to delete any existing output file so it will not exist should the compile fail.
    if AfxFileExists( pCompile->OutputFilename ) then
        AfxDeleteFile( pCompile->OutputFilename )
    end if


    ' Prepend with # so that spinner shows
    dim as DWSTRING wst = "#" + L(166, "Compiling") + "... " + AfxStrPathname("NAMEX", pCompile->MainFilename) 
    SetCompileStatusBarMessage( wst, 0 )
    SetCursor( LoadCursor(0, IDC_WAIT) )


    ' Do the compile in a separate thread so that the main thread GUI can display
    ' the spinner in the statusbar.
    dim as CompileThreadParams ctp
    ctp.pCompile     = pCompile
    ctp.wszExe       = pCompile->CompilerPath
    ctp.wszCmdLine   = wszParams
    ctp.sConsoleText = ""

    pCompile->bCompileThreadComplete = false
    dim as any ptr pThread = ThreadCreate( @compile_thread, @ctp )
    
    do until pCompile->bCompileThreadComplete = true
        AfxDoEvents()
    loop    
    
    pCompile->wszFullCommandLine = pCompile->CompilerPath & " " & wszParams
    pCompile->wszFullLogFile = ctp.sConsoleText
    dim as string sConsoleText = ctp.sConsoleText
                

    ' if this was a QuickRun then delete the temporary code file that was created.
    if pCompile->CompileID = IDM_QUICKRUN then 
        AfxDeleteFile( pCompile->MainFilename )
    end if    
    

    ' if pCompile->OutputFilename is a relative filename then convert it to full filename.
    pCompile->OutputFilename = AfxStrReplace( pCompile->OutputFilename, "/", "\" )
    if AfxPathIsRelative( pCompile->OutputFilename ) then 
        pCompile->OutputFilename = AfxPathCombine( pCompile->MainFolder, pCompile->OutputFilename )
    end if

    ' Check our compiler logfile to determine if any errors exist.
    if ParseLogForError( pCompile, sConsoleText, true, false ) = true then 
        ' Error was found. Editor has now been set to the error position. Nothing
        ' more we can do now but exit out. 
    else
        ' Run any postcompile tools
        code_PostCompileTools( pCompile )
        
        if pCompile->CompileID = IDM_DEBUG_STARTDEBUGGING then
           frmDebug_Show( _
                HWND_FRMMAIN, _
                pCompile->OutputFilename, _
                gApp.ProjectCommandLine _
                )
        end if
        
        if pCompile->RunAfterCompile then 
            RunEXE( _
                pCompile->pDocMain, _
                pCompile->OutputFilename,    _
                gApp.ProjectCommandLine _
                )
        end if
    
    end if

    code_Cleanup( pCompile )
    
    return true   ' successful
end function


