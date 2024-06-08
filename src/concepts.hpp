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

struct Task {
    int id;
    DependInfo depInfo;
};

struct Function {
    int id;
    std::string name;
    std::vector<Task> tasks;
};

struct Vars {
    std::set<std::string> vars;
    std::set<std::string> idxs;
};

int countCallExprs(const Stmt *);
DependInfo getFCallDependencies(const FunctionDecl *, const CallExpr *, const Rewriter &);
std::string constructDependClause(const DependInfo &);
Vars extractVariables(const Expr *, const Rewriter &);

#endif
