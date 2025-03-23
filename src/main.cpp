#include <consumers.hpp>
#include <frontend_actions.hpp>

#include <llvm-18/llvm/Support/CommandLine.h>

#include <clang/Basic/Diagnostic.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;

int
main(int argc, const char *argv[]) {
    if (argc != 2) {
        llvm::errs() << "Usage: autopar <filename>\n";
        return 1;
    }

    auto ExpectedParser = tooling::CommonOptionsParser::create(argc, argv, llvm::cl::getGeneralCategory());
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }

    tooling::CommonOptionsParser &OptionsParser = ExpectedParser.get();
    tooling::ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    return Tool.run(tooling::newFrontendActionFactory<TaskCreationFrontendAction>().get());
}

