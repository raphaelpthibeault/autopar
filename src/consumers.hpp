#ifndef CONSUMERS_HPP
#define CONSUMERS_HPP

#include "visitors.hpp"

#include <cstdio>

#include "clang/AST/ASTConsumer.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;

class TaskCreationASTConsumer : public ASTConsumer {
public:
  TaskCreationASTConsumer(Rewriter &R) : Visitor(R) {}

  virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b)
      Visitor.TraverseDecl(*b);
    return true;
  }

private:
  TaskCreationVisitor Visitor;
};

#endif
