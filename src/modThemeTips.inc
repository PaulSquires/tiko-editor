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

'  modThemeTips.inc  --  the two tooltip colour recipes, each written ONCE.
'
'  There were four copies of a five-line recipe: frmThemes, frmUserTools, frmDebug and
'  modCodetipTip. That is the situation this file exists to end, and the copies had already
'  done what copies do -- frmDebug titled its data tip in editor.keyword1 while modCodetipTip
'  titled its code tip in editor.text, with frmDebug's comment claiming "same call the code
'  tips make, for the same reason". It was not the same call.
'
'  TWO RECIPES, NOT ONE, and the difference is deliberate rather than drift:
'
'    Theme_FillTooltipColors  the ordinary hover tip. Reads the tooltip.* keys, which are
'                             roled onto backgroundraised -- a tip that floats over the
'                             application chrome should look like the menus and popups.
'
'    Theme_FillCodeTipColors  a tip that floats over SOURCE TEXT: the code tip and the
'                             debugger's data tip. Reads the EDITOR palette on purpose, so it
'                             reads as part of the code it is explaining rather than as a
'                             piece of chrome laid on top of it.
'
'  Both are here rather than in modThemeApply.inc because PSTOOLTIP_COLORS is declared by
'  PsTooltip.bi and all four callers are included well before the apply layer. This file sits
'  immediately after PsTooltip.inc for that reason -- the same placement, and the same
'  reasoning, as modCodetipTip.inc just below it.

#pragma once


''
''  THE ORDINARY HOVER TIP
''
sub Theme_FillTooltipColors( byref tipc as PSTOOLTIP_COLORS )
    tipc.BackColor   = theme.tooltip.text.backcolor
    tipc.BorderColor = theme.tooltip.border
    tipc.ForeColor   = theme.tooltip.text.forecolor
    tipc.TitleColor  = theme.tooltip.title
    tipc.GlyphColor  = theme.tooltip.glyph
end sub


''
''  A TIP THAT FLOATS OVER SOURCE TEXT
''
''  The code tip and the debugger's data tip. Editor palette, so the tip reads as part of the
''  code rather than as chrome on top of it -- which is why this does NOT go through the
''  tooltip.* keys even though it is the same control.
''
''  The title is the keyword colour: a code tip's title is a signature, and giving it the
''  plain text colour (which is what modCodetipTip did) makes the whole tip one flat block.
''  The two callers disagreed about this line before; they cannot now.
''
sub Theme_FillCodeTipColors( byref tipc as PSTOOLTIP_COLORS )
    tipc.BackColor   = theme.editor.text.backcolor
    tipc.BorderColor = theme.editor.divider
    tipc.ForeColor   = theme.editor.text.forecolor
    tipc.TitleColor  = theme.editor.keyword1.forecolor
    tipc.GlyphColor  = theme.editor.text.forecolor
end sub
