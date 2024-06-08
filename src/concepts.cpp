#include "concepts.hpp"

#define READ 0
#define WRITE 1

int
countCallExprs(const Stmt *s) {
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
        vars.vars.insert(declRef->getDecl()->getNameAsString());
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
