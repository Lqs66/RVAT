#include "inCallUtil.hh"
#include "svfAndersen.hh"

void svfAndersen::runSVFAndersen(llvm::Module& M){
  SVF::SVFModule* svfModule = SVF::LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(M);
  /// Build Program Assignment Graph (PAG)
  SVF::SVFIRBuilder builder(svfModule);
  SVF::SVFIR* pag = builder.build();

  /// Create Andersen's pointer analysis. The analyze() function has been called at creation time. 
  SVF::Andersen* ander = SVF::AndersenWaveDiff::createAndersenWaveDiff(pag);

  //get pta call graph
  SVF::PTACallGraph* pta = ander->getPTACallGraph();

  SVF::PointerAnalysis::CallEdgeMap ander_result = pta->getIndCallMap();

  for (auto it = ander_result.begin(); it != ander_result.end(); it++){
    const SVF::CallICFGNode* ind_call_node = it->first;
    const SVF::SVFInstruction *ind_callsite = ind_call_node->getCallSite();
    const llvm::Value* llvmInd_callsite = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(ind_callsite);
    llvm::CallBase* incallsite = const_cast<llvm::CallBase*>(llvm::dyn_cast<const llvm::CallBase>(llvmInd_callsite));
    // assert(llvm::dyn_cast<const llvm::CallBase >(llvmInd_callsite) && "indirect callsite is not CallBase type!\n");
    int ID = -1;
    llvm::Metadata* incallID = incallsite->getMetadata("inCallID");
    if (incallID) {
      auto *md = llvm::dyn_cast<llvm::MDNode>(incallID);
      ID = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(md->getOperand(0))->getValue())->getZExtValue();
    }
    if (ID == -1 || TypeMDIncallEdges.find(ID) != TypeMDIncallEdges.end()){
      continue;
    }
    std::set<llvm::Function *> svf_candidates;
    for (auto it2 = ander_result[ind_call_node].begin(); it2 != ander_result[ind_call_node].end(); it2++){
      const SVF::SVFFunction *target = *it2;
      const llvm::Value* tmp = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(target);
      llvm::Function* callee = const_cast<llvm::Function*>(llvm::dyn_cast<const llvm::Function>(tmp));
      svf_candidates.insert(callee);
    }
    SVFIncallEdges.emplace(ID, incall::IndirectCallEdge(*incallsite, svf_candidates));
  }
}