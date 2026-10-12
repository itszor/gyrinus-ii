#!/bin/sh
# Build Gyrinus II from your own copy of the game:
#     ./build.sh "Gyrinus II - Son of Gyrinus (19xx)(Tom Cooper).zip"     (or the two .adf files)
# then: cd recomp && ./gyrinus2
# The steps are recipe.json's "build", run by SuperSpider's tools/weave.py.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SUPERSPIDER="${SUPERSPIDER:-$HOME/stuff/superspider}"
[ $# -gt 0 ] || { echo "usage: $0 GAME.zip | DISC1.adf DISC2.adf" >&2; exit 2; }
(cd "$SUPERSPIDER" && dune build)
WEAVE_RECIPES="$HERE" exec python3 -I "$SUPERSPIDER/tools/weave.py" build "$@"
