# HULK Compiler — Architecture Report (draft for matcom M0)

> **Borrador jun 2026** — expandir/editar antes de renombrar a `REPORT.md` en la raíz.  
> Objetivo matcom: ≥ 2000 palabras describiendo arquitectura, decisiones, features y limitaciones.

---

## 1. Overview

HULK-Compiler is a from-scratch compiler for the **HULK** language developed for the MatCom compilers course. The pipeline implements a classic multi-phase design: lexical analysis, LL(1) parsing, two-phase semantic analysis, an optional tree-walking interpreter for development, and—since Phase 4—a LLVM IR code generator that produces native Linux x86_64 binaries via Clang.

The project targets the **matcom/compilers** interface contract: `make build` produces `./hulk`; `./hulk file.hulk` emits `./output` on success or structured diagnostics on stderr. Development mode exposes flags (`--interpret`, `--semantic`, `--ast`) without breaking the default matcom invocation.

---

## 2. Pipeline architecture

```
file.hulk
   → Lexer (Flex++)
   → Parser LL(1) (grammar.ll1 + table)
   → CST → AST (cst_to_ast)
   → Semantic Phase 2 (symbol table + type checker)
   → [matcom] LLVM codegen → .hulk_out.ll
   → clang + Codegen/runtime.c → ./output
   → [dev] Evaluator (--interpret)
```

### 2.1 Lexer

Implemented with **Flex++** (`Lexer/hulk_lexer.l` → `hulk_lexer.cpp`). Tokens follow matcom HULK syntax: identifiers, numeric/string/boolean literals, keywords (`let`, `in`, `if`, `while`, `type`, `inherits`, `function`, etc.), operators including `:=`, `==`, `@` (string coercion), and OO punctuation.

Unknown characters yield `TokenType::UNKNOWN`; the matcom pipeline (`Compiler/pipeline.cpp`) converts the first UNKNOWN into `(line,col) LEXICAL: unexpected character 'X'` without extra lexer noise on stderr.

### 2.2 Parser

**LL(1)** over a grammar file (`Parser/grammar/grammar.ll1`). Components:

| Module | Role |
|--------|------|
| `grammar_reader.cpp` | Reads productions |
| `first_follow.cpp` | FIRST/FOLLOW sets |
| `ll1_table.cpp` | Table + conflict detection |
| `ll1_parser.cpp` | Driven parser |
| `cst_to_ast.cpp` | CST → typed AST |

Parse errors throw `ParseError` with line/column; matcom mode formats `(line,col) SYNTACTIC: …`.

### 2.3 Semantic analysis

**Phase 2** (`SemanticCheck/phase2_checker.cpp`) runs after a declaration collection pass:

1. Register types (`type` / `inherits`) and functions.
2. Analyze class bodies and methods.
3. Multi-pass type inference for numeric/string/boolean pairs and OO constructs.
4. Validate `is` / `as`, method overrides, `base()` calls, and builtin signatures.

Errors map to `(line,col) SEMANTIC: message` with exit code **3**. Duplicate identical diagnostics are collapsed in `pipeline.cpp` for matcom output.

### 2.4 Interpreter (development)

`Evaluator/evaluator.cpp` implements a tree-walking evaluator used as the **oracle** for LLVM tests: `hulk --interpret` stdout must match `./output` stdout (`scripts/test_llvm_fixtures.sh`, 32 cases).

### 2.5 LLVM codegen (Phase 4, I0–I12)

`Codegen/llvm_codegen.cpp` — visitor over AST → LLVM IR via LLVM 21 C++ API.

| Iteration | Feature |
|-----------|---------|
| I0 | Empty `@main`, module init |
| I1 | Literals, `@PI`/`@E` globals |
| I2 | `let` / nested scopes (`alloca` + `store`) |
| I3 | Binary arithmetic |
| I4 | `print` → runtime C |
| I5 | `if` / `while` / blocks |
| I6 | Assignment `:=` |
| I7 | Global functions, recursion |
| I8 | Builtins (`sin`, `range`, `for`) |
| I9 | OO: `new`, attributes, methods |
| I10 | Inheritance, `base()`, `as` in methods |
| I11 | `is` / `as` polymorphic |
| I12 | Dynamic dispatch via `__hulk_rt_type__`, boxed equality, OO field types |

**E2E build:** `Codegen/output_build.cpp` writes `.hulk_out.ll`, invokes `clang` with `Codegen/runtime.c` (print, strings, `hulk_boxed_equals`, cast errors).

**Runtime:** `Codegen/runtime.c` — minimal C runtime linked into every binary.

**Matcom entry:** `Compiler/main.cpp` — no flags → `run_matcom()` → `compile_program` + `build_output_executable(..., "output")`.

---

## 3. Object-oriented model (codegen)

HULK OO uses matcom syntax (`type Name { … }`, `inherits`, methods with `{ }` bodies).

### 3.1 Instance layout

Each class becomes an LLVM struct `HulkInstance_ClassName` with:

- Runtime type tag (`__hulk_rt_type__` string) for dynamic dispatch without vtables.
- Fields as i8* boxed values or typed slots depending on declaration.

### 3.2 Dynamic dispatch (I12)

Method calls resolve the runtime type name, then branch to the correct `@hulk_meth_*` function. This avoids vtable complexity while supporting override along inheritance chains tested in matcom OOP fixtures.

### 3.3 `is` / `as`

I11/I12 lower to LLVM comparisons and guarded casts; failed `as` invokes runtime error helper (runtime-level, not compiler diagnostic).

### 3.4 Boxed equality

Cross-type comparisons use `hulk_boxed_equals` in the runtime for matcom-compatible `==` on objects and primitives.

---

## 4. Symbol table and types

`SymbolTable/` collects top-level declarations. `Types/type_info.cpp` models `Number`, `String`, `Boolean`, `Null`, object types, and unknown/inferred types. Semantic conformance uses `conformsTo` for subtyping along inheritance edges.

Builtins (`print`, `sin`, `range`, etc.) are pre-registered to match matcom expectations.

---

## 5. Build system

`Makefile` targets:

| Target | Purpose |
|--------|---------|
| `make compile` | Builds `hulk_c.exe` (+ LLVM if `llvm-config` 21.x in PATH) |
| `make build` | Linux: compiles `./hulk` with `-O2`; Windows: copies `hulk_c.exe` |
| `make test_llvm` | 24 steps: build + run I0–I11 smokes |
| `make test_llvm_fixtures` | 32 oracle cases (22 LLVM valid + 10 matcom OOP) |
| `make test_eval` / `test_eval_fixtures` | Interpreter regression |

LLVM discovery: `llvm-config` on PATH (`-DHULK_HAVE_LLVM`). Override: `make … LLVM_CONFIG=llvm-config-21`.

---

## 6. Testing strategy

### 6.1 Oracle rule

```
./hulk program.hulk && ./output   ≡   ./hulk program.hulk --interpret
```

Implemented in `scripts/test_llvm_fixtures.sh`.

### 6.2 Smoke tests

Each iteration I0–I11 has a dedicated C++ driver under `Codegen/tests/` that builds AST manually (no parser), generates IR or E2E binary, and asserts IR substrings or stdout bytes. Smokes compile with `-fno-exceptions` where LLVM flags require it.

### 6.3 matcom OOP mirror

`tests/matcom/oop/*.hulk` — 10 programs mirroring [matcom/compilers tests/hulk/ok/oop](https://github.com/matcom/compilers/tree/main/tests/hulk/ok/oop). Included in the 32-case oracle suite (I12 gate).

---

## 7. Design decisions

1. **LL(1) over LALR:** Predictable error locations, table visible for debugging, fits course grammar size.
2. **Dual mode CLI:** Single binary; absence of flags triggers matcom codegen path—no separate `hulk_codegen` binary.
3. **Interpreter as oracle:** Avoids maintaining expected-files for every LLVM test; `--interpret` is the reference semantics during development.
4. **LLVM 21:** Required API (`StringRef::starts_with`, opaque pointers migration paths). Ubuntu LTS validated with apt.llvm.org packages.
5. **Runtime in C:** Keeps codegen smaller; Clang links IR + runtime in one step matching course E2E pattern.
6. **No vtables:** Runtime type string dispatch reduces codegen surface for I12 while passing polymorphism tests.
7. **Scope safety (I10 bugfix):** `defineMethod` / `defineUserFunction` use `scopes_.resize(depth)` instead of `clear()` to avoid UAF when LLVM builder holds pointers into scope maps.

---

## 8. matcom contract compliance

| Requirement | Status (Ubuntu jun 2026) |
|-------------|--------------------------|
| `make build` → `./hulk` | OK (ELF, compiled from source on Linux) |
| Success → `./output` x86_64 | OK |
| Exit 1/2/3 + stderr `(line,col) TYPE:` | OK (lexer extra line removed; semantic deduped) |
| `REPORT.md` ≥ 2000 words | **Pending** — promote this draft |
| Runtime errors in `./output` | Separate from compiler CI |

Example lexical diagnostic:

```
(1,9) LEXICAL: unexpected character '$'
```

---

## 9. Ubuntu validation (jun 2026)

Validated on **Ubuntu 22.04 (jammy)**, g++ 11.4, LLVM **21.1.8** from apt.llvm.org.

| Gate | Result |
|------|--------|
| `make build` | `./hulk` ELF OK |
| `make test_llvm` | I0–I11 OK (~2.5 min) |
| `make test_llvm_fixtures` | 32/32 OK |
| `make test_eval` + fixtures | OK |
| `./hulk tests/matcom/oop/basic_class.hulk && ./output` | stdout `ok` ×3 |

See `scripts/setup_ubuntu_env.sh` and REQUIREMENTS.md § Ubuntu.

### 9.1 Divergences / pitfalls

- **Paths with spaces:** `llvm-config --cxxflags` breaks `make` if the LLVM prefix contains spaces; use paths without spaces or apt system install.
- **GitHub LLVM tarball:** Prebuilt `LLVM-*-Linux-X64.tar.xz` ships bitcode inside `.a` archives—not linkable with g++; use **llvm-21-dev** (.deb) instead.
- **clang-21 on jammy:** May need `--gcc-toolchain=/usr` and gcc's `include` for `stddef.h` when not installed via full apt meta-package; wrapper documented in setup script.
- **Smoke E2E on Linux:** Smokes invoke generated exe by name; `scripts/test_llvm.sh` adds `$ROOT` to PATH.

---

## 10. Limitations and future work

1. **Fase 5 / matcom bonus:** Not implemented (out of scope I0–I12).
2. **Diagnostic language:** Messages are Spanish; matcom examples use English—format matches, wording may differ.
3. **String printing:** Oracle tests accept interpreter quoting; matcom CI may differ on string stdout formatting (document if CI fails).
4. **Windows vs Linux:** `make build` copies on Windows; Linux compiles fresh—both supported.
5. **REPORT.md:** Must be finalized before issue submission.
6. **docs parity (E4/E5):** Optional checklist items in fase4 plan.

---

## 11. Repository layout

```
Lexer/          Flex lexer
Parser/         LL(1) parser + AST
SemanticCheck/  Phase 2 analyzer
SymbolTable/    Declarations
Types/          TypeInfo
Value/          Runtime values (interpreter)
Evaluator/      Tree-walk interpreter
Codegen/        LLVM backend + runtime.c
Compiler/       main, pipeline, matcom diagnostics
tests/          llvm valid, matcom oop, eval fixtures
scripts/        test runners, ubuntu env setup
```


---

## 12. How to reproduce (human checklist)

```bash
# 1. Toolchain (sudo once)
sudo apt update
sudo apt install -y build-essential flex bison clang-21 llvm-21-dev
export LLVM_CONFIG=llvm-config-21
export PATH=/usr/lib/llvm-21/bin:$PATH

# 2. Build
cd HULK-Compiler
make compile && make build

# 3. Gates
make test_llvm
make test_llvm_fixtures
make test_eval && make test_eval_fixtures

# 4. Manual
./hulk tests/matcom/oop/basic_class.hulk && ./output
```

**Do not** open matcom issue until `REPORT.md` is complete and pushed.

---

*End of draft — expand sections 2–3 with code citations and diagrams as needed for final REPORT.md.*

---

## Appendix A — Semantic phases in detail

The semantic analyzer is intentionally split into **collection** and **checking** passes so OO declarations can reference types not yet fully analyzed (forward references within the same compilation unit).

**Collection (`decl_collector.cpp`):** Walks top-level statements and registers function signatures (with arity overloading by parameter count), class names, inheritance edges, attribute names, and method signatures in the symbol table. Builtin functions and types are injected before user code.

**Class pass:** For each `type` declaration, the analyzer validates that `inherits` targets exist, detects cycles, registers attributes in the class scope, and analyzes method bodies in a scope where `self` is typed as the current class. Method override compatibility checks compare return types and parameter lists against parent methods.

**Inference passes:** HULK allows omitted type annotations in some positions (locals via `let`, inferred attribute types in later matcom versions). The analyzer runs up to ten fixed-point passes over function and method bodies until types stabilize or an error is reported. Numeric, string, and boolean binary operators each have propagation rules that assign types to untyped identifier references when the other operand is known.

**Call resolution:** Function calls resolve by name and argument count. Method calls (`obj.method(...)`) and functional-style `obj.method` references share the same underlying lookup in the symbol table's per-type method map. Dynamic dispatch is **not** a semantic concern—types are checked statically; runtime dispatch is codegen's responsibility (I12).

**`is` and `as`:** Type tests generate boolean types; casts produce the target type on success. Semantic errors on impossible casts are preferred at compile time when the operand type is known; otherwise codegen emits runtime checks.

---

## Appendix B — LLVM codegen internals

**Module initialization:** `LLVMCodeGenerator::initialize` creates an LLVM module, a `LLVMContext`, an `IRBuilder`, and declares runtime functions (`hulk_print_double`, `hulk_print_string`, etc.) with consistent signatures before any user code.

**Name stability:** LLVM `Function::Create` requires stable `const char*` names; the codegen keeps literal name strings in `std::string` storage before passing `.c_str()` to LLVM, avoiding dangling pointers from temporary strings.

**IR extraction:** `getIR()` serializes the module to a `SmallString` buffer via `raw_svector_ostream`, then returns a `std::string` copy—avoiding `Module::print` directly to `std::string` pitfalls.

**Control flow:** `if` and `while` lower to basic blocks with explicit merge blocks; boolean conditions are evaluated as i1. Short-circuit logical operators are handled in the AST visitor before LLVM lowering.

**Functions:** User functions become private LLVM functions `@hulk_fn_<name>_<arity>`. Tail recursion is not optimized specially; correctness follows interpreter semantics. Return paths must leave a value on the stack or use `ret void` for statement-only bodies as HULK defines.

**OO codegen:** `new TypeName(...)` allocates a heap struct (via runtime helper or malloc wrapper), initializes the type tag, runs constructor expressions for attributes, and returns an i8* or typed pointer per the lowering strategy. Attribute get/set use GEP on the struct type derived from static type when known, with runtime helpers for dynamic cases.

**Inheritance:** Subclass structs include or embed parent layout; `base()` calls delegate to parent method implementations by resolving the parent portion of `self`. The I10 UAF fix ensures nested scope maps for method bodies remain valid when the IR builder recursively defines nested functions.

---

## Appendix C — Error handling philosophy

HULK-Compiler follows matcom's **first error wins by phase priority** rule:

1. Lexical (exit 1) — scan stops at first UNKNOWN token.
2. Syntactic (exit 2) — parser throws on first unexpected token.
3. Semantic (exit 3) — all semantic errors may be listed, but exit code is 3 if any exist.

Development mode (`--semantic`, `--interpret`) may print additional human-readable traces (e.g. `Semantic OK`, evaluator errors) on stderr/stdout; matcom mode suppresses these.

Codegen failures (LLVM verify errors, clang link failures) return exit **3** with `(0,0) SEMANTIC: code generation failed` or a more specific message from `output_build.cpp`.

---

## Appendix D — Relationship to matcom CI

When the human opens a **Project Submission** issue on matcom/compilers, CI will:

1. Clone the public HULK-Compiler repository at the pinned commit.
2. Run `make build` on Ubuntu LTS.
3. Verify `REPORT.md` word count.
4. Run the upstream test suite (including OOP cases beyond the local 10-file mirror).
5. Post results; `/regrade` triggers re-run after fixes.

Local gates (`make test_llvm_fixtures`) are **stricter in oracle style** but **narrower in coverage** than full CI. Passing 32/32 locally is necessary but not sufficient—CI may include edge cases not mirrored locally.

---

## Appendix E — Team workflow and documentation



---

## Appendix F — Glossary

| Term | Meaning |
|------|---------|
| CST | Concrete syntax tree from parser |
| AST | Abstract syntax tree for semantic/codegen |
| Oracle | `--interpret` stdout as reference |
| matcom mode | `./hulk file.hulk` without flags |
| Smoke | Standalone C++ test driver for one iteration |
| Boxed value | Runtime representation allowing uniform comparison |

---

*Word target: ≥ 2000 for final REPORT.md — this appendix block completes the draft length requirement.*
