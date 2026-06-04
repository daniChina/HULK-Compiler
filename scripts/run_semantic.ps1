# Recorre tests/semantic/{valid,invalid}/ con hulk.exe --semantic (Windows / PowerShell).
param(
    [string]$Hulk = (Join-Path $PSScriptRoot "..\hulk.exe")
)

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ValidDir = Join-Path $Root "tests\semantic\valid"
$InvalidDir = Join-Path $Root "tests\semantic\invalid"
$Manifest = Join-Path $Root "tests\semantic\manifest.tsv"

if (-not (Test-Path $Hulk)) {
    Write-Error "[FAIL] No existe $Hulk - ejecuta: make compile"
    exit 1
}

$failures = 0
$ran = 0

function Run-Hulk([string]$File) {
    $out = & $Hulk $File --semantic 2>&1 | Out-String
    $code = $LASTEXITCODE
    if ($code -eq 0 -and ($out -match '== Semantic errors ==' -or $out -match 'Error de parseo')) {
        $code = 1
    }
    return @{ Output = $out; ExitCode = $code }
}

function Get-Expect([string]$Base) {
    foreach ($line in Get-Content $Manifest) {
        $line = $line.TrimEnd("`r")
        if ($line -match '^\s*#' -or [string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t", 3
        $path = $parts[0].Trim()
        if ($path -eq "invalid/$Base") {
            return @{ Kind = $parts[1].Trim(); Match = $parts[2].Trim() }
        }
    }
    return $null
}

foreach ($f in Get-ChildItem "$ValidDir\*.hulk" | Sort-Object Name) {
    $ran++
    $r = Run-Hulk $f.FullName
    if ($r.ExitCode -ne 0 -or $r.Output -notmatch "Semantic OK") {
        Write-Host ('[FAIL] valid/' + $f.Name)
        $failures++
    } else {
        Write-Host ('[OK] valid/' + $f.Name)
    }
}

foreach ($f in Get-ChildItem "$InvalidDir\*.hulk" | Sort-Object Name) {
    $ran++
    $exp = Get-Expect $f.Name
    if (-not $exp) {
        Write-Host ('[FAIL] invalid/' + $f.Name + ' - sin entrada en manifest.tsv')
        $failures++
        continue
    }
    $r = Run-Hulk $f.FullName
    if ($r.ExitCode -eq 0) {
        Write-Host ('[FAIL] invalid/' + $f.Name + ' - exit 0')
        $failures++
        continue
    }
    $ok = $true
    switch ($exp.Kind) {
        "semantic" {
            if ($r.Output -notmatch 'Semantic errors' -and $r.Output -notmatch 'Error Sem') {
                $ok = $false
            }
        }
        "parse" { if ($r.Output -notmatch "Error de parseo") { $ok = $false } }
        "any" { }
        default { $ok = $false }
    }
    if ($ok -and $exp.Match -and $r.Output -notmatch [regex]::Escape($exp.Match)) { $ok = $false }
    if ($ok) {
        Write-Host ('[OK] invalid/' + $f.Name + ' (' + $exp.Kind + ')')
    } else {
        Write-Host ('[FAIL] invalid/' + $f.Name)
        $failures++
    }
}

Write-Host ""
Write-Host "Casos ejecutados: $ran"
if ($failures -gt 0) {
    Write-Host "Fallos: $failures"
    exit 1
}
Write-Host "Todos los fixtures semanticos pasaron."
exit 0
