param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
      [string]$ToolchainBin = 'C:/msys64/ucrt64/bin')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$cmake = Join-Path $root '.tools\cmake\data\bin\cmake.exe'
$ninja = Join-Path $root '.tools\bin\ninja.exe'
$ToolchainBin = (Resolve-Path -LiteralPath $ToolchainBin).Path.Replace('\','/')
$env:PATH = "$ToolchainBin;$env:PATH"
& $cmake -S $root -B "$root\build\$Configuration" -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_C_COMPILER=$ToolchainBin/gcc.exe" "-DCMAKE_CXX_COMPILER=$ToolchainBin/g++.exe" "-DCMAKE_BUILD_TYPE=$Configuration"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
& $cmake --build "$root\build\$Configuration" --parallel 6
if ($LASTEXITCODE) { throw 'Build failed' }
