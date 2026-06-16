# Espejo de tests `ok/oop` — matcom/compilers

Copias locales de los 11 tests obligatorios de OOP del CI de [matcom/compilers](https://github.com/matcom/compilers/tree/main/tests/hulk/ok/oop).

| Directorio | Contenido |
|------------|-----------|
| `*.hulk` | Fuente **exacta** de matcom (`type` / `inherits` / métodos con `{ }`) |
| `dialect/*.hulk` | Traducción manual al dialecto local (`class` / `is` / `=>`) para medir capacidad real del compilador |


**Ejecución manual:**

```powershell
.\hulk_c.exe tests\matcom\oop\basic_class.hulk --semantic
.\hulk_c.exe tests\matcom\oop\dialect\basic_class.hulk --interpret
```
