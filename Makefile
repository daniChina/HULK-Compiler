MATCOM_OOP_PARSE_TARGET = matcom_oop_parse_smoke

.PHONY: all compile compile_nollvm build lexer clean execute run run-pipeline run-lexer run-parse run-semantic test_types test_symbols test_semantic test_semantic_fixtures test_r1_semantic test_r2_semantic test_r3_r4_semantic test_a4_builtins test_type_map test_eval test_eval_fixtures test_is_as_smoke test_matcom_oop_parse test_llvm test_llvm_fixtures

# --- LLVM (Fase 4): compile enlaza libLLVM si llvm-config está en PATH (Fase D) ---
LLVM_CONFIG ?= llvm-config
ifeq ($(OS),Windows_NT)
# MSYS make usa sh: 2>NUL crea un archivo literal "NUL" (nombre reservado Windows).
LLVM_AVAILABLE := $(shell "$(LLVM_CONFIG)" --version 2>/dev/null)
LLVM_CXXFLAGS_RAW := $(shell "$(LLVM_CONFIG)" --cxxflags 2>/dev/null)
LLVM_LDFLAGS := $(shell "$(LLVM_CONFIG)" --ldflags --libs core support 2>/dev/null)
else
LLVM_AVAILABLE := $(shell "$(LLVM_CONFIG)" --version 2>/dev/null)
LLVM_CXXFLAGS_RAW := $(shell "$(LLVM_CONFIG)" --cxxflags 2>/dev/null)
LLVM_LDFLAGS := $(shell "$(LLVM_CONFIG)" --ldflags --libs core support 2>/dev/null)
endif
LLVM_CXXFLAGS := $(filter-out -stdlib=libc++,$(LLVM_CXXFLAGS_RAW))
LLVM_CXXFLAGS := $(filter-out -std=c++14,$(LLVM_CXXFLAGS))
LLVM_CXXFLAGS := $(filter-out -std=gnu++14,$(LLVM_CXXFLAGS))
# Mantener -fno-exceptions/-fno-rtti en test_llvm; hulk usa LLVM_CXXFLAGS_HULK_ALL.
LLVM_BINDIR := $(shell "$(LLVM_CONFIG)" --bindir 2>/dev/null)
ifneq ($(LLVM_BINDIR),)
LLVM_PATH_EXPORT = PATH="$(LLVM_BINDIR):$(PATH)"
else
LLVM_PATH_EXPORT =
endif

ifeq ($(OS),Windows_NT)
RUN_TIMER = C:/msys64/usr/bin/bash.exe scripts/run_with_timer.sh
else
RUN_TIMER = bash scripts/run_with_timer.sh
endif

CODEGEN_SOURCES = Codegen/llvm_codegen.cpp Codegen/llvm_aux.cpp Codegen/output_build.cpp
CODEGEN_I0_SOURCES = Codegen/tests/i0_module_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
LLVM_I0_TARGET = llvm_i0_smoke
LLVM_I1_TARGET = llvm_i1_smoke
LLVM_I2_TARGET = llvm_i2_smoke
LLVM_I3_TARGET = llvm_i3_smoke
LLVM_I4_TARGET = llvm_i4_smoke
LLVM_I5_TARGET = llvm_i5_smoke
LLVM_I6_TARGET = llvm_i6_smoke
LLVM_I7_TARGET = llvm_i7_smoke
LLVM_I8_TARGET = llvm_i8_smoke

# Compilador y flags
CXX = g++
# FlexLexer.h: MSYS2 usr/include o WinFlexBison (winget install WinFlexBison.win_flex_bison)
FLEX_WIN = $(LOCALAPPDATA)/Microsoft/WinGet/Packages/WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe
CXXFLAGS = -std=c++17 -Wall -fuse-ld=bfd -I. -ILexer -IParser/core -IParser/ast -IParser/generator -IParser/syntax \
           -ISemanticCheck -ISymbolTable -ITypes -IValue -IEvaluator -ICodegen \
           -I/usr/include -IC:/msys64/usr/include -I$(FLEX_WIN)
# Smokes: -fno-exceptions/-fno-rtti de llvm-config. hulk (main.cpp): excepciones habilitadas.
LLVM_CXXFLAGS_HULK := $(filter-out -fno-exceptions -fno-rtti,$(LLVM_CXXFLAGS))
LLVM_CXXFLAGS_ALL = -std=c++17 $(LLVM_CXXFLAGS)
LLVM_CXXFLAGS_HULK_ALL = -std=c++17 $(LLVM_CXXFLAGS_HULK)

ifneq ($(LLVM_AVAILABLE),)
HULK_HAVE_LLVM := 1
COMPILE_SOURCES = $(SOURCES) $(CODEGEN_SOURCES)
COMPILE_CXXFLAGS = $(CXXFLAGS) $(LLVM_CXXFLAGS_HULK_ALL) -DHULK_HAVE_LLVM
COMPILE_LDFLAGS = $(LLVM_LDFLAGS)
else
HULK_HAVE_LLVM :=
COMPILE_SOURCES = $(SOURCES)
COMPILE_CXXFLAGS = $(CXXFLAGS)
COMPILE_LDFLAGS =
endif

# Archivos fuente
SOURCES = Lexer/hulk_lexer.cpp \
          Parser/core/token_adapter.cpp \
          Parser/core/token_stream.cpp \
          Parser/ast/expr.cpp \
          Parser/ast/cst_nodes.cpp \
          Parser/ast/cst_to_ast.cpp \
          Parser/generator/grammar_reader.cpp \
          Parser/generator/first_follow.cpp \
          Parser/generator/ll1_table.cpp \
          Parser/syntax/ll1_parser.cpp \
          SemanticCheck/binding_list.cpp \
          SemanticCheck/phase2_checker.cpp \
          Types/type_info.cpp \
          SymbolTable/decl_collector.cpp \
          Value/value.cpp \
          Evaluator/evaluator.cpp \
          Compiler/pipeline.cpp \
          Compiler/output_gen.cpp \
          Compiler/main.cpp

OUTPUT_SOURCES = Lexer/hulk_lexer.cpp \
          Parser/core/token_adapter.cpp \
          Parser/core/token_stream.cpp \
          Parser/ast/expr.cpp \
          Parser/ast/cst_nodes.cpp \
          Parser/ast/cst_to_ast.cpp \
          Parser/generator/grammar_reader.cpp \
          Parser/generator/first_follow.cpp \
          Parser/generator/ll1_table.cpp \
          Parser/syntax/ll1_parser.cpp \
          SemanticCheck/binding_list.cpp \
          SemanticCheck/phase2_checker.cpp \
          Types/type_info.cpp \
          SymbolTable/decl_collector.cpp \
          Value/value.cpp \
          Evaluator/evaluator.cpp \
          Compiler/pipeline.cpp \
          Compiler/output_main.cpp

HULK_BIN = hulk

PARSER_TEST_COMMON = Lexer/hulk_lexer.cpp Parser/core/token_adapter.cpp Parser/core/token_stream.cpp \
          Parser/ast/expr.cpp Parser/ast/cst_nodes.cpp Parser/ast/cst_to_ast.cpp \
          Parser/generator/grammar_reader.cpp Parser/generator/first_follow.cpp \
          Parser/generator/ll1_table.cpp Parser/syntax/ll1_parser.cpp \
          SemanticCheck/binding_list.cpp Types/type_info.cpp

CODEGEN_I1_SOURCES = Codegen/tests/i1_literals_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I2_SOURCES = Codegen/tests/i2_let_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I3_SOURCES = Codegen/tests/i3_binary_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I4_SOURCES = Codegen/tests/i4_print_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I5_SOURCES = Codegen/tests/i5_control_flow_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I6_SOURCES = Codegen/tests/i6_assign_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I7_SOURCES = Codegen/tests/i7_functions_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I8_SOURCES = Codegen/tests/i8_builtins_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I9_SOURCES = Codegen/tests/i9_oo_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I10_SOURCES = Codegen/tests/i10_inherit_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
CODEGEN_I11_SOURCES = Codegen/tests/i11_is_as_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp

R1_SEMANTIC_TEST_TARGET = r1_semantic_smoke
R2_SEMANTIC_TEST_TARGET = r2_semantic_smoke
R3_R4_SEMANTIC_TEST_TARGET = r3_r4_semantic_smoke
A4_BUILTINS_TEST_TARGET = a4_builtins_smoke
TYPE_MAP_TEST_TARGET = type_map_smoke
EVAL_TEST_TARGET = eval_smoke
EVAL_FUNCTIONS_TEST_TARGET = eval_functions_smoke
EVAL_OPS_TEST_TARGET = eval_ops_smoke
EVAL_LOOPS_TEST_TARGET = eval_loops_smoke
EVAL_WITH_TEST_TARGET = eval_with_smoke
EVAL_OO_TEST_TARGET = eval_oo_smoke
EVAL_LITERALS_TEST_TARGET = eval_literals_smoke
IS_AS_SMOKE_TARGET = is_as_smoke

TARGET = hulk_c.exe
TYPE_TEST_TARGET = type_info_smoke
SYMBOL_SMOKE_TARGET = symbol_table_smoke
SYMBOL_SCOPE_TEST_TARGET = symbol_table_scope_tests
SYMBOL_TEST_AST = Types/type_info.cpp Parser/ast/expr.cpp
SYMBOL_TEST_AST_COLLECTOR = $(SYMBOL_TEST_AST) SymbolTable/decl_collector.cpp
FILE ?= Parser/tests/valid_expr_pipeline.hulk

all: compile

build: $(HULK_BIN)

ifeq ($(OS),Windows_NT)
$(HULK_BIN): compile
	cmd /c copy /Y $(TARGET) $(HULK_BIN) >nul
else
$(HULK_BIN): compile
	$(CXX) $(COMPILE_CXXFLAGS) -O2 $(COMPILE_SOURCES) -o $(HULK_BIN) $(COMPILE_LDFLAGS)
endif

lexer:
	flex++ --c++ -o Lexer/hulk_lexer.cpp Lexer/hulk_lexer.l

compile:
ifeq ($(HULK_HAVE_LLVM),1)
	@echo "[make] Compilando $(TARGET) con codegen LLVM $(LLVM_AVAILABLE) (varios minutos)..."
	$(RUN_TIMER) --label compile $(CXX) $(COMPILE_CXXFLAGS) $(COMPILE_SOURCES) -o $(TARGET) $(COMPILE_LDFLAGS)
else
	@echo "[make] LLVM no detectado: $(TARGET) sin codegen (solo interprete). Instala llvm-21 o usa compile_nollvm."
	$(CXX) $(COMPILE_CXXFLAGS) $(COMPILE_SOURCES) -o $(TARGET) $(COMPILE_LDFLAGS)
endif

compile_nollvm:
	@echo "[make] compile_nollvm: sin libLLVM"
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

execute: compile
	./$(TARGET) --tokens --cst --ast $(FILE)

test_types:
	$(CXX) $(CXXFLAGS) Types/tests/type_info_smoke.cpp Types/type_info.cpp Parser/ast/expr.cpp -o $(TYPE_TEST_TARGET)
	./$(TYPE_TEST_TARGET)

test_symbols: test_symbols_smoke test_symbols_scope

test_symbols_smoke:
	$(CXX) $(CXXFLAGS) SymbolTable/tests/symbol_table_smoke.cpp $(SYMBOL_TEST_AST_COLLECTOR) -o $(SYMBOL_SMOKE_TARGET)
	./$(SYMBOL_SMOKE_TARGET)

test_symbols_scope:
	$(CXX) $(CXXFLAGS) SymbolTable/tests/symbol_table_scope_tests.cpp $(SYMBOL_TEST_AST) -o $(SYMBOL_SCOPE_TEST_TARGET)
	./$(SYMBOL_SCOPE_TEST_TARGET)

test_r1_semantic:
	$(CXX) $(CXXFLAGS) SemanticCheck/tests/r1_semantic_smoke.cpp $(PARSER_TEST_COMMON) SemanticCheck/phase2_checker.cpp -o $(R1_SEMANTIC_TEST_TARGET)
	./$(R1_SEMANTIC_TEST_TARGET)

test_r2_semantic:
	$(CXX) $(CXXFLAGS) SemanticCheck/tests/r2_semantic_smoke.cpp $(PARSER_TEST_COMMON) SemanticCheck/phase2_checker.cpp -o $(R2_SEMANTIC_TEST_TARGET)
	./$(R2_SEMANTIC_TEST_TARGET)

test_r3_r4_semantic:
	$(CXX) $(CXXFLAGS) SemanticCheck/tests/r3_r4_semantic_smoke.cpp $(PARSER_TEST_COMMON) SemanticCheck/phase2_checker.cpp -o $(R3_R4_SEMANTIC_TEST_TARGET)
	./$(R3_R4_SEMANTIC_TEST_TARGET)

test_a4_builtins:
	$(CXX) $(CXXFLAGS) SemanticCheck/tests/a4_builtins_smoke.cpp $(PARSER_TEST_COMMON) SemanticCheck/phase2_checker.cpp -o $(A4_BUILTINS_TEST_TARGET)
	./$(A4_BUILTINS_TEST_TARGET)

test_type_map:
	$(CXX) $(CXXFLAGS) SemanticCheck/tests/type_map_smoke.cpp $(PARSER_TEST_COMMON) SemanticCheck/phase2_checker.cpp -o $(TYPE_MAP_TEST_TARGET)
	./$(TYPE_MAP_TEST_TARGET)

test_semantic: test_r1_semantic test_r2_semantic test_r3_r4_semantic test_a4_builtins test_type_map

test_is_as_smoke:
	$(CXX) $(CXXFLAGS) Parser/tests/is_as_expr_pipeline_smoke.cpp $(PARSER_TEST_COMMON) -o $(IS_AS_SMOKE_TARGET)
	./$(IS_AS_SMOKE_TARGET)

test_matcom_oop_parse:
	$(CXX) $(CXXFLAGS) Parser/tests/matcom_oop_parse_smoke.cpp $(PARSER_TEST_COMMON) -o $(MATCOM_OOP_PARSE_TARGET)
	./$(MATCOM_OOP_PARSE_TARGET)

test_llvm:
ifeq ($(LLVM_AVAILABLE),)
	@echo "LLVM no encontrado: instala llvm-21 (MSYS2 UCRT64 / apt.llvm.org) y pon llvm-config en PATH, o define LLVM_CONFIG=..."
	@exit 1
else
	CXX='$(CXX)' \
	CXXFLAGS='$(CXXFLAGS)' \
	LLVM_CXXFLAGS_ALL='$(LLVM_CXXFLAGS_ALL)' \
	LLVM_LDFLAGS='$(LLVM_LDFLAGS)' \
	$(LLVM_PATH_EXPORT) \
	$(RUN_TIMER) --label test_llvm bash scripts/test_llvm.sh
endif

test_llvm_fixtures: compile
ifeq ($(LLVM_AVAILABLE),)
	@echo "LLVM no encontrado: instala llvm-21 (MSYS2 UCRT64 / apt.llvm.org) y pon llvm-config en PATH, o define LLVM_CONFIG=..."
	@exit 1
else
	$(LLVM_PATH_EXPORT) \
	$(RUN_TIMER) --label test_llvm_fixtures bash scripts/test_llvm_fixtures.sh
endif

test_eval:
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_TEST_TARGET)
	./$(EVAL_TEST_TARGET)
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_literals_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_LITERALS_TEST_TARGET)
	./$(EVAL_LITERALS_TEST_TARGET)
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_functions_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_FUNCTIONS_TEST_TARGET)
	./$(EVAL_FUNCTIONS_TEST_TARGET)
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_ops_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_OPS_TEST_TARGET)
	./$(EVAL_OPS_TEST_TARGET)
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_loops_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_LOOPS_TEST_TARGET)
	./$(EVAL_LOOPS_TEST_TARGET)
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_with_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_WITH_TEST_TARGET)
	./$(EVAL_WITH_TEST_TARGET)
	$(CXX) $(CXXFLAGS) Evaluator/tests/eval_oo_smoke.cpp $(PARSER_TEST_COMMON) Value/value.cpp Evaluator/evaluator.cpp -o $(EVAL_OO_TEST_TARGET)
	./$(EVAL_OO_TEST_TARGET)

RUN_FILE ?= playground/hello.hulk

ifeq ($(OS),Windows_NT)
run: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hulk.ps1 $(RUN_FILE)
run-pipeline: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hulk.ps1 -Pipeline $(RUN_FILE)
run-lexer: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hulk.ps1 -Lexer $(RUN_FILE)
run-parse: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hulk.ps1 -Parse $(RUN_FILE)
run-semantic: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hulk.ps1 -SemanticOnly $(RUN_FILE)
test_semantic_fixtures: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_semantic.ps1
test_eval_fixtures: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_eval.ps1
else
run: compile
	bash scripts/run_hulk.sh $(RUN_FILE)
run-pipeline: compile
	bash scripts/run_hulk.sh --pipeline $(RUN_FILE)
run-lexer: compile
	bash scripts/run_hulk.sh --lexer $(RUN_FILE)
run-parse: compile
	bash scripts/run_hulk.sh --parse $(RUN_FILE)
run-semantic: compile
	bash scripts/run_hulk.sh --semantic-only $(RUN_FILE)
test_semantic_fixtures: compile
	bash scripts/run_semantic.sh
test_eval_fixtures: compile
	bash scripts/run_eval.sh
endif

clean:
	rm -f $(TARGET) $(HULK_BIN) output .hulk_program.hulk $(LLVM_TEST_TARGET) $(TYPE_TEST_TARGET) $(SYMBOL_SMOKE_TARGET) $(SYMBOL_SCOPE_TEST_TARGET) $(R1_SEMANTIC_TEST_TARGET) $(R2_SEMANTIC_TEST_TARGET) $(R3_R4_SEMANTIC_TEST_TARGET) $(A4_BUILTINS_TEST_TARGET) $(EVAL_TEST_TARGET) $(EVAL_LITERALS_TEST_TARGET) $(EVAL_FUNCTIONS_TEST_TARGET) $(EVAL_OPS_TEST_TARGET) $(EVAL_LOOPS_TEST_TARGET) $(EVAL_WITH_TEST_TARGET) $(EVAL_OO_TEST_TARGET) $(IS_AS_SMOKE_TARGET) $(MATCOM_OOP_PARSE_TARGET) Lexer/*.o Parser/core/*.o Parser/ast/*.o Parser/generator/*.o Parser/syntax/*.o
