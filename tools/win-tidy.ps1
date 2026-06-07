# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Runs Vox's clang-tidy gate from Windows. clang-tidy must match CI's toolchain
# (Ubuntu 24.04 + clang-18 + clang-tidy-18) — a newer clang/libstdc++ diverges
# (the analyzer can't parse a newer GCC's C++26 standard library). So this picks the
# first installed WSL distro that *is* Ubuntu 24.04 with the toolchain (any custom
# distro name) and runs the Linux `just tidy` there. If none is found it falls back to
# native clang-cl, which differs from CI and may fail on the bleeding-edge MSVC STL.
# Invoked by the justfile `tidy` recipe on Windows. $Base, when set, limits tidy to the
# C++ sources changed since the merge-base with that ref.
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$RepoNative,
    [string]$Base = ''
)

$ErrorActionPreference = 'Stop'
$env:WSL_UTF8 = '1'  # make wsl.exe emit UTF-8, not UTF-16, so output parses cleanly

# First installed WSL distro that is Ubuntu 24.04 and carries the tidy toolchain.
function Find-CiDistro {
    if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) { return $null }
    foreach ($line in (wsl --list --quiet)) {
        $name = $line.Trim()
        if (-not $name) { continue }
        $osRelease = (wsl -d $name -- cat /etc/os-release) -join "`n"
        if ($osRelease -notmatch '(?m)^ID=ubuntu\b') { continue }
        if ($osRelease -notmatch '(?m)^VERSION_ID="?24\.04') { continue }
        $hasToolchain = $true
        foreach ($tool in 'cmake', 'clang-18', 'clang-tidy-18', 'just') {
            wsl -d $name -- which $tool *> $null
            if ($LASTEXITCODE -ne 0) { $hasToolchain = $false; break }
        }
        if ($hasToolchain) { return $name }
    }
    return $null
}

$distro = Find-CiDistro
if ($distro) {
    Write-Warning "clang-tidy via WSL distro '$distro' (Ubuntu 24.04, matches the CI toolchain)."
    $recipe = if ($Base) { "just tidy $Base" } else { 'just tidy' }
    wsl -d $distro --cd $RepoNative -- bash -lc $recipe
    exit $LASTEXITCODE
}

Write-Warning ('No Ubuntu-24.04 WSL distro with the toolchain found; falling back to native ' +
    'clang-cl, which differs from CI and may fail on the MSVC STL. See README — install a ' +
    'WSL "Ubuntu-24.04" distro and the Linux toolchain to match the gate.')
cmake --preset x64-clang-cl
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Base) {
    $mergeBase = (git merge-base HEAD $Base).Trim()
    $files = @(git diff --name-only $mergeBase -- 'src/*.cpp' 'tests/*.cpp')
} else {
    $files = @(git ls-files 'src/*.cpp' 'tests/*.cpp')
}
if ($files.Count -eq 0) { Write-Host 'tidy: no C++ sources to check'; exit 0 }
& clang-tidy -p build/x64-clang-cl -warnings-as-errors='*' $files
exit $LASTEXITCODE
