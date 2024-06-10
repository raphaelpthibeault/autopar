#include "consumers.hpp"

#include <cstdio>
#include <memory>
#include <string>

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

int
main(int argc, char *argv[]) {
  if (argc != 2) {
    llvm::errs() << "Usage: autopar <filename>\n";
    return 1;
  }

  CompilerInstance CI;
  CI.createDiagnostics();

  LangOptions &LO = CI.getLangOpts();
  LO.CPlusPlus = 1;

  auto TO = std::make_shared<TargetOptions>();
  TO->Triple = llvm::sys::getDefaultTargetTriple();
  TargetInfo *TI =
      TargetInfo::CreateTargetInfo(CI.getDiagnostics(), TO);
  CI.setTarget(TI);

  CI.createFileManager();
  FileManager &FileMgr = CI.getFileManager();
  CI.createSourceManager(FileMgr);
  SourceManager &SourceMgr = CI.getSourceManager();
  CI.createPreprocessor(TU_Module);
  CI.createASTContext();

  Rewriter RW;
  RW.setSourceMgr(SourceMgr, CI.getLangOpts());

  const FileEntry *FileIn = FileMgr.getFile(argv[1]).get();
  SourceMgr.setMainFileID(
      SourceMgr.createFileID(FileIn, SourceLocation(), SrcMgr::C_User));
  CI.getDiagnosticClient().BeginSourceFile(
      CI.getLangOpts(), &CI.getPreprocessor());

  TaskCreationASTConsumer C(RW, CI.getASTContext());

  ParseAST(CI.getPreprocessor(), &C,
           CI.getASTContext());

  const RewriteBuffer *RewriteBuf = RW.getRewriteBufferFor(SourceMgr.getMainFileID());

  llvm::outs() << "'" << std::string(RewriteBuf->begin(), RewriteBuf->end()) << "'\n";

  return 0;
}
