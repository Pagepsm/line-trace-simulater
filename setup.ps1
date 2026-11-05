$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    python -m pip install --target .tools cmake==4.4.4 ninja==1.13.2
    if ($LASTEXITCODE) { throw 'Build tool setup failed' }
    $dependencies = @(
        @('box2d','https://github.com/erincatto/box2d.git','v3.1.1','8c661469c9507d3ad6fbd2fea3f1aa71669c2fe3'),
        @('raylib','https://github.com/raysan5/raylib.git','6.2','02566453880e06bec97f5b8160fa6fe1e98397ca'),
        @('imgui','https://github.com/ocornut/imgui.git','v1.93.0','c1aaac65e896a45f81f47a4d68f9959e0f86c859'),
        @('rlImGui','https://github.com/raylib-extras/rlImGui.git','','1550009359ad975927f7f0e4a3f47e3f27123ea9'),
        @('json','https://github.com/nlohmann/json.git','v3.12.0','55f93686c01528224f448c19128836e7df245f72')
    )
    New-Item -ItemType Directory -Force vendor | Out-Null
    foreach ($dep in $dependencies) {
        $target = Join-Path 'vendor' $dep[0]
        if (-not (Test-Path -LiteralPath $target)) {
            if ($dep[2]) { git clone --depth 1 --branch $dep[2] $dep[1] $target }
            else { git clone $dep[1] $target }
            if ($LASTEXITCODE) { throw "Clone failed: $target" }
            git -C $target checkout --detach $dep[3]
            if ($LASTEXITCODE) { throw "Checkout failed: $target" }
        }
        $actual = git -C $target rev-parse HEAD
        if ($LASTEXITCODE -or $actual -ne $dep[3]) { throw "Dependency revision mismatch: $target" }
    }
} finally { Pop-Location }
