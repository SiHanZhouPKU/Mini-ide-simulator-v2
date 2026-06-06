#include "lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source) : source_(source) {}

char Lexer::peek() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char Lexer::advance() {
    char c = peek();
    if (c == '\n') ++line_;
    if (c != '\0') ++pos_;
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (true) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
            while (peek() != '\n' && peek() != '\0') advance();
        } else if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
            advance(); advance();
            while (!(peek() == '*' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') && peek() != '\0') {
                advance();
            }
            if (peek() == '*') { advance(); advance(); }
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenType type, const std::string& text) {
    return {type, text, line_, {}};
}

Token Lexer::readNumber() {
    std::string num;
    int startLine = line_;
    bool isFloat = false;

    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        num += advance();
    }
    if (peek() == '.') {
        isFloat = true;
        num += advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) num += advance();
    }

    // Check for double suffix (e.g. 3.14d or 3.14lf)
    bool isDouble = false;
    if (peek() == 'd' || peek() == 'D') {
        isDouble = true;
        advance();
    } else if (peek() == 'l' || peek() == 'L') {
        // long double — treat as double
        isDouble = true;
        advance();
        if (peek() == 'f' || peek() == 'F') advance();
    } else if (peek() == 'f' || peek() == 'F') {
        isFloat = true;
        advance();
    }

    Token tok;
    tok.lineNumber = startLine;
    tok.text = num;
    if (isDouble) {
        tok.type = TokenType::DOUBLE_LITERAL;
        tok.literalValue = std::stod(num);
    } else if (isFloat) {
        tok.type = TokenType::FLOAT_LITERAL;
        tok.literalValue = std::stof(num);
    } else {
        tok.type = TokenType::INT_LITERAL;
        tok.literalValue = std::stoi(num);
    }
    return tok;
}

Token Lexer::readString() {
    int startLine = line_;
    advance(); // skip opening "
    std::string s;
    while (peek() != '"' && peek() != '\0') {
        if (peek() == '\\') {
            advance();
            char c = advance();
            switch (c) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                default: s += c; break;
            }
        } else {
            s += advance();
        }
    }
    bool unterminated = (peek() == '\0');
    if (!unterminated) advance();
    if (unterminated) {
        if (s.empty())
            error_ = "第" + std::to_string(startLine) + "行: 字符串缺少左边的引号";
        else
            error_ = "第" + std::to_string(startLine) + "行: 字符串缺少右边的引号";
    }
    Token tok;
    tok.type = unterminated ? TokenType::UNKNOWN : TokenType::STRING_LITERAL;
    tok.text = s;
    tok.lineNumber = startLine;
    tok.literalValue = s;
    return tok;
}

Token Lexer::readChar() {
    int startLine = line_;
    advance(); // skip '
    char c = advance();
    if (c == '\\') {
        char next = advance();
        switch (next) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case '0': c = '\0'; break;
            case '\'': c = '\''; break;
            default: c = next; break;
        }
    }
    if (peek() == '\'') advance();
    Token tok;
    tok.type = TokenType::CHAR_LITERAL;
    tok.text = std::string("'") + c + "'";
    tok.lineNumber = startLine;
    tok.literalValue = c;
    return tok;
}

Token Lexer::readIdentifierOrKeyword() {
    std::string word;
    int startLine = line_;

    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        word += advance();
    }

    static const struct {
        const char* name;
        TokenType type;
    } keywords[] = {
        // Primitive types
        {"int", TokenType::INT},
        {"float", TokenType::FLOAT},
        {"double", TokenType::DOUBLE},
        {"char", TokenType::CHAR},
        {"bool", TokenType::BOOL},
        {"string", TokenType::STRING},
        {"void", TokenType::VOID},
        {"auto", TokenType::AUTO},
        {"long", TokenType::LONG},
        {"short", TokenType::SHORT},
        {"unsigned", TokenType::UNSIGNED},

        // STL containers
        {"vector", TokenType::VECTOR},
        {"list", TokenType::LIST},
        {"map", TokenType::MAP},
        {"set", TokenType::SET},
        {"stack", TokenType::STACK},
        {"queue", TokenType::QUEUE},
        {"deque", TokenType::DEQUE},

        // Control flow
        {"if", TokenType::IF},
        {"else", TokenType::ELSE},
        {"while", TokenType::WHILE},
        {"do", TokenType::DO},
        {"for", TokenType::FOR},
        {"break", TokenType::BREAK},
        {"return", TokenType::RETURN},
        {"continue", TokenType::CONTINUE},
        {"switch", TokenType::SWITCH},
        {"case", TokenType::CASE},
        {"default", TokenType::DEFAULT},

        // I/O
        {"cin", TokenType::CIN},
        {"cout", TokenType::COUT},
        {"endl", TokenType::ENDL},
        {"std", TokenType::STD},
        {"using", TokenType::USING},
        {"namespace", TokenType::NAMESPACE},
        {"include", TokenType::INCLUDE},
        {"fstream", TokenType::FSTREAM},
        {"ifstream", TokenType::IFSTREAM},
        {"ofstream", TokenType::OFSTREAM},

        // OOP keywords
        {"class", TokenType::CLASS},
        {"struct", TokenType::STRUCT},
        {"public", TokenType::PUBLIC},
        {"private", TokenType::PRIVATE},
        {"protected", TokenType::PROTECTED},
        {"virtual", TokenType::VIRTUAL},
        {"override", TokenType::OVERRIDE},
        {"final", TokenType::FINAL},
        {"const", TokenType::CONST},
        {"static", TokenType::STATIC},
        {"friend", TokenType::FRIEND},
        {"this", TokenType::THIS},
        {"new", TokenType::NEW},
        {"delete", TokenType::DELETE},
        {"nullptr", TokenType::NULLPTR},
        {"explicit", TokenType::EXPLICIT},
        {"operator", TokenType::OPERATOR},

        // Templates
        {"template", TokenType::TEMPLATE},
        {"typename", TokenType::TYPENAME},

        // Exception
        {"try", TokenType::TRY},
        {"catch", TokenType::CATCH},
        {"throw", TokenType::THROW},

        // Boolean literals
        {"true", TokenType::BOOL_LITERAL},
        {"false", TokenType::BOOL_LITERAL},
    };

    for (auto& kw : keywords) {
        if (word == kw.name) {
            Token tok;
            tok.type = kw.type;
            tok.text = word;
            tok.lineNumber = startLine;
            if (kw.type == TokenType::BOOL_LITERAL) {
                tok.literalValue = (word == "true");
            }
            return tok;
        }
    }

    return {TokenType::IDENTIFIER, word, startLine, {}};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();
        char c = peek();
        if (c == '\0') break;

        // Skip preprocessor directives (#include, #define, etc.)
        if (c == '#') {
            std::string directive;
            while (peek() != '\n' && peek() != '\0') {
                directive += advance();
            }
            continue;
        }

        int startLine = line_;

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
            continue;
        }
        if (c == '~') {
            advance();
            tokens.push_back({TokenType::TILDE, "~", startLine, {}});
            continue;
        }
        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }
        if (c == '\'') {
            tokens.push_back(readChar());
            continue;
        }

        advance();

        switch (c) {
            case '(': tokens.push_back(makeToken(TokenType::LPAREN, "(")); break;
            case ')': tokens.push_back(makeToken(TokenType::RPAREN, ")")); break;
            case '{': tokens.push_back(makeToken(TokenType::LBRACE, "{")); break;
            case '}': tokens.push_back(makeToken(TokenType::RBRACE, "}")); break;
            case '[': tokens.push_back(makeToken(TokenType::LBRACKET, "[")); break;
            case ']': tokens.push_back(makeToken(TokenType::RBRACKET, "]")); break;
            case ';': tokens.push_back(makeToken(TokenType::SEMICOLON, ";")); break;
            case ',': tokens.push_back(makeToken(TokenType::COMMA, ",")); break;
            case '.':
                // Check for ellipsis ...
                if (peek() == '.' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '.') {
                    advance(); advance(); // consume two more dots
                    tokens.push_back(makeToken(TokenType::ELLIPSIS, "..."));
                }
                // Check for float literal starting with . (e.g. .5)
                else if (std::isdigit(static_cast<unsigned char>(peek()))) {
                    std::string num = ".";
                    while (std::isdigit(static_cast<unsigned char>(peek()))) num += advance();
                    Token tok;
                    tok.type = TokenType::FLOAT_LITERAL;
                    tok.text = num;
                    tok.lineNumber = startLine;
                    tok.literalValue = std::stof(num);
                    tokens.push_back(tok);
                } else {
                    tokens.push_back(makeToken(TokenType::DOT, "."));
                }
                break;
            case ':':
                if (peek() == ':') { advance(); tokens.push_back(makeToken(TokenType::SCOPE, "::")); }
                else { tokens.push_back(makeToken(TokenType::COLON, ":")); }
                break;
            case '+':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::PLUSEQ, "+=")); }
                else if (peek() == '+') { advance(); tokens.push_back(makeToken(TokenType::INCREMENT, "++")); }
                else { tokens.push_back(makeToken(TokenType::PLUS, "+")); }
                break;
            case '-':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::MINUSEQ, "-=")); }
                else if (peek() == '-') { advance(); tokens.push_back(makeToken(TokenType::DECREMENT, "--")); }
                else if (peek() == '>') { advance(); tokens.push_back(makeToken(TokenType::ARROW, "->")); }
                else { tokens.push_back(makeToken(TokenType::MINUS, "-")); }
                break;
            case '*':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::STAREQ, "*=")); }
                else { tokens.push_back(makeToken(TokenType::STAR, "*")); }
                break;
            case '/':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::SLASHEQ, "/=")); }
                else { tokens.push_back(makeToken(TokenType::SLASH, "/")); }
                break;
            case '%':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::PERCENTEQ, "%=")); }
                else { tokens.push_back(makeToken(TokenType::PERCENT, "%")); }
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::NE, "!=")); }
                else { tokens.push_back(makeToken(TokenType::NOT, "!")); }
                break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::EQ, "==")); }
                else { tokens.push_back(makeToken(TokenType::ASSIGN, "=")); }
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::LE, "<=")); }
                else if (peek() == '<') { advance(); tokens.push_back(makeToken(TokenType::OUTPUT_OP, "<<")); }
                else { tokens.push_back(makeToken(TokenType::LT, "<")); }
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::GE, ">=")); }
                else if (peek() == '>') { advance(); tokens.push_back(makeToken(TokenType::INPUT_OP, ">>")); }
                else { tokens.push_back(makeToken(TokenType::GT, ">")); }
                break;
            case '&':
                if (peek() == '&') { advance(); tokens.push_back(makeToken(TokenType::AND, "&&")); }
                else { tokens.push_back(makeToken(TokenType::AMPERSAND, "&")); }
                break;
            case '|':
                if (peek() == '|') { advance(); tokens.push_back(makeToken(TokenType::OR, "||")); }
                else { tokens.push_back(makeToken(TokenType::UNKNOWN, "|")); }
                break;
            default:
                tokens.push_back(makeToken(TokenType::UNKNOWN, std::string(1, c)));
                break;
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, "EOF", line_, {}});
    return tokens;
}
