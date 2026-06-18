# Compila y ejecuta Parser/tests/is_as_expr_pipeline_smoke.cpp con las mismas rutas que el Makefile.
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$FlexWin = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe"
$FlexHeader = Join-Path $FlexWin "FlexLexer.h"

if (-not (Test-Path $FlexHeader)) {
    Write-Error @"
No se encontró FlexLexer.h en:
  $FlexHeader

Instala WinFlexBison (winget install WinFlexBison.win_flex_bison) o ajusta FLEX_WIN en el Makefile.
"@
}

$CXXFLAGS = @(
    "-std=c++17", "-Wall", "-fuse-ld=bfd",
    "-I.", "-ILexer", "-IParser/core", "-IParser/ast", "-IParser/generator", "-IParser/syntax",
    "-ISemanticCheck", "-ITypes",
    "-IC:/msys64/usr/include",
    "-I$FlexWin"
)

$SOURCES = @(
    "Lexer/hulk_lexer.cpp",
    "Parser/core/token_adapter.cpp", "Parser/core/token_stream.cpp",
    "Parser/ast/expr.cpp", "Parser/ast/cst_nodes.cpp", "Parser/ast/cst_to_ast.cpp",
    "Parser/generator/grammar_reader.cpp", "Parser/generator/first_follow.cpp",
    "Parser/generator/ll1_table.cpp", "Parser/syntax/ll1_parser.cpp",
    "SemanticCheck/binding_list.cpp", "Types/type_info.cpp",
    "Parser/tests/is_as_expr_pipeline_smoke.cpp"
)

$Out = "is_as_smoke.exe"

Write-Host "Compilando $Out ..."
& g++ @CXXFLAGS @SOURCES -o $Out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Ejecutando $Out ..."
& ".\$Out"
exit $LASTEXITCODE
