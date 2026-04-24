#include "hulk_lexer.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {

    // Puedes leer desde un archivo o desde stdin
    // Ejemplo 1: leer desde stdin
    //   echo "let x := 3.14;" | ./hulk_test.exe
    // Ejemplo 2: leer desde archivo
    //   ./hulk_test.exe mi_programa.hulk

    std::istream* input = &std::cin;
    std::ifstream file;

    if (argc > 1) {
        file.open(argv[1]);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir: " << argv[1] << "\n";
            return 1;
        }
        input = &file;
    }

    HulkLexer lexer(*input, std::cerr);

    int tok;
    const int EOF_VAL = static_cast<int>(TokenType::EOF_TOKEN);

    while ((tok = lexer.yylex()) != EOF_VAL) {

        // Imprimir posición y nombre del token
        std::cout << "[linea " << lexer.token_line()
                  << ", col "  << lexer.token_col()  << "] "
                  << HulkLexer::token_name(tok);

        // Imprimir valor semántico según el tipo
        switch (static_cast<TokenType>(tok)) {
            case TokenType::IDENTIFIER:
            case TokenType::STRING_LITERAL:
                std::cout << " => \"" << lexer.sem.str_val << "\"";
                break;
            case TokenType::NUMBER_LITERAL:
                std::cout << " => " << lexer.sem.num_val;
                break;
            case TokenType::BOOLEAN_LITERAL:
                std::cout << " => " << (lexer.sem.bool_val ? "true" : "false");
                break;
            default:
                break;
        }

        std::cout << "\n";
    }

    std::cout << "[EOF]\n";
    return 0;
}
