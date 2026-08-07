'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' See modDocViews.bi for why this exists. It is finished and permanent -- the
'' "step 2" an older version of that header promised is moot.

#pragma once

'' The body is deliberately trivial: it was introduced as a pure forward so that
'' converting 142 call sites to it could not change behaviour, and any movement
'' in the oracle was a conversion mistake rather than a design difference.
''
'' It does two things now and both are permanent. It is the ONE PLACE the
'' document's portable `any ptr` storage becomes a shell-side HWND. And the
'' null/bounds test is the ONE guard the 142 sites rely on -- it only ever turns
'' a fault into a zero, which callers routinely then test with IsWindow.
'' clsDocument's array is hWindow(1), so 0 and 1 are the whole range.
function DocView( byval pDoc as clsDocument ptr, byval idx as long ) as HWND
    if pDoc = 0 then return 0
    if (idx < lbound(pDoc->hWindow)) orelse (idx > ubound(pDoc->hWindow)) then return 0
    return pDoc->hWindow(idx)
end function
