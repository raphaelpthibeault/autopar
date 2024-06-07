#ifndef CONCEPTS_HPP
#define CONCEPTS_HPP

#include "clang/AST/Expr.h"
#include "clang/AST/Decl.h"

#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <set>
#include <llvm-14/llvm/Support/Casting.h>

using namespace clang;



struct DependInfo {
    std::set<std::string> read;
    std::set<std::string> write;
};

int countCallExprs(const Stmt *);
DependInfo getFCallDependencies(const FunctionDecl *, const CallExpr *, const Rewriter &);
std::string constructDependClause(const DependInfo &);
std::vector<std::string> extractVariables(const Expr *, const Rewriter &);

#endif
