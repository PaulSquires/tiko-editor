'' ========================================================================================
'' modAppHostWin32 -- tiko's half of the app-host seam. Shell-side, forever.
''
'' Fills app/modAppHost.bi's record with the AfxNova and Win32 bodies clsDocument and
'' clsTopTabCtl use today. Nothing here is new behaviour: each body is the code that was
'' inline at the call site, moved behind a function pointer so the caller can stop naming
'' Win32. tikoshell fills the same record with PsPlatform.
''
'' THAT IS WHAT MAKES THE CONVERSION CHECKABLE. The same code still runs, reached through one
'' indirection, so the 27-suite oracle is comparing the port against itself rather than
'' against a rewrite.
'' ========================================================================================

#pragma once

private function AppHostW32_CreateView( byval nIndex as long ) as any ptr
    '' PHASE 7d: a tikoSciHost, not a Scintilla window -- same style, same parent, same
    '' control id, and behind it a PsSurface holding a PsSciView. See frmSciHost.bi.
    return SciHost_Create( HWND_FRMMAIN, IDC_SCINTILLA + nIndex )
end function

private sub AppHostW32_DestroyView( byval pView as any ptr )
    if pView = 0 then exit sub
    DestroyWindow( cast(HWND, pView) )
end sub

private function AppHostW32_IsViewAlive( byval pView as any ptr ) as boolean
    if pView = 0 then return false
    return (IsWindow( cast(HWND, pView) ) <> 0)
end function

private sub AppHostW32_InvalidateView( byval pView as any ptr )
    if pView = 0 then exit sub
    AfxRedrawWindow( cast(HWND, pView) )
end sub

private sub AppHostW32_ViewScale( byval pView as any ptr, _
                                  byref rx as single, byref ry as single )
    '' Defaults first: a view parented somewhere without a CWindow answers 1.0 rather than
    '' leaving the caller's variables untouched, which is what the inline code did.
    rx = 1.0 : ry = 1.0
    if pView = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowOwnerPtr( cast(HWND, pView) )
    if pWindow then
        rx = pWindow->rxRatio
        ry = pWindow->ryRatio
    end if
end sub

private function AppHostW32_ViewHeight( byval pView as any ptr ) as long
    if pView = 0 then return 0
    '' The rect is in SCREEN coordinates and stays that way. The inline version mapped it
    '' into frmMain's client area first and then used only (bottom - top) -- and a
    '' translation cannot change a height, so the mapping never affected the result.
    dim as RECT rc = AfxGetWindowRect( cast(HWND, pView) )
    return rc.bottom - rc.top
end function

'' ---- THE TWO BODIES NO SUITE REACHES --------------------------------------------------
''
'' No self-test raises a file dialog, so these two are the only members of this record whose
'' behaviour is verified by a person and by nothing else. Confirmed working by the author on
'' 2026-08-10: Ctrl+I inserts a chosen file at the caret, and File > Save As saves.
''
'' KEEP THAT IN MIND BEFORE EDITING EITHER. The first versions of both had defects the
'' compiler was happy with -- a dropped CoTaskMemFree, and @aDWSTRING passed where a
'' wstring ptr was wanted, which is a WARNING and not an error. Neither would have been
'' caught by any gate in this repo.
''
'' BOTH DIALOGS RETURN COM-ALLOCATED MEMORY AND BOTH CALLERS FREE IT. The wrapper owns that
'' now, and it is the one thing here that is easy to lose in the move: the call sites this
'' replaces each ended with CoTaskMemFree, and a wrapper that returned the string without
'' freeing would leak one path per dialog with nothing to show for it.
private function AppHostW32_ViewUiFontName( byval pView as any ptr ) as string
    if pView = 0 then return ""
    dim pWindow as CWindow ptr = AfxCWindowOwnerPtr( cast(HWND, pView) )
    if pWindow = 0 then return ""
    return pWindow->DefaultFontName
end function

private function AppHostW32_ViewUiFontSize( byval pView as any ptr ) as long
    if pView = 0 then return 0
    dim pWindow as CWindow ptr = AfxCWindowOwnerPtr( cast(HWND, pView) )
    if pWindow = 0 then return 0
    return pWindow->DefaultFontSize
end function

private function AppHostW32_AskOpenPath( byref sOut as DWSTRING ) as boolean
    dim pwszName as wstring ptr = AfxIFileOpenDialogW( HWND_FRMMAIN, IDM_INSERTFILE )
    if pwszName = 0 then return false
    sOut = *pwszName
    CoTaskMemFree pwszName
    return true
end function

private function AppHostW32_AskSavePath( byref sPath as DWSTRING, _
                                         byref sExt as DWSTRING ) as boolean
    '' wstring * MAX_PATH, not DWSTRING: AfxIFileSaveDialog takes wstring ptr for both, and
    '' passing @aDWSTRING compiles with only a "different pointer types" WARNING -- which is
    '' the shape of mistake that reaches a release.
    dim wszFilename  as wstring * MAX_PATH
    dim wszExtension as wstring * MAX_PATH
    wszFilename  = *sPath.Wz()
    wszExtension = *sExt.Wz()

    dim pwszName as wstring ptr = AfxIFileSaveDialog( HWND_FRMMAIN, _
                                                      @wszFilename, @wszExtension, _
                                                      IDM_FILESAVEAS )
    if pwszName = 0 then return false
    sPath = *pwszName
    CoTaskMemFree pwszName
    return true
end function

private function AppHostW32_IsFindVisible() as boolean
    return (IsWindowVisible( HWND_FRMMAIN_FIND ) <> 0)
end function

private sub AppHostW32_CloseTab( byval nTabIdx as long )
    OnCommand_FileClose( HWND_FRMMAIN, EFC_CLOSECURRENT, nTabIdx )
end sub


'' Called once from WinMain, before anything can open a document.
sub AppHostW32_Install()
    gAppHost.CreateView     = @AppHostW32_CreateView
    gAppHost.DestroyView    = @AppHostW32_DestroyView
    gAppHost.IsViewAlive    = @AppHostW32_IsViewAlive
    gAppHost.InvalidateView = @AppHostW32_InvalidateView
    gAppHost.ViewScale      = @AppHostW32_ViewScale
    gAppHost.ViewHeight     = @AppHostW32_ViewHeight
    gAppHost.ViewUiFontName = @AppHostW32_ViewUiFontName
    gAppHost.ViewUiFontSize = @AppHostW32_ViewUiFontSize
    gAppHost.AskOpenPath    = @AppHostW32_AskOpenPath
    gAppHost.AskSavePath    = @AppHostW32_AskSavePath
    gAppHost.IsFindVisible  = @AppHostW32_IsFindVisible
    gAppHost.CloseTab       = @AppHostW32_CloseTab
end sub
