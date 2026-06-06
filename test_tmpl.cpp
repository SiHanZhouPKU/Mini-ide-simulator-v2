#include <iostream>
#include <fstream>
#include <sstream>
#include "src/lexer.h"
#include "src/parser.h"
#include "src/ast.h"
#include "src/executor.h"
#include "src/variant_value.h"

int main() {
    // Simple test: template function getMax
    std::string code = R"(
template<typename T>
T getMax(const vector<T>& data) {
    T maxVal = data[0];
    for (int i = 1; i < data.size(); i++) {
        if (data[i] > maxVal) maxVal = data[i];
    }
    return maxVal;
}

int main() {
    vector<double> scoreVec = {85.5, 92.0, 78.0};
    cout << getMax(scoreVec) << endl;
    return 0;
}
)";

    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    std::cout << "Tokens: " << tokens.size() << "\n";

    Parser parser(tokens);
    auto prog = parser.parse();
    if (!parser.error().empty()) {
        std::cout << "Parse error: " << parser.error() << "\n";
        return 1;
    }
    std::cout << "Parse OK\n";
    std::cout << "Template funcs: " << prog->templateFuncs.size() << "\n";
    if (!prog->templateFuncs.empty()) {
        auto& tf = prog->templateFuncs[0];
        std::cout << "Template func name: " << tf->func->name << "\n";
        std::cout << "Params: " << tf->func->params.size() << "\n";
        for (auto& p : tf->func->params) {
            std::cout << "  param: type='" << p.first << "' name='" << p.second << "'\n";
        }
        std::cout << "Body size: " << tf->func->body.size() << "\n";
        for (size_t i = 0; i < tf->func->body.size(); i++) {
            auto* s = tf->func->body[i].get();
            if (dynamic_cast<DeclStmt*>(s)) std::cout << "  body[" << i << "]: DeclStmt\n";
            else if (dynamic_cast<ForStmt*>(s)) std::cout << "  body[" << i << "]: ForStmt\n";
            else if (dynamic_cast<ReturnStmt*>(s)) std::cout << "  body[" << i << "]: ReturnStmt\n";
            else std::cout << "  body[" << i << "]: other\n";
        }
    }

    return 0;
}
