# Ejecuta un comando largo mostrando tiempo transcurrido cada 15 s (PowerShell).
# Uso: .\scripts\run_with_timer.ps1 -Label compile { mingw32-make compile }
param(
    [string]$Label = "build",
    [Parameter(Mandatory = $true, Position = 0)]
    [scriptblock]$Command
)

$ErrorActionPreference = "Continue"
$sw = [System.Diagnostics.Stopwatch]::StartNew()
Write-Host "[$Label] iniciando (operacion larga; actualizacion cada 15 s)..."

$timer = [System.Threading.Timer]::new({
    param($state)
    $elapsed = $state.Stopwatch.Elapsed
    $mins = [int]$elapsed.TotalMinutes
    $secs = $elapsed.Seconds
    Write-Host ("[$($state.Label)] {0}:{1:D2} transcurrido..." -f $mins, $secs)
}, @{ Stopwatch = $sw; Label = $Label }, 15000, 15000)

try {
    & $Command
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) { $exitCode = 0 }
} finally {
    $timer.Dispose()
}

$sw.Stop()
$mins = [int]$sw.Elapsed.TotalMinutes
$secs = $sw.Elapsed.Seconds
if ($exitCode -eq 0) {
    Write-Host ("[$Label] terminado OK en {0}:{1:D2}" -f $mins, $secs) -ForegroundColor Green
} else {
    Write-Host ("[$Label] fallo (exit $exitCode) tras {0}:{1:D2}" -f $mins, $secs) -ForegroundColor Red
}
exit $exitCode
