' Copyright (C) 2026 Paul Squires, PlanetSquires Software
'
' This Source Code Form is subject to the terms of the Mozilla Public
' License, v. 2.0. If a copy of the MPL was not distributed with this
' file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' The document encoding ids.
'
' Moved out of clsDocument.bi, which is shell-side because it holds Scintilla's HWND. These
' four are pure data and the app layer needs them: clsConfig.bi could not compile against
' PsCore alone without FILE_ENCODING_UTF8, and following that dependency into clsDocument.bi
' would have dragged the whole document model across with it.
'
' They map one for one onto PsCore's PsEncodingId -- see Doc_EncToPs in modEncoding.inc.
' The two lists were written years and a project apart and agree, which is worth keeping
' true: if one gains a member, so must the other.
' ========================================================================================

#pragma once

#define FILE_ENCODING_ANSI         0
#define FILE_ENCODING_UTF8         1
#define FILE_ENCODING_UTF8_BOM     2
#define FILE_ENCODING_UTF16_BOM    3
