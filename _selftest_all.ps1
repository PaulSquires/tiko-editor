# ---------------------------------------------------------------------------
# Run all 27 TIKO_*_SELFTEST suites and print their result lines.
#
# THE ORACLE IS COMPARED PAIRED, NEVER AGAINST A STORED BASELINE. A baseline is
# a snapshot of the code AND the on-disk state -- the session file, the project,
# the recent-files list -- and at least three of these suites read that state.
# An absolute baseline captured yesterday reports differences that are nothing
# but yesterday.
#
# So: run this on the BEFORE build, run it on the AFTER build, diff the two
# outputs. Usage:
#     powershell -File _selftest_all.ps1 > before.txt
#
# Each suite runs the whole editor, which does not exit on its own -- it prints
# its result and then goes on being an editor. So each is killed after its
# output stops growing. That is why this reports RESULT LINES rather than exit
# codes: there is no exit code to have.
#
# THE STOP HEURISTIC IS NOT RELIABLE, AND A DIFFERENCE IT REPORTS IS A LEAD, NOT
# A RESULT. A suite that pauses mid-run looks finished, so the run is truncated
# and the missing sub-suites read as a regression. This was hit on the first
# 7d sweep: TIKO_FORMAT appeared to lose a whole sub-suite that had merely not
# been reached. Confirm anything this turns up by running the single suite on
# both builds with a fixed generous timeout.
#
# TIKO_FORMAT_SELFTEST IS NOT A STABLE ORACLE. Its "Format apply" sub-test
# includes localization checks that assert particular lang ids are BLANK, and it
# reported 7 then 6 failures on two runs of the SAME UNMODIFIED build. Treat any
# movement there as noise until that is fixed.
# ---------------------------------------------------------------------------
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$psp  = Join-Path $root "..\PsPlatform"
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

foreach ($s in $suites) {
    $out = Join-Path $env:TEMP "tiko_$s.txt"
    Remove-Item $out -ErrorAction SilentlyContinue
    Set-Item -Path "env:$s" -Value "1"
    $p = Start-Process -FilePath (Join-Path $root "tiko.exe") -PassThru -NoNewWindow `
                       -RedirectStandardOutput $out -RedirectStandardError "$out.err"
    # Wait for the output to stop growing rather than for a fixed time: the
    # suites differ by an order of magnitude in how long they take, and a single
    # timeout would either truncate the slow ones or waste minutes on the fast.
    $last = -1; $still = 0
    for ($i = 0; $i -lt 60; $i++) {
        Start-Sleep -Milliseconds 500
        $len = 0
        if (Test-Path $out) { $len = (Get-Item $out).Length }
        if ($len -eq $last -and $len -gt 0) { $still++ } else { $still = 0 }
        if ($still -ge 4) { break }
        $last = $len
    }
    try { $p.Kill() } catch {}
    try { $p.WaitForExit(5000) | Out-Null } catch {}
    Remove-Item "env:$s" -ErrorAction SilentlyContinue

    $lines = @()
    if (Test-Path $out) {
        $lines = Get-Content $out | Where-Object { $_ -match "passed|failed|FAIL|SELFTEST" }
    }
    Write-Output "=== $s"
    if ($lines.Count -eq 0) { Write-Output "    (no result line)" }
    else { $lines | ForEach-Object { Write-Output "    $_" } }
}
