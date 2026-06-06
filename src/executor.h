#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <memory>
#include <map>
#include <string>
#include <sstream>
#include <functional>
#include <stack>
#include "ast.h"
#include "snapshot.h"

// ============================================================
// Runtime type system for objects
// ============================================================

struct RuntimeClass {
    std::string name;
    std::vector<std::string> baseClasses;
    std::map<std::string, std::string> memberTypes;   // name -> type
    std::map<std::string, int> memberOffsets;          // name -> offset (for vtable-like dispatch)
    // Virtual function table: method_name -> RuntimeClass::method_impl
    std::map<std::string, std::unique_ptr<FunctionDef>> vtable;
    // Non-virtual methods
    std::map<std::string, std::unique_ptr<FunctionDef>> methods;
    std::unique_ptr<FunctionDef> constructor;
    std::unique_ptr<FunctionDef> destructor;
    bool isAbstract = false;
};

struct Exception {
    std::string type;
    VariantValue value;
    int lineNumber;
};

class Executor {
public:
    Executor();

    void setAst(Program* prog);
    void setSnapshotManager(SnapshotManager* mgr);

    void runAll();
    void step();
    void reset();
    bool isFinished() const;
    int executedCount() const { return executedCount_; }

    void setCinCallback(std::function<std::string()> cb) { onCinRequest_ = cb; }
    void setFinishCallback(std::function<void()> cb) { onFinish_ = cb; }
    void setYieldCallback(std::function<void()> cb) { onYield_ = cb; }
    std::string error() const { return error_; }

    const std::map<std::string, std::pair<int, VariantValue>>& memoryMap() const { return memoryMap_; }

private:
    Program* prog_ = nullptr;
    SnapshotManager* snapshots_ = nullptr;

    // Variable storage: name → value
    std::map<std::string, VariantValue> vars_;
    // Type aliases for template instantiation (e.g., T → double)
    std::map<std::string, std::string> typeAliases_;
    // Current "this" pointer for method execution
    std::shared_ptr<RuntimeObject> currentThis_;

    std::vector<Stmt*> stepQueue_;
    size_t stepPos_ = 0;
    int executedCount_ = 0;

    // Sub-step state for TryCatchStmt: execute one inner statement per step()
    bool inTryCatch_ = false;
    std::vector<Stmt*> tryCatchStmts_;
    std::vector<Stmt*> catchStmts_;  // catch bodies to iterate
    bool inCatch_ = false;
    // Stack for nested try/catch
    struct TryCatchState { std::vector<Stmt*> tryStmts; std::vector<Stmt*> catStmts; bool inCat; };
    std::vector<TryCatchState> tryCatchStack_;
    std::stringstream consoleOut_;
    std::string error_;
    bool finished_ = false;

    // Runtime class registry
    std::map<std::string, RuntimeClass> classes_;

    // Exception stack
    std::stack<Exception> exceptionStack_;
    bool unwinding_ = false;

    // Break/continue signals
    bool breakRequested_ = false;
    bool continueRequested_ = false;

    std::function<std::string()> onCinRequest_;
    std::function<void()> onFinish_;
    std::function<void()> onYield_;

    // Memory allocator
    int nextAddress_ = 0x1000;
    std::map<std::string, std::pair<int, VariantValue>> memoryMap_;

    // ======== Expression Evaluation ========
    VariantValue evalExpr(Expr* expr);
    VariantValue evalBinary(BinaryExpr* bin);
    VariantValue evalUnary(UnaryExpr* un);

    // ======== Statement Execution ========
    int execStmt(Stmt* stmt);
    int execDecl(DeclStmt* stmt);
    int execAssign(AssignStmt* stmt);
    int execIf(IfStmt* stmt);
    int execWhile(WhileStmt* stmt);
    int execDoWhile(DoWhileStmt* stmt);
    int execFor(ForStmt* stmt);
    int execRangeFor(RangeForStmt* stmt);
    int execBreak(BreakStmt* stmt);
    int execContinue(ContinueStmt* stmt);
    int execBlock(BlockStmt* stmt);
    int execCin(CinStmt* stmt);
    int execCout(CoutStmt* stmt);
    void evaluateCoutExpr(Expr* expr);
    int execReturn(ReturnStmt* stmt);
    int execTryCatch(TryCatchStmt* stmt);
    int execThrow(ThrowStmt* stmt);
    int execExprStmt(ExprStmt* stmt);

    // ======== Helpers ========
    void takeSnapshot(int line);
    void collectStmts(Stmt* stmt, std::vector<Stmt*>& out);
    VariantValue castToType(const VariantValue& val, const std::string& typeName);
    VariantValue* findVariable(const std::string& name);

    void allocateMemoryForVar(const std::string& name, const VariantValue& val);
    void updateMemoryForVar(const std::string& name, const VariantValue& val);

    // Object model
    void registerClasses();
    bool ensureTemplateInstantiation(const std::string& fullTypeName);
    std::shared_ptr<RuntimeObject> createObject(const std::string& className,
                                                 const std::vector<VariantValue>& ctorArgs = {});
    VariantValue accessMember(const VariantValue& obj, const std::string& member, bool isArrow);
    VariantValue callMethod(const VariantValue& obj, const std::string& method,
                            const std::vector<VariantValue>& args);
    VariantValue callOperator(const std::string& op, const VariantValue& lhs, const VariantValue& rhs);
    void execFunctionBody(FunctionDef* func);

    // Operator overload helpers
    bool tryOperatorOverload(BinOp op, const VariantValue& lhs, const VariantValue& rhs, VariantValue& result);
};

#endif // EXECUTOR_H
