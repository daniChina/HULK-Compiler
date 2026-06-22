# Verifica toolchain HULK (Windows) — Gate A (LLVM 21.1.x)
# Uso: . .\scripts\setup_build_env.ps1; .\scripts\check_toolchain.ps1

$ErrorActionPreference = "Stop"
$fail = $false

function Require-Command {
    param([string]$Name, [scriptblock]$ExtraCheck)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        Write-Host "FAIL: $Name no encontrado en PATH" -ForegroundColor Red
        $script:fail = $true
        return
    }
    $path = $cmd.Source
    Write-Host "OK: $Name -> $path"
    if ($ExtraCheck) {
        & $ExtraCheck
        if (-not $?) { $script:fail = $true }
    }
}

Require-Command "g++" { g++ --version | Select-Object -First 1 }
Require-Command "mingw32-make" { mingw32-make --version | Select-Object -First 1 }
Require-Command "flex++" { flex++ --version | Select-Object -First 1 }
Require-Command "llvm-config" {
    $ver = llvm-config --version
    Write-Host "     llvm-config --version = $ver"
    if ($ver -notmatch "^21\.1\.") {
        Write-Host "FAIL: se espera LLVM 21.1.x (REQUIREMENTS.md)" -ForegroundColor Red
        $script:fail = $true
    }
}
Require-Command "clang" { clang --version | Select-Object -First 1 }

$llvmPaths = @(Get-Command llvm-config -All -ErrorAction SilentlyContinue | ForEach-Object { $_.Source })
if ($llvmPaths.Count -gt 1) {
    Write-Host "WARN: varios llvm-config en PATH:" -ForegroundColor Yellow
    $llvmPaths | ForEach-Object { Write-Host "  $_" }
}

if ($fail) {
    Write-Host "`nToolchain incompleto. Ver REQUIREMENTS.md." -ForegroundColor Red
    exit 1
}

Write-Host "`nToolchain OK (LLVM 21.1.x PATH-only)." -ForegroundColor Green
