#ifndef VISITORS_HPP
#define VISITORS_HPP

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>

#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;


class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
  MyASTVisitor(Rewriter &R) : RW(R) {}

  bool VisitStmt(Stmt *s) {
    if (isa<IfStmt>(s)) {
      IfStmt *IfStatement = cast<IfStmt>(s);
      Stmt *Then = IfStatement->getThen();

      RW.InsertText(Then->getBeginLoc(), "// the 'if' part\n", true,
                             true);

      Stmt *Else = IfStatement->getElse();
      if (Else)
        RW.InsertText(Else->getEndLoc(), "// the 'else' part\n",
                               true, true);
    } else if (isa<CallExpr>(s)) {
        CallExpr *FCall = cast<CallExpr>(s);

        for (unsigned i = 0; i < FCall->getNumArgs(); ++i) {
            Expr *Arg = FCall->getArg(i);
            QualType ArgType = Arg->getType();
            std::string ArgTypeStr = ArgType.getAsString();

            RW.InsertText(Arg->getBeginLoc(), "// Argument type: " + ArgTypeStr + "\n", true, true);
        }



    }

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *f) {
    if (f->hasBody()) {
      Stmt *FuncBody = f->getBody();

      QualType QT = f->getReturnType();
      std::string TypeStr = QT.getAsString();

      DeclarationName DeclName = f->getNameInfo().getName();
      std::string FuncName = DeclName.getAsString();

      std::stringstream SSBefore;
      SSBefore << "// Begin function " << FuncName << " returning " << TypeStr
               << "\n";
      SourceLocation ST = f->getSourceRange().getBegin();
      RW.InsertText(ST, SSBefore.str(), true, true);

      std::stringstream SSAfter;
      SSAfter << "\n// End function " << FuncName;
      ST = FuncBody->getEndLoc().getLocWithOffset(1);
      RW.InsertText(ST, SSAfter.str(), true, true);
    }

    return true;
  }

private:
  Rewriter &RW;
};

#endif
