#ifndef AST_H
#define AST_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <map>

struct Expr;
struct Stmt;

// ============================================================
// Base node
// ============================================================

struct ASTNode {
    int lineNumber = 0;
    virtual ~ASTNode() = default;
};

// ============================================================
// Expressions
// ============================================================

struct Expr : ASTNode {};

struct LiteralExpr : Expr {
    std::variant<int, float, double, char, bool, std::string, std::nullptr_t> value;
};

struct VarExpr : Expr {
    std::string name;
};

struct VectorLiteralExpr : Expr {
    std::string elementType; // "int", "float", etc.
    std::vector<std::unique_ptr<Expr>> elements;
};

struct IndexExpr : Expr {
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
};

struct MemberAccessExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string member;
    bool isArrow = false; // true for ->, false for .
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee; // function name or member access
    std::vector<std::unique_ptr<Expr>> args;
};

struct NewExpr : Expr {
    std::string typeName;
    bool isArray = false;
    std::unique_ptr<Expr> arraySize;
    std::vector<std::unique_ptr<Expr>> constructorArgs;
    std::unique_ptr<Expr> braceInit;
};

struct DeleteExpr : Expr {
    std::unique_ptr<Expr> target;
    bool isArray = false;
};

struct ThisExpr : Expr {};

struct NullptrExpr : Expr {};

struct CastExpr : Expr {
    std::string targetType;
    std::string castKind; // "static", "dynamic", "const", "reinterpret"
    std::unique_ptr<Expr> expr;
};

struct LambdaExpr : Expr {
    std::vector<std::string> params;
    std::unique_ptr<Stmt> body; // usually a BlockStmt
};

enum class BinOp { ADD, SUB, MUL, DIV, MOD,
                   EQ, NE, LT, GT, LE, GE,
                   AND, OR, SHL, SHR };

struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    BinOp op;
    std::unique_ptr<Expr> right;
};

enum class UnaryOp { NEG, NOT, DEREF, ADDR, PRE_INC, PRE_DEC, POST_INC, POST_DEC };

struct UnaryExpr : Expr {
    UnaryOp op;
    std::unique_ptr<Expr> operand;
};

// ============================================================
// Statements
// ============================================================

struct Stmt : ASTNode {};

struct DeclStmt : Stmt {
    std::string typeName;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<Expr>> inits;
};

enum class AssignOp { EQ, PLUSEQ, MINUSEQ, STAREQ, SLASHEQ, PERCENTEQ };

struct AssignStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> index;  // for vec[expr] or obj.member
    std::unique_ptr<Expr> memberExpr; // for obj.member = expr
    std::unique_ptr<Expr> expr;
    AssignOp op = AssignOp::EQ;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> thenBody;
    std::unique_ptr<Stmt> elseBody;
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> body;
};

struct DoWhileStmt : Stmt {
    std::unique_ptr<Stmt> body;
    std::unique_ptr<Expr> cond;
};

struct ForStmt : Stmt {
    std::unique_ptr<Stmt> init;
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Expr> update;
    std::unique_ptr<Stmt> body;
};

struct RangeForStmt : Stmt {
    std::string varName;
    std::string varType; // may be "auto"
    std::unique_ptr<Expr> container;
    std::unique_ptr<Stmt> body;
};

struct BreakStmt : Stmt {};

struct ContinueStmt : Stmt {};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

struct CinStmt : Stmt {
    std::vector<std::string> names;
};

struct CoutItem {
    enum Type { EXPR, ENDL };
    Type type;
    std::unique_ptr<Expr> expr;
};

struct CoutStmt : Stmt {
    std::vector<CoutItem> items;
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct TryCatchStmt : Stmt {
    std::unique_ptr<Stmt> tryBody;
    std::vector<std::pair<std::string, std::unique_ptr<Stmt>>> catchClauses; // (typeName, body)
    std::unique_ptr<Stmt> finallyBody; // optional
};

struct ThrowStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

// ============================================================
// Top-level structures
// ============================================================

struct FunctionDef : ASTNode {
    std::string returnType;
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; // (type, name)
    std::vector<std::unique_ptr<Stmt>> body;
    bool isVirtual = false;
    bool isPureVirtual = false;
    bool isOverride = false;
    bool isConst = false;
    bool isConstructor = false;
    bool isDestructor = false;
};

struct MemberVar {
    std::string type;
    std::string name;
    std::unique_ptr<Expr> init;
    bool isStatic = false;
    bool isConst = false;
};

struct ClassDef : ASTNode {
    std::string name;
    std::string parentKind = "class"; // class or struct
    std::vector<std::string> baseClasses;
    std::vector<std::string> accessForBase; // "public", "protected", "private"
    std::vector<MemberVar> memberVars;
    std::vector<std::unique_ptr<FunctionDef>> methods;
    std::unique_ptr<FunctionDef> constructor;
    std::unique_ptr<FunctionDef> destructor;
    bool isAbstract = false;
    std::map<std::string, std::string> staticVars; // static member initial values (name -> type)
};

struct TemplateParam {
    std::string name;
    std::string kind = "typename"; // "typename" or "class" or value type like "int"
};

struct TemplateClassDef : ASTNode {
    std::vector<TemplateParam> params;
    std::unique_ptr<ClassDef> classDef;
};

struct TemplateFuncDef : ASTNode {
    std::vector<TemplateParam> params;
    std::unique_ptr<FunctionDef> func;
};

struct Program : ASTNode {
    std::vector<std::unique_ptr<ClassDef>> classDefs;
    std::vector<std::unique_ptr<TemplateClassDef>> templateClasses;
    std::vector<std::unique_ptr<TemplateFuncDef>> templateFuncs;
    std::vector<std::unique_ptr<FunctionDef>> functions;
    std::unique_ptr<FunctionDef> mainFunc;
};

#endif // AST_H
