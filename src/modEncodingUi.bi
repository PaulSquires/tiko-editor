' Copyright (C) 2026 Paul Squires, PlanetSquires Software
'
' This Source Code Form is subject to the terms of the Mozilla Public
' License, v. 2.0. If a copy of the MPL was not distributed with this
' file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' modEncodingUi.bi -- declarations for the encoding dialogs.
'
' Separate from the .inc because modEncoding.inc CALLS one of these and is included first:
' the conversion logic decides whether to ask, the dialog decides what asking looks like,
' and the caller therefore needs the name before the body exists.
'
' That ordering is the point rather than an inconvenience. The dependency now runs one way
' -- logic names a question, UI answers it -- so the logic can be reasoned about, and
' eventually tested, without a screen.
' ========================================================================================

#pragma once

declare function Doc_ConfirmLossySave( _
            byval pDoc as clsDocument ptr, _
            byval wszPath as DWSTRING, _
            byval nEncoding as long _
            ) as boolean

declare sub Doc_ReportWriteFailure( byval wszPath as DWSTRING, byval wszErr as DWSTRING )

' Doc_ConfirmAnsiConversion was declared here until 7c step 9. Switching encodings no
' longer converts anything, so there is nothing to confirm at the click -- see the
' note where its body was, in modEncodingUi.inc.
