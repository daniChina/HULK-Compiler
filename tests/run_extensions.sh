#!/usr/bin/env bash
# ==============================================================================
# Script de prueba para validar el procesamiento de extensiones en el compilador
# ==============================================================================
set -euo pipefail

# Asegurar que el PATH tenga las dependencias de MSYS2 si estamos en Windows
export PATH="/mingw64/bin:/usr/bin:$PATH"

echo "================================================================================"
echo " Ejecutando Pruebas E2E de Extensiones HULK (Lexer -> Parser -> AST)"
echo "================================================================================"

FAILED=0

for test_file in tests/extensions/valid/*.hulk; do
    echo -n "Probando [VALID] $(basename "$test_file")... "
    if ./hulk_c.exe --ast "$test_file" > /dev/null 2>&1; then
        echo "OK"
    else
        echo "FALLO"
        FAILED=$((FAILED + 1))
    fi
done

if [ "$FAILED" -eq 0 ]; then
    echo "================================================================================"
    echo " ¡Todas las pruebas de extensiones pasaron exitosamente!"
    echo "================================================================================"
    exit 0
else
    echo "================================================================================"
    echo " Errores detectados en las pruebas de extensiones: $FAILED fallo(s)"
    echo "================================================================================"
    exit 1
fi
