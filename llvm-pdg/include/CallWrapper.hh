#ifndef CALLWRAPPER_H_
#define CALLWRAPPER_H_
#include "LLVMEssentials.hh"
#include "Tree.hh"
#include "PDGUtils.hh"
#include "FunctionWrapper.hh"

namespace pdg
{
  class CallWrapper
  {
    private:
      llvm::CallBase* _call_inst;
      llvm::Function* _called_func;
      std::vector<llvm::Value *> _arg_list;
      std::map<llvm::Value *, Tree *> _arg_actual_in_tree_map;
      std::map<llvm::Value *, Tree *> _arg_actual_out_tree_map;
      Tree * _ret_val_actual_in_tree;
      Tree * _ret_val_actual_out_tree;
      bool _has_param_trees = false;

    public:

      // handle vcall by using call graph
      CallWrapper(llvm::CallBase& ci)
      {
        _call_inst = &ci;
        for (auto arg_iter = ci.arg_begin(); arg_iter != ci.arg_end(); arg_iter++)
        {
          _arg_list.push_back(*arg_iter);
        }
      }
      void buildActualTreeForArgs(FunctionWrapper &callee_fw);
      void buildActualTreesForRetVal(FunctionWrapper &callee_fw);
      llvm::CallBase *getCallInst() { return _call_inst; }
      llvm::Function *getCalledFunc() { return _called_func; }
      void setCalledFunc(llvm::Function &called_func) { _called_func = &called_func; }
      std::vector<llvm::Value *> &getArgList() { return _arg_list; }
      Tree *getArgActualInTree(llvm::Value &actual_arg);
      Tree *getArgActualOutTree(llvm::Value &actual_arg);
      Tree *getRetActualInTree() { return _ret_val_actual_in_tree; }
      Tree *getRetActualOutTree() { return _ret_val_actual_out_tree; }
      bool hasNullRetVal() { return (_ret_val_actual_in_tree == nullptr); }
      bool hasParamTrees() { return _has_param_trees; }
      void setHasParamTrees() { _has_param_trees = true; }
  };
}

#endif
