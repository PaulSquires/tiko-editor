# ---------------------------------------------------------------------------
# tiko-editor test runner. Driven by build\_run_tests.bat -- see that file for
# the contract, the exit code, and why the parsing is not in batch.
# ---------------------------------------------------------------------------
param(
    [switch]$Verbose,
    [switch]$Keep
)

$ErrorActionPreference = "Stop"

$Root    = Split-Path -Parent $PSScriptRoot          # C:\dev\tiko-editor
$Exe     = Join-Path $Root "tiko-editor.exe"          # NEVER the copy in src\
$LogDir  = Join-Path $PSScriptRoot "testlogs"
$FbcDir  = Join-Path $Root "src\fbcParser"
$DbgDir  = Join-Path $Root "src\debugParser"

if ($Verbose) { $Keep = $true }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

# --- every gate the editor understands ------------------------------------
# Kept in one list so a new suite is added in exactly one place. The runner
# sets all of them on every launch; each gate returns immediately when unset,
# and they are independent of one another.
$Gates = @(
 'TIKO_ABOUT_SELFTEST','TIKO_AUTOC_SELFTEST','TIKO_AUTOINSERT_SELFTEST','TIKO_BUILDCONFIG_SELFTEST',
 'TIKO_CODETIP_SELFTEST','TIKO_COMPILECMD_SELFTEST','TIKO_COPYDATA_SELFTEST','TIKO_DEBUG_SELFTEST',
 'TIKO_ENCODING_SELFTEST','TIKO_EXPLORER_SELFTEST','TIKO_FILEWATCH_SELFTEST','TIKO_FINDPROJ_SELFTEST',
 'TIKO_FORMAT_SELFTEST','TIKO_HELPCENTER_SELFTEST','TIKO_INIPARSE_SELFTEST','TIKO_INPUTBOX_SELFTEST',
 'TIKO_KEYBOARD_SELFTEST','TIKO_NAVHISTORY_SELFTEST','TIKO_OPTIONS_SELFTEST','TIKO_OUTPUTFLOAT_SELFTEST',
 'TIKO_OUTPUTPANEL_SELFTEST','TIKO_PROJECTOPTIONS_SELFTEST','TIKO_SAVE_SELFTEST','TIKO_THEME_SELFTEST',
 'TIKO_UNUSED_SELFTEST','TIKO_USERTOOLS_SELFTEST','TIKO_WORKSPACE_SELFTEST',
 'PSBUFFERPAINT_SELFTEST','PSCOLORPICKER_SELFTEST'
)

# --- how each suite reports itself ----------------------------------------
# The suites do not share a report format -- they variously print
# "N passed, N failed", "N passed, N failed." , "=== N of N assertions passed ==="
# or nothing at all but a run of PASS/FAIL lines. Rather than pretend otherwise,
# each suite names its own pattern here.
#
#   Name     what the summary calls it
#   Pattern  a regex with two captures, passed then failed
#   Block    for suites that print no summary line: [start,end] markers, and the
#            runner counts PASS/FAIL lines between them
#   Baseline failures this suite ALREADY had before the FBC-Modern migration
#            began, measured on development at 2026-08-23 and carried forward
#            deliberately. Exactly this many is not a regression; MORE is. Drive
#            one of these to zero and lower the number in the same commit --
#            leaving it high would hide the next real failure behind it.
#   Flaky    the suite is nondeterministic: it returns a different split on
#            consecutive runs of an unchanged binary. Reported, never counted.
#            NOTHING CARRIES THIS TODAY. FORMAT (apply) and FORMATOPTIONS did,
#            until the cause was found -- LL() is sized by the .lang file's
#            "MAXIMUM:" line, 521, and both suites asserted ids 589-669, so
#            every lookup was an out-of-bounds heap read that fbc does not
#            bounds-check without -exx. Fixed in fc186ed3; both are now
#            deterministic (16/0 and 42/0, three runs running) and are counted
#            like everything else. The mechanism stays here because the next
#            suite to wobble should be understood, not excused.
$Suites = @(
  @{ Name='THEME';              Pattern='theme self-test: (\d+) passed, (\d+) failed' }
  @{ Name='OPTIONS (bind)';     Pattern='options bind test: (\d+) passed, (\d+) failed'; Baseline=6 }
  @{ Name='ENCODING';           Pattern='encoding self-test: (\d+) passed, (\d+) failed' }
  @{ Name='SAVE';               Pattern='TIKO_SAVE_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='COMPILECMD';         Pattern='TIKO_COMPILECMD_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='COPYDATA';           Pattern='TIKO_COPYDATA_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='INIPARSE';           Pattern='TIKO_INIPARSE_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='DEBUG';              Pattern='TIKO_DEBUG_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='UNUSED (model)';     Pattern='TIKO_UNUSED_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='FORMAT (engine)';    Pattern='Format engine self-test: (\d+) passed, (\d+) failed' }
  @{ Name='FORMAT (apply)';     Pattern='Format apply self-test: (\d+) passed, (\d+) failed' }
  @{ Name='AUTOINSERT';         Pattern='AutoInsert self-test: (\d+) passed, (\d+) failed' }
  @{ Name='FORMATOPTIONS';      Pattern='frmFormatOptions self-test \(headless\): (\d+) passed, (\d+) failed' }
  @{ Name='PSBUFFERPAINT';      Pattern='=== (\d+) of (\d+) assertions passed ==='; OfForm=$true }
  @{ Name='PSCOLORPICKER';      Pattern='PsColorPicker self-test: (\d+) passed, (\d+) failed' }
  @{ Name='KEYBOARD (vocab)';   Pattern='TIKO_KEYBOARD_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='KEYBOARD (staging)'; Pattern='TIKO_KEYBOARD_SELFTEST \(staging\): (\d+) passed, (\d+) failed' }
  @{ Name='KEYBOARD (capture)'; Pattern='TIKO_KEYBOARD_SELFTEST \(capture\): (\d+) passed, (\d+) failed' }
  @{ Name='INPUTBOX';           Pattern='TIKO_INPUTBOX_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='BUILDCONFIG';        Pattern='BUILDCONFIG: (\d+) passed, (\d+) failed' }
  @{ Name='PROJECTOPTIONS';     Pattern='frmProjectOptions self-test \(headless\)[\s\S]{0,400}?headless: (\d+) passed, (\d+) failed' }
  @{ Name='CODETIP';            Pattern='TIKO_CODETIP_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='NAVHISTORY';         Pattern='TIKO_NAVHISTORY_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='OUTPUTPANEL';        Pattern='TIKO_OUTPUTPANEL_SELFTEST: (\d+) passed, (\d+) failed \(both halves\)' }
  @{ Name='FINDPROJ (model)';   Pattern='TIKO_FINDPROJ_SELFTEST \(model\): (\d+) passed, (\d+) failed' }
  @{ Name='FINDPROJ (live)';    Pattern='TIKO_FINDPROJ_SELFTEST: (\d+) passed, (\d+) failed'; Baseline=1 }
  @{ Name='FILEWATCH';          Pattern='file watch: (\d+) passed, (\d+) failed' }
  @{ Name='HELPCENTER';         Pattern='TIKO_HELPCENTER_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='OUTPUTFLOAT';        Pattern='TIKO_OUTPUTFLOAT_SELFTEST: (\d+) passed, (\d+) failed' }
  @{ Name='AUTOC';              Block=@('=== frmAutoComplete geometry self-test ===','=== end frmAutoComplete self-test ===') }
  @{ Name='WORKSPACE';          Block=@('TIKO_WORKSPACE_SELFTEST','TIKO_WORKSPACE_SELFTEST') }
  @{ Name='EXPLORER';           Block=@('TIKO_EXPLORER_SELFTEST','TIKO_EXPLORER_SELFTEST') }
)

# Suites no unattended launch can reach. Named, always, with the reason.
$KnownManual = @(
  @{ Name='ABOUT';        Why='modal; needs TIKO_ABOUT_AUTOOPEN + TIKO_ABOUT_AUTOCLOSE by hand' }
  @{ Name='USERTOOLS';    Why='TIKO_USERTOOLS_AUTOOPEN shows a modal dialog with no auto-close hook' }
  @{ Name='UNUSED (layout)'; Why='TIKO_UNUSED_AUTOOPEN shows a window; the model half above does run' }
)

function Read-LogText([string]$path) {
    # The log is mixed ANSI / UTF-16LE in one stream. Strip the NULs, keep the rest.
    if (-not (Test-Path $path)) { return "" }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $kept  = New-Object System.Collections.Generic.List[byte]
    foreach ($b in $bytes) { if ($b -ne 0) { $kept.Add($b) } }
    return [System.Text.Encoding]::Default.GetString($kept.ToArray())
}

Write-Host ""
Write-Host "=============================================================="
Write-Host " tiko-editor test runner"
Write-Host "=============================================================="

# --- the stale-artifact guard --------------------------------------------
# A run that tested a stale binary produces a believable NUMBER, not an error.
# Four lines is what it costs to make that visible.
$fbc = $env:TIKO_FBC
if (-not $fbc) { $fbc = "C:\dev\fbc-modern\toolchains\fbc-modern-windows\fbc64.exe" }
Write-Host " compiler : $fbc"
foreach ($a in @($Exe, (Join-Path $Root 'fbcParser.dll'), (Join-Path $Root 'debugParser.dll'))) {
    if (Test-Path $a) {
        $i = Get-Item $a
        Write-Host ("  artifact : {0,-20} {1}  {2,10:N0} bytes" -f $i.Name, $i.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss"), $i.Length)
    } else {
        Write-Host "  artifact : $a  *** MISSING ***"
    }
}
if (-not (Test-Path $Exe)) {
    Write-Host ""
    Write-Host " tiko-editor.exe not found. Build it first (build\_compile_fast.bat)."
    exit 1
}
# Newest source file in src\, so "did this build include my change" is answerable.
$newest = Get-ChildItem (Join-Path $Root "src") -Recurse -Include *.bas,*.bi,*.inc -ErrorAction SilentlyContinue |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($newest -and $newest.LastWriteTime -gt (Get-Item $Exe).LastWriteTime) {
    Write-Host ""
    Write-Host " *** THE EXE IS OLDER THAN $($newest.Name). This run tests a stale binary. ***"
    Write-Host " *** Rebuild before believing anything below.                            ***"
}
Write-Host ""

# --- the editor pass ------------------------------------------------------
# One launch. TIKO_STARTUP_AUTOEXIT is the widest of the three exit hooks: it
# quits past Workspace and Explorer, which TIKO_FINDPROJ_AUTOEXIT stops short of.
foreach ($g in $Gates) { Set-Item -Path "env:$g" -Value "1" }
$env:TIKO_STARTUP_AUTOEXIT = "1"
$env:FBCPARSER_NOPAUSE     = "1"

$editorLog = Join-Path $LogDir "editor.log"
Write-Host " running the editor suites ..."
$sw = [Diagnostics.Stopwatch]::StartNew()
$p  = Start-Process -FilePath "cmd.exe" -ArgumentList "/c","`"`"$Exe`" > `"$editorLog`" 2>&1`"" -PassThru -WindowStyle Hidden
# 5 minutes is generous; the pass takes seconds. If it is hit, the editor did not
# reach its exit hook and the log is short -- say so rather than parse a stump.
$exited = $p.WaitForExit(300000)
$sw.Stop()
$hung = $false
if (-not $exited) {
    $hung = $true
    Write-Host ""
    Write-Host " *** THE EDITOR DID NOT EXIT within 300 s. ***"
    Write-Host " *** It is still running as PID $($p.Id). The runner will NOT kill it: its stdout is"
    Write-Host " *** block-buffered when redirected, so killing it would discard every suite report."
    Write-Host " *** Close it by hand. The results below are INCOMPLETE."
}
Write-Host ("   editor pass: {0:N1}s" -f $sw.Elapsed.TotalSeconds)

$log = Read-LogText $editorLog
if ($Verbose) { Write-Host ""; Write-Host $log }

# --- parse -----------------------------------------------------------------
$results = @()
foreach ($s in $Suites) {
    $passed = $null; $failed = $null
    if ($s.ContainsKey('Pattern')) {
        # Last match wins: a suite that prints a running total then a final one
        # is reporting the final one.
        $ms = [regex]::Matches($log, $s.Pattern)
        if ($ms.Count -gt 0) {
            $m = $ms[$ms.Count - 1]
            if ($s.OfForm) {
                # "=== N of M assertions passed ===" -- M is the total, not the failures.
                $passed = [int]$m.Groups[1].Value
                $failed = [int]$m.Groups[2].Value - $passed
            } else {
                $passed = [int]$m.Groups[1].Value
                $failed = [int]$m.Groups[2].Value
            }
        }
    } else {
        # No summary line: count PASS/FAIL lines inside the suite's own block.
        $i0 = $log.IndexOf($s.Block[0])
        if ($i0 -ge 0) {
            $i1 = $log.IndexOf($s.Block[1], $i0 + $s.Block[0].Length)
            if ($i1 -lt 0) { $i1 = $log.Length }
            $chunk  = $log.Substring($i0, $i1 - $i0)
            $passed = ([regex]::Matches($chunk, '(?m)^\s*PASS\b')).Count
            $failed = ([regex]::Matches($chunk, '(?m)^\s*FAIL\b')).Count
            if ($passed -eq 0 -and $failed -eq 0) { $passed = $null; $failed = $null }
        }
    }
    $base = 0
    if ($s.ContainsKey('Baseline')) { $base = [int]$s.Baseline }
    $results += [pscustomobject]@{
        Name = $s.Name; Passed = $passed; Failed = $failed
        Flaky = [bool]$s.Flaky; Baseline = $base; Kind = 'editor'
    }
}

# --- the DLL harnesses ------------------------------------------------------
# Out of process, their own fixtures, their own build scripts. tiko_fbctest
# pauses on `sleep` unless FBCPARSER_NOPAUSE=1 (set above) -- without it the
# runner waits forever on a harness that has already finished its work.
function Run-Harness([string]$dir, [string]$exe, [string]$logName, [string[]]$harnessArgs) {
    $path = Join-Path $dir $exe
    if (-not (Test-Path $path)) { return $null }
    $out = Join-Path $LogDir $logName
    $argLine = if ($harnessArgs) { " " + ($harnessArgs -join " ") } else { "" }
    & cmd.exe /c "cd /d `"$dir`" && `"$path`"$argLine > `"$out`" 2>&1"
    return @{ Code = $LASTEXITCODE; Log = $out }
}

Write-Host " running the DLL harnesses ..."
$fbcRun = Run-Harness $FbcDir "tiko_fbctest.exe" "fbctest.log" @()
$dbgRun = Run-Harness $DbgDir "tiko_dbgtest.exe" "dbgtest.log" @("_testfile_arrays.exe","vars")

foreach ($h in @(@('fbcParser harness',$fbcRun), @('debugParser harness',$dbgRun))) {
    $name = $h[0]; $run = $h[1]
    if ($null -eq $run) {
        $results += [pscustomobject]@{ Name=$name; Passed=$null; Failed=$null; Flaky=$false; Baseline=0; Kind='harness' }
    } else {
        # These harnesses report by exit code and by producing output, not by
        # counting assertions. Non-zero exit, or an empty log, is a failure.
        $len = if (Test-Path $run.Log) { (Get-Item $run.Log).Length } else { 0 }
        $ok  = ($run.Code -eq 0) -and ($len -gt 0)
        $results += [pscustomobject]@{
            Name=$name; Passed=$(if($ok){1}else{0}); Failed=$(if($ok){0}else{1})
            Flaky=$false; Baseline=0; Kind='harness'; Note="exit=$($run.Code) $len bytes"
        }
    }
}

# --- report -----------------------------------------------------------------
Write-Host ""
Write-Host "-------------------------------------------------------------- "
$totPass = 0; $totFail = 0
$nFailed = 0; $nSkipped = 0; $nFlakyFailed = 0; $nAtBaseline = 0; $nImproved = 0; $nRan = 0

foreach ($r in $results) {
    if ($null -eq $r.Passed) {
        Write-Host ("  {0,-22} SKIPPED  (produced no output this run)" -f $r.Name)
        $nSkipped++
        continue
    }
    $nRan++
    $totPass += $r.Passed; $totFail += $r.Failed
    $tag = ""
    if ($r.Failed -gt 0) {
        if ($r.Flaky) {
            # Nondeterministic: reported so it is visible, never counted, because
            # the same binary gives a different split on the next run.
            $tag = "  (nondeterministic -- not counted)"; $nFlakyFailed++
        } elseif ($r.Failed -eq $r.Baseline) {
            # Exactly the pre-existing failures. Not a regression.
            $tag = "  = baseline"; $nAtBaseline++
        } elseif ($r.Failed -lt $r.Baseline) {
            $tag = "  BETTER than baseline ($($r.Baseline)) -- lower Baseline in _run_tests.ps1"
            $nImproved++
        } else {
            $tag = "  FAILED ($($r.Failed - $r.Baseline) more than baseline $($r.Baseline))"
            $nFailed++
        }
    } elseif ($r.Baseline -gt 0) {
        $tag = "  BETTER than baseline ($($r.Baseline)) -- lower Baseline in _run_tests.ps1"
        $nImproved++
    }
    $note = if ($r.Note) { "   $($r.Note)" } else { "" }
    Write-Host ("  {0,-22} {1,6} passed  {2,4} failed{3}{4}" -f $r.Name, $r.Passed, $r.Failed, $note, $tag)
}

foreach ($m in $KnownManual) {
    Write-Host ("  {0,-22} SKIPPED  ({1})" -f $m.Name, $m.Why)
}

Write-Host "-------------------------------------------------------------- "
Write-Host ("  TOTAL {0:N0} passed, {1:N0} failed across {2} suites run; {3} skipped" -f $totPass, $totFail, $nRan, ($nSkipped + $KnownManual.Count))
if ($nAtBaseline -gt 0) {
    Write-Host "  $nAtBaseline suite(s) failed exactly as much as the recorded pre-migration baseline. Not a regression."
}
if ($nImproved -gt 0) {
    Write-Host "  $nImproved suite(s) did BETTER than their recorded baseline. Lower the Baseline in _run_tests.ps1"
    Write-Host "  in the same commit that fixed them, or the next real failure hides behind the stale number."
}
if ($nFlakyFailed -gt 0) {
    Write-Host ""
    Write-Host "  $nFlakyFailed suite(s) marked nondeterministic reported failures and were NOT counted."
    Write-Host "  Those suites vary run to run on an unchanged binary -- see docs/tiko-editor/RFC-0008 s6."
    Write-Host "  Compare them by hand, per suite; never by the grand total."
}
if ($hung) {
    Write-Host ""
    Write-Host "  THE EDITOR DID NOT EXIT. Everything above is incomplete."
    exit 99
}
Write-Host ""
if ($nFailed -eq 0) {
    Write-Host "  RESULT: no suite failed."
} else {
    Write-Host "  RESULT: $nFailed suite(s) FAILED."
}
Write-Host ""

if (-not $Keep -and $nFailed -eq 0 -and -not $hung) {
    Remove-Item $LogDir -Recurse -Force -ErrorAction SilentlyContinue
} else {
    Write-Host "  raw logs: $LogDir"
    Write-Host ""
}

exit $nFailed
