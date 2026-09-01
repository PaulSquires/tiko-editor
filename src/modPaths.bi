' #####################################################################################
' Declarations for modPaths.inc -- {CURDRIVE} placeholder conversion and filename case.
' Split out of modRoutines.bi, which forwards to this so no call site changed.
' #####################################################################################

#pragma once

declare function FilenameOriginalCase( byval wszFilename as DWSTRING ) as DWSTRING
' A document filename is a KEY compared by plain string equality, so one separator only.
' See the implementation for the project-file case that made the Functions panel empty.
declare function PathNormalizeSlashes( byval wszPath as DWSTRING ) as DWSTRING
declare function ProcessToCurdriveProject( byval wzFilename as DWSTRING ) as DWSTRING
declare function ProcessFromCurdriveProject( byval wzFilename as DWSTRING ) as DWSTRING
declare function ProcessToCurdriveApp( byval wzFilename as DWSTRING ) as DWSTRING
declare function ProcessFromCurdriveApp( byval wzFilename as DWSTRING ) as DWSTRING
