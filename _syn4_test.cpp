#include <sstream>
#include <iostream>
#include "../Parser/syntax/ll1_parser.hpp"
#include "../Parser/core/token_adapter.hpp"
#include "../Parser/generator/grammar_reader.hpp"
#include "../Parser/generator/first_follow.hpp"
#include "../Parser/generator/ll1_table.hpp"
int main() {
  try {
    std::istringstream in("let outer=1 in function f()=>outer+1 in f;");
    auto tokens = parser::tokenize_stream(in);
    auto g = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
    auto ff = parser::generator::compute_first_follow(g);
    auto t = parser::generator::build_ll1_table(g, ff);
    parser::Ll1Parser p(std::move(tokens), g, t.table);
    p.parse();
    std::cout << "ACCEPTED\n";
  } catch (const parser::ParseError& e) {
    std::cout << e.what() << "\n";
  }
}
