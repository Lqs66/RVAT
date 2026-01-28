#ifndef PDGENUMS_H_
#define PDGENUMS_H_

namespace pdg
{
  enum class EdgeType
  {
    IND_CALL,
    CONTROLDEP_CALLINV, // CONTROLDEP_CALLINV edge connects a call site with the function entry point of callee. It indicates the control flow transition from the caller to callee.
    CONTROLDEP_CALLRET, // Connects the function entry node with all the instructions in the function body.
    CONTROLDEP_ENTRY,
    CONTROLDEP_BR,    // Connects the control dependence block's terminator to the instructions in the control dependent block.
    CONTROLDEP_IND_BR,
    CONTROLDEP_INVOKE_NORMAL,
    CONTROLDEP_INVOKE_EXCEPT,
    DATA_DEF_USE,
    DATA_RAW,
    DATA_READ,
    DATA_ALIAS,
    DATA_RET,
    PARAMETER_IN,
    PARAMETER_OUT,
    PARAMETER_FIELD,
    GLOBAL_DEP,
    VAL_DEP,
    CLS_MTH,
    CLS_INH,
    CLS_CHILD, // the reverse link of CLS_INH
    ANNO_VAR,
    ANNO_GLOBAL,
    ANNO_OTHER,
    TYPE_OTHEREDGE
  };

  enum class GraphNodeType
  {
    INST_FUNCALL,
    INST_RET,
    INST_BR,
    INST_OTHER,
    FUNC_ENTRY,
    PARAM_FORMALIN,
    PARAM_FORMALOUT,
    PARAM_ACTUALIN,
    PARAM_ACTUALOUT,
    VAR_STATICALLOCGLOBALSCOPE,         // C: global variable          LLVM: global with common linkage
    VAR_STATICALLOCMODULESCOPE,         // C: static global variable   LLVM: global with internal linkage
    VAR_STATICALLOCFUNCTIONSCOPE,       // C: static function variable LLVM: global with internal linkage, variable name is prefixed by function name
    VAR_OTHER,
    FUNC,
    CLASS,
    ANNO_VAR,
    ANNO_GLOBAL,
    ANNO_OTHER
  };

  enum class AccessTag
  {
    DATA_READ,
    DATA_WRITE
  };
}

#endif