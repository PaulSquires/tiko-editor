'' ========================================================================================
'' mdCache -- the corpus index, remembered between runs.
''
'' ---- WHAT THIS IS FOR, MEASURED -------------------------------------------------------
''
'' Scanning the AfxNova docset costs 188 ms, and only 15 ms of that is file I/O. The other
'' 173 ms re-derives titles and headings that the previous run already computed. This stores
'' them and reads them back instead.
''
'' ---- IT IS A CACHE, AND THAT WORD IS LOAD-BEARING -------------------------------------
''
'' The .md tree is the ONLY source of truth. This file is never shipped, never authoritative,
'' and deleting it must only ever cost time. The worst thing a help viewer can do is serve
'' documentation that is quietly out of date, so every path here is arranged so that when in
'' doubt it rescans.
''
'' ---- WHERE IT LIVES, AND WHY NOT BESIDE THE DOCS --------------------------------------
''
'' PSFOLDER_CACHE -- %LOCALAPPDATA% or $XDG_CACHE_HOME -- and one file per root, named from a
'' hash of the canonical root path. A docset can sit somewhere unwritable (Program Files, a
'' network share, a read-only mount), and a viewer that fails to start because it cannot
'' write next to its own content is worse than a slow one. PSFOLDER_CACHE is also the folder
'' the platform DOCUMENTS as discardable, which is exactly what this is.
''
'' ---- THE STAMP ------------------------------------------------------------------------
''
'' Document count, total bytes and newest write time across the tree. Checking it is a
'' stat-only walk: cheap, because the expense was never the directory enumeration, it was the
'' reading and the parsing. A mismatch rescans and rewrites without a word.
''
'' IT CAN MISS AN EDIT that changes neither the size nor the write time. That is rare enough
'' to accept and cheap enough to escape: --rescan ignores the cache for one run.
'' ========================================================================================

#pragma once

'' The stamp for a root as it is ON DISK RIGHT NOW. False if the root cannot be walked.
declare function MdCacheStamp( byref sRoot as DWSTRING, byref nDocs as long, _
                               byref nBytes as longint, byref nNewest as longint ) as boolean

'' Appends a cached root's topics and headings to ix, exactly as MdIndexAddRoot would have.
'' False -- and ix untouched -- when there is no cache, it is for another version, or its
'' stamp no longer matches the tree. The caller then scans and calls MdCacheSave.
declare function MdCacheLoad( byref ix as MdIndex, byref sLabel as string, _
                              byref sRoot as DWSTRING ) as boolean

'' Writes the topics and headings from nFirstTopic onward -- i.e. what one MdIndexAddRoot
'' just appended. A failure is not an error: the next run simply scans again.
declare function MdCacheSave( byref ix as MdIndex, byref sRoot as DWSTRING, _
                              byval nFirstTopic as long, byval nFirstHead as long ) as boolean

declare function MdCachePath( byref sRoot as DWSTRING ) as DWSTRING
declare sub MdCacheForget( byref sRoot as DWSTRING )
