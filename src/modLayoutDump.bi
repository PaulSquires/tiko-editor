'' ========================================================================================
'' modLayoutDump -- THE LAYOUT ORACLE FOR PHASE 7c.
''
'' Prints every frmMain child's rectangle, in frmMain CLIENT coordinates, for a fixed set of
'' layout states, and then exits. Env-gated on TIKO_LAYOUT_DUMP=1; does nothing otherwise.
''
'' ---- WHY THIS EXISTS -------------------------------------------------------------------
''
'' 7c step 1 rebuilds frmMain's layout as a widget tree, and its self-test can only assert
'' RELATIONS between rectangles -- this bar is above that one, the children tile the client
'' area without gap or overlap. Relations catch a band in the wrong ORDER. They do not catch
'' a band with the wrong SIZE, because a wrong-but-self-consistent layout satisfies every
'' relation there is.
''
'' So the numbers have to come from somewhere, and the only authority on what tiko's layout
'' does is tiko's layout. This dumps them from the running editor so the new shell can be
'' diffed against them rather than against an opinion.
''
'' ---- WHAT MAKES A DUMP COMPARABLE ------------------------------------------------------
''
'' Three things, all of which have to be pinned or the output moves for reasons that are
'' nothing to do with the layout:
''
''   1. THE CLIENT SIZE. Restored from the saved window placement on a normal launch, so it
''      is whatever the last human left behind. Forced here, and the size that actually
''      resulted is printed in the header rather than assumed -- the window manager gets the
''      last word on any SetWindowPos and occasionally takes it.
''   2. THE DPI SCALE. Every band height is a design constant through pWindow->ScaleY, so a
''      dump taken at 1.0 and one taken at 1.75 disagree everywhere. Printed in the header.
''   3. THE MEASURED BAND SIZES. nHeightMenuBar, nHeightStatusBar, nWidthPanel and
''      nWidthTopTabsMenu are read by frmMain_PositionWindows OUT OF THE WINDOWS THEMSELVES
''      -- they depend on the loaded fonts, not on any constant in the source. Printed in
''      the header, because the shell has to reproduce them as inputs and cannot derive them.
''
'' ---- WHY IT RUNS FROM MSG_USER_PROCESS_COMMANDLINE -------------------------------------
''
'' Same reason frmFindInProject's suites do, and the comment beside them says it: the
'' session has to be restored first. Three of the states below need a real document -- the
'' two split modes need pDoc, and the tab-count band needs a tab -- so the dump is driven
'' with a file on the command line and reads the document the session actually opened.
''
'' It ends with `end 0` for the reason TIKO_FINDPROJ_AUTOEXIT does: tiko's console output is
'' BLOCK-BUFFERED when redirected, so a run that is killed rather than exited loses every
'' line it printed. A dump run is not an editing session and has nothing to stay open for.
''
'' ---- WHAT IT DOES NOT COVER, AND WHY ---------------------------------------------------
''
'' THE FLOATING OUTPUT PANEL. frmOutputFloat_IsFloating() is driven by a real undock, which
'' reparents HWND_FRMOUTPUT into a second top-level window. Faking the flag without the
'' reparent would dump a state the editor cannot actually be in. The floating case collapses
'' to "output reserves nothing", which the OUTPUT_HIDDEN state below already covers for the
'' only quantity that reaches the document rect.
''
'' THE PROJECT-REPLACE ARM of the replace band -- `IsProjectSearchTab andalso
'' gFind.bProjectReplaceActive` -- needs a project search tab active. The band's GEOMETRY is
'' identical to the plain bShowReplacePanel arm, which is dumped; only the predicate differs.
'' ========================================================================================

declare sub LayoutDump_Run()
