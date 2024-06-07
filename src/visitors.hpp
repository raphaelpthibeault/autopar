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

public:
    TaskCreationVisitor(Rewriter &R) : RW(R) {
        ignoreCalls = 0;
    }

    int countCallExprs(const clang::Stmt *s) const {
        int count = 0;
        for (auto *Child : s->children()) {
            if (Child) {
                if (auto *FCall = llvm::dyn_cast<CallExpr>(Child)) {
                    const FunctionDecl *CalledFunc = FCall->getDirectCallee();
                    if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace()) {
                        ++count;
                    }
                }
                count += countCallExprs(Child);
            }
        }
        return count;
    }

    bool VisitDeclStmt(DeclStmt *DeclStat) {
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
                        DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW.getLangOpts());

                        std::string varName = VarDecl->getNameAsString();
                        std::string varType = VarDecl->getType().getAsString();
                        varType = varType.find("const") != std::string::npos ? varType.substr(6) : varType;

                        std::string initializer = varName + " = " + RW.getRewrittenText(VarDecl->getInit()->getSourceRange()) + ";";
                        RW.ReplaceText(VarDecl->getSourceRange(), varType + " " + varName);

                        depInfo.write.push_back(varName);
                        std::string depClause = constructDependClause(depInfo);

                        RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp task " + depClause + "\n{\n" + initializer + "\n}\n", true, true);
                    }

                } else if (VarDecl->hasInit()) {
                    // while loop or recursive function

                }
            }
        }

        return true;
    }

    bool VisitCallExpr(CallExpr *FCall) {
        const FunctionDecl *CalledFunc = FCall->getDirectCallee();
        if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && ignoreCalls == 0) {
            DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW.getLangOpts());
            std::string depClause = constructDependClause(depInfo);

            RW.InsertText(FCall->getBeginLoc(), "#pragma omp task " + depClause + "\n{\n", true, true);
            RW.InsertText(FCall->getEndLoc().getLocWithOffset(2), "\n}\n", true, true);
        }

        if (ignoreCalls > 0)
            ignoreCalls--;

        return true;
    }

    bool VisitFunctionDecl(FunctionDecl *f) {
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
            RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp taskgroup\n{", true, true);
            RW.InsertText(FuncBody->getEndLoc(), "}\n", true, true);
        }

        return true;
    }

private:
  Rewriter &RW;
};


#endif
