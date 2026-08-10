'' ========================================================================================
'' modAppHost -- THE SEAM THE DOCUMENT MODEL LEAVES THE SHELL THROUGH.
''
'' clsDocument and clsTopTabCtl are portable in every way that matters -- they store their
'' editor views as `any ptr`, and all 801 SciMsg call sites go through a function pointer
'' whose shape PsScintilla.bi deliberately preserves ("they are not edited, they are
'' relinked"). What keeps them in src/ is a short list of things they ask the WINDOW for:
'' make me a view, redraw it, how tall is it, what is its DPI ratio, ask the user for a path.
''
'' Those are host services, not document logic. This record is how they get asked for
'' without naming Win32 -- tiko fills it with the AfxNova bodies it already has, the shell
'' fills it with PsPlatform, and the document model stops caring which it got.
''
'' ---- WHY A RECORD OF FUNCTION POINTERS AND NOT AN #IFDEF -------------------------------
''
'' `_check_app_layer.bas` says it plainly for the whole layer: "Either the code belongs in
'' the shell, or it needs a PsPlatform abstraction. Not an #ifdef." Both binaries are built
'' from the same sources in the same tree; an #ifdef would mean the app layer is compiled
'' differently for each, and the standalone gate could then only ever check one of them.
''
'' ---- THE SAME IDIOM modDocViews ALREADY MADE PERMANENT ---------------------------------
''
'' modDocViews.bi is the ONE place a document's portable `any ptr` becomes a shell-side HWND,
'' and its header explains why inlining that back into 142 call sites would be wrong. This is
'' the same boundary seen from the other side: there, the shell reads a document's view; here,
'' a document asks the shell to act on one. Neither direction names the other's types.
''
'' ---- EVERY FIELD IS REQUIRED. A NULL IS A CRASH AT THE WORST MOMENT --------------------
''
'' There is no "if the host did not supply this, do something sensible" arm anywhere, and
'' there should not be: a document that silently skipped CreateView would present an empty
'' editor with no error. AppHost_IsComplete() below exists so a host can be checked ONCE, at
'' startup, instead of each call site guessing.
'' ========================================================================================

#pragma once

type AppHostServices
    '' ---- editor views ------------------------------------------------------------------
    '' Makes the nIndex'th view for a document (0 is the main pane, 1 the split) and returns
    '' the opaque handle clsDocument stores. tiko returns a tikoSciHost HWND; the shell
    '' returns a PsSciView. Neither side says which to the other.
    CreateView     as function(byval nIndex as long) as any ptr
    DestroyView    as sub(byval pView as any ptr)

    '' Whether the handle still refers to something. tiko's IsWindow; the shell's own
    '' bookkeeping. Documents routinely hold a view through teardown and every existing
    '' call site already tests before using.
    IsViewAlive    as function(byval pView as any ptr) as boolean

    '' The view's opaque Scintilla pointer -- what all 374 SciMsg call sites pass as
    '' their first argument. tiko's SciHost_DirectPointer; the shell's PsSciView.
    ''
    '' A SECOND HANDLE FOR THE SAME VIEW, deliberately. CreateView returns what the
    '' host wants to be given back (an HWND in tiko), and this returns what Scintilla
    '' wants to be called with. They are the same object and different pointers, and
    '' clsDocument has always stored both -- hWindow(i) and pSci(i).
    ViewSciPointer as function(byval pView as any ptr) as any ptr

    '' Repaint now, rather than at the next idle. Used where a load would otherwise leave the
    '' pane stale for a visible moment.
    InvalidateView as sub(byval pView as any ptr)

    '' Suppress repainting across a bulk edit, then restore it. A PERFORMANCE
    '' AFFORDANCE, not a capability -- and it is carried across rather than dropped
    '' precisely because no gate in this repo can see flicker. The suites would stay
    '' green if it vanished; the only symptom is a large file loading visibly.
    ''
    '' The shell decides separately whether PsSciView needs an equivalent at all:
    '' PsSurface repaints from a damage region rather than per control, so it may
    '' legitimately implement this as a no-op.
    SetViewRedraw  as sub(byval pView as any ptr, byval bOn as boolean)

    '' The view's DPI ratios. Two call sites need these to size margins, and both currently
    '' reach the owning CWindow to get them -- which is also why they cannot simply read a
    '' global: a view parented elsewhere would silently answer 1.0 and put every margin at
    '' the wrong width above 100%.
    ViewScale      as sub(byval pView as any ptr, byref rx as single, byref ry as single)

    '' The view's height in pixels. The one caller divides it by the line height to get lines
    '' per page.
    ''
    '' HEIGHT, NOT A RECT, and that is a simplification rather than a shortcut: the existing
    '' code takes AfxGetWindowRect, calls MapWindowPoints to translate screen coordinates to
    '' frmMain's client area, and then uses only (bottom - top). Translation cannot change a
    '' height, so the mapping never affected the answer.
    ViewHeight     as function(byval pView as any ptr) as long

    '' The OWNER window's UI font -- not the editor's. One caller styles the autocomplete
    '' popup with it, so the popup matches the application's chrome rather than the code face.
    '' Reached through the view for the same reason ViewScale is: it is the font of whatever
    '' window this view is parented to, not a process-wide constant.
    ''
    '' Two fields returning the parts separately, rather than one filling byref parameters,
    '' because the call site casts the name straight into a Scintilla message and matching
    '' that expression exactly is worth more here than a tidier signature.
    ViewUiFontName as function(byval pView as any ptr) as string
    ViewUiFontSize as function(byval pView as any ptr) as long

    '' Paint the theme onto one view. THE BIGGEST THING BEHIND THIS SEAM by a wide margin --
    '' 401 lines in tiko, and it held 87 of clsDocument's 88 theme references.
    ''
    '' pOwnerView is NOT the view being styled: it is the document's MAIN view, asked only for
    '' DPI ratios and the UI font. A Find in Project excerpt view is styled through here too
    '' and must scale like the editor it came from rather than like itself.
    ''
    '' The three document values are passed because that is ALL the body touched -- measured,
    '' not guessed. Passing a document pointer instead would make every host name that type.
    StyleView      as sub(byval pSciView as any ptr, byval pOwnerView as any ptr, _
                          byval nEncoding as long, byval sDiskFilename as DWSTRING, _
                          byval bIsNew as boolean)

    '' ---- the theme, as far as the document model needs it ------------------------------
    '' The background a search-scope highlight is drawn in. ONE COLOUR, and that is the point:
    '' the document model needs exactly this much of the theme, and the theme itself is
    '' staying in the shell -- COLORREF appears 552 times across src/, which is a type
    '' migration on the scale of the DWSTRING swap rather than anything this step can do.
    ''
    '' A ulong rather than COLORREF, so the answer names no Win32 type. Both sides already
    '' store colours as 32-bit values.
    ThemeSelectionBack as function() as ulong

    '' Read a file into txtBuffer, detecting and recording its encoding on the document.
    ''
    '' A SERVICE RATHER THAN APP CODE, and not by choice: the decode goes through
    '' WideToUtf8, a private helper in modEncoding.inc that is WideCharToMultiByte. The WRITE
    '' path is already portable -- Doc_EncodeForDisk and Doc_WriteToDisk live in app/ -- so it
    '' is only reading that still needs the platform. The shell will implement this with
    '' PsEncDecode.
    LoadFileText   as function(byval wszPath as DWSTRING, byref txtBuffer as string, _
                               byval pDoc as clsDocument ptr) as boolean

    '' Resolve a partial #include path to something that exists.
    ''
    '' A SERVICE FOR THE SAME REASON AS LoadFileText: it resolves against the ACTIVE BUILD
    '' CONFIGURATION and reaches frmBuildConfig_getActiveBuildIndex, a form function. Both of
    '' these were tried in app/ and both came back.
    ResolveIncludePath as function(byval pDoc as clsDocument ptr, byval sFilename as string) as string

    '' The platform's text for the last failed call, or "" if it has none to offer.
    ''
    '' THE SMALLEST POSSIBLE SEAM, and it is what let Doc_WriteToDisk move into the app
    '' layer. That function is entirely PsCore -- PsFileExists, PsFileWriteAll, PsFileReplace
    '' -- except for two Win32ErrorText(GetLastError()) calls that produce a MESSAGE. Both
    '' already fall back to a fixed string when the platform gives nothing, so a host with no
    '' equivalent returns "" and loses only detail, never correctness.
    LastErrorText  as function() as DWSTRING

    '' ---- telling the user something about a save ----------------------------------------
    '' Both are PROMPTS, so both are services: the first returns the user's answer and the
    '' second must be seen before the caller carries on. A host that stubbed either would
    '' silently discard characters, or lose a file without saying so.
    ConfirmLossySave   as function(byval pDoc as clsDocument ptr, byval wszPath as DWSTRING, _
                                   byval nEncoding as long) as boolean
    ReportWriteFailure as sub(byval wszPath as DWSTRING, byval wszErr as DWSTRING)

    '' ---- asking the user for a path -----------------------------------------------------
    '' TRUE if the user chose one. tiko uses the Afx IFileDialog wrappers; the shell will use
    '' PsPlatform's, which are asynchronous and get a synchronous wrapper of their own.
    AskOpenPath    as function(byref sOut as DWSTRING) as boolean
    AskSavePath    as function(byref sPath as DWSTRING, byref sExt as DWSTRING) as boolean

    '' ---- what the tab control asks the shell --------------------------------------------
    '' Whether the Find bar is up. The tab control clears selection highlighting on a switch
    '' unless a find is active.
    IsFindVisible  as function() as boolean
end type

extern gAppHost as AppHostServices


'' ========================================================================================
'' AppHostNotify -- what the model TELLS the host, expecting nothing back.
''
'' ---- WHY THIS IS A SEPARATE RECORD, and it is not tidiness ----------------------------
''
'' EVERY FIELD HERE CAN BE SAFELY NO-OP'D. NOT ONE FIELD OF AppHostServices CAN.
''
'' That is the whole distinction, and it is the question a host actually has to answer while
'' being built. A binary with no TODO pane can leave RefreshTodoList empty and be CORRECT; a
'' binary that leaves CreateView empty presents an editor with no editor in it. Before the
'' split both kinds sat in one record behind one completeness check that said "all present"
'' and nothing about which mattered.
''
'' It also resolves a note this file already carried. CloseTab was flagged in AppHostServices
'' as "the one field here that is a command rather than a service" -- it is here now, which
'' is where a command belongs.
''
'' ---- STILL REQUIRED, STILL CHECKED ----------------------------------------------------
''
'' Safely no-op-able does NOT mean optional. A null field is still a crash, so the same
'' all-fields-set check applies; what changes is that a host filling these in knows an empty
'' body is a legitimate answer and an empty body in the other record never is.
'' ========================================================================================
type AppHostNotify
    '' Close the tab at this index, with everything that implies -- prompting to save, and
    '' removing the document.
    CloseTab       as sub(byval nTabIdx as long)

    '' Find in Project holds references to a document's buffer and excerpt views bound to it,
    '' so it has to hear about both events. Fire-and-forget: a host without that feature
    '' leaves them empty and is correct.
    OnDocumentClosing as sub(byval pDoc as clsDocument ptr)
    OnDocumentSaved   as sub(byval pDoc as clsDocument ptr)

    '' Re-scan this document's buffer in the background. The scan manager STAYS IN THE SHELL:
    '' it wakes and stops a worker thread with Win32 event objects, and PsPlatform has no
    '' threading or synchronisation service at all. Background parsing belongs to whoever owns
    '' the UI thread -- see docs/port/document-model-blockers.md.
    RequestBufferScan as sub(byval pDoc as clsDocument ptr)

    '' The document's root name for the TODO store -- ScanMgr's, and shell-side with it.
    DocRootName       as function(byval pDoc as clsDocument ptr) as DWSTRING

    '' The shell's own chrome needs re-laying-out, or its TODO pane refreshing, after a
    '' document-list change. Three separate calls rather than one "something changed",
    '' because that is how the code already reads and a merged one would relayout more than
    '' the caller meant.
    '' The MRU lists and the Explorer pane. All three are shell UI reacting to a
    '' document-list change, and all three were reached directly until the classes moved --
    '' via includes of frmExplorer.bi and modMRU.bi that turned out to be VESTIGIAL for
    '' everything else in those files.
    UpdateMruFile     as sub(byval wszPath as DWSTRING)
    UpdateMruProject  as sub(byval wszPath as DWSTRING)
    ReloadExplorer    as sub()

    RelayoutMain      as sub()
    RelayoutTopTabs   as sub()
    RefreshTodoList   as sub()
end type

extern gAppNotify as AppHostNotify

'' TRUE only when every field is set. Called once at startup by both binaries -- see the note
'' above about why there is no per-call fallback.
declare function AppHost_IsComplete() as boolean

'' Which field is missing, for the message. Empty when the record is complete.
declare function AppHost_FirstMissing() as string

'' The same pair for the notification record. Two checks rather than one, because two
'' records: a host that filled only one of them would otherwise pass.
declare function AppNotify_IsComplete() as boolean
declare function AppNotify_FirstMissing() as string
