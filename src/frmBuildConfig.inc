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

#include once "frmBuildConfig.bi"
#include once "clsConfig.bi"
#include once "clsTopTabCtl.bi"
#include once "clsDocument.bi"


' ----------------------------------------------------------------------------------------
' Child handles. Every one of these is zeroed in WM_DESTROY -- the dialog is created and
' destroyed per invocation, and a stale handle here outlives it.
' ----------------------------------------------------------------------------------------
dim shared as HWND ghBCAdd, ghBCDelete, ghBCUp, ghBCDown
dim shared as HWND ghBCSelectBar
dim shared as HWND ghBCDescription, ghBCTogDefault, ghBCTarget, ghBCShortcut
dim shared as HWND ghBCTogExclude, ghBCEditSwitches
dim shared as HWND ghBCOptions, ghBCSwitches
dim shared as HWND ghBCOK, ghBCCancel

' A fixed-pitch face at UI size, for the raw options field and the preview block. There is no
' cached ghFont entry for one -- FIXEDFONT_26 is the About box's wordmark. Created in Show and
' destroyed after the pump.
dim shared as HFONT ghBCFontFixed

' ----------------------------------------------------------------------------------------
' Dialog state.
' ----------------------------------------------------------------------------------------
' Which gBuildsWork() row the right pane is showing. -1 = none, and the pane is HIDDEN.
dim shared gBCCurrent as long = -1
' TRUE while the pane is being filled from the model, so the change callbacks that fire as a
' side effect of setting a control do not write straight back into it.
dim shared gBCLoading as boolean = false
' Which page of the right pane is up: IDP_FRMBUILDCONFIG_GENERAL or _SWITCHES.
dim shared gBCPage as long = IDP_FRMBUILDCONFIG_GENERAL

' ----------------------------------------------------------------------------------------
' The compiler-switches PREVIEW BLOCK.
'
' It is PAINTED by the dialog and hit-tested here, not built as a child window: a control
' would cost a CWindow, a WndProc and a WM_CTLCOLOR path for a target that does exactly one
' thing (jump to the switches page). Its rects are computed in PositionWindows because both
' the painter and the mouse handlers need them, and only one of those can derive them.
' ----------------------------------------------------------------------------------------
dim shared gBCPreviewRect      as RECT
dim shared gBCPreviewLabelRect as RECT
dim shared gBCPreviewHot       as boolean = false

' One label rect per General-page row, indexed by BCROW_*. Derived in PositionWindows from
' where each control actually landed, and read back by the painter -- so a label can never
' disagree with the row it names, and each gets exactly the width its own control leaves.
dim shared gBCRowLabelRect( BCROW_COUNT - 1 ) as RECT

' Hot-tracking is POLLED rather than driven by WM_MOUSEMOVE: the cursor can leave the block by
' moving onto a child control, where this window never sees another mouse message and the
' highlight would stick on. Every hot-tracking control in this family carries such a timer for
' the same reason -- WM_MOUSELEAVE is not reliable enough to be the only mechanism.
#Define IDT_BCPREVIEW  1

type BUILD_OPTIONS
    groupid as long
    wszText as DWSTRING
end type

' Create an array that holds all options in the Build compiler options listbox. The
' description contains a parenthesis enclosed action.
dim shared gBuildOptions(25) as BUILD_OPTIONS
gBuildOptions(0).groupid  = 0
gBuildOptions(0).wszText  = "Subsystem to console (-s console)"
gBuildOptions(1).groupid  = 0
gBuildOptions(1).wszText  = "Subsystem to GUI (-s gui)"

gBuildOptions(2).groupid  = 1
gBuildOptions(2).wszText  = "Compiler 32-bit assembler backend (-gen gas)"
gBuildOptions(3).groupid  = 1
gBuildOptions(3).wszText  = "Compiler 64-bit assembler backend (-gen gas64)"
gBuildOptions(4).groupid  = 1
gBuildOptions(4).wszText  = "Compiler GCC backend (-gen gcc)"
gBuildOptions(5).groupid  = 1
gBuildOptions(5).wszText  = "Compiler LLVM backend (-gen llvm)"

gBuildOptions(6).groupid  = 2
gBuildOptions(6).wszText  = "Add error checking (-e)"
gBuildOptions(7).groupid  = 2
gBuildOptions(7).wszText  = "Add error checking with RESUME support (-ex)"
gBuildOptions(8).groupid  = 2
gBuildOptions(8).wszText  = "Same as -ex with array bounds and null pointer (-exx)"

gBuildOptions(9).groupid  = 3
gBuildOptions(9).wszText  = "Enable Assert and AssertWarn code generation (-eassert)"

gBuildOptions(10).groupid = 4
gBuildOptions(10).wszText = "Add debug information (-g)"

gBuildOptions(11).groupid = 5
gBuildOptions(11).wszText = "Optimization level 1 for gcc (-Wc -O1)"
gBuildOptions(12).groupid = 5
gBuildOptions(12).wszText = "Optimization level 2 for gcc (-Wc -O2)"
gBuildOptions(13).groupid = 5
gBuildOptions(13).wszText = "Optimization level 3 for gcc (-Wc -O3)"
gBuildOptions(14).groupid = 5
gBuildOptions(14).wszText = "Optimization favor size for gcc (-Wc -Os)"
gBuildOptions(15).groupid = 5
gBuildOptions(15).wszText = "Optimization favor speed for gcc (-Wc -Ofast)"

gBuildOptions(16).groupid = 6
gBuildOptions(16).wszText = "Language compatibility FreeBasic (-lang fb)"
gBuildOptions(17).groupid = 6
gBuildOptions(17).wszText = "Language compatibility FreeBasic Lite (-lang fblite)"
gBuildOptions(18).groupid = 6
gBuildOptions(18).wszText = "Language compatibility QuickBasic (-lang qb)"

gBuildOptions(19).groupid = 7
gBuildOptions(19).wszText = "Create DLL and import library (-dll)"
gBuildOptions(20).groupid = 7
gBuildOptions(20).wszText = "Create static library (-lib)"

gBuildOptions(21).groupid = 8
gBuildOptions(21).wszText = "Compile only, do not link (-c)"
gBuildOptions(22).groupid = 9
gBuildOptions(22).wszText = "Do not delete the object files (-C)"
gBuildOptions(23).groupid = 10
gBuildOptions(23).wszText = "Emit preprocessed output, do not compile (-pp)"

gBuildOptions(24).groupid = 11
gBuildOptions(24).wszText = "Compile into *.asm / *.c / *.ll files only, do not assemble or link (-r)"
gBuildOptions(25).groupid = 11
gBuildOptions(25).wszText = "Preserve generated intermediate *.asm / *.c / *.ll files (-R)"


' ========================================================================================
' THE COMPILER SWITCH MATCHER  --  is a table row present in an options string, and what
' does toggling it produce?
'
' ONE MATCHER, TWO CALLERS. The painter asks IsChecked and the click handler calls Toggle,
' and both are built on ContainsSeq, so the two cannot come to disagree about what "checked"
' means. THEY USED TO. The painter padded both sides of the comparison and the click handler
' padded only the right, so an options string with no space before a switch was reported
' unchecked and toggled as though it were checked.
'
' EVERYTHING IS A WHITESPACE-DELIMITED TOKEN, and a table row's action is a token SEQUENCE:
' "-Wc -O1" is two tokens, not a substring. That is what makes -O1 and -O2 distinguishable
' structurally rather than by luck, and it is what stops the old PsStrRemove from cutting
' "-Wc -O2" out of the middle of a hand-typed -p "C:\libs -Wc -O2".
'
' COMPARISON IS CASE-SENSITIVE, and that is load-bearing rather than incidental: -c and -C
' are different FBC switches in different groups, and so are -r and -R. A case-insensitive
' compare fuses each pair, with no visible symptom until a build behaves oddly. This is the
' single most likely way to get this code wrong.
'
' KNOWN LIMIT, recorded rather than hidden: quoting is not honoured. A token inside "..." is
' still a token, so the -Wc -O2 inside -p "C:\libs -Wc -O2" IS removable by a group-5 click.
' That is strictly better than the old behaviour, which cut it silently on any group-5 click
' whatever the user had asked for, but it is not correct. Handling it needs a real quote-aware
' lexer and the switch list is not the place to grow one.
' ========================================================================================

' Split into tokens on any run of whitespace. Returns the count; tok() is redim'd to
' (count-1), and an empty or all-whitespace string leaves it erased.
'
' HAND-ROLLED RATHER THAN PsStrParseCountAny + PsStrParseAny, AND NOT BY PREFERENCE.
' Those two disagree with each other about how many fields a string has. PsStrParseCountAny
' advances its scan by LEN(wszDelimiter) after each hit -- the length of the whole delimiter
' SET, 4 here -- while PsStrParseAny correctly parses with nLenDelimiter = 1. So the count
' is short whenever two delimiters sit closer together than the set is long, and the parse
' loop then stops early and DROPS THE TAIL of the string. Measured: "-mt -s console -v" came
' back as three tokens and lost "-v"; "-s   console    -lang fb" came back as two and lost
' "-lang fb" entirely. Both are silent -- the result is a valid shorter options string.
'
' AfxNova is vendored and not ours to patch as a side effect of this work, so this walks the
' characters itself. It also removes the last reason this file needed a two-pass split.
private function BuildOpts_Split( byval wszOptions as DWSTRING, tok() as DWSTRING ) as long
    erase tok
    dim as long nLen = PsLen(wszOptions)
    if nLen = 0 then return 0
    dim as wstring ptr p = wszOptions.sptr
    if p = 0 then return 0

    redim tok(nLen - 1)          ' worst case, every character its own token
    dim as long n = 0
    dim as long nStart = -1      ' start of the token being accumulated, -1 = between tokens

    for i as long = 0 to nLen - 1
        dim as ulong ch = p[i]
        dim as boolean bWS = (ch = 32) orelse (ch = 9) orelse (ch = 13) orelse (ch = 10)
        if bWS then
            if nStart >= 0 then
                tok(n) = mid(*p, nStart + 1, i - nStart)
                n += 1
                nStart = -1
            end if
        else
            if nStart < 0 then nStart = i
        end if
    next
    if nStart >= 0 then
        tok(n) = mid(*p, nStart + 1, nLen - nStart)
        n += 1
    end if

    if n = 0 then
        erase tok
    else
        redim preserve tok(n - 1)
    end if
    return n
end function


' The parenthesised action of one table row, as its own token list.
private function BuildOpts_ActionTokens( byval nIdx as long, tok() as DWSTRING ) as long
    erase tok
    if (nIdx < lbound(gBuildOptions)) orelse (nIdx > ubound(gBuildOptions)) then return 0
    return BuildOpts_Split( AfxStrExtract( gBuildOptions(nIdx).wszText, "(", ")" ), tok() )
end function


' Where does the action's token sequence start inside the options tokens? -1 = nowhere.
' nFrom lets RemoveAllSeq resume the scan after a deletion.
private function BuildOpts_ContainsSeq( tok() as DWSTRING, byval nTok as long, _
                                        act() as DWSTRING, byval nAct as long, _
                                        byval nFrom as long = 0 ) as long
    if (nAct < 1) orelse (nTok < nAct) then return -1
    for i as long = nFrom to nTok - nAct
        dim as boolean bAll = true
        for j as long = 0 to nAct - 1
            ' CASE-SENSITIVE -- see the header. -c/-C and -r/-R depend on it.
            if tok(i + j) <> act(j) then bAll = false: exit for
        next
        if bAll then return i
    next
    return -1
end function


' Remove EVERY occurrence of the action's sequence. Returns how many went.
private function BuildOpts_RemoveAllSeq( tok() as DWSTRING, byref nTok as long, _
                                         act() as DWSTRING, byval nAct as long ) as long
    if (nAct < 1) orelse (nTok < nAct) then return 0
    dim as long nGone = 0
    dim as long at = BuildOpts_ContainsSeq( tok(), nTok, act(), nAct, 0 )
    while at >= 0
        for i as long = at to nTok - nAct - 1
            tok(i) = tok(i + nAct)
        next
        nTok -= nAct
        nGone += 1
        if nTok < nAct then exit while
        ' Resume AT the hole, not after it: two identical sequences back to back would
        ' otherwise leave the second one behind.
        at = BuildOpts_ContainsSeq( tok(), nTok, act(), nAct, at )
    wend
    return nGone
end function


private function BuildOpts_Join( tok() as DWSTRING, byval nTok as long ) as DWSTRING
    dim as DWSTRING wsz
    for i as long = 0 to nTok - 1
        if i > 0 then wsz = wsz & " "
        wsz = wsz & tok(i)
    next
    return wsz
end function


' ----------------------------------------------------------------------------------------
' Is this table row's switch present in the options string?
' ----------------------------------------------------------------------------------------
private function BuildOpts_IsChecked( byval wszOptions as DWSTRING, byval nIdx as long ) as boolean
    dim as DWSTRING tok(any), act(any)
    dim as long nAct = BuildOpts_ActionTokens( nIdx, act() )
    if nAct < 1 then return false
    dim as long nTok = BuildOpts_Split( wszOptions, tok() )
    return iif( BuildOpts_ContainsSeq( tok(), nTok, act(), nAct, 0 ) >= 0, true, false )
end function


' ----------------------------------------------------------------------------------------
' Toggle this table row and return the new options string.
'
' The group is cleared unconditionally and the row re-appended only if it was NOT already
' checked -- which is what gives the per-groupid radio behaviour AND makes clicking a checked
' row turn it off. Same logic the old doAddAction flag carried; only the matching changed.
'
' Tokens that belong to no table row are copied through in their original order, untouched.
' Whitespace between them is normalised to a single space, because the string is rebuilt from
' its tokens. That is deliberate and confined to this path: typing in the raw options field
' re-derives the CHECK STATE only and never rewrites the text under the caret.
' ----------------------------------------------------------------------------------------
private function BuildOpts_Toggle( byval wszOptions as DWSTRING, byval nIdx as long ) as DWSTRING
    if (nIdx < lbound(gBuildOptions)) orelse (nIdx > ubound(gBuildOptions)) then return wszOptions

    dim as DWSTRING tok(any), act(any)
    dim as long nTok = BuildOpts_Split( wszOptions, tok() )
    dim as long nAct = BuildOpts_ActionTokens( nIdx, act() )
    if nAct < 1 then return wszOptions

    dim as boolean bWasChecked = iif( BuildOpts_ContainsSeq( tok(), nTok, act(), nAct, 0 ) >= 0, true, false )

    dim as long groupid = gBuildOptions(nIdx).groupid
    for i as long = lbound(gBuildOptions) to ubound(gBuildOptions)
        if gBuildOptions(i).groupid <> groupid then continue for
        dim as DWSTRING other(any)
        dim as long nOther = BuildOpts_ActionTokens( i, other() )
        if nOther > 0 then BuildOpts_RemoveAllSeq( tok(), nTok, other(), nOther )
    next

    if bWasChecked = false then
        ' Room for nTok + nAct entries. The removals above only ever shrink nTok, never the
        ' array, so this usually does nothing -- but an empty options string leaves tok()
        ' erased, and appending to that needs the redim.
        if (nTok + nAct - 1) > ubound(tok) then redim preserve tok(nTok + nAct - 1)
        for j as long = 0 to nAct - 1
            tok(nTok + j) = act(j)
        next
        nTok += nAct
    end if

    return BuildOpts_Join( tok(), nTok )
end function


' ========================================================================================
' Determine the current active build index
' ========================================================================================
function frmBuildConfig_getActiveBuildIndex() as long
    ' determine the default build in case no other build overrides it
    dim as long nDefault
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).IsDefault then
            nDefault = i: exit for
        end if
    next

    ' determine if the workspace's own selection overrides the default.
    ' gConfig.CompilerBuild is consulted only as the SEED when a new workspace is
    ' established (Workspace_EstablishUntitled), never here.
    dim as long idx = -1
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).id = gApp.ProjectBuild then
            idx = i: exit for
        end if
    next

    if idx = -1 then idx = nDefault
    function = idx
end function


' ========================================================================================
' Return the string description of the currently selected build configuration
' ========================================================================================
function frmBuildConfig_GetSelectedBuildDescription() as DWSTRING
    dim as long nCurSel = frmBuildConfig_getActiveBuildIndex()
    if nCurSel > -1 then
        function = gConfig.Builds(nCurSel).wszDescription
    end if
end function


' ========================================================================================
' Return the string GUID of the currently selected build configuration
' ========================================================================================
function frmBuildConfig_GetSelectedBuildGUID() as string
    dim as long nCurSel = frmBuildConfig_getActiveBuildIndex()
    if nCurSel > -1 then
        function = gConfig.Builds(nCurSel).Id
    end if
end function


' ========================================================================================
' Return the string GUID of the default (if any) build configuration
' ========================================================================================
function frmBuildConfig_GetDefaultBuildGUID() as string
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).IsDefault then 
            return gConfig.Builds(i).Id
        end if   
    next
    function = ""
end function

' ========================================================================================
' Create an accelerator table based on selections for the Build Configurations.
' ========================================================================================
function frmBuildConfig_CreateAcceleratorTable() as long
    dim NumKeys  as long
    dim nKey     as long
    dim fVirt    as long
        
    if ghAccelBuildConfigurations then DestroyAcceleratorTable(ghAccelBuildConfigurations)
    dim a(any) as ACCEL
        
    NumKeys = 0
    for y as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
            
        fVirt = FNOINVERT or FVIRTKEY
        
        if gConfig.Builds(y).IsCtrl then fVirt = fVirt or FCONTROL
        if gConfig.Builds(y).IsShift then fVirt = fVirt or FSHIFT
        if gConfig.Builds(y).IsAlt then fVirt = fVirt or FALT
            
        ' The build's key is a BARE NAME picked from AddShortcutsToComboBox, so it is
        ' resolved by the same vocabulary that filled that combobox (modKeyBindings.inc).
        '
        ' This used to be a private 48-arm select case right here -- F1-F12, 0-9 and A-Z --
        ' against a pick-list that offers 89 names. Choosing any of the other 41 (Tab, Enter,
        ' PgUp, PgDn, Home, End, the arrows, the Numpad keys, the punctuation keys) left
        ' nKey = 0, this loop skipped the ACCEL entry, and the shortcut then saved, displayed
        ' and reloaded perfectly while never once firing. No warning, no visible symptom.
        '
        ' Do NOT "simplify" this to frmKeyboard_AccelKeyToValue: that one resolves the stored
        ' "None" affordance to plain N, and any stale name to its own first letter.
        nKey = KeyBindings_PickListKeyToValue( gConfig.Builds(y).wszKey )

        if nKey > 0 then
            NumKeys = NumKeys + 1
            redim preserve a(1 To NumKeys) as ACCEL
            a(NumKeys).fVirt = fVirt
            a(NumKeys).key   = nKey
            a(NumKeys).cmd   = IDM_BUILDCONFIGBASE + y
        end if

    next                                    
        
    if NumKeys then
        ghAccelBuildConfigurations = CreateAcceleratorTable(cast(LPACCEL, @a(1)), NumKeys)
    end if

    function = 0
end Function

' ========================================================================================
' THE SHORTCUT A BUILD DISPLAYS.
'
' MODIFIER ORDER IS Ctrl, Shift, Alt AND MUST STAY THAT WAY. It matches
' createToolsMenuShortcut, which is what the Tools menu has always emitted, and the two sit
' inches apart on screen. It is NOT the order frmKeyboardEdit_BuildKeyString produces
' (Ctrl+Alt+Shift), which is what keybindings.ini stores -- the two coexist deliberately,
' and the CLASH test calls that composer rather than composing a second time here. Get that
' wrong and every three-modifier shortcut silently reports free.
'
' NOTE: before this rebuild there was no build-side composer at all. modContextMenus built
' the statusbar popup with a hardcoded "" in the shortcut slot, so a build accelerator fired
' but was never displayed anywhere in the editor.
'
' "" AND "None" BOTH MEAN NO SHORTCUT. The old dialog wrote the literal string "None" into
' wszKey on every selection change and persisted it, so every keyless build in every existing
' settings.ini already carries it. That is the same shipped bug that was fixed in User Tools.
' ========================================================================================
private function frmBuildConfig_ComposeShortcut( byval wszKey as DWSTRING, _
                                                 byval nCtrl as long, _
                                                 byval nShift as long, _
                                                 byval nAlt as long ) as DWSTRING
    dim as DWSTRING wsz = PsTrim(wszKey)
    if PsLen(wsz) = 0 then return ""
    if PsUCase(wsz) = "NONE" then return ""

    dim as DWSTRING wszOut
    if nCtrl  then wszOut = wszOut & "Ctrl+"
    if nShift then wszOut = wszOut & "Shift+"
    if nAlt   then wszOut = wszOut & "Alt+"
    return wszOut & wsz
end function


' The STAGING row's shortcut -- what the dialog shows while it is open.
private function frmBuildConfig_ShortcutText( byval idx as long ) as DWSTRING
    if (idx < lbound(gBuildsWork)) orelse (idx > ubound(gBuildsWork)) then return ""
    return frmBuildConfig_ComposeShortcut( gBuildsWork(idx).wszKey, gBuildsWork(idx).IsCtrl, _
                                           gBuildsWork(idx).IsShift, gBuildsWork(idx).IsAlt )
end function


' The COMMITTED row's shortcut -- for the statusbar popup menu, built while this dialog is
' shut. Same composer, different array, so the menu and the dialog cannot disagree.
function frmBuildConfig_MenuShortcutText( byval idx as long ) as DWSTRING
    if (idx < lbound(gConfig.Builds)) orelse (idx > ubound(gConfig.Builds)) then return ""
    return frmBuildConfig_ComposeShortcut( gConfig.Builds(idx).wszKey, gConfig.Builds(idx).IsCtrl, _
                                           gConfig.Builds(idx).IsShift, gConfig.Builds(idx).IsAlt )
end function


' ========================================================================================
' A shortcut that SAVES, DISPLAYS and RELOADS perfectly but can never fire.
'
' The build's key is a bare name from the pick-list vocabulary. If PickListKeyToValue cannot
' resolve it -- a stale spelling such as "PageDn" against a resolver that knows "PGDN" --
' CreateAcceleratorTable skips the entry and there is nothing anywhere to say so.
'
' "" and "None" are NOT dead: they mean the build has no shortcut, which is a legal state.
' ========================================================================================
private function frmBuildConfig_ShortcutIsDead( byval idx as long ) as boolean
    if (idx < lbound(gBuildsWork)) orelse (idx > ubound(gBuildsWork)) then return false
    dim as DWSTRING wszKey = PsTrim(gBuildsWork(idx).wszKey)
    if PsLen(wszKey) = 0 then return false
    if PsUCase(wszKey) = "NONE" then return false
    return iif( KeyBindings_PickListKeyToValue( wszKey ) = 0, true, false )
end function


' ========================================================================================
' Does this build's shortcut already belong to an editor command, a user tool, or another
' build?  "" when it is free.
'
' Answers for a build merely SITTING in the list, which the assign dialog cannot: that dialog
' refuses to CREATE a clash, but a settings.ini written before this check existed can already
' hold one, and a build whose shortcut duplicates an editor command loses to it silently --
' the build simply never runs and nothing says why.
' ========================================================================================
private function frmBuildConfig_ShortcutClash( byval idx as long ) as DWSTRING
    if (idx < lbound(gBuildsWork)) orelse (idx > ubound(gBuildsWork)) then return ""
    dim as DWSTRING wszKey = PsTrim(gBuildsWork(idx).wszKey)
    if PsLen(wszKey) = 0 then return ""
    if PsUCase(wszKey) = "NONE" then return ""
    return frmAssignKey_ClashText( wszKey, _
                                   iif( gBuildsWork(idx).IsCtrl  <> 0, true, false ), _
                                   iif( gBuildsWork(idx).IsAlt   <> 0, true, false ), _
                                   iif( gBuildsWork(idx).IsShift <> 0, true, false ), _
                                   ASSIGNKEY_DOMAIN_BUILDS, idx )
end function


' ========================================================================================
' Staging: seed on entry, commit only on OK.
' ========================================================================================
private sub frmBuildConfig_LoadWork()
    erase gBuildsWork
    if ubound(gConfig.Builds) < 0 then exit sub
    redim gBuildsWork(ubound(gConfig.Builds))
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        gBuildsWork(i) = gConfig.Builds(i)
    next
end sub


' ========================================================================================
' UNASSIGN ANY SHORTCUT THAT CANNOT WORK, rather than reporting it.
'
' Two kinds, and neither is the user's doing:
'
'   1. UNRESOLVABLE. The stored key name is not one KeyBindings_PickListKeyToValue knows -- a
'      stale vocabulary spelling such as "PageDn" against a resolver that knows "PGDN". The
'      accelerator table skips it, so it saves, displays and reloads perfectly while never
'      once firing.
'   2. ALREADY TAKEN, by an editor command, a user tool, or an earlier build. Both accelerator
'      tables are live at once, so the build simply loses and never runs.
'
' An earlier version painted these red in the list and spelled the reason out beside the
' Shortcut row. That was the wrong place for it: the dialog reported a problem it was not
' offering to fix, on a row the user had not touched, every time they opened it. The assign
' dialog is where a shortcut is chosen and where the refusal belongs -- it REFUSES to create
' either kind -- so here the dead binding is simply dropped and the build shows "(none)".
'
' It runs against the STAGING copy only, so nothing reaches settings.ini unless OK is pressed.
' Order matters: walking forwards means the FIRST holder of a duplicated keystroke keeps it
' and later ones are cleared, which is deterministic rather than dependent on which row the
' user happens to be looking at.
' ========================================================================================
private sub frmBuildConfig_SanitizeWork()
    for i as long = lbound(gBuildsWork) to ubound(gBuildsWork)
        dim as DWSTRING wszKey = PsTrim(gBuildsWork(i).wszKey)
        if PsLen(wszKey) = 0 then continue for
        ' "None" is the legacy affordance the old dialog wrote on every selection change. It
        ' is not a key and never was, so it is normalised away here too.
        dim as boolean bDrop = (PsUCase(wszKey) = "NONE")
        if bDrop = false then bDrop = frmBuildConfig_ShortcutIsDead( i )
        if bDrop = false then bDrop = iif( len( frmBuildConfig_ShortcutClash( i ) ) > 0, true, false )
        if bDrop then
            gBuildsWork(i).wszKey  = ""
            gBuildsWork(i).IsCtrl  = 0
            gBuildsWork(i).IsAlt   = 0
            gBuildsWork(i).IsShift = 0
        end if
    next
end sub


' THE ORDER HERE IS LOAD-BEARING. gConfig.Builds first, THEN the accelerator table, THEN the
' file -- because frmBuildConfig_CreateAcceleratorTable reads gConfig.Builds. Rebuild the
' table before the copy and it reinstates the pre-edit shortcuts, with nothing reporting it.
private sub frmBuildConfig_CommitWork()
    erase gConfig.Builds
    if ubound(gBuildsWork) >= 0 then
        redim gConfig.Builds(ubound(gBuildsWork))
        for i as long = lbound(gBuildsWork) to ubound(gBuildsWork)
            gConfig.Builds(i) = gBuildsWork(i)
        next
    end if

    ' Point the active project or document at the highlighted build, exactly as the dialog
    ' this replaces did -- this is the only writer of gApp.ProjectBuild on this path.
    if (gBCCurrent >= 0) andalso (gBCCurrent <= ubound(gConfig.Builds)) then
        gApp.ProjectBuild = gConfig.Builds(gBCCurrent).Id
        frmMain_SetStatusbar
    end if

    frmBuildConfig_CreateAcceleratorTable()
    gConfig.SaveConfigFile
end sub


' The staging index behind the highlighted row. ALWAYS via itemData, never the row index --
' they are the same today only because this list is never filtered.
private function frmBuildConfig_SelectedWorkIndex() as long
    if HWND_FRMBUILDCONFIG_LIST = 0 then return -1
    dim as long row = PsListTree_GetCurSel( HWND_FRMBUILDCONFIG_LIST )
    if row < 0 then return -1
    dim as long idx = PsListTree_GetItemData( HWND_FRMBUILDCONFIG_LIST, row )
    if (idx < lbound(gBuildsWork)) orelse (idx > ubound(gBuildsWork)) then return -1
    return idx
end function


' ========================================================================================
' The builds list: Description | (uncaptioned) Default marker
'
' NO WARNING COLOURING. An earlier version painted a clashing or unresolvable shortcut cell
' in theme.compile.iconfail. There is now nothing to warn about: a shortcut that cannot fire is
' UNASSIGNED when the staging copy is seeded (see frmBuildConfig_SanitizeWork), and the assign
' dialog refuses to create a new one -- so a clash cannot survive long enough to be reported.
' ========================================================================================
private sub frmBuildConfig_ListPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )

    if p = 0 then exit sub
    dim as boolean bActive = (p->isSelected orelse p->isHot)

    p->b->SetBackColor( iif( bActive, theme.panel.texthot.backcolor, theme.panel.text.backcolor ) )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_11) )

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(6)
        dim as COLORREF clr = iif( bActive, theme.panel.texthot.forecolor, theme.panel.text.forecolor )
        ' The default marker is subdued: it is a state, not a heading.
        if (i = 1) andalso (bActive = false) then clr = theme.topmenu.textdisabled.forecolor
        p->b->SetForeColor( clr )
        p->b->PaintText( p->cells[i].wszText, @rcText, _
                         DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
    next

end sub


private sub frmBuildConfig_ListPaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub

    p->b->SetBackColor( iif( p->isHot orelse p->isResizeHot, theme.panel.texthot.backcolor, theme.panel.text.backcolor ) )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(6)
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->SetForeColor( theme.topmenu.textdisabled.forecolor )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

    dim as RECT rcEdge = p->rc
    rcEdge.left = rcEdge.right - AfxScaleX(1)
    p->b->SetBackColor( theme.ui.divider )
    p->b->PaintRect( @rcEdge )
end sub


' NO TOOLTIPS, on the rows OR the header. With no callback PsListTree falls back to the row's
' own text and PsColumnHeader to the column's own caption, so hovering "Shortcut" would pop a
' tip reading "Shortcut". TWO SEPARATE OPT-OUTS -- it is easy to fix one and leave the other.
private function frmBuildConfig_ListTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    return ""
end function

private function frmBuildConfig_HeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' The compiler switches list.
'
' ONE COLUMN, NO HEADER, so columnCount is 0 and the row rect is the whole cell -- the
' control's classic contract. The checkbox glyph is painted in a reserved cell on the left
' and the label takes the rest.
'
' THE GROUP RULE IS A HAIRLINE, NOT A ROW. Members of one groupid are mutually exclusive, and
' a spanned separator row per group would cost eleven rows of a list that has to fit 470px.
' The line is drawn at the TOP of the first row of each group, inside that row's own paint,
' so it costs no vertical space at all.
' ========================================================================================
private sub frmBuildConfig_SwitchPaintRow( byval p as PSLISTTREE_PAINTINFO ptr )

    if p = 0 then exit sub
    dim as long idx = p->itemID
    if (idx < lbound(gBuildOptions)) orelse (idx > ubound(gBuildOptions)) then exit sub

    dim as boolean bActive = (p->isSelected orelse p->isHot)
    p->b->SetBackColor( iif( bActive, theme.panel.texthot.backcolor, theme.panel.text.backcolor ) )
    p->b->PaintRect( @p->rc )

    ' The group rule. Not drawn above the very first row -- there is nothing above it to
    ' separate it from.
    if idx > lbound(gBuildOptions) then
        if gBuildOptions(idx).groupid <> gBuildOptions(idx - 1).groupid then
            dim as RECT rcRule = p->rc
            rcRule.bottom = rcRule.top + AfxScaleY(1)
            p->b->SetBackColor( theme.ui.divider )
            p->b->PaintRect( @rcRule )
            p->b->SetBackColor( iif( bActive, theme.panel.texthot.backcolor, theme.panel.text.backcolor ) )
        end if
    end if

    dim as boolean bChecked = false
    if gBCCurrent >= 0 then bChecked = BuildOpts_IsChecked( gBuildsWork(gBCCurrent).wszOptions, idx )

    dim as RECT rcGlyph = p->rc
    rcGlyph.right = rcGlyph.left + AfxScaleX(FRMBUILDCONFIG_SWGLYPH_W)
    p->b->SetFont( ghFont(SYMBOLFONT_10) )
    p->b->SetForeColor( iif( bChecked, theme.panel.texthot.forecolor, theme.topmenu.textdisabled.forecolor ) )
    p->b->PaintText( iif( bChecked, wszIconCheckBoxMarked, wszIconCheckBoxEmpty ), @rcGlyph, _
                     DT_CENTER or DT_VCENTER or DT_SINGLELINE )

    dim as RECT rcText = p->rc
    rcText.left = rcGlyph.right + AfxScaleX(4)
    p->b->SetFont( ghFont(GUIFONT_11) )
    p->b->SetForeColor( iif( bActive, theme.panel.texthot.forecolor, theme.panel.text.forecolor ) )
    p->b->PaintText( gBuildOptions(idx).wszText, @rcText, _
                     DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

end sub


' ========================================================================================
' Toggle one switch, from a click or from Space.
' ========================================================================================
private sub frmBuildConfig_ToggleSwitch( byval row as long )

    if gBCCurrent < 0 then exit sub
    if (row < lbound(gBuildOptions)) orelse (row > ubound(gBuildOptions)) then exit sub

    gBuildsWork(gBCCurrent).wszOptions = BuildOpts_Toggle( gBuildsWork(gBCCurrent).wszOptions, row )

    ' The raw field is the same value shown a different way, so it follows. SetText is silent,
    ' which is what stops this bouncing back through the change callback.
    gBCLoading = true
    PsTextBox_SetText( ghBCOptions, gBuildsWork(gBCCurrent).wszOptions )
    gBCLoading = false

    ' PsListTree_Refresh, NOT AfxRedrawWindow on the control. The checkbox glyphs are painted
    ' by the row callback onto the control's own SURFACE, which is a child window -- redrawing
    ' the container never reaches it, so the row kept its old glyph until something else
    ' happened to invalidate the surface. The check state here is DERIVED from the options
    ' string rather than stored per row, so there is no row to mark dirty; the whole list has
    ' to be re-asked. Refresh also preserves the scroll position, which matters because this
    ' fires on a click far down a 26-row list.
    PsListTree_Refresh( ghBCSwitches )
end sub


' WHOLE ROW TOGGLES. The dialog this replaces hit-tested a 20x20 box at the left of the row
' and did nothing at all for a click on the label, which is most of the row's width.
'
' FALSE, not TRUE: the control has already applied its own click selection by the time this
' runs, and returning TRUE would suppress the rest of its default handling.
private function frmBuildConfig_SwitchListMessage( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    if m->uMsg = WM_LBUTTONUP then
        if m->idx >= 0 then frmBuildConfig_ToggleSwitch( m->idx )
    end if
    return false
end function


' ========================================================================================
' Show one page of the right pane.  ALWAYS call this after PsSelectBar_SetCurSel -- like
' every setter in this family that one is silent.
' ========================================================================================
private sub frmBuildConfig_ShowPage( byval nPageID as long )

    gBCPage = nPageID
    dim as boolean bGeneral  = (nPageID = IDP_FRMBUILDCONFIG_GENERAL)
    dim as boolean bHaveRow  = (gBCCurrent >= 0)

    dim as long nGen = iif( bGeneral andalso bHaveRow, SW_SHOW, SW_HIDE )
    dim as long nSwi = iif( (bGeneral = false) andalso bHaveRow, SW_SHOW, SW_HIDE )

    ShowWindow( ghBCDescription, nGen )
    ShowWindow( ghBCTogDefault, nGen )
    ShowWindow( ghBCTarget, nGen )
    ShowWindow( ghBCShortcut, nGen )
    ShowWindow( ghBCTogExclude, nGen )
    ShowWindow( ghBCEditSwitches, nGen )

    ShowWindow( ghBCOptions, nSwi )
    ShowWindow( ghBCSwitches, nSwi )

    AfxRedrawWindow( HWND_FRMBUILDCONFIG )
end sub


' ========================================================================================
' Push the highlighted build into the right pane.  -1 hides it entirely.
'
' HIDDEN, not disabled: a page of greyed-out fields over an empty list is chrome describing
' nothing. Same call frmUserTools_LoadDetail makes.
' ========================================================================================
private sub frmBuildConfig_LoadDetail( byval nIdx as long )

    gBCCurrent = nIdx
    gBCLoading = true

    if (nIdx >= lbound(gBuildsWork)) andalso (nIdx <= ubound(gBuildsWork)) then
        PsTextBox_SetText( ghBCDescription, gBuildsWork(nIdx).wszDescription )
        PsTextBox_SetText( ghBCOptions, gBuildsWork(nIdx).wszOptions )
        PsToggle_SetChecked( ghBCTogDefault, iif( gBuildsWork(nIdx).IsDefault <> 0, true, false ) )
        PsToggle_SetChecked( ghBCTogExclude, iif( gBuildsWork(nIdx).ExcludeInPopup <> 0, true, false ) )
        ' Is64bit wins when both are set -- a settings.ini can hold any pair, and picking one
        ' deterministically is better than showing whichever the last writer happened to set.
        PsComboBox_SetCurSel( ghBCTarget, iif( gBuildsWork(nIdx).Is64bit <> 0, BUILDTARGET_64, BUILDTARGET_32 ) )

        dim as DWSTRING wszSC = frmBuildConfig_ShortcutText( nIdx )
        if PsLen(wszSC) = 0 then wszSC = L(343,"(none)")
        PsButton_SetText( ghBCShortcut, wszSC )
    else
        gBCCurrent = -1
    end if

    gBCLoading = false

    PsButton_SetEnabled( ghBCDelete, iif( gBCCurrent >= 0, true, false ) )
    PsButton_SetEnabled( ghBCUp, iif( gBCCurrent > 0, true, false ) )
    PsButton_SetEnabled( ghBCDown, iif( (gBCCurrent >= 0) andalso (gBCCurrent < ubound(gBuildsWork)), true, false ) )

    frmBuildConfig_ShowPage( gBCPage )
    PsListTree_Refresh( ghBCSwitches )
end sub


' ========================================================================================
' Fill the list from the staging array.  nSelectIdx = -2 keeps the current selection.
' ========================================================================================
private sub frmBuildConfig_BuildList( byval nSelectIdx as long = -2 )

    if HWND_FRMBUILDCONFIG_LIST = 0 then exit sub

    dim as long nWant = nSelectIdx
    if nWant = -2 then nWant = gBCCurrent

    PsListTree_BeginUpdate( HWND_FRMBUILDCONFIG_LIST )
    PsListTree_Clear( HWND_FRMBUILDCONFIG_LIST )

    for i as long = lbound(gBuildsWork) to ubound(gBuildsWork)
        dim as DWSTRING wszDesc = gBuildsWork(i).wszDescription
        if PsLen(wszDesc) = 0 then wszDesc = L(342,"(untitled)")
        dim as long row = PsListTree_AddString( HWND_FRMBUILDCONFIG_LIST, wszDesc, i )
        dim as DWSTRING wszDef
        if gBuildsWork(i).IsDefault <> 0 then wszDef = L(357,"Default")
        PsListTree_SetCellText( HWND_FRMBUILDCONFIG_LIST, row, 1, wszDef )
    next

    PsListTree_EndUpdate( HWND_FRMBUILDCONFIG_LIST )

    if ubound(gBuildsWork) < 0 then
        nWant = -1
    else
        if nWant < 0 then nWant = 0
        if nWant > ubound(gBuildsWork) then nWant = ubound(gBuildsWork)
        PsListTree_SetCurSel( HWND_FRMBUILDCONFIG_LIST, nWant )
    end if

    ' SetCurSel is SILENT, so the detail pane has to be told explicitly.
    frmBuildConfig_LoadDetail( nWant )
end sub


' One row's cells, without rebuilding the list -- which would lose the scroll position.
private sub frmBuildConfig_RefreshRow( byval idx as long )
    if HWND_FRMBUILDCONFIG_LIST = 0 then exit sub
    if (idx < lbound(gBuildsWork)) orelse (idx > ubound(gBuildsWork)) then exit sub
    dim as DWSTRING wszDesc = gBuildsWork(idx).wszDescription
    if PsLen(wszDesc) = 0 then wszDesc = L(342,"(untitled)")
    dim as DWSTRING wszDef
    if gBuildsWork(idx).IsDefault <> 0 then wszDef = L(357,"Default")
    PsListTree_SetText( HWND_FRMBUILDCONFIG_LIST, idx, wszDesc )
    PsListTree_SetCellText( HWND_FRMBUILDCONFIG_LIST, idx, 1, wszDef )
end sub


' ========================================================================================
' A themed message box owned by THIS dialog.
'
' Parented here, never to frmMain: this window is up and modal, and a box owned by a disabled
' ancestor comes up behind it. PsMessageBox runs its own nested loop, so this is a third
' modal loop inside this dialog's inside frmMain's -- which works, and is what frmUserTools
' does for the same confirmation.
' ========================================================================================
' The dressing (colours, fonts, the 22pt icon glyph) lives in TikoMsgBox -- modMsgBox.inc. All
' that is per-dialog here is the OWNER and the default button.
private function frmBuildConfig_ThemedMsgBox( byval wszText as DWSTRING, _
                                              byval wszCaption as DWSTRING ) as long
    ' Default index 1 = Cancel -- this box only ever guards a destructive action.
    return TikoMsgBox( HWND_FRMBUILDCONFIG, wszText, wszCaption, _
                       TMB_ICON_WARNING, TMB_OKCANCEL, 1 )
end function


' ========================================================================================
' Add / Delete / Move
' ========================================================================================
private sub frmBuildConfig_DoAdd()

    dim as long nAt = gBCCurrent + 1
    if nAt < 0 then nAt = 0

    dim as long nOld = ubound(gBuildsWork)
    redim preserve gBuildsWork(nOld + 1)
    for i as long = ubound(gBuildsWork) to nAt + 1 step -1
        gBuildsWork(i) = gBuildsWork(i-1)
    next

    ' FIELD BY FIELD, not a blank TYPE_BUILDS: a member added to that type later then shows up
    ' as a compile-time reminder here rather than silently defaulting to zero.
    gBuildsWork(nAt).Id             = AfxGuidText(AfxGuid())
    gBuildsWork(nAt).wszDescription = ""
    gBuildsWork(nAt).wszOptions     = ""
    gBuildsWork(nAt).IsDefault      = 0
    gBuildsWork(nAt).Is32bit        = 0
    gBuildsWork(nAt).Is64bit        = 1
    gBuildsWork(nAt).ExcludeInPopup = 0
    gBuildsWork(nAt).idMenu         = 0
    gBuildsWork(nAt).IsCtrl         = 0
    gBuildsWork(nAt).IsAlt          = 0
    gBuildsWork(nAt).IsShift        = 0
    ' "" and NOT "None" -- see frmBuildConfig_ComposeShortcut.
    gBuildsWork(nAt).wszKey         = ""

    frmBuildConfig_BuildList( nAt )
    frmBuildConfig_ShowPage( IDP_FRMBUILDCONFIG_GENERAL )
    PsSelectBar_SetCurSel( ghBCSelectBar, 0 )
    SetFocus( ghBCDescription )
end sub


private sub frmBuildConfig_DoDelete()

    if gBCCurrent < 0 then exit sub

    if frmBuildConfig_ThemedMsgBox( L(223,"Are you sure you want to delete this build configuration?"), _
                                    L(214,"Confirm") ) <> IDOK then exit sub

    dim as long nAt = gBCCurrent
    if ubound(gBuildsWork) = 0 then
        erase gBuildsWork
    else
        for i as long = nAt to ubound(gBuildsWork) - 1
            gBuildsWork(i) = gBuildsWork(i+1)
        next
        redim preserve gBuildsWork(ubound(gBuildsWork) - 1)
    end if

    frmBuildConfig_BuildList( nAt )
    SetFocus( HWND_FRMBUILDCONFIG_LIST )
end sub


' The selection follows the RECORD, not the position.
private sub frmBuildConfig_DoMove( byval nDelta as long )
    if gBCCurrent < 0 then exit sub
    dim as long nTo = gBCCurrent + nDelta
    if (nTo < lbound(gBuildsWork)) orelse (nTo > ubound(gBuildsWork)) then exit sub
    swap gBuildsWork(gBCCurrent), gBuildsWork(nTo)
    frmBuildConfig_BuildList( nTo )
    SetFocus( HWND_FRMBUILDCONFIG_LIST )
end sub


' ========================================================================================
' Open the shared Assign Shortcut dialog on the highlighted build.
' ========================================================================================
private sub frmBuildConfig_DoAssign()

    if gBCCurrent < 0 then exit sub
    dim as long idx = gBCCurrent

    gAssignKeyCaption = gBuildsWork(idx).wszDescription
    if len(gAssignKeyCaption) = 0 then gAssignKeyCaption = L(342,"(untitled)")
    ' Domain AND row, so the clash test skips this build's own current binding -- without it,
    ' opening on a build that already has Ctrl+2 reports that it clashes with itself and
    ' disables OK before the user has touched anything.
    gAssignKeyDomain = ASSIGNKEY_DOMAIN_BUILDS
    gAssignKeyIndex  = idx
    ' "None" is a legacy affordance, not a key. Hand the dialog an empty string or it starts
    ' up showing a shortcut that does not exist.
    gAssignKeyName   = gBuildsWork(idx).wszKey
    if ucase(trim(gAssignKeyName)) = "NONE" then gAssignKeyName = ""
    gAssignKeyCtrl   = iif( gBuildsWork(idx).IsCtrl  <> 0, true, false )
    gAssignKeyAlt    = iif( gBuildsWork(idx).IsAlt   <> 0, true, false )
    gAssignKeyShift  = iif( gBuildsWork(idx).IsShift <> 0, true, false )

    frmAssignKey_Show( HWND_FRMBUILDCONFIG )

    ' Cancel leaves gAssignKey* exactly as set above, so this is a no-op on that path rather
    ' than a case that has to be detected.
    gBuildsWork(idx).wszKey  = gAssignKeyName
    gBuildsWork(idx).IsCtrl  = iif( gAssignKeyCtrl,  1, 0 )
    gBuildsWork(idx).IsAlt   = iif( gAssignKeyAlt,   1, 0 )
    gBuildsWork(idx).IsShift = iif( gAssignKeyShift, 1, 0 )

    ' BOTH sentinels: leaving the domain set would make a later ASSIGNKEY_DOMAIN_NONE caller
    ' silently read staging instead of committed records.
    gAssignKeyDomain = ASSIGNKEY_DOMAIN_NONE
    gAssignKeyIndex  = -1

    dim as DWSTRING wszSC = frmBuildConfig_ShortcutText( idx )
    if PsLen(wszSC) = 0 then wszSC = L(343,"(none)")
    PsButton_SetText( ghBCShortcut, wszSC )

    frmBuildConfig_RefreshRow( idx )
    AfxRedrawWindow( HWND_FRMBUILDCONFIG )
    SetFocus( ghBCShortcut )
end sub


' ========================================================================================
' Control callbacks
' ========================================================================================
private sub frmBuildConfig_ButtonClick( byval hButton as HWND, byval id as long )
    select case id
        case IDC_FRMBUILDCONFIG_CMDADD          : frmBuildConfig_DoAdd()
        case IDC_FRMBUILDCONFIG_CMDDELETE       : frmBuildConfig_DoDelete()
        case IDC_FRMBUILDCONFIG_CMDUP           : frmBuildConfig_DoMove( -1 )
        case IDC_FRMBUILDCONFIG_CMDDOWN         : frmBuildConfig_DoMove( 1 )
        case IDC_FRMBUILDCONFIG_CMDSHORTCUT     : frmBuildConfig_DoAssign()
        case IDC_FRMBUILDCONFIG_CMDEDITSWITCHES
            PsSelectBar_SetCurSel( ghBCSelectBar, 1 )
            frmBuildConfig_ShowPage( IDP_FRMBUILDCONFIG_SWITCHES )
            SetFocus( ghBCSwitches )
        case IDC_FRMBUILDCONFIG_CMDOK
            frmBuildConfig_CommitWork()
            SendMessage( HWND_FRMBUILDCONFIG, WM_CLOSE, 0, 0 )
        case IDC_FRMBUILDCONFIG_CMDCANCEL
            SendMessage( HWND_FRMBUILDCONFIG, WM_CLOSE, 0, 0 )
    end select
end sub


private sub frmBuildConfig_TextChanged( byval hTextBox as HWND )
    if gBCLoading then exit sub
    if gBCCurrent < 0 then exit sub

    if hTextBox = ghBCDescription then
        gBuildsWork(gBCCurrent).wszDescription = PsTextBox_GetText( hTextBox )
        frmBuildConfig_RefreshRow( gBCCurrent )

    elseif hTextBox = ghBCOptions then
        ' TYPING HERE RE-DERIVES THE CHECK STATE AND NOTHING ELSE. The text is never rewritten
        ' under the caret -- only a toggle rebuilds the string from its tokens.
        gBuildsWork(gBCCurrent).wszOptions = PsTextBox_GetText( hTextBox )
        PsListTree_Refresh( ghBCSwitches )
        AfxRedrawWindow( HWND_FRMBUILDCONFIG )
    end if
end sub


private sub frmBuildConfig_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    if gBCLoading then exit sub
    if gBCCurrent < 0 then exit sub

    if hToggle = ghBCTogDefault then
        ' EXACTLY ONE BUILD MAY BE DEFAULT. Clearing the others changes column 2 on every row,
        ' so every row is refreshed -- not just this one.
        gBuildsWork(gBCCurrent).IsDefault = iif( isChecked, 1, 0 )
        if isChecked then
            for i as long = lbound(gBuildsWork) to ubound(gBuildsWork)
                if i <> gBCCurrent then gBuildsWork(i).IsDefault = 0
            next
        end if
        for i as long = lbound(gBuildsWork) to ubound(gBuildsWork)
            frmBuildConfig_RefreshRow( i )
        next

    elseif hToggle = ghBCTogExclude then
        gBuildsWork(gBCCurrent).ExcludeInPopup = iif( isChecked, 1, 0 )
    end if
end sub


' Is32bit and Is64bit are TWO fields. The combo has one index, so both are written,
' complementarily, on every change.
private sub frmBuildConfig_TargetChanged( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if gBCLoading then exit sub
    if gBCCurrent < 0 then exit sub
    gBuildsWork(gBCCurrent).Is32bit = iif( idxNew = BUILDTARGET_32, 1, 0 )
    gBuildsWork(gBCCurrent).Is64bit = iif( idxNew = BUILDTARGET_64, 1, 0 )
end sub


private sub frmBuildConfig_ListSelChange( byval hList as HWND, byval row as integer )
    frmBuildConfig_LoadDetail( frmBuildConfig_SelectedWorkIndex() )
end sub


private sub frmBuildConfig_SelectBarChange( byval hBar as HWND, byval idxOld as long, byval idxNew as long )
    frmBuildConfig_ShowPage( PsSelectBar_GetPanelID( hBar, idxNew ) )
end sub


' ========================================================================================
' Paint the dialog's own chrome.
'
' KEEP THE RUNNING y IN STEP WITH frmBuildConfig_PositionWindows. The two walk the same rows,
' and the label rect a row draws into is the row the control is placed in.
' ========================================================================================
private function frmBuildConfig_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( theme.panel.text.backcolor )
    buffer.PaintRect( @rc )

    dim as long mx      = AfxScaleX(FRMBUILDCONFIG_MARGIN)
    dim as long titleH  = AfxScaleY(FRMBUILDCONFIG_TITLE_H)
    dim as long detailX = mx + AfxScaleX(FRMBUILDCONFIG_LIST_W) + AfxScaleX(FRMBUILDCONFIG_DETAIL_GAP)
    dim as long rowH    = AfxScaleY(FRMBUILDCONFIG_ROWH)
    dim as long xRight  = rc.right - mx

    ' NOTHING IS DRAWN IN THE TITLE BAND -- it holds the list's four command buttons. A page
    ' title there would repeat the window caption an inch below itself.

    dim as RECT rcT

    ' ---- the empty state ---------------------------------------------------------------
    if gBCCurrent < 0 then
        rcT.left   = detailX
        rcT.right  = xRight
        rcT.top    = titleH
        rcT.bottom = rc.bottom - AfxScaleY(FRMBUILDCONFIG_FOOTER_H)
        buffer.SetForeColor( theme.panel.text.forecolor )
        buffer.SetFont( ghFont(GUIFONT_11) )
        buffer.PaintText( L(389,"Select a build configuration, or press Add to create one."), _
                          @rcT, DT_CENTER or DT_VCENTER or DT_SINGLELINE )
        buffer.EndDoubleBuffer()
        EndPaint( hwnd, @ps )
        return 0
    end if

    dim as long y = titleH + AfxScaleY(FRMBUILDCONFIG_SELECTBAR_H) + AfxScaleY(12)

    buffer.SetFont( ghFont(GUIFONT_11) )
    buffer.SetForeColor( theme.panel.text.forecolor )

    if gBCPage = IDP_FRMBUILDCONFIG_GENERAL then

        ' ---- Description: a STACKED row, label above a full-width field -----------------
        rcT.left = detailX : rcT.right = xRight
        rcT.top  = y       : rcT.bottom = y + AfxScaleY(FRMBUILDCONFIG_STACKLABEL_H)
        buffer.PaintText( L(216,"Description"), @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
        y += AfxScaleY(FRMBUILDCONFIG_STACKROW_H)

        ' ---- the label/control rows -----------------------------------------------------
        ' The rects come from PositionWindows, which is the only place that knows where each
        ' control landed -- so each label gets exactly the width its own control leaves, and
        ' the painter cannot drift out of step with the layout by walking a parallel y.
        '
        ' NO WARNING TEXT ON THE SHORTCUT ROW. There was: a red sentence beside the button
        ' when the accelerator was unresolvable or already taken. A shortcut that cannot work
        ' is now simply UNASSIGNED when the staging copy is seeded, and the assign dialog
        ' refuses to create another -- so the failure is reported where the choice is made,
        ' not restated on a row the user has not touched every time they open the dialog.
        #macro BCROWLABEL( _row_, _text_ )
            buffer.PaintText( _text_, @gBCRowLabelRect(_row_), _
                              DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
        #endmacro

        BCROWLABEL( BCROW_DEFAULT,  L(218,"Set as default") )
        BCROWLABEL( BCROW_TARGET,   L(358,"Compiler target") )
        BCROWLABEL( BCROW_SHORTCUT, L(338,"Shortcut") )
        BCROWLABEL( BCROW_EXCLUDE,  L(220,"Exclude build from status bar popup menu") )

        ' ---- the switches preview -------------------------------------------------------
        ' Its rect is computed in PositionWindows and stored, because the mouse handlers
        ' hit-test it -- there is no child window here to do that for us.
        dim as RECT rcLbl = gBCPreviewLabelRect
        buffer.SetForeColor( theme.panel.text.forecolor )
        buffer.PaintText( L(356,"Compiler switches"), @rcLbl, _
                          DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

        ' FILL FIRST, THEN STROKE. PaintRoundOutline strokes without filling and takes its
        ' colour from SetForeColor; PaintRoundBorderRect would fill the interior back over --
        ' the mistake this family has made four separate times.
        dim as RECT rcPrev = gBCPreviewRect
        buffer.SetBackColor( iif( gBCPreviewHot, theme.panel.texthot.backcolor, theme.ui.field.backcolor ) )
        buffer.PaintRoundRect( @rcPrev, AfxScaleY(6) )
        buffer.SetForeColor( theme.ui.divider )
        buffer.PaintRoundOutline( @rcPrev, AfxScaleY(6), 1 )

        dim as RECT rcTxt = rcPrev
        rcTxt.left   += AfxScaleX(10)
        rcTxt.right  -= AfxScaleX(10)
        rcTxt.top    += AfxScaleY(6)
        rcTxt.bottom -= AfxScaleY(6)

        ' THE RAW OPTIONS STRING, in the fixed font -- exactly what is handed to fbc, so it can
        ' never disagree with reality, and it includes any switch typed by hand that has no
        ' checkbox. Wrapped, not single-lined: PaintTextEx, because PaintText forces
        ' DT_SINGLELINE and structurally cannot wrap.
        dim as DWSTRING wszOpts = gBuildsWork(gBCCurrent).wszOptions
        if PsLen(PsTrim(wszOpts)) = 0 then
            buffer.SetFont( ghFont(GUIFONT_11) )
            buffer.SetForeColor( theme.topmenu.textdisabled.forecolor )
            buffer.PaintText( L(343,"(none)"), @rcTxt, DT_LEFT or DT_VCENTER or DT_SINGLELINE )
        else
            buffer.SetFont( ghBCFontFixed )
            buffer.SetForeColor( theme.ui.field.forecolor )
            buffer.PaintTextEx( wszOpts, @rcTxt, DT_LEFT or DT_TOP or DT_WORDBREAK or DT_END_ELLIPSIS )
        end if

    else
        ' ---- the compiler switches page -------------------------------------------------
        rcT.left = detailX : rcT.right = xRight
        rcT.top  = y       : rcT.bottom = y + AfxScaleY(FRMBUILDCONFIG_STACKLABEL_H)
        buffer.PaintText( L(217,"Compiler Options"), @rcT, DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )
    end if

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Position the shell's children. Fixed-size dialog, so this runs once after creation.
' ========================================================================================
private sub frmBuildConfig_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long mx      = AfxScaleX(FRMBUILDCONFIG_MARGIN)
    dim as long titleH  = AfxScaleY(FRMBUILDCONFIG_TITLE_H)
    dim as long footerH = AfxScaleY(FRMBUILDCONFIG_FOOTER_H)
    dim as long gap     = AfxScaleX(FRMBUILDCONFIG_GAP)
    dim as long btnW    = AfxScaleX(FRMBUILDCONFIG_BTN_W)
    dim as long btnH    = AfxScaleY(FRMBUILDCONFIG_BTN_H)
    dim as long listW   = AfxScaleX(FRMBUILDCONFIG_LIST_W)
    dim as long xRight  = rc.right - mx

    ' ---- the list's command strip, in the title band ------------------------------------
    dim as long cmdW   = AfxScaleX(FRMBUILDCONFIG_CMD_W)
    dim as long glyphW = AfxScaleX(FRMBUILDCONFIG_GLYPH_W)
    dim as long cmdY   = (titleH - btnH) \ 2
    dim as long x      = mx
    SetWindowPos( ghBCAdd, 0, x, cmdY, cmdW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    x += cmdW + gap
    SetWindowPos( ghBCDelete, 0, x, cmdY, cmdW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    x += cmdW + (gap * 2)
    SetWindowPos( ghBCUp, 0, x, cmdY, glyphW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    x += glyphW + AfxScaleX(4)
    SetWindowPos( ghBCDown, 0, x, cmdY, glyphW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the list ----------------------------------------------------------------------
    dim as long listH = rc.bottom - footerH - titleH
    if listH < 0 then listH = 0
    SetWindowPos( HWND_FRMBUILDCONFIG_LIST, 0, mx, titleH, listW, listH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- the right pane ----------------------------------------------------------------
    dim as long detailX = mx + listW + AfxScaleX(FRMBUILDCONFIG_DETAIL_GAP)
    dim as long rowH    = AfxScaleY(FRMBUILDCONFIG_ROWH)
    dim as long fieldH  = AfxScaleY(FRMBUILDCONFIG_FIELD_H)
    dim as long barH    = AfxScaleY(FRMBUILDCONFIG_SELECTBAR_H)
    dim as long paneW   = xRight - detailX
    dim as long paneBot = rc.bottom - footerH

    SetWindowPos( ghBCSelectBar, 0, detailX, titleH, paneW, barH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long y = titleH + barH + AfxScaleY(12)

    ' ---- General page ------------------------------------------------------------------
    ' Description is stacked: its field sits under its own label.
    SetWindowPos( ghBCDescription, 0, detailX, y + AfxScaleY(FRMBUILDCONFIG_STACKLABEL_H), _
                  paneW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += AfxScaleY(FRMBUILDCONFIG_STACKROW_H)

    ' Every row places its control right-aligned, then hands the label EVERYTHING to its left
    ' less one gap. That is what lets "Exclude build from status bar popup menu" have the room
    ' a 47px toggle is not using, instead of being ellipsized by a gutter sized for the
    ' shortest caption in the group.
    #macro PLACEROW( _row_, _ctrlX_ )
        gBCRowLabelRect(_row_).left   = detailX
        gBCRowLabelRect(_row_).right  = (_ctrlX_) - gap
        gBCRowLabelRect(_row_).top    = y
        gBCRowLabelRect(_row_).bottom = y + rowH
        y += rowH
    #endmacro

    dim as long tw = 0, th = 0
    PsToggle_GetIdealSize( ghBCTogDefault, tw, th )
    SetWindowPos( ghBCTogDefault, 0, xRight - tw, y + (rowH - th) \ 2, tw, th, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    PLACEROW( BCROW_DEFAULT, xRight - tw )

    ' PsComboBox sizes itself to its WIDEST item, so picking never resizes it. Clamped so it
    ' cannot eat the whole row -- a translation longer than "FBC 64-bit" is what would
    ' otherwise leave its label nowhere to go.
    dim as long cbW = 0, cbH = 0
    PsComboBox_GetIdealSize( ghBCTarget, cbW, cbH )
    dim as long cbMax = (paneW * 2) \ 5
    if cbW > cbMax then cbW = cbMax
    SetWindowPos( ghBCTarget, 0, xRight - cbW, y + (rowH - cbH) \ 2, cbW, cbH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    PLACEROW( BCROW_TARGET, xRight - cbW )

    dim as long scW = AfxScaleX(FRMBUILDCONFIG_SHORTCUT_W)
    SetWindowPos( ghBCShortcut, 0, xRight - scW, y + (rowH - fieldH) \ 2, scW, fieldH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    PLACEROW( BCROW_SHORTCUT, xRight - scW )

    PsToggle_GetIdealSize( ghBCTogExclude, tw, th )
    SetWindowPos( ghBCTogExclude, 0, xRight - tw, y + (rowH - th) \ 2, tw, th, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    PLACEROW( BCROW_EXCLUDE, xRight - tw )

    ' ---- the preview block -------------------------------------------------------------
    ' IT FOLLOWS THE ROWS. It was bottom-anchored first, on the theory that a summary belongs
    ' at the foot of the page -- and the geometry trace showed the result: the last row ended
    ' at y=558 and the block began at y=850, leaving 292px (167 unscaled) of dead band through
    ' the middle of the page at 175% DPI. Every fit assertion passed, because nothing
    ' overlapped and nothing was clipped; the page was simply broken-looking. Empty space
    ' BELOW a short page is ordinary. A hole through the middle of one is not.
    dim as long prevH  = AfxScaleY(FRMBUILDCONFIG_PREVIEW_H)
    dim as long prevLH = AfxScaleY(FRMBUILDCONFIG_PREVIEWLABEL_H)
    dim as long editW  = AfxScaleX(FRMBUILDCONFIG_EDIT_W)
    dim as long prevY  = y + AfxScaleY(12) + prevLH

    gBCPreviewLabelRect.left   = detailX
    gBCPreviewLabelRect.right  = xRight - editW - gap
    gBCPreviewLabelRect.top    = prevY - prevLH
    gBCPreviewLabelRect.bottom = prevY

    gBCPreviewRect.left   = detailX
    gBCPreviewRect.right  = xRight
    gBCPreviewRect.top    = prevY
    gBCPreviewRect.bottom = prevY + prevH

    SetWindowPos( ghBCEditSwitches, 0, xRight - editW, _
                  gBCPreviewLabelRect.top + (prevLH - AfxScaleY(24)) \ 2, _
                  editW, AfxScaleY(24), SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- Compiler switches page --------------------------------------------------------
    ' Same origin as the General page's first row, so switching tabs does not shift the eye.
    dim as long ys = titleH + barH + AfxScaleY(12)
    SetWindowPos( ghBCOptions, 0, detailX, ys + AfxScaleY(FRMBUILDCONFIG_STACKLABEL_H), _
                  paneW, fieldH, SWP_NOZORDER or SWP_NOACTIVATE )
    ys += AfxScaleY(FRMBUILDCONFIG_STACKROW_H)
    dim as long swH = paneBot - AfxScaleY(12) - ys
    if swH < 0 then swH = 0
    SetWindowPos( ghBCSwitches, 0, detailX, ys, paneW, swH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- footer ------------------------------------------------------------------------
    dim as long btnY = rc.bottom - footerH + (footerH - btnH) \ 2
    SetWindowPos( ghBCCancel, 0, xRight - btnW, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghBCOK, 0, xRight - (btnW * 2) - gap, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Push the current theme into every child that carries its own colours.
'
' Through the Options dialog's shared builders rather than a hand-copied recipe -- this file
' is included after modOptionsRows.inc, so the palettes cannot drift.
' ========================================================================================
private sub frmBuildConfig_ApplyTheme( byval hwnd as HWND )

    #macro DRESSLIST( _h_ )
        PsListTree_SetBackColor( _h_, theme.panel.text.backcolor )
        PsListTree_SetScrollBarColors( _h_, theme.panel.scrollbar.backcolor, _
                                       theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
        OptionsTheme_ApplyListBorder( _h_ )
    #endmacro

    DRESSLIST( HWND_FRMBUILDCONFIG_LIST )
    DRESSLIST( ghBCSwitches )

    OptionsTheme_ApplyTextBox( ghBCDescription )
    OptionsTheme_ApplyTextBox( ghBCOptions )
    ' The raw options field is a fixed-pitch readout of what goes to the compiler, so it uses
    ' the same face the preview block does -- applied AFTER ApplyTextBox, which sets the base
    ' font itself and would otherwise put it back.
    PsTextBox_SetFont( ghBCOptions, ghBCFontFixed )

    dim as PSTOGGLE_COLORS tc
    OptionsTheme_FillToggle( @tc )
    PsToggle_SetColors( ghBCTogDefault, @tc )
    PsToggle_SetColors( ghBCTogExclude, @tc )

    dim as PSCOMBOBOX_COLORS cc
    dim as PSPOPUPMENU_COLORS pc
    OptionsTheme_FillCombo( @cc, @pc )
    PsComboBox_SetColors( ghBCTarget, @cc )
    PsComboBox_SetListColors( ghBCTarget, @pc )

    dim as PSSELECTBAR_COLORS sc
    PsSelectBar_GetColors( ghBCSelectBar, @sc )
    sc.BackColor        = theme.panel.text.backcolor
    sc.BackColorHot     = theme.panel.text.backcolor
    sc.BackColorSelect  = theme.panel.text.backcolor
    sc.ForeColor        = theme.topmenu.textdisabled.forecolor
    sc.ForeColorHot     = theme.panel.text.forecolor
    sc.ForeColorSelect  = theme.panel.texthot.forecolor
    sc.IndicatorColor   = theme.panel.texthot.forecolor
    PsSelectBar_SetColors( ghBCSelectBar, @sc )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    dim as HWND hBtn(7) = { ghBCAdd, ghBCDelete, ghBCUp, ghBCDown, _
                            ghBCShortcut, ghBCEditSwitches, ghBCOK, ghBCCancel }
    for i as long = 0 to 7
        if hBtn(i) = 0 then continue for
        PsButton_SetColors( hBtn(i), @bc )
        PsButton_Refresh( hBtn(i) )
    next

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' Is hCandidate hContainer, or anything nested inside it?
'
' Needed because every focusable thing here is a CONTAINER whose real tab stop is an inner
' child -- a PsTextBox's focus lives on the RichEdit two levels down, and PsListTree's on its
' row surface, so GetFocus() = hControl is never true.
' ========================================================================================
private function frmBuildConfig_IsWithin( byval hCandidate as HWND, byval hContainer as HWND ) as boolean
    if (hCandidate = 0) orelse (hContainer = 0) then return false
    dim as HWND h = hCandidate
    do while h <> 0
        if h = hContainer then return true
        h = GetParent( h )
    loop
    return false
end function





' ========================================================================================
' A child's rect in the DIALOG's client coordinates.
'
' The layout suite compares controls against the shell's own bands, so every measurement has
' to be in one coordinate space. GetWindowRect gives screen coordinates; two ScreenToClient
' calls bring both corners back. A HIDDEN window still reports its last placed rect here,
' which is why the fit assertions also check the rect is non-empty -- a zeroed rect "fits
' inside" everything and would pass vacuously.
' ========================================================================================
private function frmBuildConfig_ChildRect( byval hChild as HWND ) as RECT
    dim as RECT rc
    if hChild = 0 then return rc
    GetWindowRect( hChild, @rc )
    dim as POINT pt1 = type( rc.left, rc.top )
    dim as POINT pt2 = type( rc.right, rc.bottom )
    ScreenToClient( HWND_FRMBUILDCONFIG, @pt1 )
    ScreenToClient( HWND_FRMBUILDCONFIG, @pt2 )
    rc.left = pt1.x : rc.top = pt1.y
    rc.right = pt2.x : rc.bottom = pt2.y
    return rc
end function


' ========================================================================================
' THE HEADLESS HALF.  (TIKO_BUILDCONFIG_SELFTEST=1)
'
' The switch matcher and the localized ids -- neither needs a window, so this runs from
' frmMain at startup alongside the other headless suites. The layout half is separate and
' runs from Show, because it measures real windows at the real DPI.
'
' WHY THE MATCHER IS WORTH ASSERTING AT ALL. It replaced a space-padded instr() plus a blind
' PsStrRemove, and every one of the cases below is a defect that code actually had -- the
' two paths disagreeing about "checked", a quoted path losing switches out of its middle, a
' tab-separated string matching nothing and toggling a duplicate in. None of them raises an
' error; the result is always a shorter, valid-looking options string.
' ========================================================================================
sub frmBuildConfig_RunSelfTest()

    if environ("TIKO_BUILDCONFIG_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    ' Two macros because a string comparison wants to REPORT both sides -- a bare pass/fail on
    ' "the options string is right" is close to useless when it is not.
    #macro BCEQ( _got_, _want_, _desc_ )
        scope
            dim as DWSTRING wszGot = (_got_), wszWant = (_want_)
            if wszGot = wszWant then
                nPass += 1
            else
                nFail += 1
                sMsg = "  FAIL: " & (_desc_) & !"\n         got  [" & wszGot & _
                       !"]\n         want [" & wszWant & "]"
                print sMsg
            end if
        end scope
    #endmacro

    #macro BCASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sMsg = "  FAIL: " & (_desc_)
            print sMsg
        end if
    #endmacro

    sMsg = "==== TIKO_BUILDCONFIG_SELFTEST ====" : print sMsg

    ' ---- 1. the switch table, indexed by SWITCH rather than by position ------------------
    ' So the cases below name switches instead of magic numbers, and the table can be
    ' re-ordered without silently retargeting every assertion at the wrong row.
    dim as long iConsole = -1, iGui = -1, iGas64 = -1, iGcc = -1, iErrE = -1
    dim as long iO1 = -1, iO2 = -1, iSmallC = -1, iBigC = -1, iSmallR = -1, iBigR = -1
    for i as long = lbound(gBuildOptions) to ubound(gBuildOptions)
        dim as DWSTRING a = AfxStrExtract( gBuildOptions(i).wszText, "(", ")" )
        if      a = "-s console" then : iConsole = i
        elseif  a = "-s gui"     then : iGui     = i
        elseif  a = "-gen gas64" then : iGas64   = i
        elseif  a = "-gen gcc"   then : iGcc     = i
        elseif  a = "-e"         then : iErrE    = i
        elseif  a = "-Wc -O1"    then : iO1      = i
        elseif  a = "-Wc -O2"    then : iO2      = i
        elseif  a = "-c"         then : iSmallC  = i
        elseif  a = "-C"         then : iBigC    = i
        elseif  a = "-r"         then : iSmallR  = i
        elseif  a = "-R"         then : iBigR    = i
        end if
    next
    BCASSERT( (iConsole >= 0) andalso (iGui >= 0) andalso (iGas64 >= 0) andalso (iGcc >= 0) andalso _
              (iErrE >= 0) andalso (iO1 >= 0) andalso (iO2 >= 0) andalso (iSmallC >= 0) andalso _
              (iBigC >= 0) andalso (iSmallR >= 0) andalso (iBigR >= 0), _
              "every switch the suite names is present in gBuildOptions" )

    ' Everything below is guarded on that, so a renamed switch reports ONE failure rather than
    ' eleven confusing ones from index -1.
    if iConsole >= 0 andalso iGui >= 0 andalso iGas64 >= 0 andalso iGcc >= 0 andalso iErrE >= 0 andalso _
       iO1 >= 0 andalso iO2 >= 0 andalso iSmallC >= 0 andalso iBigC >= 0 andalso iSmallR >= 0 andalso iBigR >= 0 then

        ' ---- 2. the display path and the toggle path agree ------------------------------
        ' They did NOT before: the painter padded both sides of its comparison and the click
        ' handler padded only the right, so "-Wc-e" was drawn unchecked and toggled as though
        ' it were checked. One string, two answers.
        BCASSERT( BuildOpts_IsChecked( "-Wc-e", iErrE ) = false, _
                  "'-Wc-e' does not contain the token -e" )

        ' ---- 3. -Wc -O1 versus -Wc -O2, distinguished as SEQUENCES ----------------------
        ' Two tokens each, differing in the second. Nothing here is a substring test.
        BCASSERT( BuildOpts_IsChecked( "-s console -Wc -O2", iO2 ) = true,  "-Wc -O2 is found" )
        BCASSERT( BuildOpts_IsChecked( "-s console -Wc -O2", iO1 ) = false, "-Wc -O1 is NOT found in a -O2 string" )
        BCEQ( BuildOpts_Toggle( "-s console -Wc -O2", iO1 ), "-s console -Wc -O1", _
              "-O1 replaces -O2 within group 5" )

        ' ---- 4. CASE SENSITIVITY --------------------------------------------------------
        ' -c/-C and -r/-R are four distinct FBC switches in three groups. This is the single
        ' most likely way to get the matcher wrong, and it has no visible symptom until a
        ' build behaves oddly. A/B-verified: with the compare case-folded, the last assertion
        ' here returns an EMPTY options string -- both switches gone.
        BCASSERT( BuildOpts_IsChecked( "-c", iSmallC ) = true,  "-c matches -c" )
        BCASSERT( BuildOpts_IsChecked( "-c", iBigC )   = false, "-c does NOT match -C" )
        BCASSERT( BuildOpts_IsChecked( "-R", iSmallR ) = false, "-R does NOT match -r" )
        BCASSERT( BuildOpts_IsChecked( "-R", iBigR )   = true,  "-R matches -R" )
        BCEQ( BuildOpts_Toggle( "-c", iBigC ), "-c -C", "-C does not displace -c (different groups)" )

        ' ---- 5. TAB-SEPARATED input -----------------------------------------------------
        ' The old code split on literal spaces only, so a line pasted from a makefile matched
        ' nothing -- and toggling then APPENDED A DUPLICATE of a switch already present.
        BCASSERT( BuildOpts_IsChecked( !"-s\tconsole\t-lang fb", iConsole ) = true, _
                  "a tab-separated -s console is found" )
        BCEQ( BuildOpts_Toggle( !"-s\tconsole\t-lang fb", iConsole ), "-lang fb", _
              "toggling a tab-separated switch removes it rather than duplicating it" )

        ' ---- 6. hand-typed switches survive round-tripping ------------------------------
        BCEQ( BuildOpts_Toggle( "-mt -s console -v", iGui ), "-mt -v -s gui", _
              "unknown tokens -mt and -v survive a group-0 toggle, in order" )
        BCEQ( BuildOpts_Toggle( "-Wc -O1 -someflag", iO2 ), "-someflag -Wc -O2", _
              "only the group's own sequence is removed" )

        ' ---- 7. a checked row toggles OFF -----------------------------------------------
        BCEQ( BuildOpts_Toggle( "-s console", iConsole ), "", "clicking a checked row unchecks it" )
        BCEQ( BuildOpts_Toggle( "", iGas64 ), "-gen gas64", "toggling into an empty string" )
        BCEQ( BuildOpts_Toggle( BuildOpts_Toggle( "-lang fb", iGcc ), iGcc ), "-lang fb", _
              "on then off returns the original" )

        ' ---- 8. whitespace normalisation, which is confined to the TOGGLE path ----------
        BCEQ( BuildOpts_Toggle( "-s   console    -lang fb", iGas64 ), "-s console -lang fb -gen gas64", _
              "toggling normalises runs of whitespace" )
    end if

    ' ---- 9. EVERY LOCALIZED ID THIS DIALOG OWNS MUST RESOLVE ----------------------------
    ' L(id,"default") DISCARDS the default -- it is #Define L(e,s) LL(e) -- so a missing entry
    ' renders as an EMPTY STRING: a blank caption, a blank column header, a blank button, with
    ' nothing in the build or the warnings to say so. This is the one failure this dialog can
    ' have that looks like nothing at all, which is exactly why it is asserted rather than
    ' eyeballed. A/B-verified by deleting 00367 from english.lang.
    #macro BCID( _id_ )
        BCASSERT( len( LL(_id_) ) > 0, "localized id " & str(_id_) & " resolves" )
    #endmacro
    ' The exact set this file references -- kept in step with it, not a superset. 356 ("this
    ' shortcut cannot fire") and 366 ("already used by") were dropped when the warnings gave
    ' way to simply unassigning a shortcut that cannot work; 366 is still live, but it belongs
    ' to frmAssignKey now and is asserted there by use.
    BCID( 0 )   : BCID( 1 )   : BCID( 14 )  : BCID( 218 ) : BCID( 219 )
    BCID( 220 ) : BCID( 221 ) : BCID( 222 ) : BCID( 223 ) : BCID( 224 )
    BCID( 227 ) : BCID( 342 ) : BCID( 349 ) : BCID( 350 ) : BCID( 351 )
    BCID( 352 ) : BCID( 353 ) : BCID( 354 ) : BCID( 365 ) : BCID( 367 )
    BCID( 368 ) : BCID( 369 ) : BCID( 398 ) : BCID( 399 ) : BCID( 400 )

    ' ---- 10. the shortcut composer -------------------------------------------------------
    ' MODIFIER ORDER IS Ctrl+Shift+Alt here, matching createToolsMenuShortcut and the Tools
    ' menu it sits inches from -- NOT the Ctrl+Alt+Shift that keybindings.ini stores. The two
    ' coexist deliberately, and the clash test calls frmKeyboardEdit_BuildKeyString rather
    ' than composing a second time. Asserted because getting it wrong makes every
    ' three-modifier shortcut silently report free.
    BCEQ( frmBuildConfig_ComposeShortcut( "F5", 1, 1, 1 ), "Ctrl+Shift+Alt+F5", _
          "the composer emits Ctrl+Shift+Alt, in that order" )
    BCEQ( frmBuildConfig_ComposeShortcut( "F5", 1, 0, 1 ), "Ctrl+Alt+F5", _
          "a skipped modifier does not disturb the order of the rest" )
    ' "" and "None" are BOTH no-shortcut. The old dialog wrote the literal "None" into wszKey
    ' on every selection change and persisted it, so every keyless build in every existing
    ' settings.ini carries it. Treating it as a real key is what produced "Ctrl+NONE".
    BCEQ( frmBuildConfig_ComposeShortcut( "", 1, 0, 0 ), "", "no key means no shortcut" )
    BCEQ( frmBuildConfig_ComposeShortcut( "None", 1, 0, 0 ), "", _
          "the legacy 'None' affordance is not a key" )
    BCEQ( frmBuildConfig_ComposeShortcut( "none", 1, 1, 1 ), "", _
          "...whatever its casing" )

    sMsg = "==== BUILDCONFIG: " & str(nPass) & " passed, " & str(nFail) & " failed ====" : print sMsg

end sub


' ========================================================================================
' THE LAYOUT HALF.  (TIKO_BUILDCONFIG_SELFTEST=1, and the dialog must be OPEN)
'
' Runs from Show once the controls exist and the list has been filled, so it measures the
' real windows at the real DPI rather than the unscaled constants.
'
' IT EXISTS BECAUSE THE DIALOG IS FIXED SIZE. Nothing here scrolls, so a control pushed past
' the viewport does not grow a scrollbar -- it is CLIPPED, silently, and only above 100% DPI
' where nobody is looking.
'
' What it cannot see: the ONE real layout defect found during the rebuild was a 292px dead
' band through the middle of the General page, and every fit assertion below passed straight
' through it, because nothing overlapped and nothing was clipped. A gap is not a violation.
' That one needed the geometry dumped and read.
' ========================================================================================
private sub frmBuildConfig_RunLayoutSelfTest()

    if environ("TIKO_BUILDCONFIG_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro LAYASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sMsg = "  FAIL: " & (_desc_)
            print sMsg
        end if
    #endmacro

    sMsg = "==== TIKO_BUILDCONFIG_SELFTEST (layout) ====" : print sMsg

    ' ---- 1. the shell -------------------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( HWND_FRMBUILDCONFIG, @rcClient )
    LAYASSERT( rcClient.right  = AfxScaleX(FRMBUILDCONFIG_CLIENT_W), "client width is the declared width" )
    LAYASSERT( rcClient.bottom = AfxScaleY(FRMBUILDCONFIG_CLIENT_H), "client height is the declared height" )

    ' The fixed size is not a preference, it is what makes every viewport one number. A
    ' resizable frame would make each fit assertion below meaningless.
    dim as long dwStyle = GetWindowLongPtr( HWND_FRMBUILDCONFIG, GWL_STYLE )
    LAYASSERT( (dwStyle and WS_THICKFRAME) = 0, "the dialog is not resizable" )

    dim as long titleH  = AfxScaleY(FRMBUILDCONFIG_TITLE_H)
    dim as long footerH = AfxScaleY(FRMBUILDCONFIG_FOOTER_H)
    dim as long barH    = AfxScaleY(FRMBUILDCONFIG_SELECTBAR_H)
    dim as long paneTop = titleH + barH
    dim as long paneBot = rcClient.bottom - footerH

    ' ---- 2. the list fills the body -----------------------------------------------------
    dim as RECT rcList = frmBuildConfig_ChildRect( HWND_FRMBUILDCONFIG_LIST )
    LAYASSERT( (rcList.right > rcList.left) andalso (rcList.bottom > rcList.top), _
               "the list has a non-empty rect" )
    LAYASSERT( rcList.top = titleH, "the list starts at the foot of the title band" )
    LAYASSERT( rcList.bottom = paneBot, "the list ends at the head of the footer" )
    LAYASSERT( rcList.right - rcList.left = AfxScaleX(FRMBUILDCONFIG_LIST_W), _
               "the list is the declared width" )

    ' ---- 3. the select bar sits above the pages, not over them -------------------------
    dim as RECT rcBar = frmBuildConfig_ChildRect( ghBCSelectBar )
    LAYASSERT( rcBar.left >= rcList.right, "the select bar starts right of the list" )
    LAYASSERT( rcBar.top = titleH, "the select bar starts at the foot of the title band" )
    LAYASSERT( rcBar.bottom = paneTop, "the select bar ends where the page area begins" )

    ' ---- 4. NOTHING ON EITHER PAGE IS CLIPPED -------------------------------------------
    ' The whole point. Both pages are measured, not just the visible one -- a hidden window
    ' still reports its last placed rect, which is why each is also checked non-empty.
    #macro FITS( _h_, _name_ )
        scope
            dim as RECT rcF = frmBuildConfig_ChildRect( _h_ )
            LAYASSERT( (rcF.right > rcF.left) andalso (rcF.bottom > rcF.top), _
                       _name_ & " has a non-empty rect" )
            LAYASSERT( rcF.bottom <= paneBot, _name_ & " fits above the footer" )
            LAYASSERT( rcF.top >= paneTop, _name_ & " starts below the select bar" )
            LAYASSERT( rcF.right <= rcClient.right - AfxScaleX(FRMBUILDCONFIG_MARGIN), _
                       _name_ & " fits inside the right margin" )
            LAYASSERT( rcF.left >= rcBar.left, _name_ & " starts inside the right pane" )
        end scope
    #endmacro

    FITS( ghBCDescription, "Description" )
    FITS( ghBCTogDefault,  "Set as default" )
    FITS( ghBCTarget,      "Compiler target" )
    FITS( ghBCShortcut,    "Shortcut" )
    FITS( ghBCTogExclude,  "Exclude from popup" )
    FITS( ghBCEditSwitches,"Edit" )
    FITS( ghBCOptions,     "raw options" )
    FITS( ghBCSwitches,    "switch list" )

    ' The preview block is not a window, so it is checked from its stored rects.
    LAYASSERT( gBCPreviewRect.bottom > gBCPreviewRect.top, "the preview block has a non-empty rect" )
    LAYASSERT( gBCPreviewRect.bottom <= paneBot, "the preview block fits above the footer" )
    LAYASSERT( gBCPreviewLabelRect.bottom = gBCPreviewRect.top, _
               "the preview's label sits directly on top of it" )

    ' ---- 5. the General page reads top-down, with no row overlapping the next ----------
    dim as RECT rcDesc = frmBuildConfig_ChildRect( ghBCDescription )
    dim as RECT rcTogD = frmBuildConfig_ChildRect( ghBCTogDefault )
    dim as RECT rcTgt  = frmBuildConfig_ChildRect( ghBCTarget )
    dim as RECT rcSC   = frmBuildConfig_ChildRect( ghBCShortcut )
    dim as RECT rcTogE = frmBuildConfig_ChildRect( ghBCTogExclude )
    LAYASSERT( rcDesc.bottom <= rcTogD.top, "Description does not overlap Set as default" )
    LAYASSERT( rcTogD.bottom <= rcTgt.top,  "Set as default does not overlap Compiler target" )
    LAYASSERT( rcTgt.bottom  <= rcSC.top,   "Compiler target does not overlap Shortcut" )
    LAYASSERT( rcSC.bottom   <= rcTogE.top, "Shortcut does not overlap Exclude from popup" )
    LAYASSERT( rcTogE.bottom <= gBCPreviewLabelRect.top, _
               "the last row does not overlap the preview block" )

    ' ---- 5b. EVERY ROW LABEL FITS WITHOUT ELLIPSIS -------------------------------------
    ' The row this exists for is "Exclude build from status bar popup menu", which a fixed
    ' label gutter rendered as "Exclude build from status bar po...". Each label now runs to
    ' its OWN control's left edge less a gap, so a long caption borrows the room a small
    ' toggle is not using -- and that only holds if it is measured, because DrawText clips
    ' with DT_END_ELLIPSIS and reports nothing.
    scope
        dim as HDC hDC = GetDC( HWND_FRMBUILDCONFIG )
        if hDC then
            dim as HFONT hOld = SelectObject( hDC, ghFont(GUIFONT_11) )

            #macro LABELFITS( _row_, _text_ )
                scope
                    dim as DWSTRING wszL = (_text_)
                    dim as SIZEL sz
                    GetTextExtentPoint32( hDC, wszL.sptr, PsLen(wszL), @sz )
                    dim as long avail = gBCRowLabelRect(_row_).right - gBCRowLabelRect(_row_).left
                    LAYASSERT( avail >= sz.cx, _
                               "the label """ & wszL & """ fits (" & str(sz.cx) & _
                               "px into " & str(avail) & "px)" )
                end scope
            #endmacro

            LABELFITS( BCROW_DEFAULT,  L(218,"Set as default") )
            LABELFITS( BCROW_TARGET,   L(358,"Compiler target") )
            LABELFITS( BCROW_SHORTCUT, L(338,"Shortcut") )
            LABELFITS( BCROW_EXCLUDE,  L(220,"Exclude build from status bar popup menu") )

            ' And the contract the rects are built on: a label stops exactly one gap short of
            ' its own row's control, so the two can never overlap and no row is using a gutter
            ' sized for a different row.
            #macro LABELMEETS( _row_, _h_ )
                scope
                    dim as RECT rcC = frmBuildConfig_ChildRect( _h_ )
                    LAYASSERT( gBCRowLabelRect(_row_).right = rcC.left - AfxScaleX(FRMBUILDCONFIG_GAP), _
                               "row " & str(_row_) & "'s label stops one gap short of its control" )
                end scope
            #endmacro

            LABELMEETS( BCROW_DEFAULT,  ghBCTogDefault )
            LABELMEETS( BCROW_TARGET,   ghBCTarget )
            LABELMEETS( BCROW_SHORTCUT, ghBCShortcut )
            LABELMEETS( BCROW_EXCLUDE,  ghBCTogExclude )

            if hOld then SelectObject( hDC, hOld )
            ReleaseDC( HWND_FRMBUILDCONFIG, hDC )
        end if
    end scope

    ' ---- 5c. the list has exactly TWO columns ------------------------------------------
    ' The Shortcut column was removed; a third would silently squeeze Description again.
    LAYASSERT( PsListTree_GetColumnCount( HWND_FRMBUILDCONFIG_LIST ) = 2, _
               "the builds list has two columns" )

    ' ---- 6. both pages start at the same y ---------------------------------------------
    ' So switching tabs does not shift the eye. Asserted because the two are laid out by
    ' separate arithmetic and could drift apart without anything else noticing.
    dim as RECT rcOpts = frmBuildConfig_ChildRect( ghBCOptions )
    LAYASSERT( rcOpts.top = rcDesc.top, "both pages' first field starts at the same y" )

    ' ---- 7. the footer ------------------------------------------------------------------
    dim as RECT rcOK = frmBuildConfig_ChildRect( ghBCOK )
    dim as RECT rcCa = frmBuildConfig_ChildRect( ghBCCancel )
    LAYASSERT( rcOK.top >= paneBot, "OK is in the footer band" )
    LAYASSERT( rcOK.right <= rcCa.left, "OK is left of Cancel" )
    LAYASSERT( rcCa.right = rcClient.right - AfxScaleX(FRMBUILDCONFIG_MARGIN), _
               "Cancel is flush with the right margin" )

    ' ---- 8. tab order -------------------------------------------------------------------
    ' BY CONTAINMENT, not by handle: every focusable control here is a container whose real
    ' tab stop is an inner child, so GetNextDlgTabItem returns something INSIDE the control
    ' rather than the handle this file holds.
    dim as HWND hFirst = GetNextDlgTabItem( HWND_FRMBUILDCONFIG, 0, false )
    LAYASSERT( hFirst <> 0, "the dialog has at least one tab stop" )

    dim as long nStops = 0
    dim as boolean bSawList = false, bSawDesc = false, bSawOK = false
    dim as HWND h = hFirst
    do
        if h = 0 then exit do
        nStops += 1
        if frmBuildConfig_IsWithin( h, HWND_FRMBUILDCONFIG_LIST ) then bSawList = true
        if frmBuildConfig_IsWithin( h, ghBCDescription ) then bSawDesc = true
        if frmBuildConfig_IsWithin( h, ghBCOK ) then bSawOK = true
        h = GetNextDlgTabItem( HWND_FRMBUILDCONFIG, h, false )
        if nStops > 40 then exit do          ' a cycle that never returns to hFirst
    loop until h = hFirst

    LAYASSERT( bSawList, "the builds list is in the tab order" )
    LAYASSERT( bSawDesc, "the Description field is in the tab order" )
    LAYASSERT( bSawOK,   "the OK button is in the tab order" )
    LAYASSERT( nStops <= 40, "the tab order is a closed cycle" )

    ' ---- 9. the right pane really is hidden when nothing is selected -------------------
    ' Exercised rather than reasoned about. The list is left on the row it started on.
    dim as long nWas = gBCCurrent
    frmBuildConfig_LoadDetail( -1 )
    LAYASSERT( IsWindowVisible( ghBCDescription ) = 0, "Description hides when nothing is selected" )
    LAYASSERT( IsWindowVisible( ghBCSwitches ) = 0, "the switch list hides when nothing is selected" )
    LAYASSERT( IsWindowEnabled( ghBCDelete ) = 0, "Delete is disabled when nothing is selected" )
    frmBuildConfig_LoadDetail( nWas )
    if nWas >= 0 then
        LAYASSERT( IsWindowVisible( ghBCDescription ) <> 0, "Description comes back with a selection" )
    end if

    ' ---- 10. the two pages are mutually exclusive --------------------------------------
    ' They share one parent and are shown/hidden as sets, so "both visible at once" is a
    ' reachable state rather than an impossible one.
    if nWas >= 0 then
        frmBuildConfig_ShowPage( IDP_FRMBUILDCONFIG_GENERAL )
        LAYASSERT( (IsWindowVisible( ghBCDescription ) <> 0) andalso (IsWindowVisible( ghBCSwitches ) = 0), _
                   "the General page hides the switch list" )
        frmBuildConfig_ShowPage( IDP_FRMBUILDCONFIG_SWITCHES )
        LAYASSERT( (IsWindowVisible( ghBCSwitches ) <> 0) andalso (IsWindowVisible( ghBCDescription ) = 0), _
                   "the switches page hides the General fields" )
        frmBuildConfig_ShowPage( IDP_FRMBUILDCONFIG_GENERAL )
        PsSelectBar_SetCurSel( ghBCSelectBar, 0 )
    end if

    ' ---- 11. the staging copy is genuinely separate storage ----------------------------
    ' The whole point of gBuildsWork: Cancel must be a no-op. Asserted rather than trusting
    ' the redim in LoadWork to have made a copy.
    if (ubound(gBuildsWork) >= 0) andalso (ubound(gConfig.Builds) >= 0) then
        dim as DWSTRING wszWas = gConfig.Builds(0).wszDescription
        gBuildsWork(0).wszDescription = wszWas & "##selftest##"
        LAYASSERT( gConfig.Builds(0).wszDescription = wszWas, _
                   "editing staging does not touch gConfig.Builds" )
        gBuildsWork(0).wszDescription = wszWas
        frmBuildConfig_RefreshRow( 0 )
    end if

    sMsg = "==== BUILDCONFIG (layout): " & str(nPass) & " passed, " & str(nFail) & " failed ====" : print sMsg

end sub

' ========================================================================================
' Window procedure
' ========================================================================================
private function frmBuildConfig_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

        case WM_CREATE
            ' Modal: keep the rest of the app out while this is up.
            EnableWindow( GetParent(hwnd), false )
            return 0

        case WM_ERASEBKGND
            ' The whole client is painted in WM_PAINT through one buffer.
            return 1

        case WM_PAINT
            return frmBuildConfig_OnPaint( hwnd )

        case WM_SIZE
            frmBuildConfig_PositionWindows( hwnd )
            return 0

        case WM_LBUTTONDOWN
            ' THE PREVIEW BLOCK IS PAINTED, NOT A CHILD WINDOW, so its click lands here. That
            ' is one fewer CWindow, WndProc and WM_CTLCOLOR path than a real control would
            ' cost, for a target that does exactly one thing.
            if (gBCPage = IDP_FRMBUILDCONFIG_GENERAL) andalso (gBCCurrent >= 0) then
                dim as POINT pt = type( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
                if PtInRect( @gBCPreviewRect, pt ) then
                    PsSelectBar_SetCurSel( ghBCSelectBar, 1 )
                    frmBuildConfig_ShowPage( IDP_FRMBUILDCONFIG_SWITCHES )
                    SetFocus( ghBCSwitches )
                    return 0
                end if
            end if

        case WM_TIMER
            ' Hot-tracking the preview block by POLL rather than WM_MOUSEMOVE: the cursor can
            ' leave it by moving onto a child control, where this window never sees another
            ' mouse message and the highlight would stick on. Same reason every hot-tracking
            ' control in this family carries one -- WM_MOUSELEAVE is not reliable enough to be
            ' the only mechanism.
            if wParam = IDT_BCPREVIEW then
                dim as boolean bHot = false
                if (gBCPage = IDP_FRMBUILDCONFIG_GENERAL) andalso (gBCCurrent >= 0) then
                    dim as POINT pt
                    GetCursorPos( @pt )
                    ScreenToClient( hwnd, @pt )
                    if PtInRect( @gBCPreviewRect, pt ) then
                        ' ...and only while this dialog is the one under the cursor, or the
                        ' block would light up behind a popup sitting over it.
                        dim as POINT ptScreen
                        GetCursorPos( @ptScreen )
                        dim as HWND hUnder = WindowFromPoint( ptScreen )
                        if frmBuildConfig_IsWithin( hUnder, hwnd ) then bHot = true
                    end if
                end if
                if bHot <> gBCPreviewHot then
                    gBCPreviewHot = bHot
                    InvalidateRect( hwnd, @gBCPreviewRect, false )
                end if
                return 0
            end if

        case WM_SETCURSOR
            if (gBCPage = IDP_FRMBUILDCONFIG_GENERAL) andalso gBCPreviewHot then
                SetCursor( LoadCursor( null, IDC_HAND ) )
                return true
            end if

        case WM_COMMAND
            ' PsButton routes through its click callback, not WM_COMMAND. What still arrives
            ' here is IDCANCEL, which IsDialogMessage synthesises for the Escape key.
            select case loword(wParam)
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case MSG_USER_SHOW_TOOLKEY
            frmBuildConfig_DoAssign()
            return 0

        case WM_CLOSE
            ' NOTHING IS SAVED HERE. The commit is on the OK path only, so reaching this by any
            ' route discards gBuildsWork() and leaves gConfig.Builds, settings.ini and the
            ' accelerator table exactly as they were found. The dialog this replaces had no
            ' working Cancel -- it edited gConfig.BuildsTemp and IDCANCEL simply abandoned it,
            ' which happened to work but left the array populated for the process lifetime.
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            KillTimer( hwnd, IDT_BCPREVIEW )
            HWND_FRMBUILDCONFIG      = 0
            HWND_FRMBUILDCONFIG_LIST = 0
            ghBCAdd = 0 : ghBCDelete = 0 : ghBCUp = 0 : ghBCDown = 0
            ghBCSelectBar = 0
            ghBCDescription = 0 : ghBCTogDefault = 0 : ghBCTarget = 0
            ghBCShortcut = 0 : ghBCTogExclude = 0 : ghBCEditSwitches = 0
            ghBCOptions = 0 : ghBCSwitches = 0
            ghBCOK = 0 : ghBCCancel = 0
            gBCCurrent = -1
            gBCPreviewHot = false
            ' The staging array outlives the dialog otherwise, holding live DWSTRINGs -- the
            ' exact leak gConfig.BuildsTemp had.
            erase gBuildsWork
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmBuildConfig_Show
' ========================================================================================
function frmBuildConfig_Show( byval hWndParent as HWND ) as LRESULT

    ' Seed the staging copy BEFORE any control exists -- everything below is built from
    ' gBuildsWork(), and gConfig.Builds() is not touched again unless OK commits.
    frmBuildConfig_LoadWork()
    ' Immediately after the seed, so every later read of gBuildsWork sees shortcuts that can
    ' actually fire. Staging only -- nothing is written back unless OK commits.
    frmBuildConfig_SanitizeWork()
    gBCCurrent = -1
    gBCLoading = false
    gBCPage    = IDP_FRMBUILDCONFIG_GENERAL
    gBCPreviewHot = false

    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMBUILDCONFIG = pWindow->Create( hWndParent, L(215,"Build Configurations"), _
        @frmBuildConfig_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMBUILDCONFIG_CLIENT_W, FRMBUILDCONFIG_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' There is no cached ghFont entry for a fixed-pitch face at UI size -- FIXEDFONT_26 is the
    ' About box's wordmark. Created here and destroyed after the pump.
    ghBCFontFixed = pWindow->CreateFont( GUIFIXEDFONT, 10, FW_NORMAL )

    ' ---- CREATION ORDER IS THE TAB ORDER ------------------------------------------------
    '
    ' These controls live on an ordinary CWindow, not a dialog template, so there is no
    ' ordinal to set: GetNextDlgTabItem walks the child Z-ORDER, and Z-order is creation
    ' order. The order built here is
    '
    '     list -> select bar -> [General page] -> [switches page] -> Add -> Delete
    '          -> Up -> Down -> OK -> Cancel
    '
    ' i.e. THE WORK FIRST, in reading order, and the chrome after.

    ' ---- the builds list ---------------------------------------------------------------
    HWND_FRMBUILDCONFIG_LIST = PsListTree_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_LIST )

    ' TWO columns. There was a Shortcut column here; it is gone deliberately -- the accelerator
    ' is edited on the General page and shown on its own button, and repeating it in the list
    ' bought a narrower Description for information the reader already has in front of them.
    '
    ' Column 0's width is a placeholder -- it is the FILL column and takes whatever column 1
    ' leaves. Column 1 is deliberately UNCAPTIONED: it marks a state, and a "Default" heading
    ' over a column that is empty on every row but one reads as a broken table.
    PsListTree_AddColumn( HWND_FRMBUILDCONFIG_LIST, L(216,"Description"), 100 )
    PsListTree_AddColumn( HWND_FRMBUILDCONFIG_LIST, "", pWindow->ScaleX(FRMBUILDCONFIG_COL_DEFAULT) )
    PsListTree_SetFillColumn( HWND_FRMBUILDCONFIG_LIST, 0 )
    PsListTree_ShowHeader( HWND_FRMBUILDCONFIG_LIST, true )

    PsListTree_SetPaintCallback( HWND_FRMBUILDCONFIG_LIST, @frmBuildConfig_ListPaintRow )
    ' REQUIRED, or the header does not render at all.
    PsListTree_SetHeaderPaintCallback( HWND_FRMBUILDCONFIG_LIST, @frmBuildConfig_ListPaintHeader )
    PsListTree_SetSelChangeCallback( HWND_FRMBUILDCONFIG_LIST, @frmBuildConfig_ListSelChange )
    PsListTree_SetTooltipCallback( HWND_FRMBUILDCONFIG_LIST, @frmBuildConfig_ListTooltip )
    PsListTree_SetHeaderTooltipCallback( HWND_FRMBUILDCONFIG_LIST, @frmBuildConfig_HeaderTooltip )
    PsListTree_SetFont( HWND_FRMBUILDCONFIG_LIST, ghFont(GUIFONT_11) )
    ' UNSCALED: PsListTree scales what it is handed, so a pre-scaled value is scaled twice.
    ' (PsTextBox_SetMargins and PsButton_SetPadding are the opposite rule in the same family.)
    PsListTree_SetRowHeight( HWND_FRMBUILDCONFIG_LIST, FRMBUILDCONFIG_ROW_H )

    ' ---- the two-page select bar --------------------------------------------------------
    ghBCSelectBar = PsSelectBar_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_SELECTBAR )
    PsSelectBar_SetFont( ghBCSelectBar, ghFont(GUIFONT_11) )
    ' NO LEFT PADDING. The bar's default pads every panel on both sides, which indents the
    ' first panel's text away from the left edge of the pane -- so "General" did not line up
    ' with the labels and fields directly beneath it. Zeroed on the bar, so the panels stay
    ' evenly spaced from each other; only the leading edge moves.
    PsSelectBar_SetPadding( ghBCSelectBar, 0, AfxScaleX(FRMBUILDCONFIG_SELECTBAR_PANELGAP) )
    PsSelectBar_AddPanel( ghBCSelectBar, L(354,"General"), IDP_FRMBUILDCONFIG_GENERAL )
    PsSelectBar_AddPanel( ghBCSelectBar, L(356,"Compiler switches"), IDP_FRMBUILDCONFIG_SWITCHES )
    PsSelectBar_SetSelChangeCallback( ghBCSelectBar, @frmBuildConfig_SelectBarChange )

    ' ---- General page -------------------------------------------------------------------
    ' PsTextBox documents its margins as RAW pixels and passes them verbatim to EM_SETMARGINS,
    ' so AfxScaleX is ours to apply here.
    ghBCDescription = PsTextBox_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TXTDESCRIPTION )
    PsTextBox_SetFont( ghBCDescription, ghFont(GUIFONT_11) )
    PsTextBox_SetMargins( ghBCDescription, AfxScaleX(8), AfxScaleX(6) )
    PsTextBox_SetChangeCallback( ghBCDescription, @frmBuildConfig_TextChanged )

    ghBCTogDefault = PsToggle_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TOGDEFAULT )
    PsToggle_SetCheckChangedCallback( ghBCTogDefault, @frmBuildConfig_ToggleChanged )

    ghBCTarget = PsComboBox_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_COMBOTARGET )
    PsComboBox_SetFont( ghBCTarget, ghFont(GUIFONT_11) )
    ' ORDER IS BUILDTARGET_32 / _64 and is read back by index. Do not re-order.
    PsComboBox_AddItem( ghBCTarget, L(387,"FBC 32-bit") )
    PsComboBox_AddItem( ghBCTarget, L(388,"FBC 64-bit") )
    PsComboBox_SetCurSel( ghBCTarget, BUILDTARGET_64 )
    PsComboBox_SetSelChangeCallback( ghBCTarget, @frmBuildConfig_TargetChanged )

    ghBCShortcut = PsButton_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CMDSHORTCUT )
    PsButton_SetFont( ghBCShortcut, ghFont(GUIFONT_11) )
    PsButton_SetText( ghBCShortcut, L(343,"(none)") )
    PsButton_SetTooltipText( ghBCShortcut, L(331,"Assign Shortcut") )
    PsButton_SetClickCallback( ghBCShortcut, @frmBuildConfig_ButtonClick )

    ghBCTogExclude = PsToggle_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TOGEXCLUDEPOPUP )
    PsToggle_SetCheckChangedCallback( ghBCTogExclude, @frmBuildConfig_ToggleChanged )

    ghBCEditSwitches = PsButton_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CMDEDITSWITCHES )
    PsButton_SetFont( ghBCEditSwitches, ghFont(GUIFONT_11) )
    PsButton_SetText( ghBCEditSwitches, L(14,"Edit") )
    PsButton_SetClickCallback( ghBCEditSwitches, @frmBuildConfig_ButtonClick )

    ' ---- Compiler switches page ---------------------------------------------------------
    ghBCOptions = PsTextBox_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_TXTOPTIONS )
    PsTextBox_SetMargins( ghBCOptions, AfxScaleX(8), AfxScaleX(6) )
    PsTextBox_SetChangeCallback( ghBCOptions, @frmBuildConfig_TextChanged )

    ghBCSwitches = PsListTree_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_LSTSWITCHES )
    PsListTree_SetPaintCallback( ghBCSwitches, @frmBuildConfig_SwitchPaintRow )
    PsListTree_SetMessageCallback( ghBCSwitches, @frmBuildConfig_SwitchListMessage )
    PsListTree_SetTooltipCallback( ghBCSwitches, @frmBuildConfig_ListTooltip )
    PsListTree_SetFont( ghBCSwitches, ghFont(GUIFONT_11) )
    PsListTree_SetRowHeight( ghBCSwitches, FRMBUILDCONFIG_SWROW_H )
    ' With CS_DBLCLKS a double-click delivers down/up/DBLCLK/up, and this list acts on the UP
    ' -- so a fast second click would toggle twice, net to nothing, and read as a control bug.
    PsListTree_PreventDoubleClick( ghBCSwitches, true )
    for i as long = lbound(gBuildOptions) to ubound(gBuildOptions)
        PsListTree_AddString( ghBCSwitches, gBuildOptions(i).wszText, i )
    next

    ' ---- the list's command strip -------------------------------------------------------
    ghBCAdd = PsButton_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CMDADD )
    PsButton_SetText( ghBCAdd, L(339,"Add") )
    PsButton_SetFont( ghBCAdd, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghBCAdd, @frmBuildConfig_ButtonClick )

    ghBCDelete = PsButton_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CMDDELETE )
    PsButton_SetText( ghBCDelete, L(219,"Delete") )
    PsButton_SetFont( ghBCDelete, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghBCDelete, @frmBuildConfig_ButtonClick )

    ' THE GLYPH IS THE CAPTION, NOT THE ICON, and that departs from this workspace's rule that
    ' the symbol font is a separate paint-time font. PsButton PINS its icon cells to the
    ' padding and has no icon alignment, so a glyph-only button built with SetGlyphLeft draws
    ' visibly off-centre in a 32px square. Putting the glyph in the caption slot is the only
    ' route to a centred one -- and the convention it bends exists to stop a WORD being drawn
    ' in Segoe Fluent, which cannot happen when the caption IS a glyph.
    ' The padding must come down with it. SetPadding stores RAW pixels.
    #macro MAKECHEVRON( _h_, _id_, _glyph_, _tip_ )
        _h_ = PsButton_Create( HWND_FRMBUILDCONFIG, _id_ )
        PsButton_SetText( _h_, _glyph_ )
        PsButton_SetFont( _h_, ghFont(SYMBOLFONT_10) )
        PsButton_SetTextAlign( _h_, BTN_ALIGN_CENTER )
        PsButton_SetPadding( _h_, AfxScaleX(4), AfxScaleY(6), AfxScaleX(4), AfxScaleY(6) )
        PsButton_SetTooltipText( _h_, _tip_ )
        PsButton_SetClickCallback( _h_, @frmBuildConfig_ButtonClick )
    #endmacro

    MAKECHEVRON( ghBCUp,   IDC_FRMBUILDCONFIG_CMDUP,   !"\uE70E", L(340,"Move up") )
    MAKECHEVRON( ghBCDown, IDC_FRMBUILDCONFIG_CMDDOWN, !"\uE70D", L(341,"Move down") )

    ' ---- footer --------------------------------------------------------------------------
    ghBCOK = PsButton_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CMDOK )
    PsButton_SetText( ghBCOK, L(0,"OK") )
    PsButton_SetFont( ghBCOK, ghFont(GUIFONT_11) )
    PsButton_SetDefault( ghBCOK, true )
    PsButton_SetClickCallback( ghBCOK, @frmBuildConfig_ButtonClick )

    ghBCCancel = PsButton_Create( HWND_FRMBUILDCONFIG, IDC_FRMBUILDCONFIG_CMDCANCEL )
    PsButton_SetText( ghBCCancel, L(1,"Cancel") )
    PsButton_SetFont( ghBCCancel, ghFont(GUIFONT_11) )
    PsButton_SetClickCallback( ghBCCancel, @frmBuildConfig_ButtonClick )

    ' Only the always-present chrome is shown here. The right pane's visibility belongs to
    ' ShowPage, which BuildList reaches through LoadDetail.
    ShowWindow( HWND_FRMBUILDCONFIG_LIST, SW_SHOW )
    ShowWindow( ghBCSelectBar, SW_SHOW )
    ShowWindow( ghBCAdd, SW_SHOW )
    ShowWindow( ghBCDelete, SW_SHOW )
    ShowWindow( ghBCUp, SW_SHOW )
    ShowWindow( ghBCDown, SW_SHOW )
    ShowWindow( ghBCOK, SW_SHOW )
    ShowWindow( ghBCCancel, SW_SHOW )

    frmBuildConfig_ApplyTheme( HWND_FRMBUILDCONFIG )
    frmBuildConfig_PositionWindows( HWND_FRMBUILDCONFIG )
    ' OPEN ON THE BUILD THAT IS ACTUALLY IN EFFECT, not on row 0. gBCCurrent is -1 at this
    ' point and BuildList clamps a negative index to 0, so the dialog always came up on the
    ' first configuration -- and since the detail pane follows the selection, that showed
    ' the wrong build's switches to anyone who opened this to check the current one.
    '
    ' getActiveBuildIndex is the same answer the compile path uses: the workspace's own
    ' gApp.ProjectBuild if it names a build that still exists, otherwise the one marked
    ' Default. It indexes gConfig.Builds, which gBuildsWork was copied from row for row in
    ' LoadWork above, so the index is valid here.
    frmBuildConfig_BuildList( frmBuildConfig_getActiveBuildIndex() )

    SetTimer( HWND_FRMBUILDCONFIG, IDT_BCPREVIEW, 100, 0 )

    ShowWindow( HWND_FRMBUILDCONFIG, SW_SHOW )
    UpdateWindow( HWND_FRMBUILDCONFIG )

    ' Focus the list: the usual reason for opening this dialog is to pick or edit a build that
    ' already exists. It also has to be set explicitly for a second reason -- IsDialogMessage
    ' ignores Tab until a child already holds the focus, and an ordinary CWindow has no dialog
    ' manager to arrange that.
    SetFocus( HWND_FRMBUILDCONFIG_LIST )

    ' AFTER the SetFocus above, deliberately: one of its assertions walks the tab order, and
    ' running it any earlier would sample it before anything had been focused at all.
    frmBuildConfig_RunLayoutSelfTest()

    if environ("TIKO_BUILDCONFIG_AUTOASSIGN") = "1" then
        PostMessage( HWND_FRMBUILDCONFIG, MSG_USER_SHOW_TOOLKEY, 0, 0 )
    end if

    ' ---- message loop --------------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents: that has no *_FilterMessage hooks, and this dialog
    ' owes two of them.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. Two PsTextBoxes, each owning a PsPopupMenu context menu; without this it
        ' has no keyboard navigation and never closes on an outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' MANDATORY, and it takes the CONTROL handle rather than working off a global -- so a
        ' second combo added to this dialog needs its own line here.
        if PsComboBox_FilterMessage( ghBCTarget, @uMsg ) then continue while

        ' SPACE TOGGLES THE FOCUSED SWITCH ROW.
        '   AFTER the two filters above, so an open context menu or dropdown gets its own
        '   Space first.
        '   BEFORE IsDialogMessage, or Space presses the default OK button instead.
        ' Gated by CONTAINMENT -- PsListTree's focus lives on its row surface, a child, so
        ' GetFocus() = ghBCSwitches is never true. And it has to be done in the pump at all
        ' because the control consumes WM_KEYDOWN itself and never offers it to the message
        ' callback; this is the only door.
        scope
            dim as boolean bSpace = ((uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_SPACE))
            if bSpace andalso frmBuildConfig_IsWithin( GetFocus(), ghBCSwitches ) then
                frmBuildConfig_ToggleSwitch( PsListTree_GetCurSel( ghBCSwitches ) )
                continue while
            end if
        end scope

        ' ESCAPE cancels -- explicitly, and AFTER the two popup filters so an open context menu
        ' or dropdown closes on Escape first rather than the whole dialog going with it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMBUILDCONFIG, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMBUILDCONFIG, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if ghBCFontFixed then DeleteObject( ghBCFontFixed )
    ghBCFontFixed = 0

    if pWindow then delete(pWindow)

    function = 0

end function
