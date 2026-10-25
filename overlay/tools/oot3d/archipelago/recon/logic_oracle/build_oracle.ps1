# Builds oot3d_logic_oracle.exe from an OoT3D_Randomizer checkout (MIT) and
# the driver in this directory. Objects are rebuilt only when their source or
# a header in the checkout is newer.
#
#   build_oracle.ps1 -Randomizer <OoT3D_Randomizer checkout> -Out <build dir>
param(
    [Parameter(Mandatory = $true)][string]$Randomizer,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Clang = "C:\Program Files\LLVM\bin\clang++.exe"
)
$ErrorActionPreference = "Stop"

$here = $PSScriptRoot
$source = Join-Path $Randomizer "source"
# Menu, music, console patching and presets need the 3DS; the logic does not.
# oracle_main.cpp stands in for the few of their symbols the rest links to.
$skip = "main", "menu", "music", "patch", "preset", "debug"
$sources = @(Get-ChildItem (Join-Path $source "*.cpp"), (Join-Path $source "location_access\*.cpp"),
        (Join-Path $source "gold_skulltulas\*.cpp"), (Join-Path $source "hint_list\*.cpp") |
    Where-Object { $skip -notcontains $_.BaseName })
$sources += Get-ChildItem (Join-Path $Randomizer "shared\*.c")
$sources += Get-Item (Join-Path $here "oracle_main.cpp")

$include = @("-I$here\stub", "-I$Randomizer\include", "-I$source", "-I$source\location_access",
    "-I$Randomizer\shared", "-I$source\gold_skulltulas", "-I$source\hint_list", "-I$source\custom_music")
$flags = @("-std=gnu++17", "-O1", "-w", "-D_CRT_SECURE_NO_WARNINGS")

New-Item -ItemType Directory -Force $Out | Out-Null
$newestHeader = (Get-ChildItem $source, (Join-Path $here "stub") -Recurse -Include *.hpp, *.h |
    Measure-Object LastWriteTime -Maximum).Maximum

$objects = @()
foreach ($file in $sources) {
    $object = Join-Path $Out ($file.BaseName + ".o")
    $objects += $object
    if ((Test-Path $object) -and (Get-Item $object).LastWriteTime -gt $file.LastWriteTime -and
        (Get-Item $object).LastWriteTime -gt $newestHeader) {
        continue
    }
    Write-Host "compile $($file.Name)"
    # The shared tables are C, but the MSVC ABI mangles C++ variable names:
    # compile them as C++ with their extern declarations in scope, which also
    # keeps the consts' linkage external.
    $languageFlags = if ($file.Extension -eq ".c") { @("-x", "c++", "-include", "s_dungeon.h") + $flags } else { $flags }
    & $Clang @languageFlags @include -c $file.FullName -o $object
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $($file.Name)" }
}
$exe = Join-Path $Out "oot3d_logic_oracle.exe"
& $Clang @objects -o $exe
if ($LASTEXITCODE -ne 0) { throw "link failed" }
Write-Host "built $exe"
