# Prints the structure offsets of probe.c, computed by clang for the 3DS
# (ARMv6K, hard float) from an OoT3D_Randomizer checkout's z3D headers.
#
#   guest_layout.ps1 -Randomizer <OoT3D_Randomizer checkout>
param(
    [Parameter(Mandatory = $true)][string]$Randomizer,
    [string]$Clang = "C:\Program Files\LLVM\bin\clang.exe"
)
$here = $PSScriptRoot
$code = Join-Path $Randomizer "code"
$ir = Join-Path ([IO.Path]::GetTempPath()) "triaevum_guest_layout.ll"
# inc/ stands in for newlib headers the z3D headers include but do not use.
& $Clang --target=armv6k-none-eabi -mfloat-abi=hard -std=gnu11 -w `
    "-I$code\include" "-I$code\src" "-I$Randomizer\shared" "-I$here\inc" `
    "-I$here\..\logic_oracle\stub" -S -emit-llvm -o $ir (Join-Path $here "probe.c")
if ($LASTEXITCODE -ne 0) { throw "clang failed" }
Select-String -Path $ir -Pattern '^@(\w+) = .*constant i32 (\d+)' | ForEach-Object {
    "{0,-28} 0x{1:X}" -f $_.Matches[0].Groups[1].Value, [int]$_.Matches[0].Groups[2].Value
}
Remove-Item $ir
