.PHONY: all compile lexer clean execute test_types test_symbols test_semantic test_semantic_fixtures test_r1_semantic test_r2_semantic test_r3_r4_semantic test_a4_builtins

# Compilador y flags
CXX = g++
# FlexLexer.h: MSYS2/ghcup o WinFlexBison (winget install WinFlexBison.win_flex_bison)
FLEX_WIN = $(LOCALAPPDATA)/Microsoft/WinGet/Packages/WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe
CXXFLAGS = -std=c++17 -Wall -I. -ILexer -IParser/core -IParser/ast -IParser/generator -IParser/syntax \
           -ISemanticCheck -ISymbolTable -ITypes \
           -I/usr/include -IC:/ghcup/msys64/usr/include -I$(FLEX_WIN)

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
          Compiler/main.cpp

PARSER_TEST_COMMON = Lexer/hulk_lexer.cpp Parser/core/token_adapter.cpp Parser/core/token_stream.cpp \
          Parser/ast/expr.cpp Parser/ast/cst_nodes.cpp Parser/ast/cst_to_ast.cpp \
          Parser/generator/grammar_reader.cpp Parser/generator/first_follow.cpp \
          Parser/generator/ll1_table.cpp Parser/syntax/ll1_parser.cpp \
          SemanticCheck/binding_list.cpp

R1_SEMANTIC_TEST_TARGET = r1_semantic_smoke
R2_SEMANTIC_TEST_TARGET = r2_semantic_smoke
R3_R4_SEMANTIC_TEST_TARGET = r3_r4_semantic_smoke
A4_BUILTINS_TEST_TARGET = a4_builtins_smoke

TARGET = hulk.exe
TYPE_TEST_TARGET = type_info_smoke
SYMBOL_SMOKE_TARGET = symbol_table_smoke
SYMBOL_SCOPE_TEST_TARGET = symbol_table_scope_tests
SYMBOL_TEST_AST = Types/type_info.cpp Parser/ast/expr.cpp
SYMBOL_TEST_AST_COLLECTOR = $(SYMBOL_TEST_AST) SymbolTable/decl_collector.cpp
FILE ?= Parser/tests/valid_expr_pipeline.hulk

all: compile

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

test_semantic: test_r1_semantic test_r2_semantic test_r3_r4_semantic test_a4_builtins

ifeq ($(OS),Windows_NT)
test_semantic_fixtures: compile
	powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_semantic.ps1
else
test_semantic_fixtures: compile
	bash scripts/run_semantic.sh
endif

clean:
	rm -f $(TARGET) $(TYPE_TEST_TARGET) $(SYMBOL_SMOKE_TARGET) $(SYMBOL_SCOPE_TEST_TARGET) $(R1_SEMANTIC_TEST_TARGET) $(R2_SEMANTIC_TEST_TARGET) $(R3_R4_SEMANTIC_TEST_TARGET) $(A4_BUILTINS_TEST_TARGET) Lexer/*.o Parser/core/*.o Parser/ast/*.o Parser/generator/*.o Parser/syntax/*.o
