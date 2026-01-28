#ifndef INSTRUMENTER_HH
#define INSTRUMENTER_HH

/**
 * @file instrumenter.hh
 * @brief This file contains the declaration of the instrumenter.
 */

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/WithColor.h>
#include "llvm/IRPrinter/IRPrintingPasses.h"
#include <llvm/Transforms/Utils.h>
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include <llvm/IR/Verifier.h>
#include <llvm/CodeGen/StableHashing.h> 
#include "llvm/IR/TypedPointerType.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "macro.hh"
#include "func_worklist.hpp"
#include <yaml-cpp/yaml.h> // For YAML parse
#include <boost/algorithm/string.hpp>

#include <stdio.h>

extern std::string _curr_property;
extern std::string _flightControl;
extern bool _nota;

namespace instrumenter{

    void runTimer(llvm::Module &M);
    void runInputGetter(llvm::Module &M, std::vector<llvm::Function*>& _entrypoints);

    llvm::GlobalVariable* createStringConstant(llvm::Module &M, std::string str, std::string name);
    llvm::GlobalVariable* createGlobalCStrting(llvm::Module &M, std::string str, int size);
    llvm::GlobalVariable* createGlobalPointer(llvm::Module &M, std::string str);
    llvm::GlobalVariable* createStartTimestamp(llvm::Module &M);
    llvm::GlobalVariable* createCurrTimestamp(llvm::Module &M);
    llvm::GlobalVariable* createCounter(llvm::Module &M);
    llvm::Function* createSnprintf(llvm::Module &M);
    llvm::Function* createFwriteFunc(llvm::Module &M);
    llvm::Function* createFopenFunc(llvm::Module &M);
    llvm::Function* createSetvbufFunc(llvm::Module &M);
    llvm::Function* createFflushFunc(llvm::Module &M);
    llvm::Function* createFcloseFunc(llvm::Module &M);
    llvm::Function* createTimeFunc(llvm::Module &M);
    llvm::Function* createGetpidFunc(llvm::Module &M);
    uint32_t getBBNum(std::string labelstr);
    uint32_t getSubBBNum(std::string labelstr);
    int getUID(llvm::Value* val);

    class Inputgetter;

    /// @brief For ArduCopter instrumenter.
    class ArduCopter {
    public:
        ArduCopter(){}
        
        enum class ParamType {
            I8,
            I16,
            I32
        };
        
        /// @brief Get the inputgetter.
        /// @return Inputgetter pointer.
        Inputgetter* getInputgetter() const {
            return _inputgetter;
        }

        /// @brief Set the inputgetter.
        /// @param inputgetter Inputgetter pointer.
        void setInputgetter(Inputgetter* inputgetter) { 
            _inputgetter = inputgetter; 
        }

        /// @brief Create a global parameter.
        /// @param M LLVM IR module.
        /// @param ptype The type of the parameter.
        /// @param paramName The name of the parameter.
        void createGlobalParam(llvm::Module &M, ParamType ptype, std::string paramName, uint32_t param_id, int64_t initValue=0);

        /// @brief Create dump_vars() and dump_heap_vars() function.
        ///        We call dump_heap_vars in dump_vars.
        /// @param M LLVM IR module.
        /// @param paramName The name of the parameter. We use param to control dump behavior.
        llvm::Function* createDumpVarsFunc(llvm::Module &M, std::string paramName, llvm::Function* entrypoint);

        void insertCounter(llvm::Function* entrypoint, std::string rate);

        llvm::Function* createTimeLoggerCaller(llvm::Module &M, std::string cycleParam="time_c_cycle");
    private:
        Inputgetter* _inputgetter;
    };

    /// @brief For PX4 instrumenter.
    class PX4{
    public:
        PX4(){}

        /// @brief Get the inputgetter.
        /// @return Inputgetter pointer.
        Inputgetter* getInputgetter() const {
            return _inputgetter;
        }

        /// @brief Set the inputgetter.
        /// @param inputgetter Inputgetter pointer.
        void setInputgetter(Inputgetter* inputgetter) { 
            _inputgetter = inputgetter; 
        }

        /// @brief Create dump_vars() and dump_heap_vars() function.
        ///        We call dump_heap_vars in dump_vars.
        /// @param paramName The name of the parameter. We use param to control dump behavior.
        /// @param paramHandlerName The name of the parameter handler.
        llvm::Function* createDumpVarsFunc(llvm::Module &M, std::string paramName, std::string paramHandlerName, llvm::Function* entrypoint);

        /// @brief Insert a periodic counter/check at the beginning of the given entrypoint function.
        /// @param entrypoint The function where the counter/check is inserted (usually program entry).
        /// @param cycle Name of the global parameter that specifies the cycle length (e.g. "ret_c_cycle").
        void insertCounter(llvm::Function* entrypoint, std::string cycle);

        /// @brief Create a caller function that calls the time logger function.
        /// @param M LLVM IR module.
        /// @param cycleParam Name of the global parameter that specifies the cycle length.
        llvm::Function* createTimeLoggerCaller(llvm::Module &M, std::string cycleParam="_time_c_cycle");
    private:
        Inputgetter* _inputgetter;
    };

    class BasicBlockTimer : public llvm::PassInfoMixin<BasicBlockTimer> {
    public:
        
        // constructor
        BasicBlockTimer () {}
        // BasicBlockTimer(std::string bbNums_bin_path) {
        //     if (bbNums_bin_path != ""){
        //         FILE* fp = fopen(bbNums_bin_path.c_str(), "rb");
        //         if (!fp) {
        //             ERROR("Can't open file: " << bbNums_bin_path);
        //             exit(1);
        //         }
                
        //         while(!feof(fp)){
        //             uint32_t bbNum;
        //             fread(&bbNum, sizeof(uint32_t), 1, fp);
        //             _bbNumNeedToTimer.insert(bbNum);
        //         }
        //         fclose(fp);
        //     }
        // }

        llvm::PreservedAnalyses run(llvm::Module& M, llvm::ModuleAnalysisManager&) {

            for (auto& F : M) {
                if (F.isDeclaration() || F.getName().contains("dummyAllocSTy."))
                    continue;
                // llvm::outs() << "Function: " << F.getName() << "\n";
                for (auto& BB : F){
                    if (BB.getName().startswith("bbNum")) {
                        _bbNumNeedToTimer.insert(getBBNum(BB.getName().str()));
                        _BBNeedToTimer.insert(&BB);
                    }
                    // uint32_t bbNum = getBBNum(BB.getName().str());
                    // // llvm::outs() << " BasicBlock: " << BB.getName() << ", bbNum: " << bbNum << "\n";
                    // if (_bbNumNeedToTimer.count(bbNum) > 0){
                    //     _BBNeedToTimer.insert(&BB);
                    // }
                }
            }
            // llvm::outs() << "Basic blocks that need to be timed: \n";
            createThreadLocalGlobalVariables(M);
            if (_flightControl == "ArduCopter" || _flightControl == "arducopter"){
                ArduCopter arducopter;
                arducopter.createGlobalParam(M, ArduCopter::ParamType::I32, "time_c_cycle", 258, 10000);
                arducopter.createTimeLoggerCaller(M, "time_c_cycle");
            }else if (_flightControl == "PX4" || _flightControl == "px4") {
                PX4 px4;
                px4.createTimeLoggerCaller(M, "_time_c_cycle");
            }else {
                ERROR("Unknown flight control: " << _flightControl);
                exit(1);
            }

            // createTimeLoggerCaller(M);

            for (auto& BB : _BBNeedToTimer){
                // splitCallSitesInBlock(*BB);
                insertTimeLoggerCallerIntoBasicBlock(*BB);
            }

            // for (auto& BB : _splitBBNeedToTimer){
            //     insertTimeLoggerCallerIntoBasicBlock(*BB);
            // }

            return llvm::PreservedAnalyses::all();
        }
        static bool isRequired() { return true; }

        void createThreadLocalGlobalVariables(llvm::Module &M);

        // llvm::Function* createTimeLoggerCaller(llvm::Module &M);

        // void splitCallSitesInBlock(llvm::BasicBlock& BB);

        void insertTimeLoggerCallerIntoBasicBlock(llvm::BasicBlock& BB);

        // void insertTimeLoggerIntoFunction(llvm::Function& F);

    private:
        std::set<uint32_t> _bbNumNeedToTimer; // Set of basic block numbers that need to be timed.
        std::set<llvm::BasicBlock*> _BBNeedToTimer; // Set of basic blocks that need to be timed.
        // std::set<llvm::BasicBlock*> _splitBBNeedToTimer;
    };

    /**
     * @brief 
     * Used to write memory values and the return values of non-inlinable functions to a file as input for the model.
     * 1. Instrument at the beginning of the entry function to obtain the values of global variables stored on the memory. Additionally, values of objects created with new and malloc need to be obtained.
     * 2. Retrieve the non-inlinable function IDs, indirect call IDs, and their associated basic block IDs recorded in the *_noInlineCalls.bin file during the model generation phase. Parse them into call instructions in IR. Insert instrumentation to obtain return values immediately after these call instructions.
     * 
     */
    class Inputgetter : public llvm::PassInfoMixin<Inputgetter> {
    public:
        /// @brief Constructor.
        /// @param _entrypoints Entry functions need to be instrumented.
        /// @param ids_bin_path The path of the *_noInlineCalls.bin file. 
        ///                     We use it to get the non-inlinable function IDs or indirect call IDs. 
        ///                     We insert instrumentation to obtain return values immediately after these call instructions.   
        Inputgetter(std::vector<llvm::Function*>& entrypoints) : _entrypoints(entrypoints){
            // if (ids_bin_path != ""){
            //     FILE* fp = fopen(ids_bin_path.c_str(), "rb");
            //     if (!fp) {
            //         ERROR("Can't open file: " << ids_bin_path);
            //         exit(1);
            //     }
                
            //     while(!feof(fp)){
            //         uint32_t bbNum;
            //         uint32_t size;
            //         fread(&bbNum, sizeof(uint32_t), 1, fp);
            //         fread(&size, sizeof(uint32_t), 1, fp);
            //         std::set<uint32_t> no_inlined_ids;
            //         for (int i = 0; i < size; i++){
            //             uint32_t id;
            //             fread(&id, sizeof(uint32_t), 1, fp);
            //             no_inlined_ids.insert(id);
            //         }
            //         _bbNumAndNoInlinedIDsMap[bbNum] = no_inlined_ids;
            //     }
            //     fclose(fp);
            //     fillTargets(*_entrypoints[0]->getParent());
            // }
        }

        /// @brief Run the instrumenter. According to the property name, we excute different instrumenter.
        llvm::PreservedAnalyses run(llvm::Module& M, llvm::ModuleAnalysisManager&) {
            createCounter(M);
            if (_flightControl == "ArduCopter" || _flightControl == "arducopter"){
                ArduCopter arducopter;
                // IC_GUARD used to control whether to dump the values of global variables.
                arducopter.createGlobalParam(M, ArduCopter::ParamType::I8, "ic_guard", 258, 0);
                // RET_C_RATE used to control the rate of collecting return values.
                // arducopter.createGlobalParam(M, ArduCopter::ParamType::I32, "ret_c_cycle", 259, 1000);  // COMMENTED: Function return value collection disabled
                arducopter.setInputgetter(this);
                arducopter.createDumpVarsFunc(M, "ic_guard", _entrypoints[0]);
                // arducopter.insertCounter(_entrypoints[0], "ret_c_cycle");  // COMMENTED: Function return value collection disabled
            } else if (_flightControl == "PX4" || _flightControl == "px4") {
                PX4 px4;
                px4.setInputgetter(this);
                px4.createDumpVarsFunc(M, "_ic_guard", "_ic_guard_h", _entrypoints[0]);
                // px4.insertCounter(_entrypoints[0], "_ret_c_cycle");  // COMMENTED: Function return value collection disabled
            }else {
                ERROR("Unknown flight control: " << _flightControl);
                exit(1);
            }

            for (auto& F : _entrypoints) {
                insertDumpVarsFuncCall(*F);
            }
            // warpDynamicMemoryAllocation(M);
            // insertCallRetValCollector(_entrypoints[0]);  // COMMENTED: Function return value collection disabled
            warpDynamicMemoryAllocation(M);
            if (!_nota)
                supplementDMAType(M);
            return llvm::PreservedAnalyses::all();
        }
        
        /// @brief Instrument the dump_vars() function call into target function F.
        void insertDumpVarsFuncCall(llvm::Function& F);
        /// @brief Instrument the global_init_help() function call into the entrypoint function.
        void insertGlobalInitCode(llvm::Function* entrypoint);
        /// @brief Create global_init_help() function and some global variables/constants.
        llvm::Function* createGlobalInitHelper(llvm::Module &M);
        /// @brief Fill _bbNumAndNoInlinedIDsMap and _bbAndNoInlinedCallsMap.
        void fillTargets(llvm::Module &M);
        /// @brief Insert instrumentation to obtain return values immediately after these call instructions.
        // void insertCallRetValCollector(llvm::Function* entrypoint);  // COMMENTED: Function return value collection disabled
        /// @brief Create start_timestamp_init() function.
        llvm::Function* createStartTimestampInit(llvm::Module &M);
        /// @brief Create retValCollector() function.
        // llvm::Function* createRetValCollector(llvm::Module &M);  // COMMENTED: Function return value collection disabled
        /// @brief Create dump_heap_vars() function.
        llvm::Function* createDumpHeapVarsFunc(llvm::Module &M);
        llvm::Function* createDumpEntryArgsFunc(llvm::Module &M, std::vector<llvm::Type*> entryArgTypes);
        /// @brief Replace all DMA related functions with our own functions.
        void warpDynamicMemoryAllocation(llvm::Module &M);
        /// @brief Supplement the DMA type.
        void supplementDMAType(llvm::Module &M);

    private:
        llvm::StringRef getStructTypeNamePrefix(llvm::StringRef Name);
        size_t hashType(llvm::Type* Ty);
        std::vector<llvm::Function*>& _entrypoints;
        std::map<uint32_t, std::set<uint32_t>> _bbNumAndNoInlinedIDsMap;                    // Used to store bbNum and noInlinedIDs pairs.
        std::map<llvm::BasicBlock*, std::vector<llvm::CallBase*>> _bbAndNoInlinedCallsMap;  // Convert bbNum to BasicBlock and noInlinedIDs to CallBase.
    };
}

#endif