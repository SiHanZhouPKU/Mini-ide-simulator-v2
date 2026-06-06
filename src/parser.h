#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <vector>
#include <string>
#include <set>
#include "ast.h"
#include "lexer.h"

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::unique_ptr<Program> parse();
    std::string error() const { return error_; }

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::string error_;

    // Token helpers
    const Token& peek() const;
    const Token& previous() const;
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    Token consume(TokenType type, const std::string& msg);
    bool atEnd() const;

    // Top level
    void skipPreprocessing();
    std::unique_ptr<ClassDef> parseClassDef();
    std::unique_ptr<TemplateClassDef> parseTemplateClass();
    std::unique_ptr<TemplateFuncDef> parseTemplateFunc();
    std::unique_ptr<FunctionDef> parseFunctionDef(bool isMethod = false);
    void parseClassBody(ClassDef* cls);

    // Statements
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Stmt> parseDeclaration();
    std::unique_ptr<Stmt> parseAssignmentOrExpr();
    std::unique_ptr<Stmt> parseIf();
    std::unique_ptr<Stmt> parseWhile();
    std::unique_ptr<Stmt> parseDoWhile();
    std::unique_ptr<Stmt> parseFor();
    std::unique_ptr<Stmt> parseRangeFor(const std::string& varType, const std::string& varName);
    std::unique_ptr<Stmt> parseBreak();
    std::unique_ptr<Stmt> parseContinue();
    std::unique_ptr<Stmt> parseReturn();
    std::unique_ptr<Stmt> parseBlock();
    std::unique_ptr<Stmt> parseCin();
    std::unique_ptr<Stmt> parseCout();
    std::unique_ptr<Stmt> parseTryCatch();
    std::unique_ptr<Stmt> parseThrow();
    std::unique_ptr<Stmt> parseSwitch();

    // Expressions
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAssignmentExpr();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfix();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parseVectorLiteral();
    std::unique_ptr<Expr> parseLambda();
    std::unique_ptr<Expr> parseNewExpr();
    std::unique_ptr<Expr> parseDeleteExpr();
    std::unique_ptr<Expr> parseCastExpr();

    // Helpers
    bool isTypeToken(TokenType t) const;
    bool isKnownType() const;
    void registerClassName(const std::string& name);
    std::string parseType();
    std::vector<TemplateParam> parseTemplateParams();
    void skipBracedBlock();

    // Known user-defined type names
    std::set<std::string> knownTypes_;
};

#endif // PARSER_H
