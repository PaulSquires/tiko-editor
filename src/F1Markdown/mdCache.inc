'' ========================================================================================
'' mdCache.inc -- the implementation. See mdCache.bi for where it lives and why.
''
'' THE FORMAT IS chr(1)-SEPARATED FIELDS IN chr(2)-SEPARATED RECORDS, and that choice is the
'' whole of the escaping story: neither byte can occur in markdown heading text, a file name
'' or a path, so nothing needs quoting and nothing can be broken by a tab or a quote inside a
'' title. A tab-separated format would have needed escaping for exactly one heading somewhere
'' in the corpus, and would have been found by that heading rendering as two.
'' ========================================================================================

#pragma once
#include once "mdCache.bi"

private const MDC_MAGIC = "F1MDCACHE"
private const MDC_VER   = 2

private function FSep() as string
    return chr(1)
end function
private function RSep() as string
    return chr(2)
end function

'' FNV-1a over the lowercased canonical path. Only needs to avoid collisions between the
'' handful of roots one user configures, and the stamp catches a collision anyway -- the
'' wrong cache would fail its check and be rebuilt.
private function HashPath( byref sPath as DWSTRING ) as string
    dim as string s = PsPathCanonicalise( sPath ).Utf8
    for i as long = 0 to len(s) - 1
        if (s[i] >= 65) andalso (s[i] <= 90) then s[i] += 32
        if s[i] = 92 then s[i] = 47                    '' \ and / are the same root
    next
    dim as ulongint h = 14695981039346656000ull
    for i as long = 0 to len(s) - 1
        h = h xor cast(ulongint, s[i])
        h *= 1099511628211ull
    next
    return hex(h, 16)
end function

function MdCachePath( byref sRoot as DWSTRING ) as DWSTRING
    dim as DWSTRING sDir = PsPathJoin( PsKnownFolder(PSFOLDER_CACHE), PsText("F1Markdown") )
    dim as DWSTRING sName
    sName.Utf8 = HashPath(sRoot) & ".f1x"
    return PsPathJoin( sDir, sName )
end function

sub MdCacheForget( byref sRoot as DWSTRING )
    PsFileDelete( MdCachePath(sRoot) )
end sub


'' ---------------------------------------------------------------------------------------
'' the stamp -- a stat-only walk
'' ---------------------------------------------------------------------------------------
private sub StampDir( byref sDir as DWSTRING, byval nDepth as long, byref nDocs as long, _
                      byref nBytes as longint, byref nNewest as longint )
    if nDepth > MDX_MAX_DEPTH then exit sub

    '' COLLECTED BEFORE RECURSING, for the same reason mdIndex does it: Dir() has one global
    '' iteration state and a nested walk silently restarts the outer one.
    dim as DWSTRING names(any)
    dim as long n = PsDirList( sDir, names(), true )
    if n <= 0 then exit sub

    '' The subdirectories are copied out first, because the recursion below must not hold a
    '' reference into an array anything else might touch.
    dim as DWSTRING subs(any)
    dim as long nSub = 0

    for i as long = 0 to n - 1
        dim as DWSTRING sFull = PsPathJoin( sDir, names(i) )
        dim as string sNm = names(i).Utf8
        if PsFileIsDir( sFull ) then
            if (len(sNm) > 0) andalso (sNm[0] <> 46) then
                if nSub > ubound(subs) then redim preserve subs(0 to (nSub + 1) * 2)
                subs(nSub) = sFull
                nSub += 1
            end if
        else
            if lcase( PsPathExt(names(i)).Utf8 ) = ".md" then
                nDocs += 1
                dim as longint nSz = PsFileSize( sFull )
                if nSz > 0 then nBytes += nSz
                dim as longint nT = PsFileTimeToUnix( PsFileWriteTime(sFull) )
                if nT > nNewest then nNewest = nT
            end if
        end if
    next

    for i as long = 0 to nSub - 1
        StampDir( subs(i), nDepth + 1, nDocs, nBytes, nNewest )
    next
end sub

function MdCacheStamp( byref sRoot as DWSTRING, byref nDocs as long, _
                       byref nBytes as longint, byref nNewest as longint ) as boolean
    nDocs = 0 : nBytes = 0 : nNewest = 0
    if PsFileIsDir( sRoot ) = false then return false
    StampDir( sRoot, 1, nDocs, nBytes, nNewest )
    return true
end function


'' ---------------------------------------------------------------------------------------
'' writing
'' ---------------------------------------------------------------------------------------
function MdCacheSave( byref ix as MdIndex, byref sRoot as DWSTRING, _
                      byval nFirstTopic as long, byval nFirstHead as long ) as boolean
    dim as long nDocs
    dim as longint nBytes, nNewest
    if MdCacheStamp( sRoot, nDocs, nBytes, nNewest ) = false then return false

    dim as string F = FSep(), R = RSep()
    dim as string s = MDC_MAGIC & F & str(MDC_VER) & F & str(nDocs) & F & _
                      str(nBytes) & F & str(nNewest) & F & _
                      str(ix.nTopic - nFirstTopic) & F & str(ix.nHead - nFirstHead) & R

    '' Parent indices are stored RELATIVE to the root's first topic, so a cache written when
    '' this was the only docset still loads correctly behind another one.
    for i as long = nFirstTopic to ix.nTopic - 1
        dim as long nPar = ix.topic(i).nParent
        if nPar >= nFirstTopic then nPar -= nFirstTopic else nPar = -1
        s &= "T" & F & str(ix.topic(i).kind) & F & str(nPar) & F & _
             str(ix.topic(i).nDepth) & F & str(ix.topic(i).nSort) & F & _
             ix.topic(i).sName & F & ix.topic(i).sTitle & F & ix.topic(i).sPath.Utf8 & R
    next
    for h as long = nFirstHead to ix.nHead - 1
        dim as long nT = ix.head(h).nTopic - nFirstTopic
        s &= "H" & F & str(nT) & F & str(ix.head(h).nLevel) & F & _
             str(ix.head(h).nBlock) & F & ix.head(h).sAnchor & F & ix.head(h).sText & R
    next

    dim as DWSTRING sPath = MdCachePath( sRoot )
    PsDirCreateTree( PsPathDir(sPath) )

    '' Written to a neighbour and moved into place. A half-written cache killed mid-write
    '' would otherwise be loaded on the next run, and its stamp -- which is at the TOP of the
    '' file -- would match perfectly.
    dim as DWSTRING sTmp
    sTmp.Utf8 = sPath.Utf8 & ".tmp"
    if PsFileWriteAll( sTmp, s ) = false then return false
    if PsFileReplace( sTmp, sPath ) = false then
        PsFileDelete( sTmp )
        return false
    end if
    return true
end function


'' ---------------------------------------------------------------------------------------
'' reading
'' ---------------------------------------------------------------------------------------
function MdCacheLoad( byref ix as MdIndex, byref sLabel as string, _
                      byref sRoot as DWSTRING ) as boolean
    dim as DWSTRING sPath = MdCachePath( sRoot )
    if PsFileExists( sPath ) = false then return false
    dim as boolean bOk = false
    dim as string s = PsFileReadAll( sPath, bOk )
    if (bOk = false) orelse (len(s) = 0) then return false

    dim as long F = 1, R = 2

    '' ---- the header, and every one of these checks rescans rather than guessing ---------
    dim as long nEnd = instr(s, chr(R))
    if nEnd = 0 then return false
    dim as string fld(0 to 7)
    dim as long nF = 0
    dim as long a = 1
    for i as long = 1 to nEnd
        if (i = nEnd) orelse (s[i - 1] = F) then
            if nF <= 7 then fld(nF) = mid(s, a, i - a)
            nF += 1
            a = i + 1
        end if
    next
    if nF <> 7 then return false
    if fld(0) <> MDC_MAGIC then return false
    if valint(fld(1)) <> MDC_VER then return false

    dim as long nDocs
    dim as longint nBytes, nNewest
    if MdCacheStamp( sRoot, nDocs, nBytes, nNewest ) = false then return false
    if nDocs <> valint(fld(2)) then return false
    if nBytes <> vallng(fld(3)) then return false
    if nNewest <> vallng(fld(4)) then return false

    dim as long nTopics = valint(fld(5))
    dim as long nHeads  = valint(fld(6))
    if (nTopics < 1) orelse (nHeads < 0) then return false

    '' ---- the records --------------------------------------------------------------------
    '' Reserved up front: this is the whole reason the counts are in the header, and it turns
    '' 14,000 doubling reallocations into two.
    dim as long nBase = ix.nTopic
    dim as long nHBase = ix.nHead
    if (nBase + nTopics) > ubound(ix.topic) then redim preserve ix.topic(0 to nBase + nTopics)
    if (nHBase + nHeads) > ubound(ix.head)  then redim preserve ix.head(0 to nHBase + nHeads)

    dim as long nT = 0, nH = 0
    dim as string g(0 to 7)
    dim as long nG = 0
    dim as long nStart = nEnd + 1
    dim as long nFieldAt = nStart

    for i as long = nEnd + 1 to len(s)
        dim as long c = s[i - 1]
        if (c = F) orelse (c = R) then
            if nG <= 7 then g(nG) = mid(s, nFieldAt, i - nFieldAt)
            nG += 1
            nFieldAt = i + 1
            if c = R then
                if (nG = 8) andalso (g(0) = "T") then
                    dim as long n = nBase + nT
                    if nT < nTopics then
                        with ix.topic(n)
                            .kind    = valint(g(1))
                            .nParent = valint(g(2))
                            if .nParent >= 0 then .nParent += nBase
                            .nDepth  = valint(g(3))
                            .nSort   = valint(g(4))
                            .sName   = g(5)
                            .sTitle  = g(6)
                            .sPath.Utf8 = g(7)
                            .nRow    = -1
                        end with
                        if ix.topic(n).kind = MDX_DOC then ix.nDocs += 1
                        nT += 1
                    end if
                elseif (nG = 6) andalso (g(0) = "H") then
                    dim as long n = nHBase + nH
                    if nH < nHeads then
                        with ix.head(n)
                            .nTopic  = nHBase * 0 + nBase + valint(g(1))
                            .nLevel  = valint(g(2))
                            .nBlock  = valint(g(3))
                            .sAnchor = g(4)
                            .sText   = g(5)
                            .nY      = -1
                        end with
                        nH += 1
                    end if
                end if
                nG = 0
            end if
        end if
    next

    if (nT <> nTopics) orelse (nH <> nHeads) then
        '' Truncated or corrupt. Roll the whole root back rather than leaving half of it in
        '' the index: a docset missing its second half looks like missing documentation.
        ix.nTopic = nBase
        ix.nHead  = nHBase
        ix.nDocs  = 0
        for i as long = 0 to nBase - 1
            if ix.topic(i).kind = MDX_DOC then ix.nDocs += 1
        next
        return false
    end if

    ix.nTopic = nBase + nT
    ix.nHead  = nHBase + nH

    '' The label is the CALLER's, not the cache's -- a root renamed in f1markdown.ini must not
    '' need the cache thrown away.
    ix.topic(nBase).sName  = sLabel
    ix.topic(nBase).sTitle = sLabel
    return true
end function
