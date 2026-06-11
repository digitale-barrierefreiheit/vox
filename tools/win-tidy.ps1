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

# Validate the base ref up front. Positional args don't survive
# `wsl -- bash -lc <script> $0 $1`, so the WSL path interpolates $Base into the command;
# validating here keeps that injection-safe. The allowed set covers git revision syntax
# (HEAD~1, HEAD^, @{u}, origin/dev, SHAs) but excludes shell metacharacters (; & | $ ` ( )
# spaces, quotes, ...). The native path passes $Base to git as a separate argv (safe).
if ($Base -and $Base -notmatch '^[\w./@~^{}-]+$') {
    throw "Invalid base ref '$Base'; expected a git revision (no shell metacharacters)."
}

# Is the named WSL distro Ubuntu 24.04 (matching CI)?
function Test-IsUbuntu2404($name) {
    $osRelease = (wsl -d $name -- cat /etc/os-release) -join "`n"
    return ($osRelease -match '(?m)^ID=ubuntu\b') -and ($osRelease -match '(?m)^VERSION_ID="?24\.04')
}

# Does the named WSL distro carry the clang-tidy toolchain CI uses?
function Test-HasTidyToolchain($name) {
    foreach ($tool in 'cmake', 'clang-18', 'clang-tidy-18', 'just') {
        wsl -d $name -- which $tool *> $null
        if ($LASTEXITCODE -ne 0) { return $false }
    }
    return $true
}

# First installed WSL distro that is Ubuntu 24.04 and carries the toolchain.
function Find-CiDistro {
    if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) { return $null }
    foreach ($line in (wsl --list --quiet)) {
        $name = $line.Trim()
        if (-not $name) { continue }
        if (-not (Test-IsUbuntu2404 $name)) { continue }
        if (Test-HasTidyToolchain $name) { return $name }
    }
    return $null
}

$distro = Find-CiDistro
if ($distro) {
    Write-Warning "clang-tidy via WSL distro '$distro' (Ubuntu 24.04, matches the CI toolchain)."
    $recipe = if ($Base) { "just tidy $Base" } else { 'just tidy' }   # $Base validated above
    wsl -d $distro --cd $RepoNative -- bash -lc $recipe
    exit $LASTEXITCODE
}

Write-Warning ('No Ubuntu-24.04 WSL distro with the toolchain found; falling back to native ' +
    'clang-cl, which differs from CI and may fail on the MSVC STL. See README — install a ' +
    'WSL "Ubuntu-24.04" distro and the Linux toolchain to match the gate.')
cmake --preset x64-clang-cl -DVCPKG_MANIFEST_FEATURES=benchmarks -DVOX_BUILD_BENCHMARKS=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Base) {
    $mergeBase = git merge-base HEAD $Base
    if ($LASTEXITCODE -ne 0 -or -not $mergeBase) { throw "Cannot compute merge-base with '$Base'." }
    $files = @(git diff --name-only $mergeBase.Trim() -- 'src/*.cpp' 'tests/*.cpp' 'benchmarks/*.cpp')
} else {
    $files = @(git ls-files 'src/*.cpp' 'tests/*.cpp' 'benchmarks/*.cpp')
}
if ($files.Count -eq 0) { Write-Host 'tidy: no C++ sources to check'; exit 0 }
# Invoke via a variable: clang-tidy is an external executable, not a PowerShell cmdlet,
# so calling it by literal name trips Sonar's cmdlet-casing rule (S8642, false positive).
$clangTidy = 'clang-tidy'
& $clangTidy -p build/x64-clang-cl -warnings-as-errors='*' $files
exit $LASTEXITCODE
