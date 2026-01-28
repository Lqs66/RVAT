#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/ADT/SmallVector.h"

#include <filesystem>
#include <iostream>
#include <queue>
#include <map>
#include <unordered_set>
#include <sstream>
#include <utility> 
#include <fstream>

#include "macro.hh"

using namespace llvm;

class GlobalVarsInfo : public PassInfoMixin<GlobalVarsInfo> {
public:

    GlobalVarsInfo(std::set<std::string> targetGlobalVals, std::string csvFilePath) : targetGlobalVals(targetGlobalVals), csvFilePath(csvFilePath) {};
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
private:
    void dumpGlobalVarInfo(Module &M, GlobalVariable *GV, std::ofstream &csvFile);   
    llvm::DICompositeType* findCompleteDefinition(llvm::Module &M, llvm::DICompositeType *FwdDeclDIType);
    void printTypeMembers(llvm::DIType *DIType, llvm::Module &M, std::ofstream &csvFile, std::string currentPath, unsigned BaseOffsetBytes = 0);

    std::set<std::string> targetGlobalVals;
    std::string csvFilePath;
};

void runglobalVarsInfo(Module &M, std::string output_dir = "./");