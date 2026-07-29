' ########################################################################################
'  PsTipHost -- the tooltip backend switch shared by the owner-drawn control family.
'
'  NOT A CONTROL. Like PsBufferPaint and PsImage it is a support type a control HOLDS: no
'  HWND of its own, no window, no message pump obligation. One PSTIPHOST field replaces the
'  `hToolTip as HWND` + `HoverTime as long` pair every control in the family carries today.
'
'  ------------------------------------------------------------------------------------
'  WHY THIS EXISTS
'  ------------------------------------------------------------------------------------
'  Thirteen files in this family create a comctl32 tooltip, in three shapes (whole-control,
'  per-item with TTM_POP, and TTF_TRACK). Converting each by hand means writing the same
'  sixty lines thirteen times and having them drift thirteen ways. This holds the one copy.
'
'  ------------------------------------------------------------------------------------
'  WHAT IT DOES *NOT* DO -- read this before extending it
'  ------------------------------------------------------------------------------------
'  It does not own the tooltip TEXT and must never learn to. Every control in this family
'  already has a private Ps<X>_ResolveTooltipText with its own policy (authored text wins,
'  callback second, and whether there is a caption/item-text fallback differs per control).
'  This type carries a CALLBACK to that policy and nothing else -- so switching backends
'  cannot change what a tip says, only how it is drawn and driven.
'
'  ------------------------------------------------------------------------------------
'  THE DEFAULT IS PSTIP_MODE_SYSTEM, DELIBERATELY
'  ------------------------------------------------------------------------------------
'  Every existing host keeps the comctl32 tooltip it has today unless it asks for otherwise.
'  That is not timidity: PsTooltip's colour defaults are DARK and the system tip is light, so
'  a control that silently switched would put a dark tip on a light form. A host opts in per
'  control with Ps<X>_SetTooltipMode, and themes them all at once with
'  PsTooltip_SetDefaultColors and friends.
'
'  ------------------------------------------------------------------------------------
'  SINGLE-THREADED. UI thread only, like everything else here.
' ########################################################################################

#pragma once

#include once "AfxNova/CWindow.inc"
#include once "PsTooltip.bi"

using AfxNova


enum
    PSTIP_MODE_SYSTEM = 0        ' comctl32 tooltips_class32 (the default; unchanged behaviour)
    PSTIP_MODE_PS     = 1        ' PsTooltip
end enum


' The per-control state. A control declares ONE of these and passes its address in.
'
' Both handles are kept rather than one: a mode switch destroys the outgoing backend's window
' and builds the incoming one, and keeping them in separate fields is what makes "which one am
' I on" answerable without consulting nMode and hoping the two agree.
type PSTIPHOST
    nMode        as long = PSTIP_MODE_SYSTEM
    hSysTip      as HWND         ' comctl32 window, 0 unless nMode = PSTIP_MODE_SYSTEM
    hPsTip       as HWND         ' PsTooltip window,  0 unless nMode = PSTIP_MODE_PS
    hCtrl        as HWND         ' the control being served; set by the first Ensure

    ' The text policy, supplied by the control. Installed on the PsTooltip backend and
    ' remembered so a later mode switch can re-install it. In system mode it is unused --
    ' comctl32 asks the control's own WndProc through TTN_GETDISPINFOW instead, which is why
    ' that arm stays in every control rather than being absorbed here.
    TipCallback  as TIP_TooltipCallbackFunc

    ' -1 = never set by the host, so the backend keeps its own derivation. A stored 0 is a
    ' real request for "no delay" and must survive a mode switch; conflating the two with 0
    ' would hand comctl32's system-derived hover time to a host that asked for none.
    nInitialMs   as long = -1
    nAutoPopMs   as long = -1
    nReshowMs    as long = -1

    ' A TRACKED tip is positioned and shown by the control rather than by a dwell -- see
    ' PsTipHost_SetTracked. It changes how the comctl32 tool is BUILT, so it must be declared
    ' before the first Ensure.
    bTracked     as boolean = false
end type


' --- lifecycle ----------------------------------------------------------------------------
' Build the backing tip for the CURRENT mode if it does not exist yet. Idempotent, so a
' control may call it from Create and again from any setter that needs a live tip. Returns
' true if a tip exists on return.
declare function PsTipHost_Ensure( byval pHost as PSTIPHOST ptr, byval hCtrl as HWND, _
                                   byval TipCallback as TIP_TooltipCallbackFunc = 0 ) as boolean

' Destroy whichever backend is live. Call from the control's WM_NCDESTROY, exactly where
' DestroyWindow(pBtn->hToolTip) is called today.
declare sub      PsTipHost_Destroy( byval pHost as PSTIPHOST ptr )

' --- mode ---------------------------------------------------------------------------------
' Switching destroys the outgoing tip and builds the incoming one, re-applying the stored
' delays. Cheap enough to call at any time; a no-op when the mode is already the one asked
' for. Returns true if the requested mode is live on return.
declare function PsTipHost_SetMode( byval pHost as PSTIPHOST ptr, byval nMode as long ) as boolean
declare function PsTipHost_GetMode( byval pHost as PSTIPHOST ptr ) as long

' The live handle for each backend, 0 when that backend is not the current one. The PsTooltip
' handle is the host's door to PsTooltip's own SetColors/SetFonts/SetStyle/SetMaxWidth/
' SetTitle/SetGlyph -- those are deliberately NOT mirrored per control, since thirteen
' controls x twenty setters is 260 wrappers to keep in step.
declare function PsTipHost_SysHandle( byval pHost as PSTIPHOST ptr ) as HWND
declare function PsTipHost_PsHandle( byval pHost as PSTIPHOST ptr ) as HWND

' --- per-item -----------------------------------------------------------------------------
' The hot item changed. This is the single call site that sends TTM_POP today: on the system
' backend it still does, and on PsTooltip it moves the tool index (which hides a visible tip
' and re-resolves on the next dwell). idx -1 is a whole-control tip.
declare sub      PsTipHost_SetToolIndex( byval pHost as PSTIPHOST ptr, byval idx as long )

' Take a visible tip down without changing the item -- the control is being hidden, disabled,
' pressed, or destroyed.
declare sub      PsTipHost_Pop( byval pHost as PSTIPHOST ptr )

' --- tracked tips (PsCalendar, PsDatePicker) ----------------------------------------------
' THE THIRD SHAPE. Most controls in this family let the tooltip decide when to appear: they
' name the item and the dwell does the rest. The two calendars do not -- they resolve a tip
' for the day cell under the cursor, position it themselves and show it immediately, which on
' comctl32 means a TTF_TRACK tool driven with TTM_TRACKPOSITION / TTM_TRACKACTIVATE.
'
' Declare it BEFORE the first Ensure. A tracked tool differs at CREATION (TTF_TRACK plus
' TTF_ABSOLUTE, and text pushed rather than pulled through LPSTR_TEXTCALLBACK), so switching
' this afterwards would leave a tool of the wrong kind.
declare sub      PsTipHost_SetTracked( byval pHost as PSTIPHOST ptr, byval bTracked as boolean )
declare function PsTipHost_IsTracked( byval pHost as PSTIPHOST ptr ) as boolean

' Show the tip NOW, with this text, at this place. Both arguments are in SCREEN coordinates
' and both are needed because the two backends place differently and neither can derive the
' other's input: comctl32 is told a POINT and puts its top-left corner there, PsTooltip is
' given the ANCHOR RECT and does its own flip/clamp against the work area. Handing PsTooltip a
' degenerate rect at the point would throw away exactly the placement it exists to do.
declare sub      PsTipHost_TrackShow( byval pHost as PSTIPHOST ptr, byval ptScreen as POINT, _
                                      byval rcAnchorScreen as RECT, byval wszText as DWSTRING )
' Take a tracked tip down. Safe to call when nothing is showing.
declare sub      PsTipHost_TrackHide( byval pHost as PSTIPHOST ptr )

' --- timing -------------------------------------------------------------------------------
' nWhich is comctl32's own TTDT_INITIAL / TTDT_AUTOPOP / TTDT_RESHOW, reused rather than
' re-invented so a control's existing TTM_SETDELAYTIME call site converts without a new
' vocabulary. Honoured by BOTH backends, which is the whole point: a host's hover time must
' not depend on which tooltip it happens to be looking at.
declare sub      PsTipHost_SetDelay( byval pHost as PSTIPHOST ptr, byval nWhich as long, _
                                     byval nMs as long )
declare function PsTipHost_GetDelay( byval pHost as PSTIPHOST ptr, byval nWhich as long ) as long
