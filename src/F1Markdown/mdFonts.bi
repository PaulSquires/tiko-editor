'' ========================================================================================
'' mdFonts -- the ten text engines a markdown page needs.
''
'' ---- WHY TEN -------------------------------------------------------------------------
''
'' ONE PsTextEngine IS ONE FONT FILE AT ONE PIXEL SIZE. TE_Init is FT_New_Face(face 0) plus
'' FT_Set_Pixel_Sizes; there is no bold flag, no italic flag, no family name, no TE_SetSize.
'' A markdown page mixes body, bold, italic, bold-italic, monospace, monospace-bold and four
'' heading sizes on one screen, so that is ten engines, and there is no arrangement of fewer.
''
'' Cost: each carries a 1024x1024 A8 atlas, so about 10 MB resident. That is the price of
'' the design and it is paid once. What it must NOT do is overflow -- te.statFull counts
'' refused insertions and the self-test asserts it is zero after rendering the largest page
'' in the corpus, because an exhausted atlas drops glyphs silently.
''
'' ---- WHY NOT PsPlatform'S OWN FONT -----------------------------------------------------
''
'' assets/fonts/CascadiaCode.ttf is a VARIABLE font and TE_Init sets no variation axis, so it
'' renders its default instance whatever you ask for. A "bold" opened from it is not bold and
'' nothing errors. The six static faces beside tiko.exe exist for that reason alone.
''
'' ---- SCALE ---------------------------------------------------------------------------
''
'' Sizes below are DESIGN pixels. MdFontsApplyScale reopens every engine at PsScaleBy(design,
'' f) -- TE_Free then TE_Init, because there is no other way to change a face's size. That is
'' step 1 of the four-part DPI change, and skipping it leaves perfectly scaled bands full of
'' unscaled text.
'' ========================================================================================

#pragma once

enum MdFontId
    MDF_BODY = 0        '' Source Sans 3 Regular
    MDF_BODY_B          '' Bold        -- also H5 and H6
    MDF_BODY_I          '' Italic
    MDF_BODY_BI         '' Bold Italic
    MDF_MONO            '' Cascadia Mono Regular -- code fences and `inline code`
    MDF_MONO_B          '' Cascadia Mono Bold
    MDF_H1
    MDF_H2
    MDF_H3
    MDF_H4
    MDF_COUNT
end enum

'' Opens all ten from sDir, at the given scale. False if ANY face is missing -- a viewer
'' with nine faces would render one style as nothing, which reads as a content bug.
declare function MdFontsInit( byref sDir as string, byval fScale as single ) as boolean
declare sub MdFontsFree()

'' TE_Free + TE_Init on every engine at the new scale. A no-op when no size actually changes,
'' so a resize that does not cross a DPI boundary does not rebuild ten atlases.
declare function MdFontsApplyScale( byval fScale as single ) as boolean

declare function MdFontPtr( byval nId as long ) as PsTextEngine ptr
declare function MdFontPx( byval nId as long ) as long

'' MDS_* style bits -> the engine that renders them. MDS_CODE wins over bold and italic,
'' because there is no monospace italic in the set and a fence is not the place to invent one.
declare function MdFontForInline( byval nStyle as long ) as long

'' 1..6 -> MDF_H1..MDF_H4, then MDF_BODY_B for 5 and 6.
declare function MdFontForHeading( byval nLevel as long ) as long

'' The worst atlas pressure across all ten. The self-test asserts this is 0.
declare function MdFontsWorstStatFull() as long

'' Which face failed to open, for the error message. Empty when all ten are up.
declare function MdFontsMissing() as string
