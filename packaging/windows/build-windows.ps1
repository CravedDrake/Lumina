<#
 Builds Lumina and produces dist\Lumina-<version>-Setup.exe (installer) and dist\Lumina-portable.zip.
 Run from a "x64 Native Tools Command Prompt for VS" PowerShell, with:
   -QtDir    e.g. C:\Qt\6.5.3\msvc2019_64
   -VlcDir   the extracted VLC 3.0.x win64 zip folder (contains libvlc.dll, plugins\, sdk\)
 Requires: CMake, Ninja (or VS generator), Inno Setup 6 (iscc.exe on PATH) for the installer.
#>
param(
  [Parameter(Mandatory=$true)][string]$QtDir,
  [Parameter(Mandatory=$true)][string]$VlcDir,
  [string]$Version = "0.1.0"
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\..\.."
$build = Join-Path $root "build-win"
$dist = Join-Path $root "dist\Lumina"

# MSVC import libraries are not always shipped in the VLC zip; create them from the DLLs if needed.
$sdkLib = Join-Path $VlcDir "sdk\lib"
New-Item -ItemType Directory -Force $sdkLib | Out-Null
foreach ($n in "libvlc","libvlccore") {
  if (-not (Test-Path "$sdkLib\$n.lib")) {
    Write-Host "Generating $n.lib from $n.dll"
    $dll = Join-Path $VlcDir "$n.dll"
    $syms = & dumpbin /exports $dll | Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value }
    ("LIBRARY $n", "EXPORTS") + $syms | Set-Content "$sdkLib\$n.def" -Encoding ascii
    & lib /nologo /def:"$sdkLib\$n.def" /out:"$sdkLib\$n.lib" /machine:x64
  }
}

cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtDir" -DLIBVLC_ROOT="$VlcDir\sdk" -DLUMINA_BUILD_TESTS=OFF
cmake --build $build
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "$build\lumina.exe" $dist
& "$QtDir\bin\windeployqt.exe" --release --qmldir "$root\qml" --no-translations --no-system-d3d-compiler "$dist\lumina.exe"
Copy-Item "$VlcDir\libvlc.dll","$VlcDir\libvlccore.dll" $dist
Copy-Item -Recurse "$VlcDir\plugins" "$dist\plugins"
Copy-Item "$root\LICENSE" $dist
Compress-Archive -Path "$dist\*" -DestinationPath "$root\dist\Lumina-portable.zip" -Force

if (Get-Command iscc -ErrorAction SilentlyContinue) {
  $env:LUMINA_DIST = $dist
  iscc /DAppVersion=$Version "$PSScriptRoot\lumina.iss"
  Write-Host "Installer: $root\dist\Lumina-$Version-Setup.exe"
} else { Write-Warning "iscc.exe (Inno Setup) not found - installer skipped; portable zip was created." }
