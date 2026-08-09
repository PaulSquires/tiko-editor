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

'  modKeyBindings.inc  --  implementation of the keyboard shortcut model. See the header
'  of modKeyBindings.bi for what lives here and why it is not in frmKeyboard.inc.

#include once "modKeyBindings.bi"


' ========================================================================================
' THE MODEL MOVED to app/modKeyBindings.bi / .inc (2026-08-09).
'
' KEYBINDINGS_TYPE, gKeys, AddKeyBinding, AddKeyBinding_User, CreateDefaultKeyBindings
' and SaveKeyBindings are all down there now. This file keeps what has to ask WINDOWS:
' the name -> virtual key vocabulary, whose punctuation arms go through
' frmKeyboard_OEMtoVirtKey and VkKeyScanEx against the LIVE keyboard layout.
'
' That is the line the split follows -- data from layout lookup -- and it is a second cut
' through material this file had already separated once, from the dialog.
'
' What needed it: phase 7c's shell binary. getMenuAccelText is app-layer and walks gKeys to
' put "Ctrl+S" beside a menu row, so with the array up here every menu in the new shell
' rendered a blank accelerator column.
' ========================================================================================



' ========================================================================================
' Convert a virtual key string (Ctrl, Alt, Shift) to virtual key value
' ========================================================================================
function frmKeyboard_VirtKeyToValue( byval wszString as DWSTRING ) as long
    ' .Utf8 on the selector -- `select case <DWSTRING>` binds today only because
    ' AfxNova's DWSTRING extends WSTRING. Every `case` label below is ASCII, so
    ' the ANSI-vs-UTF-8 difference between the two conversions cannot change a
    ' match. See clsConfig.inc for the long form.
    select case PsUCase(wszString).Utf8
        case "CTRL":  return FCONTROL
        case "ALT":   return FALT
        case "SHIFT": return FSHIFT
    end select
    function = 0
end function


' ========================================================================================
' Converts a VK_OEM key to propery virtual key because of different keyboard layouts
' ========================================================================================
function frmKeyboard_OEMtoVirtKey( byval ch as ubyte ) as long
    ' declare function VkKeyScanExW(byval ch as WCHAR, byval dwhkl as HKL) as SHORT
    dim as HKL layout = GetKeyboardLayout(0)  '// Current layout
    dim as short result = VkKeyScanEx(ch, layout)
    dim as ubyte vkCode = lobyte(result)
    dim as ubyte shiftState = hibyte(result)
    return vkCode
end function


' ========================================================================================
' Convert an accelerator key string (F1, TAB, "A", etc..) to virtual key value
'
' THIS FUNCTION IS THE AUTHORITY ON THE VOCABULARY. gKeyNames() lists the names a user may
' PICK; this decides what any name MEANS, and KeyBindings_VKToName inverts it by asking it
' rather than by carrying a second table. Some names here are deliberately NOT offered in
' gKeyNames() -- the bare "+" for instance, which would be unparseable in a "Ctrl++"
' shortcut string, and is kept only so a hand-edited keybindings.ini still loads.
' ========================================================================================
function frmKeyboard_AccelKeyToValue( byval wszString as DWSTRING ) as long
    ' .Utf8 on the selector -- `select case <DWSTRING>` binds today only because
    ' AfxNova's DWSTRING extends WSTRING. Every `case` label below is ASCII, so
    ' the ANSI-vs-UTF-8 difference between the two conversions cannot change a
    ' match. See clsConfig.inc for the long form.
    select case PsUCase(wszString).Utf8
        case "BACKSPACE": return VK_BACK
        case "TAB": return VK_TAB
        case "ENTER": return VK_RETURN
        case "ESCAPE": return VK_ESCAPE
        case "SPACEBAR": return VK_SPACE
        case "PGUP": return VK_PRIOR
        case "PGDN": return VK_NEXT
        case "END": return VK_END
        case "HOME": return VK_HOME
        case "LEFT": return VK_LEFT
        case "UP": return VK_UP
        case "RIGHT": return VK_RIGHT
        case "DOWN": return VK_DOWN
        case "INS": return VK_INSERT
        case "DEL": return VK_DELETE
        case "NUMPAD0": return VK_NUMPAD0
        case "NUMPAD1": return VK_NUMPAD1
        case "NUMPAD2": return VK_NUMPAD2
        case "NUMPAD3": return VK_NUMPAD3
        case "NUMPAD4": return VK_NUMPAD4
        case "NUMPAD5": return VK_NUMPAD5
        case "NUMPAD6": return VK_NUMPAD6
        case "NUMPAD7": return VK_NUMPAD7
        case "NUMPAD8": return VK_NUMPAD8
        case "NUMPAD9": return VK_NUMPAD9
        case "NUMPAD*": return VK_MULTIPLY
        case "NUMPAD+": return VK_ADD
        case "NUMPAD-": return VK_SUBTRACT
        case "NUMPAD.": return VK_DECIMAL
        case "NUMPAD/": return VK_DIVIDE
        case "F1": return VK_F1
        case "F2": return VK_F2
        case "F3": return VK_F3
        case "F4": return VK_F4
        case "F5": return VK_F5
        case "F6": return VK_F6
        case "F7": return VK_F7
        case "F8": return VK_F8
        case "F9": return VK_F9
        case "F10": return VK_F10
        case "F11": return VK_F11
        case "F12": return VK_F12
        case "+", "PLUS": return VK_OEM_PLUS
        case ",", "COMMA": return VK_OEM_COMMA
        case "-", "MINUS": return VK_OEM_MINUS
        case ".", "PERIOD": return VK_OEM_PERIOD
        case ";", "SEMICOLON"       ' VK_OEM_1
            return frmKeyboard_OEMtoVirtKey(asc(";"))
        case ":", "COLON"           ' VK_OEM_1
            return frmKeyboard_OEMtoVirtKey(asc(":"))
        case "/", "FORWARDSLASH"    ' VK_OEM_2
            return frmKeyboard_OEMtoVirtKey(asc("/"))
        case "?", "QUESTIONMARK"    ' VK_OEM_2
            return frmKeyboard_OEMtoVirtKey(asc("?"))
        case "`", "BACKTICK"        ' VK_OEM_3
            return frmKeyboard_OEMtoVirtKey(asc("`"))
        case "~", "TILDE"           ' VK_OEM_3
            return frmKeyboard_OEMtoVirtKey(asc("~"))
        case "[", "OPENSQUAREBRACKET" ' VK_OEM_4
            return frmKeyboard_OEMtoVirtKey(asc("["))
        case "{", "OPENCURLYBRACE"    ' VK_OEM_4
            return frmKeyboard_OEMtoVirtKey(asc("{"))
        case "\", "BACKSLASH"         ' VK_OEM_5
            return frmKeyboard_OEMtoVirtKey(asc("\"))
        case "|", "PIPE"              ' VK_OEM_5
            return frmKeyboard_OEMtoVirtKey(asc("|"))
        case "]", "CLOSESQUAREBRACKET"  ' VK_OEM_6
            return frmKeyboard_OEMtoVirtKey(asc("]"))
        case "}", "CLOSECURLYBRACE"     ' VK_OEM_6
            return frmKeyboard_OEMtoVirtKey(asc("}"))
        case "'", "SINGLEQUOTE"         ' VK_OEM_7
            return frmKeyboard_OEMtoVirtKey(asc("'"))
        case chr(34), "DOUBLEQUOTE"     ' VK_OEM_7
            return frmKeyboard_OEMtoVirtKey(34)
        ' asc(.Utf8), and the ORIGINAL rather than the uppercased selector -- that is
        ' the behaviour being preserved, not chosen: "a" matches the "A" to "Z" arm
        ' after PsUCase and then returns asc("a") = 97, not 65. Both reachable arms
        ' are ASCII, where the first UTF-8 byte and the first wide character agree.
        '
        ' SINGLE CHARACTERS ONLY, AND THE LENGTH TEST IS THE WHOLE POINT OF THESE ARMS.
        '
        ' `case "A" to "Z"` is a LEXICOGRAPHIC range over the WHOLE string -- the trap
        ' written up in Learnings.md -- so without the test below "PAGEUP" is in range
        ' (it sorts after "A" and before "Z") and this returned asc("P") = 80 = VK_P.
        ' Any unrecognised name beginning A-Y, or 0-8, silently became its own initial.
        '
        ' THAT WAS LIVE, not just a self-test curiosity. KeyBindings_ApplyAccelerators
        ' feeds this the last "+"-separated token straight out of keybindings.ini, so a
        ' file carrying the historical spelling "PageUp" -- which the combobox this
        ' vocabulary replaced really did offer -- installed a working accelerator on
        ' plain P. Returning 0 makes the caller skip it, which is what it already does
        ' for every other name this function does not know.
        '
        ' The vocabulary itself is unchanged and still not decided here: gKeyNames only
        ' ever supplies wchr(48..57) and wchr(65..90) to these two arms, so nothing that
        ' used to resolve correctly stops. Whether "PageUp" SHOULD be an accepted alias
        ' for "PgUp" is the separate question, and it stays open.
        case "0" to "9", "A" to "Z"
            if PsLen(wszString) <> 1 then return 0
            return asc(wszString.Utf8)
    end select
    function = 0
end function


' ========================================================================================
' Build the accelerator key vocabulary -- ONCE, lazily.
'
' The order below is the order every pick-list shows AND the search order
' KeyBindings_VKToName uses, so it is also what makes one spelling canonical when several
' map to the same virtual key.
'
' EVERY NAME HERE MUST BE ONE frmKeyboard_AccelKeyToValue UNDERSTANDS. That is not a style
' rule: a name it does not recognise yields virtual key 0, AddAccelerator is skipped, and
' the binding saves and displays perfectly while never firing. "PageUp"/"PageDn" sat in
' the old hardcoded combobox list in exactly that state; they are "PgUp"/"PgDn" here.
' TIKO_KEYBOARD_SELFTEST asserts the round trip for the whole table.
' ========================================================================================
sub KeyBindings_EnsureKeyNames()

    if ubound(gKeyNames) >= 0 then exit sub

    ' Sized generously, then trimmed to what was actually added.
    redim gKeyNames(255) as KEYNAME_TYPE
    dim as long n = -1

    #macro ADDKEYNAME( _name_ )
        n += 1
        gKeyNames(n).wszName = _name_
    #endmacro

    ADDKEYNAME( "Backspace" )
    ADDKEYNAME( "Tab" )
    ADDKEYNAME( "Enter" )
    ADDKEYNAME( "Escape" )
    ADDKEYNAME( "Spacebar" )
    ADDKEYNAME( "PgUp" )
    ADDKEYNAME( "PgDn" )
    ADDKEYNAME( "End" )
    ADDKEYNAME( "Home" )
    ADDKEYNAME( "Left" )
    ADDKEYNAME( "Right" )
    ADDKEYNAME( "Up" )
    ADDKEYNAME( "Down" )
    ADDKEYNAME( "DEL" )
    ADDKEYNAME( "INS" )

    for i as long = 48 to 57    ' 0 to 9
        ADDKEYNAME( wchr(i) )
    next
    for i as long = 65 to 90    ' A to Z
        ADDKEYNAME( wchr(i) )
    next

    ADDKEYNAME( "Numpad0" )
    ADDKEYNAME( "Numpad1" )
    ADDKEYNAME( "Numpad2" )
    ADDKEYNAME( "Numpad3" )
    ADDKEYNAME( "Numpad4" )
    ADDKEYNAME( "Numpad5" )
    ADDKEYNAME( "Numpad6" )
    ADDKEYNAME( "Numpad7" )
    ADDKEYNAME( "Numpad8" )
    ADDKEYNAME( "Numpad9" )
    ADDKEYNAME( "Numpad*" )
    ADDKEYNAME( "Numpad+" )
    ADDKEYNAME( "Numpad-" )
    ADDKEYNAME( "Numpad." )
    ADDKEYNAME( "Numpad/" )

    ADDKEYNAME( "F1" )
    ADDKEYNAME( "F2" )
    ADDKEYNAME( "F3" )
    ADDKEYNAME( "F4" )
    ADDKEYNAME( "F5" )
    ADDKEYNAME( "F6" )
    ADDKEYNAME( "F7" )
    ADDKEYNAME( "F8" )
    ADDKEYNAME( "F9" )
    ADDKEYNAME( "F10" )
    ADDKEYNAME( "F11" )
    ADDKEYNAME( "F12" )

    ADDKEYNAME( "Tilde" )
    ADDKEYNAME( "Plus" )
    ADDKEYNAME( "Comma" )
    ADDKEYNAME( "Minus" )
    ADDKEYNAME( "Period" )
    ADDKEYNAME( "SingleQuote" )
    ADDKEYNAME( "SemiColon" )
    ADDKEYNAME( "/" )
    ADDKEYNAME( "\" )
    ADDKEYNAME( "[" )
    ADDKEYNAME( "]" )

    redim preserve gKeyNames(n) as KEYNAME_TYPE

end sub


' ========================================================================================
' Virtual key -> its canonical name, or "" when this key has no name in the vocabulary.
'
' DERIVED, NOT WRITTEN. It asks frmKeyboard_AccelKeyToValue rather than carrying a reverse
' table, which is what makes the round trip correct BY CONSTRUCTION: a name this returns is
' by definition one that maps back to the same key. It also inherits the layout-dependent
' VkKeyScanEx handling of the OEM keys for free -- same function, same layout, same session.
'
' Linear over ~110 entries and called once per keystroke; the cost is not worth a cache.
' ========================================================================================
function KeyBindings_VKToName( byval vk as long ) as DWSTRING
    if vk = 0 then return ""
    KeyBindings_EnsureKeyNames()
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        if frmKeyboard_AccelKeyToValue( gKeyNames(i).wszName ) = vk then
            return gKeyNames(i).wszName
        end if
    next
    return ""
end function


' ========================================================================================
' A BARE PICK-LIST NAME -> virtual key, for the hosts that store a key NAME and nothing else.
'
' frmBuildConfig and frmUserTools do not persist a binding string the way gKeys() does. They
' keep the modifiers in three booleans (IsCtrl/IsAlt/IsShift) and the key itself as whatever
' text the pick list put in front of the user, so there is never a "Ctrl+" prefix to
' parse. This turns that stored text back into an accelerator, and it is deliberately NOT
' frmKeyboard_AccelKeyToValue.
'
' TWO REASONS IT HAS TO BE ITS OWN FUNCTION, and note that both produce a WRONG accelerator
' rather than a missing one -- so neither has a visible symptom:
'
'   (1) "None" IS A PICK-LIST ENTRY. frmAssignKey and frmKeyboardEdit prepend it, and both
'       write it straight back into gConfig as the stored key meaning "no shortcut".
'
'   (2) A stale or hand-edited name -- a config carrying "PageUp" from an older tiko --
'       must not become an accelerator on some other key.
'
' BOTH OF THOSE USED TO BE HANDLED HERE ALONE, because AccelKeyToValue's `case "A" to "Z"`
' is a LEXICOGRAPHIC RANGE over the whole string rather than a test for a single letter:
' "NONE" and "PAGEUP" both sort between "A" and "Z", so asc() took the first character and
' they became plain N and plain P. AccelKeyToValue now requires a single character in that
' arm, so it returns 0 for both on its own and this function is no longer the only guard.
'
' IT IS STILL THE RIGHT FUNCTION TO CALL, and still not a wrapper. MEMBERSHIP IN
' gKeyNames() is the validity test here, never "the return is non-zero" -- AccelKeyToValue
' deliberately understands names the pick list does not offer (the bare "+", for one), and
' a host that stores what the pick list showed must accept only what the pick list showed.
' ========================================================================================
' ========================================================================================
' Which live editor command owns this keystroke? See the declaration for the contract.
' ========================================================================================
function KeyBindings_FindCommandForKeys( byval wszKeys as DWSTRING ) as DWSTRING

    if PsLen(wszKeys) = 0 then return ""

    for i as long = lbound(gKeys) to ubound(gKeys)

        ' The EFFECTIVE binding: the user's override if there is one, otherwise the shipped
        ' default -- and nothing at all when that default has been suppressed. Written out
        ' here rather than shared with frmKeyboard_EffectiveKeys, which is private to the
        ' dialog and reads gKeysWork().
        dim as DWSTRING wszEff
        if PsLen(gKeys(i).wszUserKeys) then
            wszEff = gKeys(i).wszUserKeys
        elseif gKeys(i).bDefaultDisabled = false then
            wszEff = gKeys(i).wszDefaultKeys
        end if

        if PsLen(wszEff) = 0 then continue for
        if PsUCase(wszEff) = PsUCase(wszKeys) then return gKeys(i).wszDescription

    next

    return ""
end function


function KeyBindings_PickListKeyToValue( byval wszString as DWSTRING ) as long
    dim as DWSTRING wszName = PsTrim(wszString)
    if PsLen(wszName) = 0 then return 0

    ' The affordance, not a key. TEST 7 asserts the vocabulary holds nothing by this name,
    ' so this cannot shadow a real entry.
    if PsUCase(wszName) = "NONE" then return 0

    KeyBindings_EnsureKeyNames()

    ' Stored text is whatever the combobox held; the callers used to normalise with
    ' ucase(trim(...)), and their existing config files may carry either spelling.
    dim as DWSTRING wszUpper = PsUCase(wszName)

    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        if PsUCase(gKeyNames(i).wszName) = wszUpper then
            return frmKeyboard_AccelKeyToValue( gKeyNames(i).wszName )
        end if
    next

    return 0
end function


' ========================================================================================
' THE VOCABULARY SELF-TEST                                   (TIKO_KEYBOARD_SELFTEST=1)
'
' Needs no window, no theme and no message pump, so it runs from frmMain at startup rather
' than when the dialog opens -- which means it covers the accelerator table the editor is
' actually running on, not just the one the dialog would build.
'
' WHAT IT IS FOR. A shortcut name that frmKeyboard_AccelKeyToValue does not understand
' yields virtual key 0, AddAccelerator is skipped, and the binding then SAVES, DISPLAYS and
' SURVIVES A RELOAD perfectly while never once firing. There is no build warning and no
' visible symptom. That is the failure this file exists to make impossible, and it was live
' in the shipped code: the old hardcoded pick-list offered "PageUp" and "PageDn" against an
' AccelKeyToValue that knows only "PGUP" and "PGDN".
'
' ON WHAT IS DELIBERATELY *NOT* ASSERTED HERE. The obvious test --
'     KeyBindings_VKToName( AccelKeyToValue(name) )  maps back to the same VK
' -- is VACUOUS. VKToName is implemented by scanning this same table for the first entry
' whose AccelKeyToValue equals the VK, so that assertion reduces to "the entry the scan
' found is the entry the scan found". It can never fail, under any breakage. It was written,
' recognised as content-free, and removed rather than left to sit green. What carries the
' actual content is TEST 1 (every offered name resolves at all) and TEST 4 (the canonical
' spelling is pinned by name, independently of the search that produced it).
' ========================================================================================
sub KeyBindings_RunSelfTest()

    if environ("TIKO_KEYBOARD_SELFTEST") <> "1" then exit sub

    KeyBindings_EnsureKeyNames()

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro KBASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? ""
    ? "--- TIKO_KEYBOARD_SELFTEST (key vocabulary) ---"

    ' ---- TEST 1: no two names in the vocabulary mean the SAME virtual key. ----
    '
    ' READ THIS BEFORE WEAKENING IT BACK. The obvious version of this test -- "every offered
    ' name resolves to a non-zero virtual key" -- was written first and is ALMOST ENTIRELY
    ' VACUOUS, which an A/B pass proved: it stayed green on the very bug it was written for.
    '
    ' The reason is that frmKeyboard_AccelKeyToValue ends with
    '       case "A" to "Z": return asc(wszString)
    ' and in FreeBASIC that is a LEXICOGRAPHIC RANGE over the whole string, not a test for a
    ' single letter. "PAGEUP" sorts between "A" and "Z", so it matches, and asc() then takes
    ' the FIRST CHARACTER: "PageUp" resolves to 80 = VK_P. Any unrecognised name beginning
    ' with a letter silently resolves to its own initial. So the old hardcoded pick-list's
    ' "PageUp"/"PageDn" were not dead entries -- picking either bound the shortcut to plain
    ' P or plain P, colliding with Ctrl+P (Search Symbol). Worse than doing nothing, and
    ' invisible to a non-zero check.
    '
    ' Collision is the property that actually holds only when the table is right: two
    ' pick-list entries that produce one accelerator mean the list is lying about what it
    ' offers. It compares distinct entries against each other, so nothing can satisfy it by
    ' construction.
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        dim as long vkI = frmKeyboard_AccelKeyToValue( gKeyNames(i).wszName )
        ' .Utf8 -- these exist ONLY to build KBASSERT failure text. The implicit
        ' conversion they used goes through the ANSI codepage and .Utf8 does not, so
        ' this IS an encoding change; here it can only affect how a failure message
        ' prints, and every value is a key name or a message id. That is what makes
        ' these safe to convert ahead of the swap when a file path would not be.
        dim as string sName = gKeyNames(i).wszName.Utf8
        KBASSERT( vkI <> 0, "vocabulary name """ & sName & """ resolves to virtual key 0" )

        for j as long = i + 1 to ubound(gKeyNames)
            dim as string sOther = gKeyNames(j).wszName.Utf8
            KBASSERT( vkI <> frmKeyboard_AccelKeyToValue( gKeyNames(j).wszName ), _
                      "vocabulary names """ & sName & """ and """ & sOther & _
                      """ both resolve to virtual key " & str(vkI) )
        next
    next

    ' ---- TEST 2: no duplicate names in the vocabulary. ----
    ' A duplicate would show twice in every pick-list and make the canonical-name search
    ' order ambiguous.
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        for j as long = i + 1 to ubound(gKeyNames)
            dim as string sName = gKeyNames(i).wszName.Utf8
            KBASSERT( PsUCase(gKeyNames(i).wszName) <> PsUCase(gKeyNames(j).wszName), _
                      "duplicate vocabulary name """ & sName & """" )
        next
    next

    ' ---- TEST 3: a key with no name is reported as having no name. ----
    ' This is what makes the capture field able to REFUSE a keystroke rather than store
    ' something unusable. PrintScreen, F13, the Windows key and CapsLock are all real keys a
    ' user can press and none is expressible in the persistence format.
    KBASSERT( len( KeyBindings_VKToName( 0 ) ) = 0,        "VKToName(0) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h2C ) ) = 0,     "VKToName(VK_SNAPSHOT) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h7C ) ) = 0,     "VKToName(VK_F13) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h5B ) ) = 0,     "VKToName(VK_LWIN) should be empty" )
    KBASSERT( len( KeyBindings_VKToName( &h14 ) ) = 0,     "VKToName(VK_CAPITAL) should be empty" )

    ' ---- TEST 4: the CANONICAL spelling, pinned by name. ----
    ' Several names map to one key ("+" and "Plus" both give VK_OEM_PLUS), and which one
    ' comes back is decided by the ORDER of gKeyNames(). Pinning the expected string here is
    ' what makes that order a decision rather than an accident -- reorder the table, or add
    ' the bare "+" to it, and this goes red. Unlike the round-trip form, these compare
    ' against a literal the search cannot supply, so they are not self-satisfying.
    KBASSERT( KeyBindings_VKToName( VK_PRIOR )    = "PgUp",  "VK_PRIOR should spell as PgUp" )
    KBASSERT( KeyBindings_VKToName( VK_NEXT )     = "PgDn",  "VK_NEXT should spell as PgDn" )
    KBASSERT( KeyBindings_VKToName( VK_TAB )      = "Tab",   "VK_TAB should spell as Tab" )
    KBASSERT( KeyBindings_VKToName( VK_RETURN )   = "Enter", "VK_RETURN should spell as Enter" )
    KBASSERT( KeyBindings_VKToName( VK_DELETE )   = "DEL",   "VK_DELETE should spell as DEL" )
    KBASSERT( KeyBindings_VKToName( VK_OEM_PLUS ) = "Plus",  "VK_OEM_PLUS should spell as Plus, not +" )
    KBASSERT( KeyBindings_VKToName( VK_ADD )      = "Numpad+", "VK_ADD should spell as Numpad+" )
    KBASSERT( KeyBindings_VKToName( asc("A") )    = "A",     "VK_A should spell as A" )
    KBASSERT( KeyBindings_VKToName( asc("7") )    = "7",     "VK_7 should spell as 7" )
    KBASSERT( KeyBindings_VKToName( VK_F12 )      = "F12",   "VK_F12 should spell as F12" )

    ' ---- TEST 5: every SHIPPED DEFAULT binding names its key CANONICALLY. ----
    '
    ' Not "resolves to non-zero" -- see TEST 1 for why that is nearly worthless here. The
    ' check is that the default's final segment round-trips to ITSELF: resolve it to a
    ' virtual key, ask what that key is properly called, and require the two spellings to
    ' agree. A typo'd "Ctrl+PageUp" resolves to VK_P, whose canonical name is "P", and "P"
    ' <> "PAGEUP" -- so the typo is caught, where a non-zero test waves it through.
    '
    ' This is NOT the vacuous round trip described in the header. That one compares a name
    ' against whatever the search returned; this compares the search's answer against a
    ' string the search never saw -- the one an author typed into the defaults table.
    for i as long = lbound(gKeys) to ubound(gKeys)
        if PsLen(gKeys(i).wszDefaultKeys) = 0 then continue for

        dim as long nCount = PsStrParseCount( gKeys(i).wszDefaultKeys, "+" )
        dim as DWSTRING wszFinal = PsStrParse( gKeys(i).wszDefaultKeys, nCount, "+" )
        dim as DWSTRING wszCanon = KeyBindings_VKToName( frmKeyboard_AccelKeyToValue( wszFinal ) )

        dim as string sMsg   = gKeys(i).wszMsgString.Utf8
        dim as string sKeys  = gKeys(i).wszDefaultKeys.Utf8
        dim as string sCanon = wszCanon.Utf8
        KBASSERT( (PsLen(wszCanon) > 0) andalso (PsUCase(wszCanon) = PsUCase(wszFinal)), _
                  sMsg & " default """ & sKeys & """ final key is not canonical (got """ & _
                  sCanon & """)" )

        ' Every non-final segment must be a modifier. "Ctrl+Shft+D" would otherwise build an
        ' accelerator for plain D and no one would notice until it fired at the wrong time.
        for ii as long = 1 to nCount - 1
            dim as DWSTRING wszMod = PsStrParse( gKeys(i).wszDefaultKeys, ii, "+" )
            dim as string sMod = wszMod.Utf8
            KBASSERT( frmKeyboard_VirtKeyToValue( wszMod ) <> 0, _
                      sMsg & " default """ & sKeys & """ has an unrecognised modifier """ & sMod & """" )
        next
    next

    ' ---- TEST 6: the defaults table is internally consistent. ----
    ' Duplicate IDM_ names would make keybindings.ini ambiguous on load -- AddKeyBinding_User
    ' stops at the first match, so the second entry could never be given a user key.
    for i as long = lbound(gKeys) to ubound(gKeys)
        for j as long = i + 1 to ubound(gKeys)
            dim as string sMsg = gKeys(i).wszMsgString.Utf8
            KBASSERT( gKeys(i).wszMsgString <> gKeys(j).wszMsgString, _
                      "duplicate binding message string """ & sMsg & """" )
        next
    next

    ' ---- TEST 7: the pick-list as frmBuildConfig and frmUserTools actually resolve it. ----
    '
    ' Those two dialogs store a BARE key name, not a binding string, so their accelerator
    ' build calls KeyBindings_PickListKeyToValue. Each of them used to carry its own private
    ' 48-arm select case (F1-F12, 0-9, A-Z) against a pick-list offering all of gKeyNames():
    ' picking any of the other names left nKey = 0, the ACCEL entry was skipped, and the
    ' shortcut then SAVED, DISPLAYED and SURVIVED A RELOAD while never once firing. Same
    ' failure TEST 1 exists for, in a second and third copy of the vocabulary.
    '
    ' THIS IS NOT A DUPLICATE OF TEST 1. That one goes through AccelKeyToValue; this goes
    ' through the function the two dialogs call, which additionally has to reject the "None"
    ' affordance and refuse names outside the vocabulary. Replace its body with a plain
    ' AccelKeyToValue call -- the obvious "simplification" -- and the "None" and "PageUp"
    ' assertions below go red while every assertion in TEST 1 stays green.
    redim as long vkPick( lbound(gKeyNames) to ubound(gKeyNames) )

    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        dim as string sName = gKeyNames(i).wszName.Utf8
        vkPick(i) = KeyBindings_PickListKeyToValue( gKeyNames(i).wszName )

        KBASSERT( vkPick(i) <> 0, _
                  "pick-list name """ & sName & """ resolves to virtual key 0" )

        ' "None" is prepended by the two key-picking dialogs as an affordance meaning "no key at
        ' all". If it were ever also a vocabulary entry, PickListKeyToValue's early return
        ' would shadow it and that key would become unbindable.
        KBASSERT( PsUCase(gKeyNames(i).wszName) <> "NONE", _
                  "vocabulary must not contain a key named ""None"" -- it is the affordance" )
    next

    ' Distinct. Two pick-list entries producing one accelerator would mean the list is lying
    ' about what it offers, and the second one silently steals the first one's key.
    for i as long = lbound(gKeyNames) to ubound(gKeyNames)
        for j as long = i + 1 to ubound(gKeyNames)
            dim as string sName  = gKeyNames(i).wszName.Utf8
            dim as string sOther = gKeyNames(j).wszName.Utf8
            KBASSERT( vkPick(i) <> vkPick(j), _
                      "pick-list names """ & sName & """ and """ & sOther & _
                      """ both resolve to virtual key " & str(vkPick(i)) )
        next
    next

    ' The affordance and the empty cases all mean NO KEY. gConfig stores the literal "None"
    ' -- both dialogs write it into the staging copy the moment a row with no shortcut is
    ' selected -- so this is the ordinary case, not an edge one.
    KBASSERT( KeyBindings_PickListKeyToValue( "None" ) = 0, "pick-list ""None"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "NONE" ) = 0, "pick-list ""NONE"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "" ) = 0,     "pick-list """" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "   " ) = 0,  "pick-list all-blank must resolve to 0" )

    ' ---- THE LEXICOGRAPHIC RANGE, ASSERTED WHERE IT ACTUALLY BIT --------------------
    ' AccelKeyToValue's `case "A" to "Z"` sorts the WHOLE string, so every name below used
    ' to come back as its own first character. That was not confined to the pick-list
    ' hosts: KeyBindings_ApplyAccelerators hands this function the last "+"-separated token
    ' straight out of keybindings.ini, so a file carrying the historical spelling "PageUp"
    ' -- the combobox this vocabulary replaced really did offer it -- installed a WORKING
    ' accelerator on plain P, colliding with Ctrl+P. Asserted against AccelKeyToValue
    ' DIRECTLY, because routing it through the vocabulary tests the guard, not the defect.
    KBASSERT( frmKeyboard_AccelKeyToValue( "PageUp" ) = 0, "stale PageUp must not become VK_P" )
    KBASSERT( frmKeyboard_AccelKeyToValue( "PageDn" ) = 0, "stale PageDn must not become VK_P" )
    KBASSERT( frmKeyboard_AccelKeyToValue( "None" )   = 0, "None must not become VK_N" )
    KBASSERT( frmKeyboard_AccelKeyToValue( "Insert" ) = 0, "Insert must not become VK_I" )
    ' The "0" to "9" arm has the identical trap and is easy to fix only one of.
    KBASSERT( frmKeyboard_AccelKeyToValue( "0Bogus" ) = 0, "a digit-initial name must not become VK_0" )
    ' ...while every SINGLE character the vocabulary really offers still resolves. That is
    ' the half a length test could break, so it is pinned across the whole of both ranges.
    for i as long = 65 to 90        ' A-Z
        KBASSERT( frmKeyboard_AccelKeyToValue( wchr(i) ) = i, _
                  "letter " & chr(i) & " no longer resolves to its own virtual key" )
    next
    for i as long = 48 to 57        ' 0-9
        KBASSERT( frmKeyboard_AccelKeyToValue( wchr(i) ) = i, _
                  "digit " & chr(i) & " no longer resolves to its own virtual key" )
    next

    ' A name the pick-list does NOT offer must resolve to 0 through the vocabulary too.
    ' "PageUp"/"PageDn" are the real historical spellings, so a user's settings.ini can
    ' still be carrying them today. Whether they SHOULD be accepted aliases for
    ' "PgUp"/"PgDn" is a vocabulary question and is deliberately still open.
    KBASSERT( KeyBindings_PickListKeyToValue( "PageUp" ) = 0, "stale name ""PageUp"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "PageDn" ) = 0, "stale name ""PageDn"" must resolve to 0" )
    KBASSERT( KeyBindings_PickListKeyToValue( "Insert" ) = 0, "unoffered name ""Insert"" must resolve to 0" )

    ' Case and surrounding space, because the old private tables normalised with
    ' ucase(trim(...)) and existing config files may carry either spelling.
    KBASSERT( KeyBindings_PickListKeyToValue( "pgup" )    = VK_PRIOR, "pick-list must be case-insensitive" )
    KBASSERT( KeyBindings_PickListKeyToValue( " PgUp " )  = VK_PRIOR, "pick-list must trim surrounding space" )

    ' Four spot values spanning the ranges the old 48-arm tables could NOT reach, pinned
    ' against literals so they cannot be satisfied by whatever the lookup happens to return.
    KBASSERT( KeyBindings_PickListKeyToValue( "Tab" )     = VK_TAB,   "pick-list ""Tab"" is VK_TAB" )
    KBASSERT( KeyBindings_PickListKeyToValue( "Numpad+" ) = VK_ADD,   "pick-list ""Numpad+"" is VK_ADD" )
    KBASSERT( KeyBindings_PickListKeyToValue( "Down" )    = VK_DOWN,  "pick-list ""Down"" is VK_DOWN" )
    KBASSERT( KeyBindings_PickListKeyToValue( "F12" )     = VK_F12,   "pick-list ""F12"" is VK_F12" )

    sLine = "  vocabulary: " & str(ubound(gKeyNames) - lbound(gKeyNames) + 1) & " names, " & _
            "bindings: " & str(ubound(gKeys) - lbound(gKeys) + 1) & " commands"
    ? sLine
    sLine = "--- TIKO_KEYBOARD_SELFTEST: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine
    ? ""

end sub


' ========================================================================================
' Build the main keyboard accelerator table
' ========================================================================================
function frmKeyboard_BuildAcceleratorTable() as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit function

    ' Create mapping array that equates the string representation of the
    ' IDM_* messages to their values. We use that array we saving and
    ' restoring the keybindings from file. We can not use the literal IDM
    ' value because if we modify any of the IDM source code values then the
    ' mappings stored in the keybindings file will instantly be incorrect.

    ' Create the default keyboard bindings / shortcut mappings.
    '
    ' NOTE THAT THIS RE-READS keybindings.ini FROM DISK and rebuilds gKeys() from it. Any
    ' caller applying an edit must therefore WRITE THE FILE FIRST and call this second, or
    ' the rebuild silently reinstates whatever was on disk before the edit.
    frmKeyboard_CreateDefaultKeyBindings()

    ' Destroy any existing accelerator table
    pWindow->DestroyAcceleratorTable

    ' The following ALWAYS get created and can not be altered because they are shortcuts
    ' to activate each of the top menus.
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_F,   IDC_MENUBAR_FILE )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_E,   IDC_MENUBAR_EDIT )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_S,   IDC_MENUBAR_SEARCH )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_V,   IDC_MENUBAR_VIEW )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_P,   IDC_MENUBAR_PROJECT )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_C,   IDC_MENUBAR_COMPILE )
    pWindow->AddAccelerator( FVIRTKEY or FALT,     VK_H,   IDC_MENUBAR_HELP )

    ' Create the accelerator table based on the data in the gKeys() array
    for i as long = lbound(gKeys) to ubound(gKeys)

        dim as DWSTRING wszKeys = gKeys(i).wszDefaultKeys

        ' wszUserKeys will override the wszDefaultKeys entry
        if PsLen(gKeys(i).wszUserKeys) then
            wszKeys = gKeys(i).wszUserKeys
        else
            ' Only create the default keyboard short if it has not been disabled
            if gKeys(i).bDefaultDisabled then
                wszKeys = ""
            end if
        end if

        ' No need to proceed further if no User defined shortcut and Default keys disabled
        if PsLen(wszKeys) = 0 then continue for

        ' convert the key combination into something that AddAcelerator can understand
        ' F5
        ' TAB
        ' Alt+F4
        ' Shift+F5
        ' Ctrl+F5
        ' Ctrl+Shift+F5
        ' etc...

        ' Parse for + signs
        dim as long nCount = PsStrParseCount( wszKeys, "+" )
        dim as long nVirtValue = FVIRTKEY
        dim as long nAccelValue = 0

        for ii as long = nCount to 1 step -1
            ' The accelerator key will always be the last parse regardless of
            ' number of items in the string
            dim as DWSTRING wszString = PsStrParse( wszKeys, ii, "+" )
            if ii = nCount then
                nAccelValue = frmKeyboard_AccelKeyToValue( wszString )
            else
                nVirtValue = nVirtValue or frmKeyboard_VirtKeyToValue( wszString )
            end if
        next
        if nAccelValue <> 0 then   ' value can be zero if no default or user key defined
            pWindow->AddAccelerator( nVirtValue, nAccelValue, gKeys(i).idAction )
        end if
    next

    ' Add the SHIFT+INSERT paste accelerator so that the Paste menu operations
    ' will fire correctly.
    pWindow->AddAccelerator( FVIRTKEY or FSHIFT, VK_INSERT, IDM_PASTE )

    pWindow->CreateAcceleratorTable()

    ' The debugger's button tooltips quote the keystroke for each command, so they go stale
    ' the moment a binding changes. Re-armed here rather than in the Keyboard dialog, because
    ' this is the one place every rebind path converges on. A no-op when the window is
    ' closed, which is the usual case.
    if frmDebug_IsOpen() then frmDebug_ArmButtonTooltips()

    function = 0
end function
