#ifndef VISITORS_HPP
#define VISITORS_HPP

#include <cstdio>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Type.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "concepts.hpp"


class TaskCreationVisitor : public RecursiveASTVisitor<TaskCreationVisitor> {
private:
    int ignoreCalls;
    int funcId;
    int taskId;
    std::vector<Function> functions;
    Rewriter &RW;

public:
    TaskCreationVisitor(Rewriter &R) : RW(R) {
        ignoreCalls = 0;
        funcId = 0;
        taskId = 0;
    }

    bool
    VisitDeclStmt(DeclStmt *DeclStat) {
        int nbCallExprs = countCallExprs(DeclStat);

        if (nbCallExprs > 0) {
            ignoreCalls += nbCallExprs;
        }

        for (const auto *Decl : DeclStat->decls()) {
            if (const auto *VarDecl = llvm::dyn_cast<clang::VarDecl>(Decl)) {
                if (VarDecl->hasInit() && llvm::isa<CallExpr>(VarDecl->getInit())) {
                    const CallExpr *FCall = llvm::cast<clang::CallExpr>(VarDecl->getInit());
                    const FunctionDecl *CalledFunc = FCall->getDirectCallee();

                    if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace()) {
                        DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW); /* MUST BE BEFORE REWRITING */

                        std::string varName = VarDecl->getNameAsString();
                        std::string varType = VarDecl->getType().getAsString();
                        varType = varType.find("const") != std::string::npos ? varType.substr(6) : varType;

                        std::string initializer = varName + " = " + RW.getRewrittenText(VarDecl->getInit()->getSourceRange()) + ";";
                        RW.ReplaceText(VarDecl->getSourceRange(), varType + " " + varName);

                        depInfo.write.insert(varName);
                        std::string depClause = constructDependClause(depInfo);

                        if (addTaskWait(depInfo)) {
                            RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp taskwait\n", true, true);
                        }

                        RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp task " + depClause + "\n{\n" + initializer + "\n}\n", true, true);

                        addTask(depInfo);
                    }

                } else if (VarDecl->hasInit()) {
                    // while loop or recursive function

                }
            }
        }

        return true;
    }

    bool
    VisitCallExpr(CallExpr *FCall) {
        const FunctionDecl *CalledFunc = FCall->getDirectCallee();
        if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && ignoreCalls == 0) {
            DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW);
            std::string depClause = constructDependClause(depInfo);

            if (addTaskWait(depInfo)) {
                RW.InsertText(FCall->getBeginLoc(), "#pragma omp taskwait\n\n", true, true);
            }

            RW.InsertText(FCall->getBeginLoc(), "#pragma omp task " + depClause + "\n{\n", true, true);
            RW.InsertText(FCall->getEndLoc().getLocWithOffset(2), "\n}\n", true, true);

            addTask(depInfo);
        }

        if (ignoreCalls > 0)
            ignoreCalls--;

        return true;
    }

    bool
    VisitFunctionDecl(FunctionDecl *f) {
        if (!f->hasBody()) return true;

        Stmt *FuncBody = f->getBody();

        QualType QT = f->getReturnType();
        std::string TypeStr = QT.getAsString();

        DeclarationName DeclName = f->getNameInfo().getName();
        std::string FuncName = DeclName.getAsString();

        bool createsTask = false;
        for (Stmt::child_iterator CB = FuncBody->child_begin(), CE = FuncBody->child_end(); CB != CE; ++CB) {
            if (CallExpr *FCall = llvm::dyn_cast<CallExpr>(*CB)) {
                FunctionDecl *CalledFunc = FCall->getDirectCallee();
                if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace()) {
                    createsTask = true;
                    break;
                }
            }
        }

        if (FuncName == "main") {
            RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp parallel\n#pragma omp master", true, true);
        }

        if (createsTask) {
            addFunction(FuncName);

            RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp taskgroup\n{", true, true);
            RW.InsertText(FuncBody->getEndLoc(), "}\n", true, true);
        }

        return true;
    }

private:
    void
    addFunction(std::string funcName) {
        taskId = 0;
        Function curr;
        curr.name = funcName;
        curr.id = funcId++;
        functions.push_back(curr);
    }

    void
    addTask(DependInfo depInfo) {
        Task curr;
        curr.depInfo = depInfo;
        curr.id = taskId++;
        functions.back().tasks.push_back(curr);
    }

    bool addTaskWait(DependInfo depInfo) {
        for (auto task : functions.back().tasks) {
            for (auto read : depInfo.read) {
                if (task.depInfo.write.count(read)) {
                    return true;
                }
            }
            for (auto write : depInfo.write) {
                if (task.depInfo.write.count(write)) {
                    return true;
                }
            }
        }

        return false;
    }

};


#endif
