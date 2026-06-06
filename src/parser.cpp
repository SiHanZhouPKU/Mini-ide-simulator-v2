#include "parser.h"
#include <stdexcept>
#include <algorithm>

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const Token& Parser::peek() const { return tokens_[pos_]; }
const Token& Parser::previous() const { return tokens_[pos_ - 1]; }

Token Parser::advance() {
    if (pos_ < tokens_.size()) return tokens_[pos_++];
    return tokens_.back();
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

bool Parser::check(TokenType type) const {
    if (pos_ >= tokens_.size()) return false;
    return tokens_[pos_].type == type;
}

bool Parser::atEnd() const {
    return pos_ >= tokens_.size() || tokens_[pos_].type == TokenType::END_OF_FILE;
}

Token Parser::consume(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    // Don't overwrite a more specific earlier error with a later generic one
    if (error_.empty())
        error_ = "第" + std::to_string(peek().lineNumber) + "行: " + msg;
    return tokens_[pos_];
}

bool Parser::isTypeToken(TokenType t) const {
    return t == TokenType::INT || t == TokenType::FLOAT || t == TokenType::DOUBLE ||
           t == TokenType::CHAR || t == TokenType::BOOL || t == TokenType::STRING ||
           t == TokenType::VOID || t == TokenType::AUTO || t == TokenType::LONG ||
           t == TokenType::SHORT || t == TokenType::UNSIGNED || t == TokenType::CONST ||
           t == TokenType::VECTOR || t == TokenType::LIST || t == TokenType::MAP ||
           t == TokenType::SET || t == TokenType::STACK || t == TokenType::QUEUE ||
           t == TokenType::DEQUE || t == TokenType::OFSTREAM ||
           t == TokenType::IFSTREAM || t == TokenType::FSTREAM || t == TokenType::STD;
}

bool Parser::isKnownType() const {
    return check(TokenType::IDENTIFIER) && knownTypes_.count(peek().text) > 0;
}

void Parser::registerClassName(const std::string& name) {
    knownTypes_.insert(name);
}

std::string Parser::parseType() {
    // Handle const prefix
    bool isConst = match(TokenType::CONST);
    std::string prefix = isConst ? "const " : "";

    // Handle std:: prefix
    if (match(TokenType::STD)) {
        consume(TokenType::SCOPE, "期望 '::'");
    }

    // Handle long, short, unsigned modifiers (with early returns)
    if (match(TokenType::LONG)) {
        if (match(TokenType::LONG)) return prefix + "long long";
        else if (match(TokenType::DOUBLE)) return prefix + "long double";
        else return prefix + "long";
    }
    if (match(TokenType::SHORT)) {
        if (match(TokenType::INT)) return prefix + "short int";
        return prefix + "short";
    }
    if (match(TokenType::UNSIGNED)) {
        if (match(TokenType::LONG)) {
            if (match(TokenType::LONG)) return prefix + "unsigned long long";
            return prefix + "unsigned long";
        }
        if (match(TokenType::SHORT)) return prefix + "unsigned short";
        if (match(TokenType::INT)) return prefix + "unsigned int";
        if (match(TokenType::CHAR)) return prefix + "unsigned char";
        return prefix + "unsigned int";
    }

    std::string typeName;
    if (match(TokenType::INT)) typeName = "int";
    else if (match(TokenType::FLOAT)) typeName = "float";
    else if (match(TokenType::DOUBLE)) typeName = "double";
    else if (match(TokenType::CHAR)) typeName = "char";
    else if (match(TokenType::BOOL)) typeName = "bool";
    else if (match(TokenType::STRING)) typeName = "string";
    else if (match(TokenType::VOID)) typeName = "void";
    else if (match(TokenType::AUTO)) typeName = "auto";
    else if (match(TokenType::VECTOR)) {
        typeName = "vector";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (vector)");
        }
    }
    else if (match(TokenType::LIST)) {
        typeName = "list";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (list)");
        }
    }
    else if (match(TokenType::SET)) {
        typeName = "set";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (set)");
        }
    }
    else if (match(TokenType::MAP)) {
        typeName = "map";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType();
            match(TokenType::COMMA);  // consume the comma between key and value types
            typeName += "," + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (map)");
        }
    }
    else if (match(TokenType::STACK)) {
        typeName = "stack";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (stack)");
        }
    }
    else if (match(TokenType::QUEUE)) {
        typeName = "queue";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (queue)");
        }
    }
    else if (match(TokenType::DEQUE)) {
        typeName = "deque";
        if (match(TokenType::LT)) {
            typeName += "<" + parseType() + ">";
            consume(TokenType::GT, "期望 '>' (deque)");
        }
    }
    else if (match(TokenType::OFSTREAM)) typeName = "ofstream";
    else if (match(TokenType::IFSTREAM)) typeName = "ifstream";
    else if (match(TokenType::FSTREAM)) typeName = "fstream";
    else if (match(TokenType::IDENTIFIER)) {
        typeName = previous().text;
        // Handle ClassName<T>
        if (match(TokenType::LT)) {
            typeName += "<";
            do {
                if (!check(TokenType::GT)) typeName += parseType();
            } while (match(TokenType::COMMA));
            typeName += ">";
            consume(TokenType::GT, "期望 '>' (identifier template)");
        }
    }

    // Handle pointer/reference for ALL types: int*, int&, Person*, etc.
    while (match(TokenType::STAR)) typeName += "*";
    if (match(TokenType::AMPERSAND)) typeName += "&";

    return prefix + typeName;
}

std::vector<TemplateParam> Parser::parseTemplateParams() {
    std::vector<TemplateParam> params;
    consume(TokenType::LT, "期望 '<' 开始模板参数列表");
    do {
        TemplateParam p;
        if (match(TokenType::CLASS)) p.kind = "class";
        else if (match(TokenType::TYPENAME)) p.kind = "typename";
        else {
            // Default: treat as typename
        }
        Token name = consume(TokenType::IDENTIFIER, "期望模板参数名");
        p.name = name.text;
        params.push_back(p);
    } while (match(TokenType::COMMA));
    consume(TokenType::GT, "期望 '>' 结束模板参数列表 (template params)");
    return params;
}

// ============================================================
// Top Level
// ============================================================

void Parser::skipPreprocessing() {
    while (true) {
        if (check(TokenType::STD)) { advance(); continue; }
        if (check(TokenType::USING)) {
            advance();
            // Skip "namespace xxx;"
            while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) advance();
            if (check(TokenType::SEMICOLON)) advance();
            continue;
        }
        if (check(TokenType::NAMESPACE)) { advance(); continue; }
        if (check(TokenType::INCLUDE)) {
            advance();
            while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) advance();
            if (check(TokenType::SEMICOLON)) advance();
            continue;
        }
        if (check(TokenType::IDENTIFIER)) {
            std::string id = peek().text;
            if (id == "include" || id == "using" || id == "namespace") {
                advance();
                if (id == "include") {
                    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) advance();
                    if (check(TokenType::SEMICOLON)) advance();
                }
                continue;
            }
        }
        if (check(TokenType::LT) || check(TokenType::GT) ||
            check(TokenType::SEMICOLON)) { advance(); continue; }
        break;
    }
}

void Parser::skipBracedBlock() {
    int depth = 0;
    if (check(TokenType::LBRACE)) { depth++; advance(); }
    while (depth > 0 && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::LBRACE)) depth++;
        if (check(TokenType::RBRACE)) depth--;
        advance();
    }
}

std::unique_ptr<Program> Parser::parse() {
    auto prog = std::make_unique<Program>();
    prog->lineNumber = 1;

    // Main parse loop: handle class defs, template defs, preprocessing, then main()
    size_t lastPos = SIZE_MAX;
    int stuckCount = 0;
    int loopCount = 0;
    while (!atEnd()) {
        if (++loopCount > 50000) {
            error_ = "解析器主循环超过50000次，位置=" + std::to_string(pos_) + " token=" + peek().text;
            break;
        }
        if (pos_ == lastPos) {
            stuckCount++;
            if (stuckCount > 100) {
                error_ = "解析器在位置 " + std::to_string(pos_) + " 卡住了 (token: " + peek().text + ")";
                break;
            }
        } else {
            stuckCount = 0;
            lastPos = pos_;
        }
        skipPreprocessing();
        if (atEnd()) break;

        // template <...>
        if (check(TokenType::TEMPLATE)) {
            auto saved = pos_;
            advance();
            if (check(TokenType::LT)) {
                pos_ = saved;
                // Parse template, peek ahead to see if class or function
                auto tmplSaved = pos_;
                advance(); // template
                auto params = parseTemplateParams();

                if (check(TokenType::CLASS) || check(TokenType::STRUCT)) {
                    // Template class
                    pos_ = tmplSaved;
                    auto tc = parseTemplateClass();
                    if (tc) {
                        if (tc->classDef) registerClassName(tc->classDef->name);
                        prog->templateClasses.push_back(std::move(tc));
                    }
                } else {
                    // Template function
                    pos_ = tmplSaved;
                    auto tf = parseTemplateFunc();
                    if (tf) prog->templateFuncs.push_back(std::move(tf));
                }
                if (!error_.empty()) break;
                continue;
            } else {
                pos_ = saved;
            }
        }

        // class / struct
        if (check(TokenType::CLASS) || check(TokenType::STRUCT)) {
            auto cls = parseClassDef();
            if (cls) {
                registerClassName(cls->name);
                prog->classDefs.push_back(std::move(cls));
            }
            if (!error_.empty()) break;
            continue;
        }

        // int main() / void main() / auto main() — or any function-looking thing
        if (isTypeToken(peek().type) ||
            (check(TokenType::IDENTIFIER) && peek().text == "main")) {
            // Look ahead: type name ( main ) — must be main function
            auto saved = pos_;
            std::string retType = parseType();
            if (match(TokenType::IDENTIFIER) && previous().text == "main" &&
                check(TokenType::LPAREN)) {
                pos_ = saved;
                auto func = parseFunctionDef(false);
                if (func && func->name == "main") {
                    prog->mainFunc = std::move(func);
                    // Parse remaining statements until EOF
                    while (!atEnd()) {
                        auto stmt = parseStatement();
                        if (stmt) prog->mainFunc->body.push_back(std::move(stmt));
                    }
                }
            } else {
                pos_ = saved;
                // Check if this looks like a function definition (name followed by '(')
                // Otherwise skip it (static member def, global variable, etc.)
                std::string retType2 = parseType();
                if (check(TokenType::IDENTIFIER)) {
                    std::string name = peek().text;
                    size_t next = pos_ + 1;
                    if (next < tokens_.size() && tokens_[next].type == TokenType::LPAREN) {
                        pos_ = saved;
                        auto func = parseFunctionDef(false);
                        if (func) {
                            prog->functions.push_back(std::move(func));
                        }
                    } else {
                        // Not a function — skip to semicolon
                        pos_ = saved;
                        while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE))
                            advance();
                        if (check(TokenType::SEMICOLON)) advance();
                    }
                } else {
                    pos_ = saved;
                    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE))
                        advance();
                    if (check(TokenType::SEMICOLON)) advance();
                }
            }
            continue;
        }

        // Anything else — skip to the next meaningful boundary
        if (!error_.empty()) break;
        // Skip to next ; or { or } to avoid token-by-token advancement
        while (!check(TokenType::SEMICOLON) && !check(TokenType::LBRACE) &&
               !check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE) &&
               !isTypeToken(peek().type) &&
               !check(TokenType::CLASS) && !check(TokenType::STRUCT) &&
               !check(TokenType::TEMPLATE)) {
            advance();
        }
        if (check(TokenType::SEMICOLON)) advance();
        else if (check(TokenType::LBRACE)) skipBracedBlock();
        else if (check(TokenType::RBRACE)) advance();
    }

    if (!prog->mainFunc && error_.empty()) {
        error_ = "程序需以 int main() { ... } 开头";
    }

    return prog;
}

// ============================================================
// Class Definition
// ============================================================

std::unique_ptr<ClassDef> Parser::parseClassDef() {
    auto cls = std::make_unique<ClassDef>();
    cls->lineNumber = peek().lineNumber;

    if (match(TokenType::CLASS)) cls->parentKind = "class";
    else if (match(TokenType::STRUCT)) cls->parentKind = "struct";
    else return nullptr;

    Token nameTok = consume(TokenType::IDENTIFIER, "期望类名");
    cls->name = nameTok.text;

    // Inheritance
    if (match(TokenType::COLON)) {
        do {
            std::string access = "private"; // default for class
            if (cls->parentKind == "struct") access = "public";

            if (match(TokenType::PUBLIC)) access = "public";
            else if (match(TokenType::PROTECTED)) access = "protected";
            else if (match(TokenType::PRIVATE)) access = "private";

            if (match(TokenType::VIRTUAL)) {
                // virtual inheritance — store access before "virtual"
            }

            std::string baseName;
            Token baseTok = consume(TokenType::IDENTIFIER, "期望基类名");
            baseName = baseTok.text;
            cls->baseClasses.push_back(baseName);
            cls->accessForBase.push_back(access);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::LBRACE, "期望 '{'");
    parseClassBody(cls.get());
    if (!error_.empty()) return cls;
    consume(TokenType::RBRACE, "期望 '}'");

    consume(TokenType::SEMICOLON, "期望类定义后的 ';'");

    return cls;
}

void Parser::parseClassBody(ClassDef* cls) {
    std::string currentAccess = (cls->parentKind == "struct") ? "public" : "private";

    int loopCount = 0;
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE) && error_.empty()) {
        if (++loopCount > 50000) {
            error_ = "parseClassBody超过50000次，位置=" + std::to_string(pos_) + " token=" + peek().text;
            return;
        }
        // Access specifier
        if (check(TokenType::PUBLIC)) { advance(); consume(TokenType::COLON, "期望 ':'"); currentAccess = "public"; continue; }
        if (check(TokenType::PRIVATE)) { advance(); consume(TokenType::COLON, "期望 ':'"); currentAccess = "private"; continue; }
        if (check(TokenType::PROTECTED)) { advance(); consume(TokenType::COLON, "期望 ':'"); currentAccess = "protected"; continue; }

        // friend declaration — skip
        if (check(TokenType::FRIEND)) {
            advance();
            while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) advance();
            if (check(TokenType::SEMICOLON)) advance();
            continue;
        }

        // static member variable or function
        if (check(TokenType::STATIC)) {
            advance();
            // Peek ahead: if the next pattern is type name(...) it's a function
            auto saved = pos_;
            std::string type = parseType();
            std::string name = consume(TokenType::IDENTIFIER, "期望成员名").text;
            if (check(TokenType::LPAREN)) {
                // It's a static member function
                pos_ = saved;
                auto func = parseFunctionDef(true);
                cls->methods.push_back(std::move(func));
            } else {
                // It's a static member variable
                MemberVar mv;
                mv.type = type;
                mv.name = name;
                mv.isStatic = true;
                if (match(TokenType::ASSIGN)) mv.init = parseExpression();
                cls->memberVars.push_back(std::move(mv));
                cls->staticVars[name] = type;
                consume(TokenType::SEMICOLON, "期望 ';'");
            }
            continue;
        }

        // explicit keyword
        bool hasExplicit = false;
        if (match(TokenType::EXPLICIT)) {
            hasExplicit = true;
        }

        // Constructor: ClassName(...)
        if (check(TokenType::IDENTIFIER) && peek().text == cls->name &&
            pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LPAREN) {
            Token nameTok = advance(); // consume class name
            auto func = std::make_unique<FunctionDef>();
            func->lineNumber = nameTok.lineNumber;
            func->name = nameTok.text;
            func->isConstructor = true;
            func->returnType = "";
            // Parse parameters
            consume(TokenType::LPAREN, "期望 '('");
            if (!check(TokenType::RPAREN)) {
                do {
                    if (check(TokenType::RPAREN)) break;
                    // Handle const qualifier on parameter
                    bool isConst = match(TokenType::CONST);
                    std::string pType = parseType();
                    if (isConst) pType = "const " + pType;
                    if (match(TokenType::AMPERSAND)) { pType += "&"; }
                    if (check(TokenType::STAR)) { advance(); pType += "*"; }
                    std::string pName;
                    if (check(TokenType::IDENTIFIER)) {
                        pName = consume(TokenType::IDENTIFIER, "期望参数名").text;
                    }
                    func->params.push_back({pType, pName});
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "期望 ')'");
            // Optional initializer list: : member(expr), ...
            if (match(TokenType::COLON)) {
                do {
                    // memberName(expr)
                    if (check(TokenType::IDENTIFIER)) advance(); // skip member name
                    if (match(TokenType::LPAREN)) {
                        int parenDepth = 1;
                        while (parenDepth > 0 && !check(TokenType::END_OF_FILE)) {
                            if (check(TokenType::LPAREN)) parenDepth++;
                            if (check(TokenType::RPAREN)) parenDepth--;
                            advance();
                        }
                    }
                } while (match(TokenType::COMMA));
            }
            // Body
            if (check(TokenType::LBRACE)) {
                auto block = parseBlock();
                auto* bp = dynamic_cast<BlockStmt*>(block.get());
                if (bp) for (auto& s : bp->stmts) func->body.push_back(std::move(s));
            } else if (hasExplicit) {
                consume(TokenType::SEMICOLON, "期望 ';'");
            }
            if (!cls->constructor) cls->constructor = std::move(func);
            continue;
        }

        // Destructor: ~ClassName(...) or virtual ~ClassName(...)
        bool isVirtualDtor = false;
        if (check(TokenType::VIRTUAL) && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::TILDE) {
            advance(); // consume virtual
            isVirtualDtor = true;
        }
        if (check(TokenType::TILDE) && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::IDENTIFIER &&
            tokens_[pos_ + 1].text == cls->name) {
            advance(); // consume ~
            Token name = consume(TokenType::IDENTIFIER, "期望类名");
            auto func = std::make_unique<FunctionDef>();
            func->lineNumber = name.lineNumber;
            func->name = "~" + cls->name;
            func->isDestructor = true;
            func->isVirtual = isVirtualDtor;
            consume(TokenType::LPAREN, "期望 '('");
            consume(TokenType::RPAREN, "期望 ')'");
            if (check(TokenType::LBRACE)) {
                auto block = parseBlock();
                auto* bp = dynamic_cast<BlockStmt*>(block.get());
                if (bp) for (auto& s : bp->stmts) func->body.push_back(std::move(s));
            } else if (match(TokenType::ASSIGN)) {
                // = default; / = delete;
                if (match(TokenType::DEFAULT) || match(TokenType::DELETE) ||
                    (match(TokenType::IDENTIFIER) && previous().text == "default")) {
                    consume(TokenType::SEMICOLON, "期望 ';'");
                } else {
                    consume(TokenType::SEMICOLON, "期望 ';'");
                }
            }
            if (!cls->destructor) cls->destructor = std::move(func);
            continue;
        }

        // Member function or member variable
        // Also handle user-defined types: IDENTIFIER followed by *, &, or another IDENTIFIER
        bool looksLikeType = isTypeToken(peek().type) ||
            (peek().type == TokenType::IDENTIFIER && pos_ + 1 < tokens_.size() &&
             (tokens_[pos_ + 1].type == TokenType::STAR ||
              tokens_[pos_ + 1].type == TokenType::AMPERSAND ||
              tokens_[pos_ + 1].type == TokenType::IDENTIFIER ||
              tokens_[pos_ + 1].type == TokenType::LT));  // template like Foo<T>
        if (looksLikeType) {
            auto saved = pos_;
            std::string type = parseType();

            // Skip *, & between type and name: e.g., "T* func(...)"
            while (check(TokenType::STAR) || check(TokenType::AMPERSAND)) {
                type += advance().text;
            }
            if ((check(TokenType::IDENTIFIER) || check(TokenType::OPERATOR)) &&
                pos_ + 1 < tokens_.size() &&
                (tokens_[pos_ + 1].type == TokenType::LPAREN ||
                 (check(TokenType::OPERATOR) && pos_ + 2 < tokens_.size() &&
                  tokens_[pos_ + 2].type == TokenType::LPAREN))) {
                // It's a member function: type name(...) or type operatorX(...)
                pos_ = saved;
                auto func = parseFunctionDef(true);
                cls->methods.push_back(std::move(func));
            } else {
                // It's a member variable
                // type name;
                // type name = init;
                if (check(TokenType::IDENTIFIER) || check(TokenType::STAR) ||
                    check(TokenType::AMPERSAND)) {
                    // Handle pointers and references: T* name, T& name
                    while (check(TokenType::STAR)) { advance(); type += "*"; }
                    if (check(TokenType::AMPERSAND)) { advance(); type += "&"; }
                    std::string name;
                    if (check(TokenType::IDENTIFIER))
                        name = consume(TokenType::IDENTIFIER, "期望成员名").text;
                    MemberVar mv;
                    mv.type = type;
                    mv.name = name;
                    if (match(TokenType::ASSIGN)) mv.init = parseExpression();
                    cls->memberVars.push_back(std::move(mv));
                    consume(TokenType::SEMICOLON, "期望 ';'");
                } else {
                    // Not a valid declaration — restore and skip
                    pos_ = saved;
                    advance();
                }
            }
            continue;
        }

        // virtual / override / const keywords followed by member function
        if (check(TokenType::VIRTUAL) || check(TokenType::OVERRIDE)) {
            auto func = parseFunctionDef(true);
            cls->methods.push_back(std::move(func));
            continue;
        }

        // Skip anything unknown in class body
        advance();
    }
}

std::unique_ptr<TemplateClassDef> Parser::parseTemplateClass() {
    auto tc = std::make_unique<TemplateClassDef>();
    tc->lineNumber = peek().lineNumber;
    consume(TokenType::TEMPLATE, "期望 'template'");
    tc->params = parseTemplateParams();
    // Register template parameter names as known types for body parsing
    auto savedTypes = knownTypes_;
    for (auto& p : tc->params) {
        if (!p.name.empty()) knownTypes_.insert(p.name);
    }
    tc->classDef = parseClassDef();
    knownTypes_ = savedTypes;
    return tc;
}

std::unique_ptr<TemplateFuncDef> Parser::parseTemplateFunc() {
    auto tf = std::make_unique<TemplateFuncDef>();
    tf->lineNumber = peek().lineNumber;
    consume(TokenType::TEMPLATE, "期望 'template'");
    tf->params = parseTemplateParams();
    // Register template parameter names as known types for body parsing
    auto savedTypes = knownTypes_;
    for (auto& p : tf->params) {
        if (!p.name.empty()) knownTypes_.insert(p.name);
    }
    tf->func = parseFunctionDef(false);
    knownTypes_ = savedTypes;
    return tf;
}

// ============================================================
// Function Definition
// ============================================================

std::unique_ptr<FunctionDef> Parser::parseFunctionDef(bool isMethod) {
    auto func = std::make_unique<FunctionDef>();
    func->lineNumber = peek().lineNumber;

    // Virtual specifier
    if (match(TokenType::VIRTUAL)) func->isVirtual = true;

    // Return type
    func->returnType = parseType();
    // Handle pointer/reference return type: T*, T&
    while (check(TokenType::STAR)) { advance(); func->returnType += "*"; }
    if (check(TokenType::AMPERSAND)) { advance(); func->returnType += "&"; }

    // Function name or operator overload
    if (check(TokenType::OPERATOR)) {
        advance(); // consume 'operator'
        std::string opName = "operator";
        // Read the operator symbol(s)
        Token opTok = advance(); // consume the operator token
        opName += opTok.text;
        func->name = opName;
    } else {
        Token nameTok = consume(TokenType::IDENTIFIER, "期望函数名");
        func->name = nameTok.text;
    }

    // Parameters
    consume(TokenType::LPAREN, "期望 '('");
    if (!check(TokenType::RPAREN)) {
        do {
            if (check(TokenType::RPAREN)) break;
            bool isConst = match(TokenType::CONST);
            std::string pType = parseType();
            if (isConst) pType = "const " + pType;
            // Handle reference & and pointer *
            if (match(TokenType::AMPERSAND)) { pType += "&"; }
            if (check(TokenType::STAR)) { advance(); pType += "*"; }
            std::string pName;
            if (check(TokenType::IDENTIFIER)) {
                pName = consume(TokenType::IDENTIFIER, "期望参数名").text;
            }
            func->params.push_back({pType, pName});
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "期望 ')'");

    // const member function
    if (match(TokenType::CONST)) func->isConst = true;

    // Skip noexcept if present
    if (match(TokenType::IDENTIFIER) && previous().text == "noexcept") {
        // noexcept consumed — nothing extra to record
    }

    // override
    if (match(TokenType::OVERRIDE)) func->isOverride = true;

    // Pure virtual
    if (match(TokenType::ASSIGN)) {
        if (match(TokenType::INT_LITERAL) && previous().text == "0") {
            func->isPureVirtual = true;
        }
    }

    // Body or just declaration
    if (func->isPureVirtual) {
        consume(TokenType::SEMICOLON, "期望 ';'");
    } else if (check(TokenType::LBRACE)) {
        auto block = parseBlock();
        auto* bp = dynamic_cast<BlockStmt*>(block.get());
        if (bp) for (auto& s : bp->stmts) func->body.push_back(std::move(s));
    } else if (match(TokenType::ASSIGN)) {
        // = default; / = delete; / = 0; (pure virtual already handled above)
        if (match(TokenType::DEFAULT) || match(TokenType::DELETE) ||
            (match(TokenType::IDENTIFIER) && previous().text == "default")) {
            consume(TokenType::SEMICOLON, "期望 ';'");
        } else {
            consume(TokenType::SEMICOLON, "期望 ';'");
        }
    } else if (!isMethod) {
        consume(TokenType::SEMICOLON, "期望函数体或 ';'");
    }

    return func;
}

// ============================================================
// Statements
// ============================================================

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (check(TokenType::LBRACE)) return parseBlock();
    if (check(TokenType::IF)) return parseIf();
    if (check(TokenType::WHILE)) return parseWhile();
    if (check(TokenType::DO)) return parseDoWhile();
    if (check(TokenType::FOR)) return parseFor();
    if (check(TokenType::BREAK)) return parseBreak();
    if (check(TokenType::CONTINUE)) return parseContinue();
    if (check(TokenType::CIN)) return parseCin();
    if (check(TokenType::COUT)) return parseCout();
    // Handle std::cout and std::cin
    if (check(TokenType::STD) && pos_ + 1 < tokens_.size() &&
        tokens_[pos_ + 1].type == TokenType::SCOPE) {
        size_t saved = pos_;
        advance(); // consume std
        advance(); // consume ::
        if (check(TokenType::COUT)) { pos_ = saved; return parseCout(); }
        if (check(TokenType::CIN))  { pos_ = saved; return parseCin(); }
        pos_ = saved;
    }
    if (check(TokenType::RETURN)) return parseReturn();
    if (check(TokenType::TRY)) return parseTryCatch();
    if (check(TokenType::THROW)) return parseThrow();
    if (check(TokenType::SWITCH)) return parseSwitch();
    if (check(TokenType::CLASS) || check(TokenType::STRUCT) || check(TokenType::TEMPLATE)) {
        // Nested class/template — skip
        while (!check(TokenType::END_OF_FILE)) advance();
        return nullptr;
    }

    // delete / new expression statements
    if (check(TokenType::DELETE) || check(TokenType::NEW))
        return parseAssignmentOrExpr();

    // Declaration, assignment, or expression statement
    if (isTypeToken(peek().type) || check(TokenType::IDENTIFIER)) {
        // Peek ahead to distinguish declaration from assignment
        // If "type name" pattern followed by name not followed by ( or ::
        if (isTypeToken(peek().type)) {
            auto saved = pos_;
            std::string type = parseType();
            if (check(TokenType::IDENTIFIER)) {
                std::string name = peek().text;
                size_t next = pos_ + 1;
                // If next is ( → function declaration, but we shouldn't be in a function body
                // If next is :: → qualified name
                // If next is , or ; → declaration
                // If next is = → declaration with init
                // Otherwise → could be expression
                if (next < tokens_.size()) {
                    auto nt = tokens_[next].type;
                    if (nt == TokenType::LPAREN) {
                        // Constructor-style declaration: Type var(args)
                        pos_ = saved;
                        return parseDeclaration();
                    }
                    // Declaration: type identifier [,;=]
                    pos_ = saved;
                    return parseDeclaration();
                }
            }
            pos_ = saved;
        }

        // Check for range-for pattern: for (auto x : container)
        // Already handled by parseFor()

        // Check for user-defined type declaration: ClassName varName ...
        if (isKnownType()) {
            auto saved = pos_;
            std::string type = parseType();
            if (check(TokenType::IDENTIFIER)) {
                pos_ = saved;
                return parseDeclaration();
            }
            pos_ = saved;
        }

        return parseAssignmentOrExpr();
    }

    // Empty statement ;
    if (check(TokenType::SEMICOLON)) { advance(); return std::make_unique<BlockStmt>(); }

    advance();
    return nullptr;
}

std::unique_ptr<Stmt> Parser::parseDeclaration() {
    auto stmt = std::make_unique<DeclStmt>();
    stmt->lineNumber = peek().lineNumber;

    stmt->typeName = parseType();

    // Handle pointer: Type* var
    while (match(TokenType::STAR)) stmt->typeName += "*";

    do {
        Token nameTok = consume(TokenType::IDENTIFIER, "期望变量名");
        stmt->names.push_back(nameTok.text);

        // Handle reference: int& x = ...
        if (match(TokenType::AMPERSAND)) {
            stmt->typeName += "&";
        }

        if (match(TokenType::ASSIGN)) {
            stmt->inits.push_back(parseExpression());
        } else if (check(TokenType::LPAREN)) {
            // Constructor call: Type var(args)
            advance(); // consume '('
            auto call = std::make_unique<CallExpr>();
            call->lineNumber = nameTok.lineNumber;
            auto callee = std::make_unique<VarExpr>();
            callee->name = stmt->typeName;
            call->callee = std::move(callee);
            if (!check(TokenType::RPAREN)) {
                do {
                    call->args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "期望 ')'");
            stmt->inits.push_back(std::move(call));
        } else {
            stmt->inits.push_back(nullptr);
        }
    } while (match(TokenType::COMMA));

    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseAssignmentOrExpr() {
    if (check(TokenType::IDENTIFIER)) {
        auto saved = pos_;
        // Try parsing as expression first
        auto expr = parseExpression();

        // If followed by ;, it's an expression statement
        if (check(TokenType::SEMICOLON)) {
            advance();
            auto es = std::make_unique<ExprStmt>();
            es->expr = std::move(expr);
            return es;
        }

        // Otherwise try as assignment
        pos_ = saved;
        auto stmt = std::make_unique<AssignStmt>();
        stmt->lineNumber = peek().lineNumber;

        Token nameTok = consume(TokenType::IDENTIFIER, "期望变量名");
        stmt->name = nameTok.text;

        // Handle member access on LHS: obj.member = expr
        if (check(TokenType::DOT)) {
            advance();
            Token memberTok = consume(TokenType::IDENTIFIER, "期望成员名");
            auto memberExpr = std::make_unique<MemberAccessExpr>();
            auto varExpr = std::make_unique<VarExpr>();
            varExpr->name = stmt->name;
            memberExpr->object = std::move(varExpr);
            memberExpr->member = memberTok.text;
            stmt->memberExpr = std::move(memberExpr);
            stmt->name = "";
        }

        // Handle index on LHS
        if (check(TokenType::LBRACKET)) {
            advance();
            stmt->index = parseExpression();
            consume(TokenType::RBRACKET, "期望 ']'");
        }

        // Match assignment operator
        if (match(TokenType::ASSIGN)) stmt->op = AssignOp::EQ;
        else if (match(TokenType::PLUSEQ)) stmt->op = AssignOp::PLUSEQ;
        else if (match(TokenType::MINUSEQ)) stmt->op = AssignOp::MINUSEQ;
        else if (match(TokenType::STAREQ)) stmt->op = AssignOp::STAREQ;
        else if (match(TokenType::SLASHEQ)) stmt->op = AssignOp::SLASHEQ;
        else if (match(TokenType::PERCENTEQ)) stmt->op = AssignOp::PERCENTEQ;
        else {
            // Not an assignment — fall back to expression statement
            // Backtrack and skip until semicolon
            pos_ = saved;
            while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE))
                advance();
            if (check(TokenType::SEMICOLON)) advance();
            return std::make_unique<ExprStmt>();
        }

        stmt->expr = parseExpression();
        consume(TokenType::SEMICOLON, "期望 ';'");
        return stmt;
    }

    // Not an identifier — expression statement
    auto expr = parseExpression();
    consume(TokenType::SEMICOLON, "期望 ';'");
    auto es = std::make_unique<ExprStmt>();
    es->expr = std::move(expr);
    return es;
}

std::unique_ptr<Stmt> Parser::parseIf() {
    auto stmt = std::make_unique<IfStmt>();
    stmt->lineNumber = advance().lineNumber;
    consume(TokenType::LPAREN, "期望 '('");
    stmt->cond = parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");
    stmt->thenBody = parseStatement();
    if (match(TokenType::ELSE)) {
        stmt->elseBody = parseStatement();
    }
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseWhile() {
    auto stmt = std::make_unique<WhileStmt>();
    stmt->lineNumber = advance().lineNumber;
    consume(TokenType::LPAREN, "期望 '('");
    stmt->cond = parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");
    stmt->body = parseStatement();
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseDoWhile() {
    auto stmt = std::make_unique<DoWhileStmt>();
    stmt->lineNumber = advance().lineNumber;
    stmt->body = parseStatement();
    consume(TokenType::WHILE, "期望 'while'");
    consume(TokenType::LPAREN, "期望 '('");
    stmt->cond = parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseFor() {
    auto line = advance().lineNumber;
    consume(TokenType::LPAREN, "期望 '('");

    // Check for range-for: for (type name : container)  or  for (const type& name : container)
    auto saved = pos_;
    bool hasConst = match(TokenType::CONST);
    if (isTypeToken(peek().type) || check(TokenType::AUTO)) {
        std::string varType = hasConst ? "const " : "";
        varType += parseType();
        while (match(TokenType::STAR)) varType += "*";
        bool hasRef = match(TokenType::AMPERSAND);
        if (hasRef) varType += "&";
        if (check(TokenType::IDENTIFIER)) {
            std::string varName = consume(TokenType::IDENTIFIER, "期望变量名").text;
            if (match(TokenType::COLON)) {
                return parseRangeFor(varType, varName);
            }
        }
    }
    pos_ = saved;

    // Traditional for loop
    auto stmt = std::make_unique<ForStmt>();
    stmt->lineNumber = line;

    // Init
    if (!check(TokenType::SEMICOLON)) {
        if (isTypeToken(peek().type)) {
            stmt->init = parseDeclaration();
        } else {
            stmt->init = parseAssignmentOrExpr();
        }
    } else {
        advance();
    }

    // Cond
    if (!check(TokenType::SEMICOLON)) {
        stmt->cond = parseExpression();
    }
    consume(TokenType::SEMICOLON, "期望 ';'");

    // Update
    if (!check(TokenType::RPAREN)) {
        stmt->update = parseExpression();
    }
    consume(TokenType::RPAREN, "期望 ')'");

    stmt->body = parseStatement();
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseRangeFor(const std::string& varType, const std::string& varName) {
    auto stmt = std::make_unique<RangeForStmt>();
    stmt->varType = varType;
    stmt->varName = varName;
    stmt->container = parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");
    stmt->body = parseStatement();
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseBreak() {
    auto stmt = std::make_unique<BreakStmt>();
    stmt->lineNumber = advance().lineNumber;
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseContinue() {
    auto stmt = std::make_unique<ContinueStmt>();
    stmt->lineNumber = advance().lineNumber;
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseBlock() {
    auto stmt = std::make_unique<BlockStmt>();
    stmt->lineNumber = advance().lineNumber;
    int loopCount = 0;
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
        if (++loopCount > 50000) {
            error_ = "parseBlock超过50000次，位置=" + std::to_string(pos_) + " token=" + peek().text;
            return stmt;
        }
        auto s = parseStatement();
        if (s) stmt->stmts.push_back(std::move(s));
        else {
            // parseStatement returned nullptr without consuming — advance to avoid infinite loop
            if (!atEnd() && !check(TokenType::RBRACE)) advance();
        }
    }
    consume(TokenType::RBRACE, "期望 '}'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseCin() {
    auto stmt = std::make_unique<CinStmt>();
    // Handle both cin and std::cin
    if (check(TokenType::STD)) {
        advance(); // consume std
        consume(TokenType::SCOPE, "期望 '::'");
    }
    stmt->lineNumber = advance().lineNumber;
    consume(TokenType::INPUT_OP, "期望 '>>'");
    do {
        Token nameTok = consume(TokenType::IDENTIFIER, "期望变量名");
        stmt->names.push_back(nameTok.text);
    } while (match(TokenType::INPUT_OP));
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseCout() {
    auto stmt = std::make_unique<CoutStmt>();
    // Handle both cout and std::cout
    if (check(TokenType::STD)) {
        advance(); // consume std
        consume(TokenType::SCOPE, "期望 '::'");
    }
    stmt->lineNumber = advance().lineNumber;
    consume(TokenType::OUTPUT_OP, "期望 '<<'");
    do {
        // Handle bare endl and std::endl
        if (match(TokenType::ENDL)) {
            stmt->items.push_back({CoutItem::ENDL, nullptr});
        } else if (check(TokenType::STD) && pos_ + 1 < tokens_.size() &&
                   tokens_[pos_ + 1].type == TokenType::SCOPE &&
                   pos_ + 2 < tokens_.size() && tokens_[pos_ + 2].type == TokenType::ENDL) {
            advance(); advance(); advance(); // consume std :: endl
            stmt->items.push_back({CoutItem::ENDL, nullptr});
        } else {
            stmt->items.push_back({CoutItem::EXPR, parseExpression()});
        }
    } while (match(TokenType::OUTPUT_OP));
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseReturn() {
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->lineNumber = advance().lineNumber;
    if (!check(TokenType::SEMICOLON)) {
        stmt->expr = parseExpression();
    }
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseTryCatch() {
    auto stmt = std::make_unique<TryCatchStmt>();
    stmt->lineNumber = advance().lineNumber;
    stmt->tryBody = parseBlock();

    while (match(TokenType::CATCH)) {
        consume(TokenType::LPAREN, "期望 '('");
        std::string excType;
        if (match(TokenType::ELLIPSIS)) {
            excType = "...";
        } else {
            excType = parseType();
            // Consume optional variable name (e.g., "e" in "const GradeException& e")
            if (check(TokenType::IDENTIFIER)) advance();
        }
        consume(TokenType::RPAREN, "期望 ')'");
        auto body = parseBlock();
        stmt->catchClauses.push_back({excType, std::move(body)});
    }

    return stmt;
}

std::unique_ptr<Stmt> Parser::parseThrow() {
    auto stmt = std::make_unique<ThrowStmt>();
    stmt->lineNumber = advance().lineNumber;
    if (!check(TokenType::SEMICOLON)) {
        stmt->expr = parseExpression();
    }
    consume(TokenType::SEMICOLON, "期望 ';'");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseSwitch() {
    // Minimal switch support: parse and skip
    advance(); // switch
    consume(TokenType::LPAREN, "期望 '('");
    parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");
    consume(TokenType::LBRACE, "期望 '{'");
    int depth = 1;
    while (depth > 0 && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::LBRACE)) depth++;
        if (check(TokenType::RBRACE)) depth--;
        advance();
    }
    return std::make_unique<BlockStmt>();
}

// ============================================================
// Expressions
// ============================================================

std::unique_ptr<Expr> Parser::parseExpression() { return parseAssignmentExpr(); }

std::unique_ptr<Expr> Parser::parseAssignmentExpr() { return parseLogicalOr(); }

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (match(TokenType::OR)) {
        auto bin = std::make_unique<BinaryExpr>();
        bin->lineNumber = previous().lineNumber;
        bin->left = std::move(expr);
        bin->op = BinOp::OR;
        bin->right = parseLogicalAnd();
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto expr = parseEquality();
    while (match(TokenType::AND)) {
        auto bin = std::make_unique<BinaryExpr>();
        bin->lineNumber = previous().lineNumber;
        bin->left = std::move(expr);
        bin->op = BinOp::AND;
        bin->right = parseEquality();
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto expr = parseRelational();
    while (true) {
        BinOp op;
        if (match(TokenType::EQ)) op = BinOp::EQ;
        else if (match(TokenType::NE)) op = BinOp::NE;
        else break;
        auto bin = std::make_unique<BinaryExpr>();
        bin->lineNumber = previous().lineNumber;
        bin->left = std::move(expr);
        bin->op = op;
        bin->right = parseRelational();
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseRelational() {
    auto expr = parseAdditive();
    while (true) {
        BinOp op;
        if (match(TokenType::LT)) op = BinOp::LT;
        else if (match(TokenType::GT)) op = BinOp::GT;
        else if (match(TokenType::LE)) op = BinOp::LE;
        else if (match(TokenType::GE)) op = BinOp::GE;
        else break;
        auto bin = std::make_unique<BinaryExpr>();
        bin->lineNumber = previous().lineNumber;
        bin->left = std::move(expr);
        bin->op = op;
        bin->right = parseAdditive();
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto expr = parseMultiplicative();
    while (true) {
        BinOp op;
        if (match(TokenType::PLUS)) op = BinOp::ADD;
        else if (match(TokenType::MINUS)) op = BinOp::SUB;
        else if (match(TokenType::OUTPUT_OP)) op = BinOp::SHL;
        else if (match(TokenType::INPUT_OP)) op = BinOp::SHR;
        else break;
        auto bin = std::make_unique<BinaryExpr>();
        bin->lineNumber = previous().lineNumber;
        bin->left = std::move(expr);
        bin->op = op;
        bin->right = parseMultiplicative();
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    auto expr = parseUnary();
    while (true) {
        BinOp op;
        if (match(TokenType::STAR)) op = BinOp::MUL;
        else if (match(TokenType::SLASH)) op = BinOp::DIV;
        else if (match(TokenType::PERCENT)) op = BinOp::MOD;
        else break;
        auto bin = std::make_unique<BinaryExpr>();
        bin->lineNumber = previous().lineNumber;
        bin->left = std::move(expr);
        bin->op = op;
        bin->right = parseUnary();
        expr = std::move(bin);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (match(TokenType::MINUS)) {
        auto un = std::make_unique<UnaryExpr>();
        un->lineNumber = previous().lineNumber;
        un->op = UnaryOp::NEG;
        un->operand = parseUnary();
        return un;
    }
    if (match(TokenType::NOT)) {
        auto un = std::make_unique<UnaryExpr>();
        un->lineNumber = previous().lineNumber;
        un->op = UnaryOp::NOT;
        un->operand = parseUnary();
        return un;
    }
    if (match(TokenType::STAR)) {
        auto un = std::make_unique<UnaryExpr>();
        un->lineNumber = previous().lineNumber;
        un->op = UnaryOp::DEREF;
        un->operand = parseUnary();
        return un;
    }
    if (match(TokenType::AMPERSAND)) {
        auto un = std::make_unique<UnaryExpr>();
        un->lineNumber = previous().lineNumber;
        un->op = UnaryOp::ADDR;
        un->operand = parseUnary();
        return un;
    }
    if (match(TokenType::INCREMENT)) {
        auto un = std::make_unique<UnaryExpr>();
        un->lineNumber = previous().lineNumber;
        un->op = UnaryOp::PRE_INC;
        un->operand = parseUnary();
        return un;
    }
    if (match(TokenType::DECREMENT)) {
        auto un = std::make_unique<UnaryExpr>();
        un->lineNumber = previous().lineNumber;
        un->op = UnaryOp::PRE_DEC;
        un->operand = parseUnary();
        return un;
    }
    if (match(TokenType::NEW)) return parseNewExpr();
    if (match(TokenType::DELETE)) return parseDeleteExpr();
    return parsePostfix();
}

std::unique_ptr<Expr> Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        // Postfix ++ and --
        if (match(TokenType::INCREMENT)) {
            auto un = std::make_unique<UnaryExpr>();
            un->lineNumber = previous().lineNumber;
            un->op = UnaryOp::POST_INC;
            un->operand = std::move(expr);
            expr = std::move(un);
            continue;
        }
        if (match(TokenType::DECREMENT)) {
            auto un = std::make_unique<UnaryExpr>();
            un->lineNumber = previous().lineNumber;
            un->op = UnaryOp::POST_DEC;
            un->operand = std::move(expr);
            expr = std::move(un);
            continue;
        }

        // Function call: expr(args)
        if (check(TokenType::LPAREN)) {
            auto call = std::make_unique<CallExpr>();
            call->lineNumber = peek().lineNumber;
            call->callee = std::move(expr);
            advance(); // (
            if (!check(TokenType::RPAREN)) {
                do {
                    if (check(TokenType::RPAREN)) break;
                    call->args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "期望 ')'");
            expr = std::move(call);
            continue;
        }

        // Index: expr[index]
        if (check(TokenType::LBRACKET)) {
            auto idx = std::make_unique<IndexExpr>();
            idx->lineNumber = peek().lineNumber;
            idx->object = std::move(expr);
            advance(); // [
            idx->index = parseExpression();
            consume(TokenType::RBRACKET, "期望 ']'");
            expr = std::move(idx);
            continue;
        }

        // Member access: expr.member or expr->member
        if (check(TokenType::DOT) || check(TokenType::ARROW)) {
            auto ma = std::make_unique<MemberAccessExpr>();
            ma->lineNumber = peek().lineNumber;
            ma->object = std::move(expr);
            ma->isArrow = (advance().type == TokenType::ARROW);
            Token memberTok = consume(TokenType::IDENTIFIER, "期望成员名");
            ma->member = memberTok.text;
            expr = std::move(ma);
            continue;
        }

        // Scope resolution: expr::member
        if (check(TokenType::SCOPE)) {
            auto ma = std::make_unique<MemberAccessExpr>();
            ma->lineNumber = peek().lineNumber;
            ma->object = std::move(expr);
            advance(); // consume ::
            // Might be identifier or operator keyword
            if (check(TokenType::OPERATOR)) {
                advance(); // operator
                Token opTok = advance();
                ma->member = "operator" + opTok.text;
            } else {
                Token memberTok = consume(TokenType::IDENTIFIER, "期望成员名");
                ma->member = memberTok.text;
            }
            expr = std::move(ma);
            continue;
        }

        break;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    int line = peek().lineNumber;

    // this
    if (match(TokenType::THIS)) {
        auto e = std::make_unique<ThisExpr>();
        e->lineNumber = line;
        return e;
    }

    // nullptr
    if (match(TokenType::NULLPTR)) {
        auto e = std::make_unique<NullptrExpr>();
        e->lineNumber = line;
        return e;
    }

    // Lambda: [capture](params){ body } or [capture](params) -> type { body }
    if (check(TokenType::LBRACKET) && pos_ + 1 < tokens_.size() &&
        (tokens_[pos_ + 1].type == TokenType::RBRACKET ||
         tokens_[pos_ + 1].type == TokenType::AMPERSAND ||
         tokens_[pos_ + 1].type == TokenType::ASSIGN)) {
        return parseLambda();
    }

    // Vector/list/set literal starting with type prefix: vector<int>{1,2,3}
    if ((check(TokenType::VECTOR) || check(TokenType::LIST) || check(TokenType::SET)) &&
        pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LT) {
        advance(); // container keyword
        advance(); // <
        std::string elemType = parseType();
        consume(TokenType::GT, "期望 '>' (primary)");
        if (check(TokenType::LBRACE)) return parseVectorLiteral();
        // Not a literal — too complex, return dummy
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = 0;
        return lit;
    }

    // Brace-enclosed initializer: {1, 2, 3}
    if (check(TokenType::LBRACE)) {
        return parseVectorLiteral();
    }

    // Literals
    if (match(TokenType::INT_LITERAL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::get<int>(previous().literalValue);
        return lit;
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::get<float>(previous().literalValue);
        return lit;
    }
    if (match(TokenType::DOUBLE_LITERAL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::get<double>(previous().literalValue);
        return lit;
    }
    if (match(TokenType::CHAR_LITERAL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::get<char>(previous().literalValue);
        return lit;
    }
    if (match(TokenType::BOOL_LITERAL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::get<bool>(previous().literalValue);
        return lit;
    }
    if (match(TokenType::STRING_LITERAL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::get<std::string>(previous().literalValue);
        return lit;
    }

    // endl used as expression (e.g., in os << endl where os is a non-cout stream)
    if (match(TokenType::ENDL)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->lineNumber = line;
        lit->value = std::string("\n");
        return lit;
    }

    // Cast expressions: static_cast<T>(expr), dynamic_cast, const_cast, reinterpret_cast
    if (check(TokenType::IDENTIFIER)) {
        std::string id = peek().text;
        if ((id == "static_cast" || id == "dynamic_cast" || id == "const_cast" || id == "reinterpret_cast") &&
            pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LT) {
            return parseCastExpr();
        }
    }

    // Identifier
    if (check(TokenType::IDENTIFIER)) {
        auto var = std::make_unique<VarExpr>();
        var->lineNumber = line;
        var->name = peek().text;
        advance();
        return var;
    }

    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "期望 ')'");
        return expr;
    }

    error_ = "第" + std::to_string(line) + "行: 期望表达式, 遇到了 '" + peek().text + "'";
    return std::make_unique<LiteralExpr>();
}

std::unique_ptr<Expr> Parser::parseVectorLiteral() {
    auto vec = std::make_unique<VectorLiteralExpr>();
    vec->lineNumber = peek().lineNumber;
    if (!check(TokenType::LBRACE)) {
        // If we got here via type prefix, LBRACE should be next
        if (!match(TokenType::LBRACE)) {
            error_ = "期望 '{'";
            return vec;
        }
    } else {
        advance();
    }
    if (!check(TokenType::RBRACE)) {
        vec->elements.push_back(parseExpression());
        while (match(TokenType::COMMA)) {
            if (check(TokenType::RBRACE)) break;
            vec->elements.push_back(parseExpression());
        }
    }
    consume(TokenType::RBRACE, "期望 '}'");
    return vec;
}

std::unique_ptr<Expr> Parser::parseLambda() {
    auto lambda = std::make_unique<LambdaExpr>();
    lambda->lineNumber = peek().lineNumber;

    // Capture: [=], [&], [], [x, &y]
    consume(TokenType::LBRACKET, "期望 '['");
    while (!check(TokenType::RBRACKET) && !check(TokenType::END_OF_FILE)) advance();
    consume(TokenType::RBRACKET, "期望 ']'");

    // Params
    consume(TokenType::LPAREN, "期望 '('");
    if (!check(TokenType::RPAREN)) {
        do {
            if (check(TokenType::RPAREN)) break;
            // Handle typed parameters: (int a, int b) or (auto x)
            if (isTypeToken(peek().type) || isKnownType()) {
                parseType(); // consume the type
            }
            if (check(TokenType::IDENTIFIER)) {
                std::string pName = advance().text;
                lambda->params.push_back(pName);
            }
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "期望 ')'");

    // Optional return type
    if (match(TokenType::ARROW)) {
        // Skip return type
        parseType();
    }

    // Body
    if (check(TokenType::LBRACE)) {
        lambda->body = parseBlock();
    } else {
        lambda->body = std::make_unique<BlockStmt>();
    }

    return lambda;
}

std::unique_ptr<Expr> Parser::parseNewExpr() {
    auto expr = std::make_unique<NewExpr>();
    expr->lineNumber = previous().lineNumber;

    expr->typeName = parseType();

    // new Type[size]
    if (match(TokenType::LBRACKET)) {
        expr->isArray = true;
        expr->arraySize = parseExpression();
        consume(TokenType::RBRACKET, "期望 ']'");
        // new Type[size]{...} brace initializer
        if (check(TokenType::LBRACE)) {
            expr->braceInit = parseVectorLiteral();
        }
    }
    // new Type{...} or new Type(args)
    else if (check(TokenType::LBRACE)) {
        expr->braceInit = parseVectorLiteral();
    }
    else if (check(TokenType::LPAREN)) {
        advance();
        if (!check(TokenType::RPAREN)) {
            do {
                if (check(TokenType::RPAREN)) break;
                expr->constructorArgs.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "期望 ')'");
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseDeleteExpr() {
    auto expr = std::make_unique<DeleteExpr>();
    expr->lineNumber = previous().lineNumber;

    if (check(TokenType::LBRACKET) && pos_ + 1 < tokens_.size() &&
        tokens_[pos_ + 1].type == TokenType::RBRACKET) {
        advance(); advance();
        expr->isArray = true;
    }

    expr->target = parseExpression();
    return expr;
}

std::unique_ptr<Expr> Parser::parseCastExpr() {
    auto expr = std::make_unique<CastExpr>();
    expr->lineNumber = peek().lineNumber;

    // Consume the cast kind: static_cast, dynamic_cast, etc.
    std::string kind = advance().text; // e.g., "static_cast"
    // Strip "_cast" suffix
    if (kind.size() > 5 && kind.substr(kind.size() - 5) == "_cast")
        kind = kind.substr(0, kind.size() - 5);
    expr->castKind = kind;

    consume(TokenType::LT, "期望 '<'");
    expr->targetType = parseType();
    consume(TokenType::GT, "期望 '>' (cast)");

    consume(TokenType::LPAREN, "期望 '('");
    expr->expr = parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");

    return expr;
}
