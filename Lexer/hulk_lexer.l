/* ================================================================
 * hulk_lexer.l  —  Analizador léxico HULK  (flex++ / C++)
 *
 * Compilar:
 *   flex++ --c++ -o hulk_lexer.cpp hulk_lexer.l
 *   g++ hulk_lexer.cpp -I"C:\msys64\usr\include" -o hulk_test.exe
 *
 * Por qué funciona ahora:
 *   1. %option yyclass="HulkLexer"  → flex++ genera el cuerpo de
 *      yylex() como método de HulkLexer (no de yyFlexLexer).
 *      Dentro de las acciones `this` es HulkLexer*, por eso
 *      column_, line_, sem son visibles directamente.
 *   2. hulk_lexer.h se incluye en el prólogo (%{ ... %}) ANTES
 *      de que flex emita el código de las reglas. Así la clase
 *      está completamente declarada cuando se compila yylex().
 *   3. NO se usa %option c++ junto con yyclass — yyclass ya
 *      implica el modo C++; ponerlos juntos causaba conflictos
 *      en algunas versiones de flex.
 * ================================================================ */

/* ── Opciones ────────────────────────────────────────────── */
%option yyclass="HulkLexer"
%option noyywrap
%option never-interactive
%option nodefault

/* ── Prólogo: se copia textualmente al inicio del .cpp ────── */
%{
/*
 * CRÍTICO: incluir el header AQUÍ, dentro del bloque %{ %},
 * para que HulkLexer esté declarada antes del cuerpo de yylex().
 * Si se incluye después del %% final ya es demasiado tarde.
 */
#include "hulk_lexer.h"

/* Guarda la posición inicial del token antes de consumirlo. */
#define MARK_TOKEN() do { token_line_ = line_; token_col_ = column_; } while(0)

/* Guarda el lexema crudo cuando la regla coincide directamente con yytext. */
#define SET_LEXEME() do { lexeme_ = yytext; } while(0)

/* Macro interna: marca posición inicial, avanza columna y devuelve token. */
#define UPD(t) do { MARK_TOKEN(); SET_LEXEME(); column_ += yyleng; \
                    return static_cast<int>(TokenType::t); } while(0)
%}

/* ── Macros de patrones ──────────────────────────────────── */
DIGIT       [0-9]
ALPHA       [a-zA-Z]
ALNUM_      [a-zA-Z0-9_]
INT         {DIGIT}+
FLOAT       {DIGIT}+\.{DIGIT}+
IDENT       {ALPHA}{ALNUM_}*

%%

 /* ── 1. Espacios ────────────────────────────────────────── */
[ \t\r]+      { column_ += yyleng; }
\n            { ++line_; column_ = 1; }

 /* ── 2. Comentarios ─────────────────────────────────────── */
"//"[^\n]*    { column_ += yyleng; }

"/*"          {
                column_ += yyleng;
                int c1 = yyinput();
                if (c1 == '\n') { ++line_; column_ = 1; }
                else             { ++column_; }
                while (true) {
                    int c2 = yyinput();
                    if (c2 == 0) {
                        std::cerr << "[Lexer] linea " << line_
                                  << ": comentario de bloque sin cerrar\n";
                        break;
                    }
                    if (c2 == '\n') { ++line_; column_ = 1; }
                    else             { ++column_; }
                    if (c1 == '*' && c2 == '/') break;
                    c1 = c2;
                }
              }

 /* ── 3. Literales de cadena ─────────────────────────────── */
\"            {
                MARK_TOKEN();
                lexeme_.clear();
                lexeme_ += '"';
                ++column_;
                std::string buf;
                int ch;
                while (true) {
                    ch = yyinput();
                    if (ch == 0 || ch == '\n') {
                        std::cerr << "[Lexer] linea " << line_
                                  << ": cadena sin cerrar\n";
                        break;
                    }
                    ++column_;
                    lexeme_ += static_cast<char>(ch);
                    if (ch == '"') break;
                    if (ch == '\\') {
                        int esc = yyinput(); ++column_;
                        lexeme_ += static_cast<char>(esc);
                        switch (esc) {
                            case '"':  buf += '"';  break;
                            case '\\': buf += '\\'; break;
                            case 'n':  buf += '\n'; break;
                            case 't':  buf += '\t'; break;
                            case 'r':  buf += '\r'; break;
                            default:   buf += '\\'; buf += (char)esc; break;
                        }
                    } else {
                        buf += (char)ch;
                    }
                }
                sem.str_val = buf;
                return static_cast<int>(TokenType::STRING_LITERAL);
              }

 /* ── 4. Números ─────────────────────────────────────────── */
{FLOAT}       { MARK_TOKEN();
                SET_LEXEME();
                column_ += yyleng;
                sem.num_val = std::stof(yytext);
                return static_cast<int>(TokenType::NUMBER_LITERAL); }

{INT}         { MARK_TOKEN();
                SET_LEXEME();
                column_ += yyleng;
                sem.num_val = (float)std::atoi(yytext);
                return static_cast<int>(TokenType::NUMBER_LITERAL); }

 /* ── 5. Booleanos ───────────────────────────────────────── */
"true"        { MARK_TOKEN(); SET_LEXEME(); column_ += yyleng; sem.bool_val = true;
                return static_cast<int>(TokenType::TRUE); }
"false"       { MARK_TOKEN(); SET_LEXEME(); column_ += yyleng; sem.bool_val = false;
                return static_cast<int>(TokenType::FALSE); }

 /* ── 6. Palabras clave ──────────────────────────────────── */
"if"          { UPD(IF);         }
"elif"        { UPD(ELIF);       }
"else"        { UPD(ELSE);       }
"while"       { UPD(WHILE);      }
"for"         { UPD(FOR);        }
"function"    { UPD(FUNCTION);   }
"type"        { UPD(TYPE);       }
"protocol"    { UPD(PROTOCOL);   }
"def"         { UPD(DEF);        }
"let"         { UPD(LET);        }
"in"          { UPD(IN);         }
"new"         { UPD(NEW);        }
"inherits"    { UPD(INHERITS);   }
"self"        { UPD(SELF);       }
"base"        { UPD(BASE);       }
"is"          { UPD(IS);         }
"as"          { UPD(AS);         }
"and"         { UPD(AND);        }
"or"          { UPD(OR);         }

 /* ── 7. Identificadores y nombres globales ─────────────── */
{IDENT}       { MARK_TOKEN(); SET_LEXEME(); column_ += yyleng; sem.str_val = yytext;
                return static_cast<int>(TokenType::IDENTIFIER); }

 /* ── 8. Operadores de dos caracteres ────────────────────── */
":="          { UPD(ASSIGN);        }
"=>"          { UPD(ARROW);         }
"=="          { UPD(EQUAL_EQUAL);   }
"!="          { UPD(BANG_EQUAL);    }
"<="          { UPD(LESS_EQUAL);    }
">="          { UPD(GREATER_EQUAL); }
"@@"          { UPD(CONCAT_WS);     }
"="           { UPD(EQUAL);         }  
"@"           { UPD(CONCAT);        }   

 /* ── 9. Operadores de un carácter ───────────────────────── */
"("           { UPD(LPAREN);    }
")"           { UPD(RPAREN);    }
"{"           { UPD(LBRACE);    }
"}"           { UPD(RBRACE);    }
"["           { UPD(LBRACKET);  }
"]"           { UPD(RBRACKET);  }
","           { UPD(COMMA);     }
";"           { UPD(SEMICOLON); }
"."           { UPD(DOT);       }
":"           { UPD(COLON);     }
"+"           { UPD(PLUS);      }
"-"           { UPD(MINUS);     }
"*"           { UPD(STAR);      }
"/"           { UPD(SLASH);     }
"^"           { UPD(CARET);     }
"~"           { UPD(TILDE);     }
"!"           { UPD(BANG);      }
"<"           { UPD(LESS);      }
">"           { UPD(GREATER);   }

 /* ── 10. EOF ────────────────────────────────────────────── */
<<EOF>>       { lexeme_.clear(); return static_cast<int>(TokenType::EOF_TOKEN); }

 /* ── 11. Carácter desconocido ───────────────────────────── */
.             {
                MARK_TOKEN();
                SET_LEXEME();
                column_ += yyleng;
                std::cerr << "[Lexer] linea " << line_
                          << ", columna " << token_col_
                          << ": caracter desconocido '"
                          << yytext[0] << "'\n";
                sem.str_val = yytext;
                return static_cast<int>(TokenType::UNKNOWN);
              }

%%

/* ================================================================
 * main() de prueba — descomenta para probar standalone:
 *
 *   flex++ --c++ -o hulk_lexer.cpp hulk_lexer.l
 *   g++ hulk_lexer.cpp -I"C:\msys64\usr\include" -o hulk_test.exe
 *   echo "let x := 3.14;" | ./hulk_test.exe
 * ================================================================

int main() {
    HulkLexer lexer(std::cin, std::cerr);
    int tok;
    const int EOF_VAL = static_cast<int>(TokenType::EOF_TOKEN);
    while ((tok = lexer.yylex()) != EOF_VAL) {
        std::cout << "[" << lexer.line() << ":" << lexer.col() << "] "
                  << HulkLexer::token_name(tok);
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
            default: break;
        }
        std::cout << "\n";
    }
    return 0;
}

================================================================ */
