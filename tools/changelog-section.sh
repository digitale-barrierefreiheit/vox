#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
#
# Print one version's body from a Keep-a-Changelog file: the lines between the
# "## [<version>] …" heading and the next "## [" heading or the link-reference
# block (with leading blank lines stripped). Used for the release PR description
# and the GitHub release notes (#103).
#
# Usage: changelog-section.sh <version> [changelog-path]
set -euo pipefail
ver="${1:?usage: changelog-section.sh <version> [changelog-path]}"
file="${2:-CHANGELOG.md}"

awk -v v="$ver" '
  index($0, "## [" v "]") == 1 { f = 1; next }
  f && (/^## \[/ || /^\[[^]]+\]: /) { exit }
  f { print }
' "$file" | sed -e '/./,$!d'
