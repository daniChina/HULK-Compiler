# Ejecuta un archivo .hulk (Windows / PowerShell).
# Uso:
#   .\scripts\run_hulk.ps1                              # ejecutar (default)
#   .\scripts\run_hulk.ps1 -Pipeline playground\foo.hulk
#   .\scripts\run_hulk.ps1 -Lexer -Parse -SemanticOnly  # una fase
param(
    [string]$File = "",
    [switch]$Lexer,
    [switch]$Parse,
    [switch]$SemanticOnly,
    [switch]$Pipeline,
    [switch]$NoBuild,
    [switch]$NoAst,
    [switch]$NoCst
)

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Hulk = Join-Path $Root "hulk_c.exe"
$DefaultFile = Join-Path $Root "playground\hello.hulk"

function Resolve-HulkFile([string]$Path) {
    if (-not $Path) { $Path = $DefaultFile }
    elseif (-not [System.IO.Path]::IsPathRooted($Path)) {
        $Path = Join-Path (Get-Location).Path $Path
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Host "[ERROR] No se encontro el archivo .hulk: $Path" -ForegroundColor Red
        exit 1
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Ensure-HulkBuilt {
    if (Test-Path $Hulk) { return }
    if ($NoBuild) {
        Write-Host "[ERROR] No existe $Hulk. Compila primero: make compile" -ForegroundColor Red
        exit 1
    }
    Write-Host "Compilando hulk_c.exe ..." -ForegroundColor Yellow
    if (Get-Command make -ErrorAction SilentlyContinue) {
        make compile
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        Write-Host "[ERROR] No existe $Hulk y 'make' no esta en PATH." -ForegroundColor Red
        exit 1
    }
}

function Invoke-HulkStage {
    param(
        [string]$Title,
        [string]$FilePath,
        [string[]]$HulkArgs,
        [string]$StdoutLabel = "Salida",
        [switch]$StopOnFail
    )

    Write-Host ""
    Write-Host ("== {0} ==" -f $Title) -ForegroundColor Cyan
    Write-Host ""

    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    try {
        $allArgs = @($FilePath) + $HulkArgs
        $p = Start-Process -FilePath $Hulk `
            -ArgumentList $allArgs `
            -WorkingDirectory $Root `
            -RedirectStandardOutput $outFile `
            -RedirectStandardError $errFile `
            -Wait -PassThru -NoNewWindow

        $stdout = if (Test-Path $outFile) { Get-Content -Raw -Path $outFile -ErrorAction SilentlyContinue } else { "" }
        $stderr = if (Test-Path $errFile) { Get-Content -Raw -Path $errFile -ErrorAction SilentlyContinue } else { "" }

        if ($stdout -and $stdout.Trim().Length -gt 0) {
            Write-Host ("-- {0} --" -f $StdoutLabel) -ForegroundColor Green
            Write-Host $stdout.TrimEnd()
            Write-Host ""
        }

        if ($stderr -and $stderr.Trim().Length -gt 0) {
            Write-Host "-- Errores --" -ForegroundColor Red
            Write-Host $stderr.TrimEnd()
            Write-Host ""
        }

        if ($p.ExitCode -eq 0) {
            Write-Host "OK (exit 0)" -ForegroundColor Green
        } else {
            Write-Host "FALLO (exit $($p.ExitCode))" -ForegroundColor Red
            if ($StopOnFail) { exit $p.ExitCode }
        }

        return $p.ExitCode
    } finally {
        Remove-Item -Force -ErrorAction SilentlyContinue $outFile, $errFile
    }
}

# --- main ---

$File = Resolve-HulkFile $File
Set-Location $Root
Ensure-HulkBuilt

$phaseCount = @($Lexer, $Parse, $SemanticOnly, $Pipeline).Where({ $_ }).Count
if ($phaseCount -gt 1) {
    Write-Host "[ERROR] Usa solo uno: -Lexer, -Parse, -SemanticOnly, -Pipeline (o ninguno = ejecutar)." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "== HULK ==" -ForegroundColor White
Write-Host "Archivo: $File"

if ($Pipeline) {
    Write-Host "Modo:    pipeline (lexer -> parser -> semantico -> evaluador)"
    $code = 0
    $code = Invoke-HulkStage "1/4 Lexer" $File @("--tokens") "Tokens" -StopOnFail
    if ($code -ne 0) { exit $code }

    $parseArgs = @()
    if (-not $NoCst) { $parseArgs += "--cst" }
    if (-not $NoAst) { $parseArgs += "--ast" }
    if ($parseArgs.Count -eq 0) { $parseArgs = @("--cst", "--ast") }
    $code = Invoke-HulkStage "2/4 Parser" $File $parseArgs "CST / AST" -StopOnFail
    if ($code -ne 0) { exit $code }

    $code = Invoke-HulkStage "3/4 Analisis semantico" $File @("--semantic") "Resultado semantico" -StopOnFail
    if ($code -ne 0) { exit $code }

    $code = Invoke-HulkStage "4/4 Evaluador" $File @("--semantic", "--interpret") "Salida del programa" -StopOnFail
    exit $code
}

if ($Lexer) {
    Write-Host "Modo:    lexer"
    exit (Invoke-HulkStage "Lexer" $File @("--tokens") "Tokens")
}

if ($Parse) {
    Write-Host "Modo:    parser"
    $parseArgs = @()
    if (-not $NoCst) { $parseArgs += "--cst" }
    if (-not $NoAst) { $parseArgs += "--ast" }
    if ($parseArgs.Count -eq 0) { $parseArgs = @("--cst", "--ast") }
    exit (Invoke-HulkStage "Parser" $File $parseArgs "CST / AST")
}

if ($SemanticOnly) {
    Write-Host "Modo:    semantico"
    exit (Invoke-HulkStage "Analisis semantico" $File @("--semantic") "Resultado semantico")
}

Write-Host "Modo:    ejecutar (semantico + evaluador)"
exit (Invoke-HulkStage "Evaluador" $File @("--semantic", "--interpret") "Salida del programa")
