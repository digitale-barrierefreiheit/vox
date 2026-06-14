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
repo="${VOX_REPO_URL:-https://github.com/digitale-barrierefreiheit/vox}"

cmake="$root/CMakeLists.txt"
vcpkg="$root/vcpkg.json"
changelog="$root/CHANGELOG.md"

# 1. Current version = the first X.Y.Z after a `VERSION` keyword (project(VERSION …);
#    cmake_minimum_required's "VERSION 3.25" is two-component, so it never matches).
# `|| true` so a no-match does not trip `set -e` here — the guard below reports it.
cur="$(grep -oE 'VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+' "$cmake" | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)"
[[ -n "$cur" ]] || { echo "could not find project(VERSION X.Y.Z) in $cmake" >&2; exit 1; }
IFS=. read -r major minor patch <<< "$cur"
case "$bump" in
  major) major=$((major + 1)); minor=0; patch=0 ;;
  minor) minor=$((minor + 1)); patch=0 ;;
  patch) patch=$((patch + 1)) ;;
  *) echo "unknown release-type '$bump' (expected patch|minor|major)" >&2; exit 1 ;;
esac
new="$major.$minor.$patch"
today="$(date -u +%Y-%m-%d)"

# 2. Write the new version — portable in-place edits via awk to a temp file (no
#    `sed -i`, whose flags and `a`/`0,/…/` features differ on BSD/macOS sed). Each
#    awk fails via its END rule if the pattern never matched (same regex the version
#    read used; cmake_minimum's two-component "VERSION 3.25" never matches), so a
#    format change can't silently leave a file unbumped — which would make an
#    inconsistent release commit.
awk -v new="$new" '
  !done && /VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/ { sub(/[0-9]+\.[0-9]+\.[0-9]+/, new); done = 1 }
  { print }
  END { if (!done) exit 1 }
' "$cmake" > "$cmake.tmp" || { rm -f "$cmake.tmp"; echo "no project(VERSION X.Y.Z) line found in $cmake" >&2; exit 1; }
mv "$cmake.tmp" "$cmake"
awk -v new="$new" '
  !done && /"version"[[:space:]]*:[[:space:]]*"[0-9]+\.[0-9]+\.[0-9]+"/ { sub(/[0-9]+\.[0-9]+\.[0-9]+/, new); done = 1 }
  { print }
  END { if (!done) exit 1 }
' "$vcpkg" > "$vcpkg.tmp" || { rm -f "$vcpkg.tmp"; echo "no \"version\" field found in $vcpkg" >&2; exit 1; }
mv "$vcpkg.tmp" "$vcpkg"

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
  END { if (!promoted) exit 1 }
' "$changelog" > "$changelog.tmp" || { rm -f "$changelog.tmp"; echo "no '## [Unreleased]' heading found in $changelog" >&2; exit 1; }
mv "$changelog.tmp" "$changelog"

# 4. Extract the new version's body for the PR description and the release notes.
bash "$(dirname "$0")/changelog-section.sh" "$new" "$changelog" > "$notes"
[[ -s "$notes" ]] || { echo "no release notes to publish — add changelog entries under '## [Unreleased]' before cutting a release" >&2; exit 1; }

echo "$new"
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  echo "version=$new" >> "$GITHUB_OUTPUT"
fi
