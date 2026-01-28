#ifndef CFAMODEL_FACTORY_H
#define CFAMODEL_FACTORY_H

#include <vector>
#include <string>
#include <memory>

namespace llvm {
    class Module;
}

namespace uppllvm {
    class CFAModel;
}

/**
 * Factory class for creating CFAModel instances
 */
class CFAModelFactory {
public:
    /**
     * Creates CFAModel instances for target functions and entry point function
     * @param module LLVM Module containing the functions
     * @param targetFuncsName Vector of target function names
     * @param entryPointFuncName Entry point function name
     * @param call_depth Call depth (currently reserved for future use)
     * @param cfaModels Output vector to store created CFAModels
     * @param entryPointCFAModel Output pointer to store entry point CFAModel
     * @return true if successful, false otherwise
     */
    static bool createCFAModels(
        llvm::Module* module,
        const std::vector<std::string>& targetFuncsName,
        const std::string& entryPointFuncName,
        std::size_t call_depth,
        std::vector<std::unique_ptr<uppllvm::CFAModel>>& cfaModels,
        uppllvm::CFAModel*& entryPointCFAModel,
        bool merge = false);

#ifdef DEBUG
    static bool createCFAModelsForTest(
        llvm::Module* module,
        const std::vector<std::string>& targetFuncsName,
        const std::string& entryPointFuncName,
        std::vector<std::unique_ptr<uppllvm::CFAModel>>& cfaModels,
        uppllvm::CFAModel*& entryPointCFAModel);
#endif

private:
    CFAModelFactory() = delete; // Prevent instantiation
};

#endif // CFAMODEL_FACTORY_H
