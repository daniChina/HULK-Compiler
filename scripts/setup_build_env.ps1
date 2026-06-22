# Configura PATH para compilar HULK en Windows (lexer/parser/eval/codegen LLVM).
# Uso: . .\scripts\setup_build_env.ps1
#
# Requiere MSYS2 UCRT64 con llvm-21, clang y gcc del mismo prefix.
# Ver REQUIREMENTS.md.

$ErrorActionPreference = "Stop"

$flexWin = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe"
$strawberry = "C:\Strawberry\c\bin"

# MSYS2 UCRT64 — única fuente LLVM 21 + MinGW (no GHCup LLVM).
$msysRoots = @("C:\msys64", "C:\tools\msys64")
$ucrt64Bin = $null
$msysUsrInclude = $null
foreach ($root in $msysRoots) {
    $candidate = Join-Path $root "ucrt64\bin"
    if (Test-Path -LiteralPath $candidate) {
        $ucrt64Bin = $candidate
        $msysUsrInclude = Join-Path $root "usr\include"
        break
    }
}

$pathParts = @($strawberry) + ($env:PATH -split ';' | Where-Object { $_ })
if ($ucrt64Bin) {
    $pathParts = @($ucrt64Bin) + $pathParts
}
if (Test-Path -LiteralPath $flexWin) {
    $pathParts = @($flexWin) + $pathParts
}
$env:PATH = ($pathParts | Select-Object -Unique) -join ';'

function Test-ToolchainCommand {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        Write-Warning "$Name no encontrado en PATH."
        return $false
    }
    return $true
}

Write-Host "Build env listo. Ejecuta: mingw32-make compile"
Write-Host "Codegen LLVM (Fase 4): mingw32-make test_llvm  (requiere llvm-config 21.1.x en PATH)"

if (-not $ucrt64Bin) {
    Write-Warning "MSYS2 UCRT64 no detectado (C:\msys64\ucrt64\bin). Instala mingw-w64-ucrt-x86_64-llvm-21."
} else {
    Write-Host "UCRT64: $ucrt64Bin"
}

foreach ($tool in @("g++", "mingw32-make", "llvm-config", "clang")) {
    if (Test-ToolchainCommand $tool) {
        if ($tool -eq "llvm-config") {
            $ver = & llvm-config --version 2>$null
            Write-Host "  llvm-config --version = $ver"
            if ($ver -notmatch "^21\.1\.") {
                Write-Warning "Se espera LLVM 21.1.x (REQUIREMENTS.md). Encontrado: $ver"
            }
        }
    }
}
