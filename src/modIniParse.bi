' ########################################################################################
' modIniParse.bi
'
' ONE definition of what a line in tiko's key=value files means.
'
' settings.ini and .tiko share a format, and three separate loops had grown their own copy
' of the preamble that decodes it: clsConfig.LoadConfigFile, clsConfig.ProjectLoadFromFile
' and ProjectTrust_ScanFile. THEY HAD ALREADY DRIFTED -- the third skipped neither comment
' lines nor section headers, so a commented-out "'ProjectOther64=..." was handed to its
' switch as a key. Harmless there by luck, since no case matched; the point is that nothing
' would have caught it, and the next copy might not be lucky.
'
' Deliberately a CLASSIFIER, not a callback-driven walker. A walker would have to own the
' loop, and each of the three does something different inside it -- one fills a config
' object, one builds documents, one extracts three values -- so the callback would need a
' context pointer and the control flow would move away from the code that owns it. This
' shape leaves each loop looking exactly as it did and replaces only the part that was
' identical.
'
' ORDER MATTERS AT THE CALL SITE and is not captured here: a line inside a NOTES block is
' free text and must be consumed as such BEFORE its key=value split is considered, or a
' note containing an "=" is silently read as configuration. The classifier reports what a
' line looks like; the caller still decides, because only the caller knows whether it is
' currently inside a notes block.
' ########################################################################################

#pragma once

enum
    INI_LINE_SKIP = 0       ' blank, a ' comment, or a [section] header
    INI_LINE_NOTES_START    ' NOTES-START marker
    INI_LINE_NOTES_END      ' NOTES-END marker
    INI_LINE_KEYVALUE       ' key=value; wszKey and wszValue are filled
    INI_LINE_OTHER          ' text with no "=" -- only meaningful inside a notes block
end enum

' wszKey/wszValue are cleared for every kind except INI_LINE_KEYVALUE.
declare function Ini_ClassifyLine( byval wszLine as DWSTRING, _
                                   byref wszKey as DWSTRING, _
                                   byref wszValue as DWSTRING ) as long

declare sub Ini_RunSelfTest()
