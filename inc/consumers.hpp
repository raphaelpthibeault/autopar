#ifndef CONSUMERS_HPP
#define CONSUMERS_HPP

#include "visitors.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Frontend/FrontendAction.h>

using namespace clang;

class TaskCreationASTConsumer : public ASTConsumer {
private:
    TaskCreationVisitor Visitor;
public:
    TaskCreationASTConsumer(Rewriter &R, ASTContext &AC) : Visitor(R, AC) {}

    bool HandleTopLevelDecl(DeclGroupRef DR) override {
        for (auto & b : DR) {
            Visitor.TraverseDecl(b);
        }

        return true;
    }
};

#endif
