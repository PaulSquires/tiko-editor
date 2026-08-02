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
' CODE FORMATTER -- declarations.
'
' This file is included AHEAD of clsConfig.bi because clsConfig embeds a FORMAT_RULES.
' It therefore names no Ps* type, no clsDocument, and nothing from Scintilla: it must
' compile with only modDeclares.bi ahead of it.
'
' The engine itself (modFormat.inc) is PURE TEXT IN, TEXT OUT. It never touches a window,
' a document or a Scintilla view. That is the load-bearing decision in this feature: it
' means the Format Options live preview, the self-test, and the offline Format Project
' path all call exactly the same function the editor does, so the preview cannot drift
' from what actually happens to your file.
' ========================================================================================

#pragma once


' ----------------------------------------------------------------------------------------
' Token kinds. The token list is LOSSLESS: every byte of the input lands in exactly one
' token, so concatenating an untouched list reproduces the input byte for byte. That is
' what makes the safety checks in FMT_TokensMatch cheap -- comparing the non-whitespace
' token stream before and after is a complete statement that the program did not change,
' with no AST anywhere in sight.
' ----------------------------------------------------------------------------------------
enum
    FMTTOK_WS = 0          ' runs of spaces and tabs
    FMTTOK_WORD            ' identifier or keyword
    FMTTOK_NUMBER          ' 123  1.5  1e-5  &hFF  &o17  &b1010
    FMTTOK_STRING          ' "..."  including the !"..." and $"..." prefixed forms
    FMTTOK_COMMENT         ' ' to end of line, and a Rem statement
    FMTTOK_MLCOMMENT       ' /' ... '/  (FreeBASIC nests these, so the lexer counts depth)
    FMTTOK_OP              ' punctuation and operators
    FMTTOK_CONT            ' the trailing _ line-continuation marker
    FMTTOK_DIRECTIVE       ' a # or $ directive NAME, e.g. #include -- not the whole line
    FMTTOK_EOL             ' vbCrLf, chr(10) or chr(13)
end enum


' Casing modes. Applied independently to the four vocabularies below.
enum
    FMTCASE_LEAVE = 0      ' do not touch
    FMTCASE_LOWER          ' dim, if, screenres
    FMTCASE_UPPER          ' DIM, IF, SCREENRES
    FMTCASE_PROPER         ' Dim, If, ScreenRes  -- needs the casing table, see below
end enum


' Parenthesis interior spacing.
enum
    FMTPAREN_LEAVE = 0
    FMTPAREN_ADD           ' Foo( a, b )   -- tiko's and AfxNova's own house style
    FMTPAREN_REMOVE        ' Foo(a, b)
end enum


' Which vocabulary a word belongs to. Resolved once per word by FMT_ClassifyWord.
enum
    FMTVOCAB_NONE = 0      ' an ordinary identifier we know nothing about
    FMTVOCAB_KEYWORD       ' a reserved word from the lexer's keyword list
    FMTVOCAB_DIRECTIVE     ' a # or $ compiler directive
    FMTVOCAB_TYPE          ' an intrinsic type name (integer, zstring, ...)
end enum


' ----------------------------------------------------------------------------------------
' The rule set. One instance lives on clsConfig and is persisted to settings.ini; a second
' is the dialog's staging copy so Cancel is a true no-op.
'
' Every field is `as long` and NOT `boolean`, deliberately and for the same reason
' clsConfig's own flags are (see the AskExit note in clsConfig.bi): SaveConfigFile emits
' each field verbatim, and a real boolean would write "true"/"false" into the ini where
' every reader expects 0/1.
'
' The "leave alone" sentinel differs per field and is documented on each, because a rule
' that is off must be distinguishable from a rule that is on with a zero parameter.
' ----------------------------------------------------------------------------------------
type FORMAT_RULES
    ' -- Indentation ---------------------------------------------------------------------
    Reindent              as long = true   ' rewrite every line's leading whitespace
    IndentCase            as long = true   ' Case labels get a level inside Select Case
    ContinuationIndent    as long = 1      ' extra levels for a line continued with _
    ' -- Casing --------------------------------------------------------------------------
    CaseKeywords          as long = FMTCASE_LEAVE
    CaseDirectives        as long = FMTCASE_LEAVE
    CaseTypes             as long = FMTCASE_LEAVE
    ' -- Spacing -------------------------------------------------------------------------
    SpaceAroundOperators  as long = true
    SpaceAfterComma       as long = true
    ParenSpacing          as long = FMTPAREN_LEAVE
    SpacesBeforeComment   as long = 0      ' 0 = leave trailing-comment spacing alone
    ' -- Blank lines and whitespace ------------------------------------------------------
    TrimTrailing          as long = true
    MaxBlankLines         as long = 0      ' 0 = leave consecutive blank lines alone
    BlankLinesAroundProc  as long = -1     ' -1 = leave alone. This is the ONLY rule that
                                           ' changes the line count -- see FMT_TokensMatch.
    ' -- Triggers ------------------------------------------------------------------------
    ' Deliberately stored beside the rules but presented on their own dialog page: every
    ' IDE surveyed separates WHAT the rules are from WHEN they are applied.
    FormatOnEnter         as long = false
    FormatOnPaste         as long = false  ' ships OFF -- a paste can produce a large and
                                           ' surprising diff, so the user opts in
end type


' ----------------------------------------------------------------------------------------
' One token of the lossless stream.
' ----------------------------------------------------------------------------------------
type FMTTOKEN
    kind      as long
    text      as string
end type


' ----------------------------------------------------------------------------------------
' Engine entry points (modFormat.inc). All pure: text in, text out.
' ----------------------------------------------------------------------------------------

' The formatter proper. Returns the reformatted text. On any internal safety failure it
' returns sText UNCHANGED and sets bChanged false, so a caller that ignores the flag still
' cannot corrupt a buffer.
declare function Format_Text( byref sText as string, _
                              byref rules as FORMAT_RULES, _
                              byref bChanged as long, _
                              byval nIndentWidth as long = 4, _
                              byval bUseTabs as long = false ) as string

' Format a contiguous run of lines while knowing the block depth they sit at. This is what
' Format Selection and the paste hook use: the depth cannot be recovered from the fragment
' alone, so the caller computes it from the top of the file and passes it in.
declare function Format_TextAtDepth( byref sText as string, _
                                     byref rules as FORMAT_RULES, _
                                     byval nStartDepth as long, _
                                     byref bChanged as long, _
                                     byval nIndentWidth as long = 4, _
                                     byval bUseTabs as long = false ) as string

' Block depth at the START of each line of sText, 0-based by line. Used to seed
' Format_TextAtDepth and by the on-Enter hook.
declare function Format_DepthAtLine( byref sText as string, byval nLine as long ) as long

' Lex sText into a lossless token array. Returns the token count.
declare function FMT_Lex( byref sText as string, tokens() as FMTTOKEN ) as long

' The two safety checks. FMT_TokensMatch compares the non-whitespace token streams of two
' texts; bIgnoreEOL widens it to also ignore line breaks, which the blank-line rule
' legitimately inserts and deletes.
declare function FMT_TokensMatch( byref sBefore as string, _
                                  byref sAfter as string, _
                                  byval bIgnoreEOL as long ) as long

' Reset the cached keyword vocabulary. Called when gConfig's keyword lists change.
declare sub FMT_InvalidateVocabulary()

' Default rules, for the dialog's Reset button and for the self-test's fixtures.
declare function Format_DefaultRules() as FORMAT_RULES

' Env-gated self-test. Returns the number of FAILED assertions.
declare function Format_RunSelfTest() as long
