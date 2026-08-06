'' Copyright (C) 2026 Paul Squires, PlanetSquires Software
''
'' This Source Code Form is subject to the terms of the Mozilla Public
'' License, v. 2.0. If a copy of the MPL was not distributed with this
'' file, You can obtain one at https://mozilla.org/MPL/2.0/.

'' See modDocViews.bi for why this exists and what step 2 does to it.

#pragma once

'' STEP 1: A PURE FORWARD. The body is deliberately trivial so that converting
'' 142 call sites to it cannot change behaviour -- any movement in the oracle
'' after this commit is a conversion mistake, not a design difference.
''
'' The bounds test is the ONE thing it adds, and it only ever turns a fault into
'' a zero. clsDocument's array is hWindow(1), so 0 and 1 are the whole range.
function DocView( byval pDoc as clsDocument ptr, byval idx as long ) as HWND
    if pDoc = 0 then return 0
    if (idx < lbound(pDoc->hWindow)) orelse (idx > ubound(pDoc->hWindow)) then return 0
    return pDoc->hWindow(idx)
end function
