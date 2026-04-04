# ================================================================
# build.ps1  —  Script de compilacion automatica del lexer HULK
#
# Uso:
#   .\build.ps1              -> compila y corre prueba.hulk
#   .\build.ps1 otro.hulk   -> compila y corre otro archivo
# ================================================================

param(
    [string]$ArchivoHulk = "prueba.hulk"
)

$FLEX       = "flex++"
$GPP        = "g++"
$INCLUDES   = "-I `"C:\msys64\usr\include`""
$LEXER_L    = "hulk_lexer.l"
$LEXER_CPP  = "hulk_lexer.cpp"
$LEXER_O    = "hulk_lexer.o"
$MAIN_CPP   = "main.cpp"
$EXE        = "hulk_test.exe"

# ── Colores para mensajes ────────────────────────────────────
function Ok($msg)   { Write-Host "[OK] $msg"    -ForegroundColor Green  }
function Err($msg)  { Write-Host "[ERROR] $msg" -ForegroundColor Red    }
function Step($msg) { Write-Host "`n>>> $msg"   -ForegroundColor Cyan   }

# ── Verificar que los archivos fuente existen ────────────────
Step "Verificando archivos fuente..."
foreach ($f in @($LEXER_L, $MAIN_CPP)) {
    if (-not (Test-Path $f)) {
        Err "No se encuentra '$f'. Asegurate de estar en la carpeta correcta."
        exit 1
    }
}
Ok "Archivos fuente encontrados."

# ── Paso 1: flex++ ───────────────────────────────────────────
Step "Paso 1: Generando $LEXER_CPP con flex++..."
& $FLEX --c++ -o $LEXER_CPP $LEXER_L
if ($LASTEXITCODE -ne 0) {
    Err "flex++ fallo. Revisa los errores de arriba."
    exit 1
}
Ok "$LEXER_CPP generado."

# ── Paso 2: compilar objeto ──────────────────────────────────
Step "Paso 2: Compilando objeto $LEXER_O..."
$cmd2 = "$GPP -c $LEXER_CPP $INCLUDES -o $LEXER_O"
Invoke-Expression $cmd2
if ($LASTEXITCODE -ne 0) {
    Err "Compilacion del objeto fallo."
    exit 1
}
Ok "$LEXER_O compilado."

# ── Paso 3: compilar ejecutable ──────────────────────────────
Step "Paso 3: Compilando ejecutable $EXE..."
$cmd3 = "$GPP $LEXER_O $MAIN_CPP $INCLUDES -o $EXE"
Invoke-Expression $cmd3
if ($LASTEXITCODE -ne 0) {
    Err "Compilacion del ejecutable fallo."
    exit 1
}
Ok "$EXE compilado."

# ── Paso 4: correr prueba ────────────────────────────────────
Step "Paso 4: Corriendo prueba con '$ArchivoHulk'..."

if (-not (Test-Path $ArchivoHulk)) {
    Write-Host "  '$ArchivoHulk' no existe, creando archivo de prueba por defecto..." -ForegroundColor Yellow
    @"
let x = 42;
let saludo = "hola" @ " mundo";
if (x > 10) { print(saludo); }
function suma(a, b) => a + b;
let result := suma(1, 2);
true false PI E
/* comentario de bloque */
// comentario de linea
"@ | Set-Content $ArchivoHulk -Encoding UTF8
    Ok "'$ArchivoHulk' creado."
}

Write-Host ""
Write-Host "─────────────── Tokens encontrados ───────────────" -ForegroundColor Yellow
& ".\$EXE" $ArchivoHulk
if ($LASTEXITCODE -ne 0) {
    Err "El ejecutable termino con error."
    exit 1
}
Write-Host "──────────────────────────────────────────────────" -ForegroundColor Yellow

Write-Host ""
Ok "Todo listo."
