# Configura PATH para compilar HULK en Windows (lexer/parser/eval).
# Uso: . .\scripts\setup_build_env.ps1

$ErrorActionPreference = "Stop"

$flexWin = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe"
$strawberry = "C:\Strawberry\c\bin"

$ghcupLlvm = Get-ChildItem "C:\ghcup\ghc\*\mingw\bin" -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName

$pathParts = @($strawberry) + ($env:PATH -split ';' | Where-Object { $_ })
if (Test-Path -LiteralPath $flexWin) {
    $pathParts = @($flexWin) + $pathParts
}
if ($ghcupLlvm) {
    $pathParts = @($ghcupLlvm) + $pathParts
}
$env:PATH = ($pathParts | Select-Object -Unique) -join ';'

Write-Host "Build env listo. Ejecuta: mingw32-make compile"
Write-Host "Codegen LLVM (Fase 4): mingw32-make test_llvm  (requiere llvm-config en PATH)"
