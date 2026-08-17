#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# _run_shell.sh -- the Linux twin of _run_shell.bat. Added by 7c step 18.
#
# THE SHARED OBJECTS MUST BE FOUND, NOT COPIED, and the .bat records why on
# Windows. Linux has a sharper version of the same problem: the loader does
# NOT search the executable's own directory, so even copying everything next
# to tikoshell would not be enough without an rpath. PsPlatform's own driver
# links its binaries with -rpath $ORIGIN and stages the .so files beside them;
# this binary's libraries live in ANOTHER TREE at a path only the author
# knows, so LD_LIBRARY_PATH is the arrangement that works.
#
# Three directories, and each one is a different failure if it is missing:
#
#   PsPlatform/build/out/linux64   libpsscintilla.so -- the editor
#   PsPlatform/deps/out/linux64/lib  libSDL3.so.0, blend2d, freetype, harfbuzz
#   the tiko root                  libfbcParser.so -- the symbol scanner
#
# ALL THREE FAIL THE SAME WAY: the process dies BEFORE main with
#     error while loading shared libraries: <name>: cannot open shared object
# which at least names the library, unlike Windows' exit 127 with no message.
# That is the one respect in which this is the easier platform.
#
# Arguments are forwarded, so --selftest and friends work through it.
#
# PsPlatform is assumed to be a sibling; override with PSP=/path/to/PsPlatform.
# ---------------------------------------------------------------------------
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PSP="${PSP:-$(cd "$ROOT/.." && pwd)/PsPlatform}"
EXE="$ROOT/_shell/tikoshell"

[ -x "$EXE" ] || { echo "ERROR: $EXE is not there. Build it: bash _compile_shell.sh"; exit 1; }

export LD_LIBRARY_PATH="$PSP/build/out/linux64:$PSP/deps/out/linux64/lib:$ROOT${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec "$EXE" "$@"
