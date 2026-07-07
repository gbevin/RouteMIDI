#!/usr/bin/env bash
#
# Packs RouteMIDI into an MCP Bundle (.mcpb), the double-click install format
# for Claude Desktop and other MCP hosts. The bundle is a zip of the manifest
# in extension/ plus the binary under server/, so the user installs RouteMIDI
# into the client with no configuration file to edit.
#
# Usage: build-mcpb.sh <path-to-routemidi-binary> [output.mcpb]
#
# The bundle is platform-specific: it carries the one binary you pass, so build
# it on (or with a binary for) each platform you publish. The manifest version
# is stamped from the binary's own --version, so there is nothing to keep in
# sync by hand.
#
# The resulting bundle should be validated and (re)packed with the official
# tool before publishing, e.g. `npx @anthropic-ai/mcpb pack <dir>`, which also
# checks the manifest against the current schema.

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: build-mcpb.sh <path-to-routemidi-binary> [output.mcpb]" >&2
    exit 2
fi

BIN="$1"
OUT="${2:-routemidi.mcpb}"

if [[ ! -x "$BIN" ]]; then
    echo "error: '$BIN' is not an executable binary" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MANIFEST="$SCRIPT_DIR/../extension/manifest.json"

# the version is the single source of truth in the binary; strip everything up
# to and including the " v" of the "routemidi v1.2.3" banner
VERSION="$("$BIN" --version | head -1 | sed -E 's/.* v//')"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/server"

# name the bundled binary so the manifest's "${__dirname}/server/routemidi"
# resolves on every platform: Windows auto-appends .exe to binary commands, so
# a Windows binary must keep its .exe name
if [[ "$BIN" == *.exe ]]; then
    cp "$BIN" "$STAGE/server/routemidi.exe"
else
    cp "$BIN" "$STAGE/server/routemidi"
    chmod +x "$STAGE/server/routemidi"
fi

# stamp the placeholder version in the packed manifest (the committed one keeps
# 0.0.0 so an unstamped copy is obviously not a release)
sed "s/\"version\": \"0.0.0\"/\"version\": \"$VERSION\"/" "$MANIFEST" > "$STAGE/manifest.json"

OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
rm -f "$OUT_ABS"
( cd "$STAGE" && zip -r -q "$OUT_ABS" manifest.json server )

echo "wrote $OUT_ABS (RouteMIDI $VERSION)"
