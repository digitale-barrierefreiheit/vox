#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
#
# Prepare a release (Stage 1 of .github/workflows/release.yml, #103):
#   1. read project(VERSION) from CMakeLists.txt and bump it (patch|minor|major),
#   2. write the new version into CMakeLists.txt and vcpkg.json,
#   3. promote the CHANGELOG "[Unreleased]" section to "[<version>] - <today>",
#      leaving a fresh empty "[Unreleased]" and updating the link references,
#   4. write the new version's changelog body to the release-notes file.
# Prints the new version on stdout (and to $GITHUB_OUTPUT as version=… in CI).
#
# Pure file edits — no git, no network — so it is testable locally:
#   VOX_ROOT=/tmp/copy ./tools/prepare-release.sh patch
set -euo pipefail

bump="${1:?usage: prepare-release.sh <patch|minor|major>}"
root="${VOX_ROOT:-"$(cd "$(dirname "$0")/.." && pwd)"}"
notes="${RELEASE_NOTES_FILE:-"$root/release-notes.md"}"
repo="https://github.com/digitale-barrierefreiheit/vox"

cmake="$root/CMakeLists.txt"
vcpkg="$root/vcpkg.json"
changelog="$root/CHANGELOG.md"

# 1. Current version = the first X.Y.Z after a `VERSION` keyword (project(VERSION …);
#    cmake_minimum_required's "VERSION 3.25" is two-component, so it never matches).
cur="$(grep -oE 'VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+' "$cmake" | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
[[ -n "$cur" ]] || { echo "could not find project(VERSION X.Y.Z) in $cmake" >&2; exit 1; }
IFS=. read -r major minor patch <<EOF
$cur
EOF
case "$bump" in
  major) major=$((major + 1)); minor=0; patch=0 ;;
  minor) minor=$((minor + 1)); patch=0 ;;
  patch) patch=$((patch + 1)) ;;
  *) echo "unknown release-type '$bump' (expected patch|minor|major)" >&2; exit 1 ;;
esac
new="$major.$minor.$patch"
today="$(date -u +%Y-%m-%d)"

# 2. Write the new version — portable in-place edits via awk to a temp file (no
#    `sed -i`, whose flags and the `a`/`0,/…/` features differ on BSD/macOS sed).
#    Only the first `<spaces>VERSION X.Y.Z` (the project() line; cmake_minimum's
#    two-component "VERSION 3.25" never matches) and vcpkg.json's "version" change.
awk -v new="$new" '
  !done && /^[[:space:]]*VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/ { sub(/[0-9]+\.[0-9]+\.[0-9]+/, new); done = 1 }
  { print }
' "$cmake" > "$cmake.tmp" && mv "$cmake.tmp" "$cmake"
awk -v new="$new" '
  !done && /"version"[[:space:]]*:[[:space:]]*"[0-9]+\.[0-9]+\.[0-9]+"/ { sub(/[0-9]+\.[0-9]+\.[0-9]+/, new); done = 1 }
  { print }
' "$vcpkg" > "$vcpkg.tmp" && mv "$vcpkg.tmp" "$vcpkg"

# 3. Promote the changelog (one portable awk pass): insert "## [<new>] - <today>"
#    right after the "## [Unreleased]" heading — so the accumulated notes fall under
#    the new version, leaving [Unreleased] empty — and rewrite the link references.
awk -v new="$new" -v today="$today" -v repo="$repo" '
  /^## \[Unreleased\]/ && !promoted { print; print ""; print "## [" new "] - " today; promoted = 1; next }
  /^\[Unreleased\]:/ {
    print "[Unreleased]: " repo "/compare/v" new "...dev"
    print "[" new "]: " repo "/releases/tag/v" new
    next
  }
  { print }
' "$changelog" > "$changelog.tmp" && mv "$changelog.tmp" "$changelog"

# 4. Extract the new version's body for the PR description and the release notes.
bash "$(dirname "$0")/changelog-section.sh" "$new" "$changelog" > "$notes"
[[ -s "$notes" ]] || { echo "the [Unreleased] changelog section is empty — add release notes before cutting a release" >&2; exit 1; }

echo "$new"
[[ -n "${GITHUB_OUTPUT:-}" ]] && echo "version=$new" >> "$GITHUB_OUTPUT"
exit 0
