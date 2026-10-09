#!/bin/sh
#
# Launch the locally built QLC+ (with the Stage Geometry tab).
#
# This runs the self-contained install under build/stage/usr, so no system
# installation (and no root) is required. The binary already has the
# plugin/fixture paths baked in pointing at this prefix.
#
# Usage:  ./run-qlcplus.sh [qlcplus options]

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="$DIR/build/stage/usr"

if [ ! -x "$PREFIX/bin/qlcplus" ]; then
    echo "QLC+ is not built yet. Run:" >&2
    echo "  cmake -S . -B build -DINSTALL_ROOT=$DIR/build/stage -DCMAKE_INSTALL_PREFIX=$PREFIX" >&2
    echo "  cmake --build build -j\$(nproc) && cmake --install build" >&2
    exit 1
fi

cd "$PREFIX"
export LD_LIBRARY_PATH="$PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$PREFIX/bin/qlcplus" "$@"
