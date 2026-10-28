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

# Locate the libVLC headers (folder that contains "vlc\vlc.h"). The VLC win64 *zip* ships no SDK headers,
# so if they are missing we fetch them from the matching VLC source release.
function Find-VlcInclude($dir) {
  $h = Get-ChildItem -Path $dir -Recurse -Filter vlc.h -ErrorAction SilentlyContinue |
       Where-Object { $_.Directory.Name -eq "vlc" } | Select-Object -First 1
  if ($h) { return $h.Directory.Parent.FullName }
  return $null
}
$vlcInc = Find-VlcInclude $VlcDir
if (-not $vlcInc) {
  $ver = ((Split-Path $VlcDir -Leaf) -replace '^vlc-', '')
  if ($ver -notmatch '^\d+\.\d+\.\d+$') { throw "Cannot work out the VLC version from folder name '$VlcDir'" }
  $maj, $min, $rev = $ver.Split('.')
  $incRoot = Join-Path $VlcDir "sdk\include"
  $tmp = Join-Path $env:TEMP "vlc-src-$ver"
  New-Item -ItemType Directory -Force $tmp, $incRoot | Out-Null
  $tarball = Join-Path $tmp "vlc-$ver.tar.xz"
  $ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36"
  $got = $false
  foreach ($h in "https://download.videolan.org/pub/videolan/vlc", "https://get.videolan.org/vlc", "https://ftp.fau.de/videolan/vlc", "https://mirror.netcologne.de/videolan/vlc") {
    Write-Host "Fetching libVLC headers from $h/$ver/vlc-$ver.tar.xz"
    curl.exe -L --fail --retry 3 --retry-delay 2 -A $ua -o $tarball "$h/$ver/vlc-$ver.tar.xz"
    if ($LASTEXITCODE -eq 0 -and (Get-Item $tarball).Length -gt 10MB) { $got = $true; break }
  }
  if (-not $got) { throw "Could not download the VLC $ver source tarball for its headers" }
  Write-Host "Extracting headers from the tarball..."
  New-Item -ItemType Directory -Force "$incRoot\vlc" | Out-Null
  $py = @'
import sys, tarfile
tar_path, prefix, dest = sys.argv[1], sys.argv[2], sys.argv[3]
n = 0
with tarfile.open(tar_path, "r:xz") as t:
    for m in t:
        if m.isfile() and m.name.startswith(prefix):
            m.name = m.name[len(prefix):]
            t.extract(m, dest); n += 1
print("extracted", n, "header files")
if n == 0: sys.exit(1)
'@
  $pyFile = Join-Path $tmp "extract_headers.py"
  Set-Content -Path $pyFile -Value $py -Encoding ascii
  python $pyFile $tarball "vlc-$ver/include/vlc/" "$incRoot\vlc"
  if ($LASTEXITCODE -ne 0) { throw "Header extraction failed (is Python installed?)" }
  $src = "$incRoot\vlc"
  # libvlc_version.h is generated from a template in the source tree.
  $tpl = Join-Path $src "libvlc_version.h.in"
  if (Test-Path $tpl) {
    $t = Get-Content $tpl -Raw
    $t = $t.Replace("@VERSION_MAJOR@", $maj).Replace("@VERSION_MINOR@", $min).Replace("@VERSION_REVISION@", $rev).Replace("@VERSION_EXTRA@", "0").Replace("@VERSION@", $ver)
    if ($t -match '@[A-Z_]+@') { throw "Unresolved placeholder in libvlc_version.h.in: $($Matches[0])" }
    Set-Content -Path "$incRoot\vlc\libvlc_version.h" -Value $t -Encoding ascii
  }
  $vlcInc = Find-VlcInclude $VlcDir
  if (-not $vlcInc) { throw "libVLC headers still not found after download" }
}
Write-Host "libVLC headers: $vlcInc"

cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtDir" -DLIBVLC_ROOT="$VlcDir\sdk" `
  -DLIBVLC_INCLUDE_DIR="$vlcInc" -DLIBVLC_LIBRARY="$sdkLib\libvlc.lib" -DLIBVLCCORE_LIBRARY="$sdkLib\libvlccore.lib" -DLUMINA_BUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build $build
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "$build\lumina.exe" $dist
& "$QtDir\bin\windeployqt.exe" --release --qmldir "$root\qml" --no-translations --no-system-d3d-compiler "$dist\lumina.exe"
Copy-Item "$VlcDir\libvlc.dll","$VlcDir\libvlccore.dll" $dist
Copy-Item -Recurse "$VlcDir\plugins" "$dist\plugins"
Copy-Item "$root\LICENSE" $dist
Compress-Archive -Path "$dist\*" -DestinationPath "$root\dist\Lumina-portable.zip" -Force

# Locate Inno Setup's compiler even when it is not on PATH (Chocolatey/winget installs).
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
  $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
            "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if ($iscc) {
  $env:LUMINA_DIST = $dist
  & $iscc /DAppVersion=$Version "$PSScriptRoot\lumina.iss"
  if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
  Write-Host "Installer: $root\dist\Lumina-$Version-Setup.exe"
} else { Write-Warning "Inno Setup 6 not found - installer skipped; portable zip was created." }
