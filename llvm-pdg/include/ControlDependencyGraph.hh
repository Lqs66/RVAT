#ifndef CONTROLDEPENDENCYGRAPH_H_
#define CONTROLDEPENDENCYGRAPH_H_
#include "Graph.hh"
#include "llvm/Analysis/PostDominators.h"
#include "IndirectCallPass.hh"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
namespace pdg
{

  class ControlDependencyGraph : public llvm::PassInfoMixin<ControlDependencyGraph>
  {
  public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);
    void addControlDepFromNodeToBB(Node &n, llvm::BasicBlock &bb, EdgeType edge_type);
    void addControlDepFromEntryNodeToInsts(llvm::Function &F);
    void addControlDepFromDominatedBlockToDominator(llvm::Function &F);
    void addControlExceptionEdges(llvm::Function &F);
    static bool isRequired() { return true; }
  private:
    llvm::PostDominatorTree *_PDT;
  };
} // namespace pdg


#endif