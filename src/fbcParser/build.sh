#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# build.sh -- the Linux twin of build.bat. Builds libfbcParser.so (the
# symbol-extraction engine) and tiko_fbctest (the standalone harness).
#
# Added by 7c step 18, which is the step that builds tikoshell for Linux.
# tikoshell links -l fbcParser, so without this there is nothing to link
# against and the port stops at the last command.
#
# ---- WHY THIS IS EXPECTED TO COMPILE UNCHANGED ----------------------------
#
# These sources ARE fbc's own front end. Every __FB_WIN32__ in them is already
# a guarded branch beside a Linux one -- the compiler they came from targets
# both -- and not one of them carries an #inclib. So the Windows-ness here is
# in the BUILD SCRIPT, which is this file, and not in the code.
#
# EXPECTED, NOT VERIFIED. No Windows session can compile a .so. If this fails
# on your machine the failure is the first real datum, and worth more than the
# expectation it contradicts.
#
# ---- WHERE THE OUTPUT GOES, AND WHY IT IS NOT HERE ------------------------
#
# The .so is copied to the TIKO ROOT, beside where fbcParser.dll sits on
# Windows. On Windows the link uses an import library (src/libfbcParser.dll.a)
# and the load uses the root DLL, so the two can live apart. Linux has no
# import library -- the .so is both -- so it has to be in one place that both
# _compile_shell.sh and _run_shell.sh name, and the root is the place Windows
# already made that mean "the parser".
#
# ---- -pic IS NOT OPTIONAL -------------------------------------------------
#
# A shared object on x86-64 Linux must be position-independent; without -pic
# ld rejects the relocations at the end of a full compile, naming an object
# file rather than the flag. fbc spells it -pic, not -fPIC.
#
# -z noexecstack for the reason build/build.bas gives at length: fbc's gas64
# backend emits no .note.GNU-stack, so ld marks the stack executable, and a
# hardened distro cares. That is a Fedora problem specifically.
# ---------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"        # src/fbcParser -> tiko root

FBC="${FBC:-fbc}"
command -v "$FBC" >/dev/null 2>&1 || {
    echo "ERROR: fbc not found ($FBC). Set FBC to your compiler."
    exit 1
}

cd "$HERE"

# Everything named _testfile* is parser test INPUT, never compiled -- the same
# rule build.bat applies, and the same two files held out of the DLL build.
SOURCES=()
for f in *.bas; do
    case "$f" in
        _testfile*|fbcparser-api.bas|fbcparser_test.bas) continue ;;
    esac
    SOURCES+=("$f")
done

if [ "${#SOURCES[@]}" -eq 0 ]; then
    echo "BUILD FAILED: no sources matched -- refusing to report OK"
    exit 1
fi
echo "[fbcParser] ${#SOURCES[@]} modules"

"$FBC" -dll -pic -d FBCPARSER_BUILDING_DLL \
    -Wl "-z,noexecstack" \
    fbcparser-api.bas "${SOURCES[@]}" -x libfbcParser.so || {
    echo "BUILD FAILED: libfbcParser.so"
    exit 1
}

"$FBC" -pic fbcparser_test.bas -p . -l fbcParser \
    -Wl "-rpath,\$ORIGIN,-z,noexecstack" \
    -x tiko_fbctest || {
    echo "BUILD FAILED: tiko_fbctest"
    exit 1
}

cp -f libfbcParser.so "$ROOT/libfbcParser.so"
echo "BUILD OK: libfbcParser.so + tiko_fbctest  (.so copied to $ROOT)"
