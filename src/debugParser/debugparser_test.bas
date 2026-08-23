'' tiko_dbgtest - standalone harness for debugParser.dll.
''
'' Consumes the DLL exactly as a host application does: it includes debugParser.bi and
'' nothing else, so building it is also the cheapest test of whether that header really is
'' self-contained.
''
'' A self-test cannot reach a debug event loop -- that needs a real process, a real message
'' pump and real INT3s in real code. This drives all of it from a console.
''
''   tiko_dbgtest <exe> dump              parse and print the static tables
''   tiko_dbgtest <exe> step [n]          step n times from the first statement
''   tiko_dbgtest <exe> over <line> [n]   run to <line>, then step OVER n times
''   tiko_dbgtest <exe> bp <line> [n]     run to a breakpoint on <line>, then step
''   tiko_dbgtest <exe> vars <line>       run to <line> and print locals + globals
''   tiko_dbgtest <exe> watch <line> <expr>   run to <line> and evaluate <expr>

#define _WIN32_WINNT &h0602
#include once "windows.bi"
#include once "debugParser.bi"

dim shared as long    gMaxStops, gStopCount, gBpLine
dim shared as long    gStepMode
dim shared as string  gMode, gWatchExpr
dim shared as HWND    gHwnd
dim shared as boolean gHitBp, gShowStack

private function ReasonText( byval r as long ) as string
    select case r
        case DBGP_STOP_ENTRY      : return "entry"
        case DBGP_STOP_STEP       : return "step"
        case DBGP_STOP_BREAKPOINT : return "breakpoint"
        case DBGP_STOP_TEMPBP     : return "run-to-cursor"
        case DBGP_STOP_USERBREAK  : return "pause"
        case DBGP_STOP_EXCEPTION  : return "EXCEPTION"
    end select
    return "?"
end function

private function BaseName( byref p as string ) as string
    dim as long i = instrrev( p, "\" )
    if i = 0 then i = instrrev( p, "/" )
    if i > 0 then return mid( p, i + 1 )
    return p
end function

private sub PrintNode( byval pn as DBGP_NODE ptr, byref indent as string )
    print indent & str(pn->wszName) & " : " & str(pn->wszType) & " = " & str(pn->wszValue)

    '' If it is an array, report the shape and prove the FAR end is addressable -- the old
    '' 1000-element cap made everything past that silently unreachable, which looked exactly
    '' like a small array.
    dim as DBGP_ARRAYINFO ai
    if debugparser_array_info( pn, @ai ) <> DBGP_OK then exit sub

    dim as string s = indent & "  [array] dims=" & str(ai.dims) & _
                      iif( ai.isDynamic, " dynamic", " fixed" ) & _
                      " elemSize=" & str(ai.elemSize) & " total=" & str(ai.total) & "  "
    for d as long = 0 to ai.dims - 1
        if d then s &= ", "
        s &= str(ai.lo(d)) & ".." & str(ai.hi(d))
    next
    print s

    if ai.total > 1 then
        dim as DBGP_NODE last_
        dim as long li = cast(long, ai.total - 1)
        if debugparser_child_node( pn, li, @last_ ) = DBGP_OK then
            print indent & "  [last]  #" & str(li) & "  " & str(last_.wszName) & " = " & str(last_.wszValue)
        else
            print indent & "  [last]  #" & str(li) & "  UNREACHABLE"
        end if
    end if
end sub

'' Expand one level of children, so UDT and array handling is actually exercised.
private sub PrintChildren( byval pn as DBGP_NODE ptr, byref indent as string, byval depth as long )
    if depth <= 0 then exit sub
    dim as long n = debugparser_child_count( pn )
    if n <= 0 then exit sub
    if n > 8 then n = 8                       '' keep the transcript readable
    for i as long = 0 to n - 1
        dim as DBGP_NODE ch
        if debugparser_child_node( pn, i, @ch ) <> DBGP_OK then continue for
        PrintNode( @ch, indent )
        PrintChildren( @ch, indent & "  ", depth - 1 )
    next
end sub

private sub DumpVars()
    print "    --- locals (frame 0) ---"
    dim as long n = debugparser_local_count( 0 )
    for i as long = 0 to n - 1
        dim as DBGP_NODE nd
        if debugparser_local_node( 0, i, @nd ) <> DBGP_OK then continue for
        PrintNode( @nd, "      " )
        PrintChildren( @nd, "        ", 2 )
    next
    print "    --- globals ---"
    n = debugparser_global_count()
    for i as long = 0 to n - 1
        dim as DBGP_NODE nd
        if debugparser_global_node( i, @nd ) <> DBGP_OK then continue for
        PrintNode( @nd, "      " )
        PrintChildren( @nd, "        ", 1 )
    next
end sub

private function WndProc( byval hWnd as HWND, byval uMsg as UINT, _
                          byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case MSG_DBGP_STOPPED
            gStopCount += 1
            dim as DBGP_STOPINFO si
            debugparser_get_stop( @si )

            dim as string locTxt = "<no line>"
            if si.srcIndex >= 0 then locTxt = BaseName( str(si.wszSrcPath) ) & ":" & str(si.lineNum)

            print "  stop " & gStopCount & "  " & ReasonText( si.reason ) & "  &h" & hex(si.addr) & _
                  "  " & locTxt & "  in " & str(si.wszProcName) & "  frames=" & si.frameCount

            if si.reason = DBGP_STOP_EXCEPTION then
                print "        " & str(si.wszExcepText) & " at &h" & hex(si.excepAddr)
            end if

            if gShowStack then
                for f as long = 0 to si.frameCount - 1
                    dim as DBGP_FRAME fr
                    if debugparser_get_frame( f, @fr ) <> DBGP_OK then continue for
                    dim as string fl = "<no line>"
                    if fr.srcIndex >= 0 then fl = BaseName( str(fr.wszSrcPath) ) & ":" & str(fr.lineNum)
                    print "          #" & f & "  " & fl & "  " & str(fr.wszProcName)
                next
            end if

            if si.reason = DBGP_STOP_BREAKPOINT then gHitBp = true

            if (gMode = "vars") andalso gHitBp then
                DumpVars()
                debugparser_stop()
                return 0
            end if

            if (gMode = "watch") andalso gHitBp then
                dim as DBGP_NODE nd
                dim as wstring * 256 wexpr = gWatchExpr
                if debugparser_evaluate( @wexpr, 0, @nd ) = DBGP_OK then
                    print "    " & gWatchExpr & " => " & str(nd.wszType) & " = " & str(nd.wszValue)
                    PrintChildren( @nd, "        ", 2 )
                else
                    print "    " & gWatchExpr & " => <not found>"
                end if
                debugparser_stop()
                return 0
            end if

            dim as boolean bStepNow = true
            if (gMode = "bp") orelse (gMode = "over") then
                if gHitBp = false then bStepNow = false
            end if

            if gStopCount >= gMaxStops then
                print "  (limit reached)"
                debugparser_stop()
            elseif bStepNow then
                debugparser_resume( gStepMode )
            else
                debugparser_resume( DBGP_RUN_CONTINUE )
            end if
            return 0

        case MSG_DBGP_EXITED
            print "  debuggee exited, code " & str(cast(long, wParam))
            PostQuitMessage(0)
            return 0

        case MSG_DBGP_FAILED
            dim as wstring * 512 werr
            debugparser_get_error( @werr, 512 )
            print "  LAUNCH FAILED: " & str(werr)
            PostQuitMessage(1)
            return 0
    end select
    return DefWindowProc( hWnd, uMsg, wParam, lParam )
end function

'' ---------------------------------------------------------------------------
dim as string target = command(1)
if len(target) = 0 then
    print "usage: tiko_dbgtest <exe> dump|step|over|bp|vars|watch [args]"
    end 1
end if

gMode      = lcase( command(2) )
gMaxStops  = 8
gStepMode  = DBGP_RUN_STEPINTO
gShowStack = (environ("DBGP_STACK") = "1")

select case gMode
    case "over"
        gBpLine   = valint( command(3) )
        gStepMode = DBGP_RUN_STEPOVER
        if len(command(4)) then gMaxStops = valint( command(4) )
    case "bp"
        gBpLine = valint( command(3) )
        if len(command(4)) then gMaxStops = valint( command(4) )
    case "vars"
        gBpLine = valint( command(3) )
        gMaxStops = 200
    case "watch"
        gBpLine    = valint( command(3) )
        gWatchExpr = command(4)
        gMaxStops  = 200
    case "step", ""
        gMode = "step"
        if len(command(3)) then gMaxStops = valint( command(3) )
end select

dim as wstring * 1024 wTarget = target
dim as long rc = debugparser_load( @wTarget, 0 )
if rc <> DBGP_OK then
    dim as wstring * 512 werr
    debugparser_get_error( @werr, 512 )
    print "load failed (" & rc & "): " & str(werr)
    end 1
end if

dim as wstring * 128 wcomp
debugparser_get_compiler( @wcomp, 128 )
print "loaded: sources=" & debugparser_source_count() & _
      "  target=" & iif( debugparser_target_is_64bit(), "64-bit", "32-bit" ) & _
      "  compiler=" & str(wcomp)
if debugparser_line_cap_hit() then
    print "WARNING: a line number reached the format's 65535 ceiling"
end if

if gMode = "dump" then
    for i as long = 0 to debugparser_source_count() - 1
        dim as wstring * DBGP_MAXPATH wp
        debugparser_source_path( i, @wp, DBGP_MAXPATH )
        print "  [" & i & "] " & str(wp)
    next
    debugparser_unload()
    end 0
end if

'' A message-only window: the DLL posts its notifications to a real HWND.
dim as WNDCLASSEX wc
wc.cbSize        = sizeof(WNDCLASSEX)
wc.lpfnWndProc   = @WndProc
wc.hInstance     = GetModuleHandle(null)
wc.lpszClassName = cast(LPCTSTR, @wstr("dbgptesthost"))
if RegisterClassEx( @wc ) = 0 then print "RegisterClassEx failed" : end 1
gHwnd = CreateWindowEx( 0, cast(LPCTSTR, @wstr("dbgptesthost")), cast(LPCTSTR, @wstr("")), _
                        0, 0, 0, 0, 0, HWND_MESSAGE, 0, wc.hInstance, 0 )
if gHwnd = 0 then print "CreateWindowEx failed" : end 1

if gBpLine > 0 then
    dim as long li = debugparser_next_executable_line( 0, gBpLine )
    if li < 0 then print "line " & gBpLine & " has no executable line at or after it" : end 1
    if li <> gBpLine then print "  (line " & gBpLine & " is not executable; using " & li & ")"
    debugparser_add_breakpoint( 0, li )
    print "breakpoint at line " & li
end if

print "starting..."
rc = debugparser_start( @wTarget, null, gHwnd )
if rc <> DBGP_OK then
    dim as wstring * 512 werr
    debugparser_get_error( @werr, 512 )
    print "start failed (" & rc & "): " & str(werr)
    end 1
end if

dim as MSG uMsg
do while GetMessage( @uMsg, null, 0, 0 )
    TranslateMessage( @uMsg )
    DispatchMessage( @uMsg )
loop

debugparser_unload()
print "done."
