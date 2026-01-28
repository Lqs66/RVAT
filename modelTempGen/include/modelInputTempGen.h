#ifndef GLOBAL_VARS_FLATTEN_H
#define GLOBAL_VARS_FLATTEN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/CodeGen/StableHashing.h> 
#include "llvm/IR/TypedPointerType.h"

#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <queue>
#include <map>
#include <unordered_set>
#include <filesystem>
#include <fstream>

#include "macro.hh"

#include "modelInputs.pb.h"

using namespace llvm;

/**
 * This file used to gen model input temp file.
 */

std::string llvmBaseTypeToString(llvm::Type* type);
modelInputs::TypeSpec llvmBaseTypeToTypeSpec(llvm::Type* type);

class ModelInputTempGen : public PassInfoMixin<ModelInputTempGen> {
public:
    ModelInputTempGen( std::string json_output_path, std::string fc_type) :
        _tmpOutputPath(json_output_path),
        _fcType(fc_type)
        {};
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
private:
    /// @brief Map the global variables to memory addresses.
    void mapGVToMemAddr(llvm::Module &M);
    /// @brief Get the stable hash value of the type.
    size_t hashType(llvm::Type* Ty);
    /// @brief Delete digital suffix of the struct type name. e.g. "struct.A.1" -> "struct.A"
    StringRef getStructTypeNamePrefix(StringRef Name);
    /// @brief Fill the global variable info.
    void fillGlobalVar(Module &M, modelInputs::ModelInputs& modelInputs);
    /// @brief Fill the heap variable info.
    void fillHeapVar(Module &M, modelInputs::ModelInputs& modelInputs);
    /// @brief According to llvm IR aggregate type, fill the member info.
    template <typename T>
    void fillAggrMember(Module &M, T* var, llvm::Type* aggrType, uint32_t &startOffset);
    void dumpBinary(Module &M);
    /// @brief Find IR type by hash value and store it in _heapVarInfos.
    /// Notice: 
    /// This function should be called after parseHeapRTInfo.
    void findHeapVarInfoType(Module &M);
    void mapHeapTypeHashToType(llvm::Module &M);

    std::vector<std::pair<GlobalVariable*, uint64_t>> _gvAddrMap;
    std::map<uint64_t, llvm::Type*> _heapVarTypeMap; /// Map the heap variable hash value to IR type.
    std::string _tmpOutputPath;
    std::string _fcType;
    std::vector<llvm::CallBase*> _inCallNeedToCollectRet;
    std::set<llvm::Function*> _funcsNeedToCollectRet;
};

/// @brief Interface function to run the MISGen pass
/// @param M The module to run the pass on
/// @param tmp_output_path The path to the output tmp file
void runMITGen(Module &M, std::string tmp_output_path = "./", std::string fc_type = "arducopter");

#endif // GLOBAL_VARS_FLATTEN_H
