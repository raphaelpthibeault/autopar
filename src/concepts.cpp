#include <concepts.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/Stmt.h>
#include <llvm-18/llvm/Support/Casting.h>
#include <llvm-18/llvm/Support/raw_ostream.h>

#define READ 0
#define WRITE 1

int
countCallExprs(const Stmt *s) {
    if (!s) return 0;

    int count = 0;

    for (const Stmt *Child : s->children()) {
        if (Child) {
            if (const auto *FCall = llvm::dyn_cast<CallExpr>(Child)) {
                const FunctionDecl *CalledFunc = FCall->getDirectCallee();
                if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && CalledFunc->getIdentifier()) {
                    ++count;
                }
            }
            count += countCallExprs(Child);
        }
    }

    return count;
}

bool
checkTaskCreation(const Stmt *s) {
    if (!s) return false;

    if (const auto *FCall = llvm::dyn_cast<CallExpr>(s)) {
        const FunctionDecl *CalledFunc = FCall->getDirectCallee();
        if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && CalledFunc->getIdentifier()) {
            return true;
        }
    }

    for (const Stmt *Child : s->children()) {
        if (checkTaskCreation(Child)) {
            return true;
        }
    }

    return false;
}

DependInfo
getFCallDependencies(const FunctionDecl *FDecl, const CallExpr *FCall, const Rewriter &RW) {
    DependInfo depInfo;

    for (unsigned i = 0; i < FDecl->getNumParams(); ++i) {
        const ParmVarDecl *Param = FDecl->getParamDecl(i);
        const QualType ParamType = Param->getType();
        const Expr *Arg = FCall->getArg(i)->IgnoreImplicit();
        int depType;

        if ((ParamType->isPointerType() || ParamType->isReferenceType())
            && !ParamType.isConstQualified() && !ParamType->getPointeeType().isConstQualified()) {
            depType = WRITE;
        } else {
            depType = READ;
        }

        Vars vars = extractVariables(Arg, RW);

        for (const auto &var : vars.vars) {
            if (depType == WRITE) {
                depInfo.write.insert(var);
            } else {
                depInfo.read.insert(var);
            }
        }

        for (const auto &idx : vars.idxs) {
            depInfo.read.insert(idx);
        }
    }

    return depInfo;
}

std::string
constructDependClause(const DependInfo & depInfo) {
    std::string dependClause;

    if (!depInfo.read.empty()) {
        dependClause += "depend(in: ";
        for (const auto &var : depInfo.read) {
            dependClause += var + ", ";
        }
        dependClause.pop_back();
        dependClause.pop_back();
        dependClause += ") ";
    }

    if (!depInfo.write.empty()) {
        dependClause += "depend(inout: ";
        for (const auto &var : depInfo.write) {
            dependClause += var + ", ";
        }
        dependClause.pop_back();
        dependClause.pop_back();
        dependClause += ")";
    }

    return dependClause;
}

Vars
extractVariables(const Expr *expr, const Rewriter &RW) {
    Vars vars;

    if (const auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        if (const auto *id = declRef->getDecl()->getIdentifier()) {
            vars.vars.insert(id->getName().str());
        }
    } else if (const auto *arraySubscript = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)) {
        const auto *base = arraySubscript->getBase()->IgnoreImplicit();
        const auto *idx = arraySubscript->getIdx()->IgnoreImplicit();

        auto idxVars = extractVariables(idx, RW);
        /*
        if W[X[y] + z], add to index variables X[y], y, z since they'll be reads
        */
        vars.idxs.insert(idxVars.vars.begin(), idxVars.vars.end());
        vars.idxs.insert(idxVars.idxs.begin(), idxVars.idxs.end());

        std::string baseStr = RW.getRewrittenText(base->getSourceRange());
        std::string idxStr = RW.getRewrittenText(idx->getSourceRange());

        vars.vars.insert(baseStr + "[" + idxStr + "]");
    } else {
        for (auto b = expr->child_begin(), e = expr->child_end(); b != e; ++b) {
            if (const auto *childExpr = llvm::dyn_cast<clang::Expr>(*b)) {
                auto subVars = extractVariables(childExpr, RW);
                vars.vars.insert(subVars.vars.begin(), subVars.vars.end());
                vars.idxs.insert(subVars.idxs.begin(), subVars.idxs.end());
            }
        }
    }

    /* if variable is both an index and a variable, consider it a variable */
    for (const auto &var : vars.vars) {
        if (vars.idxs.count(var)) {
           vars.idxs.erase(var);
        }
    }

    return vars;
}

const Stmt *
getParentIfLoop(const Expr *e, ASTContext &Context) {
    const Stmt *curr = e;
    const Stmt *parent = nullptr;

    ParentMapContext &parentMapContext = Context.getParentMapContext();

    while (curr) {
        auto parents = parentMapContext.getParents(*curr);
        if (parents.empty()) break;

        const Stmt *p = parents[0].get<Stmt>();
        if (p) {
            if (llvm::isa<ForStmt>(p) || llvm::isa<WhileStmt>(p) || llvm::isa<IfStmt>(p)) {
                parent = p;
            }
            curr = p;
        } else {
            break;
        }
    }

    return parent;
}

const CallExpr *
findCallExpr(const Stmt * s) {
    if (!s) return nullptr;

    for (const Stmt *Child : s->children()) {
        if (Child) {
            if (const auto *FCall = llvm::dyn_cast<CallExpr>(Child)) {
                const FunctionDecl *CalledFunc = FCall->getDirectCallee();
                if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isInStdNamespace() && CalledFunc->getIdentifier()) {
                    return FCall;
                }
            }
            if (const CallExpr *FoundCall = findCallExpr(Child)) {
                return FoundCall;
            }
        }
    }

    return nullptr;
}
