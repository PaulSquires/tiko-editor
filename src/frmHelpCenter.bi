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
' The Help Center: a WebView2 pane showing the generated HelpCenter site staged at
' settings\help\helpcenter. It replaces BOTH of tiko's previous help mechanisms -- the .chm
' HtmlHelp path (ShowContextHelp) and the RTF-based frmHelpViewer.
'
' IT SHOWS TWO SITES, NOT ONE, AND STILL ONLY EVER ONE WINDOW. The generated reference site
' (F1) and the hand-written Tiko Editor help at settings\help\tikohelp are both static HTML
' trees loaded through the same pane, so switching between them is a NAVIGATION rather than
' a second window -- there is one WebView2 environment, one WINDOWPLACEMENT and one caption
' to keep straight. gHelpCenter.nSite records which one is up.
'
' The window is MODELESS and NON-OWNED (parent HWND_DESKTOP), so it stays open beside the
' editor and disables nothing. frmHelpViewer was tiko's only window of that shape and this
' one keeps its singleton guard, its WINDOWPLACEMENT persistence and its dark caption.
'
' THE PANE IS THE WHOLE UI. There is no native search band: every page of the generated
' site carries the same header shell, including the search box (input#q). That is what
' makes a re-F1 from a deep topic page need no navigation at all -- the box is already
' on screen, so _Show only re-focuses it and re-runs the query.
'
' The site has no ?q= URL parameter (only ?theme=), so the query is delivered by
' ExecuteScript setting #q.value and dispatching an 'input' event -- which is the event
' the site's own render() listens for, so dispatching it IS running the search. Driving
' it from this side rather than patching the site means a fresh copy of the generated
' site can never break the feature.
'
' This header is included early (before clsConfig.inc, which calls _CaptureState), so it
' must name no Ps* type. It DOES name CWebView2, which is why it has to follow the
' AfxNova\CWebView2.inc include at the top of tiko.bas.
' ========================================================================================

#pragma once

' Arguments parked for MSG_USER_SHOW_HELPCENTER -- see the note on that message in
' modDeclares.bi. Only ever written immediately before the post and read once by the handler.
dim shared gHelpPendingQuery as DWSTRING
dim shared gHelpPendingSite  as long

' The child window the WebView2 control is hosted in. The pane is NOT hosted in the frame
' itself: CWebView2's constructor writes the host window's GWLP_USERDATA, and a dedicated
' child keeps that clear of the frame's own CWindow bookkeeping. It is also the pattern the
' AfxNova examples use.
#define IDC_FRMHELPCENTER_WEBHOST          1000

' Longest query pushed into the search box. A selection longer than this is a paragraph,
' not an identifier, and the site's ranker does nothing useful with it.
#define HELPCENTER_MAXQUERY                100

' How long _Show waits for the WebView2 environment + controller to come up before it
' declares the runtime missing. IsReady pumps messages while it waits.
#define HELPCENTER_READYTIMEOUT            15000

' The two sites this one window can show, and the ONLY thing that differs between them: a
' folder under settings\help and a caption. Both are plain static HTML opened over file://,
' so nothing else in this module has to branch -- see frmHelpCenter_ApplySearch, which is a
' no-op on a site that has no search box rather than a case to add here.
#define HELPCENTER_SITE_DOCS               0    ' settings\help\helpcenter -- generated, F1
#define HELPCENTER_SITE_TIKO               1    ' settings\help\tikohelp   -- hand written


type HELPCENTER_TYPE
    ' The WebView2 wrapper. Heap-allocated because the window outlives _Show; deleted in
    ' WM_NCDESTROY.
    as CWebView2 ptr    pWeb
    ' The child window pWeb is hosted in.
    as HWND             hWebHost
    ' The two event handlers, kept so they can be UNREGISTERED BEFORE the pane is deleted:
    ' CWebView2's destructor Releases m_pCoreWebView2 without zeroing it, and the Remove*
    ' methods dereference that pointer with no null guard, so a handler destroyed *after*
    ' the pane calls into a released interface. Defensive rather than a fix for an
    ' observed crash -- see the A/B note at the WM_NCDESTROY teardown. Typed as any ptr
    ' because the handler types are declared in the .inc, below this header.
    as any ptr          pNavHandler
    as any ptr          pNewWinHandler
    ' The query waiting for the first NavigationCompleted. The very first _Show has to
    ' navigate before it can touch the DOM, so the query cannot be applied inline; every
    ' later _Show applies it directly and leaves this empty.
    as DWSTRING         wszPendingQuery
    ' Set once the site has loaded at least once, so a later _Show knows it can skip the
    ' navigation and script the page directly.
    as boolean          bLoaded
    ' Which of the two sites is loaded, HELPCENTER_SITE_*. Read by _Show to tell a re-open
    ' (focus and re-search, no navigation) from a SWITCH (navigate, and bLoaded goes false
    ' again) -- without it, asking for the other site would silently re-focus this one.
    as long             nSite
end type

dim shared as HELPCENTER_TYPE gHelpCenter


' Pure helpers. These carry every piece of arithmetic and escaping in the module and are
' what the self-test actually exercises -- see the honesty note in _RunSelfTest.
declare function HelpCenter_JsQuote( byval wszText as DWSTRING ) as DWSTRING
declare function HelpCenter_NormalizeQuery( byval wszText as DWSTRING ) as DWSTRING
declare function HelpCenter_IsSearchableWord( byval wszWord as DWSTRING ) as boolean
declare function HelpCenter_PathToFileUrl( byval wszPath as DWSTRING ) as DWSTRING
declare function HelpCenter_IndexUrl( byval nSite as long = HELPCENTER_SITE_DOCS ) as DWSTRING
declare function HelpCenter_RootFolder( byval nSite as long = HELPCENTER_SITE_DOCS ) as DWSTRING
' The window caption for a site, and the caption of its own "files not found" box. Localized,
' so it lives here rather than being written out at the three call sites.
declare function HelpCenter_SiteTitle( byval nSite as long ) as DWSTRING

' Forward declared: the NavigationCompleted handler calls it, and that type has to be
' defined above the pane it registers against.
declare sub      frmHelpCenter_ApplySearch( byval wszQuery as DWSTRING )

declare function frmHelpCenter_Show( byval wszSearch as DWSTRING, _
                                     byval nSite as long = HELPCENTER_SITE_DOCS ) as LRESULT
declare function frmHelpCenter_FilterMessage( byval pMsg as MSG ptr ) as boolean
declare sub      frmHelpCenter_CaptureState()
declare sub      frmHelpCenter_RunSelfTest()
