'' ========================================================================================
'' modViewStyle -- painting the theme onto one Scintilla view. SHELL-SIDE, and staying.
''
'' ---- WHY THIS IS NOT IN THE DOCUMENT MODEL ---------------------------------------------
''
'' It was clsDocument.ApplyPropertiesToView, and it held 87 of that class's 88 references to
'' the THEME record -- plus every other thing keeping the file in src/: Ctrue, GetDoubleClickTime,
'' GetFontCharSetID and COLORREF.
''
'' THE THEME CANNOT MOVE AND WILL NOT FOR SOME TIME. COLORREF appears 552 times across src/,
'' which is a type migration on the scale of the DWSTRING swap and deserves its own phase
'' rather than being smuggled into a document-model commit.
''
'' But it should not move anyway. STYLING IS APPEARANCE, AND APPEARANCE IS THE HOST'S -- the
'' same reasoning that already keeps clsScanMgr (a worker thread) and clsTopTabCtl (a control
'' facade) in the shell. A document knows what it CONTAINS; what it LOOKS LIKE belongs to
'' whoever owns the window it is drawn in. tikoshell will style a PsSciView from PsPlatform's
'' theme without any of this code.
''
'' ---- THE FIVE PARAMETERS, AND WHY THEY ARE NOT A pDoc ----------------------------------
''
'' Measured, not guessed: the 401-line body touched exactly four things on the document --
'' hWindow(0) three times, and FileEncoding, DiskFilename and IsNewFlag once each. Passing
'' those beats passing a clsDocument ptr, because this file must not name that type: it is
'' the shell's, and clsDocument is on its way to app/.
''
'' pOwnerView is NOT the view being styled. It is the document's MAIN view, and it is used
'' only to ask the host for DPI ratios and the UI font -- a Find in Project excerpt view is
'' styled here too, and it must scale like the editor it came from rather than like itself.
''
'' Everything else the body needs comes from gConfig, and is recomputed here rather than
'' passed, which is what keeps the signature to what actually varies.
''
'' ---- EXTRACTED VERBATIM ----------------------------------------------------------------
''
'' The body is unchanged apart from those four substitutions. That is deliberate and it is
'' what lets the 27-suite oracle gate the extraction: the same code runs, reached through one
'' more indirection.
'' ========================================================================================

#pragma once

sub ViewStyle_Apply( byval pSciView as any ptr, _
                     byval pOwnerView as any ptr, _
                     byval nEncoding as long, _
                     byval sDiskFilename as DWSTRING, _
                     byval bIsNew as boolean )
    if pSciView = 0 then exit sub

    dim nPixels     as long
    dim bitsNeeded  as long
    dim wFileExt    as wstring * MAX_PATH
    dim strFontName as string
    dim nFontExtraSpace as long
    dim nFontSize   as long
    dim nFontcase   as long
    dim rxRatio     as single = 1
    dim ryRatio     as single = 1

    ' DPI ratios come from the DOCUMENT's own window, not the caller's: an excerpt view is
    ' parented elsewhere, and a plain registered class would silently give a ratio of 1 and
    ' put every margin at the wrong width above 100%.
    '' The HOST knows the view's DPI ratios. This reached the owning CWindow, which is why
    '' it could not simply read a global: a view parented elsewhere answers 1.0 and would put
    '' every margin at the wrong width above 100%.
    gAppHost.ViewScale( pOwnerView, rxRatio, ryRatio )

    strFontName = PsText(gConfig.EditorFontname)
    nFontSize   = PsVal(gConfig.EditorFontsize)
    nFontExtraSpace = PsVal(gConfig.FontExtraSpace)

    select case gConfig.KeywordCase
        case 0:  nFontcase = SC_CASE_LOWER
        case 1:  nFontcase = SC_CASE_UPPER
        ' "Proper Case" is the keyword's canonical spelling, taken from the keyword
        ' files via SCI_SETCASEKEYWORDS below. SC_CASE_CAMEL would only capitalize the
        ' first letter, giving "Messagebox" and "Byval". The combo index is unchanged,
        ' so an existing settings.ini keeps meaning what it meant -- only the label and
        ' the rendering improved. NOT related to ChangeSelectionCase's "mixed case",
        ' which rewrites the buffer.
        case 2:  nFontcase = SC_CASE_PROPER
        case 3:  nFontcase = SC_CASE_MIXED    ' original case
    end select


		SciMsg( pSciView, SCI_SETFONTQUALITY, SC_EFF_QUALITY_LCD_OPTIMIZED, 0)

        ' Add Brace Highlighting functionality:
        ' We use indicators because the lexer can overwrite the colors set STYLE_BRACELIGHT and
        ' STYLE_BRACEBAD once they are outputted to the screen.
        ' First, we set the style of the indicator number we want to use to the Box style
        SciMsg( pSciView, SCI_INDICSETSTYLE, 9, INDIC_BOX )
        'SciMsg( pSciView, SCI_INDICSETSTYLE, 9, INDIC_PLAIN) 
        ' then, we make that indicator the current one
        SciMsg( pSciView, SCI_SETINDICATORCURRENT, 9, 0 )
        ' Lastly, we apply the indicator (in this case), to a single char
        SciMsg( pSciView, SCI_BRACEHIGHLIGHTINDICATOR, 1, 9 )  ' use indicator for SCI_BRACEHIGHLIGHTINDICATOR
        SciMsg( pSciView, SCI_BRACEBADLIGHTINDICATOR, 1, 9 )   ' and SCI_BRACEBADLIGHTINDICATOR

        SciMsg( pSciView, SCI_STYLESETFONT, STYLE_DEFAULT, cast(integer, strptr(strFontName)) )
        SciMsg( pSciView, SCI_STYLESETSIZE, STYLE_DEFAULT, nFontSize )
        SciMsg( pSciView, SCI_SETEXTRAASCENT, nFontExtraSpace, 0 )
        SciMsg( pSciView, SCI_SETEXTRADESCENT, nFontExtraSpace, 0 )
        SciMsg( pSciView, SCI_STYLESETCHARACTERSET, STYLE_DEFAULT, GetFontCharSetID(gConfig.EditorFontCharset) )

        SciMsg( pSciView, SCI_STYLESETFORE, STYLE_DEFAULT, theme.editor.text.forecolor)
        SciMsg( pSciView, SCI_STYLESETBACK, STYLE_DEFAULT, theme.editor.text.backcolor)
        SciMsg( pSciView, SCI_STYLESETBOLD, STYLE_DEFAULT, theme.editor.text.bold )
        SciMsg( pSciView, SCI_STYLESETITALIC, STYLE_DEFAULT, theme.editor.text.italic )
        SciMsg( pSciView, SCI_STYLESETUNDERLINE, STYLE_DEFAULT, theme.editor.text.underline )
        SciMsg( pSciView, SCI_STYLECLEARALL, 0, 0 )  ' Copies global style to all others
            
        ' Set the style for the AutoComplete popup list
        SciMsg( pSciView, SCI_STYLESETFONT, STYLE_AUTOCOMPLETE, cast(integer, gAppHost.ViewUiFontName(pOwnerView)) )
        SciMsg( pSciView, SCI_STYLESETSIZE, STYLE_AUTOCOMPLETE, gAppHost.ViewUiFontSize(pOwnerView))
        SciMsg( pSciView, SCI_STYLESETCHARACTERSET, STYLE_AUTOCOMPLETE, GetFontCharSetID(gConfig.EditorFontCharset) )

        ' define bookmark marker (white circle)
        SciMsg( pSciView, SCI_MARKERDEFINE, MARKER_BOOKMARK, SC_MARK_VERTICALBOOKMARK )
        SciMsg( pSciView, SCI_MARKERSETFORE, MARKER_BOOKMARK, theme.editor.bookmark.forecolor)
        SciMsg( pSciView, SCI_MARKERSETBACK, MARKER_BOOKMARK, theme.editor.bookmark.backcolor )
        
        ' define breakpoint marker (red circle)
        SciMsg( pSciView, SCI_MARKERDEFINE, MARKER_BREAKPOINT, SC_MARK_CIRCLE )
        SciMsg( pSciView, SCI_MARKERSETFORE, MARKER_BREAKPOINT, theme.editor.breakpoint.forecolor )
        SciMsg( pSciView, SCI_MARKERSETBACK, MARKER_BREAKPOINT, theme.editor.breakpoint.backcolor )

        ' Arm the mouse dwell notification. Without a time set Scintilla's default is
        ' SC_TIME_FOREVER, SCN_DWELLSTART never arrives, and the debugger's hover datatips
        ' silently never appear. Half the double-click time, so it follows the machine's own
        ' setting rather than a number invented here.
        '
        ' (Carried in from the debugger branch during the merge: it was added inside the loop
        '  this method was extracted FROM, so it belongs here now. Find in Project's excerpt
        '  views get it too, which is harmless -- they raise no datatips of their own.)
        SciMsg( pSciView, SCI_SETMOUSEDWELLTIME, GetDoubleClickTime() \ 2, 0 )

        ' define debugger cuurent line marker (yellow arrow)
        SciMsg( pSciView, SCI_MARKERDEFINE, MARKER_DEBUGGER_CURLINE, SC_MARK_SHORTARROW )
        SciMsg( pSciView, SCI_MARKERSETFORE, MARKER_DEBUGGER_CURLINE, theme.editor.debugline.forecolor )
        SciMsg( pSciView, SCI_MARKERSETBACK, MARKER_DEBUGGER_CURLINE, theme.editor.debugline.backcolor )
        ''
        ''  MARGIN 0: Line Numbering (defaults to width 0)
        nPixels = SciMsg( pSciView, SCI_TEXTWIDTH, 0, cast(integer, @"_99999"))
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER )
        SciMsg( pSciView, SCI_STYLESETFORE, STYLE_LINENUMBER, theme.editor.linenumbers.forecolor )
        SciMsg( pSciView, SCI_STYLESETBACK, STYLE_LINENUMBER, theme.editor.linenumbers.backcolor )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 0, iif(gConfig.LineNumbering, nPixels, 0) )
        SciMsg( pSciView, SCI_STYLESETBOLD, STYLE_LINENUMBER, theme.editor.linenumbers.bold )
        SciMsg( pSciView, SCI_STYLESETITALIC, STYLE_LINENUMBER, theme.editor.linenumbers.italic )
        SciMsg( pSciView, SCI_STYLESETUNDERLINE, STYLE_LINENUMBER, theme.editor.linenumbers.underline )
    
        ''
        ''  MARGIN 1: Non-Folding symbols (defaults to width 16) (Bookmark symbol, etc) (will be same color as line numbering)
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 1, SC_MARGIN_TEXT )
        SciMsg( pSciView, SCI_SETMARGINSENSITIVEN, 1, 1 )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 1, iif(gConfig.LeftMargin, 16 * rxRatio, 0) )
                    
        ''
        ''  MARGIN 2: Folding symbols (defaults to width 0)
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL )
        SciMsg( pSciView, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS )
        SciMsg( pSciView, SCI_SETFOLDMARGINCOLOUR, Ctrue, theme.editor.foldmargin )
        SciMsg( pSciView, SCI_SETFOLDMARGINHICOLOUR, Ctrue, theme.editor.foldmargin )
        SciMsg( pSciView, SCI_SETMARGINSENSITIVEN, 2, 1 )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 2, iif(gConfig.FoldMargin, 16 * rxRatio, 0) )

        ''
        ''  MARGIN 3: Small margin to offset left margins from actual text (4 pixels)
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 3, SC_MARGIN_TEXT )
        SciMsg( pSciView, SCI_SETMARGINWIDTHN, 3, 4 * rxRatio )
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 3, SC_MARGIN_FORE )
        SciMsg( pSciView, SCI_SETMARGINTYPEN, 3, SC_MARGIN_BACK )
                
        ''
        ''  CONFINE CARET to TEXT
        if gConfig.ConfineCaret then
            SciMsg( pSciView, SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION, 0 )
        else
            SciMsg( pSciView, SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION or SCVS_USERACCESSIBLE, 0 )
        end if
    
        ''
        ''  TABS as SPACES
        if gConfig.TabIndentSpaces then
            SciMsg( pSciView, SCI_SETUSETABS, false, 0 )
        else
            SciMsg( pSciView, SCI_SETUSETABS, Ctrue, 0 )
        end if
        
        ''
        ''  SELECTIONS FILL ENTIRE SCREEN SPACE
        SciMsg( pSciView, SCI_SETSELEOLFILLED, Ctrue, 0 )
            
        ''
        ''  TAB WIDTH 
        SciMsg( pSciView, SCI_SETTABWIDTH, PsVal(gConfig.TabSize), 0 )
        SciMsg( pSciView, SCI_SETINDENT, PsVal(gConfig.TabSize), 0 )
    
        ''
        ''  INDENTATION GUIDES
        if gConfig.IndentGuides then
            SciMsg( pSciView, SCI_SETINDENTATIONGUIDES, Ctrue, 0)
        else
            SciMsg( pSciView, SCI_SETINDENTATIONGUIDES, false, 0)
        end if
        SciMsg( pSciView, SCI_STYLESETFORE, STYLE_INDENTGUIDE, theme.editor.indentguides.forecolor )
        SciMsg( pSciView, SCI_STYLESETBACK, STYLE_INDENTGUIDE, theme.editor.indentguides.backcolor )
        
        ''
        ''  CARET
        SciMsg( pSciView, SCI_SETCARETFORE, theme.editor.caret, 0 )
        SciMsg( pSciView, SCI_SETCARETWIDTH, 3, 0 )   '(1,2,3  Default is 1)

        ''
        ''  SHOW CARET LINE
        if gConfig.HighlightCurrentLine then
            SciMsg( pSciView, SCI_SETCARETLINEVISIBLE, Ctrue, 0 )
            SciMsg( pSciView, SCI_SETCARETLINEBACK, theme.editor.currentline, 0 )
        else
            SciMsg( pSciView, SCI_SETCARETLINEVISIBLE, false, 0 )
        end if
        
        ''
        ''  SELECTION COLORS
        SciMsg( pSciView, SCI_SETSELFORE, Ctrue, theme.editor.selection.forecolor )
        SciMsg( pSciView, SCI_SETSELBACK, Ctrue, theme.editor.selection.backcolor )
    
        ''
        ''  MULTIPLE SELECTIONS
        SciMsg( pSciView, SCI_SETMULTIPLESELECTION, Ctrue, 0 ) 
                
        ''
        ''  ALWAYS KEEP THE CARET LINE VISIBLE
        SciMsg( pSciView, SCI_SETCARETLINEVISIBLEALWAYS, Ctrue, 0 )
        
        ''
        ''  DISABLE RIGHT CLICK POPUP MENU
        SciMsg( pSciView, SCI_USEPOPUP, false, 0 )

        ''
        ''  IDENTIFY CHARACTERS to BE USED IN WORDS
        SciMsg( pSciView, SCI_SETWORDCHARS, 0, cast(integer, @"~_\abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") )


        ''  UNICODE (UTF-8 encoding)
        if nEncoding = FILE_ENCODING_ANSI then
            SciMsg( pSciView, SCI_SETCODEPAGE, 0, 0 )
        else
            ' UTF8 or UTF16 would have been converted to UTF8 in order to display in the editor.
            SciMsg( pSciView, SCI_SETCODEPAGE, SC_CP_UTF8, 0 )
        end if

    
        '' RIGHT EDGE COLUMN
        SciMsg( pSciView, SCI_SETEDGEMODE, iif(gConfig.RightEdge, EDGE_LINE, EDGE_NONE), 0 )
        SciMsg( pSciView, SCI_SETEDGECOLUMN, PsVal(gConfig.RightEdgePosition), 0 )
        ' SCI_SETEDGECOLOUR was never sent anywhere, so the right-margin guide drew in
        ' Scintilla's built-in #C0C0C0 in every theme -- a light grey line down a dark
        ' editor. It has a key now.
        SciMsg( pSciView, SCI_SETEDGECOLOUR, theme.editor.edge, 0 )
        ' Likewise visible whitespace and the multi-cursor selections/carets: all four of
        ' these messages were absent, so they used Scintilla's defaults regardless of theme.
        SciMsg( pSciView, SCI_SETWHITESPACEFORE, 1, theme.editor.whitespace.forecolor )
        SciMsg( pSciView, SCI_SETWHITESPACEBACK, 1, theme.editor.whitespace.backcolor )
        SciMsg( pSciView, SCI_SETADDITIONALSELBACK, theme.editor.additionalselection, 0 )
        SciMsg( pSciView, SCI_SETADDITIONALCARETFORE, theme.editor.additionalcaret, 0 )

        ''
        ''  OTHER
        SciMsg( pSciView, SCI_SETADDITIONALSELECTIONTYPING, true, 0 )
    

        ''
        ''  APPLY ALL LANGUAGE SPECIFIC SYNTAX COLORING
        wFileExt = *(PsPathExt(sDiskFilename )).Wz()
        wFileExt = ucase(wFileExt)

        if cbool(wFileExt = ".BAS") orelse cbool(wFileExt = ".INC") _
            orelse cbool(wFileExt = ".BI") orelse (bIsNew = true) then

            bitsNeeded = SciMsg( pSciView, SCI_GETSTYLEBITSNEEDED, 0, 0)
            SciMsg( pSciView, SCI_SETSTYLEBITS, bitsNeeded, 0 )

            ' Set FreeBASIC Keywords
            if len(gConfig.FBKeywords) then
                SciMsg( pSciView, SCI_SETKEYWORDS, 0, cast(integer, strptr(gConfig.FBKeywords)) )
            end if

            ' Set Windows Api Keywords
            if len(gConfig.WinApiKeywords) then
                SciMsg( pSciView, SCI_SETKEYWORDS, 1, cast(integer, strptr(gConfig.WinApiKeywords)) )
            end if

            ' Set Extra Keywords
            if len(gConfig.ExtraKeywords) then
                SciMsg( pSciView, SCI_SETKEYWORDS, 2, cast(integer, strptr(gConfig.ExtraKeywords)) )
            end if

            ' Canonical spellings for SC_CASE_PROPER ("Proper Case" in Options).
            ' The SAME text the lexer gets: the keyword files carry proper casing and
            ' the lexer lower-cases both the token and its word list before matching,
            ' so one set of files serves highlighting and rendering alike. A keyword
            ' whose file spelling is still lower case simply falls back to camel case.
            ' Sent unconditionally so that clearing the option, or emptying a keyword
            ' file, actually removes the old spellings rather than leaving them behind.
            dim as string strCaseKeywords = _
                gConfig.FBKeywords & " " & gConfig.WinApiKeywords & " " & gConfig.ExtraKeywords
            SciMsg( pSciView, SCI_SETCASEKEYWORDS, 0, cast(integer, strptr(strCaseKeywords)) )


            if gConfig.SyntaxHighlighting then
            ' Set the Default text colors
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_DEFAULT, theme.editor.text.forecolor)
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_DEFAULT, theme.editor.text.backcolor)
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_DEFAULT, theme.editor.text.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_DEFAULT, theme.editor.text.italic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_DEFAULT, theme.editor.text.underline )

            ' Set the Multiline Comments style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_MULTILINECOMMENT, theme.editor.comments.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_MULTILINECOMMENT, theme.editor.comments.backcolor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_MULTILINECOMMENT, theme.editor.comments.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_MULTILINECOMMENT, theme.editor.comments.italic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_MULTILINECOMMENT, theme.editor.comments.underline )

            ' Set the Comments style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_COMMENT, theme.editor.comments.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_COMMENT, theme.editor.comments.backcolor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_COMMENT, theme.editor.comments.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_COMMENT, theme.editor.comments.italic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_COMMENT, theme.editor.comments.underline )

            ' Set the Keywords style (FreeBasic)
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_KEYWORD, theme.editor.keyword1.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_KEYWORD, theme.editor.keyword1.backcolor )
            SciMsg( pSciView, SCI_STYLESETCASE, SCE_B_KEYWORD, nFontcase )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_KEYWORD, theme.editor.keyword1.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_KEYWORD, theme.editor.keyword1.italic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_KEYWORD, theme.editor.keyword1.underline )

            ' Set the Keywords style (Windows Api)
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_KEYWORD2, theme.editor.keyword2.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_KEYWORD2, theme.editor.keyword2.backcolor )
            SciMsg( pSciView, SCI_STYLESETCASE, SCE_B_KEYWORD2, nFontcase )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_KEYWORD2, theme.editor.keyword2.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_KEYWORD2, theme.editor.keyword2.italic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_KEYWORD2, theme.editor.keyword2.underline )

            ' Set the Keywords style (Extra Keywords)
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_KEYWORD3, theme.editor.keyword3.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_KEYWORD3, theme.editor.keyword3.backcolor )
            SciMsg( pSciView, SCI_STYLESETCASE, SCE_B_KEYWORD3, nFontcase )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_KEYWORD3, theme.editor.keyword3.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_KEYWORD3, theme.editor.keyword3.italic )
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_KEYWORD3, theme.editor.keyword3.underline )

            ' Set the Numbers style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_NUMBER, theme.editor.numbers.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_NUMBER, theme.editor.numbers.backcolor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_NUMBER, theme.editor.numbers.bold )
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_NUMBER, theme.editor.numbers.italic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_NUMBER, theme.editor.numbers.underline)

            ' Set the Operators style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_OPERATOR, theme.editor.operators.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_OPERATOR, theme.editor.operators.backcolor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_OPERATOR, theme.editor.operators.bold)
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_OPERATOR, theme.editor.operators.italic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_OPERATOR, theme.editor.operators.underline)

'            ' Set the "date" style. We use this for @()[]{} brace characters
'            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_DATE, theme.editor.bracegood)
'            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_DATE, theme.editor.text.backcolor)
'            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_DATE, theme.editor.text.bold )
'            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_DATE, theme.editor.text.italic )
'            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_DATE, theme.editor.text.underline )

            ' Set the Preprocessor style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_PREPROCESSOR, theme.editor.preprocessor.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_PREPROCESSOR, theme.editor.preprocessor.backcolor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_PREPROCESSOR, theme.editor.preprocessor.bold)
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_PREPROCESSOR, theme.editor.preprocessor.italic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_PREPROCESSOR, theme.editor.preprocessor.underline)

            ' Set the strings style
            SciMsg( pSciView, SCI_STYLESETFORE, SCE_B_STRING, theme.editor.strings.forecolor )
            SciMsg( pSciView, SCI_STYLESETBACK, SCE_B_STRING, theme.editor.strings.backcolor )
            SciMsg( pSciView, SCI_STYLESETBOLD, SCE_B_STRING, theme.editor.strings.bold)
            SciMsg( pSciView, SCI_STYLESETITALIC, SCE_B_STRING, theme.editor.strings.italic)
            SciMsg( pSciView, SCI_STYLESETUNDERLINE, SCE_B_STRING, theme.editor.strings.underline)

            end if
        end if
    
        ''
        ''  CODE FOLDING
        if gConfig.FoldMargin then
            ' Enable folding of the procedures and functions
            SciMsg( pSciView, SCI_SETPROPERTY, cast(uinteger, @"fold"), cast(integer, @"1") )

            ' Initialize fold symbols for folding - Box tree
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN,    SC_MARK_BOXMINUS )
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER,        SC_MARK_BOXPLUS )
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB,     SC_MARK_VLINE)
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL,    SC_MARK_LCORNER)
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND,     SC_MARK_BOXPLUSCONNECTED)
            'SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY)   ' SC_MARK_BOXMINUSCONNECTED
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED)
            SciMsg( pSciView, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER)

            ' Draw line below if not expanded
            SciMsg( pSciView, SCI_SETFOLDFLAGS, 16, 0 )

            ' Colors for folders closed and folders opened
            dim as COLORREF clrFore = theme.editor.foldsymbol.forecolor
            dim as COLORREF clrBack = theme.editor.foldsymbol.backcolor

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, clrFore)

            SciMsg( pSciView, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, clrBack)
            SciMsg( pSciView, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, clrFore)

            SciMsg( pSciView, SCI_MARKERENABLEHIGHLIGHT, false, 0)
        
        else
            ' Disable folding of the procedures and functions
            SciMsg( pSciView, SCI_SETPROPERTY, cast(uinteger, @"fold"), cast(integer, @"0") )
        end if
    
end sub
