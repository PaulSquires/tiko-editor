#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# _compile_shell.sh -- the Linux twin of _compile_shell.bat. Added by 7c step
# 18. Read that file's header first: everything it says about WHY the shell is
# a second translation unit, and why gas64 rather than gcc, is unchanged here.
#
# THERE IS NO LINUX TWIN OF _compile_fast.bat AND THERE IS NOT MEANT TO BE.
# tiko.exe is Win32 -- AfxNova, HWNDs, a message loop -- and 7c exists because
# that is not portable. tikoshell is the binary that goes to Linux.
#
# ---- WHAT DIFFERS FROM THE .bat, AND IT IS ALL LINKING --------------------
#
#   deps/out/linux64/lib  and  build/out/linux64   instead of the win64 pair.
#
#   -Wl "-z,noexecstack" -- NOT optional, and not cosmetic. fbc's gas64
#   backend emits no .note.GNU-stack section, so ld falls back to marking the
#   stack EXECUTABLE. Hardened distros and SELinux object; build/build.bas in
#   PsPlatform calls it "a Fedora problem specifically". Passed here because
#   this build does not go through that driver.
#
#   NO -rpath. PsPlatform's driver stages its .so files next to each binary
#   and uses $ORIGIN; this binary's libraries live in ANOTHER TREE that the
#   author may have checked out anywhere, so the run script sets
#   LD_LIBRARY_PATH instead -- exactly as _run_shell.bat sets PATH on Windows,
#   and for the same reason. Skip _run_shell.sh and you get
#       error while loading shared libraries: libSDL3.so.0
#   before main, which reads as a broken build rather than a missing path.
#
#   -l fbcParser resolves against libfbcParser.so in the TIKO ROOT, built by
#   src/fbcParser/build.sh. Windows links an import library in src\ and loads
#   a DLL from the root; Linux has no import library, so both are the root
#   copy and -p names it.
#
# PsPlatform IS ASSUMED TO BE A SIBLING of this tree, as on Windows. Override
# with PSP=/path/to/PsPlatform.
# ---------------------------------------------------------------------------
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PSP="${PSP:-$(cd "$ROOT/.." && pwd)/PsPlatform}"

FBC="${FBC:-fbc}"
command -v "$FBC" >/dev/null 2>&1 || {
    echo "ERROR: fbc not found ($FBC). Set FBC to your compiler."
    exit 1
}

# A BUILD THAT CANNOT FIND ITS DEPENDENCIES MUST NOT START, because fbc's own
# error for a missing -p directory is a list of undefined references at the
# very end, naming symbols instead of the directory that is not there.
for d in "$PSP/src" "$PSP/src/bind" "$PSP/deps/out/linux64/lib" "$PSP/build/out/linux64"; do
    [ -d "$d" ] || { echo "ERROR: missing $d"; echo "       Build PsPlatform first: cd $PSP && bash build.sh check"; exit 1; }
done
[ -f "$ROOT/libfbcParser.so" ] || {
    echo "ERROR: $ROOT/libfbcParser.so is not there."
    echo "       Build it first: bash src/fbcParser/build.sh"
    exit 1
}

mkdir -p "$ROOT/_shell"
cd "$ROOT/src"

"$FBC" -gen gas64 -p . -p .. \
    -i "$PSP/src" -i "$PSP/src/bind" \
    -p "$PSP/deps/out/linux64/lib" -p "$PSP/build/out/linux64" \
    -l psscintilla -l fbcParser \
    -Wl "-z,noexecstack" \
    -x "$ROOT/_shell/tikoshell" shell/tikoshell.bas

echo "BUILD OK: $ROOT/_shell/tikoshell   (run it with _run_shell.sh)"
