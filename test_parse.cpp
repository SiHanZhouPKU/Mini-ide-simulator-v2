#include <iostream>
#include <fstream>
#include <sstream>
#include "src/lexer.h"
#include "src/parser.h"
#include "src/ast.h"

int main() {
    std::ifstream f("D:/Desktop/v2/v2/demo_full.cpp");
    if (!f) { std::cerr << "Cannot open demo_full.cpp\n"; return 1; }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string source = ss.str();
    f.close();

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    std::cout << "Token count: " << tokens.size() << "\n";

    Parser parser(tokens);
    auto prog = parser.parse();

    if (!parser.error().empty()) {
        std::cout << "PARSE ERROR: " << parser.error() << "\n";
        return 1;
    }

    std::cout << "Parse SUCCESS!\n";
    std::cout << "  Classes: " << prog->classDefs.size() << "\n";
    std::cout << "  Template classes: " << prog->templateClasses.size() << "\n";
    std::cout << "  Template funcs: " << prog->templateFuncs.size() << "\n";
    std::cout << "  Functions: " << prog->functions.size() << "\n";
    std::cout << "  Main func: " << (prog->mainFunc ? "YES" : "NO") << "\n";
    if (prog->mainFunc) {
        std::cout << "  Main body stmts: " << prog->mainFunc->body.size() << "\n";
    }

    // List functions
    for (auto& fn : prog->functions) {
        std::cout << "    - " << fn->name << " (body: " << fn->body.size() << " stmts)\n";
    }

    return 0;
}
