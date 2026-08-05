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

' ========================================================================================
' The Help Center window. See frmHelpCenter.bi for the design; this file holds the
' implementation, the two WebView2 event handlers, and the env-gated self-test.
' ========================================================================================


' ########################################################################################
' NavigationCompleted: applies the query that was waiting for the first page load.
'
' The very first _Show has to navigate before there is a DOM to script, so the query is
' parked in gHelpCenter.wszPendingQuery and applied here. Every LATER _Show scripts the
' page directly and never comes through this path -- the search box is on every page of
' the site, so there is nothing to wait for.
' ########################################################################################
type HELPCENTER_NAVCOMPLETED extends CWebView2NavigationCompletedEventHandler
    declare function Invoke( byval sender as Afx_ICoreWebView2 ptr, _
                             byval args as Afx_ICoreWebView2NavigationCompletedEventArgs ptr _
                             ) as HRESULT
    declare constructor( byval pWebView2 as CWebView2 ptr )
    declare destructor
    as CWebView2 ptr           m_pWebView2
    as EventRegistrationToken  m_token
end type

constructor HELPCENTER_NAVCOMPLETED( byval pWebView2 as CWebView2 ptr )
    if pWebView2 then
        m_pWebView2 = pWebView2
        pWebView2->AddNavigationCompleted( cast(any ptr, @this), @m_token )
    end if
end constructor

destructor HELPCENTER_NAVCOMPLETED
    if m_pWebView2 then m_pWebView2->RemoveNavigationCompleted( m_token )
end destructor

function HELPCENTER_NAVCOMPLETED.Invoke( byval sender as Afx_ICoreWebView2 ptr, _
                                         byval args as Afx_ICoreWebView2NavigationCompletedEventArgs ptr _
                                         ) as HRESULT
    gHelpCenter.bLoaded = true
    ' Focus the site's own search box, carrying the pending query if there is one. The
    ' query is consumed here so a later in-site navigation (the user clicking a topic)
    ' does not silently re-run a search they have moved on from.
    frmHelpCenter_ApplySearch( gHelpCenter.wszPendingQuery )
    gHelpCenter.wszPendingQuery = ""
    function = S_OK
end function


' ########################################################################################
' NewWindowRequested: sends the URI to the DEFAULT BROWSER, never into the pane.
'
' THIS IS LOAD-BEARING, NOT POLISH. The ~1,838 winapi\*\procedures\*.html pages in the
' generated site are link-out launchers that call window.open( learn.microsoft.com ) ON
' LOAD, and Win32 search hits carry target="_blank" for the same reason. Without this
' handler the pane navigates itself off the local docs and onto Microsoft Learn.
'
' Deliberately NOT CWebView2NewWindowRequestedEventHandlerInternal( p, FALSE ): that one
' redirects the popup INTO the same view, which is exactly the failure above.
' ########################################################################################
type HELPCENTER_NEWWINDOW extends CWebView2NewWindowRequestedEventHandler
    declare function Invoke( byval sender as Afx_ICoreWebView2 ptr, _
                             byval args as Afx_ICoreWebView2NewWindowRequestedEventArgs ptr _
                             ) as HRESULT
    declare constructor( byval pWebView2 as CWebView2 ptr )
    declare destructor
    as CWebView2 ptr           m_pWebView2
    as EventRegistrationToken  m_token
end type

constructor HELPCENTER_NEWWINDOW( byval pWebView2 as CWebView2 ptr )
    if pWebView2 then
        m_pWebView2 = pWebView2
        pWebView2->AddNewWindowRequested( cast(any ptr, @this), @m_token )
    end if
end constructor

destructor HELPCENTER_NEWWINDOW
    if m_pWebView2 then m_pWebView2->RemoveNewWindowRequested( m_token )
end destructor

function HELPCENTER_NEWWINDOW.Invoke( byval sender as Afx_ICoreWebView2 ptr, _
                                      byval args as Afx_ICoreWebView2NewWindowRequestedEventArgs ptr _
                                      ) as HRESULT
    if args = 0 then return S_OK
    ' Claim it first: whatever happens below, the pane must not navigate.
    args->put_Handled( true )
    dim as wstring ptr pUri
    if SUCCEEDED( args->get_Uri( @pUri ) ) then
        if pUri then
            ShellExecuteW( 0, "open", pUri, 0, 0, SW_SHOWNORMAL )
            CoTaskMemFree( pUri )
        end if
    end if
    function = S_OK
end function


' ========================================================================================
' The folder a help site lives in, with a trailing backslash.
'
' BOTH SITES SIT UNDER settings\help, which is what makes that one folder the whole of
' tiko's documentation -- the FreeBASIC .chm the generator ingests is already there. The
' generated tree is gitignored and staged by _copy_helpcenter.bat; the tikohelp tree is
' hand written and tracked, so the two are not interchangeable however alike they look.
' ========================================================================================
function HelpCenter_RootFolder( byval nSite as long ) as DWSTRING
    if nSite = HELPCENTER_SITE_TIKO then
        function = AfxGetExePathName & "settings\help\tikohelp\"
    else
        function = AfxGetExePathName & "settings\help\helpcenter\"
    end if
end function


' The caption the window carries while a site is up. Also the caption of that site's
' "files not found" box, so the two cannot name different things.
function HelpCenter_SiteTitle( byval nSite as long ) as DWSTRING
    if nSite = HELPCENTER_SITE_TIKO then
        function = L(519,"Tiko Editor Help")
    else
        function = L(86,"Help Center")
    end if
end function


' ========================================================================================
' A Windows path as a file:// URL.
'
' Both conversions are needed rather than tidy: backslashes are not path separators in a
' URL, and the four encoded characters change how the URL PARSES rather than merely
' looking untidy -- a "#" truncates the path into a fragment and a " " ends the URL
' outright, both silently. "%" must be encoded FIRST or it re-encodes the escapes the
' later passes introduce.
'
' Split out from HelpCenter_IndexUrl so it can be asserted against paths this machine does
' not have. IndexUrl reads the live exe path, which here has nothing to encode -- an
' assertion driving only that would pass under an encoder that does nothing at all, and
' the failure would then only ever appear on someone else's machine.
' ========================================================================================
function HelpCenter_PathToFileUrl( byval wszPath as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWork = wszPath
    wszWork = AfxStrReplace( wszWork, "\", "/" )
    wszWork = AfxStrReplace( wszWork, "%", "%25" )    ' first, or it re-encodes the rest
    wszWork = AfxStrReplace( wszWork, " ", "%20" )
    wszWork = AfxStrReplace( wszWork, "#", "%23" )
    wszWork = AfxStrReplace( wszWork, "?", "%3F" )
    function = "file:///" & wszWork
end function

function HelpCenter_IndexUrl( byval nSite as long ) as DWSTRING
    function = HelpCenter_PathToFileUrl( HelpCenter_RootFolder( nSite ) & "index.html" )
end function


' ========================================================================================
' Reduce a Scintilla selection to something a search box can use: leading/trailing
' whitespace off, FIRST LINE ONLY, and capped at HELPCENTER_MAXQUERY characters.
'
' The first-line rule is the one that matters. A multi-line selection is a block of code,
' not an identifier, and pushing it into a one-line input gives the site's ranker nothing
' to work with while looking, on screen, as though the search simply failed.
' ========================================================================================
function HelpCenter_NormalizeQuery( byval wszText as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWork = wszText
    if PsLen( wszWork ) = 0 then
        function = ""
        exit function
    end if

    ' First line only. Handle CR, LF and CRLF by cutting at whichever arrives first.
    dim as long nCR = PsInStr( wszWork, wchr(13) )
    dim as long nLF = PsInStr( wszWork, wchr(10) )
    dim as long nCut = 0
    if (nCR > 0) andalso (nLF > 0) then
        nCut = iif( nCR < nLF, nCR, nLF )
    elseif nCR > 0 then
        nCut = nCR
    elseif nLF > 0 then
        nCut = nLF
    end if
    if nCut > 0 then wszWork = PsLeft( wszWork, nCut - 1 )

    ' A tab inside the remaining line is whitespace to a search box, not a separator.
    wszWork = AfxStrReplace( wszWork, wchr(9), " " )
    wszWork = PsTrim( wszWork )

    if PsLen( wszWork ) > HELPCENTER_MAXQUERY then
        wszWork = PsLeft( wszWork, HELPCENTER_MAXQUERY )
    end if

    function = wszWork
end function


' ========================================================================================
' Is a word-under-the-caret worth searching for?
'
' Only the FALLBACK is filtered, never a selection: selecting "=>" is an explicit request
' and we honour it, but the caret merely sitting somewhere is not. GetWord walks a run of
' non-word characters when the caret is on punctuation, so F1 on the "'" that opens a
' comment line -- or on an operator, or a lone digit -- would otherwise fire a search for
' that character and show "No matches", which reads as the feature being broken.
'
' The test is "contains at least one letter", not "is a valid identifier": FreeBASIC names
' can carry digits and underscores, and "#define" or "__FB_WIN32__" are legitimate things
' to look up. A pure digit run is not.
' ========================================================================================
function HelpCenter_IsSearchableWord( byval wszWord as DWSTRING ) as boolean
    dim as long nLen = PsLen( wszWord )
    if nLen = 0 then return false
    for i as long = 1 to nLen
        dim as ulong c = asc( PsMid( wszWord, i, 1 ) )
        dim as boolean bUpper = (c >= 65) andalso (c <= 90)
        dim as boolean bLower = (c >= 97) andalso (c <= 122)
        ' Anything above ASCII is treated as a letter: accented and CJK identifiers are
        ' letters to their author, and the alternative is refusing to look them up.
        dim as boolean bWide  = (c > 127)
        if bUpper orelse bLower orelse bWide then return true
    next
    function = false
end function


' ========================================================================================
' Escape arbitrary text so it can be pasted into a JavaScript double-quoted string literal.
'
' The input is a Scintilla selection, i.e. text the user chose and we did not write, so
' every one of these is reachable. The BACKSLASH MUST BE ESCAPED FIRST or it would escape
' the escapes added after it -- a selection containing a Windows path is the ordinary case
' that breaks a wrong ordering here.
'
' U+2028 / U+2029 are the non-obvious pair: they are legal inside a JSON string but are
' LINE TERMINATORS to a JavaScript parser, so an unescaped one is a syntax error in the
' injected script rather than a wrong result.
' ========================================================================================
function HelpCenter_JsQuote( byval wszText as DWSTRING ) as DWSTRING
    dim as DWSTRING wszWork = wszText
    wszWork = AfxStrReplace( wszWork, "\", "\\" )        ' first, always
    wszWork = AfxStrReplace( wszWork, """", "\" & """" )
    wszWork = AfxStrReplace( wszWork, wchr(13), "\r" )
    wszWork = AfxStrReplace( wszWork, wchr(10), "\n" )
    wszWork = AfxStrReplace( wszWork, wchr(9),  "\t" )
    wszWork = AfxStrReplace( wszWork, wchr(&h2028), "\u2028" )
    wszWork = AfxStrReplace( wszWork, wchr(&h2029), "\u2029" )
    function = """" & wszWork & """"
end function


' ========================================================================================
' Focus the site's search box, optionally filling it and running the search.
'
' Dispatching the 'input' event is what RUNS the search: the site's own render() is bound
' to that event, so there is no separate submit step and no Enter to synthesise. Setting
' .value alone would fill the box and show nothing.
'
' An empty query focuses the box and leaves it alone -- which is plain F1 with no
' selection, and is the same thing the site's own "/" shortcut does.
'
' FOCUS TAKES TWO STEPS AND BOTH ARE NEEDED. The JS focus() only moves the DOM's focus
' INSIDE the pane; if the pane itself does not hold the Win32 focus, the caret does not
' blink in the search box and typing goes to whatever window does. MoveFocus gives the
' pane Win32 focus, then the script puts the DOM caret in #q. Doing only the second is
' what made the first F1 look like it had done nothing.
' ========================================================================================
sub frmHelpCenter_ApplySearch( byval wszQuery as DWSTRING )
    if gHelpCenter.pWeb = 0 then exit sub

    ' The query is chosen in frmMainOnCommand from Scintilla state no assertion can reach
    ' -- the selection, or the word under the caret -- so this reports what actually
    ' arrived. TIKO_HELPCENTER_TRACE=1.
    if environ("TIKO_HELPCENTER_TRACE") = "1" then
        dim as string sQ = "  helpcenter: search=[" & wszQuery & "]"
        print sQ
    end if

    dim as DWSTRING wszJS = "(function(){var i=document.getElementById('q');if(!i)return;"

    if PsLen( wszQuery ) > 0 then
        wszJS = wszJS & "i.value=" & HelpCenter_JsQuote( wszQuery ) & ";" & _
                        "i.dispatchEvent(new Event('input',{bubbles:true}));"
    end if

    ' Focus last: filling the box moves nothing, and on the pending-query path the pane
    ' has only just finished loading.
    wszJS = wszJS & "i.focus();" & _
                    "try{i.setSelectionRange(i.value.length,i.value.length);}catch(e){}" & _
                    "})()"

    ' Win32 focus onto the pane FIRST -- see the two-step note above. Without it the
    ' script below moves the DOM caret into a control that is not receiving keystrokes.
    gHelpCenter.pWeb->MoveFocus( COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC )

    ' The completion handler is passed explicitly: the default only applies to a direct
    ' call, not through the pointer.
    gHelpCenter.pWeb->ExecuteScript( wszJS, NULL )
end sub


' ========================================================================================
' A theme-matched message box for the two failure paths. Shaped after
' frmOptions_ThemedMsgBox but owned by frmMain, since this window may not exist yet.
' ========================================================================================
private function frmHelpCenter_ThemedMsgBox( byval wszText as DWSTRING, _
                                             byval wszCaption as DWSTRING ) as long
    ' Owned by frmMain, not by HWND_FRMHELPCENTER -- both callers are failure paths that run
    ' while this window may not exist yet. GUIFONT_10 is kept: this box carries long sentences.
    return TikoMsgBox( HWND_FRMMAIN, wszText, wszCaption, _
                       TMB_ICON_WARNING, TMB_OK, 0, ghFont(GUIFONT_10) )
end function


' ========================================================================================
' Size the WebView host child to the whole client area, and the pane with it.
' ========================================================================================
private function frmHelpCenter_PositionWindows() as LRESULT
    if IsWindow( HWND_FRMHELPCENTER ) = 0 then return 0

    dim as RECT rc
    GetClientRect( HWND_FRMHELPCENTER, @rc )

    if gHelpCenter.hWebHost then
        SetWindowPos( gHelpCenter.hWebHost, 0, 0, 0, rc.Right, rc.Bottom, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
    end if

    ' The controller does not follow its host window's size on its own.
    if gHelpCenter.pWeb then
        dim as RECT rcHost
        rcHost.Left = 0 : rcHost.Top = 0
        rcHost.Right = rc.Right : rcHost.Bottom = rc.Bottom
        gHelpCenter.pWeb->SetBounds( rcHost )
    end if

    function = 0
end function


' ========================================================================================
' Refresh the persisted geometry from the live window.
'
' Called from the TOP of clsConfig.SaveConfigFile, not from the close path. Capturing on
' the way out only is the shape of the Output panel bug: every other save site then writes
' a stale rectangle, and a crash makes it permanent. Skipped while minimized, where the
' live rect is not the rect worth remembering.
' ========================================================================================
sub frmHelpCenter_CaptureState()
    if IsWindow( HWND_FRMHELPCENTER ) = 0 then exit sub
    if IsIconic( HWND_FRMHELPCENTER ) then exit sub

    dim WinPla as WINDOWPLACEMENT
    WinPla.Length = sizeof(WinPla)
    if GetWindowPlacement( HWND_FRMHELPCENTER, @WinPla ) = 0 then exit sub

    with gConfig
        .HelpStartupLeft      = WinPla.rcNormalPosition.Left
        .HelpStartupTop       = WinPla.rcNormalPosition.Top
        .HelpStartupRight     = WinPla.rcNormalPosition.Right
        .HelpStartupBottom    = WinPla.rcNormalPosition.Bottom
        .HelpStartupMaximized = iif( WinPla.showCmd = SW_MAXIMIZE, true, false )
    end with
end sub


' ========================================================================================
' Pump filter. See the block comment at the call site in frmMain.inc.
'
' Claims a message only when it is a KEYBOARD message AND its root ancestor is this
' window, then dispatches it itself. Without this, every keystroke typed into the Help
' Center's search box still runs through frmMain's three TranslateAccelerator calls and
' frmMain's IsDialogMessage -- so F1 re-enters the help command, Ctrl+S saves a document
' and Tab fires IDM_INDENTBLOCK.
'
' Mouse messages are deliberately NOT claimed: they have to keep reaching
' PsMenuBar_FilterMessage / ContextMenu_FilterMessage, or clicking on the Help Center
' would leave an open tiko menu on screen.
'
' Escape is not claimed either. The site uses it to dismiss its own results panel and blur
' the search box; the window closes with its X or Alt+F4.
' ========================================================================================
function frmHelpCenter_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if HWND_FRMHELPCENTER = 0 then return false

    dim as boolean bIsKey = false
    select case pMsg->message
        case WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_SYSCHAR
            bIsKey = true
    end select
    if bIsKey = false then return false

    if GetAncestor( pMsg->hwnd, GA_ROOT ) <> HWND_FRMHELPCENTER then return false

    TranslateMessage( pMsg )
    DispatchMessage( pMsg )
    function = true
end function


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmHelpCenter_WndProc( byval hwnd   as HWND, _
                                        byval uMsg   as UINT, _
                                        byval wParam as WPARAM, _
                                        byval lParam as LPARAM _
                                        ) as LRESULT

    select case uMsg
        case WM_SIZE
            frmHelpCenter_PositionWindows()
            return 0

        case WM_SETFOCUS
            ' The frame itself is never a useful focus target -- hand it straight to the
            ' pane, or a click on the caption would leave the keyboard nowhere.
            if gHelpCenter.pWeb then
                gHelpCenter.pWeb->MoveFocus( COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC )
            end if
            return 0

        case WM_ERASEBKGND
            ' The pane covers the whole client; erasing under it only adds a flash.
            return true

        case WM_CLOSE
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            ' Persist the geometry before the window goes.
            gConfig.SaveConfigFile
            return 0

        case WM_NCDESTROY
            ' Teardown order. Both handlers are COM-refcounted and delete THEMSELVES when
            ' the pane releases its reference, and their destructors call back into the
            ' pane to unregister -- through RemoveNavigationCompleted /
            ' RemoveNewWindowRequested, which dereference m_pCoreWebView2 with NO null
            ' guard, while CWebView2's own destructor Releases that pointer and leaves it
            ' set. Deleting the pane first is therefore a use-after-release.
            '
            ' HONEST SCOPE OF THIS FIX: that was A/B'd by removing these two blocks, and
            ' the Help Center still opened and closed with tiko surviving. So this is
            ' DEFENSIVE, not a fix for an observed crash -- the released interface is
            ' evidently still mapped in practice. It is kept because it costs two `if`s
            ' and removes real undefined behaviour, not because a crash was reproduced.
            '
            ' Clearing m_pWebView2 BEFORE unregistering makes each destructor a no-op, and
            ' unregistering while the pane is still alive is what triggers the self-delete
            ' at a moment when that is unambiguously safe.
            if gHelpCenter.pNavHandler then
                dim pNav as HELPCENTER_NAVCOMPLETED ptr = gHelpCenter.pNavHandler
                dim as EventRegistrationToken tok = pNav->m_token
                pNav->m_pWebView2 = 0
                if gHelpCenter.pWeb then gHelpCenter.pWeb->RemoveNavigationCompleted( tok )
                gHelpCenter.pNavHandler = 0
            end if
            if gHelpCenter.pNewWinHandler then
                dim pNew as HELPCENTER_NEWWINDOW ptr = gHelpCenter.pNewWinHandler
                dim as EventRegistrationToken tok = pNew->m_token
                pNew->m_pWebView2 = 0
                if gHelpCenter.pWeb then gHelpCenter.pWeb->RemoveNewWindowRequested( tok )
                gHelpCenter.pNewWinHandler = 0
            end if

            ' Now the pane, which must still go before the window it is hosted in. Its
            ' destructor closes the controller and releases the environment.
            if gHelpCenter.pWeb then
                delete gHelpCenter.pWeb
                gHelpCenter.pWeb = 0
            end if
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMHELPCENTER = 0
            gHelpCenter.hWebHost = 0
            gHelpCenter.bLoaded = false
            gHelpCenter.wszPendingQuery = ""
            return 0

    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)
end function


' ========================================================================================
' Show the Help Center, optionally running a search.
'
' wszSearch is already normalized by the caller (frmMainOnCommand), so it is a single
' line and within the length cap by the time it arrives here.
'
' nSite picks which of the two trees under settings\help is shown. It defaults to the
' generated site, so every existing F1 caller is unchanged.
' ========================================================================================
function frmHelpCenter_Show( byval wszSearch as DWSTRING, byval nSite as long ) as LRESULT

    ' --- the content has to be there ------------------------------------------------
    ' AHEAD of the already-open branch, deliberately: the switch below navigates, and
    ' handing WebView2 a file:// URL that does not exist replaces a working help window
    ' with the browser's own error page. Checking first costs one stat per F1.
    dim as DWSTRING wszIndex = HelpCenter_RootFolder( nSite ) & "index.html"
    if AfxFileExists( wszIndex ) = 0 then
        dim as DWSTRING wszMissing = L(90,"Help Center files not found.")
        if nSite = HELPCENTER_SITE_TIKO then
            wszMissing = L(520,"Tiko Editor Help files not found.")
        end if
        frmHelpCenter_ThemedMsgBox( wszMissing & !"\r\n\r\n" & wszIndex, _
                                    HelpCenter_SiteTitle( nSite ) )
        exit function
    end if

    ' --- already open ----------------------------------------------------------------
    ' Every page of the generated site carries the same header, search box included, so
    ' asking for the site that is ALREADY up needs no navigation at all, however deep into
    ' a docset the user has browsed. Asking for the OTHER site is a navigation -- one
    ' window, one WebView2 environment, one placement, so the sites take turns in it.
    if IsWindow( HWND_FRMHELPCENTER ) then
        if IsIconic( HWND_FRMHELPCENTER ) then ShowWindow( HWND_FRMHELPCENTER, SW_NORMAL )
        AllowSetForegroundWindow( ASFW_ANY )
        SetForegroundWindow( HWND_FRMHELPCENTER )

        if nSite <> gHelpCenter.nSite then
            gHelpCenter.nSite   = nSite
            ' bLoaded false again, and the query parked: the DOM of the site being left is
            ' about to be replaced, so scripting it now would run against a page that is
            ' going away. NavigationCompleted delivers it, exactly as on a cold open.
            gHelpCenter.bLoaded = false
            gHelpCenter.wszPendingQuery = wszSearch
            AfxSetWindowText( HWND_FRMHELPCENTER, HelpCenter_SiteTitle( nSite ) )
            if gHelpCenter.pWeb then gHelpCenter.pWeb->Navigate( HelpCenter_IndexUrl( nSite ) )
            exit function
        end if

        if gHelpCenter.bLoaded then
            frmHelpCenter_ApplySearch( wszSearch )
        else
            ' Still loading: let NavigationCompleted deliver it.
            gHelpCenter.wszPendingQuery = wszSearch
        end if
        exit function
    end if

    dim pWindow as CWindow ptr = new CWindow

    dim as DWSTRING wszText = HelpCenter_SiteTitle( nSite )
    HWND_FRMHELPCENTER = pWindow->Create( _
        HWND_DESKTOP, wszText, @frmHelpCenter_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_OVERLAPPEDWINDOW or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_LEFT )
    ' No WS_EX_CONTROLPARENT, unlike frmHelpViewer: there are no tab-navigable native
    ' children here -- the pane owns all keyboard input, and frmHelpCenter_FilterMessage
    ' keeps frmMain's IsDialogMessage off these messages entirely.

    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' Reads the theme currently IN MEMORY, not gConfig -- see Theme_IsDark.
    Theme_ApplyDarkCaption( HWND_FRMHELPCENTER )

    if (gConfig.HelpStartupRight = 0) orelse (gConfig.HelpStartupBottom = 0) then
        gConfig.HelpStartupRight  = pWindow->ScaleX(1100)
        gConfig.HelpStartupBottom = pWindow->ScaleY(800)
    end if

    dim WinPla as WINDOWPLACEMENT
    with WinPla
        .Length = sizeof(WinPla)
        .rcNormalPosition.Left   = gConfig.HelpStartupLeft
        .rcNormalPosition.Top    = gConfig.HelpStartupTop
        .rcNormalPosition.Right  = gConfig.HelpStartupRight
        .rcNormalPosition.Bottom = gConfig.HelpStartupBottom
        .showCmd = iif( gConfig.HelpStartupMaximized, SW_MAXIMIZE, SW_SHOWNORMAL )
    end with
    SetWindowPlacement( pWindow->hWindow, @WinPla )

    ' Bring it to the front NOW, before the IsReady wait below, which can block for as
    ' long as HELPCENTER_READYTIMEOUT while the runtime starts. The reopen path above
    ' has always done this; the CREATE path did not, so the very first F1 of a session
    ' built the window behind the editor and left the keyboard in Scintilla. The pane
    ' cannot take focus until it exists, so this only handles the frame -- ApplySearch
    ' does the other half once there is something to focus.
    AllowSetForegroundWindow( ASFW_ANY )
    SetForegroundWindow( HWND_FRMHELPCENTER )

    ' --- the WebView host child ------------------------------------------------------
    ' A plain STATIC. The pane is hosted here rather than in the frame because CWebView2's
    ' constructor writes the host window's GWLP_USERDATA, and a dedicated child keeps that
    ' clear of the frame's own CWindow bookkeeping.
    dim as RECT rcClient
    GetClientRect( HWND_FRMHELPCENTER, @rcClient )
    gHelpCenter.hWebHost = CreateWindowExW( 0, "STATIC", "", _
        WS_CHILD or WS_VISIBLE or WS_CLIPCHILDREN, _
        0, 0, rcClient.Right, rcClient.Bottom, _
        HWND_FRMHELPCENTER, cast(HMENU, IDC_FRMHELPCENTER_WEBHOST), _
        pWindow->InstanceHandle, 0 )

    ' --- the pane --------------------------------------------------------------------
    ' An isolated profile beside the exe rather than the default {exe}.exe.WebView2, so
    ' the cache and cookies are somewhere we name and can delete.
    dim as DWSTRING wszUserData = AfxGetExePathName & "settings\webview2"
    gHelpCenter.pWeb = new CWebView2( gHelpCenter.hWebHost, wszUserData )

    ' IsReady pumps messages while it waits for the environment AND the controller. A
    ' missing Edge WebView2 Runtime and a missing WebView2Loader.dll both land here --
    ' the loader is resolved by plain name with DyLibLoad and simply returns E_POINTER.
    dim as boolean bReady = gHelpCenter.pWeb->IsReady( HELPCENTER_READYTIMEOUT )

    ' WebView2 is new to this codebase and nothing in the self-test can reach it, so the
    ' one fact worth reporting is whether the runtime came up and which build answered.
    if environ("TIKO_HELPCENTER_TRACE") = "1" then
        dim as string sRep = "  helpcenter: IsReady=" & str(bReady) & _
                             " browser=" & gHelpCenter.pWeb->GetBrowserVersion() & _
                             " url=" & HelpCenter_IndexUrl( nSite )
        print sRep
    end if

    if bReady = false then
        delete gHelpCenter.pWeb
        gHelpCenter.pWeb = 0
        DestroyWindow( HWND_FRMHELPCENTER )
        frmHelpCenter_ThemedMsgBox( L(89,"The Help Center needs the Microsoft Edge WebView2 Runtime, which is not installed on this computer."), _
                                    L(88,"WebView2 Runtime Required") )
        exit function
    end if

    ' Both handlers register themselves in their constructors. They are kept on
    ' gHelpCenter so WM_NCDESTROY can unregister them BEFORE the pane goes -- see the
    ' teardown comment there, which is the difference between closing this window and
    ' crashing on it.
    gHelpCenter.pNavHandler    = new HELPCENTER_NAVCOMPLETED( gHelpCenter.pWeb )
    gHelpCenter.pNewWinHandler = new HELPCENTER_NEWWINDOW( gHelpCenter.pWeb )

    ' The pane's settings are left at their defaults. CWebView2 declares them as PROPERTY
    ' pairs whose setter returns HRESULT, which fbc does not accept as a setter -- so
    ' turning DevTools or the status bar off is not reachable through the wrapper, and
    ' neither is worth patching AfxNova for. IsScriptEnabled defaults to TRUE, which is
    ' the only one this window actually depends on (ExecuteScript).

    gHelpCenter.bLoaded = false
    gHelpCenter.nSite   = nSite
    gHelpCenter.wszPendingQuery = wszSearch
    gHelpCenter.pWeb->Navigate( HelpCenter_IndexUrl( nSite ) )

    frmHelpCenter_PositionWindows()

    function = 0
end function


' ========================================================================================
' Env-gated self-test: TIKO_HELPCENTER_SELFTEST=1
'
' READ THIS BEFORE TRUSTING THE COUNT. This suite is PURE and is therefore structurally
' blind to almost everything this module does. It cannot reach the WebView2 pane, the
' navigation, ExecuteScript, focus, the pump filter, or the geometry capture -- there is
' no message pump and no window here. Stubbing frmHelpCenter_Show to `return 0` leaves it
' fully GREEN. What it does cover is the three pure helpers, which is where the escaping
' and the URL arithmetic live and where a silent wrong answer is actually possible.
'
' The evidence for everything else is the interactive pass, not this file. Two hooks exist
' for that: TIKO_HELPCENTER_AUTOOPEN=1 opens the window at startup, and
' TIKO_HELPCENTER_TRACE=1 reports whether the WebView2 runtime came up and what query
' actually reached the pane -- which is the only way to see the F1 term, since it is
' derived from Scintilla state (the selection, else the word under the caret) that no
' assertion here can reach.
' ========================================================================================
dim shared as long gHelpCenterTestPass, gHelpCenterTestFail

private sub HelpCenter_Check( byval sName as string, byval bCondition as boolean )
    if bCondition then
        gHelpCenterTestPass += 1
    else
        gHelpCenterTestFail += 1
        dim as string sMsg = "  FAIL: " & sName
        print sMsg
    end if
end sub

sub frmHelpCenter_RunSelfTest()
    if environ("TIKO_HELPCENTER_SELFTEST") <> "1" then exit sub

    gHelpCenterTestPass = 0
    gHelpCenterTestFail = 0
    print "--- TIKO_HELPCENTER_SELFTEST ---"

    ' --- HelpCenter_JsQuote ------------------------------------------------------------
    ' The result always carries its own surrounding quotes: it is pasted straight into the
    ' script as a complete literal, so a caller cannot forget them.
    HelpCenter_Check( "jsquote: plain text is quoted", _
        HelpCenter_JsQuote( "AfxStrClipLeft" ) = """AfxStrClipLeft""" )
    HelpCenter_Check( "jsquote: empty stays a valid literal", _
        HelpCenter_JsQuote( "" ) = """""" )
    ' The ordinary case that a wrong escape ORDER breaks: a Windows path.
    HelpCenter_Check( "jsquote: backslash is doubled", _
        HelpCenter_JsQuote( "C:\dev" ) = """C:\\dev""" )
    HelpCenter_Check( "jsquote: a double quote is escaped", _
        HelpCenter_JsQuote( "say " & """" & "hi" & """" ) = """say \""hi\""""" )
    ' The ordering assertion proper. If the quote pass ran before the backslash pass, the
    ' backslash it INTRODUCES would be doubled and the literal would end early.
    HelpCenter_Check( "jsquote: backslash before quote, not after", _
        HelpCenter_JsQuote( "\" & """" ) = """\\\""""" )
    HelpCenter_Check( "jsquote: CR becomes \r", _
        HelpCenter_JsQuote( wchr(13) ) = """\r""" )
    HelpCenter_Check( "jsquote: LF becomes \n", _
        HelpCenter_JsQuote( wchr(10) ) = """\n""" )
    HelpCenter_Check( "jsquote: TAB becomes \t", _
        HelpCenter_JsQuote( wchr(9) ) = """\t""" )
    ' Legal in JSON, a LINE TERMINATOR to a JavaScript parser -- so an unescaped one is a
    ' syntax error in the injected script, not merely a wrong search.
    HelpCenter_Check( "jsquote: U+2028 is escaped", _
        HelpCenter_JsQuote( wchr(&h2028) ) = """\u2028""" )
    HelpCenter_Check( "jsquote: U+2029 is escaped", _
        HelpCenter_JsQuote( wchr(&h2029) ) = """\u2029""" )
    ' Non-ASCII is passed through: ExecuteScript takes UTF-16 and the site's index holds
    ' the same, so escaping it would only make the query not match.
    HelpCenter_Check( "jsquote: non-ASCII is left alone", _
        HelpCenter_JsQuote( wchr(&h00E9) ) = """" & wchr(&h00E9) & """" )

    ' --- HelpCenter_NormalizeQuery -----------------------------------------------------
    HelpCenter_Check( "normalize: plain identifier is untouched", _
        HelpCenter_NormalizeQuery( "CWindow.SetFont" ) = "CWindow.SetFont" )
    HelpCenter_Check( "normalize: empty stays empty", _
        HelpCenter_NormalizeQuery( "" ) = "" )
    HelpCenter_Check( "normalize: surrounding whitespace is trimmed", _
        HelpCenter_NormalizeQuery( "   Abs   " ) = "Abs" )
    ' The rule that matters: a block selection is not a query.
    HelpCenter_Check( "normalize: LF cuts to the first line", _
        HelpCenter_NormalizeQuery( "Abs" & wchr(10) & "Sgn" ) = "Abs" )
    HelpCenter_Check( "normalize: CRLF cuts to the first line", _
        HelpCenter_NormalizeQuery( "Abs" & wchr(13) & wchr(10) & "Sgn" ) = "Abs" )
    HelpCenter_Check( "normalize: a bare CR cuts too", _
        HelpCenter_NormalizeQuery( "Abs" & wchr(13) & "Sgn" ) = "Abs" )
    ' An indented line: the cut happens first, then the trim, or the leading tab survives.
    HelpCenter_Check( "normalize: indented first line is still trimmed", _
        HelpCenter_NormalizeQuery( wchr(9) & "Abs" & wchr(10) & "Sgn" ) = "Abs" )
    HelpCenter_Check( "normalize: an interior tab becomes a space", _
        HelpCenter_NormalizeQuery( "a" & wchr(9) & "b" ) = "a b" )
    HelpCenter_Check( "normalize: over-length is capped", _
        len( HelpCenter_NormalizeQuery( string(400, "x") ) ) = HELPCENTER_MAXQUERY )
    HelpCenter_Check( "normalize: at the cap nothing is lost", _
        len( HelpCenter_NormalizeQuery( string(HELPCENTER_MAXQUERY, "x") ) ) = HELPCENTER_MAXQUERY )

    ' --- HelpCenter_IsSearchableWord ---------------------------------------------------
    ' Filters the word-under-the-caret FALLBACK only. Every "false" here is a case where
    ' F1 should open an empty search box rather than search for a character.
    HelpCenter_Check( "word: an identifier is searchable", _
        HelpCenter_IsSearchableWord( "PsButton_Click" ) = true )
    HelpCenter_Check( "word: a leading-underscore name is searchable", _
        HelpCenter_IsSearchableWord( "__FB_WIN32__" ) = true )
    HelpCenter_Check( "word: a directive is searchable", _
        HelpCenter_IsSearchableWord( "#define" ) = true )
    ' The one that was actually observed: F1 with the caret at the start of a comment
    ' line searched for the apostrophe and showed "No matches".
    HelpCenter_Check( "word: a lone comment apostrophe is NOT searchable", _
        HelpCenter_IsSearchableWord( "'" ) = false )
    HelpCenter_Check( "word: an operator run is NOT searchable", _
        HelpCenter_IsSearchableWord( "=>" ) = false )
    HelpCenter_Check( "word: pure digits are NOT searchable", _
        HelpCenter_IsSearchableWord( "1234" ) = false )
    HelpCenter_Check( "word: empty is NOT searchable", _
        HelpCenter_IsSearchableWord( "" ) = false )
    ' Digits alone are out, but digits attached to letters are a name.
    HelpCenter_Check( "word: letters plus digits are searchable", _
        HelpCenter_IsSearchableWord( "keyword2" ) = true )
    ' Non-ASCII counts as a letter: refusing it would mean refusing to look up any
    ' identifier its author can actually type.
    HelpCenter_Check( "word: non-ASCII counts as a letter", _
        HelpCenter_IsSearchableWord( wchr(&h00E9) ) = true )

    ' --- HelpCenter_IndexUrl -----------------------------------------------------------
    dim as DWSTRING wszUrl = HelpCenter_IndexUrl()
    HelpCenter_Check( "url: has the file scheme", _
        PsLeft( wszUrl, 8 ) = "file:///" )
    HelpCenter_Check( "url: no backslash survives", _
        PsInStr( wszUrl, "\" ) = 0 )
    HelpCenter_Check( "url: points at the landing page", _
        PsInStr( wszUrl, "settings/help/helpcenter/index.html" ) > 0 )

    ' --- the two sites are two folders -------------------------------------------------
    ' The whole of the difference between them, so it is worth pinning: the default is the
    ' generated site (every F1 caller passes no site at all), the other one is a DIFFERENT
    ' folder, and both live under settings\help.
    dim as DWSTRING wszTikoUrl = HelpCenter_IndexUrl( HELPCENTER_SITE_TIKO )
    HelpCenter_Check( "site: the default is the generated Help Center", _
        HelpCenter_IndexUrl() = HelpCenter_IndexUrl( HELPCENTER_SITE_DOCS ) )
    HelpCenter_Check( "site: Tiko Editor Help is a different folder", _
        wszTikoUrl <> wszUrl )
    HelpCenter_Check( "site: and it points at its own landing page", _
        PsInStr( wszTikoUrl, "settings/help/tikohelp/index.html" ) > 0 )
    ' Both under settings\help -- the point of the move, and the thing a future edit to
    ' either branch of RootFolder would break silently.
    HelpCenter_Check( "site: both sites live under settings\help", _
        (instr( HelpCenter_RootFolder( HELPCENTER_SITE_DOCS ), "settings\help\" ) > 0) andalso _
        (instr( HelpCenter_RootFolder( HELPCENTER_SITE_TIKO ), "settings\help\" ) > 0) )
    ' The captions differ, because the one window wears whichever it is showing. Asserted
    ' on inequality rather than on the literals: they are localized, so a run under another
    ' .lang would fail an assertion written against the English text.
    HelpCenter_Check( "site: the two captions are not the same string", _
        HelpCenter_SiteTitle( HELPCENTER_SITE_DOCS ) <> HelpCenter_SiteTitle( HELPCENTER_SITE_TIKO ) )
    ' ...and neither is EMPTY, which is what a missing .lang id gives -- L() discards its
    ' default, so an unlocalized caption is a blank title bar with nothing to report it.
    HelpCenter_Check( "site: both captions are localized", _
        (len( HelpCenter_SiteTitle( HELPCENTER_SITE_DOCS ) ) > 0) andalso _
        (len( HelpCenter_SiteTitle( HELPCENTER_SITE_TIKO ) ) > 0) )
    HelpCenter_Check( "site: the not-found message is localized too", _
        (len( L(90,"") ) > 0) andalso (len( L(520,"") ) > 0) )
    ' These drive HelpCenter_PathToFileUrl -- the function IndexUrl actually calls -- on
    ' paths this machine does not have. tiko normally installs somewhere with no spaces,
    ' so an assertion against the live path alone would pass under an encoder that does
    ' nothing whatsoever.
    HelpCenter_Check( "url: a space becomes %20", _
        HelpCenter_PathToFileUrl( "C:\Program Files\tiko\index.html" ) = _
        "file:///C:/Program%20Files/tiko/index.html" )
    ' "%" must be encoded BEFORE the others or it re-encodes the escapes they introduce.
    HelpCenter_Check( "url: percent is encoded first, not last", _
        HelpCenter_PathToFileUrl( "C:\a%b c\i.html" ) = _
        "file:///C:/a%25b%20c/i.html" )
    ' "#" would truncate the path into a fragment and "?" into a query -- both silently.
    HelpCenter_Check( "url: a hash is encoded", _
        HelpCenter_PathToFileUrl( "C:\c#\i.html" ) = "file:///C:/c%23/i.html" )
    HelpCenter_Check( "url: a question mark is encoded", _
        HelpCenter_PathToFileUrl( "C:\a?b\i.html" ) = "file:///C:/a%3Fb/i.html" )

    ' --- localized ids -----------------------------------------------------------------
    ' len() > 0 CANNOT distinguish a real translation from the English fallback -- a key
    ' left blank in a translation is filled with English at load. The resolved values are
    ' printed for that reason; read them, do not just count the passes.
    HelpCenter_Check( "lang: 90 (Help Center) is present",  len( LL(86) ) > 0 )
    HelpCenter_Check( "lang: 92 (runtime title) is present", len( LL(88) ) > 0 )
    HelpCenter_Check( "lang: 93 (runtime body) is present",  len( LL(89) ) > 0 )
    HelpCenter_Check( "lang: 94 (files missing) is present", len( LL(90) ) > 0 )
    dim as string sLang = "  resolved: 90=" & LL(86) & " | 92=" & LL(88) & _
                          " | 94=" & LL(90)
    print sLang

    print "--- TIKO_HELPCENTER_SELFTEST: " & str(gHelpCenterTestPass) & " passed, " & _
          str(gHelpCenterTestFail) & " failed ---"
end sub
