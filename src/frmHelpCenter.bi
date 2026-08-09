' ========================================================================================
' frmHelpCenter -- the Help Center, opened in the USER'S OWN BROWSER.
'
' THERE IS NO WINDOW HERE ANY MORE, AND NO WEBVIEW2. This module used to be a top-level
' window hosting an embedded Edge WebView2 pane, 970 lines of it. It is now a URL builder
' and one ShellExecute.
'
' ---- WHY --------------------------------------------------------------------------
'
' The Help Center renders LOCAL STATIC HTML THIS PROJECT GENERATES ITSELF -- the helpgen
' output under settings\help, four docsets, loaded over file://. It is not a browser and
' it browses nothing. Two event handlers and one ExecuteScript were the whole of the
' integration; nothing ever asked WebView2 to be an application platform.
'
' So the embedded pane bought one thing -- the docs opening inside tiko rather than beside
' it -- and cost:
'
'   * a Windows-only dependency, on the port's critical path, in a codebase being made
'     cross-platform. There is no WebView2 on Linux and there will not be one.
'   * the Edge WebView2 Runtime as an install-time prerequisite, with two distinct
'     failure paths (missing runtime, missing WebView2Loader.dll) that both surfaced as
'     the Help Center simply not opening.
'   * a 147 MB profile directory beside the exe, for cache and cookies nothing needs.
'   * a shipped WebView2Loader.dll in the package.
'
' The browser the user already has renders this content perfectly, on both platforms, and
' brings search, zoom, history, printing and bookmarks that the embedded pane never had.
'
' See docs/port/webview2-decision.md for the full argument.
'
' ---- WHAT F1 DOES NOW ---------------------------------------------------------------
'
' F1 on a symbol used to open the pane and then ExecuteScript the word into the site's
' search box. A browser cannot be scripted from outside, so the URL carries it instead:
'
'     file:///.../helpcenter/index.html?q=CreateWindowEx
'
' The site honours ?q= (and #q=) in assets/app.js, beside the ?theme= it already read.
' That is a change in helpgen -- OUR generator -- rather than a coupling ported forward,
' and it deletes the last thing tiko knew about the page's DOM.
' ========================================================================================

#pragma once

' The longest query passed to the site. A selection longer than this is a block of code
' rather than an identifier; see HelpCenter_NormalizeQuery.
#define HELPCENTER_MAXQUERY                100

' Which of the two trees under settings\help is opened.
#define HELPCENTER_SITE_DOCS               0    ' settings\help\helpcenter -- generated, F1
#define HELPCENTER_SITE_TIKO               1    ' settings\help\tikohelp   -- hand written

declare function HelpCenter_NormalizeQuery( byval wszText as DWSTRING ) as DWSTRING
declare function HelpCenter_IsSearchableWord( byval wszWord as DWSTRING ) as boolean
declare function HelpCenter_PathToFileUrl( byval wszPath as DWSTRING ) as DWSTRING
declare function HelpCenter_QueryEncode( byval wszText as DWSTRING ) as DWSTRING
declare function HelpCenter_IndexUrl( byval nSite as long = HELPCENTER_SITE_DOCS ) as DWSTRING
declare function HelpCenter_SearchUrl( byval wszQuery as DWSTRING, _
                                       byval nSite as long = HELPCENTER_SITE_DOCS ) as DWSTRING
declare function HelpCenter_RootFolder( byval nSite as long = HELPCENTER_SITE_DOCS ) as DWSTRING
declare function HelpCenter_SiteTitle( byval nSite as long ) as DWSTRING

declare function frmHelpCenter_Show( byval wszSearch as DWSTRING, _
                                     byval nSite as long = HELPCENTER_SITE_DOCS ) as LRESULT
declare sub      frmHelpCenter_RunSelfTest()
