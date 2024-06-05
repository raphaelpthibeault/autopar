#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <llvm-14/llvm/Support/CommandLine.h>
#include <llvm-14/llvm/Support/Error.h>
#include <llvm-14/llvm/Support/raw_ostream.h>

static llvm::cl::OptionCategory ToolCategory("autopar options");

int main(int argc, const char** argv) {
    llvm::Expected<clang::tooling::CommonOptionsParser> ExpectedParser = clang::tooling::CommonOptionsParser::create(argc, argv, ToolCategory);

    if (!ExpectedParser) {
        llvm::errs() << "Error: " << llvm::toString(ExpectedParser.takeError()) << "\n";
        return 1;
    }

    clang::tooling::CommonOptionsParser &OptionsParser =  ExpectedParser.get();
    clang::tooling::ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    return 0;
}
