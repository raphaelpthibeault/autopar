#pragma once

#include "consumers.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Frontend/FrontendAction.h>
#include <fstream>

using namespace clang;

static const std::string AUTOPAR_LIMITER_CODE = R"(#include <omp.h>
#include <cstdlib>
#include <string>
enum AUTOPAR_TASK_LIMITER {
    AUTOPAR_TASK_LIMITER_NO = 0,
    AUTOPAR_TASK_NO_LIMIT = 1 << 0,
    AUTOPAR_TASK_LIMITER_DEPTH = 1 << 1,
    AUTOPAR_TASK_LIMITER_NB = 1 << 2,
    AUTOPAR_TASK_LIMITER_BOTH = ~AUTOPAR_TASK_NO_LIMIT
};

AUTOPAR_TASK_LIMITER AUTOPAR_Limiter() {
    if (const char* env_p = std::getenv("AUTOPAR_LIMITER")) {
        std::string AUTOPAR_cl = env_p;
        if (AUTOPAR_cl == "NO") return AUTOPAR_TASK_LIMITER_NO;
        if (AUTOPAR_cl == "NOLIMIT") return AUTOPAR_TASK_NO_LIMIT;
        if (AUTOPAR_cl == "DEPTH") return AUTOPAR_TASK_LIMITER_DEPTH;
        if (AUTOPAR_cl == "NB") return AUTOPAR_TASK_LIMITER_NB;
        if (AUTOPAR_cl == "BOTH") return AUTOPAR_TASK_LIMITER_BOTH;
    }
    return AUTOPAR_TASK_LIMITER_BOTH;
}

const static int AUTOPAR_currentLimiter = AUTOPAR_Limiter();

int AUTOPAR_MaxNbTasks() {
    if (const char* env_p = std::getenv("AUTOPAR_MAX_NB_TASKS")) {
        return std::atoi(env_p);
    }
    return omp_get_max_threads() * 10;
}

const static int AUTOPAR_maxtask = AUTOPAR_MaxNbTasks();
int AUTOPAR_nbtask = 0;

int AUTOPAR_MaxDepth() {
    if (const char* env_p = std::getenv("AUTOPAR_MAX_DEPTH")) {
        return std::atoi(env_p);
    }
    return 5;
}

const static int AUTOPAR_maxdepth = AUTOPAR_MaxDepth();
int AUTOPAR_nbdepth = 0;

#pragma omp threadprivate(AUTOPAR_nbdepth)
)";

class TaskCreationFrontendAction : public ASTFrontendAction {
private:
    Rewriter R;
public:
    TaskCreationFrontendAction() = default;

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        ASTContext &AC = CI.getASTContext();
        R.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<TaskCreationASTConsumer>(R, AC);
    }

    bool BeginSourceFileAction(CompilerInstance &CI) override {
        const auto &FileID = CI.getSourceManager().getMainFileID();
        const FileEntry *MainFileEntry = CI.getSourceManager().getFileEntryForID(FileID);

        if (!MainFileEntry) {
            return false;
        }

        llvm::errs() << "Processing main file: " << MainFileEntry->getName() << "\n";

        return true;
    }

    void EndSourceFileAction() override {
        SourceManager &SM = R.getSourceMgr();
        llvm::errs() << "** EndSourceFileAction for: " << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";

        const RewriteBuffer *RewriteBuf = R.getRewriteBufferFor(SM.getMainFileID());

        if (!RewriteBuf) {
            llvm::errs() << "No rewrite buffer found.\n";
            return;
        }

        std::string outputFilePath = "output.cpp";

        std::ofstream outFile(outputFilePath);

        if (!outFile) {
            llvm::errs() << "Error opening file for writing: " << outputFilePath << "\n";
            return;
        }

        outFile << AUTOPAR_LIMITER_CODE << "\n\n\n" << std::string(RewriteBuf->begin(), RewriteBuf->end()) << "\n";
    }

};

