#include "ProgramDependencyGraph.hh"
#include "GraphWriter.hh"
#include <vector>

using namespace llvm;

PreservedAnalyses pdg::ProgramDependencyGraph::run(Module &M, ModuleAnalysisManager &MAM){
  _module = &M;
  PDGCallGraph &call_g = PDGCallGraph::getInstance();
  _PDG = &ProgramGraph::getInstance();  
  if (!_PDG->isBuild())
  {
    _PDG->build(M);
    _PDG->bindDITypeToNodes(M);
  }
  
  unsigned func_size = 0;
  // construct class inheritance relations, adding class_INH edges
  constructClassInheritanceGraph();
  connectGlobalWithUses();

  for (auto &F : M)
  {
    if (F.isDeclaration())
      continue;
    if (!_PDG->hasFuncWrapper(F))
      continue;
    auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    connectIntraprocDependencies(F,FAM);
    connectInterprocDependencies(F);
    connectClassNodeWithClassMethods(F);
    func_size++;
  }
  
  errs() << "func size: " << func_size << "\n";
  errs() << "Finsh adding dependencies" << "\n";
  errs() << "PDG Value-Node Map size: " << _PDG->numNode() << "\n";
  errs() << "PDG Node Set size: " << _PDG->NodeSetSize() << "\n";

  if (dumping)
    dumpGraph(this,"pdggraph.dot");
  return PreservedAnalyses::none();
}

void pdg::ProgramDependencyGraph::dumpGraph(pdg::ProgramDependencyGraph *pdg,std::string filename){
    outs() << "Dumping PDG to " << filename << "...\n";
    std::error_code EC;
    llvm::raw_fd_ostream File(filename, EC, llvm::sys::fs::OF_Text);

    if (EC) {
        llvm::errs() << "Error: " << EC.message() << "\n";
        return;
    }

    llvm::WriteGraph(File, pdg);
}


void pdg::ProgramDependencyGraph::connectGlobalWithUses()
{
  for (auto &global_var : _module->getGlobalList())
  {
    Node* n = _PDG->getNode(global_var);
    if (n == nullptr)
      continue;

    for (auto user : global_var.users())
    {
      Node* user_node = _PDG->getNode(*user);
      if (user_node == nullptr)
        continue;
      n->addNeighbor(*user_node, EdgeType::DATA_DEF_USE);
    }
  }
}

void pdg::ProgramDependencyGraph::connectInTrees(Tree* src_tree, Tree* dst_tree, EdgeType edge_type)
{
  if (src_tree->size() != dst_tree->size()) {
    errs() << "inequal tree size: " << src_tree->size() << " - " << dst_tree->size() << "\n";
    return;
  }
  auto src_tree_root_node = src_tree->getRootNode();
  auto dst_tree_root_node = dst_tree->getRootNode();
  std::queue<std::pair<TreeNode*, TreeNode*>> node_pairs_queue;
  node_pairs_queue.push(std::make_pair(src_tree_root_node, dst_tree_root_node));
  while (!node_pairs_queue.empty())
  {
    auto current_node_pair = node_pairs_queue.front();
    node_pairs_queue.pop();
    TreeNode* src = current_node_pair.first;
    TreeNode* dst = current_node_pair.second;
    assert(src->numOfChild() == dst->numOfChild());
    src->addNeighbor(*dst, edge_type);
    auto src_node_children = src->getChildNodes();
    auto dst_node_children = dst->getChildNodes();
    for (int i = 0; i < src->numOfChild(); i++)
    {
      node_pairs_queue.push(std::make_pair(src_node_children[i], dst_node_children[i]));
    }
  }
}

void pdg::ProgramDependencyGraph::connectOutTrees(Tree* src_tree, Tree* dst_tree, EdgeType edge_type)
{
  if (src_tree->size() != dst_tree->size())
    return;
  auto src_tree_root_node = src_tree->getRootNode();
  auto dst_tree_root_node = dst_tree->getRootNode();
  std::queue<std::pair<TreeNode*, TreeNode*>> node_pairs_queue;
  node_pairs_queue.push(std::make_pair(src_tree_root_node, dst_tree_root_node));
  while (!node_pairs_queue.empty())
  {
    auto current_node_pair = node_pairs_queue.front();
    node_pairs_queue.pop();
    TreeNode* src = current_node_pair.first;
    TreeNode* dst = current_node_pair.second;
    assert(src->numOfChild() == dst->numOfChild());
    if (src->hasWriteAccess())
      src->addNeighbor(*dst, edge_type);
    auto src_node_children = src->getChildNodes();
    auto dst_node_children = dst->getChildNodes();
    for (int i = 0; i < src->numOfChild(); i++)
    {
      node_pairs_queue.push(std::make_pair(src_node_children[i], dst_node_children[i]));
    }
  }
}

void pdg::ProgramDependencyGraph::connectCallerAndCallee(CallWrapper &cw, FunctionWrapper &fw)
{
  // step 1: connect call site node with the entry node of function
  auto call_site_node = _PDG->getNode(*cw.getCallInst());
  auto func_entry_node = fw.getEntryNode();

  if (call_site_node == nullptr || func_entry_node == nullptr )
  {
    errs() << "call site node or func entry node is null\n";
    return;
  }
  call_site_node->addNeighbor(*func_entry_node, EdgeType::CONTROLDEP_CALLINV);

  // step 2: connect actual in -> formal in, formal out -> actual out
  auto actual_arg_list = cw.getArgList();
  auto formal_arg_list = fw.getArgList();

  if (actual_arg_list.size() != formal_arg_list.size())
  {
    return;
  }

  // assert(actual_arg_list.size() == formal_arg_list.size() && "cannot connect tree edges due to inequal arg num! (connectCallerandCallee)");
  int num_arg = cw.getArgList().size();
  for (int i = 0; i < num_arg; i++)
  {
    Value *actual_arg = actual_arg_list[i];
    Argument *formal_arg =  formal_arg_list[i];
    // step 2: connect actual in -> formal in
    auto actual_in_tree = cw.getArgActualInTree(*actual_arg);
    auto formal_in_tree = fw.getArgFormalInTree(*formal_arg);
    if (actual_in_tree == nullptr || formal_in_tree == nullptr)
      continue;
    _PDG->addTreeNodesToGraph(*actual_in_tree);
    connectInTrees(actual_in_tree, formal_in_tree, EdgeType::PARAMETER_IN);
    // step 3: connect actual out -> formal out
    auto actual_out_tree = cw.getArgActualOutTree(*actual_arg);
    auto formal_out_tree = fw.getArgFormalOutTree(*formal_arg);
    _PDG->addTreeNodesToGraph(*actual_out_tree);
    connectOutTrees(formal_out_tree, actual_out_tree, EdgeType::PARAMETER_OUT);
  }

  // step3: connect return value actual in -> formal in, formal out -> actual out
  if (!fw.hasNullRetVal() && !cw.hasNullRetVal())
  {
    Tree *ret_formal_in_tree = fw.getRetFormalInTree();
    Tree *ret_formal_out_tree = fw.getRetFormalOutTree();
    Tree *ret_actual_in_tree = cw.getRetActualInTree();
    Tree *ret_actual_out_tree = cw.getRetActualOutTree();
    connectInTrees(ret_actual_in_tree, ret_formal_in_tree, EdgeType::PARAMETER_IN);
    connectInTrees(ret_actual_out_tree, ret_formal_out_tree, EdgeType::PARAMETER_OUT);
  }

  // step4: connect both control/data return edges of callee to the call site
  auto ret_insts = fw.getReturnInsts();
  auto call_inst = cw.getCallInst();
  Node *dst = _PDG->getNode(*call_inst);
  assert(dst != nullptr && "cannot add control edge to call node on nullptr!\n");
  // add control return edge

  for (auto ret_inst : ret_insts)
  {
    Node *src = _PDG->getNode(*ret_inst);
    if (src == nullptr)
      continue;
    src->addNeighbor(*dst, EdgeType::CONTROLDEP_CALLRET);
  }
  // add data return edge
  for (auto ret_inst : ret_insts)
  {
    Node* src = _PDG->getNode(*ret_inst);
    if (src == nullptr)
      continue;
    src->addNeighbor(*dst, EdgeType::DATA_RET);
  }
}

// ===== connect dependencies =====
void pdg::ProgramDependencyGraph::connectIntraprocDependencies(Function &F, FunctionAnalysisManager &FAM)
{
  // add control dependency edges
  // getAnalysis<ControlDependencyGraph>(F); // add control dependencies for nodes in F
  // FAM.getResult<ControlDependencyGraphAnalysis>(F);
  // connect formal tree with address variables
  FunctionWrapper* func_w = getFuncWrapper(F);
  if (!func_w)
    return;
  Node* entry_node = func_w->getEntryNode();
  for (auto arg : func_w->getArgList())
  {
    Tree* formal_in_tree = func_w->getArgFormalInTree(*arg);
    if (!formal_in_tree)
      return;

    Tree* formal_out_tree = func_w->getArgFormalOutTree(*arg);
    entry_node->addNeighbor(*formal_in_tree->getRootNode(), EdgeType::PARAMETER_IN);
    entry_node->addNeighbor(*formal_out_tree->getRootNode(), EdgeType::PARAMETER_OUT);
    connectFormalInTreeWithAddrVars(*formal_in_tree);
    connectFormalOutTreeWithAddrVars(*formal_out_tree);
  }

  if (!func_w->hasNullRetVal())
  {
    connectFormalInTreeWithAddrVars(*func_w->getRetFormalInTree());
    connectFormalOutTreeWithAddrVars(*func_w->getRetFormalOutTree());
  }
}

void pdg::ProgramDependencyGraph::connectInterprocDependencies(Function &F)
{
  auto func_w = getFuncWrapper(F);
  if (!func_w)
    return;
  // obtain call graph
  PDGCallGraph &call_g = PDGCallGraph::getInstance();
  auto call_insts = func_w->getCallInsts();
  auto invoke_insts = func_w->getInvokeInsts();
  std::vector<CallBase*> call_sites;
  call_sites.insert(call_sites.end(), call_insts.begin(), call_insts.end());
  call_sites.insert(call_sites.end(), invoke_insts.begin(), invoke_insts.end());

  // connect call instruction with the called targets
  for (auto call_site : call_sites)
  { 

    if (_PDG->hasCallWrappers(*call_site))
    {
      auto call_ws = getCallWrappers(*call_site);
      if (call_ws.empty())
        continue;
      auto call_site_node = _PDG->getNode(*call_site);
      if (!call_site_node)
        continue;
      // iterate through all call wrappers, one call site may have multiple call wrappers, we need to connect each of them
      for(auto call_w : call_ws)
      {
        for (auto arg : call_w->getArgList())
        {
          Tree *actual_in_tree = call_w->getArgActualInTree(*arg);
          if (!actual_in_tree)
          {
            if (auto callee = call_w->getCalledFunc())
            {
              llvm::FunctionType *funcType = callee->getFunctionType();
              if (!funcType->isVarArg()) 
              {
                errs() << "[WARNING]: empty actual tree for callsite " << *call_site << "\n";
              }
            }
            continue;
          }
          Tree *actual_out_tree = call_w->getArgActualOutTree(*arg);
          // connect call site node with actual parameter tree root node
          call_site_node->addNeighbor(*actual_in_tree->getRootNode(), EdgeType::PARAMETER_IN);
          call_site_node->addNeighbor(*actual_out_tree->getRootNode(), EdgeType::PARAMETER_OUT);
          connectActualInTreeWithAddrVars(*actual_in_tree, *call_site);
          connectActualOutTreeWithAddrVars(*actual_out_tree, *call_site);
        }

        // connect return trees
        // TODO: should change the name here. for return value, we should only connect
        // the tree node with vars after the call instruction
        if (!call_w->hasNullRetVal())
        {
          connectActualOutTreeWithAddrVars(*call_w->getRetActualInTree(), *call_site);
          connectActualOutTreeWithAddrVars(*call_w->getRetActualOutTree(), *call_site);
        }
        // connect call site with called function
        if (call_w->getCalledFunc() != nullptr)
        {
          auto called_func_w = getFuncWrapper(*call_w->getCalledFunc());
          connectCallerAndCallee(*call_w, *called_func_w);
        }
        else
        {
          // using call graph to query called functions by callsite
          auto ind_called_funcs = call_g.getIndirectCallCandidates(*call_w->getCallInst());
          for (auto ind_called_func : ind_called_funcs)
          {
            if (ind_called_func->isDeclaration() || ind_called_func->isVarArg())
              continue;
            auto called_func_w = getFuncWrapper(*ind_called_func);
            connectCallerAndCallee(*call_w, *called_func_w);
          }
        }
      }
    }
  }
}

// ====== connect class node with class methods ======
void pdg::ProgramDependencyGraph::connectClassNodeWithClassMethods(Function &F)
{
  // iterate through all function wrappers. If the function is a class method (non-empty _class_name field),
  // connect the corresponding class node with the function's entry node
  FunctionWrapper *fw = getFuncWrapper(F);
  if (!fw)
    return;
  std::string cls_name = fw->getClassName();
  if (cls_name.empty())
    return;
  auto class_node = _PDG->getClassNodeByName(cls_name);
  if (class_node == nullptr)
    return;
  assert(class_node != nullptr && "cannot connect empty class node with class methods");
  class_node->addNeighbor(*fw->getEntryNode(), EdgeType::CLS_MTH);
}

// ====== connect tree with variables ======
void pdg::ProgramDependencyGraph::connectFormalInTreeWithAddrVars(Tree &formal_in_tree)
{
  TreeNode *root_node = formal_in_tree.getRootNode();
  std::queue<TreeNode *> node_queue;
  node_queue.push(root_node);
  while (!node_queue.empty())
  {
    TreeNode *current_node = node_queue.front();
    node_queue.pop();
    TreeNode *parent_node = current_node->getParentNode();
    std::unordered_set<Value *> parent_node_addr_vars;
    if (parent_node != nullptr)
      parent_node_addr_vars = parent_node->getAddrVars();
    for (auto addr_var : current_node->getAddrVars())
    {
      if (!_PDG->hasNode(*addr_var))
        continue;
      auto addr_var_node = _PDG->getNode(*addr_var);
      current_node->addNeighbor(*addr_var_node, EdgeType::PARAMETER_IN);
      auto alias_nodes = addr_var_node->getOutNeighborsWithDepType(EdgeType::DATA_ALIAS);
      for (auto alias_node : alias_nodes)
      {
        Value *alias_node_val = alias_node->getValue();
        if (alias_node_val == nullptr)
          continue;
        if (parent_node_addr_vars.find(alias_node_val) != parent_node_addr_vars.end())
          continue;
        current_node->addNeighbor(*alias_node, EdgeType::PARAMETER_IN);
      }
    }

    for (auto child_node : current_node->getChildNodes())
    {
      node_queue.push(child_node);
    }
  }
}

void pdg::ProgramDependencyGraph::connectFormalOutTreeWithAddrVars(Tree &formal_out_tree)
{
  TreeNode *root_node = formal_out_tree.getRootNode();
  std::queue<TreeNode *> node_queue;
  node_queue.push(root_node);
  while (!node_queue.empty())
  {
    TreeNode *current_node = node_queue.front();
    node_queue.pop();
    for (auto addr_var : current_node->getAddrVars())
    {
      if (!_PDG->hasNode(*addr_var))
        continue;
      auto addr_var_node = _PDG->getNode(*addr_var);
      // TODO: add addr variables for formal out tree
      if (pdgutils::hasWriteAccess(*addr_var))
      {
        addr_var_node->addNeighbor(*current_node, EdgeType::PARAMETER_OUT);
        current_node->addAccessTag(AccessTag::DATA_WRITE);
      }
    }

    for (auto child_node : current_node->getChildNodes())
    {
      node_queue.push(child_node);
    }
  }
}

void pdg::ProgramDependencyGraph::connectActualInTreeWithAddrVars(Tree &actual_in_tree, CallBase &ci)
{
  TreeNode *root_node = actual_in_tree.getRootNode();
  std::set<Instruction *> insts_before_ci = pdgutils::getInstructionBeforeInst(ci);
  std::queue<TreeNode *> node_queue;
  node_queue.push(root_node);
  while (!node_queue.empty())
  {
    TreeNode *current_node = node_queue.front();
    node_queue.pop();
    for (auto addr_var : current_node->getAddrVars())
    {
      // only connect addr_var that are pred to the call site
      if (Instruction *i = dyn_cast<Instruction>(addr_var))
      {
        if (insts_before_ci.find(i) == insts_before_ci.end())
          continue;
      }
      if (!_PDG->hasNode(*addr_var))
        continue;
      auto addr_var_node = _PDG->getNode(*addr_var);
      addr_var_node->addNeighbor(*current_node, EdgeType::PARAMETER_IN);
    }

    for (auto child_node : current_node->getChildNodes())
    {
      node_queue.push(child_node);
    }
  }
}

void pdg::ProgramDependencyGraph::connectActualOutTreeWithAddrVars(Tree &actual_out_tree, CallBase &ci)
{
  TreeNode *root_node = actual_out_tree.getRootNode();
  // std::set<Instruction *> insts_after_ci = pdgutils::getInstructionAfterInst(ci);
  std::queue<TreeNode *> node_queue;
  node_queue.push(root_node);
  while (!node_queue.empty())
  {
    TreeNode *current_node = node_queue.front();
    node_queue.pop();
    for (auto addr_var : current_node->getAddrVars())
    {
      if (!_PDG->hasNode(*addr_var))
        continue;
      auto addr_var_node = _PDG->getNode(*addr_var);
      current_node->addNeighbor(*addr_var_node, EdgeType::PARAMETER_OUT);
    }

    for (auto child_node : current_node->getChildNodes())
    {
      node_queue.push(child_node);
    }
  }
}

void pdg::ProgramDependencyGraph::constructClassInheritanceGraph()
{
  // iterate through all the structure types in the module
  for (auto child_struct_type : _module->getIdentifiedStructTypes())
  {
    // the child class
    std::string class_name = child_struct_type->getName().str();
    // create new node if this class hasn't been added yet
    if (!_PDG->getClassNodeByName(class_name))
      _PDG->insertClassNode(class_name);
    
    Node* child_cls_node = _PDG->getClassNodeByName(class_name);
    // iterate through all the field definition
    for (auto ele_type : child_struct_type->elements())
    {
      // find potential parent class name
      if (auto st_type = dyn_cast<StructType>(ele_type))
      {
        std::string parent_class_name = st_type->getName().str();
        if (pdgutils::startWith(parent_class_name, "class."))
        {
          if (pdgutils::hasEnding(parent_class_name, ".base"))
            parent_class_name = pdgutils::stripClsTypeStrBasePostfix(parent_class_name);
          if (!_PDG->getClassNodeByName(parent_class_name))
            _PDG->insertClassNode(parent_class_name);
          Node* parent_cls_node = _PDG->getClassNodeByName(parent_class_name);
          child_cls_node->addNeighbor(*parent_cls_node, EdgeType::CLS_INH);
          parent_cls_node->addNeighbor(*child_cls_node, EdgeType::CLS_CHILD);
        }
      }
    }
  }
}

