.PHONY: all compile lexer clean execute

# Compilador y flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -I. -ILexer -IParser/core -IParser/ast -IParser/generator -IParser/syntax -I/usr/include -IC:/ghcup/msys64/usr/include

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
          Compiler/main.cpp

TARGET = hulk.exe
FILE ?= Parser/tests/valid_expr_pipeline.hulk

all: compile

lexer:
	flex++ --c++ -o Lexer/hulk_lexer.cpp Lexer/hulk_lexer.l

compile:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

execute: compile
	./$(TARGET) --tokens --cst --ast $(FILE)

clean:
	rm -f $(TARGET) Lexer/*.o Parser/core/*.o Parser/ast/*.o Parser/generator/*.o Parser/syntax/*.o
