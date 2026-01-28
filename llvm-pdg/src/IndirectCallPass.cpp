#include "IndirectCallPass.hh"

// 

using namespace llvm;

PreservedAnalyses pdg::IndirectCallPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM){
    PDGCallGraph &call_g = PDGCallGraph::getInstance();
    // call_g.setIncallsMap(std::move(incallsMap));
    if (!call_g.isBuild())
      call_g.build(M);
    // outs() << "Virtual Call Pass run" << "\n";
    return PreservedAnalyses::all(); 
}
