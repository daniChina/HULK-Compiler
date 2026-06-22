# Espejo de tests `ok/oop` — matcom/compilers

Copias locales de los 11 tests obligatorios de OOP del CI de [matcom/compilers](https://github.com/matcom/compilers/tree/main/tests/hulk/ok/oop).

| Directorio | Contenido |
|------------|-----------|
| `*.hulk` | Fuente **matcom** (`type` / `inherits` / métodos con `{ }`) — **usar estos** |
| `dialect/*.hulk` | ~~Traducción `class`/`is`~~ **obsoleta** (jun 2026) |

**Dialecto matcom:** `type` / `inherits` / métodos con `{ }` — ver [`REPORT.md`](../../../REPORT.md) § matcom.

**Ejecución:**

```bash
make build
make test_matcom_oop_parse          # 11/11 Parse OK

./hulk tests/matcom/oop/basic_class.hulk   # genera ./output
./output                                   # stdout (M2: aún con comillas en strings)

# Desarrollo
./hulk_c.exe tests/matcom/oop/basic_class.hulk --semantic
./hulk_c.exe tests/matcom/oop/basic_class.hulk --interpret
```
