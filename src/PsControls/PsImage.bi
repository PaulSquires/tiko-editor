'    PsImage - reusable image loader for the Ps* owner-drawn control family
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#pragma once

' ========================================================================================
' PsImage - loads and OWNS one raster image, so a control can show a real .ico/.png/.bmp/.jpg
' (or an existing HBITMAP/HICON) in an icon slot INSTEAD OF a Segoe Fluent Icons glyph.
'
' It is a support class in the spirit of PsBufferPaint, NOT a control: a plain TYPE a control
' holds, with no HWND and no window of its own. It does exactly three things -- LOAD (decode a
' file once, at natural size), MEASURE (report natural width/height), and hand out the decoded
' CGpImage ptr. It does NOT draw: the draw is PsBufferPaint.PaintImage, which owns the buffer's
' GDI+ graphics and must not have a second one built on the same HDC beside it. Load here, draw
' there.
'
' HOST OBLIGATIONS -- both inherited from CGdiPlus.inc, neither new:
'   1. GDI+ must be initialised (AfxGdipInit / AfxGdipShutdown bracketing the message loop)
'      before the first Load, because decoding builds GDI+ objects. A control's WM_PAINT already
'      runs inside that bracket, so a host that adopted PsBufferPaint has nothing to add.
'   2. Never name an identifier `ok`. This header pulls in GDI+'s Status enum, whose Ok = 0
'      lands in namespace AfxNova, and every host says `using AfxNova`. The family convention
'      is bOK. (Same rule PsBufferPaint states; repeated here because this file is adopted on
'      its own.)
'
' Included HERE (not left to the call site) on purpose, so the header is self-contained: a
' consumer's own .bi does `#include once "PsImage.bi"` and names PsImage without pre-loading
' CGdiPlus itself. PsImage.inc pulls in the implementation once at the host translation unit.
' ========================================================================================

#include once "AfxNova\CGdiPlus.inc"
using AfxNova

type PsImage
    private:
        ' The decoded bitmap. NULL until a successful Load*; a failed load leaves it NULL and
        ' IsValid false. CGpBitmap EXTENDS CGpImage, so it doubles as the CGpImage ptr the
        ' painter wants -- no separate handle is kept.
        _pBitmap  as CGpBitmap ptr

        declare sub Dispose()

    public:

    declare destructor()

    ' Load a raster image FILE. Recognises .ico / .png / .bmp / .jpg / .jpeg / .gif / .tif(f)
    ' by whatever GDI+'s decoders accept -- the extension only steers the .ico fast path below.
    ' Decodes ONCE, at the image's natural size; scaling happens later at draw time. Returns
    ' non-zero on success, 0 on failure (bad path, unsupported/corrupt file). A failed Load
    ' clears any image previously held, so IsValid is the single source of truth afterwards.
    declare function Load( byval wszPath as DWSTRING ) as long

    ' Wrap an existing HBITMAP (e.g. one loaded from a resource, or built procedurally). The
    ' PsImage takes a GDI+ COPY -- the caller keeps ownership of the HBITMAP and must still
    ' free it. Returns non-zero on success.
    declare function LoadFromBitmap( byval hbm as HBITMAP ) as long

    ' Wrap an existing HICON. As above, a GDI+ copy is taken; the caller still owns (and must
    ' DestroyIcon) the HICON. Returns non-zero on success.
    declare function LoadFromIcon( byval hIcon as HICON ) as long

    ' True once a Load* has succeeded and nothing has failed since.
    declare function IsValid() as long

    ' Natural (decoded) pixel size, or 0 when not valid. Handed to the painter to preserve
    ' aspect ratio when fitting into an icon cell.
    declare function NaturalWidth() as long
    declare function NaturalHeight() as long

    ' The decoded image, for PsBufferPaint.PaintImage. NULL when not valid. The PsImage retains
    ' ownership -- do NOT delete the returned pointer.
    declare function Image() as CGpImage ptr

end type
