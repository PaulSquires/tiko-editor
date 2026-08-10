'' ========================================================================================
'' mdHilite -- fence language to coloured spans. NO UI, NO PAINTING, NO PLATFORM CALLS.
''
'' ---- WHY ONLY TWO LANGUAGES ------------------------------------------------------------
''
'' The corpus is FreeBASIC and C. Every fence in the AfxNova, PsControls and tiko docsets is
'' one of `freebasic`, `fb`, `basic`, `c`, `cpp` or empty, and an unknown language renders as
'' plain text rather than guessed at -- a wrong highlight in a help page is worse than none,
'' because the reader takes it as a statement about the code.
''
'' ---- LINE AT A TIME, WITH A CARRIED STATE ---------------------------------------------
''
'' The renderer paints one line at a time, so this tokenises one line at a time. Block
'' comments are the reason nState exists: /* in C and /' in FreeBASIC both survive a newline,
'' and a per-line tokeniser with no memory reports the second line of a comment as code. Feed
'' the same variable back in for every line of a block, starting at MDH_STATE_NONE.
''
'' ---- KINDS ARE THEME ROLES, NOT COLOURS -----------------------------------------------
''
'' MDT_* map one-for-one onto PSTHEME_KEYWORD / COMMENT / STRING / NUMBER / OPERATOR /
'' PREPROCESSOR, which PsTheme already carries and every tiko .theme file already sets. This
'' header names no colour for that reason -- resolution is the renderer's job in phase 3.
'' ========================================================================================

#pragma once

enum MdTokKind
    MDT_TEXT = 0
    MDT_KEYWORD
    MDT_COMMENT
    MDT_STRING
    MDT_NUMBER
    MDT_OPERATOR
    MDT_PREPROC
end enum

enum MdLangId
    MDL_NONE = 0
    MDL_FB
    MDL_C
end enum

'' Carried between lines. Only block comments span one.
const MDH_STATE_NONE    = 0
const MDH_STATE_COMMENT = 1

type MdSpan
    nStart as long        '' byte offset into the line
    nLen   as long
    nKind  as long
end type

'' Fence info string -> language. Case-insensitive; anything unrecognised is MDL_NONE.
declare function MdLangFromName( byref sLang as string ) as long

'' Tokenise ONE line. Returns the number of spans written, which is never more than nMax --
'' a line that would need more is truncated and the REMAINDER IS RETURNED AS ONE MDT_TEXT
'' SPAN rather than dropped, so no character of the source can go missing.
''
'' The spans tile the line: they are contiguous, in order, and cover every byte. The
'' renderer can therefore walk them without checking for gaps.
declare function MdHilite( byval nLang as long, byref sLine as string, _
                           byref nState as long, spans() as MdSpan, _
                           byval nMax as long ) as long
