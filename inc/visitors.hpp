#ifndef VISITORS_HPP
#define VISITORS_HPP

#include <llvm-18/llvm/Support/Casting.h>
#include <llvm-18/llvm/Support/raw_ostream.h>
#include <string>
#include <utility>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Basic/SourceManager.h>

#include "concepts.hpp"

static const std::string AUTOPAR_TASK_CLAUSE = "firstprivate(AUTOPAR_lnbdepth) if(AUTOPAR_createtaskdepth || AUTOPAR_createtasknbr) default(shared)";

static const std::string AUTOPAR_TASK_LIMITER_CODE_TASKGROUP = R"(bool AUTOPAR_createtasknbr =
(AUTOPAR_currentLimiter != AUTOPAR_TASK_LIMITER_NO
    && (AUTOPAR_currentLimiter == AUTOPAR_TASK_NO_LIMIT
        || ((AUTOPAR_currentLimiter & AUTOPAR_TASK_LIMITER_NB)
            && (AUTOPAR_nbtask<AUTOPAR_maxtask)
		)
    )
);

int AUTOPAR_lnbdepth = AUTOPAR_nbdepth;

bool AUTOPAR_createtaskdepth =
(AUTOPAR_currentLimiter != AUTOPAR_TASK_LIMITER_NO
	&& (AUTOPAR_currentLimiter == AUTOPAR_TASK_NO_LIMIT
	   || ((AUTOPAR_currentLimiter & AUTOPAR_TASK_LIMITER_DEPTH)
		   && (AUTOPAR_lnbdepth<AUTOPAR_maxdepth)
	   )
    )
);
)";

static const std::string AUTOPAR_PRE_TASK = R"(
if(AUTOPAR_createtasknbr){
	#pragma omp atomic
	AUTOPAR_nbtask++;
})";


class TaskCreationVisitor : public RecursiveASTVisitor<TaskCreationVisitor> {
public:
    TaskCreationVisitor(Rewriter &RW, ASTContext &AC) 
        : RW(RW)
        , AC(AC)
        , MainFileId(AC.getSourceManager().getMainFileID()) {
        ignoreCalls = 0;
        funcId = 0;
        taskId = 0;
    }

    bool TraverseCallExpr(CallExpr *FCall) {
        return VisitCallExpr(FCall);
    }

    bool VisitDeclStmt(DeclStmt *DeclStat);
    bool VisitCallExpr(CallExpr *FCall);
    bool VisitFunctionDecl(FunctionDecl *f);
    bool VisitExpr(Expr *e);
    bool VisitReturnStmt(ReturnStmt *ret);

private:
    int ignoreCalls;
    std::set<std::string> awaited;
    int funcId;
    int taskId;
    std::vector<Function> functions;
    Rewriter &RW;
    ASTContext &AC;
    FunctionDecl *currentFunction;
    FileID MainFileId;

    void addFunction(std::string funcName);
    void addTask(DependInfo depInfo);
    bool shouldAddTaskWait(const DependInfo& depInfo);
    bool shouldAddTaskWait(const Vars& vars);


    bool isFromMainFile(SourceLocation loc) {
       return AC.getSourceManager().isInMainFile(loc);
    }

};

#endif
