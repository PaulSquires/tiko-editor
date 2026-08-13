' Copyright (C) 2026 Paul Squires, PlanetSquires Software
'
' This Source Code Form is subject to the terms of the Mozilla Public
' License, v. 2.0. If a copy of the MPL was not distributed with this
' file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' modEncodingUi -- the encoding questions that need a screen.
'
' Split out of modEncoding.inc so the conversion logic and the dialogs stop travelling
' together. Everything here puts a window in front of the user: two message boxes and the
' ANSI-conversion confirmation. Everything left behind converts bytes.
'
' WHY THE SPLIT IS WORTH A FILE. app/ is the layer Phase 8 carries to Linux, and its
' entry requirement is that nothing in it knows what a window is. modEncoding.inc could
' never meet that while it also owned the prompts -- and the prompts are the smallest,
' most obviously shell-shaped part of it, so they go first.
'
' THIS DOES NOT MAKE modEncoding.inc PORTABLE, and it is worth being plain about that.
' What remains still calls MultiByteToWideChar, WideCharToMultiByte, FormatMessageW,
' CreateFileW and GetLastError. Those need PsEncoding and PsFile rather than a move, and
' rewriting the save path is a bigger and riskier job than separating the dialogs from it.
' This commit makes that job possible to do on its own; it does not do it.
'
' The functions keep their names and signatures, so no call site moved.
' ========================================================================================

' ========================================================================================
' Confirm a save that cannot represent every character. Returns TRUE to go ahead.
'
' Doc_EncodeForDisk has ALWAYS reported this, through its byref bLossy, and the encoding
' self-test has always asserted that it reports it correctly. Every real save site declared
' the variable and then never looked at it:
'
'     dim as boolean bLossy
'     dim as string sOut = Doc_EncodeForDisk( ..., this.FileEncoding, bLossy )
'     pStream.Write strptr(sOut), len(sOut)      ' bLossy never examined
'
' The only consumer was the explicit "change encoding" command, which does warn. So saving
' an ANSI-labelled document holding CJK or emoji replaced those characters with "?"
' silently, permanently, on every Ctrl+S -- while the suite stayed green, because it tests
' the function that works rather than the call sites that ignore it.
'
' Answered once per document (pDoc->bLossySaveAccepted): a warning, not a nag. pDoc may be
' NULL, in which case it is asked every time.
' ========================================================================================
function Doc_ConfirmLossySave( _
            byval pDoc as clsDocument ptr, _
            byval wszPath as DWSTRING, _
            byval nEncoding as long _
            ) as boolean

    ' TEST HOOK. Zero in every normal run; the save self-test sets it to force an answer
    ' without a modal box. It exists because the defect being guarded was in the CALL SITES,
    ' not in this function -- and a suite that can only reach the function is blind to
    ' exactly that, which is how the original bug survived a green encoding suite. Same idea
    ' as PsMessageBox_LayoutForTest in the control family.
    if gLossySaveTestAnswer <> 0 then return (gLossySaveTestAnswer > 0)

    ' "pDoc <> 0", not a bare "pDoc": fbc warns 38 on mixing a pointer with a boolean in
    ' andalso, and a warning-free build is the only signal this tree has.
    if (pDoc <> 0) andalso pDoc->bLossySaveAccepted then return true

    dim as long nAnswer = TikoMsgBox( HWND_FRMMAIN, _
            L(516, "Some characters in this document cannot be represented in") & _
            " " & Doc_EncodingName( nEncoding ) & "." & vbcrlf & vbcrlf & _
            wszPath & vbcrlf & vbcrlf & _
            L(517, "Saving will replace them and cannot be undone. Change the file's " & _
                   "encoding first to keep them."), _
            L(518, "Characters will be lost"), _
            TMB_ICON_WARNING, TMB_OKCANCEL, 1 )   ' default = Cancel: this is destructive

    if nAnswer <> IDOK then return false
    if pDoc then pDoc->bLossySaveAccepted = true
    return true
end function


' ========================================================================================
' Report a failed save to the user.
'
' Split out because all three save paths must say the same thing, and because a save that
' fails silently is the defect this whole group of functions exists to remove.
' ========================================================================================
sub Doc_ReportWriteFailure( byval wszPath as DWSTRING, byval wszErr as DWSTRING )
    TikoMsgBox( HWND_FRMMAIN, _
                L(507, "The file could not be saved.") & vbcrlf & vbcrlf & _
                wszPath & vbcrlf & vbcrlf & _
                wszErr & vbcrlf & vbcrlf & _
                L(508, "The file on disk has not been changed."), _
                L(509, "Save failed"), _
                TMB_ICON_NONE, TMB_OK )
end sub

' ========================================================================================
' Doc_ConfirmAnsiConversion IS GONE (7c step 9), and so is the question it asked.
'
' It warned before switching a document to ANSI, because the switch CONVERTED the buffer
' immediately -- characters outside CP-1252 were replaced there and then, and SetText
' emptied the undo buffer so there was no way back. Its default button was Cancel for that
' reason.
'
' Switching encodings is a RELABEL now (see ConvertTextBuffer): nothing is converted and
' nothing is lost until the file is written. So the question moved to where it became true
' -- Doc_ConfirmLossySave, above, which asks at the save and leaves the document intact on
' a cancel.
'
' ONE PROMPT WHERE THERE WERE TWO, and the surviving one asks at the moment the characters
' are actually at risk. L(316) and L(317) are now unused; they are left in the .lang files
' rather than blanked, because a used id is cheaper to leave than a renumber is to get
' wrong.
' ========================================================================================
