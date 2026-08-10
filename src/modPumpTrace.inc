'' ========================================================================================
'' modPumpTrace -- implementation. See modPumpTrace.bi for what this is and why.
'' ========================================================================================

dim shared gPumpTraceOn as boolean


type PumpTraceSlot
    sPump      as string
    sWho       as string
    nCount     as ulongint
    nSamples   as long
    sSample(0 to PUMPTRACE_MAXSAMPLES - 1) as string
end type

dim shared gPumpSlots(0 to PUMPTRACE_MAXSLOTS - 1) as PumpTraceSlot
dim shared gPumpSlotCount as long
dim shared gPumpOverflow  as long      '' claims that found no free slot -- reported, not dropped silently
dim shared gPumpWritten   as boolean

'' Messages seen entering each pump, so a claim count can be read as a share rather than as
'' a bare number. Same key as the slots but one dimension.
type PumpTraceLoop
    sPump   as string
    nSeen   as ulongint
end type

dim shared gPumpLoops(0 to 31) as PumpTraceLoop
dim shared gPumpLoopCount as long


'' ---------------------------------------------------------------------------------------
'' Compact, stable descriptions. Stable matters: the sample list is DISTINCT descriptors, so
'' a describer that varied its output would fill the twelve slots with one event.
'' ---------------------------------------------------------------------------------------
private function PumpTrace_VkName( byval nVk as long ) as string
    select case nVk
        case VK_LEFT     : return "VK_LEFT"
        case VK_RIGHT    : return "VK_RIGHT"
        case VK_UP       : return "VK_UP"
        case VK_DOWN     : return "VK_DOWN"
        case VK_HOME     : return "VK_HOME"
        case VK_END      : return "VK_END"
        case VK_PRIOR    : return "VK_PRIOR"
        case VK_NEXT     : return "VK_NEXT"
        case VK_RETURN   : return "VK_RETURN"
        case VK_ESCAPE   : return "VK_ESCAPE"
        case VK_TAB      : return "VK_TAB"
        case VK_SPACE    : return "VK_SPACE"
        case VK_BACK     : return "VK_BACK"
        case VK_DELETE   : return "VK_DELETE"
        case VK_INSERT   : return "VK_INSERT"
        case VK_MENU     : return "VK_MENU"
        case VK_CONTROL  : return "VK_CONTROL"
        case VK_SHIFT    : return "VK_SHIFT"
        case VK_F1 to VK_F24 : return "VK_F" & str(nVk - VK_F1 + 1)
        case 48 to 57    : return "'" & chr(nVk) & "'"
        case 65 to 90    : return "'" & chr(nVk) & "'"
    end select
    return "vk" & str(nVk)
end function


'' The modifier state at the moment of the claim. Part of the descriptor because "the
'' accelerator table claimed VK_S" and "it claimed Ctrl+S" are different facts, and the
'' census cares which.
private function PumpTrace_Mods() as string
    dim as string s
    if (GetKeyState(VK_CONTROL) and &h8000) <> 0 then s &= "Ctrl+"
    if (GetKeyState(VK_MENU)    and &h8000) <> 0 then s &= "Alt+"
    if (GetKeyState(VK_SHIFT)   and &h8000) <> 0 then s &= "Shift+"
    return s
end function


private function PumpTrace_Describe( byval pMsg as MSG ptr ) as string
    if pMsg = 0 then return "?"

    select case pMsg->message
        case WM_KEYDOWN     : return "WM_KEYDOWN "    & PumpTrace_Mods() & PumpTrace_VkName(pMsg->wParam)
        case WM_KEYUP       : return "WM_KEYUP "      & PumpTrace_Mods() & PumpTrace_VkName(pMsg->wParam)
        case WM_SYSKEYDOWN  : return "WM_SYSKEYDOWN " & PumpTrace_Mods() & PumpTrace_VkName(pMsg->wParam)
        case WM_SYSKEYUP    : return "WM_SYSKEYUP "   & PumpTrace_Mods() & PumpTrace_VkName(pMsg->wParam)

        '' WM_CHAR is reported by CLASS, not by character. A per-character descriptor would
        '' spend all twelve sample slots on whatever the author happened to type.
        case WM_CHAR
            if pMsg->wParam < 32 then return "WM_CHAR ctrl-" & str(pMsg->wParam)
            return "WM_CHAR printable"
        case WM_SYSCHAR     : return "WM_SYSCHAR"

        case WM_LBUTTONDOWN : return "WM_LBUTTONDOWN"
        case WM_LBUTTONUP   : return "WM_LBUTTONUP"
        case WM_LBUTTONDBLCLK : return "WM_LBUTTONDBLCLK"
        case WM_RBUTTONDOWN : return "WM_RBUTTONDOWN"
        case WM_RBUTTONUP   : return "WM_RBUTTONUP"
        case WM_MBUTTONDOWN : return "WM_MBUTTONDOWN"
        case WM_MOUSEMOVE   : return "WM_MOUSEMOVE"
        case WM_MOUSEWHEEL  : return "WM_MOUSEWHEEL"
        case WM_NCLBUTTONDOWN : return "WM_NCLBUTTONDOWN"

        case WM_TIMER       : return "WM_TIMER"
        case WM_SETFOCUS    : return "WM_SETFOCUS"
        case WM_KILLFOCUS   : return "WM_KILLFOCUS"
        case WM_COMMAND     : return "WM_COMMAND"
        case WM_PAINT       : return "WM_PAINT"
    end select

    return "msg &h" & hex(pMsg->message)
end function


'' The writer runs from a module-level destructor so that it fires on a normal quit and on
'' `end` alike -- tiko's own layout oracle exits with `end 0`, and a trace that only appeared
'' when the user closed the window politely would be missing exactly when it was wanted.
'' fbc will not accept a field-less type. The flag is not filler: the destructor runs for every
'' process, including ones that never reached a message loop, and bArmed is how it tells.
type PumpTraceAtExit
    bArmed as boolean
    declare destructor()
end type

destructor PumpTraceAtExit()
    '' Armed by PumpTrace_Init only. Without this the destructor would run for a process that
    '' never reached a message loop at all -- the /help and command-line paths -- and write a
    '' trace file describing nothing.
    if this.bArmed = false then exit destructor
    PumpTrace_Write()
end destructor

dim shared gPumpTraceAtExit as PumpTraceAtExit


'' ---------------------------------------------------------------------------------------
sub PumpTrace_Init()
    gPumpTraceOn = (environ("TIKO_PUMP_TRACE") = "1")
    gPumpTraceAtExit.bArmed = true
end sub


private sub PumpTrace_Record( byval zPump as zstring ptr, _
                              byval zWho as zstring ptr, _
                              byval pMsg as MSG ptr )

    dim as string sPump = *zPump
    dim as string sWho  = *zWho

    dim as long nSlot = -1
    for i as long = 0 to gPumpSlotCount - 1
        if (gPumpSlots(i).sWho = sWho) andalso (gPumpSlots(i).sPump = sPump) then
            nSlot = i
            exit for
        end if
    next

    if nSlot = -1 then
        if gPumpSlotCount >= PUMPTRACE_MAXSLOTS then
            '' COUNTED, NOT DROPPED. A summary that silently omitted claimants past the cap
            '' would read as "these are all of them", which is the failure this whole file
            '' exists to avoid.
            gPumpOverflow += 1
            exit sub
        end if
        nSlot = gPumpSlotCount
        gPumpSlotCount += 1
        gPumpSlots(nSlot).sPump = sPump
        gPumpSlots(nSlot).sWho  = sWho
    end if

    gPumpSlots(nSlot).nCount += 1

    dim as string sDesc = PumpTrace_Describe( pMsg )
    for i as long = 0 to gPumpSlots(nSlot).nSamples - 1
        if gPumpSlots(nSlot).sSample(i) = sDesc then exit sub
    next
    if gPumpSlots(nSlot).nSamples < PUMPTRACE_MAXSAMPLES then
        gPumpSlots(nSlot).sSample( gPumpSlots(nSlot).nSamples ) = sDesc
        gPumpSlots(nSlot).nSamples += 1
    end if
end sub


function PumpTrace( byval bClaimed as boolean, _
                    byval zPump as zstring ptr, _
                    byval zWho as zstring ptr, _
                    byval pMsg as MSG ptr ) as boolean

    '' Both tests are boolean -- kept that way deliberately, because mixing a boolean and a
    '' non-boolean operand in an orelse chain is fbc warning 38.
    if bClaimed = false then return false
    if gPumpTraceOn = false then return true

    PumpTrace_Record( zPump, zWho, pMsg )
    return true
end function


sub PumpTrace_Note( byval zPump as zstring ptr, _
                    byval zWho as zstring ptr, _
                    byval pMsg as MSG ptr )
    if gPumpTraceOn = false then exit sub
    PumpTrace_Record( zPump, zWho, pMsg )
end sub


'' THE TERMINAL PAIR, SPLIT BY MESSAGE CLASS -- and the split is the whole point.
''
'' The first driven trace read "IsDialogMessage claims 4115 of 5517" and that number invites
'' exactly the wrong conclusion. IsDialogMessage DISPATCHES what it processes: the API docs
'' are explicit that a message it handled must NOT then be passed to TranslateMessage or
'' DispatchMessage. So a TRUE return means "I was the dispatcher", not "I swallowed it" --
'' which is why WM_PAINT, WM_TIMER and WM_MOUSEMOVE all appeared in its claim list while the
'' editor went on repainting perfectly.
''
'' Undivided, the count measures nothing but message volume. The quantity the port actually
'' needs is the KEYBOARD share, because dialog-key semantics -- Tab traversal, the default
'' button, Escape -- are the part with no PsPlatform analogue. Everything else it "claims" is
'' dispatch that surf.Dispatch already does.
sub PumpTrace_NoteTerminal( byval zPump as zstring ptr, _
                            byval bIsDlg as boolean, _
                            byval pMsg as MSG ptr )
    if gPumpTraceOn = false then exit sub
    if pMsg = 0 then exit sub

    dim as boolean bKey = false
    select case pMsg->message
        case WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_SYSCHAR, WM_DEADCHAR
            bKey = true
    end select

    if bIsDlg then
        if bKey then
            PumpTrace_Record( zPump, "IsDialogMessage:KEYBOARD", pMsg )
        else
            PumpTrace_Record( zPump, "IsDialogMessage:dispatched-other", pMsg )
        end if
    else
        if bKey then
            PumpTrace_Record( zPump, "DispatchMessage:KEYBOARD", pMsg )
        else
            PumpTrace_Record( zPump, "DispatchMessage:other", pMsg )
        end if
    end if
end sub


sub PumpTrace_Enter( byval zPump as zstring ptr )
    if gPumpTraceOn = false then exit sub

    dim as string sPump = *zPump
    for i as long = 0 to gPumpLoopCount - 1
        if gPumpLoops(i).sPump = sPump then
            gPumpLoops(i).nSeen += 1
            exit sub
        end if
    next
    if gPumpLoopCount >= ubound(gPumpLoops) then exit sub
    gPumpLoops(gPumpLoopCount).sPump = sPump
    gPumpLoops(gPumpLoopCount).nSeen = 1
    gPumpLoopCount += 1
end sub


'' ---------------------------------------------------------------------------------------
sub PumpTrace_Write()
    if gPumpTraceOn = false then exit sub
    if gPumpWritten then exit sub
    gPumpWritten = true

    dim as string sOut = environ("TIKO_PUMP_TRACE_OUT")
    if len(sOut) = 0 then sOut = exepath() & "/pump-trace.txt"

    dim as long f = freefile
    if open( sOut for output as #f ) <> 0 then exit sub

    print #f, "TIKO PUMP TRACE"
    print #f, ""
    print #f, "Every filter claim in every message loop, counted. Written at process exit."
    print #f, "A claimant absent from this file NEVER FIRED in this session -- which is a"
    print #f, "fact about the session, not about the code. See docs/port/pump-census.md."
    print #f, ""

    print #f, "---- messages entering each pump ----"
    for i as long = 0 to gPumpLoopCount - 1
        print #f, "  " & gPumpLoops(i).sPump & "  " & str(gPumpLoops(i).nSeen)
    next
    print #f, ""

    print #f, "---- claims ----"
    '' Grouped by pump, in the order each pump was first seen, so the main loop's claims read
    '' in roughly the order the source lists them.
    for nLoop as long = 0 to gPumpLoopCount - 1
        dim as string sPump = gPumpLoops(nLoop).sPump
        dim as boolean bAny = false
        for i as long = 0 to gPumpSlotCount - 1
            if gPumpSlots(i).sPump <> sPump then continue for
            if bAny = false then
                print #f, ""
                print #f, "[" & sPump & "]"
                bAny = true
            end if
            print #f, "  " & gPumpSlots(i).sWho & "  claims " & str(gPumpSlots(i).nCount)
            for k as long = 0 to gPumpSlots(i).nSamples - 1
                print #f, "      " & gPumpSlots(i).sSample(k)
            next
            if gPumpSlots(i).nSamples >= PUMPTRACE_MAXSAMPLES then
                print #f, "      (sample list full -- there may be more kinds than these)"
            end if
        next
    next

    if gPumpOverflow > 0 then
        print #f, ""
        print #f, "OVERFLOW: " & str(gPumpOverflow) & " claims had no free slot (cap " & _
                  str(PUMPTRACE_MAXSLOTS) & "). The list above is INCOMPLETE."
    end if

    close #f
end sub
