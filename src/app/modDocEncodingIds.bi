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
' UTF-16 LITTLE endian, which is what a bare "UTF-16 BOM" has always meant here.
#define FILE_ENCODING_UTF16_BOM    3

' ---- BIG ENDIAN, ADDED IN 7c STEP 10 --------------------------------------------------
' APPENDED, AND THE VALUE MATTERS. Several loops and clsConfig's range check walk
' ANSI..the-last-id and rely on index == value, so a new id goes on the END and nowhere
' else -- the same rule the menu ids carry, for a different reason.
'
' IT WAS MISSING BECAUSE OF A COMMENT. PsEncoding.bi described PSENC_UTF16BE_BOM as
' "decoded, never written", so step 9 concluded that an id for it "would be a label no save
' could honour" and collapsed big-endian onto the little-endian id -- meaning tiko read such
' a file correctly and silently rewrote it in the other byte order. PsEncEncode has written
' big-endian all along; nobody read it.
#define FILE_ENCODING_UTF16BE_BOM  4
