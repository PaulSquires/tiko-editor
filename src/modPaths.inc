' ########################################################################################
' modPaths.inc
'
' The {CURDRIVE} placeholder conversions and filename case recovery.
'
' Split out of modRoutines. These four Process*Curdrive* functions are the reason a
' settings.ini or .tiko can move between machines and drive letters at all, and they are
' called from clsConfig, modCompile and the scan manager -- a coherent unit that had no
' reason to live beside the file dialogs.
'
' PURE MOVE, nothing edited. AfxFilenameOriginalCase travels with FilenameOriginalCase
' because it is its only caller and it is declared in no .bi -- separating them would leave
' a use before its definition.
' ########################################################################################

#include once "modRoutines.bi"



' ========================================================================================
' Get the original case filename as stored by Windows (used when converting the GDB
' returned filename from uppercase to original case).
' ========================================================================================
function AfxFilenameOriginalCase( byval filename as DWSTRING ) as DWSTRING
    dim as long bufSize = 1024
    dim as DWSTRING buffer = wspace(bufSize \ 2)

    dim as HANDLE hfile = CreateFileW(filename.Wz(), GENERIC_READ, FILE_SHARE_READ,                                  null, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null)

    if hfile = INVALID_HANDLE_VALUE then exit function

    dim as DWORD result = GetFinalPathNameByHandleW( hfile, buffer.Wz(), bufSize, FILE_NAME_NORMALIZED )

    if (result > 0) andalso (result < bufSize) then
        ' Good data returned but still need to remove the mapping characters
        if PsLeft(buffer, 4) = "\\?\" then buffer = PsMid(buffer, 5)
    else
        ' error: could not retrieve path. 
    end if

    ' clean up
    CloseHandle(hfile)
    
    function = buffer
end function


' FilenameOriginalCase MOVED TO app/modPaths.inc IN 7c STEP 8. It was the last Win32 thing
' in this file's half of the module -- CreateFileW plus GetFinalPathNameByHandleW -- and it
' sat on _check_app_standalone's link-debt list waiting for "a PsCore canonical-path call",
' which turned out to exist already. See the new body's header for what changed for tiko:
' the case is still resolved, the symlink and junction resolution is not.


' ========================================================================================
' Process prefix {CURDRIVE} and convert to current drive letter.
' ========================================================================================
function ProcessToCurdriveApp( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the editor. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim as DWSTRING wszText = PsExePath

    dim wszCurDrive as DWSTRING = PsLCase(PsLeft(wszText, 3))  ' eg. D:\

    ' if the incoming filename is a relative file name then the following test
    ' will have no effect.
    if PsLCase(PsLeft(wszFilename, 3)) = wszCurDrive then 
        wszFilename = wstr("{CURDRIVE}") & PsMid(wszFilename, 2)
    end if

    return wszFilename
end function


' ProcessFromCurdriveApp MOVED to app/modPaths.inc (7c step 5). Its declaration has been
' in app/modPaths.bi since step 1 and the body is pure PsCore -- PsExePath, PsUCase,
' PsLeft, PsMid -- so it was on _check_app_standalone's list of things that move as soon
' as something needs them. app/clsSymbolDb.inc calls it at three sites; the shell binary
' including that body is what finally needed it.



' ========================================================================================
' Process prefix {CURDRIVE} and convert to current drive letter.
' ========================================================================================
function ProcessToCurdriveProject( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the project file. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim wszText as DWSTRING = gApp.ProjectFilename

    dim wszCurDrive as DWSTRING = PsLCase(PsLeft(wszText, 3))  ' eg. D:\

    ' if the incoming filename is a relative file name then the following test
    ' will have no effect.
    if PsLCase(PsLeft(wszFilename, 3)) = wszCurDrive then 
        wszFilename = wstr("{CURDRIVE}") & PsMid(wszFilename, 2)
    end if

    return wszFilename
end function


' ========================================================================================
' Process current drive to prefix {CURDRIVE} 
' ========================================================================================
function ProcessFromCurdriveProject( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the project file. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim wszText as DWSTRING = gApp.ProjectFilename
    
    if PsUCase(PsLeft(wszFilename, 10)) = wstr("{CURDRIVE}") then 
        wszFilename = PsLeft(wszText, 1) & PsMid(wszFilename, 11)
    end if

    return wszFilename
end function
