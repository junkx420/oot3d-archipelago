# Refreshes overlay/ and patches/triaevum.patch from a TriAevum checkout
# that has the Archipelago work on a branch.
#
#   tools/export.ps1 -TriAevum <checkout> [-Ref feature/archipelago]
#
# Files the branch adds go to overlay/ under their TriAevum paths; changes to
# files TriAevum already has go into the patch. The upstream commit is read
# from UPSTREAM.
param(
    [Parameter(Mandatory = $true)][string]$TriAevum,
    [string]$Ref = "feature/archipelago"
)
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$base = (Get-Content (Join-Path $root "UPSTREAM") | Where-Object { $_ -match '^commit ' }) -replace '^commit\s+', ''
if (-not $base) { throw "UPSTREAM has no 'commit <sha>' line" }

$overlay = Join-Path $root "overlay"
if (Test-Path $overlay) { Remove-Item -Recurse -Force $overlay }
New-Item -ItemType Directory $overlay | Out-Null

$added = git -C $TriAevum diff --name-only --diff-filter=A $base $Ref
if ($LASTEXITCODE -ne 0) { throw "git diff failed" }
foreach ($path in $added) {
    $target = Join-Path $overlay $path
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    # Blob bytes as committed, so line endings match the branch exactly.
    $process = Start-Process git -ArgumentList @("-C", "`"$TriAevum`"", "cat-file", "blob", "${Ref}:$path") `
        -RedirectStandardOutput $target -NoNewWindow -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "cannot read $path" }
}

$patches = Join-Path $root "patches"
New-Item -ItemType Directory -Force $patches | Out-Null
$modified = git -C $TriAevum diff --name-only --diff-filter=M $base $Ref
$patch = Join-Path $patches "triaevum.patch"
$process = Start-Process git -ArgumentList (@("-C", "`"$TriAevum`"", "diff", "--binary", $base, $Ref, "--") + $modified) `
    -RedirectStandardOutput $patch -NoNewWindow -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "git diff for the patch failed" }

Write-Host "overlay: $(@($added).Count) files; patch: $(@($modified).Count) files"
