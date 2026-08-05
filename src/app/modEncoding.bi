' #####################################################################################
' Declarations for modEncoding.inc -- byte conversion and the shared disk writer.
' Split out of modRoutines.bi, which forwards to this so no call site changed.
' #####################################################################################

#pragma once

declare function isUTF8encoded(byref s as string) as boolean
declare function Doc_EncodeForDisk( byref sBuffer as string, byval bSourceIsUtf8 as boolean, byval nTargetEnc as long, byref bLossy as boolean ) as string

' Outcome of Doc_WriteToDisk. Deliberately an explicit pair rather than a boolean: the
' three save paths that call it used to return a bare TRUE/FALSE that conflated "the user
' cancelled" with "the write succeeded", and had no value at all for "the write FAILED" --
' which is exactly why a failed save used to be indistinguishable from a good one.
const DOCWRITE_OK     = 0
const DOCWRITE_FAILED = 1
' Test hook for Doc_ConfirmLossySave: 0 = ask the user (always, outside a self-test),
' >0 = answer OK, <0 = answer Cancel. See that function's header for why it exists.
dim shared gLossySaveTestAnswer as long
declare function Doc_WriteToDisk( byval wszPath as DWSTRING, byref sBytes as string, byref wszErr as DWSTRING ) as long
' Doc_ConfirmLossySave and Doc_ReportWriteFailure are declared in
' modEncodingUi.bi now -- they put a window in front of the user, and this file
' is in app\, which may not know what a window is.

declare function Doc_EncodingName( byval nEnc as long ) as DWSTRING
declare function ConvertTextBuffer( byval pDoc as clsDocument ptr, byval nNewEncoding as long ) as boolean
