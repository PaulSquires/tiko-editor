'' ========================================================================================
'' mdIndex.inc -- the implementation. See mdIndex.bi for the ordering and ranking rules.
''
'' ONE RULE THAT IS NOT OBVIOUS: PsDirList's whole listing is COLLECTED BEFORE RECURSING.
'' fbc's Dir() keeps ONE GLOBAL ITERATION STATE, so calling anything that uses Dir() inside a
'' Dir() walk silently restarts the outer one -- PsPlatform's landmine register lists it, and
'' a recursive scan is exactly the shape that hits it.
'' ========================================================================================

#pragma once
#include once "mdIndex.bi"

sub MdIndexClear( byref ix as MdIndex )
    ix.nTopic = 0 : ix.nHead = 0 : ix.nDocs = 0 : ix.nSkipped = 0
end sub

'' ---------------------------------------------------------------------------------------
'' READING A DOCUMENT, AND WHY NOT PsEncDecodeAuto ALONE.
''
'' Everything downstream wants UTF-8 BYTES. PsEncDecodeAuto returns a DWSTRING -- UTF-16 --
'' so the obvious `PsEncDecodeAuto(bytes).Utf8` transcodes the whole file to UTF-16 and
'' straight back again. For a corpus that is already UTF-8, which is all of it, that is two
'' full conversions per file to arrive at the bytes that were read. Detecting first and
'' passing them through cut the AfxNova scan from 293 ms to what it is now.
''
'' The other encodings still go the long way, because they genuinely need converting.
'' ---------------------------------------------------------------------------------------
function MdReadUtf8( byref sPath as DWSTRING, byref bOk as boolean ) as string
    dim as string sBytes = PsFileReadAll( sPath, bOk )
    if bOk = false then return ""
    dim as PsEncodingId enc = PsEncDetect( sBytes )
    if enc = PSENC_UTF8 then return sBytes
    if enc = PSENC_UTF8_BOM then return mid( sBytes, PsEncBomLength(enc) + 1 )
    dim as PsEncodingId encUsed
    return PsEncDecodeAuto( sBytes, encUsed ).Utf8
end function

private function AddTopic( byref ix as MdIndex ) as long
    if ix.nTopic > ubound(ix.topic) then
        redim preserve ix.topic(0 to (ix.nTopic + 1) * 2)
    end if
    with ix.topic(ix.nTopic)
        .kind = MDX_DOC : .sName = "" : .sTitle = ""
        .nParent = -1 : .nDepth = 0 : .nSort = -1 : .nRow = -1
    end with
    ix.nTopic += 1
    return ix.nTopic - 1
end function

private sub AddHeading( byref ix as MdIndex, byval nTopic as long, byval nLevel as long, _
                        byref sText as string, byref sAnchor as string, byval nBlock as long )
    if ix.nHead > ubound(ix.head) then
        redim preserve ix.head(0 to (ix.nHead + 1) * 2)
    end if
    with ix.head(ix.nHead)
        .nTopic = nTopic : .nLevel = nLevel : .sText = sText
        .sAnchor = sAnchor : .nY = -1 : .nBlock = nBlock
    end with
    ix.nHead += 1
end sub

private function LowerA( byref s as string ) as string
    dim as string r = s
    for i as long = 0 to len(r) - 1
        if (r[i] >= 65) andalso (r[i] <= 90) then r[i] += 32
    next
    return r
end function

'' "10-intro" -> 10 and "intro"; "intro" -> -1 and "intro". Both "-" and "_" separate, and a
'' bare number with no separator is NOT a prefix: "2024.md" is a document called 2024.
private function SplitOrder( byref sStem as string, byref sRest as string ) as long
    dim as long i = 0
    while (i < len(sStem)) andalso (sStem[i] >= 48) andalso (sStem[i] <= 57)
        i += 1
    wend
    if i = 0 then sRest = sStem : return -1
    if i >= len(sStem) then sRest = sStem : return -1
    if (sStem[i] <> 45) andalso (sStem[i] <> 95) andalso (sStem[i] <> 32) then
        sRest = sStem
        return -1
    end if
    sRest = mid(sStem, i + 2)
    return clng(val(left(sStem, i)))
end function

'' The display name for a file with no H1: the stem, minus any numeric prefix, with dashes
'' and underscores turned into spaces and the first letter raised.
private function PrettyName( byref sStem as string ) as string
    dim as string sRest
    SplitOrder( sStem, sRest )
    dim as string r = sRest
    for i as long = 0 to len(r) - 1
        if (r[i] = 45) orelse (r[i] = 95) then r[i] = 32
    next
    if (len(r) > 0) andalso (r[0] >= 97) andalso (r[0] <= 122) then r[0] -= 32
    return r
end function

'' The toc.txt override, if the folder has one. Names are matched against the FILE NAME as
'' it appears on disk, extension optional, so a list can be written without .md on every line.
private function ReadTocFile( byref sDir as DWSTRING, names() as DWSTRING ) as long
    dim as DWSTRING sToc = PsPathJoin( sDir, PsText("toc.txt") )
    if PsFileExists( sToc ) = false then return 0
    dim as boolean bOk = false
    dim as string s = MdReadUtf8( sToc, bOk )
    if bOk = false then return 0

    dim as long n = 0
    dim as long nStart = 0
    for i as long = 0 to len(s)
        if (i = len(s)) orelse (s[i] = 10) orelse (s[i] = 13) then
            dim as string sLine = mid(s, nStart + 1, i - nStart)
            '' trim, and skip blanks and # comments
            while (len(sLine) > 0) andalso ((sLine[0] = 32) orelse (sLine[0] = 9))
                sLine = mid(sLine, 2)
            wend
            while (len(sLine) > 0) andalso ((sLine[len(sLine)-1] = 32) orelse (sLine[len(sLine)-1] = 9))
                sLine = left(sLine, len(sLine) - 1)
            wend
            if (len(sLine) > 0) andalso (sLine[0] <> 35) then
                if n > ubound(names) then redim preserve names(0 to (n + 1) * 2)
                names(n).Utf8 = sLine
                n += 1
            end if
            nStart = i + 1
        end if
    next
    return n
end function

'' One document: parsed, titled and indexed.
private sub ScanFile( byref ix as MdIndex, byval nParent as long, byval nDepth as long, _
                      byref sPath as DWSTRING, byref doc as MdDoc )
    dim as boolean bOk = false
    dim as string sUtf8 = MdReadUtf8( sPath, bOk )
    if bOk = false then
        ix.nSkipped += 1
        exit sub
    end if

    '' A REAL PARSE, not a grep -- the corpus documents markdown, so it contains fenced
    '' blocks whose contents start with a "#", and only the parser knows those are not
    '' headings. HEADINGS ONLY, though: the block walk is identical, and the inline parsing
    '' of 70,000 table cells nobody is about to render is not.
    MdParse( sUtf8, doc, true )

    dim as long n = AddTopic( ix )
    dim as string sStem = PsPathStem( sPath ).Utf8
    dim as string sRest
    ix.topic(n).kind    = MDX_DOC
    ix.topic(n).sPath   = sPath
    ix.topic(n).nParent = nParent
    ix.topic(n).nDepth  = nDepth
    ix.topic(n).nSort   = SplitOrder( sStem, sRest )

    dim as string sTitle = ""
    for b as long = 0 to doc.nBlk - 1
        if doc.blk(b).kind = MDBK_HEADING then
            '' The plain text of the heading, which is the runs joined -- a heading with a
            '' `code` span in it is one string here, not three.
            dim as string sH = ""
            for k as long = doc.blk(b).nFirstInl to _
                            doc.blk(b).nFirstInl + doc.blk(b).nInlCount - 1
                sH &= doc.inl(k).sText
            next
            if (len(sTitle) = 0) andalso (doc.blk(b).nLevel = 1) then sTitle = sH
            AddHeading( ix, n, doc.blk(b).nLevel, sH, doc.blk(b).sAnchor, b )
        end if
    next
    if len(sTitle) = 0 then sTitle = PrettyName( sStem )
    ix.topic(n).sTitle = sTitle
    ix.topic(n).sName  = sTitle
    ix.nDocs += 1
end sub

'' Depth-first, folders before files at each level, both in the folder's chosen order.
private sub ScanDir( byref ix as MdIndex, byval nParent as long, byval nDepth as long, _
                     byref sDir as DWSTRING, byref doc as MdDoc )
    if nDepth > MDX_MAX_DEPTH then exit sub

    '' COLLECTED FIRST. See the file header: recursing inside a Dir() walk restarts it.
    dim as DWSTRING names(any)
    dim as long n = PsDirList( sDir, names(), true )
    if n <= 0 then exit sub

    '' Partition into folders and .md files, and record each one's sort key.
    dim as DWSTRING dirName(any), fileName(any)
    dim as long dirSort(any), fileSort(any)
    dim as long nDir = 0, nFile = 0
    for i as long = 0 to n - 1
        dim as DWSTRING sFull = PsPathJoin( sDir, names(i) )
        dim as string sNm = names(i).Utf8
        if PsFileIsDir( sFull ) then
            '' A dot-folder is .git or .vscode, never content.
            if (len(sNm) > 0) andalso (sNm[0] <> 46) then
                if nDir > ubound(dirName) then
                    redim preserve dirName(0 to (nDir + 1) * 2)
                    redim preserve dirSort(0 to (nDir + 1) * 2)
                end if
                dirName(nDir) = names(i)
                dim as string sRest
                dirSort(nDir) = SplitOrder( sNm, sRest )
                nDir += 1
            end if
        else
            if LowerA( PsPathExt(names(i)).Utf8 ) = ".md" then
                if LowerA(sNm) <> "toc.txt" then
                    if nFile > ubound(fileName) then
                        redim preserve fileName(0 to (nFile + 1) * 2)
                        redim preserve fileSort(0 to (nFile + 1) * 2)
                    end if
                    fileName(nFile) = names(i)
                    dim as string sRest
                    fileSort(nFile) = SplitOrder( PsPathStem(names(i)).Utf8, sRest )
                    nFile += 1
                end if
            end if
        end if
    next

    '' toc.txt, if present, moves the names it lists to the front IN ITS OWN ORDER. Anything
    '' it does not mention keeps the default order behind them rather than vanishing -- a
    '' half-written toc.txt must not hide documents.
    dim as DWSTRING toc(any)
    dim as long nToc = ReadTocFile( sDir, toc() )
    if nToc > 0 then
        dim as long nAt = 0
        for t as long = 0 to nToc - 1
            dim as string sWant = LowerA( toc(t).Utf8 )
            for i as long = nAt to nFile - 1
                dim as string sHave = LowerA( fileName(i).Utf8 )
                dim as string sStem = LowerA( PsPathStem(fileName(i)).Utf8 )
                if (sHave = sWant) orelse (sStem = sWant) then
                    dim as DWSTRING sTmp = fileName(nAt) : fileName(nAt) = fileName(i) : fileName(i) = sTmp
                    dim as long nT = fileSort(nAt) : fileSort(nAt) = fileSort(i) : fileSort(i) = nT
                    '' Pinned: a negative key sorts ahead of every numeric prefix, and the
                    '' position within the pinned block is the toc's own order.
                    fileSort(nAt) = -1000000 + nAt
                    nAt += 1
                    exit for
                end if
            next
        next
    end if

    '' Insertion sort, twice. The lists are a folder's worth of entries -- tens, not
    '' thousands -- and a stable simple sort keeps the tie-break (alphabetical by name)
    '' obvious rather than hidden in a comparator.
    for i as long = 1 to nDir - 1
        dim as DWSTRING sN = dirName(i)
        dim as long nK = dirSort(i)
        dim as long j = i - 1
        while j >= 0
            dim as boolean bAfter
            if (nK >= 0) andalso (dirSort(j) >= 0) then
                bAfter = (dirSort(j) > nK)
            elseif (nK >= 0) andalso (dirSort(j) < 0) then
                bAfter = false
            elseif (nK < 0) andalso (dirSort(j) >= 0) then
                bAfter = true
            else
                bAfter = (LowerA(dirName(j).Utf8) > LowerA(sN.Utf8))
            end if
            if bAfter = false then exit while
            dirName(j + 1) = dirName(j) : dirSort(j + 1) = dirSort(j)
            j -= 1
        wend
        dirName(j + 1) = sN : dirSort(j + 1) = nK
    next
    for i as long = 1 to nFile - 1
        dim as DWSTRING sN = fileName(i)
        dim as long nK = fileSort(i)
        dim as long j = i - 1
        while j >= 0
            dim as boolean bAfter
            if (nK >= 0) andalso (fileSort(j) >= 0) then
                bAfter = (fileSort(j) > nK)
            elseif (nK >= 0) andalso (fileSort(j) < 0) then
                bAfter = false
            elseif (nK < 0) andalso (fileSort(j) >= 0) then
                bAfter = true
            else
                bAfter = (LowerA(fileName(j).Utf8) > LowerA(sN.Utf8))
            end if
            if bAfter = false then exit while
            fileName(j + 1) = fileName(j) : fileSort(j + 1) = fileSort(j)
            j -= 1
        wend
        fileName(j + 1) = sN : fileSort(j + 1) = nK
    next

    '' Files before folders, so a folder's own pages read before its subsections.
    for i as long = 0 to nFile - 1
        ScanFile( ix, nParent, nDepth, PsPathJoin(sDir, fileName(i)), doc )
    next
    for i as long = 0 to nDir - 1
        dim as long nF = AddTopic( ix )
        ix.topic(nF).kind    = MDX_FOLDER
        ix.topic(nF).sPath   = PsPathJoin( sDir, dirName(i) )
        ix.topic(nF).sName   = PrettyName( dirName(i).Utf8 )
        ix.topic(nF).sTitle  = ix.topic(nF).sName
        ix.topic(nF).nParent = nParent
        ix.topic(nF).nDepth  = nDepth
        ix.topic(nF).nSort   = dirSort(i)
        '' A LOCAL COPY, AND NOT FOR TIDINESS. Passing ix.topic(nF).sPath byref into the
        '' recursion hands ScanDir a reference INTO the arena -- and the first AddTopic inside
        '' redims it, moving the storage and leaving that reference dangling. It crashes with
        '' an access violation somewhere further down the walk, nowhere near the cause. Any
        '' byref argument that points into an array the callee can grow has this bug.
        dim as DWSTRING sSub = ix.topic(nF).sPath
        ScanDir( ix, nF, nDepth + 1, sSub, doc )
    next
end sub

function MdIndexAddRoot( byref ix as MdIndex, byref sLabel as string, _
                         byref sRoot as DWSTRING ) as long
    if PsFileIsDir( sRoot ) = false then return 0
    dim as long nWas = ix.nDocs

    dim as long n = AddTopic( ix )
    ix.topic(n).kind    = MDX_DOCSET
    ix.topic(n).sPath   = sRoot
    ix.topic(n).sName   = sLabel
    ix.topic(n).sTitle  = sLabel
    ix.topic(n).nParent = -1
    ix.topic(n).nDepth  = 0

    '' ONE MdDoc, REUSED for every file in the docset. MdDocClear keeps the arenas' capacity
    '' and resets the counts, so a 180-file scan allocates like the largest single file
    '' rather than like the sum of them.
    dim as MdDoc doc
    ScanDir( ix, n, 1, sRoot, doc )
    return ix.nDocs - nWas
end function

function MdIndexFindPath( byref ix as MdIndex, byref sPath as DWSTRING ) as long
    dim as string sWant = LowerA( PsPathCanonicalise(sPath).Utf8 )
    for i as long = 0 to ix.nTopic - 1
        if ix.topic(i).kind = MDX_DOC then
            if LowerA( PsPathCanonicalise(ix.topic(i).sPath).Utf8 ) = sWant then return i
        end if
    next
    return -1
end function


'' ---------------------------------------------------------------------------------------
'' ranked search
'' ---------------------------------------------------------------------------------------
'' The bands, spread far enough apart that a tie-break inside one can never cross into
'' another. A page whose TITLE is the symbol always beats one that merely has a heading of
'' that name, which is what makes F1 land on the right page.
private const SC_TITLE_EXACT  = 10000
private const SC_HEAD_EXACT   =  8000
private const SC_TITLE_PREFIX =  6000
private const SC_HEAD_PREFIX  =  4000
private const SC_TITLE_SUB    =  2000
private const SC_HEAD_SUB     =  1000

private sub PushHit( hits() as MdHit, byref nHits as long, byval nMax as long, _
                     byval nTopic as long, byval nHead as long, byval nScore as long )
    '' One entry per TOPIC, keeping its best score: ten headings matching in one page must
    '' not crowd out nine other pages.
    for i as long = 0 to nHits - 1
        if hits(i).nTopic = nTopic then
            if nScore > hits(i).nScore then
                hits(i).nScore = nScore
                hits(i).nHead  = nHead
            end if
            exit sub
        end if
    next
    if nHits >= nMax then exit sub
    hits(nHits).nTopic = nTopic
    hits(nHits).nHead  = nHead
    hits(nHits).nScore = nScore
    nHits += 1
end sub

function MdIndexSearch( byref ix as MdIndex, byref sQuery as string, _
                        hits() as MdHit, byval nMax as long ) as long
    dim as long nHits = 0
    dim as string q = LowerA( trim(sQuery) )
    if len(q) = 0 then return 0
    if nMax <= 0 then return 0
    if ubound(hits) < nMax - 1 then redim hits(0 to nMax - 1)

    for i as long = 0 to ix.nTopic - 1
        if ix.topic(i).kind <> MDX_DOC then continue for
        dim as string t = LowerA( ix.topic(i).sTitle )
        if t = q then
            PushHit( hits(), nHits, nMax, i, -1, SC_TITLE_EXACT )
        elseif left(t, len(q)) = q then
            '' Shorter titles rank higher within the band: searching "Left" should offer
            '' "Left" before "LeftJustify", and the length is the only signal available.
            PushHit( hits(), nHits, nMax, i, -1, SC_TITLE_PREFIX + 500 - len(t) )
        elseif instr(t, q) > 0 then
            PushHit( hits(), nHits, nMax, i, -1, SC_TITLE_SUB + 500 - len(t) )
        end if
    next

    for h as long = 0 to ix.nHead - 1
        dim as string t = LowerA( ix.head(h).sText )
        dim as long nTopic = ix.head(h).nTopic
        if t = q then
            PushHit( hits(), nHits, nMax, nTopic, h, SC_HEAD_EXACT )
        elseif left(t, len(q)) = q then
            PushHit( hits(), nHits, nMax, nTopic, h, SC_HEAD_PREFIX + 500 - len(t) )
        elseif instr(t, q) > 0 then
            PushHit( hits(), nHits, nMax, nTopic, h, SC_HEAD_SUB + 500 - len(t) )
        end if
    next

    '' Insertion sort, descending. nMax is a screenful.
    for i as long = 1 to nHits - 1
        dim as MdHit v = hits(i)
        dim as long j = i - 1
        while (j >= 0) andalso (hits(j).nScore < v.nScore)
            hits(j + 1) = hits(j)
            j -= 1
        wend
        hits(j + 1) = v
    next
    return nHits
end function
