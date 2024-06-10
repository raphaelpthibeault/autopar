#ifndef CONSUMERS_HPP
#define CONSUMERS_HPP

#include "visitors.hpp"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;

class TaskCreationASTConsumer : public ASTConsumer {
private:
    TaskCreationVisitor Visitor;
public:
  TaskCreationASTConsumer(Rewriter &R, ASTContext &AC) : Visitor(R, AC) {}

  virtual bool
  HandleTopLevelDecl(DeclGroupRef DR) {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      Visitor.TraverseDecl(*b);
    }

    return true;
  }
};

#endif
