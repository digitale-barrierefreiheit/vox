# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Runs Vox's local CI gates in parallel from Windows — format-check, build+coverage and
# tidy — each as a background `just` job, then reports pass/fail. `just` has no native
# parallel dependencies and runs each recipe line in a separate shell, so this
# orchestration lives in a script rather than a dense one-liner. Invoked by the justfile
# `check` recipe on Windows.
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$RepoNative
)

$ErrorActionPreference = 'Stop'

# coverage builds first and runs the suite under OpenCppCoverage; tidy is independent.
$targets = @('format-check', 'coverage', 'tidy')
$jobs = foreach ($target in $targets) {
    Start-Job -Name $target -ArgumentList $RepoNative, $target -ScriptBlock {
        param($repo, $name)
        Set-Location $repo
        & just $name
        if ($LASTEXITCODE -ne 0) { throw "just $name failed (exit $LASTEXITCODE)" }
    }
}

$jobs | Wait-Job | Out-Null
# -ErrorAction Continue: a failed gate's error must not terminate (ErrorActionPreference
# is Stop) before we print the summary and clean up; the .State check drives the exit.
$jobs | ForEach-Object { Receive-Job $_ -ErrorAction Continue }
$failed = @($jobs | Where-Object { $_.State -ne 'Completed' })
$jobs | Remove-Job -Force

if ($failed.Count) {
    Write-Host "check: FAILED ($($failed.Name -join ', '))"
    exit 1
}
Write-Host 'check: all gates passed'
