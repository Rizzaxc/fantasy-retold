#!/usr/bin/env bash
# Builds a BPS patch (vanilla Emerald -> release ROM) with RomPatcher.js.
# Run `make release` first; this only wraps the patch creation.
# Usage: ./make_patch.sh [version]   -> release/fantasy-retold-<version>.bps
# Overrides (env): CLEAN_ROM, RELEASE_ROM, ROMPATCHER (a RomPatcher.js checkout).
# With no ROMPATCHER, v3.2.1 is cloned once into .cache/ inside the repo.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="${1:-dev}"
CLEAN_ROM="${CLEAN_ROM:-$ROOT/Pokemon - Emerald Version (USA, Europe).gba}"
RELEASE_ROM="${RELEASE_ROM:-$ROOT/pokeemerald-release.gba}"
ROMPATCHER="${ROMPATCHER:-$ROOT/.cache/RomPatcher.js}"
OUT="$ROOT/release/fantasy-retold-$VERSION.bps"

[ -f "$CLEAN_ROM" ] || { echo "missing clean ROM: $CLEAN_ROM (set CLEAN_ROM)" >&2; exit 1; }
[ -f "$RELEASE_ROM" ] || { echo "missing $RELEASE_ROM, run 'make release' first (or set RELEASE_ROM)" >&2; exit 1; }

if [ ! -f "$ROMPATCHER/index.js" ]; then
    git clone --depth 1 --branch v3.2.1 https://github.com/marcrobledo/RomPatcher.js "$ROMPATCHER"
fi
[ -d "$ROMPATCHER/node_modules" ] || npm install --prefix "$ROMPATCHER"

mkdir -p "$ROOT/release"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
# RomPatcher saves the patch in the cwd, named after the modified ROM
(cd "$tmp" && node "$ROMPATCHER/index.js" create -f bps "$CLEAN_ROM" "$RELEASE_ROM")
mv "$tmp"/*.bps "$OUT"
echo "wrote $OUT"
