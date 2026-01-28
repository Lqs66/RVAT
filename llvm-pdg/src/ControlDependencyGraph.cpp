#include "ControlDependencyGraph.hh"
#include "PDGEnums.hh"

using namespace llvm;

llvm::PreservedAnalyses pdg::ControlDependencyGraph::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM){
  _PDT = &FAM.getResult<PostDominatorTreeAnalysis>(F);
  addControlDepFromEntryNodeToInsts(F);
  addControlDepFromDominatedBlockToDominator(F);
  addControlExceptionEdges(F);
  return PreservedAnalyses::all();
}

void pdg::ControlDependencyGraph::addControlExceptionEdges(Function &F) {
  ProgramGraph &prog_g = ProgramGraph::getInstance();
  for (auto instI = inst_begin(F); instI != inst_end(F); ++instI) {
    if (InvokeInst *ii = dyn_cast<InvokeInst>(&*instI)) {
      auto invoke_inst_node = prog_g.getNode(*ii);
      // get the normal block 
      auto normal_BB = ii->getNormalDest();
      addControlDepFromNodeToBB(*invoke_inst_node, *normal_BB, EdgeType::CONTROLDEP_INVOKE_NORMAL);
      // exception block
      auto exception_BB = ii->getUnwindDest();
      addControlDepFromNodeToBB(*invoke_inst_node, *exception_BB, EdgeType::CONTROLDEP_INVOKE_EXCEPT);
    }
  }
}

void pdg::ControlDependencyGraph::addControlDepFromNodeToBB(Node &n, BasicBlock &BB, EdgeType edge_type)
{
  ProgramGraph &g = ProgramGraph::getInstance();
  for (auto &inst : BB)
  {
    Node* inst_node = g.getNode(inst);
    // TODO: a special case when gep is used as a operand in load. Fix later
    if (inst_node != nullptr)
      n.addNeighbor(*inst_node, edge_type);
    // assert(inst_node != nullptr && "cannot find node for inst\n");
  }
}

void pdg::ControlDependencyGraph::addControlDepFromEntryNodeToInsts(Function &F)
{
  ProgramGraph &g = ProgramGraph::getInstance();
  FunctionWrapper* func_w = g.getFuncWrapperMap()[&F];
  if (!func_w)
    return;
  for (auto &BB : F)
  {
    addControlDepFromNodeToBB(*func_w->getEntryNode(), BB, EdgeType::CONTROLDEP_ENTRY);
  }
}

void pdg::ControlDependencyGraph::addControlDepFromDominatedBlockToDominator(Function &F)
{
  ProgramGraph &g = ProgramGraph::getInstance();
  for (auto &BB : F)
  {
    for (auto succ_iter = succ_begin(&BB); succ_iter != succ_end(&BB); succ_iter++)
    {
      BasicBlock *succ_bb = *succ_iter;
      if (&BB == &*succ_bb || !_PDT->dominates(&*succ_bb, &BB))
      {
        // get terminator and connect with the dependent block
        Instruction *terminator = BB.getTerminator();
        if (BranchInst *bi = dyn_cast<BranchInst>(terminator))
        {
          if (!bi->isConditional() || !bi->getCondition())
            break;
          // Node *cond_node = g.getNode(*bi->getCondition());
          // if (!cond_node)
          //   break;
          Node *branch_node = g.getNode(*bi);
          if (branch_node == nullptr)
            break;
          BasicBlock *nearestCommonDominator = _PDT->findNearestCommonDominator(&BB, succ_bb);
          if (nearestCommonDominator == &BB)
            addControlDepFromNodeToBB(*branch_node, *succ_bb, EdgeType::CONTROLDEP_BR);

          for (auto *cur = _PDT->getNode(&*succ_bb); cur != _PDT->getNode(nearestCommonDominator); cur = cur->getIDom())
          {
            addControlDepFromNodeToBB(*branch_node, *cur->getBlock(), EdgeType::CONTROLDEP_BR);
          }
        }
      }
    }
  }
}

// void pdg::ControlDependencyGraph::getAnalysisUsage(AnalysisUsage &AU) const
// {
//   AU.addRequired<PostDominatorTreeWrapperPass>();
//   AU.addRequired<VirtualCallPass>();
//   AU.setPreservesAll();
// }

// static RegisterPass<pdg::ControlDependencyGraph>
//     CDG("cdg", "Control Dependency Graph Construction", false, true);
