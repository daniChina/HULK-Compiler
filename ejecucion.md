Cómo compilarlo y ejecutarlo:


Compilarlo:

g++ -std=c++17 Lexer/hulk_lexer.cpp Parser/core/token_adapter.cpp Parser/core/token_stream.cpp Parser/ast/expr.cpp Parser/ast/cst_nodes.cpp Parser/ast/cst_to_ast.cpp Parser/generator/grammar_reader.cpp Parser/generator/first_follow.cpp Parser/generator/ll1_table.cpp Parser/syntax/ll1_parser.cpp Compiler/main.cpp -o /tmp/hulk_parser


Ejemplo de ejecucion:

/tmp/hulk_parser --tokens --cst --ast Parser/tests/valid_expr_pipeline.hulk
