'' ========================================================================================
'' shellhost -- tikoshell's half of the app-host seam. THE SECOND IMPLEMENTATION.
''
'' src/modAppHostWin32.inc is the first, and until now the only one, which meant the record
'' was a seam in name: nothing proved it could be filled by anything but AfxNova. This file
'' is what makes it real.
''
'' ---- THE TWO KINDS OF FIELD, AND WHY THE SPLIT EARNED ITS KEEP -------------------------
''
'' AppHostServices asks a question or does something the caller depends on. Every field here
'' is implemented for real, because a stub would be a lie the caller acts on.
''
'' AppHostNotify is fire-and-forget. Several of those ARE stubs here, and legitimately so:
'' this binary has no TODO pane, no Explorer and no MRU lists, so "refresh the TODO pane" has
'' nothing to refresh and an empty body is the CORRECT implementation, not a shortcut. That
'' is precisely the distinction 01c6c80fe split the record to express, and this file is the
'' first host to depend on it.
''
'' ---- WHERE A STUB WOULD BE A BUG, IT IS LOUD --------------------------------------------
''
'' Two things the document model can call have no meaning here yet -- saving a project, and
'' the TODO store. They print and refuse rather than returning success, because a silent
'' "yes" from a save path is how a file goes missing. Nothing in this binary calls either
'' today; the bodies exist because the LINKER needs them, and that is worth saying out loud.
'' ========================================================================================

#pragma once

'' ---------------------------------------------------------------------------------------
'' The two bodies the linker demands and this binary has no feature for.
''
'' clsConfig is SPLIT -- header and constructor in app\, the rest in src\clsConfig.inc since
'' 7c step 1 -- so the shell has to supply this method itself. TodoStore_RemoveFile is
'' declared in app\clsSymbolDb.bi and bodied in the shell's symbol database.
function clsConfig.ProjectSaveToFile() as boolean
    print "tikoshell: ProjectSaveToFile -- this binary has no project system yet."
    return false
end function

sub TodoStore_RemoveFile( byval wszFile as DWSTRING )
    '' Deliberately silent: the document model calls this on every close, and a binary with
    '' no TODO store has nothing to remove. Unlike the one above, doing nothing here IS the
    '' right answer -- there is no caller waiting on a result.
end sub


'' ---------------------------------------------------------------------------------------
'' SERVICES. Every one is real.
'' ---------------------------------------------------------------------------------------
private function ShellHost_CreateView( byval nIndex as long ) as any ptr
    '' SciMsg IS BOUND HERE, and it MUST be -- this is not symmetry with tiko for its own
    '' sake. The document model calls SciMsg at 374 sites; it is a function POINTER, and in
    '' this binary nothing else ever set it. Loading a real file segfaulted on the first
    '' attempt for exactly that reason: a null pointer called with four arguments.
    ''
    '' tiko binds it in ITS CreateView (modAppHostWin32.inc), which is where the
    '' responsibility moved in commit 3e -- the document model used to do it and had to name
    '' @SciPs_Send to manage it. Every host that hands back a view owes this line.
    if SciMsg = 0 then
        SciMsg = cast( Scintilla_Directfunction, @SciPs_Send )
    end if

    '' The shell owns exactly two views today -- the editor and its split -- and they are
    '' built by the layout, not on demand. So this hands back the existing one rather than
    '' creating a third that nothing would ever lay out.
    ''
    '' A DELIBERATE DIFFERENCE FROM tiko, which creates a window per document per view. The
    '' shell has one document, and commit 6 is where that stops being true.
    if nIndex = 0 then return g_view
    return g_view2
end function

private sub ShellHost_DestroyView( byval pView as any ptr )
    '' The views belong to the widget tree and are freed with it. A document letting go of
    '' one must not take it down -- exactly the ownership rule PsSurfaceDetachRoot exists for.
end sub

private function ShellHost_IsViewAlive( byval pView as any ptr ) as boolean
    return (pView <> 0)
end function

private function ShellHost_ViewSciPointer( byval pView as any ptr ) as any ptr
    if pView = 0 then return 0
    return cast( PsSciView ptr, pView )->pSci
end function

private sub ShellHost_InvalidateView( byval pView as any ptr )
    if pView = 0 then exit sub
    cast( PsSciView ptr, pView )->Invalidate()
end sub

private sub ShellHost_SetViewRedraw( byval pView as any ptr, byval bOn as boolean )
    '' A NO-OP, AND CORRECTLY SO. PsSurface repaints from a damage region once per frame
    '' rather than per control, so there is no per-view redraw to suspend. app/modAppHost.bi
    '' anticipates exactly this and says the shell decides whether an equivalent is needed.
end sub

private sub ShellHost_ViewScale( byval pView as any ptr, byref rx as single, byref ry as single )
    rx = 1.0 : ry = 1.0
    if g_pSurf = 0 then exit sub
    rx = g_pSurf->fScale
    ry = g_pSurf->fScale
end sub

private function ShellHost_ViewHeight( byval pView as any ptr ) as long
    if pView = 0 then return 0
    return cast( PsSciView ptr, pView )->bounds.h
end function

private function ShellHost_ViewUiFontName( byval pView as any ptr ) as string
    '' The shell's widget font, which is what the autocomplete popup should match.
    return g_sFont
end function

private function ShellHost_ViewUiFontSize( byval pView as any ptr ) as long
    return g_nFontPx
end function

private sub ShellHost_StyleView( byval pSciView as any ptr, byval pOwnerView as any ptr, _
                                 byval nEncoding as long, byval sDiskFilename as DWSTRING, _
                                 byval bIsNew as boolean )
    '' tiko paints 401 lines of theme here. THE SHELL ALREADY DOES ITS OWN STYLING, in
    '' StyleOneView, from PsPlatform's theme rather than tiko's COLORREF record -- which is
    '' the whole reason ApplyPropertiesToView was lifted out of the document model in
    '' ef358f3f7. Routing to it keeps one styler per binary instead of two.
    if pSciView = 0 then exit sub
    if g_pSurf = 0 then exit sub
    StyleOneView( cast( PsSciView ptr, pSciView ), *g_pSurf )
end sub

private function ShellHost_ThemeSelectionBack() as ulong
    return cast( ulong, PsThemeRoleColor( PSTHEME_SELECTION ) )
end function

private function ShellHost_LastErrorText() as DWSTRING
    '' "" is a legitimate answer -- app/modEncoding.inc falls back to a fixed message when a
    '' host has no platform detail to add. PsCore surfaces no errno equivalent today.
    return ""
end function

private function ShellHost_LoadFileText( byval wszPath as DWSTRING, byref txtBuffer as string, _
                                         byval pDoc as clsDocument ptr ) as boolean
    '' PsFileReadAll plus PsCore's decoder, where tiko uses CreateFileW and
    '' WideCharToMultiByte. The ENCODING IS NOT DETECTED HERE YET -- see the not-verified
    '' notes in the commit: this reads bytes and hands them over as UTF-8.
    dim as boolean bOk = false
    dim as string sRaw = PsFileReadAll( wszPath, bOk )
    if bOk = false then return false
    txtBuffer = sRaw
    return true
end function

private function ShellHost_ResolveIncludePath( byval pDoc as clsDocument ptr, _
                                               byval sFilename as string ) as string
    '' tiko resolves against the active build configuration's compiler paths. This binary has
    '' no build configurations, so an include resolves to itself.
    return sFilename
end function

private function ShellHost_ConfirmLossySave( byval pDoc as clsDocument ptr, _
                                             byval wszPath as DWSTRING, _
                                             byval nEncoding as long ) as boolean
    '' REFUSES, and that is the safe answer rather than the convenient one. Saying yes would
    '' discard characters the user never agreed to lose. A real box goes here when the shell
    '' saves for real -- commit 7.
    print "tikoshell: refusing a lossy save of " & wszPath.Utf8 & " (no prompt yet)"
    return false
end function

private sub ShellHost_ReportWriteFailure( byval wszPath as DWSTRING, byval wszErr as DWSTRING )
    print "tikoshell: could not write " & wszPath.Utf8 & " -- " & wszErr.Utf8
end sub

private function ShellHost_AskOpenPath( byref sOut as DWSTRING ) as boolean
    return ShellAskPath( false, sOut )
end function

private function ShellHost_AskSavePath( byref sPath as DWSTRING, byref sExt as DWSTRING ) as boolean
    return ShellAskPath( true, sPath )
end function

private function ShellHost_IsFindVisible() as boolean
    '' The Find bar is a stub in this binary and is never up.
    return false
end function


'' ---------------------------------------------------------------------------------------
'' NOTIFICATIONS. Several are empty, and the record's own header explains why that is
'' correct rather than lazy: this binary has no TODO pane, no Explorer and no MRU lists.
'' ---------------------------------------------------------------------------------------
private sub ShellHost_CloseTab( byval nTabIdx as long )
    print "tikoshell: close tab " & nTabIdx & " (no tab model yet -- commit 6)"
end sub

private sub ShellHost_OnDocumentClosing( byval pDoc as clsDocument ptr )
end sub

private sub ShellHost_OnDocumentSaved( byval pDoc as clsDocument ptr )
end sub

private sub ShellHost_RequestBufferScan( byval pDoc as clsDocument ptr )
    '' No background scanner here. docs/port/document-model-blockers.md records why
    '' clsScanMgr stays in tiko: its worker thread is woken with Win32 event objects and
    '' PsPlatform has no threading service at all.
end sub

private function ShellHost_DocRootName( byval pDoc as clsDocument ptr ) as DWSTRING
    if pDoc = 0 then return ""
    return pDoc->DiskFilename
end function

private sub ShellHost_UpdateMruFile( byval wszPath as DWSTRING )
end sub

private sub ShellHost_UpdateMruProject( byval wszPath as DWSTRING )
end sub

private sub ShellHost_ReloadExplorer()
end sub

private sub ShellHost_RelayoutMain()
    if g_pSurf = 0 then exit sub
    LayoutAll( *g_pSurf )
    g_pSurf->InvalidateAll()
end sub

private sub ShellHost_RelayoutTopTabs()
    ShellHost_RelayoutMain()
end sub

private sub ShellHost_RefreshTodoList()
end sub


'' Called once at startup, before anything can open a document.
sub ShellHost_Install()
    gAppHost.CreateView         = @ShellHost_CreateView
    gAppHost.DestroyView        = @ShellHost_DestroyView
    gAppHost.IsViewAlive        = @ShellHost_IsViewAlive
    gAppHost.ViewSciPointer     = @ShellHost_ViewSciPointer
    gAppHost.InvalidateView     = @ShellHost_InvalidateView
    gAppHost.SetViewRedraw      = @ShellHost_SetViewRedraw
    gAppHost.ViewScale          = @ShellHost_ViewScale
    gAppHost.ViewHeight         = @ShellHost_ViewHeight
    gAppHost.ViewUiFontName     = @ShellHost_ViewUiFontName
    gAppHost.ViewUiFontSize     = @ShellHost_ViewUiFontSize
    gAppHost.StyleView          = @ShellHost_StyleView
    gAppHost.ThemeSelectionBack = @ShellHost_ThemeSelectionBack
    gAppHost.LastErrorText      = @ShellHost_LastErrorText
    gAppHost.LoadFileText       = @ShellHost_LoadFileText
    gAppHost.ResolveIncludePath = @ShellHost_ResolveIncludePath
    gAppHost.ConfirmLossySave   = @ShellHost_ConfirmLossySave
    gAppHost.ReportWriteFailure = @ShellHost_ReportWriteFailure
    gAppHost.AskOpenPath        = @ShellHost_AskOpenPath
    gAppHost.AskSavePath        = @ShellHost_AskSavePath
    gAppHost.IsFindVisible      = @ShellHost_IsFindVisible

    gAppNotify.CloseTab          = @ShellHost_CloseTab
    gAppNotify.OnDocumentClosing = @ShellHost_OnDocumentClosing
    gAppNotify.OnDocumentSaved   = @ShellHost_OnDocumentSaved
    gAppNotify.RequestBufferScan = @ShellHost_RequestBufferScan
    gAppNotify.DocRootName       = @ShellHost_DocRootName
    gAppNotify.UpdateMruFile     = @ShellHost_UpdateMruFile
    gAppNotify.UpdateMruProject  = @ShellHost_UpdateMruProject
    gAppNotify.ReloadExplorer    = @ShellHost_ReloadExplorer
    gAppNotify.RelayoutMain      = @ShellHost_RelayoutMain
    gAppNotify.RelayoutTopTabs   = @ShellHost_RelayoutTopTabs
    gAppNotify.RefreshTodoList   = @ShellHost_RefreshTodoList
end sub
