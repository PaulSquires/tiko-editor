# ---------------------------------------------------------------------------
# Run all 27 TIKO_*_SELFTEST suites and print their result lines.
#
# Usage:
#     powershell -File _selftest_all.ps1 -Out before.txt
#     ... rebuild ...
#     powershell -File _selftest_all.ps1 -Out after.txt
#     powershell -File _selftest_all.ps1 -Diff before.txt after.txt
#
# THE ORACLE IS COMPARED PAIRED, NEVER AGAINST A STORED BASELINE. A baseline is a
# snapshot of the code AND the on-disk state; several suites read settings/, and
# an absolute baseline captured yesterday reports differences that are nothing
# but yesterday.
#
# Each suite runs the whole editor, which does not exit on its own -- it prints
# its result and then goes on being an editor. So each is killed once its output
# has been QUIET long enough. That is why this reports result lines rather than
# exit codes: there is no exit code to have.
#
# ---- WHAT WENT WRONG THE FIRST TIME, AND WHAT IS DIFFERENT ----------------
#
# The first version waited 2 seconds of quiet and called that finished. A suite
# that PAUSES mid-run looks finished, so it was truncated and its unreached
# sub-suites read as a regression -- which is exactly what it reported on its
# first use, for TIKO_FORMAT. Fixed three ways:
#
#   1. QUIET_S is 8 seconds, not 2, and there is a MIN_S floor before the quiet
#      timer is even consulted.
#   2. Hitting CAP_S is REPORTED, per suite, as [TRUNCATED]. A run that was cut
#      off now says so instead of looking like a short one.
#   3. Every suite reports its ELAPSED time. A suite that took 44s against a 45s
#      cap is not to be trusted even without the marker.
#
# STATE IS SNAPSHOT AND RESTORED. The suites write to settings/, so the second
# sweep would otherwise start from state the first sweep left behind -- which is
# indistinguishable from a code change and was the leading theory for a
# regression that turned out to be noise. -RestoreState (the default) puts the
# mutable files back before each sweep so both builds see the same starting
# point.
#
# KNOWN-UNRELIABLE SUITES, which no amount of harness fixing can help:
#
#   TIKO_FORMAT_SELFTEST -- IT READS PAST THE END OF AN ARRAY, and the garbage it
#     finds there decides pass/fail. english.lang carries MAXIMUM:521, so LL is
#     redim'd to LL(521); the format-apply and frmFormatOptions sub-tests then
#     assert len(LL(id)) > 0 for ids 593-669, ALL of them beyond that bound. FB
#     does not bounds-check a dynamic array, so each of those 39 assertions reads
#     whatever happens to be past the end.
#
#     That explains everything it does: nondeterministic within one binary
#     (25/17, 27/15, 25/17), and systematically different between two binaries
#     (baseline ~25-27 passing, a rebuilt tiko ~14-15) because the heap beyond
#     the array differs. It correlates with the build without being caused by it.
#
#     UNDERNEATH IT IS A REAL BUG, and not in the test: those ids do not exist in
#     english.lang at all, so the Format Options labels they name render BLANK in
#     the UI. The test was written to catch exactly that and cannot report it.
#
#   one suite is nondeterministic outright (24/18, 33/9, 23/19 across three runs
#   of one unchanged binary; see Learnings.md).
#
# Movement in those two is noise. The other 25 are the oracle.
# ---------------------------------------------------------------------------
param(
    [string]$Out = "",
    [string[]]$Diff = @(),
    [switch]$NoRestoreState,
    [int]$QuietS = 8,
    [int]$MinS   = 3,
    [int]$CapS   = 45
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

# ---- diff mode: compare two captures ---------------------------------------
if ($Diff.Count -eq 2) {
    $a = Get-Content $Diff[0]
    $b = Get-Content $Diff[1]
    # -SyncWindow 0 keeps the two sides aligned. The default window re-pairs
    # unrelated lines across suite boundaries and produced pages of phantom
    # differences the first time this was compared.
    $d = Compare-Object $a $b -SyncWindow 0
    if (-not $d) { Write-Output "IDENTICAL"; exit 0 }
    Write-Output "--- $($Diff[0])"
    Write-Output "+++ $($Diff[1])"
    foreach ($r in $d) {
        $sign = if ($r.SideIndicator -eq "<=") { "-" } else { "+" }
        Write-Output "$sign $($r.InputObject)"
    }
    exit 1
}

# ---- the mutable state the suites touch ------------------------------------
# NOT all of settings/ -- that is 227 MB, nearly all of it help/ and webview2/,
# neither of which any suite writes. These are the small files that change.
$stateFiles = @(
    "settings\settings.ini",
    "settings\keybindings.ini",
    "settings\default.tiko",
    "tiko.tiko"
)
$snapDir = Join-Path $env:TEMP "tiko_selftest_state"

function Save-State {
    if (-not (Test-Path $snapDir)) { New-Item -ItemType Directory $snapDir | Out-Null }
    foreach ($f in $stateFiles) {
        $src = Join-Path $root $f
        if (Test-Path $src) {
            Copy-Item $src (Join-Path $snapDir (Split-Path $f -Leaf)) -Force
        }
    }
}
function Restore-State {
    foreach ($f in $stateFiles) {
        $snap = Join-Path $snapDir (Split-Path $f -Leaf)
        if (Test-Path $snap) { Copy-Item $snap (Join-Path $root $f) -Force }
    }
}

# A snapshot is taken once and reused, so BOTH sweeps start from the state that
# existed before the first one -- not from whatever the first sweep left.
if (-not $NoRestoreState) {
    if (-not (Test-Path (Join-Path $snapDir "settings.ini"))) { Save-State }
    Restore-State
}

$psp = Join-Path $root "..\PsPlatform"
$env:PATH = "$psp\build\out\win64;$psp\deps\out\win64\bin;" + $env:PATH

$suites = @(
 "TIKO_ABOUT_SELFTEST","TIKO_AUTOC_SELFTEST","TIKO_AUTOINSERT_SELFTEST",
 "TIKO_BUILDCONFIG_SELFTEST","TIKO_CODETIP_SELFTEST","TIKO_COMPILECMD_SELFTEST",
 "TIKO_COPYDATA_SELFTEST","TIKO_DEBUG_SELFTEST","TIKO_ENCODING_SELFTEST",
 "TIKO_EXPLORER_SELFTEST","TIKO_FILEWATCH_SELFTEST","TIKO_FINDPROJ_SELFTEST",
 "TIKO_FORMAT_SELFTEST","TIKO_HELPCENTER_SELFTEST","TIKO_INIPARSE_SELFTEST",
 "TIKO_INPUTBOX_SELFTEST","TIKO_KEYBOARD_SELFTEST","TIKO_NAVHISTORY_SELFTEST",
 "TIKO_OPTIONS_SELFTEST","TIKO_OUTPUTFLOAT_SELFTEST","TIKO_OUTPUTPANEL_SELFTEST",
 "TIKO_PROJECTOPTIONS_SELFTEST","TIKO_SAVE_SELFTEST","TIKO_THEME_SELFTEST",
 "TIKO_UNUSED_SELFTEST","TIKO_USERTOOLS_SELFTEST","TIKO_WORKSPACE_SELFTEST"
)

$lines = New-Object System.Collections.Generic.List[string]
$logDir = Join-Path $env:TEMP "tiko_selftest_logs"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory $logDir | Out-Null }

foreach ($s in $suites) {
    # $outFile, NOT $out: POWERSHELL VARIABLES ARE CASE-INSENSITIVE, so a loop
    # variable named $out IS the -Out parameter. The capture was silently written
    # to the last suite's log path instead of the requested file -- the same
    # family of trap as fbc's case-insensitive identifiers, in another language.
    $outFile = Join-Path $logDir "$s.txt"
    Remove-Item $outFile -ErrorAction SilentlyContinue
    Set-Item -Path "env:$s" -Value "1"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath (Join-Path $root "tiko.exe") -PassThru -NoNewWindow `
                       -RedirectStandardOutput $outFile -RedirectStandardError "$outFile.err"
    $lastLen = -1
    $quietMs = 0
    $truncated = $false
    while ($true) {
        Start-Sleep -Milliseconds 250
        $len = 0
        if (Test-Path $outFile) { $len = (Get-Item $outFile).Length }
        if ($len -eq $lastLen) { $quietMs += 250 } else { $quietMs = 0 }
        $lastLen = $len
        $elapsed = $sw.Elapsed.TotalSeconds
        if ($elapsed -ge $CapS) { $truncated = $true; break }
        if ($elapsed -ge $MinS -and $len -gt 0 -and $quietMs -ge ($QuietS * 1000)) { break }
        if ($p.HasExited -and $quietMs -ge 1000) { break }
    }
    $sw.Stop()
    try { $p.Kill() } catch {}
    try { $p.WaitForExit(5000) | Out-Null } catch {}
    Remove-Item "env:$s" -ErrorAction SilentlyContinue

    # Elapsed is bucketed, not printed to the millisecond: an exact duration
    # differs on every run and would make every diff nonempty.
    $bucket = [int][Math]::Floor($sw.Elapsed.TotalSeconds / 10) * 10
    $mark = if ($truncated) { "  [TRUNCATED at ${CapS}s]" } else { "" }
    $lines.Add("=== $s   (~${bucket}s+)$mark")

    $res = @()
    if (Test-Path $outFile) {
        $res = Get-Content $outFile | Where-Object { $_ -match "passed|failed|FAIL|SELFTEST" }
    }
    if ($res.Count -eq 0) { $lines.Add("    (no result line)") }
    else { $res | ForEach-Object { $lines.Add("    $_") } }
}

if ($Out) {
    # ASCII, and explicitly: the default here is UTF-16, which makes every
    # ordinary text tool report the captures as "binary files differ".
    $lines | Out-File -FilePath $Out -Encoding ascii
    Write-Output "wrote $Out  ($($lines.Count) lines)"
} else {
    $lines | ForEach-Object { Write-Output $_ }
}
