#ifndef CONCEPTS_HPP
#define CONCEPTS_HPP

#include "clang/AST/Expr.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/LangOptions.h"

#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LangOptions.h"
#include <llvm-14/llvm/Support/raw_ostream.h>

using namespace clang;



struct DependInfo {
    std::vector<std::string> read;
    std::vector<std::string> write;
};

int countCallExprs(const Stmt *);
DependInfo getFCallDependencies(const FunctionDecl *, const CallExpr *, const LangOptions &);
std::string constructDependClause(const DependInfo &);
std::vector<std::string> extractVariables(const Expr *);

#endif
