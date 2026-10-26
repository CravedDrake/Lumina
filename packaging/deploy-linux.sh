#!/usr/bin/env bash
# Builds a Release tarball using system Qt and libVLC.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cmake -S "$ROOT" -B "$ROOT/build-release" -DCMAKE_BUILD_TYPE=Release -DLUMINA_BUILD_TESTS=OFF
cmake --build "$ROOT/build-release"
cmake --install "$ROOT/build-release" --prefix "$ROOT/dist/lumina"
cp "$ROOT/packaging/lumina.desktop" "$ROOT/LICENSE" "$ROOT/dist/lumina/"
tar -C "$ROOT/dist" -czf "$ROOT/dist/lumina-linux.tar.gz" lumina
echo "Created $ROOT/dist/lumina-linux.tar.gz"
