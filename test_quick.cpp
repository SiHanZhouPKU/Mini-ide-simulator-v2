#include <iostream>
#include <sstream>
#include <fstream>
#include "src/lexer.h"
#include "src/parser.h"
#include "src/ast.h"
#include "src/executor.h"

int main(int argc, char** argv) {
    std::string file = (argc > 1) ? argv[1] : "/Users/tianwenze/Desktop/程设大作业/v3/demo_full.cpp";
    std::ifstream f(file);
    if (!f) { std::cerr << "Cannot open " << file << "\n"; return 1; }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string source = ss.str();
    f.close();

    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    std::cout << "Tokens: " << tokens.size() << "\n";

    Parser parser(tokens);
    auto prog = parser.parse();
    if (!parser.error().empty()) {
        std::cout << "PARSE ERROR: " << parser.error() << "\n";
        return 1;
    }
    std::cout << "Parse OK\n";

    Executor exec;
    exec.setAst(prog.get());
    exec.runAll();
    if (!exec.error().empty()) {
        std::cout << "EXEC ERROR: " << exec.error() << "\n";
        return 1;
    }
    std::cout << "EXEC OK\n";
    return 0;
}
