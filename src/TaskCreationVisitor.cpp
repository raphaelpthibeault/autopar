#include <visitors.hpp>


/* PUBLICS */


bool TaskCreationVisitor::VisitDeclStmt(DeclStmt *DeclStat) {
    if (!isFromMainFile(DeclStat->getBeginLoc())) return true;
    int nbCallExprs = countCallExprs(DeclStat);

    if (nbCallExprs > 0) {
        ignoreCalls += nbCallExprs;
    } else {
        return true;
    }

    for (const auto *Decl : DeclStat->decls()) {
        if (const auto *VarDecl = llvm::dyn_cast<clang::VarDecl>(Decl)) {
            if (VarDecl->hasInit() && llvm::isa<CallExpr>(VarDecl->getInit())) {
                const auto *FCall = llvm::cast<clang::CallExpr>(VarDecl->getInit());
                const FunctionDecl *CalledFunc = FCall->getDirectCallee();

                if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && CalledFunc->getIdentifier()) {
                    DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW); /* MUST BE BEFORE REWRITING */

                    std::string varName = VarDecl->getNameAsString();
                    std::string varType = VarDecl->getType().getAsString();
                    varType = varType.find("const") != std::string::npos ? varType.substr(6) : varType;

                    std::string initializer = varName + " = " + RW.getRewrittenText(VarDecl->getInit()->getSourceRange()) + ";";
                    RW.ReplaceText(VarDecl->getSourceRange(), varType.append(" ").append(varName));


                    depInfo.write.insert(varName);
                    std::string depClause = constructDependClause(depInfo);

                    if (shouldAddTaskWait(depInfo)) {
                        RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp taskwait\n", true, true);
                    }

                    RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), 
                        AUTOPAR_PRE_TASK + 
                        std::string("\n#pragma omp task ").append(depClause).append(" ").append(AUTOPAR_TASK_CLAUSE)
                            .append("\n{\n")
                            .append("if (AUTOPAR_createtaskdepth || AUTOPAR_createtasknbr) {\n\tAUTOPAR_nbdepth=AUTOPAR_lnbdepth+1;\n}\n\n")
                            .append(initializer)
                            .append("\n\nif (AUTOPAR_createtasknbr) {\n\t#pragma omp atomic\n\tAUTOPAR_nbtask--;\n}\n")
                            .append("}\n"),
                        true, true);

                    addTask(depInfo);
                }

            } else if (VarDecl->hasInit() && nbCallExprs == 1) {
                if (auto *Expr = VarDecl->getInit()->getExprStmt()) {
                    if (const CallExpr *FCall = findCallExpr(Expr)) {
                        const FunctionDecl *CalledFunc = FCall->getDirectCallee();

                        if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && CalledFunc->getIdentifier()) {
                            DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW); /* MUST BE BEFORE REWRITING */

                            std::string varName = VarDecl->getNameAsString();
                            std::string varType = VarDecl->getType().getAsString();
                            varType = varType.find("const") != std::string::npos ? varType.substr(6) : varType;

                            std::string initializer = varName + " = " + RW.getRewrittenText(VarDecl->getInit()->getSourceRange()) + ";";
                            RW.ReplaceText(VarDecl->getSourceRange(), varType.append(" ").append(varName));

                            depInfo.write.insert(varName);
                            std::string depClause = constructDependClause(depInfo);

                            if (shouldAddTaskWait(depInfo)) {
                                RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), "\n#pragma omp taskwait\n", true, true);
                            }

                            RW.InsertText(DeclStat->getEndLoc().getLocWithOffset(1), 
                                AUTOPAR_PRE_TASK + 
                                std::string("\n#pragma omp task ").append(depClause).append(" ").append(AUTOPAR_TASK_CLAUSE)
                                    .append("\n{\n")
                                    .append("if (AUTOPAR_createtaskdepth || AUTOPAR_createtasknbr) {\n\tAUTOPAR_nbdepth=AUTOPAR_lnbdepth+1;\n}\n\n")
                                    .append(initializer)
                                    .append("\n\nif (AUTOPAR_createtasknbr) {\n\t#pragma omp atomic\n\tAUTOPAR_nbtask--;\n}\n")
                                    .append("}\n"),
                                true, true);

                            addTask(depInfo);
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool TaskCreationVisitor::VisitCallExpr(CallExpr *FCall) {
    if (!isFromMainFile(FCall->getBeginLoc())) return true;
    const FunctionDecl *CalledFunc = FCall->getDirectCallee();
    if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && CalledFunc->getIdentifier() && ignoreCalls == 0) {
        DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW);
        std::string depClause = constructDependClause(depInfo);

        if (shouldAddTaskWait(depInfo)) {
            RW.InsertText(FCall->getBeginLoc(), "#pragma omp taskwait\n\n", true, true);
        }

        RW.InsertText(FCall->getBeginLoc(), AUTOPAR_PRE_TASK + "\n#pragma omp task " + depClause + " " + AUTOPAR_TASK_CLAUSE + "\n{\n", true, true);
        RW.InsertText(FCall->getBeginLoc(), "if (AUTOPAR_createtaskdepth || AUTOPAR_createtasknbr) {\n\tAUTOPAR_nbdepth=AUTOPAR_lnbdepth+1;\n}\n\n", true, true);
        RW.InsertText(FCall->getEndLoc().getLocWithOffset(2), "\n\nif (AUTOPAR_createtasknbr) {\n\t#pragma omp atomic\n\tAUTOPAR_nbtask--;\n}\n", true, true);
        RW.InsertText(FCall->getEndLoc().getLocWithOffset(2), "}\n", true, true);

        addTask(depInfo);
    }

    if (ignoreCalls > 0)
        ignoreCalls--;

    return true;
}

bool TaskCreationVisitor::VisitFunctionDecl(FunctionDecl *f) {
    if (!isFromMainFile(f->getLocation())) return true;
    if (!f->isDefined()) return true;

    Stmt *FuncBody = f->getBody();
    std::string FuncName = f->getNameInfo().getName().getAsString();
    std::string returnTypeStr = f->getReturnType().getAsString();
    bool taskCreated = checkTaskCreation(FuncBody);
    currentFunction = f;

    if (!taskCreated) {
        return true;
    }

    if (returnTypeStr != "void") {
        RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n" + returnTypeStr + " AUTOPAR_res;\n", true, true);
    }

    if (FuncName == "main") {
        RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp parallel\n#pragma omp master", true, true);
    }

    addFunction(FuncName);
    llvm::outs() << "Parallelizing " << FuncName << "\n";
    RW.InsertText(FuncBody->getBeginLoc().getLocWithOffset(1), "\n#pragma omp taskgroup\n{\n\n" + AUTOPAR_TASK_LIMITER_CODE_TASKGROUP + "\n", true, true);

    std::string endLabel = "\nAUTOPAR_endtaskgrouplabel_" + FuncName + ": ;\n}\n";
    if (returnTypeStr != "void") {
        endLabel += "return AUTOPAR_res;\n";
    }

    RW.InsertText(FuncBody->getEndLoc(), endLabel, true, true);

    return true;
}

bool TaskCreationVisitor::VisitExpr(Expr *e) {
    if (!isFromMainFile(e->getExprLoc())) return true;
    if (!(isa<BinaryOperator>(e) || isa<UnaryOperator>(e))) {
        return true;
    }

    const Stmt *curr = e;
    int nbCallExprs = countCallExprs(curr);

    if (nbCallExprs > 0) {
        ignoreCalls += nbCallExprs;

        for (auto *Child : curr->children()) {
            if (Child) {
                if (auto *FCall = llvm::dyn_cast<CallExpr>(Child)) {
                    const FunctionDecl *CalledFunc = FCall->getDirectCallee();
                    if (CalledFunc && CalledFunc->isDefined() && !CalledFunc->isStdNamespace() && CalledFunc->getIdentifier()) {
                        DependInfo depInfo = getFCallDependencies(CalledFunc, FCall, RW);
                        std::string depClause = constructDependClause(depInfo);

                        if (shouldAddTaskWait(depInfo)) {
                            RW.InsertText(e->getBeginLoc(), "#pragma omp taskwait\n\n", true, true);
                        }

                        RW.InsertText(e->getBeginLoc(), AUTOPAR_PRE_TASK + std::string("\n#pragma omp task ").append(depClause).append(" ").append(AUTOPAR_TASK_CLAUSE).append("\n{\n"), true, true);
                        RW.InsertText(e->getBeginLoc(), "if (AUTOPAR_createtaskdepth || AUTOPAR_createtasknbr) {\n\tAUTOPAR_nbdepth=AUTOPAR_lnbdepth+1;\n}\n\n", true, true);
                        RW.InsertText(e->getEndLoc().getLocWithOffset(2), "\n\nif (AUTOPAR_createtasknbr) {\n\t#pragma omp atomic\n\tAUTOPAR_nbtask--;\n}\n", true, true);
                        RW.InsertText(e->getEndLoc().getLocWithOffset(2), "}\n", true, true);

                        addTask(depInfo);
                    }
                }
            }
        }
    } else {
        Vars vars = extractVariables(e, RW);

        if (shouldAddTaskWait(vars)) {
            if (auto parent = getParentIfLoop(e, AC)) {
                RW.InsertText(parent->getBeginLoc(), "\n#pragma omp taskwait\n", true, true);
            } else {
                RW.InsertText(e->getBeginLoc(), "\n#pragma omp taskwait\n", true, true);
            }
        }
    }

    return true;
}

bool TaskCreationVisitor::VisitReturnStmt(ReturnStmt *ret) {
    if (!isFromMainFile(ret->getReturnLoc())) return true;

    if (!currentFunction) {
        llvm::errs() << "Error: VisitReturnStmt() no current function\n";
    }

    if (!checkTaskCreation(currentFunction->getBody())) return true;

    std::string assignTxt = "\n";

    Expr *retValue = ret->getRetValue();
    std::string gotoTxt = "goto AUTOPAR_endtaskgrouplabel_" + currentFunction->getNameAsString();
    if (retValue) {
        gotoTxt += ";";
        assignTxt += "AUTOPAR_res = " + RW.getRewrittenText(retValue->getSourceRange()) + ";\n";
    }

    SourceLocation begin = ret->getBeginLoc();
    SourceLocation end = ret->getEndLoc().getLocWithOffset(1);

    auto lineText = Lexer::getIndentationForLine(begin, AC.getSourceManager());
    auto indentation = lineText.substr(0, lineText.find_first_not_of(" \t"));

    SourceRange range(begin, end);
    RW.ReplaceText(range, indentation.str() + assignTxt + indentation.str() + gotoTxt);

    return true;
}


/* PRIVATES */


void TaskCreationVisitor::addFunction(std::string funcName) {
    taskId = 0;
    awaited.clear();
    Function curr;
    curr.name = std::move(funcName);
    curr.id = funcId++;
    functions.push_back(curr);
}

void TaskCreationVisitor::addTask(DependInfo depInfo) {
    Task curr;

    curr.depInfo = std::move(depInfo);
    curr.id = taskId++;
    functions.back().tasks.push_back(curr);
}

bool TaskCreationVisitor::shouldAddTaskWait(const DependInfo& depInfo) {
    if (functions.size() == 0) return false;
    bool res = false;
    for (const auto& task : functions.back().tasks) {
        for (const auto& var : depInfo.read) {
            if (task.depInfo.write.count(var) && !awaited.count(var)) {
                awaited.insert(var);
                res = true;
            }
        }
        for (const auto& var : depInfo.write) {
            if (task.depInfo.write.count(var) && !awaited.count(var)) {
                awaited.insert(var);
                res = true;
            }
        }
    }

    return res;
}

bool TaskCreationVisitor::shouldAddTaskWait(const Vars& vars) {
    if (functions.size() == 0) return false;
    bool res = false;
    for (const auto& task : functions.back().tasks) {
        for (const auto& var : vars.vars) {
            if (task.depInfo.write.count(var) && !awaited.count(var)) {
                awaited.insert(var);
                res = true;
            }
        }
        for (const auto& var : vars.idxs) {
            if (task.depInfo.write.count(var) && !awaited.count(var)) {
                awaited.insert(var);
                res = true;
            }
        }
    }

    return res;
}

