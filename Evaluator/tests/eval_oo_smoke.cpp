#include <iostream>
#include <sstream>
#include <string>

#include "../../Parser/ast/cst_to_ast.hpp"
#include "../../Parser/core/token_adapter.hpp"
#include "../../Parser/generator/first_follow.hpp"
#include "../../Parser/generator/grammar_reader.hpp"
#include "../../Parser/generator/ll1_table.hpp"
#include "../../Parser/syntax/ll1_parser.hpp"
#include "../evaluator.hpp"

namespace {

bool runProgram(const std::string& source, const std::string& expected_stdout, const char* label) {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        const auto program = parser::cst_to_ast(*parse_result.cst_root);

        std::ostringstream out;
        eval::Evaluator evaluator(out);
        evaluator.evaluate(program.get());
        if (evaluator.hadError()) {
            std::cerr << "[FAIL] " << label << ": " << evaluator.lastError() << "\n";
            return false;
        }
        if (out.str() != expected_stdout) {
            std::cerr << "[FAIL] " << label << ": salida esperada '" << expected_stdout << "', obtuvo '"
                      << out.str() << "'\n";
            return false;
        }
        std::cout << "[OK] " << label << "\n";
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << label << ": excepcion: " << ex.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    ok &= runProgram(
        "class Point(x: Number, y: Number) { x: Number = x; y: Number = y; }\n"
        "print(let p = new Point(1, 2) in p.x);\n",
        "1\n",
        "C2: new + attr read");

    ok &= runProgram(
        "class Point(x: Number) { x: Number = x; }\n"
        "print(let p = new Point(0) in { p.x := 5; p.x; });\n",
        "5\n",
        "C3: attr assign via GetAttr := ");

    ok &= runProgram(
        "class Counter(n: Number) { n: Number = n; inc() => self.n := self.n + 1; }\n"
        "print(let c = new Counter(0) in c.inc());\n",
        "1\n",
        "C4: method call + self");

    ok &= runProgram(
        "class A() {}\n"
        "class B() is A {}\n"
        "print(0);\n",
        "0\n",
        "C1: dual pass registers all classes");

    ok &= runProgram(
        "class Person(name:String) {\n"
        "  name:String = name;\n"
        "  greet() => \"Hello, \" @ self.name;\n"
        "}\n"
        "class Student(name:String) is Person(name) {\n"
        "  greet() => base() @ \"!\";\n"
        "}\n"
        "print(let s = new Student(\"Alice\") in s.greet());\n",
        "\"Hello, Alice!\"\n",
        "C5: base() in method");

    ok &= runProgram(
        "class Person(n: Number) { n: Number = n; }\n"
        "class Sub(n: Number) is Person(n) { twice() => self.n * 2; }\n"
        "print(let x = new Sub(5) in (x as Sub).twice());\n",
        "10\n",
        "C6: as downcast + method");

    return ok ? 0 : 1;
}
