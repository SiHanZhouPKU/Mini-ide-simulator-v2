#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <variant>

enum class TokenType {
    // Delimiters
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, SEMICOLON, COMMA, COLON,
    ELLIPSIS, // ...

    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NE, LT, GT, LE, GE,
    ASSIGN, PLUSEQ, MINUSEQ, STAREQ, SLASHEQ, PERCENTEQ,
    AND, OR, NOT,
    INCREMENT, DECREMENT,

    // Primitive types
    INT, FLOAT, DOUBLE, CHAR, BOOL, STRING, VOID, AUTO,
    LONG, SHORT, UNSIGNED,

    // STL container types
    VECTOR, LIST, MAP, SET, STACK, QUEUE, DEQUE,

    // Control flow
    IF, ELSE, WHILE, DO, FOR, BREAK, RETURN, CONTINUE,
    SWITCH, CASE, DEFAULT,

    // I/O
    CIN, COUT, ENDL, STD, NAMESPACE, USING, INCLUDE,
    FSTREAM, IFSTREAM, OFSTREAM,

    // Class / OOP keywords
    CLASS, STRUCT, PUBLIC, PRIVATE, PROTECTED,
    VIRTUAL, OVERRIDE, FINAL, CONST, STATIC, FRIEND,
    THIS, NEW, DELETE, NULLPTR, EXPLICIT, OPERATOR,

    // Template keywords
    TEMPLATE, TYPENAME,

    // Exception keywords
    TRY, CATCH, THROW,

    // Memory / pointer symbols
    DOT, ARROW, SCOPE, AMPERSAND, STAR_PTR,

    // Literal types
    IDENTIFIER,
    INT_LITERAL, FLOAT_LITERAL, DOUBLE_LITERAL, CHAR_LITERAL, BOOL_LITERAL, STRING_LITERAL,

    // Stream operators
    INPUT_OP,    // >>
    OUTPUT_OP,   // <<

    // Misc symbols
    TILDE,       // ~

    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string text;
    int lineNumber;
    std::variant<int, float, double, char, bool, std::string, std::monostate> literalValue;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
    std::string error() const { return error_; }

private:
    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    std::string error_;

    char peek() const;
    char advance();
    void skipWhitespaceAndComments();
    Token readNumber();
    Token readString();
    Token readChar();
    Token readIdentifierOrKeyword();
    Token makeToken(TokenType type, const std::string& text);
};

#endif // LEXER_H