#ifndef VISITORS_HPP
#define VISITORS_HPP

#include <cstdio>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/ParentMapContext.h"

#include "concepts.hpp"


class TaskCreationVisitor : public RecursiveASTVisitor<TaskCreationVisitor> {
private:
    int ignoreCalls;
    std::set<std::string> awaited;
    int funcId;
    int taskId;
    std::vector<Function> functions;
    Rewriter &RW;
    ASTContext &AC;
    FunctionDecl *currentFunction;

public:
    TaskCreationVisitor(Rewriter &RW, ASTContext &AC) : RW(RW), AC(AC) {
        ignoreCalls = 0;
        funcId = 0;
        taskId = 0;
    }

    bool
    TraverseCallExpr(CallExpr *FCall) {
        return VisitCallExpr(FCall);
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

                        if (shouldAddTaskWait(depInfo)) {
                            RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp taskwait\n", true, true);
                        }

                        RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp task " + depClause + "\n{\n" + initializer + "\n}\n", true, true);

                        addTask(depInfo);
                    }

                } else if (VarDecl->hasInit()) {

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

            if (shouldAddTaskWait(depInfo)) {
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
        if (!f->isDefined()) return true;

        Stmt *FuncBody = f->getBody();
        std::string FuncName = f->getNameInfo().getName().getAsString();
        std::string returnTypeStr = f->getReturnType().getAsString();
        bool taskCreated = checkTaskCreation(FuncBody);
        currentFunction = f;

        if (!taskCreated) {
            return true;
        }

        if (returnTypeStr != "void") {
            RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n" + returnTypeStr + " autopar_res;\n", true, true);
        }

        if (FuncName == "main") {
            RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp parallel\n#pragma omp master", true, true);
        }

        addFunction(FuncName);
        RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp taskgroup\n{", true, true);

        std::string endLabel = "\nautopar_endtaskgrouplabel_" + FuncName + ": ;\n}\n";
        if (returnTypeStr != "void") {
            endLabel += "return autopar_res;\n";
        }

        RW.InsertText(FuncBody->getEndLoc(), endLabel, true, true);

        return true;
    }

    bool
    VisitExpr(Expr *e) {
        if (!(isa<BinaryOperator>(e) || isa<UnaryOperator>(e))) {
            return true;
        }

        const Stmt *curr = e;
        int nbCallExprs = countCallExprs(curr);

        if (nbCallExprs > 0) {
            ignoreCalls += nbCallExprs;

            for (auto *Child : curr->children()) {
                if (Child) {
                    if (auto *FCall = llvm::dyn_cast<CallExpr>(Child)) {
                        const FunctionDecl *CalledFunc = FCall->getDirectCallee();
                        if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace()) {
                            DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW);
                            std::string depClause = constructDependClause(depInfo);

                            if (shouldAddTaskWait(depInfo)) {
                                RW.InsertText(e->getBeginLoc(), "#pragma omp taskwait\n\n", true, true);
                            }

                            RW.InsertText(e->getBeginLoc(), "#pragma omp task " + depClause + "\n{\n", true, true);
                            RW.InsertText(e->getEndLoc().getLocWithOffset(2), "\n}\n", true, true);

                            addTask(depInfo);
                        }
                    }
                }
            }
        } else {
            Vars vars = extractVariables(e, RW);

            if (shouldAddTaskWait(vars)) {
                if (auto parent = getParentIfLoop(e, AC)) {
                    RW.InsertText(parent->getBeginLoc(), "\n#pragma omp taskwait\n", true, true);
                } else {
                    RW.InsertText(e->getBeginLoc(), "\n#pragma omp taskwait\n", true, true);
                }
            }
        }

        return true;
    }

    bool
    VisitReturnStmt(ReturnStmt *ret) {
        //ret->printPretty(llvm::outs(), nullptr, AC.getPrintingPolicy());

        if (!currentFunction) {
            llvm::errs() << "Error: VisitReturnStmt() no current function\n";
        }

        if (!checkTaskCreation(currentFunction->getBody())) return true;

        std::string assignTxt;

        Expr *retValue = ret->getRetValue();
        if (retValue) {
            assignTxt = "autopar_res = " + RW.getRewrittenText(retValue->getSourceRange()) + ";\n";
        }
        std::string gotoTxt = "goto apac_endtaskgrouplabel_" + currentFunction->getNameAsString() + ";";

        SourceLocation begin = ret->getBeginLoc();
        SourceLocation end = ret->getEndLoc().getLocWithOffset(1);

        auto lineText = Lexer::getIndentationForLine(begin, AC.getSourceManager());
        auto indentation = lineText.substr(0, lineText.find_first_not_of(" \t"));

        SourceRange range(begin, end);
        RW.ReplaceText(range, "#pragma omp taskwait\n" + indentation.str() + assignTxt + indentation.str() + gotoTxt);

        return true;
    }

private:
    void
    addFunction(std::string funcName) {
        taskId = 0;
        awaited.clear();
        Function curr;
        curr.name = funcName;
        curr.id = funcId++;
        functions.push_back(curr);
    }

    void
    addTask(DependInfo depInfo) {
        Task curr;

        for (auto var : depInfo.write) {
            if (awaited.count(var)) {
                awaited.erase(var);
            }
        }

        curr.depInfo = depInfo;
        curr.id = taskId++;
        functions.back().tasks.push_back(curr);
    }

    bool
    shouldAddTaskWait(DependInfo depInfo) {
        if (functions.size() == 0) return false;

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

    bool
    shouldAddTaskWait(Vars vars) {
        if (functions.size() == 0) return false;

        for (auto task : functions.back().tasks) {
            for (auto var : vars.vars) {
                if (task.depInfo.write.count(var) && !awaited.count(var)) {
                    awaited.insert(var);
                    return true;
                }
            }
            for (auto var : vars.idxs) {
                if (task.depInfo.write.count(var) && !awaited.count(var)) {
                    awaited.insert(var);
                    return true;
                }
            }
        }

        return false;
    }

};

#endif
