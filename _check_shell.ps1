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

foreach ($f in $files) {
    $hits = Select-String -Path $f.FullName -Pattern '^\s*#\s*include' |
            Where-Object { $_.Line -match $forbidden }
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

Write-Host ""
if ($bad -gt 0) {
    Write-Host "  $bad violation(s) in src\shell across $($files.Count) file(s)."
    Write-Host ""
    Write-Host "  The shell binary is the translation unit with no AfxNova in it,"
    Write-Host "  which is what lets it take PsPlatform's UI at global scope. Move"
    Write-Host "  the code, or move the dependency down into app\ -- not an #ifdef."
    exit 1
}
Write-Host "  ok      src\shell reaches no Win32 shell header, and carries no PsC. ($($files.Count) file(s))"
exit 0
