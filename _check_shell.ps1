# ---------------------------------------------------------------------------
# THE SHELL RATCHET -- src\shell must not reach back into the Win32 shell.
#
# Phase 7c's binary exists to be the translation unit with no AfxNova in it.
# That is not a style preference: it is the property that lets it take
# PsPlatform's UI at GLOBAL SCOPE and carry zero `PsC.` prefixes, which is 7c's
# end state and is unobtainable inside tiko.bas -- see src\shell\tikoshell.bas.
# One #include added in passing takes it away, and the file still builds.
#
# ---- WHY THIS CHECKS INCLUDES AND NOT TOKENS ------------------------------
#
# src\_check_app_layer.bas is a PROGRAM because substring matching could not
# tell `ghMenuBar` from `HMENU`. This one has the opposite problem: the shell's
# SOURCE legitimately names AfxNova, frmMain and windows.bi dozens of times, in
# comments explaining why it does not include them. A token scan reports every
# one of those.
#
# What matters is REACHABILITY, and reachability is an #include. So this reads
# only lines whose first non-blank token is #include -- exact, because an
# include is a path and no comment can be one.
#
# AND IT IS THE SHAPE OF CHECK THAT WOULD HAVE CAUGHT THE ONE REAL VIOLATION
# FOUND SO FAR, which ran the other way: app\modMenuDefinitions.inc:22 pulls
# "../modKeyBindings.bi" -- the app layer reaching UP into the shell by relative
# path. The token ratchet cannot see it, because a path is not an identifier.
#
# ---- WHY THIS IS POWERSHELL AND NOT A .bat --------------------------------
#
# It was a .bat first. A `for /f` wrapping `powershell -Command` with a pipe in
# it needs `^|`, and inside a `^` line continuation cmd mangled it: PowerShell
# errored, the loop bound nothing, and THE SCRIPT PRINTED OK ANYWAY. A check
# that cannot run must not pass, and the cheapest way to guarantee that was to
# stop nesting two quoting languages.
# ---------------------------------------------------------------------------

$ErrorActionPreference = 'Stop'
$shell = Join-Path $PSScriptRoot 'src\shell'

if (-not (Test-Path $shell)) {
    Write-Host "  FAIL  src\shell does not exist"
    exit 2
}

$files = Get-ChildItem -Path $shell -Include *.bas, *.inc, *.bi -File -Recurse

# A CHECKER THAT SCANNED NOTHING MUST NOT SAY OK. Same rule as the app-layer
# ratchet, and for the same reason: its first version resolved its directory
# wrongly, found no files, and reported a clean run.
if ($files.Count -eq 0) {
    Write-Host "  FAIL  no source files found in $shell"
    exit 2
}

$bad = 0

# The forbidden reach: Windows itself, AfxNova, the CRT header that drags
# vbcompat's globals in, and any of tiko's own shell files.
$forbidden = 'windows\.bi|AfxNova|vbcompat|win\\|/frm|\\frm|modDeclares|modRoutines|modScintilla'

# An `app/` PATH IS NOT A SHELL REACH, and the distinction has teeth now.
# modScintilla was a shell header when this list was written and MOVED INTO app\ in
# 7c step 3 commit 3c -- so `#include once "app/modScintilla.bi"` is the layer doing
# exactly what it is supposed to, and the name alone can no longer decide. Anything
# still under src\ by that name is caught as before.
$appPath = 'include\s+(once\s+)?"app/'

foreach ($f in $files) {
    $hits = Select-String -Path $f.FullName -Pattern '^\s*#\s*include' |
            Where-Object { $_.Line -match $forbidden -and $_.Line -notmatch $appPath }
    foreach ($h in $hits) {
        Write-Host ("  FAIL  {0}:{1}  {2}" -f $f.Name, $h.LineNumber, $h.Line.Trim())
        $bad++
    }
}

# ---- THE POSITIVE PROPERTY, which is the point of the negative one ---------
# Zero `PsC.` prefixes. tiko.bas fences PsPlatform's UI in `namespace PsC`
# because both sides define PsBufferPaint and PsBgrToArgb; the shell has no such
# collision and takes the UI at global scope. A PsC. appearing here means the
# fence has followed the code across, and the reason for a separate translation
# unit has gone with it.
#
# Comment lines are excluded -- this file's own source says "PsC." in prose more
# than once, and so does the shell's.
$psc = Select-String -Path ($files | ForEach-Object { $_.FullName }) -Pattern 'PsC\.' |
       Where-Object { $_.Line -notmatch "^\s*'" }
if ($psc.Count -gt 0) {
    foreach ($h in $psc) {
        Write-Host ("  FAIL  {0}:{1}  PsC. prefix  {2}" -f `
                    (Split-Path $h.Path -Leaf), $h.LineNumber, $h.Line.Trim())
    }
    $bad += $psc.Count
}

# ---- AND NO WINDOWS SEPARATOR IN A PATH LITERAL -- 7c step 18 -------------
#
# THE SAME REASON THIS FILE EXISTS AT ALL: a path is not an identifier. The
# app-layer token ratchet greps a vocabulary of Win32 and AfxNova NAMES, and
# `"settings\settings.ini"` contains none of them, so both layers were full of
# Windows separators while every gate was green.
#
# What that cost, before anyone ran it on Linux:
#   - four %TEMP% scratch directories in the shell suite that resolved to bare
#     relative names, so the suite wrote nowhere and failed silently
#   - clsConfig's seven settings paths, which were ALREADY MIXED -- PsExePath
#     returns forward slashes on both platforms, so they read
#     "C:/dev/tiko/settings\settings.ini" and Windows opened them anyway
#
# PsPath.bi states the rule: shared logic uses '/', PsFile converts at the API
# boundary with PsPathToNative. This checks both layers, because app\ is the
# half tiko.exe shares.
#
# THREE LITERALS ARE ALLOWED, BY THEIR EXACT TEXT and not by file:
#   "\"              -- the separator ITSELF, in a comparison. modProjectFolders
#                       accepts both separators on purpose and is correct.
#   "Ctrl+\"
#   "Ctrl+Shift+\"   -- the backslash KEY in a key binding name. Not a path.
# Escaped literals (!"...\n") are skipped: their backslashes are escapes.
$layers = @((Join-Path $PSScriptRoot 'src\shell'), (Join-Path $PSScriptRoot 'src\app'))
$pathFiles = @()
foreach ($d in $layers) {
    if (-not (Test-Path $d)) { Write-Host "  FAIL  $d does not exist"; exit 2 }
    $pathFiles += Get-ChildItem -Path $d -Include *.bas, *.inc, *.bi -File -Recurse
}
if ($pathFiles.Count -eq 0) {
    Write-Host "  FAIL  no source files found for the separator check"
    exit 2
}

$allowed = @('"\"', '"Ctrl+\"', '"Ctrl+Shift+\"')
$sepBad  = 0
foreach ($f in $pathFiles) {
    $n = 0
    foreach ($line in [System.IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $t = $line.TrimStart()
        if ($t.StartsWith("'") -or $t -match '^(?i)rem\s') { continue }
        # Drop escaped string literals and then the allowed ones, in that order.
        # An escaped literal ends at the first UNESCAPED quote: !"...\"...\n" is
        # one string, and a naive [^"]* stops at the \" in the middle of it.
        $probe = [regex]::Replace($line, '!"(?:\\.|[^"\\])*"', '')
        foreach ($a in $allowed) { $probe = $probe.Replace($a, '') }
        if ($probe -match '"[^"]*\\[^"]*"') {
            Write-Host ("  FAIL  {0}:{1}  Windows separator in a path literal  {2}" -f `
                        $f.Name, $n, $line.Trim())
            $sepBad++
        }
        # %TEMP% IS THE SAME CLASS WEARING AN ENVIRONMENT VARIABLE, and the
        # literal rule above cannot see it -- environ("TEMP") has no backslash
        # in it. It is UNSET ON LINUX, so the result is an empty string that
        # concatenates into a plausible relative path and fails silently.
        if ($probe -match 'environ\s*\(\s*"(TEMP|TMP)"') {
            Write-Host ("  FAIL  {0}:{1}  %TEMP% directly -- use PsKnownFolder(PSFOLDER_TEMP)  {2}" -f `
                        $f.Name, $n, $line.Trim())
            $sepBad++
        }
    }
}
$bad += $sepBad

Write-Host ""
if ($bad -gt 0) {
    Write-Host "  $bad violation(s): $($bad - $sepBad) reach/prefix in src\shell"
    Write-Host "  ($($files.Count) file(s)), $sepBad separator in src\shell + src\app"
    Write-Host "  ($($pathFiles.Count) file(s))."
    if ($bad -gt $sepBad) {
        Write-Host ""
        Write-Host "  The shell binary is the translation unit with no AfxNova in it,"
        Write-Host "  which is what lets it take PsPlatform's UI at global scope. Move"
        Write-Host "  the code, or move the dependency down into app\ -- not an #ifdef."
    }
    if ($sepBad -gt 0) {
        Write-Host ""
        Write-Host "  Use '/' and let PsFile convert at the API boundary -- PsPath.bi's"
        Write-Host "  house rule, and PsExePath has returned '/' on both platforms all"
        Write-Host "  along, so a '\' literal makes a MIXED path even on Windows. For a"
        Write-Host "  scratch directory use PsKnownFolder(PSFOLDER_TEMP), not %TEMP%."
    }
    exit 1
}
Write-Host "  ok      src\shell reaches no Win32 shell header, and carries no PsC. ($($files.Count) file(s))"
Write-Host "  ok      no Windows separator and no %TEMP% in src\shell or src\app ($($pathFiles.Count) file(s))"
exit 0
