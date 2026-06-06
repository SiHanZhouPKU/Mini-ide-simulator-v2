#include "executor.h"
#include <functional>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <stdexcept>

Executor::Executor() {}

void Executor::setAst(Program* prog) {
    prog_ = prog;
    if (prog_) registerClasses();
}

void Executor::setSnapshotManager(SnapshotManager* mgr) { snapshots_ = mgr; }

void Executor::reset() {
    vars_.clear();
    currentThis_.reset();
    stepQueue_.clear();
    stepPos_ = 0;
    executedCount_ = 0;
    consoleOut_.str("");
    consoleOut_.clear();
    error_.clear();
    finished_ = false;
    unwinding_ = false;
    breakRequested_ = false;
    continueRequested_ = false;
    inTryCatch_ = false;
    tryCatchStmts_.clear();
    catchStmts_.clear();
    inCatch_ = false;
    tryCatchStack_.clear();
    inIfSubstep_ = false;
    ifBodyStmts_.clear();
    ifBodyPos_ = 0;
    inLoopSubstep_ = false;
    loopStmt_ = nullptr;
    loopBodyStmts_.clear();
    loopBodyPos_ = 0;
    loopStack_.clear();
    stepMode_ = false;
    while (!exceptionStack_.empty()) exceptionStack_.pop();
    nextAddress_ = 0x1000;
    memoryMap_.clear();
    classes_.clear();
    if (snapshots_) snapshots_->clear();
    if (prog_) registerClasses();
}

bool Executor::isFinished() const { return finished_; }

void Executor::runAll() {
    if (!prog_ || !prog_->mainFunc) return;
    reset();
    stepMode_ = false;  // runAll uses recursive execution, not substep
    // Directly execute main function body — compound statements
    // (if/while/for) handle their own recursion via execIf/execWhile/execFor.
    // No more pre-flattening, which caused double-execution of both if branches.
    for (auto& stmt : prog_->mainFunc->body) {
        int line = execStmt(stmt.get());
        if (!error_.empty()) break;
        takeSnapshot(stmt->lineNumber);
        executedCount_++;
    }
    finished_ = true;
    if (onFinish_) onFinish_();
}

void Executor::collectStmts(Stmt* stmt, std::vector<Stmt*>& out) {
    // BlockStmt is transparent — expand its children so they appear directly
    // in the step queue.  Compound statements (IfStmt, WhileStmt, ForStmt,
    // DoWhileStmt) handle their own bodies recursively and must NOT be expanded;
    // otherwise the un-taken if-branch ends up in the queue and gets executed
    // when it shouldn't.
    if (auto* blk = dynamic_cast<BlockStmt*>(stmt)) {
        for (auto& s : blk->stmts)
            collectStmts(s.get(), out);
    } else {
        out.push_back(stmt);
    }
}

void Executor::expandStmtBody(Stmt* body, std::vector<Stmt*>& out) {
    if (auto* blk = dynamic_cast<BlockStmt*>(body)) {
        for (auto& s : blk->stmts) out.push_back(s.get());
    } else if (body) {
        out.push_back(body);
    }
}

void Executor::loopContinueOrFinish() {
    auto* fs = dynamic_cast<ForStmt*>(loopStmt_);
    auto* ws = dynamic_cast<WhileStmt*>(loopStmt_);
    auto* dw = dynamic_cast<DoWhileStmt*>(loopStmt_);

    // Execute update expression (for-loop only)
    if (fs && fs->update) {
        evalExpr(fs->update.get());
        if (!error_.empty()) { inLoopSubstep_ = false; return; }
    }

    // Re-evaluate condition
    bool shouldContinue = false;
    if (fs && fs->cond) {
        VariantValue cond = evalExpr(fs->cond.get());
        if (cond.type == VariantValue::BOOL) shouldContinue = std::get<bool>(cond.value);
        else if (cond.type == VariantValue::INT) shouldContinue = (std::get<int>(cond.value) != 0);
        else if (cond.type == VariantValue::STRING) shouldContinue = !std::get<std::string>(cond.value).empty();
    } else if (ws) {
        VariantValue cond = evalExpr(ws->cond.get());
        if (cond.type == VariantValue::BOOL) shouldContinue = std::get<bool>(cond.value);
        else if (cond.type == VariantValue::INT) shouldContinue = (std::get<int>(cond.value) != 0);
        else if (cond.type == VariantValue::STRING) shouldContinue = !std::get<std::string>(cond.value).empty();
    } else if (dw) {
        VariantValue cond = evalExpr(dw->cond.get());
        if (cond.type == VariantValue::BOOL) shouldContinue = std::get<bool>(cond.value);
        else if (cond.type == VariantValue::INT) shouldContinue = (std::get<int>(cond.value) != 0);
        else if (cond.type == VariantValue::STRING) shouldContinue = !std::get<std::string>(cond.value).empty();
    } else if (fs) {
        // for-loop without condition → infinite loop
        shouldContinue = true;
    }

    if (!shouldContinue) {
        inLoopSubstep_ = false;
        return;
    }

    // Re-load body for next iteration
    loopBodyStmts_.clear();
    loopBodyPos_ = 0;
    Stmt* body = fs ? fs->body.get() : (ws ? ws->body.get() : (dw ? dw->body.get() : nullptr));
    if (body) expandStmtBody(body, loopBodyStmts_);
}

void Executor::step() {
    if (finished_ || !prog_ || !prog_->mainFunc) return;
    if (!error_.empty()) return;
    if (error_.empty() && unwinding_) return;

    stepMode_ = true;   // compound stmts set up substep instead of recursing

    if (stepQueue_.empty() && stepPos_ == 0) {
        std::vector<Stmt*> allStmts;
        for (auto& stmt : prog_->mainFunc->body)
            collectStmts(stmt.get(), allStmts);
        stepQueue_ = allStmts;
    }

    // ================================================================
    // Sub-step handlers — execute ONE inner statement per step() call
    // ================================================================

    // -- try / catch substep -----------------------------------------
    if (inTryCatch_) {
        if (tryCatchStmts_.empty()) {
            // Finish current try/catch, pop outer state if nested
            if (!tryCatchStack_.empty()) {
                auto saved = std::move(tryCatchStack_.back());
                tryCatchStack_.pop_back();
                tryCatchStmts_ = std::move(saved.tryStmts);
                catchStmts_ = std::move(saved.catStmts);
                inCatch_ = saved.inCat;
                inTryCatch_ = !tryCatchStmts_.empty() || inCatch_;
            } else {
                inTryCatch_ = false;
                if (stepPos_ < stepQueue_.size()) stepPos_++;
            }
            if (!inTryCatch_) return;
        }
        if (tryCatchStmts_.empty()) {
            inTryCatch_ = false;
            if (stepPos_ < stepQueue_.size()) stepPos_++;
            return;
        }
        Stmt* cur = tryCatchStmts_.front();
        tryCatchStmts_.erase(tryCatchStmts_.begin());
        int line = execStmt(cur);
        if (!error_.empty()) {
            if (!exceptionStack_.empty() && !inCatch_) {
                Exception exc = exceptionStack_.top(); exceptionStack_.pop();
                unwinding_ = false; error_.clear();
                vars_["e"] = exc.value;
                if (!catchStmts_.empty()) {
                    inCatch_ = true;
                    tryCatchStmts_ = std::move(catchStmts_);
                    catchStmts_.clear();
                    return;
                }
            }
            inTryCatch_ = false; tryCatchStmts_.clear(); catchStmts_.clear();
            if (stepPos_ < stepQueue_.size()) stepPos_++;
            return;
        }
        takeSnapshot(cur->lineNumber); executedCount_++;
        if (tryCatchStmts_.empty() && catchStmts_.empty()) {
            inTryCatch_ = false; tryCatchStmts_.clear(); catchStmts_.clear();
            if (stepPos_ < stepQueue_.size()) stepPos_++;
        }
        return;
    }

    // -- if / else-if substep ---------------------------------------
    if (inIfSubstep_) {
        if (ifBodyPos_ < ifBodyStmts_.size()) {
            Stmt* cur = ifBodyStmts_[ifBodyPos_++];
            int line = execStmt(cur);
            if (!error_.empty()) { inIfSubstep_ = false; return; }
            takeSnapshot(cur->lineNumber); executedCount_++;
            return;
        }
        // all body statements done
        inIfSubstep_ = false;
        return;
    }

    // -- loop (for / while / do-while) substep ----------------------
    if (inLoopSubstep_) {
        if (loopBodyPos_ < loopBodyStmts_.size()) {
            Stmt* cur = loopBodyStmts_[loopBodyPos_++];
            int line = execStmt(cur);
            if (!error_.empty()) { inLoopSubstep_ = false; return; }
            if (breakRequested_) {
                breakRequested_ = false; inLoopSubstep_ = false; return;
            }
            if (continueRequested_) {
                continueRequested_ = false;
                // skip remaining body statements this iteration
                loopBodyPos_ = loopBodyStmts_.size();
            }
            takeSnapshot(cur->lineNumber); executedCount_++;
            return;
        }
        // Body done — execute update (for-loop), re-check condition,
        // reload body for next iteration or finish.
        loopContinueOrFinish();

        // If inner loop ended, restore outer loop state from stack
        while (!inLoopSubstep_ && !loopStack_.empty()) {
            auto saved = std::move(loopStack_.back());
            loopStack_.pop_back();
            loopStmt_ = saved.stmt;
            loopBodyStmts_ = std::move(saved.bodyStmts);
            loopBodyPos_ = saved.bodyPos + 1;  // advance past the compound stmt
            inLoopSubstep_ = true;
            // If outer body also exhausted, re-check outer condition
            if (loopBodyPos_ >= loopBodyStmts_.size())
                loopContinueOrFinish();
        }

        if (inLoopSubstep_ && loopBodyPos_ < loopBodyStmts_.size()) {
            // kick off first stmt of the new iteration immediately
            Stmt* cur = loopBodyStmts_[loopBodyPos_++];
            int line = execStmt(cur);
            if (!error_.empty()) { inLoopSubstep_ = false; return; }
            takeSnapshot(cur->lineNumber); executedCount_++;
        }
        return;
    }

    // ================================================================
    // Normal execution — next top-level statement
    // ================================================================

    if (stepPos_ >= stepQueue_.size()) {
        finished_ = true;
        if (onFinish_) onFinish_();
        return;
    }

    Stmt* current = stepQueue_[stepPos_];
    stepPos_++;
    int line = 0;
    try {
        line = execStmt(current);
    } catch (const std::bad_variant_access& e) {
        error_ = std::string("第") + std::to_string(current->lineNumber) + "行: 内部错误(variant访问) - " + e.what();
        return;
    } catch (const std::exception& e) {
        error_ = std::string("第") + std::to_string(current->lineNumber) + "行: 内部错误 - " + e.what();
        return;
    }

    if (error_.empty()) {
        try {
            takeSnapshot(current->lineNumber);
            executedCount_++;
        } catch (const std::bad_variant_access& e) {
            error_ = std::string("快照错误(variant访问) - ") + e.what();
            return;
        } catch (const std::exception& e) {
            error_ = std::string("快照错误 - ") + e.what();
            return;
        }
    }

    if (stepPos_ >= stepQueue_.size()) {
        finished_ = true;
        if (onFinish_) onFinish_();
    }
}

// ============================================================
// Class Registration
// ============================================================

void Executor::registerClasses() {
    for (auto& cd : prog_->classDefs) {
        RuntimeClass rc;
        rc.name = cd->name;
        rc.baseClasses = cd->baseClasses;
        rc.isAbstract = cd->isAbstract;

        if (cd->constructor) {
            rc.constructor = std::make_unique<FunctionDef>();
            rc.constructor->name = cd->constructor->name;
            rc.constructor->params = cd->constructor->params;
        }

        if (cd->destructor) {
            rc.destructor = std::make_unique<FunctionDef>();
            rc.destructor->name = cd->destructor->name;
        }

        for (auto& m : cd->methods) {
            if (m->isVirtual) {
                rc.vtable[m->name] = std::make_unique<FunctionDef>();
                rc.vtable[m->name]->name = m->name;
                rc.vtable[m->name]->params = m->params;
                rc.vtable[m->name]->returnType = m->returnType;
                rc.vtable[m->name]->isVirtual = true;
                rc.vtable[m->name]->isPureVirtual = m->isPureVirtual;
            }
        }

        for (auto& mv : cd->memberVars) {
            rc.memberTypes[mv.name] = mv.type;
            rc.memberOffsets[mv.name] = static_cast<int>(rc.memberOffsets.size());
        }

        for (auto& [name, type] : cd->staticVars) {
            if (type == "int") vars_[name] = VariantValue(0);
            else if (type == "float") vars_[name] = VariantValue(0.0f);
            else if (type == "double") vars_[name] = VariantValue(0.0);
            else if (type == "char") vars_[name] = VariantValue('\0');
            else if (type == "bool") vars_[name] = VariantValue(false);
            else if (type == "string") vars_[name] = VariantValue(std::string(""));
            else vars_[name] = VariantValue(0);
        }

        classes_[cd->name] = std::move(rc);
    }
}

// ============================================================
// Object Model
// ============================================================

bool Executor::ensureTemplateInstantiation(const std::string& fullTypeName) {
    auto ltPos = fullTypeName.find('<');
    if (ltPos == std::string::npos) return false;

    std::string baseName = fullTypeName.substr(0, ltPos);

    if (classes_.count(fullTypeName)) return true;

    TemplateClassDef* tcd = nullptr;
    for (auto& tc : prog_->templateClasses) {
        if (tc->classDef->name == baseName) {
            tcd = tc.get();
            break;
        }
    }
    if (!tcd) return false;

    std::vector<std::string> tArgs;
    std::string argsStr = fullTypeName.substr(ltPos + 1);
    if (!argsStr.empty() && argsStr.back() == '>') argsStr.pop_back();
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i < argsStr.size(); ++i) {
        if (argsStr[i] == '<') depth++;
        else if (argsStr[i] == '>') depth--;
        else if (argsStr[i] == ',' && depth == 0) {
            tArgs.push_back(argsStr.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < argsStr.size())
        tArgs.push_back(argsStr.substr(start));

    std::map<std::string, std::string> subMap;
    for (size_t i = 0; i < tcd->params.size() && i < tArgs.size(); ++i)
        subMap[tcd->params[i].name] = tArgs[i];

    auto substitute = [&](const std::string& type) -> std::string {
        if (subMap.count(type)) return subMap[type];
        std::string result = type;
        for (auto& [param, concrete] : subMap) {
            size_t pos = 0;
            while ((pos = result.find(param, pos)) != std::string::npos) {
                result.replace(pos, param.size(), concrete);
                pos += concrete.size();
            }
        }
        return result;
    };

    RuntimeClass rc;
    rc.name = fullTypeName;
    for (auto& b : tcd->classDef->baseClasses)
        rc.baseClasses.push_back(substitute(b));
    rc.isAbstract = tcd->classDef->isAbstract;

    if (tcd->classDef->constructor) {
        rc.constructor = std::make_unique<FunctionDef>();
        rc.constructor->name = tcd->classDef->constructor->name;
        rc.constructor->returnType = substitute(tcd->classDef->constructor->returnType);
        for (auto& p : tcd->classDef->constructor->params)
            rc.constructor->params.push_back({substitute(p.first), p.second});
    }

    if (tcd->classDef->destructor) {
        rc.destructor = std::make_unique<FunctionDef>();
        rc.destructor->name = tcd->classDef->destructor->name;
    }

    for (auto& m : tcd->classDef->methods) {
        auto copy = std::make_unique<FunctionDef>();
        copy->name = m->name;
        copy->returnType = substitute(m->returnType);
        copy->isVirtual = m->isVirtual;
        copy->isPureVirtual = m->isPureVirtual;
        copy->isConst = m->isConst;
        for (auto& p : m->params)
            copy->params.push_back({substitute(p.first), p.second});
        if (m->isVirtual)
            rc.vtable[m->name] = std::move(copy);
        else
            rc.methods[m->name] = std::move(copy);
    }

    for (auto& mv : tcd->classDef->memberVars) {
        rc.memberTypes[mv.name] = substitute(mv.type);
        rc.memberOffsets[mv.name] = static_cast<int>(rc.memberOffsets.size());
    }

    classes_[fullTypeName] = std::move(rc);
    return true;
}

std::shared_ptr<RuntimeObject> Executor::createObject(const std::string& className,
                                                       const std::vector<VariantValue>& ctorArgs) {
    if (!classes_.count(className))
        ensureTemplateInstantiation(className);

    auto it = classes_.find(className);
    if (it == classes_.end()) {
        error_ = "未定义的类 '" + className + "'";
        return nullptr;
    }

    RuntimeClass& rc = it->second;
    if (rc.isAbstract) {
        error_ = "不能实例化抽象类 '" + className + "'";
        return nullptr;
    }

    auto obj = std::make_shared<RuntimeObject>();
    obj->className = className;
    obj->classDef = std::make_shared<RuntimeClass>();
    obj->classDef->name = rc.name;
    obj->classDef->baseClasses = rc.baseClasses;
    obj->classDef->isAbstract = rc.isAbstract;
    obj->classDef->memberTypes = rc.memberTypes;
    obj->classDef->memberOffsets = rc.memberOffsets;
    if (rc.constructor) {
        obj->classDef->constructor = std::make_unique<FunctionDef>();
        obj->classDef->constructor->name = rc.constructor->name;
        obj->classDef->constructor->params = rc.constructor->params;
    }
    if (rc.destructor) {
        obj->classDef->destructor = std::make_unique<FunctionDef>();
        obj->classDef->destructor->name = rc.destructor->name;
    }
    for (auto& [name, vt] : rc.vtable) {
        obj->classDef->vtable[name] = std::make_unique<FunctionDef>();
        obj->classDef->vtable[name]->name = vt->name;
        obj->classDef->vtable[name]->params = vt->params;
        obj->classDef->vtable[name]->returnType = vt->returnType;
        obj->classDef->vtable[name]->isVirtual = vt->isVirtual;
        obj->classDef->vtable[name]->isPureVirtual = vt->isPureVirtual;
    }
    for (auto& [name, mt] : rc.methods) {
        obj->classDef->methods[name] = std::make_unique<FunctionDef>();
        obj->classDef->methods[name]->name = mt->name;
        obj->classDef->methods[name]->params = mt->params;
        obj->classDef->methods[name]->returnType = mt->returnType;
    }

    std::string lookupName = className;
    ClassDef* classDef = nullptr;
    for (auto& cd : prog_->classDefs) {
        if (cd->name == lookupName) { classDef = cd.get(); break; }
    }
    if (!classDef) {
        std::string baseName = className;
        auto ltPos = baseName.find('<');
        if (ltPos != std::string::npos) baseName = baseName.substr(0, ltPos);
        for (auto& tc : prog_->templateClasses) {
            if (tc->classDef->name == baseName) {
                classDef = tc->classDef.get();
                break;
            }
        }
    }

    if (classDef) {
        for (auto& mv : classDef->memberVars) {
            if (mv.isStatic) continue;
            if (mv.init) {
                auto savedVars = vars_;
                VariantValue initVal = evalExpr(mv.init.get());
                obj->members[mv.name] = initVal;
                vars_ = savedVars;
            } else {
                std::string memberType = mv.type;
                auto rmtIt = rc.memberTypes.find(mv.name);
                if (rmtIt != rc.memberTypes.end()) memberType = rmtIt->second;

                VariantValue defVal;
                if (memberType == "int") defVal = VariantValue(0);
                else if (memberType == "float") defVal = VariantValue(0.0f);
                else if (memberType == "double") defVal = VariantValue(0.0);
                else if (memberType == "char") defVal = VariantValue('\0');
                else if (memberType == "bool") defVal = VariantValue(false);
                else if (memberType == "string") defVal = VariantValue(std::string(""));
                else if (memberType.find("vector<") == 0 && memberType.back() == '>') {
                    std::string inner = memberType.substr(7, memberType.size() - 8);
                    if (inner == "int") defVal = VariantValue(std::vector<int>{});
                    else if (inner == "float") defVal = VariantValue(std::vector<float>{});
                    else if (inner == "double") defVal = VariantValue(std::vector<double>{});
                    else if (inner == "string") defVal = VariantValue(std::vector<std::string>{});
                    else defVal = VariantValue(std::vector<std::shared_ptr<RuntimeObject>>{});
                }
                obj->members[mv.name] = defVal;
            }
        }

        if (!ctorArgs.empty()) {
            std::vector<std::string> memberOrder;
            for (auto& baseName : obj->classDef->baseClasses) {
                for (auto& cd : prog_->classDefs) {
                    if (cd->name == baseName) {
                        for (auto& mv : cd->memberVars) {
                            if (mv.isStatic) continue;
                            memberOrder.push_back(mv.name);
                        }
                        break;
                    }
                }
            }
            for (auto& mv : classDef->memberVars) {
                if (mv.isStatic) continue;
                memberOrder.push_back(mv.name);
            }
            for (size_t i = 0; i < ctorArgs.size() && i < memberOrder.size(); ++i) {
                obj->members[memberOrder[i]] = ctorArgs[i];
            }
        }
    }
    return obj;
}

VariantValue Executor::accessMember(const VariantValue& obj, const std::string& member, bool isArrow) {
    if (isArrow) {
        if (obj.type != VariantValue::POINTER && obj.type != VariantValue::OBJECT) {
            error_ = "-> 只能用于指针或对象类型";
            return VariantValue(0);
        }
    }

    if (obj.type == VariantValue::OBJECT || obj.type == VariantValue::POINTER) {
        auto* rtObjPtr = std::get_if<std::shared_ptr<RuntimeObject>>(&obj.value);
        if (!rtObjPtr || !*rtObjPtr) {
            error_ = "空对象指针";
            return VariantValue(0);
        }
        auto& rtObj = *rtObjPtr;
        auto memIt = rtObj->members.find(member);
        if (memIt != rtObj->members.end()) {
            return memIt->second;
        }
        error_ = "类 '" + rtObj->className + "' 没有成员 '" + member + "'";
        return VariantValue(0);
    }

    error_ = "不能对非对象类型进行成员访问";
    return VariantValue(0);
}

VariantValue Executor::callMethod(const VariantValue& obj, const std::string& method,
                                   const std::vector<VariantValue>& args) {
    if (method == "push_back" && args.size() == 1) {
        if (obj.type == VariantValue::VECTOR_INT) {
            auto& v = const_cast<std::vector<int>&>(std::get<std::vector<int>>(obj.value));
            VariantValue a = args[0];
            if (a.type == VariantValue::INT) v.push_back(std::get<int>(a.value));
            else if (a.type == VariantValue::FLOAT) v.push_back(static_cast<int>(std::get<float>(a.value)));
            else if (a.type == VariantValue::DOUBLE) v.push_back(static_cast<int>(std::get<double>(a.value)));
            return VariantValue(0);
        }
        if (obj.type == VariantValue::VECTOR_FLOAT) {
            auto& v = const_cast<std::vector<float>&>(std::get<std::vector<float>>(obj.value));
            VariantValue a = args[0];
            if (a.type == VariantValue::FLOAT) v.push_back(std::get<float>(a.value));
            else if (a.type == VariantValue::INT) v.push_back(static_cast<float>(std::get<int>(a.value)));
            else if (a.type == VariantValue::DOUBLE) v.push_back(static_cast<float>(std::get<double>(a.value)));
            return VariantValue(0);
        }
        if (obj.type == VariantValue::VECTOR_DOUBLE) {
            auto& v = const_cast<std::vector<double>&>(std::get<std::vector<double>>(obj.value));
            VariantValue a = args[0];
            if (a.type == VariantValue::DOUBLE) v.push_back(std::get<double>(a.value));
            else if (a.type == VariantValue::FLOAT) v.push_back(static_cast<double>(std::get<float>(a.value)));
            else if (a.type == VariantValue::INT) v.push_back(static_cast<double>(std::get<int>(a.value)));
            return VariantValue(0);
        }
        if (obj.type == VariantValue::VECTOR_STRING) {
            auto& v = const_cast<std::vector<std::string>&>(std::get<std::vector<std::string>>(obj.value));
            v.push_back(args[0].toString());
            return VariantValue(0);
        }
        if (obj.type == VariantValue::VECTOR_OBJECT) {
            auto& v = const_cast<std::vector<std::shared_ptr<RuntimeObject>>&>(std::get<std::vector<std::shared_ptr<RuntimeObject>>>(obj.value));
            if (args[0].type == VariantValue::POINTER || args[0].type == VariantValue::OBJECT) {
                auto ptr = std::get<std::shared_ptr<RuntimeObject>>(args[0].value);
                if (ptr) v.push_back(ptr);
            }
            return VariantValue(0);
        }
        if (obj.type == VariantValue::LIST_STRING) {
            auto& v = const_cast<std::list<std::string>&>(std::get<std::list<std::string>>(obj.value));
            v.push_back(args[0].toString());
            return VariantValue(0);
        }
        if (obj.type == VariantValue::LIST_INT) {
            auto& v = const_cast<std::list<int>&>(std::get<std::list<int>>(obj.value));
            VariantValue a = args[0];
            if (a.type == VariantValue::INT) v.push_back(std::get<int>(a.value));
            else if (a.type == VariantValue::FLOAT) v.push_back(static_cast<int>(std::get<float>(a.value)));
            else if (a.type == VariantValue::DOUBLE) v.push_back(static_cast<int>(std::get<double>(a.value)));
            return VariantValue(0);
        }
        return VariantValue(0);
    }
    if (method == "size" && args.empty()) {
        if (obj.type == VariantValue::VECTOR_INT) return VariantValue((int)std::get<std::vector<int>>(obj.value).size());
        if (obj.type == VariantValue::VECTOR_FLOAT) return VariantValue((int)std::get<std::vector<float>>(obj.value).size());
        if (obj.type == VariantValue::VECTOR_DOUBLE) return VariantValue((int)std::get<std::vector<double>>(obj.value).size());
        if (obj.type == VariantValue::VECTOR_STRING) return VariantValue((int)std::get<std::vector<std::string>>(obj.value).size());
        if (obj.type == VariantValue::VECTOR_OBJECT) return VariantValue((int)std::get<std::vector<std::shared_ptr<RuntimeObject>>>(obj.value).size());
        if (obj.type == VariantValue::VECTOR_BOOL) return VariantValue((int)std::get<std::vector<bool>>(obj.value).size());
        if (obj.type == VariantValue::VECTOR_CHAR) return VariantValue((int)std::get<std::vector<char>>(obj.value).size());
        if (obj.type == VariantValue::LIST_INT) return VariantValue((int)std::get<std::list<int>>(obj.value).size());
        if (obj.type == VariantValue::LIST_STRING) return VariantValue((int)std::get<std::list<std::string>>(obj.value).size());
        if (obj.type == VariantValue::STRING) return VariantValue((int)std::get<std::string>(obj.value).size());
        return VariantValue((int)0);
    }
    if (method == "empty" && args.empty()) {
        if (obj.type == VariantValue::VECTOR_INT) return VariantValue(std::get<std::vector<int>>(obj.value).empty());
        if (obj.type == VariantValue::VECTOR_FLOAT) return VariantValue(std::get<std::vector<float>>(obj.value).empty());
        if (obj.type == VariantValue::VECTOR_DOUBLE) return VariantValue(std::get<std::vector<double>>(obj.value).empty());
        if (obj.type == VariantValue::VECTOR_STRING) return VariantValue(std::get<std::vector<std::string>>(obj.value).empty());
        if (obj.type == VariantValue::VECTOR_OBJECT) return VariantValue(std::get<std::vector<std::shared_ptr<RuntimeObject>>>(obj.value).empty());
        if (obj.type == VariantValue::VECTOR_BOOL) return VariantValue(std::get<std::vector<bool>>(obj.value).empty());
        if (obj.type == VariantValue::VECTOR_CHAR) return VariantValue(std::get<std::vector<char>>(obj.value).empty());
        if (obj.type == VariantValue::LIST_INT) return VariantValue(std::get<std::list<int>>(obj.value).empty());
        if (obj.type == VariantValue::LIST_STRING) return VariantValue(std::get<std::list<std::string>>(obj.value).empty());
        if (obj.type == VariantValue::STRING) return VariantValue(std::get<std::string>(obj.value).empty());
        return VariantValue(false);
    }
    if (method == "begin" && args.empty()) return VariantValue(0);
    if (method == "end" && args.empty()) return VariantValue(0);
    if (method == "is_open" && args.empty()) return VariantValue(false);
    if (method == "close" && args.empty()) return VariantValue(0);
    if (method == "top" && args.empty()) {
        if (obj.type == VariantValue::VECTOR_STRING) {
            auto& v = std::get<std::vector<std::string>>(obj.value);
            if (!v.empty()) return VariantValue(v.back());
        } else if (obj.type == VariantValue::VECTOR_INT) {
            auto& v = std::get<std::vector<int>>(obj.value);
            if (!v.empty()) return VariantValue(v.back());
        }
        return VariantValue(std::string(""));
    }
    if (method == "front" && args.empty()) {
        if (obj.type == VariantValue::VECTOR_STRING) {
            auto& v = std::get<std::vector<std::string>>(obj.value);
            if (!v.empty()) return VariantValue(v.front());
        } else if (obj.type == VariantValue::VECTOR_INT) {
            auto& v = std::get<std::vector<int>>(obj.value);
            if (!v.empty()) return VariantValue(v.front());
        }
        return VariantValue(std::string(""));
    }
    if (method == "c_str" && args.empty()) {
        if (obj.type == VariantValue::STRING) return obj;
        return VariantValue(obj.toString());
    }
    if (method == "push" && args.size() == 1) {
        if (obj.type == VariantValue::VECTOR_STRING) {
            auto& v = const_cast<std::vector<std::string>&>(std::get<std::vector<std::string>>(obj.value));
            v.push_back(args[0].toString());
        } else if (obj.type == VariantValue::VECTOR_INT) {
            auto& v = const_cast<std::vector<int>&>(std::get<std::vector<int>>(obj.value));
            if (args[0].type == VariantValue::INT) v.push_back(std::get<int>(args[0].value));
        }
        return VariantValue(0);
    }
    if (method == "pop" && args.empty()) {
        if (obj.type == VariantValue::VECTOR_STRING) {
            auto& v = const_cast<std::vector<std::string>&>(std::get<std::vector<std::string>>(obj.value));
            if (!v.empty()) v.pop_back();
        } else if (obj.type == VariantValue::VECTOR_INT) {
            auto& v = const_cast<std::vector<int>&>(std::get<std::vector<int>>(obj.value));
            if (!v.empty()) v.pop_back();
        }
        return VariantValue(0);
    }

    if (obj.type != VariantValue::OBJECT && obj.type != VariantValue::POINTER) {
        error_ = "不能对非对象类型 (" + obj.typeName() + ") 调用方法 '" + method + "'";
        return VariantValue(0);
    }

    auto* rtObjPtr = std::get_if<std::shared_ptr<RuntimeObject>>(&obj.value);
    if (!rtObjPtr || !*rtObjPtr || !(*rtObjPtr)->classDef) {
        error_ = "空对象";
        return VariantValue(0);
    }
    auto& rtObj = *rtObjPtr;

    bool isClassProxy = rtObj->className == rtObj->classDef->name && rtObj->members.empty() &&
                        (rtObj->classDef->methods.empty() && rtObj->classDef->vtable.empty());

    auto vtIt = rtObj->classDef->vtable.find(method);
    auto mtIt = rtObj->classDef->methods.find(method);

    bool isVirtual = (vtIt != rtObj->classDef->vtable.end());
    if (isVirtual && vtIt->second->isPureVirtual) {
        error_ = "不能调用纯虚函数 '" + method + "'";
        return VariantValue(0);
    }

    FunctionDef* targetMethod = nullptr;

    if (mtIt != rtObj->classDef->methods.end())
        targetMethod = mtIt->second.get();
    else if (isVirtual && vtIt->second && !vtIt->second->isPureVirtual)
        targetMethod = vtIt->second.get();

    if (!targetMethod) {
        for (auto& cd : prog_->classDefs) {
            if (cd->name == rtObj->className) {
                for (auto& m : cd->methods) {
                    if (m->name == method) {
                        targetMethod = m.get();
                        break;
                    }
                }
                break;
            }
        }
    }

    FunctionDef* bodySource = targetMethod;
    if (targetMethod && targetMethod->body.empty()) {
        std::string baseName = rtObj->className;
        auto ltPos = baseName.find('<');
        if (ltPos != std::string::npos) baseName = baseName.substr(0, ltPos);
        for (auto& tc : prog_->templateClasses) {
            if (tc->classDef->name == baseName) {
                for (auto& m : tc->classDef->methods) {
                    if (m->name == method) {
                        bodySource = m.get();
                        break;
                    }
                }
                break;
            }
        }
    }

    if (targetMethod && bodySource) {
        auto savedVars = vars_;
        auto savedThis = currentThis_;
        auto savedFinished = finished_;
        if (!isClassProxy) currentThis_ = rtObj;

        for (size_t i = 0; i < targetMethod->params.size() && i < args.size(); ++i)
            vars_[targetMethod->params[i].second] = args[i];

        VariantValue ret(0);
        for (auto& stmt : bodySource->body) {
            if (auto* retStmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
                if (retStmt->expr) ret = evalExpr(retStmt->expr.get());
                break;
            }
            int line = execStmt(stmt.get());
            if (!error_.empty()) break;
            if (finished_) { finished_ = false; break; }
        }

        finished_ = savedFinished;
        currentThis_ = savedThis;
        vars_ = savedVars;
        return ret;
    }

    error_ = "类 '" + rtObj->className + "' 没有方法 '" + method + "'";
    return VariantValue(0);
}

void Executor::execFunctionBody(FunctionDef* func) {
    if (!func) return;
    for (auto& stmt : func->body) {
        execStmt(stmt.get());
        if (!error_.empty()) return;
    }
}

// ============================================================
// Expression Evaluation
// ============================================================

VariantValue Executor::evalExpr(Expr* expr) {
    if (auto* lit = dynamic_cast<LiteralExpr*>(expr)) {
        return std::visit([](auto&& v) -> VariantValue { return v; }, lit->value);
    }
    if (auto* var = dynamic_cast<VarExpr*>(expr)) {
        auto* vp = findVariable(var->name);
        if (vp) return *vp;
        bool isClass = classes_.count(var->name) > 0;
        if (!isClass) {
            for (auto& cd : prog_->classDefs) {
                if (cd->name == var->name) { isClass = true; break; }
            }
        }
        if (!isClass) {
            for (auto& tc : prog_->templateClasses) {
                if (tc->classDef->name == var->name) { isClass = true; break; }
            }
        }
        if (isClass) {
            VariantValue proxy;
            proxy.type = VariantValue::OBJECT;
            auto obj = std::make_shared<RuntimeObject>();
            obj->className = var->name;
            obj->classDef = std::make_shared<RuntimeClass>();
            obj->classDef->name = var->name;
            proxy.value = obj;
            return proxy;
        }
        error_ = "第" + std::to_string(expr->lineNumber) + "行: 未定义的变量 '" + var->name + "'";
        return VariantValue(0);
    }
    if (auto* ma = dynamic_cast<MemberAccessExpr*>(expr)) {
        VariantValue obj = evalExpr(ma->object.get());
        if (!error_.empty()) return VariantValue(0);
        return accessMember(obj, ma->member, ma->isArrow);
    }
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        if (auto* ma = dynamic_cast<MemberAccessExpr*>(call->callee.get())) {
            VariantValue obj = evalExpr(ma->object.get());
            if (!error_.empty()) return VariantValue(0);
            std::vector<VariantValue> args;
            for (auto& a : call->args) args.push_back(evalExpr(a.get()));
            VariantValue result = callMethod(obj, ma->member, args);
            if (auto* varExpr = dynamic_cast<VarExpr*>(ma->object.get())) {
                vars_[varExpr->name] = obj;
            }
            return result;
        }
        if (auto* var = dynamic_cast<VarExpr*>(call->callee.get())) {
            std::vector<VariantValue> args;
            for (auto& a : call->args) {
                args.push_back(evalExpr(a.get()));
                if (!error_.empty()) return VariantValue(0);
            }
            auto clsIt = classes_.find(var->name);
            if (clsIt != classes_.end()) {
                auto obj = createObject(var->name, args);
                if (!obj) return VariantValue(0);
                VariantValue result;
                result.type = VariantValue::OBJECT;
                result.value = obj;
                return result;
            }
            for (auto& tc : prog_->templateClasses) {
                if (tc->classDef->name == var->name) {
                    auto obj = createObject(var->name, args);
                    if (!obj) return VariantValue(0);
                    VariantValue result;
                    result.type = VariantValue::OBJECT;
                    result.value = obj;
                    return result;
                }
            }
            if (var->name == "ofstream" || var->name == "ifstream" || var->name == "fstream") {
                auto obj = std::make_shared<RuntimeObject>();
                obj->className = var->name;
                VariantValue result;
                result.type = VariantValue::OBJECT;
                result.value = obj;
                return result;
            }
            if (var->name == "sort" || var->name == "find_if" ||
                var->name == "find" || var->name == "count" ||
                var->name == "copy" || var->name == "transform" ||
                var->name == "getline") {
                return VariantValue(0);
            }
            if (var->name == "to_string" && args.size() == 1) {
                return VariantValue(args[0].toString());
            }
            if (var->name == "stoi" && args.size() == 1) {
                try { return VariantValue(std::stoi(args[0].toString())); }
                catch (...) { error_ = "stoi 转换失败"; return VariantValue(0); }
            }
            if (var->name == "stod" && args.size() == 1) {
                try { return VariantValue(std::stod(args[0].toString())); }
                catch (...) { error_ = "stod 转换失败"; return VariantValue(0); }
            }
            for (auto& tf : prog_->templateFuncs) {
                if (tf->func->name == var->name) {
                    std::map<std::string, std::string> deduced;
                    for (size_t i = 0; i < tf->func->params.size() && i < args.size(); ++i) {
                        std::string pType = tf->func->params[i].first;
                        auto vPos = pType.find("vector<");
                        if (vPos != std::string::npos) {
                            auto endPos = pType.find('>', vPos);
                            if (endPos != std::string::npos) {
                                std::string inner = pType.substr(vPos + 7, endPos - vPos - 7);
                                if (args[i].type == VariantValue::VECTOR_INT) deduced[inner] = "int";
                                else if (args[i].type == VariantValue::VECTOR_FLOAT) deduced[inner] = "float";
                                else if (args[i].type == VariantValue::VECTOR_DOUBLE) deduced[inner] = "double";
                                else if (args[i].type == VariantValue::VECTOR_STRING) deduced[inner] = "string";
                                else if (args[i].type == VariantValue::VECTOR_OBJECT) deduced[inner] = "object";
                            }
                        }
                    }
                    auto savedAliases = typeAliases_;
                    typeAliases_ = deduced;
                    auto savedVars = vars_;
                    auto savedThis = currentThis_;
                    auto savedFinished = finished_;
                    for (size_t i = 0; i < tf->func->params.size() && i < args.size(); ++i)
                        vars_[tf->func->params[i].second] = args[i];
                    VariantValue ret(0);
                    for (auto& stmt : tf->func->body) {
                        if (auto* retStmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
                            if (retStmt->expr) ret = evalExpr(retStmt->expr.get());
                            break;
                        }
                        int line = execStmt(stmt.get());
                        if (!error_.empty()) break;
                        if (finished_) { finished_ = false; break; }
                    }
                    finished_ = savedFinished;
                    currentThis_ = savedThis;
                    vars_ = savedVars;
                    typeAliases_ = savedAliases;
                    return ret;
                }
            }
            for (auto& func : prog_->functions) {
                if (func->name == var->name) {
                    auto savedVars = vars_;
                    auto savedThis = currentThis_;
                    auto savedFinished = finished_;
                    for (size_t i = 0; i < func->params.size() && i < args.size(); ++i)
                        vars_[func->params[i].second] = args[i];
                    VariantValue ret(0);
                    for (auto& stmt : func->body) {
                        if (auto* retStmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
                            if (retStmt->expr) ret = evalExpr(retStmt->expr.get());
                            break;
                        }
                        int line = execStmt(stmt.get());
                        if (!error_.empty()) break;
                        if (finished_) { finished_ = false; break; }
                    }
                    finished_ = savedFinished;
                    currentThis_ = savedThis;
                    vars_ = savedVars;
                    return ret;
                }
            }
            error_ = "未定义的函数 '" + var->name + "'";
            return VariantValue(0);
        }
    }
    if (auto* idx = dynamic_cast<IndexExpr*>(expr)) {
        VariantValue obj = evalExpr(idx->object.get());
        if (!error_.empty()) return VariantValue(0);
        VariantValue idxVal = evalExpr(idx->index.get());
        if (!error_.empty()) return VariantValue(0);

        // Handle map indexing (key can be string, not int)
        if (obj.type == VariantValue::MAP_STRING_INT) {
            auto& m = std::get<std::map<std::string, int>>(obj.value);
            if (idxVal.type == VariantValue::STRING) {
                std::string key = std::get<std::string>(idxVal.value);
                auto it = m.find(key);
                if (it != m.end()) return VariantValue(it->second);
                error_ = "map 中不存在键 '" + key + "'";
                return VariantValue(0);
            }
            error_ = "map 下标必须为 string";
            return VariantValue(0);
        }

        int i = 0;
        if (idxVal.type == VariantValue::INT) i = std::get<int>(idxVal.value);
        else { error_ = "下标必须为整数"; return VariantValue(0); }

        switch (obj.type) {
        case VariantValue::VECTOR_INT: {
            auto& v = std::get<std::vector<int>>(obj.value);
            if (i < 0 || i >= (int)v.size()) { error_ = "vector 下标越界"; return VariantValue(0); }
            return VariantValue(v[i]);
        }
        case VariantValue::VECTOR_FLOAT: {
            auto& v = std::get<std::vector<float>>(obj.value);
            if (i < 0 || i >= (int)v.size()) { error_ = "vector 下标越界"; return VariantValue(0); }
            return VariantValue(v[i]);
        }
        case VariantValue::VECTOR_DOUBLE: {
            auto& v = std::get<std::vector<double>>(obj.value);
            if (i < 0 || i >= (int)v.size()) { error_ = "vector 下标越界"; return VariantValue(0); }
            return VariantValue(v[i]);
        }
        case VariantValue::VECTOR_STRING: {
            auto& v = std::get<std::vector<std::string>>(obj.value);
            if (i < 0 || i >= (int)v.size()) { error_ = "vector 下标越界"; return VariantValue(0); }
            return VariantValue(v[i]);
        }
        case VariantValue::STRING: {
            auto& s = std::get<std::string>(obj.value);
            if (i < 0 || i >= (int)s.size()) { error_ = "字符串下标越界"; return VariantValue(0); }
            return VariantValue(s[i]);
        }
        default:
            error_ = "该类型不支持下标访问";
            return VariantValue(0);
        }
    }
    if (auto* vecLit = dynamic_cast<VectorLiteralExpr*>(expr)) {
        if (vecLit->elements.empty()) {
            return VariantValue(std::vector<int>{});
        }
        VariantValue first = evalExpr(vecLit->elements[0].get());
        if (!error_.empty()) return VariantValue(0);

        if (first.type == VariantValue::INT) {
            std::vector<int> elems;
            for (auto& e : vecLit->elements) {
                VariantValue v = evalExpr(e.get());
                if (v.type == VariantValue::INT) elems.push_back(std::get<int>(v.value));
                else if (v.type == VariantValue::FLOAT) elems.push_back(static_cast<int>(std::get<float>(v.value)));
                else if (v.type == VariantValue::DOUBLE) elems.push_back(static_cast<int>(std::get<double>(v.value)));
            }
            return VariantValue(elems);
        } else if (first.type == VariantValue::FLOAT) {
            std::vector<float> elems;
            for (auto& e : vecLit->elements) {
                VariantValue v = evalExpr(e.get());
                if (v.type == VariantValue::FLOAT) elems.push_back(std::get<float>(v.value));
                else if (v.type == VariantValue::INT) elems.push_back(static_cast<float>(std::get<int>(v.value)));
            }
            return VariantValue(elems);
        } else if (first.type == VariantValue::DOUBLE) {
            std::vector<double> elems;
            for (auto& e : vecLit->elements) {
                VariantValue v = evalExpr(e.get());
                if (v.type == VariantValue::DOUBLE) elems.push_back(std::get<double>(v.value));
                else if (v.type == VariantValue::INT) elems.push_back(static_cast<double>(std::get<int>(v.value)));
                else if (v.type == VariantValue::FLOAT) elems.push_back(static_cast<double>(std::get<float>(v.value)));
            }
            return VariantValue(elems);
        } else if (first.type == VariantValue::STRING) {
            std::vector<std::string> elems;
            for (auto& e : vecLit->elements) {
                VariantValue v = evalExpr(e.get());
                if (v.type == VariantValue::STRING) elems.push_back(std::get<std::string>(v.value));
                else elems.push_back(v.toString());
            }
            return VariantValue(elems);
        } else if (first.type == VariantValue::CHAR) {
            std::vector<char> elems;
            for (auto& e : vecLit->elements) {
                VariantValue v = evalExpr(e.get());
                elems.push_back(std::get<char>(v.value));
            }
            return VariantValue(elems);
        } else if (first.type == VariantValue::BOOL) {
            std::vector<bool> elems;
            for (auto& e : vecLit->elements) {
                VariantValue v = evalExpr(e.get());
                elems.push_back(std::get<bool>(v.value));
            }
            return VariantValue(elems);
        }
    }
    if (auto* ne = dynamic_cast<NewExpr*>(expr)) {
        if (ne->isArray) {
            if (ne->arraySize) {
                VariantValue sizeVal = evalExpr(ne->arraySize.get());
                int count = 5;
                if (sizeVal.type == VariantValue::INT) count = std::get<int>(sizeVal.value);
                std::string innerType = ne->typeName;

                std::vector<VariantValue> initVals;
                if (ne->braceInit) {
                    auto* vl = dynamic_cast<VectorLiteralExpr*>(ne->braceInit.get());
                    if (vl) {
                        for (auto& e : vl->elements)
                            initVals.push_back(evalExpr(e.get()));
                    }
                }

                if (innerType == "int") {
                    std::vector<int> arr;
                    for (int i = 0; i < count; i++) {
                        if (i < (int)initVals.size() && initVals[i].type == VariantValue::INT)
                            arr.push_back(std::get<int>(initVals[i].value));
                        else
                            arr.push_back(0);
                    }
                    return VariantValue(arr);
                }
                if (innerType == "float") {
                    std::vector<float> arr;
                    for (int i = 0; i < count; i++) {
                        if (i < (int)initVals.size() && initVals[i].type == VariantValue::INT)
                            arr.push_back((float)std::get<int>(initVals[i].value));
                        else
                            arr.push_back(0.0f);
                    }
                    return VariantValue(arr);
                }
                if (innerType == "double") {
                    std::vector<double> arr;
                    for (int i = 0; i < count; i++) {
                        if (i < (int)initVals.size() && initVals[i].type == VariantValue::INT)
                            arr.push_back((double)std::get<int>(initVals[i].value));
                        else
                            arr.push_back(0.0);
                    }
                    return VariantValue(arr);
                }
                if (innerType == "char") {
                    std::vector<char> arr;
                    for (int i = 0; i < count; i++) {
                        if (i < (int)initVals.size() && initVals[i].type == VariantValue::INT)
                            arr.push_back((char)std::get<int>(initVals[i].value));
                        else
                            arr.push_back('\0');
                    }
                    return VariantValue(arr);
                }
                if (innerType == "string") {
                    std::vector<std::string> arr;
                    for (int i = 0; i < count; i++) {
                        if (i < (int)initVals.size() && initVals[i].type == VariantValue::STRING)
                            arr.push_back(std::get<std::string>(initVals[i].value));
                        else
                            arr.push_back("");
                    }
                    return VariantValue(arr);
                }
                std::vector<int> arr(count, 0);
                return VariantValue(arr);
            }
            error_ = "new Type[] 暂不支持";
            return VariantValue(0);
        }
        std::vector<VariantValue> ctorArgs;
        for (auto& a : ne->constructorArgs)
            ctorArgs.push_back(evalExpr(a.get()));
        auto obj = createObject(ne->typeName, ctorArgs);
        if (!obj) return VariantValue(0);
        VariantValue result;
        result.type = VariantValue::POINTER;
        result.value = obj;
        return result;
    }
    if (dynamic_cast<DeleteExpr*>(expr)) {
        return VariantValue(0);
    }
    if (dynamic_cast<LambdaExpr*>(expr)) {
        return VariantValue(0);
    }
    if (dynamic_cast<ThisExpr*>(expr)) {
        if (!currentThis_) {
            error_ = "this 只能在成员函数内部使用";
            return VariantValue(0);
        }
        VariantValue result;
        result.type = VariantValue::POINTER;
        result.value = currentThis_;
        return result;
    }
    if (dynamic_cast<NullptrExpr*>(expr)) {
        return VariantValue(nullptr);
    }
    if (auto* cast = dynamic_cast<CastExpr*>(expr)) {
        VariantValue val = evalExpr(cast->expr.get());
        if (!error_.empty()) return VariantValue(0);
        std::string cleanType = cast->targetType;
        while (!cleanType.empty() && (cleanType.back() == '&' || cleanType.back() == '*'))
            cleanType.pop_back();
        if (cast->castKind == "const") return val;
        return castToType(val, cleanType);
    }
    if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) return evalBinary(bin);
    if (auto* un  = dynamic_cast<UnaryExpr*>(expr))  return evalUnary(un);
    error_ = "第" + std::to_string(expr->lineNumber) + "行: 未知表达式";
    return VariantValue(0);
}

// ============================================================
// Binary Expression Evaluation
// ============================================================

VariantValue Executor::evalBinary(BinaryExpr* bin) {
    if (bin->op == BinOp::AND) {
        VariantValue l = evalExpr(bin->left.get());
        if (!error_.empty()) return VariantValue(0);
        bool lb = false;
        if (l.type == VariantValue::BOOL) lb = std::get<bool>(l.value);
        else if (l.type == VariantValue::INT) lb = (std::get<int>(l.value) != 0);
        if (!lb) return VariantValue(false);
        VariantValue r = evalExpr(bin->right.get());
        if (!error_.empty()) return VariantValue(0);
        if (r.type == VariantValue::BOOL) return r;
        if (r.type == VariantValue::INT) return VariantValue(std::get<int>(r.value) != 0);
        return VariantValue(false);
    }
    if (bin->op == BinOp::OR) {
        VariantValue l = evalExpr(bin->left.get());
        if (!error_.empty()) return VariantValue(0);
        bool lb = false;
        if (l.type == VariantValue::BOOL) lb = std::get<bool>(l.value);
        else if (l.type == VariantValue::INT) lb = (std::get<int>(l.value) != 0);
        if (lb) return VariantValue(true);
        VariantValue r = evalExpr(bin->right.get());
        if (!error_.empty()) return VariantValue(0);
        if (r.type == VariantValue::BOOL) return r;
        if (r.type == VariantValue::INT) return VariantValue(std::get<int>(r.value) != 0);
        return VariantValue(false);
    }

    VariantValue l = evalExpr(bin->left.get());
    VariantValue r = evalExpr(bin->right.get());
    if (!error_.empty()) return VariantValue(0);

    if (l.type == VariantValue::OBJECT || l.type == VariantValue::POINTER ||
        r.type == VariantValue::OBJECT || r.type == VariantValue::POINTER) {
        VariantValue result;
        if (tryOperatorOverload(bin->op, l, r, result)) return result;
    }

    bool useFloat = (l.type == VariantValue::FLOAT || r.type == VariantValue::FLOAT);
    bool useDouble = (l.type == VariantValue::DOUBLE || r.type == VariantValue::DOUBLE);
    bool isStringConcat = (bin->op == BinOp::ADD &&
                          (l.type == VariantValue::STRING || r.type == VariantValue::STRING));

    if (isStringConcat) {
        std::string ls, rs;
        if (l.type == VariantValue::STRING) ls = std::get<std::string>(l.value);
        else ls = l.toString();
        if (r.type == VariantValue::STRING) rs = std::get<std::string>(r.value);
        else rs = r.toString();
        return VariantValue(ls + rs);
    }

    auto getInt = [](const VariantValue& v) -> int {
        if (v.type == VariantValue::FLOAT) return static_cast<int>(std::get<float>(v.value));
        if (v.type == VariantValue::DOUBLE) return static_cast<int>(std::get<double>(v.value));
        if (v.type == VariantValue::CHAR) return static_cast<int>(std::get<char>(v.value));
        if (v.type == VariantValue::INT) return std::get<int>(v.value);
        if (v.type == VariantValue::BOOL) return std::get<bool>(v.value) ? 1 : 0;
        return 0;
    };
    auto getFloat = [](const VariantValue& v) -> float {
        if (v.type == VariantValue::INT) return static_cast<float>(std::get<int>(v.value));
        if (v.type == VariantValue::DOUBLE) return static_cast<float>(std::get<double>(v.value));
        if (v.type == VariantValue::FLOAT) return std::get<float>(v.value);
        if (v.type == VariantValue::CHAR) return static_cast<float>(std::get<char>(v.value));
        return 0.0f;
    };
    auto getDouble = [](const VariantValue& v) -> double {
        if (v.type == VariantValue::INT) return static_cast<double>(std::get<int>(v.value));
        if (v.type == VariantValue::FLOAT) return static_cast<double>(std::get<float>(v.value));
        if (v.type == VariantValue::DOUBLE) return std::get<double>(v.value);
        if (v.type == VariantValue::CHAR) return static_cast<double>(std::get<char>(v.value));
        return 0.0;
    };

    auto combinedDouble = [&](auto intOp, auto floatOp) -> VariantValue {
        if (useDouble) { double a = getDouble(l), b = getDouble(r); return VariantValue(floatOp(a, b)); }
        if (useFloat) { float a = getFloat(l), b = getFloat(r); return VariantValue(floatOp(a, b)); }
        int a = getInt(l), b = getInt(r);
        return VariantValue(intOp(a, b));
    };
    auto combinedCompare = [&](auto intOp, auto floatOp) -> VariantValue {
        if (useDouble) { double a = getDouble(l), b = getDouble(r); return VariantValue(static_cast<bool>(floatOp(a, b))); }
        if (useFloat) { float a = getFloat(l), b = getFloat(r); return VariantValue(static_cast<bool>(floatOp(a, b))); }
        int a = getInt(l), b = getInt(r);
        return VariantValue(static_cast<bool>(intOp(a, b)));
    };

    switch (bin->op) {
        case BinOp::ADD: return combinedDouble(std::plus<>(), std::plus<>());
        case BinOp::SUB: return combinedDouble(std::minus<>(), std::minus<>());
        case BinOp::MUL: return combinedDouble(std::multiplies<>(), std::multiplies<>());
        case BinOp::DIV: return combinedDouble(std::divides<>(), std::divides<>());
        case BinOp::MOD: {
            int a = getInt(l), b = getInt(r);
            if (b == 0) { error_ = "模零错误"; return VariantValue(0); }
            return VariantValue(a % b);
        }
        case BinOp::EQ: return combinedCompare(std::equal_to<>(), std::equal_to<>());
        case BinOp::NE: return combinedCompare(std::not_equal_to<>(), std::not_equal_to<>());
        case BinOp::LT: return combinedCompare(std::less<>(), std::less<>());
        case BinOp::GT: return combinedCompare(std::greater<>(), std::greater<>());
        case BinOp::LE: return combinedCompare(std::less_equal<>(), std::less_equal<>());
        case BinOp::GE: return combinedCompare(std::greater_equal<>(), std::greater_equal<>());
        case BinOp::SHL: case BinOp::SHR: {
            int a = getInt(l), b = getInt(r);
            if (bin->op == BinOp::SHL) return VariantValue(a << b);
            else return VariantValue(a >> b);
        }
        default: break;
    }
    error_ = "未知运算符";
    return VariantValue(0);
}

VariantValue Executor::evalUnary(UnaryExpr* un) {
    VariantValue operand = evalExpr(un->operand.get());
    if (!error_.empty()) return VariantValue(0);

    switch (un->op) {
    case UnaryOp::NEG:
        if (operand.type == VariantValue::INT)   return VariantValue(-std::get<int>(operand.value));
        if (operand.type == VariantValue::FLOAT) return VariantValue(-std::get<float>(operand.value));
        if (operand.type == VariantValue::DOUBLE) return VariantValue(-std::get<double>(operand.value));
        error_ = "不能对非数值类型取负";
        return VariantValue(0);
    case UnaryOp::NOT: {
        bool b = false;
        if (operand.type == VariantValue::BOOL) b = std::get<bool>(operand.value);
        else if (operand.type == VariantValue::INT) b = (std::get<int>(operand.value) != 0);
        else if (operand.type == VariantValue::CHAR) b = (std::get<char>(operand.value) != 0);
        else b = false;
        return VariantValue(!b);
    }
    case UnaryOp::DEREF:
        if (operand.type == VariantValue::POINTER) {
            auto* objPtr = std::get_if<std::shared_ptr<RuntimeObject>>(&operand.value);
            if (!objPtr || !*objPtr) { error_ = "对空指针解引用"; return VariantValue(0); }
            auto obj = *objPtr;
            VariantValue result;
            result.type = VariantValue::OBJECT;
            result.value = obj;
            return result;
        }
        error_ = "只能对指针类型解引用";
        return VariantValue(0);
    case UnaryOp::ADDR:
        if (dynamic_cast<VarExpr*>(un->operand.get())) {
            VariantValue ptr;
            ptr.type = VariantValue::POINTER;
            return operand;
        }
        error_ = "只能对变量取地址";
        return VariantValue(0);
    case UnaryOp::PRE_INC:
    case UnaryOp::POST_INC:
        if (auto* var = dynamic_cast<VarExpr*>(un->operand.get())) {
            VariantValue* vp = findVariable(var->name);
            if (!vp) { error_ = "未定义的变量 '" + var->name + "'"; return VariantValue(0); }
            VariantValue oldVal = *vp;
            if (vp->type == VariantValue::INT) *vp = VariantValue(std::get<int>(vp->value) + 1);
            else if (vp->type == VariantValue::FLOAT) *vp = VariantValue(std::get<float>(vp->value) + 1.0f);
            else if (vp->type == VariantValue::DOUBLE) *vp = VariantValue(std::get<double>(vp->value) + 1.0);
            else { error_ = "不能对该类型使用 ++"; return VariantValue(0); }
            updateMemoryForVar(var->name, *vp);
            return (un->op == UnaryOp::PRE_INC) ? *vp : oldVal;
        }
        error_ = "++ 只能用于变量";
        return VariantValue(0);
    case UnaryOp::PRE_DEC:
    case UnaryOp::POST_DEC:
        if (auto* var = dynamic_cast<VarExpr*>(un->operand.get())) {
            VariantValue* vp = findVariable(var->name);
            if (!vp) { error_ = "未定义的变量 '" + var->name + "'"; return VariantValue(0); }
            VariantValue oldVal = *vp;
            if (vp->type == VariantValue::INT) *vp = VariantValue(std::get<int>(vp->value) - 1);
            else if (vp->type == VariantValue::FLOAT) *vp = VariantValue(std::get<float>(vp->value) - 1.0f);
            else if (vp->type == VariantValue::DOUBLE) *vp = VariantValue(std::get<double>(vp->value) - 1.0);
            else { error_ = "不能对该类型使用 --"; return VariantValue(0); }
            updateMemoryForVar(var->name, *vp);
            return (un->op == UnaryOp::PRE_DEC) ? *vp : oldVal;
        }
        error_ = "-- 只能用于变量";
        return VariantValue(0);
    }
    return VariantValue(0);
}

bool Executor::tryOperatorOverload(BinOp op, const VariantValue& lhs, const VariantValue& rhs, VariantValue& result) {
    std::string opName;
    switch (op) {
        case BinOp::ADD: opName = "+"; break;
        case BinOp::SUB: opName = "-"; break;
        case BinOp::MUL: opName = "*"; break;
        case BinOp::DIV: opName = "/"; break;
        case BinOp::EQ: opName = "=="; break;
        case BinOp::NE: opName = "!="; break;
        case BinOp::LT: opName = "<"; break;
        case BinOp::GT: opName = ">"; break;
        case BinOp::LE: opName = "<="; break;
        case BinOp::GE: opName = ">="; break;
        case BinOp::SHL: opName = "<<"; break;
        case BinOp::SHR: opName = ">>"; break;
        default: return false;
    }
    return callOperator(opName, lhs, rhs).type != VariantValue::VOID;
}

VariantValue Executor::callOperator(const std::string& op, const VariantValue& lhs, const VariantValue& rhs) {
    return callMethod(lhs, "operator" + op, {rhs});
}

// ============================================================
// Statement Execution
// ============================================================

int Executor::execStmt(Stmt* stmt) {
    if (auto* bl = dynamic_cast<BlockStmt*>(stmt))   return execBlock(bl);
    if (auto* d  = dynamic_cast<DeclStmt*>(stmt))    return execDecl(d);
    if (auto* a  = dynamic_cast<AssignStmt*>(stmt))  return execAssign(a);
    if (auto* i  = dynamic_cast<IfStmt*>(stmt))      return execIf(i);
    if (auto* w  = dynamic_cast<WhileStmt*>(stmt))   return execWhile(w);
    if (auto* dw = dynamic_cast<DoWhileStmt*>(stmt)) return execDoWhile(dw);
    if (auto* f  = dynamic_cast<ForStmt*>(stmt))     return execFor(f);
    if (auto* rf = dynamic_cast<RangeForStmt*>(stmt)) return execRangeFor(rf);
    if (auto* b  = dynamic_cast<BreakStmt*>(stmt))   return execBreak(b);
    if (auto* c  = dynamic_cast<ContinueStmt*>(stmt)) return execContinue(c);
    if (auto* ci = dynamic_cast<CinStmt*>(stmt))     return execCin(ci);
    if (auto* co = dynamic_cast<CoutStmt*>(stmt))    return execCout(co);
    if (auto* r  = dynamic_cast<ReturnStmt*>(stmt))  return execReturn(r);
    if (auto* tc = dynamic_cast<TryCatchStmt*>(stmt)) return execTryCatch(tc);
    if (auto* th = dynamic_cast<ThrowStmt*>(stmt))   return execThrow(th);
    if (auto* es = dynamic_cast<ExprStmt*>(stmt))    return execExprStmt(es);
    return stmt->lineNumber;
}

int Executor::execBlock(BlockStmt* stmt) {
    for (auto& s : stmt->stmts) {
        int line = execStmt(s.get());
        if (!error_.empty()) return line;
    }
    return stmt->lineNumber;
}

int Executor::execDecl(DeclStmt* stmt) {
    auto stripQualifiers = [](std::string t) -> std::string {
        if (t.find("const ") == 0) t = t.substr(6);
        while (!t.empty() && (t.back() == '&' || t.back() == '*')) t.pop_back();
        return t;
    };

    for (size_t i = 0; i < stmt->names.size(); i++) {
        VariantValue val;
        if (i < stmt->inits.size() && stmt->inits[i]) {
            val = evalExpr(stmt->inits[i].get());
            if (!error_.empty()) return stmt->lineNumber;
        } else {
            std::string t = stripQualifiers(stmt->typeName);
            auto aliasIt = typeAliases_.find(t);
            if (aliasIt != typeAliases_.end()) t = aliasIt->second;
            if (t == "int") val = VariantValue(0);
            else if (t == "float") val = VariantValue(0.0f);
            else if (t == "double") val = VariantValue(0.0);
            else if (t == "char") val = VariantValue('\0');
            else if (t == "bool") val = VariantValue(false);
            else if (t == "string") val = VariantValue(std::string(""));
            else if (t == "auto") { error_ = "auto 变量必须有初始化表达式"; return stmt->lineNumber; }
            else {
                if (t.find("vector<") == 0 && t.back() == '>') {
                    std::string inner = t.substr(7, t.size() - 8);
                    if (inner == "int") val = VariantValue(std::vector<int>{});
                    else if (inner == "float") val = VariantValue(std::vector<float>{});
                    else if (inner == "double") val = VariantValue(std::vector<double>{});
                    else if (inner == "string") val = VariantValue(std::vector<std::string>{});
                    else if (inner == "char") val = VariantValue(std::vector<char>{});
                    else if (inner == "bool") val = VariantValue(std::vector<bool>{});
                    else { val = VariantValue(std::vector<std::shared_ptr<RuntimeObject>>{}); val.elementClassName = inner; }
                } else if (t.find("list<") == 0 && t.back() == '>') {
                    std::string inner = t.substr(5, t.size() - 6);
                    if (inner == "int") val = VariantValue(std::list<int>{});
                    else if (inner == "string") val = VariantValue(std::list<std::string>{});
                    else val = VariantValue(std::list<int>{});
                } else if (t == "set<int>") {
                    val = VariantValue(std::set<int>{});
                } else if (t == "map<string,int>" || t == "map<string, int>") {
                    val = VariantValue(std::map<std::string, int>{});
                } else if (t.find("stack<") == 0 && t.back() == '>') {
                    std::string inner = t.substr(6, t.size() - 7);
                    if (inner == "string") val = VariantValue(std::vector<std::string>{});
                    else if (inner == "int") val = VariantValue(std::vector<int>{});
                    else val = VariantValue(std::vector<int>{});
                } else if (t.find("queue<") == 0 && t.back() == '>') {
                    std::string inner = t.substr(6, t.size() - 7);
                    if (inner == "string") val = VariantValue(std::vector<std::string>{});
                    else if (inner == "int") val = VariantValue(std::vector<int>{});
                    else val = VariantValue(std::vector<int>{});
                } else if (t == "ofstream" || t == "ifstream") {
                    val = VariantValue(0);
                } else {
                    std::string baseType = t;
                    auto ltPos = baseType.find('<');
                    if (ltPos != std::string::npos) baseType = baseType.substr(0, ltPos);
                if (classes_.count(baseType) || classes_.count(t)) {
                    auto obj = createObject(t, {});
                    if (!obj) return stmt->lineNumber;
                    val.type = VariantValue::OBJECT;
                    val.value = obj;
                    val.elementClassName = baseType;
                    } else {
                        for (auto& tc : prog_->templateClasses) {
                            if (tc->classDef->name == baseType) {
                                auto obj = createObject(t, {});
                                if (!obj) return stmt->lineNumber;
                                val.type = VariantValue::OBJECT;
                                val.value = obj;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (!error_.empty()) return stmt->lineNumber;

        std::string t = stripQualifiers(stmt->typeName);
        auto aliasIt2 = typeAliases_.find(t);
        if (aliasIt2 != typeAliases_.end()) t = aliasIt2->second;
        // Convert vector<T> to match declared element type (e.g., vector<double> from {1, 2, 3})
        if (t.find("vector<") == 0 && t.back() == '>') {
            std::string declaredInner = t.substr(7, t.size() - 8);
            auto convertVecElements = [&](auto& srcVec) -> VariantValue {
                if (declaredInner == "int") {
                    std::vector<int> dst; for (auto& x : srcVec) dst.push_back(static_cast<int>(x)); return VariantValue(dst);
                } else if (declaredInner == "float") {
                    std::vector<float> dst; for (auto& x : srcVec) dst.push_back(static_cast<float>(x)); return VariantValue(dst);
                } else if (declaredInner == "double") {
                    std::vector<double> dst; for (auto& x : srcVec) dst.push_back(static_cast<double>(x)); return VariantValue(dst);
                }
                return VariantValue(0);
            };
            std::string currentInner;
            VariantValue::Type vt = val.type;
            if (vt == VariantValue::VECTOR_INT) currentInner = "int";
            else if (vt == VariantValue::VECTOR_FLOAT) currentInner = "float";
            else if (vt == VariantValue::VECTOR_DOUBLE) currentInner = "double";
            if (!currentInner.empty() && currentInner != declaredInner) {
                if (vt == VariantValue::VECTOR_INT) val = convertVecElements(std::get<std::vector<int>>(val.value));
                else if (vt == VariantValue::VECTOR_FLOAT) val = convertVecElements(std::get<std::vector<float>>(val.value));
                else if (vt == VariantValue::VECTOR_DOUBLE) val = convertVecElements(std::get<std::vector<double>>(val.value));
            }
        }
        if (t.find("set<") == 0 && t.back() == '>' && val.type == VariantValue::VECTOR_INT) {
            auto& vec = std::get<std::vector<int>>(val.value);
            val = VariantValue(std::set<int>(vec.begin(), vec.end()));
        } else if (t.find("list<") == 0 && t.back() == '>') {
            std::string inner = t.substr(5, t.size() - 6);
            if (inner == "string" && val.type == VariantValue::VECTOR_STRING) {
                auto& vec = std::get<std::vector<std::string>>(val.value);
                val = VariantValue(std::list<std::string>(vec.begin(), vec.end()));
            } else if (inner == "int" && val.type == VariantValue::VECTOR_INT) {
                auto& vec = std::get<std::vector<int>>(val.value);
                val = VariantValue(std::list<int>(vec.begin(), vec.end()));
            }
        }

        if (stmt->typeName != "auto" && !stmt->typeName.empty() &&
            stmt->typeName.find('*') == std::string::npos) {
            std::string castType = stmt->typeName;
            auto aliasIt = typeAliases_.find(castType);
            if (aliasIt != typeAliases_.end()) castType = aliasIt->second;
            castType = stripQualifiers(castType);
            val = castToType(val, castType);
        }

        vars_[stmt->names[i]] = val;
        allocateMemoryForVar(stmt->names[i], val);
    }
    return stmt->lineNumber;
}

int Executor::execAssign(AssignStmt* stmt) {
    VariantValue val = evalExpr(stmt->expr.get());
    if (!error_.empty()) return stmt->lineNumber;

    if (stmt->memberExpr) {
        auto* ma = dynamic_cast<MemberAccessExpr*>(stmt->memberExpr.get());
        if (ma) {
            VariantValue obj = evalExpr(ma->object.get());
            if (error_.empty() && (obj.type == VariantValue::OBJECT || obj.type == VariantValue::POINTER)) {
                auto* rtObjPtr = std::get_if<std::shared_ptr<RuntimeObject>>(&obj.value);
                if (rtObjPtr && *rtObjPtr) {
                    (*rtObjPtr)->members[ma->member] = val;
                    return stmt->lineNumber;
                }
            }
            error_ = "无效的成员赋值";
            return stmt->lineNumber;
        }
    }

    if (stmt->index) {
        VariantValue* vp = findVariable(stmt->name);
        if (!vp) {
            error_ = "第" + std::to_string(stmt->lineNumber) + "行: 未定义的变量 '" + stmt->name + "'";
            return stmt->lineNumber;
        }
        VariantValue idxVal = evalExpr(stmt->index.get());
        if (!error_.empty()) return stmt->lineNumber;

        if (vp->type == VariantValue::MAP_STRING_INT) {
            if (idxVal.type != VariantValue::STRING) {
                error_ = "map key 必须为字符串";
                return stmt->lineNumber;
            }
            std::string key = std::get<std::string>(idxVal.value);
            auto& m = std::get<std::map<std::string, int>>(vp->value);
            if (val.type == VariantValue::INT) m[key] = std::get<int>(val.value);
            else if (val.type == VariantValue::FLOAT) m[key] = static_cast<int>(std::get<float>(val.value));
            else if (val.type == VariantValue::DOUBLE) m[key] = static_cast<int>(std::get<double>(val.value));
            else m[key] = 0;
            updateMemoryForVar(stmt->name, *vp);
            return stmt->lineNumber;
        }

        if (idxVal.type != VariantValue::INT) {
            error_ = "下标必须为整数";
            return stmt->lineNumber;
        }
        int i = std::get<int>(idxVal.value);

        switch (vp->type) {
        case VariantValue::VECTOR_INT: {
            auto& v = std::get<std::vector<int>>(vp->value);
            if (i < 0 || i >= (int)v.size()) { error_ = "下标越界"; return stmt->lineNumber; }
            if (val.type == VariantValue::INT) v[i] = std::get<int>(val.value);
            else if (val.type == VariantValue::FLOAT) v[i] = static_cast<int>(std::get<float>(val.value));
            else if (val.type == VariantValue::DOUBLE) v[i] = static_cast<int>(std::get<double>(val.value));
            else { error_ = "无法将类型 '" + val.typeName() + "' 赋值给 vector<int>"; return stmt->lineNumber; }
            break;
        }
        case VariantValue::VECTOR_FLOAT: {
            auto& v = std::get<std::vector<float>>(vp->value);
            if (i < 0 || i >= (int)v.size()) { error_ = "下标越界"; return stmt->lineNumber; }
            if (val.type == VariantValue::FLOAT) v[i] = std::get<float>(val.value);
            else if (val.type == VariantValue::INT) v[i] = static_cast<float>(std::get<int>(val.value));
            else if (val.type == VariantValue::DOUBLE) v[i] = static_cast<float>(std::get<double>(val.value));
            else { error_ = "无法将类型 '" + val.typeName() + "' 赋值给 vector<float>"; return stmt->lineNumber; }
            break;
        }
        case VariantValue::VECTOR_DOUBLE: {
            auto& v = std::get<std::vector<double>>(vp->value);
            if (i < 0 || i >= (int)v.size()) { error_ = "下标越界"; return stmt->lineNumber; }
            if (val.type == VariantValue::DOUBLE) v[i] = std::get<double>(val.value);
            else if (val.type == VariantValue::FLOAT) v[i] = static_cast<double>(std::get<float>(val.value));
            else if (val.type == VariantValue::INT) v[i] = static_cast<double>(std::get<int>(val.value));
            else { error_ = "无法将类型 '" + val.typeName() + "' 赋值给 vector<double>"; return stmt->lineNumber; }
            break;
        }
        case VariantValue::VECTOR_STRING: {
            auto& v = std::get<std::vector<std::string>>(vp->value);
            if (i < 0 || i >= (int)v.size()) { error_ = "下标越界"; return stmt->lineNumber; }
            if (val.type == VariantValue::STRING) v[i] = std::get<std::string>(val.value);
            else v[i] = val.toString();
            break;
        }
        default:
            error_ = "该类型不支持下标赋值";
            return stmt->lineNumber;
        }
        updateMemoryForVar(stmt->name, *vp);
        return stmt->lineNumber;
    }

    VariantValue* vp = findVariable(stmt->name);
    if (!vp) {
        error_ = "第" + std::to_string(stmt->lineNumber) + "行: 未定义的变量 '" + stmt->name + "'";
        return stmt->lineNumber;
    }

    // Helper: promote LHS + RHS values to double for compound-assign
    auto promoteToDouble = [](const VariantValue& v) -> double {
        if (v.type == VariantValue::DOUBLE) return std::get<double>(v.value);
        if (v.type == VariantValue::FLOAT)  return static_cast<double>(std::get<float>(v.value));
        if (v.type == VariantValue::INT)    return static_cast<double>(std::get<int>(v.value));
        return 0.0;
    };
    auto promoteToFloat = [](const VariantValue& v) -> float {
        if (v.type == VariantValue::FLOAT)  return std::get<float>(v.value);
        if (v.type == VariantValue::INT)    return static_cast<float>(std::get<int>(v.value));
        if (v.type == VariantValue::DOUBLE) return static_cast<float>(std::get<double>(v.value));
        return 0.0f;
    };
    auto getIntVal = [](const VariantValue& v) -> int {
        if (v.type == VariantValue::INT)    return std::get<int>(v.value);
        if (v.type == VariantValue::FLOAT)  return static_cast<int>(std::get<float>(v.value));
        if (v.type == VariantValue::DOUBLE) return static_cast<int>(std::get<double>(v.value));
        return 0;
    };

    switch (stmt->op) {
    case AssignOp::EQ:
        *vp = val;
        break;
    case AssignOp::PLUSEQ: {
        VariantValue cur = *vp;
        VariantValue right = evalExpr(stmt->expr.get());
        if (!error_.empty()) return stmt->lineNumber;
        if (cur.type == VariantValue::STRING && right.type == VariantValue::STRING)
            *vp = VariantValue(std::get<std::string>(cur.value) + std::get<std::string>(right.value));
        else if (cur.type == VariantValue::DOUBLE || right.type == VariantValue::DOUBLE)
            *vp = VariantValue(promoteToDouble(cur) + promoteToDouble(right));
        else if (cur.type == VariantValue::FLOAT || right.type == VariantValue::FLOAT)
            *vp = VariantValue(promoteToFloat(cur) + promoteToFloat(right));
        else if (cur.type == VariantValue::INT && right.type == VariantValue::INT)
            *vp = VariantValue(std::get<int>(cur.value) + std::get<int>(right.value));
        else
            *vp = val;
        break;
    }
    case AssignOp::MINUSEQ: {
        VariantValue cur = *vp;
        VariantValue right = evalExpr(stmt->expr.get());
        if (!error_.empty()) return stmt->lineNumber;
        if (cur.type == VariantValue::DOUBLE || right.type == VariantValue::DOUBLE)
            *vp = VariantValue(promoteToDouble(cur) - promoteToDouble(right));
        else if (cur.type == VariantValue::FLOAT || right.type == VariantValue::FLOAT)
            *vp = VariantValue(promoteToFloat(cur) - promoteToFloat(right));
        else
            *vp = VariantValue(getIntVal(cur) - getIntVal(right));
        break;
    }
    case AssignOp::STAREQ: {
        VariantValue cur = *vp;
        VariantValue right = evalExpr(stmt->expr.get());
        if (!error_.empty()) return stmt->lineNumber;
        if (cur.type == VariantValue::DOUBLE || right.type == VariantValue::DOUBLE)
            *vp = VariantValue(promoteToDouble(cur) * promoteToDouble(right));
        else if (cur.type == VariantValue::FLOAT || right.type == VariantValue::FLOAT)
            *vp = VariantValue(promoteToFloat(cur) * promoteToFloat(right));
        else
            *vp = VariantValue(getIntVal(cur) * getIntVal(right));
        break;
    }
    case AssignOp::SLASHEQ: {
        VariantValue cur = *vp;
        VariantValue right = evalExpr(stmt->expr.get());
        if (!error_.empty()) return stmt->lineNumber;
        double r = promoteToDouble(right);
        if (r == 0.0) { error_ = "除以零错误"; return stmt->lineNumber; }
        if (cur.type == VariantValue::DOUBLE || right.type == VariantValue::DOUBLE)
            *vp = VariantValue(promoteToDouble(cur) / r);
        else if (cur.type == VariantValue::FLOAT || right.type == VariantValue::FLOAT)
            *vp = VariantValue(promoteToFloat(cur) / static_cast<float>(r));
        else {
            int divisor = getIntVal(right);
            if (divisor == 0) { error_ = "除以零错误"; return stmt->lineNumber; }
            *vp = VariantValue(getIntVal(cur) / divisor);
        }
        break;
    }
    case AssignOp::PERCENTEQ: {
        VariantValue cur = *vp;
        VariantValue right = evalExpr(stmt->expr.get());
        if (!error_.empty()) return stmt->lineNumber;
        int a = getIntVal(cur);
        int b = getIntVal(right);
        if (b == 0) { error_ = "模零错误"; return stmt->lineNumber; }
        *vp = VariantValue(a % b);
        break;
    }
    default:
        *vp = val;
        break;
    }
    updateMemoryForVar(stmt->name, *vp);
    return stmt->lineNumber;
}

int Executor::execIf(IfStmt* stmt) {
    VariantValue cond = evalExpr(stmt->cond.get());
    if (!error_.empty()) return stmt->lineNumber;
    bool b = false;
    if (cond.type == VariantValue::BOOL) b = std::get<bool>(cond.value);
    else if (cond.type == VariantValue::INT) b = (std::get<int>(cond.value) != 0);
    else if (cond.type == VariantValue::STRING) b = !std::get<std::string>(cond.value).empty();

    Stmt* target = b ? stmt->thenBody.get() : (stmt->elseBody ? stmt->elseBody.get() : nullptr);
    if (!target) return stmt->lineNumber;

    if (stepMode_) {
        // Substip: expand taken branch, execute one inner stmt per step()
        ifBodyStmts_.clear();
        ifBodyPos_ = 0;
        expandStmtBody(target, ifBodyStmts_);
        inIfSubstep_ = true;
        return stmt->lineNumber;
    }

    // runAll mode: execute recursively
    return execStmt(target);
}

int Executor::execWhile(WhileStmt* stmt) {
    // Evaluate condition once (for both modes)
    VariantValue cond = evalExpr(stmt->cond.get());
    if (!error_.empty()) return stmt->lineNumber;
    bool b = false;
    if (cond.type == VariantValue::BOOL) b = std::get<bool>(cond.value);
    else if (cond.type == VariantValue::INT) b = (std::get<int>(cond.value) != 0);
    else if (cond.type == VariantValue::STRING) b = !std::get<std::string>(cond.value).empty();
    if (!b) return stmt->lineNumber;  // condition false → skip loop

    if (stepMode_) {
        // Substep: expand body, execute one inner stmt per step()
        if (inLoopSubstep_) {
            loopStack_.push_back({loopStmt_, std::move(loopBodyStmts_), loopBodyPos_});
        }
        loopBodyStmts_.clear();
        loopBodyPos_ = 0;
        loopStmt_ = stmt;
        expandStmtBody(stmt->body.get(), loopBodyStmts_);
        inLoopSubstep_ = true;
        return stmt->lineNumber;
    }

    // runAll mode: execute all iterations
    int lastLine = stmt->lineNumber;
    while (true) {
        if (onYield_) onYield_();
        if (stmt->cond) {
            VariantValue cv = evalExpr(stmt->cond.get());
            if (!error_.empty()) break;
            bool cb = false;
            if (cv.type == VariantValue::BOOL) cb = std::get<bool>(cv.value);
            else if (cv.type == VariantValue::INT) cb = (std::get<int>(cv.value) != 0);
            else if (cv.type == VariantValue::STRING) cb = !std::get<std::string>(cv.value).empty();
            if (!cb) break;
        }
        lastLine = execStmt(stmt->body.get());
        if (breakRequested_) { breakRequested_ = false; break; }
        if (continueRequested_) { continueRequested_ = false; }
        if (!error_.empty()) break;
        if (stmt->update) { evalExpr(stmt->update.get()); if (!error_.empty()) break; }
    }
    return lastLine;
}

int Executor::execDoWhile(DoWhileStmt* stmt) {
    if (stepMode_) {
        // Substep: expand body, execute one inner stmt per step()
        // (do-while always executes body at least once)
        loopBodyStmts_.clear();
        loopBodyPos_ = 0;
        loopStmt_ = stmt;
        expandStmtBody(stmt->body.get(), loopBodyStmts_);
        inLoopSubstep_ = true;
        return stmt->lineNumber;
    }

    // runAll mode: execute all iterations
    int lastLine = stmt->lineNumber;
    do {
        if (onYield_) onYield_();
        lastLine = execStmt(stmt->body.get());
        if (breakRequested_) { breakRequested_ = false; break; }
        if (continueRequested_) { continueRequested_ = false; }
        if (!error_.empty()) break;
        VariantValue cond = evalExpr(stmt->cond.get());
        bool b = false;
        if (cond.type == VariantValue::BOOL) b = std::get<bool>(cond.value);
        else if (cond.type == VariantValue::INT) b = (std::get<int>(cond.value) != 0);
        else if (cond.type == VariantValue::STRING) b = !std::get<std::string>(cond.value).empty();
        if (!b) break;
    } while (true);
    return lastLine;
}

int Executor::execFor(ForStmt* stmt) {
    if (stmt->init) { execStmt(stmt->init.get()); if (!error_.empty()) return stmt->lineNumber; }

    if (stmt->cond) {
        VariantValue cond = evalExpr(stmt->cond.get());
        if (!error_.empty()) return stmt->lineNumber;
        bool b = false;
        if (cond.type == VariantValue::BOOL) b = std::get<bool>(cond.value);
        else if (cond.type == VariantValue::INT) b = (std::get<int>(cond.value) != 0);
        else if (cond.type == VariantValue::STRING) b = !std::get<std::string>(cond.value).empty();
        if (!b) return stmt->lineNumber;  // condition false → skip loop
    }

    if (stepMode_) {
        // Substep: expand body, execute one inner stmt per step()
        if (inLoopSubstep_) {
            loopStack_.push_back({loopStmt_, std::move(loopBodyStmts_), loopBodyPos_});
        }
        loopBodyStmts_.clear();
        loopBodyPos_ = 0;
        loopStmt_ = stmt;
        expandStmtBody(stmt->body.get(), loopBodyStmts_);
        inLoopSubstep_ = true;
        return stmt->lineNumber;
    }

    // runAll mode: execute all iterations
    int lastLine = stmt->lineNumber;
    while (true) {
        if (onYield_) onYield_();
        lastLine = execStmt(stmt->body.get());
        if (breakRequested_) { breakRequested_ = false; break; }
        if (continueRequested_) { continueRequested_ = false; }
        if (!error_.empty()) break;
        if (stmt->update) { evalExpr(stmt->update.get()); if (!error_.empty()) break; }
        if (stmt->cond) {
            VariantValue cond = evalExpr(stmt->cond.get());
            if (!error_.empty()) break;
            bool b = false;
            if (cond.type == VariantValue::BOOL) b = std::get<bool>(cond.value);
            else if (cond.type == VariantValue::INT) b = (std::get<int>(cond.value) != 0);
            else if (cond.type == VariantValue::STRING) b = !std::get<std::string>(cond.value).empty();
            if (!b) break;
        }
    }
    return lastLine;
}

int Executor::execRangeFor(RangeForStmt* stmt) {
    VariantValue container = evalExpr(stmt->container.get());
    if (!error_.empty()) return stmt->lineNumber;

    auto iterateElements = [&](auto& vec) {
        for (auto& elem : vec) {
            if (onYield_) onYield_();
            vars_[stmt->varName] = VariantValue(elem);
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
            if (!error_.empty()) break;
        }
    };

    switch (container.type) {
    case VariantValue::VECTOR_INT: iterateElements(std::get<std::vector<int>>(container.value)); break;
    case VariantValue::VECTOR_FLOAT: iterateElements(std::get<std::vector<float>>(container.value)); break;
    case VariantValue::VECTOR_DOUBLE: iterateElements(std::get<std::vector<double>>(container.value)); break;
    case VariantValue::VECTOR_STRING: iterateElements(std::get<std::vector<std::string>>(container.value)); break;
    case VariantValue::VECTOR_CHAR: iterateElements(std::get<std::vector<char>>(container.value)); break;
    case VariantValue::VECTOR_OBJECT: {
        auto& vec = std::get<std::vector<std::shared_ptr<RuntimeObject>>>(container.value);
        for (auto& elem : vec) {
            if (onYield_) onYield_();
            VariantValue v;
            v.type = VariantValue::POINTER;
            v.value = elem;
            vars_[stmt->varName] = v;
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
            if (!error_.empty()) break;
        }
        break;
    }
    case VariantValue::STRING: {
        for (char c : std::get<std::string>(container.value)) {
            if (onYield_) onYield_();
            vars_[stmt->varName] = VariantValue(c);
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
        }
        break;
    }
    case VariantValue::LIST_INT: {
        for (int elem : std::get<std::list<int>>(container.value)) {
            if (onYield_) onYield_();
            vars_[stmt->varName] = VariantValue(elem);
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
            if (!error_.empty()) break;
        }
        break;
    }
    case VariantValue::LIST_STRING: {
        for (auto& elem : std::get<std::list<std::string>>(container.value)) {
            if (onYield_) onYield_();
            vars_[stmt->varName] = VariantValue(elem);
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
            if (!error_.empty()) break;
        }
        break;
    }
    case VariantValue::SET_INT: {
        for (int elem : std::get<std::set<int>>(container.value)) {
            if (onYield_) onYield_();
            vars_[stmt->varName] = VariantValue(elem);
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
            if (!error_.empty()) break;
        }
        break;
    }
    case VariantValue::MAP_STRING_INT: {
        for (auto& kv : std::get<std::map<std::string, int>>(container.value)) {
            if (onYield_) onYield_();
            auto pairObj = std::make_shared<RuntimeObject>();
            pairObj->className = "pair";
            pairObj->members["first"] = VariantValue(kv.first);
            pairObj->members["second"] = VariantValue(kv.second);
            VariantValue pairVal;
            pairVal.type = VariantValue::OBJECT;
            pairVal.value = pairObj;
            vars_[stmt->varName] = pairVal;
            execStmt(stmt->body.get());
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; continue; }
            if (!error_.empty()) break;
        }
        break;
    }
    default:
        error_ = "该类型不支持范围 for 循环";
    }
    return stmt->lineNumber;
}

int Executor::execBreak(BreakStmt* stmt) {
    breakRequested_ = true;
    return stmt->lineNumber;
}

int Executor::execContinue(ContinueStmt* stmt) {
    continueRequested_ = true;
    return stmt->lineNumber;
}

int Executor::execCin(CinStmt* stmt) {
    for (auto& name : stmt->names) {
        if (onCinRequest_) {
            std::string input = onCinRequest_();
            VariantValue val;
            auto it = vars_.find(name);
            if (it != vars_.end()) {
                if (it->second.type == VariantValue::INT)
                    val = VariantValue(std::stoi(input));
                else if (it->second.type == VariantValue::FLOAT)
                    val = VariantValue(std::stof(input));
                else if (it->second.type == VariantValue::DOUBLE)
                    val = VariantValue(std::stod(input));
                else if (it->second.type == VariantValue::STRING)
                    val = VariantValue(input);
                else if (it->second.type == VariantValue::CHAR)
                    val = VariantValue(input.empty() ? '\0' : input[0]);
                else if (it->second.type == VariantValue::BOOL)
                    val = VariantValue(input == "1" || input == "true");
                else val = VariantValue(std::stoi(input));
            } else {
                val = VariantValue(std::stoi(input));
            }
            vars_[name] = val;
            updateMemoryForVar(name, val);
        }
    }
    return stmt->lineNumber;
}

void Executor::evaluateCoutExpr(Expr* expr) {
    if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
        if (bin->op == BinOp::SHL) {
            evaluateCoutExpr(bin->left.get());
            evaluateCoutExpr(bin->right.get());
            return;
        }
    }
    VariantValue val = evalExpr(expr);
    if (error_.empty()) {
        std::string s = val.toString();
        if (val.type == VariantValue::STRING && s.size() >= 2 && s.front() == '"' && s.back() == '"')
            s = s.substr(1, s.size() - 2);
        if (val.type == VariantValue::CHAR && s.size() >= 3 && s.front() == '\'' && s.back() == '\'')
            s = s.substr(1, s.size() - 2);
        consoleOut_ << s;
    }
}

int Executor::execCout(CoutStmt* stmt) {
    for (auto& item : stmt->items) {
        if (item.type == CoutItem::ENDL) {
            consoleOut_ << "\n";
        } else {
            evaluateCoutExpr(item.expr.get());
            if (!error_.empty()) return stmt->lineNumber;
        }
    }
    return stmt->lineNumber;
}

int Executor::execReturn(ReturnStmt* stmt) {
    if (!currentThis_) finished_ = true;
    return stmt->lineNumber;
}

int Executor::execTryCatch(TryCatchStmt* stmt) {
    // Push outer state for nested try/catch support
    tryCatchStack_.push_back({std::move(tryCatchStmts_), std::move(catchStmts_), inCatch_});
    tryCatchStmts_.clear();
    catchStmts_.clear();
    // Collect try body statements
    if (auto* blk = dynamic_cast<BlockStmt*>(stmt->tryBody.get())) {
        for (auto& s : blk->stmts) tryCatchStmts_.push_back(s.get());
    } else { tryCatchStmts_.push_back(stmt->tryBody.get()); }
    // Collect catch body statements
    for (auto& [excType, body] : stmt->catchClauses) {
        if (auto* blk = dynamic_cast<BlockStmt*>(body.get())) {
            for (auto& s : blk->stmts) catchStmts_.push_back(s.get());
        } else { catchStmts_.push_back(body.get()); }
    }
    inTryCatch_ = true;
    inCatch_ = false;
    return stmt->lineNumber;
}

int Executor::execThrow(ThrowStmt* stmt) {
    Exception exc;
    exc.lineNumber = stmt->lineNumber;
    if (stmt->expr) {
        exc.value = evalExpr(stmt->expr.get());
        if (exc.value.type == VariantValue::STRING)
            exc.type = "std::string";
        else if (exc.value.type == VariantValue::INT)
            exc.type = "int";
        else if (exc.value.type == VariantValue::OBJECT) {
            auto obj = std::get<std::shared_ptr<RuntimeObject>>(exc.value.value);
            exc.type = obj ? obj->className : "std::exception";
        } else
            exc.type = "std::exception";
    } else {
        exc.type = "std::exception";
        exc.value = VariantValue(std::string("exception"));
    }
    exceptionStack_.push(exc);
    unwinding_ = true;
    error_ = "异常: " + exc.value.toString();
    return stmt->lineNumber;
}

int Executor::execExprStmt(ExprStmt* stmt) {
    if (stmt->expr) evalExpr(stmt->expr.get());
    return stmt->lineNumber;
}

// ============================================================
// Type Casting
// ============================================================

VariantValue Executor::castToType(const VariantValue& val, const std::string& typeName) {
    if (typeName == "int") {
        if (val.type == VariantValue::INT) return val;
        if (val.type == VariantValue::FLOAT) return VariantValue(static_cast<int>(std::get<float>(val.value)));
        if (val.type == VariantValue::DOUBLE) return VariantValue(static_cast<int>(std::get<double>(val.value)));
        if (val.type == VariantValue::CHAR) return VariantValue(static_cast<int>(std::get<char>(val.value)));
        if (val.type == VariantValue::BOOL) return VariantValue(std::get<bool>(val.value) ? 1 : 0);
        return VariantValue(0);
    }
    if (typeName == "float") {
        if (val.type == VariantValue::FLOAT) return val;
        if (val.type == VariantValue::INT) return VariantValue(static_cast<float>(std::get<int>(val.value)));
        if (val.type == VariantValue::DOUBLE) return VariantValue(static_cast<float>(std::get<double>(val.value)));
        return VariantValue(0.0f);
    }
    if (typeName == "double") {
        if (val.type == VariantValue::DOUBLE) return val;
        if (val.type == VariantValue::INT) return VariantValue(static_cast<double>(std::get<int>(val.value)));
        if (val.type == VariantValue::FLOAT) return VariantValue(static_cast<double>(std::get<float>(val.value)));
        return VariantValue(0.0);
    }
    if (typeName == "string") {
        return VariantValue(val.toString());
    }
    if (typeName == "char") {
        if (val.type == VariantValue::CHAR) return val;
        if (val.type == VariantValue::INT) return VariantValue(static_cast<char>(std::get<int>(val.value)));
        return VariantValue('\0');
    }
    if (typeName == "bool") {
        if (val.type == VariantValue::BOOL) return val;
        if (val.type == VariantValue::INT) return VariantValue(std::get<int>(val.value) != 0);
        return VariantValue(false);
    }
    return val;
}

// ============================================================
// Memory Management
// ============================================================

void Executor::allocateMemoryForVar(const std::string& name, const VariantValue& val) {
    if (memoryMap_.find(name) == memoryMap_.end()) {
        memoryMap_[name] = {nextAddress_, val};
        nextAddress_ += val.byteSize();
    }
}

void Executor::updateMemoryForVar(const std::string& name, const VariantValue& val) {
    auto it = memoryMap_.find(name);
    if (it != memoryMap_.end()) {
        it->second.second = val;
    } else {
        allocateMemoryForVar(name, val);
    }
}

VariantValue* Executor::findVariable(const std::string& name) {
    auto it = vars_.find(name);
    if (it != vars_.end()) return &it->second;
    if (currentThis_) {
        auto mit = currentThis_->members.find(name);
        if (mit != currentThis_->members.end()) return &mit->second;
    }
    return nullptr;
}

void Executor::takeSnapshot(int line) {
    if (!snapshots_) return;
    snapshots_->takeSnapshot(line, vars_, consoleOut_.str(), memoryMap_);
}