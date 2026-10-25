# Puts this project into a TriAevum checkout: checks out the pinned upstream
# commit on a new branch, copies overlay/ in and applies patches/triaevum.patch.
#
#   tools/setup.ps1 -TriAevum <path> [-Branch archipelago]
#
# The path is cloned from UPSTREAM's repository first if it does not exist.
# Build afterwards as described in docs/BUILD.md.
param(
    [Parameter(Mandatory = $true)][string]$TriAevum,
    [string]$Branch = "archipelago"
)
# git reports progress on stderr, which Windows PowerShell turns into errors;
# failures are caught through the exit codes instead.
$ErrorActionPreference = "Continue"

$root = Split-Path -Parent $PSScriptRoot
$upstream = Get-Content (Join-Path $root "UPSTREAM")
$repository = ($upstream | Where-Object { $_ -match '^repository ' }) -replace '^repository\s+', ''
$commit = ($upstream | Where-Object { $_ -match '^commit ' }) -replace '^commit\s+', ''

if (-not (Test-Path $TriAevum)) {
    git clone $repository $TriAevum
    if ($LASTEXITCODE -ne 0) { throw "clone failed" }
}
if (git -C $TriAevum status --porcelain) { throw "$TriAevum has uncommitted changes; commit or stash them first" }
git -C $TriAevum fetch -q origin $commit
if ($LASTEXITCODE -ne 0) { throw "cannot fetch $commit" }
git -C $TriAevum switch -c $Branch $commit
if ($LASTEXITCODE -ne 0) { throw "cannot create branch $Branch at $commit" }

Copy-Item -Recurse -Force (Join-Path $root "overlay\*") $TriAevum
git -C $TriAevum apply --whitespace=nowarn (Join-Path $root "patches\triaevum.patch")
if ($LASTEXITCODE -ne 0) { throw "the patch does not apply to $commit" }
Write-Host "TriAevum at $TriAevum is on branch $Branch with the Archipelago work applied (not committed)."
