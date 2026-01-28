#ifndef PROGRAMDEPENDENCYGRAPH_H_
#define PROGRAMDEPENDENCYGRAPH_H_
#include "LLVMEssentials.hh"
#include "Graph.hh"
#include "PDGCallGraph.hh"
#include "DataDependencyGraph.hh"
#include "ControlDependencyGraph.hh"

namespace pdg
{
  class ProgramDependencyGraph : public llvm::PassInfoMixin<ProgramDependencyGraph>
  {
    public:

      ProgramDependencyGraph(bool dumping) : dumping(dumping) {}
      llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);

      ProgramGraph *getPDG() { return _PDG; }
      FunctionWrapper *getFuncWrapper(llvm::Function &F) { return _PDG->getFuncWrapperMap()[&F]; }
      std::set<CallWrapper *> getCallWrappers(llvm::CallBase &call_base) { return _PDG->getCallWrapperMap()[&call_base]; }
      void connectGlobalWithUses();
      void connectInTrees(Tree *src_tree, Tree *dst_tree, EdgeType edge_type);
      void connectOutTrees(Tree *src_tree, Tree *dst_tree, EdgeType edge_type);
      void connectCallerAndCallee(CallWrapper &cw, FunctionWrapper &fw);
      void connectIntraprocDependencies(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);
      void connectInterprocDependencies(llvm::Function &F);
      void connectClassNodeWithClassMethods(llvm::Function &F);
      void connectFormalInTreeWithAddrVars(Tree &formal_in_tree);
      void connectFormalOutTreeWithAddrVars(Tree &formal_out_tree);
      void connectActualInTreeWithAddrVars(Tree &actual_in_tree, llvm::CallBase &ci);
      void connectActualOutTreeWithAddrVars(Tree &actual_out_tree, llvm::CallBase &ci);
      void constructClassInheritanceGraph();
      bool canReach(Node &src, Node &dst);
      bool canReach(Node &src, Node &dst, std::set<EdgeType> exclude_edge_types);
      llvm::Module *getModule() { return _module; }
      static bool isRequired() { return true; }

      void dumpGraph(ProgramDependencyGraph *pdg,std::string filename);
    private:
      llvm::Module *_module;
      ProgramGraph *_PDG;

      bool dumping = false; // whether to dump the graph to a file
  };

}

#endif