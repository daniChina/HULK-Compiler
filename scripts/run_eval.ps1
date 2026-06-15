# Recorre tests/eval/{valid,invalid}/ con hulk_c.exe --semantic --interpret (Windows / PowerShell).
param(
    [string]$Hulk = (Join-Path $PSScriptRoot "..\hulk_c.exe")
)

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ValidDir = Join-Path $Root "tests\eval\valid"
$InvalidDir = Join-Path $Root "tests\eval\invalid"
$Manifest = Join-Path $Root "tests\eval\manifest.tsv"

if (-not (Test-Path $Hulk)) {
    Write-Error "[FAIL] No existe $Hulk - ejecuta: make compile"
    exit 1
}

$failures = 0
$ran = 0

function Normalize-Lf([string]$Text) {
    if ($null -eq $Text) { return "" }
    return ($Text -replace "`r`n", "`n" -replace "`r", "`n")
}

function Unescape-Expected([string]$Text) {
    if ($null -eq $Text) { return "" }
    return ($Text -replace '\\n', "`n" -replace '\\t', "`t")
}

function Run-HulkEval([string]$File) {
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    try {
        $p = Start-Process -FilePath $Hulk `
            -ArgumentList @($File, "--semantic", "--interpret") `
            -RedirectStandardOutput $outFile `
            -RedirectStandardError $errFile `
            -Wait -PassThru -NoNewWindow
        $stdout = if (Test-Path $outFile) { Get-Content -Raw -Path $outFile -ErrorAction SilentlyContinue } else { "" }
        $stderr = if (Test-Path $errFile) { Get-Content -Raw -Path $errFile -ErrorAction SilentlyContinue } else { "" }
        if ($null -eq $stdout) { $stdout = "" }
        if ($null -eq $stderr) { $stderr = "" }
        return @{
            Stdout   = $stdout
            Stderr   = $stderr
            ExitCode = $p.ExitCode
        }
    } finally {
        Remove-Item $outFile, $errFile -ErrorAction SilentlyContinue
    }
}

function Get-ValidExpect([string]$Base) {
    foreach ($line in Get-Content $Manifest) {
        $line = $line.TrimEnd("`r")
        if ($line -match '^\s*#' -or [string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t", 3
        if ($parts.Count -lt 2) { continue }
        $path = $parts[0].Trim()
        if ($path -eq "valid/$Base") {
            return $parts[1]
        }
    }
    return $null
}

function Get-InvalidExpect([string]$Base) {
    foreach ($line in Get-Content $Manifest) {
        $line = $line.TrimEnd("`r")
        if ($line -match '^\s*#' -or [string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t", 3
        if ($parts.Count -lt 2) { continue }
        $path = $parts[0].Trim()
        if ($path -eq "invalid/$Base") {
            return @{ Kind = $parts[1].Trim(); Match = $(if ($parts.Count -ge 3) { $parts[2].Trim() } else { "" }) }
        }
    }
    return $null
}

foreach ($f in Get-ChildItem "$ValidDir\*.hulk" -ErrorAction SilentlyContinue | Sort-Object Name) {
    $ran++
    $expRaw = Get-ValidExpect $f.Name
    if ($null -eq $expRaw) {
        Write-Host ('[FAIL] valid/' + $f.Name + ' - sin entrada en manifest.tsv')
        $failures++
        continue
    }
    $expected = Unescape-Expected $expRaw
    $r = Run-HulkEval $f.FullName
    $actual = Normalize-Lf $r.Stdout
    $expectedNorm = Normalize-Lf $expected
    if ($r.ExitCode -ne 0) {
        Write-Host ('[FAIL] valid/' + $f.Name + ' - exit ' + $r.ExitCode)
        if ($r.Stderr) { Write-Host $r.Stderr }
        $failures++
        continue
    }
    if ($actual -ne $expectedNorm) {
        Write-Host ('[FAIL] valid/' + $f.Name + ' - stdout distinto al manifest')
        Write-Host ('  esperado: ' + ($expectedNorm -replace "`n", '\n'))
        Write-Host ('  obtenido: ' + ($actual -replace "`n", '\n'))
        $failures++
        continue
    }
    Write-Host ('[OK] valid/' + $f.Name)
}

if (Test-Path $InvalidDir) {
    foreach ($f in Get-ChildItem "$InvalidDir\*.hulk" -ErrorAction SilentlyContinue | Sort-Object Name) {
        $ran++
        $exp = Get-InvalidExpect $f.Name
        if (-not $exp) {
            Write-Host ('[FAIL] invalid/' + $f.Name + ' - sin entrada en manifest.tsv')
            $failures++
            continue
        }
        $r = Run-HulkEval $f.FullName
        $combined = Normalize-Lf ($r.Stdout + $r.Stderr)
        if ($r.ExitCode -eq 0) {
            Write-Host ('[FAIL] invalid/' + $f.Name + ' - exit 0')
            $failures++
            continue
        }
        $ok = $true
        switch ($exp.Kind) {
            "runtime" {
                if ($combined -notmatch 'Error' -and $combined -notmatch 'espera') {
                    $ok = $false
                }
            }
            "semantic" {
                if ($combined -notmatch 'Semantic errors' -and $combined -notmatch 'Error Sem') {
                    $ok = $false
                }
            }
            "parse" {
                if ($combined -notmatch "Error de parseo") { $ok = $false }
            }
            "any" { }
            default { $ok = $false }
        }
        if ($ok -and $exp.Match -and $combined -notmatch [regex]::Escape($exp.Match)) { $ok = $false }
        if ($ok) {
            Write-Host ('[OK] invalid/' + $f.Name + ' (' + $exp.Kind + ')')
        } else {
            Write-Host ('[FAIL] invalid/' + $f.Name)
            $failures++
        }
    }
}

Write-Host ""
Write-Host "Casos ejecutados: $ran"
if ($failures -gt 0) {
    Write-Host "Fallos: $failures"
    exit 1
}
Write-Host "Todos los fixtures de eval pasaron."
exit 0
