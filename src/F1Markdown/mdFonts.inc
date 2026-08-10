'' ========================================================================================
'' mdFonts.inc -- the implementation. See mdFonts.bi for why there are ten.
'' ========================================================================================

#pragma once
#include once "mdFonts.bi"

'' Design pixel sizes. The heading ramp is deliberately shallow -- these are reference pages
'' read at arm's length, not a landing page, and an h1 three times the body size wastes the
'' fold on every topic.
private const MDF_PX_BODY = 15
private const MDF_PX_MONO = 14      '' one less: Cascadia's x-height runs large beside Source Sans
private const MDF_PX_H1   = 26
private const MDF_PX_H2   = 21
private const MDF_PX_H3   = 18
private const MDF_PX_H4   = 16

dim shared as PsTextEngine g_mdFont(0 to MDF_COUNT - 1)
dim shared as string       g_mdFontFile(0 to MDF_COUNT - 1)
dim shared as long         g_mdFontDesign(0 to MDF_COUNT - 1)
dim shared as long         g_mdFontPx(0 to MDF_COUNT - 1)
dim shared as boolean      g_mdFontOpen(0 to MDF_COUNT - 1)
dim shared as string       g_mdFontDir
dim shared as string       g_mdFontMissing

private sub SetFace( byval nId as long, byref sFile as string, byval nDesign as long )
    g_mdFontFile(nId)   = sFile
    g_mdFontDesign(nId) = nDesign
    g_mdFontPx(nId)     = 0
    g_mdFontOpen(nId)   = false
end sub

'' One engine, at a scaled size. Returns false and records the file when FreeType declines it.
private function OpenOne( byval nId as long, byval nPx as long ) as boolean
    if g_mdFontOpen(nId) then
        if g_mdFontPx(nId) = nPx then return true
        TE_Free( g_mdFont(nId) )
        g_mdFontOpen(nId) = false
    end if
    dim as string sPath = g_mdFontDir & g_mdFontFile(nId)
    if TE_Init( g_mdFont(nId), strptr(sPath), nPx ) = 0 then
        if len(g_mdFontMissing) > 0 then g_mdFontMissing &= ", "
        g_mdFontMissing &= g_mdFontFile(nId)
        return false
    end if
    g_mdFontPx(nId)   = nPx
    g_mdFontOpen(nId) = true
    return true
end function

function MdFontsInit( byref sDir as string, byval fScale as single ) as boolean
    g_mdFontDir     = sDir
    g_mdFontMissing = ""

    SetFace( MDF_BODY,    "SourceSans3-Regular.ttf",  MDF_PX_BODY )
    SetFace( MDF_BODY_B,  "SourceSans3-Bold.ttf",     MDF_PX_BODY )
    SetFace( MDF_BODY_I,  "SourceSans3-It.ttf",       MDF_PX_BODY )
    SetFace( MDF_BODY_BI, "SourceSans3-BoldIt.ttf",   MDF_PX_BODY )
    SetFace( MDF_MONO,    "CascadiaMono-Regular.ttf", MDF_PX_MONO )
    SetFace( MDF_MONO_B,  "CascadiaMono-Bold.ttf",    MDF_PX_MONO )
    SetFace( MDF_H1,      "SourceSans3-Bold.ttf",     MDF_PX_H1 )
    SetFace( MDF_H2,      "SourceSans3-Bold.ttf",     MDF_PX_H2 )
    SetFace( MDF_H3,      "SourceSans3-Bold.ttf",     MDF_PX_H3 )
    SetFace( MDF_H4,      "SourceSans3-Bold.ttf",     MDF_PX_H4 )

    return MdFontsApplyScale( fScale )
end function

sub MdFontsFree()
    for i as long = 0 to MDF_COUNT - 1
        if g_mdFontOpen(i) then
            TE_Free( g_mdFont(i) )
            g_mdFontOpen(i) = false
        end if
    next
end sub

function MdFontsApplyScale( byval fScale as single ) as boolean
    dim as single f = fScale
    if f <= 0 then f = 1.0
    g_mdFontMissing = ""
    dim as boolean bAll = true
    for i as long = 0 to MDF_COUNT - 1
        '' OpenOne short-circuits when the pixel size is unchanged, so a resize inside one
        '' display does not rebuild ten atlases for nothing.
        if OpenOne( i, PsScaleBy(g_mdFontDesign(i), f) ) = false then bAll = false
    next
    return bAll
end function

function MdFontPtr( byval nId as long ) as PsTextEngine ptr
    if (nId < 0) orelse (nId >= MDF_COUNT) then return @g_mdFont(MDF_BODY)
    if g_mdFontOpen(nId) = false then return @g_mdFont(MDF_BODY)
    return @g_mdFont(nId)
end function

function MdFontPx( byval nId as long ) as long
    if (nId < 0) orelse (nId >= MDF_COUNT) then return 0
    return g_mdFontPx(nId)
end function

function MdFontForInline( byval nStyle as long ) as long
    '' CODE FIRST, and it is not a preference: the set has no monospace italic, so `**a**`
    '' inside a fence-styled span has nowhere else to go. Bold code does exist, so that pair
    '' is honoured.
    if (nStyle and MDS_CODE) <> 0 then
        if (nStyle and MDS_BOLD) <> 0 then return MDF_MONO_B
        return MDF_MONO
    end if
    dim as boolean bB = ((nStyle and MDS_BOLD) <> 0)
    dim as boolean bI = ((nStyle and MDS_ITALIC) <> 0)
    if bB andalso bI then return MDF_BODY_BI
    if bB then return MDF_BODY_B
    if bI then return MDF_BODY_I
    return MDF_BODY
end function

function MdFontForHeading( byval nLevel as long ) as long
    select case nLevel
        case 1 : return MDF_H1
        case 2 : return MDF_H2
        case 3 : return MDF_H3
        case 4 : return MDF_H4
    end select
    '' h5 and h6 are body-bold. Anything smaller than the body text would be unreadable and
    '' the corpus barely uses them -- five occurrences across 181 files.
    return MDF_BODY_B
end function

function MdFontsWorstStatFull() as long
    dim as long nWorst = 0
    for i as long = 0 to MDF_COUNT - 1
        if g_mdFontOpen(i) then
            if g_mdFont(i).statFull > nWorst then nWorst = g_mdFont(i).statFull
        end if
    next
    return nWorst
end function

function MdFontsMissing() as string
    return g_mdFontMissing
end function
