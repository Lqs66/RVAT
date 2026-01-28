#include "PDGCallGraph.hh"

using namespace llvm;

/**
 * Build call graph for a module
*/
void pdg::PDGCallGraph::build(Module &M)
{
  if (!hasTargetsMD(M))
    createInCallCandsMapBySVF(M);
  else
    createIncallCandsMapByTargetsMD(M);
  for (auto &F : M)
  {
    
    // create node for each function.
    // skip declaration and empty function.

    if (F.isDeclaration() || F.empty())
      continue;
    Node* n = new Node(F, GraphNodeType::FUNC, F.getSubprogram());
    _val_node_map.insert(std::make_pair(&F, n));
    _funcNodeMap.insert(std::make_pair(F.getName().str(), n));
    addNode(*n);
  }

  // connect nodes
  for (auto &F : M)
  {
    if (F.isDeclaration() || F.empty())
      continue;
    
    auto caller_node = getNode(F);

    // Iterate through all instructions in function
    for (auto inst_i = inst_begin(F); inst_i != inst_end(F); inst_i++)
    {

      // Handle call instructions. 
      // should use CallBase instead of CallInst, otherwise you won't be able to recognize the function called by the invoke instruction.
      if (CallBase *ci = dyn_cast<CallBase>(&*inst_i))
      {

        // get called function
        auto called_func = pdgutils::getCalledFunc(*ci);
        // direct calls
        if (called_func != nullptr)
        {
          auto callee_node = getNode(*called_func);
          if (callee_node != nullptr)
            caller_node->addNeighbor(*callee_node, EdgeType::CONTROLDEP_CALLINV); // add call edge from caller_node to callee_node
        }
        else
        {
          // indirect calls
          auto ind_call_candidates = getIndirectCallCandidates(*ci);
          for (auto ind_call_can : ind_call_candidates)
          {
            Node* callee_node = getNode(*ind_call_can);
            if (callee_node != nullptr)
              caller_node->addNeighbor(*callee_node, EdgeType::IND_CALL);
          }
        }
      }
    }
  }
  //dump();
  _is_build = true;
}

/**
 * @author lqs66
 * Use SVF's CHA and Andersen to create indirect call candidates map.
*/
void pdg::PDGCallGraph::createInCallCandsMapBySVF(llvm::Module &M)
{
  llvm::outs() << "Start to create indirect call candidates map by SVF...\n";
  // use SVF to parse module
  SVF::SVFModule* svfModule = SVF::LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(M);
  /// Build Program Assignment Graph (PAG)
  SVF::SVFIRBuilder builder(svfModule);
  SVF::PAG* pag = builder.build();
  /// Create Andersen's pointer analysis. The analyze() function has been called at creation time. 
  SVF::Andersen* ander = SVF::AndersenWaveDiff::createAndersenWaveDiff(pag);
  //get pta call graph
  SVF::PTACallGraph* pta = ander->getPTACallGraph();

  // use CHA to get all virtual callsites and candidates
  SVF::PointerAnalysis::CallEdgeMap ander_result = pta->getIndCallMap();

  // iterate through all indirect callsites
  for(auto it = pag->getIndirectCallsites().begin(); it != pag->getIndirectCallsites().end(); it++)
  {
      const SVF::CallICFGNode* ind_call_node = it->first;

      const SVF::SVFInstruction *ind_callsite = ind_call_node->getCallSite();
      const llvm::Value* llvmInd_callsite = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(ind_callsite);
      CandidateSet candidates;
      // andersen failed, use CHA to get candidates
      if(ander_result.find(ind_call_node) == ander_result.end())
      {
        SVF::PointerAnalysis::FunctionSet emptySet;
        SVF::PointerAnalysis::FunctionSet& targets = emptySet;
        ander->getVFnsFromCHA(ind_call_node, targets);

        // check if targets is empty
        if(targets.empty())
          continue;

        for(auto it2 = targets.begin(); it2 != targets.end(); it2++)
        {
          const SVF::SVFFunction *target = *it2;
          const llvm::Value* tmp = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(target);
          llvm::Function* callee = const_cast<llvm::Function*>(dyn_cast<const Function>(tmp));
          candidates.insert(callee);
        }   
      }else
      {
        // andersen success, use andersen to get candidates
        for(auto it2 = ander_result[ind_call_node].begin(); it2 != ander_result[ind_call_node].end(); it2++)
        {
          const SVF::SVFFunction *target = *it2;
          const llvm::Value* tmp = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(target);
          llvm::Function* callee = const_cast<llvm::Function*>(dyn_cast<const Function>(tmp));
          candidates.insert(callee);
        }
      }

      assert(dyn_cast<const CallBase >(llvmInd_callsite) && "indirect callsite is not CallBase type!\n");

      if(const CallBase * ci = dyn_cast<const CallBase >(llvmInd_callsite))
      {
        _indrect_call_cands_map.insert(std::make_pair(ci, candidates));
      }else
      {
        errs() << ind_callsite << " --indirect callsite is not CallBase type!\n";
      }
  }

  // clean up memory
  SVF::AndersenWaveDiff::releaseAndersenWaveDiff();
  SVF::SVFIR::releaseSVFIR();
}

// @author lqs66
bool pdg::PDGCallGraph::hasTargetsMD(llvm::Module &M){
  for (auto &F : M){
    for (auto &BB : F){
      for (auto &I : BB){
        if (llvm::CallBase *cb = llvm::dyn_cast<llvm::CallBase>(&I)){
          if (cb->isIndirectCall()){
            if (llvm::MDNode *MD = cb->getMetadata("targets")){
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

// @author lqs66
void pdg::PDGCallGraph::createIncallCandsMapByTargetsMD(llvm::Module &M)
{
  llvm::outs() << "Start to create indirect call candidates map by !targets...\n";
  for (auto &F : M){
    for (auto &BB : F){
      for (auto &I : BB){
        if (llvm::CallBase *cb = llvm::dyn_cast<llvm::CallBase>(&I)){
          if (cb->isIndirectCall()){
            if (llvm::MDNode *MD = cb->getMetadata("targets")){
              CandidateSet candidates;
              for (unsigned i = 0, e = MD->getNumOperands(); i < e; ++i){
                if (auto *MDS = llvm::dyn_cast<llvm::MDString>(MD->getOperand(i))){
                  std::string funcName = MDS->getString().str();
                  if (llvm::Function *callee = M.getFunction(funcName)){
                    candidates.insert(callee);
                  }
                }
              }
              if (!candidates.empty()){
                _indrect_call_cands_map.insert(std::make_pair(cb, candidates));
              }
            }
          }
        }
      }
    }
  }
}

// base callsite to get candidates by using indirect call candidates map
std::set<Function *> pdg::PDGCallGraph::getIndirectCallCandidates(CallBase &ci)
{
  if(_indrect_call_cands_map.find(&ci) == _indrect_call_cands_map.end())
  {
    // assert(_indrect_call_cands_map.empty() && "indirect call candidates map is empty!\n");
    //errs() << "cannot find indirect call candidates for callsite: " << ci << "\n";
    return {};
  }
  return _indrect_call_cands_map[&ci];
}


bool pdg::PDGCallGraph::canReach(Node &src, Node &sink)
{
    std::queue<Node*> node_queue;
    std::unordered_set<Node *> seen_node;
    node_queue.push(&src);
    while (!node_queue.empty())
    {
      Node* n = node_queue.front();
      node_queue.pop();
      if (n == &sink)
        return true;
      if (seen_node.find(n) != seen_node.end())
        continue;
      seen_node.insert(n);

      for (auto out_neighbor : n->getOutNeighbors())
      {
        node_queue.push(out_neighbor);
      }
    }
    return false;
}

/**
 * dump pdg call graph, used to debug.
*/
void pdg::PDGCallGraph::dump()
{
  std::string output_dot = "digraph callgraph {\n";

  for (auto pair : _val_node_map)
  {
    if (Function *f = dyn_cast<Function>(pair.first))
    {
      output_dot += f->getName().str() + ";\n";
      // errs() << f->getName() << ": \n";
      for (auto out_node : pair.second->getOutNeighbors())
      {
        if (Function *callee = dyn_cast<Function>(out_node->getValue()))
        {
          output_dot += f->getName().str() + " -> " + callee->getName().str() + ";\n";
          // errs() << "\t\t" << callee->getName() << "\n";
        }
      }
    }
  }
  output_dot += "}\n";

  std::ofstream outfile("pdg_callgraph.dot", std::ios::out);
    if (outfile.is_open()) {
        outfile << output_dot;
        outfile.close();
    } else {
        errs() << "write file failed\n";
    }
    outs() << "dump pdg call graph done!\n";
}

void pdg::PDGCallGraph::printPaths(Node &src, Node &sink)
{
  auto pathes = computePaths(src, sink);
  unsigned count = 1;
  for (auto path : pathes)
  {
    errs() << "************* Printing Pathes **************\n";
    errs() << "path len: " << path.size() << "\n";
    for (auto iter = path.begin(); iter != path.end(); iter++)
    {
      errs() << (*iter)->getName();
      if (std::next(iter, 1) != path.end())
        errs() << " -> ";
      else 
        errs() << "\n\b";
    }
    errs() << "********************************************\n";
    count ++;
  }
}

pdg::PDGCallGraph::PathVecs pdg::PDGCallGraph::computePaths(Node &src, Node &sink)
{
  PathVecs ret;
  std::unordered_set<Function *> visited_funcs;
  bool found_path = false;
  computePathsHelper(ret, src, sink, {}, visited_funcs, found_path); // just find one path
  return ret;
}

void pdg::PDGCallGraph::computePathsHelper(PathVecs &path_vecs, Node &src, Node &sink, std::vector<llvm::Function *> cur_path, std::unordered_set<llvm::Function *> visited_funcs, bool &found_path)
{
  if (found_path)
    return;
  assert(isa<Function>(src.getValue()) && "cannot process non function node (compute path, src)\n");
  assert(isa<Function>(sink.getValue()) && "cannot process non function node (compute path, sink)\n");
  Function* src_func = cast<Function>(src.getValue());
  Function* sink_func = cast<Function>(sink.getValue());
  if (visited_funcs.find(src_func) != visited_funcs.end())
    return;
  visited_funcs.insert(src_func);
  cur_path.push_back(src_func);
  if (src_func == sink_func)
  {
    path_vecs.push_back(cur_path);
    found_path = true;
    return;
  }

  for (auto out_neighbor : src.getOutNeighbors())
  {
    computePathsHelper(path_vecs, *out_neighbor, sink, cur_path, visited_funcs, found_path);
  }
}

/**
 * @author lqs66
 * get function node by function name
 */
pdg::Node* pdg::PDGCallGraph::getFuncNodeByName(std::string funcName){
  return _funcNodeMap[funcName];
}

/**
 * Get a subgraph of calls consisting of the slice functions
 */
std::set<pdg::Node*> pdg::PDGCallGraph::getSubCallGraph(std::string &start_node_name, std::set<std::string> &slice_funcs)
{
  pdg::Node* start_node = getFuncNodeByName(start_node_name);
  if (!start_node) {
    std::cout << "Warning: start node " << start_node_name << " not found in call graph." << std::endl;
    return {};
  }
  
  // BFS - GEt all nodes reachable from the start node
  std::set<pdg::Node*> start_node_reachable;
  std::queue<pdg::Node*> node_queue;
  node_queue.push(start_node);
  while (!node_queue.empty())
  {
    pdg::Node* current_node = node_queue.front();
    node_queue.pop();
    if (start_node_reachable.count(current_node)) // avoid circle
      continue;
    start_node_reachable.insert(current_node);
    for (auto out_edge : current_node->getOutEdgeSet())
    {
      node_queue.push(out_edge->getDstNode());
    }
  }

  if (slice_funcs.empty()) {
    return start_node_reachable;
  }
  
  std::set<pdg::Node*> slice_func_nodes;
  for (auto func_name : slice_funcs)
  {
    pdg::Node* node = getFuncNodeByName(func_name);
    if (node) {
      slice_func_nodes.insert(node);
    }
  }
  
  std::set<pdg::Node*> new_slice_func_nodes;
  for (auto node : slice_func_nodes)
  {
    if (start_node_reachable.count(node))
      new_slice_func_nodes.insert(node);
  }
  return new_slice_func_nodes;
}