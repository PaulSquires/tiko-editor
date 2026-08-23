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

#include once "PsImage.bi"


' ========================================================================================
' Drop the decoded bitmap. CGpBitmap's destructor releases the underlying GpImage, so a
' plain delete is the whole teardown; guarded so a double Dispose (destructor after an
' explicit reload) is harmless.
' ========================================================================================
private sub PsImage.Dispose()
    if _pBitmap then delete _pBitmap : _pBitmap = 0
end sub

destructor PsImage()
    this.Dispose()
end destructor


' ========================================================================================
' Load a raster image file.
'
' .ico is given a fast path through LoadImageW + the HICON constructor: GdipCreateBitmapFromFile
' also decodes .ico, but it grabs an arbitrary frame, whereas LoadImageW picks the frame that
' best matches the requested size. We request LR_DEFAULTSIZE (the system large-icon size), then
' let PaintImage scale it into the cell with bicubic interpolation. Any failure there falls
' straight through to the generic file decoder, so a malformed .ico is not worse off than a .png.
' ========================================================================================
function PsImage.Load( byval wszPath as DWSTRING ) as long
    this.Dispose()

    ' A local wstring buffer sidesteps any Right()/ucase() overload ambiguity on DWSTRING.
    dim as wstring * 520 wPath = wszPath
    dim as wstring * 8    wExt  = ucase( right( wPath, 4 ) )

    if wExt = wstr(".ICO") then
        dim as HANDLE hLoaded = LoadImageW( null, @wPath, IMAGE_ICON, _
                                            0, 0, LR_LOADFROMFILE or LR_DEFAULTSIZE )
        if hLoaded then
            _pBitmap = new CGpBitmap( cast( HICON, hLoaded ) )
            DestroyIcon( cast( HICON, hLoaded ) )
            if this.IsValid() then return -1
            this.Dispose()          ' HICON decoded but GDI+ wrap failed -- try the file decoder
        end if
    end if

    _pBitmap = new CGpBitmap( wszPath.sptr() )
    if this.IsValid() = 0 then
        this.Dispose()
        return 0
    end if
    function = -1
end function


' ========================================================================================
' Wrap an existing HBITMAP. GdipCreateBitmapFromHBITMAP copies the pixels, so the caller keeps
' ownership of the HBITMAP.
' ========================================================================================
function PsImage.LoadFromBitmap( byval hbm as HBITMAP ) as long
    this.Dispose()
    if hbm = 0 then return 0
    _pBitmap = new CGpBitmap( hbm, cast( HPALETTE, null ) )
    if this.IsValid() = 0 then
        this.Dispose()
        return 0
    end if
    function = -1
end function


' ========================================================================================
' Wrap an existing HICON. GdipCreateBitmapFromHICON copies, so the caller keeps ownership.
' ========================================================================================
function PsImage.LoadFromIcon( byval hIcon as HICON ) as long
    this.Dispose()
    if hIcon = 0 then return 0
    _pBitmap = new CGpBitmap( hIcon )
    if this.IsValid() = 0 then
        this.Dispose()
        return 0
    end if
    function = -1
end function


' A CGpBitmap that failed to decode still constructs, but with a NULL underlying GpImage --
' m_pImage is the reliable validity flag (m_Status is not read back here).
function PsImage.IsValid() as long
    function = (_pBitmap <> 0) andalso (_pBitmap->m_pImage <> 0)
end function

function PsImage.NaturalWidth() as long
    if this.IsValid() = 0 then return 0
    function = _pBitmap->GetWidth()
end function

function PsImage.NaturalHeight() as long
    if this.IsValid() = 0 then return 0
    function = _pBitmap->GetHeight()
end function

' CGpBitmap EXTENDS CGpImage, so the held pointer IS the CGpImage the painter wants.
function PsImage.Image() as CGpImage ptr
    if this.IsValid() = 0 then return 0
    function = cast( CGpImage ptr, _pBitmap )
end function


' ========================================================================================
' Self-test - gated on PSIMAGE_SELFTEST=1. Proves load/measure/validity without shipping any
' binary asset: it builds an HBITMAP procedurally (LoadFromBitmap / LoadFromIcon), and asserts
' that a bad path fails cleanly rather than crashing. The FILE decode path and the PaintImage
' RENDER are covered elsewhere -- the demo loads real files by eye, and PsBufferPaint's own
' self-test drives PaintImage into an offscreen surface and counts tones.
' ========================================================================================
namespace PsImageSelfTest
    dim shared as long m_run, m_fail

    sub Chk( byval bOK as boolean, byref szWhat as string )
        m_run += 1
        if bOK = false then
            m_fail += 1
            dim as string s = "PsImage SELFTEST FAIL: " & szWhat
            print s
        end if
    end sub

    ' A solid 32x24 DIB section, so GetWidth/GetHeight have known answers.
    function MakeTestBitmap( byval cx as long, byval cy as long ) as HBITMAP
        dim as BITMAPINFO bi
        bi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER )
        bi.bmiHeader.biWidth       = cx
        bi.bmiHeader.biHeight      = -cy          ' top-down
        bi.bmiHeader.biPlanes      = 1
        bi.bmiHeader.biBitCount    = 32
        bi.bmiHeader.biCompression = BI_RGB
        dim as any ptr pBits
        dim as HBITMAP hbm = CreateDIBSection( null, @bi, DIB_RGB_COLORS, @pBits, null, 0 )
        if hbm andalso pBits then
            dim as ulong ptr p = pBits
            for i as long = 0 to (cx * cy) - 1
                p[i] = &hFF3366CC          ' opaque ARGB, arbitrary
            next
        end if
        function = hbm
    end function
end namespace

sub PsImage_RunSelfTest()
    using PsImageSelfTest
    m_run = 0 : m_fail = 0

    ' --- LoadFromBitmap: valid handle, known size ---------------------------------------
    scope
        dim as HBITMAP hbm = MakeTestBitmap( 32, 24 )
        Chk( hbm <> 0, "test HBITMAP created" )
        dim as PsImage img
        Chk( img.LoadFromBitmap( hbm ) <> 0, "LoadFromBitmap succeeds" )
        Chk( img.IsValid() <> 0,             "IsValid after LoadFromBitmap" )
        Chk( img.NaturalWidth()  = 32,       "NaturalWidth = 32" )
        Chk( img.NaturalHeight() = 24,       "NaturalHeight = 24" )
        Chk( img.Image() <> 0,               "Image() non-null when valid" )
        if hbm then DeleteObject( hbm )      ' PsImage took a copy; we still own hbm
    end scope

    ' --- a null HBITMAP is rejected, not crashed ----------------------------------------
    scope
        dim as PsImage img
        Chk( img.LoadFromBitmap( cast( HBITMAP, null ) ) = 0, "null HBITMAP rejected" )
        Chk( img.IsValid() = 0,        "IsValid false after rejected load" )
        Chk( img.Image()   = 0,        "Image() null when not valid" )
        Chk( img.NaturalWidth() = 0,   "NaturalWidth 0 when not valid" )
    end scope

    ' --- a bad file path fails cleanly --------------------------------------------------
    scope
        dim as PsImage img
        Chk( img.Load( "Z:\__psimage_does_not_exist__.png" ) = 0, "bad path fails" )
        Chk( img.IsValid() = 0,        "IsValid false after bad path" )
    end scope

    ' --- reload replaces cleanly (no leak of the first) ---------------------------------
    scope
        dim as HBITMAP hbm1 = MakeTestBitmap( 16, 16 )
        dim as HBITMAP hbm2 = MakeTestBitmap( 48, 48 )
        dim as PsImage img
        img.LoadFromBitmap( hbm1 )
        Chk( img.NaturalWidth() = 16, "first load 16" )
        img.LoadFromBitmap( hbm2 )
        Chk( img.NaturalWidth() = 48, "reload replaces -> 48" )
        img.Load( "Z:\__still_missing__.png" )
        Chk( img.IsValid() = 0,       "failed reload clears previous image" )
        if hbm1 then DeleteObject( hbm1 )
        if hbm2 then DeleteObject( hbm2 )
    end scope

    ' --- FILE round trip: save a known bitmap out, Load it back. This is the only path that
    '     actually exercises GdipCreateBitmapFromFile; the rest go through HBITMAP. ---------
    scope
        dim as HBITMAP hbm = MakeTestBitmap( 40, 24 )
        dim as PsImage src
        src.LoadFromBitmap( hbm )
        if src.IsValid() then
            AfxGdipSaveImageToPng( src.Image()->m_pImage, "__psimage_st.png" )
            AfxGdipSaveImageToBmp( src.Image()->m_pImage, "__psimage_st.bmp" )
        end if
        if hbm then DeleteObject( hbm )

        ' Inner scope so the reloaded PsImages are DESTRUCTED before the DeleteFileW below:
        ' GdipCreateBitmapFromFile keeps the file locked for the bitmap's lifetime, so a delete
        ' while they are still alive silently fails and leaves the temp files behind.
        scope
            dim as PsImage png
            Chk( png.Load( "__psimage_st.png" ) <> 0, "Load .png from disk succeeds" )
            Chk( png.IsValid() <> 0,        "reloaded .png is valid" )
            Chk( png.NaturalWidth()  = 40,  "reloaded .png width 40" )
            Chk( png.NaturalHeight() = 24,  "reloaded .png height 24" )

            dim as PsImage bmp
            Chk( bmp.Load( "__psimage_st.bmp" ) <> 0, "Load .bmp from disk succeeds" )
            Chk( bmp.NaturalWidth()  = 40,  "reloaded .bmp width 40" )
        end scope

        DeleteFileW( "__psimage_st.png" )
        DeleteFileW( "__psimage_st.bmp" )
    end scope

    dim as string s = "PsImage self-test: " & str(m_run) & " assertions run, " & str(m_fail) & " failed."
    print s
end sub
