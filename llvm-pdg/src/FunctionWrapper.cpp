#include "FunctionWrapper.hh"

using namespace llvm;

/**
 * base Inst type classfications
*/
void pdg::FunctionWrapper::addInst(Instruction &i)
{ 
  if (AllocaInst *ai = dyn_cast<AllocaInst>(&i))
    _alloca_insts.push_back(ai);
  if (StoreInst *si = dyn_cast<StoreInst>(&i))
    _store_insts.push_back(si);
  if (LoadInst *li = dyn_cast<LoadInst>(&i))
    _load_insts.push_back(li);
  if (DbgDeclareInst *dbi = dyn_cast<DbgDeclareInst>(&i))
    _dbg_declare_insts.push_back(dbi);
  if (CallInst *ci = dyn_cast<CallInst>(&i))
  {
    if (!ci->getCalledFunction())
      _call_insts.push_back(ci);
    else{
      std::string callee = ci->getCalledFunction()->getName().str();
      if (callee != "llvm.dbg.declare" && 
      callee != "llvm.dbg.value" && 
      callee != "llvm.dbg.label" &&
      callee != "llvm.lifetime.start" &&
      callee != "llvm.lifetime.end" &&
      callee != "llvm.trap" &&
      callee != "llvm.type.test" &&
      callee != "llvm.public.type.test" &&
      callee != "llvm.assume"
      )
        _call_insts.push_back(ci);
    }
  }
  if (InvokeInst *ii = dyn_cast<InvokeInst>(&i)) 
    _invoke_insts.push_back(ii);
  if (ReturnInst *reti = dyn_cast<ReturnInst>(&i))
    _return_insts.push_back(reti);
}

/**
 * 
 * @description: Iterate through all llvm.dbg.declare in the function, getting the DILocalVariable for each dbg.declare in turn and comparing it to arg.
 */
void pdg::FunctionWrapper::buildArgDbgMap()
{ 
  // check whether function's argument has sret attribute
  int sret_index = -1;
  for (auto arg : _arg_list)
  {
    if (arg->hasStructRetAttr())
    {
      sret_index = arg->getArgNo();
      break;
    }
  }
  std::map<int, llvm::DbgDeclareInst *> dbg_arg_no_map;
  for (auto dbg_declare_inst : _dbg_declare_insts)
  {
    DILocalVariable *di_local_var = dbg_declare_inst->getVariable();
    if (!di_local_var)
      continue;
    int di_arg_no = di_local_var->getArg();
    if (di_arg_no > 0)
      dbg_arg_no_map[di_arg_no] = dbg_declare_inst;
  }
  for (auto arg : _arg_list)
  {
    // argNo after sret_index should be decreased by 1
    int arg_no = arg->getArgNo();
    if (arg_no == sret_index)
      continue;
    if (sret_index >=0 && arg_no > sret_index)
      arg_no--;
    auto dbg_declare_inst = dbg_arg_no_map[arg_no + 1];
    if (dbg_declare_inst)
      _arg_dbg_declare_map[arg] = dbg_declare_inst;
  }
}

/**
 * @brief modified by lqs66
 * we use _arg_dbg_declare_map to get the DIType for the argument.
*/
DIType *pdg::FunctionWrapper::getArgDIType(Argument &arg)
{
  auto dbg_declare_inst = _arg_dbg_declare_map[&arg];
  if (dbg_declare_inst)
  {
    DILocalVariable *di_local_var = dbg_declare_inst->getVariable();
    if (di_local_var)
      return di_local_var->getType();
  }
  return nullptr;
}

void pdg::FunctionWrapper::buildFormalTreeForArgs()
{
  // traversal all args
  for (auto arg : _arg_list)
  {
    DILocalVariable* di_local_var = getArgDILocalVar(*arg);
    AllocaInst* arg_alloca_inst = getArgAllocaInst(*arg);
    // if (di_local_var == nullptr)
    // {
    //   errs() << "empty di local var: " << _func->getName().str() << " argId: " << arg->getArgNo() << "\n";
    //   continue;
    // }
    // if (arg_alloca_inst == nullptr)
    // {
    //   errs() << "empty alloca inst: " << _func->getName().str() << " argId: " << arg->getArgNo() << "\n";
    //   continue;
    // }
    // create a tree for current arg
    Tree *arg_formal_in_tree = new Tree(*arg);
    // create a root node for current tree
    TreeNode *formal_in_root_node;
    if (di_local_var == nullptr)
    {
      formal_in_root_node = new TreeNode(*_func, nullptr, 0, nullptr, arg_formal_in_tree, GraphNodeType::PARAM_FORMALIN);
      formal_in_root_node->setDILocalVariable(*di_local_var);
    }
    else
    {
      formal_in_root_node = new TreeNode(*_func, di_local_var->getType(), 0, nullptr, arg_formal_in_tree, GraphNodeType::PARAM_FORMALIN);
      formal_in_root_node->setDILocalVariable(*di_local_var);
    }
    if (arg_alloca_inst != nullptr)
    {
      auto addr_taken_vars = pdgutils::computeAddrTakenVarsFromAlloc(*arg_alloca_inst); // get all address taken instructions(load) that use this alloca instruction
      // traversal all load instructions that use this arg related alloca instruction, put it into _addr_vars
      for (auto addr_taken_var : addr_taken_vars)
      {
        formal_in_root_node->addAddrVar(*addr_taken_var);
      }
    }
    arg_formal_in_tree->setRootNode(*formal_in_root_node); //set tree root node

    arg_formal_in_tree->build();
    // insert this arg and its formal_in tree into map
    _arg_formal_in_tree_map.insert(std::make_pair(arg, arg_formal_in_tree));

    // build formal_out tree by copying fromal_in tree, this for create two edges, because dot only support one dircet edge between two nodes
    Tree* formal_out_tree = new Tree(*arg_formal_in_tree);
    formal_out_tree->setBaseVal(*arg);
    TreeNode* formal_out_root_node = formal_out_tree->getRootNode();
    // copy address variables
    for (auto addr_var : formal_in_root_node->getAddrVars())
    {
      formal_out_root_node->addAddrVar(*addr_var);
    }
    formal_out_tree->setTreeNodeType(GraphNodeType::PARAM_FORMALOUT);
    formal_out_tree->build();
    _arg_formal_out_tree_map.insert(std::make_pair(arg, formal_out_tree));
  }
}

void pdg::FunctionWrapper::buildFormalTreesForRetVal()
{
  Tree* ret_formal_in_tree = new Tree();
  DIType* func_ret_di_type = dbgutils::getFuncRetDIType(*_func);
  //create a root node for ret_val tree
  TreeNode* ret_formal_in_tree_root_node = new TreeNode(*_func, func_ret_di_type, 0, nullptr, ret_formal_in_tree, GraphNodeType::PARAM_FORMALIN);
  // traversal all return instructions in this function and put them into _addr_vars
  for (auto ret_inst : _return_insts)
  {
    auto ret_val = ret_inst->getReturnValue();
    ret_formal_in_tree_root_node->addAddrVar(*ret_val);
  }
  ret_formal_in_tree->setRootNode(*ret_formal_in_tree_root_node);
  ret_formal_in_tree->build();
  _ret_val_formal_in_tree = ret_formal_in_tree;

  Tree* ret_formal_out_tree = new Tree(*ret_formal_in_tree);
  TreeNode *ret_formal_out_tree_root_node = ret_formal_out_tree->getRootNode();
  // copy address variables
  for (auto addr_var : ret_formal_in_tree_root_node->getAddrVars())
  {
    ret_formal_out_tree_root_node->addAddrVar(*addr_var);
  }
  ret_formal_out_tree->setTreeNodeType(GraphNodeType::PARAM_FORMALOUT);
  ret_formal_out_tree->build();
  _ret_val_formal_out_tree = ret_formal_out_tree;
}

/**
 * @brief get this arg's DILocalVariable.
 * @return DILocalVariable or nullptr if no such DILocalVariable exists
 * modified by lqs66
 * we use _arg_dbg_declare_map to get the DILocalVariable for the argument.
*/
DILocalVariable *pdg::FunctionWrapper::getArgDILocalVar(Argument &arg)
{
  auto dbg_declare_inst = _arg_dbg_declare_map[&arg];
  if (dbg_declare_inst)
    return dbg_declare_inst->getVariable();
  return nullptr;
}

/**
 * @brief get the alloca instruction that allocates the memory for this arg
 * @return the alloca instruction that allocates the memory for this arg or nullptr if no such alloca instruction exists
 * modified by lqs66
 * we use _arg_dbg_declare_map to get the alloca instruction for the argument.
*/
AllocaInst *pdg::FunctionWrapper::getArgAllocaInst(Argument &arg)
{
  auto dbg_declare_inst = _arg_dbg_declare_map[&arg];
  if (dbg_declare_inst)
  {
    if (AllocaInst* ai = dyn_cast<AllocaInst>(dbg_declare_inst->getAddress()))
      return ai;
  }
  return nullptr;
}

/**
 * @brief get arg's formal_in tree.
*/
pdg::Tree *pdg::FunctionWrapper::getArgFormalInTree(Argument& arg)
{
  auto iter = _arg_formal_in_tree_map.find(&arg);
  if (iter == _arg_formal_in_tree_map.end())
    return nullptr;
  // assert(iter != _arg_formal_in_tree_map.end() && "cannot find formal tree for arg");
  return _arg_formal_in_tree_map[&arg];
}

/**
 * @brief get arg's formal_out tree.
*/
pdg::Tree *pdg::FunctionWrapper::getArgFormalOutTree(Argument& arg)
{
  auto iter = _arg_formal_out_tree_map.find(&arg);
  if (iter == _arg_formal_out_tree_map.end())
    return nullptr;
  return _arg_formal_out_tree_map[&arg];
}