'' ========================================================================================
'' mdMailbox -- one running viewer, reached by a file. NO PLATFORM CODE AT ALL.
''
'' ---- WHY A FILE, AND NOT WM_COPYDATA -------------------------------------------------
''
'' tiko presses F1 repeatedly. Without this, every press launches another viewer and they
'' pile up. The obvious Windows answer is a named mutex plus a message-only window and
'' WM_COPYDATA -- which is raw Win32 above the platform layer, exactly what PsPlatform's
'' isolation ratchet exists to stop, and which would need writing a second time as an
'' abstract-namespace unix socket for the Linux port. PsPlatform offers no IPC of any kind.
''
'' A lock file and a request file need neither, behave identically on both platforms, and
'' cost about eighty lines. The price is latency -- up to one poll interval -- which for
'' "the help window comes forward" is not a price anyone can perceive.
''
'' ---- A STALE LOCK MUST NOT WEDGE IT --------------------------------------------------
''
'' A viewer killed in the debugger leaves its lock behind. If the lock's mere existence meant
'' "an instance is running", the next F1 would write a request nobody reads and exit, and the
'' help viewer would appear to be broken forever with no message. So the lock is a HEARTBEAT:
'' the running instance rewrites it every MDM_BEAT_MS, and a lock older than MDM_STALE_MS is
'' treated as abandoned. The failure mode is then a second window, once, which is obvious and
'' harmless -- rather than no window at all, which is neither.
''
'' ---- WHAT IT CANNOT DO ---------------------------------------------------------------
''
'' RAISE THE WINDOW. IWindowBackend has no Raise and no Focus -- Wayland does not let a client
'' take focus for itself. Show(true) is the only lever and on Windows it usually suffices.
'' Named here because "the topic changed but the window stayed behind the editor" is
'' otherwise a mystery rather than a known limit.
'' ========================================================================================

#pragma once

const MDM_BEAT_MS  = 2000      '' how often the owner re-stamps the lock
const MDM_STALE_MS = 8000      '' older than this and the lock is somebody's corpse
const MDM_POLL_MS  = 250       '' how often the owner looks for a request

private function MdmDir() as DWSTRING
    return PsPathJoin( PsKnownFolder(PSFOLDER_CACHE), PsText("F1Markdown") )
end function

private function MdmLockPath() as DWSTRING
    return PsPathJoin( MdmDir(), PsText("instance.lock") )
end function

private function MdmRequestPath() as DWSTRING
    return PsPathJoin( MdmDir(), PsText("request.txt") )
end function

'' TRUE when another viewer is alive. Decided by writing a PROBE file and comparing its write
'' time with the lock's, rather than by asking the system for the time: there is no portable
'' clock in the platform interface, and the two files' timestamps come from the same source,
'' so they are comparable even when the machine's clock is wrong.
declare function MdMailboxOwnerLive() as boolean
function MdMailboxOwnerLive() as boolean
    dim as DWSTRING sLock = MdmLockPath()
    if PsFileExists( sLock ) = false then return false

    dim as DWSTRING sProbe = PsPathJoin( MdmDir(), PsText("probe.tmp") )
    PsDirCreateTree( MdmDir() )
    if PsFileWriteAll( sProbe, "." ) = false then
        '' Cannot write to our own cache folder. Assume nobody is home rather than refusing
        '' to start: a second window beats no window.
        return false
    end if
    dim as longint nNow  = PsFileTimeToUnix( PsFileWriteTime(sProbe) )
    dim as longint nThen = PsFileTimeToUnix( PsFileWriteTime(sLock) )
    PsFileDelete( sProbe )
    if (nNow <= 0) orelse (nThen <= 0) then return false
    return ((nNow - nThen) * 1000) < MDM_STALE_MS
end function

'' Hands a topic to the running viewer. The caller then exits.
declare function MdMailboxPost( byref sTopic as DWSTRING ) as boolean
function MdMailboxPost( byref sTopic as DWSTRING ) as boolean
    PsDirCreateTree( MdmDir() )
    return PsFileWriteAll( MdmRequestPath(), sTopic.Utf8 )
end function

'' Claims the lock. Called once, by the instance that is going to own the window.
declare sub MdMailboxTakeLock()
sub MdMailboxTakeLock()
    PsDirCreateTree( MdmDir() )
    PsFileWriteAll( MdmLockPath(), "F1Markdown" )
    '' Anything left by a previous run is somebody else's stale request and must not be
    '' answered as if it had just arrived.
    PsFileDelete( MdmRequestPath() )
end sub

declare sub MdMailboxReleaseLock()
sub MdMailboxReleaseLock()
    PsFileDelete( MdmLockPath() )
    PsFileDelete( MdmRequestPath() )
end sub

'' Re-stamps the lock. Called from the pump on the beat interval.
declare sub MdMailboxBeat()
sub MdMailboxBeat()
    PsFileWriteAll( MdmLockPath(), "F1Markdown" )
end sub

'' Takes a pending request, if there is one. The file is DELETED BEFORE the topic is returned,
'' so a handler that throws or a viewer that dies mid-navigation cannot make the same request
'' fire again on every poll for the rest of the session.
declare function MdMailboxTake( byref sTopic as DWSTRING ) as boolean
function MdMailboxTake( byref sTopic as DWSTRING ) as boolean
    dim as DWSTRING sReq = MdmRequestPath()
    if PsFileExists( sReq ) = false then return false
    dim as boolean bOk = false
    dim as string s = MdReadUtf8( sReq, bOk )
    PsFileDelete( sReq )
    if bOk = false then return false
    s = trim(s, any chr(32) & chr(9) & chr(13) & chr(10))
    if len(s) = 0 then return false
    sTopic.Utf8 = s
    return true
end function
