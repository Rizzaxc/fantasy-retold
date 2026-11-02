#!/usr/bin/env bash
# Builds a BPS patch (vanilla Emerald -> release ROM) with RomPatcher.js.
# Run `make release` first (in WSL); this only wraps the patch creation.
# Usage: ./make_patch.sh [version]   -> release/fantasy-retold-<version>.bps
set -euo pipefail

cd "$(dirname "$0")"
VERSION="${1:-dev}"
CLEAN_ROM="${CLEAN_ROM:-Pokemon - Emerald Version (USA, Europe).gba}"
RELEASE_ROM="${RELEASE_ROM:-pokeemerald-release.gba}"
ROMPATCHER="${ROMPATCHER:-../RomPatcher.js-3.2.1}"
OUT="release/fantasy-retold-$VERSION.bps"

[ -f "$CLEAN_ROM" ] || { echo "missing clean ROM: $CLEAN_ROM" >&2; exit 1; }
[ -f "$RELEASE_ROM" ] || { echo "missing $RELEASE_ROM, run 'make release' first" >&2; exit 1; }
[ -d "$ROMPATCHER/node_modules" ] || (cd "$ROMPATCHER" && npm install)

mkdir -p release
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
# RomPatcher saves the patch in the cwd, named after the modified ROM
(cd "$tmp" && node "$ROMPATCHER/index.js" create -f bps "$OLDPWD/$CLEAN_ROM" "$OLDPWD/$RELEASE_ROM")
mv "$tmp"/*.bps "$OUT"
echo "wrote $OUT"
