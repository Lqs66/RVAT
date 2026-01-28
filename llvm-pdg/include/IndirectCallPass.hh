/**
 * 
 * @description: This Pass is used to get all the indirect callsites in the program.
 * @date 2024-01-10 10:18
 */
#ifndef VIRTUALCALLPASS_H_
#define VIRTUALCALLPASS_H_

# include "PDGCallGraph.hh"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/PassManager.h"

namespace pdg
{
    class IndirectCallPass : public llvm::PassInfoMixin<IndirectCallPass>
    {
    public:
        IndirectCallPass() {}
        llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
        static bool isRequired() { return true; }
        // std::map<int, std::set<std::string>> incallsMap;
    };


} // namespace pdg

#endif