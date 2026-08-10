'' ========================================================================================
'' modSciText -- declarations. See modSciText.inc for why these take a view POINTER rather
'' than an editor window, and for the repaint the caller now owns.
'' ========================================================================================

#pragma once

'' The full byte extent of a buffer, NUL bytes included. Read-only.
declare function Scintilla_GetTextBytes( byval pSci as any ptr ) as string

'' Trims end-of-line whitespace in place and returns the number of lines changed.
'' THE CALLER MUST REPAINT THE VIEW afterwards -- gAppHost.InvalidateView. The SciExec macro
'' this used to be written with flushed damage on every message; SciMsg does not.
declare function Scintilla_StripTrailingWhitespace( byval pSci as any ptr ) as long
