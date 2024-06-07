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

        auto vars = extractVariables(Arg, RW);

        for (const auto &var : vars) {
            if (depType == WRITE) {
                depInfo.write.insert(var);
            } else {
                depInfo.read.insert(var);
            }
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

std::vector<std::string>
extractVariables(const Expr *expr, const Rewriter &RW) {
    std::vector<std::string> vars;

    if (const auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        vars.push_back(declRef->getDecl()->getNameAsString());
    } else if (const auto *arraySubscript = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)) {
        const auto *base = arraySubscript->getBase()->IgnoreImplicit();
        const auto *idx = arraySubscript->getIdx()->IgnoreImplicit();

        auto idxVars = extractVariables(idx, RW);
        vars.insert(vars.end(), idxVars.begin(), idxVars.end());

        std::string baseStr = RW.getRewrittenText(base->getSourceRange());
        std::string idxStr = RW.getRewrittenText(idx->getSourceRange());

        vars.push_back(baseStr + "[" + idxStr + "]");
    } else {
        for (auto b = expr->child_begin(), e = expr->child_end(); b != e; ++b) {
            if (const auto *childExpr = llvm::dyn_cast<clang::Expr>(*b)) {
                auto subVars = extractVariables(childExpr, RW);
                vars.insert(vars.end(), subVars.begin(), subVars.end());
            }
        }
    }

    return vars;
}
