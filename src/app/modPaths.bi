' #####################################################################################
' Declarations for modPaths.inc -- {CURDRIVE} placeholder conversion and filename case.
' Split out of modRoutines.bi, which forwards to this so no call site changed.
' #####################################################################################

#pragma once

declare function FilenameOriginalCase( byval wszFilename as DWSTRING ) as DWSTRING
declare function ProcessToCurdriveProject( byval wzFilename as DWSTRING ) as DWSTRING
declare function ProcessFromCurdriveProject( byval wzFilename as DWSTRING ) as DWSTRING
declare function ProcessToCurdriveApp( byval wzFilename as DWSTRING ) as DWSTRING
declare function ProcessFromCurdriveApp( byval wzFilename as DWSTRING ) as DWSTRING
