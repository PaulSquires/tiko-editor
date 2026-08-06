' ########################################################################################
' modUpdateCheck.inc
'
' The version check against planetsquires.com, and the parsing of its reply.
'
' Split out of modRoutines. This is the only network I/O in tiko and the only place a
' REMOTE input is parsed, which is reason enough for it not to be interleaved with text
' metrics and listview helpers.
'
' PURE MOVE, nothing edited. Included AFTER modRoutines.inc: nothing in modRoutines calls
' any of this, and DoCheckForUpdates is reached from frmMain through its declaration.
' ########################################################################################

#include once "modRoutines.bi"



' ========================================================================================
' Convert Tiko version to whole number for comparison to file versions being loaded.
' ========================================================================================
function ConvertTikoVersion( byref wszVersion as wstring ) as long
   dim as DWSTRING wszPart1, wszPart2, wszPart3
   
   wszPart1 = PsStrLSet( PsStrParse(wszVersion, 1, "."), 3, "0" )
   wszPart2 = PsStrLSet( PsStrParse(wszVersion, 2, "."), 3, "0" )
   wszPart3 = PsStrRSet( PsStrParse(wszVersion, 3, "."), 3, "0" )
   
   function = val(wszPart1 + wszPart2 + wszPart3) 
end function


' ========================================================================================
' Pull "latest_version=x.y.z" out of the server's reply. Pure: string in, string out.
'
' Split out so the parse is assertable without a network. It also has to be tolerant --
' this is a REMOTE input, so a truncated reply, CR/LF or LF line ends, a leading BOM,
' unexpected keys and outright HTML (a captive portal's login page, say) all have to come
' out as "" rather than as a version number.
' ========================================================================================
function ParseLatestVersion( byref sBody as string ) as DWSTRING
    dim as long nStart = 1
    dim as long nLen = len(sBody)

    while nStart <= nLen
        dim as long nEol = instr( nStart, sBody, chr(10) )
        if nEol = 0 then nEol = nLen + 1
        dim as string sLine = mid( sBody, nStart, nEol - nStart )
        ' Tolerate CRLF, a stray CR, and surrounding blanks.
        sLine = trim( sLine, any !" \r\t" )
        if lcase( left( sLine, 15 ) ) = "latest_version=" then
            dim as string sVer = trim( mid( sLine, 16 ) )
            ' A version is digits and dots. Anything else is not one, whatever it claims,
            ' and must not reach ConvertTikoVersion.
            if len( sVer ) = 0 then return ""
            for i as long = 1 to len( sVer )
                dim as long c = asc( sVer, i )
                if (c <> asc(".")) andalso ((c < asc("0")) orelse (c > asc("9"))) then return ""
            next
            return sVer
        end if
        nStart = nEol + 1
    wend

    return ""
end function


' ========================================================================================
' Check PlanetSquires server for latest Tiko version. Runs on its OWN THREAD -- see the
' MSG_USER_PROCESS_CHECKFORUPDATE handler in frmMain, which no longer joins it immediately.
'
' The reply is parsed IN MEMORY. It used to be written to
' PsExePath & "tiko_version.txt" and read back, which was wrong three ways: the
' program directory is not writable on a correctly installed copy, so the check simply
' failed there; the name was fixed and predictable, created with no exclusive-create, in a
' directory an attacker who can write to it controls; and the whole round trip existed to
' recover one line that was already in a string. Nothing is written to disk now.
' ========================================================================================
sub DoCheckForUpdates( byval param as any ptr )

    ''  Contact the PlanetSquires server and read the text file containing
    ''  the latest Tiko version number.

    dim as DWSTRING wszServerFile = "tiko_version.txt"
    dim as DWSTRING wszLatestVersion
    dim as string st

    if AfxWinHttpCheckPlatform() = false then
        ' This platform does not support the Windows HTTP Services (WinHTTP)
        ' print "Windows HTTP Services (WinHTTP) not supported on this platform"
        exit sub
    end if

    dim pWHttp as CWinHttpRequest

    ' Open an HTTP connection to an HTTP resource (synchronous mode)
    if pWHttp.Open( "GET", "https://www.planetsquires.com/" & wszServerFile ) <> S_OK then
        exit sub
    end if

    ' BOUND THE WAIT EXPLICITLY. WaitForResponse below bounds only the RESPONSE; WinHTTP's
    ' own defaults for name resolution and connect are 60 s each, and Send blocks in them.
    ' On a captive portal or a blackholed network that is a minute of nothing, which used to
    ' be a minute of frozen startup because this thread was joined immediately.
    pWHttp.SetTimeouts( 5000, 5000, 5000, 5000 )

    ' Send HTTP request and wait 5 seconds for response
    pWHttp.Send

    if pWHttp.WaitForResponse(5) = 0 then
        exit sub
    end if

    ' Only a 200 carries a version. Anything else -- a redirect to a portal, a 404, a 500 --
    ' is a page, and the old code fed whatever came back straight into the parser.
    if pWHttp.GetStatus() <> 200 then exit sub

    st = pWHttp.GetResponseBody

    ' The reply is a handful of bytes. A larger one is not this file, and there is no reason
    ' to hand an unbounded remote string to the parser.
    const MAX_VERSION_REPLY = 4096
    if PsLen(st) > MAX_VERSION_REPLY then st = PsLeft( st, MAX_VERSION_REPLY ).Utf8

    wszLatestVersion = ParseLatestVersion( st )
    if PsLen(wszLatestVersion) = 0 then exit sub
    
    ' Check the installed vs. available version numbers   
    dim as long latestVersion = ConvertTikoVersion(wszLatestVersion)
    dim as long installedVersion = ConvertTikoVersion(APPVERSION)

'print "wszLatestVersion = "; wszLatestVersion
'print "latestVersion: "; latestVersion, "installedVersion: "; installedVersion

    ' (The two PsFileDelete calls that used to be here -- the same path, deleted twice --
    '  went with the temporary file. Nothing is written now.)

    if installedVersion < latestVersion then
        AppState_SetUpdateAvailable( true )
    end if

    ' Tell the UI thread. This used to be unnecessary because the caller joined this thread
    ' immediately -- which meant the "separate thread in case the check hangs" comment at
    ' the call site described something that could not happen, and a slow network stalled
    ' startup for as long as WinHTTP took to give up.
    PostMessage( HWND_FRMMAIN, MSG_USER_UPDATECHECK_COMPLETE, 0, 0 )
end sub
