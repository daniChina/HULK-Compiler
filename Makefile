MATCOM_OOP_PARSE_TARGET = matcom_oop_parse_smoke

.PHONY: all compile build lexer clean execute run run-pipeline run-lexer run-parse run-semantic test_types test_symbols test_semantic test_semantic_fixtures test_r1_semantic test_r2_semantic test_r3_r4_semantic test_a4_builtins test_type_map test_eval test_eval_fixtures test_is_as_smoke test_matcom_oop_parse test_llvm

# --- LLVM (Fase 4): solo test_llvm enlaza libLLVM; compile principal sin LLVM (~40 s) ---
ifeq ($(OS),Windows_NT)
LLVM_CONFIG ?= C:/ghcup/ghc/9.6.7/mingw/bin/llvm-config
else
LLVM_CONFIG ?= llvm-config
endif
LLVM_AVAILABLE := $(shell "$(LLVM_CONFIG)" --version)
LLVM_CXXFLAGS_RAW := $(shell "$(LLVM_CONFIG)" --cxxflags)
LLVM_LDFLAGS := $(shell "$(LLVM_CONFIG)" --ldflags --libs core support)
LLVM_CXXFLAGS := $(filter-out -stdlib=libc++,$(LLVM_CXXFLAGS_RAW))
LLVM_CXXFLAGS := $(filter-out -std=c++14,$(LLVM_CXXFLAGS))
LLVM_CXXFLAGS := $(filter-out -std=gnu++14,$(LLVM_CXXFLAGS))
# Mantener -fno-exceptions/-fno-rtti: libLLVM-14 (GHCup) exige ABI compatible en Windows.

CODEGEN_SOURCES = Codegen/llvm_codegen.cpp Codegen/llvm_aux.cpp
CODEGEN_I0_SOURCES = Codegen/tests/i0_module_smoke.cpp $(CODEGEN_SOURCES) Parser/ast/expr.cpp
LLVM_I0_TARGET = llvm_i0_smoke
LLVM_I1_TARGET = llvm_i1_smoke
LLVM_I2_TARGET = llvm_i2_smoke
LLVM_I3_TARGET = llvm_i3_smoke

# Compilador y flags
CXX = g++
# FlexLexer.h: MSYS2/ghcup o WinFlexBison (winget install WinFlexBison.win_flex_bison)
FLEX_WIN = $(LOCALAPPDATA)/Microsoft/WinGet/Packages/WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe
CXXFLAGS = -std=c++17 -Wall -fuse-ld=bfd -I. -ILexer -IParser/core -IParser/ast -IParser/generator -IParser/syntax \
           -ISemanticCheck -ISymbolTable -ITypes -IValue -IEvaluator -ICodegen \
           -I/usr/include -IC:/ghcup/msys64/usr/include -I$(FLEX_WIN)
LLVM_CXXFLAGS_ALL = -std=c++17 $(LLVM_CXXFLAGS)

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
$(HULK_BIN):
	$(CXX) $(CXXFLAGS) -O2 $(SOURCES) -o $(HULK_BIN)
endif

lexer:
	flex++ --c++ -o Lexer/hulk_lexer.cpp Lexer/hulk_lexer.l

compile:
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
	@echo "LLVM no encontrado: instala llvm-config (GHCup: C:/ghcup/ghc/*/mingw/bin) o define LLVM_CONFIG=..."
	@exit 1
else
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS_ALL) $(CODEGEN_I0_SOURCES) -o $(LLVM_I0_TARGET) $(LLVM_LDFLAGS)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS_ALL) $(CODEGEN_I1_SOURCES) -o $(LLVM_I1_TARGET) $(LLVM_LDFLAGS)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS_ALL) $(CODEGEN_I2_SOURCES) -o $(LLVM_I2_TARGET) $(LLVM_LDFLAGS)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS_ALL) $(CODEGEN_I3_SOURCES) -o $(LLVM_I3_TARGET) $(LLVM_LDFLAGS)
ifeq ($(OS),Windows_NT)
	cmd /c "set PATH=C:/ghcup/ghc/9.6.7/mingw/bin;%PATH% && $(LLVM_I0_TARGET).exe && $(LLVM_I1_TARGET).exe && $(LLVM_I2_TARGET).exe && $(LLVM_I3_TARGET).exe"
else
	./$(LLVM_I0_TARGET)
	./$(LLVM_I1_TARGET)
	./$(LLVM_I2_TARGET)
	./$(LLVM_I3_TARGET)
endif
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
