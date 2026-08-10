'' ========================================================================================
'' mdConfig -- the docset list, and the settings that survive a restart.
''
'' ---- TWO FILES, TWO PLACES, AND THE SPLIT MATTERS -------------------------------------
''
'' f1markdown.ini sits BESIDE THE EXE. It says what documentation this installation shows,
'' it is written by whoever deploys the thing, and it is the same for every user of it.
''
'' settings.ini sits in PSFOLDER_CONFIG. It is per-user, rewritten constantly, and losing it
'' costs a window size. Putting it beside the exe would mean two users of one install fighting
'' over each other's splitter position, and an install under Program Files failing to save at
'' all -- which is the same reason the index cache lives under PSFOLDER_CACHE.
''
'' ---- WHAT IS NOT PERSISTED, AND WHY ---------------------------------------------------
''
'' THE WINDOW POSITION. IWindowBackend has SetSize and GetSize and no position pair for a
'' toplevel at all -- SetPopupPosition exists only for popups. That is not an oversight to be
'' worked around: Wayland does not let a client place its own windows, so a cross-platform
'' toolkit cannot offer it and an application that faked it on Windows alone would behave
'' differently on the two platforms for no gain. The size is restored; the compositor places
'' the window.
''
'' ---- SIZES ARE STORED IN DESIGN UNITS -------------------------------------------------
''
'' Not pixels. Saved on a 175% display and restored on a 100% one, a pixel size would come
'' back a third too small -- and that is the exact bug the whole DPI discipline in this
'' program exists to prevent, so it would be embarrassing to reintroduce it through the
'' settings file.
'' ========================================================================================

#pragma once

type MdDocset
    sName as string
    sPath as DWSTRING       '' absolute, canonicalised
end type

type MdSettings
    nWinW     as long       '' DESIGN units; 0 = never saved, use the built-in default
    nWinH     as long
    nTocW     as long       '' DESIGN units
    sTheme    as DWSTRING   '' a .theme path; empty = the built-in default
    sLastPage as DWSTRING   '' absolute path of the document that was open
end type

'' Reads f1markdown.ini from sExeDir. Relative docset paths resolve against that folder, so a
'' deployment can be moved without editing it. Returns how many roots were configured.
''
'' WRITES A COMMENTED STARTER when the file is absent and the folder is writable, because a
'' viewer that shows nothing and explains nothing is indistinguishable from a broken one.
declare function MdConfigLoadDocsets( ds() as MdDocset, byref sExeDir as DWSTRING ) as long

declare function MdSettingsPath() as DWSTRING
declare sub MdSettingsLoad( byref st as MdSettings )
declare sub MdSettingsSave( byref st as MdSettings )
