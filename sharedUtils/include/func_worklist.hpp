#ifndef FUNC_WORKLIST_HH
#define FUNC_WORKLIST_HH
#include <string>
#include <vector>
#include <queue>
#include <regex>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include "llvm/IR/Constants.h"
#include "macro.hh"

namespace utils{
    extern std::vector<std::string> dmaFuncs; // dynamic memory allocation functions

    std::vector<llvm::Function*> convertFunctionNameToFunction(llvm::Module& module, std::vector<std::string> functionNames);

}
# endif